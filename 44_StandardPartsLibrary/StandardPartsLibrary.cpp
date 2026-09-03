#include "StandardPartsLibrary.hpp"

#include <uf.h>
#include <uf_assem.h>
#include <uf_csys.h>
#include <uf_disp.h>
#include <uf_part.h>

#include <Windows.h>
#include <CommCtrl.h>
#include <commdlg.h>
#include <ShlObj.h>
#include <shobjidl_core.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
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
    ID_DETAIL_NAME, ID_DETAIL_CATEGORY, ID_DETAIL_FILE
};

struct LibraryItem
{
    std::wstring id;
    std::wstring name;
    std::wstring category;
    std::wstring relativePath;
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
    bool refreshingCategories = false;
    bool running = true;
};

void UpdatePreview(AppState* state);

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
    return true;
}

bool SaveIndex(AppState* state, std::wstring& error)
{
    if (!EnsureLibraryFolders(state, error)) return false;
    std::wostringstream content;
    content << L"# ZHIHUI_STANDARD_PARTS_V1\r\n";
    content << L"# id\tname\tcategory\trelative_model_path\r\n";
    for (const auto& item : state->items)
        content << item.id << L'\t' << item.name << L'\t' << item.category << L'\t'
                << item.relativePath << L"\r\n";

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

void LoadIndex(AppState* state)
{
    state->items.clear();
    std::wstring error;
    if (!EnsureLibraryFolders(state, error)) { SetStatus(state, error); return; }
    const fs::path index = IndexPath(state);
    if (!fs::exists(index))
    {
        SaveIndex(state, error);
        SetStatus(state, L"已创建新的标准件库。");
        return;
    }
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
            state->items.push_back({fields[0], fields[1], fields[2], fields[3]});
    }
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

void RefreshList(AppState* state)
{
    ListView_DeleteAllItems(state->list);
    state->visible.clear();
    if (state->thumbnails != nullptr)
    {
        ListView_SetImageList(state->list, nullptr, LVSIL_NORMAL);
        ImageList_Destroy(state->thumbnails);
    }
    state->thumbnails = ImageList_Create(112, 112, ILC_COLOR32 | ILC_MASK, 8, 8);
    HDC screen = GetDC(nullptr);
    HBITMAP placeholder = CreateCompatibleBitmap(screen, 112, 112);
    HDC memory = CreateCompatibleDC(screen);
    const HGDIOBJ oldBitmap = SelectObject(memory, placeholder);
    RECT placeholderRect{0, 0, 112, 112};
    FillRect(memory, &placeholderRect, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));
    HPEN pen = CreatePen(PS_SOLID, 2, RGB(130, 150, 170));
    const HGDIOBJ oldPen = SelectObject(memory, pen);
    Rectangle(memory, 18, 28, 94, 84);
    MoveToEx(memory, 18, 28, nullptr); LineTo(memory, 43, 13);
    LineTo(memory, 102, 13); LineTo(memory, 94, 28);
    SelectObject(memory, oldPen);
    DeleteObject(pen);
    SelectObject(memory, oldBitmap);
    DeleteDC(memory);
    ReleaseDC(nullptr, screen);
    ImageList_AddMasked(state->thumbnails, placeholder, RGB(255, 255, 255));
    DeleteObject(placeholder);
    ListView_SetImageList(state->list, state->thumbnails, LVSIL_NORMAL);
    ListView_SetIconSpacing(state->list, 142, 148);

    const std::wstring search = Lower(Trim(GetText(state->window, ID_SEARCH)));
    for (std::size_t index = 0; index < state->items.size(); ++index)
    {
        const auto& item = state->items[index];
        if (!state->selectedCategory.empty() && item.category != state->selectedCategory) continue;
        if (!search.empty() && Lower(item.name + L" " + item.category + L" " + item.relativePath).find(search) == std::wstring::npos) continue;
        int imageIndex = 0;
        const fs::path model = LibraryRoot(state) / item.relativePath;
        IShellItemImageFactory* factory = nullptr;
        HBITMAP thumbnail = nullptr;
        if (SUCCEEDED(SHCreateItemFromParsingName(model.c_str(), nullptr, IID_PPV_ARGS(&factory))))
        {
            SIZE size{112, 112};
            factory->GetImage(size,
                static_cast<SIIGBF>(SIIGBF_THUMBNAILONLY | SIIGBF_BIGGERSIZEOK |
                                    SIIGBF_RESIZETOFIT), &thumbnail);
            factory->Release();
        }
        if (thumbnail != nullptr)
        {
            imageIndex = ImageList_Add(state->thumbnails, thumbnail, nullptr);
            DeleteObject(thumbnail);
            if (imageIndex < 0) imageIndex = 0;
        }
        LVITEMW row{};
        row.mask = LVIF_TEXT | LVIF_IMAGE;
        row.iItem = static_cast<int>(state->visible.size());
        row.iImage = imageIndex;
        row.pszText = const_cast<wchar_t*>(item.name.c_str());
        ListView_InsertItem(state->list, &row);
        state->visible.push_back(index);
    }
    UpdatePreview(state);
}

std::size_t SelectedIndex(AppState* state)
{
    const int row = ListView_GetNextItem(state->list, -1, LVNI_SELECTED);
    if (row < 0 || static_cast<std::size_t>(row) >= state->visible.size()) return SIZE_MAX;
    return state->visible[static_cast<std::size_t>(row)];
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
    if (name.empty()) name = source.stem().wstring();
    if (category.empty()) category = L"用户自定义";
    if (name.find(L'\t') != std::wstring::npos || category.find(L'\t') != std::wstring::npos)
    {
        error = L"名称和分类不能包含制表符。";
        return false;
    }
    if (!EnsureLibraryFolders(state, error)) return false;
    const std::wstring id = std::to_wstring(GetTickCount64()) + L"_" + std::to_wstring(GetCurrentProcessId());
    const std::wstring targetName = SanitizeFileName(name) + L"_" + id + L".prt";
    const fs::path target = LibraryRoot(state) / L"models" / targetName;
    std::error_code ec;
    fs::copy_file(source, target, fs::copy_options::overwrite_existing, ec);
    if (ec)
    {
        error = std::wstring(L"复制部件模型失败：") + FromAnsi(ec.message());
        return false;
    }
    state->items.push_back({id, name, category, (fs::path(L"models") / targetName).wstring()});
    if (!SaveIndex(state, error))
    {
        state->items.pop_back();
        fs::remove(target, ec);
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

void UpdatePreview(AppState* state)
{
    if (state->previewBitmap != nullptr)
    {
        DeleteObject(state->previewBitmap);
        state->previewBitmap = nullptr;
    }
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
            IShellItemImageFactory* factory = nullptr;
            if (SUCCEEDED(SHCreateItemFromParsingName(model.c_str(), nullptr,
                                                       IID_PPV_ARGS(&factory))))
            {
                SIZE size{300, 250};
                factory->GetImage(size,
                    static_cast<SIIGBF>(SIIGBF_THUMBNAILONLY | SIIGBF_BIGGERSIZEOK |
                                        SIIGBF_RESIZETOFIT),
                    &state->previewBitmap);
                factory->Release();
            }
        }
    }
    else
    {
        SetText(state->window, ID_DETAIL_NAME, L"未选择");
        SetText(state->window, ID_DETAIL_CATEGORY, L"-");
        SetText(state->window, ID_DETAIL_FILE, L"-");
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
    double origin[3]{};
    double matrix[9]{};
    if (!AskWcs(origin, matrix, error)) return false;
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
    const fs::path model = ResolvedModel(state, item, error);
    if (model.empty()) return false;
    if (Lower(model.wstring()) == Lower(CurrentPartPath()))
    {
        error = L"不能把当前工作部件合并到自身。";
        return false;
    }
    double origin[3]{};
    double matrix[9]{};
    if (!AskWcs(origin, matrix, error)) return false;
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

void BuildUi(AppState* state)
{
    state->font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    AddControl(state, 0, L"STATIC", L"库目录：", SS_LEFT, 14, 15, 65, 22, 0);
    const std::wstring savedRoot = LoadRootPreference();
    AddControl(state, WS_EX_CLIENTEDGE, L"EDIT", savedRoot.c_str(), ES_AUTOHSCROLL, 80, 12, 960, 25, ID_ROOT);
    AddControl(state, 0, L"BUTTON", L"浏览...", BS_PUSHBUTTON, 1050, 11, 82, 27, ID_BROWSE_ROOT);
    AddControl(state, 0, L"BUTTON", L"刷新", BS_PUSHBUTTON, 1142, 11, 82, 27, ID_REFRESH);

    AddControl(state, 0, L"STATIC", L"搜索：", SS_LEFT, 14, 52, 55, 22, 0);
    AddControl(state, WS_EX_CLIENTEDGE, L"EDIT", L"", ES_AUTOHSCROLL, 66, 49, 420, 25, ID_SEARCH);
    AddControl(state, 0, L"STATIC", L"输入名称、分类或规格关键字", SS_LEFT, 500, 52, 260, 22, 0);

    AddControl(state, 0, L"BUTTON", L"标准件分类", BS_GROUPBOX, 14, 80, 220, 472, 0);
    state->category = AddControl(state, WS_EX_CLIENTEDGE, WC_TREEVIEWW, L"",
                                 TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT |
                                 TVS_SHOWSELALWAYS, 25, 105, 198, 432, ID_CATEGORY);

    AddControl(state, 0, L"BUTTON", L"标准件选择", BS_GROUPBOX, 244, 80, 606, 472, 0);
    state->list = AddControl(state, WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
                             LVS_ICON | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_AUTOARRANGE,
                             255, 105, 584, 432, ID_LIST);
    ListView_SetExtendedListViewStyle(state->list,
                                      LVS_EX_DOUBLEBUFFER | LVS_EX_BORDERSELECT | LVS_EX_INFOTIP);

    AddControl(state, 0, L"BUTTON", L"模型预览与调用", BS_GROUPBOX, 860, 80, 364, 472, 0);
    state->preview = AddControl(state, WS_EX_CLIENTEDGE, L"STATIC", L"", SS_OWNERDRAW,
                                875, 105, 334, 260, ID_PREVIEW);
    AddControl(state, 0, L"STATIC", L"名称：", SS_LEFT, 878, 381, 48, 22, 0);
    AddControl(state, 0, L"STATIC", L"未选择", SS_LEFT | SS_PATHELLIPSIS,
               928, 381, 274, 22, ID_DETAIL_NAME);
    AddControl(state, 0, L"STATIC", L"分类：", SS_LEFT, 878, 405, 48, 22, 0);
    AddControl(state, 0, L"STATIC", L"-", SS_LEFT | SS_PATHELLIPSIS,
               928, 405, 274, 22, ID_DETAIL_CATEGORY);
    AddControl(state, 0, L"STATIC", L"文件：", SS_LEFT, 878, 429, 48, 22, 0);
    AddControl(state, 0, L"STATIC", L"-", SS_LEFT | SS_PATHELLIPSIS,
               928, 429, 274, 22, ID_DETAIL_FILE);
    AddControl(state, 0, L"BUTTON", L"装配调用", BS_AUTORADIOBUTTON | WS_GROUP,
               878, 459, 102, 24, ID_MODE_ASSEMBLY);
    AddControl(state, 0, L"BUTTON", L"多实体调用", BS_AUTORADIOBUTTON,
               990, 459, 112, 24, ID_MODE_BODY);
    CheckRadioButton(state->window, ID_MODE_ASSEMBLY, ID_MODE_BODY, ID_MODE_ASSEMBLY);
    AddControl(state, 0, L"STATIC", L"按当前 WCS 原点和方向放置",
               SS_LEFT, 878, 486, 230, 20, 0);
    AddControl(state, 0, L"BUTTON", L"调用选中标准件", BS_DEFPUSHBUTTON,
               878, 510, 324, 32, ID_INSERT);

    AddControl(state, 0, L"BUTTON", L"用户标准件管理", BS_GROUPBOX, 14, 560, 1030, 106, 0);
    AddControl(state, 0, L"STATIC", L"名称：", SS_LEFT, 28, 589, 50, 22, 0);
    AddControl(state, WS_EX_CLIENTEDGE, L"EDIT", L"", ES_AUTOHSCROLL, 78, 586, 210, 25, ID_NAME);
    AddControl(state, 0, L"STATIC", L"分类：", SS_LEFT, 305, 589, 50, 22, 0);
    AddControl(state, WS_EX_CLIENTEDGE, L"EDIT", L"用户自定义", ES_AUTOHSCROLL, 355, 586, 190, 25, ID_EDIT_CATEGORY);
    AddControl(state, 0, L"BUTTON", L"入库当前部件", BS_PUSHBUTTON, 560, 584, 114, 29, ID_ADD_CURRENT);
    AddControl(state, 0, L"STATIC", L"源文件：", SS_LEFT, 28, 626, 60, 22, 0);
    AddControl(state, WS_EX_CLIENTEDGE, L"EDIT", L"", ES_AUTOHSCROLL | ES_READONLY,
               92, 623, 453, 25, ID_SOURCE);
    AddControl(state, 0, L"BUTTON", L"浏览...", BS_PUSHBUTTON, 560, 621, 72, 29, ID_BROWSE_SOURCE);
    AddControl(state, 0, L"BUTTON", L"添加文件", BS_PUSHBUTTON, 640, 621, 90, 29, ID_ADD_FILE);
    AddControl(state, 0, L"BUTTON", L"删除选中项", BS_PUSHBUTTON, 740, 621, 100, 29, ID_DELETE);
    AddControl(state, 0, L"STATIC", L"", SS_LEFT, 860, 574, 364, 48, ID_STATUS);
    AddControl(state, 0, L"BUTTON", L"关闭", BS_PUSHBUTTON, 1118, 628, 106, 32, ID_CLOSE);
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
    fs::remove(LibraryRoot(state) / item.relativePath, ec);
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
        SetStatus(state, L"已按当前 WCS " + std::wstring(assembly ? L"装配" : L"合并") +
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
            const auto* notification = reinterpret_cast<NMLISTVIEW*>(lParam);
            if ((notification->uChanged & LVIF_STATE) != 0 &&
                ((notification->uNewState ^ notification->uOldState) & LVIS_SELECTED) != 0)
                UpdatePreview(state);
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
                          L"请选中标准件\r\n\r\n若没有缩略图，请用 NX\r\n重新保存该 PRT 后刷新",
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
    default: break;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}
}

int LaunchStandardPartsLibrary()
{
    INITCOMMONCONTROLSEX commonControls{sizeof(commonControls), ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&commonControls);
    OleInitialize(nullptr);

    AppState state;
    state.parent = GetForegroundWindow();
    HMODULE module = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                          GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                      reinterpret_cast<LPCWSTR>(&LaunchStandardPartsLibrary), &module);
    const std::wstring className = L"ZhihuiStandardPartsLibrary_" +
                                   std::to_wstring(reinterpret_cast<std::uintptr_t>(module));
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = module;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    windowClass.lpszClassName = className.c_str();
    if (!RegisterClassExW(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    {
        OleUninitialize();
        return static_cast<int>(GetLastError());
    }

    RECT area{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &area, 0);
    constexpr int width = 1256;
    constexpr int height = 750;
    const int x = area.left + ((area.right - area.left) - width) / 2;
    const int y = area.top + ((area.bottom - area.top) - height) / 2;
    HWND window = CreateWindowExW(WS_EX_DLGMODALFRAME, className.c_str(), kTitle,
                                  WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                                  x, y, width, height, state.parent, nullptr, module, &state);
    if (window == nullptr)
    {
        UnregisterClassW(className.c_str(), module);
        OleUninitialize();
        return static_cast<int>(GetLastError());
    }
    if (state.parent != nullptr) EnableWindow(state.parent, FALSE);
    ShowWindow(window, SW_SHOW);
    UpdateWindow(window);
    MSG message{};
    while (state.running && GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        if (!IsDialogMessageW(window, &message))
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    if (state.parent != nullptr)
    {
        EnableWindow(state.parent, TRUE);
        SetForegroundWindow(state.parent);
    }
    UnregisterClassW(className.c_str(), module);
    OleUninitialize();
    return 0;
}
