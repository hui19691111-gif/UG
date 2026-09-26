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
#include <uf_eval.h>
#include <uf_modl.h>
#include <uf_obj.h>
#include <uf_part.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <algorithm>
using namespace tube_straighten;
using namespace NXOpen;
void Require(bool x,const char* m){if(!x)throw std::runtime_error(m);}
struct Fixture {
    Vec o,x,y,z;double u=1;tag_t body=0;std::vector<tag_t> edges;
    Vec At(double a,double b,double c)const{return o+x*(a*u)+y*(b*u)+z*(c*u);}
};
tag_t Shape(const std::vector<Vec>& points,Vec direction,double length){
    uf_list_p_t curves=nullptr,features=nullptr;Check(UF_MODL_create_list(&curves));std::vector<tag_t> tags;
    for(size_t i=0;i<points.size();++i){auto a=points[i],b=points[(i+1)%points.size()];UF_CURVE_line_t line={{a.x,a.y,a.z},{b.x,b.y,b.z}};tag_t t=0;Check(UF_CURVE_create_line(&line,&t));tags.push_back(t);Check(UF_MODL_put_list_item(curves,t));}
    char zero[]="0";std::string end=std::to_string(length);char* limits[]={zero,end.data()};double p[3]={},d[]={direction.x,direction.y,direction.z};
    Check(UF_MODL_create_extruded(curves,zero,limits,p,d,UF_NULLSIGN,&features));tag_t f=0,body=0;Check(UF_MODL_ask_list_item(features,0,&f));Check(UF_MODL_ask_feat_body(f,&body));UF_MODL_delete_list(&features);UF_MODL_delete_list(&curves);
    for(auto t:tags)Check(UF_OBJ_set_blank_status(t,UF_OBJ_BLANKED));return body;
}
tag_t Revolve(const Fixture& f,double r0,double r1,double z0,double z1,double angle){
    std::vector<Vec> points={f.At(r0,0,z0),f.At(r1,0,z0),f.At(r1,0,z1),f.At(r0,0,z1)};std::vector<tag_t> curves;
    for(size_t i=0;i<points.size();++i){auto a=points[i],b=points[(i+1)%points.size()];UF_CURVE_line_t line={{a.x,a.y,a.z},{b.x,b.y,b.z}};tag_t t=0;Check(UF_CURVE_create_line(&line,&t));curves.push_back(t);}
    char zero[]="0";std::string end=std::to_string(angle*180/pi);char* limits[]={zero,end.data()},*offsets[]={zero,zero};double axis[]={f.z.x,f.z.y,f.z.z},origin[]={f.o.x,f.o.y,f.o.z},region[3]={};tag_t* features=nullptr;int count=0;
    Check(UF_MODL_create_revolution(curves.data(),static_cast<int>(curves.size()),nullptr,limits,offsets,region,false,true,origin,axis,UF_NULLSIGN,&features,&count));Require(count==1,"revolve count");tag_t body=0;Check(UF_MODL_ask_feat_body(features[0],&body));UF_free(features);for(auto t:curves)Check(UF_OBJ_set_blank_status(t,UF_OBJ_BLANKED));return body;
}
std::pair<Vec,Vec> Ends(tag_t edge){UF_EVAL_p_t e=nullptr;Check(UF_EVAL_initialize_2(edge,&e));double limits[2],a[3],b[3];Check(UF_EVAL_ask_limits(e,limits));Check(UF_EVAL_evaluate(e,0,limits[0],a,nullptr));Check(UF_EVAL_evaluate(e,0,limits[1],b,nullptr));UF_EVAL_free(e);return {{a[0],a[1],a[2]},{b[0],b[1],b[2]}};}
Fixture MakeFixture(bool arc,bool inches,bool rotate,double angle=pi/2){
    Fixture f;f.u=inches?1/25.4:1;f.o=rotate?Vec{12,-32,9}:Vec{};f.x=rotate?tube_straighten::Unit({1,2,1}):Vec{1,0,0};f.z=rotate?tube_straighten::Unit({-1,0,1}):Vec{0,0,1};f.y=Cross(f.z,f.x);
    tag_t hole=0;std::vector<std::pair<Vec,Vec>> path;
    if(arc){f.body=Revolve(f,170,200,0,40,angle);hole=Revolve(f,172,198,2,38,angle);path.push_back({f.At(200,0,0),f.At(200*cos(angle),200*sin(angle),0)});}
    else{
        f.body=Shape({f.At(0,0,0),f.At(200,0,0),f.At(200,150,0),f.At(170,150,0),f.At(170,30,0),f.At(0,30,0)},f.z,40*f.u);
        hole=Shape({f.At(0,2,2),f.At(198,2,2),f.At(198,150,2),f.At(172,150,2),f.At(172,28,2),f.At(0,28,2)},f.z,36*f.u);
        path={{f.At(0,0,0),f.At(200,0,0)},{f.At(200,0,0),f.At(200,150,0)}};
    }
    tag_t feature=0;Check(UF_MODL_subtract_bodies_with_retained_options(f.body,hole,false,false,&feature));
    auto* body=dynamic_cast<Body*>(NXObjectManager::Get(f.body));for(auto pair:path){bool found=false;for(auto* edge:body->GetEdges()){auto ep=Ends(edge->Tag());if((Length(ep.first-pair.first)+Length(ep.second-pair.second)<1e-5*f.u)||(Length(ep.first-pair.second)+Length(ep.second-pair.first)<1e-5*f.u)){f.edges.push_back(edge->Tag());found=true;break;}}Require(found,"fixture path missing");}
    return f;
}
void Case(const std::string& file,bool arc,bool inches,bool rotate,double angle=pi/2,bool holes=false){
    tag_t partTag=0;Check(UF_PART_new(file.c_str(),inches?ENGLISH:METRIC,&partTag));auto f=MakeFixture(arc,inches,rotate,angle);auto* session=Session::GetSession();auto* part=session->Parts()->Work();
    if(holes){tag_t feature=0;auto slot=Shape({f.At(70,10,-1),f.At(100,10,-1),f.At(100,20,-1),f.At(70,20,-1)},f.z,4*f.u);Check(UF_MODL_subtract_bodies_with_retained_options(f.body,slot,false,false,&feature));
        auto side=Shape({f.At(120,-1,15),f.At(132,-1,15),f.At(132,-1,25),f.At(120,-1,25)},f.y,4*f.u);Check(UF_MODL_subtract_bodies_with_retained_options(f.body,side,false,false,&feature));}
    std::reverse(f.edges.begin(),f.edges.end());Source s=Inspect(f.edges);
    int selectedFaces=0,planarFaces=0;for(auto* face:dynamic_cast<Body*>(NXObjectManager::Get(f.body))->GetFaces()){if(face->SolidFaceType()!=Face::FaceTypePlanar)continue;++planarFaces;try{auto faceSource=InspectFace(face->Tag());Require(std::abs(faceSource.width-s.width)<1e-6*f.u,"face width mismatch");++selectedFaces;}catch(const std::exception&){}}
    Require(selectedFaces==planarFaces,"every planar face must recognize the complete tube");Require(s.holes.size()==(holes?2:0),"lost or invented holes");Require(std::abs(s.thickness-2*f.u)<1e-5*f.u&&std::abs(s.width-40*f.u)<1e-5*f.u&&std::abs(s.depth-30*f.u)<1e-5*f.u,"automatic dimensions mismatch");
    Settings settings;settings.divisions=12;auto p=MakePlan(s,settings);double volume=Volume(f.body);auto count=part->Features()->GetFeatures().size();
    auto mark=session->SetUndoMark(Session::MarkVisibilityVisible,"tube test");tag_t out=Create(p);Require(out!=f.body&&Volume(out)>0,"output invalid");Require(std::abs(Volume(f.body)-volume)<volume*1e-10,"source modified");
    double area=s.width*s.depth-(s.width-2*s.thickness)*(s.depth-2*s.thickness);double expected=area*p.length;
    // Integrate each removed profile through two side walls and the far wall.
    for(auto b:p.bends){double base=b.allowance+p.gap,tanHalf=tan(b.angle/2),root=s.thickness+p.radius,d=s.depth,t=s.thickness;
        auto integral=[&](double lo,double hi){return base*(hi-lo)+tanHalf*(std::pow(std::max(0.,hi-root),2)-std::pow(std::max(0.,lo-root),2));};
        expected-=2*t*integral(t,d)+(s.width-2*t)*integral(d-t,d);
    }
    for(const auto& h:s.holes)expected-=h.area*h.length;
    double actual=Volume(out)*1e9*std::pow(f.u,3);Require(std::abs(actual-expected)<expected*1e-6,"cut volume disagrees with independent integration");
    session->UndoToMark(mark,nullptr);session->DeleteUndoMark(mark,nullptr);Require(part->Features()->GetFeatures().size()==count,"undo leaked features");Require(std::abs(Volume(f.body)-volume)<volume*1e-10,"undo source changed");
    int rejected=0;auto duplicate=f.edges;duplicate.push_back(f.edges[0]);try{Inspect(duplicate);}catch(...){++rejected;}auto bad=settings;bad.gapMm=10000;try{MakePlan(s,bad);}catch(...){++rejected;}Require(rejected==2,"invalid inputs accepted");
    Check(UF_PART_save_as((file+"-source.prt").c_str()));Create(p);Check(UF_PART_save_as(file.c_str()));Check(UF_PART_close(partTag,0,1));UF_PART_load_status_t status={};Check(UF_PART_open(file.c_str(),&partTag,&status));UF_PART_free_load_status(&status);
    int solids=0;for(auto* body:*session->Parts()->Work()->Bodies())if(body->IsSolidBody())++solids;Require(solids==2,"saved body count");Check(UF_PART_close(partTag,0,1));
    std::cout<<"PASS "<<file<<" : recognition, stock removal volume, retained wall, source preserved, undo, invalid inputs, reopen\n"<<std::flush;
}
int main(int argc,char** argv){try{
    Require(argc==2,"Provide a new output directory");Require(!std::filesystem::exists(argv[1]),"Output directory already exists");std::filesystem::create_directories(argv[1]);Check(UF_initialize());std::string root=argv[1];
    Case(root+"/corner.prt",false,false,false);Case(root+"/corner-inch-rotated.prt",false,true,true);Case(root+"/arc.prt",true,false,false);Case(root+"/arc-inch-rotated.prt",true,true,true);Case(root+"/half-arc.prt",true,false,false,pi);Case(root+"/corner-slots.prt",false,false,false,pi/2,true);Case(root+"/corner-slots-inch.prt",false,true,true,pi/2,true);UF_terminate();return 0;
}catch(const NXException& e){std::cerr<<e.Message()<<'\n';return 1;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
