#include "../TubeGeometry.hpp"
#include <NXOpen/Body.hxx>
#include <NXOpen/BodyCollection.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/Features_FeatureCollection.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/Session.hxx>
#include <uf.h>
#include <uf_modl.h>
#include <uf_obj.h>
#include <uf_part.h>
#include <filesystem>
#include <algorithm>
#include <iostream>
using namespace tube_straighten;
using namespace NXOpen;
void Require(bool b,const char* m){if(!b)throw std::runtime_error(m);}
int Contains(tag_t body,Vec p){double q[]={p.x,p.y,p.z};int where=0;Check(UF_MODL_ask_point_containment(q,body,&where));return where;}
int Solids(Part* p){int n=0;for(auto* b:*p->Bodies())if(b->IsSolidBody())++n;return n;}
void Trim(tag_t body,Vec center,Vec normal,Vec keep){
    double q[]={center.x,center.y,center.z},n[]={normal.x,normal.y,normal.z};tag_t plane=0,feature=0;Check(UF_MODL_create_plane(q,n,&plane));Check(UF_OBJ_set_blank_status(plane,UF_OBJ_BLANKED));
    auto* session=Session::GetSession();auto mark=session->SetUndoMark(Session::MarkVisibilityInvisible,"fixture plane direction");Check(UF_MODL_trim_body(body,plane,0,&feature));
    if(Contains(body,keep)!=1){session->UndoToMark(mark,nullptr);Check(UF_MODL_trim_body(body,plane,1,&feature));}session->DeleteUndoMark(mark,nullptr);Require(Contains(body,keep)==1,"Fixture trim side");
}
double CutVolume(const Plan& p){
    double r=p.source.depth/2,ri=r-p.source.thickness,alpha=BridgeHalfAngle(p),root=p.source.thickness+p.radius,moment=0;
    const int nr=30,na=1600;double dr=(r-ri)/nr,da=(2*pi-2*alpha)/na;
    for(int j=0;j<nr;++j)for(int i=0;i<na;++i){double rho=ri+(j+.5)*dr,a=alpha+(i+.5)*da;moment+=std::max(0.,r-rho*cos(a)-root)*rho*dr*da;}
    double volume=pi*(r*r-ri*ri)*p.length;for(const auto& b:p.bends)volume-=(b.allowance+p.gap)*(pi-alpha)*(r*r-ri*ri)+2*tan(b.angle/2)*moment;return volume;
}
void CheckEnds(const Plan& p,tag_t flat){
    double r=p.source.depth/2,u=p.source.unitsPerMm;Vec flatAxis=p.source.spans.front().Tangent(0),flatInside=Cross(p.source.normal,flatAxis);
    for(bool end:{false,true}){const auto& span=end?p.source.spans.back():p.source.spans.front();Vec axis=span.Tangent(end?1:0),inside=Cross(p.source.normal,axis),center=(end?span.b:span.a)+inside*r+p.source.widthDirection*r;
        Vec normal=end?p.source.endCutNormal:p.source.startCutNormal;if(Length(normal)<.1)normal=axis;
        for(double rho:{r,r-p.source.thickness/2,r-p.source.thickness})for(int j=0;j<48;++j){double angle=2*pi*(j+.21)/48;Vec radial=inside*(rho*cos(angle))+p.source.widthDirection*(rho*sin(angle));double delta=-Dot(normal,radial)/Dot(normal,axis);
            Vec original=center+radial+axis*delta,flatPoint=FlatPoint(p,(end?p.length:0)+delta,r+rho*cos(angle),r+rho*sin(angle));
            Require(Contains(p.source.body,original)==3,"Original cap equation mismatch");Require(Contains(flat,flatPoint)==3,"Oblique cap boundary changed");
            if(rho<r&&rho>r-p.source.thickness){Require(Contains(flat,flatPoint+flatAxis*((end?-1:1)*.003*u))==1,"End wall missing");Require(Contains(flat,flatPoint+flatAxis*((end?1:-1)*.003*u))==2,"End excess stock");}
            if(!end)Require(Length(flatPoint-original)<1e-7*u,"Selected start cap moved");
        }
    }
}
void Case(const std::string& input,const std::string& output,bool makeEnds,bool user){
    std::cout<<"BEGIN "<<output<<std::endl;tag_t partTag=0;UF_PART_load_status_t load={};Check(UF_PART_open(input.c_str(),&partTag,&load));UF_PART_free_load_status(&load);auto* session=Session::GetSession();auto* part=session->Parts()->Work();Body* body=nullptr;
    for(auto* b:*part->Bodies())if(b->IsSolidBody()&&!b->IsBlanked()){Require(!body,"Multiple source solids");body=b;}Require(body,"No source body");
    if(makeEnds){auto s=InspectFace(body->GetFaces().front()->Tag());double r=s.depth/2,u=s.unitsPerMm;
        for(bool end:{false,true}){const auto& span=end?s.spans.back():s.spans.front();Vec axis=span.Tangent(end?1:0),inside=Cross(s.normal,axis),center=(end?span.b:span.a)+inside*r+s.widthDirection*r;
            Vec n=axis+inside*(end?-.7:.5)+s.widthDirection*(end?.25:.4);center=center+axis*((end?-1:1)*40*u);
            Trim(body->Tag(),center,n,center+axis*((end?-1:1)*25*u)+inside*(r-s.thickness/2));
        }
    }
    Check(UF_PART_save_as((output+"-source.prt").c_str()));std::vector<tag_t> caps;for(auto* f:body->GetFaces()){auto s=InspectFace(f->Tag());Require(s.round,"Any-face recognition failed");if(IsRoundCap(f->Tag()))caps.push_back(f->Tag());}Require(caps.size()==2,"Both end caps selectable");
    const auto features=part->Features()->GetFeatures().size();double before=Volume(body->Tag());int solids=Solids(part);double lengths[2]={};
    for(int direction=0;direction<2;++direction){auto source=InspectFace(caps[direction]);double u=source.unitsPerMm;Require(Length(source.startCutNormal)+Length(source.endCutNormal)>.1,"Oblique end omitted");
        if(user){Require(std::abs(source.depth-18)<1e-7&&std::abs(source.thickness-1)<1e-7,"User pipe dimensions");for(const auto& span:source.spans)if(span.radius)Require(std::abs(span.radius-29)<1e-7&&std::abs(span.angle-pi/4)<1e-7,"Imported torus center/radius");}
        for(int mode=0;mode<3;++mode){Settings settings;settings.divisions=15;settings.segmentArcs=mode!=0;settings.cutSource=mode==2;auto plan=MakePlan(source,settings);Require(!Preview(plan).empty(),"Oblique preview missing");
            if(mode==0){lengths[direction]=plan.length;if(user)Require(std::abs(plan.length-(68*sqrt(2.)-20*tan(pi/8)+125.14213562371-76.28427124746+5*pi))<1e-6,"Independent user neutral length");}
            auto mark=session->SetUndoMark(Session::MarkVisibilityVisible,"oblique regression");tag_t flat=Create(plan);CheckEnds(plan,flat);Require(Solids(part)==solids+1,"Extra or missing solid");
            double expected=CutVolume(plan),actual=Volume(flat)*1e9*pow(u,3);Require(std::abs(expected-actual)<expected*3e-5,"Oblique stock/notch volume");
            Require(mode==2?Volume(source.body)<before:std::abs(Volume(source.body)-before)<before*1e-9,"Source switch wrong");
            session->UndoToMark(mark,nullptr);session->DeleteUndoMark(mark,nullptr);Require(Solids(part)==solids&&part->Features()->GetFeatures().size()==features&&std::abs(Volume(source.body)-before)<before*1e-9,"Undo leaked geometry");
            std::cout<<"PASS direction="<<direction<<" mode="<<mode<<" length="<<plan.length/u<<": exact end planes, 288 cap probes, full wall, bore, volume, source/undo"<<std::endl;
        }
        auto invalid=source;Vec axis=source.spans.front().Tangent(0);invalid.startCutNormal=tube_straighten::Unit(axis*.001+Cross(source.normal,axis));bool rejected=false;try{MakePlan(invalid,Settings{});}catch(...){rejected=true;}Require(rejected,"End overlapping cuts accepted");
    }
    Require(std::abs(lengths[0]-lengths[1])<1e-7,"Reversing selected end changed length");Settings whole;whole.segmentArcs=false;Create(MakePlan(InspectFace(caps[0]),whole));Check(UF_PART_save_as((output+"-whole.prt").c_str()));Check(UF_PART_close(partTag,0,1));
    load={};Check(UF_PART_open((output+"-source.prt").c_str(),&partTag,&load));UF_PART_free_load_status(&load);part=session->Parts()->Work();for(auto* b:*part->Bodies())if(b->IsSolidBody()&&!b->IsBlanked()){Settings segmented;segmented.divisions=15;Create(MakePlan(InspectFace(b->GetFaces().front()->Tag()),segmented));break;}Check(UF_PART_save_as((output+"-segmented.prt").c_str()));Check(UF_PART_close(partTag,0,1));
    load={};Check(UF_PART_open((output+"-whole.prt").c_str(),&partTag,&load));UF_PART_free_load_status(&load);Require(Solids(session->Parts()->Work())==solids+1,"Saved output missing");Check(UF_PART_close(partTag,0,1));
}
int main(int argc,char** argv){try{Require(argc==4,"User copy, round fixture folder, new output directory required");Require(!std::filesystem::exists(argv[3]),"Output exists");std::filesystem::create_directories(argv[3]);Check(UF_initialize());std::string out=argv[3],fixtures=argv[2];
    Case(argv[1],out+"/user",false,true);Case(fixtures+"/round-arc.prt-source.prt",out+"/two-oblique",true,false);Case(fixtures+"/round-arc-inch-rotated.prt-source.prt",out+"/inch-rotated",true,false);UF_terminate();return 0;
}catch(const NXException& e){std::cerr<<e.Message()<<std::endl;return 1;}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}}
