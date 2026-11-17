#include "TianYuanDiFan.hpp"

#include "TransitionGeometry.hpp"
#include "../../common/ZhihuiContextHelp.hpp"
#include "../../common/ZhihuiDialogMemory.hpp"

#ifdef CreateDialog
#undef CreateDialog
#endif
#include <NXOpen/BlockStyler_Label.hxx>
#include <NXOpen/BlockStyler_PropertyList.hxx>
#include <NXOpen/Callback.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/UI.hxx>
#include <uf_disp.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <cmath>
#include <exception>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
constexpr const wchar_t* kMemory = L"TianYuanDiFan.ini";

struct Guard {
    bool& value;
    explicit Guard(bool& flag) : value(flag) { value = true; }
    ~Guard() { value = false; }
};

std::filesystem::path ModuleDir()
{
    HMODULE module = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&ModuleDir), &module);
    wchar_t file[MAX_PATH] = {};
    if (module == nullptr || GetModuleFileNameW(module, file, MAX_PATH) == 0)
        throw std::runtime_error("找不到天圆地方资源目录。");
    return std::filesystem::path(file).parent_path();
}

void Log(const std::string& text) noexcept
{
    wchar_t directory[MAX_PATH] = {};
    if (GetTempPathW(MAX_PATH, directory) == 0) return;
    const std::wstring path = std::wstring(directory) + L"Zhihui-TianYuanDiFan.log";
    FILE* file = nullptr;
    if (_wfopen_s(&file, path.c_str(), L"ab") != 0 || file == nullptr) return;
    fwrite(text.data(), 1, text.size(), file);
    fputc('\n', file);
    fclose(file);
}

struct QuietDisplay {
    int previous = UF_DISP_UNSUPPRESS_DISPLAY;
    bool active = false;
    QuietDisplay()
    {
        if (UF_DISP_ask_display(&previous) == 0 && UF_DISP_set_display(UF_DISP_SUPPRESS_DISPLAY) == 0)
            active = true;
    }
    ~QuietDisplay() { Restore(); }
    void Restore() noexcept
    {
        if (!active) return;
        UF_DISP_set_display(previous);
        if (previous == UF_DISP_UNSUPPRESS_DISPLAY) UF_DISP_regenerate_display();
        active = false;
    }
};

std::wstring WideText(const std::string& text)
{
    if (text.empty()) return L"天圆地方失败。";
    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
    UINT codePage = CP_UTF8;
    DWORD flags = MB_ERR_INVALID_CHARS;
    if (count <= 0) {
        codePage = CP_ACP;
        flags = 0;
        count = MultiByteToWideChar(CP_ACP, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
    }
    if (count <= 0) return L"天圆地方失败。";
    std::wstring wide(static_cast<size_t>(count), L'\0');
    MultiByteToWideChar(codePage, flags, text.c_str(), static_cast<int>(text.size()), wide.data(), count);
    return wide;
}
}

TianYuanDiFanDialog::TianYuanDiFanDialog()
{
    zhihui_context_help::EnsureGlobalHelpLoaded();
    dialog_ = NXOpen::UI::GetUI()->CreateDialog((ModuleDir() / "TianYuanDiFan.dlx").u8string().c_str());
    dialog_->AddInitializeHandler(NXOpen::make_callback(this, &TianYuanDiFanDialog::Initialize));
    dialog_->AddDialogShownHandler(NXOpen::make_callback(this, &TianYuanDiFanDialog::Shown));
    dialog_->AddUpdateHandler(NXOpen::make_callback(this, &TianYuanDiFanDialog::Update));
    dialog_->AddApplyHandler(NXOpen::make_callback(this, &TianYuanDiFanDialog::Apply));
    dialog_->AddOkHandler(NXOpen::make_callback(this, &TianYuanDiFanDialog::Apply));
    dialog_->AddCancelHandler(NXOpen::make_callback(this, &TianYuanDiFanDialog::Cancel));
}

TianYuanDiFanDialog::~TianYuanDiFanDialog()
{
    UF_DISP_refresh();
    delete dialog_;
}

void TianYuanDiFanDialog::Launch()
{
    dialog_->Launch();
}

void TianYuanDiFanDialog::Initialize()
{
    shown_ = false;
    initialized_ = false;
    try {
        status_ = dialog_->TopBlock()->FindBlock("result_status");
        split_ = dialog_->TopBlock()->FindBlock("split");
        diameter_ = dialog_->TopBlock()->FindBlock("diameter");
        length_ = dialog_->TopBlock()->FindBlock("length");
        width_ = dialog_->TopBlock()->FindBlock("width");
        height_ = dialog_->TopBlock()->FindBlock("height");
        thickness_ = dialog_->TopBlock()->FindBlock("thickness");
        corner_ = dialog_->TopBlock()->FindBlock("corner");
        seam_ = dialog_->TopBlock()->FindBlock("seam");
        segmented_ = dialog_->TopBlock()->FindBlock("segmented");
        bendCount_ = dialog_->TopBlock()->FindBlock("bendCount");
        indexMark_ = dialog_->TopBlock()->FindBlock("indexMark");
        if (status_ == nullptr || split_ == nullptr || diameter_ == nullptr || length_ == nullptr || width_ == nullptr || height_ == nullptr
            || thickness_ == nullptr || corner_ == nullptr || seam_ == nullptr || segmented_ == nullptr
            || bendCount_ == nullptr || indexMark_ == nullptr)
            throw std::runtime_error("天圆地方对话框资源不完整。");
        if (auto* label = dynamic_cast<NXOpen::BlockStyler::Label*>(status_)) label->SetWordWrap(true);
        initialized_ = true;
    } catch (const NXOpen::NXException& exception) {
        Report(exception.Message());
    } catch (const std::exception& exception) {
        Report(exception.what());
    } catch (...) {
        Report("初始化天圆地方失败。");
    }
}

void TianYuanDiFanDialog::Shown()
{
    try {
        if (!initialized_) return;
        shown_ = true;
        Guard guard(updating_);
        const double savedSide = zhihui_dialog_memory::ReadDouble(kMemory, L"Side", 200);
        zhihui_dialog_memory::TrySetDouble(diameter_, zhihui_dialog_memory::ReadDouble(kMemory, L"Diameter", 100));
        zhihui_dialog_memory::TrySetDouble(length_, zhihui_dialog_memory::ReadDouble(kMemory, L"Length", savedSide));
        zhihui_dialog_memory::TrySetDouble(width_, zhihui_dialog_memory::ReadDouble(kMemory, L"Width", savedSide));
        zhihui_dialog_memory::TrySetDouble(height_, zhihui_dialog_memory::ReadDouble(kMemory, L"Height", 150));
        zhihui_dialog_memory::TrySetDouble(thickness_, zhihui_dialog_memory::ReadDouble(kMemory, L"Thickness", 1.5));
        zhihui_dialog_memory::TrySetDouble(corner_, zhihui_dialog_memory::ReadDouble(kMemory, L"CornerRadius", 10));
        zhihui_dialog_memory::TrySetDouble(seam_, zhihui_dialog_memory::ReadDouble(kMemory, L"Seam", 1));
        zhihui_dialog_memory::LoadEnum(kMemory, L"Split", split_);
        zhihui_dialog_memory::LoadLogical(kMemory, L"SegmentedBends", segmented_);
        zhihui_dialog_memory::TrySetDouble(bendCount_, zhihui_dialog_memory::ReadDouble(kMemory, L"BendSegments", 15));
        zhihui_dialog_memory::TrySetDouble(indexMark_, zhihui_dialog_memory::ReadDouble(kMemory, L"IndexMarkLength", 5));
        ShowSegmentFields();
        Status("外尺寸，板厚向内。方口圆角必须大于板厚。");
    } catch (const NXOpen::NXException& exception) {
        Report(exception.Message());
    } catch (const std::exception& exception) {
        Report(exception.what());
    } catch (...) {
        Report("显示天圆地方失败。");
    }
}

void TianYuanDiFanDialog::Status(const char* text)
{
    std::unique_ptr<NXOpen::BlockStyler::PropertyList> properties(status_->GetProperties());
    properties->SetString("Label", text);
}

void TianYuanDiFanDialog::Report(const char* text) noexcept
{
    Log(text == nullptr ? "unknown" : text);
    try {
        MessageBoxW(nullptr, WideText(text == nullptr ? "" : text).c_str(), L"天圆地方", MB_OK | MB_ICONERROR);
    } catch (...) {}
}

void TianYuanDiFanDialog::ShowSegmentFields()
{
    bool on = true;
    zhihui_dialog_memory::TryGetLogical(segmented_, &on);
    for (auto* block : {bendCount_, indexMark_}) {
        std::unique_ptr<NXOpen::BlockStyler::PropertyList> properties(block->GetProperties());
        properties->SetLogical("Enable", on);
    }
}

int TianYuanDiFanDialog::Update(NXOpen::BlockStyler::UIBlock* block)
{
    if (!initialized_ || !shown_ || updating_ || block != segmented_) return 0;
    try {
        Guard guard(updating_);
        ShowSegmentFields();
    } catch (const NXOpen::NXException& exception) {
        Report(exception.Message());
    } catch (const std::exception& exception) {
        Report(exception.what());
    } catch (...) {
        Report("更新天圆地方对话框失败。");
    }
    return 0;
}

int TianYuanDiFanDialog::Apply()
{
    if (!initialized_ || !shown_ || updating_) return 1;
    Guard guard(updating_);
    auto* session = NXOpen::Session::GetSession();
    try {
        if (session->Parts()->Work() == nullptr) throw std::runtime_error("请先打开一个工作零件。");
        tianyuan::Input input;
        if (!zhihui_dialog_memory::TryGetDouble(diameter_, &input.diameterMm)
            || !zhihui_dialog_memory::TryGetDouble(length_, &input.lengthMm)
            || !zhihui_dialog_memory::TryGetDouble(width_, &input.widthMm)
            || !zhihui_dialog_memory::TryGetDouble(height_, &input.heightMm)
            || !zhihui_dialog_memory::TryGetDouble(thickness_, &input.thicknessMm)
            || !zhihui_dialog_memory::TryGetDouble(seam_, &input.seamMm)
            || !zhihui_dialog_memory::TryGetDouble(corner_, &input.cornerRadiusMm)
            || !zhihui_dialog_memory::TryGetLogical(segmented_, &input.segmentedBends)
            || !zhihui_dialog_memory::TryGetEnum(split_, &input.split))
            throw std::runtime_error("读取天圆地方尺寸失败。");
        double bendCount = 15;
        double indexMark = 5;
        if (!zhihui_dialog_memory::TryGetDouble(bendCount_, &bendCount)
            || !zhihui_dialog_memory::TryGetDouble(indexMark_, &indexMark))
            throw std::runtime_error("读取天圆地方尺寸失败。");
        input.bendSegments = static_cast<int>(std::lround(bendCount));
        input.indexMarkMm = indexMark;
        tianyuan::Validate(input);
        Log("Create diameter=" + std::to_string(input.diameterMm) + " length=" + std::to_string(input.lengthMm) +
            " width=" + std::to_string(input.widthMm) + " height=" + std::to_string(input.heightMm) +
            " thickness=" + std::to_string(input.thicknessMm) + " seam=" + std::to_string(input.seamMm) +
            " corner=" + std::to_string(input.cornerRadiusMm) + " segmented=" + (input.segmentedBends ? "1" : "0") +
            " segments=" + std::to_string(input.bendSegments) + " split=" + std::to_string(input.split));
        auto mark = session->SetUndoMark(NXOpen::Session::MarkVisibilityVisible, "天圆地方");
        QuietDisplay quiet;
        try {
            tianyuan::Create(input);
        } catch (...) {
            try {
                session->UndoToMark(mark, nullptr);
                session->DeleteUndoMark(mark, nullptr);
            } catch (const NXOpen::NXException& exception) {
                Log(std::string("回滚失败：") + exception.Message());
            }
            throw;
        }
        quiet.Restore();
        zhihui_dialog_memory::SaveDouble(kMemory, L"Diameter", diameter_);
        zhihui_dialog_memory::SaveDouble(kMemory, L"Length", length_);
        zhihui_dialog_memory::SaveDouble(kMemory, L"Width", width_);
        zhihui_dialog_memory::SaveDouble(kMemory, L"Height", height_);
        zhihui_dialog_memory::SaveDouble(kMemory, L"Thickness", thickness_);
        zhihui_dialog_memory::SaveDouble(kMemory, L"Seam", seam_);
        zhihui_dialog_memory::SaveDouble(kMemory, L"CornerRadius", corner_);
        zhihui_dialog_memory::SaveEnum(kMemory, L"Split", split_);
        zhihui_dialog_memory::SaveLogical(kMemory, L"SegmentedBends", segmented_);
        zhihui_dialog_memory::SaveDouble(kMemory, L"BendSegments", bendCount_);
        zhihui_dialog_memory::SaveDouble(kMemory, L"IndexMarkLength", indexMark_);
        Status("已生成天圆地方钣金体和展平图样。");
        UF_DISP_refresh();
        return 0;
    } catch (const NXOpen::NXException& exception) {
        Report(exception.Message());
    } catch (const std::exception& exception) {
        Report(exception.what());
    } catch (...) {
        Report("创建天圆地方失败。");
    }
    UF_DISP_refresh();
    return 1;
}

int TianYuanDiFanDialog::Cancel()
{
    try {
        UF_DISP_refresh();
    } catch (...) {
        Log("Cancel refresh failed");
    }
    return 0;
}
