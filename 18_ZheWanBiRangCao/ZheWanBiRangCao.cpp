#include "ZheWanBiRangCao.hpp"

#ifdef CreateDialog
#undef CreateDialog
#endif

#include <NXOpen/BlockStyler_PropertyList.hxx>
#include <NXOpen/BasePart.hxx>
#include <NXOpen/CurveDumbRule.hxx>
#include <NXOpen/Direction.hxx>
#include <NXOpen/DirectionCollection.hxx>
#include <NXOpen/Features_BooleanBuilder.hxx>
#include <NXOpen/Features_Feature.hxx>
#include <NXOpen/Features_FeatureCollection.hxx>
#include <NXOpen/Features_ToolingBox.hxx>
#include <NXOpen/Features_ToolingBoxBuilder.hxx>
#include <NXOpen/Features_ToolingFeatureCollection.hxx>
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
#include <NXOpen/ScRuleFactory.hxx>
#include <NXOpen/SelectionIntentRule.hxx>
#include <NXOpen/SelectionIntentRuleOptions.hxx>
#include <NXOpen/SmartObject.hxx>
#include <NXOpen/Tooling_ToolingSession.hxx>
#include <NXOpen/UI.hxx>
#include <NXOpen/UnitCollection.hxx>

#include <uf.h>
#include <uf_modl.h>
#include <uf_ui_types.h>

#include <cmath>
#include <algorithm>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

namespace
{
constexpr double kVectorTolerance = 1.0e-6;

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
      slotModeBlock_(nullptr),
      edgeSelectBlock_(nullptr),
      slotWidthBlock_(nullptr),
      slotDepthBlock_(nullptr),
      slotRecords_()
{
    dialog_ = ui_->CreateDialog(GetDialogFilePath().c_str());
    dialog_->AddInitializeHandler(NXOpen::make_callback(this, &ZheWanBiRangCaoDialog::initialize_cb));
    dialog_->AddEnableOKButtonHandler(NXOpen::make_callback(this, &ZheWanBiRangCaoDialog::enable_ok_cb));
    dialog_->AddUpdateHandler(NXOpen::make_callback(this, &ZheWanBiRangCaoDialog::update_cb));
    dialog_->AddApplyHandler(NXOpen::make_callback(this, &ZheWanBiRangCaoDialog::apply_cb));
    dialog_->AddOkHandler(NXOpen::make_callback(this, &ZheWanBiRangCaoDialog::ok_cb));
    dialog_->AddCancelHandler(NXOpen::make_callback(this, &ZheWanBiRangCaoDialog::cancel_cb));
}

ZheWanBiRangCaoDialog::~ZheWanBiRangCaoDialog()
{
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
    slotModeBlock_ = dialog_->TopBlock()->FindBlock("slot_mode");
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
}

bool ZheWanBiRangCaoDialog::enable_ok_cb()
{
    return GetSelectedFace() != nullptr || !slotRecords_.empty();
}

int ZheWanBiRangCaoDialog::update_cb(NXOpen::BlockStyler::UIBlock* block)
{
    if (block == edgeSelectBlock_)
    {
        return Execute();
    }

    if (block == slotWidthBlock_ || block == slotDepthBlock_)
    {
        if (!slotRecords_.empty())
        {
            return UpdateAllSlots() ? 0 : 1;
        }
    }

    return 0;
}

int ZheWanBiRangCaoDialog::apply_cb()
{
    if (GetSelectedFace() != nullptr)
    {
        return Execute();
    }

    if (!slotRecords_.empty())
    {
        return UpdateAllSlots() ? 0 : 1;
    }

    return 0;
}

int ZheWanBiRangCaoDialog::ok_cb()
{
    return apply_cb();
}

int ZheWanBiRangCaoDialog::cancel_cb()
{
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

int ZheWanBiRangCaoDialog::GetSlotMode() const
{
    NXOpen::BlockStyler::PropertyList* properties = slotModeBlock_->GetProperties();
    const int value = properties->GetEnum("Value");
    delete properties;
    return value;
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

std::vector<ZheWanBiRangCaoDialog::SlotReferenceEdge> ZheWanBiRangCaoDialog::FindInnerReferenceEdges(NXOpen::Edge* selectedEdge, NXOpen::Face* face) const
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

        const double endpointError =
            std::min(Distance(alignedStart, selectedStart), Distance(alignedStart, selectedEnd)) +
            std::min(Distance(alignedEnd, selectedStart), Distance(alignedEnd, selectedEnd));
        const double score = spacingError * 100.0 + axialOffset * 10.0 + endpointError;
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

bool ZheWanBiRangCaoDialog::CreateBoundedSlot(NXOpen::Edge* edge,
                                              const NXOpen::Point3d& boundedPointPosition,
                                              const NXOpen::Vector3d& xDirection,
                                              const NXOpen::Vector3d& yDirection,
                                              const NXOpen::Vector3d& zDirection,
                                              double offsetPositiveX,
                                              double offsetNegativeX,
                                              double offsetPositiveY,
                                              double offsetNegativeY,
                                              double offsetPositiveZ,
                                              double offsetNegativeZ,
                                              NXOpen::Features::ToolingBox** createdToolingBox,
                                              bool useDefaultMatrix) const
{
    NXOpen::Part* workPart = session_->Parts()->Work();
    NXOpen::Body* targetBody = edge->GetBody();
    if (workPart == nullptr || targetBody == nullptr)
    {
        return false;
    }

    const NXOpen::Point3d boxOrigin = Add(
        boundedPointPosition,
        Add(
            Scale(xDirection, -offsetNegativeX),
            Scale(zDirection, -offsetNegativeZ)));

    session_->ToolingSession()->SetWizardType(1);

    NXOpen::Point* boundedPoint = nullptr;
    NXOpen::SelectionIntentRuleOptions* ruleOptions = nullptr;
    NXOpen::CurveDumbRule* pointRule = nullptr;
    NXOpen::Features::ToolingBoxBuilder* toolingBoxBuilder = nullptr;
    NXOpen::Features::BooleanBuilder* booleanBuilder = nullptr;

    try
    {
        boundedPoint = workPart->Points()->CreatePoint(boundedPointPosition);
        ruleOptions = workPart->ScRuleFactory()->CreateRuleOptions();
        ruleOptions->SetSelectedFromInactive(false);

        std::vector<NXOpen::Point*> points(1, boundedPoint);
        pointRule = workPart->ScRuleFactory()->CreateRuleCurveDumbFromPoints(points, ruleOptions);

        toolingBoxBuilder = workPart->Features()->ToolingFeatureCollection()->CreateToolingBoxBuilder(nullptr);
        toolingBoxBuilder->SetType(NXOpen::Features::ToolingBoxBuilder::TypesBoundedBlock);
        toolingBoxBuilder->SetSingleOffset(false);
        toolingBoxBuilder->Clearance()->SetFormula("0");
        toolingBoxBuilder->RadialOffset()->SetFormula("0");

        NXOpen::Matrix3x3 matrix;
        if (useDefaultMatrix)
        {
            NXOpen::Vector3d normalizedY = Normalize(yDirection);
            NXOpen::Vector3d normalizedZ = Normalize(zDirection);
            if (std::fabs(Dot(normalizedY, normalizedZ)) > 0.98)
            {
                normalizedZ = NXOpen::Vector3d(0.0, 0.0, 1.0);
                if (std::fabs(Dot(normalizedY, normalizedZ)) > 0.98)
                {
                    normalizedZ = NXOpen::Vector3d(1.0, 0.0, 0.0);
                }
            }

            NXOpen::Vector3d adjustedX = Cross(normalizedY, normalizedZ);
            if (Magnitude(adjustedX) < kVectorTolerance)
            {
                normalizedZ = NXOpen::Vector3d(0.0, 1.0, 0.0);
                adjustedX = Cross(normalizedY, normalizedZ);
            }
            adjustedX = Normalize(adjustedX);
            NXOpen::Vector3d adjustedZ = Normalize(Cross(adjustedX, normalizedY));

            matrix.Xx = adjustedX.X;
            matrix.Xy = adjustedX.Y;
            matrix.Xz = adjustedX.Z;
            matrix.Yx = normalizedY.X;
            matrix.Yy = normalizedY.Y;
            matrix.Yz = normalizedY.Z;
            matrix.Zx = adjustedZ.X;
            matrix.Zy = adjustedZ.Y;
            matrix.Zz = adjustedZ.Z;
        }
        else
        {
            matrix.Xx = xDirection.X;
            matrix.Xy = xDirection.Y;
            matrix.Xz = xDirection.Z;
            matrix.Yx = yDirection.X;
            matrix.Yy = yDirection.Y;
            matrix.Yz = yDirection.Z;
            matrix.Zx = zDirection.X;
            matrix.Zy = zDirection.Y;
            matrix.Zz = zDirection.Z;
        }
        toolingBoxBuilder->SetBoxMatrixAndPosition(matrix, boxOrigin);

        NXOpen::ScCollector* boundedCollector = toolingBoxBuilder->BoundedObject();
        std::vector<NXOpen::SelectionIntentRule*> rules(1, pointRule);
        boundedCollector->ReplaceRules(rules, false);
        toolingBoxBuilder->CalculateBoxSize();
        toolingBoxBuilder->SetBoxPosition(boxOrigin);

        toolingBoxBuilder->OffsetPositiveX()->SetFormula(std::to_string(offsetPositiveX).c_str());
        toolingBoxBuilder->OffsetNegativeX()->SetFormula(std::to_string(offsetNegativeX).c_str());
        toolingBoxBuilder->OffsetPositiveY()->SetFormula(std::to_string(offsetPositiveY).c_str());
        toolingBoxBuilder->OffsetNegativeY()->SetFormula(std::to_string(offsetNegativeY).c_str());
        toolingBoxBuilder->OffsetPositiveZ()->SetFormula(std::to_string(offsetPositiveZ).c_str());
        toolingBoxBuilder->OffsetNegativeZ()->SetFormula(std::to_string(offsetNegativeZ).c_str());

        NXOpen::NXObject* committedObject = toolingBoxBuilder->Commit();
        toolingBoxBuilder->Destroy();
        toolingBoxBuilder = nullptr;
        delete ruleOptions;
        ruleOptions = nullptr;

        NXOpen::Features::Feature* toolingFeature = dynamic_cast<NXOpen::Features::Feature*>(committedObject);
        if (toolingFeature == nullptr)
        {
            return false;
        }

        if (createdToolingBox != nullptr)
        {
            *createdToolingBox = dynamic_cast<NXOpen::Features::ToolingBox*>(committedObject);
        }

        tag_t toolingBodyTag = NULL_TAG;
        if (UF_MODL_ask_feat_body(toolingFeature->Tag(), &toolingBodyTag) != 0 || toolingBodyTag == NULL_TAG)
        {
            return false;
        }

        NXOpen::Body* toolBody = dynamic_cast<NXOpen::Body*>(NXOpen::NXObjectManager::Get(toolingBodyTag));
        if (toolBody == nullptr)
        {
            return false;
        }

        booleanBuilder = workPart->Features()->CreateBooleanBuilderUsingCollector(nullptr);
        booleanBuilder->SetOperation(NXOpen::Features::Feature::BooleanTypeSubtract);
        booleanBuilder->SetTarget(targetBody);
        booleanBuilder->SetTool(toolBody);
        booleanBuilder->Commit();
        booleanBuilder->Destroy();
        ForceModelUpdate();
        return true;
    }
    catch (...)
    {
        if (booleanBuilder != nullptr)
        {
            booleanBuilder->Destroy();
        }
        if (toolingBoxBuilder != nullptr)
        {
            toolingBoxBuilder->Destroy();
        }
        if (ruleOptions != nullptr)
        {
            delete ruleOptions;
        }
        return false;
    }
}

bool ZheWanBiRangCaoDialog::EditSlotFeature(SlotFeatureRecord& record)
{
    if (record.toolingBox == nullptr)
    {
        return false;
    }

    NXOpen::Part* workPart = session_->Parts()->Work();
    if (workPart == nullptr)
    {
        return false;
    }

    NXOpen::Features::ToolingBoxBuilder* toolingBoxBuilder = nullptr;
    try
    {
        toolingBoxBuilder =
            workPart->Features()->ToolingFeatureCollection()->CreateToolingBoxBuilder(record.toolingBox);

        NXOpen::Matrix3x3 matrix;
        matrix.Xx = record.xDirection.X;
        matrix.Xy = record.xDirection.Y;
        matrix.Xz = record.xDirection.Z;
        matrix.Yx = record.yDirection.X;
        matrix.Yy = record.yDirection.Y;
        matrix.Yz = record.yDirection.Z;
        matrix.Zx = record.zDirection.X;
        matrix.Zy = record.zDirection.Y;
        matrix.Zz = record.zDirection.Z;

        const NXOpen::Point3d boxOrigin = Add(
            record.boundedPoint,
            Add(
                Scale(record.xDirection, -record.offsetNegativeX),
                Scale(record.zDirection, -record.offsetNegativeZ)));

        toolingBoxBuilder->SetBoxMatrixAndPosition(matrix, boxOrigin);
        toolingBoxBuilder->SetBoxPosition(boxOrigin);
        toolingBoxBuilder->OffsetPositiveX()->SetFormula(std::to_string(record.offsetPositiveX).c_str());
        toolingBoxBuilder->OffsetNegativeX()->SetFormula(std::to_string(record.offsetNegativeX).c_str());
        toolingBoxBuilder->OffsetPositiveY()->SetFormula(std::to_string(record.offsetPositiveY).c_str());
        toolingBoxBuilder->OffsetNegativeY()->SetFormula(std::to_string(record.offsetNegativeY).c_str());
        toolingBoxBuilder->OffsetPositiveZ()->SetFormula(std::to_string(record.offsetPositiveZ).c_str());
        toolingBoxBuilder->OffsetNegativeZ()->SetFormula(std::to_string(record.offsetNegativeZ).c_str());
        toolingBoxBuilder->Commit();
        toolingBoxBuilder->Destroy();
        ForceModelUpdate();
        return true;
    }
    catch (...)
    {
        if (toolingBoxBuilder != nullptr)
        {
            toolingBoxBuilder->Destroy();
        }
        return false;
    }
}

bool ZheWanBiRangCaoDialog::UpdateAllSlots()
{
    bool updatedAny = false;
    for (SlotFeatureRecord& record : slotRecords_)
    {
        if (record.edge == nullptr)
        {
            continue;
        }

        if (record.offsetPositiveY > 0.0)
        {
            record.offsetPositiveY = GetSlotWidthY();
        }
        record.offsetNegativeZ = record.baseNegativeZ + GetSlotDepthZ();

        updatedAny = EditSlotFeature(record) || updatedAny;
    }

    return updatedAny;
}

bool ZheWanBiRangCaoDialog::CreateOrEditSlot(NXOpen::Edge* edge,
                                             const NXOpen::Point3d& boundedPoint,
                                             const NXOpen::Vector3d& xDirection,
                                             const NXOpen::Vector3d& yDirection,
                                             const NXOpen::Vector3d& zDirection,
                                             double offsetPositiveX,
                                             double offsetNegativeX,
                                             double offsetPositiveY,
                                             double offsetNegativeY,
                                             double offsetPositiveZ,
                                             double offsetNegativeZ,
                                             double baseNegativeZ,
                                             bool useDefaultMatrix)
{
    for (SlotFeatureRecord& record : slotRecords_)
    {
        if (record.edge == edge && Distance(record.boundedPoint, boundedPoint) <= 0.01)
        {
            record.xDirection = xDirection;
            record.yDirection = yDirection;
            record.zDirection = zDirection;
            record.offsetPositiveX = offsetPositiveX;
            record.offsetNegativeX = offsetNegativeX;
            record.offsetPositiveY = offsetPositiveY;
            record.offsetNegativeY = offsetNegativeY;
            record.offsetPositiveZ = offsetPositiveZ;
            record.offsetNegativeZ = offsetNegativeZ;
            record.baseNegativeZ = baseNegativeZ;
            return EditSlotFeature(record);
        }
    }

    NXOpen::Features::ToolingBox* toolingBox = nullptr;
    if (!CreateBoundedSlot(edge,
                           boundedPoint,
                           xDirection,
                           yDirection,
                           zDirection,
                           offsetPositiveX,
                           offsetNegativeX,
                           offsetPositiveY,
                           offsetNegativeY,
                           offsetPositiveZ,
                           offsetNegativeZ,
                           &toolingBox,
                           useDefaultMatrix))
    {
        return false;
    }

    if (toolingBox == nullptr)
    {
        return false;
    }

    slotRecords_.push_back(
        SlotFeatureRecord{
            toolingBox,
            edge,
            boundedPoint,
            xDirection,
            yDirection,
            zDirection,
            offsetPositiveX,
            offsetNegativeX,
            offsetPositiveY,
            offsetNegativeY,
            offsetPositiveZ,
            offsetNegativeZ,
            baseNegativeZ});
    return true;
}

bool ZheWanBiRangCaoDialog::CreateSlotAtEnd(NXOpen::Edge* edge,
                                            NXOpen::Face* largerFace,
                                            const NXOpen::Point3d& innerPoint,
                                            const NXOpen::Point3d& outerPoint,
                                            const NXOpen::Vector3d& yDirection,
                                            const SlotParameters& parameters)
{
    NXOpen::Part* workPart = session_->Parts()->Work();
    NXOpen::Body* targetBody = edge->GetBody();
    if (workPart == nullptr || targetBody == nullptr || largerFace == nullptr)
    {
        return false;
    }

    double faceBox[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    NXOpen::Vector3d xDirection = AskPlanarFaceNormal(largerFace, faceBox);
    NXOpen::Vector3d normalizedY = Normalize(yDirection);
    xDirection = Subtract(xDirection, Scale(normalizedY, Dot(xDirection, normalizedY)));
    if (Magnitude(xDirection) < kVectorTolerance)
    {
        return false;
    }
    xDirection = Normalize(xDirection);

    const double thickness = EstimateThickness(targetBody, largerFace);
    NXOpen::Vector3d zDirection = Cross(xDirection, normalizedY);
    if (Magnitude(zDirection) < kVectorTolerance)
    {
        return false;
    }
    zDirection = Normalize(zDirection);

    const double probeZDistance = std::max(thickness * 2.0, 0.5);
    int positiveStatus = AskPointContainmentStatus(targetBody, Add(innerPoint, Scale(zDirection, probeZDistance)));

    if (positiveStatus == 1 || positiveStatus == 3)
    {
        xDirection = Scale(xDirection, -1.0);
        zDirection = Cross(xDirection, normalizedY);
        if (Magnitude(zDirection) < kVectorTolerance)
        {
            return false;
        }
        zDirection = Normalize(zDirection);
        positiveStatus = AskPointContainmentStatus(targetBody, Add(innerPoint, Scale(zDirection, probeZDistance)));
        if (positiveStatus == 1 || positiveStatus == 3)
        {
            return false;
        }
    }

    return CreateOrEditSlot(edge,
                            innerPoint,
                            xDirection,
                            normalizedY,
                            zDirection,
                            thickness + parameters.xPositiveExtra,
                            thickness + parameters.xNegativeExtra,
                            parameters.slotWidth,
                            0.0,
                            parameters.zPositiveExtra,
                            parameters.slotDepth,
                            0.0);
}

int ZheWanBiRangCaoDialog::Execute()
{
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
        NXOpen::Point3d outerStart;
        NXOpen::Point3d outerEnd;
        edge->GetVertices(&outerStart, &outerEnd);
        const NXOpen::Vector3d outerEdgeDirection = Normalize(Subtract(outerEnd, outerStart));
        const double thickness = EstimateThickness(edge->GetBody(), largerFace);
        const int slotMode = GetSlotMode();

        if (slotMode == 1)
        {
            NXOpen::Point3d pointA;
            NXOpen::Point3d pointB;
            GetPickedEndPoints(edge, pickPoint, pointA, pointB);

            // Step 1: selected endpoint A is the bounded-block reference point.
            const NXOpen::Vector3d yDirection = Normalize(Subtract(pointB, pointA));
            NXOpen::Face* carrierFace = FindInnerSlotCarrierFace(edge, yDirection, thickness);
            if (carrierFace == nullptr)
            {
                ShowError("No inner slot carrier face was found from the selected edge.");
                return 1;
            }

            NXOpen::Vector3d faceNormal = AskPlanarFaceOuterNormal(carrierFace, pointA);
            NXOpen::Vector3d zDirection = Normalize(faceNormal);
            NXOpen::Vector3d xDirection = Cross(yDirection, zDirection);
            if (Magnitude(xDirection) < kVectorTolerance)
            {
                ShowError("Failed to determine the inner slot X direction.");
                return 1;
            }
            xDirection = Normalize(xDirection);

            // Step 2: feed the fixed offsets first, consistent with the manual bounded-block workflow.
            const double xPositive = thickness + 0.5;
            const double xNegative = thickness + 0.5;
            const double yPositive = GetSlotWidthY();
            const double yNegative = 0.0;
            const double zPositive = thickness + 0.5;

            // Step 3: the negative-Z offset must follow the edge chain along the specified -Z direction.
            const double baseDepth =
                ComputeDepthChainLengthFromPoint(edge->GetBody(), edge, pointA, Scale(zDirection, -1.0), thickness);
            if (baseDepth < kVectorTolerance)
            {
                ShowError("No valid inner slot depth chain was found.");
                return 1;
            }

            const double zNegative = baseDepth + GetSlotDepthZ();
            if (!CreateOrEditSlot(edge,
                                  pointA,
                                  xDirection,
                                  yDirection,
                                  zDirection,
                                  xPositive,
                                  xNegative,
                                  yPositive,
                                  yNegative,
                                  zPositive,
                                  zNegative,
                                  baseDepth,
                                  true))
            {
                ShowError("No relief slot was created.");
                return 1;
            }
            ClearSelectedEdge();
            return 0;
        }

        const std::vector<SlotReferenceEdge> referenceEdges = FindInnerReferenceEdges(edge, largerFace);

        int createdCount = 0;
        for (const SlotReferenceEdge& referenceEdge : referenceEdges)
        {
            const NXOpen::Vector3d yDirection = Scale(
                Normalize(Subtract(referenceEdge.endPoint, referenceEdge.startPoint)),
                -1.0);
            const NXOpen::Point3d matchedOuterStart = FindNearestOuterPoint(referenceEdge.startPoint, outerStart, outerEnd);
            const NXOpen::Point3d matchedOuterEnd = FindNearestOuterPoint(referenceEdge.endPoint, outerStart, outerEnd);

            if (ShouldCreateSlotAtEnd(referenceEdge.startPoint, matchedOuterStart, outerEdgeDirection, edge->GetBody(), thickness) &&
                CreateSlotAtEnd(edge, largerFace, referenceEdge.startPoint, matchedOuterStart, yDirection, parameters))
            {
                ++createdCount;
            }
            if (ShouldCreateSlotAtEnd(referenceEdge.endPoint, matchedOuterEnd, outerEdgeDirection, edge->GetBody(), thickness) &&
                CreateSlotAtEnd(edge, largerFace, referenceEdge.endPoint, matchedOuterEnd, Scale(yDirection, -1.0), parameters))
            {
                ++createdCount;
            }
        }

        if (createdCount == 0)
        {
            ShowError("No relief slot was created.");
            return 1;
        }

        ClearSelectedEdge();
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
