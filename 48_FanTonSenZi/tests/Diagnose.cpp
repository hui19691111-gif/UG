// Read-only recognizer report: which faces of a part the 方通/圆管伸直 command accepts,
// and the exact rejection reason for each one. Never saves the part.
#include "../TubeGeometry.hpp"
#include <NXOpen/Body.hxx>
#include <NXOpen/BodyCollection.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/Edge.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/Session.hxx>
#include <uf.h>
#include <uf_modl.h>
#include <uf_part.h>
#include <algorithm>
#include <iostream>
using namespace tube_straighten;
using namespace NXOpen;
int main(int argc,char** argv){try{
    if(argc<2||argc>3){std::cout<<"usage: TubeDiagnose <part-file> [plan]\n";return 2;}
    Check(UF_initialize());tag_t part=0;UF_PART_load_status_t status={};Check(UF_PART_open(argv[1],&part,&status));UF_PART_free_load_status(&status);
    auto* work=Session::GetSession()->Parts()->Work();
    std::cout<<"PART "<<argv[1]<<" units-mm-per-unit="<<[&]{int u=0;Check(UF_PART_ask_units(part,&u));return u==ENGLISH?25.4:1.0;}()<<"\n";
    for(auto* b:*work->Bodies()){
        std::cout<<"BODY "<<b->Tag()<<" solid="<<b->IsSolidBody()<<" blanked="<<b->IsBlanked();
        if(b->IsSolidBody())std::cout<<" volume_mm3="<<Volume(b->Tag())*1e9;
        std::cout<<"\n";
        if(!b->IsSolidBody())continue;
        int planar=0,curved=0,accepted=0;
        for(auto* f:b->GetFaces()){
            int type=0,sign=0;double p[3],n[3],box[6],r=0,r2=0;Check(UF_MODL_ask_face_data(f->Tag(),&type,p,n,box,&r,&r2,&sign));
            if(type==22)++planar;else ++curved;
            std::cout<<" FACE "<<f->Tag()<<" type="<<type<<" R="<<r<<" edges="<<f->GetEdges().size()<<" -> ";
            try{auto s=InspectFace(f->Tag());++accepted;
                std::cout<<"ACCEPT "<<s.width<<"x"<<s.depth<<" t="<<s.thickness<<" R="<<s.cornerRadius<<" spans="<<s.spans.size()<<" holes="<<s.holes.size()<<" round="<<s.round<<"\n";
                for(size_t i=0;i<s.spans.size();++i){const auto& q=s.spans[i];
                    std::cout<<"   SPAN "<<i<<" a=("<<q.a.x<<","<<q.a.y<<","<<q.a.z<<") b=("<<q.b.x<<","<<q.b.y<<","<<q.b.z<<") R="<<q.radius<<" angle="<<q.angle*180/pi<<" center=("<<q.center.x<<","<<q.center.y<<","<<q.center.z<<")\n";}
            }catch(const std::exception& e){std::cout<<"REJECT "<<e.what()<<"\n";}
        }
        std::cout<<" SUMMARY planar="<<planar<<" curved="<<curved<<" accepted="<<accepted<<"\n";
        // Optional second stage: run the command's own planning and creation on the same body.
        if(argc>2&&std::string(argv[2])=="plan"){
            Source source;bool found=false;
            for(auto* f:b->GetFaces()){try{source=InspectFace(f->Tag());found=true;break;}catch(const std::exception&){}}
            if(!found){std::cout<<" PLAN skipped: no face accepted\n";continue;}
            Settings opt;opt.divisions=12;opt.radiusMm=1;opt.kFactor=.4;opt.gapMm=.2;
            if(const char* v=std::getenv("ZH_DIAG_R"))opt.radiusMm=std::atof(v);
            if(const char* v=std::getenv("ZH_DIAG_K"))opt.kFactor=std::atof(v);
            try{auto p=MakePlan(source,opt);
                std::cout<<" PLAN segmented OK length="<<p.length<<" cuts="<<p.bends.size()<<" relief="<<([&]{int n=0;for(const auto& x:p.bends)n+=x.relief?1:0;return n;}())<<" slots="<<p.sourceSlots.size()<<"\n";
                for(size_t i=0;i<p.bends.size();++i){const auto& b=p.bends[i];
                    std::cout<<"  BEND"<<i<<" angle="<<b.angle*180/pi<<"deg start="<<b.start<<" allowance="<<b.allowance<<" reversed="<<b.reversed<<" relief="<<b.relief<<" sharpHeel="<<b.sharpHeel<<" heelR="<<b.heelRadius<<" depth="<<source.depth<<" t="<<source.thickness<<"\n";
                    std::cout<<"   NOTCH";for(const auto& q:Notch(p,b,0.))std::cout<<" ("<<q.x<<","<<q.y<<")";std::cout<<"\n";}
                auto mark=Session::GetSession()->SetUndoMark(Session::MarkVisibilityInvisible,"diagnose create");
                double before=Volume(b->Tag());tag_t flat=Create(p);
                std::cout<<" CREATE OK flat="<<flat<<" source_volume_delta="<<(Volume(b->Tag())-before)*1e9<<" mm3\n";
                if(!p.bends.empty()){const auto& bend=p.bends[0];double cx=bend.start+bend.allowance/2,depth=bend.acrossZ?source.width:source.depth;
                    auto probe=[&](double y,const char* name){Vec q=CutPoint(p,bend,cx,bend.reversed?depth-y:y,source.thickness/2);double xyz[]={q.x,q.y,q.z};int status=0;Check(UF_MODL_ask_point_containment(xyz,flat,&status));std::cout<<"  PROBE "<<name<<" y="<<y<<" status="<<status<<"\n";};
                    probe(source.thickness/2,"retained-wall");
                    probe(source.thickness+2,"just-above-point");
                    probe((source.thickness+depth)/2,"inside-v");}
                Session::GetSession()->UndoToMark(mark,nullptr);Session::GetSession()->DeleteUndoMark(mark,nullptr);
            }catch(const std::exception& e){std::cout<<" PLAN segmented REJECT "<<e.what()<<"\n";}
            auto whole=opt;whole.segmentArcs=false;whole.tubeKFactor=.5;
            try{auto p=MakePlan(source,whole);std::cout<<" PLAN whole-arc OK length="<<p.length<<" arcs="<<p.machineArcs.size()<<"\n";}
            catch(const std::exception& e){std::cout<<" PLAN whole-arc REJECT "<<e.what()<<"\n";}
        }
    }
    Check(UF_PART_close(part,0,1));UF_terminate();return 0;
}catch(const NXException& e){std::cerr<<e.Message()<<"\n";return 1;}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
