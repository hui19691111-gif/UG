#include "KonFanLaLiaoCustomFeatureShared.hpp"

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

#include <fstream>
#include <sstream>

#ifndef DllExport
#define DllExport __declspec(dllexport)
#endif

namespace
{
void AppendCoreLog(const std::string& message)
{
    std::ofstream stream(
        "D:\\UG智辉钣金插件\\logs\\KonFanLaLiao.log",
        std::ios::out | std::ios::app);
    if (stream)
    {
        stream << "[CORE] " << message << '\n';
    }
}

int PreUpdateCallback(
    NXOpen::Features::CustomFeaturePreUpdateEvent* event)
{
    try
    {
        NXOpen::Features::CustomFeature* customFeature =
            event == nullptr ? nullptr : event->GetCustomFeature();
        NXOpen::Features::CustomFeatureData* data =
            customFeature == nullptr ? nullptr : customFeature->FeatureData();
        if (event == nullptr || data == nullptr)
        {
            return 1;
        }
        const std::vector<NXOpen::TaggedObject*> objects =
            data->CustomTagArrayAttributeByName(
                    zhihui_konfan_laliao::kAttrInternalFeatures)
                ->GetValues();
        const std::vector<NXOpen::Features::ConstructionFeatureData*> existing =
            event->GetConstructionFeatures();
        std::vector<NXOpen::Features::ConstructionFeatureData*> internalData;
        if (!existing.empty())
        {
            // Repair nodes made by the previous build, which omitted the
            // final Rebend from construction data and registered it as an
            // output instead.  Rebuild in the saved per-body object order.
            for (std::size_t index = 0; index < objects.size(); ++index)
            {
                NXOpen::Features::Feature* feature = dynamic_cast<
                    NXOpen::Features::Feature*>(objects[index]);
                if (feature == nullptr)
                {
                    continue;
                }
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
                {
                    item = event->CreateConstructionFeatureData(feature);
                }
                // Every member modifies the same sheet-metal body.  NX must
                // keep the whole body-owning construction chain graphically
                // enabled; suppressing any earlier member can blank the final
                // body even when the Rebend itself is enabled.
                item->SetShowInGraphicView(true);
                internalData.push_back(item);
            }
            event->SetConstructionFeatures(internalData);
            std::ostringstream log;
            log << "PREUPDATE_REPAIR custom=" << customFeature->Tag()
                << " saved_objects=" << objects.size()
                << " existing=" << existing.size()
                << " visible_members=" << internalData.size();
            AppendCoreLog(log.str());
            return objects.empty() ? 1 : 0;
        }
        // Keep the complete per-body history in one custom-feature node.  The
        // last member is that body's Rebend and must remain visible; hiding it
        // makes the body disappear.  Earlier Unbend/cut members stay hidden.
        for (std::size_t index = 0; index < objects.size(); ++index)
        {
            NXOpen::TaggedObject* object = objects[index];
            NXOpen::Features::Feature* feature =
                dynamic_cast<NXOpen::Features::Feature*>(object);
            if (feature != nullptr)
            {
                NXOpen::Features::ConstructionFeatureData* item =
                    event->CreateConstructionFeatureData(feature);
                item->SetShowInGraphicView(true);
                internalData.push_back(item);
            }
        }
        event->SetConstructionFeatures(internalData);
        std::ostringstream log;
        log << "PREUPDATE_CREATE custom=" << customFeature->Tag()
            << " saved_objects=" << objects.size()
            << " visible_members=" << internalData.size();
        AppendCoreLog(log.str());
        return objects.empty() ? 1 : 0;
    }
    catch (...)
    {
        AppendCoreLog("PREUPDATE_FAIL exception=UNKNOWN");
        return 1;
    }
}

int InformationCallback(
    NXOpen::Features::CustomFeatureInformationEvent* event)
{
    if (event != nullptr)
    {
        event->SetInformation(
            "孔防拉槽：双击可重新打开孔防拉料检查对话框。");
    }
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
                zhihui_konfan_laliao::kFeatureClassName);
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
