#include "../TubeGeometry.hpp"
#include <NXOpen/Body.hxx>
#include <NXOpen/BodyCollection.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/Features_FeatureCollection.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/NXException.hxx>
#include <uf.h>
#include <uf_modl.h>
#include <uf_part.h>
#include <iostream>
#include <stdexcept>
using namespace tube_straighten;
using namespace NXOpen;
void Require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int Contains(tag_t body,Vec q){double point[]={q.x,q.y,q.z};int state=0;Check(UF_MODL_ask_point_containment(point,body,&state));return state;}
int Solids(Part* part){int count=0;for(auto* body:*part->Bodies())if(body->IsSolidBody())++count;return count;}
int main(int argc,char** argv){try{
    Require(argc==2,"Independent 20x20x2 R3 split-side source copy required");Check(UF_initialize());tag_t partTag=0;UF_PART_load_status_t status={};Check(UF_PART_open(argv[1],&partTag,&status));UF_PART_free_load_status(&status);
    auto* session=Session::GetSession();auto* part=session->Parts()->Work();std::vector<Source> sources;size_t planarFaces=0;
    for(auto* body:*part->Bodies())if(body->IsSolidBody())for(auto* face:body->GetFaces()){
        if(face->SolidFaceType()!=Face::FaceTypePlanar)continue;++planarFaces;try{sources.push_back(InspectFace(face->Tag()));std::cout<<"ACCEPT face="<<face->Tag()<<std::endl;}
        catch(const std::exception& e){std::cout<<"REJECT face="<<face->Tag()<<" "<<e.what()<<std::endl;}
    }
    Require(sources.size()==planarFaces,"Every planar face including split sides, inner walls and caps must be selectable");
    Require(Solids(part)==1,"Expected one source solid");const auto originalFeatures=part->Features()->GetFeatures().size();double commonLength=0;
    for(size_t face=0;face<sources.size();++face){const auto& source=sources[face];
        Require(std::abs(source.width-20)<1e-5&&std::abs(source.depth-20)<1e-5&&std::abs(source.thickness-2)<1e-5&&std::abs(source.cornerRadius-3)<1e-5,"Section recognition changed");
        Require(source.spans.size()==2&&source.holes.empty(),"Missing straight or arc span");
        int straight=0,arc=0;for(const auto& span:source.spans){if(span.radius)++arc;else{++straight;Require(std::abs(Length(span.b-span.a)-310)<1e-5,"Lost straight length");}}
        Require(straight==1&&arc==1,"Expected terminal arc and straight segment");
        const double sourceVolume=Volume(source.body),area=400-(4-pi)*9-(256-(4-pi));
        for(int mode=0;mode<3;++mode){Settings settings;settings.divisions=15;settings.segmentArcs=mode!=0;settings.cutSource=mode==2;auto plan=MakePlan(source,settings);
            if(mode==0){Require(std::abs(plan.length-sourceVolume*1e9/area)<1e-5,"Whole length disagrees with independent source volume/area");if(!face)commonLength=plan.length;else Require(std::abs(plan.length-commonLength)<1e-7,"Selected split face changed whole length");}
            else Require(plan.bends.size()==15,"Seam introduced an artificial bend or lost an arc cut");
            Require(!Preview(plan).empty(),"Missing preview");auto mark=session->SetUndoMark(Session::MarkVisibilityInvisible,"split side regression");auto flat=Create(plan);
            Require(Solids(part)==2&&Volume(flat)>0,"Invalid or duplicate straight stock");
            for(Vec yz:std::vector<Vec>{{1,10,0},{19,10,0},{10,1,0},{10,19,0}}){auto q=FlatPoint(plan,0,yz.x,yz.y);Require(Contains(source.body,q)==3&&Contains(flat,q)==3,"Start cap moved");}
            if(mode==0)Require(std::abs(Volume(flat)-sourceVolume)<sourceVolume*1e-6,"Whole straight stock lost material");
            Require(mode==2?Volume(source.body)<sourceVolume:std::abs(Volume(source.body)-sourceVolume)<sourceVolume*1e-10,"Source cut switch wrong");
            session->UndoToMark(mark,nullptr);session->DeleteUndoMark(mark,nullptr);
            Require(Solids(part)==1&&part->Features()->GetFeatures().size()==originalFeatures&&std::abs(Volume(source.body)-sourceVolume)<sourceVolume*1e-10,"Undo left changed source or construction");
            std::cout<<"PASS face="<<face<<" mode="<<mode<<" length="<<plan.length<<" bends="<<plan.bends.size()<<" aligned_caps=4"<<std::endl;
        }
    }
    Check(UF_PART_close(partTag,0,1));UF_terminate();std::cout<<"PASS all planar selections x three output modes; full path, length, cap alignment, material, source switch and undo verified"<<std::endl;return 0;
}catch(const NXException& e){std::cerr<<e.Message()<<std::endl;}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;}return 1;}
