#include "StandardPartsLibrary.hpp"

#include <uf.h>
#include <NXOpen/NXException.hxx>
#include <Windows.h>
#include <cwchar>
#include <exception>
#include <string>

#ifndef DllExport
#define DllExport __declspec(dllexport)
#endif

namespace
{
using EnsureAuthorizedProc = int(__stdcall*)(const wchar_t*, const wchar_t*, wchar_t*, int);

std::wstring ToWide(const char* value)
{
    if (value == nullptr || *value == '\0') return L"未知错误";
    const int count = MultiByteToWideChar(CP_ACP, 0, value, -1, nullptr, 0);
    if (count <= 1) return L"未知错误";
    std::wstring result(static_cast<std::size_t>(count), L'\0');
    MultiByteToWideChar(CP_ACP, 0, value, -1, result.data(), count);
    result.pop_back();
    return result;
}

bool EnsureAuthorized(std::wstring& failureMessage)
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
    if (gate == nullptr)
    {
        failureMessage = L"无法加载授权保护模块 ZhaoFuNxLicenseGate.dll。";
        return false;
    }
    const auto authorize = reinterpret_cast<EnsureAuthorizedProc>(
        GetProcAddress(gate, "ZfnxEnsureAuthorized"));
    wchar_t message[1024] = {};
    const bool allowed = authorize != nullptr &&
        authorize(L"ZHIHUI.CHAIJIJIA", L"智辉标准件库", message, 1024) == 1;
    if (!allowed)
    {
        failureMessage = message[0] != L'\0'
            ? message
            : (authorize == nullptr
                   ? L"授权保护模块缺少 ZfnxEnsureAuthorized 接口。"
                   : L"当前授权未包含智辉标准件库功能。");
    }
    if (owned) FreeLibrary(gate);
    return allowed;
#endif
}
}

extern "C" DllExport void ufusr(char*, int* returnCode, int)
{
    if (returnCode != nullptr) *returnCode = 0;
    std::wstring authorizationError;
    if (!EnsureAuthorized(authorizationError))
    {
        MessageBoxW(GetForegroundWindow(), authorizationError.c_str(),
                    L"智辉标准件库", MB_OK | MB_ICONERROR);
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
    catch (const NXOpen::NXException& exception)
    {
        const std::wstring message = ToWide(exception.Message());
        MessageBoxW(GetForegroundWindow(), message.c_str(),
                    L"智辉标准件库", MB_OK | MB_ICONERROR);
        if (returnCode != nullptr) *returnCode = exception.ErrorCode();
    }
    catch (const std::exception& exception)
    {
        const std::wstring message = ToWide(exception.what());
        MessageBoxW(GetForegroundWindow(), message.c_str(),
                    L"智辉标准件库", MB_OK | MB_ICONERROR);
        if (returnCode != nullptr) *returnCode = -1;
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
