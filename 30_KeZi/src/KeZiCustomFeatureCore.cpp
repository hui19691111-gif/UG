#include <windows.h>

#include <NXOpen/Callback.hxx>
#include <NXOpen/Features_CustomFeature.hxx>
#include <NXOpen/Features_CustomFeatureClass.hxx>
#include <NXOpen/Features_CustomFeatureClassManager.hxx>
#include <NXOpen/Features_CustomFeatureInformationEvent.hxx>
#include <NXOpen/Features_CustomFeaturePreUpdateEvent.hxx>
#include <NXOpen/Session.hxx>

#include <uf.h>

#include "KeZiCustomFeatureShared.hpp"

using namespace NXOpen;

#ifdef _WIN32
#define DllExport __declspec(dllexport)
#else
#define DllExport
#endif

namespace
{
int PreUpdateCallback(Features::CustomFeaturePreUpdateEvent* event)
{
    if (event == nullptr || event->GetCustomFeature() == nullptr)
    {
        return 1;
    }

    using BuildCallback = int(__cdecl*)(void*);
    HMODULE uiModule = GetModuleHandleW(L"KeZi.dll");
    if (uiModule != nullptr)
    {
        BuildCallback build = reinterpret_cast<BuildCallback>(
            GetProcAddress(uiModule, "ZhihuiKeZiBuildCustomFeature"));
        if (build != nullptr)
        {
            return build(event);
        }
    }

    const std::vector<Features::ConstructionFeatureData*> existing =
        event->GetConstructionFeatures();
    if (!existing.empty())
    {
        event->SetConstructionFeatures(existing);
        return 0;
    }

    event->GetCustomFeature()->LogDiagnostic(
        1,
        "The single-line engraving construction callback is unavailable.",
        Features::Feature::DiagnosticTypeWarning);
    return 1;
}

int InformationCallback(Features::CustomFeatureInformationEvent* event)
{
    if (event == nullptr)
    {
        return 1;
    }
    event->SetInformation(
        "单线刻字：MODERN 单线字体曲线、对称字宽拉伸和一次性求差组成的自定义特征。");
    return 0;
}
}

extern "C" DllExport void ufusr(char* /*param*/, int* returnCode, int /*paramLength*/)
{
    if (returnCode != nullptr)
    {
        *returnCode = 0;
    }
    const int initializeStatus = UF_initialize();
    if (initializeStatus != 0)
    {
        if (returnCode != nullptr) { *returnCode = initializeStatus; }
        return;
    }

    try
    {
        Features::CustomFeatureClassManager* manager =
            Session::GetSession()->CustomFeatureClassManager();
        Features::CustomFeatureClass* featureClass = manager->GetClassFromName(
            zhihui_kezi_custom_feature::kFeatureClassName);
        featureClass->AddPreUpdateHandler(make_callback(&PreUpdateCallback));
        featureClass->AddInformationHandler(make_callback(&InformationCallback));
    }
    catch (...)
    {
        if (returnCode != nullptr) { *returnCode = -1; }
    }
    UF_terminate();
}

extern "C" DllExport int ufusr_ask_unload(void)
{
    return Session::LibraryUnloadOptionAtTermination;
}
