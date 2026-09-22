#include "ZeWanFuZuFeature.hpp"
#include <NXOpen/Features_CustomFeature.hxx>
#include <NXOpen/Features_CustomAttribute.hxx>
#include <NXOpen/Features_CustomAttributeCollection.hxx>
#include <NXOpen/Features_CustomFeatureBuilder.hxx>
#include <NXOpen/Features_CustomFeatureClassManager.hxx>
#include <NXOpen/Features_CustomFeatureData.hxx>
#include <NXOpen/Features_CustomFeatureDataCollection.hxx>
#include <NXOpen/Features_CustomDoubleArrayAttribute.hxx>
#include <NXOpen/Features_CustomTagArrayAttribute.hxx>
#include <NXOpen/Features_FeatureCollection.hxx>
#include <NXOpen/Features_EditWithRollbackManager.hxx>
#include <NXOpen/Features_FlatPattern.hxx>
#include <NXOpen/Features_FlatSolid.hxx>
#include <NXOpen/Features_SheetMetal_SheetmetalManager.hxx>
#include <NXOpen/Features_SheetMetal_FlatPatternBuilder.hxx>
#include <NXOpen/Features_SheetMetal_FlatSolidBuilder.hxx>
#include <NXOpen/Body.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/SelectFace.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/Update.hxx>
#include <uf_modl.h>
#include <uf_obj.h>
#include <stdexcept>
namespace bend_assist {
using namespace NXOpen;
namespace {
std::vector<double> Values(const Settings& s) {return {s.width,s.bridgeWidth,s.inset,s.gap,s.endDistance,s.height,s.autoAlign?1.:0.,s.reverseEnd?1.:0.};}
std::vector<double> Frame(const FaceInfo& f) {
    return {f.origin.x,f.origin.y,f.origin.z,f.x.x,f.x.y,f.x.z,f.y.x,f.y.y,f.y.z,f.z.x,f.z.y,f.z.z,
        f.thickness,f.length,f.slope,f.autoTop,f.unitsPerMm,f.circular?1.:0.,f.centerX,f.centerY,f.radius,f.branch};
}
std::vector<TaggedObject*> Objects(const std::vector<tag_t>& tags) {
    std::vector<TaggedObject*> r; for(auto tag:tags) r.push_back(NXObjectManager::Get(tag)); return r;
}
Construction Members(Features::CustomFeature* f) {
    auto* d=f->FeatureData(); auto members=d->CustomTagArrayAttributeByName(internalAttribute)->GetValues();
    if(members.size()!=2 || !members[0] || !members[1]) throw std::runtime_error("辅助板内部特征不完整。");
    Construction r; r.extrude=members[0]->Tag();r.unite=members[1]->Tag();
    for(auto* c:d->CustomTagArrayAttributeByName(curvesAttribute)->GetValues()) if(c) r.curves.push_back(c->Tag());
    return r;
}
bool SameBody(Face* face,tag_t body) {return face && face->GetBody()->Tag()==body;}
Features::Feature* FirstFlatFeature(Part* part,tag_t body) {
    Features::Feature* first=nullptr;
    auto* manager=part->Features()->SheetmetalManager();
    for(auto* feature:part->Features()->GetFeatures()) {
        bool matches=false;
        if(auto* flat=dynamic_cast<Features::FlatPattern*>(feature)) {
            auto* builder=manager->CreateFlatPatternBuilder(flat);
            try {matches=SameBody(builder->UpwardFace()->Value(),body);builder->Destroy();}
            catch(...) {builder->Destroy();throw;}
        } else if(auto* flatSolid=dynamic_cast<Features::FlatSolid*>(feature)) {
            auto* builder=manager->CreateFlatSolidFeatureBuilder(flatSolid);
            try {matches=SameBody(builder->StationaryFace()->Value(),body);builder->Destroy();}
            catch(...) {builder->Destroy();throw;}
        }
        if(matches && (!first || feature->Timestamp()<first->Timestamp())) first=feature;
    }
    return first;
}
// Inserting the entire construction at its proper timestamp also keeps the
// flat pattern's internal extract/flat-solid chain downstream. Moving only the
// visible CustomFeature node cannot fix its boolean dependencies.
class InsertBeforeFlat {
    Features::Feature* original_=nullptr;
    bool moved_=false;
public:
    Features::Feature* flat=nullptr;
    InsertBeforeFlat(Part* part,tag_t body):original_(part->CurrentFeature()) {
        flat=FirstFlatFeature(part,body);
        if(!flat) return;
        Features::Feature* previous=nullptr;
        for(auto* feature:part->Features()->GetFeatures())
            if(!feature->IsInternal() && feature->Timestamp()<flat->Timestamp() &&
                (!previous || feature->Timestamp()>previous->Timestamp())) previous=feature;
        if(!previous) throw std::runtime_error("无法确定展开之前的辅助板插入位置。");
        moved_=true;
        try {previous->MakeCurrentFeature();} catch(...) {try {Restore();} catch(...) {} throw;}
    }
    void Restore() {
        if(moved_ && original_) original_->MakeCurrentFeature();
        moved_=false;
    }
    ~InsertBeforeFlat() {try {Restore();} catch(...) {}}
};
}
void RequireFeatureClass() {
    try { if(Session::GetSession()->CustomFeatureClassManager()->GetClassFromName(featureClass)) return; } catch(...) {}
    throw std::runtime_error("折弯辅助板自定义特征尚未注册，请保存零件并重启 NX 后使用。");
}
Settings FeatureSettings(Features::CustomFeature* f) {
    auto v=f->FeatureData()->CustomDoubleArrayAttributeByName("parametersMm")->GetValues();
    if(v.size()!=8) throw std::runtime_error("辅助板参数数据不完整。");
    Settings s; s.width=v[0];s.bridgeWidth=v[1];s.inset=v[2];s.gap=v[3];s.endDistance=v[4];s.height=v[5];s.autoAlign=v[6]!=0;s.reverseEnd=v[7]!=0;return s;
}
FaceInfo FeatureFace(Features::CustomFeature* f) {
    auto v=f->FeatureData()->CustomDoubleArrayAttributeByName("sourceFrame")->GetValues();
    if(v.size()!=22) throw std::runtime_error("辅助板定位数据不完整。");
    FaceInfo r; r.origin={v[0],v[1],v[2]};r.x={v[3],v[4],v[5]};r.y={v[6],v[7],v[8]};r.z={v[9],v[10],v[11]};
    r.thickness=v[12];r.length=v[13];r.slope=v[14];r.autoTop=v[15];r.unitsPerMm=v[16];r.circular=v[17]!=0;r.centerX=v[18];r.centerY=v[19];r.radius=v[20];r.branch=v[21];
    auto c=Members(f); Check(UF_MODL_ask_feat_body(c.unite,&r.body));return r;
}
tag_t CreateFeature(const Plan& plan,const Settings& settings) {
    RequireFeatureClass(); auto* session=Session::GetSession();auto* part=session->Parts()->Work();
    InsertBeforeFlat insertion(part,plan.face.body);
    auto construction=CreateConstruction(plan);
    auto* attributes=part->Features()->CustomAttributeCollection();
    using P=Features::CustomAttribute;
    const std::vector<P::Property> owned{P::PropertyIsOutputAttribute,P::PropertyIsOwnedAttribute};
    auto* internal=attributes->CreateCustomTagArrayAttribute(internalAttribute,owned);
    auto* curves=attributes->CreateCustomTagArrayAttribute(curvesAttribute,owned);
    auto* parameters=attributes->CreateCustomDoubleArrayAttribute("parametersMm",{});
    auto* frame=attributes->CreateCustomDoubleArrayAttribute("sourceFrame",{});
    auto* data=part->Features()->CustomFeatureDataCollection()->CreateData(
        session->CustomFeatureClassManager()->GetClassFromName(featureClass),{internal,curves,parameters,frame});
    internal->SetValues(Objects({construction.extrude,construction.unite}));curves->SetValues(Objects(construction.curves));
    parameters->SetValues(Values(settings));
    auto source=plan.face; if(settings.autoAlign) source.autoTop=plan.top;
    frame->SetValues(Frame(source));
    auto* builder=part->Features()->CreateCustomFeatureBuilder(nullptr);
    try {
        builder->SetFeatureData(data);auto* result=builder->CommitFeature();builder->Destroy();builder=nullptr;
        if(!result) throw std::runtime_error("创建折弯辅助板自定义特征失败。");
        result->SetName("折弯辅助板");
        if(insertion.flat && result->Timestamp()>=insertion.flat->Timestamp())
            throw std::runtime_error("辅助板未能插入展开特征之前，已停止创建。");
        insertion.Restore();Check(UF_MODL_update());
        return result->Tag();
    } catch(...) {if(builder) builder->Destroy();throw;}
}
void EditFeature(Features::CustomFeature* feature,const Settings& settings) {
    auto* session=Session::GetSession();auto* part=session->Parts()->Work();
    const bool interactive=session->CustomFeatureClassManager()->GetEditedCustomFeature()==feature;
    auto mark=session->SetUndoMark(interactive?Session::MarkVisibilityInvisible:Session::MarkVisibilityVisible,"Edit bend assist history");
    Features::EditWithRollbackManager* rollback=nullptr;
    Features::CustomFeatureBuilder* builder=nullptr;
    try {
        // NX already owns rollback during a native double-click edit. Other
        // callers need the same history position before querying the body.
        if(!interactive) rollback=part->Features()->StartEditWithRollbackManager(feature,mark);
        auto face=FeatureFace(feature);auto plans=PlanFaces({face},settings);
        if(plans.size()!=1) throw std::runtime_error("辅助板定位数据无效。");
        auto c=Members(feature);auto oldCurves=c.curves;
        EditConstruction(plans.front(),c);
        builder=part->Features()->CreateCustomFeatureBuilder(feature);
        auto* data=feature->FeatureData();
        data->CustomDoubleArrayAttributeByName("parametersMm")->SetValues(Values(settings));
        data->CustomTagArrayAttributeByName(curvesAttribute)->SetValues(Objects(c.curves));
        builder->SetFeatureData(data);builder->CommitFeature();builder->Destroy();builder=nullptr;
        Check(UF_MODL_update());
        if(rollback) {rollback->UpdateFeature(false);rollback->Stop();rollback->Destroy();rollback=nullptr;}
        if(!interactive) Check(UF_MODL_update());
        // Remove detached profile curves after replacing the owned outputs.
        // Roll forward first: NX may restore the old curves while ending edit.
        for(auto curve:oldCurves) {
            int status=UF_OBJ_ask_status(curve);
            if(status==UF_OBJ_ALIVE) Check(UF_OBJ_delete_object(curve));
        }
        Check(UF_MODL_update());
        session->DeleteUndoMark(mark,nullptr);
    } catch(...) {
        if(builder) {try {builder->Destroy();} catch(...) {}}
        if(rollback) {
            try {rollback->UpdateFeature(true);} catch(...) {}
            try {rollback->Stop();} catch(...) {}
            try {rollback->Destroy();} catch(...) {}
        }
        session->UndoToMark(mark,nullptr);session->DeleteUndoMark(mark,nullptr);throw;
    }
}
}
