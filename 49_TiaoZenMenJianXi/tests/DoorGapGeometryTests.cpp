#include "../DoorGapGeometry.hpp"
#include <NXOpen/NXObjectManager.hxx>
#include <uf.h>
#include <uf_modl.h>
#include <uf_obj.h>
#include <uf_part.h>

#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace NXOpen;

void Check(bool pass,const char* message) {
    if (!pass) throw std::runtime_error(message);
}
void Uf(int status) {
    if (status) {
        char message[256]{};
        UF_get_fail_message(status,message);
        throw std::runtime_error(std::to_string(status)+": "+message);
    }
}
Body* Block(double x,double y,double z,double dx,double dy,double dz) {
    double corner[]{x,y,z};
    std::array<std::string,3> size{std::to_string(dx),std::to_string(dy),std::to_string(dz)};
    char* dimensions[]{size[0].data(),size[1].data(),size[2].data()};
    tag_t feature=NULL_TAG,body=NULL_TAG;
    Uf(UF_MODL_create_block1(UF_NULLSIGN,corner,dimensions,&feature));
    Uf(UF_MODL_ask_feat_body(feature,&body));
    return dynamic_cast<Body*>(NXObjectManager::Get(body));
}
int main(int argc,char** argv) {
    try {
        Uf(UF_initialize());
        tag_t part=NULL_TAG;
        Uf(UF_PART_new("door_gap_fixture.prt",1,&part));
        auto* door=Block(0,0,0,200,150,2);
        auto* left=Block(-12,0,-5,10,150,20);
        auto* right=Block(203,0,-5,10,150,20);
        auto* bottom=Block(0,-14,-5,200,10,20);
        auto* top=Block(0,155,-5,200,10,20);
        auto* offSpan=Block(-0.5,200,-5,0.4,20,20);
        auto* offDepth=Block(-1.5,0,50,1,150,5);
        Uf(UF_OBJ_set_name(door->Tag(),"DOOR_PANEL"));
        Uf(UF_OBJ_set_name(left->Tag(),"LEFT_FRAME"));
        Uf(UF_OBJ_set_name(right->Tag(),"RIGHT_FRAME"));
        Uf(UF_OBJ_set_name(bottom->Tag(),"BOTTOM_FRAME"));
        Uf(UF_OBJ_set_name(top->Tag(),"TOP_FRAME"));
        const std::vector<Body*> bodies{door,left,right,bottom,top,offSpan,offDepth};
        const Point3d origin(0,0,2);
        const Vector3d normal(0,0,1),x(1,0,0),y(0,1,0);
        struct Case { Vector3d outward,tangent; double boundary,min,max,expected; tag_t body; };
        const std::array<Case,4> cases{{
            {Vector3d(-1,0,0),y,0,0,150,2,left->Tag()},
            {x,y,200,0,150,3,right->Tag()},
            {Vector3d(0,-1,0),x,0,0,200,4,bottom->Tag()},
            {y,x,150,0,200,5,top->Tag()}
        }};
        for (const auto& item:cases) {
            const auto result=door_gap_geometry::FindNearest(bodies,door->Tag(),
                origin,normal,item.outward,item.tangent,item.boundary,
                item.min,item.max,2,100);
            Check(result.face && result.body && result.body->Tag()==item.body,
                  "Wrong reference body");
            Check(std::abs(result.gap-item.expected)<0.001,"Wrong gap measurement");
        }
        const auto missing=door_gap_geometry::FindNearest({door},door->Tag(),
            origin,normal,x,y,200,0,150,2,100);
        Check(!missing.face,"Missing reference was not rejected");
        if (argc>1 && std::string(argv[1])=="--save-fixture") {
            Uf(UF_PART_save());
            std::cout << "Saved door_gap_fixture.prt" << std::endl;
        }
        std::cout << "PASS four side gaps, distractors, and missing reference" << std::endl;
        UF_terminate();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "FAIL " << ex.what() << std::endl;
        return 1;
    }
}
