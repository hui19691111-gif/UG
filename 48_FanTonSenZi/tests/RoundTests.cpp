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
#include <uf_curve.h>
#include <uf_modl.h>
#include <uf_obj.h>
#include <uf_part.h>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
using namespace tube_straighten;
using namespace NXOpen;
void Require(bool b,const char* m){if(!b)throw std::runtime_error(m);}
std::string Number(double v){std::ostringstream s;s<<std::setprecision(16)<<v;return s.str();}
struct Fixture{Vec o,x,y,z;double u;Vec At(double a,double b,double c=0)const{return o+x*(a*u)+y*(b*u)+z*(c*u);}};
int Contains(tag_t body,Vec p);
tag_t Corner(const Fixture& f){
    auto cylinder=[&](Vec origin,Vec direction,double length,double diameter){double p[]={origin.x,origin.y,origin.z},d[]={direction.x,direction.y,direction.z};auto h=Number(length*f.u),dia=Number(diameter*f.u);tag_t feature=0,body=0;Check(UF_MODL_create_cyl1(UF_NULLSIGN,p,h.data(),dia.data(),d,&feature));Check(UF_MODL_ask_feat_body(feature,&body));return body;};
    auto pipe=[&](Vec origin,Vec direction,double length){tag_t outer=cylinder(origin,direction,length,50),inner=cylinder(origin-direction*f.u,direction,length+2,46),feature=0;Check(UF_MODL_subtract_bodies_with_retained_options(outer,inner,false,false,&feature));return outer;};
    tag_t a=pipe(f.At(0,0),f.x,300),b=pipe(f.At(250,-50),f.y,230),plane=0;Vec origin=f.At(250,0),normal=f.x+f.y;double p[]={origin.x,origin.y,origin.z},n[]={normal.x,normal.y,normal.z};Check(UF_MODL_create_plane(p,n,&plane));
    auto trim=[&](tag_t body,Vec keep){auto* session=Session::GetSession();auto mark=session->SetUndoMark(Session::MarkVisibilityInvisible,"fixture trim direction");tag_t feature=0;Check(UF_MODL_trim_body(body,plane,0,&feature));if(Contains(body,keep)!=1){session->UndoToMark(mark,nullptr);Check(UF_MODL_trim_body(body,plane,1,&feature));}session->DeleteUndoMark(mark,nullptr);Require(Contains(body,keep)==1,"Fixture trim removed desired side");};
    trim(a,f.At(100,0,24));trim(b,f.At(250,100,24));Check(UF_MODL_unite_bodies(a,b));Check(UF_OBJ_set_blank_status(plane,UF_OBJ_BLANKED));return a;
}
tag_t Tube(const Fixture& f,int shape){
    if(shape==0)return Corner(f);
    std::vector<tag_t> curves;auto line=[&](Vec a,Vec b){UF_CURVE_line_t l={{a.x,a.y,a.z},{b.x,b.y,b.z}};tag_t t=0;Check(UF_CURVE_create_line(&l,&t));curves.push_back(t);};
    if(shape==0){line(f.At(0,0),f.At(250,0));line(f.At(250,0),f.At(250,180));}
    else{double angle=shape==2?pi:pi/2;auto a=f.At(200,0),b=f.At(200*cos(angle/2),200*sin(angle/2)),c=f.At(200*cos(angle),200*sin(angle));
        if(shape!=2)line(f.At(200,-150),a);double aa[]={a.x,a.y,a.z},bb[]={b.x,b.y,b.z},cc[]={c.x,c.y,c.z};tag_t t=0;Check(UF_CURVE_create_arc_thru_3pts(1,aa,bb,cc,&t));curves.push_back(t);if(shape!=2)line(c,f.At(-120,200));
    }
    uf_list_p_t path=nullptr,features=nullptr;Check(UF_MODL_create_list(&path));for(auto t:curves)Check(UF_MODL_put_list_item(path,t));auto outer=Number(50*f.u),inner=Number(46*f.u);char* limits[]={outer.data(),inner.data()};
    Check(UF_MODL_create_tube(path,limits,UF_NULLSIGN,&features));int count=0;Check(UF_MODL_ask_list_count(features,&count));Require(count==1,"Tube fixture produced multiple features");tag_t feat=0,body=0;Check(UF_MODL_ask_list_item(features,0,&feat));Check(UF_MODL_ask_feat_body(feat,&body));UF_MODL_delete_list(&path);UF_MODL_delete_list(&features);for(auto t:curves)Check(UF_OBJ_set_blank_status(t,UF_OBJ_BLANKED));return body;
}
int Contains(tag_t body,Vec p){double q[]={p.x,p.y,p.z};int where=0;Check(UF_MODL_ask_point_containment(q,body,&where));return where;}
double ExpectedVolume(const Plan& p){
    double r=p.source.depth/2,ri=r-p.source.thickness,alpha=p.settings.bridgeWidthMm*p.source.unitsPerMm/(2*r),root=p.source.thickness+p.radius;
    double moment=0;constexpr int nr=40,na=2400;double dr=(r-ri)/nr,da=(2*pi-2*alpha)/na;
    for(int j=0;j<nr;++j){double rho=ri+(j+.5)*dr;for(int i=0;i<na;++i){double angle=alpha+(i+.5)*da;moment+=std::max(0.,r-rho*cos(angle)-root)*rho*dr*da;}}
    double volume=pi*(r*r-ri*ri)*p.length;
    for(const auto& b:p.bends)volume-=(b.allowance+p.gap)*(pi-alpha)*(r*r-ri*ri)+2*std::tan(b.angle/2)*moment;return volume;
}
void Case(const std::string& file,int shape,bool inch=false,bool rotate=false){
    std::cout<<"BEGIN "<<file<<std::endl;tag_t partTag=0;Check(UF_PART_new(file.c_str(),inch?ENGLISH:METRIC,&partTag));
    Fixture f;f.u=inch?1/25.4:1;f.o=rotate?Vec{12,-32,9}:Vec{};f.x=rotate?tube_straighten::Unit({1,2,1}):Vec{1,0,0};f.z=rotate?tube_straighten::Unit({-1,0,1}):Vec{0,0,1};f.y=Cross(f.z,f.x);
    tag_t source=Tube(f,shape);Check(UF_PART_save_as((file+"-input.prt").c_str()));auto* session=Session::GetSession();auto* part=session->Parts()->Work();auto* body=dynamic_cast<Body*>(NXObjectManager::Get(source));Source s;int caps=0;size_t faces=0;
    for(auto* face:body->GetFaces()){auto candidate=InspectFace(face->Tag());Require(candidate.round&&std::abs(candidate.depth-50*f.u)<1e-5*f.u&&std::abs(candidate.thickness-2*f.u)<1e-5*f.u,"Round dimensions mismatch");++faces;if(IsRoundCap(face->Tag())){++caps;s=candidate;}}
    Require(caps==2,"Round tube requires 2 selectable end caps");double volume=Volume(source);auto count=part->Features()->GetFeatures().size();auto mark=session->SetUndoMark(Session::MarkVisibilityVisible,"round tube verification");
    Settings settings;settings.bridgeWidthMm=rotate?12:6;settings.radiusMm=rotate?0:1;auto p=MakePlan(s,settings);Require(p.bends.size()==(shape==0?1:12),"Round cut count mismatch");Require(!Preview(p).empty(),"Missing round preview");
    tag_t result=Create(p);Require(result!=source,"Source replaced");double actual=Volume(result)*1e9*std::pow(f.u,3),expected=ExpectedVolume(p);Require(std::abs(actual-expected)<expected*2e-5,"Round cut volume disagrees with polar integration");
    double r=s.depth/2,ri=r-s.thickness,alpha=settings.bridgeWidthMm*f.u/(2*r);Vec inside=Cross(s.normal,s.spans.front().Tangent(0));
    for(int i=0;i<24;++i){double a=2*pi*(i+.37)/24;Vec q=s.spans.front().a+inside*(r-(r+ri)/2*cos(a))+s.widthDirection*(r+(r+ri)/2*sin(a));Require(Contains(source,q)==3&&Contains(result,q)==3,"Original and flat round end caps do not coincide");}
    for(const auto& b:p.bends)for(double a:{-.9*alpha,0.,.9*alpha,1.2*alpha,pi/2,pi,3*pi/2,2*pi-1.2*alpha})for(double rho:{ri+.1*s.thickness,r-.1*s.thickness}){
        Vec q=FlatPoint(p,b.start+b.allowance/2,r-rho*cos(a),r+rho*sin(a));Require(Contains(result,q)==(std::abs(a)<alpha?1:2),"Full thickness bridge or cut clearance incorrect");
    }
    Require(std::abs(Volume(source)-volume)<volume*1e-10,"Source modified");session->UndoToMark(mark,nullptr);session->DeleteUndoMark(mark,nullptr);Require(part->Features()->GetFeatures().size()==count,"Undo leaked features");
    auto bad=settings;bad.bridgeWidthMm=0;bool rejected=false;try{MakePlan(s,bad);}catch(...){rejected=true;}Require(rejected,"Zero width bridge accepted");bad.bridgeWidthMm=1000;rejected=false;try{MakePlan(s,bad);}catch(...){rejected=true;}Require(rejected,"Oversized bridge accepted");
    // A drilled cylindrical opening must not silently disappear on unfolding.
    auto holeMark=session->SetUndoMark(Session::MarkVisibilityInvisible,"unsupported round hole");Vec center=shape==0?f.At(100,0,-30):f.At(200,-70,-30);double pos[]={center.x,center.y,center.z},axis[]={f.z.x,f.z.y,f.z.z};auto length=Number(60*f.u),diam=Number(8*f.u);tag_t drillFeat=0,drill=0,cut=0;Check(UF_MODL_create_cyl1(UF_NULLSIGN,pos,length.data(),diam.data(),axis,&drillFeat));Check(UF_MODL_ask_feat_body(drillFeat,&drill));
    if(shape!=2){Check(UF_MODL_subtract_bodies_with_retained_options(source,drill,false,false,&cut));rejected=false;try{InspectFace(body->GetFaces().front()->Tag());}catch(...){rejected=true;}Require(rejected,"Round hole was ignored");}
    session->UndoToMark(holeMark,nullptr);session->DeleteUndoMark(holeMark,nullptr);
    Check(UF_PART_save_as((file+"-source.prt").c_str()));Create(p);Check(UF_PART_save_as(file.c_str()));Check(UF_PART_close(partTag,0,1));UF_PART_load_status_t load={};Check(UF_PART_open(file.c_str(),&partTag,&load));UF_PART_free_load_status(&load);int solids=0;for(auto* b:*session->Parts()->Work()->Bodies())if(b->IsSolidBody())++solids;Require(solids==2,"Saved round body count incorrect");Check(UF_PART_close(partTag,0,1));
    std::cout<<"PASS "<<file<<": "<<faces<<" selectable faces, D50 t2, caps aligned, "<<p.bends.size()<<" cuts, full-thickness bridge "<<settings.bridgeWidthMm<<"mm, polar volume, invalid inputs, source/undo/reopen"<<std::endl;
}
int main(int argc,char** argv){try{Require(argc==2,"New output directory required");Require(!std::filesystem::exists(argv[1]),"Output directory exists");std::filesystem::create_directories(argv[1]);Check(UF_initialize());std::string root=argv[1];Case(root+"/round-arc.prt",1);Case(root+"/round-corner.prt",0);Case(root+"/round-arc-inch-rotated.prt",1,true,true);Case(root+"/round-half-arc.prt",2);UF_terminate();return 0;}catch(const NXException& e){std::cerr<<e.Message()<<std::endl;return 1;}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}}
