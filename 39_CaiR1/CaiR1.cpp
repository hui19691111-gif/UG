#include "CaiR1.hpp"
#include "CaiR1CustomFeatureShared.hpp"
#include "../../common/ZhihuiDialogMemory.hpp"
#include "../../common/ZhihuiContextHelp.hpp"

#ifdef CreateDialog
#undef CreateDialog
#endif

#include <NXOpen/BlockStyler_PropertyList.hxx>
#include <NXOpen/Body.hxx>
#include <NXOpen/BodyCollection.hxx>
#include <NXOpen/BodyDumbRule.hxx>
#include <NXOpen/Callback.hxx>
#include <NXOpen/Direction.hxx>
#include <NXOpen/DirectionCollection.hxx>
#include <NXOpen/Edge.hxx>
#include <NXOpen/EdgeDumbRule.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/FaceDumbRule.hxx>
#include <NXOpen/Features_ExtrudeBuilder.hxx>
#include <NXOpen/Features_BooleanBuilder.hxx>
#include <NXOpen/Features_ConstructionFeatureData.hxx>
#include <NXOpen/Features_CustomAttribute.hxx>
#include <NXOpen/Features_CustomAttributeCollection.hxx>
#include <NXOpen/Features_CustomDoubleAttribute.hxx>
#include <NXOpen/Features_CustomFeature.hxx>
#include <NXOpen/Features_CustomFeatureBuilder.hxx>
#include <NXOpen/Features_CustomFeatureClass.hxx>
#include <NXOpen/Features_CustomFeatureClassManager.hxx>
#include <NXOpen/Features_CustomFeatureData.hxx>
#include <NXOpen/Features_CustomFeatureDataCollection.hxx>
#include <NXOpen/Features_CustomFeaturePreUpdateEvent.hxx>
#include <NXOpen/Features_CustomTagAttribute.hxx>
#include <NXOpen/Features_Feature.hxx>
#include <NXOpen/Features_FeatureCollection.hxx>
#include <NXOpen/Features_PullFaceBuilder.hxx>
#include <NXOpen/Features_TrimBody2Builder.hxx>
#include <NXOpen/GeometricUtilities_BooleanOperation.hxx>
#include <NXOpen/GeometricUtilities_BooleanToolBuilder.hxx>
#include <NXOpen/GeometricUtilities_Extend.hxx>
#include <NXOpen/GeometricUtilities_FacePlaneToolBuilder.hxx>
#include <NXOpen/GeometricUtilities_FeatureOffset.hxx>
#include <NXOpen/GeometricUtilities_FeatureOptions.hxx>
#include <NXOpen/GeometricUtilities_Limits.hxx>
#include <NXOpen/GeometricUtilities_ModlMotion.hxx>
#include <NXOpen/GeometricUtilities_SmartVolumeProfileBuilder.hxx>
#include <NXOpen/LogFile.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXMessageBox.hxx>
#include <NXOpen/NXObject.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/Plane.hxx>
#include <NXOpen/PlaneCollection.hxx>
#include <NXOpen/ScCollector.hxx>
#include <NXOpen/ScCollectorCollection.hxx>
#include <NXOpen/ScRuleFactory.hxx>
#include <NXOpen/Section.hxx>
#include <NXOpen/SectionCollection.hxx>
#include <NXOpen/Selection.hxx>
#include <NXOpen/SelectionIntentRule.hxx>
#include <NXOpen/SelectionIntentRuleOptions.hxx>
#include <NXOpen/SmartObject.hxx>
#include <NXOpen/TaggedObject.hxx>
#include <NXOpen/UI.hxx>
#include <NXOpen/Update.hxx>

#include <Windows.h>
#ifdef CreateDialog
#undef CreateDialog
#endif

#include <uf_eval.h>
#include <uf_disp.h>
#include <uf_modl.h>
#include <uf_modl_legacy.h>
#include <uf_obj.h>
#include <uf_object_types.h>
#include <uf_ui_types.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <vector>

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace
{
CaiR1Dialog* gActiveCaiR1Dialog = nullptr;
constexpr double kTolerance = 1.0e-5;
constexpr double kParallelTolerance = 0.999;

double Dot(const NXOpen::Vector3d& a, const NXOpen::Vector3d& b)
{
    return a.X * b.X + a.Y * b.Y + a.Z * b.Z;
}

double Length(const NXOpen::Vector3d& value)
{
    return std::sqrt(Dot(value, value));
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

NXOpen::Vector3d Subtract(const NXOpen::Point3d& to,
                          const NXOpen::Point3d& from)
{
    return NXOpen::Vector3d(to.X - from.X, to.Y - from.Y, to.Z - from.Z);
}

NXOpen::Vector3d Cross(const NXOpen::Vector3d& a,
                       const NXOpen::Vector3d& b)
{
    return NXOpen::Vector3d(a.Y * b.Z - a.Z * b.Y,
                            a.Z * b.X - a.X * b.Z,
                            a.X * b.Y - a.Y * b.X);
}

NXOpen::Point3d Move(const NXOpen::Point3d& point,
                     const NXOpen::Vector3d& direction,
                     double distance)
{
    return NXOpen::Point3d(point.X + direction.X * distance,
                           point.Y + direction.Y * distance,
                           point.Z + direction.Z * distance);
}

double Distance(const NXOpen::Point3d& a, const NXOpen::Point3d& b)
{
    return Length(Subtract(a, b));
}

std::string Number(double value)
{
    std::ostringstream stream;
    stream.precision(15);
    stream << value;
    return stream.str();
}

std::string DialogPath()
{
    wchar_t modulePath[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(
        reinterpret_cast<HMODULE>(&__ImageBase), modulePath, MAX_PATH);
    if (length > 0 && length < MAX_PATH)
    {
        const std::filesystem::path path =
            std::filesystem::path(modulePath).parent_path() / L"CaiR1.dlx";
        if (std::filesystem::exists(path))
        {
            return path.string();
        }
    }
    return "CaiR1.dlx";
}

std::string DialogPathWithHelp()
{
    zhihui_context_help::EnsureGlobalHelpLoaded();
    return DialogPath();
}

bool FacePlaneData(NXOpen::Face* face,
                   NXOpen::Point3d& point,
                   NXOpen::Vector3d& outwardNormal)
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
    outwardNormal =
        NXOpen::Vector3d(direction[0] * normalDirection,
                         direction[1] * normalDirection,
                         direction[2] * normalDirection);
    return Normalize(outwardNormal);
}

NXOpen::Point3d FaceBoxCenter(NXOpen::Face* face)
{
    int type = 0;
    double origin[3] = {};
    double direction[3] = {};
    double box[6] = {};
    double radius = 0.0;
    double radiusData = 0.0;
    int normalDirection = 1;
    if (face != nullptr &&
        UF_MODL_ask_face_data(face->Tag(), &type, origin, direction, box,
                              &radius, &radiusData,
                              &normalDirection) == 0)
    {
        return NXOpen::Point3d((box[0] + box[3]) * 0.5,
                               (box[1] + box[4]) * 0.5,
                               (box[2] + box[5]) * 0.5);
    }
    return NXOpen::Point3d(0.0, 0.0, 0.0);
}

NXOpen::Point3d BodyBoxCenter(NXOpen::Body* body)
{
    double box[6] = {};
    if (body != nullptr &&
        UF_MODL_ask_bounding_box(body->Tag(), box) == 0)
    {
        return NXOpen::Point3d((box[0] + box[3]) * 0.5,
                               (box[1] + box[4]) * 0.5,
                               (box[2] + box[5]) * 0.5);
    }
    return NXOpen::Point3d(0.0, 0.0, 0.0);
}

bool PointOnFace(NXOpen::Face* face, const NXOpen::Point3d& point)
{
    double coordinates[3] = {point.X, point.Y, point.Z};
    int status = 0;
    return face != nullptr &&
           UF_MODL_ask_point_containment(coordinates, face->Tag(), &status) == 0 &&
           (status == 1 || status == 3);
}

bool PointInsideBody(NXOpen::Body* body, const NXOpen::Point3d& point)
{
    double coordinates[3] = {point.X, point.Y, point.Z};
    int status = 0;
    return body != nullptr &&
           UF_MODL_ask_point_containment(
               coordinates, body->Tag(), &status) == 0 &&
           status == 1;
}

bool BodyVolume(NXOpen::Body* body, double& volume)
{
    volume = 0.0;
    if (body == nullptr || body->Tag() == NULL_TAG)
    {
        return false;
    }
    tag_t bodyTag = body->Tag();
    double accuracyValues[11] = {};
    accuracyValues[0] = 0.99;
    double massProperties[47] = {};
    double statistics[13] = {};
    if (UF_MODL_ask_mass_props_3d(
            &bodyTag, 1, 1, 4, 0.0, 1,
            accuracyValues, massProperties, statistics) != 0)
    {
        return false;
    }
    volume = std::fabs(massProperties[1]);
    return std::isfinite(volume) && volume > 0.0;
}

bool CopiedBodyVolume(NXOpen::Body* body, double& volume)
{
    volume = 0.0;
    if (body == nullptr || body->Tag() == NULL_TAG)
    {
        return false;
    }
    tag_t sourceTag = body->Tag();
    tag_t copiedTag = NULL_TAG;
    uf6511(&sourceTag, &copiedTag);
    if (copiedTag == NULL_TAG)
    {
        return false;
    }
    NXOpen::Body* copiedBody = dynamic_cast<NXOpen::Body*>(
        NXOpen::NXObjectManager::Get(copiedTag));
    return BodyVolume(copiedBody, volume);
}

bool PlaneCutsBody(NXOpen::Body* body,
                   const NXOpen::Point3d& planeOrigin,
                   const NXOpen::Vector3d& planeNormal,
                   double tolerance)
{
    if (body == nullptr)
    {
        return false;
    }

    double minimum = (std::numeric_limits<double>::max)();
    double maximum = -(std::numeric_limits<double>::max)();
    std::set<tag_t> sampledEdges;
    for (NXOpen::Face* face : body->GetFaces())
    {
        if (face == nullptr)
        {
            continue;
        }
        const double faceDistance =
            Dot(Subtract(FaceBoxCenter(face), planeOrigin),
                planeNormal);
        minimum = (std::min)(minimum, faceDistance);
        maximum = (std::max)(maximum, faceDistance);

        for (NXOpen::Edge* edge : face->GetEdges())
        {
            if (edge == nullptr ||
                !sampledEdges.insert(edge->Tag()).second)
            {
                continue;
            }
            NXOpen::Point3d first;
            NXOpen::Point3d second;
            try
            {
                edge->GetVertices(&first, &second);
            }
            catch (...)
            {
                continue;
            }
            for (const NXOpen::Point3d& point : {first, second})
            {
                const double distance =
                    Dot(Subtract(point, planeOrigin), planeNormal);
                minimum = (std::min)(minimum, distance);
                maximum = (std::max)(maximum, distance);
            }
        }
    }
    return minimum < -tolerance && maximum > tolerance;
}

double DistanceToEdge(NXOpen::Edge* edge, const NXOpen::Point3d& point)
{
    if (edge == nullptr)
    {
        return (std::numeric_limits<double>::max)();
    }

    double result = (std::numeric_limits<double>::max)();
    UF_EVAL_p_t evaluator = nullptr;
    if (UF_EVAL_initialize(edge->Tag(), &evaluator) == 0 &&
        evaluator != nullptr)
    {
        double limits[2] = {};
        if (UF_EVAL_ask_limits(evaluator, limits) == 0)
        {
            constexpr int divisions = 64;
            for (int index = 0; index <= divisions; ++index)
            {
                const double parameter =
                    limits[0] + (limits[1] - limits[0]) *
                                    static_cast<double>(index) / divisions;
                double coordinates[3] = {};
                if (UF_EVAL_evaluate(evaluator, 0, parameter,
                                     coordinates, nullptr) == 0)
                {
                    result = (std::min)(
                        result,
                        Distance(point,
                                 NXOpen::Point3d(coordinates[0],
                                                 coordinates[1],
                                                 coordinates[2])));
                }
            }
        }
        UF_EVAL_free(evaluator);
    }
    return result;
}

bool EdgePointAtFraction(NXOpen::Edge* edge, double fraction,
                         NXOpen::Point3d& point)
{
    if (edge == nullptr)
    {
        return false;
    }

    UF_EVAL_p_t evaluator = nullptr;
    if (UF_EVAL_initialize(edge->Tag(), &evaluator) != 0 ||
        evaluator == nullptr)
    {
        return false;
    }

    bool succeeded = false;
    double limits[2] = {};
    if (UF_EVAL_ask_limits(evaluator, limits) == 0)
    {
        const double boundedFraction =
            (std::max)(0.0, (std::min)(1.0, fraction));
        const double parameter =
            limits[0] + (limits[1] - limits[0]) * boundedFraction;
        double coordinates[3] = {};
        if (UF_EVAL_evaluate(evaluator, 0, parameter,
                             coordinates, nullptr) == 0)
        {
            point = NXOpen::Point3d(coordinates[0], coordinates[1],
                                    coordinates[2]);
            succeeded = true;
        }
    }
    UF_EVAL_free(evaluator);
    return succeeded;
}

NXOpen::Face* AdjacentPlanarFace(NXOpen::Edge* edge,
                                 NXOpen::Face* excludedFace,
                                 NXOpen::Body* body)
{
    if (edge == nullptr)
    {
        return nullptr;
    }
    for (NXOpen::Face* face : edge->GetUnsortedFaces())
    {
        if (face != nullptr && face != excludedFace &&
            face->GetBody() == body &&
            face->SolidFaceType() == NXOpen::Face::FaceTypePlanar)
        {
            return face;
        }
    }
    return nullptr;
}

bool EdgeTouchesPoint(NXOpen::Edge* edge, const NXOpen::Point3d& point)
{
    if (edge == nullptr)
    {
        return false;
    }
    NXOpen::Point3d first;
    NXOpen::Point3d second;
    try
    {
        edge->GetVertices(&first, &second);
    }
    catch (...)
    {
        return false;
    }
    return Distance(first, point) <= 1.0e-3 ||
           Distance(second, point) <= 1.0e-3;
}
}

CaiR1Dialog::CaiR1Dialog()
    : ui_(NXOpen::UI::GetUI()),
      session_(NXOpen::Session::GetSession()),
      dialog_(ui_->CreateDialog(DialogPathWithHelp().c_str())),
      faceSelect_(nullptr),
      extensionLengthInput_(nullptr),
      gapInput_(nullptr),
      bendRadiusInput_(nullptr),
      customFeatureManager_(nullptr),
      editedFeature_(nullptr),
      featureClass_(nullptr),
      previewMark_(static_cast<NXOpen::Session::UndoMarkId>(0)),
      selectedFaceTag_(NULL_TAG),
      targetBodyTag_(NULL_TAG),
      selectedPickPoint_(0.0, 0.0, 0.0),
      hasPreview_(false),
      rebuildingPreview_(false),
      changingSelection_(false),
      buildingCustomFeature_(false),
      loadingEditedFeature_(false),
      previewFeatures_(),
      previewBodyTranslucencies_()
{
    customFeatureManager_ = session_->CustomFeatureClassManager();
    editedFeature_ = customFeatureManager_->GetEditedCustomFeature();
    try
    {
        featureClass_ = customFeatureManager_->GetClassFromName(
            zhihui_cair1::kFeatureClassName);
    }
    catch (...)
    {
        featureClass_ = nullptr;
    }
    gActiveCaiR1Dialog = this;
    dialog_->AddInitializeHandler(
        NXOpen::make_callback(this, &CaiR1Dialog::initialize_cb));
    dialog_->AddDialogShownHandler(
        NXOpen::make_callback(this, &CaiR1Dialog::dialogShown_cb));
    dialog_->AddUpdateHandler(
        NXOpen::make_callback(this, &CaiR1Dialog::update_cb));
    dialog_->AddApplyHandler(
        NXOpen::make_callback(this, &CaiR1Dialog::apply_cb));
    dialog_->AddOkHandler(
        NXOpen::make_callback(this, &CaiR1Dialog::ok_cb));
    dialog_->AddCancelHandler(
        NXOpen::make_callback(this, &CaiR1Dialog::cancel_cb));
}

CaiR1Dialog::~CaiR1Dialog()
{
    RestorePreviewTranslucency();
    UndoPreview();
    if (gActiveCaiR1Dialog == this)
    {
        gActiveCaiR1Dialog = nullptr;
    }
    delete dialog_;
}

NXOpen::BlockStyler::BlockDialog::DialogResponse CaiR1Dialog::Launch()
{
    return dialog_->LaunchInDialogMode(
        editedFeature_ != nullptr
            ? NXOpen::BlockStyler::BlockDialog::DialogModeEdit
            : NXOpen::BlockStyler::BlockDialog::DialogModeCreate);
}

void CaiR1Dialog::initialize_cb()
{
    try
    {
        faceSelect_ = dialog_->TopBlock()->FindBlock("face_select");
        extensionLengthInput_ =
            dialog_->TopBlock()->FindBlock("extension_length");
        gapInput_ = dialog_->TopBlock()->FindBlock("gap_input");
        bendRadiusInput_ =
            dialog_->TopBlock()->FindBlock("bend_radius_input");
        if (faceSelect_ == nullptr || extensionLengthInput_ == nullptr ||
            gapInput_ == nullptr ||
            bendRadiusInput_ == nullptr)
        {
            throw std::runtime_error(
                "CaiR1.dlx 缺少必要的选择或尺寸输入控件。");
        }

        NXOpen::BlockStyler::PropertyList* properties =
            faceSelect_->GetProperties();
        std::vector<NXOpen::Selection::MaskTriple> masks;
        masks.emplace_back(UF_solid_type, UF_solid_face_subtype,
                           UF_UI_SEL_FEATURE_CYLINDRICAL_FACE);
        properties->SetSelectionFilter(
            "SelectionFilter",
            NXOpen::Selection::SelectionActionClearAndEnableSpecific,
            masks);
        properties->SetEnum("StepStatus", 1);
        delete properties;

        if (editedFeature_ == nullptr)
        {
            zhihui_dialog_memory::LoadDouble(
                L"CaiR1_state.ini", L"ExtensionLength",
                extensionLengthInput_);
            zhihui_dialog_memory::LoadDouble(
                L"CaiR1_state.ini", L"Gap", gapInput_);
            zhihui_dialog_memory::LoadDouble(
                L"CaiR1_state.ini", L"BendRadius",
                bendRadiusInput_);
        }
        LoadEditedCustomFeatureData();
    }
    catch (const NXOpen::NXException& ex)
    {
        ShowError(ex.Message() != nullptr ? ex.Message()
                                         : "初始化对话框失败。");
    }
    catch (const std::exception& ex)
    {
        ShowError(ex.what());
    }
}

void CaiR1Dialog::dialogShown_cb()
{
    try
    {
        if (customFeatureManager_ != nullptr)
        {
            NXOpen::Features::CustomFeature* currentEdited =
                customFeatureManager_->GetEditedCustomFeature();
            if (currentEdited != nullptr &&
                currentEdited != editedFeature_)
            {
                editedFeature_ = currentEdited;
                LoadEditedCustomFeatureData();
            }
        }
        if (faceSelect_ != nullptr && editedFeature_ != nullptr)
        {
            faceSelect_->SetEnable(false);
        }
        else if (faceSelect_ != nullptr)
        {
            faceSelect_->Focus();
        }
    }
    catch (const NXOpen::NXException& ex)
    {
        ShowError(ex.Message() != nullptr ? ex.Message()
                                         : "设置选择焦点失败。");
    }
    catch (...)
    {
        ShowError("设置选择焦点失败。");
    }
}

int CaiR1Dialog::update_cb(NXOpen::BlockStyler::UIBlock* block)
{
    try
    {
        if (loadingEditedFeature_ || rebuildingPreview_ ||
            changingSelection_ ||
            (block != faceSelect_ &&
             block != extensionLengthInput_ &&
             block != gapInput_ &&
             block != bendRadiusInput_))
        {
            return 0;
        }

        NXOpen::Face* newlySelectedFace = nullptr;
        NXOpen::Point3d newlySelectedPick(0.0, 0.0, 0.0);
        if (block == faceSelect_)
        {
            newlySelectedFace = SelectedFace();
            if (newlySelectedFace == nullptr)
            {
                return 0;
            }
            newlySelectedPick = PickPoint();
        }
        else if (CachedFace() == nullptr)
        {
            return 0;
        }

        rebuildingPreview_ = true;
        const bool undone = UndoPreview();
        if (undone && newlySelectedFace != nullptr)
        {
            selectedFaceTag_ = newlySelectedFace->Tag();
            selectedPickPoint_ = newlySelectedPick;
        }
        const int result = undone ? CreatePreview() : 1;
        if (result == 0 && newlySelectedFace != nullptr)
        {
            ClearSelectionAndUnhighlight(selectedFaceTag_);
        }
        rebuildingPreview_ = false;
        return result;
    }
    catch (const NXOpen::NXException& ex)
    {
        rebuildingPreview_ = false;
        ShowError(ex.Message() != nullptr ? ex.Message()
                                         : "更新预览失败。");
    }
    catch (const std::exception& ex)
    {
        rebuildingPreview_ = false;
        ShowError(ex.what());
    }
    catch (...)
    {
        rebuildingPreview_ = false;
        ShowError("更新预览时发生未知异常。");
    }
    return 1;
}

int CaiR1Dialog::apply_cb()
{
    try
    {
        const bool editing = editedFeature_ != nullptr;
        NXOpen::Face* selectedFace = CachedFace();
        if (selectedFace == nullptr)
        {
            ShowError("请选择一个圆柱面。");
            return 1;
        }
        if (!hasPreview_ && CreatePreview() != 0)
        {
            return 1;
        }

        std::string subtractError;
        RestorePreviewTranslucency();
        if (!SubtractPreviewBodies(subtractError))
        {
            ApplyPreviewTranslucency();
            ShowError(subtractError);
            return 1;
        }

        std::string featureError;
        if (!CommitCustomFeature(featureError))
        {
            ApplyPreviewTranslucency();
            ShowError(featureError);
            return 1;
        }

        const tag_t appliedFaceTag = selectedFaceTag_;
        zhihui_dialog_memory::SaveDouble(
            L"CaiR1_state.ini", L"ExtensionLength",
            extensionLengthInput_);
        zhihui_dialog_memory::SaveDouble(
            L"CaiR1_state.ini", L"Gap", gapInput_);
        zhihui_dialog_memory::SaveDouble(
            L"CaiR1_state.ini", L"BendRadius", bendRadiusInput_);

        if (!editing)
        {
            NXOpen::BlockStyler::PropertyList* properties =
                faceSelect_->GetProperties();
            changingSelection_ = true;
            properties->SetTaggedObjectVector(
                "SelectedObjects",
                std::vector<NXOpen::TaggedObject*>());
            changingSelection_ = false;
            delete properties;
            selectedFaceTag_ = NULL_TAG;
            targetBodyTag_ = NULL_TAG;
            selectedPickPoint_ =
                NXOpen::Point3d(0.0, 0.0, 0.0);
            if (appliedFaceTag != NULL_TAG &&
                UF_OBJ_ask_status(appliedFaceTag) == UF_OBJ_ALIVE)
            {
                UF_DISP_set_highlight(appliedFaceTag, 0);
            }
        }
        return 0;
    }
    catch (const NXOpen::NXException& ex)
    {
        changingSelection_ = false;
        ShowError(ex.Message() != nullptr ? ex.Message()
                                         : "应用分割圆角1失败。");
    }
    catch (const std::exception& ex)
    {
        changingSelection_ = false;
        ShowError(ex.what());
    }
    catch (...)
    {
        changingSelection_ = false;
        ShowError("应用分割圆角1时发生未知异常。");
    }
    return 1;
}

int CaiR1Dialog::ok_cb()
{
    try
    {
        return apply_cb();
    }
    catch (...)
    {
        ShowError("确认分割圆角1时发生未知异常。");
        return 1;
    }
}

int CaiR1Dialog::cancel_cb()
{
    try
    {
        return UndoPreview() ? 0 : 1;
    }
    catch (...)
    {
        ShowError("撤销预览失败。");
        return 1;
    }
}

NXOpen::Face* CaiR1Dialog::SelectedFace() const
{
    if (faceSelect_ == nullptr)
    {
        return nullptr;
    }
    NXOpen::BlockStyler::PropertyList* properties =
        faceSelect_->GetProperties();
    const std::vector<NXOpen::TaggedObject*> objects =
        properties->GetTaggedObjectVector("SelectedObjects");
    delete properties;
    if (objects.size() != 1)
    {
        return nullptr;
    }
    NXOpen::Face* face = dynamic_cast<NXOpen::Face*>(objects.front());
    return face != nullptr &&
                   face->SolidFaceType() ==
                       NXOpen::Face::FaceTypeCylindrical
               ? face
               : nullptr;
}

NXOpen::Face* CaiR1Dialog::CachedFace() const
{
    if (selectedFaceTag_ == NULL_TAG ||
        UF_OBJ_ask_status(selectedFaceTag_) != UF_OBJ_ALIVE)
    {
        return nullptr;
    }
    NXOpen::Face* face = dynamic_cast<NXOpen::Face*>(
        NXOpen::NXObjectManager::Get(selectedFaceTag_));
    return face != nullptr &&
                   face->SolidFaceType() ==
                       NXOpen::Face::FaceTypeCylindrical
               ? face
               : nullptr;
}

NXOpen::Point3d CaiR1Dialog::PickPoint() const
{
    NXOpen::BlockStyler::PropertyList* properties =
        faceSelect_->GetProperties();
    const NXOpen::Point3d point = properties->GetPoint("PickPoint");
    delete properties;
    return point;
}

double CaiR1Dialog::Gap() const
{
    NXOpen::BlockStyler::PropertyList* properties =
        gapInput_->GetProperties();
    const double value = properties->GetDouble("Value");
    delete properties;
    return (std::max)(0.0, value);
}

double CaiR1Dialog::ExtensionLength() const
{
    NXOpen::BlockStyler::PropertyList* properties =
        extensionLengthInput_->GetProperties();
    const double value = properties->GetDouble("Value");
    delete properties;
    return (std::max)(0.0, value);
}

double CaiR1Dialog::BendRadius() const
{
    NXOpen::BlockStyler::PropertyList* properties =
        bendRadiusInput_->GetProperties();
    const double value = properties->GetDouble("Value");
    delete properties;
    return (std::max)(0.0, value);
}

int CaiR1Dialog::CreatePreview()
{
    NXOpen::Face* face = CachedFace();
    if (face == nullptr)
    {
        return 0;
    }
    previewMark_ = session_->SetUndoMark(
        NXOpen::Session::MarkVisibilityInvisible,
        "分割圆角1预览");
    previewFeatures_.clear();
    if (editedFeature_ == nullptr)
    {
        targetBodyTag_ = face->GetBody()->Tag();
    }
    std::string error;
    if (!BuildSplitCorner(face, selectedPickPoint_,
                          ExtensionLength(),
                          Gap(), BendRadius(),
                          previewFeatures_, error))
    {
        UndoPreview();
        ShowError(error);
        return 1;
    }
    hasPreview_ = true;
    ApplyPreviewTranslucency();
    return 0;
}

bool CaiR1Dialog::UndoPreview()
{
    RestorePreviewTranslucency();
    const std::vector<tag_t> featureTags = previewFeatures_;
    try
    {
        if (previewMark_ !=
                static_cast<NXOpen::Session::UndoMarkId>(0) &&
            session_->DoesUndoMarkExist(previewMark_, "分割圆角1预览"))
        {
            session_->UndoToMark(previewMark_, "分割圆角1预览");
            if (session_->DoesUndoMarkExist(previewMark_,
                                            "分割圆角1预览"))
            {
                session_->DeleteUndoMark(previewMark_,
                                         "分割圆角1预览");
            }
        }
    }
    catch (...)
    {
    }

    try
    {
        std::vector<NXOpen::TaggedObject*> survivors;
        std::set<tag_t> seen;
        for (auto item = featureTags.rbegin();
             item != featureTags.rend(); ++item)
        {
            if (*item != NULL_TAG && seen.insert(*item).second &&
                UF_OBJ_ask_status(*item) == UF_OBJ_ALIVE)
            {
                NXOpen::TaggedObject* object =
                    dynamic_cast<NXOpen::TaggedObject*>(
                        NXOpen::NXObjectManager::Get(*item));
                if (object != nullptr)
                {
                    survivors.push_back(object);
                }
            }
        }
        if (!survivors.empty())
        {
            const NXOpen::Session::UndoMarkId cleanup =
                session_->SetUndoMark(
                    NXOpen::Session::MarkVisibilityInvisible,
                    "分割圆角1预览清理");
            NXOpen::Update* update = session_->UpdateManager();
            update->ClearDeleteList();
            update->ClearErrorList();
            update->AddObjectsToDeleteList(survivors);
            update->DoUpdate(cleanup);
            if (session_->DoesUndoMarkExist(cleanup,
                                            "分割圆角1预览清理"))
            {
                session_->DeleteUndoMark(cleanup,
                                         "分割圆角1预览清理");
            }
        }
    }
    catch (...)
    {
    }

    bool removed = true;
    for (tag_t tag : featureTags)
    {
        if (tag != NULL_TAG && UF_OBJ_ask_status(tag) == UF_OBJ_ALIVE)
        {
            removed = false;
            break;
        }
    }
    previewMark_ = static_cast<NXOpen::Session::UndoMarkId>(0);
    hasPreview_ = !removed;
    if (removed)
    {
        previewFeatures_.clear();
    }
    return removed;
}

void CaiR1Dialog::CommitPreview()
{
    RestorePreviewTranslucency();
    if (!hasPreview_ ||
        previewMark_ == static_cast<NXOpen::Session::UndoMarkId>(0))
    {
        return;
    }
    if (session_->DoesUndoMarkExist(previewMark_, "分割圆角1预览"))
    {
        session_->SetUndoMarkName(previewMark_, "分割圆角1");
        session_->SetUndoMarkVisibility(
            previewMark_, "分割圆角1",
            NXOpen::Session::MarkVisibilityVisible);
    }
    previewMark_ = static_cast<NXOpen::Session::UndoMarkId>(0);
    hasPreview_ = false;
    previewFeatures_.clear();
}

void CaiR1Dialog::ClearSelectionAndUnhighlight(tag_t faceTag)
{
    if (faceSelect_ == nullptr)
    {
        return;
    }
    changingSelection_ = true;
    NXOpen::BlockStyler::PropertyList* properties =
        faceSelect_->GetProperties();
    properties->SetTaggedObjectVector(
        "SelectedObjects", std::vector<NXOpen::TaggedObject*>());
    delete properties;
    if (faceTag != NULL_TAG &&
        UF_OBJ_ask_status(faceTag) == UF_OBJ_ALIVE)
    {
        UF_DISP_set_highlight(faceTag, 0);
        NXOpen::Face* face = dynamic_cast<NXOpen::Face*>(
            NXOpen::NXObjectManager::Get(faceTag));
        if (face != nullptr)
        {
            face->RedisplayObject();
        }
    }
    faceSelect_->Focus();
    changingSelection_ = false;
}

void CaiR1Dialog::ApplyPreviewTranslucency()
{
    RestorePreviewTranslucency();
    NXOpen::Part* workPart = session_->Parts()->Work();
    if (workPart == nullptr || workPart->Bodies() == nullptr)
    {
        return;
    }

    std::set<tag_t> previewBodyTags;
    for (tag_t featureTag : previewFeatures_)
    {
        if (featureTag == NULL_TAG ||
            UF_OBJ_ask_status(featureTag) != UF_OBJ_ALIVE)
        {
            continue;
        }
        NXOpen::Features::Feature* feature =
            dynamic_cast<NXOpen::Features::Feature*>(
                NXOpen::NXObjectManager::Get(featureTag));
        if (feature == nullptr)
        {
            continue;
        }
        for (NXOpen::Body* body : feature->GetBodies())
        {
            if (body != nullptr)
            {
                previewBodyTags.insert(body->Tag());
            }
        }
    }

    constexpr int kBackgroundTranslucency = 80;
    for (NXOpen::Body* body : *workPart->Bodies())
    {
        if (body == nullptr || body->Tag() == NULL_TAG ||
            UF_OBJ_ask_status(body->Tag()) != UF_OBJ_ALIVE)
        {
            continue;
        }
        UF_OBJ_translucency_t original = 0;
        if (UF_OBJ_ask_translucency(body->Tag(), &original) != 0)
        {
            continue;
        }
        previewBodyTranslucencies_.push_back(
            std::make_pair(body->Tag(),
                           static_cast<int>(original)));
        const int requested =
            previewBodyTags.count(body->Tag()) != 0
                ? 0
                : (std::max)(static_cast<int>(original),
                             kBackgroundTranslucency);
        UF_OBJ_set_translucency(
            body->Tag(),
            static_cast<UF_OBJ_translucency_t>(requested));
        body->RedisplayObject();
    }
}

void CaiR1Dialog::RestorePreviewTranslucency()
{
    for (const std::pair<tag_t, int>& state :
         previewBodyTranslucencies_)
    {
        if (state.first == NULL_TAG ||
            UF_OBJ_ask_status(state.first) != UF_OBJ_ALIVE)
        {
            continue;
        }
        UF_OBJ_set_translucency(
            state.first,
            static_cast<UF_OBJ_translucency_t>(state.second));
        NXOpen::Body* body = dynamic_cast<NXOpen::Body*>(
            NXOpen::NXObjectManager::Get(state.first));
        if (body != nullptr)
        {
            body->RedisplayObject();
        }
    }
    previewBodyTranslucencies_.clear();
}

bool CaiR1Dialog::SubtractPreviewBodies(std::string& error)
{
    NXOpen::Part* workPart = session_->Parts()->Work();
    NXOpen::Face* selectedFace = CachedFace();
    NXOpen::Body* targetBody =
        selectedFace != nullptr ? selectedFace->GetBody() : nullptr;
    if (workPart == nullptr || targetBody == nullptr)
    {
        error = "求差时未找到所选圆柱面所属实体。";
        return false;
    }

    std::vector<NXOpen::Body*> toolBodies;
    std::set<tag_t> seenBodies;
    for (tag_t featureTag : previewFeatures_)
    {
        if (featureTag == NULL_TAG ||
            UF_OBJ_ask_status(featureTag) != UF_OBJ_ALIVE)
        {
            continue;
        }
        NXOpen::Features::Feature* feature =
            dynamic_cast<NXOpen::Features::Feature*>(
                NXOpen::NXObjectManager::Get(featureTag));
        if (feature == nullptr)
        {
            continue;
        }
        for (NXOpen::Body* body : feature->GetBodies())
        {
            if (body != nullptr && body != targetBody &&
                body->Tag() != NULL_TAG &&
                UF_OBJ_ask_status(body->Tag()) == UF_OBJ_ALIVE &&
                seenBodies.insert(body->Tag()).second)
            {
                toolBodies.push_back(body);
            }
        }
    }
    if (toolBodies.empty())
    {
        error = "求差时未找到已创建的工具体。";
        return false;
    }

    NXOpen::Features::BooleanBuilder* subtract = nullptr;
    try
    {
        subtract =
            workPart->Features()->
                CreateBooleanBuilderUsingCollector(nullptr);
        subtract->SetOperation(
            NXOpen::Features::Feature::BooleanTypeSubtract);
        subtract->SetRetainTarget(false);
        subtract->SetRetainTool(false);

        NXOpen::SelectionIntentRuleOptions* targetOptions =
            workPart->ScRuleFactory()->CreateRuleOptions();
        targetOptions->SetSelectedFromInactive(false);
        NXOpen::BodyDumbRule* targetRule =
            workPart->ScRuleFactory()->CreateRuleBodyDumb(
                std::vector<NXOpen::Body*>{targetBody}, true,
                targetOptions);
        delete targetOptions;
        NXOpen::ScCollector* targetCollector =
            workPart->ScCollectors()->CreateCollector();
        targetCollector->ReplaceRules(
            std::vector<NXOpen::SelectionIntentRule*>{
                targetRule},
            false);
        subtract->SetTargetBodyCollector(targetCollector);

        NXOpen::SelectionIntentRuleOptions* toolOptions =
            workPart->ScRuleFactory()->CreateRuleOptions();
        toolOptions->SetSelectedFromInactive(false);
        NXOpen::BodyDumbRule* toolRule =
            workPart->ScRuleFactory()->CreateRuleBodyDumb(
                toolBodies, true, toolOptions);
        delete toolOptions;
        NXOpen::ScCollector* toolCollector =
            workPart->ScCollectors()->CreateCollector();
        toolCollector->ReplaceRules(
            std::vector<NXOpen::SelectionIntentRule*>{
                toolRule},
            false);
        subtract->SetToolBodyCollector(toolCollector);

        NXOpen::Features::Feature* subtractFeature =
            dynamic_cast<NXOpen::Features::Feature*>(
                subtract->Commit());
        subtract->Destroy();
        subtract = nullptr;
        if (subtractFeature == nullptr)
        {
            error = "NX 未返回求差特征。";
            return false;
        }
        previewFeatures_.push_back(subtractFeature->Tag());
        return true;
    }
    catch (const NXOpen::NXException& ex)
    {
        if (subtract != nullptr)
        {
            subtract->Destroy();
        }
        error = "创建体与选择体求差失败：" +
                std::string(ex.Message() != nullptr
                                ? ex.Message()
                                : "NXOpen 布尔求差失败。");
    }
    catch (const std::exception& ex)
    {
        if (subtract != nullptr)
        {
            subtract->Destroy();
        }
        error = "创建体与选择体求差失败：" +
                std::string(ex.what());
    }
    catch (...)
    {
        if (subtract != nullptr)
        {
            subtract->Destroy();
        }
        error = "创建体与选择体求差失败。";
    }
    return false;
}

void CaiR1Dialog::LoadEditedCustomFeatureData()
{
    if (editedFeature_ == nullptr || extensionLengthInput_ == nullptr ||
        gapInput_ == nullptr || bendRadiusInput_ == nullptr)
    {
        return;
    }

    loadingEditedFeature_ = true;
    try
    {
        NXOpen::Features::CustomFeatureData* data =
            editedFeature_->FeatureData();
        NXOpen::TaggedObject* target =
            data->CustomTagAttributeByName(
                    zhihui_cair1::kAttrTargetBody)
                ->Value();
        NXOpen::Face* storedFace =
            dynamic_cast<NXOpen::Face*>(
                data->CustomTagAttributeByName(
                        zhihui_cair1::kAttrSelectedFace)
                    ->Value());
        if (target != nullptr)
        {
            targetBodyTag_ = target->Tag();
        }
        if (storedFace != nullptr)
        {
            selectedFaceTag_ = storedFace->Tag();
        }

        NXOpen::BlockStyler::PropertyList* extensionProperties =
            extensionLengthInput_->GetProperties();
        extensionProperties->SetDouble(
            "Value",
            data->CustomDoubleAttributeByName(
                    zhihui_cair1::kAttrExtensionLength)
                ->Value());
        delete extensionProperties;
        NXOpen::BlockStyler::PropertyList* gapProperties =
            gapInput_->GetProperties();
        gapProperties->SetDouble(
            "Value",
            data->CustomDoubleAttributeByName(
                    zhihui_cair1::kAttrGap)
                ->Value());
        delete gapProperties;
        NXOpen::BlockStyler::PropertyList* radiusProperties =
            bendRadiusInput_->GetProperties();
        radiusProperties->SetDouble(
            "Value",
            data->CustomDoubleAttributeByName(
                    zhihui_cair1::kAttrBendRadius)
                ->Value());
        delete radiusProperties;

        try
        {
            selectedPickPoint_ = NXOpen::Point3d(
                data->CustomDoubleAttributeByName(
                        zhihui_cair1::kAttrPickX)
                    ->Value(),
                data->CustomDoubleAttributeByName(
                        zhihui_cair1::kAttrPickY)
                    ->Value(),
                data->CustomDoubleAttributeByName(
                        zhihui_cair1::kAttrPickZ)
                    ->Value());
        }
        catch (...)
        {
            // Compatibility for nodes created by the first one-node build,
            // before the click location was persisted.
            if (storedFace != nullptr)
            {
                int type = 0;
                double origin[3] = {};
                double direction[3] = {};
                double box[6] = {};
                double radius = 0.0;
                double radiusData = 0.0;
                int normalDirection = 1;
                if (UF_MODL_ask_face_data(
                        storedFace->Tag(), &type, origin, direction,
                        box, &radius, &radiusData,
                        &normalDirection) == 0)
                {
                    selectedPickPoint_ = NXOpen::Point3d(
                        0.5 * (box[0] + box[3]),
                        0.5 * (box[1] + box[4]),
                        0.5 * (box[2] + box[5]));
                }
            }
        }
    }
    catch (...)
    {
        loadingEditedFeature_ = false;
        throw;
    }
    loadingEditedFeature_ = false;
}

bool CaiR1Dialog::CommitCustomFeature(std::string& error)
{
    NXOpen::Part* workPart = session_->Parts()->Work();
    const bool editing = editedFeature_ != nullptr;
    if (workPart == nullptr || featureClass_ == nullptr ||
        previewFeatures_.empty())
    {
        error = featureClass_ == nullptr
                    ? "分割圆角1节点未注册，请重新启动 NX 后再试。"
                    : "没有可封装到分割圆角1节点中的建模特征。";
        return false;
    }

    NXOpen::Body* targetBody =
        targetBodyTag_ != NULL_TAG &&
                UF_OBJ_ask_status(targetBodyTag_) == UF_OBJ_ALIVE
            ? dynamic_cast<NXOpen::Body*>(
                  NXOpen::NXObjectManager::Get(targetBodyTag_))
            : nullptr;
    for (auto item = previewFeatures_.rbegin();
         item != previewFeatures_.rend() && targetBody == nullptr; ++item)
    {
        if (*item == NULL_TAG ||
            UF_OBJ_ask_status(*item) != UF_OBJ_ALIVE)
        {
            continue;
        }
        NXOpen::Features::Feature* feature =
            dynamic_cast<NXOpen::Features::Feature*>(
                NXOpen::NXObjectManager::Get(*item));
        if (feature != nullptr && !feature->GetBodies().empty())
        {
            targetBody = feature->GetBodies().front();
        }
    }
    if (targetBody == nullptr)
    {
        error = "无法取得最终求差体，不能创建分割圆角1节点。";
        return false;
    }

    NXOpen::Features::CustomFeatureBuilder* builder = nullptr;
    try
    {
        builder =
            workPart->Features()->CreateCustomFeatureBuilder(
                editedFeature_);
        NXOpen::Features::CustomFeatureData* data = nullptr;
        if (!editing)
        {
            NXOpen::Features::CustomAttributeCollection* attributes =
                workPart->Features()->CustomAttributeCollection();
            std::vector<NXOpen::Features::CustomAttribute*> values;
            const std::vector<NXOpen::Features::CustomAttribute::Property>
                targetProperties{
                    NXOpen::Features::CustomAttribute::
                        PropertyMandatoryInput,
                    NXOpen::Features::CustomAttribute::
                        PropertyIsReferencingTargetBody};
            const std::vector<
                NXOpen::Features::CustomAttribute::Property>
                optional;
            values.push_back(attributes->CreateCustomTagAttribute(
                zhihui_cair1::kAttrTargetBody,
                targetProperties));
            values.push_back(attributes->CreateCustomTagAttribute(
                zhihui_cair1::kAttrSelectedFace, optional));
            values.push_back(attributes->CreateCustomDoubleAttribute(
                zhihui_cair1::kAttrExtensionLength, optional));
            values.push_back(attributes->CreateCustomDoubleAttribute(
                zhihui_cair1::kAttrGap, optional));
            values.push_back(attributes->CreateCustomDoubleAttribute(
                zhihui_cair1::kAttrBendRadius, optional));
            values.push_back(attributes->CreateCustomDoubleAttribute(
                zhihui_cair1::kAttrPickX, optional));
            values.push_back(attributes->CreateCustomDoubleAttribute(
                zhihui_cair1::kAttrPickY, optional));
            values.push_back(attributes->CreateCustomDoubleAttribute(
                zhihui_cair1::kAttrPickZ, optional));
            data = workPart->Features()
                       ->CustomFeatureDataCollection()
                       ->CreateData(featureClass_, values);
            data->CustomTagAttributeByName(
                    zhihui_cair1::kAttrTargetBody)
                ->SetValue(targetBody);
            NXOpen::Face* selectedFace = CachedFace();
            if (selectedFace != nullptr)
            {
                data->CustomTagAttributeByName(
                        zhihui_cair1::kAttrSelectedFace)
                    ->SetValue(selectedFace);
            }
        }
        else
        {
            data = editedFeature_->FeatureData();
        }
        data->CustomDoubleAttributeByName(
                zhihui_cair1::kAttrExtensionLength)
            ->SetValue(ExtensionLength());
        data->CustomDoubleAttributeByName(zhihui_cair1::kAttrGap)
            ->SetValue(Gap());
        data->CustomDoubleAttributeByName(zhihui_cair1::kAttrBendRadius)
            ->SetValue(BendRadius());
        try
        {
            data->CustomDoubleAttributeByName(zhihui_cair1::kAttrPickX)
                ->SetValue(selectedPickPoint_.X);
            data->CustomDoubleAttributeByName(zhihui_cair1::kAttrPickY)
                ->SetValue(selectedPickPoint_.Y);
            data->CustomDoubleAttributeByName(zhihui_cair1::kAttrPickZ)
                ->SetValue(selectedPickPoint_.Z);
        }
        catch (...)
        {
            // Older nodes do not contain these optional attributes.
        }

        buildingCustomFeature_ = true;
        builder->SetFeatureData(data);
        NXOpen::Features::Feature* committed = builder->CommitFeature();
        builder->Destroy();
        builder = nullptr;
        buildingCustomFeature_ = false;

        NXOpen::Features::CustomFeature* customFeature =
            dynamic_cast<NXOpen::Features::CustomFeature*>(committed);
        if (customFeature == nullptr)
        {
            error = "NX 未返回分割圆角1自定义特征节点。";
            return false;
        }
        customFeature->SetName(zhihui_cair1::kFeatureDisplayName);
        if (editing)
        {
            editedFeature_ = customFeature;
        }
        return true;
    }
    catch (const NXOpen::NXException& ex)
    {
        buildingCustomFeature_ = false;
        if (builder != nullptr) builder->Destroy();
        error = "创建分割圆角1节点失败：" +
                std::string(ex.Message() != nullptr
                                ? ex.Message()
                                : "NXOpen 自定义特征失败。");
    }
    catch (const std::exception& ex)
    {
        buildingCustomFeature_ = false;
        if (builder != nullptr) builder->Destroy();
        error = "创建分割圆角1节点失败：" +
                std::string(ex.what());
    }
    catch (...)
    {
        buildingCustomFeature_ = false;
        if (builder != nullptr) builder->Destroy();
        error = "创建分割圆角1节点时发生未知异常。";
    }
    return false;
}

int CaiR1Dialog::BuildCustomFeatureConstruction(
    NXOpen::Features::CustomFeaturePreUpdateEvent* event)
{
    if (event == nullptr || event->GetCustomFeature() == nullptr)
    {
        return 1;
    }

    std::vector<NXOpen::Features::ConstructionFeatureData*> construction =
        event->GetConstructionFeatures();
    if (buildingCustomFeature_ && hasPreview_ &&
        !previewFeatures_.empty())
    {
        // Editing replaces the complete old internal chain with the newly
        // previewed chain. Creating starts from an empty list.
        construction.clear();
        std::set<tag_t> seen;
        for (tag_t featureTag : previewFeatures_)
        {
            if (featureTag == NULL_TAG ||
                !seen.insert(featureTag).second ||
                UF_OBJ_ask_status(featureTag) != UF_OBJ_ALIVE)
            {
                continue;
            }
            NXOpen::Features::Feature* feature =
                dynamic_cast<NXOpen::Features::Feature*>(
                    NXOpen::NXObjectManager::Get(featureTag));
            if (feature == nullptr || feature->IsInternal())
            {
                continue;
            }
            NXOpen::Features::ConstructionFeatureData* item =
                event->CreateConstructionFeatureData(feature);
            item->SetShowInGraphicView(true);
            construction.push_back(item);
        }
    }
    if (construction.empty())
    {
        return 1;
    }

    for (NXOpen::Features::ConstructionFeatureData* item : construction)
    {
        item->SetShowInGraphicView(true);
    }
    event->SetConstructionFeatures(construction);
    if (buildingCustomFeature_ && hasPreview_)
    {
        CommitPreview();
        previewFeatures_.clear();
    }
    return 0;
}

bool CaiR1Dialog::EstimateCylinderThickness(
    NXOpen::Body* body,
    NXOpen::Face* face,
    double& thickness,
    std::string& error) const
{
    thickness = 0.0;
    if (body == nullptr || face == nullptr || !body->IsSolidBody() ||
        face->SolidFaceType() != NXOpen::Face::FaceTypeCylindrical)
    {
        error = "请选择实体上的圆柱面。";
        return false;
    }

    int selectedType = 0;
    double selectedOriginData[3] = {};
    double selectedAxisData[3] = {};
    double selectedBox[6] = {};
    double selectedRadius = 0.0;
    double selectedRadiusData = 0.0;
    int selectedNormalDirection = 1;
    if (UF_MODL_ask_face_data(
            face->Tag(), &selectedType, selectedOriginData,
            selectedAxisData, selectedBox, &selectedRadius,
            &selectedRadiusData, &selectedNormalDirection) != 0 ||
        selectedType != UF_MODL_CYLINDRICAL_FACE ||
        selectedRadius <= kTolerance)
    {
        error = "无法读取所选圆柱面的轴线和半径。";
        return false;
    }

    const NXOpen::Point3d selectedOrigin(
        selectedOriginData[0], selectedOriginData[1],
        selectedOriginData[2]);
    NXOpen::Vector3d selectedAxis(
        selectedAxisData[0], selectedAxisData[1],
        selectedAxisData[2]);
    if (!Normalize(selectedAxis))
    {
        error = "所选圆柱面的轴向无效。";
        return false;
    }

    double bestDifference = (std::numeric_limits<double>::max)();
    for (NXOpen::Face* candidate : body->GetFaces())
    {
        if (candidate == nullptr || candidate == face ||
            candidate->SolidFaceType() !=
                NXOpen::Face::FaceTypeCylindrical)
        {
            continue;
        }
        int type = 0;
        double originData[3] = {};
        double axisData[3] = {};
        double box[6] = {};
        double radius = 0.0;
        double radiusData = 0.0;
        int normalDirection = 1;
        if (UF_MODL_ask_face_data(
                candidate->Tag(), &type, originData, axisData, box,
                &radius, &radiusData, &normalDirection) != 0 ||
            type != UF_MODL_CYLINDRICAL_FACE)
        {
            continue;
        }
        NXOpen::Vector3d axis(axisData[0], axisData[1], axisData[2]);
        if (!Normalize(axis) ||
            std::fabs(Dot(axis, selectedAxis)) <
                kParallelTolerance)
        {
            continue;
        }
        const NXOpen::Point3d origin(
            originData[0], originData[1], originData[2]);
        if (Length(Cross(Subtract(origin, selectedOrigin),
                         selectedAxis)) >
            (std::max)(1.0e-3, selectedRadius * 1.0e-5))
        {
            continue;
        }
        const double difference = std::fabs(radius - selectedRadius);
        if (difference > kTolerance && difference < bestDifference)
        {
            bestDifference = difference;
        }
    }

    if (bestDifference ==
        (std::numeric_limits<double>::max)())
    {
        error = "未找到与所选圆柱面同轴的另一圆柱面，无法取得板厚。";
        return false;
    }
    thickness = bestDifference;
    return true;
}

bool CaiR1Dialog::BuildSplitCorner(
    NXOpen::Face* cylindricalFace,
    const NXOpen::Point3d& pickPoint,
    double extensionLength,
    double gap,
    double bendRadius,
    std::vector<tag_t>& createdFeatures,
    std::string& error) const
{
    NXOpen::Part* workPart = session_->Parts()->Work();
    NXOpen::Body* sourceBody =
        cylindricalFace != nullptr ? cylindricalFace->GetBody() : nullptr;
    double thickness = 0.0;
    if (workPart == nullptr || sourceBody == nullptr ||
        !EstimateCylinderThickness(sourceBody, cylindricalFace,
                                   thickness, error))
    {
        return false;
    }

    NXOpen::Edge* arcEdge = nullptr;
    double nearestDistance = (std::numeric_limits<double>::max)();
    for (NXOpen::Edge* edge : cylindricalFace->GetEdges())
    {
        if (edge == nullptr ||
            edge->SolidEdgeType() != NXOpen::Edge::EdgeTypeCircular)
        {
            continue;
        }
        const double distance = DistanceToEdge(edge, pickPoint);
        if (distance < nearestDistance)
        {
            nearestDistance = distance;
            arcEdge = edge;
        }
    }
    if (arcEdge == nullptr)
    {
        error = "所选圆柱面没有可用的圆弧边。";
        return false;
    }

    NXOpen::Point3d p1;
    NXOpen::Point3d p2;
    try
    {
        arcEdge->GetVertices(&p1, &p2);
    }
    catch (...)
    {
        error = "无法读取圆弧边的两个端点。";
        return false;
    }
    if (Distance(p2, pickPoint) < Distance(p1, pickPoint))
    {
        std::swap(p1, p2);
    }

    NXOpen::Face* arcPlane =
        AdjacentPlanarFace(arcEdge, cylindricalFace, sourceBody);
    NXOpen::Point3d arcPlanePoint;
    NXOpen::Vector3d arcPlaneOutward;
    if (arcPlane == nullptr ||
        !FacePlaneData(arcPlane, arcPlanePoint, arcPlaneOutward))
    {
        error = "圆弧边未连接到可用于判断拉伸方向的平面。";
        return false;
    }
    NXOpen::Vector3d extrusionDirection(
        -arcPlaneOutward.X, -arcPlaneOutward.Y,
        -arcPlaneOutward.Z);

    int cylinderType = 0;
    double cylinderOriginData[3] = {};
    double cylinderAxisData[3] = {};
    double cylinderBox[6] = {};
    double cylinderRadius = 0.0;
    double cylinderRadiusData = 0.0;
    int cylinderNormalDirection = 1;
    if (UF_MODL_ask_face_data(
            cylindricalFace->Tag(), &cylinderType,
            cylinderOriginData, cylinderAxisData, cylinderBox,
            &cylinderRadius, &cylinderRadiusData,
            &cylinderNormalDirection) != 0)
    {
        error = "无法读取圆柱面的内法向。";
        return false;
    }
    const NXOpen::Point3d cylinderOrigin(
        cylinderOriginData[0], cylinderOriginData[1],
        cylinderOriginData[2]);
    NXOpen::Vector3d cylinderAxis(
        cylinderAxisData[0], cylinderAxisData[1],
        cylinderAxisData[2]);
    if (!Normalize(cylinderAxis))
    {
        error = "圆柱轴向无效。";
        return false;
    }
    const NXOpen::Vector3d p1FromAxis =
        Subtract(p1, cylinderOrigin);
    const NXOpen::Point3d p1OnAxis =
        Move(cylinderOrigin, cylinderAxis,
             Dot(p1FromAxis, cylinderAxis));
    NXOpen::Vector3d cylinderInnerNormal =
        Subtract(p1, p1OnAxis);
    if (!Normalize(cylinderInnerNormal))
    {
        error = "无法在 P1 处计算圆柱径向。";
        return false;
    }

    // Do not trust UF's face-normal flag for the material side.  Probe 0.05
    // from the cylindrical face along both radial directions: the point that
    // lies inside the source body identifies the cylinder's inward normal.
    constexpr double cylinderNormalProbeDistance = 0.05;
    bool radialPlusInside = PointInsideBody(
        sourceBody,
        Move(p1, cylinderInnerNormal, cylinderNormalProbeDistance));
    bool radialMinusInside = PointInsideBody(
        sourceBody,
        Move(p1, cylinderInnerNormal, -cylinderNormalProbeDistance));

    // P1 can lie exactly on an axial boundary.  If NX classifies both probes
    // alike there, move the base point slightly to either side of that
    // boundary and repeat the same radial 0.05 containment test.
    if (radialPlusInside == radialMinusInside)
    {
        const double axialProbeDistance =
            (std::max)(0.001,
                       (std::min)(0.05, (thickness + gap) * 0.25));
        for (double axialSign : {1.0, -1.0})
        {
            const NXOpen::Point3d probeBase =
                Move(p1, extrusionDirection,
                     axialSign * axialProbeDistance);
            radialPlusInside = PointInsideBody(
                sourceBody,
                Move(probeBase, cylinderInnerNormal,
                     cylinderNormalProbeDistance));
            radialMinusInside = PointInsideBody(
                sourceBody,
                Move(probeBase, cylinderInnerNormal,
                     -cylinderNormalProbeDistance));
            if (radialPlusInside != radialMinusInside)
            {
                break;
            }
        }
    }
    if (radialPlusInside == radialMinusInside)
    {
        error = "无法用圆柱面法向偏置 0.05 判断实体内侧。";
        return false;
    }

    const double inwardRadialSign = radialPlusInside ? 1.0 : -1.0;
    cylinderInnerNormal.X *= inwardRadialSign;
    cylinderInnerNormal.Y *= inwardRadialSign;
    cylinderInnerNormal.Z *= inwardRadialSign;

    // The circular edge also lies on the adjacent planar face.  Its extrusion
    // direction must enter the source solid through that plane.  The UF face
    // normal flag is not reliable enough for this trimmed sheet-metal corner:
    // offset a point slightly toward the already-confirmed cylindrical
    // material side first, then probe both planar-normal directions.
    NXOpen::Point3d arcMiddlePoint;
    if (!EdgePointAtFraction(arcEdge, 0.5, arcMiddlePoint))
    {
        error = "无法取得圆弧边中点以判断拉伸方向。";
        return false;
    }
    const NXOpen::Vector3d middleFromAxis =
        Subtract(arcMiddlePoint, cylinderOrigin);
    const NXOpen::Point3d middleOnAxis =
        Move(cylinderOrigin, cylinderAxis,
             Dot(middleFromAxis, cylinderAxis));
    NXOpen::Vector3d middleInwardNormal =
        Subtract(arcMiddlePoint, middleOnAxis);
    if (!Normalize(middleInwardNormal))
    {
        error = "无法在圆弧边中点计算圆柱径向。";
        return false;
    }
    middleInwardNormal.X *= inwardRadialSign;
    middleInwardNormal.Y *= inwardRadialSign;
    middleInwardNormal.Z *= inwardRadialSign;

    const double radialInset =
        (std::max)(0.001,
                   (std::min)(cylinderNormalProbeDistance,
                              thickness * 0.25));
    const NXOpen::Point3d planarProbeBase =
        Move(arcMiddlePoint, middleInwardNormal, radialInset);
    const double planarProbeDistance =
        (std::max)(0.001,
                   (std::min)(0.05, thickness * 0.25));
    const bool planarPlusInside = PointInsideBody(
        sourceBody,
        Move(planarProbeBase, arcPlaneOutward,
             planarProbeDistance));
    const bool planarMinusInside = PointInsideBody(
        sourceBody,
        Move(planarProbeBase, arcPlaneOutward,
             -planarProbeDistance));
    if (planarPlusInside == planarMinusInside)
    {
        error = "无法用圆弧边连接平面的法向判断拉伸内侧。";
        return false;
    }
    const double planarInwardSign = planarPlusInside ? 1.0 : -1.0;
    extrusionDirection = NXOpen::Vector3d(
        arcPlaneOutward.X * planarInwardSign,
        arcPlaneOutward.Y * planarInwardSign,
        arcPlaneOutward.Z * planarInwardSign);

    const double extrusionDistance = thickness + gap;
    const double offsetDistance = thickness + bendRadius;
    if (extrusionDistance <= kTolerance ||
        offsetDistance <= kTolerance)
    {
        error = "板厚、间隙和折弯R组合后必须大于零。";
        return false;
    }

    NXOpen::Body* toolBody = nullptr;
    NXOpen::Features::Feature* extrudeFeature = nullptr;
    NXOpen::Body* secondToolBody = nullptr;
    NXOpen::Features::Feature* secondExtrudeFeature = nullptr;
    std::string stage = "创建圆弧边拉伸体";

    auto createExtrude =
        [&](double signedOffset, double startDistance,
            NXOpen::Features::Feature*& feature,
            NXOpen::Body*& body) -> bool
    {
        NXOpen::Features::ExtrudeBuilder* builder = nullptr;
        try
        {
            builder =
                workPart->Features()->CreateExtrudeBuilder(nullptr);
            NXOpen::Section* section =
                workPart->Sections()->CreateSection(
                    0.000095, 0.0001, 0.5);
            builder->SetSection(section);
            builder->FeatureOptions()->SetBodyType(
                NXOpen::GeometricUtilities::FeatureOptions::
                    BodyStyleSolid);
            builder->BooleanOperation()->SetType(
                NXOpen::GeometricUtilities::BooleanOperation::
                    BooleanTypeCreate);
            builder->SmartVolumeProfile()->
                SetOpenProfileSmartVolumeOption(false);
            builder->SmartVolumeProfile()->SetCloseProfileRule(
                NXOpen::GeometricUtilities::
                    SmartVolumeProfileBuilder::CloseProfileRuleTypeFci);
            builder->SetDistanceTolerance(0.0001);

            section->SetDistanceTolerance(0.0001);
            section->SetChainingTolerance(0.000095);
            section->SetAllowedEntityTypes(
                NXOpen::Section::AllowTypesOnlyCurves);
            section->AllowSelfIntersection(true);
            section->AllowDegenerateCurves(false);

            NXOpen::SelectionIntentRuleOptions* options =
                workPart->ScRuleFactory()->CreateRuleOptions();
            options->SetSelectedFromInactive(false);
            NXOpen::EdgeDumbRule* rule =
                workPart->ScRuleFactory()->CreateRuleEdgeDumb(
                    std::vector<NXOpen::Edge*>{arcEdge}, options);
            delete options;
            const NXOpen::Point3d helpPoint(
                (p1.X + p2.X) * 0.5,
                (p1.Y + p2.Y) * 0.5,
                (p1.Z + p2.Z) * 0.5);
            section->AddToSection(
                std::vector<NXOpen::SelectionIntentRule*>{rule},
                arcEdge, nullptr, nullptr, helpPoint,
                NXOpen::Section::ModeCreate, false);

            NXOpen::Direction* direction =
                workPart->Directions()->CreateDirection(
                    helpPoint, extrusionDirection,
                    NXOpen::SmartObject::UpdateOptionWithinModeling);
            builder->SetDirection(direction);
            builder->Limits()->StartExtend()->Value()->
                SetFormula(Number(startDistance).c_str());
            builder->Limits()->EndExtend()->Value()->
                SetFormula(Number(extrusionDistance).c_str());
            builder->Offset()->SetOption(
                NXOpen::GeometricUtilities::TypeNonsymmetricOffset);
            builder->Offset()->StartOffset()->SetFormula("0");
            builder->Offset()->EndOffset()->SetFormula(
                Number(signedOffset).c_str());

            feature = builder->CommitFeature();
            builder->Destroy();
            builder = nullptr;
            if (feature == nullptr || feature->GetBodies().empty())
            {
                return false;
            }
            body = feature->GetBodies().front();
            return body != nullptr;
        }
        catch (...)
        {
            if (builder != nullptr)
            {
                builder->Destroy();
            }
            return false;
        }
    };

    try
    {
        // NX decides an open section's offset sign from curve orientation.
        // Build with + first, then test a point 0.05 toward the confirmed
        // cylinder material side at the arc midpoint.  This avoids using a
        // body's bounding-box center, which is unreliable on curved sections.
        const NXOpen::Session::UndoMarkId signMark =
            session_->SetUndoMark(
                NXOpen::Session::MarkVisibilityInvisible,
                "分割圆角1偏置方向判断");
        double resolvedOffset = offsetDistance;
        bool created =
            createExtrude(offsetDistance, 0.0,
                          extrudeFeature, toolBody);
        const auto toolContainsExpectedInwardProbe =
            [&](NXOpen::Body* candidateBody) -> bool
        {
            if (candidateBody == nullptr)
            {
                return false;
            }
            const double offsetProbeDistance =
                (std::min)(cylinderNormalProbeDistance,
                           offsetDistance * 0.5);
            NXOpen::Point3d offsetProbe =
                Move(arcMiddlePoint, extrusionDirection,
                     extrusionDistance * 0.5);
            offsetProbe =
                Move(offsetProbe, middleInwardNormal,
                     offsetProbeDistance);
            return PointInsideBody(candidateBody, offsetProbe);
        };
        const bool positiveIsInward =
            created && toolContainsExpectedInwardProbe(toolBody);
        if (!created || !positiveIsInward)
        {
            session_->UndoToMark(signMark,
                                 "分割圆角1偏置方向判断");
            extrudeFeature = nullptr;
            toolBody = nullptr;
            resolvedOffset = -offsetDistance;
            if (!createExtrude(resolvedOffset, 0.0,
                               extrudeFeature, toolBody))
            {
                throw std::runtime_error(
                    "正、负偏置均未能创建拉伸实体。");
            }
            if (!toolContainsExpectedInwardProbe(toolBody))
            {
                throw std::runtime_error(
                    "正、负偏置均未落在圆柱面的实体内侧。");
            }
        }
        if (session_->DoesUndoMarkExist(
                signMark, "分割圆角1偏置方向判断"))
        {
            session_->DeleteUndoMark(
                signMark, "分割圆角1偏置方向判断");
        }
        createdFeatures.push_back(extrudeFeature->Tag());

        if (gap > kTolerance)
        {
            stage = "创建板厚起始的第二拉伸体";
            if (!createExtrude(resolvedOffset, thickness,
                               secondExtrudeFeature,
                               secondToolBody))
            {
                throw std::runtime_error(
                    "未能创建起始值为板厚的第二拉伸体。");
            }
            createdFeatures.push_back(secondExtrudeFeature->Tag());
        }

        if (gap > kTolerance)
        {
            stage = "查找 P2 圆柱直边及相切直边";
            NXOpen::Edge* p2CylinderStraightEdge = nullptr;
            NXOpen::Face* p2EdgePlane = nullptr;
            for (NXOpen::Edge* edge : cylindricalFace->GetEdges())
            {
                if (edge == nullptr ||
                    edge->SolidEdgeType() !=
                        NXOpen::Edge::EdgeTypeLinear ||
                    !EdgeTouchesPoint(edge, p2))
                {
                    continue;
                }
                NXOpen::Face* plane =
                    AdjacentPlanarFace(edge, cylindricalFace,
                                       sourceBody);
                if (plane != nullptr)
                {
                    p2CylinderStraightEdge = edge;
                    p2EdgePlane = plane;
                    break;
                }
            }
            if (p2CylinderStraightEdge == nullptr ||
                p2EdgePlane == nullptr)
            {
                throw std::runtime_error(
                    "未找到 P2 侧圆柱面直边及其连接平面。");
            }

            NXOpen::Edge* p2TangentEdge = nullptr;
            NXOpen::Point3d tangentFarPoint;
            double tangentLength = 0.0;
            for (NXOpen::Edge* edge : p2EdgePlane->GetEdges())
            {
                if (edge == nullptr ||
                    edge == p2CylinderStraightEdge ||
                    edge->SolidEdgeType() !=
                        NXOpen::Edge::EdgeTypeLinear ||
                    !EdgeTouchesPoint(edge, p2))
                {
                    continue;
                }
                NXOpen::Point3d first;
                NXOpen::Point3d second;
                try
                {
                    edge->GetVertices(&first, &second);
                }
                catch (...)
                {
                    continue;
                }
                const NXOpen::Point3d farPoint =
                    Distance(first, p2) <= Distance(second, p2)
                        ? second
                        : first;
                const double length = Distance(farPoint, p2);
                if (length > tangentLength)
                {
                    tangentLength = length;
                    tangentFarPoint = farPoint;
                    p2TangentEdge = edge;
                }
            }
            if (p2TangentEdge == nullptr)
            {
                throw std::runtime_error(
                    "未找到从 P2 指向远端的相切直边。");
            }
            NXOpen::Vector3d p2TangentDirection =
                Subtract(tangentFarPoint, p2);
            if (!Normalize(p2TangentDirection))
            {
                throw std::runtime_error(
                    "P2 相切直边方向无效。");
            }

            NXOpen::Point3d p2PlanePoint;
            NXOpen::Vector3d p2PlaneOutward;
            if (!FacePlaneData(p2EdgePlane, p2PlanePoint,
                               p2PlaneOutward))
            {
                throw std::runtime_error(
                    "无法读取 P2 拉伸边平面的法向。");
            }
            NXOpen::Vector3d p2PlaneInward(
                -p2PlaneOutward.X, -p2PlaneOutward.Y,
                -p2PlaneOutward.Z);
            const NXOpen::Point3d planeProbeOrigin =
                FaceBoxCenter(p2EdgePlane);
            const double planeProbeDistance =
                (std::max)(1.0e-3, thickness * 0.1);
            const bool planePlusInside = PointInsideBody(
                sourceBody,
                Move(planeProbeOrigin, p2PlaneOutward,
                     planeProbeDistance));
            const bool planeMinusInside = PointInsideBody(
                sourceBody,
                Move(planeProbeOrigin, p2PlaneOutward,
                     -planeProbeDistance));
            if (planePlusInside != planeMinusInside)
            {
                const double inwardSign =
                    planePlusInside ? 1.0 : -1.0;
                p2PlaneInward = NXOpen::Vector3d(
                    p2PlaneOutward.X * inwardSign,
                    p2PlaneOutward.Y * inwardSign,
                    p2PlaneOutward.Z * inwardSign);
            }

            NXOpen::Point3d cylinderEdgeFirst;
            NXOpen::Point3d cylinderEdgeSecond;
            p2CylinderStraightEdge->GetVertices(
                &cylinderEdgeFirst, &cylinderEdgeSecond);
            const NXOpen::Point3d cylinderEdgeMidpoint(
                (cylinderEdgeFirst.X + cylinderEdgeSecond.X) * 0.5,
                (cylinderEdgeFirst.Y + cylinderEdgeSecond.Y) * 0.5,
                (cylinderEdgeFirst.Z + cylinderEdgeSecond.Z) * 0.5);

            const auto createThirdExtrude =
                [&](double signedOffset,
                    NXOpen::Features::Feature*& feature,
                    NXOpen::Body*& body) -> bool
            {
                NXOpen::Features::ExtrudeBuilder* builder = nullptr;
                try
                {
                    builder = workPart->Features()->
                        CreateExtrudeBuilder(nullptr);
                    NXOpen::Section* section =
                        workPart->Sections()->CreateSection(
                            0.000095, 0.0001, 0.5);
                    builder->SetSection(section);
                    builder->FeatureOptions()->SetBodyType(
                        NXOpen::GeometricUtilities::FeatureOptions::
                            BodyStyleSolid);
                    builder->BooleanOperation()->SetType(
                        NXOpen::GeometricUtilities::BooleanOperation::
                            BooleanTypeCreate);
                    builder->SmartVolumeProfile()->
                        SetOpenProfileSmartVolumeOption(false);
                    builder->SmartVolumeProfile()->SetCloseProfileRule(
                        NXOpen::GeometricUtilities::
                            SmartVolumeProfileBuilder::
                                CloseProfileRuleTypeFci);
                    builder->SetDistanceTolerance(0.0001);

                    section->SetDistanceTolerance(0.0001);
                    section->SetChainingTolerance(0.000095);
                    section->SetAllowedEntityTypes(
                        NXOpen::Section::AllowTypesOnlyCurves);
                    section->AllowSelfIntersection(true);
                    section->AllowDegenerateCurves(false);

                    NXOpen::SelectionIntentRuleOptions* options =
                        workPart->ScRuleFactory()->
                            CreateRuleOptions();
                    options->SetSelectedFromInactive(false);
                    NXOpen::EdgeDumbRule* rule =
                        workPart->ScRuleFactory()->
                            CreateRuleEdgeDumb(
                                std::vector<NXOpen::Edge*>{
                                    p2CylinderStraightEdge},
                                options);
                    delete options;
                    section->AddToSection(
                        std::vector<NXOpen::SelectionIntentRule*>{
                            rule},
                        p2CylinderStraightEdge, nullptr, nullptr,
                        cylinderEdgeMidpoint,
                        NXOpen::Section::ModeCreate, false);

                    NXOpen::Direction* direction =
                        workPart->Directions()->CreateDirection(
                            p2, p2TangentDirection,
                            NXOpen::SmartObject::
                                UpdateOptionWithinModeling);
                    builder->SetDirection(direction);
                    builder->Limits()->StartExtend()->Value()->
                        SetFormula(Number(extensionLength).c_str());
                    builder->Limits()->EndExtend()->Value()->
                        SetFormula(
                            Number(extensionLength + gap).c_str());
                    builder->Offset()->SetOption(
                        NXOpen::GeometricUtilities::
                            TypeNonsymmetricOffset);
                    builder->Offset()->StartOffset()->
                        SetFormula("0");
                    builder->Offset()->EndOffset()->SetFormula(
                        Number(signedOffset).c_str());

                    feature = builder->CommitFeature();
                    builder->Destroy();
                    builder = nullptr;
                    if (feature == nullptr ||
                        feature->GetBodies().empty())
                    {
                        return false;
                    }
                    body = feature->GetBodies().front();
                    return body != nullptr;
                }
                catch (...)
                {
                    if (builder != nullptr)
                    {
                        builder->Destroy();
                    }
                    return false;
                }
            };

            stage = "创建 P2 直边第三拉伸体";
            NXOpen::Features::Feature* thirdExtrudeFeature = nullptr;
            NXOpen::Body* thirdToolBody = nullptr;
            const NXOpen::Session::UndoMarkId thirdSignMark =
                session_->SetUndoMark(
                    NXOpen::Session::MarkVisibilityInvisible,
                    "第三拉伸体偏置方向判断");
            bool thirdCreated = createThirdExtrude(
                offsetDistance, thirdExtrudeFeature,
                thirdToolBody);
            bool thirdPositiveIsInward = false;
            if (thirdCreated && thirdToolBody != nullptr)
            {
                thirdPositiveIsInward =
                    Dot(Subtract(BodyBoxCenter(thirdToolBody),
                                 cylinderEdgeMidpoint),
                        p2PlaneInward) > 0.0;
            }
            if (!thirdCreated || !thirdPositiveIsInward)
            {
                session_->UndoToMark(
                    thirdSignMark,
                    "第三拉伸体偏置方向判断");
                thirdExtrudeFeature = nullptr;
                thirdToolBody = nullptr;
                if (!createThirdExtrude(
                        -offsetDistance, thirdExtrudeFeature,
                        thirdToolBody))
                {
                    throw std::runtime_error(
                        "第三拉伸体正、负偏置均创建失败。");
                }
            }
            if (session_->DoesUndoMarkExist(
                    thirdSignMark,
                    "第三拉伸体偏置方向判断"))
            {
                session_->DeleteUndoMark(
                    thirdSignMark,
                    "第三拉伸体偏置方向判断");
            }
            createdFeatures.push_back(
                thirdExtrudeFeature->Tag());
        }

        const auto findArcEndFace =
            [&](NXOpen::Body* targetBody,
                const NXOpen::Point3d& endpoint) -> NXOpen::Face*
        {
            if (targetBody == nullptr)
            {
                return nullptr;
            }
            NXOpen::Face* endFace = nullptr;
            double bestDistance =
                (std::numeric_limits<double>::max)();
            for (NXOpen::Face* candidate : targetBody->GetFaces())
            {
                NXOpen::Point3d planePoint;
                NXOpen::Vector3d planeNormal;
                if (candidate == nullptr ||
                    !FacePlaneData(candidate, planePoint, planeNormal) ||
                    std::fabs(Dot(planeNormal,
                                  extrusionDirection)) > 0.01)
                {
                    continue;
                }
                const double distance = std::fabs(
                    Dot(Subtract(endpoint, planePoint), planeNormal));
                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    endFace = candidate;
                }
            }
            return bestDistance <= 0.01 ? endFace : nullptr;
        };

        const auto pullFaceOutward =
            [&](NXOpen::Body* targetBody, NXOpen::Face* endFace,
                double distance,
                const char* faceName)
        {
            if (endFace == nullptr)
            {
                throw std::runtime_error(
                    std::string("未找到 ") + faceName +
                    " 处的矩形端面。");
            }
            if (distance <= kTolerance)
            {
                return;
            }

            NXOpen::Point3d pullFacePoint;
            NXOpen::Vector3d pullOutwardNormal;
            if (!FacePlaneData(endFace, pullFacePoint,
                               pullOutwardNormal))
            {
                throw std::runtime_error(
                    std::string("无法读取 ") + faceName +
                    " 矩形端面的外法向。");
            }

            pullFacePoint = FaceBoxCenter(endFace);
            const double normalProbe =
                (std::max)(0.0001,
                           (std::min)(offsetDistance,
                                      extrusionDistance) * 0.05);
            const NXOpen::Point3d positiveProbe(
                pullFacePoint.X +
                    pullOutwardNormal.X * normalProbe,
                pullFacePoint.Y +
                    pullOutwardNormal.Y * normalProbe,
                pullFacePoint.Z +
                    pullOutwardNormal.Z * normalProbe);
            const NXOpen::Point3d negativeProbe(
                pullFacePoint.X -
                    pullOutwardNormal.X * normalProbe,
                pullFacePoint.Y -
                    pullOutwardNormal.Y * normalProbe,
                pullFacePoint.Z -
                    pullOutwardNormal.Z * normalProbe);
            const bool positiveInside =
                PointInsideBody(targetBody, positiveProbe);
            const bool negativeInside =
                PointInsideBody(targetBody, negativeProbe);
            if (positiveInside && !negativeInside)
            {
                pullOutwardNormal = NXOpen::Vector3d(
                    -pullOutwardNormal.X,
                    -pullOutwardNormal.Y,
                    -pullOutwardNormal.Z);
            }

            NXOpen::Direction* pullDirection =
                workPart->Directions()->CreateDirection(
                    pullFacePoint, pullOutwardNormal,
                    NXOpen::SmartObject::UpdateOptionWithinModeling);
            NXOpen::Features::PullFaceBuilder* pull =
                workPart->Features()->CreatePullFaceBuilder(nullptr);
            pull->Motion()->SetOption(
                NXOpen::GeometricUtilities::ModlMotion::OptionsDistance);
            pull->Motion()->SetDistanceVector(pullDirection);
            pull->Motion()->DistanceValue()->SetFormula(
                Number(distance).c_str());
            NXOpen::SelectionIntentRuleOptions* options =
                workPart->ScRuleFactory()->CreateRuleOptions();
            options->SetSelectedFromInactive(false);
            NXOpen::FaceDumbRule* rule =
                workPart->ScRuleFactory()->CreateRuleFaceDumb(
                    std::vector<NXOpen::Face*>{endFace}, options);
            delete options;
            pull->FaceToPull()->ReplaceRules(
                std::vector<NXOpen::SelectionIntentRule*>{rule},
                false);
            NXOpen::NXObject* pullObject = pull->Commit();
            const std::vector<NXOpen::NXObject*> committedObjects =
                pull->GetCommittedObjects();
            pull->Destroy();

            NXOpen::Features::Feature* pullFeature =
                dynamic_cast<NXOpen::Features::Feature*>(pullObject);
            if (pullFeature == nullptr)
            {
                for (NXOpen::NXObject* committed : committedObjects)
                {
                    pullFeature =
                        dynamic_cast<NXOpen::Features::Feature*>(committed);
                    if (pullFeature != nullptr)
                    {
                        break;
                    }
                }
            }
            if (pullFeature != nullptr)
            {
                const std::vector<NXOpen::Body*> movedBodies =
                    pullFeature->GetBodies();
                (void)movedBodies;
                createdFeatures.push_back(pullFeature->Tag());
            }
        };

        if (secondToolBody != nullptr &&
            extensionLength > kTolerance)
        {
            stage = "拉动第二拉伸体 P2 圆弧切边";
            pullFaceOutward(
                secondToolBody,
                findArcEndFace(secondToolBody, p2),
                extensionLength, "第二拉伸体 P2");
        }

        stage = "拉动 P1 圆弧切边";
        pullFaceOutward(
            toolBody, findArcEndFace(toolBody, p1),
            gap, "第一拉伸体 P1");

        stage = "查找 P1 直边连接平面";
        NXOpen::Edge* p1StraightEdge = nullptr;
        NXOpen::Face* p1StraightPlane = nullptr;
        for (NXOpen::Edge* edge : cylindricalFace->GetEdges())
        {
            if (edge == nullptr || edge == arcEdge ||
                edge->SolidEdgeType() ==
                    NXOpen::Edge::EdgeTypeCircular ||
                !EdgeTouchesPoint(edge, p1))
            {
                continue;
            }
            NXOpen::Face* plane =
                AdjacentPlanarFace(edge, cylindricalFace, sourceBody);
            if (plane != nullptr)
            {
                p1StraightEdge = edge;
                p1StraightPlane = plane;
                break;
            }
        }
        (void)p1StraightEdge;
        NXOpen::Point3d straightPlanePoint;
        NXOpen::Vector3d straightPlaneOutward;
        if (p1StraightPlane == nullptr ||
            !FacePlaneData(p1StraightPlane, straightPlanePoint,
                           straightPlaneOutward))
        {
            throw std::runtime_error(
                "未找到 P1 处圆柱直边连接的平面。");
        }
        NXOpen::Vector3d straightPlaneInward(
            -straightPlaneOutward.X, -straightPlaneOutward.Y,
            -straightPlaneOutward.Z);

        // Confirm the material-side direction from the source solid.  This
        // avoids relying only on the planar face's normal_direction flag at
        // a trimmed cylindrical corner.
        const NXOpen::Point3d probeOrigin =
            FaceBoxCenter(p1StraightPlane);
        const double probeDistance =
            (std::max)(1.0e-3, thickness * 0.1);
        const bool plusIsInside = PointInsideBody(
            sourceBody,
            Move(probeOrigin, straightPlaneOutward, probeDistance));
        const bool minusIsInside = PointInsideBody(
            sourceBody,
            Move(probeOrigin, straightPlaneOutward, -probeDistance));
        if (plusIsInside == minusIsInside)
        {
            throw std::runtime_error(
                "无法用 P1 连接平面正、负法向取样确定实体内侧。");
        }
        const double sign = plusIsInside ? 1.0 : -1.0;
        straightPlaneInward =
            NXOpen::Vector3d(straightPlaneOutward.X * sign,
                             straightPlaneOutward.Y * sign,
                             straightPlaneOutward.Z * sign);
        straightPlaneOutward =
            NXOpen::Vector3d(-straightPlaneInward.X,
                             -straightPlaneInward.Y,
                             -straightPlaneInward.Z);

        const NXOpen::Point3d trimPlaneOrigin =
            Move(p1, straightPlaneInward,
                 offsetDistance);

        // An offset equal to the extrusion width can land exactly on the
        // tool body's boundary.  NX then reports "Target body completely
        // inside tool body" because the trim is a geometric no-op.  Only
        // create Trim Body when the infinite plane genuinely crosses the
        // solid on both sides.
        const double trimIntersectionTolerance =
            (std::max)(1.0e-5, offsetDistance * 1.0e-7);
        if (!PlaneCutsBody(toolBody, trimPlaneOrigin,
                           straightPlaneOutward,
                           trimIntersectionTolerance))
        {
            return true;
        }

        NXOpen::Plane* trimPlane =
            workPart->Planes()->CreatePlane(
                trimPlaneOrigin, straightPlaneOutward,
                NXOpen::SmartObject::UpdateOptionWithinModeling);

        stage = "用偏置平面修剪拉伸体";

        NXOpen::Features::TrimBody2Builder* trim =
            workPart->Features()->CreateTrimBody2Builder(nullptr);
        NXOpen::SelectionIntentRuleOptions* targetOptions =
            workPart->ScRuleFactory()->CreateRuleOptions();
        targetOptions->SetSelectedFromInactive(false);
        NXOpen::BodyDumbRule* targetRule =
            workPart->ScRuleFactory()->CreateRuleBodyDumb(
                std::vector<NXOpen::Body*>{toolBody}, true,
                targetOptions);
        delete targetOptions;
        NXOpen::ScCollector* targetCollector =
            workPart->ScCollectors()->CreateCollector();
        targetCollector->ReplaceRules(
            std::vector<NXOpen::SelectionIntentRule*>{targetRule},
            false);
        trim->SetTargetBodyCollector(targetCollector);
        trim->BooleanTool()->SetToolOption(
            NXOpen::GeometricUtilities::BooleanToolBuilder::
                BooleanToolTypeNewPlane);
        trim->BooleanTool()->FacePlaneTool()->SetToolPlane(trimPlane);

        // The required trim direction is the P1 adjacent plane's confirmed
        // inward normal.  Validate the actual tool-plane normal before any
        // flip, flip only when needed, then validate it again.
        NXOpen::Plane* configuredTrimPlane =
            trim->BooleanTool()->FacePlaneTool()->ToolPlane();
        NXOpen::Vector3d trimDirection = configuredTrimPlane->Normal();
        if (!Normalize(trimDirection))
        {
            trim->Destroy();
            throw std::runtime_error("无法获取修剪体默认方向。");
        }
        const double directionDotBefore =
            Dot(trimDirection, straightPlaneInward);
        bool flippedToolPlane = false;
        if (directionDotBefore < 1.0 - 1.0e-6)
        {
            configuredTrimPlane->SetFlip(!configuredTrimPlane->Flip());
            configuredTrimPlane->Evaluate();
            flippedToolPlane = true;
        }

        trimDirection = configuredTrimPlane->Normal();
        if (!Normalize(trimDirection))
        {
            trim->Destroy();
            throw std::runtime_error("翻转后无法获取修剪平面方向。");
        }
        const double directionDotAfter =
            Dot(trimDirection, straightPlaneInward);
        if (directionDotAfter < 1.0 - 1.0e-6)
        {
            trim->Destroy();
            throw std::runtime_error(
                "修剪平面方向未能与 P1 连接平面内法向一致。");
        }

        trim->BooleanTool()->SetReverseDirection(false);
        trim->SetTolerance(0.0001);

        {
            std::ostringstream directionLog;
            directionLog << "CaiR1 trim direction validation: dotBefore="
                         << directionDotBefore << ", flipped="
                         << (flippedToolPlane ? "true" : "false")
                         << ", dotAfter=" << directionDotAfter;
            session_->LogFile()->WriteLine(directionLog.str().c_str());
        }

        NXOpen::Features::Feature* trimFeature = trim->CommitFeature();
        trim->Destroy();
        if (trimFeature == nullptr || trimFeature->GetBodies().empty())
        {
            throw std::runtime_error("NX 未返回修剪后的拉伸实体。");
        }
        createdFeatures.push_back(trimFeature->Tag());
        return true;

#if 0

        // Find an interior point of the current tool body on the requested
        // keep side (the original plane's outward-normal side).  Sampling the
        // known arc/extrusion/offset volume is more reliable than inferring a
        // side from a bounding-box center.
        NXOpen::Point3d trimTargetProbe;
        bool trimKeepProbeFound = false;
        double bestKeepSideDistance =
            -(std::numeric_limits<double>::max)();
        const double trimProbeRadialDistance =
            (std::min)(0.05, offsetDistance * 0.25);
        for (double fraction :
             {0.01, 0.025, 0.05, 0.10, 0.20, 0.30,
              0.70, 0.80, 0.90, 0.95, 0.975, 0.99})
        {
            NXOpen::Point3d arcProbePoint;
            if (!EdgePointAtFraction(arcEdge, fraction,
                                     arcProbePoint))
            {
                continue;
            }
            const NXOpen::Vector3d probeFromAxis =
                Subtract(arcProbePoint, cylinderOrigin);
            const NXOpen::Point3d probeOnAxis =
                Move(cylinderOrigin, cylinderAxis,
                     Dot(probeFromAxis, cylinderAxis));
            NXOpen::Vector3d probeInwardNormal =
                Subtract(arcProbePoint, probeOnAxis);
            if (!Normalize(probeInwardNormal))
            {
                continue;
            }
            probeInwardNormal.X *= inwardRadialSign;
            probeInwardNormal.Y *= inwardRadialSign;
            probeInwardNormal.Z *= inwardRadialSign;

            NXOpen::Point3d candidateProbe =
                Move(arcProbePoint, extrusionDirection,
                     extrusionDistance * 0.5);
            candidateProbe =
                Move(candidateProbe, probeInwardNormal,
                     trimProbeRadialDistance);
            const double keepSideDistance =
                Dot(Subtract(candidateProbe, trimPlaneOrigin),
                    straightPlaneOutward);
            if (keepSideDistance > bestKeepSideDistance &&
                PointInsideBody(toolBody, candidateProbe))
            {
                trimTargetProbe = candidateProbe;
                trimKeepProbeFound = true;
                bestKeepSideDistance = keepSideDistance;
            }
        }
        const double trimProbeSideMargin =
            (std::max)(0.01, trimIntersectionTolerance * 10.0);
        if (!trimKeepProbeFound ||
            bestKeepSideDistance <= trimProbeSideMargin)
        {
            throw std::runtime_error(
                "未能在远离修剪平面的外法向侧找到拉伸体内部取样点。");
        }

        // Trial both keep directions and compare the actual resulting solid
        // volumes.  The final feature always keeps the smaller side, so this
        // is independent of face-normal and ReverseDirection conventions.
        const auto resolveCurrentTrimTarget = [&]() -> NXOpen::Body*
        {
            for (NXOpen::BodyCollection::iterator iterator =
                     workPart->Bodies()->begin();
                 iterator != workPart->Bodies()->end(); ++iterator)
            {
                NXOpen::Body* candidate = *iterator;
                if (candidate != nullptr && candidate != sourceBody &&
                    candidate->Tag() != NULL_TAG &&
                    UF_OBJ_ask_status(candidate->Tag()) == UF_OBJ_ALIVE &&
                    PointInsideBody(candidate, trimTargetProbe))
                {
                    return candidate;
                }
            }
            return nullptr;
        };

        const auto createTrim =
            [&](bool reverseDirection,
                NXOpen::Features::Feature*& feature,
                NXOpen::Body*& resultBody) -> bool
        {
            NXOpen::Features::TrimBody2Builder* trim = nullptr;
            try
            {
                NXOpen::Body* currentTarget =
                    resolveCurrentTrimTarget();
                if (currentTarget == nullptr)
                {
                    return false;
                }
                trim = workPart->Features()->
                    CreateTrimBody2Builder(nullptr);
                NXOpen::SelectionIntentRuleOptions* targetOptions =
                    workPart->ScRuleFactory()->CreateRuleOptions();
                targetOptions->SetSelectedFromInactive(false);
                NXOpen::BodyDumbRule* targetRule =
                    workPart->ScRuleFactory()->CreateRuleBodyDumb(
                        std::vector<NXOpen::Body*>{currentTarget}, true,
                        targetOptions);
                delete targetOptions;
                NXOpen::ScCollector* targetCollector =
                    workPart->ScCollectors()->CreateCollector();
                targetCollector->ReplaceRules(
                    std::vector<NXOpen::SelectionIntentRule*>{
                        targetRule},
                    false);
                trim->SetTargetBodyCollector(targetCollector);
                trim->BooleanTool()->SetToolOption(
                    NXOpen::GeometricUtilities::BooleanToolBuilder::
                        BooleanToolTypeNewPlane);
                NXOpen::Plane* directionPlane = trimPlane;
                if (reverseDirection)
                {
                    const NXOpen::Vector3d reversedPlaneNormal(
                        -straightPlaneOutward.X,
                        -straightPlaneOutward.Y,
                        -straightPlaneOutward.Z);
                    directionPlane = workPart->Planes()->CreatePlane(
                        trimPlaneOrigin, reversedPlaneNormal,
                        NXOpen::SmartObject::UpdateOptionWithinModeling);
                }
                trim->BooleanTool()->FacePlaneTool()->
                    SetToolPlane(directionPlane);
                // NX 2412 can return the same result for both values of
                // BooleanTool.ReverseDirection with an on-the-fly plane.
                // Keep this false and reverse the actual plane normal above.
                trim->BooleanTool()->SetReverseDirection(false);
                trim->SetTolerance(0.001);
                feature = trim->CommitFeature();
                trim->Destroy();
                trim = nullptr;
                if (feature == nullptr || feature->GetBodies().empty())
                {
                    return false;
                }
                // Read the body produced by the trim feature.  currentTarget
                // may remain alive as a historical body and reports the same
                // pre-trim volume for both keep directions.
                resultBody = feature->GetBodies().front();
                return resultBody != nullptr &&
                       resultBody->Tag() != NULL_TAG &&
                       UF_OBJ_ask_status(resultBody->Tag()) == UF_OBJ_ALIVE;
            }
            catch (...)
            {
                if (trim != nullptr)
                {
                    trim->Destroy();
                }
                return false;
            }
        };

        double defaultVolume = 0.0;
        double reverseVolume = 0.0;
        bool defaultVolumeValid = false;
        bool reverseVolumeValid = false;
        const auto measureTrimVolume =
            [&](bool reverseDirection, double& volume) -> bool
        {
            const NXOpen::Session::UndoMarkId trialMark =
                session_->SetUndoMark(
                    NXOpen::Session::MarkVisibilityInvisible,
                    reverseDirection ? "修剪体积比较-反向"
                                     : "修剪体积比较-正向");
            NXOpen::Features::Feature* trialFeature = nullptr;
            NXOpen::Body* trialBody = nullptr;
            const bool valid =
                createTrim(reverseDirection, trialFeature, trialBody) &&
                CopiedBodyVolume(trialBody, volume);
            session_->UndoToMark(
                trialMark,
                reverseDirection ? "修剪体积比较-反向"
                                 : "修剪体积比较-正向");
            return valid;
        };
        defaultVolumeValid = measureTrimVolume(false, defaultVolume);
        reverseVolumeValid = measureTrimVolume(true, reverseVolume);

        if (!defaultVolumeValid && !reverseVolumeValid)
        {
            throw std::runtime_error(
                "正、反两个修剪方向均未能取得有效体积。");
        }
        const bool keepReverseDirection =
            reverseVolumeValid &&
            (!defaultVolumeValid || reverseVolume < defaultVolume);
        {
            std::ostringstream volumeLog;
            volumeLog << "CaiR1 trim volume compare: default=";
            if (defaultVolumeValid)
            {
                volumeLog << defaultVolume;
            }
            else
            {
                volumeLog << "invalid";
            }
            volumeLog << ", reverse=";
            if (reverseVolumeValid)
            {
                volumeLog << reverseVolume;
            }
            else
            {
                volumeLog << "invalid";
            }
            volumeLog << ", keepReverse="
                      << (keepReverseDirection ? "true" : "false");
            session_->LogFile()->WriteLine(volumeLog.str().c_str());
        }
        NXOpen::Features::Feature* trimFeature = nullptr;
        NXOpen::Body* trimmedBody = nullptr;
        if (!createTrim(keepReverseDirection,
                        trimFeature, trimmedBody))
        {
            throw std::runtime_error(
                "未能按较小体积方向创建最终修剪体。");
        }
        createdFeatures.push_back(trimFeature->Tag());
        return true;
#endif
    }
    catch (const NXOpen::NXException& ex)
    {
        error = stage + "：" +
                (ex.Message() != nullptr ? ex.Message()
                                         : "NXOpen 建模失败。");
    }
    catch (const std::exception& ex)
    {
        error = stage + "：" + ex.what();
    }
    catch (...)
    {
        error = stage + "：发生未知异常。";
    }
    return false;
}

void CaiR1Dialog::ShowError(const std::string& message) const
{
    ui_->NXMessageBox()->Show(
        "分割圆角1",
        NXOpen::NXMessageBox::DialogTypeError,
        message.c_str());
}

extern "C" __declspec(dllexport) int ZhihuiCaiR1BuildCustomFeature(
    void* eventPointer)
{
    if (gActiveCaiR1Dialog == nullptr || eventPointer == nullptr)
    {
        return 1;
    }
    return gActiveCaiR1Dialog->BuildCustomFeatureConstruction(
        static_cast<NXOpen::Features::CustomFeaturePreUpdateEvent*>(
            eventPointer));
}
