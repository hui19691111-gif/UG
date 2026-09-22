#include "../TubeGeometry.hpp"
#include <NXOpen/Body.hxx>
#include <NXOpen/BodyCollection.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/Edge.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/Session.hxx>
#include <uf.h>
#include <uf_part.h>
#include <uf_modl.h>
#include <uf_eval.h>
#include <iostream>
using namespace tube_straighten;
int main(int argc,char** argv){try{if(argc!=2)return 2;Check(UF_initialize());tag_t part=0;UF_PART_load_status_t status={};Check(UF_PART_open(argv[1],&part,&status));UF_PART_free_load_status(&status);
for(auto* b:*NXOpen::Session::GetSession()->Parts()->Work()->Bodies()){
std::cout<<"BODY "<<b->Tag()<<" blank="<<b->IsBlanked()<<" solid="<<b->IsSolidBody()<<" volume="<<Volume(b->Tag())<<"\n";
if(b->IsBlanked())continue;
for(auto* f:b->GetFaces()){int type=0,sign=0;double p[3],n[3],box[6],r,r2;Check(UF_MODL_ask_face_data(f->Tag(),&type,p,n,box,&r,&r2,&sign));
std::cout<<"FACE "<<f->Tag()<<" type="<<type<<" p="<<p[0]<<","<<p[1]<<","<<p[2]<<" n="<<n[0]<<","<<n[1]<<","<<n[2]<<" r="<<r<<" sign="<<sign<<" edges="<<f->GetEdges().size()<<"\n";
for(auto* e:f->GetEdges()){UF_EVAL_p_t v=nullptr;Check(UF_EVAL_initialize_2(e->Tag(),&v));double lim[2],a[3],c[3];logical line=false,arc=false;Check(UF_EVAL_ask_limits(v,lim));Check(UF_EVAL_evaluate(v,0,lim[0],a,nullptr));Check(UF_EVAL_evaluate(v,0,lim[1],c,nullptr));Check(UF_EVAL_is_line(v,&line));Check(UF_EVAL_is_arc(v,&arc));std::cout<<" EDGE "<<e->Tag()<<" "<<(line?"line":arc?"arc":"other")<<" "<<a[0]<<","<<a[1]<<","<<a[2]<<" -> "<<c[0]<<","<<c[1]<<","<<c[2];if(arc){UF_EVAL_arc_t d;Check(UF_EVAL_ask_arc(v,&d));std::cout<<" R="<<d.radius<<" C="<<d.center[0]<<","<<d.center[1]<<","<<d.center[2];}std::cout<<"\n";UF_EVAL_free(v);}
}}
Check(UF_PART_close(part,0,1));UF_terminate();return 0;}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
