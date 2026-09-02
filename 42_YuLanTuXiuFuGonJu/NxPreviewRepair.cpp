#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <ShlObj.h>
#include <shellapi.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

namespace
{
constexpr wchar_t kTitle[] = L"UG智辉钣金插件预览图修复工具";
constexpr wchar_t kClsid[] = L"{8C785D42-59FC-42B1-A97A-6A7F9AB11574}";
constexpr wchar_t kThumbIid[] = L"{e357fccd-a995-4576-b01f-234630154e96}";
constexpr wchar_t kProviderName[] = L"NxPartThumbnailProvider.dll";
constexpr char kPayloadMarker[] = "ZHNXTHUMB_V1____";
constexpr int ID_STATUS = 101;
constexpr int ID_CHECK = 102;
constexpr int ID_REPAIR = 103;
constexpr int ID_REMOVE = 104;
HWND gStatus = nullptr;

std::wstring ModuleFolder()
{
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    return fs::path(path).parent_path().wstring();
}

std::wstring InstallFolder()
{
    wchar_t data[MAX_PATH] = {};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_COMMON_APPDATA, nullptr, SHGFP_TYPE_CURRENT, data)))
        return L"C:\\ProgramData\\Zhihui\\NXPreviewRepair";
    return (fs::path(data) / L"Zhihui" / L"NXPreviewRepair").wstring();
}

bool RegString(HKEY root, const std::wstring& subkey, const wchar_t* name, const std::wstring& value)
{
    HKEY key = nullptr;
    if (RegCreateKeyExW(root, subkey.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS)
        return false;
    const LONG result = RegSetValueExW(key, name, 0, REG_SZ,
        reinterpret_cast<const BYTE*>(value.c_str()), static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
    return result == ERROR_SUCCESS;
}

bool RegDword(HKEY root, const std::wstring& subkey, const wchar_t* name, DWORD value)
{
    HKEY key = nullptr;
    if (RegCreateKeyExW(root, subkey.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS)
        return false;
    const LONG result = RegSetValueExW(key, name, 0, REG_DWORD,
        reinterpret_cast<const BYTE*>(&value), sizeof(value));
    RegCloseKey(key);
    return result == ERROR_SUCCESS;
}

std::wstring QueryString(HKEY root, const std::wstring& subkey, const wchar_t* name = nullptr)
{
    HKEY key = nullptr;
    if (RegOpenKeyExW(root, subkey.c_str(), 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) return {};
    DWORD type = 0, bytes = 0;
    if (RegQueryValueExW(key, name, nullptr, &type, nullptr, &bytes) != ERROR_SUCCESS || type != REG_SZ)
    { RegCloseKey(key); return {}; }
    std::wstring value(bytes / sizeof(wchar_t), L'\0');
    if (RegQueryValueExW(key, name, nullptr, &type, reinterpret_cast<BYTE*>(value.data()), &bytes) != ERROR_SUCCESS)
        value.clear();
    RegCloseKey(key);
    while (!value.empty() && value.back() == L'\0') value.pop_back();
    return value;
}

DWORD QueryDword(HKEY root, const std::wstring& subkey, const wchar_t* name, DWORD fallback)
{
    HKEY key = nullptr; DWORD type = 0, value = fallback, bytes = sizeof(value);
    if (RegOpenKeyExW(root, subkey.c_str(), 0, KEY_QUERY_VALUE, &key) == ERROR_SUCCESS)
    {
        if (RegQueryValueExW(key, name, nullptr, &type, reinterpret_cast<BYTE*>(&value), &bytes) != ERROR_SUCCESS || type != REG_DWORD)
            value = fallback;
        RegCloseKey(key);
    }
    return value;
}

bool IsAdministrator()
{
    BOOL member = FALSE; SID_IDENTIFIER_AUTHORITY authority = SECURITY_NT_AUTHORITY; PSID group = nullptr;
    if (AllocateAndInitializeSid(&authority, 2, SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &group))
    { CheckTokenMembership(nullptr, group, &member); FreeSid(group); }
    return member != FALSE;
}

void SetStatus(const std::wstring& text) { SetWindowTextW(gStatus, text.c_str()); }

std::wstring Diagnose()
{
    const std::wstring installed = (fs::path(InstallFolder()) / kProviderName).wstring();
    const std::wstring handler = QueryString(HKEY_CLASSES_ROOT, std::wstring(L".prt\\ShellEx\\") + kThumbIid);
    const std::wstring server = QueryString(HKEY_CLASSES_ROOT, std::wstring(L"CLSID\\") + kClsid + L"\\InprocServer32");
    const DWORD iconsOnly = QueryDword(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"IconsOnly", 0);
    OSVERSIONINFOW version{sizeof(version)};
#pragma warning(suppress: 4996)
    GetVersionExW(&version);
    std::wostringstream out;
    out << L"系统：Windows " << version.dwMajorVersion << L"." << version.dwMinorVersion
        << L"（支持 Win10 / Win11）\r\n\r\n";
    out << L".prt 缩略图处理器：" << (_wcsicmp(handler.c_str(), kClsid) == 0 ? L"正常" : L"未注册或被其他程序覆盖") << L"\r\n";
    out << L"处理器 DLL：" << ((fs::exists(installed) && _wcsicmp(server.c_str(), installed.c_str()) == 0) ? L"正常" : L"缺失或路径错误") << L"\r\n";
    out << L"资源管理器缩略图设置：" << (iconsOnly == 0 ? L"已启用" : L"当前只显示图标") << L"\r\n";
    out << L"管理员权限：" << (IsAdministrator() ? L"是" : L"否（修复时会申请）") << L"\r\n\r\n";
    out << L"说明：本工具只显示 .prt 内已经存在的 NX 原生预览，不会启动 UG，也不会修改模型。\r\n"
        L"若某个模型内部没有预览，需要在 NX 中开启预览设置并重新保存一次。";
    return out.str();
}

bool RelaunchElevated(HWND window, const wchar_t* action)
{
    if (IsAdministrator()) return false;
    wchar_t exe[MAX_PATH] = {}; GetModuleFileNameW(nullptr, exe, MAX_PATH);
    std::wstring parameters = std::wstring(L"--") + action;
    SHELLEXECUTEINFOW info{sizeof(info)};
    info.lpVerb = L"runas"; info.lpFile = exe; info.lpParameters = parameters.c_str();
    info.nShow = SW_SHOWNORMAL; info.hwnd = window;
    if (ShellExecuteExW(&info)) { PostQuitMessage(0); return true; }
    return false;
}

void RefreshExplorer()
{
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    wchar_t windows[MAX_PATH] = {}; GetWindowsDirectoryW(windows, MAX_PATH);
    const fs::path refresh = fs::path(windows) / L"System32" / L"ie4uinit.exe";
    if (fs::exists(refresh))
        ShellExecuteW(nullptr, L"open", refresh.c_str(), L"-show", nullptr, SW_HIDE);
}

bool ExtractEmbeddedProvider(const fs::path& target, std::wstring& error)
{
    wchar_t exePath[MAX_PATH] = {}; GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    HANDLE file = CreateFileW(exePath, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) { error = L"无法读取修复工具本身。"; return false; }
    LARGE_INTEGER total{};
    if (!GetFileSizeEx(file, &total) || total.QuadPart < 32) { CloseHandle(file); error = L"修复工具文件不完整。"; return false; }
    constexpr DWORD trailerSize = 16 + sizeof(unsigned long long);
    LARGE_INTEGER trailerPosition{}; trailerPosition.QuadPart = total.QuadPart - trailerSize;
    SetFilePointerEx(file, trailerPosition, nullptr, FILE_BEGIN);
    char marker[16] = {}; unsigned long long payloadSize = 0; DWORD read = 0;
    bool valid = ReadFile(file, marker, sizeof(marker), &read, nullptr) && read == sizeof(marker) &&
        memcmp(marker, kPayloadMarker, sizeof(marker)) == 0 &&
        ReadFile(file, &payloadSize, sizeof(payloadSize), &read, nullptr) && read == sizeof(payloadSize) &&
        payloadSize >= 4096 && payloadSize <= 16ULL * 1024 * 1024 &&
        payloadSize + trailerSize <= static_cast<unsigned long long>(total.QuadPart);
    if (!valid) { CloseHandle(file); error = L"内置缩略图组件缺失，请重新下载完整的单文件版本。"; return false; }
    LARGE_INTEGER payloadPosition{};
    payloadPosition.QuadPart = total.QuadPart - trailerSize - static_cast<LONGLONG>(payloadSize);
    SetFilePointerEx(file, payloadPosition, nullptr, FILE_BEGIN);
    HANDLE output = CreateFileW(target.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (output == INVALID_HANDLE_VALUE) { CloseHandle(file); error = L"无法写入系统公共目录。"; return false; }
    BYTE buffer[64 * 1024]; unsigned long long remaining = payloadSize; bool ok = true;
    while (remaining > 0)
    {
        const DWORD wanted = static_cast<DWORD>(std::min<unsigned long long>(remaining, sizeof(buffer)));
        DWORD got = 0, written = 0;
        if (!ReadFile(file, buffer, wanted, &got, nullptr) || got != wanted ||
            !WriteFile(output, buffer, got, &written, nullptr) || written != got)
        { ok = false; break; }
        remaining -= got;
    }
    CloseHandle(output); CloseHandle(file);
    if (!ok) { DeleteFileW(target.c_str()); error = L"释放内置缩略图组件失败。"; }
    return ok;
}

bool Repair(std::wstring& error)
{
    std::error_code ec;
    const fs::path folder = InstallFolder();
    const fs::path target = folder / kProviderName;
    fs::create_directories(folder, ec);
    if (ec || !ExtractEmbeddedProvider(target, error)) return false;

    const std::wstring clsid = std::wstring(L"Software\\Classes\\CLSID\\") + kClsid;
    const std::wstring inproc = clsid + L"\\InprocServer32";
    bool ok = true;
    ok &= RegString(HKEY_LOCAL_MACHINE, clsid, nullptr, L"NX Part Thumbnail Provider");
    ok &= RegDword(HKEY_LOCAL_MACHINE, clsid, L"DisableProcessIsolation", 1);
    ok &= RegString(HKEY_LOCAL_MACHINE, inproc, nullptr, target.wstring());
    ok &= RegString(HKEY_LOCAL_MACHINE, inproc, L"ThreadingModel", L"Apartment");
    ok &= RegString(HKEY_LOCAL_MACHINE, L"Software\\Classes\\.prt", nullptr, L"NXPartFile");
    ok &= RegString(HKEY_LOCAL_MACHINE, std::wstring(L"Software\\Classes\\.prt\\ShellEx\\") + kThumbIid, nullptr, kClsid);
    ok &= RegString(HKEY_LOCAL_MACHINE, std::wstring(L"Software\\Classes\\NXPartFile\\ShellEx\\") + kThumbIid, nullptr, kClsid);
    ok &= RegString(HKEY_LOCAL_MACHINE, std::wstring(L"Software\\Classes\\SystemFileAssociations\\.prt\\ShellEx\\") + kThumbIid, nullptr, kClsid);
    ok &= RegString(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved", kClsid, L"NX Part Thumbnail Provider");
    ok &= RegDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"IconsOnly", 0);
    if (!ok) { error = L"写入注册表失败。请确认允许了管理员权限，并暂时关闭安全软件拦截。"; return false; }
    RefreshExplorer();
    return true;
}

void RemoveOwnedHandler(HKEY root, const std::wstring& key)
{
    if (_wcsicmp(QueryString(root, key).c_str(), kClsid) == 0) RegDeleteTreeW(root, key.c_str());
}

bool RemoveRepair(std::wstring& error)
{
    const std::wstring base = L"Software\\Classes\\";
    for (HKEY root : {HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE})
    {
        RemoveOwnedHandler(root, base + std::wstring(L".prt\\ShellEx\\") + kThumbIid);
        RemoveOwnedHandler(root, base + std::wstring(L"NXPartFile\\ShellEx\\") + kThumbIid);
        RemoveOwnedHandler(root, base + std::wstring(L"SystemFileAssociations\\.prt\\ShellEx\\") + kThumbIid);
        RegDeleteTreeW(root, (base + L"CLSID\\" + kClsid).c_str());
    }
    HKEY approved = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved", 0, KEY_SET_VALUE, &approved) == ERROR_SUCCESS)
    { RegDeleteValueW(approved, kClsid); RegCloseKey(approved); }
    RefreshExplorer();
    std::error_code ec; fs::remove(fs::path(InstallFolder()) / kProviderName, ec);
    if (ec) error = L"注册已移除；DLL 正被资源管理器使用，将在重启电脑后才能手动删除。";
    return true;
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        CreateWindowW(L"STATIC", L"UG智辉钣金插件预览图修复工具（Windows 10 / Windows 11）",
            WS_CHILD | WS_VISIBLE, 18, 16, 650, 24, window, nullptr, nullptr, nullptr);
        gStatus = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_MULTILINE |
            ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL, 18, 50, 650, 255, window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_STATUS)), nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"重新检测", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            18, 325, 130, 38, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_CHECK)), nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"一键修复", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            164, 325, 180, 38, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_REPAIR)), nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"卸载修复", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            360, 325, 130, 38, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_REMOVE)), nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"关闭", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            506, 325, 162, 38, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDCANCEL)), nullptr, nullptr);
        SetStatus(Diagnose());
        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case ID_CHECK: SetStatus(Diagnose()); return 0;
        case ID_REPAIR:
        {
            if (RelaunchElevated(window, L"repair")) return 0;
            std::wstring error;
            if (Repair(error)) MessageBoxW(window, L"修复完成。请关闭并重新打开 .prt 文件夹，或按 F5 刷新。", kTitle, MB_OK | MB_ICONINFORMATION);
            else MessageBoxW(window, error.c_str(), kTitle, MB_OK | MB_ICONERROR);
            SetStatus(Diagnose()); return 0;
        }
        case ID_REMOVE:
        {
            if (RelaunchElevated(window, L"remove")) return 0;
            std::wstring error; RemoveRepair(error);
            MessageBoxW(window, error.empty() ? L"本工具写入的缩略图修复已移除。" : error.c_str(), kTitle, MB_OK | MB_ICONINFORMATION);
            SetStatus(Diagnose()); return 0;
        }
        case IDCANCEL: DestroyWindow(window); return 0;
        }
        break;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR commandLine, int show)
{
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_STANDARD_CLASSES}; InitCommonControlsEx(&controls);
    WNDCLASSEXW cls{sizeof(cls)}; cls.lpfnWndProc = WindowProc; cls.hInstance = instance;
    cls.hCursor = LoadCursorW(nullptr, IDC_ARROW); cls.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    cls.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1); cls.lpszClassName = L"NxPreviewRepairWindow";
    RegisterClassExW(&cls);
    HWND window = CreateWindowExW(0, cls.lpszClassName, kTitle, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 705, 420, nullptr, nullptr, instance, nullptr);
    if (!window) return 1;
    ShowWindow(window, show); UpdateWindow(window);
    if (commandLine && wcsstr(commandLine, L"--repair")) PostMessageW(window, WM_COMMAND, ID_REPAIR, 0);
    else if (commandLine && wcsstr(commandLine, L"--remove")) PostMessageW(window, WM_COMMAND, ID_REMOVE, 0);
    MSG message{}; while (GetMessageW(&message, nullptr, 0, 0) > 0) { TranslateMessage(&message); DispatchMessageW(&message); }
    return static_cast<int>(message.wParam);
}
