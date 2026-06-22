#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#ifdef CreateDialog
#undef CreateDialog
#endif

#include <NXOpen/NXMessageBox.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/UI.hxx>

#include <string>
#include <vector>

namespace
{
typedef int(__stdcall* EnsureAuthorizedProc)(const wchar_t*, const wchar_t*, wchar_t*, int);

std::wstring GetModuleDirectory()
{
    HMODULE module = nullptr;
    GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&GetModuleDirectory),
        &module);

    wchar_t path[MAX_PATH] = {};
    DWORD length = GetModuleFileNameW(module, path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
    {
        return L"";
    }

    std::wstring value(path, length);
    const std::wstring::size_type slash = value.find_last_of(L"\\/");
    return slash == std::wstring::npos ? L"" : value.substr(0, slash);
}

void ShowMessage(NXOpen::NXMessageBox::DialogType type, const char* message)
{
    try
    {
        NXOpen::UI::GetUI()->NXMessageBox()->Show("智辉钣金检查更新", type, message);
    }
    catch (...)
    {
    }
}

HMODULE LoadProtectedLicenseGate(const std::wstring& appDir)
{
    const wchar_t* moduleName = L"ZhaoFuNxLicenseGate.dll";

    HMODULE existing = GetModuleHandleW(moduleName);
    if (existing != nullptr)
    {
        return existing;
    }

    if (!appDir.empty())
    {
        const std::wstring localPath = appDir + L"\\" + moduleName;
        HMODULE localModule = LoadLibraryW(localPath.c_str());
        if (localModule != nullptr)
        {
            return localModule;
        }
    }

    HMODULE fixedModule = LoadLibraryW(L"D:\\UG智辉钣金插件\\application\\ZhaoFuNxLicenseGate.dll");
    if (fixedModule != nullptr)
    {
        return fixedModule;
    }

    return LoadLibraryW(moduleName);
}

bool EnsureAuthorized(const std::wstring& appDir)
{
    wchar_t message[1024] = {};
    HMODULE module = LoadProtectedLicenseGate(appDir);
    if (module == nullptr)
    {
        ShowMessage(NXOpen::NXMessageBox::DialogTypeError, "授权组件不存在，无法检查更新。");
        return false;
    }

    auto ensureAuthorized = reinterpret_cast<EnsureAuthorizedProc>(GetProcAddress(module, "ZfnxEnsureAuthorized"));
    if (ensureAuthorized == nullptr)
    {
        ShowMessage(NXOpen::NXMessageBox::DialogTypeError, "授权组件入口无效，无法检查更新。");
        return false;
    }

    const int ok = ensureAuthorized(L"ZHIHUI.CHECKUPDATE", L"检查更新", message, static_cast<int>(sizeof(message) / sizeof(message[0])));
    if (ok != 1)
    {
        ShowMessage(NXOpen::NXMessageBox::DialogTypeError, "授权验证未通过，无法检查更新。");
        return false;
    }

    return true;
}

bool LaunchChecker(bool silent)
{
    const std::wstring appDir = GetModuleDirectory();
    if (appDir.empty())
    {
        return false;
    }

    const std::wstring checker = appDir + L"\\ZhihuiUpdateChecker\\ZhihuiUpdateChecker.exe";
    if (GetFileAttributesW(checker.c_str()) == INVALID_FILE_ATTRIBUTES)
    {
        return false;
    }

    const std::wstring installRoot = appDir + L"\\..";
    std::wstring command = L"\"" + checker + L"\" --target \"" + installRoot + L"\"";
    if (silent)
    {
        command += L" --silent";
    }

    STARTUPINFOW startupInfo = {};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInfo = {};
    std::vector<wchar_t> mutableCommand(command.begin(), command.end());
    mutableCommand.push_back(L'\0');
    const BOOL created = CreateProcessW(
        nullptr,
        mutableCommand.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NO_WINDOW,
        nullptr,
        appDir.c_str(),
        &startupInfo,
        &processInfo);

    if (created)
    {
        CloseHandle(processInfo.hThread);
        CloseHandle(processInfo.hProcess);
        return true;
    }

    return false;
}
}

extern "C" __declspec(dllexport) void ufusr(char*, int* retcod, int)
{
    if (retcod != nullptr)
    {
        *retcod = 0;
    }

    const std::wstring appDir = GetModuleDirectory();
    if (!EnsureAuthorized(appDir))
    {
        if (retcod != nullptr)
        {
            *retcod = 1;
        }
        return;
    }

    if (!LaunchChecker(false))
    {
        if (retcod != nullptr)
        {
            *retcod = 1;
        }
        ShowMessage(NXOpen::NXMessageBox::DialogTypeError, "检查更新程序不存在或启动失败，请重新安装智辉钣金插件。");
    }
}

extern "C" __declspec(dllexport) int ufusr_ask_unload()
{
    return static_cast<int>(NXOpen::Session::LibraryUnloadOptionImmediately);
}
