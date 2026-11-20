#include "KuaiJieJianCommon.hpp"

#include <NXOpen/LogFile.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/Session.hxx>
#include <uf.h>
#include <uf_ui.h>

#include <CommCtrl.h>
#include <cwchar>
#include <share.h>
#include <string>
#include <vector>

#ifndef DllExport
#define DllExport __declspec(dllexport)
#endif

namespace
{
using namespace zhihui::kjj;
using EnsureAuthorizedProc = int(__stdcall*)(const wchar_t*, const wchar_t*, wchar_t*, int);

constexpr int IDC_CHK_ENABLED = 1001;
constexpr int IDC_CHK_REPEAT = 1002;
constexpr int IDC_CHK_HUD = 1003;
constexpr int IDC_EDT_SEARCH = 1004;
constexpr int IDC_CMB_FILTER_MOD = 1005;
constexpr int IDC_LIST_SHORTCUTS = 1006;
constexpr int IDC_EDT_ALIAS = 1007;
constexpr int IDC_EDT_NAME = 1008;
constexpr int IDC_EDT_CMDID = 1009;
constexpr int IDC_CMB_CAT = 1010;
constexpr int IDC_CMB_MOD = 1011;
constexpr int IDC_BTN_READ_LAST = 1012;
constexpr int IDC_BTN_UPDATE_SEL = 1013;
constexpr int IDC_BTN_ADD_NEW = 1014;
constexpr int IDC_BTN_TOGGLE = 1015;
constexpr int IDC_BTN_DELETE = 1016;
constexpr int IDC_BTN_RESET = 1017;
constexpr int IDC_BTN_SAVE = 1018;
constexpr int IDC_BTN_CLOSE = 1019;
constexpr int IDC_BTN_HOTKEY = 1020;

HMODULE g_hModule = nullptr;

HMODULE LoadGate()
{
    constexpr const wchar_t* name = L"ZhaoFuNxLicenseGate.dll";
    if (HMODULE module = GetModuleHandleW(name))
    {
        return module;
    }
    wchar_t path[MAX_PATH]{};
    if (GetModuleFileNameW(g_hModule, path, MAX_PATH))
    {
        wchar_t* slash = wcsrchr(path, L'\\');
        if (slash)
        {
            *(slash + 1) = L'\0';
            if (wcscat_s(path, name) == 0)
            {
                if (HMODULE local = LoadLibraryW(path))
                {
                    return local;
                }
            }
        }
    }
    return LoadLibraryW(name);
}

bool Authorized()
{
    HMODULE gate = LoadGate();
    if (!gate)
    {
        return false;
    }
    auto* procedure = reinterpret_cast<EnsureAuthorizedProc>(
        GetProcAddress(gate, "ZfnxEnsureAuthorized"));
    if (!procedure)
    {
        return false;
    }
    wchar_t message[1024]{};
    return procedure(L"ZHIHUI.KUAIJIEJIAN", L"快捷键", message, 1024) == 1;
}

std::wstring ResolvePluginRoot()
{
    wchar_t path[MAX_PATH]{};
    const DWORD len = GetModuleFileNameW(g_hModule, path, MAX_PATH);
    if (len == 0 || len >= MAX_PATH)
    {
        return std::wstring();
    }
    return DirectoryOf(DirectoryOf(std::wstring(path, len)));
}

struct DialogState
{
    std::wstring pluginRoot;
    std::wstring configPath;
    std::wstring lastCmdPath;
    ShortcutConfig config;
    std::vector<std::size_t> visibleIndices;
    HFONT font = nullptr;
    HWND hwnd = nullptr;
    HWND chkEnabled = nullptr;
    HWND chkRepeat = nullptr;
    HWND chkHud = nullptr;
    HWND btnHotkey = nullptr;
    HWND edtSearch = nullptr;
    HWND cmbFilterMod = nullptr;
    HWND list = nullptr;
    HWND edtAlias = nullptr;
    HWND edtName = nullptr;
    HWND edtCmdId = nullptr;
    HWND cmbCat = nullptr;
    HWND cmbMod = nullptr;
    bool running = true;
    bool dirty = false;
    bool capturingHotkey = false;
    int sortColumn = -1;
    bool sortAscending = true;
};

std::wstring GetControlText(HWND hwnd)
{
    const int len = GetWindowTextLengthW(hwnd);
    if (len <= 0)
    {
        return std::wstring();
    }
    std::wstring text(static_cast<std::size_t>(len) + 1, L'\0');
    GetWindowTextW(hwnd, text.data(), len + 1);
    text.resize(static_cast<std::size_t>(len));
    return TrimWide(text);
}

std::wstring GetComboSelectedText(HWND cmb, const wchar_t* fallback)
{
    const LRESULT sel = SendMessageW(cmb, CB_GETCURSEL, 0, 0);
    if (sel == CB_ERR)
    {
        return fallback;
    }
    const LRESULT len = SendMessageW(cmb, CB_GETLBTEXTLEN, static_cast<WPARAM>(sel), 0);
    if (len <= 0)
    {
        return fallback;
    }
    std::wstring buf(static_cast<std::size_t>(len) + 1, L'\0');
    SendMessageW(cmb, CB_GETLBTEXT, static_cast<WPARAM>(sel), reinterpret_cast<LPARAM>(buf.data()));
    buf.resize(static_cast<std::size_t>(len));
    buf = TrimWide(buf);
    return buf.empty() ? fallback : buf;
}

void SelectComboText(HWND cmb, const std::wstring& value, int defaultIndex)
{
    LRESULT found = SendMessageW(cmb, CB_FINDSTRINGEXACT, static_cast<WPARAM>(-1),
        reinterpret_cast<LPARAM>(value.c_str()));
    SendMessageW(cmb, CB_SETCURSEL, (found == CB_ERR) ? defaultIndex : static_cast<WPARAM>(found), 0);
}

void UpdateListHeaderSortIndicator(const DialogState& st)
{
    static const wchar_t* kBaseTitles[6] = {
        L"简码", L"命令名称", L"命令ID (ButtonName)", L"分类", L"适用模块", L"状态"
    };
    for (int i = 0; i < 6; ++i)
    {
        std::wstring text = kBaseTitles[i];
        if (i == st.sortColumn)
        {
            text += st.sortAscending ? L" ▲" : L" ▼";
        }
        LVCOLUMNW col{};
        col.mask = LVCF_TEXT;
        col.pszText = const_cast<wchar_t*>(text.c_str());
        ListView_SetColumn(st.list, i, &col);
    }
}

void PopulateList(DialogState& st, int selectConfigIdx = -1)
{
    SendMessageW(st.list, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(st.list);
    st.visibleIndices.clear();

    const std::wstring filter = ToLowerAscii(GetControlText(st.edtSearch));
    const std::wstring modFilter = GetComboSelectedText(st.cmbFilterMod, L"全部模块");

    for (std::size_t i = 0; i < st.config.entries.size(); ++i)
    {
        const auto& e = st.config.entries[i];
        const std::wstring eMod = e.module.empty() ? L"全部" : e.module;
        if (modFilter != L"全部模块" && eMod != modFilter)
        {
            continue;
        }
        if (!filter.empty())
        {
            const bool match =
                ToLowerAscii(e.alias).find(filter) != std::wstring::npos ||
                ToLowerAscii(e.displayName).find(filter) != std::wstring::npos ||
                ToLowerAscii(e.commandId).find(filter) != std::wstring::npos ||
                ToLowerAscii(e.category).find(filter) != std::wstring::npos ||
                ToLowerAscii(eMod).find(filter) != std::wstring::npos;
            if (!match)
            {
                continue;
            }
        }
        st.visibleIndices.push_back(i);
    }

    if (st.sortColumn >= 0 && st.sortColumn < 6)
    {
        const int col = st.sortColumn;
        const bool asc = st.sortAscending;
        auto fieldOf = [&](std::size_t idx) -> std::wstring {
            const auto& e = st.config.entries[idx];
            switch (col)
            {
            case 0: return e.alias;
            case 1: return e.displayName;
            case 2: return e.commandId;
            case 3: return e.category;
            case 4: return e.module.empty() ? L"全部" : e.module;
            case 5: return e.enabled ? L"启用" : L"禁用";
            default: return L"";
            }
        };
        std::stable_sort(st.visibleIndices.begin(), st.visibleIndices.end(),
            [&](std::size_t a, std::size_t b) {
                const int cmp = lstrcmpiW(fieldOf(a).c_str(), fieldOf(b).c_str());
                return asc ? (cmp < 0) : (cmp > 0);
            });
    }
    UpdateListHeaderSortIndicator(st);

    int rowToSelect = -1;
    for (int row = 0; row < static_cast<int>(st.visibleIndices.size()); ++row)
    {
        const std::size_t idx = st.visibleIndices[static_cast<std::size_t>(row)];
        const auto& e = st.config.entries[idx];
        const std::wstring eMod = e.module.empty() ? L"全部" : e.module;
        if (static_cast<int>(idx) == selectConfigIdx)
        {
            rowToSelect = row;
        }

        LVITEMW item{};
        item.mask = LVIF_TEXT;
        item.iItem = row;
        item.pszText = const_cast<wchar_t*>(e.alias.c_str());
        ListView_InsertItem(st.list, &item);

        ListView_SetItemText(st.list, row, 1, const_cast<wchar_t*>(e.displayName.c_str()));
        ListView_SetItemText(st.list, row, 2, const_cast<wchar_t*>(e.commandId.c_str()));
        ListView_SetItemText(st.list, row, 3, const_cast<wchar_t*>(e.category.c_str()));
        ListView_SetItemText(st.list, row, 4, const_cast<wchar_t*>(eMod.c_str()));
        const wchar_t* statusText = e.enabled ? L"启用" : L"禁用";
        ListView_SetItemText(st.list, row, 5, const_cast<wchar_t*>(statusText));
    }
    if (rowToSelect >= 0)
    {
        ListView_SetItemState(st.list, rowToSelect, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        ListView_EnsureVisible(st.list, rowToSelect, FALSE);
    }
    SendMessageW(st.list, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(st.list, nullptr, TRUE);
}

void SyncCheckboxesFromConfig(const DialogState& st)
{
    SendMessageW(st.chkEnabled, BM_SETCHECK, st.config.enabled ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(st.chkRepeat, BM_SETCHECK, st.config.repeatOnEmptySpace ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(st.chkHud, BM_SETCHECK, st.config.showHud ? BST_CHECKED : BST_UNCHECKED, 0);
    if (st.btnHotkey)
    {
        const std::wstring hk = st.config.toggleHotkey.empty() ? L"ALT+F1" : st.config.toggleHotkey;
        SetWindowTextW(st.btnHotkey, hk.c_str());
    }
}

std::wstring FormatCapturedHotkey(WPARAM vk, bool& valid)
{
    valid = false;
    if (vk == VK_CONTROL || vk == VK_SHIFT || vk == VK_MENU ||
        vk == VK_LWIN || vk == VK_RWIN)
    {
        return L"";
    }
    const bool hasCtrl = (GetKeyState(VK_CONTROL) < 0);
    const bool hasAlt = (GetKeyState(VK_MENU) < 0);
    const bool hasShift = (GetKeyState(VK_SHIFT) < 0);

    std::wstring keyName;
    bool isFunctionOrTilde = false;
    if (vk >= VK_F1 && vk <= VK_F12)
    {
        keyName = L"F" + std::to_wstring(static_cast<int>(vk - VK_F1 + 1));
        isFunctionOrTilde = true;
    }
    else if (vk >= L'A' && vk <= L'Z')
    {
        keyName.push_back(static_cast<wchar_t>(vk));
    }
    else if (vk >= L'0' && vk <= L'9')
    {
        keyName.push_back(static_cast<wchar_t>(vk));
    }
    else if (vk == VK_OEM_3)
    {
        keyName = L"`";
        isFunctionOrTilde = true;
    }
    else
    {
        return L"";
    }

    if (!isFunctionOrTilde && !hasCtrl && !hasAlt && !hasShift)
    {
        return L"";
    }

    std::wstring combo;
    if (hasCtrl) combo += L"CTRL+";
    if (hasAlt) combo += L"ALT+";
    if (hasShift) combo += L"SHIFT+";
    combo += keyName;
    valid = true;
    return combo;
}

int SelectedConfigIndex(const DialogState& st)
{
    const int sel = ListView_GetNextItem(st.list, -1, LVNI_SELECTED);
    if (sel < 0 || static_cast<std::size_t>(sel) >= st.visibleIndices.size())
    {
        return -1;
    }
    return static_cast<int>(st.visibleIndices[static_cast<std::size_t>(sel)]);
}

void LoadSelectedIntoEditor(const DialogState& st)
{
    const int idx = SelectedConfigIndex(st);
    if (idx < 0)
    {
        return;
    }
    const auto& e = st.config.entries[static_cast<std::size_t>(idx)];
    SetWindowTextW(st.edtAlias, e.alias.c_str());
    SetWindowTextW(st.edtName, e.displayName.c_str());
    SetWindowTextW(st.edtCmdId, e.commandId.c_str());
    SelectComboText(st.cmbCat, e.category, 2);
    SelectComboText(st.cmbMod, e.module.empty() ? L"全部" : e.module, 0);
}

bool ValidateEditorInput(HWND hwnd, const DialogState& st, ShortcutEntry& outEntry)
{
    outEntry.alias = ToLowerAscii(GetControlText(st.edtAlias));
    outEntry.displayName = GetControlText(st.edtName);
    outEntry.commandId = GetControlText(st.edtCmdId);
    if (outEntry.alias.empty() || outEntry.commandId.empty())
    {
        MessageBoxW(hwnd, L"请填写「简码」和「命令ID」。", L"快捷键", MB_OK | MB_ICONWARNING);
        return false;
    }
    for (wchar_t ch : outEntry.alias)
    {
        if (!((ch >= L'a' && ch <= L'z') || (ch >= L'0' && ch <= L'9')))
        {
            MessageBoxW(hwnd, L"简码仅支持英文字母和数字。", L"快捷键", MB_OK | MB_ICONWARNING);
            return false;
        }
    }
    if (outEntry.displayName.empty())
    {
        outEntry.displayName = outEntry.commandId;
    }
    outEntry.category = GetComboSelectedText(st.cmbCat, L"自定义");
    outEntry.module = GetComboSelectedText(st.cmbMod, L"全部");
    outEntry.enabled = true;
    return true;
}

int FindDuplicateAliasIndex(const DialogState& st, const std::wstring& alias, const std::wstring& mod, int ignoreIdx)
{
    for (int i = 0; i < static_cast<int>(st.config.entries.size()); ++i)
    {
        if (i == ignoreIdx) continue;
        const auto& e = st.config.entries[static_cast<std::size_t>(i)];
        const std::wstring eMod = e.module.empty() ? L"全部" : e.module;
        if (e.alias == alias && eMod == mod)
        {
            return i;
        }
    }
    return -1;
}

bool CheckAllDuplicatesBeforeSave(HWND hwnd, const DialogState& st)
{
    std::wstring conflicts;
    int conflictCount = 0;
    for (std::size_t i = 0; i < st.config.entries.size(); ++i)
    {
        const auto& a = st.config.entries[i];
        if (!a.enabled) continue;
        const std::wstring aMod = a.module.empty() ? L"全部" : a.module;
        for (std::size_t j = i + 1; j < st.config.entries.size(); ++j)
        {
            const auto& b = st.config.entries[j];
            if (!b.enabled) continue;
            const std::wstring bMod = b.module.empty() ? L"全部" : b.module;
            if (a.alias == b.alias && aMod == bMod)
            {
                if (conflictCount < 8)
                {
                    conflicts += L"• 简码「" + a.alias + L"」(模块:" + aMod + L")：" +
                        a.displayName + L" 与 " + b.displayName + L"\r\n";
                }
                ++conflictCount;
            }
        }
    }
    if (conflictCount > 0)
    {
        const std::wstring msg = L"检测到 " + std::to_wstring(conflictCount) +
            L" 处重复快捷键（同一适用模块下简码相同）：\r\n\r\n" + conflicts +
            L"\r\n请先修改或删除重复项后再保存。";
        MessageBoxW(hwnd, msg.c_str(), L"快捷键重复提示", MB_OK | MB_ICONWARNING);
        return false;
    }
    return true;
}

bool SaveAndNotifyLoader(DialogState& st, bool showSuccessPopup)
{
    if (!CheckAllDuplicatesBeforeSave(st.hwnd, st))
    {
        return false;
    }
    st.config.enabled = (SendMessageW(st.chkEnabled, BM_GETCHECK, 0, 0) == BST_CHECKED);
    st.config.repeatOnEmptySpace = (SendMessageW(st.chkRepeat, BM_GETCHECK, 0, 0) == BST_CHECKED);
    st.config.showHud = (SendMessageW(st.chkHud, BM_GETCHECK, 0, 0) == BST_CHECKED);

    if (!SaveShortcutConfig(st.configPath, st.config))
    {
        MessageBoxW(st.hwnd, L"保存配置文件失败，请检查写入权限。", L"快捷键", MB_OK | MB_ICONERROR);
        return false;
    }
    st.dirty = false;
    if (HWND hud = FindWindowW(HudWindowClassName(), nullptr))
    {
        PostMessageW(hud, ReloadWindowMessage(), 0, 0);
    }
    if (showSuccessPopup)
    {
        MessageBoxW(st.hwnd, L"快捷键配置已保存并立即生效！", L"快捷键", MB_OK | MB_ICONINFORMATION);
    }
    return true;
}

bool ReadLastCommandFromNxSyslog(const DialogState& st, LastCommandInfo& outInfo)
{
    std::wstring syslogPath;
    try
    {
        auto* session = NXOpen::Session::GetSession();
        if (session && session->LogFile())
        {
            const NXOpen::NXString fn = session->LogFile()->FileName();
            if (fn.GetText())
            {
                syslogPath = Utf8ToWide(fn.GetText());
            }
        }
    }
    catch (...)
    {
        return false;
    }
    if (syslogPath.empty())
    {
        return false;
    }

    FILE* fp = _wfsopen(syslogPath.c_str(), L"rb", _SH_DENYNO);
    if (!fp)
    {
        return false;
    }
    _fseeki64(fp, 0, SEEK_END);
    const long long totalSize = _ftelli64(fp);
    const long long chunkSize = 262144;
    const long long startOffset = (totalSize > chunkSize) ? (totalSize - chunkSize) : 0;
    _fseeki64(fp, startOffset, SEEK_SET);

    std::string raw(static_cast<std::size_t>(totalSize - startOffset), '\0');
    if (!raw.empty())
    {
        const std::size_t got = fread(raw.data(), 1, raw.size(), fp);
        raw.resize(got);
    }
    fclose(fp);

    std::vector<std::string> lines;
    std::size_t pos = 0;
    while (pos < raw.size())
    {
        std::size_t eol = raw.find('\n', pos);
        if (eol == std::string::npos) eol = raw.size();
        std::string line = raw.substr(pos, eol - pos);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(std::move(line));
        pos = eol + 1;
    }

    std::wstring recentDialogTitle;
    const std::string menuPrefix = "&MACRO MENU, ";
    const std::string dlgPrefix = "&MACRO DIALOG_BEGIN \"";

    for (auto it = lines.rbegin(); it != lines.rend(); ++it)
    {
        const std::string& line = *it;
        if (line.rfind(dlgPrefix, 0) == 0)
        {
            const std::size_t q2 = line.find('"', dlgPrefix.size());
            if (q2 != std::string::npos)
            {
                const std::string title = line.substr(dlgPrefix.size(), q2 - dlgPrefix.size());
                if (title != "Persistent Dialog" && !title.empty())
                {
                    recentDialogTitle = Utf8ToWide(title);
                }
            }
            continue;
        }
        if (line.rfind(menuPrefix, 0) == 0)
        {
            const std::size_t comma2 = line.find(',', menuPrefix.size());
            if (comma2 == std::string::npos) continue;
            std::size_t p = comma2 + 1;
            while (p < line.size() && (line[p] == ' ' || line[p] == '\t')) ++p;
            std::size_t end = p;
            while (end < line.size() && line[end] != ' ' && line[end] != '\t' && line[end] != '<') ++end;
            if (end <= p) continue;
            const std::wstring btnName = Utf8ToWide(line.substr(p, end - p));
            if (btnName.empty() || btnName == L"KuaiJieJian")
            {
                recentDialogTitle.clear();
                continue;
            }

            outInfo.commandId = btnName;
            outInfo.displayName = recentDialogTitle.empty() ? btnName : recentDialogTitle;
            outInfo.category = (btnName.rfind(L"UG_", 0) == 0 || btnName.rfind(L"NX_", 0) == 0)
                ? L"NX原生" : L"智辉插件";

            for (const auto& entry : st.config.entries)
            {
                if (entry.commandId == btnName)
                {
                    outInfo.displayName = entry.displayName;
                    outInfo.category = entry.category;
                    break;
                }
            }
            return true;
        }
    }
    return false;
}

HWND MakeCtrl(HWND parent, const wchar_t* cls, const wchar_t* text,
    DWORD style, DWORD exStyle, int x, int y, int w, int h, int id, HFONT font)
{
    HWND ctrl = CreateWindowExW(exStyle, cls, text, WS_CHILD | WS_VISIBLE | style,
        x, y, w, h, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g_hModule, nullptr);
    if (ctrl && font)
    {
        SendMessageW(ctrl, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    }
    return ctrl;
}

LRESULT CALLBACK ConfigWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* st = reinterpret_cast<DialogState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg)
    {
    case WM_CREATE:
    {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        st = reinterpret_cast<DialogState*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(st));
        st->hwnd = hwnd;

        st->font = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");

        st->chkEnabled = MakeCtrl(hwnd, L"BUTTON", L"启用快捷键(空格执行)",
            BS_AUTOCHECKBOX | WS_TABSTOP, 0, 16, 12, 175, 24, IDC_CHK_ENABLED, st->font);
        st->chkRepeat = MakeCtrl(hwnd, L"BUTTON", L"空输入按空格重复上条",
            BS_AUTOCHECKBOX | WS_TABSTOP, 0, 198, 12, 180, 24, IDC_CHK_REPEAT, st->font);
        st->chkHud = MakeCtrl(hwnd, L"BUTTON", L"显示悬浮提示框",
            BS_AUTOCHECKBOX | WS_TABSTOP, 0, 386, 12, 130, 24, IDC_CHK_HUD, st->font);

        MakeCtrl(hwnd, L"STATIC", L"监听开关热键:", 0, 0, 528, 16, 98, 22, 0, st->font);
        st->btnHotkey = MakeCtrl(hwnd, L"BUTTON", L"ALT+F1",
            WS_TABSTOP, 0, 628, 10, 132, 26, IDC_BTN_HOTKEY, st->font);
        MakeCtrl(hwnd, L"STATIC", L"(点击录制)", 0, 0, 766, 16, 86, 22, 0, st->font);

        MakeCtrl(hwnd, L"STATIC", L"搜索:", 0, 0, 16, 46, 40, 22, 0, st->font);
        st->edtSearch = MakeCtrl(hwnd, L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP,
            WS_EX_CLIENTEDGE, 58, 43, 210, 24, IDC_EDT_SEARCH, st->font);

        MakeCtrl(hwnd, L"STATIC", L"模块筛选:", 0, 0, 278, 46, 66, 22, 0, st->font);
        st->cmbFilterMod = MakeCtrl(hwnd, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP,
            0, 346, 43, 100, 160, IDC_CMB_FILTER_MOD, st->font);
        static const wchar_t* kFilterMods[] = {L"全部模块", L"全部", L"建模", L"钣金", L"制图", L"草图", L"装配"};
        for (const wchar_t* m : kFilterMods)
        {
            SendMessageW(st->cmbFilterMod, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(m));
        }
        SendMessageW(st->cmbFilterMod, CB_SETCURSEL, 0, 0);

        MakeCtrl(hwnd, L"STATIC",
            L"提示: 双击行可快速改简码；原有 Ctrl/Shift/Alt 组合键保持有效",
            0, 0, 458, 46, 396, 22, 0, st->font);

        st->list = CreateWindowExW(
            WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
            16, 76, 836, 320, hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_LIST_SHORTCUTS)),
            g_hModule, nullptr);
        SendMessageW(st->list, WM_SETFONT, reinterpret_cast<WPARAM>(st->font), TRUE);
        ListView_SetExtendedListViewStyle(st->list, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

        struct ColDef { const wchar_t* title; int width; };
        const ColDef cols[] = {
            {L"简码", 75},
            {L"命令名称", 150},
            {L"命令ID (ButtonName)", 315},
            {L"分类", 95},
            {L"适用模块", 85},
            {L"状态", 65}
        };
        for (int i = 0; i < 6; ++i)
        {
            LVCOLUMNW col{};
            col.mask = LVCF_TEXT | LVCF_WIDTH;
            col.pszText = const_cast<wchar_t*>(cols[i].title);
            col.cx = cols[i].width;
            ListView_InsertColumn(st->list, i, &col);
        }

        MakeCtrl(hwnd, L"STATIC", L"简码:", 0, 0, 16, 412, 38, 22, 0, st->font);
        st->edtAlias = MakeCtrl(hwnd, L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP,
            WS_EX_CLIENTEDGE, 56, 409, 68, 24, IDC_EDT_ALIAS, st->font);

        MakeCtrl(hwnd, L"STATIC", L"名称:", 0, 0, 132, 412, 38, 22, 0, st->font);
        st->edtName = MakeCtrl(hwnd, L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP,
            WS_EX_CLIENTEDGE, 172, 409, 126, 24, IDC_EDT_NAME, st->font);

        MakeCtrl(hwnd, L"STATIC", L"命令ID:", 0, 0, 306, 412, 52, 22, 0, st->font);
        st->edtCmdId = MakeCtrl(hwnd, L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP,
            WS_EX_CLIENTEDGE, 360, 409, 200, 24, IDC_EDT_CMDID, st->font);

        MakeCtrl(hwnd, L"STATIC", L"分类:", 0, 0, 568, 412, 38, 22, 0, st->font);
        st->cmbCat = MakeCtrl(hwnd, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP,
            0, 608, 409, 90, 120, IDC_CMB_CAT, st->font);
        SendMessageW(st->cmbCat, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"NX原生"));
        SendMessageW(st->cmbCat, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"智辉插件"));
        SendMessageW(st->cmbCat, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"自定义"));
        SendMessageW(st->cmbCat, CB_SETCURSEL, 1, 0);

        MakeCtrl(hwnd, L"STATIC", L"模块:", 0, 0, 706, 412, 38, 22, 0, st->font);
        st->cmbMod = MakeCtrl(hwnd, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP,
            0, 746, 409, 106, 160, IDC_CMB_MOD, st->font);
        static const wchar_t* kMods[] = {L"全部", L"建模", L"钣金", L"制图", L"草图", L"装配"};
        for (const wchar_t* m : kMods)
        {
            SendMessageW(st->cmbMod, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(m));
        }
        SendMessageW(st->cmbMod, CB_SETCURSEL, 0, 0);

        MakeCtrl(hwnd, L"BUTTON", L"修改选中", WS_TABSTOP, 0, 16, 450, 92, 30, IDC_BTN_UPDATE_SEL, st->font);
        MakeCtrl(hwnd, L"BUTTON", L"新增命令", WS_TABSTOP, 0, 116, 450, 92, 30, IDC_BTN_ADD_NEW, st->font);
        MakeCtrl(hwnd, L"BUTTON", L"读取上个命令", WS_TABSTOP, 0, 216, 450, 114, 30, IDC_BTN_READ_LAST, st->font);
        MakeCtrl(hwnd, L"BUTTON", L"启用 / 禁用", WS_TABSTOP, 0, 338, 450, 96, 30, IDC_BTN_TOGGLE, st->font);
        MakeCtrl(hwnd, L"BUTTON", L"删除选中", WS_TABSTOP, 0, 442, 450, 88, 30, IDC_BTN_DELETE, st->font);
        MakeCtrl(hwnd, L"BUTTON", L"恢复默认", WS_TABSTOP, 0, 538, 450, 88, 30, IDC_BTN_RESET, st->font);
        MakeCtrl(hwnd, L"BUTTON", L"保存并生效", BS_DEFPUSHBUTTON | WS_TABSTOP, 0, 646, 450, 110, 30, IDC_BTN_SAVE, st->font);
        MakeCtrl(hwnd, L"BUTTON", L"关闭", WS_TABSTOP, 0, 764, 450, 88, 30, IDC_BTN_CLOSE, st->font);

        SyncCheckboxesFromConfig(*st);
        PopulateList(*st);
        return 0;
    }
    case WM_NOTIFY:
    {
        if (!st) break;
        auto* nm = reinterpret_cast<NMHDR*>(lParam);
        if (nm->idFrom == IDC_LIST_SHORTCUTS)
        {
            if (nm->code == LVN_ITEMCHANGED)
            {
                auto* lvn = reinterpret_cast<NMLISTVIEW*>(lParam);
                if ((lvn->uNewState & LVIS_SELECTED) != 0)
                {
                    LoadSelectedIntoEditor(*st);
                }
            }
            else if (nm->code == NM_DBLCLK)
            {
                LoadSelectedIntoEditor(*st);
                SetFocus(st->edtAlias);
                SendMessageW(st->edtAlias, EM_SETSEL, 0, -1);
            }
            else if (nm->code == LVN_COLUMNCLICK)
            {
                auto* lvn = reinterpret_cast<NMLISTVIEW*>(lParam);
                const int selIdx = SelectedConfigIndex(*st);
                if (st->sortColumn == lvn->iSubItem)
                {
                    st->sortAscending = !st->sortAscending;
                }
                else
                {
                    st->sortColumn = lvn->iSubItem;
                    st->sortAscending = true;
                }
                PopulateList(*st, selIdx);
            }
        }
        break;
    }
    case WM_COMMAND:
    {
        if (!st) break;
        const int id = LOWORD(wParam);
        const int code = HIWORD(wParam);
        if (id == IDC_EDT_SEARCH && code == EN_CHANGE)
        {
            PopulateList(*st);
            return 0;
        }
        if (id == IDC_CMB_FILTER_MOD && code == CBN_SELCHANGE)
        {
            PopulateList(*st);
            return 0;
        }
        switch (id)
        {
        case IDC_CHK_ENABLED:
        case IDC_CHK_REPEAT:
        case IDC_CHK_HUD:
            st->dirty = true;
            return 0;
        case IDC_BTN_HOTKEY:
            st->capturingHotkey = true;
            SetWindowTextW(st->btnHotkey, L"请按下组合键...");
            return 0;
        case IDC_BTN_READ_LAST:
        {
            LastCommandInfo last{};
            if (!ReadLastCommandFromNxSyslog(*st, last) && !LoadLastCommandW(st->lastCmdPath, last))
            {
                MessageBoxW(hwnd,
                    L"暂未记录到上一步命令。\r\n请先在 NX 界面点击或运行任意命令后再打开本窗口读取。",
                    L"读取上个命令", MB_OK | MB_ICONINFORMATION);
                return 0;
            }
            int existingIdx = -1;
            for (int i = 0; i < static_cast<int>(st->config.entries.size()); ++i)
            {
                if (st->config.entries[static_cast<std::size_t>(i)].commandId == last.commandId)
                {
                    existingIdx = i;
                    break;
                }
            }
            if (existingIdx >= 0)
            {
                const auto& e = st->config.entries[static_cast<std::size_t>(existingIdx)];
                SetWindowTextW(st->edtAlias, e.alias.c_str());
                SetWindowTextW(st->edtName, e.displayName.c_str());
                SetWindowTextW(st->edtCmdId, e.commandId.c_str());
                SelectComboText(st->cmbCat, e.category, 2);
                SelectComboText(st->cmbMod, e.module.empty() ? L"全部" : e.module, 0);
                for (int row = 0; row < static_cast<int>(st->visibleIndices.size()); ++row)
                {
                    if (static_cast<int>(st->visibleIndices[static_cast<std::size_t>(row)]) == existingIdx)
                    {
                        ListView_SetItemState(st->list, row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
                        ListView_EnsureVisible(st->list, row, FALSE);
                        break;
                    }
                }
            }
            else
            {
                SetWindowTextW(st->edtAlias, L"");
                SetWindowTextW(st->edtName, last.displayName.c_str());
                SetWindowTextW(st->edtCmdId, last.commandId.c_str());
                SelectComboText(st->cmbCat, last.category, 2);
                SelectComboText(st->cmbMod, L"全部", 0);
            }
            SetFocus(st->edtAlias);
            SendMessageW(st->edtAlias, EM_SETSEL, 0, -1);
            return 0;
        }
        case IDC_BTN_UPDATE_SEL:
        {
            const int idx = SelectedConfigIndex(*st);
            if (idx < 0)
            {
                MessageBoxW(hwnd, L"请先在列表中选中要修改的快捷键行。", L"快捷键", MB_OK | MB_ICONWARNING);
                return 0;
            }
            ShortcutEntry updated;
            if (!ValidateEditorInput(hwnd, *st, updated))
            {
                return 0;
            }
            const int dup = FindDuplicateAliasIndex(*st, updated.alias, updated.module, idx);
            if (dup >= 0)
            {
                const auto& other = st->config.entries[static_cast<std::size_t>(dup)];
                const std::wstring msg =
                    L"快捷键重复！\r\n\r\n简码「" + updated.alias + L"」在「" + updated.module +
                    L"」模块下已被「" + other.displayName + L" (" + other.commandId +
                    L")」占用。\r\n请更换简码或选择其他适用模块。";
                MessageBoxW(hwnd, msg.c_str(), L"快捷键重复提示", MB_OK | MB_ICONWARNING);
                return 0;
            }
            updated.enabled = st->config.entries[static_cast<std::size_t>(idx)].enabled;
            st->config.entries[static_cast<std::size_t>(idx)] = updated;
            st->dirty = true;
            PopulateList(*st, idx);
            return 0;
        }
        case IDC_BTN_ADD_NEW:
        {
            ShortcutEntry created;
            if (!ValidateEditorInput(hwnd, *st, created))
            {
                return 0;
            }
            const int dup = FindDuplicateAliasIndex(*st, created.alias, created.module, -1);
            if (dup >= 0)
            {
                const auto& other = st->config.entries[static_cast<std::size_t>(dup)];
                const std::wstring msg =
                    L"快捷键重复！\r\n\r\n简码「" + created.alias + L"」在「" + created.module +
                    L"」模块下已被「" + other.displayName + L" (" + other.commandId +
                    L")」占用。\r\n若要修改原命令请选中该行后点「修改选中」，或更换其他简码。";
                MessageBoxW(hwnd, msg.c_str(), L"快捷键重复提示", MB_OK | MB_ICONWARNING);
                return 0;
            }
            st->config.entries.push_back(created);
            st->dirty = true;
            PopulateList(*st, static_cast<int>(st->config.entries.size()) - 1);
            return 0;
        }
        case IDC_BTN_TOGGLE:
        {
            const int idx = SelectedConfigIndex(*st);
            if (idx >= 0)
            {
                auto& entry = st->config.entries[static_cast<std::size_t>(idx)];
                entry.enabled = !entry.enabled;
                st->dirty = true;
                PopulateList(*st, idx);
            }
            return 0;
        }
        case IDC_BTN_DELETE:
        {
            const int idx = SelectedConfigIndex(*st);
            if (idx >= 0)
            {
                st->config.entries.erase(st->config.entries.begin() + idx);
                st->dirty = true;
                PopulateList(*st);
            }
            return 0;
        }
        case IDC_BTN_RESET:
        {
            if (MessageBoxW(hwnd, L"确定恢复为默认快捷键配置吗？当前自定义修改将被覆盖。",
                L"快捷键", MB_YESNO | MB_ICONQUESTION) == IDYES)
            {
                st->config = DefaultShortcutConfig();
                st->dirty = true;
                SyncCheckboxesFromConfig(*st);
                PopulateList(*st);
            }
            return 0;
        }
        case IDC_BTN_SAVE:
            SaveAndNotifyLoader(*st, true);
            return 0;
        case IDC_BTN_CLOSE:
            PostMessageW(hwnd, WM_CLOSE, 0, 0);
            return 0;
        default:
            break;
        }
        break;
    }
    case WM_CLOSE:
        if (st && st->dirty)
        {
            const int ans = MessageBoxW(hwnd, L"快捷键配置已修改但尚未保存，是否保存并生效？",
                L"快捷键", MB_YESNOCANCEL | MB_ICONQUESTION);
            if (ans == IDCANCEL)
            {
                return 0;
            }
            if (ans == IDYES && !SaveAndNotifyLoader(*st, false))
            {
                return 0;
            }
        }
        if (st)
        {
            st->running = false;
        }
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        if (st && st->font)
        {
            DeleteObject(st->font);
            st->font = nullptr;
        }
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void ShowConfigDialog()
{
    const std::wstring pluginRoot = ResolvePluginRoot();
    if (pluginRoot.empty())
    {
        return;
    }

    DialogState st;
    st.pluginRoot = pluginRoot;
    st.configPath = pluginRoot + L"\\config\\KuaiJieJian.ini";
    st.lastCmdPath = pluginRoot + L"\\config\\KuaiJieJian.lastcmd";
    st.config = LoadOrCreateShortcutConfig(st.configPath);

    constexpr const wchar_t* kClassName = L"ZhihuiKuaiJieJianConfigWndClass";
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = ConfigWndProc;
    wc.hInstance = g_hModule;
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kClassName;
    RegisterClassExW(&wc);

    HWND owner = reinterpret_cast<HWND>(UF_UI_get_default_parent());
    constexpr int winW = 884;
    constexpr int winH = 536;
    int x = CW_USEDEFAULT;
    int y = CW_USEDEFAULT;
    RECT ownerRc{};
    if (owner && GetWindowRect(owner, &ownerRc))
    {
        x = ownerRc.left + ((ownerRc.right - ownerRc.left) - winW) / 2;
        y = ownerRc.top + ((ownerRc.bottom - ownerRc.top) - winH) / 2;
    }

    HWND dlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME, kClassName, L"智辉钣金 - 快捷键设置（按空格执行）",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        x, y, winW, winH, owner, nullptr, g_hModule, &st);
    if (!dlg)
    {
        return;
    }

    if (owner)
    {
        EnableWindow(owner, FALSE);
    }
    ShowWindow(dlg, SW_SHOW);
    UpdateWindow(dlg);

    MSG msg{};
    while (st.running && GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        if (st.capturingHotkey && (msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN))
        {
            if (msg.wParam == VK_ESCAPE)
            {
                st.capturingHotkey = false;
                SyncCheckboxesFromConfig(st);
                continue;
            }
            bool valid = false;
            const std::wstring captured = FormatCapturedHotkey(msg.wParam, valid);
            if (valid)
            {
                st.config.toggleHotkey = captured;
                st.dirty = true;
                st.capturingHotkey = false;
                SetWindowTextW(st.btnHotkey, captured.c_str());
            }
            else if (msg.wParam != VK_CONTROL && msg.wParam != VK_SHIFT && msg.wParam != VK_MENU)
            {
                MessageBoxW(dlg,
                    L"请使用组合键（如 Alt+F1、Ctrl+Shift+K）或功能键（F1~F12 / `），避免与单字母简码冲突。",
                    L"设置监听热键", MB_OK | MB_ICONINFORMATION);
                st.capturingHotkey = false;
                SyncCheckboxesFromConfig(st);
            }
            continue;
        }
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE)
        {
            PostMessageW(dlg, WM_CLOSE, 0, 0);
            continue;
        }
        if (!IsDialogMessageW(dlg, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (owner)
    {
        EnableWindow(owner, TRUE);
        SetForegroundWindow(owner);
    }
    UnregisterClassW(kClassName, g_hModule);
}

void ShowError(const wchar_t* message)
{
    MessageBoxW(nullptr, message, L"快捷键", MB_OK | MB_ICONERROR);
    try
    {
        NXOpen::Session::GetSession()->LogFile()->WriteLine(
            "KuaiJieJian: authorization or startup failed.");
    }
    catch (...)
    {
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

extern "C" DllExport void ufusr(char*, int* returnCode, int)
{
    if (returnCode)
    {
        *returnCode = 0;
    }
    if (!Authorized())
    {
        ShowError(L"授权校验失败，命令未执行。");
        if (returnCode)
        {
            *returnCode = 1;
        }
        return;
    }
    const int status = UF_initialize();
    if (status != 0)
    {
        ShowError(L"UF 初始化失败，命令未执行。");
        if (returnCode)
        {
            *returnCode = status;
        }
        return;
    }
    try
    {
        ShowConfigDialog();
    }
    catch (const NXOpen::NXException& ex)
    {
        ShowError(L"快捷键设置打开失败。");
        try
        {
            NXOpen::Session::GetSession()->LogFile()->WriteLine(
                ex.Message() ? ex.Message() : "KuaiJieJian NX exception.");
        }
        catch (...)
        {
        }
        if (returnCode)
        {
            *returnCode = ex.ErrorCode();
        }
    }
    catch (const std::exception& ex)
    {
        ShowError(L"快捷键设置打开失败。");
        try
        {
            NXOpen::Session::GetSession()->LogFile()->WriteLine(ex.what());
        }
        catch (...)
        {
        }
        if (returnCode)
        {
            *returnCode = 1;
        }
    }
    catch (...)
    {
        ShowError(L"快捷键设置发生未知错误。");
        if (returnCode)
        {
            *returnCode = 1;
        }
    }
    UF_terminate();
}

extern "C" DllExport int ufusr_ask_unload()
{
    return UF_UNLOAD_IMMEDIATELY;
}
