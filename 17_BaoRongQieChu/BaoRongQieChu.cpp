#include "BaoRongQieChu.hpp"
#include "../../common/ZhihuiEmbeddedDialog.hpp"
#include "../../common/ZhihuiDialogMemory.hpp"
#include "embedded_dialog_resources.h"

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
#include <NXOpen/Features_ReplaceFaceBuilder.hxx>
#include <NXOpen/Features_ResizeBlendBuilder.hxx>
#include <NXOpen/Features_SheetMetal_SheetmetalManager.hxx>
#include <NXOpen/GeometricUtilities_BooleanOperation.hxx>
#include <NXOpen/GeometricUtilities_Extend.hxx>
#include <NXOpen/GeometricUtilities_FeatureOptions.hxx>
#include <NXOpen/GeometricUtilities_Limits.hxx>
#include <NXOpen/GeometricUtilities_SmartVolumeProfileBuilder.hxx>
#include <NXOpen/FaceDumbRule.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXMessageBox.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/MeasureFaces.hxx>
#include <NXOpen/MeasureManager.hxx>
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
#include <NXOpen/UnitCollection.hxx>

#include <Windows.h>
#ifdef CreateDialog
#undef CreateDialog
#endif

#include <uf_modl.h>
#include <uf_modl_primitives.h>
#include <uf_modl_curves.h>
#include <uf_modl_utilities.h>
#include <uf_disp.h>
#include <uf_eval.h>
#include <uf_obj.h>
#include <uf_object_types.h>
#include <uf_ui_types.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
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

std::filesystem::path GetDebugLogPath()
{
    wchar_t buffer[MAX_PATH] = {};
    const HMODULE module = reinterpret_cast<HMODULE>(&__ImageBase);
    const DWORD length = GetModuleFileNameW(module, buffer, MAX_PATH);
    if (length > 0 && length < MAX_PATH)
    {
        const std::filesystem::path modulePath(buffer);
        const std::filesystem::path applicationDirectory = modulePath.parent_path();
        if (applicationDirectory.filename() == L"application")
        {
            return applicationDirectory.parent_path() / L"logs" / L"BaoRongQieChu_debug.log";
        }

        return applicationDirectory / L"BaoRongQieChu_debug.log";
    }

    return std::filesystem::path(L"D:\\UG\u667a\u8f89\u94a3\u91d1\u63d2\u4ef6\\logs\\BaoRongQieChu_debug.log");
}

void WriteDebugLog(const std::string& message)
{
    try
    {
        const std::filesystem::path logPath = GetDebugLogPath();
        std::error_code error;
        std::filesystem::create_directories(logPath.parent_path(), error);

        SYSTEMTIME time = {};
        GetLocalTime(&time);
        std::ofstream stream(logPath, std::ios::out | std::ios::app);
        if (!stream)
        {
            return;
        }

        stream << '['
               << time.wYear << '-'
               << (time.wMonth < 10 ? "0" : "") << time.wMonth << '-'
               << (time.wDay < 10 ? "0" : "") << time.wDay << ' '
               << (time.wHour < 10 ? "0" : "") << time.wHour << ':'
               << (time.wMinute < 10 ? "0" : "") << time.wMinute << ':'
               << (time.wSecond < 10 ? "0" : "") << time.wSecond << '.';
        stream.width(3);
        stream.fill('0');
        stream << time.wMilliseconds << "] " << message << '\n';
    }
    catch (...)
    {
    }
}

std::string FormatTagList(const std::vector<tag_t>& tags)
{
    std::ostringstream stream;
    stream << '[';
    for (size_t index = 0; index < tags.size(); ++index)
    {
        if (index > 0)
        {
            stream << ',';
        }
        stream << tags[index];
    }
    stream << ']';
    return stream.str();
}

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

    return "D:\\UG智辉钣金插件\\application\\BaoRongQieChu.dlx";
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
    double spanX;
    double spanY;
    double spanZ;
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
    geometry.spanX = hasBox ? box[3] - box[0] : 0.0;
    geometry.spanY = hasBox ? box[4] - box[1] : 0.0;
    geometry.spanZ = hasBox ? box[5] - box[2] : 0.0;
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
        << ':' << QuantizeProfileKeyValue(geometry.projectedZ)
        << ':' << QuantizeProfileKeyValue(geometry.spanX)
        << ':' << QuantizeProfileKeyValue(geometry.spanY)
        << ':' << QuantizeProfileKeyValue(geometry.spanZ);
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

bool NormalizeVector(double vector[3])
{
    const double length = std::sqrt(
        vector[0] * vector[0] + vector[1] * vector[1] + vector[2] * vector[2]);
    if (length <= 1.0e-12)
    {
        return false;
    }

    vector[0] /= length;
    vector[1] /= length;
    vector[2] /= length;
    return true;
}

double DotVector(const double first[3], const double second[3])
{
    return first[0] * second[0] + first[1] * second[1] + first[2] * second[2];
}

bool AskCylinderFaceGeometry(
    NXOpen::Face* face,
    double axisPoint[3],
    double axisDirection[3],
    double& radius)
{
    if (face == nullptr)
    {
        return false;
    }

    int faceType = 0;
    double box[6] = {};
    double radialData = 0.0;
    int normalDirection = 0;
    if (UF_MODL_ask_face_data(
            face->Tag(),
            &faceType,
            axisPoint,
            axisDirection,
            box,
            &radius,
            &radialData,
            &normalDirection) != 0 ||
        faceType != UF_MODL_CYLINDRICAL_FACE ||
        radius <= 1.0e-9 ||
        !NormalizeVector(axisDirection))
    {
        return false;
    }

    return true;
}

bool AskCylinderFaceHeight(
    NXOpen::Face* face,
    const double axisPoint[3],
    const double axisDirection[3],
    double& height)
{
    height = 0.0;
    if (face == nullptr)
    {
        return false;
    }

    double minimumProjection = std::numeric_limits<double>::max();
    double maximumProjection = -std::numeric_limits<double>::max();
    size_t pointCount = 0;
    for (NXOpen::Edge* edge : face->GetEdges())
    {
        if (edge == nullptr)
        {
            continue;
        }

        NXOpen::Point3d first;
        NXOpen::Point3d second;
        edge->GetVertices(&first, &second);
        const NXOpen::Point3d points[2] = { first, second };
        for (const NXOpen::Point3d& point : points)
        {
            const double delta[3] =
            {
                point.X - axisPoint[0],
                point.Y - axisPoint[1],
                point.Z - axisPoint[2]
            };
            const double projection = DotVector(delta, axisDirection);
            minimumProjection = std::min(minimumProjection, projection);
            maximumProjection = std::max(maximumProjection, projection);
            ++pointCount;
        }
    }

    if (pointCount < 2 ||
        minimumProjection == std::numeric_limits<double>::max() ||
        maximumProjection == -std::numeric_limits<double>::max())
    {
        return false;
    }

    height = std::abs(maximumProjection - minimumProjection);
    return height > 1.0e-9;
}

bool AskCylinderFaceAngularSpan(NXOpen::Face* face, double& spanRadians)
{
    spanRadians = 0.0;
    if (face == nullptr)
    {
        return false;
    }

    double uvMinMax[4] = {};
    if (UF_MODL_ask_face_uv_minmax(face->Tag(), uvMinMax) != 0)
    {
        return false;
    }

    const double uSpan = std::abs(uvMinMax[1] - uvMinMax[0]);
    const double vSpan = std::abs(uvMinMax[3] - uvMinMax[2]);
    int uStatus = 0;
    int vStatus = 0;
    double uPeriod = 0.0;
    double vPeriod = 0.0;
    if (UF_MODL_ask_face_periodicity(
            face->Tag(),
            &uStatus,
            &uPeriod,
            &vStatus,
            &vPeriod) == 0)
    {
        if (uPeriod > 1.0e-9 && uSpan <= uPeriod + 1.0e-6)
        {
            spanRadians = uSpan;
            return true;
        }
        if (vPeriod > 1.0e-9 && vSpan <= vPeriod + 1.0e-6)
        {
            spanRadians = vSpan;
            return true;
        }
    }

    spanRadians = std::min(uSpan, vSpan);
    return spanRadians > 1.0e-9;
}

bool AskSheetMetalThickness(
    NXOpen::Part* workPart,
    const std::vector<NXOpen::Face*>& faces,
    double& thickness)
{
    thickness = 0.0;
    if (workPart == nullptr || workPart->Features() == nullptr)
    {
        return false;
    }

    NXOpen::Body* body = nullptr;
    for (NXOpen::Face* face : faces)
    {
        if (face != nullptr)
        {
            body = face->GetBody();
            if (body != nullptr)
            {
                break;
            }
        }
    }
    if (body == nullptr)
    {
        return false;
    }

    try
    {
        NXOpen::Features::SheetMetal::SheetmetalManager* manager =
            workPart->Features()->SheetmetalManager();
        if (manager == nullptr || !manager->IsSheetmetalBody(body))
        {
            return false;
        }

        thickness = manager->GetBodyThickness(body);
        return thickness > 1.0e-9;
    }
    catch (...)
    {
        thickness = 0.0;
        return false;
    }
}

struct ProjectedPoint2d
{
    double x;
    double y;
};

double Cross2d(
    const ProjectedPoint2d& first,
    const ProjectedPoint2d& second,
    const ProjectedPoint2d& third)
{
    return (second.x - first.x) * (third.y - first.y) -
        (second.y - first.y) * (third.x - first.x);
}

double ProjectedPolygonArea(const std::vector<ProjectedPoint2d>& polygon)
{
    double signedArea = 0.0;
    for (size_t index = 0; index < polygon.size(); ++index)
    {
        const ProjectedPoint2d& current = polygon[index];
        const ProjectedPoint2d& next = polygon[(index + 1) % polygon.size()];
        signedArea += current.x * next.y - current.y * next.x;
    }
    return std::abs(signedArea) * 0.5;
}

bool AskPlanarLoopArea(
    uf_loop_p_t loop,
    const NXOpen::Vector3d& planeNormal,
    double& area)
{
    area = 0.0;
    if (loop == nullptr || loop->edge_list == nullptr)
    {
        return false;
    }

    NXOpen::Vector3d normal = NormalizeVector(planeNormal);
    NXOpen::Vector3d reference = std::abs(normal.Z) > 0.9 ?
        NXOpen::Vector3d(0.0, 1.0, 0.0) : NXOpen::Vector3d(0.0, 0.0, 1.0);
    NXOpen::Vector3d axisU(
        reference.Y * normal.Z - reference.Z * normal.Y,
        reference.Z * normal.X - reference.X * normal.Z,
        reference.X * normal.Y - reference.Y * normal.X);
    axisU = NormalizeVector(axisU);
    const NXOpen::Vector3d axisV(
        normal.Y * axisU.Z - normal.Z * axisU.Y,
        normal.Z * axisU.X - normal.X * axisU.Z,
        normal.X * axisU.Y - normal.Y * axisU.X);

    std::vector<ProjectedPoint2d> polygon;
    constexpr int kSamplesPerEdge = 32;
    for (uf_list_p_t edgeNode = loop->edge_list;
         edgeNode != nullptr;
         edgeNode = edgeNode->next)
    {
        if (edgeNode->eid == NULL_TAG)
        {
            continue;
        }

        UF_EVAL_p_t evaluator = nullptr;
        if (UF_EVAL_initialize(edgeNode->eid, &evaluator) != 0 || evaluator == nullptr)
        {
            continue;
        }

        double limits[2] = {};
        std::vector<ProjectedPoint2d> segment;
        if (UF_EVAL_ask_limits(evaluator, limits) == 0)
        {
            segment.reserve(kSamplesPerEdge + 1);
            for (int sample = 0; sample <= kSamplesPerEdge; ++sample)
            {
                const double parameter = limits[0] +
                    (limits[1] - limits[0]) *
                    static_cast<double>(sample) / static_cast<double>(kSamplesPerEdge);
                double point[3] = {};
                if (UF_EVAL_evaluate(evaluator, 0, parameter, point, nullptr) == 0)
                {
                    const NXOpen::Vector3d vector(point[0], point[1], point[2]);
                    segment.push_back(
                        { DotVector(vector, axisU), DotVector(vector, axisV) });
                }
            }
        }
        UF_EVAL_free(evaluator);

        if (segment.size() < 2)
        {
            continue;
        }
        if (!polygon.empty())
        {
            const auto squaredDistance = [](const ProjectedPoint2d& first, const ProjectedPoint2d& second)
            {
                const double dx = first.x - second.x;
                const double dy = first.y - second.y;
                return dx * dx + dy * dy;
            };
            if (squaredDistance(polygon.back(), segment.back()) <
                squaredDistance(polygon.back(), segment.front()))
            {
                std::reverse(segment.begin(), segment.end());
            }
            segment.erase(segment.begin());
        }
        polygon.insert(polygon.end(), segment.begin(), segment.end());
    }

    area = ProjectedPolygonArea(polygon);
    return polygon.size() >= 3 && area > 1.0e-9;
}

bool AskPlanarFaceData(NXOpen::Face* face, double point[3], double normal[3])
{
    if (face == nullptr)
    {
        return false;
    }

    int faceType = 0;
    double box[6] = {};
    double radius = 0.0;
    double radialData = 0.0;
    int normalDirection = 0;
    return UF_MODL_ask_face_data(
        face->Tag(),
        &faceType,
        point,
        normal,
        box,
        &radius,
        &radialData,
        &normalDirection) == 0 &&
        faceType == UF_MODL_PLANAR_FACE &&
        NormalizeVector(normal);
}

bool AskFaceArea(NXOpen::Part* workPart, NXOpen::Face* face, double& area)
{
    area = 0.0;
    if (workPart == nullptr || face == nullptr)
    {
        return false;
    }

    NXOpen::MeasureFaces* measurement = nullptr;
    try
    {
        NXOpen::Unit* areaUnit = workPart->UnitCollection()->GetBase("Area");
        NXOpen::Unit* lengthUnit = workPart->UnitCollection()->GetBase("Length");
        std::vector<NXOpen::IParameterizedSurface*> faces(1, face);
        measurement = workPart->MeasureManager()->NewFaceProperties(
            areaUnit,
            lengthUnit,
            0.99,
            faces);
        area = measurement->Area();
        delete measurement;
        return area > 1.0e-6;
    }
    catch (...)
    {
        delete measurement;
        return false;
    }
}

std::vector<ProjectedPoint2d> AskProjectedFaceHull(
    NXOpen::Face* face,
    const double origin[3],
    const double axisU[3],
    const double axisV[3])
{
    std::vector<ProjectedPoint2d> points;
    if (face == nullptr)
    {
        return points;
    }

    for (NXOpen::Edge* edge : face->GetEdges())
    {
        if (edge == nullptr)
        {
            continue;
        }
        NXOpen::Point3d vertices[2];
        edge->GetVertices(&vertices[0], &vertices[1]);
        for (const NXOpen::Point3d& vertex : vertices)
        {
            const double offset[3] =
            {
                vertex.X - origin[0],
                vertex.Y - origin[1],
                vertex.Z - origin[2]
            };
            points.push_back({ DotVector(offset, axisU), DotVector(offset, axisV) });
        }
    }

    std::sort(
        points.begin(),
        points.end(),
        [](const ProjectedPoint2d& first, const ProjectedPoint2d& second)
        {
            return std::abs(first.x - second.x) > 1.0e-7 ?
                first.x < second.x : first.y < second.y;
        });
    points.erase(
        std::unique(
            points.begin(),
            points.end(),
            [](const ProjectedPoint2d& first, const ProjectedPoint2d& second)
            {
                return std::abs(first.x - second.x) <= 1.0e-7 &&
                    std::abs(first.y - second.y) <= 1.0e-7;
            }),
        points.end());
    if (points.size() < 3)
    {
        return {};
    }

    std::vector<ProjectedPoint2d> hull;
    for (const ProjectedPoint2d& point : points)
    {
        while (hull.size() >= 2 &&
               Cross2d(hull[hull.size() - 2], hull.back(), point) <= 1.0e-9)
        {
            hull.pop_back();
        }
        hull.push_back(point);
    }
    const size_t lowerSize = hull.size();
    for (auto iterator = points.rbegin(); iterator != points.rend(); ++iterator)
    {
        while (hull.size() > lowerSize &&
               Cross2d(hull[hull.size() - 2], hull.back(), *iterator) <= 1.0e-9)
        {
            hull.pop_back();
        }
        hull.push_back(*iterator);
    }
    hull.pop_back();
    return hull;
}

ProjectedPoint2d IntersectProjectedLines(
    const ProjectedPoint2d& segmentStart,
    const ProjectedPoint2d& segmentEnd,
    const ProjectedPoint2d& clipStart,
    const ProjectedPoint2d& clipEnd)
{
    const double segmentX = segmentEnd.x - segmentStart.x;
    const double segmentY = segmentEnd.y - segmentStart.y;
    const double clipX = clipEnd.x - clipStart.x;
    const double clipY = clipEnd.y - clipStart.y;
    const double denominator = segmentX * clipY - segmentY * clipX;
    if (std::abs(denominator) <= 1.0e-12)
    {
        return segmentEnd;
    }

    const double deltaX = clipStart.x - segmentStart.x;
    const double deltaY = clipStart.y - segmentStart.y;
    const double ratio = (deltaX * clipY - deltaY * clipX) / denominator;
    return { segmentStart.x + segmentX * ratio, segmentStart.y + segmentY * ratio };
}

double AskProjectedOverlapArea(
    const std::vector<ProjectedPoint2d>& subject,
    const std::vector<ProjectedPoint2d>& clip)
{
    if (subject.size() < 3 || clip.size() < 3)
    {
        return 0.0;
    }

    std::vector<ProjectedPoint2d> output = subject;
    for (size_t clipIndex = 0; clipIndex < clip.size() && !output.empty(); ++clipIndex)
    {
        const ProjectedPoint2d& clipStart = clip[clipIndex];
        const ProjectedPoint2d& clipEnd = clip[(clipIndex + 1) % clip.size()];
        const std::vector<ProjectedPoint2d> input = output;
        output.clear();

        ProjectedPoint2d segmentStart = input.back();
        bool startInside = Cross2d(clipStart, clipEnd, segmentStart) >= -1.0e-9;
        for (const ProjectedPoint2d& segmentEnd : input)
        {
            const bool endInside = Cross2d(clipStart, clipEnd, segmentEnd) >= -1.0e-9;
            if (endInside)
            {
                if (!startInside)
                {
                    output.push_back(IntersectProjectedLines(
                        segmentStart,
                        segmentEnd,
                        clipStart,
                        clipEnd));
                }
                output.push_back(segmentEnd);
            }
            else if (startInside)
            {
                output.push_back(IntersectProjectedLines(
                    segmentStart,
                    segmentEnd,
                    clipStart,
                    clipEnd));
            }
            segmentStart = segmentEnd;
            startInside = endInside;
        }
    }
    return ProjectedPolygonArea(output);
}

bool InferSheetThicknessFromLargestPlanarFace(
    NXOpen::Part* workPart,
    const std::vector<NXOpen::Face*>& bodyFaces,
    double& thickness)
{
    thickness = 0.0;
    NXOpen::Face* largestPlanarFace = nullptr;
    double largestArea = 0.0;
    for (NXOpen::Face* face : bodyFaces)
    {
        double point[3] = {};
        double normal[3] = {};
        double area = 0.0;
        if (AskPlanarFaceData(face, point, normal) &&
            AskFaceArea(workPart, face, area) &&
            area > largestArea)
        {
            largestPlanarFace = face;
            largestArea = area;
        }
    }
    if (largestPlanarFace == nullptr)
    {
        WriteDebugLog("thickness_geometry_result available=0 reason=no_planar_face");
        return false;
    }

    double basePoint[3] = {};
    double baseNormal[3] = {};
    if (!AskPlanarFaceData(largestPlanarFace, basePoint, baseNormal))
    {
        return false;
    }
    double reference[3] = { 0.0, 0.0, 1.0 };
    if (std::abs(baseNormal[2]) > 0.9)
    {
        reference[1] = 1.0;
        reference[2] = 0.0;
    }
    double axisU[3] =
    {
        reference[1] * baseNormal[2] - reference[2] * baseNormal[1],
        reference[2] * baseNormal[0] - reference[0] * baseNormal[2],
        reference[0] * baseNormal[1] - reference[1] * baseNormal[0]
    };
    if (!NormalizeVector(axisU))
    {
        return false;
    }
    const double axisV[3] =
    {
        baseNormal[1] * axisU[2] - baseNormal[2] * axisU[1],
        baseNormal[2] * axisU[0] - baseNormal[0] * axisU[2],
        baseNormal[0] * axisU[1] - baseNormal[1] * axisU[0]
    };
    const std::vector<ProjectedPoint2d> baseHull = AskProjectedFaceHull(
        largestPlanarFace,
        basePoint,
        axisU,
        axisV);
    const double baseProjectedArea = ProjectedPolygonArea(baseHull);
    if (baseProjectedArea <= 1.0e-6)
    {
        return false;
    }

    double minimumDistance = std::numeric_limits<double>::max();
    tag_t matchedFaceTag = NULL_TAG;
    double matchedAreaRatio = 0.0;
    double matchedOverlapRatio = 0.0;
    size_t qualifiedCount = 0;
    for (NXOpen::Face* face : bodyFaces)
    {
        if (face == nullptr || face == largestPlanarFace)
        {
            continue;
        }

        double candidatePoint[3] = {};
        double candidateNormal[3] = {};
        if (!AskPlanarFaceData(face, candidatePoint, candidateNormal) ||
            std::abs(DotVector(baseNormal, candidateNormal)) < 0.999)
        {
            continue;
        }

        double candidateArea = 0.0;
        if (!AskFaceArea(workPart, face, candidateArea))
        {
            continue;
        }
        const double areaRatio = candidateArea / largestArea;
        if (areaRatio <= 0.60 + 1.0e-9)
        {
            continue;
        }

        const std::vector<ProjectedPoint2d> candidateHull = AskProjectedFaceHull(
            face,
            basePoint,
            axisU,
            axisV);
        const double overlapRatio =
            AskProjectedOverlapArea(baseHull, candidateHull) / baseProjectedArea;
        if (overlapRatio <= 0.60 + 1.0e-9)
        {
            continue;
        }

        const double offset[3] =
        {
            candidatePoint[0] - basePoint[0],
            candidatePoint[1] - basePoint[1],
            candidatePoint[2] - basePoint[2]
        };
        const double distance = std::abs(DotVector(offset, baseNormal));
        if (distance <= 0.01)
        {
            continue;
        }

        ++qualifiedCount;
        WriteDebugLog(
            "thickness_geometry_candidate base_tag=" +
            std::to_string(largestPlanarFace->Tag()) +
            " candidate_tag=" + std::to_string(face->Tag()) +
            " area_ratio=" + FormatDouble(areaRatio) +
            " overlap_ratio=" + FormatDouble(overlapRatio) +
            " distance=" + FormatDouble(distance));
        if (distance < minimumDistance)
        {
            minimumDistance = distance;
            matchedFaceTag = face->Tag();
            matchedAreaRatio = areaRatio;
            matchedOverlapRatio = overlapRatio;
        }
    }

    if (minimumDistance == std::numeric_limits<double>::max())
    {
        WriteDebugLog(
            "thickness_geometry_result available=0 base_tag=" +
            std::to_string(largestPlanarFace->Tag()) +
            " base_area=" + FormatDouble(largestArea) +
            " qualified_count=0");
        return false;
    }

    thickness = minimumDistance;
    WriteDebugLog(
        "thickness_geometry_result available=1 base_tag=" +
        std::to_string(largestPlanarFace->Tag()) +
        " base_area=" + FormatDouble(largestArea) +
        " matched_tag=" + std::to_string(matchedFaceTag) +
        " matched_area_ratio=" + FormatDouble(matchedAreaRatio) +
        " matched_overlap_ratio=" + FormatDouble(matchedOverlapRatio) +
        " qualified_count=" + std::to_string(qualifiedCount) +
        " thickness=" + FormatDouble(thickness));
    return true;
}

std::vector<NXOpen::Face*> FindBlendFacesByRadius(
    NXOpen::Part* workPart,
    const std::vector<NXOpen::Face*>& candidateFaces,
    double maxRadius,
    double knownSheetThickness = 0.0,
    bool hasKnownSheetThickness = false)
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

    double sheetThickness = knownSheetThickness;
    bool hasSheetThickness = hasKnownSheetThickness && sheetThickness > 1.0e-9;
    std::string thicknessSource = hasSheetThickness ? "pre_cut_largest_planar_overlap" : "unavailable";
    if (!hasSheetThickness)
    {
        hasSheetThickness = InferSheetThicknessFromLargestPlanarFace(
            workPart,
            candidateFaces,
            sheetThickness);
        if (hasSheetThickness)
        {
            thicknessSource = "largest_planar_overlap";
        }
    }
    WriteDebugLog(
        "blend_scan_thickness available=" + std::string(hasSheetThickness ? "1" : "0") +
        " thickness=" + FormatDouble(sheetThickness) +
        " source=" + thicknessSource);

    std::vector<NXOpen::Face*> blendFaces;
    std::vector<tag_t> qualifyingFaceTags;
    size_t recognizedBlendCount = 0;
    for (size_t index = 0; index < candidateFaces.size() && index < isBlendFace.size(); ++index)
    {
        if (!isBlendFace[index] || candidateFaces[index] == nullptr)
        {
            continue;
        }

        NXOpen::Face* candidateFace = candidateFaces[index];
        const double faceRadius = std::abs(resizeBuilder->GetBlendFaceRadius(candidateFace));
        const bool radiusQualifies = faceRadius <= maxRadius + 1.0e-6;
        bool isCylinder = false;
        bool heightEqualsThickness = false;
        bool angularSpanExcluded = false;
        double cylinderHeight = 0.0;
        double angularSpan = 0.0;
        double axisPoint[3] = {};
        double axisDirection[3] = {};
        double cylinderRadius = 0.0;
        if (AskCylinderFaceGeometry(
                candidateFace,
                axisPoint,
                axisDirection,
                cylinderRadius))
        {
            isCylinder = true;
            const bool hasHeight = AskCylinderFaceHeight(
                candidateFace,
                axisPoint,
                axisDirection,
                cylinderHeight);
            const double heightTolerance = std::max(0.02, sheetThickness * 0.01);
            heightEqualsThickness =
                hasSheetThickness && hasHeight &&
                std::abs(cylinderHeight - sheetThickness) <= heightTolerance;

            if (AskCylinderFaceAngularSpan(candidateFace, angularSpan))
            {
                const double pi = std::acos(-1.0);
                angularSpanExcluded = angularSpan >= pi - 1.0e-6;
            }
        }

        const bool qualifies =
            radiusQualifies && !heightEqualsThickness && !angularSpanExcluded;
        ++recognizedBlendCount;
        WriteDebugLog(
            "blend_scan_face tag=" + std::to_string(candidateFace->Tag()) +
            " radius=" + FormatDouble(faceRadius) +
            " radius_qualifies=" + (radiusQualifies ? "1" : "0") +
            " cylinder=" + (isCylinder ? "1" : "0") +
            " cylinder_height=" + FormatDouble(cylinderHeight) +
            " sheet_thickness=" + FormatDouble(sheetThickness) +
            " excluded_height_equals_thickness=" + (heightEqualsThickness ? "1" : "0") +
            " angular_span_deg=" + FormatDouble(angularSpan * 180.0 / std::acos(-1.0)) +
            " excluded_angular_span=" + (angularSpanExcluded ? "1" : "0") +
            " qualifies=" + (qualifies ? "1" : "0"));
        if (qualifies)
        {
            blendFaces.push_back(candidateFace);
            qualifyingFaceTags.push_back(candidateFace->Tag());
        }
    }

    resizeBuilder->Destroy();
    WriteDebugLog(
        "blend_scan_summary total_faces=" + std::to_string(candidateFaces.size()) +
        " recognized_blends=" + std::to_string(recognizedBlendCount) +
        " max_radius=" + FormatDouble(maxRadius) +
        " qualifying_blends=" + std::to_string(blendFaces.size()) +
        " qualifying_tags=" + FormatTagList(qualifyingFaceTags));
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

NXOpen::Face* ResolveFaceByTag(tag_t faceTag)
{
    if (faceTag == NULL_TAG)
    {
        return nullptr;
    }

    int type = 0;
    int subtype = 0;
    if (UF_OBJ_ask_type_and_subtype(faceTag, &type, &subtype) != 0 ||
        type != UF_solid_type || subtype != UF_solid_face_subtype)
    {
        return nullptr;
    }

    return dynamic_cast<NXOpen::Face*>(NXOpen::NXObjectManager::Get(faceTag));
}

std::vector<tag_t> FindTangentPlanarFaceTags(NXOpen::Face* blendFace)
{
    std::vector<tag_t> planarFaceTags;
    if (blendFace == nullptr)
    {
        return planarFaceTags;
    }

    std::unordered_set<tag_t> seenFaceTags;
    for (tag_t edgeTag : AskFaceEdges(blendFace->Tag()))
    {
        logical isSmooth = false;
        if (UF_MODL_ask_edge_smoothness(edgeTag, 0.0, &isSmooth) != 0 || !isSmooth)
        {
            continue;
        }

        for (tag_t adjacentFaceTag : AskEdgeFaces(edgeTag))
        {
            if (adjacentFaceTag == NULL_TAG || adjacentFaceTag == blendFace->Tag() ||
                !seenFaceTags.insert(adjacentFaceTag).second || !IsPlanarFace(adjacentFaceTag))
            {
                continue;
            }

            planarFaceTags.push_back(adjacentFaceTag);
        }
    }

    return planarFaceTags;
}

bool TryReplaceFaces(
    NXOpen::Part* workPart,
    const std::vector<NXOpen::Face*>& facesToReplace,
    NXOpen::Face* replacementFace,
    bool reverseDirection)
{
    if (workPart == nullptr || facesToReplace.empty() || replacementFace == nullptr)
    {
        return false;
    }

    NXOpen::Features::ReplaceFaceBuilder* builder =
        workPart->Features()->CreateReplaceFaceBuilder(nullptr);
    if (builder == nullptr)
    {
        return false;
    }

    try
    {
        builder->SetType(NXOpen::Features::ReplaceFaceBuilder::ReplaceTypesReplace);
        builder->OffsetDistance()->SetFormula("0");
        builder->ResetReplaceFaceMethod();
        builder->ResetFreeEdgeProjectionOption();
        builder->SetReverseDirection(reverseDirection);

        NXOpen::SelectionIntentRuleOptions* replaceOptions =
            workPart->ScRuleFactory()->CreateRuleOptions();
        replaceOptions->SetSelectedFromInactive(false);
        NXOpen::FaceDumbRule* replaceRule =
            workPart->ScRuleFactory()->CreateRuleFaceDumb(facesToReplace, replaceOptions);
        std::vector<NXOpen::SelectionIntentRule*> replaceRules(1, replaceRule);
        builder->FaceToReplace()->ReplaceRules(replaceRules, false);
        delete replaceOptions;

        NXOpen::SelectionIntentRuleOptions* replacementOptions =
            workPart->ScRuleFactory()->CreateRuleOptions();
        replacementOptions->SetSelectedFromInactive(false);
        std::vector<NXOpen::Face*> replacementFaces(1, replacementFace);
        NXOpen::FaceDumbRule* replacementRule =
            workPart->ScRuleFactory()->CreateRuleFaceDumb(replacementFaces, replacementOptions);
        std::vector<NXOpen::SelectionIntentRule*> replacementRules(1, replacementRule);
        builder->ReplacementFaces()->ReplaceRules(replacementRules, false);
        delete replacementOptions;

        builder->OnApplyPre();
        NXOpen::NXObject* result = builder->Commit();
        builder->Destroy();
        return result != nullptr;
    }
    catch (...)
    {
        builder->Destroy();
        return false;
    }
}

bool TryReplaceBlendWithTangentPlane(
    NXOpen::Part* workPart,
    tag_t blendFaceTag,
    const std::vector<tag_t>& tangentPlaneTags)
{
    NXOpen::Session* session = NXOpen::Session::GetSession();
    if (workPart == nullptr || session == nullptr)
    {
        return false;
    }

    WriteDebugLog(
        "replace_failed_blend_start blend_tag=" + std::to_string(blendFaceTag) +
        " tangent_plane_count=" + std::to_string(tangentPlaneTags.size()) +
        " tangent_plane_tags=" + FormatTagList(tangentPlaneTags));

    for (tag_t planeTag : tangentPlaneTags)
    {
        for (bool reverseDirection : { false, true })
        {
            NXOpen::Face* blendFace = ResolveFaceByTag(blendFaceTag);
            NXOpen::Face* tangentPlane = ResolveFaceByTag(planeTag);
            if (blendFace == nullptr || tangentPlane == nullptr)
            {
                WriteDebugLog(
                    "replace_failed_blend_skip blend_tag=" + std::to_string(blendFaceTag) +
                    " plane_tag=" + std::to_string(planeTag) +
                    " reason=face_not_resolved");
                continue;
            }

            const NXOpen::Session::UndoMarkId markId = session->SetUndoMark(
                NXOpen::Session::MarkVisibilityInvisible,
                "BaoRongQieChuTryReplaceBlend");
            std::vector<NXOpen::Face*> facesToReplace(1, blendFace);
            const bool replaced = TryReplaceFaces(
                workPart,
                facesToReplace,
                tangentPlane,
                reverseDirection);
            if (replaced)
            {
                session->DeleteUndoMark(markId, "BaoRongQieChuTryReplaceBlend");
                WriteDebugLog(
                    "replace_failed_blend_result blend_tag=" + std::to_string(blendFaceTag) +
                    " plane_tag=" + std::to_string(planeTag) +
                    " reverse=" + (reverseDirection ? "1" : "0") +
                    " success=1");
                return true;
            }

            session->UndoToMark(markId, "BaoRongQieChuTryReplaceBlend");
            session->DeleteUndoMark(markId, "BaoRongQieChuTryReplaceBlend");
            WriteDebugLog(
                "replace_failed_blend_result blend_tag=" + std::to_string(blendFaceTag) +
                " plane_tag=" + std::to_string(planeTag) +
                " reverse=" + (reverseDirection ? "1" : "0") +
                " success=0");
        }
    }

    WriteDebugLog(
        "replace_failed_blend_end blend_tag=" + std::to_string(blendFaceTag) +
        " success=0");
    return false;
}

std::vector<tag_t> AskPeripheralLoopEdges(tag_t planarFaceTag)
{
    std::vector<tag_t> edges;
    uf_loop_p_t loopList = nullptr;
    if (UF_MODL_ask_face_loops(planarFaceTag, &loopList) != 0 || loopList == nullptr)
    {
        return edges;
    }

    std::unordered_set<tag_t> seenEdges;
    for (uf_loop_p_t loop = loopList; loop != nullptr; loop = loop->next)
    {
        if (loop->type != 1)
        {
            continue;
        }

        for (uf_list_p_t node = loop->edge_list; node != nullptr; node = node->next)
        {
            if (node->eid != NULL_TAG && seenEdges.insert(node->eid).second)
            {
                edges.push_back(node->eid);
            }
        }
    }

    UF_MODL_delete_loop_list(&loopList);
    return edges;
}

struct PlanarBlendGroup
{
    tag_t planarFaceTag;
    std::vector<tag_t> blendFaceTags;
};

bool TryReplaceBlendGroupWithPlane(
    NXOpen::Part* workPart,
    tag_t planarFaceTag,
    const std::vector<tag_t>& blendFaceTags)
{
    NXOpen::Session* session = NXOpen::Session::GetSession();
    if (workPart == nullptr || session == nullptr || blendFaceTags.empty())
    {
        return false;
    }

    for (bool reverseDirection : { false, true })
    {
        NXOpen::Face* planarFace = ResolveFaceByTag(planarFaceTag);
        std::vector<NXOpen::Face*> blendFaces;
        blendFaces.reserve(blendFaceTags.size());
        for (tag_t blendFaceTag : blendFaceTags)
        {
            NXOpen::Face* blendFace = ResolveFaceByTag(blendFaceTag);
            if (blendFace != nullptr)
            {
                blendFaces.push_back(blendFace);
            }
        }
        if (planarFace == nullptr || blendFaces.size() != blendFaceTags.size())
        {
            WriteDebugLog(
                "outer_plane_replace_skip plane_tag=" + std::to_string(planarFaceTag) +
                " requested_count=" + std::to_string(blendFaceTags.size()) +
                " resolved_count=" + std::to_string(blendFaces.size()) +
                " reason=face_not_resolved");
            return false;
        }

        const NXOpen::Session::UndoMarkId markId = session->SetUndoMark(
            NXOpen::Session::MarkVisibilityInvisible,
            "BaoRongQieChuReplaceOuterPlaneBlends");
        const bool replaced = TryReplaceFaces(
            workPart,
            blendFaces,
            planarFace,
            reverseDirection);
        if (replaced)
        {
            session->DeleteUndoMark(markId, "BaoRongQieChuReplaceOuterPlaneBlends");
            WriteDebugLog(
                "outer_plane_replace_result plane_tag=" + std::to_string(planarFaceTag) +
                " blend_count=" + std::to_string(blendFaceTags.size()) +
                " blend_tags=" + FormatTagList(blendFaceTags) +
                " reverse=" + (reverseDirection ? "1" : "0") +
                " success=1");
            return true;
        }

        session->UndoToMark(markId, "BaoRongQieChuReplaceOuterPlaneBlends");
        session->DeleteUndoMark(markId, "BaoRongQieChuReplaceOuterPlaneBlends");
        WriteDebugLog(
            "outer_plane_replace_result plane_tag=" + std::to_string(planarFaceTag) +
            " blend_count=" + std::to_string(blendFaceTags.size()) +
            " blend_tags=" + FormatTagList(blendFaceTags) +
            " reverse=" + (reverseDirection ? "1" : "0") +
            " success=0");
    }

    return false;
}

void ReplaceQualifyingBlendsFromOuterPlanes(
    NXOpen::Part* workPart,
    NXOpen::Body* targetBody,
    double maxRadius,
    double sheetThickness,
    bool hasSheetThickness)
{
    if (workPart == nullptr || targetBody == nullptr)
    {
        return;
    }

    const std::vector<NXOpen::Face*> bodyFaces = CollectFacesFromBody(targetBody);
    const std::vector<NXOpen::Face*> qualifyingBlendFaces =
        FindBlendFacesByRadius(
            workPart,
            bodyFaces,
            maxRadius,
            sheetThickness,
            hasSheetThickness);

    std::unordered_set<tag_t> qualifyingBlendTags;
    for (NXOpen::Face* blendFace : qualifyingBlendFaces)
    {
        if (blendFace != nullptr)
        {
            qualifyingBlendTags.insert(blendFace->Tag());
        }
    }

    std::map<tag_t, std::unordered_set<tag_t>> blendsByPlanarFace;
    for (NXOpen::Face* face : bodyFaces)
    {
        if (face == nullptr || !IsPlanarFace(face->Tag()))
        {
            continue;
        }

        for (tag_t edgeTag : AskPeripheralLoopEdges(face->Tag()))
        {
            logical isSmooth = false;
            if (UF_MODL_ask_edge_smoothness(edgeTag, 0.0, &isSmooth) != 0 || !isSmooth)
            {
                continue;
            }

            for (tag_t adjacentFaceTag : AskEdgeFaces(edgeTag))
            {
                if (qualifyingBlendTags.find(adjacentFaceTag) != qualifyingBlendTags.end())
                {
                    blendsByPlanarFace[face->Tag()].insert(adjacentFaceTag);
                }
            }
        }
    }

    std::vector<PlanarBlendGroup> groups;
    groups.reserve(blendsByPlanarFace.size());
    for (const auto& entry : blendsByPlanarFace)
    {
        PlanarBlendGroup group;
        group.planarFaceTag = entry.first;
        group.blendFaceTags.assign(entry.second.begin(), entry.second.end());
        std::sort(group.blendFaceTags.begin(), group.blendFaceTags.end());
        groups.push_back(group);
    }
    std::sort(
        groups.begin(),
        groups.end(),
        [](const PlanarBlendGroup& first, const PlanarBlendGroup& second)
        {
            if (first.blendFaceTags.size() != second.blendFaceTags.size())
            {
                return first.blendFaceTags.size() > second.blendFaceTags.size();
            }
            return first.planarFaceTag < second.planarFaceTag;
        });

    WriteDebugLog(
        "outer_plane_replace_scan qualifying_blend_count=" +
        std::to_string(qualifyingBlendTags.size()) +
        " planar_group_count=" + std::to_string(groups.size()) +
        " max_radius=" + FormatDouble(maxRadius));
    for (size_t index = 0; index < groups.size(); ++index)
    {
        WriteDebugLog(
            "outer_plane_replace_group order=" + std::to_string(index + 1) +
            " plane_tag=" + std::to_string(groups[index].planarFaceTag) +
            " blend_count=" + std::to_string(groups[index].blendFaceTags.size()) +
            " blend_tags=" + FormatTagList(groups[index].blendFaceTags));
    }

    std::unordered_set<tag_t> replacedBlendTags;
    size_t successfulGroupCount = 0;
    for (const PlanarBlendGroup& group : groups)
    {
        std::vector<tag_t> pendingBlendTags;
        for (tag_t blendFaceTag : group.blendFaceTags)
        {
            if (replacedBlendTags.find(blendFaceTag) == replacedBlendTags.end() &&
                ResolveFaceByTag(blendFaceTag) != nullptr)
            {
                pendingBlendTags.push_back(blendFaceTag);
            }
        }
        if (pendingBlendTags.empty())
        {
            continue;
        }

        if (TryReplaceBlendGroupWithPlane(
                workPart,
                group.planarFaceTag,
                pendingBlendTags))
        {
            ++successfulGroupCount;
            replacedBlendTags.insert(pendingBlendTags.begin(), pendingBlendTags.end());
        }
    }

    std::vector<NXOpen::Face*> failedBlendFaces;
    std::vector<tag_t> failedBlendTags;
    for (tag_t blendFaceTag : qualifyingBlendTags)
    {
        if (replacedBlendTags.find(blendFaceTag) != replacedBlendTags.end())
        {
            continue;
        }

        failedBlendTags.push_back(blendFaceTag);
        NXOpen::Face* failedFace = ResolveFaceByTag(blendFaceTag);
        if (failedFace != nullptr)
        {
            failedBlendFaces.push_back(failedFace);
        }
    }
    std::sort(failedBlendTags.begin(), failedBlendTags.end());
    ColorFaces(NXOpen::Session::GetSession(), failedBlendFaces, kRedColor);

    WriteDebugLog(
        "outer_plane_replace_summary qualifying_blend_count=" +
        std::to_string(qualifyingBlendTags.size()) +
        " replaced_blend_count=" + std::to_string(replacedBlendTags.size()) +
        " successful_group_count=" + std::to_string(successfulGroupCount) +
        " unreplaced_blend_count=" +
        std::to_string(qualifyingBlendTags.size() - replacedBlendTags.size()) +
        " red_failure_count=" + std::to_string(failedBlendFaces.size()) +
        " failure_tags=" + FormatTagList(failedBlendTags));
}

bool CreateConnectedFaceFeature(NXOpen::Part* workPart, NXOpen::Body* body)
{
    if (workPart == nullptr || body == nullptr)
    {
        return false;
    }

    tag_t resultFeatureTag = NULL_TAG;
    tag_t faceTags[2] = { NULL_TAG, NULL_TAG };
    const int errorCode = UF_MODL_edit_face_join(
        1,
        body->Tag(),
        faceTags,
        &resultFeatureTag);
    if (errorCode != 0 || resultFeatureTag == NULL_TAG)
    {
        WriteDebugLog(
            "connected_face_commit body_tag=" + std::to_string(body->Tag()) +
            " api=UF_MODL_edit_face_join error_code=" + std::to_string(errorCode) +
            " feature_tag=" + std::to_string(resultFeatureTag) +
            " success=0");
        return false;
    }

    WriteDebugLog(
        "connected_face_commit body_tag=" + std::to_string(body->Tag()) +
        " api=UF_MODL_edit_face_join error_code=0 feature_tag=" +
        std::to_string(resultFeatureTag) + " success=1");
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

    NXOpen::Session* session = NXOpen::Session::GetSession();
    if (session == nullptr)
    {
        return;
    }

    std::vector<tag_t> candidateFaceTags;
    candidateFaceTags.reserve(candidateFaces.size());
    for (NXOpen::Face* face : candidateFaces)
    {
        if (face != nullptr)
        {
            candidateFaceTags.push_back(face->Tag());
        }
    }

    WriteDebugLog(
        "blend_delete_start qualifying_count=" + std::to_string(candidateFaceTags.size()) +
        " max_radius=" + FormatDouble(maxRadius) +
        " candidate_tags=" + FormatTagList(candidateFaceTags));

    // Phase 1: test every blend face independently against the unchanged body.
    // Each trial is rolled back immediately so an earlier deletion cannot alter
    // the topology seen by a later trial.
    std::vector<tag_t> successfullyDeletedFaceTags;
    std::vector<tag_t> failedFaceTags;
    std::map<tag_t, std::vector<tag_t>> tangentPlanesByFailedFace;
    for (tag_t faceTag : candidateFaceTags)
    {
        NXOpen::Face* blendFace = ResolveFaceByTag(faceTag);
        if (blendFace == nullptr)
        {
            failedFaceTags.push_back(faceTag);
            WriteDebugLog(
                "blend_delete_trial tag=" + std::to_string(faceTag) +
                " success=0 reason=face_not_resolved tangent_plane_count=0");
            continue;
        }

        const std::vector<tag_t> tangentPlaneTags = FindTangentPlanarFaceTags(blendFace);
        const NXOpen::Session::UndoMarkId trialMarkId = session->SetUndoMark(
            NXOpen::Session::MarkVisibilityInvisible,
            "BaoRongQieChuTryDeleteBlend");

        std::vector<NXOpen::Face*> singleFace(1, blendFace);
        const bool deleted = TryDeleteBlendFaces(workPart, singleFace, maxRadius);

        // The trial feature must never remain in the part. Tags recorded above
        // resolve to the restored original faces after this undo.
        session->UndoToMark(trialMarkId, "BaoRongQieChuTryDeleteBlend");
        session->DeleteUndoMark(trialMarkId, "BaoRongQieChuTryDeleteBlend");

        if (deleted)
        {
            successfullyDeletedFaceTags.push_back(faceTag);
        }
        else
        {
            failedFaceTags.push_back(faceTag);
            tangentPlanesByFailedFace[faceTag] = tangentPlaneTags;
        }

        WriteDebugLog(
            "blend_delete_trial tag=" + std::to_string(faceTag) +
            " success=" + (deleted ? "1" : "0") +
            " tangent_plane_count=" + std::to_string(tangentPlaneTags.size()) +
            " tangent_plane_tags=" + FormatTagList(tangentPlaneTags));
    }


    WriteDebugLog(
        "blend_delete_trial_summary qualifying_count=" + std::to_string(candidateFaceTags.size()) +
        " trial_success_count=" + std::to_string(successfullyDeletedFaceTags.size()) +
        " trial_failure_count=" + std::to_string(failedFaceTags.size()) +
        " success_tags=" + FormatTagList(successfullyDeletedFaceTags) +
        " failure_tags=" + FormatTagList(failedFaceTags));

    // Phase 2: commit the faces that passed the single-face test as one blend
    // deletion feature.
    std::vector<NXOpen::Face*> successfullyDeletedFaces;
    successfullyDeletedFaces.reserve(successfullyDeletedFaceTags.size());
    for (tag_t faceTag : successfullyDeletedFaceTags)
    {
        NXOpen::Face* face = ResolveFaceByTag(faceTag);
        if (face != nullptr)
        {
            successfullyDeletedFaces.push_back(face);
        }
    }

    bool committedSuccessfulFaces = successfullyDeletedFaces.empty();
    if (!successfullyDeletedFaces.empty())
    {
        committedSuccessfulFaces = TryDeleteBlendFaces(workPart, successfullyDeletedFaces, maxRadius);
        WriteDebugLog(
            "blend_delete_commit_success_set requested_count=" +
            std::to_string(successfullyDeletedFaces.size()) +
            " committed=" + (committedSuccessfulFaces ? "1" : "0") +
            " tags=" + FormatTagList(successfullyDeletedFaceTags));
    }
    if (!committedSuccessfulFaces)
    {
        throw std::runtime_error("Failed to commit the blend faces that passed the single-face deletion test.");
    }

    // Phase 3: a failed blend is replaced by one of its smooth adjacent planar
    // support faces. Try both support planes and both replacement directions.
    size_t replacedFailedFaceCount = 0;
    for (tag_t failedFaceTag : failedFaceTags)
    {
        const auto tangentPlanes = tangentPlanesByFailedFace.find(failedFaceTag);
        if (tangentPlanes == tangentPlanesByFailedFace.end())
        {
            continue;
        }

        const bool replaced = TryReplaceBlendWithTangentPlane(
            workPart,
            failedFaceTag,
            tangentPlanes->second);
        if (replaced)
        {
            ++replacedFailedFaceCount;
        }
    }

    WriteDebugLog(
        "blend_delete_final_summary qualifying_count=" + std::to_string(candidateFaceTags.size()) +
        " deleted_count=" + std::to_string(successfullyDeletedFaces.size()) +
        " delete_failed_count=" + std::to_string(failedFaceTags.size()) +
        " replaced_failed_count=" + std::to_string(replacedFailedFaceCount) +
        " unresolved_failed_count=" +
            std::to_string(failedFaceTags.size() - replacedFailedFaceCount));
}

std::vector<std::vector<tag_t>> PartitionFacesIntoConnectedRegions(
    const std::vector<tag_t>& faceTags)
{
    std::unordered_set<tag_t> candidates(faceTags.begin(), faceTags.end());
    std::unordered_set<tag_t> visited;
    std::vector<tag_t> orderedTags(candidates.begin(), candidates.end());
    std::sort(orderedTags.begin(), orderedTags.end());

    std::vector<std::vector<tag_t>> regions;
    for (tag_t seedFaceTag : orderedTags)
    {
        if (!visited.insert(seedFaceTag).second)
        {
            continue;
        }

        std::vector<tag_t> region;
        std::vector<tag_t> pending(1, seedFaceTag);
        while (!pending.empty())
        {
            const tag_t faceTag = pending.back();
            pending.pop_back();
            region.push_back(faceTag);

            for (tag_t edgeTag : AskFaceEdges(faceTag))
            {
                for (tag_t adjacentFaceTag : AskEdgeFaces(edgeTag))
                {
                    if (candidates.find(adjacentFaceTag) != candidates.end() &&
                        visited.insert(adjacentFaceTag).second)
                    {
                        pending.push_back(adjacentFaceTag);
                    }
                }
            }
        }

        std::sort(region.begin(), region.end());
        regions.push_back(region);
    }
    return regions;
}

bool DeleteFacesWithHeal(
    NXOpen::Part* workPart,
    const std::vector<NXOpen::Face*>& faces)
{
    if (workPart == nullptr || faces.empty())
    {
        return false;
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
        NXOpen::Features::Feature* feature = deleteFaceBuilder->CommitFeature();
        deleteFaceBuilder->Destroy();
        return feature != nullptr;
    }
    catch (...)
    {
        deleteFaceBuilder->Destroy();
        return false;
    }
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
    NXOpen::Features::BooleanFeature** outBooleanFeature = nullptr,
    NXOpen::Body** outTargetBody = nullptr,
    double* outSheetThickness = nullptr,
    bool* outHasSheetThickness = nullptr)
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

    NXOpen::Body* targetBody = dynamic_cast<NXOpen::Body*>(NXOpen::NXObjectManager::Get(ownerBodyTag));
    if (targetBody == nullptr)
    {
        throw std::runtime_error("Failed to get the owning body.");
    }

    double preCutSheetThickness = 0.0;
    bool hasPreCutSheetThickness = false;
    if (removeBlend)
    {
        hasPreCutSheetThickness = InferSheetThicknessFromLargestPlanarFace(
            workPart,
            CollectFacesFromBody(targetBody),
            preCutSheetThickness);
        WriteDebugLog(
            "pre_cut_thickness available=" +
            std::string(hasPreCutSheetThickness ? "1" : "0") +
            " thickness=" + FormatDouble(preCutSheetThickness));
    }

    if (outTargetBody != nullptr)
    {
        *outTargetBody = targetBody;
    }
    if (outSheetThickness != nullptr)
    {
        *outSheetThickness = preCutSheetThickness;
    }
    if (outHasSheetThickness != nullptr)
    {
        *outHasSheetThickness = hasPreCutSheetThickness;
    }

    NXOpen::Features::Feature* envelopeFeature = CreateEnvelopeBlock(workPart, minCorner, maxCorner, envelopeOffset);

    if (!enableBooleanSubtract)
    {
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
      pendingBlendReplacements_(),
      pendingCutFeatures_(),
      pendingConnectedFaceBodies_(),
      isInternalUpdate_(false)
{
    const std::string dlxPath = zhihui_embedded_dialog::ExtractDlxToRandomPath(IDR_ZH_DLX_BAORONGQIECHU_DLX);

    if (dlxPath.empty())

    {

        throw std::runtime_error("BaoRongQieChu dialog resource is missing.");

    }

    dialog_ = ui_->CreateDialog(dlxPath.c_str());
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

    LoadDialogMemory();
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
    SaveDialogMemory();
    return ExecuteFromSelection();
}

int BaoRongQieChuDialog::ok_cb()
{
    SaveDialogMemory();
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

    result = ExecutePendingBlendReplacement();
    if (result != 0)
    {
        return result;
    }

    result = ExecutePendingCutFaceRemoval();
    if (result != 0)
    {
        return result;
    }

    return ExecutePendingConnectedFaceCreation();
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
        const double envelopeOffset = GetOffsetValue();
        NXOpen::Features::BooleanFeature* booleanFeature = nullptr;
        NXOpen::Body* targetBody = nullptr;
        double sheetThickness = 0.0;
        bool hasSheetThickness = false;
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
            GetRemoveBlendEnabled(),
            GetBlendRadiusValue(),
            false,
            &booleanFeature,
            &targetBody,
            &sheetThickness,
            &hasSheetThickness);
        if (booleanFeature != nullptr)
        {
            ColorFaces(session_, booleanFeature->GetFaces(), kBlueColor);
        }
        if (GetHealRemovedRegionEnabled() && booleanFeature != nullptr)
        {
            RememberPendingCutFeature(booleanFeature);
        }
        if (GetRemoveBlendEnabled() && booleanFeature != nullptr && targetBody != nullptr)
        {
            RememberPendingBlendReplacement(
                targetBody,
                GetBlendRadiusValue(),
                sheetThickness,
                hasSheetThickness);
            RememberPendingConnectedFaceBody(targetBody);
            WriteDebugLog(
                "blend_replace_deferred_until_confirm body_tag=" +
                std::to_string(targetBody->Tag()) +
                " max_radius=" + FormatDouble(GetBlendRadiusValue()) +
                " sheet_thickness=" + FormatDouble(sheetThickness));
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

int BaoRongQieChuDialog::ExecutePendingBlendReplacement()
{
    if (!GetRemoveBlendEnabled())
    {
        pendingBlendReplacements_.clear();
        return 0;
    }

    if (pendingBlendReplacements_.empty())
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
        session_->SetUndoMark(NXOpen::Session::MarkVisibilityVisible, "BaoRongQieChuReplaceBlend");

    try
    {
        for (const PendingBlendReplacement& pending : pendingBlendReplacements_)
        {
            if (pending.targetBody == nullptr)
            {
                continue;
            }

            WriteDebugLog(
                "confirmed_outer_plane_replace_start body_tag=" +
                std::to_string(pending.targetBody->Tag()) +
                " max_radius=" + FormatDouble(pending.maxRadius) +
                " sheet_thickness=" + FormatDouble(pending.sheetThickness));
            ReplaceQualifyingBlendsFromOuterPlanes(
                workPart,
                pending.targetBody,
                pending.maxRadius,
                pending.sheetThickness,
                pending.hasSheetThickness);
            WriteDebugLog(
                "confirmed_outer_plane_replace_end body_tag=" +
                std::to_string(pending.targetBody->Tag()));
        }

        pendingBlendReplacements_.clear();
        session_->DeleteUndoMark(markId, "BaoRongQieChuReplaceBlend");
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

    session_->UndoToMark(markId, "BaoRongQieChuReplaceBlend");
    session_->DeleteUndoMark(markId, "BaoRongQieChuReplaceBlend");
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
        std::unordered_set<tag_t> seenFaceTags;
        size_t featureOrder = 0;
        size_t totalRegionCount = 0;
        size_t successfulRegionCount = 0;
        size_t failedRegionCount = 0;

        for (NXOpen::Features::BooleanFeature* feature : pendingCutFeatures_)
        {
            ++featureOrder;
            if (feature == nullptr)
            {
                continue;
            }

            std::vector<tag_t> featureFaceTags;
            const std::vector<NXOpen::Face*> cutFaces = feature->GetFaces();
            for (NXOpen::Face* face : cutFaces)
            {
                if (face == nullptr || !seenFaceTags.insert(face->Tag()).second)
                {
                    continue;
                }

                featureFaceTags.push_back(face->Tag());
            }

            const std::vector<std::vector<tag_t>> regions =
                PartitionFacesIntoConnectedRegions(featureFaceTags);
            WriteDebugLog(
                "cut_heal_feature feature_order=" + std::to_string(featureOrder) +
                " feature_tag=" + std::to_string(feature->Tag()) +
                " cut_face_count=" + std::to_string(featureFaceTags.size()) +
                " connected_region_count=" + std::to_string(regions.size()));

            for (size_t regionIndex = 0; regionIndex < regions.size(); ++regionIndex)
            {
                ++totalRegionCount;
                std::vector<NXOpen::Face*> regionFaces;
                std::vector<tag_t> resolvedTags;
                for (tag_t faceTag : regions[regionIndex])
                {
                    NXOpen::Face* face = ResolveFaceByTag(faceTag);
                    if (face != nullptr)
                    {
                        regionFaces.push_back(face);
                        resolvedTags.push_back(faceTag);
                    }
                }

                if (regionFaces.empty())
                {
                    ++failedRegionCount;
                    WriteDebugLog(
                        "cut_heal_region feature_order=" + std::to_string(featureOrder) +
                        " region_order=" + std::to_string(regionIndex + 1) +
                        " requested_face_count=" + std::to_string(regions[regionIndex].size()) +
                        " resolved_face_count=0 success=0 reason=faces_not_resolved");
                    continue;
                }

                ColorFaces(session_, regionFaces, kBlueColor);
                const bool healed = DeleteFacesWithHeal(workPart, regionFaces);
                if (healed)
                {
                    ++successfulRegionCount;
                }
                else
                {
                    ++failedRegionCount;
                }
                WriteDebugLog(
                    "cut_heal_region feature_order=" + std::to_string(featureOrder) +
                    " region_order=" + std::to_string(regionIndex + 1) +
                    " requested_face_count=" + std::to_string(regions[regionIndex].size()) +
                    " resolved_face_count=" + std::to_string(regionFaces.size()) +
                    " resolved_tags=" + FormatTagList(resolvedTags) +
                    " success=" + (healed ? "1" : "0"));
            }
        }

        WriteDebugLog(
            "cut_heal_summary feature_count=" + std::to_string(pendingCutFeatures_.size()) +
            " region_count=" + std::to_string(totalRegionCount) +
            " success_count=" + std::to_string(successfulRegionCount) +
            " failure_count=" + std::to_string(failedRegionCount));

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

int BaoRongQieChuDialog::ExecutePendingConnectedFaceCreation()
{
    if (!GetRemoveBlendEnabled())
    {
        pendingConnectedFaceBodies_.clear();
        return 0;
    }
    if (pendingConnectedFaceBodies_.empty())
    {
        return 0;
    }

    NXOpen::Part* workPart = session_ != nullptr && session_->Parts() != nullptr ?
        session_->Parts()->Work() : nullptr;
    if (workPart == nullptr)
    {
        ShowError("No active work part.");
        return 1;
    }

    const NXOpen::Session::UndoMarkId markId = session_->SetUndoMark(
        NXOpen::Session::MarkVisibilityVisible,
        "BaoRongQieChuConnectedFace");
    size_t successCount = 0;
    size_t failureCount = 0;
    for (NXOpen::Body* body : pendingConnectedFaceBodies_)
    {
        if (body == nullptr)
        {
            ++failureCount;
            continue;
        }

        const bool created = CreateConnectedFaceFeature(workPart, body);
        if (created)
        {
            ++successCount;
        }
        else
        {
            ++failureCount;
        }
        WriteDebugLog(
            "connected_face_result body_tag=" + std::to_string(body->Tag()) +
            " success=" + (created ? "1" : "0"));
    }

    WriteDebugLog(
        "connected_face_summary requested_count=" +
        std::to_string(pendingConnectedFaceBodies_.size()) +
        " success_count=" + std::to_string(successCount) +
        " skipped_count=" + std::to_string(failureCount));
    pendingConnectedFaceBodies_.clear();

    session_->DeleteUndoMark(markId, "BaoRongQieChuConnectedFace");
    return 0;
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
        pendingBlendReplacements_.clear();
        pendingConnectedFaceBodies_.clear();
    }

    if (!GetHealRemovedRegionEnabled())
    {
        pendingCutFeatures_.clear();
        ClearPendingHoleProfiles();
    }
}

void BaoRongQieChuDialog::LoadDialogMemory()
{
    const wchar_t* fileName = L"BaoRongQieChu_state.ini";
    zhihui_dialog_memory::LoadLogical(fileName, L"booleanSubtract", booleanToggleBlock_);
    zhihui_dialog_memory::LoadLogical(fileName, L"removeBlend", removeBlendToggleBlock_);
    zhihui_dialog_memory::LoadLogical(fileName, L"healRemovedRegion", healRemovedRegionToggleBlock_);
    zhihui_dialog_memory::LoadDouble(fileName, L"blendRadius", blendRadiusBlock_);
    zhihui_dialog_memory::LoadDouble(fileName, L"offset", offsetBlock_);
}

void BaoRongQieChuDialog::SaveDialogMemory()
{
    const wchar_t* fileName = L"BaoRongQieChu_state.ini";
    zhihui_dialog_memory::SaveLogical(fileName, L"booleanSubtract", booleanToggleBlock_);
    zhihui_dialog_memory::SaveLogical(fileName, L"removeBlend", removeBlendToggleBlock_);
    zhihui_dialog_memory::SaveLogical(fileName, L"healRemovedRegion", healRemovedRegionToggleBlock_);
    zhihui_dialog_memory::SaveDouble(fileName, L"blendRadius", blendRadiusBlock_);
    zhihui_dialog_memory::SaveDouble(fileName, L"offset", offsetBlock_);
}

void BaoRongQieChuDialog::RememberPendingBlendReplacement(
    NXOpen::Body* body,
    double maxRadius,
    double sheetThickness,
    bool hasSheetThickness)
{
    if (body == nullptr)
    {
        return;
    }

    for (PendingBlendReplacement& pending : pendingBlendReplacements_)
    {
        if (pending.targetBody == body)
        {
            pending.maxRadius = maxRadius;
            pending.sheetThickness = sheetThickness;
            pending.hasSheetThickness = hasSheetThickness;
            return;
        }
    }

    pendingBlendReplacements_.push_back(
        { body, maxRadius, sheetThickness, hasSheetThickness });
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

void BaoRongQieChuDialog::RememberPendingConnectedFaceBody(NXOpen::Body* body)
{
    if (body == nullptr)
    {
        return;
    }

    for (NXOpen::Body* existingBody : pendingConnectedFaceBodies_)
    {
        if (existingBody == body)
        {
            return;
        }
    }
    pendingConnectedFaceBodies_.push_back(body);
}

void BaoRongQieChuDialog::CapturePendingHoleProfiles(const std::vector<NXOpen::TaggedObject*>& selectedObjects)
{
    NXOpen::Part* workPart = session_ != nullptr && session_->Parts() != nullptr ?
        session_->Parts()->Work() : nullptr;
    if (workPart == nullptr)
    {
        return;
    }

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

    const auto captureMatchingLoops = [&](const std::unordered_set<tag_t>& selectedRingEdges) -> bool
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

        bool capturedAny = false;
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

                    NXOpen::Face* planarFace = dynamic_cast<NXOpen::Face*>(
                        NXOpen::NXObjectManager::Get(planeData.faceTag));
                    double faceArea = 0.0;
                    double innerLoopArea = 0.0;
                    const bool hasFaceArea = AskFaceArea(workPart, planarFace, faceArea);
                    const bool hasInnerLoopArea = AskPlanarLoopArea(
                        loop,
                        planeData.normal,
                        innerLoopArea);
                    const double areaRatio =
                        hasFaceArea && faceArea > 1.0e-9 && hasInnerLoopArea ?
                        innerLoopArea / faceArea : 0.0;
                    const bool areaQualifies =
                        hasFaceArea && hasInnerLoopArea && areaRatio < 0.15;
                    WriteDebugLog(
                        "hole_profile_inner_loop_area face_tag=" +
                        std::to_string(planeData.faceTag) +
                        " face_area=" + FormatDouble(faceArea) +
                        " inner_loop_area=" + FormatDouble(innerLoopArea) +
                        " area_ratio=" + FormatDouble(areaRatio) +
                        " limit=0.150000 qualifies=" +
                        (areaQualifies ? "1" : "0"));
                    if (!areaQualifies)
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
                        WriteDebugLog(
                            "hole_profile_skip_duplicate_plane face_tag=" +
                            std::to_string(planeData.faceTag));
                        continue;
                    }

                    if (!capturedProjectedLoopsInThisSelection.insert(projectedLoopKey).second)
                    {
                        WriteDebugLog(
                            "hole_profile_skip_duplicate_projection face_tag=" +
                            std::to_string(planeData.faceTag) +
                            " inner_loop_area=" + FormatDouble(innerLoopArea));
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
                        capturedAny = true;
                        WriteDebugLog(
                            "hole_profile_captured face_tag=" +
                            std::to_string(planeData.faceTag) +
                            " inner_loop_area=" + FormatDouble(innerLoopArea) +
                            " curve_count=" + std::to_string(profile.curveTags.size()));
                        break;
                    }
                }

                UF_MODL_delete_loop_list(&loopList);
                if (capturedFromThisFace)
                {
                    continue;
                }
        }

        return capturedAny;
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
                if (captureMatchingLoops(ringEdges))
                {
                    markConsumedEdges(ringEdges);
                    capturedFromHoleFace = true;
                }
                break;
            }

            if (!touchesHoleFace && !capturedFromHoleFace)
            {
                const std::unordered_set<tag_t> ringEdges{edge->Tag()};
                if (!hasConsumedEdge(ringEdges) && captureMatchingLoops(ringEdges))
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
            if (!hasConsumedEdge(ringEdges) && captureMatchingLoops(ringEdges))
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
