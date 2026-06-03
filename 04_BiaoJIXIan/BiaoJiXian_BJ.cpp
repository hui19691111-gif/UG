//==============================================================================
// Basic framework for BiaoJiXian dialog
//==============================================================================

#include "BiaoJiXian_BJ.hpp"

#include <Windows.h>
#ifdef CreateDialog
#undef CreateDialog
#endif

#include <algorithm>
#include <array>
#include <cfloat>
#include <cctype>
#include <cmath>
#include <exception>
#include <filesystem>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include <NXOpen/BodyDumbRule.hxx>
#include <NXOpen/ColorManager.hxx>
#include <NXOpen/CurveGroupRule.hxx>
#include <NXOpen/Drafting_PreferencesBuilder.hxx>
#include <NXOpen/Drafting_SettingsManager.hxx>
#include <NXOpen/Direction.hxx>
#include <NXOpen/DirectionCollection.hxx>
#include <NXOpen/CurveDumbRule.hxx>
#include <NXOpen/DisplayableObject.hxx>
#include <NXOpen/Drawings_BaseView.hxx>
#include <NXOpen/Drawings_DraftingView.hxx>
#include <NXOpen/Drawings_DraftingViewCollection.hxx>
#include <NXOpen/Drawings_EditViewSettingsBuilder.hxx>
#include <NXOpen/Drawings_ViewStyleFPCurvesBuilder.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/EdgeDumbRule.hxx>
#include <NXOpen/Features_ExtrudeBuilder.hxx>
#include <NXOpen/Features_BaseFeatureCollection.hxx>
#include <NXOpen/Features_EditWithRollbackManager.hxx>
#include <NXOpen/Features_FlatPattern.hxx>
#include <NXOpen/Features_MoveObjectBuilder.hxx>
#include <NXOpen/Features_ProjectCurveBuilder.hxx>
#include <NXOpen/Features_SheetMetal_FlatPatternBuilder.hxx>
#include <NXOpen/Features_SheetMetal_SheetmetalManager.hxx>
#include <NXOpen/Features_SketchFeature.hxx>
#include <NXOpen/FaceDumbRule.hxx>
#include <NXOpen/FaceTangentRule.hxx>
#include <NXOpen/Features_CurveFeatureCollection.hxx>
#include <NXOpen/Features_FeatureCollection.hxx>
#include <NXOpen/Features_ShadowCurve.hxx>
#include <NXOpen/Features_ShadowCurveBuilder.hxx>
#include <NXOpen/GeometricUtilities_BooleanOperation.hxx>
#include <NXOpen/GeometricUtilities_Extend.hxx>
#include <NXOpen/GeometricUtilities_FeatureOffset.hxx>
#include <NXOpen/GeometricUtilities_FeatureOptions.hxx>
#include <NXOpen/GeometricUtilities_Limits.hxx>
#include <NXOpen/GeometricUtilities_MultiDraft.hxx>
#include <NXOpen/GeometricUtilities_SimpleDraft.hxx>
#include <NXOpen/GeometricUtilities_SmartVolumeProfileBuilder.hxx>
#include <NXOpen/GeometricUtilities_ModlMotion.hxx>
#include <NXOpen/GeometricUtilities_CurveFitData.hxx>
#include <NXOpen/GeometricUtilities_CurveSettings.hxx>
#include <NXOpen/Group.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/Plane.hxx>
#include <NXOpen/PlaneCollection.hxx>
#include <NXOpen/Preferences_PartPreferences.hxx>
#include <NXOpen/Preferences_SheetMetalPreferencesManager.hxx>
#include <NXOpen/Preferences_SheetMetalPreferencesBuilder.hxx>
#include <NXOpen/Point.hxx>
#include <NXOpen/PointCollection.hxx>
#include <NXOpen/Scalar.hxx>
#include <NXOpen/ScalarCollection.hxx>
#include <NXOpen/ScCollectorCollection.hxx>
#include <NXOpen/ScRuleFactory.hxx>
#include <NXOpen/Section.hxx>
#include <NXOpen/ScCollector.hxx>
#include <NXOpen/SectionCollection.hxx>
#include <NXOpen/SelectISurface.hxx>
#include <NXOpen/SelectNXObjectList.hxx>
#include <NXOpen/SelectionIntentRule.hxx>
#include <NXOpen/SelectionIntentRuleOptions.hxx>
#include <NXOpen/Sketch.hxx>
#include <NXOpen/SketchCollection.hxx>
#include <NXOpen/SketchInPlaceBuilder.hxx>
#include <uf.h>
#include <uf_curve.h>
#include <uf_disp.h>
#include <uf_eval.h>
#include <uf_group.h>
#include <uf_modl.h>
#include <uf_modl_curves.h>
#include <uf_modl_utilities.h>
#include <uf_obj.h>
#include <uf_smd.h>
#include <uf_ui.h>

using namespace NXOpen;
using namespace NXOpen::BlockStyler;

namespace
{
// 相贴判断容差。
// 体与体、面与面最小距离小于这个值时，认为可以视为相贴。
const double kContactTolerance = 0.01;
const bool kDebugStopAfterProjection = false;
const bool kDebugStopAfterPreSegment = false;
const bool kDebugStopAfterSegmentation = false;

// 运行期缓存：targetBodyTag -> flatPatternFeatureTag
// 用于避免同一次执行里反复查找同一个大体的展开图样特征。
std::unordered_map<tag_t, tag_t> gFlatPatternFeatureCache;

struct ContactMatch
{
    // 当前命中的相贴大体
    tag_t targetBody = NULL_TAG;
    // 选择体上的相贴面
    tag_t sourceFace = NULL_TAG;
    // 大体上的相贴面
    tag_t targetFace = NULL_TAG;
    // 面与面之间的最小距离
    double distance = DBL_MAX;
    // 相贴点对，后面会用于确定阴影/投影方向
    double sourcePoint[3] = {0.0, 0.0, 0.0};
    double targetPoint[3] = {0.0, 0.0, 0.0};
};

struct CurveEndData
{
    // 曲线 tag
    tag_t curveTag = NULL_TAG;
    // 起点、终点
    double start[3] = {0.0, 0.0, 0.0};
    double end[3] = {0.0, 0.0, 0.0};
    // 曲线包围盒
    double minCorner[3] = {DBL_MAX, DBL_MAX, DBL_MAX};
    double maxCorner[3] = {-DBL_MAX, -DBL_MAX, -DBL_MAX};
};

struct SegmentParameters
{
    // 每段标记线长度
    double segmentLength = 40.0;
    // 标记线之间允许的最大间隔
    double maxMarkSpacing = 0.0;
    // 封闭曲线处理规则
    int closedCurveRule = 0;
};

struct FaceSelectionContext
{
    // 候选面 tag 列表
    std::vector<tag_t> faceTags;
    // 候选面 tag 集合，便于快速判断用户选中的面是不是有效候选
    std::unordered_set<tag_t> faceTagSet;
};

struct PreparedCurveSet
{
    std::vector<tag_t> curves;
    std::vector<tag_t> retiredCurves;
};

struct PreparedCurveGroupSet
{
    std::vector<std::vector<tag_t> > groups;
    std::vector<tag_t> curves;
    std::vector<tag_t> retiredCurves;
};

std::vector<tag_t> FlattenCurveGroups(const std::vector<std::vector<tag_t> >& groups)
{
    std::vector<tag_t> curves;
    for (std::size_t groupIndex = 0; groupIndex < groups.size(); ++groupIndex)
    {
        curves.insert(curves.end(), groups[groupIndex].begin(), groups[groupIndex].end());
    }
    return curves;
}

std::string ResolveDialogFilePath(const char* fileName);
std::string ToUtf8(const wchar_t* text);
std::vector<tag_t> AskPlanarFaces(tag_t bodyTag);
double DistanceSquared(const double lhs[3], const double rhs[3]);
std::vector<tag_t> AskPeripheralLoopEdges(tag_t faceTag);
std::vector<tag_t> CreateCurvesFromEdges(const std::vector<tag_t>& edgeTags);
PreparedCurveSet ApplyPreSegmentRules(Part* workPart, const std::vector<tag_t>& curveTags, const ContactMatch& match);
PreparedCurveGroupSet ApplyPreSegmentRulesToGroups(Part* workPart, const std::vector<tag_t>& curveTags, const ContactMatch& match);
PreparedCurveGroupSet ApplyPreSegmentRulesToGroups(
    Part* workPart,
    const std::vector<std::vector<tag_t> >& projectedGroups,
    const ContactMatch& match);
void DeleteObjects(const std::vector<tag_t>& objectTags);

struct CurveLengthInfo
{
    // 曲线对象 tag
    tag_t curveTag = NULL_TAG;
    // 曲线长度
    double length = 0.0;
    // 是否可以参与分段
    bool canSegment = true;
};

struct CurveSegmentRange
{
    // 沿母线起点量起的距离
    double startDistance = 0.0;
    // 沿母线起点量到的结束距离
    double endDistance = 0.0;
};

enum class MarkCurveFont
{
    // 这些枚举值和 LineFont 控件、UF、NXOpen 制图线型之间会做映射。
    NoChange = 0,
    Original = 1,
    Invisible = 2,
    Solid = 3,
    Dashed = 4,
    Phantom = 5,
    Centerline = 6,
    Dotted = 7,
    LongDashed = 8,
    DottedDashed = 9,
    Eight = 10,
    Nine = 11,
    Ten = 12,
    Eleven = 13
};

const double kCurveLengthTolerance = 1.0e-6;

std::string ToUpperAscii(const std::string& text)
{
    std::string result = text;
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char value)
    {
        return static_cast<char>(std::toupper(value));
    });
    return result;
}

std::string ToMarkCurveFontName(MarkCurveFont font)
{
    switch (font)
    {
    case MarkCurveFont::NoChange: return "NoChange";
    case MarkCurveFont::Original: return "Original";
    case MarkCurveFont::Invisible: return "Invisible";
    case MarkCurveFont::Solid: return "Solid";
    case MarkCurveFont::Dashed: return "Dashed";
    case MarkCurveFont::Phantom: return "Phantom";
    case MarkCurveFont::Centerline: return "Centerline";
    case MarkCurveFont::Dotted: return "Dotted";
    case MarkCurveFont::LongDashed: return "LongDashed";
    case MarkCurveFont::DottedDashed: return "DottedDashed";
    case MarkCurveFont::Eight: return "Eight";
    case MarkCurveFont::Nine: return "Nine";
    case MarkCurveFont::Ten: return "Ten";
    case MarkCurveFont::Eleven: return "Eleven";
    default: return "Unknown";
    }
}

int ToUfFont(MarkCurveFont font)
{
    return static_cast<int>(font);
}

NXOpen::DisplayableObject::ObjectFont ToNxObjectFont(MarkCurveFont font)
{
    return static_cast<NXOpen::DisplayableObject::ObjectFont>(static_cast<int>(font));
}

NXOpen::Preferences::Font ToNxDraftingFont(MarkCurveFont font)
{
    switch (font)
    {
    case MarkCurveFont::Original: return NXOpen::Preferences::FontOriginal;
    case MarkCurveFont::Invisible: return NXOpen::Preferences::FontInvisible;
    case MarkCurveFont::Solid: return NXOpen::Preferences::FontSolid;
    case MarkCurveFont::Dashed: return NXOpen::Preferences::FontDashed;
    case MarkCurveFont::Phantom: return NXOpen::Preferences::FontPhantom;
    case MarkCurveFont::Centerline: return NXOpen::Preferences::FontCenterline;
    case MarkCurveFont::Dotted: return NXOpen::Preferences::FontDotted;
    case MarkCurveFont::LongDashed: return NXOpen::Preferences::FontLongDashed;
    case MarkCurveFont::DottedDashed: return NXOpen::Preferences::FontDottedDashed;
    case MarkCurveFont::Eight: return NXOpen::Preferences::FontEight;
    case MarkCurveFont::Nine: return NXOpen::Preferences::FontNine;
    case MarkCurveFont::Ten: return NXOpen::Preferences::FontTen;
    case MarkCurveFont::Eleven: return NXOpen::Preferences::FontEleven;
    case MarkCurveFont::NoChange:
    default:
        return NXOpen::Preferences::FontSolid;
    }
}

double Dot(const double lhs[3], const double rhs[3])
{
    return lhs[0] * rhs[0] + lhs[1] * rhs[1] + lhs[2] * rhs[2];
}

bool Normalize(double vector[3])
{
    const double length = std::sqrt(Dot(vector, vector));
    if (length <= 1.0e-12)
    {
        return false;
    }

    vector[0] /= length;
    vector[1] /= length;
    vector[2] /= length;
    return true;
}

std::string ToUtf8(const wchar_t* text)
{
    if (text == NULL)
    {
        return std::string();
    }

    const int size = WideCharToMultiByte(CP_UTF8, 0, text, -1, NULL, 0, NULL, NULL);
    if (size <= 1)
    {
        return std::string();
    }

    std::string utf8(static_cast<std::size_t>(size - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, &utf8[0], size, NULL, NULL);
    return utf8;
}

double AskCurveLength(tag_t curveTag)
{
    double length = 0.0;
    if (UF_CURVE_ask_arc_length(curveTag, 0.0, 1.0, UF_MODL_UNITS_PART, &length) != 0)
    {
        return 0.0;
    }

    return length;
}

bool AskCurvePointAtDistance(tag_t curveTag, double distanceFromStart, double point[3])
{
    UF_EVAL_p_t evaluator = NULL;
    if (UF_EVAL_initialize(curveTag, &evaluator) != 0 || evaluator == NULL)
    {
        return false;
    }

    double limits[2] = {0.0, 0.0};
    if (UF_EVAL_ask_limits(evaluator, limits) != 0)
    {
        UF_EVAL_free(evaluator);
        return false;
    }

    double derivatives[9] = {0.0};
    double startPoint[3] = {0.0, 0.0, 0.0};
    if (UF_EVAL_evaluate(evaluator, 0, limits[0], startPoint, derivatives) != 0)
    {
        UF_EVAL_free(evaluator);
        return false;
    }

    if (distanceFromStart <= kCurveLengthTolerance)
    {
        for (int axis = 0; axis < 3; ++axis)
        {
            point[axis] = startPoint[axis];
        }
        UF_EVAL_free(evaluator);
        return true;
    }

    double fullLength = 0.0;
    if (UF_CURVE_ask_arc_length(curveTag, 0.0, 1.0, UF_MODL_UNITS_PART, &fullLength) == 0 &&
        fullLength > kCurveLengthTolerance &&
        distanceFromStart >= fullLength - kCurveLengthTolerance)
    {
        double endPoint[3] = {0.0, 0.0, 0.0};
        if (UF_EVAL_evaluate(evaluator, 0, limits[1], endPoint, derivatives) == 0)
        {
            for (int axis = 0; axis < 3; ++axis)
            {
                point[axis] = endPoint[axis];
            }
            UF_EVAL_free(evaluator);
            return true;
        }
    }

    UF_EVAL_free(evaluator);

    double parameter = 0.0;
    return UF_MODL_ask_point_along_curve_2(
        startPoint,
        curveTag,
        std::max(0.0, distanceFromStart),
        1,
        0.001,
        point,
        &parameter) == 0;
}

tag_t CreateCurveSegmentByDistances(tag_t curveTag, double startDistance, double endDistance)
{
    const double fullLength = AskCurveLength(curveTag);
    if (fullLength <= kCurveLengthTolerance)
    {
        return NULL_TAG;
    }

    startDistance = std::max(0.0, std::min(startDistance, fullLength));
    endDistance = std::max(0.0, std::min(endDistance, fullLength));
    if (endDistance - startDistance <= kCurveLengthTolerance)
    {
        return NULL_TAG;
    }

    UF_EVAL_p_t evaluator = NULL;
    if (UF_EVAL_initialize(curveTag, &evaluator) != 0 || evaluator == NULL)
    {
        return NULL_TAG;
    }

    logical isLine = false;
    if (UF_EVAL_is_line(evaluator, &isLine) == 0 && isLine)
    {
        UF_EVAL_free(evaluator);

        double startPoint[3] = {0.0, 0.0, 0.0};
        double endPoint[3] = {0.0, 0.0, 0.0};
        if (!AskCurvePointAtDistance(curveTag, startDistance, startPoint) ||
            !AskCurvePointAtDistance(curveTag, endDistance, endPoint))
        {
            return NULL_TAG;
        }

        UF_CURVE_line_t lineData;
        for (int axis = 0; axis < 3; ++axis)
        {
            lineData.start_point[axis] = startPoint[axis];
            lineData.end_point[axis] = endPoint[axis];
        }

        tag_t newLine = NULL_TAG;
        if (UF_CURVE_create_line(&lineData, &newLine) != 0)
        {
            return NULL_TAG;
        }
        return newLine;
    }

    logical isArc = false;
    if (UF_EVAL_is_arc(evaluator, &isArc) == 0 && isArc)
    {
        UF_EVAL_free(evaluator);

        const double midDistance = 0.5 * (startDistance + endDistance);
        double startPoint[3] = {0.0, 0.0, 0.0};
        double midPoint[3] = {0.0, 0.0, 0.0};
        double endPoint[3] = {0.0, 0.0, 0.0};
        if (!AskCurvePointAtDistance(curveTag, startDistance, startPoint) ||
            !AskCurvePointAtDistance(curveTag, midDistance, midPoint) ||
            !AskCurvePointAtDistance(curveTag, endDistance, endPoint))
        {
            return NULL_TAG;
        }

        tag_t newArc = NULL_TAG;
        if (UF_CURVE_create_arc_thru_3pts(1, startPoint, midPoint, endPoint, &newArc) != 0)
        {
            return NULL_TAG;
        }
        return newArc;
    }

    UF_EVAL_free(evaluator);

    const int sampleCount = 7;
    UF_CURVE_pt_slope_crvatr_t pointData[sampleCount];
    for (int index = 0; index < sampleCount; ++index)
    {
        const double ratio = static_cast<double>(index) / static_cast<double>(sampleCount - 1);
        const double distance = startDistance + (endDistance - startDistance) * ratio;
        double samplePoint[3] = {0.0, 0.0, 0.0};
        if (!AskCurvePointAtDistance(curveTag, distance, samplePoint))
        {
            return NULL_TAG;
        }

        for (int axis = 0; axis < 3; ++axis)
        {
            pointData[index].point[axis] = samplePoint[axis];
            pointData[index].slope[axis] = 0.0;
            pointData[index].crvatr[axis] = 0.0;
        }
        pointData[index].slope_type = UF_CURVE_SLOPE_NONE;
        pointData[index].crvatr_type = UF_CURVE_CRVATR_NONE;
    }

    tag_t splineTag = NULL_TAG;
    if (UF_CURVE_create_spline_thru_pts(3, 0, sampleCount, pointData, NULL, 0, &splineTag) != 0)
    {
        return NULL_TAG;
    }

    return splineTag;
}

const double kMinimumMarkGap = 2.0;
const double kFullCircleMarkGap = 1.0;
const double kPi = 3.141592653589793238462643383279502884;

double AskSegmentBreakThreshold(const SegmentParameters& parameters)
{
    const double segmentLength = std::max(kCurveLengthTolerance, parameters.segmentLength);
    return segmentLength * 2.0 + kMinimumMarkGap;
}

bool ShouldSegmentCurve(double curveLength, const SegmentParameters& parameters)
{
    return curveLength + kCurveLengthTolerance >= AskSegmentBreakThreshold(parameters);
}

int AskMarkSegmentCount(double curveLength, const SegmentParameters& parameters)
{
    if (!ShouldSegmentCurve(curveLength, parameters))
    {
        return 0;
    }

    const double segmentLength = std::min(
        std::max(kCurveLengthTolerance, parameters.segmentLength),
        curveLength);
    const double minimumGap = kMinimumMarkGap;
    const double maxGap = std::max(minimumGap, parameters.maxMarkSpacing);

    const int maxSegmentCount = std::max(
        2,
        static_cast<int>(std::floor((curveLength + minimumGap) / (segmentLength + minimumGap))));
    for (int count = 2; count <= maxSegmentCount; ++count)
    {
        const double totalGap = curveLength - segmentLength * static_cast<double>(count);
        const double gapLength = totalGap / static_cast<double>(count - 1);
        if (gapLength <= maxGap + kCurveLengthTolerance)
        {
            return count;
        }
    }

    return maxSegmentCount;
}

std::vector<CurveSegmentRange> BuildMarkSegmentRanges(double curveLength, const SegmentParameters& parameters)
{
    std::vector<CurveSegmentRange> ranges;
    const int segmentCount = AskMarkSegmentCount(curveLength, parameters);
    if (segmentCount <= 0)
    {
        return ranges;
    }

    const double segmentLength = std::min(
        std::max(kCurveLengthTolerance, parameters.segmentLength),
        curveLength);
    if (segmentCount == 1)
    {
        ranges.push_back(CurveSegmentRange{0.0, segmentLength});
        return ranges;
    }

    const double startStep = (curveLength - segmentLength) / static_cast<double>(segmentCount - 1);
    for (int segmentIndex = 0; segmentIndex < segmentCount; ++segmentIndex)
    {
        const double startDistance = startStep * static_cast<double>(segmentIndex);
        ranges.push_back(CurveSegmentRange{
            startDistance,
            std::min(curveLength, startDistance + segmentLength)});
    }
    return ranges;
}

bool IsSplittableFullCircleCurve(tag_t curveTag, double curveLength)
{
    if (curveTag == NULL_TAG || curveLength <= 2.0 * kFullCircleMarkGap + kCurveLengthTolerance)
    {
        return false;
    }

    int type = 0;
    int subtype = 0;
    if (UF_OBJ_ask_type_and_subtype(curveTag, &type, &subtype) != 0 || type != UF_circle_type)
    {
        return false;
    }

    UF_CURVE_arc_t arcData{};
    if (UF_CURVE_ask_arc_data(curveTag, &arcData) != 0 || arcData.radius <= kCurveLengthTolerance)
    {
        return false;
    }

    const double fullCircleLength = 2.0 * kPi * arcData.radius;
    const double tolerance = std::max(0.01, fullCircleLength * 1.0e-4);
    return std::fabs(curveLength - fullCircleLength) <= tolerance;
}

bool SplitFullCircleIntoTwoMarkSegments(tag_t curveTag, double curveLength, std::vector<tag_t>& segmentCurves)
{
    segmentCurves.clear();
    if (!IsSplittableFullCircleCurve(curveTag, curveLength))
    {
        return false;
    }

    const double segmentLength = 0.5 * (curveLength - 2.0 * kFullCircleMarkGap);
    if (segmentLength <= kCurveLengthTolerance)
    {
        return false;
    }

    tag_t firstSegment = CreateCurveSegmentByDistances(curveTag, 0.0, segmentLength);
    tag_t secondSegment = CreateCurveSegmentByDistances(
        curveTag,
        segmentLength + kFullCircleMarkGap,
        segmentLength + kFullCircleMarkGap + segmentLength);

    if (firstSegment == NULL_TAG || secondSegment == NULL_TAG)
    {
        DeleteObjects(std::vector<tag_t>{firstSegment, secondSegment});
        return false;
    }

    segmentCurves.push_back(firstSegment);
    segmentCurves.push_back(secondSegment);
    return true;
}

int GetCurveColorValue(ObjectColorPicker* colorPicker)
{
    if (colorPicker == NULL)
    {
        return 36;
    }

    const std::vector<int> values = colorPicker->GetValue();
    if (values.empty())
    {
        return 36;
    }

    return values[0];
}

MarkCurveFont ParseCurveFontValue(LineFont* lineFontBlock)
{
    if (lineFontBlock == NULL)
    {
        return MarkCurveFont::Solid;
    }

    PropertyList* properties = lineFontBlock->GetProperties();
    const int value = properties->GetEnum("Value");
    delete properties;

    switch (value)
    {
    case 0: return MarkCurveFont::NoChange;
    case 1: return MarkCurveFont::Original;
    case 2: return MarkCurveFont::Invisible;
    case 3: return MarkCurveFont::Solid;
    case 4: return MarkCurveFont::Dashed;
    case 5: return MarkCurveFont::Phantom;
    case 6: return MarkCurveFont::Centerline;
    case 7: return MarkCurveFont::Dotted;
    case 8: return MarkCurveFont::LongDashed;
    case 9: return MarkCurveFont::DottedDashed;
    case 10: return MarkCurveFont::Eight;
    case 11: return MarkCurveFont::Nine;
    case 12: return MarkCurveFont::Ten;
    case 13: return MarkCurveFont::Eleven;
    default: return MarkCurveFont::Solid;
    }
}

void ApplyCurveColorAndFont(const std::vector<tag_t>& curveTags, int color, MarkCurveFont font)
{
    const int ufFont = ToUfFont(font);
    for (tag_t curveTag : curveTags)
    {
        if (curveTag == NULL_TAG || UF_OBJ_ask_status(curveTag) != UF_OBJ_ALIVE)
        {
            continue;
        }

        UF_OBJ_set_color(curveTag, color);
        if (ufFont > 0)
        {
            UF_OBJ_set_font(curveTag, ufFont);
        }
        UF_OBJ_set_blank_status(curveTag, UF_OBJ_NOT_BLANKED);
    }
}

int AskCurveLayerModeOption(Enumeration* modeBlock)
{
    if (modeBlock == NULL)
    {
        return -1;
    }

    PropertyList* properties = modeBlock->GetProperties();
    const int value = properties->GetEnum("Value");
    delete properties;
    return value;
}

int AskCustomCurveLayer(IntegerBlock* layerBlock)
{
    if (layerBlock == NULL)
    {
        return 0;
    }

    PropertyList* properties = layerBlock->GetProperties();
    const int value = properties->GetInteger("Value");
    delete properties;
    return value;
}

int ResolveCurveOutputLayer(Enumeration* modeBlock, IntegerBlock* layerBlock, tag_t sourceBodyTag)
{
    const int layerMode = AskCurveLayerModeOption(modeBlock);
    const int layerValue = AskCustomCurveLayer(layerBlock);

    // 0 = follow source body, 1 = user defined layer
    if (layerMode == 1)
    {
        return layerValue;
    }

    if (layerMode == 0 && sourceBodyTag != NULL_TAG && UF_OBJ_ask_status(sourceBodyTag) == UF_OBJ_ALIVE)
    {
        Body* sourceBody = dynamic_cast<Body*>(NXObjectManager::Get(sourceBodyTag));
        if (sourceBody != NULL)
        {
            const int sourceLayer = sourceBody->Layer();
            if (sourceLayer > 0)
            {
                return sourceLayer;
            }
        }
    }

    return 0;
}

void ApplyCurveLayer(const std::vector<tag_t>& curveTags, int layer)
{
    if (layer <= 0)
    {
        return;
    }

    for (tag_t curveTag : curveTags)
    {
        if (curveTag == NULL_TAG || UF_OBJ_ask_status(curveTag) != UF_OBJ_ALIVE)
        {
            continue;
        }

        UF_OBJ_set_layer(curveTag, layer);
    }
}

bool IsTrueFlatPatternFeature(Features::Feature* feature)
{
    if (feature == NULL)
    {
        return false;
    }

    const std::string featureType = ToUpperAscii(feature->FeatureType().GetText());
    return featureType.find("FLAT_PATTERN") != std::string::npos ||
           featureType == "FLAT PATTERN" ||
           featureType == "FLAT_PATTERN";
}

bool FeatureContainsEntityTag(Features::Feature* feature, tag_t entityTag)
{
    if (feature == NULL || entityTag == NULL_TAG)
    {
        return false;
    }

    const std::vector<NXObject*> entities = feature->GetEntities();
    for (NXObject* entity : entities)
    {
        if (entity != NULL && entity->Tag() == entityTag)
        {
            return true;
        }
    }

    return false;
}

std::string BuildFlatPatternCandidateDebug(Features::Feature* feature)
{
    if (feature == NULL)
    {
        return "";
    }

    std::ostringstream stream;
    stream << "\n      candidateFeatureTag=" << feature->Tag()
           << ", type=" << feature->FeatureType().GetText();

    const std::vector<Body*> bodies = feature->GetBodies();
    for (std::size_t i = 0; i < bodies.size(); ++i)
    {
        Body* body = bodies[i];
        if (body == NULL || body->Tag() == NULL_TAG)
        {
            continue;
        }

        stream << "\n        body[" << i << "]=" << body->Tag();
        const std::vector<tag_t> planarFaces = AskPlanarFaces(body->Tag());
        tag_t bestFace = NULL_TAG;
        double bestArea = -1.0;
        for (tag_t faceTag : planarFaces)
        {
            double box[6] = {0.0};
            if (UF_MODL_ask_bounding_box(faceTag, box) == 0)
            {
                const double areaScore =
                    std::abs((box[3] - box[0]) * (box[4] - box[1])) +
                    std::abs((box[4] - box[1]) * (box[5] - box[2])) +
                    std::abs((box[3] - box[0]) * (box[5] - box[2]));
                if (areaScore > bestArea)
                {
                    bestArea = areaScore;
                    bestFace = faceTag;
                }
            }
        }
        if (bestFace != NULL_TAG)
        {
            stream << ", bestPlanarFace=" << bestFace;
        }
    }

    return stream.str();
}

Features::Feature* FindFlatPatternByBodyFromAllFeatures(Part* workPart, tag_t bodyTag, std::ostringstream& debug, const char* label)
{
    if (workPart == NULL || bodyTag == NULL_TAG)
    {
        return NULL;
    }

    std::vector<Features::Feature*> allFeatures = workPart->Features()->GetFeatures();
    debug << ", " << label << "AllFeatureCount=" << allFeatures.size();

    for (Features::Feature* feature : allFeatures)
    {
        if (!IsTrueFlatPatternFeature(feature))
        {
            continue;
        }

        debug << "\n      allFeatureFlatPatternTag=" << feature->Tag()
              << ", type=" << feature->FeatureType().GetText();

        const std::vector<Body*> bodies = feature->GetBodies();
        for (std::size_t index = 0; index < bodies.size(); ++index)
        {
            Body* body = bodies[index];
            if (body == NULL || body->Tag() == NULL_TAG)
            {
                continue;
            }

            debug << ", fpBody[" << index << "]=" << body->Tag();
            if (body->Tag() == bodyTag)
            {
                debug << ", allFeatureMatched=1";
                return feature;
            }
        }
    }

    return NULL;
}

Features::Feature* FindFlatPatternFromBodyFeatureGraph(Part* workPart, tag_t bodyTag, std::ostringstream& debug, const char* label)
{
    if (workPart == NULL || bodyTag == NULL_TAG)
    {
        return NULL;
    }

    Body* body = dynamic_cast<Body*>(NXObjectManager::Get(bodyTag));
    if (body == NULL)
    {
        debug << ", " << label << "=bodyObjectNULL";
        return NULL;
    }

    std::vector<Features::Feature*> seedFeatures = body->GetFeatures();
    debug << ", " << label << "FeatureSeeds=" << seedFeatures.size();

    std::vector<Features::Feature*> queue = seedFeatures;
    std::vector<tag_t> visited;
    std::size_t cursor = 0;
    while (cursor < queue.size())
    {
        Features::Feature* feature = queue[cursor++];
        if (feature == NULL)
        {
            continue;
        }

        if (std::find(visited.begin(), visited.end(), feature->Tag()) != visited.end())
        {
            continue;
        }
        visited.push_back(feature->Tag());

        debug << "\n      graphFeatureTag=" << feature->Tag()
              << ", type=" << feature->FeatureType().GetText();

        if (IsTrueFlatPatternFeature(feature))
        {
            debug << ", graphMatched=1";
            return feature;
        }

        const std::vector<Features::Feature*> parents = feature->GetParents();
        for (Features::Feature* parent : parents)
        {
            if (parent != NULL)
            {
                queue.push_back(parent);
            }
        }

        const std::vector<Features::Feature*> children = feature->GetChildren();
        for (Features::Feature* child : children)
        {
            if (child != NULL)
            {
                queue.push_back(child);
            }
        }
    }

    return NULL;
}

Features::Feature* AskPreferredFlatPatternFeature(Part* workPart, tag_t targetBodyTag)
{
    if (workPart == NULL)
    {
        return NULL;
    }

    if (targetBodyTag != NULL_TAG)
    {
        const std::unordered_map<tag_t, tag_t>::const_iterator cacheIt = gFlatPatternFeatureCache.find(targetBodyTag);
        if (cacheIt != gFlatPatternFeatureCache.end())
        {
            return cacheIt->second != NULL_TAG
                ? dynamic_cast<Features::Feature*>(NXObjectManager::Get(cacheIt->second))
                : NULL;
        }

        std::ostringstream debug;
        Features::Feature* allFeatureMatch =
            FindFlatPatternByBodyFromAllFeatures(workPart, targetBodyTag, debug, "targetBody");
        if (allFeatureMatch != NULL)
        {
            gFlatPatternFeatureCache[targetBodyTag] = allFeatureMatch->Tag();
            return allFeatureMatch;
        }

        tag_t targetFlatPatternTag = NULL_TAG;
        UF_SMD_ask_flat_pattern(targetBodyTag, &targetFlatPatternTag);
        if (targetFlatPatternTag != NULL_TAG)
        {
            Features::Feature* directTargetFeature = dynamic_cast<Features::Feature*>(NXObjectManager::Get(targetFlatPatternTag));
            if (IsTrueFlatPatternFeature(directTargetFeature))
            {
                gFlatPatternFeatureCache[targetBodyTag] = directTargetFeature->Tag();
                return directTargetFeature;
            }
        }

        Features::Feature* graphFeature =
            FindFlatPatternFromBodyFeatureGraph(workPart, targetBodyTag, debug, "targetBody");
        if (graphFeature != NULL)
        {
            gFlatPatternFeatureCache[targetBodyTag] = graphFeature->Tag();
            return graphFeature;
        }

        gFlatPatternFeatureCache[targetBodyTag] = NULL_TAG;
    }

    return NULL;
}

std::vector<tag_t> AppendCurvesToFlatPatternAddedGeometry(Part* workPart, tag_t sourceBodyTag, tag_t targetBodyTag, const std::vector<tag_t>& curveTags)
{
    static_cast<void>(sourceBodyTag);
    if (workPart == NULL || curveTags.empty())
    {
        return std::vector<tag_t>();
    }

    Features::Feature* flatPatternFeature = AskPreferredFlatPatternFeature(workPart, targetBodyTag);
    if (flatPatternFeature == NULL)
    {
        return std::vector<tag_t>();
    }

    Features::SheetMetal::SheetmetalManager* manager = workPart->Features()->SheetmetalManager();
    if (manager == NULL)
    {
        return std::vector<tag_t>();
    }

    Features::SheetMetal::FlatPatternBuilder* builder = manager->CreateFlatPatternBuilder(flatPatternFeature);
    if (builder == NULL)
    {
        return std::vector<tag_t>();
    }

    Session* session = Session::GetSession();
    Session::UndoMarkId markId = session != NULL
        ? session->SetUndoMark(Session::MarkVisibilityVisible, "Edit Flat Pattern")
        : static_cast<Session::UndoMarkId>(0);
    Features::EditWithRollbackManager* rollbackManager = NULL;
    Features::FlatPattern* flatPattern = dynamic_cast<Features::FlatPattern*>(flatPatternFeature);
    if (flatPattern != NULL)
    {
        rollbackManager = workPart->Features()->StartEditWithRollbackManager(flatPattern, markId);
    }

    builder->SetShowInteriorFeatureCurves(true);
    Section* section = builder->AddedGeometry();
    if (section == NULL)
    {
        builder->Destroy();
        return std::vector<tag_t>();
    }

    std::vector<IBaseCurve*> curves;
    for (tag_t curveTag : curveTags)
    {
        if (curveTag == NULL_TAG || UF_OBJ_ask_status(curveTag) != UF_OBJ_ALIVE)
        {
            continue;
        }

        int type = 0;
        int subtype = 0;
        UF_OBJ_ask_type_and_subtype(curveTag, &type, &subtype);
        if (type != UF_line_type &&
            type != UF_circle_type &&
            type != UF_conic_type &&
            type != UF_spline_type)
        {
            continue;
        }

        IBaseCurve* curve = dynamic_cast<IBaseCurve*>(NXObjectManager::Get(curveTag));
        if (curve != NULL)
        {
            curves.push_back(curve);
        }
    }

    if (curves.empty())
    {
        builder->Destroy();
        return std::vector<tag_t>();
    }

    try
    {
        section->SetAllowedEntityTypes(Section::AllowTypesCurvesAndPoints);
        section->AllowSelfIntersection(true);
        section->AllowDegenerateCurves(false);

        SelectionIntentRuleOptions* ruleOptions = workPart->ScRuleFactory()->CreateRuleOptions();
        ruleOptions->SetSelectedFromInactive(false);
        CurveDumbRule* curveRule = workPart->ScRuleFactory()->CreateRuleBaseCurveDumb(curves, ruleOptions);
        std::vector<SelectionIntentRule*> rules(1, static_cast<SelectionIntentRule*>(curveRule));

        section->AddToSection(
            rules,
            NULL,
            NULL,
            NULL,
            Point3d(0.0, 0.0, 0.0),
            Section::ModeCreate,
            false);

        builder->CommitFeature();
        delete ruleOptions;

        std::vector<NXObject*> outputCurves;
        section->GetOutputCurves(outputCurves);
        std::vector<tag_t> outputCurveTags;
        outputCurveTags.reserve(outputCurves.size());
        for (NXObject* outputCurve : outputCurves)
        {
            if (outputCurve != NULL && outputCurve->Tag() != NULL_TAG)
            {
                outputCurveTags.push_back(outputCurve->Tag());
            }
        }

        builder->Destroy();
        if (rollbackManager != NULL)
        {
            rollbackManager->UpdateFeature(false);
            rollbackManager->Stop();
            rollbackManager->Destroy();
        }
        return outputCurveTags;
    }
    catch (...)
    {
        builder->Destroy();
        if (rollbackManager != NULL)
        {
            rollbackManager->Stop();
            rollbackManager->Destroy();
        }
        return std::vector<tag_t>();
    }
}

std::vector<tag_t> AppendCurvesToFlatPatternAddedGeometry(Part* workPart, Features::Feature* flatPatternFeature, const std::vector<tag_t>& curveTags)
{
    if (workPart == NULL || curveTags.empty() || flatPatternFeature == NULL)
    {
        return std::vector<tag_t>();
    }

    Features::SheetMetal::SheetmetalManager* manager = workPart->Features()->SheetmetalManager();
    if (manager == NULL)
    {
        return std::vector<tag_t>();
    }

    Features::SheetMetal::FlatPatternBuilder* builder = manager->CreateFlatPatternBuilder(flatPatternFeature);
    if (builder == NULL)
    {
        return std::vector<tag_t>();
    }

    Session* session = Session::GetSession();
    Session::UndoMarkId markId = session != NULL
        ? session->SetUndoMark(Session::MarkVisibilityVisible, "Edit Flat Pattern")
        : static_cast<Session::UndoMarkId>(0);
    Features::EditWithRollbackManager* rollbackManager = NULL;
    Features::FlatPattern* flatPattern = dynamic_cast<Features::FlatPattern*>(flatPatternFeature);
    if (flatPattern != NULL)
    {
        rollbackManager = workPart->Features()->StartEditWithRollbackManager(flatPattern, markId);
    }

    builder->SetShowInteriorFeatureCurves(true);
    Section* section = builder->AddedGeometry();
    if (section == NULL)
    {
        builder->Destroy();
        return std::vector<tag_t>();
    }

    std::vector<IBaseCurve*> curves;
    for (tag_t curveTag : curveTags)
    {
        if (curveTag == NULL_TAG || UF_OBJ_ask_status(curveTag) != UF_OBJ_ALIVE)
        {
            continue;
        }

        int type = 0;
        int subtype = 0;
        UF_OBJ_ask_type_and_subtype(curveTag, &type, &subtype);
        if (type != UF_line_type &&
            type != UF_circle_type &&
            type != UF_conic_type &&
            type != UF_spline_type)
        {
            continue;
        }

        IBaseCurve* curve = dynamic_cast<IBaseCurve*>(NXObjectManager::Get(curveTag));
        if (curve != NULL)
        {
            curves.push_back(curve);
        }
    }

    if (curves.empty())
    {
        builder->Destroy();
        return std::vector<tag_t>();
    }

    try
    {
        section->SetAllowedEntityTypes(Section::AllowTypesCurvesAndPoints);
        section->AllowSelfIntersection(true);
        section->AllowDegenerateCurves(false);

        SelectionIntentRuleOptions* ruleOptions = workPart->ScRuleFactory()->CreateRuleOptions();
        ruleOptions->SetSelectedFromInactive(false);
        CurveDumbRule* curveRule = workPart->ScRuleFactory()->CreateRuleBaseCurveDumb(curves, ruleOptions);
        std::vector<SelectionIntentRule*> rules(1, static_cast<SelectionIntentRule*>(curveRule));

        section->AddToSection(
            rules,
            NULL,
            NULL,
            NULL,
            Point3d(0.0, 0.0, 0.0),
            Section::ModeCreate,
            false);

        builder->CommitFeature();
        delete ruleOptions;

        std::vector<NXObject*> outputCurves;
        section->GetOutputCurves(outputCurves);
        std::vector<tag_t> outputCurveTags;
        outputCurveTags.reserve(outputCurves.size());
        for (NXObject* outputCurve : outputCurves)
        {
            if (outputCurve != NULL && outputCurve->Tag() != NULL_TAG)
            {
                outputCurveTags.push_back(outputCurve->Tag());
            }
        }

        builder->Destroy();
        if (rollbackManager != NULL)
        {
            rollbackManager->UpdateFeature(false);
            rollbackManager->Stop();
            rollbackManager->Destroy();
        }
        return outputCurveTags;
    }
    catch (...)
    {
        builder->Destroy();
        if (rollbackManager != NULL)
        {
            rollbackManager->Stop();
            rollbackManager->Destroy();
        }
        return std::vector<tag_t>();
    }
}

void ApplyShallowGrooveFlatPatternDisplay(int color, MarkCurveFont font)
{
    Session* session = Session::GetSession();
    if (session == NULL || session->Parts() == NULL)
    {
        return;
    }

    Part* workPart = session->Parts()->Work();
    if (workPart == NULL)
    {
        return;
    }

    NXColor* nxColor = workPart->Colors()->Find(color);
    if (nxColor == NULL)
    {
        return;
    }

    try
    {
        NXOpen::Preferences::SheetMetalPreferencesManager* manager =
            workPart->Preferences()->SheetMetalPreferences();
        if (manager != NULL)
        {
            NXOpen::Preferences::SheetMetalPreferencesBuilder* builder =
                manager->CreateSheetMetalPreferencesBuilder();
            if (builder != NULL)
            {
                NXOpen::Preferences::SheetMetalPreferencesBuilder::FlatPatternObjectTypeDisplay displayData;
                displayData.Type = NXOpen::Preferences::SheetMetalPreferencesBuilder::FlatPatternObjectTypesInteriorFeatureCurves;
                displayData.IsEnabled = true;
                displayData.Color = nxColor;
                displayData.Layer = 1;
                displayData.Font = ToNxObjectFont(font);
                displayData.Width = NXOpen::DisplayableObject::ObjectWidthNormal;
                builder->SetFlatPatternObjectTypeDisplay(
                    NXOpen::Preferences::SheetMetalPreferencesBuilder::FlatPatternObjectTypesInteriorFeatureCurves,
                    displayData);
                builder->Commit();
                builder->Destroy();
            }
        }
    }
    catch (...)
    {
    }

    try
    {
        NXOpen::Drafting::PreferencesBuilder* preferencesBuilder =
            workPart->SettingsManager()->CreatePreferencesBuilder();
        if (preferencesBuilder != NULL)
        {
            NXOpen::Drawings::ViewStyleFPCurvesBuilder* curveBuilder =
                preferencesBuilder->ViewStyle()->GetViewStyleFPCurve(
                    NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeInteriorCurves);
            if (curveBuilder != NULL)
            {
                curveBuilder->SetState(true);
                curveBuilder->SetColor(nxColor);
                curveBuilder->SetFont(ToNxDraftingFont(font));
            }
            preferencesBuilder->Commit();
            preferencesBuilder->Destroy();
        }
    }
    catch (...)
    {
    }

    try
    {
        NXOpen::Drawings::DraftingViewCollection* draftingViews = workPart->DraftingViews();
        if (draftingViews != NULL)
        {
            for (NXOpen::Drawings::DraftingViewCollection::iterator it = draftingViews->begin();
                 it != draftingViews->end();
                 ++it)
            {
                NXOpen::Drawings::BaseView* baseView = dynamic_cast<NXOpen::Drawings::BaseView*>(*it);
                if (baseView == NULL)
                {
                    continue;
                }

                const std::string viewName = ToUpperAscii(baseView->Name().GetText());
                if (viewName.find("FLAT-PATTERN") == std::string::npos)
                {
                    continue;
                }

                std::vector<NXOpen::View*> targetViews(1, baseView);
                NXOpen::Drawings::EditViewSettingsBuilder* editBuilder =
                    workPart->SettingsManager()->CreateDrawingEditViewSettingsBuilder(targetViews);
                if (editBuilder == NULL)
                {
                    continue;
                }

                std::vector<NXOpen::Drafting::BaseEditSettingsBuilder*> editSettingsBuilders(1, editBuilder);
                workPart->SettingsManager()->ProcessForMultipleObjectsSettings(editSettingsBuilders);

                NXOpen::Drawings::ViewStyleFPCurvesBuilder* viewStyleFPCurvesBuilder8 =
                    editBuilder->ViewStyle()->GetViewStyleFPCurve(
                        NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeInteriorCurves);
                if (viewStyleFPCurvesBuilder8 != NULL)
                {
                    viewStyleFPCurvesBuilder8->SetState(true);
                    viewStyleFPCurvesBuilder8->SetColor(nxColor);
                    viewStyleFPCurvesBuilder8->SetFont(ToNxDraftingFont(font));
                    editBuilder->Commit();
                }

                editBuilder->Destroy();
            }

            draftingViews->UpdateViews(NXOpen::Drawings::DraftingViewCollection::ViewUpdateOptionAll);
        }
    }
    catch (...)
    {
    }
}

void ApplyCurveMarkFlatPatternDisplay(int color, MarkCurveFont font)
{
    Session* session = Session::GetSession();
    if (session == NULL || session->Parts() == NULL)
    {
        return;
    }

    Part* workPart = session->Parts()->Work();
    if (workPart == NULL)
    {
        return;
    }

    NXColor* nxColor = workPart->Colors()->Find(color);
    if (nxColor == NULL)
    {
        return;
    }

    try
    {
        NXOpen::Preferences::SheetMetalPreferencesManager* manager =
            workPart->Preferences()->SheetMetalPreferences();
        if (manager != NULL)
        {
            NXOpen::Preferences::SheetMetalPreferencesBuilder* builder =
                manager->CreateSheetMetalPreferencesBuilder();
            if (builder != NULL)
            {
                NXOpen::Preferences::SheetMetalPreferencesBuilder::FlatPatternObjectTypeDisplay topDisplay;
                topDisplay.Type = NXOpen::Preferences::SheetMetalPreferencesBuilder::FlatPatternObjectTypesAddedTopGeometry;
                topDisplay.IsEnabled = true;
                topDisplay.Color = nxColor;
                topDisplay.Layer = 1;
                topDisplay.Font = ToNxObjectFont(font);
                topDisplay.Width = NXOpen::DisplayableObject::ObjectWidthNormal;
                builder->SetFlatPatternObjectTypeDisplay(
                    NXOpen::Preferences::SheetMetalPreferencesBuilder::FlatPatternObjectTypesAddedTopGeometry,
                    topDisplay);

                NXOpen::Preferences::SheetMetalPreferencesBuilder::FlatPatternObjectTypeDisplay bottomDisplay;
                bottomDisplay.Type = NXOpen::Preferences::SheetMetalPreferencesBuilder::FlatPatternObjectTypesAddedBottomGeometry;
                bottomDisplay.IsEnabled = true;
                bottomDisplay.Color = nxColor;
                bottomDisplay.Layer = 1;
                bottomDisplay.Font = ToNxObjectFont(font);
                bottomDisplay.Width = NXOpen::DisplayableObject::ObjectWidthNormal;
                builder->SetFlatPatternObjectTypeDisplay(
                    NXOpen::Preferences::SheetMetalPreferencesBuilder::FlatPatternObjectTypesAddedBottomGeometry,
                    bottomDisplay);

                builder->Commit();
                builder->Destroy();
            }
        }
    }
    catch (...)
    {
    }

    try
    {
        NXOpen::Drafting::PreferencesBuilder* preferencesBuilder =
            workPart->SettingsManager()->CreatePreferencesBuilder();
        if (preferencesBuilder != NULL)
        {
            NXOpen::Drawings::ViewStyleFPCurvesBuilder* topCurveBuilder =
                preferencesBuilder->ViewStyle()->GetViewStyleFPCurve(
                    NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeLighteningHoleCenter);
            if (topCurveBuilder != NULL)
            {
                topCurveBuilder->SetState(true);
                topCurveBuilder->SetColor(nxColor);
                topCurveBuilder->SetFont(ToNxDraftingFont(font));
            }

            NXOpen::Drawings::ViewStyleFPCurvesBuilder* bottomCurveBuilder =
                preferencesBuilder->ViewStyle()->GetViewStyleFPCurve(
                    NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeJoggleLine);
            if (bottomCurveBuilder != NULL)
            {
                bottomCurveBuilder->SetState(true);
                bottomCurveBuilder->SetColor(nxColor);
                bottomCurveBuilder->SetFont(ToNxDraftingFont(font));
            }

            preferencesBuilder->Commit();
            preferencesBuilder->Destroy();
        }
    }
    catch (...)
    {
    }

    try
    {
        NXOpen::Drawings::DraftingViewCollection* draftingViews = workPart->DraftingViews();
        if (draftingViews != NULL)
        {
            for (NXOpen::Drawings::DraftingViewCollection::iterator it = draftingViews->begin();
                 it != draftingViews->end();
                 ++it)
            {
                NXOpen::Drawings::BaseView* baseView = dynamic_cast<NXOpen::Drawings::BaseView*>(*it);
                if (baseView == NULL)
                {
                    continue;
                }

                const std::string viewName = ToUpperAscii(baseView->Name().GetText());
                if (viewName.find("FLAT-PATTERN") == std::string::npos)
                {
                    continue;
                }

                std::vector<NXOpen::View*> targetViews(1, baseView);
                NXOpen::Drawings::EditViewSettingsBuilder* editBuilder =
                    workPart->SettingsManager()->CreateDrawingEditViewSettingsBuilder(targetViews);
                if (editBuilder == NULL)
                {
                    continue;
                }

                std::vector<NXOpen::Drafting::BaseEditSettingsBuilder*> editSettingsBuilders(1, editBuilder);
                workPart->SettingsManager()->ProcessForMultipleObjectsSettings(editSettingsBuilders);

                NXOpen::Drawings::ViewStyleFPCurvesBuilder* topViewBuilder =
                    editBuilder->ViewStyle()->GetViewStyleFPCurve(
                        NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeLighteningHoleCenter);
                if (topViewBuilder != NULL)
                {
                    topViewBuilder->SetState(true);
                    topViewBuilder->SetColor(nxColor);
                    topViewBuilder->SetFont(ToNxDraftingFont(font));
                }

                NXOpen::Drawings::ViewStyleFPCurvesBuilder* bottomViewBuilder =
                    editBuilder->ViewStyle()->GetViewStyleFPCurve(
                        NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeJoggleLine);
                if (bottomViewBuilder != NULL)
                {
                    bottomViewBuilder->SetState(true);
                    bottomViewBuilder->SetColor(nxColor);
                    bottomViewBuilder->SetFont(ToNxDraftingFont(font));
                }

                editBuilder->Commit();
                editBuilder->Destroy();
            }

            draftingViews->UpdateViews(NXOpen::Drawings::DraftingViewCollection::ViewUpdateOptionAll);
        }
    }
    catch (...)
    {
    }
}

void ApplyUnifiedMarkLineDisplay(int color, MarkCurveFont font)
{
    ApplyShallowGrooveFlatPatternDisplay(color, font);
    ApplyCurveMarkFlatPatternDisplay(color, font);
}

void DeleteObjects(const std::vector<tag_t>& objectTags)
{
    if (objectTags.empty())
    {
        return;
    }

    std::vector<tag_t> validTags;
    validTags.reserve(objectTags.size());
    for (std::size_t index = 0; index < objectTags.size(); ++index)
    {
        if (objectTags[index] == NULL_TAG)
        {
            continue;
        }

        if (UF_OBJ_ask_status(objectTags[index]) == UF_OBJ_ALIVE)
        {
            validTags.push_back(objectTags[index]);
        }
    }

    if (!validTags.empty())
    {
        std::sort(validTags.begin(), validTags.end());
        validTags.erase(std::unique(validTags.begin(), validTags.end()), validTags.end());
        UF_OBJ_delete_array_of_objects(static_cast<int>(validTags.size()), validTags.data(), NULL);
    }
}

std::vector<tag_t> BuildSegmentedMarkCurves(const std::vector<tag_t>& curveTags, const SegmentParameters& parameters);
std::vector<tag_t> BuildSegmentedMarkCurvesSingleGroup(const std::vector<tag_t>& curveTags, const SegmentParameters& parameters);
std::vector<std::vector<tag_t> > BuildSegmentedMarkCurveGroups(
    const std::vector<std::vector<tag_t> >& groups,
    const SegmentParameters& parameters);
std::vector<ContactMatch> AskUserToSelectContactMatches(const std::vector<ContactMatch>& matches);
std::vector<tag_t> ProjectSelectedEdgesToFace(Part* workPart, const std::vector<Edge*>& selectedEdges, const ContactMatch& match);

bool AskCurveEndpoints(tag_t curveTag, double startPoint[3], double endPoint[3])
{
    UF_EVAL_p_t evaluator = NULL;
    if (UF_EVAL_initialize(curveTag, &evaluator) != 0 || evaluator == NULL)
    {
        return false;
    }

    double limits[2] = {0.0, 0.0};
    if (UF_EVAL_ask_limits(evaluator, limits) != 0)
    {
        UF_EVAL_free(evaluator);
        return false;
    }

    double derivatives[9] = {0.0};
    const bool ok =
        UF_EVAL_evaluate(evaluator, 0, limits[0], startPoint, derivatives) == 0 &&
        UF_EVAL_evaluate(evaluator, 0, limits[1], endPoint, derivatives) == 0;

    UF_EVAL_free(evaluator);
    return ok;
}

bool IsClosedCurveGroup(const std::vector<tag_t>& curveTags)
{
    if (curveTags.size() < 2)
    {
        return false;
    }

    struct EndpointEntry
    {
        double point[3];
        int count = 0;
    };

    const double toleranceSquared = 0.05 * 0.05;
    std::vector<EndpointEntry> endpointEntries;
    endpointEntries.reserve(curveTags.size() * 2);

    const auto accumulatePoint = [&](const double point[3])
    {
        for (std::size_t index = 0; index < endpointEntries.size(); ++index)
        {
            if (DistanceSquared(endpointEntries[index].point, point) <= toleranceSquared)
            {
                ++endpointEntries[index].count;
                return;
            }
        }

        EndpointEntry entry;
        entry.point[0] = point[0];
        entry.point[1] = point[1];
        entry.point[2] = point[2];
        entry.count = 1;
        endpointEntries.push_back(entry);
    };

    for (std::size_t index = 0; index < curveTags.size(); ++index)
    {
        double startPoint[3] = {0.0, 0.0, 0.0};
        double endPoint[3] = {0.0, 0.0, 0.0};
        if (!AskCurveEndpoints(curveTags[index], startPoint, endPoint))
        {
            return false;
        }

        accumulatePoint(startPoint);
        accumulatePoint(endPoint);
    }

    for (std::size_t index = 0; index < endpointEntries.size(); ++index)
    {
        if (endpointEntries[index].count != 2)
        {
            return false;
        }
    }

    return true;
}

bool IsArcCurve(tag_t curveTag)
{
    UF_EVAL_p_t evaluator = NULL;
    if (UF_EVAL_initialize(curveTag, &evaluator) != 0 || evaluator == NULL)
    {
        return false;
    }

    logical isArc = false;
    const int errorCode = UF_EVAL_is_arc(evaluator, &isArc);
    UF_EVAL_free(evaluator);
    return errorCode == 0 && isArc;
}

double DistancePointToSegment(
    const std::array<double, 3>& point,
    const std::array<double, 3>& segmentStart,
    const std::array<double, 3>& segmentEnd)
{
    const double vx = segmentEnd[0] - segmentStart[0];
    const double vy = segmentEnd[1] - segmentStart[1];
    const double vz = segmentEnd[2] - segmentStart[2];
    const double wx = point[0] - segmentStart[0];
    const double wy = point[1] - segmentStart[1];
    const double wz = point[2] - segmentStart[2];
    const double segmentLengthSquared = vx * vx + vy * vy + vz * vz;

    if (segmentLengthSquared <= 1.0e-12)
    {
        const double dx = point[0] - segmentStart[0];
        const double dy = point[1] - segmentStart[1];
        const double dz = point[2] - segmentStart[2];
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    const double projection = (wx * vx + wy * vy + wz * vz) / segmentLengthSquared;
    const double clampedProjection = std::max(0.0, std::min(1.0, projection));
    const double closestX = segmentStart[0] + clampedProjection * vx;
    const double closestY = segmentStart[1] + clampedProjection * vy;
    const double closestZ = segmentStart[2] + clampedProjection * vz;
    const double dx = point[0] - closestX;
    const double dy = point[1] - closestY;
    const double dz = point[2] - closestZ;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

std::vector<std::array<double, 3> > SampleCurvePolyline(tag_t curveTag, double targetStep)
{
    std::vector<std::array<double, 3> > samples;
    const double curveLength = AskCurveLength(curveTag);
    if (curveLength <= kCurveLengthTolerance)
    {
        return samples;
    }

    const double safeStep = std::max(0.1, targetStep);
    const int sampleCount = std::max(1, std::min(800, static_cast<int>(std::ceil(curveLength / safeStep))));
    samples.reserve(static_cast<std::size_t>(sampleCount + 1));
    for (int index = 0; index <= sampleCount; ++index)
    {
        const double ratio = static_cast<double>(index) / static_cast<double>(sampleCount);
        const double distance = curveLength * ratio;
        double point[3] = {0.0, 0.0, 0.0};
        if (!AskCurvePointAtDistance(curveTag, distance, point))
        {
            continue;
        }

        samples.push_back(std::array<double, 3>{point[0], point[1], point[2]});
    }

    return samples;
}

std::vector<std::vector<std::array<double, 3> > > SampleCurvePolylines(
    const std::vector<tag_t>& curveTags,
    double targetStep)
{
    std::vector<std::vector<std::array<double, 3> > > samplePolylines;
    samplePolylines.reserve(curveTags.size());
    for (std::size_t index = 0; index < curveTags.size(); ++index)
    {
        const std::vector<std::array<double, 3> > samples = SampleCurvePolyline(curveTags[index], targetStep);
        if (samples.size() >= 2)
        {
            samplePolylines.push_back(samples);
        }
    }
    return samplePolylines;
}

double AskMinDistanceToSamplePolylines(
    const std::array<double, 3>& point,
    const std::vector<std::vector<std::array<double, 3> > >& samplePolylines)
{
    double minDistance = DBL_MAX;
    for (std::size_t polylineIndex = 0; polylineIndex < samplePolylines.size(); ++polylineIndex)
    {
        const std::vector<std::array<double, 3> >& samples = samplePolylines[polylineIndex];
        for (std::size_t sampleIndex = 1; sampleIndex < samples.size(); ++sampleIndex)
        {
            minDistance = std::min(
                minDistance,
                DistancePointToSegment(point, samples[sampleIndex - 1], samples[sampleIndex]));
        }
    }
    return minDistance;
}

double AskMinDistanceToBoundarySamples(
    const std::array<double, 3>& point,
    const std::vector<std::array<double, 3> >& boundarySamples)
{
    if (boundarySamples.size() < 2)
    {
        return DBL_MAX;
    }

    double minDistance = DBL_MAX;
    for (std::size_t index = 1; index < boundarySamples.size(); ++index)
    {
        minDistance = std::min(
            minDistance,
            DistancePointToSegment(point, boundarySamples[index - 1], boundarySamples[index]));
    }
    return minDistance;
}

bool IsCurveEndpointConnectedToArc(
    const CurveEndData& curveData,
    bool checkStartPoint,
    const std::vector<tag_t>& groupCurves,
    const std::vector<CurveEndData>& allEndData,
    const std::vector<bool>& isArcCurveFlags,
    double toleranceSquared)
{
    const double* point = checkStartPoint ? curveData.start : curveData.end;
    for (std::size_t otherIndex = 0; otherIndex < groupCurves.size(); ++otherIndex)
    {
        if (!isArcCurveFlags[otherIndex] || groupCurves[otherIndex] == curveData.curveTag)
        {
            continue;
        }

        const CurveEndData& other = allEndData[otherIndex];
        if (DistanceSquared(point, other.start) <= toleranceSquared ||
            DistanceSquared(point, other.end) <= toleranceSquared)
        {
            return true;
        }
    }

    return false;
}

PreparedCurveSet RemoveStraightEdgesWithoutRoundedCorners(const std::vector<tag_t>& groupCurves)
{
    PreparedCurveSet result;
    result.curves = groupCurves;

    if (groupCurves.size() < 4)
    {
        return result;
    }

    std::vector<CurveEndData> endData(groupCurves.size());
    std::vector<bool> valid(groupCurves.size(), false);
    std::vector<bool> isArcCurveFlags(groupCurves.size(), false);
    int arcCount = 0;
    for (std::size_t index = 0; index < groupCurves.size(); ++index)
    {
        endData[index].curveTag = groupCurves[index];
        valid[index] = AskCurveEndpoints(groupCurves[index], endData[index].start, endData[index].end);
        isArcCurveFlags[index] = IsArcCurve(groupCurves[index]);
        if (isArcCurveFlags[index])
        {
            ++arcCount;
        }
    }

    if (arcCount != 2)
    {
        return result;
    }

    result.curves.clear();
    const double toleranceSquared = 0.05 * 0.05;
    for (std::size_t index = 0; index < groupCurves.size(); ++index)
    {
        if (!valid[index] || isArcCurveFlags[index])
        {
            result.curves.push_back(groupCurves[index]);
            continue;
        }

        const bool startTouchesArc = IsCurveEndpointConnectedToArc(
            endData[index],
            true,
            groupCurves,
            endData,
            isArcCurveFlags,
            toleranceSquared);
        const bool endTouchesArc = IsCurveEndpointConnectedToArc(
            endData[index],
            false,
            groupCurves,
            endData,
            isArcCurveFlags,
            toleranceSquared);

        if (!startTouchesArc && !endTouchesArc)
        {
            result.retiredCurves.push_back(groupCurves[index]);
            continue;
        }

        result.curves.push_back(groupCurves[index]);
    }

    if (result.curves.empty())
    {
        result.curves = groupCurves;
        result.retiredCurves.clear();
    }

    return result;
}

PreparedCurveSet TrimCurvesBySamplePolylines(
    const std::vector<tag_t>& curveTags,
    const std::vector<std::vector<std::array<double, 3> > >& obstacleSamplePolylines,
    double clearanceDistance)
{
    PreparedCurveSet result;
    result.curves = curveTags;

    if (curveTags.empty() || obstacleSamplePolylines.empty() || clearanceDistance <= 0.0)
    {
        return result;
    }

    result.curves.clear();
    for (std::size_t curveIndex = 0; curveIndex < curveTags.size(); ++curveIndex)
    {
        const tag_t curveTag = curveTags[curveIndex];
        if (curveTag == NULL_TAG || UF_OBJ_ask_status(curveTag) != UF_OBJ_ALIVE)
        {
            continue;
        }

        const double curveLength = AskCurveLength(curveTag);
        if (curveLength <= kCurveLengthTolerance)
        {
            result.retiredCurves.push_back(curveTag);
            continue;
        }

        const int sampleCount = std::max(8, std::min(800, static_cast<int>(std::ceil(curveLength / 0.25))));
        const double step = curveLength / static_cast<double>(sampleCount);
        std::vector<bool> safeSamples(static_cast<std::size_t>(sampleCount + 1), false);

        for (int sampleIndex = 0; sampleIndex <= sampleCount; ++sampleIndex)
        {
            const double distanceAlongCurve = step * static_cast<double>(sampleIndex);
            double point[3] = {0.0, 0.0, 0.0};
            if (!AskCurvePointAtDistance(curveTag, distanceAlongCurve, point))
            {
                continue;
            }

            const std::array<double, 3> samplePoint = {point[0], point[1], point[2]};
            safeSamples[static_cast<std::size_t>(sampleIndex)] =
                AskMinDistanceToSamplePolylines(samplePoint, obstacleSamplePolylines) >= (clearanceDistance - 0.02);
        }

        int firstSafeSample = -1;
        int lastSafeSample = -1;
        for (int sampleIndex = 0; sampleIndex <= sampleCount; ++sampleIndex)
        {
            if (safeSamples[static_cast<std::size_t>(sampleIndex)])
            {
                if (firstSafeSample < 0)
                {
                    firstSafeSample = sampleIndex;
                }
                lastSafeSample = sampleIndex;
            }
        }

        if (firstSafeSample < 0 || lastSafeSample < firstSafeSample)
        {
            result.retiredCurves.push_back(curveTag);
            continue;
        }

        if (firstSafeSample == 0 && lastSafeSample == sampleCount)
        {
            result.curves.push_back(curveTag);
            continue;
        }

        double startDistance = step * static_cast<double>(firstSafeSample);
        double endDistance = step * static_cast<double>(lastSafeSample);

        tag_t replacementCurve = NULL_TAG;
        if (endDistance - startDistance > kCurveLengthTolerance)
        {
            replacementCurve = CreateCurveSegmentByDistances(curveTag, startDistance, endDistance);
        }

        if (replacementCurve == NULL_TAG)
        {
            result.retiredCurves.push_back(curveTag);
            continue;
        }

        result.curves.push_back(replacementCurve);
        result.retiredCurves.push_back(curveTag);
    }

    return result;
}

PreparedCurveSet TrimCurvesNearFaceBoundary(
    const std::vector<tag_t>& curveTags,
    tag_t targetFaceTag,
    double clearanceDistance)
{
    PreparedCurveSet result;
    result.curves = curveTags;

    if (curveTags.empty() || targetFaceTag == NULL_TAG || clearanceDistance <= 0.0)
    {
        return result;
    }

    const std::vector<tag_t> boundaryEdges = AskPeripheralLoopEdges(targetFaceTag);
    if (boundaryEdges.empty())
    {
        return result;
    }

    const std::vector<tag_t> boundaryCurves = CreateCurvesFromEdges(boundaryEdges);
    if (boundaryCurves.empty())
    {
        return result;
    }

    const std::vector<std::vector<std::array<double, 3> > > boundarySamplePolylines =
        SampleCurvePolylines(boundaryCurves, 0.5);
    DeleteObjects(boundaryCurves);

    if (boundarySamplePolylines.empty())
    {
        return result;
    }

    return TrimCurvesBySamplePolylines(curveTags, boundarySamplePolylines, clearanceDistance);
}

std::vector<std::vector<std::array<double, 3> > > BuildFaceBoundarySamplePolylines(tag_t targetFaceTag)
{
    std::vector<std::vector<std::array<double, 3> > > boundarySamplePolylines;
    if (targetFaceTag == NULL_TAG)
    {
        return boundarySamplePolylines;
    }

    const std::vector<tag_t> boundaryEdges = AskPeripheralLoopEdges(targetFaceTag);
    if (boundaryEdges.empty())
    {
        return boundarySamplePolylines;
    }

    const std::vector<tag_t> boundaryCurves = CreateCurvesFromEdges(boundaryEdges);
    if (boundaryCurves.empty())
    {
        return boundarySamplePolylines;
    }

    boundarySamplePolylines = SampleCurvePolylines(boundaryCurves, 0.5);
    DeleteObjects(boundaryCurves);
    return boundarySamplePolylines;
}

std::vector<std::vector<tag_t> > SplitCurvesIntoConnectedGroups(const std::vector<tag_t>& curveTags)
{
    std::vector<std::vector<tag_t> > groups;
    if (curveTags.empty())
    {
        return groups;
    }

    const auto distanceSquaredLocal = [](const double lhs[3], const double rhs[3]) -> double
    {
        const double dx = lhs[0] - rhs[0];
        const double dy = lhs[1] - rhs[1];
        const double dz = lhs[2] - rhs[2];
        return dx * dx + dy * dy + dz * dz;
    };

    std::vector<CurveEndData> endData(curveTags.size());
    std::vector<bool> valid(curveTags.size(), false);
    for (std::size_t index = 0; index < curveTags.size(); ++index)
    {
        endData[index].curveTag = curveTags[index];
        valid[index] = AskCurveEndpoints(curveTags[index], endData[index].start, endData[index].end);
    }

    const double toleranceSquared = 0.05 * 0.05;
    std::vector<bool> visited(curveTags.size(), false);

    for (std::size_t seed = 0; seed < curveTags.size(); ++seed)
    {
        if (visited[seed])
        {
            continue;
        }

        visited[seed] = true;
        std::vector<std::size_t> stack(1, seed);
        std::vector<tag_t> group;

        while (!stack.empty())
        {
            const std::size_t current = stack.back();
            stack.pop_back();
            group.push_back(curveTags[current]);

            if (!valid[current])
            {
                continue;
            }

            for (std::size_t other = 0; other < curveTags.size(); ++other)
            {
                if (visited[other] || !valid[other])
                {
                    continue;
                }

                const bool connected =
                    distanceSquaredLocal(endData[current].start, endData[other].start) <= toleranceSquared ||
                    distanceSquaredLocal(endData[current].start, endData[other].end) <= toleranceSquared ||
                    distanceSquaredLocal(endData[current].end, endData[other].start) <= toleranceSquared ||
                    distanceSquaredLocal(endData[current].end, endData[other].end) <= toleranceSquared;
                if (!connected)
                {
                    continue;
                }

                visited[other] = true;
                stack.push_back(other);
            }
        }

        groups.push_back(group);
    }

    return groups;
}

PreparedCurveSet TrimCurveEndpointsNearOtherGroups(
    const std::vector<std::vector<tag_t> >& groups,
    double clearanceDistance)
{
    PreparedCurveSet result;
    if (groups.empty() || clearanceDistance <= 0.0)
    {
        return result;
    }

    std::vector<std::vector<std::vector<std::array<double, 3> > > > groupSamples(groups.size());
    for (std::size_t groupIndex = 0; groupIndex < groups.size(); ++groupIndex)
    {
        groupSamples[groupIndex] = SampleCurvePolylines(groups[groupIndex], 0.5);
    }

    for (std::size_t groupIndex = 0; groupIndex < groups.size(); ++groupIndex)
    {
        std::vector<std::vector<std::array<double, 3> > > obstacleSamples;
        for (std::size_t otherGroupIndex = 0; otherGroupIndex < groups.size(); ++otherGroupIndex)
        {
            if (otherGroupIndex == groupIndex)
            {
                continue;
            }

            obstacleSamples.insert(
                obstacleSamples.end(),
                groupSamples[otherGroupIndex].begin(),
                groupSamples[otherGroupIndex].end());
        }

        if (obstacleSamples.empty())
        {
            result.curves.insert(result.curves.end(), groups[groupIndex].begin(), groups[groupIndex].end());
            continue;
        }

        PreparedCurveSet trimmedGroup =
            TrimCurvesBySamplePolylines(groups[groupIndex], obstacleSamples, clearanceDistance);
        result.curves.insert(result.curves.end(), trimmedGroup.curves.begin(), trimmedGroup.curves.end());
        result.retiredCurves.insert(
            result.retiredCurves.end(),
            trimmedGroup.retiredCurves.begin(),
            trimmedGroup.retiredCurves.end());
    }

    return result;
}

PreparedCurveSet TrimCurveGroupsByCombinedSafety(
    const std::vector<std::vector<tag_t> >& groups,
    tag_t targetFaceTag,
    double clearanceDistance)
{
    PreparedCurveSet result;
    if (groups.empty() || clearanceDistance <= 0.0)
    {
        return result;
    }

    const std::vector<std::vector<std::array<double, 3> > > boundarySamplePolylines =
        BuildFaceBoundarySamplePolylines(targetFaceTag);

    std::vector<std::vector<std::vector<std::array<double, 3> > > > groupSamples(groups.size());
    for (std::size_t groupIndex = 0; groupIndex < groups.size(); ++groupIndex)
    {
        groupSamples[groupIndex] = SampleCurvePolylines(groups[groupIndex], 0.5);
    }

    for (std::size_t groupIndex = 0; groupIndex < groups.size(); ++groupIndex)
    {
        std::vector<std::vector<std::array<double, 3> > > obstacleSamples = boundarySamplePolylines;
        for (std::size_t otherGroupIndex = 0; otherGroupIndex < groups.size(); ++otherGroupIndex)
        {
            if (otherGroupIndex == groupIndex)
            {
                continue;
            }

            obstacleSamples.insert(
                obstacleSamples.end(),
                groupSamples[otherGroupIndex].begin(),
                groupSamples[otherGroupIndex].end());
        }

        if (obstacleSamples.empty())
        {
            result.curves.insert(result.curves.end(), groups[groupIndex].begin(), groups[groupIndex].end());
            continue;
        }

        PreparedCurveSet trimmedGroup =
            TrimCurvesBySamplePolylines(groups[groupIndex], obstacleSamples, clearanceDistance);
        result.curves.insert(result.curves.end(), trimmedGroup.curves.begin(), trimmedGroup.curves.end());
        result.retiredCurves.insert(
            result.retiredCurves.end(),
            trimmedGroup.retiredCurves.begin(),
            trimmedGroup.retiredCurves.end());
    }

    return result;
}

PreparedCurveGroupSet TrimCurveGroupsByCombinedSafetyPreserveGroups(
    const std::vector<std::vector<tag_t> >& groups,
    tag_t targetFaceTag,
    double clearanceDistance)
{
    PreparedCurveGroupSet result;
    result.groups = groups;
    result.curves = FlattenCurveGroups(groups);
    if (groups.empty() || clearanceDistance <= 0.0)
    {
        return result;
    }

    const std::vector<std::vector<std::array<double, 3> > > boundarySamplePolylines =
        BuildFaceBoundarySamplePolylines(targetFaceTag);

    std::vector<std::vector<std::vector<std::array<double, 3> > > > groupSamples(groups.size());
    for (std::size_t groupIndex = 0; groupIndex < groups.size(); ++groupIndex)
    {
        groupSamples[groupIndex] = SampleCurvePolylines(groups[groupIndex], 0.5);
    }

    result.groups.clear();
    result.curves.clear();
    for (std::size_t groupIndex = 0; groupIndex < groups.size(); ++groupIndex)
    {
        std::vector<std::vector<std::array<double, 3> > > obstacleSamples = boundarySamplePolylines;
        for (std::size_t otherGroupIndex = 0; otherGroupIndex < groups.size(); ++otherGroupIndex)
        {
            if (otherGroupIndex == groupIndex)
            {
                continue;
            }

            obstacleSamples.insert(
                obstacleSamples.end(),
                groupSamples[otherGroupIndex].begin(),
                groupSamples[otherGroupIndex].end());
        }

        PreparedCurveSet trimmedGroup;
        if (obstacleSamples.empty())
        {
            trimmedGroup.curves = groups[groupIndex];
        }
        else
        {
            trimmedGroup = TrimCurvesBySamplePolylines(groups[groupIndex], obstacleSamples, clearanceDistance);
        }

        result.groups.push_back(trimmedGroup.curves);
        result.curves.insert(result.curves.end(), trimmedGroup.curves.begin(), trimmedGroup.curves.end());
        result.retiredCurves.insert(
            result.retiredCurves.end(),
            trimmedGroup.retiredCurves.begin(),
            trimmedGroup.retiredCurves.end());
    }

    return result;
}

bool AskLineCurveData(tag_t curveTag, CurveEndData& endData, double direction[3])
{
    UF_EVAL_p_t evaluator = NULL;
    if (UF_EVAL_initialize(curveTag, &evaluator) != 0 || evaluator == NULL)
    {
        return false;
    }

    logical isLine = false;
    const int lineError = UF_EVAL_is_line(evaluator, &isLine);
    UF_EVAL_free(evaluator);
    if (lineError != 0 || !isLine)
    {
        return false;
    }

    endData.curveTag = curveTag;
    if (!AskCurveEndpoints(curveTag, endData.start, endData.end))
    {
        return false;
    }

    for (int axis = 0; axis < 3; ++axis)
    {
        direction[axis] = endData.end[axis] - endData.start[axis];
    }

    return Normalize(direction);
}

bool AreLineCurveEndsConnected(const CurveEndData& left, const CurveEndData& right, double toleranceSquared)
{
    return DistanceSquared(left.start, right.start) <= toleranceSquared ||
           DistanceSquared(left.start, right.end) <= toleranceSquared ||
           DistanceSquared(left.end, right.start) <= toleranceSquared ||
           DistanceSquared(left.end, right.end) <= toleranceSquared;
}

tag_t CreateLineFromPoints(const double startPoint[3], const double endPoint[3])
{
    UF_CURVE_line_t lineData;
    for (int axis = 0; axis < 3; ++axis)
    {
        lineData.start_point[axis] = startPoint[axis];
        lineData.end_point[axis] = endPoint[axis];
    }

    tag_t lineTag = NULL_TAG;
    if (UF_CURVE_create_line(&lineData, &lineTag) != 0)
    {
        return NULL_TAG;
    }

    return lineTag;
}

std::vector<tag_t> MergeConnectedCollinearLineCurves(const std::vector<tag_t>& curveTags)
{
    if (curveTags.size() < 2)
    {
        return curveTags;
    }

    std::vector<CurveEndData> lineData(curveTags.size());
    std::vector<std::array<double, 3> > directions(curveTags.size());
    std::vector<bool> isMergeableLine(curveTags.size(), false);

    for (std::size_t index = 0; index < curveTags.size(); ++index)
    {
        double direction[3] = {0.0, 0.0, 0.0};
        isMergeableLine[index] = AskLineCurveData(curveTags[index], lineData[index], direction);
        if (isMergeableLine[index])
        {
            directions[index] = std::array<double, 3>{direction[0], direction[1], direction[2]};
        }
    }

    const double connectionToleranceSquared = 0.20 * 0.20;
    const double parallelTolerance = 0.999;

    std::vector<bool> consumed(curveTags.size(), false);
    std::vector<tag_t> mergedCurves;
    mergedCurves.reserve(curveTags.size());

    for (std::size_t seed = 0; seed < curveTags.size(); ++seed)
    {
        if (consumed[seed])
        {
            continue;
        }

        consumed[seed] = true;
        if (!isMergeableLine[seed])
        {
            mergedCurves.push_back(curveTags[seed]);
            continue;
        }

        std::vector<std::size_t> chain;
        std::vector<std::size_t> stack(1, seed);
        while (!stack.empty())
        {
            const std::size_t current = stack.back();
            stack.pop_back();
            chain.push_back(current);

            for (std::size_t other = 0; other < curveTags.size(); ++other)
            {
                if (consumed[other] || !isMergeableLine[other])
                {
                    continue;
                }

                const double directionDot =
                    directions[current][0] * directions[other][0] +
                    directions[current][1] * directions[other][1] +
                    directions[current][2] * directions[other][2];
                if (std::fabs(directionDot) < parallelTolerance)
                {
                    continue;
                }

                if (!AreLineCurveEndsConnected(lineData[current], lineData[other], connectionToleranceSquared))
                {
                    continue;
                }

                consumed[other] = true;
                stack.push_back(other);
            }
        }

        if (chain.size() < 2)
        {
            mergedCurves.push_back(curveTags[seed]);
            continue;
        }

        const CurveEndData& first = lineData[chain.front()];
        const std::array<double, 3>& baseDirection = directions[chain.front()];
        double minDistance = 0.0;
        double maxDistance = 0.0;
        double minPoint[3] = {first.start[0], first.start[1], first.start[2]};
        double maxPoint[3] = {first.start[0], first.start[1], first.start[2]};

        const auto absorbPoint = [&](const double point[3])
        {
            const double relative[3] = {
                point[0] - first.start[0],
                point[1] - first.start[1],
                point[2] - first.start[2]};
            const double signedDistance =
                relative[0] * baseDirection[0] +
                relative[1] * baseDirection[1] +
                relative[2] * baseDirection[2];
            if (signedDistance < minDistance)
            {
                minDistance = signedDistance;
                for (int axis = 0; axis < 3; ++axis)
                {
                    minPoint[axis] = point[axis];
                }
            }
            if (signedDistance > maxDistance)
            {
                maxDistance = signedDistance;
                for (int axis = 0; axis < 3; ++axis)
                {
                    maxPoint[axis] = point[axis];
                }
            }
        };

        for (std::size_t chainIndex = 0; chainIndex < chain.size(); ++chainIndex)
        {
            const CurveEndData& data = lineData[chain[chainIndex]];
            absorbPoint(data.start);
            absorbPoint(data.end);
        }

        tag_t mergedLine = CreateLineFromPoints(minPoint, maxPoint);
        if (mergedLine == NULL_TAG)
        {
            for (std::size_t chainIndex = 0; chainIndex < chain.size(); ++chainIndex)
            {
                mergedCurves.push_back(curveTags[chain[chainIndex]]);
            }
            continue;
        }

        std::vector<tag_t> retiredCurves;
        retiredCurves.reserve(chain.size());
        for (std::size_t chainIndex = 0; chainIndex < chain.size(); ++chainIndex)
        {
            retiredCurves.push_back(curveTags[chain[chainIndex]]);
        }
        DeleteObjects(retiredCurves);
        mergedCurves.push_back(mergedLine);
    }

    return mergedCurves;
}

std::vector<tag_t> BuildSegmentedMarkCurves(const std::vector<tag_t>& curveTags, const SegmentParameters& parameters)
{
    if (curveTags.empty())
    {
        return std::vector<tag_t>();
    }

    const std::vector<std::vector<tag_t> > groups = SplitCurvesIntoConnectedGroups(curveTags);
    if (groups.size() <= 1)
    {
        return BuildSegmentedMarkCurvesSingleGroup(curveTags, parameters);
    }

    std::vector<tag_t> finalCurves;
    for (std::size_t groupIndex = 0; groupIndex < groups.size(); ++groupIndex)
    {
        const std::vector<tag_t> groupCurves = BuildSegmentedMarkCurvesSingleGroup(groups[groupIndex], parameters);
        finalCurves.insert(finalCurves.end(), groupCurves.begin(), groupCurves.end());
    }
    return finalCurves;
}

std::vector<std::vector<tag_t> > BuildSegmentedMarkCurveGroups(
    const std::vector<std::vector<tag_t> >& groups,
    const SegmentParameters& parameters)
{
    std::vector<std::vector<tag_t> > finalGroups;
    finalGroups.reserve(groups.size());
    for (std::size_t groupIndex = 0; groupIndex < groups.size(); ++groupIndex)
    {
        finalGroups.push_back(BuildSegmentedMarkCurvesSingleGroup(groups[groupIndex], parameters));
    }
    return finalGroups;
}

std::vector<tag_t> BuildSegmentedMarkCurvesSingleGroup(const std::vector<tag_t>& curveTags, const SegmentParameters& parameters)
{
    if (curveTags.empty())
    {
        return std::vector<tag_t>();
    }

    const std::vector<tag_t> normalizedCurveTags = MergeConnectedCollinearLineCurves(curveTags);
    if (normalizedCurveTags.empty())
    {
        return std::vector<tag_t>();
    }

    std::vector<CurveLengthInfo> infos;
    infos.reserve(normalizedCurveTags.size());
    for (std::size_t index = 0; index < normalizedCurveTags.size(); ++index)
    {
        CurveLengthInfo info;
        info.curveTag = normalizedCurveTags[index];
        info.length = AskCurveLength(normalizedCurveTags[index]);
        info.canSegment = (info.length > kCurveLengthTolerance);
        infos.push_back(info);
    }

    bool hasSplittableFullCircle = false;
    for (std::size_t index = 0; index < infos.size(); ++index)
    {
        if (IsSplittableFullCircleCurve(infos[index].curveTag, infos[index].length))
        {
            hasSplittableFullCircle = true;
            break;
        }
    }

    bool allBelowSegmentThreshold = true;
    for (std::size_t index = 0; index < infos.size(); ++index)
    {
        if (!infos[index].canSegment)
        {
            allBelowSegmentThreshold = false;
            continue;
        }
        if (ShouldSegmentCurve(infos[index].length, parameters))
        {
            allBelowSegmentThreshold = false;
            break;
        }
    }

    if (allBelowSegmentThreshold)
    {
        if (!hasSplittableFullCircle && !IsClosedCurveGroup(normalizedCurveTags))
        {
            return normalizedCurveTags;
        }

        if (!hasSplittableFullCircle && infos.size() <= 1)
        {
            return normalizedCurveTags;
        }

        if (hasSplittableFullCircle)
        {
            allBelowSegmentThreshold = false;
        }
    }

    if (allBelowSegmentThreshold)
    {
        std::size_t shortestIndex = 0;
        std::size_t longestIndex = 0;
        for (std::size_t index = 1; index < infos.size(); ++index)
        {
            if (infos[index].length < infos[shortestIndex].length)
            {
                shortestIndex = index;
            }
            if (infos[index].length > infos[longestIndex].length)
            {
                longestIndex = index;
            }
        }

        const std::size_t removedIndex = (parameters.closedCurveRule == 1) ? longestIndex : shortestIndex;

        std::vector<tag_t> result;
        result.reserve(normalizedCurveTags.size() + 2);
        for (std::size_t index = 0; index < infos.size(); ++index)
        {
            if (index != removedIndex)
            {
                result.push_back(infos[index].curveTag);
            }
        }

        if (parameters.closedCurveRule == 1)
        {
            const CurveLengthInfo& longest = infos[longestIndex];
            const double gapLength = std::min(2.0, longest.length * 0.5);
            const double keepLength = 0.5 * (longest.length - gapLength);
            if (keepLength > kCurveLengthTolerance)
            {
                tag_t startSegment = CreateCurveSegmentByDistances(longest.curveTag, 0.0, keepLength);
                tag_t endSegment =
                    CreateCurveSegmentByDistances(longest.curveTag, longest.length - keepLength, longest.length);

                if (startSegment != NULL_TAG && endSegment != NULL_TAG)
                {
                    DeleteObjects(std::vector<tag_t>{longest.curveTag});
                    result.push_back(startSegment);
                    result.push_back(endSegment);
                    return result;
                }

                DeleteObjects(std::vector<tag_t>{startSegment, endSegment});
            }
        }

        DeleteObjects(std::vector<tag_t>{infos[removedIndex].curveTag});
        return result;
    }

    std::vector<tag_t> finalCurves;
    finalCurves.reserve(normalizedCurveTags.size() * 4);
    for (std::size_t index = 0; index < infos.size(); ++index)
    {
        const CurveLengthInfo& info = infos[index];
        if (!info.canSegment)
        {
            finalCurves.push_back(info.curveTag);
            continue;
        }

        std::vector<tag_t> fullCircleSegments;
        if (SplitFullCircleIntoTwoMarkSegments(info.curveTag, info.length, fullCircleSegments))
        {
            DeleteObjects(std::vector<tag_t>{info.curveTag});
            finalCurves.insert(finalCurves.end(), fullCircleSegments.begin(), fullCircleSegments.end());
            continue;
        }

        if (!ShouldSegmentCurve(info.length, parameters))
        {
            finalCurves.push_back(info.curveTag);
            continue;
        }

        std::vector<CurveSegmentRange> ranges = BuildMarkSegmentRanges(info.length, parameters);

        std::sort(
            ranges.begin(),
            ranges.end(),
            [](const CurveSegmentRange& lhs, const CurveSegmentRange& rhs)
            {
                return lhs.startDistance < rhs.startDistance;
            });

        std::vector<tag_t> segmentCurves;
        bool success = true;
        for (std::size_t rangeIndex = 0; rangeIndex < ranges.size(); ++rangeIndex)
        {
            const double start = std::max(0.0, std::min(ranges[rangeIndex].startDistance, info.length));
            const double end = std::max(0.0, std::min(ranges[rangeIndex].endDistance, info.length));
            if (end - start <= kCurveLengthTolerance)
            {
                continue;
            }

            tag_t segmentCurve = CreateCurveSegmentByDistances(info.curveTag, start, end);
            if (segmentCurve == NULL_TAG)
            {
                success = false;
                break;
            }
            segmentCurves.push_back(segmentCurve);
        }

        if (success && !segmentCurves.empty())
        {
            DeleteObjects(std::vector<tag_t>{info.curveTag});
            finalCurves.insert(finalCurves.end(), segmentCurves.begin(), segmentCurves.end());
        }
        else
        {
            DeleteObjects(segmentCurves);
            finalCurves.push_back(info.curveTag);
        }
    }

    return finalCurves;
}

double DistanceSquared(const double lhs[3], const double rhs[3])
{
    const double dx = lhs[0] - rhs[0];
    const double dy = lhs[1] - rhs[1];
    const double dz = lhs[2] - rhs[2];
    return dx * dx + dy * dy + dz * dz;
}

void ExpandBounds(double minCorner[3], double maxCorner[3], const double point[3])
{
    for (int axis = 0; axis < 3; ++axis)
    {
        minCorner[axis] = std::min(minCorner[axis], point[axis]);
        maxCorner[axis] = std::max(maxCorner[axis], point[axis]);
    }
}

bool IsSolidBody(tag_t bodyTag)
{
    int bodyType = 0;
    if (UF_MODL_ask_body_type(bodyTag, &bodyType) != 0)
    {
        return false;
    }
    return bodyType == UF_MODL_SOLID_BODY;
}

tag_t AskFaceOwningSolidBody(tag_t faceTag)
{
    if (faceTag == NULL_TAG)
    {
        return NULL_TAG;
    }

    tag_t bodyTag = NULL_TAG;
    if (UF_MODL_ask_face_body(faceTag, &bodyTag) != 0 || bodyTag == NULL_TAG)
    {
        return NULL_TAG;
    }

    return IsSolidBody(bodyTag) ? bodyTag : NULL_TAG;
}

double AskBoundingBoxVolume(tag_t bodyTag)
{
    double box[6] = {0.0};
    if (UF_MODL_ask_bounding_box(bodyTag, box) != 0)
    {
        return 0.0;
    }

    return (box[3] - box[0]) * (box[4] - box[1]) * (box[5] - box[2]);
}

std::vector<tag_t> AskAllCurveObjectsInWorkPart(tag_t partTag)
{
    std::vector<tag_t> curveTags;
    tag_t objectTag = NULL_TAG;
    while (UF_OBJ_cycle_objs_in_part(partTag, UF_line_type, &objectTag) == 0 && objectTag != NULL_TAG)
    {
        curveTags.push_back(objectTag);
    }

    objectTag = NULL_TAG;
    while (UF_OBJ_cycle_objs_in_part(partTag, UF_circle_type, &objectTag) == 0 && objectTag != NULL_TAG)
    {
        curveTags.push_back(objectTag);
    }

    objectTag = NULL_TAG;
    while (UF_OBJ_cycle_objs_in_part(partTag, UF_conic_type, &objectTag) == 0 && objectTag != NULL_TAG)
    {
        curveTags.push_back(objectTag);
    }

    objectTag = NULL_TAG;
    while (UF_OBJ_cycle_objs_in_part(partTag, UF_spline_type, &objectTag) == 0 && objectTag != NULL_TAG)
    {
        curveTags.push_back(objectTag);
    }

    std::sort(curveTags.begin(), curveTags.end());
    curveTags.erase(std::unique(curveTags.begin(), curveTags.end()), curveTags.end());
    return curveTags;
}

std::vector<tag_t> AskAllCurveObjectsForSearch(Part* workPart)
{
    std::vector<tag_t> curveTags;
    if (workPart != NULL)
    {
        const std::vector<tag_t> workCurves = AskAllCurveObjectsInWorkPart(workPart->Tag());
        curveTags.insert(curveTags.end(), workCurves.begin(), workCurves.end());
    }

    Part* displayPart = Session::GetSession() != NULL && Session::GetSession()->Parts() != NULL
        ? Session::GetSession()->Parts()->Display()
        : NULL;
    if (displayPart != NULL && (workPart == NULL || displayPart->Tag() != workPart->Tag()))
    {
        const std::vector<tag_t> displayCurves = AskAllCurveObjectsInWorkPart(displayPart->Tag());
        curveTags.insert(curveTags.end(), displayCurves.begin(), displayCurves.end());
    }

    std::sort(curveTags.begin(), curveTags.end());
    curveTags.erase(std::unique(curveTags.begin(), curveTags.end()), curveTags.end());
    return curveTags;
}

std::vector<tag_t> AskSolidBodiesInWorkPart(tag_t partTag)
{
    std::vector<tag_t> bodies;
    tag_t bodyTag = NULL_TAG;
    while (UF_OBJ_cycle_objs_in_part(partTag, UF_solid_type, &bodyTag) == 0 && bodyTag != NULL_TAG)
    {
        if (!IsSolidBody(bodyTag))
        {
            continue;
        }

        DisplayableObject* displayable = dynamic_cast<DisplayableObject*>(NXObjectManager::Get(bodyTag));
        if (displayable == NULL || displayable->IsBlanked())
        {
            continue;
        }

        if (UF_OBJ_ask_status(bodyTag) == UF_OBJ_ALIVE)
        {
            bodies.push_back(bodyTag);
        }
    }
    return bodies;
}

std::vector<tag_t> AskPlanarFaces(tag_t bodyTag)
{
    std::vector<tag_t> planarFaces;
    uf_list_p_t faceList = nullptr;
    if (UF_MODL_ask_body_faces(bodyTag, &faceList) != 0 || faceList == nullptr)
    {
        return planarFaces;
    }

    int count = 0;
    UF_MODL_ask_list_count(faceList, &count);
    for (int index = 0; index < count; ++index)
    {
        tag_t faceTag = NULL_TAG;
        if (UF_MODL_ask_list_item(faceList, index, &faceTag) != 0 || faceTag == NULL_TAG)
        {
            continue;
        }

        int faceType = 0;
        if (UF_MODL_ask_face_type(faceTag, &faceType) == 0 && faceType == UF_MODL_PLANAR_FACE)
        {
            planarFaces.push_back(faceTag);
        }
    }

    UF_MODL_delete_list(&faceList);
    return planarFaces;
}

std::vector<tag_t> AskPeripheralLoopEdges(tag_t faceTag)
{
    std::vector<tag_t> edges;
    if (faceTag == NULL_TAG)
    {
        return edges;
    }

    uf_loop_p_t loopList = nullptr;
    if (UF_MODL_ask_face_loops(faceTag, &loopList) != 0 || loopList == nullptr)
    {
        return edges;
    }

    for (uf_loop_p_t loop = loopList; loop != nullptr; loop = loop->next)
    {
        if (loop->type != 1 || loop->edge_list == nullptr)
        {
            continue;
        }

        int count = 0;
        UF_MODL_ask_list_count(loop->edge_list, &count);
        for (int index = 0; index < count; ++index)
        {
            tag_t edgeTag = NULL_TAG;
            if (UF_MODL_ask_list_item(loop->edge_list, index, &edgeTag) == 0 && edgeTag != NULL_TAG)
            {
                edges.push_back(edgeTag);
            }
        }
        break;
    }

    UF_MODL_delete_loop_list(&loopList);
    return edges;
}

bool AskFaceNormal(tag_t faceTag, double normal[3])
{
    int faceType = 0;
    double point[3] = {0.0};
    double direction[3] = {0.0};
    double box[6] = {0.0};
    double radius = 0.0;
    double radData = 0.0;
    int normalDir = 0;

    if (UF_MODL_ask_face_data(faceTag, &faceType, point, direction, box, &radius, &radData, &normalDir) != 0)
    {
        return false;
    }

    if (faceType != UF_MODL_PLANAR_FACE)
    {
        return false;
    }

    normal[0] = direction[0];
    normal[1] = direction[1];
    normal[2] = direction[2];
    return Normalize(normal);
}

bool AskPlanarFaceMidpoint(tag_t faceTag, double midpoint[3])
{
    int faceType = 0;
    double planePoint[3] = {0.0};
    double direction[3] = {0.0};
    double box[6] = {0.0};
    double radius = 0.0;
    double radData = 0.0;
    int normalDir = 0;

    if (UF_MODL_ask_face_data(faceTag, &faceType, planePoint, direction, box, &radius, &radData, &normalDir) != 0)
    {
        return false;
    }
    if (faceType != UF_MODL_PLANAR_FACE)
    {
        return false;
    }

    for (int axis = 0; axis < 3; ++axis)
    {
        midpoint[axis] = 0.5 * (box[axis] + box[axis + 3]);
    }

    double faceParameters[2] = {0.0, 0.0};
    double facePoint[3] = {midpoint[0], midpoint[1], midpoint[2]};
    if (UF_MODL_ask_face_parm(faceTag, midpoint, faceParameters, facePoint) == 0)
    {
        for (int axis = 0; axis < 3; ++axis)
        {
            midpoint[axis] = facePoint[axis];
        }
        return true;
    }

    if (!Normalize(direction))
    {
        return false;
    }

    const double fromPlanePoint[3] = {
        midpoint[0] - planePoint[0],
        midpoint[1] - planePoint[1],
        midpoint[2] - planePoint[2]};
    const double offset = Dot(fromPlanePoint, direction);
    for (int axis = 0; axis < 3; ++axis)
    {
        midpoint[axis] -= offset * direction[axis];
    }
    return true;
}

Plane* CreatePlaneFromPlanarFace(Part* workPart, tag_t faceTag)
{
    if (workPart == NULL || faceTag == NULL_TAG || workPart->Planes() == NULL)
    {
        return NULL;
    }

    double origin[3] = {0.0, 0.0, 0.0};
    double normal[3] = {0.0, 0.0, 0.0};
    if (!AskPlanarFaceMidpoint(faceTag, origin) || !AskFaceNormal(faceTag, normal))
    {
        return NULL;
    }

    return workPart->Planes()->CreatePlane(
        Point3d(origin[0], origin[1], origin[2]),
        Vector3d(normal[0], normal[1], normal[2]),
        SmartObject::UpdateOptionWithinModeling);
}

std::vector<tag_t> AskFeatureFaces(tag_t featureTag)
{
    std::vector<tag_t> faces;
    if (featureTag == NULL_TAG)
    {
        return faces;
    }

    uf_list_p_t faceList = nullptr;
    if (UF_MODL_ask_feat_faces(featureTag, &faceList) != 0 || faceList == nullptr)
    {
        return faces;
    }

    int count = 0;
    UF_MODL_ask_list_count(faceList, &count);
    faces.reserve(static_cast<std::size_t>(std::max(0, count)));
    for (int index = 0; index < count; ++index)
    {
        tag_t faceTag = NULL_TAG;
        if (UF_MODL_ask_list_item(faceList, index, &faceTag) == 0 && faceTag != NULL_TAG)
        {
            faces.push_back(faceTag);
        }
    }
    UF_MODL_delete_list(&faceList);

    std::sort(faces.begin(), faces.end());
    faces.erase(std::unique(faces.begin(), faces.end()), faces.end());
    return faces;
}

void SetMarkFaceAttribute(tag_t faceTag)
{
    Face* face = dynamic_cast<Face*>(NXObjectManager::Get(faceTag));
    if (face == NULL)
    {
        return;
    }

    try
    {
        face->SetUserAttribute(NXString(ToUtf8(L"标记面"), NXString::UTF8), -1, 1, Update::OptionNow);
    }
    catch (...)
    {
    }
}

void MarkShallowGrooveFaces(tag_t extrudeFeatureTag)
{
    const std::vector<tag_t> featureFaces = AskFeatureFaces(extrudeFeatureTag);
    for (std::size_t index = 0; index < featureFaces.size(); ++index)
    {
        const tag_t faceTag = featureFaces[index];
        if (faceTag == NULL_TAG)
        {
            continue;
        }

        SetMarkFaceAttribute(faceTag);
    }
}

std::vector<ContactMatch> FindContactsForTargetFace(tag_t targetFaceTag, tag_t workPartTag)
{
    std::vector<ContactMatch> matches;
    const tag_t targetBodyTag = AskFaceOwningSolidBody(targetFaceTag);
    if (targetBodyTag == NULL_TAG)
    {
        return matches;
    }

    double targetNormal[3] = {0.0};
    if (!AskFaceNormal(targetFaceTag, targetNormal))
    {
        return matches;
    }

    for (tag_t candidateBodyTag : AskSolidBodiesInWorkPart(workPartTag))
    {
        if (candidateBodyTag == targetBodyTag)
        {
            continue;
        }

        const std::vector<tag_t> sourceFaces = AskPlanarFaces(candidateBodyTag);
        for (tag_t sourceFaceTag : sourceFaces)
        {
            double sourceNormal[3] = {0.0};
            if (!AskFaceNormal(sourceFaceTag, sourceNormal))
            {
                continue;
            }

            if (Dot(sourceNormal, targetNormal) > -0.95)
            {
                continue;
            }

            double faceDistance = 0.0;
            double facePoint1[3] = {0.0};
            double facePoint2[3] = {0.0};
            if (UF_MODL_ask_minimum_dist(
                    sourceFaceTag,
                    targetFaceTag,
                    0,
                    nullptr,
                    0,
                    nullptr,
                    &faceDistance,
                    facePoint1,
                    facePoint2) != 0)
            {
                continue;
            }

            if (faceDistance <= kContactTolerance)
            {
                ContactMatch match;
                match.targetBody = targetBodyTag;
                match.sourceFace = sourceFaceTag;
                match.targetFace = targetFaceTag;
                match.distance = faceDistance;
                for (int axis = 0; axis < 3; ++axis)
                {
                    match.sourcePoint[axis] = facePoint1[axis];
                    match.targetPoint[axis] = facePoint2[axis];
                }
                matches.push_back(match);
            }
        }
    }

    std::sort(
        matches.begin(),
        matches.end(),
        [](const ContactMatch& left, const ContactMatch& right)
        {
            if (left.distance != right.distance)
            {
                return left.distance < right.distance;
            }
            if (left.targetBody != right.targetBody)
            {
                return left.targetBody < right.targetBody;
            }
            return left.targetFace < right.targetFace;
        });

    std::vector<ContactMatch> bestMatches;
    for (std::size_t index = 0; index < matches.size(); ++index)
    {
        const tag_t sourceBodyTag = AskFaceOwningSolidBody(matches[index].sourceFace);
        bool alreadyUsed = false;
        for (std::size_t usedIndex = 0; usedIndex < bestMatches.size(); ++usedIndex)
        {
            if (AskFaceOwningSolidBody(bestMatches[usedIndex].sourceFace) == sourceBodyTag &&
                bestMatches[usedIndex].targetFace == matches[index].targetFace)
            {
                alreadyUsed = true;
                break;
            }
        }
        if (!alreadyUsed)
        {
            bestMatches.push_back(matches[index]);
        }
    }

    matches.swap(bestMatches);

    matches.erase(
        std::unique(
            matches.begin(),
            matches.end(),
            [](const ContactMatch& left, const ContactMatch& right)
            {
                return left.targetBody == right.targetBody &&
                       left.sourceFace == right.sourceFace &&
                       left.targetFace == right.targetFace;
            }),
        matches.end());

    return matches;
}

bool BuildManualContactForSourceBody(tag_t targetFaceTag, tag_t sourceBodyTag, ContactMatch& match)
{
    match = ContactMatch();

    const tag_t targetBodyTag = AskFaceOwningSolidBody(targetFaceTag);
    if (targetBodyTag == NULL_TAG || sourceBodyTag == NULL_TAG || sourceBodyTag == targetBodyTag)
    {
        return false;
    }

    if (!IsSolidBody(sourceBodyTag) || UF_OBJ_ask_status(sourceBodyTag) != UF_OBJ_ALIVE)
    {
        return false;
    }

    DisplayableObject* displayable = dynamic_cast<DisplayableObject*>(NXObjectManager::Get(sourceBodyTag));
    if (displayable == NULL || displayable->IsBlanked())
    {
        return false;
    }

    double targetNormal[3] = {0.0};
    if (!AskFaceNormal(targetFaceTag, targetNormal))
    {
        return false;
    }

    const std::vector<tag_t> sourceFaces = AskPlanarFaces(sourceBodyTag);
    for (tag_t sourceFaceTag : sourceFaces)
    {
        double sourceNormal[3] = {0.0};
        if (!AskFaceNormal(sourceFaceTag, sourceNormal))
        {
            continue;
        }

        if (Dot(sourceNormal, targetNormal) > -0.95)
        {
            continue;
        }

        double faceDistance = 0.0;
        double facePoint1[3] = {0.0};
        double facePoint2[3] = {0.0};
        if (UF_MODL_ask_minimum_dist(
                sourceFaceTag,
                targetFaceTag,
                0,
                nullptr,
                0,
                nullptr,
                &faceDistance,
                facePoint1,
                facePoint2) != 0)
        {
            continue;
        }

        if (faceDistance < match.distance)
        {
            match.targetBody = targetBodyTag;
            match.sourceFace = sourceFaceTag;
            match.targetFace = targetFaceTag;
            match.distance = faceDistance;
            for (int axis = 0; axis < 3; ++axis)
            {
                match.sourcePoint[axis] = facePoint1[axis];
                match.targetPoint[axis] = facePoint2[axis];
            }
        }
    }

    return match.sourceFace != NULL_TAG;
}

std::vector<ContactMatch> AskUserToSelectSourceBodyContact(tag_t targetFaceTag, tag_t workPartTag)
{
    std::vector<ContactMatch> matches;
    const tag_t targetBodyTag = AskFaceOwningSolidBody(targetFaceTag);
    if (targetBodyTag == NULL_TAG)
    {
        return matches;
    }

    UI* ui = UI::GetUI();
    if (ui == NULL || ui->SelectionManager() == NULL)
    {
        return matches;
    }

    std::vector<NXOpen::Selection::MaskTriple> maskArray(
        1,
        NXOpen::Selection::MaskTriple(UF_solid_type, UF_solid_body_subtype, UF_UI_SEL_FEATURE_BODY));
    std::vector<NXOpen::TaggedObject*> selectedObjects;
    NXOpen::Selection::Response response = ui->SelectionManager()->SelectTaggedObjects(
        ToUtf8(L"请选择要标记的体，或取消退出").c_str(),
        ToUtf8(L"未检查到要标记的体，请手动选择或退出").c_str(),
        NXOpen::Selection::SelectionScopeWorkPart,
        NXOpen::Selection::SelectionActionClearAndEnableSpecific,
        false,
        true,
        maskArray,
        selectedObjects);

    if (!selectedObjects.empty())
    {
        std::vector<tag_t> selectedTags;
        selectedTags.reserve(selectedObjects.size());
        for (std::size_t i = 0; i < selectedObjects.size(); ++i)
        {
            if (selectedObjects[i] != NULL)
            {
                selectedTags.push_back(selectedObjects[i]->Tag());
            }
        }
        if (!selectedTags.empty())
        {
            UF_DISP_set_highlights(
                static_cast<int>(selectedTags.size()),
                selectedTags.data(),
                0);
        }
    }

    if (response != NXOpen::Selection::ResponseOk || selectedObjects.empty())
    {
        return matches;
    }

    const std::vector<tag_t> displayedBodies = AskSolidBodiesInWorkPart(workPartTag);
    std::unordered_set<tag_t> displayedBodySet(displayedBodies.begin(), displayedBodies.end());
    for (std::size_t i = 0; i < selectedObjects.size(); ++i)
    {
        if (selectedObjects[i] == NULL)
        {
            continue;
        }

        const tag_t sourceBodyTag = selectedObjects[i]->Tag();
        if (sourceBodyTag == targetBodyTag || displayedBodySet.find(sourceBodyTag) == displayedBodySet.end())
        {
            continue;
        }

        ContactMatch match;
        if (BuildManualContactForSourceBody(targetFaceTag, sourceBodyTag, match))
        {
            matches.push_back(match);
            break;
        }
    }

    if (matches.empty())
    {
        ui->NXMessageBox()->Show(
            NXString(ToUtf8(L"标记线"), NXString::UTF8),
            NXMessageBox::DialogTypeError,
            NXString(ToUtf8(L"所选体没有找到与投影面相对的平面，不能生成标记线。"), NXString::UTF8));
    }

    return matches;
}

std::vector<ContactMatch> ResolveContactsForTargetFace(tag_t targetFaceTag, tag_t workPartTag)
{
    std::vector<ContactMatch> matches = FindContactsForTargetFace(targetFaceTag, workPartTag);
    if (!matches.empty())
    {
        return matches;
    }

    return AskUserToSelectSourceBodyContact(targetFaceTag, workPartTag);
}

ContactMatch FindBestBottomContact(tag_t sourceBodyTag, tag_t workPartTag)
{
    ContactMatch bestMatch;
    const double sourceVolume = AskBoundingBoxVolume(sourceBodyTag);
    const std::vector<tag_t> sourceFaces = AskPlanarFaces(sourceBodyTag);

    for (tag_t candidateBodyTag : AskSolidBodiesInWorkPart(workPartTag))
    {
        if (candidateBodyTag == sourceBodyTag)
        {
            continue;
        }

        if (AskBoundingBoxVolume(candidateBodyTag) <= sourceVolume)
        {
            continue;
        }

        double bodyDistance = 0.0;
        double sourcePoint[3] = {0.0};
        double targetPoint[3] = {0.0};
        if (UF_MODL_ask_minimum_dist(
                sourceBodyTag,
                candidateBodyTag,
                0,
                nullptr,
                0,
                nullptr,
                &bodyDistance,
                sourcePoint,
                targetPoint) != 0)
        {
            continue;
        }

        if (bodyDistance > kContactTolerance)
        {
            continue;
        }

        const std::vector<tag_t> targetFaces = AskPlanarFaces(candidateBodyTag);
        for (tag_t sourceFaceTag : sourceFaces)
        {
            double sourceNormal[3] = {0.0};
            if (!AskFaceNormal(sourceFaceTag, sourceNormal))
            {
                continue;
            }

            for (tag_t targetFaceTag : targetFaces)
            {
                double targetNormal[3] = {0.0};
                if (!AskFaceNormal(targetFaceTag, targetNormal))
                {
                    continue;
                }

                if (Dot(sourceNormal, targetNormal) > -0.95)
                {
                    continue;
                }

                double faceDistance = 0.0;
                double facePoint1[3] = {0.0};
                double facePoint2[3] = {0.0};
                if (UF_MODL_ask_minimum_dist(
                        sourceFaceTag,
                        targetFaceTag,
                        0,
                        nullptr,
                        0,
                        nullptr,
                        &faceDistance,
                        facePoint1,
                        facePoint2) != 0)
                {
                    continue;
                }

                if (faceDistance <= kContactTolerance && faceDistance < bestMatch.distance)
                {
                    bestMatch.targetBody = candidateBodyTag;
                    bestMatch.sourceFace = sourceFaceTag;
                    bestMatch.targetFace = targetFaceTag;
                    bestMatch.distance = faceDistance;
                    for (int axis = 0; axis < 3; ++axis)
                    {
                        bestMatch.sourcePoint[axis] = facePoint1[axis];
                        bestMatch.targetPoint[axis] = facePoint2[axis];
                    }
                }
            }
        }
    }

    return bestMatch;
}

std::vector<ContactMatch> FindAllBottomContacts(tag_t sourceBodyTag, tag_t workPartTag)
{
    std::vector<ContactMatch> matches;
    const double sourceVolume = AskBoundingBoxVolume(sourceBodyTag);
    const std::vector<tag_t> sourceFaces = AskPlanarFaces(sourceBodyTag);

    for (tag_t candidateBodyTag : AskSolidBodiesInWorkPart(workPartTag))
    {
        if (candidateBodyTag == sourceBodyTag)
        {
            continue;
        }

        if (AskBoundingBoxVolume(candidateBodyTag) <= sourceVolume)
        {
            continue;
        }

        double bodyDistance = 0.0;
        double sourcePoint[3] = {0.0};
        double targetPoint[3] = {0.0};
        if (UF_MODL_ask_minimum_dist(
                sourceBodyTag,
                candidateBodyTag,
                0,
                nullptr,
                0,
                nullptr,
                &bodyDistance,
                sourcePoint,
                targetPoint) != 0)
        {
            continue;
        }

        if (bodyDistance > kContactTolerance)
        {
            continue;
        }

        const std::vector<tag_t> targetFaces = AskPlanarFaces(candidateBodyTag);

        for (tag_t sourceFaceTag : sourceFaces)
        {
            double sourceNormal[3] = {0.0};
            if (!AskFaceNormal(sourceFaceTag, sourceNormal))
            {
                continue;
            }

            for (tag_t targetFaceTag : targetFaces)
            {
                double targetNormal[3] = {0.0};
                if (!AskFaceNormal(targetFaceTag, targetNormal))
                {
                    continue;
                }

                if (Dot(sourceNormal, targetNormal) > -0.95)
                {
                    continue;
                }

                double faceDistance = 0.0;
                double facePoint1[3] = {0.0};
                double facePoint2[3] = {0.0};
                if (UF_MODL_ask_minimum_dist(
                        sourceFaceTag,
                        targetFaceTag,
                        0,
                        nullptr,
                        0,
                        nullptr,
                        &faceDistance,
                        facePoint1,
                        facePoint2) != 0)
                {
                    continue;
                }

                if (faceDistance <= kContactTolerance)
                {
                    ContactMatch match;
                    match.targetBody = candidateBodyTag;
                    match.sourceFace = sourceFaceTag;
                    match.targetFace = targetFaceTag;
                    match.distance = faceDistance;
                    for (int axis = 0; axis < 3; ++axis)
                    {
                        match.sourcePoint[axis] = facePoint1[axis];
                        match.targetPoint[axis] = facePoint2[axis];
                    }
                    matches.push_back(match);
                }
            }
        }
    }

    std::sort(
        matches.begin(),
        matches.end(),
        [](const ContactMatch& left, const ContactMatch& right)
        {
            if (left.targetBody != right.targetBody)
            {
                return left.targetBody < right.targetBody;
            }
            if (left.sourceFace != right.sourceFace)
            {
                return left.sourceFace < right.sourceFace;
            }
            if (left.targetFace != right.targetFace)
            {
                return left.targetFace < right.targetFace;
            }
            return left.distance < right.distance;
        });

    matches.erase(
        std::unique(
            matches.begin(),
            matches.end(),
            [](const ContactMatch& left, const ContactMatch& right)
            {
                return left.targetBody == right.targetBody &&
                       left.sourceFace == right.sourceFace &&
                       left.targetFace == right.targetFace;
            }),
        matches.end());

    std::sort(
        matches.begin(),
        matches.end(),
        [](const ContactMatch& left, const ContactMatch& right)
        {
            if (left.distance != right.distance)
            {
                return left.distance < right.distance;
            }
            return left.targetBody < right.targetBody;
        });

    return matches;
}

std::vector<ContactMatch> KeepBestMatchPerTargetFace(const std::vector<ContactMatch>& matches)
{
    if (matches.size() <= 1)
    {
        return matches;
    }

    std::vector<ContactMatch> sortedMatches = matches;
    std::sort(
        sortedMatches.begin(),
        sortedMatches.end(),
        [](const ContactMatch& left, const ContactMatch& right)
        {
            if (left.targetBody != right.targetBody)
            {
                return left.targetBody < right.targetBody;
            }
            if (left.targetFace != right.targetFace)
            {
                return left.targetFace < right.targetFace;
            }
            if (left.distance != right.distance)
            {
                return left.distance < right.distance;
            }
            return left.sourceFace < right.sourceFace;
        });

    std::vector<ContactMatch> bestMatches;
    for (std::size_t index = 0; index < sortedMatches.size(); ++index)
    {
        if (!bestMatches.empty() &&
            bestMatches.back().targetBody == sortedMatches[index].targetBody &&
            bestMatches.back().targetFace == sortedMatches[index].targetFace)
        {
            continue;
        }

        bestMatches.push_back(sortedMatches[index]);
    }

    return bestMatches;
}

std::vector<ContactMatch> ResolveMatchesForMarking(
    const std::vector<ContactMatch>& allMatches,
    bool oneMatchPerTargetFace)
{
    if (allMatches.size() <= 1)
    {
        return allMatches;
    }

    std::vector<ContactMatch> sortedMatches = allMatches;
    std::sort(
        sortedMatches.begin(),
        sortedMatches.end(),
        [](const ContactMatch& left, const ContactMatch& right)
        {
            if (left.targetBody != right.targetBody)
            {
                return left.targetBody < right.targetBody;
            }
            if (left.sourceFace != right.sourceFace)
            {
                return left.sourceFace < right.sourceFace;
            }
            if (left.targetFace != right.targetFace)
            {
                return left.targetFace < right.targetFace;
            }
            return left.distance < right.distance;
        });

    std::vector<ContactMatch> resolved;
    std::size_t start = 0;
    while (start < sortedMatches.size())
    {
        const tag_t targetBody = sortedMatches[start].targetBody;
        std::size_t end = start + 1;
        while (end < sortedMatches.size() && sortedMatches[end].targetBody == targetBody)
        {
            ++end;
        }

        std::vector<ContactMatch> bodyMatches(
            sortedMatches.begin() + static_cast<std::ptrdiff_t>(start),
            sortedMatches.begin() + static_cast<std::ptrdiff_t>(end));
        const std::vector<ContactMatch> selectedMatches = AskUserToSelectContactMatches(bodyMatches);
        const std::vector<ContactMatch> matchesToUse = oneMatchPerTargetFace
            ? KeepBestMatchPerTargetFace(selectedMatches)
            : selectedMatches;
        resolved.insert(resolved.end(), matchesToUse.begin(), matchesToUse.end());
        start = end;
    }

    return resolved;
}

std::string ResolveDialogFilePath(const char* fileName)
{
    char modulePath[MAX_PATH] = {};
    HMODULE moduleHandle = NULL;
    if (!GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(&ResolveDialogFilePath),
            &moduleHandle))
    {
        return fileName != NULL ? fileName : "BiaoJIXIan.dlx";
    }

    DWORD length = GetModuleFileNameA(moduleHandle, modulePath, MAX_PATH);
    if (length == 0 || length == MAX_PATH)
    {
        return fileName != NULL ? fileName : "BiaoJIXIan.dlx";
    }

    const std::filesystem::path dllDir = std::filesystem::path(modulePath).parent_path();
    const std::filesystem::path candidates[] = {
        dllDir / (fileName != NULL ? fileName : "BiaoJIXIan.dlx"),
        dllDir / "ui" / (fileName != NULL ? fileName : "BiaoJIXIan.dlx")};

    for (const std::filesystem::path& candidate : candidates)
    {
        if (std::filesystem::exists(candidate))
        {
            return candidate.string();
        }
    }

    return candidates[0].string();
}

std::string ResolveDialogPath()
{
    return ResolveDialogFilePath("BiaoJIXIan.dlx");
}

std::string ToFormulaString(double value)
{
    std::ostringstream stream;
    stream.setf(std::ios::fixed);
    stream.precision(6);
    stream << value;
    return stream.str();
}

std::vector<tag_t> CopyCurvesAsDumbGeometry(const std::vector<tag_t>& curveTags)
{
    std::vector<tag_t> copiedCurveTags;
    copiedCurveTags.reserve(curveTags.size());
    for (std::size_t index = 0; index < curveTags.size(); ++index)
    {
        const tag_t curveTag = curveTags[index];
        if (curveTag == NULL_TAG || UF_OBJ_ask_status(curveTag) != UF_OBJ_ALIVE)
        {
            continue;
        }

        const double curveLength = AskCurveLength(curveTag);
        if (curveLength <= kCurveLengthTolerance)
        {
            continue;
        }

        tag_t copiedCurveTag = CreateCurveSegmentByDistances(curveTag, 0.0, curveLength);
        if (copiedCurveTag != NULL_TAG)
        {
            copiedCurveTags.push_back(copiedCurveTag);
        }
    }
    return copiedCurveTags;
}

Sketch* FindSketchFromBuilderResult(NXObject* committedObject, const std::vector<NXObject*>& committedObjects, Features::Feature** sketchFeature)
{
    if (sketchFeature != NULL)
    {
        *sketchFeature = NULL;
    }

    Sketch* sketch = dynamic_cast<Sketch*>(committedObject);
    Features::SketchFeature* feature = dynamic_cast<Features::SketchFeature*>(committedObject);
    if (feature != NULL)
    {
        if (sketchFeature != NULL)
        {
            *sketchFeature = feature;
        }
        sketch = feature->Sketch();
    }

    for (std::size_t index = 0; sketch == NULL && index < committedObjects.size(); ++index)
    {
        sketch = dynamic_cast<Sketch*>(committedObjects[index]);
        feature = dynamic_cast<Features::SketchFeature*>(committedObjects[index]);
        if (feature != NULL)
        {
            if (sketchFeature != NULL)
            {
                *sketchFeature = feature;
            }
            sketch = feature->Sketch();
        }
    }

    return sketch;
}

Sketch* CreateGrooveProfileSketch(
    Part* workPart,
    const std::vector<tag_t>& sourceCurveTags,
    tag_t sketchFaceTag,
    const Point3d& origin,
    std::vector<IBaseCurve*>& sketchBaseCurves,
    std::vector<tag_t>& copiedCurveTags,
    Features::Feature** sketchFeature)
{
    sketchBaseCurves.clear();
    copiedCurveTags.clear();
    if (sketchFeature != NULL)
    {
        *sketchFeature = NULL;
    }

    if (workPart == NULL || sourceCurveTags.empty() || sketchFaceTag == NULL_TAG)
    {
        return NULL;
    }

    Face* sketchFace = dynamic_cast<Face*>(NXObjectManager::Get(sketchFaceTag));
    if (sketchFace == NULL)
    {
        return NULL;
    }

    copiedCurveTags = CopyCurvesAsDumbGeometry(sourceCurveTags);
    if (copiedCurveTags.empty())
    {
        return NULL;
    }

    Point* sketchOrigin = NULL;
    SketchInPlaceBuilder* sketchBuilder = NULL;
    Sketch* sketch = NULL;

    try
    {
        sketchOrigin = workPart->Points()->CreatePoint(origin);
        sketchBuilder = workPart->Sketches()->CreateSketchInPlaceBuilder2(NULL);
        if (sketchOrigin == NULL || sketchBuilder == NULL)
        {
            throw std::runtime_error("create sketch builder failed");
        }

        sketchBuilder->SetPlaneOption(Sketch::PlaneOptionExistingPlane);
        sketchBuilder->PlaneOrFace()->SetValue(dynamic_cast<ISurface*>(sketchFace));
        sketchBuilder->SetOriginOption(OriginMethodSpecifyPoint);
        sketchBuilder->SetSketchOrigin(sketchOrigin);

        NXObject* committedObject = sketchBuilder->Commit();
        const std::vector<NXObject*> committedObjects = sketchBuilder->GetCommittedObjects();
        sketch = FindSketchFromBuilderResult(committedObject, committedObjects, sketchFeature);
    }
    catch (...)
    {
        sketch = NULL;
    }

    if (sketchBuilder != NULL)
    {
        sketchBuilder->Destroy();
    }

    if (sketch == NULL)
    {
        DeleteObjects(copiedCurveTags);
        copiedCurveTags.clear();
        return NULL;
    }

    try
    {
        sketch->Activate(Sketch::ViewReorientFalse);
        for (std::size_t index = 0; index < copiedCurveTags.size(); ++index)
        {
            DisplayableObject* displayCurve =
                dynamic_cast<DisplayableObject*>(NXObjectManager::Get(copiedCurveTags[index]));
            if (displayCurve != NULL)
            {
                sketch->AddGeometry(displayCurve, Sketch::InferConstraintsOptionInferNoConstraints);
            }
        }
        sketch->Update();
        sketch->UpdateNavigator();
        sketch->Deactivate(Sketch::ViewReorientFalse, Sketch::UpdateLevelModel);

        const std::vector<NXObject*> sketchGeometry = sketch->GetAllGeometry();
        for (std::size_t index = 0; index < sketchGeometry.size(); ++index)
        {
            IBaseCurve* baseCurve = dynamic_cast<IBaseCurve*>(sketchGeometry[index]);
            if (baseCurve != NULL)
            {
                sketchBaseCurves.push_back(baseCurve);
            }
        }
    }
    catch (...)
    {
        try
        {
            if (sketch->IsActive())
            {
                sketch->Deactivate(Sketch::ViewReorientFalse, Sketch::UpdateLevelModel);
            }
        }
        catch (...)
        {
        }
        DeleteObjects(copiedCurveTags);
        copiedCurveTags.clear();
        sketchBaseCurves.clear();
        return NULL;
    }

    if (sketchBaseCurves.empty())
    {
        DeleteObjects(copiedCurveTags);
        copiedCurveTags.clear();
        Features::Feature* emptySketchFeature = sketchFeature != NULL ? *sketchFeature : sketch->Feature();
        if (emptySketchFeature != NULL)
        {
            DeleteObjects(std::vector<tag_t>{emptySketchFeature->Tag()});
        }
        return NULL;
    }

    return sketch;
}

int CreateShallowGrooveByExtrude(
    Part* workPart,
    const std::vector<tag_t>& grooveCurves,
    const ContactMatch& match,
    double grooveDepth,
    double grooveOffset)
{
    if (workPart == NULL || grooveCurves.empty() || match.targetBody == NULL_TAG || match.targetFace == NULL_TAG)
    {
        return 0;
    }

    double directionVector[3] = {0.0, 0.0, 0.0};
    if (!AskFaceNormal(match.targetFace, directionVector))
    {
        return 0;
    }

    double contactVector[3] = {
        match.targetPoint[0] - match.sourcePoint[0],
        match.targetPoint[1] - match.sourcePoint[1],
        match.targetPoint[2] - match.sourcePoint[2]};
    if (Normalize(contactVector) && Dot(directionVector, contactVector) < 0.0)
    {
        directionVector[0] = -directionVector[0];
        directionVector[1] = -directionVector[1];
        directionVector[2] = -directionVector[2];
    }

    double originPoint[3] = {0.0, 0.0, 0.0};
    if (!AskPlanarFaceMidpoint(match.targetFace, originPoint))
    {
        return 0;
    }

    Point3d origin(originPoint[0], originPoint[1], originPoint[2]);
    std::vector<IBaseCurve*> sketchBaseCurves;
    std::vector<tag_t> copiedCurveTags;
    Sketch* profileSketch = CreateGrooveProfileSketch(
        workPart,
        grooveCurves,
        match.targetFace,
        origin,
        sketchBaseCurves,
        copiedCurveTags,
        NULL);
    if (profileSketch == NULL || sketchBaseCurves.empty())
    {
        return 0;
    }

    const std::string depthFormula = ToFormulaString(grooveDepth);
    const std::string offsetFormula = ToFormulaString(grooveOffset);
    Body* targetBody = dynamic_cast<Body*>(NXObjectManager::Get(match.targetBody));
    if (targetBody == NULL)
    {
        return 0;
    }

    Features::ExtrudeBuilder* extrudeBuilder = workPart->Features()->CreateExtrudeBuilder(NULL);
    Section* section = workPart->Sections()->CreateSection(1.0e-5, 1.0e-5, 0.5);
    extrudeBuilder->SetSection(section);
    extrudeBuilder->AllowSelfIntersectingSection(true);
    extrudeBuilder->SetDistanceTolerance(1.0e-5);
    extrudeBuilder->BooleanOperation()->SetType(GeometricUtilities::BooleanOperation::BooleanTypeSubtract);
    extrudeBuilder->BooleanOperation()->SetTargetBodies(std::vector<Body*>(1, targetBody));
    extrudeBuilder->Limits()->SetSymmetricOption(true);
    extrudeBuilder->Limits()->StartExtend()->Value()->SetFormula(depthFormula.c_str());
    extrudeBuilder->Limits()->EndExtend()->Value()->SetFormula(depthFormula.c_str());
    extrudeBuilder->Limits()->StartExtend()->SetTrimType(GeometricUtilities::Extend::ExtendTypeSymmetric);
    extrudeBuilder->Limits()->EndExtend()->SetTrimType(GeometricUtilities::Extend::ExtendTypeSymmetric);
    extrudeBuilder->Offset()->SetOption(GeometricUtilities::TypeSymmetricOffset);
    extrudeBuilder->Offset()->StartOffset()->SetFormula(offsetFormula.c_str());
    extrudeBuilder->Offset()->EndOffset()->SetFormula(offsetFormula.c_str());
    extrudeBuilder->Draft()->SetDraftOption(GeometricUtilities::SimpleDraft::SimpleDraftTypeNoDraft);
    extrudeBuilder->FeatureOptions()->SetBodyType(GeometricUtilities::FeatureOptions::BodyStyleSolid);
    extrudeBuilder->SmartVolumeProfile()->SetOpenProfileSmartVolumeOption(false);
    extrudeBuilder->SmartVolumeProfile()->SetCloseProfileRule(
        GeometricUtilities::SmartVolumeProfileBuilder::CloseProfileRuleTypeFci);
    extrudeBuilder->SetParentFeatureInternal(true);

    section->SetDistanceTolerance(1.0e-5);
    section->SetChainingTolerance(1.0e-5);
    section->SetAllowedEntityTypes(Section::AllowTypesOnlyCurves);
    section->AllowSelfIntersection(true);
    section->AllowDegenerateCurves(false);

    SelectionIntentRuleOptions* ruleOptions = workPart->ScRuleFactory()->CreateRuleOptions();
    ruleOptions->SetSelectedFromInactive(false);

    std::vector<SelectionIntentRule*> rules;
    CurveDumbRule* curveRule = workPart->ScRuleFactory()->CreateRuleBaseCurveDumb(sketchBaseCurves, ruleOptions);
    if (curveRule != NULL)
    {
        rules.push_back(static_cast<SelectionIntentRule*>(curveRule));
    }
    delete ruleOptions;

    if (rules.empty())
    {
        extrudeBuilder->Destroy();
        return 0;
    }

    section->AddToSection(
        rules,
        NULL,
        NULL,
        NULL,
        origin,
        Section::ModeCreate,
        false);

    Features::Feature* extrudeFeature = NULL;
    for (int attempt = 0; attempt < 2 && extrudeFeature == NULL; ++attempt)
    {
        const double sign = attempt == 0 ? 1.0 : -1.0;
        Direction* direction = workPart->Directions()->CreateDirection(
            origin,
            Vector3d(directionVector[0] * sign, directionVector[1] * sign, directionVector[2] * sign),
            SmartObject::UpdateOptionWithinModeling);
        extrudeBuilder->SetDirection(direction);

        try
        {
            extrudeFeature = extrudeBuilder->CommitFeature();
        }
        catch (const NXException&)
        {
            extrudeFeature = NULL;
        }
    }

    extrudeBuilder->Destroy();
    if (extrudeFeature == NULL)
    {
        return 0;
    }

    try
    {
        extrudeFeature->MakeSketchInternal();
    }
    catch (...)
    {
    }

    MarkShallowGrooveFaces(extrudeFeature->Tag());
    DeleteObjects(grooveCurves);
    return 1;
}

std::vector<tag_t> CreateShadowCurveForContact(Part* workPart, Body* sourceBody, const ContactMatch& match)
{
    if (workPart == NULL || sourceBody == NULL || match.targetFace == NULL_TAG)
    {
        return std::vector<tag_t>();
    }

    Face* targetFace = dynamic_cast<Face*>(NXObjectManager::Get(match.targetFace));
    if (targetFace == NULL)
    {
        return std::vector<tag_t>();
    }

    Plane* targetPlane = CreatePlaneFromPlanarFace(workPart, match.targetFace);
    if (targetPlane == NULL)
    {
        return std::vector<tag_t>();
    }

    double targetNormal[3] = {0.0, 0.0, 0.0};
    if (!AskFaceNormal(match.sourceFace, targetNormal))
    {
        DeleteObjects(std::vector<tag_t>{targetPlane->Tag()});
        return std::vector<tag_t>();
    }

    Features::ShadowCurveBuilder* shadowCurveBuilder =
        workPart->Features()->CurveFeatureCollection()->CreateShadowCurveBuilder(NULL);

    shadowCurveBuilder->CurveSettings()->InputCurvesOption()->SetAssociative(false);
    shadowCurveBuilder->CurveSettings()->CurveFitData()->SetTolerance(0.0001);
    shadowCurveBuilder->CurveSettings()->CurveFitData()->SetAngleTolerance(0.5);
    shadowCurveBuilder->MaskingCurves()->SetDistanceTolerance(0.0001);
    shadowCurveBuilder->MaskingCurves()->SetChainingTolerance(0.0001);
    shadowCurveBuilder->MaskingCurves()->SetAngleTolerance(0.5);
    shadowCurveBuilder->SetAccuracyType(Features::ShadowCurveBuilder::AccuracyTypesStandard);
    shadowCurveBuilder->SetCurveLocationType(Features::ShadowCurveBuilder::CurveLocationTypesShadowonPlane);
    shadowCurveBuilder->SetCurveLocationPlane(targetPlane);
    shadowCurveBuilder->SetDistanceThreshold(0.001);
    shadowCurveBuilder->SetOptimizeCurveFlag(true);

    double directionVector[3] = {targetNormal[0], targetNormal[1], targetNormal[2]};
    Point3d directionOrigin(match.targetPoint[0], match.targetPoint[1], match.targetPoint[2]);
    Vector3d rayVector(directionVector[0], directionVector[1], directionVector[2]);
    Direction* rayDirection =
        workPart->Directions()->CreateDirection(directionOrigin, rayVector, SmartObject::UpdateOptionWithinModeling);
    shadowCurveBuilder->SetRayDirection(rayDirection);

    SelectionIntentRuleOptions* bodyRuleOptions = workPart->ScRuleFactory()->CreateRuleOptions();
    bodyRuleOptions->SetSelectedFromInactive(false);
    std::vector<Body*> bodies(1, sourceBody);
    BodyDumbRule* bodyRule = workPart->ScRuleFactory()->CreateRuleBodyDumb(bodies, true, bodyRuleOptions);
    delete bodyRuleOptions;
    std::vector<SelectionIntentRule*> bodyRules(1, bodyRule);
    shadowCurveBuilder->MaskingBodies()->ReplaceRules(bodyRules, false);

    const std::vector<tag_t> curvesBefore = AskAllCurveObjectsForSearch(workPart);
    NXObject* committedObject = shadowCurveBuilder->Commit();
    std::vector<NXObject*> committedObjects = shadowCurveBuilder->GetCommittedObjects();
    shadowCurveBuilder->Destroy();
    DeleteObjects(std::vector<tag_t>{targetPlane->Tag()});

    std::vector<tag_t> createdCurveTags;
    Features::ShadowCurve* feature = dynamic_cast<Features::ShadowCurve*>(committedObject);
    if (feature != NULL)
    {
        const std::vector<NXObject*> entities = feature->GetEntities();
        for (std::size_t i = 0; i < entities.size(); ++i)
        {
            Curve* curve = dynamic_cast<Curve*>(entities[i]);
            if (curve != NULL && curve->Tag() != NULL_TAG)
            {
                createdCurveTags.push_back(curve->Tag());
            }
        }
    }

    if (createdCurveTags.empty())
    {
        createdCurveTags.reserve(committedObjects.size());
        for (std::size_t i = 0; i < committedObjects.size(); ++i)
        {
            Curve* curve = dynamic_cast<Curve*>(committedObjects[i]);
            if (curve != NULL && curve->Tag() != NULL_TAG)
            {
                createdCurveTags.push_back(curve->Tag());
                continue;
            }

            Features::Feature* committedFeature = dynamic_cast<Features::Feature*>(committedObjects[i]);
            if (committedFeature != NULL)
            {
                const std::vector<NXObject*> entities = committedFeature->GetEntities();
                for (std::size_t entityIndex = 0; entityIndex < entities.size(); ++entityIndex)
                {
                    Curve* entityCurve = dynamic_cast<Curve*>(entities[entityIndex]);
                    if (entityCurve != NULL && entityCurve->Tag() != NULL_TAG)
                    {
                        createdCurveTags.push_back(entityCurve->Tag());
                    }
                }
            }
        }
    }

    if (createdCurveTags.empty())
    {
        const std::vector<tag_t> curvesAfter = AskAllCurveObjectsForSearch(workPart);
        std::set_difference(
            curvesAfter.begin(),
            curvesAfter.end(),
            curvesBefore.begin(),
            curvesBefore.end(),
            std::back_inserter(createdCurveTags));
    }

    std::sort(createdCurveTags.begin(), createdCurveTags.end());
    createdCurveTags.erase(std::unique(createdCurveTags.begin(), createdCurveTags.end()), createdCurveTags.end());

    std::vector<CurveEndData> curves;
    curves.reserve(createdCurveTags.size());

    for (std::size_t i = 0; i < createdCurveTags.size(); ++i)
    {
        Curve* curve = dynamic_cast<Curve*>(NXObjectManager::Get(createdCurveTags[i]));
        if (curve == NULL)
        {
            continue;
        }

        double parameterRange[2] = {0.0, 0.0};
        int periodicity = UF_CURVE_OPEN_CURVE;
        if (UF_CURVE_ask_parameterization(curve->Tag(), parameterRange, &periodicity) != 0)
        {
            continue;
        }

        CurveEndData data;
        data.curveTag = curve->Tag();
        double startEval[9] = {0.0};
        double midEval[9] = {0.0};
        double endEval[9] = {0.0};
        const double startParameter = parameterRange[0];
        const double endParameter = parameterRange[1];
        const double midParameter = 0.5 * (startParameter + endParameter);
        if (UF_CURVE_evaluate_curve(curve->Tag(), startParameter, UF_CURVE_LOC, startEval) != 0 ||
            UF_CURVE_evaluate_curve(curve->Tag(), midParameter, UF_CURVE_LOC, midEval) != 0 ||
            UF_CURVE_evaluate_curve(curve->Tag(), endParameter, UF_CURVE_LOC, endEval) != 0)
        {
            continue;
        }

        for (int axis = 0; axis < 3; ++axis)
        {
            data.start[axis] = startEval[axis];
            data.end[axis] = endEval[axis];
        }
        ExpandBounds(data.minCorner, data.maxCorner, startEval);
        ExpandBounds(data.minCorner, data.maxCorner, midEval);
        ExpandBounds(data.minCorner, data.maxCorner, endEval);
        curves.push_back(data);
    }

    if (curves.empty())
    {
        return std::vector<tag_t>();
    }

    if (curves.size() == 1)
    {
        return std::vector<tag_t>(1, curves[0].curveTag);
    }

    const double linkToleranceSquared = 0.05 * 0.05;
    std::vector<int> groupIndex(curves.size(), -1);
    std::vector<std::vector<std::size_t> > groups;

    for (std::size_t seed = 0; seed < curves.size(); ++seed)
    {
        if (groupIndex[seed] != -1)
        {
            continue;
        }

        const int currentGroup = static_cast<int>(groups.size());
        groups.push_back(std::vector<std::size_t>());
        std::vector<std::size_t> queue(1, seed);
        groupIndex[seed] = currentGroup;

        for (std::size_t cursor = 0; cursor < queue.size(); ++cursor)
        {
            const std::size_t curveIndex = queue[cursor];
            groups[currentGroup].push_back(curveIndex);

            for (std::size_t other = 0; other < curves.size(); ++other)
            {
                if (groupIndex[other] != -1)
                {
                    continue;
                }

                const bool connected =
                    DistanceSquared(curves[curveIndex].start, curves[other].start) <= linkToleranceSquared ||
                    DistanceSquared(curves[curveIndex].start, curves[other].end) <= linkToleranceSquared ||
                    DistanceSquared(curves[curveIndex].end, curves[other].start) <= linkToleranceSquared ||
                    DistanceSquared(curves[curveIndex].end, curves[other].end) <= linkToleranceSquared;
                if (!connected)
                {
                    continue;
                }

                groupIndex[other] = currentGroup;
                queue.push_back(other);
            }
        }
    }

    int bestGroup = -1;
    double bestScore = -1.0;
    for (std::size_t group = 0; group < groups.size(); ++group)
    {
        double minCorner[3] = {DBL_MAX, DBL_MAX, DBL_MAX};
        double maxCorner[3] = {-DBL_MAX, -DBL_MAX, -DBL_MAX};
        for (std::size_t idx = 0; idx < groups[group].size(); ++idx)
        {
            const CurveEndData& curve = curves[groups[group][idx]];
            ExpandBounds(minCorner, maxCorner, curve.minCorner);
            ExpandBounds(minCorner, maxCorner, curve.maxCorner);
        }

        const double dx = maxCorner[0] - minCorner[0];
        const double dy = maxCorner[1] - minCorner[1];
        const double dz = maxCorner[2] - minCorner[2];
        const double score = dx * dy + dy * dz + dx * dz;
        if (score > bestScore)
        {
            bestScore = score;
            bestGroup = static_cast<int>(group);
        }
    }

    std::vector<tag_t> visibleCurves;
    for (std::size_t i = 0; i < curves.size(); ++i)
    {
        if (groupIndex[i] == bestGroup)
        {
            UF_OBJ_set_blank_status(curves[i].curveTag, UF_OBJ_NOT_BLANKED);
            visibleCurves.push_back(curves[i].curveTag);
        }
        else
        {
            DeleteObjects(std::vector<tag_t>{curves[i].curveTag});
        }
    }

    return visibleCurves;
}

std::vector<tag_t> CreateCurvesFromEdges(const std::vector<tag_t>& edgeTags)
{
    std::vector<tag_t> createdCurves;
    for (std::size_t i = 0; i < edgeTags.size(); ++i)
    {
        tag_t curveTag = NULL_TAG;
        if (UF_MODL_create_curve_from_edge(edgeTags[i], &curveTag) != 0 || curveTag == NULL_TAG)
        {
            continue;
        }

        UF_OBJ_set_blank_status(curveTag, UF_OBJ_NOT_BLANKED);
        createdCurves.push_back(curveTag);
    }

    return createdCurves;
}

std::vector<tag_t> ProjectEdgeTagsToFace(const std::vector<tag_t>& edgeTags, const ContactMatch& match)
{
    std::vector<Edge*> edges;
    for (tag_t edgeTag : edgeTags)
    {
        Edge* edge = dynamic_cast<Edge*>(NXObjectManager::Get(edgeTag));
        if (edge != NULL)
        {
            edges.push_back(edge);
        }
    }

    Part* workPart = Session::GetSession() != NULL && Session::GetSession()->Parts() != NULL
        ? Session::GetSession()->Parts()->Work()
        : NULL;
    if (workPart == NULL)
    {
        return std::vector<tag_t>();
    }

    return ProjectSelectedEdgesToFace(workPart, edges, match);
}

std::vector<ContactMatch> AskUserToSelectContactMatches(const std::vector<ContactMatch>& matches)
{
    if (matches.size() <= 1)
    {
        return matches;
    }

    std::vector<tag_t> candidateFaces;
    std::unordered_set<tag_t> seenFaces;
    for (std::size_t i = 0; i < matches.size(); ++i)
    {
        if (matches[i].targetFace != NULL_TAG && seenFaces.insert(matches[i].targetFace).second)
        {
            candidateFaces.push_back(matches[i].targetFace);
        }
    }

    if (candidateFaces.size() <= 1)
    {
        return matches;
    }

    UI* ui = UI::GetUI();
    if (ui == NULL || ui->SelectionManager() == NULL)
    {
        return matches;
    }

    std::vector<NXOpen::Selection::MaskTriple> maskArray(
        1,
        NXOpen::Selection::MaskTriple(UF_solid_type, UF_all_subtype, UF_UI_SEL_FEATURE_ANY_FACE));
    std::vector<NXOpen::TaggedObject*> selectedObjects;
    NXOpen::Selection::Response response = ui->SelectionManager()->SelectTaggedObjects(
        ToUtf8(L"发现多个相贴面，请选择要标记的面，完成后点确定").c_str(),
        ToUtf8(L"选择标记面").c_str(),
        NXOpen::Selection::SelectionScopeWorkPart,
        NXOpen::Selection::SelectionActionClearAndEnableSpecific,
        false,
        true,
        maskArray,
        selectedObjects);

    if (!selectedObjects.empty())
    {
        std::vector<tag_t> selectedTags;
        selectedTags.reserve(selectedObjects.size());
        for (std::size_t i = 0; i < selectedObjects.size(); ++i)
        {
            if (selectedObjects[i] != NULL)
            {
                selectedTags.push_back(selectedObjects[i]->Tag());
            }
        }
        if (!selectedTags.empty())
        {
            UF_DISP_set_highlights(
                static_cast<int>(selectedTags.size()),
                selectedTags.data(),
                0);
        }
    }

    if (response != NXOpen::Selection::ResponseOk || selectedObjects.empty())
    {
        return std::vector<ContactMatch>();
    }

    FaceSelectionContext context;
    context.faceTags = candidateFaces;
    context.faceTagSet.insert(candidateFaces.begin(), candidateFaces.end());
    std::unordered_set<tag_t> selectedFaceSet;
    for (std::size_t i = 0; i < selectedObjects.size(); ++i)
    {
        if (selectedObjects[i] != NULL
            && context.faceTagSet.find(selectedObjects[i]->Tag()) != context.faceTagSet.end())
        {
            selectedFaceSet.insert(selectedObjects[i]->Tag());
        }
    }
    if (selectedFaceSet.empty())
    {
        return std::vector<ContactMatch>();
    }

    std::vector<ContactMatch> filteredMatches;
    for (std::size_t i = 0; i < matches.size(); ++i)
    {
        if (selectedFaceSet.find(matches[i].targetFace) != selectedFaceSet.end())
        {
            filteredMatches.push_back(matches[i]);
        }
    }
    return filteredMatches;
}

std::vector<tag_t> ProjectSelectedEdgesToFace(Part* workPart, const std::vector<Edge*>& selectedEdges, const ContactMatch& match)
{
    if (workPart == NULL || selectedEdges.empty() || match.targetFace == NULL_TAG)
    {
        return std::vector<tag_t>();
    }

    std::vector<tag_t> helperCurves;
    helperCurves.reserve(selectedEdges.size());
    for (std::size_t i = 0; i < selectedEdges.size(); ++i)
    {
        tag_t curveTag = NULL_TAG;
        if (UF_MODL_create_curve_from_edge(selectedEdges[i]->Tag(), &curveTag) == 0 && curveTag != NULL_TAG)
        {
            helperCurves.push_back(curveTag);
        }
    }

    if (helperCurves.empty())
    {
        return std::vector<tag_t>();
    }

    Plane* targetPlane = CreatePlaneFromPlanarFace(workPart, match.targetFace);
    if (targetPlane == NULL)
    {
        DeleteObjects(helperCurves);
        return std::vector<tag_t>();
    }

    std::vector<tag_t> projectedCurves;
    tag_t projectionGroup = NULL_TAG;
    int errorCode = 0;
    int groupError = 0;
    int memberCount = 0;
    tag_t* groupMembers = NULL;
    try
    {
        UF_CURVE_proj1_t projData{};
        UF_CURVE_init_proj_curves_data1(&projData);
        projData.proj_data.proj_type = 1;
        projData.join_type = UF_CURVE_NO_JOIN;

        tag_t targetPlanes[1] = {targetPlane->Tag()};
        errorCode = UF_CURVE_create_proj_curves1(
            static_cast<int>(helperCurves.size()),
            helperCurves.data(),
            1,
            targetPlanes,
            1,
            &projData,
            &projectionGroup);
        if (errorCode == 0 && projectionGroup != NULL_TAG)
        {
            groupError = UF_GROUP_ask_group_data(projectionGroup, &groupMembers, &memberCount);
            if (groupError == 0 && groupMembers != NULL)
            {
                projectedCurves.reserve(static_cast<std::size_t>(memberCount));
                for (int memberIndex = 0; memberIndex < memberCount; ++memberIndex)
                {
                    if (groupMembers[memberIndex] != NULL_TAG)
                    {
                        projectedCurves.push_back(groupMembers[memberIndex]);
                    }
                }
            }
        }
    }
    catch (...)
    {
    }

    if (groupMembers != NULL)
    {
        UF_free(groupMembers);
    }
    if (projectionGroup != NULL_TAG)
    {
        UF_GROUP_ungroup_top(projectionGroup);
    }
    DeleteObjects(std::vector<tag_t>{targetPlane->Tag()});
    DeleteObjects(helperCurves);

    std::sort(projectedCurves.begin(), projectedCurves.end());
    projectedCurves.erase(std::unique(projectedCurves.begin(), projectedCurves.end()), projectedCurves.end());
    for (std::size_t i = 0; i < projectedCurves.size(); ++i)
    {
        UF_OBJ_set_blank_status(projectedCurves[i], UF_OBJ_NOT_BLANKED);
    }

    return projectedCurves;
}

PreparedCurveSet ApplyPreSegmentRules(Part* workPart, const std::vector<tag_t>& curveTags, const ContactMatch& match)
{
    PreparedCurveSet result;
    const PreparedCurveGroupSet groupedResult = ApplyPreSegmentRulesToGroups(workPart, curveTags, match);
    result.curves = groupedResult.curves;
    result.retiredCurves = groupedResult.retiredCurves;
    return result;
}

PreparedCurveGroupSet ApplyPreSegmentRulesToGroups(Part* workPart, const std::vector<tag_t>& curveTags, const ContactMatch& match)
{
    if (curveTags.empty())
    {
        return PreparedCurveGroupSet();
    }

    return ApplyPreSegmentRulesToGroups(workPart, SplitCurvesIntoConnectedGroups(curveTags), match);
}

PreparedCurveGroupSet ApplyPreSegmentRulesToGroups(
    Part* workPart,
    const std::vector<std::vector<tag_t> >& projectedGroups,
    const ContactMatch& match)
{
    static_cast<void>(workPart);

    PreparedCurveGroupSet result;
    result.groups = projectedGroups;
    result.curves = FlattenCurveGroups(projectedGroups);

    if (projectedGroups.empty())
    {
        return result;
    }

    std::vector<tag_t> retiredCurves;
    std::vector<std::vector<tag_t> > roundedGroups;

    roundedGroups.reserve(projectedGroups.size());
    for (std::size_t groupIndex = 0; groupIndex < projectedGroups.size(); ++groupIndex)
    {
        PreparedCurveSet roundedFiltered = RemoveStraightEdgesWithoutRoundedCorners(projectedGroups[groupIndex]);
        retiredCurves.insert(
            retiredCurves.end(),
            roundedFiltered.retiredCurves.begin(),
            roundedFiltered.retiredCurves.end());
        roundedGroups.push_back(roundedFiltered.curves);
    }

    PreparedCurveGroupSet combinedTrimmed;
    if (!FlattenCurveGroups(roundedGroups).empty())
    {
        combinedTrimmed = TrimCurveGroupsByCombinedSafetyPreserveGroups(roundedGroups, match.targetFace, 1.0);
        retiredCurves.insert(
            retiredCurves.end(),
            combinedTrimmed.retiredCurves.begin(),
            combinedTrimmed.retiredCurves.end());
    }

    if (!retiredCurves.empty())
    {
        DeleteObjects(retiredCurves);
    }

    result.groups = combinedTrimmed.groups;
    result.curves = combinedTrimmed.curves;
    result.retiredCurves = retiredCurves;
    return result;
}
}

Session* BiaoJiXian_BJ::theSession = NULL;
UI* BiaoJiXian_BJ::theUI = NULL;

BiaoJiXian_BJ::BiaoJiXian_BJ()
    : theDlxFileName(NULL),
      theDialog(NULL),
      InputGroup(NULL),
      SourceBody(NULL),
      SourceEdges(NULL),
      MarkLineMode(NULL),
      OutputRuleGroup(NULL),
      MarkOutputMode(NULL),
      GrooveDepth(NULL),
      GrooveOffset(NULL),
      SegmentRuleGroup(NULL),
      SegmentLength(NULL),
      MaxMarkSpacing(NULL),
      ClosedRuleGroup(NULL),
      ClosedCurveRule(NULL),
      DisplayRuleGroup(NULL),
      CurveLayerMode(NULL),
      CurveLayerModeOption(NULL),
      CustomCurveLayer(NULL),
      CurveColorPicker(NULL),
      CurveLineFont(NULL)
{
    theSession = Session::GetSession();
    theUI = UI::GetUI();
    m_dlxPath = ResolveDialogPath();
    theDlxFileName = m_dlxPath.c_str();
    theDialog = theUI->CreateDialog(theDlxFileName);

    theDialog->AddInitializeHandler(make_callback(this, &BiaoJiXian_BJ::initialize_cb));
    theDialog->AddDialogShownHandler(make_callback(this, &BiaoJiXian_BJ::dialogShown_cb));
    theDialog->AddUpdateHandler(make_callback(this, &BiaoJiXian_BJ::update_cb));
    theDialog->AddApplyHandler(make_callback(this, &BiaoJiXian_BJ::apply_cb));
    theDialog->AddOkHandler(make_callback(this, &BiaoJiXian_BJ::ok_cb));
}

BiaoJiXian_BJ::~BiaoJiXian_BJ()
{
    if (theDialog != NULL)
    {
        delete theDialog;
        theDialog = NULL;
    }
}


#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace zhihui_license_guard
{
typedef int (__stdcall *EnsureAuthorizedProc)(const wchar_t*, const wchar_t*, wchar_t*, int);

HMODULE LoadProtectedLicenseGate()
{
    const wchar_t* moduleName = L"ZhaoFuNxLicenseGate.dll";

    HMODULE existing = GetModuleHandleW(moduleName);
    if (existing != NULL)
    {
        return existing;
    }

    HMODULE selfModule = NULL;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&LoadProtectedLicenseGate), &selfModule))
    {
        wchar_t localPath[MAX_PATH] = { 0 };
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

    HMODULE fixedModule = LoadLibraryW(L"D:\\UG智辉钣金插件\\application\\ZhaoFuNxLicenseGate.dll");
    if (fixedModule != NULL)
    {
        return fixedModule;
    }

    return LoadLibraryW(moduleName);
}

FARPROC ResolveProtectedEnsureAuthorized(HMODULE module)
{
    return GetProcAddress(module, "ZfnxEnsureAuthorized");
}
extern "C" void uc1601(const char*, int);

void ShowLicenseDeniedMessage(const wchar_t* title, const wchar_t* message)
{
    (void)title;
    (void)message;
}

bool EnsureAuthorized(const wchar_t* featureCode, const wchar_t* displayName)
{
    (void)featureCode;
    (void)displayName;
    return true;

    wchar_t message[1024] = { 0 };
    HMODULE module = LoadProtectedLicenseGate();
    if (module == NULL)
    {
        ShowLicenseDeniedMessage(displayName, L"License component was not found.");
        return false;
    }

    EnsureAuthorizedProc ensureAuthorized = reinterpret_cast<EnsureAuthorizedProc>(ResolveProtectedEnsureAuthorized(module));
    if (ensureAuthorized == NULL)
    {
        ShowLicenseDeniedMessage(displayName, L"License component entry is invalid.");
        return false;
    }

        const int ok = ensureAuthorized(featureCode, displayName, message, static_cast<int>(sizeof(message) / sizeof(message[0])));
    const int ok2 = ok == 1 ? ensureAuthorized(featureCode, displayName, message, static_cast<int>(sizeof(message) / sizeof(message[0]))) : ok;
    if (ok != 1 || ok2 != 1)
    {
        ShowLicenseDeniedMessage(displayName, message[0] != L'\0' ? message : L"License check failed.");
        return false;
    }

    return true;
}
}
extern "C" DllExport void ufusr(char* param, int* retcod, int param_len)
{
    if (!zhihui_license_guard::EnsureAuthorized(L"ZHIHUI.BIAOJIXIAN", L"BiaoJiXian"))
    {
        return;
    }

    static_cast<void>(param);
    static_cast<void>(retcod);
    static_cast<void>(param_len);

    BiaoJiXian_BJ* dialog = NULL;
    try
    {
        dialog = new BiaoJiXian_BJ();
        dialog->Launch();
    }
    catch (const std::exception& ex)
    {
        UI::GetUI()->NXMessageBox()->Show(
            NXString(ToUtf8(L"\u6807\u8BB0\u7EBF"), NXString::UTF8),
            NXMessageBox::DialogTypeError,
            NXString(ex.what(), NXString::Locale));
    }

    delete dialog;
}

extern "C" DllExport int ufusr_ask_unload()
{
    return static_cast<int>(Session::LibraryUnloadOptionImmediately);
}

extern "C" DllExport void ufusr_cleanup(void)
{
}

NXOpen::BlockStyler::BlockDialog::DialogResponse BiaoJiXian_BJ::Launch()
{
    return theDialog->Launch();
}

void BiaoJiXian_BJ::initialize_cb()
{
    InputGroup = dynamic_cast<NXOpen::BlockStyler::Group*>(theDialog->TopBlock()->FindBlock("InputGroup"));
    SourceBody = dynamic_cast<NXOpen::BlockStyler::SelectObject*>(theDialog->TopBlock()->FindBlock("SourceBody"));
    SourceEdges = dynamic_cast<NXOpen::BlockStyler::SelectObject*>(theDialog->TopBlock()->FindBlock("SourceEdges"));
    MarkLineMode = dynamic_cast<Enumeration*>(theDialog->TopBlock()->FindBlock("MarkLineMode"));
    OutputRuleGroup = dynamic_cast<NXOpen::BlockStyler::Group*>(theDialog->TopBlock()->FindBlock("OutputRuleGroup"));
    MarkOutputMode = dynamic_cast<Enumeration*>(theDialog->TopBlock()->FindBlock("MarkOutputMode"));
    GrooveDepth = dynamic_cast<DoubleBlock*>(theDialog->TopBlock()->FindBlock("GrooveDepth"));
    GrooveOffset = dynamic_cast<DoubleBlock*>(theDialog->TopBlock()->FindBlock("GrooveOffset"));
    SegmentRuleGroup = dynamic_cast<NXOpen::BlockStyler::Group*>(theDialog->TopBlock()->FindBlock("SegmentRuleGroup"));
    SegmentLength = dynamic_cast<DoubleBlock*>(theDialog->TopBlock()->FindBlock("SegmentLength"));
    MaxMarkSpacing = dynamic_cast<IntegerBlock*>(theDialog->TopBlock()->FindBlock("MaxMarkSpacing"));
    ClosedRuleGroup = dynamic_cast<NXOpen::BlockStyler::Group*>(theDialog->TopBlock()->FindBlock("ClosedRuleGroup"));
    ClosedCurveRule = dynamic_cast<Enumeration*>(theDialog->TopBlock()->FindBlock("ClosedCurveRule"));
    DisplayRuleGroup = dynamic_cast<NXOpen::BlockStyler::Group*>(theDialog->TopBlock()->FindBlock("DisplayRuleGroup"));
    CurveLayerMode = dynamic_cast<LayerBlock*>(theDialog->TopBlock()->FindBlock("CurveLayerMode"));
    CurveLayerModeOption = dynamic_cast<Enumeration*>(theDialog->TopBlock()->FindBlock("CurveLayerModeOption"));
    CustomCurveLayer = dynamic_cast<IntegerBlock*>(theDialog->TopBlock()->FindBlock("CustomCurveLayer"));
    CurveColorPicker = dynamic_cast<ObjectColorPicker*>(theDialog->TopBlock()->FindBlock("CurveColorPicker"));
    CurveLineFont = dynamic_cast<LineFont*>(theDialog->TopBlock()->FindBlock("CurveLineFont"));
    ConfigureSelectionFilters();
    RefreshUiState();
}

void BiaoJiXian_BJ::dialogShown_cb()
{
    RefreshUiState();
}

int BiaoJiXian_BJ::apply_cb()
{
    Session* session = Session::GetSession();
    Session::UndoMarkId runMarkId =
        session != NULL ? session->SetUndoMark(Session::MarkVisibilityVisible, "BiaoJiXian Run")
                        : static_cast<Session::UndoMarkId>(0);
    try
    {
        gFlatPatternFeatureCache.clear();

        const std::vector<Face*> selectedFaces = GetSelectedFaces();
        const std::vector<Edge*> selectedEdges = GetSelectedEdges();
        const int markMode = MarkLineMode != NULL ? GetEnumerationValue(MarkLineMode) : 0;
        const int outputMode = MarkOutputMode != NULL ? GetEnumerationValue(MarkOutputMode) : 0;
        Part* workPart = theSession != NULL && theSession->Parts() != NULL ? theSession->Parts()->Work() : NULL;
        SegmentParameters segmentParameters;
        double grooveDepth = 0.002;
        double grooveOffset = 0.002;
        int curveColor = 36;
        MarkCurveFont curveFont = MarkCurveFont::Solid;
        if (SegmentLength != NULL)
        {
            segmentParameters.segmentLength = GetDoubleValue(SegmentLength);
        }
        if (MaxMarkSpacing == NULL)
        {
            ShowError(ToUtf8(L"最大间隔距离控件未加载。"));
            if (session != NULL)
            {
                session->UndoToMark(runMarkId, "BiaoJiXian Run");
                session->DeleteUndoMark(runMarkId, "BiaoJiXian Run");
            }
            return 0;
        }
        segmentParameters.maxMarkSpacing = static_cast<double>(GetIntegerValue(MaxMarkSpacing));
        if (ClosedCurveRule != NULL)
        {
            segmentParameters.closedCurveRule = GetEnumerationValue(ClosedCurveRule);
        }
        if (GrooveDepth != NULL)
        {
            grooveDepth = GetDoubleValue(GrooveDepth);
        }
        if (GrooveOffset != NULL)
        {
            grooveOffset = GetDoubleValue(GrooveOffset);
        }
        if (CurveColorPicker != NULL)
        {
            curveColor = GetCurveColorValue(CurveColorPicker);
        }
        if (CurveLineFont != NULL)
        {
            curveFont = ParseCurveFontValue(CurveLineFont);
        }
        ApplyUnifiedMarkLineDisplay(curveColor, curveFont);

        if ((markMode == 2 && selectedEdges.empty()) ||
            (markMode != 2 && selectedFaces.empty()))
        {
            if (session != NULL)
            {
                session->DeleteUndoMark(runMarkId, "BiaoJiXian Run");
            }
            return 0;
        }

        auto askContinueWithoutFlatPattern = [&](bool& asked, bool& allowed) -> bool
        {
            if (!asked)
            {
                allowed = AskContinue(
                    ToUtf8(L"\u4F60\u9009\u62E9\u7684\u6295\u5F71\u9762\u6240\u5C5E\u5927\u4F53\u7F3A\u5C11\u5C55\u5F00\u56FE\u6837\uFF0C\u66F2\u7EBF\u4E0D\u4F1A\u8FFD\u52A0\u5230\u5C55\u5F00\u56FE\u6837\u4E0A\u3002\n\n\u662F\u5426\u7EE7\u7EED\uFF1F"));
                asked = true;
            }
            if (!allowed && session != NULL)
            {
                session->UndoToMark(runMarkId, "BiaoJiXian Run");
                session->DeleteUndoMark(runMarkId, "BiaoJiXian Run");
            }
            return allowed;
        };

        auto applyCurveOutputStyle = [&](const std::vector<tag_t>& curves, tag_t targetBodyTag)
        {
            ApplyCurveColorAndFont(curves, curveColor, curveFont);
            if (outputMode == 0)
            {
                ApplyCurveLayer(curves, ResolveCurveOutputLayer(CurveLayerModeOption, CustomCurveLayer, targetBodyTag));
            }
        };

        if (markMode == 0 && workPart != NULL)
        {
            bool askedContinueWithoutFlatPattern = false;
            bool continueWithoutFlatPattern = true;
            for (std::size_t i = 0; i < selectedFaces.size(); ++i)
            {
                const tag_t targetFaceTag = selectedFaces[i] != NULL ? selectedFaces[i]->Tag() : NULL_TAG;
                const tag_t targetBodyTag = AskFaceOwningSolidBody(targetFaceTag);
                if (targetBodyTag == NULL_TAG)
                {
                    continue;
                }

                const std::vector<ContactMatch> matches = ResolveContactsForTargetFace(targetFaceTag, workPart->Tag());
                std::vector<tag_t> flatCurvesForBody;
                std::vector<tag_t> projectedCurvesForFace;
                std::vector<std::vector<tag_t> > projectedCurveGroupsForFace;
                ContactMatch representativeMatch;
                bool hasRepresentativeMatch = false;
                for (std::size_t j = 0; j < matches.size(); ++j)
                {
                    const tag_t sourceBodyTag = AskFaceOwningSolidBody(matches[j].sourceFace);
                    Body* sourceBody = dynamic_cast<Body*>(NXObjectManager::Get(sourceBodyTag));
                    if (sourceBody == NULL)
                    {
                        continue;
                    }

                    const std::vector<tag_t> sourceCurves = CreateShadowCurveForContact(workPart, sourceBody, matches[j]);
                    if (!sourceCurves.empty() && !hasRepresentativeMatch)
                    {
                        representativeMatch = matches[j];
                        hasRepresentativeMatch = true;
                    }
                    if (!sourceCurves.empty())
                    {
                        projectedCurveGroupsForFace.push_back(sourceCurves);
                    }
                    projectedCurvesForFace.insert(
                        projectedCurvesForFace.end(),
                        sourceCurves.begin(),
                        sourceCurves.end());
                }

                if (projectedCurvesForFace.empty() || !hasRepresentativeMatch)
                {
                    continue;
                }

                if (kDebugStopAfterProjection)
                {
                    applyCurveOutputStyle(projectedCurvesForFace, targetBodyTag);
                    continue;
                }

                const PreparedCurveGroupSet preparedCurves =
                    ApplyPreSegmentRulesToGroups(workPart, projectedCurveGroupsForFace, representativeMatch);
                if (kDebugStopAfterPreSegment)
                {
                    applyCurveOutputStyle(preparedCurves.curves, targetBodyTag);
                    continue;
                }

                const std::vector<std::vector<tag_t> > finalCurveGroups =
                    BuildSegmentedMarkCurveGroups(preparedCurves.groups, segmentParameters);
                const std::vector<tag_t> finalCurves = FlattenCurveGroups(finalCurveGroups);
                applyCurveOutputStyle(finalCurves, targetBodyTag);
                if (kDebugStopAfterSegmentation)
                {
                    continue;
                }

                if (outputMode == 0)
                {
                    flatCurvesForBody.insert(flatCurvesForBody.end(), finalCurves.begin(), finalCurves.end());
                }
                if (outputMode == 1)
                {
                    CreateShallowGrooveByExtrude(workPart, finalCurves, representativeMatch, grooveDepth, grooveOffset);
                }
                if (outputMode == 0 && !flatCurvesForBody.empty())
                {
                    Features::Feature* flatPatternFeature =
                        AskPreferredFlatPatternFeature(workPart, matches.empty() ? NULL_TAG : matches.front().targetBody);
                    if (flatPatternFeature == NULL)
                    {
                        if (!askContinueWithoutFlatPattern(askedContinueWithoutFlatPattern, continueWithoutFlatPattern))
                        {
                            return 0;
                        }
                        continue;
                    }
                    const std::vector<tag_t> flatOutputCurves =
                        AppendCurvesToFlatPatternAddedGeometry(workPart, flatPatternFeature, flatCurvesForBody);
                    applyCurveOutputStyle(flatOutputCurves, matches.empty() ? targetBodyTag : matches.front().targetBody);
                }
            }
        }
        else if (markMode == 1 && workPart != NULL)
        {
            bool askedContinueWithoutFlatPattern = false;
            bool continueWithoutFlatPattern = true;
            for (std::size_t i = 0; i < selectedFaces.size(); ++i)
            {
                const tag_t targetFaceTag = selectedFaces[i] != NULL ? selectedFaces[i]->Tag() : NULL_TAG;
                const tag_t targetBodyTag = AskFaceOwningSolidBody(targetFaceTag);
                if (targetBodyTag == NULL_TAG)
                {
                    continue;
                }

                const std::vector<ContactMatch> matches = ResolveContactsForTargetFace(targetFaceTag, workPart->Tag());
                std::vector<tag_t> flatCurvesForBody;
                std::vector<tag_t> projectedCurvesForFace;
                std::vector<std::vector<tag_t> > projectedCurveGroupsForFace;
                ContactMatch representativeMatch;
                bool hasRepresentativeMatch = false;
                for (std::size_t j = 0; j < matches.size(); ++j)
                {
                    const std::vector<tag_t> peripheralEdges = AskPeripheralLoopEdges(matches[j].sourceFace);
                    const std::vector<tag_t> sourceCurves = ProjectEdgeTagsToFace(peripheralEdges, matches[j]);
                    if (!sourceCurves.empty() && !hasRepresentativeMatch)
                    {
                        representativeMatch = matches[j];
                        hasRepresentativeMatch = true;
                    }
                    if (!sourceCurves.empty())
                    {
                        projectedCurveGroupsForFace.push_back(sourceCurves);
                    }
                    projectedCurvesForFace.insert(
                        projectedCurvesForFace.end(),
                        sourceCurves.begin(),
                        sourceCurves.end());
                }

                if (projectedCurvesForFace.empty() || !hasRepresentativeMatch)
                {
                    continue;
                }

                if (kDebugStopAfterProjection)
                {
                    applyCurveOutputStyle(projectedCurvesForFace, targetBodyTag);
                    continue;
                }

                const PreparedCurveGroupSet preparedCurves =
                    ApplyPreSegmentRulesToGroups(workPart, projectedCurveGroupsForFace, representativeMatch);
                if (kDebugStopAfterPreSegment)
                {
                    applyCurveOutputStyle(preparedCurves.curves, targetBodyTag);
                    continue;
                }

                const std::vector<std::vector<tag_t> > finalCurveGroups =
                    BuildSegmentedMarkCurveGroups(preparedCurves.groups, segmentParameters);
                const std::vector<tag_t> finalCurves = FlattenCurveGroups(finalCurveGroups);
                applyCurveOutputStyle(finalCurves, targetBodyTag);
                if (kDebugStopAfterSegmentation)
                {
                    continue;
                }

                if (outputMode == 0)
                {
                    flatCurvesForBody.insert(flatCurvesForBody.end(), finalCurves.begin(), finalCurves.end());
                }
                if (outputMode == 1)
                {
                    CreateShallowGrooveByExtrude(workPart, finalCurves, representativeMatch, grooveDepth, grooveOffset);
                }
                if (outputMode == 0 && !flatCurvesForBody.empty())
                {
                    Features::Feature* flatPatternFeature =
                        AskPreferredFlatPatternFeature(workPart, matches.empty() ? NULL_TAG : matches.front().targetBody);
                    if (flatPatternFeature == NULL)
                    {
                        if (!askContinueWithoutFlatPattern(askedContinueWithoutFlatPattern, continueWithoutFlatPattern))
                        {
                            return 0;
                        }
                        continue;
                    }
                    const std::vector<tag_t> flatOutputCurves =
                        AppendCurvesToFlatPatternAddedGeometry(workPart, flatPatternFeature, flatCurvesForBody);
                    applyCurveOutputStyle(flatOutputCurves, matches.empty() ? targetBodyTag : matches.front().targetBody);
                }
            }
        }
        else if (markMode == 2 && workPart != NULL)
        {
            bool askedContinueWithoutFlatPattern = false;
            bool continueWithoutFlatPattern = true;
            if (!selectedEdges.empty())
            {
                tag_t sourceBodyTag = NULL_TAG;
                UF_MODL_ask_edge_body(selectedEdges[0]->Tag(), &sourceBodyTag);
                const std::vector<ContactMatch> matches =
                    ResolveMatchesForMarking(FindAllBottomContacts(sourceBodyTag, workPart->Tag()), true);
                for (std::size_t i = 0; i < matches.size(); ++i)
                {
                    std::vector<tag_t> projectedCurvesForTarget;
                    std::vector<std::vector<tag_t> > projectedCurveGroupsForTarget;
                    for (std::size_t edgeIndex = 0; edgeIndex < selectedEdges.size(); ++edgeIndex)
                    {
                        std::vector<Edge*> singleEdge(1, selectedEdges[edgeIndex]);
                        const std::vector<tag_t> sourceCurves = ProjectSelectedEdgesToFace(workPart, singleEdge, matches[i]);
                        if (!sourceCurves.empty())
                        {
                            projectedCurveGroupsForTarget.push_back(sourceCurves);
                        }
                        projectedCurvesForTarget.insert(
                            projectedCurvesForTarget.end(),
                            sourceCurves.begin(),
                            sourceCurves.end());
                    }

                    if (projectedCurvesForTarget.empty())
                    {
                        continue;
                    }

                    if (kDebugStopAfterProjection)
                    {
                        applyCurveOutputStyle(projectedCurvesForTarget, matches[i].targetBody);
                        continue;
                    }

                    const PreparedCurveGroupSet preparedCurves =
                        ApplyPreSegmentRulesToGroups(workPart, projectedCurveGroupsForTarget, matches[i]);
                    if (kDebugStopAfterPreSegment)
                    {
                        applyCurveOutputStyle(preparedCurves.curves, matches[i].targetBody);
                        continue;
                    }

                    const std::vector<std::vector<tag_t> > finalCurveGroups =
                        BuildSegmentedMarkCurveGroups(preparedCurves.groups, segmentParameters);
                    const std::vector<tag_t> finalCurves = FlattenCurveGroups(finalCurveGroups);
                    applyCurveOutputStyle(finalCurves, matches[i].targetBody);
                    if (kDebugStopAfterSegmentation)
                    {
                        continue;
                    }

                    if (outputMode == 1)
                    {
                        CreateShallowGrooveByExtrude(
                            workPart,
                            finalCurves,
                            matches[i],
                            grooveDepth,
                            grooveOffset);
                    }
                    else
                    {
                        Features::Feature* flatPatternFeature =
                            AskPreferredFlatPatternFeature(workPart, matches[i].targetBody);
                        if (flatPatternFeature == NULL)
                        {
                            if (!askContinueWithoutFlatPattern(askedContinueWithoutFlatPattern, continueWithoutFlatPattern))
                            {
                                return 0;
                            }
                            continue;
                        }
                        const std::vector<tag_t> flatOutputCurves =
                            AppendCurvesToFlatPatternAddedGeometry(workPart, flatPatternFeature, finalCurves);
                        applyCurveOutputStyle(flatOutputCurves, matches[i].targetBody);
                    }
                }
            }
        }
        else
        {
        }

        if (session != NULL)
        {
            session->DeleteUndoMark(runMarkId, "BiaoJiXian Run");
        }
        return 0;
    }
    catch (const std::exception& ex)
    {
        ShowError(ex.what());
        if (session != NULL)
        {
            session->UndoToMark(runMarkId, "BiaoJiXian Run");
            session->DeleteUndoMark(runMarkId, "BiaoJiXian Run");
        }
        return 1;
    }
}

int BiaoJiXian_BJ::ok_cb()
{
    return apply_cb();
}

int BiaoJiXian_BJ::update_cb(NXOpen::BlockStyler::UIBlock* block)
{
    if (block == SourceBody)
    {
        NormalizeFaceSelection();
    }
    if (block == MarkLineMode || block == MarkOutputMode || block == CurveLayerModeOption)
    {
        RefreshUiState();
    }
    static_cast<void>(block);
    return 0;
}

PropertyList* BiaoJiXian_BJ::GetBlockProperties(const char* blockID)
{
    return theDialog->GetBlockProperties(blockID);
}

void BiaoJiXian_BJ::RefreshUiState()
{
    if (MarkLineMode == NULL || SourceBody == NULL || SourceEdges == NULL)
    {
        return;
    }

    const int markMode = GetEnumerationValue(MarkLineMode);
    const bool isEdgeMode = (markMode == 2);
    const int outputMode = MarkOutputMode != NULL ? GetEnumerationValue(MarkOutputMode) : 0;
    const bool isCurveOutput = (outputMode == 0);

    SetShow(SourceBody, !isEdgeMode);
    SetShow(SourceEdges, isEdgeMode);
    if (DisplayRuleGroup != NULL)
    {
        SetShow(DisplayRuleGroup, true);
        SetEnable(DisplayRuleGroup, true);
    }
    if (CurveLayerMode != NULL)
    {
        SetShow(CurveLayerMode, false);
        SetEnable(CurveLayerMode, false);
    }
    if (CurveLayerModeOption != NULL)
    {
        SetShow(CurveLayerModeOption, isCurveOutput);
        SetEnable(CurveLayerModeOption, isCurveOutput);
    }
    if (CustomCurveLayer != NULL)
    {
        const bool showCustomLayer = isCurveOutput && CurveLayerModeOption != NULL && GetEnumerationValue(CurveLayerModeOption) == 1;
        SetShow(CustomCurveLayer, showCustomLayer);
        SetEnable(CustomCurveLayer, showCustomLayer);
    }
}

void BiaoJiXian_BJ::ConfigureSelectionFilters()
{
    ConfigureFaceFilter();
    ConfigureEdgeFilter();
}

void BiaoJiXian_BJ::ConfigureFaceFilter() const
{
    if (SourceBody == NULL)
    {
        return;
    }

    try
    {
        SourceBody->SetMaximumScopeAsString("Within Work Part Only");
        SourceBody->SetSelectModeAsString("Single");
        SourceBody->SetInterpartSelectionAsString("Simple");
        SourceBody->SetAllowConvergentObject(false);

        PropertyList* properties = SourceBody->GetProperties();
        Selection::SelectionAction action = Selection::SelectionActionClearAndEnableSpecific;
        std::vector<Selection::MaskTriple> maskArray(1);
        maskArray[0] = Selection::MaskTriple(UF_solid_type, UF_solid_body_subtype, UF_UI_SEL_FEATURE_ANY_FACE);
        properties->SetSelectionFilter("SelectionFilter", action, maskArray);
        delete properties;
    }
    catch (...)
    {
    }
}

void BiaoJiXian_BJ::ConfigureEdgeFilter() const
{
    if (SourceEdges == NULL)
    {
        return;
    }

    PropertyList* properties = SourceEdges->GetProperties();
    Selection::SelectionAction action = Selection::SelectionActionClearAndEnableSpecific;
    std::vector<Selection::MaskTriple> maskArray(1);
    maskArray[0] = Selection::MaskTriple(UF_solid_type, UF_solid_body_subtype, UF_UI_SEL_FEATURE_ANY_EDGE);
    properties->SetSelectionFilter("SelectionFilter", action, maskArray);
    delete properties;
}

void BiaoJiXian_BJ::NormalizeFaceSelection() const
{
    if (SourceBody == NULL)
    {
        return;
    }

    const std::vector<NXOpen::TaggedObject*> selectedObjects = SourceBody->GetSelectedObjects();
    if (selectedObjects.empty())
    {
        return;
    }

    std::vector<NXOpen::TaggedObject*> planarFaces;
    planarFaces.reserve(1);

    for (std::vector<NXOpen::TaggedObject*>::const_iterator it = selectedObjects.begin(); it != selectedObjects.end(); ++it)
    {
        NXOpen::Face* face = dynamic_cast<NXOpen::Face*>(*it);
        if (face == NULL)
        {
            continue;
        }

        double normal[3] = {0.0};
        if (AskFaceOwningSolidBody(face->Tag()) != NULL_TAG && AskFaceNormal(face->Tag(), normal))
        {
            planarFaces.push_back(face);
            break;
        }
    }

    if (planarFaces.size() != selectedObjects.size())
    {
        SourceBody->SetSelectedObjects(planarFaces);
    }
}

int BiaoJiXian_BJ::GetEnumerationValue(NXOpen::BlockStyler::Enumeration* block) const
{
    PropertyList* properties = block->GetProperties();
    const int value = properties->GetEnum("Value");
    delete properties;
    return value;
}

double BiaoJiXian_BJ::GetDoubleValue(NXOpen::BlockStyler::DoubleBlock* block) const
{
    PropertyList* properties = block->GetProperties();
    const double value = properties->GetDouble("Value");
    delete properties;
    return value;
}

int BiaoJiXian_BJ::GetIntegerValue(NXOpen::BlockStyler::IntegerBlock* block) const
{
    PropertyList* properties = block->GetProperties();
    const int value = properties->GetInteger("Value");
    delete properties;
    return value;
}

void BiaoJiXian_BJ::SetShow(NXOpen::BlockStyler::UIBlock* block, bool show) const
{
    PropertyList* properties = block->GetProperties();
    properties->SetLogical("Show", show);
    delete properties;
}

void BiaoJiXian_BJ::SetEnable(NXOpen::BlockStyler::UIBlock* block, bool enable) const
{
    PropertyList* properties = block->GetProperties();
    properties->SetLogical("Enable", enable);
    delete properties;
}

std::vector<NXOpen::Face*> BiaoJiXian_BJ::GetSelectedFaces() const
{
    std::vector<NXOpen::Face*> faces;
    if (SourceBody == NULL)
    {
        return faces;
    }

    std::vector<NXOpen::TaggedObject*> selectedObjects = SourceBody->GetSelectedObjects();
    for (std::vector<NXOpen::TaggedObject*>::const_iterator it = selectedObjects.begin(); it != selectedObjects.end(); ++it)
    {
        NXOpen::Face* face = dynamic_cast<NXOpen::Face*>(*it);
        if (face != NULL)
        {
            double normal[3] = {0.0};
            if (AskFaceOwningSolidBody(face->Tag()) != NULL_TAG && AskFaceNormal(face->Tag(), normal))
            {
                faces.push_back(face);
                break;
            }
        }
    }
    return faces;
}

std::vector<NXOpen::Edge*> BiaoJiXian_BJ::GetSelectedEdges() const
{
    std::vector<NXOpen::Edge*> edges;
    if (SourceEdges == NULL)
    {
        return edges;
    }

    std::vector<NXOpen::TaggedObject*> selectedObjects = SourceEdges->GetSelectedObjects();
    for (std::vector<NXOpen::TaggedObject*>::const_iterator it = selectedObjects.begin(); it != selectedObjects.end(); ++it)
    {
        NXOpen::Edge* edge = dynamic_cast<NXOpen::Edge*>(*it);
        if (edge != NULL)
        {
            edges.push_back(edge);
        }
    }
    return edges;
}

void BiaoJiXian_BJ::ShowInfo(const std::string& message) const
{
    theUI->NXMessageBox()->Show(
        NXString(ToUtf8(L"\u6807\u8BB0\u7EBF"), NXString::UTF8),
        NXMessageBox::DialogTypeInformation,
        NXString(message, NXString::UTF8));
}

void BiaoJiXian_BJ::ShowError(const std::string& message) const
{
    theUI->NXMessageBox()->Show(
        NXString(ToUtf8(L"\u6807\u8BB0\u7EBF"), NXString::UTF8),
        NXMessageBox::DialogTypeError,
        NXString(message, NXString::UTF8));
}

bool BiaoJiXian_BJ::AskContinue(const std::string& message) const
{
    const int answer = theUI->NXMessageBox()->Show(
        NXString(ToUtf8(L"\u6807\u8BB0\u7EBF"), NXString::UTF8),
        NXMessageBox::DialogTypeQuestion,
        NXString(message, NXString::UTF8));
    return answer == 1;
}

