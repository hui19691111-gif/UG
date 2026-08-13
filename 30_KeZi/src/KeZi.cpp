#include <NXOpen/BasePart.hxx>
#include <NXOpen/Body.hxx>
#include <NXOpen/BodyCollection.hxx>
#include <NXOpen/Arc.hxx>
#include <NXOpen/Builder.hxx>
#include <NXOpen/Callback.hxx>
#include <NXOpen/ColorManager.hxx>
#include <NXOpen/Direction.hxx>
#include <NXOpen/DirectionCollection.hxx>
#include <NXOpen/Curve.hxx>
#include <NXOpen/CurveCollection.hxx>
#include <NXOpen/IBaseCurve.hxx>
#include <NXOpen/DisplayManager.hxx>
#include <NXOpen/DisplayModification.hxx>
#include <NXOpen/DisplayableObject.hxx>
#include <NXOpen/Expression.hxx>
#include <NXOpen/Edge.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/FaceDumbRule.hxx>
#include <NXOpen/FontCollection.hxx>
#include <NXOpen/Features_TextBuilder.hxx>
#include <NXOpen/Features_ExtrudeBuilder.hxx>
#include <NXOpen/Features_BooleanBuilder.hxx>
#include <NXOpen/Features_ConstructionFeatureData.hxx>
#include <NXOpen/Features_CustomAttribute.hxx>
#include <NXOpen/Features_CustomAttributeCollection.hxx>
#include <NXOpen/Features_CustomDoubleAttribute.hxx>
#include <NXOpen/Features_CustomFeature.hxx>
#include <NXOpen/Features_CustomFeatureBuilder.hxx>
#include <NXOpen/Features_CustomFeatureClass.hxx>
#include <NXOpen/Features_CustomFeatureClassManager.hxx>
#include <NXOpen/Features_CustomFeatureData.hxx>
#include <NXOpen/Features_CustomFeatureDataCollection.hxx>
#include <NXOpen/Features_CustomFeaturePreUpdateEvent.hxx>
#include <NXOpen/Features_CustomIntegerAttribute.hxx>
#include <NXOpen/Features_CustomStringAttribute.hxx>
#include <NXOpen/Features_CustomTagAttribute.hxx>
#include <NXOpen/Features_EditWithRollbackManager.hxx>
#include <NXOpen/Features_FeatureCollection.hxx>
#include <NXOpen/Features_Text.hxx>
#include <NXOpen/GeometricUtilities_BooleanOperation.hxx>
#include <NXOpen/GeometricUtilities_Extend.hxx>
#include <NXOpen/GeometricUtilities_FeatureOffset.hxx>
#include <NXOpen/GeometricUtilities_FeatureOptions.hxx>
#include <NXOpen/GeometricUtilities_Limits.hxx>
#include <NXOpen/GeometricUtilities_MultiDraft.hxx>
#include <NXOpen/GeometricUtilities_RectangularFrameBuilder.hxx>
#include <NXOpen/GeometricUtilities_SimpleDraft.hxx>
#include <NXOpen/GeometricUtilities_SmartVolumeProfileBuilder.hxx>
#include <NXOpen/NXColor.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/ListingWindow.hxx>
#include <NXOpen/Measurement.hxx>
#include <NXOpen/Line.hxx>
#include <NXOpen/NXMessageBox.hxx>
#include <NXOpen/NXObject.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/NXString.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/Point.hxx>
#include <NXOpen/PointCollection.hxx>
#include <NXOpen/SmartObject.hxx>
#include <NXOpen/ScCollector.hxx>
#include <NXOpen/ScCollectorCollection.hxx>
#include <NXOpen/ScRuleFactory.hxx>
#include <NXOpen/Section.hxx>
#include <NXOpen/SectionCollection.hxx>
#include <NXOpen/Selection.hxx>
#include <NXOpen/SelectionIntentRule.hxx>
#include <NXOpen/SelectionIntentRuleOptions.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/TaggedObject.hxx>
#include <NXOpen/CurveDumbRule.hxx>
#include <NXOpen/BodyDumbRule.hxx>
#include <NXOpen/Tooling_InsertTextBuilder.hxx>
#include <NXOpen/Tooling_MoldwizardManager.hxx>
#include <NXOpen/Tooling_ToolingManager.hxx>
#include <NXOpen/Tooling_ToolingSession.hxx>
#include <NXOpen/UI.hxx>
#include <NXOpen/Update.hxx>
#include <NXOpen/View.hxx>
#include <NXOpen/ViewCollection.hxx>
#include <NXOpen/ugmath.hxx>

#include <NXOpen/BlockStyler_BlockDialog.hxx>
#include <NXOpen/BlockStyler_Button.hxx>
#include <NXOpen/BlockStyler_CompositeBlock.hxx>
#include <NXOpen/BlockStyler_DoubleBlock.hxx>
#include <NXOpen/BlockStyler_Enumeration.hxx>
#include <NXOpen/BlockStyler_IntegerBlock.hxx>
#include <NXOpen/BlockStyler_ObjectColorPicker.hxx>
#include <NXOpen/BlockStyler_PropertyList.hxx>
#include <NXOpen/BlockStyler_SelectObject.hxx>
#include <NXOpen/BlockStyler_SpecifyOrientation.hxx>
#include <NXOpen/BlockStyler_StringBlock.hxx>
#include <NXOpen/BlockStyler_Toggle.hxx>
#include <NXOpen/BlockStyler_UIBlock.hxx>

#include <NXOpen/Assemblies_ComponentAssembly.hxx>
#include <NXOpen/Assemblies_Component.hxx>
#include <NXOpen/Assemblies_HideComponentBuilder.hxx>
#include <NXOpen/Assemblies_ReplaceComponentBuilder.hxx>
#include <NXOpen/Assemblies_AssemblyManager.hxx>
#include <NXOpen/ErrorList.hxx>
#include <NXOpen/PartSaveStatus.hxx>
#include <NXOpen/SelectDisplayableObjectList.hxx>
#include <NXOpen/SelectTaggedObjectList.hxx>

#include <uf.h>
#include <uf_assem.h>
#include <uf_eval.h>
#include <uf_disp.h>
#include <uf_drf.h>
#include <uf_modl.h>
#include <uf_modl_types.h>
#include <uf_obj.h>
#include <uf_error_bases.h>
#include <uf_ugfont.h>
#include <uf_object_types.h>
#include <uf_part.h>
#include <uf_ui_types.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <mutex>
#include <memory>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "../../../common/ZhihuiEmbeddedDialog.hpp"
#include "../../../protection/native/ZhihuiLicenseGuard.hpp"
#include "KeZiCustomFeatureShared.hpp"
#include "../resource.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>

#pragma comment(lib, "Comdlg32.lib")

#ifdef CreateDialog
#undef CreateDialog
#endif

#ifndef DllExport
#define DllExport __declspec(dllexport)
#endif

using namespace NXOpen;
using namespace NXOpen::BlockStyler;

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace
{
constexpr double kVectorTolerance = 1.0e-6;
constexpr double kSameBodyLengthTolerance = 0.05;
constexpr double kSameBodyMassTolerance = 0.05;
constexpr double kSameBodyDistanceTolerance = 0.05;
constexpr double kSameBodyFaceAreaAbsoluteTolerance = 0.05;
constexpr double kSameBodyFaceAreaRelativeTolerance = 0.0001;

struct SameBodyPoint3
{
    double x;
    double y;
    double z;
};

struct RenderedFontSegment
{
    Point3d start;
    Point3d end;
};

struct RenderedFontArc
{
    Point3d center;
    double radius;
    double startAngle;
    double endAngle;
};

struct NxFontRenderData
{
    std::vector<RenderedFontSegment> segments;
    std::vector<RenderedFontArc> arcs;
};

UF_DRF_render_text_status_t FontBeginLine(void*) { return UF_DRF_RENDER_OK; }
UF_DRF_render_text_status_t FontEndLine(void*) { return UF_DRF_RENDER_OK; }
UF_DRF_render_text_status_t FontSetPosition(double inPoint[3], void*, double outPoint[3], logical* outStatus)
{
    std::copy(inPoint, inPoint + 3, outPoint);
    *outStatus = false;
    return UF_DRF_RENDER_OK;
}
UF_DRF_render_text_status_t FontDrawPosition(double inPoint[3], double lastPoint[3], logical, void* client,
                                              double outPoint[3], logical* outStatus)
{
    auto* data = static_cast<NxFontRenderData*>(client);
    data->segments.push_back({Point3d(lastPoint[0], lastPoint[1], lastPoint[2]),
                              Point3d(inPoint[0], inPoint[1], inPoint[2])});
    std::copy(inPoint, inPoint + 3, outPoint);
    *outStatus = true;
    return UF_DRF_RENDER_OK;
}
UF_DRF_render_text_status_t FontDrawArc(double center[3], double radius, double startAngle,
                                         double endAngle, void* client)
{
    static_cast<NxFontRenderData*>(client)->arcs.push_back(
        {Point3d(center[0], center[1], center[2]), radius, startAngle, endAngle});
    return UF_DRF_RENDER_OK;
}
UF_DRF_render_text_status_t FontDrawChar(double[3], unsigned char, void*)
{
    return UF_DRF_RENDER_CANNOT_RENDER_CHAR;
}
UF_DRF_render_text_status_t FontDrawStandard(const char*, const double[3], int, double, double, double,
                                               double, logical, logical, logical, logical, void*)
{
    return UF_DRF_RENDER_NOT_DRAWN;
}
UF_DRF_render_text_status_t FontDrawSymbol(char*, double[3], void*, void*)
{
    return UF_DRF_RENDER_CANNOT_RENDER_SYMBOL;
}
UF_DRF_render_text_status_t FontSetCfw(UF_DRF_cfw_p_t, void*) { return UF_DRF_RENDER_OK; }
UF_DRF_render_text_status_t FontPushOrientation(double[9], void*) { return UF_DRF_RENDER_OK; }
UF_DRF_render_text_status_t FontPopOrientation(void*) { return UF_DRF_RENDER_OK; }
UF_DRF_render_text_status_t FontFillRegion(int, double*, double, double lastPoint[3], logical* drawn, void*)
{
    lastPoint[0] = lastPoint[1] = lastPoint[2] = 0.0;
    *drawn = false;
    return UF_DRF_RENDER_NOT_DRAWN;
}

struct SameBodyLengthBucket
{
    double length;
    int count;
};

struct SameBodyFrame3
{
    SameBodyPoint3 origin;
    SameBodyPoint3 xAxis;
    SameBodyPoint3 yAxis;
    SameBodyPoint3 zAxis;
};

struct SameBodyPlaneFaceFeature
{
    tag_t tag;
    double area;
    double perimeter;
    int edgeCount;
    std::vector<SameBodyLengthBucket> lengthBuckets;
    SameBodyFrame3 frame;
};

struct SameBodyPlaneFaceGroup
{
    std::vector<size_t> faceIndexes;
};

struct SameBodyFingerprint
{
    Body* body;
    tag_t tag;
    double mass;
    double centroid[3];
    int edgeCount;
    int faceCount;
    std::vector<SameBodyLengthBucket> lengthBuckets;
    std::vector<SameBodyPoint3> vertexPoints;
    std::vector<SameBodyPoint3> circleCenterPoints;
    std::vector<double> circleCenterDistances;
    std::vector<SameBodyPoint3> lineEdgePoints;
    std::vector<SameBodyPoint3> curveEdgePoints;
    std::vector<SameBodyPoint3> arcEdgePoints;
    std::vector<SameBodyPoint3> fullCircleEdgePoints;
    std::vector<double> lineEdgeLengths;
    std::vector<double> curveEdgeLengths;
    std::vector<double> arcEdgeLengths;
    std::vector<double> fullCircleEdgeLengths;
    std::vector<SameBodyPlaneFaceFeature> planeFaces;
    std::vector<SameBodyPlaneFaceGroup> planeFaceGroups;
};

struct SameBodyCoarseSignature
{
    double mass;
    double principalMoments[3];
    int edgeCount;
    int faceCount;
};

struct SameBodyLocalCoordinateSignature
{
    SameBodyPoint3 centroid;
    std::vector<SameBodyPoint3> vertexLocalPoints;
    std::vector<SameBodyPoint3> lineEdgeLocalPoints;
    std::vector<SameBodyPoint3> curveEdgeLocalPoints;
    std::vector<SameBodyPoint3> arcEdgeLocalPoints;
    std::vector<SameBodyPoint3> fullCircleEdgeLocalPoints;
};

const char* kDefaultConfigText =
    "\xEF\xBB\xBF; KeZi engraving text template configuration\r\n"
    "; 可用变量: {文件名} {体名} {属性} {部件属性:属性名} {体属性:属性名} {流水号} {文本}\r\n"
    "; 示例: 模板={文件名}-{体名}-{体属性:bianhao}-{流水号}-{文本}\r\n"
    "; 流水号样式可填 1、01 或 001，仅支持数字递增\r\n"
    "[KeZi]\r\n"
    "模板={文本}\r\n"
    "文本=\r\n"
    "属性名=bianhao,PART_NO,ITEM_NO,Name\r\n"
    "流水号前缀=\r\n"
    "流水号样式=01\r\n"
    "起始号=1\r\n"
    "补零位数=2\r\n"
    "字体=Arial\r\n"
    "高度=10\r\n"
    "深度=0.3\r\n"
    "颜色=186\r\n"
    "边界=0\r\n"
    "边界深度=0.3\r\n"
    "边界颜色=186\r\n"
    "长度=100\r\n"
    "W比例=100\r\n"
    "锁定宽高比=1\r\n"
    "剪切=0\r\n"
    "文本层=254\r\n"
    "凸起文本=0\r\n"
    "V形文本=0\r\n"
    "V形刻字宽度=0.002\r\n"
    "编号设为部件名=0\r\n"
    "刻相同=0\r\n"
    "相同随机色=0\r\n"
    "所有可见方通自动刻字=0\r\n"
    "隐藏已刻字体=0\r\n"
    "模式=0\r\n"
    "长向居中=0\r\n"
    "短向居中=0\r\n"
    "X长边=0\r\n"
    "X短边=0\r\n";

const char* kVShapeFontName = "Modern";
const char* kVShapeNxCurveFontName = "blockmod1";

struct KeZiConfig
{
    std::string textTemplate = "{文本}";
    std::string text;
    std::vector<std::string> attributeNames = {"bianhao", "PART_NO", "ITEM_NO", "Name"};
    std::string serialPrefix;
    std::string serialStyle = "01";
    int serialStart = 1;
    int serialPad = 2;
    int mode = 0;
    std::string fontName = "Arial";
    double height = 10.0;
    double depth = 0.3;
    int textColor = 186;
    int boundary = 0;
    double boundaryDepth = 0.3;
    int boundaryColor = 186;
    double textLength = 100.0;
    double widthScale = 100.0;
    bool lockAspect = true;
    double shear = 0.0;
    int layer = 254;
    bool embossed = false;
    bool vShape = false;
    double vShapeWidth = 0.002;
    bool renameComponentToText = false;
    bool engraveSameBodies = false;
    bool sameRandomColor = false;
    bool autoEngraveVisibleTubes = false;
    bool hideEngravedText = false;
    bool centerLongSide = false;
    bool centerShortSide = false;
    bool xLongSide = false;
    bool xShortSide = false;
};

void ApplyPlanarFaceSelectionFilter(BlockStyler::SelectObject* selection)
{
    if (selection == nullptr)
    {
        return;
    }

    selection->AddFilter(BlockStyler::SelectObject::FilterTypeFaces);

    std::vector<NXOpen::Selection::MaskTriple> masks;
    masks.emplace_back(UF_solid_type, UF_solid_body_subtype, UF_UI_SEL_FEATURE_PLANAR_FACE);
    masks.emplace_back(UF_solid_type, UF_solid_face_subtype, UF_UI_SEL_FEATURE_PLANAR_FACE);

    std::unique_ptr<PropertyList> properties(selection->GetProperties());
    if (properties)
    {
        properties->SetSelectionFilter("SelectionFilter", NXOpen::Selection::SelectionActionClearAndEnableSpecific, masks);
    }
    selection->SetSelectionFilter(NXOpen::Selection::SelectionActionClearAndEnableSpecific, masks);
}

std::string ToString(const NXString& value)
{
    const char* text = value.GetText();
    return text == nullptr ? std::string() : std::string(text);
}

std::string Trim(std::string value)
{
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), value.end());
    return value;
}

std::string StripUtf8Bom(std::string value)
{
    if (value.size() >= 3 &&
        static_cast<unsigned char>(value[0]) == 0xEF &&
        static_cast<unsigned char>(value[1]) == 0xBB &&
        static_cast<unsigned char>(value[2]) == 0xBF)
    {
        value.erase(0, 3);
    }
    return value;
}

std::vector<std::string> SplitComma(const std::string& value)
{
    std::vector<std::string> result;
    std::stringstream input(value);
    std::string item;
    while (std::getline(input, item, ','))
    {
        item = Trim(item);
        if (!item.empty())
        {
            result.push_back(item);
        }
    }
    return result;
}

std::string FormatNumber(const double value)
{
    std::ostringstream out;
    out << std::setprecision(12) << value;
    return out.str();
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

double Magnitude(const Vector3d& value)
{
    return std::sqrt(Dot(value, value));
}

Vector3d Normalize(const Vector3d& value, const Vector3d& fallback)
{
    const double length = Magnitude(value);
    if (length < kVectorTolerance)
    {
        return fallback;
    }
    return Vector3d(value.X / length, value.Y / length, value.Z / length);
}

Matrix3x3 MakeMatrix(const Vector3d& xAxis, const Vector3d& yAxis, const Vector3d& zAxis)
{
    Matrix3x3 matrix;
    matrix.Xx = xAxis.X;
    matrix.Xy = xAxis.Y;
    matrix.Xz = xAxis.Z;
    matrix.Yx = yAxis.X;
    matrix.Yy = yAxis.Y;
    matrix.Yz = yAxis.Z;
    matrix.Zx = zAxis.X;
    matrix.Zy = zAxis.Y;
    matrix.Zz = zAxis.Z;
    return matrix;
}

Point3d OffsetPoint(const Point3d& point, const Vector3d& direction, const double distance)
{
    return Point3d(
        point.X + direction.X * distance,
        point.Y + direction.Y * distance,
        point.Z + direction.Z * distance);
}

Vector3d SubtractPoints(const Point3d& a, const Point3d& b)
{
    return Vector3d(a.X - b.X, a.Y - b.Y, a.Z - b.Z);
}

Vector3d ProjectToPlane(const Vector3d& value, const Vector3d& normal)
{
    const double distance = Dot(value, normal);
    return Vector3d(
        value.X - distance * normal.X,
        value.Y - distance * normal.Y,
        value.Z - distance * normal.Z);
}

std::vector<Point3d> FaceVertices(Face* face)
{
    std::vector<Point3d> points;
    if (face == nullptr)
    {
        return points;
    }

    try
    {
        const std::vector<Edge*> edges = face->GetEdges();
        for (Edge* edge : edges)
        {
            if (edge == nullptr)
            {
                continue;
            }

            Point3d a;
            Point3d b;
            edge->GetVertices(&a, &b);
            points.push_back(a);
            points.push_back(b);
        }
    }
    catch (...)
    {
    }
    return points;
}

double ProjectionSpan(const std::vector<Point3d>& points, const Vector3d& axis, double* center = nullptr)
{
    if (points.empty())
    {
        if (center != nullptr)
        {
            *center = 0.0;
        }
        return 0.0;
    }

    double minValue = std::numeric_limits<double>::max();
    double maxValue = -std::numeric_limits<double>::max();
    for (const Point3d& point : points)
    {
        const double value = point.X * axis.X + point.Y * axis.Y + point.Z * axis.Z;
        minValue = std::min(minValue, value);
        maxValue = std::max(maxValue, value);
    }

    if (center != nullptr)
    {
        *center = 0.5 * (minValue + maxValue);
    }
    return maxValue - minValue;
}

bool FaceLongShortAxes(Face* face, const Vector3d& normal, Vector3d* longAxis, Vector3d* shortAxis)
{
    if (face == nullptr || longAxis == nullptr || shortAxis == nullptr)
    {
        return false;
    }

    const std::vector<Point3d> vertices = FaceVertices(face);
    if (vertices.size() < 2)
    {
        return false;
    }

    Vector3d bestAxis(0.0, 0.0, 0.0);
    double bestLength = 0.0;
    try
    {
        const std::vector<Edge*> edges = face->GetEdges();
        for (Edge* edge : edges)
        {
            if (edge == nullptr)
            {
                continue;
            }

            Point3d a;
            Point3d b;
            edge->GetVertices(&a, &b);
            Vector3d edgeAxis = ProjectToPlane(SubtractPoints(b, a), normal);
            const double edgeLength = Magnitude(edgeAxis);
            if (edgeLength > bestLength)
            {
                bestLength = edgeLength;
                bestAxis = edgeAxis;
            }
        }
    }
    catch (...)
    {
    }

    if (bestLength < kVectorTolerance)
    {
        return false;
    }

    Vector3d longDir = Normalize(bestAxis, Vector3d(1.0, 0.0, 0.0));
    Vector3d shortDir = Normalize(Cross(normal, longDir), Vector3d(0.0, 1.0, 0.0));

    const double longSpan = ProjectionSpan(vertices, longDir);
    const double shortSpan = ProjectionSpan(vertices, shortDir);
    if (shortSpan > longSpan)
    {
        std::swap(longDir, shortDir);
    }

    *longAxis = longDir;
    *shortAxis = shortDir;
    return true;
}

class UfGuard
{
public:
    UfGuard() : ok_(UF_initialize() == 0) {}
    ~UfGuard()
    {
        if (ok_)
        {
            UF_terminate();
        }
    }

    bool ok() const { return ok_; }

private:
    bool ok_;
};

void ShowError(const char* title, const std::string& message)
{
    UI::GetUI()->NXMessageBox()->Show(title, NXMessageBox::DialogTypeError, message.c_str());
}

void Log(Session* session, const std::string& message)
{
    static std::mutex logMutex;
    try
    {
        std::lock_guard<std::mutex> lock(logMutex);
        SYSTEMTIME now = {};
        GetLocalTime(&now);
        std::ostringstream line;
        line << std::setfill('0')
             << '[' << std::setw(4) << now.wYear << '-'
             << std::setw(2) << now.wMonth << '-'
             << std::setw(2) << now.wDay << ' '
             << std::setw(2) << now.wHour << ':'
             << std::setw(2) << now.wMinute << ':'
             << std::setw(2) << now.wSecond << '.'
             << std::setw(3) << now.wMilliseconds << ']'
             << " [T" << GetCurrentThreadId() << "] KeZi: " << message;
        const std::string formatted = line.str();

        wchar_t tempPath[MAX_PATH] = {};
        if (GetTempPathW(MAX_PATH, tempPath) > 0)
        {
            std::ofstream file(std::filesystem::path(tempPath) / L"KeZi_debug.log", std::ios::app | std::ios::binary);
            if (file)
            {
                file << formatted << "\r\n";
                file.flush();
            }
        }
        std::vector<char> syslogText(formatted.begin(), formatted.end());
        syslogText.push_back('\n');
        syslogText.push_back('\0');
        UF_print_syslog(syslogText.data(), false);

#ifdef KEZI_ENABLE_DEBUG_LISTING
        if (session != nullptr && session->ListingWindow() != nullptr)
        {
            ListingWindow* listing = session->ListingWindow();
            listing->Open();
            listing->WriteLine(formatted.c_str());
        }
#else
        (void)session;
#endif
    }
    catch (...)
    {
    }
}

std::filesystem::path PluginDirectory()
{
    wchar_t modulePath[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(reinterpret_cast<HMODULE>(&__ImageBase), modulePath, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
    {
        return std::filesystem::current_path();
    }
    return std::filesystem::path(modulePath).parent_path();
}

std::filesystem::path ConfigFilePath()
{
    return std::filesystem::path(L"D:\\UG智辉钣金插件\\config\\KeZi.ini");
}

void EnsureDefaultConfigFile(const std::filesystem::path& path)
{
    if (std::filesystem::exists(path))
    {
        return;
    }

    std::error_code ignored;
    std::filesystem::create_directories(path.parent_path(), ignored);
    std::ofstream output(path, std::ios::binary);
    if (output)
    {
        output << kDefaultConfigText;
    }
}

std::map<std::string, std::string> ReadIniSection(const std::filesystem::path& path, const std::string& sectionName)
{
    std::map<std::string, std::string> values;
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        return values;
    }

    bool inSection = false;
    std::string line;
    while (std::getline(input, line))
    {
        line = StripUtf8Bom(Trim(line));
        if (line.empty() || line[0] == ';' || line[0] == '#')
        {
            continue;
        }
        if (line.front() == '[' && line.back() == ']')
        {
            inSection = (line.substr(1, line.size() - 2) == sectionName);
            continue;
        }
        if (!inSection)
        {
            continue;
        }
        const std::string::size_type eq = line.find('=');
        if (eq == std::string::npos)
        {
            continue;
        }
        values[Trim(line.substr(0, eq))] = Trim(line.substr(eq + 1));
    }
    return values;
}

int ToInt(const std::string& value, const int fallback)
{
    try
    {
        return std::stoi(value);
    }
    catch (...)
    {
        return fallback;
    }
}

double ToDouble(const std::string& value, const double fallback)
{
    try
    {
        return std::stod(value);
    }
    catch (...)
    {
        return fallback;
    }
}

bool ToBool(const std::string& value, const bool fallback)
{
    const std::string text = Trim(value);
    if (text == "1" || text == "true" || text == "True" || text == "TRUE" ||
        text == "yes" || text == "Yes" || text == "YES")
    {
        return true;
    }
    if (text == "0" || text == "false" || text == "False" || text == "FALSE" ||
        text == "no" || text == "No" || text == "NO")
    {
        return false;
    }
    return fallback;
}

std::string ValueOr(const std::map<std::string, std::string>& values,
                    const std::vector<std::string>& keys,
                    const std::string& fallback)
{
    for (const std::string& key : keys)
    {
        const auto found = values.find(key);
        if (found != values.end())
        {
            return found->second;
        }
    }
    return fallback;
}

KeZiConfig LoadConfig()
{
    const std::filesystem::path path = ConfigFilePath();
    EnsureDefaultConfigFile(path);

    KeZiConfig config;
    const std::map<std::string, std::string> values = ReadIniSection(path, "KeZi");
    config.textTemplate = ValueOr(values, {"模板", "template"}, config.textTemplate);
    config.text = ValueOr(values, {"文本", "text"}, config.text);
    const std::string names = ValueOr(values, {"属性名", "attributes", "attributeNames"}, "");
    if (!names.empty())
    {
        config.attributeNames = SplitComma(names);
    }
    config.serialPrefix = ValueOr(values, {"流水号前缀", "prefix"}, config.serialPrefix);
    config.serialStyle = ValueOr(values, {"流水号样式", "serialStyle", "style"}, config.serialStyle);
    config.serialStart = ToInt(ValueOr(values, {"起始号", "startNumber", "serialStart"}, std::to_string(config.serialStart)), config.serialStart);
    config.serialPad = ToInt(ValueOr(values, {"补零位数", "serialPad", "padWidth"}, std::to_string(config.serialPad)), config.serialPad);
    config.mode = ToInt(ValueOr(values, {"模式", "mode"}, std::to_string(config.mode)), config.mode);
    config.fontName = ValueOr(values, {"字体", "fontName"}, config.fontName);
    config.height = ToDouble(ValueOr(values, {"高度", "height"}, FormatNumber(config.height)), config.height);
    config.depth = ToDouble(ValueOr(values, {"深度", "depth"}, FormatNumber(config.depth)), config.depth);
    config.textColor = ToInt(ValueOr(values, {"颜色", "textColor"}, std::to_string(config.textColor)), config.textColor);
    config.boundary = ToInt(ValueOr(values, {"边界", "boundary"}, std::to_string(config.boundary)), config.boundary);
    config.boundaryDepth = ToDouble(ValueOr(values, {"边界深度", "boundaryDepth"}, FormatNumber(config.boundaryDepth)), config.boundaryDepth);
    config.boundaryColor = ToInt(ValueOr(values, {"边界颜色", "boundaryColor"}, std::to_string(config.boundaryColor)), config.boundaryColor);
    config.textLength = ToDouble(ValueOr(values, {"长度", "textLength"}, FormatNumber(config.textLength)), config.textLength);
    config.widthScale = ToDouble(ValueOr(values, {"W比例", "widthScale"}, FormatNumber(config.widthScale)), config.widthScale);
    config.lockAspect = ToBool(ValueOr(values, {"锁定宽高比", "lockAspect"}, config.lockAspect ? "1" : "0"), config.lockAspect);
    config.shear = ToDouble(ValueOr(values, {"剪切", "shear"}, FormatNumber(config.shear)), config.shear);
    config.layer = ToInt(ValueOr(values, {"文本层", "layer"}, std::to_string(config.layer)), config.layer);
    config.embossed = ToBool(ValueOr(values, {"凸起文本", "embossed"}, config.embossed ? "1" : "0"), config.embossed);
    config.vShape = ToBool(ValueOr(values, {"V形文本", "vShape"}, config.vShape ? "1" : "0"), config.vShape);
    config.vShapeWidth = ToDouble(ValueOr(values, {"V形刻字宽度", "vShapeWidth", "engravingWidth"}, FormatNumber(config.vShapeWidth)), config.vShapeWidth);
    config.renameComponentToText = ToBool(ValueOr(values, {"编号设为部件名", "renameComponentToText"}, config.renameComponentToText ? "1" : "0"), config.renameComponentToText);
    config.engraveSameBodies = ToBool(ValueOr(values, {"刻相同", "engraveSameBodies"}, config.engraveSameBodies ? "1" : "0"), config.engraveSameBodies);
    config.sameRandomColor = ToBool(ValueOr(values, {"相同随机色", "sameRandomColor"}, config.sameRandomColor ? "1" : "0"), config.sameRandomColor);
    config.autoEngraveVisibleTubes = ToBool(ValueOr(values, {"所有可见方通自动刻字", "autoEngraveVisibleTubes"}, config.autoEngraveVisibleTubes ? "1" : "0"), config.autoEngraveVisibleTubes);
    config.hideEngravedText = ToBool(ValueOr(values, {"隐藏已刻字体", "hideEngravedText"}, config.hideEngravedText ? "1" : "0"), config.hideEngravedText);
    config.centerLongSide = ToBool(ValueOr(values, {"长向居中", "centerLongSide"}, config.centerLongSide ? "1" : "0"), config.centerLongSide);
    config.centerShortSide = ToBool(ValueOr(values, {"短向居中", "centerShortSide"}, config.centerShortSide ? "1" : "0"), config.centerShortSide);
    config.xLongSide = ToBool(ValueOr(values, {"X长边", "xLongSide"}, config.xLongSide ? "1" : "0"), config.xLongSide);
    config.xShortSide = ToBool(ValueOr(values, {"X短边", "xShortSide"}, config.xShortSide ? "1" : "0"), config.xShortSide);
    if (config.attributeNames.empty())
    {
        config.attributeNames = {"bianhao", "PART_NO", "ITEM_NO", "Name"};
    }
    return config;
}

void WriteConfigValue(const std::string& key, const std::string& value)
{
    const std::filesystem::path path = ConfigFilePath();
    EnsureDefaultConfigFile(path);

    std::ifstream input(path, std::ios::binary);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(input, line))
    {
        lines.push_back(StripUtf8Bom(line));
    }

    bool inSection = false;
    bool wrote = false;
    bool sawSection = false;
    for (std::string& current : lines)
    {
        const std::string trimmed = Trim(current);
        if (trimmed.size() >= 2 && trimmed.front() == '[' && trimmed.back() == ']')
        {
            if (inSection && !wrote)
            {
                current = key + "=" + value + "\r\n" + current;
                wrote = true;
            }
            inSection = (trimmed.substr(1, trimmed.size() - 2) == "KeZi");
            sawSection = sawSection || inSection;
            continue;
        }

        if (!inSection)
        {
            continue;
        }

        const std::string::size_type eq = trimmed.find('=');
        if (eq == std::string::npos)
        {
            continue;
        }
        if (Trim(trimmed.substr(0, eq)) == key)
        {
            current = key + "=" + value;
            wrote = true;
        }
    }

    if (!sawSection)
    {
        lines.push_back("[KeZi]");
    }
    if (!wrote)
    {
        lines.push_back(key + "=" + value);
    }

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << "\xEF\xBB\xBF";
    for (const std::string& outputLine : lines)
    {
        output << outputLine << "\r\n";
    }
}

int ReadEnumValue(Enumeration* block)
{
    if (block == nullptr)
    {
        return 0;
    }

    std::unique_ptr<PropertyList> properties(block->GetProperties());
    return properties ? properties->GetEnum("Value") : 0;
}

std::string ReadString(StringBlock* block)
{
    return block == nullptr ? std::string() : Trim(ToString(block->Value()));
}

double ReadDouble(DoubleBlock* block, const double fallback)
{
    return block == nullptr ? fallback : block->Value();
}

int ReadInteger(IntegerBlock* block, const int fallback)
{
    return block == nullptr ? fallback : block->Value();
}

bool ReadToggle(Toggle* block, const bool fallback)
{
    return block == nullptr ? fallback : block->Value();
}

int ReadColorIndex(ObjectColorPicker* block, const int fallbackColor)
{
    int color = fallbackColor;
    if (block != nullptr)
    {
        const std::vector<int> values = block->GetValue();
        if (!values.empty())
        {
            color = values.front();
        }
    }
    return color;
}

NXColor* ReadColor(Part* part, ObjectColorPicker* block, const int fallbackColor)
{
    if (part == nullptr || part->Colors() == nullptr)
    {
        return nullptr;
    }

    return part->Colors()->Find(ReadColorIndex(block, fallbackColor));
}

void SetEnumValue(Enumeration* block, const int value)
{
    if (block == nullptr)
    {
        return;
    }
    std::unique_ptr<PropertyList> properties(block->GetProperties());
    if (properties)
    {
        properties->SetEnum("Value", value);
    }
}

void SetColorValue(ObjectColorPicker* block, const int color)
{
    if (block != nullptr)
    {
        block->SetValue(std::vector<int>{color});
    }
}

std::string ReadAttribute(NXObject* object, const std::string& name)
{
    if (object == nullptr || name.empty())
    {
        return std::string();
    }

    try
    {
        if (object->HasUserAttribute(name.c_str(), NXObject::AttributeTypeAny, -1))
        {
            return ToString(object->GetUserAttributeAsString(name.c_str(), NXObject::AttributeTypeAny, -1));
        }
        if (object->HasUserAttribute(name.c_str(), NXObject::AttributeTypeString, -1))
        {
            return ToString(object->GetStringUserAttribute(name.c_str(), -1));
        }
    }
    catch (...)
    {
    }
    return std::string();
}

std::wstring WideFromUtf8(const std::string& text)
{
    if (text.empty())
    {
        return std::wstring();
    }
    const int needed = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    if (needed <= 1)
    {
        return std::wstring();
    }
    std::wstring wide(static_cast<std::size_t>(needed - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, wide.data(), needed);
    return wide;
}

std::string Utf8FromWide(const std::wstring& text)
{
    if (text.empty())
    {
        return std::string();
    }
    const int needed = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (needed <= 1)
    {
        return std::string();
    }
    std::string utf8(static_cast<std::size_t>(needed - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, utf8.data(), needed, nullptr, nullptr);
    return utf8;
}

bool AskSystemFontName(const std::string& initialFont, std::string* selectedFont)
{
    if (selectedFont == nullptr)
    {
        return false;
    }

    LOGFONTW logFont = {};
    const std::wstring initial = WideFromUtf8(initialFont);
    if (!initial.empty())
    {
        wcsncpy_s(logFont.lfFaceName, initial.c_str(), _TRUNCATE);
    }

    CHOOSEFONTW chooseFont = {};
    chooseFont.lStructSize = sizeof(chooseFont);
    chooseFont.hwndOwner = GetActiveWindow();
    chooseFont.lpLogFont = &logFont;
    chooseFont.Flags = CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT | CF_NOVERTFONTS;
    if (ChooseFontW(&chooseFont) == FALSE)
    {
        return false;
    }

    *selectedFont = Utf8FromWide(logFont.lfFaceName);
    return !selectedFont->empty();
}

struct MenuDialogState
{
    std::vector<std::wstring> items;
    int selected = -1;
    bool done = false;
    HWND list = nullptr;
};

LRESULT CALLBACK MenuDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    constexpr int kListId = 1001;
    MenuDialogState* state = reinterpret_cast<MenuDialogState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (message)
    {
    case WM_CREATE:
    {
        CREATESTRUCTW* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        state = reinterpret_cast<MenuDialogState*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        state->list = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            L"LISTBOX",
            nullptr,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
            10,
            10,
            300,
            190,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kListId)),
            reinterpret_cast<HINSTANCE>(&__ImageBase),
            nullptr);
        SendMessageW(state->list, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        for (const std::wstring& item : state->items)
        {
            SendMessageW(state->list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item.c_str()));
        }
        SendMessageW(state->list, LB_SETCURSEL, 0, 0);

        HWND ok = CreateWindowExW(
            0,
            L"BUTTON",
            L"确定",
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            150,
            210,
            75,
            26,
            hwnd,
            reinterpret_cast<HMENU>(IDOK),
            reinterpret_cast<HINSTANCE>(&__ImageBase),
            nullptr);
        HWND cancel = CreateWindowExW(
            0,
            L"BUTTON",
            L"取消",
            WS_CHILD | WS_VISIBLE,
            235,
            210,
            75,
            26,
            hwnd,
            reinterpret_cast<HMENU>(IDCANCEL),
            reinterpret_cast<HINSTANCE>(&__ImageBase),
            nullptr);
        SendMessageW(ok, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        SendMessageW(cancel, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        return 0;
    }
    case WM_COMMAND:
        if (state != nullptr && LOWORD(wParam) == kListId && HIWORD(wParam) == LBN_DBLCLK)
        {
            state->selected = static_cast<int>(SendMessageW(state->list, LB_GETCURSEL, 0, 0));
            state->done = true;
            DestroyWindow(hwnd);
            return 0;
        }
        if (state != nullptr && LOWORD(wParam) == IDOK)
        {
            state->selected = static_cast<int>(SendMessageW(state->list, LB_GETCURSEL, 0, 0));
            state->done = true;
            DestroyWindow(hwnd);
            return 0;
        }
        if (state != nullptr && LOWORD(wParam) == IDCANCEL)
        {
            state->selected = -1;
            state->done = true;
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    case WM_CLOSE:
        if (state != nullptr)
        {
            state->selected = -1;
            state->done = true;
        }
        DestroyWindow(hwnd);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

int AskMenuIndex(const std::string& title, const std::vector<std::string>& items)
{
    if (items.empty())
    {
        return -1;
    }

    MenuDialogState state;
    for (const std::string& item : items)
    {
        state.items.push_back(WideFromUtf8(item));
    }

    HINSTANCE instance = reinterpret_cast<HINSTANCE>(&__ImageBase);
    static bool registered = false;
    if (!registered)
    {
        WNDCLASSW wc = {};
        wc.lpfnWndProc = MenuDialogProc;
        wc.hInstance = instance;
        wc.lpszClassName = L"KeZiMenuDialog";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
        RegisterClassW(&wc);
        registered = true;
    }

    HWND owner = GetActiveWindow();
    RECT ownerRect = {};
    GetWindowRect(owner, &ownerRect);
    const int width = 340;
    const int height = 285;
    int x = ownerRect.left + ((ownerRect.right - ownerRect.left) - width) / 2;
    int y = ownerRect.top + ((ownerRect.bottom - ownerRect.top) - height) / 2;
    if (owner == nullptr || x < 0 || y < 0)
    {
        x = CW_USEDEFAULT;
        y = CW_USEDEFAULT;
    }

    HWND hwnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        L"KeZiMenuDialog",
        WideFromUtf8(title).c_str(),
        WS_CAPTION | WS_SYSMENU | WS_POPUP,
        x,
        y,
        width,
        height,
        owner,
        nullptr,
        instance,
        &state);
    if (hwnd == nullptr)
    {
        return -1;
    }

    if (owner != nullptr)
    {
        EnableWindow(owner, FALSE);
    }
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg = {};
    while (!state.done && GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        if (!IsDialogMessageW(hwnd, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (owner != nullptr)
    {
        EnableWindow(owner, TRUE);
        SetActiveWindow(owner);
    }
    return state.selected >= 0 && state.selected < static_cast<int>(items.size()) ? state.selected : -1;
}

std::string AskMenuItemPaged(const std::string& title, const std::vector<std::string>& items)
{
    if (items.empty())
    {
        return std::string();
    }

    constexpr std::size_t kPageSize = 12;
    std::size_t page = 0;
    while (true)
    {
        const std::size_t begin = page * kPageSize;
        const std::size_t end = std::min(begin + kPageSize, items.size());
        std::vector<std::string> pageItems;
        std::vector<int> pageMap;
        for (std::size_t i = begin; i < end; ++i)
        {
            pageItems.push_back(items[i]);
            pageMap.push_back(static_cast<int>(i));
        }
        if (page > 0)
        {
            pageItems.push_back("上一页");
            pageMap.push_back(-2);
        }
        if (end < items.size())
        {
            pageItems.push_back("下一页");
            pageMap.push_back(-3);
        }

        const int selected = AskMenuIndex(title, pageItems);
        if (selected < 0)
        {
            return std::string();
        }
        const int mapped = pageMap[static_cast<std::size_t>(selected)];
        if (mapped == -2)
        {
            --page;
            continue;
        }
        if (mapped == -3)
        {
            ++page;
            continue;
        }
        return items[static_cast<std::size_t>(mapped)];
    }
}

std::vector<std::string> UserAttributeNames(NXObject* object)
{
    std::vector<std::string> names;
    if (object == nullptr)
    {
        return names;
    }

    try
    {
        const std::vector<NXObject::AttributeInformation> attributes = object->GetUserAttributes();
        for (const NXObject::AttributeInformation& attribute : attributes)
        {
            if (attribute.Unset || attribute.OwnedBySystem)
            {
                continue;
            }
            std::string title = Trim(ToString(attribute.Title));
            if (title.empty())
            {
                continue;
            }
            if (std::find(names.begin(), names.end(), title) == names.end())
            {
                names.push_back(title);
            }
        }
    }
    catch (...)
    {
    }

    std::sort(names.begin(), names.end());
    return names;
}

void WriteStringAttribute(NXObject* object, const std::string& name, const std::string& value)
{
    if (object == nullptr || name.empty())
    {
        return;
    }

    try
    {
        object->SetUserAttribute(name.c_str(), -1, value.c_str(), Update::OptionNow);
    }
    catch (...)
    {
    }
}

void SetObjectNameSafe(Session* session, NXObject* object, const std::string& name)
{
    if (object == nullptr || name.empty())
    {
        return;
    }

    try
    {
        object->SetName(name.c_str());
    }
    catch (const NXException& ex)
    {
        Log(session, std::string("体名修改失败: ") + ex.Message());
    }
    catch (const std::exception& ex)
    {
        Log(session, std::string("体名修改失败: ") + ex.what());
    }
    catch (...)
    {
        Log(session, "体名修改失败: 未知错误");
    }
}

void ThrowSameBodyUfError(int errorCode)
{
    if (errorCode != 0)
    {
        throw NXException::Create(errorCode);
    }
}

bool SameBodyNearlyEqual(double lhs, double rhs, double tolerance)
{
    return std::fabs(lhs - rhs) <= tolerance;
}

double SameBodyTolerance(double lhs, double rhs, double absoluteTolerance, double relativeTolerance)
{
    const double scale = std::max(std::fabs(lhs), std::fabs(rhs));
    return std::max(absoluteTolerance, scale * relativeTolerance);
}

bool SameBodyFaceAreasEqual(double lhs, double rhs)
{
    return std::fabs(lhs - rhs) <= SameBodyTolerance(lhs, rhs, kSameBodyFaceAreaAbsoluteTolerance, kSameBodyFaceAreaRelativeTolerance);
}

bool SameBodyFaceAreaSignificantlyGreater(double lhs, double rhs)
{
    return lhs > rhs + SameBodyTolerance(lhs, rhs, kSameBodyFaceAreaAbsoluteTolerance, kSameBodyFaceAreaRelativeTolerance);
}

SameBodyPoint3 MakeSameBodyPoint3(double x, double y, double z)
{
    SameBodyPoint3 point = {};
    point.x = x;
    point.y = y;
    point.z = z;
    return point;
}

SameBodyPoint3 SameBodyPointFromArray(const double point[3])
{
    return MakeSameBodyPoint3(point[0], point[1], point[2]);
}

SameBodyPoint3 SameBodyPointFromNx(const Point3d& point)
{
    return MakeSameBodyPoint3(point.X, point.Y, point.Z);
}

SameBodyPoint3 SameBodySubtractPoints(const SameBodyPoint3& lhs, const SameBodyPoint3& rhs)
{
    return MakeSameBodyPoint3(lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z);
}

SameBodyPoint3 SameBodyScalePoint(const SameBodyPoint3& point, double scale)
{
    return MakeSameBodyPoint3(point.x * scale, point.y * scale, point.z * scale);
}

double SameBodyDotPoint(const SameBodyPoint3& lhs, const SameBodyPoint3& rhs)
{
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

SameBodyPoint3 SameBodyCrossPoint(const SameBodyPoint3& lhs, const SameBodyPoint3& rhs)
{
    return MakeSameBodyPoint3(
        lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.x * rhs.y - lhs.y * rhs.x);
}

double SameBodyPointMagnitude(const SameBodyPoint3& point)
{
    return std::sqrt(SameBodyDotPoint(point, point));
}

bool NormalizeSameBodyPoint(SameBodyPoint3& point)
{
    const double magnitude = SameBodyPointMagnitude(point);
    if (magnitude <= 1.0e-12)
    {
        return false;
    }
    point.x /= magnitude;
    point.y /= magnitude;
    point.z /= magnitude;
    return true;
}

SameBodyPoint3 SameBodyProjectOutAxis(const SameBodyPoint3& vector, const SameBodyPoint3& axis)
{
    return SameBodySubtractPoints(vector, SameBodyScalePoint(axis, SameBodyDotPoint(vector, axis)));
}

double SameBodyDistanceBetweenPoints(const double lhs[3], const double rhs[3])
{
    const double dx = lhs[0] - rhs[0];
    const double dy = lhs[1] - rhs[1];
    const double dz = lhs[2] - rhs[2];
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

double SameBodyDistanceBetweenPoints(const SameBodyPoint3& lhs, const SameBodyPoint3& rhs)
{
    return SameBodyPointMagnitude(SameBodySubtractPoints(lhs, rhs));
}

bool AddUniqueSameBodyPoint(std::vector<SameBodyPoint3>& points, const SameBodyPoint3& point)
{
    for (size_t index = 0; index < points.size(); ++index)
    {
        if (SameBodyDistanceBetweenPoints(points[index], point) <= kSameBodyDistanceTolerance)
        {
            return false;
        }
    }
    points.push_back(point);
    return true;
}

int AskSameBodyPartUnits(tag_t partTag)
{
    int units = UF_PART_METRIC;
    if (UF_PART_ask_units(partTag, &units) != 0)
    {
        return UF_PART_METRIC;
    }
    return units;
}

int AskSameBodyOwningPartUnits(tag_t objectTag)
{
    tag_t owningPartTag = NULL_TAG;
    if (UF_OBJ_ask_owning_part(objectTag, &owningPartTag) != 0 || owningPartTag == NULL_TAG)
    {
        return UF_PART_METRIC;
    }
    return AskSameBodyPartUnits(owningPartTag);
}

int GetSameBodyMassPropsUnitsCode(int partUnits)
{
    return partUnits == UF_PART_ENGLISH ? 1 : 3;
}

double ConvertSameBodyMassPropsLengthToPartUnits(double value, int partUnits, int massPropsUnitsCode)
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

void AskSameBodyMassProperties(tag_t bodyTag, double* mass, double centroid[3], double principalMoments[3])
{
    double accuracyValues[11] = {0.99, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    double massProps[47] = {};
    double statistics[13] = {};
    tag_t objects[1] = {bodyTag};
    const int partUnits = AskSameBodyOwningPartUnits(bodyTag);
    const int massPropsUnitsCode = GetSameBodyMassPropsUnitsCode(partUnits);

    ThrowSameBodyUfError(UF_MODL_ask_mass_props_3d(objects, 1, 1, massPropsUnitsCode, 0.0, 1, accuracyValues, massProps, statistics));
    if (mass != nullptr)
    {
        *mass = massProps[2];
    }
    if (centroid != nullptr)
    {
        centroid[0] = ConvertSameBodyMassPropsLengthToPartUnits(massProps[3], partUnits, massPropsUnitsCode);
        centroid[1] = ConvertSameBodyMassPropsLengthToPartUnits(massProps[4], partUnits, massPropsUnitsCode);
        centroid[2] = ConvertSameBodyMassPropsLengthToPartUnits(massProps[5], partUnits, massPropsUnitsCode);
    }
    if (principalMoments != nullptr)
    {
        principalMoments[0] = massProps[22];
        principalMoments[1] = massProps[23];
        principalMoments[2] = massProps[24];
        std::sort(principalMoments, principalMoments + 3);
    }
}

SameBodyCoarseSignature BuildSameBodyCoarseSignature(Body* body)
{
    SameBodyCoarseSignature signature = {};
    AskSameBodyMassProperties(body->Tag(), &signature.mass, nullptr, signature.principalMoments);
    signature.edgeCount = static_cast<int>(body->GetEdges().size());
    signature.faceCount = static_cast<int>(body->GetFaces().size());
    return signature;
}

bool SameBodyCoarseSignaturesMatch(const SameBodyCoarseSignature& reference, const SameBodyCoarseSignature& candidate)
{
    if (!SameBodyNearlyEqual(reference.mass, candidate.mass, kSameBodyMassTolerance) ||
        reference.edgeCount != candidate.edgeCount ||
        reference.faceCount != candidate.faceCount)
    {
        return false;
    }
    for (int index = 0; index < 3; ++index)
    {
        if (!SameBodyNearlyEqual(reference.principalMoments[index], candidate.principalMoments[index], kSameBodyMassTolerance))
        {
            return false;
        }
    }
    return true;
}

double AskSameBodyEdgeLength(tag_t edgeTag)
{
    double length = 0.0;
    ThrowSameBodyUfError(UF_CURVE_ask_arc_length(edgeTag, 0.0, 1.0, UF_MODL_UNITS_PART, &length));
    return length;
}

std::vector<SameBodyLengthBucket> BuildSameBodyLengthBuckets(std::vector<double> lengths)
{
    std::sort(lengths.begin(), lengths.end());
    std::vector<SameBodyLengthBucket> buckets;
    for (double length : lengths)
    {
        if (buckets.empty() || !SameBodyNearlyEqual(buckets.back().length, length, kSameBodyLengthTolerance))
        {
            SameBodyLengthBucket bucket = {};
            bucket.length = length;
            bucket.count = 1;
            buckets.push_back(bucket);
            continue;
        }
        SameBodyLengthBucket& bucket = buckets.back();
        bucket.length = (bucket.length * bucket.count + length) / static_cast<double>(bucket.count + 1);
        ++bucket.count;
    }
    return buckets;
}

bool SameBodyLengthBucketsMatch(const std::vector<SameBodyLengthBucket>& referenceBuckets, const std::vector<SameBodyLengthBucket>& candidateBuckets)
{
    if (referenceBuckets.size() != candidateBuckets.size())
    {
        return false;
    }
    for (size_t index = 0; index < referenceBuckets.size(); ++index)
    {
        if (referenceBuckets[index].count != candidateBuckets[index].count ||
            !SameBodyNearlyEqual(referenceBuckets[index].length, candidateBuckets[index].length, kSameBodyLengthTolerance))
        {
            return false;
        }
    }
    return true;
}

bool AskSameBodyCircularEdgeCenter(tag_t edgeTag, double center[3])
{
    UF_EVAL_p_t evaluator = nullptr;
    if (UF_EVAL_initialize(edgeTag, &evaluator) != 0 || evaluator == nullptr)
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

void AppendSameBodyEdgeVerticesToGroup(int vertexCount, const double firstVertex[3], const double secondVertex[3], std::vector<SameBodyPoint3>& points)
{
    if (vertexCount >= 1)
    {
        points.push_back(SameBodyPointFromArray(firstVertex));
    }
    if (vertexCount >= 2)
    {
        points.push_back(SameBodyPointFromArray(secondVertex));
    }
}

void AppendUniqueSameBodyEdgeVertices(int vertexCount, const double firstVertex[3], const double secondVertex[3], std::vector<SameBodyPoint3>& vertexPoints)
{
    if (vertexCount >= 1)
    {
        AddUniqueSameBodyPoint(vertexPoints, SameBodyPointFromArray(firstVertex));
    }
    if (vertexCount >= 2)
    {
        AddUniqueSameBodyPoint(vertexPoints, SameBodyPointFromArray(secondVertex));
    }
}

void AppendSameBodyEdgeGeometryPoints(
    Edge* edge,
    double edgeLength,
    const double centroid[3],
    std::vector<double>& circleCenterDistances,
    std::vector<SameBodyPoint3>& vertexPoints,
    std::vector<SameBodyPoint3>& circleCenterPoints,
    std::vector<SameBodyPoint3>& lineEdgePoints,
    std::vector<SameBodyPoint3>& curveEdgePoints,
    std::vector<SameBodyPoint3>& arcEdgePoints,
    std::vector<SameBodyPoint3>& fullCircleEdgePoints,
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
    AppendUniqueSameBodyEdgeVertices(vertexCount, firstVertex, secondVertex, vertexPoints);

    const Edge::EdgeType edgeType = edge->SolidEdgeType();
    double center[3] = {};
    const bool hasCircularCenter = AskSameBodyCircularEdgeCenter(edge->Tag(), center);
    const bool isCircularEdge = edgeType == Edge::EdgeTypeCircular || hasCircularCenter;
    if (isCircularEdge && vertexCount == 0)
    {
        if (hasCircularCenter)
        {
            const SameBodyPoint3 centerPoint = SameBodyPointFromArray(center);
            circleCenterPoints.push_back(centerPoint);
            fullCircleEdgePoints.push_back(centerPoint);
            fullCircleEdgeLengths.push_back(edgeLength);
            circleCenterDistances.push_back(SameBodyDistanceBetweenPoints(centroid, center));
        }
        return;
    }
    if (isCircularEdge)
    {
        AppendSameBodyEdgeVerticesToGroup(vertexCount, firstVertex, secondVertex, arcEdgePoints);
        arcEdgeLengths.push_back(edgeLength);
        return;
    }
    if (edgeType == Edge::EdgeTypeLinear)
    {
        AppendSameBodyEdgeVerticesToGroup(vertexCount, firstVertex, secondVertex, lineEdgePoints);
        lineEdgeLengths.push_back(edgeLength);
    }
    else
    {
        AppendSameBodyEdgeVerticesToGroup(vertexCount, firstVertex, secondVertex, curveEdgePoints);
        curveEdgeLengths.push_back(edgeLength);
    }
}

bool SameBodyPlaneFaceMatch(const SameBodyPlaneFaceFeature& reference, const SameBodyPlaneFaceFeature& candidate)
{
    return SameBodyFaceAreasEqual(reference.area, candidate.area) &&
        SameBodyNearlyEqual(reference.perimeter, candidate.perimeter, kSameBodyLengthTolerance) &&
        reference.edgeCount == candidate.edgeCount &&
        SameBodyLengthBucketsMatch(reference.lengthBuckets, candidate.lengthBuckets);
}

bool BuildSameBodyFrameXAxisFromFaceEdges(Face* face, const SameBodyPoint3& zAxis, SameBodyPoint3& xAxis, std::vector<double>& edgeLengths)
{
    bool foundAxis = false;
    double bestLength = 0.0;
    std::vector<Edge*> faceEdges = face->GetEdges();
    edgeLengths.reserve(faceEdges.size());
    for (Edge* edge : faceEdges)
    {
        const double length = AskSameBodyEdgeLength(edge->Tag());
        edgeLengths.push_back(length);
        Point3d startPoint;
        Point3d endPoint;
        edge->GetVertices(&startPoint, &endPoint);
        SameBodyPoint3 direction = SameBodySubtractPoints(SameBodyPointFromNx(endPoint), SameBodyPointFromNx(startPoint));
        direction = SameBodyProjectOutAxis(direction, zAxis);
        if (!NormalizeSameBodyPoint(direction))
        {
            continue;
        }
        if (!foundAxis || length > bestLength + kSameBodyLengthTolerance)
        {
            xAxis = direction;
            bestLength = length;
            foundAxis = true;
        }
    }
    return foundAxis;
}

SameBodyPoint3 BuildSameBodyFallbackXAxis(const SameBodyPoint3& zAxis)
{
    SameBodyPoint3 reference = std::fabs(zAxis.x) < 0.8 ? MakeSameBodyPoint3(1.0, 0.0, 0.0) : MakeSameBodyPoint3(0.0, 1.0, 0.0);
    SameBodyPoint3 xAxis = SameBodyCrossPoint(reference, zAxis);
    if (!NormalizeSameBodyPoint(xAxis))
    {
        xAxis = MakeSameBodyPoint3(1.0, 0.0, 0.0);
    }
    return xAxis;
}

bool AskSameBodyFaceOutwardNormal(Face* face, SameBodyPoint3& normal)
{
    Session* session = Session::GetSession();
    if (face == nullptr || session == nullptr || session->Parts() == nullptr || session->Parts()->Work() == nullptr)
    {
        return false;
    }
    Direction* direction = session->Parts()->Work()->Directions()->CreateDumbDirectionFace(face, SenseForward, SmartObject::UpdateOptionWithinModeling);
    if (direction == nullptr)
    {
        return false;
    }
    const Vector3d vector = direction->Vector();
    normal = MakeSameBodyPoint3(vector.X, vector.Y, vector.Z);
    return NormalizeSameBodyPoint(normal);
}

bool BuildSameBodyPlanarFaceFeature(Face* face, SameBodyPlaneFaceFeature& feature)
{
    int faceType = 0;
    if (face == nullptr || UF_MODL_ask_face_type(face->Tag(), &faceType) != 0 || faceType != UF_MODL_PLANAR_FACE)
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
    ThrowSameBodyUfError(UF_MODL_ask_face_data(face->Tag(), &dataType, point, normal, box, &radius, &radiusData, &normalDirection));

    SameBodyPoint3 zAxis = {};
    if (!AskSameBodyFaceOutwardNormal(face, zAxis))
    {
        zAxis = SameBodyPointFromArray(normal);
        if (normalDirection < 0)
        {
            zAxis = SameBodyScalePoint(zAxis, -1.0);
        }
    }
    if (!NormalizeSameBodyPoint(zAxis))
    {
        return false;
    }

    std::vector<ISurface*> surfaces;
    surfaces.push_back(face);
    double area = 0.0;
    double perimeter = 0.0;
    double radiusDiameter = 0.0;
    Point3d cog;
    double minimumRadiusOfCurvature = 0.0;
    double areaErrorEstimate = 0.0;
    Point3d anchorPoint;
    bool isApproximate = false;
    Session::GetSession()->Measurement()->GetFaceProperties(surfaces, 0.99, Measurement::AlternateFaceRadius, true, &area, &perimeter, &radiusDiameter, &cog, &minimumRadiusOfCurvature, &areaErrorEstimate, &anchorPoint, &isApproximate);

    std::vector<double> edgeLengths;
    SameBodyPoint3 xAxis = {};
    if (!BuildSameBodyFrameXAxisFromFaceEdges(face, zAxis, xAxis, edgeLengths))
    {
        xAxis = BuildSameBodyFallbackXAxis(zAxis);
    }
    SameBodyPoint3 yAxis = SameBodyCrossPoint(zAxis, xAxis);
    if (!NormalizeSameBodyPoint(yAxis))
    {
        return false;
    }
    xAxis = SameBodyCrossPoint(yAxis, zAxis);
    if (!NormalizeSameBodyPoint(xAxis))
    {
        return false;
    }

    feature.tag = face->Tag();
    feature.area = area;
    feature.perimeter = perimeter;
    feature.edgeCount = static_cast<int>(edgeLengths.size());
    feature.lengthBuckets = BuildSameBodyLengthBuckets(edgeLengths);
    feature.frame.origin = SameBodyPointFromNx(cog);
    feature.frame.xAxis = xAxis;
    feature.frame.yAxis = yAxis;
    feature.frame.zAxis = zAxis;
    return true;
}

std::vector<SameBodyPlaneFaceGroup> BuildSameBodyPlaneFaceGroups(const std::vector<SameBodyPlaneFaceFeature>& planeFaces)
{
    std::vector<SameBodyPlaneFaceGroup> groups;
    for (size_t faceIndex = 0; faceIndex < planeFaces.size(); ++faceIndex)
    {
        bool addedToGroup = false;
        for (SameBodyPlaneFaceGroup& group : groups)
        {
            const SameBodyPlaneFaceFeature& groupFeature = planeFaces[group.faceIndexes[0]];
            if (SameBodyPlaneFaceMatch(groupFeature, planeFaces[faceIndex]))
            {
                group.faceIndexes.push_back(faceIndex);
                addedToGroup = true;
                break;
            }
        }
        if (!addedToGroup)
        {
            SameBodyPlaneFaceGroup group = {};
            group.faceIndexes.push_back(faceIndex);
            groups.push_back(group);
        }
    }
    return groups;
}

SameBodyFingerprint BuildSameBodyFingerprint(Body* body)
{
    SameBodyFingerprint fingerprint = {};
    fingerprint.body = body;
    fingerprint.tag = body->Tag();
    AskSameBodyMassProperties(fingerprint.tag, &fingerprint.mass, fingerprint.centroid, nullptr);

    std::vector<Edge*> edges = body->GetEdges();
    fingerprint.edgeCount = static_cast<int>(edges.size());
    std::vector<double> edgeLengths;
    edgeLengths.reserve(edges.size());
    for (Edge* edge : edges)
    {
        const double edgeLength = AskSameBodyEdgeLength(edge->Tag());
        edgeLengths.push_back(edgeLength);
        AppendSameBodyEdgeGeometryPoints(edge, edgeLength, fingerprint.centroid, fingerprint.circleCenterDistances, fingerprint.vertexPoints, fingerprint.circleCenterPoints, fingerprint.lineEdgePoints, fingerprint.curveEdgePoints, fingerprint.arcEdgePoints, fingerprint.fullCircleEdgePoints, fingerprint.lineEdgeLengths, fingerprint.curveEdgeLengths, fingerprint.arcEdgeLengths, fingerprint.fullCircleEdgeLengths);
    }
    fingerprint.lengthBuckets = BuildSameBodyLengthBuckets(edgeLengths);
    std::sort(fingerprint.circleCenterDistances.begin(), fingerprint.circleCenterDistances.end());
    std::sort(fingerprint.fullCircleEdgeLengths.begin(), fingerprint.fullCircleEdgeLengths.end(), std::greater<double>());
    std::sort(fingerprint.arcEdgeLengths.begin(), fingerprint.arcEdgeLengths.end(), std::greater<double>());
    std::sort(fingerprint.curveEdgeLengths.begin(), fingerprint.curveEdgeLengths.end(), std::greater<double>());
    std::sort(fingerprint.lineEdgeLengths.begin(), fingerprint.lineEdgeLengths.end(), std::greater<double>());

    std::vector<Face*> faces = body->GetFaces();
    fingerprint.faceCount = static_cast<int>(faces.size());
    for (Face* face : faces)
    {
        SameBodyPlaneFaceFeature faceFeature = {};
        if (BuildSameBodyPlanarFaceFeature(face, faceFeature))
        {
            fingerprint.planeFaces.push_back(faceFeature);
        }
    }
    fingerprint.planeFaceGroups = BuildSameBodyPlaneFaceGroups(fingerprint.planeFaces);
    return fingerprint;
}

SameBodyPoint3 TransformSameBodyWorldPointToLocal(const SameBodyFrame3& frame, const SameBodyPoint3& worldPoint)
{
    const SameBodyPoint3 delta = SameBodySubtractPoints(worldPoint, frame.origin);
    return MakeSameBodyPoint3(SameBodyDotPoint(delta, frame.xAxis), SameBodyDotPoint(delta, frame.yAxis), SameBodyDotPoint(delta, frame.zAxis));
}

SameBodyPoint3 TransformSameBodyLocalPointToWorld(const SameBodyFrame3& frame, const SameBodyPoint3& localPoint)
{
    return MakeSameBodyPoint3(
        frame.origin.x + localPoint.x * frame.xAxis.x + localPoint.y * frame.yAxis.x + localPoint.z * frame.zAxis.x,
        frame.origin.y + localPoint.x * frame.xAxis.y + localPoint.y * frame.yAxis.y + localPoint.z * frame.zAxis.y,
        frame.origin.z + localPoint.x * frame.xAxis.z + localPoint.y * frame.yAxis.z + localPoint.z * frame.zAxis.z);
}

SameBodyPoint3 TransformSameBodyWorldVectorToLocal(const SameBodyFrame3& frame, const SameBodyPoint3& worldVector)
{
    return MakeSameBodyPoint3(
        SameBodyDotPoint(worldVector, frame.xAxis),
        SameBodyDotPoint(worldVector, frame.yAxis),
        SameBodyDotPoint(worldVector, frame.zAxis));
}

SameBodyPoint3 TransformSameBodyLocalVectorToWorld(const SameBodyFrame3& frame, const SameBodyPoint3& localVector)
{
    return MakeSameBodyPoint3(
        localVector.x * frame.xAxis.x + localVector.y * frame.yAxis.x + localVector.z * frame.zAxis.x,
        localVector.x * frame.xAxis.y + localVector.y * frame.yAxis.y + localVector.z * frame.zAxis.y,
        localVector.x * frame.xAxis.z + localVector.y * frame.yAxis.z + localVector.z * frame.zAxis.z);
}

Point3d SameBodyPointToNx(const SameBodyPoint3& point)
{
    return Point3d(point.x, point.y, point.z);
}

Vector3d SameBodyVectorToNx(const SameBodyPoint3& vector)
{
    return Vector3d(vector.x, vector.y, vector.z);
}

SameBodyPoint3 SameBodyPointFromVector(const Vector3d& vector)
{
    return MakeSameBodyPoint3(vector.X, vector.Y, vector.Z);
}

void AppendSameBodyTransformedPoints(const SameBodyFrame3& frame, const std::vector<SameBodyPoint3>& worldPoints, std::vector<SameBodyPoint3>& localPoints)
{
    localPoints.reserve(worldPoints.size());
    for (const SameBodyPoint3& point : worldPoints)
    {
        localPoints.push_back(TransformSameBodyWorldPointToLocal(frame, point));
    }
}

SameBodyLocalCoordinateSignature BuildSameBodyLocalCoordinateSignature(const SameBodyFingerprint& fingerprint, const SameBodyFrame3& frame)
{
    SameBodyLocalCoordinateSignature signature = {};
    signature.centroid = TransformSameBodyWorldPointToLocal(frame, SameBodyPointFromArray(fingerprint.centroid));
    AppendSameBodyTransformedPoints(frame, fingerprint.vertexPoints, signature.vertexLocalPoints);
    AppendSameBodyTransformedPoints(frame, fingerprint.lineEdgePoints, signature.lineEdgeLocalPoints);
    AppendSameBodyTransformedPoints(frame, fingerprint.curveEdgePoints, signature.curveEdgeLocalPoints);
    AppendSameBodyTransformedPoints(frame, fingerprint.arcEdgePoints, signature.arcEdgeLocalPoints);
    AppendSameBodyTransformedPoints(frame, fingerprint.fullCircleEdgePoints, signature.fullCircleEdgeLocalPoints);
    return signature;
}

SameBodyPoint3 ApplySameBodyCoordinateVariant(const SameBodyPoint3& point, int variantIndex)
{
    const bool swapXY = (variantIndex & 1) != 0;
    const int signX = (variantIndex & 2) != 0 ? -1 : 1;
    const int signY = (variantIndex & 4) != 0 ? -1 : 1;
    const double sourceX = swapXY ? point.y : point.x;
    const double sourceY = swapXY ? point.x : point.y;
    return MakeSameBodyPoint3(static_cast<double>(signX) * sourceX, static_cast<double>(signY) * sourceY, point.z);
}

void ApplySameBodyPointVariant(const std::vector<SameBodyPoint3>& source, int variantIndex, std::vector<SameBodyPoint3>& target)
{
    target.reserve(source.size());
    for (const SameBodyPoint3& point : source)
    {
        target.push_back(ApplySameBodyCoordinateVariant(point, variantIndex));
    }
}

SameBodyLocalCoordinateSignature ApplySameBodyCoordinateVariant(const SameBodyLocalCoordinateSignature& signature, int variantIndex)
{
    SameBodyLocalCoordinateSignature result = {};
    result.centroid = ApplySameBodyCoordinateVariant(signature.centroid, variantIndex);
    ApplySameBodyPointVariant(signature.vertexLocalPoints, variantIndex, result.vertexLocalPoints);
    ApplySameBodyPointVariant(signature.lineEdgeLocalPoints, variantIndex, result.lineEdgeLocalPoints);
    ApplySameBodyPointVariant(signature.curveEdgeLocalPoints, variantIndex, result.curveEdgeLocalPoints);
    ApplySameBodyPointVariant(signature.arcEdgeLocalPoints, variantIndex, result.arcEdgeLocalPoints);
    ApplySameBodyPointVariant(signature.fullCircleEdgeLocalPoints, variantIndex, result.fullCircleEdgeLocalPoints);
    return result;
}

double SameBodyPointAxisValue(const SameBodyPoint3& point, int axisIndex)
{
    if (axisIndex == 0)
    {
        return point.x;
    }
    if (axisIndex == 1)
    {
        return point.y;
    }
    return point.z;
}

std::vector<double> SortedSameBodyPointAxisValues(const std::vector<SameBodyPoint3>& points, int axisIndex)
{
    std::vector<double> values;
    values.reserve(points.size());
    for (const SameBodyPoint3& point : points)
    {
        values.push_back(SameBodyPointAxisValue(point, axisIndex));
    }
    std::sort(values.begin(), values.end());
    return values;
}

bool CompareSameBodyPointValues(const std::vector<SameBodyPoint3>& referencePoints, const std::vector<SameBodyPoint3>& candidatePoints)
{
    if (referencePoints.size() != candidatePoints.size())
    {
        return false;
    }
    for (int axisIndex = 0; axisIndex < 3; ++axisIndex)
    {
        const std::vector<double> sortedReference = SortedSameBodyPointAxisValues(referencePoints, axisIndex);
        const std::vector<double> sortedCandidate = SortedSameBodyPointAxisValues(candidatePoints, axisIndex);
        for (size_t index = 0; index < sortedReference.size(); ++index)
        {
            if (!SameBodyNearlyEqual(sortedReference[index], sortedCandidate[index], kSameBodyDistanceTolerance))
            {
                return false;
            }
        }
    }
    return true;
}

bool SameBodyLocalCoordinateSignaturesMatch(const SameBodyLocalCoordinateSignature& reference, const SameBodyLocalCoordinateSignature& candidate, int variantIndex)
{
    const SameBodyLocalCoordinateSignature candidateVariant = ApplySameBodyCoordinateVariant(candidate, variantIndex);
    return CompareSameBodyPointValues(reference.lineEdgeLocalPoints, candidateVariant.lineEdgeLocalPoints) &&
        CompareSameBodyPointValues(reference.curveEdgeLocalPoints, candidateVariant.curveEdgeLocalPoints) &&
        CompareSameBodyPointValues(reference.arcEdgeLocalPoints, candidateVariant.arcEdgeLocalPoints) &&
        CompareSameBodyPointValues(reference.fullCircleEdgeLocalPoints, candidateVariant.fullCircleEdgeLocalPoints);
}

int FindSameBodyMatchingPlaneFaceGroup(const SameBodyFingerprint& fingerprint, const SameBodyPlaneFaceFeature& referenceFace, size_t expectedCount)
{
    int bestGroupIndex = -1;
    double bestArea = -1.0;
    for (size_t groupIndex = 0; groupIndex < fingerprint.planeFaceGroups.size(); ++groupIndex)
    {
        const SameBodyPlaneFaceGroup& group = fingerprint.planeFaceGroups[groupIndex];
        if (group.faceIndexes.size() != expectedCount || group.faceIndexes.empty())
        {
            continue;
        }
        const SameBodyPlaneFaceFeature& candidateFace = fingerprint.planeFaces[group.faceIndexes[0]];
        if (SameBodyPlaneFaceMatch(referenceFace, candidateFace))
        {
            if (bestGroupIndex < 0 || SameBodyFaceAreaSignificantlyGreater(candidateFace.area, bestArea))
            {
                bestGroupIndex = static_cast<int>(groupIndex);
                bestArea = candidateFace.area;
            }
        }
    }
    return bestGroupIndex;
}

void WriteIntegerAttribute(NXObject* object, const std::string& name, const int value)
{
    if (object == nullptr || name.empty())
    {
        return;
    }
    try
    {
        UF_ATTR_value_t attribute = {};
        attribute.type = UF_ATTR_integer;
        attribute.value.integer = value;
        UF_ATTR_assign(object->Tag(), const_cast<char*>(name.c_str()), attribute);
    }
    catch (...)
    {
    }
}

int NextRandomBodyColor()
{
    static std::mt19937 generator(static_cast<unsigned int>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count()));
    static std::uniform_int_distribution<int> colors(1, 216);
    return colors(generator);
}

void ApplySameBodyGroupMetadata(const std::vector<Body*>& bodies, const bool applyRandomColor)
{
    if (bodies.empty())
    {
        return;
    }
    const int quantity = static_cast<int>(bodies.size());
    const int color = applyRandomColor ? NextRandomBodyColor() : 0;
    for (Body* body : bodies)
    {
        if (body == nullptr)
        {
            continue;
        }
        WriteIntegerAttribute(body, "sulian", quantity);
        if (applyRandomColor)
        {
            bool displayModificationApplied = false;
            try
            {
                Session* session = Session::GetSession();
                DisplayModification* modification = session != nullptr && session->DisplayManager() != nullptr
                                                        ? session->DisplayManager()->NewDisplayModification()
                                                        : nullptr;
                if (modification != nullptr)
                {
                    modification->SetApplyToAllFaces(true);
                    modification->SetApplyToOwningParts(false);
                    modification->SetNewColor(color);
                    std::vector<DisplayableObject*> objects(1, body);
                    modification->Apply(objects);
                    delete modification;
                    displayModificationApplied = true;
                }
            }
            catch (...)
            {
            }
            if (!displayModificationApplied)
            {
                UF_OBJ_set_color(body->Tag(), color);
                for (Face* face : body->GetFaces())
                {
                    if (face != nullptr)
                    {
                        UF_OBJ_set_color(face->Tag(), color);
                    }
                }
            }
        }
    }
    if (applyRandomColor)
    {
        UF_DISP_regenerate_display();
    }
}

int FindSameBodyLargestPlaneFaceGroup(const SameBodyFingerprint& fingerprint, size_t expectedCount)
{
    int bestGroupIndex = -1;
    double bestArea = -1.0;
    for (size_t groupIndex = 0; groupIndex < fingerprint.planeFaceGroups.size(); ++groupIndex)
    {
        const SameBodyPlaneFaceGroup& group = fingerprint.planeFaceGroups[groupIndex];
        if (group.faceIndexes.size() != expectedCount || group.faceIndexes.empty())
        {
            continue;
        }
        const double area = fingerprint.planeFaces[group.faceIndexes[0]].area;
        if (bestGroupIndex < 0 || SameBodyFaceAreaSignificantlyGreater(area, bestArea))
        {
            bestGroupIndex = static_cast<int>(groupIndex);
            bestArea = area;
        }
    }
    return bestGroupIndex;
}

bool TrySameBodyAnchorPlaneGroupMatch(const SameBodyFingerprint& reference, const SameBodyPlaneFaceGroup& referenceGroup, const SameBodyFingerprint& candidate, const SameBodyPlaneFaceGroup& candidateGroup)
{
    if (referenceGroup.faceIndexes.empty() || candidateGroup.faceIndexes.empty())
    {
        return false;
    }
    const SameBodyPlaneFaceFeature& referenceFace = reference.planeFaces[referenceGroup.faceIndexes[0]];
    const SameBodyLocalCoordinateSignature referenceSignature = BuildSameBodyLocalCoordinateSignature(reference, referenceFace.frame);
    for (size_t candIndex = 0; candIndex < candidateGroup.faceIndexes.size(); ++candIndex)
    {
        const SameBodyPlaneFaceFeature& candidateFace = candidate.planeFaces[candidateGroup.faceIndexes[candIndex]];
        const SameBodyLocalCoordinateSignature candidateSignature = BuildSameBodyLocalCoordinateSignature(candidate, candidateFace.frame);
        static const int zRotationVariants[] = {0, 3, 5, 6};
        for (int variant : zRotationVariants)
        {
            if (SameBodyLocalCoordinateSignaturesMatch(referenceSignature, candidateSignature, variant))
            {
                return true;
            }
        }
    }
    return false;
}

bool SameBodyAnchorsMatch(const SameBodyFingerprint& reference, const SameBodyFingerprint& candidate)
{
    for (size_t expectedCount = 1; expectedCount <= 2; ++expectedCount)
    {
        const int referenceGroupIndex = FindSameBodyLargestPlaneFaceGroup(reference, expectedCount);
        if (referenceGroupIndex < 0)
        {
            continue;
        }
        const SameBodyPlaneFaceGroup& referenceGroup = reference.planeFaceGroups[static_cast<size_t>(referenceGroupIndex)];
        const SameBodyPlaneFaceFeature& referenceFace = reference.planeFaces[referenceGroup.faceIndexes[0]];

        const int candidateGroupIndex = FindSameBodyMatchingPlaneFaceGroup(candidate, referenceFace, expectedCount);
        if (candidateGroupIndex < 0)
        {
            if (expectedCount == 1)
            {
                continue;
            }
            return false;
        }

        const SameBodyPlaneFaceGroup& candidateGroup = candidate.planeFaceGroups[static_cast<size_t>(candidateGroupIndex)];
        if (TrySameBodyAnchorPlaneGroupMatch(reference, referenceGroup, candidate, candidateGroup))
        {
            return true;
        }
        if (expectedCount == 2)
        {
            return false;
        }
    }
    return false;
}

bool SameBodyDistanceVectorsMatch(const std::vector<double>& referenceDistances, const std::vector<double>& candidateDistances)
{
    if (referenceDistances.size() != candidateDistances.size())
    {
        return false;
    }
    for (size_t index = 0; index < referenceDistances.size(); ++index)
    {
        if (!SameBodyNearlyEqual(referenceDistances[index], candidateDistances[index], kSameBodyDistanceTolerance))
        {
            return false;
        }
    }
    return true;
}

bool SameBodyLengthSequenceMatch(const std::vector<double>& referenceLengths, const std::vector<double>& candidateLengths)
{
    if (referenceLengths.size() != candidateLengths.size())
    {
        return false;
    }
    for (size_t index = 0; index < referenceLengths.size(); ++index)
    {
        if (!SameBodyNearlyEqual(referenceLengths[index], candidateLengths[index], kSameBodyLengthTolerance))
        {
            return false;
        }
    }
    return true;
}

bool SameBodyTypedEdgeLengthsMatch(const SameBodyFingerprint& reference, const SameBodyFingerprint& candidate)
{
    return SameBodyLengthSequenceMatch(reference.fullCircleEdgeLengths, candidate.fullCircleEdgeLengths) &&
        SameBodyLengthSequenceMatch(reference.arcEdgeLengths, candidate.arcEdgeLengths) &&
        SameBodyLengthSequenceMatch(reference.curveEdgeLengths, candidate.curveEdgeLengths) &&
        SameBodyLengthSequenceMatch(reference.lineEdgeLengths, candidate.lineEdgeLengths);
}

bool SameBodyFingerprintsMatch(const SameBodyFingerprint& reference, const SameBodyFingerprint& candidate)
{
    return reference.edgeCount == candidate.edgeCount &&
        reference.faceCount == candidate.faceCount &&
        SameBodyTypedEdgeLengthsMatch(reference, candidate) &&
        SameBodyAnchorsMatch(reference, candidate);
}

std::vector<Body*> CollectVisibleBodies(Part* part)
{
    std::vector<Body*> bodies;
    if (part == nullptr || part->Bodies() == nullptr)
    {
        return bodies;
    }
    for (BodyCollection::iterator it = part->Bodies()->begin(); it != part->Bodies()->end(); ++it)
    {
        Body* body = *it;
        if (body != nullptr && !body->IsBlanked())
        {
            bodies.push_back(body);
        }
    }
    return bodies;
}

std::vector<Body*> FindMatchingVisibleBodies(Part* part, Body* referenceBody)
{
    std::vector<Body*> matches;
    if (part == nullptr || referenceBody == nullptr || referenceBody->IsBlanked())
    {
        return matches;
    }

    const int referenceEdgeCount = static_cast<int>(referenceBody->GetEdges().size());
    const int referenceFaceCount = static_cast<int>(referenceBody->GetFaces().size());
    const SameBodyCoarseSignature referenceCoarse = BuildSameBodyCoarseSignature(referenceBody);
    bool referenceFingerprintReady = false;
    SameBodyFingerprint referenceFingerprint = {};
    for (Body* candidate : CollectVisibleBodies(part))
    {
        if (candidate == nullptr || candidate == referenceBody)
        {
            continue;
        }
        if (static_cast<int>(candidate->GetEdges().size()) != referenceEdgeCount ||
            static_cast<int>(candidate->GetFaces().size()) != referenceFaceCount)
        {
            continue;
        }
        const SameBodyCoarseSignature candidateCoarse = BuildSameBodyCoarseSignature(candidate);
        if (!SameBodyCoarseSignaturesMatch(referenceCoarse, candidateCoarse))
        {
            continue;
        }
        if (!referenceFingerprintReady)
        {
            referenceFingerprint = BuildSameBodyFingerprint(referenceBody);
            referenceFingerprintReady = true;
        }
        const SameBodyFingerprint candidateFingerprint = BuildSameBodyFingerprint(candidate);
        if (SameBodyFingerprintsMatch(referenceFingerprint, candidateFingerprint))
        {
            matches.push_back(candidate);
        }
    }
    return matches;
}

bool IsAssemblyPart(Part* part)
{
    if (part == nullptr)
    {
        return false;
    }

    try
    {
        Assemblies::ComponentAssembly* assembly = part->ComponentAssembly();
        return assembly != nullptr && assembly->RootComponent() != nullptr;
    }
    catch (...)
    {
        return false;
    }
}

bool IsAssemblyContext(Session* session)
{
    if (session == nullptr || session->Parts() == nullptr)
    {
        return false;
    }

    return IsAssemblyPart(session->Parts()->Display()) || IsAssemblyPart(session->Parts()->Work());
}

void SetComponentNameSafe(Session* session, Assemblies::Component* component, const std::string& name)
{
    if (component == nullptr || name.empty())
    {
        return;
    }

    try
    {
        component->SetName(name.c_str());
    }
    catch (const NXException& ex)
    {
        Log(session, std::string("部件名修改失败: ") + ex.Message());
    }
    catch (const std::exception& ex)
    {
        Log(session, std::string("部件名修改失败: ") + ex.what());
    }
    catch (...)
    {
        Log(session, "部件名修改失败: 未知错误");
    }
}

std::string SafePartFileStem(std::string name)
{
    name = Trim(std::move(name));
    for (char& ch : name)
    {
        const unsigned char uch = static_cast<unsigned char>(ch);
        if (uch < 32 || ch == '<' || ch == '>' || ch == ':' || ch == '"' ||
            ch == '/' || ch == '\\' || ch == '|' || ch == '?' || ch == '*')
        {
            ch = '_';
        }
    }
    while (!name.empty() && (name.back() == ' ' || name.back() == '.'))
    {
        name.pop_back();
    }
    return name;
}

std::string PathToUtf8(const std::filesystem::path& path)
{
    const std::wstring wide = path.wstring();
    if (wide.empty())
    {
        return std::string();
    }
    const int needed = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (needed <= 1)
    {
        return std::string();
    }
    std::string utf8(static_cast<std::size_t>(needed - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, utf8.data(), needed, nullptr, nullptr);
    return utf8;
}

std::string ComponentNameForInstance(std::string name)
{
    name = SafePartFileStem(std::move(name));
    const size_t maxLen = static_cast<size_t>(UF_OBJ_NAME_NCHARS);
    if (name.size() > maxLen)
    {
        name.resize(maxLen);
    }
    return name;
}

bool SamePath(const std::filesystem::path& lhs, const std::filesystem::path& rhs)
{
    if (lhs.empty() || rhs.empty())
    {
        return false;
    }
    try
    {
        return std::filesystem::equivalent(lhs, rhs);
    }
    catch (...)
    {
        return _wcsicmp(lhs.wstring().c_str(), rhs.wstring().c_str()) == 0;
    }
}

void CollectComponentsByPrototypePath(Assemblies::Component* component, const std::filesystem::path& prototypePath, std::vector<Assemblies::Component*>& matches)
{
    if (component == nullptr || prototypePath.empty())
    {
        return;
    }

    try
    {
        BasePart* prototype = dynamic_cast<BasePart*>(component->Prototype());
        if (prototype != nullptr)
        {
            const std::filesystem::path componentPath(WideFromUtf8(ToString(prototype->FullPath())));
            if (SamePath(componentPath, prototypePath))
            {
                matches.push_back(component);
            }
        }
    }
    catch (...)
    {
    }

    try
    {
        for (Assemblies::Component* child : component->GetChildren())
        {
            CollectComponentsByPrototypePath(child, prototypePath, matches);
        }
    }
    catch (...)
    {
    }
}

bool SaveAndCopyComponentPart(Session* session, Part* workPart, const std::string& name, std::filesystem::path* originalPath, std::filesystem::path* replacementPath, std::string* errorMessage)
{
    const auto fail = [&](const std::string& message) {
        Log(session, message);
        if (errorMessage != nullptr)
        {
            *errorMessage = message;
        }
        return false;
    };

    const std::string safeName = SafePartFileStem(name);
    if (workPart == nullptr || safeName.empty() || originalPath == nullptr || replacementPath == nullptr)
    {
        return fail("替换部件失败: 工作部件、编号或输出路径为空");
    }

    try
    {
        std::filesystem::path currentPath(WideFromUtf8(ToString(workPart->FullPath())));
        if (currentPath.empty() || !currentPath.has_parent_path())
        {
            return fail("替换部件失败: 当前部件没有有效保存路径");
        }

        std::filesystem::path targetPath = currentPath.parent_path() / (safeName + ".prt");
        if (_wcsicmp(currentPath.wstring().c_str(), targetPath.wstring().c_str()) == 0)
        {
            return true;
        }
        if (std::filesystem::exists(targetPath))
        {
            return fail(std::string("替换部件失败: 目标文件已存在，请先关闭或移走 ") + PathToUtf8(targetPath));
        }

        std::unique_ptr<PartSaveStatus> saveStatus(workPart->Save(BasePart::SaveComponentsFalse, BasePart::CloseAfterSaveFalse));
        if (saveStatus && saveStatus->NumberUnsavedParts() > 0)
        {
            std::ostringstream oss;
            oss << "替换部件失败: 保存刻字部件失败，未保存部件数=" << saveStatus->NumberUnsavedParts();
            if (saveStatus->NumberUnsavedParts() > 0)
            {
                oss << ", 错误码=" << saveStatus->GetStatus(0);
            }
            return fail(oss.str());
        }

        std::filesystem::copy_file(currentPath, targetPath, std::filesystem::copy_options::none);
        Log(session, std::string("已复制刻字部件=") + PathToUtf8(targetPath));
        *originalPath = currentPath;
        *replacementPath = targetPath;
        return true;
    }
    catch (const NXException& ex)
    {
        return fail(std::string("复制替换部件失败: ") + ex.Message());
    }
    catch (const std::exception& ex)
    {
        return fail(std::string("复制替换部件失败: ") + ex.what());
    }
    catch (...)
    {
        return fail("复制替换部件失败: 未知错误");
    }
}

bool ReplaceCopiedComponentPart(Session* session, const std::string& componentJournalId, const std::filesystem::path& originalPath, const std::filesystem::path& replacementPath, const std::string& name, std::vector<Assemblies::Component*>* replacedComponents, std::string* errorMessage)
{
    const auto fail = [&](const std::string& message) {
        Log(session, message);
        if (errorMessage != nullptr)
        {
            *errorMessage = message;
        }
        return false;
    };

    const std::string safeName = SafePartFileStem(name);
    if (componentJournalId.empty() || replacementPath.empty() || safeName.empty())
    {
        return fail("替换部件失败: 组件标识、替换文件或编号为空");
    }

    try
    {

        Part* assemblyPart = session != nullptr && session->Parts() != nullptr ? session->Parts()->Work() : nullptr;
        if (assemblyPart == nullptr || assemblyPart->AssemblyManager() == nullptr)
        {
            return fail("替换部件失败: 当前工作部件不是有效装配");
        }
        if (assemblyPart->ComponentAssembly() == nullptr || assemblyPart->ComponentAssembly()->RootComponent() == nullptr)
        {
            return fail("替换部件失败: 当前工作部件没有有效装配根组件");
        }

        Assemblies::Component* component = nullptr;
        try
        {
            component = dynamic_cast<Assemblies::Component*>(
                assemblyPart->ComponentAssembly()->RootComponent()->FindObject(NXString(componentJournalId.c_str(), NXString::UTF8)));
        }
        catch (const NXException& ex)
        {
            return fail(std::string("替换部件失败: 重新定位组件失败: ") + ex.Message());
        }
        if (component == nullptr)
        {
            return fail("替换部件失败: 重新定位组件为空");
        }

        std::unique_ptr<Assemblies::ReplaceComponentBuilder, void (*)(Assemblies::ReplaceComponentBuilder*)> replaceBuilder(
            assemblyPart->AssemblyManager()->CreateReplaceComponentBuilder(),
            [](Assemblies::ReplaceComponentBuilder* value) {
                if (value != nullptr)
                {
                    value->Destroy();
                }
            });
        if (!replaceBuilder)
        {
            return fail("替换部件失败: 无法创建替换组件 Builder");
        }

        replaceBuilder->SetComponentNameType(Assemblies::ReplaceComponentBuilder::ComponentNameOptionAsSpecified);
        replaceBuilder->SetComponentName(NXString(ComponentNameForInstance(safeName).c_str(), NXString::UTF8));
        replaceBuilder->SetReplacementPart(NXString(PathToUtf8(replacementPath).c_str(), NXString::UTF8));
        replaceBuilder->SetComponentReferenceSetType(Assemblies::ReplaceComponentBuilder::ComponentReferenceSetMaintain, NXString("", NXString::UTF8));
        replaceBuilder->SetMaintainRelationships(true);
        replaceBuilder->SetReplaceAllOccurrences(true);
        const bool added = replaceBuilder->ComponentsToReplace()->Add(component);
        if (!added)
        {
            return fail("替换部件失败: 组件加入替换列表失败");
        }
        std::unique_ptr<PartLoadStatus> loadStatus(replaceBuilder->RegisterReplacePartLoadStatus());
        replaceBuilder->Commit();
        std::unique_ptr<ErrorList> errors(replaceBuilder->GetErrorList());
        if (errors && errors->Length() > 0)
        {
            std::ostringstream oss;
            oss << "替换部件失败: ReplaceComponentBuilder 错误数=" << errors->Length();
            return fail(oss.str());
        }

        Log(session, std::string("已替换组件为=") + PathToUtf8(replacementPath));
        if (replacedComponents != nullptr)
        {
            replacedComponents->clear();
            CollectComponentsByPrototypePath(assemblyPart->ComponentAssembly()->RootComponent(), replacementPath, *replacedComponents);
        }
        if (!originalPath.empty() && std::filesystem::exists(originalPath))
        {
            if (DeleteFileW(originalPath.wstring().c_str()) != 0)
            {
                Log(session, std::string("已从目录删除原部件=") + PathToUtf8(originalPath));
            }
            else
            {
                const DWORD err = GetLastError();
                std::ostringstream oss;
                oss << "目录删除原部件失败: Win32错误码=" << err << ", 文件=" << PathToUtf8(originalPath);
                Log(session, oss.str());
            }
        }
        return true;
    }
    catch (const NXException& ex)
    {
        return fail(std::string("替换部件失败: ") + ex.Message());
    }
    catch (const std::exception& ex)
    {
        return fail(std::string("替换部件失败: ") + ex.what());
    }
    catch (...)
    {
        return fail("替换部件失败: 未知错误");
    }
}

std::string FirstAvailableAttribute(NXObject* primary, NXObject* secondary, const std::vector<std::string>& names)
{
    for (const std::string& name : names)
    {
        std::string value = ReadAttribute(primary, name);
        if (value.empty())
        {
            value = ReadAttribute(secondary, name);
        }
        if (!value.empty())
        {
            return value;
        }
    }
    return std::string();
}

struct TextSettings
{
    int mode = 0;
    int boundary = 0;
    std::string text;
    std::string fontName = "Arial";
    double height = 10.0;
    double depth = 0.3;
    double boundaryDepth = 0.3;
    double textLength = 0.0;
    double widthScale = 100.0;
    double shear = 0.0;
    int layer = 254;
    bool lockAspect = true;
    bool embossed = false;
    bool vShape = false;
    double vShapeWidth = 0.002;
    bool renameComponentToText = false;
    bool engraveSameBodies = false;
    bool sameRandomColor = false;
    bool autoEngraveVisibleTubes = false;
    bool hideEngravedText = false;
    bool centerLongSide = false;
    bool centerShortSide = false;
    bool xLongSide = false;
    bool xShortSide = false;
};

Tooling::InsertTextBuilder::TextBoundaryType BoundaryTypeFromIndex(const int index)
{
    switch (index)
    {
    case 1:
        return Tooling::InsertTextBuilder::TextBoundaryTypeSlot;
    case 2:
        return Tooling::InsertTextBuilder::TextBoundaryTypeRectangle;
    default:
        return Tooling::InsertTextBuilder::TextBoundaryTypeNone;
    }
}

NXObject* ResolveBodyFromFace(Face* face)
{
    if (face == nullptr)
    {
        return nullptr;
    }

    tag_t bodyTag = NULL_TAG;
    if (UF_MODL_ask_face_body(face->Tag(), &bodyTag) != 0 || bodyTag == NULL_TAG)
    {
        return nullptr;
    }
    return dynamic_cast<NXObject*>(NXObjectManager::Get(bodyTag));
}

Part* SetWorkPartForFace(Session* session, Face* face)
{
    if (session == nullptr || face == nullptr)
    {
        return nullptr;
    }

    PartCollection* parts = session->Parts();
    if (parts == nullptr)
    {
        return nullptr;
    }

    if (face->IsOccurrence())
    {
        Assemblies::Component* component = face->OwningComponent();
        if (component != nullptr)
        {
            PartLoadStatus* loadStatus = nullptr;
            parts->SetWorkComponent(
                component,
                PartCollection::RefsetOptionCurrent,
                PartCollection::WorkComponentOptionGiven,
                &loadStatus);
            delete loadStatus;
            return parts->Work();
        }
    }

    BasePart* owningBasePart = face->OwningPart();
    Part* owningPart = dynamic_cast<Part*>(owningBasePart);
    if (owningPart != nullptr && parts->Work() != owningPart)
    {
        parts->SetWork(owningPart);
    }
    return parts->Work();
}

void RestoreWorkContext(PartCollection* parts, BasePart* previousWorkPart, Assemblies::Component* previousWorkComponent)
{
    if (parts == nullptr)
    {
        return;
    }

    try
    {
        if (previousWorkComponent != nullptr)
        {
            PartLoadStatus* loadStatus = nullptr;
            parts->SetWorkComponent(
                previousWorkComponent,
                PartCollection::RefsetOptionCurrent,
                PartCollection::WorkComponentOptionGiven,
                &loadStatus);
            delete loadStatus;
        }
        else if (previousWorkPart != nullptr)
        {
            parts->SetWork(previousWorkPart);
        }
        else
        {
            PartLoadStatus* loadStatus = nullptr;
            parts->SetWorkComponent(nullptr, &loadStatus);
            delete loadStatus;
        }
    }
    catch (...)
    {
    }
}

class WorkContextGuard
{
public:
    WorkContextGuard(Session* session, Face* targetFace)
    {
        if (session == nullptr || targetFace == nullptr)
        {
            return;
        }
        parts_ = session->Parts();
        if (parts_ == nullptr)
        {
            return;
        }
        previousWorkPart_ = parts_->BaseWork();
        previousWorkComponent_ = parts_->WorkComponent();
        targetWorkPart_ = SetWorkPartForFace(session, targetFace);
    }

    ~WorkContextGuard()
    {
        Restore();
    }

    WorkContextGuard(const WorkContextGuard&) = delete;
    WorkContextGuard& operator=(const WorkContextGuard&) = delete;

    Part* WorkPart() const
    {
        return targetWorkPart_;
    }

    void Restore()
    {
        if (!restored_)
        {
            RestoreWorkContext(parts_, previousWorkPart_, previousWorkComponent_);
            restored_ = true;
        }
    }

private:
    PartCollection* parts_ = nullptr;
    BasePart* previousWorkPart_ = nullptr;
    Assemblies::Component* previousWorkComponent_ = nullptr;
    Part* targetWorkPart_ = nullptr;
    bool restored_ = false;
};

Face* PrototypeFace(Face* face)
{
    if (face == nullptr || !face->IsOccurrence())
    {
        return face;
    }

    INXObject* prototype = face->Prototype();
    Face* prototypeFace = dynamic_cast<Face*>(prototype);
    return prototypeFace != nullptr ? prototypeFace : face;
}

Point3d ComponentPointToPrototype(Assemblies::Component* component, const Point3d& point)
{
    if (component == nullptr)
    {
        return point;
    }

    Point3d origin;
    Matrix3x3 orientation;
    component->GetPosition(&origin, &orientation);
    const Vector3d delta(point.X - origin.X, point.Y - origin.Y, point.Z - origin.Z);
    return Point3d(
        delta.X * orientation.Xx + delta.Y * orientation.Xy + delta.Z * orientation.Xz,
        delta.X * orientation.Yx + delta.Y * orientation.Yy + delta.Z * orientation.Yz,
        delta.X * orientation.Zx + delta.Y * orientation.Zy + delta.Z * orientation.Zz);
}

Vector3d ComponentVectorToPrototype(Assemblies::Component* component, const Vector3d& vector)
{
    if (component == nullptr)
    {
        return vector;
    }

    Point3d origin;
    Matrix3x3 orientation;
    component->GetPosition(&origin, &orientation);
    return Vector3d(
        vector.X * orientation.Xx + vector.Y * orientation.Xy + vector.Z * orientation.Xz,
        vector.X * orientation.Yx + vector.Y * orientation.Yy + vector.Z * orientation.Yz,
        vector.X * orientation.Zx + vector.Y * orientation.Zy + vector.Z * orientation.Zz);
}

Point3d ProjectPointToFacePlane(const Point3d& facePoint, const Vector3d& faceNormal, const Point3d& point)
{
    const Vector3d normal = Normalize(faceNormal, Vector3d(0.0, 0.0, 1.0));
    const Vector3d offset(point.X - facePoint.X, point.Y - facePoint.Y, point.Z - facePoint.Z);
    const double distance = Dot(offset, normal);
    return Point3d(
        point.X - distance * normal.X,
        point.Y - distance * normal.Y,
        point.Z - distance * normal.Z);
}

std::string PartLeafName(Part* part)
{
    if (part == nullptr)
    {
        return std::string();
    }
    std::string name = ToString(part->Leaf());
    const std::string::size_type dot = name.find_last_of('.');
    if (dot != std::string::npos)
    {
        name = name.substr(0, dot);
    }
    return name;
}

bool AskPlanarFaceData(Face* face, const Point3d* referencePoint, Point3d* point, Vector3d* normal)
{
    if (face == nullptr)
    {
        return false;
    }

    int type = 0;
    double origin[3] = {};
    double dir[3] = {};
    double box[6] = {};
    double radius = 0.0;
    double radData = 0.0;
    int normDir = 0;
    if (UF_MODL_ask_face_data(face->Tag(), &type, origin, dir, box, &radius, &radData, &normDir) != 0)
    {
        return false;
    }

    if (type != UF_MODL_PLANAR_FACE)
    {
        return false;
    }

    double refPoint[3] = {origin[0], origin[1], origin[2]};
    if (referencePoint != nullptr &&
        std::isfinite(referencePoint->X) &&
        std::isfinite(referencePoint->Y) &&
        std::isfinite(referencePoint->Z))
    {
        refPoint[0] = referencePoint->X;
        refPoint[1] = referencePoint->Y;
        refPoint[2] = referencePoint->Z;
    }

    double parm[2] = {};
    double facePoint[3] = {};
    if (UF_MODL_ask_face_parm_2(face->Tag(), refPoint, parm, facePoint) != 0)
    {
        facePoint[0] = origin[0];
        facePoint[1] = origin[1];
        facePoint[2] = origin[2];
    }

    if (point != nullptr)
    {
        *point = Point3d(facePoint[0], facePoint[1], facePoint[2]);
    }
    if (normal != nullptr)
    {
        double propsPoint[3] = {};
        double u1[3] = {};
        double v1[3] = {};
        double u2[3] = {};
        double v2[3] = {};
        double unitNormal[3] = {};
        double radii[2] = {};
        Vector3d value(dir[0], dir[1], dir[2]);
        if (UF_MODL_ask_face_props(face->Tag(), parm, propsPoint, u1, v1, u2, v2, unitNormal, radii) == 0)
        {
            value = Vector3d(unitNormal[0], unitNormal[1], unitNormal[2]);
        }
        else if (normDir < 0)
        {
            value = Vector3d(-value.X, -value.Y, -value.Z);
        }
        *normal = Normalize(value, Vector3d(0.0, 0.0, 1.0));
    }
    return true;
}

std::string SerialNumberText(const KeZiConfig& config)
{
    int width = config.serialPad;
    if (!config.serialStyle.empty() &&
        std::all_of(config.serialStyle.begin(), config.serialStyle.end(), [](unsigned char ch) { return std::isdigit(ch); }))
    {
        width = static_cast<int>(config.serialStyle.size());
    }

    std::ostringstream out;
    if (width > 1)
    {
        out << std::setw(width) << std::setfill('0');
    }
    out << config.serialStart;
    return out.str();
}

bool TextTemplateHasSerial(const std::string& textTemplate)
{
    return textTemplate.find("{流水号}") != std::string::npos ||
           textTemplate.find("{serial}") != std::string::npos;
}

bool TextTemplateHasBodyName(const std::string& textTemplate)
{
    return textTemplate.find("{体名}") != std::string::npos ||
           textTemplate.find("{body}") != std::string::npos;
}

bool TextTemplateHasFileName(const std::string& textTemplate)
{
    return textTemplate.find("{文件名}") != std::string::npos ||
           textTemplate.find("{file}") != std::string::npos;
}

bool TextTemplateHasText(const std::string& textTemplate)
{
    return textTemplate.find("{文本}") != std::string::npos ||
           textTemplate.find("{text}") != std::string::npos;
}

bool TextTemplateHasAttribute(const std::string& textTemplate)
{
    return textTemplate.find("{属性}") != std::string::npos ||
           textTemplate.find("{attribute}") != std::string::npos ||
           textTemplate.find("{属性:") != std::string::npos ||
           textTemplate.find("{体属性:") != std::string::npos ||
           textTemplate.find("{部件属性:") != std::string::npos ||
           textTemplate.find("{attribute:") != std::string::npos ||
           textTemplate.find("{body_attribute:") != std::string::npos ||
           textTemplate.find("{part_attribute:") != std::string::npos;
}

bool TextTemplateHasRuleToken(const std::string& textTemplate)
{
    return textTemplate.find('{') != std::string::npos &&
           textTemplate.find('}') != std::string::npos;
}

bool IncrementTextSerial(const std::string& text, std::string* nextText)
{
    if (nextText == nullptr || text.empty())
    {
        return false;
    }

    std::size_t end = text.size();
    while (end > 0 && std::isspace(static_cast<unsigned char>(text[end - 1])))
    {
        --end;
    }
    if (end == 0)
    {
        return false;
    }

    std::size_t begin = end;
    while (begin > 0 && std::isdigit(static_cast<unsigned char>(text[begin - 1])))
    {
        --begin;
    }

    if (begin < end)
    {
        const std::string numberText = text.substr(begin, end - begin);
        const int width = static_cast<int>(numberText.size());
        int value = 0;
        try
        {
            value = std::stoi(numberText);
        }
        catch (...)
        {
            return false;
        }

        std::ostringstream number;
        number << std::setw(width) << std::setfill('0') << (value + 1);
        *nextText = text.substr(0, begin) + number.str() + text.substr(end);
        return true;
    }

    return false;
}

bool LooksLikeSerialValue(const std::string& text)
{
    const std::string value = Trim(text);
    if (value.empty())
    {
        return false;
    }

    return std::all_of(value.begin(), value.end(), [](unsigned char ch) {
        return std::isdigit(ch) != 0;
    });
}

bool RuleAlreadyHasTokenType(const std::string& ruleText, const std::string& token)
{
    if (token == "{流水号}" || token == "{serial}")
    {
        return TextTemplateHasSerial(ruleText);
    }
    if (token == "{体名}" || token == "{body}")
    {
        return TextTemplateHasBodyName(ruleText);
    }
    if (token == "{文件名}" || token == "{file}")
    {
        return TextTemplateHasFileName(ruleText);
    }
    if (token == "{文本}" || token == "{text}")
    {
        return TextTemplateHasText(ruleText);
    }
    if (token.find("属性") != std::string::npos || token.find("attribute") != std::string::npos)
    {
        return ruleText.find(token) != std::string::npos;
    }
    return ruleText.find(token) != std::string::npos;
}

std::string RuleLabelText(std::string textTemplate)
{
    if (Trim(textTemplate).empty())
    {
        return "文本";
    }

    const auto addBetweenAdjacentTokens = [&textTemplate]() {
        std::string::size_type pos = 0;
        while ((pos = textTemplate.find("}{", pos)) != std::string::npos)
        {
            textTemplate.insert(pos + 1, "+");
            pos += 2;
        }
    };
    addBetweenAdjacentTokens();

    const auto replaceAll = [&textTemplate](const std::string& key, const std::string& replacement) {
        std::string::size_type pos = 0;
        while ((pos = textTemplate.find(key, pos)) != std::string::npos)
        {
            textTemplate.replace(pos, key.size(), replacement);
            pos += replacement.size();
        }
    };

    replaceAll("{text}", "文本");
    replaceAll("{文本}", "文本");
    replaceAll("{body}", "体名");
    replaceAll("{体名}", "体名");
    replaceAll("{file}", "文件名");
    replaceAll("{文件名}", "文件名");
    replaceAll("{serial}", "流水号");
    replaceAll("{流水号}", "流水号");
    replaceAll("{attribute}", "属性");
    replaceAll("{属性}", "属性");
    replaceAll("{属性名}", "属性名");

    const auto stripTokenBraces = [&textTemplate](const std::string& prefix) {
        std::string::size_type start = 0;
        while ((start = textTemplate.find(prefix, start)) != std::string::npos)
        {
            const std::string::size_type end = textTemplate.find('}', start);
            if (end == std::string::npos)
            {
                break;
            }
            const std::string body = textTemplate.substr(start + 1, end - start - 1);
            textTemplate.replace(start, end - start + 1, body);
            start += body.size();
        }
    };
    stripTokenBraces("{体属性:");
    stripTokenBraces("{部件属性:");
    stripTokenBraces("{属性:");
    stripTokenBraces("{body_attribute:");
    stripTokenBraces("{part_attribute:");
    stripTokenBraces("{attribute:");

    return Trim(textTemplate);
}

std::string RuleTextFromLabel(const std::string& label)
{
    std::string text = Trim(label);
    if (text.empty())
    {
        return "{文本}";
    }
    if (TextTemplateHasRuleToken(text))
    {
        return text;
    }

    std::string result;
    std::stringstream stream(text);
    std::string item;
    while (std::getline(stream, item, '+'))
    {
        item = Trim(item);
        if (item.empty())
        {
            continue;
        }

        if (item == "文本" || item == "text")
        {
            result += "{文本}";
        }
        else if (item == "流水号" || item == "serial")
        {
            result += "{流水号}";
        }
        else if (item == "体名" || item == "body")
        {
            result += "{体名}";
        }
        else if (item == "文件名" || item == "file")
        {
            result += "{文件名}";
        }
        else if (item == "属性" || item == "attribute")
        {
            result += "{属性}";
        }
        else if (item.rfind("体属性:", 0) == 0)
        {
            result += "{" + item + "}";
        }
        else if (item.rfind("部件属性:", 0) == 0)
        {
            result += "{" + item + "}";
        }
        else if (item.rfind("属性:", 0) == 0)
        {
            result += "{" + item + "}";
        }
        else
        {
            result += item;
        }
    }

    return result.empty() ? std::string("{文本}") : result;
}

bool SplitTextAndSerial(const std::string& value, std::string* textPart, std::string* serialPart)
{
    if (textPart == nullptr || serialPart == nullptr)
    {
        return false;
    }

    const std::string trimmed = Trim(value);
    if (trimmed.empty())
    {
        return false;
    }

    std::size_t end = trimmed.size();
    std::size_t begin = end;
    while (begin > 0 && std::isdigit(static_cast<unsigned char>(trimmed[begin - 1])))
    {
        --begin;
    }

    if (begin < end)
    {
        *textPart = trimmed.substr(0, begin);
        *serialPart = trimmed.substr(begin, end - begin);
        return true;
    }

    return false;
}

bool ExtractTrailingSerial(const std::string& value, std::string* serialPart)
{
    if (serialPart == nullptr)
    {
        return false;
    }

    const std::string trimmed = Trim(value);
    if (trimmed.empty())
    {
        return false;
    }

    std::size_t end = trimmed.size();
    std::size_t begin = end;
    while (begin > 0 && std::isdigit(static_cast<unsigned char>(trimmed[begin - 1])))
    {
        --begin;
    }
    if (begin < end)
    {
        *serialPart = trimmed.substr(begin, end - begin);
        return true;
    }

    return false;
}

struct TemplateToken
{
    enum class Kind
    {
        Text,
        Serial,
        Fixed
    };

    Kind kind = Kind::Fixed;
    std::string value;
};

std::vector<std::string> RawTemplateTokens(const std::string& textTemplate)
{
    std::vector<std::string> tokens;
    std::string::size_type start = 0;
    while ((start = textTemplate.find('{', start)) != std::string::npos)
    {
        const std::string::size_type end = textTemplate.find('}', start);
        if (end == std::string::npos)
        {
            break;
        }
        tokens.push_back(textTemplate.substr(start, end - start + 1));
        start = end + 1;
    }
    return tokens;
}

bool IsTextToken(const std::string& token)
{
    return token == "{文本}" || token == "{text}";
}

bool IsSerialToken(const std::string& token)
{
    return token == "{流水号}" || token == "{serial}";
}

std::string AttributeNameFromToken(const std::string& token, const std::string& prefix)
{
    if (token.rfind(prefix, 0) != 0 || token.empty() || token.back() != '}')
    {
        return std::string();
    }
    return token.substr(prefix.size(), token.size() - prefix.size() - 1);
}

std::string ResolveFixedTokenValue(const std::string& token,
                                   NXObject* body,
                                   NXObject* partObject,
                                   const std::string& bodyName,
                                   const std::string& fileName,
                                   const KeZiConfig& config)
{
    if (token == "{body}" || token == "{体名}")
    {
        return bodyName;
    }
    if (token == "{file}" || token == "{文件名}")
    {
        return fileName;
    }
    if (token == "{attribute}" || token == "{属性}")
    {
        return FirstAvailableAttribute(body, partObject, config.attributeNames);
    }
    if (token == "{属性名}")
    {
        return config.attributeNames.empty() ? std::string() : config.attributeNames.front();
    }

    const std::vector<std::pair<std::string, NXObject*>> attributePrefixes = {
        {"{体属性:", body},
        {"{body_attribute:", body},
        {"{部件属性:", partObject},
        {"{part_attribute:", partObject},
        {"{属性:", body},
        {"{attribute:", body},
    };
    for (const auto& entry : attributePrefixes)
    {
        const std::string name = AttributeNameFromToken(token, entry.first);
        if (!name.empty())
        {
            std::string value = ReadAttribute(entry.second, name);
            if (value.empty() && (entry.first == "{属性:" || entry.first == "{attribute:"))
            {
                value = ReadAttribute(partObject, name);
            }
            return value;
        }
    }

    return token;
}

std::vector<TemplateToken> BuildTemplateTokens(const std::string& textTemplate,
                                               NXObject* body,
                                               NXObject* partObject,
                                               const std::string& bodyName,
                                               const std::string& fileName,
                                               const KeZiConfig& config)
{
    std::vector<TemplateToken> tokens;
    for (const std::string& rawToken : RawTemplateTokens(textTemplate))
    {
        TemplateToken token;
        if (IsTextToken(rawToken))
        {
            token.kind = TemplateToken::Kind::Text;
        }
        else if (IsSerialToken(rawToken))
        {
            token.kind = TemplateToken::Kind::Serial;
        }
        else
        {
            token.kind = TemplateToken::Kind::Fixed;
            token.value = ResolveFixedTokenValue(rawToken, body, partObject, bodyName, fileName, config);
        }
        tokens.push_back(token);
    }
    return tokens;
}

std::string JoinFixedTokens(const std::vector<TemplateToken>& tokens, std::size_t begin, std::size_t end)
{
    std::string result;
    for (std::size_t i = begin; i < end && i < tokens.size(); ++i)
    {
        if (tokens[i].kind == TemplateToken::Kind::Fixed)
        {
            result += tokens[i].value;
        }
    }
    return result;
}

bool StartsWithText(const std::string& value, const std::string& prefix)
{
    return prefix.empty() || value.rfind(prefix, 0) == 0;
}

bool EndsWithText(const std::string& value, const std::string& suffix)
{
    return suffix.empty() ||
           (value.size() >= suffix.size() && value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0);
}

void ResolveEditableValuesFromDisplay(const std::vector<TemplateToken>& tokens,
                                      const std::string& displayText,
                                      const KeZiConfig& config,
                                      std::string* textValue,
                                      std::string* serialValue)
{
    if (textValue == nullptr || serialValue == nullptr)
    {
        return;
    }

    *textValue = displayText;
    *serialValue = SerialNumberText(config);

    int textIndex = -1;
    int serialIndex = -1;
    for (std::size_t i = 0; i < tokens.size(); ++i)
    {
        if (tokens[i].kind == TemplateToken::Kind::Text)
        {
            textIndex = static_cast<int>(i);
        }
        else if (tokens[i].kind == TemplateToken::Kind::Serial)
        {
            serialIndex = static_cast<int>(i);
        }
    }

    if (textIndex < 0 && serialIndex < 0)
    {
        return;
    }

    std::string editable = Trim(displayText);
    const int firstEditable = textIndex >= 0 && serialIndex >= 0 ? std::min(textIndex, serialIndex) : std::max(textIndex, serialIndex);
    const int lastEditable = textIndex >= 0 && serialIndex >= 0 ? std::max(textIndex, serialIndex) : std::max(textIndex, serialIndex);
    const std::string prefix = JoinFixedTokens(tokens, 0, static_cast<std::size_t>(firstEditable));
    const std::string suffix = JoinFixedTokens(tokens, static_cast<std::size_t>(lastEditable + 1), tokens.size());
    if (StartsWithText(editable, prefix))
    {
        editable.erase(0, prefix.size());
    }
    if (EndsWithText(editable, suffix))
    {
        editable.erase(editable.size() - suffix.size());
    }

    if (textIndex >= 0 && serialIndex >= 0)
    {
        const std::string between = textIndex < serialIndex
                                        ? JoinFixedTokens(tokens, static_cast<std::size_t>(textIndex + 1), static_cast<std::size_t>(serialIndex))
                                        : JoinFixedTokens(tokens, static_cast<std::size_t>(serialIndex + 1), static_cast<std::size_t>(textIndex));
        if (!between.empty())
        {
            const std::string::size_type split = editable.find(between);
            if (split != std::string::npos)
            {
                if (textIndex < serialIndex)
                {
                    *textValue = editable.substr(0, split);
                    *serialValue = editable.substr(split + between.size());
                }
                else
                {
                    *serialValue = editable.substr(0, split);
                    *textValue = editable.substr(split + between.size());
                }
                return;
            }
        }

        std::string textPart;
        std::string serialPart;
        if (textIndex < serialIndex && SplitTextAndSerial(editable, &textPart, &serialPart))
        {
            *textValue = textPart;
            *serialValue = serialPart;
            return;
        }
        if (serialIndex < textIndex)
        {
            std::string serialPart;
            if (ExtractTrailingSerial(editable, &serialPart) && editable.size() >= serialPart.size())
            {
                *serialValue = serialPart;
                *textValue = editable.substr(serialPart.size());
                return;
            }
        }
    }
    else if (textIndex >= 0)
    {
        *textValue = editable;
    }
    else if (serialIndex >= 0)
    {
        std::string serialPart;
        if (ExtractTrailingSerial(editable, &serialPart) && LooksLikeSerialValue(serialPart))
        {
            *serialValue = serialPart;
        }
    }
}

std::string ComposeTemplateDisplayText(const std::vector<TemplateToken>& tokens,
                                       const std::string& textValue,
                                       const std::string& serialValue)
{
    std::string result;
    for (const TemplateToken& token : tokens)
    {
        if (token.kind == TemplateToken::Kind::Text)
        {
            result += textValue;
        }
        else if (token.kind == TemplateToken::Kind::Serial)
        {
            result += serialValue;
        }
        else
        {
            result += token.value;
        }
    }
    return result;
}

std::string ExpandTextTemplate(const std::string& textTemplate,
                               const std::string& userText,
                               NXObject* body,
                               Part* part,
                               const KeZiConfig& config,
                               const std::string* explicitTextValue = nullptr,
                               const std::string* explicitSerialValue = nullptr)
{
    std::string result = textTemplate.empty() ? "{文本}" : textTemplate;
    NXObject* partObject = dynamic_cast<NXObject*>(part);
    const std::string bodyName = body == nullptr ? std::string() : ToString(body->Name());
    const std::string fileName = PartLeafName(part);
    const std::vector<TemplateToken> tokens = BuildTemplateTokens(result, body, partObject, bodyName, fileName, config);
    const bool hasTextToken = std::any_of(tokens.begin(), tokens.end(), [](const TemplateToken& token) { return token.kind == TemplateToken::Kind::Text; });
    const bool hasSerialToken = std::any_of(tokens.begin(), tokens.end(), [](const TemplateToken& token) { return token.kind == TemplateToken::Kind::Serial; });
    std::string resolvedUserText = explicitTextValue != nullptr ? *explicitTextValue : userText;
    std::string serial = explicitSerialValue != nullptr ? *explicitSerialValue : SerialNumberText(config);
    if ((hasTextToken || hasSerialToken) && (explicitTextValue == nullptr || explicitSerialValue == nullptr))
    {
        std::string parsedText = userText;
        std::string parsedSerial = SerialNumberText(config);
        ResolveEditableValuesFromDisplay(tokens, userText, config, &parsedText, &parsedSerial);
        if (explicitTextValue == nullptr)
        {
            resolvedUserText = parsedText;
        }
        if (explicitSerialValue == nullptr)
        {
            serial = parsedSerial;
        }
    }

    const auto replaceAll = [&result](const std::string& key, const std::string& replacement) {
        std::string::size_type pos = 0;
        while ((pos = result.find(key, pos)) != std::string::npos)
        {
            result.replace(pos, key.size(), replacement);
            pos += replacement.size();
        }
    };

    replaceAll("{text}", resolvedUserText);
    replaceAll("{文本}", resolvedUserText);
    replaceAll("{body}", bodyName);
    replaceAll("{体名}", bodyName);
    replaceAll("{file}", fileName);
    replaceAll("{文件名}", fileName);
    replaceAll("{serial}", serial);
    replaceAll("{流水号}", serial);
    replaceAll("{attribute}", FirstAvailableAttribute(body, partObject, config.attributeNames));
    replaceAll("{属性}", FirstAvailableAttribute(body, partObject, config.attributeNames));
    replaceAll("{属性名}", config.attributeNames.empty() ? std::string() : config.attributeNames.front());

    const auto replaceAttributeToken = [&result](const std::string& prefix, NXObject* object) {
        std::string::size_type start = 0;
        while ((start = result.find(prefix, start)) != std::string::npos)
        {
            const std::string::size_type end = result.find('}', start);
            if (end == std::string::npos)
            {
                break;
            }
            const std::string attributeName = result.substr(start + prefix.size(), end - start - prefix.size());
            const std::string attributeValue = ReadAttribute(object, attributeName);
            result.replace(start, end - start + 1, attributeValue);
            start += attributeValue.size();
        }
    };

    const auto replaceCombinedAttributeToken = [&result](const std::string& prefix, NXObject* primary, NXObject* secondary) {
        std::string::size_type start = 0;
        while ((start = result.find(prefix, start)) != std::string::npos)
        {
            const std::string::size_type end = result.find('}', start);
            if (end == std::string::npos)
            {
                break;
            }
            const std::string attributeName = result.substr(start + prefix.size(), end - start - prefix.size());
            std::string attributeValue = ReadAttribute(primary, attributeName);
            if (attributeValue.empty())
            {
                attributeValue = ReadAttribute(secondary, attributeName);
            }
            result.replace(start, end - start + 1, attributeValue);
            start += attributeValue.size();
        }
    };

    replaceAttributeToken("{body_attribute:", body);
    replaceAttributeToken("{体属性:", body);
    replaceAttributeToken("{part_attribute:", partObject);
    replaceAttributeToken("{部件属性:", partObject);
    replaceCombinedAttributeToken("{attribute:", body, partObject);
    replaceCombinedAttributeToken("{属性:", body, partObject);
    if (result.find('{') == std::string::npos && result.find('}') == std::string::npos)
    {
        return Trim(result);
    }
    return Trim(result);
}
}

class KeZiDialog;
KeZiDialog* gActiveKeZiDialog = nullptr;
KeZiDialog* gDeferredKeZiDialog = nullptr;

Features::CustomTagAttribute* CreateKeZiTagAttribute(
    Features::CustomAttributeCollection* attributes,
    const char* name,
    TaggedObject* value,
    const bool mandatory,
    const bool targetBody)
{
    std::vector<Features::CustomAttribute::Property> properties;
    if (mandatory) { properties.push_back(Features::CustomAttribute::PropertyMandatoryInput); }
    if (targetBody) { properties.push_back(Features::CustomAttribute::PropertyIsReferencingTargetBody); }
    Features::CustomTagAttribute* attribute = attributes->CreateCustomTagAttribute(name, properties);
    attribute->SetValue(value);
    return attribute;
}

Features::CustomDoubleAttribute* CreateKeZiDoubleAttribute(
    Features::CustomAttributeCollection* attributes,
    const char* name,
    const double value)
{
    Features::CustomDoubleAttribute* attribute = attributes->CreateCustomDoubleAttribute(
        name, std::vector<Features::CustomAttribute::Property>());
    attribute->SetValue(value);
    return attribute;
}

Features::CustomIntegerAttribute* CreateKeZiIntegerAttribute(
    Features::CustomAttributeCollection* attributes,
    const char* name,
    const int value)
{
    Features::CustomIntegerAttribute* attribute = attributes->CreateCustomIntegerAttribute(
        name, std::vector<Features::CustomAttribute::Property>());
    attribute->SetValue(value);
    return attribute;
}

Features::CustomStringAttribute* CreateKeZiStringAttribute(
    Features::CustomAttributeCollection* attributes,
    const char* name,
    const std::string& value)
{
    Features::CustomStringAttribute* attribute = attributes->CreateCustomStringAttribute(
        name, std::vector<Features::CustomAttribute::Property>());
    attribute->SetValue(NXString(value.c_str(), NXString::UTF8));
    return attribute;
}

class KeZiDialog
{
public:
    KeZiDialog()
    {
        session_ = Session::GetSession();
        ui_ = UI::GetUI();
        customFeatureManager_ = session_->CustomFeatureClassManager();
        editedFeature_ = customFeatureManager_ == nullptr
            ? nullptr
            : customFeatureManager_->GetEditedCustomFeature();
        const std::string dlxPath =
            zhihui_embedded_dialog::ExtractDlxToRandomPath(IDR_KEZI_DLX);
        if (dlxPath.empty())
        {
            throw std::runtime_error("KeZi dialog resource is missing.");
        }
        dialog_ = ui_->CreateDialog(dlxPath.c_str());
        dialog_->AddApplyHandler(make_callback(this, &KeZiDialog::ApplyCb));
        dialog_->AddOkHandler(make_callback(this, &KeZiDialog::OkCb));
        dialog_->AddCancelHandler(make_callback(this, &KeZiDialog::CancelCb));
        dialog_->AddCloseHandler(make_callback(this, &KeZiDialog::CancelCb));
        dialog_->AddUpdateHandler(make_callback(this, &KeZiDialog::UpdateCb));
        dialog_->AddInitializeHandler(make_callback(this, &KeZiDialog::InitializeCb));
        dialog_->AddDialogShownHandler(make_callback(this, &KeZiDialog::DialogShownCb));
        gActiveKeZiDialog = this;
    }

    ~KeZiDialog()
    {
        Log(session_, "KeZiDialog析构开始");
        ClearPreviewBuilder();
        if (editRollbackManager_ != nullptr)
        {
            try { FinishEditedFeatureRollback(true); } catch (...) {}
        }
        Log(session_, "KeZiDialog析构: 跳过delete dialog，避免NX已释放后重复释放DialogCreator");
        dialog_ = nullptr;
        if (gActiveKeZiDialog == this) { gActiveKeZiDialog = nullptr; }
        Log(session_, "KeZiDialog析构完成");
    }

    int BuildModernCustomFeatureConstruction(Features::CustomFeaturePreUpdateEvent* event)
    {
        if (event == nullptr) { return 1; }
        const std::vector<Features::ConstructionFeatureData*> existing = event->GetConstructionFeatures();
        Log(session_, std::string("单线刻字PreUpdate: existing=") +
                          std::to_string(existing.size()) +
                          ", pending=" + std::to_string(modernConstructionFeatureTags_.size()) +
                          ", building=" + (buildingModernCustomFeature_ ? "1" : "0"));
        if (modernConstructionFeatureTags_.empty() && !existing.empty())
        {
            event->SetConstructionFeatures(existing);
            Log(session_, "单线刻字PreUpdate: 没有待替换特征，保留当前内部链");
            return 0;
        }
        if (modernConstructionFeatureTags_.empty())
        {
            return 1;
        }
        std::vector<Features::ConstructionFeatureData*> construction;
        for (tag_t featureTag : modernConstructionFeatureTags_)
        {
            if (featureTag == NULL_TAG || UF_OBJ_ask_status(featureTag) != UF_OBJ_ALIVE) { continue; }
            Features::Feature* feature = dynamic_cast<Features::Feature*>(NXObjectManager::Get(featureTag));
            if (feature == nullptr || feature->IsInternal()) { continue; }
            Features::ConstructionFeatureData* item = event->CreateConstructionFeatureData(feature);
            item->SetShowInGraphicView(true);
            construction.push_back(item);
        }
        if (construction.empty()) { return 1; }
        event->SetConstructionFeatures(construction);
        Log(session_, std::string("单线刻字自定义特征已登记内部节点，数量=") +
                          std::to_string(construction.size()) +
                          ", 替换旧数量=" + std::to_string(existing.size()));
        modernConstructionFeatureTags_.clear();
        return 0;
    }

    int Show()
    {
        Log(session_, "对话框Launch开始");
        const int result = static_cast<int>(dialog_->LaunchInDialogMode(
            editedFeature_ != nullptr
                ? BlockDialog::DialogModeEdit
                : BlockDialog::DialogModeCreate));
        Log(session_, std::string("对话框Launch结束 result=") + std::to_string(result));
        if (deferredEditedReplacement_)
        {
            if (gDeferredKeZiDialog != nullptr && gDeferredKeZiDialog != this)
            {
                throw std::runtime_error("Another deferred engraving edit is still pending.");
            }
            gDeferredKeZiDialog = this;
            deferredTimerId_ = SetTimer(nullptr, 0, 50, &KeZiDialog::DeferredReplacementTimerProc);
            if (deferredTimerId_ == 0)
            {
                gDeferredKeZiDialog = nullptr;
                RestoreDisplayAfterDeferredReplacement();
                throw std::runtime_error("Could not schedule the deferred engraving replacement.");
            }
            Log(session_, "编辑单线刻字: 已排队到NX命令返回后的主线程消息回调");
        }
        return result;
    }

    bool HasScheduledReplacement() const
    {
        return deferredTimerId_ != 0;
    }

private:
    static void CALLBACK DeferredReplacementTimerProc(HWND, UINT, UINT_PTR timerId, DWORD)
    {
        KeZiDialog* dialog = gDeferredKeZiDialog;
        if (timerId != 0) { KillTimer(nullptr, timerId); }
        gDeferredKeZiDialog = nullptr;
        if (dialog == nullptr) { return; }
        dialog->deferredTimerId_ = 0;
        const int initializeResult = UF_initialize();
        try
        {
            Log(dialog->session_, "编辑单线刻字: NX命令已返回，开始主线程延时替换");
            dialog->deferredEditedReplacement_ = false;
            dialog->executingDeferredEditedReplacement_ = true;
            const int result = dialog->ApplyCb();
            dialog->executingDeferredEditedReplacement_ = false;
            Log(dialog->session_, result == 0
                                      ? "编辑单线刻字: 主线程延时替换完成"
                                      : "编辑单线刻字: 主线程延时替换失败");
        }
        catch (...)
        {
            dialog->executingDeferredEditedReplacement_ = false;
            Log(dialog->session_, "编辑单线刻字: 主线程延时替换未知异常");
        }
        dialog->RestoreDisplayAfterDeferredReplacement();
        if (initializeResult == 0) { UF_terminate(); }
        delete dialog;
    }

    void SuppressDisplayForDeferredReplacement()
    {
        if (deferredDisplaySuppressed_)
        {
            return;
        }
        int displayState = UF_DISP_UNSUPPRESS_DISPLAY;
        if (UF_DISP_ask_display(&displayState) != 0 ||
            displayState != UF_DISP_SUPPRESS_DISPLAY)
        {
            if (UF_DISP_set_display(UF_DISP_SUPPRESS_DISPLAY) == 0)
            {
                deferredDisplaySuppressed_ = true;
                Log(session_, "编辑单线刻字: 已暂停模型显示刷新");
            }
        }
    }

    void RestoreDisplayAfterDeferredReplacement()
    {
        if (!deferredDisplaySuppressed_)
        {
            return;
        }
        deferredDisplaySuppressed_ = false;
        UF_DISP_set_display(UF_DISP_UNSUPPRESS_DISPLAY);
        UF_DISP_regenerate_display();
        UF_DISP_make_display_up_to_date();
        Log(session_, "编辑单线刻字: 已恢复模型显示并一次性刷新");
    }

    void InitializeCb()
    {
        try
        {
            InitializeCbCore();
        }
        catch (const NXException& ex)
        {
            Log(session_, std::string("初始化对话框NX异常: ") + ex.Message());
        }
        catch (const std::exception& ex)
        {
            Log(session_, std::string("初始化对话框std异常: ") + ex.what());
        }
        catch (...)
        {
            Log(session_, "初始化对话框未知异常");
        }
    }

    void InitializeCbCore()
    {
        Log(session_, "初始化对话框");
        const bool afterReplace = componentReplacedInApply_;
        componentReplacedInApply_ = false;
        suppressPreviewUntilSelection_ = afterReplace;
        if (afterReplace)
        {
            Log(session_, "初始化对话框: Apply后重启，执行安全初始化");
        }
        Log(session_, "初始化对话框: LoadConfig开始");
        config_ = LoadConfig();
        Log(session_, "初始化对话框: LoadConfig完成");
        Log(session_, "初始化对话框: TopBlock开始");
        CompositeBlock* top = dialog_->TopBlock();
        Log(session_, "初始化对话框: TopBlock完成");
        Log(session_, "初始化对话框: FindBlock开始");
        mode_ = dynamic_cast<Enumeration*>(top->FindBlock("mode"));
        manualFace_ = dynamic_cast<BlockStyler::SelectObject*>(top->FindBlock("manualFace"));
        orientation_ = dynamic_cast<SpecifyOrientation*>(top->FindBlock("orientation"));
        rotate90Button_ = dynamic_cast<Button*>(top->FindBlock("rotate90Button"));
        centerLongSide_ = dynamic_cast<Toggle*>(top->FindBlock("centerLongSide"));
        centerShortSide_ = dynamic_cast<Toggle*>(top->FindBlock("centerShortSide"));
        xLongSide_ = dynamic_cast<Toggle*>(top->FindBlock("xLongSide"));
        xShortSide_ = dynamic_cast<Toggle*>(top->FindBlock("xShortSide"));
        ruleValue_ = dynamic_cast<StringBlock*>(top->FindBlock("ruleValue"));
        textValue_ = dynamic_cast<StringBlock*>(top->FindBlock("textValue"));
        appendBodyName_ = dynamic_cast<Button*>(top->FindBlock("appendBodyName"));
        appendFileName_ = dynamic_cast<Button*>(top->FindBlock("appendFileName"));
        appendSerial_ = dynamic_cast<Button*>(top->FindBlock("appendSerial"));
        appendAttribute_ = dynamic_cast<Button*>(top->FindBlock("appendAttribute"));
        fontName_ = dynamic_cast<StringBlock*>(top->FindBlock("fontName"));
        textHeight_ = dynamic_cast<DoubleBlock*>(top->FindBlock("textHeight"));
        depth_ = dynamic_cast<DoubleBlock*>(top->FindBlock("depth"));
        textColor_ = dynamic_cast<ObjectColorPicker*>(top->FindBlock("textColor"));
        boundary_ = dynamic_cast<Enumeration*>(top->FindBlock("boundary"));
        boundaryDepth_ = dynamic_cast<DoubleBlock*>(top->FindBlock("boundaryDepth"));
        boundaryColor_ = dynamic_cast<ObjectColorPicker*>(top->FindBlock("boundaryColor"));
        margin_ = dynamic_cast<DoubleBlock*>(top->FindBlock("margin"));
        wScale_ = dynamic_cast<DoubleBlock*>(top->FindBlock("wScale"));
        lockAspect_ = dynamic_cast<Toggle*>(top->FindBlock("lockAspect"));
        shear_ = dynamic_cast<DoubleBlock*>(top->FindBlock("shear"));
        textLayer_ = dynamic_cast<IntegerBlock*>(top->FindBlock("textLayer"));
        embossedText_ = dynamic_cast<Toggle*>(top->FindBlock("embossedText"));
        verticalText_ = dynamic_cast<Toggle*>(top->FindBlock("verticalText"));
        vShapeWidth_ = dynamic_cast<DoubleBlock*>(top->FindBlock("vShapeWidth"));
        renameComponentToText_ = dynamic_cast<Toggle*>(top->FindBlock("renameComponentToText"));
        engraveSameBodies_ = dynamic_cast<Toggle*>(top->FindBlock("engraveSameBodies"));
        sameRandomColor_ = dynamic_cast<Toggle*>(top->FindBlock("sameRandomColor"));
        autoEngraveVisibleTubes_ = dynamic_cast<Toggle*>(top->FindBlock("autoEngraveVisibleTubes"));
        hideEngravedText_ = dynamic_cast<Toggle*>(top->FindBlock("hideEngravedText"));
        editConfig_ = dynamic_cast<Button*>(top->FindBlock("editConfig"));
        Log(session_, "初始化对话框: FindBlock完成");
        isAssemblyContext_ = IsAssemblyContext(session_);
        ruleText_ = config_.textTemplate;
        if (Trim(ruleText_).empty() || Trim(ruleText_) == "{文本}" || Trim(ruleText_) == "{text}")
        {
            ruleText_ = config_.text;
        }
        if (Trim(ruleText_) == "{流水号}" || Trim(ruleText_) == "{serial}")
        {
            ruleText_ = config_.text;
        }
        Log(session_, "初始化对话框: ApplyConfigToDialog开始");
        ApplyConfigToDialog();
        Log(session_, "初始化对话框: ApplyConfigToDialog完成");

        if (manualFace_ != nullptr)
        {
            Log(session_, "初始化对话框: 设置选择块开始");
            if (afterReplace)
            {
                try
                {
                    std::vector<TaggedObject*> emptySelection;
                    manualFace_->SetSelectedObjects(emptySelection);
                    Log(session_, "初始化对话框: 已清空Apply前旧选择");
                }
                catch (const NXException& ex)
                {
                    Log(session_, std::string("初始化对话框: 清空旧选择失败: ") + ex.Message());
                }
                catch (...)
                {
                    Log(session_, "初始化对话框: 清空旧选择失败: 未知异常");
                }
            }
            manualFace_->SetAutomaticProgression(false);
            manualFace_->SetMaximumScopeAsString("Within Work Part and Components");
            manualFace_->SetCreateInterpartLink(false);
            manualFace_->SetInterpartSelectionAsString("Simple");
            ApplyPlanarFaceSelectionFilter(manualFace_);
            Log(session_, "初始化对话框: 设置选择块完成");
        }
        if (orientation_ != nullptr)
        {
            orientation_->SetVisibleManipulatorHandles(0x47);
        }
        LoadEditedModernCustomFeatureData();
        if (editedFeature_ != nullptr)
        {
            BeginEditedFeatureRollback();
            // Topology references are resolved again after NX has rolled the
            // model to immediately before the existing engraving node.
            LoadEditedModernCustomFeatureData();
        }
        if (textValue_ != nullptr)
        {
            if (!config_.text.empty())
            {
                textValue_->SetValue(config_.text.c_str());
            }
            else if (TextTemplateHasSerial(ruleText_))
            {
                const std::string serialText = SerialNumberText(config_);
                textValue_->SetValue(serialText.c_str());
            }
            UpdateTextRuleLabel();
        }
        Log(session_, "初始化对话框: UpdateRuleInputValue开始");
        UpdateRuleInputValue();
        if (editedFeature_ != nullptr)
        {
            Log(session_, "初始化对话框: 编辑单线刻字节点，保留节点文字和定位手柄");
        }
        else if (!afterReplace)
        {
            Log(session_, "初始化对话框: UpdateResolvedTextFromRule开始");
            UpdateResolvedTextFromRule();
        }
        else
        {
            Log(session_, "初始化对话框: Apply后重启，跳过规则解析，等待重新选面");
        }
        Log(session_, "初始化对话框: UpdateUiState开始");
        UpdateUiState();
        Log(session_, "对话框初始化完成");
    }

    void ApplyConfigToDialog()
    {
        SetEnumValue(mode_, config_.mode);
        SetEnumValue(boundary_, config_.boundary);
        if (fontName_ != nullptr)
        {
            fontName_->SetValue(config_.fontName.c_str());
            lastValidFontName_ = config_.fontName;
            normalFontName_ = config_.fontName;
        }
        if (textHeight_ != nullptr)
        {
            textHeight_->SetValue(config_.height);
        }
        if (depth_ != nullptr)
        {
            depth_->SetValue(config_.depth);
        }
        SetColorValue(textColor_, config_.textColor);
        if (boundaryDepth_ != nullptr)
        {
            boundaryDepth_->SetValue(config_.boundaryDepth);
        }
        SetColorValue(boundaryColor_, config_.boundaryColor);
        if (margin_ != nullptr)
        {
            margin_->SetValue(config_.textLength);
        }
        if (wScale_ != nullptr)
        {
            wScale_->SetValue(config_.widthScale);
        }
        if (lockAspect_ != nullptr)
        {
            lockAspect_->SetValue(config_.lockAspect);
        }
        if (shear_ != nullptr)
        {
            shear_->SetValue(config_.shear);
        }
        if (textLayer_ != nullptr)
        {
            textLayer_->SetValue(config_.layer);
        }
        if (embossedText_ != nullptr)
        {
            embossedText_->SetValue(config_.embossed);
        }
        if (verticalText_ != nullptr)
        {
            verticalText_->SetValue(config_.vShape);
        }
        if (config_.vShape && fontName_ != nullptr)
        {
            fontName_->SetValue(kVShapeFontName);
            lastValidFontName_ = kVShapeFontName;
        }
        if (vShapeWidth_ != nullptr)
        {
            vShapeWidth_->SetValue(config_.vShapeWidth);
        }
        if (renameComponentToText_ != nullptr)
        {
            renameComponentToText_->SetValue(config_.renameComponentToText && isAssemblyContext_);
        }
        if (engraveSameBodies_ != nullptr)
        {
            engraveSameBodies_->SetValue(config_.engraveSameBodies && !isAssemblyContext_);
        }
        if (sameRandomColor_ != nullptr)
        {
            sameRandomColor_->SetValue(config_.sameRandomColor && !isAssemblyContext_);
        }
        if (autoEngraveVisibleTubes_ != nullptr)
        {
            autoEngraveVisibleTubes_->SetValue(config_.autoEngraveVisibleTubes);
        }
        if (hideEngravedText_ != nullptr)
        {
            hideEngravedText_->SetValue(config_.hideEngravedText);
        }
        if (centerLongSide_ != nullptr)
        {
            centerLongSide_->SetValue(config_.centerLongSide);
        }
        if (centerShortSide_ != nullptr)
        {
            centerShortSide_->SetValue(config_.centerShortSide);
        }
        if (xLongSide_ != nullptr)
        {
            xLongSide_->SetValue(config_.xLongSide);
        }
        if (xShortSide_ != nullptr)
        {
            xShortSide_->SetValue(config_.xShortSide);
        }
    }

    void LoadEditedModernCustomFeatureData()
    {
        if (editedFeature_ == nullptr) { return; }
        loadingEditedFeature_ = true;
        try
        {
            Features::CustomFeatureData* data = editedFeature_->FeatureData();
            const std::string text = ToString(data->CustomStringAttributeByName(
                zhihui_kezi_custom_feature::kAttrText)->Value());
            const Point3d origin(
                data->CustomDoubleAttributeByName(zhihui_kezi_custom_feature::kAttrOriginX)->Value(),
                data->CustomDoubleAttributeByName(zhihui_kezi_custom_feature::kAttrOriginY)->Value(),
                data->CustomDoubleAttributeByName(zhihui_kezi_custom_feature::kAttrOriginZ)->Value());
            double matrixValues[9] = {};
            for (int index = 0; index < 9; ++index)
            {
                const std::string name = std::string(zhihui_kezi_custom_feature::kAttrMatrixPrefix) +
                                         std::to_string(index);
                matrixValues[index] = data->CustomDoubleAttributeByName(name.c_str())->Value();
            }
            config_.text = text;
            config_.textTemplate = text;
            ruleText_ = text;
            config_.height = data->CustomDoubleAttributeByName(
                zhihui_kezi_custom_feature::kAttrHeight)->Value();
            config_.depth = data->CustomDoubleAttributeByName(
                zhihui_kezi_custom_feature::kAttrDepth)->Value();
            config_.vShapeWidth = data->CustomDoubleAttributeByName(
                zhihui_kezi_custom_feature::kAttrWidth)->Value();
            config_.widthScale = data->CustomDoubleAttributeByName(
                zhihui_kezi_custom_feature::kAttrWidthScale)->Value();
            config_.shear = data->CustomDoubleAttributeByName(
                zhihui_kezi_custom_feature::kAttrShear)->Value();
            config_.vShape = true;
            config_.autoEngraveVisibleTubes = false;
            // Editing one CustomFeature must never inherit the global batch
            // switches.  Otherwise a double-click edit can also engrave the
            // matching bodies and leave those extra features outside the node.
            config_.engraveSameBodies = false;
            config_.sameRandomColor = false;
            config_.renameComponentToText = false;
            config_.hideEngravedText = false;
            config_.centerLongSide = false;
            config_.centerShortSide = false;
            ApplyConfigToDialog();
            if (ruleValue_ != nullptr) { ruleValue_->SetValue(text.c_str()); }
            if (textValue_ != nullptr) { textValue_->SetValue(text.c_str()); }
            if (orientation_ != nullptr)
            {
                orientation_->SetOriginSpecified(true);
                orientation_->SetOrigin(origin);
                orientation_->SetXAxis(Vector3d(matrixValues[0], matrixValues[1], matrixValues[2]));
                orientation_->SetYAxis(Vector3d(matrixValues[3], matrixValues[4], matrixValues[5]));
                orientation_->SetVisibleManipulatorHandles(0x47);
            }
            bool restoredFace = false;
            try
            {
                TaggedObject* storedFace = data->CustomTagAttributeByName(
                    zhihui_kezi_custom_feature::kAttrFace)->Value();
                if (storedFace != nullptr && manualFace_ != nullptr)
                {
                    manualFace_->SetSelectedObjects(std::vector<TaggedObject*>{storedFace});
                    restoredFace = true;
                }
            }
            catch (...)
            {
            }
            if (!restoredFace && manualFace_ != nullptr)
            {
                Body* targetBody = dynamic_cast<Body*>(data->CustomTagAttributeByName(
                    zhihui_kezi_custom_feature::kAttrTargetBody)->Value());
                const Vector3d storedNormal = Normalize(
                    Vector3d(matrixValues[6], matrixValues[7], matrixValues[8]),
                    Vector3d(0.0, 0.0, 1.0));
                if (targetBody != nullptr)
                {
                    for (Face* candidate : targetBody->GetFaces())
                    {
                        Point3d candidatePoint;
                        Vector3d candidateNormal;
                        if (!AskPlanarFaceData(candidate, &origin, &candidatePoint, &candidateNormal) ||
                            std::fabs(Dot(Normalize(candidateNormal, storedNormal), storedNormal)) < 0.999)
                        {
                            continue;
                        }
                        const Vector3d delta(origin.X - candidatePoint.X,
                                             origin.Y - candidatePoint.Y,
                                             origin.Z - candidatePoint.Z);
                        if (std::fabs(Dot(delta, Normalize(candidateNormal, storedNormal))) > 0.01)
                        {
                            continue;
                        }
                        double point[3] = {origin.X, origin.Y, origin.Z};
                        int containment = 0;
                        if (UF_MODL_ask_point_containment(point, candidate->Tag(), &containment) == 0 &&
                            containment == 1)
                        {
                            manualFace_->SetSelectedObjects(std::vector<TaggedObject*>{candidate});
                            restoredFace = true;
                            Log(session_, std::string("编辑单线刻字: 已在回滚实体上重新定位刻字平面，tag=") +
                                              std::to_string(candidate->Tag()));
                            break;
                        }
                    }
                }
            }
            if (!restoredFace)
            {
                Log(session_, "编辑单线刻字: 未能恢复刻字平面");
            }
            Log(session_, std::string("编辑单线刻字: 手柄已关联文字中心 (") +
                              FormatNumber(origin.X) + "," + FormatNumber(origin.Y) + "," +
                              FormatNumber(origin.Z) + ")");
        }
        catch (...)
        {
            loadingEditedFeature_ = false;
            throw;
        }
        loadingEditedFeature_ = false;
    }

    void BeginEditedFeatureRollback()
    {
        if (editedFeature_ == nullptr || editRollbackManager_ != nullptr) { return; }
        Part* workPart = session_->Parts()->Work();
        if (workPart == nullptr)
        {
            throw std::runtime_error("No work part is active for single-line engraving edit.");
        }
        editRollbackMark_ = session_->SetUndoMark(
            Session::MarkVisibilityVisible, "KeZi Single-Line Edit With Rollback");
        try
        {
            editRollbackManager_ = workPart->Features()->StartEditWithRollbackManager(
                editedFeature_, editRollbackMark_);
            if (editRollbackManager_ == nullptr)
            {
                throw std::runtime_error("NX did not start edit-with-rollback for the engraving node.");
            }
            Log(session_, "编辑单线刻字: 已回滚到原节点之前，旧刻字暂时移除");
        }
        catch (...)
        {
            if (editRollbackMark_ != static_cast<Session::UndoMarkId>(0))
            {
                try { session_->DeleteUndoMark(editRollbackMark_, "KeZi Single-Line Edit With Rollback"); } catch (...) {}
                editRollbackMark_ = static_cast<Session::UndoMarkId>(0);
            }
            throw;
        }
    }

    void FinishEditedFeatureRollback(const bool errorDuringEdit)
    {
        if (editRollbackManager_ == nullptr) { return; }
        Features::EditWithRollbackManager* manager = editRollbackManager_;
        editRollbackManager_ = nullptr;
        try
        {
            manager->UpdateFeature(errorDuringEdit);
            manager->Stop();
            manager->Destroy();
            Log(session_, errorDuringEdit
                              ? "编辑单线刻字: 已取消编辑并恢复原刻字"
                              : "编辑单线刻字: 已接受新刻字并滚动恢复后续模型");
        }
        catch (...)
        {
            try { manager->Stop(); } catch (...) {}
            try { manager->Destroy(); } catch (...) {}
            throw;
        }
        if (editRollbackMark_ != static_cast<Session::UndoMarkId>(0))
        {
            try { session_->DeleteUndoMark(editRollbackMark_, "KeZi Single-Line Edit With Rollback"); } catch (...) {}
            editRollbackMark_ = static_cast<Session::UndoMarkId>(0);
        }
    }

    void DialogShownCb()
    {
        Log(session_, "对话框显示，读取当前控件值");
        isAssemblyContext_ = IsAssemblyContext(session_);
        UpdateUiState();
        ApplyExclusiveAxisToggle(xLongSide_);
        ApplyExclusiveAxisToggle(xShortSide_);
        if (orientation_ != nullptr)
        {
            orientation_->SetVisibleManipulatorHandles(0x47);
        }
        if (editedFeature_ == nullptr)
        {
            ApplyAxisModeToCurrentOrientation();
        }
        if (!suppressPreviewUntilSelection_)
        {
            RefreshPreview();
        }
        else
        {
            Log(session_, "对话框显示: 等待重新选面，暂不刷新预览");
        }
        if (textValue_ != nullptr)
        {
            UpdateTextRuleLabel();
            UpdateRuleInputValue();
            if (!suppressPreviewUntilSelection_ && editedFeature_ == nullptr)
            {
                UpdateResolvedTextFromRule();
            }
            else
            {
                try
                {
                    updatingResolvedText_ = true;
                    textValue_->SetValue(config_.text.c_str());
                    updatingResolvedText_ = false;
                    Log(session_, std::string("对话框显示: 已回填递增文本=") + config_.text);
                }
                catch (const NXException& ex)
                {
                    updatingResolvedText_ = false;
                    Log(session_, std::string("对话框显示: 回填递增文本失败: ") + ex.Message());
                }
                catch (...)
                {
                    updatingResolvedText_ = false;
                    Log(session_, "对话框显示: 回填递增文本失败: 未知异常");
                }
                Log(session_, "对话框显示: Apply后重启，跳过规则解析");
            }
            textValue_->Focus();
        }
    }

    int CancelCb()
    {
        Log(session_, "取消/关闭对话框");
        deferredEditedReplacement_ = false;
        if (!suppressPreviewUntilSelection_)
        {
            SaveCurrentRule();
        }
        else
        {
            Log(session_, "取消/关闭对话框: Apply后重启状态，不保存旧控件模板");
        }
        ClearPreviewBuilder();
        if (editRollbackManager_ != nullptr)
        {
            FinishEditedFeatureRollback(true);
        }
        return 0;
    }

    int UpdateCb(UIBlock* block)
    {
        if (applying_ || loadingEditedFeature_)
        {
            return 0;
        }
        if (suppressPreviewUntilSelection_ && block != manualFace_)
        {
            return 0;
        }
        if (block == manualFace_)
        {
            suppressPreviewUntilSelection_ = false;
        }
        if (block == ruleValue_ && updatingRuleInput_)
        {
            return 0;
        }
        if (block == textValue_ && updatingResolvedText_)
        {
            return 0;
        }

        if (block == ruleValue_)
        {
            ruleText_ = ReadString(ruleValue_);
            SaveCurrentRule();
            UpdateResolvedTextFromRule();
            RefreshPreview();
            return 0;
        }

        if (block == appendBodyName_)
        {
            Log(session_, "追加变量: 体名");
            AppendTextToken("{体名}");
            RefreshPreview();
        }
        else if (block == appendFileName_)
        {
            Log(session_, "追加变量: 文件名");
            AppendTextToken("{文件名}");
            RefreshPreview();
        }
        else if (block == appendSerial_)
        {
            Log(session_, "追加变量: 流水号");
            AppendTextToken("{流水号}");
            RefreshPreview();
        }
        else if (block == appendAttribute_)
        {
            ChooseAndAppendAttributeToken();
        }
        else if (block == rotate90Button_)
        {
            Log(session_, "旋转方位 90 度");
            RotateOrientationNinetyDegrees();
            RefreshPreview();
        }
        else if (block == mode_ || block == boundary_ || block == verticalText_)
        {
            if (block == verticalText_)
            {
                ApplyVShapeFontSelection();
            }
            UpdateUiState();
            RefreshPreview();
        }
        else if (block == lockAspect_)
        {
            UpdateUiState();
            RefreshPreview();
        }
        else if (block == fontName_)
        {
            if (!selectingSystemFont_ && Trim(ReadString(fontName_)) == "More...")
            {
                selectingSystemFont_ = true;
                std::string selectedFont;
                const std::string fallback = lastValidFontName_.empty() ? std::string("Arial") : lastValidFontName_;
                if (AskSystemFontName(fallback, &selectedFont))
                {
                    lastValidFontName_ = selectedFont;
                    fontName_->SetValue(selectedFont.c_str());
                }
                else
                {
                    fontName_->SetValue(fallback.c_str());
                }
                selectingSystemFont_ = false;
            }
            else if (!selectingSystemFont_)
            {
                const std::string selectedFont = Trim(ReadString(fontName_));
                if (!selectedFont.empty())
                {
                    lastValidFontName_ = selectedFont;
                }
            }
            RefreshPreview();
        }
        else if (block == autoEngraveVisibleTubes_)
        {
            UpdateUiState();
            if (ReadToggle(autoEngraveVisibleTubes_, false))
            {
                ClearPreviewBuilder();
            }
            else
            {
                RefreshPreview();
            }
        }
        else if (block == manualFace_)
        {
            MoveOrientationToSelectedFace();
            if (orientation_ != nullptr)
            {
                orientation_->Focus();
            }
            RefreshPreview();
        }
        else if (block == xLongSide_ || block == xShortSide_)
        {
            ApplyExclusiveAxisToggle(block);
            ApplyAxisModeToCurrentOrientation();
            RefreshPreview();
        }
        else if (block == manualFace_ || block == orientation_ || block == textValue_ ||
                 block == textHeight_ || block == depth_ || block == textColor_ || block == boundaryDepth_ ||
                 block == boundaryColor_ || block == margin_ || block == wScale_ || block == lockAspect_ ||
                 block == shear_ || block == textLayer_ || block == embossedText_ || block == vShapeWidth_ ||
                 block == centerLongSide_ || block == centerShortSide_ || block == xLongSide_ || block == xShortSide_)
        {
            if (block == orientation_ || block == centerLongSide_ || block == centerShortSide_)
            {
                SnapOrientationToCenterOptions();
            }
            RefreshPreview();
        }
        return 0;
    }

    int ApplyCb()
    {
        Log(session_, "ApplyCb进入");
        // NX's edit-mode Block Styler owns the edited CustomFeature until
        // LaunchInDialogMode returns. Deleting that node inside Apply/OK makes
        // NX dereference its stale pointer while closing the dialog. Apply in
        // edit mode therefore records the request; OK performs it after exit.
        if (editedFeature_ != nullptr && !executingDeferredEditedReplacement_)
        {
            deferredEditedReplacement_ = true;
            Log(session_, "编辑单线刻字: 已记录替换请求，等待对话框退出后执行");
            return 0;
        }
        applying_ = true;
        Session::UndoMarkId replacementMark = static_cast<Session::UndoMarkId>(0);
        try
        {
            if (editedFeature_ != nullptr)
            {
                replacementMark = ReplaceEditedModernCustomFeatureBeforeCreate();
            }
            ApplyEngraving();
            if (editRollbackManager_ != nullptr)
            {
                FinishEditedFeatureRollback(false);
            }
            if (replacementMark != static_cast<Session::UndoMarkId>(0))
            {
                session_->SetUndoMarkName(replacementMark, "编辑单线刻字");
                Log(session_, "编辑单线刻字: 旧节点已由新打包节点整体替换");
            }
        }
        catch (const NXException& ex)
        {
            if (editRollbackManager_ != nullptr)
            {
                try { FinishEditedFeatureRollback(true); } catch (...) {}
            }
            if (replacementMark != static_cast<Session::UndoMarkId>(0))
            {
                try { session_->UndoToMark(replacementMark, "编辑单线刻字"); } catch (...) {}
                try { session_->DeleteUndoMark(replacementMark, "编辑单线刻字"); } catch (...) {}
                Log(session_, "编辑单线刻字: 新节点生成失败，已撤销并恢复原节点");
            }
            applying_ = false;
            Log(session_, std::string("刻字失败: NXException code=") +
                              std::to_string(ex.ErrorCode()) + ", message=" + ex.Message());
            if (ex.ErrorCode() == 66 || ex.ErrorCode() == UF_err_operation_aborted)
            {
                Log(session_, "用户已取消刻字，ApplyCb立即返回");
                return 1;
            }
            ShowError("KeZi Engrave Text", std::string("Engraving failed: ") + ex.Message());
            Log(session_, "ApplyCb异常返回=1");
            return 1;
        }
        catch (const std::exception& ex)
        {
            if (editRollbackManager_ != nullptr)
            {
                try { FinishEditedFeatureRollback(true); } catch (...) {}
            }
            if (replacementMark != static_cast<Session::UndoMarkId>(0))
            {
                try { session_->UndoToMark(replacementMark, "编辑单线刻字"); } catch (...) {}
                try { session_->DeleteUndoMark(replacementMark, "编辑单线刻字"); } catch (...) {}
                Log(session_, "编辑单线刻字: 新节点生成失败，已撤销并恢复原节点");
            }
            applying_ = false;
            Log(session_, std::string("刻字失败: ") + ex.what());
            ShowError("KeZi Engrave Text", std::string("Engraving failed: ") + ex.what());
            Log(session_, "ApplyCb异常返回=1");
            return 1;
        }
        catch (...)
        {
            if (editRollbackManager_ != nullptr)
            {
                try { FinishEditedFeatureRollback(true); } catch (...) {}
            }
            if (replacementMark != static_cast<Session::UndoMarkId>(0))
            {
                try { session_->UndoToMark(replacementMark, "编辑单线刻字"); } catch (...) {}
                try { session_->DeleteUndoMark(replacementMark, "编辑单线刻字"); } catch (...) {}
                Log(session_, "编辑单线刻字: 新节点生成失败，已撤销并恢复原节点");
            }
            applying_ = false;
            Log(session_, "刻字失败: 未知异常");
            ShowError("刻字", "刻字失败: 未知异常");
            Log(session_, "ApplyCb未知异常返回=1");
            return 1;
        }
        applying_ = false;
        Log(session_, "ApplyCb正常返回=0");
        return 0;
    }

    int OkCb()
    {
        Log(session_, "OkCb进入");
        if (editedFeature_ != nullptr)
        {
            deferredReplacementSettings_ = ReadSettings();
            if (orientation_ != nullptr && orientation_->IsOriginSpecified())
            {
                deferredReplacementOrigin_ = orientation_->Origin();
                const Vector3d capturedX = Normalize(
                    orientation_->XAxis(), Vector3d(1.0, 0.0, 0.0));
                Vector3d capturedY = Normalize(
                    orientation_->YAxis(), Vector3d(0.0, 1.0, 0.0));
                const Vector3d capturedZ = Normalize(
                    Cross(capturedX, capturedY), Vector3d(0.0, 0.0, 1.0));
                capturedY = Normalize(Cross(capturedZ, capturedX), capturedY);
                deferredReplacementMatrix_ = MakeMatrix(capturedX, capturedY, capturedZ);
                deferredPlacementCaptured_ = true;
                std::ostringstream placementLog;
                placementLog << "编辑单线刻字: OK手柄快照 origin=("
                             << deferredReplacementOrigin_.X << ","
                             << deferredReplacementOrigin_.Y << ","
                             << deferredReplacementOrigin_.Z << ")";
                Log(session_, placementLog.str());
            }
            deferredReplacementText_ = textValue_ == nullptr
                ? config_.text
                : Trim(ReadString(textValue_));
            deferredEditedReplacement_ = true;
            // Suppress before deleting the preview and before NX closes edit
            // mode. Otherwise the restored old node is painted for one frame
            // while the delayed safe replacement is being scheduled.
            SuppressDisplayForDeferredReplacement();
            ClearPreviewBuilder();
            Log(session_, "编辑单线刻字: OK已接收，节点将在对话框退出后替换");
            return 0;
        }
        return ApplyCb();
    }

    Session::UndoMarkId ReplaceEditedModernCustomFeatureBeforeCreate()
    {
        if (editedFeature_ == nullptr)
        {
            return static_cast<Session::UndoMarkId>(0);
        }
        Part* workPart = session_ == nullptr ? nullptr : session_->Parts()->Work();
        if (workPart == nullptr)
        {
            throw std::runtime_error("No work part is active while replacing the engraving node.");
        }

        Features::CustomFeature* oldFeature = editedFeature_;
        const tag_t oldFeatureTag = oldFeature->Tag();
        const int oldFeatureTimestamp = oldFeature->Timestamp();
        deferredReplacementAnchorTag_ = NULL_TAG;
        int nextTimestamp = std::numeric_limits<int>::max();
        for (Features::Feature* candidate : *workPart->Features())
        {
            if (candidate == nullptr || candidate->Tag() == oldFeatureTag)
            {
                continue;
            }
            const int candidateTimestamp = candidate->Timestamp();
            if (candidateTimestamp > oldFeatureTimestamp && candidateTimestamp < nextTimestamp)
            {
                nextTimestamp = candidateTimestamp;
                deferredReplacementAnchorTag_ = candidate->Tag();
            }
        }
        Log(session_, std::string("编辑单线刻字: 记录原特征树位置 timestamp=") +
                          std::to_string(oldFeatureTimestamp) +
                          ", nextTag=" + std::to_string(deferredReplacementAnchorTag_));
        Features::CustomFeatureData* oldData = oldFeature->FeatureData();
        Body* targetBody = dynamic_cast<Body*>(oldData->CustomTagAttributeByName(
            zhihui_kezi_custom_feature::kAttrTargetBody)->Value());
        const tag_t targetBodyTag = targetBody == nullptr ? NULL_TAG : targetBody->Tag();
        const Point3d origin(
            oldData->CustomDoubleAttributeByName(zhihui_kezi_custom_feature::kAttrOriginX)->Value(),
            oldData->CustomDoubleAttributeByName(zhihui_kezi_custom_feature::kAttrOriginY)->Value(),
            oldData->CustomDoubleAttributeByName(zhihui_kezi_custom_feature::kAttrOriginZ)->Value());
        double matrixValues[9] = {};
        for (int index = 0; index < 9; ++index)
        {
            const std::string name = std::string(zhihui_kezi_custom_feature::kAttrMatrixPrefix) +
                                     std::to_string(index);
            matrixValues[index] = oldData->CustomDoubleAttributeByName(name.c_str())->Value();
        }
        const Vector3d storedNormal = Normalize(
            Vector3d(matrixValues[6], matrixValues[7], matrixValues[8]),
            Vector3d(0.0, 0.0, 1.0));
        const std::size_t oldConstructionCount = oldFeature->GetConstructionFeatures().size();

        ClearPreviewBuilder();
        // Restore the original feature first. Deleting a feature while its
        // EditWithRollbackManager is active is unsupported by NX.
        FinishEditedFeatureRollback(true);

        const Session::UndoMarkId replacementMark = session_->SetUndoMark(
            Session::MarkVisibilityVisible, "编辑单线刻字");
        try
        {
            Update* update = session_->UpdateManager();
            update->ClearDeleteList();
            update->ClearErrorList();
            update->AddObjectsToDeleteList(std::vector<TaggedObject*>{oldFeature});
            const int deleteErrors = update->DoUpdate(replacementMark);
            update->ClearDeleteList();
            if (deleteErrors != 0 || UF_OBJ_ask_status(oldFeatureTag) == UF_OBJ_ALIVE)
            {
                throw std::runtime_error("NX did not delete the original single-line engraving node.");
            }
            Log(session_, std::string("编辑单线刻字: 已删除原节点 tag=") +
                              std::to_string(oldFeatureTag) +
                              ", 原内部特征数=" + std::to_string(oldConstructionCount));

            Face* replacementFace = nullptr;
            double bestPlaneDistance = std::numeric_limits<double>::max();
            // Deleting the boolean/custom-feature chain can replace NX's C++
            // wrapper for the target body even when its tag survives. Never
            // dereference the pre-delete Body* after DoUpdate.
            Body* restoredTargetBody = nullptr;
            if (targetBodyTag != NULL_TAG && UF_OBJ_ask_status(targetBodyTag) == UF_OBJ_ALIVE)
            {
                restoredTargetBody = dynamic_cast<Body*>(NXObjectManager::Get(targetBodyTag));
            }
            if (restoredTargetBody != nullptr)
            {
                for (Face* candidate : restoredTargetBody->GetFaces())
                {
                    Point3d candidatePoint;
                    Vector3d candidateNormal;
                    if (!AskPlanarFaceData(candidate, &origin, &candidatePoint, &candidateNormal))
                    {
                        continue;
                    }
                    const Vector3d unitNormal = Normalize(candidateNormal, storedNormal);
                    if (std::fabs(Dot(unitNormal, storedNormal)) < 0.999)
                    {
                        continue;
                    }
                    const Vector3d delta(origin.X - candidatePoint.X,
                                         origin.Y - candidatePoint.Y,
                                         origin.Z - candidatePoint.Z);
                    const double planeDistance = std::fabs(Dot(delta, unitNormal));
                    double point[3] = {origin.X, origin.Y, origin.Z};
                    int containment = 0;
                    const bool containsOrigin =
                        UF_MODL_ask_point_containment(point, candidate->Tag(), &containment) == 0 &&
                        containment == 1;
                    if (containsOrigin)
                    {
                        replacementFace = candidate;
                        break;
                    }
                    if (planeDistance < bestPlaneDistance)
                    {
                        bestPlaneDistance = planeDistance;
                        replacementFace = candidate;
                    }
                }
            }
            if (replacementFace == nullptr)
            {
                throw std::runtime_error("Could not relocate the engraving face after deleting the old node.");
            }

            editedFeature_ = nullptr;
            deferredReplacementFace_ = replacementFace;
            // The stored placement belongs to the old node and is used above
            // only to relocate its target face.  When the user moved/rotated
            // the edit handle, OkCb already captured the new placement before
            // Block Styler destroyed its controls; never overwrite it here.
            if (!deferredPlacementCaptured_)
            {
                deferredReplacementOrigin_ = origin;
                deferredReplacementMatrix_.Xx = matrixValues[0];
                deferredReplacementMatrix_.Xy = matrixValues[1];
                deferredReplacementMatrix_.Xz = matrixValues[2];
                deferredReplacementMatrix_.Yx = matrixValues[3];
                deferredReplacementMatrix_.Yy = matrixValues[4];
                deferredReplacementMatrix_.Yz = matrixValues[5];
                deferredReplacementMatrix_.Zx = matrixValues[6];
                deferredReplacementMatrix_.Zy = matrixValues[7];
                deferredReplacementMatrix_.Zz = matrixValues[8];
                Log(session_, "编辑单线刻字: 未取得手柄快照，沿用原节点位置");
            }
            else
            {
                Log(session_, "编辑单线刻字: 使用确定时的新手柄位置和方向");
            }
            Log(session_, std::string("编辑单线刻字: 已重新定位刻字面 tag=") +
                              std::to_string(replacementFace->Tag()) +
                              ", 开始创建新打包节点");
            return replacementMark;
        }
        catch (...)
        {
            try { session_->UndoToMark(replacementMark, "编辑单线刻字"); } catch (...) {}
            try { session_->DeleteUndoMark(replacementMark, "编辑单线刻字"); } catch (...) {}
            throw;
        }
    }

    void AppendTextToken(const std::string& token)
    {
        if (token.empty())
        {
            return;
        }
        ruleText_ = ReadString(ruleValue_);
        if (RuleAlreadyHasTokenType(ruleText_, token))
        {
            SaveCurrentRule();
            UpdateResolvedTextFromRule();
            return;
        }
        ruleText_ += token;
        UpdateTextRuleLabel();
        SaveCurrentRule();
        UpdateResolvedTextFromRule();
    }

    void UpdateTextRuleLabel()
    {
        if (textValue_ != nullptr)
        {
            textValue_->SetLabel("文本");
        }
    }

    void UpdateRuleInputValue()
    {
        if (ruleValue_ == nullptr)
        {
            return;
        }
        if (ReadString(ruleValue_) == ruleText_)
        {
            return;
        }
        updatingRuleInput_ = true;
        ruleValue_->SetValue(ruleText_.c_str());
        updatingRuleInput_ = false;
    }

    void SaveCurrentRule()
    {
        std::string rule = Trim(ruleText_);
        config_.textTemplate = rule;
        WriteConfigValue("模板", rule);
        UpdateRuleInputValue();
    }

    static std::string BoolText(const bool value)
    {
        return value ? "1" : "0";
    }

    void SaveDialogValues()
    {
        const TextSettings settings = ReadSettings();
        WriteConfigValue("模式", std::to_string(settings.mode));
        WriteConfigValue("文本", settings.text);
        const std::string savedFontName = settings.vShape && !normalFontName_.empty() ? normalFontName_ : settings.fontName;
        WriteConfigValue("字体", savedFontName);
        WriteConfigValue("高度", FormatNumber(settings.height));
        WriteConfigValue("深度", FormatNumber(settings.depth));
        WriteConfigValue("颜色", std::to_string(ReadColorIndex(textColor_, config_.textColor)));
        WriteConfigValue("边界", std::to_string(settings.boundary));
        WriteConfigValue("边界深度", FormatNumber(settings.boundaryDepth));
        WriteConfigValue("边界颜色", std::to_string(ReadColorIndex(boundaryColor_, config_.boundaryColor)));
        WriteConfigValue("长度", FormatNumber(settings.textLength));
        WriteConfigValue("W比例", FormatNumber(settings.widthScale));
        WriteConfigValue("锁定宽高比", BoolText(settings.lockAspect));
        WriteConfigValue("剪切", FormatNumber(settings.shear));
        WriteConfigValue("文本层", std::to_string(settings.layer));
        WriteConfigValue("凸起文本", BoolText(settings.embossed));
        WriteConfigValue("V形文本", BoolText(settings.vShape));
        WriteConfigValue("V形刻字宽度", FormatNumber(settings.vShapeWidth));
        WriteConfigValue("编号设为部件名", BoolText(settings.renameComponentToText));
        WriteConfigValue("刻相同", BoolText(settings.engraveSameBodies));
        WriteConfigValue("相同随机色", BoolText(settings.sameRandomColor));
        WriteConfigValue("所有可见方通自动刻字", BoolText(settings.autoEngraveVisibleTubes));
        WriteConfigValue("隐藏已刻字体", BoolText(settings.hideEngravedText));
        WriteConfigValue("长向居中", BoolText(settings.centerLongSide));
        WriteConfigValue("短向居中", BoolText(settings.centerShortSide));
        WriteConfigValue("X长边", BoolText(settings.xLongSide));
        WriteConfigValue("X短边", BoolText(settings.xShortSide));

        config_.mode = settings.mode;
        config_.text = settings.text;
        config_.fontName = savedFontName;
        config_.height = settings.height;
        config_.depth = settings.depth;
        config_.textColor = ReadColorIndex(textColor_, config_.textColor);
        config_.boundary = settings.boundary;
        config_.boundaryDepth = settings.boundaryDepth;
        config_.boundaryColor = ReadColorIndex(boundaryColor_, config_.boundaryColor);
        config_.textLength = settings.textLength;
        config_.widthScale = settings.widthScale;
        config_.lockAspect = settings.lockAspect;
        config_.shear = settings.shear;
        config_.layer = settings.layer;
        config_.embossed = settings.embossed;
        config_.vShape = settings.vShape;
        config_.vShapeWidth = settings.vShapeWidth;
        config_.renameComponentToText = settings.renameComponentToText;
        config_.engraveSameBodies = settings.engraveSameBodies;
        config_.sameRandomColor = settings.sameRandomColor;
        config_.autoEngraveVisibleTubes = settings.autoEngraveVisibleTubes;
        config_.hideEngravedText = settings.hideEngravedText;
        config_.centerLongSide = settings.centerLongSide;
        config_.centerShortSide = settings.centerShortSide;
        config_.xLongSide = settings.xLongSide;
        config_.xShortSide = settings.xShortSide;
    }

    void EnsureTextInputHasValue()
    {
        if (textValue_ == nullptr || !ReadString(textValue_).empty() || !TextTemplateHasSerial(ruleText_))
        {
            return;
        }
        const std::string serialText = SerialNumberText(config_);
        textValue_->SetValue(serialText.c_str());
    }

    void AppendDisplayValueForToken(const std::string& token)
    {
        if (textValue_ == nullptr)
        {
            return;
        }

        if (token == "{流水号}" || token == "{serial}")
        {
            const std::string current = ReadString(textValue_);
            const std::string serialText = SerialNumberText(config_);
            updatingResolvedText_ = true;
            textValue_->SetValue((current + serialText).c_str());
            updatingResolvedText_ = false;
        }
    }

    void SetTextInputResolvedValue(const std::string& value)
    {
        if (textValue_ == nullptr || ReadString(textValue_) == value)
        {
            return;
        }

        updatingResolvedText_ = true;
        textValue_->SetValue(value.c_str());
        updatingResolvedText_ = false;
    }

    void UpdateResolvedTextFromRule()
    {
        if (textValue_ == nullptr)
        {
            return;
        }

        const std::string rule = Trim(ruleText_);
        if (rule.empty())
        {
            SetTextInputResolvedValue("");
            return;
        }

        NXObject* body = lastBody_;
        Part* part = lastWorkPart_;
        if (Face* selectedFace = SelectedFace())
        {
            Face* face = PrototypeFace(selectedFace);
            if (face != nullptr)
            {
                body = ResolveBodyFromFace(face);
                part = dynamic_cast<Part*>(face->OwningPart());
            }
        }
        if (part == nullptr)
        {
            part = session_ != nullptr && session_->Parts() != nullptr ? session_->Parts()->Work() : nullptr;
        }

        const std::string currentText = ReadString(textValue_);
        const std::string resolved = ExpandTextTemplate(rule, currentText, body, part, config_);
        SetTextInputResolvedValue(resolved);
    }

    bool IncrementDisplaySerialByRule(const std::string& displayText, std::string* nextText)
    {
        if (nextText == nullptr || !TextTemplateHasSerial(lastTextTemplate_))
        {
            return false;
        }

        const std::string trimmedDisplay = Trim(displayText);
        if (IncrementTextSerial(trimmedDisplay, nextText))
        {
            return true;
        }

        Part* part = lastWorkPart_;
        NXObject* partObject = dynamic_cast<NXObject*>(part);
        const std::string bodyName = lastBody_ == nullptr ? std::string() : ToString(lastBody_->Name());
        const std::string fileName = PartLeafName(part);
        const std::vector<TemplateToken> tokens = BuildTemplateTokens(lastTextTemplate_, lastBody_, partObject, bodyName, fileName, config_);

        std::string textPart;
        std::string serialPart;
        ResolveEditableValuesFromDisplay(tokens, displayText, config_, &textPart, &serialPart);

        std::string nextSerial;
        if (!IncrementTextSerial(serialPart, &nextSerial))
        {
            return false;
        }

        *nextText = ComposeTemplateDisplayText(tokens, textPart, nextSerial);
        return true;
    }

    void RotateOrientationNinetyDegrees()
    {
        if (orientation_ == nullptr)
        {
            return;
        }

        const Vector3d xAxis = orientation_->XAxis();
        const Vector3d yAxis = orientation_->YAxis();
        orientation_->SetXAxis(yAxis);
        orientation_->SetYAxis(Vector3d(-xAxis.X, -xAxis.Y, -xAxis.Z));
    }

    void ApplyExclusiveAxisToggle(UIBlock* block)
    {
        if (block == xLongSide_ && ReadToggle(xLongSide_, false) && xShortSide_ != nullptr)
        {
            xShortSide_->SetValue(false);
        }
        else if (block == xShortSide_ && ReadToggle(xShortSide_, false) && xLongSide_ != nullptr)
        {
            xLongSide_->SetValue(false);
        }
    }

    void UpdateUiState()
    {
        const auto ensurePositive = [](DoubleBlock* block, const double fallback) {
            if (block != nullptr && ReadDouble(block, fallback) <= 0.0)
            {
                block->SetValue(fallback);
            }
        };

        if (textHeight_ != nullptr && depth_ != nullptr)
        {
            const double height = ReadDouble(textHeight_, 10.0);
            const double depth = ReadDouble(depth_, 0.3);
            if (height > 0.0 && height < 1.0 && depth > 1.0)
            {
                textHeight_->SetValue(depth);
                depth_->SetValue(height);
            }
        }
        ensurePositive(textHeight_, 10.0);
        ensurePositive(depth_, 0.3);
        ensurePositive(vShapeWidth_, 0.002);
        ensurePositive(boundaryDepth_, 0.3);
        ensurePositive(margin_, 100.0);
        ensurePositive(wScale_, 100.0);

        const bool vShape = ReadToggle(verticalText_, false);
        if (vShapeWidth_ != nullptr)
        {
            vShapeWidth_->SetShow(vShape);
            vShapeWidth_->SetEnable(vShape);
        }

        const int boundaryValue = ReadEnumValue(boundary_);
        if (boundaryDepth_ != nullptr)
        {
            boundaryDepth_->SetEnable(boundaryValue != 0);
        }
        if (boundaryColor_ != nullptr)
        {
            boundaryColor_->SetEnable(boundaryValue != 0);
        }
        if (renameComponentToText_ != nullptr)
        {
            renameComponentToText_->SetShow(isAssemblyContext_);
            renameComponentToText_->SetEnable(isAssemblyContext_);
            if (!isAssemblyContext_)
            {
                renameComponentToText_->SetValue(false);
            }
        }
        if (engraveSameBodies_ != nullptr)
        {
            const bool showForSinglePart = !isAssemblyContext_;
            engraveSameBodies_->SetShow(showForSinglePart);
            engraveSameBodies_->SetEnable(showForSinglePart);
            if (!showForSinglePart)
            {
                engraveSameBodies_->SetValue(false);
            }
        }
        if (hideEngravedText_ != nullptr)
        {
            hideEngravedText_->SetShow(true);
            hideEngravedText_->SetEnable(true);
        }
        if (sameRandomColor_ != nullptr)
        {
            const bool showForSinglePart = !isAssemblyContext_;
            sameRandomColor_->SetShow(showForSinglePart);
            sameRandomColor_->SetEnable(showForSinglePart);
            if (!showForSinglePart)
            {
                sameRandomColor_->SetValue(false);
            }
        }
        const bool autoEngraveVisibleTubes = ReadToggle(autoEngraveVisibleTubes_, false);
        if (!executingDeferredEditedReplacement_ && manualFace_ != nullptr)
        {
            manualFace_->SetShow(!autoEngraveVisibleTubes);
            manualFace_->SetEnable(!autoEngraveVisibleTubes);
        }
        const bool lockAspect = ReadToggle(lockAspect_, true);
        if (wScale_ != nullptr)
        {
            wScale_->SetEnable(!lockAspect);
        }
    }

    TextSettings ReadSettings() const
    {
        TextSettings settings;
        settings.mode = ReadEnumValue(mode_);
        settings.boundary = ReadEnumValue(boundary_);
        settings.text = ReadString(textValue_);
        settings.fontName = ReadString(fontName_);
        settings.height = ReadDouble(textHeight_, 10.0);
        settings.depth = ReadDouble(depth_, 0.3);
        if (settings.height > 0.0 && settings.height < 1.0 && settings.depth > 1.0)
        {
            std::swap(settings.height, settings.depth);
        }
        settings.boundaryDepth = ReadDouble(boundaryDepth_, 0.3);
        settings.textLength = ReadDouble(margin_, 0.0);
        settings.widthScale = ReadDouble(wScale_, 100.0);
        if (settings.height <= 0.0)
        {
            settings.height = 10.0;
        }
        if (settings.depth <= 0.0)
        {
            settings.depth = 0.3;
        }
        if (settings.boundaryDepth <= 0.0)
        {
            settings.boundaryDepth = 0.3;
        }
        if (settings.textLength <= 0.0)
        {
            settings.textLength = 100.0;
        }
        if (settings.widthScale <= 0.0)
        {
            settings.widthScale = 100.0;
        }
        settings.lockAspect = ReadToggle(lockAspect_, true);
        settings.shear = ReadDouble(shear_, 0.0);
        settings.layer = ReadInteger(textLayer_, 254);
        settings.embossed = ReadToggle(embossedText_, false);
        settings.vShape = ReadToggle(verticalText_, false);
        settings.vShapeWidth = ReadDouble(vShapeWidth_, 1.0);
        if (settings.vShapeWidth <= 0.0)
        {
            settings.vShapeWidth = 1.0;
        }
        settings.renameComponentToText = isAssemblyContext_ && ReadToggle(renameComponentToText_, false);
        settings.engraveSameBodies = !isAssemblyContext_ && ReadToggle(engraveSameBodies_, false);
        settings.sameRandomColor = !isAssemblyContext_ && ReadToggle(sameRandomColor_, false);
        settings.autoEngraveVisibleTubes = ReadToggle(autoEngraveVisibleTubes_, false);
        settings.hideEngravedText = ReadToggle(hideEngravedText_, false);
        settings.centerLongSide = ReadToggle(centerLongSide_, false);
        settings.centerShortSide = ReadToggle(centerShortSide_, false);
        settings.xLongSide = ReadToggle(xLongSide_, false);
        settings.xShortSide = ReadToggle(xShortSide_, false);
        if (settings.fontName.empty())
        {
            settings.fontName = "Arial";
        }
        return settings;
    }

    void ApplyVShapeFontSelection()
    {
        if (fontName_ == nullptr)
        {
            return;
        }

        if (ReadToggle(verticalText_, false))
        {
            const std::string currentFont = Trim(ReadString(fontName_));
            if (!currentFont.empty() && currentFont != kVShapeFontName && currentFont != "More...")
            {
                normalFontName_ = currentFont;
            }
            fontName_->SetValue(kVShapeFontName);
            lastValidFontName_ = kVShapeFontName;
            Log(session_, "V形文本已自动切换字体: Modern");
            return;
        }

        if (normalFontName_.empty())
        {
            normalFontName_ = "Arial";
        }
        fontName_->SetValue(normalFontName_.c_str());
        lastValidFontName_ = normalFontName_;
        Log(session_, std::string("取消V形文本，恢复字体: ") + normalFontName_);
    }

    std::string EffectiveTextTemplate(const TextSettings& settings) const
    {
        const std::string dialogRule = Trim(ruleText_);
        if (!dialogRule.empty())
        {
            return ruleText_;
        }

        const std::string configuredRule = Trim(config_.textTemplate);
        if (!configuredRule.empty())
        {
            return config_.textTemplate;
        }

        return settings.text.empty() ? std::string("{文本}") : settings.text;
    }

    Face* SelectedFace() const
    {
        if (manualFace_ == nullptr)
        {
            return nullptr;
        }

        const std::vector<TaggedObject*> selected = manualFace_->GetSelectedObjects();
        for (TaggedObject* object : selected)
        {
            if (Face* face = dynamic_cast<Face*>(object))
            {
                return face;
            }
        }
        return nullptr;
    }

    void ChooseAndAppendAttributeToken()
    {
        Face* selectedFace = SelectedFace();
        if (selectedFace == nullptr)
        {
            ShowError("刻字", "请先选择一个面，再选择属性变量。");
            return;
        }

        const int sourceIndex = AskMenuIndex("选择属性来源", std::vector<std::string>{"体属性", "部件属性"});
        if (sourceIndex < 0)
        {
            return;
        }

        Face* prototypeFace = PrototypeFace(selectedFace);
        NXObject* target = nullptr;
        std::string tokenPrefix;
        std::string sourceName;
        if (sourceIndex == 0)
        {
            target = ResolveBodyFromFace(prototypeFace);
            tokenPrefix = "{体属性:";
            sourceName = "体属性";
        }
        else
        {
            Part* part = nullptr;
            if (prototypeFace != nullptr)
            {
                part = dynamic_cast<Part*>(prototypeFace->OwningPart());
            }
            target = dynamic_cast<NXObject*>(part);
            tokenPrefix = "{部件属性:";
            sourceName = "部件属性";
        }

        std::vector<std::string> names = UserAttributeNames(target);
        if (names.empty())
        {
            ShowError("刻字", sourceName + "列表为空。");
            return;
        }

        const std::string attributeName = AskMenuItemPaged("选择" + sourceName, names);
        if (attributeName.empty())
        {
            return;
        }

        Log(session_, std::string("追加变量: ") + sourceName + ":" + attributeName);
        AppendTextToken(tokenPrefix + attributeName + "}");
        RefreshPreview();
    }

    Matrix3x3 TextMatrixFromDialog(Face* face,
                                   const Vector3d& faceNormal,
                                   const TextSettings& settings,
                                   Assemblies::Component* component) const
    {
        Vector3d xAxis(1.0, 0.0, 0.0);
        Vector3d yAxis(0.0, 1.0, 0.0);
        if (orientation_ != nullptr)
        {
            xAxis = orientation_->XAxis();
            yAxis = orientation_->YAxis();
        }
        xAxis = ComponentVectorToPrototype(component, xAxis);
        yAxis = ComponentVectorToPrototype(component, yAxis);

        Vector3d z = Normalize(faceNormal, Vector3d(0.0, 0.0, 1.0));
        Vector3d x = Normalize(ProjectToPlane(xAxis, z), Vector3d(1.0, 0.0, 0.0));
        if (settings.xLongSide || settings.xShortSide)
        {
            Vector3d longAxis;
            Vector3d shortAxis;
            if (FaceLongShortAxes(face, z, &longAxis, &shortAxis))
            {
                x = settings.xShortSide ? shortAxis : longAxis;
                if (Dot(ProjectToPlane(xAxis, z), x) < 0.0)
                {
                    x = Vector3d(-x.X, -x.Y, -x.Z);
                }
            }
        }
        Vector3d y = Normalize(Cross(z, x), Normalize(yAxis, Vector3d(0.0, 1.0, 0.0)));
        x = Normalize(Cross(y, z), x);
        return MakeMatrix(x, y, z);
    }

    Point3d TextOriginFromPickPoint(const Point3d& facePoint, const Vector3d& faceNormal) const
    {
        if (orientation_ != nullptr && orientation_->IsOriginSpecified())
        {
            Point3d origin = orientation_->Origin();
            if (std::isfinite(origin.X) && std::isfinite(origin.Y) && std::isfinite(origin.Z))
            {
                const Vector3d normal = Normalize(faceNormal, Vector3d(0.0, 0.0, 1.0));
                const Vector3d offset(
                    origin.X - facePoint.X,
                    origin.Y - facePoint.Y,
                    origin.Z - facePoint.Z);
                const double distance = Dot(offset, normal);
                return Point3d(
                    origin.X - distance * normal.X,
                    origin.Y - distance * normal.Y,
                    origin.Z - distance * normal.Z);
            }
        }

        if (manualFace_ == nullptr)
        {
            return facePoint;
        }

        Point3d pickPoint = manualFace_->PickPoint();
        if (!std::isfinite(pickPoint.X) || !std::isfinite(pickPoint.Y) || !std::isfinite(pickPoint.Z))
        {
            return facePoint;
        }

        const Vector3d normal = Normalize(faceNormal, Vector3d(0.0, 0.0, 1.0));
        const Vector3d offset(
            pickPoint.X - facePoint.X,
            pickPoint.Y - facePoint.Y,
            pickPoint.Z - facePoint.Z);
        const double distance = Dot(offset, normal);
        return Point3d(
            pickPoint.X - distance * normal.X,
            pickPoint.Y - distance * normal.Y,
            pickPoint.Z - distance * normal.Z);
    }

    Point3d ApplyCenterOptions(Face* face, const Vector3d& faceNormal, const Point3d& origin, const TextSettings& settings) const
    {
        if (face == nullptr || (!settings.centerLongSide && !settings.centerShortSide))
        {
            return origin;
        }

        const std::vector<Point3d> vertices = FaceVertices(face);
        if (vertices.empty())
        {
            return origin;
        }

        const Vector3d z = Normalize(faceNormal, Vector3d(0.0, 0.0, 1.0));
        Vector3d longAxis;
        Vector3d shortAxis;
        if (!FaceLongShortAxes(face, z, &longAxis, &shortAxis))
        {
            return origin;
        }

        Point3d result = origin;
        if (settings.centerLongSide)
        {
            double center = 0.0;
            ProjectionSpan(vertices, longAxis, &center);
            const double current = result.X * longAxis.X + result.Y * longAxis.Y + result.Z * longAxis.Z;
            result = OffsetPoint(result, longAxis, center - current);
        }
        if (settings.centerShortSide)
        {
            double center = 0.0;
            ProjectionSpan(vertices, shortAxis, &center);
            const double current = result.X * shortAxis.X + result.Y * shortAxis.Y + result.Z * shortAxis.Z;
            result = OffsetPoint(result, shortAxis, center - current);
        }
        return result;
    }

    void ClearPreviewBuilder()
    {
        Log(session_, "ClearPreviewBuilder进入");
        Tooling::InsertTextBuilder* builder = previewBuilder_;
        previewBuilder_ = nullptr;
        previewUdo_ = nullptr;
        if (builder != nullptr)
        {
            try
            {
                Log(session_, "ClearPreviewBuilder: Destroy预览Builder开始");
                builder->Destroy();
                Log(session_, "ClearPreviewBuilder: Destroy预览Builder完成");
            }
            catch (const NXException& ex)
            {
                Log(session_, std::string("ClearPreviewBuilder: Destroy预览Builder NX异常: ") + ex.Message());
            }
            catch (...)
            {
                Log(session_, "ClearPreviewBuilder: Destroy预览Builder未知异常");
            }
        }
        for (tag_t curveTag : modernPreviewCurveTags_)
        {
            if (curveTag != NULL_TAG)
            {
                try { UF_OBJ_delete_object(curveTag); } catch (...) {}
            }
        }
        if (!modernPreviewCurveTags_.empty())
        {
            Log(session_, std::string("ClearPreviewBuilder: 已删除MODERN曲线预览，数量=") +
                              std::to_string(modernPreviewCurveTags_.size()));
            modernPreviewCurveTags_.clear();
        }
        Log(session_, "ClearPreviewBuilder退出");
    }

    void ClearManualSelectionAfterReplace()
    {
        if (manualFace_ == nullptr)
        {
            return;
        }
        try
        {
            std::vector<TaggedObject*> emptySelection;
            manualFace_->SetSelectedObjects(emptySelection);
            Log(session_, "已清空选择面，避免Apply后恢复旧组件选择");
        }
        catch (const NXException& ex)
        {
            Log(session_, std::string("清空选择面失败: ") + ex.Message());
        }
        catch (...)
        {
            Log(session_, "清空选择面失败: 未知异常");
        }
    }

    void MoveOrientationToSelectedFace()
    {
        if (orientation_ == nullptr)
        {
            return;
        }

        Face* face = SelectedFace();
        if (face == nullptr)
        {
            return;
        }

        Point3d pickedPoint = manualFace_ != nullptr ? manualFace_->PickPoint() : Point3d(0.0, 0.0, 0.0);
        Point3d facePoint;
        Vector3d faceNormal;
        if (!AskPlanarFaceData(face, &pickedPoint, &facePoint, &faceNormal))
        {
            return;
        }

        orientation_->SetOriginSpecified(true);
        orientation_->SetOrigin(facePoint);
        orientation_->SetVisibleManipulatorHandles(0x47);
        ApplyTextAxisModeToOrientation(face, faceNormal, ReadSettings());
        SnapOrientationToCenterOptions();
        Log(session_, "选面后切换到指定方位");
    }

    void ApplyTextAxisModeToOrientation(Face* face, const Vector3d& faceNormal, const TextSettings& settings)
    {
        if (orientation_ == nullptr || face == nullptr)
        {
            return;
        }

        if (!settings.xLongSide && !settings.xShortSide)
        {
            return;
        }

        const Vector3d z = Normalize(faceNormal, Vector3d(0.0, 0.0, 1.0));
        Vector3d longAxis;
        Vector3d shortAxis;
        if (!FaceLongShortAxes(face, z, &longAxis, &shortAxis))
        {
            return;
        }

        Vector3d desiredX = settings.xShortSide ? shortAxis : longAxis;
        const Vector3d currentX = Normalize(ProjectToPlane(orientation_->XAxis(), z), desiredX);
        if (Dot(currentX, desiredX) < 0.0)
        {
            desiredX = Vector3d(-desiredX.X, -desiredX.Y, -desiredX.Z);
        }

        const Vector3d desiredY = Normalize(Cross(z, desiredX), shortAxis);
        orientation_->SetXAxis(desiredX);
        orientation_->SetYAxis(desiredY);
    }

    void ApplyAxisModeToCurrentOrientation()
    {
        if (orientation_ == nullptr)
        {
            return;
        }

        const TextSettings settings = ReadSettings();
        if (!settings.xLongSide && !settings.xShortSide)
        {
            return;
        }

        Face* face = SelectedFace();
        if (face == nullptr)
        {
            return;
        }

        Point3d origin = orientation_->IsOriginSpecified() ? orientation_->Origin() : Point3d(0.0, 0.0, 0.0);
        Point3d facePoint;
        Vector3d faceNormal;
        if (!AskPlanarFaceData(face, &origin, &facePoint, &faceNormal))
        {
            return;
        }

        orientation_->SetVisibleManipulatorHandles(0x47);
        ApplyTextAxisModeToOrientation(face, faceNormal, settings);
        Log(session_, "按 X长/X短调整文字方向");
    }

    void SnapOrientationToCenterOptions()
    {
        if (orientation_ == nullptr)
        {
            return;
        }

        const TextSettings settings = ReadSettings();
        if (!settings.centerLongSide && !settings.centerShortSide)
        {
            return;
        }

        Face* face = SelectedFace();
        if (face == nullptr)
        {
            return;
        }

        Point3d pickedPoint = orientation_->IsOriginSpecified() ? orientation_->Origin() : Point3d(0.0, 0.0, 0.0);
        Point3d facePoint;
        Vector3d faceNormal;
        if (!AskPlanarFaceData(face, &pickedPoint, &facePoint, &faceNormal))
        {
            return;
        }

        const std::vector<Point3d> vertices = FaceVertices(face);
        if (vertices.empty())
        {
            return;
        }

        const Vector3d z = Normalize(faceNormal, Vector3d(0.0, 0.0, 1.0));
        Vector3d longAxis;
        Vector3d shortAxis;
        if (!FaceLongShortAxes(face, z, &longAxis, &shortAxis))
        {
            return;
        }

        Point3d result = facePoint;
        if (settings.centerLongSide)
        {
            double center = 0.0;
            ProjectionSpan(vertices, longAxis, &center);
            const double current = result.X * longAxis.X + result.Y * longAxis.Y + result.Z * longAxis.Z;
            result = OffsetPoint(result, longAxis, center - current);
        }
        if (settings.centerShortSide)
        {
            double center = 0.0;
            ProjectionSpan(vertices, shortAxis, &center);
            const double current = result.X * shortAxis.X + result.Y * shortAxis.Y + result.Z * shortAxis.Z;
            result = OffsetPoint(result, shortAxis, center - current);
        }

        orientation_->SetOriginSpecified(true);
        orientation_->SetOrigin(result);
        Log(session_, "按居中开关调整方位原点");
    }

    std::string ResolveEngravingText(NXObject* body,
                                     Part* workPart,
                                     const TextSettings& settings,
                                     const std::string* overrideText)
    {
        const std::string textTemplate = EffectiveTextTemplate(settings);
        const std::string textRule = overrideText != nullptr
                                         ? *overrideText
                                         : ExpandTextTemplate(textTemplate, settings.text, body, workPart, config_);
        if (textRule.empty())
        {
            throw std::runtime_error("Engraving text is empty.");
        }
        lastTextTemplate_ = textTemplate;
        lastResolvedText_ = textRule;
        lastBody_ = body;
        lastWorkPart_ = workPart;
        return textRule;
    }

    Vector3d InwardExtrudeDirection(Body* targetBody,
                                    const Point3d& facePoint,
                                    const Vector3d& faceNormal,
                                    const double depth) const
    {
        const Vector3d normal = Normalize(faceNormal, Vector3d(0.0, 0.0, 1.0));
        const double probe = std::max(0.01, std::min(std::max(depth, 0.1) * 0.25, 1.0));
        const auto contained = [targetBody](const Point3d& point) {
            if (targetBody == nullptr)
            {
                return false;
            }
            double coordinates[3] = {point.X, point.Y, point.Z};
            int status = 0;
            return UF_MODL_ask_point_containment(coordinates, targetBody->Tag(), &status) == 0 &&
                   (status == 1 || status == 3);
        };
        if (contained(OffsetPoint(facePoint, normal, probe)))
        {
            return normal;
        }
        return Vector3d(-normal.X, -normal.Y, -normal.Z);
    }

    void CreateModernCurveEngraving(Face* face,
                                    const Point3d& textOrigin,
                                    const Matrix3x3& textMatrix,
                                    const std::string& text,
                                    const TextSettings& settings,
                                    Part* workPart,
                                    Body* targetBody,
                                    const bool commitGeometry)
    {
        if (face == nullptr || workPart == nullptr || targetBody == nullptr)
        {
            throw std::runtime_error("Modern curve engraving requires a face, work part, and target body.");
        }
        if (commitGeometry)
        {
            modernConstructionFeatureTags_.clear();
        }
        Log(session_, std::string("V形新逻辑: 入口 text=") + text +
                          ", height=" + FormatNumber(settings.height) +
                          ", depth=" + FormatNumber(settings.depth) +
                          ", width=" + FormatNumber(settings.vShapeWidth) +
                          ", faceTag=" + std::to_string(face->Tag()) +
                          ", bodyTag=" + std::to_string(targetBody->Tag()));

        Log(session_, "V形新逻辑: 开始加载NX单线字体 blockmod1");
        const int modernFontIndex = workPart->Fonts()->AddFont(
            kVShapeNxCurveFontName, FontCollection::TypeNx);
        if (modernFontIndex <= 0 || !workPart->Fonts()->DoesFontExist(modernFontIndex))
        {
            throw std::runtime_error("Could not load the NX Modern single-line font (blockmod1.fnx).");
        }
        Log(session_, std::string("V形新逻辑: NX单线字体已加载，索引=") + std::to_string(modernFontIndex));

        UF_DRF_draft_aid_text_info_t textInfo = {};
        textInfo.text_type = UF_DRF_TEXT_APP_AT_CREATION;
        textInfo.text_font = modernFontIndex;
        textInfo.size = 1.0;
        textInfo.height = 1.0;
        textInfo.aspect_ratio = 1.0;
        textInfo.gap = 1.0;
        textInfo.line_spacing = 1.0;
        textInfo.num_lines = 1;
        std::vector<char> renderedText(text.begin(), text.end());
        renderedText.push_back('\0');
        char* textLines[1] = {renderedText.data()};
        UF_DRF_draft_aid_text_t draftText = {};
        const size_t textByteCount = renderedText.size() - 1;
        draftText.num_chars = static_cast<int>(textByteCount);
        draftText.num_ints = static_cast<int>(textByteCount);
        draftText.full_num_chars = static_cast<int>(textByteCount);
        draftText.full_string = renderedText.data();
        strncpy_s(draftText.string, renderedText.data(), _TRUNCATE);
        textInfo.text = &draftText;
        NxFontRenderData renderData;
        UF_DRF_render_table_t renderTable = {};
        renderTable.begin_line = FontBeginLine;
        renderTable.end_line = FontEndLine;
        renderTable.set_to_position = FontSetPosition;
        renderTable.draw_to_position = FontDrawPosition;
        renderTable.draw_arc = FontDrawArc;
        renderTable.draw_char = FontDrawChar;
        renderTable.draw_standard_font_string = FontDrawStandard;
        renderTable.draw_user_symbol = FontDrawSymbol;
        renderTable.set_cfw = FontSetCfw;
        renderTable.push_orientation = FontPushOrientation;
        renderTable.pop_orientation = FontPopOrientation;
        renderTable.fill_region = FontFillRegion;
        renderTable.standardFontFunCharSize = 1.0;

        // NX 2412 rejects NULL_TAG for UF_DRF_render_text's annotation argument
        // even though the SDK header documents it as optional.  A temporary note
        // supplies only the required annotation context; all font/stroke geometry
        // still comes from blockmod1 and the render callbacks below.
        tag_t renderContextNote = NULL_TAG;
        double contextOrigin[3] = {0.0, 0.0, 0.0};
        const int noteError = UF_DRF_create_note(1, textLines, contextOrigin, 0, &renderContextNote);
        if (noteError != 0 || renderContextNote == NULL_TAG)
        {
            char errorMessage[133] = {};
            UF_get_fail_message(noteError, errorMessage);
            Log(session_, std::string("V形新逻辑: 创建临时渲染注释失败，错误=") +
                              std::to_string(noteError) + ", message=" + errorMessage);
            throw std::runtime_error("Could not create the temporary NX text rendering context.");
        }
        Log(session_, std::string("V形新逻辑: 临时渲染注释已创建，tag=") +
                          std::to_string(renderContextNote));
        Log(session_, "V形新逻辑: 开始调用 UF_DRF_render_text");
        const int renderError = UF_DRF_render_text(
            workPart->Tag(), renderContextNote, 1, textLines, &textInfo, &renderTable, &renderData);
        char renderMessage[133] = {};
        UF_get_fail_message(renderError, renderMessage);
        Log(session_, std::string("V形新逻辑: UF_DRF_render_text 返回=") +
                          std::to_string(renderError) + ", message=" + renderMessage);
        const int deleteNoteError = UF_OBJ_delete_object(renderContextNote);
        if (deleteNoteError != 0)
        {
            char deleteMessage[133] = {};
            UF_get_fail_message(deleteNoteError, deleteMessage);
            Log(session_, std::string("V形新逻辑: 删除临时渲染注释失败，错误=") +
                              std::to_string(deleteNoteError) + ", message=" + deleteMessage);
        }
        else
        {
            Log(session_, "V形新逻辑: 临时渲染注释已删除");
        }
        if (renderError != 0 || (renderData.segments.empty() && renderData.arcs.empty()))
        {
            throw std::runtime_error("NX Modern font rendering returned no stroke curves.");
        }
        Log(session_, std::string("V形新逻辑: 笔画渲染完成，线段=") +
                          std::to_string(renderData.segments.size()) +
                          ", 圆弧=" + std::to_string(renderData.arcs.size()));

        double minX = DBL_MAX, maxX = -DBL_MAX, minY = DBL_MAX, maxY = -DBL_MAX;
        const auto includePoint = [&](const Point3d& p) {
            minX = std::min(minX, p.X); maxX = std::max(maxX, p.X);
            minY = std::min(minY, p.Y); maxY = std::max(maxY, p.Y);
        };
        for (const RenderedFontSegment& segment : renderData.segments)
        {
            includePoint(segment.start); includePoint(segment.end);
        }
        for (const RenderedFontArc& arc : renderData.arcs)
        {
            includePoint(Point3d(arc.center.X - arc.radius, arc.center.Y - arc.radius, 0.0));
            includePoint(Point3d(arc.center.X + arc.radius, arc.center.Y + arc.radius, 0.0));
        }
        const double centerX = (minX + maxX) * 0.5;
        const double centerY = (minY + maxY) * 0.5;
        const double xScale = settings.height * settings.widthScale / 100.0;
        const double yScale = settings.height;
        const double shear = std::tan(settings.shear * 3.14159265358979323846 / 180.0);
        const Vector3d xAxis(textMatrix.Xx, textMatrix.Xy, textMatrix.Xz);
        const Vector3d yAxis(textMatrix.Yx, textMatrix.Yy, textMatrix.Yz);
        const auto toWorld = [&](const Point3d& local) {
            const double localY = (local.Y - centerY) * yScale;
            const double localX = (local.X - centerX) * xScale + localY * shear;
            return OffsetPoint(OffsetPoint(textOrigin, xAxis, localX), yAxis, localY);
        };
        std::vector<Curve*> curves;
        for (const RenderedFontSegment& segment : renderData.segments)
        {
            const Point3d start = toWorld(segment.start);
            const Point3d end = toWorld(segment.end);
            if (Magnitude(SubtractPoints(end, start)) > kVectorTolerance)
            {
                curves.push_back(workPart->Curves()->CreateLine(start, end));
            }
        }
        for (const RenderedFontArc& arc : renderData.arcs)
        {
            const Point3d center = toWorld(arc.center);
            curves.push_back(workPart->Curves()->CreateArc(
                center, xAxis, yAxis, arc.radius * xScale, arc.startAngle, arc.endAngle));
        }
        if (curves.empty())
        {
            throw std::runtime_error("Modern text feature returned no single-line curves.");
        }
        Log(session_, std::string("V形新逻辑: NX曲线已创建，数量=") + std::to_string(curves.size()));

        if (!commitGeometry)
        {
            modernPreviewCurveTags_.clear();
            modernPreviewCurveTags_.reserve(curves.size());
            for (Curve* curve : curves)
            {
                if (curve != nullptr)
                {
                    modernPreviewCurveTags_.push_back(curve->Tag());
                }
            }
            Log(session_, std::string("V形新逻辑: 仅显示MODERN单线曲线预览，数量=") +
                              std::to_string(modernPreviewCurveTags_.size()));
            return;
        }

        Point3d facePoint;
        Vector3d faceNormal;
        if (!AskPlanarFaceData(face, &textOrigin, &facePoint, &faceNormal))
        {
            throw std::runtime_error("Could not determine engraving face normal.");
        }
        const Vector3d inward = InwardExtrudeDirection(targetBody, facePoint, faceNormal, settings.depth);
        const std::string depthFormula = FormatNumber(settings.depth);
        const std::string halfWidthFormula = FormatNumber(settings.vShapeWidth * 0.5);

        int successCount = 0;
        int curveIndex = 0;
        std::vector<Body*> cutterBodies;
        const Session::UndoMarkId modernEngravingMark = session_->SetUndoMark(
            Session::MarkVisibilityInvisible, "KeZi MODERN engraving");
        try
        {
            Log(session_, std::string("V形新逻辑: 开始由原始曲线生成对称偏置刀具体，曲线数=") +
                              std::to_string(curves.size()));
            for (Curve* curve : curves)
            {
                ++curveIndex;

                Features::ExtrudeBuilder* extrudeBuilder = workPart->Features()->CreateExtrudeBuilder(nullptr);
                if (extrudeBuilder == nullptr)
                {
                    throw std::runtime_error("CreateExtrudeBuilder returned NULL.");
                }
                Section* section = workPart->Sections()->CreateSection(1.0e-5, 1.0e-5, 0.5);
                if (section == nullptr)
                {
                    throw std::runtime_error("CreateSection returned NULL.");
                }
                extrudeBuilder->SetSection(section);
                extrudeBuilder->SetDistanceTolerance(1.0e-5);
                extrudeBuilder->BooleanOperation()->SetType(
                    GeometricUtilities::BooleanOperation::BooleanTypeCreate);
                extrudeBuilder->Limits()->SetSymmetricOption(false);
                extrudeBuilder->Limits()->StartExtend()->Value()->SetFormula("0");
                extrudeBuilder->Limits()->EndExtend()->Value()->SetFormula(depthFormula.c_str());
                extrudeBuilder->Limits()->StartExtend()->SetTrimType(GeometricUtilities::Extend::ExtendTypeValue);
                extrudeBuilder->Limits()->EndExtend()->SetTrimType(GeometricUtilities::Extend::ExtendTypeValue);
                extrudeBuilder->Offset()->SetOption(GeometricUtilities::TypeSymmetricOffset);
                extrudeBuilder->Offset()->StartOffset()->SetFormula(halfWidthFormula.c_str());
                extrudeBuilder->Offset()->EndOffset()->SetFormula(halfWidthFormula.c_str());
                extrudeBuilder->Draft()->SetDraftOption(GeometricUtilities::SimpleDraft::SimpleDraftTypeNoDraft);
                extrudeBuilder->FeatureOptions()->SetBodyType(GeometricUtilities::FeatureOptions::BodyStyleSolid);
                extrudeBuilder->SmartVolumeProfile()->SetOpenProfileSmartVolumeOption(false);
                extrudeBuilder->SmartVolumeProfile()->SetCloseProfileRule(
                    GeometricUtilities::SmartVolumeProfileBuilder::CloseProfileRuleTypeFci);

                section->SetDistanceTolerance(1.0e-5);
                section->SetChainingTolerance(1.0e-5);
                section->SetAllowedEntityTypes(Section::AllowTypesOnlyCurves);
                section->AllowSelfIntersection(true);
                section->AllowDegenerateCurves(false);

                SelectionIntentRuleOptions* ruleOptions = workPart->ScRuleFactory()->CreateRuleOptions();
                ruleOptions->SetSelectedFromInactive(false);
                std::vector<IBaseCurve*> seedCurves(1, curve);
                CurveDumbRule* curveRule = workPart->ScRuleFactory()->CreateRuleBaseCurveDumb(seedCurves, ruleOptions);
                delete ruleOptions;
                if (curveRule == nullptr)
                {
                    extrudeBuilder->Destroy();
                    throw std::runtime_error("Could not create a rule for a Modern font curve.");
                }
                std::vector<SelectionIntentRule*> rules(1, curveRule);
                section->AddToSection(rules, curve, nullptr, nullptr, textOrigin, Section::ModeCreate, false);
                Direction* direction = workPart->Directions()->CreateDirection(
                    textOrigin, inward, SmartObject::UpdateOptionWithinModeling);
                if (direction == nullptr)
                {
                    extrudeBuilder->Destroy();
                    throw std::runtime_error("CreateDirection returned NULL.");
                }
                extrudeBuilder->SetDirection(direction);
                Features::Feature* cutterFeature = extrudeBuilder->CommitFeature();
                extrudeBuilder->Destroy();
                if (cutterFeature == nullptr || cutterFeature->GetBodies().empty())
                {
                    throw std::runtime_error("Symmetric curve extrusion did not create a cutter body.");
                }
                modernConstructionFeatureTags_.push_back(cutterFeature->Tag());
                for (Body* cutterBody : cutterFeature->GetBodies())
                {
                    if (cutterBody != nullptr)
                    {
                        cutterBodies.push_back(cutterBody);
                    }
                }
                if (curveIndex % 25 == 0 || curveIndex == static_cast<int>(curves.size()))
                {
                    Log(session_, std::string("V形新逻辑: 临时刀具体进度 ") +
                                      std::to_string(curveIndex) + "/" +
                                      std::to_string(curves.size()));
                }
            }

            if (cutterBodies.empty())
            {
                throw std::runtime_error("No symmetric Modern curve cutter bodies were created.");
            }

            Log(session_, std::string("V形新逻辑: 开始一次性批量求差，刀具体数量=") +
                              std::to_string(cutterBodies.size()));
            Features::BooleanBuilder* subtract = workPart->Features()->CreateBooleanBuilderUsingCollector(nullptr);
            subtract->SetOperation(Features::Feature::BooleanTypeSubtract);
            subtract->SetRetainTarget(false);
            subtract->SetRetainTool(false);

            SelectionIntentRuleOptions* targetOptions = workPart->ScRuleFactory()->CreateRuleOptions();
            targetOptions->SetSelectedFromInactive(false);
            BodyDumbRule* targetRule = workPart->ScRuleFactory()->CreateRuleBodyDumb(
                std::vector<Body*>(1, targetBody), true, targetOptions);
            delete targetOptions;
            ScCollector* targetCollector = workPart->ScCollectors()->CreateCollector();
            targetCollector->ReplaceRules(std::vector<SelectionIntentRule*>(1, targetRule), false);
            subtract->SetTargetBodyCollector(targetCollector);

            SelectionIntentRuleOptions* toolOptions = workPart->ScRuleFactory()->CreateRuleOptions();
            toolOptions->SetSelectedFromInactive(false);
            BodyDumbRule* toolRule = workPart->ScRuleFactory()->CreateRuleBodyDumb(
                cutterBodies, true, toolOptions);
            delete toolOptions;
            ScCollector* toolCollector = workPart->ScCollectors()->CreateCollector();
            toolCollector->ReplaceRules(std::vector<SelectionIntentRule*>(1, toolRule), false);
            subtract->SetToolBodyCollector(toolCollector);
            Features::Feature* subtractFeature = dynamic_cast<Features::Feature*>(subtract->Commit());
            subtract->Destroy();
            if (subtractFeature == nullptr)
            {
                throw std::runtime_error("Batch subtract returned no engraving feature.");
            }
            modernConstructionFeatureTags_.push_back(subtractFeature->Tag());
            successCount = static_cast<int>(cutterBodies.size());
            Log(session_, "V形新逻辑: 一次性批量求差成功");
        }
        catch (const NXException& ex)
        {
            Log(session_, std::string("V形新逻辑: 对称偏置批量刀具体异常 code=") +
                              std::to_string(ex.ErrorCode()) + ", message=" + ex.Message());
            try { session_->UndoToMark(modernEngravingMark, "KeZi MODERN engraving"); } catch (...) {}
            try { session_->DeleteUndoMark(modernEngravingMark, "KeZi MODERN engraving"); } catch (...) {}
            if (ex.ErrorCode() == 66 || ex.ErrorCode() == UF_err_operation_aborted)
            {
                Log(session_, "V形新逻辑: 收到用户停止信号，已回滚本次刻字");
            }
            throw;
        }
        catch (...)
        {
            try { session_->UndoToMark(modernEngravingMark, "KeZi MODERN engraving"); } catch (...) {}
            try { session_->DeleteUndoMark(modernEngravingMark, "KeZi MODERN engraving"); } catch (...) {}
            throw;
        }
        if (successCount == 0)
        {
            try { session_->DeleteUndoMark(modernEngravingMark, "KeZi MODERN engraving"); } catch (...) {}
            throw std::runtime_error("All Modern single-line curve extrusions failed.");
        }
        for (Curve* curve : curves)
        {
            if (curve != nullptr) { UF_OBJ_set_blank_status(curve->Tag(), UF_OBJ_BLANKED); }
        }
        Log(session_, std::string("Modern原始曲线对称偏置批量刻字完成，刀具体数=") + std::to_string(successCount));

        Features::CustomFeatureClassManager* classManager = session_->CustomFeatureClassManager();
        Features::CustomFeatureClass* featureClass = classManager == nullptr
            ? nullptr
            : classManager->GetClassFromName(zhihui_kezi_custom_feature::kFeatureClassName);
        if (featureClass == nullptr)
        {
            Log(session_, "单线刻字自定义特征类尚未加载，本次保留普通内部特征链");
            try { session_->DeleteUndoMark(modernEngravingMark, "KeZi MODERN engraving"); } catch (...) {}
            return;
        }
        Features::CustomFeatureBuilder* customBuilder = nullptr;
        try
        {
            Features::CustomFeatureData* data = nullptr;
            if (editedFeature_ == nullptr)
            {
                Features::CustomAttributeCollection* attributes = workPart->Features()->CustomAttributeCollection();
                std::vector<Features::CustomAttribute*> values;
                values.push_back(CreateKeZiTagAttribute(attributes,
                    zhihui_kezi_custom_feature::kAttrTargetBody, targetBody, true, true));
                values.push_back(attributes->CreateCustomTagAttribute(
                    zhihui_kezi_custom_feature::kAttrFace,
                    std::vector<Features::CustomAttribute::Property>()));
                values.push_back(CreateKeZiStringAttribute(attributes,
                    zhihui_kezi_custom_feature::kAttrText, text));
                values.push_back(CreateKeZiDoubleAttribute(attributes,
                    zhihui_kezi_custom_feature::kAttrHeight, settings.height));
                values.push_back(CreateKeZiDoubleAttribute(attributes,
                    zhihui_kezi_custom_feature::kAttrDepth, settings.depth));
                values.push_back(CreateKeZiDoubleAttribute(attributes,
                    zhihui_kezi_custom_feature::kAttrWidth, settings.vShapeWidth));
                values.push_back(CreateKeZiDoubleAttribute(attributes,
                    zhihui_kezi_custom_feature::kAttrWidthScale, settings.widthScale));
                values.push_back(CreateKeZiDoubleAttribute(attributes,
                    zhihui_kezi_custom_feature::kAttrShear, settings.shear));
                values.push_back(CreateKeZiDoubleAttribute(attributes,
                    zhihui_kezi_custom_feature::kAttrOriginX, textOrigin.X));
                values.push_back(CreateKeZiDoubleAttribute(attributes,
                    zhihui_kezi_custom_feature::kAttrOriginY, textOrigin.Y));
                values.push_back(CreateKeZiDoubleAttribute(attributes,
                    zhihui_kezi_custom_feature::kAttrOriginZ, textOrigin.Z));
                const double initialMatrixValues[9] = {
                    textMatrix.Xx, textMatrix.Xy, textMatrix.Xz,
                    textMatrix.Yx, textMatrix.Yy, textMatrix.Yz,
                    textMatrix.Zx, textMatrix.Zy, textMatrix.Zz};
                for (int index = 0; index < 9; ++index)
                {
                    const std::string name = std::string(zhihui_kezi_custom_feature::kAttrMatrixPrefix) +
                                             std::to_string(index);
                    values.push_back(CreateKeZiDoubleAttribute(attributes, name.c_str(), initialMatrixValues[index]));
                }
                values.push_back(CreateKeZiIntegerAttribute(attributes,
                    zhihui_kezi_custom_feature::kAttrSchemaVersion, 1));
                data = workPart->Features()->CustomFeatureDataCollection()->CreateData(featureClass, values);
            }
            else
            {
                data = editedFeature_->FeatureData();
            }
            data->CustomTagAttributeByName(zhihui_kezi_custom_feature::kAttrTargetBody)->SetValue(targetBody);
            if (face != nullptr && UF_OBJ_ask_status(face->Tag()) == UF_OBJ_ALIVE)
            {
                data->CustomTagAttributeByName(zhihui_kezi_custom_feature::kAttrFace)->SetValue(face);
            }
            data->CustomStringAttributeByName(zhihui_kezi_custom_feature::kAttrText)
                ->SetValue(NXString(text.c_str(), NXString::UTF8));
            data->CustomDoubleAttributeByName(zhihui_kezi_custom_feature::kAttrHeight)->SetValue(settings.height);
            data->CustomDoubleAttributeByName(zhihui_kezi_custom_feature::kAttrDepth)->SetValue(settings.depth);
            data->CustomDoubleAttributeByName(zhihui_kezi_custom_feature::kAttrWidth)->SetValue(settings.vShapeWidth);
            data->CustomDoubleAttributeByName(zhihui_kezi_custom_feature::kAttrWidthScale)->SetValue(settings.widthScale);
            data->CustomDoubleAttributeByName(zhihui_kezi_custom_feature::kAttrShear)->SetValue(settings.shear);
            data->CustomDoubleAttributeByName(zhihui_kezi_custom_feature::kAttrOriginX)->SetValue(textOrigin.X);
            data->CustomDoubleAttributeByName(zhihui_kezi_custom_feature::kAttrOriginY)->SetValue(textOrigin.Y);
            data->CustomDoubleAttributeByName(zhihui_kezi_custom_feature::kAttrOriginZ)->SetValue(textOrigin.Z);
            const double matrixValues[9] = {
                textMatrix.Xx, textMatrix.Xy, textMatrix.Xz,
                textMatrix.Yx, textMatrix.Yy, textMatrix.Yz,
                textMatrix.Zx, textMatrix.Zy, textMatrix.Zz};
            for (int index = 0; index < 9; ++index)
            {
                const std::string name = std::string(zhihui_kezi_custom_feature::kAttrMatrixPrefix) +
                                         std::to_string(index);
                data->CustomDoubleAttributeByName(name.c_str())->SetValue(matrixValues[index]);
            }
            const bool editingExistingFeature = editedFeature_ != nullptr;
            customBuilder = workPart->Features()->CreateCustomFeatureBuilder(editedFeature_);
            buildingModernCustomFeature_ = true;
            customBuilder->SetFeatureData(data);
            Features::Feature* committed = customBuilder->CommitFeature();

            if (editingExistingFeature)
            {
                // NX can defer CustomFeature PreUpdate when CommitFeature is
                // called under EditWithRollbackManager.  Keep the pending new
                // extrude/subtract tags alive and force that scheduled pass
                // before ending rollback, so PreUpdate can replace the old
                // construction list instead of leaving the new chain loose.
                Log(session_, "编辑单线刻字: 强制执行自定义特征内部链更新");
                const int updateResult = UF_MODL_update();
                if (updateResult != 0)
                {
                    throw NXException::Create(
                        updateResult,
                        "Failed to update the edited single-line engraving CustomFeature.");
                }
                Log(session_, "编辑单线刻字: 自定义特征内部链更新完成");
            }
            buildingModernCustomFeature_ = false;
            customBuilder->Destroy();
            customBuilder = nullptr;
            Features::CustomFeature* customFeature = dynamic_cast<Features::CustomFeature*>(committed);
            if (customFeature == nullptr)
            {
                throw std::runtime_error("NX did not return the single-line engraving CustomFeature.");
            }
            customFeature->SetName(zhihui_kezi_custom_feature::kFeatureDisplayName);
            if (editedFeature_ != nullptr) { editedFeature_ = customFeature; }
            Log(session_, std::string("单线刻字自定义特征节点创建成功，tag=") +
                              std::to_string(customFeature->Tag()));
            if (executingDeferredEditedReplacement_ &&
                deferredReplacementAnchorTag_ != NULL_TAG &&
                UF_OBJ_ask_status(deferredReplacementAnchorTag_) == UF_OBJ_ALIVE)
            {
                Features::Feature* anchor = dynamic_cast<Features::Feature*>(
                    NXObjectManager::Get(deferredReplacementAnchorTag_));
                if (anchor == nullptr)
                {
                    throw std::runtime_error("Could not resolve the original feature-tree anchor.");
                }

                // A CustomFeature's visible node and its construction members
                // form one timestamp chain. Move the entire chain together;
                // moving only the outer node can leave its boolean/extrude
                // dependencies at the end of the model history.
                std::vector<Features::Feature*> replacementChain;
                for (Features::ConstructionFeatureData* construction :
                     customFeature->GetConstructionFeatures())
                {
                    Features::Feature* member = construction == nullptr
                        ? nullptr
                        : construction->GetFeature();
                    if (member != nullptr && member->Tag() != anchor->Tag())
                    {
                        replacementChain.push_back(member);
                    }
                }
                replacementChain.push_back(customFeature);
                std::sort(replacementChain.begin(), replacementChain.end(),
                    [](Features::Feature* lhs, Features::Feature* rhs) {
                        return lhs->Timestamp() < rhs->Timestamp();
                    });

                Features::FeatureCollection* features = workPart->Features();
                bool modelDelaySuspended = false;
                try
                {
                    features->SuspendModelDelayBeforeReorder();
                    modelDelaySuspended = true;
                    features->ReorderFeature(
                        replacementChain,
                        anchor,
                        Features::FeatureCollection::ReorderTypeBefore);
                    features->RestoreModelDelayAfterReorder();
                    modelDelaySuspended = false;
                    Log(session_, std::string("编辑单线刻字: 新节点及内部链已恢复原特征树位置，数量=") +
                                      std::to_string(replacementChain.size()) +
                                      ", anchorTag=" + std::to_string(anchor->Tag()));
                }
                catch (...)
                {
                    if (modelDelaySuspended)
                    {
                        try { features->RestoreModelDelayAfterReorder(); } catch (...) {}
                    }
                    throw;
                }
            }
            else if (executingDeferredEditedReplacement_)
            {
                Log(session_, "编辑单线刻字: 原节点位于特征树末尾，无需重排");
            }
            try { session_->DeleteUndoMark(modernEngravingMark, "KeZi MODERN engraving"); } catch (...) {}
        }
        catch (...)
        {
            buildingModernCustomFeature_ = false;
            if (customBuilder != nullptr) { try { customBuilder->Destroy(); } catch (...) {} }
            try { session_->UndoToMark(modernEngravingMark, "KeZi MODERN engraving"); } catch (...) {}
            try { session_->DeleteUndoMark(modernEngravingMark, "KeZi MODERN engraving"); } catch (...) {}
            throw;
        }
    }

    Tooling::InsertTextBuilder* CreatePreparedBuilder(NXObject** textUdoOut,
                                                      Face* overrideSelectedFace = nullptr,
                                                      const Point3d* overrideOriginReference = nullptr,
                                                      const Matrix3x3* overrideMatrix = nullptr,
                                                      const std::string* overrideText = nullptr,
                                                      const bool commitModernGeometry = false)
    {
        Log(session_, "开始准备注塑模向导刻字 Builder");
        if (textUdoOut != nullptr)
        {
            *textUdoOut = nullptr;
        }

        TextSettings settings = executingDeferredEditedReplacement_
            ? deferredReplacementSettings_
            : ReadSettings();
        if (settings.height <= 0.0 || settings.depth <= 0.0 || settings.widthScale <= 0.0 ||
            (settings.vShape && settings.vShapeWidth <= 0.0))
        {
            throw std::runtime_error("Text height, depth, length, and width ratio must be greater than 0.");
        }

        Face* selectedFace = overrideSelectedFace != nullptr
            ? overrideSelectedFace
            : (executingDeferredEditedReplacement_ && deferredReplacementFace_ != nullptr
                   ? deferredReplacementFace_
                   : SelectedFace());
        if (selectedFace == nullptr)
        {
            throw std::runtime_error("Manual mode requires a selected engraving face.");
        }
        Log(session_, "已取得选择面");
        Assemblies::Component* selectedComponent = selectedFace->IsOccurrence() ? selectedFace->OwningComponent() : nullptr;
        if (overrideSelectedFace != nullptr)
        {
            selectedComponent = nullptr;
        }
        lastComponent_ = selectedComponent;
        lastComponentJournalId_.clear();
        if (selectedComponent != nullptr)
        {
            lastComponentJournalId_ = ToString(selectedComponent->JournalIdentifier());
        }
        Face* face = PrototypeFace(selectedFace);
        if (face == nullptr)
        {
            throw std::runtime_error("Could not resolve prototype face of the selected assembly face.");
        }

        Point3d facePoint;
        Vector3d faceNormal;
        Point3d pickedPoint(0.0, 0.0, 0.0);
        bool hasPickedPoint = false;
        if (!executingDeferredEditedReplacement_ && manualFace_ != nullptr)
        {
            pickedPoint = manualFace_->PickPoint();
            hasPickedPoint = std::isfinite(pickedPoint.X) &&
                             std::isfinite(pickedPoint.Y) &&
                             std::isfinite(pickedPoint.Z);
        }
        if (selectedComponent != nullptr && hasPickedPoint)
        {
            pickedPoint = ComponentPointToPrototype(selectedComponent, pickedPoint);
        }
        if (!AskPlanarFaceData(face, hasPickedPoint ? &pickedPoint : nullptr, &facePoint, &faceNormal))
        {
            throw std::runtime_error("Selected face is not a valid planar engraving face.");
        }

        Point3d originReference = overrideOriginReference != nullptr
            ? *overrideOriginReference
            : (executingDeferredEditedReplacement_ ? deferredReplacementOrigin_ : pickedPoint);
        if (!executingDeferredEditedReplacement_ &&
            orientation_ != nullptr && orientation_->IsOriginSpecified())
        {
            if (overrideOriginReference == nullptr)
            {
                originReference = orientation_->Origin();
            }
            if (selectedComponent != nullptr)
            {
                originReference = ComponentPointToPrototype(selectedComponent, originReference);
            }
        }
        const Point3d pickedOrigin = ProjectPointToFacePlane(facePoint, faceNormal, originReference);

        WorkContextGuard workContext(session_, selectedFace);
        Part* workPart = workContext.WorkPart();
        if (workPart == nullptr)
        {
            throw std::runtime_error("Could not set the selected component as work part.");
        }
        Log(session_, std::string("目标叶子部件: ") + PartLeafName(workPart));

        NXObject* body = ResolveBodyFromFace(face);
        if (body == nullptr)
        {
            throw std::runtime_error("Could not resolve the body of the selected face.");
        }
        Log(session_, std::string("已取得目标体: ") + ToString(body->Name()));

        const std::string textRule = ResolveEngravingText(body, workPart, settings, overrideText);
        Log(session_, std::string("解析刻字文本: ") + textRule);
        const Point3d textOrigin = overrideOriginReference != nullptr
                                       ? *overrideOriginReference
                                       : (executingDeferredEditedReplacement_
                                              ? deferredReplacementOrigin_
                                              : ApplyCenterOptions(face, faceNormal, pickedOrigin, settings));
        const Matrix3x3 textMatrix = overrideMatrix != nullptr
                                         ? *overrideMatrix
                                         : (executingDeferredEditedReplacement_
                                                ? deferredReplacementMatrix_
                                                : TextMatrixFromDialog(face, faceNormal, settings, selectedComponent));

        // V mode never enters Moldwizard/InsertTextBuilder.  Preview displays
        // the MODERN center-line curves; commit extrudes every curve with a
        // symmetric user width and subtracts it from the selected body.
        if (settings.vShape)
        {
            Log(session_, commitModernGeometry
                              ? "V形正式提交: 进入MODERN曲线对称拉伸求差逻辑"
                              : "V形预览: 进入MODERN单线曲线预览逻辑");
            CreateModernCurveEngraving(
                face,
                textOrigin,
                textMatrix,
                textRule,
                settings,
                workPart,
                dynamic_cast<Body*>(body),
                commitModernGeometry);
            workContext.Restore();
            return nullptr;
        }

        if (session_->ToolingSession() != nullptr)
        {
            session_->ToolingSession()->SetWizardType(1);
        }

        Tooling::InsertTextBuilder* builder = workPart->ToolingManager()->Moldwizard()->CreateInsertTextBuilder();
        if (builder == nullptr)
        {
            throw std::runtime_error("Could not create InsertTextBuilder.");
        }

        try
        {
            builder->SetInsertTextType(Tooling::InsertTextBuilder::InsertTypeThroughPoint);
            builder->SetLockAspectRatio(settings.lockAspect);
            builder->SetTextLayer(settings.layer);
            builder->ComponentTextHeight()->SetFormula(FormatNumber(settings.height).c_str());
            builder->SetTextRule(textRule.c_str());
            builder->FontHeight()->SetFormula(FormatNumber(settings.height).c_str());
            builder->SetTextPosition(Tooling::InsertTextBuilder::TextPositionOptionTopCenter);
            builder->Offset()->SetFormula(FormatNumber(std::max(0.0, settings.textLength)).c_str());
            if (settings.xShortSide)
            {
                builder->SetNumberingDirection(Tooling::InsertTextBuilder::NumberDirectionOptionAlongYAxis);
            }
            else
            {
                builder->SetNumberingDirection(Tooling::InsertTextBuilder::NumberDirectionOptionAlongXAxis);
            }
            builder->NumberingWidth()->SetFormula(FormatNumber(settings.textLength > 0.0 ? settings.textLength : settings.height).c_str());
            builder->LetteringDepth()->SetFormula(FormatNumber(settings.depth).c_str());
            builder->SetTextColor(ReadColor(workPart, textColor_, 186));
            builder->BoundingDepth()->SetFormula(FormatNumber(settings.boundaryDepth).c_str());
            builder->SetBoundaryColor(ReadColor(workPart, boundaryColor_, 186));
            if (settings.textLength > 0.0)
            {
                builder->TextLength()->SetFormula(FormatNumber(settings.textLength).c_str());
            }
            builder->TextShear()->SetFormula(FormatNumber(settings.shear).c_str());
            builder->SetTextRule(textRule.c_str());
            builder->SetScript(Features::TextBuilder::ScriptOptionsWestern);
            builder->SetTextWScale(settings.widthScale);
            builder->SetCreateEmbossedText(settings.embossed);
            builder->SetCreateVShapeText(false);
            builder->SetFontName(settings.fontName.c_str());
            builder->SetBoundaryType(BoundaryTypeFromIndex(settings.boundary));
            builder->CleanUpRedundantData();
            builder->SetCsysOrigin(textOrigin);
            builder->SetCsysMatrix(textMatrix);

            NXObject* textUdo = nullptr;
            builder->CreateNewTextUDO(face, "MW_TEXT_SingleTextUDO", &textUdo);
            if (textUdo == nullptr)
            {
                throw std::runtime_error("InsertTextBuilder did not create preview UDO.");
            }
            Log(session_, "已创建刻字预览 UDO");

            builder->SetSingleTextUDO(textUdo);
            builder->SetLastSelectedFace(face);
            builder->SetFontName(settings.fontName.c_str());
            builder->SetScript(Features::TextBuilder::ScriptOptionsWestern);
            builder->UpdateTextPreview(textUdo);

            SelectionIntentRuleOptions* options = workPart->ScRuleFactory()->CreateRuleOptions();
            options->SetSelectedFromInactive(false);
            std::vector<Face*> faces(1, face);
            FaceDumbRule* faceRule = workPart->ScRuleFactory()->CreateRuleFaceDumb(faces, options);
            delete options;

            std::vector<SelectionIntentRule*> rules;
            rules.push_back(faceRule);
            builder->SelectFace()->ReplaceRules(rules, false);

            builder->SetCsysOrigin(textOrigin);
            builder->SetCsysMatrix(textMatrix);
            builder->UpdateTextUDOClientData(9, true);

            if (textUdoOut != nullptr)
            {
                *textUdoOut = textUdo;
            }
            Log(session_, "Builder 准备完成");
            workContext.Restore();
            return builder;
        }
        catch (...)
        {
            builder->Destroy();
            throw;
        }
    }

    void RefreshPreview()
    {
        if (refreshingPreview_)
        {
            return;
        }

        refreshingPreview_ = true;
        ClearPreviewBuilder();
        try
        {
            previewBuilder_ = CreatePreparedBuilder(&previewUdo_);
            SetTextInputResolvedValue(lastResolvedText_);
            Log(session_, "预览刷新完成");
        }
        catch (const NXException& ex)
        {
            Log(session_, std::string("预览失败: ") + ex.Message());
            ClearPreviewBuilder();
        }
        catch (const std::exception& ex)
        {
            Log(session_, std::string("预览失败: ") + ex.what());
            ClearPreviewBuilder();
        }
        catch (...)
        {
            Log(session_, "预览失败: 未知错误");
            ClearPreviewBuilder();
        }
        refreshingPreview_ = false;
    }

    bool FindMatchingFaceForSameBody(Body* candidateBody,
                                     const Face* referenceFace,
                                     const Point3d& referenceFacePoint,
                                     const Vector3d& referenceNormal,
                                     Face** matchedFace,
                                     Point3d* matchedFacePoint,
                                     Vector3d* matchedNormal,
                                     SameBodyPlaneFaceFeature* referenceFeatureOut = nullptr,
                                     SameBodyPlaneFaceFeature* matchedFeatureOut = nullptr) const
    {
        if (candidateBody == nullptr || referenceFace == nullptr || matchedFace == nullptr || matchedFacePoint == nullptr || matchedNormal == nullptr)
        {
            return false;
        }

        SameBodyPlaneFaceFeature referenceFeature = {};
        if (!BuildSameBodyPlanarFaceFeature(const_cast<Face*>(referenceFace), referenceFeature))
        {
            return false;
        }

        const Vector3d normalizedReferenceNormal = Normalize(referenceNormal, Vector3d(0.0, 0.0, 1.0));
        Face* bestFace = nullptr;
        Point3d bestPoint;
        Vector3d bestNormal;
        SameBodyPlaneFaceFeature bestFeature = {};
        double bestDistance = std::numeric_limits<double>::max();
        for (Face* candidateFace : candidateBody->GetFaces())
        {
            SameBodyPlaneFaceFeature candidateFeature = {};
            if (!BuildSameBodyPlanarFaceFeature(candidateFace, candidateFeature) ||
                !SameBodyPlaneFaceMatch(referenceFeature, candidateFeature))
            {
                continue;
            }

            Point3d candidatePoint;
            Vector3d candidateNormal;
            if (!AskPlanarFaceData(candidateFace, nullptr, &candidatePoint, &candidateNormal))
            {
                continue;
            }

            const Vector3d normalizedCandidateNormal = Normalize(candidateNormal, Vector3d(0.0, 0.0, 1.0));
            if (Dot(normalizedReferenceNormal, normalizedCandidateNormal) < 0.95)
            {
                continue;
            }

            const double dx = candidatePoint.X - referenceFacePoint.X;
            const double dy = candidatePoint.Y - referenceFacePoint.Y;
            const double dz = candidatePoint.Z - referenceFacePoint.Z;
            const double distance = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                bestFace = candidateFace;
                bestPoint = candidatePoint;
                bestNormal = candidateNormal;
                bestFeature = candidateFeature;
            }
        }

        if (bestFace == nullptr)
        {
            return false;
        }
        *matchedFace = bestFace;
        *matchedFacePoint = bestPoint;
        *matchedNormal = bestNormal;
        if (referenceFeatureOut != nullptr)
        {
            *referenceFeatureOut = referenceFeature;
        }
        if (matchedFeatureOut != nullptr)
        {
            *matchedFeatureOut = bestFeature;
        }
        return true;
    }

    Point3d TransferSameBodyTextOrigin(const Point3d& referenceOrigin,
                                       const SameBodyFrame3& referenceFrame,
                                       const SameBodyFrame3& candidateFrame) const
    {
        SameBodyFrame3 alignedCandidateFrame = candidateFrame;
        if (SameBodyDotPoint(referenceFrame.xAxis, alignedCandidateFrame.xAxis) < 0.0)
        {
            alignedCandidateFrame.xAxis = SameBodyScalePoint(alignedCandidateFrame.xAxis, -1.0);
            alignedCandidateFrame.yAxis = SameBodyScalePoint(alignedCandidateFrame.yAxis, -1.0);
        }
        const SameBodyPoint3 localPoint = TransformSameBodyWorldPointToLocal(referenceFrame, SameBodyPointFromNx(referenceOrigin));
        return SameBodyPointToNx(TransformSameBodyLocalPointToWorld(alignedCandidateFrame, localPoint));
    }

    Matrix3x3 TransferSameBodyTextMatrix(const Matrix3x3& referenceMatrix,
                                         const SameBodyFrame3& referenceFrame,
                                         const SameBodyFrame3& candidateFrame) const
    {
        SameBodyFrame3 alignedCandidateFrame = candidateFrame;
        if (SameBodyDotPoint(referenceFrame.xAxis, alignedCandidateFrame.xAxis) < 0.0)
        {
            alignedCandidateFrame.xAxis = SameBodyScalePoint(alignedCandidateFrame.xAxis, -1.0);
            alignedCandidateFrame.yAxis = SameBodyScalePoint(alignedCandidateFrame.yAxis, -1.0);
        }
        const SameBodyPoint3 refX = SameBodyPointFromVector(Vector3d(referenceMatrix.Xx, referenceMatrix.Xy, referenceMatrix.Xz));
        const SameBodyPoint3 refY = SameBodyPointFromVector(Vector3d(referenceMatrix.Yx, referenceMatrix.Yy, referenceMatrix.Yz));
        const SameBodyPoint3 refZ = SameBodyPointFromVector(Vector3d(referenceMatrix.Zx, referenceMatrix.Zy, referenceMatrix.Zz));

        Vector3d x = SameBodyVectorToNx(TransformSameBodyLocalVectorToWorld(alignedCandidateFrame, TransformSameBodyWorldVectorToLocal(referenceFrame, refX)));
        Vector3d y = SameBodyVectorToNx(TransformSameBodyLocalVectorToWorld(alignedCandidateFrame, TransformSameBodyWorldVectorToLocal(referenceFrame, refY)));
        Vector3d z = SameBodyVectorToNx(TransformSameBodyLocalVectorToWorld(alignedCandidateFrame, TransformSameBodyWorldVectorToLocal(referenceFrame, refZ)));

        z = Normalize(z, SameBodyVectorToNx(alignedCandidateFrame.zAxis));
        x = Normalize(ProjectToPlane(x, z), SameBodyVectorToNx(alignedCandidateFrame.xAxis));
        y = Normalize(Cross(z, x), SameBodyVectorToNx(alignedCandidateFrame.yAxis));
        x = Normalize(Cross(y, z), x);
        return MakeMatrix(x, y, z);
    }

    void HideEngravedBody(NXObject* object)
    {
        if (object == nullptr)
        {
            return;
        }
        try
        {
            DisplayableObject* displayable = dynamic_cast<DisplayableObject*>(object);
            if (displayable != nullptr)
            {
                displayable->Blank();
                return;
            }
        }
        catch (...)
        {
        }
        try
        {
            UF_OBJ_set_blank_status(object->Tag(), UF_OBJ_BLANKED);
        }
        catch (...)
        {
        }
    }

    void HideComponents(const std::vector<Assemblies::Component*>& components)
    {
        if (components.empty())
        {
            return;
        }
        for (Assemblies::Component* component : components)
        {
            if (component == nullptr)
            {
                continue;
            }
            try
            {
                // Use the occurrence's standard blank status. This is the
                // same recoverable display state used by NX's normal
                // Show/Hide commands and is visible in the Assembly Navigator.
                // HideComponentBuilder stores a separate assembly display
                // state which may not be recoverable with ordinary Show.
                component->Blank();
                Log(session_, std::string("已标准隐藏刻字组件，可用显示命令恢复: ") +
                                  ToString(component->Name()) +
                                  ", tag=" + std::to_string(component->Tag()));
            }
            catch (const NXException& ex)
            {
                Log(session_, std::string("标准隐藏刻字组件失败: ") + ex.Message());
            }
            catch (...)
            {
                Log(session_, "标准隐藏刻字组件失败: 未知异常");
            }
        }
    }

    void CommitPreparedBuilder(Tooling::InsertTextBuilder* builder, NXObject* textUdo)
    {
        if (builder == nullptr)
        {
            return;
        }
        builder->Commit();
    }

    struct AutoTubeEngravingTarget
    {
        Body* body = nullptr;
        Face* face = nullptr;
        Point3d origin;
        Matrix3x3 matrix;
        double screenX = 0.0;
        double screenY = 0.0;
    };

    static std::size_t Utf8CharacterCount(const std::string& text)
    {
        std::size_t count = 0;
        for (unsigned char ch : text)
        {
            if ((ch & 0xC0) != 0x80)
            {
                ++count;
            }
        }
        return count;
    }

    bool TextEnvelopeFitsFace(Face* face,
                              const Point3d& origin,
                              const Vector3d& longAxis,
                              const Vector3d& shortAxis,
                              const double halfLong,
                              const double halfShort,
                              const double sampleSpacing) const
    {
        if (face == nullptr || halfLong <= 0.0 || halfShort <= 0.0)
        {
            return false;
        }

        const double spacing = std::max(0.5, sampleSpacing);
        const int longSamples = std::clamp(static_cast<int>(std::ceil((2.0 * halfLong) / spacing)) + 1, 5, 61);
        const int shortSamples = std::clamp(static_cast<int>(std::ceil((2.0 * halfShort) / spacing)) + 1, 5, 21);
        const auto pointIsInside = [face, &origin, &longAxis, &shortAxis](const double longOffset, const double shortOffset) {
            const Point3d sample = OffsetPoint(OffsetPoint(origin, longAxis, longOffset), shortAxis, shortOffset);
            double point[3] = {sample.X, sample.Y, sample.Z};
            int status = 0;
            return UF_MODL_ask_point_containment(point, face->Tag(), &status) == 0 && status == 1;
        };

        // Most rejected locations intersect an opening or an outer boundary.  Check
        // representative points first, then retain the original dense scan for safety.
        static const double quickFractions[][2] = {
            {0.0, 0.0}, {-1.0, -1.0}, {-1.0, 1.0}, {1.0, -1.0}, {1.0, 1.0},
            {-1.0, 0.0}, {1.0, 0.0}, {0.0, -1.0}, {0.0, 1.0},
            {-0.5, 0.0}, {0.5, 0.0}, {0.0, -0.5}, {0.0, 0.5}
        };
        for (const auto& fraction : quickFractions)
        {
            if (!pointIsInside(fraction[0] * halfLong, fraction[1] * halfShort))
            {
                return false;
            }
        }
        for (int longIndex = 0; longIndex < longSamples; ++longIndex)
        {
            const double longFraction = longSamples == 1 ? 0.0 : static_cast<double>(longIndex) / static_cast<double>(longSamples - 1);
            const double longOffset = -halfLong + 2.0 * halfLong * longFraction;
            for (int shortIndex = 0; shortIndex < shortSamples; ++shortIndex)
            {
                const double shortFraction = shortSamples == 1 ? 0.0 : static_cast<double>(shortIndex) / static_cast<double>(shortSamples - 1);
                const double shortOffset = -halfShort + 2.0 * halfShort * shortFraction;
                if (!pointIsInside(longOffset, shortOffset))
                {
                    return false;
                }
            }
        }
        return true;
    }

    bool FindAutomaticTextOrigin(Face* face,
                                 const Point3d& faceCenter,
                                 const Vector3d& longAxis,
                                 const Vector3d& shortAxis,
                                 const double longSpan,
                                 const double shortSpan,
                                 const TextSettings& settings,
                                 const std::string& text,
                                 Point3d* origin) const
    {
        if (face == nullptr || origin == nullptr)
        {
            return false;
        }

        const double height = std::max(0.1, settings.height);
        const double textWidth = height *
            static_cast<double>(std::max<std::size_t>(1, Utf8CharacterCount(text))) *
            settings.widthScale / 100.0;
        const double clearance = std::max(0.5, height * 0.2);
        const double halfLong = 0.5 * textWidth + clearance;
        const double halfShort = 0.5 * height + clearance;
        const double maximumLongOffset = 0.5 * longSpan - halfLong;
        if (maximumLongOffset < 0.0 || 0.5 * shortSpan < halfShort)
        {
            return false;
        }

        const double spacing = std::max(0.5, height * 0.25);
        const double searchStep = std::max(1.0, height * 0.5);
        const int steps = static_cast<int>(std::ceil(maximumLongOffset / searchStep));
        for (int step = 0; step <= steps; ++step)
        {
            const double distance = std::min(maximumLongOffset, static_cast<double>(step) * searchStep);
            const int directionCount = step == 0 ? 1 : 2;
            for (int directionIndex = 0; directionIndex < directionCount; ++directionIndex)
            {
                const double signedDistance = step == 0 || directionIndex == 0 ? distance : -distance;
                const Point3d candidate = OffsetPoint(faceCenter, longAxis, signedDistance);
                if (TextEnvelopeFitsFace(face, candidate, longAxis, shortAxis, halfLong, halfShort, spacing))
                {
                    *origin = candidate;
                    return true;
                }
            }
        }
        return false;
    }

    bool BuildAutomaticTubeTarget(Body* body,
                                  const Vector3d& viewX,
                                  const Vector3d& viewY,
                                  const Vector3d& viewZ,
                                  const TextSettings& settings,
                                  AutoTubeEngravingTarget* target) const
    {
        if (body == nullptr || target == nullptr || body->IsBlanked() || !body->IsSolidBody())
        {
            return false;
        }

        struct FaceChoice
        {
            Face* face = nullptr;
            Point3d center;
            Vector3d normal;
            Vector3d longAxis;
            Vector3d shortAxis;
            double longSpan = 0.0;
            double shortSpan = 0.0;
            double alignment = 0.0;
            double area = 0.0;
            double planeOffset = 0.0;
        };

        std::vector<FaceChoice> choices;
        int elongatedPlanarFaceCount = 0;
        for (Face* face : body->GetFaces())
        {
            try
            {
                Point3d facePoint;
                Vector3d faceNormal;
                if (!AskPlanarFaceData(face, nullptr, &facePoint, &faceNormal))
                {
                    continue;
                }
                faceNormal = Normalize(faceNormal, Vector3d(0.0, 0.0, 1.0));

                Vector3d longAxis;
                Vector3d shortAxis;
                if (!FaceLongShortAxes(face, faceNormal, &longAxis, &shortAxis))
                {
                    continue;
                }
                if (Dot(longAxis, viewX) < 0.0)
                {
                    longAxis = Vector3d(-longAxis.X, -longAxis.Y, -longAxis.Z);
                }
                shortAxis = Normalize(Cross(faceNormal, longAxis), shortAxis);
                longAxis = Normalize(Cross(shortAxis, faceNormal), longAxis);

                const std::vector<Point3d> vertices = FaceVertices(face);
                double longCenter = 0.0;
                double shortCenter = 0.0;
                const double longSpan = ProjectionSpan(vertices, longAxis, &longCenter);
                const double shortSpan = ProjectionSpan(vertices, shortAxis, &shortCenter);
                if (longSpan < std::max(shortSpan * 1.5, settings.height * 3.0) || shortSpan < settings.height * 1.2)
                {
                    continue;
                }
                ++elongatedPlanarFaceCount;

                const double alignment = Dot(faceNormal, viewZ);
                if (alignment <= 0.05)
                {
                    continue;
                }

                // Auto face selection only needs the planar center and relative area.
                // Using the face point and vertex projection avoids an expensive exact
                // area/centroid measurement on every face.
                Point3d center = facePoint;
                const double currentLong = center.X * longAxis.X + center.Y * longAxis.Y + center.Z * longAxis.Z;
                const double currentShort = center.X * shortAxis.X + center.Y * shortAxis.Y + center.Z * shortAxis.Z;
                center = OffsetPoint(center, longAxis, longCenter - currentLong);
                center = OffsetPoint(center, shortAxis, shortCenter - currentShort);

                FaceChoice choice;
                choice.face = face;
                choice.center = center;
                choice.normal = faceNormal;
                choice.longAxis = longAxis;
                choice.shortAxis = shortAxis;
                choice.longSpan = longSpan;
                choice.shortSpan = shortSpan;
                choice.alignment = alignment;
                choice.area = longSpan * shortSpan;
                choice.planeOffset = center.X * faceNormal.X + center.Y * faceNormal.Y + center.Z * faceNormal.Z;
                choices.push_back(choice);
            }
            catch (...)
            {
            }
        }

        if (elongatedPlanarFaceCount < 4 || choices.empty())
        {
            return false;
        }

        std::vector<FaceChoice> exteriorChoices;
        for (std::size_t candidateIndex = 0; candidateIndex < choices.size(); ++candidateIndex)
        {
            bool hasOuterParallelFace = false;
            for (std::size_t otherIndex = 0; otherIndex < choices.size(); ++otherIndex)
            {
                if (candidateIndex == otherIndex)
                {
                    continue;
                }
                const FaceChoice& candidate = choices[candidateIndex];
                const FaceChoice& other = choices[otherIndex];
                if (Dot(candidate.normal, other.normal) > 0.98 &&
                    other.planeOffset > candidate.planeOffset + kSameBodyDistanceTolerance)
                {
                    hasOuterParallelFace = true;
                    break;
                }
            }
            if (!hasOuterParallelFace)
            {
                exteriorChoices.push_back(choices[candidateIndex]);
            }
        }
        if (exteriorChoices.empty())
        {
            return false;
        }

        std::sort(exteriorChoices.begin(), exteriorChoices.end(), [](const FaceChoice& lhs, const FaceChoice& rhs) {
            if (std::fabs(lhs.alignment - rhs.alignment) > 0.02)
            {
                return lhs.alignment > rhs.alignment;
            }
            return lhs.area > rhs.area;
        });

        for (const FaceChoice& choice : exteriorChoices)
        {
            Point3d origin;
            if (!FindAutomaticTextOrigin(
                    choice.face,
                    choice.center,
                    choice.longAxis,
                    choice.shortAxis,
                    choice.longSpan,
                    choice.shortSpan,
                    settings,
                    settings.text,
                    &origin))
            {
                continue;
            }

            target->body = body;
            target->face = choice.face;
            target->origin = origin;
            target->matrix = MakeMatrix(choice.longAxis, choice.shortAxis, choice.normal);
            target->screenX = origin.X * viewX.X + origin.Y * viewX.Y + origin.Z * viewX.Z;
            target->screenY = origin.X * viewY.X + origin.Y * viewY.Y + origin.Z * viewY.Z;
            return true;
        }
        return false;
    }

    std::vector<AutoTubeEngravingTarget> CollectAutomaticTubeTargets(Part* part,
                                                                     const TextSettings& settings,
                                                                     int* visibleBodyCount) const
    {
        std::vector<AutoTubeEngravingTarget> targets;
        if (visibleBodyCount != nullptr)
        {
            *visibleBodyCount = 0;
        }
        if (part == nullptr || part->Views() == nullptr)
        {
            return targets;
        }

        View* workView = part->Views()->WorkView();
        if (workView == nullptr)
        {
            return targets;
        }
        const Matrix3x3 viewMatrix = workView->Matrix();
        const Vector3d viewX = Normalize(Vector3d(viewMatrix.Xx, viewMatrix.Xy, viewMatrix.Xz), Vector3d(1.0, 0.0, 0.0));
        const Vector3d viewY = Normalize(Vector3d(viewMatrix.Yx, viewMatrix.Yy, viewMatrix.Yz), Vector3d(0.0, 1.0, 0.0));
        const Vector3d viewZ = Normalize(Vector3d(viewMatrix.Zx, viewMatrix.Zy, viewMatrix.Zz), Vector3d(0.0, 0.0, 1.0));

        const std::vector<Body*> bodies = CollectVisibleBodies(part);
        if (visibleBodyCount != nullptr)
        {
            *visibleBodyCount = static_cast<int>(bodies.size());
        }
        for (Body* body : bodies)
        {
            AutoTubeEngravingTarget target;
            if (BuildAutomaticTubeTarget(body, viewX, viewY, viewZ, settings, &target))
            {
                targets.push_back(target);
            }
        }
        std::sort(targets.begin(), targets.end(), [](const AutoTubeEngravingTarget& lhs, const AutoTubeEngravingTarget& rhs) {
            if (std::fabs(lhs.screenY - rhs.screenY) > 0.01)
            {
                return lhs.screenY > rhs.screenY;
            }
            return lhs.screenX < rhs.screenX;
        });
        return targets;
    }

    void ApplyAutomaticVisibleTubeEngraving(const TextSettings& settings)
    {
        const auto automaticStartedAt = std::chrono::steady_clock::now();
        Part* workPart = session_ != nullptr && session_->Parts() != nullptr ? session_->Parts()->Work() : nullptr;
        if (workPart == nullptr)
        {
            throw std::runtime_error("Could not get the current work part for automatic tube engraving.");
        }

        int visibleBodyCount = 0;
        const std::vector<AutoTubeEngravingTarget> targets = CollectAutomaticTubeTargets(workPart, settings, &visibleBodyCount);
        if (targets.empty())
        {
            throw std::runtime_error("当前工作部件中没有找到朝向当前视图且有足够刻字空间的可见方通。");
        }

        const std::string textTemplate = EffectiveTextTemplate(settings);
        const bool hasSerial = TextTemplateHasSerial(textTemplate);
        int firstSerial = std::max(0, config_.serialStart);
        std::string batchSerialStyle = config_.serialStyle;
        std::string batchEditableText = settings.text;
        if (hasSerial)
        {
            NXObject* partObject = dynamic_cast<NXObject*>(workPart);
            const std::string firstBodyName = targets.front().body == nullptr ? std::string() : ToString(targets.front().body->Name());
            const std::vector<TemplateToken> firstTokens = BuildTemplateTokens(
                textTemplate, targets.front().body, partObject, firstBodyName, PartLeafName(workPart), config_);
            std::string dialogSerial = SerialNumberText(config_);
            ResolveEditableValuesFromDisplay(firstTokens, settings.text, config_, &batchEditableText, &dialogSerial);
            dialogSerial = Trim(dialogSerial);
            const bool numericDialogSerial = !dialogSerial.empty() &&
                std::all_of(dialogSerial.begin(), dialogSerial.end(), [](unsigned char ch) { return std::isdigit(ch); });
            if (numericDialogSerial)
            {
                try
                {
                    const long long parsed = std::stoll(dialogSerial);
                    if (parsed >= 0 && parsed <= std::numeric_limits<int>::max())
                    {
                        firstSerial = static_cast<int>(parsed);
                        batchSerialStyle = dialogSerial;
                    }
                }
                catch (...)
                {
                }
            }
        }

        const auto analysisFinishedAt = std::chrono::steady_clock::now();
        const long long selectionMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(analysisFinishedAt - automaticStartedAt).count();
        int successfulCount = 0;
        int failedCount = 0;
        int serialOffset = 0;
        int sameBodyGroupCount = 0;
        std::vector<bool> processedTargets(targets.size(), false);
        std::vector<Body*> cachedVisibleBodies;
        std::vector<SameBodyFingerprint> cachedFingerprints;
        if (settings.engraveSameBodies || settings.sameRandomColor)
        {
            cachedVisibleBodies = CollectVisibleBodies(workPart);
            cachedFingerprints.reserve(cachedVisibleBodies.size());
            for (Body* body : cachedVisibleBodies)
            {
                cachedFingerprints.push_back(BuildSameBodyFingerprint(body));
            }
        }

        int coloredGroupCount = 0;
        if (settings.sameRandomColor && cachedVisibleBodies.size() == cachedFingerprints.size())
        {
            std::vector<bool> metadataAssigned(cachedVisibleBodies.size(), false);
            for (std::size_t referenceIndex = 0; referenceIndex < cachedVisibleBodies.size(); ++referenceIndex)
            {
                if (metadataAssigned[referenceIndex])
                {
                    continue;
                }
                metadataAssigned[referenceIndex] = true;
                std::vector<Body*> metadataGroup(1, cachedVisibleBodies[referenceIndex]);
                for (std::size_t candidateIndex = referenceIndex + 1; candidateIndex < cachedVisibleBodies.size(); ++candidateIndex)
                {
                    if (!metadataAssigned[candidateIndex] &&
                        SameBodyFingerprintsMatch(cachedFingerprints[referenceIndex], cachedFingerprints[candidateIndex]))
                    {
                        metadataAssigned[candidateIndex] = true;
                        metadataGroup.push_back(cachedVisibleBodies[candidateIndex]);
                    }
                }

                const bool containsAutomaticTube = std::any_of(
                    metadataGroup.begin(), metadataGroup.end(), [&targets](Body* body) {
                        return std::any_of(targets.begin(), targets.end(), [body](const AutoTubeEngravingTarget& target) {
                            return target.body == body;
                        });
                    });
                if (containsAutomaticTube)
                {
                    ApplySameBodyGroupMetadata(metadataGroup, true);
                    ++coloredGroupCount;
                }
            }
        }

        for (std::size_t representativeIndex = 0; representativeIndex < targets.size(); ++representativeIndex)
        {
            if (processedTargets[representativeIndex])
            {
                continue;
            }
            processedTargets[representativeIndex] = true;
            const AutoTubeEngravingTarget& representative = targets[representativeIndex];
            std::vector<AutoTubeEngravingTarget> groupTargets(1, representative);
            if (settings.engraveSameBodies)
            {
                try
                {
                    Point3d referenceFacePoint;
                    Vector3d referenceFaceNormal;
                    if (!AskPlanarFaceData(representative.face, &representative.origin, &referenceFacePoint, &referenceFaceNormal))
                    {
                        throw std::runtime_error("无法获取自动基准刻字面数据。");
                    }

                    std::vector<Body*> matchingBodies;
                    std::size_t referenceFingerprintIndex = cachedVisibleBodies.size();
                    for (std::size_t index = 0; index < cachedVisibleBodies.size(); ++index)
                    {
                        if (cachedVisibleBodies[index] == representative.body)
                        {
                            referenceFingerprintIndex = index;
                            break;
                        }
                    }
                    if (referenceFingerprintIndex < cachedFingerprints.size())
                    {
                        const SameBodyFingerprint& referenceFingerprint = cachedFingerprints[referenceFingerprintIndex];
                        for (std::size_t index = 0; index < cachedVisibleBodies.size(); ++index)
                        {
                            if (index != referenceFingerprintIndex &&
                                SameBodyFingerprintsMatch(referenceFingerprint, cachedFingerprints[index]))
                            {
                                matchingBodies.push_back(cachedVisibleBodies[index]);
                            }
                        }
                    }
                    for (Body* matchingBody : matchingBodies)
                    {
                        bool alreadyProcessed = false;
                        for (std::size_t candidateIndex = 0; candidateIndex < targets.size(); ++candidateIndex)
                        {
                            if (targets[candidateIndex].body == matchingBody)
                            {
                                alreadyProcessed = processedTargets[candidateIndex];
                                processedTargets[candidateIndex] = true;
                                break;
                            }
                        }
                        if (alreadyProcessed)
                        {
                            continue;
                        }

                        Face* sameFace = nullptr;
                        Point3d sameFacePoint;
                        Vector3d sameFaceNormal;
                        SameBodyPlaneFaceFeature referenceFeature = {};
                        SameBodyPlaneFaceFeature sameFeature = {};
                        if (!FindMatchingFaceForSameBody(
                                matchingBody,
                                representative.face,
                                referenceFacePoint,
                                referenceFaceNormal,
                                &sameFace,
                                &sameFacePoint,
                                &sameFaceNormal,
                                &referenceFeature,
                                &sameFeature))
                        {
                            continue;
                        }

                        AutoTubeEngravingTarget sameTarget;
                        sameTarget.body = matchingBody;
                        sameTarget.face = sameFace;
                        sameTarget.origin = TransferSameBodyTextOrigin(representative.origin, referenceFeature.frame, sameFeature.frame);
                        sameTarget.matrix = TransferSameBodyTextMatrix(representative.matrix, referenceFeature.frame, sameFeature.frame);
                        groupTargets.push_back(sameTarget);
                    }
                }
                catch (const std::exception& ex)
                {
                    Log(session_, std::string("自动刻相同分组失败，按单体处理: ") + ex.what());
                }
                catch (...)
                {
                    Log(session_, "自动刻相同分组失败，按单体处理: 未知异常");
                }
            }
            ++sameBodyGroupCount;

            std::string resolvedText;
            try
            {
                KeZiConfig targetConfig = config_;
                targetConfig.serialStart = firstSerial + serialOffset;
                targetConfig.serialStyle = batchSerialStyle;
                targetConfig.serialPad = static_cast<int>(batchSerialStyle.size());
                NXObject* partObject = dynamic_cast<NXObject*>(workPart);
                const std::string bodyName = representative.body == nullptr ? std::string() : ToString(representative.body->Name());
                const std::vector<TemplateToken> tokens = BuildTemplateTokens(textTemplate, representative.body, partObject, bodyName, PartLeafName(workPart), targetConfig);
                std::string editableText = batchEditableText;
                std::string ignoredSerial = SerialNumberText(targetConfig);
                ResolveEditableValuesFromDisplay(tokens, settings.text, targetConfig, &editableText, &ignoredSerial);
                const std::string explicitSerial = SerialNumberText(targetConfig);
                resolvedText = ExpandTextTemplate(
                    textTemplate,
                    settings.text,
                    representative.body,
                    workPart,
                    targetConfig,
                    &editableText,
                    &explicitSerial);
                if (resolvedText.empty())
                {
                    failedCount += static_cast<int>(groupTargets.size());
                    continue;
                }
            }
            catch (const NXException& ex)
            {
                failedCount += static_cast<int>(groupTargets.size());
                Log(session_, std::string("自动方通规则解析失败: ") + ex.Message());
                continue;
            }
            catch (const std::exception& ex)
            {
                failedCount += static_cast<int>(groupTargets.size());
                Log(session_, std::string("自动方通规则解析失败: ") + ex.what());
                continue;
            }
            catch (...)
            {
                failedCount += static_cast<int>(groupTargets.size());
                Log(session_, "自动方通规则解析失败: 未知异常");
                continue;
            }

            int groupSuccessCount = 0;
            for (const AutoTubeEngravingTarget& target : groupTargets)
            {
                try
                {
                    std::unique_ptr<Tooling::InsertTextBuilder, void (*)(Tooling::InsertTextBuilder*)> builder(
                        nullptr,
                        [](Tooling::InsertTextBuilder* value) {
                            if (value != nullptr)
                            {
                                try { value->Destroy(); } catch (...) {}
                            }
                        });
                    NXObject* textUdo = nullptr;
                    builder.reset(CreatePreparedBuilder(&textUdo, target.face, &target.origin, &target.matrix, &resolvedText, true));
                    CommitPreparedBuilder(builder.get(), textUdo);
                    builder.reset();

                    WriteStringAttribute(target.body, "bianhao", resolvedText);
                    SetObjectNameSafe(session_, target.body, resolvedText);
                    if (settings.hideEngravedText)
                    {
                        HideEngravedBody(target.body);
                    }
                    ++successfulCount;
                    ++groupSuccessCount;
                }
                catch (const NXException& ex)
                {
                    ++failedCount;
                    Log(session_, std::string("自动方通刻字失败: ") + ex.Message());
                }
                catch (const std::exception& ex)
                {
                    ++failedCount;
                    Log(session_, std::string("自动方通刻字失败: ") + ex.what());
                }
                catch (...)
                {
                    ++failedCount;
                    Log(session_, "自动方通刻字失败: 未知异常");
                }
            }
            if (hasSerial && groupSuccessCount > 0)
            {
                ++serialOffset;
            }
        }

        SaveCurrentRule();
        SaveDialogValues();
        if (hasSerial && serialOffset > 0)
        {
            config_.serialStart = firstSerial + serialOffset;
            config_.serialStyle = batchSerialStyle;
            config_.serialPad = static_cast<int>(batchSerialStyle.size());
            WriteConfigValue("起始号", std::to_string(config_.serialStart));
            WriteConfigValue("流水号样式", config_.serialStyle);

            KeZiConfig nextConfig = config_;
            NXObject* partObject = dynamic_cast<NXObject*>(workPart);
            const std::string firstBodyName = targets.front().body == nullptr ? std::string() : ToString(targets.front().body->Name());
            const std::vector<TemplateToken> displayTokens = BuildTemplateTokens(
                textTemplate, targets.front().body, partObject, firstBodyName, PartLeafName(workPart), nextConfig);
            const std::string nextDisplayText = ComposeTemplateDisplayText(displayTokens, batchEditableText, SerialNumberText(nextConfig));
            config_.text = nextDisplayText;
            WriteConfigValue("文本", nextDisplayText);
            if (textValue_ != nullptr)
            {
                updatingResolvedText_ = true;
                textValue_->SetValue(nextDisplayText.c_str());
                updatingResolvedText_ = false;
            }
        }
        if (successfulCount == 0)
        {
            throw std::runtime_error("找到了方通候选面，但所有刻字操作都失败。");
        }

        std::ostringstream summary;
        const auto completedAt = std::chrono::steady_clock::now();
        const long long processingMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(completedAt - analysisFinishedAt).count();
        summary << "自动方通刻字完成\n"
                << "可见实体: " << visibleBodyCount << "\n"
                << "识别方通: " << targets.size() << "\n"
                << "刻字分组: " << sameBodyGroupCount << "\n"
                << "成功: " << successfulCount << "\n"
                << "失败: " << failedCount << "\n"
                << "随机色分组: " << coloredGroupCount << "\n"
                << "识别选面耗时: " << selectionMilliseconds << " ms\n"
                << "分组及刻字耗时: " << processingMilliseconds << " ms";
        ui_->NXMessageBox()->Show("刻字", NXMessageBox::DialogTypeInformation, summary.str().c_str());
    }

    void ApplyEngraving()
    {
        Log(session_, "开始提交刻字");
        struct SameBodyEngravingTarget
        {
            Body* body = nullptr;
            Face* face = nullptr;
            Point3d origin;
            Matrix3x3 matrix;
        };

        std::unique_ptr<Tooling::InsertTextBuilder, void (*)(Tooling::InsertTextBuilder*)> builder(
            nullptr,
            [](Tooling::InsertTextBuilder* value) {
                if (value != nullptr)
                {
                    try
                    {
                        value->Destroy();
                    }
                    catch (...)
                    {
                    }
                }
        });

        const std::string dialogTextAtApply = executingDeferredEditedReplacement_
            ? deferredReplacementText_
            : (textValue_ == nullptr ? std::string() : Trim(ReadString(textValue_)));
        ClearPreviewBuilder();
        Face* selectedFace = executingDeferredEditedReplacement_
            ? deferredReplacementFace_
            : SelectedFace();
        const TextSettings settings = executingDeferredEditedReplacement_
            ? deferredReplacementSettings_
            : ReadSettings();
        if (settings.autoEngraveVisibleTubes)
        {
            ApplyAutomaticVisibleTubeEngraving(settings);
            Log(session_, "自动方通刻字完成");
            return;
        }
        const bool renameComponentToText = settings.renameComponentToText;
        const bool engraveSameBodies = settings.engraveSameBodies;
        const bool sameRandomColor = settings.sameRandomColor;
        const bool hideEngravedText = settings.hideEngravedText;
        WorkContextGuard workContext(session_, selectedFace);

        // Resolve matching bodies before engraving changes the reference body's
        // topology.  Reusing this list also avoids an expensive post-commit scan.
        std::vector<Body*> preEngravingMatchingBodies;
        Body* preEngravingReferenceBody = nullptr;
        if ((engraveSameBodies || sameRandomColor) && selectedFace != nullptr)
        {
            Face* preEngravingFace = PrototypeFace(selectedFace);
            preEngravingReferenceBody = dynamic_cast<Body*>(ResolveBodyFromFace(preEngravingFace));
            Part* preEngravingWorkPart = workContext.WorkPart();
            if (preEngravingReferenceBody != nullptr && preEngravingWorkPart != nullptr)
            {
                const auto sameBodyScanStartedAt = std::chrono::steady_clock::now();
                preEngravingMatchingBodies = FindMatchingVisibleBodies(
                    preEngravingWorkPart, preEngravingReferenceBody);
                const long long sameBodyScanMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - sameBodyScanStartedAt).count();
                Log(session_, std::string("刻字前相同体识别完成，匹配=") +
                                  std::to_string(preEngravingMatchingBodies.size()) +
                                  ", 耗时=" + std::to_string(sameBodyScanMilliseconds) + " ms");
            }
        }
        NXObject* textUdo = nullptr;
        const std::string* deferredTextOverride = executingDeferredEditedReplacement_
            ? &deferredReplacementText_
            : nullptr;
        builder.reset(CreatePreparedBuilder(
            &textUdo,
            executingDeferredEditedReplacement_ ? deferredReplacementFace_ : nullptr,
            executingDeferredEditedReplacement_ ? &deferredReplacementOrigin_ : nullptr,
            executingDeferredEditedReplacement_ ? &deferredReplacementMatrix_ : nullptr,
            deferredTextOverride,
            true));
        NXObject* committedBody = lastBody_;
        Assemblies::Component* committedComponent = lastComponent_;
        Part* committedWorkPart = lastWorkPart_;
        Face* committedPrototypeFace = selectedFace != nullptr ? PrototypeFace(selectedFace) : nullptr;
        Body* referenceBody = dynamic_cast<Body*>(committedBody);
        Point3d referenceFacePoint;
        Vector3d referenceFaceNormal;
        Point3d referenceOrigin;
        Matrix3x3 referenceMatrix;
        bool hasSameBodyReference = false;
        if (!executingDeferredEditedReplacement_ &&
            committedPrototypeFace != nullptr && referenceBody != nullptr && committedWorkPart != nullptr)
        {
            Point3d pickedPoint = manualFace_ != nullptr ? manualFace_->PickPoint() : Point3d(0.0, 0.0, 0.0);
            if (selectedFace != nullptr && selectedFace->IsOccurrence() && selectedFace->OwningComponent() != nullptr)
            {
                pickedPoint = ComponentPointToPrototype(selectedFace->OwningComponent(), pickedPoint);
            }
            if (AskPlanarFaceData(committedPrototypeFace, &pickedPoint, &referenceFacePoint, &referenceFaceNormal))
            {
                referenceOrigin = ProjectPointToFacePlane(referenceFacePoint, referenceFaceNormal, orientation_ != nullptr && orientation_->IsOriginSpecified() ? orientation_->Origin() : pickedPoint);
                referenceOrigin = ApplyCenterOptions(committedPrototypeFace, referenceFaceNormal, referenceOrigin, settings);
                referenceMatrix = TextMatrixFromDialog(committedPrototypeFace, referenceFaceNormal, settings, selectedFace != nullptr && selectedFace->IsOccurrence() ? selectedFace->OwningComponent() : nullptr);
                hasSameBodyReference = true;
            }
        }
        const std::string committedComponentJournalId = lastComponentJournalId_;
        const std::string committedText =
            (!dialogTextAtApply.empty() && !TextTemplateHasRuleToken(dialogTextAtApply)) ? dialogTextAtApply : lastResolvedText_;
        const std::string committedTemplate = lastTextTemplate_;
        Log(session_, std::string("应用文本: 对话框=") + dialogTextAtApply + ", 解析=" + lastResolvedText_ + ", 使用=" + committedText);

        std::vector<SameBodyEngravingTarget> sameBodyTargets;
        std::vector<Body*> matchingBodies = preEngravingMatchingBodies;
        if (engraveSameBodies && !committedText.empty())
        {
            for (Body* sameBody : matchingBodies)
            {
                Face* sameFace = nullptr;
                Point3d sameFacePoint;
                Vector3d sameFaceNormal;
                SameBodyPlaneFaceFeature referenceFeature = {};
                SameBodyPlaneFaceFeature sameFeature = {};
                if (!FindMatchingFaceForSameBody(sameBody, committedPrototypeFace, referenceFacePoint, referenceFaceNormal, &sameFace, &sameFacePoint, &sameFaceNormal, &referenceFeature, &sameFeature))
                {
                    continue;
                }

                SameBodyEngravingTarget target;
                target.body = sameBody;
                target.face = sameFace;
                target.origin = TransferSameBodyTextOrigin(referenceOrigin, referenceFeature.frame, sameFeature.frame);
                target.matrix = TransferSameBodyTextMatrix(referenceMatrix, referenceFeature.frame, sameFeature.frame);
                sameBodyTargets.push_back(target);
            }
        }
        if (sameRandomColor && referenceBody != nullptr)
        {
            std::vector<Body*> metadataBodies(1, referenceBody);
            metadataBodies.insert(metadataBodies.end(), matchingBodies.begin(), matchingBodies.end());
            ApplySameBodyGroupMetadata(metadataBodies, true);
        }

        CommitPreparedBuilder(builder.get(), textUdo);
        Log(session_, "刻字 Commit 完成");
        if (committedBody != nullptr && !committedText.empty())
        {
            WriteStringAttribute(committedBody, "bianhao", committedText);
            Log(session_, std::string("已写入体属性 bianhao=") + committedText);
            SetObjectNameSafe(session_, committedBody, committedText);
            Log(session_, std::string("已修改体名=") + committedText);
            if (hideEngravedText && committedComponent == nullptr)
            {
                HideEngravedBody(committedBody);
            }
        }
        Log(session_, "提交Builder销毁开始");
        builder.reset();
        Log(session_, "提交Builder销毁完成");
        if (executingDeferredEditedReplacement_)
        {
            // The replacement node and all construction members are already
            // committed. Everything below is create-mode UI/config/assembly
            // post-processing and may dereference Block Styler controls after
            // the edit dialog has closed.
            workContext.Restore();
            Log(session_, "编辑单线刻字: 新打包节点已完成，跳过新建模式后处理");
            Log(session_, "ApplyEngraving完成");
            return;
        }
        for (const SameBodyEngravingTarget& target : sameBodyTargets)
        {
            std::unique_ptr<Tooling::InsertTextBuilder, void (*)(Tooling::InsertTextBuilder*)> sameBuilder(
                nullptr,
                [](Tooling::InsertTextBuilder* value) {
                    if (value != nullptr)
                    {
                        try { value->Destroy(); } catch (...) {}
                    }
                });
            NXObject* sameTextUdo = nullptr;
            sameBuilder.reset(CreatePreparedBuilder(&sameTextUdo, target.face, &target.origin, &target.matrix, &committedText, true));
            CommitPreparedBuilder(sameBuilder.get(), sameTextUdo);
            sameBuilder.reset();
            WriteStringAttribute(target.body, "bianhao", committedText);
            SetObjectNameSafe(session_, target.body, committedText);
            if (hideEngravedText)
            {
                HideEngravedBody(target.body);
            }
        }
        SaveCurrentRule();
        SaveDialogValues();
        if (textValue_ != nullptr)
        {
            std::string nextText;
            const std::string currentText = ReadString(textValue_);
            const bool incremented = TextTemplateHasSerial(committedTemplate)
                                         ? IncrementDisplaySerialByRule(currentText, &nextText)
                                         : IncrementTextSerial(Trim(currentText), &nextText);
            if (incremented)
            {
                config_.text = nextText;
                WriteConfigValue("文本", nextText);
                try
                {
                    updatingResolvedText_ = true;
                    textValue_->SetValue(nextText.c_str());
                    updatingResolvedText_ = false;
                }
                catch (...)
                {
                    updatingResolvedText_ = false;
                }
                Log(session_, std::string("流水号已递增为: ") + nextText);
            }
        }
        std::filesystem::path originalPartPath;
        std::filesystem::path replacementPartPath;
        std::string replaceError;
        const bool shouldReplaceComponent =
            renameComponentToText && committedComponent != nullptr && !committedComponentJournalId.empty() && !committedText.empty();
        bool copiedReplacementPart = false;
        if (shouldReplaceComponent)
        {
            Log(session_, "准备保存并复制部件文件");
            copiedReplacementPart = SaveAndCopyComponentPart(
                session_,
                lastWorkPart_,
                committedText,
                &originalPartPath,
                &replacementPartPath,
                &replaceError);
        }

        Log(session_, "准备恢复装配工作上下文");
        workContext.Restore();
        Log(session_, "已恢复装配工作上下文");
        if (shouldReplaceComponent)
        {
            Log(session_, "准备替换装配组件引用");
            std::vector<Assemblies::Component*> replacedComponents;
            if (!copiedReplacementPart ||
                !ReplaceCopiedComponentPart(session_, committedComponentJournalId, originalPartPath, replacementPartPath, committedText, &replacedComponents, &replaceError))
            {
                Log(session_, std::string("替换装配组件引用失败: ") + replaceError);
                ShowError("刻字", replaceError);
            }
            else
            {
                Log(session_, "替换装配组件引用完成");
                if (hideEngravedText)
                {
                    HideComponents(replacedComponents);
                }
                componentReplacedInApply_ = true;
                ClearManualSelectionAfterReplace();
            }
        }
        else if (hideEngravedText && committedComponent != nullptr)
        {
            HideComponents(std::vector<Assemblies::Component*>{committedComponent});
        }
        Log(session_, "ApplyEngraving完成");
    }

private:
    Session* session_ = nullptr;
    UI* ui_ = nullptr;
    BlockDialog* dialog_ = nullptr;
    Features::CustomFeatureClassManager* customFeatureManager_ = nullptr;
    Features::CustomFeature* editedFeature_ = nullptr;
    Features::EditWithRollbackManager* editRollbackManager_ = nullptr;
    Session::UndoMarkId editRollbackMark_ = static_cast<Session::UndoMarkId>(0);

    Enumeration* mode_ = nullptr;
    BlockStyler::SelectObject* manualFace_ = nullptr;
    SpecifyOrientation* orientation_ = nullptr;
    Button* rotate90Button_ = nullptr;
    Toggle* centerLongSide_ = nullptr;
    Toggle* centerShortSide_ = nullptr;
    Toggle* xLongSide_ = nullptr;
    Toggle* xShortSide_ = nullptr;
    StringBlock* ruleValue_ = nullptr;
    StringBlock* textValue_ = nullptr;
    Button* appendBodyName_ = nullptr;
    Button* appendFileName_ = nullptr;
    Button* appendSerial_ = nullptr;
    Button* appendAttribute_ = nullptr;
    StringBlock* fontName_ = nullptr;
    DoubleBlock* textHeight_ = nullptr;
    DoubleBlock* depth_ = nullptr;
    ObjectColorPicker* textColor_ = nullptr;
    Enumeration* boundary_ = nullptr;
    DoubleBlock* boundaryDepth_ = nullptr;
    ObjectColorPicker* boundaryColor_ = nullptr;
    DoubleBlock* margin_ = nullptr;
    DoubleBlock* wScale_ = nullptr;
    Toggle* lockAspect_ = nullptr;
    DoubleBlock* shear_ = nullptr;
    IntegerBlock* textLayer_ = nullptr;
    Toggle* embossedText_ = nullptr;
    Toggle* verticalText_ = nullptr;
    DoubleBlock* vShapeWidth_ = nullptr;
    Toggle* renameComponentToText_ = nullptr;
    Toggle* engraveSameBodies_ = nullptr;
    Toggle* sameRandomColor_ = nullptr;
    Toggle* autoEngraveVisibleTubes_ = nullptr;
    Toggle* hideEngravedText_ = nullptr;
    Button* editConfig_ = nullptr;
    Tooling::InsertTextBuilder* previewBuilder_ = nullptr;
    NXObject* previewUdo_ = nullptr;
    std::vector<tag_t> modernPreviewCurveTags_;
    NXObject* lastBody_ = nullptr;
    Assemblies::Component* lastComponent_ = nullptr;
    Part* lastWorkPart_ = nullptr;
    std::string lastComponentJournalId_;
    std::string lastResolvedText_;
    std::string lastTextTemplate_;
    std::string ruleText_;
    std::string lastValidFontName_ = "Arial";
    std::string normalFontName_ = "Arial";
    bool isAssemblyContext_ = false;
    bool refreshingPreview_ = false;
    bool updatingResolvedText_ = false;
    bool updatingRuleInput_ = false;
    bool selectingSystemFont_ = false;
    bool applying_ = false;
    bool loadingEditedFeature_ = false;
    bool componentReplacedInApply_ = false;
    bool suppressPreviewUntilSelection_ = false;
    bool deferredEditedReplacement_ = false;
    bool executingDeferredEditedReplacement_ = false;
    UINT_PTR deferredTimerId_ = 0;
    Face* deferredReplacementFace_ = nullptr;
    Point3d deferredReplacementOrigin_ = Point3d(0.0, 0.0, 0.0);
    Matrix3x3 deferredReplacementMatrix_ = {};
    bool deferredPlacementCaptured_ = false;
    tag_t deferredReplacementAnchorTag_ = NULL_TAG;
    bool deferredDisplaySuppressed_ = false;
    TextSettings deferredReplacementSettings_ = {};
    std::string deferredReplacementText_;
    bool buildingModernCustomFeature_ = false;
    std::vector<tag_t> modernConstructionFeatureTags_;
    KeZiConfig config_;
};

extern "C" DllExport int ZhihuiKeZiBuildCustomFeature(void* eventPointer)
{
    if (gActiveKeZiDialog == nullptr || eventPointer == nullptr)
    {
        return 1;
    }
    return gActiveKeZiDialog->BuildModernCustomFeatureConstruction(
        static_cast<Features::CustomFeaturePreUpdateEvent*>(eventPointer));
}

extern "C" DllExport void ufusr(char* /*param*/, int* /*retcod*/, int /*param_len*/)
{
    Log(Session::GetSession(), "========== ufusr进入 ==========");
    if (!zhihui_license_guard::EnsureAuthorized(L"ZHIHUI.KEZI", L"KeZi"))
    {
        Log(Session::GetSession(), "授权失败，ufusr退出");
        return;
    }

    UfGuard uf;
    if (!uf.ok())
    {
        Log(Session::GetSession(), "UF_initialize失败");
        ShowError("KeZi Engrave Text", "UF_initialize failed.");
        return;
    }

    try
    {
        KeZiDialog* dialog = new KeZiDialog();
        dialog->Show();
        Log(Session::GetSession(), "ufusr: dialog.Show返回");
        if (!dialog->HasScheduledReplacement())
        {
            delete dialog;
        }
        else
        {
            Log(Session::GetSession(), "ufusr: 对话框对象保留至主线程延时替换完成");
        }
    }
    catch (const NXException& ex)
    {
        Log(Session::GetSession(), std::string("ufusr捕获NX异常: ") + ex.Message());
        ShowError("KeZi Engrave Text", ex.Message());
    }
    catch (const std::exception& ex)
    {
        Log(Session::GetSession(), std::string("ufusr捕获std异常: ") + ex.what());
        ShowError("KeZi Engrave Text", ex.what());
    }
    catch (...)
    {
        Log(Session::GetSession(), "ufusr捕获未知异常");
        ShowError("刻字", "刻字发生未知异常，详情请查看日志。");
    }
    Log(Session::GetSession(), "========== ufusr退出 ==========");
}

extern "C" DllExport int ufusr_ask_unload()
{
    return static_cast<int>(Session::LibraryUnloadOptionAtTermination);
}

extern "C" DllExport void ufusr_cleanup(void)
{
}


