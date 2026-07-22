#include "CaiRBan.hpp"
#include "CaiRBanCustomFeatureShared.hpp"
#include "../../common/ZhihuiDialogMemory.hpp"
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
#include <NXOpen/EdgeBoundaryRule.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/FaceDumbRule.hxx>
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
#include <NXOpen/Features_ExtrudeBuilder.hxx>
#include <NXOpen/Features_Feature.hxx>
#include <NXOpen/Features_FeatureCollection.hxx>
#include <NXOpen/Features_OffsetFaceBuilder.hxx>
#include <NXOpen/Features_ThickenBuilder.hxx>
#include <NXOpen/Features_TrimBody2Builder.hxx>
#include <NXOpen/Features_ReplaceFaceBuilder.hxx>
#include <NXOpen/GeometricUtilities_BooleanOperation.hxx>
#include <NXOpen/GeometricUtilities_Extend.hxx>
#include <NXOpen/GeometricUtilities_FeatureOptions.hxx>
#include <NXOpen/GeometricUtilities_BooleanToolBuilder.hxx>
#include <NXOpen/GeometricUtilities_FacePlaneToolBuilder.hxx>
#include <NXOpen/GeometricUtilities_Limits.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXMessageBox.hxx>
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

#include <uf_modl.h>
#include <uf_eval.h>
#include <uf_obj.h>
#include <uf_object_types.h>
#include <uf_ui_types.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <vector>

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace
{
constexpr double kTolerance = 1.0e-5;
constexpr double kParallelTolerance = 0.999;
constexpr double kCoverageThreshold = 0.60;
CaiRBanDialog* gActiveCaiRBanDialog = nullptr;

struct OppositeFaceCandidate
{
    NXOpen::Face* face = nullptr;
    double signedDistance = 0.0;
    double distance = 0.0;
};

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

NXOpen::Vector3d Subtract(const NXOpen::Point3d& to, const NXOpen::Point3d& from)
{
    return NXOpen::Vector3d(to.X - from.X, to.Y - from.Y, to.Z - from.Z);
}

NXOpen::Vector3d Cross(const NXOpen::Vector3d& a, const NXOpen::Vector3d& b)
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

bool FacePlaneData(NXOpen::Face* face,
                   NXOpen::Point3d& point,
                   NXOpen::Vector3d& outwardNormal)
{
    if (face == nullptr || face->SolidFaceType() != NXOpen::Face::FaceTypePlanar)
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
                              &radius, &radiusData, &normalDirection) != 0)
    {
        return false;
    }

    point = NXOpen::Point3d(origin[0], origin[1], origin[2]);
    outwardNormal = NXOpen::Vector3d(direction[0] * normalDirection,
                                     direction[1] * normalDirection,
                                     direction[2] * normalDirection);
    return Normalize(outwardNormal);
}

bool PointOnFace(NXOpen::Face* face, const NXOpen::Point3d& point)
{
    double coordinates[3] = {point.X, point.Y, point.Z};
    int status = 0;
    return face != nullptr &&
           UF_MODL_ask_point_containment(coordinates, face->Tag(), &status) == 0 &&
           (status == 1 || status == 3);
}

bool OuterBoundaryEdges(NXOpen::Face* face,
                        std::vector<NXOpen::Edge*>& edges)
{
    edges.clear();
    if (face == nullptr)
    {
        return false;
    }

    uf_loop_p_t loops = nullptr;
    if (UF_MODL_ask_face_loops(face->Tag(), &loops) != 0 || loops == nullptr)
    {
        return false;
    }

    int loopCount = 0;
    UF_MODL_ask_loop_list_count(loops, &loopCount);
    for (int loopIndex = 0; loopIndex < loopCount; ++loopIndex)
    {
        int loopType = 0;
        uf_list_p_t edgeList = nullptr;
        if (UF_MODL_ask_loop_list_item(loops, loopIndex, &loopType, &edgeList) != 0 ||
            loopType != 1 || edgeList == nullptr)
        {
            continue;
        }

        int edgeCount = 0;
        UF_MODL_ask_list_count(edgeList, &edgeCount);
        for (int edgeIndex = 0; edgeIndex < edgeCount; ++edgeIndex)
        {
            tag_t edgeTag = NULL_TAG;
            if (UF_MODL_ask_list_item(edgeList, edgeIndex, &edgeTag) != 0 ||
                edgeTag == NULL_TAG)
            {
                continue;
            }
            NXOpen::Edge* edge =
                dynamic_cast<NXOpen::Edge*>(NXOpen::NXObjectManager::Get(edgeTag));
            if (edge != nullptr)
            {
                edges.push_back(edge);
            }
        }
    }
    UF_MODL_delete_loop_list(&loops);
    return !edges.empty();
}

bool EdgeSamplePoints(NXOpen::Edge* edge,
                      std::vector<NXOpen::Point3d>& samples)
{
    samples.clear();
    if (edge == nullptr)
    {
        return false;
    }

    UF_EVAL_p_t evaluator = nullptr;
    if (UF_EVAL_initialize(edge->Tag(), &evaluator) == 0 && evaluator != nullptr)
    {
        double limits[2] = {};
        if (UF_EVAL_ask_limits(evaluator, limits) == 0 && limits[1] > limits[0])
        {
            for (double fraction : {0.25, 0.50, 0.75})
            {
                const double parameter =
                    limits[0] + (limits[1] - limits[0]) * fraction;
                double point[3] = {};
                if (UF_EVAL_evaluate(evaluator, 0, parameter, point, nullptr) == 0)
                {
                    samples.emplace_back(point[0], point[1], point[2]);
                }
            }
        }
        UF_EVAL_free(evaluator);
    }
    if (samples.size() >= 2)
    {
        return true;
    }

    samples.clear();
    try
    {
        NXOpen::Point3d first;
        NXOpen::Point3d second;
        edge->GetVertices(&first, &second);
        samples.push_back(first);
        samples.emplace_back((first.X + second.X) * 0.5,
                             (first.Y + second.Y) * 0.5,
                             (first.Z + second.Z) * 0.5);
        samples.push_back(second);
    }
    catch (...)
    {
        samples.clear();
    }
    return samples.size() >= 2;
}

NXOpen::Face* AdjacentBodyFace(NXOpen::Edge* edge,
                               NXOpen::Face* selectedFace,
                               NXOpen::Body* targetBody)
{
    if (edge == nullptr)
    {
        return nullptr;
    }
    for (NXOpen::Face* face : edge->GetUnsortedFaces())
    {
        if (face != nullptr && face != selectedFace && face->GetBody() == targetBody)
        {
            return face;
        }
    }
    return nullptr;
}

NXOpen::Face* ExtrudedSideFaceForEdge(
    NXOpen::Edge* sourceEdge,
    const std::vector<NXOpen::Face*>& toolFaces,
    const NXOpen::Vector3d& extrusionDirection)
{
    std::vector<NXOpen::Point3d> samples;
    if (!EdgeSamplePoints(sourceEdge, samples))
    {
        return nullptr;
    }

    NXOpen::Face* match = nullptr;
    for (NXOpen::Face* candidate : toolFaces)
    {
        if (candidate == nullptr)
        {
            continue;
        }

        NXOpen::Point3d planePoint;
        NXOpen::Vector3d planeNormal;
        if (FacePlaneData(candidate, planePoint, planeNormal) &&
            std::fabs(Dot(planeNormal, extrusionDirection)) > kParallelTolerance)
        {
            continue; // Start/end cap, not the side generated by this edge.
        }

        std::size_t contained = 0;
        for (const NXOpen::Point3d& sample : samples)
        {
            contained += PointOnFace(candidate, sample) ? 1U : 0U;
        }
        if (contained >= 2)
        {
            if (match != nullptr)
            {
                return nullptr; // Ambiguous mapping is unsafe for Replace Face.
            }
            match = candidate;
        }
    }
    return match;
}

bool ProjectionAxes(NXOpen::Face* face,
                    const NXOpen::Vector3d& normal,
                    NXOpen::Vector3d& xAxis,
                    NXOpen::Vector3d& yAxis)
{
    if (face != nullptr)
    {
        for (NXOpen::Edge* edge : face->GetEdges())
        {
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
            xAxis = Subtract(second, first);
            const double component = Dot(xAxis, normal);
            xAxis.X -= normal.X * component;
            xAxis.Y -= normal.Y * component;
            xAxis.Z -= normal.Z * component;
            if (Normalize(xAxis))
            {
                yAxis = Cross(normal, xAxis);
                return Normalize(yAxis);
            }
        }
    }

    const NXOpen::Vector3d seed(std::fabs(normal.X) < 0.9 ? 1.0 : 0.0,
                                std::fabs(normal.X) < 0.9 ? 0.0 : 1.0,
                                0.0);
    xAxis = Cross(seed, normal);
    if (!Normalize(xAxis))
    {
        return false;
    }
    yAxis = Cross(normal, xAxis);
    return Normalize(yAxis);
}

bool BuildInteriorSamples(NXOpen::Face* face,
                          const NXOpen::Point3d& planeOrigin,
                          const NXOpen::Vector3d& xAxis,
                          const NXOpen::Vector3d& yAxis,
                          std::vector<NXOpen::Point3d>& samples)
{
    samples.clear();
    double minimumX = std::numeric_limits<double>::max();
    double maximumX = -minimumX;
    double minimumY = minimumX;
    double maximumY = -minimumX;
    for (NXOpen::Edge* edge : face->GetEdges())
    {
        NXOpen::Point3d points[2];
        try
        {
            edge->GetVertices(&points[0], &points[1]);
        }
        catch (...)
        {
            continue;
        }
        for (const NXOpen::Point3d& point : points)
        {
            const NXOpen::Vector3d offset = Subtract(point, planeOrigin);
            const double x = Dot(offset, xAxis);
            const double y = Dot(offset, yAxis);
            minimumX = std::min(minimumX, x);
            maximumX = std::max(maximumX, x);
            minimumY = std::min(minimumY, y);
            maximumY = std::max(maximumY, y);
        }
    }
    if (!std::isfinite(minimumX) || maximumX - minimumX <= kTolerance ||
        maximumY - minimumY <= kTolerance)
    {
        return false;
    }

    // Same principle as feature 36: sample only the selected trimmed face,
    // so concave boundaries and inner loops do not contaminate thickness.
    constexpr int divisions = 11;
    for (int xIndex = 0; xIndex < divisions; ++xIndex)
    {
        for (int yIndex = 0; yIndex < divisions; ++yIndex)
        {
            const double x = minimumX + (maximumX - minimumX) *
                (static_cast<double>(xIndex) + 0.5) / divisions;
            const double y = minimumY + (maximumY - minimumY) *
                (static_cast<double>(yIndex) + 0.5) / divisions;
            const NXOpen::Point3d candidate(
                planeOrigin.X + xAxis.X * x + yAxis.X * y,
                planeOrigin.Y + xAxis.Y * x + yAxis.Y * y,
                planeOrigin.Z + xAxis.Z * x + yAxis.Z * y);
            if (PointOnFace(face, candidate))
            {
                samples.push_back(candidate);
            }
        }
    }
    return !samples.empty();
}

std::string DialogPath()
{
    wchar_t modulePath[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(
        reinterpret_cast<HMODULE>(&__ImageBase), modulePath, MAX_PATH);
    if (length > 0 && length < MAX_PATH)
    {
        const std::filesystem::path path =
            std::filesystem::path(modulePath).parent_path() / L"CaiRBan.dlx";
        if (std::filesystem::exists(path))
        {
            return path.string();
        }
    }
    return "CaiRBan.dlx";
}

std::string Number(double value)
{
    std::ostringstream stream;
    stream.precision(15);
    stream << value;
    return stream.str();
}
}

CaiRBanDialog::CaiRBanDialog()
    : ui_(NXOpen::UI::GetUI()),
      session_(NXOpen::Session::GetSession()),
      dialog_(ui_->CreateDialog(DialogPath().c_str())),
      recolorToggle_(nullptr),
      colorMode_(nullptr),
      fixedColor_(nullptr),
      extensionLength_(nullptr),
      wrapMode_(nullptr),
      faceSelect_(nullptr),
      customFeatureManager_(nullptr),
      editedFeature_(nullptr),
      featureClass_(nullptr),
      previewUndoMark_(static_cast<NXOpen::Session::UndoMarkId>(0)),
      hasPreview_(false),
      loadingEditedFeature_(false),
      rebuildingPreview_(false),
      buildingCustomFeature_(false),
      previewSubtractCreated_(false),
      previewColor_(0),
      previewTargetBodyTag_(NULL_TAG),
      previewSelectedFaceTag_(NULL_TAG),
      previewCreatedFeatureTags_(),
      previewBodyTranslucencies_()
{
    customFeatureManager_ = session_->CustomFeatureClassManager();
    editedFeature_ = customFeatureManager_->GetEditedCustomFeature();
    try
    {
        featureClass_ = customFeatureManager_->GetClassFromName(
            zhihui_cairban::kFeatureClassName);
    }
    catch (...)
    {
        featureClass_ = nullptr;
    }
    gActiveCaiRBanDialog = this;
    dialog_->AddInitializeHandler(NXOpen::make_callback(this, &CaiRBanDialog::initialize_cb));
    dialog_->AddDialogShownHandler(NXOpen::make_callback(this, &CaiRBanDialog::dialogShown_cb));
    dialog_->AddUpdateHandler(NXOpen::make_callback(this, &CaiRBanDialog::update_cb));
    dialog_->AddApplyHandler(NXOpen::make_callback(this, &CaiRBanDialog::apply_cb));
    dialog_->AddOkHandler(NXOpen::make_callback(this, &CaiRBanDialog::ok_cb));
    dialog_->AddCancelHandler(NXOpen::make_callback(this, &CaiRBanDialog::cancel_cb));
}

CaiRBanDialog::~CaiRBanDialog()
{
    RestorePreviewTranslucency();
    if (gActiveCaiRBanDialog == this)
    {
        gActiveCaiRBanDialog = nullptr;
    }
    delete dialog_;
}

NXOpen::BlockStyler::BlockDialog::DialogResponse CaiRBanDialog::Launch()
{
    return dialog_->LaunchInDialogMode(
        editedFeature_ != nullptr
            ? NXOpen::BlockStyler::BlockDialog::DialogModeEdit
            : NXOpen::BlockStyler::BlockDialog::DialogModeCreate);
}

void CaiRBanDialog::initialize_cb()
{
    recolorToggle_ = dialog_->TopBlock()->FindBlock("recolor_toggle");
    colorMode_ = dialog_->TopBlock()->FindBlock("color_mode");
    fixedColor_ = dynamic_cast<NXOpen::BlockStyler::ObjectColorPicker*>(
        dialog_->TopBlock()->FindBlock("fixed_color"));
    extensionLength_ = dialog_->TopBlock()->FindBlock("extension_length");
    wrapMode_ = dialog_->TopBlock()->FindBlock("wrap_mode");
    faceSelect_ = dialog_->TopBlock()->FindBlock("face_select");
    if (recolorToggle_ == nullptr || colorMode_ == nullptr ||
        fixedColor_ == nullptr || extensionLength_ == nullptr ||
        wrapMode_ == nullptr || faceSelect_ == nullptr)
    {
        throw std::runtime_error("CaiRBan.dlx is missing a required block.");
    }

    if (fixedColor_->GetValue().empty())
    {
        fixedColor_->SetValue(std::vector<int>{36});
    }
    if (editedFeature_ == nullptr)
    {
        LoadDialogState();
    }
    UpdateColorControlVisibility();

    NXOpen::BlockStyler::PropertyList* properties = faceSelect_->GetProperties();
    std::vector<NXOpen::Selection::MaskTriple> masks;
    masks.emplace_back(UF_solid_type, UF_solid_face_subtype,
                       UF_UI_SEL_FEATURE_CYLINDRICAL_FACE);
    properties->SetSelectionFilter(
        "SelectionFilter",
        NXOpen::Selection::SelectionActionClearAndEnableSpecific,
        masks);
    properties->SetEnum("StepStatus", 0);
    delete properties;

    LoadEditedCustomFeatureData();
}

void CaiRBanDialog::dialogShown_cb()
{
    if (customFeatureManager_ != nullptr)
    {
        NXOpen::Features::CustomFeature* currentEdited =
            customFeatureManager_->GetEditedCustomFeature();
        if (currentEdited != nullptr && currentEdited != editedFeature_)
        {
            editedFeature_ = currentEdited;
            LoadEditedCustomFeatureData();
        }
    }
    UpdateColorControlVisibility();
}

int CaiRBanDialog::update_cb(NXOpen::BlockStyler::UIBlock* block)
{
    if (loadingEditedFeature_ || rebuildingPreview_)
    {
        return 0;
    }

    const bool parameterChanged =
        block == recolorToggle_ || block == colorMode_ ||
        block == fixedColor_ || block == extensionLength_ ||
        block == wrapMode_;
    const bool selectionChanged = block == faceSelect_;
    if (!parameterChanged && !selectionChanged)
    {
        return 0;
    }

    rebuildingPreview_ = true;
    if (parameterChanged)
    {
        UpdateColorControlVisibility();
        SaveDialogState();
    }

    // Every result-affecting dialog change follows the same transaction:
    // remove the old preview first, then build a new preview from the current
    // control values. Rebuild even when the preceding preview failed and
    // hasPreview_ is false, so changing a parameter can immediately retry.
    if (hasPreview_ ||
        previewUndoMark_ != static_cast<NXOpen::Session::UndoMarkId>(0) ||
        !previewCreatedFeatureTags_.empty())
    {
        if (!UndoPreview())
        {
            rebuildingPreview_ = false;
            ShowError("旧预览特征未能完全撤销，已停止创建新预览。");
            return 1;
        }
    }
    const int result = SelectedFace() != nullptr ? CreatePreview() : 0;
    rebuildingPreview_ = false;
    return result;
}

void CaiRBanDialog::UpdateColorControlVisibility() const
{
    if (recolorToggle_ == nullptr || colorMode_ == nullptr || fixedColor_ == nullptr)
    {
        return;
    }
    NXOpen::BlockStyler::PropertyList* toggleProperties =
        recolorToggle_->GetProperties();
    const bool enabled = toggleProperties->GetLogical("Value");
    delete toggleProperties;

    colorMode_->SetShow(enabled);
    int mode = 0;
    if (enabled)
    {
        NXOpen::BlockStyler::PropertyList* modeProperties =
            colorMode_->GetProperties();
        mode = modeProperties->GetEnum("Value");
        delete modeProperties;
    }
    fixedColor_->SetShow(enabled && mode == 1);
}

bool CaiRBanDialog::RecolorEnabled() const
{
    if (recolorToggle_ == nullptr)
    {
        return false;
    }
    NXOpen::BlockStyler::PropertyList* properties =
        recolorToggle_->GetProperties();
    const bool value = properties->GetLogical("Value");
    delete properties;
    return value;
}

int CaiRBanDialog::RequestedColor() const
{
    const int mode = ColorMode();
    if (mode == 1 && fixedColor_ != nullptr)
    {
        return FixedColor();
    }

    static std::mt19937 generator(static_cast<unsigned int>(GetTickCount64()));
    static std::uniform_int_distribution<int> colors(1, 216);
    return colors(generator);
}

int CaiRBanDialog::ColorMode() const
{
    if (colorMode_ == nullptr)
    {
        return 0;
    }
    NXOpen::BlockStyler::PropertyList* properties = colorMode_->GetProperties();
    const int value = properties->GetEnum("Value");
    delete properties;
    return value;
}

int CaiRBanDialog::FixedColor() const
{
    if (fixedColor_ != nullptr)
    {
        const std::vector<int> values = fixedColor_->GetValue();
        if (!values.empty())
        {
            return (std::max)(1, (std::min)(216, values.front()));
        }
    }
    return 36;
}

double CaiRBanDialog::ExtensionLength() const
{
    double value = 0.0;
    zhihui_dialog_memory::TryGetDouble(extensionLength_, &value);
    return (std::max)(0.0, value);
}

int CaiRBanDialog::WrapMode() const
{
    int value = 0;
    zhihui_dialog_memory::TryGetEnum(wrapMode_, &value);
    return value == 1 ? 1 : 0;
}

void CaiRBanDialog::LoadDialogState() const
{
    constexpr const wchar_t* fileName = L"CaiRBan_state.ini";
    zhihui_dialog_memory::LoadLogical(
        fileName, L"recolorEnabled", recolorToggle_);
    zhihui_dialog_memory::LoadEnum(fileName, L"colorMode", colorMode_);
    zhihui_dialog_memory::LoadDouble(
        fileName, L"extensionLength", extensionLength_);
    zhihui_dialog_memory::LoadEnum(fileName, L"wrapMode", wrapMode_);
    const int color = (std::max)(
        1,
        (std::min)(216,
                   zhihui_dialog_memory::ReadInt(
                       fileName, L"fixedColor", FixedColor())));
    fixedColor_->SetValue(std::vector<int>{color});
}

void CaiRBanDialog::SaveDialogState() const
{
    constexpr const wchar_t* fileName = L"CaiRBan_state.ini";
    zhihui_dialog_memory::SaveLogical(
        fileName, L"recolorEnabled", recolorToggle_);
    zhihui_dialog_memory::SaveEnum(fileName, L"colorMode", colorMode_);
    zhihui_dialog_memory::SaveDouble(
        fileName, L"extensionLength", extensionLength_);
    zhihui_dialog_memory::SaveEnum(fileName, L"wrapMode", wrapMode_);
    zhihui_dialog_memory::WriteInt(
        fileName, L"fixedColor", FixedColor());
}

void CaiRBanDialog::LoadEditedCustomFeatureData()
{
    if (editedFeature_ == nullptr || faceSelect_ == nullptr)
    {
        return;
    }
    loadingEditedFeature_ = true;
    try
    {
        NXOpen::Features::CustomFeatureData* data = editedFeature_->FeatureData();
        try
        {
            NXOpen::Face* storedFace = dynamic_cast<NXOpen::Face*>(
                data->CustomTagAttributeByName(
                        zhihui_cairban::kAttrSelectedFace)
                    ->Value());
            if (storedFace != nullptr)
            {
                NXOpen::BlockStyler::PropertyList* faceProperties =
                    faceSelect_->GetProperties();
                faceProperties->SetTaggedObjectVector(
                    "SelectedObjects",
                    std::vector<NXOpen::TaggedObject*>{storedFace});
                delete faceProperties;
            }
        }
        catch (...)
        {
            // Older or topologically consumed faces need not be displayed.
        }
        NXOpen::BlockStyler::PropertyList* toggleProperties =
            recolorToggle_->GetProperties();
        toggleProperties->SetLogical(
            "Value",
            data->CustomDoubleAttributeByName(
                    zhihui_cairban::kAttrRecolorEnabled)
                    ->Value() > 0.5);
        delete toggleProperties;

        NXOpen::BlockStyler::PropertyList* modeProperties =
            colorMode_->GetProperties();
        modeProperties->SetEnum(
            "Value",
            static_cast<int>(data->CustomDoubleAttributeByName(
                zhihui_cairban::kAttrColorMode)->Value()));
        delete modeProperties;

        const int fixedColor = static_cast<int>(
            data->CustomDoubleAttributeByName(
                    zhihui_cairban::kAttrFixedColor)
                ->Value());
        fixedColor_->SetValue(std::vector<int>{
            (std::max)(1, (std::min)(216, fixedColor))});
        zhihui_dialog_memory::TrySetDouble(
            extensionLength_,
            data->CustomDoubleAttributeByName(
                    zhihui_cairban::kAttrExtensionLength)
                ->Value());
        zhihui_dialog_memory::TrySetEnum(
            wrapMode_,
            static_cast<int>(data->CustomDoubleAttributeByName(
                zhihui_cairban::kAttrWrapMode)->Value()));
        faceSelect_->SetEnable(false);
        UpdateColorControlVisibility();
    }
    catch (...)
    {
        loadingEditedFeature_ = false;
        throw;
    }
    loadingEditedFeature_ = false;
}

bool CaiRBanDialog::CommitEditableCustomFeature(std::string& error)
{
    NXOpen::Part* workPart = session_->Parts()->Work();
    const bool editing = editedFeature_ != nullptr;
    NXOpen::Body* targetBody = nullptr;
    if (!editing && previewTargetBodyTag_ != NULL_TAG &&
        UF_OBJ_ask_status(previewTargetBodyTag_) == UF_OBJ_ALIVE)
    {
        targetBody = dynamic_cast<NXOpen::Body*>(
            NXOpen::NXObjectManager::Get(previewTargetBodyTag_));
    }
    if (workPart == nullptr || featureClass_ == nullptr ||
        (!editing && targetBody == nullptr))
    {
        error = featureClass_ == nullptr
                    ? "拆圆弧板可编辑特征未注册，部署后需重启 UG。"
                    : "没有可用的目标体。";
        return false;
    }

    NXOpen::Features::CustomFeatureBuilder* builder = nullptr;
    try
    {
        builder = workPart->Features()->CreateCustomFeatureBuilder(editedFeature_);
        NXOpen::Features::CustomFeatureData* data = nullptr;
        if (!editing)
        {
            NXOpen::Features::CustomAttributeCollection* attributes =
                workPart->Features()->CustomAttributeCollection();
            std::vector<NXOpen::Features::CustomAttribute*> values;
            const std::vector<NXOpen::Features::CustomAttribute::Property>
                required{NXOpen::Features::CustomAttribute::PropertyMandatoryInput};
            std::vector<NXOpen::Features::CustomAttribute::Property>
                targetProperties = required;
            targetProperties.push_back(
                NXOpen::Features::CustomAttribute::PropertyIsReferencingTargetBody);
            values.push_back(attributes->CreateCustomTagAttribute(
                zhihui_cairban::kAttrTargetBody, targetProperties));
            values.push_back(attributes->CreateCustomTagAttribute(
                zhihui_cairban::kAttrSelectedFace,
                std::vector<NXOpen::Features::CustomAttribute::Property>()));
            const std::vector<NXOpen::Features::CustomAttribute::Property> optional;
            values.push_back(attributes->CreateCustomDoubleAttribute(
                zhihui_cairban::kAttrRecolorEnabled, optional));
            values.push_back(attributes->CreateCustomDoubleAttribute(
                zhihui_cairban::kAttrColorMode, optional));
            values.push_back(attributes->CreateCustomDoubleAttribute(
                zhihui_cairban::kAttrFixedColor, optional));
            values.push_back(attributes->CreateCustomDoubleAttribute(
                zhihui_cairban::kAttrAppliedColor, optional));
            values.push_back(attributes->CreateCustomDoubleAttribute(
                zhihui_cairban::kAttrExtensionLength, optional));
            values.push_back(attributes->CreateCustomDoubleAttribute(
                zhihui_cairban::kAttrWrapMode, optional));
            data = workPart->Features()->CustomFeatureDataCollection()->CreateData(
                featureClass_, values);
            data->CustomTagAttributeByName(
                zhihui_cairban::kAttrTargetBody)
                ->SetValue(targetBody);
            if (previewSelectedFaceTag_ != NULL_TAG &&
                UF_OBJ_ask_status(previewSelectedFaceTag_) == UF_OBJ_ALIVE)
            {
                data->CustomTagAttributeByName(
                        zhihui_cairban::kAttrSelectedFace)
                    ->SetValue(NXOpen::NXObjectManager::Get(
                        previewSelectedFaceTag_));
            }
        }
        else
        {
            data = editedFeature_->FeatureData();
        }

        // A new feature has already been coloured by the live preview.  Reuse
        // that exact colour when it is committed; otherwise random mode would
        // draw a second colour here and visibly change on Apply.
        const int appliedColor =
            RecolorEnabled()
                ? (!editing && hasPreview_ && previewColor_ >= 1
                       ? previewColor_
                       : RequestedColor())
                : 0;
        data->CustomDoubleAttributeByName(
                zhihui_cairban::kAttrRecolorEnabled)
            ->SetValue(RecolorEnabled() ? 1.0 : 0.0);
        data->CustomDoubleAttributeByName(zhihui_cairban::kAttrColorMode)
            ->SetValue(static_cast<double>(ColorMode()));
        data->CustomDoubleAttributeByName(zhihui_cairban::kAttrFixedColor)
            ->SetValue(static_cast<double>(FixedColor()));
        data->CustomDoubleAttributeByName(zhihui_cairban::kAttrAppliedColor)
            ->SetValue(static_cast<double>(appliedColor));
        data->CustomDoubleAttributeByName(
                zhihui_cairban::kAttrExtensionLength)
            ->SetValue(ExtensionLength());
        data->CustomDoubleAttributeByName(zhihui_cairban::kAttrWrapMode)
            ->SetValue(static_cast<double>(WrapMode()));

        buildingCustomFeature_ = !editing;
        builder->SetFeatureData(data);
        NXOpen::Features::Feature* committed = builder->CommitFeature();
        builder->Destroy();
        builder = nullptr;
        buildingCustomFeature_ = false;
        NXOpen::Features::CustomFeature* customFeature =
            dynamic_cast<NXOpen::Features::CustomFeature*>(committed);
        if (customFeature == nullptr)
        {
            error = "NX 未返回拆圆弧板自定义特征。";
            return false;
        }
        customFeature->SetName(zhihui_cairban::kFeatureDisplayName);
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
        error = ex.Message();
    }
    catch (const std::exception& ex)
    {
        buildingCustomFeature_ = false;
        if (builder != nullptr) builder->Destroy();
        error = ex.what();
    }
    return false;
}

int CaiRBanDialog::BuildCustomFeatureConstruction(
    NXOpen::Features::CustomFeaturePreUpdateEvent* event)
{
    if (event == nullptr || event->GetCustomFeature() == nullptr)
    {
        return 1;
    }

    std::vector<NXOpen::Features::ConstructionFeatureData*> construction =
        event->GetConstructionFeatures();
    if (construction.empty())
    {
        if (!buildingCustomFeature_ || !hasPreview_ ||
            previewCreatedFeatureTags_.empty())
        {
            return 1;
        }
        std::set<tag_t> seen;
        for (tag_t featureTag : previewCreatedFeatureTags_)
        {
            if (featureTag == NULL_TAG || !seen.insert(featureTag).second ||
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
        if (construction.empty())
        {
            return 1;
        }
    }

    NXOpen::Features::CustomFeatureData* data =
        event->GetCustomFeature()->FeatureData();
    const bool recolor =
        data->CustomDoubleAttributeByName(
                zhihui_cairban::kAttrRecolorEnabled)
                ->Value() > 0.5;
    const int color = static_cast<int>(
        data->CustomDoubleAttributeByName(
                zhihui_cairban::kAttrAppliedColor)
            ->Value());
    if (recolor && color >= 1 && color <= 216)
    {
        // The final construction is the commit-time subtract. Its body is the
        // target remainder; the retained tool is produced by the preceding
        // thicken/trim/offset feature and is the body that should be coloured.
        auto item = construction.rbegin();
        if (item != construction.rend())
        {
            ++item;
        }
        for (; item != construction.rend(); ++item)
        {
            NXOpen::Features::Feature* resultFeature = (*item)->GetFeature();
            if (resultFeature != nullptr && !resultFeature->GetBodies().empty())
            {
                UF_OBJ_set_color(resultFeature->GetBodies().front()->Tag(),
                                 color);
                break;
            }
        }
    }
    for (NXOpen::Features::ConstructionFeatureData* item : construction)
    {
        item->SetShowInGraphicView(true);
    }
    event->SetConstructionFeatures(construction);
    if (buildingCustomFeature_ && hasPreview_)
    {
        CommitPreview();
        previewCreatedFeatureTags_.clear();
    }
    return 0;
}

int CaiRBanDialog::apply_cb()
{
    SaveDialogState();
    const bool editing = editedFeature_ != nullptr;
    if (editedFeature_ == nullptr && !hasPreview_ && SelectedFace() == nullptr)
    {
        ShowError("请选择一个圆柱面。");
        return 1;
    }
    if (editedFeature_ == nullptr && !hasPreview_)
    {
        const int previewResult = CreatePreview();
        if (previewResult != 0)
        {
            return previewResult;
        }
    }
    std::string error;
    if (!editing && !CommitPreviewSubtract(error))
    {
        ShowError(error);
        return 1;
    }
    if (!CommitEditableCustomFeature(error))
    {
        ShowError(error);
        return 1;
    }
    if (!editing)
    {
        NXOpen::BlockStyler::PropertyList* properties =
            faceSelect_->GetProperties();
        loadingEditedFeature_ = true;
        properties->SetTaggedObjectVector(
            "SelectedObjects", std::vector<NXOpen::TaggedObject*>());
        loadingEditedFeature_ = false;
        delete properties;
        previewTargetBodyTag_ = NULL_TAG;
        previewSelectedFaceTag_ = NULL_TAG;
        previewColor_ = 0;
        previewSubtractCreated_ = false;
    }
    return 0;
}

int CaiRBanDialog::ok_cb()
{
    return apply_cb();
}

int CaiRBanDialog::cancel_cb()
{
    UndoPreview();
    return 0;
}

int CaiRBanDialog::CreatePreview()
{
    NXOpen::Face* face = SelectedFace();
    if (face == nullptr)
    {
        return 0;
    }

    previewUndoMark_ = session_->SetUndoMark(
        NXOpen::Session::MarkVisibilityInvisible,
        "拆圆弧板预览");
    previewTargetBodyTag_ = face->GetBody()->Tag();
    previewSelectedFaceTag_ = face->Tag();
    std::string error;
    double diameter = 0.0;
    double thickness = 0.0;
    bool reverseThicken = false;
    previewCreatedFeatureTags_.clear();
    previewSubtractCreated_ = false;
    previewColor_ = RecolorEnabled() ? RequestedColor() : 0;
    if (!EstimateCylinderThickness(face->GetBody(), face, diameter, thickness,
                                   reverseThicken, WrapMode(), error) ||
        !CreateArcPanel(face, thickness, reverseThicken, ExtensionLength(),
                        WrapMode(), error, RecolorEnabled(), previewColor_,
                        &previewCreatedFeatureTags_))
    {
        UndoPreview();
        ShowError(error);
        return 1;
    }
    NXOpen::Body* previewBody = nullptr;
    for (auto item = previewCreatedFeatureTags_.rbegin();
         item != previewCreatedFeatureTags_.rend(); ++item)
    {
        if (*item == NULL_TAG || UF_OBJ_ask_status(*item) != UF_OBJ_ALIVE)
        {
            continue;
        }
        NXOpen::Features::Feature* feature =
            dynamic_cast<NXOpen::Features::Feature*>(
                NXOpen::NXObjectManager::Get(*item));
        if (feature != nullptr && !feature->GetBodies().empty())
        {
            previewBody = feature->GetBodies().front();
            break;
        }
    }
    ApplyPreviewTranslucency(previewBody);
    hasPreview_ = true;
    return 0;
}

bool CaiRBanDialog::UndoPreview()
{
    RestorePreviewTranslucency();
    const std::vector<tag_t> previewFeatureTags =
        previewCreatedFeatureTags_;
    try
    {
        if (previewUndoMark_ !=
                static_cast<NXOpen::Session::UndoMarkId>(0) &&
            session_->DoesUndoMarkExist(previewUndoMark_,
                                        "拆圆弧板预览"))
        {
            session_->UndoToMark(previewUndoMark_, "拆圆弧板预览");
            if (session_->DoesUndoMarkExist(previewUndoMark_, "拆圆弧板预览"))
            {
                session_->DeleteUndoMark(previewUndoMark_, "拆圆弧板预览");
            }
        }
    }
    catch (...)
    {
    }

    // NX can recycle an invisible undo mark while Block Styler is processing
    // model updates. Never assume the old preview disappeared merely because
    // UndoToMark returned. Check every feature created by this preview and
    // remove any survivors in one dependency-aware update transaction.
    try
    {
        std::vector<NXOpen::TaggedObject*> survivingFeatures;
        std::set<tag_t> seen;
        for (auto item = previewFeatureTags.rbegin();
             item != previewFeatureTags.rend(); ++item)
        {
            const tag_t featureTag = *item;
            if (featureTag == NULL_TAG || !seen.insert(featureTag).second ||
                UF_OBJ_ask_status(featureTag) != UF_OBJ_ALIVE)
            {
                continue;
            }
            NXOpen::TaggedObject* object =
                dynamic_cast<NXOpen::TaggedObject*>(
                    NXOpen::NXObjectManager::Get(featureTag));
            if (object != nullptr)
            {
                survivingFeatures.push_back(object);
            }
        }

        if (!survivingFeatures.empty() && session_->UpdateManager() != nullptr)
        {
            const NXOpen::Session::UndoMarkId cleanupMark =
                session_->SetUndoMark(
                    NXOpen::Session::MarkVisibilityInvisible,
                    "拆圆弧板预览清理");
            NXOpen::Update* update = session_->UpdateManager();
            update->ClearDeleteList();
            update->ClearErrorList();
            update->AddObjectsToDeleteList(survivingFeatures);
            update->DoUpdate(cleanupMark);
            if (session_->DoesUndoMarkExist(cleanupMark,
                                            "拆圆弧板预览清理"))
            {
                session_->DeleteUndoMark(cleanupMark,
                                         "拆圆弧板预览清理");
            }
        }
    }
    catch (...)
    {
        // State is reset below; CreatePreview will report any remaining
        // dependency conflict instead of silently committing stale tags.
    }

    bool allPreviewFeaturesRemoved = true;
    for (tag_t featureTag : previewFeatureTags)
    {
        if (featureTag != NULL_TAG &&
            UF_OBJ_ask_status(featureTag) == UF_OBJ_ALIVE)
        {
            allPreviewFeaturesRemoved = false;
            break;
        }
    }

    previewUndoMark_ = static_cast<NXOpen::Session::UndoMarkId>(0);
    if (!allPreviewFeaturesRemoved)
    {
        hasPreview_ = true;
        previewCreatedFeatureTags_ = previewFeatureTags;
        return false;
    }
    hasPreview_ = false;
    previewCreatedFeatureTags_.clear();
    previewColor_ = 0;
    previewSubtractCreated_ = false;
    previewTargetBodyTag_ = NULL_TAG;
    previewSelectedFaceTag_ = NULL_TAG;
    return true;
}

void CaiRBanDialog::CommitPreview()
{
    RestorePreviewTranslucency();
    if (!hasPreview_ ||
        previewUndoMark_ == static_cast<NXOpen::Session::UndoMarkId>(0))
    {
        return;
    }
    if (session_->DoesUndoMarkExist(previewUndoMark_, "拆圆弧板预览"))
    {
        session_->SetUndoMarkName(previewUndoMark_, "拆圆弧板");
        session_->SetUndoMarkVisibility(
            previewUndoMark_, "拆圆弧板",
            NXOpen::Session::MarkVisibilityVisible);
    }
    previewUndoMark_ = static_cast<NXOpen::Session::UndoMarkId>(0);
    hasPreview_ = false;
}

NXOpen::Face* CaiRBanDialog::SelectedFace() const
{
    if (faceSelect_ == nullptr)
    {
        return nullptr;
    }
    NXOpen::BlockStyler::PropertyList* properties = faceSelect_->GetProperties();
    const std::vector<NXOpen::TaggedObject*> objects =
        properties->GetTaggedObjectVector("SelectedObjects");
    delete properties;
    return objects.size() == 1 ? dynamic_cast<NXOpen::Face*>(objects.front()) : nullptr;
}

void CaiRBanDialog::ApplyPreviewTranslucency(NXOpen::Body* previewBody)
{
    RestorePreviewTranslucency();
    NXOpen::Part* workPart = session_->Parts()->Work();
    if (workPart == nullptr || workPart->Bodies() == nullptr ||
        previewBody == nullptr)
    {
        return;
    }

    constexpr int kPreviewTranslucency = 80;
    for (NXOpen::Body* body : *workPart->Bodies())
    {
        if (body == nullptr || body == previewBody ||
            body->Tag() == NULL_TAG ||
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
            std::make_pair(body->Tag(), static_cast<int>(original)));
        UF_OBJ_set_translucency(
            body->Tag(),
            static_cast<UF_OBJ_translucency_t>(
                (std::max)(static_cast<int>(original),
                           kPreviewTranslucency)));
        body->RedisplayObject();
    }
}

void CaiRBanDialog::RestorePreviewTranslucency()
{
    for (const std::pair<tag_t, int>& state : previewBodyTranslucencies_)
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

bool CaiRBanDialog::CommitPreviewSubtract(std::string& error)
{
    if (previewSubtractCreated_)
    {
        return true;
    }

    NXOpen::Part* workPart = session_->Parts()->Work();
    NXOpen::Body* targetBody = nullptr;
    if (previewTargetBodyTag_ != NULL_TAG &&
        UF_OBJ_ask_status(previewTargetBodyTag_) == UF_OBJ_ALIVE)
    {
        targetBody = dynamic_cast<NXOpen::Body*>(
            NXOpen::NXObjectManager::Get(previewTargetBodyTag_));
    }

    NXOpen::Body* toolBody = nullptr;
    for (auto item = previewCreatedFeatureTags_.rbegin();
         item != previewCreatedFeatureTags_.rend(); ++item)
    {
        if (*item == NULL_TAG || UF_OBJ_ask_status(*item) != UF_OBJ_ALIVE)
        {
            continue;
        }
        NXOpen::Features::Feature* feature =
            dynamic_cast<NXOpen::Features::Feature*>(
                NXOpen::NXObjectManager::Get(*item));
        if (feature != nullptr && !feature->GetBodies().empty())
        {
            toolBody = feature->GetBodies().front();
            break;
        }
    }

    if (workPart == nullptr || targetBody == nullptr || toolBody == nullptr ||
        targetBody == toolBody)
    {
        error = "提交求差时未找到有效的目标体或保留工具体。";
        return false;
    }

    NXOpen::Features::BooleanBuilder* subtract = nullptr;
    try
    {
        subtract =
            workPart->Features()->CreateBooleanBuilderUsingCollector(nullptr);
        subtract->SetOperation(NXOpen::Features::Feature::BooleanTypeSubtract);
        subtract->SetRetainTarget(false);
        subtract->SetRetainTool(true);

        NXOpen::SelectionIntentRuleOptions* targetOptions =
            workPart->ScRuleFactory()->CreateRuleOptions();
        targetOptions->SetSelectedFromInactive(false);
        NXOpen::BodyDumbRule* targetRule =
            workPart->ScRuleFactory()->CreateRuleBodyDumb(
                std::vector<NXOpen::Body*>{targetBody}, true, targetOptions);
        delete targetOptions;
        NXOpen::ScCollector* targetCollector =
            workPart->ScCollectors()->CreateCollector();
        targetCollector->ReplaceRules(
            std::vector<NXOpen::SelectionIntentRule*>{targetRule}, false);
        subtract->SetTargetBodyCollector(targetCollector);

        NXOpen::SelectionIntentRuleOptions* toolOptions =
            workPart->ScRuleFactory()->CreateRuleOptions();
        toolOptions->SetSelectedFromInactive(false);
        NXOpen::BodyDumbRule* toolRule =
            workPart->ScRuleFactory()->CreateRuleBodyDumb(
                std::vector<NXOpen::Body*>{toolBody}, true, toolOptions);
        delete toolOptions;
        NXOpen::ScCollector* toolCollector =
            workPart->ScCollectors()->CreateCollector();
        toolCollector->ReplaceRules(
            std::vector<NXOpen::SelectionIntentRule*>{toolRule}, false);
        subtract->SetToolBodyCollector(toolCollector);

        NXOpen::Features::Feature* subtractFeature =
            dynamic_cast<NXOpen::Features::Feature*>(subtract->Commit());
        subtract->Destroy();
        subtract = nullptr;
        if (subtractFeature == nullptr)
        {
            error = "NX 未返回提交求差特征。";
            return false;
        }
        previewCreatedFeatureTags_.push_back(subtractFeature->Tag());
        previewSubtractCreated_ = true;
        return true;
    }
    catch (const NXOpen::NXException& ex)
    {
        if (subtract != nullptr)
        {
            subtract->Destroy();
        }
        error = "提交时求差失败：" +
                std::string(ex.Message() != nullptr ? ex.Message()
                                                    : "NXOpen 布尔求差失败。");
    }
    catch (const std::exception& ex)
    {
        if (subtract != nullptr)
        {
            subtract->Destroy();
        }
        error = "提交时求差失败：" + std::string(ex.what());
    }
    return false;
}

bool CaiRBanDialog::EstimateThicknessAndInnerNormal(
    NXOpen::Body* body,
    NXOpen::Face* face,
    double& thickness,
    NXOpen::Vector3d& innerNormal,
    NXOpen::Point3d& helpPoint,
    std::string& error) const
{
    thickness = 0.0;
    NXOpen::Point3d basePoint;
    NXOpen::Vector3d baseNormal;
    if (body == nullptr || !body->IsSolidBody() ||
        !FacePlaneData(face, basePoint, baseNormal))
    {
        error = "请选择实体上的平面。";
        return false;
    }

    NXOpen::Vector3d xAxis;
    NXOpen::Vector3d yAxis;
    std::vector<NXOpen::Point3d> samples;
    if (!ProjectionAxes(face, baseNormal, xAxis, yAxis) ||
        !BuildInteriorSamples(face, basePoint, xAxis, yAxis, samples))
    {
        error = "无法在选择面内生成板厚检测点。";
        return false;
    }
    helpPoint = samples.front();

    std::vector<OppositeFaceCandidate> candidates;
    for (NXOpen::Face* candidateFace : body->GetFaces())
    {
        if (candidateFace == nullptr || candidateFace == face)
        {
            continue;
        }
        NXOpen::Point3d candidatePoint;
        NXOpen::Vector3d candidateNormal;
        if (!FacePlaneData(candidateFace, candidatePoint, candidateNormal) ||
            std::fabs(Dot(baseNormal, candidateNormal)) < kParallelTolerance)
        {
            continue;
        }
        const double signedDistance = Dot(Subtract(candidatePoint, basePoint), baseNormal);
        if (std::fabs(signedDistance) > kTolerance)
        {
            candidates.push_back(
                {candidateFace, signedDistance, std::fabs(signedDistance)});
        }
    }
    std::sort(candidates.begin(), candidates.end(),
              [](const OppositeFaceCandidate& a, const OppositeFaceCandidate& b) {
                  return a.distance < b.distance;
              });

    // Feature 36 groups coplanar split faces before checking projected
    // coverage. This prevents a seam or cutout from hiding the true back face.
    for (std::size_t first = 0; first < candidates.size();)
    {
        std::size_t last = first + 1;
        const double groupTolerance = std::max(1.0e-3, candidates[first].distance * 1.0e-4);
        while (last < candidates.size() &&
               std::fabs(candidates[last].signedDistance -
                         candidates[first].signedDistance) <= groupTolerance)
        {
            ++last;
        }

        std::size_t covered = 0;
        for (const NXOpen::Point3d& sample : samples)
        {
            const NXOpen::Point3d projected =
                Move(sample, baseNormal, candidates[first].signedDistance);
            bool found = false;
            for (std::size_t index = first; index < last; ++index)
            {
                if (PointOnFace(candidates[index].face, projected))
                {
                    found = true;
                    break;
                }
            }
            covered += found ? 1U : 0U;
        }
        const double coverage = static_cast<double>(covered) /
                                static_cast<double>(samples.size());
        if (coverage + 1.0e-9 >= kCoverageThreshold)
        {
            thickness = candidates[first].distance;
            const double sign = candidates[first].signedDistance >= 0.0 ? 1.0 : -1.0;
            innerNormal = NXOpen::Vector3d(baseNormal.X * sign,
                                           baseNormal.Y * sign,
                                           baseNormal.Z * sign);
            return true;
        }
        first = last;
    }

    error = "未找到与选择面投影重合的板厚对面，无法计算板厚。";
    return false;
}

bool CaiRBanDialog::CreateAndSubtract(
    NXOpen::Face* face,
    double thickness,
    const NXOpen::Vector3d& innerNormal,
    const NXOpen::Point3d& helpPoint,
    std::string& error,
    bool recolor,
    int color,
    std::vector<tag_t>* createdFeatureTags) const
{
    NXOpen::Part* workPart = session_->Parts()->Work();
    NXOpen::Body* targetBody = face != nullptr ? face->GetBody() : nullptr;
    if (workPart == nullptr || targetBody == nullptr || thickness <= kTolerance)
    {
        error = "没有可用的工作部件、目标体或板厚。";
        return false;
    }

    NXOpen::Features::ExtrudeBuilder* extrude = nullptr;
    NXOpen::Features::ReplaceFaceBuilder* replace = nullptr;
    NXOpen::Features::BooleanBuilder* subtract = nullptr;
    std::string failureStage = "创建拉伸截面";
    try
    {
        std::vector<NXOpen::Edge*> outerEdges;
        if (!OuterBoundaryEdges(face, outerEdges))
        {
            throw std::runtime_error("无法取得选择面的外围边。");
        }

        std::vector<NXOpen::Face*> originalAdjacentFaces;
        originalAdjacentFaces.reserve(outerEdges.size());
        for (NXOpen::Edge* outerEdge : outerEdges)
        {
            NXOpen::Face* adjacent = AdjacentBodyFace(outerEdge, face, targetBody);
            if (adjacent == nullptr)
            {
                throw std::runtime_error("选择面外围边没有找到原体相邻面。");
            }
            originalAdjacentFaces.push_back(adjacent);
        }

        const std::vector<NXOpen::Edge*> boundaryEdges = face->GetEdges();
        if (boundaryEdges.empty() || boundaryEdges.front() == nullptr)
        {
            throw std::runtime_error("选择面没有可用的边界边。");
        }

        failureStage = "设置拉伸参数";
        extrude = workPart->Features()->CreateExtrudeBuilder(nullptr);
        NXOpen::Section* section =
            workPart->Sections()->CreateSection(0.000095, 0.0001, 0.5);
        extrude->SetSection(section);
        extrude->FeatureOptions()->SetBodyType(
            NXOpen::GeometricUtilities::FeatureOptions::BodyStyleSolid);
        extrude->BooleanOperation()->SetType(
            NXOpen::GeometricUtilities::BooleanOperation::BooleanTypeCreate);

        section->SetDistanceTolerance(0.0001);
        section->SetChainingTolerance(0.000095);
        section->SetAllowedEntityTypes(NXOpen::Section::AllowTypesOnlyCurves);

        NXOpen::SelectionIntentRuleOptions* boundaryOptions =
            workPart->ScRuleFactory()->CreateRuleOptions();
        boundaryOptions->SetSelectedFromInactive(false);
        NXOpen::EdgeBoundaryRule* boundaryRule =
            workPart->ScRuleFactory()->CreateRuleEdgeBoundary(
                std::vector<NXOpen::Face*>{face}, boundaryOptions);
        delete boundaryOptions;

        NXOpen::Point3d edgeStart;
        NXOpen::Point3d edgeEnd;
        boundaryEdges.front()->GetVertices(&edgeStart, &edgeEnd);
        const NXOpen::Point3d boundaryHelpPoint(
            (edgeStart.X + edgeEnd.X) * 0.5,
            (edgeStart.Y + edgeEnd.Y) * 0.5,
            (edgeStart.Z + edgeEnd.Z) * 0.5);
        section->AddToSection(
            std::vector<NXOpen::SelectionIntentRule*>{boundaryRule},
            boundaryEdges.front(),
            nullptr,
            nullptr,
            boundaryHelpPoint,
            NXOpen::Section::ModeCreate,
            false);

        NXOpen::Direction* direction = workPart->Directions()->CreateDirection(
            helpPoint, innerNormal,
            NXOpen::SmartObject::UpdateOptionWithinModeling);
        extrude->SetDirection(direction);
        extrude->Limits()->StartExtend()->SetValue("0");
        extrude->Limits()->EndExtend()->SetValue(Number(thickness).c_str());
        failureStage = "提交拉伸工具体";
        NXOpen::Features::Feature* toolFeature = extrude->CommitFeature();
        extrude->Destroy();
        extrude = nullptr;
        if (toolFeature == nullptr || toolFeature->GetBodies().empty())
        {
            throw std::runtime_error("NX 未返回拉伸工具体。");
        }
        NXOpen::Body* toolBody = toolFeature->GetBodies().front();
        if (createdFeatureTags != nullptr)
        {
            createdFeatureTags->push_back(toolFeature->Tag());
        }

        // Replace one matched pair at a time. A failed pair is skipped while
        // successful pairs remain in the tool body for the final subtraction.
        for (std::size_t index = 0; index < outerEdges.size(); ++index)
        {
            try
            {
                failureStage = "匹配第一对替换面";
                const std::vector<NXOpen::Face*> currentToolFaces =
                    toolBody->GetFaces();
                NXOpen::Face* extrudedSide =
                    ExtrudedSideFaceForEdge(
                        outerEdges[index], currentToolFaces, innerNormal);
                if (extrudedSide == nullptr)
                {
                    continue;
                }

                NXOpen::Face* originalAdjacent = originalAdjacentFaces[index];
                if (originalAdjacent == nullptr)
                {
                    continue;
                }

                failureStage = "逐对替换拉伸侧面";
                replace = workPart->Features()->CreateReplaceFaceBuilder(nullptr);
                replace->SetType(
                    NXOpen::Features::ReplaceFaceBuilder::ReplaceTypesReplace);
                replace->OffsetDistance()->SetFormula("0");
                replace->ResetReplaceFaceMethod();
                replace->ResetFreeEdgeProjectionOption();
                replace->SetReverseDirection(false);

                NXOpen::SelectionIntentRuleOptions* replaceOptions =
                    workPart->ScRuleFactory()->CreateRuleOptions();
                replaceOptions->SetSelectedFromInactive(false);
                NXOpen::FaceDumbRule* targetFaceRule =
                    workPart->ScRuleFactory()->CreateRuleFaceDumb(
                        std::vector<NXOpen::Face*>{extrudedSide}, replaceOptions);
                delete replaceOptions;
                replace->FaceToReplace()->ReplaceRules(
                    std::vector<NXOpen::SelectionIntentRule*>{targetFaceRule},
                    false);

                NXOpen::SelectionIntentRuleOptions* replacementOptions =
                    workPart->ScRuleFactory()->CreateRuleOptions();
                replacementOptions->SetSelectedFromInactive(false);
                NXOpen::FaceDumbRule* replacementFaceRule =
                    workPart->ScRuleFactory()->CreateRuleFaceDumb(
                        std::vector<NXOpen::Face*>{originalAdjacent},
                        replacementOptions);
                delete replacementOptions;
                replace->ReplacementFaces()->ReplaceRules(
                    std::vector<NXOpen::SelectionIntentRule*>{replacementFaceRule},
                    false);

                replace->OnApplyPre();
                NXOpen::Features::Feature* replaceFeature =
                    dynamic_cast<NXOpen::Features::Feature*>(replace->Commit());
                replace->Destroy();
                replace = nullptr;
                if (replaceFeature != nullptr &&
                    !replaceFeature->GetBodies().empty())
                {
                    toolBody = replaceFeature->GetBodies().front();
                    if (createdFeatureTags != nullptr)
                    {
                        createdFeatureTags->push_back(replaceFeature->Tag());
                    }
                }
            }
            catch (...)
            {
                if (replace != nullptr)
                {
                    replace->Destroy();
                    replace = nullptr;
                }
                // Deliberately silent: skip only this pair and continue.
            }
        }

        if (recolor && color >= 1 && color <= 216)
        {
            UF_OBJ_set_color(toolBody->Tag(), color);
        }

        failureStage = "设置布尔减";
        subtract = workPart->Features()->CreateBooleanBuilderUsingCollector(nullptr);
        subtract->SetOperation(NXOpen::Features::Feature::BooleanTypeSubtract);
        subtract->SetRetainTarget(false);
        subtract->SetRetainTool(true);

        NXOpen::SelectionIntentRuleOptions* targetOptions =
            workPart->ScRuleFactory()->CreateRuleOptions();
        targetOptions->SetSelectedFromInactive(false);
        NXOpen::BodyDumbRule* targetRule =
            workPart->ScRuleFactory()->CreateRuleBodyDumb(
                std::vector<NXOpen::Body*>{targetBody}, true, targetOptions);
        delete targetOptions;
        NXOpen::ScCollector* targetCollector =
            workPart->ScCollectors()->CreateCollector();
        targetCollector->ReplaceRules(
            std::vector<NXOpen::SelectionIntentRule*>{targetRule}, false);
        subtract->SetTargetBodyCollector(targetCollector);

        NXOpen::SelectionIntentRuleOptions* toolOptions =
            workPart->ScRuleFactory()->CreateRuleOptions();
        toolOptions->SetSelectedFromInactive(false);
        NXOpen::BodyDumbRule* toolRule =
            workPart->ScRuleFactory()->CreateRuleBodyDumb(
                std::vector<NXOpen::Body*>{toolBody}, true, toolOptions);
        delete toolOptions;
        NXOpen::ScCollector* toolCollector =
            workPart->ScCollectors()->CreateCollector();
        toolCollector->ReplaceRules(
            std::vector<NXOpen::SelectionIntentRule*>{toolRule}, false);
        subtract->SetToolBodyCollector(toolCollector);

        failureStage = "提交布尔减";
        NXOpen::NXObject* result = subtract->Commit();
        subtract->Destroy();
        subtract = nullptr;
        if (result == nullptr)
        {
            throw std::runtime_error("NX 未返回布尔减特征。");
        }
        if (createdFeatureTags != nullptr)
        {
            NXOpen::Features::Feature* booleanFeature =
                dynamic_cast<NXOpen::Features::Feature*>(result);
            if (booleanFeature != nullptr)
            {
                createdFeatureTags->push_back(booleanFeature->Tag());
            }
        }
        return true;
    }
    catch (const NXOpen::NXException& ex)
    {
        if (extrude != nullptr) extrude->Destroy();
        if (replace != nullptr) replace->Destroy();
        if (subtract != nullptr) subtract->Destroy();
        error = failureStage + "：" +
                (ex.Message() != nullptr ? ex.Message() : "NXOpen 建模失败。");
    }
    catch (const std::exception& ex)
    {
        if (extrude != nullptr) extrude->Destroy();
        if (replace != nullptr) replace->Destroy();
        if (subtract != nullptr) subtract->Destroy();
        error = failureStage + "：" + ex.what();
    }
    return false;
}

bool CaiRBanDialog::EstimateCylinderThickness(
    NXOpen::Body* body,
    NXOpen::Face* face,
    double& diameter,
    double& thickness,
    bool& reverseThicken,
    int wrapMode,
    std::string& error) const
{
    (void)wrapMode;
    diameter = 0.0;
    thickness = 0.0;
    reverseThicken = false;
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
            face->Tag(), &selectedType, selectedOriginData, selectedAxisData,
            selectedBox, &selectedRadius, &selectedRadiusData,
            &selectedNormalDirection) != 0 ||
        selectedType != UF_MODL_CYLINDRICAL_FACE ||
        selectedRadius <= kTolerance)
    {
        error = "无法读取所选圆柱面的轴线和直径。";
        return false;
    }

    NXOpen::Point3d selectedOrigin(selectedOriginData[0], selectedOriginData[1],
                                   selectedOriginData[2]);
    NXOpen::Vector3d selectedAxis(selectedAxisData[0], selectedAxisData[1],
                                  selectedAxisData[2]);
    if (!Normalize(selectedAxis))
    {
        error = "所选圆柱面的轴向无效。";
        return false;
    }
    diameter = selectedRadius * 2.0;

    double bestRadius = 0.0;
    double bestDifference = (std::numeric_limits<double>::max)();
    for (NXOpen::Face* candidate : body->GetFaces())
    {
        if (candidate == nullptr || candidate == face ||
            candidate->SolidFaceType() != NXOpen::Face::FaceTypeCylindrical)
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
        if (UF_MODL_ask_face_data(candidate->Tag(), &type, originData, axisData,
                                  box, &radius, &radiusData,
                                  &normalDirection) != 0 ||
            type != UF_MODL_CYLINDRICAL_FACE)
        {
            continue;
        }
        NXOpen::Vector3d axis(axisData[0], axisData[1], axisData[2]);
        if (!Normalize(axis) ||
            std::fabs(Dot(axis, selectedAxis)) < kParallelTolerance)
        {
            continue;
        }
        const NXOpen::Point3d origin(originData[0], originData[1], originData[2]);
        const NXOpen::Vector3d between = Subtract(origin, selectedOrigin);
        if (Length(Cross(between, selectedAxis)) >
            (std::max)(1.0e-3, selectedRadius * 1.0e-5))
        {
            continue;
        }

        const double radialDifference = radius - selectedRadius;
        if (std::fabs(radialDifference) > kTolerance &&
            std::fabs(radialDifference) < bestDifference)
        {
            bestDifference = std::fabs(radialDifference);
            bestRadius = radius;
        }
    }

    if (bestDifference == (std::numeric_limits<double>::max)())
    {
        error = "未找到与所选圆柱面同轴的另一圆柱面，无法获得板厚。";
        return false;
    }

    thickness = bestDifference;
    const double desiredRadialSign = bestRadius > selectedRadius ? 1.0 : -1.0;
    // UF face-data normal_direction is the radial surface-normal sense.
    // Thicken's positive first offset follows that face normal.
    reverseThicken = desiredRadialSign *
                         static_cast<double>(selectedNormalDirection) <
                     0.0;
    return true;
}

bool CaiRBanDialog::CreateArcPanel(
    NXOpen::Face* face,
    double thickness,
    bool reverseThicken,
    double extensionLength,
    int wrapMode,
    std::string& error,
    bool recolor,
    int color,
    std::vector<tag_t>* createdFeatureTags) const
{
    NXOpen::Part* workPart = session_->Parts()->Work();
    NXOpen::Body* sourceBody = face != nullptr ? face->GetBody() : nullptr;
    if (workPart == nullptr || sourceBody == nullptr ||
        thickness <= kTolerance)
    {
        error = "没有可用的工作部件、目标体或板厚。";
        return false;
    }

    int cylinderType = 0;
    double cylinderOriginData[3] = {};
    double cylinderAxisData[3] = {};
    double cylinderBox[6] = {};
    double cylinderRadius = 0.0;
    double cylinderRadiusData = 0.0;
    int cylinderNormalDirection = 1;
    if (UF_MODL_ask_face_data(
            face->Tag(), &cylinderType, cylinderOriginData, cylinderAxisData,
            cylinderBox, &cylinderRadius, &cylinderRadiusData,
            &cylinderNormalDirection) != 0 ||
        cylinderType != UF_MODL_CYLINDRICAL_FACE)
    {
        error = "无法读取圆柱面数据。";
        return false;
    }
    NXOpen::Vector3d cylinderAxis(cylinderAxisData[0], cylinderAxisData[1],
                                  cylinderAxisData[2]);
    if (!Normalize(cylinderAxis))
    {
        error = "圆柱面的轴向无效。";
        return false;
    }

    struct TangentEnd
    {
        NXOpen::Face* tangentFace;
        NXOpen::Edge* tangentEdge;
        NXOpen::Point3d edgeMidpoint;
        NXOpen::Vector3d awayDirection;
    };
    std::vector<NXOpen::Face*> facesToThicken{face};
    std::vector<TangentEnd> tangentEnds;
    if (extensionLength > kTolerance)
    {
        for (NXOpen::Edge* edge : face->GetEdges())
        {
            if (edge == nullptr)
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
            NXOpen::Vector3d edgeDirection = Subtract(second, first);
            const double edgeLength = Length(edgeDirection);
            if (edgeLength <= kTolerance)
            {
                continue;
            }
            edgeDirection.X /= edgeLength;
            edgeDirection.Y /= edgeLength;
            edgeDirection.Z /= edgeLength;
            if (std::fabs(Dot(edgeDirection, cylinderAxis)) <
                kParallelTolerance)
            {
                continue;
            }

            NXOpen::Face* tangentFace =
                AdjacentBodyFace(edge, face, sourceBody);
            NXOpen::Point3d planePoint;
            NXOpen::Vector3d planeNormal;
            if (tangentFace == nullptr ||
                !FacePlaneData(tangentFace, planePoint, planeNormal) ||
                std::fabs(Dot(planeNormal, cylinderAxis)) > 0.01)
            {
                continue;
            }
            NXOpen::Vector3d away = Cross(cylinderAxis, planeNormal);
            if (!Normalize(away))
            {
                continue;
            }
            const NXOpen::Point3d midpoint((first.X + second.X) * 0.5,
                                           (first.Y + second.Y) * 0.5,
                                           (first.Z + second.Z) * 0.5);

            // Pick the tangent direction that actually enters the adjacent
            // planar face by inspecting its vertices away from the shared edge.
            double strongestProjection = 0.0;
            for (NXOpen::Edge* adjacentEdge : tangentFace->GetEdges())
            {
                NXOpen::Point3d a;
                NXOpen::Point3d b;
                try
                {
                    adjacentEdge->GetVertices(&a, &b);
                }
                catch (...)
                {
                    continue;
                }
                for (const NXOpen::Point3d& point : {a, b})
                {
                    const double projection = Dot(Subtract(point, midpoint), away);
                    if (std::fabs(projection) > std::fabs(strongestProjection))
                    {
                        strongestProjection = projection;
                    }
                }
            }
            if (strongestProjection < 0.0)
            {
                away.X = -away.X;
                away.Y = -away.Y;
                away.Z = -away.Z;
            }
            if (std::fabs(strongestProjection) <= kTolerance)
            {
                continue;
            }
            if (std::find(facesToThicken.begin(), facesToThicken.end(),
                          tangentFace) == facesToThicken.end())
            {
                facesToThicken.push_back(tangentFace);
                tangentEnds.push_back(
                    {tangentFace, edge, midpoint, away});
            }
        }
        if (tangentEnds.size() != 2)
        {
            error = "延伸模式要求圆弧两端各连接一个相切平面，当前未能准确找到两个相切面。";
            return false;
        }
    }

    NXOpen::Features::ThickenBuilder* thicken = nullptr;
    NXOpen::Features::TrimBody2Builder* trim = nullptr;
    NXOpen::Features::OffsetFaceBuilder* innerOffset = nullptr;
    std::string stage = "创建圆弧板加厚";
    try
    {
        thicken = workPart->Features()->CreateThickenBuilder(nullptr);
        NXOpen::SelectionIntentRuleOptions* faceOptions =
            workPart->ScRuleFactory()->CreateRuleOptions();
        faceOptions->SetSelectedFromInactive(false);
        NXOpen::FaceDumbRule* faceRule =
            workPart->ScRuleFactory()->CreateRuleFaceDumb(
                facesToThicken, faceOptions);
        delete faceOptions;
        thicken->FaceCollector()->ReplaceRules(
            std::vector<NXOpen::SelectionIntentRule*>{faceRule}, false);
        thicken->BooleanOperation()->SetType(
            NXOpen::GeometricUtilities::BooleanOperation::BooleanTypeCreate);
        thicken->FirstOffset()->SetFormula(Number(thickness).c_str());
        thicken->SecondOffset()->SetFormula("0");
        thicken->SetReverseDirection(reverseThicken);
        thicken->SetTolerance(0.001);
        NXOpen::Features::Feature* thickenFeature = thicken->CommitFeature();
        thicken->Destroy();
        thicken = nullptr;
        if (thickenFeature == nullptr || thickenFeature->GetBodies().empty())
        {
            throw std::runtime_error("NX 未返回加厚实体。");
        }
        NXOpen::Body* panelBody = thickenFeature->GetBodies().front();
        if (createdFeatureTags != nullptr)
        {
            createdFeatureTags->push_back(thickenFeature->Tag());
        }

        for (std::size_t index = 0; index < tangentEnds.size(); ++index)
        {
            stage = "创建并执行第 " + std::to_string(index + 1) +
                    " 个修剪平面";
            const TangentEnd& end = tangentEnds[index];
            const NXOpen::Point3d planeOrigin =
                Move(end.edgeMidpoint, end.awayDirection, extensionLength);
            NXOpen::Plane* plane = workPart->Planes()->CreatePlane(
                planeOrigin, end.awayDirection,
                NXOpen::SmartObject::UpdateOptionWithinModeling);

            trim = workPart->Features()->CreateTrimBody2Builder(nullptr);
            NXOpen::SelectionIntentRuleOptions* targetOptions =
                workPart->ScRuleFactory()->CreateRuleOptions();
            targetOptions->SetSelectedFromInactive(false);
            NXOpen::BodyDumbRule* targetRule =
                workPart->ScRuleFactory()->CreateRuleBodyDumb(
                    std::vector<NXOpen::Body*>{panelBody}, true,
                    targetOptions);
            delete targetOptions;
            NXOpen::ScCollector* targetCollector =
                workPart->ScCollectors()->CreateCollector();
            targetCollector->ReplaceRules(
                std::vector<NXOpen::SelectionIntentRule*>{targetRule}, false);
            trim->SetTargetBodyCollector(targetCollector);
            trim->BooleanTool()->SetToolOption(
                NXOpen::GeometricUtilities::BooleanToolBuilder::
                    BooleanToolTypeNewPlane);
            trim->BooleanTool()->FacePlaneTool()->SetToolPlane(plane);
            // Plane normal points from the arc toward the tangent flange, so
            // the default side retains the arc and cuts the excess extension.
            trim->BooleanTool()->SetReverseDirection(false);
            trim->SetTolerance(0.001);
            NXOpen::Features::Feature* trimFeature = trim->CommitFeature();
            trim->Destroy();
            trim = nullptr;
            if (trimFeature == nullptr || trimFeature->GetBodies().empty())
            {
                throw std::runtime_error("NX 未返回修剪后的圆弧板实体。");
            }
            panelBody = trimFeature->GetBodies().front();
            if (createdFeatureTags != nullptr)
            {
                createdFeatureTags->push_back(trimFeature->Tag());
            }
        }

        // 外包：加厚体完成修剪后直接结束。
        // 内包：将与同轴圆弧边相连的端部平面，沿实体内法向
        // 偏置一个板厚。对两端平面同时使用负偏置，即各自沿
        // 其外法向的反方向移动。
        if (wrapMode == 1)
        {
            stage = "内包端面沿内法向偏置一个板厚";
            std::vector<NXOpen::Face*> inwardOffsetFaces;
            const NXOpen::Point3d cylinderOrigin(
                cylinderOriginData[0], cylinderOriginData[1],
                cylinderOriginData[2]);
            for (NXOpen::Face* candidateFace : panelBody->GetFaces())
            {
                NXOpen::Point3d planePoint;
                NXOpen::Vector3d planeNormal;
                if (candidateFace == nullptr ||
                    !FacePlaneData(candidateFace, planePoint, planeNormal) ||
                    std::fabs(Dot(planeNormal, cylinderAxis)) <
                        kParallelTolerance)
                {
                    continue;
                }

                bool connectedToCoaxialArc = false;
                for (NXOpen::Edge* candidateEdge : candidateFace->GetEdges())
                {
                    if (candidateEdge == nullptr ||
                        candidateEdge->SolidEdgeType() !=
                            NXOpen::Edge::EdgeTypeCircular)
                    {
                        continue;
                    }
                    for (NXOpen::Face* adjacentFace :
                         candidateEdge->GetUnsortedFaces())
                    {
                        if (adjacentFace == nullptr ||
                            adjacentFace == candidateFace ||
                            adjacentFace->SolidFaceType() !=
                                NXOpen::Face::FaceTypeCylindrical)
                        {
                            continue;
                        }
                        int adjacentType = 0;
                        double adjacentOriginData[3] = {};
                        double adjacentAxisData[3] = {};
                        double adjacentBox[6] = {};
                        double adjacentRadius = 0.0;
                        double adjacentRadiusData = 0.0;
                        int adjacentNormalDirection = 1;
                        if (UF_MODL_ask_face_data(
                                adjacentFace->Tag(), &adjacentType,
                                adjacentOriginData, adjacentAxisData,
                                adjacentBox, &adjacentRadius,
                                &adjacentRadiusData,
                                &adjacentNormalDirection) != 0 ||
                            adjacentType != UF_MODL_CYLINDRICAL_FACE)
                        {
                            continue;
                        }
                        NXOpen::Vector3d adjacentAxis(
                            adjacentAxisData[0], adjacentAxisData[1],
                            adjacentAxisData[2]);
                        const NXOpen::Point3d adjacentOrigin(
                            adjacentOriginData[0], adjacentOriginData[1],
                            adjacentOriginData[2]);
                        if (Normalize(adjacentAxis) &&
                            std::fabs(Dot(adjacentAxis, cylinderAxis)) >=
                                kParallelTolerance &&
                            Length(Cross(Subtract(adjacentOrigin,
                                                  cylinderOrigin),
                                         cylinderAxis)) <=
                                (std::max)(1.0e-3,
                                           cylinderRadius * 1.0e-5))
                        {
                            connectedToCoaxialArc = true;
                            break;
                        }
                    }
                    if (connectedToCoaxialArc)
                    {
                        break;
                    }
                }
                if (connectedToCoaxialArc)
                {
                    inwardOffsetFaces.push_back(candidateFace);
                }
            }

            if (inwardOffsetFaces.empty())
            {
                throw std::runtime_error(
                    "未找到与加厚体圆弧边连接的端部平面。");
            }

            innerOffset =
                workPart->Features()->CreateOffsetFaceBuilder(nullptr);
            innerOffset->Distance()->SetFormula(
                ("-(" + Number(thickness) + ")").c_str());
            innerOffset->SetDirection(false);
            NXOpen::SelectionIntentRuleOptions* offsetOptions =
                workPart->ScRuleFactory()->CreateRuleOptions();
            offsetOptions->SetSelectedFromInactive(false);
            NXOpen::FaceDumbRule* offsetRule =
                workPart->ScRuleFactory()->CreateRuleFaceDumb(
                    inwardOffsetFaces, offsetOptions);
            delete offsetOptions;
            innerOffset->FaceCollector()->ReplaceRules(
                std::vector<NXOpen::SelectionIntentRule*>{offsetRule}, false);
            NXOpen::Features::Feature* offsetFeature =
                innerOffset->CommitFeature();
            innerOffset->Destroy();
            innerOffset = nullptr;
            if (offsetFeature == nullptr || offsetFeature->GetBodies().empty())
            {
                throw std::runtime_error(
                    "NX 未返回内包端面偏置后的实体。");
            }
            panelBody = offsetFeature->GetBodies().front();
            if (createdFeatureTags != nullptr)
            {
                createdFeatureTags->push_back(offsetFeature->Tag());
            }
        }

        if (recolor && color >= 1 && color <= 216)
        {
            UF_OBJ_set_color(panelBody->Tag(), color);
        }
        return true;
    }
    catch (const NXOpen::NXException& ex)
    {
        if (thicken != nullptr) thicken->Destroy();
        if (trim != nullptr) trim->Destroy();
        if (innerOffset != nullptr) innerOffset->Destroy();
        error = stage + "：" +
                (ex.Message() != nullptr ? ex.Message() : "NXOpen 建模失败。");
    }
    catch (const std::exception& ex)
    {
        if (thicken != nullptr) thicken->Destroy();
        if (trim != nullptr) trim->Destroy();
        if (innerOffset != nullptr) innerOffset->Destroy();
        error = stage + "：" + ex.what();
    }
    return false;
}

int CaiRBanDialog::Execute()
{
    NXOpen::Face* face = SelectedFace();
    if (face == nullptr)
    {
        ShowError("请选择一个圆柱面。");
        return 1;
    }

    const NXOpen::Session::UndoMarkId mark =
        session_->SetUndoMark(NXOpen::Session::MarkVisibilityVisible, "拆圆弧板");
    std::string error;
    double diameter = 0.0;
    double thickness = 0.0;
    bool reverseThicken = false;
    if (!EstimateCylinderThickness(face->GetBody(), face, diameter, thickness,
                                   reverseThicken, WrapMode(), error) ||
        !CreateArcPanel(face, thickness, reverseThicken, ExtensionLength(),
                        WrapMode(), error, RecolorEnabled(), RequestedColor(),
                        nullptr))
    {
        session_->UndoToMark(mark, "拆圆弧板失败");
        session_->DeleteUndoMark(mark, "拆圆弧板失败");
        ShowError(error);
        return 1;
    }
    session_->SetUndoMarkName(mark, "拆圆弧板");
    return 0;
}

void CaiRBanDialog::ShowError(const std::string& message) const
{
    ui_->NXMessageBox()->Show("拆圆弧板",
                              NXOpen::NXMessageBox::DialogTypeError,
                              message.c_str());
}

extern "C" __declspec(dllexport) int ZhihuiCaiRBanBuildCustomFeature(
    void* eventPointer)
{
    if (gActiveCaiRBanDialog == nullptr || eventPointer == nullptr)
    {
        return 1;
    }
    return gActiveCaiRBanDialog->BuildCustomFeatureConstruction(
        static_cast<NXOpen::Features::CustomFeaturePreUpdateEvent*>(
            eventPointer));
}
