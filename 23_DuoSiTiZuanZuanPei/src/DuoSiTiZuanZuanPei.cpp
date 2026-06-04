#include <NXOpen/BlockStyler_BlockDialog.hxx>
#include <NXOpen/BlockStyler_Button.hxx>
#include <NXOpen/BlockStyler_CompositeBlock.hxx>
#include <NXOpen/BlockStyler_Node.hxx>
#include <NXOpen/BlockStyler_PropertyList.hxx>
#include <NXOpen/BlockStyler_SelectObject.hxx>
#include <NXOpen/BlockStyler_Tree.hxx>
#include <NXOpen/BlockStyler_UIBlock.hxx>
#include <NXOpen/Assemblies_AssemblyManager.hxx>
#include <NXOpen/Assemblies_CreateNewComponentBuilder.hxx>
#include <NXOpen/Body.hxx>
#include <NXOpen/Callback.hxx>
#include <NXOpen/Direction.hxx>
#include <NXOpen/DirectionCollection.hxx>
#include <NXOpen/Edge.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/DisplayManager.hxx>
#include <NXOpen/DisplayModification.hxx>
#include <NXOpen/DisplayableObject.hxx>
#include <NXOpen/FileNew.hxx>
#include <NXOpen/ISurface.hxx>
#include <NXOpen/ListingWindow.hxx>
#include <NXOpen/Measurement.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXMessageBox.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/NXString.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/PartSaveStatus.hxx>
#include <NXOpen/Selection.hxx>
#include <NXOpen/SelectDisplayableObjectList.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/SmartObject.hxx>
#include <NXOpen/TaggedObject.hxx>
#include <NXOpen/UI.hxx>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#undef CreateDialog

#include <uf.h>
#include <uf_assem.h>
#include <uf_csys.h>
#include <uf_curve.h>
#include <uf_disp.h>
#include <uf_eval.h>
#include <uf_exit.h>
#include <uf_mtx.h>
#include <uf_modl.h>
#include <uf_modl_utilities.h>
#include <uf_obj.h>
#include <uf_object_types.h>
#include <uf_part.h>
#include <uf_ui_types.h>

#include <algorithm>
#include <cctype>
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

#include "../../../protection/native/ZhihuiLicenseGuard.hpp"

#ifndef DllExport
#define DllExport __declspec(dllexport)
#endif

extern "C" DllExport void ufusr(char* param, int* retcode, int param_len);

namespace
{
const char* kDialogName = "DuoSiTiZuanZuanPei";
const char* kDialogFile = "DuoSiTiZuanZuanPei.dlx";
const char* kBodySelectionBlockId = "bodySelection";
const char* kOutputFolderBlockId = "nativeFolderBrowser0";
const char* kColorMatchedBodiesToggleBlockId = "colorMatchedBodiesToggle";
const char* kFindSameBodiesButtonBlockId = "findSameBodiesButton";
const char* kNameConfigButtonBlockId = "nameConfigButton";
const char* kAssemblyListBlockId = "assemblyList";
const double kLengthTolerance = 0.05;
const double kMassTolerance = 0.05;
const double kDistanceTolerance = 0.05;
const double kFaceAreaAbsoluteTolerance = 0.05;
const double kFaceAreaRelativeTolerance = 0.0001;
const bool kDebugLogEnabled = false;
const bool kCoordinateDebugPauseEnabled = false;
const bool kShowCompletionMessageBox = false;
const bool kWriteResultToListingWindow = false;
const bool kConvertMatchedGroupsToAssembly = true;
const bool kDeleteOriginalMatchedBodiesAfterAssembly = true;
const double kCoordinateDebugAxisLength = 50.0;
const int kMinimumColorIndex = 1;
const int kMaximumColorIndex = 216;
const int kDebugXAxisColor = 186;
const int kDebugYAxisColor = 70;
const int kDebugZAxisColor = 36;
const int kDebugCsysColor = 141;
const int kPreferredGroupColors[] = {6, 36, 42, 70, 96, 103, 121, 141, 151, 159, 186, 211};
const int kAssemblyNameColumnId = 1;
const int kAssemblyParentColumnId = 2;
const int kAssemblyQuantityColumnId = 3;
const char* kComponentTemplateFileName = "model-plain-1-mm-template.prt";
const char* kDefaultComponentNamePrefix = "SameBody";
const char* kDefaultNameFormat = "\x7B\xE8\x87\xAA\xE5\xAE\x9A\xE4\xB9\x89\x7D\x5F\x7B\xE6\xB5\x81\xE6\xB0\xB4\xE5\x8F\xB7\x7D";
const char* kDefaultBodyNameAttribute = "DB_PART_NAME";
const char* kNameConfigFileName = "DuoSiTiZuanZuanPei_name_config.ini";
std::string gSelectedOutputDirectory;
bool gColorMatchedBodies = true;

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

std::string GetPreviewDebugLogPath()
{
    const std::string moduleDirectory = GetCurrentModuleDirectory();
    if (moduleDirectory.empty())
    {
        return "DuoSiTiZuanZuanPei_preview.log";
    }

    return moduleDirectory + "DuoSiTiZuanZuanPei_preview.log";
}

std::string GetNameConfigPath()
{
    const std::string moduleDirectory = GetCurrentModuleDirectory();
    if (moduleDirectory.empty())
    {
        return kNameConfigFileName;
    }

    return moduleDirectory + kNameConfigFileName;
}

void ResetPreviewDebugLog()
{
    if (!kDebugLogEnabled)
    {
        return;
    }

    std::ofstream log(GetPreviewDebugLogPath().c_str(), std::ios::out | std::ios::trunc);
    if (log.is_open())
    {
        log << "DuoSiTiZuanZuanPei preview callback log" << std::endl;
    }
}

void WritePreviewDebugLog(const std::string& text)
{
    if (!kDebugLogEnabled)
    {
        return;
    }

    std::ofstream log(GetPreviewDebugLogPath().c_str(), std::ios::out | std::ios::app);
    if (log.is_open())
    {
        log << GetTickCount() << " " << text << std::endl;
    }
}

void LogCallbackError(const std::string& context, const std::string& message)
{
    WritePreviewDebugLog(context + ": " + message);

    NXOpen::Session* session = NXOpen::Session::GetSession();
    if (session == NULL)
    {
        return;
    }

    NXOpen::ListingWindow* listingWindow = session->ListingWindow();
    if (listingWindow == NULL)
    {
        return;
    }

    try
    {
        listingWindow->Open();
        listingWindow->WriteLine((context + ": " + message).c_str());
    }
    catch (...)
    {
    }
}

std::string PointerText(const void* pointer)
{
    std::ostringstream stream;
    stream << pointer;
    return stream.str();
}

std::string SafeBlockName(NXOpen::BlockStyler::UIBlock* block)
{
    if (block == NULL)
    {
        return std::string();
    }

    try
    {
        return block->Name().GetLocaleText();
    }
    catch (...)
    {
        return std::string();
    }
}

bool IsBlockNamed(NXOpen::BlockStyler::UIBlock* block, const char* blockName)
{
    return SafeBlockName(block) == std::string(blockName);
}

NXOpen::BlockStyler::UIBlock* FindBlockRecursive(
    NXOpen::BlockStyler::CompositeBlock* rootBlock,
    const char* blockName)
{
    if (rootBlock == NULL)
    {
        return NULL;
    }

    NXOpen::BlockStyler::UIBlock* directBlock = rootBlock->FindBlock(blockName);
    if (directBlock != NULL)
    {
        return directBlock;
    }

    std::vector<NXOpen::BlockStyler::UIBlock*> childBlocks = rootBlock->GetBlocks();
    for (std::size_t index = 0; index < childBlocks.size(); ++index)
    {
        NXOpen::BlockStyler::UIBlock* childBlock = childBlocks[index];
        if (IsBlockNamed(childBlock, blockName))
        {
            return childBlock;
        }

        NXOpen::BlockStyler::CompositeBlock* childComposite =
            dynamic_cast<NXOpen::BlockStyler::CompositeBlock*>(childBlock);
        NXOpen::BlockStyler::UIBlock* nestedBlock =
            FindBlockRecursive(childComposite, blockName);
        if (nestedBlock != NULL)
        {
            return nestedBlock;
        }
    }

    return NULL;
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
    int edgeCount;
    int faceCount;
    std::vector<LengthBucket> lengthBuckets;
    std::vector<Point3> vertexPoints;
    std::vector<Point3> circleCenterPoints;
    std::vector<Point3> lineEdgePoints;
    std::vector<Point3> curveEdgePoints;
    std::vector<Point3> arcEdgePoints;
    std::vector<Point3> fullCircleEdgePoints;
    std::vector<double> lineEdgeLengths;
    std::vector<double> curveEdgeLengths;
    std::vector<double> arcEdgeLengths;
    std::vector<double> fullCircleEdgeLengths;
    std::vector<PlaneFaceFeature> planeFaces;
    std::vector<PlaneFaceGroup> planeFaceGroups;
    bool planeFeaturesBuilt;
};

struct RigidTransform3
{
    double origin[3];
    double csysMatrix[9];
};

struct BodyMatchInfo
{
    Frame3 referenceFrame;
    Frame3 candidateFrame;
    int variantIndex;
    tag_t referenceFaceTag;
    tag_t candidateFaceTag;
};

struct MatchedBodyInstance
{
    tag_t bodyTag;
    RigidTransform3 transformFromReference;
};

struct MatchedBodyGroup
{
    std::vector<MatchedBodyInstance> instances;
};

struct SameBodySearchData
{
    std::vector<BodyFingerprint> fingerprints;
    std::vector<tag_t> failedTags;
    std::vector<std::vector<tag_t> > matchedGroups;
    std::vector<MatchedBodyGroup> assemblyGroups;
    std::vector<tag_t> activeDebugObjectTags;
    bool coordinateDebugCanceled;
};

struct AssemblyPreviewRow
{
    MatchedBodyGroup group;
    std::vector<tag_t> tags;
    std::string componentName;
    std::string parentName;
};

struct AssemblyConversionResult
{
    int convertedGroups;
    int createdComponents;
    int deletedOriginalBodies;
    int failedGroups;
    std::vector<std::string> partFiles;
    std::vector<std::string> failureReasons;
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
             << ", edges=" << fingerprint.edgeCount
             << ", faces=" << fingerprint.faceCount
             << ", edgeLengthBuckets=" << fingerprint.lengthBuckets.size()
             << ", uniqueVertices=" << fingerprint.vertexPoints.size()
             << ", fullCircleCenters=" << fingerprint.circleCenterPoints.size()
             << ", lineEdgePoints=" << fingerprint.lineEdgePoints.size()
             << ", curveEdgePoints=" << fingerprint.curveEdgePoints.size()
             << ", arcEdgePoints=" << fingerprint.arcEdgePoints.size()
             << ", fullCircleEdgePoints=" << fingerprint.fullCircleEdgePoints.size()
             << ", fullCircleEdgeLengths=" << fingerprint.fullCircleEdgeLengths.size()
             << ", arcEdgeLengths=" << fingerprint.arcEdgeLengths.size()
             << ", curveEdgeLengths=" << fingerprint.curveEdgeLengths.size()
             << ", lineEdgeLengths=" << fingerprint.lineEdgeLengths.size()
             << ", planarFaces=" << fingerprint.planeFaces.size()
             << ", planeFaceGroups=" << fingerprint.planeFaceGroups.size()
             << ", planeFeaturesBuilt=" << (fingerprint.planeFeaturesBuilt ? "true" : "false")
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

RigidTransform3 IdentityTransform()
{
    RigidTransform3 transform = {};
    transform.origin[0] = 0.0;
    transform.origin[1] = 0.0;
    transform.origin[2] = 0.0;
    transform.csysMatrix[0] = 1.0;
    transform.csysMatrix[4] = 1.0;
    transform.csysMatrix[8] = 1.0;
    return transform;
}

double FrameAxisComponent(const Point3& axis, int componentIndex)
{
    switch (componentIndex)
    {
    case 0:
        return axis.x;
    case 1:
        return axis.y;
    default:
        break;
    }

    return axis.z;
}

Point3 FrameAxis(const Frame3& frame, int axisIndex)
{
    switch (axisIndex)
    {
    case 0:
        return frame.xAxis;
    case 1:
        return frame.yAxis;
    default:
        break;
    }

    return frame.zAxis;
}

Point3 TransformPointByMatrix(const double matrix[3][3], const Point3& point)
{
    return MakePoint3(
        matrix[0][0] * point.x + matrix[0][1] * point.y + matrix[0][2] * point.z,
        matrix[1][0] * point.x + matrix[1][1] * point.y + matrix[1][2] * point.z,
        matrix[2][0] * point.x + matrix[2][1] * point.y + matrix[2][2] * point.z);
}

RigidTransform3 BuildReferenceToCandidateTransform(
    const Frame3& referenceFrame,
    const Frame3& candidateFrame)
{
    double matrix[3][3] = {};
    for (int row = 0; row < 3; ++row)
    {
        for (int column = 0; column < 3; ++column)
        {
            double value = 0.0;
            for (int axisIndex = 0; axisIndex < 3; ++axisIndex)
            {
                const Point3 candidateAxis = FrameAxis(candidateFrame, axisIndex);
                const Point3 referenceAxis = FrameAxis(referenceFrame, axisIndex);
                value +=
                    FrameAxisComponent(candidateAxis, row) *
                    FrameAxisComponent(referenceAxis, column);
            }
            matrix[row][column] = value;
        }
    }

    const Point3 rotatedReferenceOrigin =
        TransformPointByMatrix(matrix, referenceFrame.origin);
    const Point3 origin =
        SubtractPoints(candidateFrame.origin, rotatedReferenceOrigin);

    RigidTransform3 transform = {};
    CopyPointToArray(origin, transform.origin);

    transform.csysMatrix[0] = matrix[0][0];
    transform.csysMatrix[1] = matrix[1][0];
    transform.csysMatrix[2] = matrix[2][0];
    transform.csysMatrix[3] = matrix[0][1];
    transform.csysMatrix[4] = matrix[1][1];
    transform.csysMatrix[5] = matrix[2][1];
    transform.csysMatrix[6] = matrix[0][2];
    transform.csysMatrix[7] = matrix[1][2];
    transform.csysMatrix[8] = matrix[2][2];

    return transform;
}

double TransformMatrixValue(const RigidTransform3& transform, int row, int column)
{
    return transform.csysMatrix[column * 3 + row];
}

void SetTransformMatrixValue(
    RigidTransform3& transform,
    int row,
    int column,
    double value)
{
    transform.csysMatrix[column * 3 + row] = value;
}

RigidTransform3 TransformFromUfMatrix(const double ufTransform[4][4])
{
    RigidTransform3 transform = {};
    transform.origin[0] = ufTransform[0][3];
    transform.origin[1] = ufTransform[1][3];
    transform.origin[2] = ufTransform[2][3];

    for (int column = 0; column < 3; ++column)
    {
        for (int row = 0; row < 3; ++row)
        {
            SetTransformMatrixValue(transform, row, column, ufTransform[row][column]);
        }
    }

    return transform;
}

RigidTransform3 ComposeTransforms(
    const RigidTransform3& outer,
    const RigidTransform3& inner)
{
    RigidTransform3 result = {};
    for (int row = 0; row < 3; ++row)
    {
        for (int column = 0; column < 3; ++column)
        {
            double value = 0.0;
            for (int k = 0; k < 3; ++k)
            {
                value +=
                    TransformMatrixValue(outer, row, k) *
                    TransformMatrixValue(inner, k, column);
            }
            SetTransformMatrixValue(result, row, column, value);
        }
    }

    for (int row = 0; row < 3; ++row)
    {
        double value = outer.origin[row];
        for (int k = 0; k < 3; ++k)
        {
            value += TransformMatrixValue(outer, row, k) * inner.origin[k];
        }
        result.origin[row] = value;
    }

    return result;
}

RigidTransform3 AskSelectedBodyToAssemblyTransform(tag_t bodyTag)
{
    if (!UF_ASSEM_is_occurrence(bodyTag))
    {
        return IdentityTransform();
    }

    double ufTransform[4][4] = {};
    ThrowUfError(UF_ASSEM_ask_transform_of_occ(bodyTag, ufTransform));
    return TransformFromUfMatrix(ufTransform);
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
    double edgeLength,
    std::vector<Point3>& vertexPoints,
    std::vector<Point3>& circleCenterPoints,
    std::vector<Point3>& lineEdgePoints,
    std::vector<Point3>& curveEdgePoints,
    std::vector<Point3>& arcEdgePoints,
    std::vector<Point3>& fullCircleEdgePoints,
    std::vector<double>& lineEdgeLengths,
    std::vector<double>& curveEdgeLengths,
    std::vector<double>& arcEdgeLengths,
    std::vector<double>& fullCircleEdgeLengths)
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
            fullCircleEdgeLengths.push_back(edgeLength);
        }
        return;
    }

    if (isCircularEdge)
    {
        AppendEdgeVerticesToGroup(vertexCount, firstVertex, secondVertex, arcEdgePoints);
        arcEdgeLengths.push_back(edgeLength);
        return;
    }

    if (edgeType == NXOpen::Edge::EdgeTypeLinear)
    {
        AppendEdgeVerticesToGroup(vertexCount, firstVertex, secondVertex, lineEdgePoints);
        lineEdgeLengths.push_back(edgeLength);
    }
    else
    {
        AppendEdgeVerticesToGroup(vertexCount, firstVertex, secondVertex, curveEdgePoints);
        curveEdgeLengths.push_back(edgeLength);
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

int AskPartUnits(tag_t partTag);

std::string NormalizeDirectoryPath(std::string path)
{
    while (!path.empty() && (path[path.size() - 1] == '\\' || path[path.size() - 1] == '/'))
    {
        path.erase(path.size() - 1);
    }
    return path;
}

std::string DirectoryName(const std::string& path)
{
    const std::string::size_type pos = path.find_last_of("\\/");
    if (pos == std::string::npos)
    {
        return std::string();
    }
    return path.substr(0, pos);
}

std::string FileStem(const std::string& path)
{
    std::string name = path;
    const std::string::size_type slashPos = name.find_last_of("\\/");
    if (slashPos != std::string::npos)
    {
        name = name.substr(slashPos + 1);
    }

    const std::string::size_type dotPos = name.find_last_of('.');
    if (dotPos != std::string::npos)
    {
        name = name.substr(0, dotPos);
    }

    return name.empty() ? "work_part" : name;
}

std::string SanitizeFileStem(const std::string& name)
{
    std::string trimmed = name;
    while (!trimmed.empty() &&
           std::isspace(static_cast<unsigned char>(trimmed[0])) != 0)
    {
        trimmed.erase(trimmed.begin());
    }
    while (!trimmed.empty())
    {
        const unsigned char ch = static_cast<unsigned char>(trimmed[trimmed.size() - 1]);
        if (std::isspace(ch) == 0 && ch != '.')
        {
            break;
        }
        trimmed.erase(trimmed.end() - 1);
    }

    std::string result;
    result.reserve(trimmed.size());
    for (std::size_t index = 0; index < trimmed.size(); ++index)
    {
        const unsigned char ch = static_cast<unsigned char>(trimmed[index]);
        if (ch < 32 || ch == '<' || ch == '>' || ch == ':' || ch == '"' ||
            ch == '/' || ch == '\\' || ch == '|' || ch == '?' || ch == '*')
        {
            result.push_back('_');
            continue;
        }

        result.push_back(static_cast<char>(ch));
    }

    return result.empty() ? "work_part" : result;
}

std::string JoinPath(const std::string& directory, const std::string& name)
{
    if (directory.empty())
    {
        return name;
    }

    const char last = directory[directory.size() - 1];
    if (last == '\\' || last == '/')
    {
        return directory + name;
    }

    return directory + "\\" + name;
}

std::wstring LocaleToWide(const std::string& text)
{
    if (text.empty())
    {
        return std::wstring();
    }

    int utf8Size = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        text.c_str(),
        -1,
        NULL,
        0);
    if (utf8Size > 0)
    {
        std::wstring result(static_cast<std::size_t>(utf8Size - 1), L'\0');
        MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            text.c_str(),
            -1,
            &result[0],
            utf8Size);
        return result;
    }

    int size = MultiByteToWideChar(
        CP_ACP,
        0,
        text.c_str(),
        -1,
        NULL,
        0);
    if (size <= 0)
    {
        return std::wstring();
    }

    std::wstring result(static_cast<std::size_t>(size - 1), L'\0');
    MultiByteToWideChar(
        CP_ACP,
        0,
        text.c_str(),
        -1,
        &result[0],
        size);
    return result;
}

std::string WideToUtf8(const std::wstring& text)
{
    if (text.empty())
    {
        return std::string();
    }

    const int size = WideCharToMultiByte(
        CP_UTF8,
        0,
        text.c_str(),
        -1,
        NULL,
        0,
        NULL,
        NULL);
    if (size <= 0)
    {
        return std::string();
    }

    std::string result(static_cast<std::size_t>(size - 1), '\0');
    WideCharToMultiByte(
        CP_UTF8,
        0,
        text.c_str(),
        -1,
        &result[0],
        size,
        NULL,
        NULL);
    return result;
}

std::string LocaleBytesToUtf8(const std::string& text)
{
    return WideToUtf8(LocaleToWide(text));
}

std::string NxStringToUtf8(const NXOpen::NXString& text)
{
    const char* utf8Text = text.GetUTF8Text();
    if (utf8Text != NULL && utf8Text[0] != '\0')
    {
        return std::string(utf8Text);
    }

    const char* localeText = text.GetLocaleText();
    return localeText != NULL ? LocaleBytesToUtf8(localeText) : std::string();
}

std::string NxStringToLocale(const NXOpen::NXString& text)
{
    const char* localeText = text.GetLocaleText();
    if (localeText != NULL && localeText[0] != '\0')
    {
        return std::string(localeText);
    }

    const char* utf8Text = text.GetUTF8Text();
    return utf8Text != NULL ? std::string(utf8Text) : std::string();
}

std::string TrimText(const std::string& text)
{
    std::string result = text;
    while (!result.empty() &&
           std::isspace(static_cast<unsigned char>(result[0])) != 0)
    {
        result.erase(result.begin());
    }
    while (!result.empty() &&
           std::isspace(static_cast<unsigned char>(result[result.size() - 1])) != 0)
    {
        result.erase(result.end() - 1);
    }
    return result;
}

std::string ReadWholeFile(const std::string& path)
{
    std::ifstream input(path.c_str(), std::ios::in | std::ios::binary);
    if (!input.is_open())
    {
        return std::string();
    }

    std::ostringstream content;
    content << input.rdbuf();
    return content.str();
}

std::string ReadSimpleIniValue(
    const std::string& sectionName,
    const std::string& keyName,
    const std::string& defaultValue)
{
    const std::string content = ReadWholeFile(GetNameConfigPath());
    if (content.empty())
    {
        return defaultValue;
    }

    bool inTargetSection = false;
    std::istringstream lines(content);
    std::string line;
    while (std::getline(lines, line))
    {
        if (!line.empty() && line[line.size() - 1] == '\r')
        {
            line.erase(line.size() - 1);
        }

        const std::string trimmedLine = TrimText(line);
        if (trimmedLine.empty() || trimmedLine[0] == ';' || trimmedLine[0] == '#')
        {
            continue;
        }

        if (trimmedLine.size() >= 2 &&
            trimmedLine[0] == '[' &&
            trimmedLine[trimmedLine.size() - 1] == ']')
        {
            inTargetSection =
                trimmedLine.substr(1, trimmedLine.size() - 2) == sectionName;
            continue;
        }

        if (!inTargetSection)
        {
            continue;
        }

        const std::string::size_type equalPos = trimmedLine.find('=');
        if (equalPos == std::string::npos)
        {
            continue;
        }

        const std::string key = TrimText(trimmedLine.substr(0, equalPos));
        if (key != keyName)
        {
            continue;
        }

        return TrimText(trimmedLine.substr(equalPos + 1));
    }

    return defaultValue;
}

std::string ReplaceAllText(
    std::string text,
    const std::string& token,
    const std::string& replacement)
{
    if (token.empty())
    {
        return text;
    }

    std::string::size_type pos = 0;
    while ((pos = text.find(token, pos)) != std::string::npos)
    {
        text.replace(pos, token.size(), replacement);
        pos += replacement.size();
    }
    return text;
}

void EnsureNameConfigFileExists()
{
    const std::string configPath = GetNameConfigPath();
    std::string nameFormat = ReadSimpleIniValue(
        Utf8Text("\xE5\x91\xBD\xE5\x90\x8D\xE8\xA7\x84\xE5\x88\x99"),
        Utf8Text("\xE5\x90\x8D\xE7\xA7\xB0\xE6\xA0\xBC\xE5\xBC\x8F"),
        std::string());
    std::string customName = ReadSimpleIniValue(
        Utf8Text("\xE5\x91\xBD\xE5\x90\x8D\xE8\xA7\x84\xE5\x88\x99"),
        Utf8Text("\xE8\x87\xAA\xE5\xAE\x9A\xE4\xB9\x89"),
        std::string());
    std::string attributeName = ReadSimpleIniValue(
        Utf8Text("\xE5\x91\xBD\xE5\x90\x8D\xE8\xA7\x84\xE5\x88\x99"),
        Utf8Text("\xE4\xBD\x93\xE5\xB1\x9E\xE6\x80\xA7\xE5\x90\x8D"),
        std::string());

    if (nameFormat.empty())
    {
        const std::string oldRule = ReadSimpleIniValue(
            "Name",
            "Rule",
            "CustomSerial");
        if (oldRule == "BodyAttribute")
        {
            nameFormat = Utf8Text("\x7B\xE4\xBD\x93\xE5\xB1\x9E\xE6\x80\xA7\x7D");
        }
        else if (oldRule == "BodyName")
        {
            nameFormat = Utf8Text("\x7B\xE4\xBD\x93\xE5\x90\x8D\x7D");
        }
        else if (oldRule == "WorkPartSerial")
        {
            nameFormat = Utf8Text("\x7B\xE5\xB7\xA5\xE4\xBD\x9C\xE9\x83\xA8\xE4\xBB\xB6\xE5\x90\x8D\x7D\x5F\x7B\xE6\xB5\x81\xE6\xB0\xB4\xE5\x8F\xB7\x7D");
        }
        else
        {
            nameFormat = kDefaultNameFormat;
        }
    }

    if (customName.empty())
    {
        customName = ReadSimpleIniValue(
            "Name",
            "Prefix",
            kDefaultComponentNamePrefix);
    }
    if (attributeName.empty())
    {
        attributeName = ReadSimpleIniValue(
            "Name",
            "AttributeName",
            kDefaultBodyNameAttribute);
    }

    std::ofstream config(configPath.c_str(), std::ios::out | std::ios::trunc);
    if (!config.is_open())
    {
        return;
    }

    config << Utf8Text("\x23\x20\xE5\x8F\xAF\xE7\x94\xA8\xE5\x8D\xA0\xE4\xBD\x8D\xE7\xAC\xA6\xEF\xBC\x9A\x7B\xE6\xB5\x81\xE6\xB0\xB4\xE5\x8F\xB7\x7D\xE3\x80\x81\x7B\xE8\x87\xAA\xE5\xAE\x9A\xE4\xB9\x89\x7D\xE3\x80\x81\x7B\xE5\xB7\xA5\xE4\xBD\x9C\xE9\x83\xA8\xE4\xBB\xB6\xE5\x90\x8D\x7D\xE3\x80\x81\x7B\xE4\xBD\x93\xE5\x90\x8D\x7D\xE3\x80\x81\x7B\xE4\xBD\x93\xE5\xB1\x9E\xE6\x80\xA7\x7D") << std::endl;
    config << Utf8Text("\x23\x20\xE6\x83\xB3\xE5\x8A\xA0\xE6\xB5\x81\xE6\xB0\xB4\xE5\x8F\xB7\xE5\xB0\xB1\xE5\x9C\xA8\xE5\x90\x8D\xE7\xA7\xB0\xE6\xA0\xBC\xE5\xBC\x8F\xE9\x87\x8C\xE5\x86\x99\x20\x7B\xE6\xB5\x81\xE6\xB0\xB4\xE5\x8F\xB7\x7D\xEF\xBC\x8C\xE4\xB8\x8D\xE5\x86\x99\xE5\xB0\xB1\xE4\xB8\x8D\xE5\x8A\xA0") << std::endl;
    config << Utf8Text("\x23\x20\xE4\xBE\x8B\xE5\xA6\x82\xEF\xBC\x9A\x7B\xE8\x87\xAA\xE5\xAE\x9A\xE4\xB9\x89\x7D\x5F\x7B\xE6\xB5\x81\xE6\xB0\xB4\xE5\x8F\xB7\x7D") << std::endl;
    config << Utf8Text("\x23\x20\xE4\xBE\x8B\xE5\xA6\x82\xEF\xBC\x9A\x7B\xE5\xB7\xA5\xE4\xBD\x9C\xE9\x83\xA8\xE4\xBB\xB6\xE5\x90\x8D\x7D\x5F\x7B\xE6\xB5\x81\xE6\xB0\xB4\xE5\x8F\xB7\x7D") << std::endl;
    config << Utf8Text("\x23\x20\xE4\xBE\x8B\xE5\xA6\x82\xEF\xBC\x9A\x7B\xE4\xBD\x93\xE5\xB1\x9E\xE6\x80\xA7\x7D") << std::endl;
    config << std::endl;
    config << Utf8Text("\x5B\xE5\x91\xBD\xE5\x90\x8D\xE8\xA7\x84\xE5\x88\x99\x5D") << std::endl;
    config << Utf8Text("\xE5\x90\x8D\xE7\xA7\xB0\xE6\xA0\xBC\xE5\xBC\x8F\x3D") << nameFormat << std::endl;
    config << Utf8Text("\xE8\x87\xAA\xE5\xAE\x9A\xE4\xB9\x89\x3D") << customName << std::endl;
    config << Utf8Text("\xE4\xBD\x93\xE5\xB1\x9E\xE6\x80\xA7\xE5\x90\x8D\x3D") << attributeName << std::endl;
}

std::string ReadNameConfigValue(
    const char* section,
    const char* key,
    const char* defaultValue)
{
    EnsureNameConfigFileExists();

    char buffer[MAX_PATH * 4] = {};
    GetPrivateProfileStringA(
        section,
        key,
        defaultValue,
        buffer,
        static_cast<DWORD>(sizeof(buffer)),
        GetNameConfigPath().c_str());
    return std::string(buffer);
}

std::string ReadConfiguredComponentPrefix()
{
    std::string prefix = SanitizeFileStem(
        ReadSimpleIniValue(
            Utf8Text("\xE5\x91\xBD\xE5\x90\x8D\xE8\xA7\x84\xE5\x88\x99"),
            Utf8Text("\xE8\x87\xAA\xE5\xAE\x9A\xE4\xB9\x89"),
            kDefaultComponentNamePrefix));
    return prefix.empty() ? kDefaultComponentNamePrefix : prefix;
}

std::string ReadConfiguredNameFormat()
{
    const std::string nameFormat = ReadSimpleIniValue(
        Utf8Text("\xE5\x91\xBD\xE5\x90\x8D\xE8\xA7\x84\xE5\x88\x99"),
        Utf8Text("\xE5\x90\x8D\xE7\xA7\xB0\xE6\xA0\xBC\xE5\xBC\x8F"),
        kDefaultNameFormat);
    return nameFormat.empty() ? kDefaultNameFormat : nameFormat;
}

std::string ReadConfiguredBodyNameAttribute()
{
    const std::string attributeName = ReadSimpleIniValue(
        Utf8Text("\xE5\x91\xBD\xE5\x90\x8D\xE8\xA7\x84\xE5\x88\x99"),
        Utf8Text("\xE4\xBD\x93\xE5\xB1\x9E\xE6\x80\xA7\xE5\x90\x8D"),
        kDefaultBodyNameAttribute);
    return attributeName.empty() ? kDefaultBodyNameAttribute : attributeName;
}

bool OpenNameConfigFileAndWait()
{
    EnsureNameConfigFileExists();
    const std::wstring configPath = LocaleToWide(GetNameConfigPath());
    if (configPath.empty())
    {
        return false;
    }

    std::wstring commandLine = L"notepad.exe \"";
    commandLine += configPath;
    commandLine += L"\"";

    STARTUPINFOW startupInfo = {};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInfo = {};
    if (CreateProcessW(
            NULL,
            &commandLine[0],
            NULL,
            NULL,
            FALSE,
            0,
            NULL,
            NULL,
            &startupInfo,
            &processInfo))
    {
        WaitForSingleObject(processInfo.hProcess, INFINITE);
        CloseHandle(processInfo.hProcess);
        CloseHandle(processInfo.hThread);
        EnsureNameConfigFileExists();
        return true;
    }

    ShellExecuteW(NULL, L"open", configPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
    return false;
}

bool PathExists(const std::string& path)
{
    const std::wstring widePath = LocaleToWide(path);
    if (widePath.empty())
    {
        return false;
    }

    return GetFileAttributesW(widePath.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool EnsureWideDirectoryExists(const std::wstring& directory)
{
    if (directory.empty())
    {
        return false;
    }

    const DWORD attributes = GetFileAttributesW(directory.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES)
    {
        return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }

    std::wstring parent = directory;
    while (!parent.empty() && (parent[parent.size() - 1] == L'\\' || parent[parent.size() - 1] == L'/'))
    {
        parent.erase(parent.size() - 1);
    }

    const std::wstring::size_type slashPos = parent.find_last_of(L"\\/");
    if (slashPos != std::wstring::npos)
    {
        const std::wstring parentDirectory = parent.substr(0, slashPos);
        if (!parentDirectory.empty() &&
            parentDirectory.find_last_of(L"\\/") != std::wstring::npos &&
            !EnsureWideDirectoryExists(parentDirectory))
        {
            return false;
        }
    }

    if (CreateDirectoryW(directory.c_str(), NULL))
    {
        return true;
    }

    return GetLastError() == ERROR_ALREADY_EXISTS;
}

bool EnsureDirectoryExists(const std::string& directory)
{
    if (directory.empty())
    {
        return false;
    }

    const std::wstring wideDirectory = LocaleToWide(directory);
    if (wideDirectory.empty())
    {
        return false;
    }

    return EnsureWideDirectoryExists(wideDirectory);
}

std::string FormatPaddedNumber(int value, int width)
{
    std::ostringstream stream;
    stream << std::setw(width) << std::setfill('0') << value;
    return stream.str();
}

std::string BuildAssemblyOutputDirectory(NXOpen::Part* workPart)
{
    if (!gSelectedOutputDirectory.empty())
    {
        return gSelectedOutputDirectory;
    }

    std::string directory;
    if (workPart != NULL)
    {
        NXOpen::NXString fullPathNx = workPart->FullPath();
        const std::string fullPath = NxStringToLocale(fullPathNx);
        directory = DirectoryName(fullPath);
    }

    if (directory.empty())
    {
        char tempPathBuffer[MAX_PATH] = {};
        const DWORD tempPathLength = GetTempPathA(MAX_PATH, tempPathBuffer);
        if (tempPathLength > 0 && tempPathLength < MAX_PATH)
        {
            directory = NormalizeDirectoryPath(std::string(tempPathBuffer, tempPathLength));
        }
    }

    if (directory.empty())
    {
        directory = "C:\\Temp";
    }

    return directory;
}

std::string BuildUniqueComponentPartPath(
    const std::string& directory,
    const std::string& componentStem)
{
    const std::string safeStem = SanitizeFileStem(componentStem);
    for (int suffix = 0; suffix < 1000; ++suffix)
    {
        std::ostringstream name;
        name << safeStem;
        if (suffix > 0)
        {
            name << "_" << FormatPaddedNumber(suffix, 3);
        }
        name << ".prt";

        const std::string path = JoinPath(directory, name.str());
        if (!PathExists(path))
        {
            return path;
        }
    }

    std::ostringstream fallback;
    fallback << safeStem << "_new.prt";
    return JoinPath(directory, fallback.str());
}

std::string BuildDefaultComponentName(int groupNumber)
{
    std::ostringstream name;
    name << kDefaultComponentNamePrefix
         << "_"
         << FormatPaddedNumber(groupNumber, 3);
    return name.str();
}

std::string AskBodyObjectName(tag_t bodyTag)
{
    if (bodyTag == NULL_TAG)
    {
        return std::string();
    }

    char name[UF_OBJ_NAME_BUFSIZE] = {};
    if (UF_OBJ_ask_name(bodyTag, name) != 0 || name[0] == '\0')
    {
        return std::string();
    }
    return SanitizeFileStem(LocaleBytesToUtf8(name));
}

std::string AskBodyAttributeAsName(tag_t bodyTag, const std::string& attributeName)
{
    if (bodyTag == NULL_TAG || attributeName.empty())
    {
        return std::string();
    }

    try
    {
        NXOpen::TaggedObject* taggedObject =
            NXOpen::NXObjectManager::Get(bodyTag);
        NXOpen::NXObject* nxObject =
            dynamic_cast<NXOpen::NXObject*>(taggedObject);
        if (nxObject == NULL)
        {
            return std::string();
        }

        return SanitizeFileStem(NxStringToUtf8(
            nxObject->GetUserAttributeAsString(
                attributeName.c_str(),
                NXOpen::NXObject::AttributeTypeAny,
                -1)));
    }
    catch (...)
    {
        return std::string();
    }
}

std::string GetWorkPartNameStem(NXOpen::Session* session)
{
    NXOpen::Part* workPart =
        session != NULL && session->Parts() != NULL
        ? session->Parts()->Work()
        : NULL;
    if (workPart == NULL)
    {
        return std::string();
    }

    return SanitizeFileStem(FileStem(NxStringToUtf8(workPart->FullPath())));
}

std::string BuildSerialComponentName(const std::string& stem, int groupNumber)
{
    std::ostringstream name;
    name << (stem.empty() ? kDefaultComponentNamePrefix : stem)
         << "_"
         << FormatPaddedNumber(groupNumber, 3);
    return name.str();
}

std::string BuildComponentNameForRule(
    NXOpen::Session* session,
    const MatchedBodyGroup& group,
    int groupNumber)
{
    const tag_t referenceTag = group.instances.empty()
        ? NULL_TAG
        : group.instances[0].bodyTag;
    std::string bodyAttributeName = AskBodyAttributeAsName(
        referenceTag,
        ReadConfiguredBodyNameAttribute());
    if (bodyAttributeName.empty())
    {
        bodyAttributeName = BuildDefaultComponentName(groupNumber);
    }

    std::string bodyName = AskBodyObjectName(referenceTag);
    if (bodyName.empty())
    {
        bodyName = BuildDefaultComponentName(groupNumber);
    }

    std::string workPartName = GetWorkPartNameStem(session);
    if (workPartName.empty())
    {
        workPartName = kDefaultComponentNamePrefix;
    }

    std::string name = ReadConfiguredNameFormat();
    name = ReplaceAllText(
        name,
        Utf8Text("\x7B\xE6\xB5\x81\xE6\xB0\xB4\xE5\x8F\xB7\x7D"),
        FormatPaddedNumber(groupNumber, 3));
    name = ReplaceAllText(
        name,
        Utf8Text("\x7B\xE8\x87\xAA\xE5\xAE\x9A\xE4\xB9\x89\x7D"),
        ReadConfiguredComponentPrefix());
    name = ReplaceAllText(
        name,
        Utf8Text("\x7B\xE5\xB7\xA5\xE4\xBD\x9C\xE9\x83\xA8\xE4\xBB\xB6\xE5\x90\x8D\x7D"),
        workPartName);
    name = ReplaceAllText(
        name,
        Utf8Text("\x7B\xE4\xBD\x93\xE5\x90\x8D\x7D"),
        bodyName);
    name = ReplaceAllText(
        name,
        Utf8Text("\x7B\xE4\xBD\x93\xE5\xB1\x9E\xE6\x80\xA7\x7D"),
        bodyAttributeName);

    name = SanitizeFileStem(name);
    return name.empty() ? BuildDefaultComponentName(groupNumber) : name;
}

std::string BuildComponentInstanceName(
    int groupNumber,
    int instanceNumber,
    const std::string& componentStem)
{
    const std::string safeStem = SanitizeFileStem(componentStem.empty()
        ? BuildDefaultComponentName(groupNumber)
        : componentStem);
    std::ostringstream name;
    name << safeStem
         << "_"
         << FormatPaddedNumber(instanceNumber, 3);
    return name.str();
}

void FreeLoadStatus(UF_PART_load_status_t& loadStatus)
{
    if (loadStatus.n_parts > 0 ||
        loadStatus.file_names != NULL ||
        loadStatus.statuses != NULL)
    {
        UF_PART_free_load_status(&loadStatus);
    }
}

std::string UfErrorMessage(int errorCode)
{
    char message[133] = {};
    if (UF_get_fail_message(errorCode, message) == 0 && message[0] != '\0')
    {
        return std::string(message);
    }

    std::ostringstream stream;
    stream << "UF error " << errorCode;
    return stream.str();
}

tag_t ResolveExportableBodyTag(tag_t selectedBodyTag)
{
    if (!UF_ASSEM_is_occurrence(selectedBodyTag))
    {
        return selectedBodyTag;
    }

    return UF_ASSEM_ask_prototype_of_occ(selectedBodyTag);
}

void ExportReferenceBodyToPart(
    const std::string& componentPartPath,
    tag_t selectedBodyTag)
{
    tag_t exportBodyTag = ResolveExportableBodyTag(selectedBodyTag);
    if (exportBodyTag == NULL_TAG)
    {
        throw NXOpen::NXException::Create(1);
    }

    tag_t objects[1] = {exportBodyTag};
    ThrowUfError(UF_PART_export(componentPartPath.c_str(), 1, objects));
}

NXOpen::Part::Units PartUnitsFromUf(int units)
{
    return units == UF_PART_ENGLISH
        ? NXOpen::Part::UnitsInches
        : NXOpen::Part::UnitsMillimeters;
}

NXOpen::Part* CreateTemplateComponentPart(
    NXOpen::Session* session,
    const std::string& componentPartPath,
    int units)
{
    if (session == NULL || session->Parts() == NULL)
    {
        throw NXOpen::NXException::Create("NX session parts collection is null");
    }

    NXOpen::Part* originalWorkPart = session->Parts()->Work();
    NXOpen::Part* originalDisplayPart = session->Parts()->Display();
    NXOpen::FileNew* fileNew = session->Parts()->FileNew();
    if (fileNew == NULL)
    {
        throw NXOpen::NXException::Create("cannot create FileNew builder");
    }

    NXOpen::Part* createdPart = NULL;
    try
    {
        fileNew->SetUseBlankTemplate(false);
        fileNew->SetApplicationName("ModelTemplate");
        fileNew->SetUnits(PartUnitsFromUf(units));
        fileNew->SetTemplateType(NXOpen::FileNewTemplateTypeItem);
        fileNew->SetTemplatePresentationName(
            NXOpen::NXString(Utf8Text("\xE6\xA8\xA1\xE5\x9E\x8B"), NXOpen::NXString::UTF8));
        fileNew->AllowTemplatePostPartCreationAction(false);
        fileNew->SetTemplateFileName(kComponentTemplateFileName);
        fileNew->SetNewFileName(componentPartPath.c_str());
        fileNew->SetMakeDisplayedPart(false);
        NXOpen::NXObject* committedObject = fileNew->Commit();
        createdPart = dynamic_cast<NXOpen::Part*>(committedObject);
        fileNew->Destroy();
    }
    catch (...)
    {
        try
        {
            fileNew->Destroy();
        }
        catch (...)
        {
        }
        throw;
    }

    if (createdPart == NULL)
    {
        NXOpen::PartLoadStatus* loadStatus = NULL;
        createdPart = session->Parts()->Open(componentPartPath.c_str(), &loadStatus);
        delete loadStatus;
    }

    if (originalDisplayPart != NULL && session->Parts()->Display() != originalDisplayPart)
    {
        NXOpen::PartLoadStatus* displayLoadStatus = NULL;
        session->Parts()->SetDisplay(originalDisplayPart, false, true, &displayLoadStatus);
        delete displayLoadStatus;
    }
    if (originalWorkPart != NULL && session->Parts()->Work() != originalWorkPart)
    {
        session->Parts()->SetWork(originalWorkPart);
    }

    return createdPart;
}

void ConfigureRecordedFileNew(
    NXOpen::FileNew* fileNew,
    const std::string& componentPartPath,
    int units)
{
    if (fileNew == NULL)
    {
        throw NXOpen::NXException::Create("FileNew builder is null");
    }

    fileNew->SetUseBlankTemplate(false);
    fileNew->SetApplicationName("ModelTemplate");
    fileNew->SetUnits(PartUnitsFromUf(units));
    fileNew->SetTemplateType(NXOpen::FileNewTemplateTypeItem);
    fileNew->SetTemplatePresentationName(
        NXOpen::NXString(Utf8Text("\xE6\xA8\xA1\xE5\x9E\x8B"), NXOpen::NXString::UTF8));
    fileNew->AllowTemplatePostPartCreationAction(false);
    fileNew->SetTemplateFileName(kComponentTemplateFileName);
    fileNew->SetNewFileName(componentPartPath.c_str());
    fileNew->SetMakeDisplayedPart(false);
}

void SaveComponentPart(NXOpen::Part* componentPart)
{
    if (componentPart == NULL)
    {
        return;
    }

    NXOpen::PartSaveStatus* saveStatus = componentPart->Save(
        NXOpen::BasePart::SaveComponentsFalse,
        NXOpen::BasePart::CloseAfterSaveFalse);
    if (saveStatus != NULL)
    {
        const int unsavedParts = saveStatus->NumberUnsavedParts();
        delete saveStatus;
        if (unsavedParts > 0)
        {
            throw NXOpen::NXException::Create("component part save failed");
        }
    }
}

void CreateReferenceComponentWithRecordedBuilder(
    NXOpen::Session* session,
    NXOpen::Part* workPart,
    const std::string& componentPartPath,
    const std::string& instanceName,
    int units,
    tag_t bodyTag,
    AssemblyConversionResult& result)
{
    if (session == NULL || session->Parts() == NULL || workPart == NULL)
    {
        throw NXOpen::NXException::Create("invalid session or work part");
    }

    NXOpen::TaggedObject* taggedObject = NXOpen::NXObjectManager::Get(bodyTag);
    NXOpen::Body* body = dynamic_cast<NXOpen::Body*>(taggedObject);
    if (body == NULL)
    {
        throw NXOpen::NXException::Create("reference body is not available for CreateNewComponentBuilder");
    }

    NXOpen::FileNew* fileNew = session->Parts()->FileNew();
    ConfigureRecordedFileNew(fileNew, componentPartPath, units);
    WritePreviewDebugLog("CreateNewComponentBuilder newFile=" + componentPartPath);

    NXOpen::Session::UndoMarkId undoMark = session->SetUndoMark(
        NXOpen::Session::MarkVisibilityVisible,
        NXOpen::NXString("Create Same Body Component"));
    NXOpen::Assemblies::CreateNewComponentBuilder* builder =
        workPart->AssemblyManager()->CreateNewComponentBuilder();
    if (builder == NULL)
    {
        throw NXOpen::NXException::Create("cannot create CreateNewComponentBuilder");
    }

    try
    {
        WritePreviewDebugLog("CreateNewComponentBuilder set name=" + instanceName);
        builder->SetNewComponentName(
            NXOpen::NXString(instanceName, NXOpen::NXString::UTF8));
        WritePreviewDebugLog("CreateNewComponentBuilder set refset");
        builder->SetReferenceSetName("MODEL");
        WritePreviewDebugLog("CreateNewComponentBuilder add body");
        builder->ObjectForNewComponent()->Add(body);
        WritePreviewDebugLog("CreateNewComponentBuilder set new file");
        builder->SetNewFile(fileNew);
        WritePreviewDebugLog("CreateNewComponentBuilder commit begin");
        builder->Commit();
        WritePreviewDebugLog("CreateNewComponentBuilder commit end");
        builder->Destroy();
        WritePreviewDebugLog("CreateNewComponentBuilder destroy end");
        session->SetUndoMarkName(undoMark, NXOpen::NXString("Create Same Body Component"));
        session->CleanUpFacetedFacesAndEdges();
        ++result.createdComponents;
    }
    catch (...)
    {
        WritePreviewDebugLog("CreateNewComponentBuilder exception cleanup begin");
        try
        {
            builder->Destroy();
        }
        catch (...)
        {
        }
        WritePreviewDebugLog("CreateNewComponentBuilder exception cleanup end");
        throw;
    }
}

void CreateReferenceComponentPart(
    tag_t parentPartTag,
    const std::string& componentPartPath,
    const std::string& instanceName,
    int units,
    tag_t bodyTag,
    tag_t* instanceTag)
{
    double origin[3] = {0.0, 0.0, 0.0};
    double identityCsys[6] = {1.0, 0.0, 0.0, 0.0, 1.0, 0.0};
    tag_t objects[1] = {bodyTag};

    ThrowUfError(UF_ASSEM_create_component_part(
        parentPartTag,
        componentPartPath.c_str(),
        "MODEL",
        instanceName.c_str(),
        units,
        -1,
        origin,
        identityCsys,
        1,
        objects,
        instanceTag));
}

void AddComponentInstance(
    tag_t parentPartTag,
    const std::string& componentPartPath,
    const std::string& instanceName,
    const RigidTransform3& transform,
    tag_t* instanceTag)
{
    double origin[3] = {
        transform.origin[0],
        transform.origin[1],
        transform.origin[2]};
    double csysMatrix[9] = {
        transform.csysMatrix[0],
        transform.csysMatrix[1],
        transform.csysMatrix[2],
        transform.csysMatrix[3],
        transform.csysMatrix[4],
        transform.csysMatrix[5],
        transform.csysMatrix[6],
        transform.csysMatrix[7],
        transform.csysMatrix[8]};

    UF_PART_load_status_t loadStatus = {};
    const int status = UF_ASSEM_add_part_to_assembly2(
        parentPartTag,
        componentPartPath.c_str(),
        "MODEL",
        instanceName.c_str(),
        origin,
        csysMatrix,
        -1,
        instanceTag,
        &loadStatus);
    FreeLoadStatus(loadStatus);
    ThrowUfError(status);
}

void DeleteOriginalBodyIfPossible(tag_t bodyTag, AssemblyConversionResult& result)
{
    if (!kDeleteOriginalMatchedBodiesAfterAssembly)
    {
        return;
    }

    if (bodyTag == NULL_TAG)
    {
        return;
    }

    tag_t deleteTag = bodyTag;
    if (UF_ASSEM_is_occurrence(bodyTag))
    {
        tag_t parentComponent = NULL_TAG;
        if (UF_ASSEM_ask_parent_component(bodyTag, &parentComponent) == 0 &&
            parentComponent != NULL_TAG)
        {
            deleteTag = parentComponent;
        }
    }

    if (UF_OBJ_delete_object(deleteTag) == 0)
    {
        ++result.deletedOriginalBodies;
    }
}

void DeleteOriginalGroupBodies(
    const MatchedBodyGroup& group,
    AssemblyConversionResult& result)
{
    if (!kDeleteOriginalMatchedBodiesAfterAssembly)
    {
        return;
    }

    for (std::size_t index = 0; index < group.instances.size(); ++index)
    {
        std::ostringstream line;
        line << "Delete original body begin index=" << (index + 1)
             << ", tag=" << group.instances[index].bodyTag;
        WritePreviewDebugLog(line.str());

        DeleteOriginalBodyIfPossible(group.instances[index].bodyTag, result);

        std::ostringstream endLine;
        endLine << "Delete original body end index=" << (index + 1);
        WritePreviewDebugLog(endLine.str());
    }
}

void AppendOriginalGroupBodyTags(
    const MatchedBodyGroup& group,
    std::vector<tag_t>& bodyTags)
{
    for (std::size_t index = 1; index < group.instances.size(); ++index)
    {
        if (group.instances[index].bodyTag != NULL_TAG)
        {
            bodyTags.push_back(group.instances[index].bodyTag);
        }
    }
}

void DeleteOriginalBodyTagsInBatch(
    const std::vector<tag_t>& bodyTags,
    AssemblyConversionResult& result)
{
    if (!kDeleteOriginalMatchedBodiesAfterAssembly || bodyTags.empty())
    {
        return;
    }

    std::vector<tag_t> deleteTags;
    deleteTags.reserve(bodyTags.size());
    std::set<tag_t> seenTags;
    for (std::size_t index = 0; index < bodyTags.size(); ++index)
    {
        tag_t deleteTag = bodyTags[index];
        if (deleteTag == NULL_TAG)
        {
            continue;
        }

        if (UF_ASSEM_is_occurrence(deleteTag))
        {
            tag_t parentComponent = NULL_TAG;
            if (UF_ASSEM_ask_parent_component(deleteTag, &parentComponent) == 0 &&
                parentComponent != NULL_TAG)
            {
                deleteTag = parentComponent;
            }
        }

        if (seenTags.find(deleteTag) != seenTags.end())
        {
            continue;
        }

        const int status = UF_OBJ_ask_status(deleteTag);
        if (status != UF_OBJ_ALIVE && status != UF_OBJ_TEMPORARY)
        {
            std::ostringstream line;
            line << "Skip delete body tag=" << deleteTag
                 << ", status=" << status;
            WritePreviewDebugLog(line.str());
            continue;
        }

        seenTags.insert(deleteTag);
        deleteTags.push_back(deleteTag);
    }

    if (deleteTags.empty())
    {
        return;
    }

    std::ostringstream beginLine;
    beginLine << "Delete original body batch begin count=" << deleteTags.size();
    WritePreviewDebugLog(beginLine.str());

    int* statuses = NULL;
    const int deleteResult = UF_OBJ_delete_array_of_objects(
        static_cast<int>(deleteTags.size()),
        &deleteTags[0],
        &statuses);
    {
        std::ostringstream line;
        line << "Delete original body batch result=" << deleteResult;
        WritePreviewDebugLog(line.str());
    }

    for (std::size_t index = 0; index < deleteTags.size(); ++index)
    {
        const int deleteStatus = statuses != NULL ? statuses[index] : deleteResult;
        const int objectStatus = UF_OBJ_ask_status(deleteTags[index]);
        std::ostringstream line;
        line << "Delete original body batch item index=" << (index + 1)
             << ", tag=" << deleteTags[index]
             << ", deleteStatus=" << deleteStatus
             << ", objectStatus=" << objectStatus;
        WritePreviewDebugLog(line.str());
        if (objectStatus == UF_OBJ_DELETED || objectStatus == UF_OBJ_CONDEMNED)
        {
            ++result.deletedOriginalBodies;
        }
    }

    if (statuses != NULL)
    {
        UF_free(statuses);
    }
    WritePreviewDebugLog("Delete original body batch end");
}

bool BodyHasFeatureParameters(tag_t bodyTag)
{
    tag_t parameterBodyTag = ResolveExportableBodyTag(bodyTag);
    if (parameterBodyTag == NULL_TAG)
    {
        return false;
    }

    uf_list_p_t featureList = NULL;
    const int askResult = UF_MODL_ask_body_feats(parameterBodyTag, &featureList);
    if (askResult != 0)
    {
        return false;
    }

    int featureCount = 0;
    if (featureList != NULL)
    {
        UF_MODL_ask_list_count(featureList, &featureCount);
        UF_MODL_delete_list(&featureList);
    }

    return featureCount > 0;
}

std::vector<tag_t> FindBodiesWithFeatureParameters(
    const std::vector<NXOpen::Body*>& selectedBodies)
{
    std::vector<tag_t> parameterBodyTags;
    std::set<tag_t> seenTags;
    for (std::size_t index = 0; index < selectedBodies.size(); ++index)
    {
        if (selectedBodies[index] == NULL)
        {
            continue;
        }

        const tag_t bodyTag = ResolveExportableBodyTag(selectedBodies[index]->Tag());
        if (bodyTag == NULL_TAG || !seenTags.insert(bodyTag).second)
        {
            continue;
        }

        if (BodyHasFeatureParameters(bodyTag))
        {
            parameterBodyTags.push_back(bodyTag);
        }
    }

    return parameterBodyTags;
}

void DeleteBodyParameters(const std::vector<tag_t>& bodyTags)
{
    if (bodyTags.empty())
    {
        return;
    }

    uf_list_p_t bodyList = NULL;
    ThrowUfError(UF_MODL_create_list(&bodyList));
    try
    {
        for (std::size_t index = 0; index < bodyTags.size(); ++index)
        {
            ThrowUfError(UF_MODL_put_list_item(bodyList, bodyTags[index]));
        }

        ThrowUfError(UF_MODL_delete_body_parms(bodyList));
    }
    catch (...)
    {
        if (bodyList != NULL)
        {
            UF_MODL_delete_list(&bodyList);
        }
        throw;
    }

    if (bodyList != NULL)
    {
        UF_MODL_delete_list(&bodyList);
    }
}

void AddExportedReferenceBodyInstances(
    NXOpen::Session* session,
    NXOpen::Part* workPart,
    tag_t parentPartTag,
    const std::string& componentPartPath,
    const std::string& componentName,
    int groupNumber,
    int parentUnits,
    const MatchedBodyGroup& group,
    AssemblyConversionResult& result)
{
    WritePreviewDebugLog("Ask reference transform begin");
    const RigidTransform3 referenceBodyTransform =
        AskSelectedBodyToAssemblyTransform(group.instances[0].bodyTag);
    WritePreviewDebugLog("Ask reference transform end");

    CreateReferenceComponentWithRecordedBuilder(
        session,
        workPart,
        componentPartPath,
        BuildComponentInstanceName(groupNumber, 1, componentName),
        parentUnits,
        group.instances[0].bodyTag,
        result);

    for (std::size_t instanceIndex = 1; instanceIndex < group.instances.size(); ++instanceIndex)
    {
        RigidTransform3 componentTransform = referenceBodyTransform;
        componentTransform = ComposeTransforms(
            group.instances[instanceIndex].transformFromReference,
            referenceBodyTransform);

        {
            std::ostringstream line;
            line << "Add component instance begin group=" << groupNumber
                 << ", index=" << (instanceIndex + 1);
            WritePreviewDebugLog(line.str());
        }
        tag_t instanceTag = NULL_TAG;
        AddComponentInstance(
            parentPartTag,
            componentPartPath,
            BuildComponentInstanceName(
                groupNumber,
                static_cast<int>(instanceIndex + 1),
                componentName),
            componentTransform,
            &instanceTag);
        ++result.createdComponents;
        {
            std::ostringstream line;
            line << "Add component instance end group=" << groupNumber
                 << ", index=" << (instanceIndex + 1)
                 << ", tag=" << instanceTag;
            WritePreviewDebugLog(line.str());
        }
    }

}

void AddMovedReferenceBodyInstances(
    tag_t parentPartTag,
    const std::string& componentPartPath,
    const std::string& componentName,
    int groupNumber,
    int parentUnits,
    const MatchedBodyGroup& group,
    AssemblyConversionResult& result)
{
    tag_t referenceInstanceTag = NULL_TAG;
    CreateReferenceComponentPart(
        parentPartTag,
        componentPartPath,
        BuildComponentInstanceName(groupNumber, 1, componentName),
        parentUnits,
        group.instances[0].bodyTag,
        &referenceInstanceTag);
    ++result.createdComponents;

    for (std::size_t instanceIndex = 1; instanceIndex < group.instances.size(); ++instanceIndex)
    {
        tag_t instanceTag = NULL_TAG;
        AddComponentInstance(
            parentPartTag,
            componentPartPath,
            BuildComponentInstanceName(
                groupNumber,
                static_cast<int>(instanceIndex + 1),
                componentName),
            group.instances[instanceIndex].transformFromReference,
            &instanceTag);
        ++result.createdComponents;
    }

}

AssemblyConversionResult ConvertMatchedGroupsToAssembly(
    NXOpen::Session* session,
    const std::vector<MatchedBodyGroup>& matchedGroups,
    const std::vector<std::string>& componentNames)
{
    AssemblyConversionResult result = {};
    if (session == NULL || matchedGroups.empty())
    {
        return result;
    }

    NXOpen::Part* workPart = session->Parts()->Work();
    if (workPart == NULL)
    {
        result.failedGroups = static_cast<int>(matchedGroups.size());
        result.failureReasons.push_back("work part is null");
        return result;
    }

    const tag_t parentPartTag = workPart->Tag();
    const int parentUnits = AskPartUnits(parentPartTag);
    const std::string outputDirectory = BuildAssemblyOutputDirectory(workPart);
    if (!EnsureDirectoryExists(outputDirectory))
    {
        result.failedGroups = static_cast<int>(matchedGroups.size());
        std::ostringstream reason;
        reason << "cannot create output directory: " << outputDirectory
               << ", Win32 error=" << GetLastError();
        result.failureReasons.push_back(reason.str());
        return result;
    }

    std::vector<tag_t> originalBodyTagsToDelete;
    for (std::size_t groupIndex = 0; groupIndex < matchedGroups.size(); ++groupIndex)
    {
        const MatchedBodyGroup& group = matchedGroups[groupIndex];
        if (group.instances.empty())
        {
            ++result.failedGroups;
            std::ostringstream reason;
            reason << "group " << (groupIndex + 1) << " skipped: empty group";
            result.failureReasons.push_back(reason.str());
            continue;
        }

        try
        {
            const int groupNumber = static_cast<int>(groupIndex + 1);
            std::string componentName =
                BuildComponentNameForRule(session, group, groupNumber);
            if (groupIndex < componentNames.size() && !componentNames[groupIndex].empty())
            {
                componentName = componentNames[groupIndex];
            }
            const std::string componentPartPath =
                BuildUniqueComponentPartPath(outputDirectory, componentName);
            WritePreviewDebugLog("Assembly group path=" + componentPartPath);

            AddExportedReferenceBodyInstances(
                session,
                workPart,
                parentPartTag,
                componentPartPath,
                componentName,
                groupNumber,
                parentUnits,
                group,
                result);

            AppendOriginalGroupBodyTags(group, originalBodyTagsToDelete);
            ++result.convertedGroups;
            result.partFiles.push_back(componentPartPath);
        }
        catch (const NXOpen::NXException& ex)
        {
            ++result.failedGroups;
            std::ostringstream reason;
            reason << "group " << (groupIndex + 1)
                   << " failed: NX error " << ex.ErrorCode()
                   << ", " << UfErrorMessage(ex.ErrorCode());
            result.failureReasons.push_back(reason.str());
        }
    }

    DeleteOriginalBodyTagsInBatch(originalBodyTagsToDelete, result);
    UF_DISP_refresh();
    return result;
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

    std::vector<NXOpen::Edge*> edges = body->GetEdges();
    fingerprint.edgeCount = static_cast<int>(edges.size());

    std::vector<double> edgeLengths;
    edgeLengths.reserve(edges.size());
    fingerprint.vertexPoints.reserve(edges.size() * 2);
    fingerprint.circleCenterPoints.reserve(edges.size());
    fingerprint.lineEdgePoints.reserve(edges.size() * 2);
    fingerprint.curveEdgePoints.reserve(edges.size() * 2);
    fingerprint.arcEdgePoints.reserve(edges.size() * 2);
    fingerprint.fullCircleEdgePoints.reserve(edges.size());
    fingerprint.lineEdgeLengths.reserve(edges.size());
    fingerprint.curveEdgeLengths.reserve(edges.size());
    fingerprint.arcEdgeLengths.reserve(edges.size());
    fingerprint.fullCircleEdgeLengths.reserve(edges.size());
    for (std::size_t index = 0; index < edges.size(); ++index)
    {
        NXOpen::Edge* edge = edges[index];
        const double edgeLength = AskEdgeLength(edge->Tag());
        edgeLengths.push_back(edgeLength);
        AppendEdgeGeometryPoints(
            edge,
            edgeLength,
            fingerprint.vertexPoints,
            fingerprint.circleCenterPoints,
            fingerprint.lineEdgePoints,
            fingerprint.curveEdgePoints,
            fingerprint.arcEdgePoints,
            fingerprint.fullCircleEdgePoints,
            fingerprint.lineEdgeLengths,
            fingerprint.curveEdgeLengths,
            fingerprint.arcEdgeLengths,
            fingerprint.fullCircleEdgeLengths);
    }

    fingerprint.lengthBuckets = BuildLengthBuckets(edgeLengths);
    std::sort(fingerprint.fullCircleEdgeLengths.begin(), fingerprint.fullCircleEdgeLengths.end(), std::greater<double>());
    std::sort(fingerprint.arcEdgeLengths.begin(), fingerprint.arcEdgeLengths.end(), std::greater<double>());
    std::sort(fingerprint.curveEdgeLengths.begin(), fingerprint.curveEdgeLengths.end(), std::greater<double>());
    std::sort(fingerprint.lineEdgeLengths.begin(), fingerprint.lineEdgeLengths.end(), std::greater<double>());

    std::vector<NXOpen::Face*> faces = body->GetFaces();
    fingerprint.faceCount = static_cast<int>(faces.size());
    fingerprint.planeFeaturesBuilt = false;

    return fingerprint;
}

void EnsurePlaneFaceFeatures(BodyFingerprint& fingerprint)
{
    if (fingerprint.planeFeaturesBuilt || fingerprint.body == NULL)
    {
        return;
    }

    std::vector<NXOpen::Face*> faces = fingerprint.body->GetFaces();
    fingerprint.faceCount = static_cast<int>(faces.size());
    fingerprint.planeFaces.clear();
    fingerprint.planeFaceGroups.clear();
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
    fingerprint.planeFeaturesBuilt = true;
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

bool CompareLengthSequenceDescending(
    const std::vector<double>& referenceLengths,
    const std::vector<double>& candidateLengths,
    const char* lengthName,
    std::string& rejectReason)
{
    if (referenceLengths.size() != candidateLengths.size())
    {
        std::ostringstream stream;
        stream << lengthName << " count mismatch: ref=" << referenceLengths.size()
               << ", cand=" << candidateLengths.size();
        rejectReason = stream.str();
        return false;
    }

    for (std::size_t index = 0; index < referenceLengths.size(); ++index)
    {
        if (!NearlyEqual(referenceLengths[index], candidateLengths[index], kLengthTolerance))
        {
            std::ostringstream stream;
            stream << lengthName << " sorted length " << (index + 1)
                   << " mismatch: ref=" << FormatDouble(referenceLengths[index])
                   << ", cand=" << FormatDouble(candidateLengths[index]);
            rejectReason = stream.str();
            return false;
        }
    }

    return true;
}

bool CoarseBodyCountsMatch(
    const BodyFingerprint& reference,
    const BodyFingerprint& candidate,
    std::string& rejectReason)
{
    if (reference.edgeCount != candidate.edgeCount)
    {
        std::ostringstream stream;
        stream << "coarse edge count mismatch: ref=" << reference.edgeCount
               << ", cand=" << candidate.edgeCount;
        rejectReason = stream.str();
        return false;
    }

    if (reference.faceCount != candidate.faceCount)
    {
        std::ostringstream stream;
        stream << "coarse face count mismatch: ref=" << reference.faceCount
               << ", cand=" << candidate.faceCount;
        rejectReason = stream.str();
        return false;
    }

    rejectReason.clear();
    return true;
}

bool MiddleTypedEdgeLengthsMatch(
    const BodyFingerprint& reference,
    const BodyFingerprint& candidate,
    std::string& rejectReason)
{
    if (!CompareLengthSequenceDescending(
            reference.fullCircleEdgeLengths,
            candidate.fullCircleEdgeLengths,
            "middle full circle edge",
            rejectReason))
    {
        return false;
    }

    if (!CompareLengthSequenceDescending(
            reference.arcEdgeLengths,
            candidate.arcEdgeLengths,
            "middle arc edge",
            rejectReason))
    {
        return false;
    }

    if (!CompareLengthSequenceDescending(
            reference.curveEdgeLengths,
            candidate.curveEdgeLengths,
            "middle curve edge",
            rejectReason))
    {
        return false;
    }

    if (!CompareLengthSequenceDescending(
            reference.lineEdgeLengths,
            candidate.lineEdgeLengths,
            "middle line edge",
            rejectReason))
    {
        return false;
    }

    rejectReason.clear();
    return true;
}

struct LocalCoordinateSignature
{
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

    (*debugLog) << label << std::endl;
    WritePointVectorDebugLog(debugLog, "  full circle center local points", signature.fullCircleEdgeLocalPoints);
    WritePointVectorDebugLog(debugLog, "  arc edge local points", signature.arcEdgeLocalPoints);
    WritePointVectorDebugLog(debugLog, "  curve edge local points", signature.curveEdgeLocalPoints);
    WritePointVectorDebugLog(debugLog, "  line edge local points", signature.lineEdgeLocalPoints);
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
    BodyMatchInfo* matchInfo,
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
                if (matchInfo != NULL)
                {
                    matchInfo->referenceFrame = referenceFace.frame;
                    matchInfo->candidateFrame = candidateVariantFrame;
                    matchInfo->variantIndex = variantIndex;
                    matchInfo->referenceFaceTag = referenceFace.tag;
                    matchInfo->candidateFaceTag = candidateFace.tag;
                }
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
    BodyMatchInfo* matchInfo,
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
                matchInfo,
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

bool CompareFingerprints(
    BodyFingerprint& reference,
    BodyFingerprint& candidate,
    std::ofstream* debugLog,
    int* coordinateDebugStep,
    std::vector<tag_t>* activeDebugObjectTags,
    bool* coordinateDebugCanceled,
    BodyMatchInfo* matchInfo,
    std::string& rejectReason)
{
    if (!CoarseBodyCountsMatch(reference, candidate, rejectReason))
    {
        return false;
    }

    if (!MiddleTypedEdgeLengthsMatch(reference, candidate, rejectReason))
    {
        return false;
    }

    EnsurePlaneFaceFeatures(reference);
    EnsurePlaneFaceFeatures(candidate);

    if (!BodiesMatchByAnchorPlaneCoordinates(
            reference,
            candidate,
            debugLog,
            coordinateDebugStep,
            activeDebugObjectTags,
            coordinateDebugCanceled,
            matchInfo,
            rejectReason))
    {
        return false;
    }

    rejectReason.clear();
    return true;
}

std::vector<tag_t> ExtractGroupTags(const MatchedBodyGroup& group)
{
    std::vector<tag_t> tags;
    tags.reserve(group.instances.size());
    for (std::size_t index = 0; index < group.instances.size(); ++index)
    {
        tags.push_back(group.instances[index].bodyTag);
    }
    return tags;
}

std::string BuildGroupKey(const std::vector<tag_t>& tags)
{
    std::vector<tag_t> sortedTags = tags;
    std::sort(sortedTags.begin(), sortedTags.end());

    std::ostringstream stream;
    for (std::size_t index = 0; index < sortedTags.size(); ++index)
    {
        if (index != 0)
        {
            stream << ";";
        }
        stream << sortedTags[index];
    }
    return stream.str();
}

SameBodySearchData BuildSameBodySearchData(
    const std::vector<NXOpen::Body*>& selectedBodies,
    std::ofstream* debugLog)
{
    SameBodySearchData data = {};
    data.coordinateDebugCanceled = false;

    data.fingerprints.reserve(selectedBodies.size());
    for (std::size_t index = 0; index < selectedBodies.size(); ++index)
    {
        try
        {
            BodyFingerprint fingerprint = BuildFingerprint(selectedBodies[index]);
            if (debugLog != NULL)
            {
                WriteFingerprintDebugLog(*debugLog, index, fingerprint);
            }
            data.fingerprints.push_back(fingerprint);
        }
        catch (const NXOpen::NXException& ex)
        {
            if (debugLog != NULL && debugLog->is_open())
            {
                (*debugLog) << "Build fingerprint failed: bodyIndex=" << (index + 1)
                            << ", tag=" << selectedBodies[index]->Tag()
                            << ", errorCode=" << ex.ErrorCode()
                            << std::endl;
            }
            data.failedTags.push_back(selectedBodies[index]->Tag());
        }
    }

    std::vector<bool> grouped(data.fingerprints.size(), false);
    int coordinateDebugStep = 0;
    for (std::size_t referenceIndex = 0; referenceIndex < data.fingerprints.size(); ++referenceIndex)
    {
        if (data.coordinateDebugCanceled)
        {
            break;
        }

        if (grouped[referenceIndex])
        {
            continue;
        }

        grouped[referenceIndex] = true;
        std::vector<tag_t> groupTags;
        groupTags.push_back(data.fingerprints[referenceIndex].tag);
        MatchedBodyGroup assemblyGroup = {};
        MatchedBodyInstance referenceInstance = {};
        referenceInstance.bodyTag = data.fingerprints[referenceIndex].tag;
        referenceInstance.transformFromReference = IdentityTransform();
        assemblyGroup.instances.push_back(referenceInstance);

        for (std::size_t candidateIndex = referenceIndex + 1; candidateIndex < data.fingerprints.size(); ++candidateIndex)
        {
            if (grouped[candidateIndex])
            {
                continue;
            }

            std::string rejectReason;
            BodyMatchInfo matchInfo = {};
            bool matched = false;
            try
            {
                matched = CompareFingerprints(
                    data.fingerprints[referenceIndex],
                    data.fingerprints[candidateIndex],
                    debugLog,
                    &coordinateDebugStep,
                    &data.activeDebugObjectTags,
                    &data.coordinateDebugCanceled,
                    &matchInfo,
                    rejectReason);
            }
            catch (const NXOpen::NXException& ex)
            {
                std::ostringstream stream;
                stream << "NX error " << ex.ErrorCode()
                       << ", " << UfErrorMessage(ex.ErrorCode());
                rejectReason = stream.str();
                matched = false;
            }
            catch (const std::exception& ex)
            {
                rejectReason = ex.what();
                matched = false;
            }

            if (!matched)
            {
                if (debugLog != NULL && debugLog->is_open())
                {
                    (*debugLog) << "Compare failed: refIndex=" << (referenceIndex + 1)
                                << ", refTag=" << data.fingerprints[referenceIndex].tag
                                << ", candIndex=" << (candidateIndex + 1)
                                << ", candTag=" << data.fingerprints[candidateIndex].tag
                                << ", reason=" << rejectReason
                                << std::endl;
                }
                if (data.coordinateDebugCanceled)
                {
                    break;
                }
                continue;
            }

            if (debugLog != NULL && debugLog->is_open())
            {
                (*debugLog) << "Compare matched: refIndex=" << (referenceIndex + 1)
                            << ", refTag=" << data.fingerprints[referenceIndex].tag
                            << ", candIndex=" << (candidateIndex + 1)
                            << ", candTag=" << data.fingerprints[candidateIndex].tag
                            << std::endl;
            }
            grouped[candidateIndex] = true;
            groupTags.push_back(data.fingerprints[candidateIndex].tag);

            MatchedBodyInstance candidateInstance = {};
            candidateInstance.bodyTag = data.fingerprints[candidateIndex].tag;
            candidateInstance.transformFromReference =
                BuildReferenceToCandidateTransform(
                    matchInfo.referenceFrame,
                    matchInfo.candidateFrame);
            assemblyGroup.instances.push_back(candidateInstance);
        }

        if (data.coordinateDebugCanceled)
        {
            break;
        }

        if (groupTags.size() > 1)
        {
            data.matchedGroups.push_back(groupTags);
            if (debugLog != NULL && debugLog->is_open())
            {
                (*debugLog) << "Matched group " << data.matchedGroups.size()
                            << ": count=" << groupTags.size()
                            << ", tags=";
                for (std::size_t tagIndex = 0; tagIndex < groupTags.size(); ++tagIndex)
                {
                    if (tagIndex != 0)
                    {
                        (*debugLog) << ", ";
                    }
                    (*debugLog) << groupTags[tagIndex];
                }
                (*debugLog) << std::endl;
            }
        }

        data.assemblyGroups.push_back(assemblyGroup);
    }

    return data;
}

class DuoSiTiZuanZuanPeiDialog
{
public:
    DuoSiTiZuanZuanPeiDialog()
        : session(NXOpen::Session::GetSession()),
          ui(NXOpen::UI::GetUI()),
          dialog(NULL),
          bodySelection(NULL),
          outputFolder(NULL),
          colorMatchedBodiesToggle(NULL),
          findSameBodiesButton(NULL),
          nameConfigButton(NULL),
          assemblyList(NULL),
          assemblyListReady(false),
          suppressBodySelectionUpdate(false),
          previewRowHighlightActive(false),
          cachedSearchDataValid(false)
    {
        ResetPreviewDebugLog();
        WritePreviewDebugLog("constructor begin, dialogPath=" + GetDialogFullPath());
        dialog = ui->CreateDialog(GetDialogFullPath().c_str());
        dialog->AddInitializeHandler(NXOpen::make_callback(this, &DuoSiTiZuanZuanPeiDialog::Initialize));
        dialog->AddDialogShownHandler(NXOpen::make_callback(this, &DuoSiTiZuanZuanPeiDialog::DialogShown));
        dialog->AddUpdateHandler(NXOpen::make_callback(this, &DuoSiTiZuanZuanPeiDialog::Update));
        dialog->AddApplyHandler(NXOpen::make_callback(this, &DuoSiTiZuanZuanPeiDialog::Apply));
        dialog->AddOkHandler(NXOpen::make_callback(this, &DuoSiTiZuanZuanPeiDialog::Ok));
        WritePreviewDebugLog("constructor end");
    }

    ~DuoSiTiZuanZuanPeiDialog()
    {
        if (dialog != NULL)
        {
            delete dialog;
            dialog = NULL;
        }
    }

    NXOpen::BlockStyler::BlockDialog::DialogResponse Show()
    {
        return dialog->Launch();
    }

private:
    NXOpen::Session* session;
    NXOpen::UI* ui;
    NXOpen::BlockStyler::BlockDialog* dialog;
    NXOpen::BlockStyler::SelectObject* bodySelection;
    NXOpen::BlockStyler::UIBlock* outputFolder;
    NXOpen::BlockStyler::UIBlock* colorMatchedBodiesToggle;
    NXOpen::BlockStyler::Button* findSameBodiesButton;
    NXOpen::BlockStyler::Button* nameConfigButton;
    NXOpen::BlockStyler::Tree* assemblyList;
    bool assemblyListReady;
    std::vector<AssemblyPreviewRow> previewRows;
    std::vector<NXOpen::BlockStyler::Node*> previewNodes;
    std::vector<tag_t> previewSelectionTags;
    std::vector<NXOpen::TaggedObject*> searchSelectionObjects;
    bool suppressBodySelectionUpdate;
    bool previewRowHighlightActive;
    SameBodySearchData cachedSearchData;
    std::vector<tag_t> cachedSearchSelectionTags;
    bool cachedSearchDataValid;

    void Initialize()
    {
        WritePreviewDebugLog("Initialize begin");
        EnsureNameConfigFileExists();
        RefreshBlockPointers();
        ConfigureBodySelectionFilter();
        InitializeOutputFolder();
        UpdateColorMatchedBodiesOption();
        WritePreviewDebugLog("Initialize end");
    }

    void DialogShown()
    {
        WritePreviewDebugLog("DialogShown begin");
        try
        {
            RefreshBlockPointers();
            InitializeAssemblyList();
            WritePreviewDebugLog("DialogShown end, assemblyReady=" + std::string(assemblyListReady ? "true" : "false"));
        }
        catch (const NXOpen::NXException& ex)
        {
            WritePreviewDebugLog(std::string("DialogShown NXException: ") + ex.Message());
            assemblyListReady = false;
        }
        catch (const std::exception& ex)
        {
            WritePreviewDebugLog(std::string("DialogShown exception: ") + ex.what());
            assemblyListReady = false;
        }
        catch (...)
        {
            WritePreviewDebugLog("DialogShown unknown exception");
            assemblyListReady = false;
        }
    }

    int Update(NXOpen::BlockStyler::UIBlock* block)
    {
        const std::string blockName = SafeBlockName(block);
        WritePreviewDebugLog(
            "Update begin, block=" + blockName +
            ", pointer=" + PointerText(block));
        try
        {
            if (block == findSameBodiesButton || blockName == kFindSameBodiesButtonBlockId)
            {
                findSameBodiesButton = dynamic_cast<NXOpen::BlockStyler::Button*>(block);
                WritePreviewDebugLog("Update routed to BuildAssemblyPreviewFromSelection");
                BuildAssemblyPreviewFromSelection();
            }
            else if (block == nameConfigButton || blockName == kNameConfigButtonBlockId)
            {
                nameConfigButton = dynamic_cast<NXOpen::BlockStyler::Button*>(block);
                WritePreviewDebugLog("Update routed to EditNameConfig");
                EditNameConfig();
            }
            else if (block == outputFolder || blockName == kOutputFolderBlockId)
            {
                outputFolder = block;
                UpdateSelectedOutputFolder();
                WritePreviewDebugLog("Update routed to outputFolder");
            }
            else if (block == colorMatchedBodiesToggle || blockName == kColorMatchedBodiesToggleBlockId)
            {
                colorMatchedBodiesToggle = block;
                UpdateColorMatchedBodiesOption();
                WritePreviewDebugLog("Update routed to colorMatchedBodiesToggle");
            }
            else if (block == bodySelection || blockName == kBodySelectionBlockId)
            {
                bodySelection = dynamic_cast<NXOpen::BlockStyler::SelectObject*>(block);
                if (suppressBodySelectionUpdate)
                {
                    WritePreviewDebugLog("Update bodySelection skipped: suppressBodySelectionUpdate");
                    return 0;
                }
                previewRowHighlightActive = false;
                InitializeAssemblyList();
                std::vector<NXOpen::Body*> selectedBodies = GetSelectedBodies();
                StoreSearchSelectionFromBodySelection();
                std::ostringstream line;
                line << "Update bodySelection selectedBodies=" << selectedBodies.size();
                WritePreviewDebugLog(line.str());
                if (selectedBodies.empty())
                {
                    ClearAssemblyList();
                    previewSelectionTags.clear();
                    ClearCachedSearchData();
                    WritePreviewDebugLog("Update routed to bodySelection clear");
                }
                else
                {
                    RefreshAssemblyPreview();
                    WritePreviewDebugLog("Update routed to RefreshAssemblyPreview");
                }
            }
        }
        catch (const NXOpen::NXException& ex)
        {
            WritePreviewDebugLog(std::string("Update NXException: ") + ex.Message());
            try
            {
                ClearAssemblyList();
            }
            catch (...)
            {
                previewNodes.clear();
                previewRows.clear();
            }
        }
        catch (const std::exception& ex)
        {
            WritePreviewDebugLog(std::string("Update exception: ") + ex.what());
            try
            {
                ClearAssemblyList();
            }
            catch (...)
            {
                previewNodes.clear();
                previewRows.clear();
            }
        }
        catch (...)
        {
            WritePreviewDebugLog("Update unknown exception");
            try
            {
                ClearAssemblyList();
            }
            catch (...)
            {
                previewNodes.clear();
                previewRows.clear();
            }
        }
        WritePreviewDebugLog("Update end");
        return 0;
    }

    void RefreshBlockPointers()
    {
        if (dialog == NULL)
        {
            WritePreviewDebugLog("RefreshBlockPointers skipped: dialog is null");
            return;
        }

        NXOpen::BlockStyler::CompositeBlock* topBlock = dialog->TopBlock();
        if (bodySelection == NULL)
        {
            bodySelection = dynamic_cast<NXOpen::BlockStyler::SelectObject*>(
                FindBlockRecursive(topBlock, kBodySelectionBlockId));
        }
        if (outputFolder == NULL)
        {
            outputFolder = FindBlockRecursive(topBlock, kOutputFolderBlockId);
        }
        if (colorMatchedBodiesToggle == NULL)
        {
            colorMatchedBodiesToggle = FindBlockRecursive(
                topBlock,
                kColorMatchedBodiesToggleBlockId);
        }
        if (findSameBodiesButton == NULL)
        {
            findSameBodiesButton = dynamic_cast<NXOpen::BlockStyler::Button*>(
                FindBlockRecursive(topBlock, kFindSameBodiesButtonBlockId));
        }
        if (nameConfigButton == NULL)
        {
            nameConfigButton = dynamic_cast<NXOpen::BlockStyler::Button*>(
                FindBlockRecursive(topBlock, kNameConfigButtonBlockId));
        }
        if (assemblyList == NULL)
        {
            assemblyList = dynamic_cast<NXOpen::BlockStyler::Tree*>(
                FindBlockRecursive(topBlock, kAssemblyListBlockId));
        }

        std::ostringstream line;
        line << "RefreshBlockPointers body=" << PointerText(bodySelection)
             << ", outputFolder=" << PointerText(outputFolder)
             << ", colorToggle=" << PointerText(colorMatchedBodiesToggle)
             << ", button=" << PointerText(findSameBodiesButton)
             << ", configButton=" << PointerText(nameConfigButton)
             << ", tree=" << PointerText(assemblyList);
        WritePreviewDebugLog(line.str());
    }

    std::string GetWorkPartDirectory() const
    {
        NXOpen::Part* workPart = session != NULL && session->Parts() != NULL
            ? session->Parts()->Work()
            : NULL;
        if (workPart == NULL)
        {
            return std::string();
        }

        return DirectoryName(NxStringToLocale(workPart->FullPath()));
    }

    void InitializeOutputFolder()
    {
        if (outputFolder == NULL)
        {
            return;
        }

        const std::string workDirectory = GetWorkPartDirectory();
        if (!workDirectory.empty())
        {
            NXOpen::BlockStyler::PropertyList* properties = outputFolder->GetProperties();
            properties->SetString("Path", workDirectory.c_str());
            delete properties;
            gSelectedOutputDirectory = workDirectory;
        }
    }

    void UpdateSelectedOutputFolder()
    {
        if (outputFolder == NULL)
        {
            return;
        }

        NXOpen::BlockStyler::PropertyList* properties = outputFolder->GetProperties();
        gSelectedOutputDirectory = NormalizeDirectoryPath(
            NxStringToLocale(properties->GetString("Path")));
        delete properties;
    }

    void UpdateColorMatchedBodiesOption()
    {
        if (colorMatchedBodiesToggle == NULL)
        {
            return;
        }

        NXOpen::BlockStyler::PropertyList* properties =
            colorMatchedBodiesToggle->GetProperties();
        gColorMatchedBodies = properties->GetLogical("Value");
        delete properties;
    }

    void InitializeAssemblyList()
    {
        WritePreviewDebugLog("InitializeAssemblyList begin");
        RefreshBlockPointers();

        if (assemblyList == NULL || assemblyListReady)
        {
            WritePreviewDebugLog(
                "InitializeAssemblyList skipped, tree=" + PointerText(assemblyList) +
                ", ready=" + std::string(assemblyListReady ? "true" : "false"));
            return;
        }

        assemblyList->InsertColumn(
            kAssemblyNameColumnId,
            NXOpen::NXString(Utf8Text("\xE5\x90\x8D\xE7\xA7\xB0"), NXOpen::NXString::UTF8),
            210);
        assemblyList->InsertColumn(
            kAssemblyParentColumnId,
            NXOpen::NXString(Utf8Text("\xE7\x88\xB6\xE7\xBB\x84\xE4\xBB\xB6\xE5\x90\x8D\xE7\xA7\xB0"), NXOpen::NXString::UTF8),
            150);
        assemblyList->InsertColumn(
            kAssemblyQuantityColumnId,
            NXOpen::NXString(Utf8Text("\xE6\x95\xB0\xE9\x87\x8F"), NXOpen::NXString::UTF8),
            60);
        assemblyList->SetColumnSortable(kAssemblyNameColumnId, false);
        assemblyList->SetColumnSortable(kAssemblyParentColumnId, false);
        assemblyList->SetColumnSortable(kAssemblyQuantityColumnId, false);
        assemblyList->SetColumnResizePolicy(
            kAssemblyNameColumnId,
            NXOpen::BlockStyler::Tree::ColumnResizePolicyResizeWithTree);
        assemblyList->SetSortRootNodes(false);
        assemblyList->SetOnBeginLabelEditHandler(
            NXOpen::make_callback(this, &DuoSiTiZuanZuanPeiDialog::OnBeginAssemblyNameEdit));
        assemblyList->SetOnEndLabelEditHandler(
            NXOpen::make_callback(this, &DuoSiTiZuanZuanPeiDialog::OnEndAssemblyNameEdit));
        assemblyList->SetOnSelectHandler(
            NXOpen::make_callback(this, &DuoSiTiZuanZuanPeiDialog::OnAssemblyRowSelect));
        assemblyListReady = true;
        WritePreviewDebugLog("InitializeAssemblyList end");
    }

    void RefreshPreviewNamesFromConfig()
    {
        if (previewRows.empty())
        {
            return;
        }

        for (std::size_t index = 0; index < previewRows.size(); ++index)
        {
            previewRows[index].componentName = BuildComponentNameForRule(
                session,
                previewRows[index].group,
                static_cast<int>(index + 1));
        }
        PopulateAssemblyList(previewRows);
    }

    void EditNameConfig()
    {
        const bool waited = OpenNameConfigFileAndWait();
        if (waited)
        {
            RefreshPreviewNamesFromConfig();
        }
    }

    NXOpen::BlockStyler::Tree::BeginLabelEditState OnBeginAssemblyNameEdit(
        NXOpen::BlockStyler::Tree* tree,
        NXOpen::BlockStyler::Node* node,
        int columnId)
    {
        (void)tree;
        return (node != NULL && columnId == kAssemblyNameColumnId)
            ? NXOpen::BlockStyler::Tree::BeginLabelEditStateAllow
            : NXOpen::BlockStyler::Tree::BeginLabelEditStateDisallow;
    }

    NXOpen::BlockStyler::Tree::EndLabelEditState OnEndAssemblyNameEdit(
        NXOpen::BlockStyler::Tree* tree,
        NXOpen::BlockStyler::Node* node,
        int columnId,
        NXOpen::NXString editedText)
    {
        (void)tree;
        if (node == NULL || columnId != kAssemblyNameColumnId)
        {
            return NXOpen::BlockStyler::Tree::EndLabelEditStateRejectText;
        }

        std::string name;
        try
        {
            name = SanitizeFileStem(editedText.GetLocaleText());
        }
        catch (...)
        {
            return NXOpen::BlockStyler::Tree::EndLabelEditStateRejectText;
        }
        if (name.empty())
        {
            return NXOpen::BlockStyler::Tree::EndLabelEditStateRejectText;
        }

        for (std::size_t index = 0; index < previewNodes.size(); ++index)
        {
            if (previewNodes[index] == node && index < previewRows.size())
            {
                previewRows[index].componentName = name;
                node->SetColumnDisplayText(
                    kAssemblyNameColumnId,
                    NXOpen::NXString(name, NXOpen::NXString::UTF8));
                break;
            }
        }
        return NXOpen::BlockStyler::Tree::EndLabelEditStateAcceptText;
    }

    void StoreSearchSelectionFromBodySelection()
    {
        searchSelectionObjects.clear();
        if (bodySelection == NULL)
        {
            return;
        }

        searchSelectionObjects = bodySelection->GetSelectedObjects();
    }

    std::vector<NXOpen::TaggedObject*> BuildTaggedObjectsFromTags(
        const std::vector<tag_t>& tags) const
    {
        std::vector<NXOpen::TaggedObject*> objects;
        objects.reserve(tags.size());
        for (std::size_t index = 0; index < tags.size(); ++index)
        {
            NXOpen::TaggedObject* object =
                NXOpen::NXObjectManager::Get(tags[index]);
            if (object != NULL)
            {
                objects.push_back(object);
            }
        }
        return objects;
    }

    void SetBodySelectionObjects(
        const std::vector<NXOpen::TaggedObject*>& objects,
        bool suppressUpdate)
    {
        if (bodySelection == NULL)
        {
            return;
        }

        const bool previousSuppress = suppressBodySelectionUpdate;
        suppressBodySelectionUpdate = suppressUpdate;
        try
        {
            std::vector<NXOpen::TaggedObject*> emptyObjects;
            bodySelection->SetSelectedObjects(emptyObjects);
            if (!objects.empty())
            {
                bodySelection->SetSelectedObjects(objects);
            }
        }
        catch (...)
        {
            suppressBodySelectionUpdate = previousSuppress;
            throw;
        }
        suppressBodySelectionUpdate = previousSuppress;
    }

    bool RestoreSearchSelectionForExecution()
    {
        if (!previewRowHighlightActive || searchSelectionObjects.empty())
        {
            return false;
        }

        SetBodySelectionObjects(searchSelectionObjects, true);
        previewRowHighlightActive = false;
        previewSelectionTags = GetCurrentSelectionTags();
        WritePreviewDebugLog("RestoreSearchSelectionForExecution restored body selection");
        return true;
    }

    bool ConfirmAndDeleteFeatureParameters(
        const std::vector<NXOpen::Body*>& selectedBodies)
    {
        const std::vector<tag_t> parameterBodyTags =
            FindBodiesWithFeatureParameters(selectedBodies);
        if (parameterBodyTags.empty())
        {
            WritePreviewDebugLog("ConfirmAndDeleteFeatureParameters skipped: no parameterized bodies");
            return true;
        }

        std::ostringstream message;
        message << Utf8Text("\xE6\xA3\x80\xE6\xB5\x8B\xE5\x88\xB0\x20")
                << parameterBodyTags.size()
                << Utf8Text("\x20\xE4\xB8\xAA\xE5\xAE\x9E\xE4\xBD\x93\xE5\xB8\xA6\xE6\x9C\x89\xE7\x89\xB9\xE5\xBE\x81\xE5\x8F\x82\xE6\x95\xB0\xE3\x80\x82\x0A"
                            "\xE8\xBD\xAC\xE8\xA3\x85\xE9\x85\x8D\xE5\x89\x8D\xE9\x9C\x80\xE8\xA6\x81\xE5\x85\x88\xE7\xA7\xBB\xE9\x99\xA4\xE8\xBF\x99\xE4\xBA\x9B\xE5\xAE\x9E\xE4\xBD\x93\xE7\x9A\x84\xE5\x8F\x82\xE6\x95\xB0\xE3\x80\x82\x0A"
                            "\xE7\xA7\xBB\xE9\x99\xA4\xE5\x90\x8E\xE5\xB0\x86\xE5\x8F\x98\xE4\xB8\xBA\xE6\x97\xA0\xE5\x8F\x82\xE5\xAE\x9E\xE4\xBD\x93\xEF\xBC\x8C\xE8\xAF\xB7\xE7\xA1\xAE\xE8\xAE\xA4\xE6\x98\xAF\xE5\x90\xA6\xE7\xBB\xA7\xE7\xBB\xAD\xEF\xBC\x9F");

        const int response = ui->NXMessageBox()->Show(
            NXOpen::NXString(kDialogName),
            NXOpen::NXMessageBox::DialogTypeQuestion,
            NXOpen::NXString(message.str(), NXOpen::NXString::UTF8));
        if (response != 1)
        {
            WritePreviewDebugLog("ConfirmAndDeleteFeatureParameters canceled by user");
            return false;
        }

        NXOpen::Session::UndoMarkId undoMark =
            session->SetUndoMark(
                NXOpen::Session::MarkVisibilityVisible,
                NXOpen::NXString(Utf8Text("\xE7\xA7\xBB\xE9\x99\xA4\xE7\x89\xB9\xE5\xBE\x81\xE5\x8F\x82\xE6\x95\xB0"), NXOpen::NXString::UTF8));
        DeleteBodyParameters(parameterBodyTags);
        session->SetUndoMarkName(
            undoMark,
            NXOpen::NXString(Utf8Text("\xE7\xA7\xBB\xE9\x99\xA4\xE7\x89\xB9\xE5\xBE\x81\xE5\x8F\x82\xE6\x95\xB0"), NXOpen::NXString::UTF8));

        std::ostringstream line;
        line << "ConfirmAndDeleteFeatureParameters deleted count="
             << parameterBodyTags.size();
        WritePreviewDebugLog(line.str());
        return true;
    }

    void OnAssemblyRowSelect(
        NXOpen::BlockStyler::Tree* tree,
        NXOpen::BlockStyler::Node* node,
        int columnId,
        bool isSelected)
    {
        (void)tree;
        (void)columnId;
        if (!isSelected || node == NULL)
        {
            return;
        }

        for (std::size_t index = 0; index < previewNodes.size() && index < previewRows.size(); ++index)
        {
            if (previewNodes[index] != node)
            {
                continue;
            }

            if (!previewRowHighlightActive)
            {
                StoreSearchSelectionFromBodySelection();
            }

            const std::vector<NXOpen::TaggedObject*> rowObjects =
                BuildTaggedObjectsFromTags(previewRows[index].tags);
            SetBodySelectionObjects(rowObjects, true);
            previewRowHighlightActive = true;

            std::ostringstream line;
            line << "OnAssemblyRowSelect highlighted row=" << (index + 1)
                 << ", bodies=" << rowObjects.size();
            WritePreviewDebugLog(line.str());
            return;
        }
    }

    std::vector<tag_t> GetCurrentSelectionTags()
    {
        std::vector<NXOpen::Body*> selectedBodies = GetSelectedBodies();
        std::vector<tag_t> tags;
        tags.reserve(selectedBodies.size());
        for (std::size_t index = 0; index < selectedBodies.size(); ++index)
        {
            tags.push_back(selectedBodies[index]->Tag());
        }
        return tags;
    }

    std::string FindExistingPreviewName(const std::vector<tag_t>& tags) const
    {
        const std::string key = BuildGroupKey(tags);
        for (std::size_t index = 0; index < previewRows.size(); ++index)
        {
            if (BuildGroupKey(previewRows[index].tags) == key)
            {
                return previewRows[index].componentName;
            }
        }
        return std::string();
    }

    std::string GetParentAssemblyDisplayName() const
    {
        if (session == NULL || session->Parts() == NULL)
        {
            return std::string();
        }

        NXOpen::Part* workPart = session->Parts()->Work();
        if (workPart == NULL)
        {
            return std::string();
        }

        const char* leafText = workPart->Leaf().GetUTF8Text();
        std::string parentName = leafText != NULL ? leafText : "";
        if (parentName.empty())
        {
            const char* fullPathText = workPart->FullPath().GetUTF8Text();
            parentName = fullPathText != NULL ? fullPathText : "";
        }

        if (parentName.empty())
        {
            return "WorkPart";
        }

        return parentName;
    }

    void ClearAssemblyList()
    {
        WritePreviewDebugLog("ClearAssemblyList begin");
        if (assemblyList == NULL || !assemblyListReady)
        {
            previewNodes.clear();
            previewRows.clear();
            WritePreviewDebugLog("ClearAssemblyList skipped: list not ready");
            return;
        }

        assemblyList->Redraw(false);
        for (std::size_t index = previewNodes.size(); index > 0; --index)
        {
            NXOpen::BlockStyler::Node* node = previewNodes[index - 1];
            if (node != NULL)
            {
                assemblyList->DeleteNode(node);
            }
        }
        assemblyList->Redraw(true);
        previewNodes.clear();
        previewRows.clear();
        WritePreviewDebugLog("ClearAssemblyList end");
    }

    void PopulateAssemblyList(const std::vector<AssemblyPreviewRow>& rows)
    {
        std::vector<AssemblyPreviewRow> rowsCopy = rows;
        std::ostringstream beginLine;
        beginLine << "PopulateAssemblyList begin, rows=" << rowsCopy.size();
        WritePreviewDebugLog(beginLine.str());
        ClearAssemblyList();
        previewRows = rowsCopy;

        if (assemblyList == NULL || !assemblyListReady)
        {
            WritePreviewDebugLog("PopulateAssemblyList skipped: list not ready");
            return;
        }

        assemblyList->Redraw(false);
        for (std::size_t index = 0; index < previewRows.size(); ++index)
        {
            AssemblyPreviewRow& row = previewRows[index];
            NXOpen::BlockStyler::Node* node =
                assemblyList->CreateNode(row.componentName.c_str());

            assemblyList->InsertNode(
                node,
                NULL,
                NULL,
                NXOpen::BlockStyler::Tree::NodeInsertOptionLast);

            node->SetColumnDisplayText(
                kAssemblyNameColumnId,
                NXOpen::NXString(row.componentName, NXOpen::NXString::UTF8));
            node->SetColumnDisplayText(
                kAssemblyParentColumnId,
                NXOpen::NXString(row.parentName, NXOpen::NXString::UTF8));

            std::ostringstream quantity;
            quantity << row.group.instances.size();
            node->SetColumnDisplayText(
                kAssemblyQuantityColumnId,
                NXOpen::NXString(quantity.str(), NXOpen::NXString::UTF8));

            previewNodes.push_back(node);
        }
        assemblyList->Redraw(true);
        WritePreviewDebugLog("PopulateAssemblyList end");
    }

    std::vector<AssemblyPreviewRow> BuildPreviewRows(
        const std::vector<MatchedBodyGroup>& assemblyGroups)
    {
        std::vector<AssemblyPreviewRow> rows;
        rows.reserve(assemblyGroups.size());
        const std::string parentName = GetParentAssemblyDisplayName();
        for (std::size_t index = 0; index < assemblyGroups.size(); ++index)
        {
            AssemblyPreviewRow row = {};
            row.group = assemblyGroups[index];
            row.tags = ExtractGroupTags(assemblyGroups[index]);
            row.componentName = FindExistingPreviewName(row.tags);
            if (row.componentName.empty())
            {
                row.componentName = BuildComponentNameForRule(
                    session,
                    row.group,
                    static_cast<int>(index + 1));
            }
            row.parentName = parentName;
            rows.push_back(row);
        }
        return rows;
    }

    void ClearCachedSearchData()
    {
        DeleteDebugCoordinateObjects(&cachedSearchData.activeDebugObjectTags);
        cachedSearchData = SameBodySearchData();
        cachedSearchSelectionTags.clear();
        cachedSearchDataValid = false;
        WritePreviewDebugLog("ClearCachedSearchData");
    }

    bool CachedSearchMatchesSelection(const std::vector<tag_t>& selectionTags) const
    {
        return cachedSearchDataValid && selectionTags == cachedSearchSelectionTags;
    }

    void StoreCachedSearchData(
        const SameBodySearchData& data,
        const std::vector<tag_t>& selectionTags)
    {
        ClearCachedSearchData();
        cachedSearchData = data;
        cachedSearchData.activeDebugObjectTags.clear();
        cachedSearchSelectionTags = selectionTags;
        cachedSearchDataValid = true;
        WritePreviewDebugLog("StoreCachedSearchData");
    }

    void RefreshAssemblyPreview()
    {
        WritePreviewDebugLog("RefreshAssemblyPreview begin");
        RefreshBlockPointers();
        InitializeAssemblyList();
        if (assemblyList == NULL || !assemblyListReady)
        {
            WritePreviewDebugLog("RefreshAssemblyPreview stopped: list not ready");
            return;
        }

        const std::vector<tag_t> selectionTags = GetCurrentSelectionTags();
        if (selectionTags == previewSelectionTags)
        {
            WritePreviewDebugLog("RefreshAssemblyPreview skipped: same selection");
            return;
        }
        previewSelectionTags = selectionTags;

        std::vector<NXOpen::Body*> selectedBodies = GetSelectedBodies();
        {
            std::ostringstream line;
            line << "RefreshAssemblyPreview selectedBodies=" << selectedBodies.size();
            WritePreviewDebugLog(line.str());
        }
        if (selectedBodies.empty())
        {
            ClearAssemblyList();
            ClearCachedSearchData();
            WritePreviewDebugLog("RefreshAssemblyPreview stopped: empty selection");
            return;
        }

        if (CachedSearchMatchesSelection(selectionTags))
        {
            WritePreviewDebugLog("RefreshAssemblyPreview using cached search data");
            PopulateAssemblyList(BuildPreviewRows(cachedSearchData.assemblyGroups));
            return;
        }

        SameBodySearchData data = BuildSameBodySearchData(selectedBodies, NULL);
        {
            std::ostringstream line;
            line << "RefreshAssemblyPreview groups="
                 << data.assemblyGroups.size()
                 << ", matched=" << data.matchedGroups.size()
                 << ", failed=" << data.failedTags.size();
            WritePreviewDebugLog(line.str());
        }
        DeleteDebugCoordinateObjects(&data.activeDebugObjectTags);
        StoreCachedSearchData(data, selectionTags);
        PopulateAssemblyList(BuildPreviewRows(data.assemblyGroups));
        WritePreviewDebugLog("RefreshAssemblyPreview end");
    }

    void BuildAssemblyPreviewFromSelection()
    {
        WritePreviewDebugLog("BuildAssemblyPreviewFromSelection begin");
        RefreshBlockPointers();
        InitializeAssemblyList();
        if (assemblyList == NULL || !assemblyListReady)
        {
            WritePreviewDebugLog("BuildAssemblyPreviewFromSelection stopped: list not ready");
            if (kWriteResultToListingWindow)
            {
                NXOpen::ListingWindow* listingWindow = session->ListingWindow();
                if (listingWindow != NULL && !listingWindow->IsOpen())
                {
                    listingWindow->Open();
                }
                WriteLineUtf8(listingWindow, Utf8Text("\xE5\x88\x97\xE8\xA1\xA8\xE6\x8E\xA7\xE4\xBB\xB6\xE6\x9C\xAA\xE5\x8A\xA0\xE8\xBD\xBD"));
            }
            return;
        }

        std::vector<NXOpen::Body*> selectedBodies = GetSelectedBodies();
        {
            std::ostringstream line;
            line << "BuildAssemblyPreviewFromSelection selectedBodies=" << selectedBodies.size();
            WritePreviewDebugLog(line.str());
        }
        if (selectedBodies.empty())
        {
            ClearAssemblyList();
            previewSelectionTags.clear();
            ClearCachedSearchData();
            WritePreviewDebugLog("BuildAssemblyPreviewFromSelection stopped: empty selection");
            return;
        }

        const std::vector<tag_t> selectionTags = GetCurrentSelectionTags();
        if (CachedSearchMatchesSelection(selectionTags))
        {
            WritePreviewDebugLog("BuildAssemblyPreviewFromSelection using cached search data");
            PopulateAssemblyList(BuildPreviewRows(cachedSearchData.assemblyGroups));
            previewSelectionTags = selectionTags;
            StoreSearchSelectionFromBodySelection();
            previewRowHighlightActive = false;
            return;
        }

        SameBodySearchData data = BuildSameBodySearchData(selectedBodies, NULL);
        {
            std::ostringstream line;
            line << "BuildAssemblyPreviewFromSelection search groups="
                 << data.assemblyGroups.size()
                 << ", matched=" << data.matchedGroups.size()
                 << ", failed=" << data.failedTags.size();
            WritePreviewDebugLog(line.str());
        }
        DeleteDebugCoordinateObjects(&data.activeDebugObjectTags);
        StoreCachedSearchData(data, selectionTags);
        PopulateAssemblyList(BuildPreviewRows(data.assemblyGroups));
        previewSelectionTags = selectionTags;
        StoreSearchSelectionFromBodySelection();
        previewRowHighlightActive = false;

        if (kWriteResultToListingWindow)
        {
            NXOpen::ListingWindow* listingWindow = session->ListingWindow();
            if (listingWindow != NULL && !listingWindow->IsOpen())
            {
                listingWindow->Open();
            }
            std::ostringstream line;
            line << Utf8Text("\xE8\xAF\x86\xE5\x88\xAB\xE5\x88\x97\xE8\xA1\xA8\xE8\xA1\x8C\xE6\x95\xB0\x3D")
                 << data.assemblyGroups.size();
            WriteLineUtf8(listingWindow, line.str());
        }
        WritePreviewDebugLog("BuildAssemblyPreviewFromSelection end");
    }

    void HarvestPreviewNames()
    {
        for (std::size_t index = 0; index < previewNodes.size() && index < previewRows.size(); ++index)
        {
            NXOpen::BlockStyler::Node* node = previewNodes[index];
            if (node == NULL)
            {
                continue;
            }
            previewRows[index].componentName =
                SanitizeFileStem(node->GetColumnDisplayText(kAssemblyNameColumnId).GetLocaleText());
        }
    }

    std::vector<std::string> ResolveComponentNames(
        const std::vector<MatchedBodyGroup>& assemblyGroups)
    {
        HarvestPreviewNames();

        std::vector<std::string> names;
        names.reserve(assemblyGroups.size());
        for (std::size_t index = 0; index < assemblyGroups.size(); ++index)
        {
            const std::vector<tag_t> tags = ExtractGroupTags(assemblyGroups[index]);
            std::string name = FindExistingPreviewName(tags);
            if (name.empty())
            {
                name = BuildComponentNameForRule(
                    session,
                    assemblyGroups[index],
                    static_cast<int>(index + 1));
            }
            names.push_back(name);
        }
        return names;
    }

    int Ok()
    {
        WritePreviewDebugLog("Ok begin");
        return ConvertSelectedBodies();
    }

    int Apply()
    {
        WritePreviewDebugLog("Apply begin");
        try
        {
            RefreshBlockPointers();
            RestoreSearchSelectionForExecution();
            std::vector<NXOpen::Body*> selectedBodies = GetSelectedBodies();
            {
                std::ostringstream line;
                line << "Apply selectedBodies=" << selectedBodies.size();
                WritePreviewDebugLog(line.str());
            }
            if (selectedBodies.empty())
            {
                ui->NXMessageBox()->Show(
                    NXOpen::NXString(kDialogName),
                    NXOpen::NXMessageBox::DialogTypeInformation,
                    NXOpen::NXString(
                        Utf8Text("\xE8\xAF\xB7\xE8\x87\xB3\xE5\xB0\x91\xE9\x80\x89\xE6\x8B\xA9\xE4\xB8\x80\xE4\xB8\xAA\xE5\xAE\x9E\xE4\xBD\x93\xE3\x80\x82"),
                        NXOpen::NXString::UTF8));
                return 1;
            }

            BuildAssemblyPreviewFromSelection();
            WritePreviewDebugLog("Apply end");
            return 0;
        }
        catch (const NXOpen::NXException& ex)
        {
            LogCallbackError("Apply NXException", ex.Message());
            return 1;
        }
        catch (const std::exception& ex)
        {
            LogCallbackError("Apply exception", ex.what());
            return 1;
        }
        catch (...)
        {
            LogCallbackError("Apply unknown exception", "Unknown apply error.");
            return 1;
        }
    }

    void ExecuteCachedSameBodySearch()
    {
        WritePreviewDebugLog("ExecuteCachedSameBodySearch begin");
        std::vector<tag_t> colorFailedTags;
        AssemblyConversionResult assemblyConversionResult = {};
        const std::vector<std::vector<tag_t> >& matchedGroups =
            cachedSearchData.matchedGroups;
        const std::vector<MatchedBodyGroup>& assemblyGroups =
            cachedSearchData.assemblyGroups;

        if (!cachedSearchData.coordinateDebugCanceled)
        {
            DeleteDebugCoordinateObjects(&cachedSearchData.activeDebugObjectTags);
            if (gColorMatchedBodies)
            {
                ColorMatchedGroups(session, matchedGroups, colorFailedTags);
            }
            if (kConvertMatchedGroupsToAssembly)
            {
                const std::vector<std::string> componentNames =
                    ResolveComponentNames(assemblyGroups);
                assemblyConversionResult =
                    ConvertMatchedGroupsToAssembly(session, assemblyGroups, componentNames);
                {
                    std::ostringstream line;
                    line << "Cached assembly conversion convertedGroups="
                         << assemblyConversionResult.convertedGroups
                         << ", components=" << assemblyConversionResult.createdComponents
                         << ", deletedOriginalBodies=" << assemblyConversionResult.deletedOriginalBodies
                         << ", failedGroups=" << assemblyConversionResult.failedGroups;
                    WritePreviewDebugLog(line.str());
                }
                PopulateAssemblyList(BuildPreviewRows(assemblyGroups));
            }
        }

        {
            std::ostringstream line;
            line << "ExecuteCachedSameBodySearch end, matched="
                 << matchedGroups.size()
                 << ", assemblyInputGroups=" << assemblyGroups.size()
                 << ", colorFailed=" << colorFailedTags.size()
                 << ", convertedGroups=" << assemblyConversionResult.convertedGroups;
            WritePreviewDebugLog(line.str());
        }
    }

    int ConvertSelectedBodies()
    {
        WritePreviewDebugLog("ConvertSelectedBodies begin");
        try
        {
            RefreshBlockPointers();
            RestoreSearchSelectionForExecution();
            std::vector<NXOpen::Body*> selectedBodies = GetSelectedBodies();
            {
                std::ostringstream line;
                line << "ConvertSelectedBodies selectedBodies=" << selectedBodies.size();
                WritePreviewDebugLog(line.str());
            }
            if (selectedBodies.empty())
            {
                ui->NXMessageBox()->Show(
                    NXOpen::NXString(kDialogName),
                    NXOpen::NXMessageBox::DialogTypeInformation,
                    NXOpen::NXString(
                        Utf8Text("\xE8\xAF\xB7\xE8\x87\xB3\xE5\xB0\x91\xE9\x80\x89\xE6\x8B\xA9\xE4\xB8\x80\xE4\xB8\xAA\xE5\xAE\x9E\xE4\xBD\x93\xE3\x80\x82"),
                        NXOpen::NXString::UTF8));
                return 1;
            }

            if (!ConfirmAndDeleteFeatureParameters(selectedBodies))
            {
                WritePreviewDebugLog("ConvertSelectedBodies stopped: delete parameters canceled");
                return 1;
            }
            selectedBodies = GetSelectedBodies();
            StoreSearchSelectionFromBodySelection();

            UpdateSelectedOutputFolder();
            const std::vector<tag_t> selectionTags = GetCurrentSelectionTags();
            if (CachedSearchMatchesSelection(selectionTags))
            {
                WritePreviewDebugLog("ConvertSelectedBodies using cached search data");
                ExecuteCachedSameBodySearch();
                ClearCachedSearchData();
            }
            else
            {
                RunSameBodySearch(selectedBodies);
                ClearCachedSearchData();
            }
            WritePreviewDebugLog("ConvertSelectedBodies end");
            return 0;
        }
        catch (const NXOpen::NXException& ex)
        {
            LogCallbackError("ConvertSelectedBodies NXException", ex.Message());
            return 1;
        }
        catch (const std::exception& ex)
        {
            LogCallbackError("ConvertSelectedBodies exception", ex.what());
            return 1;
        }
        catch (...)
        {
            LogCallbackError("ConvertSelectedBodies unknown exception", "Unknown conversion error.");
            return 1;
        }
    }

    void ConfigureBodySelectionFilter()
    {
        if (bodySelection == NULL)
        {
            return;
        }

        std::vector<NXOpen::Selection::MaskTriple> masks;
        masks.push_back(NXOpen::Selection::MaskTriple(
            UF_solid_type,
            UF_solid_body_subtype,
            UF_UI_SEL_FEATURE_BODY));

        NXOpen::BlockStyler::PropertyList* properties = bodySelection->GetProperties();
        properties->SetEnum("SelectMode", 1);
        properties->SetLogical("AutomaticProgression", false);
        properties->SetSelectionFilter(
            "SelectionFilter",
            NXOpen::Selection::SelectionActionClearAndEnableSpecific,
            masks);
        delete properties;
    }

    std::vector<NXOpen::Body*> GetSelectedBodies()
    {
        RefreshBlockPointers();
        std::vector<NXOpen::Body*> bodies;
        if (bodySelection == NULL)
        {
            WritePreviewDebugLog("GetSelectedBodies stopped: bodySelection is null");
            return bodies;
        }

        std::vector<NXOpen::TaggedObject*> selectedObjects =
            bodySelection->GetSelectedObjects();

        std::set<tag_t> seenTags;
        for (std::size_t index = 0; index < selectedObjects.size(); ++index)
        {
            NXOpen::Body* body = dynamic_cast<NXOpen::Body*>(selectedObjects[index]);
            if (body == NULL)
            {
                continue;
            }

            if (seenTags.insert(body->Tag()).second)
            {
                bodies.push_back(body);
            }
        }

        std::ostringstream line;
        line << "GetSelectedBodies bodies=" << bodies.size();
        WritePreviewDebugLog(line.str());
        return bodies;
    }

    void RunSameBodySearch(const std::vector<NXOpen::Body*>& selectedBodies)
    {
        std::ostringstream beginLine;
        beginLine << "RunSameBodySearch begin, selectedBodies=" << selectedBodies.size();
        WritePreviewDebugLog(beginLine.str());
        NXOpen::ListingWindow* listingWindow = NULL;
        if (kWriteResultToListingWindow)
        {
            listingWindow = session->ListingWindow();
            if (listingWindow != NULL && !listingWindow->IsOpen())
            {
                listingWindow->Open();
            }
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

        SameBodySearchData searchData = BuildSameBodySearchData(selectedBodies, &debugLog);
        {
            std::ostringstream line;
            line << "RunSameBodySearch data groups="
                 << searchData.assemblyGroups.size()
                 << ", matched=" << searchData.matchedGroups.size()
                 << ", failed=" << searchData.failedTags.size();
            WritePreviewDebugLog(line.str());
        }
        const std::vector<BodyFingerprint>& fingerprints = searchData.fingerprints;
        const std::vector<tag_t>& failedTags = searchData.failedTags;
        const std::vector<std::vector<tag_t> >& matchedGroups = searchData.matchedGroups;
        const std::vector<MatchedBodyGroup>& assemblyGroups = searchData.assemblyGroups;
        std::vector<tag_t> colorFailedTags;
        AssemblyConversionResult assemblyConversionResult = {};

        if (searchData.coordinateDebugCanceled)
        {
            if (debugLog.is_open())
            {
                debugLog << "Coordinate debug canceled; active coordinate objects were kept."
                         << std::endl;
            }
        }
        else
        {
            DeleteDebugCoordinateObjects(&searchData.activeDebugObjectTags);
            if (gColorMatchedBodies)
            {
                ColorMatchedGroups(session, matchedGroups, colorFailedTags);
            }
            if (kConvertMatchedGroupsToAssembly)
            {
                const std::vector<std::string> componentNames =
                    ResolveComponentNames(assemblyGroups);
                assemblyConversionResult =
                    ConvertMatchedGroupsToAssembly(session, assemblyGroups, componentNames);
                {
                    std::ostringstream line;
                    line << "Assembly conversion convertedGroups="
                         << assemblyConversionResult.convertedGroups
                         << ", components=" << assemblyConversionResult.createdComponents
                         << ", deletedOriginalBodies=" << assemblyConversionResult.deletedOriginalBodies
                         << ", failedGroups=" << assemblyConversionResult.failedGroups;
                    WritePreviewDebugLog(line.str());
                }
                for (std::size_t reasonIndex = 0;
                     reasonIndex < assemblyConversionResult.failureReasons.size();
                     ++reasonIndex)
                {
                    WritePreviewDebugLog(
                        "Assembly conversion failure: " +
                        assemblyConversionResult.failureReasons[reasonIndex]);
                }
                PopulateAssemblyList(BuildPreviewRows(assemblyGroups));
            }
        }

        if (kWriteResultToListingWindow && listingWindow != NULL)
        {
            listingWindow->WriteLine("");
            WriteLineUtf8(
                listingWindow,
                Utf8Text("\x3D\x3D\x3D\x3D\x20\xE7\x9B\xB8\xE5\x90\x8C\xE4\xBD\x93\xE7\xBB\x93\xE6\x9E\x9C\x20\x3D\x3D\x3D\x3D"));
            if (searchData.coordinateDebugCanceled)
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

            if (!searchData.coordinateDebugCanceled && kConvertMatchedGroupsToAssembly && !assemblyGroups.empty())
            {
                std::ostringstream line;
                line << Utf8Text("\xE8\xBD\xAC\xE8\xA3\x85\xE9\x85\x8D\xEF\xBC\x9A\xE7\xBB\x84\xE6\x95\xB0\x3D")
                     << assemblyConversionResult.convertedGroups
                     << Utf8Text("\xEF\xBC\x8C\xE7\xBB\x84\xE4\xBB\xB6\xE6\x95\xB0\x3D")
                     << assemblyConversionResult.createdComponents
                     << Utf8Text("\xEF\xBC\x8C\xE5\x88\xA0\xE9\x99\xA4\xE5\x8E\x9F\xE5\xAE\x9E\xE4\xBD\x93\x3D")
                     << assemblyConversionResult.deletedOriginalBodies;
                if (assemblyConversionResult.failedGroups > 0)
                {
                    line << Utf8Text("\xEF\xBC\x8C\xE5\xA4\xB1\xE8\xB4\xA5\xE7\xBB\x84\xE6\x95\xB0\x3D")
                         << assemblyConversionResult.failedGroups;
                }
                WriteLineUtf8(listingWindow, line.str());

                if (!assemblyConversionResult.partFiles.empty())
                {
                    std::ostringstream fileLine;
                    fileLine << Utf8Text("\xE9\x9B\xB6\xE4\xBB\xB6\xE7\x9B\xAE\xE5\xBD\x95\x3D")
                             << DirectoryName(assemblyConversionResult.partFiles[0]);
                    WriteLineUtf8(listingWindow, fileLine.str());
                }

                const std::size_t reasonCount =
                    std::min<std::size_t>(assemblyConversionResult.failureReasons.size(), 5);
                for (std::size_t reasonIndex = 0; reasonIndex < reasonCount; ++reasonIndex)
                {
                    std::ostringstream reasonLine;
                    reasonLine << Utf8Text("\xE8\xBD\xAC\xE8\xA3\x85\xE9\x85\x8D\xE5\xA4\xB1\xE8\xB4\xA5\xE5\x8E\x9F\xE5\x9B\xA0\x3A\x20")
                               << assemblyConversionResult.failureReasons[reasonIndex];
                    WriteLineUtf8(listingWindow, reasonLine.str());
                }
            }
        }

        if (debugLog.is_open())
        {
            debugLog << "Matched groups=" << matchedGroups.size()
                     << ", assemblyInputGroups=" << assemblyGroups.size()
                     << ", buildFailed=" << failedTags.size()
                     << ", colorFailed=" << colorFailedTags.size()
                     << ", assemblyConvertedGroups=" << assemblyConversionResult.convertedGroups
                     << ", assemblyComponents=" << assemblyConversionResult.createdComponents
                     << ", assemblyFailedGroups=" << assemblyConversionResult.failedGroups
                     << std::endl;
        }

        if (kShowCompletionMessageBox)
        {
            std::string completeMessageText;
            if (searchData.coordinateDebugCanceled)
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
        WritePreviewDebugLog("RunSameBodySearch end");
    }
};

void ShowError(const char* message)
{
    NXOpen::UI::GetUI()->NXMessageBox()->Show(
        kDialogName,
        NXOpen::NXMessageBox::DialogTypeError,
        message);
}
}

extern "C" DllExport void ufusr(char* param, int* retcode, int param_len)
{
    (void)param;
    (void)param_len;

    if (retcode != NULL)
    {
        *retcode = 0;
    }

    if (!zhihui_license_guard::EnsureAuthorized(L"ZHIHUI.DUOSITIZUANZUANPEI", L"DuoSiTiZuanZuanPei"))
    {
        if (retcode != NULL)
        {
            *retcode = 1;
        }
        return;
    }

    int initialized = 0;
    try
    {
        UF_initialize();
        initialized = 1;

        DuoSiTiZuanZuanPeiDialog commandDialog;
        commandDialog.Show();

        UF_terminate();
    }
    catch (const NXOpen::NXException& ex)
    {
        NXOpen::UI::GetUI()->NXMessageBox()->Show(
            kDialogName,
            NXOpen::NXMessageBox::DialogTypeError,
            ex.Message());
        if (retcode != NULL)
        {
            *retcode = ex.ErrorCode();
        }
        if (initialized != 0)
        {
            UF_terminate();
        }
    }
    catch (const std::exception& ex)
    {
        ShowError(ex.what());
        if (retcode != NULL)
        {
            *retcode = 1;
        }
        if (initialized != 0)
        {
            UF_terminate();
        }
    }
}

extern "C" DllExport int ufusr_ask_unload()
{
    return UF_UNLOAD_IMMEDIATELY;
}
