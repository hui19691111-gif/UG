#include "TiaoZenBanLeiCiCun.hpp"

#ifdef CreateDialog
#undef CreateDialog
#endif

#include <NXOpen/BlockStyler_PropertyList.hxx>
#include <NXOpen/BlockStyler_LinearDimension.hxx>
#include <NXOpen/BlockStyler_TabControl.hxx>
#include <NXOpen/BlockStyler_UIBlock.hxx>
#include <NXOpen/Body.hxx>
#include <NXOpen/Callback.hxx>
#include <NXOpen/Direction.hxx>
#include <NXOpen/DirectionCollection.hxx>
#include <NXOpen/Edge.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/FaceDumbRule.hxx>
#include <NXOpen/Features_Feature.hxx>
#include <NXOpen/Features_FeatureCollection.hxx>
#include <NXOpen/Features_PullFaceBuilder.hxx>
#include <NXOpen/GeometricUtilities_ModlMotion.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXMessageBox.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/ScRuleFactory.hxx>
#include <NXOpen/Selection.hxx>
#include <NXOpen/SelectionIntentRule.hxx>
#include <NXOpen/SelectionIntentRuleOptions.hxx>
#include <NXOpen/SmartObject.hxx>
#include <NXOpen/TaggedObject.hxx>
#include <NXOpen/UI.hxx>
#include <NXOpen/Update.hxx>

#include <uf_disp.h>
#include <uf_modl.h>
#include <uf_obj.h>
#include <uf_object_types.h>
#include <uf_ui_types.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#ifdef CreateDialog
#undef CreateDialog
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace
{
constexpr double kTolerance = 1.0e-5;
constexpr double kParallelTolerance = 0.995;

double Dot(const NXOpen::Vector3d& a, const NXOpen::Vector3d& b)
{
    return a.X * b.X + a.Y * b.Y + a.Z * b.Z;
}

double Dot(const NXOpen::Point3d& point, const NXOpen::Vector3d& direction)
{
    return point.X * direction.X +
           point.Y * direction.Y +
           point.Z * direction.Z;
}

double Length(const NXOpen::Vector3d& value)
{
    return std::sqrt(Dot(value, value));
}

NXOpen::Vector3d Subtract(const NXOpen::Point3d& to,
                          const NXOpen::Point3d& from)
{
    return NXOpen::Vector3d(to.X - from.X,
                            to.Y - from.Y,
                            to.Z - from.Z);
}

NXOpen::Vector3d Scale(const NXOpen::Vector3d& value, double scale)
{
    return NXOpen::Vector3d(value.X * scale,
                            value.Y * scale,
                            value.Z * scale);
}

NXOpen::Point3d Move(const NXOpen::Point3d& point,
                     const NXOpen::Vector3d& direction,
                     double distance)
{
    return NXOpen::Point3d(point.X + direction.X * distance,
                           point.Y + direction.Y * distance,
                           point.Z + direction.Z * distance);
}

NXOpen::Vector3d Cross(const NXOpen::Vector3d& a,
                       const NXOpen::Vector3d& b)
{
    return NXOpen::Vector3d(a.Y * b.Z - a.Z * b.Y,
                            a.Z * b.X - a.X * b.Z,
                            a.X * b.Y - a.Y * b.X);
}

bool Normalize(NXOpen::Vector3d& value)
{
    const double length = Length(value);
    if (length <= kTolerance)
    {
        return false;
    }
    value.X /= length;
    value.Y /= length;
    value.Z /= length;
    return true;
}

void Canonicalize(NXOpen::Vector3d& value)
{
    const double components[3] = {
        std::abs(value.X), std::abs(value.Y), std::abs(value.Z)};
    int dominant = 0;
    if (components[1] > components[dominant])
    {
        dominant = 1;
    }
    if (components[2] > components[dominant])
    {
        dominant = 2;
    }
    const double signedComponent =
        dominant == 0 ? value.X : (dominant == 1 ? value.Y : value.Z);
    if (signedComponent < 0.0)
    {
        value = Scale(value, -1.0);
    }
}

std::string Number(double value)
{
    std::ostringstream stream;
    stream << std::setprecision(15) << value;
    return stream.str();
}

std::string Fixed(double value)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2) << value;
    return stream.str();
}

std::string SignedFixed(double value)
{
    std::ostringstream stream;
    stream << std::showpos << std::fixed << std::setprecision(2) << value;
    return stream.str();
}

std::string DialogPath()
{
    wchar_t modulePath[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(
        reinterpret_cast<HMODULE>(&__ImageBase), modulePath, MAX_PATH);
    if (length > 0 && length < MAX_PATH)
    {
        const std::filesystem::path candidate =
            std::filesystem::path(modulePath).parent_path() /
            L"TiaoZenBanLeiCiCun.dlx";
        if (std::filesystem::exists(candidate))
        {
            return candidate.string();
        }
    }
    return "TiaoZenBanLeiCiCun.dlx";
}

std::string ModuleAssetPath(const wchar_t* fileName)
{
    wchar_t modulePath[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(
        reinterpret_cast<HMODULE>(&__ImageBase), modulePath, MAX_PATH);
    if (length > 0 && length < MAX_PATH)
    {
        return (std::filesystem::path(modulePath).parent_path() / fileName)
            .string();
    }
    return std::filesystem::path(fileName).string();
}

std::filesystem::path ResolveLogPath()
{
    std::filesystem::path logDirectory;
    wchar_t modulePath[MAX_PATH] = {};
    const DWORD moduleLength = GetModuleFileNameW(
        reinterpret_cast<HMODULE>(&__ImageBase), modulePath, MAX_PATH);
    if (moduleLength > 0 && moduleLength < MAX_PATH)
    {
        const std::filesystem::path moduleDirectory =
            std::filesystem::path(modulePath).parent_path();
        logDirectory = moduleDirectory.parent_path() / L"logs";
    }

    std::error_code error;
    if (!logDirectory.empty())
    {
        std::filesystem::create_directories(logDirectory, error);
        if (!error)
        {
            return logDirectory / L"TiaoZenBanLeiCiCun.log";
        }
    }

    wchar_t temporaryPath[MAX_PATH] = {};
    const DWORD temporaryLength =
        GetTempPathW(MAX_PATH, temporaryPath);
    if (temporaryLength > 0 && temporaryLength < MAX_PATH)
    {
        logDirectory =
            std::filesystem::path(temporaryPath) / L"UGZhihuiLogs";
    }
    else
    {
        logDirectory = std::filesystem::current_path();
    }
    error.clear();
    std::filesystem::create_directories(logDirectory, error);
    return logDirectory / L"TiaoZenBanLeiCiCun.log";
}

std::filesystem::path ResolveSettingsPath()
{
    std::filesystem::path settingsDirectory;
    wchar_t modulePath[MAX_PATH] = {};
    const DWORD moduleLength = GetModuleFileNameW(
        reinterpret_cast<HMODULE>(&__ImageBase), modulePath, MAX_PATH);
    if (moduleLength > 0 && moduleLength < MAX_PATH)
    {
        const std::filesystem::path moduleDirectory =
            std::filesystem::path(modulePath).parent_path();
        settingsDirectory = moduleDirectory.parent_path() / L"config";
    }

    std::error_code error;
    if (!settingsDirectory.empty())
    {
        std::filesystem::create_directories(settingsDirectory, error);
        if (!error)
        {
            return settingsDirectory /
                   L"TiaoZenBanLeiCiCun.settings";
        }
    }

    wchar_t temporaryPath[MAX_PATH] = {};
    const DWORD temporaryLength =
        GetTempPathW(MAX_PATH, temporaryPath);
    settingsDirectory =
        temporaryLength > 0 && temporaryLength < MAX_PATH
            ? std::filesystem::path(temporaryPath) / L"UGZhihuiConfig"
            : std::filesystem::current_path();
    error.clear();
    std::filesystem::create_directories(settingsDirectory, error);
    return settingsDirectory / L"TiaoZenBanLeiCiCun.settings";
}

const std::filesystem::path& SettingsPath()
{
    static const std::filesystem::path path = ResolveSettingsPath();
    return path;
}

std::map<std::string, std::string> ReadSettings()
{
    std::map<std::string, std::string> values;
    std::ifstream input(SettingsPath(), std::ios::in | std::ios::binary);
    std::string line;
    while (std::getline(input, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        const std::size_t separator = line.find('=');
        if (separator == std::string::npos || separator == 0)
        {
            continue;
        }
        values[line.substr(0, separator)] =
            line.substr(separator + 1);
    }
    return values;
}

const std::filesystem::path& LogPath()
{
    static const std::filesystem::path path = []()
    {
        std::filesystem::path resolved = ResolveLogPath();
        std::error_code error;
        if (std::filesystem::exists(resolved, error) &&
            !error &&
            std::filesystem::file_size(resolved, error) >
                5ULL * 1024ULL * 1024ULL)
        {
            const std::filesystem::path backup =
                resolved.parent_path() /
                L"TiaoZenBanLeiCiCun.previous.log";
            error.clear();
            std::filesystem::remove(backup, error);
            error.clear();
            std::filesystem::rename(resolved, backup, error);
        }
        return resolved;
    }();
    return path;
}

std::mutex& LogMutex()
{
    static std::mutex mutex;
    return mutex;
}

std::string VectorText(const NXOpen::Vector3d& value)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(6)
           << "(" << value.X << ", " << value.Y << ", "
           << value.Z << ")";
    return stream.str();
}

std::string PointText(const NXOpen::Point3d& value)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(6)
           << "(" << value.X << ", " << value.Y << ", "
           << value.Z << ")";
    return stream.str();
}

bool FacePlaneData(NXOpen::Face* face,
                   NXOpen::Point3d& point,
                   NXOpen::Vector3d& normal)
{
    if (face == nullptr ||
        face->SolidFaceType() != NXOpen::Face::FaceTypePlanar)
    {
        return false;
    }

    int type = 0;
    double origin[3] = {};
    double direction[3] = {};
    double box[6] = {};
    double radius = 0.0;
    double radiusData = 0.0;
    int normalDirection = 1;
    if (UF_MODL_ask_face_data(face->Tag(), &type, origin, direction, box,
                              &radius, &radiusData,
                              &normalDirection) != 0)
    {
        return false;
    }

    point = NXOpen::Point3d(origin[0], origin[1], origin[2]);
    normal = NXOpen::Vector3d(
        direction[0] * normalDirection,
        direction[1] * normalDirection,
        direction[2] * normalDirection);
    return Normalize(normal);
}

bool FaceBox(NXOpen::Face* face, double box[6])
{
    if (face == nullptr || box == nullptr)
    {
        return false;
    }
    int type = 0;
    double origin[3] = {};
    double direction[3] = {};
    double radius = 0.0;
    double radiusData = 0.0;
    int normalDirection = 1;
    return UF_MODL_ask_face_data(
               face->Tag(), &type, origin, direction, box,
               &radius, &radiusData, &normalDirection) == 0;
}

bool FacesBoxCenter(const std::vector<NXOpen::Face*>& faces,
                    NXOpen::Point3d& center)
{
    double combined[6] = {
        std::numeric_limits<double>::max(),
        std::numeric_limits<double>::max(),
        std::numeric_limits<double>::max(),
        -std::numeric_limits<double>::max(),
        -std::numeric_limits<double>::max(),
        -std::numeric_limits<double>::max()};
    bool found = false;
    for (NXOpen::Face* face : faces)
    {
        double box[6] = {};
        if (!FaceBox(face, box))
        {
            continue;
        }
        combined[0] = (std::min)(combined[0], box[0]);
        combined[1] = (std::min)(combined[1], box[1]);
        combined[2] = (std::min)(combined[2], box[2]);
        combined[3] = (std::max)(combined[3], box[3]);
        combined[4] = (std::max)(combined[4], box[4]);
        combined[5] = (std::max)(combined[5], box[5]);
        found = true;
    }
    if (found)
    {
        center = NXOpen::Point3d(
            (combined[0] + combined[3]) * 0.5,
            (combined[1] + combined[4]) * 0.5,
            (combined[2] + combined[5]) * 0.5);
    }
    return found;
}

bool FaceProjectionRange(
    NXOpen::Face* face, const NXOpen::Point3d& origin,
    const NXOpen::Vector3d& direction,
    double& minimum, double& maximum)
{
    if (face == nullptr)
    {
        return false;
    }
    minimum = std::numeric_limits<double>::max();
    maximum = -std::numeric_limits<double>::max();
    bool found = false;
    for (NXOpen::Edge* edge : face->GetEdges())
    {
        if (edge == nullptr)
        {
            continue;
        }
        NXOpen::Point3d first;
        NXOpen::Point3d second;
        edge->GetVertices(&first, &second);
        for (const NXOpen::Point3d& point : {first, second})
        {
            const double coordinate =
                Dot(Subtract(point, origin), direction);
            minimum = (std::min)(minimum, coordinate);
            maximum = (std::max)(maximum, coordinate);
            found = true;
        }
    }
    return found;
}

bool IsAlive(tag_t tag)
{
    return tag != NULL_TAG && UF_OBJ_ask_status(tag) == UF_OBJ_ALIVE;
}
}

void TiaoZenWriteLog(const std::string& level,
                     const std::string& message) noexcept
{
    try
    {
        std::lock_guard<std::mutex> lock(LogMutex());
        SYSTEMTIME time = {};
        GetLocalTime(&time);
        std::ofstream output(
            LogPath(), std::ios::out | std::ios::app | std::ios::binary);
        if (!output)
        {
            return;
        }
        output << std::setfill('0')
               << std::setw(4) << time.wYear << "-"
               << std::setw(2) << time.wMonth << "-"
               << std::setw(2) << time.wDay << " "
               << std::setw(2) << time.wHour << ":"
               << std::setw(2) << time.wMinute << ":"
               << std::setw(2) << time.wSecond << "."
               << std::setw(3) << time.wMilliseconds
               << " [" << level << "]"
               << " [PID " << GetCurrentProcessId()
               << "/TID " << GetCurrentThreadId() << "] "
               << message << "\r\n";
        output.flush();
    }
    catch (...)
    {
    }
}

std::string TiaoZenLogFilePath() noexcept
{
    try
    {
        return LogPath().string();
    }
    catch (...)
    {
        return std::string();
    }
}

bool PanelSizeDialog::SideOffsets::IsZero(double tolerance) const
{
    return std::abs(left) <= tolerance &&
           std::abs(right) <= tolerance &&
           std::abs(bottom) <= tolerance &&
           std::abs(top) <= tolerance;
}

PanelSizeDialog::PanelSizeDialog()
    : session_(NXOpen::Session::GetSession()),
      ui_(NXOpen::UI::GetUI()),
      dialog_(nullptr),
      planeSelect_(nullptr),
      swapDirection_(nullptr),
      currentSize_(nullptr),
      adjustTabs_(nullptr),
      independentGroup_(nullptr),
      uniformGroup_(nullptr),
      roundGroup_(nullptr),
      topOffset_(nullptr),
      bottomOffset_(nullptr),
      leftOffset_(nullptr),
      rightOffset_(nullptr),
      targetLength_(nullptr),
      targetWidth_(nullptr),
      roundLength_(nullptr),
      roundWidth_(nullptr),
      lengthStep_(nullptr),
      widthStep_(nullptr),
      roundPolicy_(nullptr),
      anchor_(nullptr),
      anchorGroup_(nullptr),
      resultLength_(nullptr),
      resultWidth_(nullptr),
      livePreview_(nullptr),
      leftHandle_(nullptr),
      rightHandle_(nullptr),
      bottomHandle_(nullptr),
      topHandle_(nullptr),
      previewMark_(static_cast<NXOpen::Session::UndoMarkId>(0)),
      hasPreview_(false),
      rebuilding_(false),
      changingUi_(false),
      configuringHandles_(false),
      targetValuesInitialized_(false)
{
    TiaoZenWriteLog(
        "INFO",
        "创建板件调尺对话框；DLX=" + DialogPath() +
            "；日志=" + TiaoZenLogFilePath());
    dialog_ = ui_->CreateDialog(DialogPath().c_str());
    dialog_->AddInitializeHandler(
        NXOpen::make_callback(this, &PanelSizeDialog::initialize_cb));
    dialog_->AddDialogShownHandler(
        NXOpen::make_callback(this, &PanelSizeDialog::dialogShown_cb));
    dialog_->AddUpdateHandler(
        NXOpen::make_callback(this, &PanelSizeDialog::update_cb));
    dialog_->AddApplyHandler(
        NXOpen::make_callback(this, &PanelSizeDialog::apply_cb));
    dialog_->AddOkHandler(
        NXOpen::make_callback(this, &PanelSizeDialog::ok_cb));
    dialog_->AddCancelHandler(
        NXOpen::make_callback(this, &PanelSizeDialog::cancel_cb));
    dialog_->AddFilterHandler(
        NXOpen::make_callback(this, &PanelSizeDialog::filter_cb));
}

PanelSizeDialog::~PanelSizeDialog()
{
    TiaoZenWriteLog("INFO", "销毁板件调尺对话框。");
    UndoPreview();
    delete dialog_;
    dialog_ = nullptr;
}

NXOpen::BlockStyler::BlockDialog::DialogResponse PanelSizeDialog::Launch()
{
    return dialog_->Launch();
}

void PanelSizeDialog::initialize_cb()
{
    TiaoZenWriteLog("INFO", "initialize_cb 开始。");
    auto* top = dialog_->TopBlock();
    planeSelect_ = top->FindBlock("plane_select");
    swapDirection_ = top->FindBlock("swap_direction");
    currentSize_ = top->FindBlock("current_size");
    adjustTabs_ = dynamic_cast<NXOpen::BlockStyler::TabControl*>(
        top->FindBlock("adjust_tabs"));
    independentGroup_ = top->FindBlock("independent_group");
    uniformGroup_ = top->FindBlock("uniform_group");
    roundGroup_ = top->FindBlock("round_group");
    topOffset_ = top->FindBlock("top_offset");
    bottomOffset_ = top->FindBlock("bottom_offset");
    leftOffset_ = top->FindBlock("left_offset");
    rightOffset_ = top->FindBlock("right_offset");
    targetLength_ = top->FindBlock("target_length");
    targetWidth_ = top->FindBlock("target_width");
    roundLength_ = top->FindBlock("round_length");
    roundWidth_ = top->FindBlock("round_width");
    lengthStep_ = top->FindBlock("length_step");
    widthStep_ = top->FindBlock("width_step");
    roundPolicy_ = top->FindBlock("round_policy");
    anchor_ = top->FindBlock("anchor");
    anchorGroup_ = top->FindBlock("anchor_group");
    resultLength_ = top->FindBlock("result_length");
    resultWidth_ = top->FindBlock("result_width");
    livePreview_ = top->FindBlock("live_preview");
    leftHandle_ = dynamic_cast<NXOpen::BlockStyler::LinearDimension*>(
        top->FindBlock("left_handle"));
    rightHandle_ = dynamic_cast<NXOpen::BlockStyler::LinearDimension*>(
        top->FindBlock("right_handle"));
    bottomHandle_ = dynamic_cast<NXOpen::BlockStyler::LinearDimension*>(
        top->FindBlock("bottom_handle"));
    topHandle_ = dynamic_cast<NXOpen::BlockStyler::LinearDimension*>(
        top->FindBlock("top_handle"));

    const std::vector<NXOpen::BlockStyler::UIBlock*> required = {
        planeSelect_, swapDirection_, currentSize_, adjustTabs_,
        independentGroup_, uniformGroup_, roundGroup_,
        topOffset_, bottomOffset_, leftOffset_, rightOffset_,
        targetLength_, targetWidth_, roundLength_, roundWidth_,
        lengthStep_, widthStep_, roundPolicy_, anchor_, anchorGroup_,
        resultLength_, resultWidth_, livePreview_,
        leftHandle_, rightHandle_, bottomHandle_, topHandle_};
    if (std::find(required.begin(), required.end(), nullptr) != required.end())
    {
        throw std::runtime_error(
            "TiaoZenBanLeiCiCun.dlx 缺少必要控件，请重新生成对话框文件。");
    }

    NXOpen::BlockStyler::PropertyList* properties =
        planeSelect_->GetProperties();
    std::vector<NXOpen::Selection::MaskTriple> masks;
    masks.emplace_back(UF_solid_type, UF_solid_face_subtype,
                       UF_UI_SEL_FEATURE_PLANAR_FACE);
    properties->SetSelectionFilter(
        "SelectionFilter",
        NXOpen::Selection::SelectionActionClearAndEnableSpecific,
        masks);
    properties->SetEnum("StepStatus", 0);
    delete properties;

    LoadSettings();
    RefreshModeVisibility();
    RefreshDimensionText();
    TiaoZenWriteLog("INFO", "initialize_cb 完成，全部 DLX 控件已找到。");
}

void PanelSizeDialog::dialogShown_cb()
{
    TiaoZenWriteLog("INFO", "对话框已显示。");
    // The DLX creates the native dimension manipulators with ShowHandle=true,
    // matching feature 38. Hide them only after the dialog is fully realized;
    // otherwise NX 2412 defers creating them until an unrelated second update.
    HideDragHandles();
    UF_DISP_refresh();
    TiaoZenWriteLog(
        "DEBUG",
        "拖拽手柄已在对话框构建阶段完成实例化，等待选面后显示。");
    if (planeSelect_ != nullptr)
    {
        planeSelect_->Focus();
    }
}

int PanelSizeDialog::update_cb(NXOpen::BlockStyler::UIBlock* block)
{
    if (changingUi_ || rebuilding_ || configuringHandles_)
    {
        return 0;
    }

    try
    {
        TiaoZenWriteLog(
            "DEBUG",
            std::string("update_cb：block=") +
                (block != nullptr ? block->Name().GetText() : "<null>"));
        rebuilding_ = true;
        UndoPreview();

        const bool dragChanged = IsDragHandle(block);
        if (dragChanged)
        {
            ReadDragHandleValues();
        }

        if (block == planeSelect_ || block == swapDirection_)
        {
            NXOpen::Face* face = SelectedFace();
            if (face == nullptr)
            {
                TiaoZenWriteLog(
                    "INFO", "选择控件当前没有唯一有效平面，清空分析结果。");
                frame_ = PanelFrame();
            }
            else
            {
                std::string error;
                if (!AnalyzeSelectedFace(face, error))
                {
                    frame_ = PanelFrame();
                    ShowError(error);
                }
                else
                {
                    // A focused selection block keeps NX in object-picking
                    // mode and suppresses Block Styler dimension handles.
                    // Leave selection mode as soon as the single face has
                    // been accepted so the four drag handles paint now,
                    // without requiring a tab switch from the user.
                    adjustTabs_->Focus();
                    TiaoZenWriteLog(
                        "DEBUG",
                        "选面完成，焦点已移至调整方式，退出选面状态并显示拖拽手柄。");
                }
            }
        }

        RefreshModeVisibility();
        RefreshDimensionText();
        if (block == targetLength_ || block == targetWidth_)
        {
            targetValuesInitialized_ = true;
        }
        SaveSettings();

        if (frame_.valid && LivePreviewEnabled())
        {
            std::string error;
            if (!CreatePreview(error) && !error.empty())
            {
                ShowError(error);
            }
        }
        ConfigureDragHandles();
        rebuilding_ = false;
        TiaoZenWriteLog("DEBUG", "update_cb 完成。");
        return 0;
    }
    catch (const NXOpen::NXException& ex)
    {
        rebuilding_ = false;
        ShowError(ex.Message() != nullptr ? ex.Message()
                                         : "更新板件调尺预览失败。");
    }
    catch (const std::exception& ex)
    {
        rebuilding_ = false;
        ShowError(ex.what());
    }
    catch (...)
    {
        rebuilding_ = false;
        ShowError("更新板件调尺预览时发生未知错误。");
    }
    return 1;
}

int PanelSizeDialog::apply_cb()
{
    try
    {
        TiaoZenWriteLog("INFO", "apply_cb 开始。");
        SaveSettings();
        if (!frame_.valid || CachedFace() == nullptr)
        {
            ShowError("请先选择一个有效的板件平面。");
            return 1;
        }

        std::string error;
        const SideOffsets offsets = CalculateOffsets(error);
        if (!error.empty())
        {
            ShowError(error);
            return 1;
        }
        if (offsets.IsZero(kTolerance))
        {
            ShowError("当前设置没有产生尺寸变化。");
            return 1;
        }

        if (!hasPreview_ && !CreatePreview(error))
        {
            ShowError(error.empty() ? "创建尺寸调整失败。" : error);
            return 1;
        }

        CommitPreview();
        TiaoZenWriteLog("INFO", "尺寸调整已提交。");
        ClearSelection();
        frame_ = PanelFrame();
        SetLabel(currentSize_, "当前尺寸：请先选择板件平面");
        SetLabel(resultLength_, "调整后长度：--");
        SetLabel(resultWidth_, "调整后宽度：--");
        planeSelect_->Focus();
        return 0;
    }
    catch (const NXOpen::NXException& ex)
    {
        ShowError(ex.Message() != nullptr ? ex.Message()
                                         : "应用板件调尺失败。");
    }
    catch (const std::exception& ex)
    {
        ShowError(ex.what());
    }
    catch (...)
    {
        ShowError("应用板件调尺时发生未知错误。");
    }
    return 1;
}

int PanelSizeDialog::ok_cb()
{
    return apply_cb();
}

int PanelSizeDialog::cancel_cb()
{
    TiaoZenWriteLog("INFO", "用户取消对话框，准备撤销预览。");
    SaveSettings();
    return UndoPreview() ? 0 : 1;
}

int PanelSizeDialog::filter_cb(NXOpen::BlockStyler::UIBlock* block,
                               NXOpen::TaggedObject* selectedObject)
{
    if (block != planeSelect_)
    {
        return UF_UI_SEL_ACCEPT;
    }
    NXOpen::Face* face = dynamic_cast<NXOpen::Face*>(selectedObject);
    return face != nullptr &&
                   face->SolidFaceType() == NXOpen::Face::FaceTypePlanar
               ? UF_UI_SEL_ACCEPT
               : UF_UI_SEL_REJECT;
}

NXOpen::Face* PanelSizeDialog::SelectedFace() const
{
    if (planeSelect_ == nullptr)
    {
        return nullptr;
    }
    NXOpen::BlockStyler::PropertyList* properties =
        planeSelect_->GetProperties();
    const std::vector<NXOpen::TaggedObject*> objects =
        properties->GetTaggedObjectVector("SelectedObjects");
    delete properties;
    if (objects.size() != 1)
    {
        return nullptr;
    }
    NXOpen::Face* face = dynamic_cast<NXOpen::Face*>(objects.front());
    return face != nullptr &&
                   face->SolidFaceType() == NXOpen::Face::FaceTypePlanar
               ? face
               : nullptr;
}

NXOpen::Face* PanelSizeDialog::CachedFace() const
{
    if (!frame_.valid || !IsAlive(frame_.faceTag))
    {
        return nullptr;
    }
    return dynamic_cast<NXOpen::Face*>(
        NXOpen::NXObjectManager::Get(frame_.faceTag));
}

bool PanelSizeDialog::FindPanelDirections(
    NXOpen::Face* face, PanelFrame& frame, std::string& error) const
{
    NXOpen::Point3d planePoint;
    if (!FacePlaneData(face, planePoint, frame.normal))
    {
        error = "无法读取所选平面的法向。";
        return false;
    }
    frame.origin = planePoint;
    TiaoZenWriteLog(
        "DEBUG",
        "读取主平面：faceTag=" + std::to_string(face->Tag()) +
            "，origin=" + PointText(frame.origin) +
            "，normal=" + VectorText(frame.normal));

    double longest = 0.0;
    NXOpen::Vector3d candidate(0.0, 0.0, 0.0);
    for (NXOpen::Edge* edge : face->GetEdges())
    {
        if (edge == nullptr ||
            edge->SolidEdgeType() != NXOpen::Edge::EdgeTypeLinear)
        {
            continue;
        }
        NXOpen::Point3d first;
        NXOpen::Point3d second;
        edge->GetVertices(&first, &second);
        NXOpen::Vector3d direction = Subtract(second, first);
        const double length = Length(direction);
        if (length > longest && Normalize(direction))
        {
            const double normalComponent = Dot(direction, frame.normal);
            direction = NXOpen::Vector3d(
                direction.X - frame.normal.X * normalComponent,
                direction.Y - frame.normal.Y * normalComponent,
                direction.Z - frame.normal.Z * normalComponent);
            if (Normalize(direction))
            {
                longest = length;
                candidate = direction;
            }
        }
    }

    if (longest <= kTolerance)
    {
        error = "所选平面没有可用于识别长宽方向的直线边。";
        return false;
    }

    Canonicalize(candidate);
    NXOpen::Vector3d perpendicular = Cross(frame.normal, candidate);
    if (!Normalize(perpendicular))
    {
        error = "无法建立板件的宽度方向。";
        return false;
    }

    if (LogicalValue(swapDirection_))
    {
        frame.lengthDirection = perpendicular;
        frame.widthDirection = Scale(candidate, -1.0);
    }
    else
    {
        frame.lengthDirection = candidate;
        frame.widthDirection = perpendicular;
    }
    TiaoZenWriteLog(
        "INFO",
        "建立板件方向：最长直边=" + Fixed(longest) +
            "，swap=" +
            std::string(LogicalValue(swapDirection_) ? "true" : "false") +
            "，lengthDirection=" + VectorText(frame.lengthDirection) +
            "，widthDirection=" + VectorText(frame.widthDirection));
    return true;
}

bool PanelSizeDialog::AnalyzeBodyExtents(
    NXOpen::Body* body, PanelFrame& frame, std::string& error) const
{
    if (body == nullptr)
    {
        error = "所选平面不属于有效实体。";
        return false;
    }

    frame.minLength = std::numeric_limits<double>::max();
    frame.maxLength = -std::numeric_limits<double>::max();
    frame.minWidth = std::numeric_limits<double>::max();
    frame.maxWidth = -std::numeric_limits<double>::max();
    frame.minNormal = std::numeric_limits<double>::max();
    frame.maxNormal = -std::numeric_limits<double>::max();
    bool found = false;
    int edgeCount = 0;

    for (NXOpen::Edge* edge : body->GetEdges())
    {
        if (edge == nullptr)
        {
            continue;
        }
        ++edgeCount;
        NXOpen::Point3d first;
        NXOpen::Point3d second;
        edge->GetVertices(&first, &second);
        const NXOpen::Point3d points[2] = {first, second};
        for (const NXOpen::Point3d& point : points)
        {
            const NXOpen::Vector3d relative = Subtract(point, frame.origin);
            const double lengthCoordinate =
                Dot(relative, frame.lengthDirection);
            const double widthCoordinate =
                Dot(relative, frame.widthDirection);
            const double normalCoordinate =
                Dot(relative, frame.normal);
            frame.minLength =
                (std::min)(frame.minLength, lengthCoordinate);
            frame.maxLength =
                (std::max)(frame.maxLength, lengthCoordinate);
            frame.minWidth =
                (std::min)(frame.minWidth, widthCoordinate);
            frame.maxWidth =
                (std::max)(frame.maxWidth, widthCoordinate);
            frame.minNormal =
                (std::min)(frame.minNormal, normalCoordinate);
            frame.maxNormal =
                (std::max)(frame.maxNormal, normalCoordinate);
            found = true;
        }
    }

    if (!found || frame.Length() <= kTolerance ||
        frame.Width() <= kTolerance)
    {
        error = "无法从该实体计算有效的长宽尺寸。";
        return false;
    }
    TiaoZenWriteLog(
        "INFO",
        "实体投影范围：bodyTag=" + std::to_string(body->Tag()) +
            "，edgeCount=" + std::to_string(edgeCount) +
            "，minLength=" + Fixed(frame.minLength) +
            "，maxLength=" + Fixed(frame.maxLength) +
            "，minWidth=" + Fixed(frame.minWidth) +
            "，maxWidth=" + Fixed(frame.maxWidth) +
            "，length=" + Fixed(frame.Length()) +
            "，width=" + Fixed(frame.Width()) +
            "，normalDepth=" + Fixed(frame.NormalDepth()));
    return true;
}

bool PanelSizeDialog::AnalyzeFaceExtents(
    NXOpen::Face* face, PanelFrame& frame, std::string& error) const
{
    if (face == nullptr)
    {
        error = "无法读取主平面的裙边内轮廓。";
        return false;
    }
    frame.faceMinLength = std::numeric_limits<double>::max();
    frame.faceMaxLength = -std::numeric_limits<double>::max();
    frame.faceMinWidth = std::numeric_limits<double>::max();
    frame.faceMaxWidth = -std::numeric_limits<double>::max();
    bool found = false;
    for (NXOpen::Edge* edge : face->GetEdges())
    {
        if (edge == nullptr)
        {
            continue;
        }
        NXOpen::Point3d first;
        NXOpen::Point3d second;
        edge->GetVertices(&first, &second);
        for (const NXOpen::Point3d& point : {first, second})
        {
            const NXOpen::Vector3d relative =
                Subtract(point, frame.origin);
            const double lengthCoordinate =
                Dot(relative, frame.lengthDirection);
            const double widthCoordinate =
                Dot(relative, frame.widthDirection);
            frame.faceMinLength =
                (std::min)(frame.faceMinLength, lengthCoordinate);
            frame.faceMaxLength =
                (std::max)(frame.faceMaxLength, lengthCoordinate);
            frame.faceMinWidth =
                (std::min)(frame.faceMinWidth, widthCoordinate);
            frame.faceMaxWidth =
                (std::max)(frame.faceMaxWidth, widthCoordinate);
            found = true;
        }
    }
    if (!found)
    {
        error = "无法从主平面边界计算裙边宽度。";
        return false;
    }
    TiaoZenWriteLog(
        "INFO",
        "主平面内轮廓：minLength=" + Fixed(frame.faceMinLength) +
            "，maxLength=" + Fixed(frame.faceMaxLength) +
            "，minWidth=" + Fixed(frame.faceMinWidth) +
            "，maxWidth=" + Fixed(frame.faceMaxWidth));
    return true;
}

double PanelSizeDialog::EstimatePanelThickness(
    NXOpen::Body* body, const PanelFrame& frame) const
{
    if (body == nullptr)
    {
        return 0.0;
    }
    double thickness = std::numeric_limits<double>::max();
    for (NXOpen::Face* candidate : body->GetFaces())
    {
        if (candidate == nullptr || candidate->Tag() == frame.faceTag)
        {
            continue;
        }
        NXOpen::Point3d point;
        NXOpen::Vector3d normal;
        if (!FacePlaneData(candidate, point, normal) ||
            std::abs(Dot(normal, frame.normal)) < kParallelTolerance)
        {
            continue;
        }
        const double distance = std::abs(
            Dot(Subtract(point, frame.origin), frame.normal));
        if (distance > 1.0e-3)
        {
            thickness = (std::min)(thickness, distance);
        }
    }
    if (thickness == std::numeric_limits<double>::max())
    {
        return 0.0;
    }
    TiaoZenWriteLog(
        "INFO",
        "自动识别板厚：thickness=" + Fixed(thickness));
    return thickness;
}

bool PanelSizeDialog::ValidateSkirtDimensions(
    NXOpen::Body* body, std::string& error) const
{
    if (body == nullptr || !frame_.valid)
    {
        error = "无法校验裙边尺寸。";
        return false;
    }
    double minNormal = std::numeric_limits<double>::max();
    double maxNormal = -std::numeric_limits<double>::max();
    bool found = false;
    for (NXOpen::Edge* edge : body->GetEdges())
    {
        if (edge == nullptr)
        {
            continue;
        }
        NXOpen::Point3d first;
        NXOpen::Point3d second;
        edge->GetVertices(&first, &second);
        for (const NXOpen::Point3d& point : {first, second})
        {
            const double coordinate =
                Dot(Subtract(point, frame_.origin), frame_.normal);
            minNormal = (std::min)(minNormal, coordinate);
            maxNormal = (std::max)(maxNormal, coordinate);
            found = true;
        }
    }
    if (!found)
    {
        error = "调整后无法读取裙边尺寸。";
        return false;
    }

    const double tolerance =
        (std::max)(1.0e-3, frame_.NormalDepth() * 1.0e-6);
    const double minChange = minNormal - frame_.minNormal;
    const double maxChange = maxNormal - frame_.maxNormal;
    const double adjustedThickness =
        EstimatePanelThickness(body, frame_);
    PanelFrame adjustedFrame = frame_;
    std::string measurementError;
    NXOpen::Face* adjustedMainFace = CachedFace();
    if (!AnalyzeBodyExtents(
            body, adjustedFrame, measurementError) ||
        !AnalyzeFaceExtents(
            adjustedMainFace, adjustedFrame, measurementError))
    {
        error =
            "调整后无法复测裙边宽度：" + measurementError;
        return false;
    }
    const std::array<double, 4> originalSkirts{
        frame_.LeftSkirt(), frame_.RightSkirt(),
        frame_.BottomSkirt(), frame_.TopSkirt()};
    const std::array<double, 4> adjustedSkirts{
        adjustedFrame.LeftSkirt(), adjustedFrame.RightSkirt(),
        adjustedFrame.BottomSkirt(), adjustedFrame.TopSkirt()};
    bool skirtWidthChanged = false;
    for (std::size_t index = 0; index < originalSkirts.size(); ++index)
    {
        if (std::abs(adjustedSkirts[index] - originalSkirts[index]) >
            tolerance)
        {
            skirtWidthChanged = true;
        }
    }
    TiaoZenWriteLog(
        "INFO",
        "裙边尺寸校验：beforeMin=" + Fixed(frame_.minNormal) +
            "，afterMin=" + Fixed(minNormal) +
            "，beforeMax=" + Fixed(frame_.maxNormal) +
            "，afterMax=" + Fixed(maxNormal) +
            "，beforeDepth=" + Fixed(frame_.NormalDepth()) +
            "，afterDepth=" + Fixed(maxNormal - minNormal) +
            "，beforeThickness=" + Fixed(frame_.thickness) +
            "，afterThickness=" + Fixed(adjustedThickness) +
            "，beforeSkirt(L/R/B/T)=" +
            Fixed(originalSkirts[0]) + "/" +
            Fixed(originalSkirts[1]) + "/" +
            Fixed(originalSkirts[2]) + "/" +
            Fixed(originalSkirts[3]) +
            "，afterSkirt(L/R/B/T)=" +
            Fixed(adjustedSkirts[0]) + "/" +
            Fixed(adjustedSkirts[1]) + "/" +
            Fixed(adjustedSkirts[2]) + "/" +
            Fixed(adjustedSkirts[3]) +
            "，tolerance=" + Number(tolerance));
    if (std::abs(minChange) > tolerance ||
        std::abs(maxChange) > tolerance ||
        skirtWidthChanged ||
        (frame_.thickness > tolerance &&
         std::abs(adjustedThickness - frame_.thickness) > tolerance))
    {
        error =
            "本次调整会改变原裙边宽度、高度或板厚，已拒绝该结果。"
            "请检查该侧是否存在未识别的裙边面。";
        return false;
    }
    return true;
}

bool PanelSizeDialog::AnalyzeSelectedFace(
    NXOpen::Face* face, std::string& error)
{
    TiaoZenWriteLog(
        "INFO",
        "开始分析所选主平面：faceTag=" +
            std::to_string(face != nullptr ? face->Tag() : NULL_TAG));
    PanelFrame analyzed;
    if (face == nullptr ||
        face->SolidFaceType() != NXOpen::Face::FaceTypePlanar)
    {
        error = "请选择一个平面。";
        return false;
    }
    if (!FindPanelDirections(face, analyzed, error))
    {
        return false;
    }
    NXOpen::Body* body = face->GetBody();
    if (!AnalyzeBodyExtents(body, analyzed, error))
    {
        return false;
    }
    if (!AnalyzeFaceExtents(face, analyzed, error))
    {
        return false;
    }

    analyzed.faceTag = face->Tag();
    analyzed.bodyTag = body->Tag();
    analyzed.thickness = EstimatePanelThickness(body, analyzed);
    analyzed.valid = true;
    frame_ = analyzed;
    TiaoZenWriteLog(
        "INFO",
        "主平面分析成功：faceTag=" + std::to_string(frame_.faceTag) +
            "，bodyTag=" + std::to_string(frame_.bodyTag) +
            "，当前长度=" + Fixed(frame_.Length()) +
            "，当前宽度=" + Fixed(frame_.Width()) +
            "，裙边(左/右/下/上)=" +
            Fixed(frame_.LeftSkirt()) + "/" +
            Fixed(frame_.RightSkirt()) + "/" +
            Fixed(frame_.BottomSkirt()) + "/" +
            Fixed(frame_.TopSkirt()) +
            "，板厚=" + Fixed(frame_.thickness));

    if (!targetValuesInitialized_)
    {
        changingUi_ = true;
        SetDoubleValue(targetLength_, frame_.Length());
        SetDoubleValue(targetWidth_, frame_.Width());
        changingUi_ = false;
        targetValuesInitialized_ = true;
    }
    return true;
}

PanelSizeDialog::AdjustMode PanelSizeDialog::CurrentMode() const
{
    const int value = adjustTabs_->ActivePage();
    return static_cast<AdjustMode>((std::max)(0, (std::min)(2, value)));
}

PanelSizeDialog::RoundPolicy PanelSizeDialog::CurrentRoundPolicy() const
{
    NXOpen::BlockStyler::PropertyList* properties =
        roundPolicy_->GetProperties();
    const int value = properties->GetEnum("Value");
    delete properties;
    return static_cast<RoundPolicy>((std::max)(0, (std::min)(2, value)));
}

int PanelSizeDialog::CurrentAnchor() const
{
    NXOpen::BlockStyler::PropertyList* properties = anchor_->GetProperties();
    const int value = properties->GetEnum("Value");
    delete properties;
    return (std::max)(0, (std::min)(8, value));
}

bool PanelSizeDialog::LivePreviewEnabled() const
{
    return LogicalValue(livePreview_);
}

double PanelSizeDialog::DoubleValue(
    NXOpen::BlockStyler::UIBlock* block) const
{
    NXOpen::BlockStyler::PropertyList* properties = block->GetProperties();
    const double value = properties->GetDouble("Value");
    delete properties;
    return value;
}

bool PanelSizeDialog::LogicalValue(
    NXOpen::BlockStyler::UIBlock* block) const
{
    NXOpen::BlockStyler::PropertyList* properties = block->GetProperties();
    const bool value = properties->GetLogical("Value");
    delete properties;
    return value;
}

void PanelSizeDialog::SetDoubleValue(
    NXOpen::BlockStyler::UIBlock* block, double value)
{
    NXOpen::BlockStyler::PropertyList* properties = block->GetProperties();
    properties->SetDouble("Value", value);
    delete properties;
}

void PanelSizeDialog::SetLogicalValue(
    NXOpen::BlockStyler::UIBlock* block, bool value)
{
    NXOpen::BlockStyler::PropertyList* properties = block->GetProperties();
    properties->SetLogical("Value", value);
    delete properties;
}

void PanelSizeDialog::SetEnumValue(
    NXOpen::BlockStyler::UIBlock* block, int value)
{
    NXOpen::BlockStyler::PropertyList* properties = block->GetProperties();
    properties->SetEnum("Value", value);
    delete properties;
}

void PanelSizeDialog::SetLabel(
    NXOpen::BlockStyler::UIBlock* block, const std::string& value)
{
    NXOpen::BlockStyler::PropertyList* properties = block->GetProperties();
    properties->SetString("Label", value.c_str());
    delete properties;
}

void PanelSizeDialog::ClearSelection()
{
    NXOpen::BlockStyler::PropertyList* properties =
        planeSelect_->GetProperties();
    changingUi_ = true;
    properties->SetTaggedObjectVector(
        "SelectedObjects", std::vector<NXOpen::TaggedObject*>());
    changingUi_ = false;
    delete properties;
    if (IsAlive(frame_.faceTag))
    {
        UF_DISP_set_highlight(frame_.faceTag, 0);
    }
    HideDragHandles();
}

void PanelSizeDialog::LoadSettings()
{
    const std::map<std::string, std::string> values = ReadSettings();
    if (values.empty())
    {
        TiaoZenWriteLog(
            "INFO",
            "没有已保存的用户参数，使用 DLX 默认值；settings=" +
                SettingsPath().string());
        return;
    }

    const auto readInt =
        [&values](const char* key, int fallback)
    {
        const auto item = values.find(key);
        if (item == values.end())
        {
            return fallback;
        }
        try
        {
            return std::stoi(item->second);
        }
        catch (...)
        {
            return fallback;
        }
    };
    const auto readDouble =
        [&values](const char* key, double fallback)
    {
        const auto item = values.find(key);
        if (item == values.end())
        {
            return fallback;
        }
        try
        {
            return std::stod(item->second);
        }
        catch (...)
        {
            return fallback;
        }
    };

    changingUi_ = true;
    adjustTabs_->SetActivePage(
        (std::max)(0, (std::min)(2, readInt("mode", 0))));
    SetLogicalValue(
        swapDirection_, readInt("swapDirection", 0) != 0);
    SetDoubleValue(
        leftOffset_, readDouble("leftOffset", 0.0));
    SetDoubleValue(
        rightOffset_, readDouble("rightOffset", 0.0));
    SetDoubleValue(
        bottomOffset_, readDouble("bottomOffset", 0.0));
    SetDoubleValue(
        topOffset_, readDouble("topOffset", 0.0));
    SetLogicalValue(
        roundLength_, readInt("roundLength", 1) != 0);
    SetLogicalValue(
        roundWidth_, readInt("roundWidth", 1) != 0);
    SetDoubleValue(
        lengthStep_, readDouble("lengthStep", 10.0));
    SetDoubleValue(
        widthStep_, readDouble("widthStep", 10.0));
    SetEnumValue(
        roundPolicy_,
        (std::max)(0, (std::min)(2, readInt("roundPolicy", 0))));
    SetEnumValue(
        anchor_,
        (std::max)(0, (std::min)(8, readInt("anchor", 4))));
    SetLogicalValue(
        livePreview_, readInt("livePreview", 1) != 0);

    const bool hasTargetLength =
        values.find("targetLength") != values.end();
    const bool hasTargetWidth =
        values.find("targetWidth") != values.end();
    if (hasTargetLength && hasTargetWidth)
    {
        SetDoubleValue(
            targetLength_, readDouble("targetLength", 0.0));
        SetDoubleValue(
            targetWidth_, readDouble("targetWidth", 0.0));
        targetValuesInitialized_ = true;
    }
    changingUi_ = false;
    TiaoZenWriteLog(
        "INFO",
        "已恢复用户参数：mode=" +
            std::to_string(static_cast<int>(CurrentMode())) +
            "，left=" + Fixed(DoubleValue(leftOffset_)) +
            "，right=" + Fixed(DoubleValue(rightOffset_)) +
            "，bottom=" + Fixed(DoubleValue(bottomOffset_)) +
            "，top=" + Fixed(DoubleValue(topOffset_)) +
            "，settings=" + SettingsPath().string());
}

void PanelSizeDialog::SaveSettings() const
{
    if (adjustTabs_ == nullptr || leftOffset_ == nullptr)
    {
        return;
    }
    try
    {
        const std::filesystem::path temporary =
            SettingsPath().parent_path() /
            L"TiaoZenBanLeiCiCun.settings.tmp";
        std::ofstream output(
            temporary, std::ios::out | std::ios::trunc |
                           std::ios::binary);
        output << std::setprecision(17)
               << "mode=" << static_cast<int>(CurrentMode()) << "\n"
               << "swapDirection=" << (LogicalValue(swapDirection_) ? 1 : 0) << "\n"
               << "leftOffset=" << DoubleValue(leftOffset_) << "\n"
               << "rightOffset=" << DoubleValue(rightOffset_) << "\n"
               << "bottomOffset=" << DoubleValue(bottomOffset_) << "\n"
               << "topOffset=" << DoubleValue(topOffset_) << "\n"
               << "targetLength=" << DoubleValue(targetLength_) << "\n"
               << "targetWidth=" << DoubleValue(targetWidth_) << "\n"
               << "roundLength=" << (LogicalValue(roundLength_) ? 1 : 0) << "\n"
               << "roundWidth=" << (LogicalValue(roundWidth_) ? 1 : 0) << "\n"
               << "lengthStep=" << DoubleValue(lengthStep_) << "\n"
               << "widthStep=" << DoubleValue(widthStep_) << "\n"
               << "roundPolicy=" << static_cast<int>(CurrentRoundPolicy()) << "\n"
               << "anchor=" << CurrentAnchor() << "\n"
               << "livePreview=" << (LivePreviewEnabled() ? 1 : 0) << "\n";
        output.close();
        if (!output)
        {
            throw std::runtime_error("写入参数临时文件失败。");
        }
        if (!MoveFileExW(
                temporary.c_str(), SettingsPath().c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            throw std::runtime_error(
                "替换参数文件失败，Win32Error=" +
                std::to_string(GetLastError()));
        }
        TiaoZenWriteLog(
            "DEBUG",
            "用户参数已保存：settings=" +
                SettingsPath().string());
    }
    catch (const std::exception& ex)
    {
        TiaoZenWriteLog(
            "ERROR",
            std::string("保存用户参数失败：") + ex.what());
    }
}

double PanelSizeDialog::RoundDimension(
    double value, double step, RoundPolicy policy) const
{
    if (step <= kTolerance)
    {
        return value;
    }
    const double units = value / step;
    switch (policy)
    {
    case RoundPolicy::Up:
        return std::ceil(units - kTolerance) * step;
    case RoundPolicy::Down:
        return std::floor(units + kTolerance) * step;
    case RoundPolicy::Nearest:
    default:
        return std::round(units) * step;
    }
}

void PanelSizeDialog::TargetSizeToOffsets(
    double targetLength, double targetWidth, SideOffsets& offsets) const
{
    const double lengthDelta = targetLength - frame_.Length();
    const double widthDelta = targetWidth - frame_.Width();
    const int anchor = CurrentAnchor();
    const int row = anchor / 3;
    const int column = anchor % 3;

    if (column == 0)
    {
        offsets.right = lengthDelta;
    }
    else if (column == 1)
    {
        offsets.left = lengthDelta * 0.5;
        offsets.right = lengthDelta * 0.5;
    }
    else
    {
        offsets.left = lengthDelta;
    }

    if (row == 0)
    {
        offsets.bottom = widthDelta;
    }
    else if (row == 1)
    {
        offsets.bottom = widthDelta * 0.5;
        offsets.top = widthDelta * 0.5;
    }
    else
    {
        offsets.top = widthDelta;
    }
}

PanelSizeDialog::SideOffsets PanelSizeDialog::CalculateOffsets(
    std::string& error) const
{
    SideOffsets offsets;
    if (!frame_.valid)
    {
        TiaoZenWriteLog(
            "DEBUG", "CalculateOffsets：frame 无效，返回零偏移。");
        return offsets;
    }

    const AdjustMode mode = CurrentMode();
    switch (mode)
    {
    case AdjustMode::Independent:
        offsets.left = DoubleValue(leftOffset_);
        offsets.right = DoubleValue(rightOffset_);
        offsets.bottom = DoubleValue(bottomOffset_);
        offsets.top = DoubleValue(topOffset_);
        break;

    case AdjustMode::TargetSize:
        TargetSizeToOffsets(DoubleValue(targetLength_),
                            DoubleValue(targetWidth_), offsets);
        break;

    case AdjustMode::Round:
    {
        double targetLength = frame_.Length();
        double targetWidth = frame_.Width();
        if (LogicalValue(roundLength_))
        {
            const double step = DoubleValue(lengthStep_);
            if (step <= kTolerance)
            {
                error = "长度取整步长必须大于零。";
                return offsets;
            }
            targetLength =
                RoundDimension(targetLength, step, CurrentRoundPolicy());
        }
        if (LogicalValue(roundWidth_))
        {
            const double step = DoubleValue(widthStep_);
            if (step <= kTolerance)
            {
                error = "宽度取整步长必须大于零。";
                return offsets;
            }
            targetWidth =
                RoundDimension(targetWidth, step, CurrentRoundPolicy());
        }
        if (!LogicalValue(roundLength_) && !LogicalValue(roundWidth_))
        {
            error = "请至少选择一个需要取整的方向。";
            return offsets;
        }
        TargetSizeToOffsets(targetLength, targetWidth, offsets);
        break;
    }
    }

    if (!ValidateOffsets(offsets, error))
    {
        TiaoZenWriteLog(
            "ERROR", "偏移校验失败：" + error);
        return SideOffsets();
    }
    TiaoZenWriteLog(
        "DEBUG",
        "计算偏移：mode=" +
            std::to_string(static_cast<int>(mode)) +
            "，anchor=" + std::to_string(CurrentAnchor()) +
            "，left=" + SignedFixed(offsets.left) +
            "，right=" + SignedFixed(offsets.right) +
            "，bottom=" + SignedFixed(offsets.bottom) +
            "，top=" + SignedFixed(offsets.top) +
            "，resultLength=" +
            Fixed(frame_.Length() + offsets.left + offsets.right) +
            "，resultWidth=" +
            Fixed(frame_.Width() + offsets.bottom + offsets.top));
    return offsets;
}

bool PanelSizeDialog::ValidateOffsets(
    const SideOffsets& offsets, std::string& error) const
{
    const double newLength =
        frame_.Length() + offsets.left + offsets.right;
    const double newWidth =
        frame_.Width() + offsets.bottom + offsets.top;
    if (newLength <= kTolerance)
    {
        error = "调整后的长度必须大于零。";
        return false;
    }
    if (newWidth <= kTolerance)
    {
        error = "调整后的宽度必须大于零。";
        return false;
    }
    return true;
}

void PanelSizeDialog::RefreshModeVisibility()
{
    const AdjustMode mode = CurrentMode();
    anchorGroup_->SetEnable(mode != AdjustMode::Independent);
    lengthStep_->SetEnable(LogicalValue(roundLength_));
    widthStep_->SetEnable(LogicalValue(roundWidth_));
}

bool PanelSizeDialog::IsDragHandle(
    NXOpen::BlockStyler::UIBlock* block) const
{
    return block == leftHandle_ || block == rightHandle_ ||
           block == bottomHandle_ || block == topHandle_;
}

void PanelSizeDialog::HideDragHandles()
{
    const std::array<NXOpen::BlockStyler::LinearDimension*, 4> handles{
        leftHandle_, rightHandle_, bottomHandle_, topHandle_};
    configuringHandles_ = true;
    for (NXOpen::BlockStyler::LinearDimension* handle : handles)
    {
        if (handle != nullptr)
        {
            handle->SetShowHandle(false);
            handle->SetShowFocusHandle(false);
        }
    }
    configuringHandles_ = false;
}

void PanelSizeDialog::ReadDragHandleValues()
{
    if (leftHandle_ == nullptr || rightHandle_ == nullptr ||
        bottomHandle_ == nullptr || topHandle_ == nullptr)
    {
        return;
    }
    changingUi_ = true;
    SetDoubleValue(leftOffset_, leftHandle_->Value());
    SetDoubleValue(rightOffset_, rightHandle_->Value());
    SetDoubleValue(bottomOffset_, bottomHandle_->Value());
    SetDoubleValue(topOffset_, topHandle_->Value());
    changingUi_ = false;
    TiaoZenWriteLog(
        "DEBUG",
        "读取拖拽手柄：left=" + SignedFixed(DoubleValue(leftOffset_)) +
            "，right=" + SignedFixed(DoubleValue(rightOffset_)) +
            "，bottom=" + SignedFixed(DoubleValue(bottomOffset_)) +
            "，top=" + SignedFixed(DoubleValue(topOffset_)));
}

void PanelSizeDialog::SetDragHandleValues(const SideOffsets& offsets)
{
    const std::array<NXOpen::BlockStyler::LinearDimension*, 4> handles{
        leftHandle_, rightHandle_, bottomHandle_, topHandle_};
    const std::array<double, 4> values{
        offsets.left, offsets.right, offsets.bottom, offsets.top};
    for (std::size_t index = 0; index < handles.size(); ++index)
    {
        NXOpen::BlockStyler::LinearDimension* handle = handles[index];
        if (handle == nullptr)
        {
            continue;
        }
        handle->SetFormula(Number(values[index]).c_str());
        handle->SetValue(values[index]);
    }
}

void PanelSizeDialog::ConfigureDragHandles()
{
    if (!frame_.valid || CurrentMode() != AdjustMode::Independent)
    {
        HideDragHandles();
        return;
    }

    const double middleLength =
        (frame_.minLength + frame_.maxLength) * 0.5;
    const double middleWidth =
        (frame_.minWidth + frame_.maxWidth) * 0.5;
    const auto pointAt =
        [this](double lengthCoordinate, double widthCoordinate)
    {
        return Move(
            Move(frame_.origin, frame_.lengthDirection, lengthCoordinate),
            frame_.widthDirection, widthCoordinate);
    };

    std::array<NXOpen::Point3d, 4> origins{
        pointAt(frame_.minLength, middleWidth),
        pointAt(frame_.maxLength, middleWidth),
        pointAt(middleLength, frame_.minWidth),
        pointAt(middleLength, frame_.maxWidth)};
    const std::array<NXOpen::Vector3d, 4> directions{
        Scale(frame_.lengthDirection, -1.0),
        frame_.lengthDirection,
        Scale(frame_.widthDirection, -1.0),
        frame_.widthDirection};

    NXOpen::Body* body = IsAlive(frame_.bodyTag)
                             ? dynamic_cast<NXOpen::Body*>(
                                   NXOpen::NXObjectManager::Get(frame_.bodyTag))
                             : nullptr;
    if (body != nullptr)
    {
        const std::array<double, 4> boundaryCoordinates{
            -frame_.minLength, frame_.maxLength,
            -frame_.minWidth, frame_.maxWidth};
        for (std::size_t index = 0; index < origins.size(); ++index)
        {
            const std::vector<NXOpen::Face*> movingFaces =
                FindBoundaryFaces(
                    body, directions[index], boundaryCoordinates[index]);
            NXOpen::Point3d movingFaceCenter;
            if (FacesBoxCenter(movingFaces, movingFaceCenter))
            {
                origins[index] = movingFaceCenter;
                TiaoZenWriteLog(
                    "DEBUG",
                    "拖拽手柄位于实际拉动面中心：index=" +
                        std::to_string(index) +
                        "，faceCount=" +
                        std::to_string(movingFaces.size()) +
                        "，origin=" + PointText(movingFaceCenter));
            }
        }
    }
    const SideOffsets offsets{
        DoubleValue(leftOffset_), DoubleValue(rightOffset_),
        DoubleValue(bottomOffset_), DoubleValue(topOffset_)};
    const std::array<NXOpen::BlockStyler::LinearDimension*, 4> handles{
        leftHandle_, rightHandle_, bottomHandle_, topHandle_};

    configuringHandles_ = true;
    try
    {
        SetDragHandleValues(offsets);
        for (std::size_t index = 0; index < handles.size(); ++index)
        {
            NXOpen::BlockStyler::LinearDimension* handle = handles[index];
            handle->SetAutoReverseDuringDrag(true);
            handle->SetShowFocusHandle(false);
            handle->SetHandleOrigin(origins[index]);
            handle->SetHandleOrientation(directions[index]);
            handle->SetShowHandle(true);
        }
    }
    catch (...)
    {
        configuringHandles_ = false;
        throw;
    }
    configuringHandles_ = false;
    UF_DISP_refresh();
}

void PanelSizeDialog::RefreshDimensionText()
{
    if (!frame_.valid)
    {
        SetLabel(currentSize_, "当前尺寸：请先选择板件平面");
        SetLabel(resultLength_, "调整后长度：--");
        SetLabel(resultWidth_, "调整后宽度：--");
        return;
    }

    SetLabel(currentSize_,
             "当前尺寸：长 " + Fixed(frame_.Length()) +
                 " mm    宽 " + Fixed(frame_.Width()) + " mm");

    std::string error;
    const SideOffsets offsets = CalculateOffsets(error);
    if (!error.empty())
    {
        SetLabel(resultLength_, "调整结果无效：" + error);
        SetLabel(resultWidth_, "");
        return;
    }
    const double newLength =
        frame_.Length() + offsets.left + offsets.right;
    const double newWidth =
        frame_.Width() + offsets.bottom + offsets.top;
    SetLabel(resultLength_,
             "调整后长度：" + Fixed(newLength) +
                 " mm    变化 " +
                 SignedFixed(newLength - frame_.Length()));
    SetLabel(resultWidth_,
             "调整后宽度：" + Fixed(newWidth) +
                 " mm    变化 " +
                 SignedFixed(newWidth - frame_.Width()));
}

void PanelSizeDialog::RefreshPreview()
{
    if (!frame_.valid || !LivePreviewEnabled())
    {
        return;
    }
    std::string error;
    if (!CreatePreview(error) && !error.empty())
    {
        ShowError(error);
    }
}

std::vector<NXOpen::Face*> PanelSizeDialog::FindBoundaryFaces(
    NXOpen::Body* body,
    const NXOpen::Vector3d& outwardDirection,
    double boundaryCoordinate) const
{
    std::vector<NXOpen::Face*> result;
    std::vector<std::pair<NXOpen::Face*, double>> candidates;
    const double dimension =
        (std::max)(frame_.Length(), frame_.Width());
    const double coordinateTolerance =
        (std::max)(1.0e-3, dimension * 1.0e-6);
    double skirtInset = 0.0;
    const double lengthAlignment =
        Dot(outwardDirection, frame_.lengthDirection);
    const double widthAlignment =
        Dot(outwardDirection, frame_.widthDirection);
    if (lengthAlignment < -kParallelTolerance)
    {
        skirtInset = frame_.LeftSkirt();
    }
    else if (lengthAlignment > kParallelTolerance)
    {
        skirtInset = frame_.RightSkirt();
    }
    else if (widthAlignment < -kParallelTolerance)
    {
        skirtInset = frame_.BottomSkirt();
    }
    else if (widthAlignment > kParallelTolerance)
    {
        skirtInset = frame_.TopSkirt();
    }
    skirtInset = (std::max)(0.0, skirtInset);
    const double selectionBand =
        skirtInset +
        (frame_.thickness > coordinateTolerance
             ? frame_.thickness * 2.25
             : 0.0) +
        coordinateTolerance;
    double sideCenterCoordinate = 0.0;
    if (lengthAlignment < -kParallelTolerance)
    {
        sideCenterCoordinate =
            -(frame_.minLength + frame_.maxLength) * 0.5;
    }
    else if (lengthAlignment > kParallelTolerance)
    {
        sideCenterCoordinate =
            (frame_.minLength + frame_.maxLength) * 0.5;
    }
    else if (widthAlignment < -kParallelTolerance)
    {
        sideCenterCoordinate =
            -(frame_.minWidth + frame_.maxWidth) * 0.5;
    }
    else if (widthAlignment > kParallelTolerance)
    {
        sideCenterCoordinate =
            (frame_.minWidth + frame_.maxWidth) * 0.5;
    }
    const bool skirtTowardMaximum =
        std::abs(frame_.maxNormal) >=
        std::abs(frame_.minNormal);
    const double skirtExtreme =
        skirtTowardMaximum
            ? frame_.maxNormal
            : frame_.minNormal;
    const double skirtHeightTolerance =
        (std::max)(1.0e-3, frame_.NormalDepth() * 1.0e-5);
    const auto belongsToSkirt =
        [this, sideCenterCoordinate, coordinateTolerance,
         skirtTowardMaximum, skirtExtreme,
         skirtHeightTolerance](
            NXOpen::Face* face, double sideCoordinate)
    {
        if (sideCoordinate <
            sideCenterCoordinate - coordinateTolerance)
        {
            return false;
        }
        double minimum = 0.0;
        double maximum = 0.0;
        if (!FaceProjectionRange(
                face, frame_.origin, frame_.normal,
                minimum, maximum))
        {
            return false;
        }
        return skirtTowardMaximum
                   ? std::abs(maximum - skirtExtreme) <=
                         skirtHeightTolerance
                   : std::abs(minimum - skirtExtreme) <=
                         skirtHeightTolerance;
    };
    TiaoZenWriteLog(
        "DEBUG",
        "开始搜索边界面：bodyTag=" + std::to_string(body->Tag()) +
            "，direction=" + VectorText(outwardDirection) +
            "，boundaryCoordinate=" + Fixed(boundaryCoordinate) +
            "，coordinateTolerance=" +
            Number(coordinateTolerance));

    for (NXOpen::Face* face : body->GetFaces())
    {
        NXOpen::Point3d point;
        NXOpen::Vector3d normal;
        if (!FacePlaneData(face, point, normal) ||
            std::abs(Dot(normal, outwardDirection)) <
                kParallelTolerance)
        {
            continue;
        }
        const NXOpen::Vector3d relative = Subtract(point, frame_.origin);
        const double coordinate = Dot(relative, outwardDirection);
        candidates.emplace_back(face, coordinate);
        TiaoZenWriteLog(
            "TRACE",
            "平行平面候选：faceTag=" + std::to_string(face->Tag()) +
                "，point=" + PointText(point) +
                "，normal=" + VectorText(normal) +
                "，coordinate=" + Fixed(coordinate) +
                "，boundaryDelta=" +
                SignedFixed(coordinate - boundaryCoordinate));
        if (std::abs(coordinate - boundaryCoordinate) <=
            coordinateTolerance)
        {
            result.push_back(face);
        }
    }

    if (!result.empty())
    {
        result.clear();
        int skirtFaceCount = 0;
        for (const auto& candidate : candidates)
        {
            const double inwardDistance =
                boundaryCoordinate - candidate.second;
            if (inwardDistance >= -coordinateTolerance &&
                (inwardDistance <= selectionBand ||
                 belongsToSkirt(
                     candidate.first, candidate.second)))
            {
                result.push_back(candidate.first);
                if (inwardDistance > selectionBand)
                {
                    ++skirtFaceCount;
                }
            }
        }
        TiaoZenWriteLog(
            "INFO",
            "边界外侧面和内侧面匹配完成：parallelCandidates=" +
                std::to_string(candidates.size()) +
                "，thickness=" + Fixed(frame_.thickness) +
                "，skirtInset=" + Fixed(skirtInset) +
                "，selectionBand=" + Fixed(selectionBand) +
                "，skirtTopologyFaces=" +
                std::to_string(skirtFaceCount) +
                "，selectedCount=" + std::to_string(result.size()));
        return result;
    }
    if (candidates.empty())
    {
        TiaoZenWriteLog(
            "WARN", "没有找到与边界方向平行的平面。");
        return result;
    }

    // A folded sheet-metal boundary is often formed by a cylindrical bend
    // whose extreme point lies slightly outside the nearest planar flange
    // end face. In that case the body extent and the movable planar face do
    // not share exactly the same coordinate. Fall back to the outermost
    // parallel planar face, but only when it remains close enough to the
    // measured body boundary to avoid selecting a remote hole wall.
    double outermostCandidate = -std::numeric_limits<double>::max();
    for (const auto& candidate : candidates)
    {
        outermostCandidate =
            (std::max)(outermostCandidate, candidate.second);
    }
    const double maximumFallbackInset =
        (std::max)(10.0, dimension * 5.0e-3);
    const double inset = boundaryCoordinate - outermostCandidate;
    TiaoZenWriteLog(
        "WARN",
        "精确匹配为空，评估回退：outermostCandidate=" +
            Fixed(outermostCandidate) +
            "，inset=" + Fixed(inset) +
            "，maximumFallbackInset=" +
            Fixed(maximumFallbackInset));
    if (inset < -coordinateTolerance ||
        inset > maximumFallbackInset)
    {
        TiaoZenWriteLog(
            "ERROR",
            "回退候选超出安全范围，拒绝选择边界面。");
        return result;
    }

    int skirtFaceCount = 0;
    for (const auto& candidate : candidates)
    {
        const double inwardDistance =
            outermostCandidate - candidate.second;
        if (inwardDistance >= -coordinateTolerance &&
            (inwardDistance <= selectionBand ||
             belongsToSkirt(
                 candidate.first, candidate.second)))
        {
            result.push_back(candidate.first);
            if (inwardDistance > selectionBand)
            {
                ++skirtFaceCount;
            }
        }
    }
    std::ostringstream selected;
    for (NXOpen::Face* face : result)
    {
        if (selected.tellp() > 0)
        {
            selected << ",";
        }
        selected << face->Tag();
    }
    TiaoZenWriteLog(
        "INFO",
        "采用最外侧平面回退并包含板厚内侧面：selectedCount=" +
            std::to_string(result.size()) +
            "，thickness=" + Fixed(frame_.thickness) +
            "，skirtInset=" + Fixed(skirtInset) +
            "，selectionBand=" + Fixed(selectionBand) +
            "，skirtTopologyFaces=" +
            std::to_string(skirtFaceCount) +
            "，faceTags=[" + selected.str() + "]");
    return result;
}

bool PanelSizeDialog::PullBoundaryFaces(
    NXOpen::Body* body,
    const char* sideName,
    const NXOpen::Vector3d& outwardDirection,
    double boundaryCoordinate,
    double distance,
    std::vector<tag_t>& createdFeatures,
    std::string& error) const
{
    const std::string side =
        sideName != nullptr ? sideName : "<unknown>";
    TiaoZenWriteLog(
        "INFO",
        "准备调整" + side +
            "侧：distance=" + SignedFixed(distance) +
            "，direction=" + VectorText(outwardDirection) +
            "，boundaryCoordinate=" + Fixed(boundaryCoordinate));
    if (std::abs(distance) <= kTolerance)
    {
        TiaoZenWriteLog(
            "DEBUG", side + "侧距离为零，跳过。");
        return true;
    }

    const std::vector<NXOpen::Face*> faces =
        FindBoundaryFaces(body, outwardDirection, boundaryCoordinate);
    if (faces.empty())
    {
        error =
            "未找到板件边界侧面。请选择能代表板件整体长宽的主平面。";
        TiaoZenWriteLog(
            "ERROR", side + "侧未找到可移动边界面。");
        return false;
    }
    std::ostringstream faceTags;
    for (NXOpen::Face* face : faces)
    {
        if (faceTags.tellp() > 0)
        {
            faceTags << ",";
        }
        faceTags << face->Tag();
    }
    TiaoZenWriteLog(
        "INFO",
        side + "侧选面完成：count=" +
            std::to_string(faces.size()) +
            "，faceTags=[" + faceTags.str() + "]");

    NXOpen::Part* workPart = session_->Parts()->Work();
    NXOpen::Features::PullFaceBuilder* builder = nullptr;
    try
    {
        NXOpen::Direction* direction =
            workPart->Directions()->CreateDirection(
                frame_.origin, outwardDirection,
                NXOpen::SmartObject::UpdateOptionWithinModeling);
        builder = workPart->Features()->CreatePullFaceBuilder(nullptr);
        builder->Motion()->SetOption(
            NXOpen::GeometricUtilities::ModlMotion::OptionsDistance);
        builder->Motion()->SetDistanceVector(direction);
        builder->Motion()->DistanceValue()->SetFormula(
            Number(distance).c_str());

        NXOpen::SelectionIntentRuleOptions* options =
            workPart->ScRuleFactory()->CreateRuleOptions();
        options->SetSelectedFromInactive(false);
        NXOpen::FaceDumbRule* rule =
            workPart->ScRuleFactory()->CreateRuleFaceDumb(faces, options);
        delete options;
        builder->FaceToPull()->ReplaceRules(
            std::vector<NXOpen::SelectionIntentRule*>{rule}, false);

        NXOpen::NXObject* committed = builder->Commit();
        NXOpen::Features::Feature* feature =
            dynamic_cast<NXOpen::Features::Feature*>(committed);
        if (feature == nullptr)
        {
            for (NXOpen::NXObject* object : builder->GetCommittedObjects())
            {
                feature = dynamic_cast<NXOpen::Features::Feature*>(object);
                if (feature != nullptr)
                {
                    break;
                }
            }
        }
        builder->Destroy();
        builder = nullptr;
        if (feature == nullptr)
        {
            error = "NX 未返回有效的拉动面特征。";
            return false;
        }
        createdFeatures.push_back(feature->Tag());
        TiaoZenWriteLog(
            "INFO",
            side + "侧 PullFace 提交成功：featureTag=" +
                std::to_string(feature->Tag()) +
                "，distance=" + SignedFixed(distance));
        return true;
    }
    catch (const NXOpen::NXException& ex)
    {
        if (builder != nullptr)
        {
            builder->Destroy();
        }
        error = ex.Message() != nullptr ? ex.Message()
                                       : "拉动板件边界面失败。";
        TiaoZenWriteLog(
            "ERROR",
            side + "侧 PullFace 失败：" + error);
        return false;
    }
}

bool PanelSizeDialog::ApplyOffsets(
    const SideOffsets& offsets,
    std::vector<tag_t>& createdFeatures,
    std::string& error) const
{
    TiaoZenWriteLog(
        "INFO",
        "ApplyOffsets：bodyTag=" + std::to_string(frame_.bodyTag) +
            "，left=" + SignedFixed(offsets.left) +
            "，right=" + SignedFixed(offsets.right) +
            "，bottom=" + SignedFixed(offsets.bottom) +
            "，top=" + SignedFixed(offsets.top));
    if (!IsAlive(frame_.bodyTag))
    {
        error = "板件实体已经失效，请重新选择。";
        return false;
    }
    NXOpen::Body* body = dynamic_cast<NXOpen::Body*>(
        NXOpen::NXObjectManager::Get(frame_.bodyTag));
    if (body == nullptr)
    {
        error = "无法获取板件实体。";
        return false;
    }

    if (!PullBoundaryFaces(
            body, "左", Scale(frame_.lengthDirection, -1.0),
            -frame_.minLength, offsets.left,
            createdFeatures, error))
    {
        return false;
    }
    if (!PullBoundaryFaces(
            body, "右", frame_.lengthDirection,
            frame_.maxLength, offsets.right,
            createdFeatures, error))
    {
        return false;
    }
    if (!PullBoundaryFaces(
            body, "下", Scale(frame_.widthDirection, -1.0),
            -frame_.minWidth, offsets.bottom,
            createdFeatures, error))
    {
        return false;
    }
    if (!PullBoundaryFaces(
            body, "上", frame_.widthDirection,
            frame_.maxWidth, offsets.top,
            createdFeatures, error))
    {
        return false;
    }
    return true;
}

bool PanelSizeDialog::CreatePreview(std::string& error)
{
    TiaoZenWriteLog("DEBUG", "CreatePreview 开始。");
    if (!frame_.valid)
    {
        return true;
    }
    const SideOffsets offsets = CalculateOffsets(error);
    if (!error.empty())
    {
        return false;
    }
    if (offsets.IsZero(kTolerance))
    {
        return true;
    }

    previewMark_ = session_->SetUndoMark(
        NXOpen::Session::MarkVisibilityInvisible,
        "板件调尺预览");
    previewFeatures_.clear();
    TiaoZenWriteLog(
        "DEBUG",
        "已创建不可见预览撤销标记，markId=" +
            std::to_string(static_cast<int>(previewMark_)));
    if (!ApplyOffsets(offsets, previewFeatures_, error))
    {
        UndoPreview();
        return false;
    }
    NXOpen::Body* adjustedBody =
        IsAlive(frame_.bodyTag)
            ? dynamic_cast<NXOpen::Body*>(
                  NXOpen::NXObjectManager::Get(frame_.bodyTag))
            : nullptr;
    if (!ValidateSkirtDimensions(adjustedBody, error))
    {
        TiaoZenWriteLog(
            "ERROR", "裙边尺寸保护触发：" + error);
        UndoPreview();
        return false;
    }
    hasPreview_ = true;
    TiaoZenWriteLog(
        "INFO",
        "实时预览创建成功：featureCount=" +
            std::to_string(previewFeatures_.size()));
    return true;
}

bool PanelSizeDialog::UndoPreview()
{
    if (!hasPreview_ &&
        previewMark_ == static_cast<NXOpen::Session::UndoMarkId>(0) &&
        previewFeatures_.empty())
    {
        return true;
    }
    const std::vector<tag_t> featureTags = previewFeatures_;
    bool markUndoSucceeded = false;
    try
    {
        TiaoZenWriteLog(
            "DEBUG",
            "撤销预览：hasPreview=" +
                std::string(hasPreview_ ? "true" : "false") +
                "，markId=" +
                std::to_string(static_cast<int>(previewMark_)) +
                "，featureCount=" +
                std::to_string(previewFeatures_.size()));
        if (previewMark_ !=
            static_cast<NXOpen::Session::UndoMarkId>(0))
        {
            const bool markExists = session_->DoesUndoMarkExist(
                previewMark_, "板件调尺预览");
            TiaoZenWriteLog(
                markExists ? "DEBUG" : "WARN",
                "预览撤销标记检查：markId=" +
                    std::to_string(static_cast<int>(previewMark_)) +
                    "，exists=" +
                    std::string(markExists ? "true" : "false"));
            if (markExists)
            {
                session_->UndoToMark(previewMark_, "板件调尺预览");
                if (session_->DoesUndoMarkExist(
                        previewMark_, "板件调尺预览"))
                {
                    session_->DeleteUndoMark(
                        previewMark_, "板件调尺预览");
                }
                markUndoSucceeded = true;
                TiaoZenWriteLog(
                    "INFO", "已通过 NX 撤销标记恢复预览。");
            }
        }
    }
    catch (const NXOpen::NXException& ex)
    {
        TiaoZenWriteLog(
            "ERROR",
            std::string("通过撤销标记恢复预览失败：code=") +
                std::to_string(ex.ErrorCode()) +
                "，message=" +
                (ex.Message() != nullptr ? ex.Message() : "<null>"));
    }
    catch (...)
    {
        TiaoZenWriteLog(
            "ERROR", "通过撤销标记恢复预览时发生未知异常。");
    }

    std::vector<NXOpen::TaggedObject*> survivors;
    std::set<tag_t> seen;
    for (auto item = featureTags.rbegin();
         item != featureTags.rend(); ++item)
    {
        if (*item == NULL_TAG ||
            !seen.insert(*item).second ||
            !IsAlive(*item))
        {
            continue;
        }
        NXOpen::Features::Feature* feature =
            dynamic_cast<NXOpen::Features::Feature*>(
                NXOpen::NXObjectManager::Get(*item));
        if (feature != nullptr)
        {
            survivors.push_back(feature);
        }
    }

    if (!survivors.empty())
    {
        TiaoZenWriteLog(
            "WARN",
            "撤销后仍有 " + std::to_string(survivors.size()) +
                " 个预览特征存活，启动 UpdateManager 回退清理。");
        try
        {
            const NXOpen::Session::UndoMarkId cleanupMark =
                session_->SetUndoMark(
                    NXOpen::Session::MarkVisibilityInvisible,
                    "板件调尺预览残留清理");
            NXOpen::Update* update = session_->UpdateManager();
            update->ClearDeleteList();
            update->ClearErrorList();
            update->AddObjectsToDeleteList(survivors);
            const int updateErrors = update->DoUpdate(cleanupMark);
            if (session_->DoesUndoMarkExist(
                    cleanupMark, "板件调尺预览残留清理"))
            {
                session_->DeleteUndoMark(
                    cleanupMark, "板件调尺预览残留清理");
            }
            TiaoZenWriteLog(
                updateErrors == 0 ? "INFO" : "WARN",
                "UpdateManager 预览清理完成：queued=" +
                    std::to_string(survivors.size()) +
                    "，updateErrors=" +
                    std::to_string(updateErrors));
        }
        catch (const NXOpen::NXException& ex)
        {
            TiaoZenWriteLog(
                "ERROR",
                std::string("UpdateManager 清理预览失败：code=") +
                    std::to_string(ex.ErrorCode()) +
                    "，message=" +
                    (ex.Message() != nullptr ? ex.Message() : "<null>"));
        }
        catch (...)
        {
            TiaoZenWriteLog(
                "ERROR", "UpdateManager 清理预览时发生未知异常。");
        }
    }

    int remainingFeatureCount = 0;
    for (tag_t featureTag : featureTags)
    {
        if (IsAlive(featureTag))
        {
            ++remainingFeatureCount;
        }
    }
    previewMark_ = static_cast<NXOpen::Session::UndoMarkId>(0);
    hasPreview_ = false;
    previewFeatures_.clear();
    TiaoZenWriteLog(
        remainingFeatureCount == 0 ? "INFO" : "ERROR",
        "预览回滚结束：markUndoSucceeded=" +
            std::string(markUndoSucceeded ? "true" : "false") +
            "，remainingFeatureCount=" +
            std::to_string(remainingFeatureCount));
    return remainingFeatureCount == 0;
}

void PanelSizeDialog::CommitPreview()
{
    if (!hasPreview_ ||
        previewMark_ == static_cast<NXOpen::Session::UndoMarkId>(0))
    {
        TiaoZenWriteLog(
            "DEBUG", "CommitPreview：没有可提交的预览。");
        return;
    }
    if (session_->DoesUndoMarkExist(previewMark_, "板件调尺预览"))
    {
        session_->SetUndoMarkName(previewMark_, "板件调尺");
        session_->SetUndoMarkVisibility(
            previewMark_, "板件调尺",
            NXOpen::Session::MarkVisibilityVisible);
    }
    previewMark_ = static_cast<NXOpen::Session::UndoMarkId>(0);
    hasPreview_ = false;
    previewFeatures_.clear();
    TiaoZenWriteLog("INFO", "预览撤销标记已转为正式“板件调尺”特征。");
}

void PanelSizeDialog::ShowError(const std::string& message) const
{
    TiaoZenWriteLog("ERROR", "界面错误提示：" + message);
    ui_->NXMessageBox()->Show(
        "板件调尺",
        NXOpen::NXMessageBox::DialogTypeError,
        message.c_str());
}
