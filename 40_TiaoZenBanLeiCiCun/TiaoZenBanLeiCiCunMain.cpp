#include "TiaoZenBanLeiCiCun.hpp"

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
               L"ZHIHUI.TIAOZENBANLEICICUN",
               L"板件调尺", message, 1024) == 1;
#endif
}
}

extern "C" DllExport void ufusr(char*, int* returnCode, int)
{
    TiaoZenWriteLog(
        "INFO",
        "================ 板件调尺启动 ================ 版本=1.0.1");
    if (returnCode != nullptr)
    {
        *returnCode = 0;
    }
    if (!EnsureAuthorized())
    {
        TiaoZenWriteLog(
            "ERROR", "ZhaoFu 授权校验失败，命令已终止。");
        if (returnCode != nullptr)
        {
            *returnCode = 1;
        }
        return;
    }

    const int initializeStatus = UF_initialize();
    if (initializeStatus != 0)
    {
        TiaoZenWriteLog(
            "ERROR",
            "UF_initialize 失败，状态码=" +
                std::to_string(initializeStatus));
        if (returnCode != nullptr)
        {
            *returnCode = initializeStatus;
        }
        return;
    }

    try
    {
        TiaoZenWriteLog("INFO", "授权及 UF_initialize 成功。");
        PanelSizeDialog dialog;
        dialog.Launch();
        TiaoZenWriteLog("INFO", "对话框 Launch 已返回。");
    }
    catch (const NXOpen::NXException& ex)
    {
        TiaoZenWriteLog(
            "ERROR",
            std::string("ufusr 捕获 NXException：code=") +
                std::to_string(ex.ErrorCode()) +
                "，message=" +
                (ex.Message() != nullptr ? ex.Message() : "<null>"));
        NXOpen::UI::GetUI()->NXMessageBox()->Show(
            "板件调尺",
            NXOpen::NXMessageBox::DialogTypeError,
            ex.Message());
        if (returnCode != nullptr)
        {
            *returnCode = ex.ErrorCode();
        }
    }
    catch (...)
    {
        TiaoZenWriteLog("ERROR", "ufusr 捕获未知异常。");
        NXOpen::UI::GetUI()->NXMessageBox()->Show(
            "板件调尺",
            NXOpen::NXMessageBox::DialogTypeError,
            "板件调尺发生未处理异常。");
        if (returnCode != nullptr)
        {
            *returnCode = -1;
        }
    }
    UF_terminate();
    TiaoZenWriteLog("INFO", "UF_terminate 完成，命令结束。");
}

extern "C" DllExport int ufusr_ask_unload()
{
    return UF_UNLOAD_IMMEDIATELY;
}
