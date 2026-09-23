#include "ZeWanMoNi.hpp"

#include <NXOpen/NXException.hxx>
#include <NXOpen/NXMessageBox.hxx>
#include <NXOpen/UI.hxx>
#include <uf.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <cwchar>
#include <exception>

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
    return {};
}

bool EnsureAuthorized(std::wstring& error)
{
#ifndef ZH_PROTECTED_BUILD
    return true;
#else
    const LicenseGateHandle gate = LoadLicenseGate();
    if (gate.module == nullptr)
    {
        error = L"无法加载折弯模拟授权模块。";
        return false;
    }
    const auto procedure = reinterpret_cast<EnsureAuthorizedProc>(
        GetProcAddress(gate.module, "ZfnxEnsureAuthorized"));
    if (procedure == nullptr)
    {
        if (gate.owned) FreeLibrary(gate.module);
        error = L"折弯模拟授权模块缺少校验入口。";
        return false;
    }
    wchar_t message[1024] = {};
    const int result = procedure(L"ZHIHUI.CHAIJIJIA", L"折弯模拟",
                                 message, 1024);
    if (gate.owned) FreeLibrary(gate.module);
    if (result == 1) return true;
    error = message[0] ? message : L"折弯模拟授权校验未通过。";
    return false;
#endif
}
}

extern "C" DllExport void ufusr(char*, int* returnCode, int)
{
    if (returnCode != nullptr) *returnCode = 0;
    std::wstring authorizationError;
    if (!EnsureAuthorized(authorizationError))
    {
        MessageBoxW(nullptr, authorizationError.c_str(), L"折弯模拟", MB_OK | MB_ICONERROR);
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
        ZeWanMoNiDialog dialog;
        dialog.Launch();
    }
    catch (const NXOpen::NXException& ex)
    {
        NXOpen::UI::GetUI()->NXMessageBox()->Show(
            "折弯模拟", NXOpen::NXMessageBox::DialogTypeError,
            ex.Message());
        if (returnCode != nullptr) *returnCode = ex.ErrorCode();
    }
    catch (const std::exception& ex)
    {
        NXOpen::UI::GetUI()->NXMessageBox()->Show(
            "折弯模拟", NXOpen::NXMessageBox::DialogTypeError, ex.what());
        if (returnCode != nullptr) *returnCode = -1;
    }
    catch (...)
    {
        NXOpen::UI::GetUI()->NXMessageBox()->Show(
            "折弯模拟", NXOpen::NXMessageBox::DialogTypeError,
            "折弯模拟发生未处理异常。");
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
    // selections and operation tags are released by ZeWanMoNiDialog's
    // destructor before NX calls this immediate-unload hook.
}
