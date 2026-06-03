#include "TwoPointBiLanCao.hpp"
#include "../../protection/native/ZhihuiLicenseGuard.hpp"

#include <NXOpen/NXException.hxx>
#include <NXOpen/NXMessageBox.hxx>
#include <NXOpen/UI.hxx>

#include <uf.h>

#ifndef DllExport
#define DllExport __declspec(dllexport)
#endif

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

    if (!zhihui_license_guard::EnsureAuthorized(L"ZHIHUI.TWOPOINTBILANCAO", L"TwoPointBiLanCao"))
    {
        UF_terminate();
        if (returnCode != nullptr)
        {
            *returnCode = 1;
        }
        return;
    }

    try
    {
        TwoPointBiLanCaoDialog dialog;
        dialog.Launch();
    }
    catch (const NXOpen::NXException& ex)
    {
        NXOpen::UI::GetUI()->NXMessageBox()->Show("2P_BiLanCao", NXOpen::NXMessageBox::DialogTypeError, ex.Message());
        if (returnCode != nullptr)
        {
            *returnCode = ex.ErrorCode();
        }
    }
    catch (...)
    {
        NXOpen::UI::GetUI()->NXMessageBox()->Show("2P_BiLanCao", NXOpen::NXMessageBox::DialogTypeError, "2P_BiLanCao unhandled exception.");
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
