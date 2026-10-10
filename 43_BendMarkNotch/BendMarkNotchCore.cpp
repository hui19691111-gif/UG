#include "BendMarkNotchCustomFeatureShared.hpp"

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

#include <cstdio>
#include <stdexcept>
#include <string>

#ifndef DllExport
#define DllExport __declspec(dllexport)
#endif

namespace
{
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
                    zhihui_bend_mark_notch::kAttrInternalFeatures)
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
        // An empty child list used to fail this feature. That failure makes
        // NX undo the whole edit and report "Update undo happened." Keeping
        // the already built notches is more important than adopting them.
        {
            FILE* log = nullptr;
            if (_wfopen_s(&log,
                    L"D:\\UG智辉钣金插件\\logs\\BendMarkNotch_debug.log",
                    L"ab") == 0 && log != nullptr)
            {
                const std::string line =
                    "BendMarkNotchCore: PreUpdate feature=" +
                    std::to_string(customFeature->Tag()) +
                    " objects=" + std::to_string(objects.size()) +
                    " existing=" + std::to_string(existing.size()) +
                    " construction=" + std::to_string(construction.size()) +
                    "\r\n";
                fputs(line.c_str(), log);
                fclose(log);
            }
        }
        return 0;
    }
    catch (...)
    {
        return 1;
    }
}

int InformationCallback(
    NXOpen::Features::CustomFeatureInformationEvent* event)
{
    if (event != nullptr)
        event->SetInformation(
            "折弯标记缺口：内部包含伸直、缺口拉伸、布尔减和重新折弯。\n");
    return 0;
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
                zhihui_bend_mark_notch::kFeatureClassName);
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
