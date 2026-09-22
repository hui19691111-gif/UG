// Read-only audit after NX has closed its interactive custom-feature editor.
// Only run on the dedicated integration fixture; never deployed.
#include <NXOpen/Body.hxx>
#include <NXOpen/BodyCollection.hxx>
#include <NXOpen/Curve.hxx>
#include <NXOpen/CurveCollection.hxx>
#include <NXOpen/Features_CustomFeature.hxx>
#include <NXOpen/Features_CustomFeatureData.hxx>
#include <NXOpen/Features_CustomTagArrayAttribute.hxx>
#include <NXOpen/Features_CustomIntegerAttribute.hxx>
#include <NXOpen/Features_CustomLogicalAttribute.hxx>
#include <NXOpen/Features_ConstructionFeatureData.hxx>
#include <NXOpen/Features_FeatureCollection.hxx>
#include <NXOpen/Features_SketchFeature.hxx>
#include <NXOpen/Features_Extrude.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/Sketch.hxx>
#include <NXOpen/SketchCollection.hxx>
#include <NXOpen/NXException.hxx>
#include <uf.h>
#include <filesystem>
#include <fstream>
#include <set>
#include <stdexcept>
using namespace NXOpen;
void Require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
extern "C" __declspec(dllexport) void ufusr(char*,int* rc,int){
    *rc=UF_initialize();if(*rc)return;
    std::ofstream log(std::filesystem::temp_directory_path()/"Zhihui-SphereUIAudit.log",std::ios::app);
    try{
        auto* part=Session::GetSession()->Parts()->Work();Require(part!=nullptr,"no work part");
        Features::CustomFeature* custom=nullptr;int count=0,sketches=0,extrudes=0,loose=0,solids=0;
        for(auto* feature:part->Features()->GetFeatures()){
            if(auto* f=dynamic_cast<Features::CustomFeature*>(feature)){custom=f;++count;}
            if(auto* f=dynamic_cast<Features::SketchFeature*>(feature)){++sketches;Require(f->IsInternal()&&f->Sketch()->IsBlanked(),"sketch external or visible");}
            if(dynamic_cast<Features::Extrude*>(feature))++extrudes;
        }
        Require(count==1,"expected one custom feature in fixture");
        auto* data=custom->FeatureData();int petals=data->CustomIntegerAttributeByName("Petals")->Value();
        bool flat=data->CustomLogicalAttributeByName("Flat")->Value();
        std::set<tag_t> expected,actual;
        for(auto* o:data->CustomTagArrayAttributeByName("InternalFeatures")->GetValues())expected.insert(o->Tag());
        for(auto* o:custom->GetConstructionFeatures())actual.insert(o->GetFeature()->Tag());
        Require(actual==expected,"construction membership differs from saved outputs");
        Require(custom->GetFeatureErrorMessages().empty(),"feature has update errors");
        Require(sketches==petals*3&&extrudes==sketches,"old profile history leaked");
        for(auto* curve:*part->Curves())if(!part->Sketches()->GetOwningSketch(curve))++loose;
        const bool importedFixture=std::string(part->FullPath().GetUTF8Text()).find("SameBody_08")!=std::string::npos;
        Require(loose==(importedFixture?0:6),"loose profile curves leaked"); // imported solid or source revolve's six curves
        for(auto* body:*part->Bodies())if(body->IsSolidBody())++solids;
        Require(solids==(flat?3:2),"old bodies leaked");
        log<<"PASS part="<<part->FullPath().GetUTF8Text()<<" custom="<<custom->Tag()<<" petals="<<petals<<" flat="<<flat<<" members="<<actual.size()<<" internal_sketches="<<sketches<<" source_curves="<<loose<<" solids="<<solids<<'\n';
    }catch(const NXException& e){*rc=e.ErrorCode();log<<"FAIL NX "<<e.Message()<<'\n';}
    catch(const std::exception& e){*rc=1;log<<"FAIL "<<e.what()<<'\n';}
    UF_terminate();
}
extern "C" __declspec(dllexport) int ufusr_ask_unload(){return UF_UNLOAD_IMMEDIATELY;}
