#include "../TubeGeometry.hpp"
#include <NXOpen/Body.hxx>
#include <NXOpen/BodyCollection.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/Edge.hxx>
#include <NXOpen/Features_FeatureCollection.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/Session.hxx>
#include <uf.h>
#include <uf_eval.h>
#include <uf_modl.h>
#include <uf_part.h>
#include <filesystem>
#include <iostream>
using namespace tube_straighten;
using namespace NXOpen;
void Require(bool b,const char* m){if(!b)throw std::runtime_error(m);}
int Contains(tag_t body,Vec p){double q[]={p.x,p.y,p.z};int status=0;Check(UF_MODL_ask_point_containment(q,body,&status));return status;}
int Solids(Part* part){int n=0;for(auto* b:*part->Bodies())if(b->IsSolidBody())++n;return n;}
double RemovedVolume(const Plan& p){
    double sum=0,t=p.source.thickness,w=p.source.width,d=p.source.depth,r=d/2,ri=r-t,h=p.gap/2,alpha=BridgeHalfAngle(p);
    for(const auto& s:p.sourceSlots){
        if(!s.pathRadius){sum+=p.gap*(p.source.round?(pi-alpha)*(r*r-ri*ri):t*(2*d+w-4*t))/s.cornerCos;continue;}
        constexpr int nx=24;double dx=p.gap/nx;
        for(int k=0;k<nx;++k){double x=-h+(k+.5)*dx;
            if(!p.source.round){auto y=[&](double depth){return sqrt((s.pathRadius-depth)*(s.pathRadius-depth)-x*x);};sum+=dx*(2*t*(y(t)-y(d))+(w-2*t)*(y(d-t)-y(d)));}
            else{constexpr int nr=24,na=600;double dr=t/nr,da=(2*pi-2*alpha)/na;
                for(int j=0;j<nr;++j)for(int i=0;i<na;++i){double rho=ri+(j+.5)*dr,beta=alpha+(i+.5)*da,radial=s.pathRadius-r+rho*cos(beta);sum+=dx*dr*da*rho*radial/sqrt(radial*radial-x*x);}
            }
        }
    }return sum;
}
void Case(const std::filesystem::path& input,const std::filesystem::path& output,bool rounded=false){
    std::cout<<"BEGIN "<<output.string()<<std::endl;tag_t partTag=0;UF_PART_load_status_t load={};Check(UF_PART_open(input.string().c_str(),&partTag,&load));UF_PART_free_load_status(&load);
    auto* session=Session::GetSession();auto* part=session->Parts()->Work();Source s;
    int initialSolids=Solids(part);std::vector<std::pair<tag_t,double>> otherBodies;
    for(auto* body:*part->Bodies())if(!body->IsBlanked()&&body->IsSolidBody())for(auto* face:body->GetFaces()){try{s=InspectFace(face->Tag());break;}catch(const std::exception&){}if(s.body)break;}
    for(auto* body:*part->Bodies())if(body->IsSolidBody()&&body->Tag()!=s.body)otherBodies.push_back({body->Tag(),Volume(body->Tag())});
    Require(s.body!=0,"No source recognized");Settings settings;settings.cutSource=true;settings.hideSource=true;settings.bridgeWidthMm=s.unitsPerMm<1?12:6;
    auto p=MakePlan(s,settings);Require(p.sourceSlots.size()==p.bends.size(),"Slot count does not match flat cuts");Require(!Preview(p).empty(),"No dual preview");
    auto bad=settings;bad.gapMm=0;bool rejected=false;try{MakePlan(s,bad);}catch(...){rejected=true;}Require(rejected,"Zero source gap accepted");
    double before=Volume(s.body);auto featureCount=part->Features()->GetFeatures().size();auto mark=session->SetUndoMark(Session::MarkVisibilityVisible,"source slots test");
    tag_t flat=Create(p);Require(flat!=s.body&&Solids(part)==initialSolids+1,"Expected exactly one added flat solid");for(auto other:otherBodies)Require(std::abs(Volume(other.first)-other.second)<other.second*1e-10,"Unselected body altered");auto* original=dynamic_cast<Body*>(NXObjectManager::Get(s.body));Require(!original->IsBlanked(),"Slotted source hidden");
    // Removed material is a small difference of two large integrated bodies.
    // Allow 0.1 ppm of stock volume for that subtraction; gap-edge containment
    // below independently tests both physical slot planes at +/- 1% of width.
    double removed=(before-Volume(s.body))*1e9*std::pow(s.unitsPerMm,3);Require(removed>0,"Source not slotted");if(!rounded){double expected=RemovedVolume(p);std::cout<<"REMOVAL "<<removed<<" expected "<<expected<<std::endl;Require(std::abs(removed-expected)<std::max(expected*2e-5,before*1e9*std::pow(s.unitsPerMm,3)*1e-7),"Source gap volume differs from independent integration");}
    // Verify the physical gap width using probes immediately inside/outside
    // its two parallel planes, and unchanged material on the retained side.
    double t=s.thickness,d=s.depth,w=s.width;
    for(const auto& slot:p.sourceSlots)for(double fraction:{-.51,-.49,0.,.49,.51})for(double depth:{t/2,d-t/2}){
        double x=p.gap*fraction,y;
        if(slot.pathRadius)y=slot.pathRadius-sqrt((slot.pathRadius-depth)*(slot.pathRadius-depth)-x*x);
        else y=(depth+std::abs(x)*sqrt(1-slot.cornerCos*slot.cornerCos))/slot.cornerCos;
        Vec q=slot.origin+slot.axis*x+Cross(s.normal,slot.axis)*y+s.widthDirection*(w/2);
        Require(Contains(s.body,q)==(depth<t||std::abs(fraction)>.5?1:2),"Measured source slot width/bridge incorrect");
    }
    // End sections stay in place and retain their original material.
    for(const auto& span:{s.spans.front(),s.spans.back()})for(double f:{0.,1.})if(Length(span.Point(f)-s.spans.front().a)<1e-7||Length(span.Point(f)-s.spans.back().b)<1e-7){Vec q=span.Point(f)+Cross(s.normal,span.Tangent(f))*(t/2)+s.widthDirection*(w/2);Require(Contains(s.body,q)==3,"Original end cap moved/trimmed");}
    for(const auto& hole:s.holes)for(const auto& c:hole.profile){
        for(double f:{0.,.125,.25,.375,.5,.625,.75,.875})Require(Contains(s.body,c.Point(f))==3,"Source hole boundary altered");
        if(c.radius&&c.angle>2*pi-1e-6){double best=1e100;for(auto* edge:original->GetEdges()){UF_EVAL_p_t eval=nullptr;Check(UF_EVAL_initialize_2(edge->Tag(),&eval));logical arc=false;Check(UF_EVAL_is_arc(eval,&arc));if(arc){UF_EVAL_arc_t a;Check(UF_EVAL_ask_arc(eval,&a));best=std::min(best,Length(Vec{a.center[0],a.center[1],a.center[2]}-c.center)+std::abs(a.radius-c.radius));}UF_EVAL_free(eval);}Require(best<1e-5*s.unitsPerMm,"Original hole center/radius changed");}
    }
    session->UndoToMark(mark,nullptr);session->DeleteUndoMark(mark,nullptr);Require(part->Features()->GetFeatures().size()==featureCount&&Solids(part)==initialSolids&&std::abs(Volume(s.body)-before)<before*1e-10,"Combined operation undo incomplete");
    // Mode off must continue to leave the original unmodified.
    auto baselineMark=session->SetUndoMark(Session::MarkVisibilityInvisible,"mode off");settings.cutSource=false;settings.hideSource=false;Create(MakePlan(s,settings));Require(std::abs(Volume(s.body)-before)<before*1e-10,"Mode off altered source");session->UndoToMark(baselineMark,nullptr);session->DeleteUndoMark(baselineMark,nullptr);
    if(rounded){auto broken=p;broken.sourceSlots.back().origin=broken.sourceSlots.back().origin+s.normal*(100*s.unitsPerMm);auto failureMark=session->SetUndoMark(Session::MarkVisibilityVisible,"failed combined operation");bool failed=false;try{Create(broken);}catch(...){failed=true;}session->UndoToMark(failureMark,nullptr);session->DeleteUndoMark(failureMark,nullptr);Require(failed&&Solids(part)==initialSolids&&part->Features()->GetFeatures().size()==featureCount&&std::abs(Volume(s.body)-before)<before*1e-10,"Failure rollback left a partial result");}
    Create(p);Check(UF_PART_save_as(output.string().c_str()));Check(UF_PART_close(partTag,0,1));load={};Check(UF_PART_open(output.string().c_str(),&partTag,&load));UF_PART_free_load_status(&load);Require(Solids(session->Parts()->Work())==initialSolids+1,"Saved result body count incorrect");Check(UF_PART_close(partTag,0,1));
    std::cout<<"PASS "<<output.string()<<": paired slots, true source geometry, full wall bridge, gap width, holes/endpoints unchanged, undo, mode off, save/reopen"<<std::endl;
}
int main(int argc,char** argv){try{
    Require(argc==5||argc==6,"Rectangle fixtures, round fixtures, user model copy, NEW output folder, optional case name required");std::filesystem::path square=argv[1],round=argv[2],out=argv[4];Require(!std::filesystem::exists(out),"Output folder exists");std::filesystem::create_directories(out);Check(UF_initialize());
    auto selected=[&](const char* name){return argc==5||std::string(argv[5])==name;};
    for(auto name:{"arc","arc-inch-rotated","half-arc","corner","corner-inch-rotated","corner-slots"})if(selected(name))Case(square/(std::string(name)+".prt-source.prt"),out/(std::string(name)+".prt"));
    for(auto name:{"round-arc","round-arc-inch-rotated","round-half-arc","round-corner"})if(selected(name))Case(round/(std::string(name)+".prt-source.prt"),out/(std::string(name)+".prt"));
    if(selected("model2"))Case(argv[3],out/"model2-slotted-and-flat.prt",true);UF_terminate();return 0;
}catch(const NXException& e){std::cerr<<e.Message()<<std::endl;return 1;}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}}
