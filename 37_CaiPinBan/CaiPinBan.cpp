#include "CaiPinBan.hpp"
#include "CaiPinBanCustomFeatureShared.hpp"
#include "../../common/ZhihuiDialogMemory.hpp"
#include "../../common/ZhihuiContextHelp.hpp"
#ifdef CreateDialog
#undef CreateDialog
#endif

#include <NXOpen/BlockStyler_PropertyList.hxx>
#include <NXOpen/Body.hxx>
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
#include <NXOpen/Features_ReplaceFaceBuilder.hxx>
#include <NXOpen/GeometricUtilities_BooleanOperation.hxx>
#include <NXOpen/GeometricUtilities_Extend.hxx>
#include <NXOpen/GeometricUtilities_FeatureOptions.hxx>
#include <NXOpen/GeometricUtilities_Limits.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXMessageBox.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
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
CaiPinBanDialog* gActiveCaiPinBanDialog = nullptr;

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
            std::filesystem::path(modulePath).parent_path() / L"CaiPinBan.dlx";
        if (std::filesystem::exists(path))
        {
            return path.string();
        }
    }
    return "CaiPinBan.dlx";
}

std::string DialogPathWithHelp()
{
    zhihui_context_help::EnsureGlobalHelpLoaded();
    return DialogPath();
}

std::string Number(double value)
{
    std::ostringstream stream;
    stream.precision(15);
    stream << value;
    return stream.str();
}
}

CaiPinBanDialog::CaiPinBanDialog()
    : ui_(NXOpen::UI::GetUI()),
      session_(NXOpen::Session::GetSession()),
      dialog_(ui_->CreateDialog(DialogPathWithHelp().c_str())),
      recolorToggle_(nullptr),
      colorMode_(nullptr),
      fixedColor_(nullptr),
      faceSelect_(nullptr),
      customFeatureManager_(nullptr),
      editedFeature_(nullptr),
      featureClass_(nullptr),
      previewUndoMark_(static_cast<NXOpen::Session::UndoMarkId>(0)),
      hasPreview_(false),
      loadingEditedFeature_(false),
      buildingCustomFeature_(false),
      previewColor_(0),
      previewTargetBodyTag_(NULL_TAG),
      previewSelectedFaceTag_(NULL_TAG),
      previewCreatedFeatureTags_()
{
    customFeatureManager_ = session_->CustomFeatureClassManager();
    editedFeature_ = customFeatureManager_->GetEditedCustomFeature();
    try
    {
        featureClass_ = customFeatureManager_->GetClassFromName(
            zhihui_caipinban::kFeatureClassName);
    }
    catch (...)
    {
        featureClass_ = nullptr;
    }
    gActiveCaiPinBanDialog = this;
    dialog_->AddInitializeHandler(NXOpen::make_callback(this, &CaiPinBanDialog::initialize_cb));
    dialog_->AddDialogShownHandler(NXOpen::make_callback(this, &CaiPinBanDialog::dialogShown_cb));
    dialog_->AddUpdateHandler(NXOpen::make_callback(this, &CaiPinBanDialog::update_cb));
    dialog_->AddApplyHandler(NXOpen::make_callback(this, &CaiPinBanDialog::apply_cb));
    dialog_->AddOkHandler(NXOpen::make_callback(this, &CaiPinBanDialog::ok_cb));
    dialog_->AddCancelHandler(NXOpen::make_callback(this, &CaiPinBanDialog::cancel_cb));
}

CaiPinBanDialog::~CaiPinBanDialog()
{
    if (gActiveCaiPinBanDialog == this)
    {
        gActiveCaiPinBanDialog = nullptr;
    }
    delete dialog_;
}

NXOpen::BlockStyler::BlockDialog::DialogResponse CaiPinBanDialog::Launch()
{
    return dialog_->LaunchInDialogMode(
        editedFeature_ != nullptr
            ? NXOpen::BlockStyler::BlockDialog::DialogModeEdit
            : NXOpen::BlockStyler::BlockDialog::DialogModeCreate);
}

void CaiPinBanDialog::initialize_cb()
{
    recolorToggle_ = dialog_->TopBlock()->FindBlock("recolor_toggle");
    colorMode_ = dialog_->TopBlock()->FindBlock("color_mode");
    fixedColor_ = dynamic_cast<NXOpen::BlockStyler::ObjectColorPicker*>(
        dialog_->TopBlock()->FindBlock("fixed_color"));
    faceSelect_ = dialog_->TopBlock()->FindBlock("face_select");
    if (recolorToggle_ == nullptr || colorMode_ == nullptr ||
        fixedColor_ == nullptr || faceSelect_ == nullptr)
    {
        throw std::runtime_error("CaiPinBan.dlx is missing a required block.");
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
                       UF_UI_SEL_FEATURE_ANY_FACE);
    properties->SetSelectionFilter(
        "SelectionFilter",
        NXOpen::Selection::SelectionActionClearAndEnableSpecific,
        masks);
    properties->SetEnum("StepStatus", 0);
    delete properties;

    LoadEditedCustomFeatureData();
}

void CaiPinBanDialog::dialogShown_cb()
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

int CaiPinBanDialog::update_cb(NXOpen::BlockStyler::UIBlock* block)
{
    if (loadingEditedFeature_)
    {
        return 0;
    }
    if (block == recolorToggle_ || block == colorMode_ || block == fixedColor_)
    {
        UpdateColorControlVisibility();
        SaveDialogState();
    }
    if (block == faceSelect_)
    {
        if (hasPreview_)
        {
            UndoPreview();
        }
        if (SelectedFace() != nullptr)
        {
            return CreatePreview();
        }
    }
    return 0;
}

void CaiPinBanDialog::UpdateColorControlVisibility() const
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

bool CaiPinBanDialog::RecolorEnabled() const
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

int CaiPinBanDialog::RequestedColor() const
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

int CaiPinBanDialog::ColorMode() const
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

int CaiPinBanDialog::FixedColor() const
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

void CaiPinBanDialog::LoadDialogState() const
{
    constexpr const wchar_t* fileName = L"CaiPinBan_state.ini";
    zhihui_dialog_memory::LoadLogical(
        fileName, L"recolorEnabled", recolorToggle_);
    zhihui_dialog_memory::LoadEnum(fileName, L"colorMode", colorMode_);
    const int color = (std::max)(
        1,
        (std::min)(216,
                   zhihui_dialog_memory::ReadInt(
                       fileName, L"fixedColor", FixedColor())));
    fixedColor_->SetValue(std::vector<int>{color});
}

void CaiPinBanDialog::SaveDialogState() const
{
    constexpr const wchar_t* fileName = L"CaiPinBan_state.ini";
    zhihui_dialog_memory::SaveLogical(
        fileName, L"recolorEnabled", recolorToggle_);
    zhihui_dialog_memory::SaveEnum(fileName, L"colorMode", colorMode_);
    zhihui_dialog_memory::WriteInt(
        fileName, L"fixedColor", FixedColor());
}

void CaiPinBanDialog::LoadEditedCustomFeatureData()
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
                        zhihui_caipinban::kAttrSelectedFace)
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
                    zhihui_caipinban::kAttrRecolorEnabled)
                    ->Value() > 0.5);
        delete toggleProperties;

        NXOpen::BlockStyler::PropertyList* modeProperties =
            colorMode_->GetProperties();
        modeProperties->SetEnum(
            "Value",
            static_cast<int>(data->CustomDoubleAttributeByName(
                zhihui_caipinban::kAttrColorMode)->Value()));
        delete modeProperties;

        const int fixedColor = static_cast<int>(
            data->CustomDoubleAttributeByName(
                    zhihui_caipinban::kAttrFixedColor)
                ->Value());
        fixedColor_->SetValue(std::vector<int>{
            (std::max)(1, (std::min)(216, fixedColor))});
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

bool CaiPinBanDialog::CommitEditableCustomFeature(std::string& error)
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
                    ? "拆平板可编辑特征未注册，部署后需重启 UG。"
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
                zhihui_caipinban::kAttrTargetBody, targetProperties));
            values.push_back(attributes->CreateCustomTagAttribute(
                zhihui_caipinban::kAttrSelectedFace,
                std::vector<NXOpen::Features::CustomAttribute::Property>()));
            const std::vector<NXOpen::Features::CustomAttribute::Property> optional;
            values.push_back(attributes->CreateCustomDoubleAttribute(
                zhihui_caipinban::kAttrRecolorEnabled, optional));
            values.push_back(attributes->CreateCustomDoubleAttribute(
                zhihui_caipinban::kAttrColorMode, optional));
            values.push_back(attributes->CreateCustomDoubleAttribute(
                zhihui_caipinban::kAttrFixedColor, optional));
            values.push_back(attributes->CreateCustomDoubleAttribute(
                zhihui_caipinban::kAttrAppliedColor, optional));
            data = workPart->Features()->CustomFeatureDataCollection()->CreateData(
                featureClass_, values);
            data->CustomTagAttributeByName(
                zhihui_caipinban::kAttrTargetBody)
                ->SetValue(targetBody);
            if (previewSelectedFaceTag_ != NULL_TAG &&
                UF_OBJ_ask_status(previewSelectedFaceTag_) == UF_OBJ_ALIVE)
            {
                data->CustomTagAttributeByName(
                        zhihui_caipinban::kAttrSelectedFace)
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
                zhihui_caipinban::kAttrRecolorEnabled)
            ->SetValue(RecolorEnabled() ? 1.0 : 0.0);
        data->CustomDoubleAttributeByName(zhihui_caipinban::kAttrColorMode)
            ->SetValue(static_cast<double>(ColorMode()));
        data->CustomDoubleAttributeByName(zhihui_caipinban::kAttrFixedColor)
            ->SetValue(static_cast<double>(FixedColor()));
        data->CustomDoubleAttributeByName(zhihui_caipinban::kAttrAppliedColor)
            ->SetValue(static_cast<double>(appliedColor));

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
            error = "NX 未返回拆平板自定义特征。";
            return false;
        }
        customFeature->SetName(zhihui_caipinban::kFeatureDisplayName);
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

int CaiPinBanDialog::BuildCustomFeatureConstruction(
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
                zhihui_caipinban::kAttrRecolorEnabled)
                ->Value() > 0.5;
    const int color = static_cast<int>(
        data->CustomDoubleAttributeByName(
                zhihui_caipinban::kAttrAppliedColor)
            ->Value());
    if (recolor && color >= 1 && color <= 216 && construction.size() >= 2)
    {
        NXOpen::Features::Feature* toolFeature =
            construction[construction.size() - 2]->GetFeature();
        if (toolFeature != nullptr && !toolFeature->GetBodies().empty())
        {
            UF_OBJ_set_color(toolFeature->GetBodies().front()->Tag(), color);
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

int CaiPinBanDialog::apply_cb()
{
    SaveDialogState();
    const bool editing = editedFeature_ != nullptr;
    if (editedFeature_ == nullptr && !hasPreview_ && SelectedFace() == nullptr)
    {
        ShowError("请选择一个平面。");
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
    }
    return 0;
}

int CaiPinBanDialog::ok_cb()
{
    return apply_cb();
}

int CaiPinBanDialog::cancel_cb()
{
    UndoPreview();
    return 0;
}

int CaiPinBanDialog::CreatePreview()
{
    NXOpen::Face* face = SelectedFace();
    if (face == nullptr)
    {
        return 0;
    }

    previewUndoMark_ = session_->SetUndoMark(
        NXOpen::Session::MarkVisibilityInvisible,
        "拆平板预览");
    previewTargetBodyTag_ = face->GetBody()->Tag();
    previewSelectedFaceTag_ = face->Tag();
    std::string error;
    double thickness = 0.0;
    NXOpen::Vector3d innerNormal;
    NXOpen::Point3d helpPoint;
    previewCreatedFeatureTags_.clear();
    previewColor_ = RecolorEnabled() ? RequestedColor() : 0;
    if (!EstimateThicknessAndInnerNormal(face->GetBody(), face, thickness,
                                         innerNormal, helpPoint, error) ||
        !CreateAndSubtract(face, thickness, innerNormal, helpPoint, error,
                           RecolorEnabled(), previewColor_,
                           &previewCreatedFeatureTags_))
    {
        UndoPreview();
        ShowError(error);
        return 1;
    }
    hasPreview_ = true;
    return 0;
}

void CaiPinBanDialog::UndoPreview()
{
    if (previewUndoMark_ == static_cast<NXOpen::Session::UndoMarkId>(0))
    {
        hasPreview_ = false;
        return;
    }
    try
    {
        if (session_->DoesUndoMarkExist(previewUndoMark_, "拆平板预览"))
        {
            session_->UndoToMark(previewUndoMark_, "拆平板预览");
            if (session_->DoesUndoMarkExist(previewUndoMark_, "拆平板预览"))
            {
                session_->DeleteUndoMark(previewUndoMark_, "拆平板预览");
            }
        }
    }
    catch (...)
    {
    }
    previewUndoMark_ = static_cast<NXOpen::Session::UndoMarkId>(0);
    hasPreview_ = false;
    previewCreatedFeatureTags_.clear();
    previewColor_ = 0;
    previewTargetBodyTag_ = NULL_TAG;
    previewSelectedFaceTag_ = NULL_TAG;
}

void CaiPinBanDialog::CommitPreview()
{
    if (!hasPreview_ ||
        previewUndoMark_ == static_cast<NXOpen::Session::UndoMarkId>(0))
    {
        return;
    }
    if (session_->DoesUndoMarkExist(previewUndoMark_, "拆平板预览"))
    {
        session_->SetUndoMarkName(previewUndoMark_, "拆平板");
        session_->SetUndoMarkVisibility(
            previewUndoMark_, "拆平板",
            NXOpen::Session::MarkVisibilityVisible);
    }
    previewUndoMark_ = static_cast<NXOpen::Session::UndoMarkId>(0);
    hasPreview_ = false;
}

NXOpen::Face* CaiPinBanDialog::SelectedFace() const
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

bool CaiPinBanDialog::EstimateThicknessAndInnerNormal(
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

bool CaiPinBanDialog::CreateAndSubtract(
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
            // Replace Face is only meaningful here when the face connected to
            // the selected-face boundary is planar.  Preserve the one-to-one
            // edge indexing with a null entry so curved adjacent faces are
            // simply excluded while the final subtraction still proceeds.
            originalAdjacentFaces.push_back(
                adjacent->SolidFaceType() == NXOpen::Face::FaceTypePlanar
                    ? adjacent
                    : nullptr);
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
                NXOpen::Face* originalAdjacent = originalAdjacentFaces[index];
                if (originalAdjacent == nullptr)
                {
                    continue;
                }

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

int CaiPinBanDialog::Execute()
{
    NXOpen::Face* face = SelectedFace();
    if (face == nullptr)
    {
        ShowError("请选择一个平面。");
        return 1;
    }

    const NXOpen::Session::UndoMarkId mark =
        session_->SetUndoMark(NXOpen::Session::MarkVisibilityVisible, "拆平板");
    std::string error;
    double thickness = 0.0;
    NXOpen::Vector3d innerNormal;
    NXOpen::Point3d helpPoint;
    if (!EstimateThicknessAndInnerNormal(face->GetBody(), face, thickness,
                                         innerNormal, helpPoint, error) ||
        !CreateAndSubtract(face, thickness, innerNormal, helpPoint, error,
                           RecolorEnabled(), RequestedColor(), nullptr))
    {
        session_->UndoToMark(mark, "拆平板失败");
        session_->DeleteUndoMark(mark, "拆平板失败");
        ShowError(error);
        return 1;
    }
    session_->SetUndoMarkName(mark, "拆平板");
    return 0;
}

void CaiPinBanDialog::ShowError(const std::string& message) const
{
    ui_->NXMessageBox()->Show("拆平板",
                              NXOpen::NXMessageBox::DialogTypeError,
                              message.c_str());
}

extern "C" __declspec(dllexport) int ZhihuiCaiPinBanBuildCustomFeature(
    void* eventPointer)
{
    if (gActiveCaiPinBanDialog == nullptr || eventPointer == nullptr)
    {
        return 1;
    }
    return gActiveCaiPinBanDialog->BuildCustomFeatureConstruction(
        static_cast<NXOpen::Features::CustomFeaturePreUpdateEvent*>(
            eventPointer));
}
