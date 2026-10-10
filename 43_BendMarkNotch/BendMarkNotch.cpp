#include "BendMarkNotch.hpp"
#include "BendMarkNotchCustomFeatureShared.hpp"
#include "../../common/ZhihuiDialogMemory.hpp"

#ifdef CreateDialog
#undef CreateDialog
#endif

#include <NXOpen/BlockStyler_PropertyList.hxx>
#include <NXOpen/BlockStyler_SelectObject.hxx>
#include <NXOpen/Body.hxx>
#include <NXOpen/BodyCollection.hxx>
#include <NXOpen/BodyDumbRule.hxx>
#include <NXOpen/BodyFeatureRule.hxx>
#include <NXOpen/Expression.hxx>
#include <NXOpen/NXMatrix.hxx>
#include <NXOpen/Arc.hxx>
#include <NXOpen/Curve.hxx>
#include <NXOpen/CurveCollection.hxx>
#include <NXOpen/CurveFeatureRule.hxx>
#include <NXOpen/Direction.hxx>
#include <NXOpen/DirectionCollection.hxx>
#include <NXOpen/DisplayableObject.hxx>
#include <NXOpen/Edge.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/FaceDumbRule.hxx>
#include <NXOpen/Features_BooleanBuilder.hxx>
#include <NXOpen/Features_BooleanFeature.hxx>
#include <NXOpen/Features_CustomAttribute.hxx>
#include <NXOpen/Features_CustomAttributeCollection.hxx>
#include <NXOpen/Features_ConstructionFeatureData.hxx>
#include <NXOpen/Features_CustomFeature.hxx>
#include <NXOpen/Features_CustomFeatureBuilder.hxx>
#include <NXOpen/Features_CustomFeatureClass.hxx>
#include <NXOpen/Features_CustomFeatureClassManager.hxx>
#include <NXOpen/Features_CustomFeatureData.hxx>
#include <NXOpen/Features_CustomFeatureDataCollection.hxx>
#include <NXOpen/Features_CustomStringAttribute.hxx>
#include <NXOpen/Features_CustomTagArrayAttribute.hxx>
#include <NXOpen/Features_ExtrudeBuilder.hxx>
#include <NXOpen/Features_Feature.hxx>
#include <NXOpen/Features_FeatureCollection.hxx>
#include <NXOpen/Features_FlatPattern.hxx>
#include <NXOpen/Features_SketchFeature.hxx>
#include <NXOpen/Features_SheetMetal_FlatPatternBuilder.hxx>
#include <NXOpen/Features_SheetMetal_RebendBuilder.hxx>
#include <NXOpen/Features_SheetMetal_SheetmetalManager.hxx>
#include <NXOpen/Features_SheetMetal_UnbendBuilder.hxx>
#include <NXOpen/GeometricUtilities_BooleanOperation.hxx>
#include <NXOpen/GeometricUtilities_Extend.hxx>
#include <NXOpen/GeometricUtilities_FeatureOffset.hxx>
#include <NXOpen/GeometricUtilities_FeatureOptions.hxx>
#include <NXOpen/GeometricUtilities_Limits.hxx>
#include <NXOpen/GeometricUtilities_SmartVolumeProfileBuilder.hxx>
#include <NXOpen/Line.hxx>
#include <NXOpen/ErrorInfo.hxx>
#include <NXOpen/ErrorList.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXMessageBox.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/Plane.hxx>
#include <NXOpen/PlaneCollection.hxx>
#include <NXOpen/Point.hxx>
#include <NXOpen/PointCollection.hxx>
#include <NXOpen/ScCollector.hxx>
#include <NXOpen/ScCollectorCollection.hxx>
#include <NXOpen/ScRuleFactory.hxx>
#include <NXOpen/SelectFace.hxx>
#include <NXOpen/Selection.hxx>
#include <NXOpen/SelectionIntentRule.hxx>
#include <NXOpen/Section.hxx>
#include <NXOpen/SectionCollection.hxx>
#include <NXOpen/SmartObject.hxx>
#include <NXOpen/Sketch.hxx>
#include <NXOpen/SketchCollection.hxx>
#include <NXOpen/SketchInPlaceBuilder.hxx>
#include <NXOpen/UI.hxx>
#include <NXOpen/NXMessageBox.hxx>
#include <NXOpen/Update.hxx>

#include <uf.h>
#include <uf_disp.h>
#include <uf_modl.h>
#include <uf_modl_utilities.h>
#include <uf_obj.h>
#include <uf_smd.h>
#include <uf_ui_types.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>

namespace
{
constexpr double kTolerance = 1.0e-6;
constexpr double kMarkingLineWidth = 0.001;
constexpr double kMarkingLineDepth = 0.001;
constexpr double kMarkingLineZeroEdgeOffset = 0.001;
constexpr double kPi = 3.14159265358979323846;

void AppendDebugLog(const std::string& message) noexcept
{
    try
    {
        SYSTEMTIME now = {};
        GetLocalTime(&now);
        char prefix[128] = {};
        sprintf_s(prefix, "[%04u-%02u-%02u %02u:%02u:%02u.%03u] [T%lu] "
                          "BendMarkNotch: ",
                  now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute,
                  now.wSecond, now.wMilliseconds, GetCurrentThreadId());
        const std::string line = std::string(prefix) + message + "\r\n";

        const wchar_t* directory = L"D:\\UG智辉钣金插件\\logs";
        const wchar_t* path =
            L"D:\\UG智辉钣金插件\\logs\\BendMarkNotch_debug.log";
        CreateDirectoryW(directory, nullptr);
        HANDLE file = CreateFileW(
            path, FILE_APPEND_DATA,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
            OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file != INVALID_HANDLE_VALUE)
        {
            DWORD written = 0;
            WriteFile(file, line.data(), static_cast<DWORD>(line.size()),
                      &written, nullptr);
            CloseHandle(file);
        }

        std::vector<char> syslog(line.begin(), line.end());
        syslog.push_back('\0');
        UF_print_syslog(syslog.data(), false);
    }
    catch (...) {}
}

std::string PointText(const NXOpen::Point3d& point)
{
    std::ostringstream text;
    text.precision(16);
    text << point.X << ',' << point.Y << ',' << point.Z;
    return text.str();
}

std::string VectorText(const NXOpen::Vector3d& vector)
{
    std::ostringstream text;
    text.precision(16);
    text << vector.X << ',' << vector.Y << ',' << vector.Z;
    return text.str();
}

class DisplaySuppressionGuard
{
public:
    DisplaySuppressionGuard()
    {
        active_ = UF_DISP_ask_display(&previousState_) == 0 &&
            previousState_ != UF_DISP_SUPPRESS_DISPLAY &&
            UF_DISP_set_display(UF_DISP_SUPPRESS_DISPLAY) == 0;
    }

    ~DisplaySuppressionGuard()
    {
        if (!active_) return;
        UF_DISP_set_display(previousState_);
        UF_DISP_regenerate_display();
    }

private:
    int previousState_ = UF_DISP_UNSUPPRESS_DISPLAY;
    bool active_ = false;
};

std::string ModuleDirectory()
{
    HMODULE module = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                              GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          reinterpret_cast<LPCWSTR>(&ModuleDirectory), &module))
    {
        return {};
    }
    wchar_t path[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(module, path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) return {};
    if (wchar_t* slash = wcsrchr(path, L'\\')) *(slash + 1) = L'\0';
    return zhihui_dialog_memory::Utf8FromWide(path);
}

bool IsAlive(tag_t tag)
{
    return tag != NULL_TAG && UF_OBJ_ask_status(tag) == UF_OBJ_ALIVE;
}

void LogUpdateUndoFeature()
{
    tag_t feature = NULL_TAG;
    int failure = 0;
    if (UF_MODL_ask_update_undo_feat(&feature, &failure) != 0) return;
    if (feature == NULL_TAG && failure == 0) return;
    char name[UF_OBJ_NAME_LEN + 1] = {};
    if (feature != NULL_TAG) UF_OBJ_ask_name(feature, name);
    AppendDebugLog("update undo feature=" + std::to_string(feature) +
        " code=" + std::to_string(failure) +
        " name=" + (name[0] == '\0' ? "(none)" : name));
}

void LogUpdateErrors()
{
    NXOpen::ErrorList* errors =
        NXOpen::Session::GetSession()->UpdateManager()->ErrorList();
    if (errors == nullptr || errors->Length() <= 0) return;
    AppendDebugLog("update error count=" + std::to_string(errors->Length()));
    for (int index = 0; index < errors->Length(); ++index)
    {
        NXOpen::ErrorInfo* info = errors->GetErrorInfo(index);
        if (info == nullptr) continue;
        NXOpen::NXObject* object = info->ErrorObject();
        AppendDebugLog("update error code=" + std::to_string(info->ErrorCode()) +
            " object=" + std::to_string(object == nullptr ? 0 : object->Tag()));
    }
}

int ChangeFeatureSuppression(const std::vector<tag_t>& tags, bool suppress)
{
    std::set<tag_t> unique;
    for (tag_t tag : tags)
        if (IsAlive(tag)) unique.insert(tag);
    if (unique.empty()) return 0;
    uf_list_p_t list = nullptr;
    if (UF_MODL_create_list(&list) != 0 || list == nullptr) return -1;
    for (tag_t tag : unique)
        UF_MODL_put_list_item(list, tag);
    const int error = suppress ? UF_MODL_suppress_feature(list)
                               : UF_MODL_unsuppress_feature(list);
    UF_MODL_delete_list(&list);
    return error;
}

double Dot(const NXOpen::Vector3d& a, const NXOpen::Vector3d& b)
{
    return a.X * b.X + a.Y * b.Y + a.Z * b.Z;
}

double Length(const NXOpen::Vector3d& value)
{
    return std::sqrt(Dot(value, value));
}

NXOpen::Vector3d Scale(const NXOpen::Vector3d& value, double factor)
{
    return {value.X * factor, value.Y * factor, value.Z * factor};
}

NXOpen::Vector3d Normalize(const NXOpen::Vector3d& value)
{
    const double length = Length(value);
    if (length <= kTolerance) throw std::runtime_error("无法确定几何方向。");
    return Scale(value, 1.0 / length);
}

NXOpen::Vector3d Subtract(const NXOpen::Point3d& a,
                          const NXOpen::Point3d& b)
{
    return {a.X - b.X, a.Y - b.Y, a.Z - b.Z};
}

NXOpen::Point3d Add(const NXOpen::Point3d& point,
                    const NXOpen::Vector3d& vector)
{
    return {point.X + vector.X, point.Y + vector.Y, point.Z + vector.Z};
}

NXOpen::Vector3d Cross(const NXOpen::Vector3d& a,
                       const NXOpen::Vector3d& b)
{
    return {a.Y * b.Z - a.Z * b.Y,
            a.Z * b.X - a.X * b.Z,
            a.X * b.Y - a.Y * b.X};
}

double Distance(const NXOpen::Point3d& a, const NXOpen::Point3d& b)
{
    return Length(Subtract(a, b));
}

bool EdgeEnds(NXOpen::Edge* edge, NXOpen::Point3d& first,
              NXOpen::Point3d& second)
{
    double a[3] = {}, b[3] = {};
    int count = 0;
    if (edge == nullptr || !IsAlive(edge->Tag()) ||
        UF_MODL_ask_edge_verts(edge->Tag(), a, b, &count) != 0 || count < 2)
    {
        return false;
    }
    first = {a[0], a[1], a[2]};
    second = {b[0], b[1], b[2]};
    return Distance(first, second) > kTolerance;
}

void DeleteObject(tag_t tag)
{
    if (IsAlive(tag)) UF_OBJ_delete_object(tag);
}

std::vector<char> NumberBuffer(double value)
{
    std::ostringstream stream;
    stream.precision(15);
    stream << value;
    const std::string text = stream.str();
    std::vector<char> buffer(text.begin(), text.end());
    buffer.push_back('\0');
    return buffer;
}

BendMarkNotchDialog* g_deferredDialog = nullptr;
int g_deferredTries = 0;
bool g_keepLoaded = false;

void CALLBACK DeferredRebuildTimer(HWND, UINT, UINT_PTR id, DWORD)
{
    KillTimer(nullptr, id);
    BendMarkNotchDialog* dialog = g_deferredDialog;
    if (dialog == nullptr) return;
    if (UF_initialize() != 0)
    {
        g_deferredDialog = nullptr;
        g_keepLoaded = false;
        delete dialog;
        return;
    }
    bool finished = true;
    try
    {
        NXOpen::Features::CustomFeature* still = nullptr;
        try
        {
            still = NXOpen::Session::GetSession()->CustomFeatureClassManager()
                        ->GetEditedCustomFeature();
        }
        catch (const NXOpen::NXException&) {}
        if (still != nullptr && g_deferredTries < 40 &&
            SetTimer(nullptr, 0, 250, DeferredRebuildTimer) != 0)
        {
            ++g_deferredTries;
            finished = false;
            AppendDebugLog("rebuild wait edited=" + std::to_string(still->Tag()) +
                " try=" + std::to_string(g_deferredTries));
        }
        else
        {
            g_deferredDialog = nullptr;
            g_deferredTries = 0;
            g_keepLoaded = false;
            dialog->RunDeferredRebuild();
        }
    }
    catch (...)
    {
        g_deferredDialog = nullptr;
        g_keepLoaded = false;
        AppendDebugLog("rebuild timer failed");
    }
    if (finished) delete dialog;
    UF_terminate();
}

NXOpen::Point3d PlanarFacePoint(NXOpen::Face* face)
{
    int type = 0, normalDirection = 0;
    double point[3] = {}, direction[3] = {}, box[6] = {};
    double radius = 0.0, radiusData = 0.0;
    if (face == nullptr || !IsAlive(face->Tag()) ||
        UF_MODL_ask_face_data(face->Tag(), &type, point, direction, box,
            &radius, &radiusData, &normalDirection) != 0 ||
        type != UF_MODL_PLANAR_FACE)
        throw std::runtime_error("无法读取伸直后折弯面的平面位置。");
    return {point[0], point[1], point[2]};
}
}

BendMarkNotchDialog::BendMarkNotchDialog()
    : ui_(NXOpen::UI::GetUI()), session_(NXOpen::Session::GetSession()),
      dialog_(ui_->CreateDialog(DialogPath().c_str())), autoSelect_(nullptr),
      bodySelect_(nullptr), distinguishBends_(nullptr), directionHelp_(nullptr),
      largeBendRatio_(nullptr), divideLargeBend_(nullptr),
      largeBendSegments_(nullptr), largeBendHelp_(nullptr),
      status_(nullptr), initialized_(false), shown_(false), updating_(false),
      editedFeatureTag_(NULL_TAG)
{
    try
    {
        NXOpen::Features::CustomFeature* edited =
            session_->CustomFeatureClassManager()->GetEditedCustomFeature();
        if (edited != nullptr && IsBendMarkNotchFeature(edited))
            editedFeatureTag_ = edited->Tag();
    }
    catch (const NXOpen::NXException& ex)
    {
        AppendDebugLog("query edited custom feature failed code=" +
            std::to_string(ex.ErrorCode()));
    }
    AppendDebugLog("dialog open edited=" + std::to_string(editedFeatureTag_));
    dialog_->AddInitializeHandler(
        NXOpen::make_callback(this, &BendMarkNotchDialog::initialize_cb));
    dialog_->AddDialogShownHandler(
        NXOpen::make_callback(this, &BendMarkNotchDialog::dialogShown_cb));
    dialog_->AddUpdateHandler(
        NXOpen::make_callback(this, &BendMarkNotchDialog::update_cb));
    dialog_->AddFilterHandler(
        NXOpen::make_callback(this, &BendMarkNotchDialog::filter_cb));
    dialog_->AddApplyHandler(
        NXOpen::make_callback(this, &BendMarkNotchDialog::apply_cb));
    dialog_->AddOkHandler(
        NXOpen::make_callback(this, &BendMarkNotchDialog::ok_cb));
    dialog_->AddCancelHandler(
        NXOpen::make_callback(this, &BendMarkNotchDialog::cancel_cb));
}

BendMarkNotchDialog::~BendMarkNotchDialog()
{
    pendingInternalFeatureTags_.clear();
    pendingInternalFeatureTags_.shrink_to_fit();
    initialized_ = false;
    shown_ = false;
    updating_ = false;

    delete dialog_;
    dialog_ = nullptr;
    autoSelect_ = nullptr;
    bodySelect_ = nullptr;
    distinguishBends_ = nullptr;
    directionHelp_ = nullptr;
    largeBendRatio_ = nullptr;
    divideLargeBend_ = nullptr;
    largeBendSegments_ = nullptr;
    largeBendHelp_ = nullptr;
    commonNotch_ = {};
    upNotch_ = {};
    downNotch_ = {};
    status_ = nullptr;
    ui_ = nullptr;
    session_ = nullptr;
}

bool BendMarkNotchShouldStayLoaded()
{
    return g_keepLoaded;
}

bool BendMarkNotchDialog::DeferredRebuildPending() const
{
    return g_deferredDialog == this;
}

bool BendMarkNotchDialog::ScheduleDeferredRebuild()
{
    if (g_deferredDialog != nullptr) return false;
    g_deferredDialog = this;
    g_deferredTries = 0;
    g_keepLoaded = true;
    if (SetTimer(nullptr, 0, 400, DeferredRebuildTimer) == 0)
    {
        g_deferredDialog = nullptr;
        g_keepLoaded = false;
        return false;
    }
    AppendDebugLog("rebuild scheduled feature=" +
        std::to_string(editedFeatureTag_));
    return true;
}

void BendMarkNotchDialog::RunDeferredRebuild()
{
    // The Block Styler dialog is already closed. Leave its blocks alone.
    status_ = nullptr;
    NXOpen::Features::CustomFeature* still = nullptr;
    try
    {
        still = session_->CustomFeatureClassManager()->GetEditedCustomFeature();
    }
    catch (const NXOpen::NXException&) {}
    if (still != nullptr)
    {
        AppendDebugLog("rebuild abandoned edited=" + std::to_string(still->Tag()));
        ShowError("NX 仍处于编辑状态，折弯标记缺口未更新。");
        return;
    }
    try
    {
        RebuildEditedFeature();
    }
    catch (const NXOpen::NXException& ex)
    {
        AppendDebugLog("rebuild failed code=" + std::to_string(ex.ErrorCode()) +
            " message=" + ex.Message());
        ShowError(ex.Message());
    }
    catch (const std::exception& ex)
    {
        AppendDebugLog(std::string("rebuild failed: ") + ex.what());
        ShowError(ex.what());
    }
}

NXOpen::BlockStyler::BlockDialog::DialogResponse
BendMarkNotchDialog::Launch()
{
    const NXOpen::BlockStyler::BlockDialog::DialogResponse response =
        dialog_->LaunchInDialogMode(
            editedFeatureTag_ != NULL_TAG
                ? NXOpen::BlockStyler::BlockDialog::DialogModeEdit
                : NXOpen::BlockStyler::BlockDialog::DialogModeCreate);
    NXOpen::Features::CustomFeature* stillEdited = nullptr;
    try
    {
        stillEdited = session_->CustomFeatureClassManager()->GetEditedCustomFeature();
    }
    catch (const NXOpen::NXException&) {}
    AppendDebugLog("dialog closed response=" + std::to_string(response) +
        " rebuild=" + std::to_string(editRebuildPending_ ? 1 : 0) +
        " stillEdited=" + std::to_string(stillEdited == nullptr ? 0 : stillEdited->Tag()));
    if (!editRebuildPending_) return response;
    editRebuildPending_ = false;
    if (stillEdited != nullptr)
    {
        // NX keeps the edit session until this command returns, and deleting
        // the node before then crashes. Rebuild on a timer after it ends.
        if (!ScheduleDeferredRebuild())
            ShowError("折弯标记缺口未能在编辑结束后重新创建。");
        return response;
    }
    try
    {
        RebuildEditedFeature();
    }
    catch (const NXOpen::NXException& ex)
    {
        AppendDebugLog("rebuild failed code=" + std::to_string(ex.ErrorCode()) +
            " message=" + ex.Message());
        ShowError(ex.Message());
    }
    catch (const std::exception& ex)
    {
        AppendDebugLog(std::string("rebuild failed: ") + ex.what());
        ShowError(ex.what());
    }
    return response;
}
std::string BendMarkNotchDialog::DialogPath() const
{
    return ModuleDirectory() + "BendMarkNotch.dlx";
}

void BendMarkNotchDialog::initialize_cb()
{
    try
    {
        autoSelect_ = dialog_->TopBlock()->FindBlock("auto_select");
        bodySelect_ = dialog_->TopBlock()->FindBlock("body_select");
        distinguishBends_ = dialog_->TopBlock()->FindBlock("distinguish_bends");
        directionHelp_ = dialog_->TopBlock()->FindBlock("direction_help");
        largeBendRatio_ = dialog_->TopBlock()->FindBlock("large_bend_ratio");
        divideLargeBend_ = dialog_->TopBlock()->FindBlock("divide_large_bend");
        largeBendSegments_ = dialog_->TopBlock()->FindBlock("large_bend_segments");
        largeBendHelp_ = dialog_->TopBlock()->FindBlock("large_bend_help");
        commonNotch_ = FindNotchControls("");
        // Preserve existing block IDs so saved per-direction parameters survive
        // the user-facing rename from inner/outer to up/down.
        upNotch_ = FindNotchControls("inner_");
        downNotch_ = FindNotchControls("outer_");
        status_ = dialog_->TopBlock()->FindBlock("result_status");
        if (autoSelect_ == nullptr || bodySelect_ == nullptr ||
            distinguishBends_ == nullptr || directionHelp_ == nullptr ||
            largeBendRatio_ == nullptr || divideLargeBend_ == nullptr ||
            largeBendSegments_ == nullptr || largeBendHelp_ == nullptr ||
            status_ == nullptr)
        {
            throw std::runtime_error("BendMarkNotch.dlx 缺少必要控件。");
        }

        auto* selection = dynamic_cast<NXOpen::BlockStyler::SelectObject*>(
            bodySelect_);
        if (selection == nullptr) throw std::runtime_error("选择体控件类型错误。");
        std::vector<NXOpen::Selection::MaskTriple> masks;
        masks.emplace_back(UF_solid_type, UF_solid_body_subtype, 0);
        selection->SetSelectionFilter(
            NXOpen::Selection::SelectionActionClearAndEnableSpecific, masks);
        selection->SetAutomaticProgression(false);
        selection->SetSelectModeAsString("Multiple");
        initialized_ = true;
    }
    catch (const NXOpen::NXException& ex)
    {
        ShowError(ex.Message());
    }
    catch (const std::exception& ex)
    {
        ShowError(ex.what());
    }
}

void BendMarkNotchDialog::dialogShown_cb()
{
    try
    {
        if (!initialized_) return;
        shown_ = true;
        if (editedFeatureTag_ != NULL_TAG && IsAlive(editedFeatureTag_))
        {
            const char* stored = nullptr;
            try
            {
                auto* edited = dynamic_cast<NXOpen::Features::CustomFeature*>(
                    NXOpen::NXObjectManager::Get(editedFeatureTag_));
                NXOpen::Features::CustomFeatureData* data =
                    edited == nullptr ? nullptr : edited->FeatureData();
                NXOpen::Features::CustomStringAttribute* attribute =
                    data == nullptr ? nullptr :
                    data->CustomStringAttributeByName(
                        zhihui_bend_mark_notch::kAttrSettings);
                if (attribute != nullptr)
                    stored = attribute->Value().GetUTF8Text();
            }
            catch (const NXOpen::NXException& ex)
            {
                AppendDebugLog("dialog settings read failed code=" +
                    std::to_string(ex.ErrorCode()));
                stored = nullptr;
            }
            DialogSettings settings;
            if (stored != nullptr && DecodeDialogSettings(stored, settings))
            {
                ApplyDialogSettings(settings);
                AppendDebugLog("dialog restored settings feature=" +
                    std::to_string(editedFeatureTag_));
            }
            else
            {
                AppendDebugLog("dialog has no stored settings feature=" +
                    std::to_string(editedFeatureTag_));
            }
        }
        UpdateControlState();
        if (!ToggleValue(autoSelect_) && bodySelect_ != nullptr)
            bodySelect_->Focus();
    }
    catch (const NXOpen::NXException& ex) { ShowError(ex.Message()); }
    catch (const std::exception& ex) { ShowError(ex.what()); }
}

int BendMarkNotchDialog::update_cb(NXOpen::BlockStyler::UIBlock* block)
{
    if (!initialized_ || !shown_ || updating_) return 0;
    try
    {
        if (block == autoSelect_ || block == distinguishBends_ ||
            block == divideLargeBend_ ||
            block == commonNotch_.method || block == upNotch_.method ||
            block == downNotch_.method ||
            block == commonNotch_.type || block == upNotch_.type ||
            block == downNotch_.type)
            UpdateControlState();
        if (block == autoSelect_ && !ToggleValue(autoSelect_) &&
            bodySelect_ != nullptr)
        {
            updating_ = true;
            try
            {
                bodySelect_->Focus();
            }
            catch (...)
            {
                updating_ = false;
                throw;
            }
            updating_ = false;
        }
    }
    catch (const NXOpen::NXException& ex) { ShowError(ex.Message()); return ex.ErrorCode(); }
    catch (const std::exception& ex) { ShowError(ex.what()); return 1; }
    return 0;
}

int BendMarkNotchDialog::filter_cb(NXOpen::BlockStyler::UIBlock* block,
                                    NXOpen::TaggedObject* selectedObject)
{
    try
    {
        if (block != bodySelect_) return UF_UI_SEL_ACCEPT;
        NXOpen::Body* body = dynamic_cast<NXOpen::Body*>(selectedObject);
        return body != nullptr && IsSheetMetalBody(body)
            ? UF_UI_SEL_ACCEPT : UF_UI_SEL_REJECT;
    }
    catch (const NXOpen::NXException& ex) { ShowError(ex.Message()); }
    catch (const std::exception& ex) { ShowError(ex.what()); }
    return UF_UI_SEL_REJECT;
}

int BendMarkNotchDialog::apply_cb()
{
    try
    {
        if (!initialized_ || !shown_ || updating_) return 1;
        return Execute();
    }
    catch (const NXOpen::NXException& ex) { ShowError(ex.Message()); return 1; }
    catch (const std::exception& ex) { ShowError(ex.what()); return 1; }
}

int BendMarkNotchDialog::ok_cb()
{
    return apply_cb();
}

int BendMarkNotchDialog::cancel_cb()
{
    return 0;
}

void BendMarkNotchDialog::UpdateControlState()
{
    if (!initialized_ || !shown_ || updating_) return;
    updating_ = true;
    try
    {
        const bool automatic = ToggleValue(autoSelect_);
        const bool separate = ToggleValue(distinguishBends_);
        auto setShow = [](NXOpen::BlockStyler::UIBlock* block, bool show)
        {
            std::unique_ptr<NXOpen::BlockStyler::PropertyList> properties(
                block->GetProperties());
            properties->SetLogical("Show", show);
            properties->SetLogical("Enable", show);
        };
        const auto showSettings = [&](const NotchControls& controls, bool show)
        {
            const bool line = MarkingMethod(controls) == 1;
            const int type = NotchType(controls);
            setShow(controls.method, show);
            setShow(controls.lineLength, show && line);
            setShow(controls.lineOffset, show && line);
            setShow(controls.type, show && !line);
            setShow(controls.diameter, show && !line && type == 0);
            setShow(controls.angle, show && !line && type == 1);
            setShow(controls.depth, show && !line && type == 1);
            setShow(controls.rectangleWidth, show && !line && type == 2);
            setShow(controls.rectangleDepth, show && !line && type == 2);
        };
        setShow(bodySelect_, !automatic);
        setShow(directionHelp_, separate);
        setShow(largeBendSegments_, ToggleValue(divideLargeBend_));
        setShow(largeBendHelp_, ToggleValue(divideLargeBend_));
        showSettings(commonNotch_, !separate);
        showSettings(upNotch_, separate);
        showSettings(downNotch_, separate);
        SetStatus(automatic ? "将批量处理当前工作部件中的全部钣金体。"
                            : "请选择一个或多个钣金体。");
    }
    catch (...)
    {
        updating_ = false;
        throw;
    }
    updating_ = false;
}

bool BendMarkNotchDialog::ToggleValue(
    NXOpen::BlockStyler::UIBlock* block) const
{
    std::unique_ptr<NXOpen::BlockStyler::PropertyList> properties(block->GetProperties());
    const bool value = properties->GetLogical("Value");
    return value;
}

BendMarkNotchDialog::NotchControls BendMarkNotchDialog::FindNotchControls(
    const std::string& prefix) const
{
    NotchControls controls;
    controls.method = dialog_->TopBlock()->FindBlock((prefix + "marking_method").c_str());
    controls.lineLength = dialog_->TopBlock()->FindBlock((prefix + "line_length").c_str());
    controls.lineOffset = dialog_->TopBlock()->FindBlock((prefix + "line_offset").c_str());
    controls.type = dialog_->TopBlock()->FindBlock((prefix + "notch_type").c_str());
    controls.diameter = dialog_->TopBlock()->FindBlock((prefix + "diameter").c_str());
    controls.angle = dialog_->TopBlock()->FindBlock((prefix + "angle").c_str());
    controls.depth = dialog_->TopBlock()->FindBlock((prefix + "depth").c_str());
    controls.rectangleWidth = dialog_->TopBlock()->FindBlock(
        (prefix + "rectangle_width").c_str());
    controls.rectangleDepth = dialog_->TopBlock()->FindBlock(
        (prefix + "rectangle_depth").c_str());
    if (!controls.method || !controls.lineLength || !controls.lineOffset ||
        !controls.type || !controls.diameter || !controls.angle ||
        !controls.depth || !controls.rectangleWidth || !controls.rectangleDepth)
        throw std::runtime_error("BendMarkNotch.dlx 缺少缺口参数控件。");
    return controls;
}

int BendMarkNotchDialog::MarkingMethod(const NotchControls& controls) const
{
    std::unique_ptr<NXOpen::BlockStyler::PropertyList> properties(
        controls.method->GetProperties());
    const int value = properties->GetEnum("Value");
    if (value != 0 && value != 1)
        throw std::runtime_error("折弯标记方式无效。");
    return value;
}

int BendMarkNotchDialog::NotchType(const NotchControls& controls) const
{
    std::unique_ptr<NXOpen::BlockStyler::PropertyList> properties(
        controls.type->GetProperties());
    const int value = properties->GetEnum("Value");
    return value;
}

BendMarkNotchDialog::NotchSettings BendMarkNotchDialog::ReadNotchSettings(
    const NotchControls& controls) const
{
    NotchSettings settings;
    settings.lineMark = MarkingMethod(controls) == 1;
    if (settings.lineMark)
    {
        settings.type = 2;
        settings.width = kMarkingLineWidth;
        settings.depth = DoubleValue(controls.lineLength);
        settings.edgeOffset = DoubleValue(controls.lineOffset);
        // Keep zero available in the dialog, but leave a small material bridge
        // at the contour so the shallow groove does not open the bend end.
        if (settings.edgeOffset == 0.0)
            settings.edgeOffset = kMarkingLineZeroEdgeOffset;
        return settings;
    }
    settings.type = NotchType(controls);
    if (settings.type == 0)
        settings.diameter = DoubleValue(controls.diameter);
    else if (settings.type == 1)
    {
        settings.angle = DoubleValue(controls.angle);
        settings.depth = DoubleValue(controls.depth);
    }
    else if (settings.type == 2)
    {
        settings.width = DoubleValue(controls.rectangleWidth);
        settings.depth = DoubleValue(controls.rectangleDepth);
    }
    return settings;
}

BendMarkNotchDialog::DialogSettings BendMarkNotchDialog::ReadDialogSettings() const
{
    DialogSettings settings;
    settings.automatic = ToggleValue(autoSelect_);
    settings.separate = ToggleValue(distinguishBends_);
    settings.large = ReadLargeBendSettings();
    settings.common = ReadNotchSettings(commonNotch_);
    settings.up = ReadNotchSettings(upNotch_);
    settings.down = ReadNotchSettings(downNotch_);
    return settings;
}

std::string BendMarkNotchDialog::EncodeDialogSettings(
    const DialogSettings& settings) const
{
    std::ostringstream text;
    text.precision(16);
    const auto writeNotch = [&](const NotchSettings& notch)
    {
        text << (notch.lineMark ? 1 : 0) << ' ' << notch.edgeOffset << ' '
             << notch.type << ' ' << notch.diameter << ' ' << notch.angle << ' '
             << notch.depth << ' ' << notch.width << ' ';
    };
    text << "v1 " << (settings.automatic ? 1 : 0) << ' '
         << (settings.separate ? 1 : 0) << ' '
         << settings.large.radiusThicknessRatio << ' '
         << settings.large.segments << ' ';
    writeNotch(settings.common);
    writeNotch(settings.up);
    writeNotch(settings.down);
    return text.str();
}

bool BendMarkNotchDialog::DecodeDialogSettings(
    const std::string& text, DialogSettings& settings) const
{
    std::istringstream input(text);
    std::string version;
    int automatic = 0;
    int separate = 0;
    const auto readNotch = [&](NotchSettings& notch)
    {
        int lineMark = 0;
        input >> lineMark >> notch.edgeOffset >> notch.type >> notch.diameter >>
            notch.angle >> notch.depth >> notch.width;
        notch.lineMark = lineMark != 0;
    };
    input >> version >> automatic >> separate >>
        settings.large.radiusThicknessRatio >> settings.large.segments;
    readNotch(settings.common);
    readNotch(settings.up);
    readNotch(settings.down);
    if (version != "v1" || input.fail()) return false;
    settings.automatic = automatic != 0;
    settings.separate = separate != 0;
    return true;
}

void BendMarkNotchDialog::ApplyDialogSettings(const DialogSettings& settings) const
{
    const auto setLogical = [](NXOpen::BlockStyler::UIBlock* block, bool value)
    {
        std::unique_ptr<NXOpen::BlockStyler::PropertyList> properties(
            block->GetProperties());
        properties->SetLogical("Value", value);
    };
    const auto setDouble = [](NXOpen::BlockStyler::UIBlock* block, double value)
    {
        std::unique_ptr<NXOpen::BlockStyler::PropertyList> properties(
            block->GetProperties());
        properties->SetDouble("Value", value);
    };
    const auto setEnum = [](NXOpen::BlockStyler::UIBlock* block, int value)
    {
        std::unique_ptr<NXOpen::BlockStyler::PropertyList> properties(
            block->GetProperties());
        properties->SetEnum("Value", value);
    };
    const auto setInteger = [](NXOpen::BlockStyler::UIBlock* block, int value)
    {
        std::unique_ptr<NXOpen::BlockStyler::PropertyList> properties(
            block->GetProperties());
        properties->SetInteger("Value", value);
    };
    const auto applyNotch = [&](const NotchControls& controls,
                                 const NotchSettings& notch)
    {
        setEnum(controls.method, notch.lineMark ? 1 : 0);
        setEnum(controls.type, notch.type);
        setDouble(controls.diameter, notch.diameter);
        setDouble(controls.angle, notch.angle);
        if (notch.lineMark)
        {
            setDouble(controls.lineLength, notch.depth);
            setDouble(controls.lineOffset, notch.edgeOffset);
        }
        else
        {
            setDouble(controls.depth, notch.depth);
            setDouble(controls.rectangleWidth, notch.width);
            setDouble(controls.rectangleDepth, notch.depth);
        }
    };
    setLogical(autoSelect_, settings.automatic);
    setLogical(distinguishBends_, settings.separate);
    setDouble(largeBendRatio_, settings.large.radiusThicknessRatio);
    setLogical(divideLargeBend_, settings.large.segments > 1);
    setInteger(largeBendSegments_,
        settings.large.segments > 1 ? settings.large.segments : 2);
    applyNotch(commonNotch_, settings.common);
    applyNotch(upNotch_, settings.up);
    applyNotch(downNotch_, settings.down);
}

void BendMarkNotchDialog::ValidateNotchSettings(
    const NotchSettings& settings, const std::string& label) const
{
    if (settings.lineMark)
    {
        if (!std::isfinite(settings.depth) || settings.depth <= kTolerance)
            throw std::runtime_error(label + "标记线长度必须大于 0。");
        if (!std::isfinite(settings.edgeOffset) || settings.edgeOffset < 0.0)
            throw std::runtime_error(label + "标记线离轮廓边距离不能小于 0。");
        return;
    }
    if (settings.type != 0 && settings.type != 1 && settings.type != 2)
        throw std::runtime_error(label + "缺口类型无效。");
    if (settings.type == 0 && (!std::isfinite(settings.diameter) ||
                              settings.diameter <= kTolerance))
        throw std::runtime_error(label + "半圆缺口直径必须大于 0。");
    if (settings.type == 1 && (!std::isfinite(settings.angle) ||
        !std::isfinite(settings.depth) || settings.angle <= 0.0 ||
        settings.angle >= 179.0 || settings.depth <= kTolerance))
        throw std::runtime_error(label + "锐角缺口角度须在 0～179°之间，深度必须大于 0。");
    if (settings.type == 2 && (!std::isfinite(settings.width) ||
        !std::isfinite(settings.depth) || settings.width <= kTolerance ||
        settings.depth <= kTolerance))
        throw std::runtime_error(label + "矩形缺口宽度和深度必须大于 0。");
}

BendMarkNotchDialog::LargeBendSettings
BendMarkNotchDialog::ReadLargeBendSettings() const
{
    LargeBendSettings settings;
    settings.radiusThicknessRatio = DoubleValue(largeBendRatio_);
    if (!std::isfinite(settings.radiusThicknessRatio) ||
        settings.radiusThicknessRatio <= 0.0)
        throw std::runtime_error("大圆弧判定倍数 R/t 必须大于 0（R 为内半径，t 为板厚）。");
    if (ToggleValue(divideLargeBend_))
    {
        std::unique_ptr<NXOpen::BlockStyler::PropertyList> properties(
            largeBendSegments_->GetProperties());
        settings.segments = properties->GetInteger("Value");
        if (settings.segments < 2 || settings.segments > 1000)
            throw std::runtime_error("大圆弧均分段数必须是 2～1000 的整数。");
    }
    return settings;
}

double BendMarkNotchDialog::DoubleValue(
    NXOpen::BlockStyler::UIBlock* block) const
{
    std::unique_ptr<NXOpen::BlockStyler::PropertyList> properties(block->GetProperties());
    const double value = properties->GetDouble("Value");
    return value;
}

void BendMarkNotchDialog::SetStatus(const std::string& text) const
{
    if (status_ == nullptr) return;
    std::unique_ptr<NXOpen::BlockStyler::PropertyList> properties(status_->GetProperties());
    properties->SetString(NXOpen::NXString("Label", NXOpen::NXString::UTF8),
                          NXOpen::NXString(text, NXOpen::NXString::UTF8));
}

void BendMarkNotchDialog::ShowError(const std::string& text) const noexcept
{
    AppendDebugLog("callback error: " + text);
    try
    {
        ui_->NXMessageBox()->Show("折弯标记缺口",
                              NXOpen::NXMessageBox::DialogTypeError,
                              text.c_str());
    }
    catch (...) { AppendDebugLog("Unable to display callback error"); }
}

bool BendMarkNotchDialog::IsSheetMetalBody(NXOpen::Body* body) const
{
    if (body == nullptr || session_ == nullptr) return false;
    try
    {
        NXOpen::Part* part = session_->Parts()->Work();
        return part != nullptr && part->Features() != nullptr &&
            part->Features()->SheetmetalManager()->GetBodyThickness(body) >
                kTolerance;
    }
    catch (...) { return false; }
}

std::vector<NXOpen::Body*> BendMarkNotchDialog::TargetBodies() const
{
    std::vector<NXOpen::Body*> result;
    NXOpen::Part* part = session_ == nullptr ? nullptr : session_->Parts()->Work();
    if (part == nullptr) return result;
    if (ToggleValue(autoSelect_))
    {
        for (NXOpen::Body* body : *part->Bodies())
            if (body != nullptr && body->IsSolidBody() &&
                IsSheetMetalBody(body) &&
                FindFlatPatternFeature(body) != nullptr)
                result.push_back(body);
    }
    else
    {
        auto* selection = dynamic_cast<NXOpen::BlockStyler::SelectObject*>(
            bodySelect_);
        if (selection != nullptr)
        {
            std::set<tag_t> tags;
            for (NXOpen::TaggedObject* object : selection->GetSelectedObjects())
            {
                NXOpen::Body* body = dynamic_cast<NXOpen::Body*>(object);
                if (body != nullptr && IsSheetMetalBody(body) &&
                    tags.insert(body->Tag()).second)
                    result.push_back(body);
            }
        }
    }
    return result;
}

int BendMarkNotchDialog::Execute()
{
    ReadLargeBendSettings();
    const bool separate = ToggleValue(distinguishBends_);
    if (separate)
    {
        ValidateNotchSettings(ReadNotchSettings(upNotch_), "上折：");
        ValidateNotchSettings(ReadNotchSettings(downNotch_), "下折：");
    }
    else
        ValidateNotchSettings(ReadNotchSettings(commonNotch_), "");
    if (editedFeatureTag_ != NULL_TAG && IsAlive(editedFeatureTag_))
        return ExecuteEdit();

    std::vector<NXOpen::Body*> bodies;
    const NXOpen::Session::UndoMarkId validationMark = session_->SetUndoMark(
        NXOpen::Session::MarkVisibilityInvisible,
        "折弯标记缺口扫描钣金体");
    try
    {
        bodies = TargetBodies();
        session_->DeleteUndoMark(validationMark, nullptr);
    }
    catch (...)
    {
        session_->DeleteUndoMark(validationMark, nullptr);
        throw;
    }
    if (bodies.empty()) throw std::runtime_error("请先创建展平特征");
    NXOpen::Part* workPart = session_->Parts()->Work();
    if (workPart == nullptr)
        throw std::runtime_error("当前没有可用的工作部件。");
    NXOpen::Features::Feature* originalCurrentFeature =
        workPart->CurrentFeature();

    const NXOpen::Session::UndoMarkId operationMark = session_->SetUndoMark(
        NXOpen::Session::MarkVisibilityVisible, "折弯标记缺口");
    DisplaySuppressionGuard displayGuard;
    pendingInternalFeatureTags_.clear();
    try
    {
        int bodyCount = 0;
        int notchCount = 0;
        bool rebuilt = false;
        bool showMarkSet = false;
        NXOpen::Session::UndoMarkId showMark =
            static_cast<NXOpen::Session::UndoMarkId>(0);
        for (NXOpen::Body* body : bodies)
        {
            NXOpen::Features::Feature* flatPattern =
                FindFlatPatternFeature(body);
            NXOpen::Features::Feature* insertionFeature =
                FindFeatureBefore(flatPattern);
            if (flatPattern == nullptr || insertionFeature == nullptr)
                throw std::runtime_error(
                    "未找到该钣金体的展平图样或其前一特征，"
                    "无法在展平图样前插入缺口特征。");
            // Editing or running again lands on the existing custom feature.
            // Keep that node: deleting it while NX is editing it raises a
            // memory access violation after the command returns. Roll back
            // before its old construction features so the new tools cut the
            // clean sheet, then retarget the same node.
            std::vector<NXOpen::Features::Feature*> existingNodes;
            std::set<tag_t> oldConstructionTags;
            tag_t reusedNotchTag = NULL_TAG;
            tag_t lastBuiltFeature = NULL_TAG;
            while (IsBendMarkNotchFeature(insertionFeature))
            {
                NXOpen::Features::Feature* previous =
                    FindFeatureBefore(insertionFeature);
                if (previous == nullptr)
                    throw std::runtime_error(
                        "无法替换已有的折弯标记缺口特征。");
                const int nodeTimestamp = insertionFeature->Timestamp();
                const int previousTimestamp = previous->Timestamp();
                for (NXOpen::Features::Feature* feature :
                     workPart->Features()->GetFeatures())
                {
                    if (feature == nullptr || !IsAlive(feature->Tag())) continue;
                    const int timestamp = feature->Timestamp();
                    if (timestamp > previousTimestamp && timestamp < nodeTimestamp)
                        oldConstructionTags.insert(feature->Tag());
                }
                try
                {
                    auto* custom = dynamic_cast<NXOpen::Features::CustomFeature*>(
                        insertionFeature);
                    if (custom != nullptr)
                    {
                        for (NXOpen::Features::ConstructionFeatureData* data :
                             custom->GetConstructionFeatures())
                        {
                            NXOpen::Features::Feature* owned =
                                data == nullptr ? nullptr : data->GetFeature();
                            if (owned != nullptr && IsAlive(owned->Tag()))
                                oldConstructionTags.insert(owned->Tag());
                        }
                    }
                }
                catch (const NXOpen::NXException& ex)
                {
                    AppendDebugLog(
                        "existing construction query failed code=" +
                        std::to_string(ex.ErrorCode()));
                }
                AppendDebugLog(
                    "reuse existing bend mark feature=" +
                    std::to_string(insertionFeature->Tag()) +
                    " previous=" + std::to_string(previous->Tag()) +
                    " oldConstruction=" +
                    std::to_string(oldConstructionTags.size()));
                if (reusedNotchTag == NULL_TAG)
                    reusedNotchTag = insertionFeature->Tag();
                existingNodes.push_back(insertionFeature);
                insertionFeature = previous;
            }
            // Read the committed upward face before rolling model history back.
            // A largest-web heuristic must not choose the meaning of up/down.
            NXOpen::Face* upwardFace = (separate || MarkingMethod(commonNotch_) == 1)
                ? FlatPatternUpwardFace(flatPattern) : nullptr;
            {
                std::ostringstream log;
                log << "body insertion body="
                    << (body == nullptr ? 0 : body->Tag())
                    << " beforeFlatPattern=" << flatPattern->Tag()
                    << " flatTimestamp=" << flatPattern->Timestamp()
                    << " insertionFeature=" << insertionFeature->Tag()
                    << " insertionTimestamp=" << insertionFeature->Timestamp()
                    << " restoreFeature="
                    << (originalCurrentFeature == nullptr
                            ? 0 : originalCurrentFeature->Tag());
                AppendDebugLog(log.str());
            }
            // The old node stays later in the tree. Committing a new unbend
            // updates it, its previous construction features fail, and NX
            // undoes the whole update ("Update undo happened"). Park it
            // first. Roll forward when an edit has already rolled the model
            // back, so the park runs against the feature that currently works.
            if (!existingNodes.empty())
            {
                NXOpen::Features::Feature* current = workPart->CurrentFeature();
                const bool rolledBack = current == nullptr ||
                    current->Timestamp() <= insertionFeature->Timestamp();
                if (rolledBack && flatPattern != nullptr &&
                    IsAlive(flatPattern->Tag()))
                {
                    try
                    {
                        AppendDebugLog(
                            "roll forward before replacing existing notch");
                        flatPattern->MakeCurrentFeature();
                    }
                    catch (const NXOpen::NXException& ex)
                    {
                        AppendDebugLog(
                            "roll forward failed code=" +
                            std::to_string(ex.ErrorCode()) + " message=" +
                            ex.Message());
                        LogUpdateUndoFeature();
                    }
                }
                std::vector<tag_t> nodeTags;
                for (NXOpen::Features::Feature* node : existingNodes)
                    if (node != nullptr) nodeTags.push_back(node->Tag());
                AppendDebugLog("suppress existing notch nodes=" +
                    std::to_string(nodeTags.size()));
                const int nodeError = ChangeFeatureSuppression(nodeTags, true);
                if (nodeError != 0)
                {
                    LogUpdateUndoFeature();
                    AppendDebugLog(
                        "suppress existing notch nodes failed code=" +
                        std::to_string(nodeError));
                    throw std::runtime_error(
                        "暂时停用已有的折弯标记缺口失败。");
                }
                const int constructionError = ChangeFeatureSuppression(
                    std::vector<tag_t>(oldConstructionTags.begin(),
                                       oldConstructionTags.end()),
                    true);
                if (constructionError != 0)
                    AppendDebugLog("suppress old construction code=" +
                        std::to_string(constructionError));
            }
            if (workPart->CurrentFeature() == nullptr ||
                workPart->CurrentFeature()->Tag() != insertionFeature->Tag())
                insertionFeature->MakeCurrentFeature();

            // Each body owns an independent custom-feature node.  Do not let
            // internal feature tags from a previous body leak into this one.
            pendingInternalFeatureTags_.clear();
            const int created = ProcessBody(body, upwardFace);
            if (created > 0)
            {
                const bool retargeted = !existingNodes.empty();
                if (retargeted)
                {
                    tag_t committedTag = NULL_TAG;
                    if (!RetargetCustomFeature(existingNodes.front(), committedTag))
                        throw std::runtime_error(
                            "更新已有的折弯标记缺口特征失败。");
                    if (committedTag != NULL_TAG)
                        reusedNotchTag = committedTag;
                }
                else if (!CreateCustomFeatureNode())
                    throw std::runtime_error(
                        "创建单个钣金体的“折弯标记缺口”自定义特征失败，"
                        "请重启 NX 后重试。");
                AppendDebugLog(
                    std::string(retargeted
                        ? "custom feature node retargeted for body="
                        : "custom feature node created for body=") +
                    std::to_string(body == nullptr ? 0 : body->Tag()) +
                    " internalFeatures=" +
                    std::to_string(pendingInternalFeatureTags_.size()));
                if (retargeted)
                {
                    std::set<tag_t> kept(
                        pendingInternalFeatureTags_.begin(),
                        pendingInternalFeatureTags_.end());
                    std::vector<NXOpen::TaggedObject*> superseded;
                    for (tag_t tag : oldConstructionTags)
                    {
                        if (!IsAlive(tag) || kept.count(tag) != 0) continue;
                        NXOpen::TaggedObject* object =
                            NXOpen::NXObjectManager::Get(tag);
                        if (object != nullptr) superseded.push_back(object);
                    }
                    for (std::size_t index = 1; index < existingNodes.size(); ++index)
                    {
                        NXOpen::Features::Feature* extra = existingNodes[index];
                        if (extra != nullptr && IsAlive(extra->Tag()))
                            superseded.push_back(extra);
                    }
                    // The new notches already exist. A failed unsuppress must
                    // not roll back to the mark that owns them. The old sketch
                    // is what fails; suppress that failure and keep the edit.
                    session_->SetUndoMark(
                        NXOpen::Session::MarkVisibilityVisible,
                        "折弯标记缺口已生成");
                    UF_MODL_update_option_t previousOption =
                        UF_MODL_UPDATE_UNDO;
                    const bool optionSet =
                        UF_MODL_ask_update_fail_option(&previousOption) == 0 &&
                        UF_MODL_set_update_fail_option(
                            UF_MODL_UPDATE_SUPPRESS) == 0;
                    const int unsuppressError = ChangeFeatureSuppression(
                        std::vector<tag_t>(1, reusedNotchTag),
                        false);
                    if (optionSet)
                        UF_MODL_set_update_fail_option(previousOption);
                    AppendDebugLog("unsuppress retargeted notch code=" +
                        std::to_string(unsuppressError) +
                        " saved=" + std::to_string(reusedNotchTag) +
                        " status=" +
                        std::to_string(UF_OBJ_ask_status(reusedNotchTag)));
                    if (unsuppressError != 0)
                    {
                        LogUpdateUndoFeature();
                        LogUpdateErrors();
                        bool newNotchesAlive = false;
                        for (tag_t tag : pendingInternalFeatureTags_)
                        {
                            if (IsAlive(tag))
                            {
                                newNotchesAlive = true;
                                break;
                            }
                        }
                        AppendDebugLog(
                            "unsuppress failed newNotchesAlive=" +
                            std::to_string(newNotchesAlive ? 1 : 0));
                        if (!newNotchesAlive)
                            throw std::runtime_error(
                                "恢复折弯标记缺口特征失败。");
                    }
                    if (!superseded.empty())
                    {
                        // These features are already suppressed. Deleting them
                        // also deletes the custom-feature node, which hides
                        // the new notches. Leave the old tools suppressed.
                        AppendDebugLog(
                            "keep suppressed old construction=" +
                            std::to_string(superseded.size()));
                    }
                    NXOpen::Features::Feature* afterUnsuppress =
                        workPart->CurrentFeature();
                    AppendDebugLog("after unsuppress current=" +
                        std::to_string(afterUnsuppress == nullptr
                            ? 0 : afterUnsuppress->Tag()));
                }
                rebuilt = true;
                ++bodyCount;
                notchCount += created;
                if (!pendingInternalFeatureTags_.empty())
                    lastBuiltFeature = pendingInternalFeatureTags_.back();
                pendingInternalFeatureTags_.clear();
            }

            // A menu run returns to the feature that was current. An edit
            // rolls NX back before the node being edited; restoring that
            // position hides the new notches. The flat pattern is later than
            // an unrelated feature that fails update and undoes the roll, so
            // stop on the reused node. Its cuts are then the current model.
            // CommitFeature and the later delete update drop the wrapper
            // that was current while editing, so its Tag() becomes 0.
            // Resolve the saved tag again before choosing what to display.
            NXOpen::Features::Feature* notchNode = nullptr;
            if (reusedNotchTag != NULL_TAG && IsAlive(reusedNotchTag))
            {
                notchNode = dynamic_cast<NXOpen::Features::Feature*>(
                    NXOpen::NXObjectManager::Get(reusedNotchTag));
                if (notchNode == nullptr || notchNode->Tag() != reusedNotchTag)
                {
                    notchNode = nullptr;
                    for (NXOpen::Features::Feature* feature :
                         workPart->Features()->GetFeatures())
                    {
                        if (feature != nullptr && feature->Tag() == reusedNotchTag)
                        {
                            notchNode = feature;
                            break;
                        }
                    }
                }
            }
            AppendDebugLog("restore notch tag=" +
                std::to_string(reusedNotchTag) + " resolved=" +
                std::to_string(notchNode == nullptr ? 0 : notchNode->Tag()) +
                " timestamp=" +
                std::to_string(notchNode == nullptr ? 0 : notchNode->Timestamp()));
            int nodeSuppressed = 0;
            if (reusedNotchTag != NULL_TAG && IsAlive(reusedNotchTag))
                UF_MODL_ask_suppress_feature(reusedNotchTag, &nodeSuppressed);
            NXOpen::Features::Feature* builtFeature = nullptr;
            if (lastBuiltFeature != NULL_TAG && IsAlive(lastBuiltFeature))
            {
                builtFeature = dynamic_cast<NXOpen::Features::Feature*>(
                    NXOpen::NXObjectManager::Get(lastBuiltFeature));
                if (builtFeature != nullptr &&
                    builtFeature->Tag() != lastBuiltFeature)
                    builtFeature = nullptr;
            }
            NXOpen::Features::Feature* restoreTarget = originalCurrentFeature;
            if (nodeSuppressed && builtFeature != nullptr)
            {
                AppendDebugLog(
                    "restore built feature because node stays suppressed=" +
                    std::to_string(builtFeature->Tag()));
                restoreTarget = builtFeature;
            }
            else if (notchNode != nullptr)
                restoreTarget = notchNode;
            else if (reusedNotchTag != NULL_TAG)
                restoreTarget = nullptr;
            else if (flatPattern != nullptr &&
                (restoreTarget == nullptr || !IsAlive(restoreTarget->Tag()) ||
                 (insertionFeature != nullptr &&
                  restoreTarget->Timestamp() <= insertionFeature->Timestamp())))
                restoreTarget = flatPattern;
            if (!showMarkSet)
            {
                // Latest visible mark. A failed refresh rolls back here,
                // after the notches exist, instead of cancelling the edit.
                showMark = session_->SetUndoMark(
                    NXOpen::Session::MarkVisibilityVisible,
                    "折弯标记缺口结果显示");
                showMarkSet = true;
            }
            if (restoreTarget != nullptr && IsAlive(restoreTarget->Tag()))
            {
                try
                {
                    AppendDebugLog("restore current feature target=" +
                        std::to_string(restoreTarget->Tag()));
                    restoreTarget->MakeCurrentFeature();
                }
                catch (const NXOpen::NXException& ex)
                {
                    AppendDebugLog("restore current feature failed code=" +
                        std::to_string(ex.ErrorCode()) + " message=" +
                        ex.Message());
                    LogUpdateUndoFeature();
                    LogUpdateErrors();
                    if (!rebuilt) throw;
                    // The failed roll undoes itself and can leave the node
                    // looking inactive. Accept that one failure and stop on
                    // the reused node so the new notches stay current.
                    NXOpen::Features::Feature* kept = nullptr;
                    if (reusedNotchTag != NULL_TAG && IsAlive(reusedNotchTag))
                    {
                        kept = dynamic_cast<NXOpen::Features::Feature*>(
                            NXOpen::NXObjectManager::Get(reusedNotchTag));
                        if (kept != nullptr && kept->Tag() != reusedNotchTag)
                            kept = nullptr;
                        if (kept == nullptr)
                        {
                            for (NXOpen::Features::Feature* feature :
                                 workPart->Features()->GetFeatures())
                            {
                                if (feature != nullptr &&
                                    feature->Tag() == reusedNotchTag)
                                {
                                    kept = feature;
                                    break;
                                }
                            }
                        }
                    }
                    AppendDebugLog("restore kept notch feature=" +
                        std::to_string(kept == nullptr ? 0 : kept->Tag()) +
                        " alive=" +
                        std::to_string(kept != nullptr && IsAlive(kept->Tag()) ? 1 : 0));
                    if (kept != nullptr && IsAlive(kept->Tag()))
                    {
                        UF_MODL_update_option_t previousOption =
                            UF_MODL_UPDATE_UNDO;
                        const bool optionSet =
                            UF_MODL_ask_update_fail_option(&previousOption) == 0 &&
                            UF_MODL_set_update_fail_option(
                                UF_MODL_UPDATE_ACCEPT_ALL) == 0;
                        try
                        {
                            kept->MakeCurrentFeature();
                            AppendDebugLog("restore kept notch current=" +
                                std::to_string(kept->Tag()));
                        }
                        catch (const NXOpen::NXException& keptEx)
                        {
                            AppendDebugLog(
                                "restore kept notch failed code=" +
                                std::to_string(keptEx.ErrorCode()) +
                                " message=" + keptEx.Message());
                            LogUpdateUndoFeature();
                            LogUpdateErrors();
                        }
                        if (optionSet)
                            UF_MODL_set_update_fail_option(previousOption);
                    }
                }
            }
            if (reusedNotchTag != NULL_TAG)
            {
                AppendDebugLog("uf notch status=" +
                    std::to_string(UF_OBJ_ask_status(reusedNotchTag)) +
                    " alive=" +
                    std::to_string(IsAlive(reusedNotchTag) ? 1 : 0));
            }
            if (nodeSuppressed && builtFeature != nullptr &&
                IsAlive(builtFeature->Tag()))
            {
                AppendDebugLog("skip set current on suppressed notch node");
            }
            else if (reusedNotchTag != NULL_TAG && IsAlive(reusedNotchTag))
            {
                tag_t currentTag = NULL_TAG;
                UF_MODL_ask_current_feature(workPart->Tag(), &currentTag);
                if (currentTag != reusedNotchTag)
                {
                    int suppressed = 0;
                    int objectType = 0;
                    int objectSubtype = 0;
                    UF_MODL_ask_suppress_feature(reusedNotchTag, &suppressed);
                    UF_OBJ_ask_type_and_subtype(
                        reusedNotchTag, &objectType, &objectSubtype);
                    AppendDebugLog(
                        "uf notch before set current suppressed=" +
                        std::to_string(suppressed) +
                        " type=" + std::to_string(objectType) +
                        " subtype=" + std::to_string(objectSubtype) +
                        " current=" + std::to_string(currentTag));
                    UF_MODL_update_option_t previousOption =
                        UF_MODL_UPDATE_UNDO;
                    const bool optionSet =
                        UF_MODL_ask_update_fail_option(&previousOption) == 0 &&
                        UF_MODL_set_update_fail_option(
                            UF_MODL_UPDATE_ACCEPT_ALL) == 0;
                    const int setCode =
                        UF_MODL_set_current_feature(reusedNotchTag);
                    tag_t afterTag = NULL_TAG;
                    UF_MODL_ask_current_feature(workPart->Tag(), &afterTag);
                    AppendDebugLog("uf set current notch code=" +
                        std::to_string(setCode) +
                        " after=" + std::to_string(afterTag));
                    LogUpdateUndoFeature();
                    if (optionSet)
                        UF_MODL_set_update_fail_option(previousOption);
                }
            }
        }
        if (notchCount == 0)
            throw std::runtime_error("未找到可伸直并标记的折弯。");
        if (!showMarkSet)
        {
            showMark = session_->SetUndoMark(
                NXOpen::Session::MarkVisibilityVisible,
                "折弯标记缺口结果显示");
            showMarkSet = true;
        }
        int updateErrors = 0;
        try
        {
            AppendDebugLog("final update begin rebuilt=" +
                std::string(rebuilt ? "1" : "0"));
            updateErrors = session_->UpdateManager()->DoUpdate(showMark);
        }
        catch (const NXOpen::NXException& ex)
        {
            AppendDebugLog("final update failed code=" +
                std::to_string(ex.ErrorCode()) + " message=" + ex.Message());
            LogUpdateUndoFeature();
            LogUpdateErrors();
            if (!rebuilt) throw;
            updateErrors = ex.ErrorCode();
        }
        {
            NXOpen::Features::Feature* afterRestore =
                workPart->CurrentFeature();
            std::ostringstream log;
            log << "restore original current feature after="
                << (afterRestore == nullptr ? 0 : afterRestore->Tag())
                << " expected="
                << (originalCurrentFeature == nullptr
                        ? 0 : originalCurrentFeature->Tag())
                << " updateErrors=" << updateErrors;
            AppendDebugLog(log.str());
        }

        std::ostringstream message;
        message << "完成：处理 " << bodyCount << " 个钣金体，创建 "
                << notchCount << " 个折弯端部缺口。";
        SetStatus(message.str());
        pendingInternalFeatureTags_.clear();
        return 0;
    }
    catch (...)
    {
        pendingInternalFeatureTags_.clear();
        session_->UndoToMark(operationMark, "折弯标记缺口失败回退");
        throw;
    }
}

int BendMarkNotchDialog::ExecuteEdit()
{
    // Editing never modifies the existing node. NX replays a node's old
    // construction features when its edit session ends, which is what breaks
    // the second edit. The dialog only collects the values; the old node is
    // deleted and a fresh one is created once NX has finished editing.
    cachedDialog_ = ReadDialogSettings();
    cachedLarge_ = cachedDialog_.large;
    cachedSeparate_ = cachedDialog_.separate;
    cachedUp_ = cachedDialog_.separate ? cachedDialog_.up : cachedDialog_.common;
    cachedDown_ = cachedDialog_.separate ? cachedDialog_.down : cachedUp_;
    editRebuildPending_ = true;
    AppendDebugLog("edit accepted feature=" + std::to_string(editedFeatureTag_));
    return 0;
}

void BendMarkNotchDialog::RebuildEditedFeature()
{
    NXOpen::Part* part = session_->Parts()->Work();
    auto* node = dynamic_cast<NXOpen::Features::CustomFeature*>(
        IsAlive(editedFeatureTag_)
            ? NXOpen::NXObjectManager::Get(editedFeatureTag_) : nullptr);
    if (part == nullptr || node == nullptr)
        throw std::runtime_error("未找到正在编辑的折弯标记缺口特征。");

    std::vector<tag_t> oldChain;
    std::set<tag_t> oldSet;
    const auto addOld = [&](NXOpen::TaggedObject* object)
    {
        auto* feature = dynamic_cast<NXOpen::Features::Feature*>(object);
        if (feature == nullptr || !IsAlive(feature->Tag()) ||
            feature->Tag() == editedFeatureTag_ ||
            !oldSet.insert(feature->Tag()).second)
            return;
        oldChain.push_back(feature->Tag());
    };
    for (NXOpen::Features::ConstructionFeatureData* data :
         node->GetConstructionFeatures())
        if (data != nullptr) addOld(data->GetFeature());
    try
    {
        NXOpen::Features::CustomFeatureData* data = node->FeatureData();
        NXOpen::Features::CustomTagArrayAttribute* attribute =
            data == nullptr ? nullptr :
            data->CustomTagArrayAttributeByName(
                zhihui_bend_mark_notch::kAttrInternalFeatures);
        if (attribute != nullptr)
            for (NXOpen::TaggedObject* object : attribute->GetValues())
                addOld(object);
    }
    catch (const NXOpen::NXException& ex)
    {
        AppendDebugLog("rebuild attribute query failed code=" +
            std::to_string(ex.ErrorCode()));
    }

    int boundary = node->Timestamp();
    for (tag_t tag : oldChain)
    {
        auto* feature = dynamic_cast<NXOpen::Features::Feature*>(
            NXOpen::NXObjectManager::Get(tag));
        if (feature != nullptr)
            boundary = (std::min)(boundary, feature->Timestamp());
    }
    NXOpen::Features::Feature* insertion = nullptr;
    for (NXOpen::Features::Feature* feature : part->Features()->GetFeatures())
    {
        if (feature == nullptr || !IsAlive(feature->Tag()) ||
            feature->IsInternal() || oldSet.count(feature->Tag()) != 0 ||
            feature->Tag() == editedFeatureTag_ ||
            feature->Timestamp() >= boundary)
            continue;
        if (insertion == nullptr || feature->Timestamp() > insertion->Timestamp())
            insertion = feature;
    }
    NXOpen::Body* body = nullptr;
    if (insertion != nullptr)
        for (NXOpen::Body* candidate : insertion->GetBodies())
            if (candidate != nullptr && IsAlive(candidate->Tag()) &&
                candidate->IsSolidBody())
            {
                body = candidate;
                break;
            }
    NXOpen::Features::Feature* originalCurrent = part->CurrentFeature();
    tag_t restoreTag = originalCurrent == nullptr ? NULL_TAG : originalCurrent->Tag();
    if (restoreTag == editedFeatureTag_ || oldSet.count(restoreTag) != 0)
        restoreTag = NULL_TAG;
    {
        std::ostringstream log;
        log << "rebuild begin feature=" << editedFeatureTag_
            << " timestamp=" << node->Timestamp()
            << " current=" << (originalCurrent == nullptr ? 0 : originalCurrent->Tag())
            << " restore=" << restoreTag
            << " insertion=" << (insertion == nullptr ? 0 : insertion->Tag())
            << " body=" << (body == nullptr ? 0 : body->Tag())
            << " old=";
        for (tag_t tag : oldChain) log << tag << ",";
        AppendDebugLog(log.str());
    }
    if (insertion == nullptr || body == nullptr)
        throw std::runtime_error("未找到折弯标记缺口所在的钣金体。");
    const tag_t insertionTag = insertion->Tag();

    const tag_t bodyTag = body->Tag();
    const NXOpen::Session::UndoMarkId operationMark = session_->SetUndoMark(
        NXOpen::Session::MarkVisibilityVisible, "折弯标记缺口");
    DisplaySuppressionGuard displayGuard;
    pendingInternalFeatureTags_.clear();
    createdNodeTag_ = NULL_TAG;
    useCachedSettings_ = true;
    try
    {
        std::vector<NXOpen::TaggedObject*> doomed;
        for (auto item = oldChain.rbegin(); item != oldChain.rend(); ++item)
        {
            NXOpen::TaggedObject* object = NXOpen::NXObjectManager::Get(*item);
            if (object != nullptr) doomed.push_back(object);
        }
        doomed.push_back(node);
        NXOpen::Update* update = session_->UpdateManager();
        update->ClearDeleteList();
        update->ClearErrorList();
        update->AddObjectsToDeleteList(doomed);
        const int deleteErrors = update->DoUpdate(operationMark);
        std::size_t survivors = 0;
        for (tag_t tag : oldChain)
            if (IsAlive(tag)) ++survivors;
        AppendDebugLog("rebuild delete old node count=" +
            std::to_string(doomed.size()) +
            " errors=" + std::to_string(deleteErrors) +
            " nodeAlive=" + std::to_string(IsAlive(editedFeatureTag_) ? 1 : 0) +
            " survivors=" + std::to_string(survivors));
        if (deleteErrors != 0) LogUpdateErrors();
        if (IsAlive(editedFeatureTag_) || survivors != 0)
            throw std::runtime_error("删除旧的折弯标记缺口失败。");

        body = IsAlive(bodyTag)
            ? dynamic_cast<NXOpen::Body*>(NXOpen::NXObjectManager::Get(bodyTag))
            : nullptr;
        insertion = IsAlive(insertionTag)
            ? dynamic_cast<NXOpen::Features::Feature*>(
                  NXOpen::NXObjectManager::Get(insertionTag))
            : nullptr;
        if (body == nullptr || insertion == nullptr)
            throw std::runtime_error("未找到折弯标记缺口所在的钣金体。");
        NXOpen::Features::Feature* flatPattern = FindFlatPatternFeature(body);
        NXOpen::Face* upwardFace =
            flatPattern != nullptr && (cachedSeparate_ || cachedUp_.lineMark)
                ? FlatPatternUpwardFace(flatPattern) : nullptr;
        if (part->CurrentFeature() == nullptr ||
            part->CurrentFeature()->Tag() != insertionTag)
            insertion->MakeCurrentFeature();

        const int created = ProcessBody(body, upwardFace);
        if (created <= 0) throw std::runtime_error("未找到可伸直并标记的折弯。");
        if (!CreateCustomFeatureNode())
            throw std::runtime_error("创建折弯标记缺口特征失败。");

        if (restoreTag != NULL_TAG && IsAlive(restoreTag))
        {
            auto* restore = dynamic_cast<NXOpen::Features::Feature*>(
                NXOpen::NXObjectManager::Get(restoreTag));
            if (restore != nullptr) restore->MakeCurrentFeature();
        }
        auto* rebuilt = dynamic_cast<NXOpen::Features::CustomFeature*>(
            IsAlive(createdNodeTag_)
                ? NXOpen::NXObjectManager::Get(createdNodeTag_) : nullptr);
        std::set<tag_t> adopted;
        if (rebuilt != nullptr)
            for (NXOpen::Features::ConstructionFeatureData* data :
                 rebuilt->GetConstructionFeatures())
            {
                NXOpen::Features::Feature* feature =
                    data == nullptr ? nullptr : data->GetFeature();
                if (feature != nullptr) adopted.insert(feature->Tag());
            }
        std::size_t adoptedNew = 0;
        for (tag_t tag : pendingInternalFeatureTags_)
            if (adopted.count(tag) != 0) ++adoptedNew;
        int nodeSuppressed = 0;
        if (rebuilt != nullptr)
            UF_MODL_ask_suppress_feature(createdNodeTag_, &nodeSuppressed);
        NXOpen::Features::Feature* current = part->CurrentFeature();
        AppendDebugLog("rebuild done node=" + std::to_string(createdNodeTag_) +
            " suppressed=" + std::to_string(nodeSuppressed) +
            " adoptedNew=" + std::to_string(adoptedNew) + "/" +
            std::to_string(pendingInternalFeatureTags_.size()) +
            " current=" + std::to_string(current == nullptr ? 0 : current->Tag()));
        if (rebuilt == nullptr || nodeSuppressed != 0 ||
            adoptedNew != pendingInternalFeatureTags_.size())
            throw std::runtime_error("折弯标记缺口特征未能正确封装内部特征。");

        try
        {
            SetStatus("完成：创建 " + std::to_string(created) + " 个折弯端部缺口。");
        }
        catch (...) {}
        useCachedSettings_ = false;
        pendingInternalFeatureTags_.clear();
        editedFeatureTag_ = createdNodeTag_;
    }
    catch (...)
    {
        useCachedSettings_ = false;
        pendingInternalFeatureTags_.clear();
        session_->UndoToMark(operationMark, "折弯标记缺口失败回退");
        throw;
    }
}
bool BendMarkNotchDialog::RetargetCustomFeature(
    NXOpen::Features::Feature* feature,
    tag_t& committedTag)
{
    auto* custom = dynamic_cast<NXOpen::Features::CustomFeature*>(feature);
    NXOpen::Part* part = session_->Parts()->Work();
    if (custom == nullptr || part == nullptr ||
        pendingInternalFeatureTags_.empty() || !IsAlive(custom->Tag()))
        return false;
    std::vector<NXOpen::TaggedObject*> internalObjects;
    std::set<tag_t> uniqueTags;
    for (tag_t tag : pendingInternalFeatureTags_)
    {
        if (!IsAlive(tag) || !uniqueTags.insert(tag).second) continue;
        NXOpen::TaggedObject* object = NXOpen::NXObjectManager::Get(tag);
        if (dynamic_cast<NXOpen::Features::Feature*>(object) != nullptr)
            internalObjects.push_back(object);
    }
    if (internalObjects.empty()) return false;
    NXOpen::Features::CustomFeatureBuilder* builder = nullptr;
    try
    {
        builder = part->Features()->CreateCustomFeatureBuilder(custom);
        AppendDebugLog("retarget suppressed=" +
            std::string(custom->Suppressed() ? "1" : "0") +
            " feature=" + std::to_string(custom->Tag()));
        NXOpen::Features::CustomFeatureData* data = builder->FeatureData();
        NXOpen::Features::CustomTagArrayAttribute* attribute =
            data == nullptr ? nullptr :
            data->CustomTagArrayAttributeByName(
                zhihui_bend_mark_notch::kAttrInternalFeatures);
        if (attribute == nullptr)
        {
            builder->Destroy();
            return false;
        }
        attribute->SetValues(internalObjects);
        NXOpen::Features::Feature* committed = builder->CommitFeature();
        builder->Destroy();
        builder = nullptr;
        committedTag = committed == nullptr ? NULL_TAG : committed->Tag();
        AppendDebugLog(
            "retarget committed=" + std::to_string(committedTag) +
            " committedAlive=" +
            std::to_string(committedTag != NULL_TAG && IsAlive(committedTag) ? 1 : 0) +
            " custom=" + std::to_string(custom->Tag()) +
            " customStatus=" + std::to_string(UF_OBJ_ask_status(custom->Tag())));
        return committedTag != NULL_TAG && IsAlive(committedTag);
    }
    catch (const NXOpen::NXException& ex)
    {
        AppendDebugLog("retarget custom feature failed code=" +
            std::to_string(ex.ErrorCode()) + " message=" + ex.Message());
        if (builder != nullptr) builder->Destroy();
        return false;
    }
    catch (...)
    {
        if (builder != nullptr) builder->Destroy();
        return false;
    }
}

bool BendMarkNotchDialog::CreateCustomFeatureNode()
{
    NXOpen::Part* part = session_->Parts()->Work();
    if (part == nullptr || pendingInternalFeatureTags_.empty()) return false;
    NXOpen::Features::CustomFeatureClass* featureClass = nullptr;
    try
    {
        featureClass = session_->CustomFeatureClassManager()->GetClassFromName(
            zhihui_bend_mark_notch::kFeatureClassName);
    }
    catch (const NXOpen::NXException&)
    {
        // A newly deployed custom-feature class is registered only when NX
        // starts. Treat a pre-deployment NX session as an unavailable class
        // so Execute can roll back and show one actionable message.
        return false;
    }
    if (featureClass == nullptr) return false;

    std::vector<NXOpen::TaggedObject*> internalObjects;
    std::set<tag_t> uniqueTags;
    for (tag_t tag : pendingInternalFeatureTags_)
    {
        if (!IsAlive(tag) || !uniqueTags.insert(tag).second) continue;
        NXOpen::TaggedObject* object = NXOpen::NXObjectManager::Get(tag);
        if (dynamic_cast<NXOpen::Features::Feature*>(object) != nullptr)
            internalObjects.push_back(object);
    }
    if (internalObjects.empty()) return false;

    NXOpen::Features::CustomFeatureBuilder* builder = nullptr;
    try
    {
        NXOpen::Features::CustomAttributeCollection* attributes =
            part->Features()->CustomAttributeCollection();
        const std::vector<NXOpen::Features::CustomAttribute::Property>
            properties{
                NXOpen::Features::CustomAttribute::PropertyIsOutputAttribute,
                NXOpen::Features::CustomAttribute::PropertyIsOwnedAttribute};
        NXOpen::Features::CustomTagArrayAttribute* internalAttribute =
            attributes->CreateCustomTagArrayAttribute(
                zhihui_bend_mark_notch::kAttrInternalFeatures, properties);
        NXOpen::Features::CustomStringAttribute* settingsAttribute =
            attributes->CreateCustomStringAttribute(
                zhihui_bend_mark_notch::kAttrSettings,
                std::vector<NXOpen::Features::CustomAttribute::Property>());
        NXOpen::Features::CustomFeatureData* data =
            part->Features()->CustomFeatureDataCollection()->CreateData(
                featureClass,
                std::vector<NXOpen::Features::CustomAttribute*>{
                    internalAttribute, settingsAttribute});
        internalAttribute->SetValues(internalObjects);
        settingsAttribute->SetValue(NXOpen::NXString(
            EncodeDialogSettings(useCachedSettings_
                ? cachedDialog_ : ReadDialogSettings()),
            NXOpen::NXString::UTF8));
        builder = part->Features()->CreateCustomFeatureBuilder(nullptr);
        builder->SetFeatureData(data);
        NXOpen::Features::Feature* feature = builder->CommitFeature();
        builder->Destroy();
        builder = nullptr;
        if (feature == nullptr) return false;
        feature->SetName(zhihui_bend_mark_notch::kFeatureDisplayName);
        createdNodeTag_ = feature->Tag();
        return true;
    }
    catch (...)
    {
        if (builder != nullptr) builder->Destroy();
        return false;
    }
}

NXOpen::Features::Feature* BendMarkNotchDialog::FindFlatPatternFeature(
    NXOpen::Body* body) const
{
    NXOpen::Part* part = session_ == nullptr ? nullptr : session_->Parts()->Work();
    if (part == nullptr || body == nullptr || !IsAlive(body->Tag()))
        return nullptr;

    // Prefer the direct sheet-metal association when it resolves to the
    // actual FlatPattern feature.
    tag_t directTag = NULL_TAG;
    if (UF_SMD_ask_flat_pattern(body->Tag(), &directTag) == 0 &&
        IsAlive(directTag))
    {
        auto* direct = dynamic_cast<NXOpen::Features::FlatPattern*>(
            NXOpen::NXObjectManager::Get(directTag));
        if (direct != nullptr) return direct;
    }

    // Some parts return the legacy flat-pattern group tag above.  In that
    // case identify the feature by its committed upward face on the formed
    // body, which also distinguishes flat patterns in multi-body parts.
    std::set<tag_t> bodyFaceTags;
    for (NXOpen::Face* face : body->GetFaces())
        if (face != nullptr && IsAlive(face->Tag()))
            bodyFaceTags.insert(face->Tag());

    auto* manager = part->Features()->SheetmetalManager();
    NXOpen::Features::Feature* best = nullptr;
    for (NXOpen::Features::Feature* feature : part->Features()->GetFeatures())
    {
        if (feature == nullptr || !IsAlive(feature->Tag()) ||
            dynamic_cast<NXOpen::Features::FlatPattern*>(feature) == nullptr)
            continue;
        NXOpen::Features::SheetMetal::FlatPatternBuilder* builder = nullptr;
        try
        {
            builder = manager->CreateFlatPatternBuilder(feature);
            NXOpen::Face* upward = builder == nullptr ||
                    builder->UpwardFace() == nullptr
                ? nullptr : builder->UpwardFace()->Value();
            const bool matched = upward != nullptr &&
                bodyFaceTags.find(upward->Tag()) != bodyFaceTags.end();
            if (builder != nullptr) builder->Destroy();
            builder = nullptr;
            if (matched && (best == nullptr ||
                feature->Timestamp() > best->Timestamp()))
                best = feature;
        }
        catch (...)
        {
            if (builder != nullptr)
            {
                try { builder->Destroy(); }
                catch (...) {}
            }
        }
    }
    return best;
}

NXOpen::Features::Feature* BendMarkNotchDialog::FindFeatureBefore(
    NXOpen::Features::Feature* target) const
{
    NXOpen::Part* part = session_ == nullptr ? nullptr : session_->Parts()->Work();
    if (part == nullptr || target == nullptr || !IsAlive(target->Tag()))
        return nullptr;
    const int targetTimestamp = target->Timestamp();
    NXOpen::Features::Feature* previous = nullptr;
    for (NXOpen::Features::Feature* feature : part->Features()->GetFeatures())
    {
        if (feature == nullptr || !IsAlive(feature->Tag()) ||
            feature->IsInternal() || feature->Timestamp() >= targetTimestamp)
            continue;
        if (previous == nullptr ||
            feature->Timestamp() > previous->Timestamp())
            previous = feature;
    }
    return previous;
}

bool BendMarkNotchDialog::IsBendMarkNotchFeature(
    NXOpen::Features::Feature* feature) const
{
    auto* custom = dynamic_cast<NXOpen::Features::CustomFeature*>(feature);
    if (custom == nullptr || !IsAlive(custom->Tag())) return false;
    try
    {
        NXOpen::Features::CustomFeatureData* data = custom->FeatureData();
        if (data == nullptr) return false;
        const NXOpen::NXString className = data->ClassName();
        const char* text = className.GetUTF8Text();
        if (text == nullptr) return false;
        const std::string name(text);
        return name == zhihui_bend_mark_notch::kFeatureClassName ||
            name == "BendMarkNotch";
    }
    catch (const NXOpen::NXException&)
    {
        return false;
    }
}

NXOpen::Features::Feature* BendMarkNotchDialog::FindBodyInsertionFeature(
    const std::vector<NXOpen::Body*>& bodies) const
{
    NXOpen::Features::Feature* insertion = nullptr;
    for (NXOpen::Body* body : bodies)
    {
        uf_list_p_t featureList = nullptr;
        if (body == nullptr || UF_MODL_ask_body_feats(
                body->Tag(), &featureList) != 0 || featureList == nullptr)
            return nullptr;
        int count = 0;
        if (UF_MODL_ask_list_count(featureList, &count) != 0)
        {
            UF_MODL_delete_list(&featureList);
            return nullptr;
        }
        NXOpen::Features::Feature* bodyInsertion = nullptr;
        for (int index = 0; index < count; ++index)
        {
            tag_t featureTag = NULL_TAG;
            if (UF_MODL_ask_list_item(featureList, index, &featureTag) != 0 ||
                !IsAlive(featureTag))
                continue;
            auto* feature = dynamic_cast<NXOpen::Features::Feature*>(
                NXOpen::NXObjectManager::Get(featureTag));
            if (feature == nullptr || feature->IsInternal()) continue;
            bool ownsBody = false;
            try
            {
                for (NXOpen::Body* featureBody : feature->GetBodies())
                    if (featureBody != nullptr &&
                        featureBody->Tag() == body->Tag())
                    {
                        ownsBody = true;
                        break;
                    }
            }
            catch (...) { ownsBody = false; }
            if (ownsBody && (bodyInsertion == nullptr ||
                feature->Timestamp() > bodyInsertion->Timestamp()))
                bodyInsertion = feature;
        }
        UF_MODL_delete_list(&featureList);
        if (bodyInsertion == nullptr) return nullptr;
        if (insertion == nullptr ||
            bodyInsertion->Timestamp() > insertion->Timestamp())
            insertion = bodyInsertion;
    }
    return insertion;
}

NXOpen::Face* BendMarkNotchDialog::FindReferenceFace(NXOpen::Body* body) const
{
    if (body == nullptr) return nullptr;

    NXOpen::Part* part = session_->Parts()->Work();
    if (part == nullptr) return nullptr;
    auto* manager = part->Features()->SheetmetalManager();
    if (manager == nullptr) return nullptr;

    NXOpen::Face* best = nullptr;
    double bestScore = -1.0;
    for (NXOpen::Face* face : body->GetFaces())
    {
        int type = 0, normalDirection = 0;
        double point[3] = {}, direction[3] = {}, box[6] = {};
        double radius = 0.0, radiusData = 0.0;
        if (face == nullptr || UF_MODL_ask_face_data(
                face->Tag(), &type, point, direction, box, &radius,
                &radiusData, &normalDirection) != 0 ||
            type != UF_MODL_PLANAR_FACE)
            continue;

        // Unbend accepts only a planar sheet-metal web as its stationary
        // entity.  A geometrically planar face may belong to a normal solid,
        // a boolean result, or a thickness side and therefore have no sheet-
        // metal attribute.  Passing such a face makes NX report error 3675320
        // ("Face has no SM attribute" / "Select planar stationary face").
        try
        {
            if (manager->GetFaceType(face) !=
                    NXOpen::Features::SheetMetal::SheetmetalFaceTypeWeb)
                continue;
        }
        catch (const NXOpen::NXException&)
        {
            continue;
        }

        const double dx = box[3] - box[0];
        const double dy = box[4] - box[1];
        const double dz = box[5] - box[2];
        const double score = dx * dx + dy * dy + dz * dz;
        if (score > bestScore) { bestScore = score; best = face; }
    }
    return best;
}

NXOpen::Face* BendMarkNotchDialog::FindOppositeBendFace(
    NXOpen::Body* body, NXOpen::Face* innerFace, double thickness) const
{
    int type = 0, normalDirection = 0;
    double axisPoint[3] = {}, axisData[3] = {}, box[6] = {};
    double innerRadius = 0.0, radiusData = 0.0;
    if (UF_MODL_ask_face_data(innerFace->Tag(), &type, axisPoint, axisData,
                              box, &innerRadius, &radiusData,
                              &normalDirection) != 0 ||
        type != UF_MODL_CYLINDRICAL_FACE)
        return nullptr;
    const NXOpen::Point3d origin(axisPoint[0], axisPoint[1], axisPoint[2]);
    const NXOpen::Vector3d axis = Normalize(
        {axisData[0], axisData[1], axisData[2]});
    NXOpen::Face* best = nullptr;
    double bestScore = DBL_MAX;
    for (NXOpen::Face* candidate : body->GetFaces())
    {
        if (candidate == nullptr || candidate->Tag() == innerFace->Tag())
            continue;
        int candidateType = 0, candidateNormal = 0;
        double candidatePoint[3] = {}, candidateAxisData[3] = {},
               candidateBox[6] = {};
        double candidateRadius = 0.0, candidateRadiusData = 0.0;
        if (UF_MODL_ask_face_data(candidate->Tag(), &candidateType,
                candidatePoint, candidateAxisData, candidateBox,
                &candidateRadius, &candidateRadiusData, &candidateNormal) != 0 ||
            candidateType != UF_MODL_CYLINDRICAL_FACE)
            continue;
        const NXOpen::Vector3d candidateAxis = Normalize(
            {candidateAxisData[0], candidateAxisData[1], candidateAxisData[2]});
        if (std::fabs(Dot(axis, candidateAxis)) < 0.9999) continue;
        const NXOpen::Vector3d offset = Subtract(
            {candidatePoint[0], candidatePoint[1], candidatePoint[2]}, origin);
        const NXOpen::Vector3d perpendicular =
            {offset.X - axis.X * Dot(offset, axis),
             offset.Y - axis.Y * Dot(offset, axis),
             offset.Z - axis.Z * Dot(offset, axis)};
        if (Length(perpendicular) > 1.0e-3) continue;
        const double radiusDifference = std::fabs(candidateRadius - innerRadius);
        if (radiusDifference <= kTolerance) continue;
        const double score = std::fabs(radiusDifference - thickness);
        if (score < bestScore) { bestScore = score; best = candidate; }
    }
    return bestScore <= (std::max)(0.02, thickness * 0.1) ? best : nullptr;
}

bool BendMarkNotchDialog::FindBoundaryEdges(
    NXOpen::Face* bendFace, NXOpen::Edge*& first, NXOpen::Edge*& second) const
{
    first = nullptr;
    second = nullptr;
    int type = 0, normalDirection = 0;
    double point[3] = {}, axisData[3] = {}, box[6] = {};
    double radius = 0.0, radiusData = 0.0;
    if (UF_MODL_ask_face_data(bendFace->Tag(), &type, point, axisData, box,
                              &radius, &radiusData, &normalDirection) != 0 ||
        type != UF_MODL_CYLINDRICAL_FACE)
        return false;
    const NXOpen::Vector3d axis = Normalize(
        {axisData[0], axisData[1], axisData[2]});
    std::vector<std::pair<double, NXOpen::Edge*>> candidates;
    for (NXOpen::Edge* edge : bendFace->GetEdges())
    {
        NXOpen::Point3d a, b;
        if (!EdgeEnds(edge, a, b)) continue;
        const NXOpen::Vector3d direction = Normalize(Subtract(b, a));
        if (std::fabs(Dot(direction, axis)) >= 0.999)
            candidates.emplace_back(Distance(a, b), edge);
    }
    std::sort(candidates.begin(), candidates.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });
    if (candidates.size() < 2) return false;
    first = candidates[0].second;
    NXOpen::Point3d firstStart, firstEnd;
    if (!EdgeEnds(first, firstStart, firstEnd)) return false;
    for (std::size_t index = 1; index < candidates.size(); ++index)
    {
        NXOpen::Point3d start, end;
        if (!EdgeEnds(candidates[index].second, start, end)) continue;
        // A tangent boundary may be split into several collinear edges. The
        // other boundary must be on a different cylinder generatrix.
        if (Length(Cross(Subtract(start, firstStart), axis)) <= kTolerance)
            continue;
        second = candidates[index].second;
        return true;
    }
    return false;
}

std::vector<BendMarkNotchDialog::BendRecord>
BendMarkNotchDialog::CollectBends(NXOpen::Body* body, double thickness) const
{
    NXOpen::Part* part = session_->Parts()->Work();
    auto* manager = part->Features()->SheetmetalManager();
    std::vector<NXOpen::Face*> innerFaces;
    std::vector<NXOpen::Features::SheetMetal::SheetmetalBendState> states;
    manager->GetInnerBendFaces(body, innerFaces, states);
    AppendDebugLog("CollectBends query body=" +
        std::to_string(body == nullptr ? 0 : body->Tag()) +
        " innerFaces=" + std::to_string(innerFaces.size()) +
        " states=" + std::to_string(states.size()));
    std::vector<BendRecord> result;
    std::set<tag_t> seen;
    for (std::size_t index = 0; index < innerFaces.size(); ++index)
    {
        NXOpen::Face* inner = innerFaces[index];
        if (inner == nullptr)
        {
            AppendDebugLog("CollectBends skip index=" +
                std::to_string(index) + " reason=null-inner-face");
            continue;
        }
        if (!seen.insert(inner->Tag()).second)
        {
            AppendDebugLog("CollectBends skip index=" +
                std::to_string(index) + " inner=" +
                std::to_string(inner->Tag()) + " reason=duplicate-inner-face");
            continue;
        }
        if (index < states.size() && states[index] ==
                NXOpen::Features::SheetMetal::SheetmetalBendStateFlat)
        {
            AppendDebugLog("CollectBends skip index=" +
                std::to_string(index) + " inner=" +
                std::to_string(inner->Tag()) + " reason=already-flat");
            continue;
        }
        BendRecord record;
        record.innerFace = inner;
        int faceType = 0, normalDirection = 0;
        double axisPoint[3] = {}, axisDirection[3] = {}, bounds[6] = {};
        double radiusData = 0.0;
        if (UF_MODL_ask_face_data(inner->Tag(), &faceType, axisPoint,
                axisDirection, bounds, &record.innerRadius, &radiusData,
                &normalDirection) != 0 || faceType != UF_MODL_CYLINDRICAL_FACE ||
            !std::isfinite(record.innerRadius) || record.innerRadius <= 0.0)
            throw std::runtime_error("无法读取折弯内圆弧半径。");
        try { record.outerFace = manager->GetOppositeFace(inner); }
        catch (const NXOpen::NXException& ex)
        {
            AppendDebugLog("CollectBends NX opposite failed index=" +
                std::to_string(index) + " inner=" +
                std::to_string(inner->Tag()) + " code=" +
                std::to_string(ex.ErrorCode()));
            record.outerFace = nullptr;
        }
        if (record.outerFace == nullptr ||
            !IsAlive(record.outerFace->Tag()))
        {
            record.outerFace = FindOppositeBendFace(body, inner, thickness);
            AppendDebugLog("CollectBends geometric opposite index=" +
                std::to_string(index) + " inner=" +
                std::to_string(inner->Tag()) + " outer=" +
                std::to_string(record.outerFace == nullptr
                    ? 0 : record.outerFace->Tag()));
        }
        if (record.outerFace == nullptr)
        {
            AppendDebugLog("CollectBends skip index=" +
                std::to_string(index) + " inner=" +
                std::to_string(inner->Tag()) + " reason=no-opposite-face");
            continue;
        }
        if (!FindBoundaryEdges(inner, record.firstBoundary,
                               record.secondBoundary) &&
            !FindBoundaryEdges(record.outerFace, record.firstBoundary,
                               record.secondBoundary))
        {
            AppendDebugLog("CollectBends skip index=" +
                std::to_string(index) + " inner=" +
                std::to_string(inner->Tag()) + " outer=" +
                std::to_string(record.outerFace->Tag()) +
                " reason=no-two-axial-boundaries");
            continue;
        }
        AppendDebugLog("CollectBends accept index=" +
            std::to_string(index) + " inner=" +
            std::to_string(inner->Tag()) + " outer=" +
            std::to_string(record.outerFace->Tag()));
        result.push_back(record);
    }
    AppendDebugLog("CollectBends result body=" +
        std::to_string(body == nullptr ? 0 : body->Tag()) +
        " accepted=" + std::to_string(result.size()));
    return result;
}

BendMarkNotchDialog::FlatBend BendMarkNotchDialog::ResolveFlatBend(
    const BendRecord& bend) const
{
    NXOpen::Point3d a0, a1, b0, b1;
    if (!EdgeEnds(bend.firstBoundary, a0, a1) ||
        !EdgeEnds(bend.secondBoundary, b0, b1))
        throw std::runtime_error("伸直后折弯边已失效。");
    if (Distance(a0, b0) + Distance(a1, b1) >
        Distance(a0, b1) + Distance(a1, b0))
        std::swap(b0, b1);

    // Keep the center line midway between both flattened bend boundaries,
    // but use the complete length of the longer boundary.  Applying one
    // common transverse offset to the longer edge preserves its exact end
    // span instead of shortening the center line by averaging both ends.
    const double aLength = Distance(a0, a1);
    const double bLength = Distance(b0, b1);
    NXOpen::Point3d long0 = a0;
    NXOpen::Point3d long1 = a1;
    NXOpen::Point3d short0 = b0;
    NXOpen::Point3d short1 = b1;
    if (bLength > aLength)
    {
        long0 = b0;
        long1 = b1;
        short0 = a0;
        short1 = a1;
    }
    const NXOpen::Vector3d centerOffset = Scale(
        {short0.X - long0.X + short1.X - long1.X,
         short0.Y - long0.Y + short1.Y - long1.Y,
         short0.Z - long0.Z + short1.Z - long1.Z},
        0.25);
    FlatBend result;
    result.firstEnd = Add(long0, centerOffset);
    result.secondEnd = Add(long1, centerOffset);
    if (Distance(result.firstEnd, result.secondEnd) <= kTolerance)
        throw std::runtime_error("伸直后折弯中心线长度为零。");
    return result;
}

std::vector<BendMarkNotchDialog::FlatBend>
BendMarkNotchDialog::ResolveLargeBendMarks(
    const BendRecord& bend, int segments) const
{
    NXOpen::Point3d a0, a1, b0, b1;
    if (!EdgeEnds(bend.firstBoundary, a0, a1) ||
        !EdgeEnds(bend.secondBoundary, b0, b1))
        throw std::runtime_error("伸直后大圆弧相切边已失效。");
    if (Distance(a0, b0) + Distance(a1, b1) >
        Distance(a0, b1) + Distance(a1, b0))
        std::swap(b0, b1);

    // Use actual tangent-edge endpoints, without the longer-edge extension
    // used by ordinary bend center marks. Equal positions across the flattened
    // bend allowance correspond to equal arc angles on a cylindrical bend.
    std::vector<FlatBend> result;
    result.reserve(static_cast<std::size_t>(segments) + 1);
    for (int index = 0; index <= segments; ++index)
    {
        const double fraction = static_cast<double>(index) / segments;
        FlatBend mark;
        mark.firstEnd = Add(a0, Scale(Subtract(b0, a0), fraction));
        mark.secondEnd = Add(a1, Scale(Subtract(b1, a1), fraction));
        if (Distance(mark.firstEnd, mark.secondEnd) <= kTolerance)
            throw std::runtime_error("大圆弧标记线长度为零。");
        result.push_back(mark);
    }
    return result;
}

NXOpen::Vector3d BendMarkNotchDialog::ReferenceNormal(
    NXOpen::Face* referenceFace) const
{
    int type = 0, normalDirection = 0;
    double point[3] = {}, normal[3] = {}, box[6] = {};
    double radius = 0.0, radiusData = 0.0;
    if (referenceFace == nullptr || UF_MODL_ask_face_data(
            referenceFace->Tag(), &type, point, normal, box, &radius,
            &radiusData, &normalDirection) != 0 ||
        type != UF_MODL_PLANAR_FACE)
        throw std::runtime_error("无法读取展平体大平面方向。");
    NXOpen::Vector3d value = Normalize({normal[0], normal[1], normal[2]});
    return normalDirection < 0 ? Scale(value, -1.0) : value;
}

NXOpen::Face* BendMarkNotchDialog::FlatPatternUpwardFace(
    NXOpen::Features::Feature* feature) const
{
    auto* manager = session_->Parts()->Work()->Features()->SheetmetalManager();
    auto* builder = manager->CreateFlatPatternBuilder(feature);
    NXOpen::Face* upward = nullptr;
    try
    {
        if (builder != nullptr && builder->UpwardFace() != nullptr)
            upward = builder->UpwardFace()->Value();
    }
    catch (...)
    {
        if (builder != nullptr) builder->Destroy();
        throw;
    }
    if (builder != nullptr) builder->Destroy();
    if (upward == nullptr || !IsAlive(upward->Tag()))
        throw std::runtime_error("无法读取展平图样的朝上面，不能区分上下折。");
    return upward;
}

int BendMarkNotchDialog::ProcessBody(NXOpen::Body* body, NXOpen::Face* upwardFace)
{
    NXOpen::Part* part = session_->Parts()->Work();
    auto* manager = part->Features()->SheetmetalManager();
    AppendDebugLog("============================================================");
    AppendDebugLog("ProcessBody begin body=" +
        std::to_string(body == nullptr ? 0 : body->Tag()));
    const double thickness = manager->GetBodyThickness(body);
    const LargeBendSettings largeBendSettings =
        useCachedSettings_ ? cachedLarge_ : ReadLargeBendSettings();
    const bool separate =
        useCachedSettings_ ? cachedSeparate_ : ToggleValue(distinguishBends_);
    const NotchSettings upSettings = useCachedSettings_ ? cachedUp_ :
        ReadNotchSettings(separate ? upNotch_ : commonNotch_);
    const NotchSettings downSettings = useCachedSettings_ ? cachedDown_ :
        (separate ? ReadNotchSettings(downNotch_) : upSettings);
    NXOpen::Face* referenceFace = FindReferenceFace(body);
    const std::vector<BendRecord> bends = CollectBends(body, thickness);
    {
        std::ostringstream log;
        log.precision(16);
        log << "sheet-metal query thickness=" << thickness
            << " referenceFace="
            << (referenceFace == nullptr ? 0 : referenceFace->Tag())
            << " bendCount=" << bends.size();
        AppendDebugLog(log.str());
    }
    for (std::size_t index = 0; index < bends.size(); ++index)
    {
        const BendRecord& bend = bends[index];
        std::ostringstream log;
        log << "bend[" << index << "] innerFace="
            << (bend.innerFace == nullptr ? 0 : bend.innerFace->Tag())
            << " outerFace="
            << (bend.outerFace == nullptr ? 0 : bend.outerFace->Tag())
            << " firstBoundary="
            << (bend.firstBoundary == nullptr ? 0 : bend.firstBoundary->Tag())
            << " secondBoundary="
            << (bend.secondBoundary == nullptr ? 0 : bend.secondBoundary->Tag());
        AppendDebugLog(log.str());
    }
    if (referenceFace == nullptr || bends.empty())
    {
        AppendDebugLog("ProcessBody skipped: no valid reference web or bends");
        return 0;
    }

    const NXOpen::Session::UndoMarkId mark = session_->SetUndoMark(
        NXOpen::Session::MarkVisibilityInvisible, "折弯标记缺口内部处理");
    std::string failureStage = "prepare unbend faces";
    try
    {
        std::vector<NXOpen::Face*> bendFaces;
        for (const BendRecord& bend : bends)
        {
            bendFaces.push_back(bend.innerFace);
            bendFaces.push_back(bend.outerFace);
        }
        NXOpen::Features::Feature* unbendFeature = nullptr;
        if (reuseChain_ != nullptr)
        {
            failureStage = "reuse unbend";
            unbendFeature = ActivateReusedFeature(reuseChain_->front());
        }
        else
        {
            NXOpen::FaceDumbRule* rule =
                part->ScRuleFactory()->CreateRuleFaceDumb(bendFaces);
            NXOpen::ScCollector* collector = part->ScCollectors()->CreateCollector();
            collector->ReplaceRules(
                std::vector<NXOpen::SelectionIntentRule*>(1, rule), false);
            auto* unbend = manager->CreateUnbendFeatureBuilder(nullptr);
            unbend->SetReferenceEntity(referenceFace);
            unbend->SetFaceCollector(collector);
            failureStage = "commit unbend";
            AppendDebugLog("unbend commit begin referenceFace=" +
                std::to_string(referenceFace->Tag()) + " collectorFaces=" +
                std::to_string(bendFaces.size()));
            unbendFeature = unbend->CommitFeature();
            unbend->Destroy();
        }
        if (unbendFeature == nullptr)
            throw std::runtime_error("钣金体全部折弯一次伸直失败。");
        AppendDebugLog("unbend commit success feature=" +
            std::to_string(unbendFeature->Tag()));

        failureStage = "verify unbent body envelope thickness";
        const NXOpen::Vector3d normal = ReferenceNormal(referenceFace);
        const NXOpen::Vector3d helper = std::fabs(normal.X) < 0.9
            ? NXOpen::Vector3d{1.0, 0.0, 0.0}
            : NXOpen::Vector3d{0.0, 1.0, 0.0};
        const NXOpen::Vector3d secondary = Normalize(Cross(normal, helper));
        const NXOpen::Vector3d tertiary = Normalize(Cross(normal, secondary));
        double maximumProjection = -DBL_MAX;
        double minimumProjection = DBL_MAX;
        std::size_t measuredFaceCount = 0;
        for (NXOpen::Face* face : body->GetFaces())
        {
            if (face == nullptr || !IsAlive(face->Tag())) continue;
            double positiveDirection[3] = {
                normal.X, normal.Y, normal.Z};
            double negativeDirection[3] = {
                -normal.X, -normal.Y, -normal.Z};
            double secondDirection[3] = {
                secondary.X, secondary.Y, secondary.Z};
            double thirdDirection[3] = {
                tertiary.X, tertiary.Y, tertiary.Z};
            tag_t positiveSubentity = NULL_TAG;
            tag_t negativeSubentity = NULL_TAG;
            double positivePoint[3] = {};
            double negativePoint[3] = {};
            if (UF_MODL_ask_extreme(
                    face->Tag(), positiveDirection, secondDirection,
                    thirdDirection, &positiveSubentity, positivePoint) != 0 ||
                UF_MODL_ask_extreme(
                    face->Tag(), negativeDirection, secondDirection,
                    thirdDirection, &negativeSubentity, negativePoint) != 0)
                continue;
            maximumProjection = (std::max)(maximumProjection,
                positivePoint[0] * normal.X + positivePoint[1] * normal.Y +
                positivePoint[2] * normal.Z);
            minimumProjection = (std::min)(minimumProjection,
                negativePoint[0] * normal.X + negativePoint[1] * normal.Y +
                negativePoint[2] * normal.Z);
            ++measuredFaceCount;
        }
        if (measuredFaceCount == 0 || maximumProjection < minimumProjection)
            throw std::runtime_error("无法计算伸直实体的包容体厚度。");
        const double envelopeThickness =
            maximumProjection - minimumProjection;
        const double thicknessTolerance =
            (std::max)(1.0e-3, thickness * 1.0e-3);
        {
            std::ostringstream log;
            log.precision(16);
            log << "unbent envelope thickness body=" << body->Tag()
                << " measuredFaces=" << measuredFaceCount
                << " normal=" << VectorText(normal)
                << " minimumProjection=" << minimumProjection
                << " maximumProjection=" << maximumProjection
                << " envelopeThickness=" << envelopeThickness
                << " sheetThickness=" << thickness
                << " difference=" << std::fabs(envelopeThickness - thickness)
                << " tolerance=" << thicknessTolerance;
            AppendDebugLog(log.str());
        }
        if (std::fabs(envelopeThickness - thickness) > thicknessTolerance)
        {
            std::ostringstream message;
            message.precision(10);
            message << "伸直后包容体厚度 " << envelopeThickness
                    << " mm 与板厚 " << thickness
                    << " mm 不一致。";
            throw std::runtime_error(message.str());
        }

        failureStage = "resolve flattened bend endpoints";
        std::vector<FlatBend> flatBends;
        // Compare the actual skin planes after Unbend. Surface parameterization
        // (and its normal sense) is not a reliable indication of the original
        // bend direction. The original concave skin on the upward plane is an
        // up bend; the other skin must be exactly one thickness away.
        const NXOpen::Vector3d upwardNormal = upwardFace != nullptr
            ? ReferenceNormal(upwardFace) : normal;
        const NXOpen::Point3d upwardPoint = upwardFace != nullptr
            ? PlanarFacePoint(upwardFace) : NXOpen::Point3d{};
        NXOpen::Vector3d markingOutward = normal;
        if (upSettings.lineMark || downSettings.lineMark)
        {
            // Use the actual material envelope: a face's parameter normal can
            // have the opposite sense after Unbend even on the upward skin.
            const double skinProjection = upwardPoint.X * normal.X +
                upwardPoint.Y * normal.Y + upwardPoint.Z * normal.Z;
            const double toMinimum = std::fabs(skinProjection - minimumProjection);
            const double toMaximum = std::fabs(skinProjection - maximumProjection);
            if ((std::min)(toMinimum, toMaximum) > thicknessTolerance)
                throw std::runtime_error("无法确定标记线所在的展平表面。");
            markingOutward = toMinimum < toMaximum ? Scale(normal, -1.0) : normal;
            AppendDebugLog("marking line skin=" + PointText(upwardPoint) +
                " outward=" + VectorText(markingOutward));
        }
        const double sideTolerance = (std::max)(1.0e-6, thickness * 1.0e-4);
        for (const BendRecord& bend : bends)
        {
            FlatBend flat = ResolveFlatBend(bend);
            if (separate)
            {
                failureStage = "classify up/down bend";
                const double innerOffset = Dot(Subtract(
                    PlanarFacePoint(bend.innerFace), upwardPoint), upwardNormal);
                const double outerOffset = Dot(Subtract(
                    PlanarFacePoint(bend.outerFace), upwardPoint), upwardNormal);
                const bool innerOnUpward = std::fabs(innerOffset) <= sideTolerance;
                const bool outerOnUpward = std::fabs(outerOffset) <= sideTolerance;
                AppendDebugLog("direction planes innerOffset=" +
                    std::to_string(innerOffset) + " outerOffset=" +
                    std::to_string(outerOffset) + " thickness=" +
                    std::to_string(thickness));
                if (innerOnUpward == outerOnUpward ||
                    std::fabs(std::fabs(innerOffset - outerOffset) - thickness) >
                        sideTolerance)
                    throw std::runtime_error(
                        "伸直后的折弯面方向不明确，无法可靠区分上下折。");
                flat.upward = innerOnUpward;
                AppendDebugLog("bend direction innerFace=" +
                    std::to_string(bend.innerFace->Tag()) + " upwardFace=" +
                    std::to_string(upwardFace->Tag()) + " direction=" +
                    (flat.upward ? "up" : "down"));
            }
            const bool largeBend = bend.innerRadius / thickness >=
                largeBendSettings.radiusThicknessRatio;
            AppendDebugLog("bend marking radius=" +
                std::to_string(bend.innerRadius) + " thickness=" +
                std::to_string(thickness) + " ratio=" +
                std::to_string(bend.innerRadius / thickness) + " large=" +
                std::to_string(largeBend) + " segments=" +
                std::to_string(largeBendSettings.segments));
            if (!largeBend)
            {
                flatBends.push_back(flat);
                continue;
            }
            failureStage = "resolve large bend tangent and division marks";
            std::vector<FlatBend> marks = ResolveLargeBendMarks(
                bend, largeBendSettings.segments);
            const NXOpen::Vector3d axis = Normalize(
                Subtract(marks.front().secondEnd, marks.front().firstEnd));
            const double pitch = Length(Cross(axis, Subtract(
                marks.back().firstEnd, marks.front().firstEnd))) /
                largeBendSettings.segments;
            const NotchSettings& settings = flat.upward ? upSettings : downSettings;
            const double width = settings.type == 0 ? settings.diameter :
                (settings.type == 1 ? 2.0 * settings.depth *
                    std::tan(0.5 * settings.angle * kPi / 180.0) : settings.width);
            if (pitch <= width + kTolerance)
                throw std::runtime_error(
                    "大圆弧标记间距小于或等于缺口宽度，请减少均分段数或减小缺口尺寸。");
            for (std::size_t index = 0; index < marks.size(); ++index)
            {
                FlatBend& markLine = marks[index];
                markLine.upward = flat.upward;
                std::ostringstream log;
                log.precision(16);
                log << "large bend mark index=" << index
                    << " segments=" << largeBendSettings.segments
                    << " tangent=" << (index == 0 || index + 1 == marks.size())
                    << " pitch=" << pitch << " first=" << PointText(markLine.firstEnd)
                    << " second=" << PointText(markLine.secondEnd);
                AppendDebugLog(log.str());
                flatBends.push_back(markLine);
            }
        }

        // Collapse bend segments that lie on the same infinite line into one
        // overall span.  Only the two extreme endpoints of that span receive
        // notches; intermediate endpoints between collinear segments do not.
        struct CollinearSpan
        {
            NXOpen::Point3d origin;
            NXOpen::Vector3d direction;
            double minimum = 0.0;
            double maximum = 0.0;
            NXOpen::Point3d minimumPoint;
            NXOpen::Point3d maximumPoint;
            bool upward = true;
        };
        std::vector<CollinearSpan> spans;
        constexpr double kCollinearTolerance = 1.0e-3;
        for (const FlatBend& flat : flatBends)
        {
            const NXOpen::Vector3d direction = Normalize(
                Subtract(flat.secondEnd, flat.firstEnd));
            CollinearSpan* matched = nullptr;
            for (CollinearSpan& span : spans)
            {
                // Opposite directions may share a line but use different marks.
                if (separate && flat.upward != span.upward) continue;
                if (std::fabs(Dot(direction, span.direction)) < 0.999999)
                    continue;
                const auto distanceToLine = [&](const NXOpen::Point3d& point)
                {
                    const NXOpen::Vector3d offset =
                        Subtract(point, span.origin);
                    return Length(Subtract(
                        Add(span.origin, Scale(
                            span.direction, Dot(offset, span.direction))),
                        point));
                };
                if (distanceToLine(flat.firstEnd) <= kCollinearTolerance &&
                    distanceToLine(flat.secondEnd) <= kCollinearTolerance)
                {
                    matched = &span;
                    break;
                }
            }
            if (matched == nullptr)
            {
                CollinearSpan span;
                span.origin = flat.firstEnd;
                span.direction = direction;
                span.upward = flat.upward;
                span.minimum = 0.0;
                span.maximum = Dot(Subtract(flat.secondEnd, span.origin),
                                   span.direction);
                span.minimumPoint = flat.firstEnd;
                span.maximumPoint = flat.secondEnd;
                if (span.maximum < span.minimum)
                {
                    std::swap(span.minimum, span.maximum);
                    std::swap(span.minimumPoint, span.maximumPoint);
                }
                spans.push_back(span);
                continue;
            }
            const auto extendSpan = [&](const NXOpen::Point3d& point)
            {
                const double parameter = Dot(
                    Subtract(point, matched->origin), matched->direction);
                if (parameter < matched->minimum)
                {
                    matched->minimum = parameter;
                    matched->minimumPoint = point;
                }
                if (parameter > matched->maximum)
                {
                    matched->maximum = parameter;
                    matched->maximumPoint = point;
                }
            };
            extendSpan(flat.firstEnd);
            extendSpan(flat.secondEnd);
        }
        const std::size_t originalFlatBendCount = flatBends.size();
        flatBends.clear();
        flatBends.reserve(spans.size());
        for (const CollinearSpan& span : spans)
            flatBends.push_back(
                {span.minimumPoint, span.maximumPoint, span.upward});
        AppendDebugLog("collinear bend merge original=" +
            std::to_string(originalFlatBendCount) + " spans=" +
            std::to_string(flatBends.size()));
        AppendDebugLog("flat reference origin=" +
            PointText(flatBends.front().firstEnd) + " normal=" +
            VectorText(normal));
        const NXOpen::Point3d flatReference = flatBends.front().firstEnd;
        for (std::size_t index = 0; index < flatBends.size(); ++index)
        {
            const FlatBend& flat = flatBends[index];
            const double firstOffset = Dot(
                Subtract(flat.firstEnd, flatReference), normal);
            const double secondOffset = Dot(
                Subtract(flat.secondEnd, flatReference), normal);
            std::ostringstream log;
            log.precision(16);
            log << "flatBend[" << index << "] first="
                << PointText(flat.firstEnd) << " second="
                << PointText(flat.secondEnd) << " firstPlaneOffset="
                << firstOffset << " secondPlaneOffset=" << secondOffset;
            AppendDebugLog(log.str());
        }

        failureStage = "build notch profiles";
        std::vector<NotchProfile> profiles;
        std::vector<NotchProfile> lineProfiles;
        profiles.reserve(flatBends.size() * 2);
        for (const FlatBend& flat : flatBends)
        {
            const NotchSettings& settings = flat.upward ? upSettings : downSettings;
            const NXOpen::Vector3d lineDirection = Normalize(
                Subtract(flat.secondEnd, flat.firstEnd));
            AppendDebugLog(std::string("notch span direction=") +
                (separate ? (flat.upward ? "up" : "down") : "common") +
                " method=" + (settings.lineMark ? "line" : "notch") +
                " type=" + std::to_string(settings.type) +
                " diameter=" + std::to_string(settings.diameter) +
                " angle=" + std::to_string(settings.angle) +
                " width=" + std::to_string(settings.width) +
                " depth=" + std::to_string(settings.depth) +
                " edgeOffset=" + std::to_string(settings.edgeOffset) +
                " cutDepth=" + std::to_string(settings.lineMark ? kMarkingLineDepth : thickness));
            if (settings.lineMark)
            {
                if (thickness <= kMarkingLineDepth + kTolerance)
                    throw std::runtime_error("板厚必须大于标记线固定深度 0.001。");
                if (2.0 * (settings.edgeOffset + settings.depth) >=
                    Distance(flat.firstEnd, flat.secondEnd) - kTolerance)
                    throw std::runtime_error(
                        "标记线长度与离轮廓边距离过大，两端标记线会相接或重叠，请减小参数。");
            }
            if (settings.type == 0)
            {
                const double radius = settings.diameter * 0.5;
                NotchProfile first;
                first.center = flat.firstEnd;
                first.circleRadius = radius;
                profiles.push_back(first);
                NotchProfile second;
                second.center = flat.secondEnd;
                second.circleRadius = radius;
                profiles.push_back(second);
            }
            else
            {
                const double depth = settings.depth;
                const double halfWidth = settings.type == 1
                    ? depth * std::tan(0.5 * settings.angle * kPi / 180.0)
                    : settings.width * 0.5;
                const auto appendPolygon = [&](
                    const NXOpen::Point3d& edgeCenter,
                    const NXOpen::Vector3d& inwardInput)
                {
                    const NXOpen::Vector3d inward = Normalize(inwardInput);
                    const NXOpen::Vector3d baseDirection =
                        Normalize(Cross(normal, inward));
                    NXOpen::Point3d start = Add(
                        edgeCenter, Scale(inward, settings.edgeOffset));
                    if (settings.lineMark)
                    {
                        // All shallow marks start on the flat pattern's upward
                        // skin, including down bends whose inner skin is below it.
                        start = Add(start, Scale(upwardNormal,
                            -Dot(Subtract(start, upwardPoint), upwardNormal)));
                    }
                    NotchProfile profile;
                    profile.center = start;
                    profile.polygon = {
                        Add(start, Scale(baseDirection, halfWidth)),
                        Add(start, Scale(baseDirection, -halfWidth))};
                    const NXOpen::Point3d innerCenter =
                        Add(start, Scale(inward, depth));
                    if (settings.type == 1)
                        profile.polygon.push_back(innerCenter);
                    else
                    {
                        profile.polygon.push_back(
                            Add(innerCenter, Scale(baseDirection, -halfWidth)));
                        profile.polygon.push_back(
                            Add(innerCenter, Scale(baseDirection, halfWidth)));
                    }
                    (settings.lineMark ? lineProfiles : profiles).push_back(profile);
                };
                appendPolygon(flat.firstEnd, lineDirection);
                appendPolygon(flat.secondEnd, Scale(lineDirection, -1.0));
            }
        }
        AppendDebugLog("notch profiles built separate=" +
            std::to_string(separate) + " count=" +
            std::to_string(profiles.size()));
        AppendDebugLog("marking line profiles built count=" +
            std::to_string(lineProfiles.size()));
        const std::size_t profileCount = profiles.size() + lineProfiles.size();
        if (profileCount == 0) throw std::runtime_error("展平状态创建折弯标记失败。");
        std::vector<NXOpen::Features::Feature*> reuseTools;
        if (reuseChain_ != nullptr)
        {
            failureStage = "match reused tools";
            std::vector<bool> wanted;
            if (!profiles.empty()) wanted.push_back(false);
            if (!lineProfiles.empty()) wanted.push_back(true);
            std::vector<bool> existing;
            for (std::size_t index = 1; index + 2 < reuseChain_->size(); ++index)
            {
                auto* extrude = dynamic_cast<NXOpen::Features::Feature*>(
                    NXOpen::NXObjectManager::Get((*reuseChain_)[index]));
                if (extrude == nullptr)
                    throw std::runtime_error("折弯标记缺口的内部特征已丢失。");
                reuseTools.push_back(extrude);
                existing.push_back(IsMarkingLineTool(extrude));
            }
            AppendDebugLog("reuse tools existing=" + std::to_string(existing.size()) +
                " wanted=" + std::to_string(wanted.size()));
            if (existing != wanted)
                throw std::runtime_error(
                    "标记方式（缺口 / 标记线）的组合已改变，无法在原特征上修改。"
                    "请删除该折弯标记缺口后重新创建。");
        }
        std::vector<ToolRecord> tools;
        std::size_t reuseIndex = 0;
        const auto nextReuseTool = [&]() -> NXOpen::Features::Feature*
        {
            if (reuseChain_ == nullptr) return nullptr;
            NXOpen::Features::Feature* extrude = reuseTools[reuseIndex++];
            return ActivateReusedFeature(extrude->Tag());
        };
        if (!profiles.empty())
        {
            ToolRecord groupedTool;
            failureStage = "create combined notch extrusion tool";
            if (!CreateInternalSketchExtrudeTool(
                    normal, profiles, thickness, groupedTool, 0.0, nextReuseTool()))
                throw std::runtime_error("全部缺口合并草图拉伸失败。");
            tools.push_back(groupedTool);
        }
        if (!lineProfiles.empty())
        {
            ToolRecord lineTool;
            failureStage = "create shallow marking line tool";
            if (!CreateInternalSketchExtrudeTool(
                    markingOutward, lineProfiles, thickness, lineTool,
                    kMarkingLineDepth, nextReuseTool()))
                throw std::runtime_error("标记线浅槽拉伸失败。");
            tools.push_back(lineTool);
        }
        NXOpen::Features::Feature* booleanFeature = nullptr;
        failureStage = "subtract combined tool bodies";
        if (reuseChain_ != nullptr)
        {
            const tag_t booleanTag = (*reuseChain_)[reuseChain_->size() - 2];
            if (!RetargetSubtractTools(booleanTag, reuseTools))
                throw std::runtime_error("全部缺口工具体一次布尔减失败。");
            booleanFeature = ActivateReusedFeature(booleanTag);
        }
        else if (!SubtractToolsOnce(body, tools, booleanFeature, !lineProfiles.empty()) ||
            booleanFeature == nullptr)
            throw std::runtime_error("全部缺口工具体一次布尔减失败。");

        // Reuse exactly the same paired bend faces selected for Unbend.  Do
        // not re-query, remap, de-duplicate, or pre-validate them after the
        // Boolean; Rebend receives the original selection unchanged.
        AppendDebugLog("rebend selection source=original-unbend-pairs faceCount=" +
            std::to_string(bendFaces.size()));
        NXOpen::Features::Feature* rebendFeature = nullptr;
        if (reuseChain_ != nullptr)
        {
            failureStage = "reuse rebend";
            rebendFeature = ActivateReusedFeature(reuseChain_->back());
        }
        else
        {
            NXOpen::FaceDumbRule* rebendRule =
                part->ScRuleFactory()->CreateRuleFaceDumb(bendFaces);
            NXOpen::ScCollector* rebendCollector =
                part->ScCollectors()->CreateCollector();
            rebendCollector->ReplaceRules(
                std::vector<NXOpen::SelectionIntentRule*>(1, rebendRule), false);
            auto* rebend = manager->CreateRebendFeatureBuilder(nullptr);
            rebend->SetFaceCollector(rebendCollector);
            failureStage = "commit rebend";
            rebendFeature = rebend->CommitFeature();
            rebend->Destroy();
        }
        if (rebendFeature == nullptr)
            throw std::runtime_error("创建缺口后重新折弯失败。");
        failureStage = "update model after rebend";
        const int errors = session_->UpdateManager()->DoUpdate(mark);
        if (errors != 0) throw std::runtime_error("折弯标记缺口模型更新失败。");

        pendingInternalFeatureTags_.push_back(unbendFeature->Tag());
        for (const ToolRecord& tool : tools)
            pendingInternalFeatureTags_.push_back(tool.featureTag);
        pendingInternalFeatureTags_.push_back(booleanFeature->Tag());
        pendingInternalFeatureTags_.push_back(rebendFeature->Tag());
        session_->DeleteUndoMark(mark, "折弯标记缺口内部处理");
        AppendDebugLog("ProcessBody success profiles=" +
            std::to_string(profileCount) + " rebendFeature=" +
            std::to_string(rebendFeature->Tag()));
        return static_cast<int>(profileCount);
    }
    catch (const NXOpen::NXException& ex)
    {
        AppendDebugLog("ProcessBody NXException stage=" + failureStage +
            " code=" + std::to_string(ex.ErrorCode()) + " message=" +
            ex.Message());
        LogUpdateUndoFeature();
        session_->UndoToMark(mark, "折弯标记缺口失败回退");
        throw;
    }
    catch (const std::exception& ex)
    {
        AppendDebugLog("ProcessBody std::exception stage=" + failureStage +
            " message=" + ex.what());
        session_->UndoToMark(mark, "折弯标记缺口失败回退");
        throw;
    }
    catch (...)
    {
        AppendDebugLog("ProcessBody unknown exception stage=" + failureStage);
        session_->UndoToMark(mark, "折弯标记缺口失败回退");
        throw;
    }
}

NXOpen::Features::Feature* BendMarkNotchDialog::ActivateReusedFeature(tag_t tag) const
{
    auto* feature = IsAlive(tag)
        ? dynamic_cast<NXOpen::Features::Feature*>(NXOpen::NXObjectManager::Get(tag))
        : nullptr;
    if (feature == nullptr)
        throw std::runtime_error("折弯标记缺口的内部特征已丢失。");
    feature->MakeCurrentFeature();
    AppendDebugLog("reuse activate feature=" + std::to_string(tag));
    return feature;
}

bool BendMarkNotchDialog::IsMarkingLineTool(NXOpen::Features::Feature* extrude) const
{
    NXOpen::Part* part = session_->Parts()->Work();
    NXOpen::Features::ExtrudeBuilder* builder =
        part->Features()->CreateExtrudeBuilder(extrude);
    const double start = builder->Limits()->StartExtend()->Value()->Value();
    builder->Destroy();
    return std::fabs(start + kMarkingLineDepth) < 1.0e-9;
}

bool BendMarkNotchDialog::RetargetSubtractTools(
    tag_t booleanTag, const std::vector<NXOpen::Features::Feature*>& extrudes) const
{
    auto* feature = IsAlive(booleanTag)
        ? dynamic_cast<NXOpen::Features::BooleanFeature*>(
              NXOpen::NXObjectManager::Get(booleanTag))
        : nullptr;
    if (feature == nullptr || extrudes.empty()) return false;
    NXOpen::Part* part = session_->Parts()->Work();
    NXOpen::Features::BooleanBuilder* builder = nullptr;
    try
    {
        // Feature-body intent keeps every tool body when an edit changes
        // how many notches the extrusions produce.
        builder = part->Features()->CreateBooleanBuilderUsingCollector(feature);
        NXOpen::ScCollector* toolCollector = part->ScCollectors()->CreateCollector();
        NXOpen::BodyFeatureRule* toolRule =
            part->ScRuleFactory()->CreateRuleBodyFeature(extrudes);
        toolCollector->ReplaceRules(
            std::vector<NXOpen::SelectionIntentRule*>(1, toolRule), false);
        builder->SetToolBodyCollector(toolCollector);
        builder->CommitFeature();
        builder->Destroy();
        AppendDebugLog("reuse subtract retargeted feature=" + std::to_string(booleanTag) +
            " extrudes=" + std::to_string(extrudes.size()));
        return true;
    }
    catch (const NXOpen::NXException& ex)
    {
        AppendDebugLog("reuse subtract retarget failed code=" +
            std::to_string(ex.ErrorCode()) + " message=" + ex.Message());
        if (builder != nullptr) builder->Destroy();
        return false;
    }
}

bool BendMarkNotchDialog::SubtractToolsOnce(
    NXOpen::Body* body, const std::vector<ToolRecord>& tools,
    NXOpen::Features::Feature*& booleanFeature, bool hasMarkingLines) const
{
    booleanFeature = nullptr;
    if (body == nullptr || tools.empty())
    {
        AppendDebugLog("SubtractToolsOnce rejected empty body/tools");
        return false;
    }
    NXOpen::Part* part = session_->Parts()->Work();
    std::vector<NXOpen::Body*> toolBodies;
    for (const ToolRecord& tool : tools)
    {
        for (tag_t bodyTag : tool.bodyTags)
        {
            if (!IsAlive(bodyTag)) return false;
            NXOpen::Body* toolBody = dynamic_cast<NXOpen::Body*>(
                NXOpen::NXObjectManager::Get(bodyTag));
            if (toolBody == nullptr) return false;
            toolBodies.push_back(toolBody);
        }
    }
    if (toolBodies.empty())
    {
        AppendDebugLog("SubtractToolsOnce found no alive tool bodies");
        return false;
    }
    AppendDebugLog("SubtractToolsOnce begin target=" +
        std::to_string(body->Tag()) + " toolBodies=" +
        std::to_string(toolBodies.size()));

    auto* booleanBuilder =
        part->Features()->CreateBooleanBuilderUsingCollector(nullptr);
    try
    {
        booleanBuilder->SetOperation(
            NXOpen::Features::Feature::BooleanTypeSubtract);
        booleanBuilder->SetCopyTargets(false);
        booleanBuilder->SetCopyTools(false);
        if (hasMarkingLines)
            booleanBuilder->SetTolerance(1.0e-5);

        NXOpen::ScCollector* targetCollector =
            part->ScCollectors()->CreateCollector();
        NXOpen::BodyDumbRule* targetRule =
            part->ScRuleFactory()->CreateRuleBodyDumb(
                std::vector<NXOpen::Body*>(1, body));
        targetCollector->ReplaceRules(
            std::vector<NXOpen::SelectionIntentRule*>(1, targetRule), false);
        booleanBuilder->SetTargetBodyCollector(targetCollector);

        NXOpen::ScCollector* toolCollector =
            part->ScCollectors()->CreateCollector();
        NXOpen::BodyDumbRule* toolRule =
            part->ScRuleFactory()->CreateRuleBodyDumb(toolBodies);
        toolCollector->ReplaceRules(
            std::vector<NXOpen::SelectionIntentRule*>(1, toolRule), false);
        booleanBuilder->SetToolBodyCollector(toolCollector);

        booleanFeature = booleanBuilder->CommitFeature();
        booleanBuilder->Destroy();
        AppendDebugLog("SubtractToolsOnce commit result=" +
            std::to_string(booleanFeature == nullptr ? 0 : booleanFeature->Tag()));
        return booleanFeature != nullptr;
    }
    catch (const NXOpen::NXException& ex)
    {
        AppendDebugLog("SubtractToolsOnce NXException code=" +
            std::to_string(ex.ErrorCode()) + " message=" + ex.Message());
        booleanBuilder->Destroy();
        return false;
    }
    catch (const std::exception& ex)
    {
        AppendDebugLog("SubtractToolsOnce std::exception message=" +
            std::string(ex.what()));
        booleanBuilder->Destroy();
        return false;
    }
    catch (...)
    {
        AppendDebugLog("SubtractToolsOnce unknown exception");
        booleanBuilder->Destroy();
        return false;
    }
}

bool BendMarkNotchDialog::CreateInternalSketchExtrudeTool(
    const NXOpen::Vector3d& normalInput,
    const std::vector<NotchProfile>& profiles,
    double thickness, ToolRecord& tool, double cutDepth,
    NXOpen::Features::Feature* reuseExtrude) const
{
    tool = ToolRecord();
    NXOpen::Part* part = session_->Parts()->Work();
    if (part == nullptr || profiles.empty()) return false;
    for (const NotchProfile& profile : profiles)
        if (profile.circleRadius <= kTolerance && profile.polygon.size() < 3)
            return false;

    NXOpen::Sketch* reuseSketch = nullptr;
    if (reuseExtrude != nullptr)
    {
        for (NXOpen::Features::Feature* parent : reuseExtrude->GetParents())
        {
            auto* sketchParent = dynamic_cast<NXOpen::Features::SketchFeature*>(parent);
            if (sketchParent != nullptr)
            {
                reuseSketch = sketchParent->Sketch();
                break;
            }
        }
        AppendDebugLog("reuse extrude=" + std::to_string(reuseExtrude->Tag()) +
            " sketch=" + std::to_string(reuseSketch == nullptr ? 0 : reuseSketch->Tag()));
        if (reuseSketch == nullptr) return false;
    }

    NXOpen::Vector3d normal = Normalize(normalInput);
    NXOpen::Point3d origin = profiles.front().center;
    if (reuseSketch != nullptr)
    {
        // Profiles are projected onto the existing sketch plane.
        const NXOpen::Matrix3x3 orientation = reuseSketch->Orientation()->Element();
        origin = reuseSketch->Origin();
        normal = Normalize({orientation.Zx, orientation.Zy, orientation.Zz});
    }
    const NXOpen::Vector3d reference =
        std::fabs(normal.Z) < 0.9 ? NXOpen::Vector3d{0.0, 0.0, 1.0}
                                  : NXOpen::Vector3d{1.0, 0.0, 0.0};
    const NXOpen::Vector3d xAxis = Normalize(Cross(reference, normal));
    const NXOpen::Vector3d yAxis = Normalize(Cross(normal, xAxis));
    const auto projectToSketchPlane = [&](const NXOpen::Point3d& point)
    {
        const double offset = Dot(Subtract(point, origin), normal);
        return Add(point, Scale(normal, -offset));
    };
    {
        std::ostringstream log;
        log.precision(16);
        log << "CreateInternalSketchExtrudeTool begin profiles="
            << profiles.size() << " thickness=" << thickness
            << " cutDepth=" << cutDepth
            << " origin=" << PointText(origin)
            << " normal=" << VectorText(normal)
            << " xAxis=" << VectorText(xAxis)
            << " yAxis=" << VectorText(yAxis);
        AppendDebugLog(log.str());
    }
    for (std::size_t index = 0; index < profiles.size(); ++index)
    {
        const NotchProfile& profile = profiles[index];
        const double centerOffset = Dot(Subtract(profile.center, origin), normal);
        std::ostringstream log;
        log.precision(16);
        log << "profile[" << index << "] center="
            << PointText(profile.center) << " planeOffset=" << centerOffset
            << " circleRadius=" << profile.circleRadius
            << " polygonPoints=" << profile.polygon.size();
        for (std::size_t pointIndex = 0;
             pointIndex < profile.polygon.size(); ++pointIndex)
        {
            log << " p" << pointIndex << '='
                << PointText(profile.polygon[pointIndex])
                << " p" << pointIndex << "Offset="
                << Dot(Subtract(profile.polygon[pointIndex], origin), normal);
        }
        AppendDebugLog(log.str());
    }

    NXOpen::SketchInPlaceBuilder* sketchBuilder = nullptr;
    NXOpen::Features::ExtrudeBuilder* extrudeBuilder = nullptr;
    NXOpen::Sketch* sketch = nullptr;
    NXOpen::Features::Feature* sketchFeature = nullptr;
    NXOpen::Features::Feature* extrudeFeature = nullptr;
    tag_t planeTag = NULL_TAG;
    tag_t pointTag = NULL_TAG;
    tag_t axisTag = NULL_TAG;
    tag_t extrusionDirectionTag = NULL_TAG;
    std::vector<tag_t> curveTags;
    std::string failureStage = "initialization";
    const auto cleanup = [&]()
    {
        if (extrudeBuilder != nullptr) extrudeBuilder->Destroy();
        if (sketchBuilder != nullptr) sketchBuilder->Destroy();
        try
        {
            if (sketch != nullptr && sketch->IsActive())
                sketch->Deactivate(
                    NXOpen::Sketch::ViewReorientFalse,
                    NXOpen::Sketch::UpdateLevelModel);
        }
        catch (...) {}
        if (extrudeFeature != nullptr) DeleteObject(extrudeFeature->Tag());
        if (sketchFeature != nullptr) DeleteObject(sketchFeature->Tag());
        for (tag_t curve : curveTags) DeleteObject(curve);
        DeleteObject(extrusionDirectionTag);
        DeleteObject(pointTag);
        DeleteObject(axisTag);
        DeleteObject(planeTag);
    };
    try
    {
      if (reuseSketch != nullptr)
      {
        sketch = reuseSketch;
      }
      else
      {
        failureStage = "create sketch plane, axis and origin";
        NXOpen::Plane* plane = part->Planes()->CreatePlane(
            origin, normal, NXOpen::SmartObject::UpdateOptionWithinModeling);
        NXOpen::Direction* axis = part->Directions()->CreateDirection(
            origin, xAxis, NXOpen::SmartObject::UpdateOptionWithinModeling);
        NXOpen::Point* sketchOrigin = part->Points()->CreatePoint(origin);
        if (plane == nullptr || axis == nullptr || sketchOrigin == nullptr)
            throw std::runtime_error("创建缺口草图基准失败。");
        planeTag = plane->Tag();
        axisTag = axis->Tag();
        pointTag = sketchOrigin->Tag();
        plane->SetVisibility(NXOpen::SmartObject::VisibilityOptionInvisible);
        axis->SetVisibility(NXOpen::SmartObject::VisibilityOptionInvisible);
        sketchOrigin->SetVisibility(NXOpen::SmartObject::VisibilityOptionInvisible);

        failureStage = "commit sketch";
        sketchBuilder = part->Sketches()->CreateSketchInPlaceBuilder2(nullptr);
        sketchBuilder->SetPlaneOption(NXOpen::Sketch::PlaneOptionExistingPlane);
        sketchBuilder->SetPlaneReference(plane);
        sketchBuilder->SetAxisReference(axis);
        sketchBuilder->SetOriginOption(NXOpen::OriginMethodSpecifyPoint);
        sketchBuilder->SetSketchOrigin(sketchOrigin);
        NXOpen::NXObject* committedSketch = sketchBuilder->Commit();
        const std::vector<NXOpen::NXObject*> committedObjects =
            sketchBuilder->GetCommittedObjects();
        sketchBuilder->Destroy();
        sketchBuilder = nullptr;

        sketch = dynamic_cast<NXOpen::Sketch*>(committedSketch);
        auto* directSketchFeature =
            dynamic_cast<NXOpen::Features::SketchFeature*>(committedSketch);
        if (directSketchFeature != nullptr)
        {
            sketchFeature = directSketchFeature;
            sketch = directSketchFeature->Sketch();
        }
        for (NXOpen::NXObject* object : committedObjects)
        {
            if (sketch == nullptr) sketch = dynamic_cast<NXOpen::Sketch*>(object);
            auto* feature = dynamic_cast<NXOpen::Features::SketchFeature*>(object);
            if (feature != nullptr)
            {
                sketchFeature = feature;
                sketch = feature->Sketch();
            }
        }
        if (sketch == nullptr)
            throw std::runtime_error("创建缺口草图失败。");
        if (sketchFeature == nullptr) sketchFeature = sketch->Feature();
        if (sketchFeature == nullptr)
            throw std::runtime_error("取得缺口草图特征失败。");
        AppendDebugLog("sketch commit success sketch=" +
            std::to_string(sketch->Tag()) + " feature=" +
            std::to_string(sketchFeature->Tag()));
      }

        failureStage = "activate sketch";
        sketch->Activate(NXOpen::Sketch::ViewReorientFalse);
        if (reuseSketch != nullptr)
        {
            failureStage = "delete previous sketch geometry";
            std::vector<NXOpen::TaggedObject*> previous;
            for (NXOpen::NXObject* geometry : sketch->GetAllGeometry())
                if (geometry != nullptr) previous.push_back(geometry);
            if (!previous.empty())
            {
                const NXOpen::Session::UndoMarkId deleteMark = session_->SetUndoMark(
                    NXOpen::Session::MarkVisibilityInvisible, "折弯标记缺口草图清除");
                NXOpen::Update* update = session_->UpdateManager();
                update->ClearDeleteList();
                update->ClearErrorList();
                update->AddObjectsToDeleteList(previous);
                const int errors = update->DoUpdate(deleteMark);
                AppendDebugLog("reuse sketch cleared geometry=" +
                    std::to_string(previous.size()) +
                    " errors=" + std::to_string(errors));
            }
        }
        for (std::size_t profileIndex = 0;
             profileIndex < profiles.size(); ++profileIndex)
        {
            const NotchProfile& profile = profiles[profileIndex];
            if (profile.circleRadius > kTolerance)
            {
                failureStage = "create/add circle profile " +
                    std::to_string(profileIndex);
                const NXOpen::Point3d projectedCenter =
                    projectToSketchPlane(profile.center);
                AppendDebugLog("project circle profile[" +
                    std::to_string(profileIndex) + "] source=" +
                    PointText(profile.center) + " projected=" +
                    PointText(projectedCenter));
                NXOpen::Arc* circle = part->Curves()->CreateArc(
                    projectedCenter, xAxis, yAxis, profile.circleRadius,
                    0.0, 2.0 * kPi);
                if (circle == nullptr)
                    throw std::runtime_error("创建圆形缺口草图失败。");
                curveTags.push_back(circle->Tag());
                sketch->AddGeometry(
                    circle,
                    NXOpen::Sketch::InferConstraintsOptionInferNoConstraints);
            }
            else
            {
                for (std::size_t index = 0;
                     index < profile.polygon.size(); ++index)
                {
                    failureStage = "create/add polygon profile " +
                        std::to_string(profileIndex) + " edge " +
                        std::to_string(index);
                    const NXOpen::Point3d projectedStart =
                        projectToSketchPlane(profile.polygon[index]);
                    const NXOpen::Point3d projectedEnd =
                        projectToSketchPlane(profile.polygon[
                            (index + 1) % profile.polygon.size()]);
                    {
                        std::ostringstream log;
                        log.precision(16);
                        log << "project polygon profile[" << profileIndex
                            << "] edge=" << index << " sourceStart="
                            << PointText(profile.polygon[index])
                            << " sourceEnd="
                            << PointText(profile.polygon[
                                (index + 1) % profile.polygon.size()])
                            << " projectedStart=" << PointText(projectedStart)
                            << " projectedEnd=" << PointText(projectedEnd)
                            << " projectedStartOffset="
                            << Dot(Subtract(projectedStart, origin), normal)
                            << " projectedEndOffset="
                            << Dot(Subtract(projectedEnd, origin), normal);
                        AppendDebugLog(log.str());
                    }
                    NXOpen::Line* line = part->Curves()->CreateLine(
                        projectedStart, projectedEnd);
                    if (line == nullptr)
                        throw std::runtime_error("创建缺口轮廓线失败。");
                    curveTags.push_back(line->Tag());
                    sketch->AddGeometry(
                        line,
                        NXOpen::Sketch::InferConstraintsOptionInferNoConstraints);
                }
            }
        }
        AppendDebugLog("sketch geometry added curveTags=" +
            std::to_string(curveTags.size()));
        failureStage = "update sketch";
        sketch->Update();
        sketch->UpdateNavigator();
        failureStage = "deactivate sketch";
        sketch->Deactivate(
            NXOpen::Sketch::ViewReorientFalse, NXOpen::Sketch::UpdateLevelModel);

        failureStage = "read sketch geometry";
        std::vector<NXOpen::Curve*> profileCurves;
        for (NXOpen::NXObject* geometry : sketch->GetAllGeometry())
        {
            auto* curve = dynamic_cast<NXOpen::Curve*>(geometry);
            if (curve != nullptr) profileCurves.push_back(curve);
        }
        if (profileCurves.empty())
            throw std::runtime_error("缺口草图中没有有效轮廓。");
        AppendDebugLog("sketch geometry query count=" +
            std::to_string(profileCurves.size()) + " firstCurve=" +
            std::to_string(profileCurves.front()->Tag()));
        if (reuseExtrude != nullptr)
        {
            failureStage = "collect reused extrusion tool bodies";
            for (NXOpen::Body* toolBody : reuseExtrude->GetBodies())
                if (toolBody != nullptr && IsAlive(toolBody->Tag()))
                    tool.bodyTags.push_back(toolBody->Tag());
            if (tool.bodyTags.empty())
                throw std::runtime_error("取得合并缺口拉伸工具体失败。");
            tool.featureTag = reuseExtrude->Tag();
            AppendDebugLog("reuse extrude updated feature=" +
                std::to_string(tool.featureTag) + " toolBodies=" +
                std::to_string(tool.bodyTags.size()));
            return true;
        }

        failureStage = "configure extrude builder";
        extrudeBuilder = part->Features()->CreateExtrudeBuilder(nullptr);
        NXOpen::Section* section =
            part->Sections()->CreateSection(9.5e-05, 0.0001, 0.5);
        extrudeBuilder->SetSection(section);
        extrudeBuilder->AllowSelfIntersectingSection(true);
        extrudeBuilder->SetDistanceTolerance(0.0001);
        extrudeBuilder->BooleanOperation()->SetType(
            NXOpen::GeometricUtilities::BooleanOperation::BooleanTypeCreate);
        extrudeBuilder->SmartVolumeProfile()->SetOpenProfileSmartVolumeOption(false);
        extrudeBuilder->SmartVolumeProfile()->SetCloseProfileRule(
            NXOpen::GeometricUtilities::SmartVolumeProfileBuilder::CloseProfileRuleTypeFci);
        const double margin = (std::max)(0.5, thickness);
        extrudeBuilder->Limits()->SetSymmetricOption(false);
        extrudeBuilder->Limits()->StartExtend()->Value()->SetFormula(
            NumberBuffer(cutDepth > 0.0 ? -cutDepth : -(thickness + margin)).data());
        extrudeBuilder->Limits()->EndExtend()->Value()->SetFormula(
            NumberBuffer(cutDepth > 0.0 ? margin : thickness + margin).data());
        extrudeBuilder->Limits()->StartExtend()->SetTrimType(
            NXOpen::GeometricUtilities::Extend::ExtendTypeValue);
        extrudeBuilder->Limits()->EndExtend()->SetTrimType(
            NXOpen::GeometricUtilities::Extend::ExtendTypeValue);
        extrudeBuilder->Offset()->SetOption(NXOpen::GeometricUtilities::TypeNoOffset);
        extrudeBuilder->Offset()->StartOffset()->SetFormula("0");
        extrudeBuilder->Offset()->EndOffset()->SetFormula("0");
        extrudeBuilder->FeatureOptions()->SetBodyType(
            NXOpen::GeometricUtilities::FeatureOptions::BodyStyleSolid);

        section->SetDistanceTolerance(0.0001);
        section->SetChainingTolerance(9.5e-05);
        section->SetAllowedEntityTypes(NXOpen::Section::AllowTypesOnlyCurves);
        section->AllowSelfIntersection(true);
        section->AllowDegenerateCurves(false);
        NXOpen::CurveFeatureRule* profileRule =
            part->ScRuleFactory()->CreateRuleCurveFeature(
                std::vector<NXOpen::Features::Feature*>(1, sketchFeature));
        failureStage = "add sketch rule to extrude section";
        section->AddToSection(
            std::vector<NXOpen::SelectionIntentRule*>(1, profileRule),
            profileCurves.front(), nullptr, nullptr, origin,
            NXOpen::Section::ModeCreate, false);
        AppendDebugLog("extrude section added sketchFeature=" +
            std::to_string(sketchFeature->Tag()) + " seedCurve=" +
            std::to_string(profileCurves.front()->Tag()));

        failureStage = "create extrusion direction";
        NXOpen::Direction* extrusionDirection = part->Directions()->CreateDirection(
            origin, normal, NXOpen::SmartObject::UpdateOptionWithinModeling);
        if (extrusionDirection == nullptr)
            throw std::runtime_error("创建缺口拉伸方向失败。");
        extrusionDirectionTag = extrusionDirection->Tag();
        extrusionDirection->SetVisibility(
            NXOpen::SmartObject::VisibilityOptionInvisible);
        extrudeBuilder->SetDirection(extrusionDirection);
        extrudeBuilder->SetParentFeatureInternal(sketchFeature);
        failureStage = "commit extrusion";
        AppendDebugLog("extrude commit begin direction=" +
            std::to_string(extrusionDirectionTag));
        extrudeFeature = extrudeBuilder->CommitFeature();
        extrudeBuilder->Destroy();
        extrudeBuilder = nullptr;
        if (extrudeFeature == nullptr)
            throw std::runtime_error("创建缺口拉伸体失败。");
        AppendDebugLog("extrude commit success feature=" +
            std::to_string(extrudeFeature->Tag()));
        failureStage = "make sketch internal";
        if (!sketchFeature->IsInternal()) extrudeFeature->MakeSketchInternal();
        if (!sketchFeature->IsInternal())
            throw std::runtime_error("缺口草图未能放入拉伸特征内部。");

        failureStage = "collect extrusion tool bodies";
        const std::vector<NXOpen::Body*> toolBodies = extrudeFeature->GetBodies();
        for (NXOpen::Body* toolBody : toolBodies)
            if (toolBody != nullptr && IsAlive(toolBody->Tag()))
                tool.bodyTags.push_back(toolBody->Tag());
        if (tool.bodyTags.empty())
            throw std::runtime_error("取得合并缺口拉伸工具体失败。");
        tool.featureTag = extrudeFeature->Tag();
        AppendDebugLog("CreateInternalSketchExtrudeTool success feature=" +
            std::to_string(tool.featureTag) + " toolBodies=" +
            std::to_string(tool.bodyTags.size()));
        return true;
    }
    catch (const NXOpen::NXException& ex)
    {
        AppendDebugLog("CreateInternalSketchExtrudeTool NXException stage=" +
            failureStage + " code=" + std::to_string(ex.ErrorCode()) +
            " message=" + ex.Message());
        cleanup();
        return false;
    }
    catch (const std::exception& ex)
    {
        AppendDebugLog("CreateInternalSketchExtrudeTool std::exception stage=" +
            failureStage + " message=" + ex.what());
        cleanup();
        return false;
    }
    catch (...)
    {
        AppendDebugLog("CreateInternalSketchExtrudeTool unknown exception stage=" +
            failureStage);
        cleanup();
        return false;
    }
}
