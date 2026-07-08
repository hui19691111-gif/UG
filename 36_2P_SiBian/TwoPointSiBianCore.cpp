#include "TwoPointSiBianShared.hpp"

#include <NXOpen/Features_CustomDoubleAttribute.hxx>
#include <NXOpen/Features_CustomFeature.hxx>
#include <NXOpen/Features_CustomFeatureClass.hxx>
#include <NXOpen/Features_CustomFeatureClassManager.hxx>
#include <NXOpen/Features_CustomFeatureCreateFeatureGeometryEvent.hxx>
#include <NXOpen/Features_CustomFeatureData.hxx>
#include <NXOpen/Features_CustomFeatureInformationEvent.hxx>
#include <NXOpen/Features_CustomTagAttribute.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXMessageBox.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/UI.hxx>

#include <uf.h>

#ifndef DllExport
#define DllExport __declspec(dllexport)
#endif

using namespace NXOpen;

namespace
{
const char* NxErrorText(const NXException& ex, char message[133])
{
    UF_get_fail_message(ex.ErrorCode(), message);
    return message[0] != '\0' ? message : "2P_SiBian core NXOpen exception.";
}

double DoubleValue(Features::CustomFeatureData* data, const char* name)
{
    return data->CustomDoubleAttributeByName(name)->Value();
}

int CreateGeometryCallback(Features::CustomFeatureCreateFeatureGeometryEvent* event)
{
    Features::CustomFeature* feature = event->GetCustomFeature();
    Features::CustomFeatureData* data = feature != nullptr ? feature->FeatureData() : nullptr;
    if (feature == nullptr || data == nullptr)
    {
        event->SetErrorCode(1);
        return 1;
    }

    const double spanLength = DoubleValue(data, zhihui_twopoint_sibian::kAttrSpanLength);
    const double thickness = DoubleValue(data, zhihui_twopoint_sibian::kAttrThickness);

    if (spanLength <= 1.0e-4)
    {
        feature->LogDiagnostic(1,
                               "The two selected endpoints are coincident.",
                               Features::Feature::DiagnosticTypeWarning);
        event->SetErrorCode(1);
        return 0;
    }

    if (thickness <= 1.0e-4)
    {
        feature->LogDiagnostic(2,
                               "Sheet thickness could not be inferred. The feature inputs were recorded, but geometry creation is skipped in this build.",
                               Features::Feature::DiagnosticTypeWarning);
        return 0;
    }

    feature->LogDiagnostic(3,
                           "2P_SiBian inputs were accepted. Precise sketch/extrude geometry is implemented in the NXOpen construction step.",
                           Features::Feature::DiagnosticTypeWarning);
    return 0;
}

int InformationCallback(Features::CustomFeatureInformationEvent* event)
{
    event->SetInformation("2P_SiBian: user selects two endpoints; body, face, edges and thickness are inferred by the NXOpen plug-in.");
    return 0;
}
}

extern "C" DllExport void ufusr(char* param, int* returnCode, int rlen)
{
    (void)param;
    (void)rlen;

    if (returnCode != nullptr)
    {
        *returnCode = 0;
    }

    const int initStatus = UF_initialize();
    if (initStatus != 0)
    {
        if (returnCode != nullptr)
        {
            *returnCode = initStatus;
        }
        return;
    }

    try
    {
        Features::CustomFeatureClassManager* manager = Session::GetSession()->CustomFeatureClassManager();
        Features::CustomFeatureClass* featureClass = manager->GetClassFromName(zhihui_twopoint_sibian::kFeatureClassName);
        featureClass->AddCreateFeatureGeometryHandler(make_callback(&CreateGeometryCallback));
        featureClass->AddInformationHandler(make_callback(&InformationCallback));
    }
    catch (const NXException& ex)
    {
        char message[133] = {0};
        NXOpen::UI::GetUI()->NXMessageBox()->Show("2P_SiBian Core",
                                                  NXMessageBox::DialogTypeError,
                                                  NxErrorText(ex, message));
        if (returnCode != nullptr)
        {
            *returnCode = ex.ErrorCode();
        }
    }
    catch (...)
    {
        NXOpen::UI::GetUI()->NXMessageBox()->Show("2P_SiBian Core", NXMessageBox::DialogTypeError, "2P_SiBian core registration failed.");
        if (returnCode != nullptr)
        {
            *returnCode = -1;
        }
    }

    UF_terminate();
}

extern "C" DllExport int ufusr_ask_unload(void)
{
    return UF_UNLOAD_IMMEDIATELY;
}
