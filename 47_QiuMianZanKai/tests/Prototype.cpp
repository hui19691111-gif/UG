// External NX experiment: prove a trimmed cylindrical petal is developable.
// This executable is never deployed or run against a user's part.
#include <NXOpen/Body.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/Features_FeatureCollection.hxx>
#include <NXOpen/Features_SheetMetal_SheetmetalManager.hxx>
#include <NXOpen/Features_SheetMetal_ConvertToSheetmetalBuilder.hxx>
#include <NXOpen/Features_SheetMetal_FlatSolidBuilder.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/SelectFace.hxx>
#include <NXOpen/Session.hxx>
#include <uf.h>
#include <uf_part.h>
#include <uf_curve.h>
#include <uf_modl.h>
#include <uf_csys.h>
#include <uf_obj.h>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace NXOpen;
void Check(int code) { if(code) {char m[256]={};UF_get_fail_message(code,m);throw std::runtime_error(std::to_string(code)+": "+m);} }
tag_t Extrude(const std::vector<tag_t>& curves,double length,std::array<double,3> dir={0,1,0}) {
    uf_list_p_t list=nullptr,features=nullptr;Check(UF_MODL_create_list(&list));
    for(auto t:curves) Check(UF_MODL_put_list_item(list,t));
    char taper[]="0",start[]="0"; std::string end=std::to_string(length);char* limits[]={start,end.data()};
    double point[3]={};
    int code=UF_MODL_create_extruded2(list,taper,limits,point,dir.data(),UF_NULLSIGN,&features);
    UF_MODL_delete_list(&list);Check(code);tag_t f=0,body=0;Check(UF_MODL_ask_list_item(features,0,&f));UF_MODL_delete_list(&features);
    Check(UF_MODL_ask_feat_body(f,&body));for(auto t:curves) Check(UF_OBJ_set_blank_status(t,UF_OBJ_BLANKED));return body;
}
using V=std::array<double,3>;
V Rot(V p,double a) {return {p[0]*cos(a)-p[1]*sin(a),p[0]*sin(a)+p[1]*cos(a),p[2]};}
tag_t Prism(const std::vector<V>& points,V direction,double length) {
    std::vector<tag_t> curves;
    for(size_t i=0;i<points.size();++i) {auto p=points[i],q=points[(i+1)%points.size()];UF_CURVE_line_t line={{p[0],p[1],p[2]},{q[0],q[1],q[2]}};tag_t c=0;Check(UF_CURVE_create_line(&line,&c));curves.push_back(c);}
    return Extrude(curves,length,direction);
}
tag_t Petal(double angle) {
    std::vector<tag_t> curves;
    auto line=[&](double x,double z,double x2,double z2) {auto p=Rot({x,-30,z},angle),q=Rot({x2,-30,z2},angle);UF_CURVE_line_t l={{p[0],p[1],p[2]},{q[0],q[1],q[2]}};tag_t t=0;Check(UF_CURVE_create_line(&l,&t));curves.push_back(t);};
    auto arc=[&](double r) {auto a=Rot({r,-30,0},angle),b=Rot({r*cos(.5),-30,r*sin(.5)},angle),c=Rot({r*cos(1.),-30,r*sin(1.)},angle);tag_t t=0;Check(UF_CURVE_create_arc_thru_3pts(1,a.data(),b.data(),c.data(),&t));curves.push_back(t);};
    line(100,-5,100,0);arc(100);line(100*cos(1.),100*sin(1.),98*cos(1.),98*sin(1.));arc(98);line(98,0,98,-5);line(98,-5,100,-5);
    tag_t petal=Extrude(curves,60,Rot({0,1,0},angle));
    double slope=tan(.1),inset=.5/cos(.1);
    std::vector<V> wedge={Rot({inset/slope,0,-6},angle),Rot({200,-200*slope+inset,-6},angle),Rot({200,200*slope-inset,-6},angle)};
    tag_t tool=Prism(wedge,{0,0,1},120);int count=0;tag_t* bodies=nullptr;Check(UF_MODL_intersect_bodies(petal,tool,&count,&bodies));if(count!=1) throw std::runtime_error("Intersection count");petal=bodies[0];UF_free(bodies);
    std::vector<V> base={Rot({98,-98*slope,-40},angle),Rot({100,-100*slope,-40},angle),Rot({100,100*slope,-40},angle),Rot({98,98*slope,-40},angle)};
    tag_t band=Prism(base,{0,0,1},36),f=0;Check(UF_MODL_unite_bodies_with_retained_options(petal,band,false,false,&f));return petal;
}
int main(int argc,char** argv) {
    try {
        if(argc!=2) throw std::runtime_error("Expected a new .prt output path");
        Check(UF_initialize());tag_t partTag=0;Check(UF_PART_new(argv[1],METRIC,&partTag));
        tag_t bodyTag=Petal(0);
        for(int i=1;i<5;++i) {tag_t f=0;Check(UF_MODL_unite_bodies_with_retained_options(bodyTag,Petal(i*.2),false,false,&f));}
        auto* body=dynamic_cast<Body*>(NXObjectManager::Get(bodyTag));auto* part=Session::GetSession()->Parts()->Work();
        Face* base=nullptr;
        for(auto* f:body->GetFaces()) {int type,sign;double p[3],d[3],box[6],r,r2;Check(UF_MODL_ask_face_data(f->Tag(),&type,p,d,box,&r,&r2,&sign));if(type==22 && std::abs(d[0])>.99) base=f;}
        if(!base) throw std::runtime_error("Missing stationary plane");
        auto* sm=part->Features()->SheetmetalManager();auto* convert=sm->CreateConvertToSheetmetalFeatureBuilder(nullptr);
        convert->SetBaseFace(base);convert->SetMaintainZeroBendRadius(true);convert->CommitFeature();convert->Destroy();
        auto* flat=sm->CreateFlatSolidFeatureBuilder(nullptr);flat->StationaryFace()->SetValue(base);flat->SetAssociative(true);
        auto* result=flat->CommitFeature();flat->Destroy();if(!result) throw std::runtime_error("No flat solid");
        Check(UF_PART_save());Check(UF_PART_close(partTag,0,1));Check(UF_terminate());std::cout<<"PASS native convert and flat solid\n";return 0;
    } catch(const NXException& e) {std::cerr<<"NX "<<e.ErrorCode()<<" "<<e.Message()<<'\n';}
      catch(const std::exception& e) {std::cerr<<e.what()<<'\n';}return 1;
}
