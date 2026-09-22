#include "../TubeGeometry.hpp"
#include <NXOpen/Body.hxx>
#include <NXOpen/BodyCollection.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/Edge.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/Features_FeatureCollection.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/Session.hxx>
#include <uf.h>
#include <uf_modl.h>
#include <uf_part.h>
#include <uf_eval.h>
#include <iostream>
#include <filesystem>
using namespace tube_straighten;
void Require(bool b,const char* m){if(!b)throw std::runtime_error(m);}
int main(int argc,char** argv){try{
Require(argc==3,"Input part and NEW output part required");Require(!std::filesystem::exists(argv[2]),"Output already exists");Check(UF_initialize());tag_t tag=0;UF_PART_load_status_t status={};Check(UF_PART_open(argv[1],&tag,&status));UF_PART_free_load_status(&status);
auto* session=NXOpen::Session::GetSession();auto* part=session->Parts()->Work();Plan chosen;int sides=0;
for(auto* b:*part->Bodies())if(!b->IsBlanked()&&b->IsSolidBody())for(auto* f:b->GetFaces()){
int type,sign;double pt[3],n[3],box[6],r,r2;Check(UF_MODL_ask_face_data(f->Tag(),&type,pt,n,box,&r,&r2,&sign));if(type!=22)continue;
try{auto source=InspectFace(f->Tag());std::cout<<"FACE "<<f->Tag()<<" accepted: "<<source.width<<"x"<<source.depth<<" t="<<source.thickness<<" R="<<source.cornerRadius<<" holes="<<source.holes.size()<<std::endl;
Require(source.holes.size()==6,"Expected all 6 wall openings");auto p=MakePlan(source,Settings{});std::cout<<"PLAN length="<<p.length<<" adjusted="<<p.adjustedCuts<<" holes="<<p.holeSegments.size()<<std::endl;chosen=p;Settings eight;eight.divisions=8;auto p8=MakePlan(source,eight);Require(p8.holeSegments.size()==6,"8-cut plan lost holes");std::cout<<"8-CUT PLAN adjusted="<<p8.adjustedCuts<<" length="<<p8.length<<std::endl;++sides;
}catch(const std::exception& e){std::cout<<"FACE "<<f->Tag()<<" rejected: "<<e.what()<<std::endl;}}
Require(sides==2,"Both complete exterior side faces must be selectable");double before=Volume(chosen.source.body);auto count=part->Features()->GetFeatures().size();auto mark=session->SetUndoMark(NXOpen::Session::MarkVisibilityVisible,"user model verification");
tag_t result=Create(chosen);auto* body=dynamic_cast<NXOpen::Body*>(NXOpen::NXObjectManager::Get(result));double maxError=0;
// Probe the real end-cap material in source coordinates, independently of
// FlatPoint, so an unintended display/placement offset fails this check.
const auto& src=chosen.source;Vec inside=Cross(src.normal,src.spans.front().Tangent(0));
for(Vec yz:std::vector<Vec>{{src.thickness/2,src.width/2,0},{src.depth-src.thickness/2,src.width/2,0},{src.depth/2,src.thickness/2,0},{src.depth/2,src.width-src.thickness/2,0}}){
Vec q=src.spans.front().a+inside*yz.x+src.widthDirection*yz.y;double xyz[]={q.x,q.y,q.z};int originalStatus=0,flatStatus=0;
Check(UF_MODL_ask_point_containment(xyz,src.body,&originalStatus));Check(UF_MODL_ask_point_containment(xyz,result,&flatStatus));
Require(originalStatus==3&&flatStatus==3,"Straightened start section does not coincide with original end cap");
}
std::cout<<"PLACEMENT original and flat start end caps coincide at all 4 walls"<<std::endl;
for(size_t i=0;i<chosen.source.holes.size();++i){auto local=FlatHole(chosen,i);const auto& original=chosen.source.holes[i];Require(original.profile.size()==1&&original.profile[0].radius>0,"Expected circular user holes");auto expected=FlatPoint(chosen,local.profile[0].center.x,local.profile[0].center.y,local.profile[0].center.z);double best=1e100;
for(auto* edge:body->GetEdges()){UF_EVAL_p_t eval=nullptr;Check(UF_EVAL_initialize_2(edge->Tag(),&eval));logical arc=false;Check(UF_EVAL_is_arc(eval,&arc));if(arc){UF_EVAL_arc_t data;Check(UF_EVAL_ask_arc(eval,&data));Vec center={data.center[0],data.center[1],data.center[2]};double error=Length(center-expected)+std::abs(data.radius-original.profile[0].radius);best=std::min(best,error);}UF_EVAL_free(eval);}
Require(best<1e-5,"Created hole center or radius changed");auto restored=ToFolded(chosen,chosen.holeSegments[i],local.profile[0].center);maxError=std::max(maxError,Length(restored-original.profile[0].center)+best);
std::cout<<"HOLE "<<i+1<<" segment="<<chosen.holeSegments[i]<<" folded center error="<<Length(restored-original.profile[0].center)+best<<std::endl;
}
Require(std::abs(Volume(chosen.source.body)-before)<before*1e-10,"Original body modified");session->UndoToMark(mark,nullptr);session->DeleteUndoMark(mark,nullptr);Require(part->Features()->GetFeatures().size()==count,"Undo leaked geometry");Create(chosen);Check(UF_PART_save_as(argv[2]));Check(UF_PART_close(tag,0,1));UF_terminate();std::cout<<"PASS user rounded 50x50x2, coincident start section, 2 face selections, 6 wall holes, measured hole centers/radii, inverse folding error="<<maxError<<", source unchanged, undo, saved output\n";return 0;
}catch(const NXOpen::NXException& e){std::cerr<<e.Message()<<"\n";return 1;}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
