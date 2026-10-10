#include "ZuJianZuanTi.hpp"

#include <NXOpen/LogFile.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/UI.hxx>
#include <uf.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <cwchar>

namespace {
using EnsureAuthorizedProc = int(__stdcall*)(const wchar_t*, const wchar_t*, wchar_t*, int);

HMODULE LoadGate() {
    constexpr const wchar_t* name = L"ZhaoFuNxLicenseGate.dll";
    if (HMODULE module = GetModuleHandleW(name)) return module;
    HMODULE self = nullptr;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&LoadGate), &self)) {
        wchar_t path[MAX_PATH]{};
        if (GetModuleFileNameW(self, path, MAX_PATH)) {
            wchar_t* slash = wcsrchr(path, L'\\');
            if (slash) {
                *(slash + 1) = L'\0';
                if (wcscat_s(path, name) == 0)
                    if (HMODULE local = LoadLibraryW(path)) return local;
            }
        }
    }
    return LoadLibraryW(name);
}

bool Authorized() {
    HMODULE gate = LoadGate();
    if (!gate) return false;
    auto* procedure = reinterpret_cast<EnsureAuthorizedProc>(
        GetProcAddress(gate, "ZfnxEnsureAuthorized"));
    if (!procedure) return false;
    wchar_t message[1024]{};
    return procedure(L"ZHIHUI.ZUJIANZHUANTI", L"组件转实体", message, 1024) == 1;
}

void ShowError(const wchar_t* message) {
    MessageBoxW(nullptr, message, L"组件转实体", MB_OK | MB_ICONERROR);
    try {
        NXOpen::Session::GetSession()->LogFile()->WriteLine(
            "ZuJianZuanTi: authorization or startup failed.");
    } catch (...) {}
}
}

extern "C" __declspec(dllexport) void ufusr(char*, int* returnCode, int) {
    if (returnCode) *returnCode = 0;
    if (!Authorized()) {
        ShowError(L"授权校验失败，命令未执行。");
        if (returnCode) *returnCode = 1;
        return;
    }
    const int status = UF_initialize();
    if (status != 0) {
        ShowError(L"UF 初始化失败，命令未执行。");
        if (returnCode) *returnCode = status;
        return;
    }
    try {
        ComponentToSolidDialog dialog;
        dialog.Launch();
    } catch (const NXOpen::NXException& ex) {
        ShowError(L"组件转实体启动失败。");
        try {
            NXOpen::Session::GetSession()->LogFile()->WriteLine(
                ex.Message() ? ex.Message() : "ZuJianZuanTi NX exception.");
        } catch (...) {}
        if (returnCode) *returnCode = ex.ErrorCode();
    } catch (const std::exception& ex) {
        ShowError(L"组件转实体启动失败。");
        try { NXOpen::Session::GetSession()->LogFile()->WriteLine(ex.what()); }
        catch (...) {}
        if (returnCode) *returnCode = 1;
    } catch (...) {
        ShowError(L"组件转实体发生未知错误。");
        if (returnCode) *returnCode = 1;
    }
    UF_terminate();
}

// Dialog command: unload immediately so the next deploy can replace the DLL.
extern "C" __declspec(dllexport) int ufusr_ask_unload() {
    return UF_UNLOAD_IMMEDIATELY;
}
