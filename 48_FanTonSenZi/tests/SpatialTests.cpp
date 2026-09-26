#include "../TubeCustomFeature.hpp"
#include <NXOpen/Body.hxx>
#include <NXOpen/BodyCollection.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/Features_CustomFeature.hxx>
#include <NXOpen/Features_FeatureCollection.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/NXException.hxx>
#include <uf.h>
#include <uf_curve.h>
#include <uf_modl.h>
#include <uf_part.h>
#include <uf_obj.h>
#include <filesystem>
#include <iostream>
#include <algorithm>
using namespace tube_straighten;using namespace NXOpen;
void Require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int Contains(tag_t body,Vec q){double xyz[]={q.x,q.y,q.z};int status=0;Check(UF_MODL_ask_point_containment(xyz,body,&status));return status;}
int Solids(Part* p){int n=0;for(auto* b:*p->Bodies())if(b->IsSolidBody())++n;return n;}
tag_t Sweep(const Span& span,double d,double w,double t,bool rounded){
    std::vector<Vec> points;for(auto yz:std::vector<Vec>{{-d/2+t,-w/2+t,0},{d/2-t,-w/2+t,0},{d/2-t,w/2-t,0},{-d/2+t,w/2-t,0}})points.push_back(span.a+span.frameY*yz.x+span.frameZ*yz.y);
    std::vector<tag_t> curves;auto line=[&](Vec a,Vec b){UF_CURVE_line_t definition={{a.x,a.y,a.z},{b.x,b.y,b.z}};tag_t tag=0;Check(UF_CURVE_create_line(&definition,&tag));curves.push_back(tag);};
    if(!rounded)for(size_t i=0;i<4;++i)line(points[i],points[(i+1)%4]);
    else{double a=d-2*t,b=w-2*t,r=3-t,k=r/std::sqrt(2.);auto at=[&](double y,double z){return span.a+span.frameY*(y-d/2+t)+span.frameZ*(z-w/2+t);};auto arc=[&](Vec a,Vec b,Vec c){double x[]={a.x,a.y,a.z},y[]={b.x,b.y,b.z},z[]={c.x,c.y,c.z};tag_t tag=0;Check(UF_CURVE_create_arc_thru_3pts(1,x,y,z,&tag));curves.push_back(tag);};
        line(at(r,0),at(a-r,0));arc(at(a-r,0),at(a-r+k,r-k),at(a,r));line(at(a,r),at(a,b-r));arc(at(a,b-r),at(a-r+k,b-r+k),at(a-r,b));line(at(a-r,b),at(r,b));arc(at(r,b),at(r-k,b-r+k),at(0,b-r));line(at(0,b-r),at(0,r));arc(at(0,r),at(r-k,r-k),at(r,0));}
    char zero[]="0";std::string limit=std::to_string(span.radius?span.angle*180/pi:Length(span.b-span.a));char* limits[]={zero,limit.data()},*offsets[]={zero,zero};tag_t body=0;
    if(span.radius){tag_t* features=nullptr;int count=0;double origin[]={span.center.x,span.center.y,span.center.z},axis[]={span.normal.x,span.normal.y,span.normal.z},region[3]={};Check(UF_MODL_create_revolution(curves.data(),static_cast<int>(curves.size()),nullptr,limits,offsets,region,false,true,origin,axis,UF_NULLSIGN,&features,&count));Require(count==1,"fixture revolution count");Check(UF_MODL_ask_feat_body(features[0],&body));UF_free(features);}
    else{uf_list_p_t list=nullptr,features=nullptr;Check(UF_MODL_create_list(&list));for(auto tag:curves)Check(UF_MODL_put_list_item(list,tag));auto tangent=span.Tangent(0);double origin[3]={},axis[]={tangent.x,tangent.y,tangent.z};Check(UF_MODL_create_extruded(list,zero,limits,origin,axis,UF_NULLSIGN,&features));tag_t feature=0;Check(UF_MODL_ask_list_item(features,0,&feature));Check(UF_MODL_ask_feat_body(feature,&body));UF_MODL_delete_list(&features);UF_MODL_delete_list(&list);}
    for(auto tag:curves)Check(UF_OBJ_set_blank_status(tag,UF_OBJ_BLANKED));return body;
}
std::vector<Span> Path(bool space){
    std::vector<Span> path;Vec position={-100,0,0},axis={1,0,0},y={0,1,0},z={0,0,1};
    auto line=[&](double length){Span s;s.a=position;s.b=position+axis*length;s.frameY=y;s.frameZ=z;path.push_back(s);position=s.b;};
    auto arc=[&](Vec normal,double radius){Span s;s.a=position;s.center=position+Cross(normal,axis)*radius;s.normal=normal;s.radius=radius;s.angle=pi/2;s.frameY=y;s.frameZ=z;s.b=s.Point(1);path.push_back(s);position=s.b;axis=s.Tangent(1);y=Rotate(y,normal,pi/2);z=Rotate(z,normal,pi/2);};
    line(100);arc({0,0,1},100);line(100);
    if(space){arc({1,0,0},120);line(100);arc({0,1,0},80);line(100);arc({0,0,1},110);line(100);}
    else{arc({0,0,-1},160);line(140);}return path;
}
void Case(const std::filesystem::path& directory,bool space,bool rounded=false){
    auto path=Path(space);std::string name=space?"spatial":"reverse";if(rounded){name+="-rounded-rotated";Vec n=tube_straighten::Unit({1,2,3}),offset={50,-23,40};for(auto& span:path){span.a=Rotate(span.a,n,.63)+offset;span.b=Rotate(span.b,n,.63)+offset;span.center=Rotate(span.center,n,.63)+offset;span.normal=Rotate(span.normal,n,.63);span.frameY=Rotate(span.frameY,n,.63);span.frameZ=Rotate(span.frameZ,n,.63);}}
    tag_t partTag=0;Check(UF_PART_new((directory/(name+"-source.prt")).string().c_str(),METRIC,&partTag));
    tag_t body=0,bore=0;for(const auto& span:path){auto outer=Sweep(span,20,30,0,rounded),inner=Sweep(span,20,30,2,rounded);if(!body){body=outer;bore=inner;}else{Check(UF_MODL_unite_bodies(body,outer));Check(UF_MODL_unite_bodies(bore,inner));}}
    tag_t feature=0;Check(UF_MODL_subtract_bodies_with_retained_options(body,bore,false,false,&feature));
    for(size_t i:{size_t(0),size_t(2),path.size()-1}){const auto& span=path[i];Vec p=(span.a+span.b)*.5-span.frameZ*16;double xyz[]={p.x,p.y,p.z},dir[]={span.frameZ.x,span.frameZ.y,span.frameZ.z};char length[]="32",diameter[]="4";tag_t drill=0;Check(UF_MODL_create_cyl1(UF_NULLSIGN,xyz,length,diameter,dir,&feature));Check(UF_MODL_ask_feat_body(feature,&drill));Check(UF_MODL_subtract_bodies_with_retained_options(body,drill,false,false,&feature));}
    Check(UF_OBJ_set_color(body,36));Check(UF_PART_save());auto* session=Session::GetSession();auto* part=session->Parts()->Work();auto initialCount=part->Features()->GetFeatures().size();double volume=Volume(body);
    std::vector<tag_t> selected;Source source;for(auto* face:dynamic_cast<Body*>(NXObjectManager::Get(body))->GetFaces())if(face->SolidFaceType()==Face::FaceTypePlanar){source=InspectFace(face->Tag());Require(source.holes.size()==6,"any-plane recognition lost wall openings");selected.push_back(face->Tag());}
    Require(!selected.empty()&&source.spatial==space,"wrong path classification");std::cout<<name<<" accepted planar faces="<<selected.size()<<" spans="<<source.spans.size()<<" holes="<<source.holes.size()<<std::endl;
    const auto& fixed=path[2];Vec fixedCenter=fixed.a+(fixed.b-fixed.a)*.35;Settings settings;settings.divisions=6;settings.useAnchor=true;settings.anchorPoint=fixedCenter+fixed.frameY*10;
    for(int mode=0;mode<3;++mode){settings.segmentArcs=mode!=0;settings.cutSource=mode==2;auto p=MakePlan(source,settings);
        if(mode==0){double expected=0;for(const auto& span:path)expected+=span.radius?span.radius*span.angle:Length(span.b-span.a);Require(std::abs(p.length-expected)<1e-5,"spatial/negative centerline length wrong");}
        else{
            Require(p.bends.size()==(space?24:12),"wrong spatial cut count");int walls[4]={};for(const auto& b:p.bends)++walls[(b.acrossZ?2:0)+(b.reversed?1:0)];if(space)for(int n:walls)Require(n==6,"notches did not alternate across all four walls");
            auto zero=settings;zero.cutSource=false;zero.gapMm=0;auto folded=MakePlan(source,zero);for(size_t i=0;i<folded.bends.size();++i){const auto& b=folded.bends[i];double depth=BendDepth(folded,b),y=b.reversed?1.:depth-1,spread=(depth-1-folded.radius-source.thickness)*tan(b.angle/2),breadth=b.acrossZ?source.depth:source.width;Vec a={b.start-spread,b.acrossZ?breadth/2:y,b.acrossZ?y:breadth/2},c=a;c.x=b.start+b.allowance+spread;Require(Length(ToFolded(folded,i,a)-ToFolded(folded,i+1,c))<1e-6,"3D cut faces do not meet after inverse folding");}
        }
        auto mark=session->SetUndoMark(Session::MarkVisibilityInvisible,"spatial verification");auto flat=Create(p);Require(Solids(part)==2,"spatial create body count");
        for(Vec q:std::vector<Vec>{fixedCenter+fixed.frameY*9,fixedCenter-fixed.frameY*9,fixedCenter+fixed.frameZ*14,fixedCenter-fixed.frameZ*14})Require(Contains(flat,q)==1&&Contains(body,q)==1,"selected fixed straight section moved");
        for(size_t i=0;i<source.holes.size();++i){auto hole=FlatHole(p,i);for(size_t j=0;j<hole.profile.size();++j)for(double f:{0.,.25,.5,.75})Require(Length(ToFolded(p,p.holeSegments[i],hole.profile[j].Point(f))-source.holes[i].profile[j].Point(f))<1e-7,"spatial hole moved on inverse fold");}
        Require(mode==2?Volume(body)<volume:std::abs(Volume(body)-volume)<volume*1e-9,"spatial source switch wrong");session->UndoToMark(mark,nullptr);session->DeleteUndoMark(mark,nullptr);Require(Solids(part)==1&&part->Features()->GetFeatures().size()==initialCount&&std::abs(Volume(body)-volume)<volume*1e-9,"spatial undo left changes");
        std::cout<<"PASS "<<name<<" mode="<<mode<<" length="<<p.length<<" cuts="<<p.bends.size()<<" fixed_walls=4 holes=6"<<std::endl;
    }
    auto mark=session->SetUndoMark(Session::MarkVisibilityVisible,"spatial custom");auto result=CreateFeature(selected[0],settings,nullptr,mark);auto* custom=dynamic_cast<Features::CustomFeature*>(NXObjectManager::Get(result.feature));tag_t savedFace=0;auto saved=ReadFeature(custom,savedFace);Require(saved.useAnchor&&Length(saved.anchorPoint-settings.anchorPoint)<1e-9,"anchor custom attributes missing");
    settings.divisions=8;mark=session->SetUndoMark(Session::MarkVisibilityVisible,"edit spatial custom");CreateFeature(selected[0],settings,custom,mark);Check(UF_PART_save_as((directory/(name+"-result.prt")).string().c_str()));Check(UF_PART_close(partTag,0,1));UF_PART_load_status_t load={};Check(UF_PART_open((directory/(name+"-result.prt")).string().c_str(),&partTag,&load));UF_PART_free_load_status(&load);part=session->Parts()->Work();custom=nullptr;for(auto* f:part->Features()->GetFeatures())if(auto* c=dynamic_cast<Features::CustomFeature*>(f))custom=c;Require(custom,"saved spatial custom lost");saved=ReadFeature(custom,savedFace);Require(saved.useAnchor&&saved.divisions==8&&Length(saved.anchorPoint-settings.anchorPoint)<1e-9,"saved anchor lost");saved.segmentArcs=false;saved.cutSource=false;mark=session->SetUndoMark(Session::MarkVisibilityVisible,"reopen spatial edit");CreateFeature(savedFace,saved,custom,mark);Require(Solids(part)==2,"spatial custom edit leaked bodies");Check(UF_PART_save());Check(UF_PART_close(partTag,0,1));std::cout<<"PASS "<<name<<" custom edit, anchor persistence, save/reopen/edit"<<std::endl;
}
int main(int argc,char** argv){try{Require(argc==2&&!std::filesystem::exists(argv[1]),"New output directory required");std::filesystem::create_directories(argv[1]);Check(UF_initialize());RequireFeatureClass();Case(argv[1],false);Case(argv[1],true);Case(argv[1],true,true);UF_terminate();return 0;}catch(const NXException& e){std::cerr<<"NX "<<e.ErrorCode()<<" "<<e.Message()<<std::endl;}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;}return 1;}

