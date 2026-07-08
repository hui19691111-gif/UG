#include "TwoPointSiBianUI.hpp"

#include <NXOpen/NXException.hxx>
#include <NXOpen/NXMessageBox.hxx>
#include <NXOpen/UI.hxx>

#include <uf.h>

#ifndef DllExport
#define DllExport __declspec(dllexport)
#endif

namespace
{
const char* NxErrorText(const NXOpen::NXException& ex, char message[133])
{
    UF_get_fail_message(ex.ErrorCode(), message);
    return message[0] != '\0' ? message : "2P_SiBian NXOpen exception.";
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
        TwoPointSiBianUI dialog;
        dialog.Launch();
    }
    catch (const NXOpen::NXException& ex)
    {
        char message[133] = {0};
        NXOpen::UI::GetUI()->NXMessageBox()->Show("2P_SiBian",
                                                  NXOpen::NXMessageBox::DialogTypeError,
                                                  NxErrorText(ex, message));
        if (returnCode != nullptr)
        {
            *returnCode = ex.ErrorCode();
        }
    }
    catch (...)
    {
        NXOpen::UI::GetUI()->NXMessageBox()->Show("2P_SiBian", NXOpen::NXMessageBox::DialogTypeError, "2P_SiBian unhandled exception.");
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
