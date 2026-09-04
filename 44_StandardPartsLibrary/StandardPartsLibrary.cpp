#include "StandardPartsLibrary.hpp"

#include <uf.h>
#include <uf_assem.h>
#include <uf_csys.h>
#include <uf_disp.h>
#include <uf_group.h>
#include <uf_modl.h>
#include <uf_obj.h>
#include <uf_part.h>
#include <uf_retiring_ugopenint.h>
#include <uf_ui.h>

#include <NXOpen/BlockStyler_BlockDialog.hxx>
#include <NXOpen/BlockStyler_CompositeBlock.hxx>
#include <NXOpen/BlockStyler_DrawingArea.hxx>
#include <NXOpen/BlockStyler_PropertyList.hxx>
#include <NXOpen/BlockStyler_SelectObject.hxx>
#include <NXOpen/BlockStyler_SpecifyOrientation.hxx>
#include <NXOpen/BlockStyler_UIBlock.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXMessageBox.hxx>
#include <NXOpen/Selection.hxx>
#include <NXOpen/UI.hxx>

#include <Windows.h>
#include <CommCtrl.h>
#include <commdlg.h>
#include <ShlObj.h>
#include <shobjidl_core.h>
#include <shellapi.h>

#ifdef CreateDialog
#undef CreateDialog
#endif

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <new>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{
constexpr wchar_t kTitle[] = L"智辉标准件库";
constexpr wchar_t kDefaultRoot[] = L"D:\\UG智辉标准件库";

enum ControlId
{
    ID_ROOT = 1001, ID_BROWSE_ROOT, ID_REFRESH, ID_SEARCH, ID_CATEGORY,
    ID_LIST, ID_NAME, ID_EDIT_CATEGORY, ID_SOURCE, ID_BROWSE_SOURCE,
    ID_ADD_CURRENT, ID_ADD_FILE, ID_DELETE,
    ID_MODE_ASSEMBLY, ID_MODE_BODY, ID_INSERT, ID_CLOSE, ID_STATUS, ID_PREVIEW,
    ID_DETAIL_NAME, ID_DETAIL_CATEGORY, ID_DETAIL_FILE, ID_SPEC,
    ID_FILTER_ALL, ID_FILTER_STATIC, ID_FILTER_PARAM,
    ID_EDIT_SPEC, ID_PLACE_WCS, ID_PLACE_POINT, ID_PLACE_FACE,
    ID_PLACE_CIRCLE, ID_PICK_PLACE, ID_QUICK_ORIENT, ID_PLACE_STATUS,
    ID_AUTO_TRIM, ID_SELECT_TRIM, ID_TRIM_STATUS, ID_LAYER,
    ID_MANAGE_CONFIG, ID_PATTERN_SINGLE, ID_PATTERN_X, ID_PATTERN_Y,
    ID_PATTERN_DIAGONAL, ID_PATTERN_FOUR, ID_PATTERN_CENTER, ID_PATTERN_ARRAY,
    ID_PARAM_LABEL1, ID_PARAM_LABEL2, ID_PARAM_LABEL3, ID_PARAM_LABEL4,
    ID_PARAM_VALUE1, ID_PARAM_VALUE2, ID_PARAM_VALUE3, ID_PARAM_VALUE4
};

struct LibraryItem
{
    std::wstring id;
    std::wstring name;
    std::wstring category;
    std::wstring relativePath;
    std::wstring specification;
    bool parameterized = false;
};

struct AppState
{
    HWND window = nullptr;
    HWND parent = nullptr;
    HWND list = nullptr;
    HWND category = nullptr;
    HWND preview = nullptr;
    HBITMAP previewBitmap = nullptr;
    HIMAGELIST thumbnails = nullptr;
    HFONT font = nullptr;
    std::vector<LibraryItem> items;
    std::vector<std::size_t> visible;
    std::vector<std::wstring> categories;
    std::wstring selectedCategory;
    int libraryFilter = 0;
    int placementMode = 0;
    int patternMode = 0;
    bool hasPlacement = false;
    double placementOrigin[3]{};
    double placementMatrix[9]{1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0};
    std::vector<tag_t> trimTargets;
    bool refreshingCategories = false;
    bool refreshingList = false;
    bool running = true;
    bool ownsUfSession = false;
    bool ownsOleSession = false;
    bool embedded = false;
    HMODULE moduleReference = nullptr;
    std::wstring windowClassName;
};

AppState* g_appState = nullptr;
HWND g_managerWindow = nullptr;

struct ModuleReleaseContext
{
    HMODULE module = nullptr;
    std::wstring mainWindowClass;
};

DWORD WINAPI ReleaseModuleAfterWindowProc(void* parameter)
{
    ModuleReleaseContext* context =
        static_cast<ModuleReleaseContext*>(parameter);
    HMODULE module = context->module;
    // WM_NCDESTROY must return before the final module reference is released;
    // otherwise execution would continue in code that has already been unmapped.
    Sleep(50);
    UnregisterClassW(L"ZhihuiStandardPartsManagerWindow", module);
    if (!context->mainWindowClass.empty())
        UnregisterClassW(context->mainWindowClass.c_str(), module);
    delete context;
    FreeLibraryAndExitThread(module, 0);
}

void UpdatePreview(AppState* state);
std::wstring Lower(std::wstring value);

std::wstring Trim(std::wstring value)
{
    const auto nonSpace = [](wchar_t ch) { return !iswspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), nonSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), nonSpace).base(), value.end());
    return value;
}

std::wstring GetText(HWND parent, int id)
{
    const HWND control = GetDlgItem(parent, id);
    const int length = GetWindowTextLengthW(control);
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    if (length > 0) GetWindowTextW(control, result.data(), length + 1);
    return result;
}

void SetText(HWND parent, int id, const std::wstring& text)
{
    SetWindowTextW(GetDlgItem(parent, id), text.c_str());
}

void SetStatus(AppState* state, const std::wstring& text)
{
    if (state != nullptr) SetText(state->window, ID_STATUS, text);
}

std::string ToAnsi(const std::wstring& value)
{
    if (value.empty()) return {};
    const int count = WideCharToMultiByte(CP_ACP, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (count <= 1) return {};
    std::string result(static_cast<std::size_t>(count), '\0');
    WideCharToMultiByte(CP_ACP, 0, value.c_str(), -1, result.data(), count, nullptr, nullptr);
    result.pop_back();
    return result;
}

std::wstring FromAnsi(const std::string& value)
{
    if (value.empty()) return {};
    const int count = MultiByteToWideChar(CP_ACP, 0, value.c_str(), -1, nullptr, 0);
    if (count <= 1) return {};
    std::wstring result(static_cast<std::size_t>(count), L'\0');
    MultiByteToWideChar(CP_ACP, 0, value.c_str(), -1, result.data(), count);
    result.pop_back();
    return result;
}

std::wstring UfError(int code)
{
    char message[512] = {};
    if (UF_get_fail_message(code, message) != 0) return L"NX 错误 " + std::to_wstring(code);
    const int count = MultiByteToWideChar(CP_ACP, 0, message, -1, nullptr, 0);
    if (count <= 1) return L"NX 错误 " + std::to_wstring(code);
    std::wstring result(static_cast<std::size_t>(count), L'\0');
    MultiByteToWideChar(CP_ACP, 0, message, -1, result.data(), count);
    result.pop_back();
    return result;
}

bool IsPartFile(const fs::path& path)
{
    std::wstring extension = path.extension().wstring();
    std::transform(extension.begin(), extension.end(), extension.begin(), towlower);
    return extension == L".prt";
}

fs::path FindSidecarPreview(const fs::path& model)
{
    static const wchar_t* extensions[] = {L".png", L".jpg", L".jpeg", L".bmp"};
    std::error_code ec;
    for (const wchar_t* extension : extensions)
    {
        fs::path candidate = model;
        candidate.replace_extension(extension);
        if (fs::exists(candidate, ec) && fs::is_regular_file(candidate, ec)) return candidate;
        ec.clear();
    }
    return {};
}

std::wstring SanitizeFileName(std::wstring value)
{
    constexpr wchar_t invalid[] = L"<>:\"/\\|?*";
    for (wchar_t& ch : value)
        if (wcschr(invalid, ch) != nullptr || ch < 32) ch = L'_';
    value = Trim(value);
    while (!value.empty() && (value.back() == L'.' || value.back() == L' ')) value.pop_back();
    return value.empty() ? L"标准件" : value;
}

std::vector<std::wstring> SplitTabs(const std::wstring& line)
{
    std::vector<std::wstring> result;
    std::size_t begin = 0;
    while (true)
    {
        const std::size_t end = line.find(L'\t', begin);
        result.push_back(line.substr(begin, end == std::wstring::npos ? end : end - begin));
        if (end == std::wstring::npos) break;
        begin = end + 1;
    }
    return result;
}

bool ReadUtf16File(const fs::path& path, std::wstring& text)
{
    text.clear();
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return false;
    stream.seekg(0, std::ios::end);
    const auto byteCount = stream.tellg();
    if (byteCount < 0 || (static_cast<std::uint64_t>(byteCount) % 2) != 0) return false;
    stream.seekg(0);
    std::vector<wchar_t> chars(static_cast<std::size_t>(byteCount) / 2);
    stream.read(reinterpret_cast<char*>(chars.data()), static_cast<std::streamsize>(byteCount));
    if (!stream && byteCount != 0) return false;
    std::size_t offset = !chars.empty() && chars[0] == 0xFEFF ? 1 : 0;
    text.assign(chars.begin() + static_cast<std::ptrdiff_t>(offset), chars.end());
    return true;
}

bool WriteUtf16File(const fs::path& path, const std::wstring& text)
{
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) return false;
    const wchar_t bom = 0xFEFF;
    stream.write(reinterpret_cast<const char*>(&bom), sizeof(bom));
    stream.write(reinterpret_cast<const char*>(text.data()),
                 static_cast<std::streamsize>(text.size() * sizeof(wchar_t)));
    stream.flush();
    return stream.good();
}

fs::path LibraryRoot(AppState* state)
{
    const std::wstring input = Trim(GetText(state->window, ID_ROOT));
    return input.empty() ? fs::path(kDefaultRoot) : fs::path(input);
}

std::wstring LoadRootPreference()
{
    wchar_t value[2048] = {};
    DWORD bytes = sizeof(value);
    if (RegGetValueW(HKEY_CURRENT_USER,
                     L"Software\\ZhihuiSheetMetal\\StandardPartsLibrary",
                     L"LibraryRoot", RRF_RT_REG_SZ, nullptr, value, &bytes) == ERROR_SUCCESS &&
        value[0] != L'\0')
        return value;
    return kDefaultRoot;
}

void SaveRootPreference(AppState* state)
{
    const std::wstring root = LibraryRoot(state).wstring();
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER,
                        L"Software\\ZhihuiSheetMetal\\StandardPartsLibrary",
                        0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) == ERROR_SUCCESS)
    {
        RegSetValueExW(key, L"LibraryRoot", 0, REG_SZ,
                       reinterpret_cast<const BYTE*>(root.c_str()),
                       static_cast<DWORD>((root.size() + 1) * sizeof(wchar_t)));
        RegCloseKey(key);
    }
}

fs::path IndexPath(AppState* state)
{
    return LibraryRoot(state) / L"library.tsv";
}

bool EnsureLibraryFolders(AppState* state, std::wstring& error)
{
    std::error_code ec;
    const fs::path root = LibraryRoot(state);
    fs::create_directories(root / L"models", ec);
    if (ec) { error = std::wstring(L"无法创建模型目录：") + FromAnsi(ec.message()); return false; }
    fs::create_directories(root / L"backup", ec);
    if (ec) { error = std::wstring(L"无法创建备份目录：") + FromAnsi(ec.message()); return false; }
    fs::create_directories(root / L"Lib", ec);
    if (ec) { error = std::wstring(L"无法创建无参库目录：") + FromAnsi(ec.message()); return false; }
    fs::create_directories(root / L"LibParam", ec);
    if (ec) { error = std::wstring(L"无法创建有参库目录：") + FromAnsi(ec.message()); return false; }
    return true;
}

bool SaveIndex(AppState* state, std::wstring& error)
{
    if (!EnsureLibraryFolders(state, error)) return false;
    std::wostringstream content;
    content << L"# ZHIHUI_STANDARD_PARTS_V3\r\n";
    content << L"# id\tname\tcategory\trelative_model_path\tspecification\tparameterized\r\n";
    for (const auto& item : state->items)
        content << item.id << L'\t' << item.name << L'\t' << item.category << L'\t'
                << item.relativePath << L'\t' << item.specification << L'\t'
                << (item.parameterized ? L"1" : L"0") << L"\r\n";

    const fs::path index = IndexPath(state);
    const fs::path temporary = index.wstring() + L".tmp";
    if (!WriteUtf16File(temporary, content.str()))
    {
        error = L"无法写入标准件索引临时文件。";
        return false;
    }
    std::error_code ec;
    if (fs::exists(index))
    {
        SYSTEMTIME now{};
        GetLocalTime(&now);
        wchar_t stamp[32] = {};
        swprintf_s(stamp, L"%04u%02u%02u_%02u%02u%02u", now.wYear, now.wMonth,
                   now.wDay, now.wHour, now.wMinute, now.wSecond);
        fs::copy_file(index, LibraryRoot(state) / L"backup" /
                      (std::wstring(L"library_") + stamp + L".tsv"),
                      fs::copy_options::overwrite_existing, ec);
    }
    if (!MoveFileExW(temporary.c_str(), index.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        DeleteFileW(temporary.c_str());
        error = L"无法替换标准件索引，请检查目录权限。";
        return false;
    }
    return true;
}

void ScanLibraryFolder(AppState* state, const fs::path& base, bool parameterized)
{
    std::error_code ec;
    if (!fs::exists(base, ec)) return;
    const fs::path root = LibraryRoot(state);
    for (fs::recursive_directory_iterator iterator(base, fs::directory_options::skip_permission_denied, ec), end;
         iterator != end; iterator.increment(ec))
    {
        if (ec) { ec.clear(); continue; }
        if (!iterator->is_regular_file(ec) || !IsPartFile(iterator->path())) continue;
        const fs::path relativeToRoot = fs::relative(iterator->path(), root, ec);
        if (ec) { ec.clear(); continue; }
        const std::wstring relativeText = relativeToRoot.wstring();
        if (std::any_of(state->items.begin(), state->items.end(),
            [&](const LibraryItem& item) { return Lower(item.relativePath) == Lower(relativeText); }))
            continue;
        const fs::path relativeToBase = fs::relative(iterator->path(), base, ec);
        if (ec) { ec.clear(); continue; }
        std::vector<std::wstring> components;
        for (const auto& component : relativeToBase) components.push_back(component.wstring());
        const std::wstring specification = iterator->path().stem().wstring();
        std::wstring category = L"未分类";
        std::wstring family = specification;
        if (components.size() >= 2) category = components.front();
        if (components.size() >= 3) family = components[components.size() - 2];
        const std::wstring id = L"folder_" +
            std::to_wstring(std::hash<std::wstring>{}(Lower(relativeText)));
        state->items.push_back({id, family, category, relativeText, specification, parameterized});
    }
}

void LoadIndex(AppState* state)
{
    state->items.clear();
    std::wstring error;
    if (!EnsureLibraryFolders(state, error)) { SetStatus(state, error); return; }
    const fs::path index = IndexPath(state);
    if (!fs::exists(index))
    {
        SaveIndex(state, error);
    }
    else
    {
        std::wstring text;
        if (!ReadUtf16File(index, text))
        {
            SetStatus(state, L"索引文件无法读取或不是 UTF-16 格式。");
            return;
        }
        std::wistringstream lines(text);
        std::wstring line;
        while (std::getline(lines, line))
        {
            if (!line.empty() && line.back() == L'\r') line.pop_back();
            if (line.empty() || line[0] == L'#') continue;
            const auto fields = SplitTabs(line);
            if (fields.size() >= 4 && !fields[0].empty() && !fields[3].empty())
                state->items.push_back({fields[0], fields[1], fields[2], fields[3],
                                        fields.size() >= 5 && !fields[4].empty()
                                            ? fields[4] : fs::path(fields[3]).stem().wstring(),
                                        fields.size() >= 6 ? fields[5] == L"1" :
                                            Lower(fields[3]).rfind(L"libparam\\", 0) == 0});
        }
    }
    ScanLibraryFolder(state, LibraryRoot(state) / L"Lib", false);
    ScanLibraryFolder(state, LibraryRoot(state) / L"LibParam", true);
    SetStatus(state, L"已载入 " + std::to_wstring(state->items.size()) + L" 个标准件。");
}

void RefreshCategories(AppState* state)
{
    state->refreshingCategories = true;
    const std::wstring old = state->selectedCategory;
    TreeView_DeleteAllItems(state->category);
    state->categories.clear();
    std::set<std::wstring> uniqueCategories;
    for (const auto& item : state->items) uniqueCategories.insert(item.category);
    state->categories.assign(uniqueCategories.begin(), uniqueCategories.end());

    TVINSERTSTRUCTW insert{};
    insert.hParent = TVI_ROOT;
    insert.hInsertAfter = TVI_LAST;
    insert.item.mask = TVIF_TEXT | TVIF_PARAM;
    std::wstring rootText = L"全部标准件  (" + std::to_wstring(state->items.size()) + L")";
    insert.item.pszText = rootText.data();
    insert.item.lParam = 0;
    const HTREEITEM root = TreeView_InsertItem(state->category, &insert);
    HTREEITEM selected = root;
    for (std::size_t index = 0; index < state->categories.size(); ++index)
    {
        const auto& category = state->categories[index];
        const auto count = std::count_if(state->items.begin(), state->items.end(),
            [&](const LibraryItem& item) { return item.category == category; });
        std::wstring label = category + L"  (" + std::to_wstring(count) + L")";
        insert.hParent = root;
        insert.item.pszText = label.data();
        insert.item.lParam = static_cast<LPARAM>(index + 1);
        const HTREEITEM node = TreeView_InsertItem(state->category, &insert);
        if (category == old) selected = node;
    }
    state->selectedCategory = selected == root ? L"" : old;
    TreeView_Expand(state->category, root, TVE_EXPAND);
    TreeView_SelectItem(state->category, selected);
    state->refreshingCategories = false;
}

std::wstring Lower(std::wstring value)
{
    std::transform(value.begin(), value.end(), value.begin(), towlower);
    return value;
}

std::size_t SelectedFamilyIndex(AppState* state)
{
    const int row = ListView_GetNextItem(state->list, -1, LVNI_SELECTED);
    if (row < 0 || static_cast<std::size_t>(row) >= state->visible.size()) return SIZE_MAX;
    return state->visible[static_cast<std::size_t>(row)];
}

bool IsParameterizedFamily(const AppState* state, const LibraryItem& candidate)
{
    static_cast<void>(state);
    return candidate.parameterized;
}

void RefreshSpecifications(AppState* state)
{
    const HWND combo = GetDlgItem(state->window, ID_SPEC);
    if (combo == nullptr) return;
    const std::wstring old = GetText(state->window, ID_SPEC);
    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    const std::size_t family = SelectedFamilyIndex(state);
    if (family == SIZE_MAX) return;
    int selected = 0;
    int row = 0;
    for (std::size_t index = 0; index < state->items.size(); ++index)
    {
        const auto& item = state->items[index];
        if (item.category != state->items[family].category || item.name != state->items[family].name ||
            item.parameterized != state->items[family].parameterized)
            continue;
        const int inserted = static_cast<int>(SendMessageW(combo, CB_ADDSTRING, 0,
            reinterpret_cast<LPARAM>(item.specification.c_str())));
        SendMessageW(combo, CB_SETITEMDATA, inserted, static_cast<LPARAM>(index));
        if (item.specification == old) selected = row;
        ++row;
    }
    if (row > 0) SendMessageW(combo, CB_SETCURSEL, selected, 0);
}

void RefreshList(AppState* state)
{
    state->refreshingList = true;
    ListView_DeleteAllItems(state->list);
    state->visible.clear();
    const std::wstring search = Lower(Trim(GetText(state->window, ID_SEARCH)));
    std::set<std::wstring> families;
    for (std::size_t index = 0; index < state->items.size(); ++index)
    {
        const auto& item = state->items[index];
        if (!state->selectedCategory.empty() && item.category != state->selectedCategory) continue;
        const bool parameterized = IsParameterizedFamily(state, item);
        if (state->libraryFilter == 1 && parameterized) continue;
        if (state->libraryFilter == 2 && !parameterized) continue;
        if (!search.empty() && Lower(item.name + L" " + item.category + L" " +
                                     item.specification + L" " + item.relativePath).find(search) == std::wstring::npos)
            continue;
        const std::wstring familyKey = (item.parameterized ? L"P\n" : L"S\n") +
                                       item.category + L"\n" + item.name;
        if (!families.insert(familyKey).second) continue;
        LVITEMW row{};
        row.mask = LVIF_TEXT;
        row.iItem = static_cast<int>(state->visible.size());
        row.pszText = const_cast<wchar_t*>(item.name.c_str());
        ListView_InsertItem(state->list, &row);
        state->visible.push_back(index);
    }
    if (!state->visible.empty())
        ListView_SetItemState(state->list, 0, LVIS_SELECTED | LVIS_FOCUSED,
                              LVIS_SELECTED | LVIS_FOCUSED);
    state->refreshingList = false;
    RefreshSpecifications(state);
    UpdatePreview(state);
}

std::size_t SelectedIndex(AppState* state)
{
    const HWND combo = GetDlgItem(state->window, ID_SPEC);
    const LRESULT row = combo == nullptr ? CB_ERR : SendMessageW(combo, CB_GETCURSEL, 0, 0);
    if (row != CB_ERR)
    {
        const LRESULT data = SendMessageW(combo, CB_GETITEMDATA, row, 0);
        if (data != CB_ERR && data >= 0 && static_cast<std::size_t>(data) < state->items.size())
            return static_cast<std::size_t>(data);
    }
    return SelectedFamilyIndex(state);
}

std::wstring BrowsePartFile(HWND owner)
{
    wchar_t file[MAX_PATH] = {};
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrFilter = L"NX 部件 (*.prt)\0*.prt\0所有文件 (*.*)\0*.*\0";
    dialog.lpstrFile = file;
    dialog.nMaxFile = MAX_PATH;
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    return GetOpenFileNameW(&dialog) ? std::wstring(file) : std::wstring();
}

std::wstring BrowseFolder(HWND owner)
{
    BROWSEINFOW info{};
    info.hwndOwner = owner;
    info.lpszTitle = L"选择标准件库根目录";
    info.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    PIDLIST_ABSOLUTE item = SHBrowseForFolderW(&info);
    if (item == nullptr) return {};
    wchar_t path[MAX_PATH] = {};
    const bool ok = SHGetPathFromIDListW(item, path) != FALSE;
    CoTaskMemFree(item);
    return ok ? std::wstring(path) : std::wstring();
}

std::wstring CurrentPartPath()
{
    const tag_t part = UF_ASSEM_ask_work_part();
    if (part == NULL_TAG) return {};
    char path[MAX_FSPEC_BUFSIZE] = {};
    if (UF_PART_ask_part_name(part, path) != 0 || path[0] == '\0') return {};
    const int count = MultiByteToWideChar(CP_ACP, 0, path, -1, nullptr, 0);
    if (count <= 1) return {};
    std::wstring result(static_cast<std::size_t>(count), L'\0');
    MultiByteToWideChar(CP_ACP, 0, path, -1, result.data(), count);
    result.pop_back();
    return result;
}

bool AddToLibrary(AppState* state, const fs::path& source, std::wstring& error)
{
    if (!fs::exists(source) || !IsPartFile(source))
    {
        error = L"请选择有效的 NX .prt 部件文件。";
        return false;
    }
    std::wstring name = Trim(GetText(state->window, ID_NAME));
    std::wstring category = Trim(GetText(state->window, ID_EDIT_CATEGORY));
    std::wstring specification = Trim(GetText(state->window, ID_EDIT_SPEC));
    if (name.empty()) name = source.stem().wstring();
    if (category.empty()) category = L"用户自定义";
    if (specification.empty()) specification = source.stem().wstring();
    if (name.find(L'\t') != std::wstring::npos || category.find(L'\t') != std::wstring::npos ||
        specification.find(L'\t') != std::wstring::npos)
    {
        error = L"名称和分类不能包含制表符。";
        return false;
    }
    if (!EnsureLibraryFolders(state, error)) return false;
    const std::wstring id = std::to_wstring(GetTickCount64()) + L"_" + std::to_wstring(GetCurrentProcessId());
    const bool parameterized = state->libraryFilter == 2;
    const fs::path familyDirectory = LibraryRoot(state) /
        (parameterized ? L"LibParam" : L"Lib") /
        SanitizeFileName(category) / SanitizeFileName(name);
    std::error_code ec;
    fs::create_directories(familyDirectory, ec);
    if (ec)
    {
        error = std::wstring(L"创建标准件分类目录失败：") + FromAnsi(ec.message());
        return false;
    }
    fs::path target = familyDirectory / (SanitizeFileName(specification) + L".prt");
    if (fs::exists(target))
        target = familyDirectory / (SanitizeFileName(specification) + L"_" + id + L".prt");
    fs::copy_file(source, target, fs::copy_options::overwrite_existing, ec);
    if (ec)
    {
        error = std::wstring(L"复制部件模型失败：") + FromAnsi(ec.message());
        return false;
    }
    const fs::path previewSource = FindSidecarPreview(source);
    fs::path previewTarget;
    if (!previewSource.empty())
    {
        previewTarget = target;
        previewTarget.replace_extension(previewSource.extension());
        fs::copy_file(previewSource, previewTarget, fs::copy_options::overwrite_existing, ec);
        if (ec)
        {
            fs::remove(target, ec);
            error = std::wstring(L"复制标准件预览图失败：") + FromAnsi(ec.message());
            return false;
        }
    }
    state->items.push_back({id, name, category,
                            fs::relative(target, LibraryRoot(state)).wstring(),
                            specification, parameterized});
    if (!SaveIndex(state, error))
    {
        state->items.pop_back();
        fs::remove(target, ec);
        if (!previewTarget.empty()) fs::remove(previewTarget, ec);
        return false;
    }
    RefreshCategories(state);
    RefreshList(state);
    SetStatus(state, L"已入库：" + name);
    return true;
}

bool AskWcs(double origin[3], double matrix[9], std::wstring& error)
{
    tag_t wcs = NULL_TAG;
    tag_t matrixTag = NULL_TAG;
    int code = UF_CSYS_ask_wcs(&wcs);
    if (code == 0) code = UF_CSYS_ask_csys_info(wcs, &matrixTag, origin);
    if (code == 0) code = UF_CSYS_ask_matrix_values(matrixTag, matrix);
    if (code != 0) { error = L"无法读取当前 WCS：" + UfError(code); return false; }
    return true;
}

void UpdatePlacementStatus(AppState* state)
{
    static const wchar_t* modes[] = {L"点投影面", L"任意点", L"面中心", L"圆/圆弧中心"};
    std::wostringstream text;
    text.setf(std::ios::fixed);
    text.precision(2);
    text << (state->hasPlacement ? L"✓ " : L"○ ") << modes[state->placementMode];
    if (state->hasPlacement)
        text << L"  (" << state->placementOrigin[0] << L", " << state->placementOrigin[1]
             << L", " << state->placementOrigin[2] << L")";
    SetText(state->window, ID_PLACE_STATUS, text.str());
}

void SetPlacementMode(AppState* state, int mode)
{
    state->placementMode = std::clamp(mode, 0, 3);
    CheckRadioButton(state->window, ID_PLACE_WCS, ID_PLACE_CIRCLE,
                     ID_PLACE_WCS + state->placementMode);
    state->hasPlacement = false;
    UpdatePlacementStatus(state);
}

bool PickObjectCenter(AppState* state, const wchar_t* cue, std::wstring& error)
{
    ShowWindow(state->window, SW_HIDE);
    tag_t selected = NULL_TAG;
    int response = 0;
    double cursor[3]{};
    tag_t view = NULL_TAG;
    const std::string cueText = ToAnsi(cue);
    const int code = UF_UI_select_with_single_dialog(
        const_cast<char*>(cueText.c_str()), const_cast<char*>("选择"),
        UF_UI_SEL_SCOPE_WORK_PART, nullptr, nullptr, &response, &selected, cursor, &view);
    ShowWindow(state->window, SW_SHOW);
    SetForegroundWindow(state->window);
    if (code != 0)
    {
        error = L"选择对象失败：" + UfError(code);
        return false;
    }
    if (response != UF_UI_OBJECT_SELECTED && response != UF_UI_OBJECT_SELECTED_BY_NAME)
        return false;
    double box[6]{};
    const int boxCode = UF_MODL_ask_bounding_box(selected, box);
    if (boxCode != 0)
    {
        error = L"无法取得所选对象中心：" + UfError(boxCode);
        return false;
    }
    for (int axis = 0; axis < 3; ++axis)
        state->placementOrigin[axis] = (box[axis] + box[axis + 3]) * 0.5;
    bool orientedToFace = false;
    if (state->placementMode == 2)
    {
        int faceType = 0;
        int normalDirection = 0;
        double facePoint[3]{};
        double direction[3]{};
        double faceBox[6]{};
        double radius = 0.0;
        double radiusData = 0.0;
        if (UF_MODL_ask_face_data(selected, &faceType, facePoint, direction, faceBox,
                                  &radius, &radiusData, &normalDirection) == 0 &&
            faceType == 22)
        {
            if (normalDirection < 0)
                for (double& value : direction) value = -value;
            const double length = std::sqrt(direction[0] * direction[0] +
                                            direction[1] * direction[1] +
                                            direction[2] * direction[2]);
            if (length > 1.0e-9)
            {
                double z[3] = {direction[0] / length, direction[1] / length,
                               direction[2] / length};
                double reference[3] = {std::abs(z[0]) < 0.9 ? 1.0 : 0.0,
                                       std::abs(z[0]) < 0.9 ? 0.0 : 1.0, 0.0};
                double y[3] = {z[1] * reference[2] - z[2] * reference[1],
                               z[2] * reference[0] - z[0] * reference[2],
                               z[0] * reference[1] - z[1] * reference[0]};
                const double yLength = std::sqrt(y[0] * y[0] + y[1] * y[1] + y[2] * y[2]);
                for (double& value : y) value /= yLength;
                double x[3] = {y[1] * z[2] - y[2] * z[1],
                               y[2] * z[0] - y[0] * z[2],
                               y[0] * z[1] - y[1] * z[0]};
                std::copy(x, x + 3, state->placementMatrix);
                std::copy(y, y + 3, state->placementMatrix + 3);
                std::copy(z, z + 3, state->placementMatrix + 6);
                orientedToFace = true;
            }
        }
    }
    if (!orientedToFace && !AskWcs(cursor, state->placementMatrix, error)) return false;
    state->hasPlacement = true;
    UpdatePlacementStatus(state);
    return true;
}

void PickPlacement(AppState* state)
{
    std::wstring error;
    if (state->placementMode == 0)
    {
        ShowWindow(state->window, SW_HIDE);
        double pickedPoint[3]{};
        tag_t view = NULL_TAG;
        int pointResponse = 0;
        const int pointCode = UF_UI_specify_screen_position(
            const_cast<char*>("选择需要投影的点"), nullptr, nullptr,
            pickedPoint, &view, &pointResponse);
        ShowWindow(state->window, SW_SHOW);
        SetForegroundWindow(state->window);
        if (pointCode == 0 && pointResponse == UF_UI_PICK_RESPONSE)
        {
            int response = 0;
            tag_t selected = NULL_TAG;
            double cursor[3]{};
            tag_t selectedView = NULL_TAG;
            ShowWindow(state->window, SW_HIDE);
            const int selectCode = UF_UI_select_with_single_dialog(
                const_cast<char*>("选择投影目标平面"), const_cast<char*>("投影"),
                UF_UI_SEL_SCOPE_WORK_PART, nullptr, nullptr, &response,
                &selected, cursor, &selectedView);
            ShowWindow(state->window, SW_SHOW);
            SetForegroundWindow(state->window);
            if (selectCode == 0 &&
                (response == UF_UI_OBJECT_SELECTED || response == UF_UI_OBJECT_SELECTED_BY_NAME))
            {
                int type = 0, normalDirection = 0;
                double planePoint[3]{}, direction[3]{}, box[6]{}, radius = 0.0, radiusData = 0.0;
                if (UF_MODL_ask_face_data(selected, &type, planePoint, direction, box,
                                          &radius, &radiusData, &normalDirection) == 0 && type == 22)
                {
                    const double normalLength = std::sqrt(direction[0] * direction[0] +
                                                          direction[1] * direction[1] +
                                                          direction[2] * direction[2]);
                    if (normalLength > 1.0e-9)
                    {
                        for (double& value : direction) value /= normalLength;
                        const double distance = (pickedPoint[0] - planePoint[0]) * direction[0] +
                                                (pickedPoint[1] - planePoint[1]) * direction[1] +
                                                (pickedPoint[2] - planePoint[2]) * direction[2];
                        for (int axis = 0; axis < 3; ++axis)
                            state->placementOrigin[axis] = pickedPoint[axis] - distance * direction[axis];
                        double unusedOrigin[3]{};
                        if (AskWcs(unusedOrigin, state->placementMatrix, error))
                            state->hasPlacement = true;
                    }
                }
                else
                    error = L"点投影面只支持平面。";
            }
            else if (selectCode != 0)
                error = L"选择投影平面失败：" + UfError(selectCode);
        }
        else if (pointCode != 0)
            error = L"指定投影点失败：" + UfError(pointCode);
    }
    else if (state->placementMode == 1)
    {
        ShowWindow(state->window, SW_HIDE);
        tag_t view = NULL_TAG;
        int response = 0;
        const int code = UF_UI_specify_screen_position(
            const_cast<char*>("指定标准件放置点"), nullptr, nullptr,
            state->placementOrigin, &view, &response);
        ShowWindow(state->window, SW_SHOW);
        SetForegroundWindow(state->window);
        if (code == 0 && response == UF_UI_PICK_RESPONSE)
        {
            double unusedOrigin[3]{};
            if (AskWcs(unusedOrigin, state->placementMatrix, error))
                state->hasPlacement = true;
        }
        else if (code != 0)
            error = L"指定放置点失败：" + UfError(code);
    }
    else
    {
        PickObjectCenter(state, state->placementMode == 2
            ? L"选择作为放置基准的面" : L"选择作为放置基准的圆或圆弧", error);
    }
    UpdatePlacementStatus(state);
    if (!error.empty()) MessageBoxW(state->window, error.c_str(), kTitle, MB_OK | MB_ICONERROR);
}

void QuickOrient(AppState* state)
{
    std::wstring error;
    if (!AskWcs(state->placementOrigin, state->placementMatrix, error))
    {
        MessageBoxW(state->window, error.c_str(), kTitle, MB_OK | MB_ICONERROR);
        return;
    }
    // Do not invoke UF_UI_specify_csys here. It starts an NX modal command from
    // inside this Win32 dialog's message loop and makes the application appear
    // frozen until the nested CSYS dialog is explicitly completed or cancelled.
    state->hasPlacement = true;
    UpdatePlacementStatus(state);
    SetStatus(state, L"已采用当前 WCS 的原点和方向。需要调整时请先修改 NX 工作坐标系。");
}

bool AskPlacement(AppState* state, double origin[3], double matrix[9], std::wstring& error)
{
    if (!state->hasPlacement) return AskWcs(origin, matrix, error);
    std::copy(std::begin(state->placementOrigin), std::end(state->placementOrigin), origin);
    std::copy(std::begin(state->placementMatrix), std::end(state->placementMatrix), matrix);
    return true;
}

int RequestedLayer(AppState* state)
{
    const std::wstring text = Trim(GetText(state->window, ID_LAYER));
    wchar_t* end = nullptr;
    const long value = wcstol(text.c_str(), &end, 10);
    return end != text.c_str() && *end == L'\0' && value >= 1 && value <= 256
        ? static_cast<int>(value) : 1;
}

void SelectTrimTargets(AppState* state)
{
    ShowWindow(state->window, SW_HIDE);
    int response = 0;
    int count = 0;
    tag_t* objects = nullptr;
    const int code = UF_UI_select_with_class_dialog(
        const_cast<char*>("选择需要修剪的实体"), const_cast<char*>("自动修剪"),
        UF_UI_SEL_SCOPE_WORK_PART, nullptr, nullptr, &response, &count, &objects);
    ShowWindow(state->window, SW_SHOW);
    SetForegroundWindow(state->window);
    if (code != 0)
    {
        const std::wstring error = L"选择修剪实体失败：" + UfError(code);
        MessageBoxW(state->window, error.c_str(), kTitle, MB_OK | MB_ICONERROR);
        if (objects != nullptr) UF_free(objects);
        return;
    }
    if (response == UF_UI_OK)
    {
        state->trimTargets.clear();
        for (int index = 0; index < count; ++index)
        {
            int type = 0;
            int subtype = 0;
            if (UF_OBJ_ask_type_and_subtype(objects[index], &type, &subtype) == 0 &&
                type == UF_solid_type && subtype == UF_solid_body_subtype)
                state->trimTargets.push_back(objects[index]);
            UF_DISP_set_highlight(objects[index], 0);
        }
    }
    if (objects != nullptr) UF_free(objects);
    SetText(state->window, ID_TRIM_STATUS,
            L"选择需要修剪的实体 (" + std::to_wstring(state->trimTargets.size()) + L")");
}

std::vector<tag_t> ImportedBodies(tag_t group)
{
    std::vector<tag_t> bodies;
    tag_t* members = nullptr;
    int count = 0;
    if (group != NULL_TAG && UF_GROUP_ask_group_data(group, &members, &count) == 0)
    {
        for (int index = 0; index < count; ++index)
        {
            int type = 0;
            int subtype = 0;
            if (UF_OBJ_ask_type_and_subtype(members[index], &type, &subtype) == 0 &&
                type == UF_solid_type && subtype == UF_solid_body_subtype)
                bodies.push_back(members[index]);
        }
    }
    if (members != nullptr) UF_free(members);
    return bodies;
}

bool ApplyAutomaticTrim(AppState* state, const std::vector<tag_t>& toolBodies,
                        bool hideTools, std::wstring& error)
{
    for (tag_t target : state->trimTargets)
    {
        for (tag_t tool : toolBodies)
        {
            tag_t booleanFeature = NULL_TAG;
            const int code = UF_MODL_subtract_bodies_with_retained_options(
                target, tool, false, true, &booleanFeature);
            if (code != 0)
            {
                error = L"自动修剪失败：" + UfError(code);
                return false;
            }
        }
    }
    if (hideTools)
        for (tag_t tool : toolBodies) UF_OBJ_set_blank_status(tool, UF_OBJ_BLANKED);
    return true;
}

fs::path ResolvedModel(AppState* state, const LibraryItem& item, std::wstring& error)
{
    std::error_code ec;
    const fs::path root = fs::weakly_canonical(LibraryRoot(state), ec);
    if (ec) { error = L"标准件库路径无效。"; return {}; }
    const fs::path model = fs::weakly_canonical(root / item.relativePath, ec);
    if (ec || !fs::exists(model) || !IsPartFile(model))
    {
        error = L"标准件模型不存在：" + item.relativePath;
        return {};
    }
    const std::wstring rootText = Lower(root.wstring() + L"\\");
    if (Lower(model.wstring()).rfind(rootText, 0) != 0)
    {
        error = L"索引中的模型路径超出标准件库，已拒绝调用。";
        return {};
    }
    return model;
}

std::vector<std::pair<std::wstring, std::wstring>> LoadSpecificationParameters(
    const fs::path& model, const std::wstring& specification)
{
    std::vector<std::pair<std::wstring, std::wstring>> result;
    const fs::path table = model.parent_path() / L"parameters.tsv";
    std::wstring text;
    if (!ReadUtf16File(table, text)) return result;
    std::vector<std::wstring> headers;
    std::wistringstream lines(text);
    std::wstring line;
    while (std::getline(lines, line))
    {
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        if (line.empty() || line[0] == L'#') continue;
        const auto fields = SplitTabs(line);
        if (headers.empty())
        {
            headers = fields;
            continue;
        }
        if (fields.empty() || fields[0] != specification) continue;
        for (std::size_t index = 1; index < fields.size() && index < headers.size(); ++index)
            if (!headers[index].empty()) result.emplace_back(headers[index], fields[index]);
        break;
    }
    return result;
}

void UpdatePreview(AppState* state)
{
    if (state->previewBitmap != nullptr)
    {
        DeleteObject(state->previewBitmap);
        state->previewBitmap = nullptr;
    }
    for (int id = ID_PARAM_LABEL1; id <= ID_PARAM_LABEL4; ++id) SetText(state->window, id, L"");
    for (int id = ID_PARAM_VALUE1; id <= ID_PARAM_VALUE4; ++id) SetText(state->window, id, L"");
    const std::size_t index = SelectedIndex(state);
    if (index != SIZE_MAX)
    {
        SetText(state->window, ID_DETAIL_NAME, state->items[index].name);
        SetText(state->window, ID_DETAIL_CATEGORY, state->items[index].category);
        SetText(state->window, ID_DETAIL_FILE, state->items[index].relativePath);
        std::wstring error;
        const fs::path model = ResolvedModel(state, state->items[index], error);
        if (!model.empty())
        {
            const auto parameters = LoadSpecificationParameters(
                model, state->items[index].specification);
            const int labelIds[] = {ID_PARAM_LABEL1, ID_PARAM_LABEL2,
                                    ID_PARAM_LABEL3, ID_PARAM_LABEL4};
            const int valueIds[] = {ID_PARAM_VALUE1, ID_PARAM_VALUE2,
                                    ID_PARAM_VALUE3, ID_PARAM_VALUE4};
            for (int row = 0; row < 4; ++row)
            {
                if (static_cast<std::size_t>(row) < parameters.size())
                {
                    SetText(state->window, labelIds[row], parameters[row].first);
                    SetText(state->window, valueIds[row], parameters[row].second);
                }
                else
                {
                    static const wchar_t* fallbackLabels[] = {L"名称", L"分类", L"模型", L""};
                    const std::wstring fallbackValues[] = {
                        state->items[index].name, state->items[index].category,
                        state->items[index].relativePath, L""};
                    SetText(state->window, labelIds[row], fallbackLabels[row]);
                    SetText(state->window, valueIds[row], fallbackValues[row]);
                }
            }
            const fs::path preview = FindSidecarPreview(model);
            // Never ask the Windows shell to extract a thumbnail from an NX .prt file.
            // Siemens' shell thumbnail provider can re-enter the active NX process while
            // this dialog is running on NX's UI thread, leaving both sides waiting.
            if (!preview.empty())
            {
                IShellItemImageFactory* factory = nullptr;
                if (SUCCEEDED(SHCreateItemFromParsingName(preview.c_str(), nullptr,
                                                           IID_PPV_ARGS(&factory))))
                {
                    SIZE size{400, 240};
                    factory->GetImage(size,
                        static_cast<SIIGBF>(SIIGBF_BIGGERSIZEOK | SIIGBF_RESIZETOFIT),
                        &state->previewBitmap);
                    factory->Release();
                }
            }
        }
    }
    else
    {
        SetText(state->window, ID_DETAIL_NAME, L"未选择");
        SetText(state->window, ID_DETAIL_CATEGORY, L"-");
        SetText(state->window, ID_DETAIL_FILE, L"-");
        for (int id = ID_PARAM_LABEL1; id <= ID_PARAM_LABEL4; ++id) SetText(state->window, id, L"");
        for (int id = ID_PARAM_VALUE1; id <= ID_PARAM_VALUE4; ++id) SetText(state->window, id, L"");
    }
    if (state->preview != nullptr) InvalidateRect(state->preview, nullptr, TRUE);
}

bool InsertAssembly(AppState* state, const LibraryItem& item, std::wstring& error)
{
    const tag_t workPart = UF_ASSEM_ask_work_part();
    if (workPart == NULL_TAG) { error = L"请先打开一个工作部件。"; return false; }
    const fs::path model = ResolvedModel(state, item, error);
    if (model.empty()) return false;
    if (Lower(model.wstring()) == Lower(CurrentPartPath()))
    {
        error = L"不能把当前工作部件装配到自身。";
        return false;
    }
    const bool autoTrim = IsDlgButtonChecked(state->window, ID_AUTO_TRIM) == BST_CHECKED;
    if (autoTrim && state->trimTargets.empty())
    {
        error = L"已启用自动修剪，请先选择需要修剪的目标实体。";
        return false;
    }
    double origin[3]{};
    double matrix[9]{};
    if (!AskPlacement(state, origin, matrix, error)) return false;
    UF_PART_load_status_t loadStatus{};
    tag_t instance = NULL_TAG;
    const std::string path = ToAnsi(model.wstring());
    const std::string instanceName = ToAnsi(SanitizeFileName(item.name).substr(0, 30));
    const int code = UF_ASSEM_add_part_to_assembly2(
        workPart, path.c_str(), "MODEL", instanceName.c_str(), origin, matrix,
        -1, &instance, &loadStatus);
    UF_PART_free_load_status(&loadStatus);
    if (code != 0 || instance == NULL_TAG)
    {
        error = L"创建装配组件失败：" + UfError(code);
        return false;
    }
    UF_OBJ_set_layer(instance, RequestedLayer(state));
    if (autoTrim)
    {
        double destinationCsys[6] = {
            matrix[0], matrix[1], matrix[2], matrix[3], matrix[4], matrix[5]};
        UF_import_part_modes_t modes{};
        modes.layer_mode = IP_ORIG;
        modes.group_mode = IP_GROUP;
        modes.view_mode = IP_NO_VIEW;
        modes.cam_mode = false;
        modes.use_search_dirs = false;
        tag_t group = NULL_TAG;
        const int importCode = UF_PART_import(path.c_str(), &modes, destinationCsys,
                                               origin, 1.0, &group);
        if (importCode != 0)
        {
            error = L"组件已装配，但导入修剪工具失败：" + UfError(importCode);
            return false;
        }
        if (!ApplyAutomaticTrim(state, ImportedBodies(group), true, error))
        {
            error = L"组件已装配，但" + error;
            return false;
        }
    }
    UF_DISP_regenerate_display();
    return true;
}

bool InsertBodies(AppState* state, const LibraryItem& item, std::wstring& error)
{
    if (UF_ASSEM_ask_work_part() == NULL_TAG)
    {
        error = L"请先打开一个工作部件。";
        return false;
    }
    const bool autoTrim = IsDlgButtonChecked(state->window, ID_AUTO_TRIM) == BST_CHECKED;
    if (autoTrim && state->trimTargets.empty())
    {
        error = L"已启用自动修剪，请先选择需要修剪的目标实体。";
        return false;
    }
    const fs::path model = ResolvedModel(state, item, error);
    if (model.empty()) return false;
    if (Lower(model.wstring()) == Lower(CurrentPartPath()))
    {
        error = L"不能把当前工作部件合并到自身。";
        return false;
    }
    double origin[3]{};
    double matrix[9]{};
    if (!AskPlacement(state, origin, matrix, error)) return false;
    double destinationCsys[6] = {
        matrix[0], matrix[1], matrix[2], matrix[3], matrix[4], matrix[5]};
    UF_import_part_modes_t modes{};
    modes.layer_mode = IP_ORIG;
    modes.group_mode = IP_GROUP;
    modes.view_mode = IP_NO_VIEW;
    modes.cam_mode = false;
    modes.use_search_dirs = false;
    tag_t group = NULL_TAG;
    const std::string path = ToAnsi(model.wstring());
    const int code = UF_PART_import(path.c_str(), &modes, destinationCsys, origin, 1.0, &group);
    if (code != 0)
    {
        error = L"合并标准件模型失败：" + UfError(code);
        return false;
    }
    const std::vector<tag_t> importedBodies = ImportedBodies(group);
    const int layer = RequestedLayer(state);
    for (tag_t body : importedBodies) UF_OBJ_set_layer(body, layer);
    if (autoTrim && !ApplyAutomaticTrim(state, importedBodies, false, error))
    {
        error = L"标准件已导入，但" + error;
        return false;
    }
    UF_DISP_regenerate_display();
    return true;
}

HWND AddControl(AppState* state, DWORD exStyle, const wchar_t* cls, const wchar_t* text,
                DWORD style, int x, int y, int width, int height, int id)
{
    HWND control = CreateWindowExW(exStyle, cls, text, style | WS_CHILD | WS_VISIBLE,
                                   x, y, width, height, state->window,
                                   reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                   GetModuleHandleW(nullptr), nullptr);
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(state->font), TRUE);
    return control;
}

LRESULT CALLBACK ManagerWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    AppState* state = reinterpret_cast<AppState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE)
    {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        state = static_cast<AppState*>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    }
    constexpr int rootId = 2101;
    constexpr int browseId = 2102;
    constexpr int openStaticId = 2103;
    constexpr int openParamId = 2104;
    constexpr int categoryId = 2105;
    constexpr int familyId = 2106;
    constexpr int specificationId = 2107;
    constexpr int addStaticId = 2108;
    constexpr int addParamId = 2109;
    constexpr int screenshotId = 2110;
    if (message == WM_CREATE && state != nullptr)
    {
        const HFONT font = state->font;
        const auto add = [&](DWORD ex, const wchar_t* cls, const wchar_t* text, DWORD style,
                             int x, int y, int width, int height, int id)
        {
            HWND control = CreateWindowExW(ex, cls, text, style | WS_CHILD | WS_VISIBLE,
                x, y, width, height, window,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr), nullptr);
            SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        };
        add(0, L"STATIC", L"标准件库根目录", SS_LEFT, 16, 22, 92, 22, 0);
        add(WS_EX_CLIENTEDGE, L"EDIT", LibraryRoot(state).c_str(), ES_AUTOHSCROLL,
            112, 18, 430, 26, rootId);
        add(0, L"BUTTON", L"浏览", BS_PUSHBUTTON, 550, 17, 72, 28, browseId);
        add(0, L"STATIC", L"无参目录：根目录\\Lib", SS_LEFT, 16, 64, 260, 22, 0);
        add(0, L"STATIC", L"有参目录：根目录\\LibParam", SS_LEFT, 16, 92, 280, 22, 0);
        add(0, L"STATIC", L"分类", SS_LEFT, 16, 126, 38, 22, 0);
        add(WS_EX_CLIENTEDGE, L"EDIT", L"用户自定义", ES_AUTOHSCROLL, 56, 122, 132, 25, categoryId);
        add(0, L"STATIC", L"零件族", SS_LEFT, 198, 126, 50, 22, 0);
        add(WS_EX_CLIENTEDGE, L"EDIT", L"", ES_AUTOHSCROLL, 250, 122, 150, 25, familyId);
        add(0, L"STATIC", L"规格", SS_LEFT, 410, 126, 38, 22, 0);
        add(WS_EX_CLIENTEDGE, L"EDIT", L"默认", ES_AUTOHSCROLL, 450, 122, 172, 25, specificationId);
        add(0, L"BUTTON", L"当前部件加入无参库", BS_PUSHBUTTON, 16, 160, 166, 38, addStaticId);
        add(0, L"BUTTON", L"当前部件加入有参库", BS_PUSHBUTTON, 192, 160, 166, 38, addParamId);
        add(0, L"BUTTON", L"截图预览", BS_PUSHBUTTON, 368, 160, 112, 38, screenshotId);
        add(0, L"BUTTON", L"管理本地无参", BS_PUSHBUTTON, 16, 210, 132, 42, openStaticId);
        add(0, L"BUTTON", L"管理本地有参", BS_PUSHBUTTON, 158, 210, 132, 42, openParamId);
        add(0, L"STATIC", L"PRT 与同名 PNG/JPG/BMP 放在同一目录即可显示预览。",
            SS_LEFT, 310, 215, 310, 40, 0);
        add(0, L"BUTTON", L"确定", BS_DEFPUSHBUTTON, 466, 278, 74, 30, IDOK);
        add(0, L"BUTTON", L"取消", BS_PUSHBUTTON, 548, 278, 74, 30, IDCANCEL);
        return 0;
    }
    if (message == WM_COMMAND && state != nullptr)
    {
        switch (LOWORD(wParam))
        {
        case browseId:
        {
            const std::wstring folder = BrowseFolder(window);
            if (!folder.empty()) SetText(window, rootId, folder);
            return 0;
        }
        case openStaticId:
        case openParamId:
        {
            const fs::path root = Trim(GetText(window, rootId));
            const fs::path folder = root /
                (LOWORD(wParam) == openStaticId ? L"Lib" : L"LibParam");
            std::error_code ec;
            fs::create_directories(folder, ec);
            if (!ec) ShellExecuteW(window, L"open", folder.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            return 0;
        }
        case addStaticId:
        case addParamId:
        {
            const std::wstring current = CurrentPartPath();
            if (current.empty())
            {
                MessageBoxW(window, L"当前工作部件尚未保存，请先保存 PRT。", kTitle,
                            MB_OK | MB_ICONWARNING);
                return 0;
            }
            std::wstring family = Trim(GetText(window, familyId));
            if (family.empty()) family = fs::path(current).stem().wstring();
            SetText(state->window, ID_ROOT, Trim(GetText(window, rootId)));
            SetText(state->window, ID_NAME, family);
            SetText(state->window, ID_EDIT_CATEGORY, Trim(GetText(window, categoryId)));
            SetText(state->window, ID_EDIT_SPEC, Trim(GetText(window, specificationId)));
            state->libraryFilter = LOWORD(wParam) == addParamId ? 2 : 1;
            std::wstring error;
            if (!AddToLibrary(state, current, error))
                MessageBoxW(window, error.c_str(), kTitle, MB_OK | MB_ICONERROR);
            else
                MessageBoxW(window, L"当前部件和同名预览图已加入标准件库。", kTitle,
                            MB_OK | MB_ICONINFORMATION);
            return 0;
        }
        case screenshotId:
        {
            const std::wstring current = CurrentPartPath();
            if (current.empty())
            {
                MessageBoxW(window, L"当前工作部件尚未保存，不能创建同名预览图。", kTitle,
                            MB_OK | MB_ICONWARNING);
                return 0;
            }
            fs::path preview = current;
            preview.replace_extension(L".png");
            if (fs::exists(preview) && MessageBoxW(window,
                (L"同名预览图已经存在，确定覆盖？\r\n" + preview.wstring()).c_str(),
                kTitle, MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES)
                return 0;
            std::string output = ToAnsi(preview.wstring());
            ShowWindow(window, SW_HIDE);
            ShowWindow(state->window, SW_HIDE);
            const int code = UF_DISP_create_image(output.data(), UF_DISP_PNG, UF_DISP_WHITE);
            ShowWindow(state->window, SW_SHOW);
            ShowWindow(window, SW_SHOW);
            SetForegroundWindow(window);
            if (code == 0)
                MessageBoxW(window, (L"预览图已创建：\r\n" + preview.wstring()).c_str(),
                            kTitle, MB_OK | MB_ICONINFORMATION);
            else
            {
                const std::wstring error = L"创建预览图失败：" + UfError(code);
                MessageBoxW(window, error.c_str(), kTitle, MB_OK | MB_ICONERROR);
            }
            return 0;
        }
        case IDOK:
            SetText(state->window, ID_ROOT, Trim(GetText(window, rootId)));
            SaveRootPreference(state);
            LoadIndex(state);
            RefreshCategories(state);
            RefreshList(state);
            DestroyWindow(window);
            return 0;
        case IDCANCEL: DestroyWindow(window); return 0;
        default: break;
        }
    }
    if (message == WM_CLOSE) { DestroyWindow(window); return 0; }
    if (message == WM_NCDESTROY)
    {
        if (g_managerWindow == window) g_managerWindow = nullptr;
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

void ShowManagerConfig(AppState* state)
{
    if (g_managerWindow != nullptr && IsWindow(g_managerWindow))
    {
        ShowWindow(g_managerWindow, SW_RESTORE);
        SetForegroundWindow(g_managerWindow);
        return;
    }
    const wchar_t* className = L"ZhihuiStandardPartsManagerWindow";
    HMODULE module = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                          GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                      reinterpret_cast<LPCWSTR>(&ShowManagerConfig), &module);
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = ManagerWindowProc;
    windowClass.hInstance = module;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    windowClass.lpszClassName = className;
    RegisterClassExW(&windowClass);
    RECT parentRect{};
    GetWindowRect(state->window, &parentRect);
    HWND manager = CreateWindowExW(WS_EX_DLGMODALFRAME, className,
        L"智辉零件库管理配置", WS_CAPTION | WS_SYSMENU | WS_POPUP,
        parentRect.left + 10, parentRect.top + 100, 654, 355,
        state->window, nullptr, module, state);
    if (manager == nullptr) return;
    g_managerWindow = manager;
    ShowWindow(manager, SW_SHOW);
}

void BuildLegacyUi(AppState* state)
{
    state->font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    const std::wstring savedRoot = LoadRootPreference();
    AddControl(state, 0, L"BUTTON", L"全部", BS_AUTORADIOBUTTON | BS_PUSHLIKE | WS_GROUP,
               12, 10, 66, 27, ID_FILTER_ALL);
    AddControl(state, 0, L"BUTTON", L"本地无参", BS_AUTORADIOBUTTON | BS_PUSHLIKE,
               80, 10, 82, 27, ID_FILTER_STATIC);
    AddControl(state, 0, L"BUTTON", L"本地有参", BS_AUTORADIOBUTTON | BS_PUSHLIKE,
               164, 10, 82, 27, ID_FILTER_PARAM);
    CheckRadioButton(state->window, ID_FILTER_ALL, ID_FILTER_PARAM, ID_FILTER_ALL);
    AddControl(state, 0, L"STATIC", L"搜索", SS_LEFT, 264, 15, 38, 20, 0);
    AddControl(state, WS_EX_CLIENTEDGE, L"EDIT", L"", ES_AUTOHSCROLL,
               304, 11, 270, 25, ID_SEARCH);
    AddControl(state, WS_EX_CLIENTEDGE, L"EDIT", savedRoot.c_str(), ES_AUTOHSCROLL,
               584, 11, 208, 25, ID_ROOT);
    AddControl(state, 0, L"BUTTON", L"库设置", BS_PUSHBUTTON, 798, 10, 62, 27, ID_BROWSE_ROOT);
    AddControl(state, 0, L"BUTTON", L"刷新", BS_PUSHBUTTON, 864, 10, 44, 27, ID_REFRESH);

    AddControl(state, 0, L"BUTTON", L"分类", BS_GROUPBOX, 12, 43, 180, 448, 0);
    state->category = AddControl(state, WS_EX_CLIENTEDGE, WC_TREEVIEWW, L"",
                                 TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT |
                                 TVS_SHOWSELALWAYS, 22, 64, 160, 416, ID_CATEGORY);

    AddControl(state, 0, L"BUTTON", L"标准件", BS_GROUPBOX, 198, 43, 260, 448, 0);
    state->list = AddControl(state, WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
                             LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOCOLUMNHEADER,
                             208, 64, 240, 416, ID_LIST);
    ListView_SetExtendedListViewStyle(state->list, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT);
    LVCOLUMNW familyColumn{};
    familyColumn.mask = LVCF_WIDTH;
    familyColumn.cx = 220;
    ListView_InsertColumn(state->list, 0, &familyColumn);

    AddControl(state, 0, L"BUTTON", L"预览与规格", BS_GROUPBOX, 464, 43, 444, 448, 0);
    state->preview = AddControl(state, WS_EX_CLIENTEDGE, L"STATIC", L"", SS_OWNERDRAW,
                                478, 62, 416, 252, ID_PREVIEW);
    AddControl(state, 0, L"STATIC", L"规格", SS_LEFT, 480, 327, 58, 22, 0);
    AddControl(state, 0, WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_VSCROLL,
               546, 323, 344, 250, ID_SPEC);
    AddControl(state, 0, L"STATIC", L"名称", SS_LEFT, 480, 357, 58, 22, 0);
    AddControl(state, 0, L"STATIC", L"未选择", SS_LEFT | SS_PATHELLIPSIS,
               546, 357, 344, 22, ID_DETAIL_NAME);
    AddControl(state, 0, L"STATIC", L"分类", SS_LEFT, 480, 384, 58, 22, 0);
    AddControl(state, 0, L"STATIC", L"-", SS_LEFT | SS_PATHELLIPSIS,
               546, 384, 344, 22, ID_DETAIL_CATEGORY);
    AddControl(state, 0, L"STATIC", L"模型", SS_LEFT, 480, 411, 58, 22, 0);
    AddControl(state, 0, L"STATIC", L"-", SS_LEFT | SS_PATHELLIPSIS,
               546, 411, 344, 22, ID_DETAIL_FILE);
    AddControl(state, 0, L"BUTTON", L"装配调用", BS_AUTORADIOBUTTON | WS_GROUP,
               480, 439, 96, 24, ID_MODE_ASSEMBLY);
    AddControl(state, 0, L"BUTTON", L"多实体调用", BS_AUTORADIOBUTTON,
               584, 439, 106, 24, ID_MODE_BODY);
    CheckRadioButton(state->window, ID_MODE_ASSEMBLY, ID_MODE_BODY, ID_MODE_ASSEMBLY);
    AddControl(state, 0, L"BUTTON", L"调用选中标准件", BS_DEFPUSHBUTTON,
               704, 435, 186, 32, ID_INSERT);
    AddControl(state, 0, L"BUTTON", L"自动修剪", BS_AUTOCHECKBOX,
               480, 466, 96, 22, ID_AUTO_TRIM);
    AddControl(state, 0, L"STATIC", L"放置图层", SS_LEFT, 584, 468, 62, 20, 0);
    AddControl(state, WS_EX_CLIENTEDGE, L"EDIT", L"1", ES_NUMBER,
               650, 464, 50, 24, ID_LAYER);

    AddControl(state, 0, L"BUTTON", L"放置方式", BS_GROUPBOX, 12, 500, 896, 132, 0);
    AddControl(state, 0, L"BUTTON", L"WCS 原点", BS_AUTORADIOBUTTON | BS_PUSHLIKE | WS_GROUP,
               24, 524, 104, 30, ID_PLACE_WCS);
    AddControl(state, 0, L"BUTTON", L"任意点", BS_AUTORADIOBUTTON | BS_PUSHLIKE,
               134, 524, 104, 30, ID_PLACE_POINT);
    AddControl(state, 0, L"BUTTON", L"面中心", BS_AUTORADIOBUTTON | BS_PUSHLIKE,
               244, 524, 104, 30, ID_PLACE_FACE);
    AddControl(state, 0, L"BUTTON", L"圆/圆弧中心", BS_AUTORADIOBUTTON | BS_PUSHLIKE,
               354, 524, 126, 30, ID_PLACE_CIRCLE);
    CheckRadioButton(state->window, ID_PLACE_WCS, ID_PLACE_CIRCLE, ID_PLACE_WCS);
    AddControl(state, 0, L"BUTTON", L"指定位置", BS_PUSHBUTTON, 496, 524, 105, 30, ID_PICK_PLACE);
    AddControl(state, 0, L"BUTTON", L"采用当前WCS", BS_PUSHBUTTON, 608, 524, 105, 30, ID_QUICK_ORIENT);
    AddControl(state, 0, L"STATIC", L"○ 当前 WCS", SS_LEFT,
               24, 568, 760, 24, ID_PLACE_STATUS);
    AddControl(state, 0, L"STATIC", L"提示：快速定位可使用 NX 动态坐标系确定原点和方向",
               SS_LEFT, 24, 596, 620, 22, 0);
    AddControl(state, 0, L"STATIC", L"选择需要修剪的实体 (0)", SS_RIGHT,
               646, 570, 190, 22, ID_TRIM_STATUS);
    AddControl(state, 0, L"BUTTON", L"选择...", BS_PUSHBUTTON,
               842, 566, 54, 28, ID_SELECT_TRIM);

    AddControl(state, 0, L"BUTTON", L"用户标准件管理", BS_GROUPBOX, 12, 640, 896, 112, 0);
    AddControl(state, 0, L"STATIC", L"名称", SS_LEFT, 24, 666, 38, 22, 0);
    AddControl(state, WS_EX_CLIENTEDGE, L"EDIT", L"", ES_AUTOHSCROLL, 64, 663, 170, 25, ID_NAME);
    AddControl(state, 0, L"STATIC", L"分类", SS_LEFT, 244, 666, 38, 22, 0);
    AddControl(state, WS_EX_CLIENTEDGE, L"EDIT", L"用户自定义", ES_AUTOHSCROLL, 284, 663, 155, 25, ID_EDIT_CATEGORY);
    AddControl(state, 0, L"STATIC", L"规格", SS_LEFT, 449, 666, 38, 22, 0);
    AddControl(state, WS_EX_CLIENTEDGE, L"EDIT", L"默认", ES_AUTOHSCROLL, 489, 663, 138, 25, ID_EDIT_SPEC);
    AddControl(state, 0, L"BUTTON", L"入库当前部件", BS_PUSHBUTTON, 640, 661, 116, 29, ID_ADD_CURRENT);
    AddControl(state, 0, L"STATIC", L"源文件", SS_LEFT, 24, 706, 52, 22, 0);
    AddControl(state, WS_EX_CLIENTEDGE, L"EDIT", L"", ES_AUTOHSCROLL | ES_READONLY,
               78, 703, 476, 25, ID_SOURCE);
    AddControl(state, 0, L"BUTTON", L"浏览...", BS_PUSHBUTTON, 562, 701, 70, 29, ID_BROWSE_SOURCE);
    AddControl(state, 0, L"BUTTON", L"添加文件", BS_PUSHBUTTON, 640, 701, 90, 29, ID_ADD_FILE);
    AddControl(state, 0, L"BUTTON", L"删除选中", BS_PUSHBUTTON, 738, 701, 90, 29, ID_DELETE);
    AddControl(state, 0, L"BUTTON", L"修改配置", BS_PUSHBUTTON, 14, 758, 88, 30, ID_MANAGE_CONFIG);
    AddControl(state, 0, L"STATIC", L"", SS_LEFT, 112, 762, 610, 26, ID_STATUS);
    AddControl(state, 0, L"BUTTON", L"关闭", BS_PUSHBUTTON, 824, 758, 84, 30, ID_CLOSE);
}

void BuildUi(AppState* state)
{
    state->font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    const std::wstring savedRoot = LoadRootPreference();
    HWND hiddenRoot = AddControl(state, WS_EX_CLIENTEDGE, L"EDIT", savedRoot.c_str(),
                                 ES_AUTOHSCROLL, -1000, -1000, 1, 1, ID_ROOT);
    ShowWindow(hiddenRoot, SW_HIDE);
    const int hiddenIds[] = {ID_NAME, ID_EDIT_CATEGORY, ID_EDIT_SPEC, ID_SOURCE};
    for (int id : hiddenIds)
    {
        HWND hidden = AddControl(state, 0, L"EDIT", L"", ES_AUTOHSCROLL,
                                 -1000, -1000, 1, 1, id);
        ShowWindow(hidden, SW_HIDE);
    }

    AddControl(state, 0, L"BUTTON", L"全部", BS_AUTORADIOBUTTON | BS_PUSHLIKE | WS_GROUP,
               10, 12, 72, 30, ID_FILTER_ALL);
    AddControl(state, 0, L"BUTTON", L"本地无参", BS_AUTORADIOBUTTON | BS_PUSHLIKE,
               10, 46, 72, 30, ID_FILTER_STATIC);
    AddControl(state, 0, L"BUTTON", L"本地有参", BS_AUTORADIOBUTTON | BS_PUSHLIKE,
               10, 80, 72, 30, ID_FILTER_PARAM);
    CheckRadioButton(state->window, ID_FILTER_ALL, ID_FILTER_PARAM, ID_FILTER_ALL);
    AddControl(state, WS_EX_CLIENTEDGE, L"EDIT", L"", ES_AUTOHSCROLL,
               88, 13, 116, 27, ID_SEARCH);
    AddControl(state, 0, L"STATIC", L"搜索", SS_LEFT, 209, 18, 38, 20, 0);

    state->category = AddControl(state, WS_EX_CLIENTEDGE, WC_TREEVIEWW, L"",
        TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS,
        88, 44, 152, 474, ID_CATEGORY);
    state->list = AddControl(state, WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
        LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOCOLUMNHEADER,
        244, 44, 126, 474, ID_LIST);
    ListView_SetExtendedListViewStyle(state->list, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT);
    LVCOLUMNW familyColumn{};
    familyColumn.mask = LVCF_WIDTH;
    familyColumn.cx = 108;
    ListView_InsertColumn(state->list, 0, &familyColumn);

    state->preview = AddControl(state, WS_EX_CLIENTEDGE, L"STATIC", L"", SS_OWNERDRAW,
                                375, 44, 229, 225, ID_PREVIEW);
    AddControl(state, 0, L"STATIC", L"规格", SS_LEFT, 379, 282, 54, 22, 0);
    AddControl(state, 0, WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_VSCROLL,
               441, 278, 158, 200, ID_SPEC);
    AddControl(state, 0, L"STATIC", L"名称", SS_LEFT, 379, 310, 58, 22, ID_PARAM_LABEL1);
    AddControl(state, 0, L"STATIC", L"未选择", SS_LEFT | SS_PATHELLIPSIS,
               441, 310, 158, 22, ID_PARAM_VALUE1);
    AddControl(state, 0, L"STATIC", L"分类", SS_LEFT, 379, 336, 58, 22, ID_PARAM_LABEL2);
    AddControl(state, 0, L"STATIC", L"-", SS_LEFT | SS_PATHELLIPSIS,
               441, 336, 158, 22, ID_PARAM_VALUE2);
    AddControl(state, 0, L"STATIC", L"模型", SS_LEFT, 379, 362, 58, 22, ID_PARAM_LABEL3);
    AddControl(state, 0, L"STATIC", L"-", SS_LEFT | SS_PATHELLIPSIS,
               441, 362, 158, 22, ID_PARAM_VALUE3);
    AddControl(state, 0, L"STATIC", L"", SS_LEFT, 379, 388, 58, 22, ID_PARAM_LABEL4);
    AddControl(state, 0, L"STATIC", L"", SS_LEFT | SS_PATHELLIPSIS,
               441, 388, 158, 22, ID_PARAM_VALUE4);
    AddControl(state, 0, L"BUTTON", L"自动修剪", BS_AUTOCHECKBOX,
               378, 414, 96, 24, ID_AUTO_TRIM);
    AddControl(state, 0, L"STATIC", L"指定图层", SS_LEFT, 378, 446, 64, 22, 0);
    AddControl(state, WS_EX_CLIENTEDGE, L"EDIT", L"1", ES_NUMBER,
               446, 442, 72, 25, ID_LAYER);
    AddControl(state, 0, L"BUTTON", L"装配", BS_AUTORADIOBUTTON | WS_GROUP,
               378, 474, 62, 22, ID_MODE_ASSEMBLY);
    AddControl(state, 0, L"BUTTON", L"多实体", BS_AUTORADIOBUTTON,
               444, 474, 72, 22, ID_MODE_BODY);
    CheckRadioButton(state->window, ID_MODE_ASSEMBLY, ID_MODE_BODY, ID_MODE_ASSEMBLY);
    AddControl(state, 0, L"BUTTON", L"修改配置", BS_PUSHBUTTON,
               10, 488, 72, 30, ID_MANAGE_CONFIG);

    if (state->embedded) return;

    AddControl(state, 0, L"BUTTON", L"放置方式", BS_GROUPBOX, 10, 528, 594, 168, 0);
    AddControl(state, 0, L"BUTTON", L"任意点", BS_AUTORADIOBUTTON | BS_PUSHLIKE | WS_GROUP,
               22, 552, 88, 31, ID_PLACE_POINT);
    AddControl(state, 0, L"BUTTON", L"面中心", BS_AUTORADIOBUTTON | BS_PUSHLIKE,
               114, 552, 88, 31, ID_PLACE_FACE);
    AddControl(state, 0, L"BUTTON", L"点投影面", BS_AUTORADIOBUTTON | BS_PUSHLIKE,
               206, 552, 92, 31, ID_PLACE_WCS);
    AddControl(state, 0, L"BUTTON", L"圆弧", BS_AUTORADIOBUTTON | BS_PUSHLIKE,
               302, 552, 72, 31, ID_PLACE_CIRCLE);
    CheckRadioButton(state->window, ID_PLACE_WCS, ID_PLACE_CIRCLE, ID_PLACE_WCS);
    AddControl(state, 0, L"STATIC", L"指定放置 (0)", SS_LEFT,
               22, 598, 104, 22, 0);
    AddControl(state, 0, L"BUTTON", L"...", BS_PUSHBUTTON,
               130, 592, 36, 29, ID_PICK_PLACE);
    AddControl(state, 0, L"STATIC", L"○ 当前 WCS", SS_LEFT,
               174, 598, 220, 22, ID_PLACE_STATUS);
    AddControl(state, 0, L"STATIC", L"指定方位", SS_LEFT,
               22, 642, 72, 22, 0);
    AddControl(state, 0, L"BUTTON", L"采用当前WCS", BS_PUSHBUTTON,
               96, 635, 100, 30, ID_QUICK_ORIENT);
    AddControl(state, 0, L"STATIC", L"选择需要修剪的实体 (0)", SS_RIGHT,
               322, 642, 214, 22, ID_TRIM_STATUS);
    AddControl(state, 0, L"BUTTON", L"选择", BS_PUSHBUTTON,
               542, 635, 50, 30, ID_SELECT_TRIM);

    AddControl(state, 0, L"STATIC", L"", SS_LEFT, 12, 708, 360, 42, ID_STATUS);
    AddControl(state, 0, L"BUTTON", L"确定", BS_DEFPUSHBUTTON,
               382, 718, 66, 30, ID_INSERT);
    AddControl(state, 0, L"BUTTON", L"应用", BS_PUSHBUTTON,
               454, 718, 66, 30, ID_INSERT);
    AddControl(state, 0, L"BUTTON", L"取消", BS_PUSHBUTTON,
               526, 718, 66, 30, ID_CLOSE);
}

void DeleteSelected(AppState* state)
{
    const std::size_t index = SelectedIndex(state);
    if (index == SIZE_MAX) { SetStatus(state, L"请先选中要删除的标准件。"); return; }
    const LibraryItem item = state->items[index];
    if (MessageBoxW(state->window, (L"确定删除标准件“" + item.name + L"”及库内模型副本？").c_str(),
                    kTitle, MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) return;
    state->items.erase(state->items.begin() + static_cast<std::ptrdiff_t>(index));
    std::wstring error;
    if (!SaveIndex(state, error))
    {
        state->items.insert(state->items.begin() + static_cast<std::ptrdiff_t>(index), item);
        SetStatus(state, error);
        return;
    }
    std::error_code ec;
    const fs::path model = LibraryRoot(state) / item.relativePath;
    const fs::path preview = FindSidecarPreview(model);
    fs::remove(model, ec);
    if (!preview.empty()) fs::remove(preview, ec);
    RefreshCategories(state);
    RefreshList(state);
    SetStatus(state, L"已删除：" + item.name);
}

void InsertSelected(AppState* state)
{
    const std::size_t index = SelectedIndex(state);
    if (index == SIZE_MAX) { SetStatus(state, L"请先选中要调用的标准件。"); return; }
    std::wstring error;
    const bool assembly = IsDlgButtonChecked(state->window, ID_MODE_ASSEMBLY) == BST_CHECKED;
    const bool ok = assembly ? InsertAssembly(state, state->items[index], error)
                             : InsertBodies(state, state->items[index], error);
    if (ok)
        SetStatus(state, L"已按指定位置" + std::wstring(assembly ? L"装配" : L"合并") +
                         L"标准件：" + state->items[index].name);
    else
        MessageBoxW(state->window, error.c_str(), kTitle, MB_OK | MB_ICONERROR);
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    AppState* state = reinterpret_cast<AppState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE)
    {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        state = static_cast<AppState*>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        state->window = window;
    }
    switch (message)
    {
    case WM_CREATE:
        BuildUi(state);
        LoadIndex(state);
        RefreshCategories(state);
        RefreshList(state);
        return 0;
    case WM_COMMAND:
        if (state == nullptr) break;
        switch (LOWORD(wParam))
        {
        case ID_CLOSE: DestroyWindow(window); return 0;
        case ID_REFRESH:
            SaveRootPreference(state); LoadIndex(state); RefreshCategories(state); RefreshList(state); return 0;
        case ID_BROWSE_ROOT:
        {
            const std::wstring folder = BrowseFolder(window);
            if (!folder.empty()) { SetText(window, ID_ROOT, folder); SaveRootPreference(state); LoadIndex(state); RefreshCategories(state); RefreshList(state); }
            return 0;
        }
        case ID_BROWSE_SOURCE:
        {
            const std::wstring file = BrowsePartFile(window);
            if (!file.empty())
            {
                SetText(window, ID_SOURCE, file);
                if (Trim(GetText(window, ID_NAME)).empty()) SetText(window, ID_NAME, fs::path(file).stem().wstring());
            }
            return 0;
        }
        case ID_ADD_CURRENT:
        {
            const std::wstring file = CurrentPartPath();
            if (file.empty()) { MessageBoxW(window, L"当前工作部件还没有保存，请先保存为 .prt 文件。", kTitle, MB_OK | MB_ICONWARNING); return 0; }
            SetText(window, ID_SOURCE, file);
            if (Trim(GetText(window, ID_NAME)).empty()) SetText(window, ID_NAME, fs::path(file).stem().wstring());
            std::wstring error;
            if (!AddToLibrary(state, file, error)) MessageBoxW(window, error.c_str(), kTitle, MB_OK | MB_ICONERROR);
            return 0;
        }
        case ID_ADD_FILE:
        {
            std::wstring file = GetText(window, ID_SOURCE);
            if (file.empty()) file = BrowsePartFile(window);
            if (file.empty()) return 0;
            SetText(window, ID_SOURCE, file);
            if (Trim(GetText(window, ID_NAME)).empty()) SetText(window, ID_NAME, fs::path(file).stem().wstring());
            std::wstring error;
            if (!AddToLibrary(state, file, error)) MessageBoxW(window, error.c_str(), kTitle, MB_OK | MB_ICONERROR);
            return 0;
        }
        case ID_DELETE: DeleteSelected(state); return 0;
        case ID_INSERT: InsertSelected(state); return 0;
        case ID_FILTER_ALL:
        case ID_FILTER_STATIC:
        case ID_FILTER_PARAM:
            if (HIWORD(wParam) == BN_CLICKED)
            {
                state->libraryFilter = LOWORD(wParam) - ID_FILTER_ALL;
                RefreshList(state);
            }
            return 0;
        case ID_PLACE_WCS:
        case ID_PLACE_POINT:
        case ID_PLACE_FACE:
        case ID_PLACE_CIRCLE:
            if (HIWORD(wParam) == BN_CLICKED)
                SetPlacementMode(state, LOWORD(wParam) - ID_PLACE_WCS);
            return 0;
        case ID_PICK_PLACE: PickPlacement(state); return 0;
        case ID_QUICK_ORIENT: QuickOrient(state); return 0;
        case ID_SELECT_TRIM: SelectTrimTargets(state); return 0;
        case ID_MANAGE_CONFIG: ShowManagerConfig(state); return 0;
        case ID_SPEC:
            if (HIWORD(wParam) == CBN_SELCHANGE) UpdatePreview(state);
            return 0;
        case ID_SEARCH:
            if (HIWORD(wParam) == EN_CHANGE) RefreshList(state);
            return 0;
        default: break;
        }
        break;
    case WM_NOTIFY:
        if (state == nullptr) return 0;
        if (reinterpret_cast<NMHDR*>(lParam)->idFrom == ID_CATEGORY &&
            reinterpret_cast<NMHDR*>(lParam)->code == TVN_SELCHANGEDW)
        {
            if (state->refreshingCategories) return 0;
            const auto* notification = reinterpret_cast<NMTREEVIEWW*>(lParam);
            const std::size_t categoryIndex = static_cast<std::size_t>(notification->itemNew.lParam);
            state->selectedCategory = categoryIndex == 0 || categoryIndex > state->categories.size()
                ? L"" : state->categories[categoryIndex - 1];
            RefreshList(state);
        }
        else if (reinterpret_cast<NMHDR*>(lParam)->idFrom == ID_LIST &&
                 reinterpret_cast<NMHDR*>(lParam)->code == NM_DBLCLK)
        {
            InsertSelected(state);
        }
        else if (reinterpret_cast<NMHDR*>(lParam)->idFrom == ID_LIST &&
                 reinterpret_cast<NMHDR*>(lParam)->code == LVN_ITEMCHANGED)
        {
            if (state->refreshingList) return 0;
            const auto* notification = reinterpret_cast<NMLISTVIEW*>(lParam);
            if ((notification->uChanged & LVIF_STATE) != 0 &&
                ((notification->uNewState ^ notification->uOldState) & LVIS_SELECTED) != 0)
            {
                RefreshSpecifications(state);
                UpdatePreview(state);
            }
        }
        return 0;
    case WM_DRAWITEM:
        if (state != nullptr && wParam == ID_PREVIEW)
        {
            const auto* draw = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
            FillRect(draw->hDC, &draw->rcItem, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));
            if (state->previewBitmap != nullptr)
            {
                BITMAP bitmap{};
                GetObjectW(state->previewBitmap, sizeof(bitmap), &bitmap);
                const int areaWidth = draw->rcItem.right - draw->rcItem.left - 16;
                const int areaHeight = draw->rcItem.bottom - draw->rcItem.top - 16;
                const double scale = std::min(
                    static_cast<double>(areaWidth) / std::max(1L, bitmap.bmWidth),
                    static_cast<double>(areaHeight) / std::max(1L, bitmap.bmHeight));
                const int width = std::max(1, static_cast<int>(bitmap.bmWidth * scale));
                const int height = std::max(1, static_cast<int>(bitmap.bmHeight * scale));
                const int x = draw->rcItem.left + (draw->rcItem.right - draw->rcItem.left - width) / 2;
                const int y = draw->rcItem.top + (draw->rcItem.bottom - draw->rcItem.top - height) / 2;
                HDC memory = CreateCompatibleDC(draw->hDC);
                const HGDIOBJ old = SelectObject(memory, state->previewBitmap);
                SetStretchBltMode(draw->hDC, HALFTONE);
                StretchBlt(draw->hDC, x, y, width, height, memory, 0, 0,
                           bitmap.bmWidth, bitmap.bmHeight, SRCCOPY);
                SelectObject(memory, old);
                DeleteDC(memory);
            }
            else
            {
                RECT textArea = draw->rcItem;
                SetBkMode(draw->hDC, TRANSPARENT);
                SetTextColor(draw->hDC, RGB(105, 105, 105));
                DrawTextW(draw->hDC,
                          L"请选中标准件\r\n\r\n可在 PRT 旁放置同名 PNG/JPG/BMP 作为预览图",
                          -1, &textArea, DT_CENTER | DT_VCENTER | DT_WORDBREAK);
            }
            FrameRect(draw->hDC, &draw->rcItem, reinterpret_cast<HBRUSH>(COLOR_3DSHADOW + 1));
            return TRUE;
        }
        break;
    case WM_CLOSE:
        DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        if (state != nullptr)
        {
            if (g_managerWindow != nullptr && IsWindow(g_managerWindow))
                DestroyWindow(g_managerWindow);
            if (state->previewBitmap != nullptr)
            {
                DeleteObject(state->previewBitmap);
                state->previewBitmap = nullptr;
            }
            if (state->thumbnails != nullptr)
            {
                ListView_SetImageList(state->list, nullptr, LVSIL_NORMAL);
                ImageList_Destroy(state->thumbnails);
                state->thumbnails = nullptr;
            }
            state->running = false;
        }
        return 0;
    case WM_NCDESTROY:
        if (state != nullptr)
        {
            SetWindowLongPtrW(window, GWLP_USERDATA, 0);
            if (state->embedded)
            {
                state->window = nullptr;
                return DefWindowProcW(window, message, wParam, lParam);
            }
            if (g_appState == state) g_appState = nullptr;
            if (state->ownsOleSession) OleUninitialize();
            if (state->ownsUfSession) UF_terminate();
            HMODULE moduleReference = state->moduleReference;
            std::wstring className;
            className.swap(state->windowClassName);
            delete state;
            if (moduleReference != nullptr)
            {
                ModuleReleaseContext* context =
                    new (std::nothrow) ModuleReleaseContext();
                if (context != nullptr)
                {
                    context->module = moduleReference;
                    context->mainWindowClass.swap(className);
                }
                HANDLE unloadThread = context == nullptr ? nullptr :
                    CreateThread(nullptr, 0, ReleaseModuleAfterWindowProc,
                                 context, 0, nullptr);
                if (unloadThread != nullptr) CloseHandle(unloadThread);
                else delete context;
                // If CreateThread fails, deliberately retain the reference.
                // Leaking until NX exits is safer than unloading from WndProc.
            }
        }
        return DefWindowProcW(window, message, wParam, lParam);
    default: break;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

HWND FindBlockStylerWindow()
{
    struct Search
    {
        DWORD process = 0;
        HWND result = nullptr;
    } search{GetCurrentProcessId(), nullptr};
    EnumWindows([](HWND window, LPARAM parameter) -> BOOL
    {
        auto* search = reinterpret_cast<Search*>(parameter);
        DWORD process = 0;
        GetWindowThreadProcessId(window, &process);
        if (process != search->process) return TRUE;
        wchar_t title[256] = {};
        GetWindowTextW(window, title, 256);
        if (wcscmp(title, kTitle) == 0)
        {
            search->result = window;
            return FALSE;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&search));
    if (search.result != nullptr) return search.result;

    // NX calls dialogShown before every Block Styler window has published its
    // caption to EnumWindows.  The active window on this UI thread is already
    // the real dialog at that point, so use it as the lifecycle-safe fallback.
    HWND active = GetActiveWindow();
    DWORD process = 0;
    if (active != nullptr)
    {
        GetWindowThreadProcessId(active, &process);
        RECT bounds{};
        GetWindowRect(active, &bounds);
        const int width = bounds.right - bounds.left;
        const int height = bounds.bottom - bounds.top;
        if (process == search.process && width >= 300 && width <= 1200 &&
            height >= 300 && height <= 1200)
            return active;
    }
    return search.result;
}

std::string DialogFilePath()
{
    HMODULE module = nullptr;
    wchar_t path[MAX_PATH] = {};
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(&DialogFilePath), &module) ||
        GetModuleFileNameW(module, path, MAX_PATH) == 0)
        return "StandardPartsLibrary.dlx";
    wchar_t* slash = wcsrchr(path, L'\\');
    if (slash != nullptr) *(slash + 1) = L'\0';
    return ToAnsi(std::wstring(path) + L"StandardPartsLibrary.dlx");
}

class StandardPartsDialogHost
{
public:
    StandardPartsDialogHost()
        : ui_(NXOpen::UI::GetUI()),
          dialog_(ui_->CreateDialog(DialogFilePath().c_str()))
    {
        state_.embedded = true;
        dialog_->AddInitializeHandler(NXOpen::make_callback(
            this, &StandardPartsDialogHost::Initialize));
        dialog_->AddDialogShownHandler(NXOpen::make_callback(
            this, &StandardPartsDialogHost::DialogShown));
        dialog_->AddUpdateHandler(NXOpen::make_callback(
            this, &StandardPartsDialogHost::Update));
        dialog_->AddApplyHandler(NXOpen::make_callback(
            this, &StandardPartsDialogHost::Apply));
        dialog_->AddOkHandler(NXOpen::make_callback(
            this, &StandardPartsDialogHost::Ok));
        dialog_->AddCancelHandler(NXOpen::make_callback(
            this, &StandardPartsDialogHost::Cancel));
    }

    ~StandardPartsDialogHost()
    {
        if (timerId_ != 0)
            KillTimer(timerWindow_, timerId_);
        if (pendingTimerHost_ == this) pendingTimerHost_ = nullptr;
        if (pane_ != nullptr && IsWindow(pane_)) DestroyWindow(pane_);
        if (!paneClass_.empty() && module_ != nullptr)
            UnregisterClassW(paneClass_.c_str(), module_);
        if (g_appState == &state_) g_appState = nullptr;
        delete dialog_;
    }

    int Launch()
    {
        return static_cast<int>(dialog_->Launch());
    }

private:
    NXOpen::UI* ui_ = nullptr;
    NXOpen::BlockStyler::BlockDialog* dialog_ = nullptr;
    NXOpen::BlockStyler::DrawingArea* drawingArea_ = nullptr;
    NXOpen::BlockStyler::UIBlock* group_ = nullptr;
    NXOpen::BlockStyler::UIBlock* placementButtons_[4]{};
    NXOpen::BlockStyler::SelectObject* placementSelection_ = nullptr;
    NXOpen::BlockStyler::SelectObject* orientationSelection_ = nullptr;
    NXOpen::BlockStyler::SpecifyOrientation* orientation_ = nullptr;
    NXOpen::BlockStyler::UIBlock* quickPosition_ = nullptr;
    NXOpen::BlockStyler::SelectObject* trimSelection_ = nullptr;
    AppState state_;
    HWND pane_ = nullptr;
    HMODULE module_ = nullptr;
    std::wstring paneClass_;
    bool updating_ = false;
    HWND timerWindow_ = nullptr;
    UINT_PTR timerId_ = 0;
    int paneAttempts_ = 0;
    int pendingPlacementMode_ = -1;
    bool pendingQuickOrient_ = false;
    static StandardPartsDialogHost* pendingTimerHost_;

    static void CALLBACK BrowserTimerProc(HWND window, UINT, UINT_PTR timerId,
                                          DWORD)
    {
        auto* self = pendingTimerHost_;
        if (self == nullptr) return;
        if (self->pane_ != nullptr && IsWindow(self->pane_))
        {
            // NX may promote the DrawingArea again after another modal window
            // (for example the update notice) closes.  Keep our browser above
            // that sibling without activating it or disturbing user input.
            SetWindowPos(self->pane_, HWND_TOP, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE |
                             SWP_SHOWWINDOW);
            // Selection dialogs must not be opened from update_cb because NX
            // can re-enter the Block Styler update pipeline and deadlock.
            // Execute requested placement/orientation after that callback has
            // fully returned to the UI message loop.
            try
            {
                if (self->pendingPlacementMode_ >= 0)
                {
                    self->pendingPlacementMode_ = -1;
                    self->ActivatePlacementSelection();
                }
                if (self->pendingQuickOrient_)
                {
                    self->pendingQuickOrient_ = false;
                    QuickOrient(&self->state_);
                    self->SyncOrientationBlock();
                }
            }
            catch (const NXOpen::NXException& ex)
            {
                self->pendingPlacementMode_ = -1;
                self->pendingQuickOrient_ = false;
                self->ShowError(ex.Message());
            }
            catch (const std::exception& ex)
            {
                self->pendingPlacementMode_ = -1;
                self->pendingQuickOrient_ = false;
                self->ShowError(ex.what());
            }
            catch (...)
            {
                self->pendingPlacementMode_ = -1;
                self->pendingQuickOrient_ = false;
                self->ShowError("执行标准件定位时发生未知错误。");
            }
            return;
        }
        ++self->paneAttempts_;
        if (FindBlockStylerWindow() == nullptr && self->paneAttempts_ < 20)
            return;
        try { self->CreateBrowserPane(); }
        catch (const NXOpen::NXException& ex)
        {
            KillTimer(window, timerId); self->timerId_ = 0;
            pendingTimerHost_ = nullptr; self->ShowError(ex.Message());
        }
        catch (const std::exception& ex)
        {
            KillTimer(window, timerId); self->timerId_ = 0;
            pendingTimerHost_ = nullptr; self->ShowError(ex.what());
        }
        catch (...)
        {
            KillTimer(window, timerId); self->timerId_ = 0;
            pendingTimerHost_ = nullptr;
            self->ShowError("创建标准件库浏览区时发生未知错误。");
        }
    }

    static void SetLabel(NXOpen::BlockStyler::UIBlock* block,
                         const char* label)
    {
        if (block == nullptr) return;
        NXOpen::BlockStyler::PropertyList* properties = block->GetProperties();
        properties->SetString(
            NXOpen::NXString("Label", NXOpen::NXString::UTF8),
            NXOpen::NXString(label, NXOpen::NXString::UTF8));
        delete properties;
    }

    static void SetShow(NXOpen::BlockStyler::UIBlock* block, bool show)
    {
        if (block == nullptr) return;
        NXOpen::BlockStyler::PropertyList* properties = block->GetProperties();
        properties->SetLogical("Show", show);
        properties->SetLogical("Enable", show);
        delete properties;
    }

    void SyncOrientationBlock()
    {
        if (orientation_ == nullptr || !state_.hasPlacement) return;
        const bool wasUpdating = updating_;
        updating_ = true;
        try
        {
            orientation_->SetOrigin(NXOpen::Point3d(
                state_.placementOrigin[0], state_.placementOrigin[1],
                state_.placementOrigin[2]));
            orientation_->SetXAxis(NXOpen::Vector3d(
                state_.placementMatrix[0], state_.placementMatrix[1],
                state_.placementMatrix[2]));
            orientation_->SetYAxis(NXOpen::Vector3d(
                state_.placementMatrix[3], state_.placementMatrix[4],
                state_.placementMatrix[5]));
            orientation_->SetOriginSpecified(true);
        }
        catch (...)
        {
            updating_ = wasUpdating;
            throw;
        }
        updating_ = wasUpdating;
    }

    void ReadOrientationBlock()
    {
        if (orientation_ == nullptr) return;
        const NXOpen::Point3d origin = orientation_->Origin();
        NXOpen::Vector3d x = orientation_->XAxis();
        NXOpen::Vector3d y = orientation_->YAxis();
        auto normalize = [](NXOpen::Vector3d& vector) -> bool
        {
            const double length = std::sqrt(vector.X * vector.X +
                                            vector.Y * vector.Y +
                                            vector.Z * vector.Z);
            if (length <= 1.0e-9) return false;
            vector.X /= length;
            vector.Y /= length;
            vector.Z /= length;
            return true;
        };
        if (!normalize(x) || !normalize(y))
            throw std::runtime_error("指定方位的坐标轴无效。");
        NXOpen::Vector3d z(
            x.Y * y.Z - x.Z * y.Y,
            x.Z * y.X - x.X * y.Z,
            x.X * y.Y - x.Y * y.X);
        if (!normalize(z))
            throw std::runtime_error("指定方位的 X/Y 轴不能平行。");
        y = NXOpen::Vector3d(
            z.Y * x.Z - z.Z * x.Y,
            z.Z * x.X - z.X * x.Z,
            z.X * x.Y - z.Y * x.X);
        normalize(y);
        state_.placementOrigin[0] = origin.X;
        state_.placementOrigin[1] = origin.Y;
        state_.placementOrigin[2] = origin.Z;
        const double matrix[9] = {
            x.X, x.Y, x.Z, y.X, y.Y, y.Z, z.X, z.Y, z.Z};
        std::copy(matrix, matrix + 9, state_.placementMatrix);
        state_.hasPlacement = true;
        UpdatePlacementStatus(&state_);
    }

    void ActivatePlacementSelection()
    {
        if (placementSelection_ == nullptr) return;

        const int mode = state_.placementMode;
        std::vector<NXOpen::Selection::MaskTriple> masks;
        if (mode == 0)
        {
            masks.emplace_back(UF_solid_type, UF_all_subtype,
                               UF_UI_SEL_FEATURE_PLANAR_FACE);
            placementSelection_->SetCue("在目标平面上指定投影点");
            placementSelection_->SetLabelString("指定投影点");
        }
        else if (mode == 1)
        {
            masks.emplace_back(UF_point_type, UF_all_subtype, 0);
            masks.emplace_back(UF_solid_type, UF_all_subtype,
                               UF_UI_SEL_FEATURE_ANY_FACE);
            masks.emplace_back(UF_solid_type, UF_all_subtype,
                               UF_UI_SEL_FEATURE_ANY_EDGE);
            masks.emplace_back(UF_line_type, UF_all_subtype, 0);
            masks.emplace_back(UF_circle_type, UF_all_subtype, 0);
            masks.emplace_back(UF_conic_type, UF_all_subtype, 0);
            masks.emplace_back(UF_spline_type, UF_all_subtype, 0);
            placementSelection_->SetCue("在模型上捕捉标准件放置点");
            placementSelection_->SetLabelString("指定任意点");
        }
        else if (mode == 2)
        {
            masks.emplace_back(UF_solid_type, UF_all_subtype,
                               UF_UI_SEL_FEATURE_ANY_FACE);
            placementSelection_->SetCue("选择作为放置基准的面");
            placementSelection_->SetLabelString("指定基准面");
        }
        else
        {
            masks.emplace_back(UF_solid_type, UF_all_subtype,
                               UF_UI_SEL_FEATURE_ANY_EDGE);
            masks.emplace_back(UF_circle_type, UF_all_subtype, 0);
            placementSelection_->SetCue("选择圆或圆弧");
            placementSelection_->SetLabelString("指定圆弧");
        }

        updating_ = true;
        try
        {
            placementSelection_->SetSelectedObjects({});
            placementSelection_->SetSelectionFilter(
                NXOpen::Selection::SelectionActionClearAndEnableSpecific, masks);
            placementSelection_->SetSelectModeAsString("Single");
            placementSelection_->SetAutomaticProgression(false);
            placementSelection_->SetPointOverlay(true);
        }
        catch (...)
        {
            updating_ = false;
            throw;
        }
        updating_ = false;
        placementSelection_->Focus();
    }

    static bool MatrixFromPlanarFace(tag_t face, double matrix[9],
                                     double planePoint[3])
    {
        int faceType = 0;
        int normalDirection = 0;
        double direction[3]{};
        double box[6]{};
        double radius = 0.0;
        double radiusData = 0.0;
        if (UF_MODL_ask_face_data(face, &faceType, planePoint, direction, box,
                                  &radius, &radiusData, &normalDirection) != 0 ||
            faceType != 22)
            return false;
        if (normalDirection < 0)
            for (double& value : direction) value = -value;
        const double length = std::sqrt(direction[0] * direction[0] +
                                        direction[1] * direction[1] +
                                        direction[2] * direction[2]);
        if (length <= 1.0e-9) return false;
        double z[3] = {direction[0] / length, direction[1] / length,
                       direction[2] / length};
        double reference[3] = {std::abs(z[0]) < 0.9 ? 1.0 : 0.0,
                               std::abs(z[0]) < 0.9 ? 0.0 : 1.0, 0.0};
        double y[3] = {z[1] * reference[2] - z[2] * reference[1],
                       z[2] * reference[0] - z[0] * reference[2],
                       z[0] * reference[1] - z[1] * reference[0]};
        const double yLength = std::sqrt(y[0] * y[0] + y[1] * y[1] +
                                         y[2] * y[2]);
        if (yLength <= 1.0e-9) return false;
        for (double& value : y) value /= yLength;
        double x[3] = {y[1] * z[2] - y[2] * z[1],
                       y[2] * z[0] - y[0] * z[2],
                       y[0] * z[1] - y[1] * z[0]};
        std::copy(x, x + 3, matrix);
        std::copy(y, y + 3, matrix + 3);
        std::copy(z, z + 3, matrix + 6);
        return true;
    }

    void ReadPlacementSelection()
    {
        if (placementSelection_ == nullptr) return;
        const std::vector<NXOpen::TaggedObject*> objects =
            placementSelection_->GetSelectedObjects();
        const NXOpen::Point3d picked = placementSelection_->PickPoint();
        std::wstring error;

        if (state_.placementMode == 1)
        {
            state_.placementOrigin[0] = picked.X;
            state_.placementOrigin[1] = picked.Y;
            state_.placementOrigin[2] = picked.Z;
            double unusedOrigin[3]{};
            state_.hasPlacement = AskWcs(unusedOrigin,
                                          state_.placementMatrix, error);
        }
        else if (objects.empty() || objects.front() == nullptr)
        {
            return;
        }
        else if (state_.placementMode == 0)
        {
            double planePoint[3]{};
            if (!MatrixFromPlanarFace(objects.front()->Tag(),
                                      state_.placementMatrix, planePoint))
            {
                error = L"点投影面只支持平面。";
            }
            else
            {
                const double point[3] = {picked.X, picked.Y, picked.Z};
                const double* normal = state_.placementMatrix + 6;
                const double distance =
                    (point[0] - planePoint[0]) * normal[0] +
                    (point[1] - planePoint[1]) * normal[1] +
                    (point[2] - planePoint[2]) * normal[2];
                for (int axis = 0; axis < 3; ++axis)
                    state_.placementOrigin[axis] =
                        point[axis] - distance * normal[axis];
                state_.hasPlacement = true;
            }
        }
        else
        {
            double box[6]{};
            const int code = UF_MODL_ask_bounding_box(objects.front()->Tag(), box);
            if (code != 0)
                error = L"无法取得所选对象中心：" + UfError(code);
            else
            {
                for (int axis = 0; axis < 3; ++axis)
                    state_.placementOrigin[axis] =
                        (box[axis] + box[axis + 3]) * 0.5;
                bool oriented = false;
                if (state_.placementMode == 2)
                {
                    double planePoint[3]{};
                    oriented = MatrixFromPlanarFace(objects.front()->Tag(),
                                                     state_.placementMatrix,
                                                     planePoint);
                }
                if (!oriented)
                {
                    double unusedOrigin[3]{};
                    if (!AskWcs(unusedOrigin, state_.placementMatrix, error))
                    {
                        UpdatePlacementStatus(&state_);
                        return;
                    }
                }
                state_.hasPlacement = true;
            }
        }
        UpdatePlacementStatus(&state_);
        if (state_.hasPlacement) SyncOrientationBlock();
        if (!error.empty())
            ShowError(ToAnsi(error));
    }

    void ShowError(const std::string& message) noexcept
    {
        try
        {
            ui_->NXMessageBox()->Show(
                "智辉标准件库", NXOpen::NXMessageBox::DialogTypeError,
                message.c_str());
        }
        catch (...) {}
    }

    void Initialize()
    {
        try
        {
            NXOpen::BlockStyler::CompositeBlock* top = dialog_->TopBlock();
            drawingArea_ = dynamic_cast<NXOpen::BlockStyler::DrawingArea*>(
                top->FindBlock("drawingArea0"));
            group_ = top->FindBlock("group01");
            placementButtons_[0] = top->FindBlock("buttonS1");
            placementButtons_[1] = top->FindBlock("buttonS2");
            placementButtons_[2] = top->FindBlock("buttonS3");
            placementButtons_[3] = top->FindBlock("buttonS4");
            placementSelection_ = dynamic_cast<NXOpen::BlockStyler::SelectObject*>(
                top->FindBlock("selection0"));
            orientationSelection_ = dynamic_cast<NXOpen::BlockStyler::SelectObject*>(
                top->FindBlock("selection01"));
            orientation_ = dynamic_cast<NXOpen::BlockStyler::SpecifyOrientation*>(
                top->FindBlock("manip0"));
            quickPosition_ = top->FindBlock("buttonCsysPos");
            trimSelection_ = dynamic_cast<NXOpen::BlockStyler::SelectObject*>(
                top->FindBlock("selectionTrim"));
            if (drawingArea_ == nullptr || group_ == nullptr ||
                placementSelection_ == nullptr || trimSelection_ == nullptr)
                throw std::runtime_error(
                    "StandardPartsLibrary.dlx 缺少燕秀布局的必需控件。");

            // Drawing-area geometry is an initialization-only Block Styler
            // property.  Yanxiu's managed host sets it while initializing; do
            // the same here, before NX finishes constructing the dialog.
            // NX 2412 adds these logical values to the fixed geometry already
            // stored in Yanxiu's DLX.  Keep only the small compatibility
            // allowance needed for its original roughly 660 x 770 layout;
            // the embedded browser itself remains a 610 x 525 child surface.
            drawingArea_->SetWidth(90);
            drawingArea_->SetHeight(10);

            std::vector<NXOpen::Selection::MaskTriple> bodyMasks;
            bodyMasks.emplace_back(UF_solid_type, UF_solid_body_subtype, 0);
            trimSelection_->SetSelectionFilter(
                NXOpen::Selection::SelectionActionClearAndEnableSpecific,
                bodyMasks);
            trimSelection_->SetSelectModeAsString("Multiple");
            trimSelection_->SetAutomaticProgression(false);

            placementSelection_->SetSelectModeAsString("Single");
            placementSelection_->SetAutomaticProgression(false);
            placementSelection_->SetPointOverlay(true);
        }
        catch (const NXOpen::NXException& ex) { ShowError(ex.Message()); }
        catch (const std::exception& ex) { ShowError(ex.what()); }
        catch (...) { ShowError("初始化燕秀对话框结构时发生未知错误。"); }
    }

    void DialogShown()
    {
        try
        {
            // Keep the labels and icons from the copied Yanxiu DLX intact.
            SetShow(topHiddenParameters(), false);
            // Yanxiu's host hides this auxiliary block.  Leaving it visible
            // creates an unrelated second "指定放置" row on NX 2412.
            SetShow(orientationSelection_, false);
            // dialogShown precedes publication of the native NX window title
            // on NX 2412.  Defer the Win32 child attachment on the same UI
            // thread until the real Block Styler window can be enumerated.
            // dialogShown may run while NX deliberately reports no foreground
            // HWND.  A thread timer belongs to this NX UI thread and therefore
            // does not depend on an incompletely published native dialog.
            timerWindow_ = nullptr;
            pendingTimerHost_ = this;
            timerId_ = SetTimer(nullptr, 0, 100, BrowserTimerProc);
            if (timerId_ == 0)
            {
                pendingTimerHost_ = nullptr;
                throw std::runtime_error("无法启动标准件库界面挂载任务。");
            }
        }
        catch (const NXOpen::NXException& ex) { ShowError(ex.Message()); }
        catch (const std::exception& ex) { ShowError(ex.what()); }
        catch (...) { ShowError("显示标准件库时发生未知错误。"); }
    }

    NXOpen::BlockStyler::UIBlock* topHiddenParameters()
    {
        return dialog_->TopBlock()->FindBlock("group");
    }

    void CreateBrowserPane()
    {
        if (pane_ != nullptr && IsWindow(pane_)) return;
        HWND host = FindBlockStylerWindow();
        if (host == nullptr)
            throw std::runtime_error("无法找到 NX 标准件库对话框窗口。");
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                              GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          reinterpret_cast<LPCWSTR>(&DialogFilePath), &module_);
        paneClass_ = L"ZhihuiStandardPartsBrowserPane_" +
            std::to_wstring(reinterpret_cast<std::uintptr_t>(module_));
        WNDCLASSEXW windowClass{};
        windowClass.cbSize = sizeof(windowClass);
        windowClass.lpfnWndProc = WindowProc;
        windowClass.hInstance = module_;
        windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        windowClass.hbrBackground =
            reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
        windowClass.lpszClassName = paneClass_.c_str();
        if (!RegisterClassExW(&windowClass) &&
            GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            throw std::runtime_error("无法创建标准件库浏览区。");

        RECT client{};
        GetClientRect(host, &client);
        const int width = std::max(610,
            static_cast<int>(client.right - client.left - 16));
        state_.parent = host;
        state_.embedded = true;
        pane_ = CreateWindowExW(
            0, paneClass_.c_str(), L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
            8, 8, width, 525, host, nullptr, module_, &state_);
        if (pane_ == nullptr)
            throw std::runtime_error("标准件库浏览区创建失败。");
        g_appState = &state_;
        BringWindowToTop(pane_);
    }

    int Update(NXOpen::BlockStyler::UIBlock* block)
    {
        if (updating_) return 0;
        try
        {
            updating_ = true;
            for (int index = 0; index < 4; ++index)
            {
                if (block == placementButtons_[index])
                {
                    // Actual Yanxiu button order, identified by the original
                    // DLX icons: face centre, circle/arc centre, arbitrary
                    // point, point projected to face.
                    static const int modes[] = {2, 3, 1, 0};
                    SetPlacementMode(&state_, modes[index]);
                    pendingPlacementMode_ = modes[index];
                }
            }
            if (block == placementSelection_)
                ReadPlacementSelection();
            if (block == orientation_)
                ReadOrientationBlock();
            if (block == quickPosition_) pendingQuickOrient_ = true;
            updating_ = false;
        }
        catch (const NXOpen::NXException& ex)
        {
            updating_ = false; ShowError(ex.Message()); return ex.ErrorCode();
        }
        catch (const std::exception& ex)
        {
            updating_ = false; ShowError(ex.what()); return 1;
        }
        catch (...)
        {
            updating_ = false; ShowError("更新标准件库对话框时发生未知错误。"); return 1;
        }
        return 0;
    }

    void ReadTrimTargets()
    {
        state_.trimTargets.clear();
        for (NXOpen::TaggedObject* object : trimSelection_->GetSelectedObjects())
        {
            if (object != nullptr) state_.trimTargets.push_back(object->Tag());
        }
    }

    int Apply()
    {
        try
        {
            if (state_.hasPlacement) ReadOrientationBlock();
            ReadTrimTargets();
            InsertSelected(&state_);
            return 0;
        }
        catch (const NXOpen::NXException& ex) { ShowError(ex.Message()); return ex.ErrorCode(); }
        catch (const std::exception& ex) { ShowError(ex.what()); return 1; }
        catch (...) { ShowError("调用标准件时发生未知错误。"); return 1; }
    }

    int Ok() { return Apply(); }
    int Cancel() { return 0; }
};

StandardPartsDialogHost* StandardPartsDialogHost::pendingTimerHost_ = nullptr;
}

int LaunchStandardPartsLibrary(bool& keepUfInitialized)
{
    keepUfInitialized = false;
    // Block Styler refuses to create a dialog when NX has no work part.  Catch
    // that state here so NX does not turn it into a callback automation error.
    if (UF_ASSEM_ask_work_part() == NULL_TAG)
    {
        MessageBoxW(GetForegroundWindow(), L"请先打开一个工作部件。",
                    L"智辉标准件库", MB_OK | MB_ICONINFORMATION);
        return 0;
    }
    INITCOMMONCONTROLSEX commonControls{sizeof(commonControls), ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&commonControls);
    const HRESULT oleStatus = OleInitialize(nullptr);
    int result = 0;
    try
    {
        StandardPartsDialogHost host;
        result = host.Launch();
    }
    catch (...)
    {
        if (SUCCEEDED(oleStatus)) OleUninitialize();
        throw;
    }
    if (SUCCEEDED(oleStatus)) OleUninitialize();
    return result;
}
