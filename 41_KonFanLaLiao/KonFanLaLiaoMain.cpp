#include "KonFanLaLiao.hpp"

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
#include <Windows.h>

#include <cwchar>

#ifndef DllExport
#define DllExport __declspec(dllexport)
#endif

namespace
{
using EnsureAuthorizedProc =
    int(__stdcall*)(const wchar_t*, const wchar_t*, wchar_t*, int);

HMODULE LoadLicenseGate()
{
    constexpr const wchar_t* moduleName = L"ZhaoFuNxLicenseGate.dll";
    if (HMODULE existing = GetModuleHandleW(moduleName))
    {
        return existing;
    }

    HMODULE self = nullptr;
    if (GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&LoadLicenseGate), &self))
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
                {
                    if (HMODULE local = LoadLibraryW(path))
                    {
                        return local;
                    }
                }
            }
        }
    }
    return LoadLibraryW(moduleName);
}

bool EnsureAuthorized()
{
#ifndef ZH_PROTECTED_BUILD
    return true;
#else
    HMODULE module = LoadLicenseGate();
    if (module == nullptr)
    {
        return false;
    }
    const auto procedure = reinterpret_cast<EnsureAuthorizedProc>(
        GetProcAddress(module, "ZfnxEnsureAuthorized"));
    if (procedure == nullptr)
    {
        return false;
    }
    wchar_t message[1024] = {};
    return procedure(
               L"ZHIHUI.CHAIJIJIA",
               L"孔防拉料检查", message, 1024) == 1;
#endif
}
}

extern "C" DllExport void ufusr(char*, int* returnCode, int)
{
    if (returnCode != nullptr)
    {
        *returnCode = 0;
    }
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
    try
    {
        KonFanLaLiaoDialog dialog;
        dialog.Launch();
    }
    catch (const NXOpen::NXException& ex)
    {
        NXOpen::UI::GetUI()->NXMessageBox()->Show(
            "孔防拉料检查",
            NXOpen::NXMessageBox::DialogTypeError,
            ex.Message());
        if (returnCode != nullptr) *returnCode = ex.ErrorCode();
    }
    catch (...)
    {
        NXOpen::UI::GetUI()->NXMessageBox()->Show(
            "孔防拉料检查",
            NXOpen::NXMessageBox::DialogTypeError,
            "孔防拉料检查发生未处理异常。");
        if (returnCode != nullptr) *returnCode = -1;
    }
    UF_terminate();
}

extern "C" DllExport int ufusr_ask_unload()
{
    return UF_UNLOAD_IMMEDIATELY;
}
