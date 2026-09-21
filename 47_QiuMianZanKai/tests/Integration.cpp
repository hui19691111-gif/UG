// Runs in an independent NX process; never opens or saves a user part.
#include "../SphereGeometry.hpp"
#include "../SphereCustomFeature.hpp"
#include <NXOpen/Body.hxx>
#include <NXOpen/BodyCollection.hxx>
#include <NXOpen/Curve.hxx>
#include <NXOpen/CurveCollection.hxx>
#include <NXOpen/CurveFeatureRule.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/Features_Extrude.hxx>
#include <NXOpen/Features_ExtrudeBuilder.hxx>
#include <NXOpen/Features_FeatureCollection.hxx>
#include <NXOpen/Features_FeatureGroup.hxx>
#include <NXOpen/Features_CustomFeature.hxx>
#include <NXOpen/Features_ConstructionFeatureData.hxx>
#include <NXOpen/Features_SketchFeature.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/Section.hxx>
#include <NXOpen/SectionData.hxx>
#include <NXOpen/Sketch.hxx>
#include <NXOpen/SketchCollection.hxx>
#include <uf.h>
#include <uf_curve.h>
#include <uf_modl.h>
#include <uf_obj.h>
#include <uf_part.h>
#include <filesystem>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <vector>
using namespace sphere_unfold;
using namespace NXOpen;
void Require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
struct Fixture{tag_t body=0,cylinder=0,sphere=0,innerSphere=0,innerCylinder=0;};
Fixture MakeFixture(bool rotated,bool inches,double sweep,double latitude,double thickness=2){
    double u=inches?1/25.4:1;Vec origin=rotated?Vec{30*u,-45*u,75*u}:Vec{},x=rotated?sphere_unfold::Unit({1,2,1}):Vec{1,0,0},z=rotated?sphere_unfold::Unit({-1,0,1}):Vec{0,0,1};Vec y=Cross(z,x);
    auto at=[&](double r,double h){return origin+x*(r*u)+z*(h*u);};std::vector<tag_t> curves;
    auto line=[&](double r,double h,double r2,double h2){auto a=at(r,h),b=at(r2,h2);UF_CURVE_line_t l={{a.x,a.y,a.z},{b.x,b.y,b.z}};tag_t t=0;Check(UF_CURVE_create_line(&l,&t));curves.push_back(t);};
    auto arc=[&](double r){auto a=at(r,0),b=at(r*cos(latitude/2),r*sin(latitude/2)),c=at(r*cos(latitude),r*sin(latitude));double p[]={a.x,a.y,a.z},q[]={b.x,b.y,b.z},v[]={c.x,c.y,c.z};tag_t t=0;Check(UF_CURVE_create_arc_thru_3pts(1,p,q,v,&t));curves.push_back(t);};
    double inner=100-thickness;
    line(100,-40,100,0);arc(100);line(100*cos(latitude),100*sin(latitude),inner*cos(latitude),inner*sin(latitude));arc(inner);line(inner,0,inner,-40);line(inner,-40,100,-40);
    char zero[]="0";std::string angle=std::to_string(sweep*180/pi);char* limits[]={zero,angle.data()},*offsets[]={zero,zero};double point[]={origin.x,origin.y,origin.z},axis[]={z.x,z.y,z.z},region[3]={};tag_t* features=nullptr;int count=0;
    Check(UF_MODL_create_revolution(curves.data(),static_cast<int>(curves.size()),nullptr,limits,offsets,region,false,true,point,axis,UF_NULLSIGN,&features,&count));
    Require(count==1,"fixture revolve count");Fixture f;Check(UF_MODL_ask_feat_body(features[0],&f.body));UF_free(features);
    for(auto t:curves)Check(UF_OBJ_set_blank_status(t,UF_OBJ_BLANKED));
    auto* body=dynamic_cast<Body*>(NXObjectManager::Get(f.body));
    for(auto* face:body->GetFaces()){int type,sign;double p[3],d[3],box[6],r,r2;Check(UF_MODL_ask_face_data(face->Tag(),&type,p,d,box,&r,&r2,&sign));if(type==16&&std::abs(r-100*u)<1e-6*u)f.cylinder=face->Tag();if(type==18&&std::abs(r-100*u)<1e-6*u)f.sphere=face->Tag();if(type==18&&std::abs(r-inner*u)<1e-6*u)f.innerSphere=face->Tag();}
    for(auto* face:body->GetFaces()){int type,sign;double p[3],d[3],box[6],r,r2;Check(UF_MODL_ask_face_data(face->Tag(),&type,p,d,box,&r,&r2,&sign));if(type==16&&std::abs(r-inner*u)<1e-6*u)f.innerCylinder=face->Tag();}
    Require(f.cylinder&&f.sphere&&f.innerSphere&&f.innerCylinder,"fixture analytic faces missing");return f;
}
double Volume(tag_t body){double acc[11]={.99999999},props[47]={},stats[13]={};Check(UF_MODL_ask_mass_props_3d(&body,1,1,4,1,1,acc,props,stats));return props[1];}
int LooseCurves(Part* part){int count=0;for(auto* curve:*part->Curves())if(!part->Sketches()->GetOwningSketch(curve))++count;return count;}
void CheckProfiles(Part* part,int petals,int originalLooseCurves){
    Require(LooseCurves(part)==originalLooseCurves,"generated loose profile curves");int sketches=0,extrusions=0;std::map<tag_t,int> owners;
    for(auto* feature:part->Features()->GetFeatures()){
        if(auto* sketch=dynamic_cast<Features::SketchFeature*>(feature)){
            ++sketches;Require(sketch->IsInternal(),"profile sketch is external");Require(sketch->Sketch()->IsBlanked(),"profile sketch is visible");
        }
        if(auto* extrusion=dynamic_cast<Features::Extrude*>(feature)){
            // Internal sketch ownership is stored in the section rule; NX omits
            // these embedded sketches from the normal feature-parent graph.
            ++extrusions;std::set<tag_t> profiles;auto* edit=part->Features()->CreateExtrudeBuilder(extrusion);
            try{
                std::vector<SectionData*> data;edit->Section()->GetSectionData(data);
                for(auto* item:data){std::vector<SelectionIntentRule*> rules;item->GetRules(rules);
                    for(auto* rule:rules)if(auto* curveRule=dynamic_cast<CurveFeatureRule*>(rule)){
                        std::vector<Features::Feature*> inputs;curveRule->GetData(inputs);
                        for(auto* input:inputs)if(dynamic_cast<Features::SketchFeature*>(input)&&input->IsInternal())profiles.insert(input->Tag());
                    }
                    delete item;
                }
                edit->Destroy();
            }catch(...){edit->Destroy();throw;}
            Require(profiles.size()==1,"extrusion has no internal profile");++owners[*profiles.begin()];
        }
        if(auto* group=dynamic_cast<Features::FeatureGroup*>(feature)){
            std::vector<Features::Feature*> members;group->GetMembers(members);
            for(auto* member:members)Require(!member->IsInternal(),"group detached an internal sketch from extrusion");
        }
    }
    Require(sketches==3*petals&&extrusions==sketches,"missing sketch/extrusion history");
    Require(owners.size()==static_cast<size_t>(sketches),"unreferenced internal sketch");for(auto owner:owners)Require(owner.second==1,"profile is shared between extrusions");
}
void Case(const std::string& path,bool rotated,bool inches,double sweep,double latitude,bool inner=false,bool flat=true,int petals=0,double thickness=2,double gap=.5,double relief=4){
    tag_t partTag=0;Check(UF_PART_new(path.c_str(),inches?ENGLISH:METRIC,&partTag));auto* session=Session::GetSession();auto* part=session->Parts()->Work();
    auto f=MakeFixture(rotated,inches,sweep,latitude,thickness);Source source=Inspect(inner?f.innerCylinder:f.cylinder,inner?f.innerSphere:f.sphere);double u=inches?1/25.4:1;
    Require(std::abs(source.radius/u-(inner?100-thickness:100))<1e-7&&std::abs(source.height/u-40)<1e-7,"radius/height/units mismatch");
    Require(std::abs(source.thickness/u-thickness)<1e-7&&source.innerSurface==inner,"automatic thickness/side mismatch");
    auto opposite=Inspect(inner?f.cylinder:f.innerCylinder,inner?f.sphere:f.innerSphere);
    Require(std::abs(opposite.thickness-source.thickness)<1e-7*u&&opposite.innerSurface!=inner,"opposite wall recognition mismatch");
    Require(std::abs(source.sweep-sweep)<1e-6&&std::abs(source.latitude-latitude)<1e-6,"angular extent mismatch");
    Settings settings;settings.petals=petals?petals:static_cast<int>(std::round(sweep/(pi/16)));settings.gap=gap;settings.relief=relief;settings.flat=flat;settings.hideSource=true;
    auto plan=MakePlan(source,settings);double before=Volume(f.body);auto featureCount=part->Features()->GetFeatures().size();int originalLooseCurves=LooseCurves(part);
    auto mark=session->SetUndoMark(Session::MarkVisibilityVisible,"test create");Result result;
    try{result=CreateFeature(plan,nullptr,mark);}catch(...){UF_PART_save_as((path+"-failed.prt").c_str());throw;}
    Require(result.body&&result.convert&&result.feature&&(flat?result.flatBody!=0:result.flatBody==0),"missing output");
    auto* custom=dynamic_cast<Features::CustomFeature*>(NXObjectManager::Get(result.feature));Require(custom!=nullptr,"not a custom feature");
    Require(custom->GetConstructionFeatures().size()==result.members.size(),"custom construction ownership missing");
    Require(std::string(custom->Name().GetUTF8Text())=="球面展开_"+std::to_string(settings.petals)+"瓣","feature name encoding mismatch");
    Require(std::abs(Volume(f.body)-before)<before*1e-8,"source changed");if(flat)Require(Volume(result.flatBody)>0,"flat volume zero");
    try{CheckProfiles(part,settings.petals,originalLooseCurves);}catch(...){UF_PART_save_as((path+"-profiles-failed.prt").c_str());throw;}
    // Open and recommit an extrusion to check that the stored internal section is editable.
    for(auto* feature:part->Features()->GetFeatures())if(auto* extrusion=dynamic_cast<Features::Extrude*>(feature)){
        auto* edit=part->Features()->CreateExtrudeBuilder(extrusion);
        try{edit->CommitFeature();edit->Destroy();}catch(...){edit->Destroy();throw;}break;
    }
    CheckProfiles(part,settings.petals,originalLooseCurves);
    // At an interior latitude each requested petal has a separate cross-section.
    // Check center material and the radial slits against the actual NX solid.
    for(int i=0;i<settings.petals;++i){
        double theta=(i+.5)*plan.step,phi=latitude*.5,r=(plan.inner+plan.outer)/2;
        Vec q=source.center+(source.x*cos(theta)+source.y*sin(theta))*(r*cos(phi))+source.z*(r*sin(phi));double point[]={q.x,q.y,q.z};int contains=0;Check(UF_MODL_ask_point_containment(point,result.body,&contains));Require(contains==1,"missing petal material");
        if(i>0){theta=i*plan.step;Vec radial=source.x*cos(theta)+source.y*sin(theta);q=source.center+radial*(r*cos(phi)/cos(plan.step/2))+source.z*(r*sin(phi));double slitPoint[]={q.x,q.y,q.z};Check(UF_MODL_ask_point_containment(slitPoint,result.body,&contains));Require(contains==2,"petal slit obstructed");}
    }
    if(path.find("profiles.prt")!=std::string::npos||path.find("quarter-mm.prt")!=std::string::npos){
        auto editMark=session->SetUndoMark(Session::MarkVisibilityVisible,"test custom edit");auto changed=settings;changed.petals+=2;changed.flat=!flat;
        auto updated=CreateFeature(MakePlan(source,changed),custom,editMark);Require(updated.feature==result.feature,"edit changed custom identity");
        Require(custom->GetConstructionFeatures().size()==updated.members.size(),"edited construction ownership missing");
        tag_t c=0,s=0;auto saved=ReadFeature(custom,c,s);std::cout<<"EDIT settings petals="<<saved.petals<<" expected="<<changed.petals<<" flat="<<saved.flat<<" expected="<<changed.flat<<" sources="<<c<<","<<s<<" expected="<<source.cylinder<<","<<source.sphere<<'\n'<<std::flush;Require(saved.petals==changed.petals&&saved.flat==changed.flat&&c==source.cylinder&&s==source.sphere,"edit settings not persisted");
        CheckProfiles(part,changed.petals,originalLooseCurves);
        int solids=0;for(auto* body:*part->Bodies())if(body->IsSolidBody())++solids;Require(solids==(changed.flat?3:2),"edit leaked previous bodies");
        session->UndoToMark(editMark,nullptr);session->DeleteUndoMark(editMark,nullptr);CheckProfiles(part,settings.petals,originalLooseCurves);
    }
    session->UndoToMark(mark,nullptr);session->DeleteUndoMark(mark,nullptr);
    Require(part->Features()->GetFeatures().size()==featureCount,"undo leaked features");Require(std::abs(Volume(f.body)-before)<before*1e-8,"undo changed source");Require(LooseCurves(part)==originalLooseCurves,"undo leaked curves");
    int rejected=0;try{Inspect(f.sphere,f.cylinder);}catch(...){++rejected;}try{Inspect(f.cylinder,f.innerSphere);}catch(...){++rejected;}
    auto bad=settings;bad.gap=1000;try{MakePlan(source,bad);}catch(...){++rejected;}bad=settings;bad.petals=1;try{MakePlan(source,bad);}catch(...){++rejected;}bad=settings;bad.relief=40;try{MakePlan(source,bad);}catch(...){++rejected;}
    bad=settings;bad.relief=bad.gap*.5;try{MakePlan(source,bad);}catch(...){++rejected;}
    Require(rejected==6,"invalid selection/settings accepted");Require(part->Features()->GetFeatures().size()==featureCount,"validation changed model");
    Check(UF_PART_save_as((path+"-source.prt").c_str()));auto saveMark=session->SetUndoMark(Session::MarkVisibilityVisible,"save custom");CreateFeature(plan,nullptr,saveMark);Check(UF_PART_save_as(path.c_str()));
    Check(UF_PART_close(partTag,0,1));UF_PART_load_status_t status={};Check(UF_PART_open(path.c_str(),&partTag,&status));UF_PART_free_load_status(&status);
    int solids=0;for(auto* body:*session->Parts()->Work()->Bodies())if(body->IsSolidBody())++solids;Require(solids==(flat?3:2),"saved output body count");CheckProfiles(session->Parts()->Work(),settings.petals,originalLooseCurves);Check(UF_PART_close(partTag,0,1));
    std::cout<<"PASS "<<path<<" : auto thickness/side, custom feature, native flatten, internal profiles, no loose curves, material/slits, source unchanged, undo, invalid inputs, reopen\n"<<std::flush;
}
int main(int argc,char** argv){
    try{Require(argc==2||argc==3,"Provide new output directory and optional root-relief or shallow-relief mode");std::filesystem::create_directories(argv[1]);Check(UF_initialize());std::string out=argv[1];
        if(argc==3&&std::string(argv[2])=="shallow-relief"){
            Case(out+"/equal-gap-thick.prt",false,false,pi/2,1.2,false,true,12,5,.5,.5);
            Case(out+"/below-thickness.prt",false,false,pi,1.2,false,true,8,2,.5,1);
            Case(out+"/equal-gap-inch.prt",true,true,pi/2,1.2,false,true,12,5,.5,.5);
            Case(out+"/minimum-gap.prt",false,false,pi/2,1.2,false,true,12,1,.05,.05);
            UF_terminate();return 0;
        }
        if(argc==3&&std::string(argv[2])=="profiles"){
            Case(out+"/profiles.prt",false,false,pi/2,1.2);
            UF_terminate();return 0;
        }
        if(argc==3){
            Require(std::string(argv[2])=="root-relief","unknown test mode");
            Case(out+"/thick-quarter.prt",false,false,pi/2,1.2,false,true,12,5,.5,6);
            Case(out+"/few-half.prt",false,false,pi,1.2,false,true,8,2,.5,4);
            Case(out+"/full-default.prt",false,false,2*pi,1.2,false,true,12,1,.5,3);
            Case(out+"/narrow-gap.prt",false,false,pi/2,1.2,false,true,12,1,.05,3);
            Case(out+"/relief-inch.prt",true,true,pi/2,1.2,false,true,12,5,.5,6);
            UF_terminate();return 0;
        }
        Case(out+"/quarter-mm.prt",false,false,pi/2,1.2);
        Case(out+"/quarter-rotated.prt",true,false,pi/2,1.2);
        Case(out+"/quarter-inch.prt",true,true,pi/2,1.2);
        Case(out+"/hemisphere.prt",false,false,pi/2,pi/2);
        Case(out+"/half-ring.prt",false,false,pi,1.2);
        Case(out+"/inner-reference.prt",false,false,pi/2,1.2,true);
        Case(out+"/folded-only.prt",false,false,pi/2,1.2,false,false);
        Case(out+"/full-ring.prt",false,false,2*pi,1.2);
        UF_terminate();return 0;
    }catch(const NXException& e){std::cerr<<"NX FAIL "<<e.ErrorCode()<<" "<<e.Message()<<'\n';}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';}return 1;
}
