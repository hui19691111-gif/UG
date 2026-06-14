#include "ZheWanBiRangCao.hpp"
#include "../../common/ZhihuiEmbeddedDialog.hpp"
#include "embedded_dialog_resources.h"

#ifdef CreateDialog
#undef CreateDialog
#endif

#include <NXOpen/BlockStyler_PropertyList.hxx>
#include <stdexcept>
#include <NXOpen/BasePart.hxx>
#include <NXOpen/CurveDumbRule.hxx>
#include <NXOpen/Direction.hxx>
#include <NXOpen/DirectionCollection.hxx>
#include <NXOpen/BodyDumbRule.hxx>
#include <NXOpen/Features_BooleanBuilder.hxx>
#include <NXOpen/Features_BooleanFeature.hxx>
#include <NXOpen/Features_ExtrudeBuilder.hxx>
#include <NXOpen/Features_Feature.hxx>
#include <NXOpen/Features_FeatureCollection.hxx>
#include <NXOpen/GeometricUtilities_BooleanOperation.hxx>
#include <NXOpen/GeometricUtilities_Extend.hxx>
#include <NXOpen/GeometricUtilities_FeatureOptions.hxx>
#include <NXOpen/GeometricUtilities_Limits.hxx>
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
#include <uf_modl_sweep.h>
#include <uf_obj.h>
#include <uf_ui_types.h>

#include <windows.h>

#include <cmath>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

namespace
{
constexpr double kVectorTolerance = 1.0e-6;
const char* kDialogStatePath = "D:\\UG智辉钣金插件\\config\\ZheWanBiRangCao_state.ini";

double ReadStateDouble(const char* key, const double fallbackValue)
{
    char buffer[128] = {};
    GetPrivateProfileStringA("Dialog", key, "", buffer, static_cast<DWORD>(sizeof(buffer)), kDialogStatePath);
    if (buffer[0] == '\0')
    {
        return fallbackValue;
    }

    char* end = nullptr;
    const double value = std::strtod(buffer, &end);
    return end != buffer ? value : fallbackValue;
}

void WriteStateDouble(const char* key, const double value)
{
    char buffer[64] = {};
    std::snprintf(buffer, sizeof(buffer), "%.15g", value);
    WritePrivateProfileStringA("Dialog", key, buffer, kDialogStatePath);
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
    if (objectTag != NULL_TAG)
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
      slotWidthBlock_(nullptr),
      slotDepthBlock_(nullptr),
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
}

ZheWanBiRangCaoDialog::~ZheWanBiRangCaoDialog()
{
    CleanupHiddenTemporaryObjects();

    if (dialog_ != nullptr)
    {
        delete dialog_;
        dialog_ = nullptr;
    }
}

NXOpen::BlockStyler::BlockDialog::DialogResponse ZheWanBiRangCaoDialog::Launch()
{
    return dialog_->Launch();
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
}

bool ZheWanBiRangCaoDialog::enable_ok_cb()
{
    return GetSelectedFace() != nullptr || !slotRecords_.empty();
}

int ZheWanBiRangCaoDialog::update_cb(NXOpen::BlockStyler::UIBlock* block)
{
    if (block == edgeSelectBlock_)
    {
        CleanupPreviewObjects();
        CleanupHiddenTemporaryObjects();
        return Execute();
    }

    if (block == slotWidthBlock_ || block == slotDepthBlock_)
    {
        SaveDialogState();
        if (GetSelectedFace() != nullptr)
        {
            CleanupPreviewObjects();
            CleanupHiddenTemporaryObjects();
            return Execute();
        }
    }

    return 0;
}

int ZheWanBiRangCaoDialog::apply_cb()
{
    SaveDialogState();

    if (GetSelectedFace() != nullptr)
    {
        previewFeatureTags_.clear();
        CleanupHiddenTemporaryObjects();
        ClearSelectedEdge();
        return 0;
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
    SetDoubleValue(slotWidthBlock_, ReadStateDouble("slotWidthY", GetSlotWidthY()));
    SetDoubleValue(slotDepthBlock_, ReadStateDouble("slotDepthZ", GetSlotDepthZ()));
}

void ZheWanBiRangCaoDialog::SaveDialogState() const
{
    WriteStateDouble("slotWidthY", GetSlotWidthY());
    WriteStateDouble("slotDepthZ", GetSlotDepthZ());
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

bool ZheWanBiRangCaoDialog::CreateSlotOutlineOnSelectedFace(NXOpen::Edge* selectedEdge,
                                                            NXOpen::Face* selectedFace,
                                                            const NXOpen::Point3d& innerPoint,
                                                            const NXOpen::Point3d& selectedPlanePoint,
                                                            const NXOpen::Vector3d& selectedPlaneNormal,
                                                            double slotWidth,
                                                            double slotDepth)
{
    if (selectedEdge == nullptr || selectedFace == nullptr || slotWidth <= 0.0 || slotDepth <= 0.0)
    {
        return false;
    }

    NXOpen::Point3d edgeStart;
    NXOpen::Point3d edgeEnd;
    selectedEdge->GetVertices(&edgeStart, &edgeEnd);
    const NXOpen::Vector3d edgeDirection = Normalize(Subtract(edgeEnd, edgeStart));

    const NXOpen::Point3d pointB = ProjectPointToPlane(innerPoint, selectedPlanePoint, selectedPlaneNormal);
    const NXOpen::Point3d pointA = ClosestPointOnLine(pointB, edgeStart, edgeDirection);
    NXOpen::Vector3d depthDirection = Subtract(pointB, pointA);
    if (Magnitude(depthDirection) < kVectorTolerance)
    {
        return false;
    }
    depthDirection = Normalize(depthDirection);

    NXOpen::Point3d nearEdgeEnd = edgeStart;
    NXOpen::Point3d farEdgeEnd = edgeEnd;
    if (Distance(pointA, edgeEnd) < Distance(pointA, edgeStart))
    {
        nearEdgeEnd = edgeEnd;
        farEdgeEnd = edgeStart;
    }

    NXOpen::Vector3d widthDirection = Subtract(nearEdgeEnd, farEdgeEnd);
    if (Magnitude(widthDirection) < kVectorTolerance)
    {
        widthDirection = edgeDirection;
    }
    widthDirection = Normalize(widthDirection);

    const NXOpen::Point3d pointC = Add(pointB, Scale(depthDirection, slotDepth));
    const NXOpen::Point3d pointD = Add(pointC, Scale(widthDirection, slotWidth));
    const NXOpen::Point3d pointE = Add(pointA, Scale(widthDirection, slotWidth));

    std::vector<tag_t> curveTags;
    curveTags.reserve(5);
    curveTags.push_back(CreateLineBetweenPoints(pointA, pointB));
    curveTags.push_back(CreateLineBetweenPoints(pointB, pointC));
    curveTags.push_back(CreateLineBetweenPoints(pointC, pointD));
    curveTags.push_back(CreateLineBetweenPoints(pointD, pointE));
    curveTags.push_back(CreateLineBetweenPoints(pointE, pointA));

    for (tag_t line : curveTags)
    {
        if (line == NULL_TAG)
        {
            DeleteObjects(curveTags);
            return false;
        }
    }
    HideObjects(curveTags);
    hiddenTemporaryTags_.insert(hiddenTemporaryTags_.end(), curveTags.begin(), curveTags.end());

    const double thickness = EstimateThickness(selectedEdge->GetBody(), selectedFace);
    const bool subtracted =
        ExtrudeSubtractAndDeleteCurves(selectedEdge->GetBody(), selectedFace, pointA, curveTags, thickness);
    return subtracted;
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
    DeleteObjects(previewFeatureTags_);
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

int ZheWanBiRangCaoDialog::Execute()
{
    SaveDialogState();

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

        int createdCount = 0;
        for (const SlotReferenceEdge& referenceEdge : referenceEdges)
        {
            if (CreateSlotOutlineOnSelectedFace(edge,
                                                largerFace,
                                                referenceEdge.startPoint,
                                                selectedPlanePoint,
                                                selectedFaceNormal,
                                                parameters.slotWidth,
                                                parameters.slotDepth))
            {
                ++createdCount;
            }

            if (CreateSlotOutlineOnSelectedFace(edge,
                                                largerFace,
                                                referenceEdge.endPoint,
                                                selectedPlanePoint,
                                                selectedFaceNormal,
                                                parameters.slotWidth,
                                                parameters.slotDepth))
            {
                ++createdCount;
            }
        }

        if (createdCount == 0)
        {
            ShowError("No relief slot was created.");
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
