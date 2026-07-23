#include "CaiR1CustomFeatureShared.hpp"

#include <NXOpen/Callback.hxx>
#include <NXOpen/Features_ConstructionFeatureData.hxx>
#include <NXOpen/Features_CustomFeature.hxx>
#include <NXOpen/Features_CustomFeatureClass.hxx>
#include <NXOpen/Features_CustomFeatureClassManager.hxx>
#include <NXOpen/Features_CustomFeatureInformationEvent.hxx>
#include <NXOpen/Features_CustomFeaturePreUpdateEvent.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/Session.hxx>

#include <Windows.h>
#include <uf.h>

#ifndef DllExport
#define DllExport __declspec(dllexport)
#endif

namespace
{
int PreUpdateCallback(NXOpen::Features::CustomFeaturePreUpdateEvent* event)
{
    if (event == nullptr || event->GetCustomFeature() == nullptr)
    {
        return 1;
    }

    using BuildCallback = int(__cdecl*)(void*);
    HMODULE uiModule = GetModuleHandleW(L"CaiR1.dll");
    if (uiModule != nullptr)
    {
        BuildCallback build = reinterpret_cast<BuildCallback>(
            GetProcAddress(uiModule, "ZhihuiCaiR1BuildCustomFeature"));
        if (build != nullptr)
        {
            return build(event);
        }
    }

    const std::vector<NXOpen::Features::ConstructionFeatureData*> existing =
        event->GetConstructionFeatures();
    if (!existing.empty())
    {
        event->SetConstructionFeatures(existing);
        return 0;
    }

    event->GetCustomFeature()->LogDiagnostic(
        1,
        "The CaiR1 construction callback is unavailable.",
        NXOpen::Features::Feature::DiagnosticTypeWarning);
    return 1;
}

int InformationCallback(
    NXOpen::Features::CustomFeatureInformationEvent* event)
{
    event->SetInformation(
        "CaiR1: split-corner construction is contained in one feature node.");
    return 0;
}
}

extern "C" DllExport void ufusr(char*, int* returnCode, int)
{
    if (returnCode != nullptr) *returnCode = 0;
    const int status = UF_initialize();
    if (status != 0)
    {
        if (returnCode != nullptr) *returnCode = status;
        return;
    }
    try
    {
        NXOpen::Features::CustomFeatureClassManager* manager =
            NXOpen::Session::GetSession()->CustomFeatureClassManager();
        NXOpen::Features::CustomFeatureClass* featureClass =
            manager->GetClassFromName(zhihui_cair1::kFeatureClassName);
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
    UF_terminate();
}

extern "C" DllExport int ufusr_ask_unload(void)
{
    return NXOpen::Session::LibraryUnloadOptionAtTermination;
}
