#include "SphereGeometry.hpp"
#include <NXOpen/Body.hxx>
#include <NXOpen/Edge.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/Curve.hxx>
#include <NXOpen/CurveFeatureRule.hxx>
#include <NXOpen/Direction.hxx>
#include <NXOpen/DirectionCollection.hxx>
#include <NXOpen/Expression.hxx>
#include <NXOpen/Features_ExtrudeBuilder.hxx>
#include <NXOpen/Features_FeatureCollection.hxx>
#include <NXOpen/Features_SketchFeature.hxx>
#include <NXOpen/Features_SheetMetal_SheetmetalManager.hxx>
#include <NXOpen/Features_SheetMetal_ConvertToSheetmetalBuilder.hxx>
#include <NXOpen/Features_SheetMetal_FlatSolidBuilder.hxx>
#include <NXOpen/GeometricUtilities_BooleanOperation.hxx>
#include <NXOpen/GeometricUtilities_Extend.hxx>
#include <NXOpen/GeometricUtilities_Limits.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/Plane.hxx>
#include <NXOpen/PlaneCollection.hxx>
#include <NXOpen/Point.hxx>
#include <NXOpen/PointCollection.hxx>
#include <NXOpen/ScRuleFactory.hxx>
#include <NXOpen/Section.hxx>
#include <NXOpen/SectionCollection.hxx>
#include <NXOpen/SelectFace.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/Sketch.hxx>
#include <NXOpen/SketchCollection.hxx>
#include <NXOpen/SketchInPlaceBuilder.hxx>
#include <NXOpen/Update.hxx>
#include <uf.h>
#include <uf_curve.h>
#include <uf_csys.h>
#include <uf_eval.h>
#include <uf_modl.h>
#include <uf_obj.h>
#include <uf_part.h>
#include <algorithm>
#include <iomanip>
#include <locale>
#include <set>
#include <sstream>
#include <stdexcept>

namespace sphere_unfold {
using namespace NXOpen;
void Check(int code){if(code){char msg[1024]={};UF_get_fail_message(code,msg);throw std::runtime_error(std::string(msg)+" ("+std::to_string(code)+")");}}
Vec Unit(Vec v){double len=Length(v);if(len<1e-12)throw std::runtime_error("无效方向向量。");return v*(1/len);}
namespace {
Vec V(const double* p){return {p[0],p[1],p[2]};}
std::string Number(double v){std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(16)<<v;return out.str();}
struct Eval {
    UF_EVAL_p_t value=nullptr;
    explicit Eval(tag_t edge){Check(UF_EVAL_initialize(edge,&value));}
    ~Eval(){if(value)UF_EVAL_free(value);}
    Vec At(double t){double p[3];Check(UF_EVAL_evaluate(value,0,t,p,nullptr));return V(p);}
};
struct Surface {int type=0,sign=0;Vec point,axis;double radius=0;};
Surface Data(tag_t face){Surface f;double p[3],d[3],box[6],r2;Check(UF_MODL_ask_face_data(face,&f.type,p,d,box,&f.radius,&r2,&f.sign));f.point=V(p);f.axis=V(d);return f;}
std::vector<Vec> EdgePoints(Face* face){
    std::vector<Vec> out;
    for(auto* e:face->GetEdges()){Eval eval(e->Tag());double limits[2];Check(UF_EVAL_ask_limits(eval.value,limits));for(int i=0;i<=24;++i)out.push_back(eval.At(limits[0]+(limits[1]-limits[0])*i/24));}
    return out;
}
double Theta(const Source& s,Vec point){Vec q=point-s.center;double t=std::atan2(Dot(q,s.y),Dot(q,s.x));if(t<-1e-8)t+=2*pi;return t;}
bool OnSide(const Source& s,Vec q){
    Vec r=q-s.center;r=r-s.z*Dot(r,s.z);if(Length(r)<s.radius*1e-7)return true;
    double t=Theta(s,q);return std::min({std::abs(t),std::abs(t-2*pi),std::abs(t-s.sweep)})<1e-6;
}
// Only rotational patches with two meridians and level end boundaries are
// accepted. Reject holes, arbitrary trims, and partial source geometry rather
// than silently constructing the untrimmed underlying sphere/cylinder.
void ValidateBoundary(const Source& s,Face* face,bool spherical){
    double tol=std::max(1e-5*s.unitsPerMm,s.radius*1e-7);
    for(auto* e:face->GetEdges()){
        Eval eval(e->Tag());double limits[2];Check(UF_EVAL_ask_limits(eval.value,limits));
        bool equator=true,end=true,side=true;
        for(int i=0;i<=32;++i){Vec p=eval.At(limits[0]+(limits[1]-limits[0])*i/32),q=p-s.center;double h=Dot(q,s.z);
            equator=equator&&std::abs(h)<tol;
            end=end&&std::abs(h-(spherical?s.radius*std::sin(s.latitude):-s.height))<tol;
            side=side&&OnSide(s,p);
            if(h<(spherical?-tol:-s.height-tol)||h>(spherical?s.radius*std::sin(s.latitude)+tol:tol))throw std::runtime_error("所选面的轴向范围不一致。");
            Vec radial=q-s.z*h;
            if(Length(radial)>tol&&s.sweep<2*pi-1e-6&&Theta(s,p)>s.sweep+1e-6&&Theta(s,p)<2*pi-1e-6)throw std::runtime_error("圆柱面与球面的周向范围不一致。");
        }
        if(!equator&&!end&&!side)throw std::runtime_error("当前支持完整的回转面片：圆形端口与经线边界；请先处理孔槽或不规则裁剪。");
    }
}
tag_t Line(Vec p,Vec q){UF_CURVE_line_t l={{p.x,p.y,p.z},{q.x,q.y,q.z}};tag_t t=0;Check(UF_CURVE_create_line(&l,&t));return t;}
tag_t Arc(Vec a,Vec b,Vec c){double p[]={a.x,a.y,a.z},q[]={b.x,b.y,b.z},r[]={c.x,c.y,c.z};tag_t t=0;Check(UF_CURVE_create_arc_thru_3pts(1,p,q,r,&t));return t;}
Sketch* ProfileSketch(const std::vector<tag_t>& curves,Vec normal){
    auto* part=Session::GetSession()->Parts()->Work();
    // Every profile starts with a straight edge in the section plane.
    Eval first(curves.front());double limits[2];Check(UF_EVAL_ask_limits(first.value,limits));
    Vec start=first.At(limits[0]),axis=Unit(first.At(limits[1])-start);normal=Unit(normal);
    Point3d origin{start.x,start.y,start.z};
    auto* plane=part->Planes()->CreatePlane(origin,{normal.x,normal.y,normal.z},SmartObject::UpdateOptionWithinModeling);
    auto* xAxis=part->Directions()->CreateDirection(origin,{axis.x,axis.y,axis.z},SmartObject::UpdateOptionWithinModeling);
    auto* point=part->Points()->CreatePoint(origin);
    plane->SetVisibility(SmartObject::VisibilityOptionInvisible);xAxis->SetVisibility(SmartObject::VisibilityOptionInvisible);point->SetVisibility(SmartObject::VisibilityOptionInvisible);
    auto* builder=part->Sketches()->CreateSketchInPlaceBuilder2(nullptr);Sketch* sketch=nullptr;
    try{
        builder->SetPlaneOption(Sketch::PlaneOptionExistingPlane);builder->SetPlaneReference(plane);builder->SetAxisReference(xAxis);
        builder->SetOriginOption(OriginMethodSpecifyPoint);builder->SetSketchOrigin(point);
        auto* object=builder->Commit();sketch=dynamic_cast<Sketch*>(object);
        if(auto* feature=dynamic_cast<Features::SketchFeature*>(object))sketch=feature->Sketch();
        builder->Destroy();builder=nullptr;
        if(!sketch||!sketch->Feature())throw std::runtime_error("创建分瓣拉伸截面草图失败。");
        sketch->Activate(Sketch::ViewReorientFalse);
        for(auto tag:curves)sketch->AddGeometry(dynamic_cast<Curve*>(NXObjectManager::Get(tag)),Sketch::InferConstraintsOptionInferNoConstraints);
        sketch->Update();sketch->Deactivate(Sketch::ViewReorientFalse,Sketch::UpdateLevelModel);
        sketch->Feature()->SetName(NXString("分瓣拉伸截面",NXString::UTF8));sketch->Blank();return sketch;
    }catch(...){
        if(builder)builder->Destroy();
        try{if(sketch&&sketch->IsActive())sketch->Deactivate(Sketch::ViewReorientFalse,Sketch::UpdateLevelModel);}catch(...){}
        throw;
    }
}
tag_t Extrude(const std::vector<tag_t>& curves,Vec direction,double length,double units){
    auto* part=Session::GetSession()->Parts()->Work();auto* sketch=ProfileSketch(curves,direction);
    auto* builder=part->Features()->CreateExtrudeBuilder(nullptr);
    try{
        auto* section=part->Sections()->CreateSection(1e-5*units,1e-4*units,.5);std::vector<Curve*> objects;
        for(auto t:curves)objects.push_back(dynamic_cast<Curve*>(NXObjectManager::Get(t)));
        auto* rule=part->ScRuleFactory()->CreateRuleCurveFeature({sketch->Feature()});
        section->AddToSection({rule},objects.front(),nullptr,nullptr,{0,0,0},Section::ModeCreate);
        builder->SetSection(section);builder->SetDirection(part->Directions()->CreateDirection({0,0,0},{direction.x,direction.y,direction.z},SmartObject::UpdateOptionWithinModeling));
        builder->SetDistanceTolerance(1e-4*units);builder->BooleanOperation()->SetType(GeometricUtilities::BooleanOperation::BooleanTypeCreate);
        builder->Limits()->StartExtend()->Value()->SetFormula("0");builder->Limits()->EndExtend()->Value()->SetFormula(Number(length).c_str());
        builder->SetParentFeatureInternal(sketch->Feature());
        auto* result=builder->CommitFeature();builder->Destroy();builder=nullptr;auto bodies=result->GetBodies();
        if(bodies.size()!=1||!bodies[0]->IsSolidBody())throw std::runtime_error("分瓣拉伸未生成单个实体。");
        if(!sketch->Feature()->IsInternal())result->MakeSketchInternal();
        if(!sketch->Feature()->IsInternal())throw std::runtime_error("截面草图未能放入拉伸特征内部。");
        for(auto t:curves)Check(UF_OBJ_set_blank_status(t,UF_OBJ_BLANKED));return bodies[0]->Tag();
    }catch(...){if(builder)builder->Destroy();throw;}
}
tag_t Prism(const std::vector<Vec>& points,Vec direction,double length,double units){
    std::vector<tag_t> curves;for(size_t i=0;i<points.size();++i)curves.push_back(Line(points[i],points[(i+1)%points.size()]));return Extrude(curves,direction,length,units);
}
void Unite(tag_t target,tag_t tool){tag_t f=0;Check(UF_MODL_unite_bodies_with_retained_options(target,tool,false,false,&f));if(!f)throw std::runtime_error("分瓣根部连接失败。");}
tag_t Intersect(tag_t target,tag_t tool){
    int count=0;tag_t* bodies=nullptr;Check(UF_MODL_intersect_bodies(target,tool,&count,&bodies));
    if(count!=1){UF_free(bodies);throw std::runtime_error("开缝切割未生成单个瓣；请减少缝宽或瓣数。");}tag_t result=bodies[0];UF_free(bodies);return result;
}
tag_t Petal(const Plan& p,int index){
    const auto& s=p.source;double a=p.step*(index+.5);Vec radial=s.x*std::cos(a)+s.y*std::sin(a),tangent=s.x*(-std::sin(a))+s.y*std::cos(a);
    auto at=[&](double x,double y,double z){return s.center+radial*x+tangent*y+s.z*z;};
    // Bound the petal by the root band's inner-face pitch. A slit measured
    // only on the outer radius can be narrower than the bend setback: after
    // flattening those wide cylindrical petal roots overlap each other.
    double half=p.rootGap>p.gap+1e-8*s.unitsPerMm?p.outer*std::tan(p.step/2)-p.rootGap/(2*std::cos(p.step/2)):p.outer*std::tan(p.step/2)+s.unitsPerMm;
    double low=-p.relief-s.unitsPerMm*.1;
    std::vector<tag_t> curves;
    auto line=[&](double x,double z,double x2,double z2){curves.push_back(Line(at(x,-half,z),at(x2,-half,z2)));};
    auto arc=[&](double r){curves.push_back(Arc(at(r,-half,0),at(r*std::cos(s.latitude/2),-half,r*std::sin(s.latitude/2)),at(r*std::cos(s.latitude),-half,r*std::sin(s.latitude))));};
    line(p.outer,low,p.outer,0);arc(p.outer);
    line(p.outer*std::cos(s.latitude),p.outer*std::sin(s.latitude),p.inner*std::cos(s.latitude),p.inner*std::sin(s.latitude));
    arc(p.inner);line(p.inner,0,p.inner,low);line(p.inner,low,p.outer,low);
    tag_t petal=Extrude(curves,tangent,half*2,s.unitsPerMm);
    double slope=std::tan(p.step/2),inset=p.gap/(2*std::cos(p.step/2)),extent=p.outer*2;
    tag_t wedge=Prism({at(inset/slope,0,low-s.unitsPerMm),at(extent,-extent*slope+inset,low-s.unitsPerMm),at(extent,extent*slope-inset,low-s.unitsPerMm)},s.z,extent+p.relief+2*s.unitsPerMm,s.unitsPerMm);
    petal=Intersect(petal,wedge);
    tag_t band=Prism({at(p.inner,-p.inner*slope,-s.height),at(p.outer,-p.outer*slope,-s.height),at(p.outer,p.outer*slope,-s.height),at(p.inner,p.inner*slope,-s.height)},s.z,s.height-p.relief,s.unitsPerMm);
    Unite(petal,band);return petal;
}
Face* BaseFace(tag_t bodyTag,const Plan& p){
    auto* body=dynamic_cast<Body*>(NXObjectManager::Get(bodyTag));double angle=p.step/2;Vec r=p.source.x*std::cos(angle)+p.source.y*std::sin(angle);
    for(auto* face:body->GetFaces()){auto d=Data(face->Tag());if(d.type==22&&std::abs(Dot(d.axis,r))>.999999&&std::abs(Dot(d.point-p.source.center,r)-p.outer)<1e-5*p.source.unitsPerMm)return face;}
    throw std::runtime_error("未找到分瓣根部的固定平面。");
}
}
static Source InspectPair(tag_t cylinder,tag_t sphere){
    auto* work=Session::GetSession()->Parts()->Work();if(!work)throw std::runtime_error("请先打开零件。");
    auto* c=dynamic_cast<Face*>(NXObjectManager::Get(cylinder));auto* f=dynamic_cast<Face*>(NXObjectManager::Get(sphere));
    if(!c||!f||c->IsOccurrence()||f->IsOccurrence()||c->OwningPart()!=work||f->OwningPart()!=work)throw std::runtime_error("请选择当前工作零件中的圆柱面和球面。");
    if(c->GetBody()!=f->GetBody())throw std::runtime_error("圆柱面和球面必须属于同一个参考体并共享圆弧边。");
    auto cd=Data(cylinder),sd=Data(sphere);if(cd.type!=16||sd.type!=18)throw std::runtime_error("第一个输入必须是圆柱面，第二个必须是真正的球面；暂不支持环面或样条拟合面。");
    Source s;s.cylinder=cylinder;s.sphere=sphere;s.body=c->GetBody()->Tag();s.center=sd.point;s.radius=sd.radius;s.z=Unit(cd.axis);
    s.unitsPerMm=work->PartUnits()==BasePart::UnitsInches?1/25.4:1.;double tol=std::max(1e-5*s.unitsPerMm,s.radius*1e-7);
    if(std::abs(cd.radius-s.radius)>tol||Length((sd.point-cd.point)-s.z*Dot(sd.point-cd.point,s.z))>tol)throw std::runtime_error("需要同轴、同半径且在球面赤道处相切的圆柱面与球面。");
    std::set<tag_t> edges;for(auto* e:c->GetEdges())edges.insert(e->Tag());std::vector<tag_t> shared;
    for(auto* e:f->GetEdges())if(edges.count(e->Tag()))shared.push_back(e->Tag());
    if(shared.size()!=1)throw std::runtime_error("当前要求两个面共享一条完整圆弧边；请先合并被拆分的面或边。");
    Eval seam(shared[0]);logical circular=false;Check(UF_EVAL_is_arc(seam.value,&circular));if(!circular)throw std::runtime_error("相接边不是圆弧。");
    UF_EVAL_arc_t arc={};Check(UF_EVAL_ask_arc(seam.value,&arc));double lim[2];Check(UF_EVAL_ask_limits(seam.value,lim));s.sweep=lim[1]-lim[0];
    if(Length(V(arc.center)-s.center)>tol||std::abs(arc.radius-s.radius)>tol)throw std::runtime_error("相接圆弧不在球面赤道处。");
    auto cp=EdgePoints(c);double minz=0,maxz=0;for(auto point:cp){double z=Dot(point-s.center,s.z);minz=std::min(minz,z);maxz=std::max(maxz,z);}
    if(minz<-tol&&maxz>tol)throw std::runtime_error("圆柱面跨过球面赤道，无法确定展开方向。");
    if(maxz>tol)s.z=s.z*-1;s.height=std::max(-minz,maxz);
    s.x=Unit(seam.At(lim[0])-s.center);s.y=Unit(Cross(s.z,s.x));
    Vec next=seam.At(lim[0]+s.sweep*.01);if(Dot(next-s.center,s.y)<0){s.x=Unit(seam.At(lim[1])-s.center);s.y=Unit(Cross(s.z,s.x));}
    double top=0;for(auto point:EdgePoints(f)){double z=Dot(point-s.center,s.z);if(z<-tol)throw std::runtime_error("球面与圆柱面位于相接圆的同一侧。");top=std::max(top,z);}
    s.latitude=std::asin(std::clamp(top/s.radius,0.,1.));
    if(s.height<tol||s.latitude<pi/180||s.sweep<pi/180||s.sweep>2*pi+1e-7)throw std::runtime_error("所选面范围过小或无法识别球冠边界。");
    ValidateBoundary(s,c,false);ValidateBoundary(s,f,true);return s;
}
Source Inspect(tag_t cylinder,tag_t sphere){
    auto s=InspectPair(cylinder,sphere);
    auto* body=dynamic_cast<Body*>(NXObjectManager::Get(s.body));
    if(!body->IsSolidBody())throw std::runtime_error("自动识别板厚需要等厚实体，请选择实体上的圆柱面和球面。");
    const double tol=std::max(1e-5*s.unitsPerMm,s.radius*1e-7);
    std::vector<Source> matches;
    for(auto* oppositeSphere:body->GetFaces()){
        auto sd=Data(oppositeSphere->Tag());
        if(sd.type!=18||Length(sd.point-s.center)>tol||std::abs(sd.radius-s.radius)<tol)continue;
        for(auto* oppositeCylinder:body->GetFaces()){
            auto cd=Data(oppositeCylinder->Tag());
            if(cd.type!=16||std::abs(cd.radius-sd.radius)>tol)continue;
            Source other;
            try{other=InspectPair(oppositeCylinder->Tag(),oppositeSphere->Tag());}catch(const std::exception&){continue;}
            if(Dot(other.z,s.z)<1-1e-7||std::abs(other.height-s.height)>tol||std::abs(other.sweep-s.sweep)>1e-6||std::abs(other.latitude-s.latitude)>1e-6)continue;
            if(s.sweep<2*pi-1e-6&&Dot(other.x,s.x)<1-1e-7)continue;
            const double lo=std::min(sd.radius,s.radius),hi=std::max(sd.radius,s.radius),t=hi-lo;
            bool valid=true;
            // Check both walls and the material between them in several sections.
            // This rejects separate concentric shells and nonuniform wall pairs.
            for(double a:{.2,.5,.8})for(double b:{.25,.5,.75})for(bool spherical:{false,true}){
                Vec radial=s.x*std::cos(s.sweep*a)+s.y*std::sin(s.sweep*a);
                for(int sample=0;sample<5;++sample){
                    double r=sample==0?lo-t*.1:sample==4?hi+t*.1:lo+t*sample/4;
                    double phi=s.latitude*b;
                    Vec q=s.center+radial*(spherical?r*std::cos(phi):r)+s.z*(spherical?r*std::sin(phi):-s.height*b);
                    double xyz[]={q.x,q.y,q.z};int state=0;Check(UF_MODL_ask_point_containment(xyz,s.body,&state));
                    if(state!=((sample==0||sample==4)?2:1))valid=false;
                }
            }
            if(valid)matches.push_back(other);
        }
    }
    if(matches.size()!=1)throw std::runtime_error("无法唯一识别等厚内外壁：圆柱段与球面段须具有同轴、同心且厚度一致的对应面。");
    s.thickness=std::abs(matches[0].radius-s.radius);s.innerSurface=matches[0].radius>s.radius;
    return s;
}
Plan MakePlan(const Source& source,const Settings& settings){
    Plan p;p.source=source;p.settings=settings;double u=source.unitsPerMm;
    if(settings.petals<2||settings.petals>180)throw std::runtime_error("瓣数范围为 2–180。");
    for(double v:{source.thickness,settings.gap,settings.relief,source.radius,source.height,source.sweep,source.latitude,u})if(!std::isfinite(v)||v<=0)throw std::runtime_error("板厚、缝宽和根部避让必须是正数。");
    if(source.thickness/u<.05-1e-8||settings.gap<.05)throw std::runtime_error("板厚与缝宽不得小于 0.05 mm。");
    p.outer=source.radius+(source.innerSurface?source.thickness:0);p.inner=p.outer-source.thickness;p.gap=settings.gap*u;p.relief=settings.relief*u;
    if(p.inner<=0||source.thickness>source.radius*.2)throw std::runtime_error("板厚相对于球面半径过大。");
    if(p.relief<p.gap)throw std::runtime_error("根部避让深度不能小于瓣间隙。");
    if(p.relief+source.thickness>=source.height)throw std::runtime_error("根部避让深度须小于圆柱高度减板厚，以保留底部连接带。");
    // A full revolution requires an open seam through the root band as well.
    p.sweep=source.sweep-(source.sweep>2*pi-1e-6?p.gap/p.outer:0.);
    p.step=p.sweep/settings.petals;
    if(p.step>pi/6)throw std::runtime_error("每瓣周向角度不能超过 30°，请增加瓣数。");
    if(p.gap>=p.inner*2*std::sin(p.step/2)*.8)throw std::runtime_error("瓣间隙过大；请减小缝宽或减少瓣数。");
    p.rootGap=std::max(p.gap,2*(p.outer-p.inner+.02*u)*std::sin(p.step/2)+.1*u);
    if(p.rootGap>=p.inner*2*std::sin(p.step/2)*.8)throw std::runtime_error("根部折弯避让后连接过窄；请减少瓣数或板厚。");
    p.errorMm=p.outer*(1/std::cos(p.step/2)-1)/u;return p;
}
std::vector<std::pair<Vec,Vec>> Preview(const Plan& p){
    std::vector<std::pair<Vec,Vec>> result;const auto& s=p.source;
    for(int i=0;i<p.settings.petals;++i){double a=p.step*(i+.5);Vec r=s.x*std::cos(a)+s.y*std::sin(a),t=s.x*(-std::sin(a))+s.y*std::cos(a);
        auto at=[&](double phi,int side){double radius=p.outer*std::cos(phi),rootHalf=p.outer*std::tan(p.step/2)-p.rootGap/(2*std::cos(p.step/2));double width=std::max(0.,std::min(rootHalf,radius*std::tan(p.step/2)-p.gap/(2*std::cos(p.step/2))));return s.center+r*radius+t*(side*width)+s.z*(p.outer*std::sin(phi));};
        for(int side:{-1,1}){result.emplace_back(at(0,side)-s.z*p.relief,at(0,side));for(int j=0;j<40;++j)result.emplace_back(at(s.latitude*j/40,side),at(s.latitude*(j+1)/40,side));}
        result.emplace_back(at(s.latitude,-1),at(s.latitude,1));
    }return result;
}
Result Create(const Plan& p){
    auto* session=Session::GetSession();auto* part=session->Parts()->Work();std::set<tag_t> old;
    for(auto* f:part->Features()->GetFeatures())old.insert(f->Tag());
    Result result;result.body=Petal(p,0);for(int i=1;i<p.settings.petals;++i)Unite(result.body,Petal(p,i));
    auto* manager=part->Features()->SheetmetalManager();auto* convert=manager->CreateConvertToSheetmetalFeatureBuilder(nullptr);
    try{convert->SetBaseFace(BaseFace(result.body,p));convert->SetMaintainZeroBendRadius(true);
        auto* f=convert->CommitFeature();result.convert=f->Tag();f->SetName(NXString("球面分瓣钣金",NXString::UTF8));convert->Destroy();convert=nullptr;
        auto errors=f->GetFeatureErrorMessages();if(!errors.empty())throw std::runtime_error(std::string("NX 钣金转换失败：")+errors.front().GetText());
        auto messages=f->GetFeatureWarningMessages();if(!messages.empty())throw std::runtime_error(std::string("NX 钣金转换警告：")+messages.front().GetText());
    }catch(...){if(convert)convert->Destroy();throw;}
    // A failed or partially formed flat body is a failed command; callers roll
    // back the entire operation. Never report a mere conversion as unfoldable.
    auto mark=session->SetUndoMark(Session::MarkVisibilityInvisible,"验证球面分瓣展开");
    auto* flat=manager->CreateFlatSolidFeatureBuilder(nullptr);
    try{
        flat->StationaryFace()->SetValue(BaseFace(result.body,p));flat->SetAssociative(true);flat->SetOrientation(Features::SheetMetal::FlatSolidBuilder::OrientationTypeDefault);auto* f=flat->CommitFeature();flat->Destroy();flat=nullptr;
        auto errors=f->GetFeatureErrorMessages();if(!errors.empty())throw std::runtime_error(std::string("NX 展开失败：")+errors.front().GetText());
        auto warnings=f->GetFeatureWarningMessages();if(!warnings.empty())throw std::runtime_error(std::string("NX 展开警告：")+warnings.front().GetText());
        auto bodies=f->GetBodies();if(bodies.size()!=1||!bodies[0]->IsSolidBody())throw std::runtime_error("NX 展平未生成单个实体。");
        // Rounded relief walls may legitimately be cylindrical in a flat
        // sheet. Check the exact solid envelope normal to the fixed face,
        // rather than rejecting those thickness-wall cylinders.
        double angle=p.step/2;Vec normal=p.source.x*std::cos(angle)+p.source.y*std::sin(angle),x=p.source.z,y=Cross(normal,x);
        double matrix[]={x.x,x.y,x.z,y.x,y.y,y.z,normal.x,normal.y,normal.z},origin[3]={};tag_t matrixTag=0,csys=0;
        Check(UF_CSYS_create_matrix(matrix,&matrixTag));Check(UF_CSYS_create_csys(origin,matrixTag,&csys));
        double corner[3],directions[3][3],distances[3];int boxStatus=UF_MODL_ask_bounding_box_exact(bodies[0]->Tag(),csys,corner,directions,distances);UF_OBJ_delete_object(csys);Check(boxStatus);
        double thickness=p.outer-p.inner;
        if(std::abs(distances[2]-thickness)>std::max(.005*p.source.unitsPerMm,thickness*.005))throw std::runtime_error("NX 未能完全展平，已取消本次创建。请增加瓣数或瓣间隙，使根部折弯避开切缝。");
        result.flatBody=bodies[0]->Tag();f->SetName(NXString("球面分瓣展开",NXString::UTF8));
        if(!p.settings.flat){session->UndoToMark(mark,nullptr);result.flatBody=0;}
        session->DeleteUndoMark(mark,nullptr);
    }catch(...){if(flat)flat->Destroy();try{session->UndoToMark(mark,nullptr);session->DeleteUndoMark(mark,nullptr);}catch(...){}throw;}
    // Internal sketches remain owned by their extrusion. The custom feature
    // owns the remaining native history as construction features.
    for(auto* f:part->Features()->GetFeatures())if(!old.count(f->Tag())&&!f->IsInternal())result.members.push_back(f->Tag());
    Check(UF_OBJ_set_color(result.body,186));Check(UF_OBJ_set_blank_status(result.body,p.settings.flat?UF_OBJ_BLANKED:UF_OBJ_NOT_BLANKED));
    if(result.flatBody){Check(UF_OBJ_set_color(result.flatBody,70));Check(UF_OBJ_set_blank_status(result.flatBody,UF_OBJ_NOT_BLANKED));}
    if(p.settings.hideSource&&p.source.body)Check(UF_OBJ_set_blank_status(p.source.body,UF_OBJ_BLANKED));
    return result;
}
}
