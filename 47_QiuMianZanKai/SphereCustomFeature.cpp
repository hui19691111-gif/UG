#include "SphereCustomFeature.hpp"
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
#include <fstream>
#include <filesystem>
#include <uf_modl.h>

namespace sphere_unfold {
using namespace NXOpen;
using namespace NXOpen::Features;
void RequireFeatureClass(){
    CustomFeatureClass* cls=nullptr;
    try{cls=Session::GetSession()->CustomFeatureClassManager()->GetClassFromName(featureClassName);}catch(const NXException&){}
    if(!cls)throw std::runtime_error("球面展开自定义特征尚未注册。请保存零件并重启 NX 后再使用。");
}
Settings ReadFeature(CustomFeature* feature,tag_t& cylinder,tag_t& sphere){
    auto* data=feature->FeatureData();Settings s;
    auto* c=data->CustomTagAttributeByName("Cylinder")->Value();auto* f=data->CustomTagAttributeByName("Sphere")->Value();
    if(!c||!f)throw std::runtime_error("原参考面已失效，请重新选择圆柱面和球面或环面。");
    cylinder=c->Tag();sphere=f->Tag();
    s.petals=data->CustomIntegerAttributeByName("Petals")->Value();
    s.gap=data->CustomDoubleAttributeByName("GapMm")->Value();s.relief=data->CustomDoubleAttributeByName("ReliefMm")->Value();
    s.flat=data->CustomLogicalAttributeByName("Flat")->Value();s.hideSource=data->CustomLogicalAttributeByName("HideSource")->Value();return s;
}
Result CreateFeature(const Plan& p,CustomFeature* edited,Session::UndoMarkId mark){
    RequireFeatureClass();auto* session=Session::GetSession();auto* part=session->Parts()->Work();
    EditWithRollbackManager* rollback=nullptr;CustomFeatureBuilder* builder=nullptr;
    try{
        const bool interactiveEdit=edited&&session->CustomFeatureClassManager()->GetEditedCustomFeature()==edited;
        if(edited&&!interactiveEdit)rollback=part->Features()->StartEditWithRollbackManager(edited,mark);
        builder=part->Features()->CreateCustomFeatureBuilder(edited);
        auto* data=edited?builder->FeatureData():nullptr;
        auto result=Create(p);
        auto* attrs=part->Features()->CustomAttributeCollection();
        std::vector<CustomAttribute::Property> input,output{CustomAttribute::PropertyIsOutputAttribute,CustomAttribute::PropertyIsOwnedAttribute};
        auto* members=data?data->CustomTagArrayAttributeByName("InternalFeatures"):attrs->CreateCustomTagArrayAttribute("InternalFeatures",output);
        auto* cylinder=data?data->CustomTagAttributeByName("Cylinder"):attrs->CreateCustomTagAttribute("Cylinder",input);cylinder->SetValue(NXObjectManager::Get(p.source.cylinder));
        auto* sphere=data?data->CustomTagAttributeByName("Sphere"):attrs->CreateCustomTagAttribute("Sphere",input);sphere->SetValue(NXObjectManager::Get(p.source.sphere));
        auto* petals=data?data->CustomIntegerAttributeByName("Petals"):attrs->CreateCustomIntegerAttribute("Petals",input);petals->SetValue(p.settings.petals);
        auto* gap=data?data->CustomDoubleAttributeByName("GapMm"):attrs->CreateCustomDoubleAttribute("GapMm",input);gap->SetValue(p.settings.gap);
        auto* relief=data?data->CustomDoubleAttributeByName("ReliefMm"):attrs->CreateCustomDoubleAttribute("ReliefMm",input);relief->SetValue(p.settings.relief);
        auto* flat=data?data->CustomLogicalAttributeByName("Flat"):attrs->CreateCustomLogicalAttribute("Flat",input);flat->SetValue(p.settings.flat);
        auto* hide=data?data->CustomLogicalAttributeByName("HideSource"):attrs->CreateCustomLogicalAttribute("HideSource",input);hide->SetValue(p.settings.hideSource);
        std::vector<TaggedObject*> objects;for(auto tag:result.members)objects.push_back(NXObjectManager::Get(tag));members->SetValues(objects);
        auto* cls=session->CustomFeatureClassManager()->GetClassFromName(featureClassName);
        if(!data)data=part->Features()->CustomFeatureDataCollection()->CreateData(cls,{members,cylinder,sphere,petals,gap,relief,flat,hide});
        builder->SetFeatureData(data);
        auto* feature=dynamic_cast<CustomFeature*>(builder->CommitFeature());builder->Destroy();builder=nullptr;
        if(!feature)throw std::runtime_error("创建球面展开自定义特征失败。");
        feature->SetName(NXString(std::string(p.source.IsTorus()?"环面展开_":"球面展开_")+std::to_string(p.settings.petals)+"瓣",NXString::UTF8));
        result.feature=feature->Tag();
        // NX owns rollback during a double-click edit and runs PreUpdate after
        // the Edit dialog closes. Do not start a nested rollback manager or
        // compare construction membership before that deferred update runs.
        if(interactiveEdit)feature->MakeCurrentFeature();
        if(edited&&!interactiveEdit)Check(UF_MODL_update());
        if(rollback){rollback->UpdateFeature(false);rollback->Stop();rollback->Destroy();rollback=nullptr;}
        if(edited&&!interactiveEdit){session->UpdateManager()->LogForUpdate(feature);if(session->UpdateManager()->DoUpdate(mark))throw std::runtime_error("球面展开自定义特征更新失败。");}
        if(!feature->GetFeatureErrorMessages().empty())throw std::runtime_error("球面展开自定义特征更新失败。");
        if(interactiveEdit)return result;
        std::set<tag_t> owned;for(auto* item:feature->GetConstructionFeatures())if(item&&item->GetFeature())owned.insert(item->GetFeature()->Tag());
        if(owned!=std::set<tag_t>(result.members.begin(),result.members.end())){
            std::ofstream log(std::filesystem::temp_directory_path()/"Zhihui-QiuMianZanKai.log",std::ios::app);
            log<<"ownership mismatch: expected="<<result.members.size()<<" actual="<<owned.size()<<" attributes="<<feature->FeatureData()->CustomTagArrayAttributeByName("InternalFeatures")->GetValues().size()<<" petals="<<feature->FeatureData()->CustomIntegerAttributeByName("Petals")->Value()<<'\n';
            throw std::runtime_error("球面展开内部特征未正确收纳，已取消本次操作。");
        }
        return result;
    }catch(...){
        if(builder)builder->Destroy();
        if(rollback){try{rollback->UpdateFeature(true);rollback->Stop();rollback->Destroy();}catch(...){}}
        throw;
    }
}
}
