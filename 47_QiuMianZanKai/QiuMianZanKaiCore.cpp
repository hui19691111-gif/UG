#include "SphereCustomFeature.hpp"

#include <NXOpen/Callback.hxx>
#include <NXOpen/Features_ConstructionFeatureData.hxx>
#include <NXOpen/Features_CustomFeature.hxx>
#include <NXOpen/Features_CustomFeatureClass.hxx>
#include <NXOpen/Features_CustomFeatureClassManager.hxx>
#include <NXOpen/Features_CustomFeatureData.hxx>
#include <NXOpen/Features_CustomFeatureInformationEvent.hxx>
#include <NXOpen/Features_CustomFeaturePreUpdateEvent.hxx>
#include <NXOpen/Features_CustomTagArrayAttribute.hxx>
#include <NXOpen/Features_Feature.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/Session.hxx>

#include <stdexcept>
#include <fstream>
#include <filesystem>

#ifndef DllExport
#define DllExport __declspec(dllexport)
#endif

namespace
{
void Log(const std::string& message)noexcept{try{std::ofstream(std::filesystem::temp_directory_path()/"Zhihui-SphereCore.log",std::ios::app)<<message<<'\n';}catch(...){}}
int PreUpdateCallback(
    NXOpen::Features::CustomFeaturePreUpdateEvent* event)
{
    try
    {
        NXOpen::Features::CustomFeature* customFeature =
            event == nullptr ? nullptr : event->GetCustomFeature();
        NXOpen::Features::CustomFeatureData* data =
            customFeature == nullptr ? nullptr : customFeature->FeatureData();
        if (event == nullptr || data == nullptr) return 1;

        const std::vector<NXOpen::TaggedObject*> objects =
            data->CustomTagArrayAttributeByName(
                    "InternalFeatures")
                ->GetValues();
        const std::vector<NXOpen::Features::ConstructionFeatureData*> existing =
            event->GetConstructionFeatures();
        std::vector<NXOpen::Features::ConstructionFeatureData*> construction;
        construction.reserve(objects.size());
        for (NXOpen::TaggedObject* object : objects)
        {
            auto* feature = dynamic_cast<NXOpen::Features::Feature*>(object);
            if (feature == nullptr) continue;
            NXOpen::Features::ConstructionFeatureData* item = nullptr;
            for (NXOpen::Features::ConstructionFeatureData* current : existing)
            {
                NXOpen::Features::Feature* currentFeature =
                    current == nullptr ? nullptr : current->GetFeature();
                if (currentFeature != nullptr &&
                    currentFeature->Tag() == feature->Tag())
                {
                    item = current;
                    break;
                }
            }
            if (item == nullptr)
                item = event->CreateConstructionFeatureData(feature);
            // These members form the live sheet-metal history. They remain
            // graphically enabled while NX hides their navigator nodes under
            // the owning custom feature.
            item->SetShowInGraphicView(true);
            construction.push_back(item);
        }
        event->SetConstructionFeatures(construction);
        return construction.empty() ? 1 : 0;
    }
    catch(const NXOpen::NXException& ex){Log(ex.Message());return 1;}
    catch (...)
    {
        Log("PreUpdate unknown failure");
        return 1;
    }
}

int InformationCallback(
    NXOpen::Features::CustomFeatureInformationEvent* event)
{
    try{
        if (event != nullptr)
            event->SetInformation(
                "球面/环面展开：自动识别板厚和内外侧，内部包含分瓣、根部连接、钣金转换及展开。双击编辑参数并重新生成；参考体变化后需编辑确认。\n");
        return 0;
    }catch(const NXOpen::NXException& ex){Log(ex.Message());return 1;}
    catch(...){Log("Information callback failed");return 1;}
}
}

extern "C" DllExport void ufusr(char*, int* returnCode, int)
{
    if (returnCode != nullptr) *returnCode = 0;
    try
    {
        NXOpen::Features::CustomFeatureClassManager* manager =
            NXOpen::Session::GetSession()->CustomFeatureClassManager();
        NXOpen::Features::CustomFeatureClass* featureClass =
            manager->GetClassFromName(
                sphere_unfold::featureClassName);
        if (featureClass == nullptr) throw std::runtime_error("class missing");
        featureClass->AddPreUpdateHandler(
            NXOpen::make_callback(&PreUpdateCallback));
        featureClass->AddInformationHandler(
            NXOpen::make_callback(&InformationCallback));
    }
    catch (const NXOpen::NXException& ex)
    {
        if (returnCode != nullptr) *returnCode = ex.ErrorCode();
    }
    catch (...)
    {
        if (returnCode != nullptr) *returnCode = -1;
    }
}

extern "C" DllExport int ufusr_ask_unload()
{
    return static_cast<int>(
        NXOpen::Session::LibraryUnloadOptionAtTermination);
}
