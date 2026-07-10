#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>

#include "../../../protection/native/ZhihuiLicenseGuard.hpp"

#ifndef DllExport
#define DllExport __declspec(dllexport)
#endif

namespace
{
bool GetSpecFilePath(wchar_t path[MAX_PATH])
{
    return wcscpy_s(path, MAX_PATH, L"D:\\UG智辉钣金插件\\config\\FanTonGMK_specs.txt") == 0;
}
}

extern "C" DllExport void ufusr(char* param, int* retcode, int param_len)
{
    (void)param;
    (void)param_len;
    if (retcode != NULL)
    {
        *retcode = 0;
    }

    if (!zhihui_license_guard::EnsureAuthorized(L"ZHIHUI.FANTONGGMK_SPECS", L"FanTonGMK Specs"))
    {
        if (retcode != NULL)
        {
            *retcode = 1;
        }
        return;
    }

    wchar_t specPath[MAX_PATH] = {0};
    if (!GetSpecFilePath(specPath))
    {
        MessageBoxW(NULL, L"Could not resolve FanTonGMK spec table path.", L"FanTonGMK", MB_ICONERROR);
        if (retcode != NULL)
        {
            *retcode = 1;
        }
        return;
    }

    if (GetFileAttributesW(specPath) == INVALID_FILE_ATTRIBUTES)
    {
        MessageBoxW(NULL, L"FanTonGMK_specs.txt was not found in D:\\UG智辉钣金插件\\config.", L"FanTonGMK", MB_ICONERROR);
        if (retcode != NULL)
        {
            *retcode = 1;
        }
        return;
    }

    HINSTANCE result = ShellExecuteW(NULL, L"open", specPath, NULL, NULL, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(result) <= 32)
    {
        ShellExecuteW(NULL, L"open", L"notepad.exe", specPath, NULL, SW_SHOWNORMAL);
    }
}

extern "C" DllExport int ufusr_ask_unload()
{
    return 1;
}
