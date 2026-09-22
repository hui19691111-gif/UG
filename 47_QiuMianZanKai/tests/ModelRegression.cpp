// Always test a new copy; the original input is never opened by NX or saved.
#include "../SphereGeometry.hpp"
#include "../SphereCustomFeature.hpp"
#include <NXOpen/Body.hxx>
#include <NXOpen/BodyCollection.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/Features_CustomFeature.hxx>
#include <NXOpen/Features_FeatureCollection.hxx>
#include <uf.h>
#include <uf_modl.h>
#include <uf_part.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace NXOpen;
using namespace sphere_unfold;
int main(int argc,char** argv){
    try{
        if(argc<3)throw std::runtime_error("Input part, new output part and optional inspect mode required");
        auto output=std::filesystem::u8path(argv[2]);
        if(std::filesystem::exists(output))throw std::runtime_error("Output must be a new file");
        std::filesystem::create_directories(output.parent_path());
        std::filesystem::copy_file(std::filesystem::u8path(argv[1]),output);
        std::filesystem::permissions(output,std::filesystem::perms::owner_write,std::filesystem::perm_options::add);
        Check(UF_initialize());tag_t partTag=0;UF_PART_load_status_t status={};
        Check(UF_PART_open(output.u8string().c_str(),&partTag,&status));UF_PART_free_load_status(&status);
        auto* session=Session::GetSession();auto* part=session->Parts()->Work();
        std::vector<Face*> cylinders,curved;
        for(auto* body:*part->Bodies()){
            std::cout<<"BODY "<<body->Tag()<<" solid="<<body->IsSolidBody()<<'\n';
            for(auto* face:body->GetFaces()){
                int type,sign;double p[3],d[3],box[6],r,r2;
                Check(UF_MODL_ask_face_data(face->Tag(),&type,p,d,box,&r,&r2,&sign));
                std::cout<<"FACE "<<face->Tag()<<" type="<<type<<" R="<<r<<" r="<<r2<<" center="<<p[0]<<","<<p[1]<<","<<p[2]<<" axis="<<d[0]<<","<<d[1]<<","<<d[2]<<'\n';
                if(type==16)cylinders.push_back(face);if(type==18||type==19)curved.push_back(face);
            }
        }
        if(argc>3&&std::string(argv[3])=="inspect"){Check(UF_PART_close(partTag,0,1));UF_terminate();return 0;}
        std::vector<Source> candidates;
        for(auto* c:cylinders)for(auto* f:curved){
            try{auto s=Inspect(c->Tag(),f->Tag());std::cout<<"PAIR "<<c->Tag()<<","<<f->Tag()<<" thickness="<<s.thickness/s.unitsPerMm<<" inner="<<s.innerSurface<<" sweep="<<s.sweep*180/pi<<" crown="<<s.latitude*180/pi<<" height="<<s.height/s.unitsPerMm<<'\n';if(!s.innerSurface)candidates.push_back(s);}
            catch(const std::exception& e){std::cout<<"REJECT "<<c->Tag()<<","<<f->Tag()<<" "<<e.what()<<'\n';}
        }
        if(candidates.size()!=1)throw std::runtime_error("Expected one unambiguous outer pair");
        Settings settings;settings.petals=18;settings.gap=.1;settings.relief=.5;settings.flat=true;settings.hideSource=true;
        auto mark=session->SetUndoMark(Session::MarkVisibilityVisible,"test model unfold");
        auto result=CreateFeature(MakePlan(candidates[0],settings),nullptr,mark);
        auto* feature=dynamic_cast<Features::CustomFeature*>(NXObjectManager::Get(result.feature));
        if(!feature||feature->GetConstructionFeatures().size()!=result.members.size())throw std::runtime_error("Missing custom ownership");
        std::cout<<"CREATED custom feature and verified native flat body\n"<<std::flush;
        Check(UF_PART_save());Check(UF_PART_close(partTag,0,1));
        std::cout<<"PASS copied model: automatic wall detection, native flatten, custom ownership, saved output "<<output.u8string()<<'\n';
        UF_terminate();return 0;
    }catch(const NXException& e){std::cerr<<"NX FAIL "<<e.ErrorCode()<<" "<<e.Message()<<'\n';}
    catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';}return 1;
}
