#include "../TubeGeometry.hpp"
#include <NXOpen/Body.hxx>
#include <NXOpen/BodyCollection.hxx>
#include <NXOpen/Edge.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/Features_FeatureCollection.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/Session.hxx>
#include <uf.h>
#include <uf_curve.h>
#include <uf_eval.h>
#include <uf_modl.h>
#include <uf_part.h>
#include <filesystem>
#include <iostream>
#include <algorithm>
using namespace tube_straighten;
using namespace NXOpen;
void Require(bool b,const char* m){if(!b)throw std::runtime_error(m);}
int Contains(tag_t body,Vec q){double xyz[]={q.x,q.y,q.z};int status=0;Check(UF_MODL_ask_point_containment(xyz,body,&status));return status;}
int Solids(Part* p){int n=0;for(auto* b:*p->Bodies())if(b->IsSolidBody())++n;return n;}
double SectionArea(const Source& s){auto area=[](double w,double d,double r){return w*d-(4-pi)*r*r;};return area(s.width,s.depth,s.cornerRadius)-area(s.width-2*s.thickness,s.depth-2*s.thickness,std::max(0.,s.cornerRadius-s.thickness));}
Source Recognize(Part* part){for(auto* b:*part->Bodies())if(b->IsSolidBody()&&!b->IsBlanked())for(auto* f:b->GetFaces()){try{return InspectFace(f->Tag());}catch(const std::exception&){}}throw std::runtime_error("No source recognized");}
#include "MachineFixture.hpp"
void ArcHoleRejection(const char* input){
    tag_t tag=0;UF_PART_load_status_t load={};Check(UF_PART_open(input,&tag,&load));UF_PART_free_load_status(&load);auto* session=Session::GetSession();auto* part=session->Parts()->Work();auto s=Recognize(part);double before=Volume(s.body);int solids=Solids(part);auto count=part->Features()->GetFeatures().size();Settings opt;opt.segmentArcs=false;bool rejected=false;
    try{MakePlan(s,opt);}catch(const std::exception& e){rejected=std::string(e.what()).find("弯曲区或跨弯孔槽")!=std::string::npos;}
    Require(rejected&&std::abs(before-Volume(s.body))<before*1e-10&&solids==Solids(part)&&count==part->Features()->GetFeatures().size(),"Deforming hole not rejected without mutation");
    auto mark=session->SetUndoMark(Session::MarkVisibilityInvisible,"user original mode");Create(MakePlan(s,Settings{}));session->UndoToMark(mark,nullptr);session->DeleteUndoMark(mark,nullptr);Require(std::abs(before-Volume(s.body))<before*1e-10&&solids==Solids(part),"User original mode/undo changed");Check(UF_PART_close(tag,0,1));
    std::cout<<"PASS user curved-wall holes: explicit whole-mode rejection before mutation; original segmented mode and undo remain usable"<<std::endl;
}
void Case(const std::filesystem::path& input,const std::filesystem::path& output){
    std::cout<<"BEGIN "<<output.string()<<std::endl;tag_t partTag=0;UF_PART_load_status_t load={};Check(UF_PART_open(input.string().c_str(),&partTag,&load));UF_PART_free_load_status(&load);
    auto* session=Session::GetSession();auto* part=session->Parts()->Work();auto s=Recognize(part);double u=s.unitsPerMm,sourceVolume=Volume(s.body);int initialSolids=Solids(part);auto initialFeatures=part->Features()->GetFeatures().size();
    Settings opt;opt.segmentArcs=false;auto p=MakePlan(s,opt);double neutral=0,totalArcAngle=0;int corners=0;
    for(size_t i=0;i<s.spans.size();++i){const auto& span=s.spans[i];neutral+=span.radius?(span.radius-s.depth/2)*span.angle:Length(span.b-span.a);if(span.radius)totalArcAngle+=span.angle;
        if(i){double angle=std::atan2(Dot(Cross(s.spans[i-1].Tangent(1),span.Tangent(0)),s.normal),Dot(s.spans[i-1].Tangent(1),span.Tangent(0)));if(angle>1e-7){++corners;neutral-=2*(opt.radiusMm*u+s.thickness)*std::tan(angle/2);neutral+=(opt.radiusMm*u+opt.kFactor*s.thickness)*angle;}}
    }
    Require(std::abs(p.length-neutral)<1e-7*u&&p.bends.size()==corners,"Exact neutral length or true corner count wrong");
    auto otherK=opt;otherK.tubeKFactor=.42;auto kp=MakePlan(s,otherK);Require(std::abs(p.length-kp.length-.08*s.depth*totalArcAngle)<1e-7*u,"Tube K length sensitivity wrong");
    Require(!Preview(p).empty(),"No whole-tube preview");
    auto mark=session->SetUndoMark(Session::MarkVisibilityVisible,"whole bending test");tag_t flat=Create(p);Require(flat!=s.body&&Solids(part)==initialSolids+1,"Whole tube body count");Require(std::abs(Volume(s.body)-sourceVolume)<sourceVolume*1e-10,"Whole mode altered source");
    if(!s.round||p.bends.empty()){
        double volume=SectionArea(s)*neutral,t=s.thickness;
        for(const auto& b:p.bends){auto integral=[&](double a,double z){double root=t+opt.radiusMm*u;return (b.allowance+p.gap)*(z-a)+std::tan(b.angle/2)*(std::pow(std::max(0.,z-root),2)-std::pow(std::max(0.,a-root),2));};volume-=2*t*integral(t,s.depth)+(s.width-2*t)*integral(s.depth-t,s.depth);}
        for(const auto& h:s.holes)volume-=h.area*h.length;
        Require(std::abs(Volume(flat)*1e9*std::pow(u,3)-volume)<volume*1e-6,"Independent stock volume wrong");
    }
    // Every former smooth bend has uninterrupted full wall material, including
    // the opposite wall where segmented mode would have removed a V notch.
    for(const auto& arc:p.machineArcs)for(double f:{.03,.25,.5,.75,.97}){
        double x=arc.start+arc.length*f,t=s.thickness,d=s.depth,w=s.width;
        if(s.round){double r=d/2;for(int j=0;j<16;++j){double a=2*pi*j/16;Require(Contains(flat,FlatPoint(p,x,r-(r-t/2)*cos(a),r+(r-t/2)*sin(a)))==1,"Whole arc has cut wall");}}
        else for(auto yz:std::vector<Vec>{{0,t/2,w/2},{0,d-t/2,w/2},{0,d/2,t/2},{0,d/2,w-t/2}})Require(Contains(flat,FlatPoint(p,x,yz.y,yz.z))==1,"Whole arc has cut wall");
        Require(Contains(flat,FlatPoint(p,x,d/2,w/2))==2,"Whole tube bore filled");
    }
    Vec cap=FlatPoint(p,0,s.thickness/2,s.width/2);Require(Contains(s.body,cap)==3&&Contains(flat,cap)==3,"Start cap not coincident");
    auto* flatBody=dynamic_cast<Body*>(NXObjectManager::Get(flat));
    for(size_t i=0;i<s.holes.size();++i){auto hole=FlatHole(p,i);for(size_t j=0;j<hole.profile.size();++j){const auto& c=hole.profile[j];for(double f:{0.,.125,.25,.5,.75}){
        auto local=c.Point(f);Require(Length(ToFolded(p,p.holeSegments[i],local)-s.holes[i].profile[j].Point(f))<1e-7*u,"Straight hole return position changed");Require(Contains(flat,FlatPoint(p,local.x,local.y,local.z))==3,"Flat hole boundary changed");Require(Contains(s.body,s.holes[i].profile[j].Point(f))==3,"Source hole changed");}
        if(c.radius&&c.angle>2*pi-1e-6){double best=1e100;Vec center=FlatPoint(p,c.center.x,c.center.y,c.center.z);for(auto* edge:flatBody->GetEdges()){UF_EVAL_p_t e=nullptr;Check(UF_EVAL_initialize_2(edge->Tag(),&e));logical isArc=false;Check(UF_EVAL_is_arc(e,&isArc));if(isArc){UF_EVAL_arc_t a;Check(UF_EVAL_ask_arc(e,&a));best=std::min(best,Length(center-Vec{a.center[0],a.center[1],a.center[2]})+std::abs(c.radius-a.radius));}UF_EVAL_free(e);}Require(best<1e-5*u,"Flat hole center/radius mismatch");}
    }}
    session->UndoToMark(mark,nullptr);session->DeleteUndoMark(mark,nullptr);Require(initialFeatures==part->Features()->GetFeatures().size()&&initialSolids==Solids(part),"Whole operation undo leaked geometry");
    auto cut=opt;cut.cutSource=true;cut.hideSource=true;if(!corners){cut.gapMm=0;cut.radiusMm=10000;cut.kFactor=0;cut.divisions=0;cut.bridgeWidthMm=0;}
    auto paired=MakePlan(s,cut);Require(paired.sourceSlots.size()==corners,"Source smooth arcs received slots");for(const auto& slot:paired.sourceSlots)Require(!slot.pathRadius,"Smooth source arc was slotted");
    mark=session->SetUndoMark(Session::MarkVisibilityInvisible,"source switch in whole mode");Create(paired);Require(corners?Volume(s.body)<sourceVolume:std::abs(Volume(s.body)-sourceVolume)<sourceVolume*1e-10,"Source slot switch affected smooth arc");session->UndoToMark(mark,nullptr);session->DeleteUndoMark(mark,nullptr);
    // Old enabled mode remains independently usable on the same source.
    mark=session->SetUndoMark(Session::MarkVisibilityInvisible,"segmented regression");auto old=MakePlan(s,Settings{});Require(old.machineArcs.empty()&&old.bends.size()>=p.bends.size(),"Enabled mode changed");Create(old);session->UndoToMark(mark,nullptr);session->DeleteUndoMark(mark,nullptr);
    Create(p);Check(UF_PART_save_as(output.string().c_str()));Check(UF_PART_close(partTag,0,1));load={};Check(UF_PART_open(output.string().c_str(),&partTag,&load));UF_PART_free_load_status(&load);Require(Solids(session->Parts()->Work())==initialSolids+1,"Saved whole tube missing");Check(UF_PART_close(partTag,0,1));
    std::cout<<"PASS length="<<p.length/u<<" mm, arcs="<<p.machineArcs.size()<<", corners="<<corners<<", holes="<<s.holes.size()<<": K, wall, source, end cap, holes, undo, segmented regression, reopen"<<std::endl;
}
int main(int argc,char** argv){try{
    Require(argc==5||argc==6,"Square fixtures, round fixtures, user copy, new output folder, optional extra required");std::filesystem::path square=argv[1],round=argv[2],out=argv[4];Require(!std::filesystem::exists(out),"Output folder exists");std::filesystem::create_directories(out);Check(UF_initialize());
    if(argc==5){for(auto name:{"arc","arc-inch-rotated","half-arc","corner","corner-inch-rotated","corner-slots"})Case(square/(std::string(name)+".prt-source.prt"),out/(std::string(name)+".prt"));
        for(auto name:{"round-arc","round-arc-inch-rotated","round-half-arc","round-corner"})Case(round/(std::string(name)+".prt-source.prt"),out/(std::string(name)+".prt"));}
    if(argc==5||std::string(argv[5])!="user"){MachineFixture(out/"hybrid-source.prt");Case(out/"hybrid-source.prt",out/"hybrid-whole.prt");}ArcHoleRejection(argv[3]);UF_terminate();return 0;
}catch(const NXException& e){std::cerr<<e.Message()<<std::endl;return 1;}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}}
