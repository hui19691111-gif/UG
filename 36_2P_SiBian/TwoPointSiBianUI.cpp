#include "TwoPointSiBianUI.hpp"

#include <NXOpen/BlockStyler_Enumeration.hxx>
#include <NXOpen/BlockStyler_PropertyList.hxx>
#include <NXOpen/BlockStyler_StringBlock.hxx>
#include <NXOpen/Direction.hxx>
#include <NXOpen/DirectionCollection.hxx>
#include <NXOpen/Features_CustomAttribute.hxx>
#include <NXOpen/Features_CustomAttributeCollection.hxx>
#include <NXOpen/Features_CustomDoubleAttribute.hxx>
#include <NXOpen/Features_BooleanBuilder.hxx>
#include <NXOpen/Features_CustomFeatureBuilder.hxx>
#include <NXOpen/Features_CustomFeatureData.hxx>
#include <NXOpen/Features_CustomFeatureDataCollection.hxx>
#include <NXOpen/Features_CustomTagAttribute.hxx>
#include <NXOpen/Features_Feature.hxx>
#include <NXOpen/Features_FeatureCollection.hxx>
#include <NXOpen/Features_OffsetFaceBuilder.hxx>
#include <NXOpen/Features_SheetMetal_EdgeRipBuilder.hxx>
#include <NXOpen/Features_SheetMetal_SheetmetalManager.hxx>
#include <NXOpen/FaceFeatureRule.hxx>
#include <NXOpen/MeasureDistance.hxx>
#include <NXOpen/MeasureFaces.hxx>
#include <NXOpen/MeasureManager.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXMessageBox.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/BodyCollection.hxx>
#include <NXOpen/Point.hxx>
#include <NXOpen/PointCollection.hxx>
#include <NXOpen/Section.hxx>
#include <NXOpen/SectionCollection.hxx>
#include <NXOpen/ScRuleFactory.hxx>
#include <NXOpen/SelectionIntentRule.hxx>
#include <NXOpen/TaggedObject.hxx>
#include <NXOpen/Unit.hxx>
#include <NXOpen/UnitCollection.hxx>

#include <uf.h>
#include <uf_assem.h>
#include <uf_curve.h>
#include <uf_modl.h>
#include <uf_modl_sweep.h>
#include <uf_modl_expressions_retiring.h>
#include <uf_modl_udf.h>
#include <uf_modl_utilities.h>
#include <uf_obj.h>
#include <uf_object_types.h>
#include <uf_part.h>
#include <uf_point.h>
#include <uf_ui.h>

#include <windows.h>
#include <objbase.h>
#include "resource.h"
#ifdef CreateDialog
#undef CreateDialog
#endif

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <set>
#include <string>
#include <ctime>
#include <vector>

using namespace NXOpen;

namespace
{
constexpr double kPointTolerance = 1.0e-4;
constexpr double kPlaneTolerance = 1.0e-3;
constexpr double kCornerExtensionDistance = 0.05;
constexpr double kThicknessProjectionOverlapRatio = 0.60;
constexpr double kThicknessProjectionAreaErrorRatio = 0.05;
constexpr double kEndpointPairMinimumAngleDegrees = 150.0;
constexpr double kEndpointPairMaximumAngleDegrees = 180.0;
constexpr double kConvexCornerMaximumAngleDegrees = 180.0;
constexpr const char* kTemplatePartName = "2p_SiBian_1.prt";
constexpr const char* kTemplate90LeftPartName = "2P_SiBian_90R.prt";
constexpr const wchar_t* kTempTemplateRoot = L"ZhihuiSheetMetal\\UDF\\36_2P_SiBian";

struct ProjectionPoint2d
{
    double x = 0.0;
    double y = 0.0;
};

struct ThicknessCandidate
{
    Face* face = nullptr;
    double planeDistance = 0.0;
    double signedPlaneDistance = 0.0;
};

struct BoundaryPointCandidate
{
    Point3d point;
    double distance = 0.0;
    double thicknessScore = 0.0;
};

struct UdfTemplateSpec
{
    int resourceId;
    const wchar_t* tempPrefix;
    const char* logName;
};

UdfTemplateSpec TemplateSpecForMode(TwoPointSiBianUI::FeatureMode mode)
{
    if (mode == TwoPointSiBianUI::FeatureMode::NinetyLeft)
    {
        return {IDR_UDF_TEMPLATE_90L_PRT, L"2P_SiBian_90L_", kTemplate90LeftPartName};
    }

    return {IDR_UDF_TEMPLATE_PRT, L"2p_SiBian_1_", kTemplatePartName};
}

std::string NarrowFromWide(const std::wstring& value)
{
    if (value.empty())
    {
        return {};
    }

    const int size = WideCharToMultiByte(
        CP_ACP,
        0,
        value.c_str(),
        static_cast<int>(value.size()),
        nullptr,
        0,
        nullptr,
        nullptr);
    if (size <= 0)
    {
        return {};
    }

    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(
        CP_ACP,
        0,
        value.c_str(),
        static_cast<int>(value.size()),
        result.data(),
        size,
        nullptr,
        nullptr);
    return result;
}

std::string CurrentModuleDir()
{
    HMODULE module = nullptr;
    if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&CurrentModuleDir),
            &module) ||
        module == nullptr)
    {
        return {};
    }

    std::wstring path(32768, L'\0');
    const DWORD length = GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
    if (length == 0 || length >= path.size())
    {
        return {};
    }

    path.resize(length);
    const std::size_t slash = path.find_last_of(L"\\/");
    if (slash == std::wstring::npos)
    {
        return {};
    }

    return NarrowFromWide(path.substr(0, slash));
}

std::string DeployedApplicationDir()
{
    const unsigned char bytes[] = {
        'D', ':', '\\',
        0x55, 0x47,
        0xe6, 0x99, 0xba,
        0xe8, 0xbe, 0x89,
        0xe9, 0x92, 0xa3,
        0xe9, 0x87, 0x91,
        0xe6, 0x8f, 0x92,
        0xe4, 0xbb, 0xb6,
        '\\', 'a', 'p', 'p', 'l', 'i', 'c', 'a', 't', 'i', 'o', 'n'};
    return std::string(reinterpret_cast<const char*>(bytes), sizeof(bytes));
}

HMODULE CurrentModuleHandle()
{
    HMODULE module = nullptr;
    if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&CurrentModuleHandle),
            &module))
    {
        return nullptr;
    }
    return module;
}

std::string DebugLogPath()
{
    const char* temp = std::getenv("TEMP");
    if (temp != nullptr && temp[0] != '\0')
    {
        return std::string(temp) + "\\TwoPointSiBian_udf.log";
    }
    return "TwoPointSiBian_udf.log";
}

std::string UfMessage(int code)
{
    if (code == 0)
    {
        return "OK";
    }

    char message[133] = {0};
    UF_get_fail_message(code, message);
    return message[0] != '\0' ? message : "UF error " + std::to_string(code);
}

void AppendDebugLog(const std::string& message)
{
    std::ofstream log(DebugLogPath(), std::ios::app);
    if (log)
    {
        std::time_t now = std::time(nullptr);
        std::tm localTime = {};
        localtime_s(&localTime, &now);
        char stamp[32] = {0};
        std::strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &localTime);
        log << "[" << stamp << "] " << message << std::endl;
    }
}

void BeginDebugLogSection()
{
    std::ostringstream trace;
    trace << "\n============================================================\n"
          << "2P_SiBian apply begin\n"
          << "log=" << DebugLogPath() << "\n"
          << "moduleDir=" << CurrentModuleDir() << "\n"
          << "deployedDir=" << DeployedApplicationDir();
    AppendDebugLog(trace.str());
}

std::string TrimAscii(const std::string& value)
{
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    });
    if (first == value.end())
    {
        return {};
    }
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    }).base();
    return std::string(first, last);
}

std::string ReadStringBlockValue(NXOpen::BlockStyler::StringBlock* block, const char* blockName, const char* fallback)
{
    if (block == nullptr)
    {
        AppendDebugLog(std::string("string block missing: ") + blockName + ", using fallback=" + fallback);
        return fallback;
    }

    NXOpen::BlockStyler::PropertyList* properties = nullptr;
    try
    {
        properties = block->GetProperties();
        const NXString value = properties->GetString("Value");
        const char* text = value.GetLocaleText();
        std::string result = TrimAscii(text != nullptr ? text : "");
        delete properties;
        properties = nullptr;

        if (result.empty())
        {
            result = fallback;
        }
        AppendDebugLog(std::string("read string block ") + blockName + "=" + result);
        return result;
    }
    catch (const NXException& ex)
    {
        if (properties != nullptr)
        {
            delete properties;
        }
        AppendDebugLog(std::string("failed to read string block ") + blockName + ": " + UfMessage(ex.ErrorCode()));
        return fallback;
    }
    catch (...)
    {
        if (properties != nullptr)
        {
            delete properties;
        }
        AppendDebugLog(std::string("failed to read string block ") + blockName + ", using fallback=" + fallback);
        return fallback;
    }
}

std::string NxExceptionText(const NXException& ex)
{
    const int code = ex.ErrorCode();
    if (code != 0)
    {
        return UfMessage(code);
    }

    return "NXOpen exception. See " + DebugLogPath();
}

std::wstring MakeUniqueId()
{
    GUID guid = {};
    if (SUCCEEDED(CoCreateGuid(&guid)))
    {
        wchar_t buffer[64] = {0};
        if (StringFromGUID2(guid, buffer, static_cast<int>(std::size(buffer))) > 0)
        {
            std::wstring result = buffer;
            result.erase(std::remove(result.begin(), result.end(), L'{'), result.end());
            result.erase(std::remove(result.begin(), result.end(), L'}'), result.end());
            return result;
        }
    }

    const auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    std::wostringstream fallback;
    fallback << GetCurrentProcessId() << L"_" << GetCurrentThreadId() << L"_" << now;
    return fallback.str();
}

class ExtractedTemplatePart
{
public:
    ExtractedTemplatePart() = default;
    ~ExtractedTemplatePart()
    {
        Cleanup();
    }

    bool Extract(TwoPointSiBianUI::FeatureMode mode, std::string& path, std::string& trace)
    {
        Cleanup();
        const UdfTemplateSpec spec = TemplateSpecForMode(mode);

        HMODULE module = CurrentModuleHandle();
        if (module == nullptr)
        {
            trace += "failed to get module handle\n";
            return false;
        }

        HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(spec.resourceId), RT_RCDATA);
        if (resource == nullptr)
        {
            trace += std::string("embedded UDF template resource was not found: ") + spec.logName + "\n";
            return false;
        }

        const DWORD byteCount = SizeofResource(module, resource);
        HGLOBAL loaded = LoadResource(module, resource);
        const void* bytes = LockResource(loaded);
        if (byteCount == 0 || bytes == nullptr)
        {
            trace += "embedded UDF template resource is empty or unreadable\n";
            return false;
        }

        try
        {
            const std::wstring uniqueId = MakeUniqueId();
            directory_ = std::filesystem::temp_directory_path() /
                         std::filesystem::path(kTempTemplateRoot) /
                         std::filesystem::path(uniqueId);
            std::filesystem::create_directories(directory_);
            file_ = directory_ / std::filesystem::path(std::wstring(spec.tempPrefix) + uniqueId + L".prt");

            std::ofstream output(file_, std::ios::binary | std::ios::trunc);
            if (!output)
            {
                trace += "failed to create temp UDF template file\n";
                return false;
            }
            output.write(static_cast<const char*>(bytes), byteCount);
            output.close();
            if (!output)
            {
                trace += "failed to write temp UDF template bytes\n";
                return false;
            }

            path = file_.string();
            trace += std::string("extracted embedded UDF template ") + spec.logName + " to " + path +
                     ", bytes=" + std::to_string(byteCount) + "\n";
            return true;
        }
        catch (const std::exception& ex)
        {
            trace += "failed to extract embedded UDF template: ";
            trace += ex.what();
            trace += "\n";
            return false;
        }
    }

    void Cleanup()
    {
        if (!file_.empty())
        {
            std::error_code error;
            std::filesystem::remove(file_, error);
            if (error)
            {
                AppendDebugLog("failed to delete temp UDF template file " + file_.string() + ": " + error.message());
            }
        }
        if (!directory_.empty())
        {
            std::error_code error;
            std::filesystem::remove(directory_, error);
            if (error)
            {
                AppendDebugLog("failed to delete temp UDF template dir " + directory_.string() + ": " + error.message());
            }
        }
        file_.clear();
        directory_.clear();
    }

private:
    std::filesystem::path file_;
    std::filesystem::path directory_;
};

std::string ToLowerAscii(const std::string& value)
{
    std::string lowered = value;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return lowered;
}

std::string Utf8FromAcp(const std::string& value)
{
    if (value.empty())
    {
        return {};
    }

    const int wideSize = MultiByteToWideChar(
        CP_ACP,
        0,
        value.c_str(),
        static_cast<int>(value.size()),
        nullptr,
        0);
    if (wideSize <= 0)
    {
        return value;
    }

    std::wstring wide(static_cast<std::size_t>(wideSize), L'\0');
    MultiByteToWideChar(
        CP_ACP,
        0,
        value.c_str(),
        static_cast<int>(value.size()),
        wide.data(),
        wideSize);

    const int utf8Size = WideCharToMultiByte(
        CP_UTF8,
        0,
        wide.c_str(),
        static_cast<int>(wide.size()),
        nullptr,
        0,
        nullptr,
        nullptr);
    if (utf8Size <= 0)
    {
        return value;
    }

    std::string utf8(static_cast<std::size_t>(utf8Size), '\0');
    WideCharToMultiByte(
        CP_UTF8,
        0,
        wide.c_str(),
        static_cast<int>(wide.size()),
        utf8.data(),
        utf8Size,
        nullptr,
        nullptr);
    return utf8;
}

bool ContainsAny(const std::string& text, std::initializer_list<const char*> tokens)
{
    const std::string utf8Text = Utf8FromAcp(text);
    const std::string lowered = ToLowerAscii(text);
    const std::string loweredUtf8 = ToLowerAscii(utf8Text);
    for (const char* token : tokens)
    {
        const std::string loweredToken = ToLowerAscii(token);
        if (lowered.find(loweredToken) != std::string::npos ||
            loweredUtf8.find(loweredToken) != std::string::npos)
        {
            return true;
        }
    }
    return false;
}

void CopyPoint(const Point3d& point, double out[3])
{
    out[0] = point.X;
    out[1] = point.Y;
    out[2] = point.Z;
}

double Distance(const Point3d& a, const Point3d& b)
{
    const double dx = a.X - b.X;
    const double dy = a.Y - b.Y;
    const double dz = a.Z - b.Z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

Vector3d Subtract(const Point3d& a, const Point3d& b)
{
    return Vector3d(a.X - b.X, a.Y - b.Y, a.Z - b.Z);
}

double Dot(const Vector3d& a, const Vector3d& b)
{
    return a.X * b.X + a.Y * b.Y + a.Z * b.Z;
}

Vector3d Cross(const Vector3d& a, const Vector3d& b)
{
    return Vector3d(
        a.Y * b.Z - a.Z * b.Y,
        a.Z * b.X - a.X * b.Z,
        a.X * b.Y - a.Y * b.X);
}

double Length(const Vector3d& vector)
{
    return std::sqrt(Dot(vector, vector));
}

std::string FormatTriple(const double values[3])
{
    std::ostringstream stream;
    stream.setf(std::ios::fixed, std::ios::floatfield);
    stream.precision(6);
    stream << "(" << values[0] << "," << values[1] << "," << values[2] << ")";
    return stream.str();
}

std::string FormatVector(const Vector3d& vector)
{
    const double values[3] = {vector.X, vector.Y, vector.Z};
    return FormatTriple(values);
}

std::string FormatPoint(const Point3d& point)
{
    const double values[3] = {point.X, point.Y, point.Z};
    return FormatTriple(values);
}

bool Normalize(Vector3d& vector)
{
    const double length = Length(vector);
    if (length <= 1.0e-9)
    {
        return false;
    }

    vector.X /= length;
    vector.Y /= length;
    vector.Z /= length;
    return true;
}

bool EdgeTouchesPoint(Edge* edge, const Point3d& point)
{
    if (edge == nullptr)
    {
        return false;
    }

    try
    {
        Point3d first;
        Point3d second;
        edge->GetVertices(&first, &second);
        return Distance(first, point) <= kPointTolerance || Distance(second, point) <= kPointTolerance;
    }
    catch (...)
    {
    }

    return false;
}

bool PointHasThicknessLengthEdge(Body* body,
                                 const Point3d& point,
                                 double thickness,
                                 Edge* excludedFirst = nullptr,
                                 Edge* excludedSecond = nullptr,
                                 Edge** matchedEdge = nullptr)
{
    if (matchedEdge != nullptr)
    {
        *matchedEdge = nullptr;
    }
    if (body == nullptr || thickness <= kPointTolerance)
    {
        return false;
    }

    const double tolerance = std::max(kPlaneTolerance, thickness * 0.01);
    for (Edge* edge : body->GetEdges())
    {
        if (edge == nullptr || edge == excludedFirst || edge == excludedSecond ||
            !EdgeTouchesPoint(edge, point))
        {
            continue;
        }
        if (std::fabs(edge->GetLength() - thickness) <= tolerance)
        {
            if (matchedEdge != nullptr)
            {
                *matchedEdge = edge;
            }
            return true;
        }
    }
    return false;
}

bool EdgeDirectionAwayFromPoint(Edge* edge, const Point3d& point, Vector3d& direction)
{
    if (edge == nullptr)
    {
        return false;
    }

    try
    {
        Point3d first;
        Point3d second;
        edge->GetVertices(&first, &second);
        if (Distance(first, point) <= Distance(second, point))
        {
            direction = Subtract(second, first);
        }
        else
        {
            direction = Subtract(first, second);
        }
        return Normalize(direction);
    }
    catch (...)
    {
    }

    return false;
}

bool EdgeOtherPoint(Edge* edge, const Point3d& point, Point3d& otherPoint)
{
    if (edge == nullptr)
    {
        return false;
    }

    try
    {
        Point3d first;
        Point3d second;
        edge->GetVertices(&first, &second);
        if (Distance(first, point) <= Distance(second, point))
        {
            otherPoint = second;
        }
        else
        {
            otherPoint = first;
        }
        return true;
    }
    catch (...)
    {
    }

    return false;
}

bool EdgeNaturalStartEnd(Edge* edge, Point3d& start, Point3d& end)
{
    if (edge == nullptr)
    {
        return false;
    }

    try
    {
        edge->GetVertices(&start, &end);
        return true;
    }
    catch (...)
    {
    }

    return false;
}

UF_MODL_udf_reverse_dir_t EdgeDirectionWithStartPoint(Edge* edge, const Point3d& desiredStart, const char* label)
{
    Point3d naturalStart;
    Point3d naturalEnd;
    if (!EdgeNaturalStartEnd(edge, naturalStart, naturalEnd))
    {
        AppendDebugLog(std::string("edge direction ") + label + ": failed to read vertices, keeping UDF direction");
        return UF_MODL_UDF_KEEP_DIR;
    }

    const double startDistance = Distance(naturalStart, desiredStart);
    const double endDistance = Distance(naturalEnd, desiredStart);
    const bool reverse = endDistance < startDistance;
    std::ostringstream trace;
    trace << "edge direction " << label
          << ": naturalStart=(" << naturalStart.X << "," << naturalStart.Y << "," << naturalStart.Z << ")"
          << ", naturalEnd=(" << naturalEnd.X << "," << naturalEnd.Y << "," << naturalEnd.Z << ")"
          << ", desiredStart=(" << desiredStart.X << "," << desiredStart.Y << "," << desiredStart.Z << ")"
          << ", startDistance=" << startDistance
          << ", endDistance=" << endDistance
          << ", reverse=" << (reverse ? "true" : "false");
    AppendDebugLog(trace.str());
    return reverse ? UF_MODL_UDF_REVERSE_DIR : UF_MODL_UDF_KEEP_DIR;
}

UF_MODL_udf_reverse_dir_t EdgeDirectionWithEndPoint(Edge* edge, const Point3d& desiredEnd, const char* label)
{
    Point3d naturalStart;
    Point3d naturalEnd;
    if (!EdgeNaturalStartEnd(edge, naturalStart, naturalEnd))
    {
        AppendDebugLog(std::string("edge direction ") + label + ": failed to read vertices, keeping UDF direction");
        return UF_MODL_UDF_KEEP_DIR;
    }

    const double startDistance = Distance(naturalStart, desiredEnd);
    const double endDistance = Distance(naturalEnd, desiredEnd);
    const bool reverse = startDistance < endDistance;
    std::ostringstream trace;
    trace << "edge direction " << label
          << ": naturalStart=(" << naturalStart.X << "," << naturalStart.Y << "," << naturalStart.Z << ")"
          << ", naturalEnd=(" << naturalEnd.X << "," << naturalEnd.Y << "," << naturalEnd.Z << ")"
          << ", desiredEnd=(" << desiredEnd.X << "," << desiredEnd.Y << "," << desiredEnd.Z << ")"
          << ", startDistance=" << startDistance
          << ", endDistance=" << endDistance
          << ", reverse=" << (reverse ? "true" : "false");
    AppendDebugLog(trace.str());
    return reverse ? UF_MODL_UDF_REVERSE_DIR : UF_MODL_UDF_KEEP_DIR;
}

bool FaceNormalAtPoint(Face* face, const Point3d& pointOnFace, Vector3d& normal)
{
    if (face == nullptr || face->SolidFaceType() != Face::FaceTypePlanar)
    {
        return false;
    }

    int faceType = 0;
    double facePoint[3] = {0.0, 0.0, 0.0};
    double faceDirection[3] = {0.0, 0.0, 0.0};
    double faceBox[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    double faceRadius = 0.0;
    double faceRadData = 0.0;
    int normalDirection = 1;
    int result = UF_MODL_ask_face_data(face->Tag(),
                                       &faceType,
                                       facePoint,
                                       faceDirection,
                                       faceBox,
                                       &faceRadius,
                                       &faceRadData,
                                       &normalDirection);
    if (result != 0)
    {
        AppendDebugLog("FaceNormalAtPoint UF_MODL_ask_face_data failed: " + std::to_string(result) + " " + UfMessage(result));
        return false;
    }

    normal = Vector3d(faceDirection[0] * normalDirection,
                      faceDirection[1] * normalDirection,
                      faceDirection[2] * normalDirection);
    if (!Normalize(normal))
    {
        AppendDebugLog("FaceNormalAtPoint UF_MODL_ask_face_data returned zero normal.");
        return false;
    }

    double samplePoint[3] = {pointOnFace.X, pointOnFace.Y, pointOnFace.Z};
    double propsParam[2] = {0.0, 0.0};
    double propsFacePointFromParm[3] = {0.0, 0.0, 0.0};
    double propsPoint[3] = {0.0, 0.0, 0.0};
    double propsU1[3] = {0.0, 0.0, 0.0};
    double propsV1[3] = {0.0, 0.0, 0.0};
    double propsU2[3] = {0.0, 0.0, 0.0};
    double propsV2[3] = {0.0, 0.0, 0.0};
    double propsUnitNorm[3] = {0.0, 0.0, 0.0};
    double propsRadii[2] = {0.0, 0.0};
    int parmResult = UF_MODL_ask_face_parm_2(face->Tag(), samplePoint, propsParam, propsFacePointFromParm);
    int propsResult = -1;
    double propsDotData = 0.0;
    if (parmResult == 0)
    {
        propsResult = UF_MODL_ask_face_props(face->Tag(),
                                             propsParam,
                                             propsPoint,
                                             propsU1,
                                             propsV1,
                                             propsU2,
                                             propsV2,
                                             propsUnitNorm,
                                             propsRadii);
        if (propsResult == 0)
        {
            Vector3d propsVector(propsUnitNorm[0], propsUnitNorm[1], propsUnitNorm[2]);
            Normalize(propsVector);
            propsDotData = Dot(propsVector, normal);
        }
    }

    std::ostringstream trace;
    trace << "FaceNormalAtPoint UF faceData:"
          << " face=" << face->Tag()
          << ", type=" << faceType
          << ", samplePoint=" << FormatPoint(pointOnFace)
          << ", facePoint=" << FormatTriple(facePoint)
          << ", dir=" << FormatTriple(faceDirection)
          << ", norm_dir=" << normalDirection
          << ", dataOutward=" << FormatVector(normal)
          << ", propsParmResult=" << parmResult
          << ", propsResult=" << propsResult
          << ", propsParam=(" << propsParam[0] << "," << propsParam[1] << ")"
          << ", propsFacePoint=" << FormatTriple(propsFacePointFromParm)
          << ", propsPoint=" << FormatTriple(propsPoint)
          << ", propsUnitNorm=" << FormatTriple(propsUnitNorm)
          << ", dot(propsUnitNorm,dataOutward)=" << propsDotData;
    AppendDebugLog(trace.str());
    return true;
}

bool FaceNormal(Face* face, Vector3d& normal)
{
    if (face == nullptr || face->SolidFaceType() != Face::FaceTypePlanar)
    {
        return false;
    }

    std::vector<Edge*> edges = face->GetEdges();
    if (!edges.empty())
    {
        Point3d first;
        Point3d second;
        try
        {
            edges.front()->GetVertices(&first, &second);
            const Point3d midPoint(
                (first.X + second.X) * 0.5,
                (first.Y + second.Y) * 0.5,
                (first.Z + second.Z) * 0.5);
            if (FaceNormalAtPoint(face, midPoint, normal))
            {
                return true;
            }
        }
        catch (...)
        {
        }
    }

    for (std::size_t i = 0; i < edges.size(); ++i)
    {
        Point3d a1;
        Point3d a2;
        try
        {
            edges[i]->GetVertices(&a1, &a2);
        }
        catch (...)
        {
            continue;
        }

        Vector3d va = Subtract(a2, a1);
        if (!Normalize(va))
        {
            continue;
        }

        for (std::size_t j = i + 1; j < edges.size(); ++j)
        {
            Point3d b1;
            Point3d b2;
            try
            {
                edges[j]->GetVertices(&b1, &b2);
            }
            catch (...)
            {
                continue;
            }

            Vector3d vb = Subtract(b2, b1);
            if (!Normalize(vb))
            {
                continue;
            }

            normal = Cross(va, vb);
            if (Normalize(normal))
            {
                return true;
            }
        }
    }

    return false;
}

bool FacePlanePoint(Face* face, Point3d& point)
{
    if (face == nullptr || face->SolidFaceType() != Face::FaceTypePlanar)
    {
        return false;
    }

    int faceType = 0;
    double facePoint[3] = {0.0, 0.0, 0.0};
    double faceDirection[3] = {0.0, 0.0, 0.0};
    double faceBox[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    double faceRadius = 0.0;
    double faceRadData = 0.0;
    int normalDirection = 1;
    const int result = UF_MODL_ask_face_data(face->Tag(),
                                             &faceType,
                                             facePoint,
                                             faceDirection,
                                             faceBox,
                                             &faceRadius,
                                             &faceRadData,
                                             &normalDirection);
    if (result == 0)
    {
        point = Point3d(facePoint[0], facePoint[1], facePoint[2]);
        return true;
    }

    std::vector<Edge*> edges = face->GetEdges();
    if (edges.empty())
    {
        AppendDebugLog("FacePlanePoint failed: " + std::to_string(result) + " " + UfMessage(result));
        return false;
    }

    Point3d unused;
    try
    {
        edges.front()->GetVertices(&point, &unused);
        AppendDebugLog("FacePlanePoint used edge vertex fallback after UF failure: " +
                       std::to_string(result) + " " + UfMessage(result));
        return true;
    }
    catch (...)
    {
        AppendDebugLog("FacePlanePoint failed to read fallback edge vertex.");
        return false;
    }
}

bool FacePlaneData(Face* face, Point3d& point, Vector3d& normal)
{
    if (face == nullptr || face->SolidFaceType() != Face::FaceTypePlanar)
    {
        return false;
    }

    int faceType = 0;
    double facePoint[3] = {0.0, 0.0, 0.0};
    double faceDirection[3] = {0.0, 0.0, 0.0};
    double faceBox[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    double faceRadius = 0.0;
    double faceRadData = 0.0;
    int normalDirection = 1;
    const int result = UF_MODL_ask_face_data(face->Tag(),
                                             &faceType,
                                             facePoint,
                                             faceDirection,
                                             faceBox,
                                             &faceRadius,
                                             &faceRadData,
                                             &normalDirection);
    if (result != 0)
    {
        return false;
    }

    point = Point3d(facePoint[0], facePoint[1], facePoint[2]);
    normal = Vector3d(faceDirection[0] * normalDirection,
                      faceDirection[1] * normalDirection,
                      faceDirection[2] * normalDirection);
    return Normalize(normal);
}

bool AlmostSamePoint(const Point3d& first, const Point3d& second)
{
    return Distance(first, second) <= kPlaneTolerance;
}

bool FaceBoundaryPoints(Face* face, std::vector<Point3d>& points)
{
    points.clear();
    if (face == nullptr)
    {
        return false;
    }

    auto addUniquePoint = [&points](const Point3d& point) {
        for (const Point3d& existing : points)
        {
            if (AlmostSamePoint(existing, point))
            {
                return;
            }
        }
        points.push_back(point);
    };

    try
    {
        uf_loop_p_t loopList = nullptr;
        const int loopResult = UF_MODL_ask_face_loops(face->Tag(), &loopList);
        if (loopResult == 0 && loopList != nullptr)
        {
            int loopCount = 0;
            UF_MODL_ask_loop_list_count(loopList, &loopCount);
            for (int loopIndex = 0; loopIndex < loopCount; ++loopIndex)
            {
                int loopType = 0;
                uf_list_p_t edgeList = nullptr;
                if (UF_MODL_ask_loop_list_item(loopList, loopIndex, &loopType, &edgeList) != 0 ||
                    loopType != 1 ||
                    edgeList == nullptr)
                {
                    continue;
                }

                int edgeCount = 0;
                UF_MODL_ask_list_count(edgeList, &edgeCount);
                for (int edgeIndex = 0; edgeIndex < edgeCount; ++edgeIndex)
                {
                    tag_t edgeTag = NULL_TAG;
                    if (UF_MODL_ask_list_item(edgeList, edgeIndex, &edgeTag) != 0 || edgeTag == NULL_TAG)
                    {
                        continue;
                    }

                    Edge* edge = dynamic_cast<Edge*>(NXOpen::NXObjectManager::Get(edgeTag));
                    if (edge == nullptr)
                    {
                        continue;
                    }

                    Point3d first;
                    Point3d second;
                    edge->GetVertices(&first, &second);
                    addUniquePoint(first);
                    addUniquePoint(second);
                }
            }

            UF_MODL_delete_loop_list(&loopList);
            if (points.size() >= 2)
            {
                AppendDebugLog("FaceBoundaryPoints used peripheral loop points=" + std::to_string(points.size()) +
                               " face=" + std::to_string(face->Tag()));
                return true;
            }
        }

        if (loopList != nullptr)
        {
            UF_MODL_delete_loop_list(&loopList);
        }
        AppendDebugLog("FaceBoundaryPoints peripheral loop unavailable; falling back to all face edges. result=" +
                       std::to_string(loopResult));
    }
    catch (const NXException& ex)
    {
        AppendDebugLog("FaceBoundaryPoints peripheral loop NXException: " + UfMessage(ex.ErrorCode()));
        points.clear();
    }
    catch (...)
    {
        AppendDebugLog("FaceBoundaryPoints peripheral loop unknown exception.");
        points.clear();
    }

    try
    {
        for (Edge* edge : face->GetEdges())
        {
            if (edge == nullptr)
            {
                continue;
            }

            Point3d first;
            Point3d second;
            edge->GetVertices(&first, &second);
            addUniquePoint(first);
            addUniquePoint(second);
        }
    }
    catch (...)
    {
        return false;
    }

    AppendDebugLog("FaceBoundaryPoints used fallback edge points=" + std::to_string(points.size()) +
                   " face=" + std::to_string(face->Tag()));
    return points.size() >= 2;
}

bool FaceHasEdge(Face* face, Edge* edge)
{
    if (face == nullptr || edge == nullptr)
    {
        return false;
    }

    try
    {
        const tag_t edgeTag = edge->Tag();
        for (Edge* faceEdge : face->GetEdges())
        {
            if (faceEdge != nullptr && faceEdge->Tag() == edgeTag)
            {
                return true;
            }
        }
    }
    catch (...)
    {
    }

    return false;
}

double AngleDegrees(const Vector3d& first, const Vector3d& second)
{
    Vector3d firstDirection = first;
    Vector3d secondDirection = second;
    if (!Normalize(firstDirection) || !Normalize(secondDirection))
    {
        return -1.0;
    }

    const double cosine = std::max(-1.0, std::min(1.0, Dot(firstDirection, secondDirection)));
    return std::acos(cosine) * 180.0 / 3.14159265358979323846;
}

bool PointContainmentOnFace(Face* face, const Point3d& point, int& status)
{
    status = 0;
    if (face == nullptr)
    {
        return false;
    }

    double coordinates[3] = {point.X, point.Y, point.Z};
    return UF_MODL_ask_point_containment(coordinates, face->Tag(), &status) == 0;
}

bool CornerExtensionPointsOnBody(Body* body,
                                 Edge* firstEdge,
                                 Edge* secondEdge,
                                 const Point3d& corner,
                                 Point3d& firstExtension,
                                 Point3d& secondExtension,
                                 int& firstStatus,
                                 int& secondStatus,
                                 bool& bothOnOrInBody)
{
    firstStatus = 0;
    secondStatus = 0;
    bothOnOrInBody = false;
    if (body == nullptr || firstEdge == nullptr || secondEdge == nullptr)
    {
        return false;
    }

    Vector3d firstDirection;
    Vector3d secondDirection;
    if (!EdgeDirectionAwayFromPoint(firstEdge, corner, firstDirection) ||
        !EdgeDirectionAwayFromPoint(secondEdge, corner, secondDirection))
    {
        return false;
    }

    firstExtension = Point3d(corner.X - firstDirection.X * kCornerExtensionDistance,
                             corner.Y - firstDirection.Y * kCornerExtensionDistance,
                             corner.Z - firstDirection.Z * kCornerExtensionDistance);
    secondExtension = Point3d(corner.X - secondDirection.X * kCornerExtensionDistance,
                              corner.Y - secondDirection.Y * kCornerExtensionDistance,
                              corner.Z - secondDirection.Z * kCornerExtensionDistance);
    double firstCoordinates[3] = {firstExtension.X, firstExtension.Y, firstExtension.Z};
    double secondCoordinates[3] = {secondExtension.X, secondExtension.Y, secondExtension.Z};
    const int firstResult =
        UF_MODL_ask_point_containment(firstCoordinates, body->Tag(), &firstStatus);
    const int secondResult =
        UF_MODL_ask_point_containment(secondCoordinates, body->Tag(), &secondStatus);
    if (firstResult != 0 || secondResult != 0)
    {
        return false;
    }

    const bool firstOnOrInBody = firstStatus == 1 || firstStatus == 3;
    const bool secondOnOrInBody = secondStatus == 1 || secondStatus == 3;
    bothOnOrInBody = firstOnOrInBody && secondOnOrInBody;
    return true;
}

bool FaceInteriorCornerAngle(Face* face, const Point3d& corner, double& interiorAngle)
{
    interiorAngle = 0.0;
    if (face == nullptr)
    {
        return false;
    }

    struct ConnectedDirection
    {
        Vector3d direction;
        double edgeLength = 0.0;
        tag_t edgeTag = NULL_TAG;
    };

    std::vector<ConnectedDirection> connectedDirections;
    try
    {
        for (Edge* edge : face->GetEdges())
        {
            if (edge == nullptr || !EdgeTouchesPoint(edge, corner))
            {
                continue;
            }

            Vector3d direction;
            if (!EdgeDirectionAwayFromPoint(edge, corner, direction))
            {
                continue;
            }

            bool duplicate = false;
            for (const ConnectedDirection& existing : connectedDirections)
            {
                if (std::fabs(Dot(existing.direction, direction)) > 1.0 - 1.0e-6)
                {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate)
            {
                connectedDirections.push_back({direction, edge->GetLength(), edge->Tag()});
            }
        }
    }
    catch (...)
    {
        return false;
    }

    if (connectedDirections.size() != 2)
    {
        AppendDebugLog("FaceInteriorCornerAngle requires exactly two face edges at corner=" +
                       FormatPoint(corner) + ", actual=" + std::to_string(connectedDirections.size()));
        return false;
    }

    const Vector3d& first = connectedDirections[0].direction;
    const Vector3d& second = connectedDirections[1].direction;
    const double smallerAngle = AngleDegrees(first, second);
    if (smallerAngle < 0.0)
    {
        return false;
    }

    // Each direction points from Q toward the far endpoint of its edge.  Move
    // 0.05 mm from Q in the opposite direction, which is the continuation of
    // the far-end-to-Q line beyond Q.  At a reflex corner both continuation
    // points lie in the trimmed face region; at a convex corner they do not.
    const Point3d firstExtension(corner.X - first.X * kCornerExtensionDistance,
                                 corner.Y - first.Y * kCornerExtensionDistance,
                                 corner.Z - first.Z * kCornerExtensionDistance);
    const Point3d secondExtension(corner.X - second.X * kCornerExtensionDistance,
                                  corner.Y - second.Y * kCornerExtensionDistance,
                                  corner.Z - second.Z * kCornerExtensionDistance);
    int firstExtensionStatus = 0;
    int secondExtensionStatus = 0;
    const bool firstExtensionInside =
        PointContainmentOnFace(face, firstExtension, firstExtensionStatus) &&
        firstExtensionStatus == 1;
    const bool secondExtensionInside =
        PointContainmentOnFace(face, secondExtension, secondExtensionStatus) &&
        secondExtensionStatus == 1;
    const bool reflexCorner = firstExtensionInside && secondExtensionInside;
    interiorAngle = reflexCorner ? 360.0 - smallerAngle : smallerAngle;
    std::ostringstream trace;
    trace << "FaceInteriorCornerAngle corner=" << FormatPoint(corner)
          << ", edges=(" << connectedDirections[0].edgeTag << ","
          << connectedDirections[1].edgeTag << ")"
          << ", smallerAngle=" << smallerAngle
          << ", extensionDistance=" << kCornerExtensionDistance
          << ", firstExtension=" << FormatPoint(firstExtension)
          << ", firstStatus=" << firstExtensionStatus
          << ", firstInside=" << (firstExtensionInside ? "true" : "false")
          << ", secondExtension=" << FormatPoint(secondExtension)
          << ", secondStatus=" << secondExtensionStatus
          << ", secondInside=" << (secondExtensionInside ? "true" : "false")
          << ", reflex=" << (reflexCorner ? "true" : "false")
          << ", interiorAngle=" << interiorAngle;
    AppendDebugLog(trace.str());
    return true;
}

ProjectionPoint2d ProjectToPlane2d(const Point3d& point,
                         const Point3d& origin,
                         const Vector3d& xAxis,
                         const Vector3d& yAxis)
{
    const Vector3d vector = Subtract(point, origin);
    return {Dot(vector, xAxis), Dot(vector, yAxis)};
}

double SignedPolygonArea(const std::vector<ProjectionPoint2d>& polygon)
{
    if (polygon.size() < 3)
    {
        return 0.0;
    }

    double area = 0.0;
    for (std::size_t i = 0; i < polygon.size(); ++i)
    {
        const ProjectionPoint2d& a = polygon[i];
        const ProjectionPoint2d& b = polygon[(i + 1) % polygon.size()];
        area += a.x * b.y - b.x * a.y;
    }
    return area * 0.5;
}

double PolygonArea(const std::vector<ProjectionPoint2d>& polygon)
{
    return std::fabs(SignedPolygonArea(polygon));
}

std::vector<ProjectionPoint2d> SortPolygonByAngle(std::vector<ProjectionPoint2d> polygon)
{
    if (polygon.size() < 3)
    {
        return polygon;
    }

    ProjectionPoint2d centroid;
    for (const ProjectionPoint2d& point : polygon)
    {
        centroid.x += point.x;
        centroid.y += point.y;
    }
    centroid.x /= static_cast<double>(polygon.size());
    centroid.y /= static_cast<double>(polygon.size());

    std::sort(polygon.begin(), polygon.end(), [centroid](const ProjectionPoint2d& first, const ProjectionPoint2d& second) {
        const double firstAngle = std::atan2(first.y - centroid.y, first.x - centroid.x);
        const double secondAngle = std::atan2(second.y - centroid.y, second.x - centroid.x);
        return firstAngle < secondAngle;
    });

    if (SignedPolygonArea(polygon) < 0.0)
    {
        std::reverse(polygon.begin(), polygon.end());
    }
    return polygon;
}

double Cross2d(const ProjectionPoint2d& origin, const ProjectionPoint2d& first, const ProjectionPoint2d& second)
{
    return (first.x - origin.x) * (second.y - origin.y) -
           (first.y - origin.y) * (second.x - origin.x);
}

ProjectionPoint2d IntersectLines2d(const ProjectionPoint2d& firstStart,
                         const ProjectionPoint2d& firstEnd,
                         const ProjectionPoint2d& secondStart,
                         const ProjectionPoint2d& secondEnd)
{
    const double x1 = firstStart.x;
    const double y1 = firstStart.y;
    const double x2 = firstEnd.x;
    const double y2 = firstEnd.y;
    const double x3 = secondStart.x;
    const double y3 = secondStart.y;
    const double x4 = secondEnd.x;
    const double y4 = secondEnd.y;
    const double denominator = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
    if (std::fabs(denominator) <= 1.0e-9)
    {
        return firstEnd;
    }

    const double firstDet = x1 * y2 - y1 * x2;
    const double secondDet = x3 * y4 - y3 * x4;
    return {
        (firstDet * (x3 - x4) - (x1 - x2) * secondDet) / denominator,
        (firstDet * (y3 - y4) - (y1 - y2) * secondDet) / denominator};
}

std::vector<ProjectionPoint2d> ClipPolygonByConvexPolygon(std::vector<ProjectionPoint2d> subject,
                                                const std::vector<ProjectionPoint2d>& clip)
{
    if (subject.size() < 3 || clip.size() < 3)
    {
        return {};
    }

    std::vector<ProjectionPoint2d> output = std::move(subject);
    for (std::size_t i = 0; i < clip.size(); ++i)
    {
        const ProjectionPoint2d clipStart = clip[i];
        const ProjectionPoint2d clipEnd = clip[(i + 1) % clip.size()];
        const std::vector<ProjectionPoint2d> input = output;
        output.clear();
        if (input.empty())
        {
            break;
        }

        auto inside = [clipStart, clipEnd](const ProjectionPoint2d& point) {
            return Cross2d(clipStart, clipEnd, point) >= -1.0e-6;
        };

        ProjectionPoint2d previous = input.back();
        bool previousInside = inside(previous);
        for (const ProjectionPoint2d& current : input)
        {
            const bool currentInside = inside(current);
            if (currentInside)
            {
                if (!previousInside)
                {
                    output.push_back(IntersectLines2d(previous, current, clipStart, clipEnd));
                }
                output.push_back(current);
            }
            else if (previousInside)
            {
                output.push_back(IntersectLines2d(previous, current, clipStart, clipEnd));
            }

            previous = current;
            previousInside = currentInside;
        }
    }

    return output;
}

bool FaceProjectionPolygon(Face* face,
                           const Point3d& origin,
                           const Vector3d& xAxis,
                           const Vector3d& yAxis,
                           std::vector<ProjectionPoint2d>& polygon)
{
    std::vector<Point3d> boundaryPoints;
    if (!FaceBoundaryPoints(face, boundaryPoints))
    {
        return false;
    }

    polygon.clear();
    for (const Point3d& point : boundaryPoints)
    {
        polygon.push_back(ProjectToPlane2d(point, origin, xAxis, yAxis));
    }
    polygon = SortPolygonByAngle(std::move(polygon));
    return PolygonArea(polygon) > kPointTolerance;
}

bool FaceProjectionAxes(Face* face, const Vector3d& normal, Vector3d& xAxis, Vector3d& yAxis)
{
    std::vector<Edge*> edges = face != nullptr ? face->GetEdges() : std::vector<Edge*>();
    for (Edge* edge : edges)
    {
        if (edge == nullptr)
        {
            continue;
        }

        Point3d first;
        Point3d second;
        try
        {
            edge->GetVertices(&first, &second);
        }
        catch (...)
        {
            continue;
        }

        xAxis = Subtract(second, first);
        const double normalComponent = Dot(xAxis, normal);
        xAxis.X -= normal.X * normalComponent;
        xAxis.Y -= normal.Y * normalComponent;
        xAxis.Z -= normal.Z * normalComponent;
        if (Normalize(xAxis))
        {
            yAxis = Cross(normal, xAxis);
            return Normalize(yAxis);
        }
    }

    Vector3d seed(std::fabs(normal.X) < 0.9 ? 1.0 : 0.0,
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

double ProjectedOverlapRatio(Face* baseFace,
                             Face* candidateFace,
                             const Point3d& origin,
                             const Vector3d& xAxis,
                             const Vector3d& yAxis,
                             double& overlapArea,
                             double& baseProjectedArea,
                             double& candidateProjectedArea)
{
    overlapArea = 0.0;
    baseProjectedArea = 0.0;
    candidateProjectedArea = 0.0;

    std::vector<ProjectionPoint2d> basePolygon;
    std::vector<ProjectionPoint2d> candidatePolygon;
    if (!FaceProjectionPolygon(baseFace, origin, xAxis, yAxis, basePolygon) ||
        !FaceProjectionPolygon(candidateFace, origin, xAxis, yAxis, candidatePolygon))
    {
        return 0.0;
    }

    baseProjectedArea = PolygonArea(basePolygon);
    candidateProjectedArea = PolygonArea(candidatePolygon);
    if (baseProjectedArea <= kPointTolerance || candidateProjectedArea <= kPointTolerance)
    {
        return 0.0;
    }

    const std::vector<ProjectionPoint2d> intersection = ClipPolygonByConvexPolygon(candidatePolygon, basePolygon);
    overlapArea = PolygonArea(intersection);
    return overlapArea / baseProjectedArea;
}

double ProjectedOverlapRatioWithBase(const std::vector<ProjectionPoint2d>& basePolygon,
                                     double baseProjectedArea,
                                     Face* candidateFace,
                                     const Point3d& origin,
                                     const Vector3d& xAxis,
                                     const Vector3d& yAxis,
                                     double& overlapArea,
                                     double& candidateProjectedArea)
{
    overlapArea = 0.0;
    candidateProjectedArea = 0.0;
    if (basePolygon.size() < 3 || baseProjectedArea <= kPointTolerance)
    {
        return 0.0;
    }

    std::vector<ProjectionPoint2d> candidatePolygon;
    if (!FaceProjectionPolygon(candidateFace, origin, xAxis, yAxis, candidatePolygon))
    {
        return 0.0;
    }

    candidateProjectedArea = PolygonArea(candidatePolygon);
    if (candidateProjectedArea <= kPointTolerance)
    {
        return 0.0;
    }

    const std::vector<ProjectionPoint2d> intersection = ClipPolygonByConvexPolygon(candidatePolygon, basePolygon);
    overlapArea = PolygonArea(intersection);
    return overlapArea / baseProjectedArea;
}

bool PointOnFacePlane(Face* face, const Point3d& point)
{
    if (face == nullptr)
    {
        return false;
    }

    std::vector<Edge*> edges = face->GetEdges();
    if (edges.empty())
    {
        return false;
    }

    Point3d origin;
    Point3d unused;
    try
    {
        edges.front()->GetVertices(&origin, &unused);
    }
    catch (...)
    {
        return false;
    }

    Vector3d normal;
    if (!FaceNormal(face, normal))
    {
        return false;
    }

    return std::fabs(Dot(Subtract(point, origin), normal)) <= kPlaneTolerance;
}

Features::CustomTagAttribute* CreateTagAttribute(Features::CustomAttributeCollection* attrs,
                                                 const char* name,
                                                 TaggedObject* value,
                                                 bool mandatory,
                                                 bool targetBody = false)
{
    std::vector<Features::CustomAttribute::Property> props;
    if (mandatory)
    {
        props.push_back(Features::CustomAttribute::PropertyMandatoryInput);
    }
    if (targetBody)
    {
        props.push_back(Features::CustomAttribute::PropertyIsReferencingTargetBody);
    }

    Features::CustomTagAttribute* attr = attrs->CreateCustomTagAttribute(name, props);
    attr->SetValue(value);
    return attr;
}

Features::CustomDoubleAttribute* CreateDoubleAttribute(Features::CustomAttributeCollection* attrs,
                                                       const char* name,
                                                       double value)
{
    std::vector<Features::CustomAttribute::Property> props;
    Features::CustomDoubleAttribute* attr = attrs->CreateCustomDoubleAttribute(name, props);
    attr->SetValue(value);
    return attr;
}

class WorkPartContextGuard
{
public:
    explicit WorkPartContextGuard(tag_t newWorkPart)
        : context_(nullptr),
          active_(false)
    {
        if (newWorkPart != NULL_TAG)
        {
            active_ = UF_ASSEM_set_work_part_context_quietly(newWorkPart, &context_) == 0;
        }
    }

    ~WorkPartContextGuard()
    {
        if (active_)
        {
            UF_ASSEM_restore_work_part_context_quietly(&context_);
        }
    }

    bool IsActive() const
    {
        return active_;
    }

private:
    UF_ASSEM_work_part_context_p_t context_;
    bool active_;
};

std::string FindTemplatePath()
{
    std::vector<std::string> candidates;
    const std::string moduleDir = CurrentModuleDir();
    if (!moduleDir.empty())
    {
        candidates.push_back(moduleDir + "\\" + kTemplatePartName);
    }
    candidates.push_back(DeployedApplicationDir() + "\\" + kTemplatePartName);

    char* ugiiUserDir = nullptr;
    if (UF_translate_variable("UGII_USER_DIR", &ugiiUserDir) == 0 && ugiiUserDir != nullptr)
    {
        candidates.push_back(std::string(ugiiUserDir) + "\\application\\" + kTemplatePartName);
        candidates.push_back(std::string(ugiiUserDir) + "\\DATA\\" + kTemplatePartName);
        UF_free(ugiiUserDir);
    }

    candidates.push_back(kTemplatePartName);

    std::string trace = "template candidates:";
    for (const std::string& candidate : candidates)
    {
        trace += "\n  " + candidate;
        std::ifstream file(candidate, std::ios::binary);
        if (file.good())
        {
            trace += " [selected]";
            AppendDebugLog(trace);
            return candidate;
        }
    }

    AppendDebugLog(trace + "\n  no candidate existed; using fallback name");
    return kTemplatePartName;
}

std::string BaseFileName(const std::string& path)
{
    const std::size_t slash = path.find_last_of("\\/");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string NormalizePathForCompare(const std::string& path)
{
    try
    {
        return ToLowerAscii(std::filesystem::absolute(std::filesystem::path(path)).lexically_normal().string());
    }
    catch (...)
    {
        return ToLowerAscii(path);
    }
}

tag_t FindLoadedTemplatePart(const std::string& templatePath, std::string& trace)
{
    const std::string targetPath = NormalizePathForCompare(templatePath);
    const int partCount = UF_PART_ask_num_parts();
    for (int index = 0; index < partCount; ++index)
    {
        const tag_t part = UF_PART_ask_nth_part(index);
        if (part == NULL_TAG)
        {
            continue;
        }

        char partName[MAX_FSPEC_BUFSIZE] = {0};
        if (UF_PART_ask_part_name(part, partName) != 0)
        {
            continue;
        }

        const std::string loadedPath = partName;
        trace += "loaded part tag=" + std::to_string(part) + ", path=" + loadedPath + "\n";
        if (NormalizePathForCompare(loadedPath) == targetPath)
        {
            return part;
        }
    }

    return NULL_TAG;
}

bool CopyTemplateToTemp(const std::string& templatePath, std::string& tempPath, std::string& trace)
{
    try
    {
        const auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        const std::filesystem::path tempDir = std::filesystem::temp_directory_path();
        const std::filesystem::path target =
            tempDir / ("2p_SiBian_udf_" + std::to_string(now) + ".prt");
        std::filesystem::copy_file(
            std::filesystem::path(templatePath),
            target,
            std::filesystem::copy_options::overwrite_existing);
        tempPath = target.string();
        trace += "copied template to temp=" + tempPath + "\n";
        return true;
    }
    catch (const std::exception& ex)
    {
        trace += "failed to copy template to temp: ";
        trace += ex.what();
        trace += "\n";
        return false;
    }
}

void* AllocateUfMemory(std::size_t byteCount, const char* label)
{
    if (byteCount == 0 || byteCount > static_cast<std::size_t>(std::numeric_limits<unsigned int>::max()))
    {
        AppendDebugLog(std::string("UF_allocate_memory skipped for ") + label +
                       ", invalid bytes=" + std::to_string(byteCount));
        return nullptr;
    }

    int errorCode = 0;
    void* memory = UF_allocate_memory(static_cast<unsigned int>(byteCount), &errorCode);
    std::ostringstream trace;
    trace << "UF_allocate_memory " << label
          << " bytes=" << byteCount
          << " error=" << errorCode << " " << UfMessage(errorCode)
          << " ptr=" << memory;
    AppendDebugLog(trace.str());
    return memory;
}

std::string ExpressionRightHandSide(tag_t expression)
{
    if (expression == NULL_TAG)
    {
        return {};
    }

    char* expString = nullptr;
    if (UF_MODL_ask_exp_tag_string(expression, &expString) != 0 || expString == nullptr)
    {
        if (expString != nullptr)
        {
            UF_free(expString);
        }
        return {};
    }

    std::string text = expString;
    UF_free(expString);

    const std::size_t equals = text.find('=');
    if (equals != std::string::npos)
    {
        text = text.substr(equals + 1);
    }
    return TrimAscii(text);
}

std::string ExpressionLeftHandSide(tag_t expression)
{
    if (expression == NULL_TAG)
    {
        return {};
    }

    char* expString = nullptr;
    if (UF_MODL_ask_exp_tag_string(expression, &expString) != 0 || expString == nullptr)
    {
        if (expString != nullptr)
        {
            UF_free(expString);
        }
        return {};
    }

    std::string text = expString;
    UF_free(expString);

    const std::size_t equals = text.find('=');
    if (equals != std::string::npos)
    {
        text = text.substr(0, equals);
    }
    return TrimAscii(text);
}

std::string FormatExpressionNumber(double value)
{
    std::ostringstream stream;
    stream.setf(std::ios::fixed, std::ios::floatfield);
    stream.precision(6);
    stream << value;
    std::string text = stream.str();
    while (text.size() > 1 && text.back() == '0')
    {
        text.pop_back();
    }
    if (!text.empty() && text.back() == '.')
    {
        text.pop_back();
    }
    return text.empty() ? "0" : text;
}

bool TryParseExpressionNumber(const std::string& text, double& value)
{
    const std::string trimmed = TrimAscii(text);
    if (trimmed.empty())
    {
        return false;
    }

    char* end = nullptr;
    value = std::strtod(trimmed.c_str(), &end);
    return end != trimmed.c_str();
}

bool AllocateUdfExpressionValues(UF_MODL_udf_exp_data_t& expData,
                                 TwoPointSiBianUI::FeatureMode featureMode,
                                 const std::string& clearanceValue,
                                 const std::string& bendRadiusValue,
                                 double sheetThickness,
                                 std::string& errorMessage)
{
    if (expData.num_exps <= 0)
    {
        return true;
    }

    expData.new_exp_values = static_cast<char**>(
        AllocateUfMemory(sizeof(char*) * static_cast<std::size_t>(expData.num_exps),
                         "expData.new_exp_values"));
    if (expData.new_exp_values == nullptr)
    {
        errorMessage = "Failed to allocate UDF expression value data.";
        return false;
    }
    std::memset(expData.new_exp_values, 0, sizeof(char*) * expData.num_exps);

    const std::string sheetThicknessValue = FormatExpressionNumber(sheetThickness);
    AppendDebugLog("expression inputs: sheetThickness=" + sheetThicknessValue +
                   ", bendRadius=" + bendRadiusValue +
                   ", clearance=" + clearanceValue);

    for (int index = 0; index < expData.num_exps; ++index)
    {
        std::string value;
        const tag_t oldExpression = expData.old_exps != nullptr ? expData.old_exps[index] : NULL_TAG;
        const std::string expressionName = ExpressionLeftHandSide(oldExpression);
        const std::string expressionKey = ToLowerAscii(TrimAscii(expressionName));
        if (expressionKey == "p7")
        {
            value = sheetThicknessValue;
        }
        else if (expressionKey == "p8")
        {
            value = bendRadiusValue;
        }
        else if (expressionKey == "p9")
        {
            value = clearanceValue;
        }
        else
        if (ContainsAny(expressionName, {"鏉垮帤", "banhou", "thickness", "sheet"}))
        {
            value = sheetThicknessValue;
        }
        else if (ContainsAny(expressionName, {"鎶樺集r", "鎶樺集R", "bend", "radius", "r"}))
        {
            value = bendRadiusValue;
        }
        else if (ContainsAny(expressionName, {"闂撮殭", "gap", "clearance"}))
        {
            value = clearanceValue;
        }
        else if (featureMode == TwoPointSiBianUI::FeatureMode::Chamfer)
        {
            if (index == 0)
            {
                value = sheetThicknessValue;
            }
            else if (index == 1)
            {
                value = bendRadiusValue;
            }
            else if (index == 2)
            {
                value = clearanceValue;
            }
        }
        else if (index == 0)
        {
            value = clearanceValue;
        }
        else if (index == 1)
        {
            value = bendRadiusValue;
        }
        else
        {
            value = ExpressionRightHandSide(oldExpression);
            if (value.empty())
            {
                value = bendRadiusValue;
            }
        }
        AppendDebugLog("expression assign index=" + std::to_string(index) +
                       ", name=\"" + expressionName + "\", value=" + value);

        value = TrimAscii(value);
        if (value.empty())
        {
            value = "0.2";
        }

        char* stored = static_cast<char*>(
            AllocateUfMemory(value.size() + 1, "expData.new_exp_values[]"));
        if (stored == nullptr)
        {
            errorMessage = "Failed to allocate one UDF expression value.";
            return false;
        }

        std::memcpy(stored, value.c_str(), value.size() + 1);
        expData.new_exp_values[index] = stored;
    }

    return true;
}

int ObjectType(tag_t object)
{
    int type = 0;
    int subtype = 0;
    if (object != NULL_TAG && UF_OBJ_ask_type_and_subtype(object, &type, &subtype) == 0)
    {
        return type;
    }
    return 0;
}

bool IsPointLike(tag_t object)
{
    return ObjectType(object) == UF_point_type;
}

bool IsFaceLike(tag_t object)
{
    int type = 0;
    int subtype = 0;
    return object != NULL_TAG &&
           UF_OBJ_ask_type_and_subtype(object, &type, &subtype) == 0 &&
           type == UF_solid_type &&
           subtype == UF_solid_face_subtype;
}

bool IsEdgeLike(tag_t object)
{
    int type = 0;
    int subtype = 0;
    return object != NULL_TAG &&
           UF_OBJ_ask_type_and_subtype(object, &type, &subtype) == 0 &&
           type == UF_solid_type &&
           subtype == UF_solid_edge_subtype;
}

bool IsCurveLike(tag_t object)
{
    const int type = ObjectType(object);
    return type == UF_line_type ||
           type == UF_circle_type ||
           type == UF_conic_type ||
           type == UF_spline_type ||
           type == UF_old_spline_type;
}

bool IsBodyLike(tag_t object)
{
    int type = 0;
    int subtype = 0;
    return object != NULL_TAG &&
           UF_OBJ_ask_type_and_subtype(object, &type, &subtype) == 0 &&
           type == UF_solid_type &&
           subtype == UF_solid_body_subtype;
}

tag_t MatchReference(const std::string& prompt,
                     tag_t oldRef,
                     tag_t startPoint,
                     tag_t endPoint,
                     tag_t targetBody,
                     tag_t baseFace,
                     tag_t startPositiveYEdge,
                     tag_t startNegativeYEdge,
                     tag_t endPositiveYEdge,
                     tag_t endNegativeYEdge,
                     tag_t startEdge,
                     tag_t endEdge,
                     int& pointOrdinal,
                     int& edgeOrdinal,
                     int& sketchExternalOrdinal)
{
    (void)startPositiveYEdge;
    (void)startNegativeYEdge;
    (void)endPositiveYEdge;
    (void)endNegativeYEdge;
    (void)sketchExternalOrdinal;

    if (ContainsAny(prompt, {"start", "first", "p1", "point1", "begin", "qi", "beginpoint"}))
    {
        return startPoint;
    }
    if (ContainsAny(prompt, {"end", "second", "p2", "point2", "finish", "zhong", "endpoint"}))
    {
        return endPoint;
    }
    if (ContainsAny(prompt, {"body", "target", "sheet", "solid"}))
    {
        return targetBody;
    }
    if (ContainsAny(prompt, {"face", "plane", "surface", "mian"}))
    {
        return baseFace;
    }
    if (ContainsAny(prompt, {"edge", "bian"}))
    {
        return edgeOrdinal++ == 0 ? startEdge : (endEdge != NULL_TAG ? endEdge : startEdge);
    }

    if (IsPointLike(oldRef))
    {
        return pointOrdinal++ == 0 ? startPoint : endPoint;
    }
    if (IsBodyLike(oldRef))
    {
        return targetBody;
    }
    if (IsFaceLike(oldRef))
    {
        return baseFace;
    }
    if (IsEdgeLike(oldRef) || IsCurveLike(oldRef))
    {
        return edgeOrdinal++ == 0 ? startEdge : (endEdge != NULL_TAG ? endEdge : startEdge);
    }

    return NULL_TAG;
}
tag_t FindUdfDefinitionFeature(tag_t partTag, std::string& trace)
{
    tag_t object = NULL_TAG;
    while (UF_OBJ_cycle_objs_in_part(partTag, UF_feature_type, &object) == 0 && object != NULL_TAG)
    {
        char* featureType = nullptr;
        char* sysName = nullptr;
        UF_MODL_ask_feat_type(object, &featureType);
        UF_MODL_ask_feat_or_udf_sysname(object, &sysName);

        std::string typeText = featureType != nullptr ? featureType : "";
        std::string nameText = sysName != nullptr ? sysName : "";
        trace += "feature tag=" + std::to_string(object) + ", type=" + typeText + ", name=" + nameText + "\n";

        if (featureType != nullptr)
        {
            UF_free(featureType);
        }
        if (sysName != nullptr)
        {
            UF_free(sysName);
        }

        if (typeText != "UDF_DEF")
        {
            continue;
        }

        tag_t* parents = nullptr;
        char** parentPrompts = nullptr;
        int parentCount = 0;
        tag_t* expressions = nullptr;
        char** expressionPrompts = nullptr;
        int expressionCount = 0;
        const int askResult = UF_MODL_ask_udf_definition(
            object,
            &parents,
            &parentPrompts,
            &parentCount,
            &expressions,
            &expressionPrompts,
            &expressionCount);

        if (parents != nullptr)
        {
            UF_free(parents);
        }
        if (parentPrompts != nullptr)
        {
            UF_free_string_array(parentCount, parentPrompts);
        }
        if (expressions != nullptr)
        {
            UF_free(expressions);
        }
        if (expressionPrompts != nullptr)
        {
            UF_free_string_array(expressionCount, expressionPrompts);
        }

        if (askResult == 0)
        {
            return object;
        }
    }

    return NULL_TAG;
}

std::string DescribeRefs(const UF_MODL_udf_ref_data_t& refData)
{
    std::ostringstream trace;
    trace << "UDF refs=" << refData.num_refs;
    for (int index = 0; index < refData.num_refs; ++index)
    {
        const tag_t oldRef = refData.old_refs != nullptr ? refData.old_refs[index] : NULL_TAG;
        int type = 0;
        int subtype = 0;
        if (oldRef != NULL_TAG)
        {
            UF_OBJ_ask_type_and_subtype(oldRef, &type, &subtype);
        }
        trace << "\n  ref[" << index << "] old=" << oldRef << " type=" << type << " subtype=" << subtype;
        if (refData.mapping_data != nullptr)
        {
            const UF_MODL_udf_mapping_data_t& mapping = refData.mapping_data[index];
            trace << " mappingCount=" << mapping.num_mapping_objs
                  << " reverseObjsDir=" << static_cast<const void*>(mapping.reverse_objs_dir);
            if (mapping.num_mapping_objs > 0)
            {
                trace << " oldOutputObjs="
                      << static_cast<const void*>(mapping.defined_by.output_objs_non_ss.old_output_objs)
                      << " oldOutputObjIndex="
                      << static_cast<const void*>(mapping.defined_by.output_objs_ss.old_output_objs_index);
            }
        }
    }
    return trace.str();
}

std::string DescribeExpressions(const UF_MODL_udf_exp_data_t& expData)
{
    std::ostringstream trace;
    trace << "UDF expressions=" << expData.num_exps;
    for (int index = 0; index < expData.num_exps; ++index)
    {
        const tag_t oldExp = expData.old_exps != nullptr ? expData.old_exps[index] : NULL_TAG;
        trace << "\n  exp[" << index << "] old=" << oldExp;

        if (oldExp != NULL_TAG)
        {
            char* expString = nullptr;
            const int result = UF_MODL_ask_exp_tag_string(oldExp, &expString);
            trace << " stringResult=" << result << " " << UfMessage(result);
            if (result == 0 && expString != nullptr)
            {
                trace << " value=\"" << expString << "\"";
            }
            if (expString != nullptr)
            {
                UF_free(expString);
            }
        }

        if (expData.new_exp_values != nullptr && expData.new_exp_values[index] != nullptr)
        {
            trace << " new=\"" << expData.new_exp_values[index] << "\"";
        }
    }
    return trace.str();
}
}

TwoPointSiBianUI::TwoPointSiBianUI()
    : session_(Session::GetSession()),
      ui_(UI::GetUI()),
      dialog_(nullptr),
      startPointBlock_(nullptr),
      endPointBlock_(nullptr),
      clearanceBlock_(nullptr),
      bendRadiusBlock_(nullptr),
      featureModeBlock_(nullptr),
      customFeatureManager_(nullptr),
      editedFeature_(nullptr),
      featureClass_(nullptr),
      previewUndoMark_(static_cast<Session::UndoMarkId>(0)),
      previewUdfTag_(NULL_TAG),
      previewReferenceTags_(),
      hasPreview_(false),
      isUpdatingPreview_(false)
{
    customFeatureManager_ = session_->CustomFeatureClassManager();
    editedFeature_ = customFeatureManager_->GetEditedCustomFeature();
    featureClass_ = customFeatureManager_->GetClassFromName(zhihui_twopoint_sibian::kFeatureClassName);

    dialog_ = ui_->CreateDialog("TwoPointSiBian.dlx");
    dialog_->AddInitializeHandler(make_callback(this, &TwoPointSiBianUI::initialize_cb));
    dialog_->AddDialogShownHandler(make_callback(this, &TwoPointSiBianUI::dialogShown_cb));
    dialog_->AddEnableOKButtonHandler(make_callback(this, &TwoPointSiBianUI::enable_ok_cb));
    dialog_->AddUpdateHandler(make_callback(this, &TwoPointSiBianUI::update_cb));
    dialog_->AddApplyHandler(make_callback(this, &TwoPointSiBianUI::apply_cb));
    dialog_->AddOkHandler(make_callback(this, &TwoPointSiBianUI::ok_cb));
    dialog_->AddCancelHandler(make_callback(this, &TwoPointSiBianUI::cancel_cb));
    dialog_->AddCloseHandler(make_callback(this, &TwoPointSiBianUI::close_cb));
}

TwoPointSiBianUI::~TwoPointSiBianUI()
{
    delete dialog_;
}

NXOpen::BlockStyler::BlockDialog::DialogResponse TwoPointSiBianUI::Launch()
{
    return dialog_->Launch();
}

void TwoPointSiBianUI::initialize_cb()
{
    startPointBlock_ = dynamic_cast<NXOpen::BlockStyler::SelectObject*>(dialog_->TopBlock()->FindBlock("selection0"));
    endPointBlock_ = dynamic_cast<NXOpen::BlockStyler::SelectObject*>(dialog_->TopBlock()->FindBlock("selection01"));
    clearanceBlock_ = dynamic_cast<NXOpen::BlockStyler::StringBlock*>(dialog_->TopBlock()->FindBlock("string0"));
    bendRadiusBlock_ = dynamic_cast<NXOpen::BlockStyler::StringBlock*>(dialog_->TopBlock()->FindBlock("string01"));
    featureModeBlock_ = dynamic_cast<NXOpen::BlockStyler::Enumeration*>(dialog_->TopBlock()->FindBlock("wrapCornerMode"));

    if (featureModeBlock_ != nullptr)
    {
        std::vector<NXString> members;
        members.emplace_back("鏂滆", NXString::UTF8);
        members.emplace_back("90搴﹀乏", NXString::UTF8);
        members.emplace_back("90搴﹀彸", NXString::UTF8);
        featureModeBlock_->SetEnumMembers(members);
        featureModeBlock_->SetValueAsString(NXString("鏂滆", NXString::UTF8));
        AppendDebugLog("feature mode enum initialized.");
    }
    else
    {
        AppendDebugLog("feature mode enum block missing; defaulting to chamfer.");
    }

    ConfigurePointSelection(startPointBlock_);
    ConfigurePointSelection(endPointBlock_);
}

TwoPointSiBianUI::FeatureMode TwoPointSiBianUI::ReadFeatureMode() const
{
    if (featureModeBlock_ == nullptr)
    {
        return FeatureMode::Chamfer;
    }

    try
    {
        const NXString value = featureModeBlock_->ValueAsString();
        const char* text = value.GetUTF8Text();
        const std::string mode = text != nullptr ? text : "";
        AppendDebugLog("read feature mode=" + mode);
        if (mode == "90搴﹀乏")
        {
            return FeatureMode::NinetyLeft;
        }
        if (mode == "90搴﹀彸")
        {
            return FeatureMode::NinetyRight;
        }
    }
    catch (const NXException& ex)
    {
        AppendDebugLog("failed to read feature mode enum: " + UfMessage(ex.ErrorCode()));
    }

    return FeatureMode::Chamfer;
}

void TwoPointSiBianUI::dialogShown_cb()
{
    if (startPointBlock_ != nullptr)
    {
        startPointBlock_->Focus();
    }
}

bool TwoPointSiBianUI::enable_ok_cb()
{
    TaggedObject* startObject = nullptr;
    TaggedObject* endObject = nullptr;
    Point3d startPoint;
    Point3d endPoint;
    const bool hasStart = ReadSelectedPoint(startPointBlock_, startObject, startPoint);
    const bool hasEnd = ReadSelectedPoint(endPointBlock_, endObject, endPoint);
    if (hasStart && hasEnd && Distance(startPoint, endPoint) > kPointTolerance)
    {
        return true;
    }

    return hasEnd || hasStart;
}

int TwoPointSiBianUI::update_cb(NXOpen::BlockStyler::UIBlock* block)
{
    (void)block;
    if (enable_ok_cb())
    {
        if (!isUpdatingPreview_)
        {
            isUpdatingPreview_ = true;
            const bool ok = CreatePreview();
            isUpdatingPreview_ = false;
            if (!ok)
            {
                AppendDebugLog("update_cb preview failed; keeping dialog input accepted.");
            }
        }
    }
    else
    {
        UndoPreview();
    }
    return 0;
}

int TwoPointSiBianUI::apply_cb()
{
    BeginDebugLogSection();
    AppendDebugLog("apply_cb entered; committing preview");

    try
    {
        if (!hasPreview_)
        {
            if (!CreatePreview())
            {
                throw std::runtime_error("Preview creation failed. See " + DebugLogPath());
            }
        }

        CommitPreview();
        return 0;
    }
    catch (const NXException& ex)
    {
        const std::string message = NxExceptionText(ex);
        AppendDebugLog("apply_cb NXException: code=" + std::to_string(ex.ErrorCode()) + ", message=" + message);
        ShowError(message);
        return 1;
    }
    catch (const std::exception& ex)
    {
        AppendDebugLog(std::string("apply_cb std::exception: ") + ex.what());
        ShowError(ex.what());
        return 1;
    }
}

int TwoPointSiBianUI::ok_cb()
{
    return apply_cb();
}

int TwoPointSiBianUI::cancel_cb()
{
    AppendDebugLog("cancel_cb entered; undoing preview");
    UndoPreview();
    return 0;
}

int TwoPointSiBianUI::close_cb()
{
    AppendDebugLog("close_cb entered; undoing uncommitted preview");
    UndoPreview();
    return 0;
}

bool TwoPointSiBianUI::CreatePreview()
{
    AppendDebugLog("CreatePreview entered");
    UndoPreview();

    previewUndoMark_ = session_->SetUndoMark(Session::MarkVisibilityInvisible, "2P_SiBian Preview");

    InferredInputs inputs;
    if (!ReadInputs(inputs))
    {
        AppendDebugLog("CreatePreview ReadInputs failed.");
        UndoPreview();
        return false;
    }

    std::vector<tag_t> allToolBodyTags;
    std::vector<tag_t> allReferenceTags;
    if (inputs.inferredFromSingleClick)
    {
        bool ripCreated = false;
        tag_t secondUdfTag = NULL_TAG;
        std::vector<tag_t> secondToolBodyTags;
        std::vector<tag_t> secondReferenceTags;
        std::string ripError;
        if (!TryCreateSecondPointRip(inputs,
                                     ripCreated,
                                     secondUdfTag,
                                     secondToolBodyTags,
                                     secondReferenceTags,
                                     ripError))
        {
            UndoPreview();
            ShowError(ripError);
            return false;
        }
        allToolBodyTags.insert(allToolBodyTags.end(),
                               secondToolBodyTags.begin(),
                               secondToolBodyTags.end());
        allReferenceTags.insert(allReferenceTags.end(),
                                secondReferenceTags.begin(),
                                secondReferenceTags.end());
        if (ripCreated)
        {
            AppendDebugLog("CreatePreview edge rip committed; recalculating P2 from the original selection click.");
            InferredInputs refreshedInputs;
            if (!ReadInputs(refreshedInputs))
            {
                UndoPreview();
                ShowError("The second endpoint could not be recalculated after creating the sheet-metal rip.");
                return false;
            }
            inputs = refreshedInputs;
        }
    }

    std::string errorMessage;
    tag_t firstUdfTag = NULL_TAG;
    std::vector<tag_t> firstReferenceTags;
    std::vector<tag_t> firstToolBodyTags;
    if (!CreateUserDefinedFeature(inputs,
                                  errorMessage,
                                  &firstUdfTag,
                                  &firstReferenceTags,
                                  &firstToolBodyTags))
    {
        UndoPreview();
        ShowError(errorMessage);
        return false;
    }
    allToolBodyTags.insert(allToolBodyTags.end(),
                           firstToolBodyTags.begin(),
                           firstToolBodyTags.end());
    allReferenceTags.insert(allReferenceTags.end(),
                            firstReferenceTags.begin(),
                            firstReferenceTags.end());

    tag_t finalSubtractTag = NULL_TAG;
    if (!SubtractToolBodies(inputs.targetBody,
                            allToolBodyTags,
                            finalSubtractTag,
                            errorMessage))
    {
        UndoPreview();
        ShowError(errorMessage);
        return false;
    }

    previewUdfTag_ = finalSubtractTag;
    previewReferenceTags_ = allReferenceTags;
    hasPreview_ = true;
    AppendDebugLog("CreatePreview OK, undoMark=" + std::to_string(static_cast<int>(previewUndoMark_)) +
                   ", previewUdfTag=" + std::to_string(previewUdfTag_) +
                   ", previewReferenceCount=" + std::to_string(previewReferenceTags_.size()));
    return true;
}

void TwoPointSiBianUI::UndoPreview()
{
    if (!hasPreview_ &&
        previewUdfTag_ == NULL_TAG &&
        previewReferenceTags_.empty() &&
        previewUndoMark_ == static_cast<Session::UndoMarkId>(0))
    {
        return;
    }

    bool undoSucceeded = false;
    try
    {
        if (previewUndoMark_ != static_cast<Session::UndoMarkId>(0))
        {
            AppendDebugLog("UndoPreview undoing mark=" + std::to_string(static_cast<int>(previewUndoMark_)));
            session_->UndoToMark(previewUndoMark_, "2P_SiBian Preview");
            session_->DeleteUndoMark(previewUndoMark_, "2P_SiBian Preview");
            undoSucceeded = true;
        }
    }
    catch (const NXException& ex)
    {
        AppendDebugLog("UndoPreview NXException: " + UfMessage(ex.ErrorCode()));
    }
    catch (...)
    {
        AppendDebugLog("UndoPreview unknown exception");
    }

    if (!undoSucceeded)
    {
        if (previewUdfTag_ != NULL_TAG)
        {
            const int deleteResult = UF_OBJ_delete_object(previewUdfTag_);
            AppendDebugLog("UndoPreview fallback delete result feature tag=" + std::to_string(previewUdfTag_) +
                           ", result=" + std::to_string(deleteResult) +
                           " " + UfMessage(deleteResult));
        }

        for (tag_t referenceTag : previewReferenceTags_)
        {
            if (referenceTag == NULL_TAG)
            {
                continue;
            }
            const int deleteResult = UF_OBJ_delete_object(referenceTag);
            AppendDebugLog("UndoPreview fallback delete reference tag=" + std::to_string(referenceTag) +
                           ", result=" + std::to_string(deleteResult) +
                           " " + UfMessage(deleteResult));
        }
    }

    hasPreview_ = false;
    previewUndoMark_ = static_cast<Session::UndoMarkId>(0);
    previewUdfTag_ = NULL_TAG;
    previewReferenceTags_.clear();
}

void TwoPointSiBianUI::CommitPreview()
{
    if (!hasPreview_)
    {
        return;
    }

    try
    {
        AppendDebugLog("CommitPreview mark=" + std::to_string(static_cast<int>(previewUndoMark_)));
        session_->DeleteUndoMark(previewUndoMark_, "2P_SiBian Preview");
    }
    catch (const NXException& ex)
    {
        AppendDebugLog("CommitPreview NXException: " + UfMessage(ex.ErrorCode()));
    }
    catch (...)
    {
        AppendDebugLog("CommitPreview unknown exception");
    }

    hasPreview_ = false;
    previewUndoMark_ = static_cast<Session::UndoMarkId>(0);
    previewUdfTag_ = NULL_TAG;
    previewReferenceTags_.clear();
}
bool TwoPointSiBianUI::ReadInputs(InferredInputs& inputs) const
{
    inputs.featureMode = ReadFeatureMode();

    TaggedObject* selectedStartObject = nullptr;
    TaggedObject* selectedEndObject = nullptr;
    Point3d selectedStartPoint;
    Point3d selectedEndPoint;
    const bool hasStart = ReadSelectedPoint(startPointBlock_, selectedStartObject, selectedStartPoint);
    const bool hasEnd = ReadSelectedPoint(endPointBlock_, selectedEndObject, selectedEndPoint);

    if (hasStart && hasEnd && Distance(selectedStartPoint, selectedEndPoint) > kPointTolerance)
    {
        inputs.startObject = selectedStartObject;
        inputs.endObject = selectedEndObject;
        inputs.startPoint = selectedStartPoint;
        inputs.endPoint = selectedEndPoint;
    }
    else
    {
        TaggedObject* clickObject = hasEnd ? selectedEndObject : selectedStartObject;
        const Point3d clickPoint = hasEnd ? selectedEndPoint : selectedStartPoint;
        inputs.inferredFromSingleClick = true;
        inputs.selectionClickPoint = clickPoint;
        if (!InferEndpointsFromFaceClick(clickObject, clickPoint, inputs))
        {
            return false;
        }
    }

    if (inputs.targetBody == nullptr)
    {
        inputs.targetBody = FindBody(inputs.startObject);
    }
    if (inputs.targetBody == nullptr)
    {
        inputs.targetBody = FindBody(inputs.endObject);
    }

    if (inputs.targetBody != nullptr && inputs.baseFace == nullptr)
    {
        inputs.baseFace = FindPlanarFaceContainingPoints(inputs.targetBody, inputs.startPoint, inputs.endPoint);
    }
    if (inputs.baseFace == nullptr)
    {
        inputs.targetBody = FindBodyAndFaceContainingPoints(inputs.startPoint, inputs.endPoint, inputs.baseFace);
    }
    if (inputs.targetBody == nullptr || inputs.baseFace == nullptr)
    {
        return false;
    }

    Vector3d xDirection = Subtract(inputs.endPoint, inputs.startPoint);
    Vector3d faceNormalVector;
    const Point3d normalPoint(
        (inputs.startPoint.X + inputs.endPoint.X) * 0.5,
        (inputs.startPoint.Y + inputs.endPoint.Y) * 0.5,
        (inputs.startPoint.Z + inputs.endPoint.Z) * 0.5);
    if (!Normalize(xDirection) || !FaceNormalAtPoint(inputs.baseFace, normalPoint, faceNormalVector))
    {
        return false;
    }
    OrientNormalAwayFromOppositeFace(inputs.targetBody, inputs.baseFace, normalPoint, faceNormalVector);
    Vector3d yDirection = Cross(faceNormalVector, xDirection);
    if (!Normalize(yDirection))
    {
        return false;
    }
    AppendDebugLog("local directions: X=(" + FormatExpressionNumber(xDirection.X) + "," +
                   FormatExpressionNumber(xDirection.Y) + "," +
                   FormatExpressionNumber(xDirection.Z) + "), Z=(" +
                   FormatExpressionNumber(faceNormalVector.X) + "," +
                   FormatExpressionNumber(faceNormalVector.Y) + "," +
                   FormatExpressionNumber(faceNormalVector.Z) + "), Y=(" +
                   FormatExpressionNumber(yDirection.X) + "," +
                   FormatExpressionNumber(yDirection.Y) + "," +
                   FormatExpressionNumber(yDirection.Z) + ")");
    if (!FindSignedEdgesAtPoint(inputs.targetBody,
                                inputs.startPoint,
                                xDirection,
                                yDirection,
                                faceNormalVector,
                                inputs.startPositiveYEdge,
                                inputs.startNegativeYEdge) ||
        !FindSignedEdgesAtPoint(inputs.targetBody,
                                inputs.endPoint,
                                xDirection,
                                yDirection,
                                faceNormalVector,
                                inputs.endPositiveYEdge,
                                inputs.endNegativeYEdge))
    {
        return false;
    }
    inputs.startEdge = inputs.startPositiveYEdge;
    inputs.endEdge = inputs.endPositiveYEdge;
    if (inputs.thickness <= kPointTolerance)
    {
        inputs.thickness = EstimateSheetThickness(inputs.targetBody, inputs.baseFace);
    }
    inputs.spanLength = Distance(inputs.startPoint, inputs.endPoint);
    inputs.clearanceValue = ReadStringBlockValue(clearanceBlock_, "string0", "0.2");
    inputs.bendRadiusValue = ReadStringBlockValue(bendRadiusBlock_, "string01", "0.2");

    std::ostringstream trace;
    trace << "ReadInputs OK:"
          << "\n  startObject=" << (inputs.startObject != nullptr ? inputs.startObject->Tag() : NULL_TAG)
          << " point=(" << inputs.startPoint.X << "," << inputs.startPoint.Y << "," << inputs.startPoint.Z << ")"
          << "\n  endObject=" << (inputs.endObject != nullptr ? inputs.endObject->Tag() : NULL_TAG)
          << " point=(" << inputs.endPoint.X << "," << inputs.endPoint.Y << "," << inputs.endPoint.Z << ")"
          << "\n  body=" << (inputs.targetBody != nullptr ? inputs.targetBody->Tag() : NULL_TAG)
          << ", face=" << (inputs.baseFace != nullptr ? inputs.baseFace->Tag() : NULL_TAG)
          << ", startEdge=" << (inputs.startEdge != nullptr ? inputs.startEdge->Tag() : NULL_TAG)
          << ", endEdge=" << (inputs.endEdge != nullptr ? inputs.endEdge->Tag() : NULL_TAG)
          << "\n  A(P1,Y+)=" << (inputs.startPositiveYEdge != nullptr ? inputs.startPositiveYEdge->Tag() : NULL_TAG)
          << ", B(P1,Y-)=" << (inputs.startNegativeYEdge != nullptr ? inputs.startNegativeYEdge->Tag() : NULL_TAG)
          << ", a(P2,Y+)=" << (inputs.endPositiveYEdge != nullptr ? inputs.endPositiveYEdge->Tag() : NULL_TAG)
          << ", b(P2,Y-)=" << (inputs.endNegativeYEdge != nullptr ? inputs.endNegativeYEdge->Tag() : NULL_TAG)
          << "\n  spanLength=" << inputs.spanLength
          << ", thickness=" << inputs.thickness
          << "\n  clearanceValue=" << inputs.clearanceValue
          << ", bendRadiusValue=" << inputs.bendRadiusValue;
    AppendDebugLog(trace.str());
    return true;
}

bool TwoPointSiBianUI::ReadSelectedPoint(NXOpen::BlockStyler::SelectObject* block,
                                         TaggedObject*& selectedObject,
                                         Point3d& point) const
{
    if (block == nullptr)
    {
        return false;
    }

    std::vector<TaggedObject*> selected = block->GetSelectedObjects();
    if (selected.empty() || selected.front() == nullptr)
    {
        return false;
    }

    selectedObject = selected.front();
    point = block->PickPoint();
    return true;
}

bool TwoPointSiBianUI::CompleteInputsForEndpoints(InferredInputs& inputs) const
{
    if (inputs.targetBody == nullptr || inputs.baseFace == nullptr ||
        Distance(inputs.startPoint, inputs.endPoint) <= kPointTolerance)
    {
        return false;
    }

    Vector3d xDirection = Subtract(inputs.endPoint, inputs.startPoint);
    const Point3d normalPoint((inputs.startPoint.X + inputs.endPoint.X) * 0.5,
                              (inputs.startPoint.Y + inputs.endPoint.Y) * 0.5,
                              (inputs.startPoint.Z + inputs.endPoint.Z) * 0.5);
    Vector3d faceNormalVector;
    if (!Normalize(xDirection) ||
        !FaceNormalAtPoint(inputs.baseFace, normalPoint, faceNormalVector))
    {
        return false;
    }
    OrientNormalAwayFromOppositeFace(inputs.targetBody,
                                     inputs.baseFace,
                                     normalPoint,
                                     faceNormalVector);
    Vector3d yDirection = Cross(faceNormalVector, xDirection);
    if (!Normalize(yDirection) ||
        !FindSignedEdgesAtPoint(inputs.targetBody,
                                inputs.startPoint,
                                xDirection,
                                yDirection,
                                faceNormalVector,
                                inputs.startPositiveYEdge,
                                inputs.startNegativeYEdge) ||
        !FindSignedEdgesAtPoint(inputs.targetBody,
                                inputs.endPoint,
                                xDirection,
                                yDirection,
                                faceNormalVector,
                                inputs.endPositiveYEdge,
                                inputs.endNegativeYEdge))
    {
        return false;
    }

    inputs.startEdge = inputs.startPositiveYEdge;
    inputs.endEdge = inputs.endPositiveYEdge;
    if (inputs.thickness <= kPointTolerance)
    {
        inputs.thickness = EstimateSheetThickness(inputs.targetBody, inputs.baseFace);
    }
    inputs.spanLength = Distance(inputs.startPoint, inputs.endPoint);
    return inputs.thickness > kPointTolerance;
}

bool TwoPointSiBianUI::InferEndpointsFromFaceClick(TaggedObject* selectedObject,
                                                   const Point3d& clickPoint,
                                                   InferredInputs& inputs) const
{
    if (selectedObject == nullptr)
    {
        return false;
    }

    Body* body = FindBody(selectedObject);
    Face* face = dynamic_cast<Face*>(selectedObject);
    if (body == nullptr)
    {
        return false;
    }
    if (face == nullptr || face->SolidFaceType() != Face::FaceTypePlanar)
    {
        face = FindPlanarFaceAtPoint(body, clickPoint);
    }
    if (face == nullptr || face->SolidFaceType() != Face::FaceTypePlanar)
    {
        AppendDebugLog("InferEndpointsFromFaceClick failed: no planar face at click point.");
        return false;
    }

    Vector3d faceNormal;
    Point3d facePlanePoint;
    if (!FacePlaneData(face, facePlanePoint, faceNormal))
    {
        AppendDebugLog("InferEndpointsFromFaceClick failed: no face plane data.");
        return false;
    }
    OrientNormalAwayFromOppositeFace(body, face, clickPoint, faceNormal);

    std::vector<Point3d> boundaryPoints;
    if (!FaceBoundaryPoints(face, boundaryPoints) || boundaryPoints.size() < 2)
    {
        AppendDebugLog("InferEndpointsFromFaceClick failed: not enough face boundary points.");
        return false;
    }

    const double expectedThickness = EstimateSheetThickness(body, face);
    std::vector<BoundaryPointCandidate> candidates;
    candidates.reserve(boundaryPoints.size());
    for (const Point3d& point : boundaryPoints)
    {
        BoundaryPointCandidate candidate;
        candidate.point = point;
        candidate.distance = Distance(point, clickPoint);
        candidate.thicknessScore = ThicknessEdgeScoreAtPoint(body, face, point, faceNormal, expectedThickness);
        candidates.push_back(candidate);
    }

    std::sort(candidates.begin(), candidates.end(), [](const BoundaryPointCandidate& first,
                                                        const BoundaryPointCandidate& second) {
        return first.distance < second.distance;
    });

    BoundaryPointCandidate first;
    BoundaryPointCandidate second;
    double selectedPairAngle = 0.0;
    double bestDistanceSum = std::numeric_limits<double>::max();
    double bestMaximumDistance = std::numeric_limits<double>::max();
    bool foundPair = false;
    for (std::size_t firstIndex = 0; firstIndex < candidates.size(); ++firstIndex)
    {
        const Vector3d firstFromClick = Subtract(candidates[firstIndex].point, clickPoint);
        if (Length(firstFromClick) <= kPointTolerance)
        {
            continue;
        }

        for (std::size_t secondIndex = firstIndex + 1; secondIndex < candidates.size(); ++secondIndex)
        {
            if (Distance(candidates[firstIndex].point, candidates[secondIndex].point) <= kPointTolerance)
            {
                continue;
            }

            const Vector3d secondFromClick = Subtract(candidates[secondIndex].point, clickPoint);
            const double pairAngle = AngleDegrees(firstFromClick, secondFromClick);
            if (pairAngle + 1.0e-6 < kEndpointPairMinimumAngleDegrees ||
                pairAngle - 1.0e-6 > kEndpointPairMaximumAngleDegrees)
            {
                continue;
            }

            const double distanceSum = candidates[firstIndex].distance + candidates[secondIndex].distance;
            const double maximumDistance = std::max(candidates[firstIndex].distance,
                                                    candidates[secondIndex].distance);
            if (!foundPair ||
                distanceSum < bestDistanceSum - kPointTolerance ||
                (std::fabs(distanceSum - bestDistanceSum) <= kPointTolerance &&
                 maximumDistance < bestMaximumDistance))
            {
                first = candidates[firstIndex];
                second = candidates[secondIndex];
                selectedPairAngle = pairAngle;
                bestDistanceSum = distanceSum;
                bestMaximumDistance = maximumDistance;
                foundPair = true;
            }
        }
    }
    if (!foundPair)
    {
        AppendDebugLog("InferEndpointsFromFaceClick failed: no nearest boundary point pair has a click angle in [150,180] degrees.");
        return false;
    }

    const BoundaryPointCandidate* p1 = &first;
    const BoundaryPointCandidate* p2 = &second;
    double firstInteriorAngle = 0.0;
    double secondInteriorAngle = 0.0;
    const bool hasFirstInteriorAngle = FaceInteriorCornerAngle(face, first.point, firstInteriorAngle);
    const bool hasSecondInteriorAngle = FaceInteriorCornerAngle(face, second.point, secondInteriorAngle);
    const bool firstIsConvex = hasFirstInteriorAngle &&
                               firstInteriorAngle < kConvexCornerMaximumAngleDegrees - 1.0e-6;
    const bool secondIsConvex = hasSecondInteriorAngle &&
                                secondInteriorAngle < kConvexCornerMaximumAngleDegrees - 1.0e-6;
    if (firstIsConvex)
    {
        // The nearer member of the selected pair is already a convex face corner.
    }
    else if (secondIsConvex)
    {
        p1 = &second;
        p2 = &first;
    }
    else if (hasFirstInteriorAngle && hasSecondInteriorAngle)
    {
        AppendDebugLog("InferEndpointsFromFaceClick failed: neither selected boundary point has a face-interior angle below 180 degrees.");
        return false;
    }
    else if (first.thicknessScore > second.thicknessScore + 1.0e-6)
    {
        p1 = &second;
        p2 = &first;
        AppendDebugLog("InferEndpointsFromFaceClick: corner classification unavailable; used thickness-edge score for P1 ordering.");
    }
    else
    {
        AppendDebugLog("InferEndpointsFromFaceClick warning: one or both face-interior corner angles could not be classified.");
    }

    inputs.startObject = selectedObject;
    inputs.endObject = selectedObject;
    inputs.startPoint = p1->point;
    inputs.endPoint = p2->point;
    inputs.targetBody = body;
    inputs.baseFace = face;
    inputs.thickness = expectedThickness;

    std::ostringstream trace;
    trace << "InferEndpointsFromFaceClick OK:"
          << "\n  click=" << FormatPoint(clickPoint)
          << "\n  body=" << body->Tag()
          << ", face=" << face->Tag()
          << "\n  expectedThickness=" << expectedThickness
          << "\n  selectedPairAngle=" << selectedPairAngle
          << " (required " << kEndpointPairMinimumAngleDegrees << "-"
          << kEndpointPairMaximumAngleDegrees << ")"
          << "\n  nearestPair[0]=" << FormatPoint(first.point)
          << " distance=" << first.distance
          << " thicknessScore=" << first.thicknessScore
          << " interiorAngle=" << (hasFirstInteriorAngle ? firstInteriorAngle : -1.0)
          << " convex=" << (firstIsConvex ? "true" : "false")
          << "\n  nearestPair[1]=" << FormatPoint(second.point)
          << " distance=" << second.distance
          << " thicknessScore=" << second.thicknessScore
          << " interiorAngle=" << (hasSecondInteriorAngle ? secondInteriorAngle : -1.0)
          << " convex=" << (secondIsConvex ? "true" : "false")
          << "\n  inferred P1=" << FormatPoint(inputs.startPoint)
          << ", P2=" << FormatPoint(inputs.endPoint);
    AppendDebugLog(trace.str());
    return Distance(inputs.startPoint, inputs.endPoint) > kPointTolerance;
}

Body* TwoPointSiBianUI::FindBody(TaggedObject* object) const
{
    if (object == nullptr)
    {
        return nullptr;
    }

    if (Body* body = dynamic_cast<Body*>(object))
    {
        return body;
    }
    if (Face* face = dynamic_cast<Face*>(object))
    {
        return face->GetBody();
    }
    if (Edge* edge = dynamic_cast<Edge*>(object))
    {
        return edge->GetBody();
    }

    Part* workPart = session_->Parts()->Work();
    for (Body* body : *workPart->Bodies())
    {
        if (body == nullptr)
        {
            continue;
        }

        std::vector<Edge*> edges = body->GetEdges();
        if (std::find(edges.begin(), edges.end(), dynamic_cast<Edge*>(object)) != edges.end())
        {
            return body;
        }
    }

    return nullptr;
}

Body* TwoPointSiBianUI::FindBodyAndFaceContainingPoints(const Point3d& first,
                                                        const Point3d& second,
                                                        Face*& face) const
{
    face = nullptr;
    Part* workPart = session_->Parts()->Work();
    if (workPart == nullptr)
    {
        return nullptr;
    }

    for (Body* body : *workPart->Bodies())
    {
        if (body == nullptr)
        {
            continue;
        }

        Face* candidateFace = FindPlanarFaceContainingPoints(body, first, second);
        if (candidateFace != nullptr)
        {
            face = candidateFace;
            return body;
        }
    }

    return nullptr;
}

Edge* TwoPointSiBianUI::FindEdgeAtPoint(Body* body, const Point3d& point) const
{
    if (body == nullptr)
    {
        return nullptr;
    }

    std::vector<Edge*> edges = body->GetEdges();
    Edge* best = nullptr;
    double bestLength = std::numeric_limits<double>::max();
    for (Edge* edge : edges)
    {
        if (!EdgeTouchesPoint(edge, point))
        {
            continue;
        }

        const double length = edge->GetLength();
        if (length < bestLength)
        {
            best = edge;
            bestLength = length;
        }
    }
    return best;
}

Face* TwoPointSiBianUI::FindPlanarFaceAtPoint(Body* body, const Point3d& point) const
{
    if (body == nullptr)
    {
        return nullptr;
    }

    Face* bestFace = nullptr;
    double bestDistance = std::numeric_limits<double>::max();
    for (Face* face : body->GetFaces())
    {
        if (face == nullptr || face->SolidFaceType() != Face::FaceTypePlanar)
        {
            continue;
        }

        Point3d planePoint;
        Vector3d normal;
        if (!FacePlaneData(face, planePoint, normal))
        {
            continue;
        }

        const double distance = std::fabs(Dot(Subtract(point, planePoint), normal));
        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestFace = face;
        }
    }

    return bestDistance <= kPlaneTolerance ? bestFace : nullptr;
}

double TwoPointSiBianUI::ThicknessEdgeScoreAtPoint(Body* body,
                                                   Face* baseFace,
                                                   const Point3d& point,
                                                   const Vector3d& faceNormal,
                                                   double expectedThickness) const
{
    if (body == nullptr || baseFace == nullptr)
    {
        return 0.0;
    }

    double bestScore = 0.0;
    for (Edge* edge : body->GetEdges())
    {
        if (edge == nullptr || !EdgeTouchesPoint(edge, point) || FaceHasEdge(baseFace, edge))
        {
            continue;
        }

        Point3d first;
        Point3d second;
        try
        {
            edge->GetVertices(&first, &second);
        }
        catch (...)
        {
            continue;
        }

        const Point3d other = Distance(first, point) <= Distance(second, point) ? second : first;
        const Vector3d edgeVector = Subtract(other, point);
        const double edgeLength = Distance(other, point);
        const double normalDistance = std::fabs(Dot(edgeVector, faceNormal));
        if (normalDistance <= kPlaneTolerance || edgeLength <= kPointTolerance)
        {
            continue;
        }

        double score = 0.0;
        if (expectedThickness > kPointTolerance)
        {
            const double thicknessTolerance = std::max(0.2, expectedThickness * 0.30);
            const double lengthLimit = std::max(expectedThickness * 2.5, expectedThickness + 0.5);
            const double tangentialSquared = std::max(0.0, edgeLength * edgeLength - normalDistance * normalDistance);
            const double tangentialDistance = std::sqrt(tangentialSquared);
            const double tangentialLimit = std::max(0.2, expectedThickness * 0.35);
            if (std::fabs(normalDistance - expectedThickness) > thicknessTolerance ||
                edgeLength > lengthLimit ||
                tangentialDistance > tangentialLimit)
            {
                std::ostringstream trace;
                trace << "ThicknessEdgeScoreAtPoint reject edge=" << edge->Tag()
                      << " point=" << FormatPoint(point)
                      << " other=" << FormatPoint(other)
                      << " edgeLength=" << edgeLength
                      << " normalDistance=" << normalDistance
                      << " tangentialDistance=" << tangentialDistance
                      << " expectedThickness=" << expectedThickness;
                AppendDebugLog(trace.str());
                continue;
            }

            score = 1.0 + (thicknessTolerance - std::fabs(normalDistance - expectedThickness)) / thicknessTolerance;
        }
        else
        {
            Vector3d direction = edgeVector;
            if (!Normalize(direction))
            {
                continue;
            }
            score = std::fabs(Dot(direction, faceNormal));
        }

        if (score > bestScore)
        {
            bestScore = score;
        }
    }

    return bestScore >= 0.65 ? bestScore : 0.0;
}

bool TwoPointSiBianUI::FindSignedEdgesAtPoint(Body* body,
                                              const Point3d& point,
                                              const Vector3d& xDirection,
                                              const Vector3d& yDirection,
                                              const Vector3d& zDirection,
                                              Edge*& positiveEdge,
                                              Edge*& negativeEdge) const
{
    positiveEdge = nullptr;
    negativeEdge = nullptr;
    if (body == nullptr)
    {
        return false;
    }

    double bestPositive = kPointTolerance;
    double bestNegative = kPointTolerance;
    std::ostringstream trace;
    trace << "FindSignedEdgesAtPoint local-csys point=(" << point.X << "," << point.Y << "," << point.Z << ")";

    for (Edge* edge : body->GetEdges())
    {
        if (!EdgeTouchesPoint(edge, point))
        {
            continue;
        }

        Point3d otherPoint;
        if (!EdgeOtherPoint(edge, point, otherPoint))
        {
            continue;
        }

        const Vector3d localVector = Subtract(otherPoint, point);
        const double localX = Dot(localVector, xDirection);
        const double localY = Dot(localVector, yDirection);
        const double localZ = Dot(localVector, zDirection);
        trace << "\n  edge=" << edge->Tag()
              << " other=(" << otherPoint.X << "," << otherPoint.Y << "," << otherPoint.Z << ")"
              << " local=(" << localX << "," << localY << "," << localZ << ")"
              << " length=" << edge->GetLength();
        if (localY > bestPositive)
        {
            bestPositive = localY;
            positiveEdge = edge;
        }
        if (localY < -bestNegative)
        {
            bestNegative = -localY;
            negativeEdge = edge;
        }
    }

    trace << "\n  selected positive=" << (positiveEdge != nullptr ? positiveEdge->Tag() : NULL_TAG)
          << ", negative=" << (negativeEdge != nullptr ? negativeEdge->Tag() : NULL_TAG);
    AppendDebugLog(trace.str());
    return positiveEdge != nullptr && negativeEdge != nullptr && positiveEdge != negativeEdge;
}

Face* TwoPointSiBianUI::FindPlanarFaceContainingPoints(Body* body,
                                                       const Point3d& first,
                                                       const Point3d& second) const
{
    if (body == nullptr)
    {
        return nullptr;
    }

    std::vector<Face*> faces = body->GetFaces();
    for (Face* face : faces)
    {
        if (face == nullptr || face->SolidFaceType() != Face::FaceTypePlanar)
        {
            continue;
        }

        if (PointOnFacePlane(face, first) && PointOnFacePlane(face, second))
        {
            return face;
        }
    }

    return nullptr;
}

void TwoPointSiBianUI::OrientNormalAwayFromOppositeFace(Body* body,
                                                       Face* baseFace,
                                                       const Point3d& pointOnFace,
                                                       Vector3d& normal) const
{
    if (body == nullptr || baseFace == nullptr || !Normalize(normal))
    {
        return;
    }

    try
    {
        const double baseArea = MeasureFaceArea(baseFace);
        const double minParallelArea = baseArea * 0.60;
        Face* bestFace = nullptr;
        double bestDistance = std::numeric_limits<double>::max();
        Vector3d bestVector;

        std::ostringstream trace;
        trace << "OrientNormalAwayFromOppositeFace baseFace=" << baseFace->Tag()
              << ", baseArea=" << baseArea
              << ", inputNormal=" << FormatVector(normal);

        std::vector<Face*> faces = body->GetFaces();
        for (Face* face : faces)
        {
            if (face == nullptr || face == baseFace || face->SolidFaceType() != Face::FaceTypePlanar)
            {
                continue;
            }

            Vector3d faceNormal;
            Point3d facePlanePoint;
            if (!FacePlaneData(face, facePlanePoint, faceNormal))
            {
                continue;
            }

            const double parallel = std::fabs(Dot(normal, faceNormal));
            if (parallel < 0.999)
            {
                continue;
            }

            const double area = MeasureFaceArea(face);
            if (baseArea > kPointTolerance && area < minParallelArea)
            {
                trace << "\n  skip opposite face=" << face->Tag()
                      << " area=" << area
                      << " parallel=" << parallel;
                continue;
            }

            const double value = std::fabs(Dot(Subtract(facePlanePoint, pointOnFace), normal));
            if (value <= kPointTolerance || value >= bestDistance)
            {
                continue;
            }

            bestFace = face;
            bestDistance = value;
            bestVector = Subtract(facePlanePoint, pointOnFace);
            trace << "\n  candidate opposite face=" << face->Tag()
                  << " area=" << area
                  << " parallel=" << parallel
                  << " distance=" << value
                  << " facePoint=" << FormatPoint(facePlanePoint)
                  << " vectorToFace=" << FormatVector(bestVector);
        }

        if (bestFace == nullptr || bestDistance == std::numeric_limits<double>::max())
        {
            trace << "\n  no opposite face found; keep normal=" << FormatVector(normal);
            AppendDebugLog(trace.str());
            return;
        }

        const double dotToOpposite = Dot(normal, bestVector);
        const bool flipped = dotToOpposite > 0.0;
        if (flipped)
        {
            normal.X = -normal.X;
            normal.Y = -normal.Y;
            normal.Z = -normal.Z;
        }
        Normalize(normal);

        trace << "\n  selected opposite face=" << bestFace->Tag()
              << ", distance=" << bestDistance
              << ", dot(normal,vectorToFace)=" << dotToOpposite
              << ", flipped=" << (flipped ? "true" : "false")
              << ", outputNormal=" << FormatVector(normal);
        AppendDebugLog(trace.str());
    }
    catch (const NXException& ex)
    {
        AppendDebugLog("OrientNormalAwayFromOppositeFace NXException: " + UfMessage(ex.ErrorCode()));
    }
    catch (...)
    {
        AppendDebugLog("OrientNormalAwayFromOppositeFace unknown exception.");
    }
}

double TwoPointSiBianUI::MeasureFaceArea(Face* face) const
{
    if (face == nullptr)
    {
        return 0.0;
    }

    Part* workPart = session_->Parts()->Work();
    if (workPart == nullptr)
    {
        return 0.0;
    }

    try
    {
        Unit* areaUnit = workPart->UnitCollection()->FindObject("SquareMilliMeter");
        Unit* lengthUnit = workPart->UnitCollection()->FindObject("MilliMeter");
        if (areaUnit == nullptr || lengthUnit == nullptr)
        {
            AppendDebugLog("MeasureFaceArea failed to find measurement units.");
            return 0.0;
        }

        std::vector<IParameterizedSurface*> faces;
        faces.push_back(face);
        MeasureFaces* measure = workPart->MeasureManager()->NewFaceProperties(
            areaUnit,
            lengthUnit,
            0.99,
            faces);
        if (measure == nullptr)
        {
            return 0.0;
        }

        const double area = measure->Area();
        delete measure;
        return area;
    }
    catch (const NXException& ex)
    {
        AppendDebugLog("MeasureFaceArea NXException: " + UfMessage(ex.ErrorCode()));
    }
    catch (...)
    {
        AppendDebugLog("MeasureFaceArea unknown exception.");
    }

    return 0.0;
}

bool TwoPointSiBianUI::EdgeHasParallelMateAtThickness(Body* body,
                                                       Edge* edge,
                                                       double thickness,
                                                       Edge*& parallelEdge,
                                                       double& minimumDistance) const
{
    parallelEdge = nullptr;
    minimumDistance = std::numeric_limits<double>::max();
    if (body == nullptr || edge == nullptr || thickness <= kPointTolerance)
    {
        return false;
    }

    Point3d edgeStart;
    Point3d edgeEnd;
    if (!EdgeNaturalStartEnd(edge, edgeStart, edgeEnd))
    {
        return false;
    }
    Vector3d edgeDirection = Subtract(edgeEnd, edgeStart);
    const double edgeLength = Length(edgeDirection);
    if (!Normalize(edgeDirection))
    {
        return false;
    }

    Part* workPart = session_->Parts()->Work();
    if (workPart == nullptr)
    {
        return false;
    }

    double selectedOverlapLength = 0.0;
    int parallelWithoutOverlapCount = 0;
    for (Edge* candidate : body->GetEdges())
    {
        if (candidate == nullptr || candidate == edge)
        {
            continue;
        }

        Point3d candidateStart;
        Point3d candidateEnd;
        if (!EdgeNaturalStartEnd(candidate, candidateStart, candidateEnd))
        {
            continue;
        }
        Vector3d candidateDirection = Subtract(candidateEnd, candidateStart);
        if (!Normalize(candidateDirection) || std::fabs(Dot(edgeDirection, candidateDirection)) < 0.999)
        {
            continue;
        }

        const double candidateProjectionStart = Dot(Subtract(candidateStart, edgeStart), edgeDirection);
        const double candidateProjectionEnd = Dot(Subtract(candidateEnd, edgeStart), edgeDirection);
        const double candidateMinimum = std::min(candidateProjectionStart, candidateProjectionEnd);
        const double candidateMaximum = std::max(candidateProjectionStart, candidateProjectionEnd);
        const double overlapStart = std::max(0.0, candidateMinimum);
        const double overlapEnd = std::min(edgeLength, candidateMaximum);
        const double overlapLength = overlapEnd - overlapStart;
        if (overlapLength <= kPlaneTolerance)
        {
            ++parallelWithoutOverlapCount;
            continue;
        }

        try
        {
            MeasureDistance* measurement = workPart->MeasureManager()->NewDistance(
                nullptr,
                MeasureManager::MeasureTypeMinimum,
                edge,
                candidate);
            if (measurement == nullptr)
            {
                continue;
            }
            const double distance = measurement->Value();
            delete measurement;
            if (distance <= kPlaneTolerance || distance >= minimumDistance)
            {
                continue;
            }
            minimumDistance = distance;
            parallelEdge = candidate;
            selectedOverlapLength = overlapLength;
        }
        catch (...)
        {
        }
    }

    const double tolerance = std::max(0.02, thickness * 0.10);
    const bool matched = parallelEdge != nullptr &&
                         std::fabs(minimumDistance - thickness) <= tolerance;
    std::ostringstream trace;
    trace << "EdgeHasParallelMateAtThickness edge=" << edge->Tag()
          << ", nearestParallel=" << (parallelEdge != nullptr ? parallelEdge->Tag() : NULL_TAG)
          << ", minimumDistance="
          << (minimumDistance < std::numeric_limits<double>::max() ? minimumDistance : -1.0)
          << ", thickness=" << thickness
          << ", overlapLength=" << selectedOverlapLength
          << ", parallelWithoutOverlapCount=" << parallelWithoutOverlapCount
          << ", tolerance=" << tolerance
          << ", matched=" << (matched ? "true" : "false");
    AppendDebugLog(trace.str());
    return matched;
}

Face* TwoPointSiBianUI::FindPlanarFaceContainingEdges(Body* body,
                                                       Edge* first,
                                                       Edge* second) const
{
    if (body == nullptr || first == nullptr || second == nullptr)
    {
        return nullptr;
    }
    for (Face* face : body->GetFaces())
    {
        if (face != nullptr && face->SolidFaceType() == Face::FaceTypePlanar &&
            FaceHasEdge(face, first) && FaceHasEdge(face, second))
        {
            return face;
        }
    }
    return nullptr;
}

Face* TwoPointSiBianUI::FindParallelFaceAtThickness(Body* body,
                                                     Face* sourceFace,
                                                     double thickness) const
{
    if (body == nullptr || sourceFace == nullptr || thickness <= kPointTolerance)
    {
        return nullptr;
    }

    Point3d sourcePoint;
    Vector3d sourceNormal;
    if (!FacePlaneData(sourceFace, sourcePoint, sourceNormal))
    {
        return nullptr;
    }

    const double tolerance = std::max(0.02, thickness * 0.10);
    Face* best = nullptr;
    double bestError = std::numeric_limits<double>::max();
    for (Face* candidate : body->GetFaces())
    {
        if (candidate == nullptr || candidate == sourceFace ||
            candidate->SolidFaceType() != Face::FaceTypePlanar)
        {
            continue;
        }
        Point3d candidatePoint;
        Vector3d candidateNormal;
        if (!FacePlaneData(candidate, candidatePoint, candidateNormal) ||
            std::fabs(Dot(sourceNormal, candidateNormal)) < 0.999)
        {
            continue;
        }
        const double distance = std::fabs(Dot(Subtract(candidatePoint, sourcePoint), sourceNormal));
        const double error = std::fabs(distance - thickness);
        if (error <= tolerance && error < bestError)
        {
            best = candidate;
            bestError = error;
        }
    }
    return best;
}

bool TwoPointSiBianUI::BuildFallbackSecondInputs(const InferredInputs& sourceInputs,
                                                  Edge* firstEdgeAtQ,
                                                  Edge* secondEdgeAtQ,
                                                  const Point3d& qPoint,
                                                  InferredInputs& secondInputs) const
{
    // P2-Q only locates Q.  The fallback common face is defined by the two
    // remaining edges at Q, explicitly excluding the P2-Q rip edge.
    Face* commonFace = FindPlanarFaceContainingEdges(sourceInputs.targetBody,
                                                     firstEdgeAtQ,
                                                     secondEdgeAtQ);
    double commonFaceInteriorAngle = 0.0;
    const bool commonCornerIsConvex =
        commonFace != nullptr &&
        FaceInteriorCornerAngle(commonFace, qPoint, commonFaceInteriorAngle) &&
        commonFaceInteriorAngle < kConvexCornerMaximumAngleDegrees - 1.0e-6;
    AppendDebugLog("P2 fallback common-face corner check: commonFace=" +
                   std::to_string(commonFace != nullptr ? commonFace->Tag() : NULL_TAG) +
                   ", B1=" + std::to_string(firstEdgeAtQ != nullptr ? firstEdgeAtQ->Tag() : NULL_TAG) +
                   ", B2=" + std::to_string(secondEdgeAtQ != nullptr ? secondEdgeAtQ->Tag() : NULL_TAG) +
                   ", Q=" + FormatPoint(qPoint) +
                   ", interiorAngle=" + FormatExpressionNumber(commonFaceInteriorAngle) +
                   ", convex=" + (commonCornerIsConvex ? "true" : "false"));
    if (!commonCornerIsConvex)
    {
        return false;
    }
    Point3d firstExtension;
    Point3d secondExtension;
    int firstBodyStatus = 0;
    int secondBodyStatus = 0;
    bool bothExtensionsOnBody = false;
    if (!CornerExtensionPointsOnBody(sourceInputs.targetBody,
                                     firstEdgeAtQ,
                                     secondEdgeAtQ,
                                     qPoint,
                                     firstExtension,
                                     secondExtension,
                                     firstBodyStatus,
                                     secondBodyStatus,
                                     bothExtensionsOnBody))
    {
        return false;
    }
    AppendDebugLog("P2 fallback branch-4 body containment: body=" +
                   std::to_string(sourceInputs.targetBody->Tag()) +
                   ", Q=" + FormatPoint(qPoint) +
                   ", firstExtension=" + FormatPoint(firstExtension) +
                   ", firstStatus=" + std::to_string(firstBodyStatus) +
                   ", secondExtension=" + FormatPoint(secondExtension) +
                   ", secondStatus=" + std::to_string(secondBodyStatus) +
                   ", bothOnOrInBody=" + (bothExtensionsOnBody ? "true" : "false"));
    if (!bothExtensionsOnBody)
    {
        return false;
    }
    Face* parallelFace = FindParallelFaceAtThickness(sourceInputs.targetBody,
                                                      commonFace,
                                                      sourceInputs.thickness);
    if (commonFace == nullptr || parallelFace == nullptr)
    {
        return false;
    }

    std::vector<Point3d> boundaryPoints;
    if (!FaceBoundaryPoints(parallelFace, boundaryPoints) || boundaryPoints.size() < 2)
    {
        return false;
    }
    std::sort(boundaryPoints.begin(), boundaryPoints.end(), [&qPoint](const Point3d& first,
                                                                      const Point3d& second) {
        return Distance(first, qPoint) < Distance(second, qPoint);
    });
    const Point3d q1 = boundaryPoints.front();
    Point3d q2;
    double q2Distance = std::numeric_limits<double>::max();
    bool foundQ2 = false;
    Edge* q2ThicknessEdge = nullptr;
    for (std::size_t index = 1; index < boundaryPoints.size(); ++index)
    {
        const double distance = Distance(q1, boundaryPoints[index]);
        Edge* candidateThicknessEdge = nullptr;
        if (distance > kPointTolerance && distance < q2Distance &&
            PointHasThicknessLengthEdge(sourceInputs.targetBody,
                                        boundaryPoints[index],
                                        sourceInputs.thickness,
                                        nullptr,
                                        nullptr,
                                        &candidateThicknessEdge))
        {
            q2 = boundaryPoints[index];
            q2Distance = distance;
            q2ThicknessEdge = candidateThicknessEdge;
            foundQ2 = true;
        }
    }
    if (!foundQ2)
    {
        return false;
    }

    secondInputs = sourceInputs;
    secondInputs.inferredFromSingleClick = false;
    secondInputs.startObject = parallelFace;
    secondInputs.endObject = parallelFace;
    secondInputs.startPoint = q1;
    secondInputs.endPoint = q2;
    secondInputs.baseFace = parallelFace;
    secondInputs.startEdge = nullptr;
    secondInputs.endEdge = nullptr;
    secondInputs.startPositiveYEdge = nullptr;
    secondInputs.startNegativeYEdge = nullptr;
    secondInputs.endPositiveYEdge = nullptr;
    secondInputs.endNegativeYEdge = nullptr;
    if (!CompleteInputsForEndpoints(secondInputs))
    {
        return false;
    }

    AppendDebugLog("P2 fallback second UDF inputs: commonFace=" + std::to_string(commonFace->Tag()) +
                   ", parallelFace=" + std::to_string(parallelFace->Tag()) +
                   ", Q=" + FormatPoint(qPoint) +
                   ", Q1=" + FormatPoint(q1) +
                   ", Q2=" + FormatPoint(q2) +
                   ", Q2ThicknessEdge=" +
                   std::to_string(q2ThicknessEdge != nullptr ? q2ThicknessEdge->Tag() : NULL_TAG));
    return true;
}

bool TwoPointSiBianUI::BuildQFirstSecondInputs(const InferredInputs& sourceInputs,
                                               Edge* firstEdgeAtQ,
                                               Edge* secondEdgeAtQ,
                                               const Point3d& qPoint,
                                               InferredInputs& secondInputs) const
{
    Face* commonFace = FindPlanarFaceContainingEdges(sourceInputs.targetBody,
                                                     firstEdgeAtQ,
                                                     secondEdgeAtQ);
    double interiorAngle = 0.0;
    if (commonFace == nullptr ||
        !FaceInteriorCornerAngle(commonFace, qPoint, interiorAngle))
    {
        return false;
    }

    bool qFirstRequired = interiorAngle > kConvexCornerMaximumAngleDegrees + 1.0e-6;
    Point3d firstExtension;
    Point3d secondExtension;
    int firstBodyStatus = 0;
    int secondBodyStatus = 0;
    bool bothExtensionsOnBody = false;
    if (!qFirstRequired)
    {
        if (!CornerExtensionPointsOnBody(sourceInputs.targetBody,
                                         firstEdgeAtQ,
                                         secondEdgeAtQ,
                                         qPoint,
                                         firstExtension,
                                         secondExtension,
                                         firstBodyStatus,
                                         secondBodyStatus,
                                         bothExtensionsOnBody))
        {
            return false;
        }
        qFirstRequired = !bothExtensionsOnBody;
        AppendDebugLog("Q-first convex body containment: body=" +
                       std::to_string(sourceInputs.targetBody->Tag()) +
                       ", commonFace=" + std::to_string(commonFace->Tag()) +
                       ", Q=" + FormatPoint(qPoint) +
                       ", firstExtension=" + FormatPoint(firstExtension) +
                       ", firstStatus=" + std::to_string(firstBodyStatus) +
                       ", secondExtension=" + FormatPoint(secondExtension) +
                       ", secondStatus=" + std::to_string(secondBodyStatus) +
                       ", bothOnOrInBody=" + (bothExtensionsOnBody ? "true" : "false") +
                       ", useQFirst=" + (qFirstRequired ? "true" : "false"));
    }
    if (!qFirstRequired)
    {
        return false;
    }

    std::vector<Point3d> boundaryPoints;
    if (!FaceBoundaryPoints(commonFace, boundaryPoints) || boundaryPoints.size() < 2)
    {
        return false;
    }

    Point3d nearestPoint;
    double nearestDistance = std::numeric_limits<double>::max();
    bool foundNearestPoint = false;
    for (const Point3d& boundaryPoint : boundaryPoints)
    {
        const double distance = Distance(qPoint, boundaryPoint);
        if (distance > kPointTolerance && distance < nearestDistance)
        {
            nearestPoint = boundaryPoint;
            nearestDistance = distance;
            foundNearestPoint = true;
        }
    }
    if (!foundNearestPoint)
    {
        return false;
    }

    secondInputs = sourceInputs;
    secondInputs.inferredFromSingleClick = false;
    secondInputs.startObject = commonFace;
    secondInputs.endObject = commonFace;
    secondInputs.startPoint = qPoint;
    secondInputs.endPoint = nearestPoint;
    secondInputs.baseFace = commonFace;
    secondInputs.startEdge = nullptr;
    secondInputs.endEdge = nullptr;
    secondInputs.startPositiveYEdge = nullptr;
    secondInputs.startNegativeYEdge = nullptr;
    secondInputs.endPositiveYEdge = nullptr;
    secondInputs.endNegativeYEdge = nullptr;
    if (!CompleteInputsForEndpoints(secondInputs))
    {
        AppendDebugLog("Q-first second UDF rejected: endpoint inputs could not be completed"
                       ", commonFace=" + std::to_string(commonFace->Tag()) +
                       ", Q=" + FormatPoint(qPoint) +
                       ", nearest=" + FormatPoint(nearestPoint));
        return false;
    }

    AppendDebugLog("Q-first second UDF inputs: commonFace=" +
                   std::to_string(commonFace->Tag()) +
                   ", interiorAngle=" + FormatExpressionNumber(interiorAngle) +
                   ", firstPointQ=" + FormatPoint(qPoint) +
                   ", secondPoint=" + FormatPoint(nearestPoint) +
                   ", distance=" + FormatExpressionNumber(nearestDistance));
    return true;
}

bool TwoPointSiBianUI::BuildConcaveStripPlan(const InferredInputs& sourceInputs,
                                              Edge* firstEdgeAtQ,
                                              Edge* secondEdgeAtQ,
                                              const Point3d& qPoint,
                                              Point3d& q3,
                                              Point3d& q4,
                                              Vector3d& planeNormal) const
{
    Face* commonFace = FindPlanarFaceContainingEdges(sourceInputs.targetBody,
                                                     firstEdgeAtQ,
                                                     secondEdgeAtQ);
    double interiorAngle = 0.0;
    if (commonFace == nullptr ||
        !FaceInteriorCornerAngle(commonFace, qPoint, interiorAngle) ||
        interiorAngle <= kConvexCornerMaximumAngleDegrees + 1.0e-6)
    {
        return false;
    }

    std::vector<Point3d> boundaryPoints;
    if (!FaceBoundaryPoints(commonFace, boundaryPoints) || boundaryPoints.size() < 2)
    {
        return false;
    }
    std::sort(boundaryPoints.begin(), boundaryPoints.end(), [&qPoint](const Point3d& first,
                                                                      const Point3d& second) {
        return Distance(first, qPoint) < Distance(second, qPoint);
    });
    bool foundQ3 = false;
    double q3DistanceFromQ = std::numeric_limits<double>::max();
    for (const Point3d& boundaryPoint : boundaryPoints)
    {
        const double distance = Distance(boundaryPoint, qPoint);
        if (distance > kPointTolerance && distance < q3DistanceFromQ)
        {
            q3 = boundaryPoint;
            q3DistanceFromQ = distance;
            foundQ3 = true;
        }
    }
    if (!foundQ3)
    {
        AppendDebugLog("concave Q3/Q4 branch rejected: no boundary endpoint distinct from Q"
                       ", commonFace=" + std::to_string(commonFace->Tag()) +
                       ", interiorAngle=" + FormatExpressionNumber(interiorAngle) +
                       ", Q=" + FormatPoint(qPoint));
        return false;
    }
    Edge* q3ThicknessEdge = nullptr;
    if (!PointHasThicknessLengthEdge(sourceInputs.targetBody,
                                     q3,
                                     sourceInputs.thickness,
                                     nullptr,
                                     nullptr,
                                     &q3ThicknessEdge))
    {
        AppendDebugLog("concave Q3/Q4 branch rejected: nearest Q3 has no thickness-length edge"
                       ", commonFace=" + std::to_string(commonFace->Tag()) +
                       ", interiorAngle=" + FormatExpressionNumber(interiorAngle) +
                       ", Q=" + FormatPoint(qPoint) +
                       ", Q3=" + FormatPoint(q3));
        return false;
    }

    double nearestQ4Distance = std::numeric_limits<double>::max();
    bool foundQ4 = false;
    for (const Point3d& boundaryPoint : boundaryPoints)
    {
        const double distance = Distance(q3, boundaryPoint);
        if (distance > kPointTolerance && distance < nearestQ4Distance)
        {
            q4 = boundaryPoint;
            nearestQ4Distance = distance;
            foundQ4 = true;
        }
    }

    Point3d planePoint;
    if (!foundQ4 || !FacePlaneData(commonFace, planePoint, planeNormal))
    {
        return false;
    }
    AppendDebugLog("concave Q3/Q4 strip plan: commonFace=" + std::to_string(commonFace->Tag()) +
                   ", interiorAngle=" + FormatExpressionNumber(interiorAngle) +
                   ", Q=" + FormatPoint(qPoint) +
                   ", Q3=" + FormatPoint(q3) +
                   ", Q3DistanceFromQ=" + FormatExpressionNumber(q3DistanceFromQ) +
                   ", Q3ThicknessEdge=" + std::to_string(q3ThicknessEdge->Tag()) +
                   ", Q4=" + FormatPoint(q4) +
                   ", normal=" + FormatVector(planeNormal));
    return true;
}

bool TwoPointSiBianUI::CreateConcaveStripCut(const InferredInputs& inputs,
                                              const Point3d& q3,
                                              const Point3d& q4,
                                              const Vector3d& inputPlaneNormal,
                                              tag_t& subtractFeatureTag,
                                              std::string& errorMessage) const
{
    subtractFeatureTag = NULL_TAG;
    Vector3d lineDirection = Subtract(q4, q3);
    Vector3d planeNormal = inputPlaneNormal;
    if (!Normalize(lineDirection) || !Normalize(planeNormal))
    {
        errorMessage = "The Q3-Q4 strip direction or plane normal is invalid.";
        return false;
    }
    Vector3d sideDirection = Cross(planeNormal, lineDirection);
    if (!Normalize(sideDirection))
    {
        errorMessage = "The symmetric Q3-Q4 strip offset direction is invalid.";
        return false;
    }

    const Point3d extendedStart(q3.X - lineDirection.X,
                                q3.Y - lineDirection.Y,
                                q3.Z - lineDirection.Z);
    const Point3d extendedEnd(q4.X + lineDirection.X,
                              q4.Y + lineDirection.Y,
                              q4.Z + lineDirection.Z);
    constexpr double halfWidth = 0.1;
    const Point3d corners[] = {
        Point3d(extendedStart.X + sideDirection.X * halfWidth,
                extendedStart.Y + sideDirection.Y * halfWidth,
                extendedStart.Z + sideDirection.Z * halfWidth),
        Point3d(extendedEnd.X + sideDirection.X * halfWidth,
                extendedEnd.Y + sideDirection.Y * halfWidth,
                extendedEnd.Z + sideDirection.Z * halfWidth),
        Point3d(extendedEnd.X - sideDirection.X * halfWidth,
                extendedEnd.Y - sideDirection.Y * halfWidth,
                extendedEnd.Z - sideDirection.Z * halfWidth),
        Point3d(extendedStart.X - sideDirection.X * halfWidth,
                extendedStart.Y - sideDirection.Y * halfWidth,
                extendedStart.Z - sideDirection.Z * halfWidth)};

    std::vector<tag_t> curveTags;
    uf_list_p_t curveList = nullptr;
    uf_list_p_t featureList = nullptr;
    auto cleanupLists = [&]() {
        if (curveList != nullptr) UF_MODL_delete_list(&curveList);
        if (featureList != nullptr) UF_MODL_delete_list(&featureList);
    };
    for (int index = 0; index < 4; ++index)
    {
        UF_CURVE_line_t lineData;
        const Point3d& start = corners[index];
        const Point3d& end = corners[(index + 1) % 4];
        lineData.start_point[0] = start.X; lineData.start_point[1] = start.Y; lineData.start_point[2] = start.Z;
        lineData.end_point[0] = end.X; lineData.end_point[1] = end.Y; lineData.end_point[2] = end.Z;
        tag_t lineTag = NULL_TAG;
        const int lineResult = UF_CURVE_create_line(&lineData, &lineTag);
        if (lineResult != 0 || lineTag == NULL_TAG)
        {
            errorMessage = "Failed to create the symmetric Q3-Q4 strip profile.\n" + UfMessage(lineResult);
            cleanupLists();
            return false;
        }
        curveTags.push_back(lineTag);
    }
    if (UF_MODL_create_list(&curveList) != 0 || curveList == nullptr)
    {
        errorMessage = "Failed to allocate the Q3-Q4 strip profile list.";
        return false;
    }
    for (tag_t curveTag : curveTags) UF_MODL_put_list_item(curveList, curveTag);

    std::string startLimit = "-" + FormatExpressionNumber(inputs.thickness);
    std::string endLimit = FormatExpressionNumber(inputs.thickness);
    char taper[] = "0.0";
    char* limits[2] = {const_cast<char*>(startLimit.c_str()), const_cast<char*>(endLimit.c_str())};
    double origin[3] = {q3.X, q3.Y, q3.Z};
    double direction[3] = {planeNormal.X, planeNormal.Y, planeNormal.Z};
    const int extrudeResult = UF_MODL_create_extruded(curveList,
                                                       taper,
                                                       limits,
                                                       origin,
                                                       direction,
                                                       UF_NULLSIGN,
                                                       &featureList);
    tag_t extrudeFeatureTag = NULL_TAG;
    tag_t toolBodyTag = NULL_TAG;
    int featureCount = 0;
    if (extrudeResult == 0 && featureList != nullptr &&
        UF_MODL_ask_list_count(featureList, &featureCount) == 0 && featureCount > 0)
    {
        UF_MODL_ask_list_item(featureList, 0, &extrudeFeatureTag);
        UF_MODL_ask_feat_body(extrudeFeatureTag, &toolBodyTag);
    }
    cleanupLists();
    if (extrudeResult != 0 || toolBodyTag == NULL_TAG)
    {
        errorMessage = "Failed to extrude the Q3-Q4 strip from -thickness to +thickness.\n" +
                       UfMessage(extrudeResult);
        return false;
    }

    if (!SubtractToolBodies(inputs.targetBody,
                            std::vector<tag_t>{toolBodyTag},
                            subtractFeatureTag,
                            errorMessage))
    {
        return false;
    }
    AppendDebugLog("concave Q3/Q4 strip cut completed: Q3=" + FormatPoint(q3) +
                   ", Q4=" + FormatPoint(q4) +
                   ", extensionEachEnd=1, symmetricHalfWidth=0.1"
                   ", startLimit=" + startLimit +
                   ", endLimit=" + endLimit +
                   ", extrudeFeature=" + std::to_string(extrudeFeatureTag) +
                   ", subtractFeature=" + std::to_string(subtractFeatureTag));
    return true;
}

bool TwoPointSiBianUI::FindAuxiliaryRipPair(Body* body,
                                             Face* commonFace,
                                             Edge* b1,
                                             Edge* b2,
                                             double thickness,
                                             Edge*& edgeToRip,
                                             Edge*& parallelRipEdge) const
{
    edgeToRip = nullptr;
    parallelRipEdge = nullptr;
    if (body == nullptr || commonFace == nullptr || b1 == nullptr || b2 == nullptr)
    {
        return false;
    }

    Point3d b1Start;
    Point3d b1End;
    Point3d b2Start;
    Point3d b2End;
    if (!EdgeNaturalStartEnd(b1, b1Start, b1End) ||
        !EdgeNaturalStartEnd(b2, b2Start, b2End))
    {
        return false;
    }

    Point3d q;
    Point3d b1Other;
    Point3d b2Other;
    if (AlmostSamePoint(b1Start, b2Start))
    {
        q = b1Start; b1Other = b1End; b2Other = b2End;
    }
    else if (AlmostSamePoint(b1Start, b2End))
    {
        q = b1Start; b1Other = b1End; b2Other = b2Start;
    }
    else if (AlmostSamePoint(b1End, b2Start))
    {
        q = b1End; b1Other = b1Start; b2Other = b2End;
    }
    else if (AlmostSamePoint(b1End, b2End))
    {
        q = b1End; b1Other = b1Start; b2Other = b2Start;
    }
    else
    {
        return false;
    }

    Vector3d b1Direction = Subtract(b1Other, q);
    Vector3d b2Direction = Subtract(b2Other, q);
    if (!Normalize(b1Direction) || !Normalize(b2Direction))
    {
        return false;
    }

    Edge* matchedPeripheral = nullptr;
    Edge* matchedThicknessEdge = nullptr;
    for (Edge* candidate : commonFace->GetEdges())
    {
        if (candidate == nullptr || candidate == b1 || candidate == b2)
        {
            continue;
        }
        Point3d candidateStart;
        Point3d candidateEnd;
        if (!EdgeNaturalStartEnd(candidate, candidateStart, candidateEnd))
        {
            continue;
        }
        Vector3d candidateDirection = Subtract(candidateEnd, candidateStart);
        if (!Normalize(candidateDirection))
        {
            continue;
        }

        const bool parallelB1 = std::fabs(Dot(candidateDirection, b1Direction)) >= 0.999;
        const bool parallelB2 = std::fabs(Dot(candidateDirection, b2Direction)) >= 0.999;
        Edge* thicknessEdgeAtCommonPoint = nullptr;
        if (parallelB1 && EdgeTouchesPoint(candidate, b2Other) &&
            PointHasThicknessLengthEdge(body,
                                        b2Other,
                                        thickness,
                                        candidate,
                                        b2,
                                        &thicknessEdgeAtCommonPoint))
        {
            edgeToRip = b2;
            matchedPeripheral = candidate;
            matchedThicknessEdge = thicknessEdgeAtCommonPoint;
            break;
        }
        if (parallelB2 && EdgeTouchesPoint(candidate, b1Other) &&
            PointHasThicknessLengthEdge(body,
                                        b1Other,
                                        thickness,
                                        candidate,
                                        b1,
                                        &thicknessEdgeAtCommonPoint))
        {
            edgeToRip = b1;
            matchedPeripheral = candidate;
            matchedThicknessEdge = thicknessEdgeAtCommonPoint;
            break;
        }
    }
    if (edgeToRip == nullptr)
    {
        return false;
    }

    double parallelDistance = 0.0;
    EdgeHasParallelMateAtThickness(body,
                                   edgeToRip,
                                   1.0,
                                   parallelRipEdge,
                                   parallelDistance);
    if (parallelRipEdge == nullptr)
    {
        return false;
    }

    AppendDebugLog("auxiliary rip topology matched: commonFace=" + std::to_string(commonFace->Tag()) +
                   ", B1=" + std::to_string(b1->Tag()) +
                   ", B2=" + std::to_string(b2->Tag()) +
                   ", matchedPeripheral=" + std::to_string(matchedPeripheral->Tag()) +
                   ", commonPointThicknessEdge=" +
                   std::to_string(matchedThicknessEdge != nullptr ? matchedThicknessEdge->Tag() : NULL_TAG) +
                   ", edgeToRip=" + std::to_string(edgeToRip->Tag()) +
                   ", parallelRipEdge=" + std::to_string(parallelRipEdge->Tag()) +
                   ", parallelDistance=" + FormatExpressionNumber(parallelDistance));
    return true;
}

bool TwoPointSiBianUI::CreateSheetMetalRip(const InferredInputs& inputs,
                                            Edge* firstEdge,
                                            Edge* secondEdge,
                                            bool offsetCreatedFaces,
                                            tag_t& createdRipTag,
                                            std::string& errorMessage) const
{
    createdRipTag = NULL_TAG;
    Part* workPart = session_->Parts()->Work();
    if (workPart == nullptr || firstEdge == nullptr || secondEdge == nullptr)
    {
        errorMessage = "The sheet-metal rip input edges are unavailable.";
        return false;
    }

    Features::SheetMetal::EdgeRipBuilder* ripBuilder = nullptr;
    Section* section = nullptr;
    try
    {
        ripBuilder = workPart->Features()->SheetmetalManager()->CreateEdgeRipFeatureBuilder(nullptr);
        ripBuilder->SetApplicationContext(Features::SheetMetal::ApplicationContextNxSheetMetal);
        ripBuilder->Width()->SetFormula(inputs.clearanceValue);
        ripBuilder->BlendRadius()->SetFormula(inputs.bendRadiusValue);
        ripBuilder->SetEndCapShape(Features::SheetMetal::EdgeRipBuilder::EndCapShapeOptionsRound);
        ripBuilder->SetSketch(nullptr);
        section = workPart->Sections()->CreateSection(9.5e-05, 0.0001, 0.5);
        ripBuilder->SetSection(section);
        ripBuilder->SetRipEdges(std::vector<Edge*>{firstEdge, secondEdge});
        ripBuilder->SetUseSystemWidth(false);
        ripBuilder->SetBlendSharpCorners(false);
        ripBuilder->SetSymmetric(true);
        ripBuilder->SetReverseWidthDirection(false);
        ripBuilder->SetParentFeatureInternal(false);
        Features::Feature* ripFeature = ripBuilder->CommitFeature();
        createdRipTag = ripFeature != nullptr ? ripFeature->Tag() : NULL_TAG;
        ripBuilder->Destroy();
        ripBuilder = nullptr;
        section->Destroy();
        section = nullptr;
        if (ripFeature == nullptr)
        {
            errorMessage = "NX did not create the requested sheet-metal rip.";
            return false;
        }

        if (offsetCreatedFaces)
        {
            Features::OffsetFaceBuilder* offsetBuilder =
                workPart->Features()->CreateOffsetFaceBuilder(nullptr);
            offsetBuilder->Distance()->SetFormula(("-(" + inputs.clearanceValue + ")").c_str());
            offsetBuilder->SetDirection(false);
            std::vector<Features::Feature*> featureList(1, ripFeature);
            FaceFeatureRule* faceRule = workPart->ScRuleFactory()->CreateRuleFaceFeature(featureList);
            std::vector<SelectionIntentRule*> rules(1, faceRule);
            offsetBuilder->FaceCollector()->ReplaceRules(rules, false);
            Features::Feature* offsetFeature = offsetBuilder->CommitFeature();
            const tag_t offsetTag = offsetFeature != nullptr ? offsetFeature->Tag() : NULL_TAG;
            offsetBuilder->Destroy();
            if (offsetTag == NULL_TAG)
            {
                errorMessage = "The auxiliary rip was created, but its faces could not be offset by the clearance value.";
                return false;
            }
            AppendDebugLog("auxiliary rip face offset created: rip=" + std::to_string(createdRipTag) +
                           ", offset=" + std::to_string(offsetTag) +
                           ", clearance=-" + inputs.clearanceValue);
        }
        return true;
    }
    catch (const NXException& ex)
    {
        if (ripBuilder != nullptr) ripBuilder->Destroy();
        if (section != nullptr) section->Destroy();
        errorMessage = "Failed to create or offset the sheet-metal rip.\n" + NxExceptionText(ex);
        AppendDebugLog(errorMessage);
        return false;
    }
}

bool TwoPointSiBianUI::TryCreateSecondPointRip(const InferredInputs& inputs,
                                                bool& ripCreated,
                                                tag_t& secondUdfTag,
                                                std::vector<tag_t>& secondToolBodyTags,
                                                std::vector<tag_t>& secondReferenceTags,
                                                std::string& errorMessage) const
{
    ripCreated = false;
    secondUdfTag = NULL_TAG;
    secondToolBodyTags.clear();
    secondReferenceTags.clear();
    errorMessage.clear();
    if (!inputs.inferredFromSingleClick ||
        inputs.targetBody == nullptr ||
        inputs.baseFace == nullptr ||
        inputs.thickness <= kPointTolerance)
    {
        return true;
    }

    const double directCreateTolerance =
        std::max(kPlaneTolerance, inputs.thickness * 0.01);
    for (Edge* edgeAtP2 : inputs.targetBody->GetEdges())
    {
        if (edgeAtP2 == nullptr ||
            !EdgeTouchesPoint(edgeAtP2, inputs.endPoint) ||
            FaceHasEdge(inputs.baseFace, edgeAtP2))
        {
            continue;
        }
        const double p2QCandidateLength = edgeAtP2->GetLength();
        if (std::fabs(p2QCandidateLength - inputs.thickness) <= directCreateTolerance)
        {
            AppendDebugLog("TryCreateSecondPointRip direct-create branch: P2-Q length equals thickness"
                           ", edge=" + std::to_string(edgeAtP2->Tag()) +
                           ", edgeLength=" + FormatExpressionNumber(p2QCandidateLength) +
                           ", thickness=" + FormatExpressionNumber(inputs.thickness) +
                           ", tolerance=" + FormatExpressionNumber(directCreateTolerance));
            return true;
        }
    }

    Edge* ripEdge = nullptr;
    Edge* confirmingEdge = nullptr;
    Edge* confirmingParallelEdge = nullptr;
    Edge* pairedRipEdge = nullptr;
    Point3d farEndpoint;
    double confirmingDistance = 0.0;
    double pairedRipDistance = 0.0;
    bool useFallbackSecondUdf = false;
    bool useAuxiliaryRipBranch = false;
    bool useConcaveStripBranch = false;
    bool hasQFirstEqualCandidate = false;
    bool hasConcaveStripCandidate = false;
    Edge* auxiliaryRipEdge = nullptr;
    Edge* auxiliaryParallelRipEdge = nullptr;
    Point3d concaveQ3;
    Point3d concaveQ4;
    Vector3d concavePlaneNormal;
    InferredInputs fallbackSecondInputs;
    const double lengthTolerance = std::max(kPlaneTolerance, inputs.thickness * 0.01);

    for (Edge* candidateRipEdge : inputs.targetBody->GetEdges())
    {
        if (candidateRipEdge == nullptr ||
            !EdgeTouchesPoint(candidateRipEdge, inputs.endPoint) ||
            FaceHasEdge(inputs.baseFace, candidateRipEdge) ||
            candidateRipEdge->GetLength() <= inputs.thickness + lengthTolerance)
        {
            continue;
        }

        Point3d candidateFarEndpoint;
        if (!EdgeOtherPoint(candidateRipEdge, inputs.endPoint, candidateFarEndpoint))
        {
            continue;
        }

        std::vector<Edge*> remainingEdgesAtQ;
        for (Edge* edgeAtFarEndpoint : inputs.targetBody->GetEdges())
        {
            if (edgeAtFarEndpoint == nullptr ||
                edgeAtFarEndpoint == candidateRipEdge ||
                !EdgeTouchesPoint(edgeAtFarEndpoint, candidateFarEndpoint))
            {
                continue;
            }

            remainingEdgesAtQ.push_back(edgeAtFarEndpoint);
        }

        for (Edge* edgeAtFarEndpoint : remainingEdgesAtQ)
        {
            Edge* parallelEdge = nullptr;
            double minimumDistance = 0.0;
            if (EdgeHasParallelMateAtThickness(inputs.targetBody,
                                               edgeAtFarEndpoint,
                                               inputs.thickness,
                                               parallelEdge,
                                               minimumDistance))
            {
                ripEdge = candidateRipEdge;
                confirmingEdge = edgeAtFarEndpoint;
                confirmingParallelEdge = parallelEdge;
                farEndpoint = candidateFarEndpoint;
                confirmingDistance = minimumDistance;
                break;
            }
        }

        InferredInputs candidateFallbackInputs;
        Edge* fallbackFirstEdge = nullptr;
        Edge* fallbackSecondEdge = nullptr;
        if (ripEdge == nullptr)
        {
            for (std::size_t firstIndex = 0;
                 firstIndex < remainingEdgesAtQ.size() && fallbackFirstEdge == nullptr;
                 ++firstIndex)
            {
                for (std::size_t secondIndex = firstIndex + 1;
                     secondIndex < remainingEdgesAtQ.size();
                     ++secondIndex)
                {
                    Face* commonFace = FindPlanarFaceContainingEdges(
                        inputs.targetBody,
                        remainingEdgesAtQ[firstIndex],
                        remainingEdgesAtQ[secondIndex]);
                    Edge* candidateAuxiliaryRip = nullptr;
                    Edge* candidateAuxiliaryParallel = nullptr;
                    if (FindAuxiliaryRipPair(inputs.targetBody,
                                             commonFace,
                                             remainingEdgesAtQ[firstIndex],
                                             remainingEdgesAtQ[secondIndex],
                                             inputs.thickness,
                                             candidateAuxiliaryRip,
                                             candidateAuxiliaryParallel))
                    {
                        fallbackFirstEdge = remainingEdgesAtQ[firstIndex];
                        fallbackSecondEdge = remainingEdgesAtQ[secondIndex];
                        auxiliaryRipEdge = candidateAuxiliaryRip;
                        auxiliaryParallelRipEdge = candidateAuxiliaryParallel;
                        useAuxiliaryRipBranch = true;
                        break;
                    }
                    Point3d candidateQ3;
                    Point3d candidateQ4;
                    Vector3d candidatePlaneNormal;
                    InferredInputs candidateQFirstInputs;
                    const bool candidateHasQFirstInputs =
                        BuildQFirstSecondInputs(inputs,
                                               remainingEdgesAtQ[firstIndex],
                                               remainingEdgesAtQ[secondIndex],
                                               candidateFarEndpoint,
                                               candidateQFirstInputs);
                    const bool candidateHasConcaveStripPlan =
                        BuildConcaveStripPlan(inputs,
                                              remainingEdgesAtQ[firstIndex],
                                              remainingEdgesAtQ[secondIndex],
                                              candidateFarEndpoint,
                                              candidateQ3,
                                              candidateQ4,
                                              candidatePlaneNormal);
                    if (candidateHasQFirstInputs || candidateHasConcaveStripPlan)
                    {
                        fallbackFirstEdge = remainingEdgesAtQ[firstIndex];
                        fallbackSecondEdge = remainingEdgesAtQ[secondIndex];
                        if (candidateHasQFirstInputs)
                        {
                            fallbackSecondInputs = candidateQFirstInputs;
                            hasQFirstEqualCandidate = true;
                        }
                        if (candidateHasConcaveStripPlan)
                        {
                            concaveQ3 = candidateQ3;
                            concaveQ4 = candidateQ4;
                            concavePlaneNormal = candidatePlaneNormal;
                            hasConcaveStripCandidate = true;
                        }
                        break;
                    }
                    if (BuildFallbackSecondInputs(inputs,
                                                  remainingEdgesAtQ[firstIndex],
                                                  remainingEdgesAtQ[secondIndex],
                                                  candidateFarEndpoint,
                                                  candidateFallbackInputs))
                    {
                        fallbackFirstEdge = remainingEdgesAtQ[firstIndex];
                        fallbackSecondEdge = remainingEdgesAtQ[secondIndex];
                        break;
                    }
                }
            }
        }
        if (ripEdge == nullptr && fallbackFirstEdge != nullptr && fallbackSecondEdge != nullptr)
        {
            ripEdge = candidateRipEdge;
            confirmingEdge = fallbackFirstEdge;
            farEndpoint = candidateFarEndpoint;
            if (!useAuxiliaryRipBranch &&
                !hasQFirstEqualCandidate &&
                !hasConcaveStripCandidate)
            {
                fallbackSecondInputs = candidateFallbackInputs;
                useFallbackSecondUdf = true;
            }
            AppendDebugLog("TryCreateSecondPointRip fallback Q edges: first=" +
                           std::to_string(fallbackFirstEdge->Tag()) +
                           ", second=" + std::to_string(fallbackSecondEdge->Tag()) +
                           ", excludedP2Q=" + std::to_string(candidateRipEdge->Tag()));
        }
        if (ripEdge != nullptr)
        {
            break;
        }
    }

    if (ripEdge == nullptr)
    {
        AppendDebugLog("TryCreateSecondPointRip: no qualifying non-selected-face edge was found at P2.");
        return true;
    }

    // NX's edge-rip command expects both sheet-side edges of the open seam.
    // The Q edge and its parallel mate above only prove that this is a
    // thickness corner; they are not the pair passed to the rip builder.
    EdgeHasParallelMateAtThickness(inputs.targetBody,
                                   ripEdge,
                                   inputs.thickness,
                                   pairedRipEdge,
                                   pairedRipDistance);
    if (pairedRipEdge == nullptr)
    {
        errorMessage = "A matching parallel edge could not be found for the detected P2 rip edge.";
        AppendDebugLog("TryCreateSecondPointRip: " + errorMessage);
        return false;
    }

    const double ripEdgeLength = ripEdge->GetLength();
    const double pairedRipEdgeLength = pairedRipEdge->GetLength();
    const double pairedLengthTolerance =
        std::max(kPlaneTolerance, std::max(ripEdgeLength, pairedRipEdgeLength) * 1.0e-4);
    double requiredPairedRipLength = ripEdgeLength;
    double requiredLengthTolerance = pairedLengthTolerance;
    std::string selectedLengthRule = "equal";
    if (hasQFirstEqualCandidate || hasConcaveStripCandidate)
    {
        const bool equalLengthMatch =
            std::fabs(pairedRipEdgeLength - ripEdgeLength) <= pairedLengthTolerance;
        const double stripRequiredLength = ripEdgeLength + 2.0 * inputs.thickness;
        const double stripLengthTolerance =
            std::max(pairedLengthTolerance, inputs.thickness * 0.01);
        const bool stripLengthMatch =
            std::fabs(pairedRipEdgeLength - stripRequiredLength) <= stripLengthTolerance;
        if (equalLengthMatch && hasQFirstEqualCandidate)
        {
            useFallbackSecondUdf = true;
            selectedLengthRule = "Q-first-equal: second-UDF(Q,nearest), rip, primary-UDF";
        }
        else if (stripLengthMatch && hasConcaveStripCandidate)
        {
            useConcaveStripBranch = true;
            requiredPairedRipLength = stripRequiredLength;
            requiredLengthTolerance = stripLengthTolerance;
            selectedLengthRule = "concave-strip: P2Q+2*thickness";
        }
        else
        {
            std::ostringstream mismatchTrace;
            mismatchTrace << "TryCreateSecondPointRip skipped: concave P2-Q length matched no available branch"
                          << ", P2QEdge=" << ripEdge->Tag()
                          << ", P2QLength=" << ripEdgeLength
                          << ", nearestParallelEdge=" << pairedRipEdge->Tag()
                          << ", nearestParallelLength=" << pairedRipEdgeLength
                          << ", qFirstEqualCandidate=" << (hasQFirstEqualCandidate ? "true" : "false")
                          << ", stripCandidate=" << (hasConcaveStripCandidate ? "true" : "false")
                          << ", stripRequiredParallelLength=" << stripRequiredLength;
            AppendDebugLog(mismatchTrace.str());
            return true;
        }
    }
    if (std::fabs(pairedRipEdgeLength - requiredPairedRipLength) > requiredLengthTolerance)
    {
        std::ostringstream mismatchTrace;
        mismatchTrace << "TryCreateSecondPointRip skipped: P2-Q and parallel edge lengths do not satisfy branch rule"
                      << ", P2QEdge=" << ripEdge->Tag()
                      << ", P2QLength=" << ripEdgeLength
                      << ", nearestParallelEdge=" << pairedRipEdge->Tag()
                      << ", nearestParallelLength=" << pairedRipEdgeLength
                      << ", requiredParallelLength=" << requiredPairedRipLength
                      << ", lengthRule=" << selectedLengthRule
                      << ", tolerance=" << requiredLengthTolerance;
        AppendDebugLog(mismatchTrace.str());
        return true;
    }

    std::ostringstream trace;
    trace << "TryCreateSecondPointRip qualified: P2=" << FormatPoint(inputs.endPoint)
          << ", ripEdge=" << ripEdge->Tag()
          << ", ripEdgeLength=" << ripEdgeLength
          << ", pairedRipEdge=" << pairedRipEdge->Tag()
          << ", pairedRipEdgeLength=" << pairedRipEdgeLength
          << ", requiredPairedRipEdgeLength=" << requiredPairedRipLength
          << ", lengthRule=" << selectedLengthRule
          << ", pairedRipDistance=" << pairedRipDistance
          << ", farEndpoint=" << FormatPoint(farEndpoint)
          << ", confirmingEdge=" << (confirmingEdge != nullptr ? confirmingEdge->Tag() : NULL_TAG)
          << ", confirmingParallelEdge="
          << (confirmingParallelEdge != nullptr ? confirmingParallelEdge->Tag() : NULL_TAG)
          << ", minimumParallelDistance=" << confirmingDistance
          << ", thickness=" << inputs.thickness;
    AppendDebugLog(trace.str());

    if (useAuxiliaryRipBranch)
    {
        tag_t auxiliaryRipTag = NULL_TAG;
        if (!CreateSheetMetalRip(inputs,
                                 auxiliaryRipEdge,
                                 auxiliaryParallelRipEdge,
                                 true,
                                 auxiliaryRipTag,
                                 errorMessage))
        {
            return false;
        }

        // The auxiliary rip and offset alter the lengths of the P2-Q edges,
        // but NX preserves these edge objects in this workflow.  Keep the
        // original identifiers instead of attempting to rediscover them from
        // endpoints that have moved because of the offset.
        AppendDebugLog("auxiliary rip branch completed before P2-Q rip: auxiliaryRip=" +
                       std::to_string(auxiliaryRipTag) +
                       ", retainedP2Q=" + std::to_string(ripEdge->Tag()) +
                       ", retainedP2QLength=" + FormatExpressionNumber(ripEdge->GetLength()) +
                       ", retainedParallel=" + std::to_string(pairedRipEdge->Tag()) +
                       ", retainedParallelLength=" + FormatExpressionNumber(pairedRipEdge->GetLength()));
    }

    // Required operation order for the no-parallel-edge fallback:
    // second UDF first, then edge rip, then the refreshed primary UDF.
    if (useFallbackSecondUdf)
    {
        if (!CreateUserDefinedFeature(fallbackSecondInputs,
                                      errorMessage,
                                      &secondUdfTag,
                                      &secondReferenceTags,
                                      &secondToolBodyTags))
        {
            return false;
        }
        AppendDebugLog("TryCreateSecondPointRip created fallback second UDF before rip, tag=" +
                       std::to_string(secondUdfTag) +
                       ", toolBodies=" + std::to_string(secondToolBodyTags.size()));
    }

    Features::SheetMetal::EdgeRipBuilder* builder = nullptr;
    Section* section = nullptr;
    try
    {
        Part* workPart = session_->Parts()->Work();
        if (workPart == nullptr)
        {
            errorMessage = "No work part is active while creating the sheet-metal rip.";
            return false;
        }
        builder = workPart->Features()->SheetmetalManager()->CreateEdgeRipFeatureBuilder(nullptr);
        builder->SetApplicationContext(Features::SheetMetal::ApplicationContextNxSheetMetal);
        builder->Width()->SetFormula(inputs.clearanceValue);
        builder->BlendRadius()->SetFormula(inputs.bendRadiusValue);
        builder->SetEndCapShape(Features::SheetMetal::EdgeRipBuilder::EndCapShapeOptionsRound);
        builder->SetSketch(nullptr);
        builder->SetSection(nullptr);
        builder->SetRipEdges(std::vector<Edge*>());

        section = workPart->Sections()->CreateSection(9.5e-05, 0.0001, 0.5);
        builder->SetSketch(nullptr);
        builder->SetSection(section);
        builder->SetRipEdges(std::vector<Edge*>{ripEdge, pairedRipEdge});
        builder->SetUseSystemWidth(false);
        builder->SetBlendSharpCorners(false);
        builder->SetEndCapShape(Features::SheetMetal::EdgeRipBuilder::EndCapShapeOptionsRound);
        builder->SetSymmetric(true);
        builder->SetReverseWidthDirection(false);
        builder->SetParentFeatureInternal(false);

        Features::Feature* createdRip = builder->CommitFeature();
        const tag_t createdRipTag = createdRip != nullptr ? createdRip->Tag() : NULL_TAG;
        builder->Destroy();
        builder = nullptr;
        section->Destroy();
        section = nullptr;
        if (createdRipTag == NULL_TAG)
        {
            errorMessage = "NX did not return a feature after creating the sheet-metal rip.";
            return false;
        }
        if (useConcaveStripBranch)
        {
            tag_t stripSubtractTag = NULL_TAG;
            if (!CreateConcaveStripCut(inputs,
                                       concaveQ3,
                                       concaveQ4,
                                       concavePlaneNormal,
                                       stripSubtractTag,
                                       errorMessage))
            {
                return false;
            }
            AppendDebugLog("TryCreateSecondPointRip concave strip subtraction completed after P2-Q rip, feature=" +
                           std::to_string(stripSubtractTag));
        }
        ripCreated = true;
        AppendDebugLog("TryCreateSecondPointRip created edge-rip feature tag=" +
                       std::to_string(createdRipTag));
        return true;
    }
    catch (const NXException& ex)
    {
        if (builder != nullptr)
        {
            builder->Destroy();
        }
        if (section != nullptr)
        {
            section->Destroy();
        }
        errorMessage = "Failed to create the sheet-metal rip at P2.\n" + NxExceptionText(ex);
        AppendDebugLog(errorMessage);
        return false;
    }
    catch (const std::exception& ex)
    {
        if (builder != nullptr)
        {
            builder->Destroy();
        }
        if (section != nullptr)
        {
            section->Destroy();
        }
        errorMessage = std::string("Failed to create the sheet-metal rip at P2.\n") + ex.what();
        AppendDebugLog(errorMessage);
        return false;
    }
}

double TwoPointSiBianUI::EstimateSheetThickness(Body* body, Face* baseFace) const
{
    if (body == nullptr || baseFace == nullptr)
    {
        return 0.0;
    }

    double bestDistance = std::numeric_limits<double>::max();

    try
    {
        Vector3d baseNormal;
        Point3d basePlanePoint;
        if (!FacePlaneData(baseFace, basePlanePoint, baseNormal))
        {
            AppendDebugLog("EstimateSheetThickness failed to read base face plane data.");
            return 0.0;
        }

        Vector3d projectionXAxis;
        Vector3d projectionYAxis;
        if (!FaceProjectionAxes(baseFace, baseNormal, projectionXAxis, projectionYAxis))
        {
            AppendDebugLog("EstimateSheetThickness failed to build projection axes.");
            return 0.0;
        }

        const double baseArea = MeasureFaceArea(baseFace);
        const double minProjectedOverlapRatio = kThicknessProjectionOverlapRatio;
        std::vector<ProjectionPoint2d> basePolygon;
        if (!FaceProjectionPolygon(baseFace, basePlanePoint, projectionXAxis, projectionYAxis, basePolygon))
        {
            AppendDebugLog("EstimateSheetThickness failed to build base projection polygon.");
            return 0.0;
        }
        const double baseProjectedArea = PolygonArea(basePolygon);
        const double baseProjectionAreaErrorRatio =
            baseArea > kPointTolerance
                ? std::fabs(baseProjectedArea - baseArea) / baseArea
                : std::numeric_limits<double>::max();
        const bool baseProjectionIsReliable =
            baseProjectionAreaErrorRatio <= kThicknessProjectionAreaErrorRatio;

        std::ostringstream trace;
        trace << "EstimateSheetThickness baseFace=" << baseFace->Tag()
              << ", baseArea=" << baseArea
              << ", baseProjectedArea=" << baseProjectedArea
              << ", baseProjectionAreaErrorRatio=" << baseProjectionAreaErrorRatio
              << ", baseProjectionIsReliable=" << (baseProjectionIsReliable ? "true" : "false")
              << ", minProjectedOverlapRatio=" << minProjectedOverlapRatio
              << ", basePlanePoint=" << FormatPoint(basePlanePoint)
              << ", projectionX=" << FormatVector(projectionXAxis)
              << ", projectionY=" << FormatVector(projectionYAxis);

        std::vector<ThicknessCandidate> candidates;
        std::vector<Face*> faces = body->GetFaces();
        for (Face* face : faces)
        {
            if (face == nullptr || face == baseFace || face->SolidFaceType() != Face::FaceTypePlanar)
            {
                continue;
            }

            Vector3d faceNormal;
            Point3d candidatePlanePoint;
            if (!FacePlaneData(face, candidatePlanePoint, faceNormal))
            {
                continue;
            }
            const double parallel = std::fabs(Dot(baseNormal, faceNormal));
            if (parallel < 0.999)
            {
                continue;
            }

            const double signedPlaneDistance = Dot(Subtract(candidatePlanePoint, basePlanePoint), baseNormal);
            const double planeDistance = std::fabs(signedPlaneDistance);
            if (planeDistance <= kPlaneTolerance)
            {
                continue;
            }

            candidates.push_back({face, planeDistance, signedPlaneDistance});
        }

        std::sort(candidates.begin(), candidates.end(), [](const ThicknessCandidate& first, const ThicknessCandidate& second) {
            return first.planeDistance < second.planeDistance;
        });

        trace << "\n  parallelPlaneCandidates=" << candidates.size();
        for (const ThicknessCandidate& candidate : candidates)
        {
            const double candidateArea = MeasureFaceArea(candidate.face);
            const double candidateToBaseAreaRatio =
                baseArea > kPointTolerance ? candidateArea / baseArea : 0.0;
            if (candidateToBaseAreaRatio + 1.0e-9 < minProjectedOverlapRatio)
            {
                trace << "\n  skip face=" << candidate.face->Tag()
                      << " planeDistance=" << candidate.planeDistance
                      << " reason=candidate area too small"
                      << " candidateArea=" << candidateArea
                      << " candidateToBaseAreaRatio=" << candidateToBaseAreaRatio;
                continue;
            }

            double overlapArea = 0.0;
            double candidateProjectedArea = 0.0;
            const double projectedOverlapRatio = ProjectedOverlapRatioWithBase(basePolygon,
                                                                                baseProjectedArea,
                                                                                candidate.face,
                                                                                basePlanePoint,
                                                                                projectionXAxis,
                                                                                projectionYAxis,
                                                                                overlapArea,
                                                                                candidateProjectedArea);
            const double candidateProjectionAreaErrorRatio =
                candidateArea > kPointTolerance
                    ? std::fabs(candidateProjectedArea - candidateArea) / candidateArea
                    : std::numeric_limits<double>::max();
            const bool candidateProjectionIsReliable =
                candidateProjectionAreaErrorRatio <= kThicknessProjectionAreaErrorRatio;
            const bool projectionIsReliable =
                baseProjectionIsReliable && candidateProjectionIsReliable;
            const double overlapRatio =
                baseArea > kPointTolerance ? overlapArea / baseArea : projectedOverlapRatio;
            if (projectionIsReliable &&
                overlapRatio + 1.0e-9 < minProjectedOverlapRatio)
            {
                trace << "\n  skip face=" << candidate.face->Tag()
                      << " planeDistance=" << candidate.planeDistance
                      << " reason=reliable projected overlap is too small"
                      << " overlapRatio=" << overlapRatio
                      << " projectedOverlapRatio=" << projectedOverlapRatio
                      << " overlapArea=" << overlapArea
                      << " baseArea=" << baseArea
                      << " baseProjectedArea=" << baseProjectedArea
                      << " candidateArea=" << candidateArea
                      << " candidateProjectedArea=" << candidateProjectedArea;
                continue;
            }

            trace << "\n  selected candidate face=" << candidate.face->Tag()
                  << " planeDistance=" << candidate.planeDistance
                  << " signedPlaneDistance=" << candidate.signedPlaneDistance
                  << " selectionRule="
                  << (projectionIsReliable ? "projected overlap" : "nearest parallel face with sufficient area")
                  << " candidateArea=" << candidateArea
                  << " candidateToBaseAreaRatio=" << candidateToBaseAreaRatio
                  << " overlapRatio=" << overlapRatio
                  << " projectedOverlapRatio=" << projectedOverlapRatio
                  << " overlapArea=" << overlapArea
                  << " baseArea=" << baseArea
                  << " baseProjectedArea=" << baseProjectedArea
                  << " baseProjectionAreaErrorRatio=" << baseProjectionAreaErrorRatio
                  << " candidateProjectedArea=" << candidateProjectedArea
                  << " candidateProjectionAreaErrorRatio=" << candidateProjectionAreaErrorRatio;
            bestDistance = candidate.planeDistance;
            break;
        }
        trace << "\n  selected thickness="
              << (bestDistance == std::numeric_limits<double>::max() ? 0.0 : bestDistance);
        AppendDebugLog(trace.str());
    }
    catch (const NXException& ex)
    {
        AppendDebugLog("EstimateSheetThickness NXException: " + UfMessage(ex.ErrorCode()));
    }
    catch (...)
    {
        AppendDebugLog("EstimateSheetThickness unknown exception.");
    }

    return bestDistance == std::numeric_limits<double>::max() ? 0.0 : bestDistance;
}

bool TwoPointSiBianUI::CreateUserDefinedFeature(const InferredInputs& inputs,
                                                std::string& errorMessage,
                                                tag_t* createdUdfTag,
                                                std::vector<tag_t>* createdReferenceTags,
                                                std::vector<tag_t>* createdToolBodyTags) const
{
    if (inputs.featureMode == FeatureMode::NinetyRight)
    {
        errorMessage = "90 degree right UDF template is not connected yet.";
        AppendDebugLog(errorMessage);
        return false;
    }

    if (createdUdfTag != nullptr)
    {
        *createdUdfTag = NULL_TAG;
    }
    if (createdReferenceTags != nullptr)
    {
        createdReferenceTags->clear();
    }
    if (createdToolBodyTags != nullptr)
    {
        createdToolBodyTags->clear();
    }

    Part* workPart = session_->Parts()->Work();
    if (workPart == nullptr)
    {
        errorMessage = "No work part is active.";
        return false;
    }

    const tag_t workPartTag = workPart->Tag();
    std::set<tag_t> bodyTagsBeforeUdf;
    for (Body* body : *workPart->Bodies())
    {
        if (body != nullptr)
        {
            bodyTagsBeforeUdf.insert(body->Tag());
        }
    }
    std::ostringstream trace;
    trace << "==== 2P_SiBian UDF insert ====\n"
          << "workPart=" << workPartTag << "\n"
          << "start=(" << inputs.startPoint.X << "," << inputs.startPoint.Y << "," << inputs.startPoint.Z << ")\n"
          << "end=(" << inputs.endPoint.X << "," << inputs.endPoint.Y << "," << inputs.endPoint.Z << ")\n"
          << "body=" << (inputs.targetBody != nullptr ? inputs.targetBody->Tag() : NULL_TAG)
          << ", face=" << (inputs.baseFace != nullptr ? inputs.baseFace->Tag() : NULL_TAG)
          << ", startEdge=" << (inputs.startEdge != nullptr ? inputs.startEdge->Tag() : NULL_TAG)
          << ", endEdge=" << (inputs.endEdge != nullptr ? inputs.endEdge->Tag() : NULL_TAG)
          << "\nclearanceValue=" << inputs.clearanceValue
          << ", bendRadiusValue=" << inputs.bendRadiusValue;
    AppendDebugLog(trace.str());

    ExtractedTemplatePart extractedTemplate;
    tag_t templatePart = NULL_TAG;
    bool closeTemplatePart = true;
    std::string openedTemplatePath;

    std::string extractTrace = "embedded template extraction:\n";
    if (!extractedTemplate.Extract(inputs.featureMode, openedTemplatePath, extractTrace))
    {
        AppendDebugLog(extractTrace);
        errorMessage = "Failed to extract embedded UDF template.\nSee " + DebugLogPath();
        AppendDebugLog(errorMessage);
        return false;
    }
    AppendDebugLog(extractTrace);

    UF_PART_load_status_t loadStatus;
    std::memset(&loadStatus, 0, sizeof(loadStatus));
    int result = UF_PART_open_quiet(openedTemplatePath.c_str(), &templatePart, &loadStatus);
    if (loadStatus.n_parts > 0 || loadStatus.failed)
    {
        std::ostringstream loadTrace;
        loadTrace << "open embedded temp template failed=" << loadStatus.failed
                  << ", parts=" << loadStatus.n_parts;
        for (int i = 0; i < loadStatus.n_parts; ++i)
        {
            const int status = loadStatus.statuses != nullptr ? loadStatus.statuses[i] : 0;
            loadTrace << "\n  " << (loadStatus.file_names != nullptr ? loadStatus.file_names[i] : "")
                      << ": " << status << " " << UfMessage(status);
        }
        AppendDebugLog(loadTrace.str());
    }
    UF_PART_free_load_status(&loadStatus);

    auto closeTemplate = [&]() {
        if (closeTemplatePart && templatePart != NULL_TAG)
        {
            AppendDebugLog("closing opened template part tag=" + std::to_string(templatePart));
            UF_PART_close(templatePart, 0, 1);
            templatePart = NULL_TAG;
        }
    };

    if (result != 0 || templatePart == NULL_TAG)
    {
        errorMessage = "Failed to open UDF template: " + openedTemplatePath + "\n" + UfMessage(result);
        AppendDebugLog(errorMessage);
        return false;
    }

    AppendDebugLog("template part ready: tag=" + std::to_string(templatePart) +
                   ", path=" + openedTemplatePath +
                   ", closeAfterUse=" + (closeTemplatePart ? "true" : "false"));

    std::string definitionTrace;
    tag_t udfDefinition = FindUdfDefinitionFeature(templatePart, definitionTrace);
    AppendDebugLog(definitionTrace);
    if (udfDefinition == NULL_TAG)
    {
        closeTemplate();
        const UdfTemplateSpec currentTemplateSpec = TemplateSpecForMode(inputs.featureMode);
        errorMessage = "No UDF definition feature was found in " + openedTemplatePath +
                       ". Please confirm " + std::string(currentTemplateSpec.logName) +
                       " is a UG User Defined Feature template.";
        AppendDebugLog(errorMessage);
        return false;
    }
    AppendDebugLog("UDF definition selected tag=" + std::to_string(udfDefinition));

    UF_MODL_udf_exp_data_t expData;
    UF_MODL_udf_ref_data_t refData;
    UF_MODL_udf_init_exp_data(&expData);
    UF_MODL_udf_init_ref_data(&refData);
    result = UF_MODL_udf_init_insert_data_from_def(udfDefinition, &expData, &refData);
    AppendDebugLog("UF_MODL_udf_init_insert_data_from_def result=" + std::to_string(result) + " " + UfMessage(result));
    if (result != 0)
    {
        closeTemplate();
        errorMessage = "Failed to read UDF insert data from template.\n" + UfMessage(result);
        AppendDebugLog(errorMessage);
        return false;
    }

    AppendDebugLog(DescribeExpressions(expData));
    AppendDebugLog(DescribeRefs(refData));

    if (inputs.featureMode == FeatureMode::Chamfer && refData.num_refs != 4)
    {
        UF_MODL_udf_free_exp_data(&expData);
        UF_MODL_udf_free_ref_data(&refData);
        closeTemplate();
        errorMessage = "The chamfer UDF template must have exactly 4 references in order: A edge, face, a edge, B edge. Actual reference count: " +
                       std::to_string(refData.num_refs) + ".";
        AppendDebugLog(errorMessage);
        return false;
    }

    if (!AllocateUdfExpressionValues(expData,
                                     inputs.featureMode,
                                     inputs.clearanceValue,
                                     inputs.bendRadiusValue,
                                     inputs.thickness,
                                     errorMessage))
    {
        UF_MODL_udf_free_exp_data(&expData);
        UF_MODL_udf_free_ref_data(&refData);
        closeTemplate();
        AppendDebugLog(errorMessage);
        return false;
    }
    AppendDebugLog(DescribeExpressions(expData));

    NXOpen::Point* startPoint = nullptr;
    NXOpen::Point* endPoint = nullptr;
    try
    {
        WorkPartContextGuard workPartGuard(workPartTag);
        if (!workPartGuard.IsActive())
        {
            UF_MODL_udf_free_exp_data(&expData);
            UF_MODL_udf_free_ref_data(&refData);
            closeTemplate();
            errorMessage = "Failed to restore the target work part before creating UDF point references.";
            AppendDebugLog(errorMessage);
            return false;
        }

        startPoint = workPart->Points()->CreatePoint(inputs.startPoint);
        endPoint = workPart->Points()->CreatePoint(inputs.endPoint);
    }
    catch (const NXException& ex)
    {
        UF_MODL_udf_free_exp_data(&expData);
        UF_MODL_udf_free_ref_data(&refData);
        closeTemplate();
        errorMessage = "Failed to create UDF point references.\n" + NxExceptionText(ex);
        AppendDebugLog(errorMessage);
        return false;
    }

    const tag_t startPointTag = startPoint != nullptr ? startPoint->Tag() : NULL_TAG;
    const tag_t endPointTag = endPoint != nullptr ? endPoint->Tag() : NULL_TAG;
    AppendDebugLog("created point refs: start=" + std::to_string(startPointTag) +
                   ", end=" + std::to_string(endPointTag));
    if (startPointTag == NULL_TAG || endPointTag == NULL_TAG)
    {
        UF_MODL_udf_free_exp_data(&expData);
        UF_MODL_udf_free_ref_data(&refData);
        closeTemplate();
        errorMessage = "Failed to create UDF point references.";
        AppendDebugLog(errorMessage);
        return false;
    }

    std::vector<tag_t> createdRefs;
    createdRefs.push_back(startPointTag);
    createdRefs.push_back(endPointTag);
    auto deleteCreatedRefsOnFailure = [&]() {
        for (tag_t referenceTag : createdRefs)
        {
            if (referenceTag == NULL_TAG)
            {
                continue;
            }

            const int deleteResult = UF_OBJ_delete_object(referenceTag);
            AppendDebugLog("failure cleanup delete reference tag=" + std::to_string(referenceTag) +
                           ", result=" + std::to_string(deleteResult) +
                           " " + UfMessage(deleteResult));
        }
    };

    if (refData.num_refs > 0)
    {
        refData.new_refs = static_cast<tag_t*>(
            AllocateUfMemory(sizeof(tag_t) * static_cast<std::size_t>(refData.num_refs),
                             "refData.new_refs"));
        if (refData.reverse_refs_dir == nullptr)
        {
            refData.reverse_refs_dir = static_cast<UF_MODL_udf_reverse_dir_t*>(
                AllocateUfMemory(sizeof(UF_MODL_udf_reverse_dir_t) * static_cast<std::size_t>(refData.num_refs),
                                 "refData.reverse_refs_dir"));
        }
        if (refData.new_refs == nullptr || refData.reverse_refs_dir == nullptr)
        {
            UF_MODL_udf_free_exp_data(&expData);
            UF_MODL_udf_free_ref_data(&refData);
            closeTemplate();
            deleteCreatedRefsOnFailure();
            errorMessage = "Failed to allocate UDF reference mapping data.";
            AppendDebugLog(errorMessage);
            return false;
        }
    }

    tag_t* oldParents = nullptr;
    char** parentPrompts = nullptr;
    int parentCount = 0;
    tag_t* expressions = nullptr;
    char** expressionPrompts = nullptr;
    int expressionCount = 0;
    const int askDefResult = UF_MODL_ask_udf_definition(
        udfDefinition,
        &oldParents,
        &parentPrompts,
        &parentCount,
        &expressions,
        &expressionPrompts,
        &expressionCount);
    std::ostringstream promptTrace;
    promptTrace << "UF_MODL_ask_udf_definition result=" << askDefResult << " " << UfMessage(askDefResult)
                << ", parentCount=" << parentCount
                << ", expressionCount=" << expressionCount;
    for (int index = 0; index < parentCount; ++index)
    {
        promptTrace << "\n  parent[" << index << "] tag="
                    << (oldParents != nullptr ? oldParents[index] : NULL_TAG)
                    << " prompt=\""
                    << (parentPrompts != nullptr && parentPrompts[index] != nullptr ? parentPrompts[index] : "")
                    << "\"";
    }
    for (int index = 0; index < expressionCount; ++index)
    {
        promptTrace << "\n  expressionPrompt[" << index << "] tag="
                    << (expressions != nullptr ? expressions[index] : NULL_TAG)
                    << " prompt=\""
                    << (expressionPrompts != nullptr && expressionPrompts[index] != nullptr ? expressionPrompts[index] : "")
                    << "\"";
    }
    AppendDebugLog(promptTrace.str());

    int pointOrdinal = 0;
    int edgeOrdinal = 0;
    int sketchExternalOrdinal = 0;
    std::ostringstream mapTrace;
    mapTrace << "reference mapping:";
    for (int index = 0; index < refData.num_refs; ++index)
    {
        std::string prompt;
        if (parentPrompts != nullptr && index < parentCount && parentPrompts[index] != nullptr)
        {
            prompt = parentPrompts[index];
        }

        tag_t matched = MatchReference(
            prompt,
            refData.old_refs != nullptr ? refData.old_refs[index] : NULL_TAG,
            startPointTag,
            endPointTag,
            inputs.targetBody != nullptr ? inputs.targetBody->Tag() : NULL_TAG,
            inputs.baseFace != nullptr ? inputs.baseFace->Tag() : NULL_TAG,
            inputs.startPositiveYEdge != nullptr ? inputs.startPositiveYEdge->Tag() : NULL_TAG,
            inputs.startNegativeYEdge != nullptr ? inputs.startNegativeYEdge->Tag() : NULL_TAG,
            inputs.endPositiveYEdge != nullptr ? inputs.endPositiveYEdge->Tag() : NULL_TAG,
            inputs.endNegativeYEdge != nullptr ? inputs.endNegativeYEdge->Tag() : NULL_TAG,
            inputs.startEdge != nullptr ? inputs.startEdge->Tag() : NULL_TAG,
            inputs.endEdge != nullptr ? inputs.endEdge->Tag() : NULL_TAG,
            pointOrdinal,
            edgeOrdinal,
            sketchExternalOrdinal);

        if (inputs.featureMode == FeatureMode::Chamfer && index >= 0 && index <= 3)
        {
            const tag_t chamferRefs[] = {
                inputs.startPositiveYEdge != nullptr ? inputs.startPositiveYEdge->Tag() : NULL_TAG,
                inputs.baseFace != nullptr ? inputs.baseFace->Tag() : NULL_TAG,
                inputs.endPositiveYEdge != nullptr ? inputs.endPositiveYEdge->Tag() : NULL_TAG,
                inputs.startNegativeYEdge != nullptr ? inputs.startNegativeYEdge->Tag() : NULL_TAG};
            matched = chamferRefs[index];
            const UF_MODL_udf_reverse_dir_t chamferDirs[] = {
                EdgeDirectionWithStartPoint(inputs.startPositiveYEdge, inputs.startPoint, "A(P1,Y+) start=P1"),
                UF_MODL_UDF_KEEP_DIR,
                EdgeDirectionWithStartPoint(inputs.endPositiveYEdge, inputs.endPoint, "a(P2,Y+) start=P2"),
                EdgeDirectionWithEndPoint(inputs.startNegativeYEdge, inputs.startPoint, "B(P1,Y-) end=P1")};
            refData.reverse_refs_dir[index] = chamferDirs[index];
        }
        else if (inputs.featureMode == FeatureMode::NinetyLeft && index >= 0 && index <= 5)
        {
            const tag_t ninetyLeftRefs[] = {
                startPointTag,
                inputs.startPositiveYEdge != nullptr ? inputs.startPositiveYEdge->Tag() : NULL_TAG,
                inputs.baseFace != nullptr ? inputs.baseFace->Tag() : NULL_TAG,
                endPointTag,
                inputs.endPositiveYEdge != nullptr ? inputs.endPositiveYEdge->Tag() : NULL_TAG,
                inputs.startNegativeYEdge != nullptr ? inputs.startNegativeYEdge->Tag() : NULL_TAG};
            matched = ninetyLeftRefs[index];
            refData.reverse_refs_dir[index] = UF_MODL_UDF_KEEP_DIR;
        }
        else
        {
            refData.reverse_refs_dir[index] = UF_MODL_UDF_KEEP_DIR;
        }

        refData.new_refs[index] = matched;
        mapTrace << "\n  ref[" << index << "] prompt=\"" << prompt << "\" old="
                 << (refData.old_refs != nullptr ? refData.old_refs[index] : NULL_TAG)
                 << " -> new=" << matched
                 << " dir=" << (refData.reverse_refs_dir[index] == UF_MODL_UDF_REVERSE_DIR ? "REVERSE" : "KEEP");
    }
    AppendDebugLog(mapTrace.str());

    if (oldParents != nullptr)
    {
        UF_free(oldParents);
    }
    if (parentPrompts != nullptr)
    {
        UF_free_string_array(parentCount, parentPrompts);
    }
    if (expressions != nullptr)
    {
        UF_free(expressions);
    }
    if (expressionPrompts != nullptr)
    {
        UF_free_string_array(expressionCount, expressionPrompts);
    }

    for (int index = 0; index < refData.num_refs; ++index)
    {
        if (refData.new_refs[index] == NULL_TAG)
        {
            UF_MODL_udf_free_exp_data(&expData);
            UF_MODL_udf_free_ref_data(&refData);
            closeTemplate();
            deleteCreatedRefsOnFailure();
            errorMessage = "A required UDF reference could not be matched. See " + DebugLogPath();
            AppendDebugLog(errorMessage);
            return false;
        }
    }

    tag_t newUdf = NULL_TAG;
    {
        WorkPartContextGuard workPartGuard(workPartTag);
        if (!workPartGuard.IsActive())
        {
            UF_MODL_udf_free_exp_data(&expData);
            UF_MODL_udf_free_ref_data(&refData);
            closeTemplate();
            errorMessage = "Failed to restore the target work part before UDF instantiation.";
            AppendDebugLog(errorMessage);
            return false;
        }

        AppendDebugLog("calling UF_MODL_create_instantiated_udf1 with def=" + std::to_string(udfDefinition) +
                       ", refs=" + std::to_string(refData.num_refs) +
                       ", exps=" + std::to_string(expData.num_exps));
        result = UF_MODL_create_instantiated_udf1(udfDefinition, &expData, &refData, &newUdf);
        AppendDebugLog("UF_MODL_create_instantiated_udf1 result=" + std::to_string(result) +
                       " " + UfMessage(result) +
                       ", newUdf=" + std::to_string(newUdf));
    }

    UF_MODL_udf_free_exp_data(&expData);
    UF_MODL_udf_free_ref_data(&refData);
    closeTemplate();

    if (result != 0 || newUdf == NULL_TAG)
    {
        if (newUdf != NULL_TAG)
        {
            const int deleteResult = UF_OBJ_delete_object(newUdf);
            AppendDebugLog("failure cleanup delete partial udf tag=" + std::to_string(newUdf) +
                           ", result=" + std::to_string(deleteResult) +
                           " " + UfMessage(deleteResult));
        }
        deleteCreatedRefsOnFailure();
        errorMessage = "UDF instantiation failed.\n" + UfMessage(result) + "\nSee " + DebugLogPath();
        AppendDebugLog(errorMessage);
        return false;
    }

    std::vector<Body*> createdToolBodies;
    for (Body* body : *workPart->Bodies())
    {
        if (body != nullptr &&
            body->Tag() != (inputs.targetBody != nullptr ? inputs.targetBody->Tag() : NULL_TAG) &&
            bodyTagsBeforeUdf.find(body->Tag()) == bodyTagsBeforeUdf.end())
        {
            createdToolBodies.push_back(body);
        }
    }

    if (createdToolBodies.empty())
    {
        Features::Feature* udfFeature = dynamic_cast<Features::Feature*>(NXObjectManager::Get(newUdf));
        if (udfFeature != nullptr)
        {
            for (Body* body : udfFeature->GetBodies())
            {
                if (body != nullptr &&
                    body->Tag() != (inputs.targetBody != nullptr ? inputs.targetBody->Tag() : NULL_TAG))
                {
                    createdToolBodies.push_back(body);
                }
            }
        }
    }

    if (inputs.targetBody == nullptr || createdToolBodies.empty())
    {
        UF_OBJ_delete_object(newUdf);
        deleteCreatedRefsOnFailure();
        errorMessage = "The UDF was created, but its independent tool body could not be found.";
        AppendDebugLog(errorMessage);
        return false;
    }

    if (createdUdfTag != nullptr)
    {
        *createdUdfTag = newUdf;
    }
    if (createdReferenceTags != nullptr)
    {
        *createdReferenceTags = createdRefs;
    }
    if (createdToolBodyTags != nullptr)
    {
        for (Body* toolBody : createdToolBodies)
        {
            if (toolBody != nullptr)
            {
                createdToolBodyTags->push_back(toolBody->Tag());
            }
        }
    }
    AppendDebugLog("created UDF instance tag=" + std::to_string(newUdf) +
                   ", deferred tool body count=" + std::to_string(createdToolBodies.size()));
    return true;
}

bool TwoPointSiBianUI::SubtractToolBodies(Body* targetBody,
                                           const std::vector<tag_t>& toolBodyTags,
                                           tag_t& resultFeatureTag,
                                           std::string& errorMessage) const
{
    resultFeatureTag = NULL_TAG;
    if (targetBody == nullptr || toolBodyTags.empty())
    {
        errorMessage = "No target body or UDF tool body is available for the final subtraction.";
        return false;
    }

    Part* workPart = session_->Parts()->Work();
    if (workPart == nullptr)
    {
        errorMessage = "No work part is active during the final subtraction.";
        return false;
    }

    try
    {
        for (tag_t toolTag : toolBodyTags)
        {
            Body* toolBody = dynamic_cast<Body*>(NXObjectManager::Get(toolTag));
            if (toolBody == nullptr || toolBody == targetBody)
            {
                errorMessage = "A deferred UDF tool body is no longer available for subtraction.";
                return false;
            }

            Features::BooleanBuilder* builder = workPart->Features()->CreateBooleanBuilder(nullptr);
            builder->SetOperation(Features::Feature::BooleanTypeSubtract);
            builder->SetTarget(targetBody);
#pragma warning(push)
#pragma warning(disable : 4996)
            builder->SetTool(toolBody);
#pragma warning(pop)
            builder->SetRetainTarget(false);
            builder->SetRetainTool(false);
            NXObject* result = builder->Commit();
            resultFeatureTag = result != nullptr ? result->Tag() : NULL_TAG;
            builder->Destroy();
            if (resultFeatureTag == NULL_TAG)
            {
                errorMessage = "NX returned no feature from the final Boolean Subtract.";
                return false;
            }
            AppendDebugLog("final deferred subtraction: target=" + std::to_string(targetBody->Tag()) +
                           ", tool=" + std::to_string(toolTag) +
                           ", resultFeature=" + std::to_string(resultFeatureTag));
        }
    }
    catch (const NXException& ex)
    {
        errorMessage = "Final subtraction of the UDF tool bodies failed.\n" + NxExceptionText(ex);
        AppendDebugLog(errorMessage);
        return false;
    }
    return resultFeatureTag != NULL_TAG;
}

void TwoPointSiBianUI::ConfigurePointSelection(NXOpen::BlockStyler::SelectObject* block) const
{
    if (block == nullptr)
    {
        return;
    }

    NXOpen::BlockStyler::PropertyList* properties = block->GetProperties();
    if (properties == nullptr)
    {
        return;
    }

    try
    {
        properties->SetEnum("StepStatus", 1);
        properties->SetEnum("SelectMode", 0);
        std::vector<NXOpen::Selection::MaskTriple> masks;
        masks.emplace_back(UF_point_type, UF_point_subtype, 0);
        masks.emplace_back(UF_solid_type, UF_solid_body_subtype, UF_UI_SEL_FEATURE_ANY_EDGE);
        masks.emplace_back(UF_solid_type, UF_solid_face_subtype, UF_UI_SEL_FEATURE_ANY_FACE);
        properties->SetSelectionFilter(
            "SelectionFilter",
            NXOpen::Selection::SelectionActionClearAndEnableSpecific,
            masks);
    }
    catch (...)
    {
    }

    delete properties;
}

void TwoPointSiBianUI::ShowError(const std::string& message) const
{
    AppendDebugLog("ShowError: " + message);
    ui_->NXMessageBox()->Show(zhihui_twopoint_sibian::kFeatureDisplayName,
                              NXMessageBox::DialogTypeError,
                              message.c_str());
}

