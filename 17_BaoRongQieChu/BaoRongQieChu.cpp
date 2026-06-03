#include "BaoRongQieChu.hpp"

#include <NXOpen/BlockStyler_PropertyList.hxx>
#include <NXOpen/Body.hxx>
#include <NXOpen/BodyDumbRule.hxx>
#include <NXOpen/Callback.hxx>
#include <NXOpen/DisplayManager.hxx>
#include <NXOpen/DisplayModification.hxx>
#include <NXOpen/DisplayableObject.hxx>
#include <NXOpen/Direction.hxx>
#include <NXOpen/DirectionCollection.hxx>
#include <NXOpen/Edge.hxx>
#include <NXOpen/Expression.hxx>
#include <NXOpen/ExpressionCollection.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/Features_BooleanBuilder.hxx>
#include <NXOpen/Features_BooleanFeature.hxx>
#include <NXOpen/Features_DeleteFaceBuilder.hxx>
#include <NXOpen/Features_ExtrudeBuilder.hxx>
#include <NXOpen/Features_Feature.hxx>
#include <NXOpen/Features_FeatureCollection.hxx>
#include <NXOpen/Features_RemoveParametersBuilder.hxx>
#include <NXOpen/Features_ResizeBlendBuilder.hxx>
#include <NXOpen/GeometricUtilities_BooleanOperation.hxx>
#include <NXOpen/GeometricUtilities_Extend.hxx>
#include <NXOpen/GeometricUtilities_FeatureOptions.hxx>
#include <NXOpen/GeometricUtilities_Limits.hxx>
#include <NXOpen/GeometricUtilities_SmartVolumeProfileBuilder.hxx>
#include <NXOpen/FaceDumbRule.hxx>
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
#include <NXOpen/Selection.hxx>
#include <NXOpen/SelectionIntentRule.hxx>
#include <NXOpen/SelectionIntentRuleOptions.hxx>
#include <NXOpen/Section.hxx>
#include <NXOpen/SectionCollection.hxx>
#include <NXOpen/SelectNXObjectList.hxx>
#include <NXOpen/SelectObjectList.hxx>
#include <NXOpen/SmartObject.hxx>
#include <NXOpen/Unit.hxx>

#include <Windows.h>
#ifdef CreateDialog
#undef CreateDialog
#endif

#include <uf_modl.h>
#include <uf_modl_primitives.h>
#include <uf_modl_curves.h>
#include <uf_modl_utilities.h>
#include <uf_disp.h>
#include <uf_obj.h>
#include <uf_object_types.h>
#include <uf_ui_types.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include <vector>

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace
{
const double kMinimumBlockSize = 0.001;
const double kHoleRestoreOverrun = 1.0;
const double kHoleProfileKeyTolerance = 0.01;
const double kRedColor[3] = {1.0, 0.0, 0.0};
const double kBlueColor[3] = {0.0, 0.2, 1.0};

std::string GetDialogFilePath()
{
    char buffer[MAX_PATH] = {};
    const HMODULE module = reinterpret_cast<HMODULE>(&__ImageBase);
    const DWORD length = GetModuleFileNameA(module, buffer, MAX_PATH);
    if (length > 0 && length < MAX_PATH)
    {
        const std::filesystem::path modulePath(buffer);
        const std::filesystem::path deployedPath = modulePath.parent_path() / "BaoRongQieChu.dlx";
        if (std::filesystem::exists(deployedPath))
        {
            return deployedPath.string();
        }

        const std::filesystem::path uiPath = modulePath.parent_path() / "ui" / "BaoRongQieChu.dlx";
        if (std::filesystem::exists(uiPath))
        {
            return uiPath.string();
        }
    }

    return "D:\\UGPluginRepo\\BaoRongQieChu\\BaoRongQieChu.dlx";
}

std::string FormatDouble(double value)
{
    std::ostringstream stream;
    stream.setf(std::ios::fixed);
    stream.precision(6);
    stream << value;
    return stream.str();
}

bool IsSolidBody(tag_t bodyTag)
{
    if (bodyTag == NULL_TAG)
    {
        return false;
    }

    int bodyType = -1;
    return UF_MODL_ask_body_type(bodyTag, &bodyType) == 0 && bodyType == UF_MODL_SOLID_BODY;
}

tag_t ResolveOwningBody(tag_t objectTag)
{
    if (objectTag == NULL_TAG)
    {
        return NULL_TAG;
    }

    int type = 0;
    int subtype = 0;
    if (UF_OBJ_ask_type_and_subtype(objectTag, &type, &subtype) != 0 || type != UF_solid_type)
    {
        return NULL_TAG;
    }

    if (subtype == UF_solid_body_subtype)
    {
        return IsSolidBody(objectTag) ? objectTag : NULL_TAG;
    }

    if (subtype == UF_solid_face_subtype)
    {
        tag_t bodyTag = NULL_TAG;
        if (UF_MODL_ask_face_body(objectTag, &bodyTag) == 0 && IsSolidBody(bodyTag))
        {
            return bodyTag;
        }
        return NULL_TAG;
    }

    if (subtype == UF_solid_edge_subtype)
    {
        tag_t bodyTag = NULL_TAG;
        if (UF_MODL_ask_edge_body(objectTag, &bodyTag) == 0 && IsSolidBody(bodyTag))
        {
            return bodyTag;
        }
        return NULL_TAG;
    }

    return NULL_TAG;
}

std::array<double, 6> AskBoundingBox(tag_t objectTag)
{
    std::array<double, 6> box = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    if (UF_MODL_ask_bounding_box(objectTag, box.data()) != 0)
    {
        throw std::runtime_error("Failed to read bounding box of selected object.");
    }
    return box;
}

NXOpen::Features::Feature* CreateEnvelopeBlock(
    NXOpen::Part* workPart,
    const std::array<double, 3>& minCorner,
    const std::array<double, 3>& maxCorner,
    double envelopeOffset)
{
    if (workPart == nullptr)
    {
        throw std::runtime_error("No active work part.");
    }

    double corner[3] =
    {
        minCorner[0] - envelopeOffset,
        minCorner[1] - envelopeOffset,
        minCorner[2] - envelopeOffset
    };

    const std::array<double, 3> edgeLength =
    {
        std::max(maxCorner[0] - minCorner[0] + 2.0 * envelopeOffset, kMinimumBlockSize),
        std::max(maxCorner[1] - minCorner[1] + 2.0 * envelopeOffset, kMinimumBlockSize),
        std::max(maxCorner[2] - minCorner[2] + 2.0 * envelopeOffset, kMinimumBlockSize)
    };

    std::string edgeLengthX = FormatDouble(edgeLength[0]);
    std::string edgeLengthY = FormatDouble(edgeLength[1]);
    std::string edgeLengthZ = FormatDouble(edgeLength[2]);
    char* edgeLengthText[3] =
    {
        const_cast<char*>(edgeLengthX.c_str()),
        const_cast<char*>(edgeLengthY.c_str()),
        const_cast<char*>(edgeLengthZ.c_str())
    };

    tag_t featureTag = NULL_TAG;
    if (UF_MODL_create_block1(UF_NULLSIGN, corner, edgeLengthText, &featureTag) != 0 || featureTag == NULL_TAG)
    {
        throw std::runtime_error("Failed to create envelope block.");
    }

    NXOpen::Features::Feature* feature =
        dynamic_cast<NXOpen::Features::Feature*>(NXOpen::NXObjectManager::Get(featureTag));
    if (feature == nullptr)
    {
        throw std::runtime_error("Failed to resolve envelope feature.");
    }

    return feature;
}

NXOpen::Body* AskFeatureBody(NXOpen::Features::Feature* feature)
{
    if (feature == nullptr)
    {
        return nullptr;
    }

    const std::vector<NXOpen::Body*> bodies = feature->GetBodies();
    return bodies.empty() ? nullptr : bodies.front();
}

void DeleteObjectIfAlive(tag_t objectTag)
{
    if (objectTag == NULL_TAG)
    {
        return;
    }

    int type = 0;
    int subtype = 0;
    if (UF_OBJ_ask_type_and_subtype(objectTag, &type, &subtype) != 0)
    {
        return;
    }

    static_cast<void>(UF_OBJ_delete_object(objectTag));
}

void BlankObjectIfAlive(tag_t objectTag)
{
    if (objectTag == NULL_TAG)
    {
        return;
    }

    int type = 0;
    int subtype = 0;
    if (UF_OBJ_ask_type_and_subtype(objectTag, &type, &subtype) != 0)
    {
        return;
    }

    static_cast<void>(UF_OBJ_set_blank_status(objectTag, UF_OBJ_BLANKED));
}

struct PlanarFaceData
{
    tag_t faceTag;
    tag_t bodyTag;
    NXOpen::Point3d origin;
    NXOpen::Vector3d normal;
};

struct HoleDepthDirection
{
    double depth;
    NXOpen::Vector3d direction;
};

double DotVector(const NXOpen::Vector3d& a, const NXOpen::Vector3d& b)
{
    return a.X * b.X + a.Y * b.Y + a.Z * b.Z;
}

double VectorLength(const NXOpen::Vector3d& vector)
{
    return std::sqrt(DotVector(vector, vector));
}

NXOpen::Vector3d NormalizeVector(const NXOpen::Vector3d& vector)
{
    const double length = VectorLength(vector);
    if (length <= kMinimumBlockSize)
    {
        return NXOpen::Vector3d(0.0, 0.0, 1.0);
    }

    return NXOpen::Vector3d(vector.X / length, vector.Y / length, vector.Z / length);
}

NXOpen::Vector3d VectorBetween(const NXOpen::Point3d& from, const NXOpen::Point3d& to)
{
    return NXOpen::Vector3d(to.X - from.X, to.Y - from.Y, to.Z - from.Z);
}

bool AskPlanarFaceData(tag_t faceTag, PlanarFaceData& data)
{
    int faceType = 0;
    double point[3] = {0.0, 0.0, 0.0};
    double dir[3] = {0.0, 0.0, 1.0};
    double box[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    double radius = 0.0;
    double radData = 0.0;
    int normDir = 1;
    if (UF_MODL_ask_face_data(faceTag, &faceType, point, dir, box, &radius, &radData, &normDir) != 0 ||
        faceType != 22)
    {
        return false;
    }

    tag_t bodyTag = NULL_TAG;
    if (UF_MODL_ask_face_body(faceTag, &bodyTag) != 0 || bodyTag == NULL_TAG)
    {
        return false;
    }

    data.faceTag = faceTag;
    data.bodyTag = bodyTag;
    data.origin = NXOpen::Point3d(point[0], point[1], point[2]);
    data.normal = NXOpen::Vector3d(
        dir[0] * static_cast<double>(normDir),
        dir[1] * static_cast<double>(normDir),
        dir[2] * static_cast<double>(normDir));
    return true;
}

bool IsPlanarFace(tag_t faceTag)
{
    PlanarFaceData ignored;
    return AskPlanarFaceData(faceTag, ignored);
}

HoleDepthDirection EstimateHoleDepthDirectionFromPlanes(
    const std::vector<PlanarFaceData>& planes,
    const PlanarFaceData& profilePlane)
{
    const NXOpen::Vector3d profileNormal = NormalizeVector(profilePlane.normal);
    double bestDepth = std::numeric_limits<double>::max();
    double bestSignedDistance = 0.0;

    for (const PlanarFaceData& plane : planes)
    {
        if (plane.faceTag == profilePlane.faceTag)
        {
            continue;
        }

        const NXOpen::Vector3d candidateNormal = NormalizeVector(plane.normal);
        if (std::fabs(DotVector(profileNormal, candidateNormal)) < 0.99)
        {
            continue;
        }

        const double signedDistance = DotVector(VectorBetween(profilePlane.origin, plane.origin), profileNormal);
        const double distance = std::fabs(signedDistance);
        if (distance > kMinimumBlockSize && distance < bestDepth)
        {
            bestDepth = distance;
            bestSignedDistance = signedDistance;
        }
    }

    HoleDepthDirection result;
    result.depth = bestDepth == std::numeric_limits<double>::max() ? kMinimumBlockSize : bestDepth;
    result.direction = bestSignedDistance < 0.0
        ? NXOpen::Vector3d(-profileNormal.X, -profileNormal.Y, -profileNormal.Z)
        : profileNormal;
    return result;
}

bool FaceHasHoleLoop(tag_t faceTag)
{
    uf_loop_p_t loopList = nullptr;
    if (UF_MODL_ask_face_loops(faceTag, &loopList) != 0 || loopList == nullptr)
    {
        return false;
    }

    bool hasHoleLoop = false;
    for (uf_loop_p_t loop = loopList; loop != nullptr; loop = loop->next)
    {
        if (loop->type == 2)
        {
            hasHoleLoop = true;
            break;
        }
    }

    UF_MODL_delete_loop_list(&loopList);
    return hasHoleLoop;
}

std::vector<tag_t> AskFaceEdges(tag_t faceTag)
{
    std::vector<tag_t> edges;
    uf_list_p_t edgeList = nullptr;
    if (UF_MODL_ask_face_edges(faceTag, &edgeList) != 0 || edgeList == nullptr)
    {
        return edges;
    }

    for (uf_list_p_t node = edgeList; node != nullptr; node = node->next)
    {
        if (node->eid != NULL_TAG)
        {
            edges.push_back(node->eid);
        }
    }

    UF_MODL_delete_list(&edgeList);
    return edges;
}

std::vector<tag_t> AskEdgeFaces(tag_t edgeTag)
{
    std::vector<tag_t> faces;
    uf_list_p_t faceList = nullptr;
    if (UF_MODL_ask_edge_faces(edgeTag, &faceList) != 0 || faceList == nullptr)
    {
        return faces;
    }

    for (uf_list_p_t node = faceList; node != nullptr; node = node->next)
    {
        if (node->eid != NULL_TAG)
        {
            faces.push_back(node->eid);
        }
    }

    UF_MODL_delete_list(&faceList);
    return faces;
}

std::unordered_set<tag_t> ExpandHoleRingEdgesFromSeedEdge(tag_t seedEdge)
{
    std::unordered_set<tag_t> ringEdges;
    if (seedEdge == NULL_TAG)
    {
        return ringEdges;
    }

    ringEdges.insert(seedEdge);
    const std::vector<tag_t> adjacentFaces = AskEdgeFaces(seedEdge);
    for (tag_t faceTag : adjacentFaces)
    {
        if (faceTag == NULL_TAG || IsPlanarFace(faceTag))
        {
            continue;
        }

        const std::vector<tag_t> faceEdges = AskFaceEdges(faceTag);
        ringEdges.insert(faceEdges.begin(), faceEdges.end());
    }

    return ringEdges;
}

std::string MakeLoopKey(tag_t faceTag, uf_loop_p_t loop)
{
    std::vector<tag_t> edgeTags;
    for (uf_list_p_t edgeNode = loop != nullptr ? loop->edge_list : nullptr;
         edgeNode != nullptr;
         edgeNode = edgeNode->next)
    {
        if (edgeNode->eid != NULL_TAG)
        {
            edgeTags.push_back(edgeNode->eid);
        }
    }

    std::sort(edgeTags.begin(), edgeTags.end());
    std::ostringstream key;
    key << faceTag;
    for (tag_t edgeTag : edgeTags)
    {
        key << ':' << edgeTag;
    }
    return key.str();
}

long long QuantizeProfileKeyValue(double value)
{
    return static_cast<long long>(std::llround(value / kHoleProfileKeyTolerance));
}

NXOpen::Vector3d CanonicalNormalForKey(const NXOpen::Vector3d& normal)
{
    NXOpen::Vector3d result = NormalizeVector(normal);
    const double components[3] = {result.X, result.Y, result.Z};
    for (double component : components)
    {
        if (std::fabs(component) <= 1.0e-6)
        {
            continue;
        }

        if (component < 0.0)
        {
            result = NXOpen::Vector3d(-result.X, -result.Y, -result.Z);
        }
        break;
    }
    return result;
}

struct LoopKeyGeometry
{
    std::size_t edgeCount;
    NXOpen::Vector3d canonicalNormal;
    double axial;
    double projectedX;
    double projectedY;
    double projectedZ;
};

LoopKeyGeometry AskLoopKeyGeometry(const NXOpen::Vector3d& normal, uf_loop_p_t loop)
{
    std::array<double, 6> box =
    {
        std::numeric_limits<double>::max(),
        std::numeric_limits<double>::max(),
        std::numeric_limits<double>::max(),
        -std::numeric_limits<double>::max(),
        -std::numeric_limits<double>::max(),
        -std::numeric_limits<double>::max()
    };
    bool hasBox = false;
    std::size_t edgeCount = 0;

    for (uf_list_p_t edgeNode = loop != nullptr ? loop->edge_list : nullptr;
         edgeNode != nullptr;
         edgeNode = edgeNode->next)
    {
        if (edgeNode->eid == NULL_TAG)
        {
            continue;
        }

        double edgeBox[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
        if (UF_MODL_ask_bounding_box(edgeNode->eid, edgeBox) != 0)
        {
            continue;
        }

        hasBox = true;
        ++edgeCount;
        for (int index = 0; index < 3; ++index)
        {
            box[index] = std::min(box[index], edgeBox[index]);
            box[index + 3] = std::max(box[index + 3], edgeBox[index + 3]);
        }
    }

    const NXOpen::Vector3d canonicalNormal = CanonicalNormalForKey(normal);
    NXOpen::Point3d center(0.0, 0.0, 0.0);
    if (hasBox)
    {
        center = NXOpen::Point3d(
            (box[0] + box[3]) * 0.5,
            (box[1] + box[4]) * 0.5,
            (box[2] + box[5]) * 0.5);
    }

    const double axial =
        center.X * canonicalNormal.X +
        center.Y * canonicalNormal.Y +
        center.Z * canonicalNormal.Z;
    const double projectedX = center.X - axial * canonicalNormal.X;
    const double projectedY = center.Y - axial * canonicalNormal.Y;
    const double projectedZ = center.Z - axial * canonicalNormal.Z;

    LoopKeyGeometry geometry;
    geometry.edgeCount = edgeCount;
    geometry.canonicalNormal = canonicalNormal;
    geometry.axial = axial;
    geometry.projectedX = projectedX;
    geometry.projectedY = projectedY;
    geometry.projectedZ = projectedZ;
    return geometry;
}

std::string MakeLoopSpatialKey(
    tag_t bodyTag,
    const LoopKeyGeometry& geometry,
    bool includeProfilePlanePosition)
{
    std::ostringstream key;
    key << bodyTag
        << ':' << geometry.edgeCount
        << ':' << QuantizeProfileKeyValue(geometry.canonicalNormal.X)
        << ':' << QuantizeProfileKeyValue(geometry.canonicalNormal.Y)
        << ':' << QuantizeProfileKeyValue(geometry.canonicalNormal.Z)
        << ':' << QuantizeProfileKeyValue(geometry.projectedX)
        << ':' << QuantizeProfileKeyValue(geometry.projectedY)
        << ':' << QuantizeProfileKeyValue(geometry.projectedZ);
    if (includeProfilePlanePosition)
    {
        key << ':' << QuantizeProfileKeyValue(geometry.axial);
    }
    return key.str();
}

std::string MakeProjectedLoopKey(tag_t bodyTag, const NXOpen::Vector3d& normal, uf_loop_p_t loop)
{
    return MakeLoopSpatialKey(bodyTag, AskLoopKeyGeometry(normal, loop), false);
}

std::string MakeProfilePlaneLoopKey(tag_t bodyTag, const NXOpen::Vector3d& normal, uf_loop_p_t loop)
{
    return MakeLoopSpatialKey(bodyTag, AskLoopKeyGeometry(normal, loop), true);
}

bool LoopUsesAnyEdge(uf_loop_p_t loop, const std::unordered_set<tag_t>& selectedRingEdges)
{
    for (uf_list_p_t edgeNode = loop != nullptr ? loop->edge_list : nullptr;
         edgeNode != nullptr;
         edgeNode = edgeNode->next)
    {
        if (selectedRingEdges.find(edgeNode->eid) != selectedRingEdges.end())
        {
            return true;
        }
    }

    return false;
}

int AskClosestColorIndex(const double rgb[3])
{
    int colorIndex = 0;
    if (UF_DISP_ask_closest_color(
            UF_DISP_rgb_model,
            const_cast<double*>(rgb),
            UF_DISP_CCM_EUCLIDEAN_DISTANCE,
            &colorIndex) != 0)
    {
        return 0;
    }
    return colorIndex;
}

void ColorFaces(
    NXOpen::Session* session,
    const std::vector<NXOpen::Face*>& faces,
    const double rgb[3])
{
    if (session == nullptr || faces.empty())
    {
        return;
    }

    const int colorIndex = AskClosestColorIndex(rgb);
    if (colorIndex <= 0)
    {
        return;
    }

    std::vector<NXOpen::DisplayableObject*> displayableFaces;
    displayableFaces.reserve(faces.size());
    for (NXOpen::Face* face : faces)
    {
        if (face != nullptr)
        {
            displayableFaces.push_back(face);
        }
    }

    if (displayableFaces.empty())
    {
        return;
    }

    NXOpen::DisplayModification* modification = session->DisplayManager()->NewDisplayModification();
    modification->SetApplyToAllFaces(false);
    modification->SetApplyToOwningParts(false);
    modification->SetNewColor(colorIndex);
    modification->Apply(displayableFaces);
    delete modification;
}

std::vector<NXOpen::Face*> CollectFacesFromBody(NXOpen::Body* body)
{
    std::vector<NXOpen::Face*> faces;
    if (body == nullptr)
    {
        return faces;
    }

    std::unordered_set<tag_t> seenFaceTags;
    for (NXOpen::Face* face : body->GetFaces())
    {
        if (face != nullptr && seenFaceTags.insert(face->Tag()).second)
        {
            faces.push_back(face);
        }
    }

    return faces;
}

NXOpen::Body* ResolveTargetBodyFromSelection(const std::vector<NXOpen::TaggedObject*>& selectedObjects)
{
    tag_t ownerBodyTag = NULL_TAG;

    for (NXOpen::TaggedObject* object : selectedObjects)
    {
        if (object == nullptr)
        {
            continue;
        }

        const tag_t currentBody = ResolveOwningBody(object->Tag());
        if (currentBody == NULL_TAG)
        {
            throw std::runtime_error("Only solid bodies, faces, and edges are supported.");
        }

        if (ownerBodyTag == NULL_TAG)
        {
            ownerBodyTag = currentBody;
        }
        else if (ownerBodyTag != currentBody)
        {
            throw std::runtime_error("Please select objects from the same owning body in one run.");
        }
    }

    if (ownerBodyTag == NULL_TAG)
    {
        throw std::runtime_error("No owning solid body was found for subtract.");
    }

    NXOpen::Body* targetBody = dynamic_cast<NXOpen::Body*>(NXOpen::NXObjectManager::Get(ownerBodyTag));
    if (targetBody == nullptr)
    {
        throw std::runtime_error("Failed to get the owning body.");
    }

    return targetBody;
}

std::vector<NXOpen::Face*> FindBlendFacesByRadius(
    NXOpen::Part* workPart,
    const std::vector<NXOpen::Face*>& candidateFaces,
    double maxRadius)
{
    if (workPart == nullptr || candidateFaces.empty())
    {
        return {};
    }
    if (maxRadius < 0.0)
    {
        throw std::runtime_error("R value must be greater than or equal to 0.");
    }

    NXOpen::Features::ResizeBlendBuilder* resizeBuilder = workPart->Features()->CreateResizeBlendBuilder(nullptr);
    std::vector<bool> isBlendFace;
    resizeBuilder->IsBlendFace(candidateFaces, isBlendFace);

    std::vector<NXOpen::Face*> blendFaces;
    for (size_t index = 0; index < candidateFaces.size() && index < isBlendFace.size(); ++index)
    {
        if (!isBlendFace[index] || candidateFaces[index] == nullptr)
        {
            continue;
        }

        const double faceRadius = std::abs(resizeBuilder->GetBlendFaceRadius(candidateFaces[index]));
        if (faceRadius <= maxRadius + 1.0e-6)
        {
            blendFaces.push_back(candidateFaces[index]);
        }
    }

    resizeBuilder->Destroy();
    return blendFaces;
}

bool TryDeleteBlendFaces(
    NXOpen::Part* workPart,
    const std::vector<NXOpen::Face*>& candidateFaces,
    double maxRadius)
{
    if (workPart == nullptr || candidateFaces.empty())
    {
        return true;
    }

    NXOpen::Features::DeleteFaceBuilder* deleteFaceBuilder = workPart->Features()->CreateDeleteFaceBuilder(nullptr);
    deleteFaceBuilder->FaceRecognized()->SetRelationScope(1023);

    NXOpen::Point3d origin(0.0, 0.0, 0.0);
    NXOpen::Vector3d normal(0.0, 0.0, 1.0);
    NXOpen::Plane* capPlane =
        workPart->Planes()->CreatePlane(origin, normal, NXOpen::SmartObject::UpdateOptionWithinModeling);
    deleteFaceBuilder->SetCapPlane(capPlane);

    NXOpen::Unit* unit = deleteFaceBuilder->MaxHoleDiameter()->Units();
    NXOpen::Expression* expr1 = workPart->Expressions()->CreateSystemExpressionWithUnits("0", unit);
    NXOpen::Expression* expr2 = workPart->Expressions()->CreateSystemExpressionWithUnits("0", unit);

    deleteFaceBuilder->SetHeal(true);
    deleteFaceBuilder->SetDeletePartialBlend(false);
    deleteFaceBuilder->SetFaceEdgeBlendPreference(
        NXOpen::Features::DeleteFaceBuilder::FaceEdgeBlendPreferenceOptionsCliff);
    deleteFaceBuilder->MaxHoleDiameter()->SetFormula("5");
    deleteFaceBuilder->MaxBlendRadius()->SetFormula(FormatDouble(maxRadius).c_str());
    deleteFaceBuilder->SetCapPlane(nullptr);

    // Follow the journaled interactive workflow as closely as possible.
    deleteFaceBuilder->FaceRecognized()->SetCoplanarEnabled(false);
    deleteFaceBuilder->FaceRecognized()->SetCoplanarAxesEnabled(false);
    deleteFaceBuilder->FaceRecognized()->SetCoaxialEnabled(false);
    deleteFaceBuilder->FaceRecognized()->SetSameOrbitEnabled(false);
    deleteFaceBuilder->FaceRecognized()->SetEqualDiameterEnabled(false);
    deleteFaceBuilder->FaceRecognized()->SetTangentEnabled(false);
    deleteFaceBuilder->FaceRecognized()->SetSymmetricEnabled(false);
    deleteFaceBuilder->FaceRecognized()->SetOffsetEnabled(false);
    deleteFaceBuilder->FaceRecognized()->SetRigidBodyFaceEnabled(false);
    deleteFaceBuilder->FaceRecognized()->SetCloneScope(511);
    deleteFaceBuilder->FaceRecognized()->SetUseFindClone(true);
    deleteFaceBuilder->FaceRecognized()->SetUseFindRelated(false);
    deleteFaceBuilder->FaceRecognized()->SetUseFaceBrowse(true);
    deleteFaceBuilder->FaceRecognized()->SetRelationScope(0);
    deleteFaceBuilder->FaceRecognized()->SetCloneScope(511);
    deleteFaceBuilder->SetType(NXOpen::Features::DeleteFaceBuilder::SelectTypesFaceEdgeBlend);

    std::vector<NXOpen::SelectionIntentRule*> emptyRules;
    deleteFaceBuilder->FaceRecognized()->FaceCollector()->ReplaceRules(emptyRules, false);

    NXOpen::SelectionIntentRuleOptions* ruleOptions = workPart->ScRuleFactory()->CreateRuleOptions();
    ruleOptions->SetSelectedFromInactive(false);

    NXOpen::ScCollector* blendCollector = deleteFaceBuilder->BlendCollector();
    NXOpen::FaceDumbRule* blendRule =
        workPart->ScRuleFactory()->CreateRuleFaceDumb(candidateFaces, ruleOptions);
    std::vector<NXOpen::SelectionIntentRule*> blendRules(1, blendRule);
    blendCollector->ReplaceRules(blendRules, false);
    delete ruleOptions;
    deleteFaceBuilder->SetType(NXOpen::Features::DeleteFaceBuilder::SelectTypesBlend);

    try
    {
        deleteFaceBuilder->Commit();
    }
    catch (...)
    {
        try
        {
            workPart->Expressions()->Delete(expr2);
        }
        catch (...)
        {
        }

        try
        {
            workPart->Expressions()->Delete(expr1);
        }
        catch (...)
        {
        }

        capPlane->DestroyPlane();
        deleteFaceBuilder->Destroy();
        return false;
    }

    try
    {
        workPart->Expressions()->Delete(expr2);
    }
    catch (...)
    {
    }

    try
    {
        workPart->Expressions()->Delete(expr1);
    }
    catch (...)
    {
    }

    capPlane->DestroyPlane();
    deleteFaceBuilder->Destroy();
    return true;
}

void DeleteBlendFaces(
    NXOpen::Part* workPart,
    const std::vector<NXOpen::Face*>& candidateFaces,
    double maxRadius)
{
    if (workPart == nullptr || candidateFaces.empty())
    {
        return;
    }

    for (NXOpen::Face* blendFace : candidateFaces)
    {
        if (blendFace == nullptr)
        {
            continue;
        }

        std::vector<NXOpen::Face*> singleFace(1, blendFace);
        static_cast<void>(TryDeleteBlendFaces(workPart, singleFace, maxRadius));
        return;
    }
}

void DeleteFacesWithHeal(
    NXOpen::Part* workPart,
    const std::vector<NXOpen::Face*>& faces)
{
    if (workPart == nullptr || faces.empty())
    {
        return;
    }

    NXOpen::Features::DeleteFaceBuilder* deleteFaceBuilder = workPart->Features()->CreateDeleteFaceBuilder(nullptr);
    deleteFaceBuilder->SetType(NXOpen::Features::DeleteFaceBuilder::SelectTypesFace);
    deleteFaceBuilder->SetHeal(true);

    NXOpen::SelectionIntentRuleOptions* ruleOptions = workPart->ScRuleFactory()->CreateRuleOptions();
    ruleOptions->SetSelectedFromInactive(false);

    NXOpen::ScCollector* faceCollector = deleteFaceBuilder->FaceCollector();
    NXOpen::FaceDumbRule* faceRule =
        workPart->ScRuleFactory()->CreateRuleFaceDumb(faces, ruleOptions);
    std::vector<NXOpen::SelectionIntentRule*> faceRules(1, faceRule);
    faceCollector->ReplaceRules(faceRules, false);
    delete ruleOptions;

    try
    {
        deleteFaceBuilder->CommitFeature();
    }
    catch (...)
    {
        deleteFaceBuilder->Destroy();
        return;
    }

    deleteFaceBuilder->Destroy();
}

NXOpen::Features::BooleanFeature* SubtractToolBody(
    NXOpen::Part* workPart,
    NXOpen::Body* targetBody,
    NXOpen::Body* toolBody)
{
    if (workPart == nullptr || targetBody == nullptr || toolBody == nullptr)
    {
        throw std::runtime_error("Boolean subtract input bodies are incomplete.");
    }

    NXOpen::Features::BooleanFeature* seedFeature = nullptr;
    NXOpen::Features::BooleanBuilder* booleanBuilder =
        workPart->Features()->CreateBooleanBuilderUsingCollector(seedFeature);
    booleanBuilder->SetOperation(NXOpen::Features::Feature::BooleanTypeSubtract);
    booleanBuilder->SetCopyTargets(false);
    booleanBuilder->SetCopyTools(false);

    NXOpen::SelectionIntentRuleOptions* ruleOptions = workPart->ScRuleFactory()->CreateRuleOptions();
    ruleOptions->SetSelectedFromInactive(false);

    NXOpen::ScCollector* targetCollector = workPart->ScCollectors()->CreateCollector();
    std::vector<NXOpen::Body*> targetBodies(1, targetBody);
    NXOpen::BodyDumbRule* targetRule =
        workPart->ScRuleFactory()->CreateRuleBodyDumb(targetBodies, true, ruleOptions);
    std::vector<NXOpen::SelectionIntentRule*> targetRules(1, targetRule);
    targetCollector->ReplaceRules(targetRules, false);
    booleanBuilder->SetTargetBodyCollector(targetCollector);

    NXOpen::ScCollector* toolCollector = workPart->ScCollectors()->CreateCollector();
    std::vector<NXOpen::Body*> toolBodies(1, toolBody);
    NXOpen::BodyDumbRule* toolRule =
        workPart->ScRuleFactory()->CreateRuleBodyDumb(toolBodies, true, ruleOptions);
    std::vector<NXOpen::SelectionIntentRule*> toolRules(1, toolRule);
    toolCollector->ReplaceRules(toolRules, false);
    booleanBuilder->SetToolBodyCollector(toolCollector);

    delete ruleOptions;

    try
    {
        NXOpen::Features::Feature* committedFeature = booleanBuilder->CommitFeature();
        NXOpen::Features::BooleanFeature* booleanFeature =
            dynamic_cast<NXOpen::Features::BooleanFeature*>(committedFeature);
        booleanBuilder->Destroy();
        return booleanFeature;
    }
    catch (...)
    {
        booleanBuilder->Destroy();
        throw std::runtime_error("Boolean subtract failed. Check whether the selected region is valid.");
    }
}

void RemoveParametersFromBody(NXOpen::Part* workPart, NXOpen::Body* body)
{
    if (workPart == nullptr || body == nullptr)
    {
        return;
    }

    NXOpen::Features::RemoveParametersBuilder* removeParametersBuilder =
        workPart->Features()->CreateRemoveParametersBuilder();
    try
    {
        removeParametersBuilder->Objects()->Add(body);
        static_cast<void>(removeParametersBuilder->Commit());
        removeParametersBuilder->Destroy();
    }
    catch (...)
    {
        removeParametersBuilder->Destroy();
        throw;
    }
}

void ExecuteEnvelopeCut(
    NXOpen::Part* workPart,
    const std::vector<NXOpen::TaggedObject*>& selectedObjects,
    bool enableBooleanSubtract,
    double envelopeOffset,
    bool removeBlend,
    double blendRadius,
    bool healRemovedRegion,
    NXOpen::Features::BooleanFeature** outBooleanFeature = nullptr)
{
    if (workPart == nullptr || selectedObjects.empty())
    {
        return;
    }
    if (envelopeOffset < 0.0)
    {
        throw std::runtime_error("Offset value must be greater than or equal to 0.");
    }

    tag_t ownerBodyTag = NULL_TAG;
    std::array<double, 3> minCorner =
    {
        std::numeric_limits<double>::max(),
        std::numeric_limits<double>::max(),
        std::numeric_limits<double>::max()
    };
    std::array<double, 3> maxCorner =
    {
        -std::numeric_limits<double>::max(),
        -std::numeric_limits<double>::max(),
        -std::numeric_limits<double>::max()
    };

    for (NXOpen::TaggedObject* object : selectedObjects)
    {
        if (object == nullptr)
        {
            continue;
        }

        const tag_t currentBody = ResolveOwningBody(object->Tag());
        if (currentBody == NULL_TAG)
        {
            throw std::runtime_error("Only solid bodies, faces, and edges are supported.");
        }

        if (ownerBodyTag == NULL_TAG)
        {
            ownerBodyTag = currentBody;
        }
        else if (ownerBodyTag != currentBody)
        {
            throw std::runtime_error("Please select objects from the same owning body in one run.");
        }

        const std::array<double, 6> box = AskBoundingBox(object->Tag());
        for (int axis = 0; axis < 3; ++axis)
        {
            minCorner[axis] = std::min(minCorner[axis], box[axis]);
            maxCorner[axis] = std::max(maxCorner[axis], box[axis + 3]);
        }
    }

    if (ownerBodyTag == NULL_TAG)
    {
        throw std::runtime_error("No owning solid body was found for subtract.");
    }

    NXOpen::Features::Feature* envelopeFeature = CreateEnvelopeBlock(workPart, minCorner, maxCorner, envelopeOffset);
    NXOpen::Body* targetBody = dynamic_cast<NXOpen::Body*>(NXOpen::NXObjectManager::Get(ownerBodyTag));
    if (targetBody == nullptr)
    {
        throw std::runtime_error("Failed to get the owning body.");
    }

    if (!enableBooleanSubtract)
    {
      if (removeBlend)
      {
          const std::vector<NXOpen::Face*> candidateFaces = CollectFacesFromBody(targetBody);
          const std::vector<NXOpen::Face*> blendFaces = FindBlendFacesByRadius(workPart, candidateFaces, blendRadius);
          ColorFaces(NXOpen::Session::GetSession(), blendFaces, kRedColor);
          DeleteBlendFaces(workPart, blendFaces, blendRadius);
      }
        return;
    }

    NXOpen::Body* toolBody = AskFeatureBody(envelopeFeature);
    if (targetBody == nullptr || toolBody == nullptr)
    {
        throw std::runtime_error("Failed to get bodies required by boolean subtract.");
    }

    NXOpen::Features::BooleanFeature* booleanFeature = SubtractToolBody(workPart, targetBody, toolBody);
    static_cast<void>(healRemovedRegion);
    if (outBooleanFeature != nullptr)
    {
        *outBooleanFeature = booleanFeature;
    }

    if (removeBlend)
    {
        const std::vector<NXOpen::Face*> candidateFaces = CollectFacesFromBody(targetBody);
        const std::vector<NXOpen::Face*> blendFaces = FindBlendFacesByRadius(workPart, candidateFaces, blendRadius);
        DeleteBlendFaces(workPart, blendFaces, blendRadius);
    }
}
}

BaoRongQieChuDialog::BaoRongQieChuDialog()
    : ui_(NXOpen::UI::GetUI()),
      session_(NXOpen::Session::GetSession()),
      dialog_(nullptr),
      mainGroup_(nullptr),
      objectSelectBlock_(nullptr),
      booleanToggleBlock_(nullptr),
      removeBlendToggleBlock_(nullptr),
      blendRadiusBlock_(nullptr),
      healRemovedRegionToggleBlock_(nullptr),
      offsetBlock_(nullptr),
      pendingHoleProfiles_(),
      pendingBlendBodies_(),
      pendingCutFeatures_(),
      isInternalUpdate_(false)
{
    dialog_ = ui_->CreateDialog(GetDialogFilePath().c_str());
    dialog_->AddInitializeHandler(NXOpen::make_callback(this, &BaoRongQieChuDialog::initialize_cb));
    dialog_->AddDialogShownHandler(NXOpen::make_callback(this, &BaoRongQieChuDialog::dialogShown_cb));
    dialog_->AddUpdateHandler(NXOpen::make_callback(this, &BaoRongQieChuDialog::update_cb));
    dialog_->AddApplyHandler(NXOpen::make_callback(this, &BaoRongQieChuDialog::apply_cb));
    dialog_->AddOkHandler(NXOpen::make_callback(this, &BaoRongQieChuDialog::ok_cb));
    dialog_->AddCancelHandler(NXOpen::make_callback(this, &BaoRongQieChuDialog::cancel_cb));
}

BaoRongQieChuDialog::~BaoRongQieChuDialog()
{
    if (dialog_ != nullptr)
    {
        delete dialog_;
        dialog_ = nullptr;
    }
}

NXOpen::BlockStyler::BlockDialog::DialogResponse BaoRongQieChuDialog::Launch()
{
    return dialog_->Launch();
}

void BaoRongQieChuDialog::initialize_cb()
{
    mainGroup_ = dialog_->TopBlock()->FindBlock("main_group");
    objectSelectBlock_ = dialog_->TopBlock()->FindBlock("object_select");
    booleanToggleBlock_ = dialog_->TopBlock()->FindBlock("boolean_subtract");
    removeBlendToggleBlock_ = dialog_->TopBlock()->FindBlock("remove_body_blend");
    blendRadiusBlock_ = dialog_->TopBlock()->FindBlock("blend_radius_value");
    healRemovedRegionToggleBlock_ = dialog_->TopBlock()->FindBlock("heal_removed_region");
    offsetBlock_ = dialog_->TopBlock()->FindBlock("offset_value");

    NXOpen::BlockStyler::PropertyList* properties = objectSelectBlock_->GetProperties();
    NXOpen::Selection::SelectionAction action = NXOpen::Selection::SelectionActionClearAndEnableSpecific;
    std::vector<NXOpen::Selection::MaskTriple> selectionMaskArray;
    selectionMaskArray.emplace_back(UF_solid_type, UF_solid_body_subtype, UF_UI_SEL_FEATURE_BODY);
    selectionMaskArray.emplace_back(UF_solid_type, UF_solid_body_subtype, UF_UI_SEL_FEATURE_ANY_FACE);
    selectionMaskArray.emplace_back(UF_solid_type, UF_solid_body_subtype, UF_UI_SEL_FEATURE_ANY_EDGE);
    properties->SetSelectionFilter("SelectionFilter", action, selectionMaskArray);
    properties->SetLogical("AutomaticProgression", true);
    delete properties;

    SyncOptionalControls();
}

void BaoRongQieChuDialog::dialogShown_cb()
{
    SyncOptionalControls();
}

int BaoRongQieChuDialog::update_cb(NXOpen::BlockStyler::UIBlock* block)
{
    if (block == removeBlendToggleBlock_)
    {
        SyncOptionalControls();
        return 0;
    }

    if (!isInternalUpdate_ && block == objectSelectBlock_)
    {
        return ExecuteImmediateCutFromSelection();
    }

    return 0;
}

int BaoRongQieChuDialog::apply_cb()
{
    return ExecuteFromSelection();
}

int BaoRongQieChuDialog::ok_cb()
{
    return ExecuteFromSelection();
}

int BaoRongQieChuDialog::cancel_cb()
{
    return 0;
}

int BaoRongQieChuDialog::ExecuteFromSelection()
{
    int result = ExecuteImmediateCutFromSelection();
    if (result != 0)
    {
        return result;
    }

    result = ExecutePendingBlendRemoval();
    if (result != 0)
    {
        return result;
    }

    return ExecutePendingCutFaceRemoval();
}

int BaoRongQieChuDialog::ExecuteImmediateCutFromSelection()
{
    const std::vector<NXOpen::TaggedObject*> selectedObjects = GetSelectedObjects();
    if (selectedObjects.empty())
    {
        return 0;
    }

    NXOpen::Part* workPart = session_ != nullptr && session_->Parts() != nullptr ? session_->Parts()->Work() : nullptr;
    if (workPart == nullptr)
    {
        ShowError("No active work part.");
        return 1;
    }

    const NXOpen::Session::UndoMarkId markId =
        session_->SetUndoMark(NXOpen::Session::MarkVisibilityVisible, "BaoRongQieChu");

    try
    {
        NXOpen::Body* targetBody = ResolveTargetBodyFromSelection(selectedObjects);
        const double envelopeOffset = GetOffsetValue();
        NXOpen::Features::BooleanFeature* booleanFeature = nullptr;
        if (GetHealRemovedRegionEnabled())
        {
            CapturePendingHoleProfiles(selectedObjects);
        }
        else
        {
            ClearPendingHoleProfiles();
        }
        ExecuteEnvelopeCut(
            workPart,
            selectedObjects,
            GetBooleanSubtractEnabled(),
            envelopeOffset,
            false,
            0.0,
            false,
            &booleanFeature);
        if (GetRemoveBlendEnabled())
        {
            RememberPendingBlendBody(targetBody);
        }
        if (booleanFeature != nullptr)
        {
            ColorFaces(session_, booleanFeature->GetFaces(), kBlueColor);
        }
        if (GetHealRemovedRegionEnabled() && booleanFeature != nullptr)
        {
            RememberPendingCutFeature(booleanFeature);
        }
        ClearSelection();
        session_->DeleteUndoMark(markId, "BaoRongQieChu");
        return 0;
    }
    catch (const NXOpen::NXException& ex)
    {
        ClearPendingHoleProfiles();
        const char* message = ex.Message();
        ShowError(message != nullptr ? message : "NXOpen execution failed.");
    }
    catch (const std::exception& ex)
    {
        ClearPendingHoleProfiles();
        ShowError(ex.what());
    }

    session_->UndoToMark(markId, "BaoRongQieChu");
    session_->DeleteUndoMark(markId, "BaoRongQieChu");
    return 1;
}

int BaoRongQieChuDialog::ExecutePendingBlendRemoval()
{
    if (!GetRemoveBlendEnabled())
    {
        pendingBlendBodies_.clear();
        return 0;
    }

    if (pendingBlendBodies_.empty())
    {
        return 0;
    }

    NXOpen::Part* workPart = session_ != nullptr && session_->Parts() != nullptr ? session_->Parts()->Work() : nullptr;
    if (workPart == nullptr)
    {
        ShowError("No active work part.");
        return 1;
    }

    const NXOpen::Session::UndoMarkId markId =
        session_->SetUndoMark(NXOpen::Session::MarkVisibilityVisible, "BaoRongQieChuRemoveBlend");

    try
    {
        for (NXOpen::Body* body : pendingBlendBodies_)
        {
            if (body == nullptr)
            {
                continue;
            }

            const std::vector<NXOpen::Face*> candidateFaces = CollectFacesFromBody(body);
            const std::vector<NXOpen::Face*> blendFaces =
                FindBlendFacesByRadius(workPart, candidateFaces, GetBlendRadiusValue());
            ColorFaces(session_, blendFaces, kRedColor);
            DeleteBlendFaces(workPart, blendFaces, GetBlendRadiusValue());
        }

        pendingBlendBodies_.clear();
        session_->DeleteUndoMark(markId, "BaoRongQieChuRemoveBlend");
        return 0;
    }
    catch (const NXOpen::NXException& ex)
    {
        const char* message = ex.Message();
        ShowError(message != nullptr ? message : "NXOpen execution failed.");
    }
    catch (const std::exception& ex)
    {
        ShowError(ex.what());
    }

    session_->UndoToMark(markId, "BaoRongQieChuRemoveBlend");
    session_->DeleteUndoMark(markId, "BaoRongQieChuRemoveBlend");
    return 1;
}

int BaoRongQieChuDialog::ExecutePendingCutFaceRemoval()
{
    if (!GetHealRemovedRegionEnabled())
    {
        pendingCutFeatures_.clear();
        ClearPendingHoleProfiles();
        return 0;
    }

    if (pendingCutFeatures_.empty())
    {
        return 0;
    }

    NXOpen::Part* workPart = session_ != nullptr && session_->Parts() != nullptr ? session_->Parts()->Work() : nullptr;
    if (workPart == nullptr)
    {
        ShowError("No active work part.");
        return 1;
    }

    const NXOpen::Session::UndoMarkId markId =
        session_->SetUndoMark(NXOpen::Session::MarkVisibilityVisible, "BaoRongQieChuHealCutFaces");

    try
    {
        std::map<tag_t, std::vector<NXOpen::Face*>> facesByBody;
        std::unordered_set<tag_t> seenFaceTags;

        for (NXOpen::Features::BooleanFeature* feature : pendingCutFeatures_)
        {
            if (feature == nullptr)
            {
                continue;
            }

            const std::vector<NXOpen::Face*> cutFaces = feature->GetFaces();
            for (NXOpen::Face* face : cutFaces)
            {
                if (face == nullptr || !seenFaceTags.insert(face->Tag()).second)
                {
                    continue;
                }

                tag_t bodyTag = NULL_TAG;
                if (UF_MODL_ask_face_body(face->Tag(), &bodyTag) != 0 || bodyTag == NULL_TAG)
                {
                    continue;
                }

                facesByBody[bodyTag].push_back(face);
            }
        }

        for (const auto& entry : facesByBody)
        {
            if (entry.second.empty())
            {
                continue;
            }

            ColorFaces(session_, entry.second, kBlueColor);
            DeleteFacesWithHeal(workPart, entry.second);
        }

        RestorePendingHoleProfiles(workPart);
        pendingCutFeatures_.clear();
        session_->DeleteUndoMark(markId, "BaoRongQieChuHealCutFaces");
        return 0;
    }
    catch (const NXOpen::NXException& ex)
    {
        const char* message = ex.Message();
        ShowError(message != nullptr ? message : "NXOpen execution failed.");
    }
    catch (const std::exception& ex)
    {
        ShowError(ex.what());
    }

    session_->UndoToMark(markId, "BaoRongQieChuHealCutFaces");
    session_->DeleteUndoMark(markId, "BaoRongQieChuHealCutFaces");
    return 1;
}

std::vector<NXOpen::TaggedObject*> BaoRongQieChuDialog::GetSelectedObjects() const
{
    if (objectSelectBlock_ == nullptr)
    {
        return {};
    }

    NXOpen::BlockStyler::PropertyList* properties = objectSelectBlock_->GetProperties();
    const std::vector<NXOpen::TaggedObject*> selectedObjects = properties->GetTaggedObjectVector("SelectedObjects");
    delete properties;
    return selectedObjects;
}

bool BaoRongQieChuDialog::GetBooleanSubtractEnabled() const
{
    if (booleanToggleBlock_ == nullptr)
    {
        return true;
    }

    NXOpen::BlockStyler::PropertyList* properties = booleanToggleBlock_->GetProperties();
    const bool value = properties->GetLogical("Value");
    delete properties;
    return value;
}

bool BaoRongQieChuDialog::GetRemoveBlendEnabled() const
{
    if (removeBlendToggleBlock_ == nullptr)
    {
        return false;
    }

    NXOpen::BlockStyler::PropertyList* properties = removeBlendToggleBlock_->GetProperties();
    const bool value = properties->GetLogical("Value");
    delete properties;
    return value;
}

bool BaoRongQieChuDialog::GetHealRemovedRegionEnabled() const
{
    if (healRemovedRegionToggleBlock_ == nullptr)
    {
        return true;
    }

    NXOpen::BlockStyler::PropertyList* properties = healRemovedRegionToggleBlock_->GetProperties();
    const bool value = properties->GetLogical("Value");
    delete properties;
    return value;
}

double BaoRongQieChuDialog::GetOffsetValue() const
{
    if (offsetBlock_ == nullptr)
    {
        return 0.1;
    }

    NXOpen::BlockStyler::PropertyList* properties = offsetBlock_->GetProperties();
    const double value = properties->GetDouble("Value");
    delete properties;
    return value;
}

double BaoRongQieChuDialog::GetBlendRadiusValue() const
{
    if (blendRadiusBlock_ == nullptr)
    {
        return 0.0;
    }

    NXOpen::BlockStyler::PropertyList* properties = blendRadiusBlock_->GetProperties();
    const double value = properties->GetDouble("Value");
    delete properties;
    return value;
}

void BaoRongQieChuDialog::ClearSelection()
{
    if (objectSelectBlock_ == nullptr)
    {
        return;
    }

    isInternalUpdate_ = true;
    NXOpen::BlockStyler::PropertyList* properties = objectSelectBlock_->GetProperties();
    properties->SetTaggedObjectVector("SelectedObjects", std::vector<NXOpen::TaggedObject*>());
    delete properties;
    isInternalUpdate_ = false;
}

void BaoRongQieChuDialog::ShowError(const std::string& message) const
{
    ui_->NXMessageBox()->Show("BaoRongQieChu", NXOpen::NXMessageBox::DialogTypeError, message);
}

void BaoRongQieChuDialog::SyncOptionalControls()
{
    const bool removeBlend = GetRemoveBlendEnabled();

    if (blendRadiusBlock_ != nullptr)
    {
        NXOpen::BlockStyler::PropertyList* properties = blendRadiusBlock_->GetProperties();
        properties->SetLogical("Show", removeBlend);
        delete properties;
    }

    if (!removeBlend)
    {
        pendingBlendBodies_.clear();
    }

    if (!GetHealRemovedRegionEnabled())
    {
        pendingCutFeatures_.clear();
        ClearPendingHoleProfiles();
    }
}

void BaoRongQieChuDialog::RememberPendingBlendBody(NXOpen::Body* body)
{
    if (body == nullptr)
    {
        return;
    }

    for (NXOpen::Body* existingBody : pendingBlendBodies_)
    {
        if (existingBody == body)
        {
            return;
        }
    }

    pendingBlendBodies_.push_back(body);
}

void BaoRongQieChuDialog::RememberPendingCutFeature(NXOpen::Features::BooleanFeature* feature)
{
    if (feature == nullptr)
    {
        return;
    }

    for (NXOpen::Features::BooleanFeature* existingFeature : pendingCutFeatures_)
    {
        if (existingFeature == feature)
        {
            return;
        }
    }

    pendingCutFeatures_.push_back(feature);
}

void BaoRongQieChuDialog::CapturePendingHoleProfiles(const std::vector<NXOpen::TaggedObject*>& selectedObjects)
{
    std::unordered_set<std::string> capturedLoops;
    std::unordered_set<std::string> capturedProfilePlaneLoops;
    for (const HoleLoopProfile& profile : pendingHoleProfiles_)
    {
        if (!profile.profileKey.empty())
        {
            capturedProfilePlaneLoops.insert(profile.profileKey);
        }
    }

    std::unordered_set<std::string> capturedProjectedLoopsInThisSelection;
    std::unordered_set<tag_t> capturedHoleFaces;
    std::unordered_set<tag_t> consumedHoleEdges;
    const auto markConsumedEdges = [&](const std::unordered_set<tag_t>& edges)
    {
        consumedHoleEdges.insert(edges.begin(), edges.end());
    };

    const auto hasConsumedEdge = [&](const std::unordered_set<tag_t>& edges) -> bool
    {
        for (tag_t edgeTag : edges)
        {
            if (consumedHoleEdges.find(edgeTag) != consumedHoleEdges.end())
            {
                return true;
            }
        }
        return false;
    };

    const auto captureFirstMatchingLoop = [&](const std::unordered_set<tag_t>& selectedRingEdges) -> bool
    {
        std::unordered_set<tag_t> checkedPlanarFaces;
        std::vector<PlanarFaceData> candidatePlanes;
        for (tag_t ringEdge : selectedRingEdges)
        {
            const std::vector<tag_t> adjacentFaces = AskEdgeFaces(ringEdge);
            for (tag_t faceTag : adjacentFaces)
            {
                if (faceTag == NULL_TAG || !checkedPlanarFaces.insert(faceTag).second)
                {
                    continue;
                }

                PlanarFaceData planeData;
                if (!AskPlanarFaceData(faceTag, planeData))
                {
                    continue;
                }

                candidatePlanes.push_back(planeData);
            }
        }

        for (const PlanarFaceData& planeData : candidatePlanes)
        {
                NXOpen::Body* targetBody = dynamic_cast<NXOpen::Body*>(NXOpen::NXObjectManager::Get(planeData.bodyTag));
                if (targetBody == nullptr)
                {
                    continue;
                }

                uf_loop_p_t loopList = nullptr;
                if (UF_MODL_ask_face_loops(planeData.faceTag, &loopList) != 0 || loopList == nullptr)
                {
                    continue;
                }

                bool capturedFromThisFace = false;
                for (uf_loop_p_t loop = loopList; loop != nullptr; loop = loop->next)
                {
                    if (loop->type != 2 || loop->edge_list == nullptr || !LoopUsesAnyEdge(loop, selectedRingEdges))
                    {
                        continue;
                    }

                    const std::string loopKey = MakeLoopKey(planeData.faceTag, loop);
                    if (!capturedLoops.insert(loopKey).second)
                    {
                        continue;
                    }

                    const std::string projectedLoopKey =
                        MakeProjectedLoopKey(planeData.bodyTag, planeData.normal, loop);
                    const std::string profilePlaneLoopKey =
                        MakeProfilePlaneLoopKey(planeData.bodyTag, planeData.normal, loop);
                    if (capturedProfilePlaneLoops.find(profilePlaneLoopKey) != capturedProfilePlaneLoops.end())
                    {
                        continue;
                    }

                    if (!capturedProjectedLoopsInThisSelection.insert(projectedLoopKey).second)
                    {
                        continue;
                    }
                    capturedProfilePlaneLoops.insert(profilePlaneLoopKey);

                    HoleLoopProfile profile;
                    profile.targetBody = targetBody;
                    profile.origin = planeData.origin;
                    const HoleDepthDirection depthDirection =
                        EstimateHoleDepthDirectionFromPlanes(candidatePlanes, planeData);
                    profile.normal = depthDirection.direction;
                    profile.depth = depthDirection.depth;
                    profile.profileKey = profilePlaneLoopKey;

                    for (uf_list_p_t edgeNode = loop->edge_list; edgeNode != nullptr; edgeNode = edgeNode->next)
                    {
                        tag_t curveTag = NULL_TAG;
                        if (UF_MODL_create_curve_from_edge(edgeNode->eid, &curveTag) == 0 && curveTag != NULL_TAG)
                        {
                            profile.curveTags.push_back(curveTag);
                        }
                    }

                    if (!profile.curveTags.empty())
                    {
                        pendingHoleProfiles_.push_back(profile);
                        capturedFromThisFace = true;
                        break;
                    }
                }

                UF_MODL_delete_loop_list(&loopList);
                if (capturedFromThisFace)
                {
                    return true;
                }
        }

        return false;
    };

    for (NXOpen::TaggedObject* object : selectedObjects)
    {
        NXOpen::Edge* edge = dynamic_cast<NXOpen::Edge*>(object);
        if (edge != nullptr)
        {
            if (consumedHoleEdges.find(edge->Tag()) != consumedHoleEdges.end())
            {
                continue;
            }

            bool touchesHoleFace = false;
            bool capturedFromHoleFace = false;
            const std::vector<tag_t> adjacentFaces = AskEdgeFaces(edge->Tag());
            for (tag_t adjacentFace : adjacentFaces)
            {
                if (adjacentFace == NULL_TAG || IsPlanarFace(adjacentFace))
                {
                    continue;
                }

                touchesHoleFace = true;
                if (!capturedHoleFaces.insert(adjacentFace).second)
                {
                    continue;
                }

                const std::unordered_set<tag_t> ringEdges = ExpandHoleRingEdgesFromSeedEdge(edge->Tag());
                if (captureFirstMatchingLoop(ringEdges))
                {
                    markConsumedEdges(ringEdges);
                    capturedFromHoleFace = true;
                }
                break;
            }

            if (!touchesHoleFace && !capturedFromHoleFace)
            {
                const std::unordered_set<tag_t> ringEdges{edge->Tag()};
                if (!hasConsumedEdge(ringEdges) && captureFirstMatchingLoop(ringEdges))
                {
                    markConsumedEdges(ringEdges);
                }
            }
            continue;
        }

        NXOpen::Face* face = dynamic_cast<NXOpen::Face*>(object);
        if (face == nullptr)
        {
            continue;
        }

        int faceType = 0;
        double point[3] = {0.0, 0.0, 0.0};
        double dir[3] = {0.0, 0.0, 1.0};
        double box[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
        double radius = 0.0;
        double radData = 0.0;
        int normDir = 1;
        if (UF_MODL_ask_face_data(face->Tag(), &faceType, point, dir, box, &radius, &radData, &normDir) != 0)
        {
            continue;
        }

        // Do not treat a selected carrier plane as "all holes on this plane".
        if (faceType == 22 && FaceHasHoleLoop(face->Tag()))
        {
            continue;
        }

        if (faceType != 22 && !capturedHoleFaces.insert(face->Tag()).second)
        {
            continue;
        }

        const std::vector<tag_t> faceEdges = AskFaceEdges(face->Tag());
        if (!faceEdges.empty())
        {
            const std::unordered_set<tag_t> ringEdges(faceEdges.begin(), faceEdges.end());
            if (!hasConsumedEdge(ringEdges) && captureFirstMatchingLoop(ringEdges))
            {
                markConsumedEdges(ringEdges);
            }
        }
    }
}

void BaoRongQieChuDialog::RestorePendingHoleProfiles(NXOpen::Part* workPart)
{
    if (workPart == nullptr || pendingHoleProfiles_.empty())
    {
        return;
    }

    int previousBodyTypePreference = UF_MODL_SOLID_BODY;
    const bool shouldRestoreBodyTypePreference =
        UF_MODL_ask_body_type_pref(&previousBodyTypePreference) == 0;
    static_cast<void>(UF_MODL_set_body_type_pref(UF_MODL_SOLID_BODY));

    bool reportedFailure = false;
    for (const HoleLoopProfile& profile : pendingHoleProfiles_)
    {
        if (profile.targetBody == nullptr || profile.curveTags.empty())
        {
            continue;
        }

        NXOpen::Features::ExtrudeBuilder* extrudeBuilder = nullptr;
        std::string failureStage = "preparing";
        double restoreDepth = 0.0;
        NXOpen::Features::Feature* toolFeature = nullptr;
        tag_t toolBodyTag = NULL_TAG;
        const auto cleanupTool = [&]()
        {
            if (toolBodyTag != NULL_TAG)
            {
                DeleteObjectIfAlive(toolBodyTag);
                toolBodyTag = NULL_TAG;
            }
            else if (toolFeature != nullptr)
            {
                DeleteObjectIfAlive(toolFeature->Tag());
                toolFeature = nullptr;
            }
        };

        try
        {
            std::vector<NXOpen::NXObject*> curves;
            curves.reserve(profile.curveTags.size());
            for (tag_t curveTag : profile.curveTags)
            {
                NXOpen::NXObject* curveObject = dynamic_cast<NXOpen::NXObject*>(NXOpen::NXObjectManager::Get(curveTag));
                if (curveObject != nullptr)
                {
                    curves.push_back(curveObject);
                }
            }

            if (curves.empty())
            {
                if (!reportedFailure)
                {
                    ShowError("Hole restore skipped: profile curves were created, but NX could not resolve them.");
                    reportedFailure = true;
                }
                continue;
            }

            failureStage = "creating section";
            std::vector<NXOpen::Section*> sections;
            workPart->Sections()->CreateSectionsUsingCurves(
                curves,
                NXOpen::SectionCollection::LoopOptionSeparate,
                0.01,
                0.01,
                0.5,
                sections);
            if (sections.empty())
            {
                if (!reportedFailure)
                {
                    ShowError("Hole restore skipped: NX did not create a closed section from the selected inner loop.");
                    reportedFailure = true;
                }
                continue;
            }

            failureStage = "creating extrude";
            extrudeBuilder = workPart->Features()->CreateExtrudeBuilder(nullptr);
            extrudeBuilder->FeatureOptions()->SetBodyType(
                NXOpen::GeometricUtilities::FeatureOptions::BodyStyleSolid);
            extrudeBuilder->SmartVolumeProfile()->SetOpenProfileSmartVolumeOption(false);
            NXOpen::Direction* direction =
                workPart->Directions()->CreateDirection(
                    profile.origin,
                    profile.normal,
                    NXOpen::SmartObject::UpdateOptionWithinModeling);
            extrudeBuilder->SetSection(sections.front());
            extrudeBuilder->SetDirection(direction);
            extrudeBuilder->BooleanOperation()->SetType(
                NXOpen::GeometricUtilities::BooleanOperation::BooleanTypeCreate);
            const double startDistance = -kHoleRestoreOverrun;
            restoreDepth = std::max(profile.depth, kMinimumBlockSize) + kHoleRestoreOverrun;
            extrudeBuilder->Limits()->StartExtend()->SetTrimType(
                NXOpen::GeometricUtilities::Extend::ExtendTypeValue);
            extrudeBuilder->Limits()->StartExtend()->SetValue(FormatDouble(startDistance).c_str());
            extrudeBuilder->Limits()->EndExtend()->SetTrimType(
                NXOpen::GeometricUtilities::Extend::ExtendTypeValue);
            extrudeBuilder->Limits()->EndExtend()->SetValue(FormatDouble(restoreDepth).c_str());
            failureStage = "committing tool extrude";
            toolFeature = extrudeBuilder->CommitFeature();
            if (toolFeature == nullptr)
            {
                if (!reportedFailure)
                {
                    ShowError("Hole restore failed: NX returned no tool extrude feature. Depth = " + FormatDouble(restoreDepth));
                    reportedFailure = true;
                }
                extrudeBuilder->Destroy();
                extrudeBuilder = nullptr;
                continue;
            }

            NXOpen::Body* toolBody = AskFeatureBody(toolFeature);
            if (toolBody == nullptr)
            {
                if (!reportedFailure)
                {
                    ShowError(
                        "Hole restore failed: NX returned an extrude feature without a body. "
                        "Depth = " + FormatDouble(restoreDepth));
                    reportedFailure = true;
                }
                extrudeBuilder->Destroy();
                extrudeBuilder = nullptr;
                continue;
            }
            toolBodyTag = toolBody->Tag();

            extrudeBuilder->Destroy();
            extrudeBuilder = nullptr;

            failureStage = "removing tool body parameters";
            RemoveParametersFromBody(workPart, toolBody);
            toolFeature = nullptr;

            failureStage = "subtracting restored hole";
            static_cast<void>(SubtractToolBody(workPart, profile.targetBody, toolBody));
        }
        catch (const NXOpen::NXException& ex)
        {
            if (extrudeBuilder != nullptr)
            {
                try
                {
                    extrudeBuilder->Destroy();
                }
                catch (...)
                {
                }
            }

            cleanupTool();
            if (failureStage == "subtracting restored hole")
            {
                continue;
            }

            if (!reportedFailure)
            {
                const char* message = ex.Message();
                ShowError(
                    "Hole restore failed while " + failureStage +
                    ". Depth = " + FormatDouble(restoreDepth) +
                    ". NX message: " + (message != nullptr ? message : "NXOpen exception."));
                reportedFailure = true;
            }
            continue;
        }
        catch (const std::exception& ex)
        {
            if (extrudeBuilder != nullptr)
            {
                try
                {
                    extrudeBuilder->Destroy();
                }
                catch (...)
                {
                }
            }

            cleanupTool();
            if (failureStage == "subtracting restored hole")
            {
                continue;
            }

            if (!reportedFailure)
            {
                ShowError(
                    "Hole restore failed while " + failureStage +
                    ". Depth = " + FormatDouble(restoreDepth) +
                    ". Error: " + ex.what());
                reportedFailure = true;
            }
            continue;
        }
        catch (...)
        {
            if (extrudeBuilder != nullptr)
            {
                try
                {
                    extrudeBuilder->Destroy();
                }
                catch (...)
                    {
                }
            }
            cleanupTool();
            if (failureStage == "subtracting restored hole")
            {
                continue;
            }

            if (!reportedFailure)
            {
                ShowError(
                    "Hole restore failed while " + failureStage +
                    ". Depth = " + FormatDouble(restoreDepth) +
                    ". Unknown NX error.");
                reportedFailure = true;
            }
            continue;
        }
    }

    if (shouldRestoreBodyTypePreference)
    {
        static_cast<void>(UF_MODL_set_body_type_pref(previousBodyTypePreference));
    }

    for (const HoleLoopProfile& profile : pendingHoleProfiles_)
    {
        for (tag_t curveTag : profile.curveTags)
        {
            DeleteObjectIfAlive(curveTag);
        }
    }

    pendingHoleProfiles_.clear();
}

void BaoRongQieChuDialog::ClearPendingHoleProfiles()
{
    for (const HoleLoopProfile& profile : pendingHoleProfiles_)
    {
        for (tag_t curveTag : profile.curveTags)
        {
            DeleteObjectIfAlive(curveTag);
        }
    }

    pendingHoleProfiles_.clear();
}
