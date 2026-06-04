#include <NXOpen/Body.hxx>
#include <NXOpen/Direction.hxx>
#include <NXOpen/DirectionCollection.hxx>
#include <NXOpen/DisplayManager.hxx>
#include <NXOpen/DisplayModification.hxx>
#include <NXOpen/DisplayableObject.hxx>
#include <NXOpen/Edge.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/ISurface.hxx>
#include <NXOpen/ListingWindow.hxx>
#include <NXOpen/Measurement.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXMessageBox.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/NXString.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/Selection.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/SmartObject.hxx>
#include <NXOpen/TaggedObject.hxx>
#include <NXOpen/UI.hxx>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#undef CreateDialog

#include <uf.h>
#include <uf_csys.h>
#include <uf_curve.h>
#include <uf_disp.h>
#include <uf_eval.h>
#include <uf_mtx.h>
#include <uf_modl.h>
#include <uf_obj.h>
#include <uf_object_types.h>
#include <uf_part.h>
#include <uf_ui_types.h>

#include <algorithm>
#include <cmath>
#include <exception>
#include <fstream>
#include <functional>
#include <iomanip>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#ifndef DllExport
#define DllExport __declspec(dllexport)
#endif

extern "C" DllExport void ufusr(char* param, int* retcode, int param_len);

namespace tube_same_body
{
const char* kDialogName = "选择管件";
const char* kDialogFile = "DuoSiTiZuanZuanPei.dlx";
const char* kBodySelectionBlockId = "bodySelection";
const double kLengthTolerance = 0.05;
const double kMassTolerance = 0.05;
const double kDistanceTolerance = 0.05;
const double kFaceAreaAbsoluteTolerance = 0.05;
const double kFaceAreaRelativeTolerance = 0.0001;
const bool kDebugLogEnabled = false;
const bool kCoordinateDebugPauseEnabled = false;
const bool kShowCompletionMessageBox = false;
const double kCoordinateDebugAxisLength = 50.0;
const bool kShowResultListingWindow = false;
const int kMinimumColorIndex = 1;
const int kMaximumColorIndex = 216;
const int kDebugXAxisColor = 186;
const int kDebugYAxisColor = 70;
const int kDebugZAxisColor = 36;
const int kDebugCsysColor = 141;
const int kPreferredGroupColors[] = {6, 36, 42, 70, 96, 103, 121, 141, 151, 159, 186, 211};

std::string GetCurrentModuleDirectory()
{
    HMODULE moduleHandle = NULL;
    if (!GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(&ufusr),
            &moduleHandle))
    {
        return std::string();
    }

    char pathBuffer[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameA(moduleHandle, pathBuffer, MAX_PATH);
    if (length == 0 || length == MAX_PATH)
    {
        return std::string();
    }

    std::string modulePath(pathBuffer, length);
    const std::string::size_type pos = modulePath.find_last_of("\\/");
    if (pos == std::string::npos)
    {
        return std::string();
    }

    return modulePath.substr(0, pos + 1);
}

std::string GetDialogFullPath()
{
    const std::string moduleDirectory = GetCurrentModuleDirectory();
    if (moduleDirectory.empty())
    {
        return kDialogFile;
    }

    return moduleDirectory + kDialogFile;
}

std::string GetMatchDebugLogPath()
{
    const std::string moduleDirectory = GetCurrentModuleDirectory();
    if (moduleDirectory.empty())
    {
        return "DuoSiTiZuanZuanPei_match.log";
    }

    return moduleDirectory + "DuoSiTiZuanZuanPei_match.log";
}

struct Point3
{
    double x;
    double y;
    double z;
};

struct LengthBucket
{
    double length;
    int count;
};

struct Frame3
{
    Point3 origin;
    Point3 xAxis;
    Point3 yAxis;
    Point3 zAxis;
};

struct PlaneFaceFeature
{
    tag_t tag;
    double area;
    double perimeter;
    int edgeCount;
    std::vector<LengthBucket> lengthBuckets;
    Frame3 frame;
};

struct PlaneFaceGroup
{
    std::vector<std::size_t> faceIndexes;
};

struct BodyFingerprint
{
    NXOpen::Body* body;
    tag_t tag;
    double mass;
    double centroid[3];
    int edgeCount;
    int faceCount;
    std::vector<LengthBucket> lengthBuckets;
    std::vector<double> fullCircleEdgeLengths;
    std::vector<double> arcEdgeLengths;
    std::vector<double> curveEdgeLengths;
    std::vector<double> lineEdgeLengths;
    std::vector<Point3> vertexPoints;
    std::vector<Point3> circleCenterPoints;
    std::vector<double> circleCenterDistances;
    std::vector<Point3> lineEdgePoints;
    std::vector<Point3> curveEdgePoints;
    std::vector<Point3> arcEdgePoints;
    std::vector<Point3> fullCircleEdgePoints;
    std::vector<PlaneFaceFeature> planeFaces;
    std::vector<PlaneFaceGroup> planeFaceGroups;
};

void ThrowUfError(int errorCode)
{
    if (errorCode != 0)
    {
        throw NXOpen::NXException::Create(errorCode);
    }
}

bool NearlyEqual(double lhs, double rhs, double tolerance)
{
    return std::fabs(lhs - rhs) <= tolerance;
}

double AbsoluteRelativeTolerance(
    double lhs,
    double rhs,
    double absoluteTolerance,
    double relativeTolerance)
{
    const double scale = std::max(std::fabs(lhs), std::fabs(rhs));
    return std::max(absoluteTolerance, scale * relativeTolerance);
}

bool NearlyEqualAbsoluteRelative(
    double lhs,
    double rhs,
    double absoluteTolerance,
    double relativeTolerance)
{
    return std::fabs(lhs - rhs) <=
        AbsoluteRelativeTolerance(lhs, rhs, absoluteTolerance, relativeTolerance);
}

bool FaceAreasNearlyEqual(double lhs, double rhs)
{
    return NearlyEqualAbsoluteRelative(
        lhs,
        rhs,
        kFaceAreaAbsoluteTolerance,
        kFaceAreaRelativeTolerance);
}

bool FaceAreaSignificantlyGreater(double lhs, double rhs)
{
    return lhs > rhs + AbsoluteRelativeTolerance(
        lhs,
        rhs,
        kFaceAreaAbsoluteTolerance,
        kFaceAreaRelativeTolerance);
}

std::string FormatDouble(double value)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(6) << value;
    return stream.str();
}

std::string Utf8Text(const char* text)
{
    return std::string(text);
}

void WriteLineUtf8(NXOpen::ListingWindow* listingWindow, const std::string& text)
{
    listingWindow->WriteLine(NXOpen::NXString(text, NXOpen::NXString::UTF8));
}

std::vector<int> GenerateRandomGroupColors(std::size_t count)
{
    std::vector<int> colors(
        kPreferredGroupColors,
        kPreferredGroupColors + sizeof(kPreferredGroupColors) / sizeof(kPreferredGroupColors[0]));

    const unsigned int seed =
        static_cast<unsigned int>(GetTickCount()) ^
        static_cast<unsigned int>(count * 2654435761u);
    std::mt19937 generator(seed);
    std::shuffle(colors.begin(), colors.end(), generator);

    std::uniform_int_distribution<int> distribution(kMinimumColorIndex, kMaximumColorIndex);
    while (colors.size() < count)
    {
        colors.push_back(distribution(generator));
    }

    colors.resize(count);
    return colors;
}

void AppendTags(std::ostringstream& line, const std::vector<tag_t>& tags)
{
    for (std::size_t index = 0; index < tags.size(); ++index)
    {
        if (index != 0)
        {
            line << ", ";
        }
        line << tags[index];
    }
}

std::string FormatPoint(const double point[3])
{
    std::ostringstream stream;
    stream << "(" << FormatDouble(point[0]) << ", "
           << FormatDouble(point[1]) << ", "
           << FormatDouble(point[2]) << ")";
    return stream.str();
}

std::string FormatPoint(const Point3& point)
{
    std::ostringstream stream;
    stream << "(" << FormatDouble(point.x) << ", "
           << FormatDouble(point.y) << ", "
           << FormatDouble(point.z) << ")";
    return stream.str();
}

std::string FormatCoordinateVariantName(int variantIndex)
{
    switch (variantIndex)
    {
    case 0:
        return "Z0: (X,Y)";
    case 3:
        return "Z90: (-Y,X)";
    case 5:
        return "Z270: (Y,-X)";
    case 6:
        return "Z180: (-X,-Y)";
    default:
        break;
    }

    std::ostringstream stream;
    stream << "variant " << variantIndex;
    return stream.str();
}

std::string FormatLengthBuckets(const std::vector<LengthBucket>& buckets);

void WriteFingerprintDebugLog(
    std::ofstream& debugLog,
    std::size_t bodyIndex,
    const BodyFingerprint& fingerprint)
{
    if (!debugLog.is_open())
    {
        return;
    }

    debugLog << "Body " << (bodyIndex + 1)
             << ": tag=" << fingerprint.tag
             << ", mass=" << FormatDouble(fingerprint.mass)
             << ", centroid=" << FormatPoint(fingerprint.centroid)
             << ", edges=" << fingerprint.edgeCount
             << ", faces=" << fingerprint.faceCount
             << ", edgeLengthBuckets=" << fingerprint.lengthBuckets.size()
             << ", uniqueVertices=" << fingerprint.vertexPoints.size()
             << ", fullCircleCenters=" << fingerprint.circleCenterPoints.size()
             << ", lineEdgePoints=" << fingerprint.lineEdgePoints.size()
             << ", curveEdgePoints=" << fingerprint.curveEdgePoints.size()
             << ", arcEdgePoints=" << fingerprint.arcEdgePoints.size()
             << ", fullCircleEdgePoints=" << fingerprint.fullCircleEdgePoints.size()
             << ", planarFaces=" << fingerprint.planeFaces.size()
             << ", planeFaceGroups=" << fingerprint.planeFaceGroups.size()
             << std::endl;
}

void WritePlaneFaceGroupsDebugLog(
    std::ofstream& debugLog,
    std::size_t bodyIndex,
    const BodyFingerprint& fingerprint)
{
    if (!debugLog.is_open())
    {
        return;
    }

    debugLog << "Body " << (bodyIndex + 1) << " planar face groups detail:" << std::endl;
    for (std::size_t groupIndex = 0; groupIndex < fingerprint.planeFaceGroups.size(); ++groupIndex)
    {
        const PlaneFaceGroup& group = fingerprint.planeFaceGroups[groupIndex];
        debugLog << "  groupIndex=" << groupIndex
                 << ", count=" << group.faceIndexes.size();
        if (!group.faceIndexes.empty())
        {
            const std::size_t faceIndex = group.faceIndexes[0];
            if (faceIndex < fingerprint.planeFaces.size())
            {
                const PlaneFaceFeature& face = fingerprint.planeFaces[faceIndex];
                debugLog << ", firstFaceTag=" << face.tag
                         << ", area=" << FormatDouble(face.area)
                         << ", perimeter=" << FormatDouble(face.perimeter)
                         << ", edgeCount=" << face.edgeCount
                         << ", lengthBuckets=" << FormatLengthBuckets(face.lengthBuckets);
            }
        }
        debugLog << std::endl;

        for (std::size_t memberIndex = 0; memberIndex < group.faceIndexes.size(); ++memberIndex)
        {
            const std::size_t faceIndex = group.faceIndexes[memberIndex];
            if (faceIndex >= fingerprint.planeFaces.size())
            {
                continue;
            }

            const PlaneFaceFeature& face = fingerprint.planeFaces[faceIndex];
            debugLog << "    member=" << memberIndex
                     << ", faceTag=" << face.tag
                     << ", area=" << FormatDouble(face.area)
                     << ", perimeter=" << FormatDouble(face.perimeter)
                     << ", edgeCount=" << face.edgeCount
                     << ", lengthBuckets=" << FormatLengthBuckets(face.lengthBuckets)
                     << std::endl;
        }
    }
}

Point3 MakePoint3(double x, double y, double z)
{
    Point3 point = {};
    point.x = x;
    point.y = y;
    point.z = z;
    return point;
}

Point3 PointFromArray(const double point[3])
{
    return MakePoint3(point[0], point[1], point[2]);
}

Point3 PointFromNx(const NXOpen::Point3d& point)
{
    return MakePoint3(point.X, point.Y, point.Z);
}

Point3 AddPoints(const Point3& lhs, const Point3& rhs)
{
    return MakePoint3(lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z);
}

Point3 SubtractPoints(const Point3& lhs, const Point3& rhs)
{
    return MakePoint3(lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z);
}

Point3 ScalePoint(const Point3& point, double scale)
{
    return MakePoint3(point.x * scale, point.y * scale, point.z * scale);
}

double DotPoint(const Point3& lhs, const Point3& rhs)
{
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

Point3 CrossPoint(const Point3& lhs, const Point3& rhs)
{
    return MakePoint3(
        lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.x * rhs.y - lhs.y * rhs.x);
}

double PointMagnitude(const Point3& point)
{
    return std::sqrt(DotPoint(point, point));
}

bool NormalizePoint(Point3& point)
{
    const double magnitude = PointMagnitude(point);
    if (magnitude <= 1.0e-12)
    {
        return false;
    }

    point.x /= magnitude;
    point.y /= magnitude;
    point.z /= magnitude;
    return true;
}

Point3 ProjectOutAxis(const Point3& vector, const Point3& axis)
{
    return SubtractPoints(vector, ScalePoint(axis, DotPoint(vector, axis)));
}

void CopyPointToArray(const Point3& point, double target[3])
{
    target[0] = point.x;
    target[1] = point.y;
    target[2] = point.z;
}

Frame3 ApplyFrameCoordinateVariant(const Frame3& frame, int variantIndex)
{
    const bool swapXY = (variantIndex & 1) != 0;
    const int signX = (variantIndex & 2) != 0 ? -1 : 1;
    const int signY = (variantIndex & 4) != 0 ? -1 : 1;

    Frame3 result = frame;
    const Point3 sourceX = swapXY ? frame.yAxis : frame.xAxis;
    const Point3 sourceY = swapXY ? frame.xAxis : frame.yAxis;
    result.xAxis = ScalePoint(sourceX, static_cast<double>(signX));
    result.yAxis = ScalePoint(sourceY, static_cast<double>(signY));
    result.zAxis = frame.zAxis;
    return result;
}

void AddDebugObjectTag(std::vector<tag_t>* objectTags, tag_t objectTag)
{
    if (objectTags != NULL && objectTag != NULL_TAG)
    {
        objectTags->push_back(objectTag);
    }
}

void DeleteDebugCoordinateObjects(std::vector<tag_t>* objectTags)
{
    if (objectTags == NULL || objectTags->empty())
    {
        return;
    }

    for (std::vector<tag_t>::reverse_iterator iterator = objectTags->rbegin();
         iterator != objectTags->rend();
         ++iterator)
    {
        if (*iterator != NULL_TAG)
        {
            UF_OBJ_delete_object(*iterator);
        }
    }

    objectTags->clear();
    UF_DISP_refresh();
}

void TryCreateVisibleCsys(
    const Frame3& frame,
    const std::string& name,
    std::vector<tag_t>* objectTags)
{
    double origin[3] = {};
    double xAxis[3] = {};
    double yAxis[3] = {};
    double matrixValues[9] = {};
    CopyPointToArray(frame.origin, origin);
    CopyPointToArray(frame.xAxis, xAxis);
    CopyPointToArray(frame.yAxis, yAxis);

    if (UF_MTX3_initialize(xAxis, yAxis, matrixValues) != 0)
    {
        return;
    }

    tag_t matrixTag = NULL_TAG;
    if (UF_CSYS_create_matrix(matrixValues, &matrixTag) != 0)
    {
        return;
    }
    AddDebugObjectTag(objectTags, matrixTag);

    tag_t csysTag = NULL_TAG;
    if (UF_CSYS_create_csys(origin, matrixTag, &csysTag) != 0 || csysTag == NULL_TAG)
    {
        return;
    }

    UF_OBJ_set_name(csysTag, (name + "_CSYS").c_str());
    UF_OBJ_set_color(csysTag, kDebugCsysColor);
    AddDebugObjectTag(objectTags, csysTag);
}

void TryCreateAxisLine(
    const Point3& origin,
    const Point3& axis,
    double length,
    int color,
    const std::string& name,
    std::vector<tag_t>* objectTags)
{
    UF_CURVE_line_t lineData = {};
    const Point3 endPoint = AddPoints(origin, ScalePoint(axis, length));
    CopyPointToArray(origin, lineData.start_point);
    CopyPointToArray(endPoint, lineData.end_point);

    tag_t lineTag = NULL_TAG;
    if (UF_CURVE_create_line(&lineData, &lineTag) != 0 || lineTag == NULL_TAG)
    {
        return;
    }

    UF_OBJ_set_name(lineTag, name.c_str());
    UF_OBJ_set_color(lineTag, color);
    AddDebugObjectTag(objectTags, lineTag);
}

void CreateVisibleCoordinateFrame(
    const Frame3& frame,
    const std::string& name,
    std::vector<tag_t>* objectTags)
{
    TryCreateVisibleCsys(frame, name, objectTags);
    TryCreateAxisLine(
        frame.origin,
        frame.xAxis,
        kCoordinateDebugAxisLength,
        kDebugXAxisColor,
        name + "_X",
        objectTags);
    TryCreateAxisLine(
        frame.origin,
        frame.yAxis,
        kCoordinateDebugAxisLength,
        kDebugYAxisColor,
        name + "_Y",
        objectTags);
    TryCreateAxisLine(
        frame.origin,
        frame.zAxis,
        kCoordinateDebugAxisLength,
        kDebugZAxisColor,
        name + "_Z",
        objectTags);
}

bool CreateAndPauseCoordinateDebugStep(
    int* coordinateDebugStep,
    std::vector<tag_t>* activeDebugObjectTags,
    bool* coordinateDebugCanceled,
    tag_t referenceTag,
    tag_t candidateTag,
    tag_t referenceFaceTag,
    tag_t candidateFaceTag,
    int variantIndex,
    const Frame3& referenceFrame,
    const Frame3& candidateFrame)
{
    if (!kCoordinateDebugPauseEnabled)
    {
        return true;
    }

    if (coordinateDebugCanceled != NULL && *coordinateDebugCanceled)
    {
        return false;
    }

    if (coordinateDebugStep == NULL)
    {
        return true;
    }

    DeleteDebugCoordinateObjects(activeDebugObjectTags);

    ++(*coordinateDebugStep);
    const int step = *coordinateDebugStep;

    std::ostringstream refName;
    refName << "DST_" << std::setw(3) << std::setfill('0') << step << "_REF";
    std::ostringstream candName;
    candName << "DST_" << std::setw(3) << std::setfill('0') << step << "_CAND";

    CreateVisibleCoordinateFrame(referenceFrame, refName.str(), activeDebugObjectTags);
    CreateVisibleCoordinateFrame(candidateFrame, candName.str(), activeDebugObjectTags);
    UF_DISP_refresh();

    std::ostringstream message;
    message << "Temporary coordinate step " << step << "\n"
            << "Reference body tag: " << referenceTag << "\n"
            << "Candidate body tag: " << candidateTag << "\n"
            << "Reference face tag: " << referenceFaceTag << "\n"
            << "Candidate face tag: " << candidateFaceTag << "\n"
            << "Candidate rotation: " << FormatCoordinateVariantName(variantIndex) << "\n"
            << "Reference origin: " << FormatPoint(referenceFrame.origin) << "\n"
            << "Candidate origin: " << FormatPoint(candidateFrame.origin) << "\n"
            << "Created objects prefix: " << refName.str() << " / " << candName.str() << "\n"
            << "Click OK to create the next coordinate set.\n"
            << "Click Cancel to stop and keep this coordinate set.";

    const int response = MessageBoxA(
        NULL,
        message.str().c_str(),
        kDialogName,
        MB_OKCANCEL | MB_ICONINFORMATION | MB_TASKMODAL);
    if (response == IDCANCEL)
    {
        if (coordinateDebugCanceled != NULL)
        {
            *coordinateDebugCanceled = true;
        }
        return false;
    }

    return true;
}

double DistanceBetweenPoints(const double lhs[3], const double rhs[3])
{
    const double dx = lhs[0] - rhs[0];
    const double dy = lhs[1] - rhs[1];
    const double dz = lhs[2] - rhs[2];
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

double DistanceBetweenPoints(const Point3& lhs, const Point3& rhs)
{
    return PointMagnitude(SubtractPoints(lhs, rhs));
}

double DistanceToPoint(const double centroid[3], const NXOpen::Point3d& point)
{
    const double target[3] = {point.X, point.Y, point.Z};
    return DistanceBetweenPoints(centroid, target);
}

bool AskCircularEdgeCenter(tag_t edgeTag, double center[3])
{
    UF_EVAL_p_t evaluator = NULL;
    if (UF_EVAL_initialize(edgeTag, &evaluator) != 0 || evaluator == NULL)
    {
        return false;
    }

    logical isArc = false;
    const int isArcStatus = UF_EVAL_is_arc(evaluator, &isArc);
    if (isArcStatus != 0 || !isArc)
    {
        UF_EVAL_free(evaluator);
        return false;
    }

    UF_EVAL_arc_t arc = {};
    const int arcStatus = UF_EVAL_ask_arc(evaluator, &arc);
    UF_EVAL_free(evaluator);
    if (arcStatus != 0)
    {
        return false;
    }

    center[0] = arc.center[0];
    center[1] = arc.center[1];
    center[2] = arc.center[2];
    return true;
}

bool AddUniquePoint(std::vector<Point3>& points, const Point3& point)
{
    for (std::size_t index = 0; index < points.size(); ++index)
    {
        if (DistanceBetweenPoints(points[index], point) <= kDistanceTolerance)
        {
            return false;
        }
    }

    points.push_back(point);
    return true;
}

void AppendEdgeVerticesToGroup(
    int vertexCount,
    const double firstVertex[3],
    const double secondVertex[3],
    std::vector<Point3>& points)
{
    if (vertexCount >= 1)
    {
        points.push_back(PointFromArray(firstVertex));
    }

    if (vertexCount >= 2)
    {
        points.push_back(PointFromArray(secondVertex));
    }
}

void AppendUniqueEdgeVertices(
    int vertexCount,
    const double firstVertex[3],
    const double secondVertex[3],
    std::vector<Point3>& vertexPoints)
{
    if (vertexCount >= 1)
    {
        AddUniquePoint(vertexPoints, PointFromArray(firstVertex));
    }

    if (vertexCount >= 2)
    {
        AddUniquePoint(vertexPoints, PointFromArray(secondVertex));
    }
}

void AppendEdgeGeometryPoints(
    NXOpen::Edge* edge,
    const double centroid[3],
    std::vector<double>& circleCenterDistances,
    std::vector<Point3>& vertexPoints,
    std::vector<Point3>& circleCenterPoints,
    std::vector<Point3>& lineEdgePoints,
    std::vector<Point3>& curveEdgePoints,
    std::vector<Point3>& arcEdgePoints,
    std::vector<Point3>& fullCircleEdgePoints)
{
    double firstVertex[3] = {};
    double secondVertex[3] = {};
    int vertexCount = 0;
    if (UF_MODL_ask_edge_verts(edge->Tag(), firstVertex, secondVertex, &vertexCount) != 0)
    {
        vertexCount = 0;
    }

    AppendUniqueEdgeVertices(vertexCount, firstVertex, secondVertex, vertexPoints);

    const NXOpen::Edge::EdgeType edgeType = edge->SolidEdgeType();
    double center[3] = {};
    const bool hasCircularCenter = AskCircularEdgeCenter(edge->Tag(), center);
    const bool isCircularEdge =
        edgeType == NXOpen::Edge::EdgeTypeCircular || hasCircularCenter;
    if (isCircularEdge && vertexCount == 0)
    {
        if (hasCircularCenter)
        {
            const Point3 centerPoint = PointFromArray(center);
            circleCenterPoints.push_back(centerPoint);
            fullCircleEdgePoints.push_back(centerPoint);
            circleCenterDistances.push_back(DistanceBetweenPoints(centroid, center));
        }
        return;
    }

    if (isCircularEdge)
    {
        AppendEdgeVerticesToGroup(vertexCount, firstVertex, secondVertex, arcEdgePoints);
        return;
    }

    if (edgeType == NXOpen::Edge::EdgeTypeLinear)
    {
        AppendEdgeVerticesToGroup(vertexCount, firstVertex, secondVertex, lineEdgePoints);
    }
    else
    {
        AppendEdgeVerticesToGroup(vertexCount, firstVertex, secondVertex, curveEdgePoints);
    }
}

void ColorMatchedGroups(
    NXOpen::Session* session,
    const std::vector<std::vector<tag_t> >& matchedGroups,
    std::vector<tag_t>& colorFailedTags)
{
    if (session == NULL || matchedGroups.empty())
    {
        return;
    }

    std::vector<int> colors = GenerateRandomGroupColors(matchedGroups.size());
    for (std::size_t groupIndex = 0; groupIndex < matchedGroups.size(); ++groupIndex)
    {
        const int colorIndex = colors[groupIndex];
        const std::vector<tag_t>& groupTags = matchedGroups[groupIndex];
        std::vector<NXOpen::DisplayableObject*> displayableObjects;
        displayableObjects.reserve(groupTags.size());

        for (std::size_t tagIndex = 0; tagIndex < groupTags.size(); ++tagIndex)
        {
            NXOpen::TaggedObject* taggedObject = NXOpen::NXObjectManager::Get(groupTags[tagIndex]);
            NXOpen::DisplayableObject* displayableObject =
                dynamic_cast<NXOpen::DisplayableObject*>(taggedObject);
            if (displayableObject != NULL)
            {
                displayableObjects.push_back(displayableObject);
            }
            else
            {
                colorFailedTags.push_back(groupTags[tagIndex]);
            }
        }

        if (displayableObjects.empty())
        {
            continue;
        }

        try
        {
            NXOpen::DisplayModification* modification = session->DisplayManager()->NewDisplayModification();
            modification->SetApplyToAllFaces(true);
            modification->SetApplyToOwningParts(false);
            modification->SetNewColor(colorIndex);
            modification->Apply(displayableObjects);
            delete modification;
        }
        catch (const NXOpen::NXException& ex)
        {
            (void)ex;
            colorFailedTags.insert(colorFailedTags.end(), groupTags.begin(), groupTags.end());
        }
    }

    session->DisplayManager()->MakeUpToDate();
}

int AskPartUnits(tag_t partTag)
{
    int units = UF_PART_METRIC;
    if (UF_PART_ask_units(partTag, &units) != 0)
    {
        return UF_PART_METRIC;
    }
    return units;
}

int AskOwningPartUnits(tag_t objectTag)
{
    tag_t owningPartTag = NULL_TAG;
    if (UF_OBJ_ask_owning_part(objectTag, &owningPartTag) != 0 || owningPartTag == NULL_TAG)
    {
        return UF_PART_METRIC;
    }
    return AskPartUnits(owningPartTag);
}

int GetMassPropsUnitsCode(int partUnits)
{
    return partUnits == UF_PART_ENGLISH ? 1 : 3;
}

double ConvertMassPropsLengthToPartUnits(double value, int partUnits, int massPropsUnitsCode)
{
    if (partUnits == UF_PART_ENGLISH)
    {
        return value;
    }
    if (massPropsUnitsCode == 3)
    {
        return value * 10.0;
    }
    if (massPropsUnitsCode == 4)
    {
        return value * 1000.0;
    }
    return value;
}

std::string FormatLengthBuckets(const std::vector<LengthBucket>& buckets)
{
    std::ostringstream stream;
    for (std::size_t index = 0; index < buckets.size(); ++index)
    {
        if (index != 0)
        {
            stream << ", ";
        }
        stream << FormatDouble(buckets[index].length) << " x " << buckets[index].count;
    }
    return stream.str();
}

void AskBodyMassProperties(
    tag_t bodyTag,
    double* mass,
    double centroid[3])
{
    double accuracyValues[11] = {0.99, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    double massProps[47] = {};
    double statistics[13] = {};
    tag_t objects[1] = {bodyTag};
    const int partUnits = AskOwningPartUnits(bodyTag);
    const int massPropsUnitsCode = GetMassPropsUnitsCode(partUnits);

    ThrowUfError(UF_MODL_ask_mass_props_3d(
        objects,
        1,
        1,
        massPropsUnitsCode,
        0.0,
        1,
        accuracyValues,
        massProps,
        statistics));

    if (mass != NULL)
    {
        *mass = massProps[2];
    }
    if (centroid != NULL)
    {
        centroid[0] = ConvertMassPropsLengthToPartUnits(massProps[3], partUnits, massPropsUnitsCode);
        centroid[1] = ConvertMassPropsLengthToPartUnits(massProps[4], partUnits, massPropsUnitsCode);
        centroid[2] = ConvertMassPropsLengthToPartUnits(massProps[5], partUnits, massPropsUnitsCode);
    }
}

double AskEdgeLength(tag_t edgeTag)
{
    double length = 0.0;
    ThrowUfError(UF_CURVE_ask_arc_length(edgeTag, 0.0, 1.0, UF_MODL_UNITS_PART, &length));
    return length;
}

std::vector<LengthBucket> BuildLengthBuckets(std::vector<double> lengths)
{
    std::sort(lengths.begin(), lengths.end());

    std::vector<LengthBucket> buckets;
    for (std::size_t index = 0; index < lengths.size(); ++index)
    {
        const double length = lengths[index];
        if (buckets.empty() || !NearlyEqual(buckets.back().length, length, kLengthTolerance))
        {
            LengthBucket bucket = {};
            bucket.length = length;
            bucket.count = 1;
            buckets.push_back(bucket);
            continue;
        }

        LengthBucket& bucket = buckets.back();
        bucket.length = (bucket.length * static_cast<double>(bucket.count) + length) /
                        static_cast<double>(bucket.count + 1);
        ++bucket.count;
    }

    return buckets;
}

std::vector<double> SortLengthsDescending(std::vector<double> lengths)
{
    std::sort(lengths.begin(), lengths.end(), std::greater<double>());
    return lengths;
}

bool LengthBucketsMatch(
    const std::vector<LengthBucket>& referenceBuckets,
    const std::vector<LengthBucket>& candidateBuckets,
    double tolerance)
{
    if (referenceBuckets.size() != candidateBuckets.size())
    {
        return false;
    }

    for (std::size_t index = 0; index < referenceBuckets.size(); ++index)
    {
        if (!NearlyEqual(referenceBuckets[index].length, candidateBuckets[index].length, tolerance) ||
            referenceBuckets[index].count != candidateBuckets[index].count)
        {
            return false;
        }
    }

    return true;
}

std::string PlaneFaceSignatureMismatch(
    const PlaneFaceFeature& reference,
    const PlaneFaceFeature& candidate)
{
    if (!FaceAreasNearlyEqual(reference.area, candidate.area))
    {
        std::ostringstream stream;
        stream << "area mismatch: ref=" << FormatDouble(reference.area)
               << ", cand=" << FormatDouble(candidate.area)
               << ", diff=" << FormatDouble(std::fabs(reference.area - candidate.area))
               << ", tol=" << FormatDouble(AbsoluteRelativeTolerance(
                      reference.area,
                      candidate.area,
                      kFaceAreaAbsoluteTolerance,
                      kFaceAreaRelativeTolerance));
        return stream.str();
    }

    if (!NearlyEqual(reference.perimeter, candidate.perimeter, kLengthTolerance))
    {
        std::ostringstream stream;
        stream << "perimeter mismatch: ref=" << FormatDouble(reference.perimeter)
               << ", cand=" << FormatDouble(candidate.perimeter)
               << ", diff=" << FormatDouble(std::fabs(reference.perimeter - candidate.perimeter))
               << ", tol=" << FormatDouble(kLengthTolerance);
        return stream.str();
    }

    if (reference.edgeCount != candidate.edgeCount)
    {
        std::ostringstream stream;
        stream << "edge count mismatch: ref=" << reference.edgeCount
               << ", cand=" << candidate.edgeCount;
        return stream.str();
    }

    if (reference.lengthBuckets.size() != candidate.lengthBuckets.size())
    {
        std::ostringstream stream;
        stream << "face edge-length bucket count mismatch: ref="
               << reference.lengthBuckets.size()
               << ", cand=" << candidate.lengthBuckets.size()
               << ", refBuckets=" << FormatLengthBuckets(reference.lengthBuckets)
               << ", candBuckets=" << FormatLengthBuckets(candidate.lengthBuckets);
        return stream.str();
    }

    for (std::size_t index = 0; index < reference.lengthBuckets.size(); ++index)
    {
        const LengthBucket& referenceBucket = reference.lengthBuckets[index];
        const LengthBucket& candidateBucket = candidate.lengthBuckets[index];
        if (!NearlyEqual(referenceBucket.length, candidateBucket.length, kLengthTolerance))
        {
            std::ostringstream stream;
            stream << "face edge-length bucket " << (index + 1)
                   << " length mismatch: ref=" << FormatDouble(referenceBucket.length)
                   << ", cand=" << FormatDouble(candidateBucket.length)
                   << ", diff=" << FormatDouble(std::fabs(referenceBucket.length - candidateBucket.length))
                   << ", tol=" << FormatDouble(kLengthTolerance)
                   << ", refBuckets=" << FormatLengthBuckets(reference.lengthBuckets)
                   << ", candBuckets=" << FormatLengthBuckets(candidate.lengthBuckets);
            return stream.str();
        }

        if (referenceBucket.count != candidateBucket.count)
        {
            std::ostringstream stream;
            stream << "face edge-length bucket " << (index + 1)
                   << " count mismatch: length=" << FormatDouble(referenceBucket.length)
                   << ", ref=" << referenceBucket.count
                   << ", cand=" << candidateBucket.count
                   << ", refBuckets=" << FormatLengthBuckets(reference.lengthBuckets)
                   << ", candBuckets=" << FormatLengthBuckets(candidate.lengthBuckets);
            return stream.str();
        }
    }

    return "";
}

bool SamePlaneFaceSignature(
    const PlaneFaceFeature& reference,
    const PlaneFaceFeature& candidate)
{
    return PlaneFaceSignatureMismatch(reference, candidate).empty();
}

Point3 BuildFallbackXAxis(const Point3& zAxis)
{
    Point3 reference = std::fabs(zAxis.x) < 0.8 ? MakePoint3(1.0, 0.0, 0.0) : MakePoint3(0.0, 1.0, 0.0);
    Point3 xAxis = CrossPoint(reference, zAxis);
    if (!NormalizePoint(xAxis))
    {
        xAxis = MakePoint3(1.0, 0.0, 0.0);
    }
    return xAxis;
}

bool BuildFrameXAxisFromFaceEdges(
    NXOpen::Face* face,
    const Point3& zAxis,
    Point3& xAxis,
    std::vector<double>& edgeLengths)
{
    bool foundAxis = false;
    double bestLength = 0.0;
    std::vector<NXOpen::Edge*> faceEdges = face->GetEdges();
    edgeLengths.reserve(faceEdges.size());

    for (std::size_t index = 0; index < faceEdges.size(); ++index)
    {
        NXOpen::Edge* edge = faceEdges[index];
        const double length = AskEdgeLength(edge->Tag());
        edgeLengths.push_back(length);

        NXOpen::Point3d startPoint;
        NXOpen::Point3d endPoint;
        edge->GetVertices(&startPoint, &endPoint);

        Point3 direction = SubtractPoints(PointFromNx(endPoint), PointFromNx(startPoint));
        direction = ProjectOutAxis(direction, zAxis);
        if (!NormalizePoint(direction))
        {
            continue;
        }

        if (!foundAxis || length > bestLength + kLengthTolerance)
        {
            xAxis = direction;
            bestLength = length;
            foundAxis = true;
        }
    }

    return foundAxis;
}

bool AskFaceOutwardNormal(NXOpen::Face* face, Point3& normal)
{
    NXOpen::Session* session = NXOpen::Session::GetSession();
    if (face == NULL || session == NULL || session->Parts() == NULL || session->Parts()->Work() == NULL)
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

    const NXOpen::Vector3d vector = direction->Vector();
    normal = MakePoint3(vector.X, vector.Y, vector.Z);
    return NormalizePoint(normal);
}

bool BuildPlanarFaceFeature(NXOpen::Face* face, PlaneFaceFeature& feature)
{
    int faceType = 0;
    if (UF_MODL_ask_face_type(face->Tag(), &faceType) != 0 || faceType != UF_MODL_PLANAR_FACE)
    {
        return false;
    }

    int dataType = 0;
    double point[3] = {};
    double normal[3] = {};
    double box[6] = {};
    double radius = 0.0;
    double radiusData = 0.0;
    int normalDirection = 0;
    ThrowUfError(UF_MODL_ask_face_data(
        face->Tag(),
        &dataType,
        point,
        normal,
        box,
        &radius,
        &radiusData,
        &normalDirection));

    Point3 zAxis = {};
    if (!AskFaceOutwardNormal(face, zAxis))
    {
        zAxis = PointFromArray(normal);
        if (normalDirection < 0)
        {
            zAxis = ScalePoint(zAxis, -1.0);
        }
    }
    if (!NormalizePoint(zAxis))
    {
        return false;
    }

    std::vector<NXOpen::ISurface*> faces;
    faces.push_back(face);

    double area = 0.0;
    double perimeter = 0.0;
    double radiusDiameter = 0.0;
    NXOpen::Point3d cog;
    double minimumRadiusOfCurvature = 0.0;
    double areaErrorEstimate = 0.0;
    NXOpen::Point3d anchorPoint;
    bool isApproximate = false;

    NXOpen::Session::GetSession()->Measurement()->GetFaceProperties(
        faces,
        0.99,
        NXOpen::Measurement::AlternateFaceRadius,
        true,
        &area,
        &perimeter,
        &radiusDiameter,
        &cog,
        &minimumRadiusOfCurvature,
        &areaErrorEstimate,
        &anchorPoint,
        &isApproximate);

    std::vector<double> edgeLengths;
    Point3 xAxis = {};
    if (!BuildFrameXAxisFromFaceEdges(face, zAxis, xAxis, edgeLengths))
    {
        xAxis = BuildFallbackXAxis(zAxis);
    }

    Point3 yAxis = CrossPoint(zAxis, xAxis);
    if (!NormalizePoint(yAxis))
    {
        return false;
    }

    xAxis = CrossPoint(yAxis, zAxis);
    if (!NormalizePoint(xAxis))
    {
        return false;
    }

    feature.tag = face->Tag();
    feature.area = area;
    feature.perimeter = perimeter;
    feature.edgeCount = static_cast<int>(edgeLengths.size());
    feature.lengthBuckets = BuildLengthBuckets(edgeLengths);
    feature.frame.origin = PointFromNx(cog);
    feature.frame.xAxis = xAxis;
    feature.frame.yAxis = yAxis;
    feature.frame.zAxis = zAxis;
    return true;
}

std::vector<PlaneFaceGroup> BuildPlaneFaceGroups(const std::vector<PlaneFaceFeature>& planeFaces)
{
    std::vector<PlaneFaceGroup> groups;
    for (std::size_t faceIndex = 0; faceIndex < planeFaces.size(); ++faceIndex)
    {
        bool addedToGroup = false;
        for (std::size_t groupIndex = 0; groupIndex < groups.size(); ++groupIndex)
        {
            const PlaneFaceFeature& groupFeature = planeFaces[groups[groupIndex].faceIndexes[0]];
            if (SamePlaneFaceSignature(groupFeature, planeFaces[faceIndex]))
            {
                groups[groupIndex].faceIndexes.push_back(faceIndex);
                addedToGroup = true;
                break;
            }
        }

        if (!addedToGroup)
        {
            PlaneFaceGroup group = {};
            group.faceIndexes.push_back(faceIndex);
            groups.push_back(group);
        }
    }

    return groups;
}

BodyFingerprint BuildFingerprint(NXOpen::Body* body)
{
    BodyFingerprint fingerprint = {};
    fingerprint.body = body;
    fingerprint.tag = body->Tag();
    AskBodyMassProperties(
        fingerprint.tag,
        &fingerprint.mass,
        fingerprint.centroid);

    std::vector<NXOpen::Edge*> edges = body->GetEdges();
    fingerprint.edgeCount = static_cast<int>(edges.size());

    std::vector<double> edgeLengths;
    edgeLengths.reserve(edges.size());
    fingerprint.vertexPoints.reserve(edges.size() * 2);
    fingerprint.circleCenterPoints.reserve(edges.size());
    fingerprint.circleCenterDistances.reserve(edges.size());
    fingerprint.lineEdgePoints.reserve(edges.size() * 2);
    fingerprint.curveEdgePoints.reserve(edges.size() * 2);
    fingerprint.arcEdgePoints.reserve(edges.size() * 2);
    fingerprint.fullCircleEdgePoints.reserve(edges.size());
    for (std::size_t index = 0; index < edges.size(); ++index)
    {
        NXOpen::Edge* edge = edges[index];
        const double edgeLength = AskEdgeLength(edge->Tag());
        edgeLengths.push_back(edgeLength);

        double firstVertex[3] = {};
        double secondVertex[3] = {};
        int vertexCount = 0;
        if (UF_MODL_ask_edge_verts(edge->Tag(), firstVertex, secondVertex, &vertexCount) != 0)
        {
            vertexCount = 0;
        }
        double center[3] = {};
        const NXOpen::Edge::EdgeType edgeType = edge->SolidEdgeType();
        const bool hasCircularCenter = AskCircularEdgeCenter(edge->Tag(), center);
        const bool isCircularEdge =
            edgeType == NXOpen::Edge::EdgeTypeCircular || hasCircularCenter;
        if (isCircularEdge && vertexCount == 0)
        {
            fingerprint.fullCircleEdgeLengths.push_back(edgeLength);
        }
        else if (isCircularEdge)
        {
            fingerprint.arcEdgeLengths.push_back(edgeLength);
        }
        else if (edgeType == NXOpen::Edge::EdgeTypeLinear)
        {
            fingerprint.lineEdgeLengths.push_back(edgeLength);
        }
        else
        {
            fingerprint.curveEdgeLengths.push_back(edgeLength);
        }

        AppendEdgeGeometryPoints(
            edge,
            fingerprint.centroid,
            fingerprint.circleCenterDistances,
            fingerprint.vertexPoints,
            fingerprint.circleCenterPoints,
            fingerprint.lineEdgePoints,
            fingerprint.curveEdgePoints,
            fingerprint.arcEdgePoints,
            fingerprint.fullCircleEdgePoints);
    }

    fingerprint.lengthBuckets = BuildLengthBuckets(edgeLengths);
    fingerprint.fullCircleEdgeLengths = SortLengthsDescending(fingerprint.fullCircleEdgeLengths);
    fingerprint.arcEdgeLengths = SortLengthsDescending(fingerprint.arcEdgeLengths);
    fingerprint.curveEdgeLengths = SortLengthsDescending(fingerprint.curveEdgeLengths);
    fingerprint.lineEdgeLengths = SortLengthsDescending(fingerprint.lineEdgeLengths);
    std::sort(fingerprint.circleCenterDistances.begin(), fingerprint.circleCenterDistances.end());

    std::vector<NXOpen::Face*> faces = body->GetFaces();
    fingerprint.faceCount = static_cast<int>(faces.size());
    fingerprint.planeFaces.reserve(faces.size());
    for (std::size_t index = 0; index < faces.size(); ++index)
    {
        PlaneFaceFeature faceFeature = {};
        if (BuildPlanarFaceFeature(faces[index], faceFeature))
        {
            fingerprint.planeFaces.push_back(faceFeature);
        }
    }
    fingerprint.planeFaceGroups = BuildPlaneFaceGroups(fingerprint.planeFaces);

    return fingerprint;
}

const BodyFingerprint* FindFingerprintByTag(
    const std::vector<BodyFingerprint>& fingerprints,
    tag_t tag)
{
    for (std::size_t index = 0; index < fingerprints.size(); ++index)
    {
        if (fingerprints[index].tag == tag)
        {
            return &fingerprints[index];
        }
    }
    return NULL;
}

bool CompareDistanceVectors(
    const std::vector<double>& referenceDistances,
    const std::vector<double>& candidateDistances,
    const char* distanceName,
    std::string& rejectReason)
{
    if (referenceDistances.size() != candidateDistances.size())
    {
        std::ostringstream stream;
        stream << distanceName << " count mismatch: ref=" << referenceDistances.size()
               << ", cand=" << candidateDistances.size();
        rejectReason = stream.str();
        return false;
    }

    for (std::size_t index = 0; index < referenceDistances.size(); ++index)
    {
        if (!NearlyEqual(referenceDistances[index], candidateDistances[index], kDistanceTolerance))
        {
            std::ostringstream stream;
            stream << distanceName << " " << (index + 1) << " mismatch: ref="
                   << FormatDouble(referenceDistances[index]) << ", cand="
                   << FormatDouble(candidateDistances[index]);
            rejectReason = stream.str();
            return false;
        }
    }

    return true;
}

struct LocalCoordinateSignature
{
    Point3 centroid;
    std::vector<Point3> vertexLocalPoints;
    std::vector<Point3> lineEdgeLocalPoints;
    std::vector<Point3> curveEdgeLocalPoints;
    std::vector<Point3> arcEdgeLocalPoints;
    std::vector<Point3> fullCircleEdgeLocalPoints;
};

Point3 TransformWorldPointToLocal(const Frame3& frame, const Point3& worldPoint)
{
    const Point3 delta = SubtractPoints(worldPoint, frame.origin);
    return MakePoint3(
        DotPoint(delta, frame.xAxis),
        DotPoint(delta, frame.yAxis),
        DotPoint(delta, frame.zAxis));
}

Point3 TransformBodyCentroidToLocal(const Frame3& frame, const double centroid[3])
{
    return TransformWorldPointToLocal(frame, PointFromArray(centroid));
}

void AppendTransformedPoints(
    const Frame3& frame,
    const std::vector<Point3>& worldPoints,
    std::vector<Point3>& localPoints)
{
    localPoints.reserve(worldPoints.size());
    for (std::size_t index = 0; index < worldPoints.size(); ++index)
    {
        localPoints.push_back(TransformWorldPointToLocal(frame, worldPoints[index]));
    }
}

LocalCoordinateSignature BuildLocalCoordinateSignature(
    const BodyFingerprint& fingerprint,
    const Frame3& frame)
{
    LocalCoordinateSignature signature = {};
    signature.centroid = TransformBodyCentroidToLocal(frame, fingerprint.centroid);

    AppendTransformedPoints(frame, fingerprint.vertexPoints, signature.vertexLocalPoints);
    AppendTransformedPoints(frame, fingerprint.lineEdgePoints, signature.lineEdgeLocalPoints);
    AppendTransformedPoints(frame, fingerprint.curveEdgePoints, signature.curveEdgeLocalPoints);
    AppendTransformedPoints(frame, fingerprint.arcEdgePoints, signature.arcEdgeLocalPoints);
    AppendTransformedPoints(frame, fingerprint.fullCircleEdgePoints, signature.fullCircleEdgeLocalPoints);

    return signature;
}

Point3 ApplyCoordinateVariant(const Point3& point, int variantIndex)
{
    const bool swapXY = (variantIndex & 1) != 0;
    const int signX = (variantIndex & 2) != 0 ? -1 : 1;
    const int signY = (variantIndex & 4) != 0 ? -1 : 1;

    const double sourceX = swapXY ? point.y : point.x;
    const double sourceY = swapXY ? point.x : point.y;
    return MakePoint3(
        static_cast<double>(signX) * sourceX,
        static_cast<double>(signY) * sourceY,
        point.z);
}

void ApplyPointVariant(
    const std::vector<Point3>& source,
    int variantIndex,
    std::vector<Point3>& target)
{
    target.reserve(source.size());
    for (std::size_t index = 0; index < source.size(); ++index)
    {
        target.push_back(ApplyCoordinateVariant(source[index], variantIndex));
    }
}

LocalCoordinateSignature ApplyCoordinateVariant(
    const LocalCoordinateSignature& signature,
    int variantIndex)
{
    LocalCoordinateSignature result = {};
    result.centroid = ApplyCoordinateVariant(signature.centroid, variantIndex);
    ApplyPointVariant(signature.vertexLocalPoints, variantIndex, result.vertexLocalPoints);
    ApplyPointVariant(signature.lineEdgeLocalPoints, variantIndex, result.lineEdgeLocalPoints);
    ApplyPointVariant(signature.curveEdgeLocalPoints, variantIndex, result.curveEdgeLocalPoints);
    ApplyPointVariant(signature.arcEdgeLocalPoints, variantIndex, result.arcEdgeLocalPoints);
    ApplyPointVariant(signature.fullCircleEdgeLocalPoints, variantIndex, result.fullCircleEdgeLocalPoints);
    return result;
}

bool LocalPointLess(const Point3& lhs, const Point3& rhs)
{
    if (lhs.x < rhs.x)
    {
        return true;
    }

    if (rhs.x < lhs.x)
    {
        return false;
    }

    if (lhs.y < rhs.y)
    {
        return true;
    }

    if (rhs.y < lhs.y)
    {
        return false;
    }

    return lhs.z < rhs.z;
}

std::vector<Point3> SortedLocalPoints(const std::vector<Point3>& points)
{
    std::vector<Point3> sortedPoints = points;
    std::sort(sortedPoints.begin(), sortedPoints.end(), LocalPointLess);
    return sortedPoints;
}

double PointAxisValue(const Point3& point, int axisIndex)
{
    switch (axisIndex)
    {
    case 0:
        return point.x;
    case 1:
        return point.y;
    default:
        break;
    }

    return point.z;
}

const char* AxisName(int axisIndex)
{
    switch (axisIndex)
    {
    case 0:
        return "X";
    case 1:
        return "Y";
    default:
        break;
    }

    return "Z";
}

std::vector<double> SortedPointAxisValues(const std::vector<Point3>& points, int axisIndex)
{
    std::vector<double> values;
    values.reserve(points.size());
    for (std::size_t index = 0; index < points.size(); ++index)
    {
        values.push_back(PointAxisValue(points[index], axisIndex));
    }

    std::sort(values.begin(), values.end());
    return values;
}

void WriteDoubleVectorDebugLog(
    std::ofstream* debugLog,
    const char* label,
    const std::vector<double>& values)
{
    if (debugLog == NULL || !debugLog->is_open())
    {
        return;
    }

    (*debugLog) << label << " count=" << values.size() << ":";
    for (std::size_t index = 0; index < values.size(); ++index)
    {
        (*debugLog) << " [" << (index + 1) << "]=" << FormatDouble(values[index]);
    }
    (*debugLog) << std::endl;
}

void WritePointVectorDebugLog(
    std::ofstream* debugLog,
    const char* label,
    const std::vector<Point3>& points)
{
    if (debugLog == NULL || !debugLog->is_open())
    {
        return;
    }

    (*debugLog) << label << " count=" << points.size() << " axis sorted" << std::endl;
    for (int axisIndex = 0; axisIndex < 3; ++axisIndex)
    {
        const std::vector<double> values = SortedPointAxisValues(points, axisIndex);
        std::ostringstream axisLabel;
        axisLabel << "    " << AxisName(axisIndex);
        WriteDoubleVectorDebugLog(debugLog, axisLabel.str().c_str(), values);
    }
}

void WriteLocalCoordinateSignatureDebugLog(
    std::ofstream* debugLog,
    const char* label,
    const LocalCoordinateSignature& signature)
{
    if (debugLog == NULL || !debugLog->is_open())
    {
        return;
    }

    (*debugLog) << label << " local centroid=" << FormatPoint(signature.centroid) << std::endl;
    WritePointVectorDebugLog(debugLog, "  unique vertex local points", signature.vertexLocalPoints);
    WritePointVectorDebugLog(debugLog, "  line edge local points", signature.lineEdgeLocalPoints);
    WritePointVectorDebugLog(debugLog, "  curve edge local points", signature.curveEdgeLocalPoints);
    WritePointVectorDebugLog(debugLog, "  arc edge local points", signature.arcEdgeLocalPoints);
    WritePointVectorDebugLog(debugLog, "  full circle center local points", signature.fullCircleEdgeLocalPoints);
}

bool PointsNearlyEqual(const Point3& reference, const Point3& candidate)
{
    return NearlyEqual(reference.x, candidate.x, kDistanceTolerance) &&
           NearlyEqual(reference.y, candidate.y, kDistanceTolerance) &&
           NearlyEqual(reference.z, candidate.z, kDistanceTolerance);
}

bool ComparePointValues(
    const std::vector<Point3>& referencePoints,
    const std::vector<Point3>& candidatePoints,
    const char* valueName,
    std::string& rejectReason)
{
    if (referencePoints.size() != candidatePoints.size())
    {
        std::ostringstream stream;
        stream << valueName << " count mismatch: ref=" << referencePoints.size()
               << ", cand=" << candidatePoints.size();
        rejectReason = stream.str();
        return false;
    }

    for (int axisIndex = 0; axisIndex < 3; ++axisIndex)
    {
        const std::vector<double> sortedReference =
            SortedPointAxisValues(referencePoints, axisIndex);
        const std::vector<double> sortedCandidate =
            SortedPointAxisValues(candidatePoints, axisIndex);
        for (std::size_t index = 0; index < sortedReference.size(); ++index)
        {
            if (!NearlyEqual(sortedReference[index], sortedCandidate[index], kDistanceTolerance))
            {
                std::ostringstream stream;
                stream << valueName << " " << AxisName(axisIndex) << " " << (index + 1)
                       << " mismatch: ref=" << FormatDouble(sortedReference[index])
                       << ", cand=" << FormatDouble(sortedCandidate[index]);
                rejectReason = stream.str();
                return false;
            }
        }
    }

    return true;
}

bool LocalCoordinateSignaturesMatch(
    const LocalCoordinateSignature& reference,
    const LocalCoordinateSignature& candidate,
    int variantIndex,
    std::string& rejectReason)
{
    const LocalCoordinateSignature candidateVariant = ApplyCoordinateVariant(candidate, variantIndex);

    if (!ComparePointValues(reference.fullCircleEdgeLocalPoints, candidateVariant.fullCircleEdgeLocalPoints, "local full circle center", rejectReason) ||
        !ComparePointValues(reference.arcEdgeLocalPoints, candidateVariant.arcEdgeLocalPoints, "local arc edge endpoint", rejectReason) ||
        !ComparePointValues(reference.curveEdgeLocalPoints, candidateVariant.curveEdgeLocalPoints, "local curve edge point", rejectReason) ||
        !ComparePointValues(reference.lineEdgeLocalPoints, candidateVariant.lineEdgeLocalPoints, "local line edge point", rejectReason))
    {
        return false;
    }

    rejectReason.clear();
    return true;
}

int FindMatchingPlaneFaceGroup(
    const BodyFingerprint& fingerprint,
    const PlaneFaceFeature& referenceFace,
    std::size_t expectedCount,
    std::ofstream* debugLog)
{
    for (std::size_t groupIndex = 0; groupIndex < fingerprint.planeFaceGroups.size(); ++groupIndex)
    {
        const PlaneFaceGroup& group = fingerprint.planeFaceGroups[groupIndex];
        if (group.faceIndexes.size() != expectedCount || group.faceIndexes.empty())
        {
            if (debugLog != NULL && debugLog->is_open())
            {
                (*debugLog) << "Candidate plane group skipped: groupIndex=" << groupIndex
                            << ", expectedCount=" << expectedCount
                            << ", actualCount=" << group.faceIndexes.size()
                            << std::endl;
            }
            continue;
        }

        const PlaneFaceFeature& candidateFace = fingerprint.planeFaces[group.faceIndexes[0]];
        const std::string mismatchReason = PlaneFaceSignatureMismatch(referenceFace, candidateFace);
        if (mismatchReason.empty())
        {
            if (debugLog != NULL && debugLog->is_open())
            {
                (*debugLog) << "Candidate plane group matched: groupIndex=" << groupIndex
                            << ", faceTag=" << candidateFace.tag
                            << ", expectedCount=" << expectedCount
                            << std::endl;
            }
            return static_cast<int>(groupIndex);
        }

        if (debugLog != NULL && debugLog->is_open())
        {
            (*debugLog) << "Candidate plane group signature mismatch: groupIndex="
                        << groupIndex
                        << ", faceTag=" << candidateFace.tag
                        << ", expectedCount=" << expectedCount
                        << ", area=" << FormatDouble(candidateFace.area)
                        << ", perimeter=" << FormatDouble(candidateFace.perimeter)
                        << ", edgeCount=" << candidateFace.edgeCount
                        << ", lengthBuckets=" << FormatLengthBuckets(candidateFace.lengthBuckets)
                        << ", reason=" << mismatchReason
                        << std::endl;
        }
    }

    return -1;
}

int FindLargestPlaneFaceGroup(
    const BodyFingerprint& fingerprint,
    std::size_t expectedCount)
{
    int bestGroupIndex = -1;
    double bestArea = -1.0;
    for (std::size_t groupIndex = 0; groupIndex < fingerprint.planeFaceGroups.size(); ++groupIndex)
    {
        const PlaneFaceGroup& group = fingerprint.planeFaceGroups[groupIndex];
        if (group.faceIndexes.size() != expectedCount || group.faceIndexes.empty())
        {
            continue;
        }

        const std::size_t faceIndex = group.faceIndexes[0];
        if (faceIndex >= fingerprint.planeFaces.size())
        {
            continue;
        }

        const double area = fingerprint.planeFaces[faceIndex].area;
        if (bestGroupIndex < 0 || FaceAreaSignificantlyGreater(area, bestArea))
        {
            bestGroupIndex = static_cast<int>(groupIndex);
            bestArea = area;
        }
    }

    return bestGroupIndex;
}

bool TryAnchorPlaneGroupMatch(
    const BodyFingerprint& reference,
    const PlaneFaceGroup& referenceGroup,
    const BodyFingerprint& candidate,
    const PlaneFaceGroup& candidateGroup,
    std::ofstream* debugLog,
    int* coordinateDebugStep,
    std::vector<tag_t>* activeDebugObjectTags,
    bool* coordinateDebugCanceled,
    std::string& rejectReason)
{
    if (candidateGroup.faceIndexes.empty())
    {
        rejectReason = "candidate anchor group is empty";
        return false;
    }

    if (referenceGroup.faceIndexes.empty())
    {
        rejectReason = "reference anchor group is empty";
        return false;
    }

    const PlaneFaceFeature& referenceFace = reference.planeFaces[referenceGroup.faceIndexes[0]];
    const LocalCoordinateSignature referenceSignature =
        BuildLocalCoordinateSignature(reference, referenceFace.frame);

    std::string firstLocalRejectReason;
    for (std::size_t candIndex = 0; candIndex < candidateGroup.faceIndexes.size(); ++candIndex)
    {
        const PlaneFaceFeature& candidateFace = candidate.planeFaces[candidateGroup.faceIndexes[candIndex]];
        const LocalCoordinateSignature candidateSignature =
            BuildLocalCoordinateSignature(candidate, candidateFace.frame);

        static const int zRotationVariants[] = {0, 3, 5, 6};
        for (std::size_t variant = 0; variant < sizeof(zRotationVariants) / sizeof(zRotationVariants[0]); ++variant)
        {
            const int variantIndex = zRotationVariants[variant];
            const LocalCoordinateSignature candidateVariantForLog =
                ApplyCoordinateVariant(candidateSignature, variantIndex);
            const Frame3 candidateVariantFrame =
                ApplyFrameCoordinateVariant(candidateFace.frame, variantIndex);

            if (!CreateAndPauseCoordinateDebugStep(
                    coordinateDebugStep,
                    activeDebugObjectTags,
                    coordinateDebugCanceled,
                    reference.tag,
                    candidate.tag,
                    referenceFace.tag,
                    candidateFace.tag,
                    variantIndex,
                    referenceFace.frame,
                    candidateVariantFrame))
            {
                if (debugLog != NULL && debugLog->is_open())
                {
                    (*debugLog) << "Coordinate debug canceled by user; current coordinate set kept."
                                << std::endl;
                }
                rejectReason = "coordinate debug canceled by user";
                return false;
            }

            if (debugLog != NULL && debugLog->is_open())
            {
                (*debugLog) << "Recognition attempt: refTag=" << reference.tag
                            << ", candTag=" << candidate.tag
                            << ", refFaceTag=" << referenceFace.tag
                            << ", candFaceTag=" << candidateFace.tag
                            << ", " << FormatCoordinateVariantName(variantIndex)
                            << std::endl;
                WriteLocalCoordinateSignatureDebugLog(debugLog, "  reference", referenceSignature);
                WriteLocalCoordinateSignatureDebugLog(debugLog, "  candidate", candidateVariantForLog);
            }

            std::string localRejectReason;
            if (LocalCoordinateSignaturesMatch(
                    referenceSignature,
                    candidateSignature,
                    variantIndex,
                    localRejectReason))
            {
                if (debugLog != NULL && debugLog->is_open())
                {
                    (*debugLog) << "Recognition attempt result: matched" << std::endl;
                }
                rejectReason.clear();
                return true;
            }

            if (debugLog != NULL && debugLog->is_open())
            {
                (*debugLog) << "Recognition attempt result: failed, reason="
                            << localRejectReason << std::endl;
            }

            if (firstLocalRejectReason.empty())
            {
                std::ostringstream stream;
                stream << "anchor local coordinate mismatch: refFaceTag=" << referenceFace.tag
                       << ", candFaceTag=" << candidateFace.tag
                       << ", " << FormatCoordinateVariantName(variantIndex)
                       << ", reason=" << localRejectReason;
                firstLocalRejectReason = stream.str();
            }
        }
    }

    rejectReason = firstLocalRejectReason.empty()
        ? "anchor local coordinate mismatch"
        : firstLocalRejectReason;
    return false;
}

bool BodiesMatchByAnchorPlaneCoordinates(
    const BodyFingerprint& reference,
    const BodyFingerprint& candidate,
    std::ofstream* debugLog,
    int* coordinateDebugStep,
    std::vector<tag_t>* activeDebugObjectTags,
    bool* coordinateDebugCanceled,
    std::string& rejectReason)
{
    std::string firstAnchorRejectReason;
    for (std::size_t expectedCount = 1; expectedCount <= 2; ++expectedCount)
    {
        const int referenceGroupIndex = FindLargestPlaneFaceGroup(reference, expectedCount);
        if (referenceGroupIndex < 0)
        {
            if (debugLog != NULL && debugLog->is_open())
            {
                (*debugLog) << "Reference anchor group missing: expectedCount="
                            << expectedCount << std::endl;
            }
            continue;
        }

        const PlaneFaceGroup& referenceGroup =
            reference.planeFaceGroups[static_cast<std::size_t>(referenceGroupIndex)];
        const PlaneFaceFeature& referenceFace =
            reference.planeFaces[referenceGroup.faceIndexes[0]];

        int candidateGroupIndex = -1;
        if (expectedCount == 1)
        {
            candidateGroupIndex = FindMatchingPlaneFaceGroup(
                candidate,
                referenceFace,
                expectedCount,
                debugLog);
        }
        else
        {
            candidateGroupIndex = FindLargestPlaneFaceGroup(candidate, expectedCount);
        }

        if (candidateGroupIndex < 0)
        {
            std::string currentRejectReason = expectedCount == 1
                ? "candidate largest unique anchor plane group mismatch"
                : "candidate largest two-count anchor plane group missing";
            if (debugLog != NULL && debugLog->is_open())
            {
                (*debugLog) << "Anchor attempt skipped: expectedCount=" << expectedCount
                            << ", reason=" << currentRejectReason
                            << std::endl;
            }
            if (firstAnchorRejectReason.empty())
            {
                firstAnchorRejectReason = currentRejectReason;
            }
            if (expectedCount == 1)
            {
                continue;
            }

            rejectReason = currentRejectReason;
            return false;
        }

        const PlaneFaceGroup& candidateGroup =
            candidate.planeFaceGroups[static_cast<std::size_t>(candidateGroupIndex)];
        const PlaneFaceFeature& candidateFace =
            candidate.planeFaces[candidateGroup.faceIndexes[0]];

        if (debugLog != NULL && debugLog->is_open())
        {
            (*debugLog) << "Fixed reference plane group selected: expectedCount="
                        << expectedCount
                        << ", refGroupIndex=" << referenceGroupIndex
                        << ", refFaceTag=" << referenceFace.tag
                        << ", refArea=" << FormatDouble(referenceFace.area)
                        << ", candGroupIndex=" << candidateGroupIndex
                        << ", candFirstFaceTag=" << candidateFace.tag
                        << ", candArea=" << FormatDouble(candidateFace.area)
                        << std::endl;
        }

        if (expectedCount == 2 && !SamePlaneFaceSignature(referenceFace, candidateFace))
        {
            rejectReason = "candidate largest two-count anchor plane group mismatch: " +
                PlaneFaceSignatureMismatch(referenceFace, candidateFace);
            if (debugLog != NULL && debugLog->is_open())
            {
                (*debugLog) << "Anchor attempt failed: expectedCount=" << expectedCount
                            << ", reason=" << rejectReason
                            << std::endl;
            }
            return false;
        }

        std::string anchorRejectReason;
        if (TryAnchorPlaneGroupMatch(
                reference,
                referenceGroup,
                candidate,
                candidateGroup,
                debugLog,
                coordinateDebugStep,
                activeDebugObjectTags,
                coordinateDebugCanceled,
                anchorRejectReason))
        {
            return true;
        }

        std::string currentRejectReason = anchorRejectReason.empty()
            ? "fixed reference anchor local coordinate mismatch"
            : anchorRejectReason;
        if (debugLog != NULL && debugLog->is_open())
        {
            (*debugLog) << "Anchor attempt failed: expectedCount=" << expectedCount
                        << ", reason=" << currentRejectReason
                        << std::endl;
        }
        if (firstAnchorRejectReason.empty())
        {
            firstAnchorRejectReason = currentRejectReason;
        }
        if (expectedCount == 1)
        {
            continue;
        }

        rejectReason = currentRejectReason;
        return false;
    }

    rejectReason = firstAnchorRejectReason.empty()
        ? "no unique or two-count planar anchor group"
        : firstAnchorRejectReason;
    return false;
}

bool RoughFingerprintsMatch(
    const BodyFingerprint& reference,
    const BodyFingerprint& candidate,
    std::string& rejectReason)
{
    if (reference.edgeCount != candidate.edgeCount)
    {
        std::ostringstream stream;
        stream << "rough edge count mismatch: ref=" << reference.edgeCount
               << ", cand=" << candidate.edgeCount;
        rejectReason = stream.str();
        return false;
    }

    if (reference.faceCount != candidate.faceCount)
    {
        std::ostringstream stream;
        stream << "rough face count mismatch: ref=" << reference.faceCount
               << ", cand=" << candidate.faceCount;
        rejectReason = stream.str();
        return false;
    }

    rejectReason.clear();
    return true;
}

bool SortedLengthsMatch(
    const std::vector<double>& referenceLengths,
    const std::vector<double>& candidateLengths,
    const char* className,
    std::string& rejectReason)
{
    if (referenceLengths.size() != candidateLengths.size())
    {
        std::ostringstream stream;
        stream << "middle " << className << " edge count mismatch: ref="
               << referenceLengths.size() << ", cand=" << candidateLengths.size();
        rejectReason = stream.str();
        return false;
    }

    for (std::size_t index = 0; index < referenceLengths.size(); ++index)
    {
        if (!NearlyEqual(referenceLengths[index], candidateLengths[index], kLengthTolerance))
        {
            std::ostringstream stream;
            stream << "middle " << className << " edge length " << (index + 1)
                   << " mismatch: ref=" << FormatDouble(referenceLengths[index])
                   << ", cand=" << FormatDouble(candidateLengths[index]);
            rejectReason = stream.str();
            return false;
        }
    }

    return true;
}

bool MiddleFingerprintsMatch(
    const BodyFingerprint& reference,
    const BodyFingerprint& candidate,
    std::string& rejectReason)
{
    if (!SortedLengthsMatch(reference.fullCircleEdgeLengths, candidate.fullCircleEdgeLengths, "full circle", rejectReason) ||
        !SortedLengthsMatch(reference.arcEdgeLengths, candidate.arcEdgeLengths, "arc", rejectReason) ||
        !SortedLengthsMatch(reference.curveEdgeLengths, candidate.curveEdgeLengths, "curve", rejectReason) ||
        !SortedLengthsMatch(reference.lineEdgeLengths, candidate.lineEdgeLengths, "line", rejectReason))
    {
        return false;
    }

    rejectReason.clear();
    return true;
}

bool RefinedFingerprintsMatch(
    const BodyFingerprint& reference,
    const BodyFingerprint& candidate,
    std::ofstream* debugLog,
    int* coordinateDebugStep,
    std::vector<tag_t>* activeDebugObjectTags,
    bool* coordinateDebugCanceled,
    std::string& rejectReason)
{
    if (!BodiesMatchByAnchorPlaneCoordinates(
            reference,
            candidate,
            debugLog,
            coordinateDebugStep,
            activeDebugObjectTags,
            coordinateDebugCanceled,
            rejectReason))
    {
        return false;
    }

    rejectReason.clear();
    return true;
}


std::vector<std::vector<tag_t> > RunSameBodySearch(
    NXOpen::Session* session,
    NXOpen::UI* ui,
    const std::vector<NXOpen::Body*>& selectedBodies)
{

        NXOpen::ListingWindow* listingWindow = kShowResultListingWindow ? session->ListingWindow() : NULL;
        if (listingWindow != NULL && !listingWindow->IsOpen())
        {
            listingWindow->Open();
        }

        std::ofstream debugLog;
        if (kDebugLogEnabled)
        {
            debugLog.open(GetMatchDebugLogPath().c_str(), std::ios::out | std::ios::trunc);
        }
        if (debugLog.is_open())
        {
            debugLog << "DuoSiTiZuanZuanPei match debug log" << std::endl;
            debugLog << "Tolerance: length=" << FormatDouble(kLengthTolerance)
                     << ", mass=" << FormatDouble(kMassTolerance)
                     << ", distance=" << FormatDouble(kDistanceTolerance)
                     << ", faceAreaAbs=" << FormatDouble(kFaceAreaAbsoluteTolerance)
                     << ", faceAreaRel=" << FormatDouble(kFaceAreaRelativeTolerance)
                     << std::endl;
            debugLog << "Selected bodies=" << selectedBodies.size() << std::endl;
            debugLog << "Coordinate debug pause is "
                     << (kCoordinateDebugPauseEnabled ? "enabled" : "disabled")
                     << ", axisLength=" << FormatDouble(kCoordinateDebugAxisLength)
                     << std::endl;
            debugLog << "Z rotation variants only: "
                     << FormatCoordinateVariantName(0) << "; "
                     << FormatCoordinateVariantName(3) << "; "
                     << FormatCoordinateVariantName(5) << "; "
                     << FormatCoordinateVariantName(6) << std::endl;
        }

        std::vector<BodyFingerprint> fingerprints;
        std::vector<tag_t> failedTags;
        fingerprints.reserve(selectedBodies.size());
        for (std::size_t index = 0; index < selectedBodies.size(); ++index)
        {
            try
            {
                BodyFingerprint fingerprint = BuildFingerprint(selectedBodies[index]);
                WriteFingerprintDebugLog(debugLog, index, fingerprint);
                WritePlaneFaceGroupsDebugLog(debugLog, index, fingerprint);
                fingerprints.push_back(fingerprint);
            }
            catch (const NXOpen::NXException& ex)
            {
                if (debugLog.is_open())
                {
                    debugLog << "Build fingerprint failed: bodyIndex=" << (index + 1)
                             << ", tag=" << selectedBodies[index]->Tag()
                             << ", errorCode=" << ex.ErrorCode()
                             << std::endl;
                }
                failedTags.push_back(selectedBodies[index]->Tag());
            }
        }

        std::vector<bool> grouped(fingerprints.size(), false);
        std::vector<std::vector<tag_t> > matchedGroups;
        std::vector<tag_t> colorFailedTags;
        int coordinateDebugStep = 0;
        std::vector<tag_t> activeDebugObjectTags;
        bool coordinateDebugCanceled = false;
        for (std::size_t referenceIndex = 0; referenceIndex < fingerprints.size(); ++referenceIndex)
        {
            if (coordinateDebugCanceled)
            {
                break;
            }

            if (grouped[referenceIndex])
            {
                continue;
            }

            grouped[referenceIndex] = true;
            std::vector<tag_t> groupTags;
            groupTags.push_back(fingerprints[referenceIndex].tag);

            for (std::size_t candidateIndex = referenceIndex + 1; candidateIndex < fingerprints.size(); ++candidateIndex)
            {
                if (grouped[candidateIndex])
                {
                    continue;
                }

                std::string rejectReason;
                if (!RoughFingerprintsMatch(
                        fingerprints[referenceIndex],
                        fingerprints[candidateIndex],
                        rejectReason))
                {
                    if (debugLog.is_open())
                    {
                        debugLog << "Compare failed: refIndex=" << (referenceIndex + 1)
                                 << ", refTag=" << fingerprints[referenceIndex].tag
                                 << ", candIndex=" << (candidateIndex + 1)
                                 << ", candTag=" << fingerprints[candidateIndex].tag
                                 << ", stage=rough"
                                 << ", reason=" << rejectReason
                                 << std::endl;
                    }
                    continue;
                }

                if (!MiddleFingerprintsMatch(
                        fingerprints[referenceIndex],
                        fingerprints[candidateIndex],
                        rejectReason))
                {
                    if (debugLog.is_open())
                    {
                        debugLog << "Compare failed: refIndex=" << (referenceIndex + 1)
                                 << ", refTag=" << fingerprints[referenceIndex].tag
                                 << ", candIndex=" << (candidateIndex + 1)
                                 << ", candTag=" << fingerprints[candidateIndex].tag
                                 << ", stage=middle"
                                 << ", reason=" << rejectReason
                                 << std::endl;
                    }
                    continue;
                }

                if (!RefinedFingerprintsMatch(
                        fingerprints[referenceIndex],
                        fingerprints[candidateIndex],
                        &debugLog,
                        &coordinateDebugStep,
                        &activeDebugObjectTags,
                        &coordinateDebugCanceled,
                        rejectReason))
                {
                    if (debugLog.is_open())
                    {
                        debugLog << "Compare failed: refIndex=" << (referenceIndex + 1)
                                 << ", refTag=" << fingerprints[referenceIndex].tag
                                 << ", candIndex=" << (candidateIndex + 1)
                                 << ", candTag=" << fingerprints[candidateIndex].tag
                                 << ", stage=refined"
                                 << ", reason=" << rejectReason
                                 << std::endl;
                    }
                    if (coordinateDebugCanceled)
                    {
                        break;
                    }
                    continue;
                }

                if (debugLog.is_open())
                {
                    debugLog << "Compare matched: refIndex=" << (referenceIndex + 1)
                             << ", refTag=" << fingerprints[referenceIndex].tag
                             << ", candIndex=" << (candidateIndex + 1)
                             << ", candTag=" << fingerprints[candidateIndex].tag
                             << std::endl;
                }
                grouped[candidateIndex] = true;
                groupTags.push_back(fingerprints[candidateIndex].tag);
            }

            if (coordinateDebugCanceled)
            {
                break;
            }

            if (groupTags.size() > 1)
            {
                matchedGroups.push_back(groupTags);
                if (debugLog.is_open())
                {
                    debugLog << "Matched group " << matchedGroups.size()
                             << ": count=" << groupTags.size()
                             << ", tags=";
                    for (std::size_t tagIndex = 0; tagIndex < groupTags.size(); ++tagIndex)
                    {
                        if (tagIndex != 0)
                        {
                            debugLog << ", ";
                        }
                        debugLog << groupTags[tagIndex];
                    }
                    debugLog << std::endl;
                }
            }
        }

        if (coordinateDebugCanceled)
        {
            if (debugLog.is_open())
            {
                debugLog << "Coordinate debug canceled; active coordinate objects were kept."
                         << std::endl;
            }
        }
        else
        {
            DeleteDebugCoordinateObjects(&activeDebugObjectTags);
        }

        if (listingWindow != NULL)
        {
            listingWindow->WriteLine("");
            WriteLineUtf8(
                listingWindow,
                Utf8Text("\x3D\x3D\x3D\x3D\x20\xE7\x9B\xB8\xE5\x90\x8C\xE4\xBD\x93\xE7\xBB\x93\xE6\x9E\x9C\x20\x3D\x3D\x3D\x3D"));
            if (coordinateDebugCanceled)
            {
                WriteLineUtf8(
                    listingWindow,
                    "Coordinate debug canceled; current coordinate set is kept in the model.");
            }
            else if (matchedGroups.empty())
            {
                WriteLineUtf8(
                    listingWindow,
                    Utf8Text("\xE6\x9C\xAA\xE6\x89\xBE\xE5\x88\xB0\xE7\x9B\xB8\xE5\x90\x8C\xE4\xBD\x93\xE7\xBB\x84\xE3\x80\x82"));
            }
            else
            {
                for (std::size_t groupIndex = 0; groupIndex < matchedGroups.size(); ++groupIndex)
                {
                    const std::vector<tag_t>& groupTags = matchedGroups[groupIndex];
                    std::ostringstream line;
                    line << Utf8Text("\xE7\xAC\xAC\x20") << (groupIndex + 1)
                         << Utf8Text("\x20\xE7\xBB\x84\xEF\xBC\x9A\xE6\x95\xB0\xE9\x87\x8F\x3D")
                         << groupTags.size()
                         << Utf8Text("\xEF\xBC\x8C\xE6\xA0\x87\xE7\xAD\xBE\x3D");
                    AppendTags(line, groupTags);
                    const BodyFingerprint* targetFingerprint = FindFingerprintByTag(fingerprints, groupTags[0]);
                    if (targetFingerprint != NULL)
                    {
                        line << Utf8Text("\xEF\xBC\x8C\xE8\xB4\xA8\xE5\xBF\x83\x3D")
                             << FormatPoint(targetFingerprint->centroid);
                    }
                    WriteLineUtf8(listingWindow, line.str());
                }
            }

            if (!failedTags.empty())
            {
                std::ostringstream line;
                line << Utf8Text("\xE8\xAF\xBB\xE5\x8F\x96\xE5\xA4\xB1\xE8\xB4\xA5\xEF\xBC\x9A\xE6\x95\xB0\xE9\x87\x8F\x3D")
                     << failedTags.size();
                WriteLineUtf8(listingWindow, line.str());
            }

            if (!colorFailedTags.empty())
            {
                std::ostringstream line;
                line << Utf8Text("\xE6\x94\xB9\xE8\x89\xB2\xE5\xA4\xB1\xE8\xB4\xA5\xEF\xBC\x9A\xE6\x95\xB0\xE9\x87\x8F\x3D")
                     << colorFailedTags.size();
                WriteLineUtf8(listingWindow, line.str());
            }
        }

        if (debugLog.is_open())
        {
            debugLog << "Matched groups=" << matchedGroups.size()
                     << ", buildFailed=" << failedTags.size()
                     << ", colorFailed=" << colorFailedTags.size()
                     << std::endl;
        }

        if (kShowCompletionMessageBox)
        {
            std::string completeMessageText;
            if (coordinateDebugCanceled)
            {
                completeMessageText = "Coordinate debug canceled. Current coordinate set is kept in the model.";
            }
            else if (matchedGroups.empty())
            {
                completeMessageText = Utf8Text("\xE6\xAF\x94\xE8\xBE\x83\xE5\xAE\x8C\xE6\x88\x90\xEF\xBC\x8C\xE6\x9C\xAA\xE6\x89\xBE\xE5\x88\xB0\xE7\x9B\xB8\xE5\x90\x8C\xE4\xBD\x93\xEF\xBC\x8C\xE8\xAF\xB7\xE6\x9F\xA5\xE7\x9C\x8B\xE4\xBF\xA1\xE6\x81\xAF\xE7\xAA\x97\xE5\x8F\xA3\xE7\xBB\x93\xE6\x9E\x9C\xE3\x80\x82");
            }
            else if (!colorFailedTags.empty())
            {
                completeMessageText = Utf8Text("\xE6\xAF\x94\xE8\xBE\x83\xE5\xAE\x8C\xE6\x88\x90\xEF\xBC\x8C\xE9\x83\xA8\xE5\x88\x86\xE5\xAE\x9E\xE4\xBD\x93\xE6\x94\xB9\xE8\x89\xB2\xE5\xA4\xB1\xE8\xB4\xA5\xEF\xBC\x8C\xE8\xAF\xB7\xE6\x9F\xA5\xE7\x9C\x8B\xE4\xBF\xA1\xE6\x81\xAF\xE7\xAA\x97\xE5\x8F\xA3\xE7\xBB\x93\xE6\x9E\x9C\xE3\x80\x82");
            }
            else
            {
                completeMessageText = Utf8Text("\xE6\xAF\x94\xE8\xBE\x83\xE5\xAE\x8C\xE6\x88\x90\xEF\xBC\x8C\xE7\x9B\xB8\xE5\x90\x8C\xE4\xBD\x93\xE5\xB7\xB2\xE6\x8C\x89\xE5\xAE\x9A\xE4\xBD\x8D\xE5\xB9\xB3\xE9\x9D\xA2\xE5\xB1\x80\xE9\x83\xA8\xE5\x9D\x90\xE6\xA0\x87\xE9\xAA\x8C\xE8\xAF\x81\xE5\xB9\xB6\xE9\x9A\x8F\xE6\x9C\xBA\xE6\x94\xB9\xE8\x89\xB2\xEF\xBC\x8C\xE8\xAF\xB7\xE6\x9F\xA5\xE7\x9C\x8B\xE4\xBF\xA1\xE6\x81\xAF\xE7\xAA\x97\xE5\x8F\xA3\xE7\xBB\x93\xE6\x9E\x9C\xE3\x80\x82");
            }
            NXOpen::NXString completeMessage(
                completeMessageText,
                NXOpen::NXString::UTF8);
            ui->NXMessageBox()->Show(
                NXOpen::NXString(kDialogName),
                NXOpen::NXMessageBox::DialogTypeInformation,
                completeMessage);
        }
        return matchedGroups;
    
}
}

