#include "TianYuanDiFan.hpp"

#include <NXOpen/NXException.hxx>
#include <NXOpen/UI.hxx>
#include <uf.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <cwchar>
#include <exception>
#include <string>

#ifndef DllExport
#define DllExport __declspec(dllexport)
#endif

namespace {
using EnsureAuthorizedProc = int(__stdcall*)(const wchar_t*, const wchar_t*, wchar_t*, int);

struct LicenseGateHandle {
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
                          reinterpret_cast<LPCWSTR>(&LoadLicenseGate), &self)) {
        wchar_t path[MAX_PATH] = {};
        const DWORD length = GetModuleFileNameW(self, path, MAX_PATH);
        if (length > 0 && length < MAX_PATH) {
            if (wchar_t* slash = wcsrchr(path, L'\\')) {
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
    if (procedure == nullptr) {
        if (gate.owned) FreeLibrary(gate.module);
        return false;
    }
    wchar_t message[1024] = {};
    const bool authorized =
        procedure(L"ZHIHUI.CHAIJIJIA", L"天圆地方", message, 1024) == 1;
    if (gate.owned) FreeLibrary(gate.module);
    return authorized;
#endif
}

std::wstring WideText(const std::string& text)
{
    if (text.empty()) return L"天圆地方失败。";
    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
    UINT codePage = CP_UTF8;
    DWORD flags = MB_ERR_INVALID_CHARS;
    if (count <= 0) {
        codePage = CP_ACP;
        flags = 0;
        count = MultiByteToWideChar(CP_ACP, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
    }
    if (count <= 0) return L"天圆地方失败。";
    std::wstring wide(static_cast<size_t>(count), L'\0');
    MultiByteToWideChar(codePage, flags, text.c_str(), static_cast<int>(text.size()), wide.data(), count);
    return wide;
}

void ShowError(const std::string& text)
{
    MessageBoxW(nullptr, WideText(text).c_str(), L"天圆地方", MB_OK | MB_ICONERROR);
}
}

extern "C" DllExport void ufusr(char*, int* returnCode, int)
{
    if (returnCode != nullptr) *returnCode = 0;
    if (!EnsureAuthorized()) {
        if (returnCode != nullptr) *returnCode = 1;
        return;
    }
    const int status = UF_initialize();
    if (status != 0) {
        if (returnCode != nullptr) *returnCode = status;
        return;
    }
    try {
        TianYuanDiFanDialog dialog;
        dialog.Launch();
    } catch (const NXOpen::NXException& exception) {
        ShowError(exception.Message());
        if (returnCode != nullptr) *returnCode = exception.ErrorCode();
    } catch (const std::exception& exception) {
        ShowError(exception.what());
        if (returnCode != nullptr) *returnCode = -1;
    } catch (...) {
        ShowError("天圆地方发生未处理异常。");
        if (returnCode != nullptr) *returnCode = -1;
    }
    UF_terminate();
}

extern "C" DllExport int ufusr_ask_unload()
{
    // The dialog is modal and releases its blocks before ufusr returns.
    return UF_UNLOAD_IMMEDIATELY;
}

extern "C" DllExport void ufusr_cleanup()
{
}
