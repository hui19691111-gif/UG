#include "CaiPinBan.hpp"

#include <NXOpen/NXException.hxx>
#include <NXOpen/NXMessageBox.hxx>
#include <NXOpen/UI.hxx>
#include <uf.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#ifndef DllExport
#define DllExport __declspec(dllexport)
#endif

namespace zhihui_license_guard
{
using EnsureAuthorizedProc = int (__stdcall *)(const wchar_t*, const wchar_t*, wchar_t*, int);

HMODULE LoadGate()
{
    constexpr const wchar_t* moduleName = L"ZhaoFuNxLicenseGate.dll";
    if (HMODULE module = GetModuleHandleW(moduleName)) return module;

    HMODULE self = nullptr;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                              GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          reinterpret_cast<LPCWSTR>(&LoadGate), &self))
    {
        wchar_t path[MAX_PATH] = {};
        const DWORD length = GetModuleFileNameW(self, path, MAX_PATH);
        if (length > 0 && length < MAX_PATH)
        {
            wchar_t* slash = wcsrchr(path, L'\\');
            if (slash != nullptr)
            {
                *(slash + 1) = L'\0';
                if (wcscat_s(path, moduleName) == 0)
                    if (HMODULE module = LoadLibraryW(path)) return module;
            }
        }
    }
    return LoadLibraryW(moduleName);
}

bool EnsureAuthorized()
{
    HMODULE module = LoadGate();
    if (module == nullptr) return false;
    auto proc = reinterpret_cast<EnsureAuthorizedProc>(
        GetProcAddress(module, "ZfnxEnsureAuthorized"));
    if (proc == nullptr) return false;
    wchar_t message[1024] = {};
    return proc(L"ZHIHUI.CAIPINBAN", L"拆平板", message, 1024) == 1 &&
           proc(L"ZHIHUI.CAIPINBAN", L"拆平板", message, 1024) == 1;
}
}

extern "C" DllExport void ufusr(char*, int* returnCode, int)
{
    if (returnCode != nullptr) *returnCode = 0;
    if (!zhihui_license_guard::EnsureAuthorized())
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
            "拆平板", NXOpen::NXMessageBox::DialogTypeError, "拆平板发生未处理异常。");
        if (returnCode != nullptr) *returnCode = -1;
    }
    UF_terminate();
}

extern "C" DllExport int ufusr_ask_unload(void)
{
    return UF_UNLOAD_IMMEDIATELY;
}
