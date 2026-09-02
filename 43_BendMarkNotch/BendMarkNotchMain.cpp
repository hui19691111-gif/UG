#include "BendMarkNotch.hpp"

#include <NXOpen/NXException.hxx>
#include <NXOpen/NXMessageBox.hxx>
#include <NXOpen/UI.hxx>
#include <uf.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
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

struct LicenseGateHandle
{
    HMODULE module = nullptr;
    bool owned = false;
};

LicenseGateHandle LoadLicenseGate()
{
    constexpr const wchar_t* name = L"ZhaoFuNxLicenseGate.dll";
    if (HMODULE existing = GetModuleHandleW(name)) return {existing, false};
    HMODULE self = nullptr;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                              GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          reinterpret_cast<LPCWSTR>(&LoadLicenseGate), &self))
    {
        wchar_t path[MAX_PATH] = {};
        const DWORD length = GetModuleFileNameW(self, path, MAX_PATH);
        if (length > 0 && length < MAX_PATH)
        {
            if (wchar_t* slash = wcsrchr(path, L'\\'))
            {
                *(slash + 1) = L'\0';
                if (wcscat_s(path, name) == 0)
                    if (HMODULE local = LoadLibraryW(path)) return {local, true};
            }
        }
    }
    return {LoadLibraryW(name), true};
}

bool EnsureAuthorized()
{
#ifndef ZH_PROTECTED_BUILD
    return true;
#else
    const LicenseGateHandle gate = LoadLicenseGate();
    if (gate.module == nullptr) return false;
    const auto procedure = reinterpret_cast<EnsureAuthorizedProc>(
        GetProcAddress(gate.module, "ZfnxEnsureAuthorized"));
    if (procedure == nullptr)
    {
        if (gate.owned) FreeLibrary(gate.module);
        return false;
    }
    wchar_t message[1024] = {};
    const bool authorized =
        procedure(L"ZHIHUI.CHAIJIJIA", L"折弯标记缺口",
                  message, 1024) == 1;
    if (gate.owned) FreeLibrary(gate.module);
    return authorized;
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
    try
    {
        BendMarkNotchDialog dialog;
        dialog.Launch();
    }
    catch (const NXOpen::NXException& ex)
    {
        NXOpen::UI::GetUI()->NXMessageBox()->Show(
            "折弯标记缺口", NXOpen::NXMessageBox::DialogTypeError,
            ex.Message());
        if (returnCode != nullptr) *returnCode = ex.ErrorCode();
    }
    catch (...)
    {
        NXOpen::UI::GetUI()->NXMessageBox()->Show(
            "折弯标记缺口", NXOpen::NXMessageBox::DialogTypeError,
            "折弯标记缺口发生未处理异常。");
        if (returnCode != nullptr) *returnCode = -1;
    }
    UF_terminate();
}

extern "C" DllExport int ufusr_ask_unload()
{
    return UF_UNLOAD_IMMEDIATELY;
}

extern "C" DllExport void ufusr_cleanup()
{
    // The command owns no process-wide UI state. All dialog blocks, cached
    // selections and operation tags are released by BendMarkNotchDialog's
    // destructor before NX calls this immediate-unload hook.
}
