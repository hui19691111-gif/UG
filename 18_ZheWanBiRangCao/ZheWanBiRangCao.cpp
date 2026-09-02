#include "ZheWanBiRangCao.hpp"
#include "../../common/ZhihuiDialogMemory.hpp"
#include "../../common/ZhihuiEmbeddedDialog.hpp"
#include "embedded_dialog_resources.h"
#include "ZheWanBiRangCaoCustomFeatureShared.hpp"

#ifdef CreateDialog
#undef CreateDialog
#endif

#include <NXOpen/BlockStyler_PropertyList.hxx>
#include <stdexcept>
#include <NXOpen/BasePart.hxx>
#include <NXOpen/CurveDumbRule.hxx>
#include <NXOpen/CartesianCoordinateSystem.hxx>
#include <NXOpen/CoordinateSystemCollection.hxx>
#include <NXOpen/Direction.hxx>
#include <NXOpen/DirectionCollection.hxx>
#include <NXOpen/BodyDumbRule.hxx>
#include <NXOpen/BodyCollection.hxx>
#include <NXOpen/Features_BooleanBuilder.hxx>
#include <NXOpen/Features_BaseFeatureCollection.hxx>
#include <NXOpen/Features_CustomAttribute.hxx>
#include <NXOpen/Features_CustomAttributeCollection.hxx>
#include <NXOpen/Features_CustomDoubleArrayAttribute.hxx>
#include <NXOpen/Features_CustomDoubleAttribute.hxx>
#include <NXOpen/Features_CustomFeatureBuilder.hxx>
#include <NXOpen/Features_CustomFeatureData.hxx>
#include <NXOpen/Features_CustomFeatureDataCollection.hxx>
#include <NXOpen/Features_CustomTagAttribute.hxx>
#include <NXOpen/Features_DatumCsys.hxx>
#include <NXOpen/Features_DatumCsysBuilder.hxx>
#include <NXOpen/Features_BooleanFeature.hxx>
#include <NXOpen/Features_ExtrudeBuilder.hxx>
#include <NXOpen/Features_Feature.hxx>
#include <NXOpen/Features_FeatureCollection.hxx>
#include <NXOpen/Features_MoveObject.hxx>
#include <NXOpen/Features_MoveObjectBuilder.hxx>
#include <NXOpen/GeometricUtilities_BooleanOperation.hxx>
#include <NXOpen/GeometricUtilities_Extend.hxx>
#include <NXOpen/GeometricUtilities_FeatureOptions.hxx>
#include <NXOpen/GeometricUtilities_Limits.hxx>
#include <NXOpen/GeometricUtilities_ModlMotion.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXMessageBox.hxx>
#include <NXOpen/NXObject.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/Point.hxx>
#include <NXOpen/PointCollection.hxx>
#include <NXOpen/MeasureFaces.hxx>
#include <NXOpen/MeasureManager.hxx>
#include <NXOpen/ScCollector.hxx>
#include <NXOpen/ScCollectorCollection.hxx>
#include <NXOpen/ScRuleFactory.hxx>
#include <NXOpen/SelectObject.hxx>
#include <NXOpen/SelectNXObjectList.hxx>
#include <NXOpen/SelectionIntentRule.hxx>
#include <NXOpen/SelectionIntentRuleOptions.hxx>
#include <NXOpen/Section.hxx>
#include <NXOpen/SectionCollection.hxx>
#include <NXOpen/SmartObject.hxx>
#include <NXOpen/UI.hxx>
#include <NXOpen/UnitCollection.hxx>

#include <uf.h>
#include <uf_curve.h>
#include <uf_modl.h>
#include <uf_modl_datum_features.h>
#include <uf_modl_sweep.h>
#include <uf_modl_udf.h>
#include <uf_mtx.h>
#include <uf_obj.h>
#include <uf_part.h>
#include <uf_trns.h>
#include <uf_ui_types.h>

#include <windows.h>

#include <cmath>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <set>
#include <string>
#include <vector>

namespace
{
constexpr double kVectorTolerance = 1.0e-6;
std::string FormatDouble(double value);

class ExtractedReliefUdfTemplate
{
public:
    ExtractedReliefUdfTemplate() = default;
    ~ExtractedReliefUdfTemplate()
    {
        std::error_code error;
        if (!file_.empty())
        {
            std::filesystem::remove(file_, error);
        }
        error.clear();
        if (!directory_.empty())
        {
            std::filesystem::remove(directory_, error);
        }
    }

    bool Extract(std::string& path)
    {
        HMODULE module = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                   GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               reinterpret_cast<LPCWSTR>(&FormatDouble),
                               &module))
        {
            return false;
        }

        HRSRC resource = FindResourceW(module,
                                       MAKEINTRESOURCEW(IDR_ZH_UDF_ZHEWANBIRANGCAO_PRT),
                                       RT_RCDATA);
        if (resource == nullptr)
        {
            return false;
        }

        const DWORD byteCount = SizeofResource(module, resource);
        HGLOBAL loaded = LoadResource(module, resource);
        const void* bytes = loaded != nullptr ? LockResource(loaded) : nullptr;
        if (byteCount == 0 || bytes == nullptr)
        {
            return false;
        }

        try
        {
            const std::wstring uniqueName =
                L"ZW_BiLanCao_" + std::to_wstring(GetCurrentProcessId()) + L"_" +
                std::to_wstring(GetTickCount64());
            directory_ = std::filesystem::temp_directory_path() / uniqueName;
            std::filesystem::create_directories(directory_);
            file_ = directory_ / L"ZW_BiLanCao1.prt";

            std::ofstream output(file_, std::ios::binary | std::ios::trunc);
            output.write(static_cast<const char*>(bytes), byteCount);
            output.close();
            if (!output)
            {
                return false;
            }
            path = file_.string();
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

private:
    std::filesystem::path directory_;
    std::filesystem::path file_;
};

tag_t FindUdfDefinitionFeature(tag_t partTag)
{
    tag_t object = NULL_TAG;
    while (UF_OBJ_cycle_objs_in_part(partTag, UF_feature_type, &object) == 0 &&
           object != NULL_TAG)
    {
        char* featureType = nullptr;
        UF_MODL_ask_feat_type(object, &featureType);
        const bool isDefinition = featureType != nullptr &&
                                  std::string(featureType) == "UDF_DEF";
        if (featureType != nullptr)
        {
            UF_free(featureType);
        }
        if (isDefinition)
        {
            return object;
        }
    }
    return NULL_TAG;
}

std::string gLastSlotFailure;

bool ReportSlotFailure(const std::string& stage, int errorCode = 0)
{
    gLastSlotFailure = stage;
    if (errorCode != 0)
    {
        gLastSlotFailure += " (NX error " + std::to_string(errorCode) + ")";
    }

    std::string syslogMessage =
        "ZheWanBiRangCao: relief-slot failure: " + gLastSlotFailure + "\n";
    UF_print_syslog(syslogMessage.data(), FALSE);
    return false;
}

bool AllocateUdfExpressionValues(UF_MODL_udf_exp_data_t& expressionData,
                                 double slotDepth,
                                 double slotWidth,
                                 double thickness)
{
    if (expressionData.num_exps <= 0)
    {
        return true;
    }

    int allocationError = 0;
    expressionData.new_exp_values = static_cast<char**>(UF_allocate_memory(
        static_cast<unsigned int>(sizeof(char*) * expressionData.num_exps),
        &allocationError));
    if (allocationError != 0 || expressionData.new_exp_values == nullptr)
    {
        return false;
    }
    std::memset(expressionData.new_exp_values, 0,
                sizeof(char*) * expressionData.num_exps);

    for (int index = 0; index < expressionData.num_exps; ++index)
    {
        double numericValue = slotDepth;
        if (index == 1)
        {
            numericValue = slotWidth;
        }
        else if (index == 2)
        {
            numericValue = thickness;
        }

        const std::string value = FormatDouble(numericValue);
        char* stored = static_cast<char*>(UF_allocate_memory(
            static_cast<unsigned int>(value.size() + 1), &allocationError));
        if (allocationError != 0 || stored == nullptr)
        {
            return false;
        }
        std::memcpy(stored, value.c_str(), value.size() + 1);
        expressionData.new_exp_values[index] = stored;
    }
    return true;
}
std::string FormatDouble(const double value)
{
    char buffer[64] = {};
    std::snprintf(buffer, sizeof(buffer), "%.15g", value);
    return std::string(buffer);
}

double Dot(const NXOpen::Vector3d& a, const NXOpen::Vector3d& b)
{
    return a.X * b.X + a.Y * b.Y + a.Z * b.Z;
}

NXOpen::Vector3d Cross(const NXOpen::Vector3d& a, const NXOpen::Vector3d& b)
{
    return NXOpen::Vector3d(
        a.Y * b.Z - a.Z * b.Y,
        a.Z * b.X - a.X * b.Z,
        a.X * b.Y - a.Y * b.X);
}

NXOpen::Vector3d Scale(const NXOpen::Vector3d& vector, const double scale)
{
    return NXOpen::Vector3d(vector.X * scale, vector.Y * scale, vector.Z * scale);
}

NXOpen::Vector3d Add(const NXOpen::Vector3d& a, const NXOpen::Vector3d& b)
{
    return NXOpen::Vector3d(a.X + b.X, a.Y + b.Y, a.Z + b.Z);
}

NXOpen::Vector3d Subtract(const NXOpen::Vector3d& a, const NXOpen::Vector3d& b)
{
    return NXOpen::Vector3d(a.X - b.X, a.Y - b.Y, a.Z - b.Z);
}

NXOpen::Vector3d Subtract(const NXOpen::Point3d& a, const NXOpen::Point3d& b)
{
    return NXOpen::Vector3d(a.X - b.X, a.Y - b.Y, a.Z - b.Z);
}

NXOpen::Point3d Add(const NXOpen::Point3d& point, const NXOpen::Vector3d& vector)
{
    return NXOpen::Point3d(point.X + vector.X, point.Y + vector.Y, point.Z + vector.Z);
}

double Magnitude(const NXOpen::Vector3d& vector)
{
    return std::sqrt(Dot(vector, vector));
}

double Distance(const NXOpen::Point3d& a, const NXOpen::Point3d& b)
{
    return Magnitude(Subtract(a, b));
}

double DistancePointToSegment(const NXOpen::Point3d& point,
                              const NXOpen::Point3d& segmentStart,
                              const NXOpen::Point3d& segmentEnd)
{
    const NXOpen::Vector3d segment = Subtract(segmentEnd, segmentStart);
    const double segmentLengthSquared = Dot(segment, segment);
    if (segmentLengthSquared < kVectorTolerance)
    {
        return Distance(point, segmentStart);
    }

    const NXOpen::Vector3d fromStart = Subtract(point, segmentStart);
    double parameter = Dot(fromStart, segment) / segmentLengthSquared;
    parameter = std::max(0.0, std::min(1.0, parameter));
    const NXOpen::Point3d closestPoint = Add(segmentStart, Scale(segment, parameter));
    return Distance(point, closestPoint);
}

NXOpen::Point3d MidPoint(const NXOpen::Point3d& a, const NXOpen::Point3d& b)
{
    return NXOpen::Point3d(
        0.5 * (a.X + b.X),
        0.5 * (a.Y + b.Y),
        0.5 * (a.Z + b.Z));
}

NXOpen::Point3d ProjectPointToPlane(const NXOpen::Point3d& point,
                                    const NXOpen::Point3d& planePoint,
                                    const NXOpen::Vector3d& planeNormal)
{
    const double normalLength = Magnitude(planeNormal);
    if (normalLength < kVectorTolerance)
    {
        throw NXOpen::NXException::Create(1, "Zero-length vector.");
    }
    const NXOpen::Vector3d normal = Scale(planeNormal, 1.0 / normalLength);
    const double signedDistance = Dot(Subtract(point, planePoint), normal);
    return Add(point, Scale(normal, -signedDistance));
}

NXOpen::Point3d ClosestPointOnLine(const NXOpen::Point3d& point,
                                   const NXOpen::Point3d& linePoint,
                                   const NXOpen::Vector3d& lineDirection)
{
    const double directionLength = Magnitude(lineDirection);
    if (directionLength < kVectorTolerance)
    {
        throw NXOpen::NXException::Create(1, "Zero-length vector.");
    }
    const NXOpen::Vector3d direction = Scale(lineDirection, 1.0 / directionLength);
    return Add(linePoint, Scale(direction, Dot(Subtract(point, linePoint), direction)));
}

double DistancePointToLine(const NXOpen::Point3d& point,
                           const NXOpen::Point3d& linePoint,
                           const NXOpen::Vector3d& lineDirection)
{
    return Distance(point, ClosestPointOnLine(point, linePoint, lineDirection));
}

tag_t CreateLineBetweenPoints(const NXOpen::Point3d& startPoint, const NXOpen::Point3d& endPoint)
{
    if (Distance(startPoint, endPoint) < kVectorTolerance)
    {
        return NULL_TAG;
    }

    UF_CURVE_line_t lineData;
    lineData.start_point[0] = startPoint.X;
    lineData.start_point[1] = startPoint.Y;
    lineData.start_point[2] = startPoint.Z;
    lineData.end_point[0] = endPoint.X;
    lineData.end_point[1] = endPoint.Y;
    lineData.end_point[2] = endPoint.Z;

    tag_t lineTag = NULL_TAG;
    if (UF_CURVE_create_line(&lineData, &lineTag) != 0)
    {
        return NULL_TAG;
    }

    return lineTag;
}

bool EditLineBetweenPoints(tag_t lineTag, const NXOpen::Point3d& startPoint, const NXOpen::Point3d& endPoint)
{
    if (lineTag == NULL_TAG || Distance(startPoint, endPoint) < kVectorTolerance)
    {
        return false;
    }

    UF_CURVE_line_t lineData;
    lineData.start_point[0] = startPoint.X;
    lineData.start_point[1] = startPoint.Y;
    lineData.start_point[2] = startPoint.Z;
    lineData.end_point[0] = endPoint.X;
    lineData.end_point[1] = endPoint.Y;
    lineData.end_point[2] = endPoint.Z;
    return UF_CURVE_edit_line_data(lineTag, &lineData) == 0;
}

void DeleteObjectIfAlive(tag_t objectTag)
{
    if (objectTag != NULL_TAG && UF_OBJ_ask_status(objectTag) == UF_OBJ_ALIVE)
    {
        static_cast<void>(UF_OBJ_delete_object(objectTag));
    }
}

void DeleteObjects(const std::vector<tag_t>& objectTags)
{
    for (tag_t objectTag : objectTags)
    {
        DeleteObjectIfAlive(objectTag);
    }
}

std::vector<tag_t> AskBodyFeatureTags(tag_t bodyTag)
{
    std::vector<tag_t> featureTags;
    if (bodyTag == NULL_TAG)
    {
        return featureTags;
    }

    uf_list_p_t featureList = NULL;
    if (UF_MODL_ask_body_feats(bodyTag, &featureList) != 0 || featureList == NULL)
    {
        return featureTags;
    }

    int featureCount = 0;
    if (UF_MODL_ask_list_count(featureList, &featureCount) == 0)
    {
        for (int index = 0; index < featureCount; ++index)
        {
            tag_t featureTag = NULL_TAG;
            if (UF_MODL_ask_list_item(featureList, index, &featureTag) == 0 &&
                featureTag != NULL_TAG)
            {
                featureTags.push_back(featureTag);
            }
        }
    }
    UF_MODL_delete_list(&featureList);
    return featureTags;
}

bool ContainsTag(const std::vector<tag_t>& tags, tag_t tag)
{
    return std::find(tags.begin(), tags.end(), tag) != tags.end();
}

NXOpen::Vector3d Normalize(const NXOpen::Vector3d& vector)
{
    const double length = Magnitude(vector);
    if (length < kVectorTolerance)
    {
        throw NXOpen::NXException::Create(1, "Zero-length vector.");
    }

    return Scale(vector, 1.0 / length);
}

double FaceBoxScore(const double box[6])
{
    return (box[3] - box[0]) * (box[4] - box[1]) +
           (box[4] - box[1]) * (box[5] - box[2]) +
           (box[3] - box[0]) * (box[5] - box[2]);
}

NXOpen::Point3d FaceBoxCenter(const double box[6])
{
    return NXOpen::Point3d(
        0.5 * (box[0] + box[3]),
        0.5 * (box[1] + box[4]),
        0.5 * (box[2] + box[5]));
}

NXOpen::Vector3d AskPlanarFaceNormal(NXOpen::Face* face, double box[6])
{
    int faceType = 0;
    double point[3] = {0.0, 0.0, 0.0};
    double direction[3] = {0.0, 0.0, 0.0};
    double radius = 0.0;
    double radiusData = 0.0;
    int normalDirection = 0;

    if (UF_MODL_ask_face_data(face->Tag(), &faceType, point, direction, box, &radius, &radiusData, &normalDirection) != 0)
    {
        throw NXOpen::NXException::Create(1, "Failed to query face data.");
    }

    if (faceType != 22)
    {
        throw NXOpen::NXException::Create(1, "Adjacent planar face is required.");
    }

    NXOpen::Vector3d normal(direction[0], direction[1], direction[2]);
    if (normalDirection < 0)
    {
        normal = Scale(normal, -1.0);
    }

    return Normalize(normal);
}

int AskPointContainmentStatus(NXOpen::Body* body, const NXOpen::Point3d& point)
{
    if (body == nullptr)
    {
        return 0;
    }

    double coordinates[3] = {point.X, point.Y, point.Z};
    int status = 0;
    if (UF_MODL_ask_point_containment(coordinates, body->Tag(), &status) != 0)
    {
        return 0;
    }

    return status;
}

NXOpen::Vector3d AskPlanarFaceOuterNormal(NXOpen::Face* face, const NXOpen::Point3d& referencePoint)
{
    if (face == nullptr || face->SolidFaceType() != NXOpen::Face::FaceTypePlanar)
    {
        throw NXOpen::NXException::Create(1, "Planar face is required.");
    }

    NXOpen::BasePart* owningPart = face->OwningPart();
    if (owningPart == nullptr)
    {
        throw NXOpen::NXException::Create(1, "Failed to get owning part.");
    }

    NXOpen::Point* normalPoint = owningPart->Points()->CreatePoint(referencePoint);
    normalPoint->Blank();
    NXOpen::Direction* normalDirection =
        owningPart->Directions()->CreateDirection(
            face,
            normalPoint,
            NXOpen::SenseForward,
            NXOpen::SmartObject::UpdateOptionWithinModeling);

    const NXOpen::Vector3d outerNormal = Normalize(normalDirection->Vector());
    return outerNormal;
}

double AskFaceArea(NXOpen::Face* face)
{
    if (face == nullptr)
    {
        return 0.0;
    }

    NXOpen::Part* workPart = NXOpen::Session::GetSession()->Parts()->Work();
    if (workPart == nullptr)
    {
        return 0.0;
    }

    try
    {
        NXOpen::Unit* areaUnit = workPart->UnitCollection()->FindObject("SquareMilliMeter");
        NXOpen::Unit* lengthUnit = workPart->UnitCollection()->GetBase("Length");
        std::vector<NXOpen::IParameterizedSurface*> faces(1, face);
        NXOpen::MeasureFaces* measureFaces =
            workPart->MeasureManager()->NewFaceProperties(areaUnit, lengthUnit, 0.99, faces);
        const double area = measureFaces->Area();
        delete measureFaces;
        return area;
    }
    catch (...)
    {
        double box[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
        AskPlanarFaceNormal(face, box);
        return FaceBoxScore(box);
    }
}

void ForceModelUpdate()
{
    if (UF_MODL_update() != 0)
    {
        throw NXOpen::NXException::Create(1, "Failed to update the model.");
    }
}

}

ZheWanBiRangCaoDialog::ZheWanBiRangCaoDialog()
    : ui_(NXOpen::UI::GetUI()),
      session_(NXOpen::Session::GetSession()),
      dialog_(nullptr),
      mainGroup_(nullptr),
      edgeSelectBlock_(nullptr),
      cutModeBlock_(nullptr),
      slotWidthBlock_(nullptr),
      slotDepthBlock_(nullptr),
      customFeatureManager_(nullptr),
      editedFeature_(nullptr),
      featureClass_(nullptr),
      loadingEditedFeature_(false),
      hasEditedPickPoint_(false),
      editedPickPoint_(0.0, 0.0, 0.0),
      slotRecords_(),
      hiddenTemporaryTags_(),
      previewFeatureTags_()
{
    const std::string dlxPath = zhihui_embedded_dialog::ExtractDlxToRandomPath(IDR_ZH_DLX_ZHEWANBIRANGCAO_DLX);

    if (dlxPath.empty())

    {

        throw std::runtime_error("ZheWanBiRangCao dialog resource is missing.");

    }

    dialog_ = ui_->CreateDialog(dlxPath.c_str());
    dialog_->AddInitializeHandler(NXOpen::make_callback(this, &ZheWanBiRangCaoDialog::initialize_cb));
    dialog_->AddEnableOKButtonHandler(NXOpen::make_callback(this, &ZheWanBiRangCaoDialog::enable_ok_cb));
    dialog_->AddUpdateHandler(NXOpen::make_callback(this, &ZheWanBiRangCaoDialog::update_cb));
    dialog_->AddApplyHandler(NXOpen::make_callback(this, &ZheWanBiRangCaoDialog::apply_cb));
    dialog_->AddOkHandler(NXOpen::make_callback(this, &ZheWanBiRangCaoDialog::ok_cb));
    dialog_->AddCancelHandler(NXOpen::make_callback(this, &ZheWanBiRangCaoDialog::cancel_cb));

    customFeatureManager_ = session_->CustomFeatureClassManager();
    editedFeature_ = customFeatureManager_->GetEditedCustomFeature();
    featureClass_ = customFeatureManager_->GetClassFromName(
        zhihui_zhewan_birangcao::kFeatureClassName);
}

ZheWanBiRangCaoDialog::~ZheWanBiRangCaoDialog()
{
    CleanupPreviewObjects();
    CleanupHiddenTemporaryObjects();

    if (dialog_ != nullptr)
    {
        delete dialog_;
        dialog_ = nullptr;
    }
}

NXOpen::BlockStyler::BlockDialog::DialogResponse ZheWanBiRangCaoDialog::Launch()
{
    const NXOpen::BlockStyler::BlockDialog::DialogMode mode =
        editedFeature_ != nullptr
            ? NXOpen::BlockStyler::BlockDialog::DialogModeEdit
            : NXOpen::BlockStyler::BlockDialog::DialogModeCreate;
    return dialog_->LaunchInDialogMode(mode);
}

std::string ZheWanBiRangCaoDialog::GetDialogFilePath() const
{
    const std::filesystem::path modulePath = std::filesystem::path(__FILE__).parent_path();
    return (modulePath / "ZheWanBiRangCao.dlx").string();
}

void ZheWanBiRangCaoDialog::initialize_cb()
{
    mainGroup_ = dialog_->TopBlock()->FindBlock("main_group");
    edgeSelectBlock_ = dialog_->TopBlock()->FindBlock("edge_select");
    cutModeBlock_ = dialog_->TopBlock()->FindBlock("cut_mode");
    slotWidthBlock_ = dialog_->TopBlock()->FindBlock("slot_width_y");
    slotDepthBlock_ = dialog_->TopBlock()->FindBlock("slot_depth_z");

    NXOpen::BlockStyler::PropertyList* properties = edgeSelectBlock_->GetProperties();
    std::vector<NXOpen::Selection::MaskTriple> selectionMaskArray;
    selectionMaskArray.emplace_back(UF_solid_type, UF_solid_body_subtype, UF_UI_SEL_FEATURE_PLANAR_FACE);
    properties->SetSelectionFilter(
        "SelectionFilter",
        NXOpen::Selection::SelectionActionClearAndEnableSpecific,
        selectionMaskArray);
    delete properties;

    LoadDialogState();
    LoadEditedCustomFeatureData();
    if (editedFeature_ != nullptr && GetSelectedFace() != nullptr)
    {
        CleanupPreviewObjects();
        CleanupHiddenTemporaryObjects();
        static_cast<void>(Preview());
    }
}

void ZheWanBiRangCaoDialog::LoadEditedCustomFeatureData()
{
    if (editedFeature_ == nullptr || edgeSelectBlock_ == nullptr)
    {
        return;
    }

    loadingEditedFeature_ = true;
    try
    {
        NXOpen::Features::CustomFeatureData* data = editedFeature_->FeatureData();
        SetDoubleValue(
            slotWidthBlock_,
            data->CustomDoubleAttributeByName(
                    zhihui_zhewan_birangcao::kAttrSlotWidth)
                ->Value());
        SetDoubleValue(
            slotDepthBlock_,
            data->CustomDoubleAttributeByName(
                    zhihui_zhewan_birangcao::kAttrSlotDepth)
                ->Value());

        NXOpen::Face* face = dynamic_cast<NXOpen::Face*>(
            data->CustomTagAttributeByName(
                    zhihui_zhewan_birangcao::kAttrSelectedFace)
                ->Value());
        const std::vector<double> pickPoint =
            data->CustomDoubleArrayAttributeByName(
                    zhihui_zhewan_birangcao::kAttrPickPoint)
                ->GetValues();
        const std::vector<double> transforms =
            data->CustomDoubleArrayAttributeByName(
                    zhihui_zhewan_birangcao::kAttrToolTransforms)
                ->GetValues();

        const std::size_t transformCount =
            transforms.size() / zhihui_zhewan_birangcao::kTransformValueCount;
        zhihui_dialog_memory::TrySetEnum(
            cutModeBlock_, transformCount <= 1 ? 0 : 1);

        NXOpen::BlockStyler::PropertyList* properties = edgeSelectBlock_->GetProperties();
        if (face != nullptr)
        {
            std::vector<NXOpen::TaggedObject*> selectedObjects(1, face);
            properties->SetTaggedObjectVector("SelectedObjects", selectedObjects);
        }
        if (pickPoint.size() >= 3)
        {
            editedPickPoint_ = NXOpen::Point3d(
                pickPoint[0], pickPoint[1], pickPoint[2]);
            hasEditedPickPoint_ = true;
        }
        delete properties;
    }
    catch (...)
    {
        loadingEditedFeature_ = false;
        throw;
    }
    loadingEditedFeature_ = false;
}

bool ZheWanBiRangCaoDialog::enable_ok_cb()
{
    return GetSelectedFace() != nullptr || !slotRecords_.empty();
}

int ZheWanBiRangCaoDialog::update_cb(NXOpen::BlockStyler::UIBlock* block)
{
    if (loadingEditedFeature_)
    {
        return 0;
    }
    if (block == edgeSelectBlock_)
    {
        hasEditedPickPoint_ = false;
        if (GetSelectedFace() != nullptr)
        {
            CleanupPreviewObjects();
            CleanupHiddenTemporaryObjects();
            return Preview();
        }
        return 0;
    }

    if (block == cutModeBlock_ || block == slotWidthBlock_ ||
        block == slotDepthBlock_)
    {
        SaveDialogState();
        if (GetSelectedFace() != nullptr)
        {
            CleanupPreviewObjects();
            CleanupHiddenTemporaryObjects();
            return Preview();
        }
        return 0;
    }

    return 0;
}

int ZheWanBiRangCaoDialog::apply_cb()
{
    SaveDialogState();

    if (GetSelectedFace() != nullptr)
    {
        CleanupPreviewObjects();
        CleanupHiddenTemporaryObjects();
        const int result = Execute();
        if (result == 0 && editedFeature_ == nullptr)
        {
            ClearSelectedEdge();
        }
        return result;
    }

    previewFeatureTags_.clear();
    CleanupHiddenTemporaryObjects();
    return 0;
}

int ZheWanBiRangCaoDialog::ok_cb()
{
    return apply_cb();
}

int ZheWanBiRangCaoDialog::cancel_cb()
{
    CleanupPreviewObjects();
    CleanupHiddenTemporaryObjects();
    return 0;
}

NXOpen::Face* ZheWanBiRangCaoDialog::GetSelectedFace() const
{
    if (edgeSelectBlock_ == nullptr)
    {
        return nullptr;
    }

    NXOpen::BlockStyler::PropertyList* properties = edgeSelectBlock_->GetProperties();
    const std::vector<NXOpen::TaggedObject*> selectedObjects = properties->GetTaggedObjectVector("SelectedObjects");
    delete properties;
    if (selectedObjects.empty())
    {
        return nullptr;
    }

    return dynamic_cast<NXOpen::Face*>(selectedObjects.front());
}

NXOpen::Point3d ZheWanBiRangCaoDialog::GetSelectionPickPoint() const
{
    if (hasEditedPickPoint_)
    {
        return editedPickPoint_;
    }

    NXOpen::BlockStyler::PropertyList* properties = edgeSelectBlock_->GetProperties();
    const NXOpen::Point3d point = properties->GetPoint("PickPoint");
    delete properties;
    return point;
}

void ZheWanBiRangCaoDialog::ClearSelectedEdge() const
{
    if (edgeSelectBlock_ == nullptr)
    {
        return;
    }

    NXOpen::BlockStyler::PropertyList* properties = edgeSelectBlock_->GetProperties();
    properties->SetTaggedObjectVector("SelectedObjects", std::vector<NXOpen::TaggedObject*>());
    delete properties;
}

double ZheWanBiRangCaoDialog::GetSlotWidthY() const
{
    NXOpen::BlockStyler::PropertyList* properties = slotWidthBlock_->GetProperties();
    const double value = properties->GetDouble("Value");
    delete properties;
    return value;
}

double ZheWanBiRangCaoDialog::GetSlotDepthZ() const
{
    NXOpen::BlockStyler::PropertyList* properties = slotDepthBlock_->GetProperties();
    const double value = properties->GetDouble("Value");
    delete properties;
    return value;
}

int ZheWanBiRangCaoDialog::GetCutMode() const
{
    if (cutModeBlock_ == nullptr)
    {
        return 1;
    }
    NXOpen::BlockStyler::PropertyList* properties = cutModeBlock_->GetProperties();
    const int value = properties->GetEnum("Value");
    delete properties;
    return value;
}

void ZheWanBiRangCaoDialog::SetDoubleValue(NXOpen::BlockStyler::UIBlock* block, double value) const
{
    if (block == nullptr)
    {
        return;
    }

    NXOpen::BlockStyler::PropertyList* properties = block->GetProperties();
    properties->SetDouble("Value", value);
    delete properties;
}

void ZheWanBiRangCaoDialog::LoadDialogState() const
{
    zhihui_dialog_memory::LoadEnum(L"ZheWanBiRangCao_state.ini", L"cutMode", cutModeBlock_);
    zhihui_dialog_memory::LoadDouble(L"ZheWanBiRangCao_state.ini", L"slotWidthY", slotWidthBlock_);
    zhihui_dialog_memory::LoadDouble(L"ZheWanBiRangCao_state.ini", L"slotDepthZ", slotDepthBlock_);
}

void ZheWanBiRangCaoDialog::SaveDialogState() const
{
    zhihui_dialog_memory::SaveEnum(L"ZheWanBiRangCao_state.ini", L"cutMode", cutModeBlock_);
    zhihui_dialog_memory::SaveDouble(L"ZheWanBiRangCao_state.ini", L"slotWidthY", slotWidthBlock_);
    zhihui_dialog_memory::SaveDouble(L"ZheWanBiRangCao_state.ini", L"slotDepthZ", slotDepthBlock_);
}

void ZheWanBiRangCaoDialog::ShowError(const std::string& message) const
{
    ui_->NXMessageBox()->Show("ZheWanBiRangCao", NXOpen::NXMessageBox::DialogTypeError, message.c_str());
}

void ZheWanBiRangCaoDialog::ColorFaceBlue(NXOpen::Face* face) const
{
    if (face == nullptr)
    {
        return;
    }

    face->SetColor(6);
    face->RedisplayObject();
}

NXOpen::Edge* ZheWanBiRangCaoDialog::FindClosestLinearEdgeOnFace(NXOpen::Face* face, const NXOpen::Point3d& pickPoint) const
{
    if (face == nullptr)
    {
        return nullptr;
    }

    NXOpen::Edge* bestEdge = nullptr;
    double bestDistance = std::numeric_limits<double>::max();
    for (NXOpen::Edge* edge : face->GetEdges())
    {
        if (edge == nullptr || edge->SolidEdgeType() != NXOpen::Edge::EdgeTypeLinear)
        {
            continue;
        }

        NXOpen::Point3d startPoint;
        NXOpen::Point3d endPoint;
        edge->GetVertices(&startPoint, &endPoint);
        const double distance = DistancePointToSegment(pickPoint, startPoint, endPoint);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestEdge = edge;
        }
    }

    return bestEdge;
}

double ZheWanBiRangCaoDialog::EstimateThickness(NXOpen::Body* body, NXOpen::Face* referenceFace) const
{
    if (body == nullptr)
    {
        throw NXOpen::NXException::Create(1, "Failed to estimate sheet thickness.");
    }

    NXOpen::Face* baseFace = referenceFace;
    double bestArea = -1.0;
    for (NXOpen::Face* face : body->GetFaces())
    {
        if (face == nullptr || face->SolidFaceType() != NXOpen::Face::FaceTypePlanar)
        {
            continue;
        }

        const double area = AskFaceArea(face);
        if (area > bestArea)
        {
            bestArea = area;
            baseFace = face;
        }
    }

    if (baseFace == nullptr)
    {
        throw NXOpen::NXException::Create(1, "Failed to estimate sheet thickness.");
    }

    double baseBox[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    const NXOpen::Vector3d baseNormal = AskPlanarFaceNormal(baseFace, baseBox);
    const NXOpen::Point3d basePoint = FaceBoxCenter(baseBox);

    double bestDistance = std::numeric_limits<double>::max();
    for (NXOpen::Face* candidate : body->GetFaces())
    {
        if (candidate == nullptr || candidate == baseFace || candidate->SolidFaceType() != NXOpen::Face::FaceTypePlanar)
        {
            continue;
        }

        double candidateBox[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
        const NXOpen::Vector3d candidateNormal = AskPlanarFaceNormal(candidate, candidateBox);
        if (std::fabs(Dot(candidateNormal, baseNormal)) < 0.98)
        {
            continue;
        }

        const NXOpen::Point3d candidatePoint = FaceBoxCenter(candidateBox);
        const double distance = std::fabs(Dot(Subtract(candidatePoint, basePoint), baseNormal));
        if (distance > 0.01 && distance < bestDistance)
        {
            bestDistance = distance;
        }
    }

    if (bestDistance == std::numeric_limits<double>::max())
    {
        throw NXOpen::NXException::Create(1, "Failed to estimate sheet thickness.");
    }

    return bestDistance;
}

double ZheWanBiRangCaoDialog::ComputeInnerEdgeDistance(NXOpen::Edge* selectedEdge) const
{
    std::vector<NXOpen::Face*> faces = selectedEdge->GetFaces();
    if (faces.size() < 2)
    {
        throw NXOpen::NXException::Create(1, "Selected edge must have two adjacent faces.");
    }

    double box1[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    double box2[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    const NXOpen::Vector3d normal1 = AskPlanarFaceNormal(faces[0], box1);
    const NXOpen::Vector3d normal2 = AskPlanarFaceNormal(faces[1], box2);

    NXOpen::Face* largerFace = AskFaceArea(faces[0]) >= AskFaceArea(faces[1]) ? faces[0] : faces[1];
    const double thickness = EstimateThickness(selectedEdge->GetBody(), largerFace);

    double cosine = std::fabs(Dot(normal1, normal2));
    cosine = std::max(-1.0, std::min(1.0, cosine));
    const double angle = std::acos(cosine);
    if (angle < 1.0e-3)
    {
        throw NXOpen::NXException::Create(1, "Failed to compute bend angle.");
    }

    return thickness / std::sin(0.5 * angle);
}

std::vector<ZheWanBiRangCaoDialog::SlotReferenceEdge> ZheWanBiRangCaoDialog::FindInnerReferenceEdges(
    NXOpen::Edge* selectedEdge,
    NXOpen::Face* face,
    const NXOpen::Point3d& selectedPlanePoint,
    const NXOpen::Vector3d& selectedPlaneNormal) const
{
    NXOpen::Body* body = selectedEdge->GetBody();
    NXOpen::Point3d selectedStart;
    NXOpen::Point3d selectedEnd;
    selectedEdge->GetVertices(&selectedStart, &selectedEnd);

    const NXOpen::Vector3d selectedDirection = Normalize(Subtract(selectedEnd, selectedStart));
    const NXOpen::Point3d selectedMidPoint = MidPoint(selectedStart, selectedEnd);
    const double selectedLength = Distance(selectedStart, selectedEnd);
    const double thickness = EstimateThickness(body, face);
    const double targetDistance = ComputeInnerEdgeDistance(selectedEdge);
    const double distanceTolerance = std::max(0.5, thickness * 0.35);
    const double projectedThicknessTolerance = std::max(0.2, thickness * 0.2);
    const double axialOffsetTolerance = std::max(thickness * 2.0, selectedLength * 0.6);

    struct CandidateEdge
    {
        double score;
        SlotReferenceEdge edge;
    };

    std::vector<CandidateEdge> candidates;

    for (NXOpen::Edge* candidate : body->GetEdges())
    {
        if (candidate == nullptr || candidate == selectedEdge || candidate->SolidEdgeType() != NXOpen::Edge::EdgeTypeLinear)
        {
            continue;
        }

        NXOpen::Point3d candidateStart;
        NXOpen::Point3d candidateEnd;
        candidate->GetVertices(&candidateStart, &candidateEnd);

        NXOpen::Vector3d candidateDirection = Normalize(Subtract(candidateEnd, candidateStart));
        if (std::fabs(Dot(candidateDirection, selectedDirection)) < 0.98)
        {
            continue;
        }

        NXOpen::Point3d alignedStart = candidateStart;
        NXOpen::Point3d alignedEnd = candidateEnd;
        if (Dot(candidateDirection, selectedDirection) < 0.0)
        {
            alignedStart = candidateEnd;
            alignedEnd = candidateStart;
        }

        const NXOpen::Point3d candidateMidPoint = MidPoint(alignedStart, alignedEnd);
        const NXOpen::Vector3d midOffset = Subtract(candidateMidPoint, selectedMidPoint);
        const double edgeSpacing = Magnitude(Cross(midOffset, selectedDirection));
        const double spacingError = std::fabs(edgeSpacing - targetDistance);
        if (spacingError > distanceTolerance)
        {
            continue;
        }

        const double axialOffset = std::fabs(Dot(midOffset, selectedDirection));
        if (axialOffset > axialOffsetTolerance)
        {
            continue;
        }

        const double candidateStartProjection = Dot(Subtract(alignedStart, selectedStart), selectedDirection);
        const double candidateEndProjection = Dot(Subtract(alignedEnd, selectedStart), selectedDirection);
        const double candidateMinProjection = std::min(candidateStartProjection, candidateEndProjection);
        const double candidateMaxProjection = std::max(candidateStartProjection, candidateEndProjection);
        const double overlapLength =
            std::min(selectedLength, candidateMaxProjection) - std::max(0.0, candidateMinProjection);
        if (overlapLength <= std::max(thickness * 0.5, 0.2))
        {
            continue;
        }

        const NXOpen::Point3d projectedStart = ProjectPointToPlane(alignedStart, selectedPlanePoint, selectedPlaneNormal);
        const NXOpen::Point3d projectedEnd = ProjectPointToPlane(alignedEnd, selectedPlanePoint, selectedPlaneNormal);
        const double projectedStartDistance = DistancePointToLine(projectedStart, selectedStart, selectedDirection);
        const double projectedEndDistance = DistancePointToLine(projectedEnd, selectedStart, selectedDirection);
        const double projectedDistance = 0.5 * (projectedStartDistance + projectedEndDistance);
        const double projectedDistanceError = std::fabs(projectedDistance - thickness);
        if (projectedDistanceError > projectedThicknessTolerance)
        {
            continue;
        }

        const double endpointError =
            std::min(Distance(alignedStart, selectedStart), Distance(alignedStart, selectedEnd)) +
            std::min(Distance(alignedEnd, selectedStart), Distance(alignedEnd, selectedEnd));
        const double score = spacingError * 100.0 + projectedDistanceError * 100.0 + axialOffset * 10.0 + endpointError;
        candidates.push_back(CandidateEdge{score, SlotReferenceEdge{candidate, alignedStart, alignedEnd}});
    }

    if (candidates.empty())
    {
        throw NXOpen::NXException::Create(1, "No inner corner edge was found from the selected edge.");
    }

    std::sort(
        candidates.begin(),
        candidates.end(),
        [](const CandidateEdge& left, const CandidateEdge& right)
        {
            return left.score < right.score;
        });

    std::vector<SlotReferenceEdge> result;
    result.reserve(candidates.size());
    for (const CandidateEdge& candidate : candidates)
    {
        bool duplicated = false;
        for (const SlotReferenceEdge& existing : result)
        {
            if (existing.edge == candidate.edge.edge)
            {
                duplicated = true;
                break;
            }
        }
        if (!duplicated)
        {
            result.push_back(candidate.edge);
        }
    }

    return result;
}

std::vector<ZheWanBiRangCaoDialog::SlotEndCandidate>
ZheWanBiRangCaoDialog::BuildSlotEndCandidates(
    const std::vector<SlotReferenceEdge>& referenceEdges,
    const NXOpen::Point3d& pickPoint) const
{
    std::vector<SlotEndCandidate> candidates;
    candidates.reserve(referenceEdges.size() * 2);
    for (const SlotReferenceEdge& referenceEdge : referenceEdges)
    {
        candidates.push_back(
            SlotEndCandidate{referenceEdge.startPoint, referenceEdge.endPoint});
        candidates.push_back(
            SlotEndCandidate{referenceEdge.endPoint, referenceEdge.startPoint});
    }

    // 0 = single cut: keep only the end nearest to the actual face pick point.
    // 1 = multiple cuts: preserve the existing behavior and keep every end.
    if (GetCutMode() == 0 && candidates.size() > 1)
    {
        const auto nearest = std::min_element(
            candidates.begin(),
            candidates.end(),
            [&pickPoint](const SlotEndCandidate& left,
                         const SlotEndCandidate& right)
            {
                return Distance(pickPoint, left.endpoint) <
                       Distance(pickPoint, right.endpoint);
            });
        const SlotEndCandidate selected = *nearest;
        candidates.assign(1, selected);
    }
    return candidates;
}

NXOpen::Point3d ZheWanBiRangCaoDialog::FindNearestOuterPoint(const NXOpen::Point3d& innerPoint,
                                                             const NXOpen::Point3d& outerStart,
                                                             const NXOpen::Point3d& outerEnd) const
{
    return Distance(innerPoint, outerStart) <= Distance(innerPoint, outerEnd) ? outerStart : outerEnd;
}

void ZheWanBiRangCaoDialog::GetPickedEndPoints(NXOpen::Edge* edge,
                                               const NXOpen::Point3d& pickPoint,
                                               NXOpen::Point3d& pointA,
                                               NXOpen::Point3d& pointB) const
{
    NXOpen::Point3d startPoint;
    NXOpen::Point3d endPoint;
    edge->GetVertices(&startPoint, &endPoint);

    if (Distance(pickPoint, startPoint) <= Distance(pickPoint, endPoint))
    {
        pointA = startPoint;
        pointB = endPoint;
    }
    else
    {
        pointA = endPoint;
        pointB = startPoint;
    }
}

NXOpen::Face* ZheWanBiRangCaoDialog::FindInnerSlotCarrierFace(NXOpen::Edge* edge,
                                                              const NXOpen::Vector3d& yDirection,
                                                              double thickness) const
{
    const NXOpen::Vector3d normalizedY = Normalize(yDirection);
    const double distanceTolerance = std::max(thickness * 0.25, 0.1);
    NXOpen::Face* bestFace = nullptr;
    double bestSpacingError = std::numeric_limits<double>::max();

    for (NXOpen::Face* face : edge->GetFaces())
    {
        if (face == nullptr || face->SolidFaceType() != NXOpen::Face::FaceTypePlanar)
        {
            continue;
        }

        NXOpen::Point3d edgeStart;
        NXOpen::Point3d edgeEnd;
        edge->GetVertices(&edgeStart, &edgeEnd);
        const NXOpen::Point3d edgeMid = MidPoint(edgeStart, edgeEnd);

        for (NXOpen::Edge* candidate : face->GetEdges())
        {
            if (candidate == nullptr || candidate == edge || candidate->SolidEdgeType() != NXOpen::Edge::EdgeTypeLinear)
            {
                continue;
            }

            NXOpen::Point3d candidateStart;
            NXOpen::Point3d candidateEnd;
            candidate->GetVertices(&candidateStart, &candidateEnd);

            const NXOpen::Vector3d candidateDirection = Normalize(Subtract(candidateEnd, candidateStart));
            if (std::fabs(Dot(candidateDirection, normalizedY)) < 0.98)
            {
                continue;
            }

            const double spacing = DistancePointToSegment(edgeMid, candidateStart, candidateEnd);
            const double spacingError = std::fabs(spacing - thickness);
            if (spacingError > distanceTolerance)
            {
                continue;
            }

            if (spacingError < bestSpacingError)
            {
                bestSpacingError = spacingError;
                bestFace = face;
            }
        }
    }

    return bestFace;
}

double ZheWanBiRangCaoDialog::ComputeDepthChainLengthFromPoint(NXOpen::Body* body,
                                                               NXOpen::Edge* selectedEdge,
                                                               const NXOpen::Point3d& startPoint,
                                                               const NXOpen::Vector3d& zDirection,
                                                               double thickness) const
{
    if (body == nullptr)
    {
        return 0.0;
    }

    const NXOpen::Vector3d normalizedZ = Normalize(zDirection);
    const double pointTolerance = std::max(thickness * 0.25, 0.05);
    const double directionTolerance = 0.95;

    NXOpen::Point3d currentPoint = startPoint;
    NXOpen::Edge* previousEdge = selectedEdge;
    double totalLength = 0.0;

    for (;;)
    {
        NXOpen::Edge* matchedEdge = nullptr;
        NXOpen::Point3d matchedOtherPoint;
        double bestAlignment = directionTolerance;

        for (NXOpen::Edge* candidate : body->GetEdges())
        {
            if (candidate == nullptr || candidate == previousEdge || candidate->SolidEdgeType() != NXOpen::Edge::EdgeTypeLinear)
            {
                continue;
            }

            NXOpen::Point3d candidateStart;
            NXOpen::Point3d candidateEnd;
            candidate->GetVertices(&candidateStart, &candidateEnd);

            const bool startMatches = Distance(candidateStart, currentPoint) <= pointTolerance;
            const bool endMatches = Distance(candidateEnd, currentPoint) <= pointTolerance;
            if (!startMatches && !endMatches)
            {
                continue;
            }

            const NXOpen::Point3d otherPoint = startMatches ? candidateEnd : candidateStart;
            const NXOpen::Vector3d candidateDirection = Normalize(Subtract(otherPoint, currentPoint));
            const double alignment = Dot(candidateDirection, normalizedZ);
            if (alignment < directionTolerance)
            {
                continue;
            }

            if (matchedEdge == nullptr || alignment > bestAlignment)
            {
                matchedEdge = candidate;
                matchedOtherPoint = otherPoint;
                bestAlignment = alignment;
            }
        }

        if (matchedEdge == nullptr)
        {
            break;
        }

        const double segmentLength = Distance(currentPoint, matchedOtherPoint);
        totalLength += segmentLength;
        currentPoint = matchedOtherPoint;
        previousEdge = matchedEdge;
    }

    return totalLength;
}

bool ZheWanBiRangCaoDialog::ShouldCreateSlotAtEnd(const NXOpen::Point3d& innerPoint,
                                                  const NXOpen::Point3d& outerPoint,
                                                  const NXOpen::Vector3d& outerEdgeDirection,
                                                  NXOpen::Body* body,
                                                  double thickness) const
{
    (void)body;
    const double endExtension = std::fabs(Dot(Subtract(outerPoint, innerPoint), Normalize(outerEdgeDirection)));
    const double extensionTolerance = std::max(thickness * 0.5, 0.5);
    return endExtension > extensionTolerance;
}

NXOpen::Vector3d ZheWanBiRangCaoDialog::AskSelectedFaceOuterNormal(
    NXOpen::Face* face,
    NXOpen::Body* body,
    const NXOpen::Point3d& validFacePoint) const
{
    if (face == nullptr || body == nullptr)
    {
        throw NXOpen::NXException::Create(1, "A planar face and its body are required.");
    }

    double faceBox[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    const NXOpen::Vector3d candidate = AskPlanarFaceNormal(face, faceBox);
    const NXOpen::Point3d positiveProbe = Add(validFacePoint, Scale(candidate, 0.05));
    if (AskPointContainmentStatus(body, positiveProbe) == 2)
    {
        return candidate;
    }
    return Scale(candidate, -1.0);
}

bool ZheWanBiRangCaoDialog::BuildSlotToolTransformAtEnd(
    NXOpen::Edge* selectedEdge,
    NXOpen::Face* selectedFace,
    const NXOpen::Point3d& selectionPickPoint,
    const NXOpen::Point3d& endpoint,
    const NXOpen::Point3d& otherEndpoint,
    double slotWidth,
    double slotDepth,
    double thickness,
    SlotToolTransform& transform) const
{
    NXOpen::Body* targetBody = selectedEdge != nullptr ? selectedEdge->GetBody() : nullptr;
    if (targetBody == nullptr || selectedFace == nullptr || slotWidth <= 0.0 ||
        slotDepth <= 0.0 || thickness <= 0.0)
    {
        return false;
    }

    NXOpen::Vector3d endpointInwardDirection = Subtract(otherEndpoint, endpoint);
    if (Magnitude(endpointInwardDirection) < kVectorTolerance)
    {
        return false;
    }
    endpointInwardDirection = Normalize(endpointInwardDirection);

    NXOpen::Point3d selectedEdgeStart;
    NXOpen::Point3d selectedEdgeEnd;
    selectedEdge->GetVertices(&selectedEdgeStart, &selectedEdgeEnd);
    NXOpen::Vector3d nearestEdgeDirection = Subtract(selectedEdgeEnd, selectedEdgeStart);
    if (Magnitude(nearestEdgeDirection) < kVectorTolerance)
    {
        return false;
    }
    nearestEdgeDirection = Normalize(nearestEdgeDirection);
    const NXOpen::Point3d perpendicularPoint =
        ClosestPointOnLine(selectionPickPoint, selectedEdgeStart, nearestEdgeDirection);

    transform.zDirection =
        AskSelectedFaceOuterNormal(selectedFace, targetBody, selectionPickPoint);
    transform.yDirection = Subtract(perpendicularPoint, selectionPickPoint);
    transform.yDirection = Subtract(
        transform.yDirection,
        Scale(transform.zDirection, Dot(transform.yDirection, transform.zDirection)));
    if (Magnitude(transform.yDirection) < kVectorTolerance)
    {
        return false;
    }
    transform.yDirection = Normalize(transform.yDirection);
    transform.xDirection = Cross(transform.yDirection, transform.zDirection);
    if (Magnitude(transform.xDirection) < kVectorTolerance)
    {
        return false;
    }
    transform.xDirection = Normalize(transform.xDirection);
    transform.origin = Add(endpoint, Scale(endpointInwardDirection, -0.5 * slotWidth));
    return true;
}

bool ZheWanBiRangCaoDialog::CommitEditableCustomFeature(
    NXOpen::Body* targetBody,
    NXOpen::Face* selectedFace,
    NXOpen::Edge* selectedEdge,
    const NXOpen::Point3d& pickPoint,
    double slotWidth,
    double slotDepth,
    double thickness,
    const std::vector<SlotToolTransform>& transforms)
{
    NXOpen::Part* workPart = session_->Parts()->Work();
    if (featureClass_ == nullptr)
    {
        return ReportSlotFailure(
            "editable custom feature class is not registered; restart NX after deployment");
    }

    if (workPart == nullptr || targetBody == nullptr || selectedFace == nullptr ||
        selectedEdge == nullptr || transforms.empty())
    {
        return ReportSlotFailure("invalid editable custom feature inputs");
    }

    NXOpen::Features::CustomFeatureBuilder* builder = nullptr;
    try
    {
        NXOpen::Features::CustomFeatureData* data = nullptr;
        builder = workPart->Features()->CreateCustomFeatureBuilder(editedFeature_);
        if (editedFeature_ == nullptr)
        {
            NXOpen::Features::CustomAttributeCollection* attributes =
                workPart->Features()->CustomAttributeCollection();
            std::vector<NXOpen::Features::CustomAttribute*> values;

            std::vector<NXOpen::Features::CustomAttribute::Property> required{
                NXOpen::Features::CustomAttribute::PropertyMandatoryInput};
            std::vector<NXOpen::Features::CustomAttribute::Property> targetProperties = required;
            targetProperties.push_back(
                NXOpen::Features::CustomAttribute::PropertyIsReferencingTargetBody);
            values.push_back(attributes->CreateCustomTagAttribute(
                zhihui_zhewan_birangcao::kAttrTargetBody, targetProperties));
            values.push_back(attributes->CreateCustomTagAttribute(
                zhihui_zhewan_birangcao::kAttrSelectedFace, required));
            values.push_back(attributes->CreateCustomTagAttribute(
                zhihui_zhewan_birangcao::kAttrSelectedEdge, required));

            const std::vector<NXOpen::Features::CustomAttribute::Property> optional;
            values.push_back(attributes->CreateCustomDoubleAttribute(
                zhihui_zhewan_birangcao::kAttrSlotWidth, optional));
            values.push_back(attributes->CreateCustomDoubleAttribute(
                zhihui_zhewan_birangcao::kAttrSlotDepth, optional));
            values.push_back(attributes->CreateCustomDoubleAttribute(
                zhihui_zhewan_birangcao::kAttrThickness, optional));
            values.push_back(attributes->CreateCustomDoubleArrayAttribute(
                zhihui_zhewan_birangcao::kAttrPickPoint, optional));
            values.push_back(attributes->CreateCustomDoubleArrayAttribute(
                zhihui_zhewan_birangcao::kAttrToolTransforms, optional));
            data = workPart->Features()->CustomFeatureDataCollection()->CreateData(
                featureClass_, values);
        }
        else
        {
            data = editedFeature_->FeatureData();
        }

        std::vector<double> flattenedTransforms;
        flattenedTransforms.reserve(
            transforms.size() * zhihui_zhewan_birangcao::kTransformValueCount);
        for (const SlotToolTransform& transform : transforms)
        {
            flattenedTransforms.insert(
                flattenedTransforms.end(),
                {transform.origin.X,
                 transform.origin.Y,
                 transform.origin.Z,
                 transform.xDirection.X,
                 transform.xDirection.Y,
                 transform.xDirection.Z,
                 transform.yDirection.X,
                 transform.yDirection.Y,
                 transform.yDirection.Z,
                 transform.zDirection.X,
                 transform.zDirection.Y,
                 transform.zDirection.Z});
        }

        const auto assignCurrentDialogValues =
            [&](NXOpen::Features::CustomFeatureData* destination)
        {
            destination->CustomTagAttributeByName(
                    zhihui_zhewan_birangcao::kAttrTargetBody)
                ->SetValue(targetBody);
            destination->CustomTagAttributeByName(
                    zhihui_zhewan_birangcao::kAttrSelectedFace)
                ->SetValue(selectedFace);
            destination->CustomTagAttributeByName(
                    zhihui_zhewan_birangcao::kAttrSelectedEdge)
                ->SetValue(selectedEdge);
            destination->CustomDoubleAttributeByName(
                    zhihui_zhewan_birangcao::kAttrSlotWidth)
                ->SetValue(slotWidth);
            destination->CustomDoubleAttributeByName(
                    zhihui_zhewan_birangcao::kAttrSlotDepth)
                ->SetValue(slotDepth);
            destination->CustomDoubleAttributeByName(
                    zhihui_zhewan_birangcao::kAttrThickness)
                ->SetValue(thickness);
            destination->CustomDoubleArrayAttributeByName(
                    zhihui_zhewan_birangcao::kAttrPickPoint)
                ->SetValues({pickPoint.X, pickPoint.Y, pickPoint.Z});
            destination->CustomDoubleArrayAttributeByName(
                    zhihui_zhewan_birangcao::kAttrToolTransforms)
                ->SetValues(flattenedTransforms);
        };
        assignCurrentDialogValues(data);

        const bool editingExistingFeature = editedFeature_ != nullptr;
        builder->SetFeatureData(data);

        NXOpen::Features::Feature* feature = nullptr;
        try
        {
            feature = builder->CommitFeature();
            if (editingExistingFeature)
            {
                // Editing can defer the CustomFeature's internal-feature
                // callbacks until the next model update. Those callbacks edit
                // the existing UDF and placement feature in place.
                ForceModelUpdate();
            }
        }
        catch (...)
        {
            throw;
        }
        builder->Destroy();
        builder = nullptr;
        if (feature == nullptr)
        {
            return ReportSlotFailure("commit editable custom feature");
        }
        feature->SetName(zhihui_zhewan_birangcao::kFeatureDisplayName);
        gLastSlotFailure.clear();
        return true;
    }
    catch (const NXOpen::NXException& ex)
    {
        if (builder != nullptr)
        {
            builder->Destroy();
        }
        return ReportSlotFailure("commit editable custom feature: " +
                                     std::string(ex.Message()),
                                 ex.ErrorCode());
    }
}

bool ZheWanBiRangCaoDialog::CreateSlotCustomFeatureAtEnd(
    NXOpen::Edge* selectedEdge,
    NXOpen::Face* selectedFace,
    const NXOpen::Point3d& selectionPickPoint,
    const NXOpen::Point3d& endpoint,
    const NXOpen::Point3d& otherEndpoint,
    double slotWidth,
    double slotDepth,
    double thickness,
    bool previewToolOnly)
{
    NXOpen::Body* targetBody = selectedEdge != nullptr ? selectedEdge->GetBody() : nullptr;
    NXOpen::Part* workPart = session_->Parts()->Work();
    if (targetBody == nullptr || selectedFace == nullptr || workPart == nullptr ||
        slotWidth <= 0.0 || slotDepth <= 0.0 || thickness <= 0.0)
    {
        return ReportSlotFailure("invalid face, body, work part, or parameter");
    }

    NXOpen::Vector3d endpointInwardDirection = Subtract(otherEndpoint, endpoint);
    if (Magnitude(endpointInwardDirection) < kVectorTolerance)
    {
        return ReportSlotFailure("reference edge has zero length");
    }
    endpointInwardDirection = Normalize(endpointInwardDirection);

    NXOpen::Point3d selectedEdgeStart;
    NXOpen::Point3d selectedEdgeEnd;
    selectedEdge->GetVertices(&selectedEdgeStart, &selectedEdgeEnd);
    NXOpen::Vector3d nearestEdgeDirection = Subtract(selectedEdgeEnd, selectedEdgeStart);
    if (Magnitude(nearestEdgeDirection) < kVectorTolerance)
    {
        return ReportSlotFailure("nearest edge has zero length");
    }
    nearestEdgeDirection = Normalize(nearestEdgeDirection);
    const NXOpen::Point3d perpendicularPoint =
        ClosestPointOnLine(selectionPickPoint, selectedEdgeStart, nearestEdgeDirection);

    const NXOpen::Vector3d outwardNormal =
        AskSelectedFaceOuterNormal(selectedFace, targetBody, selectionPickPoint);
    NXOpen::Vector3d yDirection = Subtract(perpendicularPoint, selectionPickPoint);
    // Remove any numerical component normal to the picked face so Y remains
    // in the face plane and points from the click position to the edge foot.
    yDirection = Subtract(yDirection, Scale(outwardNormal, Dot(yDirection, outwardNormal)));
    if (Magnitude(yDirection) < kVectorTolerance)
    {
        return ReportSlotFailure("pick point is too close to the nearest edge to define Y");
    }
    yDirection = Normalize(yDirection);
    NXOpen::Vector3d xDirection = Cross(yDirection, outwardNormal);
    if (Magnitude(xDirection) < kVectorTolerance)
    {
        return ReportSlotFailure("failed to derive X from Y and the face outward normal");
    }
    xDirection = Normalize(xDirection);

    // Each origin is extended outwards from its reference-edge endpoint by
    // half the dialog slot width.  The extension direction is independent of
    // local X: A extends opposite A->B and B extends opposite B->A.
    const NXOpen::Point3d origin =
        Add(endpoint, Scale(endpointInwardDirection, -0.5 * slotWidth));

    std::set<tag_t> bodyTagsBefore;
    for (NXOpen::Body* body : *workPart->Bodies())
    {
        if (body != nullptr)
        {
            bodyTagsBefore.insert(body->Tag());
        }
    }

    ExtractedReliefUdfTemplate extractedTemplate;
    std::string templatePath;
    if (!extractedTemplate.Extract(templatePath))
    {
        return ReportSlotFailure("extract embedded UDF template");
    }

    UF_PART_load_status_t loadStatus;
    std::memset(&loadStatus, 0, sizeof(loadStatus));
    tag_t templatePart = NULL_TAG;
    const int openResult = UF_PART_open_quiet(templatePath.c_str(), &templatePart, &loadStatus);
    UF_PART_free_load_status(&loadStatus);
    if (openResult != 0 || templatePart == NULL_TAG)
    {
        return ReportSlotFailure("open embedded UDF template", openResult);
    }

    auto closeTemplate = [&]()
    {
        if (templatePart != NULL_TAG)
        {
            UF_PART_close(templatePart, 0, 1);
            templatePart = NULL_TAG;
        }
    };

    const tag_t udfDefinition = FindUdfDefinitionFeature(templatePart);
    if (udfDefinition == NULL_TAG)
    {
        closeTemplate();
        return ReportSlotFailure("find UDF definition in template");
    }

    UF_MODL_udf_exp_data_t expressionData;
    UF_MODL_udf_ref_data_t referenceData;
    UF_MODL_udf_init_exp_data(&expressionData);
    UF_MODL_udf_init_ref_data(&referenceData);
    const int initUdfResult = UF_MODL_udf_init_insert_data_from_def(udfDefinition,
                                                                    &expressionData,
                                                                    &referenceData);
    if (initUdfResult != 0 ||
        expressionData.num_exps != 3 ||
        !AllocateUdfExpressionValues(expressionData, slotDepth, slotWidth, thickness))
    {
        const bool expressionCountMismatch = expressionData.num_exps != 3;
        UF_MODL_udf_free_exp_data(&expressionData);
        UF_MODL_udf_free_ref_data(&referenceData);
        closeTemplate();
        return ReportSlotFailure(
            expressionCountMismatch ? "UDF template must contain exactly three expressions"
                                    : "initialize UDF insertion data",
            initUdfResult);
    }

    // The revised UDF is positioned by the XZ datum plane, X datum axis and
    // origin point of a target Datum CSYS.  This gives the UDF its final
    // coordinates at creation time and removes the extra Move Object feature.
    tag_t datumCsysTag = NULL_TAG;
    tag_t smartCsysTag = NULL_TAG;
    tag_t datumOriginTag = NULL_TAG;
    tag_t datumAxes[3] = {NULL_TAG, NULL_TAG, NULL_TAG};
    tag_t datumPlanes[3] = {NULL_TAG, NULL_TAG, NULL_TAG};
    NXOpen::Features::DatumCsysBuilder* datumBuilder = nullptr;
    try
    {
        NXOpen::CartesianCoordinateSystem* targetCsys =
            workPart->CoordinateSystems()->CreateCoordinateSystem(
                origin, xDirection, yDirection);
        if (targetCsys == nullptr)
        {
            throw NXOpen::NXException::Create(1, "Failed to create the target coordinate system.");
        }
        datumBuilder = workPart->Features()->CreateDatumCsysBuilder(nullptr);
        datumBuilder->SetCsys(targetCsys);
        NXOpen::Features::Feature* datumFeature = datumBuilder->CommitFeature();
        datumBuilder->Destroy();
        datumBuilder = nullptr;
        datumCsysTag = datumFeature != nullptr ? datumFeature->Tag() : NULL_TAG;
        if (datumCsysTag == NULL_TAG)
        {
            throw NXOpen::NXException::Create(1, "Datum CSYS returned no feature.");
        }
        UF_MODL_ask_datum_csys_components(
            datumCsysTag, &smartCsysTag, &datumOriginTag, datumAxes, datumPlanes);
        if (datumOriginTag == NULL_TAG || datumAxes[0] == NULL_TAG ||
            datumPlanes[0] == NULL_TAG)
        {
            throw NXOpen::NXException::Create(1, "Datum CSYS components are unavailable.");
        }
        static_cast<void>(UF_MODL_set_datum_csys_visibility(datumCsysTag, FALSE));
    }
    catch (const NXOpen::NXException& ex)
    {
        if (datumBuilder != nullptr)
        {
            datumBuilder->Destroy();
        }
        DeleteObjectIfAlive(datumCsysTag);
        UF_MODL_udf_free_exp_data(&expressionData);
        UF_MODL_udf_free_ref_data(&referenceData);
        closeTemplate();
        return ReportSlotFailure("create UDF placement coordinate system: " +
                                     std::string(ex.Message()),
                                 ex.ErrorCode());
    }

    int allocationError = 0;
    referenceData.new_refs = static_cast<tag_t*>(UF_allocate_memory(
        static_cast<unsigned int>(sizeof(tag_t) * referenceData.num_refs),
        &allocationError));
    if (referenceData.reverse_refs_dir == nullptr)
    {
        referenceData.reverse_refs_dir = static_cast<UF_MODL_udf_reverse_dir_t*>(UF_allocate_memory(
            static_cast<unsigned int>(sizeof(UF_MODL_udf_reverse_dir_t) * referenceData.num_refs),
            &allocationError));
    }
    const bool referenceCountMismatch = referenceData.num_refs != 3;
    if (referenceCountMismatch || allocationError != 0 ||
        referenceData.new_refs == nullptr || referenceData.reverse_refs_dir == nullptr)
    {
        UF_MODL_udf_free_exp_data(&expressionData);
        UF_MODL_udf_free_ref_data(&referenceData);
        closeTemplate();
        DeleteObjectIfAlive(datumCsysTag);
        return ReportSlotFailure(
            referenceCountMismatch
                ? "UDF template must contain exactly three coordinate references"
                : "allocate the UDF coordinate references",
            allocationError);
    }
    referenceData.new_refs[0] = datumPlanes[0];
    referenceData.new_refs[1] = datumAxes[0];
    referenceData.new_refs[2] = datumOriginTag;
    for (int index = 0; index < referenceData.num_refs; ++index)
    {
        referenceData.reverse_refs_dir[index] = UF_MODL_UDF_KEEP_DIR;
    }

    tag_t udfTag = NULL_TAG;
    const int createResult = UF_MODL_create_instantiated_udf1(
        udfDefinition, &expressionData, &referenceData, &udfTag);
    UF_MODL_udf_free_exp_data(&expressionData);
    UF_MODL_udf_free_ref_data(&referenceData);
    closeTemplate();
    if (createResult != 0 || udfTag == NULL_TAG)
    {
        DeleteObjectIfAlive(udfTag);
        DeleteObjectIfAlive(datumCsysTag);
        return ReportSlotFailure("instantiate UDF", createResult);
    }

    std::vector<tag_t> toolBodyTags;
    for (NXOpen::Body* body : *workPart->Bodies())
    {
        if (body != nullptr && body != targetBody &&
            bodyTagsBefore.find(body->Tag()) == bodyTagsBefore.end())
        {
            toolBodyTags.push_back(body->Tag());
        }
    }
    if (toolBodyTags.size() != 1)
    {
        for (tag_t toolBodyTag : toolBodyTags)
        {
            DeleteObjectIfAlive(toolBodyTag);
        }
        DeleteObjectIfAlive(udfTag);
        DeleteObjectIfAlive(datumCsysTag);
        return ReportSlotFailure("find the single tool body created by the UDF");
    }
    tag_t transformedToolBodyTag = toolBodyTags.front();

    // Make the directly positioned UDF current before querying its body or
    // performing the separate Boolean subtract.
    try
    {
        ForceModelUpdate();
    }
    catch (const NXOpen::NXException& ex)
    {
        DeleteObjectIfAlive(udfTag);
        DeleteObjectIfAlive(datumCsysTag);
        return ReportSlotFailure("update positioned UDF before interference: " +
                                     std::string(ex.Message()),
                                 ex.ErrorCode());
    }

    // Updating a parametric UDF may replace the generated body object.  Resolve
    // the live body again instead of retaining the tag captured before update.
    std::vector<tag_t> updatedToolBodyTags;
    for (NXOpen::Body* body : *workPart->Bodies())
    {
        if (body != nullptr && body != targetBody &&
            bodyTagsBefore.find(body->Tag()) == bodyTagsBefore.end())
        {
            updatedToolBodyTags.push_back(body->Tag());
        }
    }
    if (updatedToolBodyTags.size() != 1)
    {
        DeleteObjectIfAlive(udfTag);
        DeleteObjectIfAlive(datumCsysTag);
        return ReportSlotFailure("find the updated tool body created by the UDF");
    }
    transformedToolBodyTag = updatedToolBodyTags.front();
    toolBodyTags[0] = transformedToolBodyTag;

    if (previewToolOnly)
    {
        previewFeatureTags_.push_back(datumCsysTag);
        previewFeatureTags_.push_back(udfTag);
        ForceModelUpdate();
        gLastSlotFailure.clear();
        return true;
    }

    double targetBox[6] = {};
    double toolBox[6] = {};
    const int targetBoxResult = UF_MODL_ask_bounding_box(targetBody->Tag(), targetBox);
    const int toolBoxResult = UF_MODL_ask_bounding_box(transformedToolBodyTag, toolBox);
    std::ostringstream placementTrace;
    placementTrace << "ZheWanBiRangCao: placed relief UDF"
                   << ", origin=(" << origin.X << ',' << origin.Y << ',' << origin.Z << ')'
                   << ", X=(" << xDirection.X << ',' << xDirection.Y << ',' << xDirection.Z << ')'
                   << ", Y=(" << yDirection.X << ',' << yDirection.Y << ',' << yDirection.Z << ')'
                   << ", Z=(" << outwardNormal.X << ',' << outwardNormal.Y << ',' << outwardNormal.Z << ')'
                   << ", thickness=" << thickness;
    if (targetBoxResult == 0 && toolBoxResult == 0)
    {
        placementTrace << ", targetBox=[" << targetBox[0] << ',' << targetBox[1] << ',' << targetBox[2]
                       << "]-[" << targetBox[3] << ',' << targetBox[4] << ',' << targetBox[5] << ']'
                       << ", toolBox=[" << toolBox[0] << ',' << toolBox[1] << ',' << toolBox[2]
                       << "]-[" << toolBox[3] << ',' << toolBox[4] << ',' << toolBox[5] << ']';
    }
    placementTrace << '\n';
    std::string placementMessage = placementTrace.str();
    UF_print_syslog(placementMessage.data(), FALSE);

    // UF_MODL_check_interference distinguishes actual solid interference from
    // coincident-face touching.  The former SimpleInterference face method can
    // report intersecting/touching faces even when there is no volume to cut.
    int interferenceStatus = 0;
    tag_t interferenceToolTag = transformedToolBodyTag;
    const int interferenceCheckResult = UF_MODL_check_interference(
        targetBody->Tag(), 1, &interferenceToolTag, &interferenceStatus);
    // The UF documentation requires an update after this check to remove its
    // temporary modeling objects.
    const int interferenceCleanupResult = UF_MODL_update();

    std::ostringstream interferenceTrace;
    interferenceTrace << "ZheWanBiRangCao: solid interference check"
                      << ", apiResult=" << interferenceCheckResult
                      << ", status=" << interferenceStatus
                      << ", cleanupResult=" << interferenceCleanupResult << '\n';
    std::string interferenceMessage = interferenceTrace.str();
    UF_print_syslog(interferenceMessage.data(), FALSE);

    if (interferenceCheckResult != 0 || interferenceCleanupResult != 0)
    {
        DeleteObjectIfAlive(udfTag);
        DeleteObjectIfAlive(datumCsysTag);
        return ReportSlotFailure(interferenceCheckResult != 0
                                     ? "solid-body interference check"
                                     : "clean up solid-body interference check",
                                 interferenceCheckResult != 0
                                     ? interferenceCheckResult
                                     : interferenceCleanupResult);
    }

    if (interferenceStatus != 1)
    {
        DeleteObjectIfAlive(udfTag);
        DeleteObjectIfAlive(datumCsysTag);
        const char* reason =
            interferenceStatus == 3
                ? "tool body only touches the target; custom feature deleted"
                : (interferenceStatus == 2
                       ? "tool body does not intersect the target; custom feature deleted"
                       : "body-interference check could not be performed; custom feature deleted");
        return ReportSlotFailure(reason);
    }

    NXOpen::Body* toolBody = dynamic_cast<NXOpen::Body*>(
        NXOpen::NXObjectManager::Get(transformedToolBodyTag));
    if (toolBody == nullptr)
    {
        DeleteObjectIfAlive(udfTag);
        DeleteObjectIfAlive(datumCsysTag);
        return ReportSlotFailure("the positioned UDF tool body is unavailable");
    }

    std::vector<tag_t> booleanFeatureTags;
    NXOpen::Features::BooleanBuilder* booleanBuilder = nullptr;
    try
    {
        booleanBuilder = workPart->Features()->CreateBooleanBuilder(nullptr);
        booleanBuilder->SetOperation(NXOpen::Features::Feature::BooleanTypeSubtract);
        booleanBuilder->SetTarget(targetBody);
#pragma warning(push)
#pragma warning(disable : 4996)
        booleanBuilder->SetTool(toolBody);
#pragma warning(pop)
        booleanBuilder->SetRetainTarget(false);
        booleanBuilder->SetRetainTool(false);
        NXOpen::NXObject* result = booleanBuilder->Commit();
        const tag_t booleanTag = result != nullptr ? result->Tag() : NULL_TAG;
        booleanBuilder->Destroy();
        booleanBuilder = nullptr;
        if (booleanTag == NULL_TAG)
        {
            throw NXOpen::NXException::Create(1, "Boolean subtract returned no feature.");
        }
        booleanFeatureTags.push_back(booleanTag);
    }
    catch (const NXOpen::NXException& ex)
    {
        if (booleanBuilder != nullptr)
        {
            try
            {
                booleanBuilder->Destroy();
            }
            catch (...)
            {
            }
        }
        DeleteObjectIfAlive(udfTag);
        DeleteObjectIfAlive(datumCsysTag);
        return ReportSlotFailure("Boolean subtract: " + std::string(ex.Message()), ex.ErrorCode());
    }
    catch (const std::exception& ex)
    {
        if (booleanBuilder != nullptr)
        {
            try
            {
                booleanBuilder->Destroy();
            }
            catch (...)
            {
            }
        }
        DeleteObjectIfAlive(udfTag);
        DeleteObjectIfAlive(datumCsysTag);
        return ReportSlotFailure("Boolean subtract: " + std::string(ex.what()));
    }
    catch (...)
    {
        if (booleanBuilder != nullptr)
        {
            try
            {
                booleanBuilder->Destroy();
            }
            catch (...)
            {
            }
        }
        DeleteObjectIfAlive(udfTag);
        DeleteObjectIfAlive(datumCsysTag);
        return ReportSlotFailure("Boolean subtract: unknown failure");
    }

    previewFeatureTags_.push_back(datumCsysTag);
    previewFeatureTags_.push_back(udfTag);
    previewFeatureTags_.insert(previewFeatureTags_.end(),
                               booleanFeatureTags.begin(),
                               booleanFeatureTags.end());
    ForceModelUpdate();
    gLastSlotFailure.clear();
    return true;
}

bool ZheWanBiRangCaoDialog::EditSlotOutline(SlotFeatureRecord& record, double slotWidth, double slotDepth) const
{
    const NXOpen::Point3d pointC = Add(record.pointB, Scale(record.depthDirection, slotDepth));
    const NXOpen::Point3d pointD = Add(pointC, Scale(record.widthDirection, slotWidth));
    const NXOpen::Point3d pointE = Add(record.pointA, Scale(record.widthDirection, slotWidth));

    bool updated = true;
    updated = EditLineBetweenPoints(record.lines[0], record.pointA, record.pointB) && updated;
    updated = EditLineBetweenPoints(record.lines[1], record.pointB, pointC) && updated;
    updated = EditLineBetweenPoints(record.lines[2], pointC, pointD) && updated;
    updated = EditLineBetweenPoints(record.lines[3], pointD, pointE) && updated;
    updated = EditLineBetweenPoints(record.lines[4], pointE, record.pointA) && updated;
    if (updated)
    {
        ForceModelUpdate();
    }
    return updated;
}

void ZheWanBiRangCaoDialog::HideObject(tag_t objectTag) const
{
    if (objectTag != NULL_TAG)
    {
        static_cast<void>(UF_OBJ_set_blank_status(objectTag, UF_OBJ_BLANKED));
    }
}

void ZheWanBiRangCaoDialog::HideObjects(const std::vector<tag_t>& objectTags) const
{
    for (tag_t objectTag : objectTags)
    {
        HideObject(objectTag);
    }
}

void ZheWanBiRangCaoDialog::CleanupHiddenTemporaryObjects()
{
    DeleteObjects(hiddenTemporaryTags_);
    hiddenTemporaryTags_.clear();
}

void ZheWanBiRangCaoDialog::CleanupPreviewObjects()
{
    for (auto iterator = previewFeatureTags_.rbegin();
         iterator != previewFeatureTags_.rend();
         ++iterator)
    {
        DeleteObjectIfAlive(*iterator);
    }
    previewFeatureTags_.clear();
}

NXOpen::Vector3d ZheWanBiRangCaoDialog::AskSelectedFaceInnerNormal(NXOpen::Face* face,
                                                                   NXOpen::Body* body,
                                                                   const NXOpen::Point3d& referencePoint,
                                                                   double thickness) const
{
    if (face == nullptr)
    {
        throw NXOpen::NXException::Create(1, "Planar face is required.");
    }

    NXOpen::Part* workPart = session_->Parts()->Work();
    if (workPart == nullptr)
    {
        throw NXOpen::NXException::Create(1, "Failed to get work part.");
    }

    NXOpen::Point* normalPoint = workPart->Points()->CreatePoint(referencePoint);
    normalPoint->Blank();
    NXOpen::Direction* normalDirection =
        workPart->Directions()->CreateDirection(
            face,
            normalPoint,
            NXOpen::SenseForward,
            NXOpen::SmartObject::UpdateOptionWithinModeling);
    NXOpen::Vector3d normal = Normalize(normalDirection->Vector());

    const double probeDistance = std::max(thickness * 0.5, 0.5);
    const int positiveStatus = AskPointContainmentStatus(body, Add(referencePoint, Scale(normal, probeDistance)));
    if (positiveStatus == 1 || positiveStatus == 3)
    {
        return normal;
    }

    const int negativeStatus = AskPointContainmentStatus(body, Add(referencePoint, Scale(normal, -probeDistance)));
    if (negativeStatus == 1 || negativeStatus == 3)
    {
        return Scale(normal, -1.0);
    }

    return Scale(normal, -1.0);
}

bool ZheWanBiRangCaoDialog::ExtrudeSubtractAndDeleteCurves(NXOpen::Body* targetBody,
                                                           NXOpen::Face* selectedFace,
                                                           const NXOpen::Point3d& referencePoint,
                                                           const std::vector<tag_t>& curveTags,
                                                           double thickness)
{
    if (targetBody == nullptr || selectedFace == nullptr || curveTags.empty() || thickness <= 0.0)
    {
        return false;
    }

    tag_t toolFeatureTag = NULL_TAG;
    tag_t toolBodyTag = NULL_TAG;
    uf_list_p_t curveList = NULL;
    uf_list_p_t featureList = NULL;

    try
    {
        if (UF_MODL_create_list(&curveList) != 0 || curveList == NULL)
        {
            return false;
        }
        for (tag_t curveTag : curveTags)
        {
            if (curveTag != NULL_TAG && UF_MODL_put_list_item(curveList, curveTag) != 0)
            {
                UF_MODL_delete_list(&curveList);
                return false;
            }
        }

        const NXOpen::Vector3d innerNormal = AskSelectedFaceInnerNormal(selectedFace, targetBody, referencePoint, thickness);
        std::string startLimit = "0";
        std::string endLimit = FormatDouble(thickness);
        char taperAngle[] = "0";
        char* limits[2] = {const_cast<char*>(startLimit.c_str()), const_cast<char*>(endLimit.c_str())};
        double origin[3] = {referencePoint.X, referencePoint.Y, referencePoint.Z};
        double direction[3] = {innerNormal.X, innerNormal.Y, innerNormal.Z};

        const int extrudeResult = UF_MODL_create_extruded(
            curveList,
            taperAngle,
            limits,
            origin,
            direction,
            UF_NULLSIGN,
            &featureList);
        UF_MODL_delete_list(&curveList);
        curveList = NULL;
        if (extrudeResult != 0 || featureList == NULL)
        {
            return false;
        }

        int featureCount = 0;
        if (UF_MODL_ask_list_count(featureList, &featureCount) == 0 && featureCount > 0)
        {
            static_cast<void>(UF_MODL_ask_list_item(featureList, 0, &toolFeatureTag));
        }
        UF_MODL_delete_list(&featureList);
        featureList = NULL;

        if (toolFeatureTag == NULL_TAG || UF_MODL_ask_feat_body(toolFeatureTag, &toolBodyTag) != 0 || toolBodyTag == NULL_TAG)
        {
            DeleteObjectIfAlive(toolFeatureTag);
            return false;
        }

        uf_list_p_t bodyList = NULL;
        if (UF_MODL_create_list(&bodyList) == 0 && bodyList != NULL)
        {
            if (UF_MODL_put_list_item(bodyList, toolBodyTag) == 0)
            {
                static_cast<void>(UF_MODL_delete_body_parms(bodyList));
            }
            UF_MODL_delete_list(&bodyList);
        }

        const std::vector<tag_t> featuresBefore = AskBodyFeatureTags(targetBody->Tag());
        int resultCount = 0;
        tag_t* resultingBodies = NULL;
        const int subtractResult =
            UF_MODL_subtract_bodies(targetBody->Tag(), toolBodyTag, &resultCount, &resultingBodies);
        if (resultingBodies != NULL)
        {
            UF_free(resultingBodies);
        }
        if (subtractResult != 0)
        {
            HideObject(toolBodyTag);
            hiddenTemporaryTags_.push_back(toolBodyTag);
            return false;
        }

        const std::vector<tag_t> featuresAfter = AskBodyFeatureTags(targetBody->Tag());
        for (tag_t featureTag : featuresAfter)
        {
            if (featureTag != NULL_TAG && !ContainsTag(featuresBefore, featureTag) && featureTag != toolFeatureTag)
            {
                previewFeatureTags_.push_back(featureTag);
            }
        }
        DeleteObjectIfAlive(toolBodyTag);
        ForceModelUpdate();
        return true;
    }
    catch (...)
    {
        if (curveList != NULL)
        {
            UF_MODL_delete_list(&curveList);
        }
        if (featureList != NULL)
        {
            UF_MODL_delete_list(&featureList);
        }
        HideObject(toolBodyTag);
        if (toolBodyTag != NULL_TAG)
        {
            hiddenTemporaryTags_.push_back(toolBodyTag);
        }
        return false;
    }
}

bool ZheWanBiRangCaoDialog::UpdateAllSlots()
{
    return false;
}

int ZheWanBiRangCaoDialog::Preview()
{
    gLastSlotFailure.clear();
    NXOpen::Face* selectedFace = GetSelectedFace();
    if (selectedFace == nullptr)
    {
        return 0;
    }

    const NXOpen::Point3d pickPoint = GetSelectionPickPoint();
    NXOpen::Edge* edge = FindClosestLinearEdgeOnFace(selectedFace, pickPoint);
    if (edge == nullptr || edge->SolidEdgeType() != NXOpen::Edge::EdgeTypeLinear)
    {
        return 0;
    }

    try
    {
        const SlotParameters parameters{GetSlotWidthY(), GetSlotDepthZ(), 0.5, 0.5, 6.0};
        double selectedFaceBox[6] = {};
        const NXOpen::Vector3d selectedFaceNormal =
            AskPlanarFaceNormal(selectedFace, selectedFaceBox);
        const NXOpen::Point3d selectedPlanePoint = FaceBoxCenter(selectedFaceBox);
        const std::vector<SlotReferenceEdge> referenceEdges =
            FindInnerReferenceEdges(edge,
                                    selectedFace,
                                    selectedPlanePoint,
                                    selectedFaceNormal);
        const std::vector<SlotEndCandidate> endCandidates =
            BuildSlotEndCandidates(referenceEdges, pickPoint);
        const double thickness = EstimateThickness(edge->GetBody(), selectedFace);

        for (const SlotEndCandidate& candidate : endCandidates)
        {
            static_cast<void>(CreateSlotCustomFeatureAtEnd(edge,
                                                            selectedFace,
                                                            pickPoint,
                                                            candidate.endpoint,
                                                            candidate.otherEndpoint,
                                                            parameters.slotWidth,
                                                            parameters.slotDepth,
                                                            thickness,
                                                            false));
        }
        return 0;
    }
    catch (...)
    {
        CleanupPreviewObjects();
        CleanupHiddenTemporaryObjects();
        return 0;
    }
}

int ZheWanBiRangCaoDialog::Execute()
{
    SaveDialogState();
    gLastSlotFailure.clear();

    NXOpen::Face* selectedFace = GetSelectedFace();
    if (selectedFace == nullptr)
    {
        ShowError("Please select one planar face near the bend root edge.");
        return 1;
    }

    const NXOpen::Point3d pickPoint = GetSelectionPickPoint();
    NXOpen::Edge* edge = FindClosestLinearEdgeOnFace(selectedFace, pickPoint);
    if (edge == nullptr)
    {
        ShowError("No linear bend root edge was found near the picked face position.");
        return 1;
    }

    if (edge->SolidEdgeType() != NXOpen::Edge::EdgeTypeLinear)
    {
        ShowError("Only linear bend root edges are supported.");
        return 1;
    }

    try
    {
        NXOpen::Face* largerFace = selectedFace;
        if (largerFace == nullptr)
        {
            throw NXOpen::NXException::Create(1, "No usable adjacent planar face was found.");
        }

        const SlotParameters parameters{GetSlotWidthY(), GetSlotDepthZ(), 0.5, 0.5, 6.0};
        double selectedFaceBox[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
        const NXOpen::Vector3d selectedFaceNormal = AskPlanarFaceNormal(largerFace, selectedFaceBox);
        const NXOpen::Point3d selectedPlanePoint = FaceBoxCenter(selectedFaceBox);

        const std::vector<SlotReferenceEdge> referenceEdges =
            FindInnerReferenceEdges(edge, largerFace, selectedPlanePoint, selectedFaceNormal);
        const std::vector<SlotEndCandidate> endCandidates =
            BuildSlotEndCandidates(referenceEdges, pickPoint);
        const double thickness = EstimateThickness(edge->GetBody(), largerFace);

        std::vector<SlotToolTransform> transforms;
        transforms.reserve(endCandidates.size());
        for (const SlotEndCandidate& candidate : endCandidates)
        {
            SlotToolTransform transform;
            if (BuildSlotToolTransformAtEnd(edge,
                                            largerFace,
                                            pickPoint,
                                            candidate.endpoint,
                                            candidate.otherEndpoint,
                                            parameters.slotWidth,
                                            parameters.slotDepth,
                                            thickness,
                                            transform))
            {
                transforms.push_back(transform);
            }
        }

        if (transforms.empty() ||
            !CommitEditableCustomFeature(edge->GetBody(),
                                         largerFace,
                                         edge,
                                         pickPoint,
                                         parameters.slotWidth,
                                         parameters.slotDepth,
                                         thickness,
                                         transforms))
        {
            std::string message = "No relief slot was created.";
            if (!gLastSlotFailure.empty())
            {
                message += "\nFailure stage: " + gLastSlotFailure;
            }
            ShowError(message);
            return 1;
        }

        return 0;
    }
    catch (const NXOpen::NXException& ex)
    {
        ShowError(ex.Message());
        return ex.ErrorCode();
    }
    catch (const std::exception& ex)
    {
        ShowError(ex.what());
        return 1;
    }
}
