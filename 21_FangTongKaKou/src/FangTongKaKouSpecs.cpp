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
    HMODULE selfModule = NULL;
    if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&GetSpecFilePath),
            &selfModule))
    {
        return false;
    }

    DWORD length = GetModuleFileNameW(selfModule, path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
    {
        return false;
    }

    DWORD slash = length;
    while (slash > 0 && path[slash - 1] != L'\\' && path[slash - 1] != L'/')
    {
        --slash;
    }
    if (slash == 0)
    {
        return false;
    }

    const wchar_t* suffix = L"config\\FangTongKaKou_specs.txt";
    DWORD pos = slash;
    for (DWORD index = 0; suffix[index] != L'\0' && pos + 1 < MAX_PATH; ++index, ++pos)
    {
        path[pos] = suffix[index];
    }
    path[pos] = L'\0';
    return true;
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

    if (!zhihui_license_guard::EnsureAuthorized(L"ZHIHUI.FANGTONGKAKOU_SPECS", L"FangTongKaKou Specs"))
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
        MessageBoxW(NULL, L"Could not resolve FangTongKaKou spec table path.", L"FangTongKaKou", MB_ICONERROR);
        if (retcode != NULL)
        {
            *retcode = 1;
        }
        return;
    }

    if (GetFileAttributesW(specPath) == INVALID_FILE_ATTRIBUTES)
    {
        MessageBoxW(NULL, L"FangTongKaKou_specs.txt was not found in application\\config.", L"FangTongKaKou", MB_ICONERROR);
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
