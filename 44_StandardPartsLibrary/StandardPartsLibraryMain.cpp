#include "StandardPartsLibrary.hpp"

#include <uf.h>
#include <Windows.h>
#include <cwchar>

#ifndef DllExport
#define DllExport __declspec(dllexport)
#endif

namespace
{
using EnsureAuthorizedProc = int(__stdcall*)(const wchar_t*, const wchar_t*, wchar_t*, int);

bool EnsureAuthorized()
{
#ifndef ZH_PROTECTED_BUILD
    return true;
#else
    constexpr const wchar_t* dllName = L"ZhaoFuNxLicenseGate.dll";
    HMODULE gate = GetModuleHandleW(dllName);
    bool owned = false;
    if (gate == nullptr)
    {
        HMODULE self = nullptr;
        wchar_t path[MAX_PATH] = {};
        if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                  GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                              reinterpret_cast<LPCWSTR>(&EnsureAuthorized), &self) &&
            GetModuleFileNameW(self, path, MAX_PATH) > 0)
        {
            if (wchar_t* slash = wcsrchr(path, L'\\'))
            {
                *(slash + 1) = L'\0';
                wcscat_s(path, dllName);
                gate = LoadLibraryW(path);
                owned = gate != nullptr;
            }
        }
        if (gate == nullptr)
        {
            gate = LoadLibraryW(dllName);
            owned = gate != nullptr;
        }
    }
    if (gate == nullptr) return false;
    const auto authorize = reinterpret_cast<EnsureAuthorizedProc>(
        GetProcAddress(gate, "ZfnxEnsureAuthorized"));
    wchar_t message[1024] = {};
    const bool allowed = authorize != nullptr &&
        authorize(L"ZHIHUI.CHAIJIJIA", L"智辉标准件库", message, 1024) == 1;
    if (owned) FreeLibrary(gate);
    return allowed;
#endif
}
}

extern "C" DllExport void ufusr(char*, int* returnCode, int)
{
    if (returnCode != nullptr) *returnCode = 0;
    if (!EnsureAuthorized())
    {
        if (returnCode != nullptr) *returnCode = 1;
        return;
    }
    const int status = UF_initialize();
    if (status != 0)
    {
        if (returnCode != nullptr) *returnCode = status;
        return;
    }
    bool keepUfInitialized = false;
    try
    {
        const int result = LaunchStandardPartsLibrary(keepUfInitialized);
        if (returnCode != nullptr) *returnCode = result;
    }
    catch (...)
    {
        MessageBoxW(GetForegroundWindow(), L"智辉标准件库发生未处理异常。",
                    L"智辉标准件库", MB_OK | MB_ICONERROR);
        if (returnCode != nullptr) *returnCode = -1;
    }
    if (!keepUfInitialized) UF_terminate();
}

extern "C" DllExport int ufusr_ask_unload()
{
    // LaunchStandardPartsLibrary owns a private module reference while its
    // modeless window is alive and releases it only after WM_NCDESTROY returns.
    return UF_UNLOAD_IMMEDIATELY;
}

extern "C" DllExport void ufusr_cleanup()
{
    // UI state, selections, UF/OLE and GDI resources are released by
    // WM_NCDESTROY before the private DLL reference is released.
}
