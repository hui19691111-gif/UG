#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <ShObjIdl.h>
#ifdef CreateDialog
#undef CreateDialog
#endif

#include <NXOpen/BasePart.hxx>
#include <NXOpen/Layout.hxx>
#include <NXOpen/LayoutCollection.hxx>
#include <NXOpen/ModelingView.hxx>
#include <NXOpen/ModelingViewCollection.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXMessageBox.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/PartLoadStatus.hxx>
#include <NXOpen/PartSaveStatus.hxx>
#include <NXOpen/PreviewPropertiesBuilder.hxx>
#include <NXOpen/PropertiesManager.hxx>
#include <NXOpen/ugmath.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/UI.hxx>
#include <NXOpen/View.hxx>

#include <uf.h>
#include <uf_disp.h>
#include <uf_part.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{
using EnsureAuthorizedProc = int(__stdcall*)(const wchar_t*, const wchar_t*, wchar_t*, int);
std::ofstream gLog;

void Log(const std::string& text)
{
    if (!gLog.is_open())
    {
        std::error_code ec;
        fs::create_directories(L"D:\\UGZhiHuiLogs", ec);
        gLog.open(fs::path(L"D:\\UGZhiHuiLogs") / L"YuLanTuXiuFuGonJu.log",
            std::ios::binary | std::ios::app);
    }
    if (gLog.is_open())
    {
        SYSTEMTIME now{}; GetLocalTime(&now);
        char stamp[64] = {};
        sprintf_s(stamp, "%04u-%02u-%02u %02u:%02u:%02u.%03u ", now.wYear, now.wMonth,
            now.wDay, now.wHour, now.wMinute, now.wSecond, now.wMilliseconds);
        gLog << stamp << text << '\n';
        gLog.flush();
    }
}

std::string Utf8(const std::wstring& value)
{
    if (value.empty()) return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<size_t>(size - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, result.data(), size, nullptr, nullptr);
    return result;
}

std::string Locale(const std::wstring& value)
{
    if (value.empty()) return {};
    const int size = WideCharToMultiByte(CP_ACP, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<size_t>(size - 1), '\0');
    WideCharToMultiByte(CP_ACP, 0, value.c_str(), -1, result.data(), size, nullptr, nullptr);
    return result;
}

HMODULE LoadLicenseGate()
{
    constexpr const wchar_t* name = L"ZhaoFuNxLicenseGate.dll";
    if (HMODULE loaded = GetModuleHandleW(name)) return loaded;
    HMODULE self = nullptr;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&LoadLicenseGate), &self))
    {
        wchar_t path[MAX_PATH] = {};
        if (GetModuleFileNameW(self, path, MAX_PATH) > 0)
        {
            if (wchar_t* slash = wcsrchr(path, L'\\'))
            {
                *(slash + 1) = L'\0';
                wcscat_s(path, name);
                if (HMODULE local = LoadLibraryW(path)) return local;
            }
        }
    }
    return LoadLibraryW(name);
}

bool EnsureAuthorized()
{
#ifndef ZH_PROTECTED_BUILD
    return true;
#else
    HMODULE module = LoadLicenseGate();
    if (!module) return false;
    auto procedure = reinterpret_cast<EnsureAuthorizedProc>(GetProcAddress(module, "ZfnxEnsureAuthorized"));
    if (!procedure) return false;
    wchar_t message[1024] = {};
    return procedure(L"ZHIHUI.PREVIEW_REPAIR", L"预览图修复", message, 1024) == 1;
#endif
}

void Show(NXOpen::NXMessageBox::DialogType type, const std::string& text)
{
    NXOpen::UI::GetUI()->NXMessageBox()->Show("预览图修复", type, text.c_str());
}

bool PickFolder(std::wstring& folder)
{
    IFileOpenDialog* dialog = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&dialog)))) return false;
    DWORD options = 0;
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    dialog->SetTitle(L"选择要批量修复预览图的文件夹");
    const HRESULT shown = dialog->Show(nullptr);
    if (SUCCEEDED(shown))
    {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dialog->GetResult(&item)))
        {
            PWSTR path = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)))
            {
                folder = path;
                CoTaskMemFree(path);
            }
            item->Release();
        }
    }
    dialog->Release();
    return !folder.empty();
}

NXOpen::ModelingView* ActivateModelView(NXOpen::BasePart* part)
{
    NXOpen::ModelingView* view = nullptr;
    for (const char* name : {"Isometric", "Trimetric", "Top"})
    {
        try { view = part->ModelingViews()->FindObject(name); if (view) break; } catch (...) {}
    }
    if (!view) view = part->ModelingViews()->WorkView();
    if (!view) throw std::runtime_error("找不到模型空间视图");

    if (NXOpen::Layout* current = part->Layouts()->Current())
    {
        try { current->ReplaceView(view, 0, 0, true); } catch (...) {}
    }
    view->MakeWork();
    view->Orient(NXOpen::View::CannedIsometric, NXOpen::View::ScaleAdjustmentFit);
    view->Fit();
    view->Regenerate();
    view->UpdateDisplay();
    return view;
}

struct PreviewInfo
{
    int width = 0;
    int height = 0;
    size_t changes = 0;
    unsigned long long hash = 0;
};

PreviewInfo ReadPreview(NXOpen::BasePart* part)
{
    PreviewInfo info;
    std::vector<int> pixels;
    part->GetPreviewImage(&info.width, &info.height, pixels);
    unsigned long long hash = 1469598103934665603ULL;
    int previous = 0; bool first = true;
    for (int pixel : pixels)
    {
        hash ^= static_cast<unsigned int>(pixel); hash *= 1099511628211ULL;
        if (first || pixel != previous) { ++info.changes; previous = pixel; first = false; }
    }
    info.hash = hash;
    if (info.width < 32 || info.height < 32 || pixels.empty()) throw std::runtime_error("NX 未生成预览图");
    if (info.changes < 8) throw std::runtime_error("NX 生成的预览图为空白");
    return info;
}

void SavePreview(NXOpen::BasePart* part)
{
    ActivateModelView(part);
    UF_DISP_regenerate_display();
    UF_DISP_refresh();
    UF_DISP_make_display_up_to_date();

    std::vector<NXOpen::NXObject*> objects{part};
    NXOpen::PreviewPropertiesBuilder* builder =
        part->PropertiesManager()->CreatePreviewPropertiesBuilder(objects);
    try
    {
        builder->SetStorePartPreview(true);
        builder->SetPartCreation(NXOpen::PreviewPropertiesBuilder::PartCreationOptionsOnSave);
        builder->SetStoreModelViewPreview(true);
        builder->SetModelViewCreation(NXOpen::PreviewPropertiesBuilder::ModelViewCreationOptionsOnPartSave);
        builder->Commit();
        builder->Destroy();
        builder = nullptr;
    }
    catch (...)
    {
        if (builder) builder->Destroy();
        throw;
    }
    NXOpen::PartSaveStatus* status = part->Save(
        NXOpen::BasePart::SaveComponentsFalse,
        NXOpen::BasePart::CloseAfterSaveFalse);
    const int unsaved = status ? status->NumberUnsavedParts() : 0;
    delete status;
    if (unsaved > 0) throw std::runtime_error("零件保存失败");
    const PreviewInfo saved = ReadPreview(part);
    Log("saved native preview width=" + std::to_string(saved.width) +
        " height=" + std::to_string(saved.height) +
        " changes=" + std::to_string(saved.changes) +
        " hash=" + std::to_string(saved.hash));
}

struct ViewState
{
    NXOpen::ModelingView* view = nullptr;
    NXOpen::Matrix3x3 matrix{};
    NXOpen::Point3d origin{};
    double scale = 1.0;
    bool valid = false;
};

ViewState CaptureView(NXOpen::BasePart* part)
{
    ViewState state;
    if (!part) return state;
    try
    {
        state.view = part->ModelingViews()->WorkView();
        if (state.view)
        {
            state.matrix = state.view->Matrix();
            state.origin = state.view->Origin();
            state.scale = state.view->Scale();
            state.valid = true;
        }
    }
    catch (...) {}
    return state;
}

void RestoreView(const ViewState& state)
{
    if (!state.valid || !state.view) return;
    try
    {
        state.view->MakeWork();
        state.view->SetRotationTranslationScale(state.matrix, state.origin, state.scale);
        state.view->Regenerate();
        state.view->UpdateDisplay();
    }
    catch (...) {}
}

struct Summary { int success = 0; int failed = 0; std::vector<std::string> errors; };

Summary ProcessCurrent(NXOpen::Session* session)
{
    Summary result;
    NXOpen::BasePart* part = session->Parts()->BaseDisplay();
    if (!part) { result.failed = 1; result.errors.push_back("当前没有打开零件"); return result; }
    const ViewState state = CaptureView(part);
    try { Log("current begin"); SavePreview(part); ++result.success; Log("current success"); }
    catch (const std::exception& ex) { ++result.failed; result.errors.push_back(ex.what()); Log(std::string("current failed: ") + ex.what()); }
    RestoreView(state);
    return result;
}

Summary ProcessFolder(NXOpen::Session* session, const fs::path& folder, bool recursive)
{
    Summary result;
    NXOpen::BasePart* original = session->Parts()->BaseDisplay();
    const ViewState originalView = CaptureView(original);
    std::vector<fs::path> files;
    try
    {
        if (recursive)
            for (const auto& entry : fs::recursive_directory_iterator(folder))
                if (entry.is_regular_file() && _wcsicmp(entry.path().extension().c_str(), L".prt") == 0) files.push_back(entry.path());
        else
            for (const auto& child : fs::directory_iterator(folder))
                if (child.is_regular_file() && _wcsicmp(child.path().extension().c_str(), L".prt") == 0) files.push_back(child.path());
    }
    catch (const std::exception& ex) { result.failed = 1; result.errors.push_back(ex.what()); return result; }
    std::sort(files.begin(), files.end());
    files.erase(std::unique(files.begin(), files.end(), [](const fs::path& left, const fs::path& right) {
        return _wcsicmp(left.c_str(), right.c_str()) == 0;
    }), files.end());
    Log("batch files=" + std::to_string(files.size()));

    for (const fs::path& path : files)
    {
        NXOpen::BasePart* part = nullptr;
        NXOpen::PartLoadStatus* load = nullptr;
        bool openedByTool = false;
        fs::path rollback;
        try
        {
            Log("begin " + Utf8(path.wstring()));
            const std::string pathLocale = Locale(path.wstring());
            const tag_t loadedTag = UF_PART_ask_part_tag(pathLocale.c_str());
            if (loadedTag != NULL_TAG)
            {
                part = dynamic_cast<NXOpen::BasePart*>(NXOpen::NXObjectManager::Get(loadedTag));
                if (part && part->IsModified())
                    throw std::runtime_error("零件已在当前会话修改，已跳过以免覆盖");
                if (part)
                {
                    session->Parts()->SetDisplay(part, false, false, &load);
                }
            }
            else
            {
                wchar_t tempPath[MAX_PATH] = {};
                GetTempPathW(MAX_PATH, tempPath);
                rollback = fs::path(tempPath) / L"UG2412PreviewRepair" /
                    (std::to_wstring(GetCurrentProcessId()) + L"-" +
                     std::to_wstring(result.success + result.failed) + L".prt");
                fs::create_directories(rollback.parent_path());
                fs::copy_file(path, rollback, fs::copy_options::overwrite_existing);
                part = session->Parts()->OpenBaseDisplay(pathLocale.c_str(), &load);
                openedByTool = true;
            }
            if (!part || (load && load->NumberUnloadedParts() > 0)) throw std::runtime_error("零件未完整加载");
            SavePreview(part);
            ++result.success;
            Log("success " + Utf8(path.wstring()));
            if (!rollback.empty()) { std::error_code ec; fs::remove(rollback, ec); }
        }
        catch (const NXOpen::NXException& ex)
        {
            ++result.failed;
            result.errors.push_back(Utf8(path.filename().wstring()) + "：" + ex.Message());
            Log("NX failed " + Utf8(path.wstring()) + ": " + ex.Message());
        }
        catch (const std::exception& ex)
        {
            ++result.failed;
            result.errors.push_back(Utf8(path.filename().wstring()) + "：" + ex.what());
            Log("failed " + Utf8(path.wstring()) + ": " + ex.what());
        }
        delete load;
        if (openedByTool && part)
        {
            try { part->Close(NXOpen::BasePart::CloseWholeTreeFalse,
                NXOpen::BasePart::CloseModifiedCloseModified, nullptr); } catch (...) {}
        }
        if (!rollback.empty() && fs::exists(rollback))
        {
            std::error_code ec;
            fs::copy_file(rollback, path, fs::copy_options::overwrite_existing, ec);
            fs::remove(rollback, ec);
        }
    }

    if (original)
    {
        NXOpen::PartLoadStatus* status = nullptr;
        try { session->Parts()->SetDisplay(original, false, false, &status); } catch (...) {}
        delete status;
        RestoreView(originalView);
    }
    return result;
}

void ShowSummary(const Summary& summary)
{
    std::ostringstream text;
    text << "处理完成：成功 " << summary.success << "，失败 " << summary.failed << "。";
    if (!summary.errors.empty())
    {
        text << "\n\n失败明细：";
        const size_t shown = std::min<size_t>(summary.errors.size(), 12);
        for (size_t i = 0; i < shown; ++i) text << "\n" << summary.errors[i];
        if (summary.errors.size() > shown) text << "\n……另有 " << (summary.errors.size() - shown) << " 项";
    }
    Show(summary.failed == 0 ? NXOpen::NXMessageBox::DialogTypeInformation : NXOpen::NXMessageBox::DialogTypeWarning, text.str());
}
}

extern "C" __declspec(dllexport) void ufusr(char*, int* returnCode, int)
{
    if (returnCode) *returnCode = 0;
    if (!EnsureAuthorized())
    {
        if (returnCode) *returnCode = 1;
        Show(NXOpen::NXMessageBox::DialogTypeError, "授权验证未通过。");
        return;
    }
    const int initialized = UF_initialize();
    if (initialized != 0) { if (returnCode) *returnCode = initialized; return; }
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    Log("session start");
    try
    {
        NXOpen::Session* session = NXOpen::Session::GetSession();
        const int choice = MessageBoxW(nullptr,
            L"选择处理方式：\n\n是：修复当前显示零件\n否：选择文件夹批量修复\n取消：退出\n\n处理时会短暂切换模型，结束后恢复当前零件和视图。",
            L"UG2412 预览图修复", MB_YESNOCANCEL | MB_ICONINFORMATION | MB_TOPMOST);
        if (choice == IDYES) ShowSummary(ProcessCurrent(session));
        else if (choice == IDNO)
        {
            std::wstring folder;
            if (PickFolder(folder))
            {
                const int recurse = MessageBoxW(nullptr, L"是否同时处理子文件夹中的 .prt？",
                    L"UG2412 预览图修复", MB_YESNO | MB_ICONQUESTION | MB_TOPMOST);
                ShowSummary(ProcessFolder(session, folder, recurse == IDYES));
            }
        }
    }
    catch (const NXOpen::NXException& ex)
    {
        if (returnCode) *returnCode = ex.ErrorCode();
        Show(NXOpen::NXMessageBox::DialogTypeError, ex.Message());
    }
    catch (const std::exception& ex)
    {
        if (returnCode) *returnCode = -1;
        Show(NXOpen::NXMessageBox::DialogTypeError, ex.what());
    }
    CoUninitialize();
    Log("session end");
    UF_terminate();
}

extern "C" __declspec(dllexport) int ufusr_ask_unload()
{
    return UF_UNLOAD_IMMEDIATELY;
}
