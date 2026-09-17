#include "StandardPartsLibrary.hpp"

#include <uf.h>
#include <uf_assem.h>
#include <uf_csys.h>
#include <uf_curve.h>
#include <uf_disp.h>
#include <uf_eval.h>
#include <uf_group.h>
#include <uf_modl.h>
#include <uf_modl_expressions_retiring.h>
#include <uf_mtx.h>
#include <uf_trns.h>
#include <uf_obj.h>
#include <uf_part.h>
#include <uf_retiring_ugopenint.h>
#include <uf_ui.h>

#include <NXOpen/BlockStyler_BlockDialog.hxx>
#include <NXOpen/BlockStyler_CompositeBlock.hxx>
#include <NXOpen/BlockStyler_DrawingArea.hxx>
#include <NXOpen/BlockStyler_DoubleBlock.hxx>
#include <NXOpen/BlockStyler_PropertyList.hxx>
#include <NXOpen/BlockStyler_SelectObject.hxx>
#include <NXOpen/BlockStyler_SpecifyPoint.hxx>
#include <NXOpen/BlockStyler_StringBlock.hxx>
#include <NXOpen/BlockStyler_SpecifyOrientation.hxx>
#include <NXOpen/BlockStyler_UIBlock.hxx>
#include <NXOpen/Gateway_ImageExportBuilder.hxx>
#include <NXOpen/Expression.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/PartSaveStatus.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXMessageBox.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/Selection.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/UI.hxx>
#include <NXOpen/ViewCollection.hxx>

#include <Windows.h>
#include <CommCtrl.h>
#include <commdlg.h>
#include <ShlObj.h>
#include <shobjidl_core.h>
#include <shellapi.h>
#include <wincodec.h>
#include <wrl/client.h>

#ifdef CreateDialog
#undef CreateDialog
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <new>
#include <memory>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{
class CapturePreviewImage;
struct ParameterSaveRequest;
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
    ID_PATTERN_SPACING_X, ID_PATTERN_SPACING_Y, ID_PATTERN_COUNT_X,
    ID_PATTERN_COUNT_Y,
    ID_PARAM_LABEL1, ID_PARAM_LABEL2, ID_PARAM_LABEL3, ID_PARAM_LABEL4,
    ID_PARAM_VALUE1, ID_PARAM_VALUE2, ID_PARAM_VALUE3, ID_PARAM_VALUE4,
    ID_EDIT_MODEL, ID_CAPTURE_BODIES, ID_INSERT_PARAMETERS, ID_SPEC_LABEL,
    ID_REPLACE_PREVIEW, ID_CAPTURE_PREVIEW, ID_DEFINE_PARAMETERS
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
    int libraryFilter = 1;
    std::array<std::wstring, 3> libraryCategories;
    std::array<std::wstring, 3> librarySearches;
    int placementMode = 1;
    int patternMode = 0;
    double patternSpacingX = 100.0;
    double patternSpacingY = 100.0;
    int patternCountX = 2;
    int patternCountY = 2;
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
    bool axisLocked[3]{false, false, false};
    void* orientationContext = nullptr;
    void (*orientationChanged)(void*) = nullptr;
    void (*capturePreview)(void*, CapturePreviewImage&) = nullptr;
    void (*prepareParameterSave)(void*, ParameterSaveRequest&) = nullptr;
    HMODULE moduleReference = nullptr;
    std::wstring windowClassName;
    fs::path requestedEditModel;
    fs::path requestedCaptureRoot;
    std::wstring requestedCaptureCategory;
    std::shared_ptr<ParameterSaveRequest> requestedParameterSave;
    std::wstring resumeItemId;
    std::wstring resumeGroupName;
    std::vector<std::wstring> specificationGroups;
    fs::path libraryRootOverride;
};

AppState* g_appState = nullptr;

// The browser pane is native Win32, not a Block Styler selection block.
// Only a valid placement controls OK/Apply availability; insertion validates
// the selected library item and optional trim targets and reports errors.
bool CanConfirmPlacement(bool shown, bool closing, bool hasPlacement,
                         bool insertedAny) noexcept
{
    return shown && !closing && (hasPlacement || insertedAny);
}

HWND g_managerWindow = nullptr;
HWND g_parameterWindow = nullptr;
HWND g_quickPositionWindow = nullptr;

bool RegisterOwnedWindowClass(const WNDCLASSEXW& windowClass, HWND owner)
{
    // NX can unload and reload this DLL at the same address. A class left by
    // an older build must never retain its now invalid window procedure.
    UnregisterClassW(windowClass.lpszClassName, windowClass.hInstance);
    if (RegisterClassExW(&windowClass)) return true;
    UF_print_syslog("[StandardPartsLibrary] Window class registration failed\n", false);
    MessageBoxW(owner, L"管理窗口初始化失败，请关闭标准件库后重新打开。",
                L"智辉标准件库", MB_OK | MB_ICONERROR);
    return false;
}

void UnregisterOwnedWindowClasses(HMODULE module)
{
    if (module == nullptr) return;
    for (const auto* name : {L"ZhihuiStandardPartsManagerWindow",
                            L"ZhihuiStandardPartsQuickPositionWindow",
                            L"ZhihuiStandardPartsParameters",
                            L"ZhihuiStandardPartsParameterDefinition"})
        UnregisterClassW(name, module);
}

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
    UnregisterOwnedWindowClasses(module);
    if (!context->mainWindowClass.empty())
        UnregisterClassW(context->mainWindowClass.c_str(), module);
    delete context;
    FreeLibraryAndExitThread(module, 0);
}

void UpdatePreview(AppState* state);
std::wstring Lower(std::wstring value);
void ShowQuickPosition(AppState* state);

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
    if (!state->libraryRootOverride.empty()) return state->libraryRootOverride;
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
            [&](const LibraryItem& item)
            {
                return Lower(fs::path(item.relativePath).lexically_normal().generic_wstring()) ==
                    Lower(relativeToRoot.lexically_normal().generic_wstring());
            }))
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
            {
                const auto relative = Lower(fs::path(fields[3]).lexically_normal().generic_wstring());
                const bool parameterized = relative.rfind(L"libparam/", 0) == 0 ||
                    (relative.rfind(L"lib/", 0) != 0 && fields.size() >= 6 && fields[5] == L"1");
                state->items.push_back({fields[0], fields[1], fields[2], fields[3],
                                        fields.size() >= 5 && !fields[4].empty()
                                            ? fields[4] : fs::path(fields[3]).stem().wstring(),
                                        parameterized});
            }
        }
    }
    ScanLibraryFolder(state, LibraryRoot(state) / L"Lib", false);
    ScanLibraryFolder(state, LibraryRoot(state) / L"LibParam", true);
    SetStatus(state, L"已载入 " + std::to_wstring(state->items.size()) + L" 个标准件。");
}

bool InCurrentLibrary(const AppState* state, const LibraryItem& item)
{
    return item.parameterized == (state->libraryFilter == 2);
}

const wchar_t* CurrentLibraryName(const AppState* state)
{
    return state->libraryFilter == 2 ? L"本地有参" : L"本地无参";
}

fs::path CurrentLibraryFolder(AppState* state)
{
    return LibraryRoot(state) / (state->libraryFilter == 2 ? L"LibParam" : L"Lib");
}

void RefreshCategories(AppState* state)
{
    state->refreshingCategories = true;
    const std::wstring old = state->selectedCategory;
    TreeView_DeleteAllItems(state->category);
    state->categories.clear();
    std::set<std::wstring> uniqueCategories;
    std::size_t total = 0;
    for (const auto& item : state->items)
        if (InCurrentLibrary(state, item)) { uniqueCategories.insert(item.category); ++total; }
    state->categories.assign(uniqueCategories.begin(), uniqueCategories.end());

    TVINSERTSTRUCTW insert{};
    insert.hParent = TVI_ROOT;
    insert.hInsertAfter = TVI_LAST;
    insert.item.mask = TVIF_TEXT | TVIF_PARAM;
    std::wstring rootText = std::wstring(CurrentLibraryName(state)) + L"  (" + std::to_wstring(total) + L")";
    insert.item.pszText = rootText.data();
    insert.item.lParam = 0;
    const HTREEITEM root = TreeView_InsertItem(state->category, &insert);
    HTREEITEM selected = root;
    for (std::size_t index = 0; index < state->categories.size(); ++index)
    {
        const auto& category = state->categories[index];
        const auto count = std::count_if(state->items.begin(), state->items.end(),
            [&](const LibraryItem& item) { return InCurrentLibrary(state, item) && item.category == category; });
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

void RefreshSpecifications(AppState* state, std::size_t preferred = SIZE_MAX,
    const std::wstring& groupName = {});

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
        if (!InCurrentLibrary(state, item)) continue;
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
        if (data != CB_ERR && data >= 0 && static_cast<std::size_t>(data) < state->items.size() &&
            InCurrentLibrary(state, state->items[static_cast<std::size_t>(data)]))
            return static_cast<std::size_t>(data);
    }
    const auto family = SelectedFamilyIndex(state);
    return family < state->items.size() && InCurrentLibrary(state, state->items[family]) ? family : SIZE_MAX;
}

void SelectLibrary(AppState* state, int filter)
{
    // A manager belongs to the library that opened it; never retarget it silently.
    if (g_managerWindow && IsWindow(g_managerWindow)) DestroyWindow(g_managerWindow);
    state->libraryCategories[state->libraryFilter] = state->selectedCategory;
    state->librarySearches[state->libraryFilter] = GetText(state->window, ID_SEARCH);
    state->libraryFilter = filter == 2 ? 2 : 1;
    state->selectedCategory = state->libraryCategories[state->libraryFilter];
    SetText(state->window, ID_SEARCH, state->librarySearches[state->libraryFilter]);
    CheckRadioButton(state->window, ID_FILTER_STATIC, ID_FILTER_PARAM,
        state->libraryFilter == 2 ? ID_FILTER_PARAM : ID_FILTER_STATIC);
    SetText(state->window, ID_CAPTURE_BODIES,
        state->libraryFilter == 2 ? L"当前部件\r\n加入库" : L"选择体\r\n加入库");
    RefreshCategories(state);
    RefreshList(state);
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
    ShowQuickPosition(state);
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

double PositiveNumber(HWND window, int id, double fallback)
{
    const std::wstring text = Trim(GetText(window, id));
    wchar_t* end = nullptr;
    const double value = wcstod(text.c_str(), &end);
    return end != text.c_str() && *end == L'\0' && std::isfinite(value) &&
                   value > 0.0
        ? value
        : fallback;
}

int PositiveCount(HWND window, int id, int fallback)
{
    const std::wstring text = Trim(GetText(window, id));
    wchar_t* end = nullptr;
    const long value = wcstol(text.c_str(), &end, 10);
    return end != text.c_str() && *end == L'\0'
        ? std::clamp(static_cast<int>(value), 1, 50)
        : fallback;
}

void ReadPatternSettings(AppState* state)
{
    state->patternSpacingX = PositiveNumber(
        state->window, ID_PATTERN_SPACING_X, state->patternSpacingX);
    state->patternSpacingY = PositiveNumber(
        state->window, ID_PATTERN_SPACING_Y, state->patternSpacingY);
    state->patternCountX = PositiveCount(
        state->window, ID_PATTERN_COUNT_X, state->patternCountX);
    state->patternCountY = PositiveCount(
        state->window, ID_PATTERN_COUNT_Y, state->patternCountY);
}

std::array<double, 3> OffsetPoint(const double origin[3],
                                  const double matrix[9], double x, double y)
{
    return {origin[0] + x * matrix[0] + y * matrix[3],
            origin[1] + x * matrix[1] + y * matrix[4],
            origin[2] + x * matrix[2] + y * matrix[5]};
}

bool TargetPatternFrame(const AppState* state, const double matrix[9],
                        std::array<double, 3>& center,
                        double& width, double& height)
{
    if (state->trimTargets.empty()) return false;
    double box[6]{};
    if (UF_MODL_ask_bounding_box(state->trimTargets.front(), box) != 0)
        return false;
    center = {(box[0] + box[3]) * 0.5,
              (box[1] + box[4]) * 0.5,
              (box[2] + box[5]) * 0.5};
    double minX = 1.0e100, maxX = -1.0e100;
    double minY = 1.0e100, maxY = -1.0e100;
    for (int ix = 0; ix < 2; ++ix)
        for (int iy = 0; iy < 2; ++iy)
            for (int iz = 0; iz < 2; ++iz)
            {
                const double point[3] = {box[ix ? 3 : 0], box[iy ? 4 : 1],
                                         box[iz ? 5 : 2]};
                const double relative[3] = {
                    point[0] - center[0], point[1] - center[1],
                    point[2] - center[2]};
                const double x = relative[0] * matrix[0] +
                                 relative[1] * matrix[1] +
                                 relative[2] * matrix[2];
                const double y = relative[0] * matrix[3] +
                                 relative[1] * matrix[4] +
                                 relative[2] * matrix[5];
                minX = std::min(minX, x); maxX = std::max(maxX, x);
                minY = std::min(minY, y); maxY = std::max(maxY, y);
            }
    width = maxX - minX;
    height = maxY - minY;
    return width > 1.0e-6 && height > 1.0e-6;
}

std::vector<std::array<double, 3>> PatternOrigins(
    AppState* state, const double origin[3], const double matrix[9])
{
    ReadPatternSettings(state);
    std::vector<std::array<double, 3>> points;
    std::array<double, 3> center{origin[0], origin[1], origin[2]};
    double width = state->patternSpacingX;
    double height = state->patternSpacingY;
    const bool hasTarget = TargetPatternFrame(state, matrix, center, width, height);
    const double halfX = width * 0.5;
    const double halfY = height * 0.5;
    const double* base = (state->patternMode == 5 && hasTarget)
        ? center.data() : origin;
    const auto add = [&](double x, double y)
    {
        points.push_back(OffsetPoint(base, matrix, x, y));
    };
    switch (state->patternMode)
    {
    case 1:
        for (int i = 0; i < state->patternCountX; ++i)
            add((i - (state->patternCountX - 1) * 0.5) * state->patternSpacingX, 0.0);
        break;
    case 2:
        for (int i = 0; i < state->patternCountY; ++i)
            add(0.0, (i - (state->patternCountY - 1) * 0.5) * state->patternSpacingY);
        break;
    case 3: add(-halfX, -halfY); add(halfX, halfY); break;
    case 4:
        add(-halfX, -halfY); add(halfX, -halfY);
        add(-halfX, halfY); add(halfX, halfY); break;
    case 5: add(0.0, 0.0); break;
    case 6:
    {
        const int countX = state->patternCountX;
        const int countY = state->patternCountY;
        for (int row = 0; row < countY; ++row)
            for (int column = 0; column < countX; ++column)
                add((column - (countX - 1) * 0.5) * state->patternSpacingX,
                    (row - (countY - 1) * 0.5) * state->patternSpacingY);
        break;
    }
    default: add(0.0, 0.0); break;
    }
    return points;
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

#include "StandardPartsParameterData.inc"
#include "StandardPartsGroupSelection.inc"

std::vector<std::pair<std::wstring, std::wstring>> LoadSpecificationParameters(
    const fs::path& model, const std::wstring& specification)
{
    std::vector<std::pair<std::wstring, std::wstring>> result;
    try
    {
        const auto schema = ReadParameterSchema(model);
        if (!schema.empty())
        {
            const auto values = SpecificationValues(model, specification, schema);
            for (std::size_t i = 0; i < schema.size(); ++i)
            {
                std::wstring label = schema[i].label;
                const std::wstring suffix = L" " + schema[i].expression;
                if (label.size() > suffix.size() &&
                    label.compare(label.size() - suffix.size(), suffix.size(), suffix) == 0)
                    label.resize(label.size() - suffix.size());
                result.emplace_back(label, values[i]);
            }
            return result;
        }
    }
    catch (const std::exception& ex) { return {{L"参数配置错误", FromAnsi(ex.what())}}; }
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

#include "StandardPartsPreviewImage.inc"

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
    const bool parameterized = index < state->items.size() && state->items[index].parameterized;
    if (auto button = GetDlgItem(state->window, ID_INSERT_PARAMETERS))
        ShowWindow(button, parameterized ? SW_SHOW : SW_HIDE);
    if (auto label = GetDlgItem(state->window, ID_SPEC_LABEL))
        ShowWindow(label, parameterized ? SW_HIDE : SW_SHOW);
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
            fs::path preview = FindSidecarPreview(model);
            if (preview.empty() && state->items[index].parameterized &&
                fs::exists(model.parent_path() / L"parameter-preview.png"))
                preview = model.parent_path() / L"parameter-preview.png";
            // Never ask the Windows shell to extract a thumbnail from an NX .prt file.
            // Siemens' shell thumbnail provider can re-enter the active NX process while
            // this dialog is running on NX's UI thread, leaving both sides waiting.
            if (!preview.empty())
            {
                // Read the image itself so replacing a same-named sidecar is
                // visible immediately instead of reusing the shell's cache.
                try { state->previewBitmap = ReadPreviewBitmap(preview); }
                catch (const std::exception& ex) { UF_print_syslog(const_cast<char*>(ex.what()), false); }
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
    if (state->patternMode == 5 && state->trimTargets.empty())
    {
        error = L"居中放置需要先在“选择需要修剪的实体”中选择一个目标实体。";
        return false;
    }
    double origin[3]{};
    double matrix[9]{};
    if (!AskPlacement(state, origin, matrix, error)) return false;
    UF_PART_load_status_t loadStatus{};
    const std::string path = ToAnsi(model.wstring());
    const std::string instanceName = ToAnsi(SanitizeFileName(item.name).substr(0, 30));
    const auto origins = PatternOrigins(state, origin, matrix);
    for (std::size_t placement = 0; placement < origins.size(); ++placement)
    {
        tag_t instance = NULL_TAG;
        std::string numberedName = instanceName;
        if (origins.size() > 1)
            numberedName += "_" + std::to_string(placement + 1);
        const int code = UF_ASSEM_add_part_to_assembly2(
            // User-defined library parts need not contain a MODEL reference
            // set. A null name selects the entire part (UF_ASSEM contract).
            workPart, path.c_str(), nullptr, numberedName.c_str(),
            const_cast<double*>(origins[placement].data()), matrix,
            -1, &instance, &loadStatus);
        UF_PART_free_load_status(&loadStatus);
        loadStatus = {};
        if (code != 0 || instance == NULL_TAG)
        {
            error = L"创建第 " + std::to_wstring(placement + 1) +
                    L" 个装配组件失败：" + UfError(code);
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
            const int importCode = UF_PART_import(
                path.c_str(), &modes, destinationCsys,
                const_cast<double*>(origins[placement].data()), 1.0, &group);
            if (importCode != 0)
            {
                error = L"组件已装配，但导入修剪工具失败：" +
                        UfError(importCode);
                return false;
            }
            if (!ApplyAutomaticTrim(state, ImportedBodies(group), true, error))
            {
                error = L"组件已装配，但" + error;
                return false;
            }
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
    if (state->patternMode == 5 && state->trimTargets.empty())
    {
        error = L"居中放置需要先在“选择需要修剪的实体”中选择一个目标实体。";
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
    const std::string path = ToAnsi(model.wstring());
    const auto origins = PatternOrigins(state, origin, matrix);
    for (std::size_t placement = 0; placement < origins.size(); ++placement)
    {
        tag_t group = NULL_TAG;
        const int code = UF_PART_import(
            path.c_str(), &modes, destinationCsys,
            const_cast<double*>(origins[placement].data()), 1.0, &group);
        if (code != 0)
        {
            error = L"合并第 " + std::to_wstring(placement + 1) +
                    L" 个标准件失败：" + UfError(code);
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
    }
    UF_DISP_regenerate_display();
    return true;
}

struct PreviewSegment
{
    std::array<double, 3> start;
    std::array<double, 3> end;
};

constexpr std::size_t kPreviewSegmentBudget = 5000;
using PreviewBox = std::array<double, 6>;

void IncludePreviewPoint(PreviewBox& box, const std::array<double, 3>& point)
{
    for (int axis = 0; axis < 3; ++axis)
    {
        box[axis] = std::min(box[axis], point[axis]);
        box[axis + 3] = std::max(box[axis + 3], point[axis]);
    }
}

std::vector<PreviewSegment> PreviewBoxWireframe(const std::vector<PreviewBox>& boxes)
{
    if (boxes.empty()) return {};
    // Keep every body's extent. If there are too many bodies, merge their boxes
    // instead of dropping later bodies or allocating an unbounded preview.
    if (boxes.size() > kPreviewSegmentBudget / 12)
    {
        PreviewBox overall = boxes.front();
        for (const auto& box : boxes)
        {
            IncludePreviewPoint(overall, {box[0], box[1], box[2]});
            IncludePreviewPoint(overall, {box[3], box[4], box[5]});
        }
        return PreviewBoxWireframe({overall});
    }
    std::vector<PreviewSegment> result;
    result.reserve(boxes.size() * 12);
    for (const auto& box : boxes)
        for (int corner = 0; corner < 8; ++corner)
            for (int axis = 0; axis < 3; ++axis)
                if ((corner & (1 << axis)) == 0)
                {
                    std::array<double, 3> start{};
                    for (int coordinate = 0; coordinate < 3; ++coordinate)
                        start[coordinate] = box[coordinate + ((corner & (1 << coordinate)) ? 3 : 0)];
                    auto end = start;
                    end[axis] = box[axis + 3];
                    result.push_back({start, end});
                }
    return result;
}

PreviewBox PreviewExtent(const std::vector<PreviewSegment>& segments)
{
    const auto& first = segments.front().start;
    PreviewBox box{first[0], first[1], first[2], first[0], first[1], first[2]};
    for (const auto& segment : segments)
    {
        IncludePreviewPoint(box, segment.start);
        IncludePreviewPoint(box, segment.end);
    }
    return box;
}

void CheckPreviewUf(int code)
{
    if (code != 0) throw std::runtime_error(ToAnsi(UfError(code)));
}

double UnitMillimeters(int units)
{
    switch (units)
    {
    case 1: return 1.0;
    case 2: return 25.4;
    case 3: return 0.001;
    case 4: return 1000.0;
    default: throw std::runtime_error("Unsupported preview units");
    }
}

// Read-only source geometry. Quiet loading never changes the work/display part;
// already-loaded user parts are never closed by this helper.
std::vector<PreviewSegment> LoadPlacementWireframe(const fs::path& path, int& units,
    bool* simplified = nullptr)
{
    if (simplified != nullptr) *simplified = false;
    const std::string name = ToAnsi(path.wstring());
    const int loadState = UF_PART_is_loaded(name.c_str());
    if (loadState < 0 || loadState > 2) CheckPreviewUf(loadState);
    const bool wasLoaded = loadState != 0;
    tag_t part = UF_PART_ask_part_tag(name.c_str());
    struct PartScope
    {
        tag_t& part;
        bool owned;
        ~PartScope()
        {
            if (owned && part != NULL_TAG) UF_PART_close(part, 0, 2);
        }
    } partScope{part, !wasLoaded};
    if (loadState != 1 || part == NULL_TAG)
    {
        UF_PART_load_status_t status{};
        const int code = UF_PART_open_quiet(name.c_str(), &part, &status);
        UF_PART_free_load_status(&status);
        CheckPreviewUf(code);
    }
    CheckPreviewUf(UF_PART_ask_units(part, &units));
    std::vector<PreviewSegment> segments;
    std::vector<tag_t> bodies;
    tag_t body = NULL_TAG;
    while (true)
    {
        CheckPreviewUf(UF_OBJ_cycle_objs_in_part(part, UF_solid_type, &body));
        if (body == NULL_TAG) break;
        int type = 0, subtype = 0;
        CheckPreviewUf(UF_OBJ_ask_type_and_subtype(body, &type, &subtype));
        if (subtype != UF_solid_body_subtype) continue;
        bodies.push_back(body);
    }
    const auto simplifiedOutline = [&]()
    {
        std::vector<PreviewBox> boxes;
        for (tag_t solid : bodies)
        {
            PreviewBox box{};
            CheckPreviewUf(UF_MODL_ask_bounding_box(solid, box.data()));
            boxes.push_back(box);
        }
        if (simplified != nullptr) *simplified = true;
        return PreviewBoxWireframe(boxes);
    };
    for (tag_t solid : bodies)
    {
        struct EdgeList
        {
            uf_list_p_t value = nullptr;
            ~EdgeList() { if (value != nullptr) UF_MODL_delete_list(&value); }
        } edges;
        CheckPreviewUf(UF_MODL_ask_body_edges(solid, &edges.value));
        int count = 0;
        CheckPreviewUf(UF_MODL_ask_list_count(edges.value, &count));
        for (int index = 0; index < count; ++index)
        {
            tag_t edge = NULL_TAG;
            CheckPreviewUf(UF_MODL_ask_list_item(edges.value, index, &edge));
            struct Evaluator
            {
                UF_EVAL_p_t value = nullptr;
                ~Evaluator() { if (value != nullptr) UF_EVAL_free(value); }
            } evaluator;
            CheckPreviewUf(UF_EVAL_initialize(edge, &evaluator.value));
            double limits[2]{};
            CheckPreviewUf(UF_EVAL_ask_limits(evaluator.value, limits));
            logical straight = false;
            CheckPreviewUf(UF_EVAL_is_line(evaluator.value, &straight));
            const int steps = straight ? 1 : 32;
            std::array<double, 3> previous{};
            CheckPreviewUf(UF_EVAL_evaluate(evaluator.value, 0, limits[0], previous.data(), nullptr));
            for (int step = 1; step <= steps; ++step)
            {
                std::array<double, 3> point{};
                const double parameter = limits[0] + (limits[1] - limits[0]) * step / steps;
                CheckPreviewUf(UF_EVAL_evaluate(evaluator.value, 0, parameter, point.data(), nullptr));
                segments.push_back({previous, point});
                previous = point;
                if (segments.size() > kPreviewSegmentBudget)
                    return simplifiedOutline();
            }
        }
    }
    if (segments.empty()) throw std::runtime_error("标准件未包含可预览的实体轮廓；仍可点击确定插入。");
    return segments;
}

std::array<double, 3> TransformPreviewPoint(const std::array<double, 3>& point,
    const std::array<double, 3>& origin, const double matrix[9], double scale)
{
    std::array<double, 3> result{};
    for (int axis = 0; axis < 3; ++axis)
        result[axis] = origin[axis] + scale *
            (point[0] * matrix[axis] + point[1] * matrix[axis + 3] + point[2] * matrix[axis + 6]);
    return result;
}

std::vector<PreviewSegment> BuildPlacedPreview(const std::vector<PreviewSegment>& source,
    const std::vector<std::array<double, 3>>& origins, const double matrix[9],
    double scale, bool& simplified)
{
    if (source.empty() || origins.empty()) return {};
    const auto* outline = &source;
    std::vector<PreviewSegment> boxOutline;
    if (source.size() > kPreviewSegmentBudget / origins.size())
    {
        boxOutline = PreviewBoxWireframe({PreviewExtent(source)});
        outline = &boxOutline;
        simplified = true;
    }
    const bool overallOnly = outline->size() > kPreviewSegmentBudget / origins.size();
    const auto first = TransformPreviewPoint(outline->front().start, origins.front(), matrix, scale);
    PreviewBox overall{first[0], first[1], first[2], first[0], first[1], first[2]};
    std::vector<PreviewSegment> placed;
    if (!overallOnly) placed.reserve(outline->size() * origins.size());
    for (const auto& origin : origins)
        for (const auto& segment : *outline)
        {
            const auto start = TransformPreviewPoint(segment.start, origin, matrix, scale);
            const auto end = TransformPreviewPoint(segment.end, origin, matrix, scale);
            if (overallOnly)
            {
                IncludePreviewPoint(overall, start);
                IncludePreviewPoint(overall, end);
            }
            else placed.push_back({start, end});
        }
    return overallOnly ? PreviewBoxWireframe({overall}) : placed;
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

namespace QuickPositionIds
{
constexpr int Keep = 3101;
constexpr int Wcs = 3102;
constexpr int Absolute = 3103;
constexpr int RotateX = 3104;
constexpr int RotateY = 3105;
constexpr int RotateZ = 3106;
constexpr int LockX = 3107;
constexpr int LockY = 3108;
constexpr int LockZ = 3109;
constexpr int FlipX = 3110;
constexpr int FlipY = 3111;
constexpr int FlipZ = 3112;
constexpr int RotateAll = 3113;
}

struct QuickPositionContext
{
    AppState* state = nullptr;
    double originalOrigin[3]{};
    double originalMatrix[9]{};
    bool originalHasPlacement = false;
    bool committed = false;
};

void NotifyOrientationChanged(AppState* state)
{
    state->hasPlacement = true;
    UpdatePlacementStatus(state);
    if (state->orientationChanged != nullptr)
        state->orientationChanged(state->orientationContext);
}

void RotateFrame(double matrix[9], int axis, double degrees)
{
    if (!std::isfinite(degrees) || std::abs(degrees) < 1.0e-10) return;
    constexpr double pi = 3.14159265358979323846;
    const double radians = degrees * pi / 180.0;
    const double c = std::cos(radians);
    const double s = std::sin(radians);
    double old[9]{};
    std::copy(matrix, matrix + 9, old);
    if (axis == 0)
    {
        for (int i = 0; i < 3; ++i)
        {
            matrix[3 + i] = c * old[3 + i] + s * old[6 + i];
            matrix[6 + i] = -s * old[3 + i] + c * old[6 + i];
        }
    }
    else if (axis == 1)
    {
        for (int i = 0; i < 3; ++i)
        {
            matrix[i] = c * old[i] - s * old[6 + i];
            matrix[6 + i] = s * old[i] + c * old[6 + i];
        }
    }
    else
    {
        for (int i = 0; i < 3; ++i)
        {
            matrix[i] = c * old[i] + s * old[3 + i];
            matrix[3 + i] = -s * old[i] + c * old[3 + i];
        }
    }
}

double AngleValue(HWND window, int id)
{
    const std::wstring text = Trim(GetText(window, id));
    wchar_t* end = nullptr;
    const double value = wcstod(text.c_str(), &end);
    return end != text.c_str() && *end == L'\0' && std::isfinite(value)
        ? value : 0.0;
}

bool QuickAxisLocked(HWND window, int axis)
{
    return IsDlgButtonChecked(window, QuickPositionIds::LockX + axis) ==
           BST_CHECKED;
}

LRESULT CALLBACK QuickPositionWindowProc(HWND window, UINT message,
                                         WPARAM wParam, LPARAM lParam)
{
    auto* context = reinterpret_cast<QuickPositionContext*>(
        GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE)
    {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        context = static_cast<QuickPositionContext*>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(context));
    }
    if (message == WM_CREATE && context != nullptr)
    {
        AppState* state = context->state;
        const HFONT font = state->font;
        const auto add = [&](DWORD ex, const wchar_t* cls, const wchar_t* text,
                             DWORD style, int x, int y, int width, int height,
                             int id)
        {
            HWND control = CreateWindowExW(
                ex, cls, text, style | WS_CHILD | WS_VISIBLE, x, y, width,
                height, window,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                GetModuleHandleW(nullptr), nullptr);
            SendMessageW(control, WM_SETFONT,
                         reinterpret_cast<WPARAM>(font), TRUE);
        };
        add(0, L"BUTTON", L"保留已指定方位",
            BS_AUTORADIOBUTTON | WS_GROUP, 16, 16, 150, 24,
            QuickPositionIds::Keep);
        add(0, L"BUTTON", L"工作坐标系 WCS", BS_AUTORADIOBUTTON,
            170, 16, 138, 24, QuickPositionIds::Wcs);
        add(0, L"BUTTON", L"绝对坐标系 ABS", BS_AUTORADIOBUTTON,
            312, 16, 138, 24, QuickPositionIds::Absolute);
        CheckRadioButton(window, QuickPositionIds::Keep,
                         QuickPositionIds::Absolute, QuickPositionIds::Keep);
        add(0, L"BUTTON", L"修改参数", BS_GROUPBOX,
            14, 52, 440, 126, 0);
        const wchar_t* labels[] = {L"X旋转", L"Y旋转", L"Z旋转"};
        const int edits[] = {QuickPositionIds::RotateX,
                             QuickPositionIds::RotateY,
                             QuickPositionIds::RotateZ};
        const int locks[] = {QuickPositionIds::LockX,
                             QuickPositionIds::LockY,
                             QuickPositionIds::LockZ};
        for (int row = 0; row < 3; ++row)
        {
            const int y = 76 + row * 30;
            add(0, L"STATIC", labels[row], SS_LEFT, 28, y + 3, 52, 22, 0);
            add(WS_EX_CLIENTEDGE, L"EDIT", L"0", ES_AUTOHSCROLL,
                84, y, 94, 24, edits[row]);
            add(0, L"STATIC", L"度", SS_LEFT, 182, y + 3, 22, 22, 0);
            add(0, L"BUTTON", L"锁定", BS_AUTOCHECKBOX,
                216, y, 58, 24, locks[row]);
        }
        CheckDlgButton(window, QuickPositionIds::LockX,
                       state->axisLocked[0] ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(window, QuickPositionIds::LockY,
                       state->axisLocked[1] ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(window, QuickPositionIds::LockZ,
                       state->axisLocked[2] ? BST_CHECKED : BST_UNCHECKED);
        add(0, L"BUTTON", L"X翻转", BS_PUSHBUTTON,
            292, 76, 66, 26, QuickPositionIds::FlipX);
        add(0, L"BUTTON", L"Y翻转", BS_PUSHBUTTON,
            364, 76, 66, 26, QuickPositionIds::FlipY);
        add(0, L"BUTTON", L"Z翻转", BS_PUSHBUTTON,
            292, 108, 66, 26, QuickPositionIds::FlipZ);
        add(0, L"BUTTON", L"XYZ同时旋转", BS_PUSHBUTTON,
            364, 108, 66, 56, QuickPositionIds::RotateAll);
        add(0, L"STATIC",
            L"面中心、圆弧中心、任意点和点投影由主对话框的四个定位按钮指定。",
            SS_LEFT, 16, 188, 438, 38, 0);
        add(0, L"BUTTON", L"确定", BS_DEFPUSHBUTTON,
            292, 238, 74, 30, IDOK);
        add(0, L"BUTTON", L"取消", BS_PUSHBUTTON,
            374, 238, 74, 30, IDCANCEL);
        return 0;
    }
    if (message == WM_COMMAND && context != nullptr)
    {
        AppState* state = context->state;
        switch (LOWORD(wParam))
        {
        case QuickPositionIds::FlipX:
            if (!QuickAxisLocked(window, 0))
                RotateFrame(state->placementMatrix, 0, 180.0);
            NotifyOrientationChanged(state); return 0;
        case QuickPositionIds::FlipY:
            if (!QuickAxisLocked(window, 1))
                RotateFrame(state->placementMatrix, 1, 180.0);
            NotifyOrientationChanged(state); return 0;
        case QuickPositionIds::FlipZ:
            if (!QuickAxisLocked(window, 2))
                RotateFrame(state->placementMatrix, 2, 180.0);
            NotifyOrientationChanged(state); return 0;
        case QuickPositionIds::RotateAll:
            for (int axis = 0; axis < 3; ++axis)
            {
                if (!QuickAxisLocked(window, axis))
                    RotateFrame(state->placementMatrix, axis,
                                AngleValue(window, QuickPositionIds::RotateX + axis));
                SetText(window, QuickPositionIds::RotateX + axis, L"0");
            }
            NotifyOrientationChanged(state); return 0;
        case IDOK:
        {
            state->axisLocked[0] = IsDlgButtonChecked(
                window, QuickPositionIds::LockX) == BST_CHECKED;
            state->axisLocked[1] = IsDlgButtonChecked(
                window, QuickPositionIds::LockY) == BST_CHECKED;
            state->axisLocked[2] = IsDlgButtonChecked(
                window, QuickPositionIds::LockZ) == BST_CHECKED;
            if (IsDlgButtonChecked(window, QuickPositionIds::Wcs) == BST_CHECKED)
            {
                double wcsOrigin[3]{}, wcsMatrix[9]{};
                std::wstring error;
                if (!AskWcs(wcsOrigin, wcsMatrix, error))
                {
                    MessageBoxW(window, error.c_str(), kTitle,
                                MB_OK | MB_ICONERROR);
                    return 0;
                }
                if (!state->hasPlacement)
                    std::copy(wcsOrigin, wcsOrigin + 3, state->placementOrigin);
                std::copy(wcsMatrix, wcsMatrix + 9, state->placementMatrix);
            }
            else if (IsDlgButtonChecked(window, QuickPositionIds::Absolute) == BST_CHECKED)
            {
                const double identity[9] = {
                    1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0};
                std::copy(identity, identity + 9, state->placementMatrix);
            }
            for (int axis = 0; axis < 3; ++axis)
                if (!state->axisLocked[axis])
                    RotateFrame(state->placementMatrix, axis,
                                AngleValue(window, QuickPositionIds::RotateX + axis));
            context->committed = true;
            NotifyOrientationChanged(state);
            DestroyWindow(window);
            return 0;
        }
        case IDCANCEL: DestroyWindow(window); return 0;
        default: break;
        }
    }
    if (message == WM_CLOSE) { DestroyWindow(window); return 0; }
    if (message == WM_NCDESTROY && context != nullptr)
    {
        if (!context->committed)
        {
            std::copy(context->originalOrigin, context->originalOrigin + 3,
                      context->state->placementOrigin);
            std::copy(context->originalMatrix, context->originalMatrix + 9,
                      context->state->placementMatrix);
            context->state->hasPlacement = context->originalHasPlacement;
            UpdatePlacementStatus(context->state);
            if (context->state->orientationChanged != nullptr)
                context->state->orientationChanged(
                    context->state->orientationContext);
        }
        if (g_quickPositionWindow == window) g_quickPositionWindow = nullptr;
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        delete context;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

void ShowQuickPosition(AppState* state)
{
    if (g_quickPositionWindow != nullptr && IsWindow(g_quickPositionWindow))
    {
        ShowWindow(g_quickPositionWindow, SW_RESTORE);
        SetForegroundWindow(g_quickPositionWindow);
        return;
    }
    HMODULE module = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                          GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                      reinterpret_cast<LPCWSTR>(&ShowQuickPosition), &module);
    const wchar_t* className = L"ZhihuiStandardPartsQuickPositionWindow";
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = QuickPositionWindowProc;
    windowClass.hInstance = module;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    windowClass.lpszClassName = className;
    if (!RegisterOwnedWindowClass(windowClass, state->window)) return;
    auto* context = new (std::nothrow) QuickPositionContext();
    if (context == nullptr) return;
    context->state = state;
    std::copy(state->placementOrigin, state->placementOrigin + 3,
              context->originalOrigin);
    std::copy(state->placementMatrix, state->placementMatrix + 9,
              context->originalMatrix);
    context->originalHasPlacement = state->hasPlacement;
    RECT parent{};
    GetWindowRect(state->parent != nullptr ? state->parent : state->window,
                  &parent);
    HWND quick = CreateWindowExW(
        WS_EX_DLGMODALFRAME, className, L"快速定位 - 智辉标准件库",
        WS_CAPTION | WS_SYSMENU | WS_POPUP,
        parent.left + 90, parent.top + 110, 486, 326,
        state->parent != nullptr ? state->parent : state->window,
        nullptr, module, context);
    if (quick == nullptr) { delete context; return; }
    g_quickPositionWindow = quick;
    ShowWindow(quick, SW_SHOW);
    SetForegroundWindow(quick);
}

void CaptureManagedPreview(AppState* state, HWND owner);
void ShowParameterManager(AppState* state, bool forInsertion = false);
void ShowParameterDefinition(AppState*,const fs::path&,std::size_t,const std::wstring&,const std::wstring&,const std::wstring&);

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
    constexpr int parametersId = 2111;
    constexpr int defineId = 2112;
    constexpr int addFileId = 2113;
    try
    {
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
        const bool param = state->libraryFilter == 2;
        const auto folder = CurrentLibraryFolder(state).wstring();
        add(0, L"STATIC", folder.c_str(), SS_LEFT | SS_PATHELLIPSIS, 16, 64, 606, 24, 0);
        add(0, L"STATIC", param ? L"新建流程：选有参 PRT → 定义参数 → 创建零件族 → 管理参数另存规格。" : L"当前操作仅作用于本地无参。",
            SS_LEFT, 16, 92, 606, 22, 0);
        add(0, L"STATIC", L"分类", SS_LEFT, 16, 126, 38, 22, 0);
        add(WS_EX_CLIENTEDGE, L"EDIT", state->selectedCategory.empty() ? L"用户自定义" : state->selectedCategory.c_str(),
            ES_AUTOHSCROLL, 56, 122, 132, 25, categoryId);
        add(0, L"STATIC", L"零件族", SS_LEFT, 198, 126, 50, 22, 0);
        add(WS_EX_CLIENTEDGE, L"EDIT", L"", ES_AUTOHSCROLL, 250, 122, 150, 25, familyId);
        add(0, L"STATIC", L"规格", SS_LEFT, 410, 126, 38, 22, 0);
        add(WS_EX_CLIENTEDGE, L"EDIT", L"默认", ES_AUTOHSCROLL, 450, 122, 172, 25, specificationId);
        add(0, L"BUTTON", param ? L"用当前部件新建有参" : L"当前部件加入无参库",
            BS_PUSHBUTTON, 16, 160, 166, 38, param ? addParamId : addStaticId);
        add(0, L"BUTTON", L"抓取选中图档图片", BS_PUSHBUTTON, 192, 160, 166, 38, screenshotId);
        if (param) add(0, L"BUTTON", L"管理参数", BS_PUSHBUTTON, 368, 160, 112, 38, parametersId);
        if (param) add(0, L"BUTTON", L"定义参数", BS_PUSHBUTTON, 490, 160, 132, 38, defineId);
        add(0, L"BUTTON", L"打开当前库目录", BS_PUSHBUTTON, 16, 210, 166, 42,
            param ? openParamId : openStaticId);
        if (param) add(0,L"BUTTON",L"选择 PRT 新建有参",BS_PUSHBUTTON,192,210,166,42,addFileId);
        add(0, L"STATIC", L"同名图片自动复制，也可入库后抓图。",
            SS_LEFT, param?370:310, 215, param?250:310, 40, 0);
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
            if ((LOWORD(wParam) == openParamId) != (state->libraryFilter == 2)) return 0;
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
        case addFileId:
        {
            const bool parameterized=LOWORD(wParam)!=addStaticId;
            if (parameterized != (state->libraryFilter == 2)) return 0;
            const std::wstring current = LOWORD(wParam)==addFileId?BrowsePartFile(window):CurrentPartPath();
            if(current.empty()&&LOWORD(wParam)==addFileId)return 0;
            if (current.empty())
            {
                MessageBoxW(window, L"当前工作部件尚未保存，请先保存 PRT。", kTitle,
                            MB_OK | MB_ICONWARNING);
                return 0;
            }
            std::wstring family = Trim(GetText(window, familyId));
            if (family.empty()) family = fs::path(current).stem().wstring();
            if(parameterized)
            {
                if(fs::path(Trim(GetText(window,rootId))).lexically_normal()!=LibraryRoot(state).lexically_normal())
                    throw std::runtime_error(ToAnsi(L"库根目录已修改，请先按确定切换库，再新建有参标准件。"));
                ShowParameterDefinition(state,current,SIZE_MAX,Trim(GetText(window,categoryId)),family,Trim(GetText(window,specificationId)));
                return 0;
            }
            SetText(state->window, ID_ROOT, Trim(GetText(window, rootId)));
            SetText(state->window, ID_NAME, family);
            SetText(state->window, ID_EDIT_CATEGORY, Trim(GetText(window, categoryId)));
            SetText(state->window, ID_EDIT_SPEC, Trim(GetText(window, specificationId)));
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
            CaptureManagedPreview(state, window);
            return 0;
        }
        case parametersId: ShowParameterManager(state); return 0;
        case defineId:
        {
            const auto index=SelectedIndex(state);
            if(state->libraryFilter!=2||index>=state->items.size())throw std::runtime_error(ToAnsi(L"请先选择本地有参图档。"));
            std::wstring error;const auto model=ResolvedModel(state,state->items[index],error);
            if(model.empty())throw std::runtime_error(ToAnsi(error));
            const auto& item=state->items[index];ShowParameterDefinition(state,model,index,item.category,item.name,item.specification);return 0;
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
    }
    catch(const NXOpen::NXException& ex){UF_print_syslog(const_cast<char*>(ex.Message()),false);MessageBoxW(window,FromAnsi(ex.Message()).c_str(),L"图档管理",MB_OK|MB_ICONERROR);}
    catch(const std::exception& ex){UF_print_syslog(const_cast<char*>(ex.what()),false);MessageBoxW(window,FromAnsi(ex.what()).c_str(),L"图档管理",MB_OK|MB_ICONERROR);}
    catch(...){UF_print_syslog("[StandardPartsLibrary] Manager callback failed\n",false);}
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
    if (!RegisterOwnedWindowClass(windowClass, state->window)) return;
    RECT parentRect{};
    GetWindowRect(state->window, &parentRect);
    const auto title = std::wstring(CurrentLibraryName(state)) + L" - 图档管理";
    HWND manager = CreateWindowExW(WS_EX_DLGMODALFRAME, className,
        title.c_str(), WS_CAPTION | WS_SYSMENU | WS_POPUP,
        parentRect.left + 10, parentRect.top + 100, 654, 355,
        state->window, nullptr, module, state);
    if (manager == nullptr)
    {
        UF_print_syslog("[StandardPartsLibrary] Manager window creation failed\n", false);
        MessageBoxW(state->window, L"无法创建图档管理窗口，请关闭标准件库后重新打开。",
                    L"图档管理", MB_OK | MB_ICONERROR);
        return;
    }
    g_managerWindow = manager;
    ShowWindow(manager, SW_SHOW);
}

void BuildLegacyUi(AppState* state)
{
    state->font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    const std::wstring savedRoot = LoadRootPreference();
    AddControl(state, 0, L"BUTTON", L"全部", BS_PUSHBUTTON,
               12, 10, 66, 27, ID_FILTER_ALL);
    AddControl(state, 0, L"BUTTON", L"本地无参", BS_AUTORADIOBUTTON | BS_PUSHLIKE | WS_GROUP,
               80, 10, 82, 27, ID_FILTER_STATIC);
    AddControl(state, 0, L"BUTTON", L"本地有参", BS_AUTORADIOBUTTON | BS_PUSHLIKE,
               164, 10, 82, 27, ID_FILTER_PARAM);
    CheckRadioButton(state->window, ID_FILTER_STATIC, ID_FILTER_PARAM, ID_FILTER_STATIC);
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
    AddControl(state, 0, L"BUTTON", L"图档管理", BS_PUSHBUTTON, 14, 758, 88, 30, ID_MANAGE_CONFIG);
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

    AddControl(state, 0, L"BUTTON", L"全部", BS_PUSHBUTTON,
               10, 12, 72, 30, ID_FILTER_ALL);
    AddControl(state, 0, L"BUTTON", L"本地无参", BS_AUTORADIOBUTTON | BS_PUSHLIKE | WS_GROUP,
               10, 46, 72, 30, ID_FILTER_STATIC);
    AddControl(state, 0, L"BUTTON", L"本地有参", BS_AUTORADIOBUTTON | BS_PUSHLIKE,
               10, 80, 72, 30, ID_FILTER_PARAM);
    CheckRadioButton(state->window, ID_FILTER_STATIC, ID_FILTER_PARAM, ID_FILTER_STATIC);
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
                                375, 44, 229, 190, ID_PREVIEW);
    AddControl(state, 0, L"STATIC", L"规格", SS_LEFT, 379, 242, 54, 22, ID_SPEC_LABEL);
    AddControl(state, 0, L"BUTTON", L"选择参数", BS_PUSHBUTTON,
               375, 238, 64, 25, ID_INSERT_PARAMETERS);
    AddControl(state, 0, WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_VSCROLL,
               441, 238, 158, 200, ID_SPEC);
    AddControl(state, 0, L"STATIC", L"名称", SS_LEFT, 379, 270, 58, 22, ID_PARAM_LABEL1);
    AddControl(state, 0, L"STATIC", L"未选择", SS_LEFT | SS_PATHELLIPSIS,
               441, 270, 158, 22, ID_PARAM_VALUE1);
    AddControl(state, 0, L"STATIC", L"分类", SS_LEFT, 379, 294, 58, 22, ID_PARAM_LABEL2);
    AddControl(state, 0, L"STATIC", L"-", SS_LEFT | SS_PATHELLIPSIS,
               441, 294, 158, 22, ID_PARAM_VALUE2);
    AddControl(state, 0, L"STATIC", L"模型", SS_LEFT, 379, 318, 58, 22, ID_PARAM_LABEL3);
    AddControl(state, 0, L"STATIC", L"-", SS_LEFT | SS_PATHELLIPSIS,
               441, 318, 158, 22, ID_PARAM_VALUE3);
    AddControl(state, 0, L"STATIC", L"", SS_LEFT, 379, 342, 58, 22, ID_PARAM_LABEL4);
    AddControl(state, 0, L"STATIC", L"", SS_LEFT | SS_PATHELLIPSIS,
               441, 342, 158, 22, ID_PARAM_VALUE4);
    AddControl(state, 0, L"BUTTON", L"自动修剪", BS_AUTOCHECKBOX,
               378, 366, 96, 22, ID_AUTO_TRIM);
    AddControl(state, 0, L"STATIC", L"指定图层", SS_LEFT, 378, 393, 64, 22, 0);
    AddControl(state, WS_EX_CLIENTEDGE, L"EDIT", L"1", ES_NUMBER,
               446, 390, 54, 23, ID_LAYER);
    AddControl(state, 0, L"BUTTON", L"装配", BS_AUTORADIOBUTTON | WS_GROUP,
               378, 416, 58, 22, ID_MODE_ASSEMBLY);
    AddControl(state, 0, L"BUTTON", L"多实体", BS_AUTORADIOBUTTON,
               440, 416, 68, 22, ID_MODE_BODY);
    CheckRadioButton(state->window, ID_MODE_ASSEMBLY, ID_MODE_BODY, ID_MODE_ASSEMBLY);
    AddControl(state, 0, L"BUTTON", L"单个", BS_AUTORADIOBUTTON | WS_GROUP,
               378, 440, 52, 21, ID_PATTERN_SINGLE);
    AddControl(state, 0, L"BUTTON", L"X轴", BS_AUTORADIOBUTTON,
               430, 440, 44, 21, ID_PATTERN_X);
    AddControl(state, 0, L"BUTTON", L"Y轴", BS_AUTORADIOBUTTON,
               474, 440, 44, 21, ID_PATTERN_Y);
    AddControl(state, 0, L"BUTTON", L"对角", BS_AUTORADIOBUTTON,
               518, 440, 48, 21, ID_PATTERN_DIAGONAL);
    AddControl(state, 0, L"BUTTON", L"4角", BS_AUTORADIOBUTTON,
               378, 462, 52, 21, ID_PATTERN_FOUR);
    AddControl(state, 0, L"BUTTON", L"居中", BS_AUTORADIOBUTTON,
               430, 462, 52, 21, ID_PATTERN_CENTER);
    AddControl(state, 0, L"BUTTON", L"阵列", BS_AUTORADIOBUTTON,
               482, 462, 52, 21, ID_PATTERN_ARRAY);
    CheckRadioButton(state->window, ID_PATTERN_SINGLE, ID_PATTERN_ARRAY,
                     ID_PATTERN_SINGLE);
    AddControl(state, 0, L"STATIC", L"X距", SS_LEFT, 378, 489, 24, 20, 0);
    AddControl(state, WS_EX_CLIENTEDGE, L"EDIT", L"100", ES_AUTOHSCROLL,
               402, 486, 34, 22, ID_PATTERN_SPACING_X);
    AddControl(state, 0, L"STATIC", L"Y距", SS_LEFT, 438, 489, 24, 20, 0);
    AddControl(state, WS_EX_CLIENTEDGE, L"EDIT", L"100", ES_AUTOHSCROLL,
               462, 486, 34, 22, ID_PATTERN_SPACING_Y);
    AddControl(state, 0, L"STATIC", L"X数", SS_LEFT, 498, 489, 24, 20, 0);
    AddControl(state, WS_EX_CLIENTEDGE, L"EDIT", L"2", ES_NUMBER,
               522, 486, 30, 22, ID_PATTERN_COUNT_X);
    AddControl(state, 0, L"STATIC", L"Y数", SS_LEFT, 554, 489, 24, 20, 0);
    AddControl(state, WS_EX_CLIENTEDGE, L"EDIT", L"2", ES_NUMBER,
               578, 486, 26, 22, ID_PATTERN_COUNT_Y);
    AddControl(state, 0, L"BUTTON", L"图档管理", BS_PUSHBUTTON,
               10, 488, 72, 30, ID_MANAGE_CONFIG);
    AddControl(state, 0, L"BUTTON", L"选择体\r\n加入库", BS_PUSHBUTTON | BS_MULTILINE,
               10, 424, 72, 56, ID_CAPTURE_BODIES);

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

bool RemoveLibraryItem(AppState* state, std::size_t index, fs::path& backup,
    std::wstring& error)
{
    if (index >= state->items.size() || !InCurrentLibrary(state, state->items[index]))
    { error = L"请先选中当前库内要删除的标准件。"; return false; }
    const LibraryItem item = state->items[index];
    const fs::path root = fs::weakly_canonical(LibraryRoot(state));
    const std::wstring prefix = Lower(root.wstring() + L"\\");
    const auto withinLibrary = [&](const fs::path& path)
    {
        return Lower(fs::weakly_canonical(path).wstring()).rfind(prefix, 0) == 0;
    };
    const fs::path model = fs::weakly_canonical(root / item.relativePath);
    if (!withinLibrary(model) || !IsPartFile(model))
    {
        error = L"模型路径不在标准件库内，已拒绝删除。";
        return false;
    }
    // Other specifications may deliberately share the same model file.
    bool shared = false;
    for (std::size_t other = 0; other < state->items.size(); ++other)
        if (other != index && Lower(fs::weakly_canonical(root / state->items[other].relativePath).wstring()) == Lower(model.wstring()))
            shared = true;
    std::vector<fs::path> files;
    if (!shared)
    {
        if (fs::exists(model)) files.push_back(model);
        for (const auto* extension : {L".png", L".jpg", L".jpeg", L".bmp"})
        {
            fs::path preview = model;
            preview.replace_extension(extension);
            if (fs::exists(preview)) files.push_back(preview);
        }
        for (const auto& file : files)
            if (!withinLibrary(file) || !fs::is_regular_file(file))
            {
                error = L"模型或配图路径异常，已拒绝删除。";
                return false;
            }
    }
    backup.clear();
    std::vector<std::pair<fs::path, fs::path>> moved;
    const auto restore = [&]()
    {
        for (auto it = moved.rbegin(); it != moved.rend(); ++it)
        {
            std::error_code ec;
            fs::rename(it->second, it->first, ec);
            if (ec) error += L"\r\n恢复失败，请从备份恢复：" + it->second.wstring();
        }
    };
    if (!files.empty())
    {
        SYSTEMTIME now{};
        GetLocalTime(&now);
        wchar_t stamp[64]{};
        swprintf_s(stamp, L"deleted_%04u%02u%02u_%02u%02u%02u_%03u_%llu",
            now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond,
            now.wMilliseconds, GetTickCount64());
        backup = root / L"backup" / stamp;
        if (!withinLibrary(backup)) { error = L"备份目录不在库内，已停止删除。"; return false; }
        if (!fs::create_directories(backup)) { error = L"无法创建独立删除备份目录。"; return false; }
        if (!WriteUtf16File(backup / L"restore.tsv",
            L"# id\tname\tcategory\trelative_model_path\tspecification\tparameterized\r\n" +
            item.id + L"\t" + item.name + L"\t" + item.category + L"\t" + item.relativePath +
            L"\t" + item.specification + L"\t" + (item.parameterized ? L"1" : L"0") + L"\r\n"))
        { error = L"无法写入删除恢复记录，已停止删除。"; return false; }
        try
        {
            moved.reserve(files.size());
            for (const auto& file : files)
            {
                const fs::path destination = backup / file.filename();
                // Record before moving so allocation failure cannot strand a file.
                moved.emplace_back(file, destination);
                std::error_code ec;
                fs::rename(file, destination, ec);
                if (ec)
                {
                    moved.pop_back();
                    error = L"无法移走库内文件（可能正在使用）：" + file.wstring();
                    restore();
                    return false;
                }
            }
        }
        catch (...) { error = L"移走文件失败。"; restore(); return false; }
    }
    state->items.erase(state->items.begin() + static_cast<std::ptrdiff_t>(index));
    bool saved = false;
    try { saved = SaveIndex(state, error); }
    catch (...) { error = L"保存删除后的索引失败。"; }
    if (!saved)
    {
        state->items.insert(state->items.begin() + static_cast<std::ptrdiff_t>(index), item);
        restore();
        return false;
    }
    return true;
}

void DeleteSelected(AppState* state, std::size_t index = SIZE_MAX)
{
    if (index == SIZE_MAX) index = SelectedIndex(state);
    if (index >= state->items.size()) { SetStatus(state, L"请先选中要删除的标准件。"); return; }
    const LibraryItem item = state->items[index];
    const std::wstring prompt = L"确定删除当前规格？\r\n标准件：" + item.name +
        L"\r\n规格：" + item.specification + L"\r\n文件：" + item.relativePath +
        L"\r\n\r\n仅删除当前规格；未被其他规格共用的库内模型和同名配图会移到库内 backup 目录，可恢复。";
    if (MessageBoxW(state->window, prompt.c_str(), kTitle,
                    MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) return;
    std::wstring error;
    fs::path backup;
    try
    {
        if (!RemoveLibraryItem(state, index, backup, error))
        {
            MessageBoxW(state->window, error.c_str(), kTitle, MB_OK | MB_ICONERROR);
            return;
        }
    }
    catch (const std::exception& ex)
    {
        MessageBoxW(state->window, FromAnsi(ex.what()).c_str(), kTitle, MB_OK | MB_ICONERROR);
        return;
    }
    RefreshCategories(state);
    RefreshList(state);
    SetStatus(state, L"已删除当前规格：" + item.name + L" / " + item.specification +
        (backup.empty() ? L"" : L"；可从备份恢复：" + backup.wstring()));
}

#include "StandardPartsCapture.inc"

std::size_t SelectContextRow(AppState* state, int row)
{
    if (row < 0 || static_cast<std::size_t>(row) >= state->visible.size()) return SIZE_MAX;
    if ((ListView_GetItemState(state->list, row, LVIS_SELECTED) & LVIS_SELECTED) == 0)
    {
        // Refresh specifications exactly once after selecting the clicked row.
        state->refreshingList = true;
        ListView_SetItemState(state->list, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
        ListView_SetItemState(state->list, row, LVIS_SELECTED | LVIS_FOCUSED,
            LVIS_SELECTED | LVIS_FOCUSED);
        state->refreshingList = false;
        RefreshSpecifications(state);
        UpdatePreview(state);
    }
    return SelectedIndex(state);
}

#include "StandardPartsParameterManager.inc"

void OpenLibraryPartForEdit(const fs::path& model)
{
    const std::string name = ToAnsi(model.wstring());
    tag_t part = UF_PART_ask_part_tag(name.c_str());
    if (part == NULL_TAG || UF_PART_is_loaded(name.c_str()) != 1)
    {
        UF_PART_load_status_t status{};
        const int code = UF_PART_open_quiet(name.c_str(), &part, &status);
        UF_PART_free_load_status(&status);
        CheckPreviewUf(code);
    }
    if (part == NULL_TAG) throw std::runtime_error("标准件 PRT 打开失败。");
    // Do not close/save the existing work part or reopen an already-loaded model.
    CheckPreviewUf(UF_PART_set_display_part(part));
    CheckPreviewUf(UF_ASSEM_set_work_part(part));
}

void PostLibraryDialogCancel(AppState* state)
{
    // The captioned NX window is a wrapper. Its WM_CLOSE does not dispatch
    // Block Styler Cancel. Route the real navigation button's notification to
    // its owning dialog, asynchronously so the browser callback can return.
    struct Search { HWND pane; HWND button = nullptr; int count = 0; } search{state->window};
    EnumChildWindows(state->parent, [](HWND child, LPARAM parameter) -> BOOL
    {
        auto* result = reinterpret_cast<Search*>(parameter);
        wchar_t className[64]{};
        GetClassNameW(child, className, 64);
        if (GetDlgCtrlID(child) == IDCANCEL && _wcsicmp(className, L"Button") == 0 &&
            !IsChild(result->pane, child) && IsWindowVisible(child) && IsWindowEnabled(child))
        {
            result->button = child;
            ++result->count;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&search));
    if (search.count != 1 || !PostMessageW(GetParent(search.button), WM_COMMAND,
        MAKEWPARAM(IDCANCEL, BN_CLICKED), reinterpret_cast<LPARAM>(search.button)))
        throw std::runtime_error("无法找到标准件库的取消控件，未启动窗口切换。");
    UF_print_syslog("[StandardPartsLibrary] Posted native navigation Cancel for dialog transition\n", false);
}

void RequestLibraryPartEdit(AppState* state, std::size_t index)
{
    if (index >= state->items.size()) return;
    std::wstring error;
    const fs::path model = ResolvedModel(state, state->items[index], error);
    if (model.empty())
    {
        MessageBoxW(state->window, error.c_str(), kTitle, MB_OK | MB_ICONERROR);
        return;
    }
    if (!state->embedded) { OpenLibraryPartForEdit(model); return; }
    // Close only our verified Block Styler parent. Opening another work part
    // inside a modal selection callback would leave stale NX selections alive.
    DWORD process = 0;
    wchar_t title[256]{};
    GetWindowThreadProcessId(state->parent, &process);
    GetWindowTextW(state->parent, title, 256);
    if (!IsWindow(state->parent) || !IsChild(state->parent, state->window) ||
        process != GetCurrentProcessId() || wcscmp(title, kTitle) != 0)
        throw std::runtime_error("无法安全关闭标准件库对话框，未打开编辑文件。");
    state->requestedEditModel = model;
    try { PostLibraryDialogCancel(state); }
    catch (...)
    {
        state->requestedEditModel.clear();
        throw;
    }
}

void RequestBodyCapture(AppState* state)
{
    DWORD process = 0;
    wchar_t title[256]{};
    GetWindowThreadProcessId(state->parent, &process);
    GetWindowTextW(state->parent, title, 256);
    if (!state->embedded || !IsWindow(state->parent) || !IsChild(state->parent, state->window) ||
        process != GetCurrentProcessId() || wcscmp(title, kTitle) != 0)
        throw std::runtime_error("无法安全结束标准件库对话框，未启动选择体入库。");
    state->requestedCaptureRoot = fs::weakly_canonical(LibraryRoot(state));
    state->requestedCaptureCategory = state->selectedCategory;
    try { PostLibraryDialogCancel(state); }
    catch (...)
    {
        state->requestedCaptureRoot.clear();
        state->requestedCaptureCategory.clear();
        throw;
    }
}

void ShowStandardPartContextMenu(AppState* state, POINT screenPoint)
{
    int row = -1;
    if (screenPoint.x == -1 && screenPoint.y == -1)
    {
        row = ListView_GetNextItem(state->list, -1, LVNI_SELECTED);
        RECT bounds{};
        if (row < 0 || !ListView_GetItemRect(state->list, row, &bounds, LVIR_BOUNDS)) return;
        screenPoint = {bounds.left + 8, bounds.bottom};
        ClientToScreen(state->list, &screenPoint);
    }
    else
    {
        LVHITTESTINFO hit{};
        hit.pt = screenPoint;
        ScreenToClient(state->list, &hit.pt);
        row = ListView_HitTest(state->list, &hit);
    }
    const std::size_t index = SelectContextRow(state, row);
    if (index == SIZE_MAX) return; // Right-clicking empty space must not delete the old selection.
    const HMENU menu = CreatePopupMenu();
    if (menu == nullptr) return;
    AppendMenuW(menu, MF_STRING, ID_EDIT_MODEL, L"编辑标准件 PRT");
    AppendMenuW(menu, MF_STRING, ID_REPLACE_PREVIEW, L"更换预览图片...");
    AppendMenuW(menu, MF_STRING, ID_CAPTURE_PREVIEW, L"抓图作为预览...");
    if (state->items[index].parameterized)
    {
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, ID_INSERT_PARAMETERS, L"参数组 / 选择尺寸...");
        AppendMenuW(menu, MF_STRING, ID_DEFINE_PARAMETERS, L"修改参数设定...");
    }
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_DELETE, L"删除当前规格...");
    const UINT command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
        screenPoint.x, screenPoint.y, 0, state->window, nullptr);
    DestroyMenu(menu);
    if (command == ID_EDIT_MODEL) RequestLibraryPartEdit(state, index);
    if (command == ID_REPLACE_PREVIEW) BrowseManagedPreview(state, state->window);
    if (command == ID_CAPTURE_PREVIEW) CaptureManagedPreview(state, state->window);
    if (command == ID_INSERT_PARAMETERS) ShowParameterManager(state, true);
    if (command == ID_DEFINE_PARAMETERS)
    {
        std::wstring error;
        const auto model = ResolvedModel(state, state->items[index], error);
        if (model.empty()) throw std::runtime_error(ToAnsi(error));
        const auto item = state->items[index];
        ShowParameterDefinition(state, model, index, item.category, item.name, item.specification);
    }
    if (command == ID_DELETE) DeleteSelected(state, index);
}

bool InsertSelected(AppState* state)
{
    const std::size_t index = SelectedIndex(state);
    if (index == SIZE_MAX)
    {
        MessageBoxW(state->window, L"请先选中标准件及规格，再指定插入点。",
                    kTitle, MB_OK | MB_ICONINFORMATION);
        return false;
    }
    if (!state->hasPlacement)
    {
        MessageBoxW(state->window, L"请先指定标准件插入点。", kTitle,
                    MB_OK | MB_ICONINFORMATION);
        return false;
    }
    std::wstring error;
    NXOpen::Session* session = nullptr;
    NXOpen::Session::UndoMarkId mark =
        static_cast<NXOpen::Session::UndoMarkId>(0);
    bool hasMark = false;
    try
    {
        session = NXOpen::Session::GetSession();
        if (session == nullptr)
            throw std::runtime_error("NX session is unavailable; insertion was not started.");
        if (session != nullptr)
        {
            mark = session->SetUndoMark(
                NXOpen::Session::MarkVisibilityVisible,
                "调用智辉标准件");
            hasMark = true;
        }
    }
    catch (const NXOpen::NXException& ex)
    {
        std::string log = "[StandardPartsLibrary] Undo mark failed: " + std::string(ex.Message()) + "\n";
        UF_print_syslog(log.data(), false);
        MessageBoxW(state->window, (L"无法创建撤销事务：" + FromAnsi(ex.Message())).c_str(),
                    kTitle, MB_OK | MB_ICONERROR);
        return false;
    }
    catch (const std::exception& ex)
    {
        MessageBoxW(state->window, (L"无法创建撤销事务：" + FromAnsi(ex.what())).c_str(),
                    kTitle, MB_OK | MB_ICONERROR);
        return false;
    }
    const bool assembly = IsDlgButtonChecked(state->window, ID_MODE_ASSEMBLY) == BST_CHECKED;
    bool ok = false;
    try
    {
        ok = assembly ? InsertAssembly(state, state->items[index], error)
                      : InsertBodies(state, state->items[index], error);
    }
    catch (const NXOpen::NXException& ex) { error = FromAnsi(ex.Message()); }
    catch (const std::exception& ex) { error = FromAnsi(ex.what()); }
    catch (...) { error = L"创建标准件时发生未知错误。"; }
    if (ok)
    {
        const auto count = PatternOrigins(
            state, state->placementOrigin, state->placementMatrix).size();
        SetStatus(state, L"已" + std::wstring(assembly ? L"装配" : L"合并") +
                         std::to_wstring(count) + L" 个标准件：" +
                         state->items[index].name);
    }
    else
    {
        if (hasMark && session != nullptr)
        {
            try
            {
                session->UndoToMark(mark, "调用智辉标准件");
                session->DeleteUndoMark(mark, "调用智辉标准件");
            }
            catch (...)
            {
                error += L"\r\n本次回滚未完成，请使用 NX 撤销并检查模型。";
            }
        }
        std::string log = "[StandardPartsLibrary] Insert failed: " + ToAnsi(error) + "\n";
        UF_print_syslog(log.data(), false);
        MessageBoxW(state->window, error.c_str(), kTitle, MB_OK | MB_ICONERROR);
    }
    return ok;
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
        SelectLibrary(state, state->libraryFilter);
        if (!state->resumeItemId.empty())
            for (std::size_t i = 0; i < state->items.size(); ++i)
                if (state->items[i].id == state->resumeItemId)
                { SelectLibrarySpecification(state, i, state->resumeGroupName); break; }
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
        case ID_CAPTURE_BODIES:
            if (state->libraryFilter == 2) { ShowManagerConfig(state); return 0; }
            try { RequestBodyCapture(state); }
            catch (const NXOpen::NXException& ex)
            {
                UF_print_syslog(const_cast<char*>(ex.Message()), false);
                MessageBoxW(window, FromAnsi(ex.Message()).c_str(), kTitle, MB_OK | MB_ICONERROR);
            }
            catch (const std::exception& ex) { MessageBoxW(window, FromAnsi(ex.what()).c_str(), kTitle, MB_OK | MB_ICONERROR); }
            catch (...) { MessageBoxW(window, L"启动选择体入库失败。", kTitle, MB_OK | MB_ICONERROR); }
            return 0;
        case ID_INSERT: InsertSelected(state); return 0;
        case ID_FILTER_ALL:
            state->selectedCategory.clear();
            SetText(state->window, ID_SEARCH, L"");
            RefreshCategories(state);
            RefreshList(state);
            return 0;
        case ID_FILTER_STATIC:
        case ID_FILTER_PARAM:
            if (HIWORD(wParam) == BN_CLICKED)
            {
                SelectLibrary(state, LOWORD(wParam) == ID_FILTER_PARAM ? 2 : 1);
            }
            return 0;
        case ID_PATTERN_SINGLE:
        case ID_PATTERN_X:
        case ID_PATTERN_Y:
        case ID_PATTERN_DIAGONAL:
        case ID_PATTERN_FOUR:
        case ID_PATTERN_CENTER:
        case ID_PATTERN_ARRAY:
            if (HIWORD(wParam) == BN_CLICKED)
            {
                state->patternMode = LOWORD(wParam) - ID_PATTERN_SINGLE;
                CheckRadioButton(state->window, ID_PATTERN_SINGLE,
                                 ID_PATTERN_ARRAY, LOWORD(wParam));
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
        case ID_INSERT_PARAMETERS: ShowParameterManager(state, true); return 0;
        case ID_SPEC:
            if (HIWORD(wParam) == CBN_SELCHANGE) HandleSpecificationSelection(state);
            return 0;
        case ID_SEARCH:
            if (HIWORD(wParam) == EN_CHANGE) RefreshList(state);
            return 0;
        default: break;
        }
        break;
    case WM_CONTEXTMENU:
        if (state != nullptr && reinterpret_cast<HWND>(wParam) == state->list)
        {
            try
            {
                ShowStandardPartContextMenu(state,
                    {static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam))});
            }
            catch (const NXOpen::NXException& ex)
            {
                UF_print_syslog(const_cast<char*>(ex.Message()), false);
                MessageBoxW(window, FromAnsi(ex.Message()).c_str(), kTitle, MB_OK | MB_ICONERROR);
            }
            catch (const std::exception& ex)
            {
                MessageBoxW(window, FromAnsi(ex.what()).c_str(), kTitle, MB_OK | MB_ICONERROR);
            }
            catch (...) { MessageBoxW(window, L"标准件右键操作失败。", kTitle, MB_OK | MB_ICONERROR); }
            return 0;
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
            if (g_parameterWindow != nullptr && IsWindow(g_parameterWindow))
                DestroyWindow(g_parameterWindow);
            if (g_quickPositionWindow != nullptr && IsWindow(g_quickPositionWindow))
                DestroyWindow(g_quickPositionWindow);
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
    StandardPartsDialogHost(const std::shared_ptr<ParameterSaveRequest>& resume = {})
        : ui_(NXOpen::UI::GetUI()),
          dialog_(ui_->CreateDialog(DialogFilePath().c_str())), resume_(resume)
    {
        if (resume)
        {
            state_.libraryRootOverride = resume->root;
            state_.libraryFilter = 2;
            state_.resumeItemId = resume->savedItemId.empty() ? resume->itemId : resume->savedItemId;
            state_.resumeGroupName = resume->savedItemId.empty() ? L"" : resume->groupName;
        }
        state_.embedded = true;
        state_.orientationContext = this;
        state_.orientationChanged = &StandardPartsDialogHost::OrientationChangedThunk;
        state_.capturePreview = &StandardPartsDialogHost::CapturePreviewThunk;
        state_.prepareParameterSave = &StandardPartsDialogHost::PrepareParameterSaveThunk;
        dialog_->AddInitializeHandler(NXOpen::make_callback(
            this, &StandardPartsDialogHost::Initialize));
        dialog_->AddDialogShownHandler(NXOpen::make_callback(
            this, &StandardPartsDialogHost::DialogShown));
        dialog_->AddUpdateHandler(NXOpen::make_callback(
            this, &StandardPartsDialogHost::Update));
        dialog_->AddEnableOKButtonHandler(NXOpen::make_callback(
            this, &StandardPartsDialogHost::EnableOK));
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
        try { ClearPlacementPreview(); }
        catch (...) { UF_print_syslog("[StandardPartsLibrary] Preview display cleanup failed\n", false); }
        if (pendingTimerHost_ == this) pendingTimerHost_ = nullptr;
        if (g_quickPositionWindow != nullptr &&
            IsWindow(g_quickPositionWindow))
            DestroyWindow(g_quickPositionWindow);
        state_.orientationChanged = nullptr;
        state_.capturePreview = nullptr;
        state_.prepareParameterSave = nullptr;
        state_.orientationContext = nullptr;
        if (pane_ != nullptr && IsWindow(pane_)) DestroyWindow(pane_);
        UnregisterOwnedWindowClasses(module_);
        if (!paneClass_.empty() && module_ != nullptr)
            UnregisterClassW(paneClass_.c_str(), module_);
        if (g_appState == &state_) g_appState = nullptr;
        delete dialog_;
    }

    int Launch()
    {
        return static_cast<int>(dialog_->Launch());
    }

    fs::path RequestedEditModel() const { return state_.requestedEditModel; }
    fs::path RequestedCaptureRoot() const { return state_.requestedCaptureRoot; }
    std::wstring RequestedCaptureCategory() const { return state_.requestedCaptureCategory; }
    std::shared_ptr<ParameterSaveRequest> RequestedParameterSave() const { return state_.requestedParameterSave; }

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
    std::shared_ptr<ParameterSaveRequest> resume_;
    HWND pane_ = nullptr;
    HMODULE module_ = nullptr;
    std::wstring paneClass_;
    bool updating_ = false;
    HWND timerWindow_ = nullptr;
    UINT_PTR timerId_ = 0;
    int paneAttempts_ = 0;
    int pendingPlacementMode_ = -1;
    bool pendingQuickOrient_ = false;
    bool shown_ = false;
    bool insertedAny_ = false;
    bool closing_ = false;
    std::wstring previewKey_;
    fs::path previewModel_;
    int previewUnits_ = 1;
    bool sourcePreviewSimplified_ = false;
    std::vector<PreviewSegment> sourceWireframe_;
    std::vector<PreviewSegment> placedWireframe_;
    bool previewVisible_ = false;
    ULONGLONG lastPreviewDraw_ = 0;
    static StandardPartsDialogHost* pendingTimerHost_;

    static void PrepareParameterSaveThunk(void* context, ParameterSaveRequest& request)
    {
        auto* self = static_cast<StandardPartsDialogHost*>(context);
        if (!self || !self->shown_ || self->closing_ || self->updating_)
            throw std::runtime_error(ToAnsi(L"定位界面正在更新，请稍后选择参数。"));
        // Called by the exception-guarded parameter window on the NX UI thread.
        if (self->state_.hasPlacement) self->ReadOrientationBlock();
        self->ReadTrimTargets();
        const auto& state = self->state_;
        auto& saved = request.placement;
        saved.workPart = UF_ASSEM_ask_work_part();
        saved.hasPlacement = state.hasPlacement;
        saved.mode = state.placementMode;
        saved.pattern = state.patternMode;
        std::copy(std::begin(state.placementOrigin),std::end(state.placementOrigin),saved.origin);
        std::copy(std::begin(state.placementMatrix),std::end(state.placementMatrix),saved.matrix);
        std::copy(std::begin(state.axisLocked),std::end(state.axisLocked),saved.axisLocked);
        for (auto* object : self->placementSelection_->GetSelectedObjects())
            if (object) saved.selection.push_back(object->Tag());
        saved.trim = state.trimTargets;
        saved.assembly = IsDlgButtonChecked(state.window,ID_MODE_ASSEMBLY) == BST_CHECKED;
        saved.autoTrim = IsDlgButtonChecked(state.window,ID_AUTO_TRIM) == BST_CHECKED;
        saved.layer = GetText(state.window,ID_LAYER);
        saved.spacingX = GetText(state.window,ID_PATTERN_SPACING_X);
        saved.spacingY = GetText(state.window,ID_PATTERN_SPACING_Y);
        saved.countX = GetText(state.window,ID_PATTERN_COUNT_X);
        saved.countY = GetText(state.window,ID_PATTERN_COUNT_Y);
        saved.captured = true;
    }

    void RestoreParameterPlacement()
    {
        const auto resume = std::move(resume_);
        if (!resume || !resume->placement.captured ||
            resume->placement.workPart != UF_ASSEM_ask_work_part()) return;
        const auto& saved = resume->placement;
        // The pane is created only after dialogShown. Suppress recursive NX
        // updates while restoring native selections and the orientation block.
        struct RestoreScope { bool& flag; ~RestoreScope() { flag = false; } } scope{updating_};
        updating_ = true;
        const auto objects = [](const std::vector<tag_t>& tags)
        {
            std::vector<NXOpen::TaggedObject*> result;
            for (const auto tag : tags)
                if (tag != NULL_TAG && UF_OBJ_ask_status(tag) == UF_OBJ_ALIVE)
                    if (auto* object = NXOpen::NXObjectManager::Get(tag)) result.push_back(object);
            return result;
        };
        SetPlacementMode(&state_,saved.mode);
        ActivatePlacementSelection();
        placementSelection_->SetSelectedObjects(objects(saved.selection));
        trimSelection_->SetSelectedObjects(objects(saved.trim));
        state_.patternMode = saved.pattern;
        CheckRadioButton(state_.window,ID_PATTERN_SINGLE,ID_PATTERN_ARRAY,ID_PATTERN_SINGLE+saved.pattern);
        CheckRadioButton(state_.window,ID_MODE_ASSEMBLY,ID_MODE_BODY,saved.assembly ? ID_MODE_ASSEMBLY : ID_MODE_BODY);
        CheckDlgButton(state_.window,ID_AUTO_TRIM,saved.autoTrim ? BST_CHECKED : BST_UNCHECKED);
        SetText(state_.window,ID_LAYER,saved.layer);
        SetText(state_.window,ID_PATTERN_SPACING_X,saved.spacingX);
        SetText(state_.window,ID_PATTERN_SPACING_Y,saved.spacingY);
        SetText(state_.window,ID_PATTERN_COUNT_X,saved.countX);
        SetText(state_.window,ID_PATTERN_COUNT_Y,saved.countY);
        std::copy(std::begin(saved.origin),std::end(saved.origin),state_.placementOrigin);
        std::copy(std::begin(saved.matrix),std::end(saved.matrix),state_.placementMatrix);
        std::copy(std::begin(saved.axisLocked),std::end(saved.axisLocked),state_.axisLocked);
        state_.hasPlacement = saved.hasPlacement;
        SyncOrientationBlock();
        UpdatePlacementStatus(&state_);
    }

    static void OrientationChangedThunk(void* context) noexcept
    {
        auto* self = static_cast<StandardPartsDialogHost*>(context);
        if (self == nullptr) return;
        try
        {
            if (self->state_.hasPlacement)
                self->SyncOrientationBlock();
        }
        catch (const NXOpen::NXException& ex)
        {
            self->ShowError(ex.Message());
        }
        catch (const std::exception& ex)
        {
            self->ShowError(ex.what());
        }
        catch (...)
        {
            self->ShowError("同步标准件方位时发生未知错误。");
        }
    }

    static void CapturePreviewThunk(void* context, CapturePreviewImage& image)
    {
        auto* self = static_cast<StandardPartsDialogHost*>(context);
        if (!self || !self->shown_ || self->closing_ || self->updating_)
            throw std::runtime_error(ToAnsi(L"当前对话框正在更新，请稍后抓图。"));
        struct CaptureUiScope
        {
            StandardPartsDialogHost* host;
            bool visible;
            ~CaptureUiScope()
            {
                try { host->orientation_->SetShow(visible); }
                catch (const NXOpen::NXException& ex)
                { UF_print_syslog(const_cast<char*>(ex.Message()), false); }
                catch (...) { UF_print_syslog("[StandardPartsLibrary] Preview orientation restore failed\n", false); }
                host->updating_ = false;
            }
        } scope{self, self->orientation_->Show()};
        self->updating_ = true;
        self->ClearPlacementPreview();
        self->orientation_->SetShow(false);
        std::vector<tag_t> highlights;
        for (auto* selection : {self->placementSelection_, self->trimSelection_, self->orientationSelection_})
            if (selection)
                for (auto* object : selection->GetSelectedObjects())
                    if (object) highlights.push_back(object->Tag());
        image.Capture(highlights);
    }

    static void CALLBACK BrowserTimerProc(HWND window, UINT, UINT_PTR timerId,
                                          DWORD)
    {
        auto* self = pendingTimerHost_;
        if (self == nullptr || self->updating_ || self->closing_) return;
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
                self->RefreshPlacementPreview();
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

        const bool wasUpdating = updating_;
        updating_ = true;
        try
        {
            placementSelection_->SetSelectedObjects({});
            placementSelection_->SetSelectionFilter(
                NXOpen::Selection::SelectionActionClearAndEnableSpecific, masks);
            placementSelection_->SetSelectModeAsString("Single");
            placementSelection_->SetAutomaticProgression(false);
            placementSelection_->SetPointOverlay(true);
            placementSelection_->Focus();
        }
        catch (...)
        {
            updating_ = wasUpdating;
            throw;
        }
        updating_ = wasUpdating;
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
        state_.hasPlacement = false;
        const std::vector<NXOpen::TaggedObject*> objects =
            placementSelection_->GetSelectedObjects();
        // Clearing selection is not a new point. PickPoint may still contain
        // the last cursor location (or zero) after NX deselects an object.
        if (objects.empty() || objects.front() == nullptr)
        {
            UpdatePlacementStatus(&state_);
            return;
        }
        const NXOpen::Point3d picked = placementSelection_->PickPoint();
        std::wstring error;

        if (state_.placementMode == 1)
        {
            state_.placementOrigin[0] = picked.X;
            state_.placementOrigin[1] = picked.Y;
            state_.placementOrigin[2] = picked.Z;
            int type = 0, subtype = 0;
            if (UF_OBJ_ask_type_and_subtype(objects.front()->Tag(), &type, &subtype) == 0 &&
                type == UF_point_type)
                UF_CURVE_ask_point_data(objects.front()->Tag(), state_.placementOrigin);
            double unusedOrigin[3]{};
            state_.hasPlacement = AskWcs(unusedOrigin,
                                          state_.placementMatrix, error);
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
        // Keep the selected point until the user presses OK/Apply. Creating
        // geometry and clearing mandatory input here made confirmation gray.
    }

    void ShowError(const std::string& message) noexcept
    {
        try
        {
            std::string log = "[StandardPartsLibrary] " + message + "\n";
            UF_print_syslog(log.data(), false);
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
                placementSelection_ == nullptr || trimSelection_ == nullptr ||
                orientationSelection_ == nullptr)
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
            trimSelection_->SetStepStatusAsString("Optional");
            orientationSelection_->SetStepStatusAsString("Optional");

            placementSelection_->SetSelectModeAsString("Single");
            placementSelection_->SetAutomaticProgression(false);
            placementSelection_->SetPointOverlay(true);
            // EnableOK owns readiness, including OK after a successful Apply.
            placementSelection_->SetStepStatusAsString("Optional");
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
            shown_ = true;
            ActivatePlacementSelection();
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
        RestoreParameterPlacement();
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
            updating_ = false; ShowError(ex.Message()); return 1;
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

    void ClearPlacementPreview()
    {
        placedWireframe_.clear();
        if (previewVisible_)
        {
            CheckPreviewUf(UF_DISP_regenerate_display());
            previewVisible_ = false;
        }
    }

    void RefreshPlacementPreview()
    {
        if (!shown_ || updating_ || closing_) return;
        struct UpdateScope
        {
            bool& flag;
            explicit UpdateScope(bool& value) : flag(value) { flag = true; }
            ~UpdateScope() { flag = false; }
        } updateScope(updating_);
        const std::size_t index = SelectedIndex(&state_);
        if (!state_.hasPlacement || index == SIZE_MAX)
        {
            ClearPlacementPreview();
            previewKey_.clear();
            return;
        }
        // Read native-pane settings on the NX UI thread, after update_cb.
        // This also detects specification, pattern and direction changes.
        ReadTrimTargets();
        const auto origins = PatternOrigins(&state_, state_.placementOrigin, state_.placementMatrix);
        std::wostringstream signature;
        signature.precision(17);
        signature << LibraryRoot(&state_).wstring() << L'|' << state_.items[index].relativePath
                  << L'|' << state_.items[index].specification << L'|' << UF_ASSEM_ask_work_part();
        for (double value : state_.placementMatrix) signature << L'|' << value;
        for (const auto& origin : origins)
            for (double value : origin) signature << L'|' << value;
        const std::wstring key = signature.str();
        if (key != previewKey_)
        {
            ClearPlacementPreview();
            // Remember failed requests too, so timer errors cannot loop dialogs.
            previewKey_ = key;
            try
            {
                std::wstring error;
                const fs::path model = ResolvedModel(&state_, state_.items[index], error);
                if (model.empty()) throw std::runtime_error(ToAnsi(error));
                if (model != previewModel_ || sourceWireframe_.empty())
                {
                    sourceWireframe_.clear();
                    sourceWireframe_ = LoadPlacementWireframe(model, previewUnits_, &sourcePreviewSimplified_);
                    previewModel_ = model;
                }
                int destinationUnits = 1;
                CheckPreviewUf(UF_PART_ask_units(UF_ASSEM_ask_work_part(), &destinationUnits));
                const double scale = UnitMillimeters(previewUnits_) / UnitMillimeters(destinationUnits);
                bool simplified = sourcePreviewSimplified_;
                placedWireframe_ = BuildPlacedPreview(sourceWireframe_, origins,
                    state_.placementMatrix, scale, simplified);
                lastPreviewDraw_ = 0;
                SetStatus(&state_, simplified
                    ? L"简化外形框预览（复杂零件/阵列）；确定/应用插入完整模型，取消清除预览。"
                    : L"插入位置线框预览；点击确定/应用正式插入，取消清除预览。");
            }
            catch (...)
            {
                placedWireframe_.clear();
                throw;
            }
        }
        if (placedWireframe_.empty() || GetTickCount64() - lastPreviewDraw_ < 500) return;
        UF_OBJ_disp_props_t attributes{};
        attributes.color = 186;
        attributes.font = 1;
        attributes.line_width = -1;
        previewVisible_ = true;
        try
        {
            for (auto& segment : placedWireframe_)
                CheckPreviewUf(UF_DISP_display_temporary_line(NULL_TAG, UF_DISP_USE_WORK_VIEW,
                    segment.start.data(), segment.end.data(), &attributes));
        }
        catch (...)
        {
            ClearPlacementPreview();
            throw;
        }
        lastPreviewDraw_ = GetTickCount64();
    }

    bool EnableOK() noexcept
    {
        // Read-only callback: no selection changes, focus, or nested updates.
        return CanConfirmPlacement(shown_, closing_, state_.hasPlacement, insertedAny_);
    }

    int Apply()
    {
        if (updating_ || closing_) return 1;
        // A point is consumed after successful insertion. Apply/OK must not
        // insert another component at the previously consumed location.
        if (!state_.hasPlacement && insertedAny_) return 0;
        updating_ = true;
        try
        {
            ClearPlacementPreview();
            previewKey_.clear();
            if (state_.hasPlacement) ReadOrientationBlock();
            ReadTrimTargets();
            if (!InsertSelected(&state_))
            {
                updating_ = false;
                return 1;
            }
            insertedAny_ = true;
            state_.hasPlacement = false;
            placementSelection_->SetSelectedObjects({});
            UpdatePlacementStatus(&state_);
            placementSelection_->Focus();
            updating_ = false;
            return 0;
        }
        catch (const NXOpen::NXException& ex) { updating_ = false; ShowError(ex.Message()); return 1; }
        catch (const std::exception& ex) { updating_ = false; ShowError(ex.what()); return 1; }
        catch (...) { updating_ = false; ShowError("调用标准件时发生未知错误。"); return 1; }
    }

    int Ok()
    {
        const int result = Apply();
        if (result == 0) closing_ = true;
        return result;
    }
    int Cancel()
    {
        closing_ = true;
        try { ClearPlacementPreview(); }
        catch (const NXOpen::NXException& ex) { ShowError(ex.Message()); closing_ = false; return 1; }
        catch (const std::exception& ex) { ShowError(ex.what()); closing_ = false; return 1; }
        catch (...) { ShowError("清除插入预览失败。"); closing_ = false; return 1; }
        pendingPlacementMode_ = -1;
        pendingQuickOrient_ = false;
        return 0;
    }
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
        std::shared_ptr<ParameterSaveRequest> resumeParameters;
        for (;;)
        {
            fs::path editModel, captureRoot;
            std::wstring captureCategory;
            std::shared_ptr<ParameterSaveRequest> parameterSave;
            {
                StandardPartsDialogHost host(resumeParameters);
                result = host.Launch();
                editModel = host.RequestedEditModel();
                captureRoot = host.RequestedCaptureRoot();
                captureCategory = host.RequestedCaptureCategory();
                parameterSave = host.RequestedParameterSave();
            } // Dispose dialog/timers/preview before switching part or starting selections.
            resumeParameters.reset();
            if (parameterSave)
            {
                CompleteParameterSave(*parameterSave);
                resumeParameters = parameterSave;
                continue;
            }
            if (!captureRoot.empty())
            {
                ShowBodyCapture(captureRoot, captureCategory);
                continue; // Reopen the browser and rescan newly added parts.
            }
            if (!editModel.empty()) OpenLibraryPartForEdit(editModel);
            break;
        }
    }
    catch (...)
    {
        if (SUCCEEDED(oleStatus)) OleUninitialize();
        throw;
    }
    if (SUCCEEDED(oleStatus)) OleUninitialize();
    return result;
}
