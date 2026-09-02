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
#include <NXOpen/Features_SketchFeature.hxx>
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
                FindBodyInsertionFeature(
                    std::vector<NXOpen::Body*>(1, body)) != nullptr)
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
    NXOpen::Features::Feature* insertionFeature =
        FindBodyInsertionFeature(bodies);
    if (workPart == nullptr || insertionFeature == nullptr)
        throw std::runtime_error(
            "无法确定钣金体的插入位置，已停止创建缺口。");
    NXOpen::Features::Feature* originalCurrentFeature =
        workPart->CurrentFeature();

    const NXOpen::Session::UndoMarkId operationMark = session_->SetUndoMark(
        NXOpen::Session::MarkVisibilityVisible, "折弯标记缺口");
    DisplaySuppressionGuard displayGuard;
    pendingInternalFeatureTags_.clear();
    try
    {
        if (originalCurrentFeature == nullptr ||
            originalCurrentFeature->Tag() != insertionFeature->Tag())
            insertionFeature->MakeCurrentFeature();

        int bodyCount = 0;
        int notchCount = 0;
        for (NXOpen::Body* body : bodies)
        {
            const int created = ProcessBody(body);
            if (created > 0)
            {
                ++bodyCount;
                notchCount += created;
            }
        }
        if (notchCount == 0)
            throw std::runtime_error("未找到可伸直并标记的折弯。");
        if (!CreateCustomFeatureNode())
            throw std::runtime_error(
                "创建“折弯标记缺口”自定义特征失败，请重启 NX 后重试。");
        if (originalCurrentFeature != nullptr &&
            IsAlive(originalCurrentFeature->Tag()))
            originalCurrentFeature->MakeCurrentFeature();
        else
            workPart->ResetTimestampToLatestFeature();
        const int updateErrors =
            session_->UpdateManager()->DoUpdate(operationMark);
        if (updateErrors != 0)
            throw std::runtime_error("折弯标记缺口自定义特征更新失败。");

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
    std::vector<BendRecord> result;
    std::set<tag_t> seen;
    for (std::size_t index = 0; index < innerFaces.size(); ++index)
    {
        NXOpen::Face* inner = innerFaces[index];
        if (inner == nullptr || !seen.insert(inner->Tag()).second) continue;
        if (index < states.size() && states[index] ==
                NXOpen::Features::SheetMetal::SheetmetalBendStateFlat)
            continue;
        BendRecord record;
        record.innerFace = inner;
        record.outerFace = FindOppositeBendFace(body, inner, thickness);
        if (record.outerFace == nullptr ||
            !FindBoundaryEdges(inner, record.firstBoundary,
                               record.secondBoundary))
            continue;
        result.push_back(record);
    }
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
    const double thickness = manager->GetBodyThickness(body);
    NXOpen::Face* referenceFace = FindReferenceFace(body);
    const std::vector<BendRecord> bends = CollectBends(body, thickness);
    if (referenceFace == nullptr || bends.empty()) return 0;

    const NXOpen::Session::UndoMarkId mark = session_->SetUndoMark(
        NXOpen::Session::MarkVisibilityInvisible, "折弯标记缺口内部处理");
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
        NXOpen::Features::Feature* unbendFeature = unbend->CommitFeature();
        unbend->Destroy();
        if (unbendFeature == nullptr)
            throw std::runtime_error("钣金体全部折弯一次伸直失败。");

        std::vector<FlatBend> flatBends;
        for (const BendRecord& bend : bends)
            flatBends.push_back(ResolveFlatBend(bend));
        const NXOpen::Vector3d normal = ReferenceNormal(referenceFace);

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
        if (profiles.empty()) throw std::runtime_error("展平状态创建缺口失败。");
        ToolRecord groupedTool;
        if (!CreateInternalSketchExtrudeTool(
                normal, profiles, thickness, groupedTool))
            throw std::runtime_error("全部缺口合并草图拉伸失败。");
        const std::vector<ToolRecord> tools(1, groupedTool);
        NXOpen::Features::Feature* booleanFeature = nullptr;
        if (!SubtractToolsOnce(body, tools, booleanFeature) ||
            booleanFeature == nullptr)
            throw std::runtime_error("全部缺口工具体一次布尔减失败。");

        // 端部缺口会改变折弯面的边界拓扑，切除前保存的圆柱面标签可能
        // 因此失效。重新向钣金管理器查询当前 Flat 状态的折弯面，避免
        // 把陈旧标签交给 RebendBuilder。
        std::vector<NXOpen::Face*> currentInnerFaces;
        std::vector<NXOpen::Features::SheetMetal::SheetmetalBendState>
            currentStates;
        manager->GetInnerBendFaces(body, currentInnerFaces, currentStates);
        std::vector<NXOpen::Face*> rebendFaces;
        std::set<tag_t> rebendTags;
        std::size_t rebendPairCount = 0;
        for (std::size_t index = 0; index < currentInnerFaces.size(); ++index)
        {
            NXOpen::Face* face = currentInnerFaces[index];
            const bool flat = index < currentStates.size() &&
                currentStates[index] ==
                    NXOpen::Features::SheetMetal::SheetmetalBendStateFlat;
            if (face == nullptr || !IsAlive(face->Tag()) || !flat) continue;

            // Rebend requires the current inner/outer layer pair.  A Boolean
            // notch may replace either planar bend face, so resolve the
            // opposite face from the post-Boolean topology instead of using
            // the pre-cut outer-face tag.
            NXOpen::Face* opposite = nullptr;
            try { opposite = manager->GetOppositeFace(face); }
            catch (const NXOpen::NXException&) { opposite = nullptr; }
            if (opposite == nullptr || !IsAlive(opposite->Tag())) continue;
            if (manager->GetFaceType(face) !=
                    NXOpen::Features::SheetMetal::SheetmetalFaceTypeBend ||
                manager->GetFaceType(opposite) !=
                    NXOpen::Features::SheetMetal::SheetmetalFaceTypeBend)
                continue;

            if (rebendTags.insert(face->Tag()).second)
                rebendFaces.push_back(face);
            if (rebendTags.insert(opposite->Tag()).second)
                rebendFaces.push_back(opposite);
            ++rebendPairCount;
        }
        if (rebendPairCount < bends.size() ||
            rebendFaces.size() < bends.size() * 2)
            throw std::runtime_error(
                "切除后未找到完整的内、外层 Flat 折弯面对。");
        NXOpen::FaceDumbRule* rebendRule =
            part->ScRuleFactory()->CreateRuleFaceDumb(rebendFaces);
        NXOpen::ScCollector* rebendCollector =
            part->ScCollectors()->CreateCollector();
        rebendCollector->ReplaceRules(
            std::vector<NXOpen::SelectionIntentRule*>(1, rebendRule), false);
        auto* rebend = manager->CreateRebendFeatureBuilder(nullptr);
        rebend->SetFaceCollector(rebendCollector);
        NXOpen::Features::Feature* rebendFeature = rebend->CommitFeature();
        rebend->Destroy();
        if (rebendFeature == nullptr)
            throw std::runtime_error("创建缺口后重新折弯失败。");
        const int errors = session_->UpdateManager()->DoUpdate(mark);
        if (errors != 0) throw std::runtime_error("折弯标记缺口模型更新失败。");

        std::vector<NXOpen::Face*> verifiedInnerFaces;
        std::vector<NXOpen::Features::SheetMetal::SheetmetalBendState>
            verifiedStates;
        manager->GetInnerBendFaces(body, verifiedInnerFaces, verifiedStates);
        std::size_t bentCount = 0;
        for (NXOpen::Features::SheetMetal::SheetmetalBendState state :
             verifiedStates)
        {
            if (state !=
                NXOpen::Features::SheetMetal::SheetmetalBendStateFlat)
                ++bentCount;
        }
        if (bentCount < bends.size())
            throw std::runtime_error(
                "重新折弯特征已提交，但钣金体仍处于展平状态。");
        pendingInternalFeatureTags_.push_back(unbendFeature->Tag());
        pendingInternalFeatureTags_.push_back(groupedTool.featureTag);
        pendingInternalFeatureTags_.push_back(booleanFeature->Tag());
        pendingInternalFeatureTags_.push_back(rebendFeature->Tag());
        session_->DeleteUndoMark(mark, "折弯标记缺口内部处理");
        return static_cast<int>(profiles.size());
    }
    catch (...)
    {
        session_->UndoToMark(mark, "折弯标记缺口失败回退");
        throw;
    }
}

bool BendMarkNotchDialog::SubtractToolsOnce(
    NXOpen::Body* body, const std::vector<ToolRecord>& tools,
    NXOpen::Features::Feature*& booleanFeature) const
{
    booleanFeature = nullptr;
    if (body == nullptr || tools.empty()) return false;
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
    if (toolBodies.empty()) return false;

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
        return booleanFeature != nullptr;
    }
    catch (...)
    {
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
    try
    {
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

        sketch->Activate(NXOpen::Sketch::ViewReorientFalse);
        for (const NotchProfile& profile : profiles)
        {
            if (profile.circleRadius > kTolerance)
            {
                NXOpen::Arc* circle = part->Curves()->CreateArc(
                    profile.center, xAxis, yAxis, profile.circleRadius,
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
                    NXOpen::Line* line = part->Curves()->CreateLine(
                        profile.polygon[index],
                        profile.polygon[(index + 1) % profile.polygon.size()]);
                    if (line == nullptr)
                        throw std::runtime_error("创建三角缺口草图失败。");
                    curveTags.push_back(line->Tag());
                    sketch->AddGeometry(
                        line,
                        NXOpen::Sketch::InferConstraintsOptionInferNoConstraints);
                }
            }
        }
        sketch->Update();
        sketch->UpdateNavigator();
        sketch->Deactivate(
            NXOpen::Sketch::ViewReorientFalse, NXOpen::Sketch::UpdateLevelModel);

        std::vector<NXOpen::Curve*> profileCurves;
        for (NXOpen::NXObject* geometry : sketch->GetAllGeometry())
        {
            auto* curve = dynamic_cast<NXOpen::Curve*>(geometry);
            if (curve != nullptr) profileCurves.push_back(curve);
        }
        if (profileCurves.empty())
            throw std::runtime_error("缺口草图中没有有效轮廓。");

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
        section->AddToSection(
            std::vector<NXOpen::SelectionIntentRule*>(1, profileRule),
            profileCurves.front(), nullptr, nullptr, origin,
            NXOpen::Section::ModeCreate, false);

        NXOpen::Direction* extrusionDirection = part->Directions()->CreateDirection(
            origin, normal, NXOpen::SmartObject::UpdateOptionWithinModeling);
        if (extrusionDirection == nullptr)
            throw std::runtime_error("创建缺口拉伸方向失败。");
        extrusionDirectionTag = extrusionDirection->Tag();
        extrusionDirection->SetVisibility(
            NXOpen::SmartObject::VisibilityOptionInvisible);
        extrudeBuilder->SetDirection(extrusionDirection);
        extrudeBuilder->SetParentFeatureInternal(sketchFeature);
        extrudeFeature = extrudeBuilder->CommitFeature();
        extrudeBuilder->Destroy();
        extrudeBuilder = nullptr;
        if (extrudeFeature == nullptr)
            throw std::runtime_error("创建缺口拉伸体失败。");
        if (!sketchFeature->IsInternal()) extrudeFeature->MakeSketchInternal();
        if (!sketchFeature->IsInternal())
            throw std::runtime_error("缺口草图未能放入拉伸特征内部。");

        const std::vector<NXOpen::Body*> toolBodies = extrudeFeature->GetBodies();
        for (NXOpen::Body* toolBody : toolBodies)
            if (toolBody != nullptr && IsAlive(toolBody->Tag()))
                tool.bodyTags.push_back(toolBody->Tag());
        if (tool.bodyTags.empty())
            throw std::runtime_error("取得合并缺口拉伸工具体失败。");
        tool.featureTag = extrudeFeature->Tag();
        return true;
    }
    catch (...)
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
        return false;
    }
}
