#include "../BendSimulation.hpp"
#include <NXOpen/Body.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/Edge.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/ModelingView.hxx>
#include <NXOpen/ModelingViewCollection.hxx>
#include <NXOpen/Features_FeatureCollection.hxx>
#include <uf.h>
#include <uf_curve.h>
#include <uf_csys.h>
#include <uf_modl.h>
#include <uf_obj.h>
#include <uf_part.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace bend_sim;
void Require(bool b,const char* text){if(!b)throw std::runtime_error(text);}
template<class F> void Reject(F f,const char* message){bool rejected=false;try{f();}catch(...){rejected=true;}Require(rejected,message);}
Vec Transform(Vec p,bool rotated,double scale){return (rotated?Vec{p.y+31,p.z-17,p.x+83}:p)*scale;}
Vec Direction(Vec p,bool rotated){return rotated?Vec{p.y,p.z,p.x}:p;}
struct Fixture{tag_t body=0,inner=0,outer=0,plane=0;};
Fixture MakeFixture(bool rotated,double scale,bool sharp){
    std::vector<tag_t> curves;
    auto line=[&](Vec a,Vec b){a=Transform(a,rotated,scale);b=Transform(b,rotated,scale);UF_CURVE_line_t l={{a.x,a.y,a.z},{b.x,b.y,b.z}};tag_t t=0;Check(UF_CURVE_create_line(&l,&t));curves.push_back(t);};
    if(sharp){
        std::vector<Vec> points={{0,-2,50},{0,0,50},{0,0,0},{0,50,0},{0,50,-2},{0,-2,-2}};
        for(size_t i=0;i<points.size();++i)line(points[i],points[(i+1)%points.size()]);
    }else{
        Vec x=Direction({0,1,0},rotated),y=Direction({0,0,1},rotated),z=Direction({1,0,0},rotated);double m[]={x.x,x.y,x.z,y.x,y.y,y.z,z.x,z.y,z.z};tag_t matrix=0;Check(UF_CSYS_create_matrix(m,&matrix));
        auto arc=[&](double r){UF_CURVE_arc_t a={};a.matrix_tag=matrix;a.start_angle=3.141592653589793;a.end_angle=4.71238898038469;a.radius=r*scale;Vec c=Transform({},rotated,scale);a.arc_center[0]=Dot(c,x);a.arc_center[1]=Dot(c,y);a.arc_center[2]=Dot(c,z);tag_t t=0;Check(UF_CURVE_create_arc(&a,&t));curves.push_back(t);};
        line({0,-3,50},{0,-1,50});line({0,-1,50},{0,-1,0});arc(1);line({0,0,-1},{0,50,-1});line({0,50,-1},{0,50,-3});line({0,50,-3},{0,0,-3});arc(3);line({0,-3,0},{0,-3,50});
    }
    uf_list_p_t list=nullptr,out=nullptr;Check(UF_MODL_create_list(&list));for(auto t:curves)Check(UF_MODL_put_list_item(list,t));
    char zero[]="0";std::string end=std::to_string(80*scale);char* limits[]={zero,end.data()};Vec direction=Direction({1,0,0},rotated);double p[3]={},d[]={direction.x,direction.y,direction.z};
    Check(UF_MODL_create_extruded1(list,zero,limits,p,d,UF_NULLSIGN,0,&out));Fixture f;tag_t feature=0;Check(UF_MODL_ask_list_item(out,0,&feature));Check(UF_MODL_ask_feat_body(feature,&f.body));UF_MODL_delete_list(&list);UF_MODL_delete_list(&out);for(auto t:curves)Check(UF_OBJ_set_blank_status(t,UF_OBJ_BLANKED));
    auto* body=dynamic_cast<NXOpen::Body*>(NXOpen::NXObjectManager::Get(f.body));
    for(auto* face:body->GetFaces()){
        int type=0,sign=0;double point[3],axis[3],box[6],r=0,r2=0;Check(UF_MODL_ask_face_data(face->Tag(),&type,point,axis,box,&r,&r2,&sign));
        if(type==16&&std::abs(r-scale)<scale*1e-4)f.inner=face->Tag();
        if(type==16&&std::abs(r-3*scale)<scale*1e-4)f.outer=face->Tag();
        if(type==22)f.plane=face->Tag();
    }
    if(sharp)for(auto* e:body->GetEdges()){
        if(e->SolidEdgeType()!=NXOpen::Edge::EdgeTypeLinear)continue;NXOpen::Point3d a,b;e->GetVertices(&a,&b);Vec center={(a.X+b.X)/2,(a.Y+b.Y)/2,(a.Z+b.Z)/2};
        Vec inner=Transform({40,0,0},rotated,scale),outer=Transform({40,-2,-2},rotated,scale);
        if(Dot(center-inner,center-inner)<1e-6*scale*scale)f.inner=e->Tag();
        if(Dot(center-outer,center-outer)<1e-6*scale*scale)f.outer=e->Tag();
    }
    Require(f.inner&&f.outer&&f.plane,"fixture selection discovery failed");return f;
}
double Volume(tag_t body){double a[11]={.999999999},p[47]={},s[13]={};Check(UF_MODL_ask_mass_props_3d(&body,1,1,4,1,1,a,p,s));return p[1];}
size_t FeatureCount(){return NXOpen::Session::GetSession()->Parts()->Work()->Features()->GetFeatures().size();}
void VerifyRedBoundary(const Placement& placement,const Result& result){
    Require(result.status==Status::Interference&&result.interferenceRegions>0&&!result.interferenceLines.empty(),"missing red overlap boundary");
    auto* session=NXOpen::Session::GetSession();auto mark=session->SetUndoMark(NXOpen::Session::MarkVisibilityInvisible,"Verify overlap locations");
    try{
        tag_t tool=CreateToolBody(placement);
        for(const auto& line:result.interferenceLines)for(Vec p:{line.first,line.second}){
            double q[]={p.x,p.y,p.z};int inPart=0,inTool=0;
            Check(UF_MODL_ask_point_containment(q,placement.bend.body,&inPart));Check(UF_MODL_ask_point_containment(q,tool,&inTool));
            Require(inPart!=2&&inTool!=2,"red boundary is outside actual overlap");
        }
    }catch(...){session->UndoToMark(mark,nullptr);session->DeleteUndoMark(mark,nullptr);throw;}
    session->UndoToMark(mark,nullptr);session->DeleteUndoMark(mark,nullptr);
}
void GeometryTest(const std::filesystem::path& path,bool rotated,bool inch,bool sharp){
    double scale=inch?1.0/25.4:1;tag_t part=0;Check(UF_PART_new(path.u8string().c_str(),inch?UF_PART_ENGLISH:UF_PART_METRIC,&part));Fixture f=MakeFixture(rotated,scale,sharp);auto b=Inspect(f.inner,1);
    Require(b.sharp==sharp&&std::abs(b.length-80*scale)<scale*1e-4,"incorrect input mode or bend length");
    Require(std::abs(b.radius-scale)<1e-5*scale&&std::abs(b.unitsPerMm-scale)<1e-9,"radius/units mismatch");
    Require(std::abs(Dot(b.axis,b.up))<1e-8&&std::abs(Dot(b.x,b.up))<1e-8,"invalid placement basis");
    Reject([&]{Inspect(f.outer,1);},"outer radius/edge accepted");Reject([&]{Inspect(f.plane,1);},"plane accepted");
    if(sharp){
        Reject([&]{Inspect(f.inner,-1);},"invalid sharp radius accepted");
        auto zero=Inspect(f.inner,0);Vec actual=Transform({40,0,0},rotated,scale);
        Require(zero.radius==0&&Dot(zero.tip-actual,zero.tip-actual)<1e-10,"automatic sharp placement invents a radius");
    }
    auto* view=NXOpen::Session::GetSession()->Parts()->Work()->ModelingViews()->WorkView();
    Vec z=Unit(Direction({1,1,1},rotated)),x=Unit(Cross(Direction({0,0,1},rotated),z)),y=Cross(z,x);
    view->Orient({x.x,x.y,x.z,y.x,y.y,y.z,z.x,z.y,z.z});view->Fit();
    Check(UF_PART_save());double volume=Volume(f.body);size_t count=FeatureCount();auto tools=BuiltinTools();
    Settings s;s.length=30;s.lift=15;
    auto clear=InspectPlacement(Place(b,tools[0],s),.5);Require(clear.status==Status::Clear&&clear.interferenceLines.empty(),"separated tool reported collision/red boundary");
    if(!sharp){
        s.lift=-.5;auto penetrating=Place(b,tools[0],s);VerifyRedBoundary(penetrating,InspectPlacement(penetrating,.5));
        s.lift=0;auto touch=InspectPlacement(Place(b,tools[0],s),.5);Require(touch.status==Status::Contact&&touch.interferenceLines.empty(),"contact incorrectly displayed as red penetration");
        s.lift=.2;auto near=InspectPlacement(Place(b,tools[0],s),.5);Require(near.status==Status::Near&&std::abs(near.distanceMm-.2)<.02,"clearance distance/category wrong");
        Tool shoulders={"Two separate overlaps",{{0,0},{1,2},{1,10},{50,30},{50,45},{-50,45},{-50,30},{-1,10},{-1,2}}};
        s.lift=5;auto multiple=Place(b,shoulders,s);auto result=InspectPlacement(multiple,.5);
        Require(result.interferenceRegions==2,"did not collect both independent overlaps");VerifyRedBoundary(multiple,result);
        std::cout<<"PASS exact overlap locations, two independent regions, contact has no red lines\n";
    }
    s.lift=5;s.axial=9;s.lateral=2;s.tilt=12;auto placement=Place(b,tools[2],s);s.reverse=true;auto reverse=Place(b,tools[2],s);
    Require(Dot(placement.x,reverse.x)<-.999&&Dot(placement.axis,reverse.axis)<-.999&&Dot(placement.up,reverse.up)>.999,"reverse rotates incorrect axis");
    for(int i=0;i<6;++i){s.reverse=(i%2)==0;InspectPlacement(Place(b,tools[i%3],s),.5);}
    Require(FeatureCount()==count,"temporary tool features leaked");Require(std::abs(Volume(f.body)-volume)<volume*1e-9,"source volume changed");
    auto bad=Place(b,tools[0],s);bad.bend.body=0;Reject([&]{InspectPlacement(bad,.5);},"invalid target accepted");Require(FeatureCount()==count,"failure cleanup leaked features");
    s.length=-1;Reject([&]{Place(b,tools[0],s);},"negative length accepted");
    Require(!NXOpen::Session::GetSession()->Parts()->Work()->IsModified(),"source marked modified after check/rollback");
    std::cout<<"PASS "<<path.filename().u8string()<<" : selection, units, placement, "<<(sharp?"clearance":"interference/contact/gap")<<", repeat and rollback; modified=0\n";
    Check(UF_PART_close(part,0,1));
}
void ProfileTest(const std::filesystem::path& output){
    for(const auto& t:BuiltinTools())ValidateTool(t);
    Tool crossing={"bad",{{0,0},{5,5},{0,5},{5,0}}};Reject([&]{ValidateTool(crossing);},"self intersection accepted");
    Tool duplicate={"bad",{{0,0},{5,5},{0,5},{0,0}}};Reject([&]{ValidateTool(duplicate);},"duplicate point accepted");
    auto path=output/"custom.ztool";std::ofstream(path)<<"ZH_TOOL_V1\nname=TEST\n0,0\n8,8\n8,60\n-8,60\n-8,8\n";Require(ReadTool(path).profile.size()==5,"custom parser failed");
    std::ofstream(path)<<"ZH_TOOL_V1\nname=BAD\n0,0\n1,2junk\n-1,2\n";Reject([&]{ReadTool(path);},"malformed coordinate accepted");std::cout<<"PASS custom tool validation\n";
}
void LibraryTest(const std::filesystem::path& output,const std::filesystem::path& directory,bool inch){
    tag_t part=0;double scale=inch?1.0/25.4:1;
    Check(UF_PART_new((output/(inch?"library-inch.prt":"library-mm.prt")).u8string().c_str(),inch?UF_PART_ENGLISH:UF_PART_METRIC,&part));
    auto fixture=MakeFixture(inch,scale,true);auto bend=Inspect(fixture.inner,1);Check(UF_PART_save());
    auto* session=NXOpen::Session::GetSession();size_t count=FeatureCount();double volume=Volume(fixture.body);int checked=0;
    for(const auto& entry:std::filesystem::directory_iterator(directory)){
        if(entry.path().extension()!=L".ztool")continue;
        Tool tool=ReadTool(entry.path());double twiceArea=0;
        for(size_t i=0;i<tool.profile.size();++i){auto a=tool.profile[i],b=tool.profile[(i+1)%tool.profile.size()];twiceArea+=a.x*b.z-b.x*a.z;}
        for(bool reversed:{false,true}){
            Settings s;s.length=40;s.lift=200;s.reverse=reversed;
            auto placement=Place(bend,tool,s);auto mark=session->SetUndoMark(NXOpen::Session::MarkVisibilityInvisible,"Library test");
            try{
                tag_t body=CreateToolBody(placement);
                // UF mass-property unit 4 returns volume in cubic metres.
                double expected=std::abs(twiceArea)*.5*s.length*1e-9;
                Require(std::abs(Volume(body)-expected)<expected*1e-7,"library extrusion volume mismatch");
                auto result=CheckBodies(fixture.body,body,scale,.5,bend.tolerance);
                Require(result.status==Status::Clear,"raised library tool did not clear fixture");
            }catch(...){std::cerr<<"Library failure "<<entry.path().filename().u8string()<<" units="<<scale<<" tolerance="<<bend.tolerance<<" reverse="<<reversed<<" stage=raised solid\n";session->UndoToMark(mark,nullptr);session->DeleteUndoMark(mark,nullptr);throw;}
            session->UndoToMark(mark,nullptr);session->DeleteUndoMark(mark,nullptr);
            s.lift=0;Result near;
            try{near=InspectPlacement(Place(bend,tool,s),.5);}catch(...){std::cerr<<"Library failure "<<entry.path().filename().u8string()<<" units="<<scale<<" tolerance="<<bend.tolerance<<" reverse="<<reversed<<" stage=near solid\n";throw;}
            Require(std::isfinite(near.distanceMm),"library check returned invalid distance");
            Require(FeatureCount()==count&&std::abs(Volume(fixture.body)-volume)<volume*1e-9,"library test changed fixture");
            double tolerance=0;Check(UF_MODL_ask_distance_tolerance_of_part(part,&tolerance));
            Require(tolerance==bend.tolerance,"library test changed modeling tolerance");
            Require(!session->Parts()->Work()->IsModified(),"library test did not restore saved part state");
        }
        ++checked;std::cout<<"PASS "<<entry.path().filename().u8string()<<" "<<(inch?"rotated inch":"mm")<<": parser, solid volume, forward/reverse, near/clear checks, rollback\n";
    }
    Require(checked==10,"expected ten reviewed DWG tools");
    Settings failureSettings;failureSettings.length=40;
    auto bad=Place(bend,BuiltinTools()[0],failureSettings);
    bad.tool.profile={{0,0},{1,0},{2,0}};
    Reject([&]{InspectPlacement(bad,.5);},"degenerate extrusion accepted");
    double restored=0;Check(UF_MODL_ask_distance_tolerance_of_part(part,&restored));
    Require(restored==bend.tolerance&&FeatureCount()==count&&!session->Parts()->Work()->IsModified(),"failed extrusion did not restore preference/state");
    std::cout<<"PASS failed extrusion restores modeling tolerance and part state\n";
    Check(UF_PART_close(part,0,1));
}
int wmain(int argc,wchar_t** argv){
    try{Require(argc==2||argc==3,"provide a new output folder, optionally a tool library directory");std::filesystem::path out(argv[1]);Require(!std::filesystem::exists(out),"output directory must not exist");std::filesystem::create_directories(out);ProfileTest(out);Check(UF_initialize());
        if(argc==3){LibraryTest(out,argv[2],false);LibraryTest(out,argv[2],true);UF_terminate();return 0;}
        GeometryTest(out/"bend-mm.prt",false,false,false);GeometryTest(out/"bend-rotated.prt",true,false,false);GeometryTest(out/"bend-inch.prt",false,true,false);GeometryTest(out/"sharp-mm.prt",false,false,true);GeometryTest(out/"sharp-rotated.prt",true,false,true);GeometryTest(out/"sharp-inch.prt",false,true,true);UF_terminate();return 0;
    }catch(const NXOpen::NXException& e){std::cerr<<"NX FAIL "<<e.ErrorCode()<<" "<<e.Message()<<'\n';}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';}return 1;
}
