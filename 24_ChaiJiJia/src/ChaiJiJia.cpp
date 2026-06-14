#include <NXOpen/BlockStyler_BlockDialog.hxx>
#include <NXOpen/BlockStyler_DoubleBlock.hxx>
#include <NXOpen/BlockStyler_PropertyList.hxx>
#include <NXOpen/BlockStyler_SelectObject.hxx>
#include <NXOpen/BlockStyler_UIBlock.hxx>
#include <NXOpen/Callback.hxx>
#include <NXOpen/Direction.hxx>
#include <NXOpen/DirectionCollection.hxx>
#include <NXOpen/DisplayManager.hxx>
#include <NXOpen/DisplayModification.hxx>
#include <NXOpen/DisplayableObject.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXMessageBox.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/Selection.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/SmartObject.hxx>
#include <NXOpen/TaggedObject.hxx>
#include <NXOpen/UI.hxx>
#include <NXOpen/View.hxx>
#include <NXOpen/ViewCollection.hxx>
#include <NXOpen/ViewDependentDisplayManager.hxx>

#include <uf.h>
#include <uf_curve.h>
#include <uf_defs.h>
#include <uf_disp.h>
#include <uf_exit.h>
#include <uf_modl.h>
#include <uf_obj.h>
#include <uf_object_types.h>
#include <uf_ui_types.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <exception>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "../../../common/ZhihuiEmbeddedDialog.hpp"
#include "../../../common/ZhihuiDialogMemory.hpp"
#include "../embedded_dialog_resources.h"
#include "../../../protection/native/ZhihuiLicenseGuard.hpp"

#ifndef DllExport
#define DllExport __declspec(dllexport)
#endif

namespace
{
std::string DebugLogPath()
{
    return "D:\\ChaiJiJia-debug.log";
}

void DebugLog(const std::string& message)
{
    std::ofstream stream(DebugLogPath().c_str(), std::ios::app);
    if (!stream)
    {
        return;
    }
    stream << message << "\n";
}

std::string FormatPoint(const double point[3])
{
    std::ostringstream stream;
    stream.setf(std::ios::fixed);
    stream.precision(3);
    stream << point[0] << "," << point[1] << "," << point[2];
    return stream.str();
}

std::string FormatDouble(double value)
{
    std::ostringstream stream;
    stream.setf(std::ios::fixed);
    stream.precision(2);
    stream << value;
    return stream.str();
}

void ThrowUfError(int rc, const char* operation)
{
    if (rc == 0)
    {
        return;
    }

    std::ostringstream message;
    message << operation << " failed, UF error " << rc;
    throw std::runtime_error(message.str());
}

double Dot3(const double lhs[3], const double rhs[3])
{
    return lhs[0] * rhs[0] + lhs[1] * rhs[1] + lhs[2] * rhs[2];
}

double Distance3(const double lhs[3], const double rhs[3])
{
    const double dx = rhs[0] - lhs[0];
    const double dy = rhs[1] - lhs[1];
    const double dz = rhs[2] - lhs[2];
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

void Cross3(const double lhs[3], const double rhs[3], double result[3])
{
    result[0] = lhs[1] * rhs[2] - lhs[2] * rhs[1];
    result[1] = lhs[2] * rhs[0] - lhs[0] * rhs[2];
    result[2] = lhs[0] * rhs[1] - lhs[1] * rhs[0];
}

bool Normalize3(double vector[3])
{
    const double length = std::sqrt(Dot3(vector, vector));
    if (length < 1.0e-9)
    {
        return false;
    }

    vector[0] /= length;
    vector[1] /= length;
    vector[2] /= length;
    return true;
}

double DistancePointToSegment3(const double point[3], const double segmentStart[3], const double segmentEnd[3])
{
    double segment[3] =
    {
        segmentEnd[0] - segmentStart[0],
        segmentEnd[1] - segmentStart[1],
        segmentEnd[2] - segmentStart[2]
    };

    const double lengthSquared = Dot3(segment, segment);
    if (lengthSquared < 1.0e-9)
    {
        return Distance3(point, segmentStart);
    }

    double startToPoint[3] =
    {
        point[0] - segmentStart[0],
        point[1] - segmentStart[1],
        point[2] - segmentStart[2]
    };

    double parameter = Dot3(startToPoint, segment) / lengthSquared;
    parameter = std::max(0.0, std::min(1.0, parameter));

    double closest[3] =
    {
        segmentStart[0] + segment[0] * parameter,
        segmentStart[1] + segment[1] * parameter,
        segmentStart[2] + segment[2] * parameter
    };

    return Distance3(point, closest);
}

double DistancePointToLine3(const double point[3], const double lineStart[3], const double lineEnd[3])
{
    double direction[3] =
    {
        lineEnd[0] - lineStart[0],
        lineEnd[1] - lineStart[1],
        lineEnd[2] - lineStart[2]
    };

    const double lengthSquared = Dot3(direction, direction);
    if (lengthSquared < 1.0e-9)
    {
        return Distance3(point, lineStart);
    }

    double startToPoint[3] =
    {
        point[0] - lineStart[0],
        point[1] - lineStart[1],
        point[2] - lineStart[2]
    };
    const double parameter = Dot3(startToPoint, direction) / lengthSquared;

    double closest[3] =
    {
        lineStart[0] + direction[0] * parameter,
        lineStart[1] + direction[1] * parameter,
        lineStart[2] + direction[2] * parameter
    };

    return Distance3(point, closest);
}

std::vector<tag_t> UfListToTags(uf_list_p_t list)
{
    std::vector<tag_t> tags;
    int count = 0;
    if (list == NULL || UF_MODL_ask_list_count(list, &count) != 0)
    {
        return tags;
    }

    for (int index = 0; index < count; ++index)
    {
        tag_t tag = NULL_TAG;
        if (UF_MODL_ask_list_item(list, index, &tag) == 0 && tag != NULL_TAG)
        {
            tags.push_back(tag);
        }
    }

    return tags;
}

struct EdgeEndpointPair
{
    tag_t edgeTag;
    double first[3];
    double second[3];
};

struct SelectedFaceInfo
{
    NXOpen::Face* face;
    double pickPoint[3];
};

struct FaceChainProfile
{
    tag_t faceTag;
    std::vector<tag_t> curveTags;
};

bool IsLinearEdge(tag_t edgeTag)
{
    int edgeType = 0;
    return edgeTag != NULL_TAG &&
        UF_MODL_ask_edge_type(edgeTag, &edgeType) == 0 &&
        edgeType == UF_MODL_LINEAR_EDGE;
}

bool AskFaceEdgeEndpointPairs(tag_t faceTag, std::vector<EdgeEndpointPair>& endpointPairs)
{
    endpointPairs.clear();

    uf_list_p_t edgeList = NULL;
    if (UF_MODL_ask_face_edges(faceTag, &edgeList) != 0 || edgeList == NULL)
    {
        return false;
    }

    const std::vector<tag_t> edgeTags = UfListToTags(edgeList);
    UF_MODL_delete_list(&edgeList);

    for (std::size_t index = 0; index < edgeTags.size(); ++index)
    {
        double point1[3] = {0.0, 0.0, 0.0};
        double point2[3] = {0.0, 0.0, 0.0};
        int vertexCount = 0;
        if (UF_MODL_ask_edge_verts(edgeTags[index], point1, point2, &vertexCount) != 0 || vertexCount != 2)
        {
            continue;
        }

        EdgeEndpointPair endpoints = {};
        endpoints.edgeTag = edgeTags[index];
        for (int axis = 0; axis < 3; ++axis)
        {
            endpoints.first[axis] = point1[axis];
            endpoints.second[axis] = point2[axis];
        }
        endpointPairs.push_back(endpoints);
    }

    return !endpointPairs.empty();
}

bool ComputeFaceCenterFromEndpoints(const std::vector<EdgeEndpointPair>& endpointPairs, double center[3])
{
    if (endpointPairs.empty())
    {
        return false;
    }

    center[0] = 0.0;
    center[1] = 0.0;
    center[2] = 0.0;

    int pointCount = 0;
    for (std::size_t index = 0; index < endpointPairs.size(); ++index)
    {
        for (int axis = 0; axis < 3; ++axis)
        {
            center[axis] += endpointPairs[index].first[axis] + endpointPairs[index].second[axis];
        }
        pointCount += 2;
    }

    center[0] /= static_cast<double>(pointCount);
    center[1] /= static_cast<double>(pointCount);
    center[2] /= static_cast<double>(pointCount);
    return true;
}

bool AskFaceNormalFromDirectionCollection(tag_t faceTag, double normal[3])
{
    NXOpen::Face* face = dynamic_cast<NXOpen::Face*>(NXOpen::NXObjectManager::Get(faceTag));
    NXOpen::Session* session = NXOpen::Session::GetSession();
    NXOpen::Part* workPart = session != NULL && session->Parts() != NULL ? session->Parts()->Work() : NULL;
    if (face == NULL || workPart == NULL)
    {
        return false;
    }

    NXOpen::Direction* direction = workPart->Directions()->CreateDumbDirectionFace(
        face,
        NXOpen::SenseForward,
        NXOpen::SmartObject::UpdateOptionWithinModeling);
    if (direction == NULL)
    {
        return false;
    }

    NXOpen::Vector3d vector = direction->Vector();
    normal[0] = vector.X;
    normal[1] = vector.Y;
    normal[2] = vector.Z;
    return Normalize3(normal);
}

bool AskPlanarFacePlane(tag_t faceTag, double point[3], double normal[3])
{
    int faceType = 0;
    double box[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    double radius = 0.0;
    double radData = 0.0;
    int normalDir = 0;
    if (UF_MODL_ask_face_data(faceTag, &faceType, point, normal, box, &radius, &radData, &normalDir) != 0 ||
        faceType != UF_MODL_PLANAR_FACE)
    {
        return false;
    }

    std::vector<EdgeEndpointPair> endpoints;
    if (AskFaceEdgeEndpointPairs(faceTag, endpoints))
    {
        double center[3] = {0.0, 0.0, 0.0};
        if (ComputeFaceCenterFromEndpoints(endpoints, center))
        {
            point[0] = center[0];
            point[1] = center[1];
            point[2] = center[2];
        }
    }

    return AskFaceNormalFromDirectionCollection(faceTag, normal);
}

bool AskFaceCenter(tag_t faceTag, double center[3])
{
    std::vector<EdgeEndpointPair> endpoints;
    return AskFaceEdgeEndpointPairs(faceTag, endpoints) && ComputeFaceCenterFromEndpoints(endpoints, center);
}

NXOpen::Face* FaceFromTag(tag_t faceTag)
{
    return faceTag == NULL_TAG ? NULL : dynamic_cast<NXOpen::Face*>(NXOpen::NXObjectManager::Get(faceTag));
}

bool AskEdgeEndpoints(tag_t edgeTag, double first[3], double second[3])
{
    int vertexCount = 0;
    return edgeTag != NULL_TAG &&
        UF_MODL_ask_edge_verts(edgeTag, first, second, &vertexCount) == 0 &&
        vertexCount == 2;
}

bool AskEdgeDirection(tag_t edgeTag, double direction[3])
{
    double first[3] = {0.0, 0.0, 0.0};
    double second[3] = {0.0, 0.0, 0.0};
    if (!AskEdgeEndpoints(edgeTag, first, second))
    {
        return false;
    }

    direction[0] = second[0] - first[0];
    direction[1] = second[1] - first[1];
    direction[2] = second[2] - first[2];
    return Normalize3(direction);
}

bool FindNearestLinearFaceEdge(tag_t faceTag, const double pickPoint[3], tag_t& edgeTag, double edgeDirection[3])
{
    edgeTag = NULL_TAG;
    edgeDirection[0] = 0.0;
    edgeDirection[1] = 0.0;
    edgeDirection[2] = 0.0;

    std::vector<EdgeEndpointPair> endpoints;
    if (!AskFaceEdgeEndpointPairs(faceTag, endpoints))
    {
        return false;
    }

    double bestDistance = DBL_MAX;
    for (std::size_t index = 0; index < endpoints.size(); ++index)
    {
        if (!IsLinearEdge(endpoints[index].edgeTag))
        {
            continue;
        }

        const double distance = DistancePointToSegment3(pickPoint, endpoints[index].first, endpoints[index].second);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            edgeTag = endpoints[index].edgeTag;
            edgeDirection[0] = endpoints[index].second[0] - endpoints[index].first[0];
            edgeDirection[1] = endpoints[index].second[1] - endpoints[index].first[1];
            edgeDirection[2] = endpoints[index].second[2] - endpoints[index].first[2];
        }
    }

    return edgeTag != NULL_TAG && Normalize3(edgeDirection);
}

std::vector<tag_t> AskEdgeAdjacentFaces(tag_t edgeTag)
{
    std::vector<tag_t> faces;
    uf_list_p_t faceList = NULL;
    if (edgeTag == NULL_TAG || UF_MODL_ask_edge_faces(edgeTag, &faceList) != 0 || faceList == NULL)
    {
        return faces;
    }

    faces = UfListToTags(faceList);
    UF_MODL_delete_list(&faceList);
    return faces;
}

bool IsEdgeOnFace(tag_t edgeTag, tag_t faceTag)
{
    const std::vector<tag_t> adjacentFaces = AskEdgeAdjacentFaces(edgeTag);
    return std::find(adjacentFaces.begin(), adjacentFaces.end(), faceTag) != adjacentFaces.end();
}

tag_t FindOtherFaceAcrossEdge(tag_t edgeTag, tag_t currentFaceTag, tag_t bodyTag)
{
    const std::vector<tag_t> adjacentFaces = AskEdgeAdjacentFaces(edgeTag);
    for (std::size_t index = 0; index < adjacentFaces.size(); ++index)
    {
        if (adjacentFaces[index] == currentFaceTag)
        {
            continue;
        }

        tag_t adjacentBody = NULL_TAG;
        if (UF_MODL_ask_face_body(adjacentFaces[index], &adjacentBody) == 0 && adjacentBody == bodyTag)
        {
            return adjacentFaces[index];
        }
    }

    return NULL_TAG;
}

tag_t FindNearestLinearEdgeParallelToEdge(
    tag_t faceTag,
    tag_t referenceEdgeTag,
    tag_t excludedEdgeTag,
    tag_t bodyTag,
    tag_t seedFaceTag,
    const std::set<tag_t>& visitedFaces)
{
    double referenceFirst[3] = {0.0, 0.0, 0.0};
    double referenceSecond[3] = {0.0, 0.0, 0.0};
    double referenceDirection[3] = {0.0, 0.0, 0.0};
    if (!AskEdgeEndpoints(referenceEdgeTag, referenceFirst, referenceSecond) ||
        !AskEdgeDirection(referenceEdgeTag, referenceDirection))
    {
        return NULL_TAG;
    }

    std::vector<EdgeEndpointPair> endpoints;
    if (!AskFaceEdgeEndpointPairs(faceTag, endpoints))
    {
        return NULL_TAG;
    }

    tag_t bestEdge = NULL_TAG;
    double bestDistance = DBL_MAX;
    for (std::size_t index = 0; index < endpoints.size(); ++index)
    {
        if (endpoints[index].edgeTag == excludedEdgeTag || !IsLinearEdge(endpoints[index].edgeTag))
        {
            continue;
        }

        double candidateDirection[3] =
        {
            endpoints[index].second[0] - endpoints[index].first[0],
            endpoints[index].second[1] - endpoints[index].first[1],
            endpoints[index].second[2] - endpoints[index].first[2]
        };
        if (!Normalize3(candidateDirection) || std::fabs(Dot3(candidateDirection, referenceDirection)) < 0.92)
        {
            continue;
        }

        const tag_t otherFace = FindOtherFaceAcrossEdge(endpoints[index].edgeTag, faceTag, bodyTag);
        if (otherFace == NULL_TAG)
        {
            continue;
        }
        if (visitedFaces.find(otherFace) != visitedFaces.end() && otherFace != seedFaceTag)
        {
            std::ostringstream log;
            log << "Skip parallel edge=" << endpoints[index].edgeTag
                << " because it returns to visited face=" << otherFace;
            DebugLog(log.str());
            continue;
        }

        double midpoint[3] =
        {
            (endpoints[index].first[0] + endpoints[index].second[0]) * 0.5,
            (endpoints[index].first[1] + endpoints[index].second[1]) * 0.5,
            (endpoints[index].first[2] + endpoints[index].second[2]) * 0.5
        };
        const double distance = DistancePointToLine3(midpoint, referenceFirst, referenceSecond);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestEdge = endpoints[index].edgeTag;
        }
    }

    return bestEdge;
}

std::vector<tag_t> CollectClosedFaceChainByOppositeParallelEdges(tag_t seedFaceTag, tag_t bodyTag, tag_t seedEdgeTag)
{
    std::vector<tag_t> chainFaces;
    std::set<tag_t> visitedFaces;

    {
        std::ostringstream log;
        log << "Collect chain start: seedFace=" << seedFaceTag
            << " body=" << bodyTag
            << " seedEdge=" << seedEdgeTag;
        DebugLog(log.str());
    }

    if (seedFaceTag == NULL_TAG || bodyTag == NULL_TAG || seedEdgeTag == NULL_TAG)
    {
        DebugLog("Collect chain failed: seed face/body/edge is null.");
        return chainFaces;
    }

    tag_t currentFace = seedFaceTag;
    tag_t currentEdge = seedEdgeTag;
    visitedFaces.insert(seedFaceTag);
    chainFaces.push_back(seedFaceTag);

    for (int step = 0; step < 64; ++step)
    {
        {
            std::ostringstream log;
            log << "Step " << step
                << ": currentFace=" << currentFace
                << " currentEdge=" << currentEdge;
            DebugLog(log.str());
        }

        const tag_t nextFace = FindOtherFaceAcrossEdge(currentEdge, currentFace, bodyTag);
        if (nextFace == NULL_TAG)
        {
            DebugLog("Collect chain failed: no other face across current edge.");
            chainFaces.clear();
            return chainFaces;
        }

        {
            std::ostringstream log;
            log << "Step " << step << ": nextFace=" << nextFace;
            DebugLog(log.str());
        }

        if (nextFace == seedFaceTag)
        {
            std::ostringstream log;
            log << "Collect chain closed by crossing back to seed face. count=" << chainFaces.size();
            DebugLog(log.str());
            return chainFaces.size() >= 3 ? chainFaces : std::vector<tag_t>();
        }

        if (visitedFaces.find(nextFace) != visitedFaces.end())
        {
            std::ostringstream log;
            log << "Collect chain failed: next face already visited, face=" << nextFace;
            DebugLog(log.str());
            chainFaces.clear();
            return chainFaces;
        }

        visitedFaces.insert(nextFace);
        chainFaces.push_back(nextFace);

        const tag_t oppositeEdge = FindNearestLinearEdgeParallelToEdge(
            nextFace,
            currentEdge,
            currentEdge,
            bodyTag,
            seedFaceTag,
            visitedFaces);
        if (oppositeEdge == NULL_TAG)
        {
            DebugLog("Collect chain failed: no nearest parallel edge on next face.");
            chainFaces.clear();
            return chainFaces;
        }

        {
            std::ostringstream log;
            log << "Step " << step << ": nearestParallelEdge=" << oppositeEdge
                << " edgeOnSeed=" << (IsEdgeOnFace(oppositeEdge, seedFaceTag) ? "true" : "false");
            DebugLog(log.str());
        }

        if (IsEdgeOnFace(oppositeEdge, seedFaceTag))
        {
            std::ostringstream log;
            log << "Collect chain closed by nearest parallel edge on seed face. count=" << chainFaces.size();
            DebugLog(log.str());
            return chainFaces.size() >= 3 ? chainFaces : std::vector<tag_t>();
        }

        currentFace = nextFace;
        currentEdge = oppositeEdge;
    }

    chainFaces.clear();
    DebugLog("Collect chain failed: exceeded max step count.");
    return chainFaces;
}

bool AskPlanarFaceProjectedArea(tag_t faceTag, double& area)
{
    area = 0.0;

    double center[3] = {0.0, 0.0, 0.0};
    double normal[3] = {0.0, 0.0, 0.0};
    if (!AskPlanarFacePlane(faceTag, center, normal))
    {
        return false;
    }

    std::vector<EdgeEndpointPair> endpoints;
    if (!AskFaceEdgeEndpointPairs(faceTag, endpoints))
    {
        return false;
    }

    double firstAxis[3] =
    {
        endpoints.front().second[0] - endpoints.front().first[0],
        endpoints.front().second[1] - endpoints.front().first[1],
        endpoints.front().second[2] - endpoints.front().first[2]
    };
    if (!Normalize3(firstAxis))
    {
        return false;
    }

    double secondAxis[3] = {0.0, 0.0, 0.0};
    Cross3(normal, firstAxis, secondAxis);
    if (!Normalize3(secondAxis))
    {
        return false;
    }

    double minFirst = DBL_MAX;
    double maxFirst = -DBL_MAX;
    double minSecond = DBL_MAX;
    double maxSecond = -DBL_MAX;

    for (std::size_t index = 0; index < endpoints.size(); ++index)
    {
        const double* points[2] = {endpoints[index].first, endpoints[index].second};
        for (int pointIndex = 0; pointIndex < 2; ++pointIndex)
        {
            const double firstProjection = Dot3(points[pointIndex], firstAxis);
            const double secondProjection = Dot3(points[pointIndex], secondAxis);
            minFirst = std::min(minFirst, firstProjection);
            maxFirst = std::max(maxFirst, firstProjection);
            minSecond = std::min(minSecond, secondProjection);
            maxSecond = std::max(maxSecond, secondProjection);
        }
    }

    if (minFirst == DBL_MAX || minSecond == DBL_MAX)
    {
        return false;
    }

    area = std::fabs((maxFirst - minFirst) * (maxSecond - minSecond));
    return area > 1.0e-6;
}

double AskSquareTubeThicknessFromSelectedFace(tag_t bodyTag, tag_t selectedFaceTag)
{
    double selectedPoint[3] = {0.0, 0.0, 0.0};
    double selectedNormal[3] = {0.0, 0.0, 0.0};
    double selectedArea = 0.0;
    if (!AskPlanarFacePlane(selectedFaceTag, selectedPoint, selectedNormal) ||
        !AskPlanarFaceProjectedArea(selectedFaceTag, selectedArea))
    {
        return 0.0;
    }

    uf_list_p_t faceList = NULL;
    if (bodyTag == NULL_TAG || UF_MODL_ask_body_faces(bodyTag, &faceList) != 0 || faceList == NULL)
    {
        return 0.0;
    }

    const std::vector<tag_t> faceTags = UfListToTags(faceList);
    UF_MODL_delete_list(&faceList);

    double bestDistance = DBL_MAX;
    for (std::size_t index = 0; index < faceTags.size(); ++index)
    {
        if (faceTags[index] == selectedFaceTag)
        {
            continue;
        }

        double point[3] = {0.0, 0.0, 0.0};
        double normal[3] = {0.0, 0.0, 0.0};
        double area = 0.0;
        if (!AskPlanarFacePlane(faceTags[index], point, normal) ||
            std::fabs(Dot3(selectedNormal, normal)) < 0.98 ||
            !AskPlanarFaceProjectedArea(faceTags[index], area) ||
            area <= selectedArea * 0.5)
        {
            continue;
        }

        double delta[3] =
        {
            point[0] - selectedPoint[0],
            point[1] - selectedPoint[1],
            point[2] - selectedPoint[2]
        };
        const double distance = std::fabs(Dot3(delta, selectedNormal));
        if (distance > 0.05 && distance < bestDistance)
        {
            bestDistance = distance;
        }
    }

    if (bestDistance == DBL_MAX)
    {
        return 0.0;
    }

    return std::min(bestDistance, 18.0);
}

void DeleteObjects(const std::vector<tag_t>& tags)
{
    for (std::size_t index = 0; index < tags.size(); ++index)
    {
        if (tags[index] != NULL_TAG)
        {
            UF_OBJ_delete_object(tags[index]);
        }
    }
}

void HideProcessObject(tag_t objectTag)
{
    if (objectTag != NULL_TAG)
    {
        UF_OBJ_set_blank_status(objectTag, UF_OBJ_BLANKED);
    }
}

void AddUniqueCurveFromEdge(tag_t edgeTag, std::set<tag_t>& sourceEdges, std::vector<tag_t>& curveTags)
{
    if (edgeTag == NULL_TAG || sourceEdges.find(edgeTag) != sourceEdges.end())
    {
        return;
    }

    tag_t curveTag = NULL_TAG;
    if (UF_MODL_create_curve_from_edge(edgeTag, &curveTag) == 0 && curveTag != NULL_TAG)
    {
        sourceEdges.insert(edgeTag);
        curveTags.push_back(curveTag);
    }
}

std::vector<FaceChainProfile> CreateInnerLoopProfilesFromFaceChain(const std::vector<tag_t>& faceChain)
{
    std::vector<FaceChainProfile> profiles;
    for (std::size_t faceIndex = 0; faceIndex < faceChain.size(); ++faceIndex)
    {
        uf_loop_p_t loopList = NULL;
        if (UF_MODL_ask_face_loops(faceChain[faceIndex], &loopList) != 0 || loopList == NULL)
        {
            continue;
        }

        std::set<tag_t> sourceEdges;
        for (uf_loop_p_t loop = loopList; loop != NULL; loop = loop->next)
        {
            if (loop->type != 2)
            {
                continue;
            }

            FaceChainProfile profile = {};
            profile.faceTag = faceChain[faceIndex];
            for (uf_list_p_t edgeNode = loop->edge_list; edgeNode != NULL; edgeNode = edgeNode->next)
            {
                AddUniqueCurveFromEdge(edgeNode->eid, sourceEdges, profile.curveTags);
            }

            if (!profile.curveTags.empty())
            {
                profiles.push_back(profile);
            }
        }

        UF_MODL_delete_loop_list(&loopList);
    }

    return profiles;
}

bool AskFaceInnerNormalFromNxDirection(NXOpen::Face* face, tag_t bodyTag, const double referencePoint[3], double innerNormal[3])
{
    if (face == NULL)
    {
        return false;
    }

    NXOpen::Session* session = NXOpen::Session::GetSession();
    NXOpen::Part* workPart = session != NULL && session->Parts() != NULL ? session->Parts()->Work() : NULL;
    if (workPart == NULL)
    {
        return false;
    }

    NXOpen::Direction* direction = workPart->Directions()->CreateDirection(
        face,
        NXOpen::SenseForward,
        NXOpen::SmartObject::UpdateOptionWithinModeling);
    if (direction == NULL)
    {
        return false;
    }

    NXOpen::Vector3d vector = direction->Vector();
    innerNormal[0] = vector.X;
    innerNormal[1] = vector.Y;
    innerNormal[2] = vector.Z;
    if (!Normalize3(innerNormal))
    {
        return false;
    }

    uf_list_p_t faceList = NULL;
    if (bodyTag == NULL_TAG || UF_MODL_ask_body_faces(bodyTag, &faceList) != 0 || faceList == NULL)
    {
        return true;
    }

    const std::vector<tag_t> bodyFaces = UfListToTags(faceList);
    UF_MODL_delete_list(&faceList);

    double bestSignedDistance = 0.0;
    double bestDistance = DBL_MAX;
    for (std::size_t index = 0; index < bodyFaces.size(); ++index)
    {
        if (bodyFaces[index] == face->Tag())
        {
            continue;
        }

        double point[3] = {0.0, 0.0, 0.0};
        double normal[3] = {0.0, 0.0, 0.0};
        if (!AskPlanarFacePlane(bodyFaces[index], point, normal) ||
            std::fabs(Dot3(innerNormal, normal)) < 0.98)
        {
            continue;
        }

        double delta[3] =
        {
            point[0] - referencePoint[0],
            point[1] - referencePoint[1],
            point[2] - referencePoint[2]
        };
        const double signedDistance = Dot3(delta, innerNormal);
        const double distance = std::fabs(signedDistance);
        if (distance > 0.05 && distance < bestDistance)
        {
            bestDistance = distance;
            bestSignedDistance = signedDistance;
        }
    }

    if (bestDistance < DBL_MAX && bestSignedDistance < 0.0)
    {
        innerNormal[0] = -innerNormal[0];
        innerNormal[1] = -innerNormal[1];
        innerNormal[2] = -innerNormal[2];
    }

    return true;
}

tag_t CreateExtrudedToolBody(const std::vector<tag_t>& profileCurves, const double direction[3], double startLimitValue, double endLimitValue)
{
    uf_list_p_t curveList = NULL;
    ThrowUfError(UF_MODL_create_list(&curveList), "UF_MODL_create_list");

    for (std::size_t index = 0; index < profileCurves.size(); ++index)
    {
        ThrowUfError(UF_MODL_put_list_item(curveList, profileCurves[index]), "UF_MODL_put_list_item");
    }

    std::string startLimit = FormatDouble(startLimitValue);
    std::string endLimit = FormatDouble(endLimitValue);
    char taperAngle[] = "0.0";
    char* limits[2] = {const_cast<char*>(startLimit.c_str()), const_cast<char*>(endLimit.c_str())};
    double unusedPoint[3] = {0.0, 0.0, 0.0};
    double extrudeDirection[3] = {direction[0], direction[1], direction[2]};
    uf_list_p_t featureList = NULL;

    ThrowUfError(
        UF_MODL_create_extruded(curveList, taperAngle, limits, unusedPoint, extrudeDirection, UF_NULLSIGN, &featureList),
        "UF_MODL_create_extruded");
    UF_MODL_delete_list(&curveList);

    const std::vector<tag_t> featureTags = UfListToTags(featureList);
    UF_MODL_delete_list(&featureList);
    if (featureTags.empty())
    {
        throw std::runtime_error("No extruded body was created.");
    }

    tag_t toolBody = NULL_TAG;
    ThrowUfError(UF_MODL_ask_feat_body(featureTags.front(), &toolBody), "UF_MODL_ask_feat_body");
    if (toolBody == NULL_TAG)
    {
        throw std::runtime_error("Failed to resolve extruded body.");
    }

    uf_list_p_t bodyList = NULL;
    ThrowUfError(UF_MODL_create_list(&bodyList), "UF_MODL_create_list");
    ThrowUfError(UF_MODL_put_list_item(bodyList, toolBody), "UF_MODL_put_list_item");
    ThrowUfError(UF_MODL_delete_body_parms(bodyList), "UF_MODL_delete_body_parms");
    ThrowUfError(UF_MODL_ask_list_item(bodyList, 0, &toolBody), "UF_MODL_ask_list_item");
    UF_MODL_delete_list(&bodyList);

    return toolBody;
}

std::vector<tag_t> CreateInnerLoopExtrudeBodies(tag_t bodyTag, const std::vector<tag_t>& faceChain, double thickness)
{
    std::vector<tag_t> toolBodies;
    std::vector<FaceChainProfile> profiles = CreateInnerLoopProfilesFromFaceChain(faceChain);

    for (std::size_t profileIndex = 0; profileIndex < profiles.size(); ++profileIndex)
    {
        NXOpen::Face* profileFace = FaceFromTag(profiles[profileIndex].faceTag);
        double profileCenter[3] = {0.0, 0.0, 0.0};
        double innerNormal[3] = {0.0, 0.0, 0.0};

        if (profileFace == NULL ||
            !AskFaceCenter(profiles[profileIndex].faceTag, profileCenter) ||
            !AskFaceInnerNormalFromNxDirection(profileFace, bodyTag, profileCenter, innerNormal))
        {
            DeleteObjects(profiles[profileIndex].curveTags);
            continue;
        }

        try
        {
            tag_t body = CreateExtrudedToolBody(
                profiles[profileIndex].curveTags,
                innerNormal,
                0.0,
                thickness);
            toolBodies.push_back(body);
        }
        catch (...)
        {
            DeleteObjects(profiles[profileIndex].curveTags);
            throw;
        }

        DeleteObjects(profiles[profileIndex].curveTags);
    }

    return toolBodies;
}

std::vector<tag_t> CreateIntersectionCurvesForFaceChain(
    const std::vector<tag_t>& faceChain,
    const double pickPoint[3],
    const double nearEdgeDirection[3])
{
    std::vector<tag_t> curveTags;
    double planeNormal[3] = {nearEdgeDirection[0], nearEdgeDirection[1], nearEdgeDirection[2]};
    if (!Normalize3(planeNormal))
    {
        return curveTags;
    }

    tag_t planeTag = NULL_TAG;
    ThrowUfError(
        UF_MODL_create_plane(const_cast<double*>(pickPoint), planeNormal, &planeTag),
        "UF_MODL_create_plane");

    for (std::size_t faceIndex = 0; faceIndex < faceChain.size(); ++faceIndex)
    {
        int intersectionCount = 0;
        UF_MODL_intersect_info_p_t* intersections = NULL;
        const int rc = UF_MODL_intersect_objects(faceChain[faceIndex], planeTag, 0.001, &intersectionCount, &intersections);
        if (rc != 0)
        {
            if (intersections != NULL)
            {
                UF_free(intersections);
            }
            continue;
        }

        for (int index = 0; index < intersectionCount; ++index)
        {
            UF_MODL_intersect_info_p_t info = intersections[index];
            if (info == NULL)
            {
                continue;
            }

            if (info->intersect_type == UF_MODL_INTERSECT_CURVE)
            {
                const tag_t curveTag = info->intersect.curve.identifier;
                if (curveTag != NULL_TAG)
                {
                    curveTags.push_back(curveTag);
                }
            }

            UF_free(info);
        }

        if (intersections != NULL)
        {
            UF_free(intersections);
        }
    }

    UF_OBJ_delete_object(planeTag);
    return curveTags;
}

struct CurveSegmentInfo
{
    tag_t curveTag;
    tag_t faceTag;
    double start[3];
    double end[3];
    double mid[3];
    bool closed;
};

bool AskCurveSegmentInfo(tag_t curveTag, tag_t faceTag, CurveSegmentInfo& info)
{
    info = {};
    info.curveTag = curveTag;
    info.faceTag = faceTag;

    int periodicity = UF_MODL_OPEN_CURVE;
    if (UF_MODL_ask_curve_periodicity(curveTag, &periodicity) == 0 &&
        periodicity != UF_MODL_OPEN_CURVE)
    {
        info.closed = true;
    }

    double tangent[3] = {0.0, 0.0, 0.0};
    double principalNormal[3] = {0.0, 0.0, 0.0};
    double binormal[3] = {0.0, 0.0, 0.0};
    double torsion = 0.0;
    double radiusOfCurvature = 0.0;
    if (UF_MODL_ask_curve_props(curveTag, 0.0, info.start, tangent, principalNormal, binormal, &torsion, &radiusOfCurvature) != 0 ||
        UF_MODL_ask_curve_props(curveTag, 0.5, info.mid, tangent, principalNormal, binormal, &torsion, &radiusOfCurvature) != 0 ||
        UF_MODL_ask_curve_props(curveTag, 1.0, info.end, tangent, principalNormal, binormal, &torsion, &radiusOfCurvature) != 0)
    {
        return false;
    }

    if (Distance3(info.start, info.end) <= 0.25)
    {
        info.closed = true;
    }

    return true;
}

double MinDistanceToCurveInfos(const std::vector<CurveSegmentInfo>& infos, const std::vector<int>& group, const double pickPoint[3])
{
    double bestDistance = DBL_MAX;
    for (std::size_t index = 0; index < group.size(); ++index)
    {
        const CurveSegmentInfo& info = infos[group[index]];
        bestDistance = std::min(bestDistance, DistancePointToSegment3(pickPoint, info.start, info.end));
        bestDistance = std::min(bestDistance, Distance3(pickPoint, info.mid));
    }
    return bestDistance;
}

bool PointsCoincident(const double first[3], const double second[3], double tolerance)
{
    return Distance3(first, second) <= tolerance;
}

struct KeptSectionLoop
{
    std::vector<tag_t> curveTags;
    std::vector<tag_t> faceTags;
};

struct SectionLineInfo
{
    tag_t curveTag;
    double start[3];
    double end[3];
    double mid[3];
    double length;
};

bool AskSectionLineInfo(tag_t curveTag, SectionLineInfo& info)
{
    info = {};
    info.curveTag = curveTag;

    double tangent[3] = {0.0, 0.0, 0.0};
    double principalNormal[3] = {0.0, 0.0, 0.0};
    double binormal[3] = {0.0, 0.0, 0.0};
    double torsion = 0.0;
    double radiusOfCurvature = 0.0;
    if (UF_MODL_ask_curve_props(curveTag, 0.0, info.start, tangent, principalNormal, binormal, &torsion, &radiusOfCurvature) != 0 ||
        UF_MODL_ask_curve_props(curveTag, 0.5, info.mid, tangent, principalNormal, binormal, &torsion, &radiusOfCurvature) != 0 ||
        UF_MODL_ask_curve_props(curveTag, 1.0, info.end, tangent, principalNormal, binormal, &torsion, &radiusOfCurvature) != 0)
    {
        return false;
    }

    info.length = Distance3(info.start, info.end);
    if (info.length < 0.01)
    {
        return false;
    }

    const double lineDistance = DistancePointToSegment3(info.mid, info.start, info.end);
    return lineDistance <= std::max(0.01, info.length * 0.002);
}

void ReverseSectionLine(SectionLineInfo& line)
{
    for (int axis = 0; axis < 3; ++axis)
    {
        std::swap(line.start[axis], line.end[axis]);
    }
}

double OrderedLineGapScore(const std::vector<SectionLineInfo>& lines)
{
    double score = 0.0;
    for (std::size_t index = 0; index < lines.size(); ++index)
    {
        const std::size_t next = (index + 1) % lines.size();
        score += Distance3(lines[index].end, lines[next].start);
    }
    return score;
}

bool OrderFourSectionLines(std::vector<SectionLineInfo>& lines)
{
    if (lines.size() != 4)
    {
        return false;
    }

    std::vector<int> permutation;
    permutation.push_back(0);
    permutation.push_back(1);
    permutation.push_back(2);
    permutation.push_back(3);

    std::vector<SectionLineInfo> bestLines;
    double bestScore = DBL_MAX;
    std::sort(permutation.begin(), permutation.end());
    do
    {
        for (int mask = 0; mask < 16; ++mask)
        {
            std::vector<SectionLineInfo> candidate;
            for (int index = 0; index < 4; ++index)
            {
                SectionLineInfo line = lines[permutation[index]];
                if ((mask & (1 << index)) != 0)
                {
                    ReverseSectionLine(line);
                }
                candidate.push_back(line);
            }

            const double score = OrderedLineGapScore(candidate);
            if (score < bestScore)
            {
                bestScore = score;
                bestLines = candidate;
            }
        }
    }
    while (std::next_permutation(permutation.begin(), permutation.end()));

    if (bestLines.empty())
    {
        return false;
    }

    lines = bestLines;
    return true;
}

bool ClosestPointBetweenInfiniteLines(
    const double firstPoint[3],
    const double firstDirection[3],
    const double secondPoint[3],
    const double secondDirection[3],
    double closestPoint[3])
{
    double d1[3] = {firstDirection[0], firstDirection[1], firstDirection[2]};
    double d2[3] = {secondDirection[0], secondDirection[1], secondDirection[2]};
    if (!Normalize3(d1) || !Normalize3(d2))
    {
        return false;
    }

    double r[3] =
    {
        firstPoint[0] - secondPoint[0],
        firstPoint[1] - secondPoint[1],
        firstPoint[2] - secondPoint[2]
    };
    const double b = Dot3(d1, d2);
    const double d = Dot3(d1, r);
    const double e = Dot3(d2, r);
    const double denominator = 1.0 - b * b;
    if (std::fabs(denominator) < 1.0e-8)
    {
        return false;
    }

    const double s = (b * e - d) / denominator;
    const double t = (e - b * d) / denominator;
    double pointOnFirst[3] =
    {
        firstPoint[0] + d1[0] * s,
        firstPoint[1] + d1[1] * s,
        firstPoint[2] + d1[2] * s
    };
    double pointOnSecond[3] =
    {
        secondPoint[0] + d2[0] * t,
        secondPoint[1] + d2[1] * t,
        secondPoint[2] + d2[2] * t
    };

    for (int axis = 0; axis < 3; ++axis)
    {
        closestPoint[axis] = (pointOnFirst[axis] + pointOnSecond[axis]) * 0.5;
    }
    return true;
}

tag_t CreateLineCurve(const double start[3], const double end[3])
{
    UF_CURVE_line_t lineData = {};
    for (int axis = 0; axis < 3; ++axis)
    {
        lineData.start_point[axis] = start[axis];
        lineData.end_point[axis] = end[axis];
    }

    tag_t lineTag = NULL_TAG;
    ThrowUfError(UF_CURVE_create_line(&lineData, &lineTag), "UF_CURVE_create_line");
    return lineTag;
}

tag_t CreateArcThroughThreePoints(const double start[3], const double mid[3], const double end[3])
{
    tag_t arcTag = NULL_TAG;
    ThrowUfError(
        UF_CURVE_create_arc_thru_3pts(
            1,
            const_cast<double*>(start),
            const_cast<double*>(mid),
            const_cast<double*>(end),
            &arcTag),
        "UF_CURVE_create_arc_thru_3pts");
    return arcTag;
}

std::vector<tag_t> RebuildSectionLoopCornersWithRadius(const std::vector<tag_t>& sourceCurves, double radius)
{
    if (radius <= 0.0)
    {
        throw std::runtime_error("Square tube R must be greater than zero.");
    }

    std::vector<SectionLineInfo> lines;
    for (std::size_t index = 0; index < sourceCurves.size(); ++index)
    {
        SectionLineInfo line = {};
        if (AskSectionLineInfo(sourceCurves[index], line))
        {
            lines.push_back(line);
        }
    }

    if (lines.size() != 4)
    {
        std::ostringstream log;
        log << "Failed: expected 4 straight section edges, got " << lines.size()
            << ", source curves=" << sourceCurves.size();
        DebugLog(log.str());
        throw std::runtime_error("The kept section loop must contain exactly 4 straight edges.");
    }

    if (!OrderFourSectionLines(lines))
    {
        throw std::runtime_error("Failed to order the 4 section edges.");
    }

    double lineDirections[4][3] = {};
    for (int index = 0; index < 4; ++index)
    {
        lineDirections[index][0] = lines[index].end[0] - lines[index].start[0];
        lineDirections[index][1] = lines[index].end[1] - lines[index].start[1];
        lineDirections[index][2] = lines[index].end[2] - lines[index].start[2];
        if (!Normalize3(lineDirections[index]))
        {
            throw std::runtime_error("Failed to normalize section edge direction.");
        }
    }

    double corners[4][3] = {};
    for (int index = 0; index < 4; ++index)
    {
        const int next = (index + 1) % 4;
        if (!ClosestPointBetweenInfiniteLines(
            lines[index].mid,
            lineDirections[index],
            lines[next].mid,
            lineDirections[next],
            corners[index]))
        {
            std::ostringstream log;
            log << "Failed to calculate section corner " << index
                << ", lineA=" << lines[index].curveTag
                << ", lineB=" << lines[next].curveTag;
            DebugLog(log.str());
            throw std::runtime_error("Failed to calculate section corner.");
        }
    }

    double tangentStarts[4][3] = {};
    double tangentEnds[4][3] = {};
    for (int index = 0; index < 4; ++index)
    {
        const int previousCorner = (index + 3) % 4;
        const int nextCorner = index;
        const double sideLength = Distance3(corners[previousCorner], corners[nextCorner]);
        if (sideLength <= radius * 2.0 + 0.01)
        {
            std::ostringstream log;
            log << "Failed: section side too short for R, side=" << index
                << ", length=" << FormatDouble(sideLength)
                << ", radius=" << FormatDouble(radius);
            DebugLog(log.str());
            throw std::runtime_error("Section side is too short for the requested R.");
        }

        for (int axis = 0; axis < 3; ++axis)
        {
            tangentStarts[index][axis] = corners[previousCorner][axis] + lineDirections[index][axis] * radius;
            tangentEnds[index][axis] = corners[nextCorner][axis] - lineDirections[index][axis] * radius;
        }
    }

    std::vector<tag_t> profileCurves;
    try
    {
        for (int index = 0; index < 4; ++index)
        {
            profileCurves.push_back(CreateLineCurve(tangentStarts[index], tangentEnds[index]));
        }

        for (int index = 0; index < 4; ++index)
        {
            const int next = (index + 1) % 4;
            double inwardA[3] =
            {
                -lineDirections[index][0],
                -lineDirections[index][1],
                -lineDirections[index][2]
            };
            double inwardB[3] =
            {
                lineDirections[next][0],
                lineDirections[next][1],
                lineDirections[next][2]
            };
            Normalize3(inwardA);
            Normalize3(inwardB);

            double bisector[3] =
            {
                inwardA[0] + inwardB[0],
                inwardA[1] + inwardB[1],
                inwardA[2] + inwardB[2]
            };
            if (!Normalize3(bisector))
            {
                throw std::runtime_error("Failed to calculate section R bisector.");
            }

            const double cosine = std::max(-1.0, std::min(1.0, Dot3(inwardA, inwardB)));
            const double halfSine = std::sqrt(std::max(0.0, (1.0 - cosine) * 0.5));
            if (halfSine < 1.0e-6)
            {
                throw std::runtime_error("Invalid section corner angle.");
            }

            const double centerDistance = radius / halfSine;
            double arcCenter[3] =
            {
                corners[index][0] + bisector[0] * centerDistance,
                corners[index][1] + bisector[1] * centerDistance,
                corners[index][2] + bisector[2] * centerDistance
            };
            double arcMid[3] =
            {
                arcCenter[0] - bisector[0] * radius,
                arcCenter[1] - bisector[1] * radius,
                arcCenter[2] - bisector[2] * radius
            };

            profileCurves.push_back(CreateArcThroughThreePoints(tangentEnds[index], arcMid, tangentStarts[next]));
        }
    }
    catch (...)
    {
        DeleteObjects(profileCurves);
        throw;
    }

    DeleteObjects(sourceCurves);
    {
        std::ostringstream log;
        log << "Rebuilt section profile with R=" << FormatDouble(radius)
            << ", profileCurves=" << profileCurves.size();
        DebugLog(log.str());
    }
    return profileCurves;
}

bool AskExtrudeLimitsFromHitEdge(tag_t edgeTag, const double planePoint[3], const double direction[3], double& startLimit, double& endLimit)
{
    double first[3] = {0.0, 0.0, 0.0};
    double second[3] = {0.0, 0.0, 0.0};
    if (!AskEdgeEndpoints(edgeTag, first, second))
    {
        return false;
    }

    double unitDirection[3] = {direction[0], direction[1], direction[2]};
    if (!Normalize3(unitDirection))
    {
        return false;
    }

    double firstDelta[3] =
    {
        first[0] - planePoint[0],
        first[1] - planePoint[1],
        first[2] - planePoint[2]
    };
    double secondDelta[3] =
    {
        second[0] - planePoint[0],
        second[1] - planePoint[1],
        second[2] - planePoint[2]
    };

    const double firstDistance = Dot3(firstDelta, unitDirection);
    const double secondDistance = Dot3(secondDelta, unitDirection);
    startLimit = std::min(firstDistance, secondDistance);
    endLimit = std::max(firstDistance, secondDistance);
    return std::fabs(endLimit - startLimit) > 0.01;
}

KeptSectionLoop KeepClosedIntersectionLoopNearPick(
    const std::vector<std::pair<tag_t, tag_t> >& curveFacePairs,
    const double pickPoint[3])
{
    std::vector<CurveSegmentInfo> infos;
    for (std::size_t index = 0; index < curveFacePairs.size(); ++index)
    {
        CurveSegmentInfo info = {};
        if (AskCurveSegmentInfo(curveFacePairs[index].first, curveFacePairs[index].second, info))
        {
            infos.push_back(info);
        }
        else if (curveFacePairs[index].first != NULL_TAG)
        {
            UF_OBJ_delete_object(curveFacePairs[index].first);
        }
    }

    std::vector<int> bestGroup;
    double bestDistance = DBL_MAX;
    std::vector<bool> visited(infos.size(), false);
    const double endpointTolerance = 0.5;

    for (std::size_t seed = 0; seed < infos.size(); ++seed)
    {
        if (visited[seed])
        {
            continue;
        }

        std::vector<int> stack;
        std::vector<int> group;
        stack.push_back(static_cast<int>(seed));
        visited[seed] = true;

        while (!stack.empty())
        {
            const int current = stack.back();
            stack.pop_back();
            group.push_back(current);

            for (std::size_t candidate = 0; candidate < infos.size(); ++candidate)
            {
                if (visited[candidate])
                {
                    continue;
                }

                const bool connected =
                    PointsCoincident(infos[current].start, infos[candidate].start, endpointTolerance) ||
                    PointsCoincident(infos[current].start, infos[candidate].end, endpointTolerance) ||
                    PointsCoincident(infos[current].end, infos[candidate].start, endpointTolerance) ||
                    PointsCoincident(infos[current].end, infos[candidate].end, endpointTolerance);
                if (connected)
                {
                    visited[candidate] = true;
                    stack.push_back(static_cast<int>(candidate));
                }
            }
        }

        bool closed = false;
        if (group.size() == 1 && infos[group.front()].closed)
        {
            closed = true;
        }
        else
        {
            int oddEndpoints = 0;
            for (std::size_t item = 0; item < group.size(); ++item)
            {
                const CurveSegmentInfo& source = infos[group[item]];
                int startMatches = 0;
                int endMatches = 0;
                for (std::size_t other = 0; other < group.size(); ++other)
                {
                    if (item == other)
                    {
                        continue;
                    }
                    const CurveSegmentInfo& target = infos[group[other]];
                    if (PointsCoincident(source.start, target.start, endpointTolerance) ||
                        PointsCoincident(source.start, target.end, endpointTolerance))
                    {
                        ++startMatches;
                    }
                    if (PointsCoincident(source.end, target.start, endpointTolerance) ||
                        PointsCoincident(source.end, target.end, endpointTolerance))
                    {
                        ++endMatches;
                    }
                }
                if (startMatches == 0)
                {
                    ++oddEndpoints;
                }
                if (endMatches == 0)
                {
                    ++oddEndpoints;
                }
            }
            closed = oddEndpoints == 0 && !group.empty();
        }

        if (closed)
        {
            const double distance = MinDistanceToCurveInfos(infos, group, pickPoint);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                bestGroup = group;
            }
        }
    }

    std::set<int> keepIndices(bestGroup.begin(), bestGroup.end());
    KeptSectionLoop kept = {};
    std::set<tag_t> keptFaces;
    for (std::size_t index = 0; index < infos.size(); ++index)
    {
        if (keepIndices.find(static_cast<int>(index)) != keepIndices.end())
        {
            kept.curveTags.push_back(infos[index].curveTag);
            if (infos[index].faceTag != NULL_TAG)
            {
                keptFaces.insert(infos[index].faceTag);
            }
        }
        else if (infos[index].curveTag != NULL_TAG)
        {
            UF_OBJ_delete_object(infos[index].curveTag);
        }
    }

    kept.faceTags.assign(keptFaces.begin(), keptFaces.end());
    return kept;
}

KeptSectionLoop CreateBodyIntersectionCurvesAtPick(
    tag_t bodyTag,
    const double pickPoint[3],
    const double nearEdgeDirection[3])
{
    KeptSectionLoop empty = {};
    std::vector<std::pair<tag_t, tag_t> > curveFacePairs;
    double planeNormal[3] = {nearEdgeDirection[0], nearEdgeDirection[1], nearEdgeDirection[2]};
    if (!Normalize3(planeNormal))
    {
        return empty;
    }

    tag_t planeTag = NULL_TAG;
    ThrowUfError(
        UF_MODL_create_plane(const_cast<double*>(pickPoint), planeNormal, &planeTag),
        "UF_MODL_create_plane");

    uf_list_p_t faceList = NULL;
    ThrowUfError(UF_MODL_ask_body_faces(bodyTag, &faceList), "UF_MODL_ask_body_faces");
    const std::vector<tag_t> faceTags = UfListToTags(faceList);
    UF_MODL_delete_list(&faceList);

    for (std::size_t faceIndex = 0; faceIndex < faceTags.size(); ++faceIndex)
    {
        int intersectionCount = 0;
        UF_MODL_intersect_info_p_t* intersections = NULL;
        const int rc = UF_MODL_intersect_objects(faceTags[faceIndex], planeTag, 0.001, &intersectionCount, &intersections);
        if (rc != 0)
        {
            if (intersections != NULL)
            {
                UF_free(intersections);
            }
            continue;
        }

        for (int index = 0; index < intersectionCount; ++index)
        {
            UF_MODL_intersect_info_p_t info = intersections[index];
            if (info == NULL)
            {
                continue;
            }

            if (info->intersect_type == UF_MODL_INTERSECT_CURVE)
            {
                const tag_t curveTag = info->intersect.curve.identifier;
                if (curveTag != NULL_TAG)
                {
                    curveFacePairs.push_back(std::make_pair(curveTag, faceTags[faceIndex]));
                }
            }

            UF_free(info);
        }

        if (intersections != NULL)
        {
            UF_free(intersections);
        }
    }

    UF_OBJ_delete_object(planeTag);
    return KeepClosedIntersectionLoopNearPick(curveFacePairs, pickPoint);
}

void ColorFacesYellow(const std::vector<tag_t>& faceTags)
{
    const int yellowColor = 6;
    for (std::size_t index = 0; index < faceTags.size(); ++index)
    {
        if (faceTags[index] != NULL_TAG)
        {
            UF_OBJ_set_color(faceTags[index], yellowColor);
        }
    }
}

void StylePreviewBody(tag_t bodyTag)
{
    if (bodyTag == NULL_TAG)
    {
        return;
    }

    int blueColor = 0;
    double blueRgb[3] = {0.0, 0.0, 1.0};
    if (UF_DISP_ask_closest_color(
            UF_DISP_rgb_model,
            blueRgb,
            UF_DISP_CCM_EUCLIDEAN_DISTANCE,
            &blueColor) != 0 ||
        blueColor <= 0)
    {
        blueColor = 211;
    }

    UF_OBJ_set_color(bodyTag, blueColor);
    UF_OBJ_set_line_width(bodyTag, UF_OBJ_WIDTH_THICK);
    UF_OBJ_set_font(bodyTag, UF_OBJ_FONT_SOLID);
    UF_OBJ_set_translucency(bodyTag, 100);

    NXOpen::DisplayableObject* displayObject =
        dynamic_cast<NXOpen::DisplayableObject*>(NXOpen::NXObjectManager::Get(bodyTag));
    NXOpen::Session* session = NXOpen::Session::GetSession();
    if (displayObject == NULL || session == NULL || session->DisplayManager() == NULL)
    {
        return;
    }

    NXOpen::DisplayModification* modification = session->DisplayManager()->NewDisplayModification();
    modification->SetApplyToAllFaces(true);
    modification->SetApplyToOwningParts(false);
    modification->SetNewColor(blueColor);
    modification->SetNewWidth(NXOpen::DisplayableObject::ObjectWidthThick);
    modification->SetNewFont(NXOpen::DisplayableObject::ObjectFontSolid);
    modification->SetNewTranslucency(100);
    modification->SetPartiallyShaded(false);
    std::vector<NXOpen::DisplayableObject*> objects;
    objects.push_back(displayObject);
    modification->Apply(objects);
    delete modification;

    NXOpen::Part* workPart = session->Parts() != NULL ? session->Parts()->Work() : NULL;
    NXOpen::View* workView = workPart != NULL && workPart->Views() != NULL ? workPart->Views()->WorkView() : NULL;
    if (workView != NULL && workView->DependentDisplay() != NULL)
    {
        workView->DependentDisplay()->ApplyShadeEdit(
            NXOpen::ViewDependentDisplayManager::PartialShadingNo,
            NXOpen::ViewDependentDisplayManager::TranslucencyYes,
            100,
            objects);
        workView->DependentDisplay()->ApplyWireframeEdit(
            NXOpen::ViewDependentDisplayManager::FontSolid,
            NXOpen::ViewDependentDisplayManager::WidthThick,
            objects);
        workView->Regenerate();
    }
}

double GetDoubleBlockValue(NXOpen::BlockStyler::DoubleBlock* block, double defaultValue)
{
    if (block == NULL)
    {
        return defaultValue;
    }

    NXOpen::BlockStyler::PropertyList* properties = NULL;
    try
    {
        properties = block->GetProperties();
        const double value = properties->GetDouble("Value");
        delete properties;
        properties = NULL;
        return value;
    }
    catch (...)
    {
        if (properties != NULL)
        {
            delete properties;
        }
        return defaultValue;
    }
}

class ChaiJiJiaDialog
{
public:
    ChaiJiJiaDialog()
        : ui(NXOpen::UI::GetUI()),
          dialog(NULL),
          faceSelection(NULL),
          squareTubeRValue(NULL),
          springbackThicknessValue(NULL),
          previewSelectionFace(NULL_TAG)
    {
        const std::string dlxPath =
            zhihui_embedded_dialog::ExtractDlxToRandomPath(IDR_ZH_DLX_CHAIJIJIA_DLX);
        if (dlxPath.empty())
        {
            throw std::runtime_error("ChaiJiJia dialog resource is missing.");
        }
        dialog = ui->CreateDialog(dlxPath.c_str());
        dialog->AddInitializeHandler(NXOpen::make_callback(this, &ChaiJiJiaDialog::Initialize));
        dialog->AddUpdateHandler(NXOpen::make_callback(this, &ChaiJiJiaDialog::Update));
        dialog->AddOkHandler(NXOpen::make_callback(this, &ChaiJiJiaDialog::Ok));
        dialog->AddApplyHandler(NXOpen::make_callback(this, &ChaiJiJiaDialog::Apply));
    }

    ~ChaiJiJiaDialog()
    {
        ClearPreview();
        if (dialog != NULL)
        {
            delete dialog;
            dialog = NULL;
        }
    }

    int Show()
    {
        return dialog->Launch();
    }

private:
    NXOpen::UI* ui;
    NXOpen::BlockStyler::BlockDialog* dialog;
    NXOpen::BlockStyler::SelectObject* faceSelection;
    NXOpen::BlockStyler::DoubleBlock* squareTubeRValue;
    NXOpen::BlockStyler::DoubleBlock* springbackThicknessValue;
    std::vector<tag_t> previewObjects;
    tag_t previewSelectionFace;

    static const wchar_t* DialogMemoryFileName()
    {
        return L"ChaiJiJia_state.ini";
    }

    void Initialize()
    {
        faceSelection = dynamic_cast<NXOpen::BlockStyler::SelectObject*>(
            dialog->TopBlock()->FindBlock("faceSelection"));
        squareTubeRValue = dynamic_cast<NXOpen::BlockStyler::DoubleBlock*>(
            dialog->TopBlock()->FindBlock("squareTubeRValue"));
        springbackThicknessValue = dynamic_cast<NXOpen::BlockStyler::DoubleBlock*>(
            dialog->TopBlock()->FindBlock("springbackThicknessValue"));
        LoadDialogMemory();

        if (faceSelection == NULL)
        {
            return;
        }

        NXOpen::Selection::SelectionAction action = NXOpen::Selection::SelectionActionClearAndEnableSpecific;
        std::vector<NXOpen::Selection::MaskTriple> masks;
        masks.push_back(NXOpen::Selection::MaskTriple(
            UF_solid_type,
            UF_solid_body_subtype,
            UF_UI_SEL_FEATURE_ANY_FACE));

        NXOpen::BlockStyler::PropertyList* properties = faceSelection->GetProperties();
        properties->SetSelectionFilter("SelectionFilter", action, masks);
        delete properties;
    }

    int Ok()
    {
        try
        {
            SaveDialogMemory();
            ClearPreview();
            DebugLog("========== ChaiJiJia command ==========");
            const double squareTubeR = GetDoubleBlockValue(squareTubeRValue, 1.0);
            const double springbackThickness = GetDoubleBlockValue(springbackThicknessValue, 1.0);
            {
                std::ostringstream log;
                log << "Dialog values: squareTubeR=" << FormatDouble(squareTubeR)
                    << " springbackThickness=" << FormatDouble(springbackThickness);
                DebugLog(log.str());
            }

            SelectedFaceInfo selected = GetSelectedFace();
            if (selected.face == NULL)
            {
                DebugLog("No selected face.");
                ui->NXMessageBox()->Show(
                    "ChaiJiJia",
                    NXOpen::NXMessageBox::DialogTypeInformation,
                    "Please select one face.");
                return 1;
            }

            tag_t bodyTag = NULL_TAG;
            ThrowUfError(UF_MODL_ask_face_body(selected.face->Tag(), &bodyTag), "UF_MODL_ask_face_body");
            if (bodyTag == NULL_TAG)
            {
                throw std::runtime_error("Selected face has no body.");
            }

            {
                std::ostringstream log;
                log << "Selected face=" << selected.face->Tag()
                    << " body=" << bodyTag
                    << " pick=" << FormatPoint(selected.pickPoint);
                DebugLog(log.str());
            }

            tag_t nearEdgeTag = NULL_TAG;
            double nearEdgeDirection[3] = {0.0, 0.0, 0.0};
            if (!FindNearestLinearFaceEdge(selected.face->Tag(), selected.pickPoint, nearEdgeTag, nearEdgeDirection))
            {
                DebugLog("Failed: no linear edge near pick point.");
                throw std::runtime_error("No linear edge was found near the pick point.");
            }

            {
                std::ostringstream log;
                log << "Nearest edge=" << nearEdgeTag
                    << " dir=" << FormatPoint(nearEdgeDirection);
                DebugLog(log.str());
            }

            const KeptSectionLoop keptLoop =
                CreateBodyIntersectionCurvesAtPick(bodyTag, selected.pickPoint, nearEdgeDirection);
            if (keptLoop.curveTags.empty())
            {
                DebugLog("Failed: no closed intersection loop was kept.");
                throw std::runtime_error("Failed to keep a closed intersection loop near the pick point.");
            }

            ColorFacesYellow(keptLoop.faceTags);
            double startLimit = 0.0;
            double endLimit = 0.0;
            if (!AskExtrudeLimitsFromHitEdge(nearEdgeTag, selected.pickPoint, nearEdgeDirection, startLimit, endLimit))
            {
                DebugLog("Failed: cannot compute extrude limits from hit edge endpoints.");
                throw std::runtime_error("Failed to compute extrusion limits from the hit edge.");
            }

            const std::vector<tag_t> profileCurves =
                RebuildSectionLoopCornersWithRadius(keptLoop.curveTags, squareTubeR);
            tag_t sectionBody = CreateExtrudedToolBody(
                profileCurves,
                nearEdgeDirection,
                startLimit,
                endLimit);

            std::ostringstream message;
            message << "Kept intersection curves: " << keptLoop.curveTags.size()
                    << ", yellow faces: " << keptLoop.faceTags.size()
                    << ", R profile curves: " << profileCurves.size()
                    << ", extrude body: " << sectionBody
                    << ", limits: " << FormatDouble(startLimit)
                    << " to " << FormatDouble(endLimit);
            DebugLog(message.str());
            ui->NXMessageBox()->Show(
                "ChaiJiJia",
                NXOpen::NXMessageBox::DialogTypeInformation,
                message.str().c_str());

            return 0;
        }
        catch (const NXOpen::NXException& ex)
        {
            ui->NXMessageBox()->Show("ChaiJiJia", NXOpen::NXMessageBox::DialogTypeError, ex.Message());
            return 1;
        }
        catch (const std::exception& ex)
        {
            ui->NXMessageBox()->Show("ChaiJiJia", NXOpen::NXMessageBox::DialogTypeError, ex.what());
            return 1;
        }
    }

    int Apply()
    {
        return Ok();
    }

    int Update(NXOpen::BlockStyler::UIBlock* block)
    {
        if (block == faceSelection || block == squareTubeRValue || block == springbackThicknessValue)
        {
            try
            {
                CreatePreview();
            }
            catch (const std::exception& ex)
            {
                DebugLog(std::string("Preview failed: ") + ex.what());
            }
            catch (...)
            {
                DebugLog("Preview failed: unknown exception.");
            }
        }

        return 0;
    }

    void LoadDialogMemory()
    {
        zhihui_dialog_memory::LoadDouble(DialogMemoryFileName(), L"squareTubeR", squareTubeRValue);
        zhihui_dialog_memory::LoadDouble(DialogMemoryFileName(), L"springbackThickness", springbackThicknessValue);
    }

    void SaveDialogMemory()
    {
        zhihui_dialog_memory::SaveDouble(DialogMemoryFileName(), L"squareTubeR", squareTubeRValue);
        zhihui_dialog_memory::SaveDouble(DialogMemoryFileName(), L"springbackThickness", springbackThicknessValue);
    }

    void ClearPreview()
    {
        DeleteObjects(previewObjects);
        previewObjects.clear();
        previewSelectionFace = NULL_TAG;
    }

    tag_t CreateSectionBodyFromCurrentSelection(bool previewMode, std::vector<tag_t>* ownedObjects)
    {
        const double squareTubeR = GetDoubleBlockValue(squareTubeRValue, 1.0);
        SelectedFaceInfo selected = GetSelectedFace();
        if (selected.face == NULL)
        {
            return NULL_TAG;
        }

        tag_t bodyTag = NULL_TAG;
        ThrowUfError(UF_MODL_ask_face_body(selected.face->Tag(), &bodyTag), "UF_MODL_ask_face_body");
        if (bodyTag == NULL_TAG)
        {
            throw std::runtime_error("Selected face has no body.");
        }

        tag_t nearEdgeTag = NULL_TAG;
        double nearEdgeDirection[3] = {0.0, 0.0, 0.0};
        if (!FindNearestLinearFaceEdge(selected.face->Tag(), selected.pickPoint, nearEdgeTag, nearEdgeDirection))
        {
            throw std::runtime_error("No linear edge was found near the pick point.");
        }

        const KeptSectionLoop keptLoop =
            CreateBodyIntersectionCurvesAtPick(bodyTag, selected.pickPoint, nearEdgeDirection);
        if (keptLoop.curveTags.empty())
        {
            throw std::runtime_error("Failed to keep a closed intersection loop near the pick point.");
        }

        if (!previewMode)
        {
            ColorFacesYellow(keptLoop.faceTags);
        }

        const std::vector<tag_t> profileCurves =
            RebuildSectionLoopCornersWithRadius(keptLoop.curveTags, squareTubeR);
        double startLimit = 0.0;
        double endLimit = 0.0;
        if (!AskExtrudeLimitsFromHitEdge(nearEdgeTag, selected.pickPoint, nearEdgeDirection, startLimit, endLimit))
        {
            DeleteObjects(profileCurves);
            throw std::runtime_error("Failed to compute extrusion limits from the hit edge.");
        }

        tag_t sectionBody = NULL_TAG;
        try
        {
            sectionBody = CreateExtrudedToolBody(
                profileCurves,
                nearEdgeDirection,
                startLimit,
                endLimit);
        }
        catch (...)
        {
            DeleteObjects(profileCurves);
            throw;
        }

        if (previewMode)
        {
            StylePreviewBody(sectionBody);
            if (ownedObjects != NULL)
            {
                ownedObjects->push_back(sectionBody);
                ownedObjects->insert(ownedObjects->end(), profileCurves.begin(), profileCurves.end());
            }
        }

        {
            std::ostringstream log;
            log << (previewMode ? "Preview" : "Commit")
                << " section body=" << sectionBody
                << ", profileCurves=" << profileCurves.size()
                << ", limits=" << FormatDouble(startLimit)
                << " to " << FormatDouble(endLimit);
            DebugLog(log.str());
        }

        return sectionBody;
    }

    void CreatePreview()
    {
        SelectedFaceInfo selected = GetSelectedFace();
        if (selected.face == NULL)
        {
            ClearPreview();
            return;
        }

        const double squareTubeR = GetDoubleBlockValue(squareTubeRValue, 1.0);
        std::vector<tag_t> newPreviewObjects;
        tag_t previewBody = NULL_TAG;
        try
        {
            previewBody = CreateSectionBodyFromCurrentSelection(true, &newPreviewObjects);
        }
        catch (...)
        {
            DeleteObjects(newPreviewObjects);
            throw;
        }

        ClearPreview();
        previewObjects = newPreviewObjects;
        previewSelectionFace = selected.face->Tag();
        (void)previewBody;
        (void)squareTubeR;
    }

    SelectedFaceInfo GetSelectedFace()
    {
        SelectedFaceInfo result = {};
        result.face = NULL;
        result.pickPoint[0] = 0.0;
        result.pickPoint[1] = 0.0;
        result.pickPoint[2] = 0.0;

        if (faceSelection == NULL)
        {
            return result;
        }

        NXOpen::BlockStyler::PropertyList* properties = faceSelection->GetProperties();
        std::vector<NXOpen::TaggedObject*> selectedObjects =
            properties->GetTaggedObjectVector("SelectedObjects");
        NXOpen::Point3d pickPoint = properties->GetPoint("PickPoint");
        delete properties;

        if (selectedObjects.empty())
        {
            return result;
        }

        result.face = dynamic_cast<NXOpen::Face*>(selectedObjects.front());
        result.pickPoint[0] = pickPoint.X;
        result.pickPoint[1] = pickPoint.Y;
        result.pickPoint[2] = pickPoint.Z;
        return result;
    }
};
}

extern "C" DllExport void ufusr(char* param, int* retcode, int param_len)
{
    (void)param;
    (void)param_len;

    if (retcode != NULL)
    {
        *retcode = 0;
    }

    if (!zhihui_license_guard::EnsureAuthorized(L"ZHIHUI.CHAIJIJIA", L"ChaiJiJia"))
    {
        if (retcode != NULL)
        {
            *retcode = 1;
        }
        return;
    }

    try
    {
        UF_initialize();
        ChaiJiJiaDialog commandDialog;
        commandDialog.Show();
        UF_terminate();
    }
    catch (const NXOpen::NXException& ex)
    {
        NXOpen::UI::GetUI()->NXMessageBox()->Show(
            "ChaiJiJia",
            NXOpen::NXMessageBox::DialogTypeError,
            ex.Message());
        if (retcode != NULL)
        {
            *retcode = ex.ErrorCode();
        }
        UF_terminate();
    }
    catch (const std::exception& ex)
    {
        NXOpen::UI::GetUI()->NXMessageBox()->Show(
            "ChaiJiJia",
            NXOpen::NXMessageBox::DialogTypeError,
            ex.what());
        if (retcode != NULL)
        {
            *retcode = 1;
        }
        UF_terminate();
    }
}

extern "C" DllExport int ufusr_ask_unload()
{
    return UF_UNLOAD_IMMEDIATELY;
}
