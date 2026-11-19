#include "KuaiJieJianCommon.hpp"

#include <uf.h>
#include <uf_exit.h>
#include <uf_mb.h>
#include <uf_ui.h>

#include <NXOpen/NXException.hxx>
#include <NXOpen/Session.hxx>

#include <algorithm>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#ifndef DllExport
#define DllExport __declspec(dllexport)
#endif

namespace
{
using namespace zhihui::kjj;

constexpr UINT WM_APP_EXEC_CMD = WM_APP + 101;
constexpr UINT_PTR TIMER_CLEAR_BUFFER = 1;
constexpr UINT_PTR TIMER_HIDE_HUD = 2;

struct ExecPayload
{
    std::wstring commandId;
    std::wstring displayName;
};

HMODULE g_hModule = nullptr;
HHOOK g_kbHook = nullptr;
HHOOK g_wndProcHook = nullptr;
HHOOK g_getMsgHook = nullptr;
HWND g_nxMainHwnd = nullptr;
HWND g_hudHwnd = nullptr;
HFONT g_hudFontMain = nullptr;
HFONT g_hudFontSub = nullptr;
DWORD g_nxThreadId = 0;
UINT g_reloadMsg = 0;
bool g_ufInitialized = false;
bool g_executing = false;

std::wstring g_pluginRoot;
std::wstring g_configPath;
std::wstring g_lastCmdPath;
FILETIME g_configMtime{};
ShortcutConfig g_config;
std::unordered_map<int, LastCommandInfo> g_buttonIdMap;
bool g_buttonMapBuilt = false;

std::wstring g_buffer;
std::wstring g_lastCommandId;
std::wstring g_lastDisplayName;
std::wstring g_hudTitle;
std::wstring g_hudSub;
bool g_hudIsError = false;

std::wstring ModulePathW()
{
    wchar_t path[MAX_PATH]{};
    const DWORD len = GetModuleFileNameW(g_hModule, path, MAX_PATH);
    return (len > 0 && len < MAX_PATH) ? std::wstring(path, len) : std::wstring();
}

void ReloadConfigIfNeeded(bool force)
{
    if (g_configPath.empty())
    {
        return;
    }
    FILETIME ft{};
    const bool hasFt = GetFileWriteTimeW(g_configPath, ft);
    if (!force && hasFt &&
        ft.dwLowDateTime == g_configMtime.dwLowDateTime &&
        ft.dwHighDateTime == g_configMtime.dwHighDateTime)
    {
        return;
    }
    g_config = LoadOrCreateShortcutConfig(g_configPath);
    if (hasFt)
    {
        g_configMtime = ft;
    }
}

HWND ResolveNxMainWindow()
{
    if (g_nxMainHwnd && IsWindow(g_nxMainHwnd))
    {
        return g_nxMainHwnd;
    }
    HWND parent = reinterpret_cast<HWND>(UF_UI_get_default_parent());
    if (parent && IsWindow(parent))
    {
        g_nxMainHwnd = parent;
        return g_nxMainHwnd;
    }
    struct SearchCtx
    {
        HWND found = nullptr;
    } ctx;
    EnumThreadWindows(g_nxThreadId, [](HWND hwnd, LPARAM lp) -> BOOL {
        if (IsWindowVisible(hwnd) && GetWindow(hwnd, GW_OWNER) == nullptr)
        {
            wchar_t cls[128]{};
            GetClassNameW(hwnd, cls, 128);
            if (wcscmp(cls, HudWindowClassName()) != 0)
            {
                reinterpret_cast<SearchCtx*>(lp)->found = hwnd;
                return FALSE;
            }
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&ctx));
    g_nxMainHwnd = ctx.found;
    return g_nxMainHwnd;
}

bool IsTextOrListControl(HWND hwnd)
{
    if (!hwnd)
    {
        return false;
    }
    wchar_t cls[128]{};
    if (GetClassNameW(hwnd, cls, 128) <= 0)
    {
        return false;
    }
    const std::wstring lower = ToLowerAscii(cls);
    static const wchar_t* kBlocked[] = {
        L"edit", L"richedit", L"combo", L"listbox", L"scintilla",
        L"qlineedit", L"qtextedit", L"qplaintextedit", L"qcombobox",
        L"textbox", L"syslistview", L"systreeview"
    };
    for (const wchar_t* token : kBlocked)
    {
        if (lower.find(token) != std::wstring::npos)
        {
            return true;
        }
    }
    return false;
}

bool IsSafeInputContext()
{
    if (g_executing)
    {
        return false;
    }
    if (GetKeyState(VK_CONTROL) < 0 || GetKeyState(VK_MENU) < 0 ||
        GetKeyState(VK_LWIN) < 0 || GetKeyState(VK_RWIN) < 0)
    {
        return false;
    }
    HWND mainHwnd = ResolveNxMainWindow();
    if (!mainHwnd)
    {
        return false;
    }
    HWND active = GetActiveWindow();
    if (active != mainHwnd)
    {
        return false;
    }
    HWND focus = GetFocus();
    if (IsTextOrListControl(focus))
    {
        return false;
    }
    return true;
}

bool MatchesToggleHotkey(WPARAM vk)
{
    const std::wstring hk = ToUpperAscii(g_config.toggleHotkey);
    const bool needAlt = hk.find(L"ALT+") != std::wstring::npos;
    const bool needCtrl = hk.find(L"CTRL+") != std::wstring::npos;
    const bool needShift = hk.find(L"SHIFT+") != std::wstring::npos;

    const bool hasAlt = (GetKeyState(VK_MENU) < 0);
    const bool hasCtrl = (GetKeyState(VK_CONTROL) < 0);
    const bool hasShift = (GetKeyState(VK_SHIFT) < 0);
    if (needAlt != hasAlt || needCtrl != hasCtrl || needShift != hasShift)
    {
        return false;
    }

    std::wstring keyPart = hk;
    const std::size_t plus = hk.find_last_of(L'+');
    if (plus != std::wstring::npos)
    {
        keyPart = hk.substr(plus + 1);
    }
    if (keyPart.size() >= 2 && keyPart[0] == L'F')
    {
        const int fnum = _wtoi(keyPart.c_str() + 1);
        if (fnum >= 1 && fnum <= 12)
        {
            return vk == static_cast<WPARAM>(VK_F1 + (fnum - 1));
        }
    }
    if (keyPart.size() == 1 && keyPart[0] >= L'A' && keyPart[0] <= L'Z')
    {
        return vk == static_cast<WPARAM>(keyPart[0]);
    }
    return vk == VK_F1 && hasAlt;
}

void ShowHud(const std::wstring& title, const std::wstring& sub, bool isError, UINT autoHideMs)
{
    if (!g_hudHwnd || !g_config.showHud)
    {
        return;
    }
    g_hudTitle = title;
    g_hudSub = sub;
    g_hudIsError = isError;

    constexpr int width = 360;
    const int height = sub.empty() ? 42 : 66;
    HWND mainHwnd = ResolveNxMainWindow();
    RECT rc{};
    if (mainHwnd && GetWindowRect(mainHwnd, &rc))
    {
        const int x = rc.left + ((rc.right - rc.left) - width) / 2;
        const int y = rc.bottom - height - 64;
        SetWindowPos(g_hudHwnd, HWND_TOPMOST, x, y, width, height,
            SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }
    else
    {
        SetWindowPos(g_hudHwnd, HWND_TOPMOST, 600, 700, width, height,
            SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }
    InvalidateRect(g_hudHwnd, nullptr, TRUE);
    KillTimer(g_hudHwnd, TIMER_HIDE_HUD);
    if (autoHideMs > 0)
    {
        SetTimer(g_hudHwnd, TIMER_HIDE_HUD, autoHideMs, nullptr);
    }
}

void HideHud()
{
    if (g_hudHwnd)
    {
        KillTimer(g_hudHwnd, TIMER_HIDE_HUD);
        ShowWindow(g_hudHwnd, SW_HIDE);
    }
}

std::wstring CurrentNxModule()
{
    try
    {
        auto* session = NXOpen::Session::GetSession();
        if (session)
        {
            if (session->ActiveSketch() != nullptr)
            {
                return L"草图";
            }
            const NXOpen::NXString app = session->ApplicationName();
            const std::wstring appW = ToUpperAscii(Utf8ToWide(app.GetText() ? app.GetText() : ""));
            if (appW.find(L"DRAFTING") != std::wstring::npos) return L"制图";
            if (appW.find(L"SBSM") != std::wstring::npos || appW.find(L"SHEET_METAL") != std::wstring::npos) return L"钣金";
            if (appW.find(L"SKETCH") != std::wstring::npos) return L"草图";
            if (appW.find(L"ASSEMBLIES") != std::wstring::npos) return L"装配";
            if (appW.find(L"MODELING") != std::wstring::npos) return L"建模";
        }
    }
    catch (...)
    {
    }
    return L"全部";
}

bool ModuleMatches(const std::wstring& entryModule, const std::wstring& currentModule)
{
    if (entryModule.empty() || entryModule == L"全部")
    {
        return true;
    }
    return entryModule == currentModule;
}

void RegisterButtonInMap(const std::wstring& buttonName, const std::wstring& displayName, const std::wstring& category)
{
    if (buttonName.empty() || buttonName == L"KuaiJieJian")
    {
        return;
    }
    const std::string ansi = WideToAnsi(buttonName);
    int btnId = 0;
    if (UF_MB_ask_button_id(const_cast<char*>(ansi.c_str()), &btnId) == 0 && btnId != 0)
    {
        g_buttonIdMap[btnId] = LastCommandInfo{buttonName, displayName.empty() ? buttonName : displayName, category};
    }
}

void ScanMenuFileForButtons(const std::wstring& menuPath, const std::wstring& category)
{
    std::wstring text;
    if (!ReadUtf8FileW(menuPath, text))
    {
        return;
    }
    std::wstring pendingBtn;
    std::wstring pendingLabel;
    auto flushPending = [&]() {
        if (!pendingBtn.empty())
        {
            RegisterButtonInMap(pendingBtn, pendingLabel.empty() ? pendingBtn : pendingLabel, category);
            pendingBtn.clear();
            pendingLabel.clear();
        }
    };
    std::size_t pos = 0;
    while (pos <= text.size())
    {
        std::size_t eol = text.find(L'\n', pos);
        if (eol == std::wstring::npos) eol = text.size();
        std::wstring line = TrimWide(text.substr(pos, eol - pos));
        pos = eol + 1;
        if (line.empty() || line[0] == L'!') continue;
        if (line.rfind(L"BUTTON ", 0) == 0 || line.rfind(L"BUTTON\t", 0) == 0)
        {
            flushPending();
            pendingBtn = TrimWide(line.substr(6));
            const std::size_t sp = pendingBtn.find_first_of(L" \t");
            if (sp != std::wstring::npos) pendingBtn = pendingBtn.substr(0, sp);
        }
        else if (!pendingBtn.empty() && (line.rfind(L"LABEL ", 0) == 0 || line.rfind(L"LABEL\t", 0) == 0))
        {
            pendingLabel = TrimWide(line.substr(5));
        }
    }
    flushPending();
}

void EnsureButtonMapBuilt()
{
    if (g_buttonMapBuilt)
    {
        return;
    }
    g_buttonMapBuilt = true;
    for (const auto& entry : g_config.entries)
    {
        RegisterButtonInMap(entry.commandId, entry.displayName, entry.category);
    }
    ScanMenuFileForButtons(g_pluginRoot + L"\\startup\\UGZH_design.men", L"智辉插件");

    wchar_t ugiiDir[MAX_PATH]{};
    if (GetEnvironmentVariableW(L"UGII_BASE_DIR", ugiiDir, MAX_PATH) > 0)
    {
        const std::wstring menusDir = std::wstring(ugiiDir) + L"\\UGII\\menus";
        static const wchar_t* kNativeMenus[] = {
            L"ug_main.men", L"ug_modeling.men", L"ug_sbsm.men",
            L"ug_gateway.men", L"ug_modeling_sketch_tools.men"
        };
        for (const wchar_t* file : kNativeMenus)
        {
            ScanMenuFileForButtons(menusDir + L"\\" + file, L"NX原生");
        }
    }
}

void RecordObservedCommandId(int btnId)
{
    if (btnId == 0)
    {
        return;
    }
    EnsureButtonMapBuilt();
    const auto it = g_buttonIdMap.find(btnId);
    if (it == g_buttonIdMap.end())
    {
        return;
    }
    if (it->second.commandId == L"KuaiJieJian")
    {
        return;
    }
    if (!g_lastCmdPath.empty())
    {
        SaveLastCommandW(g_lastCmdPath, it->second);
    }
}

void UpdateHudForBuffer()
{
    if (g_buffer.empty())
    {
        HideHud();
        return;
    }
    const std::wstring curMod = CurrentNxModule();
    const ShortcutEntry* exact = nullptr;
    std::vector<const ShortcutEntry*> prefixMatches;
    for (const auto& entry : g_config.entries)
    {
        if (!entry.enabled || !ModuleMatches(entry.module, curMod))
        {
            continue;
        }
        if (entry.alias == g_buffer)
        {
            if (!exact || (entry.module == curMod && exact->module == L"全部"))
            {
                exact = &entry;
            }
        }
        else if (entry.alias.rfind(g_buffer, 0) == 0 && prefixMatches.size() < 3)
        {
            prefixMatches.push_back(&entry);
        }
    }

    const std::wstring title = L"简码: " + ToUpperAscii(g_buffer) +
        (exact ? (L"  →  " + exact->displayName + L"  [空格执行]") : L"");
    std::wstring sub;
    if (!exact && prefixMatches.empty())
    {
        sub = L"当前模块(" + curMod + L")无匹配命令 (Esc取消)";
    }
    else if (!prefixMatches.empty())
    {
        sub = L"[" + curMod + L"] 候选: ";
        for (std::size_t i = 0; i < prefixMatches.size(); ++i)
        {
            if (i > 0) sub += L"  |  ";
            sub += prefixMatches[i]->alias + L":" + prefixMatches[i]->displayName;
        }
    }
    else
    {
        sub = exact->commandId + L" (" + exact->category + L" · " + exact->module + L")";
    }
    ShowHud(title, sub, false, 0);
}

const ShortcutEntry* FindExactShortcut(const std::wstring& alias)
{
    const std::wstring curMod = CurrentNxModule();
    const ShortcutEntry* fallback = nullptr;
    for (const auto& entry : g_config.entries)
    {
        if (!entry.enabled || entry.alias != alias)
        {
            continue;
        }
        if (entry.module == curMod && curMod != L"全部")
        {
            return &entry;
        }
        if (entry.module.empty() || entry.module == L"全部")
        {
            if (!fallback) fallback = &entry;
        }
    }
    return fallback;
}

void PostExecuteCommand(const std::wstring& commandId, const std::wstring& displayName)
{
    if (!g_hudHwnd || commandId.empty())
    {
        return;
    }
    auto* payload = new ExecPayload{commandId, displayName};
    if (!PostMessageW(g_hudHwnd, WM_APP_EXEC_CMD, 0, reinterpret_cast<LPARAM>(payload)))
    {
        delete payload;
    }
}

void ExecuteCommandNow(const ExecPayload& payload)
{
    if (g_executing)
    {
        return;
    }
    g_executing = true;
    ReloadConfigIfNeeded(false);

    std::wstring dllCandidate = payload.commandId;
    if (dllCandidate == L"BanJinCaiTu") dllCandidate = L"BanJinCaiTuDialog";
    else if (dllCandidate == L"TwoPointSiBian") dllCandidate = L"TwoPointSiBianUI";

    const std::wstring dllPath = g_pluginRoot + L"\\application\\" + dllCandidate + L".dll";
    if (FileExistsW(dllPath))
    {
        using UfusrProc = void(*)(char*, int*, int);
        using AskUnloadProc = int(*)();

        const int lockStatus = UF_UI_lock_ug_access(UF_UI_FROM_CUSTOM);
        if (lockStatus == UF_UI_LOCK_ERROR)
        {
            ShowHud(L"请先关闭当前对话框", payload.displayName, true, 1500);
            AppendDebugLogW(g_pluginRoot, L"UF_UI_lock_ug_access returned UF_UI_LOCK_ERROR for " + dllPath);
            g_executing = false;
            return;
        }
        const bool needUnlock = (lockStatus == UF_UI_LOCK_SET);

        HMODULE existingMod = GetModuleHandleW(dllPath.c_str());
        HMODULE mod = existingMod ? existingMod : LoadLibraryW(dllPath.c_str());
        if (mod)
        {
            auto ufusrFn = reinterpret_cast<UfusrProc>(GetProcAddress(mod, "ufusr"));
            auto askUnloadFn = reinterpret_cast<AskUnloadProc>(GetProcAddress(mod, "ufusr_ask_unload"));
            if (ufusrFn)
            {
                g_lastCommandId = payload.commandId;
                g_lastDisplayName = payload.displayName;
                if (payload.commandId != L"KuaiJieJian" && !g_lastCmdPath.empty())
                {
                    SaveLastCommandW(g_lastCmdPath, {payload.commandId, payload.displayName, L"智辉插件"});
                }
                HideHud();
                AppendDebugLogW(g_pluginRoot, L"Invoking native ufusr: " + dllPath);
                char paramBuf[4]{};
                int retCode = 0;
                try
                {
                    ufusrFn(paramBuf, &retCode, 0);
                }
                catch (...)
                {
                    AppendDebugLogW(g_pluginRoot, L"Exception inside ufusr: " + dllPath);
                }
                if (!existingMod && (!askUnloadFn || askUnloadFn() == UF_UNLOAD_IMMEDIATELY))
                {
                    FreeLibrary(mod);
                }
                if (needUnlock)
                {
                    UF_UI_unlock_ug_access(UF_UI_FROM_CUSTOM);
                }
                g_executing = false;
                return;
            }
            if (!existingMod)
            {
                FreeLibrary(mod);
            }
        }
        if (needUnlock)
        {
            UF_UI_unlock_ug_access(UF_UI_FROM_CUSTOM);
        }
        AppendDebugLogW(g_pluginRoot, L"Native ufusr not found or LoadLibraryW failed: " + dllPath);
    }

    const std::string cmdAnsi = WideToAnsi(payload.commandId);
    HWND mainHwnd = ResolveNxMainWindow();
    int buttonId = 0;
    const int askStatus = UF_MB_ask_button_id(const_cast<char*>(cmdAnsi.c_str()), &buttonId);

    if (askStatus == 0 && buttonId > 0 && buttonId <= 0xFFFF && mainHwnd)
    {
        PostMessageW(mainHwnd, WM_COMMAND, static_cast<WPARAM>(buttonId), 0);
        g_lastCommandId = payload.commandId;
        g_lastDisplayName = payload.displayName;
        if (payload.commandId != L"KuaiJieJian" && !g_lastCmdPath.empty())
        {
            SaveLastCommandW(g_lastCmdPath, {payload.commandId, payload.displayName, L"NX原生"});
        }
        ShowHud(L"执行: " + payload.displayName, L"按空格可重复此命令", false, 1100);
        AppendDebugLogW(g_pluginRoot,
            L"Dispatched WM_COMMAND: " + payload.commandId + L" id=" + std::to_wstring(buttonId));
        g_executing = false;
        return;
    }

    ShowHud(L"未找到或无法执行: " + payload.displayName, payload.commandId, true, 1600);
    AppendDebugLogW(g_pluginRoot, L"Command not found or >16bit: " + payload.commandId +
        L" id=" + std::to_wstring(buttonId));
    g_executing = false;
}

LRESULT CALLBACK KeyboardHookProc(int code, WPARAM wParam, LPARAM lParam)
{
    if (code != HC_ACTION)
    {
        return CallNextHookEx(g_kbHook, code, wParam, lParam);
    }

    const bool isKeyUp = (lParam & 0x80000000) != 0;
    if (isKeyUp)
    {
        return CallNextHookEx(g_kbHook, code, wParam, lParam);
    }

    if (wParam == VK_PROCESSKEY)
    {
        return CallNextHookEx(g_kbHook, code, wParam, lParam);
    }

    ReloadConfigIfNeeded(false);

    if (MatchesToggleHotkey(wParam))
    {
        g_config.enabled = !g_config.enabled;
        g_buffer.clear();
        SaveShortcutConfig(g_configPath, g_config);
        GetFileWriteTimeW(g_configPath, g_configMtime);
        ShowHud(
            g_config.enabled ? L"快捷键(空格执行): 已开启" : L"快捷键(空格执行): 已暂停",
            L"按 " + g_config.toggleHotkey + L" 可随时切换",
            !g_config.enabled, 1400);
        return 1;
    }

    if (!g_config.enabled || !IsSafeInputContext())
    {
        return CallNextHookEx(g_kbHook, code, wParam, lParam);
    }

    wchar_t ch = 0;
    if (wParam >= L'A' && wParam <= L'Z')
    {
        ch = static_cast<wchar_t>(wParam - L'A' + L'a');
    }
    else if (wParam >= L'0' && wParam <= L'9')
    {
        ch = static_cast<wchar_t>(wParam);
    }
    else if (wParam >= VK_NUMPAD0 && wParam <= VK_NUMPAD9)
    {
        ch = static_cast<wchar_t>(L'0' + (wParam - VK_NUMPAD0));
    }

    if (ch != 0)
    {
        if (g_buffer.size() < 16)
        {
            g_buffer.push_back(ch);
        }
        if (g_hudHwnd)
        {
            SetTimer(g_hudHwnd, TIMER_CLEAR_BUFFER, 3000, nullptr);
        }
        UpdateHudForBuffer();
        return 1;
    }

    if (wParam == VK_BACK && !g_buffer.empty())
    {
        g_buffer.pop_back();
        if (g_hudHwnd)
        {
            SetTimer(g_hudHwnd, TIMER_CLEAR_BUFFER, 3000, nullptr);
        }
        UpdateHudForBuffer();
        return 1;
    }

    if (wParam == VK_ESCAPE && !g_buffer.empty())
    {
        g_buffer.clear();
        if (g_hudHwnd)
        {
            KillTimer(g_hudHwnd, TIMER_CLEAR_BUFFER);
        }
        HideHud();
        return 1;
    }

    if (wParam == VK_SPACE || (wParam == VK_RETURN && !g_buffer.empty()))
    {
        if (!g_buffer.empty())
        {
            const std::wstring typed = g_buffer;
            g_buffer.clear();
            if (g_hudHwnd)
            {
                KillTimer(g_hudHwnd, TIMER_CLEAR_BUFFER);
            }
            if (const ShortcutEntry* entry = FindExactShortcut(typed))
            {
                PostExecuteCommand(entry->commandId, entry->displayName);
            }
            else
            {
                ShowHud(L"未找到简码: " + ToUpperAscii(typed),
                    L"可在菜单「快捷键」中添加或修改", true, 1500);
            }
            return 1;
        }
        if (wParam == VK_SPACE && g_config.repeatOnEmptySpace && !g_lastCommandId.empty())
        {
            PostExecuteCommand(g_lastCommandId, g_lastDisplayName);
            return 1;
        }
    }

    return CallNextHookEx(g_kbHook, code, wParam, lParam);
}

LRESULT CALLBACK HudWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == g_reloadMsg && g_reloadMsg != 0)
    {
        ReloadConfigIfNeeded(true);
        ShowHud(L"快捷键配置已刷新",
            g_config.enabled ? L"监听已开启" : L"监听已暂停", false, 1200);
        return 0;
    }
    switch (msg)
    {
    case WM_APP_EXEC_CMD:
    {
        auto* payload = reinterpret_cast<ExecPayload*>(lParam);
        if (payload)
        {
            ExecuteCommandNow(*payload);
            delete payload;
        }
        return 0;
    }
    case WM_TIMER:
        if (wParam == TIMER_CLEAR_BUFFER)
        {
            KillTimer(hwnd, TIMER_CLEAR_BUFFER);
            if (!g_buffer.empty())
            {
                g_buffer.clear();
                HideHud();
            }
        }
        else if (wParam == TIMER_HIDE_HUD)
        {
            KillTimer(hwnd, TIMER_HIDE_HUD);
            if (g_buffer.empty())
            {
                HideHud();
            }
        }
        return 0;
    case WM_PAINT:
    {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc{};
        GetClientRect(hwnd, &rc);

        const COLORREF bgColor = g_hudIsError ? RGB(72, 26, 28) : RGB(24, 30, 40);
        const COLORREF borderColor = g_hudIsError ? RGB(220, 90, 90) : RGB(68, 168, 255);
        HBRUSH bgBrush = CreateSolidBrush(bgColor);
        FillRect(hdc, &rc, bgBrush);
        DeleteObject(bgBrush);

        HPEN pen = CreatePen(PS_SOLID, 1, borderColor);
        HGDIOBJ oldPen = SelectObject(hdc, pen);
        HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        DeleteObject(pen);

        SetBkMode(hdc, TRANSPARENT);
        HGDIOBJ oldFont = SelectObject(hdc, g_hudFontMain);
        SetTextColor(hdc, RGB(242, 246, 252));
        RECT titleRc = {rc.left + 12, rc.top + 8, rc.right - 12, rc.top + 34};
        DrawTextW(hdc, g_hudTitle.c_str(), -1, &titleRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        if (!g_hudSub.empty())
        {
            SelectObject(hdc, g_hudFontSub);
            SetTextColor(hdc, RGB(158, 184, 214));
            RECT subRc = {rc.left + 12, rc.top + 34, rc.right - 12, rc.bottom - 8};
            DrawTextW(hdc, g_hudSub.c_str(), -1, &subRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        }
        SelectObject(hdc, oldFont);
        EndPaint(hwnd, &ps);
        return 0;
    }
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

LRESULT CALLBACK CallWndProcHook(int code, WPARAM wParam, LPARAM lParam)
{
    if (code == HC_ACTION && lParam)
    {
        const auto* cwp = reinterpret_cast<const CWPSTRUCT*>(lParam);
        if (cwp->message == WM_COMMAND && cwp->hwnd == ResolveNxMainWindow())
        {
            RecordObservedCommandId(LOWORD(cwp->wParam));
        }
    }
    return CallNextHookEx(g_wndProcHook, code, wParam, lParam);
}

LRESULT CALLBACK GetMsgHook(int code, WPARAM wParam, LPARAM lParam)
{
    if (code == HC_ACTION && wParam == PM_REMOVE && lParam)
    {
        const auto* msg = reinterpret_cast<const MSG*>(lParam);
        if (msg->message == WM_COMMAND && msg->hwnd == ResolveNxMainWindow())
        {
            RecordObservedCommandId(LOWORD(msg->wParam));
        }
    }
    return CallNextHookEx(g_getMsgHook, code, wParam, lParam);
}

void InitializeShortcutHook()
{
    const std::wstring modulePath = ModulePathW();
    const std::wstring startupDir = DirectoryOf(modulePath);
    g_pluginRoot = DirectoryOf(startupDir);
    if (g_pluginRoot.empty())
    {
        return;
    }

    if (UF_initialize() != 0)
    {
        AppendDebugLogW(g_pluginRoot, L"UF_initialize failed in ufsta.");
        return;
    }
    g_ufInitialized = true;

    g_configPath = g_pluginRoot + L"\\config\\KuaiJieJian.ini";
    g_lastCmdPath = g_pluginRoot + L"\\config\\KuaiJieJian.lastcmd";
    ReloadConfigIfNeeded(true);
    g_nxThreadId = GetCurrentThreadId();
    g_reloadMsg = ReloadWindowMessage();
    ResolveNxMainWindow();

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = HudWndProc;
    wc.hInstance = g_hModule;
    wc.lpszClassName = HudWindowClassName();
    RegisterClassExW(&wc);

    g_hudFontMain = CreateFontW(
        -16, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
    g_hudFontSub = CreateFontW(
        -12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");

    g_hudHwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED,
        HudWindowClassName(), L"", WS_POPUP,
        0, 0, 360, 66, nullptr, nullptr, g_hModule, nullptr);
    if (g_hudHwnd)
    {
        SetLayeredWindowAttributes(g_hudHwnd, 0, 236, LWA_ALPHA);
    }

    g_kbHook = SetWindowsHookExW(WH_KEYBOARD, KeyboardHookProc, nullptr, g_nxThreadId);
    g_wndProcHook = SetWindowsHookExW(WH_CALLWNDPROC, CallWndProcHook, nullptr, g_nxThreadId);
    g_getMsgHook = SetWindowsHookExW(WH_GETMESSAGE, GetMsgHook, nullptr, g_nxThreadId);
    AppendDebugLogW(g_pluginRoot,
        L"KuaiJieJianLoader initialized. entries=" + std::to_wstring(g_config.entries.size()) +
        L" enabled=" + std::to_wstring(g_config.enabled ? 1 : 0) +
        L" hook=" + std::to_wstring(g_kbHook ? 1 : 0));
}

void CleanupShortcutHook()
{
    if (g_getMsgHook)
    {
        UnhookWindowsHookEx(g_getMsgHook);
        g_getMsgHook = nullptr;
    }
    if (g_wndProcHook)
    {
        UnhookWindowsHookEx(g_wndProcHook);
        g_wndProcHook = nullptr;
    }
    if (g_kbHook)
    {
        UnhookWindowsHookEx(g_kbHook);
        g_kbHook = nullptr;
    }
    if (g_hudHwnd)
    {
        DestroyWindow(g_hudHwnd);
        g_hudHwnd = nullptr;
    }
    if (g_hudFontMain)
    {
        DeleteObject(g_hudFontMain);
        g_hudFontMain = nullptr;
    }
    if (g_hudFontSub)
    {
        DeleteObject(g_hudFontSub);
        g_hudFontSub = nullptr;
    }
    if (g_ufInitialized)
    {
        UF_terminate();
        g_ufInitialized = false;
    }
}
} // namespace

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        g_hModule = hModule;
        DisableThreadLibraryCalls(hModule);
    }
    return TRUE;
}

extern "C" DllExport void ufsta(char*, int* returnCode, int)
{
    if (returnCode)
    {
        *returnCode = 0;
    }
    InitializeShortcutHook();
}

extern "C" DllExport int ufusr_ask_unload()
{
    return UF_UNLOAD_UG_TERMINATE;
}

extern "C" DllExport void ufusr_cleanup()
{
    CleanupShortcutHook();
}
