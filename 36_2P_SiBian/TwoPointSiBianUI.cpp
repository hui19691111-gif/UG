#include "TwoPointSiBianUI.hpp"

#include <NXOpen/BlockStyler_Enumeration.hxx>
#include <NXOpen/BlockStyler_PropertyList.hxx>
#include <NXOpen/BlockStyler_StringBlock.hxx>
#include <NXOpen/Direction.hxx>
#include <NXOpen/DirectionCollection.hxx>
#include <NXOpen/Features_CustomAttribute.hxx>
#include <NXOpen/Features_CustomAttributeCollection.hxx>
#include <NXOpen/Features_CustomDoubleAttribute.hxx>
#include <NXOpen/Features_CustomFeatureBuilder.hxx>
#include <NXOpen/Features_CustomFeatureData.hxx>
#include <NXOpen/Features_CustomFeatureDataCollection.hxx>
#include <NXOpen/Features_CustomTagAttribute.hxx>
#include <NXOpen/Features_FeatureCollection.hxx>
#include <NXOpen/MeasureDistance.hxx>
#include <NXOpen/MeasureFaces.hxx>
#include <NXOpen/MeasureManager.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXMessageBox.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/BodyCollection.hxx>
#include <NXOpen/Point.hxx>
#include <NXOpen/PointCollection.hxx>
#include <NXOpen/TaggedObject.hxx>
#include <NXOpen/Unit.hxx>
#include <NXOpen/UnitCollection.hxx>

#include <uf.h>
#include <uf_assem.h>
#include <uf_modl.h>
#include <uf_modl_expressions_retiring.h>
#include <uf_modl_udf.h>
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
#include <string>
#include <ctime>
#include <vector>

using namespace NXOpen;

namespace
{
constexpr double kPointTolerance = 1.0e-4;
constexpr double kPlaneTolerance = 1.0e-3;
constexpr const char* kTemplatePartName = "2p_SiBian_1.prt";
constexpr const char* kTemplate90LeftPartName = "2P_SiBian_90R.prt";
constexpr const wchar_t* kTempTemplateRoot = L"ZhihuiSheetMetal\\UDF\\36_2P_SiBian";

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
        if (ContainsAny(expressionName, {"板厚", "banhou", "thickness", "sheet"}))
        {
            value = sheetThicknessValue;
        }
        else if (ContainsAny(expressionName, {"折弯r", "折弯R", "bend", "radius", "r"}))
        {
            value = bendRadiusValue;
        }
        else if (ContainsAny(expressionName, {"间隙", "gap", "clearance"}))
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
        members.emplace_back("斜角", NXString::UTF8);
        members.emplace_back("90度左", NXString::UTF8);
        members.emplace_back("90度右", NXString::UTF8);
        featureModeBlock_->SetEnumMembers(members);
        featureModeBlock_->SetValueAsString(NXString("斜角", NXString::UTF8));
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
        if (mode == "90度左")
        {
            return FeatureMode::NinetyLeft;
        }
        if (mode == "90度右")
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
    return ReadSelectedPoint(startPointBlock_, startObject, startPoint) &&
           ReadSelectedPoint(endPointBlock_, endObject, endPoint) &&
           Distance(startPoint, endPoint) > kPointTolerance;
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
            return ok ? 0 : 1;
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

    InferredInputs inputs;
    if (!ReadInputs(inputs))
    {
        AppendDebugLog("CreatePreview ReadInputs failed.");
        return false;
    }

    previewUndoMark_ = session_->SetUndoMark(Session::MarkVisibilityInvisible, "2P_SiBian Preview");
    std::string errorMessage;
    tag_t createdUdfTag = NULL_TAG;
    std::vector<tag_t> createdReferenceTags;
    if (!CreateUserDefinedFeature(inputs, errorMessage, &createdUdfTag, &createdReferenceTags))
    {
        UndoPreview();
        ShowError(errorMessage);
        return false;
    }

    previewUdfTag_ = createdUdfTag;
    previewReferenceTags_ = createdReferenceTags;
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

    if (previewUdfTag_ != NULL_TAG)
    {
        const int deleteResult = UF_OBJ_delete_object(previewUdfTag_);
        AppendDebugLog("UndoPreview delete udf tag=" + std::to_string(previewUdfTag_) +
                       ", result=" + std::to_string(deleteResult) +
                       " " + UfMessage(deleteResult));
        previewUdfTag_ = NULL_TAG;
    }

    for (tag_t referenceTag : previewReferenceTags_)
    {
        if (referenceTag == NULL_TAG)
        {
            continue;
        }

        const int deleteResult = UF_OBJ_delete_object(referenceTag);
        AppendDebugLog("UndoPreview delete reference tag=" + std::to_string(referenceTag) +
                       ", result=" + std::to_string(deleteResult) +
                       " " + UfMessage(deleteResult));
    }
    previewReferenceTags_.clear();

    try
    {
        AppendDebugLog("UndoPreview undoing mark=" + std::to_string(static_cast<int>(previewUndoMark_)));
        session_->UndoToMark(previewUndoMark_, "2P_SiBian Preview");
        session_->DeleteUndoMark(previewUndoMark_, "2P_SiBian Preview");
    }
    catch (const NXException& ex)
    {
        AppendDebugLog("UndoPreview NXException: " + UfMessage(ex.ErrorCode()));
    }
    catch (...)
    {
        AppendDebugLog("UndoPreview unknown exception");
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

    if (!ReadSelectedPoint(startPointBlock_, inputs.startObject, inputs.startPoint) ||
        !ReadSelectedPoint(endPointBlock_, inputs.endObject, inputs.endPoint))
    {
        return false;
    }

    if (Distance(inputs.startPoint, inputs.endPoint) <= kPointTolerance)
    {
        return false;
    }

    inputs.targetBody = FindBody(inputs.startObject);
    if (inputs.targetBody == nullptr)
    {
        inputs.targetBody = FindBody(inputs.endObject);
    }

    if (inputs.targetBody != nullptr)
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
    inputs.thickness = EstimateSheetThickness(inputs.targetBody, inputs.baseFace);
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

    Part* workPart = session_->Parts()->Work();
    if (workPart == nullptr)
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
            if (!FaceNormal(face, faceNormal))
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

            MeasureDistance* distance = workPart->MeasureManager()->NewDistance(
                nullptr,
                MeasureManager::MeasureTypeMinimum,
                baseFace,
                face);
            if (distance == nullptr)
            {
                continue;
            }

            const double value = distance->Value();
            delete distance;
            if (value <= kPointTolerance || value >= bestDistance)
            {
                continue;
            }

            double facePoint[3] = {0.0, 0.0, 0.0};
            int faceType = 0;
            double faceDirection[3] = {0.0, 0.0, 0.0};
            double faceBox[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
            double faceRadius = 0.0;
            double faceRadData = 0.0;
            int normalDirection = 1;
            const int faceDataResult = UF_MODL_ask_face_data(face->Tag(),
                                                             &faceType,
                                                             facePoint,
                                                             faceDirection,
                                                             faceBox,
                                                             &faceRadius,
                                                             &faceRadData,
                                                             &normalDirection);
            if (faceDataResult != 0)
            {
                AppendDebugLog("OrientNormalAwayFromOppositeFace UF_MODL_ask_face_data failed for candidate: " +
                               std::to_string(faceDataResult) + " " + UfMessage(faceDataResult));
                continue;
            }

            bestFace = face;
            bestDistance = value;
            bestVector = Vector3d(facePoint[0] - pointOnFace.X,
                                  facePoint[1] - pointOnFace.Y,
                                  facePoint[2] - pointOnFace.Z);
            trace << "\n  candidate opposite face=" << face->Tag()
                  << " area=" << area
                  << " parallel=" << parallel
                  << " distance=" << value
                  << " facePoint=" << FormatTriple(facePoint)
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

double TwoPointSiBianUI::EstimateSheetThickness(Body* body, Face* baseFace) const
{
    if (body == nullptr || baseFace == nullptr)
    {
        return 0.0;
    }

    Part* workPart = session_->Parts()->Work();
    double bestDistance = std::numeric_limits<double>::max();

    try
    {
        Vector3d baseNormal;
        if (!FaceNormal(baseFace, baseNormal))
        {
            AppendDebugLog("EstimateSheetThickness failed to read base face normal.");
            return 0.0;
        }

        const double baseArea = MeasureFaceArea(baseFace);
        const double minParallelArea = baseArea * 0.60;
        std::ostringstream trace;
        trace << "EstimateSheetThickness baseFace=" << baseFace->Tag()
              << ", baseArea=" << baseArea
              << ", minParallelArea=" << minParallelArea;

        std::vector<Face*> faces = body->GetFaces();
        for (Face* face : faces)
        {
            if (face == nullptr || face == baseFace || face->SolidFaceType() != Face::FaceTypePlanar)
            {
                continue;
            }

            Vector3d faceNormal;
            if (!FaceNormal(face, faceNormal))
            {
                continue;
            }
            const double parallel = std::fabs(Dot(baseNormal, faceNormal));
            if (parallel < 0.999)
            {
                trace << "\n  skip face=" << face->Tag() << " parallel=" << parallel;
                continue;
            }

            const double area = MeasureFaceArea(face);
            if (baseArea > kPointTolerance && area < minParallelArea)
            {
                trace << "\n  skip face=" << face->Tag()
                      << " area=" << area
                      << " parallel=" << parallel;
                continue;
            }

            MeasureDistance* distance = workPart->MeasureManager()->NewDistance(
                nullptr,
                MeasureManager::MeasureTypeMinimum,
                baseFace,
                face);
            if (distance == nullptr)
            {
                continue;
            }

            const double value = distance->Value();
            delete distance;
            trace << "\n  candidate face=" << face->Tag()
                  << " area=" << area
                  << " parallel=" << parallel
                  << " distance=" << value;
            if (value > kPointTolerance && value < bestDistance)
            {
                bestDistance = value;
            }
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
                                                std::vector<tag_t>* createdReferenceTags) const
{
    if (inputs.featureMode == FeatureMode::NinetyRight)
    {
        errorMessage = "90度右模板尚未接入，请先提供对应的UDF模板PRT。";
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

    Part* workPart = session_->Parts()->Work();
    if (workPart == nullptr)
    {
        errorMessage = "No work part is active.";
        return false;
    }

    const tag_t workPartTag = workPart->Tag();
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

    if (createdUdfTag != nullptr)
    {
        *createdUdfTag = newUdf;
    }
    if (createdReferenceTags != nullptr)
    {
        *createdReferenceTags = createdRefs;
    }
    AppendDebugLog("created UDF instance tag=" + std::to_string(newUdf));
    return true;
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
