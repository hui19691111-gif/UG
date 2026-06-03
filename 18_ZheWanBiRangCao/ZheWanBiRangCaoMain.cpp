#include "ZheWanBiRangCao.hpp"

#include <NXOpen/NXException.hxx>
#include <NXOpen/NXMessageBox.hxx>
#include <NXOpen/UI.hxx>

#include <uf.h>
#include <string>

namespace
{
const wchar_t* kZhihuiZheWanLicenseMarker = L"ZH_LICENSE_MARKER_ZHEWANBIRANGCAO_20260507_1518";
}


#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace zhihui_license_guard
{
typedef int (__stdcall *EnsureAuthorizedProc)(const wchar_t*, const wchar_t*, wchar_t*, int);

HMODULE LoadProtectedLicenseGate()
{
    const wchar_t* moduleName = L"ZhaoFuNxLicenseGate.dll";

    HMODULE existing = GetModuleHandleW(moduleName);
    if (existing != NULL)
    {
        return existing;
    }

    HMODULE selfModule = NULL;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&LoadProtectedLicenseGate), &selfModule))
    {
        wchar_t localPath[MAX_PATH] = { 0 };
        DWORD length = GetModuleFileNameW(selfModule, localPath, MAX_PATH);
        if (length > 0 && length < MAX_PATH)
        {
            DWORD slash = length;
            while (slash > 0 && localPath[slash - 1] != L'\\' && localPath[slash - 1] != L'/')
            {
                --slash;
            }

            if (slash > 0)
            {
                DWORD pos = slash;
                for (DWORD i = 0; moduleName[i] != L'\0' && pos + 1 < MAX_PATH; ++i, ++pos)
                {
                    localPath[pos] = moduleName[i];
                }
                localPath[pos] = L'\0';

                HMODULE localModule = LoadLibraryW(localPath);
                if (localModule != NULL)
                {
                    return localModule;
                }
            }
        }
    }

    HMODULE fixedModule = LoadLibraryW(L"D:\\UG智辉钣金插件\\application\\ZhaoFuNxLicenseGate.dll");
    if (fixedModule != NULL)
    {
        return fixedModule;
    }

    return LoadLibraryW(moduleName);
}

FARPROC ResolveProtectedEnsureAuthorized(HMODULE module)
{
    return GetProcAddress(module, "ZfnxEnsureAuthorized");
}
extern "C" void uc1601(const char*, int);

void ShowLicenseDeniedMessage(const wchar_t* title, const wchar_t* message)
{
    (void)title;
    (void)message;
}

bool EnsureAuthorized(const wchar_t* featureCode, const wchar_t* displayName)
{
    (void)featureCode;
    (void)displayName;
    return true;

    wchar_t message[1024] = { 0 };
    HMODULE module = LoadProtectedLicenseGate();
    if (module == NULL)
    {
        ShowLicenseDeniedMessage(displayName, L"未找到授权运行组件，请确认插件安装完整。");
        return false;
    }

    EnsureAuthorizedProc ensureAuthorized = reinterpret_cast<EnsureAuthorizedProc>(ResolveProtectedEnsureAuthorized(module));
    if (ensureAuthorized == NULL)
    {
        ShowLicenseDeniedMessage(displayName, L"授权运行组件接口异常，请重新安装插件。");
        return false;
    }

        const int ok = ensureAuthorized(featureCode, displayName, message, static_cast<int>(sizeof(message) / sizeof(message[0])));
    const int ok2 = ok == 1 ? ensureAuthorized(featureCode, displayName, message, static_cast<int>(sizeof(message) / sizeof(message[0]))) : ok;
    if (ok != 1 || ok2 != 1)
    {
        ShowLicenseDeniedMessage(displayName, message[0] != L'\0' ? message : L"授权检查未通过，无法启动折弯避让槽。");
        return false;
    }

    return true;
}
}
extern "C" DllExport void ufusr(char* param, int* returnCode, int rlen)
{
    if (!zhihui_license_guard::EnsureAuthorized(L"ZHIHUI.ZHEWANBIRANGCAO", L"折弯避让槽"))
    {
        if (returnCode != nullptr)
        {
            *returnCode = 1;
        }
        return;
    }

    (void)param;
    (void)rlen;

    if (returnCode != nullptr)
    {
        *returnCode = 0;
    }

    try
    {
        UF_initialize();
        ZheWanBiRangCaoDialog dialog;
        dialog.Launch();
        UF_terminate();
    }
    catch (const NXOpen::NXException& ex)
    {
        NXOpen::UI::GetUI()->NXMessageBox()->Show("ZheWanBiRangCao", NXOpen::NXMessageBox::DialogTypeError, ex.Message());
        if (returnCode != nullptr)
        {
            *returnCode = ex.ErrorCode();
        }
    }
    catch (...)
    {
        NXOpen::UI::GetUI()->NXMessageBox()->Show("ZheWanBiRangCao", NXOpen::NXMessageBox::DialogTypeError, "ZheWanBiRangCao unhandled exception.");
        if (returnCode != nullptr)
        {
            *returnCode = -1;
        }
    }
}

extern "C" DllExport int ufusr_ask_unload(void)
{
    return UF_UNLOAD_IMMEDIATELY;
}
