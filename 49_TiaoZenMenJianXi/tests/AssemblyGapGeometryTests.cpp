#include "../DoorGapGeometry.hpp"
#include "../../40_TiaoZenBanLeiCiCun/PanelSkirtGeometry.hpp"
#include <NXOpen/Assemblies_Component.hxx>
#include <NXOpen/DirectionCollection.hxx>
#include <NXOpen/FaceDumbRule.hxx>
#include <NXOpen/Features_FeatureCollection.hxx>
#include <NXOpen/Features_MoveFaceBuilder.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/PartLoadStatus.hxx>
#include <NXOpen/ScRuleFactory.hxx>
#include <NXOpen/SelectionIntentRuleOptions.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/SmartObject.hxx>
#include <uf.h>
#include <uf_assem.h>
#include <uf_modl.h>
#include <uf_part.h>

#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace NXOpen;
void Uf(int status) {
    if (status) {
        char message[256]{}; UF_get_fail_message(status,message);
        throw std::runtime_error(std::to_string(status)+": "+message);
    }
}
void Check(bool condition,const char* message) {
    if (!condition) throw std::runtime_error(message);
}
tag_t Block(double x,double y,double z,double dx,double dy,double dz) {
    double corner[]{x,y,z};
    std::array<std::string,3> lengths{std::to_string(dx),std::to_string(dy),std::to_string(dz)};
    char* dimensions[]{lengths[0].data(),lengths[1].data(),lengths[2].data()};
    tag_t feature=NULL_TAG,body=NULL_TAG;
    Uf(UF_MODL_create_block1(UF_NULLSIGN,corner,dimensions,&feature));
    Uf(UF_MODL_ask_feat_body(feature,&body));
    return body;
}
tag_t AddComponentAt(tag_t assembly,const char* part,const char* name,
                     double origin[3],double matrix[9]) {
    tag_t instance=NULL_TAG;
    UF_PART_load_status_t load{};
    const int code=UF_ASSEM_add_part_to_assembly2(assembly,part,nullptr,name,
        origin,matrix,-1,&instance,&load);
    UF_PART_free_load_status(&load);
    Uf(code);
    Check(instance!=NULL_TAG,"Component was not added");
    const tag_t occurrence=UF_ASSEM_ask_part_occ_of_inst(NULL_TAG,instance);
    Check(occurrence!=NULL_TAG,"Part occurrence was not found");
    return occurrence;
}
tag_t AddComponent(tag_t assembly,const char* part,const char* name) {
    double origin[]{10,20,0};
    double matrix[]{1,0,0,0,1,0,0,0,1};
    return AddComponentAt(assembly,part,name,origin,matrix);
}
int main() {
    try {
        Uf(UF_initialize());
        tag_t part=NULL_TAG;
        Uf(UF_PART_new("gap_door_component.prt",1,&part));
        const tag_t doorPart=part;
        const tag_t door=Block(0,0,0,200,150,2);
        tag_t doorFace=NULL_TAG;
        for (auto* face:dynamic_cast<Body*>(NXObjectManager::Get(door))->GetFaces()) {
            Point3d point; Vector3d normal;
            if (door_gap_geometry::Plane(face,point,normal) &&
                std::abs(point.Z-2)<0.001 && std::abs(normal.Z)>0.99) {
                doorFace=face->Tag(); break;
            }
        }
        Check(doorFace!=NULL_TAG,"Door skin not found");
        Uf(UF_PART_save());
        Uf(UF_PART_new("gap_frame_component.prt",1,&part));
        const std::array<tag_t,4> frames{
            Block(-12,0,-5,10,150,20),Block(203,0,-5,10,150,20),
            Block(0,-14,-5,200,10,20),Block(0,155,-5,200,10,20)};
        Uf(UF_PART_save());
        tag_t assembly=NULL_TAG;
        Uf(UF_PART_new("gap_assembly_fixture.prt",1,&assembly));
        const tag_t doorOcc=AddComponent(assembly,"gap_door_component.prt","DOOR");
        const tag_t frameOcc=AddComponent(assembly,"gap_frame_component.prt","FRAME");
        const tag_t selectedBody=UF_ASSEM_find_occurrence(doorOcc,door);
        const tag_t selectedFace=UF_ASSEM_find_occurrence(doorOcc,doorFace);
        Check(selectedBody!=NULL_TAG && selectedFace!=NULL_TAG,
              "Door object occurrences not found");
        double transform[4][4]{};
        Uf(UF_ASSEM_ask_transform_of_occ(selectedFace,transform));
        Check(std::abs(transform[0][3]-10)<0.001 &&
              std::abs(transform[1][3]-20)<0.001,
              "Door occurrence transform is incorrect");
        std::vector<Body*> bodies;
        bodies.push_back(dynamic_cast<Body*>(NXObjectManager::Get(selectedBody)));
        for (tag_t tag:frames) {
            const tag_t occurrence=UF_ASSEM_find_occurrence(frameOcc,tag);
            Check(occurrence!=NULL_TAG,"Frame body occurrence not found");
            bodies.push_back(dynamic_cast<Body*>(NXObjectManager::Get(occurrence)));
        }
        const Point3d origin(10,20,2);
        const Vector3d normal(0,0,1),x(1,0,0),y(0,1,0);
        struct Case { Vector3d outward,tangent; double boundary,min,max,expected; tag_t body; };
        const std::array<Case,4> cases{{
            {Vector3d(-1,0,0),y,0,0,150,2,bodies[1]->Tag()},
            {x,y,200,0,150,3,bodies[2]->Tag()},
            {Vector3d(0,-1,0),x,0,0,200,4,bodies[3]->Tag()},
            {y,x,150,0,200,5,bodies[4]->Tag()}
        }};
        for (const auto& item:cases) {
            const auto result=door_gap_geometry::FindNearest(bodies,selectedBody,
                origin,normal,item.outward,item.tangent,item.boundary,
                item.min,item.max,2,100);
            Check(result.face && result.body && result.body->Tag()==item.body,
                  "Wrong assembly reference body");
            Check(std::abs(result.gap-item.expected)<0.001,
                  "Wrong assembly gap");
        }
        Uf(UF_PART_save());
        auto* parts=Session::GetSession()->Parts();
        Check(parts->Work()->Tag()==assembly,"Assembly did not remain the work part");
        auto* doorComponent=dynamic_cast<Assemblies::Component*>(
            NXObjectManager::Get(doorOcc));
        Check(doorComponent!=nullptr,"Door work component was not found");
        PartLoadStatus* loadStatus=nullptr;
        parts->SetWorkComponent(doorComponent,PartCollection::RefsetOptionCurrent,
            PartCollection::WorkComponentOptionGiven,&loadStatus);
        delete loadStatus;
        Check(parts->Work()->Tag()==doorPart,"Door work part was not selected automatically");
        auto* work=parts->Work();
        auto* prototype=dynamic_cast<Face*>(NXObjectManager::Get(doorFace));
        const Point3d localOrigin(0,0,2);
        const Vector3d localNormal(0,0,1),localOutward(-1,0,0);
        auto selection=panel_skirt::Collect(prototype,localOrigin,localNormal,
            localOutward,2,0.001);
        Check(selection.error.empty() && !selection.faces.empty(),
              "Could not collect door boundary faces");
        std::vector<panel_skirt::SurfacePosition> before;
        for (auto* face:selection.faces) {
            panel_skirt::SurfacePosition position;
            Check(panel_skirt::Measure(face,localOrigin,localNormal,localOutward,position),
                  "Could not measure door boundary before move");
            before.push_back(position);
        }
        auto* direction=work->Directions()->CreateDirection(localOrigin,localOutward,
            SmartObject::UpdateOptionWithinModeling);
        auto* builder=work->Features()->CreateMoveFaceBuilder(nullptr);
        builder->SetType(Features::MoveFaceBuilder::TypesTranslateDirectionAndDistance);
        builder->SetDirection(direction);
        builder->Distance()->SetFormula("-0.5");
        auto* options=work->ScRuleFactory()->CreateRuleOptions();
        options->SetSelectedFromInactive(false);
        auto* rule=work->ScRuleFactory()->CreateRuleFaceDumb(selection.faces,options);
        delete options;
        builder->MoveFaceCollector()->ReplaceRules(
            std::vector<SelectionIntentRule*>{rule},false);
        auto move=panel_skirt::CommitAndVerifyMove(builder,before,localOrigin,
            localNormal,localOutward,-0.5,0.001);
        builder->Destroy();
        Check(move.error.empty(),move.error.c_str());
        Uf(UF_MODL_update());
        auto* movedOccurrence=dynamic_cast<Body*>(NXObjectManager::Get(selectedBody));
        Check(movedOccurrence!=nullptr,"Door occurrence disappeared after move");
        double minX=1.0e9;
        for (auto* edge:movedOccurrence->GetEdges()) {
            Point3d a,b; edge->GetVertices(&a,&b);
            minX=(std::min)(minX,(std::min)(a.X,b.X));
        }
        Check(std::abs(minX-10.5)<0.001,
              ("Door occurrence extents did not update: minX="+
               std::to_string(minX)).c_str());
        std::vector<Body*> updated{movedOccurrence};
        updated.insert(updated.end(),bodies.begin()+1,bodies.end());
        auto left=door_gap_geometry::FindNearest(updated,selectedBody,origin,normal,
            Vector3d(-1,0,0),y,10-minX,0,150,2,100);
        Check(left.face && std::abs(left.gap-2.5)<0.001,
              ("Assembly gap did not update to 2.5 mm: face="+
               std::to_string(left.face!=nullptr)+", gap="+
               std::to_string(left.gap)).c_str());
        loadStatus=nullptr;
        parts->SetWorkComponent(nullptr,&loadStatus);
        delete loadStatus;
        Check(parts->Work()->Tag()==assembly,
              "Assembly work part was not restored after editing the door");
        std::cout << "PASS translated assembly four side gaps and Move Face" << std::endl;

        tag_t sharedPart=NULL_TAG;
        Uf(UF_PART_new("gap_shared_door.prt",1,&sharedPart));
        const tag_t sharedBody=Block(0,0,0,200,150,2);
        tag_t sharedTop=NULL_TAG,sharedRight=NULL_TAG;
        for (auto* candidate:dynamic_cast<Body*>(NXObjectManager::Get(sharedBody))->GetFaces()) {
            Point3d point; Vector3d direction;
            if (!door_gap_geometry::Plane(candidate,point,direction)) continue;
            if (std::abs(point.Z-2)<0.001 && std::abs(direction.Z)>0.99)
                sharedTop=candidate->Tag();
            if (std::abs(point.X-200)<0.001 && std::abs(direction.X)>0.99)
                sharedRight=candidate->Tag();
        }
        Check(sharedTop!=NULL_TAG && sharedRight!=NULL_TAG,
              "Shared door faces not found");
        Uf(UF_PART_save());
        tag_t sharedAssembly=NULL_TAG;
        Uf(UF_PART_new("gap_shared_fixture.prt",1,&sharedAssembly));
        double firstOrigin[]{0,0,0},secondOrigin[]{402,150,0};
        double identity[]{1,0,0,0,1,0,0,0,1};
        double mirrored[]{-1,0,0,0,-1,0,0,0,1};
        const tag_t first=AddComponentAt(sharedAssembly,"gap_shared_door.prt",
            "DOOR_A",firstOrigin,identity);
        const tag_t second=AddComponentAt(sharedAssembly,"gap_shared_door.prt",
            "DOOR_B",secondOrigin,mirrored);
        const tag_t firstBody=UF_ASSEM_find_occurrence(first,sharedBody);
        const tag_t secondBody=UF_ASSEM_find_occurrence(second,sharedBody);
        const tag_t secondRight=UF_ASSEM_find_occurrence(second,sharedRight);
        Check(firstBody && secondBody && secondRight,"Shared occurrences not found");
        std::vector<Body*> pair{
            dynamic_cast<Body*>(NXObjectManager::Get(firstBody)),
            dynamic_cast<Body*>(NXObjectManager::Get(secondBody))};
        const Point3d pairOrigin(0,0,2);
        const Vector3d pairNormal(0,0,1),pairOutward(1,0,0),pairTangent(0,1,0);
        auto initial=door_gap_geometry::FindNearest(pair,firstBody,pairOrigin,
            pairNormal,pairOutward,pairTangent,200,0,150,2,100);
        Check(initial.face && initial.face->Tag()==secondRight &&
              std::abs(initial.gap-2)<0.001,
              "Mirrored shared reference was not measured at 2 mm");
        Uf(UF_PART_save());
        Check(parts->Work()->Tag()==sharedAssembly,
              "Shared assembly did not remain the work part");
        auto* sharedComponent=dynamic_cast<Assemblies::Component*>(
            NXObjectManager::Get(first));
        Check(sharedComponent!=nullptr,"Shared door work component was not found");
        loadStatus=nullptr;
        parts->SetWorkComponent(sharedComponent,PartCollection::RefsetOptionCurrent,
            PartCollection::WorkComponentOptionGiven,&loadStatus);
        delete loadStatus;
        Check(parts->Work()->Tag()==sharedPart,
              "Shared door work part was not selected automatically");
        auto* sharedFace=dynamic_cast<Face*>(NXObjectManager::Get(sharedTop));
        auto sharedSelection=panel_skirt::Collect(sharedFace,pairOrigin,pairNormal,
            pairOutward,2,0.001);
        Check(sharedSelection.error.empty() && !sharedSelection.faces.empty(),
              "Shared door moving face was not collected");
        const tag_t referencePrototype=UF_ASSEM_ask_prototype_of_occ(secondRight);
        Check(referencePrototype==sharedRight,"Shared reference prototype differs");
        Check(std::any_of(sharedSelection.faces.begin(),sharedSelection.faces.end(),
            [referencePrototype](Face* f){return f && f->Tag()==referencePrototype;}),
            "Shared reference is not a moving face");
        double secondTransform[4][4]{};
        Uf(UF_ASSEM_ask_transform_of_occ(secondRight,secondTransform));
        const double projection=pairOutward.X*(secondTransform[0][0]*pairOutward.X+
            secondTransform[0][1]*pairOutward.Y+secondTransform[0][2]*pairOutward.Z);
        Check(std::abs(projection+1)<0.001,"Mirrored response projection is not -1");
        const double compensatedDistance=(initial.gap-3.0)/(1.0-projection);
        std::vector<panel_skirt::SurfacePosition> sharedBefore;
        for (auto* moving:sharedSelection.faces) {
            panel_skirt::SurfacePosition position;
            Check(panel_skirt::Measure(moving,pairOrigin,pairNormal,pairOutward,position),
                  "Could not measure shared boundary");
            sharedBefore.push_back(position);
        }
        auto* sharedWork=Session::GetSession()->Parts()->Work();
        auto* sharedDirection=sharedWork->Directions()->CreateDirection(
            pairOrigin,pairOutward,SmartObject::UpdateOptionWithinModeling);
        auto* sharedBuilder=sharedWork->Features()->CreateMoveFaceBuilder(nullptr);
        sharedBuilder->SetType(Features::MoveFaceBuilder::TypesTranslateDirectionAndDistance);
        sharedBuilder->SetDirection(sharedDirection);
        sharedBuilder->Distance()->SetFormula(std::to_string(compensatedDistance).c_str());
        auto* sharedOptions=sharedWork->ScRuleFactory()->CreateRuleOptions();
        sharedOptions->SetSelectedFromInactive(false);
        auto* sharedRule=sharedWork->ScRuleFactory()->CreateRuleFaceDumb(
            sharedSelection.faces,sharedOptions);
        delete sharedOptions;
        sharedBuilder->MoveFaceCollector()->ReplaceRules(
            std::vector<SelectionIntentRule*>{sharedRule},false);
        auto sharedMove=panel_skirt::CommitAndVerifyMove(sharedBuilder,sharedBefore,
            pairOrigin,pairNormal,pairOutward,compensatedDistance,0.001);
        sharedBuilder->Destroy();
        Check(sharedMove.error.empty(),sharedMove.error.c_str());
        Uf(UF_MODL_update());
        auto* firstMoved=dynamic_cast<Body*>(NXObjectManager::Get(firstBody));
        auto* secondMoved=dynamic_cast<Body*>(NXObjectManager::Get(secondBody));
        Check(firstMoved && secondMoved,"Shared door occurrence was lost");
        double firstMax=-1.0e9,secondMin=1.0e9;
        for (auto* edge:firstMoved->GetEdges()) {
            Point3d a,b; edge->GetVertices(&a,&b);
            firstMax=(std::max)(firstMax,(std::max)(a.X,b.X));
        }
        for (auto* edge:secondMoved->GetEdges()) {
            Point3d a,b; edge->GetVertices(&a,&b);
            secondMin=(std::min)(secondMin,(std::min)(a.X,b.X));
        }
        Check(std::abs(firstMax-199.5)<0.001 &&
              std::abs(secondMin-202.5)<0.001 &&
              std::abs(secondMin-firstMax-3)<0.001,
              ("Shared assembly did not reach 3 mm: first="+
               std::to_string(firstMax)+", second="+std::to_string(secondMin)).c_str());
        loadStatus=nullptr;
        parts->SetWorkComponent(nullptr,&loadStatus);
        delete loadStatus;
        Check(parts->Work()->Tag()==sharedAssembly,
              "Shared assembly work part was not restored");
        std::cout << "PASS mirrored shared-prototype gap 2 to 3 mm" << std::endl;
        UF_terminate();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "FAIL " << ex.what() << std::endl;
        return 1;
    }
}
