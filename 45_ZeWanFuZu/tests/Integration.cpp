#include "../ZeWanFuZuGeometry.hpp"
#include "../ZeWanFuZuFeature.hpp"
#include <NXOpen/Features_CustomFeature.hxx>
#include <NXOpen/Features_CustomFeatureData.hxx>
#include <NXOpen/Features_CustomTagArrayAttribute.hxx>
#include <NXOpen/Features_FeatureCollection.hxx>
#include <NXOpen/Features_ConstructionFeatureData.hxx>
#include <NXOpen/Features_FlatPattern.hxx>
#include <NXOpen/Features_SheetMetal_SheetmetalManager.hxx>
#include <NXOpen/Features_SheetMetal_ConvertToSheetmetalBuilder.hxx>
#include <NXOpen/Features_SheetMetal_FlatPatternBuilder.hxx>
#include <NXOpen/Features_SheetMetal_FlatSolidBuilder.hxx>
#include <NXOpen/SelectFace.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/Body.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/Edge.hxx>
#include <uf.h>
#include <uf_curve.h>
#include <uf_csys.h>
#include <uf_modl.h>
#include <uf_obj.h>
#include <uf_part.h>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace bend_assist;
static void Require(bool b,const char* text) { if(!b) throw std::runtime_error(text); }
struct Fixture { tag_t body=0,sloped=0,parallel=0,broad=0; };
static Vec Transform(Vec p,bool rotate,double scale) {
    // Proper rigid rotation plus translation, to catch WCS-dependent code.
    if(rotate) return Vec{p.y+31,p.z-17,p.x+83}*scale;
    return p*scale;
}
static tag_t Extrude(const std::vector<tag_t>& curves,Vec dir,double length,tag_t target=0) {
    uf_list_p_t list=nullptr,features=nullptr; Check(UF_MODL_create_list(&list));
    for(tag_t c:curves) Check(UF_MODL_put_list_item(list,c));
    char zero[]="0"; auto end=std::to_string(length); char* limits[]={zero,end.data()};
    double origin[3]={},direction[]={dir.x,dir.y,dir.z};
    Check(UF_MODL_create_extruded1(list,zero,limits,origin,direction,target?UF_NEGATIVE:UF_NULLSIGN,target,&features));
    tag_t f=0,b=0; Check(UF_MODL_ask_list_item(features,0,&f)); Check(UF_MODL_ask_feat_body(f,&b));
    UF_MODL_delete_list(&list); UF_MODL_delete_list(&features);
    for(auto c:curves) UF_OBJ_set_blank_status(c,UF_OBJ_BLANKED);
    return b;
}
static Fixture MakeFixture(bool rotate=false,double scale=1,int curved=0) {
    auto trans=[&](Vec v){return Transform(v,rotate,scale);};
    auto vector=[&](Vec v){return rotate?Vec{v.y,v.z,v.x}:v;};
    std::vector<tag_t> curves;
    auto line=[&](Vec a,Vec b){a=trans(a);b=trans(b);UF_CURVE_line_t l={{a.x,a.y,a.z},{b.x,b.y,b.z}};tag_t t=0;Check(UF_CURVE_create_line(&l,&t));curves.push_back(t);};
    Vec ay=vector({0,1,0}),az=vector({0,0,1}),ax=vector({1,0,0});
    double matrix[]={ay.x,ay.y,ay.z,az.x,az.y,az.z,ax.x,ax.y,ax.z}; tag_t matrixTag=0;
    Check(UF_CSYS_create_matrix(matrix,&matrixTag));
    auto arc=[&](double radius){
        UF_CURVE_arc_t a={}; a.matrix_tag=matrixTag; a.start_angle=3.141592653589793; a.end_angle=4.71238898038469; a.radius=radius*scale;
        Vec center=trans({0,0,0});
        // UF arc center is expressed in its matrix coordinates.
        a.arc_center[0]=Dot(center,ay);a.arc_center[1]=Dot(center,az);a.arc_center[2]=Dot(center,ax);
        tag_t t=0;Check(UF_CURVE_create_arc(&a,&t));curves.push_back(t);
    };
    double height=curved?210:70;
    line({0,-3,height},{0,-1,height}); line({0,-1,height},{0,-1,0}); arc(1);
    line({0,0,-1},{0,80,-1});line({0,80,-1},{0,80,-3});line({0,80,-3},{0,0,-3});arc(3);line({0,-3,0},{0,-3,height});
    Fixture f; f.body=Extrude(curves,vector({1,0,0}),200*scale);
    curves.clear();
    if(curved) {
        double endHeight=curved==1?20:180,midHeight=curved==1?120:80;
        Vec a=trans({0,-4,endHeight}),b=trans({100,-4,midHeight}),c=trans({200,-4,endHeight});
        double first[]={a.x,a.y,a.z},middle[]={b.x,b.y,b.z},last[]={c.x,c.y,c.z}; tag_t t=0;
        Check(UF_CURVE_create_arc_thru_3pts(1,first,middle,last,&t));curves.push_back(t);
        line({200,-4,endHeight},{201,-4,endHeight});line({201,-4,endHeight},{201,-4,230});
        line({201,-4,230},{-1,-4,230});line({-1,-4,230},{-1,-4,endHeight});line({-1,-4,endHeight},{0,-4,endHeight});
    } else {
        line({-1,-4,19.75},{201,-4,70.25});line({201,-4,70.25},{201,-4,100});
        line({201,-4,100},{-1,-4,100});line({-1,-4,100},{-1,-4,19.75});
    }
    f.body=Extrude(curves,vector({0,1,0}),5*scale,f.body);
    auto* body=dynamic_cast<NXOpen::Body*>(NXOpen::NXObjectManager::Get(f.body));
    for(auto* face:body->GetFaces()) {
        int type,sign; double p[3],d[3],box[6],r,r2;
        Check(UF_MODL_ask_face_data(face->Tag(),&type,p,d,box,&r,&r2,&sign));
        if(type==16 && curved && std::abs(r-100*scale)<1e-5*scale) f.sloped=face->Tag();
        if(type!=22) continue;
        double uv[2],onFace[3],u1[3],v1[3],u2[3],v2[3],outward[3],radii[2];
        Check(UF_MODL_ask_face_parm(face->Tag(),p,uv,onFace));
        Check(UF_MODL_ask_face_props(face->Tag(),uv,onFace,u1,v1,u2,v2,outward,radii));
        Vec n={outward[0],outward[1],outward[2]};
        if(Dot(n,vector(Unit({-.25,0,1})))>.9999) f.sloped=face->Tag();
        if(Dot(n,vector({0,1,0}))>.9999 && Dot(Vec{p[0],p[1],p[2]}-trans({0,80,0}),vector({0,1,0}))<1e-4*scale &&
            std::abs(Dot(Vec{p[0],p[1],p[2]}-trans({0,80,0}),vector({0,1,0})))<1e-4*scale) f.parallel=face->Tag();
        if(Dot(n,vector({0,1,0}))>.9999 && std::abs(Dot(Vec{p[0],p[1],p[2]}-trans({0,-1,0}),vector({0,1,0})))<1e-4*scale) f.broad=face->Tag();
    }
    Require(f.sloped && f.parallel && f.broad,"Fixture face discovery failed");return f;
}
static double Volume(tag_t body) {
    // The added helper is tiny relative to the parent. Use a tight mass-property
    // tolerance so differencing two large curved-body volumes is meaningful.
    double accuracy[11]={.999999999},props[47]={},stats[13]={};
    Check(UF_MODL_ask_mass_props_3d(&body,1,1,4,1,1,accuracy,props,stats)); return props[1];
}
static int Contains(tag_t body,Vec p) {
    double a[]={p.x,p.y,p.z};int status=0;Check(UF_MODL_ask_point_containment(a,body,&status));return status;
}
static void Test(const std::string& path,bool rotate,bool inch,int curved=0,bool atEndpoint=false) {
    tag_t part=0;Check(UF_PART_new(path.c_str(),inch?UF_PART_ENGLISH:UF_PART_METRIC,&part));
    Fixture f=MakeFixture(rotate,inch?1.0/25.4:1,curved);
    auto info=Inspect(f.sloped); auto parallel=Inspect(f.parallel);
    Require(!info.parallel && parallel.parallel,"Line-plane parallel decision failed");
    Require(info.circular==(curved!=0),"Circular thickness face classification failed");
    Require(std::abs(info.thickness-2*info.unitsPerMm)<1e-5,"Thickness changed");
    bool rejected=false;try{Inspect(f.broad);}catch(...){rejected=true;}Require(rejected,"Broad face was accepted");
    Settings s;if(atEndpoint){s.width=60;s.endDistance=0;}auto p=PlanFaces({info,parallel,info},s);
    Require(p.size()==1,"Duplicate/parallel faces not skipped");
    Require(PlanFaces({parallel},s).empty(),"All-parallel selection must be a no-op");
    Require(std::abs(p[0].top-(curved==1?100:curved==2?0:50)*info.unitsPerMm)<1e-5,"Automatic highest-edge alignment failed");
    if(curved) {
        int arcs=0;for(const auto& segment:p[0].segments) if(segment.circular) ++arcs;
        Require(arcs==5,"Curved boundary was faceted instead of modeled with exact arcs");
        auto crest=s;crest.width=20;crest.endDistance=90;
        bool rejectedAtCrest=false;try{PlanFaces({info},crest);}catch(...){rejectedAtCrest=true;}
        Require(rejectedAtCrest==(curved==1),"Interior circular maximum height check failed");
        crest.autoAlign=false;crest.height=120;
        Require(PlanFaces({info},crest).size()==1,"Arc with parallel tangent at center was skipped");
        if(curved==2) {
            auto invalidGap=s;invalidGap.gap=101;bool invalidRadius=false;
            try{PlanFaces({info},invalidGap);}catch(...){invalidRadius=true;}
            Require(invalidRadius,"Concave offset radius may not be zero or negative");
            auto beyondOffset=s;beyondOffset.endDistance=0;bool invalidEndpoint=false;
            try{PlanFaces({info},beyondOffset);}catch(...){invalidEndpoint=true;}
            Require(invalidEndpoint,"Concave offset outside its domain was silently clamped");
        }
        auto* body=dynamic_cast<NXOpen::Body*>(NXOpen::NXObjectManager::Get(f.body));
        for(auto* face:body->GetFaces()) {
            int type,sign;double q[3],d[3],box[6],r,r2;
            Check(UF_MODL_ask_face_data(face->Tag(),&type,q,d,box,&r,&r2,&sign));
            if(type==16 && r<4*info.unitsPerMm) {
                bool rejectedBend=false;try{Inspect(face->Tag());}catch(...){rejectedBend=true;}
                Require(rejectedBend,"Actual bend surface mistaken for circular thickness edge");
            }
        }
    }
    auto manual=s; manual.autoAlign=false;manual.height=120;manual.reverseEnd=true;
    auto rev=PlanFaces({info},manual);
    Require(std::abs(rev[0].x0-(200-s.endDistance-s.width)*info.unitsPerMm)<1e-5,"Reverse endpoint failed");
    auto invalid=s;invalid.width=300;rejected=false;try{PlanFaces({info},invalid);}catch(...){rejected=true;}Require(rejected,"Overflow width accepted");
    invalid=s;invalid.inset=s.width/2;rejected=false;try{PlanFaces({info},invalid);}catch(...){rejected=true;}Require(rejected,"Overlapping bridges accepted");
    double before=Volume(f.body);
    auto* session=NXOpen::Session::GetSession();auto mark=session->SetUndoMark(NXOpen::Session::MarkVisibilityVisible,"test rollback");
    auto created=CreateFeature(p[0],s);
    auto* custom=dynamic_cast<NXOpen::Features::CustomFeature*>(NXOpen::NXObjectManager::Get(created));
    Require(custom && custom->GetConstructionFeatures().size()==2,"Expected custom feature owning extrusion and unite");
    Require(FeatureSettings(custom).gap==s.gap,"Saved gap differs from UI");
    const auto& plan=p[0];auto pos=[&](double x,double dy){return info.origin+info.x*x+info.y*(BoundaryHeight(info,x)+dy)+info.z*(info.thickness/2);};
    Require(Contains(f.body,pos(plan.x0+plan.inset+plan.bridgeWidth/2,plan.gap/2))==1,"Bridge missing");
    Require(Contains(f.body,pos((plan.x0+plan.x1)/2,plan.gap/2))==2,"Gap filled unexpectedly");
    Require(Contains(f.body,info.origin+info.x*((plan.x0+plan.x1)/2)+info.y*(plan.top-info.unitsPerMm)+info.z*(info.thickness/2))==1,"Auxiliary plate missing");
    double width=s.width*info.unitsPerMm;
    auto integral=[&](double x,bool offset) {
        if(!curved) return info.slope*x*x/2+(offset?plan.gap*std::sqrt(1+info.slope*info.slope)*x:0);
        double dx=x-info.centerX,r=info.radius+(offset?info.branch*plan.gap:0);
        return info.centerY*x+info.branch*0.5*(dx*std::sqrt(std::max(0.0,r*r-dx*dx))+r*r*std::asin(dx/r));
    };
    if(curved) {
        for(double x:{plan.x0+plan.inset,plan.x0+plan.inset+plan.bridgeWidth,
                      plan.x1-plan.inset-plan.bridgeWidth,plan.x1-plan.inset}) {
            Require(Contains(f.body,pos(x,0))!=2,"Bridge does not reach circular source edge");
        }
    }
    double area=width*plan.top-(integral(plan.x1,true)-integral(plan.x0,true));
    for(double a:{plan.x0+plan.inset,plan.x1-plan.inset-plan.bridgeWidth}) {
        double b=a+plan.bridgeWidth;
        area+=integral(b,true)-integral(a,true)-integral(b,false)+integral(a,false);
    }
    double added=area*info.thickness;
    double expectedM3=added*std::pow(inch?.0254:.001,3);
    double modelingTolerance=0;Check(UF_MODL_ask_distance_tolerance_of_part(part,&modelingTolerance));
    // Four trimmed curved bridge junctions are resolved within the NX part's
    // distance tolerance (0.01 mm by default). Bound their possible volume
    // error dimensionally, independently of the observed test residual.
    double junctionError=curved?4*plan.bridgeWidth*info.thickness*modelingTolerance*std::pow(inch?.0254:.001,3):0;
    Require(std::abs(Volume(f.body)-before-expectedM3)<std::max(expectedM3*1e-5,junctionError),"Added volume exceeds NX modeling tolerance");
    if(curved) {
        auto* result=dynamic_cast<NXOpen::Body*>(NXOpen::NXObjectManager::Get(f.body));int retainedCircularFaces=0,gapFaces=0;bool reselected=false;
        Vec originalCenter=info.origin+info.x*info.centerX+info.y*info.centerY;
        double offsetRadius=info.radius+info.branch*plan.gap;
        for(auto* face:result->GetFaces()) {
            int type,sign;double q[3],d[3],box[6],r,r2;
            Check(UF_MODL_ask_face_data(face->Tag(),&type,q,d,box,&r,&r2,&sign));
            if(type==16 && r>4*info.unitsPerMm) {
                bool isSource=std::abs(r-info.radius)<1e-7*info.unitsPerMm,isGap=std::abs(r-offsetRadius)<1e-7*info.unitsPerMm;
                Require(isSource || isGap,"Unexpected circular radius after union");++retainedCircularFaces;
                Vec delta=Vec{q[0],q[1],q[2]}-originalCenter;
                if(isGap) {
                    // Inspect actual NX cylinders: a translated same-radius
                    // arc (the original bug) fails both of these checks.
                    Require(std::abs(Dot(delta,info.x))<1e-7*info.unitsPerMm && std::abs(Dot(delta,info.y))<1e-7*info.unitsPerMm,"Gap circle is not concentric");
                    Require(std::abs(std::abs(r-info.radius)-plan.gap)<1e-7*info.unitsPerMm,"Normal gap differs from requested distance");++gapFaces;
                }
                if(isSource && std::abs(Dot(delta,info.y))<1e-5*info.unitsPerMm) {
                    double lo=1e100,hi=-1e100;
                    for(auto* edge:face->GetEdges()) {NXOpen::Point3d a,b;edge->GetVertices(&a,&b);for(auto vertex:{a,b}){double x=Dot(Vec{vertex.X,vertex.Y,vertex.Z},info.x);lo=std::min(lo,x);hi=std::max(hi,x);}}
                    if(hi-lo>100*info.unitsPerMm) {auto remaining=Inspect(face->Tag());Require(PlanFaces({remaining},s).size()==1,"Cannot preview another helper on remaining circular face");reselected=true;}
                }
            }
        }
        Require(retainedCircularFaces>=4,"Circular faces were replaced by faceted planar faces");
        Require(gapFaces==3,"Expected three concentric gap spans separated by two bridges");
        Require(reselected,"Remaining source arc cannot be selected after Apply");
        for(const auto& segment:plan.segments) if(segment.circular && std::abs(segment.radius-offsetRadius)<1e-7*info.unitsPerMm) {
            for(double fraction:{0.2,0.5,0.8}) {
                double x=Dot(segment.start-info.origin,info.x)*(1-fraction)+Dot(segment.end-info.origin,info.x)*fraction;
                double dx=x-info.centerX,dy=info.branch*std::sqrt(offsetRadius*offsetRadius-dx*dx);
                Vec radial=(info.x*dx+info.y*dy)*(1/offsetRadius);
                Vec mid=originalCenter+radial*(info.radius+info.branch*plan.gap/2)+info.z*(info.thickness/2);
                // Near a vertical bridge side the radial ray can enter the
                // bridge; use points whose ray stays within this open span.
                double mx=Dot(mid-info.origin,info.x),a=Dot(segment.start-info.origin,info.x),b=Dot(segment.end-info.origin,info.x);
                if(mx>a+0.1*info.unitsPerMm && mx<b-0.1*info.unitsPerMm)
                    Require(Contains(f.body,mid)==2,"Normal gap is obstructed away from micro bridges");
            }
        }
    }
    double originalVolume=Volume(f.body);
    auto editMark=session->SetUndoMark(NXOpen::Session::MarkVisibilityVisible,"test edit");
    auto edited=s;edited.gap=3;edited.width=s.width+2;edited.inset=0;edited.autoAlign=false;edited.height=160;
    if(atEndpoint) edited.endDistance=1;
    auto memberTags=custom->FeatureData()->CustomTagArrayAttributeByName(internalAttribute)->GetValues();
    tag_t originalExtrude=memberTags[0]->Tag(),originalUnite=memberTags[1]->Tag();
    std::vector<tag_t> oldProfile;
    for(auto* curve:custom->FeatureData()->CustomTagArrayAttributeByName(curvesAttribute)->GetValues()) oldProfile.push_back(curve->Tag());
    EditFeature(custom,edited);
    Require(FeatureSettings(custom).gap==3 && FeatureSettings(custom).inset==0,"Edited parameters not retained");
    auto editedMembers=custom->FeatureData()->CustomTagArrayAttributeByName(internalAttribute)->GetValues();
    Require(editedMembers[0]->Tag()==originalExtrude && editedMembers[1]->Tag()==originalUnite,"Edit replaced downstream feature references");
    Require(Volume(f.body)>originalVolume,"Edit did not change actual geometry");
    for(auto curve:oldProfile) Require(UF_OBJ_ask_status(curve)!=UF_OBJ_ALIVE,"Edit leaked detached profile curves");
    double editedVolume=Volume(f.body);
    auto bad=edited;bad.width=300;bool invalidEdit=false;
    try{EditFeature(custom,bad);}catch(...){invalidEdit=true;}
    Require(invalidEdit && FeatureSettings(custom).width==edited.width && std::abs(Volume(f.body)-editedVolume)<editedVolume*1e-8,"Invalid edit changed feature");
    session->UndoToMark(editMark,nullptr);session->DeleteUndoMark(editMark,nullptr);
    Require(std::abs(Volume(f.body)-originalVolume)<originalVolume*1e-8 && FeatureSettings(custom).gap==s.gap,"Undo edit failed");
    session->UndoToMark(mark,nullptr);session->DeleteUndoMark(mark,nullptr);
    Require(std::abs(Volume(f.body)-before)<before*1e-8,"Undo did not restore source");
    Check(UF_PART_save_as((path+"-source.prt").c_str()));
    CreateFeature(p[0],s);Check(UF_PART_save_as(path.c_str()));
    std::cout<<"PASS "<<path<<" : "<<(curved?"concentric normal gap, exact arcs, extremum, curved bridges, ":"")<<"parallel skip, face validation, units, alignment, reverse, gap, volume, undo\n";
    Check(UF_PART_close(part,0,1));
    UF_PART_load_status_t load={};Check(UF_PART_open(path.c_str(),&part,&load));UF_PART_free_load_status(&load);
    NXOpen::Features::CustomFeature* reopened=nullptr;
    for(auto* feature:session->Parts()->Work()->Features()->GetFeatures())
        if(auto* candidate=dynamic_cast<NXOpen::Features::CustomFeature*>(feature)) reopened=candidate;
    Require(reopened && FeatureSettings(reopened).gap==s.gap,"Custom feature did not persist through reopen");
    EditFeature(reopened,edited);Require(FeatureSettings(reopened).gap==3,"Reopened feature cannot edit");
    Check(UF_PART_close(part,0,1));
    std::cout<<"PASS custom edit, zero inset, stable internal feature tags, undo and reopen\n";
}
static NXOpen::Features::Feature* MakeFlat(Fixture& f,bool solidOnly=false) {
    using namespace NXOpen;
    auto* part=Session::GetSession()->Parts()->Work();
    auto* manager=part->Features()->SheetmetalManager();
    auto* convert=manager->CreateConvertToSheetmetalFeatureBuilder(nullptr);
    convert->SetApplicationContext(Features::SheetMetal::ApplicationContextNxSheetMetal);
    convert->SetBaseFace(dynamic_cast<Face*>(NXObjectManager::Get(f.broad)));
    convert->CommitFeature();convert->Destroy();
    if(solidOnly) {
        auto* builder=manager->CreateFlatSolidFeatureBuilder(nullptr);
        builder->SetApplicationContext(Features::SheetMetal::ApplicationContextNxSheetMetal);
        builder->StationaryFace()->SetValue(dynamic_cast<Face*>(NXObjectManager::Get(f.broad)));
        auto* result=builder->CommitFeature();builder->Destroy();return result;
    }
    auto* flatBuilder=manager->CreateFlatPatternBuilder(nullptr);
    flatBuilder->SetApplicationContext(Features::SheetMetal::ApplicationContextNxSheetMetal);
    flatBuilder->SetKeepFlatSolidExternal(false);
    flatBuilder->SetOrientation(Features::SheetMetal::FlatSolidBuilder::OrientationTypeDefault);
    flatBuilder->UpwardFace()->SetValue(dynamic_cast<Face*>(NXObjectManager::Get(f.broad)));
    auto* flat=flatBuilder->CommitFeature();flatBuilder->Destroy();
    Require(flat && flat->GetFeatureErrorMessages().empty(),"Fixture flat pattern failed");
    return flat;
}
static tag_t FlatBody(NXOpen::Features::Feature* flat) {
    if(auto* pattern=dynamic_cast<NXOpen::Features::FlatPattern*>(flat)) {
        std::vector<NXOpen::Features::FlatPattern::ObjectDataFace> faces;
        pattern->GetBendUpCenterLines(faces);
        if(faces.empty()) pattern->GetBendDownCenterLines(faces);
        Require(!faces.empty() && faces.front().FlatSolidObject,"Missing flat solid bend data");
        return faces.front().FlatSolidObject->GetBody()->Tag();
    }
    return flat->GetBodies().front()->Tag();
}
static void TestFlatHistory(const std::string& path,int curved=0,bool multipleBodies=false,bool solidOnly=false) {
    using namespace NXOpen;
    tag_t partTag=0;Check(UF_PART_new(path.c_str(),UF_PART_METRIC,&partTag));
    auto* session=Session::GetSession();auto* part=session->Parts()->Work();
    Features::Feature* foreignFlat=nullptr;double foreignVolume=0;
    if(multipleBodies) {
        auto other=MakeFixture(true);foreignFlat=MakeFlat(other);foreignVolume=Volume(FlatBody(foreignFlat));
    }
    auto f=MakeFixture(false,1,curved);auto* flat=MakeFlat(f,solidOnly);
    auto info=Inspect(f.sloped);Settings s;
    const double sourceVolume=Volume(f.body),sourceFlatVolume=Volume(FlatBody(flat));
    auto originalCurrent=part->CurrentFeature()->Tag();
    Check(UF_PART_save_as((path+"-source.prt").c_str()));
    auto createMark=session->SetUndoMark(Session::MarkVisibilityVisible,"flat history create");
    std::cout<<"HISTORY curved="<<curved<<" stage=create\n";
    auto* custom=dynamic_cast<Features::CustomFeature*>(NXObjectManager::Get(CreateFeature(PlanFaces({info},s).front(),s)));
    Require(custom->Timestamp()<flat->Timestamp(),"Bend assist was appended after existing flat pattern");
    for(auto* member:custom->GetConstructionFeatures())
        Require(member->GetFeature()->Timestamp()<flat->Timestamp(),"Construction member left after flat pattern");
    Require(part->CurrentFeature()->Tag()==originalCurrent,"Creation changed original current feature");
    Require(Volume(FlatBody(flat))>sourceFlatVolume,"Existing flat pattern omitted new helper geometry");
    const double helperVolume=Volume(f.body),helperFlatVolume=Volume(FlatBody(flat));
    // Match the rollback around a native double-click edit. Keep the existing
    // downstream flat feature alive while NX rolls backward and forward.
    auto editMark=session->SetUndoMark(Session::MarkVisibilityVisible,"flat history edit");
    std::cout<<"HISTORY stage=rollback edit\n";
    s.width=20;EditFeature(custom,s);
    Check(UF_MODL_update());
    Require(FeatureSettings(custom).width==20 && Volume(f.body)>helperVolume,"Edit did not change the folded body");
    Require(Volume(FlatBody(flat))>helperFlatVolume,"Flat pattern did not update after helper edit");
    Require(custom->GetFeatureErrorMessages().empty() && flat->GetFeatureErrorMessages().empty(),"Edit broke downstream flat pattern");
    Require(part->CurrentFeature()->Tag()==originalCurrent,"Editing changed original current feature");
    session->UndoToMark(editMark,nullptr);session->DeleteUndoMark(editMark,nullptr);
    Require(FeatureSettings(custom).width==10 && std::abs(Volume(FlatBody(flat))-helperFlatVolume)<helperFlatVolume*1e-8,"Undo edit did not restore flat pattern");
    s.width=10;s.endDistance=30;s.reverseEnd=true;
    std::cout<<"HISTORY stage=repeat create\n";
    auto* second=dynamic_cast<Features::CustomFeature*>(NXObjectManager::Get(CreateFeature(PlanFaces({info},s).front(),s)));
    Require(second->Timestamp()>custom->Timestamp() && second->Timestamp()<flat->Timestamp(),"Repeated creation did not stay before flat pattern");
    if(foreignFlat) {
        Require(foreignFlat->Timestamp()<custom->Timestamp(),"Unrelated body's flat pattern used as insertion anchor");
        Require(std::abs(Volume(FlatBody(foreignFlat))-foreignVolume)<foreignVolume*1e-8,"Unrelated flat geometry changed");
    }
    session->UndoToMark(createMark,nullptr);session->DeleteUndoMark(createMark,nullptr);
    Require(std::abs(Volume(f.body)-sourceVolume)<sourceVolume*1e-8 && std::abs(Volume(FlatBody(flat))-sourceFlatVolume)<sourceFlatVolume*1e-8,"Undo create did not restore folded and flat bodies");
    std::cout<<"HISTORY stage=recreate after undo\n";
    s.reverseEnd=false;s.endDistance=5;CreateFeature(PlanFaces({info},s).front(),s);
    Check(UF_PART_save_as(path.c_str()));Check(UF_PART_close(partTag,0,1));
    UF_PART_load_status_t load={};Check(UF_PART_open(path.c_str(),&partTag,&load));UF_PART_free_load_status(&load);
    part=session->Parts()->Work();custom=nullptr;
    for(auto* feature:part->Features()->GetFeatures()) if(auto* c=dynamic_cast<Features::CustomFeature*>(feature)) custom=c;
    Require(custom,"Reopened auxiliary custom feature missing");
    std::cout<<"HISTORY stage=reopened edit\n";
    s.width=20;EditFeature(custom,s);Check(UF_MODL_update());
    for(auto* feature:part->Features()->GetFeatures()) Require(feature->GetFeatureErrorMessages().empty(),"Reopened history has dependency errors");
    Check(UF_PART_close(partTag,0,1));
    std::cout<<"PASS existing flat "<<(solidOnly?"solid":"pattern")<<" curved="<<curved<<" multibody="<<multipleBodies<<": ordered construction, regenerated flat geometry, rollback edit, repeat create, undo and reopen\n";
}
int main(int argc,char** argv) {
    try {
        Require(argc==2,"Provide a unique output directory");Check(UF_initialize());
        std::string out=argv[1];
        TestFlatHistory(out+"/bend-assist-flat-history.prt");
        TestFlatHistory(out+"/bend-assist-flat-arc.prt",1);
        TestFlatHistory(out+"/bend-assist-flat-multibody.prt",0,true);
        TestFlatHistory(out+"/bend-assist-flat-solid.prt",0,false,true);
        Test(out+"/bend-assist-mm.prt",false,false);
        Test(out+"/bend-assist-rotated.prt",true,false);
        Test(out+"/bend-assist-inch.prt",false,true);
        Test(out+"/bend-assist-convex.prt",false,false,1);
        Test(out+"/bend-assist-convex-rotated.prt",true,false,1);
        Test(out+"/bend-assist-convex-inch.prt",false,true,1);
        Test(out+"/bend-assist-concave.prt",false,false,2);
        Test(out+"/bend-assist-convex-endpoint.prt",false,false,1,true);
        Test(out+"/bend-assist-concave-rotated.prt",true,false,2);
        Test(out+"/bend-assist-concave-inch.prt",false,true,2);
        UF_terminate();return 0;
    }catch(const NXOpen::NXException& e){std::cerr<<"NX FAIL "<<e.ErrorCode()<<" "<<e.Message()<<'\n';}
     catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';}
    return 1;
}
