#include "BendSimulation.hpp"
#include <NXOpen/Body.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/Edge.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/Curve.hxx>
#include <NXOpen/CurveDumbRule.hxx>
#include <NXOpen/BodyDumbRule.hxx>
#include <NXOpen/ScCollector.hxx>
#include <NXOpen/ScCollectorCollection.hxx>
#include <NXOpen/Features_BooleanBuilder.hxx>
#include <NXOpen/ScRuleFactory.hxx>
#include <NXOpen/Section.hxx>
#include <NXOpen/SectionCollection.hxx>
#include <NXOpen/DirectionCollection.hxx>
#include <NXOpen/Features_ExtrudeBuilder.hxx>
#include <NXOpen/Features_FeatureCollection.hxx>
#include <NXOpen/Features_Feature.hxx>
#include <NXOpen/GeometricUtilities_Limits.hxx>
#include <NXOpen/GeometricUtilities_Extend.hxx>
#include <NXOpen/GeometricUtilities_BooleanOperation.hxx>
#include <NXOpen/Expression.hxx>
#include <uf.h>
#include <uf_assem.h>
#include <uf_curve.h>
#include <uf_eval.h>
#include <uf_modl.h>
#include <uf_obj.h>
#include <uf_part.h>
#include <algorithm>
#include <cmath>
#include <exception>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace bend_sim {
void Check(int code){if(code){char message[1024]={};UF_get_fail_message(code,message);throw std::runtime_error(std::string(message)+" ("+std::to_string(code)+")");}}
namespace {
Vec V(const double* a){return {a[0],a[1],a[2]};}
Vec V(NXOpen::Point3d a){return {a.X,a.Y,a.Z};}
Vec Canonical(Vec a){a=Unit(a);double v=std::abs(a.x)>=std::abs(a.y)&&std::abs(a.x)>=std::abs(a.z)?a.x:std::abs(a.y)>=std::abs(a.z)?a.y:a.z;return v<0?a*-1:a;}
Vec Normal(NXOpen::Face* f,Vec point){
    double p[]={point.x,point.y,point.z},uv[2],on[3],u1[3],v1[3],u2[3],v2[3],n[3],r[2];
    Check(UF_MODL_ask_face_parm(f->Tag(),p,uv,on));Check(UF_MODL_ask_face_props(f->Tag(),uv,on,u1,v1,u2,v2,n,r));return Unit(V(n));
}
void AddNormal(std::vector<Vec>& normals,Vec n){for(auto v:normals)if(Dot(v,n)>.999999)return;normals.push_back(n);}
int Contains(tag_t b,Vec p){double q[]={p.x,p.y,p.z};int r=0;Check(UF_MODL_ask_point_containment(q,b,&r));return r;}
std::string Number(double v){std::ostringstream s;s.imbue(std::locale::classic());s<<std::setprecision(16)<<v;return s.str();}
double ChordError(Vec p,Vec a,Vec b){
    Vec d=b-a;double length2=Dot(d,d);
    double t=length2>0?std::clamp(Dot(p-a,d)/length2,0.0,1.0):0;
    Vec off=p-(a+d*t);return std::sqrt(Dot(off,off));
}
void SampleEdge(NXOpen::Edge* edge,double tolerance,std::vector<std::pair<Vec,Vec>>& lines){
    if(edge->SolidEdgeType()==NXOpen::Edge::EdgeTypeLinear){
        NXOpen::Point3d a,b;edge->GetVertices(&a,&b);lines.emplace_back(V(a),V(b));return;
    }
    UF_EVAL_p_t evaluator=nullptr;Check(UF_EVAL_initialize(edge->Tag(),&evaluator));
    try{
        double limits[2];Check(UF_EVAL_ask_limits(evaluator,limits));
        auto at=[&](double t){double p[3];Check(UF_EVAL_evaluate(evaluator,0,t,p,nullptr));return V(p);};
        auto subdivide=[&](auto&& self,double start,double end,Vec a,Vec b,int depth)->void{
            double mid=(start+end)/2;Vec q=at(mid),q1=at((start+mid)/2),q3=at((mid+end)/2);
            if(std::max({ChordError(q,a,b),ChordError(q1,a,b),ChordError(q3,a,b)})<=tolerance){
                if(lines.size()>=100000)throw std::runtime_error("干涉边界过于复杂，无法完整显示。");
                lines.emplace_back(a,b);return;
            }
            if(depth>=20)throw std::runtime_error("干涉边界离散失败，请检查模型。");
            self(self,start,mid,a,q,depth+1);self(self,mid,end,q,b,depth+1);
        };
        // Seed separate intervals so closed curves cannot collapse to a chord.
        for(int i=0;i<16;++i){double a=limits[0]+(limits[1]-limits[0])*i/16,b=limits[0]+(limits[1]-limits[0])*(i+1)/16;subdivide(subdivide,a,b,at(a),at(b),0);}
    }catch(...){UF_EVAL_free(evaluator);throw;}
    Check(UF_EVAL_free(evaluator));
}
void CollectInterference(tag_t part,tag_t tool,double units,Result& result){
    // Retain both inputs; the caller rolls the Boolean and all tool geometry back.
    auto* work=NXOpen::Session::GetSession()->Parts()->Work();
    auto* builder=work->Features()->CreateBooleanBuilderUsingCollector(nullptr);
    NXOpen::Features::Feature* f=nullptr;
    try{
        builder->SetOperation(NXOpen::Features::Feature::BooleanTypeIntersect);
        builder->SetTarget(dynamic_cast<NXOpen::Body*>(NXOpen::NXObjectManager::Get(tool)));
        builder->SetRetainTarget(true);builder->SetRetainTool(true);
        auto* rule=work->ScRuleFactory()->CreateRuleBodyDumb({dynamic_cast<NXOpen::Body*>(NXOpen::NXObjectManager::Get(part))});
        auto* collector=work->ScCollectors()->CreateCollector();collector->ReplaceRules({rule},false);builder->SetToolBodyCollector(collector);
        builder->SetTolerance(.0001*units);f=builder->CommitFeature();
    }catch(...){builder->Destroy();throw;}
    builder->Destroy();
    if(!f)throw std::runtime_error("无法取得实际干涉区域。");
    for(auto* body:f->GetBodies()){
        if(body->Tag()==part||body->Tag()==tool)throw std::runtime_error("干涉区域返回了原实体，已停止显示。");
        if(!body->IsSolidBody())continue;
        ++result.interferenceRegions;
        for(auto* edge:body->GetEdges())SampleEdge(edge,.005*units,result.interferenceLines);
    }
    if(result.interferenceRegions==0||result.interferenceLines.empty())throw std::runtime_error("已检测到干涉，但未能提取干涉边界。");
}
}
Bend Inspect(tag_t selected,double sharpInnerRadiusMm){
    auto* object=NXOpen::NXObjectManager::Get(selected);auto* face=dynamic_cast<NXOpen::Face*>(object);auto* edge=dynamic_cast<NXOpen::Edge*>(object);
    if((!face&&!edge)||(face&&face->IsOccurrence())||(edge&&edge->IsOccurrence()))throw std::runtime_error("请选择当前工作零件的内折弯圆柱面或内侧直线锐边。");
    auto* body=face?face->GetBody():edge->GetBody();
    tag_t owning=0;if(body)Check(UF_OBJ_ask_owning_part(body->Tag(),&owning));
    if(!body||!body->IsSolidBody()||owning!=UF_ASSEM_ask_work_part())throw std::runtime_error("只支持当前工作零件中的实体。");
    Bend b;b.selection=selected;b.body=body->Tag();int units=0;Check(UF_PART_ask_units(UF_ASSEM_ask_work_part(),&units));b.unitsPerMm=units==UF_PART_ENGLISH?1.0/25.4:1;
    Check(UF_MODL_ask_distance_tolerance_of_part(UF_ASSEM_ask_work_part(),&b.tolerance));
    std::vector<Vec> normals;Vec center;double lo=std::numeric_limits<double>::max(),hi=-lo;
    if(face){
        int type=0,sign=0;double p[3],d[3],box[6],r2=0;Check(UF_MODL_ask_face_data(selected,&type,p,d,box,&b.radius,&r2,&sign));
        if(type!=16||b.radius<=0)throw std::runtime_error("请选择内侧圆柱折弯面；平面、圆孔和外圆角不适用。");
        b.axis=Canonical(V(d));center=V(p);
        double uvbox[4],uv[2],on[3],u1[3],v1[3],u2[3],v2[3],normal[3],radii[2];Check(UF_MODL_ask_face_uv_minmax(selected,uvbox));
        uv[0]=(uvbox[0]+uvbox[1])/2;uv[1]=(uvbox[2]+uvbox[3])/2;Check(UF_MODL_ask_face_props(selected,uv,on,u1,v1,u2,v2,normal,radii));
        Vec radial=V(on)-center;radial=radial-b.axis*Dot(radial,b.axis);
        if(Dot(Unit(radial),Unit(V(normal)))>-.99)throw std::runtime_error("当前是外侧圆柱面，请选择折弯内侧面。");
        for(auto* e:face->GetEdges()){
            if(e->SolidEdgeType()!=NXOpen::Edge::EdgeTypeLinear)continue;
            NXOpen::Point3d a,c;e->GetVertices(&a,&c);Vec delta=V(c)-V(a);
            if(Dot(delta,delta)<=b.tolerance*b.tolerance||std::abs(Dot(Unit(delta),b.axis))<.999999)continue;
            bool tangent=false;
            for(auto* f:e->GetFaces())if(f!=face&&f->SolidFaceType()==NXOpen::Face::FaceTypePlanar){
                Vec mid=(V(a)+V(c))*.5,n=Normal(f,mid),rd=mid-center;rd=rd-b.axis*Dot(rd,b.axis);
                if(std::abs(Dot(n,b.axis))<1e-6&&Dot(n,Unit(rd))<-.999){AddNormal(normals,n);tangent=true;}
            }
            if(tangent){lo=std::min({lo,Dot(V(a)-center,b.axis),Dot(V(c)-center,b.axis)});hi=std::max({hi,Dot(V(a)-center,b.axis),Dot(V(c)-center,b.axis)});}
        }
        if(normals.size()!=2||Dot(normals[0],normals[1])>.9999||Dot(normals[0],normals[1])<-.9999)throw std::runtime_error("未找到两侧相切板面；圆孔、压死边、拆分或复杂折弯暂不支持。");
        b.up=Unit(normals[0]+normals[1]);b.length=hi-lo;b.tip=center+b.axis*((lo+hi)/2)-b.up*b.radius;
    }else{
        if(edge->SolidEdgeType()!=NXOpen::Edge::EdgeTypeLinear)throw std::runtime_error("锐边模式只支持直线内边。");
        if(!std::isfinite(sharpInnerRadiusMm)||sharpInnerRadiusMm<0||sharpInnerRadiusMm>1000)throw std::runtime_error("锐边定位内 R 必须在 0～1000 mm 范围内。");
        NXOpen::Point3d a,c;edge->GetVertices(&a,&c);Vec delta=V(c)-V(a);b.length=std::sqrt(Dot(delta,delta));b.axis=Canonical(delta);center=(V(a)+V(c))*.5;
        for(auto* f:edge->GetFaces())if(f->SolidFaceType()==NXOpen::Face::FaceTypePlanar)AddNormal(normals,Normal(f,center));
        if(normals.size()!=2||std::abs(Dot(normals[0],normals[1]))>.9999)throw std::runtime_error("锐边必须连接两个非平行平面。");
        b.up=Unit(normals[0]+normals[1]);double eps=std::max(b.tolerance*10,b.unitsPerMm*.1);
        if(Contains(b.body,center+(normals[0]-normals[1])*eps)!=1||Contains(b.body,center+(normals[1]-normals[0])*eps)!=1)throw std::runtime_error("所选为外侧凸边或局部尺寸过小，请选择内折弯锐边。");
        b.radius=sharpInnerRadiusMm*b.unitsPerMm;b.sharp=true;double cosine=std::sqrt((1+Dot(normals[0],normals[1]))/2);
        b.tip=center+b.up*(b.radius*(1/cosine-1));
    }
    if(b.length<=2*b.tolerance)throw std::runtime_error("折弯长度太小。");
    b.x=Unit(Cross(b.axis,b.up));return b;
}
tag_t CreateToolBody(const Placement& p){
    std::vector<NXOpen::Curve*> curves;
    auto at=[&](Point q){return p.origin+p.x*(q.x*p.bend.unitsPerMm)+p.up*(q.z*p.bend.unitsPerMm)-p.axis*(p.length/2);};
    for(size_t i=0;i<p.tool.profile.size();++i){Vec a=at(p.tool.profile[i]),b=at(p.tool.profile[(i+1)%p.tool.profile.size()]);UF_CURVE_line_t line={{a.x,a.y,a.z},{b.x,b.y,b.z}};tag_t t=0;Check(UF_CURVE_create_line(&line,&t));curves.push_back(dynamic_cast<NXOpen::Curve*>(NXOpen::NXObjectManager::Get(t)));}
    auto* work=NXOpen::Session::GetSession()->Parts()->Work();
    auto* builder=work->Features()->CreateExtrudeBuilder(nullptr);tag_t b=0;
    try{
        // Set tolerances on this temporary feature, never on the user's part.
        // The explicit section avoids legacy UF chaining failures on short DWG
        // segments in rotated inch parts. Physical tolerance is 0.0001 mm.
        double tolerance=.0001*p.bend.unitsPerMm;
        builder->SetDistanceTolerance(tolerance);builder->SetChainingTolerance(tolerance);
        auto* section=work->Sections()->CreateSection(tolerance,tolerance,.5);
        builder->SetSection(section);section->AllowSelfIntersection(false);
        section->SetAllowedEntityTypes(NXOpen::Section::AllowTypesOnlyCurves);
        auto* rule=work->ScRuleFactory()->CreateRuleCurveDumb(curves);
        section->AddToSection({rule},curves.front(),nullptr,nullptr,{p.origin.x,p.origin.y,p.origin.z},NXOpen::Section::ModeCreate);
        builder->SetDirection(work->Directions()->CreateDirection({0,0,0},{p.axis.x,p.axis.y,p.axis.z},NXOpen::SmartObject::UpdateOptionWithinModeling));
        builder->BooleanOperation()->SetType(NXOpen::GeometricUtilities::BooleanOperation::BooleanTypeCreate);
        builder->Limits()->StartExtend()->Value()->SetRightHandSide("0");
        builder->Limits()->EndExtend()->Value()->SetRightHandSide(Number(p.length).c_str());
        auto* feature=builder->CommitFeature();Check(UF_MODL_ask_feat_body(feature->Tag(),&b));
    }catch(...){builder->Destroy();throw;}
    builder->Destroy();
    auto* body=dynamic_cast<NXOpen::Body*>(NXOpen::NXObjectManager::Get(b));if(!body||!body->IsSolidBody())throw std::runtime_error("刀具建模失败，未生成有效实体。");return b;
}
Result CheckBodies(tag_t body,tag_t tool,double units,double safeGap,double tolerance){
    if(body==tool||!body||!tool||units<=0||!std::isfinite(safeGap)||safeGap<0)throw std::runtime_error("干涉检测输入无效。");
    int status=0;int rc=UF_MODL_check_interference(body,1,&tool,&status);int update=UF_MODL_update();Check(rc);Check(update);
    Result r;r.toleranceMm=tolerance/units;
    if(status==1){r.status=Status::Interference;return r;}
    if(status!=2&&status!=3)throw std::runtime_error("NX 未返回明确的干涉结果。");
    double guess[3]={},a[3],b[3],distance=0,accuracy=0;Check(UF_MODL_ask_minimum_dist_3(2,body,tool,0,guess,0,guess,&distance,a,b,&accuracy));
    if(!std::isfinite(distance)||distance<0)throw std::runtime_error("最小距离计算失败。");
    r.distanceMm=distance/units;r.onPart=V(a);r.onTool=V(b);
    r.status=status==3||distance<=tolerance?Status::Contact:r.distanceMm<safeGap?Status::Near:Status::Clear;return r;
}
Result InspectPlacement(const Placement& p,double safeGap){
    auto* session=NXOpen::Session::GetSession();auto mark=session->SetUndoMark(NXOpen::Session::MarkVisibilityInvisible,"折弯模拟临时刀具");
    Result r;std::exception_ptr failure;
    try{
        tag_t tool=CreateToolBody(p);r=CheckBodies(p.bend.body,tool,p.bend.unitsPerMm,safeGap,p.bend.tolerance);
        if(r.status==Status::Interference)CollectInterference(p.bend.body,tool,p.bend.unitsPerMm,r);
    }catch(...){failure=std::current_exception();}
    // Never return a valid result when cleanup failed. No tags of the tool escape.
    session->UndoToMark(mark,nullptr);session->DeleteUndoMark(mark,nullptr);
    if(failure)std::rethrow_exception(failure);return r;
}
}
