#include <NXOpen/Body.hxx>

#include <NXOpen/BlockStyler_BlockDialog.hxx>

#include <NXOpen/BlockStyler_DoubleBlock.hxx>

#include <NXOpen/BlockStyler_Enumeration.hxx>

#include <NXOpen/BlockStyler_PropertyList.hxx>

#include <NXOpen/BlockStyler_SelectObject.hxx>

#include <NXOpen/BlockStyler_UIBlock.hxx>

#include <NXOpen/Callback.hxx>

#include <NXOpen/DisplayableObject.hxx>

#include <NXOpen/Direction.hxx>

#include <NXOpen/DirectionCollection.hxx>

#include <NXOpen/Edge.hxx>

#include <NXOpen/Face.hxx>

#include <NXOpen/BodyDumbRule.hxx>

#include <NXOpen/Features_BlockFeatureBuilder.hxx>

#include <NXOpen/Features_BooleanBuilder.hxx>

#include <NXOpen/FaceDumbRule.hxx>

#include <NXOpen/Features_Feature.hxx>

#include <NXOpen/Features_FeatureCollection.hxx>

#include <NXOpen/Features_OffsetFaceBuilder.hxx>

#include <NXOpen/NXException.hxx>

#include <NXOpen/NXMessageBox.hxx>

#include <NXOpen/NXString.hxx>

#include <NXOpen/NXObjectManager.hxx>

#include <NXOpen/Part.hxx>

#include <NXOpen/PartCollection.hxx>

#include <NXOpen/ScCollector.hxx>

#include <NXOpen/ScCollectorCollection.hxx>

#include <NXOpen/ScRuleFactory.hxx>

#include <NXOpen/SelectFace.hxx>

#include <NXOpen/SelectionIntentRule.hxx>

#include <NXOpen/Selection.hxx>

#include <NXOpen/Session.hxx>

#include <NXOpen/SmartObject.hxx>

#include <NXOpen/TaggedObject.hxx>

#include <NXOpen/Expression.hxx>

#include <NXOpen/SelectBodyList.hxx>

#include <NXOpen/Tooling_StockSizeBuilder.hxx>

#include <NXOpen/Tooling_StockSizeCollection.hxx>

#include <NXOpen/Tooling_ToolingManager.hxx>

#include <NXOpen/UI.hxx>

#include <uf.h>

#include <uf_defs.h>

#include <uf_exit.h>

#include <uf_modl.h>

#include <uf_modl_curves.h>

#include <uf_modl_sweep.h>

#include <uf_modl_utilities.h>

#include <uf_obj.h>

#include <uf_object_types.h>

#include <uf_part.h>

#include <uf_curve.h>

#include <uf_disp.h>

#include <uf_ui_types.h>

#include <algorithm>

#include <cfloat>

#include <cmath>

#include <cstdlib>

#include <set>


#include <sstream>

#include <stdexcept>

#include <string>

#include <vector>

#include "../../../common/ZhihuiEmbeddedDialog.hpp"
#include "../embedded_dialog_resources.h"

#ifndef DllExport

#define DllExport __declspec(dllexport)

#endif

#ifndef WIN32_LEAN_AND_MEAN

#define WIN32_LEAN_AND_MEAN

#endif

#ifndef NOMINMAX

#define NOMINMAX

#endif

#include <windows.h>

#include <shellapi.h>

#ifdef CreateDialog

#undef CreateDialog

#endif

namespace zhihui_license_guard

{

typedef int(__stdcall* EnsureAuthorizedProc)(const wchar_t*, const wchar_t*, wchar_t*, int);

HMODULE LoadProtectedLicenseGate()

{

    const wchar_t* moduleName = L"ZhaoFuNxLicenseGate.dll";

    HMODULE existing = GetModuleHandleW(moduleName);

    if (existing != NULL)

    {

        return existing;

    }

    HMODULE selfModule = NULL;

    if (GetModuleHandleExW(

            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,

            reinterpret_cast<LPCWSTR>(&LoadProtectedLicenseGate),

            &selfModule))

    {

        wchar_t localPath[MAX_PATH] = {0};

        DWORD length = GetModuleFileNameW(selfModule, localPath, MAX_PATH);

        if (length > 0 && length < MAX_PATH)

        {

            DWORD slash = length;

            while (slash > 0 && localPath[slash - 1] != L'\\' && localPath[slash - 1] != L'/')

            {

                --slash;

            }

            if (slash > 0)

            {

                DWORD pos = slash;

                for (DWORD i = 0; moduleName[i] != L'\0' && pos + 1 < MAX_PATH; ++i, ++pos)

                {

                    localPath[pos] = moduleName[i];

                }

                localPath[pos] = L'\0';

                HMODULE localModule = LoadLibraryW(localPath);

                if (localModule != NULL)

                {

                    return localModule;

                }

            }

        }

    }

    HMODULE fixedModule = LoadLibraryW(L"D:\\UG\u667A\u8F89\u94A3\u91D1\u63D2\u4EF6\\application\\ZhaoFuNxLicenseGate.dll");

    if (fixedModule != NULL)

    {

        return fixedModule;

    }

    return LoadLibraryW(moduleName);

}

bool EnsureAuthorized(const wchar_t* featureCode, const wchar_t* displayName)

{

    wchar_t message[1024] = {0};

    HMODULE module = LoadProtectedLicenseGate();

    if (module == NULL)

    {

        return false;

    }

    EnsureAuthorizedProc ensureAuthorized =

        reinterpret_cast<EnsureAuthorizedProc>(GetProcAddress(module, "ZfnxEnsureAuthorized"));

    if (ensureAuthorized == NULL)

    {

        return false;

    }

    const int ok = ensureAuthorized(

        featureCode,

        displayName,

        message,

        static_cast<int>(sizeof(message) / sizeof(message[0])));

    return ok == 1;

}

}

namespace

{

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

void HideProcessObject(tag_t objectTag)

{

    if (objectTag == NULL_TAG)

    {

        return;

    }

    UF_OBJ_set_blank_status(objectTag, UF_OBJ_BLANKED);

}

class DisplaySuppressionGuard

{

public:

    DisplaySuppressionGuard()

        : originalDisplayCode(UF_DISP_UNSUPPRESS_DISPLAY),

          suppressed(false)

    {

        if (UF_DISP_ask_display(&originalDisplayCode) == 0 &&

            originalDisplayCode == UF_DISP_UNSUPPRESS_DISPLAY &&

            UF_DISP_set_display(UF_DISP_SUPPRESS_DISPLAY) == 0)

        {

            suppressed = true;

        }

    }

    ~DisplaySuppressionGuard()

    {

        if (suppressed)

        {

            UF_DISP_set_display(originalDisplayCode);

        }

    }

private:

    int originalDisplayCode;

    bool suppressed;

};

double Distance3(const double lhs[3], const double rhs[3])

{

    const double dx = rhs[0] - lhs[0];

    const double dy = rhs[1] - lhs[1];

    const double dz = rhs[2] - lhs[2];

    return std::sqrt(dx * dx + dy * dy + dz * dz);

}

double Dot3(const double lhs[3], const double rhs[3])

{

    return lhs[0] * rhs[0] + lhs[1] * rhs[1] + lhs[2] * rhs[2];

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

    if (parameter < 0.0)

    {

        parameter = 0.0;

    }

    else if (parameter > 1.0)

    {

        parameter = 1.0;

    }

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

struct FacePlacement

{

    tag_t faceTag;

    tag_t bodyTag;

    double center[3];

    double normal[3];

    double widthAxis[3];

    double lengthAxis[3];

    double lengthMin;

    double lengthMax;

    double widthMin;

    double widthMax;

    double endCoord;

    double inwardSign;

    double wallThickness;

    double toolDepth;

};

struct TouchingPortPlacement

{

    FacePlacement malePlacement;

    tag_t femaleBodyTag;

};

struct SelectedFaceInfo

{

    NXOpen::Face* face;

    double pickPoint[3];

};

std::vector<tag_t> TagsFromUfList(uf_list_p_t list)

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

bool AskFaceEdgeEndpointPairs(tag_t faceTag, std::vector<EdgeEndpointPair>& endpointPairs)

{

    endpointPairs.clear();

    uf_list_p_t edgeList = NULL;

    if (UF_MODL_ask_face_edges(faceTag, &edgeList) != 0)

    {

        return false;

    }

    std::vector<tag_t> edgeTags = UfListToTags(edgeList);

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

    if (pointCount <= 0)

    {

        return false;

    }

    center[0] /= static_cast<double>(pointCount);

    center[1] /= static_cast<double>(pointCount);

    center[2] /= static_cast<double>(pointCount);

    return true;

}

bool AskFaceNormalFromDirectionCollection(tag_t faceTag, double normal[3])

{

    NXOpen::Face* face =

        dynamic_cast<NXOpen::Face*>(NXOpen::NXObjectManager::Get(faceTag));

    if (face == NULL)

    {

        return false;

    }

    NXOpen::Session* session = NXOpen::Session::GetSession();

    if (session == NULL || session->Parts() == NULL || session->Parts()->Work() == NULL)

    {

        return false;

    }

    NXOpen::Direction* direction =

        session->Parts()->Work()->Directions()->CreateDumbDirectionFace(

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

bool AskPlanarFaceCenterAndEdgeNormal(tag_t faceTag, double center[3], double normal[3])

{

    int faceType = 0;

    double point[3] = {0.0, 0.0, 0.0};

    double direction[3] = {0.0, 0.0, 0.0};

    double faceBox[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};

    double radius = 0.0;

    double radData = 0.0;

    int normalDir = 0;

    if (UF_MODL_ask_face_data(faceTag, &faceType, point, direction, faceBox, &radius, &radData, &normalDir) != 0 ||

        faceType != UF_MODL_PLANAR_FACE)

    {

        return false;

    }

    std::vector<EdgeEndpointPair> endpointPairs;

    return AskFaceEdgeEndpointPairs(faceTag, endpointPairs) &&

        ComputeFaceCenterFromEndpoints(endpointPairs, center) &&

        AskFaceNormalFromDirectionCollection(faceTag, normal);

}

void AddScaledVector(const double origin[3], const double direction[3], double scale, double result[3])

{

    result[0] = origin[0] + direction[0] * scale;

    result[1] = origin[1] + direction[1] * scale;

    result[2] = origin[2] + direction[2] * scale;

}

bool IntersectLines3(

    const double firstPoint[3],

    const double firstDirection[3],

    const double secondPoint[3],

    const double secondDirection[3],

    double intersection[3])

{

    double cross[3] = {0.0, 0.0, 0.0};

    Cross3(firstDirection, secondDirection, cross);

    const double denominator = Dot3(cross, cross);

    if (denominator < 1.0e-9)

    {

        return false;

    }

    double pointDelta[3] =

    {

        secondPoint[0] - firstPoint[0],

        secondPoint[1] - firstPoint[1],

        secondPoint[2] - firstPoint[2]

    };

    double deltaCrossSecond[3] = {0.0, 0.0, 0.0};

    Cross3(pointDelta, secondDirection, deltaCrossSecond);

    const double parameter = Dot3(deltaCrossSecond, cross) / denominator;

    AddScaledVector(firstPoint, firstDirection, parameter, intersection);

    double secondLineEnd[3] = {0.0, 0.0, 0.0};

    AddScaledVector(secondPoint, secondDirection, 1.0, secondLineEnd);

    return DistancePointToLine3(intersection, secondPoint, secondLineEnd) <= 0.05;

}
bool AskPlanarFacePlacement(NXOpen::Face* face, const double pickPoint[3], FacePlacement& placement, bool useWholeBodyEnds = true)

{

    if (face == NULL)

    {

        return false;

    }

    placement.faceTag = face->Tag();

    ThrowUfError(UF_MODL_ask_face_body(placement.faceTag, &placement.bodyTag), "UF_MODL_ask_face_body");

    if (placement.bodyTag == NULL_TAG)

    {

        return false;

    }

    int faceType = 0;

    double point[3] = {0.0, 0.0, 0.0};

    double direction[3] = {0.0, 0.0, 0.0};

    double faceBox[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};

    double radius = 0.0;

    double radData = 0.0;

    int normalDir = 0;

    ThrowUfError(

        UF_MODL_ask_face_data(placement.faceTag, &faceType, point, direction, faceBox, &radius, &radData, &normalDir),

        "UF_MODL_ask_face_data");

    if (faceType != UF_MODL_PLANAR_FACE)

    {

        return false;

    }

    std::vector<EdgeEndpointPair> endpointPairs;

    if (!AskFaceEdgeEndpointPairs(placement.faceTag, endpointPairs) ||

        !AskFaceNormalFromDirectionCollection(placement.faceTag, placement.normal))

    {

        return false;

    }

    double bestLength = 0.0;

    double bestAxis[3] = {0.0, 0.0, 0.0};

    for (std::size_t index = 0; index < endpointPairs.size(); ++index)

    {

        double edgeDirection[3] =

        {

            endpointPairs[index].second[0] - endpointPairs[index].first[0],

            endpointPairs[index].second[1] - endpointPairs[index].first[1],

            endpointPairs[index].second[2] - endpointPairs[index].first[2]

        };

        const double length = Distance3(endpointPairs[index].first, endpointPairs[index].second);

        if (length > bestLength && Normalize3(edgeDirection))

        {

            bestLength = length;

            bestAxis[0] = edgeDirection[0];

            bestAxis[1] = edgeDirection[1];

            bestAxis[2] = edgeDirection[2];

        }

    }

    if (!ComputeFaceCenterFromEndpoints(endpointPairs, placement.center))

    {

        placement.center[0] = (faceBox[0] + faceBox[3]) * 0.5;

        placement.center[1] = (faceBox[1] + faceBox[4]) * 0.5;

        placement.center[2] = (faceBox[2] + faceBox[5]) * 0.5;

    }

    if (bestLength > 1.0e-6)

    {

            placement.lengthAxis[0] = bestAxis[0];

            placement.lengthAxis[1] = bestAxis[1];

            placement.lengthAxis[2] = bestAxis[2];

    }

    else

    {

        double worldX[3] = {1.0, 0.0, 0.0};

        Cross3(placement.normal, worldX, placement.lengthAxis);

        if (!Normalize3(placement.lengthAxis))

        {

            double worldY[3] = {0.0, 1.0, 0.0};

            Cross3(placement.normal, worldY, placement.lengthAxis);

        }

    }

    if (!Normalize3(placement.lengthAxis))

    {

        return false;

    }

    double bestOppositeDistance = DBL_MAX;

    double bestOppositeSignedDistance = 0.0;

    uf_list_p_t orientationFaceList = NULL;

    ThrowUfError(UF_MODL_ask_body_faces(placement.bodyTag, &orientationFaceList), "UF_MODL_ask_body_faces");

    std::vector<tag_t> orientationFaces = UfListToTags(orientationFaceList);

    UF_MODL_delete_list(&orientationFaceList);

    for (std::size_t index = 0; index < orientationFaces.size(); ++index)

    {

        if (orientationFaces[index] == placement.faceTag)

        {

            continue;

        }

        double otherCenter[3] = {0.0, 0.0, 0.0};

        double otherNormal[3] = {0.0, 0.0, 0.0};

        if (!AskPlanarFaceCenterAndEdgeNormal(orientationFaces[index], otherCenter, otherNormal))

        {

            continue;

        }

        if (std::fabs(Dot3(otherNormal, placement.normal)) < 0.95)

        {

            continue;

        }

        double delta[3] =

        {

            otherCenter[0] - placement.center[0],

            otherCenter[1] - placement.center[1],

            otherCenter[2] - placement.center[2]

        };

        const double signedDistance = Dot3(delta, placement.normal);

        const double distance = std::fabs(signedDistance);

        if (distance > 0.05 && distance < bestOppositeDistance)

        {

            bestOppositeDistance = distance;

            bestOppositeSignedDistance = signedDistance;

        }

    }

    if (bestOppositeDistance < DBL_MAX && bestOppositeSignedDistance > 0.0)

    {

        placement.normal[0] = -placement.normal[0];

        placement.normal[1] = -placement.normal[1];

        placement.normal[2] = -placement.normal[2];

    }

    Cross3(placement.normal, placement.lengthAxis, placement.widthAxis);

    if (!Normalize3(placement.widthAxis))

    {

        return false;

    }

    double faceLengthMin = DBL_MAX;

    double faceLengthMax = -DBL_MAX;

    placement.widthMin = DBL_MAX;

    placement.widthMax = -DBL_MAX;

    if (!endpointPairs.empty())

    {

        for (std::size_t index = 0; index < endpointPairs.size(); ++index)

        {

            const double lengthProjection1 = Dot3(endpointPairs[index].first, placement.lengthAxis);

            const double lengthProjection2 = Dot3(endpointPairs[index].second, placement.lengthAxis);

            faceLengthMin = std::min(faceLengthMin, std::min(lengthProjection1, lengthProjection2));

            faceLengthMax = std::max(faceLengthMax, std::max(lengthProjection1, lengthProjection2));

            const double widthProjection1 = Dot3(endpointPairs[index].first, placement.widthAxis);

            const double widthProjection2 = Dot3(endpointPairs[index].second, placement.widthAxis);

            placement.widthMin = std::min(placement.widthMin, std::min(widthProjection1, widthProjection2));

            placement.widthMax = std::max(placement.widthMax, std::max(widthProjection1, widthProjection2));

        }

    }

    else

    {

        return false;

    }

    placement.lengthMin = faceLengthMin;

    placement.lengthMax = faceLengthMax;

    if (useWholeBodyEnds)

    {

        // Auto mode works on whole tubes, so use the body ends along the tube length.

        double bodyLengthMin = DBL_MAX;

        double bodyLengthMax = -DBL_MAX;

        uf_list_p_t bodyEdgeList = NULL;

        ThrowUfError(UF_MODL_ask_body_edges(placement.bodyTag, &bodyEdgeList), "UF_MODL_ask_body_edges");

        std::vector<tag_t> bodyEdgeTags = UfListToTags(bodyEdgeList);

        UF_MODL_delete_list(&bodyEdgeList);

        for (std::size_t index = 0; index < bodyEdgeTags.size(); ++index)

        {

            double point1[3] = {0.0, 0.0, 0.0};

            double point2[3] = {0.0, 0.0, 0.0};

            int vertexCount = 0;

            ThrowUfError(UF_MODL_ask_edge_verts(bodyEdgeTags[index], point1, point2, &vertexCount), "UF_MODL_ask_edge_verts");

            if (vertexCount != 2)

            {

                continue;

            }

            const double bodyProjection1 = Dot3(point1, placement.lengthAxis);

            const double bodyProjection2 = Dot3(point2, placement.lengthAxis);

            bodyLengthMin = std::min(bodyLengthMin, std::min(bodyProjection1, bodyProjection2));

            bodyLengthMax = std::max(bodyLengthMax, std::max(bodyProjection1, bodyProjection2));

        }

        if (bodyLengthMin != DBL_MAX && bodyLengthMax != -DBL_MAX)

        {

            placement.lengthMin = bodyLengthMin;

            placement.lengthMax = bodyLengthMax;

        }

    }

    const double pickProjection = Dot3(pickPoint, placement.lengthAxis);

    bool pickedEndEdgeFound = false;

    double pickedEndCoord = 0.0;

    if (!useWholeBodyEnds)

    {

        double bestDistance = DBL_MAX;

        for (std::size_t index = 0; index < endpointPairs.size(); ++index)

        {

            double edgeDirection[3] =

            {

                endpointPairs[index].second[0] - endpointPairs[index].first[0],

                endpointPairs[index].second[1] - endpointPairs[index].first[1],

                endpointPairs[index].second[2] - endpointPairs[index].first[2]

            };

            if (!Normalize3(edgeDirection))

            {

                continue;

            }

            const double lengthSpan = std::fabs(

                Dot3(endpointPairs[index].second, placement.lengthAxis) -

                Dot3(endpointPairs[index].first, placement.lengthAxis));

            const double widthSpan = std::fabs(

                Dot3(endpointPairs[index].second, placement.widthAxis) -

                Dot3(endpointPairs[index].first, placement.widthAxis));

            const double lengthAlignment = std::fabs(Dot3(edgeDirection, placement.lengthAxis));

            const bool isEndEdge =

                widthSpan > 0.1 &&

                lengthSpan <= std::max(0.5, widthSpan * 0.35) &&

                lengthAlignment < 0.65;

            if (!isEndEdge)

            {

                continue;

            }

            const double distance = DistancePointToSegment3(pickPoint, endpointPairs[index].first, endpointPairs[index].second);

            if (distance < bestDistance)

            {

                bestDistance = distance;

                pickedEndCoord =

                    (Dot3(endpointPairs[index].first, placement.lengthAxis) +

                     Dot3(endpointPairs[index].second, placement.lengthAxis)) * 0.5;

                pickedEndEdgeFound = true;

            }

        }

    }

    if (pickedEndEdgeFound)

    {

        placement.endCoord = pickedEndCoord;

        if (std::fabs(pickedEndCoord - faceLengthMin) <= std::fabs(pickedEndCoord - faceLengthMax))

        {

            placement.inwardSign = 1.0;

        }

        else

        {

            placement.inwardSign = -1.0;

        }

    }

    else if (std::fabs(pickProjection - placement.lengthMin) <= std::fabs(pickProjection - placement.lengthMax))

    {

        placement.endCoord = placement.lengthMin;

        placement.inwardSign = 1.0;

    }

    else

    {

        placement.endCoord = placement.lengthMax;

        placement.inwardSign = -1.0;

    }

    double bodyBox[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};

    ThrowUfError(UF_MODL_ask_bounding_box(placement.bodyTag, bodyBox), "UF_MODL_ask_bounding_box");

    placement.wallThickness = bestOppositeDistance;

    uf_list_p_t faceList = NULL;

    ThrowUfError(UF_MODL_ask_body_faces(placement.bodyTag, &faceList), "UF_MODL_ask_body_faces");

    std::vector<tag_t> bodyFaces = UfListToTags(faceList);

    UF_MODL_delete_list(&faceList);

    for (std::size_t index = 0; index < bodyFaces.size(); ++index)

    {

        if (bodyFaces[index] == placement.faceTag)

        {

            continue;

        }

        double otherCenter[3] = {0.0, 0.0, 0.0};

        double otherNormal[3] = {0.0, 0.0, 0.0};

        if (!AskPlanarFaceCenterAndEdgeNormal(bodyFaces[index], otherCenter, otherNormal) ||

            std::fabs(Dot3(otherNormal, placement.normal)) < 0.95)

        {

            continue;

        }

        double delta[3] =

        {

            otherCenter[0] - placement.center[0],

            otherCenter[1] - placement.center[1],

            otherCenter[2] - placement.center[2]

        };

        const double distance = std::fabs(Dot3(delta, placement.normal));

        if (distance > 0.05 && distance < placement.wallThickness)

        {

            placement.wallThickness = distance;

        }

    }

    if (placement.wallThickness == DBL_MAX)

    {

        const double dx = bodyBox[3] - bodyBox[0];

        const double dy = bodyBox[4] - bodyBox[1];

        const double dz = bodyBox[5] - bodyBox[2];

        placement.wallThickness = std::max(0.5, std::min(dx, std::min(dy, dz)) * 0.05);

    }

    placement.toolDepth = placement.wallThickness + 1.0;

    return true;

}

bool AskPickDirectedLongestFaceAxis(tag_t faceTag, const double pickPoint[3], double xAxis[3])

{

    std::vector<EdgeEndpointPair> endpointPairs;

    if (!AskFaceEdgeEndpointPairs(faceTag, endpointPairs))

    {

        return false;

    }

    double maxLength = 0.0;

    for (std::size_t index = 0; index < endpointPairs.size(); ++index)

    {

        maxLength = std::max(

            maxLength,

            Distance3(endpointPairs[index].first, endpointPairs[index].second));

    }

    if (maxLength <= 1.0e-6)

    {

        return false;

    }

    const EdgeEndpointPair* bestPair = NULL;

    double bestPickDistance = DBL_MAX;

    for (std::size_t index = 0; index < endpointPairs.size(); ++index)

    {

        const double edgeLength = Distance3(endpointPairs[index].first, endpointPairs[index].second);

        if (edgeLength < maxLength * 0.98)

        {

            continue;

        }

        const double pickDistance =

            DistancePointToSegment3(pickPoint, endpointPairs[index].first, endpointPairs[index].second);

        if (pickDistance < bestPickDistance)

        {

            bestPickDistance = pickDistance;

            bestPair = &endpointPairs[index];

        }

    }

    if (bestPair == NULL)

    {

        return false;

    }

    const double firstDistance = Distance3(pickPoint, bestPair->first);

    const double secondDistance = Distance3(pickPoint, bestPair->second);

    const double* farPoint = firstDistance > secondDistance ? bestPair->first : bestPair->second;

    const double* nearPoint = firstDistance > secondDistance ? bestPair->second : bestPair->first;

    for (int axis = 0; axis < 3; ++axis)

    {

        xAxis[axis] = nearPoint[axis] - farPoint[axis];

    }

    return Normalize3(xAxis);

}

tag_t CreateSelectedBodyStockBox(

    const SelectedFaceInfo& selection,

    double positiveXOffset)

{

    if (selection.face == NULL)

    {

        return NULL_TAG;

    }

    NXOpen::Session* session = NXOpen::Session::GetSession();

    NXOpen::Part* workPart =

        session != NULL && session->Parts() != NULL ? session->Parts()->Work() : NULL;

    if (workPart == NULL)

    {

        return NULL_TAG;

    }

    FacePlacement placement = {};

    if (!AskPlanarFacePlacement(selection.face, selection.pickPoint, placement, false))

    {

        return NULL_TAG;

    }

    NXOpen::Body* body =

        dynamic_cast<NXOpen::Body*>(NXOpen::NXObjectManager::Get(placement.bodyTag));

    if (body == NULL)

    {

        return NULL_TAG;

    }

    double zAxis[3] = {placement.normal[0], placement.normal[1], placement.normal[2]};

    if (!Normalize3(zAxis))

    {

        return NULL_TAG;

    }

    double xAxis[3] = {0.0, 0.0, 0.0};

    if (!AskPickDirectedLongestFaceAxis(selection.face->Tag(), selection.pickPoint, xAxis))

    {

        xAxis[0] = placement.lengthAxis[0];

        xAxis[1] = placement.lengthAxis[1];

        xAxis[2] = placement.lengthAxis[2];

    }

    const double xOnZ = Dot3(xAxis, zAxis);

    for (int axis = 0; axis < 3; ++axis)

    {

        xAxis[axis] -= zAxis[axis] * xOnZ;

    }

    if (!Normalize3(xAxis))

    {

        return NULL_TAG;

    }

    double yAxis[3] = {0.0, 0.0, 0.0};

    Cross3(zAxis, xAxis, yAxis);

    if (!Normalize3(yAxis))

    {

        return NULL_TAG;

    }

    NXOpen::Matrix3x3 matrix = {};

    matrix.Xx = xAxis[0];

    matrix.Xy = xAxis[1];

    matrix.Xz = xAxis[2];

    matrix.Yx = yAxis[0];

    matrix.Yy = yAxis[1];

    matrix.Yz = yAxis[2];

    matrix.Zx = zAxis[0];

    matrix.Zy = zAxis[1];

    matrix.Zz = zAxis[2];

    (void)matrix;

    uf_list_p_t edgeList = NULL;

    if (UF_MODL_ask_body_edges(body->Tag(), &edgeList) != 0 || edgeList == NULL)

    {

        return NULL_TAG;

    }

    const std::vector<tag_t> edgeTags = UfListToTags(edgeList);

    UF_MODL_delete_list(&edgeList);

    bool hasPoint = false;

    double minX = DBL_MAX;

    double maxX = -DBL_MAX;

    double minY = DBL_MAX;

    double maxY = -DBL_MAX;

    double minZ = DBL_MAX;

    double maxZ = -DBL_MAX;

    for (std::size_t index = 0; index < edgeTags.size(); ++index)

    {

        double point1[3] = {0.0, 0.0, 0.0};

        double point2[3] = {0.0, 0.0, 0.0};

        int vertexCount = 0;

        if (UF_MODL_ask_edge_verts(edgeTags[index], point1, point2, &vertexCount) != 0 ||

            vertexCount <= 0)

        {

            continue;

        }

        const double* points[2] = {point1, point2};

        const int pointCount = vertexCount >= 2 ? 2 : 1;

        for (int pointIndex = 0; pointIndex < pointCount; ++pointIndex)

        {

            const double x = Dot3(points[pointIndex], xAxis);

            const double y = Dot3(points[pointIndex], yAxis);

            const double z = Dot3(points[pointIndex], zAxis);

            minX = std::min(minX, x);

            maxX = std::max(maxX, x);

            minY = std::min(minY, y);

            maxY = std::max(maxY, y);

            minZ = std::min(minZ, z);

            maxZ = std::max(maxZ, z);

            hasPoint = true;

        }

    }

    if (!hasPoint)

    {

        return NULL_TAG;

    }

    maxX += std::max(0.0, positiveXOffset);

    const double length = maxX - minX;

    const double width = maxY - minY;

    const double height = maxZ - minZ;

    if (length <= 0.05 || width <= 0.05 || height <= 0.05)

    {

        return NULL_TAG;

    }

    NXOpen::Point3d origin;

    origin.X = xAxis[0] * minX + yAxis[0] * minY + zAxis[0] * minZ;

    origin.Y = xAxis[1] * minX + yAxis[1] * minY + zAxis[1] * minZ;

    origin.Z = xAxis[2] * minX + yAxis[2] * minY + zAxis[2] * minZ;

    NXOpen::Vector3d nxXAxis = {xAxis[0], xAxis[1], xAxis[2]};

    NXOpen::Vector3d nxYAxis = {yAxis[0], yAxis[1], yAxis[2]};

    NXOpen::Features::BlockFeatureBuilder* blockBuilder =

        workPart->Features()->CreateBlockFeatureBuilder(NULL);

    if (blockBuilder == NULL)

    {

        return NULL_TAG;

    }

    NXOpen::NXObject* blockObject = NULL;

    tag_t blockBodyTag = NULL_TAG;

    try

    {

        blockBuilder->SetType(NXOpen::Features::BlockFeatureBuilder::TypesOriginAndEdgeLengths);

        blockBuilder->SetOrigin(origin);

        blockBuilder->SetOrientation(nxXAxis, nxYAxis);

        blockBuilder->SetLength(FormatDouble(length).c_str());

        blockBuilder->SetWidth(FormatDouble(width).c_str());

        blockBuilder->SetHeight(FormatDouble(height).c_str());

        blockBuilder->SetBooleanOperationAndTarget(

            NXOpen::Features::Feature::BooleanTypeCreate,

            NULL);

        blockObject = blockBuilder->Commit();

        NXOpen::Body* blockBody = dynamic_cast<NXOpen::Body*>(blockObject);

        if (blockBody != NULL)

        {

            blockBodyTag = blockBody->Tag();

        }

        else if (blockObject != NULL)

        {

            UF_MODL_ask_feat_body(blockObject->Tag(), &blockBodyTag);

        }

    }

    catch (...)

    {

        blockBuilder->Destroy();

        throw;

    }

    blockBuilder->Destroy();

    HideProcessObject(blockBodyTag);

    return blockBodyTag;

}

struct FaceProfilePoint

{

    double length;

    double width;

};

tag_t CreateLineBetweenPoints(const double startPoint[3], const double endPoint[3])

{

    UF_CURVE_line_t lineData = {};

    for (int axis = 0; axis < 3; ++axis)

    {

        lineData.start_point[axis] = startPoint[axis];

        lineData.end_point[axis] = endPoint[axis];

    }

    tag_t lineTag = NULL_TAG;

    ThrowUfError(UF_CURVE_create_line(&lineData, &lineTag), "UF_CURVE_create_line");

    return lineTag;

}

double AskTubeOuterCornerRadius(const FacePlacement& placement);

tag_t CreateExtrudedToolBody(const std::vector<tag_t>& profileCurves, const double direction[3], double startLimitValue, double endLimitValue);

bool TrySubtractToolBody(tag_t& targetBody, tag_t toolBody, const char* label);

bool TryUniteToolBody(tag_t& targetBody, tag_t toolBody, const char* label);

void DeleteObjects(const std::vector<tag_t>& tags);

bool FindNearestPlanarEndFaceByPickAlongAxis(

    tag_t bodyTag,

    const double pickPoint[3],

    const double lengthAxis[3],

    tag_t& endFaceTag);

bool AskPlanarFacePlane(tag_t faceTag, double point[3], double normal[3]);

NXOpen::Face* FaceFromTag(tag_t faceTag);

bool AskFaceInnerNormalFromNxDirection(NXOpen::Face* face, tag_t bodyTag, const double referencePoint[3], double innerNormal[3]);

std::vector<tag_t> CollectFaceChainAlongLongEdges(tag_t seedFaceTag, tag_t bodyTag, const double lengthAxis[3]);

double AskFaceChainDistancePerpendicularToPlane(const std::vector<tag_t>& faceChain, const double planeNormal[3]);

void RedisplayTag(tag_t objectTag);

bool IsLinearEdge(tag_t edgeTag)

{

    int edgeType = 0;

    return edgeTag != NULL_TAG &&

        UF_MODL_ask_edge_type(edgeTag, &edgeType) == 0 &&

        edgeType == UF_MODL_LINEAR_EDGE;

}

bool FindSecondTubeEdgeCoincidentWithFirstEnd(

    tag_t firstEndFaceTag,

    tag_t secondEndFaceTag,

    double firstPoint[3],

    double secondPoint[3])

{

    double firstFacePoint[3] = {0.0, 0.0, 0.0};

    double firstFaceNormal[3] = {0.0, 0.0, 0.0};

    if (!AskPlanarFacePlane(firstEndFaceTag, firstFacePoint, firstFaceNormal))

    {

        return false;

    }

    double secondFacePoint[3] = {0.0, 0.0, 0.0};

    double secondFaceNormal[3] = {0.0, 0.0, 0.0};

    if (!AskPlanarFacePlane(secondEndFaceTag, secondFacePoint, secondFaceNormal))

    {

        return false;

    }

    std::vector<EdgeEndpointPair> firstEndpointPairs;

    std::vector<EdgeEndpointPair> secondEndpointPairs;

    if (!AskFaceEdgeEndpointPairs(firstEndFaceTag, firstEndpointPairs) ||

        !AskFaceEdgeEndpointPairs(secondEndFaceTag, secondEndpointPairs))

    {

        return false;

    }

    const double collinearTolerance = 0.25;

    double bestScore = DBL_MAX;

    bool found = false;

    for (std::size_t secondIndex = 0; secondIndex < secondEndpointPairs.size(); ++secondIndex)

    {

        if (!IsLinearEdge(secondEndpointPairs[secondIndex].edgeTag))

        {

            continue;

        }

        double secondDirection[3] =

        {

            secondEndpointPairs[secondIndex].second[0] - secondEndpointPairs[secondIndex].first[0],

            secondEndpointPairs[secondIndex].second[1] - secondEndpointPairs[secondIndex].first[1],

            secondEndpointPairs[secondIndex].second[2] - secondEndpointPairs[secondIndex].first[2]

        };

        const double secondLength = std::sqrt(Dot3(secondDirection, secondDirection));

        if (secondLength <= 1.0e-6 || !Normalize3(secondDirection))

        {

            continue;

        }

        for (std::size_t firstIndex = 0; firstIndex < firstEndpointPairs.size(); ++firstIndex)

        {

            if (!IsLinearEdge(firstEndpointPairs[firstIndex].edgeTag))

            {

                continue;

            }

            double firstDirection[3] =

            {

                firstEndpointPairs[firstIndex].second[0] - firstEndpointPairs[firstIndex].first[0],

                firstEndpointPairs[firstIndex].second[1] - firstEndpointPairs[firstIndex].first[1],

                firstEndpointPairs[firstIndex].second[2] - firstEndpointPairs[firstIndex].first[2]

            };

            const double firstLength = std::sqrt(Dot3(firstDirection, firstDirection));

            if (firstLength <= 1.0e-6 || !Normalize3(firstDirection))

            {

                continue;

            }

            if (std::fabs(Dot3(firstDirection, secondDirection)) < 0.995)

            {

                continue;

            }

            const double distance1 =

                DistancePointToLine3(

                    secondEndpointPairs[secondIndex].first,

                    firstEndpointPairs[firstIndex].first,

                    firstEndpointPairs[firstIndex].second);

            const double distance2 =

                DistancePointToLine3(

                    secondEndpointPairs[secondIndex].second,

                    firstEndpointPairs[firstIndex].first,

                    firstEndpointPairs[firstIndex].second);

            if (distance1 > collinearTolerance || distance2 > collinearTolerance)

            {

                continue;

            }

            const double score = distance1 + distance2 + std::fabs(firstLength - secondLength) * 0.001;

            if (score < bestScore)

            {

                bestScore = score;

                for (int axis = 0; axis < 3; ++axis)

                {

                    firstPoint[axis] = secondEndpointPairs[secondIndex].first[axis];

                    secondPoint[axis] = secondEndpointPairs[secondIndex].second[axis];

                }

                found = true;

            }

        }

    }

    if (found)

    {

    }

    return found;

}

bool FindFarthestFaceEdgeParallelToReference(

    tag_t faceTag,

    const double referenceEdgeStart[3],

    const double referenceEdgeEnd[3],

    double firstPoint[3],

    double secondPoint[3])

{

    double facePoint[3] = {0.0, 0.0, 0.0};

    double faceNormal[3] = {0.0, 0.0, 0.0};

    if (!AskPlanarFacePlane(faceTag, facePoint, faceNormal))

    {

        return false;

    }

    double referenceDirection[3] =

    {

        referenceEdgeEnd[0] - referenceEdgeStart[0],

        referenceEdgeEnd[1] - referenceEdgeStart[1],

        referenceEdgeEnd[2] - referenceEdgeStart[2]

    };

    if (!Normalize3(referenceDirection))

    {

        return false;

    }

    std::vector<EdgeEndpointPair> endpointPairs;

    if (!AskFaceEdgeEndpointPairs(faceTag, endpointPairs))

    {

        return false;

    }

    const double referenceMidpoint[3] =

    {

        (referenceEdgeStart[0] + referenceEdgeEnd[0]) * 0.5,

        (referenceEdgeStart[1] + referenceEdgeEnd[1]) * 0.5,

        (referenceEdgeStart[2] + referenceEdgeEnd[2]) * 0.5

    };

    double bestDistance = -DBL_MAX;

    bool found = false;

    for (std::size_t index = 0; index < endpointPairs.size(); ++index)

    {

        if (!IsLinearEdge(endpointPairs[index].edgeTag))

        {

            continue;

        }

        double edgeDirection[3] =

        {

            endpointPairs[index].second[0] - endpointPairs[index].first[0],

            endpointPairs[index].second[1] - endpointPairs[index].first[1],

            endpointPairs[index].second[2] - endpointPairs[index].first[2]

        };

        if (!Normalize3(edgeDirection) ||

            std::fabs(Dot3(edgeDirection, referenceDirection)) < 0.92)

        {

            continue;

        }

        const double midpoint[3] =

        {

            (endpointPairs[index].first[0] + endpointPairs[index].second[0]) * 0.5,

            (endpointPairs[index].first[1] + endpointPairs[index].second[1]) * 0.5,

            (endpointPairs[index].first[2] + endpointPairs[index].second[2]) * 0.5

        };

        const double distance = Distance3(midpoint, referenceMidpoint);

        if (distance > bestDistance)

        {

            bestDistance = distance;

            for (int axis = 0; axis < 3; ++axis)

            {

                firstPoint[axis] = endpointPairs[index].first[axis];

                secondPoint[axis] = endpointPairs[index].second[axis];

            }

            found = true;

        }

    }

    return found;

}

void ProjectPointToFacePlane(const FacePlacement& placement, double point[3])

{

    double delta[3] =

    {

        point[0] - placement.center[0],

        point[1] - placement.center[1],

        point[2] - placement.center[2]

    };

    const double signedDistance = Dot3(delta, placement.normal);

    for (int axis = 0; axis < 3; ++axis)

    {

        point[axis] -= placement.normal[axis] * signedDistance;

    }

}

void ChooseEdgeStartAlongDirection(

    const double edgePoint1[3],

    const double edgePoint2[3],

    const double forwardDirection[3],

    double startPoint[3])

{

    const double projection1 = Dot3(edgePoint1, forwardDirection);

    const double projection2 = Dot3(edgePoint2, forwardDirection);

    const double* chosen = projection1 <= projection2 ? edgePoint1 : edgePoint2;

    for (int axis = 0; axis < 3; ++axis)

    {

        startPoint[axis] = chosen[axis];

    }

}

tag_t FindPlanarFaceForSweptProfileEdge(

    tag_t bodyTag,

    const double edgeStart[3],

    const double edgeEnd[3],

    const double sweepDirection[3],

    const char* label)

{

    tag_t matchedFace = NULL_TAG;

    if (bodyTag == NULL_TAG)

    {

        return NULL_TAG;

    }

    double edgeDirection[3] =

    {

        edgeEnd[0] - edgeStart[0],

        edgeEnd[1] - edgeStart[1],

        edgeEnd[2] - edgeStart[2]

    };

    double sweepAxis[3] =

    {

        sweepDirection[0],

        sweepDirection[1],

        sweepDirection[2]

    };

    if (!Normalize3(edgeDirection) || !Normalize3(sweepAxis))

    {

        return NULL_TAG;

    }

    double expectedNormal[3] = {0.0, 0.0, 0.0};

    Cross3(edgeDirection, sweepAxis, expectedNormal);

    if (!Normalize3(expectedNormal))

    {

        return NULL_TAG;

    }

    uf_list_p_t faceList = NULL;

    if (UF_MODL_ask_body_faces(bodyTag, &faceList) != 0 || faceList == NULL)

    {

        return NULL_TAG;

    }

    const std::vector<tag_t> faceTags = TagsFromUfList(faceList);

    UF_MODL_delete_list(&faceList);

    double bestScore = DBL_MAX;

    int planarCount = 0;

    int normalCandidateCount = 0;

    for (std::size_t index = 0; index < faceTags.size(); ++index)

    {

        double facePoint[3] = {0.0, 0.0, 0.0};

        double faceNormal[3] = {0.0, 0.0, 0.0};

        if (!AskPlanarFacePlane(faceTags[index], facePoint, faceNormal))

        {

            continue;

        }

        ++planarCount;

        const double normalCos = std::fabs(Dot3(faceNormal, expectedNormal));

        if (normalCos < 0.94)

        {

            continue;

        }

        ++normalCandidateCount;

        double startDelta[3] =

        {

            edgeStart[0] - facePoint[0],

            edgeStart[1] - facePoint[1],

            edgeStart[2] - facePoint[2]

        };

        double endDelta[3] =

        {

            edgeEnd[0] - facePoint[0],

            edgeEnd[1] - facePoint[1],

            edgeEnd[2] - facePoint[2]

        };

        const double startDistance = std::fabs(Dot3(startDelta, faceNormal));

        const double endDistance = std::fabs(Dot3(endDelta, faceNormal));

        const double score = startDistance + endDistance + (1.0 - normalCos) * 10.0;

        if (score < bestScore)

        {

            bestScore = score;

            matchedFace = faceTags[index];

        }

    }

    if (bestScore > 0.5)

    {

        matchedFace = NULL_TAG;

    }

    return matchedFace;

}

bool CreateAndUniteRectFromBevelEfFace(

    tag_t& firstBodyTag,

    tag_t efFaceTag,

    const FacePlacement& firstPlacement,

    const FacePlacement& secondPlacement,

    double secondRadius,

    double cutDepthROffset)

{

    if (firstBodyTag == NULL_TAG || efFaceTag == NULL_TAG)

    {

        return false;

    }

    const double extrudeDistance = secondRadius + cutDepthROffset - secondPlacement.wallThickness;

    if (extrudeDistance <= 0.05)

    {

        return false;

    }

    double redFacePoint[3] = {0.0, 0.0, 0.0};

    double redFaceNormal[3] = {0.0, 0.0, 0.0};

    if (!AskPlanarFacePlane(efFaceTag, redFacePoint, redFaceNormal))

    {

        return false;

    }

    double bodyBox[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};

    if (UF_MODL_ask_bounding_box(firstBodyTag, bodyBox) == 0)

    {

        double bodyCenter[3] =

        {

            (bodyBox[0] + bodyBox[3]) * 0.5,

            (bodyBox[1] + bodyBox[4]) * 0.5,

            (bodyBox[2] + bodyBox[5]) * 0.5

        };

        double centerToFace[3] =

        {

            redFacePoint[0] - bodyCenter[0],

            redFacePoint[1] - bodyCenter[1],

            redFacePoint[2] - bodyCenter[2]

        };

        if (Dot3(redFaceNormal, centerToFace) < 0.0)

        {

            redFaceNormal[0] = -redFaceNormal[0];

            redFaceNormal[1] = -redFaceNormal[1];

            redFaceNormal[2] = -redFaceNormal[2];

        }

    }

    Normalize3(redFaceNormal);

    std::vector<EdgeEndpointPair> edges;

    if (!AskFaceEdgeEndpointPairs(efFaceTag, edges))

    {

        return false;

    }

    struct CandidateEdge

    {

        EdgeEndpointPair endpoints;

        double direction[3];

        double length;

    };

    std::vector<CandidateEdge> candidates;

    for (std::size_t index = 0; index < edges.size(); ++index)

    {

        if (!IsLinearEdge(edges[index].edgeTag))

        {

            continue;

        }

        CandidateEdge candidate = {};

        candidate.endpoints = edges[index];

        candidate.direction[0] = edges[index].second[0] - edges[index].first[0];

        candidate.direction[1] = edges[index].second[1] - edges[index].first[1];

        candidate.direction[2] = edges[index].second[2] - edges[index].first[2];

        candidate.length = std::sqrt(Dot3(candidate.direction, candidate.direction));

        if (candidate.length <= 0.05 || !Normalize3(candidate.direction))

        {

            continue;

        }

        const double perpendicularToSelectedFace =

            std::fabs(Dot3(candidate.direction, firstPlacement.normal));

        if (perpendicularToSelectedFace < 0.90)

        {

            continue;

        }

        candidates.push_back(candidate);

    }

    const double targetDistance = firstPlacement.wallThickness;

    double bestScore = DBL_MAX;

    int bestFirst = -1;

    int bestSecond = -1;

    double bestOffset[3] = {0.0, 0.0, 0.0};

    for (std::size_t firstIndex = 0; firstIndex < candidates.size(); ++firstIndex)

    {

        for (std::size_t secondIndex = firstIndex + 1; secondIndex < candidates.size(); ++secondIndex)

        {

            const double parallelCos =

                std::fabs(Dot3(candidates[firstIndex].direction, candidates[secondIndex].direction));

            if (parallelCos < 0.95)

            {

                continue;

            }

            const CandidateEdge* base = &candidates[firstIndex];

            const CandidateEdge* other = &candidates[secondIndex];

            if (other->length < base->length)

            {

                base = &candidates[secondIndex];

                other = &candidates[firstIndex];

            }

            double baseMid[3] =

            {

                (base->endpoints.first[0] + base->endpoints.second[0]) * 0.5,

                (base->endpoints.first[1] + base->endpoints.second[1]) * 0.5,

                (base->endpoints.first[2] + base->endpoints.second[2]) * 0.5

            };

            double otherMid[3] =

            {

                (other->endpoints.first[0] + other->endpoints.second[0]) * 0.5,

                (other->endpoints.first[1] + other->endpoints.second[1]) * 0.5,

                (other->endpoints.first[2] + other->endpoints.second[2]) * 0.5

            };

            double midDelta[3] =

            {

                otherMid[0] - baseMid[0],

                otherMid[1] - baseMid[1],

                otherMid[2] - baseMid[2]

            };

            const double alongBase = Dot3(midDelta, base->direction);

            double offset[3] =

            {

                midDelta[0] - base->direction[0] * alongBase,

                midDelta[1] - base->direction[1] * alongBase,

                midDelta[2] - base->direction[2] * alongBase

            };

            const double edgeDistance = std::sqrt(Dot3(offset, offset));

            const double score =

                std::fabs(edgeDistance - targetDistance) + (1.0 - parallelCos) * 10.0;

            if (edgeDistance > 0.05 && score < bestScore)

            {

                bestScore = score;

                bestFirst = static_cast<int>(base - &candidates[0]);

                bestSecond = static_cast<int>(other - &candidates[0]);

                bestOffset[0] = offset[0];

                bestOffset[1] = offset[1];

                bestOffset[2] = offset[2];

            }

        }

    }

    if (bestFirst < 0 || bestSecond < 0 || bestScore > 0.35)

    {

        return false;

    }

    const CandidateEdge& baseEdge = candidates[bestFirst];

    double p1[3] = {baseEdge.endpoints.first[0], baseEdge.endpoints.first[1], baseEdge.endpoints.first[2]};

    double p2[3] = {baseEdge.endpoints.second[0], baseEdge.endpoints.second[1], baseEdge.endpoints.second[2]};

    double p3[3] =

    {

        p2[0] + bestOffset[0],

        p2[1] + bestOffset[1],

        p2[2] + bestOffset[2]

    };

    double p4[3] =

    {

        p1[0] + bestOffset[0],

        p1[1] + bestOffset[1],

        p1[2] + bestOffset[2]

    };

    std::vector<tag_t> rectangleCurves;

    tag_t extrudeBody = NULL_TAG;

    bool uniteOk = false;

    try

    {

        rectangleCurves.push_back(CreateLineBetweenPoints(p1, p2));

        rectangleCurves.push_back(CreateLineBetweenPoints(p2, p3));

        rectangleCurves.push_back(CreateLineBetweenPoints(p3, p4));

        rectangleCurves.push_back(CreateLineBetweenPoints(p4, p1));

        extrudeBody = CreateExtrudedToolBody(rectangleCurves, redFaceNormal, 0.0, extrudeDistance);

        tag_t targetBody = firstBodyTag;

        uniteOk = TryUniteToolBody(targetBody, extrudeBody, "first-unite-bevel-EF-rect");

        if (uniteOk)

        {

            firstBodyTag = targetBody;

            extrudeBody = NULL_TAG;

        }

    }

    catch (const std::exception& ex)

    {

    }

    catch (...)

    {

    }

    if (!uniteOk && extrudeBody != NULL_TAG)

    {

        UF_OBJ_delete_object(extrudeBody);

    }

    DeleteObjects(rectangleCurves);

    return uniteOk;

}

std::vector<tag_t> CreateBevelJointSketchCurves(

    const FacePlacement& firstPlacement,

    const FacePlacement& secondPlacement,

    tag_t firstEndFaceTag,

    tag_t secondEndFaceTag,

    double secondChainDepth,

    double cutDepthROffset)

{

    std::vector<tag_t> curveTags;

    if (firstEndFaceTag == NULL_TAG || secondEndFaceTag == NULL_TAG)

    {

        return curveTags;

    }

    double secondEdgePoint1[3] = {0.0, 0.0, 0.0};

    double secondEdgePoint2[3] = {0.0, 0.0, 0.0};

    if (!FindSecondTubeEdgeCoincidentWithFirstEnd(

            firstEndFaceTag,

            secondEndFaceTag,

            secondEdgePoint1,

            secondEdgePoint2))

    {

        return curveTags;

    }

    double firstLongDir[3] =

    {

        firstPlacement.lengthAxis[0] * firstPlacement.inwardSign,

        firstPlacement.lengthAxis[1] * firstPlacement.inwardSign,

        firstPlacement.lengthAxis[2] * firstPlacement.inwardSign

    };

    if (!Normalize3(firstLongDir))

    {

        return curveTags;

    }

    double pointA[3] = {0.0, 0.0, 0.0};

    ChooseEdgeStartAlongDirection(secondEdgePoint1, secondEdgePoint2, firstLongDir, pointA);

    ProjectPointToFacePlane(firstPlacement, pointA);

    double firstEdgePoint1[3] = {0.0, 0.0, 0.0};

    double firstEdgePoint2[3] = {0.0, 0.0, 0.0};

    if (!FindFarthestFaceEdgeParallelToReference(

            firstEndFaceTag,

            secondEdgePoint1,

            secondEdgePoint2,

            firstEdgePoint1,

            firstEdgePoint2))

    {

        return curveTags;

    }

    double pointB[3] = {0.0, 0.0, 0.0};

    ChooseEdgeStartAlongDirection(firstEdgePoint1, firstEdgePoint2, firstLongDir, pointB);

    ProjectPointToFacePlane(firstPlacement, pointB);

    double secondLongDir[3] =

    {

        secondPlacement.lengthAxis[0],

        secondPlacement.lengthAxis[1],

        secondPlacement.lengthAxis[2]

    };

    const double secondOnNormal = Dot3(secondLongDir, firstPlacement.normal);

    for (int axis = 0; axis < 3; ++axis)

    {

        secondLongDir[axis] -= firstPlacement.normal[axis] * secondOnNormal;

    }

    if (!Normalize3(secondLongDir))

    {

        double sideDir[3] =

        {

            pointA[0] - pointB[0],

            pointA[1] - pointB[1],

            pointA[2] - pointB[2]

        };

        for (int axis = 0; axis < 3; ++axis)

        {

            secondLongDir[axis] = sideDir[axis];

        }

        if (!Normalize3(secondLongDir))

        {

            return curveTags;

        }

    }

    double sideDir[3] =

    {

        pointA[0] - pointB[0],

        pointA[1] - pointB[1],

        pointA[2] - pointB[2]

    };

    if (Normalize3(sideDir) && Dot3(secondLongDir, sideDir) < 0.0)

    {

        for (int axis = 0; axis < 3; ++axis)

        {

            secondLongDir[axis] = -secondLongDir[axis];

        }

    }

    const double firstRadius = std::max(0.0, AskTubeOuterCornerRadius(firstPlacement));

    const double secondRadius = std::max(0.0, AskTubeOuterCornerRadius(secondPlacement));

    const double bcLength = std::max(0.0, secondChainDepth);

    const double cdLength = firstRadius + cutDepthROffset;

    const double afLength = secondRadius + cutDepthROffset;

    double firstInnerNormal[3] =

    {

        firstPlacement.normal[0],

        firstPlacement.normal[1],

        firstPlacement.normal[2]

    };

    NXOpen::Face* firstSelectedFace = FaceFromTag(firstPlacement.faceTag);

    if (firstSelectedFace != NULL)

    {

        double firstFaceCenter[3] =

        {

            firstPlacement.center[0],

            firstPlacement.center[1],

            firstPlacement.center[2]

        };

        AskFaceInnerNormalFromNxDirection(

            firstSelectedFace,

            firstPlacement.bodyTag,

            firstFaceCenter,

            firstInnerNormal);

    }

    Normalize3(firstInnerNormal);

    double firstChainDepth = 0.0;

    {

        const std::vector<tag_t> firstFaceChain =

            CollectFaceChainAlongLongEdges(

                firstPlacement.faceTag,

                firstPlacement.bodyTag,

                firstPlacement.lengthAxis);

        firstChainDepth =

            AskFaceChainDistancePerpendicularToPlane(firstFaceChain, firstInnerNormal);

        if (firstChainDepth <= 0.05)

        {

            firstChainDepth = firstPlacement.wallThickness;

        }

    }

    double pointC[3] = {0.0, 0.0, 0.0};

    double pointD[3] = {0.0, 0.0, 0.0};

    double pointE[3] = {0.0, 0.0, 0.0};

    double pointF[3] = {0.0, 0.0, 0.0};

    double pointG[3] = {0.0, 0.0, 0.0};

    double pointH[3] = {0.0, 0.0, 0.0};

    std::vector<tag_t> secondContourCurves;

    AddScaledVector(pointB, firstLongDir, bcLength, pointC);

    AddScaledVector(pointC, secondLongDir, cdLength, pointD);

    AddScaledVector(pointA, firstLongDir, afLength, pointF);

    AddScaledVector(pointF, secondLongDir, -cdLength, pointE);

    AddScaledVector(pointD, firstLongDir, -afLength, pointG);

    const bool hasPointH =

        IntersectLines3(pointD, secondLongDir, pointF, firstLongDir, pointH);

    if (!hasPointH)

    {

    }

    curveTags.push_back(CreateLineBetweenPoints(pointA, pointF));

    curveTags.push_back(CreateLineBetweenPoints(pointF, pointE));

    curveTags.push_back(CreateLineBetweenPoints(pointE, pointG));

    curveTags.push_back(CreateLineBetweenPoints(pointG, pointD));

    curveTags.push_back(CreateLineBetweenPoints(pointD, pointC));

    curveTags.push_back(CreateLineBetweenPoints(pointC, pointB));

    curveTags.push_back(CreateLineBetweenPoints(pointB, pointA));

    {

        tag_t toolBody = NULL_TAG;

        try

        {

            toolBody = CreateExtrudedToolBody(curveTags, firstInnerNormal, 0.0, firstChainDepth);

            tag_t targetBody = firstPlacement.bodyTag;

            const bool cutOk =

                TrySubtractToolBody(targetBody, toolBody, "first-minus-bevel-sketch-tool");

            tag_t efCutFace = NULL_TAG;

            if (cutOk)

            {

                efCutFace = FindPlanarFaceForSweptProfileEdge(

                    targetBody,

                    pointE,

                    pointF,

                    firstInnerNormal,

                    "first-bevel-EF-face");

                if (efCutFace != NULL_TAG)

                {

                    CreateAndUniteRectFromBevelEfFace(

                        targetBody,

                        efCutFace,

                        firstPlacement,

                        secondPlacement,

                        secondRadius,

                        cutDepthROffset);

                }

            }

        }

        catch (const std::exception& ex)

        {

            if (toolBody != NULL_TAG)

            {

                UF_OBJ_delete_object(toolBody);

            }

        }

        catch (...)

        {

            if (toolBody != NULL_TAG)

            {

                UF_OBJ_delete_object(toolBody);

            }

        }

    }

    if (hasPointH)

    {

        secondContourCurves.push_back(CreateLineBetweenPoints(pointF, pointE));

        secondContourCurves.push_back(CreateLineBetweenPoints(pointE, pointG));

        secondContourCurves.push_back(CreateLineBetweenPoints(pointG, pointD));

        secondContourCurves.push_back(CreateLineBetweenPoints(pointD, pointH));

        secondContourCurves.push_back(CreateLineBetweenPoints(pointH, pointF));

        double secondInnerNormal[3] =

        {

            secondPlacement.normal[0],

            secondPlacement.normal[1],

            secondPlacement.normal[2]

        };

        NXOpen::Face* secondSelectedFace = FaceFromTag(secondPlacement.faceTag);

        if (secondSelectedFace != NULL)

        {

            double secondFaceCenter[3] =

            {

                secondPlacement.center[0],

                secondPlacement.center[1],

                secondPlacement.center[2]

            };

            AskFaceInnerNormalFromNxDirection(

                secondSelectedFace,

                secondPlacement.bodyTag,

                secondFaceCenter,

                secondInnerNormal);

        }

        Normalize3(secondInnerNormal);

        double secondCutDepth = 0.0;

        {

            const std::vector<tag_t> secondFaceChain =

                CollectFaceChainAlongLongEdges(

                    secondPlacement.faceTag,

                    secondPlacement.bodyTag,

                    secondPlacement.lengthAxis);

            secondCutDepth =

                AskFaceChainDistancePerpendicularToPlane(secondFaceChain, secondInnerNormal);

            if (secondCutDepth <= 0.05)

            {

                secondCutDepth = secondPlacement.wallThickness;

            }

        }

        tag_t secondToolBody = NULL_TAG;

        try

        {

            secondToolBody =

                CreateExtrudedToolBody(secondContourCurves, secondInnerNormal, 0.0, secondCutDepth);

            tag_t secondTargetBody = secondPlacement.bodyTag;

            const bool secondCutOk =

                TrySubtractToolBody(secondTargetBody, secondToolBody, "second-minus-bevel-sketch-tool");

        }

        catch (const std::exception& ex)

        {

            if (secondToolBody != NULL_TAG)

            {

                UF_OBJ_delete_object(secondToolBody);

            }

        }

        catch (...)

        {

            if (secondToolBody != NULL_TAG)

            {

                UF_OBJ_delete_object(secondToolBody);

            }

        }

    }

    DeleteObjects(curveTags);

    DeleteObjects(secondContourCurves);

    return std::vector<tag_t>();

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

    char* limits[2] =

    {

        const_cast<char*>(startLimit.c_str()),

        const_cast<char*>(endLimit.c_str())

    };

    double unusedPoint[3] = {0.0, 0.0, 0.0};

    double extrudeDirection[3] = {direction[0], direction[1], direction[2]};

    uf_list_p_t featureList = NULL;

    ThrowUfError(

        UF_MODL_create_extruded(

            curveList,

            taperAngle,

            limits,

            unusedPoint,

            extrudeDirection,

            UF_NULLSIGN,

            &featureList),

        "UF_MODL_create_extruded");

    UF_MODL_delete_list(&curveList);

    std::vector<tag_t> featureTags = UfListToTags(featureList);

    UF_MODL_delete_list(&featureList);

    if (featureTags.empty())

    {

        throw std::runtime_error("No tool body was created.");

    }

    tag_t toolBody = NULL_TAG;

    ThrowUfError(UF_MODL_ask_feat_body(featureTags.front(), &toolBody), "UF_MODL_ask_feat_body");

    if (toolBody == NULL_TAG)

    {

        throw std::runtime_error("Failed to resolve tool body.");

    }

    uf_list_p_t bodyList = NULL;

    ThrowUfError(UF_MODL_create_list(&bodyList), "UF_MODL_create_list");

    ThrowUfError(UF_MODL_put_list_item(bodyList, toolBody), "UF_MODL_put_list_item");

    ThrowUfError(UF_MODL_delete_body_parms(bodyList), "UF_MODL_delete_body_parms");

    ThrowUfError(UF_MODL_ask_list_item(bodyList, 0, &toolBody), "UF_MODL_ask_list_item");

    UF_MODL_delete_list(&bodyList);

    return toolBody;

}

bool AskBodyBoundingBoxSafe(tag_t bodyTag, double box[6]);

bool BodyBoundingBoxesOverlap(tag_t firstBody, tag_t secondBody, double tolerance);

bool TrySubtractToolBody(tag_t& targetBody, tag_t toolBody, const char* label)

{

    if (targetBody == NULL_TAG || toolBody == NULL_TAG)

    {

        return false;

    }

    const tag_t originalTargetBody = targetBody;

    double targetBox[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};

    double toolBox[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};

    const bool hasTargetBox = AskBodyBoundingBoxSafe(targetBody, targetBox);

    const bool hasToolBox = AskBodyBoundingBoxSafe(toolBody, toolBox);

    const bool boxOverlap = hasTargetBox && hasToolBox && BodyBoundingBoxesOverlap(targetBody, toolBody, 0.01);

    NXOpen::Session* session = NXOpen::Session::GetSession();

    NXOpen::Part* workPart =

        session != NULL && session->Parts() != NULL ? session->Parts()->Work() : NULL;

    NXOpen::Body* targetNxBody =

        dynamic_cast<NXOpen::Body*>(NXOpen::NXObjectManager::Get(targetBody));

    NXOpen::Body* toolNxBody =

        dynamic_cast<NXOpen::Body*>(NXOpen::NXObjectManager::Get(toolBody));

    if (workPart == NULL || targetNxBody == NULL || toolNxBody == NULL)

    {

        UF_OBJ_delete_object(toolBody);

        return false;

    }

    NXOpen::Features::BooleanBuilder* builder = NULL;

    NXOpen::ScCollector* targetCollector = NULL;

    NXOpen::ScCollector* toolCollector = NULL;

    try

    {

        NXOpen::Features::BooleanFeature* nullBooleanFeature = NULL;

        builder = workPart->Features()->CreateBooleanBuilderUsingCollector(nullBooleanFeature);

        builder->SetTolerance(0.01);

        builder->SetOperation(NXOpen::Features::Feature::BooleanTypeSubtract);

        builder->SetCopyTargets(false);

        builder->SetCopyTools(false);

        builder->SetRetainTarget(false);

        builder->SetRetainTool(false);

        targetCollector = workPart->ScCollectors()->CreateCollector();

        std::vector<NXOpen::Body*> targetBodies(1);

        targetBodies[0] = targetNxBody;

        NXOpen::BodyDumbRule* targetRule =

            workPart->ScRuleFactory()->CreateRuleBodyDumb(targetBodies, true);

        std::vector<NXOpen::SelectionIntentRule*> targetRules(1);

        targetRules[0] = targetRule;

        targetCollector->ReplaceRules(targetRules, false);

        builder->SetTargetBodyCollector(targetCollector);

        toolCollector = workPart->ScCollectors()->CreateCollector();

        std::vector<NXOpen::Body*> toolBodies(1);

        toolBodies[0] = toolNxBody;

        NXOpen::BodyDumbRule* toolRule =

            workPart->ScRuleFactory()->CreateRuleBodyDumb(toolBodies, true);

        std::vector<NXOpen::SelectionIntentRule*> toolRules(1);

        toolRules[0] = toolRule;

        toolCollector->ReplaceRules(toolRules, false);

        builder->SetToolBodyCollector(toolCollector);

        NXOpen::Features::Feature* booleanFeature = builder->CommitFeature();

        if (booleanFeature != NULL)

        {

            std::vector<NXOpen::Body*> resultBodies = booleanFeature->GetBodies();

            if (!resultBodies.empty() && resultBodies[0] != NULL)

            {

                targetBody = resultBodies[0]->Tag();

            }

        }

        if (builder != NULL)

        {

            builder->Destroy();

            builder = NULL;

        }

        if (targetCollector != NULL)

        {

            targetCollector->Destroy();

            targetCollector = NULL;

        }

        if (toolCollector != NULL)

        {

            toolCollector->Destroy();

            toolCollector = NULL;

        }

        return true;

    }

    catch (const NXOpen::NXException& ex)

    {

    }

    catch (const std::exception& ex)

    {

    }

    if (builder != NULL)

    {

        builder->Destroy();

    }

    if (targetCollector != NULL)

    {

        targetCollector->Destroy();

    }

    if (toolCollector != NULL)

    {

        toolCollector->Destroy();

    }

    UF_OBJ_delete_object(toolBody);

    return false;

}

bool TryUniteToolBody(tag_t& targetBody, tag_t toolBody, const char* label)

{

    if (targetBody == NULL_TAG || toolBody == NULL_TAG)

    {

        return false;

    }

    const tag_t originalTargetBody = targetBody;

    NXOpen::Session* session = NXOpen::Session::GetSession();

    NXOpen::Part* workPart =

        session != NULL && session->Parts() != NULL ? session->Parts()->Work() : NULL;

    NXOpen::Body* targetNxBody =

        dynamic_cast<NXOpen::Body*>(NXOpen::NXObjectManager::Get(targetBody));

    NXOpen::Body* toolNxBody =

        dynamic_cast<NXOpen::Body*>(NXOpen::NXObjectManager::Get(toolBody));

    if (workPart == NULL || targetNxBody == NULL || toolNxBody == NULL)

    {

        UF_OBJ_delete_object(toolBody);

        return false;

    }

    NXOpen::Features::BooleanBuilder* builder = NULL;

    NXOpen::ScCollector* targetCollector = NULL;

    NXOpen::ScCollector* toolCollector = NULL;

    try

    {

        NXOpen::Features::BooleanFeature* nullBooleanFeature = NULL;

        builder = workPart->Features()->CreateBooleanBuilderUsingCollector(nullBooleanFeature);

        builder->SetTolerance(0.01);

        builder->SetOperation(NXOpen::Features::Feature::BooleanTypeUnite);

        builder->SetCopyTargets(false);

        builder->SetCopyTools(false);

        builder->SetRetainTarget(false);

        builder->SetRetainTool(false);

        targetCollector = workPart->ScCollectors()->CreateCollector();

        std::vector<NXOpen::Body*> targetBodies(1);

        targetBodies[0] = targetNxBody;

        NXOpen::BodyDumbRule* targetRule =

            workPart->ScRuleFactory()->CreateRuleBodyDumb(targetBodies, true);

        std::vector<NXOpen::SelectionIntentRule*> targetRules(1);

        targetRules[0] = targetRule;

        targetCollector->ReplaceRules(targetRules, false);

        builder->SetTargetBodyCollector(targetCollector);

        toolCollector = workPart->ScCollectors()->CreateCollector();

        std::vector<NXOpen::Body*> toolBodies(1);

        toolBodies[0] = toolNxBody;

        NXOpen::BodyDumbRule* toolRule =

            workPart->ScRuleFactory()->CreateRuleBodyDumb(toolBodies, true);

        std::vector<NXOpen::SelectionIntentRule*> toolRules(1);

        toolRules[0] = toolRule;

        toolCollector->ReplaceRules(toolRules, false);

        builder->SetToolBodyCollector(toolCollector);

        NXOpen::Features::Feature* booleanFeature = builder->CommitFeature();

        if (booleanFeature != NULL)

        {

            std::vector<NXOpen::Body*> resultBodies = booleanFeature->GetBodies();

            if (!resultBodies.empty() && resultBodies[0] != NULL)

            {

                targetBody = resultBodies[0]->Tag();

            }

        }

        builder->Destroy();

        targetCollector->Destroy();

        toolCollector->Destroy();

        return true;

    }

    catch (const NXOpen::NXException& ex)

    {

    }

    catch (const std::exception& ex)

    {

    }

    if (builder != NULL)

    {

        builder->Destroy();

    }

    if (targetCollector != NULL)

    {

        targetCollector->Destroy();

    }

    if (toolCollector != NULL)

    {

        toolCollector->Destroy();

    }

    UF_OBJ_delete_object(toolBody);

    return false;

}

void SubtractHoleToolsFromSecondTube(

    tag_t& secondTubeBody,

    std::vector<tag_t>& holeToolBodies,

    const char* label,

    bool retainSuccessfulTools = false)

{

    if (secondTubeBody == NULL_TAG || holeToolBodies.empty())

    {

        return;

    }

    NXOpen::Session* session = NXOpen::Session::GetSession();

    NXOpen::Part* workPart =

        session != NULL && session->Parts() != NULL ? session->Parts()->Work() : NULL;

    NXOpen::Body* targetNxBody =

        dynamic_cast<NXOpen::Body*>(NXOpen::NXObjectManager::Get(secondTubeBody));

    if (workPart == NULL || targetNxBody == NULL)

    {

        return;

    }

    std::vector<NXOpen::Body*> toolNxBodies;

    std::vector<tag_t> validToolTags;

    std::vector<tag_t> outsideTargetToolTags;

    for (std::size_t index = 0; index < holeToolBodies.size(); ++index)

    {

        if (holeToolBodies[index] == NULL_TAG)

        {

            continue;

        }

        if (!BodyBoundingBoxesOverlap(secondTubeBody, holeToolBodies[index], 0.01))

        {

            outsideTargetToolTags.push_back(holeToolBodies[index]);

            continue;

        }

        NXOpen::Body* toolNxBody =

            dynamic_cast<NXOpen::Body*>(NXOpen::NXObjectManager::Get(holeToolBodies[index]));

        if (toolNxBody == NULL)

        {

            continue;

        }

        toolNxBodies.push_back(toolNxBody);

        validToolTags.push_back(holeToolBodies[index]);

    }

    if (toolNxBodies.empty())

    {

        holeToolBodies.swap(outsideTargetToolTags);

        return;

    }

    const tag_t originalSecondBody = secondTubeBody;

    NXOpen::Features::BooleanBuilder* builder = NULL;

    NXOpen::ScCollector* targetCollector = NULL;

    NXOpen::ScCollector* toolCollector = NULL;

    try

    {

        NXOpen::Features::BooleanFeature* nullBooleanFeature = NULL;

        builder = workPart->Features()->CreateBooleanBuilderUsingCollector(nullBooleanFeature);

        builder->SetTolerance(0.01);

        builder->SetOperation(NXOpen::Features::Feature::BooleanTypeSubtract);

        builder->SetCopyTargets(false);

        builder->SetCopyTools(retainSuccessfulTools);

        builder->SetRetainTarget(false);

        builder->SetRetainTool(false);

        targetCollector = workPart->ScCollectors()->CreateCollector();

        std::vector<NXOpen::Body*> targetBodies(1);

        targetBodies[0] = targetNxBody;

        NXOpen::SelectionIntentRuleOptions* targetRuleOptions =

            workPart->ScRuleFactory()->CreateRuleOptions();

        targetRuleOptions->SetSelectedFromInactive(false);

        NXOpen::BodyDumbRule* targetRule =

            workPart->ScRuleFactory()->CreateRuleBodyDumb(targetBodies, true, targetRuleOptions);

        delete targetRuleOptions;

        std::vector<NXOpen::SelectionIntentRule*> targetRules(1);

        targetRules[0] = targetRule;

        targetCollector->ReplaceRules(targetRules, false);

        builder->SetTargetBodyCollector(targetCollector);

        toolCollector = workPart->ScCollectors()->CreateCollector();

        NXOpen::SelectionIntentRuleOptions* toolRuleOptions =

            workPart->ScRuleFactory()->CreateRuleOptions();

        toolRuleOptions->SetSelectedFromInactive(false);

        NXOpen::BodyDumbRule* toolRule =

            workPart->ScRuleFactory()->CreateRuleBodyDumb(toolNxBodies, true, toolRuleOptions);

        delete toolRuleOptions;

        std::vector<NXOpen::SelectionIntentRule*> toolRules(1);

        toolRules[0] = toolRule;

        toolCollector->ReplaceRules(toolRules, false);

        builder->SetToolBodyCollector(toolCollector);

        NXOpen::Features::Feature* booleanFeature = builder->CommitFeature();

        if (booleanFeature != NULL)

        {

            std::vector<NXOpen::Body*> resultBodies = booleanFeature->GetBodies();

            if (!resultBodies.empty() && resultBodies[0] != NULL)

            {

                secondTubeBody = resultBodies[0]->Tag();

            }

        }

        if (retainSuccessfulTools)

        {

            holeToolBodies = validToolTags;

            holeToolBodies.insert(

                holeToolBodies.end(),

                outsideTargetToolTags.begin(),

                outsideTargetToolTags.end());

        }

        else

        {

            holeToolBodies.swap(outsideTargetToolTags);

        }

    }

    catch (const NXOpen::NXException& ex)

    {

        holeToolBodies = validToolTags;

        holeToolBodies.insert(

            holeToolBodies.end(),

            outsideTargetToolTags.begin(),

            outsideTargetToolTags.end());

    }

    catch (const std::exception& ex)

    {

        holeToolBodies = validToolTags;

        holeToolBodies.insert(

            holeToolBodies.end(),

            outsideTargetToolTags.begin(),

            outsideTargetToolTags.end());

    }

    if (builder != NULL)

    {

        builder->Destroy();

    }

    if (targetCollector != NULL)

    {

        targetCollector->Destroy();

    }

    if (toolCollector != NULL)

    {

        toolCollector->Destroy();

    }

}

bool AskBodyBoundingBoxSafe(tag_t bodyTag, double box[6])

{

    if (bodyTag == NULL_TAG)

    {

        return false;

    }

    return UF_MODL_ask_bounding_box(bodyTag, box) == 0;

}

bool BodyBoundingBoxesOverlap(tag_t firstBody, tag_t secondBody, double tolerance = 0.01)

{

    double firstBox[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};

    double secondBox[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};

    if (!AskBodyBoundingBoxSafe(firstBody, firstBox) ||

        !AskBodyBoundingBoxSafe(secondBody, secondBox))

    {

        return false;

    }

    return

        firstBox[0] <= secondBox[3] + tolerance &&

        firstBox[3] + tolerance >= secondBox[0] &&

        firstBox[1] <= secondBox[4] + tolerance &&

        firstBox[4] + tolerance >= secondBox[1] &&

        firstBox[2] <= secondBox[5] + tolerance &&

        firstBox[5] + tolerance >= secondBox[2];

}

void DeleteBodiesNotIntersectingBox(std::vector<tag_t>& toolBodies, tag_t boxBody)

{

    std::vector<tag_t> keptBodies;

    for (std::size_t index = 0; index < toolBodies.size(); ++index)

    {

        const tag_t toolBody = toolBodies[index];

        if (toolBody == NULL_TAG)

        {

            continue;

        }

        if (boxBody != NULL_TAG && BodyBoundingBoxesOverlap(toolBody, boxBody))

        {

            keptBodies.push_back(toolBody);

            continue;

        }

        UF_OBJ_delete_object(toolBody);

    }

    toolBodies.swap(keptBodies);

}

double AskTubeOuterCornerRadius(const FacePlacement& placement)

{

    uf_list_p_t faceList = NULL;

    if (placement.bodyTag == NULL_TAG ||

        UF_MODL_ask_body_faces(placement.bodyTag, &faceList) != 0 ||

        faceList == NULL)

    {

        return 0.0;

    }

    std::vector<tag_t> faceTags = UfListToTags(faceList);

    UF_MODL_delete_list(&faceList);

    const double faceWidth = placement.widthMax - placement.widthMin;

    const double maxReasonableRadius =

        std::max(placement.wallThickness * 12.0, faceWidth * 0.75 + 2.0);

    double outerRadius = 0.0;

    for (std::size_t index = 0; index < faceTags.size(); ++index)

    {

        int faceType = 0;

        double point[3] = {0.0, 0.0, 0.0};

        double direction[3] = {0.0, 0.0, 0.0};

        double box[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};

        double radius = 0.0;

        double radData = 0.0;

        int normalDir = 0;

        if (UF_MODL_ask_face_data(

                faceTags[index],

                &faceType,

                point,

                direction,

                box,

                &radius,

                &radData,

                &normalDir) != 0)

        {

            continue;

        }

        if (faceType != UF_MODL_CYLINDRICAL_FACE &&

            faceType != UF_MODL_BLENDING_FACE)

        {

            continue;

        }

        if (radius <= 0.05 || radius > maxReasonableRadius || !Normalize3(direction))

        {

            continue;

        }

        outerRadius = std::max(outerRadius, radius);

    }

    return outerRadius;

}

bool AskPlanarFacePlane(tag_t faceTag, double point[3], double normal[3])

{

    int faceType = 0;

    double box[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};

    double radius = 0.0;

    double radData = 0.0;

    int normalDir = 0;

    int rc = UF_MODL_ask_face_data(faceTag, &faceType, point, normal, box, &radius, &radData, &normalDir);

    if (rc != 0 || faceType != UF_MODL_PLANAR_FACE)

    {

        return false;

    }

    std::vector<EdgeEndpointPair> endpointPairs;

    if (AskFaceEdgeEndpointPairs(faceTag, endpointPairs))

    {

        double center[3] = {0.0, 0.0, 0.0};

        if (ComputeFaceCenterFromEndpoints(endpointPairs, center))

        {

            point[0] = center[0];

            point[1] = center[1];

            point[2] = center[2];

        }

    }

    return AskFaceNormalFromDirectionCollection(faceTag, normal);

}

bool IsCoplanarFace(const FacePlacement& referencePlacement, tag_t faceTag)

{

    double point[3] = {0.0, 0.0, 0.0};

    double normal[3] = {0.0, 0.0, 0.0};

    if (!AskPlanarFacePlane(faceTag, point, normal))

    {

        return false;

    }

    const double normalCosTolerance = 0.999;

    if (std::fabs(Dot3(referencePlacement.normal, normal)) < normalCosTolerance)

    {

        return false;

    }

    double delta[3] =

    {

        point[0] - referencePlacement.center[0],

        point[1] - referencePlacement.center[1],

        point[2] - referencePlacement.center[2]

    };

    const double planeDistanceTolerance = 0.25;

    return std::fabs(Dot3(delta, referencePlacement.normal)) <= planeDistanceTolerance;

}

bool AskFaceBodyTag(NXOpen::Face* face, tag_t& bodyTag)

{

    bodyTag = NULL_TAG;

    if (face == NULL)

    {

        return false;

    }

    return UF_MODL_ask_face_body(face->Tag(), &bodyTag) == 0 && bodyTag != NULL_TAG;

}

NXOpen::Face* FaceFromTag(tag_t faceTag)

{

    if (faceTag == NULL_TAG)

    {

        return NULL;

    }

    return dynamic_cast<NXOpen::Face*>(NXOpen::NXObjectManager::Get(faceTag));

}

bool AskFaceCenter(tag_t faceTag, double center[3])

{

    std::vector<EdgeEndpointPair> endpoints;

    return AskFaceEdgeEndpointPairs(faceTag, endpoints) &&

        ComputeFaceCenterFromEndpoints(endpoints, center);

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

bool IsEdgeParallelToAxis(tag_t edgeTag, const double axis[3], double toleranceCos)

{

    double point1[3] = {0.0, 0.0, 0.0};

    double point2[3] = {0.0, 0.0, 0.0};

    int vertexCount = 0;

    if (UF_MODL_ask_edge_verts(edgeTag, point1, point2, &vertexCount) != 0 || vertexCount != 2)

    {

        return false;

    }

    double edgeDirection[3] =

    {

        point2[0] - point1[0],

        point2[1] - point1[1],

        point2[2] - point1[2]

    };

    return Normalize3(edgeDirection) && std::fabs(Dot3(edgeDirection, axis)) >= toleranceCos;

}

bool IsFaceInClosedChainAlongAxis(tag_t faceTag, const double lengthAxis[3]);

void RedisplayTag(tag_t objectTag);

double AskEdgeLength(tag_t edgeTag)

{

    double point1[3] = {0.0, 0.0, 0.0};

    double point2[3] = {0.0, 0.0, 0.0};

    int vertexCount = 0;

    if (UF_MODL_ask_edge_verts(edgeTag, point1, point2, &vertexCount) != 0 || vertexCount != 2)

    {

        return 0.0;

    }

    return Distance3(point1, point2);

}

tag_t FindLongestFaceEdgeParallelToAxis(tag_t faceTag, const double axis[3], tag_t excludedEdge)

{

    uf_list_p_t edgeList = NULL;

    if (UF_MODL_ask_face_edges(faceTag, &edgeList) != 0 || edgeList == NULL)

    {

        return NULL_TAG;

    }

    const std::vector<tag_t> edgeTags = UfListToTags(edgeList);

    UF_MODL_delete_list(&edgeList);

    tag_t bestEdge = NULL_TAG;

    double bestLength = 0.0;

    for (std::size_t index = 0; index < edgeTags.size(); ++index)

    {

        if (edgeTags[index] == excludedEdge ||

            !IsEdgeParallelToAxis(edgeTags[index], axis, 0.92))

        {

            continue;

        }

        const double length = AskEdgeLength(edgeTags[index]);

        if (length > bestLength)

        {

            bestLength = length;

            bestEdge = edgeTags[index];

        }

    }

    return bestEdge;

}

tag_t FindOtherChainFaceAcrossEdge(tag_t edgeTag, tag_t currentFaceTag, tag_t bodyTag, const double lengthAxis[3])

{

    const std::vector<tag_t> adjacentFaces = AskEdgeAdjacentFaces(edgeTag);

    for (std::size_t index = 0; index < adjacentFaces.size(); ++index)

    {

        if (adjacentFaces[index] == currentFaceTag)

        {

            continue;

        }

        tag_t adjacentBody = NULL_TAG;

        if (UF_MODL_ask_face_body(adjacentFaces[index], &adjacentBody) == 0 &&

            adjacentBody == bodyTag &&

            IsFaceInClosedChainAlongAxis(adjacentFaces[index], lengthAxis))

        {

            return adjacentFaces[index];

        }

    }

    return NULL_TAG;

}

bool IsFaceInClosedChainAlongAxis(tag_t faceTag, const double lengthAxis[3])

{

    int faceType = 0;

    double point[3] = {0.0, 0.0, 0.0};

    double direction[3] = {0.0, 0.0, 0.0};

    double box[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};

    double radius = 0.0;

    double radData = 0.0;

    int normalDir = 0;

    if (UF_MODL_ask_face_data(faceTag, &faceType, point, direction, box, &radius, &radData, &normalDir) != 0)

    {

        return false;

    }

    if (faceType == UF_MODL_PLANAR_FACE)

    {

        double normal[3] =

        {

            direction[0] * static_cast<double>(normalDir),

            direction[1] * static_cast<double>(normalDir),

            direction[2] * static_cast<double>(normalDir)

        };

        return Normalize3(normal) && std::fabs(Dot3(normal, lengthAxis)) <= 0.12;

    }

    if (faceType == UF_MODL_CYLINDRICAL_FACE || faceType == UF_MODL_CONICAL_FACE)

    {

        return Normalize3(direction) && std::fabs(Dot3(direction, lengthAxis)) >= 0.92;

    }

    return false;

}

std::vector<tag_t> CollectFaceChainAlongLongEdges(tag_t seedFaceTag, tag_t bodyTag, const double lengthAxis[3])

{

    std::vector<tag_t> chainFaces;

    std::set<tag_t> visitedFaces;

    if (!IsFaceInClosedChainAlongAxis(seedFaceTag, lengthAxis))

    {

        return chainFaces;

    }

    const tag_t seedEdge = FindLongestFaceEdgeParallelToAxis(seedFaceTag, lengthAxis, NULL_TAG);

    if (seedEdge == NULL_TAG)

    {

        return chainFaces;

    }

    tag_t currentFace = seedFaceTag;

    tag_t currentEdge = seedEdge;

    visitedFaces.insert(seedFaceTag);

    chainFaces.push_back(seedFaceTag);

    for (int step = 0; step < 32; ++step)

    {

        const tag_t nextFace = FindOtherChainFaceAcrossEdge(currentEdge, currentFace, bodyTag, lengthAxis);

        if (nextFace == NULL_TAG)

        {

            break;

        }

        if (visitedFaces.find(nextFace) != visitedFaces.end())

        {

            if (nextFace == seedFaceTag)

            {

                break;

            }

            break;

        }

        visitedFaces.insert(nextFace);

        chainFaces.push_back(nextFace);

        const tag_t nextEdge = FindLongestFaceEdgeParallelToAxis(nextFace, lengthAxis, currentEdge);

        if (nextEdge == NULL_TAG)

        {

            break;

        }

        currentFace = nextFace;

        currentEdge = nextEdge;

    }

    return chainFaces;

}

double AskFaceChainDistancePerpendicularToPlane(

    const std::vector<tag_t>& faceChain,

    const double planeNormal[3])

{

    double axis[3] = {planeNormal[0], planeNormal[1], planeNormal[2]};

    if (!Normalize3(axis))

    {

        return 0.0;

    }

    bool hasProjection = false;

    double minProjection = DBL_MAX;

    double maxProjection = -DBL_MAX;

    std::set<tag_t> visitedEdges;

    for (std::size_t faceIndex = 0; faceIndex < faceChain.size(); ++faceIndex)

    {

        uf_list_p_t edgeList = NULL;

        if (faceChain[faceIndex] == NULL_TAG ||

            UF_MODL_ask_face_edges(faceChain[faceIndex], &edgeList) != 0 ||

            edgeList == NULL)

        {

            continue;

        }

        const std::vector<tag_t> edgeTags = UfListToTags(edgeList);

        UF_MODL_delete_list(&edgeList);

        for (std::size_t edgeIndex = 0; edgeIndex < edgeTags.size(); ++edgeIndex)

        {

            if (visitedEdges.find(edgeTags[edgeIndex]) != visitedEdges.end())

            {

                continue;

            }

            visitedEdges.insert(edgeTags[edgeIndex]);

            double point1[3] = {0.0, 0.0, 0.0};

            double point2[3] = {0.0, 0.0, 0.0};

            int vertexCount = 0;

            if (UF_MODL_ask_edge_verts(edgeTags[edgeIndex], point1, point2, &vertexCount) != 0 ||

                vertexCount <= 0)

            {

                continue;

            }

            const double projection1 = Dot3(point1, axis);

            minProjection = std::min(minProjection, projection1);

            maxProjection = std::max(maxProjection, projection1);

            hasProjection = true;

            if (vertexCount >= 2)

            {

                const double projection2 = Dot3(point2, axis);

                minProjection = std::min(minProjection, projection2);

                maxProjection = std::max(maxProjection, projection2);

            }

        }

    }

    return hasProjection ? std::fabs(maxProjection - minProjection) : 0.0;

}

double AskOtherTubeFaceChainPlaneDepth(

    const SelectedFaceInfo& otherSelection,

    const double currentFaceNormal[3])

{

    FacePlacement otherPlacement = {};

    if (!AskPlanarFacePlacement(otherSelection.face, otherSelection.pickPoint, otherPlacement, false))

    {

        return 0.0;

    }

    const std::vector<tag_t> otherFaceChain =

        CollectFaceChainAlongLongEdges(

            otherPlacement.faceTag,

            otherPlacement.bodyTag,

            otherPlacement.lengthAxis);

    if (otherFaceChain.empty())

    {

        return 0.0;

    }

    return AskFaceChainDistancePerpendicularToPlane(otherFaceChain, currentFaceNormal);

}

void HighlightFaceChain(const std::vector<tag_t>& faceChain)

{

    (void)faceChain;

}

double AskTubeThicknessFromParallelFaces(tag_t bodyTag, tag_t referenceFaceTag, const double referencePoint[3], const double referenceNormal[3])

{

    double referenceArea = 0.0;

    if (!AskPlanarFaceProjectedArea(referenceFaceTag, referenceArea))

    {

        return 0.0;

    }

    uf_list_p_t faceList = NULL;

    if (bodyTag == NULL_TAG || UF_MODL_ask_body_faces(bodyTag, &faceList) != 0 || faceList == NULL)

    {

        return 0.0;

    }

    std::vector<tag_t> faceTags = UfListToTags(faceList);

    UF_MODL_delete_list(&faceList);

    double bestDistance = DBL_MAX;

    for (std::size_t index = 0; index < faceTags.size(); ++index)

    {

        if (faceTags[index] == referenceFaceTag)

        {

            continue;

        }

        double point[3] = {0.0, 0.0, 0.0};

        double normal[3] = {0.0, 0.0, 0.0};

        if (!AskPlanarFacePlane(faceTags[index], point, normal) ||

            std::fabs(Dot3(referenceNormal, normal)) < 0.98)

        {

            continue;

        }

        double area = 0.0;

        if (!AskPlanarFaceProjectedArea(faceTags[index], area) || area < referenceArea * 0.5)

        {

            continue;

        }

        double delta[3] =

        {

            point[0] - referencePoint[0],

            point[1] - referencePoint[1],

            point[2] - referencePoint[2]

        };

        const double distance = std::fabs(Dot3(delta, referenceNormal));

        if (distance > 0.05 && distance < bestDistance)

        {

            bestDistance = distance;

        }

    }

    return bestDistance < DBL_MAX ? bestDistance : 0.0;

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

    std::vector<tag_t> faceTags = UfListToTags(faceList);

    UF_MODL_delete_list(&faceList);

    double bestSignedDistance = 0.0;

    double bestDistance = DBL_MAX;

    for (std::size_t index = 0; index < faceTags.size(); ++index)

    {

        if (faceTags[index] == face->Tag())

        {

            continue;

        }

        double point[3] = {0.0, 0.0, 0.0};

        double normal[3] = {0.0, 0.0, 0.0};

        if (!AskPlanarFacePlane(faceTags[index], point, normal) ||

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

bool AddCircleFromCircularEdge(tag_t edgeTag, std::set<tag_t>& sourceEdges, std::vector<tag_t>& curveTags)

{

    if (edgeTag == NULL_TAG || sourceEdges.find(edgeTag) != sourceEdges.end())

    {

        return false;

    }

    tag_t arcCurveTag = NULL_TAG;

    if (UF_MODL_create_curve_from_edge(edgeTag, &arcCurveTag) != 0 || arcCurveTag == NULL_TAG)

    {

        return false;

    }

    double firstPoint[3] = {0.0, 0.0, 0.0};

    double middlePoint[3] = {0.0, 0.0, 0.0};

    double lastPoint[3] = {0.0, 0.0, 0.0};

    double tangent[3] = {0.0, 0.0, 0.0};

    double principalNormal[3] = {0.0, 0.0, 0.0};

    double binormal[3] = {0.0, 0.0, 0.0};

    double torsion = 0.0;

    double radiusOfCurvature = 0.0;

    const bool hasPoints =

        UF_MODL_ask_curve_props(arcCurveTag, 0.0, firstPoint, tangent, principalNormal, binormal, &torsion, &radiusOfCurvature) == 0 &&

        UF_MODL_ask_curve_props(arcCurveTag, 0.5, middlePoint, tangent, principalNormal, binormal, &torsion, &radiusOfCurvature) == 0 &&

        UF_MODL_ask_curve_props(arcCurveTag, 1.0, lastPoint, tangent, principalNormal, binormal, &torsion, &radiusOfCurvature) == 0;

    UF_OBJ_delete_object(arcCurveTag);

    if (!hasPoints)

    {

        return false;

    }

    tag_t circleTag = NULL_TAG;

    if (UF_CURVE_create_arc_thru_3pts(2, firstPoint, middlePoint, lastPoint, &circleTag) != 0 ||

        circleTag == NULL_TAG)

    {

        return false;

    }

    sourceEdges.insert(edgeTag);

    curveTags.push_back(circleTag);

    return true;

}

struct FaceChainProfile

{

    tag_t faceTag;

    std::vector<tag_t> curveTags;

};

struct CapturedFaceOneHoleRestoreProfile

{

    std::vector<tag_t> curveTags;

    double innerNormal[3];

    double thickness;

};

struct CapturedFaceOneHoleRestore

{

    tag_t bodyTag;

    std::vector<CapturedFaceOneHoleRestoreProfile> profiles;

};

std::vector<FaceChainProfile> CreateHoleAndOuterArcProfilesFromFaceChain(const std::vector<tag_t>& faceChain)

{

    std::vector<FaceChainProfile> profiles;

    for (std::size_t faceIndex = 0; faceIndex < faceChain.size(); ++faceIndex)

    {

        double center[3] = {0.0, 0.0, 0.0};

        double normal[3] = {0.0, 0.0, 0.0};

        if (!AskPlanarFacePlane(faceChain[faceIndex], center, normal))

        {

            continue;

        }

        uf_loop_p_t loopList = NULL;

        if (UF_MODL_ask_face_loops(faceChain[faceIndex], &loopList) != 0 || loopList == NULL)

        {

            continue;

        }

        std::set<tag_t> sourceEdges;

        for (uf_loop_p_t loop = loopList; loop != NULL; loop = loop->next)

        {

            if (loop->type == 2)

            {

                FaceChainProfile loopProfile = {};

                loopProfile.faceTag = faceChain[faceIndex];

                for (uf_list_p_t edgeNode = loop->edge_list; edgeNode != NULL; edgeNode = edgeNode->next)

                {

                    AddUniqueCurveFromEdge(edgeNode->eid, sourceEdges, loopProfile.curveTags);

                }

                if (!loopProfile.curveTags.empty())

                {

                    profiles.push_back(loopProfile);

                }

                continue;

            }

            if (loop->type == 1)

            {

                for (uf_list_p_t edgeNode = loop->edge_list; edgeNode != NULL; edgeNode = edgeNode->next)

                {

                    if (edgeNode->eid == NULL_TAG)

                    {

                        continue;

                    }

                    int edgeType = 0;

                    if (UF_MODL_ask_edge_type(edgeNode->eid, &edgeType) == 0 &&

                        edgeType == UF_MODL_CIRCULAR_EDGE)

                    {

                        FaceChainProfile circleProfile = {};

                        circleProfile.faceTag = faceChain[faceIndex];

                        if (AddCircleFromCircularEdge(edgeNode->eid, sourceEdges, circleProfile.curveTags))

                        {

                            profiles.push_back(circleProfile);

                        }

                    }

                }

            }

        }

        UF_MODL_delete_loop_list(&loopList);

    }

    return profiles;

}

void ClearCapturedFaceOneHoleRestore(CapturedFaceOneHoleRestore& restore)

{

    for (std::size_t index = 0; index < restore.profiles.size(); ++index)

    {

        DeleteObjects(restore.profiles[index].curveTags);

        restore.profiles[index].curveTags.clear();

    }

    restore.profiles.clear();

    restore.bodyTag = NULL_TAG;

}

CapturedFaceOneHoleRestore CaptureFaceOneHoleRestore(const SelectedFaceInfo& selection)

{

    CapturedFaceOneHoleRestore captured = {};

    captured.bodyTag = NULL_TAG;

    if (selection.face == NULL)

    {

        return captured;

    }

    FacePlacement placement = {};

    if (!AskPlanarFacePlacement(selection.face, selection.pickPoint, placement, false))

    {

        return captured;

    }

    captured.bodyTag = placement.bodyTag;

    const std::vector<tag_t> faceChain =

        CollectFaceChainAlongLongEdges(placement.faceTag, placement.bodyTag, placement.lengthAxis);

    if (faceChain.empty())

    {

        return captured;

    }

    HighlightFaceChain(faceChain);

    std::vector<FaceChainProfile> profiles = CreateHoleAndOuterArcProfilesFromFaceChain(faceChain);

    if (profiles.empty())

    {

        return captured;

    }

    for (std::size_t profileIndex = 0; profileIndex < profiles.size(); ++profileIndex)

    {

        NXOpen::Face* profileFace = FaceFromTag(profiles[profileIndex].faceTag);

        double profileCenter[3] = {0.0, 0.0, 0.0};

        if (profileFace == NULL || !AskFaceCenter(profiles[profileIndex].faceTag, profileCenter))

        {

            DeleteObjects(profiles[profileIndex].curveTags);

            continue;

        }

        CapturedFaceOneHoleRestoreProfile capturedProfile = {};

        capturedProfile.thickness = 0.0;

        if (!AskFaceInnerNormalFromNxDirection(profileFace, placement.bodyTag, profileCenter, capturedProfile.innerNormal))

        {

            DeleteObjects(profiles[profileIndex].curveTags);

            continue;

        }

        capturedProfile.thickness =

            AskTubeThicknessFromParallelFaces(placement.bodyTag, profiles[profileIndex].faceTag, profileCenter, capturedProfile.innerNormal);

        if (capturedProfile.thickness <= 0.05)

        {

            DeleteObjects(profiles[profileIndex].curveTags);

            continue;

        }

        capturedProfile.curveTags.swap(profiles[profileIndex].curveTags);

        captured.profiles.push_back(capturedProfile);

    }

    return captured;

}

std::vector<tag_t> CreateCapturedFaceOneHoleTools(CapturedFaceOneHoleRestore& captured, bool clearCaptured = true)

{

    std::vector<tag_t> toolBodies;

    if (captured.bodyTag == NULL_TAG)

    {

        if (clearCaptured)

        {

            ClearCapturedFaceOneHoleRestore(captured);

        }

        return toolBodies;

    }

    for (std::size_t profileIndex = 0; profileIndex < captured.profiles.size(); ++profileIndex)

    {

        if (captured.profiles[profileIndex].curveTags.empty() ||

            captured.profiles[profileIndex].thickness <= 0.05)

        {

            continue;

        }

        tag_t toolBody = NULL_TAG;

        try

        {

            toolBody = CreateExtrudedToolBody(

                captured.profiles[profileIndex].curveTags,

                captured.profiles[profileIndex].innerNormal,

                0.0,

                captured.profiles[profileIndex].thickness);

            {

            }

            HideProcessObject(toolBody);

            toolBodies.push_back(toolBody);

            toolBody = NULL_TAG;

        }

        catch (...)

        {

            if (toolBody != NULL_TAG)

            {

                UF_OBJ_delete_object(toolBody);

            }

        }

    }

    if (clearCaptured)

    {

        ClearCapturedFaceOneHoleRestore(captured);

    }

    return toolBodies;

}

bool FindNearestPlanarEndFaceByPick(tag_t bodyTag, const double pickPoint[3], tag_t& endFaceTag)

{

    endFaceTag = NULL_TAG;

    uf_list_p_t faceList = NULL;

    if (bodyTag == NULL_TAG || UF_MODL_ask_body_faces(bodyTag, &faceList) != 0 || faceList == NULL)

    {

        return false;

    }

    std::vector<tag_t> faceTags = TagsFromUfList(faceList);

    UF_MODL_delete_list(&faceList);

    double bestDistance = DBL_MAX;

    for (std::size_t index = 0; index < faceTags.size(); ++index)

    {

        double center[3] = {0.0, 0.0, 0.0};

        double normal[3] = {0.0, 0.0, 0.0};

        if (!AskPlanarFacePlane(faceTags[index], center, normal))

        {

            continue;

        }

        const double distance = Distance3(center, pickPoint);

        if (distance < bestDistance)

        {

            bestDistance = distance;

            endFaceTag = faceTags[index];

        }

    }

    return endFaceTag != NULL_TAG;

}

bool FindNearestPlanarEndFaceByPickAlongAxis(

    tag_t bodyTag,

    const double pickPoint[3],

    const double lengthAxis[3],

    tag_t& endFaceTag)

{

    endFaceTag = NULL_TAG;

    double axis[3] = {lengthAxis[0], lengthAxis[1], lengthAxis[2]};

    if (!Normalize3(axis))

    {

        return false;

    }

    uf_list_p_t faceList = NULL;

    if (bodyTag == NULL_TAG || UF_MODL_ask_body_faces(bodyTag, &faceList) != 0 || faceList == NULL)

    {

        return false;

    }

    std::vector<tag_t> faceTags = TagsFromUfList(faceList);

    UF_MODL_delete_list(&faceList);

    const double pickProjection = Dot3(pickPoint, axis);

    double bestDistance = DBL_MAX;

    int planarCount = 0;

    int axisCandidateCount = 0;

    for (std::size_t index = 0; index < faceTags.size(); ++index)

    {

        double center[3] = {0.0, 0.0, 0.0};

        double normal[3] = {0.0, 0.0, 0.0};

        if (!AskPlanarFacePlane(faceTags[index], center, normal))

        {

            continue;

        }

        ++planarCount;

        const double normalAxisCos = std::fabs(Dot3(normal, axis));

        if (normalAxisCos < 0.92)

        {

            continue;

        }

        ++axisCandidateCount;

        const double distance = std::fabs(Dot3(center, axis) - pickProjection);

        if (distance < bestDistance)

        {

            bestDistance = distance;

            endFaceTag = faceTags[index];

        }

    }

    {

    }

    return endFaceTag != NULL_TAG;

}

tag_t FindOffsetEndFaceForBody(

    tag_t bodyTag,

    const double pickPoint[3],

    const double lengthAxis[3])

{

    tag_t endFaceTag = NULL_TAG;

    bool foundByAxis = FindNearestPlanarEndFaceByPickAlongAxis(bodyTag, pickPoint, lengthAxis, endFaceTag);

    bool foundByFallback = false;

    if (!foundByAxis)

    {

        foundByFallback = FindNearestPlanarEndFaceByPick(bodyTag, pickPoint, endFaceTag);

    }

    return endFaceTag;

}

bool OffsetFaceByDistance(tag_t faceTag, double distance, bool direction, tag_t* resultFaceTag = NULL)

{

    if (resultFaceTag != NULL)

    {

        *resultFaceTag = NULL_TAG;

    }

    if (faceTag == NULL_TAG || std::fabs(distance) <= 0.05)

    {

        return false;

    }

    NXOpen::Session* session = NXOpen::Session::GetSession();

    NXOpen::Part* workPart =

        session != NULL && session->Parts() != NULL ? session->Parts()->Work() : NULL;

    NXOpen::Face* face = dynamic_cast<NXOpen::Face*>(NXOpen::NXObjectManager::Get(faceTag));

    if (workPart == NULL || face == NULL)

    {

        return false;

    }

    NXOpen::Features::Feature* offsetFeature = NULL;

    NXOpen::Features::OffsetFaceBuilder* builder =

        workPart->Features()->CreateOffsetFaceBuilder(NULL);

    try

    {

        std::vector<NXOpen::Face*> faces(1, face);

        NXOpen::FaceDumbRule* faceRule =

            workPart->ScRuleFactory()->CreateRuleFaceDumb(faces);

        std::vector<NXOpen::SelectionIntentRule*> rules(1, faceRule);

        builder->FaceCollector()->ReplaceRules(rules, false);

        builder->SetDirection(direction);

        builder->Distance()->SetFormula(FormatDouble(distance).c_str());

        offsetFeature = builder->CommitFeature();

    }

    catch (const std::exception& ex)

    {

    }

    catch (...)

    {

    }

    builder->Destroy();

    int resultFaceCount = 0;

    if (offsetFeature != NULL)

    {

        std::vector<NXOpen::Face*> resultFaces = offsetFeature->GetFaces();

        resultFaceCount = static_cast<int>(resultFaces.size());

        tag_t bestFaceTag = NULL_TAG;

        double sourcePoint[3] = {0.0, 0.0, 0.0};

        double sourceNormal[3] = {0.0, 0.0, 0.0};

        const bool hasSourcePlane = AskPlanarFacePlane(faceTag, sourcePoint, sourceNormal);

        double bestScore = -DBL_MAX;

        for (std::size_t index = 0; index < resultFaces.size(); ++index)

        {

            if (resultFaces[index] == NULL)

            {

                continue;

            }

            const tag_t candidateTag = resultFaces[index]->Tag();

            if (!hasSourcePlane)

            {

                bestFaceTag = candidateTag;

                break;

            }

            double candidatePoint[3] = {0.0, 0.0, 0.0};

            double candidateNormal[3] = {0.0, 0.0, 0.0};

            if (!AskPlanarFacePlane(candidateTag, candidatePoint, candidateNormal) ||

                std::fabs(Dot3(sourceNormal, candidateNormal)) < 0.95)

            {

                continue;

            }

            double delta[3] =

            {

                candidatePoint[0] - sourcePoint[0],

                candidatePoint[1] - sourcePoint[1],

                candidatePoint[2] - sourcePoint[2]

            };

            const double signedDistance = Dot3(delta, sourceNormal);

            const double score = direction ? signedDistance : -signedDistance;

            if (score > bestScore)

            {

                bestScore = score;

                bestFaceTag = candidateTag;

            }

        }

        if (resultFaceTag != NULL)

        {

            *resultFaceTag = bestFaceTag;

        }

    }

    return offsetFeature != NULL;

}

class FanTonGMKDialog

{

public:

    FanTonGMKDialog()

        : dialog(NULL),

          jointTypeBlock(NULL),

          maleFaceBlock(NULL),

          femaleFaceBlock(NULL),

          cutDepthROffsetValueBlock(NULL)

    {

        ui = NXOpen::UI::GetUI();

        const std::string dlxPath =
            zhihui_embedded_dialog::ExtractDlxToRandomPath(IDR_ZH_DLX_FANTONGGMK_DLX);
        if (dlxPath.empty())
        {
            throw std::runtime_error("FanTonGMK dialog resource is missing.");
        }
        dialog = ui->CreateDialog(dlxPath.c_str());

        dialog->AddInitializeHandler(NXOpen::make_callback(this, &FanTonGMKDialog::Initialize));

        dialog->AddDialogShownHandler(NXOpen::make_callback(this, &FanTonGMKDialog::DialogShown));

        dialog->AddFilterHandler(NXOpen::make_callback(this, &FanTonGMKDialog::Filter));

        dialog->AddUpdateHandler(NXOpen::make_callback(this, &FanTonGMKDialog::Update));

        dialog->AddOkHandler(NXOpen::make_callback(this, &FanTonGMKDialog::Ok));

        dialog->AddApplyHandler(NXOpen::make_callback(this, &FanTonGMKDialog::Apply));

    }

    ~FanTonGMKDialog()

    {

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

    NXOpen::BlockStyler::Enumeration* jointTypeBlock;

    NXOpen::BlockStyler::SelectObject* maleFaceBlock;

    NXOpen::BlockStyler::SelectObject* femaleFaceBlock;

    NXOpen::BlockStyler::DoubleBlock* cutDepthROffsetValueBlock;

    FacePlacement firstSelectedPlacement;

    bool hasFirstSelectedPlacement;

    std::wstring GetSettingsFilePath()

    {

        CreateDirectoryW(L"D:\\UG智辉钣金插件", NULL);

        CreateDirectoryW(L"D:\\UG智辉钣金插件\\config", NULL);

        return L"D:\\UG智辉钣金插件\\config\\FanTonGMK.ini";

    }

    int ReadPrivateProfileIntClamped(const wchar_t* section, const wchar_t* key, int fallback, int minValue, int maxValue)

    {

        const std::wstring path = GetSettingsFilePath();

        if (path.empty())

        {

            return fallback;

        }

        const int value = GetPrivateProfileIntW(section, key, fallback, path.c_str());

        if (value < minValue)

        {

            return minValue;

        }

        if (value > maxValue)

        {

            return maxValue;

        }

        return value;

    }

    double ReadPrivateProfileDoubleClamped(const wchar_t* section, const wchar_t* key, double fallback, double minValue)

    {

        const std::wstring path = GetSettingsFilePath();

        if (path.empty())

        {

            return fallback;

        }

        wchar_t buffer[128] = {0};

        GetPrivateProfileStringW(section, key, L"", buffer, 128, path.c_str());

        if (buffer[0] == L'\0')

        {

            return fallback;

        }

        wchar_t* endPtr = NULL;

        const double value = wcstod(buffer, &endPtr);

        if (endPtr == buffer || value < minValue)

        {

            return fallback;

        }

        return value;

    }

    void ApplyRememberedDialogValues()

    {

        const int jointType = ReadPrivateProfileIntClamped(L"Dialog", L"JointType", 0, 0, 1);

        if (jointTypeBlock != NULL)

        {

            std::vector<NXOpen::NXString> members = jointTypeBlock->GetEnumMembers();

            if (jointType >= 0 && jointType < static_cast<int>(members.size()))

            {

                jointTypeBlock->SetValueAsString(members[static_cast<std::size_t>(jointType)]);

            }

        }

        if (cutDepthROffsetValueBlock != NULL)

        {

            cutDepthROffsetValueBlock->SetValue(

                ReadPrivateProfileDoubleClamped(L"Dialog", L"CutDepthROffset", 1.0, 0.0));

        }

    }

    void SaveDialogValues()

    {

        const std::wstring path = GetSettingsFilePath();

        if (path.empty())

        {

            return;

        }

        wchar_t value[128] = {0};

        swprintf_s(value, 128, L"%d", GetJointTypeValue());

        WritePrivateProfileStringW(L"Dialog", L"JointType", value, path.c_str());

        const double cutDepthROffset =

            cutDepthROffsetValueBlock != NULL ? cutDepthROffsetValueBlock->Value() : 1.0;

        swprintf_s(value, 128, L"%.6f", cutDepthROffset);

        WritePrivateProfileStringW(L"Dialog", L"CutDepthROffset", value, path.c_str());

    }

    void SetFaceSelectionFilter(NXOpen::BlockStyler::SelectObject* selection)

    {

        if (selection == NULL)

        {

            return;

        }

        NXOpen::Selection::SelectionAction action = NXOpen::Selection::SelectionActionClearAndEnableSpecific;

        std::vector<NXOpen::Selection::MaskTriple> masks;

        masks.push_back(NXOpen::Selection::MaskTriple(

            UF_solid_type,

            UF_solid_body_subtype,

            UF_UI_SEL_FEATURE_ANY_FACE));

        NXOpen::BlockStyler::PropertyList* properties = selection->GetProperties();

        properties->SetSelectionFilter("SelectionFilter", action, masks);

        delete properties;

    }

    void SetBlockVisible(NXOpen::BlockStyler::UIBlock* block, bool visible)

    {

        if (block == NULL)

        {

            return;

        }

        const char* names[] = {"Show", "Visibility"};

        for (std::size_t index = 0; index < 2; ++index)

        {

            NXOpen::BlockStyler::PropertyList* properties = NULL;

            try

            {

                properties = block->GetProperties();

                properties->SetLogical(names[index], visible);

            }

            catch (...)

            {

            }

            if (properties != NULL)

            {

                delete properties;

            }

        }

    }

    void SetBlockEnabled(NXOpen::BlockStyler::UIBlock* block, bool enabled)

    {

        if (block == NULL)

        {

            return;

        }

        const char* names[] = {"Enable", "Sensitivity"};

        for (std::size_t index = 0; index < 2; ++index)

        {

            NXOpen::BlockStyler::PropertyList* properties = NULL;

            try

            {

                properties = block->GetProperties();

                properties->SetLogical(names[index], enabled);

            }

            catch (...)

            {

            }

            if (properties != NULL)

            {

                delete properties;

            }

        }

    }

    void SetBlockLabel(NXOpen::BlockStyler::UIBlock* block, const char* utf8Label)

    {

        if (block == NULL)

        {

            return;

        }

        NXOpen::BlockStyler::PropertyList* properties = NULL;

        try

        {

            properties = block->GetProperties();

            properties->SetString(

                NXOpen::NXString("Label", NXOpen::NXString::UTF8),

                NXOpen::NXString(utf8Label, NXOpen::NXString::UTF8));

        }

        catch (...)

        {

            try

            {

                if (properties != NULL)

                {

                    properties->SetString(

                        NXOpen::NXString("Title", NXOpen::NXString::UTF8),

                        NXOpen::NXString(utf8Label, NXOpen::NXString::UTF8));

                }

            }

            catch (...)

            {

            }

        }

        if (properties != NULL)

        {

            delete properties;

        }

    }

    void SetBlockBitmap(NXOpen::BlockStyler::UIBlock* block, const char* bitmapName)

    {

        if (block == NULL || bitmapName == NULL)

        {

            return;

        }

        const char* propertyNames[] = {"Bitmap", "Icon"};

        for (std::size_t index = 0; index < 2; ++index)

        {

            NXOpen::BlockStyler::PropertyList* properties = NULL;

            try

            {

                properties = block->GetProperties();

                properties->SetString(

                    NXOpen::NXString(propertyNames[index], NXOpen::NXString::UTF8),

                    NXOpen::NXString(bitmapName, NXOpen::NXString::UTF8));

            }

            catch (...)

            {

            }

            if (properties != NULL)

            {

                delete properties;

            }

        }

    }

    std::string ToUtf8(const NXOpen::NXString& value)

    {

        const char* text = value.GetUTF8Text();

        return text == NULL ? std::string() : std::string(text);

    }

    void SetJointTypeMembers()

    {

        if (jointTypeBlock == NULL)

        {

            return;

        }

        std::vector<NXOpen::NXString> members;

        members.push_back(NXOpen::NXString(

            "\xE5\xB9\xB3\xE5\x8F\xA3",

            NXOpen::NXString::UTF8));

        members.push_back(NXOpen::NXString(

            "\xE6\x96\x9C\xE5\x8F\xA3",

            NXOpen::NXString::UTF8));

        jointTypeBlock->SetEnumMembers(members);

        jointTypeBlock->SetValueAsString(members.front());

        SetBlockLabel(jointTypeBlock, "\xE6\x96\xB9\xE9\x80\x9A\xE4\xBA\xA4\xE6\x8E\xA5\xE6\x96\xB9\xE5\xBC\x8F");

    }

    void UpdateDialogControls()

    {

        SetBlockVisible(cutDepthROffsetValueBlock, true);

        SetBlockEnabled(cutDepthROffsetValueBlock, true);

        SetBlockVisible(femaleFaceBlock, true);

        SetBlockEnabled(femaleFaceBlock, true);

    }

    int GetJointTypeValue()

    {

        if (jointTypeBlock == NULL)

        {

            return 0;

        }

        try

        {

            const std::string value = ToUtf8(jointTypeBlock->ValueAsString());

            if (value.find("\xE6\x96\x9C") != std::string::npos)

            {

                return 1;

            }

            if (value.find("\xE5\xB9\xB3") != std::string::npos)

            {

                return 0;

            }

        }

        catch (...)

        {

        }

        return GetEnumValue(jointTypeBlock, 0);

    }

    int GetEnumValue(NXOpen::BlockStyler::Enumeration* block, int fallback)

    {

        if (block == NULL)

        {

            return fallback;

        }

        NXOpen::BlockStyler::PropertyList* properties = NULL;

        try

        {

            properties = block->GetProperties();

            const int value = properties->GetEnum("Value");

            delete properties;

            return value;

        }

        catch (...)

        {

            if (properties != NULL)

            {

                delete properties;

            }

        }

        return fallback;

    }

    SelectedFaceInfo GetSelectedFace(NXOpen::BlockStyler::SelectObject* selection)

    {

        SelectedFaceInfo result = {};

        result.face = NULL;

        result.pickPoint[0] = 0.0;

        result.pickPoint[1] = 0.0;

        result.pickPoint[2] = 0.0;

        if (selection == NULL)

        {

            return result;

        }

        NXOpen::BlockStyler::PropertyList* properties = selection->GetProperties();

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

    void Initialize()

    {

        hasFirstSelectedPlacement = false;

        jointTypeBlock = dynamic_cast<NXOpen::BlockStyler::Enumeration*>(

            dialog->TopBlock()->FindBlock("jointType"));

        maleFaceBlock = dynamic_cast<NXOpen::BlockStyler::SelectObject*>(

            dialog->TopBlock()->FindBlock("maleFace"));

        femaleFaceBlock = dynamic_cast<NXOpen::BlockStyler::SelectObject*>(

            dialog->TopBlock()->FindBlock("femaleFace"));

        cutDepthROffsetValueBlock = dynamic_cast<NXOpen::BlockStyler::DoubleBlock*>(

            dialog->TopBlock()->FindBlock("cutDepthROffsetValue"));

        SetJointTypeMembers();

        ApplyRememberedDialogValues();

        SetFaceSelectionFilter(maleFaceBlock);

        SetFaceSelectionFilter(femaleFaceBlock);

        SetBlockBitmap(maleFaceBlock, "FanTon1Icon.bmp");

        SetBlockBitmap(femaleFaceBlock, "FanTon2Icon.bmp");

        SetBlockLabel(maleFaceBlock, "\xE9\x80\x89\xE6\x8B\xA9\xE7\xAC\xAC\x31\xE6\x96\xB9\xE9\x80\x9A\xE5\xB9\xB3\xE9\x9D\xA2");

        SetBlockLabel(femaleFaceBlock, "\xE9\x80\x89\xE6\x8B\xA9\xE7\xAC\xAC\x32\xE6\x96\xB9\xE9\x80\x9A\xE5\x85\xB1\xE9\x9D\xA2\xE9\x9D\xA2");

        UpdateDialogControls();

    }

    void DialogShown()

    {

        UpdateDialogControls();

    }

    int Update(NXOpen::BlockStyler::UIBlock* block)

    {

        if (block == maleFaceBlock)

        {

            SelectedFaceInfo selected = GetSelectedFace(maleFaceBlock);

            hasFirstSelectedPlacement = false;

            if (selected.face != NULL)

            {

                FacePlacement placement = {};

                if (AskPlanarFacePlacement(selected.face, selected.pickPoint, placement, true))

                {

                    firstSelectedPlacement = placement;

                    hasFirstSelectedPlacement = true;

                    if (femaleFaceBlock != NULL)

                    {

                        femaleFaceBlock->Focus();

                    }

                }

            }

        }

        (void)block;

        return 0;

    }

    int Filter(NXOpen::BlockStyler::UIBlock* block, NXOpen::TaggedObject* selectedObject)

    {

        NXOpen::Face* face = dynamic_cast<NXOpen::Face*>(selectedObject);

        if (face == NULL)

        {

            return 0;

        }

        if (block == femaleFaceBlock && hasFirstSelectedPlacement)

        {

            tag_t bodyTag = NULL_TAG;

            if (!AskFaceBodyTag(face, bodyTag) || bodyTag == firstSelectedPlacement.bodyTag)

            {

                return 0;

            }

            return IsCoplanarFace(firstSelectedPlacement, face->Tag()) ? 1 : 0;

        }

        return 1;

    }

    int Ok()

    {

        try

        {

            const int jointType = GetJointTypeValue();

            const bool isBevelJoint = jointType == 1;

            const double cutDepthROffset =

                cutDepthROffsetValueBlock != NULL ? cutDepthROffsetValueBlock->Value() : 1.0;

            if (cutDepthROffset < 0.0)

            {

                ui->NXMessageBox()->Show(

                    "FanTonGMK",

                    NXOpen::NXMessageBox::DialogTypeError,

                    "\xE5\x88\x87\xE6\xB7\xB1\x52\x2B\xE5\xBF\x85\xE9\xA1\xBB\xE5\xA4\xA7\xE4\xBA\x8E\xE7\xAD\x89\xE4\xBA\x8E\x30\xE3\x80\x82");

                return 1;

            }

            SaveDialogValues();

            SelectedFaceInfo maleFace = GetSelectedFace(maleFaceBlock);

            SelectedFaceInfo femaleFace = GetSelectedFace(femaleFaceBlock);

            if (maleFace.face == NULL || femaleFace.face == NULL)

            {

                ui->NXMessageBox()->Show(

                    "FanTonGMK",

                    NXOpen::NXMessageBox::DialogTypeInformation,

                    "\xE8\xAF\xB7\xE9\x80\x89\xE6\x8B\xA9\xE4\xB8\xA4\xE4\xB8\xAA\xE6\x96\xB9\xE9\x80\x9A\xE7\x9A\x84\xE5\x85\xB1\xE9\x9D\xA2\xE5\xB9\xB3\xE9\x9D\xA2\xE3\x80\x82");

                return 1;

            }

            CapturedFaceOneHoleRestore faceOneHoleRestore = CaptureFaceOneHoleRestore(maleFace);

            CapturedFaceOneHoleRestore faceTwoHoleRestore = CaptureFaceOneHoleRestore(femaleFace);

            FacePlacement malePlacement = {};

            FacePlacement femalePlacement = {};

            const bool hasMalePlacement =

                AskPlanarFacePlacement(maleFace.face, maleFace.pickPoint, malePlacement, false);

            const bool hasFemalePlacement =

                AskPlanarFacePlacement(femaleFace.face, femaleFace.pickPoint, femalePlacement, false);

            const double malePositiveXOffset = hasMalePlacement ?

                AskOtherTubeFaceChainPlaneDepth(femaleFace, malePlacement.normal) :

                0.0;

            const double femalePositiveXOffset = hasFemalePlacement ?

                AskOtherTubeFaceChainPlaneDepth(maleFace, femalePlacement.normal) :

                0.0;

            {

            }

            const tag_t maleStockBoxBody =

                CreateSelectedBodyStockBox(maleFace, malePositiveXOffset);

            const tag_t femaleStockBoxBody =

                CreateSelectedBodyStockBox(femaleFace, femalePositiveXOffset);

            {

            }

            std::vector<tag_t> maleHoleToolBodies =

                CreateCapturedFaceOneHoleTools(faceOneHoleRestore, !isBevelJoint);

            std::vector<tag_t> maleHoleToolBodiesForSecond;

            if (isBevelJoint)

            {

                maleHoleToolBodiesForSecond =

                    CreateCapturedFaceOneHoleTools(faceOneHoleRestore, true);

            }

            std::vector<tag_t> femaleHoleToolBodies =

                CreateCapturedFaceOneHoleTools(faceTwoHoleRestore, !isBevelJoint);

            std::vector<tag_t> femaleHoleToolBodiesForSecond;

            if (isBevelJoint)

            {

                femaleHoleToolBodiesForSecond =

                    CreateCapturedFaceOneHoleTools(faceTwoHoleRestore, true);

            }

            std::vector<tag_t> sharedHoleToolBodies = maleHoleToolBodies;

            sharedHoleToolBodies.insert(

                sharedHoleToolBodies.end(),

                femaleHoleToolBodies.begin(),

                femaleHoleToolBodies.end());

            std::vector<tag_t> sharedHoleToolBodiesForSecond = maleHoleToolBodiesForSecond;

            sharedHoleToolBodiesForSecond.insert(

                sharedHoleToolBodiesForSecond.end(),

                femaleHoleToolBodiesForSecond.begin(),

                femaleHoleToolBodiesForSecond.end());

            {

            }

            if (isBevelJoint)

            {

                DeleteBodiesNotIntersectingBox(sharedHoleToolBodies, femaleStockBoxBody);

                DeleteBodiesNotIntersectingBox(sharedHoleToolBodiesForSecond, femaleStockBoxBody);

            }

            else

            {

                DeleteBodiesNotIntersectingBox(maleHoleToolBodies, femaleStockBoxBody);

                DeleteBodiesNotIntersectingBox(femaleHoleToolBodies, maleStockBoxBody);

            }

            {

            }

            if (hasMalePlacement && femaleStockBoxBody != NULL_TAG)

            {

                TrySubtractToolBody(malePlacement.bodyTag, femaleStockBoxBody, "male-minus-female-box");

            }

            if (hasFemalePlacement && maleStockBoxBody != NULL_TAG)

            {

                TrySubtractToolBody(femalePlacement.bodyTag, maleStockBoxBody, "female-minus-male-box");

            }

            {

            }

            tag_t maleOffsetEndFaceTag = NULL_TAG;

            tag_t femaleOffsetEndFaceTag = NULL_TAG;

            if (isBevelJoint && hasMalePlacement)

            {

                maleOffsetEndFaceTag =

                    FindOffsetEndFaceForBody(

                        malePlacement.bodyTag,

                        maleFace.pickPoint,

                        malePlacement.lengthAxis);

            }

            if (hasFemalePlacement)

            {

                femaleOffsetEndFaceTag =

                    FindOffsetEndFaceForBody(

                        femalePlacement.bodyTag,

                        femaleFace.pickPoint,

                        femalePlacement.lengthAxis);

            }

            {

            }

            tag_t maleOffsetResultFaceTag = NULL_TAG;

            tag_t femaleOffsetResultFaceTag = NULL_TAG;

            if (isBevelJoint && hasMalePlacement && malePositiveXOffset > 0.05)

            {

                OffsetFaceByDistance(

                    maleOffsetEndFaceTag,

                    malePositiveXOffset,

                    false,

                    &maleOffsetResultFaceTag);

            }

            if (hasFemalePlacement && femalePositiveXOffset > 0.05)

            {

                OffsetFaceByDistance(

                    femaleOffsetEndFaceTag,

                    femalePositiveXOffset,

                    false,

                    &femaleOffsetResultFaceTag);

            }

            if (isBevelJoint)

            {

                if (hasMalePlacement)

                {

                    SubtractHoleToolsFromSecondTube(

                        malePlacement.bodyTag,

                        sharedHoleToolBodies,

                        "first-minus-shared-hole-tools",

                        false);

                }

                if (hasFemalePlacement && !sharedHoleToolBodiesForSecond.empty())

                {

                    SubtractHoleToolsFromSecondTube(

                        femalePlacement.bodyTag,

                        sharedHoleToolBodiesForSecond,

                        "second-minus-shared-hole-tools");

                }

            }

            if (isBevelJoint && hasMalePlacement && hasFemalePlacement)

            {

                std::vector<tag_t> bevelSketchCurves =

                    CreateBevelJointSketchCurves(

                        malePlacement,

                        femalePlacement,

                        maleOffsetResultFaceTag != NULL_TAG ? maleOffsetResultFaceTag : maleOffsetEndFaceTag,

                        femaleOffsetResultFaceTag != NULL_TAG ? femaleOffsetResultFaceTag : femaleOffsetEndFaceTag,

                        malePositiveXOffset,

                        cutDepthROffset);

                (void)bevelSketchCurves;

            }

            if (!isBevelJoint && hasFemalePlacement)

            {

                SubtractHoleToolsFromSecondTube(

                    femalePlacement.bodyTag,

                    maleHoleToolBodies,

                    "second-minus-first-hole-tools");

                SubtractHoleToolsFromSecondTube(

                    femalePlacement.bodyTag,

                    femaleHoleToolBodies,

                    "second-minus-second-hole-tools");

            }

            (void)jointType;

            (void)cutDepthROffset;

            (void)maleFace;

            (void)femaleFace;

            return 0;

        }

        catch (const NXOpen::NXException& ex)

        {

            ui->NXMessageBox()->Show("FanTonGMK", NXOpen::NXMessageBox::DialogTypeError, ex.Message());

            return 1;

        }

        catch (const std::exception& ex)

        {

            ui->NXMessageBox()->Show("FanTonGMK", NXOpen::NXMessageBox::DialogTypeError, ex.what());

            return 1;

        }

    }

    int Apply()

    {

        return Ok();

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

    if (!zhihui_license_guard::EnsureAuthorized(L"ZHIHUI.FANTONGGMK", L"FanTonGMK"))

    {

        return;

    }

    try

    {

        UF_initialize();

        FanTonGMKDialog commandDialog;

        commandDialog.Show();

        UF_terminate();

    }

    catch (const NXOpen::NXException& ex)

    {

        NXOpen::UI::GetUI()->NXMessageBox()->Show(

            "FanTonGMK",

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

            "FanTonGMK",

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

