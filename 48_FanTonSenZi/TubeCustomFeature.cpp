#include "TubeCustomFeature.hpp"
#include <NXOpen/Features_CustomAttributeCollection.hxx>
#include <NXOpen/Features_ConstructionFeatureData.hxx>
#include <NXOpen/Features_CustomFeature.hxx>
#include <NXOpen/Features_CustomFeatureBuilder.hxx>
#include <NXOpen/Features_CustomFeatureClass.hxx>
#include <NXOpen/Features_CustomFeatureClassManager.hxx>
#include <NXOpen/Features_CustomFeatureData.hxx>
#include <NXOpen/Features_CustomFeatureDataCollection.hxx>
#include <NXOpen/Features_CustomDoubleAttribute.hxx>
#include <NXOpen/Features_CustomIntegerAttribute.hxx>
#include <NXOpen/Features_CustomLogicalAttribute.hxx>
#include <NXOpen/Features_CustomTagAttribute.hxx>
#include <NXOpen/Features_CustomTagArrayAttribute.hxx>
#include <NXOpen/Features_EditWithRollbackManager.hxx>
#include <NXOpen/Features_FeatureCollection.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/Update.hxx>
#include <stdexcept>
#include <set>
#include <uf_modl.h>

namespace tube_straighten {
using namespace NXOpen;
using namespace NXOpen::Features;
static std::set<std::string> AttributeNames(CustomFeatureData* data){std::set<std::string> result;if(data){std::vector<NXString> names;std::vector<CustomAttribute::Type> types;data->GetAllCustomAttributeNameAndTypes(names,types);for(const auto& name:names)result.insert(name.GetUTF8Text());}return result;}
void RequireFeatureClass(){
    CustomFeatureClass* cls=nullptr;
    try{cls=Session::GetSession()->CustomFeatureClassManager()->GetClassFromName(featureClassName);}catch(const NXException&){}
    if(!cls)throw std::runtime_error("方通/圆管伸直自定义特征尚未注册。请保存零件并重启 NX 后再使用。");
}
Settings ReadFeature(CustomFeature* feature,tag_t& face){
    auto* data=feature->FeatureData();Settings s;
    auto* selected=data->CustomTagAttributeByName("SourceFace")->Value();
    if(!selected)throw std::runtime_error("原管参考面已失效，请恢复原管参考面后再编辑。");
    face=selected->Tag();
    s.divisions=data->CustomIntegerAttributeByName("Divisions")->Value();
    s.radiusMm=data->CustomDoubleAttributeByName("RadiusMm")->Value();
    s.kFactor=data->CustomDoubleAttributeByName("KFactor")->Value();
    s.gapMm=data->CustomDoubleAttributeByName("GapMm")->Value();
    s.bridgeWidthMm=data->CustomDoubleAttributeByName("BridgeWidthMm")->Value();
    s.tubeKFactor=data->CustomDoubleAttributeByName("TubeKFactor")->Value();
    s.segmentArcs=data->CustomLogicalAttributeByName("SegmentArcs")->Value();
    s.cutSource=data->CustomLogicalAttributeByName("CutSource")->Value();
    s.hideSource=data->CustomLogicalAttributeByName("HideSource")->Value();
    if(AttributeNames(data).count("UseAnchor")){s.useAnchor=data->CustomLogicalAttributeByName("UseAnchor")->Value();s.anchorPoint={data->CustomDoubleAttributeByName("AnchorX")->Value(),data->CustomDoubleAttributeByName("AnchorY")->Value(),data->CustomDoubleAttributeByName("AnchorZ")->Value()};}return s;
}
FeatureResult CreateFeature(tag_t face,const Settings& settings,CustomFeature* edited,Session::UndoMarkId mark){
    RequireFeatureClass();auto* session=Session::GetSession();auto* part=session->Parts()->Work();
    EditWithRollbackManager* rollback=nullptr;CustomFeatureBuilder* builder=nullptr;
    try{
        const bool interactiveEdit=edited&&session->CustomFeatureClassManager()->GetEditedCustomFeature()==edited;
        if(edited&&!interactiveEdit)rollback=part->Features()->StartEditWithRollbackManager(edited,mark);
        builder=part->Features()->CreateCustomFeatureBuilder(edited);
        auto* data=edited?builder->FeatureData():nullptr;
        // Keep the old construction dormant while creating its replacement.
        // Rollback alone hides its geometry but still permits old source-cut
        // booleans to update against the new cuts. Suppression prevents that
        // dependency conflict; replacing the owned array removes the old
        // construction on commit. Caller undo restores it on any failure.
        if(data){
            uf_list_p_t list=nullptr;Check(UF_MODL_create_list(&list));
            try{for(auto* item:data->CustomTagArrayAttributeByName("InternalFeatures")->GetValues())if(item)Check(UF_MODL_put_list_item(list,item->Tag()));Check(UF_MODL_suppress_feature(list));UF_MODL_delete_list(&list);}catch(...){UF_MODL_delete_list(&list);throw;}
        }
        FeatureResult result;result.plan=MakePlan(InspectFace(face),settings);
        result.body=Create(result.plan,&result.members);
        const auto& p=result.plan;
        auto* attrs=part->Features()->CustomAttributeCollection();
        std::vector<CustomAttribute::Property> input,output{CustomAttribute::PropertyIsOutputAttribute,CustomAttribute::PropertyIsOwnedAttribute};
        auto* members=data?data->CustomTagArrayAttributeByName("InternalFeatures"):attrs->CreateCustomTagArrayAttribute("InternalFeatures",output);
        // This non-owned output tag is an edit-selection bookmark. An input
        // face dependency would make cutting that same tube update this feature
        // recursively while replacing its construction (NX update undo). The
        // construction booleans retain the real source-body history dependency.
        auto* selected=data?data->CustomTagAttributeByName("SourceFace"):attrs->CreateCustomTagAttribute("SourceFace",{CustomAttribute::PropertyIsOutputAttribute});selected->SetValue(NXObjectManager::Get(face));
        auto* divisions=data?data->CustomIntegerAttributeByName("Divisions"):attrs->CreateCustomIntegerAttribute("Divisions",input);divisions->SetValue(settings.divisions);
        std::vector<CustomAttribute*> attributes{members,selected,divisions};
        const auto names=AttributeNames(data);std::vector<CustomAttribute*> added;
        auto number=[&](const char* name,double value){bool exists=names.count(name)!=0;auto* a=exists?data->CustomDoubleAttributeByName(name):attrs->CreateCustomDoubleAttribute(name,input);a->SetValue(value);attributes.push_back(a);if(data&&!exists)added.push_back(a);};
        number("RadiusMm",settings.radiusMm);number("KFactor",settings.kFactor);number("GapMm",settings.gapMm);number("BridgeWidthMm",settings.bridgeWidthMm);number("TubeKFactor",settings.tubeKFactor);
        auto logical=[&](const char* name,bool value){bool exists=names.count(name)!=0;auto* a=exists?data->CustomLogicalAttributeByName(name):attrs->CreateCustomLogicalAttribute(name,input);a->SetValue(value);attributes.push_back(a);if(data&&!exists)added.push_back(a);};
        logical("SegmentArcs",settings.segmentArcs);logical("CutSource",settings.cutSource);logical("HideSource",settings.hideSource);
        logical("UseAnchor",settings.useAnchor);number("AnchorX",settings.anchorPoint.x);number("AnchorY",settings.anchorPoint.y);number("AnchorZ",settings.anchorPoint.z);
        std::vector<TaggedObject*> objects;for(auto tag:result.members)objects.push_back(NXObjectManager::Get(tag));members->SetValues(objects);
        auto* cls=session->CustomFeatureClassManager()->GetClassFromName(featureClassName);
        if(!data)data=part->Features()->CustomFeatureDataCollection()->CreateData(cls,attributes);
        else if(!added.empty())data->AddCustomAttributes(added);
        builder->SetFeatureData(data);
        auto* feature=dynamic_cast<CustomFeature*>(builder->CommitFeature());builder->Destroy();builder=nullptr;
        if(!feature)throw std::runtime_error("创建方通/圆管伸直自定义特征失败。");
        feature->SetName(NXString(std::string(p.source.round?"圆管伸直_":"方通伸直_")+(p.machineArcs.empty()?"":"弯管机_")+std::to_string(p.bends.size())+"切口",NXString::UTF8));
        result.feature=feature->Tag();
        // A native double-click already owns rollback; PreUpdate is deferred
        // until the edit dialog closes. Do not start a nested rollback manager.
        if(interactiveEdit)feature->MakeCurrentFeature();
        if(edited&&!interactiveEdit)Check(UF_MODL_update());
        if(rollback){rollback->UpdateFeature(false);rollback->Stop();rollback->Destroy();rollback=nullptr;}
        if(edited&&!interactiveEdit){session->UpdateManager()->LogForUpdate(feature);if(session->UpdateManager()->DoUpdate(mark))throw std::runtime_error("方通/圆管伸直自定义特征更新失败。");}
        if(!feature->GetFeatureErrorMessages().empty())throw std::runtime_error("方通/圆管伸直自定义特征更新失败。");
        if(!interactiveEdit){
            std::set<tag_t> owned;for(auto* item:feature->GetConstructionFeatures())if(item&&item->GetFeature())owned.insert(item->GetFeature()->Tag());
            if(owned!=std::set<tag_t>(result.members.begin(),result.members.end()))throw std::runtime_error("伸直内部特征未正确收纳，已取消本次操作。");
        }
        return result;
    }catch(...){
        if(builder)builder->Destroy();
        if(rollback){try{rollback->UpdateFeature(true);rollback->Stop();rollback->Destroy();}catch(...){}}
        throw;
    }
}
}
