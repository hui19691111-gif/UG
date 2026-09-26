#include "../TubeCustomFeature.hpp"
#include <NXOpen/Body.hxx>
#include <NXOpen/BodyCollection.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/Features_ConstructionFeatureData.hxx>
#include <NXOpen/Features_CustomFeature.hxx>
#include <NXOpen/Features_FeatureGroup.hxx>
#include <NXOpen/Features_FeatureCollection.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <uf.h>
#include <uf_modl.h>
#include <uf_part.h>
#include <filesystem>
#include <iostream>
#include <set>
using namespace tube_straighten;
using namespace NXOpen;
int baselineSolids=0;
void Require(bool condition,const char* text){if(!condition)throw std::runtime_error(text);}
void CheckResult(Features::CustomFeature* custom,const std::set<tag_t>& original,const Settings& expected,tag_t face,double sourceVolume){
    auto* part=Session::GetSession()->Parts()->Work();tag_t selected=0;auto stored=ReadFeature(custom,selected);
    Require(selected==face&&stored.divisions==expected.divisions&&stored.segmentArcs==expected.segmentArcs&&stored.cutSource==expected.cutSource&&stored.hideSource==expected.hideSource,"settings/selection not persisted");
    Require(std::abs(stored.tubeKFactor-expected.tubeKFactor)<1e-12&&std::abs(stored.bridgeWidthMm-expected.bridgeWidthMm)<1e-12&&std::abs(stored.gapMm-expected.gapMm)<1e-12,"numeric settings lost");
    std::set<tag_t> owned;for(auto* data:custom->GetConstructionFeatures())owned.insert(data->GetFeature()->Tag());Require(!owned.empty(),"empty construction");
    int customCount=0;for(auto* feature:part->Features()->GetFeatures()){
        if(dynamic_cast<Features::CustomFeature*>(feature))++customCount;
        Require(!dynamic_cast<Features::FeatureGroup*>(feature),"ordinary feature group created");
        if(original.count(feature->Tag())||feature==custom||feature->IsInternal())continue;
        Require(owned.count(feature->Tag())==1,"loose or stale construction feature");
    }
    Require(customCount==1,"duplicate custom features");Require(!custom->Suppressed(),"custom feature remained suppressed");Require(custom->GetFeatureErrorMessages().empty(),"custom feature has errors");
    int solids=0;for(auto* b:*part->Bodies())if(b->IsSolidBody())++solids;Require(solids==baselineSolids+1,"old edit bodies leaked");
    auto* input=dynamic_cast<Face*>(NXObjectManager::Get(face));Require(input,"source selection lost");
    double volume=Volume(input->GetBody()->Tag());
    Require(expected.cutSource&&expected.segmentArcs?volume<sourceVolume:std::abs(volume-sourceVolume)<sourceVolume*1e-8,"source cuts not rebuilt or removed");
    Require(input->GetBody()->IsBlanked()==(expected.hideSource&&!(expected.cutSource&&expected.segmentArcs)),"source visibility wrong");
    std::cout<<"CHECK divisions="<<stored.divisions<<" segmented="<<stored.segmentArcs<<" cut_source="<<stored.cutSource<<" owned="<<owned.size()<<" source_volume="<<volume<<'\n'<<std::flush;
}
int main(int argc,char** argv){try{
    if(argc==4&&std::string(argv[1])=="--upgrade"){
        Require(!std::filesystem::exists(argv[3]),"upgrade output exists");Check(UF_initialize());RequireFeatureClass();tag_t partTag=0;UF_PART_load_status_t load={};Check(UF_PART_open(argv[2],&partTag,&load));UF_PART_free_load_status(&load);
        auto* session=Session::GetSession();auto* part=session->Parts()->Work();Features::CustomFeature* custom=nullptr;for(auto* f:part->Features()->GetFeatures())if(auto* c=dynamic_cast<Features::CustomFeature*>(f))custom=c;Require(custom,"old custom missing");tag_t face=0;auto s=ReadFeature(custom,face);Require(!s.useAnchor,"not a legacy anchorless fixture");auto source=InspectFace(face);s.useAnchor=true;s.anchorPoint=source.spans.front().Point(.25);auto mark=session->SetUndoMark(Session::MarkVisibilityVisible,"upgrade old custom");CreateFeature(face,s,custom,mark);auto saved=ReadFeature(custom,face);Require(saved.useAnchor&&Length(saved.anchorPoint-s.anchorPoint)<1e-9,"legacy anchor upgrade failed");Check(UF_PART_save_as(argv[3]));Check(UF_PART_close(partTag,0,1));UF_terminate();std::cout<<"PASS legacy custom anchor attributes upgrade and edit"<<std::endl;return 0;
    }
    if(argc==3&&(std::string(argv[1])=="--inspect-ui"||std::string(argv[1])=="--inspect-anchor-ui")){
        const bool anchored=std::string(argv[1])=="--inspect-anchor-ui";
        Check(UF_initialize());RequireFeatureClass();tag_t partTag=0;UF_PART_load_status_t load={};Check(UF_PART_open(argv[2],&partTag,&load));UF_PART_free_load_status(&load);
        auto* part=Session::GetSession()->Parts()->Work();Features::CustomFeature* custom=nullptr;int customs=0,solids=0,loose=0;
        for(auto* f:part->Features()->GetFeatures()){
            Require(!dynamic_cast<Features::FeatureGroup*>(f),"saved UI result has ordinary group");
            Require(!f->Suppressed()&&f->GetFeatureErrorMessages().empty(),"saved UI result has suppressed/error feature");
            if(auto* c=dynamic_cast<Features::CustomFeature*>(f)){custom=c;++customs;}
        }
        Require(customs==1,"saved UI result needs exactly one custom feature");tag_t face=0;auto s=ReadFeature(custom,face);
        Require(s.divisions==(anchored?4:12)&&!s.segmentArcs&&s.cutSource&&!s.hideSource&&std::abs(s.tubeKFactor-.42)<1e-12,"UI edit/cancel parameters not persisted");
        std::set<tag_t> owned;for(auto* c:custom->GetConstructionFeatures())owned.insert(c->GetFeature()->Tag());
        for(auto* f:part->Features()->GetFeatures())if(f!=custom&&!f->IsInternal()&&!owned.count(f->Tag()))++loose;
        for(auto* b:*part->Bodies())if(b->IsSolidBody())++solids;
        Require(solids==2&&loose==1&&!owned.empty(),"saved UI result has stale bodies/construction");
        auto source=InspectFace(face);MakePlan(source,s);
        if(anchored){
            Require(s.useAnchor,"UI pick point was not persisted");
            double xyz[]={s.anchorPoint.x,s.anchorPoint.y,s.anchorPoint.z};int status=0;Check(UF_MODL_ask_point_containment(xyz,source.body,&status));Require(status!=2,"persisted pick point is outside the source body");
            tag_t flat=0;for(auto* b:*part->Bodies())if(b->IsSolidBody()&&b->Tag()!=source.body)flat=b->Tag();Check(UF_MODL_ask_point_containment(xyz,flat,&status));Require(status!=2,"persisted fixed point moved off the flat body");
            std::cout<<"PASS UI anchor="<<s.anchorPoint.x<<','<<s.anchorPoint.y<<','<<s.anchorPoint.z<<" lies on both saved source and flat bodies"<<std::endl;
        }
        std::cout<<"PASS saved UI custom=1 groups=0 solids="<<solids<<" owned="<<owned.size()<<" source_features="<<loose<<" divisions="<<s.divisions<<" segmented="<<s.segmentArcs<<" tube_K="<<s.tubeKFactor<<(anchored?" anchor_persisted source_volume=":" canceled_K_0.41_not_committed source_volume=")<<Volume(source.body)<<std::endl;
        Check(UF_PART_close(partTag,0,1));UF_terminate();return 0;
    }
    Require(argc==3,"input and new output required");Require(!std::filesystem::exists(argv[2]),"output exists");Check(UF_initialize());RequireFeatureClass();
    tag_t partTag=0;UF_PART_load_status_t load={};Check(UF_PART_open(argv[1],&partTag,&load));UF_PART_free_load_status(&load);
    auto* session=Session::GetSession();auto* part=session->Parts()->Work();tag_t face=0;Source source;
    for(auto* b:*part->Bodies())if(!face&&b->IsSolidBody())for(auto* f:b->GetFaces())if(!face&&f->SolidFaceType()==Face::FaceTypePlanar){try{source=InspectFace(f->Tag());face=f->Tag();}catch(...){}}
    Require(face!=0,"no suitable planar face");for(auto* b:*part->Bodies())if(b->IsSolidBody())++baselineSolids;
    bool supportsWhole=true;try{Settings whole;whole.segmentArcs=false;MakePlan(source,whole);}catch(...){supportsWhole=false;}
    std::cout<<"INPUT holes="<<source.holes.size()<<" solids="<<baselineSolids<<" whole="<<supportsWhole<<std::endl;double volume=Volume(source.body);std::set<tag_t> original;for(auto* f:part->Features()->GetFeatures())original.insert(f->Tag());
    auto rawMark=session->SetUndoMark(Session::MarkVisibilityInvisible,"raw geometry comparison");Settings raw;raw.divisions=10;raw.gapMm=.25;raw.cutSource=true;std::cout<<"RAW geometry control"<<std::endl;Create(MakePlan(source,raw));session->UndoToMark(rawMark,nullptr);session->DeleteUndoMark(rawMark,nullptr);
    auto mark=session->SetUndoMark(Session::MarkVisibilityVisible,"custom test");Settings s;s.divisions=8;s.cutSource=true;
    auto result=CreateFeature(face,s,nullptr,mark);auto* custom=dynamic_cast<Features::CustomFeature*>(NXObjectManager::Get(result.feature));Require(custom,"not custom");CheckResult(custom,original,s,face,volume);
    for(int i=0;i<5;++i){
        s.segmentArcs=!supportsWhole||(i!=2&&i!=3);s.cutSource=i<4;s.hideSource=i==3;s.divisions=9+i;s.tubeKFactor=.43;s.gapMm=.25;
        auto editMark=session->SetUndoMark(Session::MarkVisibilityVisible,"edit custom test");
        std::cout<<"EDIT BEGIN "<<i<<std::endl;result=CreateFeature(face,s,custom,editMark);Require(result.feature==custom->Tag(),"edit identity changed");CheckResult(custom,original,s,face,volume);
    }
    auto bad=s;bad.segmentArcs=true;bad.gapMm=1000;auto failureMark=session->SetUndoMark(Session::MarkVisibilityVisible,"invalid edit");bool failed=false;
    try{CreateFeature(face,bad,custom,failureMark);}catch(...){failed=true;session->UndoToMark(failureMark,nullptr);session->DeleteUndoMark(failureMark,nullptr);}Require(failed,"invalid edit accepted");CheckResult(custom,original,s,face,volume);
    session->UndoToMark(mark,nullptr);session->DeleteUndoMark(mark,nullptr);Require(part->Features()->GetFeatures().size()==original.size(),"undo leaked features");Require(std::abs(Volume(source.body)-volume)<volume*1e-8,"undo changed source");
    s.cutSource=true;s.segmentArcs=true;s.hideSource=false;s.divisions=8;mark=session->SetUndoMark(Session::MarkVisibilityVisible,"save custom");result=CreateFeature(face,s,nullptr,mark);Check(UF_PART_save_as(argv[2]));Check(UF_PART_close(partTag,0,1));
    Check(UF_PART_open(argv[2],&partTag,&load));UF_PART_free_load_status(&load);part=session->Parts()->Work();custom=nullptr;for(auto* f:part->Features()->GetFeatures())if(auto* c=dynamic_cast<Features::CustomFeature*>(f))custom=c;
    Require(custom,"saved custom feature missing");s=ReadFeature(custom,face);s.cutSource=false;s.segmentArcs=!supportsWhole;
    mark=session->SetUndoMark(Session::MarkVisibilityVisible,"edit reopened");result=CreateFeature(face,s,custom,mark);Require(result.body&&custom->GetFeatureErrorMessages().empty(),"reopened edit failed");
    Require(std::abs(Volume(result.plan.source.body)-volume)<volume*1e-8,"reopened edit failed to restore uncut source");
    int solids=0;for(auto* b:*part->Bodies())if(b->IsSolidBody())++solids;Require(solids==baselineSolids+1,"reopened edit leaked solids");Check(UF_PART_save());Check(UF_PART_close(partTag,0,1));UF_terminate();
    std::cout<<"PASS create, parameter edits, source slots on/off, no stale features/bodies, failed-edit rollback, undo, save/reopen/edit\n";return 0;
}catch(const NXException& e){std::cerr<<"NX FAIL "<<e.ErrorCode()<<" "<<e.Message()<<'\n';}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';}return 1;}
