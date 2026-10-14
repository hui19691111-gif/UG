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
#include <uf_modl.h>
#include <uf_obj.h>
#include <uf_part.h>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace tube_straighten;
using namespace NXOpen;
void Require(bool b,const char* m){if(!b)throw std::runtime_error(m);}
int Contains(tag_t body,Vec q){double xyz[]={q.x,q.y,q.z};int status=0;Check(UF_MODL_ask_point_containment(xyz,body,&status));return status;}
int Solids(Part* p){int n=0;for(auto* b:*p->Bodies())if(b->IsSolidBody())++n;return n;}
double SectionArea(const Source& s){auto area=[](double w,double d,double r){return w*d-(4-pi)*r*r;};return area(s.width,s.depth,s.cornerRadius)-area(s.width-2*s.thickness,s.depth-2*s.thickness,std::max(0.,s.cornerRadius-s.thickness));}
Source Recognize(Part* part){for(auto* b:*part->Bodies())if(b->IsSolidBody()&&!b->IsBlanked())for(auto* f:b->GetFaces()){try{return InspectFace(f->Tag());}catch(const std::exception&){}}throw std::runtime_error("No source recognized");}

// 40 wide x 30 deep x 2 thick square tube, 90 deg bend whose inner side closes to the apex:
// the bend outer radius equals the section depth, so the bend inner radius is exactly 0.
// Built as one revolution minus another plus two tangent straight legs, so the wall stays uniform.
const double Depth=30,Thick=2,Width=40,BendRadius=Depth;
static tag_t Line(Vec a,Vec b){UF_CURVE_line_t l={{a.x,a.y,a.z},{b.x,b.y,b.z}};tag_t c=0;Check(UF_CURVE_create_line(&l,&c));return c;}
static tag_t Revolve90(const std::vector<Vec>& points){
    std::vector<tag_t> curves;for(size_t i=0;i<points.size();++i)curves.push_back(Line(points[i],points[(i+1)%points.size()]));
    char zero[]="0",quarter[]="90";char* limits[]={zero,quarter},*offsets[]={zero,zero};double origin[3]={},axis[]={0,0,1};tag_t* features=nullptr;int count=0;
    Check(UF_MODL_create_revolution(curves.data(),static_cast<int>(curves.size()),nullptr,limits,offsets,origin,false,true,origin,axis,UF_NULLSIGN,&features,&count));
    Require(count==1,"fixture revolve");tag_t body=0;Check(UF_MODL_ask_feat_body(features[0],&body));UF_free(features);
    for(auto c:curves)Check(UF_OBJ_set_blank_status(c,UF_OBJ_BLANKED));return body;
}
static tag_t Extrude(const std::vector<Vec>& points,Vec direction,double length){
    uf_list_p_t curves=nullptr,features=nullptr;Check(UF_MODL_create_list(&curves));std::vector<tag_t> tags;
    for(size_t i=0;i<points.size();++i){auto t=Line(points[i],points[(i+1)%points.size()]);tags.push_back(t);Check(UF_MODL_put_list_item(curves,t));}
    char zero[]="0";std::string end=std::to_string(length);char* limits[]={zero,end.data()};double p[3]={},d[]={direction.x,direction.y,direction.z};
    Check(UF_MODL_create_extruded(curves,zero,limits,p,d,UF_NULLSIGN,&features));tag_t f=0,body=0;Check(UF_MODL_ask_list_item(features,0,&f));Check(UF_MODL_ask_feat_body(f,&body));
    UF_MODL_delete_list(&features);UF_MODL_delete_list(&curves);for(auto t:tags)Check(UF_OBJ_set_blank_status(t,UF_OBJ_BLANKED));return body;
}
static tag_t ZeroInnerFixture(const std::filesystem::path& file){
    const double t=Thick,w=Width,R=BendRadius,leg=120;
    tag_t part=0;Check(UF_PART_new(file.string().c_str(),METRIC,&part));
    tag_t outer=Revolve90({{0,0,0},{R,0,0},{R,0,w},{0,0,w}});
    tag_t cavity=Revolve90({{t,0,t},{R-t,0,t},{R-t,0,w-t},{t,0,w-t}});
    tag_t legA=Extrude({{0,-leg,0},{R,-leg,0},{R,0,0},{0,0,0}},{0,0,1},w);
    tag_t legABore=Extrude({{t,-leg,t},{R-t,-leg,t},{R-t,0,t},{t,0,t}},{0,0,1},w-2*t);
    tag_t legB=Extrude({{0,0,0},{0,R,0},{0,R,w},{0,0,w}},{-1,0,0},leg);
    tag_t legBBore=Extrude({{0,t,t},{0,R-t,t},{0,R-t,w-t},{0,t,w-t}},{-1,0,0},leg);
    Check(UF_MODL_unite_bodies(outer,legA));Check(UF_MODL_unite_bodies(outer,legB));
    Check(UF_MODL_unite_bodies(cavity,legABore));Check(UF_MODL_unite_bodies(cavity,legBBore));
    tag_t feature=0;Check(UF_MODL_subtract_bodies_with_retained_options(outer,cavity,false,false,&feature));
    Check(UF_PART_save());Check(UF_PART_close(part,0,1));return outer;
}

static void ZeroInnerCase(const std::filesystem::path& source,const std::filesystem::path& output){
    std::cout<<"BEGIN "<<output.string()<<std::endl;
    tag_t partTag=0;UF_PART_load_status_t load={};Check(UF_PART_open(source.string().c_str(),&partTag,&load));UF_PART_free_load_status(&load);
    auto* session=Session::GetSession();auto* part=session->Parts()->Work();auto s=Recognize(part);
    double u=s.unitsPerMm,sourceVolume=Volume(s.body);int initialSolids=Solids(part);auto initialFeatures=part->Features()->GetFeatures().size();
    Require(std::abs(s.width-Width)<1e-6&&std::abs(s.depth-Depth)<1e-6&&std::abs(s.thickness-Thick)<1e-6,"Section recognition changed");
    int arcs=0;for(const auto& span:s.spans)if(span.radius){++arcs;Require(std::abs(span.radius-s.depth)<1e-6,"Fixture is not an inner-radius-zero bend");}
    Require(arcs==1&&s.spans.size()==3,"Fixture path changed");
    // 选面: 方通任一平面(侧平面、直段管壁、内壁、端面)都应能识别; 圆弧曲面不在选择范围。
    int planar=0,accepted=0,curved=0;
    for(auto* face:dynamic_cast<Body*>(NXObjectManager::Get(s.body))->GetFaces()){
        int type=0,sign=0;double point[3],normal[3],box[6],radius=0,radius2=0;Check(UF_MODL_ask_face_data(face->Tag(),&type,point,normal,box,&radius,&radius2,&sign));
        bool flat=type==22;if(flat)++planar;else ++curved;
        try{InspectFace(face->Tag());++accepted;}catch(const std::exception&){}
    }
    std::cout<<"  faces: planar="<<planar<<" curved="<<curved<<" selectable="<<accepted<<std::endl;
    Require(accepted>=planar,"Not every planar face of the zero inner radius tube is selectable");
    // 逐弯位自动判断: 内R=0 的圆弧弯位只留正中一刀, 料长按外壁展开。
    Settings opt;opt.divisions=12;opt.radiusMm=1;opt.kFactor=.4;opt.gapMm=.2;
    auto p=MakePlan(s,opt);
    Require(p.bends.size()==1&&std::abs(p.bends[0].angle-pi/2)<1e-9,"Inner radius zero must keep exactly one full-angle cut");
    Require(p.bends[0].relief,"Zero inner radius corner not marked as a single relief cut");
    double wall=(BendRadius-(1-opt.kFactor)*Thick)*pi/2,legs=2*120;
    Require(std::abs(p.length-(legs+wall))<.1,"Flat length must follow the outer wall");
    Require(!Preview(p).empty(),"No preview for the zero inner radius corner");
    // 整体圆弧(弯管机)模式必须拒绝内R=0。
    auto whole=opt;whole.segmentArcs=false;whole.tubeKFactor=.5;bool rejected=false;
    try{MakePlan(s,whole);}catch(const std::exception&){rejected=true;}
    Require(rejected,"Whole-arc machine mode accepted an inner-radius-zero bend");
    // 生成下料件: 保留壁完整、另外三面在弯位被切开、直段仍是完整方通。
    auto mark=session->SetUndoMark(Session::MarkVisibilityVisible,"zero inner radius blank");tag_t flat=Create(p);
    Require(flat!=s.body&&Solids(part)==initialSolids+1,"Blank body count");
    Require(std::abs(Volume(s.body)-sourceVolume)<sourceVolume*1e-10,"Blank generation altered the source");
    // 弯位正中三个面必须被切开、保留壁连续; 远离切口的两端壁厚完整。
    double mid=p.bends[0].start+p.bends[0].allowance/2;
    for(double x:{5.,p.length-5.}){
        Require(Contains(flat,FlatPoint(p,x,Thick/2,Width/2))==1,"Retained wall missing at the blank end");
        for(auto yz:std::vector<Vec>{{0,Depth-Thick/2,Width/2},{0,Depth/2,Thick/2},{0,Depth/2,Width-Thick/2}})
            Require(Contains(flat,FlatPoint(p,x,yz.y,yz.z))==1,"Blank end walls cut although far from the notch");
    }
    Require(Contains(flat,FlatPoint(p,mid,Thick/2,Width/2))==1,"Retained wall missing at the notch");
    for(auto yz:std::vector<Vec>{{0,Depth-Thick/2,Width/2},{0,Depth/2,Thick/2},{0,Depth/2,Width-Thick/2}})
        Require(Contains(flat,FlatPoint(p,mid,yz.y,yz.z))==2,"Notch did not open the other three walls");
    Require(Contains(flat,FlatPoint(p,mid,Depth/2,Width/2))==2,"Blank bore filled");
    Vec cap=FlatPoint(p,0,Thick/2,Width/2);Require(Contains(s.body,cap)==3&&Contains(flat,cap)==3,"Start cap not coincident");
    session->UndoToMark(mark,nullptr);session->DeleteUndoMark(mark,nullptr);
    Require(initialFeatures==part->Features()->GetFeatures().size()&&initialSolids==Solids(part),"Blank undo leaked geometry");
    // 原管同步开槽: 圆弧正中一刀, 原管去料但不移动。
    auto cut=opt;cut.cutSource=true;cut.hideSource=true;auto slotted=MakePlan(s,cut);
    Require(slotted.sourceSlots.size()==1&&slotted.sourceSlots[0].pathRadius>0,"Source slot planning failed");
    Require(std::abs(std::atan2(slotted.sourceSlots[0].origin.y,slotted.sourceSlots[0].origin.x)-pi/4)<1e-6,"Source slot not at the middle of the arc");
    mark=session->SetUndoMark(Session::MarkVisibilityInvisible,"zero inner radius source slot");Create(slotted);
    double afterSlot=Volume(s.body);
    Require(afterSlot<sourceVolume-1e-9,"Source slot did not remove material");
    const auto& slot=slotted.sourceSlots[0];
    Require(Contains(s.body,SourceSlotPoint(slotted,slot,0,Depth-Thick/2,Width/2))==2,"Source slot did not open the opposite wall");
    for(double z:{Thick/2,Width-Thick/2})Require(Contains(s.body,SourceSlotPoint(slotted,slot,0,Depth/2,z))==2,"Source slot did not open the side walls");
    Require(Contains(s.body,SourceSlotPoint(slotted,slot,0,Thick/2,Width/2))==1,"Source slot cut into the retained wall");
    Require(Solids(part)==initialSolids+1,"Source slot split or lost bodies");
    std::cout<<"  source slot removed "<<(sourceVolume-afterSlot)*1e9<<" mm^3 at the middle of the arc"<<std::endl;
    session->UndoToMark(mark,nullptr);session->DeleteUndoMark(mark,nullptr);
    Require(std::abs(Volume(s.body)-sourceVolume)<sourceVolume*1e-10&&initialSolids==Solids(part),"Source slot undo failed");
    // 保存重开: 下料件与原管都在。
    Create(p);Check(UF_PART_save_as(output.string().c_str()));Check(UF_PART_close(partTag,0,1));
    load={};Check(UF_PART_open(output.string().c_str(),&partTag,&load));UF_PART_free_load_status(&load);
    Require(Solids(session->Parts()->Work())==initialSolids+1,"Saved blank missing");
    Check(UF_PART_close(partTag,0,1));
    std::cout<<"PASS inner-radius-zero arc bend: length="<<p.length/u<<" mm, cuts="<<p.bends.size()
             <<", relief angle="<<p.bends[0].angle*180/pi<<" deg, retained wall continuous, other three walls cut at the bend,"
             <<" source slot at the middle of the arc, whole-arc mode rejected, undo and reopen clean"<<std::endl;
}
int main(int argc,char** argv){try{
    Require(argc==2,"usage: TubeZeroInner <new output folder>");
    std::filesystem::path out=argv[1];Require(!std::filesystem::exists(out),"Output folder exists");
    std::filesystem::create_directories(out);Check(UF_initialize());
    tag_t fixture=ZeroInnerFixture(out/"zero-inner-source.prt");
    Require(fixture!=0,"Fixture not built");
    ZeroInnerCase(out/"zero-inner-source.prt",out/"zero-inner-blank.prt");
    UF_terminate();return 0;
}catch(const NXException& e){std::cerr<<e.Message()<<std::endl;return 1;}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}}
