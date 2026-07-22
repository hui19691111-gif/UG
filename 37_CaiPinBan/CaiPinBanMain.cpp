#include "CaiPinBan.hpp"

#include <NXOpen/NXException.hxx>
#include <NXOpen/NXMessageBox.hxx>
#include <NXOpen/UI.hxx>
#include <uf.h>

#ifndef DllExport
#define DllExport __declspec(dllexport)
#endif

extern "C" DllExport void ufusr(char*, int* returnCode, int)
{
    if (returnCode != nullptr)
    {
        *returnCode = 0;
    }
    const int status = UF_initialize();
    if (status != 0)
    {
        if (returnCode != nullptr) *returnCode = status;
        return;
    }
    try
    {
        CaiPinBanDialog dialog;
        dialog.Launch();
    }
    catch (const NXOpen::NXException& ex)
    {
        NXOpen::UI::GetUI()->NXMessageBox()->Show(
            "拆平板", NXOpen::NXMessageBox::DialogTypeError, ex.Message());
        if (returnCode != nullptr) *returnCode = ex.ErrorCode();
    }
    catch (...)
    {
        NXOpen::UI::GetUI()->NXMessageBox()->Show(
            "拆平板", NXOpen::NXMessageBox::DialogTypeError,
            "拆平板发生未处理异常。");
        if (returnCode != nullptr) *returnCode = -1;
    }
    UF_terminate();
}

extern "C" DllExport int ufusr_ask_unload(void)
{
    return UF_UNLOAD_IMMEDIATELY;
}
