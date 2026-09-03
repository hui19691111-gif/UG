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
#include <NXOpen/Features_CustomAttribute.hxx>
#include <NXOpen/Features_CustomAttributeCollection.hxx>
#include <NXOpen/Features_CustomFeatureBuilder.hxx>
#include <NXOpen/Features_CustomFeatureClass.hxx>
#include <NXOpen/Features_CustomFeatureClassManager.hxx>
#include <NXOpen/Features_CustomFeatureData.hxx>
#include <NXOpen/Features_CustomFeatureDataCollection.hxx>
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
#include <uf_obj.h>
#include <uf_smd.h>
#include <uf_ui_types.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <set>
#include <sstream>
#include <stdexcept>

namespace
{
constexpr double kTolerance = 1.0e-6;
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
}

BendMarkNotchDialog::BendMarkNotchDialog()
    : ui_(NXOpen::UI::GetUI()), session_(NXOpen::Session::GetSession()),
      dialog_(ui_->CreateDialog(DialogPath().c_str())), autoSelect_(nullptr),
      bodySelect_(nullptr), notchType_(nullptr), diameter_(nullptr),
      angle_(nullptr), depth_(nullptr), status_(nullptr), initialized_(false),
      updating_(false)
{
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
    updating_ = false;

    delete dialog_;
    dialog_ = nullptr;
    autoSelect_ = nullptr;
    bodySelect_ = nullptr;
    notchType_ = nullptr;
    diameter_ = nullptr;
    angle_ = nullptr;
    depth_ = nullptr;
    status_ = nullptr;
    ui_ = nullptr;
    session_ = nullptr;
}

NXOpen::BlockStyler::BlockDialog::DialogResponse
BendMarkNotchDialog::Launch()
{
    return dialog_->Launch();
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
        notchType_ = dialog_->TopBlock()->FindBlock("notch_type");
        diameter_ = dialog_->TopBlock()->FindBlock("diameter");
        angle_ = dialog_->TopBlock()->FindBlock("angle");
        depth_ = dialog_->TopBlock()->FindBlock("depth");
        status_ = dialog_->TopBlock()->FindBlock("result_status");
        if (autoSelect_ == nullptr || bodySelect_ == nullptr ||
            notchType_ == nullptr || diameter_ == nullptr || angle_ == nullptr ||
            depth_ == nullptr || status_ == nullptr)
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
        UpdateControlState();
        if (!ToggleValue(autoSelect_) && bodySelect_ != nullptr)
            bodySelect_->Focus();
    }
    catch (const NXOpen::NXException& ex) { ShowError(ex.Message()); }
    catch (const std::exception& ex) { ShowError(ex.what()); }
}

int BendMarkNotchDialog::update_cb(NXOpen::BlockStyler::UIBlock* block)
{
    if (!initialized_ || updating_) return 0;
    try
    {
        if (block == autoSelect_ || block == notchType_)
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
    try { return Execute(); }
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
    if (!initialized_) return;
    updating_ = true;
    try
    {
        const bool automatic = ToggleValue(autoSelect_);
        const bool round = NotchType() == 0;
        auto setShow = [](NXOpen::BlockStyler::UIBlock* block, bool show)
        {
            NXOpen::BlockStyler::PropertyList* properties = block->GetProperties();
            properties->SetLogical("Show", show);
            properties->SetLogical("Enable", show);
            delete properties;
        };
        setShow(bodySelect_, !automatic);
        setShow(diameter_, round);
        setShow(angle_, !round);
        setShow(depth_, !round);
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
    NXOpen::BlockStyler::PropertyList* properties = block->GetProperties();
    const bool value = properties->GetLogical("Value");
    delete properties;
    return value;
}

int BendMarkNotchDialog::NotchType() const
{
    NXOpen::BlockStyler::PropertyList* properties = notchType_->GetProperties();
    const int value = properties->GetEnum("Value");
    delete properties;
    return value;
}

double BendMarkNotchDialog::DoubleValue(
    NXOpen::BlockStyler::UIBlock* block) const
{
    NXOpen::BlockStyler::PropertyList* properties = block->GetProperties();
    const double value = properties->GetDouble("Value");
    delete properties;
    return value;
}

void BendMarkNotchDialog::SetStatus(const std::string& text) const
{
    if (status_ == nullptr) return;
    NXOpen::BlockStyler::PropertyList* properties = status_->GetProperties();
    properties->SetString(NXOpen::NXString("Label", NXOpen::NXString::UTF8),
                          NXOpen::NXString(text, NXOpen::NXString::UTF8));
    delete properties;
}

void BendMarkNotchDialog::ShowError(const std::string& text) const
{
    ui_->NXMessageBox()->Show("折弯标记缺口",
                              NXOpen::NXMessageBox::DialogTypeError,
                              text.c_str());
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
    const int type = NotchType();
    const double diameter = DoubleValue(diameter_);
    const double angle = DoubleValue(angle_);
    const double depth = DoubleValue(depth_);
    if (type == 0 && diameter <= kTolerance)
        throw std::runtime_error("半圆缺口直径必须大于 0。");
    if (type == 1 && (angle <= 0.0 || angle >= 179.0 || depth <= kTolerance))
        throw std::runtime_error("锐角缺口角度须在 0～179°之间，深度必须大于 0。");

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
    if (bodies.empty()) throw std::runtime_error("没有可处理的钣金体。");

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
            if (workPart->CurrentFeature() == nullptr ||
                workPart->CurrentFeature()->Tag() != insertionFeature->Tag())
                insertionFeature->MakeCurrentFeature();

            // Each body owns an independent custom-feature node.  Do not let
            // internal feature tags from a previous body leak into this one.
            pendingInternalFeatureTags_.clear();
            const int created = ProcessBody(body);
            if (created > 0)
            {
                if (!CreateCustomFeatureNode())
                    throw std::runtime_error(
                        "创建单个钣金体的“折弯标记缺口”自定义特征失败，"
                        "请重启 NX 后重试。");
                AppendDebugLog("custom feature node created for body=" +
                    std::to_string(body == nullptr ? 0 : body->Tag()) +
                    " internalFeatures=" +
                    std::to_string(pendingInternalFeatureTags_.size()));
                ++bodyCount;
                notchCount += created;
                pendingInternalFeatureTags_.clear();
            }

            // Return to exactly the current-feature position that was active
            // before the command.  The newly inserted node remains before the
            // target body's Flat Pattern in model history.
            if (originalCurrentFeature != nullptr &&
                IsAlive(originalCurrentFeature->Tag()))
                originalCurrentFeature->MakeCurrentFeature();
        }
        if (notchCount == 0)
            throw std::runtime_error("未找到可伸直并标记的折弯。");
        const int updateErrors =
            session_->UpdateManager()->DoUpdate(operationMark);
        if (updateErrors != 0)
            throw std::runtime_error("折弯标记缺口自定义特征更新失败。");
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
        NXOpen::Features::CustomFeatureData* data =
            part->Features()->CustomFeatureDataCollection()->CreateData(
                featureClass,
                std::vector<NXOpen::Features::CustomAttribute*>(
                    1, internalAttribute));
        internalAttribute->SetValues(internalObjects);
        builder = part->Features()->CreateCustomFeatureBuilder(nullptr);
        builder->SetFeatureData(data);
        NXOpen::Features::Feature* feature = builder->CommitFeature();
        builder->Destroy();
        builder = nullptr;
        if (feature == nullptr) return false;
        feature->SetName(zhihui_bend_mark_notch::kFeatureDisplayName);
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
    second = candidates[1].second;
    return true;
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

int BendMarkNotchDialog::ProcessBody(NXOpen::Body* body)
{
    NXOpen::Part* part = session_->Parts()->Work();
    auto* manager = part->Features()->SheetmetalManager();
    AppendDebugLog("============================================================");
    AppendDebugLog("ProcessBody begin body=" +
        std::to_string(body == nullptr ? 0 : body->Tag()));
    const double thickness = manager->GetBodyThickness(body);
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
        NXOpen::Features::Feature* unbendFeature = unbend->CommitFeature();
        unbend->Destroy();
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
        for (const BendRecord& bend : bends)
            flatBends.push_back(ResolveFlatBend(bend));

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
                {span.minimumPoint, span.maximumPoint});
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
        profiles.reserve(flatBends.size() * 2);
        for (const FlatBend& flat : flatBends)
        {
            const NXOpen::Vector3d lineDirection = Normalize(
                Subtract(flat.secondEnd, flat.firstEnd));
            if (NotchType() == 0)
            {
                const double radius = DoubleValue(diameter_) * 0.5;
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
                const double angle = DoubleValue(angle_);
                const double depth = DoubleValue(depth_);
                const double halfWidth = depth *
                    std::tan(0.5 * angle * kPi / 180.0);
                const auto appendTriangle = [&](
                    const NXOpen::Point3d& edgeCenter,
                    const NXOpen::Vector3d& inwardInput)
                {
                    const NXOpen::Vector3d inward = Normalize(inwardInput);
                    const NXOpen::Vector3d baseDirection =
                        Normalize(Cross(normal, inward));
                    NotchProfile profile;
                    profile.center = edgeCenter;
                    profile.polygon = {
                        Add(edgeCenter, Scale(baseDirection, halfWidth)),
                        Add(edgeCenter, Scale(baseDirection, -halfWidth)),
                        Add(edgeCenter, Scale(inward, depth))};
                    profiles.push_back(profile);
                };
                appendTriangle(flat.firstEnd, lineDirection);
                appendTriangle(flat.secondEnd, Scale(lineDirection, -1.0));
            }
        }
        AppendDebugLog("notch profiles built type=" +
            std::to_string(NotchType()) + " count=" +
            std::to_string(profiles.size()));
        if (profiles.empty()) throw std::runtime_error("展平状态创建缺口失败。");
        ToolRecord groupedTool;
        failureStage = "create combined sketch extrusion tool";
        if (!CreateInternalSketchExtrudeTool(
                normal, profiles, thickness, groupedTool))
            throw std::runtime_error("全部缺口合并草图拉伸失败。");
        const std::vector<ToolRecord> tools(1, groupedTool);
        NXOpen::Features::Feature* booleanFeature = nullptr;
        failureStage = "subtract combined tool bodies";
        if (!SubtractToolsOnce(body, tools, booleanFeature) ||
            booleanFeature == nullptr)
            throw std::runtime_error("全部缺口工具体一次布尔减失败。");

        // Reuse exactly the same paired bend faces selected for Unbend.  Do
        // not re-query, remap, de-duplicate, or pre-validate them after the
        // Boolean; Rebend receives the original selection unchanged.
        AppendDebugLog("rebend selection source=original-unbend-pairs faceCount=" +
            std::to_string(bendFaces.size()));
        NXOpen::FaceDumbRule* rebendRule =
            part->ScRuleFactory()->CreateRuleFaceDumb(bendFaces);
        NXOpen::ScCollector* rebendCollector =
            part->ScCollectors()->CreateCollector();
        rebendCollector->ReplaceRules(
            std::vector<NXOpen::SelectionIntentRule*>(1, rebendRule), false);
        auto* rebend = manager->CreateRebendFeatureBuilder(nullptr);
        rebend->SetFaceCollector(rebendCollector);
        failureStage = "commit rebend";
        NXOpen::Features::Feature* rebendFeature = rebend->CommitFeature();
        rebend->Destroy();
        if (rebendFeature == nullptr)
            throw std::runtime_error("创建缺口后重新折弯失败。");
        failureStage = "update model after rebend";
        const int errors = session_->UpdateManager()->DoUpdate(mark);
        if (errors != 0) throw std::runtime_error("折弯标记缺口模型更新失败。");

        pendingInternalFeatureTags_.push_back(unbendFeature->Tag());
        pendingInternalFeatureTags_.push_back(groupedTool.featureTag);
        pendingInternalFeatureTags_.push_back(booleanFeature->Tag());
        pendingInternalFeatureTags_.push_back(rebendFeature->Tag());
        session_->DeleteUndoMark(mark, "折弯标记缺口内部处理");
        AppendDebugLog("ProcessBody success profiles=" +
            std::to_string(profiles.size()) + " rebendFeature=" +
            std::to_string(rebendFeature->Tag()));
        return static_cast<int>(profiles.size());
    }
    catch (const NXOpen::NXException& ex)
    {
        AppendDebugLog("ProcessBody NXException stage=" + failureStage +
            " code=" + std::to_string(ex.ErrorCode()) + " message=" +
            ex.Message());
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

bool BendMarkNotchDialog::SubtractToolsOnce(
    NXOpen::Body* body, const std::vector<ToolRecord>& tools,
    NXOpen::Features::Feature*& booleanFeature) const
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
    double thickness, ToolRecord& tool) const
{
    tool = ToolRecord();
    NXOpen::Part* part = session_->Parts()->Work();
    if (part == nullptr || profiles.empty()) return false;
    for (const NotchProfile& profile : profiles)
        if (profile.circleRadius <= kTolerance && profile.polygon.size() < 3)
            return false;

    const NXOpen::Vector3d normal = Normalize(normalInput);
    const NXOpen::Point3d origin = profiles.front().center;
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

        failureStage = "activate sketch";
        sketch->Activate(NXOpen::Sketch::ViewReorientFalse);
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
                        throw std::runtime_error("创建三角缺口草图失败。");
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
            NumberBuffer(-(thickness + margin)).data());
        extrudeBuilder->Limits()->EndExtend()->Value()->SetFormula(
            NumberBuffer(thickness + margin).data());
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
