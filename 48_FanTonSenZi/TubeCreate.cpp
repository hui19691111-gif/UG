#include "TubeGeometry.hpp"
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


namespace tube_straighten {
using namespace NXOpen;
namespace {
Vec V(const double* p){return {p[0],p[1],p[2]};}
std::string Number(double v){std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(16)<<v;return out.str();}
struct Eval {
    UF_EVAL_p_t value=nullptr;
    explicit Eval(tag_t edge){Check(UF_EVAL_initialize(edge,&value));}
    ~Eval(){if(value)UF_EVAL_free(value);}
    Vec At(double t){double p[3];Check(UF_EVAL_evaluate(value,0,t,p,nullptr));return V(p);}
};
tag_t Line(Vec p,Vec q){UF_CURVE_line_t l={{p.x,p.y,p.z},{q.x,q.y,q.z}};tag_t t=0;Check(UF_CURVE_create_line(&l,&t));return t;}
tag_t Arc(Vec a,Vec b,Vec c){double p[]={a.x,a.y,a.z},q[]={b.x,b.y,b.z},r[]={c.x,c.y,c.z};tag_t t=0;Check(UF_CURVE_create_arc_thru_3pts(1,p,q,r,&t));return t;}
tag_t SlotRootArc(const Plan& p,const SourceSlot& s,double h,double z){
    // A narrow gap has almost collinear endpoints/midpoint. Use its known
    // center/radius instead of fitting three points, especially in inch parts.
    Vec x=Unit(Cross(p.source.normal,s.axis)*(-1)),y=Unit(s.axis),normal=Unit(Cross(x,y)),center=s.center+p.source.widthDirection*z;
    double matrix[]={x.x,x.y,x.z,y.x,y.y,y.z,normal.x,normal.y,normal.z};tag_t frame=0,curve=0;Check(UF_CSYS_create_matrix(matrix,&frame));
    UF_CURVE_arc_t arc={};arc.matrix_tag=frame;arc.radius=s.pathRadius-p.source.thickness;arc.start_angle=-std::asin(h/arc.radius);arc.end_angle=-arc.start_angle;
    arc.arc_center[0]=Dot(center,x);arc.arc_center[1]=Dot(center,y);arc.arc_center[2]=Dot(center,normal);Check(UF_CURVE_create_arc(&arc,&curve));return curve;
}
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
        if(!sketch||!sketch->Feature())throw std::runtime_error("创建方通拉伸截面草图失败。");
        sketch->Activate(Sketch::ViewReorientFalse);
        for(auto tag:curves)sketch->AddGeometry(dynamic_cast<Curve*>(NXObjectManager::Get(tag)),Sketch::InferConstraintsOptionInferNoConstraints);
        sketch->Update();sketch->Deactivate(Sketch::ViewReorientFalse,Sketch::UpdateLevelModel);
        sketch->Feature()->SetName(NXString("方通拉伸截面",NXString::UTF8));sketch->Blank();return sketch;
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
        if(bodies.size()!=1||!bodies[0]->IsSolidBody())throw std::runtime_error("方通拉伸未生成单个实体。");
        if(!sketch->Feature()->IsInternal())result->MakeSketchInternal();
        if(!sketch->Feature()->IsInternal())throw std::runtime_error("截面草图未能放入拉伸特征内部。");
        for(auto t:curves)Check(UF_OBJ_set_blank_status(t,UF_OBJ_BLANKED));return bodies[0]->Tag();
    }catch(...){if(builder)builder->Destroy();throw;}
}
tag_t RoundedPrism(const Plan& p,double y0,double z0,double depth,double width,double radius,double x,double length){
    auto at=[&](double y,double z){return FlatPoint(p,x,y+y0,z+z0);};std::vector<tag_t> curves;
    if(p.source.round){
        auto circle=[&](double a){return at(depth/2-radius*cos(a),width/2+radius*sin(a));};
        for(int i=0;i<4;++i)curves.push_back(Arc(circle(i*pi/2),circle((i+.5)*pi/2),circle((i+1)*pi/2)));
    }
    else if(radius<1e-10){std::vector<Vec> q={at(0,0),at(depth,0),at(depth,width),at(0,width)};for(size_t i=0;i<q.size();++i)curves.push_back(Line(q[i],q[(i+1)%q.size()]));}
    else {double r=radius,k=r/std::sqrt(2.);curves={
        Line(at(r,0),at(depth-r,0)),Arc(at(depth-r,0),at(depth-r+k,r-k),at(depth,r)),
        Line(at(depth,r),at(depth,width-r)),Arc(at(depth,width-r),at(depth-r+k,width-r+k),at(depth-r,width)),
        Line(at(depth-r,width),at(r,width)),Arc(at(r,width),at(r-k,width-r+k),at(0,width-r)),
        Line(at(0,width-r),at(0,r)),Arc(at(0,r),at(r-k,r-k),at(r,0))};}
    return Extrude(curves,p.source.spans.front().Tangent(0),length,p.source.unitsPerMm);
}
tag_t HoleTool(const Plan& p,const Hole& h){
    double pad=.01*p.source.unitsPerMm;Vec axis=p.source.spans.front().Tangent(0),inside=Cross(p.source.normal,axis),width=p.source.widthDirection;
    auto vector=[&](Vec q){return axis*q.x+inside*q.y+width*q.z;};
    auto at=[&](Vec q){q=q-h.direction*pad;return FlatPoint(p,q.x,q.y,q.z);};std::vector<tag_t> curves;
    for(const auto& c:h.profile){if(!c.radius)curves.push_back(Line(at(c.a),at(c.b)));else{
        // Splitting a full circle also gives the internal sketch a stable axis.
        int pieces=c.angle>pi+1e-8?2:1;
        for(int i=0;i<pieces;++i)curves.push_back(Arc(at(c.Point(double(i)/pieces)),at(c.Point((i+.5)/pieces)),at(c.Point(double(i+1)/pieces))));
    }}
    return Extrude(curves,vector(h.direction),h.length+2*pad,p.source.unitsPerMm);
}
tag_t Prism(const std::vector<Vec>& points,Vec direction,double length,double units){
    std::vector<tag_t> curves;for(size_t i=0;i<points.size();++i)curves.push_back(Line(points[i],points[(i+1)%points.size()]));return Extrude(curves,direction,length,units);
}
tag_t RevolvedTriangle(const std::vector<Vec>& points,Vec center,Vec normal,double angle){
    std::vector<tag_t> curves;for(size_t i=0;i<points.size();++i)curves.push_back(Line(points[i],points[(i+1)%points.size()]));
    char zero[]="0";auto end=Number(angle*180/pi);char* limits[]={zero,end.data()},*offsets[]={zero,zero};
    double axis[]={normal.x,normal.y,normal.z},origin[]={center.x,center.y,center.z},region[3]={};tag_t* features=nullptr;int count=0;
    Check(UF_MODL_create_revolution(curves.data(),static_cast<int>(curves.size()),nullptr,limits,offsets,region,false,true,origin,axis,UF_NULLSIGN,&features,&count));
    tag_t body=0;try{if(count!=1)throw std::runtime_error("圆管连接带保护体创建失败。");Check(UF_MODL_ask_feat_body(features[0],&body));}catch(...){UF_free(features);throw;}UF_free(features);
    for(auto curve:curves)Check(UF_OBJ_set_blank_status(curve,UF_OBJ_BLANKED));return body;
}
int Contains(tag_t body,Vec q){double xyz[]={q.x,q.y,q.z};int status=0;Check(UF_MODL_ask_point_containment(xyz,body,&status));return status;}
Vec SourceMaterialPoint(const Plan& p,const SourceSlot& s,double x,double depth,double z){
    double y=s.pathRadius?s.pathRadius-std::sqrt((s.pathRadius-depth)*(s.pathRadius-depth)-x*x):(depth+std::abs(x)*std::sqrt(1-s.cornerCos*s.cornerCos))/s.cornerCos;
    return SourceSlotPoint(p,s,x,y,z);
}
void CutSourceSlots(const Plan& p){
    if(!p.settings.cutSource)return;
    double w=p.source.width,d=p.source.depth,t=p.source.thickness,u=p.source.unitsPerMm,h=p.gap/2;
    if(p.sourceSlots.size()!=p.bends.size()||h<.005*u)throw std::runtime_error("原管间隙槽规划无效。");
    struct Probe{Vec q;int expected;};std::vector<Probe> probes;
    for(const auto& s:p.sourceSlots)for(double x:{-.8*h,0.,.8*h}){
        auto sample=[&](double depth,double z,int expected){Vec q=SourceMaterialPoint(p,s,x,depth,z);if(Contains(p.source.body,q)!=1)throw std::runtime_error("原管槽位的壁厚检查失败，已取消开槽。");probes.push_back({q,expected});};
        if(p.source.round){double r=d/2,alpha=BridgeHalfAngle(p);for(double angle:{-.9*alpha,0.,.9*alpha,1.2*alpha,pi/2,pi,3*pi/2})for(double rho:{r-.1*t,r-.9*t})sample(r-rho*cos(angle),r+rho*sin(angle),std::abs(angle)<alpha?1:2);}
        else{for(double depth:{.1*t,.9*t})sample(depth,w/2,1);sample(d-t/2,w/2,2);sample(d/2,t/2,2);sample(d/2,w-t/2,2);}
    }
    double volume=Volume(p.source.body);
    for(const auto& s:p.sourceSlots){
        auto at=[&](double x,double y,double z){return SourceSlotPoint(p,s,x,y,z);};tag_t feature=0;
        auto subtract=[&](tag_t cut){
            Check(UF_MODL_subtract_bodies_with_retained_options(p.source.body,cut,false,false,&feature));
            auto* result=dynamic_cast<Features::Feature*>(NXObjectManager::Get(feature));
            if(!result||result->GetBodies().size()!=1||!result->GetBodies()[0]->IsSolidBody())throw std::runtime_error("原管开槽导致实体断开，已取消生成。");
        };
        if(!p.source.round){
            double top=SourceSlotTop(p,s,h),bottom=d/s.cornerCos+u;std::vector<tag_t> curves;
            if(s.pathRadius)curves.push_back(SlotRootArc(p,s,h,-u));
            else{curves.push_back(Line(at(-h,top,-u),at(0,t/s.cornerCos,-u)));curves.push_back(Line(at(0,t/s.cornerCos,-u),at(h,top,-u)));}
            curves.push_back(Line(at(h,top,-u),at(h,bottom,-u)));curves.push_back(Line(at(h,bottom,-u),at(-h,bottom,-u)));curves.push_back(Line(at(-h,bottom,-u),at(-h,top,-u)));
            subtract(Extrude(curves,p.source.widthDirection,w+2*u,u));
        }else{
            // Circular bridges are protected through their full radial wall,
            // following the torus on arcs and each cylinder at a sharp corner.
            double r=d/2,half=(r+2*u)*std::tan(BridgeHalfAngle(p));
            int pieces=s.pathRadius?1:2;
            for(int side=0;side<pieces;++side){
                double a=pieces==1?-h:(side?0:-h),b=pieces==1?h:(side?h:0);
                tag_t cut=Prism({at(a,-u,-u),at(b,-u,-u),at(b,d/s.cornerCos+u,-u),at(a,d/s.cornerCos+u,-u)},p.source.widthDirection,w+2*u,u),protect=0;
                if(s.pathRadius){
                    double delta=std::asin(h/(s.pathRadius-d))+.01;
                    auto rotate=[&](Vec q){Vec v=q-s.center;return s.center+v*cos(delta)-Cross(p.source.normal,v)*sin(delta)+p.source.normal*(Dot(p.source.normal,v)*(1-cos(delta)));};
                    protect=RevolvedTriangle({rotate(at(0,r,r)),rotate(at(0,-2*u,r-half)),rotate(at(0,-2*u,r+half))},s.center,p.source.normal,2*delta);
                }else{
                    Vec axis=side?s.outgoing:s.incoming,inside=Cross(p.source.normal,axis);double extent=4*(d/s.cornerCos+p.gap+u);
                    auto q=[&](double y,double z){return s.origin+inside*y+p.source.widthDirection*z-axis*extent;};
                    protect=Prism({q(r,r),q(-2*u,r-half),q(-2*u,r+half)},axis,2*extent,u);
                }
                Check(UF_MODL_subtract_bodies_with_retained_options(cut,protect,false,false,&feature));subtract(cut);
            }
        }
    }
    double after=Volume(p.source.body);if(after<=0||after>=volume)throw std::runtime_error("原管间隙槽未去除有效材料。");
    for(const auto& probe:probes)if(Contains(p.source.body,probe.q)!=probe.expected)throw std::runtime_error("原管间隙槽或保留连接壁检查失败，已取消生成。");
    // The source is trimmed in place; every original hole boundary stays put.
    for(const auto& hole:p.source.holes)for(const auto& curve:hole.profile)for(double f:{0.,.25,.5,.75})if(Contains(p.source.body,curve.Point(f))!=3)throw std::runtime_error("原管开槽影响了已有孔槽，已取消生成。");
}

}
tag_t Create(const Plan& p){
    auto* part=Session::GetSession()->Parts()->Work();
    std::set<tag_t> before;for(auto* f:part->Features()->GetFeatures())before.insert(f->Tag());
    double w=p.source.width,d=p.source.depth,t=p.source.thickness,u=p.source.unitsPerMm;
    Vec axis=p.source.spans.front().Tangent(0),width=p.source.widthDirection;
    auto at=[&](double x,double y,double z){return FlatPoint(p,x,y,z);};
    tag_t body=RoundedPrism(p,0,0,d,w,p.source.cornerRadius,0,p.length);
    tag_t hollow=RoundedPrism(p,t,t,d-2*t,w-2*t,std::max(0.,p.source.cornerRadius-t),-u,p.length+2*u);
    tag_t feature=0;Check(UF_MODL_subtract_bodies_with_retained_options(body,hollow,false,false,&feature));
    for(const auto& bend:p.bends){
        auto points=Notch(p,bend,u);for(auto& q:points)q=at(q.x,q.y,-u);
        tag_t cut=Prism(points,width,w+2*u,u);
        if(p.source.round){
            // Protect an angular strip through the complete radial wall. A
            // horizontal cut alone would taper the bridge to zero at its sides.
            double r=d/2,half=(r+2*u)*std::tan(BridgeHalfAngle(p));
            std::vector<Vec> sector={at(-u,r,r),at(-u,-2*u,r-half),at(-u,-2*u,r+half)};
            tag_t protect=Prism(sector,axis,p.length+2*u,u);
            Check(UF_MODL_subtract_bodies_with_retained_options(cut,protect,false,false,&feature));
        }
        Check(UF_MODL_subtract_bodies_with_retained_options(body,cut,false,false,&feature));
    }
    auto* output=dynamic_cast<Body*>(NXObjectManager::Get(body));
    if(!output||!output->IsSolidBody()||Volume(body)<=0)throw std::runtime_error("伸直未生成有效实体。");
    // Every hinge must retain exactly one wall; verify solid material and removed stock.
    for(const auto& b:p.bends)for(double z:{std::max(t/2,p.source.cornerRadius),w/2,w-std::max(t/2,p.source.cornerRadius)})for(double y:{t/2,t+(d-t)/2}){
        Vec q=at(b.start+b.allowance/2,y,z);double xyz[]={q.x,q.y,q.z};int status=0;
        Check(UF_MODL_ask_point_containment(xyz,body,&status));if(status!=(y<t?1:2))throw std::runtime_error("连接壁或切口检查失败。");
    }
    if(p.source.round)for(const auto& b:p.bends)for(double angle:{-.9*BridgeHalfAngle(p),0.,.9*BridgeHalfAngle(p)})for(double radius:{d/2-t*.1,d/2-t*.9}){
        Vec q=at(b.start+b.allowance/2,d/2-radius*cos(angle),w/2+radius*sin(angle));double xyz[]={q.x,q.y,q.z};int status=0;Check(UF_MODL_ask_point_containment(xyz,body,&status));if(status!=1)throw std::runtime_error("圆管连接带未保留完整壁厚，已取消生成。");
    }
    double holeVolume=0,volumeBefore=Volume(body)*1e9*std::pow(u,3);
    for(size_t i=0;i<p.source.holes.size();++i){auto hole=FlatHole(p,i);tag_t tool=HoleTool(p,hole);Check(UF_MODL_subtract_bodies_with_retained_options(body,tool,false,false,&feature));holeVolume+=hole.area*hole.length;
        for(const auto& c:hole.profile)for(double f:{0.,.25,.5,.75}){Vec local=c.Point(f),q=at(local.x,local.y,local.z);double xyz[]={q.x,q.y,q.z};int status=0;Check(UF_MODL_ask_point_containment(xyz,body,&status));if(status!=3)throw std::runtime_error("孔轮廓未能完整保留，已取消生成。");}
    }
    double removed=volumeBefore-Volume(body)*1e9*std::pow(u,3);
    if(std::abs(removed-holeVolume)>std::max(.0001*u*u*u,holeVolume*1e-6))throw std::runtime_error("展开后的孔槽去料体积不一致，已取消生成。");
    CutSourceSlots(p);
    output->SetName(NXString(p.source.round?"圆管伸直_下料实体":"方通伸直_下料实体",NXString::UTF8));
    std::vector<tag_t> members;for(auto* f:part->Features()->GetFeatures())if(!before.count(f->Tag())&&!f->IsInternal())members.push_back(f->Tag());
    char groupName[]="FanTonSenZi";tag_t group=0;Check(UF_MODL_create_set_of_feature(groupName,members.data(),static_cast<int>(members.size()),false,&group));
    auto* object=dynamic_cast<NXObject*>(NXObjectManager::Get(group));
    object->SetName(NXString(std::string(p.source.round?"圆管伸直_":"方通伸直_")+(p.machineArcs.empty()?"":"弯管机_")+std::to_string(p.bends.size())+"切口",NXString::UTF8));
    object->SetUserAttribute("FTSZ_HoleWallCount",-1,static_cast<int>(p.source.holes.size()),Update::OptionNow);
    object->SetUserAttribute("FTSZ_RoundTube",-1,p.source.round?1:0,Update::OptionNow);
    object->SetUserAttribute("FTSZ_SourceSlots",-1,p.settings.cutSource?1:0,Update::OptionNow);
    object->SetUserAttribute("FTSZ_SegmentArcs",-1,p.settings.segmentArcs?1:0,Update::OptionNow);
    object->SetUserAttribute("FTSZ_TubeKFactor",-1,p.settings.tubeKFactor,Update::OptionNow);
    object->SetUserAttribute("FTSZ_MachineArcCount",-1,static_cast<int>(p.machineArcs.size()),Update::OptionNow);
    if(p.source.round)object->SetUserAttribute("FTSZ_BridgeWidth_mm",-1,p.settings.bridgeWidthMm,Update::OptionNow);
    object->SetUserAttribute("FTSZ_SectionRadius_mm",-1,p.source.cornerRadius/u,Update::OptionNow);
    object->SetUserAttribute("FTSZ_Length_mm",-1,p.length/u,Update::OptionNow);
    object->SetUserAttribute("FTSZ_Thickness_mm",-1,t/u,Update::OptionNow);
    object->SetUserAttribute("FTSZ_InsideRadius_mm",-1,p.settings.radiusMm,Update::OptionNow);
    object->SetUserAttribute("FTSZ_KFactor",-1,p.settings.kFactor,Update::OptionNow);
    object->SetUserAttribute("FTSZ_Gap_mm",-1,p.settings.gapMm,Update::OptionNow);
    object->SetUserAttribute("FTSZ_Approximation_mm",-1,p.errorMm,Update::OptionNow);
    if(p.settings.hideSource&&!p.settings.cutSource)Check(UF_OBJ_set_blank_status(p.source.body,UF_OBJ_BLANKED));
    if(p.settings.cutSource)Check(UF_OBJ_set_blank_status(p.source.body,UF_OBJ_NOT_BLANKED));
    Check(UF_OBJ_set_color(body,186));return body;
}
}
