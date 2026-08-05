#include "AutoCreateThreeViews.hpp"
#include "../../common/ZhihuiEmbeddedDialog.hpp"
#include "embedded_dialog_resources.h"

#ifdef CreateDialog
#undef CreateDialog
#endif

#include <NXOpen/BlockStyler_PropertyList.hxx>
#include <stdexcept>
#include <NXOpen/Body.hxx>
#include <NXOpen/BodyCollection.hxx>
#include <NXOpen/CoordinateSystem.hxx>
#include <NXOpen/DraftingManager.hxx>
#include <NXOpen/Direction.hxx>
#include <NXOpen/DirectionCollection.hxx>
#include <NXOpen/ColorManager.hxx>
#include <NXOpen/ModelingView.hxx>
#include <NXOpen/ModelingViewCollection.hxx>
#include <NXOpen/NXColor.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXMessageBox.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/SelectNXObject.hxx>
#include <NXOpen/ViewCollection.hxx>
#include <NXOpen/Drawings_BaseView.hxx>
#include <NXOpen/Drawings_BaseViewBuilder.hxx>
#include <NXOpen/Drawings_DraftingDrawingSheet.hxx>
#include <NXOpen/Drawings_DraftingDrawingSheetBuilder.hxx>
#include <NXOpen/Drawings_DraftingDrawingSheetCollection.hxx>
#include <NXOpen/Drawings_DraftingBody.hxx>
#include <NXOpen/Drawings_DraftingBodyCollection.hxx>
#include <NXOpen/Drawings_DraftingCurve.hxx>
#include <NXOpen/Drawings_DraftingCurveCollection.hxx>
#include <NXOpen/Drawings_DraftingComponentSelectionBuilder.hxx>
#include <NXOpen/Drawings_DraftingView.hxx>
#include <NXOpen/Drawings_DraftingViewCollection.hxx>
#include <NXOpen/Drawings_DrawingSheet.hxx>
#include <NXOpen/Drawings_DrawingSheetBuilder.hxx>
#include <NXOpen/Drawings_DrawingSheetCollection.hxx>
#include <NXOpen/Drawings_ProjectedView.hxx>
#include <NXOpen/Drawings_ProjectedViewBuilder.hxx>
#include <NXOpen/Drawings_SelectDraftingView.hxx>
#include <NXOpen/Drawings_SheetDraftingViewCollection.hxx>
#include <NXOpen/Drawings_ViewPlacementBuilder.hxx>
#include <NXOpen/Drawings_ViewScaleBuilder.hxx>
#include <NXOpen/Drawings_ViewStyleBuilder.hxx>
#include <NXOpen/Drawings_ViewStyleSmoothEdgesBuilder.hxx>
#include <NXOpen/Drawings_ViewStyleVirtualIntersectionsBuilder.hxx>
#include <NXOpen/Drawings_OrientationViewStyle.hxx>
#include <NXOpen/Annotations_AnnotationManager.hxx>
#include <NXOpen/Annotations_LineArrowStyleBuilder.hxx>
#include <NXOpen/Annotations_StyleBuilder.hxx>
#include <NXOpen/Annotations_BaseAngularDimension.hxx>
#include <NXOpen/Annotations_BaseAngularDimensionBuilder.hxx>
#include <NXOpen/Annotations_DimensionCollection.hxx>
#include <NXOpen/Annotations_DimensionMeasurementBuilder.hxx>
#include <NXOpen/Annotations_DimensionStyleBuilder.hxx>
#include <NXOpen/Annotations_DraftingNoteBuilder.hxx>
#include <NXOpen/Annotations_LetteringStyleBuilder.hxx>
#include <NXOpen/Annotations_MinorAngularDimensionBuilder.hxx>
#include <NXOpen/Annotations_OriginBuilder.hxx>
#include <NXOpen/Annotations_PlaneBuilder.hxx>
#include <NXOpen/Annotations_RapidDimensionBuilder.hxx>
#include <NXOpen/Annotations_RadialDimensionBuilder.hxx>
#include <NXOpen/Annotations_SimpleDraftingAid.hxx>
#include <NXOpen/Annotations_TextWithEditControlsBuilder.hxx>
#include <NXOpen/Annotations_TextWithSymbolsBuilder.hxx>
#include <NXOpen/DisplayManager.hxx>
#include <NXOpen/Drafting_PreferencesBuilder.hxx>
#include <NXOpen/Drafting_SettingsManager.hxx>
#include <NXOpen/DisplayModification.hxx>
#include <NXOpen/DisplayableObject.hxx>
#include <NXOpen/Layer.hxx>
#include <NXOpen/Layer_LayerManager.hxx>
#include <NXOpen/Edge.hxx>
#include <NXOpen/Expression.hxx>
#include <NXOpen/ExpressionCollection.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/Features_Feature.hxx>
#include <NXOpen/Features_FeatureCollection.hxx>
#include <NXOpen/Features_FlatPattern.hxx>
#include <NXOpen/Features_SheetMetal_FlatPatternBuilder.hxx>
#include <NXOpen/Features_SheetMetal_SheetmetalManager.hxx>
#include <NXOpen/FontCollection.hxx>
#include <NXOpen/IParameterizedSurface.hxx>
#include <NXOpen/MeasureFaces.hxx>
#include <NXOpen/MeasureManager.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/SelectEdge.hxx>
#include <NXOpen/SelectFace.hxx>
#include <NXOpen/SelectDisplayableObject.hxx>
#include <NXOpen/Selection.hxx>
#include <NXOpen/SmartObject.hxx>
#include <NXOpen/TaggedObject.hxx>
#include <NXOpen/Unit.hxx>
#include <NXOpen/UnitCollection.hxx>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <ctime>
#include <fstream>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <limits>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <uf_draw.h>
#include <uf_drf.h>
#include <uf_disp.h>
#include <uf.h>
#include <uf_curve.h>
#include <uf_eval.h>
#include <uf_assem.h>
#include <uf_layer.h>
#include <uf_modl.h>
#include <uf_modl_utilities.h>
#include <uf_obj.h>
#include <uf_object_types.h>
#include <uf_part.h>
#include <uf_ui_types.h>
#include <uf_view.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifdef CreateDialog
#undef CreateDialog
#endif

namespace
{
struct RequestValues
{
    std::string drawingTargetMode = "partOrAssembly";
    int targetLayer = 0;
    int layerIndex = 0;
    int layersPerSheet = 1;
    bool appendToCurrentSheet = false;
    std::string templatePath;
    bool inheritDraftingPreferences = true;
    std::string layerLayoutMode = "auto";
    std::string frontDirectionMode = "largestFaceLongestEdge";
    double viewSpacing = 20.0;
    double sheetMargin = 20.0;
    bool firstAngle = true;
    bool front = true;
    bool top = false;
    bool bottom = false;
    bool left = false;
    bool right = false;
    bool back = false;
    bool backBottom = false;
    bool iso = true;
    bool flat = true;
    bool autoDimensions = true;
    bool dimensionOverall = true;
    bool dimensionLinear = false;
    bool dimensionAngle = true;
    bool dimensionHole = true;
    bool dimensionHoleLocation = true;
    bool dimensionInnerClosedCurve = true;
    bool auxiliaryAutoCompact = false;
    bool assemblyDrawing = false;
    std::string isoCorner = "TopLeft";
    std::string flatCorner = "BottomRight";
    bool technicalRequirementEnabled = false;
    bool technicalRequirementIndexed = true;
    std::string technicalRequirementCorner = "TopLeft";
    std::string technicalRequirementText;
};

int g_activeTargetLayer = 0;

class ScopedTargetDrawingLayer
{
public:
    explicit ScopedTargetDrawingLayer(int layer)
        : previous_(g_activeTargetLayer)
    {
        g_activeTargetLayer = layer;
    }

    ~ScopedTargetDrawingLayer()
    {
        g_activeTargetLayer = previous_;
    }

private:
    int previous_ = 0;
};

struct PlannedView
{
    std::string label;
    std::string modelViewName;
    NXOpen::Point3d point;
    bool required;
};

struct CreatedView
{
    std::string label;
    NXOpen::Point3d plannedPoint;
    NXOpen::Drawings::DraftingView* view = nullptr;
};

struct CreatedAuxiliaryView
{
    std::string label;
    std::string corner;
    NXOpen::Drawings::DraftingView* view = nullptr;
};

struct ModelBounds
{
    double width = 160.0;
    double height = 100.0;
    double sizeX = 160.0;
    double sizeY = 100.0;
    double sizeZ = 20.0;
    bool valid = false;
};

std::vector<tag_t> CollectVisibleSolidBodyTags(NXOpen::Part* part);

void AccumulateBodyBounds(
    tag_t bodyTag,
    bool& initialized,
    double minValues[3],
    double maxValues[3])
{
    if (bodyTag == NULL_TAG)
    {
        return;
    }

    double box[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    if (UF_MODL_ask_bounding_box(bodyTag, box) != 0)
    {
        return;
    }

    if (!initialized)
    {
        minValues[0] = box[0];
        minValues[1] = box[1];
        minValues[2] = box[2];
        maxValues[0] = box[3];
        maxValues[1] = box[4];
        maxValues[2] = box[5];
        initialized = true;
        return;
    }

    minValues[0] = std::min(minValues[0], box[0]);
    minValues[1] = std::min(minValues[1], box[1]);
    minValues[2] = std::min(minValues[2], box[2]);
    maxValues[0] = std::max(maxValues[0], box[3]);
    maxValues[1] = std::max(maxValues[1], box[4]);
    maxValues[2] = std::max(maxValues[2], box[5]);
}

struct LayoutBounds
{
    double minX = 0.0;
    double minY = 0.0;
    double maxX = 0.0;
    double maxY = 0.0;
};

struct DraftingCurveExtent
{
    NXOpen::Drawings::DraftingCurve* curve = nullptr;
    tag_t tag = NULL_TAG;
    LayoutBounds bounds;
    bool initialized = false;
};

struct ClosedCurveSegment
{
    DraftingCurveExtent extent;
    NXOpen::Point3d startModel = NXOpen::Point3d(0.0, 0.0, 0.0);
    NXOpen::Point3d endModel = NXOpen::Point3d(0.0, 0.0, 0.0);
    double startX = 0.0;
    double startY = 0.0;
    double endX = 0.0;
    double endY = 0.0;
};

struct ClosedCurveLoopCandidate
{
    std::vector<DraftingCurveExtent> extents;
    LayoutBounds bounds;
    double enclosedArea = 0.0;
    bool initialized = false;
};

struct ClosedCurveDimensionRecord
{
    bool measuresX = true;
    double minMeasure = 0.0;
    double maxMeasure = 0.0;
    double minCross = 0.0;
    double maxCross = 0.0;
};

struct CurveAssocCandidate
{
    NXOpen::Drawings::DraftingCurve* curve = nullptr;
    NXOpen::InferSnapType::SnapType snapType = NXOpen::InferSnapType::SnapTypeStart;
    NXOpen::Point3d modelPoint = NXOpen::Point3d(0.0, 0.0, 0.0);
    double drawingX = 0.0;
    double drawingY = 0.0;
};

struct HoleCircleCandidate
{
    NXOpen::Drawings::DraftingCurve* curve = nullptr;
    double modelRadius = 0.0;
    double drawingRadius = 0.0;
    NXOpen::Point3d pickPoint = NXOpen::Point3d(0.0, 0.0, 0.0);
    NXOpen::Point3d centerPoint = NXOpen::Point3d(0.0, 0.0, 0.0);
};

struct HoleArcSegmentCandidate
{
    DraftingCurveExtent extent;
    double modelRadius = 0.0;
    double drawingRadius = 0.0;
    NXOpen::Point3d centerPoint = NXOpen::Point3d(0.0, 0.0, 0.0);
    NXOpen::Point3d pickPoint = NXOpen::Point3d(0.0, 0.0, 0.0);
    double startAngle = 0.0;
    double endAngle = 0.0;
};

struct HoleRuleRecord
{
    std::string annotationType;
    std::string series;
    std::string size;
    std::string threadSpec;
    std::string lengthText;
    double bottomHole = 0.0;
    std::string displayText;
    std::string standardComment;
    std::string note;
};

void WriteLine(NXOpen::Session* session, const std::string& message);
std::string PartResultName(NXOpen::Part* part);
void AppendRunResultLine(const std::string& line);
std::string BuildRunResultText();

std::vector<std::string> g_runResultLines;

struct LineSegmentCandidate
{
    NXOpen::Drawings::DraftingCurve* curve = nullptr;
    NXOpen::Point3d startModel = NXOpen::Point3d(0.0, 0.0, 0.0);
    NXOpen::Point3d endModel = NXOpen::Point3d(0.0, 0.0, 0.0);
    double startX = 0.0;
    double startY = 0.0;
    double endX = 0.0;
    double endY = 0.0;
    double minX = 0.0;
    double maxX = 0.0;
    double minY = 0.0;
    double maxY = 0.0;
    double length = 0.0;
};

struct LineProjectionFaceCandidate
{
    tag_t faceTag = NULL_TAG;
    tag_t bodyTag = NULL_TAG;
    NXOpen::Vector3d normal = NXOpen::Vector3d(0.0, 0.0, 0.0);
    NXOpen::Point3d planePoint = NXOpen::Point3d(0.0, 0.0, 0.0);
    LineSegmentCandidate line;
    bool horizontalLine = false;
    bool verticalLine = false;
    bool angledLine = false;
    double directionX = 1.0;
    double directionY = 0.0;
    double normalX = 0.0;
    double normalY = 1.0;
    double lineOffset = 0.0;
    double minAlong = 0.0;
    double maxAlong = 0.0;
    double minX = 0.0;
    double maxX = 0.0;
    double minY = 0.0;
    double maxY = 0.0;
    double centerX = 0.0;
    double centerY = 0.0;
    double length = 0.0;
    bool outerContourDatum = false;
};

struct ShallowDetailFilterCache
{
    NXOpen::Part* part = nullptr;
    bool initialized = false;
    std::set<tag_t> shallowFaceTags;
    std::map<tag_t, std::vector<tag_t>> edgeFaceTags;
};

bool DraftingCurveHasShallowDetailParent(
    NXOpen::Part* part,
    ShallowDetailFilterCache& cache,
    const DraftingCurveExtent& extent);
std::vector<DraftingCurveExtent> CollectDraftingCurveExtents(NXOpen::Drawings::DraftingView* view);
double BoundsWidth(const LayoutBounds& bounds);
double BoundsHeight(const LayoutBounds& bounds);
double BoundsArea(const LayoutBounds& bounds);
bool TryBuildVisibleCurveBounds(const std::vector<DraftingCurveExtent>& extents, LayoutBounds& bounds);

struct FacePlaneSignature
{
    long long nx = 0;
    long long ny = 0;
    long long nz = 0;
    long long distance = 0;
    bool valid = false;
};

struct FacePairKey
{
    tag_t first = NULL_TAG;
    tag_t second = NULL_TAG;
    FacePlaneSignature firstPlane;
    FacePlaneSignature secondPlane;
    bool hasPlane = false;
};

struct MainViewFacePairRules
{
    NXOpen::Vector3d axes[3] = {
        NXOpen::Vector3d(1.0, 0.0, 0.0),
        NXOpen::Vector3d(0.0, 1.0, 0.0),
        NXOpen::Vector3d(0.0, 0.0, 1.0)};
    NXOpen::Point3d center = NXOpen::Point3d(0.0, 0.0, 0.0);
    bool valid = false;
    std::vector<FacePlaneSignature> usedPairedPlanes;
};

struct AutoViewDirection
{
    NXOpen::Vector3d normal = NXOpen::Vector3d(0.0, 0.0, 1.0);
    NXOpen::Vector3d xDirection = NXOpen::Vector3d(1.0, 0.0, 0.0);
    std::string normalName = "Z";
    std::string xName = "X";
    std::string source;
    double faceArea = 0.0;
    double edgeLength = 0.0;
    tag_t faceTag = NULL_TAG;
    tag_t edgeTag = NULL_TAG;
    bool valid = false;
};

std::filesystem::path CurrentModuleDirectory()
{
    HMODULE module = nullptr;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(&CurrentModuleDirectory),
                           &module))
    {
        wchar_t moduleFileName[MAX_PATH] = {0};
        const DWORD length = GetModuleFileNameW(module, moduleFileName, MAX_PATH);
        if (length > 0 && length < MAX_PATH)
        {
            return std::filesystem::path(moduleFileName).parent_path();
        }
    }

    return std::filesystem::path(__FILE__).parent_path();
}

std::string Trim(const std::string& value)
{
    const std::string whitespace = " \t\r\n";
    const size_t first = value.find_first_not_of(whitespace);
    if (first == std::string::npos)
    {
        return "";
    }
    const size_t last = value.find_last_not_of(whitespace);
    return value.substr(first, last - first + 1);
}

std::string ToLowerAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string RemoveUtf8Bom(const std::string& value)
{
    if (value.size() >= 3 &&
        static_cast<unsigned char>(value[0]) == 0xEF &&
        static_cast<unsigned char>(value[1]) == 0xBB &&
        static_cast<unsigned char>(value[2]) == 0xBF)
    {
        return value.substr(3);
    }
    return value;
}

void ReplaceAllText(std::string& text, const std::string& from, const std::string& to)
{
    if (from.empty())
    {
        return;
    }

    size_t pos = 0;
    while ((pos = text.find(from, pos)) != std::string::npos)
    {
        text.replace(pos, from.size(), to);
        pos += to.size();
    }
}

std::string DecodeConfigEscapes(const std::string& value)
{
    std::string result;
    bool escape = false;
    for (const char ch : value)
    {
        if (escape)
        {
            if (ch == 'n')
            {
                result += '\n';
            }
            else if (ch == 't')
            {
                result += '\t';
            }
            else if (ch != 'r')
            {
                result += ch;
            }
            escape = false;
            continue;
        }

        if (ch == '\\')
        {
            escape = true;
            continue;
        }
        result += ch;
    }

    if (escape)
    {
        result += '\\';
    }
    return result;
}

std::string FormatRealForNote(double value)
{
    if (std::fabs(value - std::round(value)) < 1.0e-6)
    {
        return std::to_string(static_cast<long long>(std::llround(value)));
    }

    std::ostringstream stream;
    stream.setf(std::ios::fixed);
    stream.precision(3);
    stream << value;
    std::string text = stream.str();
    while (!text.empty() && text.back() == '0')
    {
        text.pop_back();
    }
    if (!text.empty() && text.back() == '.')
    {
        text.pop_back();
    }
    return text;
}

bool ContainsAnyKeyword(const std::string& text, const std::vector<std::string>& keywords)
{
    const std::string lower = ToLowerAscii(text);
    for (const std::string& keyword : keywords)
    {
        if (lower.find(ToLowerAscii(keyword)) != std::string::npos)
        {
            return true;
        }
    }

    return false;
}

std::string ReadAttributeText(NXOpen::NXObject* object, const char* title)
{
    if (object == nullptr || title == nullptr)
    {
        return "";
    }

    try
    {
        if (object->HasUserAttribute(title, NXOpen::NXObject::AttributeType::AttributeTypeString, -1))
        {
            return Trim(object->GetStringAttribute(title).GetLocaleText());
        }
        if (object->HasUserAttribute(title, NXOpen::NXObject::AttributeType::AttributeTypeInteger, -1))
        {
            return std::to_string(object->GetIntegerAttribute(title));
        }
        if (object->HasUserAttribute(title, NXOpen::NXObject::AttributeType::AttributeTypeReal, -1))
        {
            return FormatRealForNote(object->GetRealAttribute(title));
        }
    }
    catch (...)
    {
    }
    return "";
}

bool HasAnyUserAttribute(NXOpen::NXObject* object, const char* title)
{
    if (object == nullptr || title == nullptr)
    {
        return false;
    }

    try
    {
        return object->HasUserAttribute(title, NXOpen::NXObject::AttributeType::AttributeTypeString, -1) ||
               object->HasUserAttribute(title, NXOpen::NXObject::AttributeType::AttributeTypeInteger, -1) ||
               object->HasUserAttribute(title, NXOpen::NXObject::AttributeType::AttributeTypeReal, -1);
    }
    catch (...)
    {
        return false;
    }
}

bool IsSafeAttributeReferenceObjectName(const std::string& name)
{
    if (name.empty())
    {
        return false;
    }

    for (const char ch : name)
    {
        const unsigned char value = static_cast<unsigned char>(ch);
        if (value >= 128 || std::isspace(value) || ch == '<' || ch == '>' || ch == '@')
        {
            return false;
        }
    }
    return true;
}

std::string ObjectAttributeReferenceText(NXOpen::NXObject* object, const char* title)
{
    if (object == nullptr || title == nullptr)
    {
        return "";
    }

    try
    {
        if (!HasAnyUserAttribute(object, title))
        {
            return "";
        }

        std::string objectName = object->Name().GetLocaleText();
        if (!IsSafeAttributeReferenceObjectName(objectName))
        {
            char generatedName[64] = {};
            sprintf_s(generatedName, "ZDCT_BODY_%u", static_cast<unsigned int>(object->Tag()));
            object->SetName(generatedName);
            objectName = generatedName;
        }

        return std::string("<W") + objectName + "@" + title + ">";
    }
    catch (...)
    {
        return "";
    }
}

std::string PartAttributeReferenceText(NXOpen::Part* part, const char* title)
{
    if (!HasAnyUserAttribute(part, title))
    {
        return "";
    }

    return std::string("<W@") + title + ">";
}

std::string ExpressionReferenceText(NXOpen::Part* part, const std::string& expressionName)
{
    if (part == nullptr || expressionName.empty())
    {
        return "";
    }

    try
    {
        NXOpen::Expression* expression = part->Expressions()->FindObject(expressionName.c_str());
        if (expression == nullptr)
        {
            return "";
        }
        return std::string("<X0.0@") + expressionName + ">";
    }
    catch (...)
    {
        return "";
    }
}

NXOpen::Body* FirstSolidBody(NXOpen::Part* part)
{
    if (part == nullptr || part->Bodies() == nullptr)
    {
        return nullptr;
    }

    for (NXOpen::BodyCollection::iterator it = part->Bodies()->begin(); it != part->Bodies()->end(); ++it)
    {
        NXOpen::Body* body = *it;
        if (body != nullptr)
        {
            return body;
        }
    }
    return nullptr;
}

std::string ReadBodyOrPartAttribute(NXOpen::Part* part, NXOpen::Body* body, const std::vector<const char*>& names)
{
    for (const char* name : names)
    {
        std::string value = ReadAttributeText(body, name);
        if (!value.empty())
        {
            return value;
        }
    }

    for (const char* name : names)
    {
        std::string value = ReadAttributeText(part, name);
        if (!value.empty())
        {
            return value;
        }
    }

    return "";
}

std::string ReadBodyQuantityText(NXOpen::Part* part, NXOpen::Body* body)
{
    if (part != nullptr && body != nullptr)
    {
        try
        {
            const int bodyId = body->GetIntegerAttribute("BodyID");
            char expressionName[128] = {};
            sprintf_s(expressionName, "ZSuLian_%d", bodyId);
            NXOpen::Expression* expression = part->Expressions()->FindObject(expressionName);
            if (expression != nullptr)
            {
                return std::to_string(expression->IntegerValue());
            }
        }
        catch (...)
        {
        }
    }

    return ReadBodyOrPartAttribute(part, body, {"sulian", "数量", "qty", "Qty", "QTY"});
}

std::string ReadBodyQuantityReferenceText(NXOpen::Part* part, NXOpen::Body* body)
{
    if (part != nullptr && body != nullptr)
    {
        try
        {
            const int bodyId = body->GetIntegerAttribute("BodyID");
            char expressionName[128] = {};
            sprintf_s(expressionName, "ZSuLian_%d", bodyId);
            const std::string expressionReference = ExpressionReferenceText(part, expressionName);
            if (!expressionReference.empty())
            {
                return expressionReference;
            }
        }
        catch (...)
        {
        }
    }

    return "";
}

std::string ReadBodyThicknessText(NXOpen::Part* part, NXOpen::Body* body)
{
    return ReadBodyOrPartAttribute(part, body, {"Z", "厚度", "Thickness", "THICKNESS", "t"});
}

bool TryReadFlatNoteFormatLine(const std::string& rawLine, std::string& formatText)
{
    std::string line = Trim(RemoveUtf8Bom(rawLine));
    if (line.empty() || line[0] == '#' || line[0] == ';')
    {
        return false;
    }
    if (line.front() == '[' && line.back() == ']')
    {
        return false;
    }

    const std::string key = "body_note_format";
    if (line.compare(0, key.size(), key) == 0)
    {
        const size_t equalPos = line.find('=');
        if (equalPos != std::string::npos)
        {
            line = Trim(line.substr(equalPos + 1));
        }
    }

    if (line.empty())
    {
        return false;
    }

    formatText = DecodeConfigEscapes(line);
    return !formatText.empty();
}

std::string LoadFlatPatternNoteFormat()
{
    static const char* defaultFormat = "{\xE7\xBC\x96\xE5\x8F\xB7=}{\xE6\x9D\x90\xE6\x96\x99} T={\xE5\x8E\x9A\xE5\xBA\xA6} {\xE6\x95\xB0\xE9\x87\x8F}PCS{\xE9\x95\x9C\xE5\x83\x8F}";
    const std::vector<std::filesystem::path> candidates = {
        std::filesystem::path("D:\\UG智辉钣金插件\\config\\ZiDonCuTu_note_format.ini")};

    for (const std::filesystem::path& path : candidates)
    {
        try
        {
            std::ifstream file(path, std::ios::binary);
            if (!file)
            {
                continue;
            }

            std::string line;
            std::string formatText;
            while (std::getline(file, line))
            {
                if (TryReadFlatNoteFormatLine(line, formatText))
                {
                    return formatText;
                }
            }
        }
        catch (...)
        {
        }
    }

    return defaultFormat;
}

std::vector<NXOpen::NXString> BuildDraftNoteLines(const std::string& noteText, bool leadingBlank = true)
{
    std::vector<NXOpen::NXString> lines;
    if (leadingBlank)
    {
        lines.push_back("");
    }

    std::string current;
    for (const char ch : noteText)
    {
        if (ch == '\r')
        {
            continue;
        }
        if (ch == '\n')
        {
            lines.push_back(NXOpen::NXString(current.c_str(), NXOpen::NXString::UTF8));
            current.clear();
            continue;
        }
        current += ch;
    }
    lines.push_back(NXOpen::NXString(current.c_str(), NXOpen::NXString::UTF8));
    return lines;
}

std::string BuildFlatPatternNoteText(NXOpen::Part* part)
{
    NXOpen::Body* body = FirstSolidBody(part);
    const std::string material = ObjectAttributeReferenceText(body, "cailiao");
    const std::string thickness = ObjectAttributeReferenceText(body, "Z");
    const std::string quantity = ReadBodyQuantityReferenceText(part, body);
    const std::string mirror = ObjectAttributeReferenceText(body, "MIRR");
    const std::string number = ObjectAttributeReferenceText(body, "bianhao");

    std::string text = LoadFlatPatternNoteFormat();
    ReplaceAllText(text, "{\xE7\xBC\x96\xE5\x8F\xB7=}", number.empty() ? "" : number + "=");
    ReplaceAllText(text, "{\xE7\xBC\x96\xE5\x8F\xB7}", number);
    ReplaceAllText(text, "{\xE6\x9D\x90\xE6\x96\x99}", material);
    ReplaceAllText(text, "{\xE5\x8E\x9A\xE5\xBA\xA6}", thickness);
    ReplaceAllText(text, "{\xE6\x95\xB0\xE9\x87\x8F}", quantity);
    ReplaceAllText(text, "{\xE9\x95\x9C\xE5\x83\x8F}", mirror);
    return Trim(text);
}

bool HasHoleRuleContent(const HoleRuleRecord& rule)
{
    return !Trim(rule.annotationType).empty() ||
           !Trim(rule.series).empty() ||
           !Trim(rule.size).empty() ||
           !Trim(rule.threadSpec).empty() ||
           !Trim(rule.lengthText).empty() ||
           rule.bottomHole > 0.0 ||
           !Trim(rule.displayText).empty() ||
           !Trim(rule.standardComment).empty() ||
           !Trim(rule.note).empty();
}

void ApplyHoleRuleField(HoleRuleRecord& rule, const std::string& key, const std::string& value)
{
    const std::string decoded = DecodeConfigEscapes(value);
    if (key == "annotationType")
    {
        rule.annotationType = decoded;
    }
    else if (key == "series")
    {
        rule.series = decoded;
    }
    else if (key == "size")
    {
        rule.size = decoded;
    }
    else if (key == "threadSpec")
    {
        rule.threadSpec = decoded;
    }
    else if (key == "lengthText")
    {
        rule.lengthText = decoded;
    }
    else if (key == "displayText")
    {
        rule.displayText = decoded;
    }
    else if (key == "standardComment")
    {
        rule.standardComment = decoded;
    }
    else if (key == "note")
    {
        rule.note = decoded;
    }
    else if (key == "bottomHole")
    {
        try
        {
            rule.bottomHole = std::stod(Trim(decoded));
        }
        catch (...)
        {
            rule.bottomHole = 0.0;
        }
    }
}

std::vector<HoleRuleRecord> LoadHoleRulesFromIni(const std::filesystem::path& path)
{
    std::vector<HoleRuleRecord> rules;
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        return rules;
    }

    HoleRuleRecord current;
    bool inRule = false;
    std::string rawLine;
    while (std::getline(input, rawLine))
    {
        std::string line = Trim(RemoveUtf8Bom(rawLine));
        if (line.empty() || line[0] == ';' || line[0] == '#')
        {
            continue;
        }
        if (line.front() == '[' && line.back() == ']')
        {
            if (inRule && HasHoleRuleContent(current))
            {
                rules.push_back(current);
            }
            current = HoleRuleRecord();
            inRule = true;
            continue;
        }

        const size_t equal = line.find('=');
        if (equal == std::string::npos || !inRule)
        {
            continue;
        }
        ApplyHoleRuleField(current, Trim(line.substr(0, equal)), Trim(line.substr(equal + 1)));
    }

    if (inRule && HasHoleRuleContent(current))
    {
        rules.push_back(current);
    }
    return rules;
}

std::vector<HoleRuleRecord> BuildFallbackHoleRules()
{
    const HoleRuleRecord defaults[] = {
        {"螺牙标注", "公制粗牙", "M2.5", "M2.5", "", 2.10, "攻牙 {规格}", "标准公制粗牙底孔", "标准公制粗牙底孔"},
        {"螺牙标注", "公制粗牙", "M3", "M3x0.5", "", 2.50, "攻牙 {规格}", "标准公制粗牙底孔", "标准公制粗牙底孔"},
        {"螺牙标注", "公制粗牙", "M4", "M4x0.7", "", 3.30, "攻牙 {规格}", "标准公制粗牙底孔", "标准公制粗牙底孔"},
        {"螺牙标注", "公制粗牙", "M5", "M5x0.8", "", 4.20, "攻牙 {规格}", "标准公制粗牙底孔", "标准公制粗牙底孔"},
        {"螺牙标注", "公制粗牙", "M6", "M6x1.0", "", 5.00, "攻牙 {规格}", "标准公制粗牙底孔", "标准公制粗牙底孔"},
        {"螺牙标注", "公制粗牙", "M8", "M8x1.25", "", 6.80, "攻牙 {规格}", "标准公制粗牙底孔", "标准公制粗牙底孔"},
        {"螺牙标注", "公制粗牙", "M10", "M10", "", 8.50, "攻牙 {规格}", "标准公制粗牙底孔", "标准公制粗牙底孔"},
        {"螺牙标注", "公制粗牙", "M12", "M12", "", 10.30, "攻牙 {规格}", "标准公制粗牙底孔", "标准公制粗牙底孔"},
        {"沉孔标注", "标准沉孔", "Φ4", "标准沉孔-Φ4", "", 4.00, "沉孔 {规格}", "可按企业标准继续补充沉孔规格和注释", "可按企业标准继续补充沉孔规格和注释"},
        {"沉孔标注", "标准沉孔", "Φ5", "标准沉孔-Φ5", "", 5.00, "沉孔 {规格}", "可按企业标准继续补充沉孔规格和注释", "可按企业标准继续补充沉孔规格和注释"},
        {"沉孔标注", "标准沉孔", "Φ6", "标准沉孔-Φ6", "", 6.00, "沉孔 {规格}", "可按企业标准继续补充沉孔规格和注释", "可按企业标准继续补充沉孔规格和注释"}};
    return std::vector<HoleRuleRecord>(defaults, defaults + (sizeof(defaults) / sizeof(defaults[0])));
}

std::vector<std::filesystem::path> HoleRuleCandidatePaths()
{
    return {
        std::filesystem::path("D:\\UG智辉钣金插件\\config\\KonBiaoZuRules.ini")};
}

const std::vector<HoleRuleRecord>& HoleRules(NXOpen::Session* session)
{
    static std::vector<HoleRuleRecord> rules;
    static bool loaded = false;
    if (loaded)
    {
        return rules;
    }
    loaded = true;

    for (const std::filesystem::path& path : HoleRuleCandidatePaths())
    {
        try
        {
            if (!std::filesystem::exists(path))
            {
                continue;
            }
            rules = LoadHoleRulesFromIni(path);
            if (!rules.empty())
            {
                std::ostringstream log;
                log << "AutoCreateThreeViews: hole rules loaded"
                    << " path=" << path.string()
                    << ", count=" << rules.size() << ".";
                WriteLine(session, log.str());
                return rules;
            }
        }
        catch (...)
        {
        }
    }

    rules = BuildFallbackHoleRules();
    std::ostringstream log;
    log << "AutoCreateThreeViews: hole rules fallback used, count=" << rules.size() << ".";
    WriteLine(session, log.str());
    return rules;
}

std::string ApplyHoleRuleTemplate(std::string text, const HoleRuleRecord& rule)
{
    ReplaceAllText(text, "{子类型}", rule.series);
    ReplaceAllText(text, "{规格}", rule.size);
    ReplaceAllText(text, "{螺纹}", rule.threadSpec);
    ReplaceAllText(text, "{底孔}", FormatRealForNote(rule.bottomHole));
    ReplaceAllText(text, "{长度}", rule.lengthText);
    return Trim(text);
}

int HoleRulePriority(const HoleRuleRecord& rule)
{
    if (rule.annotationType == "孔标注")
    {
        return 0;
    }
    if (rule.annotationType == "螺牙标注")
    {
        return 1;
    }
    if (rule.annotationType == "压铆螺母标注")
    {
        return 2;
    }
    if (rule.annotationType == "焊接螺母标注")
    {
        return 3;
    }
    if (rule.annotationType == "压铆螺钉标注")
    {
        return 4;
    }
    if (rule.annotationType == "压铆螺母柱标注")
    {
        return 5;
    }
    if (rule.annotationType == "沉孔标注")
    {
        return 6;
    }
    return 9;
}

std::string HoleRuleDisplayText(const HoleRuleRecord& rule)
{
    std::string text = ApplyHoleRuleTemplate(rule.displayText, rule);
    if (!text.empty())
    {
        return text;
    }
    if (!rule.threadSpec.empty())
    {
        return rule.threadSpec;
    }
    return rule.size;
}

bool ResolveHoleRule(
    NXOpen::Session* session,
    double diameter,
    double tolerance,
    HoleRuleRecord& matched,
    int& candidateCount)
{
    candidateCount = 0;
    bool found = false;
    for (const HoleRuleRecord& rule : HoleRules(session))
    {
        if (rule.bottomHole <= 0.0 || std::abs(rule.bottomHole - diameter) > tolerance)
        {
            continue;
        }
        ++candidateCount;
        if (!found ||
            HoleRulePriority(rule) < HoleRulePriority(matched) ||
            (HoleRulePriority(rule) == HoleRulePriority(matched) && rule.annotationType < matched.annotationType))
        {
            matched = rule;
            found = true;
        }
    }
    return found;
}

bool IsTappedHoleRule(const HoleRuleRecord& rule)
{
    const std::string tappedType = "\xE8\x9E\xBA\xE7\x89\x99"; // 螺牙
    const std::string tappedText = "\xE6\x94\xBB\xE7\x89\x99"; // 攻牙
    return rule.annotationType.find(tappedType) != std::string::npos ||
           rule.displayText.find(tappedText) != std::string::npos ||
           rule.note.find(tappedText) != std::string::npos;
}

bool ResolveTappedHoleRule(
    NXOpen::Session* session,
    double diameter,
    double tolerance,
    HoleRuleRecord& matched,
    int& candidateCount)
{
    candidateCount = 0;
    bool found = false;
    for (const HoleRuleRecord& rule : HoleRules(session))
    {
        if (!IsTappedHoleRule(rule) ||
            rule.bottomHole <= 0.0 ||
            std::abs(rule.bottomHole - diameter) > tolerance)
        {
            continue;
        }

        ++candidateCount;
        if (!found ||
            HoleRulePriority(rule) < HoleRulePriority(matched) ||
            (HoleRulePriority(rule) == HoleRulePriority(matched) && rule.annotationType < matched.annotationType))
        {
            matched = rule;
            found = true;
        }
    }

    return found;
}

std::string BuildHoleDiameterPrefix(
    NXOpen::Session* session,
    double diameter,
    int sameDiameterCount)
{
    std::ostringstream prefix;
    if (sameDiameterCount > 1)
    {
        prefix << sameDiameterCount << "-";
    }

    HoleRuleRecord rule;
    int candidateCount = 0;
    if (ResolveTappedHoleRule(session, diameter, 0.02, rule, candidateCount))
    {
        const std::string ruleText = HoleRuleDisplayText(rule);
        prefix << ruleText;

        std::ostringstream log;
        log << "AutoCreateThreeViews: hole rule matched"
            << " diameter=" << diameter
            << ", count=" << sameDiameterCount
            << ", candidates=" << candidateCount
            << ", type=" << rule.annotationType
            << ", series=" << rule.series
            << ", size=" << rule.size
            << ", text=" << ruleText << ".";
        WriteLine(session, log.str());
    }
    else
    {
        prefix << "\xE5\xAD\x94"; // 孔

        std::ostringstream log;
        log << "AutoCreateThreeViews: hole rule not matched"
            << " diameter=" << diameter
            << ", count=" << sameDiameterCount
            << "; use plain hole diameter text.";
        WriteLine(session, log.str());
    }

    return Trim(prefix.str());
}

bool TryBuildTappedHoleDiameterPrefix(
    NXOpen::Session* session,
    double diameter,
    int sameDiameterCount,
    std::string& beforeText)
{
    HoleRuleRecord rule;
    int candidateCount = 0;
    if (!ResolveTappedHoleRule(session, diameter, 0.02, rule, candidateCount))
    {
        std::ostringstream log;
        log << "AutoCreateThreeViews: tapped hole rule not matched"
            << " diameter=" << diameter
            << ", count=" << sameDiameterCount
            << "; use plain diameter annotation.";
        WriteLine(session, log.str());
        beforeText.clear();
        return false;
    }

    const std::string ruleText = HoleRuleDisplayText(rule);
    if (Trim(ruleText).empty())
    {
        std::ostringstream log;
        log << "AutoCreateThreeViews: tapped hole rule matched but display text is empty"
            << " diameter=" << diameter
            << ", count=" << sameDiameterCount
            << "; use plain diameter annotation.";
        WriteLine(session, log.str());
        beforeText.clear();
        return false;
    }

    std::ostringstream prefix;
    if (sameDiameterCount > 1)
    {
        prefix << sameDiameterCount << "-";
    }
    prefix << ruleText;
    beforeText = Trim(prefix.str());

    std::ostringstream log;
    log << "AutoCreateThreeViews: tapped hole rule matched"
        << " diameter=" << diameter
        << ", count=" << sameDiameterCount
        << ", candidates=" << candidateCount
        << ", type=" << rule.annotationType
        << ", series=" << rule.series
        << ", size=" << rule.size
        << ", text=" << beforeText << ".";
    WriteLine(session, log.str());
    return true;
}

std::string BuildHoleDiameterBeforeText(
    NXOpen::Session* session,
    double diameter,
    int sameDiameterCount,
    bool& tapped)
{
    std::string beforeText;
    tapped = TryBuildTappedHoleDiameterPrefix(session, diameter, sameDiameterCount, beforeText);
    if (tapped)
    {
        return beforeText;
    }

    std::ostringstream prefix;
    if (sameDiameterCount > 1)
    {
        prefix << sameDiameterCount << "-";
    }

    std::ostringstream log;
    log << "AutoCreateThreeViews: plain hole diameter annotation"
        << " diameter=" << diameter
        << ", count=" << sameDiameterCount
        << ", beforeText=" << Trim(prefix.str()) << ".";
    WriteLine(session, log.str());
    return Trim(prefix.str());
}

bool ParseBool(const std::map<std::string, std::string>& values, const std::string& key, bool fallback)
{
    const auto it = values.find(key);
    if (it == values.end())
    {
        return fallback;
    }

    const std::string text = Trim(it->second);
    return text == "1" || text == "true" || text == "True" || text == "TRUE" || text == "yes";
}

std::string ReadText(const std::map<std::string, std::string>& values, const std::string& key, const std::string& fallback)
{
    const auto it = values.find(key);
    return it == values.end() ? fallback : Trim(it->second);
}

double ReadDouble(const std::map<std::string, std::string>& values, const std::string& key, double fallback)
{
    const auto it = values.find(key);
    if (it == values.end())
    {
        return fallback;
    }

    try
    {
        return std::stod(Trim(it->second));
    }
    catch (...)
    {
        return fallback;
    }
}

int Base64Value(unsigned char ch)
{
    if (ch >= 'A' && ch <= 'Z')
    {
        return ch - 'A';
    }
    if (ch >= 'a' && ch <= 'z')
    {
        return ch - 'a' + 26;
    }
    if (ch >= '0' && ch <= '9')
    {
        return ch - '0' + 52;
    }
    if (ch == '+')
    {
        return 62;
    }
    if (ch == '/')
    {
        return 63;
    }
    return -1;
}

std::string DecodeBase64OrOriginal(const std::string& text)
{
    std::string clean;
    clean.reserve(text.size());
    for (unsigned char ch : text)
    {
        if (!std::isspace(ch))
        {
            clean.push_back(static_cast<char>(ch));
        }
    }

    if (clean.empty() || clean.size() % 4 != 0)
    {
        return text;
    }

    std::string decoded;
    decoded.reserve(clean.size() * 3 / 4);
    for (size_t i = 0; i < clean.size(); i += 4)
    {
        const int a = Base64Value(static_cast<unsigned char>(clean[i]));
        const int b = Base64Value(static_cast<unsigned char>(clean[i + 1]));
        const char cChar = clean[i + 2];
        const char dChar = clean[i + 3];
        const int c = cChar == '=' ? 0 : Base64Value(static_cast<unsigned char>(cChar));
        const int d = dChar == '=' ? 0 : Base64Value(static_cast<unsigned char>(dChar));
        if (a < 0 || b < 0 || (cChar != '=' && c < 0) || (dChar != '=' && d < 0))
        {
            return text;
        }

        decoded.push_back(static_cast<char>((a << 2) | (b >> 4)));
        if (cChar != '=')
        {
            decoded.push_back(static_cast<char>(((b & 0x0F) << 4) | (c >> 2)));
        }
        if (dChar != '=')
        {
            decoded.push_back(static_cast<char>(((c & 0x03) << 6) | d));
        }
    }

    return decoded;
}

std::filesystem::path PathFromUtf8(const std::string& value)
{
    if (value.empty())
    {
        return {};
    }

#if defined(__cpp_lib_char8_t)
    return std::filesystem::u8path(reinterpret_cast<const char8_t*>(value.c_str()));
#else
    return std::filesystem::u8path(value);
#endif
}

std::string LocalPathString(const std::filesystem::path& path)
{
    return path.string();
}

RequestValues ReadRequestFile(const std::filesystem::path& requestPath)
{
    std::ifstream input(requestPath);
    std::map<std::string, std::string> values;
    std::string line;
    while (std::getline(input, line))
    {
        const size_t equal = line.find('=');
        if (equal == std::string::npos)
        {
            continue;
        }

        values[Trim(line.substr(0, equal))] = Trim(line.substr(equal + 1));
    }

    RequestValues request;
    request.drawingTargetMode = ReadText(values, "drawingTargetMode", "partOrAssembly");
    request.targetLayer = std::max(0, std::min(256, static_cast<int>(ReadDouble(values, "targetLayer", 0.0))));
    request.layerIndex = std::max(0, static_cast<int>(ReadDouble(values, "layerIndex", 0.0)));
    request.layersPerSheet = std::max(1, static_cast<int>(ReadDouble(values, "layersPerSheet", 1.0)));
    request.appendToCurrentSheet = ParseBool(values, "appendToCurrentSheet", false);
    request.templatePath = ReadText(values, "templatePath", "");
    request.inheritDraftingPreferences = ParseBool(values, "inheritDraftingPreferences", true);
    request.layerLayoutMode = ReadText(values, "layerLayoutMode", "auto");
    request.frontDirectionMode = ReadText(values, "frontDirectionMode", "largestFaceLongestEdge");
    request.viewSpacing = std::max(5.0, ReadDouble(values, "viewSpacing", 20.0));
    request.sheetMargin = std::max(5.0, ReadDouble(values, "sheetMargin", 20.0));
    request.firstAngle = ReadText(values, "projection", "first") != "third";
    request.front = ParseBool(values, "front", true);
    request.top = ParseBool(values, "top", false);
    request.bottom = ParseBool(values, "bottom", false);
    request.left = ParseBool(values, "left", false);
    request.right = ParseBool(values, "right", false);
    request.back = ParseBool(values, "back", false);
    request.backBottom = ParseBool(values, "backBottom", false);
    request.iso = ParseBool(values, "iso", true);
    request.flat = ParseBool(values, "flat", true);
    request.autoDimensions = ParseBool(values, "autoDimensions", true);
    request.dimensionOverall = ParseBool(values, "dimensionOverall", true);
    request.dimensionLinear = false;
    request.dimensionAngle = ParseBool(values, "dimensionAngle", false);
    request.dimensionHole = ParseBool(values, "dimensionHole", true);
    request.dimensionHoleLocation = ParseBool(values, "dimensionHoleLocation", true);
    request.dimensionInnerClosedCurve = ParseBool(values, "dimensionInnerClosedCurve", false);
    request.auxiliaryAutoCompact = ParseBool(values, "auxAutoCompact", false);
    request.assemblyDrawing = ParseBool(values, "assemblyDrawing", false);
    request.isoCorner = ReadText(values, "isoCorner", "TopLeft");
    request.flatCorner = ReadText(values, "flatCorner", "BottomRight");
    request.technicalRequirementEnabled = ParseBool(values, "technicalRequirementEnabled", false);
    request.technicalRequirementIndexed = ParseBool(values, "technicalRequirementIndexed", true);
    request.technicalRequirementCorner = ReadText(values, "technicalRequirementCorner", "TopLeft");
    request.technicalRequirementText = Trim(DecodeBase64OrOriginal(ReadText(values, "technicalRequirementText", "")));
    if (request.targetLayer > 0)
    {
        // Layer drawing is a grouped orthographic drawing, never a sheet-metal flat-pattern drawing.
        request.flat = false;
        // Multi-layer sheets need a denser group layout than standalone part
        // drawings.  Feature 08 uses compact fixed gaps for body groups.
        request.viewSpacing = std::min(request.viewSpacing, 10.0);
    }
    if (request.assemblyDrawing)
    {
        request.dimensionAngle = false;
        request.dimensionInnerClosedCurve = false;
    }
    return request;
}

void WriteLine(NXOpen::Session* session, const std::string& message)
{
    (void)session;
    try
    {
        std::ofstream log(CurrentModuleDirectory() / "AutoCreateThreeViews.log", std::ios::app);
        if (log)
        {
            log << message << '\n';
        }
    }
    catch (...)
    {
    }
}

using TimingClock = std::chrono::steady_clock;

double ElapsedMilliseconds(const TimingClock::time_point& started)
{
    return std::chrono::duration<double, std::milli>(TimingClock::now() - started).count();
}

void WriteTimingLine(
    NXOpen::Session* session,
    const std::string& partLabel,
    const std::string& stage,
    const TimingClock::time_point& started)
{
    std::ostringstream line;
    line << std::fixed << std::setprecision(1)
         << "AutoCreateThreeViews: [timing] part=" << partLabel
         << ", stage=" << stage
         << ", elapsedMs=" << ElapsedMilliseconds(started)
         << ".";
    WriteLine(session, line.str());
}

class ScopedPartTiming
{
public:
    ScopedPartTiming(NXOpen::Session* session, std::string partLabel)
        : session_(session), partLabel_(std::move(partLabel)), started_(TimingClock::now())
    {
    }

    ~ScopedPartTiming()
    {
        WriteTimingLine(session_, partLabel_, "part_total", started_);
    }

private:
    NXOpen::Session* session_ = nullptr;
    std::string partLabel_;
    TimingClock::time_point started_;
};

void EnsureLayer230Visible(NXOpen::Session* session, NXOpen::Part* workPart)
{
    if (workPart == nullptr)
    {
        return;
    }

    int currentStatus = UF_LAYER_INACTIVE_LAYER;
    const int askResult = UF_LAYER_ask_status(230, &currentStatus);
    if (askResult != 0)
    {
        WriteLine(
            session,
            "AutoCreateThreeViews: ask layer 230 status failed, UF error " +
                std::to_string(askResult) + ".");
        return;
    }

    if (currentStatus == UF_LAYER_WORK_LAYER || currentStatus == UF_LAYER_ACTIVE_LAYER)
    {
        WriteLine(session, "AutoCreateThreeViews: layer 230 is already visible.");
        return;
    }

    const int setResult = UF_LAYER_set_status(230, UF_LAYER_ACTIVE_LAYER);
    if (setResult != 0)
    {
        WriteLine(
            session,
            "AutoCreateThreeViews: open layer 230 failed, UF error " +
                std::to_string(setResult) + ".");
        return;
    }

    UF_DISP_make_display_up_to_date();
    WriteLine(session, "AutoCreateThreeViews: layer 230 opened before measuring and creating views.");
}

struct DrawingLayerStateSnapshot
{
    bool valid = false;
    std::vector<int> status = std::vector<int>(UF_LAYER_MAX_LAYER + 1, UF_LAYER_INACTIVE_LAYER);
};

class ScopedDrawingLayerIsolation
{
public:
    ScopedDrawingLayerIsolation(NXOpen::Session* session, int targetLayer)
        : session_(session), targetLayer_(targetLayer)
    {
        if (targetLayer_ < UF_LAYER_MIN_LAYER || targetLayer_ > UF_LAYER_MAX_LAYER)
        {
            return;
        }

        for (int layer = UF_LAYER_MIN_LAYER; layer <= UF_LAYER_MAX_LAYER; ++layer)
        {
            if (UF_LAYER_ask_status(layer, &snapshot_.status[layer]) != 0)
            {
                WriteLine(session_, "AutoCreateThreeViews: save model layer states failed; layer isolation skipped.");
                return;
            }
        }
        snapshot_.valid = true;

        // Feature 08 convention: layer 1 is always the work layer; only the
        // requested body layer and drafting support layer 230 stay open.
        UF_LAYER_set_status(1, UF_LAYER_WORK_LAYER);
        for (int layer = UF_LAYER_MIN_LAYER; layer <= UF_LAYER_MAX_LAYER; ++layer)
        {
            if (layer == 1)
            {
                continue;
            }
            const bool keepOpen = layer == targetLayer_ || layer == 230;
            const int result = UF_LAYER_set_status(
                layer,
                keepOpen ? UF_LAYER_ACTIVE_LAYER : UF_LAYER_INACTIVE_LAYER);
            if (result != 0)
            {
                WriteLine(
                    session_,
                    "AutoCreateThreeViews: set model layer " + std::to_string(layer) +
                        " status failed, UF error " + std::to_string(result) + ".");
            }
        }
        UF_DISP_make_display_up_to_date();
        active_ = true;
        WriteLine(
            session_,
            "AutoCreateThreeViews: model layers isolated, workLayer=1, openLayers=1," +
                std::to_string(targetLayer_) + ",230.");
    }

    ~ScopedDrawingLayerIsolation()
    {
        Restore();
    }

    ScopedDrawingLayerIsolation(const ScopedDrawingLayerIsolation&) = delete;
    ScopedDrawingLayerIsolation& operator=(const ScopedDrawingLayerIsolation&) = delete;

private:
    void Restore()
    {
        if (!active_ || !snapshot_.valid)
        {
            return;
        }

        int originalWorkLayer = 1;
        for (int layer = UF_LAYER_MIN_LAYER; layer <= UF_LAYER_MAX_LAYER; ++layer)
        {
            if (snapshot_.status[layer] == UF_LAYER_WORK_LAYER)
            {
                originalWorkLayer = layer;
                break;
            }
        }
        UF_LAYER_set_status(originalWorkLayer, UF_LAYER_WORK_LAYER);
        for (int layer = UF_LAYER_MIN_LAYER; layer <= UF_LAYER_MAX_LAYER; ++layer)
        {
            if (layer == originalWorkLayer)
            {
                continue;
            }
            int status = snapshot_.status[layer];
            if (status == UF_LAYER_WORK_LAYER)
            {
                status = UF_LAYER_ACTIVE_LAYER;
            }
            UF_LAYER_set_status(layer, status);
        }
        UF_DISP_make_display_up_to_date();
        active_ = false;
        WriteLine(
            session_,
            "AutoCreateThreeViews: restored model layer states, workLayer=" +
                std::to_string(originalWorkLayer) + ".");
    }

    NXOpen::Session* session_ = nullptr;
    int targetLayer_ = 0;
    bool active_ = false;
    DrawingLayerStateSnapshot snapshot_;
};

std::filesystem::path ProgressPathFromRequest(const std::filesystem::path& requestPath)
{
    std::filesystem::path directory = requestPath.parent_path();
    if (directory.empty())
    {
        directory = CurrentModuleDirectory();
    }
    return directory / "AutoCreateThreeViews.progress";
}

void WriteProgressFile(
    const std::filesystem::path& requestPath,
    int current,
    int total,
    const std::string& message,
    bool done)
{
    try
    {
        const std::filesystem::path progressPath = ProgressPathFromRequest(requestPath);
        std::filesystem::create_directories(progressPath.parent_path());
        std::ofstream output(progressPath, std::ios::binary | std::ios::trunc);
        output << "current=" << current << '\n'
               << "total=" << total << '\n'
               << "message=" << message << '\n'
               << "done=" << (done ? "1" : "0") << '\n';
    }
    catch (...)
    {
    }
}

std::string PartResultName(NXOpen::Part* part)
{
    if (part == nullptr)
    {
        return "(no part)";
    }

    try
    {
        return part->Leaf().GetText();
    }
    catch (...)
    {
        return "(part)";
    }
}

void AppendRunResultLine(const std::string& line)
{
    if (!line.empty())
    {
        g_runResultLines.push_back(line);
    }
}

std::string BuildRunResultText()
{
    std::ostringstream text;
    text << u8"运行结果";
    if (g_runResultLines.empty())
    {
        text << "\n" << u8"未生成图纸。";
        return text.str();
    }

    for (const std::string& line : g_runResultLines)
    {
        text << "\n" << line;
    }
    return text.str();
}

struct RunResultSummary
{
    int success = 0;
    int failed = 0;
    int skipped = 0;
    int progress = 0;
};

RunResultSummary SummarizeRunResults()
{
    RunResultSummary summary;
    for (const std::string& line : g_runResultLines)
    {
        if (line.find("Progress:") != std::string::npos)
        {
            ++summary.progress;
        }
        else if (line.find("success") != std::string::npos ||
                 line.find("Success") != std::string::npos ||
                 line.find(u8"成功") != std::string::npos)
        {
            ++summary.success;
        }
        else if (line.find("skip") != std::string::npos ||
                 line.find("Skip") != std::string::npos ||
                 line.find(u8"跳过") != std::string::npos ||
                 line.find(u8"璺宠繃") != std::string::npos)
        {
            ++summary.skipped;
        }
        else if (line.find("fail") != std::string::npos ||
                 line.find("Fail") != std::string::npos ||
                 line.find(u8"失败") != std::string::npos ||
                 line.find(u8"澶辫触") != std::string::npos)
        {
            ++summary.failed;
        }
    }
    return summary;
}

int LoadChineseDraftNoteFont(NXOpen::Session* session, NXOpen::Part* part)
{
    if (part == nullptr || part->Fonts() == nullptr)
    {
        return 0;
    }

    const std::vector<const char*> candidates = {
        "Microsoft YaHei",
        "SimSun",
        "NSimSun",
        "Arial Unicode MS"};

    for (const char* fontName : candidates)
    {
        try
        {
            const int fontIndex = part->Fonts()->AddFont(fontName, NXOpen::FontCollection::TypeStandard);
            if (fontIndex > 0 && part->Fonts()->DoesFontExist(fontIndex))
            {
                std::ostringstream log;
                log << "AutoCreateThreeViews: flat pattern note font=" << fontName
                    << ", index=" << fontIndex << ".";
                WriteLine(session, log.str());
                return fontIndex;
            }
        }
        catch (const NXOpen::NXException& ex)
        {
            std::ostringstream log;
            log << "AutoCreateThreeViews: flat pattern note font skipped " << fontName
                << ", NX " << ex.ErrorCode() << ": " << ex.Message();
            WriteLine(session, log.str());
        }
        catch (...)
        {
        }
    }

    WriteLine(session, "AutoCreateThreeViews: flat pattern note font fallback to current drafting font.");
    return 0;
}

NXOpen::ModelingView* FindModelingView(NXOpen::Part* part, const std::vector<std::string>& names, bool allowWorkViewFallback)
{
    if (part == nullptr || part->ModelingViews() == nullptr)
    {
        return nullptr;
    }

    for (const std::string& name : names)
    {
        try
        {
            NXOpen::ModelingView* view =
                dynamic_cast<NXOpen::ModelingView*>(part->ModelingViews()->FindObject(name.c_str()));
            if (view != nullptr)
            {
                return view;
            }
        }
        catch (...)
        {
        }
    }

    if (!allowWorkViewFallback)
    {
        return nullptr;
    }

    try
    {
        return dynamic_cast<NXOpen::ModelingView*>(part->Views()->WorkView());
    }
    catch (...)
    {
        return nullptr;
    }
}

NXOpen::ModelingView* FindModelingViewByKeyword(NXOpen::Part* part, const std::vector<std::string>& keywords)
{
    if (part == nullptr || part->ModelingViews() == nullptr)
    {
        return nullptr;
    }

    try
    {
        for (NXOpen::ModelingView* view : *part->ModelingViews())
        {
            if (view == nullptr)
            {
                continue;
            }

            std::string viewName;
            std::string journalId;
            try
            {
                viewName = view->Name().GetLocaleText();
            }
            catch (...)
            {
            }
            try
            {
                journalId = view->JournalIdentifier().GetLocaleText();
            }
            catch (...)
            {
            }

            if (ContainsAnyKeyword(viewName, keywords) || ContainsAnyKeyword(journalId, keywords))
            {
                return view;
            }
        }
    }
    catch (...)
    {
    }

    return nullptr;
}

bool HasPartName(NXOpen::Part* part)
{
    try
    {
        return part != nullptr &&
               part->HasUserAttribute("名称", NXOpen::NXObject::AttributeType::AttributeTypeString, -1) &&
               std::string(part->GetStringAttribute("名称").GetLocaleText()).length() > 0;
    }
    catch (...)
    {
        return false;
    }
}

enum class BoundsBodyFilterResult
{
    Include,
    HiddenBody,
    HiddenLayer,
    LayerAbove256
};

int AskObjectLayer(tag_t objectTag)
{
    UF_OBJ_disp_props_t displayProperties = {};
    return objectTag != NULL_TAG && UF_OBJ_ask_display_properties(objectTag, &displayProperties) == 0
        ? displayProperties.layer
        : 0;
}

bool MatchesActiveTargetLayer(tag_t bodyTag)
{
    return g_activeTargetLayer <= 0 || AskObjectLayer(bodyTag) == g_activeTargetLayer;
}

BoundsBodyFilterResult ClassifyBodyForBounds(tag_t bodyTag, int& layer)
{
    layer = 0;
    if (bodyTag == NULL_TAG)
    {
        return BoundsBodyFilterResult::HiddenBody;
    }

    UF_OBJ_disp_props_t displayProperties = {};
    if (UF_OBJ_ask_display_properties(bodyTag, &displayProperties) != 0)
    {
        return BoundsBodyFilterResult::Include;
    }

    layer = displayProperties.layer;
    if (displayProperties.blank_status != UF_OBJ_NOT_BLANKED)
    {
        return BoundsBodyFilterResult::HiddenBody;
    }
    if (layer > 256)
    {
        return BoundsBodyFilterResult::LayerAbove256;
    }

    int layerStatus = UF_LAYER_INACTIVE_LAYER;
    if (layer != g_activeTargetLayer &&
        layer >= UF_LAYER_MIN_LAYER &&
        UF_LAYER_ask_status(layer, &layerStatus) == 0 &&
        layerStatus == UF_LAYER_INACTIVE_LAYER)
    {
        return BoundsBodyFilterResult::HiddenLayer;
    }

    return BoundsBodyFilterResult::Include;
}

ModelBounds AskModelBounds(NXOpen::Part* part)
{
    ModelBounds result;
    if (part == nullptr)
    {
        return result;
    }

    bool initialized = false;
    double minValues[3] = {0.0, 0.0, 0.0};
    double maxValues[3] = {0.0, 0.0, 0.0};
    int includedBodyCount = 0;
    int hiddenBodyCount = 0;
    int hiddenLayerBodyCount = 0;
    int layerAbove256BodyCount = 0;
    std::set<int> excludedLayers;

    auto accumulateVisibleBody = [&](tag_t bodyTag) {
        if (!MatchesActiveTargetLayer(bodyTag))
        {
            return;
        }
        int layer = 0;
        const BoundsBodyFilterResult filterResult = ClassifyBodyForBounds(bodyTag, layer);
        if (filterResult == BoundsBodyFilterResult::HiddenBody)
        {
            ++hiddenBodyCount;
            return;
        }
        if (filterResult == BoundsBodyFilterResult::HiddenLayer)
        {
            ++hiddenLayerBodyCount;
            excludedLayers.insert(layer);
            return;
        }
        if (filterResult == BoundsBodyFilterResult::LayerAbove256)
        {
            ++layerAbove256BodyCount;
            excludedLayers.insert(layer);
            return;
        }

        AccumulateBodyBounds(bodyTag, initialized, minValues, maxValues);
        ++includedBodyCount;
    };

    if (part->Bodies() != nullptr)
    {
        for (NXOpen::BodyCollection::iterator it = part->Bodies()->begin(); it != part->Bodies()->end(); ++it)
        {
            NXOpen::Body* body = *it;
            if (body == nullptr)
            {
                continue;
            }
            accumulateVisibleBody(body->Tag());
        }
    }

    if (!initialized)
    {
        const std::vector<tag_t> visibleBodyTags = CollectVisibleSolidBodyTags(part);
        for (tag_t bodyTag : visibleBodyTags)
        {
            accumulateVisibleBody(bodyTag);
        }
    }

    std::ostringstream filterLog;
    filterLog << "AutoCreateThreeViews: model bounds body filter included="
              << includedBodyCount
              << ", hiddenBodies="
              << hiddenBodyCount
              << ", hiddenLayerBodies="
              << hiddenLayerBodyCount
              << ", layerAbove256Bodies="
              << layerAbove256BodyCount;
    if (!excludedLayers.empty())
    {
        filterLog << ", excludedLayers=";
        bool first = true;
        for (int layer : excludedLayers)
        {
            if (!first)
            {
                filterLog << ",";
            }
            filterLog << layer;
            first = false;
        }
    }
    filterLog << ".";
    WriteLine(nullptr, filterLog.str());

    if (!initialized)
    {
        return result;
    }

    const double dx = std::max(1.0, maxValues[0] - minValues[0]);
    const double dy = std::max(1.0, maxValues[1] - minValues[1]);
    const double dz = std::max(1.0, maxValues[2] - minValues[2]);
    result.width = std::max(dx, dy);
    result.height = std::max(dy, dz);
    result.sizeX = dx;
    result.sizeY = dy;
    result.sizeZ = dz;
    result.valid = true;
    return result;
}

bool AskModelBoundsCenter(NXOpen::Part* part, NXOpen::Point3d& center)
{
    if (part == nullptr || part->Bodies() == nullptr)
    {
        return false;
    }

    bool initialized = false;
    double minValues[3] = {0.0, 0.0, 0.0};
    double maxValues[3] = {0.0, 0.0, 0.0};

    for (NXOpen::BodyCollection::iterator it = part->Bodies()->begin(); it != part->Bodies()->end(); ++it)
    {
        NXOpen::Body* body = *it;
        if (body == nullptr || body->IsBlanked() || !MatchesActiveTargetLayer(body->Tag()))
        {
            continue;
        }

        double box[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
        if (UF_MODL_ask_bounding_box(body->Tag(), box) != 0)
        {
            continue;
        }

        if (!initialized)
        {
            minValues[0] = box[0];
            minValues[1] = box[1];
            minValues[2] = box[2];
            maxValues[0] = box[3];
            maxValues[1] = box[4];
            maxValues[2] = box[5];
            initialized = true;
        }
        else
        {
            minValues[0] = std::min(minValues[0], box[0]);
            minValues[1] = std::min(minValues[1], box[1]);
            minValues[2] = std::min(minValues[2], box[2]);
            maxValues[0] = std::max(maxValues[0], box[3]);
            maxValues[1] = std::max(maxValues[1], box[4]);
            maxValues[2] = std::max(maxValues[2], box[5]);
        }
    }

    if (!initialized)
    {
        return false;
    }

    center = NXOpen::Point3d(
        (minValues[0] + maxValues[0]) * 0.5,
        (minValues[1] + maxValues[1]) * 0.5,
        (minValues[2] + maxValues[2]) * 0.5);
    return true;
}

NXOpen::Vector3d AxisVector(int axis, double sign = 1.0)
{
    if (axis == 0)
    {
        return NXOpen::Vector3d(sign, 0.0, 0.0);
    }
    if (axis == 1)
    {
        return NXOpen::Vector3d(0.0, sign, 0.0);
    }
    return NXOpen::Vector3d(0.0, 0.0, sign);
}

std::string AxisName(int axis)
{
    if (axis == 0)
    {
        return "X";
    }
    if (axis == 1)
    {
        return "Y";
    }
    return "Z";
}

double VectorLength(const NXOpen::Vector3d& vector)
{
    return std::sqrt(vector.X * vector.X + vector.Y * vector.Y + vector.Z * vector.Z);
}

NXOpen::Vector3d NormalizeVector(const NXOpen::Vector3d& vector)
{
    const double length = VectorLength(vector);
    if (length < 1.0e-9)
    {
        return NXOpen::Vector3d(0.0, 0.0, 0.0);
    }
    return NXOpen::Vector3d(vector.X / length, vector.Y / length, vector.Z / length);
}

double DotVector(const NXOpen::Vector3d& a, const NXOpen::Vector3d& b)
{
    return a.X * b.X + a.Y * b.Y + a.Z * b.Z;
}

NXOpen::Vector3d CrossVector(const NXOpen::Vector3d& a, const NXOpen::Vector3d& b)
{
    return NXOpen::Vector3d(
        a.Y * b.Z - a.Z * b.Y,
        a.Z * b.X - a.X * b.Z,
        a.X * b.Y - a.Y * b.X);
}

NXOpen::Vector3d ProjectPerpendicular(
    const NXOpen::Vector3d& vector,
    const NXOpen::Vector3d& normal)
{
    const double dot = DotVector(vector, normal);
    return NXOpen::Vector3d(
        vector.X - normal.X * dot,
        vector.Y - normal.Y * dot,
        vector.Z - normal.Z * dot);
}

void StabilizeDirectionSign(NXOpen::Vector3d& vector)
{
    const double values[3] = {vector.X, vector.Y, vector.Z};
    int dominant = 0;
    for (int axis = 1; axis < 3; ++axis)
    {
        if (std::abs(values[axis]) > std::abs(values[dominant]))
        {
            dominant = axis;
        }
    }

    if (values[dominant] < 0.0)
    {
        vector.X = -vector.X;
        vector.Y = -vector.Y;
        vector.Z = -vector.Z;
    }
}

std::string DominantAxisName(const NXOpen::Vector3d& vector)
{
    const double values[3] = {vector.X, vector.Y, vector.Z};
    int dominant = 0;
    for (int axis = 1; axis < 3; ++axis)
    {
        if (std::abs(values[axis]) > std::abs(values[dominant]))
        {
            dominant = axis;
        }
    }

    const double sign = values[dominant] < 0.0 ? -1.0 : 1.0;
    return std::string(sign < 0.0 ? "-" : "") + AxisName(dominant);
}

int DominantAxisIndex(const NXOpen::Vector3d& vector)
{
    const double values[3] = {vector.X, vector.Y, vector.Z};
    int dominant = 0;
    for (int axis = 1; axis < 3; ++axis)
    {
        if (std::abs(values[axis]) > std::abs(values[dominant]))
        {
            dominant = axis;
        }
    }
    return dominant;
}

std::vector<std::string> ModelViewNamesForNormal(const AutoViewDirection& orientation)
{
    const double ax = std::abs(orientation.normal.X);
    const double ay = std::abs(orientation.normal.Y);
    const double az = std::abs(orientation.normal.Z);
    if (az >= ax && az >= ay)
    {
        return {"Top", "Bottom"};
    }
    if (ay >= ax && ay >= az)
    {
        return {"Front", "Back"};
    }
    return {"Right", "Left"};
}

bool IsStraightEdge(tag_t edgeTag, const double start[3], const double end[3])
{
    UF_EVAL_p_t evaluator = nullptr;
    if (UF_EVAL_initialize(edgeTag, &evaluator) != 0 || evaluator == nullptr)
    {
        return true;
    }

    bool straight = true;
    double limits[2] = {0.0, 0.0};
    const double direction[3] = {end[0] - start[0], end[1] - start[1], end[2] - start[2]};
    const double length = std::sqrt(direction[0] * direction[0] + direction[1] * direction[1] + direction[2] * direction[2]);
    if (length < 1.0e-6 || UF_EVAL_ask_limits(evaluator, limits) != 0)
    {
        UF_EVAL_free(evaluator);
        return false;
    }

    double derivatives[3] = {0.0, 0.0, 0.0};
    for (int i = 1; i < 4; ++i)
    {
        const double t = static_cast<double>(i) / 4.0;
        const double parameter = limits[0] + (limits[1] - limits[0]) * t;
        double point[3] = {0.0, 0.0, 0.0};
        if (UF_EVAL_evaluate(evaluator, 0, parameter, point, derivatives) != 0)
        {
            continue;
        }

        const double fromStart[3] = {point[0] - start[0], point[1] - start[1], point[2] - start[2]};
        const double cross[3] = {
            fromStart[1] * direction[2] - fromStart[2] * direction[1],
            fromStart[2] * direction[0] - fromStart[0] * direction[2],
            fromStart[0] * direction[1] - fromStart[1] * direction[0]};
        const double distance =
            std::sqrt(cross[0] * cross[0] + cross[1] * cross[1] + cross[2] * cross[2]) / length;
        if (distance > std::max(0.01, length * 0.001))
        {
            straight = false;
            break;
        }
    }

    UF_EVAL_free(evaluator);
    return straight;
}

double ApproximatePlanarFaceArea(const double normal[3], const double box[6])
{
    const double dx = std::max(0.0, box[3] - box[0]);
    const double dy = std::max(0.0, box[4] - box[1]);
    const double dz = std::max(0.0, box[5] - box[2]);
    const double ax = std::abs(normal[0]);
    const double ay = std::abs(normal[1]);
    const double az = std::abs(normal[2]);
    if (ax >= ay && ax >= az)
    {
        return dy * dz;
    }
    if (ay >= ax && ay >= az)
    {
        return dx * dz;
    }
    return dx * dy;
}

double AskPlanarFaceArea(NXOpen::Part* part, tag_t faceTag)
{
    if (part == nullptr || faceTag == NULL_TAG || part->MeasureManager() == nullptr)
    {
        return 0.0;
    }

    NXOpen::Face* face = dynamic_cast<NXOpen::Face*>(NXOpen::NXObjectManager::Get(faceTag));
    if (face == nullptr)
    {
        return 0.0;
    }

    try
    {
        NXOpen::Unit* areaUnit = part->UnitCollection()->FindObject("SquareMilliMeter");
        NXOpen::Unit* lengthUnit = part->UnitCollection()->GetBase("Length");
        std::vector<NXOpen::IParameterizedSurface*> faces(1, face);
        NXOpen::MeasureFaces* measureFaces =
            part->MeasureManager()->NewFaceProperties(areaUnit, lengthUnit, 0.99, faces);
        const double area = measureFaces != nullptr ? measureFaces->Area() : 0.0;
        delete measureFaces;
        return area;
    }
    catch (...)
    {
        return 0.0;
    }
}

NXOpen::Vector3d AskPlanarFaceVDirection(tag_t faceTag, const NXOpen::Vector3d& faceNormal)
{
    if (faceTag == NULL_TAG)
    {
        return NXOpen::Vector3d(0.0, 0.0, 0.0);
    }

    double uv[4] = {0.0, 0.0, 0.0, 0.0};
    if (UF_MODL_ask_face_uv_minmax(faceTag, uv) != 0)
    {
        return NXOpen::Vector3d(0.0, 0.0, 0.0);
    }

    double param[2] = {
        (uv[0] + uv[1]) * 0.5,
        (uv[2] + uv[3]) * 0.5};
    double point[3] = {0.0, 0.0, 0.0};
    double u1[3] = {0.0, 0.0, 0.0};
    double v1[3] = {0.0, 0.0, 0.0};
    double u2[3] = {0.0, 0.0, 0.0};
    double v2[3] = {0.0, 0.0, 0.0};
    double normal[3] = {0.0, 0.0, 0.0};
    double radii[2] = {0.0, 0.0};
    if (UF_MODL_ask_face_props(faceTag, param, point, u1, v1, u2, v2, normal, radii) != 0)
    {
        return NXOpen::Vector3d(0.0, 0.0, 0.0);
    }

    NXOpen::Vector3d vDirection(v1[0], v1[1], v1[2]);
    vDirection = ProjectPerpendicular(vDirection, faceNormal);
    return NormalizeVector(vDirection);
}

bool IsSheetMetalBody(NXOpen::Part* part, NXOpen::Body* body)
{
    if (part == nullptr || body == nullptr || part->Features() == nullptr || part->Features()->SheetmetalManager() == nullptr)
    {
        return false;
    }

    try
    {
        return part->Features()->SheetmetalManager()->GetBodyThickness(body) > 1.0e-6;
    }
    catch (...)
    {
        return false;
    }
}

double AskVisibleSheetMetalThickness(NXOpen::Part* part)
{
    if (part == nullptr || part->Bodies() == nullptr)
    {
        return 0.0;
    }

    double bestThickness = 0.0;
    for (NXOpen::BodyCollection::iterator it = part->Bodies()->begin(); it != part->Bodies()->end(); ++it)
    {
        NXOpen::Body* body = *it;
        if (body == nullptr || body->IsBlanked())
        {
            continue;
        }

        struct PlanarFaceInfo
        {
            tag_t faceTag = NULL_TAG;
            NXOpen::Vector3d normal = NXOpen::Vector3d(0.0, 0.0, 0.0);
            NXOpen::Point3d point = NXOpen::Point3d(0.0, 0.0, 0.0);
            double area = 0.0;
        };

        std::vector<PlanarFaceInfo> faces;
        uf_list_p_t faceList = nullptr;
        if (UF_MODL_ask_body_faces(body->Tag(), &faceList) != 0 || faceList == nullptr)
        {
            continue;
        }

        int faceCount = 0;
        UF_MODL_ask_list_count(faceList, &faceCount);
        for (int faceIndex = 0; faceIndex < faceCount; ++faceIndex)
        {
            tag_t faceTag = NULL_TAG;
            if (UF_MODL_ask_list_item(faceList, faceIndex, &faceTag) != 0 || faceTag == NULL_TAG)
            {
                continue;
            }

            int faceType = 0;
            double point[3] = {0.0, 0.0, 0.0};
            double normalData[3] = {0.0, 0.0, 0.0};
            double box[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
            double radius = 0.0;
            double radData = 0.0;
            int normDir = 1;
            if (UF_MODL_ask_face_data(faceTag, &faceType, point, normalData, box, &radius, &radData, &normDir) != 0 ||
                faceType != 22)
            {
                continue;
            }

            NXOpen::Vector3d normal(
                normalData[0] * static_cast<double>(normDir),
                normalData[1] * static_cast<double>(normDir),
                normalData[2] * static_cast<double>(normDir));
            normal = NormalizeVector(normal);
            if (VectorLength(normal) < 1.0e-6)
            {
                continue;
            }

            double area = AskPlanarFaceArea(part, faceTag);
            if (area <= 1.0e-6)
            {
                area = ApproximatePlanarFaceArea(normalData, box);
            }
            if (area <= 1.0e-6)
            {
                continue;
            }

            PlanarFaceInfo info;
            info.faceTag = faceTag;
            info.normal = normal;
            info.point = NXOpen::Point3d(point[0], point[1], point[2]);
            info.area = area;
            faces.push_back(info);
        }

        UF_MODL_delete_list(&faceList);
        if (faces.size() < 2)
        {
            continue;
        }

        const PlanarFaceInfo* largestFace = &faces.front();
        for (const PlanarFaceInfo& face : faces)
        {
            if (face.area > largestFace->area)
            {
                largestFace = &face;
            }
        }

        double bodyThickness = 0.0;
        const double minimumParallelArea = largestFace->area * 0.5;
        for (const PlanarFaceInfo& face : faces)
        {
            if (face.faceTag == largestFace->faceTag || face.area <= minimumParallelArea)
            {
                continue;
            }
            if (std::abs(DotVector(largestFace->normal, face.normal)) < 0.995)
            {
                continue;
            }

            const double gap = std::abs(
                largestFace->normal.X * (face.point.X - largestFace->point.X) +
                largestFace->normal.Y * (face.point.Y - largestFace->point.Y) +
                largestFace->normal.Z * (face.point.Z - largestFace->point.Z));
            if (gap <= 1.0e-6)
            {
                continue;
            }

            if (bodyThickness <= 1.0e-6 || gap < bodyThickness)
            {
                bodyThickness = gap;
            }
        }

        if (bodyThickness > 1.0e-6 && (bestThickness <= 1.0e-6 || bodyThickness < bestThickness))
        {
            bestThickness = bodyThickness;
        }
    }

    return bestThickness;
}

bool HasVisibleSheetMetalBody(NXOpen::Part* part)
{
    if (part == nullptr || part->Bodies() == nullptr)
    {
        return false;
    }

    for (NXOpen::BodyCollection::iterator it = part->Bodies()->begin(); it != part->Bodies()->end(); ++it)
    {
        NXOpen::Body* body = *it;
        if (body == nullptr || body->IsBlanked())
        {
            continue;
        }
        if (IsSheetMetalBody(part, body))
        {
            return true;
        }
    }

    return false;
}

bool TryReadPlanarFacePointAndNormal(tag_t faceTag, NXOpen::Point3d* pointOnPlane, NXOpen::Vector3d& normal)
{
    if (faceTag == NULL_TAG)
    {
        return false;
    }

    NXOpen::Face* face = dynamic_cast<NXOpen::Face*>(NXOpen::NXObjectManager::Get(faceTag));
    if (face == nullptr)
    {
        return false;
    }
    try
    {
        if (face->SolidFaceType() != NXOpen::Face::FaceTypePlanar)
        {
            return false;
        }
    }
    catch (...)
    {
        return false;
    }

    int faceType = 0;
    double point[3] = {0.0, 0.0, 0.0};
    double normalData[3] = {0.0, 0.0, 0.0};
    double box[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    double radius = 0.0;
    double radData = 0.0;
    int normDir = 1;
    if (UF_MODL_ask_face_data(faceTag, &faceType, point, normalData, box, &radius, &radData, &normDir) != 0 ||
        faceType != 22)
    {
        return false;
    }

    normal = NormalizeVector(NXOpen::Vector3d(
        normalData[0] * static_cast<double>(normDir),
        normalData[1] * static_cast<double>(normDir),
        normalData[2] * static_cast<double>(normDir)));
    if (pointOnPlane != nullptr)
    {
        *pointOnPlane = NXOpen::Point3d(point[0], point[1], point[2]);
    }
    return VectorLength(normal) > 1.0e-6;
}

bool TryReadPlanarFaceNormal(tag_t faceTag, NXOpen::Vector3d& normal)
{
    return TryReadPlanarFacePointAndNormal(faceTag, nullptr, normal);
}

double AskPlanarFaceAreaWithApproximation(NXOpen::Part* part, tag_t faceTag)
{
    double area = AskPlanarFaceArea(part, faceTag);
    if (area > 1.0e-6)
    {
        return area;
    }

    int faceType = 0;
    double point[3] = {0.0, 0.0, 0.0};
    double normalData[3] = {0.0, 0.0, 0.0};
    double box[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    double radius = 0.0;
    double radData = 0.0;
    int normDir = 1;
    if (UF_MODL_ask_face_data(faceTag, &faceType, point, normalData, box, &radius, &radData, &normDir) == 0 &&
        faceType == 22)
    {
        area = ApproximatePlanarFaceArea(normalData, box);
    }
    return area;
}

bool CorrectSelectedPlanarFaceOuterNormal(
    NXOpen::Part* part,
    tag_t selectedFaceTag,
    NXOpen::Vector3d& normal)
{
    NXOpen::Point3d selectedPoint(0.0, 0.0, 0.0);
    NXOpen::Vector3d selectedNormal(0.0, 0.0, 0.0);
    if (!TryReadPlanarFacePointAndNormal(selectedFaceTag, &selectedPoint, selectedNormal))
    {
        WriteLine(nullptr, "AutoCreateThreeViews: manual face normal correction skipped; selected face plane was not readable.");
        return false;
    }

    const double selectedArea = AskPlanarFaceAreaWithApproximation(part, selectedFaceTag);
    if (selectedArea <= 1.0e-6)
    {
        WriteLine(nullptr, "AutoCreateThreeViews: manual face normal correction skipped; selected face area was not readable.");
        return false;
    }

    tag_t bodyTag = NULL_TAG;
    if (UF_MODL_ask_face_body(selectedFaceTag, &bodyTag) != 0 || bodyTag == NULL_TAG)
    {
        WriteLine(nullptr, "AutoCreateThreeViews: manual face normal correction skipped; selected face body was not readable.");
        return false;
    }

    uf_list_p_t faceList = nullptr;
    if (UF_MODL_ask_body_faces(bodyTag, &faceList) != 0 || faceList == nullptr)
    {
        WriteLine(nullptr, "AutoCreateThreeViews: manual face normal correction skipped; body faces were not readable.");
        return false;
    }

    const double minimumParallelArea = selectedArea * 0.60;
    tag_t nearestParallelFaceTag = NULL_TAG;
    double nearestParallelArea = 0.0;
    double nearestSignedDistance = 0.0;
    double nearestDistance = std::numeric_limits<double>::max();

    int faceCount = 0;
    UF_MODL_ask_list_count(faceList, &faceCount);
    for (int faceIndex = 0; faceIndex < faceCount; ++faceIndex)
    {
        tag_t faceTag = NULL_TAG;
        if (UF_MODL_ask_list_item(faceList, faceIndex, &faceTag) != 0 ||
            faceTag == NULL_TAG ||
            faceTag == selectedFaceTag)
        {
            continue;
        }

        NXOpen::Point3d candidatePoint(0.0, 0.0, 0.0);
        NXOpen::Vector3d candidateNormal(0.0, 0.0, 0.0);
        if (!TryReadPlanarFacePointAndNormal(faceTag, &candidatePoint, candidateNormal))
        {
            continue;
        }

        const double candidateArea = AskPlanarFaceAreaWithApproximation(part, faceTag);
        if (candidateArea + 1.0e-6 < minimumParallelArea)
        {
            continue;
        }

        if (std::abs(DotVector(selectedNormal, candidateNormal)) < 0.995)
        {
            continue;
        }

        const NXOpen::Vector3d pointDelta(
            candidatePoint.X - selectedPoint.X,
            candidatePoint.Y - selectedPoint.Y,
            candidatePoint.Z - selectedPoint.Z);
        const double signedDistance = DotVector(pointDelta, selectedNormal);
        const double distance = std::abs(signedDistance);
        if (distance <= 1.0e-6 || distance >= nearestDistance)
        {
            continue;
        }

        nearestParallelFaceTag = faceTag;
        nearestParallelArea = candidateArea;
        nearestSignedDistance = signedDistance;
        nearestDistance = distance;
    }

    UF_MODL_delete_list(&faceList);

    if (nearestParallelFaceTag == NULL_TAG)
    {
        std::ostringstream line;
        line << "AutoCreateThreeViews: manual face normal correction found no parallel face, selectedArea="
             << selectedArea
             << ", minimumParallelArea="
             << minimumParallelArea
             << ".";
        WriteLine(nullptr, line.str());
        return false;
    }

    const bool reversed = nearestSignedDistance > 1.0e-6;
    if (reversed)
    {
        normal.X = -normal.X;
        normal.Y = -normal.Y;
        normal.Z = -normal.Z;
    }

    std::ostringstream line;
    line << "AutoCreateThreeViews: manual face normal correction selectedArea="
         << selectedArea
         << ", nearestParallelFaceTag="
         << static_cast<unsigned long long>(nearestParallelFaceTag)
         << ", nearestParallelArea="
         << nearestParallelArea
         << ", signedDistanceAlongSelectedNormal="
         << nearestSignedDistance
         << ", reversed="
         << (reversed ? "true" : "false")
         << ".";
    WriteLine(nullptr, line.str());
    return reversed;
}

bool TryReadPlanarFaceNormalWithNxOpenDirection(
    NXOpen::Part* part,
    NXOpen::Face* face,
    NXOpen::Vector3d& normal)
{
    if (part == nullptr || part->Directions() == nullptr || face == nullptr)
    {
        return false;
    }

    try
    {
        NXOpen::Direction* direction =
            part->Directions()->CreateDirection(
                face,
                NXOpen::SenseForward,
                NXOpen::SmartObject::UpdateOptionWithinModeling);
        if (direction == nullptr)
        {
            return false;
        }

        normal = NormalizeVector(direction->Vector());
        return VectorLength(normal) > 1.0e-6;
    }
    catch (...)
    {
        return false;
    }
}

bool TryReadStraightEdgeDirection(tag_t edgeTag, const NXOpen::Point3d& referenceVertex, NXOpen::Vector3d& xDirection, double& edgeLength)
{
    edgeLength = 0.0;
    if (edgeTag == NULL_TAG)
    {
        return false;
    }

    double start[3] = {0.0, 0.0, 0.0};
    double end[3] = {0.0, 0.0, 0.0};
    int vertexCount = 0;
    if (UF_MODL_ask_edge_verts(edgeTag, start, end, &vertexCount) != 0 || vertexCount < 2 || !IsStraightEdge(edgeTag, start, end))
    {
        return false;
    }

    NXOpen::Vector3d startToEnd(end[0] - start[0], end[1] - start[1], end[2] - start[2]);
    edgeLength = VectorLength(startToEnd);
    if (edgeLength <= 1.0e-6)
    {
        return false;
    }

    const double distToStart =
        std::sqrt((referenceVertex.X - start[0]) * (referenceVertex.X - start[0]) +
                  (referenceVertex.Y - start[1]) * (referenceVertex.Y - start[1]) +
                  (referenceVertex.Z - start[2]) * (referenceVertex.Z - start[2]));
    const double distToEnd =
        std::sqrt((referenceVertex.X - end[0]) * (referenceVertex.X - end[0]) +
                  (referenceVertex.Y - end[1]) * (referenceVertex.Y - end[1]) +
                  (referenceVertex.Z - end[2]) * (referenceVertex.Z - end[2]));
    if (distToEnd < distToStart)
    {
        startToEnd.X = -startToEnd.X;
        startToEnd.Y = -startToEnd.Y;
        startToEnd.Z = -startToEnd.Z;
    }

    xDirection = NormalizeVector(startToEnd);
    return VectorLength(xDirection) > 1.0e-6;
}

bool TryReadStraightEdgeDirection(tag_t edgeTag, NXOpen::Vector3d& xDirection, double& edgeLength)
{
    edgeLength = 0.0;
    if (edgeTag == NULL_TAG)
    {
        return false;
    }

    double start[3] = {0.0, 0.0, 0.0};
    double end[3] = {0.0, 0.0, 0.0};
    int vertexCount = 0;
    if (UF_MODL_ask_edge_verts(edgeTag, start, end, &vertexCount) != 0 ||
        vertexCount < 2 ||
        !IsStraightEdge(edgeTag, start, end))
    {
        return false;
    }

    NXOpen::Vector3d startToEnd(end[0] - start[0], end[1] - start[1], end[2] - start[2]);
    edgeLength = VectorLength(startToEnd);
    if (edgeLength <= 1.0e-6)
    {
        return false;
    }

    xDirection = NormalizeVector(startToEnd);
    return VectorLength(xDirection) > 1.0e-6;
}

bool TryReadStraightEdgeDirectionTowardPoint(
    tag_t edgeTag,
    const NXOpen::Point3d& endPointReference,
    NXOpen::Vector3d& xDirection,
    double& edgeLength,
    double& distanceToStart,
    double& distanceToEnd)
{
    edgeLength = 0.0;
    distanceToStart = 0.0;
    distanceToEnd = 0.0;
    if (edgeTag == NULL_TAG)
    {
        return false;
    }

    double start[3] = {0.0, 0.0, 0.0};
    double end[3] = {0.0, 0.0, 0.0};
    int vertexCount = 0;
    if (UF_MODL_ask_edge_verts(edgeTag, start, end, &vertexCount) != 0 ||
        vertexCount < 2 ||
        !IsStraightEdge(edgeTag, start, end))
    {
        return false;
    }

    NXOpen::Vector3d startToEnd(end[0] - start[0], end[1] - start[1], end[2] - start[2]);
    edgeLength = VectorLength(startToEnd);
    if (edgeLength <= 1.0e-6)
    {
        return false;
    }

    distanceToStart =
        std::sqrt((endPointReference.X - start[0]) * (endPointReference.X - start[0]) +
                  (endPointReference.Y - start[1]) * (endPointReference.Y - start[1]) +
                  (endPointReference.Z - start[2]) * (endPointReference.Z - start[2]));
    distanceToEnd =
        std::sqrt((endPointReference.X - end[0]) * (endPointReference.X - end[0]) +
                  (endPointReference.Y - end[1]) * (endPointReference.Y - end[1]) +
                  (endPointReference.Z - end[2]) * (endPointReference.Z - end[2]));

    const bool reverseFromEnd = distanceToEnd < distanceToStart;
    if (reverseFromEnd)
    {
        startToEnd.X = -startToEnd.X;
        startToEnd.Y = -startToEnd.Y;
        startToEnd.Z = -startToEnd.Z;
    }

    xDirection = NormalizeVector(startToEnd);
    return VectorLength(xDirection) > 1.0e-6;
}

bool TryReadFlatPatternCsysDirection(
    NXOpen::Features::SheetMetal::FlatPatternBuilder* builder,
    NXOpen::Vector3d& normal,
    NXOpen::Vector3d& xDirection)
{
    if (builder == nullptr || builder->OrientationCsys() == nullptr)
    {
        return false;
    }

    try
    {
        NXOpen::Vector3d yDirection(0.0, 0.0, 0.0);
        builder->OrientationCsys()->GetDirections(&xDirection, &yDirection);
        xDirection = NormalizeVector(xDirection);
        yDirection = NormalizeVector(yDirection);
        normal = NormalizeVector(CrossVector(xDirection, yDirection));
        return VectorLength(xDirection) > 1.0e-6 && VectorLength(normal) > 1.0e-6;
    }
    catch (...)
    {
        return false;
    }
}

bool TryComputeAutoFrontDirectionFromSheetMetalFlatPattern(
    NXOpen::Part* part,
    AutoViewDirection& result)
{
    if (part == nullptr || part->Features() == nullptr || !HasVisibleSheetMetalBody(part))
    {
        return false;
    }

    std::vector<NXOpen::Features::Feature*> features;
    try
    {
        features = part->Features()->GetFeatures();
    }
    catch (...)
    {
        return false;
    }

    for (NXOpen::Features::Feature* feature : features)
    {
        if (feature == nullptr)
        {
            continue;
        }

        NXOpen::Features::FlatPattern* flatPattern =
            dynamic_cast<NXOpen::Features::FlatPattern*>(NXOpen::NXObjectManager::Get(feature->Tag()));
        if (flatPattern == nullptr)
        {
            continue;
        }

        NXOpen::Features::SheetMetal::FlatPatternBuilder* flatPatternBuilder = nullptr;
        try
        {
            flatPatternBuilder = part->Features()->SheetmetalManager()->CreateFlatPatternBuilder(flatPattern);
            if (flatPatternBuilder == nullptr ||
                flatPatternBuilder->UpwardFace() == nullptr ||
                flatPatternBuilder->UpwardFace()->Value() == nullptr)
            {
                if (flatPatternBuilder != nullptr)
                {
                    flatPatternBuilder->Destroy();
                }
                continue;
            }

            NXOpen::Face* upwardFace = flatPatternBuilder->UpwardFace()->Value();
            const tag_t upwardFaceTag = upwardFace->Tag();
            NXOpen::Body* upwardBody = upwardFace->GetBody();
            if (upwardBody == nullptr || upwardBody->IsBlanked() || !IsSheetMetalBody(part, upwardBody))
            {
                flatPatternBuilder->Destroy();
                continue;
            }

            NXOpen::Vector3d normal(0.0, 0.0, 0.0);
            NXOpen::Vector3d xDirection(0.0, 0.0, 0.0);
            double edgeLength = 0.0;
            tag_t edgeTag = NULL_TAG;
            bool hasDirection =
                TryReadPlanarFaceNormalWithNxOpenDirection(part, upwardFace, normal) ||
                TryReadPlanarFaceNormal(upwardFaceTag, normal);

            NXOpen::Edge* xAxisEdge = nullptr;
            try
            {
                xAxisEdge = flatPatternBuilder->XAxisEdge() != nullptr ? flatPatternBuilder->XAxisEdge()->Value() : nullptr;
            }
            catch (...)
            {
                xAxisEdge = nullptr;
            }

            if (hasDirection && xAxisEdge != nullptr)
            {
                edgeTag = xAxisEdge->Tag();
                hasDirection = TryReadStraightEdgeDirection(edgeTag, flatPatternBuilder->ReferenceVertex(), xDirection, edgeLength);
            }
            else
            {
                hasDirection = TryReadFlatPatternCsysDirection(flatPatternBuilder, normal, xDirection);
            }

            flatPatternBuilder->Destroy();
            if (!hasDirection)
            {
                continue;
            }

            xDirection = ProjectPerpendicular(xDirection, normal);
            xDirection = NormalizeVector(xDirection);
            if (VectorLength(xDirection) < 1.0e-6)
            {
                continue;
            }

            result.normal = normal;
            result.xDirection = xDirection;
            result.normalName = DominantAxisName(result.normal);
            result.xName = DominantAxisName(result.xDirection);
            result.faceArea = AskPlanarFaceArea(part, upwardFaceTag);
            result.edgeLength = edgeLength;
            result.faceTag = upwardFaceTag;
            result.edgeTag = edgeTag;
            result.source = edgeTag != NULL_TAG ?
                "sheet metal flat pattern upward face + x axis edge" :
                "sheet metal flat pattern orientation csys";
            result.valid = true;

            std::ostringstream line;
            line << "AutoCreateThreeViews: selected sheet metal front direction normal="
                 << result.normalName
                 << ", x="
                 << result.xName
                 << ", source="
                 << result.source
                 << ", faceArea="
                 << result.faceArea
                 << ", edgeLength="
                 << result.edgeLength
                 << ", faceTag="
                 << static_cast<unsigned long long>(result.faceTag)
                 << ", edgeTag="
                 << static_cast<unsigned long long>(result.edgeTag)
                 << ".";
            WriteLine(nullptr, line.str());
            return true;
        }
        catch (const NXOpen::NXException& ex)
        {
            if (flatPatternBuilder != nullptr)
            {
                try
                {
                    flatPatternBuilder->Destroy();
                }
                catch (...)
                {
                }
            }
            std::ostringstream line;
            line << "AutoCreateThreeViews: sheet metal flat pattern direction skipped, NX "
                 << ex.ErrorCode()
                 << ": "
                 << ex.Message();
            WriteLine(nullptr, line.str());
        }
        catch (...)
        {
            if (flatPatternBuilder != nullptr)
            {
                try
                {
                    flatPatternBuilder->Destroy();
                }
                catch (...)
                {
                }
            }
        }
    }

    WriteLine(nullptr, "AutoCreateThreeViews: sheet metal flat pattern direction not found.");
    return false;
}

struct AssemblyOccurrenceTransform
{
    double m[4][4] = {
        {1.0, 0.0, 0.0, 0.0},
        {0.0, 1.0, 0.0, 0.0},
        {0.0, 0.0, 1.0, 0.0},
        {0.0, 0.0, 0.0, 1.0}};
};

AssemblyOccurrenceTransform IdentityOccurrenceTransform()
{
    return AssemblyOccurrenceTransform();
}

AssemblyOccurrenceTransform MultiplyOccurrenceTransform(
    const AssemblyOccurrenceTransform& parent,
    const double child[4][4])
{
    AssemblyOccurrenceTransform result;
    for (int row = 0; row < 4; ++row)
    {
        for (int col = 0; col < 4; ++col)
        {
            result.m[row][col] = 0.0;
            for (int k = 0; k < 4; ++k)
            {
                result.m[row][col] += parent.m[row][k] * child[k][col];
            }
        }
    }
    return result;
}

NXOpen::Point3d TransformOccurrencePoint(
    const AssemblyOccurrenceTransform& transform,
    const double point[3])
{
    return NXOpen::Point3d(
        transform.m[0][0] * point[0] + transform.m[0][1] * point[1] + transform.m[0][2] * point[2] + transform.m[0][3],
        transform.m[1][0] * point[0] + transform.m[1][1] * point[1] + transform.m[1][2] * point[2] + transform.m[1][3],
        transform.m[2][0] * point[0] + transform.m[2][1] * point[1] + transform.m[2][2] * point[2] + transform.m[2][3]);
}

NXOpen::Vector3d TransformOccurrenceVector(
    const AssemblyOccurrenceTransform& transform,
    const NXOpen::Vector3d& vector)
{
    return NXOpen::Vector3d(
        transform.m[0][0] * vector.X + transform.m[0][1] * vector.Y + transform.m[0][2] * vector.Z,
        transform.m[1][0] * vector.X + transform.m[1][1] * vector.Y + transform.m[1][2] * vector.Z,
        transform.m[2][0] * vector.X + transform.m[2][1] * vector.Y + transform.m[2][2] * vector.Z);
}

void CollectLeafPartOccurrences(
    tag_t occurrence,
    const AssemblyOccurrenceTransform& parentTransform,
    std::vector<std::pair<tag_t, AssemblyOccurrenceTransform>>& leaves)
{
    if (occurrence == NULL_TAG)
    {
        return;
    }

    double localTransform[4][4] = {
        {1.0, 0.0, 0.0, 0.0},
        {0.0, 1.0, 0.0, 0.0},
        {0.0, 0.0, 1.0, 0.0},
        {0.0, 0.0, 0.0, 1.0}};
    AssemblyOccurrenceTransform occurrenceTransform = parentTransform;
    if (UF_ASSEM_ask_transform_of_occ(occurrence, localTransform) == 0)
    {
        occurrenceTransform = MultiplyOccurrenceTransform(parentTransform, localTransform);
    }

    int childCount = 0;
    tag_t* children = nullptr;
    childCount = UF_ASSEM_ask_part_occ_children(occurrence, &children);
    if (childCount <= 0 || children == nullptr)
    {
        leaves.push_back({occurrence, occurrenceTransform});
        return;
    }

    for (int index = 0; index < childCount; ++index)
    {
        if (children[index] != NULL_TAG)
        {
            CollectLeafPartOccurrences(children[index], occurrenceTransform, leaves);
        }
    }
    UF_free(children);
}

bool IsSolidBodyTag(tag_t objectTag)
{
    if (objectTag == NULL_TAG)
    {
        return false;
    }

    int type = 0;
    int subtype = 0;
    if (UF_OBJ_ask_type_and_subtype(objectTag, &type, &subtype) != 0)
    {
        return false;
    }
    return type == UF_solid_type && subtype == UF_solid_body_subtype;
}

void AddUniqueBodyOccurrence(
    tag_t bodyTag,
    std::vector<tag_t>& bodyOccurrences,
    std::set<tag_t>& seen)
{
    if (bodyTag == NULL_TAG ||
        !IsSolidBodyTag(bodyTag) ||
        !MatchesActiveTargetLayer(bodyTag) ||
        seen.find(bodyTag) != seen.end())
    {
        return;
    }

    seen.insert(bodyTag);
    bodyOccurrences.push_back(bodyTag);
}

std::vector<tag_t> CollectVisibleSolidBodyTags(NXOpen::Part* part)
{
    std::vector<tag_t> bodyTags;
    std::set<tag_t> seenBodyTags;
    if (part == nullptr)
    {
        return bodyTags;
    }

    if (part->Bodies() != nullptr)
    {
        for (NXOpen::BodyCollection::iterator it = part->Bodies()->begin(); it != part->Bodies()->end(); ++it)
        {
            NXOpen::Body* body = *it;
            if (body == nullptr || body->IsBlanked())
            {
                continue;
            }
            AddUniqueBodyOccurrence(body->Tag(), bodyTags, seenBodyTags);
        }
    }

    const tag_t rootOccurrence = UF_ASSEM_ask_root_part_occ(part->Tag());
    if (rootOccurrence == NULL_TAG)
    {
        return bodyTags;
    }

    std::vector<std::pair<tag_t, AssemblyOccurrenceTransform>> leaves;
    int rootChildCount = 0;
    tag_t* rootChildren = nullptr;
    rootChildCount = UF_ASSEM_ask_part_occ_children(rootOccurrence, &rootChildren);
    if (rootChildCount <= 0 || rootChildren == nullptr)
    {
        return bodyTags;
    }

    const AssemblyOccurrenceTransform identity = IdentityOccurrenceTransform();
    for (int index = 0; index < rootChildCount; ++index)
    {
        if (rootChildren[index] != NULL_TAG)
        {
            CollectLeafPartOccurrences(rootChildren[index], identity, leaves);
        }
    }
    UF_free(rootChildren);

    for (const auto& leaf : leaves)
    {
        const tag_t occurrence = leaf.first;
        tag_t objectOccurrence = NULL_TAG;
        while ((objectOccurrence = UF_ASSEM_cycle_ents_in_part_occ(occurrence, objectOccurrence)) != NULL_TAG)
        {
            AddUniqueBodyOccurrence(objectOccurrence, bodyTags, seenBodyTags);
        }

        const tag_t prototypeTag = UF_ASSEM_ask_prototype_of_occ(occurrence);
        if (prototypeTag == NULL_TAG)
        {
            continue;
        }

        tag_t prototypeBody = NULL_TAG;
        while (UF_OBJ_cycle_objs_in_part(prototypeTag, UF_solid_type, &prototypeBody) == 0 &&
               prototypeBody != NULL_TAG)
        {
            if (!IsSolidBodyTag(prototypeBody))
            {
                continue;
            }
            AddUniqueBodyOccurrence(UF_ASSEM_find_occurrence(occurrence, prototypeBody), bodyTags, seenBodyTags);
        }
    }

    return bodyTags;
}

std::string ReadObjectName(tag_t objectTag)
{
    if (objectTag == NULL_TAG)
    {
        return "";
    }

    NXOpen::NXObject* object = nullptr;
    try
    {
        object = dynamic_cast<NXOpen::NXObject*>(NXOpen::NXObjectManager::Get(objectTag));
    }
    catch (...)
    {
        object = nullptr;
    }
    if (object != nullptr)
    {
        try
        {
            return object->Name().GetLocaleText();
        }
        catch (...)
        {
        }
    }

    char name[MAX_FSPEC_BUFSIZE] = {};
    if (UF_OBJ_ask_name(objectTag, name) == 0)
    {
        return std::string(name);
    }
    return "";
}

bool BodyNameLooksLikeFastener(tag_t bodyTag)
{
    std::string text = ReadObjectName(bodyTag);
    const tag_t prototype = UF_ASSEM_ask_prototype_of_occ(bodyTag);
    if (prototype != NULL_TAG)
    {
        text += " ";
        text += ReadObjectName(prototype);
    }

    return ContainsAnyKeyword(
        text,
        {
            u8"螺母",
            u8"螺钉",
            u8"螺柱",
            u8"螺栓",
            "nut",
            "screw",
            "bolt",
            "stud",
            "fastener"});
}

std::vector<NXOpen::Point3d> CollectBodyEdgePoints(tag_t bodyTag)
{
    std::vector<NXOpen::Point3d> points;
    if (bodyTag == NULL_TAG)
    {
        return points;
    }

    uf_list_p_t faceList = nullptr;
    if (UF_MODL_ask_body_faces(bodyTag, &faceList) != 0 || faceList == nullptr)
    {
        return points;
    }

    int faceCount = 0;
    UF_MODL_ask_list_count(faceList, &faceCount);
    std::set<tag_t> seenEdges;
    for (int faceIndex = 0; faceIndex < faceCount; ++faceIndex)
    {
        tag_t faceTag = NULL_TAG;
        if (UF_MODL_ask_list_item(faceList, faceIndex, &faceTag) != 0 || faceTag == NULL_TAG)
        {
            continue;
        }

        uf_list_p_t edgeList = nullptr;
        if (UF_MODL_ask_face_edges(faceTag, &edgeList) != 0 || edgeList == nullptr)
        {
            continue;
        }

        int edgeCount = 0;
        UF_MODL_ask_list_count(edgeList, &edgeCount);
        for (int edgeIndex = 0; edgeIndex < edgeCount; ++edgeIndex)
        {
            tag_t edgeTag = NULL_TAG;
            if (UF_MODL_ask_list_item(edgeList, edgeIndex, &edgeTag) != 0 ||
                edgeTag == NULL_TAG ||
                seenEdges.find(edgeTag) != seenEdges.end())
            {
                continue;
            }
            seenEdges.insert(edgeTag);

            double start[3] = {0.0, 0.0, 0.0};
            double end[3] = {0.0, 0.0, 0.0};
            int vertexCount = 0;
            if (UF_MODL_ask_edge_verts(edgeTag, start, end, &vertexCount) == 0 && vertexCount >= 1)
            {
                points.push_back(NXOpen::Point3d(start[0], start[1], start[2]));
                if (vertexCount >= 2)
                {
                    points.push_back(NXOpen::Point3d(end[0], end[1], end[2]));
                }
            }
        }
        UF_MODL_delete_list(&edgeList);
    }

    UF_MODL_delete_list(&faceList);
    return points;
}

bool BodyGeometryLooksLikeFastener(tag_t bodyTag)
{
    if (bodyTag == NULL_TAG)
    {
        return false;
    }

    uf_list_p_t faceList = nullptr;
    if (UF_MODL_ask_body_faces(bodyTag, &faceList) != 0 || faceList == nullptr)
    {
        return false;
    }

    const std::vector<NXOpen::Point3d> bodyPoints = CollectBodyEdgePoints(bodyTag);
    bool matched = false;
    int cylinderFaceCount = 0;
    int strongCylinderCount = 0;
    int faceCount = 0;
    UF_MODL_ask_list_count(faceList, &faceCount);
    for (int faceIndex = 0; faceIndex < faceCount; ++faceIndex)
    {
        tag_t faceTag = NULL_TAG;
        if (UF_MODL_ask_list_item(faceList, faceIndex, &faceTag) != 0 || faceTag == NULL_TAG)
        {
            continue;
        }

        int faceType = 0;
        double axisPoint[3] = {0.0, 0.0, 0.0};
        double axisDirection[3] = {0.0, 0.0, 1.0};
        double box[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
        double radius = 0.0;
        double radData = 0.0;
        int normDir = 1;
        if (UF_MODL_ask_face_data(faceTag, &faceType, axisPoint, axisDirection, box, &radius, &radData, &normDir) != 0 ||
            faceType != UF_MODL_CYLINDRICAL_FACE ||
            radius <= 1.0e-6)
        {
            continue;
        }
        ++cylinderFaceCount;

        NXOpen::Vector3d axis(axisDirection[0], axisDirection[1], axisDirection[2]);
        axis = NormalizeVector(axis);
        if (VectorLength(axis) <= 1.0e-6)
        {
            continue;
        }

        double minProjection = std::numeric_limits<double>::max();
        double maxProjection = -std::numeric_limits<double>::max();
        for (const NXOpen::Point3d& point : bodyPoints)
        {
            const NXOpen::Vector3d delta(point.X - axisPoint[0], point.Y - axisPoint[1], point.Z - axisPoint[2]);
            const double projection = DotVector(delta, axis);
            minProjection = std::min(minProjection, projection);
            maxProjection = std::max(maxProjection, projection);
        }

        const double height = minProjection <= maxProjection ? maxProjection - minProjection : 0.0;
        const double diameter = radius * 2.0;
        const double ratio = diameter > 1.0e-6 ? height / diameter : 0.0;
        if (height > diameter * 0.30 + 1.0e-4 && ratio <= 12.0)
        {
            ++strongCylinderCount;
        }
    }

    UF_MODL_delete_list(&faceList);
    matched = strongCylinderCount > 0 && cylinderFaceCount >= 1;
    return matched;
}

bool ShouldSkipFastenerBodyForParallelDimensions(tag_t bodyTag)
{
    try
    {
        return BodyNameLooksLikeFastener(bodyTag) || BodyGeometryLooksLikeFastener(bodyTag);
    }
    catch (...)
    {
        return false;
    }
}

bool AskSolidBodyTagBoundsCenter(NXOpen::Part* part, NXOpen::Point3d& center)
{
    const std::vector<tag_t> bodyTags = CollectVisibleSolidBodyTags(part);
    if (bodyTags.empty())
    {
        return false;
    }

    bool initialized = false;
    double minValues[3] = {0.0, 0.0, 0.0};
    double maxValues[3] = {0.0, 0.0, 0.0};
    for (tag_t bodyTag : bodyTags)
    {
        double box[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
        if (UF_MODL_ask_bounding_box(bodyTag, box) != 0)
        {
            continue;
        }

        if (!initialized)
        {
            minValues[0] = box[0];
            minValues[1] = box[1];
            minValues[2] = box[2];
            maxValues[0] = box[3];
            maxValues[1] = box[4];
            maxValues[2] = box[5];
            initialized = true;
        }
        else
        {
            minValues[0] = std::min(minValues[0], box[0]);
            minValues[1] = std::min(minValues[1], box[1]);
            minValues[2] = std::min(minValues[2], box[2]);
            maxValues[0] = std::max(maxValues[0], box[3]);
            maxValues[1] = std::max(maxValues[1], box[4]);
            maxValues[2] = std::max(maxValues[2], box[5]);
        }
    }

    if (!initialized)
    {
        return false;
    }

    center = NXOpen::Point3d(
        (minValues[0] + maxValues[0]) * 0.5,
        (minValues[1] + maxValues[1]) * 0.5,
        (minValues[2] + maxValues[2]) * 0.5);
    return true;
}

bool TryComputeAutoFrontDirectionFromLeafAssemblyBodies(
    NXOpen::Part* assemblyPart,
    AutoViewDirection& result)
{
    if (assemblyPart == nullptr)
    {
        return false;
    }

    const tag_t rootOccurrence = UF_ASSEM_ask_root_part_occ(assemblyPart->Tag());
    if (rootOccurrence == NULL_TAG)
    {
        return false;
    }

    std::vector<std::pair<tag_t, AssemblyOccurrenceTransform>> leaves;
    int rootChildCount = 0;
    tag_t* rootChildren = nullptr;
    rootChildCount = UF_ASSEM_ask_part_occ_children(rootOccurrence, &rootChildren);
    if (rootChildCount <= 0 || rootChildren == nullptr)
    {
        return false;
    }

    const AssemblyOccurrenceTransform identity = IdentityOccurrenceTransform();
    for (int index = 0; index < rootChildCount; ++index)
    {
        if (rootChildren[index] != NULL_TAG)
        {
            CollectLeafPartOccurrences(rootChildren[index], identity, leaves);
        }
    }
    UF_free(rootChildren);

    struct AssemblyPlanarFaceCandidate
    {
        tag_t occurrence = NULL_TAG;
        tag_t faceTag = NULL_TAG;
        tag_t edgeTag = NULL_TAG;
        NXOpen::Vector3d normal = NXOpen::Vector3d(0.0, 0.0, 0.0);
        NXOpen::Vector3d xDirection = NXOpen::Vector3d(0.0, 0.0, 0.0);
        double area = 0.0;
        double edgeLength = 0.0;
    };

    AssemblyPlanarFaceCandidate best;
    bool found = false;
    int leafCount = 0;
    int directObjectOccurrenceCount = 0;
    int directBodyOccurrenceCount = 0;
    int prototypeBodyCount = 0;
    int mappedBodyOccurrenceCount = 0;
    int bodyCount = 0;
    int planarFaceCount = 0;

    for (const auto& leaf : leaves)
    {
        const tag_t occurrence = leaf.first;
        ++leafCount;

        std::vector<tag_t> bodyOccurrences;
        std::set<tag_t> seenBodyOccurrences;
        tag_t objectOccurrence = NULL_TAG;
        while ((objectOccurrence = UF_ASSEM_cycle_ents_in_part_occ(occurrence, objectOccurrence)) != NULL_TAG)
        {
            ++directObjectOccurrenceCount;
            const size_t before = bodyOccurrences.size();
            AddUniqueBodyOccurrence(objectOccurrence, bodyOccurrences, seenBodyOccurrences);
            if (bodyOccurrences.size() > before)
            {
                ++directBodyOccurrenceCount;
            }
        }

        const tag_t prototypeTag = UF_ASSEM_ask_prototype_of_occ(occurrence);
        if (prototypeTag != NULL_TAG)
        {
            tag_t prototypeBody = NULL_TAG;
            while (UF_OBJ_cycle_objs_in_part(prototypeTag, UF_solid_type, &prototypeBody) == 0 &&
                   prototypeBody != NULL_TAG)
            {
                if (!IsSolidBodyTag(prototypeBody))
                {
                    continue;
                }
                ++prototypeBodyCount;

                const tag_t mappedOccurrence = UF_ASSEM_find_occurrence(occurrence, prototypeBody);
                const size_t before = bodyOccurrences.size();
                AddUniqueBodyOccurrence(mappedOccurrence, bodyOccurrences, seenBodyOccurrences);
                if (bodyOccurrences.size() > before)
                {
                    ++mappedBodyOccurrenceCount;
                }
            }
        }

        for (tag_t bodyOccurrence : bodyOccurrences)
        {
            ++bodyCount;

            uf_list_p_t faceList = nullptr;
            if (UF_MODL_ask_body_faces(bodyOccurrence, &faceList) != 0 || faceList == nullptr)
            {
                continue;
            }

            int faceCount = 0;
            UF_MODL_ask_list_count(faceList, &faceCount);
            for (int faceIndex = 0; faceIndex < faceCount; ++faceIndex)
            {
                tag_t faceTag = NULL_TAG;
                if (UF_MODL_ask_list_item(faceList, faceIndex, &faceTag) != 0 || faceTag == NULL_TAG)
                {
                    continue;
                }

                int faceType = 0;
                double point[3] = {0.0, 0.0, 0.0};
                double normalData[3] = {0.0, 0.0, 0.0};
                double box[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
                double radius = 0.0;
                double radData = 0.0;
                int normDir = 1;
                if (UF_MODL_ask_face_data(faceTag, &faceType, point, normalData, box, &radius, &radData, &normDir) != 0 ||
                    faceType != 22)
                {
                    continue;
                }

                NXOpen::Vector3d faceNormal(
                    normalData[0] * static_cast<double>(normDir),
                    normalData[1] * static_cast<double>(normDir),
                    normalData[2] * static_cast<double>(normDir));
                faceNormal = NormalizeVector(faceNormal);
                if (VectorLength(faceNormal) < 1.0e-6)
                {
                    continue;
                }
                StabilizeDirectionSign(faceNormal);

                double area = AskPlanarFaceArea(assemblyPart, faceTag);
                if (area <= 1.0e-6)
                {
                    area = ApproximatePlanarFaceArea(normalData, box);
                }
                if (area <= 1.0e-6)
                {
                    continue;
                }
                ++planarFaceCount;

                double longestEdge = 0.0;
                tag_t longestEdgeTag = NULL_TAG;
                NXOpen::Vector3d longestDirection(0.0, 0.0, 0.0);
                uf_list_p_t edgeList = nullptr;
                if (UF_MODL_ask_face_edges(faceTag, &edgeList) == 0 && edgeList != nullptr)
                {
                    int edgeCount = 0;
                    UF_MODL_ask_list_count(edgeList, &edgeCount);
                    for (int edgeIndex = 0; edgeIndex < edgeCount; ++edgeIndex)
                    {
                        tag_t edgeTag = NULL_TAG;
                        if (UF_MODL_ask_list_item(edgeList, edgeIndex, &edgeTag) != 0 || edgeTag == NULL_TAG)
                        {
                            continue;
                        }

                        double start[3] = {0.0, 0.0, 0.0};
                        double end[3] = {0.0, 0.0, 0.0};
                        int vertexCount = 0;
                        if (UF_MODL_ask_edge_verts(edgeTag, start, end, &vertexCount) != 0 ||
                            vertexCount < 2 ||
                            !IsStraightEdge(edgeTag, start, end))
                        {
                            continue;
                        }

                        NXOpen::Vector3d edgeVector(
                            end[0] - start[0],
                            end[1] - start[1],
                            end[2] - start[2]);
                        const double edgeLength = VectorLength(edgeVector);
                        if (edgeLength <= longestEdge)
                        {
                            continue;
                        }

                        longestEdge = edgeLength;
                        longestEdgeTag = edgeTag;
                        longestDirection = edgeVector;
                    }
                    UF_MODL_delete_list(&edgeList);
                }

                if (longestEdge <= 1.0e-6 || longestEdgeTag == NULL_TAG)
                {
                    continue;
                }
                longestDirection = ProjectPerpendicular(longestDirection, faceNormal);
                longestDirection = NormalizeVector(longestDirection);
                if (VectorLength(longestDirection) < 1.0e-6)
                {
                    continue;
                }
                StabilizeDirectionSign(longestDirection);

                if (!found ||
                    area > best.area + 1.0e-6 ||
                    (std::abs(area - best.area) <= 1.0e-6 && longestEdge > best.edgeLength))
                {
                    found = true;
                    best.occurrence = occurrence;
                    best.faceTag = faceTag;
                    best.edgeTag = longestEdgeTag;
                    best.normal = faceNormal;
                    best.xDirection = longestDirection;
                    best.area = area;
                    best.edgeLength = longestEdge;
                }
            }

            UF_MODL_delete_list(&faceList);
        }
    }

    if (!found)
    {
        std::ostringstream line;
        line << "AutoCreateThreeViews: assembly leaf front direction not found"
             << ", leaves=" << leaves.size()
             << ", visitedLeaves=" << leafCount
             << ", objectOccurrences=" << directObjectOccurrenceCount
             << ", directBodies=" << directBodyOccurrenceCount
             << ", prototypeBodies=" << prototypeBodyCount
             << ", mappedBodies=" << mappedBodyOccurrenceCount
             << ", bodies=" << bodyCount
             << ", planarFaces=" << planarFaceCount << ".";
        WriteLine(nullptr, line.str());
        return false;
    }

    result.normal = best.normal;
    result.xDirection = best.xDirection;
    if (DominantAxisName(result.normal) == "Y" && DominantAxisName(result.xDirection) == "X")
    {
        result.xDirection.X = -result.xDirection.X;
        result.xDirection.Y = -result.xDirection.Y;
        result.xDirection.Z = -result.xDirection.Z;
    }
    result.normalName = DominantAxisName(result.normal);
    result.xName = DominantAxisName(result.xDirection);
    result.faceArea = best.area;
    result.edgeLength = best.edgeLength;
    result.faceTag = best.faceTag;
    result.edgeTag = best.edgeTag;
    result.source = "assembly leaf component largest planar face + longest straight edge";
    result.valid = true;

    std::ostringstream line;
    line << "AutoCreateThreeViews: selected assembly leaf front direction normal="
         << result.normalName
         << ", x="
         << result.xName
         << ", faceArea="
         << result.faceArea
         << ", edgeLength="
         << result.edgeLength
         << ", occurrence="
         << static_cast<unsigned long long>(best.occurrence)
         << ", faceTag="
         << static_cast<unsigned long long>(result.faceTag)
         << ", edgeTag="
         << static_cast<unsigned long long>(result.edgeTag)
         << ", leaves="
         << leaves.size()
         << ", objectOccurrences="
         << directObjectOccurrenceCount
         << ", directBodies="
         << directBodyOccurrenceCount
         << ", prototypeBodies="
         << prototypeBodyCount
         << ", mappedBodies="
         << mappedBodyOccurrenceCount
         << ".";
    WriteLine(nullptr, line.str());
    return true;
}

bool TryComputeAutoFrontDirectionFromOverallBoundingBox(
    NXOpen::Part* part,
    AutoViewDirection& result)
{
    const ModelBounds bounds = AskModelBounds(part);
    if (!bounds.valid)
    {
        return false;
    }

    struct BoxProjectionCandidate
    {
        int normalAxis = 2;
        int horizontalAxis = 0;
        int verticalAxis = 1;
        double area = 0.0;
        double horizontalLength = 0.0;
        double verticalLength = 0.0;
    };

    const double sizes[3] = {bounds.sizeX, bounds.sizeY, bounds.sizeZ};
    std::vector<BoxProjectionCandidate> candidates;
    for (int normalAxis = 0; normalAxis < 3; ++normalAxis)
    {
        int projectedAxes[2] = {0, 1};
        int out = 0;
        for (int axis = 0; axis < 3; ++axis)
        {
            if (axis != normalAxis)
            {
                projectedAxes[out++] = axis;
            }
        }

        BoxProjectionCandidate candidate;
        candidate.normalAxis = normalAxis;
        candidate.area = sizes[projectedAxes[0]] * sizes[projectedAxes[1]];
        if (sizes[projectedAxes[0]] >= sizes[projectedAxes[1]])
        {
            candidate.horizontalAxis = projectedAxes[0];
            candidate.verticalAxis = projectedAxes[1];
        }
        else
        {
            candidate.horizontalAxis = projectedAxes[1];
            candidate.verticalAxis = projectedAxes[0];
        }
        candidate.horizontalLength = sizes[candidate.horizontalAxis];
        candidate.verticalLength = sizes[candidate.verticalAxis];
        candidates.push_back(candidate);
    }

    const BoxProjectionCandidate* best = nullptr;
    for (const BoxProjectionCandidate& candidate : candidates)
    {
        if (best == nullptr ||
            candidate.area > best->area + 1.0e-6 ||
            (std::abs(candidate.area - best->area) <= 1.0e-6 &&
             candidate.horizontalLength > best->horizontalLength + 1.0e-6))
        {
            best = &candidate;
        }
    }
    if (best == nullptr || best->area <= 1.0e-6 || best->horizontalLength <= 1.0e-6)
    {
        return false;
    }

    result.normal = AxisVector(best->normalAxis, 1.0);
    result.xDirection = AxisVector(best->horizontalAxis, 1.0);
    StabilizeDirectionSign(result.normal);
    StabilizeDirectionSign(result.xDirection);
    result.normalName = DominantAxisName(result.normal);
    result.xName = DominantAxisName(result.xDirection);
    result.faceArea = best->area;
    result.edgeLength = best->horizontalLength;
    result.faceTag = NULL_TAG;
    result.edgeTag = NULL_TAG;
    result.source = "overall bounding box max projected area + longest projected direction";
    result.valid = true;

    std::ostringstream line;
    line << "AutoCreateThreeViews: selected front overall bounding box normal="
         << result.normalName
         << ", x="
         << result.xName
         << ", projectionArea="
         << result.faceArea
         << ", horizontalLength="
         << result.edgeLength
         << ", verticalLength="
         << best->verticalLength
         << ", sizeX="
         << bounds.sizeX
         << ", sizeY="
         << bounds.sizeY
         << ", sizeZ="
         << bounds.sizeZ
         << ".";
    WriteLine(nullptr, line.str());
    return true;
}

bool TryComputeAutoFrontDirectionFromLargestPlanarFace(
    NXOpen::Part* part,
    AutoViewDirection& result)
{
    if (part == nullptr || part->Bodies() == nullptr)
    {
        return false;
    }

    struct PlanarFaceCandidate
    {
        tag_t faceTag = NULL_TAG;
        tag_t edgeTag = NULL_TAG;
        NXOpen::Vector3d normal = NXOpen::Vector3d(0.0, 0.0, 0.0);
        NXOpen::Vector3d xDirection = NXOpen::Vector3d(0.0, 0.0, 0.0);
        double offset = 0.0;
        double area = 0.0;
        double box[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
        double edgeLength = 0.0;
    };

    struct CoplanarFaceGroup
    {
        NXOpen::Vector3d normal = NXOpen::Vector3d(0.0, 0.0, 0.0);
        double offset = 0.0;
        double area = 0.0;
        double minBox[3] = {0.0, 0.0, 0.0};
        double maxBox[3] = {0.0, 0.0, 0.0};
        bool hasBox = false;
        double edgeLength = 0.0;
        tag_t faceTag = NULL_TAG;
        tag_t edgeTag = NULL_TAG;
        NXOpen::Vector3d xDirection = NXOpen::Vector3d(0.0, 0.0, 0.0);
    };

    std::vector<PlanarFaceCandidate> candidates;
    std::vector<CoplanarFaceGroup> groups;
    const double normalTolerance = 0.9995;
    const double offsetTolerance = 0.5;

    for (NXOpen::BodyCollection::iterator it = part->Bodies()->begin(); it != part->Bodies()->end(); ++it)
    {
        NXOpen::Body* body = *it;
        if (body == nullptr || body->IsBlanked() || !MatchesActiveTargetLayer(body->Tag()))
        {
            continue;
        }

        uf_list_p_t faceList = nullptr;
        if (UF_MODL_ask_body_faces(body->Tag(), &faceList) != 0 || faceList == nullptr)
        {
            continue;
        }

        int faceCount = 0;
        UF_MODL_ask_list_count(faceList, &faceCount);
        for (int faceIndex = 0; faceIndex < faceCount; ++faceIndex)
        {
            tag_t faceTag = NULL_TAG;
            if (UF_MODL_ask_list_item(faceList, faceIndex, &faceTag) != 0 || faceTag == NULL_TAG)
            {
                continue;
            }

            int faceType = 0;
            double point[3] = {0.0, 0.0, 0.0};
            double normal[3] = {0.0, 0.0, 0.0};
            double box[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
            double radius = 0.0;
            double radData = 0.0;
            int normDir = 1;
            if (UF_MODL_ask_face_data(faceTag, &faceType, point, normal, box, &radius, &radData, &normDir) != 0 ||
                faceType != 22)
            {
                continue;
            }

            NXOpen::Vector3d faceNormal(
                normal[0] * static_cast<double>(normDir),
                normal[1] * static_cast<double>(normDir),
                normal[2] * static_cast<double>(normDir));
            faceNormal = NormalizeVector(faceNormal);
            if (VectorLength(faceNormal) < 1.0e-6)
            {
                continue;
            }
            StabilizeDirectionSign(faceNormal);

            const double area = AskPlanarFaceArea(part, faceTag);
            if (area <= 1.0e-6)
            {
                continue;
            }

            double longestEdge = 0.0;
            tag_t longestEdgeTag = NULL_TAG;
            NXOpen::Vector3d longestDirection(0.0, 0.0, 0.0);
            uf_list_p_t edgeList = nullptr;
            if (UF_MODL_ask_face_edges(faceTag, &edgeList) == 0 && edgeList != nullptr)
            {
                int edgeCount = 0;
                UF_MODL_ask_list_count(edgeList, &edgeCount);
                for (int edgeIndex = 0; edgeIndex < edgeCount; ++edgeIndex)
                {
                    tag_t edgeTag = NULL_TAG;
                    if (UF_MODL_ask_list_item(edgeList, edgeIndex, &edgeTag) != 0 || edgeTag == NULL_TAG)
                    {
                        continue;
                    }

                    double start[3] = {0.0, 0.0, 0.0};
                    double end[3] = {0.0, 0.0, 0.0};
                    int vertexCount = 0;
                    if (UF_MODL_ask_edge_verts(edgeTag, start, end, &vertexCount) != 0 || vertexCount < 2)
                    {
                        continue;
                    }

                    NXOpen::Vector3d edgeVector(end[0] - start[0], end[1] - start[1], end[2] - start[2]);
                    const double edgeLength = VectorLength(edgeVector);
                    if (edgeLength <= longestEdge || !IsStraightEdge(edgeTag, start, end))
                    {
                        continue;
                    }

                    longestEdge = edgeLength;
                    longestEdgeTag = edgeTag;
                    longestDirection = edgeVector;
                }

                UF_MODL_delete_list(&edgeList);
            }

            if (longestEdge > 1.0e-6)
            {
                longestDirection = ProjectPerpendicular(longestDirection, faceNormal);
                longestDirection = NormalizeVector(longestDirection);
            }
            else
            {
                continue;
            }
            if (VectorLength(longestDirection) < 1.0e-6)
            {
                continue;
            }
            StabilizeDirectionSign(longestDirection);

            PlanarFaceCandidate candidate;
            candidate.faceTag = faceTag;
            candidate.edgeTag = longestEdgeTag;
            candidate.normal = faceNormal;
            candidate.xDirection = longestDirection;
            candidate.offset = faceNormal.X * point[0] + faceNormal.Y * point[1] + faceNormal.Z * point[2];
            candidate.area = area;
            for (int axis = 0; axis < 3; ++axis)
            {
                candidate.box[axis] = box[axis];
                candidate.box[axis + 3] = box[axis + 3];
            }
            candidate.edgeLength = longestEdge;

            candidates.push_back(candidate);

            bool merged = false;
            for (CoplanarFaceGroup& group : groups)
            {
                const double dot = group.normal.X * candidate.normal.X +
                                   group.normal.Y * candidate.normal.Y +
                                   group.normal.Z * candidate.normal.Z;
                if (dot >= normalTolerance && std::abs(group.offset - candidate.offset) <= offsetTolerance)
                {
                    group.area += candidate.area;
                    for (int axis = 0; axis < 3; ++axis)
                    {
                        group.minBox[axis] = std::min(group.minBox[axis], candidate.box[axis]);
                        group.maxBox[axis] = std::max(group.maxBox[axis], candidate.box[axis + 3]);
                    }
                    if (candidate.edgeLength > group.edgeLength)
                    {
                        group.edgeLength = candidate.edgeLength;
                        group.edgeTag = candidate.edgeTag;
                        group.xDirection = candidate.xDirection;
                    }
                    if (candidate.area > 0.0 && group.faceTag == NULL_TAG)
                    {
                        group.faceTag = candidate.faceTag;
                    }
                    merged = true;
                    break;
                }
            }

            if (!merged)
            {
                CoplanarFaceGroup group;
                group.normal = candidate.normal;
                group.offset = candidate.offset;
                group.area = candidate.area;
                for (int axis = 0; axis < 3; ++axis)
                {
                    group.minBox[axis] = candidate.box[axis];
                    group.maxBox[axis] = candidate.box[axis + 3];
                }
                group.hasBox = true;
                group.edgeLength = candidate.edgeLength;
                group.faceTag = candidate.faceTag;
                group.edgeTag = candidate.edgeTag;
                group.xDirection = candidate.xDirection;
                groups.push_back(group);
            }
        }

        UF_MODL_delete_list(&faceList);
    }

    if (candidates.empty())
    {
        return false;
    }

    const PlanarFaceCandidate* best = nullptr;
    for (const PlanarFaceCandidate& candidate : candidates)
    {
        if (best == nullptr ||
            candidate.area > best->area + 1.0e-6 ||
            (std::abs(candidate.area - best->area) <= 1.0e-6 && candidate.edgeLength > best->edgeLength))
        {
            best = &candidate;
        }
    }

    if (best == nullptr ||
        best->area <= 1.0e-6 ||
        best->edgeLength <= 1.0e-6 ||
        best->edgeTag == NULL_TAG ||
        VectorLength(best->xDirection) < 1.0e-6)
    {
        return false;
    }

    result.normal = best->normal;
    result.xDirection = best->xDirection;
    if (DominantAxisName(result.normal) == "Y" && DominantAxisName(result.xDirection) == "X")
    {
        result.xDirection.X = -result.xDirection.X;
        result.xDirection.Y = -result.xDirection.Y;
        result.xDirection.Z = -result.xDirection.Z;
    }
    result.normalName = DominantAxisName(best->normal);
    result.xName = DominantAxisName(result.xDirection);
    result.faceArea = best->area;
    result.edgeLength = best->edgeLength;
    result.faceTag = best->faceTag;
    result.edgeTag = best->edgeTag;
    result.source = "largest visible single planar face + longest straight edge";
    result.valid = true;
    {
        std::ostringstream line;
        line << "AutoCreateThreeViews: selected front single face normal="
             << result.normalName
             << ", x="
             << result.xName
             << ", faceArea="
             << result.faceArea
             << ", edgeLength="
             << result.edgeLength
             << ", faceTag="
             << static_cast<unsigned long long>(result.faceTag)
             << ", edgeTag="
             << static_cast<unsigned long long>(result.edgeTag)
             << ".";
        WriteLine(nullptr, line.str());
    }
    return true;
}

NXOpen::TaggedObject* SelectSingleSolidObject(
    const char* message,
    const char* title,
    int solidSubtype,
    NXOpen::Point3d* selectedCursor = nullptr)
{
    NXOpen::UI* ui = nullptr;
    NXOpen::Selection* selection = nullptr;
    try
    {
        ui = NXOpen::UI::GetUI();
        selection = ui != nullptr ? ui->SelectionManager() : nullptr;
    }
    catch (...)
    {
        selection = nullptr;
    }

    if (selection == nullptr)
    {
        return nullptr;
    }

    std::vector<NXOpen::Selection::MaskTriple> maskArray;
    maskArray.emplace_back(UF_solid_type, 0, solidSubtype);
    NXOpen::TaggedObject* selectedObject = nullptr;
    NXOpen::Point3d cursor(0.0, 0.0, 0.0);
    NXOpen::Selection::Response response = NXOpen::Selection::ResponseCancel;
    try
    {
        response = selection->SelectTaggedObject(
            message,
            title,
            NXOpen::Selection::SelectionScopeAnyInAssembly,
            NXOpen::Selection::SelectionActionClearAndEnableSpecific,
            false,
            true,
            maskArray,
            &selectedObject,
            &cursor);
    }
    catch (...)
    {
        selectedObject = nullptr;
    }

    if (response != NXOpen::Selection::ResponseObjectSelected &&
        response != NXOpen::Selection::ResponseObjectSelectedByName)
    {
        std::ostringstream line;
        line << "AutoCreateThreeViews: selection canceled or skipped, title="
             << title
             << ", response="
             << static_cast<int>(response)
             << ".";
        WriteLine(nullptr, line.str());
        return nullptr;
    }

    if (selectedCursor != nullptr)
    {
        *selectedCursor = cursor;
    }

    return selectedObject;
}

bool TryComputeFrontDirectionFromSelectedFaceAndX(
    NXOpen::Part* part,
    AutoViewDirection& result)
{
    if (part == nullptr)
    {
        return false;
    }

    WriteLine(nullptr, "AutoCreateThreeViews: manual front direction selection started.");
    NXOpen::TaggedObject* selectedFaceObject = SelectSingleSolidObject(
        u8"请选择主视图基面（平面）",
        u8"自动创建三视图 - 选择基面",
        UF_UI_SEL_FEATURE_ANY_FACE);
    NXOpen::Face* selectedFace = dynamic_cast<NXOpen::Face*>(selectedFaceObject);
    if (selectedFace == nullptr)
    {
        WriteLine(nullptr, "AutoCreateThreeViews: manual front direction canceled or no planar face selected.");
        return false;
    }
    {
        std::ostringstream line;
        line << "AutoCreateThreeViews: manual front direction face selected, tag="
             << static_cast<unsigned long long>(selectedFace->Tag())
             << ".";
        WriteLine(nullptr, line.str());
    }

    NXOpen::Point3d edgeCursor(0.0, 0.0, 0.0);
    NXOpen::TaggedObject* selectedEdgeObject = SelectSingleSolidObject(
        u8"请选择主视图 X 方向直边",
        u8"自动创建三视图 - 选择 X 向直边",
        UF_UI_SEL_FEATURE_ANY_EDGE,
        &edgeCursor);
    NXOpen::Edge* selectedEdge = dynamic_cast<NXOpen::Edge*>(selectedEdgeObject);

    if (selectedFace == nullptr || selectedEdge == nullptr)
    {
        WriteLine(nullptr, "AutoCreateThreeViews: manual front direction canceled or no straight edge selected.");
        return false;
    }

    NXOpen::Vector3d normal(0.0, 0.0, 0.0);
    if (!TryReadPlanarFaceNormal(selectedFace->Tag(), normal))
    {
        WriteLine(nullptr, "AutoCreateThreeViews: selected front face is not planar.");
        return false;
    }
    CorrectSelectedPlanarFaceOuterNormal(part, selectedFace->Tag(), normal);

    NXOpen::Vector3d xDirection(0.0, 0.0, 0.0);
    double edgeLength = 0.0;
    double edgeCursorDistanceToStart = 0.0;
    double edgeCursorDistanceToEnd = 0.0;
    if (!TryReadStraightEdgeDirectionTowardPoint(
            selectedEdge->Tag(),
            edgeCursor,
            xDirection,
            edgeLength,
            edgeCursorDistanceToStart,
            edgeCursorDistanceToEnd))
    {
        WriteLine(nullptr, "AutoCreateThreeViews: selected X direction object is not a straight edge.");
        return false;
    }

    xDirection = ProjectPerpendicular(xDirection, normal);
    xDirection = NormalizeVector(xDirection);
    if (VectorLength(xDirection) < 1.0e-6)
    {
        WriteLine(nullptr, "AutoCreateThreeViews: selected X direction edge is parallel to selected face normal.");
        return false;
    }

    const NXOpen::Vector3d selectedOuterNormal = normal;
    const NXOpen::Vector3d drawingViewNormal = selectedOuterNormal;

    result.normal = drawingViewNormal;
    result.xDirection = xDirection;
    result.normalName = DominantAxisName(result.normal);
    result.xName = DominantAxisName(result.xDirection);
    result.faceArea = AskPlanarFaceArea(part, selectedFace->Tag());
    result.edgeLength = edgeLength;
    result.faceTag = selectedFace->Tag();
    result.edgeTag = selectedEdge->Tag();
    result.source = "selected planar face + selected straight edge";
    result.valid = true;

    std::ostringstream line;
    line << "AutoCreateThreeViews: selected manual front direction selectedFaceOuterNormal="
         << DominantAxisName(selectedOuterNormal)
         << ", drawingNormal="
         << result.normalName
         << ", x="
         << result.xName
         << ", faceArea="
         << result.faceArea
         << ", edgeLength="
         << result.edgeLength
         << ", edgeCursorDistToStart="
         << edgeCursorDistanceToStart
         << ", edgeCursorDistToEnd="
         << edgeCursorDistanceToEnd
         << ", xDirectionFromPickedEnd="
         << (edgeCursorDistanceToEnd < edgeCursorDistanceToStart ? "end-to-start" : "start-to-end")
         << ", faceTag="
         << static_cast<unsigned long long>(result.faceTag)
         << ", edgeTag="
         << static_cast<unsigned long long>(result.edgeTag)
         << ".";
    WriteLine(nullptr, line.str());
    return true;
}

std::string NormalizeFrontDirectionMode(const std::string& value)
{
    std::string mode = Trim(value);
    std::transform(mode.begin(), mode.end(), mode.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

    if (mode == "flatpatternx" || mode == "flat" || mode == "sheetmetalflatpattern")
    {
        return "flatPatternX";
    }
    if (mode == "manualfacex" || mode == "manual" || mode == "selectedfacex")
    {
        return "manualFaceX";
    }
    if (mode == "absolutecoordinate" || mode == "absolute" || mode == "absoluteview")
    {
        return "absoluteCoordinate";
    }
    return "overallBoxMaxArea";
}

std::string FrontDirectionFailureMessage(const std::string& mode)
{
    if (mode == "flatPatternX")
    {
        return u8"主视图创建失败：未找到钣金展开基面或展开 X 向。";
    }
    if (mode == "manualFaceX")
    {
        return u8"主视图创建失败：请先选择一个平面和一条直边作为 X 向。";
    }
    return u8"主视图创建失败：未找到带直边的最大可见平面。";
}

AutoViewDirection ComputeFrontDirection(NXOpen::Part* part, const RequestValues& request)
{
    AutoViewDirection result;
    const std::string mode = NormalizeFrontDirectionMode(request.frontDirectionMode);
    if (mode == "flatPatternX")
    {
        TryComputeAutoFrontDirectionFromSheetMetalFlatPattern(part, result);
        return result;
    }

    if (mode == "manualFaceX")
    {
        TryComputeFrontDirectionFromSelectedFaceAndX(part, result);
        return result;
    }

    if (!TryComputeAutoFrontDirectionFromOverallBoundingBox(part, result))
    {
        TryComputeAutoFrontDirectionFromLargestPlanarFace(part, result);
    }
    return result;
}

int FindYellowColorIndex(NXOpen::Part* part)
{
    const int fallbackYellow = 7;
    if (part == nullptr || part->Colors() == nullptr)
    {
        return fallbackYellow;
    }

    for (const char* name : {"Yellow", "yellow", "YELLOW"})
    {
        try
        {
            NXOpen::NXColor* color = part->Colors()->Find(name);
            if (color != nullptr && color->Id() > 0)
            {
                return color->Id();
            }
        }
        catch (...)
        {
        }
    }

    int bestColor = fallbackYellow;
    double bestScore = -std::numeric_limits<double>::max();
    for (int colorId = 1; colorId <= 216; ++colorId)
    {
        try
        {
            NXOpen::NXColor* color = part->Colors()->Find(colorId);
            if (color == nullptr)
            {
                continue;
            }

            NXOpen::NXColor::Rgb rgb = color->GetRgb();
            const double maxComponent = std::max(rgb.R, std::max(rgb.G, rgb.B));
            if (maxComponent > 1.0)
            {
                rgb.R /= 255.0;
                rgb.G /= 255.0;
                rgb.B /= 255.0;
            }

            if (rgb.R < 0.55 || rgb.G < 0.55 || rgb.B > 0.45)
            {
                continue;
            }

            const double score = rgb.R + rgb.G - 1.5 * rgb.B;
            if (score > bestScore)
            {
                bestScore = score;
                bestColor = colorId;
            }
        }
        catch (...)
        {
        }
    }

    return bestColor;
}

void HighlightFrontLineProjectionFaces(
    NXOpen::Session* session,
    NXOpen::Part* part,
    const AutoViewDirection& frontDirection)
{
    if (session == nullptr ||
        part == nullptr ||
        part->Bodies() == nullptr ||
        !frontDirection.valid ||
        VectorLength(frontDirection.normal) < 1.0e-6)
    {
        return;
    }

    const double dotTolerance = 0.01;
    const int yellowColor = FindYellowColorIndex(part);
    int planarFaceCount = 0;
    std::vector<NXOpen::DisplayableObject*> facesToColor;
    std::vector<tag_t> faceTags;

    for (NXOpen::BodyCollection::iterator it = part->Bodies()->begin(); it != part->Bodies()->end(); ++it)
    {
        NXOpen::Body* body = *it;
        if (body == nullptr || body->IsBlanked())
        {
            continue;
        }

        uf_list_p_t faceList = nullptr;
        if (UF_MODL_ask_body_faces(body->Tag(), &faceList) != 0 || faceList == nullptr)
        {
            continue;
        }

        int faceCount = 0;
        UF_MODL_ask_list_count(faceList, &faceCount);
        for (int faceIndex = 0; faceIndex < faceCount; ++faceIndex)
        {
            tag_t faceTag = NULL_TAG;
            if (UF_MODL_ask_list_item(faceList, faceIndex, &faceTag) != 0 || faceTag == NULL_TAG)
            {
                continue;
            }

            NXOpen::Vector3d faceNormal(0.0, 0.0, 0.0);
            if (!TryReadPlanarFaceNormal(faceTag, faceNormal))
            {
                continue;
            }

            ++planarFaceCount;
            const double alignment = std::abs(DotVector(faceNormal, frontDirection.normal));
            if (alignment > dotTolerance)
            {
                continue;
            }

            NXOpen::Face* face = dynamic_cast<NXOpen::Face*>(NXOpen::NXObjectManager::Get(faceTag));
            if (face == nullptr)
            {
                continue;
            }

            facesToColor.push_back(face);
            faceTags.push_back(faceTag);
        }

        UF_MODL_delete_list(&faceList);
    }

    if (!facesToColor.empty())
    {
        try
        {
            NXOpen::DisplayModification* modification = session->DisplayManager()->NewDisplayModification();
            modification->SetApplyToAllFaces(false);
            modification->SetApplyToOwningParts(false);
            modification->SetNewColor(yellowColor);
            modification->Apply(facesToColor);
            delete modification;
            session->DisplayManager()->MakeUpToDate();
        }
        catch (const NXOpen::NXException& ex)
        {
            WriteLine(session, std::string("AutoCreateThreeViews: highlight front line-projection faces failed, NXException: ") + ex.Message() + ".");
        }
        catch (...)
        {
            WriteLine(session, "AutoCreateThreeViews: highlight front line-projection faces failed, unknown exception.");
        }
    }

    std::ostringstream log;
    log << "AutoCreateThreeViews: front line-projection planar faces highlighted"
        << " frontNormal=(" << frontDirection.normal.X << "," << frontDirection.normal.Y << "," << frontDirection.normal.Z << ")"
        << ", tolerance=" << dotTolerance
        << ", planarFaces=" << planarFaceCount
        << ", matched=" << faceTags.size()
        << ", yellowColor=" << yellowColor;
    const int maxLoggedTags = 20;
    if (!faceTags.empty())
    {
        log << ", faceTags=";
        for (int index = 0; index < static_cast<int>(faceTags.size()) && index < maxLoggedTags; ++index)
        {
            if (index > 0)
            {
                log << ",";
            }
            log << static_cast<unsigned long long>(faceTags[index]);
        }
        if (static_cast<int>(faceTags.size()) > maxLoggedTags)
        {
            log << "...";
        }
    }
    log << ".";
    WriteLine(session, log.str());
}

double EstimateOneToOneLayoutWidth(const ModelBounds& bounds, const RequestValues& request)
{
    const double viewWidth = std::max(20.0, bounds.width);
    const double sideViewWidth = std::max(12.0, std::min(bounds.width, bounds.height) * 0.45);
    double width = request.front ? viewWidth : 0.0;
    if (request.left)
    {
        width += sideViewWidth + request.viewSpacing;
    }
    if (request.right)
    {
        width += sideViewWidth + request.viewSpacing;
    }
    if (request.back)
    {
        width += sideViewWidth + request.viewSpacing;
    }
    if (request.iso)
    {
        width += std::max(80.0, viewWidth * 0.75) + request.viewSpacing;
    }
    if (request.flat)
    {
        width += std::max(80.0, viewWidth) + request.viewSpacing;
    }

    return std::max(viewWidth, width);
}

double EstimateOneToOneLayoutHeight(const ModelBounds& bounds, const RequestValues& request)
{
    const double viewHeight = std::max(15.0, bounds.height);
    const double topBottomHeight = std::max(15.0, bounds.width * 0.45);
    double height = request.front ? viewHeight : 0.0;
    if (request.top)
    {
        height += topBottomHeight + request.viewSpacing;
    }
    if (request.bottom)
    {
        height += topBottomHeight + request.viewSpacing;
    }

    const int auxiliaryCount = (request.iso ? 1 : 0) + (request.flat ? 1 : 0);
    if (auxiliaryCount > 1)
    {
        height = std::max(height, auxiliaryCount * std::max(60.0, viewHeight * 0.6) + (auxiliaryCount - 1) * request.viewSpacing);
    }

    return std::max(viewHeight, height);
}

double ScaleUsableWidth(const RequestValues& request, double sheetLength);
double ScaleUsableHeight(const RequestValues& request, double sheetHeight);
double EffectiveLayoutMargin(const RequestValues& request);

double ChooseSheetScaleDenominator(
    const ModelBounds& bounds,
    const RequestValues& request,
    double sheetLength,
    double sheetHeight)
{
    const double usableWidth = ScaleUsableWidth(request, sheetLength);
    const double usableHeight = ScaleUsableHeight(request, sheetHeight);
    const double layoutMargin = EffectiveLayoutMargin(request);
    const double requiredWidth = EstimateOneToOneLayoutWidth(bounds, request) + layoutMargin * 2.0;
    const double requiredHeight = EstimateOneToOneLayoutHeight(bounds, request) + layoutMargin * 2.0;
    const double needed = std::max(requiredWidth / usableWidth, requiredHeight / usableHeight);
    const double standards[] = {1.0, 2.0, 5.0, 10.0, 20.0, 25.0, 50.0, 100.0, 200.0};
    for (double value : standards)
    {
        if (value >= needed)
        {
            return value;
        }
    }

    return std::ceil(needed / 100.0) * 100.0;
}

double NextPreferredScaleDenominator(double needed)
{
    const double standards[] = {
        1.0, 2.0, 5.0,
        10.0, 20.0, 25.0, 50.0,
        100.0, 200.0, 250.0, 500.0,
        1000.0
    };

    for (double value : standards)
    {
        if (value >= needed)
        {
            return value;
        }
    }

    return std::ceil(needed / 500.0) * 500.0;
}

void ApplyDrawingSheetScale(
    NXOpen::Session* session,
    NXOpen::Part* part,
    NXOpen::Drawings::DraftingDrawingSheet* sheet,
    double sheetScaleDenominator)
{
    if (part == nullptr || part->DraftingDrawingSheets() == nullptr || sheet == nullptr)
    {
        return;
    }

    NXOpen::Drawings::DraftingDrawingSheetBuilder* builder = nullptr;
    try
    {
        builder = part->DraftingDrawingSheets()->CreateDraftingDrawingSheetBuilder(sheet);
        builder->SetStandardMetricScale(NXOpen::Drawings::DrawingSheetBuilder::SheetStandardMetricScaleCustom);
        builder->SetScaleNumerator(1.0);
        builder->SetScaleDenominator(sheetScaleDenominator);
        builder->Commit();
        builder->Destroy();
    }
    catch (const NXOpen::NXException& ex)
    {
        if (builder != nullptr)
        {
            builder->Destroy();
        }
        std::ostringstream stream;
        stream << "AutoCreateThreeViews: set drawing sheet scale failed, NX "
               << ex.ErrorCode() << ", " << ex.Message();
        WriteLine(session, stream.str());
    }
    catch (...)
    {
        if (builder != nullptr)
        {
            builder->Destroy();
        }
        WriteLine(session, "AutoCreateThreeViews: set drawing sheet scale failed, unknown exception.");
    }
}

std::filesystem::path AutoTemplatePath(NXOpen::Part* part, const RequestValues& request)
{
    (void)part;
    if (!request.templatePath.empty())
    {
        return PathFromUtf8(request.templatePath);
    }

    const std::filesystem::path dataDir = CurrentModuleDirectory().parent_path() / "DATA";
    const std::filesystem::path partTemplate =
        dataDir /
        (request.firstAngle ? "A4-noviews-template1.prt" : "A4-noviews-template.prt");
    if (!request.assemblyDrawing)
    {
        return partTemplate;
    }

    const std::filesystem::path assemblyTemplate =
        dataDir /
        (request.firstAngle ? "A4-noviews-template1-ASM.prt" : "A4-noviews-template-ASM.prt");
    return std::filesystem::exists(assemblyTemplate) ? assemblyTemplate : partTemplate;
}

struct DraftingPreferencesSnapshot
{
    int integerParameters[100] = {};
    double realParameters[70] = {};
    char radiusValue[27] = {};
    char diameterValue[27] = {};
    UF_DRF_dimension_preferences_t dimension = {};
    UF_DRF_line_arrow_preferences_t lineArrow = {};
    UF_DRF_lettering_preferences_t lettering = {};
    UF_DRF_symbol_preferences_t symbol = {};
    UF_DRF_units_format_preferences_t unitsFormat = {};
    UF_DRF_diameter_radius_preferences_t diameterRadius = {};
    UF_DRF_hatch_fill_preferences_t hatchFill = {};
    NXOpen::Annotations::ArrowheadType nxFirstArrowType =
        NXOpen::Annotations::ArrowheadTypeFilledArrow;
    NXOpen::Annotations::ArrowheadType nxSecondArrowType =
        NXOpen::Annotations::ArrowheadTypeFilledArrow;
    bool nxArrowTypesAvailable = false;
};

bool AskNxOpenArrowPreferences(tag_t templatePartTag, DraftingPreferencesSnapshot& snapshot, std::string& failure)
{
    NXOpen::Drafting::PreferencesBuilder* builder = nullptr;
    try
    {
        NXOpen::Part* templatePart = dynamic_cast<NXOpen::Part*>(
            NXOpen::NXObjectManager::Get(templatePartTag));
        if (templatePart == nullptr || templatePart->SettingsManager() == nullptr)
        {
            failure = "NXOpen template drafting settings manager is unavailable";
            return false;
        }
        builder = templatePart->SettingsManager()->CreatePreferencesBuilder();
        builder->InheritSettingsFromPreferences();
        NXOpen::Annotations::LineArrowStyleBuilder* lineArrow =
            builder->AnnotationStyle()->LineArrowStyle();
        snapshot.nxFirstArrowType = lineArrow->FirstArrowType();
        snapshot.nxSecondArrowType = lineArrow->SecondArrowType();
        snapshot.nxArrowTypesAvailable = true;
        builder->Destroy();
        return true;
    }
    catch (const NXOpen::NXException& ex)
    {
        if (builder != nullptr)
        {
            builder->Destroy();
        }
        failure = std::string("NXOpen read arrow preferences failed: ") + ex.Message();
        return false;
    }
    catch (...)
    {
        if (builder != nullptr)
        {
            builder->Destroy();
        }
        failure = "NXOpen read arrow preferences failed";
        return false;
    }
}

bool ApplyNxOpenArrowPreferences(
    NXOpen::Part* targetPart,
    const DraftingPreferencesSnapshot& snapshot,
    std::string& failure)
{
    if (!snapshot.nxArrowTypesAvailable)
    {
        failure = "NXOpen template arrow preferences were not captured";
        return false;
    }

    NXOpen::Drafting::PreferencesBuilder* builder = nullptr;
    try
    {
        if (targetPart == nullptr || targetPart->SettingsManager() == nullptr)
        {
            failure = "NXOpen target drafting settings manager is unavailable";
            return false;
        }
        builder = targetPart->SettingsManager()->CreatePreferencesBuilder();
        builder->InheritSettingsFromPreferences();
        NXOpen::Annotations::LineArrowStyleBuilder* lineArrow =
            builder->AnnotationStyle()->LineArrowStyle();
        lineArrow->SetFirstArrowType(snapshot.nxFirstArrowType);
        lineArrow->SetSecondArrowType(snapshot.nxSecondArrowType);
        builder->Commit();
        builder->Destroy();
        return true;
    }
    catch (const NXOpen::NXException& ex)
    {
        if (builder != nullptr)
        {
            builder->Destroy();
        }
        failure = std::string("NXOpen write arrow preferences failed: ") + ex.Message();
        return false;
    }
    catch (...)
    {
        if (builder != nullptr)
        {
            builder->Destroy();
        }
        failure = "NXOpen write arrow preferences failed";
        return false;
    }
}

std::string UfFailureText(int status)
{
    char message[256] = {};
    if (status != 0 && UF_get_fail_message(status, message) == 0 && message[0] != '\0')
    {
        return message;
    }
    return "UF status " + std::to_string(status);
}

int AskDraftingPreferences(DraftingPreferencesSnapshot& snapshot, std::string& failedItem)
{
    struct PreferenceQuery
    {
        const char* item;
        std::function<int()> query;
    };

    const std::vector<PreferenceQuery> queries = {
        {"global drafting parameters", [&]() {
             return UF_DRF_ask_preferences(
                 snapshot.integerParameters,
                 snapshot.realParameters,
                 snapshot.radiusValue,
                 snapshot.diameterValue);
         }},
        {"dimension preferences", [&]() { return UF_DRF_ask_dimension_preferences(&snapshot.dimension); }},
        {"line and arrow preferences", [&]() { return UF_DRF_ask_line_arrow_preferences(&snapshot.lineArrow); }},
        {"lettering preferences", [&]() { return UF_DRF_ask_lettering_preferences(&snapshot.lettering); }},
        {"symbol preferences", [&]() { return UF_DRF_ask_symbol_preferences(&snapshot.symbol); }},
        {"units and format preferences", [&]() { return UF_DRF_ask_units_format_preferences(&snapshot.unitsFormat); }},
        {"diameter and radius preferences", [&]() { return UF_DRF_ask_diameter_radius_preferences(&snapshot.diameterRadius); }},
        {"hatch and fill preferences", [&]() { return UF_DRF_ask_hatch_fill_preferences(&snapshot.hatchFill); }} };

    for (const PreferenceQuery& entry : queries)
    {
        const int status = entry.query();
        if (status != 0)
        {
            failedItem = entry.item;
            return status;
        }
    }
    return 0;
}

int SetDraftingPreferences(
    DraftingPreferencesSnapshot& snapshot,
    std::string& failedItem,
    const DraftingPreferencesSnapshot* compatibilityFallback = nullptr)
{
    struct PreferenceSetter
    {
        const char* item;
        std::function<int()> setter;
    };

    const std::vector<PreferenceSetter> setters = {
        {"global drafting parameters", [&]() {
             return UF_DRF_set_preferences(
                 snapshot.integerParameters,
                 snapshot.realParameters,
                 snapshot.radiusValue,
                 snapshot.diameterValue);
         }},
        {"dimension preferences", [&]() { return UF_DRF_set_dimension_preferences(&snapshot.dimension); }},
        {"line and arrow preferences", [&]() {
             int status = UF_DRF_set_line_arrow_preferences(&snapshot.lineArrow);
             if (status == UF_DRF_INVALID_ARROW_TYPE && compatibilityFallback != nullptr)
             {
                 // NX can read newer/special arrow types from a part even
                 // though the legacy UF setter rejects those enum values.
                 // Preserve only the target part's compatible arrow types and
                 // still inherit every other line/arrow preference.
                 // The target part can contain the same newer value, so do
                 // not reuse it here.  Use the universally supported filled
                 // arrow for this legacy setter.  The aggregate drafting
                 // preferences were already copied before this specialized
                 // setter, so this only provides a compatible UF value.
                 snapshot.lineArrow.first_arrow_type = UF_DRF_FILLED_ARROW;
                 snapshot.lineArrow.second_arrow_type = UF_DRF_FILLED_ARROW;
                 status = UF_DRF_set_line_arrow_preferences(&snapshot.lineArrow);
                 if (status == UF_DRF_INVALID_ARROW_TYPE)
                 {
                     // Do not reject the whole preference set because a
                     // legacy specialized setter cannot represent a modern
                     // arrow.  The global preference copy remains valid.
                     status = 0;
                 }
             }
             return status;
         }},
        {"lettering preferences", [&]() { return UF_DRF_set_lettering_preferences(&snapshot.lettering); }},
        {"symbol preferences", [&]() { return UF_DRF_set_symbol_preferences(&snapshot.symbol); }},
        {"units and format preferences", [&]() { return UF_DRF_set_units_format_preferences(&snapshot.unitsFormat); }},
        {"diameter and radius preferences", [&]() { return UF_DRF_set_diameter_radius_preferences(&snapshot.diameterRadius); }},
        {"hatch and fill preferences", [&]() { return UF_DRF_set_hatch_fill_preferences(&snapshot.hatchFill); }} };

    for (const PreferenceSetter& entry : setters)
    {
        const int status = entry.setter();
        if (status != 0)
        {
            failedItem = entry.item;
            return status;
        }
    }
    return 0;
}

bool InheritDraftingPreferencesFromTemplate(
    NXOpen::Session* session,
    NXOpen::Part* targetPart,
    const std::filesystem::path& templatePath)
{
    if (targetPart == nullptr || templatePath.empty() || !std::filesystem::exists(templatePath))
    {
        WriteLine(session, "AutoCreateThreeViews: drafting preference inheritance skipped; template is unavailable.");
        return false;
    }

    const std::string templateName = LocalPathString(templatePath);
    const int originalLoadState = UF_PART_is_loaded(templateName.c_str());
    const bool templateWasLoaded = originalLoadState == 1 || originalLoadState == 2;
    tag_t templatePart = templateWasLoaded ? UF_PART_ask_part_tag(templateName.c_str()) : NULL_TAG;
    UF_PART_load_status_t loadStatus = {};
    int openStatus = 0;
    if (templatePart == NULL_TAG)
    {
        openStatus = UF_PART_open_quiet(templateName.c_str(), &templatePart, &loadStatus);
        UF_PART_free_load_status(&loadStatus);
    }

    if (templatePart == NULL_TAG)
    {
        WriteLine(
            session,
            "AutoCreateThreeViews: failed to open template for drafting preferences, status=" +
                std::to_string(openStatus) + ", " + UfFailureText(openStatus) + ".");
        MessageBoxW(
            nullptr,
            (std::wstring(L"无法读取模板的制图首选项。\n\n模板路径：") + templatePath.wstring()).c_str(),
            L"自动创建三视图 - 首选项继承失败",
            MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
        return false;
    }

    DraftingPreferencesSnapshot templatePreferences;
    DraftingPreferencesSnapshot targetPreferences;
    std::string failedItem;
    int status = 0;
    UF_ASSEM_work_part_context_p_t previousContext = nullptr;
    status = UF_ASSEM_set_work_part_context_quietly(templatePart, &previousContext);
    if (status == 0)
    {
        status = AskDraftingPreferences(templatePreferences, failedItem);
    }
    if (status == 0 && !AskNxOpenArrowPreferences(templatePart, templatePreferences, failedItem))
    {
        status = 1;
    }
    const int restoreStatus = previousContext != nullptr
                                  ? UF_ASSEM_restore_work_part_context_quietly(&previousContext)
                                  : 0;
    if (status == 0 && restoreStatus != 0)
    {
        status = restoreStatus;
        failedItem = "restore target work part context";
    }

    if (status == 0)
    {
        status = AskDraftingPreferences(targetPreferences, failedItem);
    }
    if (status == 0)
    {
        const UF_DRF_arrowhead_and_fill_type_t templateFirstArrow =
            templatePreferences.lineArrow.first_arrow_type;
        const UF_DRF_arrowhead_and_fill_type_t templateSecondArrow =
            templatePreferences.lineArrow.second_arrow_type;
        status = SetDraftingPreferences(templatePreferences, failedItem, &targetPreferences);
        if (status == 0 &&
            (templatePreferences.lineArrow.first_arrow_type != templateFirstArrow ||
             templatePreferences.lineArrow.second_arrow_type != templateSecondArrow))
        {
            WriteLine(
                session,
                "AutoCreateThreeViews: template uses an arrow type rejected by the legacy UF setter; "
                "kept the target part arrow type and inherited the remaining line/arrow preferences.");
        }
        if (status == 0 &&
            !ApplyNxOpenArrowPreferences(targetPart, templatePreferences, failedItem))
        {
            status = 1;
        }
        if (status != 0)
        {
            std::string rollbackItem;
            const int rollbackStatus = SetDraftingPreferences(targetPreferences, rollbackItem);
            WriteLine(
                session,
                "AutoCreateThreeViews: drafting preference inheritance rolled back, rollbackStatus=" +
                    std::to_string(rollbackStatus) + ".");
        }
    }

    if (!templateWasLoaded)
    {
        const int closeStatus = UF_PART_close(templatePart, 0, 2);
        if (closeStatus != 0)
        {
            WriteLine(
                session,
                "AutoCreateThreeViews: temporary preference template close warning, status=" +
                    std::to_string(closeStatus) + ".");
        }
    }

    if (status != 0)
    {
        WriteLine(
            session,
            "AutoCreateThreeViews: drafting preference inheritance failed at " + failedItem +
                ", status=" + std::to_string(status) + ", " + UfFailureText(status) + ".");
        std::wostringstream message;
        message << L"无法从模板继承制图首选项。\n\n"
                << L"模板路径：" << templatePath.wstring() << L"\n"
                << L"失败项目：" << failedItem.c_str() << L"\n"
                << L"NX 状态码：" << status;
        MessageBoxW(
            nullptr,
            message.str().c_str(),
            L"自动创建三视图 - 首选项继承失败",
            MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
        return false;
    }

    WriteLine(
        session,
        "AutoCreateThreeViews: inherited drafting preferences only from template " + templateName +
            "; no drawing sheet objects were copied by this step.");
    return true;
}

std::wstring TextToWide(const std::string& text)
{
    if (text.empty())
    {
        return std::wstring();
    }

    UINT codePage = CP_UTF8;
    DWORD flags = MB_ERR_INVALID_CHARS;
    int length = MultiByteToWideChar(
        codePage,
        flags,
        text.data(),
        static_cast<int>(text.size()),
        nullptr,
        0);
    if (length <= 0)
    {
        codePage = CP_ACP;
        flags = 0;
        length = MultiByteToWideChar(
            codePage,
            flags,
            text.data(),
            static_cast<int>(text.size()),
            nullptr,
            0);
    }
    if (length <= 0)
    {
        return std::wstring(text.begin(), text.end());
    }

    std::wstring result(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(
        codePage,
        flags,
        text.data(),
        static_cast<int>(text.size()),
        result.data(),
        length);
    return result;
}

std::wstring AttributeTypeToChinese(const std::string& type)
{
    const std::string lower = ToLowerAscii(Trim(type));
    if (lower == "string")
    {
        return L"\u5b57\u7b26\u4e32";
    }
    if (lower == "integer")
    {
        return L"\u6574\u6570";
    }
    if (lower == "real" || lower == "double")
    {
        return L"\u5b9e\u6570";
    }
    if (lower == "boolean")
    {
        return L"\u5e03\u5c14";
    }
    if (lower == "date" || lower == "time")
    {
        return L"\u65e5\u671f/\u65f6\u95f4";
    }
    return TextToWide(Trim(type));
}

struct AttributeConflictDetails
{
    bool parsed = false;
    std::string title;
    std::string templateValue;
    std::string templateType;
    std::string existingValue;
    std::string existingType;
};

AttributeConflictDetails ParseAttributeConflict(const std::string& nxMessage)
{
    AttributeConflictDetails details;
    const std::regex pattern(
        R"(duplicate attribute of title '([^']*)' with value '([^']*)' and type ([^.]+)\.\s*Keeping existing value '([^']*)'\s+([^.]+))",
        std::regex::icase);
    std::smatch match;
    if (!std::regex_search(nxMessage, match, pattern) || match.size() != 6)
    {
        return details;
    }

    details.parsed = true;
    details.title = match[1].str();
    details.templateValue = match[2].str();
    details.templateType = Trim(match[3].str());
    details.existingValue = match[4].str();
    details.existingType = Trim(match[5].str());
    return details;
}

NXOpen::Drawings::DraftingDrawingSheet* CommitDrawingSheetTemplate(
    NXOpen::Part* part,
    const std::filesystem::path& templatePath,
    NXOpen::Drawings::DrawingSheetBuilder::SheetProjectionAngle projection,
    double sheetScaleDenominator)
{
    NXOpen::Drawings::DraftingDrawingSheetBuilder* builder = nullptr;
    try
    {
        builder = part->DraftingDrawingSheets()->CreateDraftingDrawingSheetBuilder(nullptr);
        builder->SetProjectionAngle(projection);
        const std::string templatePathText = LocalPathString(templatePath);
        builder->SetMetricSheetTemplateLocation(templatePathText.c_str());
        builder->SetStandardMetricScale(NXOpen::Drawings::DrawingSheetBuilder::SheetStandardMetricScaleCustom);
        builder->SetScaleNumerator(1.0);
        builder->SetScaleDenominator(sheetScaleDenominator);
        NXOpen::NXObject* sheetObject = builder->Commit();
        try
        {
            builder->Destroy();
        }
        catch (...)
        {
            // A successful commit already owns the sheet. Builder cleanup must not
            // replace that result with NX error 65 if NX invalidated the JAM handle.
        }
        builder = nullptr;
        return dynamic_cast<NXOpen::Drawings::DraftingDrawingSheet*>(sheetObject);
    }
    catch (...)
    {
        // NX invalidates this JAM builder when template instantiation fails.
        // Destroying that invalid handle raises error 65 ("NULL tag for this")
        // and can corrupt the recovery path, so leave cleanup to NX.
        throw;
    }
}

NXOpen::Drawings::DraftingDrawingSheet* CreateDrawingSheet(NXOpen::Session* session, NXOpen::Part* part, const RequestValues& request, double sheetScaleDenominator)
{
    if (part == nullptr || part->DraftingDrawingSheets() == nullptr)
    {
        return nullptr;
    }

    const std::filesystem::path templatePath = AutoTemplatePath(part, request);
    std::filesystem::path activeTemplatePath = templatePath;
    WriteLine(
        session,
        "AutoCreateThreeViews: resolved template path: " + LocalPathString(templatePath) +
            (request.templatePath.empty() ? " (automatic)" : " (selected)"));
    NXOpen::Drawings::DrawingSheetBuilder::SheetProjectionAngle builderProjection =
        request.firstAngle
            ? NXOpen::Drawings::DrawingSheetBuilder::SheetProjectionAngleFirst
            : NXOpen::Drawings::DrawingSheetBuilder::SheetProjectionAngleThird;

    NXOpen::Drawings::DraftingDrawingSheet* sheet = nullptr;
    if (std::filesystem::exists(activeTemplatePath))
    {
        try
        {
            sheet = CommitDrawingSheetTemplate(
                part,
                activeTemplatePath,
                builderProjection,
                sheetScaleDenominator);
        }
        catch (const NXOpen::NXException& ex)
        {
            if (ex.ErrorCode() != 512035)
            {
                throw;
            }

            const std::string nxMessage = ex.Message();
            const AttributeConflictDetails conflict = ParseAttributeConflict(nxMessage);
            WriteLine(
                session,
                "AutoCreateThreeViews: stopped on actual template attribute conflict (NX 512035): " +
                    nxMessage);
            std::wostringstream message;
            message << L"\u65e0\u6cd5\u521b\u5efa\u56fe\u7eb8\u9875\uff0cNX "
                    << L"\u68c0\u6d4b\u5230\u5c5e\u6027\u51b2\u7a81\u3002\n\n";
            if (conflict.parsed)
            {
                message
                    << L"\u51b2\u7a81\u5c5e\u6027\uff1a"
                    << TextToWide(conflict.title) << L"\n"
                    << L"\u5f53\u524d\u90e8\u4ef6\uff1a"
                    << AttributeTypeToChinese(conflict.existingType)
                    << L"\u7c7b\u578b\uff0c\u503c\u4e3a\u201c"
                    << TextToWide(conflict.existingValue) << L"\u201d\n"
                    << L"\u6a21\u677f\u5c1d\u8bd5\u5199\u5165\uff1a"
                    << AttributeTypeToChinese(conflict.templateType)
                    << L"\u7c7b\u578b\uff0c\u503c\u4e3a\u201c"
                    << TextToWide(conflict.templateValue) << L"\u201d\n\n"
                    << L"\u539f\u56e0\uff1a\u90e8\u4ef6\u548c\u6a21\u677f\u4e2d"
                    << L"\u5b58\u5728\u540c\u540d\u4f46\u7c7b\u578b\u4e0d\u540c\u7684\u5c5e\u6027\u3002\n"
                    << L"\u8bf7\u5c06\u4e24\u8005\u7684\u5c5e\u6027\u7c7b\u578b\u7edf\u4e00\u540e"
                    << L"\u91cd\u65b0\u51fa\u56fe\u3002";
            }
            else
            {
                message
                    << L"\u672a\u80fd\u4ece NX \u4fe1\u606f\u4e2d\u81ea\u52a8\u63d0\u53d6"
                    << L"\u5c5e\u6027\u540d\u79f0\u548c\u7c7b\u578b\u3002\n\n"
                    << L"NX \u539f\u59cb\u9519\u8bef\uff1a"
                    << TextToWide(nxMessage);
            }
            message
                << L"\n\nNX \u9519\u8bef\u7801\uff1a512035"
                << L"\n\u6a21\u677f\u8def\u5f84\uff1a" << activeTemplatePath.wstring();
            MessageBoxW(
                nullptr,
                message.str().c_str(),
                L"\u81ea\u52a8\u521b\u5efa\u4e09\u89c6\u56fe - \u5c5e\u6027\u51b2\u7a81",
                MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
            return nullptr;
        }
        if (part->Drafting() != nullptr)
        {
            part->Drafting()->SetTemplateInstantiationIsComplete(true);
        }
        WriteLine(session, "AutoCreateThreeViews: created sheet from template " + LocalPathString(activeTemplatePath));
    }
    else
    {
        WriteLine(session, "AutoCreateThreeViews: template not found; drawing stopped: " + LocalPathString(templatePath));
        std::wostringstream message;
        message << L"找不到出图模板文件。\n\n"
                << L"投影方式：" << (request.firstAngle ? L"第一角法" : L"第三角法") << L"\n"
                << L"缺少模板：" << templatePath.filename().wstring() << L"\n"
                << L"完整路径：" << templatePath.wstring() << L"\n\n"
                << L"请将模板文件放到以上路径后重新出图。";
        MessageBoxW(
            nullptr,
            message.str().c_str(),
            L"自动创建三视图 - 缺少模板",
            MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
        return nullptr;
    }

    if (sheet != nullptr)
    {
        sheet->Open();
        UF_DRAW_open_drawing(sheet->Tag());
    }

    return sheet;
}

NXOpen::Drawings::BaseView* CreateBaseView(
    NXOpen::Session* session,
    NXOpen::Part* part,
    const std::string& label,
    const std::vector<std::string>& modelViewNames,
    const NXOpen::Point3d& placement,
    bool allowWorkViewFallback,
    double scaleDenominator,
    const AutoViewDirection* orientation = nullptr)
{
    NXOpen::ModelingView* modelView = FindModelingView(part, modelViewNames, allowWorkViewFallback);
    if (label == "front" && orientation != nullptr && orientation->valid)
    {
        modelView = FindModelingView(part, {"Top"}, false);
        std::ostringstream log;
        log << "AutoCreateThreeViews: front uses Top model view + OVT direction only, normal="
            << orientation->normalName
            << ", x="
            << orientation->xName
            << ".";
        WriteLine(session, log.str());
    }
    if (modelView == nullptr)
    {
        WriteLine(session, "AutoCreateThreeViews: skip " + label + ", modeling view not found.");
        return nullptr;
    }

    NXOpen::Drawings::BaseViewBuilder* builder = nullptr;
    try
    {
        builder = part->DraftingViews()->CreateBaseViewBuilder(nullptr);
        builder->SelectModelView()->SetSelectedView(modelView);
        builder->Scale()->SetScaleType(NXOpen::Drawings::ViewScaleBuilder::TypeRatio);
        builder->Scale()->SetNumerator(1.0);
        builder->Scale()->SetDenominator(scaleDenominator);
        if (label == "front")
        {
            builder->Style()->ViewStyleSmoothEdges()->SetSmoothEdge(false);
            builder->Style()->ViewStyleVirtualIntersections()->SetVirtualIntersections(true);
            builder->Style()->ViewStyleVirtualIntersections()->SetAdjacentBlends(true);
        }
        if (orientation != nullptr && orientation->valid)
        {
            const NXOpen::Point3d origin(0.0, 0.0, 0.0);
            NXOpen::Direction* normalDirection =
                part->Directions()->CreateDirection(origin, orientation->normal, NXOpen::SmartObject::UpdateOptionAfterModeling);
            NXOpen::Direction* xDirection =
                part->Directions()->CreateDirection(origin, orientation->xDirection, NXOpen::SmartObject::UpdateOptionAfterModeling);
            builder->Style()->ViewStyleOrientation()->Ovt()->SetAssociativeOrientation(true);
            builder->Style()->ViewStyleOrientation()->Ovt()->SetNormalDirection(normalDirection);
            builder->Style()->ViewStyleOrientation()->Ovt()->SetXDirection(xDirection);
        }
        builder->Placement()->Placement()->SetValue(nullptr, part->Views()->WorkView(), placement);

        NXOpen::NXObject* object = builder->Commit();
        NXOpen::Drawings::BaseView* view = dynamic_cast<NXOpen::Drawings::BaseView*>(object);
        builder->Destroy();
        WriteLine(session, "AutoCreateThreeViews: created " + label + ".");
        return view;
    }
    catch (const NXOpen::NXException& ex)
    {
        if (builder != nullptr)
        {
            builder->Destroy();
        }
        std::ostringstream stream;
        stream << "AutoCreateThreeViews: create " << label << " failed, NX "
               << ex.ErrorCode() << ", " << ex.Message();
        WriteLine(session, stream.str());
        return nullptr;
    }
}

AutoViewDirection ReversedViewDirection(const AutoViewDirection& direction)
{
    AutoViewDirection reversed = direction;
    reversed.normal.X = -reversed.normal.X;
    reversed.normal.Y = -reversed.normal.Y;
    reversed.normal.Z = -reversed.normal.Z;
    reversed.xDirection.X = -reversed.xDirection.X;
    reversed.xDirection.Y = -reversed.xDirection.Y;
    reversed.xDirection.Z = -reversed.xDirection.Z;
    reversed.normalName = DominantAxisName(reversed.normal);
    reversed.xName = DominantAxisName(reversed.xDirection);
    reversed.source += " + back-side curve comparison";
    return reversed;
}

int CountVisibleDraftingCurveExtents(NXOpen::Drawings::DraftingView* view)
{
    return static_cast<int>(CollectDraftingCurveExtents(view).size());
}

void DeleteTemporaryDraftingViews(NXOpen::Part* part, const std::vector<NXOpen::Drawings::DraftingView*>& views)
{
    if (part == nullptr || part->DraftingViews() == nullptr || views.empty())
    {
        return;
    }

    try
    {
        part->DraftingViews()->DeleteViewsInOriginalPart(views);
    }
    catch (...)
    {
        for (NXOpen::Drawings::DraftingView* view : views)
        {
            if (view != nullptr)
            {
                UF_OBJ_delete_object(view->Tag());
            }
        }
    }
}

void PreferBackSideIfMoreCurves(
    NXOpen::Session* session,
    NXOpen::Part* part,
    AutoViewDirection& frontDirection,
    double scaleDenominator)
{
    if (part == nullptr || !frontDirection.valid)
    {
        return;
    }

    // Exact imported views must remain inside the drawing sheet when a
    // view-dependent layer mask forces hidden-line regeneration.
    const NXOpen::Point3d frontProbePoint(75.0, 125.0, 0.0);
    const NXOpen::Point3d backProbePoint(220.0, 125.0, 0.0);
    AutoViewDirection backDirection = ReversedViewDirection(frontDirection);
    std::vector<NXOpen::Drawings::DraftingView*> temporaryViews;

    NXOpen::Drawings::BaseView* frontProbe = CreateBaseView(
        session,
        part,
        "front",
        {"Top"},
        frontProbePoint,
        false,
        scaleDenominator,
        &frontDirection);
    if (frontProbe != nullptr)
    {
        temporaryViews.push_back(frontProbe);
    }

    NXOpen::Drawings::BaseView* backProbe = CreateBaseView(
        session,
        part,
        "front",
        {"Top"},
        backProbePoint,
        false,
        scaleDenominator,
        &backDirection);
    if (backProbe != nullptr)
    {
        temporaryViews.push_back(backProbe);
    }

    int frontCurveCount = -1;
    int backCurveCount = -1;
    try
    {
        if (!temporaryViews.empty())
        {
            part->DraftingViews()->UpdateViews(temporaryViews);
        }
        frontCurveCount = CountVisibleDraftingCurveExtents(frontProbe);
        backCurveCount = CountVisibleDraftingCurveExtents(backProbe);
    }
    catch (...)
    {
    }

    std::ostringstream line;
    line << "AutoCreateThreeViews: front/back curve comparison frontCurves="
         << frontCurveCount
         << ", backCurves="
         << backCurveCount;

    if (backCurveCount > frontCurveCount)
    {
        frontDirection = backDirection;
        line << ", selected=back.";
    }
    else
    {
        line << ", selected=front.";
    }
    WriteLine(session, line.str());

    DeleteTemporaryDraftingViews(part, temporaryViews);
}

void PreferOverallBoxDirectionWithMostCurves(
    NXOpen::Session* session,
    NXOpen::Part* part,
    AutoViewDirection& frontDirection,
    double scaleDenominator)
{
    const ModelBounds bounds = AskModelBounds(part);
    if (part == nullptr || !bounds.valid)
    {
        WriteLine(session, "AutoCreateThreeViews: drafting view boundary direction skipped; model bounds invalid.");
        return;
    }

    struct BoxDirectionProbe
    {
        AutoViewDirection direction;
        double area = 0.0;
        double horizontalLength = 0.0;
        double viewArea = 0.0;
        double viewWidth = 0.0;
        double viewHeight = 0.0;
        int curveCount = -1;
    };

    std::vector<BoxDirectionProbe> probes;
    auto pushProbe = [&](const AutoViewDirection& direction, double area, double horizontalLength) {
        BoxDirectionProbe probe;
        probe.direction = direction;
        probe.area = area;
        probe.horizontalLength = horizontalLength;
        probes.push_back(probe);
    };

    AutoViewDirection base = frontDirection;
    double area = base.faceArea;
    double horizontalLength = base.edgeLength;
    if (!base.valid)
    {
        const double sizes[3] = {bounds.sizeX, bounds.sizeY, bounds.sizeZ};
        int bestNormalAxis = -1;
        int bestHorizontalAxis = -1;
        double bestArea = -1.0;
        double bestHorizontalLength = -1.0;
        for (int normalAxis = 0; normalAxis < 3; ++normalAxis)
        {
            int projectedAxes[2] = {0, 1};
            int out = 0;
            for (int axis = 0; axis < 3; ++axis)
            {
                if (axis != normalAxis)
                {
                    projectedAxes[out++] = axis;
                }
            }
            const int horizontalAxis =
                sizes[projectedAxes[0]] >= sizes[projectedAxes[1]]
                    ? projectedAxes[0]
                    : projectedAxes[1];
            const double candidateArea = sizes[projectedAxes[0]] * sizes[projectedAxes[1]];
            const double candidateHorizontalLength = sizes[horizontalAxis];
            if (candidateArea > bestArea + 1.0e-6 ||
                (std::abs(candidateArea - bestArea) <= 1.0e-6 &&
                 candidateHorizontalLength > bestHorizontalLength + 1.0e-6))
            {
                bestNormalAxis = normalAxis;
                bestHorizontalAxis = horizontalAxis;
                bestArea = candidateArea;
                bestHorizontalLength = candidateHorizontalLength;
            }
        }

        if (bestNormalAxis < 0 || bestHorizontalAxis < 0 ||
            bestArea <= 1.0e-6 || bestHorizontalLength <= 1.0e-6)
        {
            return;
        }

        base.normal = AxisVector(bestNormalAxis, 1.0);
        base.xDirection = AxisVector(bestHorizontalAxis, 1.0);
        StabilizeDirectionSign(base.normal);
        StabilizeDirectionSign(base.xDirection);
        base.normalName = DominantAxisName(base.normal);
        base.xName = DominantAxisName(base.xDirection);
        base.faceArea = bestArea;
        base.edgeLength = bestHorizontalLength;
        base.valid = true;
        area = bestArea;
        horizontalLength = bestHorizontalLength;
    }
    else
    {
        if (area <= 1.0e-6)
        {
            const int normalAxis = DominantAxisIndex(base.normal);
            int projectedAxes[2] = {0, 1};
            int out = 0;
            for (int axis = 0; axis < 3; ++axis)
            {
                if (axis != normalAxis)
                {
                    projectedAxes[out++] = axis;
                }
            }
            const double sizes[3] = {bounds.sizeX, bounds.sizeY, bounds.sizeZ};
            area = sizes[projectedAxes[0]] * sizes[projectedAxes[1]];
        }
        if (horizontalLength <= 1.0e-6)
        {
            const int horizontalAxis = DominantAxisIndex(base.xDirection);
            const double sizes[3] = {bounds.sizeX, bounds.sizeY, bounds.sizeZ};
            horizontalLength = sizes[horizontalAxis];
        }
    }

    base.faceArea = area;
    base.edgeLength = horizontalLength;
    base.source = "overall box max-area plane + longest x direction + front/back visible curve comparison";
    pushProbe(base, area, horizontalLength);

    AutoViewDirection back = ReversedViewDirection(base);
    back.source = base.source;
    pushProbe(back, area, horizontalLength);

    /*
      The overall bounding box already selects the projection plane and the
      longest in-plane X direction. Only the two opposite viewing sides can
      differ in hidden/visible curve content, so probing all three axes and
      both in-plane rotations would create ten redundant drafting views.
    */
    if (probes.size() != 2)
    {
        return;
    }

    std::vector<NXOpen::Drawings::DraftingView*> temporaryViews;
    std::vector<NXOpen::Drawings::BaseView*> probeViews(probes.size(), nullptr);
    for (size_t index = 0; index < probes.size(); ++index)
    {
        // Keep temporary exact views on-sheet.  NX 2412 rejects masked exact
        // view updates outside the sheet with error 731009 and reports the
        // secondary hidden-line/occurrence failure seen in the update dialog.
        const NXOpen::Point3d probePoint(
            index == 0 ? 75.0 : 220.0,
            125.0,
            0.0);
        NXOpen::Drawings::BaseView* probeView = CreateBaseView(
            session,
            part,
            "front probe",
            {"Top"},
            probePoint,
            false,
            scaleDenominator,
            &probes[index].direction);
        probeViews[index] = probeView;
        if (probeView != nullptr)
        {
            temporaryViews.push_back(probeView);
        }
    }

    try
    {
        if (!temporaryViews.empty() && part->DraftingViews() != nullptr)
        {
            part->DraftingViews()->UpdateViews(temporaryViews);
        }
        for (size_t index = 0; index < probes.size(); ++index)
        {
            std::vector<DraftingCurveExtent> extents = CollectDraftingCurveExtents(probeViews[index]);
            probes[index].curveCount = static_cast<int>(extents.size());
            LayoutBounds curveBounds;
            if (TryBuildVisibleCurveBounds(extents, curveBounds))
            {
                probes[index].viewWidth = BoundsWidth(curveBounds);
                probes[index].viewHeight = BoundsHeight(curveBounds);
                probes[index].viewArea = BoundsArea(curveBounds);
            }
        }
    }
    catch (...)
    {
    }

    size_t bestIndex = 0;
    for (size_t index = 1; index < probes.size(); ++index)
    {
        const BoxDirectionProbe& candidate = probes[index];
        const BoxDirectionProbe& best = probes[bestIndex];
        const bool candidateLandscape = candidate.viewWidth >= candidate.viewHeight - 1.0e-6;
        const bool bestLandscape = best.viewWidth >= best.viewHeight - 1.0e-6;
        if (candidate.viewArea > best.viewArea + 1.0e-6 ||
            (std::abs(candidate.viewArea - best.viewArea) <= 1.0e-6 &&
             candidateLandscape && !bestLandscape) ||
            (std::abs(candidate.viewArea - best.viewArea) <= 1.0e-6 &&
             candidateLandscape == bestLandscape &&
             candidate.curveCount > best.curveCount) ||
            (std::abs(candidate.viewArea - best.viewArea) <= 1.0e-6 &&
             candidateLandscape == bestLandscape &&
             candidate.curveCount == best.curveCount &&
             candidate.horizontalLength > best.horizontalLength + 1.0e-6))
        {
            bestIndex = index;
        }
    }

    std::ostringstream line;
    line << "AutoCreateThreeViews: drafting view boundary direction comparison";
    for (size_t index = 0; index < probes.size(); ++index)
    {
        line << " [normal=" << probes[index].direction.normalName
             << ", x=" << probes[index].direction.xName
             << ", boxArea=" << probes[index].area
             << ", length=" << probes[index].horizontalLength
             << ", viewArea=" << probes[index].viewArea
             << ", viewWidth=" << probes[index].viewWidth
             << ", viewHeight=" << probes[index].viewHeight
             << ", curves=" << probes[index].curveCount << "]";
    }
    line << ", selected normal=" << probes[bestIndex].direction.normalName
         << ", x=" << probes[bestIndex].direction.xName
         << ".";
    WriteLine(session, line.str());

    frontDirection = probes[bestIndex].direction;
    DeleteTemporaryDraftingViews(part, temporaryViews);
}

NXOpen::Drawings::BaseView* CreateBaseViewFromModelingView(
    NXOpen::Session* session,
    NXOpen::Part* part,
    NXOpen::ModelingView* modelView,
    const std::string& label,
    const NXOpen::Point3d& placement,
    double scaleDenominator)
{
    if (part == nullptr || modelView == nullptr)
    {
        return nullptr;
    }

    NXOpen::Drawings::BaseViewBuilder* builder = nullptr;
    try
    {
        builder = part->DraftingViews()->CreateBaseViewBuilder(nullptr);
        builder->SelectModelView()->SetSelectedView(modelView);
        builder->Scale()->SetScaleType(NXOpen::Drawings::ViewScaleBuilder::TypeRatio);
        builder->Scale()->SetNumerator(1.0);
        builder->Scale()->SetDenominator(scaleDenominator);
        builder->Placement()->Placement()->SetValue(nullptr, part->Views()->WorkView(), placement);

        NXOpen::NXObject* object = builder->Commit();
        NXOpen::Drawings::BaseView* view = dynamic_cast<NXOpen::Drawings::BaseView*>(object);
        builder->Destroy();
        WriteLine(session, "AutoCreateThreeViews: created " + label + ".");
        return view;
    }
    catch (const NXOpen::NXException& ex)
    {
        if (builder != nullptr)
        {
            builder->Destroy();
        }
        std::ostringstream stream;
        stream << "AutoCreateThreeViews: create " << label << " failed, NX "
               << ex.ErrorCode() << ", " << ex.Message();
        WriteLine(session, stream.str());
        return nullptr;
    }
}

NXOpen::Drawings::BaseView* CreateFlatPatternView(
    NXOpen::Session* session,
    NXOpen::Part* part,
    const NXOpen::Point3d& placement,
    double scaleDenominator)
{
    const std::vector<std::string> exactNames = {
        "Flat Pattern", "Flat-Pattern", "FlatPattern", "FLAT_PATTERN", "ZK", "UNFOLD",
        "展开图", "展开", "平面展开", "展平"
    };

    NXOpen::ModelingView* modelView = FindModelingView(part, exactNames, false);
    if (modelView == nullptr)
    {
        modelView = FindModelingViewByKeyword(part, {"flat", "pattern", "unfold", "blank", "展开", "展平"});
    }

    if (modelView == nullptr)
    {
        WriteLine(session, "AutoCreateThreeViews: flat pattern modeling view not found; fallback to current work view.");
        modelView = FindModelingView(part, {}, true);
    }

    return CreateBaseViewFromModelingView(session, part, modelView, "flat pattern", placement, scaleDenominator);
}

NXOpen::Drawings::ProjectedView* CreateProjectedView(
    NXOpen::Session* session,
    NXOpen::Part* part,
    NXOpen::Drawings::DraftingView* parentView,
    const std::string& label,
    const std::string& parentLabel,
    const NXOpen::Point3d& placement,
    double scaleDenominator)
{
    if (part == nullptr || part->DraftingViews() == nullptr || parentView == nullptr)
    {
        return nullptr;
    }

    NXOpen::Drawings::ProjectedViewBuilder* builder = nullptr;
    try
    {
        builder = part->DraftingViews()->CreateProjectedViewBuilder(nullptr);
        builder->Parent()->View()->SetValue(parentView);
        builder->Placement()->SetAlignmentMethod(
            NXOpen::Drawings::ViewPlacementBuilder::MethodPerpendicularToHingeLine);
        builder->Placement()->SetAlignmentOption(
            NXOpen::Drawings::ViewPlacementBuilder::OptionModelPoint);
        builder->SecondaryComponents()->SetObjectType(
            NXOpen::Drawings::DraftingComponentSelectionBuilder::GeometryPrimaryGeometry);
        builder->Style()->ViewStyleGeneral()->SetExtractedEdges(
            NXOpen::Preferences::GeneralExtractedEdgesOptionAssociative);
        builder->Style()->ViewStyleSmoothEdges()->SetSmoothEdge(false);
        builder->Style()->ViewStyleVirtualIntersections()->SetVirtualIntersections(true);
        builder->Style()->ViewStyleVirtualIntersections()->SetAdjacentBlends(true);
        builder->Style()->ViewStyleGeneral()->Scale()->SetNumerator(1.0);
        builder->Style()->ViewStyleGeneral()->Scale()->SetDenominator(scaleDenominator);
        builder->Placement()->AlignmentView()->SetValue(parentView);
        builder->Placement()->Placement()->SetValue(parentView, part->Views()->WorkView(), placement);

        NXOpen::NXObject* object = builder->Commit();
        NXOpen::Drawings::ProjectedView* view =
            dynamic_cast<NXOpen::Drawings::ProjectedView*>(object);
        builder->Destroy();
        WriteLine(session, "AutoCreateThreeViews: projected " + label + " from " + parentLabel + " view.");
        return view;
    }
    catch (const NXOpen::NXException& ex)
    {
        if (builder != nullptr)
        {
            builder->Destroy();
        }
        std::ostringstream stream;
        stream << "AutoCreateThreeViews: project " << label << " failed, NX "
               << ex.ErrorCode() << ", " << ex.Message();
        WriteLine(session, stream.str());
        return nullptr;
    }
    catch (...)
    {
        if (builder != nullptr)
        {
            builder->Destroy();
        }
        WriteLine(session, "AutoCreateThreeViews: project " + label + " failed, unknown exception.");
        return nullptr;
    }
}

NXOpen::Point3d CornerPoint(const std::string& corner)
{
    if (corner == "TopRight")
    {
        return NXOpen::Point3d(245.0, 172.0, 0.0);
    }
    if (corner == "BottomLeft")
    {
        return NXOpen::Point3d(52.0, 42.0, 0.0);
    }
    if (corner == "BottomRight")
    {
        return NXOpen::Point3d(245.0, 42.0, 0.0);
    }
    return NXOpen::Point3d(52.0, 172.0, 0.0);
}

NXOpen::Point3d SheetCenterPoint(double sheetLength, double sheetHeight)
{
    return NXOpen::Point3d(sheetLength * 0.5, sheetHeight * 0.5, 0.0);
}

void AddBounds(LayoutBounds& bounds, const NXOpen::Point3d& point, double width, double height, bool* initialized)
{
    const double minX = point.X - width * 0.5;
    const double maxX = point.X + width * 0.5;
    const double minY = point.Y - height * 0.5;
    const double maxY = point.Y + height * 0.5;

    if (!*initialized)
    {
        bounds.minX = minX;
        bounds.maxX = maxX;
        bounds.minY = minY;
        bounds.maxY = maxY;
        *initialized = true;
        return;
    }

    bounds.minX = std::min(bounds.minX, minX);
    bounds.maxX = std::max(bounds.maxX, maxX);
    bounds.minY = std::min(bounds.minY, minY);
    bounds.maxY = std::max(bounds.maxY, maxY);
}

void ShiftViews(std::vector<PlannedView>& views, double dx, double dy)
{
    for (PlannedView& view : views)
    {
        view.point.X += dx;
        view.point.Y += dy;
    }
}

void AccumulateBoundsPoint(LayoutBounds& bounds, double x, double y, bool& initialized)
{
    if (!initialized)
    {
        bounds.minX = x;
        bounds.maxX = x;
        bounds.minY = y;
        bounds.maxY = y;
        initialized = true;
        return;
    }

    bounds.minX = std::min(bounds.minX, x);
    bounds.maxX = std::max(bounds.maxX, x);
    bounds.minY = std::min(bounds.minY, y);
    bounds.maxY = std::max(bounds.maxY, y);
}

bool EvaluateCurveDrawingPoint(
    UF_EVAL_p_t evaluator,
    tag_t viewTag,
    double parameter,
    double modelPoint[3],
    double drawingPoint[2])
{
    if (evaluator == nullptr || viewTag == NULL_TAG)
    {
        return false;
    }

    double derivatives[3] = {0.0, 0.0, 0.0};
    if (UF_EVAL_evaluate(evaluator, 0, parameter, modelPoint, derivatives) != 0)
    {
        return false;
    }

    return UF_VIEW_map_model_to_drawing(viewTag, modelPoint, drawingPoint) == 0;
}

bool EvaluateCurveDrawingCoordinate(
    UF_EVAL_p_t evaluator,
    tag_t viewTag,
    double parameter,
    bool useX,
    double& value)
{
    double modelPoint[3] = {0.0, 0.0, 0.0};
    double drawingPoint[2] = {0.0, 0.0};
    if (!EvaluateCurveDrawingPoint(evaluator, viewTag, parameter, modelPoint, drawingPoint))
    {
        return false;
    }

    value = useX ? drawingPoint[0] : drawingPoint[1];
    return true;
}

void AppendUniqueParameter(std::vector<double>& parameters, double parameter, double tolerance)
{
    for (double existing : parameters)
    {
        if (std::abs(existing - parameter) <= tolerance)
        {
            return;
        }
    }
    parameters.push_back(parameter);
}

double RefineCurveCoordinateExtreme(
    UF_EVAL_p_t evaluator,
    tag_t viewTag,
    double low,
    double high,
    bool useX,
    bool findMaximum)
{
    if (high < low)
    {
        std::swap(low, high);
    }

    for (int iteration = 0; iteration < 48; ++iteration)
    {
        const double first = low + (high - low) / 3.0;
        const double second = high - (high - low) / 3.0;
        double firstValue = 0.0;
        double secondValue = 0.0;
        if (!EvaluateCurveDrawingCoordinate(evaluator, viewTag, first, useX, firstValue) ||
            !EvaluateCurveDrawingCoordinate(evaluator, viewTag, second, useX, secondValue))
        {
            break;
        }

        if (findMaximum ? (firstValue < secondValue) : (firstValue > secondValue))
        {
            low = first;
        }
        else
        {
            high = second;
        }
    }

    return (low + high) * 0.5;
}

std::vector<double> CollectDraftingCurveExtremeParameters(
    UF_EVAL_p_t evaluator,
    tag_t viewTag,
    const double limits[2])
{
    std::vector<double> parameters;
    const double parameterSpan = std::abs(limits[1] - limits[0]);
    const double parameterTolerance = std::max(parameterSpan * 1.0e-7, 1.0e-9);
    AppendUniqueParameter(parameters, limits[0], parameterTolerance);
    AppendUniqueParameter(parameters, limits[1], parameterTolerance);
    return parameters;
}

bool ExpandBoundsWithDraftingCurve(tag_t viewTag, tag_t curveTag, LayoutBounds& bounds, bool& initialized)
{
    if (viewTag == NULL_TAG || curveTag == NULL_TAG)
    {
        return false;
    }

    UF_EVAL_p_t evaluator = nullptr;
    if (UF_EVAL_initialize(curveTag, &evaluator) != 0 || evaluator == nullptr)
    {
        return false;
    }

    bool expanded = false;
    double limits[2] = {0.0, 0.0};
    if (UF_EVAL_ask_limits(evaluator, limits) == 0)
    {
        const std::vector<double> parameters =
            CollectDraftingCurveExtremeParameters(evaluator, viewTag, limits);
        for (double parameter : parameters)
        {
            double modelPoint[3] = {0.0, 0.0, 0.0};
            double drawingPoint[2] = {0.0, 0.0};
            if (EvaluateCurveDrawingPoint(evaluator, viewTag, parameter, modelPoint, drawingPoint))
            {
                AccumulateBoundsPoint(bounds, drawingPoint[0], drawingPoint[1], initialized);
                expanded = true;
            }
        }
    }

    UF_EVAL_free(evaluator);
    return expanded;
}

bool GetDraftingCurveExtent(tag_t viewTag, NXOpen::Drawings::DraftingCurve* curve, DraftingCurveExtent& extent)
{
    if (curve == nullptr)
    {
        return false;
    }

    extent = DraftingCurveExtent{};
    extent.curve = curve;
    extent.tag = curve->Tag();
    return ExpandBoundsWithDraftingCurve(viewTag, extent.tag, extent.bounds, extent.initialized) && extent.initialized;
}

bool IsVisibleDimensionCurve(NXOpen::Drawings::DraftingCurve* curve)
{
    NXOpen::DisplayableObject* displayableCurve =
        dynamic_cast<NXOpen::DisplayableObject*>(curve);
    if (displayableCurve == nullptr)
    {
        return false;
    }

    if (displayableCurve->IsBlanked())
    {
        return false;
    }

    try
    {
        return displayableCurve->LineFont() == NXOpen::DisplayableObject::ObjectFontSolid;
    }
    catch (...)
    {
        return false;
    }
}

bool IsOuterContourChainCurve(NXOpen::Drawings::DraftingCurve* curve)
{
    if (curve == nullptr || curve->Tag() == NULL_TAG)
    {
        return false;
    }

    if (!IsVisibleDimensionCurve(curve))
    {
        return false;
    }

    UF_DRAW_drafting_curve_type_t curveType = UF_DRAW_unknown_type;
    if (UF_DRAW_ask_drafting_curve_type(curve->Tag(), &curveType) != 0)
    {
        return false;
    }

    return curveType == UF_DRAW_extracted_edge_type ||
        curveType == UF_DRAW_section_edge_type;
}

std::vector<DraftingCurveExtent> CollectDraftingCurveExtents(NXOpen::Drawings::DraftingView* view)
{
    std::vector<DraftingCurveExtent> extents;
    if (view == nullptr || view->DraftingBodies() == nullptr)
    {
        return extents;
    }

    for (NXOpen::Drawings::DraftingBody* draftingBody : *view->DraftingBodies())
    {
        if (draftingBody == nullptr || draftingBody->DraftingCurves() == nullptr)
        {
            continue;
        }

        for (NXOpen::Drawings::DraftingCurve* draftingCurve : *draftingBody->DraftingCurves())
        {
            if (draftingCurve == nullptr)
            {
                continue;
            }

            if (!IsVisibleDimensionCurve(draftingCurve))
            {
                continue;
            }

            DraftingCurveExtent extent;
            if (GetDraftingCurveExtent(view->Tag(), draftingCurve, extent))
            {
                extents.push_back(extent);
            }
        }
    }

    return extents;
}

double BoundsWidth(const LayoutBounds& bounds)
{
    return std::max(0.0, bounds.maxX - bounds.minX);
}

double BoundsHeight(const LayoutBounds& bounds)
{
    return std::max(0.0, bounds.maxY - bounds.minY);
}

bool IsZeroDimensionValue(double value)
{
    return std::abs(value) <= 0.01;
}

double BoundsArea(const LayoutBounds& bounds)
{
    return BoundsWidth(bounds) * BoundsHeight(bounds);
}

bool BoundsMissVisibleExtrema(const LayoutBounds& candidate, const LayoutBounds& visible, double tolerance)
{
    return candidate.minX > visible.minX + tolerance ||
           candidate.minY > visible.minY + tolerance ||
           candidate.maxX < visible.maxX - tolerance ||
           candidate.maxY < visible.maxY - tolerance;
}

bool TryBuildVisibleCurveBounds(
    const std::vector<DraftingCurveExtent>& extents,
    LayoutBounds& bounds)
{
    bool initialized = false;
    for (const DraftingCurveExtent& extent : extents)
    {
        if (!extent.initialized || extent.curve == nullptr || extent.tag == NULL_TAG)
        {
            continue;
        }

        AccumulateBoundsPoint(bounds, extent.bounds.minX, extent.bounds.minY, initialized);
        AccumulateBoundsPoint(bounds, extent.bounds.maxX, extent.bounds.maxY, initialized);
    }

    return initialized;
}

double BottomTitleBlockReserve(double sheetHeight)
{
    return sheetHeight <= 220.0 ? 35.0 : 45.0;
}

double EffectiveLayoutMargin(const RequestValues& request)
{
    // Layer drawing already divides the sheet into cells inside the page and
    // above the title block.  Reusing the full-sheet annotation margin here
    // subtracts it once again from every cell.  A two-row A4 layout then loses
    // 76 mm from an 81.5 mm-high cell and is forced down to an unnecessarily
    // small scale (for example 1:7).  Eight millimetres leaves room for the
    // compact 5.33 mm dimensions and the layer caption without wasting most of
    // the cell.
    if (request.targetLayer > 0)
    {
        return 8.0;
    }

    const bool createsDimensions =
        request.autoDimensions &&
        (request.dimensionOverall ||
         request.dimensionLinear ||
         request.dimensionAngle ||
         request.dimensionHole ||
         request.dimensionHoleLocation ||
         request.dimensionInnerClosedCurve);
    const double annotationSafety = createsDimensions
        ? std::max(18.0, request.viewSpacing)
        : 0.0;
    return std::max(5.0, request.sheetMargin) + annotationSafety;
}

double ScaleUsableWidth(const RequestValues& request, double sheetLength)
{
    return std::max(20.0, sheetLength - EffectiveLayoutMargin(request) * 2.0);
}

double ScaleUsableHeight(const RequestValues& request, double sheetHeight)
{
    return std::max(20.0, sheetHeight - EffectiveLayoutMargin(request) * 2.0);
}

bool AskDisplayedBodyBounds(NXOpen::Drawings::DraftingView* view, double borders[4], bool filterLargeBodies)
{
    if (view == nullptr || borders == nullptr || view->DraftingBodies() == nullptr)
    {
        return false;
    }
    (void)filterLargeBodies;

    std::vector<LayoutBounds> bodyBounds;
    for (NXOpen::Drawings::DraftingBody* draftingBody : *view->DraftingBodies())
    {
        if (draftingBody == nullptr || draftingBody->DraftingCurves() == nullptr)
        {
            continue;
        }

        LayoutBounds currentBodyBounds;
        bool currentBodyInitialized = false;
        for (NXOpen::Drawings::DraftingCurve* draftingCurve : *draftingBody->DraftingCurves())
        {
            if (draftingCurve == nullptr)
            {
                continue;
            }

            NXOpen::DisplayableObject* displayableCurve =
                dynamic_cast<NXOpen::DisplayableObject*>(draftingCurve);
            if (displayableCurve != nullptr && displayableCurve->IsBlanked())
            {
                continue;
            }

            ExpandBoundsWithDraftingCurve(
                view->Tag(),
                draftingCurve->Tag(),
                currentBodyBounds,
                currentBodyInitialized);
        }

        if (currentBodyInitialized &&
            currentBodyBounds.maxX > currentBodyBounds.minX &&
            currentBodyBounds.maxY > currentBodyBounds.minY)
        {
            bodyBounds.push_back(currentBodyBounds);
        }
    }

    if (bodyBounds.empty())
    {
        return false;
    }

    LayoutBounds bounds;
    bool initialized = false;
    for (const LayoutBounds& body : bodyBounds)
    {
        AccumulateBoundsPoint(bounds, body.minX, body.minY, initialized);
        AccumulateBoundsPoint(bounds, body.maxX, body.maxY, initialized);
    }

    if (!initialized)
    {
        const LayoutBounds* smallest = &bodyBounds[0];
        for (const LayoutBounds& body : bodyBounds)
        {
            if (BoundsArea(body) < BoundsArea(*smallest))
            {
                smallest = &body;
            }
        }
        bounds = *smallest;
    }

    borders[0] = bounds.minX;
    borders[1] = bounds.minY;
    borders[2] = bounds.maxX;
    borders[3] = bounds.maxY;
    return true;
}

bool AskViewCurveBorders(NXOpen::Drawings::DraftingView* view, double borders[4], bool filterLargeBodies = false)
{
    return AskDisplayedBodyBounds(view, borders, filterLargeBodies);
}

bool AskViewBorders(NXOpen::Drawings::DraftingView* view, double borders[4], bool filterLargeBodies = false)
{
    if (AskViewCurveBorders(view, borders, filterLargeBodies))
    {
        return true;
    }
    return view != nullptr && borders != nullptr && UF_DRAW_ask_view_borders(view->Tag(), borders) == 0;
}

void MoveViewByDelta(NXOpen::Drawings::DraftingView* view, double dx, double dy)
{
    if (view == nullptr || (std::abs(dx) < 0.001 && std::abs(dy) < 0.001))
    {
        return;
    }

    try
    {
        NXOpen::Point3d point = view->GetDrawingReferencePoint();
        point.X += dx;
        point.Y += dy;
        view->MoveView(point);
    }
    catch (const NXOpen::NXException&)
    {
    }
}

void MoveViewToBorderPosition(
    NXOpen::Drawings::DraftingView* view,
    double targetMinX,
    double targetMinY,
    double targetMaxX,
    double targetMaxY,
    bool useMinX,
    bool useMinY)
{
    double borders[4] = {0.0, 0.0, 0.0, 0.0};
    if (!AskViewBorders(view, borders))
    {
        return;
    }

    const double currentCenterX = (borders[0] + borders[2]) * 0.5;
    const double currentCenterY = (borders[1] + borders[3]) * 0.5;
    const double targetCenterX = (targetMinX + targetMaxX) * 0.5;
    const double targetCenterY = (targetMinY + targetMaxY) * 0.5;

    double dx = targetCenterX - currentCenterX;
    double dy = targetCenterY - currentCenterY;
    if (useMinX)
    {
        dx = targetMinX - borders[0];
    }
    else if (targetMaxX > targetMinX)
    {
        dx = targetMaxX - borders[2];
    }

    if (useMinY)
    {
        dy = targetMinY - borders[1];
    }
    else if (targetMaxY > targetMinY)
    {
        dy = targetMaxY - borders[3];
    }

    MoveViewByDelta(view, dx, dy);
}

LayoutBounds BoundsForCreatedViews(const std::vector<CreatedView>& views)
{
    LayoutBounds bounds;
    bool initialized = false;
    for (const CreatedView& created : views)
    {
        double borders[4] = {0.0, 0.0, 0.0, 0.0};
        if (!AskViewBorders(created.view, borders))
        {
            continue;
        }

        if (!initialized)
        {
            bounds.minX = borders[0];
            bounds.minY = borders[1];
            bounds.maxX = borders[2];
            bounds.maxY = borders[3];
            initialized = true;
        }
        else
        {
            bounds.minX = std::min(bounds.minX, borders[0]);
            bounds.minY = std::min(bounds.minY, borders[1]);
            bounds.maxX = std::max(bounds.maxX, borders[2]);
            bounds.maxY = std::max(bounds.maxY, borders[3]);
        }
    }

    return bounds;
}

void PromoteLargestAreaViewToFront(NXOpen::Session* session, std::vector<CreatedView>& views)
{
    if (views.empty())
    {
        return;
    }

    int frontIndex = -1;
    int bestIndex = -1;
    double frontArea = -1.0;
    double bestArea = -1.0;
    std::ostringstream log;
    log << "AutoCreateThreeViews: final view area check";

    for (int index = 0; index < static_cast<int>(views.size()); ++index)
    {
        CreatedView& created = views[static_cast<size_t>(index)];
        if (created.view == nullptr)
        {
            continue;
        }

        const std::vector<DraftingCurveExtent> extents = CollectDraftingCurveExtents(created.view);
        LayoutBounds bounds;
        double area = 0.0;
        double width = 0.0;
        double height = 0.0;
        if (TryBuildVisibleCurveBounds(extents, bounds))
        {
            width = BoundsWidth(bounds);
            height = BoundsHeight(bounds);
            area = BoundsArea(bounds);
        }

        log << " [" << created.label
            << ", area=" << area
            << ", width=" << width
            << ", height=" << height
            << ", curves=" << extents.size() << "]";

        if (created.label == "front")
        {
            frontIndex = index;
            frontArea = area;
        }
        if (area > bestArea + 1.0e-6)
        {
            bestArea = area;
            bestIndex = index;
        }
    }

    if (frontIndex >= 0 && bestIndex >= 0 && bestIndex != frontIndex && bestArea > frontArea + 1.0e-6)
    {
        std::swap(views[static_cast<size_t>(frontIndex)].label, views[static_cast<size_t>(bestIndex)].label);
        std::swap(views[static_cast<size_t>(frontIndex)].plannedPoint, views[static_cast<size_t>(bestIndex)].plannedPoint);
        log << ", promoted=" << views[static_cast<size_t>(frontIndex)].label
            << ", oldFrontIndex=" << frontIndex
            << ", newFrontIndex=" << bestIndex << ".";
    }
    else
    {
        log << ", promoted=none.";
    }
    WriteLine(session, log.str());
}

NXOpen::Drawings::DraftingView* FindCreatedView(const std::vector<CreatedView>& views, const std::string& label)
{
    for (const CreatedView& created : views)
    {
        if (created.label == label)
        {
            return created.view;
        }
    }

    return nullptr;
}

NXOpen::Point3d FindPlannedPoint(const std::vector<CreatedView>& views, const std::string& label)
{
    for (const CreatedView& created : views)
    {
        if (created.label == label)
        {
            return created.plannedPoint;
        }
    }

    return NXOpen::Point3d(0.0, 0.0, 0.0);
}

void ArrangeCreatedProjectedViews(
    const RequestValues& request,
    std::vector<CreatedView>& views,
    double sheetLength,
    double sheetHeight)
{
    NXOpen::Drawings::DraftingView* frontView = FindCreatedView(views, "front");
    if (frontView == nullptr)
    {
        return;
    }

    double frontBorders[4] = {0.0, 0.0, 0.0, 0.0};
    if (!AskViewBorders(frontView, frontBorders))
    {
        return;
    }

    const NXOpen::Point3d frontPoint = FindPlannedPoint(views, "front");
    const double frontCenterX = (frontBorders[0] + frontBorders[2]) * 0.5;
    const double frontCenterY = (frontBorders[1] + frontBorders[3]) * 0.5;
    const double frontHeight = frontBorders[3] - frontBorders[1];

    for (CreatedView& created : views)
    {
        if (created.label == "front" || created.view == nullptr)
        {
            continue;
        }

        double borders[4] = {0.0, 0.0, 0.0, 0.0};
        if (!AskViewBorders(created.view, borders))
        {
            continue;
        }

        const double width = borders[2] - borders[0];
        const double height = borders[3] - borders[1];
        const double plannedDx = created.plannedPoint.X - frontPoint.X;
        const double plannedDy = created.plannedPoint.Y - frontPoint.Y;

        if (std::abs(plannedDx) >= std::abs(plannedDy))
        {
            if (plannedDx > 0.0)
            {
                const double minX = frontBorders[2] + request.viewSpacing;
                MoveViewToBorderPosition(created.view, minX, frontCenterY - height * 0.5, minX + width, frontCenterY + height * 0.5, true, true);
            }
            else
            {
                const double maxX = frontBorders[0] - request.viewSpacing;
                MoveViewToBorderPosition(created.view, maxX - width, frontCenterY - height * 0.5, maxX, frontCenterY + height * 0.5, false, true);
            }
        }
        else
        {
            if (plannedDy > 0.0)
            {
                const double minY = frontBorders[3] + request.viewSpacing;
                MoveViewToBorderPosition(created.view, frontCenterX - width * 0.5, minY, frontCenterX + width * 0.5, minY + height, true, true);
            }
            else
            {
                const double maxY = frontBorders[1] - request.viewSpacing;
                MoveViewToBorderPosition(created.view, frontCenterX - width * 0.5, maxY - height, frontCenterX + width * 0.5, maxY, true, false);
            }
        }
    }

    (void)sheetLength;
    (void)sheetHeight;
}

LayoutBounds BoundsForAuxiliaryViews(const std::vector<CreatedAuxiliaryView>& views)
{
    LayoutBounds bounds;
    bool initialized = false;
    for (const CreatedAuxiliaryView& created : views)
    {
        double borders[4] = {0.0, 0.0, 0.0, 0.0};
        if (!AskViewBorders(created.view, borders, false))
        {
            continue;
        }

        if (!initialized)
        {
            bounds.minX = borders[0];
            bounds.minY = borders[1];
            bounds.maxX = borders[2];
            bounds.maxY = borders[3];
            initialized = true;
        }
        else
        {
            bounds.minX = std::min(bounds.minX, borders[0]);
            bounds.minY = std::min(bounds.minY, borders[1]);
            bounds.maxX = std::max(bounds.maxX, borders[2]);
            bounds.maxY = std::max(bounds.maxY, borders[3]);
        }
    }

    return bounds;
}

LayoutBounds MergeBounds(const LayoutBounds& first, const LayoutBounds& second)
{
    LayoutBounds merged;
    merged.minX = std::min(first.minX, second.minX);
    merged.minY = std::min(first.minY, second.minY);
    merged.maxX = std::max(first.maxX, second.maxX);
    merged.maxY = std::max(first.maxY, second.maxY);
    return merged;
}

LayoutBounds InflateBounds(const LayoutBounds& bounds, double amount)
{
    LayoutBounds inflated = bounds;
    inflated.minX -= amount;
    inflated.minY -= amount;
    inflated.maxX += amount;
    inflated.maxY += amount;
    return inflated;
}

LayoutBounds BoundsForAllViews(
    const std::vector<CreatedView>& projectedViews,
    const std::vector<CreatedAuxiliaryView>& auxiliaryViews)
{
    LayoutBounds projectedBounds = BoundsForCreatedViews(projectedViews);
    LayoutBounds auxiliaryBounds = BoundsForAuxiliaryViews(auxiliaryViews);
    const bool hasProjected = projectedBounds.maxX > projectedBounds.minX && projectedBounds.maxY > projectedBounds.minY;
    const bool hasAuxiliary = auxiliaryBounds.maxX > auxiliaryBounds.minX && auxiliaryBounds.maxY > auxiliaryBounds.minY;
    if (hasProjected && hasAuxiliary)
    {
        return MergeBounds(projectedBounds, auxiliaryBounds);
    }
    if (hasProjected)
    {
        return projectedBounds;
    }
    return auxiliaryBounds;
}

void MoveViewGroupToCenter(
    const std::vector<CreatedView>& projectedViews,
    const std::vector<CreatedAuxiliaryView>& auxiliaryViews,
    double targetCenterX,
    double targetCenterY)
{
    const LayoutBounds bounds = BoundsForAllViews(projectedViews, auxiliaryViews);
    if (bounds.maxX <= bounds.minX || bounds.maxY <= bounds.minY)
    {
        return;
    }
    const double dx = targetCenterX - (bounds.minX + bounds.maxX) * 0.5;
    const double dy = targetCenterY - (bounds.minY + bounds.maxY) * 0.5;
    for (const CreatedView& created : projectedViews)
    {
        MoveViewByDelta(created.view, dx, dy);
    }
    for (const CreatedAuxiliaryView& created : auxiliaryViews)
    {
        MoveViewByDelta(created.view, dx, dy);
    }
}

bool BoundsOverlapWithGap(const LayoutBounds& first, const LayoutBounds& second, double gap)
{
    return first.minX < second.maxX + gap &&
           first.maxX + gap > second.minX &&
           first.minY < second.maxY + gap &&
           first.maxY + gap > second.minY;
}

double ClampDouble(double value, double minValue, double maxValue)
{
    return std::max(minValue, std::min(value, maxValue));
}

struct ViewLayoutSize
{
    double width = 0.0;
    double height = 0.0;
    bool valid = false;
};

ViewLayoutSize ViewSizeAtDenominator(const ViewLayoutSize& sizeAtCurrentScale, double currentDenominator, double targetDenominator)
{
    const double scale = currentDenominator / std::max(1.0, targetDenominator);
    return {sizeAtCurrentScale.width * scale, sizeAtCurrentScale.height * scale, sizeAtCurrentScale.valid};
}

ViewLayoutSize AskCreatedViewSize(NXOpen::Drawings::DraftingView* view, bool filterLargeBodies = false)
{
    double borders[4] = {0.0, 0.0, 0.0, 0.0};
    if (!AskViewCurveBorders(view, borders, filterLargeBodies))
    {
        return {};
    }

    const double width = std::max(0.0, borders[2] - borders[0]);
    const double height = std::max(0.0, borders[3] - borders[1]);
    return {width, height, width > 0.0 && height > 0.0};
}

std::string EffectiveAuxiliaryCorner(const RequestValues& request, const CreatedAuxiliaryView& created);

void LogViewCurveBounds(
    NXOpen::Session* session,
    const std::string& label,
    NXOpen::Drawings::DraftingView* view,
    bool filterLargeBodies)
{
    if (session == nullptr || view == nullptr)
    {
        return;
    }

    double borders[4] = {0.0, 0.0, 0.0, 0.0};
    if (AskViewCurveBorders(view, borders, filterLargeBodies))
    {
        std::ostringstream line;
        line << "AutoCreateThreeViews: [view curve bounds] label=" << label
             << ", filterLargeBodies=" << (filterLargeBodies ? 1 : 0)
             << ", minX=" << borders[0]
             << ", minY=" << borders[1]
             << ", maxX=" << borders[2]
             << ", maxY=" << borders[3]
             << ", width=" << std::max(0.0, borders[2] - borders[0])
             << ", height=" << std::max(0.0, borders[3] - borders[1]);
        WriteLine(session, line.str());
    }
    else
    {
        std::ostringstream line;
        line << "AutoCreateThreeViews: [view curve bounds failed] label=" << label
             << ", filterLargeBodies=" << (filterLargeBodies ? 1 : 0)
             << ", DraftingBodies/DraftingCurves not available.";
        WriteLine(session, line.str());
    }

    if (view->DraftingBodies() == nullptr)
    {
        std::ostringstream line;
        line << "AutoCreateThreeViews: [view body bounds] label=" << label
             << ", DraftingBodies=null.";
        WriteLine(session, line.str());
        return;
    }

    int bodyIndex = 0;
    for (NXOpen::Drawings::DraftingBody* draftingBody : *view->DraftingBodies())
    {
        int curveCount = 0;
        int usedCurveCount = 0;
        LayoutBounds bodyBounds;
        bool initialized = false;

        if (draftingBody != nullptr && draftingBody->DraftingCurves() != nullptr)
        {
            for (NXOpen::Drawings::DraftingCurve* draftingCurve : *draftingBody->DraftingCurves())
            {
                ++curveCount;
                if (draftingCurve == nullptr)
                {
                    continue;
                }

                NXOpen::DisplayableObject* displayableCurve =
                    dynamic_cast<NXOpen::DisplayableObject*>(draftingCurve);
                if (displayableCurve != nullptr && displayableCurve->IsBlanked())
                {
                    continue;
                }

                if (ExpandBoundsWithDraftingCurve(view->Tag(), draftingCurve->Tag(), bodyBounds, initialized))
                {
                    ++usedCurveCount;
                }
            }
        }

        std::ostringstream line;
        line << "AutoCreateThreeViews: [view body bounds] label=" << label
             << ", bodyIndex=" << bodyIndex
             << ", curves=" << curveCount
             << ", usedCurves=" << usedCurveCount;
        if (initialized)
        {
            line << ", minX=" << bodyBounds.minX
                 << ", minY=" << bodyBounds.minY
                 << ", maxX=" << bodyBounds.maxX
                 << ", maxY=" << bodyBounds.maxY
                 << ", width=" << BoundsWidth(bodyBounds)
                 << ", height=" << BoundsHeight(bodyBounds)
                 << ", area=" << BoundsArea(bodyBounds);
        }
        WriteLine(session, line.str());
        ++bodyIndex;
    }
}

bool ProjectedLayoutSizeAtDenominator(
    const RequestValues& request,
    const std::vector<CreatedView>& projectedViews,
    double currentDenominator,
    double targetDenominator,
    double& width,
    double& height)
{
    width = 0.0;
    height = 0.0;
    NXOpen::Drawings::DraftingView* frontView = FindCreatedView(projectedViews, "front");
    if (frontView == nullptr)
    {
        return false;
    }

    const NXOpen::Point3d frontPoint = FindPlannedPoint(projectedViews, "front");
    ViewLayoutSize front = ViewSizeAtDenominator(AskCreatedViewSize(frontView), currentDenominator, targetDenominator);
    if (!front.valid)
    {
        return false;
    }

    double horizontalWidth = front.width;
    double horizontalHeight = front.height;
    int horizontalCount = 0;
    double verticalHeight = 0.0;
    double verticalWidth = 0.0;
    int verticalCount = 0;

    for (const CreatedView& created : projectedViews)
    {
        if (created.label == "front" || created.view == nullptr)
        {
            continue;
        }

        ViewLayoutSize size = ViewSizeAtDenominator(AskCreatedViewSize(created.view), currentDenominator, targetDenominator);
        if (!size.valid)
        {
            // A very small part measured at the temporary 1:1000 scale can make
            // an edge-on projected view collapse to a point. That view consumes
            // no measurable sheet area and must not cancel scale calculation for
            // the other valid views.
            continue;
        }

        const double plannedDx = created.plannedPoint.X - frontPoint.X;
        const double plannedDy = created.plannedPoint.Y - frontPoint.Y;
        if (std::abs(plannedDx) >= std::abs(plannedDy))
        {
            horizontalWidth += request.viewSpacing + size.width;
            horizontalHeight = std::max(horizontalHeight, size.height);
            ++horizontalCount;
        }
        else
        {
            verticalHeight += request.viewSpacing + size.height;
            verticalWidth = std::max(verticalWidth, size.width);
            ++verticalCount;
        }
    }

    width = std::max(horizontalWidth, verticalCount > 0 ? verticalWidth : 0.0);
    height = horizontalHeight + verticalHeight;
    return width > 0.0 && height > 0.0;
}

void AddAuxiliaryStackSize(
    const RequestValues& request,
    const std::vector<CreatedAuxiliaryView>& auxiliaryViews,
    const std::string& corner,
    double currentDenominator,
    double targetDenominator,
    double& width,
    double& height)
{
    width = 0.0;
    height = 0.0;
    int count = 0;
    for (const CreatedAuxiliaryView& created : auxiliaryViews)
    {
        if (created.corner != corner || created.view == nullptr)
        {
            continue;
        }

        ViewLayoutSize size = ViewSizeAtDenominator(AskCreatedViewSize(created.view, false), currentDenominator, targetDenominator);
        if (!size.valid)
        {
            continue;
        }

        width = std::max(width, size.width);
        if (count > 0)
        {
            height += request.viewSpacing;
        }
        height += size.height;
        ++count;
    }
}

void AddAuxiliarySideStackSize(
    const RequestValues& request,
    const std::vector<CreatedAuxiliaryView>& auxiliaryViews,
    bool leftSide,
    double currentDenominator,
    double targetDenominator,
    double& width,
    double& height)
{
    width = 0.0;
    height = 0.0;
    int count = 0;
    for (const CreatedAuxiliaryView& created : auxiliaryViews)
    {
        if (created.view == nullptr)
        {
            continue;
        }

        const std::string corner = EffectiveAuxiliaryCorner(request, created);
        const bool createdLeftSide = corner == "TopLeft" || corner == "BottomLeft";
        if (createdLeftSide != leftSide)
        {
            continue;
        }

        ViewLayoutSize size = ViewSizeAtDenominator(AskCreatedViewSize(created.view, false), currentDenominator, targetDenominator);
        if (!size.valid)
        {
            continue;
        }

        width = std::max(width, size.width);
        if (count > 0)
        {
            height += request.viewSpacing;
        }
        height += size.height;
        ++count;
    }
}

bool AllLayoutSizeAtDenominator(
    const RequestValues& request,
    const std::vector<CreatedView>& projectedViews,
    const std::vector<CreatedAuxiliaryView>& auxiliaryViews,
    double currentDenominator,
    double targetDenominator,
    double& width,
    double& height)
{
    double baseWidth = 0.0;
    double baseHeight = 0.0;
    if (!ProjectedLayoutSizeAtDenominator(request, projectedViews, currentDenominator, targetDenominator, baseWidth, baseHeight))
    {
        return false;
    }

    double leftWidth = 0.0, leftHeight = 0.0;
    double rightWidth = 0.0, rightHeight = 0.0;
    AddAuxiliarySideStackSize(request, auxiliaryViews, true, currentDenominator, targetDenominator, leftWidth, leftHeight);
    AddAuxiliarySideStackSize(request, auxiliaryViews, false, currentDenominator, targetDenominator, rightWidth, rightHeight);

    width = baseWidth;
    if (leftWidth > 0.0)
    {
        width += request.viewSpacing + leftWidth;
    }
    if (rightWidth > 0.0)
    {
        width += request.viewSpacing + rightWidth;
    }

    height = std::max(baseHeight, std::max(leftHeight, rightHeight));
    return width > 0.0 && height > 0.0;
}

std::string ChineseViewLabel(const std::string& label)
{
    if (label == "front") return "主视图";
    if (label == "top") return "俯视图";
    if (label == "bottom") return "仰视图";
    if (label == "left") return "左视图";
    if (label == "right") return "右视图";
    if (label == "back") return "后视图";
    if (label == "back bottom") return "下后视图";
    if (label == "isometric") return "轴测图";
    if (label == "flat pattern") return "展开图";
    return label;
}

std::string ChineseCornerLabel(const std::string& corner)
{
    if (corner == "TopLeft") return "左上";
    if (corner == "TopRight") return "右上";
    if (corner == "BottomLeft") return "左下";
    if (corner == "BottomRight") return "右下";
    return corner;
}

std::string EffectiveAuxiliaryCorner(const RequestValues& request, const CreatedAuxiliaryView& created)
{
    if (!request.auxiliaryAutoCompact)
    {
        return created.corner;
    }

    if (created.label == "isometric")
    {
        return "TopRight";
    }
    if (created.label == "flat pattern")
    {
        return "BottomRight";
    }

    return created.corner;
}

void LogScaleFormulaDetails(
    NXOpen::Session* session,
    const RequestValues& request,
    const std::vector<CreatedView>& projectedViews,
    const std::vector<CreatedAuxiliaryView>& auxiliaryViews,
    double currentDenominator,
    double targetDenominator,
    double sheetLength,
    double sheetHeight,
    const std::string& stage)
{
    const double usableWidth = ScaleUsableWidth(request, sheetLength);
    const double usableHeight = ScaleUsableHeight(request, sheetHeight);
    const NXOpen::Point3d frontPoint = FindPlannedPoint(projectedViews, "front");

    double horizontalWidth = 0.0;
    double horizontalHeight = 0.0;
    int horizontalCount = 0;
    double verticalWidth = 0.0;
    double verticalHeight = 0.0;
    int verticalCount = 0;

    std::ostringstream detail;
    detail << "AutoCreateThreeViews: 【比例计算明细-" << stage << "】目标比例 1:" << targetDenominator
           << "，图纸=" << sheetLength << "x" << sheetHeight
           << "，有效边距=" << EffectiveLayoutMargin(request)
           << "，间距=" << request.viewSpacing
           << "，比例可用宽=" << usableWidth
           << "，比例可用高=" << usableHeight << "。";
    WriteLine(session, detail.str());

    for (const CreatedView& created : projectedViews)
    {
        if (created.view == nullptr)
        {
            continue;
        }

        LogViewCurveBounds(session, created.label, created.view, true);

        const ViewLayoutSize size = ViewSizeAtDenominator(
            AskCreatedViewSize(created.view),
            currentDenominator,
            targetDenominator);
        if (!size.valid)
        {
            continue;
        }

        const double plannedDx = created.plannedPoint.X - frontPoint.X;
        const double plannedDy = created.plannedPoint.Y - frontPoint.Y;
        const bool horizontal = created.label == "front" || std::abs(plannedDx) >= std::abs(plannedDy);

        std::ostringstream line;
        line << "AutoCreateThreeViews: 【比例计算明细】"
             << ChineseViewLabel(created.label)
             << " 真实宽=" << size.width
             << "，真实高=" << size.height
             << "，参与方向=" << (horizontal ? "横向" : "纵向") << "。";
        WriteLine(session, line.str());

        if (horizontal)
        {
            if (horizontalCount > 0)
            {
                horizontalWidth += request.viewSpacing;
            }
            horizontalWidth += size.width;
            horizontalHeight = std::max(horizontalHeight, size.height);
            ++horizontalCount;
        }
        else
        {
            if (verticalCount > 0)
            {
                verticalHeight += request.viewSpacing;
            }
            verticalHeight += size.height;
            verticalWidth = std::max(verticalWidth, size.width);
            ++verticalCount;
        }
    }

    const double baseWidth = std::max(horizontalWidth, verticalWidth);
    const double baseHeight = horizontalHeight + (verticalCount > 0 ? request.viewSpacing : 0.0) + verticalHeight;

    double leftWidth = 0.0;
    double rightWidth = 0.0;
    double leftHeight = 0.0;
    double rightHeight = 0.0;
    int leftCount = 0;
    int rightCount = 0;
    for (const CreatedAuxiliaryView& created : auxiliaryViews)
    {
        if (created.view == nullptr)
        {
            continue;
        }

        LogViewCurveBounds(session, created.label, created.view, false);

        const ViewLayoutSize size = ViewSizeAtDenominator(
            AskCreatedViewSize(created.view, false),
            currentDenominator,
            targetDenominator);
        if (!size.valid)
        {
            continue;
        }

        const std::string corner = EffectiveAuxiliaryCorner(request, created);
        const bool leftSide = corner == "TopLeft" || corner == "BottomLeft";
        std::ostringstream line;
        line << "AutoCreateThreeViews: 【比例计算明细】"
             << ChineseViewLabel(created.label)
             << " 位置=" << ChineseCornerLabel(corner)
             << "，真实宽=" << size.width
             << "，真实高=" << size.height
             << "，参与方向=" << (leftSide ? "左侧附加" : "右侧附加") << "。";
        WriteLine(session, line.str());

        if (leftSide)
        {
            leftWidth = std::max(leftWidth, size.width);
            if (leftCount > 0)
            {
                leftHeight += request.viewSpacing;
            }
            leftHeight += size.height;
            ++leftCount;
        }
        else
        {
            rightWidth = std::max(rightWidth, size.width);
            if (rightCount > 0)
            {
                rightHeight += request.viewSpacing;
            }
            rightHeight += size.height;
            ++rightCount;
        }
    }

    double totalWidth = baseWidth;
    if (leftWidth > 0.0)
    {
        totalWidth += request.viewSpacing + leftWidth;
    }
    if (rightWidth > 0.0)
    {
        totalWidth += request.viewSpacing + rightWidth;
    }
    const double totalHeight = std::max(baseHeight, std::max(leftHeight, rightHeight));
    const double layoutMargin = EffectiveLayoutMargin(request);
    const double fullWidth = totalWidth + layoutMargin * 2.0;
    const double fullHeight = totalHeight + layoutMargin * 2.0;
    const double widthRatio = totalWidth / std::max(1.0, usableWidth);
    const double heightRatio = totalHeight / std::max(1.0, usableHeight);

    std::ostringstream formula;
    formula << "AutoCreateThreeViews: 【比例计算公式】横向主体宽=" << horizontalWidth
            << "，纵向最大宽=" << verticalWidth
            << "，主体宽=max(" << horizontalWidth << "," << verticalWidth << ")=" << baseWidth
            << "，左附加宽=" << leftWidth
            << "，右附加宽=" << rightWidth
            << "，内容总宽=" << totalWidth
            << "，比例可用宽=" << usableWidth
            << "，宽度需要比例=" << totalWidth << "/" << usableWidth << "=" << widthRatio << "。";
    WriteLine(session, formula.str());

    std::ostringstream formulaHeight;
    formulaHeight << "AutoCreateThreeViews: 【比例计算公式】横向主体高=" << horizontalHeight
                  << "，纵向累计高=" << verticalHeight
                  << "，主体高=" << baseHeight
                  << "，左附加高=" << leftHeight
                  << "，右附加高=" << rightHeight
                  << "，内容总高=" << totalHeight
                  << "，比例可用高=" << usableHeight
                  << "，高度需要比例=" << totalHeight << "/" << usableHeight << "=" << heightRatio
                  << "，决定方向=" << (widthRatio >= heightRatio ? "宽度" : "高度") << "。";
    WriteLine(session, formulaHeight.str());
}

double ChooseCompactSheetScaleDenominatorFromViews(
    double currentDenominator,
    const RequestValues& request,
    const std::vector<CreatedView>& projectedViews,
    const std::vector<CreatedAuxiliaryView>& auxiliaryViews,
    double sheetLength,
    double sheetHeight)
{
    const LayoutBounds bounds = BoundsForAllViews(projectedViews, auxiliaryViews);
    if (bounds.maxX <= bounds.minX || bounds.maxY <= bounds.minY)
    {
        return std::max(1.0, currentDenominator);
    }

    const double usableWidth = ScaleUsableWidth(request, sheetLength);
    const double usableHeight = ScaleUsableHeight(request, sheetHeight);

    for (double denominator = 1.0; denominator < 10000.0; denominator += 1.0)
    {
        double contentWidth = 0.0;
        double contentHeight = 0.0;
        if (!AllLayoutSizeAtDenominator(
                request,
                projectedViews,
                auxiliaryViews,
                currentDenominator,
                denominator,
                contentWidth,
                contentHeight))
        {
            break;
        }

        if (contentWidth <= usableWidth + 1.0e-6 && contentHeight <= usableHeight + 1.0e-6)
        {
            return denominator;
        }
    }

    return std::max(1.0, currentDenominator);
}

double RefineScaleDenominatorFromActualBounds(
    NXOpen::Session* session,
    double currentDenominator,
    const RequestValues& request,
    const std::vector<CreatedView>& projectedViews,
    const std::vector<CreatedAuxiliaryView>& auxiliaryViews,
    double sheetLength,
    double sheetHeight)
{
    const double usableWidth = ScaleUsableWidth(request, sheetLength);
    const double usableHeight = ScaleUsableHeight(request, sheetHeight);

    double currentWidth = 0.0;
    double currentHeight = 0.0;
    if (!AllLayoutSizeAtDenominator(
            request,
            projectedViews,
            auxiliaryViews,
            currentDenominator,
            currentDenominator,
            currentWidth,
            currentHeight))
    {
        return currentDenominator;
    }

    LogScaleFormulaDetails(
        session,
        request,
        projectedViews,
        auxiliaryViews,
        currentDenominator,
        currentDenominator,
        sheetLength,
        sheetHeight,
        "预测");

    std::ostringstream refineLog;
    refineLog << "AutoCreateThreeViews: 【比例计算结果】预测内容宽="
              << currentWidth << "/" << usableWidth
              << "，预测内容高=" << currentHeight << "/" << usableHeight
              << "，使用率=" << std::max(currentWidth / usableWidth, currentHeight / usableHeight)
              << "，当前比例=1:" << currentDenominator;

    double result = currentDenominator;
    for (double denominator = currentDenominator - 1.0; denominator >= 1.0; denominator -= 1.0)
    {
        double contentWidth = 0.0;
        double contentHeight = 0.0;
        if (!AllLayoutSizeAtDenominator(
                request,
                projectedViews,
                auxiliaryViews,
                currentDenominator,
                denominator,
                contentWidth,
                contentHeight))
        {
            break;
        }

        if (contentWidth > usableWidth + 1.0e-6 || contentHeight > usableHeight + 1.0e-6)
        {
            break;
        }

        result = denominator;
    }

    if (result >= currentDenominator)
    {
        refineLog << "，保持当前比例。";
        WriteLine(session, refineLog.str());
        return currentDenominator;
    }

    refineLog << "，预测收紧到 1:" << result << "。";
    WriteLine(session, refineLog.str());
    return result;
}

double ChooseScaleDenominatorFromOneToOneActualSizes(
    NXOpen::Session* session,
    const RequestValues& request,
    const std::vector<CreatedView>& projectedViews,
    const std::vector<CreatedAuxiliaryView>& auxiliaryViews,
    double currentDenominator,
    double sheetLength,
    double sheetHeight)
{
    const double usableWidth = ScaleUsableWidth(request, sheetLength);
    const double usableHeight = ScaleUsableHeight(request, sheetHeight);

    LogScaleFormulaDetails(
        session,
        request,
        projectedViews,
        auxiliaryViews,
        currentDenominator,
        1.0,
        sheetLength,
        sheetHeight,
        "1:1真实数据");

    double contentWidth = 0.0;
    double contentHeight = 0.0;
    if (!AllLayoutSizeAtDenominator(
            request,
            projectedViews,
            auxiliaryViews,
            currentDenominator,
            1.0,
            contentWidth,
            contentHeight))
    {
        WriteLine(session, "AutoCreateThreeViews: [final scale] real drafting curves missing; keep current denominator and do not use view border fallback.");
        return currentDenominator;
    }

    const double widthDenominator = contentWidth / usableWidth;
    const double heightDenominator = contentHeight / usableHeight;
    const double rawDenominator = std::max(widthDenominator, heightDenominator);
    const double finalDenominator = std::max(1.0, std::ceil(rawDenominator - 1.0e-9));

    std::ostringstream line;
    line << "AutoCreateThreeViews: 【最终比例公式】内容宽=" << contentWidth
         << "，可用宽=" << usableWidth
         << "，宽度需要比例=" << widthDenominator
         << "；内容高=" << contentHeight
         << "，可用高=" << usableHeight
         << "，高度需要比例=" << heightDenominator
         << "；取较大值=" << rawDenominator
         << "，向上取整，最终比例=1:" << finalDenominator << "。";
    WriteLine(session, line.str());

    return finalDenominator;
}

void SetExistingBaseViewScale(
    NXOpen::Session* session,
    NXOpen::Part* part,
    NXOpen::Drawings::BaseView* view,
    double numerator,
    double denominator)
{
    if (part == nullptr || part->DraftingViews() == nullptr || view == nullptr)
    {
        return;
    }

    NXOpen::Drawings::BaseViewBuilder* builder = nullptr;
    try
    {
        const NXOpen::Point3d placement = view->GetDrawingReferencePoint();
        builder = part->DraftingViews()->CreateBaseViewBuilder(view);
        builder->Scale()->SetNumerator(numerator);
        builder->Scale()->SetDenominator(denominator);
        builder->Style()->ViewStyleGeneral()->Scale()->SetNumerator(numerator);
        builder->Style()->ViewStyleGeneral()->Scale()->SetDenominator(denominator);
        builder->Placement()->Placement()->SetValue(nullptr, part->Views()->WorkView(), placement);
        builder->Commit();
        builder->Destroy();
    }
    catch (const NXOpen::NXException& ex)
    {
        if (builder != nullptr)
        {
            builder->Destroy();
        }
        std::ostringstream stream;
        stream << "AutoCreateThreeViews: rescale base view failed, NX "
               << ex.ErrorCode() << ", " << ex.Message();
        WriteLine(session, stream.str());
    }
    catch (...)
    {
        if (builder != nullptr)
        {
            builder->Destroy();
        }
        WriteLine(session, "AutoCreateThreeViews: rescale base view failed.");
    }
}

void SetExistingProjectedViewScale(
    NXOpen::Session* session,
    NXOpen::Part* part,
    NXOpen::Drawings::ProjectedView* view,
    double numerator,
    double denominator)
{
    if (part == nullptr || part->DraftingViews() == nullptr || view == nullptr)
    {
        return;
    }

    NXOpen::Drawings::ProjectedViewBuilder* builder = nullptr;
    try
    {
        builder = part->DraftingViews()->CreateProjectedViewBuilder(view);
        builder->Style()->ViewStyleGeneral()->Scale()->SetNumerator(numerator);
        builder->Style()->ViewStyleGeneral()->Scale()->SetDenominator(denominator);
        builder->Commit();
        builder->Destroy();
    }
    catch (const NXOpen::NXException& ex)
    {
        if (builder != nullptr)
        {
            builder->Destroy();
        }
        std::ostringstream stream;
        stream << "AutoCreateThreeViews: rescale projected view failed, NX "
               << ex.ErrorCode() << ", " << ex.Message();
        WriteLine(session, stream.str());
    }
    catch (...)
    {
        if (builder != nullptr)
        {
            builder->Destroy();
        }
        WriteLine(session, "AutoCreateThreeViews: rescale projected view failed.");
    }
}

void SetAllCreatedViewScales(
    NXOpen::Session* session,
    NXOpen::Part* part,
    const std::vector<CreatedView>& projectedViews,
    const std::vector<CreatedAuxiliaryView>& auxiliaryViews,
    double denominator)
{
    for (const CreatedView& created : projectedViews)
    {
        if (NXOpen::Drawings::BaseView* baseView = dynamic_cast<NXOpen::Drawings::BaseView*>(created.view))
        {
            SetExistingBaseViewScale(session, part, baseView, 1.0, denominator);
        }
        else if (NXOpen::Drawings::ProjectedView* projectedView = dynamic_cast<NXOpen::Drawings::ProjectedView*>(created.view))
        {
            SetExistingProjectedViewScale(session, part, projectedView, 1.0, denominator);
        }
    }

    for (const CreatedAuxiliaryView& created : auxiliaryViews)
    {
        const double numerator = created.label == "isometric" ? 0.7 : 1.0;
        if (NXOpen::Drawings::BaseView* baseView = dynamic_cast<NXOpen::Drawings::BaseView*>(created.view))
        {
            SetExistingBaseViewScale(session, part, baseView, numerator, denominator);
        }
        else if (NXOpen::Drawings::ProjectedView* projectedView = dynamic_cast<NXOpen::Drawings::ProjectedView*>(created.view))
        {
            SetExistingProjectedViewScale(session, part, projectedView, numerator, denominator);
        }
    }
}

std::vector<NXOpen::Drawings::DraftingView*> CollectCreatedDraftingViews(
    const std::vector<CreatedView>& projectedViews,
    const std::vector<CreatedAuxiliaryView>& auxiliaryViews)
{
    std::vector<NXOpen::Drawings::DraftingView*> views;
    std::set<tag_t> seen;
    auto addView = [&](NXOpen::Drawings::DraftingView* view) {
        if (view != nullptr && view->Tag() != NULL_TAG && seen.insert(view->Tag()).second)
        {
            views.push_back(view);
        }
    };
    for (const CreatedView& created : projectedViews)
    {
        addView(created.view);
    }
    for (const CreatedAuxiliaryView& created : auxiliaryViews)
    {
        addView(created.view);
    }
    return views;
}

void UpdateCreatedDraftingViews(
    NXOpen::Session* session,
    NXOpen::Part* part,
    const std::vector<CreatedView>& projectedViews,
    const std::vector<CreatedAuxiliaryView>& auxiliaryViews,
    const std::string& stage)
{
    if (part == nullptr || part->DraftingViews() == nullptr)
    {
        return;
    }
    const std::vector<NXOpen::Drawings::DraftingView*> views =
        CollectCreatedDraftingViews(projectedViews, auxiliaryViews);
    if (views.empty())
    {
        return;
    }
    try
    {
        part->DraftingViews()->UpdateViews(views);
    }
    catch (const NXOpen::NXException& ex)
    {
        std::ostringstream line;
        line << "AutoCreateThreeViews: update created views failed, stage=" << stage
             << ", NX " << ex.ErrorCode() << ", " << ex.Message();
        WriteLine(session, line.str());
    }
}

void MoveAuxiliaryViewsByDelta(const std::vector<CreatedAuxiliaryView>& views, double dx, double dy)
{
    for (const CreatedAuxiliaryView& created : views)
    {
        MoveViewByDelta(created.view, dx, dy);
    }
}

void CenterAllViewsInUsableArea(
    const RequestValues& request,
    const std::vector<CreatedView>& projectedViews,
    const std::vector<CreatedAuxiliaryView>& auxiliaryViews,
    double sheetLength,
    double sheetHeight)
{
    LayoutBounds projectedBounds = BoundsForCreatedViews(projectedViews);
    LayoutBounds auxiliaryBounds = BoundsForAuxiliaryViews(auxiliaryViews);
    const bool hasProjected = projectedBounds.maxX > projectedBounds.minX && projectedBounds.maxY > projectedBounds.minY;
    const bool hasAuxiliary = auxiliaryBounds.maxX > auxiliaryBounds.minX && auxiliaryBounds.maxY > auxiliaryBounds.minY;
    if (!hasProjected && !hasAuxiliary)
    {
        return;
    }

    LayoutBounds allBounds = hasProjected ? projectedBounds : auxiliaryBounds;
    if (hasProjected && hasAuxiliary)
    {
        allBounds = MergeBounds(projectedBounds, auxiliaryBounds);
    }

    const double layoutMargin = EffectiveLayoutMargin(request);
    const double usableMinX = layoutMargin;
    const double usableMaxX = std::max(usableMinX + 20.0, sheetLength - layoutMargin);
    const double usableMinY = layoutMargin;
    const double usableMaxY = std::max(usableMinY + 20.0, sheetHeight - layoutMargin);
    const double targetCenterX = (usableMinX + usableMaxX) * 0.5;
    const double targetCenterY = (usableMinY + usableMaxY) * 0.5 + 10.0;
    const double currentCenterX = (allBounds.minX + allBounds.maxX) * 0.5;
    const double currentCenterY = (allBounds.minY + allBounds.maxY) * 0.5;
    double dx = targetCenterX - currentCenterX;
    double dy = targetCenterY - currentCenterY;
    const double allWidth = BoundsWidth(allBounds);
    const double allHeight = BoundsHeight(allBounds);
    const double usableWidth = usableMaxX - usableMinX;
    const double usableHeight = usableMaxY - usableMinY;

    if (allWidth <= usableWidth && allBounds.minX + dx < usableMinX)
    {
        dx += usableMinX - (allBounds.minX + dx);
    }
    if (allWidth <= usableWidth && allBounds.maxX + dx > usableMaxX)
    {
        dx -= (allBounds.maxX + dx) - usableMaxX;
    }
    if (allHeight <= usableHeight && allBounds.minY + dy < usableMinY)
    {
        dy += usableMinY - (allBounds.minY + dy);
    }
    if (allHeight <= usableHeight && allBounds.maxY + dy > usableMaxY)
    {
        dy -= (allBounds.maxY + dy) - usableMaxY;
    }

    for (const CreatedView& created : projectedViews)
    {
        MoveViewByDelta(created.view, dx, dy);
    }
    MoveAuxiliaryViewsByDelta(auxiliaryViews, dx, dy);
}

void ArrangeAuxiliaryViews(
    const RequestValues& request,
    const std::vector<CreatedView>& projectedViews,
    const std::vector<CreatedAuxiliaryView>& views,
    double sheetLength,
    double sheetHeight)
{
    if (views.empty())
    {
        CenterAllViewsInUsableArea(request, projectedViews, views, sheetLength, sheetHeight);
        return;
    }

    const double layoutMargin = EffectiveLayoutMargin(request);
    const double usableMinX = layoutMargin;
    const double usableMinY = layoutMargin;
    const double usableMaxX = std::max(usableMinX + 20.0, sheetLength - layoutMargin);
    const double usableMaxY = std::max(usableMinY + 20.0, sheetHeight - layoutMargin);
    LayoutBounds baseBounds = BoundsForCreatedViews(projectedViews);
    const bool hasProjected = baseBounds.maxX > baseBounds.minX && baseBounds.maxY > baseBounds.minY;
    if (!hasProjected)
    {
        baseBounds.minX = usableMinX;
        baseBounds.minY = usableMinY;
        baseBounds.maxX = usableMinX;
        baseBounds.maxY = usableMinY;
    }

    std::map<std::string, int> cornerCounts;
    std::vector<LayoutBounds> placedAuxiliaryBounds;
    std::vector<LayoutBounds> occupiedBounds;
    if (hasProjected)
    {
        occupiedBounds.push_back(baseBounds);
    }

    std::map<std::string, std::vector<const CreatedAuxiliaryView*>> sideGroups;
    for (const CreatedAuxiliaryView& created : views)
    {
        if (created.view == nullptr)
        {
            continue;
        }
        const bool leftSide = created.corner == "TopLeft" || created.corner == "BottomLeft";
        sideGroups[leftSide ? "left" : "right"].push_back(&created);
    }

    std::map<const CreatedAuxiliaryView*, double> stackedY;
    for (const auto& group : sideGroups)
    {
        double totalHeight = 0.0;
        struct StackEntry
        {
            const CreatedAuxiliaryView* created = nullptr;
            double height = 0.0;
        };
        std::vector<StackEntry> entries;
        for (const CreatedAuxiliaryView* created : group.second)
        {
            double borders[4] = {0.0, 0.0, 0.0, 0.0};
            if (created == nullptr || !AskViewBorders(created->view, borders, false))
            {
                continue;
            }
            if (!entries.empty())
            {
                totalHeight += request.viewSpacing;
            }
            const double height = borders[3] - borders[1];
            totalHeight += height;
            entries.push_back({created, height});
        }

        if (entries.empty())
        {
            continue;
        }
        std::stable_sort(entries.begin(), entries.end(), [](const StackEntry& first, const StackEntry& second) {
            const bool firstTop = first.created != nullptr &&
                (first.created->corner == "TopLeft" || first.created->corner == "TopRight");
            const bool secondTop = second.created != nullptr &&
                (second.created->corner == "TopLeft" || second.created->corner == "TopRight");
            if (firstTop != secondTop)
            {
                return firstTop;
            }
            return false;
        });

        const bool hasTop = std::any_of(entries.begin(), entries.end(), [](const StackEntry& entry) {
            return entry.created != nullptr &&
                (entry.created->corner == "TopLeft" || entry.created->corner == "TopRight");
        });
        const bool hasBottom = std::any_of(entries.begin(), entries.end(), [](const StackEntry& entry) {
            return entry.created != nullptr &&
                (entry.created->corner == "BottomLeft" || entry.created->corner == "BottomRight");
        });

        double currentTop = baseBounds.maxY;
        if (hasBottom && !hasTop)
        {
            currentTop = baseBounds.minY + totalHeight;
        }
        if (currentTop - totalHeight < usableMinY)
        {
            currentTop = usableMinY + totalHeight;
        }
        if (currentTop > usableMaxY)
        {
            currentTop = usableMaxY;
        }

        for (const StackEntry& entry : entries)
        {
            const double minY = currentTop - entry.height;
            stackedY[entry.created] = minY;
            currentTop = minY - request.viewSpacing;
        }
    }

    for (const CreatedAuxiliaryView& created : views)
    {
        if (created.view == nullptr)
        {
            continue;
        }

        double borders[4] = {0.0, 0.0, 0.0, 0.0};
        if (!AskViewBorders(created.view, borders, false))
        {
            continue;
        }

        const double width = borders[2] - borders[0];
        const double height = borders[3] - borders[1];
        const int stackIndex = cornerCounts[created.corner]++;
        const bool leftSide = created.corner == "TopLeft" || created.corner == "BottomLeft";
        const double stackStep = (created.corner == "TopLeft" || created.corner == "TopRight")
            ? -(height + request.viewSpacing)
            : (height + request.viewSpacing);

        double targetMinX = baseBounds.maxX + request.viewSpacing;
        double targetMinY = baseBounds.minY + stackIndex * stackStep;

        if (leftSide)
        {
            targetMinX = baseBounds.minX - request.viewSpacing - width;
        }

        if (created.corner == "TopLeft" || created.corner == "TopRight")
        {
            targetMinY = baseBounds.maxY - height + stackIndex * stackStep;
        }
        const auto stackedIt = stackedY.find(&created);
        if (stackedIt != stackedY.end())
        {
            targetMinY = stackedIt->second;
        }

        if (targetMinY < usableMinY)
        {
            targetMinY = baseBounds.minY;
        }
        if (targetMinY + height > usableMaxY)
        {
            targetMinY = baseBounds.maxY - height;
        }
        targetMinY = ClampDouble(targetMinY, usableMinY, usableMaxY - height);

        if (width <= usableMaxX - usableMinX)
        {
            targetMinX = ClampDouble(targetMinX, usableMinX, usableMaxX - width);
        }

        MoveViewToBorderPosition(
            created.view,
            targetMinX,
            targetMinY,
            targetMinX + width,
            targetMinY + height,
            true,
            true);

        LayoutBounds placedBounds;
        placedBounds.minX = targetMinX;
        placedBounds.maxX = targetMinX + width;
        placedBounds.minY = targetMinY;
        placedBounds.maxY = targetMinY + height;
        placedAuxiliaryBounds.push_back(placedBounds);
        occupiedBounds.push_back(placedBounds);
    }

    CenterAllViewsInUsableArea(request, projectedViews, views, sheetLength, sheetHeight);
}

bool NeedsLargerScaleAfterUserPlacement(
    const RequestValues& request,
    const std::vector<CreatedView>& projectedViews,
    const std::vector<CreatedAuxiliaryView>& auxiliaryViews)
{
    const LayoutBounds projectedBounds = BoundsForCreatedViews(projectedViews);
    const bool hasProjected =
        projectedBounds.maxX > projectedBounds.minX &&
        projectedBounds.maxY > projectedBounds.minY;
    if (!hasProjected)
    {
        return false;
    }

    for (const CreatedAuxiliaryView& created : auxiliaryViews)
    {
        double borders[4] = {0.0, 0.0, 0.0, 0.0};
        if (!AskViewBorders(created.view, borders, false))
        {
            continue;
        }

        LayoutBounds auxiliaryBounds;
        auxiliaryBounds.minX = borders[0];
        auxiliaryBounds.minY = borders[1];
        auxiliaryBounds.maxX = borders[2];
        auxiliaryBounds.maxY = borders[3];
        if (BoundsOverlapWithGap(auxiliaryBounds, projectedBounds, request.viewSpacing))
        {
            return true;
        }
    }

    return false;
}

struct ViewTargetBounds
{
    NXOpen::Drawings::DraftingView* view = nullptr;
    LayoutBounds current;
    LayoutBounds target;
};

bool AskViewLayoutBounds(NXOpen::Drawings::DraftingView* view, bool auxiliary, LayoutBounds& bounds)
{
    double borders[4] = {0.0, 0.0, 0.0, 0.0};
    if (!AskViewBorders(view, borders, !auxiliary))
    {
        return false;
    }

    bounds.minX = borders[0];
    bounds.minY = borders[1];
    bounds.maxX = borders[2];
    bounds.maxY = borders[3];
    return bounds.maxX > bounds.minX && bounds.maxY > bounds.minY;
}

LayoutBounds BoundsWithSize(double minX, double minY, double width, double height)
{
    LayoutBounds bounds;
    bounds.minX = minX;
    bounds.minY = minY;
    bounds.maxX = minX + width;
    bounds.maxY = minY + height;
    return bounds;
}

void OffsetBounds(LayoutBounds& bounds, double dx, double dy)
{
    bounds.minX += dx;
    bounds.maxX += dx;
    bounds.minY += dy;
    bounds.maxY += dy;
}

void MoveViewToTargetBounds(const ViewTargetBounds& placement)
{
    if (placement.view == nullptr)
    {
        return;
    }

    MoveViewByDelta(
        placement.view,
        placement.target.minX - placement.current.minX,
        placement.target.minY - placement.current.minY);
}

UF_DRF_object_t MakeDrawingPositionObject(tag_t viewTag, double x, double y)
{
    UF_DRF_object_t object{};
    object.object_tag = NULL_TAG;
    object.object_view_tag = viewTag;
    object.object_assoc_type = UF_DRF_dwg_pos;
    object.object_assoc_modifier = 0;
    object.object2_tag = NULL_TAG;
    object.assoc_dwg_pos[0] = x;
    object.assoc_dwg_pos[1] = y;
    return object;
}

UF_DRF_object_t MakeDraftingCurveObject(tag_t viewTag, tag_t curveTag)
{
    UF_DRF_object_t object{};
    object.object_tag = curveTag;
    object.object_view_tag = viewTag;
    object.object_assoc_type = UF_DRF_assoc_type_none;
    object.object_assoc_modifier = 0;
    object.object2_tag = NULL_TAG;
    object.assoc_dwg_pos[0] = 0.0;
    object.assoc_dwg_pos[1] = 0.0;
    return object;
}

bool SelectOverallDimensionCurves(
    NXOpen::Drawings::DraftingView* view,
    const std::vector<DraftingCurveExtent>& extents,
    const LayoutBounds& bounds,
    bool horizontalDimension,
    double preferredCrossCoordinate,
    CurveAssocCandidate& first,
    CurveAssocCandidate& second)
{
    if (view == nullptr || view->Tag() == NULL_TAG)
    {
        return false;
    }

    struct ExtremePointCandidate
    {
        NXOpen::Drawings::DraftingCurve* curve = nullptr;
        NXOpen::InferSnapType::SnapType snapType = NXOpen::InferSnapType::SnapTypeCurve;
        NXOpen::Point3d modelPoint = NXOpen::Point3d(0.0, 0.0, 0.0);
        double drawingX = 0.0;
        double drawingY = 0.0;
        int sampleIndex = 0;
        int sampleCount = 0;
    };

    std::vector<ExtremePointCandidate> candidates;
    LayoutBounds visibleBounds;
    bool visibleInitialized = false;

    auto addCandidate = [&](const DraftingCurveExtent& extent,
                            const double modelPoint[3],
                            double drawingX,
                            double drawingY,
                            int sampleIndex,
                            int sampleCount) {
        ExtremePointCandidate candidate;
        candidate.curve = extent.curve;
        candidate.modelPoint = NXOpen::Point3d(modelPoint[0], modelPoint[1], modelPoint[2]);
        candidate.drawingX = drawingX;
        candidate.drawingY = drawingY;
        candidate.sampleIndex = sampleIndex;
        candidate.sampleCount = sampleCount;
        if (sampleIndex == 0)
        {
            candidate.snapType = NXOpen::InferSnapType::SnapTypeStart;
        }
        else if (sampleIndex == sampleCount)
        {
            candidate.snapType = NXOpen::InferSnapType::SnapTypeEnd;
        }
        else
        {
            candidate.snapType = NXOpen::InferSnapType::SnapTypeCurve;
        }
        candidates.push_back(candidate);
        AccumulateBoundsPoint(visibleBounds, drawingX, drawingY, visibleInitialized);
    };

    auto collectSampledPoints = [&](const DraftingCurveExtent& extent) {
        UF_EVAL_p_t evaluator = nullptr;
        if (UF_EVAL_initialize(extent.tag, &evaluator) != 0 || evaluator == nullptr)
        {
            return;
        }

        double limits[2] = {0.0, 0.0};
        if (UF_EVAL_ask_limits(evaluator, limits) != 0)
        {
            UF_EVAL_free(evaluator);
            return;
        }

        const std::vector<double> parameters =
            CollectDraftingCurveExtremeParameters(evaluator, view->Tag(), limits);
        const double parameterSpan = std::abs(limits[1] - limits[0]);
        const double endTolerance = std::max(parameterSpan * 1.0e-7, 1.0e-9);
        for (double parameter : parameters)
        {
            double modelPoint[3] = {0.0, 0.0, 0.0};
            double drawingPoint[2] = {0.0, 0.0};
            if (EvaluateCurveDrawingPoint(evaluator, view->Tag(), parameter, modelPoint, drawingPoint))
            {
                int sampleIndex = -1;
                int sampleCount = 0;
                if (std::abs(parameter - limits[0]) <= endTolerance)
                {
                    sampleIndex = 0;
                    sampleCount = 1;
                }
                else if (std::abs(parameter - limits[1]) <= endTolerance)
                {
                    sampleIndex = 1;
                    sampleCount = 1;
                }
                addCandidate(extent, modelPoint, drawingPoint[0], drawingPoint[1], sampleIndex, sampleCount);
            }
        }

        UF_EVAL_free(evaluator);
    };

    for (const DraftingCurveExtent& extent : extents)
    {
        if (!extent.initialized || extent.curve == nullptr || extent.tag == NULL_TAG)
        {
            continue;
        }

        collectSampledPoints(extent);
    }

    if (!visibleInitialized || candidates.empty())
    {
        return false;
    }

    const double totalWidth = std::max(1.0, BoundsWidth(visibleBounds));
    const double totalHeight = std::max(1.0, BoundsHeight(visibleBounds));
    const double tolerance = std::max(0.05, std::max(totalWidth, totalHeight) * 0.002);
    const double firstTarget = horizontalDimension ? bounds.maxX : bounds.maxY;
    const double secondTarget = horizontalDimension ? bounds.minX : bounds.minY;
    bool firstFound = false;
    bool secondFound = false;
    double firstMetric = std::numeric_limits<double>::max();
    double secondMetric = std::numeric_limits<double>::max();

    auto useExtremeCandidate = [&](const ExtremePointCandidate& candidate,
                                   bool forFirst,
                                   double target) {
        const double coord = horizontalDimension ? candidate.drawingX : candidate.drawingY;
        const double distance = std::abs(coord - target);
        if (distance > tolerance)
        {
            return;
        }

        const double crossCoord = horizontalDimension ? candidate.drawingY : candidate.drawingX;
        const double score = distance * 100000.0 + std::abs(crossCoord - preferredCrossCoordinate);

        CurveAssocCandidate& targetCandidate = forFirst ? first : second;
        bool& targetFound = forFirst ? firstFound : secondFound;
        double& targetMetric = forFirst ? firstMetric : secondMetric;
        if (!targetFound || score < targetMetric)
        {
            targetFound = true;
            targetMetric = score;
            targetCandidate.curve = candidate.curve;
            targetCandidate.snapType = candidate.snapType;
            targetCandidate.modelPoint = candidate.modelPoint;
            targetCandidate.drawingX = candidate.drawingX;
            targetCandidate.drawingY = candidate.drawingY;
        }
    };

    for (const ExtremePointCandidate& candidate : candidates)
    {
        useExtremeCandidate(candidate, true, firstTarget);
        useExtremeCandidate(candidate, false, secondTarget);
    }

    return firstFound && secondFound && first.curve != nullptr && second.curve != nullptr;
}

bool CreateHorizontalOverallDimension(
    NXOpen::Session* session,
    NXOpen::Part* workPart,
    NXOpen::Drawings::DraftingView* view,
    const LayoutBounds& bounds,
    const std::vector<DraftingCurveExtent>& extents,
    double offset)
{
    if (view == nullptr || view->Tag() == NULL_TAG)
    {
        return false;
    }
    if (IsZeroDimensionValue(BoundsWidth(bounds)))
    {
        WriteLine(session, "AutoCreateThreeViews: auto dimension horizontal overall skipped; dimension value is zero.");
        return false;
    }

    CurveAssocCandidate first;
    CurveAssocCandidate second;
    const double originY = bounds.maxY + offset;
    if (!SelectOverallDimensionCurves(view, extents, bounds, true, originY, first, second))
    {
        WriteLine(session, "AutoCreateThreeViews: auto dimension horizontal overall failed; left/right boundary curves not found.");
        return false;
    }

    NXOpen::View* nullView = nullptr;
    NXOpen::Annotations::Dimension* nullDimension = nullptr;
    NXOpen::Annotations::RapidDimensionBuilder* builder = workPart->Dimensions()->CreateRapidDimensionBuilder(nullDimension);
    if (builder == nullptr)
    {
        WriteLine(session, "AutoCreateThreeViews: auto dimension horizontal overall failed; builder not available.");
        return false;
    }

    try
    {
        NXOpen::Point3d assistPoint(0.0, 0.0, 0.0);
        builder->FirstAssociativity()->SetValue(first.snapType, first.curve, view, first.modelPoint, nullptr, nullView, assistPoint);
        builder->SecondAssociativity()->SetValue(second.snapType, second.curve, view, second.modelPoint, nullptr, nullView, assistPoint);
        builder->Style()->DimensionStyle()->SetTextCentered(true);
        builder->Style()->DimensionStyle()->SetNarrowDisplayType(NXOpen::Annotations::NarrowDisplayOptionNone);
        builder->Measurement()->SetMethod(NXOpen::Annotations::DimensionMeasurementBuilder::MeasurementMethodHorizontal);
        builder->Origin()->Origin()->SetValue(
            nullptr,
            nullView,
            NXOpen::Point3d((bounds.minX + bounds.maxX) * 0.5, originY, 0.0));
        builder->Commit();
        builder->Destroy();
        std::ostringstream log;
        log << "AutoCreateThreeViews: auto dimension horizontal overall created"
            << " leftCurve=" << static_cast<unsigned long long>(second.curve != nullptr ? second.curve->Tag() : NULL_TAG)
            << ", leftDrawing=(" << second.drawingX << "," << second.drawingY << ")"
            << ", rightCurve=" << static_cast<unsigned long long>(first.curve != nullptr ? first.curve->Tag() : NULL_TAG)
            << ", rightDrawing=(" << first.drawingX << "," << first.drawingY << ")"
            << ", targetMinX=" << bounds.minX
            << ", targetMaxX=" << bounds.maxX << ".";
        WriteLine(session, log.str());
        return true;
    }
    catch (const NXOpen::NXException& ex)
    {
        builder->Destroy();
        WriteLine(session, std::string("AutoCreateThreeViews: auto dimension horizontal overall failed, NXException: ") + ex.Message() + ".");
        return false;
    }
    catch (...)
    {
        builder->Destroy();
        WriteLine(session, "AutoCreateThreeViews: auto dimension horizontal overall failed, unknown exception.");
        return false;
    }
}

bool CreateVerticalOverallDimension(
    NXOpen::Session* session,
    NXOpen::Part* workPart,
    NXOpen::Drawings::DraftingView* view,
    const LayoutBounds& bounds,
    const std::vector<DraftingCurveExtent>& extents,
    double offset)
{
    if (view == nullptr || view->Tag() == NULL_TAG)
    {
        return false;
    }
    if (IsZeroDimensionValue(BoundsHeight(bounds)))
    {
        WriteLine(session, "AutoCreateThreeViews: auto dimension vertical overall skipped; dimension value is zero.");
        return false;
    }

    CurveAssocCandidate first;
    CurveAssocCandidate second;
    const double originX = bounds.minX - offset;
    if (!SelectOverallDimensionCurves(view, extents, bounds, false, originX, first, second))
    {
        WriteLine(session, "AutoCreateThreeViews: auto dimension vertical overall failed; bottom/top boundary curves not found.");
        return false;
    }

    NXOpen::View* nullView = nullptr;
    NXOpen::Annotations::Dimension* nullDimension = nullptr;
    NXOpen::Annotations::RapidDimensionBuilder* builder = workPart->Dimensions()->CreateRapidDimensionBuilder(nullDimension);
    if (builder == nullptr)
    {
        WriteLine(session, "AutoCreateThreeViews: auto dimension vertical overall failed; builder not available.");
        return false;
    }

    try
    {
        NXOpen::Point3d assistPoint(0.0, 0.0, 0.0);
        builder->FirstAssociativity()->SetValue(first.snapType, first.curve, view, first.modelPoint, nullptr, nullView, assistPoint);
        builder->SecondAssociativity()->SetValue(second.snapType, second.curve, view, second.modelPoint, nullptr, nullView, assistPoint);
        builder->Style()->DimensionStyle()->SetTextCentered(true);
        builder->Style()->DimensionStyle()->SetNarrowDisplayType(NXOpen::Annotations::NarrowDisplayOptionNone);
        builder->Measurement()->SetMethod(NXOpen::Annotations::DimensionMeasurementBuilder::MeasurementMethodVertical);
        builder->Origin()->Origin()->SetValue(
            nullptr,
            nullView,
            NXOpen::Point3d(originX, (bounds.minY + bounds.maxY) * 0.5, 0.0));
        builder->Commit();
        builder->Destroy();
        std::ostringstream log;
        log << "AutoCreateThreeViews: auto dimension vertical overall created"
            << " bottomCurve=" << static_cast<unsigned long long>(second.curve != nullptr ? second.curve->Tag() : NULL_TAG)
            << ", bottomDrawing=(" << second.drawingX << "," << second.drawingY << ")"
            << ", topCurve=" << static_cast<unsigned long long>(first.curve != nullptr ? first.curve->Tag() : NULL_TAG)
            << ", topDrawing=(" << first.drawingX << "," << first.drawingY << ")"
            << ", targetMinY=" << bounds.minY
            << ", targetMaxY=" << bounds.maxY << ".";
        WriteLine(session, log.str());
        return true;
    }
    catch (const NXOpen::NXException& ex)
    {
        builder->Destroy();
        WriteLine(session, std::string("AutoCreateThreeViews: auto dimension vertical overall failed, NXException: ") + ex.Message() + ".");
        return false;
    }
    catch (...)
    {
        builder->Destroy();
        WriteLine(session, "AutoCreateThreeViews: auto dimension vertical overall failed, unknown exception.");
        return false;
    }
}

void ExpandBounds(LayoutBounds& target, bool& initialized, const LayoutBounds& source)
{
    if (!initialized)
    {
        target = source;
        initialized = true;
        return;
    }

    target.minX = std::min(target.minX, source.minX);
    target.minY = std::min(target.minY, source.minY);
    target.maxX = std::max(target.maxX, source.maxX);
    target.maxY = std::max(target.maxY, source.maxY);
}

double ClosedCurvePointDistance2d(double ax, double ay, double bx, double by)
{
    const double dx = ax - bx;
    const double dy = ay - by;
    return std::sqrt(dx * dx + dy * dy);
}

bool GetCurveEndPoints(
    NXOpen::Drawings::DraftingView* view,
    const DraftingCurveExtent& extent,
    ClosedCurveSegment& segment)
{
    if (view == nullptr || !extent.initialized || extent.curve == nullptr || extent.tag == NULL_TAG)
    {
        return false;
    }

    UF_EVAL_p_t evaluator = nullptr;
    if (UF_EVAL_initialize(extent.tag, &evaluator) != 0 || evaluator == nullptr)
    {
        return false;
    }

    bool isArc = false;
    if (UF_EVAL_is_arc(evaluator, &isArc) == 0 && isArc)
    {
        UF_EVAL_arc_t arcData{};
        if (UF_EVAL_ask_arc(evaluator, &arcData) == 0 && arcData.is_periodic)
        {
            UF_EVAL_free(evaluator);
            return false;
        }
    }

    double limits[2] = {0.0, 0.0};
    if (UF_EVAL_ask_limits(evaluator, limits) != 0)
    {
        UF_EVAL_free(evaluator);
        return false;
    }

    double startModel[3] = {0.0, 0.0, 0.0};
    double endModel[3] = {0.0, 0.0, 0.0};
    double derivatives[3] = {0.0, 0.0, 0.0};
    const bool startOk = UF_EVAL_evaluate(evaluator, 0, limits[0], startModel, derivatives) == 0;
    const bool endOk = UF_EVAL_evaluate(evaluator, 0, limits[1], endModel, derivatives) == 0;
    UF_EVAL_free(evaluator);
    if (!startOk || !endOk)
    {
        return false;
    }

    double startDrawing[2] = {0.0, 0.0};
    double endDrawing[2] = {0.0, 0.0};
    if (UF_VIEW_map_model_to_drawing(view->Tag(), startModel, startDrawing) != 0 ||
        UF_VIEW_map_model_to_drawing(view->Tag(), endModel, endDrawing) != 0)
    {
        return false;
    }

    segment.extent = extent;
    segment.startModel = NXOpen::Point3d(startModel[0], startModel[1], startModel[2]);
    segment.endModel = NXOpen::Point3d(endModel[0], endModel[1], endModel[2]);
    segment.startX = startDrawing[0];
    segment.startY = startDrawing[1];
    segment.endX = endDrawing[0];
    segment.endY = endDrawing[1];
    return true;
}

bool GetCurveDrawingSamplePoints(
    NXOpen::Drawings::DraftingView* view,
    const DraftingCurveExtent& extent,
    std::vector<NXOpen::Point3d>& points)
{
    points.clear();
    if (view == nullptr || !extent.initialized || extent.curve == nullptr || extent.tag == NULL_TAG)
    {
        return false;
    }

    UF_EVAL_p_t evaluator = nullptr;
    if (UF_EVAL_initialize(extent.tag, &evaluator) != 0 || evaluator == nullptr)
    {
        return false;
    }

    bool isArc = false;
    if (UF_EVAL_is_arc(evaluator, &isArc) == 0 && isArc)
    {
        UF_EVAL_arc_t arcData{};
        if (UF_EVAL_ask_arc(evaluator, &arcData) == 0 && arcData.is_periodic)
        {
            UF_EVAL_free(evaluator);
            return false;
        }
    }

    double limits[2] = {0.0, 0.0};
    if (UF_EVAL_ask_limits(evaluator, limits) != 0)
    {
        UF_EVAL_free(evaluator);
        return false;
    }

    bool isLine = false;
    UF_EVAL_is_line(evaluator, &isLine);
    const int sampleCount = isLine ? 2 : 24;
    points.reserve(sampleCount);
    for (int i = 0; i < sampleCount; ++i)
    {
        const double ratio = sampleCount <= 1 ? 0.0 : static_cast<double>(i) / static_cast<double>(sampleCount - 1);
        const double parameter = limits[0] + (limits[1] - limits[0]) * ratio;
        double modelPoint[3] = {0.0, 0.0, 0.0};
        double derivatives[3] = {0.0, 0.0, 0.0};
        if (UF_EVAL_evaluate(evaluator, 0, parameter, modelPoint, derivatives) != 0)
        {
            continue;
        }

        double drawingPoint[2] = {0.0, 0.0};
        if (UF_VIEW_map_model_to_drawing(view->Tag(), modelPoint, drawingPoint) != 0)
        {
            continue;
        }
        points.push_back(NXOpen::Point3d(drawingPoint[0], drawingPoint[1], 0.0));
    }
    UF_EVAL_free(evaluator);

    return points.size() >= 2;
}

double GetDraftingCurveDrawingLength(
    NXOpen::Drawings::DraftingView* view,
    const DraftingCurveExtent& extent)
{
    std::vector<NXOpen::Point3d> points;
    if (!GetCurveDrawingSamplePoints(view, extent, points))
    {
        const double width = BoundsWidth(extent.bounds);
        const double height = BoundsHeight(extent.bounds);
        return std::sqrt(width * width + height * height);
    }

    double length = 0.0;
    for (size_t i = 1; i < points.size(); ++i)
    {
        length += ClosedCurvePointDistance2d(points[i - 1].X, points[i - 1].Y, points[i].X, points[i].Y);
    }
    return length;
}

double GetClosedCurveLoopDrawingPerimeter(
    NXOpen::Drawings::DraftingView* view,
    const ClosedCurveLoopCandidate& loop)
{
    double perimeter = 0.0;
    std::set<tag_t> usedCurveTags;
    for (const DraftingCurveExtent& extent : loop.extents)
    {
        if (extent.tag == NULL_TAG || usedCurveTags.find(extent.tag) != usedCurveTags.end())
        {
            continue;
        }
        usedCurveTags.insert(extent.tag);
        perimeter += GetDraftingCurveDrawingLength(view, extent);
    }
    return perimeter;
}

bool SegmentEndpointsTouch(
    const ClosedCurveSegment& a,
    const ClosedCurveSegment& b,
    double tolerance)
{
    return ClosedCurvePointDistance2d(a.startX, a.startY, b.startX, b.startY) <= tolerance ||
        ClosedCurvePointDistance2d(a.startX, a.startY, b.endX, b.endY) <= tolerance ||
        ClosedCurvePointDistance2d(a.endX, a.endY, b.startX, b.startY) <= tolerance ||
        ClosedCurvePointDistance2d(a.endX, a.endY, b.endX, b.endY) <= tolerance;
}

bool EndpointHasMate(
    const std::vector<ClosedCurveSegment>& segments,
    const std::vector<int>& indices,
    int segmentIndex,
    bool startPoint,
    double tolerance)
{
    const ClosedCurveSegment& segment = segments[segmentIndex];
    const double x = startPoint ? segment.startX : segment.endX;
    const double y = startPoint ? segment.startY : segment.endY;

    for (int otherIndex : indices)
    {
        const ClosedCurveSegment& other = segments[otherIndex];
        if (!(otherIndex == segmentIndex && startPoint))
        {
            if (ClosedCurvePointDistance2d(x, y, other.startX, other.startY) <= tolerance)
            {
                return true;
            }
        }
        if (!(otherIndex == segmentIndex && !startPoint))
        {
            if (ClosedCurvePointDistance2d(x, y, other.endX, other.endY) <= tolerance)
            {
                return true;
            }
        }
    }

    return false;
}

bool ComponentIsClosed(
    const std::vector<ClosedCurveSegment>& segments,
    const std::vector<int>& indices,
    double tolerance)
{
    if (indices.empty())
    {
        return false;
    }

    for (int segmentIndex : indices)
    {
        if (!EndpointHasMate(segments, indices, segmentIndex, true, tolerance) ||
            !EndpointHasMate(segments, indices, segmentIndex, false, tolerance))
        {
            return false;
        }
    }

    return true;
}

int FindParent(std::vector<int>& parents, int index)
{
    if (parents[index] != index)
    {
        parents[index] = FindParent(parents, parents[index]);
    }
    return parents[index];
}

void UnionParent(std::vector<int>& parents, int a, int b)
{
    const int parentA = FindParent(parents, a);
    const int parentB = FindParent(parents, b);
    if (parentA != parentB)
    {
        parents[parentB] = parentA;
    }
}

bool BoundsMatchViewBounds(const LayoutBounds& loopBounds, const LayoutBounds& viewBounds, double tolerance)
{
    const double edgeTolerance = std::max(0.5, tolerance * 5.0);
    return std::abs(loopBounds.minX - viewBounds.minX) <= edgeTolerance &&
        std::abs(loopBounds.minY - viewBounds.minY) <= edgeTolerance &&
        std::abs(loopBounds.maxX - viewBounds.maxX) <= edgeTolerance &&
        std::abs(loopBounds.maxY - viewBounds.maxY) <= edgeTolerance;
}

double BoundsOverlapArea(const LayoutBounds& a, const LayoutBounds& b)
{
    const double overlapWidth = std::min(a.maxX, b.maxX) - std::max(a.minX, b.minX);
    const double overlapHeight = std::min(a.maxY, b.maxY) - std::max(a.minY, b.minY);
    if (overlapWidth <= 0.0 || overlapHeight <= 0.0)
    {
        return 0.0;
    }
    return overlapWidth * overlapHeight;
}

bool ClosedLoopCoveredByKeptLoop(
    const ClosedCurveLoopCandidate& candidate,
    const ClosedCurveLoopCandidate& kept,
    double tolerance)
{
    const double candidateArea = BoundsArea(candidate.bounds);
    const double keptArea = BoundsArea(kept.bounds);
    const double smallerArea = std::min(candidateArea, keptArea);
    if (smallerArea <= 1.0e-6)
    {
        return false;
    }

    const double overlapRatio = BoundsOverlapArea(candidate.bounds, kept.bounds) / smallerArea;
    if (overlapRatio >= 0.55)
    {
        return true;
    }

    const double candidateCenterX = (candidate.bounds.minX + candidate.bounds.maxX) * 0.5;
    const double candidateCenterY = (candidate.bounds.minY + candidate.bounds.maxY) * 0.5;
    const double keptCenterX = (kept.bounds.minX + kept.bounds.maxX) * 0.5;
    const double keptCenterY = (kept.bounds.minY + kept.bounds.maxY) * 0.5;
    const double centerDistance = ClosedCurvePointDistance2d(candidateCenterX, candidateCenterY, keptCenterX, keptCenterY);
    const double widthDelta = std::abs(BoundsWidth(candidate.bounds) - BoundsWidth(kept.bounds));
    const double heightDelta = std::abs(BoundsHeight(candidate.bounds) - BoundsHeight(kept.bounds));
    const double maxSize = std::max(
        std::max(BoundsWidth(candidate.bounds), BoundsHeight(candidate.bounds)),
        std::max(BoundsWidth(kept.bounds), BoundsHeight(kept.bounds)));

    return centerDistance <= std::max(tolerance * 8.0, maxSize * 0.08) &&
        widthDelta <= std::max(tolerance * 8.0, maxSize * 0.12) &&
        heightDelta <= std::max(tolerance * 8.0, maxSize * 0.12);
}

struct ClosedLoopGraphVertex
{
    double x = 0.0;
    double y = 0.0;
};

struct ClosedLoopGraphEdge
{
    int startVertex = -1;
    int endVertex = -1;
    int segmentIndex = -1;
};

struct ClosedLoopGraphAdjacency
{
    int toVertex = -1;
    int edgeIndex = -1;
    bool forward = true;
    double angle = 0.0;
};

int FindOrAddClosedLoopGraphVertex(
    std::vector<ClosedLoopGraphVertex>& vertices,
    double x,
    double y,
    double tolerance)
{
    for (int i = 0; i < static_cast<int>(vertices.size()); ++i)
    {
        if (ClosedCurvePointDistance2d(vertices[i].x, vertices[i].y, x, y) <= tolerance)
        {
            return i;
        }
    }

    ClosedLoopGraphVertex vertex;
    vertex.x = x;
    vertex.y = y;
    vertices.push_back(vertex);
    return static_cast<int>(vertices.size()) - 1;
}

bool SameClosedLoopHalfEdge(const std::pair<int, bool>& a, int edgeIndex, bool forward)
{
    return a.first == edgeIndex && a.second == forward;
}

std::vector<ClosedCurveLoopCandidate> ExtractClosedBoundaryLoopsFromComponent(
    const std::vector<ClosedCurveSegment>& segments,
    const std::vector<int>& indices,
    double tolerance)
{
    std::vector<ClosedCurveLoopCandidate> loops;
    if (indices.size() < 3)
    {
        return loops;
    }

    std::vector<ClosedLoopGraphVertex> vertices;
    std::vector<ClosedLoopGraphEdge> edges;
    vertices.reserve(indices.size() * 2);
    edges.reserve(indices.size());

    for (int segmentIndex : indices)
    {
        const ClosedCurveSegment& segment = segments[segmentIndex];
        const int startVertex =
            FindOrAddClosedLoopGraphVertex(vertices, segment.startX, segment.startY, tolerance);
        const int endVertex =
            FindOrAddClosedLoopGraphVertex(vertices, segment.endX, segment.endY, tolerance);
        if (startVertex == endVertex)
        {
            continue;
        }

        ClosedLoopGraphEdge edge;
        edge.startVertex = startVertex;
        edge.endVertex = endVertex;
        edge.segmentIndex = segmentIndex;
        edges.push_back(edge);
    }

    if (edges.size() < 3 || vertices.size() < 3)
    {
        return loops;
    }

    std::vector<std::vector<ClosedLoopGraphAdjacency>> adjacency(vertices.size());
    for (int edgeIndex = 0; edgeIndex < static_cast<int>(edges.size()); ++edgeIndex)
    {
        const ClosedLoopGraphEdge& edge = edges[edgeIndex];
        const ClosedLoopGraphVertex& start = vertices[edge.startVertex];
        const ClosedLoopGraphVertex& end = vertices[edge.endVertex];

        ClosedLoopGraphAdjacency forward;
        forward.toVertex = edge.endVertex;
        forward.edgeIndex = edgeIndex;
        forward.forward = true;
        forward.angle = std::atan2(end.y - start.y, end.x - start.x);
        adjacency[edge.startVertex].push_back(forward);

        ClosedLoopGraphAdjacency backward;
        backward.toVertex = edge.startVertex;
        backward.edgeIndex = edgeIndex;
        backward.forward = false;
        backward.angle = std::atan2(start.y - end.y, start.x - end.x);
        adjacency[edge.endVertex].push_back(backward);
    }

    for (std::vector<ClosedLoopGraphAdjacency>& entries : adjacency)
    {
        std::sort(entries.begin(), entries.end(), [](const ClosedLoopGraphAdjacency& a, const ClosedLoopGraphAdjacency& b) {
            if (std::abs(a.angle - b.angle) > 1.0e-10)
            {
                return a.angle < b.angle;
            }
            return a.edgeIndex < b.edgeIndex;
        });
    }

    std::set<std::pair<int, bool>> visitedHalfEdges;
    const int maxSteps = static_cast<int>(edges.size()) * 4 + 16;
    for (int startEdgeIndex = 0; startEdgeIndex < static_cast<int>(edges.size()); ++startEdgeIndex)
    {
        for (bool startForward : {true, false})
        {
            if (visitedHalfEdges.find({startEdgeIndex, startForward}) != visitedHalfEdges.end())
            {
                continue;
            }

            int currentEdgeIndex = startEdgeIndex;
            bool currentForward = startForward;
            std::vector<int> cycleEdgeIndices;
            std::vector<int> cycleVertexIndices;
            bool closed = false;

            for (int step = 0; step < maxSteps; ++step)
            {
                if (!cycleEdgeIndices.empty() &&
                    SameClosedLoopHalfEdge({currentEdgeIndex, currentForward}, startEdgeIndex, startForward))
                {
                    closed = true;
                    break;
                }

                const std::pair<int, bool> currentKey{currentEdgeIndex, currentForward};
                if (visitedHalfEdges.find(currentKey) != visitedHalfEdges.end())
                {
                    break;
                }
                visitedHalfEdges.insert(currentKey);

                const ClosedLoopGraphEdge& currentEdge = edges[currentEdgeIndex];
                const int fromVertex = currentForward ? currentEdge.startVertex : currentEdge.endVertex;
                const int toVertex = currentForward ? currentEdge.endVertex : currentEdge.startVertex;
                cycleVertexIndices.push_back(fromVertex);
                cycleEdgeIndices.push_back(currentEdgeIndex);

                const std::vector<ClosedLoopGraphAdjacency>& entries = adjacency[toVertex];
                if (entries.empty())
                {
                    break;
                }

                int reverseIndex = -1;
                for (int i = 0; i < static_cast<int>(entries.size()); ++i)
                {
                    if (entries[i].edgeIndex == currentEdgeIndex && entries[i].toVertex == fromVertex)
                    {
                        reverseIndex = i;
                        break;
                    }
                }
                if (reverseIndex < 0)
                {
                    break;
                }

                const int nextIndex =
                    (reverseIndex - 1 + static_cast<int>(entries.size())) % static_cast<int>(entries.size());
                const ClosedLoopGraphAdjacency& next = entries[nextIndex];
                currentEdgeIndex = next.edgeIndex;
                currentForward = next.forward;
            }

            if (!closed || cycleEdgeIndices.size() < 3 || cycleVertexIndices.size() < 3)
            {
                continue;
            }

            double signedArea = 0.0;
            for (size_t i = 0; i < cycleVertexIndices.size(); ++i)
            {
                const ClosedLoopGraphVertex& a = vertices[cycleVertexIndices[i]];
                const ClosedLoopGraphVertex& b = vertices[cycleVertexIndices[(i + 1) % cycleVertexIndices.size()]];
                signedArea += a.x * b.y - b.x * a.y;
            }
            signedArea *= 0.5;
            if (std::abs(signedArea) <= tolerance * tolerance)
            {
                continue;
            }

            ClosedCurveLoopCandidate loop;
            std::set<int> uniqueEdges;
            for (int edgeIndex : cycleEdgeIndices)
            {
                if (uniqueEdges.insert(edgeIndex).second)
                {
                    const int segmentIndex = edges[edgeIndex].segmentIndex;
                    ExpandBounds(loop.bounds, loop.initialized, segments[segmentIndex].extent.bounds);
                    loop.extents.push_back(segments[segmentIndex].extent);
                }
            }

            if (loop.initialized && !loop.extents.empty())
            {
                loop.enclosedArea = std::abs(signedArea);
                loops.push_back(loop);
            }
        }
    }

    return loops;
}

struct ClosedLoopCoordinateTrace
{
    std::vector<int> edgeIndices;
    std::vector<int> vertexIndices;
    double signedArea = 0.0;
    bool initialized = false;
};

bool TraceClosedLoopByHalfEdge(
    const std::vector<ClosedLoopGraphVertex>& vertices,
    const std::vector<ClosedLoopGraphEdge>& edges,
    const std::vector<std::vector<ClosedLoopGraphAdjacency>>& adjacency,
    int startEdgeIndex,
    bool startForward,
    bool clockwiseChoice,
    ClosedLoopCoordinateTrace& trace)
{
    trace = ClosedLoopCoordinateTrace();
    if (startEdgeIndex < 0 || startEdgeIndex >= static_cast<int>(edges.size()))
    {
        return false;
    }

    std::set<std::pair<int, bool>> visited;
    int currentEdgeIndex = startEdgeIndex;
    bool currentForward = startForward;
    const int maxSteps = static_cast<int>(edges.size()) * 4 + 16;

    for (int step = 0; step < maxSteps; ++step)
    {
        if (!trace.edgeIndices.empty() &&
            currentEdgeIndex == startEdgeIndex &&
            currentForward == startForward)
        {
            trace.initialized = true;
            break;
        }

        const std::pair<int, bool> key{currentEdgeIndex, currentForward};
        if (visited.find(key) != visited.end())
        {
            return false;
        }
        visited.insert(key);

        const ClosedLoopGraphEdge& edge = edges[currentEdgeIndex];
        const int fromVertex = currentForward ? edge.startVertex : edge.endVertex;
        const int toVertex = currentForward ? edge.endVertex : edge.startVertex;
        trace.vertexIndices.push_back(fromVertex);
        trace.edgeIndices.push_back(currentEdgeIndex);

        if (toVertex < 0 || toVertex >= static_cast<int>(adjacency.size()))
        {
            return false;
        }

        const std::vector<ClosedLoopGraphAdjacency>& entries = adjacency[toVertex];
        int reverseIndex = -1;
        for (int i = 0; i < static_cast<int>(entries.size()); ++i)
        {
            if (entries[i].edgeIndex == currentEdgeIndex && entries[i].toVertex == fromVertex)
            {
                reverseIndex = i;
                break;
            }
        }
        if (reverseIndex < 0 || entries.empty())
        {
            return false;
        }

        const int stepDirection = clockwiseChoice ? -1 : 1;
        const int nextIndex =
            (reverseIndex + stepDirection + static_cast<int>(entries.size())) % static_cast<int>(entries.size());
        const ClosedLoopGraphAdjacency& next = entries[nextIndex];
        currentEdgeIndex = next.edgeIndex;
        currentForward = next.forward;
    }

    if (!trace.initialized || trace.edgeIndices.size() < 3 || trace.vertexIndices.size() < 3)
    {
        return false;
    }

    for (size_t i = 0; i < trace.vertexIndices.size(); ++i)
    {
        const ClosedLoopGraphVertex& a = vertices[trace.vertexIndices[i]];
        const ClosedLoopGraphVertex& b = vertices[trace.vertexIndices[(i + 1) % trace.vertexIndices.size()]];
        trace.signedArea += a.x * b.y - b.x * a.y;
    }
    trace.signedArea *= 0.5;
    return std::abs(trace.signedArea) > 1.0e-8;
}

bool TryExtractOuterLoopByPointCoordinates(
    const std::vector<ClosedCurveSegment>& segments,
    const std::vector<int>& indices,
    double tolerance,
    ClosedCurveLoopCandidate& outerLoop,
    double& outerArea)
{
    outerLoop = ClosedCurveLoopCandidate();
    outerArea = 0.0;
    if (indices.size() < 3)
    {
        return false;
    }

    std::vector<ClosedLoopGraphVertex> vertices;
    std::vector<ClosedLoopGraphEdge> edges;
    vertices.reserve(indices.size() * 2);
    edges.reserve(indices.size());

    for (int segmentIndex : indices)
    {
        const ClosedCurveSegment& segment = segments[segmentIndex];
        const int startVertex =
            FindOrAddClosedLoopGraphVertex(vertices, segment.startX, segment.startY, tolerance);
        const int endVertex =
            FindOrAddClosedLoopGraphVertex(vertices, segment.endX, segment.endY, tolerance);
        if (startVertex == endVertex)
        {
            continue;
        }

        ClosedLoopGraphEdge edge;
        edge.startVertex = startVertex;
        edge.endVertex = endVertex;
        edge.segmentIndex = segmentIndex;
        edges.push_back(edge);
    }

    if (edges.size() < 3 || vertices.size() < 3)
    {
        return false;
    }

    std::vector<std::vector<ClosedLoopGraphAdjacency>> adjacency(vertices.size());
    for (int edgeIndex = 0; edgeIndex < static_cast<int>(edges.size()); ++edgeIndex)
    {
        const ClosedLoopGraphEdge& edge = edges[edgeIndex];
        const ClosedLoopGraphVertex& start = vertices[edge.startVertex];
        const ClosedLoopGraphVertex& end = vertices[edge.endVertex];

        ClosedLoopGraphAdjacency forward;
        forward.toVertex = edge.endVertex;
        forward.edgeIndex = edgeIndex;
        forward.forward = true;
        forward.angle = std::atan2(end.y - start.y, end.x - start.x);
        adjacency[edge.startVertex].push_back(forward);

        ClosedLoopGraphAdjacency backward;
        backward.toVertex = edge.startVertex;
        backward.edgeIndex = edgeIndex;
        backward.forward = false;
        backward.angle = std::atan2(start.y - end.y, start.x - end.x);
        adjacency[edge.endVertex].push_back(backward);
    }

    for (std::vector<ClosedLoopGraphAdjacency>& entries : adjacency)
    {
        std::sort(entries.begin(), entries.end(), [](const ClosedLoopGraphAdjacency& a, const ClosedLoopGraphAdjacency& b) {
            if (std::abs(a.angle - b.angle) > 1.0e-10)
            {
                return a.angle < b.angle;
            }
            return a.edgeIndex < b.edgeIndex;
        });
    }

    int startVertex = -1;
    for (int i = 0; i < static_cast<int>(vertices.size()); ++i)
    {
        if (startVertex < 0 ||
            vertices[i].x > vertices[startVertex].x + tolerance ||
            (std::abs(vertices[i].x - vertices[startVertex].x) <= tolerance &&
             vertices[i].y > vertices[startVertex].y))
        {
            startVertex = i;
        }
    }
    if (startVertex < 0 || adjacency[startVertex].empty())
    {
        return false;
    }

    ClosedLoopCoordinateTrace bestTrace;
    double bestArea = -1.0;
    for (const ClosedLoopGraphAdjacency& start : adjacency[startVertex])
    {
        for (bool clockwiseChoice : {true, false})
        {
            ClosedLoopCoordinateTrace trace;
            if (!TraceClosedLoopByHalfEdge(
                    vertices,
                    edges,
                    adjacency,
                    start.edgeIndex,
                    start.forward,
                    clockwiseChoice,
                    trace))
            {
                continue;
            }

            const double area = std::abs(trace.signedArea);
            if (area > bestArea)
            {
                bestArea = area;
                bestTrace = trace;
            }
        }
    }

    if (!bestTrace.initialized || bestArea <= 1.0e-8)
    {
        return false;
    }

    std::set<int> uniqueEdges;
    for (int edgeIndex : bestTrace.edgeIndices)
    {
        if (uniqueEdges.insert(edgeIndex).second)
        {
            const int segmentIndex = edges[edgeIndex].segmentIndex;
            ExpandBounds(outerLoop.bounds, outerLoop.initialized, segments[segmentIndex].extent.bounds);
            outerLoop.extents.push_back(segments[segmentIndex].extent);
        }
    }

    outerArea = bestArea;
    outerLoop.enclosedArea = bestArea;
    return outerLoop.initialized && !outerLoop.extents.empty();
}

bool TryFindOuterContourLoopByPointCoordinates(
    NXOpen::Session* session,
    NXOpen::Part* workPart,
    ShallowDetailFilterCache& shallowCache,
    NXOpen::Drawings::DraftingView* view,
    const std::vector<DraftingCurveExtent>& extents,
    const LayoutBounds& viewBounds,
    const std::string& label,
    ClosedCurveLoopCandidate& outerLoop,
    double& outerArea)
{
    outerLoop = ClosedCurveLoopCandidate();
    outerArea = 0.0;

    std::vector<ClosedCurveSegment> segments;
    const double viewSize = std::max(BoundsWidth(viewBounds), BoundsHeight(viewBounds));
    const double connectTolerance = std::max(0.05, viewSize * 0.0015);
    int skippedShallowDetailCurves = 0;
    int skippedEndpointCurves = 0;

    for (const DraftingCurveExtent& extent : extents)
    {
        if (DraftingCurveHasShallowDetailParent(workPart, shallowCache, extent))
        {
            ++skippedShallowDetailCurves;
            continue;
        }

        ClosedCurveSegment segment;
        if (GetCurveEndPoints(view, extent, segment))
        {
            segments.push_back(segment);
        }
        else
        {
            ++skippedEndpointCurves;
        }
    }

    const int segmentCount = static_cast<int>(segments.size());
    if (segmentCount < 3)
    {
        WriteLine(session, "AutoCreateThreeViews: " + label + " point-coordinate outer contour not found; not enough drawable curve segments.");
        return false;
    }

    std::vector<int> parents(segmentCount);
    for (int i = 0; i < segmentCount; ++i)
    {
        parents[i] = i;
    }

    for (int i = 0; i < segmentCount; ++i)
    {
        for (int j = i + 1; j < segmentCount; ++j)
        {
            if (SegmentEndpointsTouch(segments[i], segments[j], connectTolerance))
            {
                UnionParent(parents, i, j);
            }
        }
    }

    std::map<int, std::vector<int>> components;
    for (int i = 0; i < segmentCount; ++i)
    {
        components[FindParent(parents, i)].push_back(i);
    }

    int attemptedComponents = 0;
    int closedComponents = 0;
    int candidateLoops = 0;
    for (const auto& entry : components)
    {
        const std::vector<int>& indices = entry.second;
        if (indices.size() < 3)
        {
            continue;
        }
        ++attemptedComponents;

        if (!ComponentIsClosed(segments, indices, connectTolerance))
        {
            continue;
        }
        ++closedComponents;

        ClosedCurveLoopCandidate componentLoop;
        double componentArea = 0.0;
        if (!TryExtractOuterLoopByPointCoordinates(
                segments,
                indices,
                connectTolerance,
                componentLoop,
                componentArea))
        {
            continue;
        }

        ++candidateLoops;
        const double componentBoundsArea = BoundsArea(componentLoop.bounds);
        const double bestBoundsArea = BoundsArea(outerLoop.bounds);
        if (!outerLoop.initialized ||
            componentArea > outerArea + 1.0e-8 ||
            (std::abs(componentArea - outerArea) <= 1.0e-8 && componentBoundsArea > bestBoundsArea))
        {
            outerLoop = componentLoop;
            outerArea = componentArea;
        }
    }

    std::ostringstream log;
    log << "AutoCreateThreeViews: " << label
        << " point-coordinate outer contour"
        << " segments=" << segmentCount
        << ", components=" << components.size()
        << ", attemptedComponents=" << attemptedComponents
        << ", closedComponents=" << closedComponents
        << ", candidateLoops=" << candidateLoops
        << ", skippedShallow=" << skippedShallowDetailCurves
        << ", skippedEndpoint=" << skippedEndpointCurves;
    if (outerLoop.initialized)
    {
        log << ", area=" << outerArea
            << ", minX=" << outerLoop.bounds.minX
            << ", minY=" << outerLoop.bounds.minY
            << ", maxX=" << outerLoop.bounds.maxX
            << ", maxY=" << outerLoop.bounds.maxY
            << ", width=" << BoundsWidth(outerLoop.bounds)
            << ", height=" << BoundsHeight(outerLoop.bounds);
    }
    log << ".";
    WriteLine(session, log.str());

    return outerLoop.initialized && !outerLoop.extents.empty();
}

bool TryFindOuterContourLoopByMaxArea(
    NXOpen::Session* session,
    NXOpen::Part* workPart,
    ShallowDetailFilterCache& shallowCache,
    NXOpen::Drawings::DraftingView* view,
    const std::vector<DraftingCurveExtent>& extents,
    const LayoutBounds& viewBounds,
    const std::string& label,
    ClosedCurveLoopCandidate& outerLoop,
    double& outerArea)
{
    outerLoop = ClosedCurveLoopCandidate();
    outerArea = 0.0;

    std::vector<ClosedCurveSegment> segments;
    const double viewSize = std::max(BoundsWidth(viewBounds), BoundsHeight(viewBounds));
    const double connectTolerance = std::max(0.05, viewSize * 0.0015);
    int skippedHiddenOrSmooth = 0;
    int skippedShallowDetailCurves = 0;
    int skippedEndpointCurves = 0;

    for (const DraftingCurveExtent& extent : extents)
    {
        if (!IsOuterContourChainCurve(extent.curve))
        {
            ++skippedHiddenOrSmooth;
            continue;
        }

        if (DraftingCurveHasShallowDetailParent(workPart, shallowCache, extent))
        {
            ++skippedShallowDetailCurves;
            continue;
        }

        ClosedCurveSegment segment;
        if (GetCurveEndPoints(view, extent, segment))
        {
            segments.push_back(segment);
        }
        else
        {
            ++skippedEndpointCurves;
        }
    }

    const int segmentCount = static_cast<int>(segments.size());
    if (segmentCount < 3)
    {
        WriteLine(session, "AutoCreateThreeViews: " + label + " max-area outer contour not found; not enough filtered curve segments.");
        return false;
    }

    std::vector<int> parents(segmentCount);
    for (int i = 0; i < segmentCount; ++i)
    {
        parents[i] = i;
    }

    for (int i = 0; i < segmentCount; ++i)
    {
        for (int j = i + 1; j < segmentCount; ++j)
        {
            if (SegmentEndpointsTouch(segments[i], segments[j], connectTolerance))
            {
                UnionParent(parents, i, j);
            }
        }
    }

    std::map<int, std::vector<int>> components;
    for (int i = 0; i < segmentCount; ++i)
    {
        components[FindParent(parents, i)].push_back(i);
    }

    int closedComponents = 0;
    int loopCandidates = 0;
    for (const auto& entry : components)
    {
        const std::vector<int>& indices = entry.second;
        if (indices.size() < 3 || !ComponentIsClosed(segments, indices, connectTolerance))
        {
            continue;
        }
        ++closedComponents;

        std::vector<ClosedCurveLoopCandidate> componentLoops =
            ExtractClosedBoundaryLoopsFromComponent(segments, indices, connectTolerance);

        for (const ClosedCurveLoopCandidate& loop : componentLoops)
        {
            if (!loop.initialized || loop.extents.empty())
            {
                continue;
            }

            const double enclosedArea = loop.enclosedArea;
            const double boundsArea = BoundsArea(loop.bounds);
            const double bestBoundsArea = BoundsArea(outerLoop.bounds);
            if (enclosedArea <= connectTolerance * connectTolerance)
            {
                continue;
            }
            ++loopCandidates;

            if (!outerLoop.initialized ||
                enclosedArea > outerArea + connectTolerance * connectTolerance ||
                (std::abs(enclosedArea - outerArea) <= connectTolerance * connectTolerance && boundsArea > bestBoundsArea))
            {
                outerLoop = loop;
                outerArea = enclosedArea;
            }
        }
    }

    std::ostringstream log;
    log << "AutoCreateThreeViews: " << label
        << " max-area outer contour"
        << " filteredSegments=" << segmentCount
        << ", components=" << components.size()
        << ", closedComponents=" << closedComponents
        << ", loopCandidates=" << loopCandidates
        << ", skippedHiddenOrSmooth=" << skippedHiddenOrSmooth
        << ", skippedShallow=" << skippedShallowDetailCurves
        << ", skippedEndpoint=" << skippedEndpointCurves;
    if (outerLoop.initialized)
    {
        log << ", area=" << outerArea
            << ", minX=" << outerLoop.bounds.minX
            << ", minY=" << outerLoop.bounds.minY
            << ", maxX=" << outerLoop.bounds.maxX
            << ", maxY=" << outerLoop.bounds.maxY
            << ", width=" << BoundsWidth(outerLoop.bounds)
            << ", height=" << BoundsHeight(outerLoop.bounds);
    }
    log << ".";
    WriteLine(session, log.str());

    return outerLoop.initialized && !outerLoop.extents.empty();
}

std::vector<ClosedCurveLoopCandidate> CollectClosedCurveLoops(
    NXOpen::Session* session,
    NXOpen::Part* workPart,
    ShallowDetailFilterCache& shallowCache,
    NXOpen::Drawings::DraftingView* view,
    const std::vector<DraftingCurveExtent>& extents,
    const LayoutBounds& viewBounds,
    const std::string& label,
    bool skipViewOuterLoop = true)
{
    std::vector<ClosedCurveLoopCandidate> loops;
    std::vector<ClosedCurveSegment> segments;
    const double viewSize = std::max(BoundsWidth(viewBounds), BoundsHeight(viewBounds));
    const double connectTolerance = std::max(0.05, viewSize * 0.0015);
    int skippedShallowDetailCurves = 0;

    for (const DraftingCurveExtent& extent : extents)
    {
        if (DraftingCurveHasShallowDetailParent(workPart, shallowCache, extent))
        {
            ++skippedShallowDetailCurves;
            continue;
        }

        ClosedCurveSegment segment;
        if (GetCurveEndPoints(view, extent, segment))
        {
            segments.push_back(segment);
        }
    }

    const int segmentCount = static_cast<int>(segments.size());
    if (segmentCount == 0)
    {
        return loops;
    }

    std::vector<int> parents(segmentCount);
    for (int i = 0; i < segmentCount; ++i)
    {
        parents[i] = i;
    }

    for (int i = 0; i < segmentCount; ++i)
    {
        for (int j = i + 1; j < segmentCount; ++j)
        {
            if (SegmentEndpointsTouch(segments[i], segments[j], connectTolerance))
            {
                UnionParent(parents, i, j);
            }
        }
    }

    std::map<int, std::vector<int>> components;
    for (int i = 0; i < segmentCount; ++i)
    {
        components[FindParent(parents, i)].push_back(i);
    }

    std::set<std::string> createdSignatures;
    int skippedOuter = 0;
    int skippedDuplicate = 0;
    int extractedBoundaryLoops = 0;
    for (const auto& entry : components)
    {
        const std::vector<int>& indices = entry.second;
        if (!ComponentIsClosed(segments, indices, connectTolerance))
        {
            continue;
        }

        std::vector<ClosedCurveLoopCandidate> componentLoops =
            ExtractClosedBoundaryLoopsFromComponent(segments, indices, connectTolerance);
        if (componentLoops.empty())
        {
            ClosedCurveLoopCandidate loop;
            for (int segmentIndex : indices)
            {
                ExpandBounds(loop.bounds, loop.initialized, segments[segmentIndex].extent.bounds);
                loop.extents.push_back(segments[segmentIndex].extent);
            }
            if (loop.initialized)
            {
                componentLoops.push_back(loop);
            }
        }
        extractedBoundaryLoops += static_cast<int>(componentLoops.size());

        for (const ClosedCurveLoopCandidate& loop : componentLoops)
        {
            if (!loop.initialized)
            {
                continue;
            }

            const double width = BoundsWidth(loop.bounds);
            const double height = BoundsHeight(loop.bounds);
            if (std::max(width, height) <= std::max(0.2, viewSize * 0.004))
            {
                continue;
            }

            if (skipViewOuterLoop && BoundsMatchViewBounds(loop.bounds, viewBounds, connectTolerance))
            {
                ++skippedOuter;
                continue;
            }

            std::ostringstream signature;
            signature << static_cast<long long>(std::llround(width * 100.0))
                << "x"
                << static_cast<long long>(std::llround(height * 100.0));
            if (createdSignatures.find(signature.str()) != createdSignatures.end())
            {
                ++skippedDuplicate;
                continue;
            }
            createdSignatures.insert(signature.str());
            loops.push_back(loop);
        }
    }

    std::sort(loops.begin(), loops.end(), [](const ClosedCurveLoopCandidate& a, const ClosedCurveLoopCandidate& b) {
        return BoundsArea(a.bounds) > BoundsArea(b.bounds);
    });

    std::vector<ClosedCurveLoopCandidate> filteredLoops;
    int skippedOverlapping = 0;
    for (const ClosedCurveLoopCandidate& loop : loops)
    {
        bool covered = false;
        for (const ClosedCurveLoopCandidate& kept : filteredLoops)
        {
            if (ClosedLoopCoveredByKeptLoop(loop, kept, connectTolerance))
            {
                covered = true;
                break;
            }
        }
        if (covered)
        {
            ++skippedOverlapping;
            continue;
        }
        filteredLoops.push_back(loop);
    }

    std::ostringstream log;
    log << "AutoCreateThreeViews: " << label
        << " closed curve loops"
        << " segments=" << segments.size()
        << ", loops=" << filteredLoops.size()
        << ", skippedOuter=" << skippedOuter
        << ", skippedDuplicate=" << skippedDuplicate
        << ", skippedOverlapping=" << skippedOverlapping
        << ", extractedBoundaryLoops=" << extractedBoundaryLoops
        << ", skippedShallowDetailCurves=" << skippedShallowDetailCurves << ".";
    WriteLine(session, log.str());
    return filteredLoops;
}

void HighlightClosedCurveLoop(
    NXOpen::Session* session,
    NXOpen::Part* part,
    const ClosedCurveLoopCandidate& loop,
    const std::string& label)
{
    if (session == nullptr || part == nullptr || loop.extents.empty())
    {
        return;
    }

    std::vector<NXOpen::DisplayableObject*> curvesToColor;
    std::set<tag_t> addedCurves;
    for (const DraftingCurveExtent& extent : loop.extents)
    {
        if (extent.curve == nullptr || extent.tag == NULL_TAG || addedCurves.find(extent.tag) != addedCurves.end())
        {
            continue;
        }
        NXOpen::DisplayableObject* displayableCurve =
            dynamic_cast<NXOpen::DisplayableObject*>(extent.curve);
        if (displayableCurve == nullptr)
        {
            continue;
        }
        curvesToColor.push_back(displayableCurve);
        addedCurves.insert(extent.tag);
    }

    if (curvesToColor.empty())
    {
        WriteLine(session, "AutoCreateThreeViews: " + label + " outer contour highlight skipped; no displayable curves.");
        return;
    }

    try
    {
        NXOpen::DisplayModification* modification = session->DisplayManager()->NewDisplayModification();
        modification->SetApplyToAllFaces(false);
        modification->SetApplyToOwningParts(false);
        modification->SetNewColor(FindYellowColorIndex(part));
        modification->Apply(curvesToColor);
        delete modification;
        session->DisplayManager()->MakeUpToDate();

        std::ostringstream log;
        log << "AutoCreateThreeViews: " << label
            << " outer contour highlighted"
            << " curves=" << curvesToColor.size() << ".";
        WriteLine(session, log.str());
    }
    catch (const NXOpen::NXException& ex)
    {
        WriteLine(session, std::string("AutoCreateThreeViews: ") + label + " outer contour highlight failed, NX " + std::to_string(ex.ErrorCode()) + ": " + ex.Message());
    }
    catch (...)
    {
        WriteLine(session, "AutoCreateThreeViews: " + label + " outer contour highlight failed, unknown exception.");
    }
}

void DrawClosedCurveLoopOverlay(
    NXOpen::Session* session,
    NXOpen::Part* part,
    NXOpen::Drawings::DraftingView* view,
    const ClosedCurveLoopCandidate& loop,
    const std::string& label)
{
    if (session == nullptr || part == nullptr || view == nullptr || loop.extents.empty())
    {
        return;
    }

    const int yellowColor = FindYellowColorIndex(part);
    int createdSegments = 0;
    int skippedCurves = 0;
    std::set<tag_t> addedCurves;

    for (const DraftingCurveExtent& extent : loop.extents)
    {
        if (extent.tag == NULL_TAG || addedCurves.find(extent.tag) != addedCurves.end())
        {
            continue;
        }
        addedCurves.insert(extent.tag);

        std::vector<NXOpen::Point3d> samplePoints;
        if (!GetCurveDrawingSamplePoints(view, extent, samplePoints))
        {
            ++skippedCurves;
            continue;
        }

        for (size_t i = 1; i < samplePoints.size(); ++i)
        {
            const NXOpen::Point3d& start = samplePoints[i - 1];
            const NXOpen::Point3d& end = samplePoints[i];
            if (ClosedCurvePointDistance2d(start.X, start.Y, end.X, end.Y) <= 1.0e-4)
            {
                continue;
            }

            UF_CURVE_line_t lineData{};
            lineData.start_point[0] = start.X;
            lineData.start_point[1] = start.Y;
            lineData.start_point[2] = 0.0;
            lineData.end_point[0] = end.X;
            lineData.end_point[1] = end.Y;
            lineData.end_point[2] = 0.0;

            tag_t lineTag = NULL_TAG;
            if (UF_CURVE_create_line(&lineData, &lineTag) != 0 || lineTag == NULL_TAG)
            {
                continue;
            }
            UF_OBJ_set_color(lineTag, yellowColor);
            UF_OBJ_set_font(lineTag, UF_OBJ_FONT_SOLID);
            UF_OBJ_set_line_width(lineTag, UF_OBJ_WIDTH_THICK);
            ++createdSegments;
        }
    }

    std::ostringstream log;
    log << "AutoCreateThreeViews: " << label
        << " outer contour overlay curves"
        << " segments=" << createdSegments
        << ", sourceCurves=" << addedCurves.size()
        << ", skippedCurves=" << skippedCurves << ".";
    WriteLine(session, log.str());
}

bool TryFindLargestOuterContourBounds(
    NXOpen::Session* session,
    NXOpen::Part* workPart,
    ShallowDetailFilterCache& shallowCache,
    NXOpen::Drawings::DraftingView* view,
    const std::vector<DraftingCurveExtent>& extents,
    const LayoutBounds& viewBounds,
    const std::string& label,
    LayoutBounds& outerBounds)
{
    ClosedCurveLoopCandidate outerLoop;
    double outerArea = 0.0;
    if (!TryFindOuterContourLoopByMaxArea(
            session,
            workPart,
            shallowCache,
            view,
            extents,
            viewBounds,
            label,
            outerLoop,
            outerArea))
    {
        WriteLine(session, "AutoCreateThreeViews: " + label + " max-area outer contour not found; using view bounds for overall dimensions.");
        return false;
    }

    outerBounds = outerLoop.bounds;
    std::ostringstream log;
    log << "AutoCreateThreeViews: " << label
        << " outer contour bounds"
        << " minX=" << outerBounds.minX
        << ", minY=" << outerBounds.minY
        << ", maxX=" << outerBounds.maxX
        << ", maxY=" << outerBounds.maxY
        << ", width=" << BoundsWidth(outerBounds)
        << ", height=" << BoundsHeight(outerBounds)
        << ", maxArea=" << outerArea << ".";
    WriteLine(session, log.str());
    return true;
}

bool TryFindOverallVisibleCurveBounds(
    NXOpen::Session* session,
    NXOpen::Part* workPart,
    ShallowDetailFilterCache& shallowCache,
    const std::vector<DraftingCurveExtent>& extents,
    const std::string& label,
    LayoutBounds& outerBounds,
    std::vector<DraftingCurveExtent>* outerExtents = nullptr)
{
    bool initialized = false;
    int used = 0;
    int skippedType = 0;
    int skippedShallow = 0;
    int skippedInvalid = 0;
    if (outerExtents != nullptr)
    {
        outerExtents->clear();
    }

    for (const DraftingCurveExtent& extent : extents)
    {
        if (!extent.initialized || extent.curve == nullptr || extent.tag == NULL_TAG)
        {
            ++skippedInvalid;
            continue;
        }
        if (!IsOuterContourChainCurve(extent.curve))
        {
            ++skippedType;
            continue;
        }
        if (DraftingCurveHasShallowDetailParent(workPart, shallowCache, extent))
        {
            ++skippedShallow;
            continue;
        }

        ExpandBounds(outerBounds, initialized, extent.bounds);
        if (outerExtents != nullptr)
        {
            outerExtents->push_back(extent);
        }
        ++used;
    }

    if (!initialized)
    {
        std::ostringstream log;
        log << "AutoCreateThreeViews: " << label
            << " visible outer curve bounds not found"
            << " curves=" << extents.size()
            << ", skippedInvalid=" << skippedInvalid
            << ", skippedType=" << skippedType
            << ", skippedShallow=" << skippedShallow << ".";
        WriteLine(session, log.str());
        return false;
    }

    std::ostringstream log;
    log << "AutoCreateThreeViews: " << label
        << " visible outer curve bounds"
        << " minX=" << outerBounds.minX
        << ", minY=" << outerBounds.minY
        << ", maxX=" << outerBounds.maxX
        << ", maxY=" << outerBounds.maxY
        << ", width=" << BoundsWidth(outerBounds)
        << ", height=" << BoundsHeight(outerBounds)
        << ", used=" << used
        << ", skippedInvalid=" << skippedInvalid
        << ", skippedType=" << skippedType
        << ", skippedShallow=" << skippedShallow << ".";
    WriteLine(session, log.str());
    return true;
}

bool TryFindClosedCurveFacePair(
    const std::vector<LineProjectionFaceCandidate>& faceCandidates,
    const ClosedCurveLoopCandidate& loop,
    bool horizontalMeasurement,
    double axisTolerance,
    FacePairKey& pairKey,
    std::vector<LineProjectionFaceCandidate>* pairFaces = nullptr);

bool CreateFacePairDimension(
    NXOpen::Session* session,
    NXOpen::Part* workPart,
    NXOpen::Drawings::DraftingView* view,
    const LineProjectionFaceCandidate& first,
    const LineProjectionFaceCandidate& second,
    NXOpen::Annotations::DimensionMeasurementBuilder::MeasurementMethod method,
    const NXOpen::Point3d& origin,
    const std::string& label);

bool ContainsFacePairKey(const std::vector<FacePairKey>& keys, const FacePairKey& key);

void CreateClosedCurveMaxDimensions(
    NXOpen::Session* session,
    NXOpen::Part* workPart,
    ShallowDetailFilterCache& shallowCache,
    NXOpen::Drawings::DraftingView* view,
    const std::string& label,
    const std::vector<DraftingCurveExtent>& extents,
    const std::vector<LineProjectionFaceCandidate>& faceCandidates,
    const LayoutBounds& viewBounds,
    double offset,
    std::vector<FacePairKey>& usedFacePairs,
    std::vector<ClosedCurveDimensionRecord>& closedCurveDimensionRecords)
{
    if (workPart == nullptr || view == nullptr)
    {
        return;
    }

    const std::vector<ClosedCurveLoopCandidate> loops =
        CollectClosedCurveLoops(session, workPart, shallowCache, view, extents, viewBounds, label);
    const double loopOffset = std::max(5.0, offset * 0.6);
    int createdHorizontal = 0;
    int createdVertical = 0;

    for (const ClosedCurveLoopCandidate& loop : loops)
    {
        const double width = BoundsWidth(loop.bounds);
        const double height = BoundsHeight(loop.bounds);
        const double minDimensionSize = std::max(0.2, std::max(BoundsWidth(viewBounds), BoundsHeight(viewBounds)) * 0.004);
        FacePairKey horizontalPairKey;
        std::vector<LineProjectionFaceCandidate> horizontalFaces;
        const bool hasHorizontalFaces =
            TryFindClosedCurveFacePair(faceCandidates, loop, true, minDimensionSize, horizontalPairKey, &horizontalFaces);
        const bool horizontalPairAlreadyDimensioned =
            hasHorizontalFaces && ContainsFacePairKey(usedFacePairs, horizontalPairKey);
        if (width > minDimensionSize &&
            hasHorizontalFaces &&
            !horizontalPairAlreadyDimensioned &&
            horizontalFaces.size() >= 2 &&
            CreateFacePairDimension(
                session,
                workPart,
                view,
                horizontalFaces[0],
                horizontalFaces[1],
                NXOpen::Annotations::DimensionMeasurementBuilder::MeasurementMethodHorizontal,
                NXOpen::Point3d((loop.bounds.minX + loop.bounds.maxX) * 0.5, loop.bounds.maxY + loopOffset, 0.0),
                label + " closed curve horizontal"))
        {
            ClosedCurveDimensionRecord record;
            record.measuresX = true;
            record.minMeasure = loop.bounds.minX;
            record.maxMeasure = loop.bounds.maxX;
            record.minCross = loop.bounds.minY;
            record.maxCross = loop.bounds.maxY;
            closedCurveDimensionRecords.push_back(record);
            usedFacePairs.push_back(horizontalPairKey);
            ++createdHorizontal;
        }
        else if (width > minDimensionSize && horizontalPairAlreadyDimensioned)
        {
            WriteLine(session, "AutoCreateThreeViews: " + label + " closed curve horizontal skipped; face pair already dimensioned.");
        }
        else if (width > minDimensionSize)
        {
            WriteLine(session, "AutoCreateThreeViews: " + label + " closed curve horizontal skipped; no perpendicular face pair or face dimension failed.");
        }

        FacePairKey verticalPairKey;
        std::vector<LineProjectionFaceCandidate> verticalFaces;
        const bool hasVerticalFaces =
            TryFindClosedCurveFacePair(faceCandidates, loop, false, minDimensionSize, verticalPairKey, &verticalFaces);
        const bool verticalPairAlreadyDimensioned =
            hasVerticalFaces && ContainsFacePairKey(usedFacePairs, verticalPairKey);
        if (height > minDimensionSize &&
            hasVerticalFaces &&
            !verticalPairAlreadyDimensioned &&
            verticalFaces.size() >= 2 &&
            CreateFacePairDimension(
                session,
                workPart,
                view,
                verticalFaces[0],
                verticalFaces[1],
                NXOpen::Annotations::DimensionMeasurementBuilder::MeasurementMethodVertical,
                NXOpen::Point3d(loop.bounds.minX - loopOffset, (loop.bounds.minY + loop.bounds.maxY) * 0.5, 0.0),
                label + " closed curve vertical"))
        {
            ClosedCurveDimensionRecord record;
            record.measuresX = false;
            record.minMeasure = loop.bounds.minY;
            record.maxMeasure = loop.bounds.maxY;
            record.minCross = loop.bounds.minX;
            record.maxCross = loop.bounds.maxX;
            closedCurveDimensionRecords.push_back(record);
            usedFacePairs.push_back(verticalPairKey);
            ++createdVertical;
        }
        else if (height > minDimensionSize && verticalPairAlreadyDimensioned)
        {
            WriteLine(session, "AutoCreateThreeViews: " + label + " closed curve vertical skipped; face pair already dimensioned.");
        }
        else if (height > minDimensionSize)
        {
            WriteLine(session, "AutoCreateThreeViews: " + label + " closed curve vertical skipped; no perpendicular face pair or face dimension failed.");
        }
    }

    std::ostringstream log;
    log << "AutoCreateThreeViews: " << label
        << " closed curve max dimensions"
        << " loops=" << loops.size()
        << ", horizontal=" << createdHorizontal
        << ", vertical=" << createdVertical << ".";
    WriteLine(session, log.str());
}

bool CentersAreClose(const NXOpen::Point3d& a, const NXOpen::Point3d& b, double tolerance)
{
    return std::abs(a.X - b.X) <= tolerance && std::abs(a.Y - b.Y) <= tolerance;
}

double NormalizeAngleRadians(double angle)
{
    const double twoPi = 6.28318530717958647692;
    double normalized = std::fmod(angle, twoPi);
    if (normalized < 0.0)
    {
        normalized += twoPi;
    }
    return normalized;
}

double ArcCoverageRadians(double startAngle, double endAngle)
{
    const double twoPi = 6.28318530717958647692;
    double span = endAngle - startAngle;
    while (span < 0.0)
    {
        span += twoPi;
    }
    while (span > twoPi)
    {
        span -= twoPi;
    }
    return span;
}

bool ArcSegmentsCoverFullCircle(const std::vector<HoleArcSegmentCandidate>& segments)
{
    if (segments.empty())
    {
        return false;
    }

    const double twoPi = 6.28318530717958647692;
    const double tolerance = 0.035;
    std::vector<std::pair<double, double>> intervals;
    for (const HoleArcSegmentCandidate& segment : segments)
    {
        const double span = ArcCoverageRadians(segment.startAngle, segment.endAngle);
        if (span >= twoPi - tolerance)
        {
            return true;
        }
        if (span <= tolerance)
        {
            continue;
        }

        const double start = NormalizeAngleRadians(segment.startAngle);
        double end = start + span;
        if (end <= twoPi + tolerance)
        {
            intervals.push_back({start, std::min(end, twoPi)});
        }
        else
        {
            intervals.push_back({start, twoPi});
            intervals.push_back({0.0, end - twoPi});
        }
    }

    if (intervals.empty())
    {
        return false;
    }

    std::sort(intervals.begin(), intervals.end(), [](const auto& a, const auto& b) {
        if (std::abs(a.first - b.first) > 1.0e-9)
        {
            return a.first < b.first;
        }
        return a.second < b.second;
    });

    double coveredStart = intervals.front().first;
    double coveredEnd = intervals.front().second;
    double coveredTotal = 0.0;
    for (size_t index = 1; index < intervals.size(); ++index)
    {
        if (intervals[index].first <= coveredEnd + tolerance)
        {
            coveredEnd = std::max(coveredEnd, intervals[index].second);
        }
        else
        {
            coveredTotal += std::max(0.0, coveredEnd - coveredStart);
            coveredStart = intervals[index].first;
            coveredEnd = intervals[index].second;
        }
    }
    coveredTotal += std::max(0.0, coveredEnd - coveredStart);
    return coveredTotal >= twoPi - tolerance;
}

std::vector<HoleCircleCandidate> CollectHoleCircleCandidates(
    NXOpen::Session* session,
    NXOpen::Part* workPart,
    ShallowDetailFilterCache& shallowCache,
    NXOpen::Drawings::DraftingView* view,
    const std::vector<DraftingCurveExtent>& extents,
    const LayoutBounds& bounds)
{
    std::vector<HoleCircleCandidate> holes;
    if (view == nullptr)
    {
        return holes;
    }

    int skippedInvalid = 0;
    int skippedEval = 0;
    int skippedNotArc = 0;
    int skippedNotFullCircle = 0;
    int skippedMap = 0;
    int skippedRadius = 0;
    int skippedOutside = 0;
    int skippedDuplicate = 0;
    std::vector<HoleArcSegmentCandidate> arcSegments;
    for (const DraftingCurveExtent& extent : extents)
    {
        if (!extent.initialized || extent.curve == nullptr || extent.tag == NULL_TAG)
        {
            ++skippedInvalid;
            continue;
        }

        UF_EVAL_p_t evaluator = nullptr;
        if (UF_EVAL_initialize(extent.tag, &evaluator) != 0 || evaluator == nullptr)
        {
            ++skippedEval;
            continue;
        }

        bool isArc = false;
        if (UF_EVAL_is_arc(evaluator, &isArc) != 0 || !isArc)
        {
            UF_EVAL_free(evaluator);
            ++skippedNotArc;
            continue;
        }

        UF_EVAL_arc_t arcData{};
        if (UF_EVAL_ask_arc(evaluator, &arcData) != 0 || arcData.radius <= 0.0)
        {
            UF_EVAL_free(evaluator);
            ++skippedNotFullCircle;
            continue;
        }

        double centerDrawing[2] = {0.0, 0.0};
        double pickModel[3] = {
            arcData.center[0] + arcData.radius * arcData.x_axis[0],
            arcData.center[1] + arcData.radius * arcData.x_axis[1],
            arcData.center[2] + arcData.radius * arcData.x_axis[2]};
        double pickDrawing[2] = {0.0, 0.0};
        const bool mappedCenter = UF_VIEW_map_model_to_drawing(view->Tag(), arcData.center, centerDrawing) == 0;
        const bool mappedPick = UF_VIEW_map_model_to_drawing(view->Tag(), pickModel, pickDrawing) == 0;
        UF_EVAL_free(evaluator);
        if (!mappedCenter || !mappedPick)
        {
            ++skippedMap;
            continue;
        }

        const double drawingRadius = std::sqrt(
            (pickDrawing[0] - centerDrawing[0]) * (pickDrawing[0] - centerDrawing[0]) +
            (pickDrawing[1] - centerDrawing[1]) * (pickDrawing[1] - centerDrawing[1]));
        if (drawingRadius <= 0.0)
        {
            ++skippedRadius;
            continue;
        }

        const double inset = std::max(0.1, drawingRadius * 0.5);
        if (centerDrawing[0] <= bounds.minX + inset ||
            centerDrawing[0] >= bounds.maxX - inset ||
            centerDrawing[1] <= bounds.minY + inset ||
            centerDrawing[1] >= bounds.maxY - inset)
        {
            ++skippedOutside;
            continue;
        }

        HoleArcSegmentCandidate segment;
        segment.extent = extent;
        segment.modelRadius = arcData.radius;
        segment.drawingRadius = drawingRadius;
        segment.centerPoint = NXOpen::Point3d(centerDrawing[0], centerDrawing[1], 0.0);
        segment.pickPoint = NXOpen::Point3d(pickDrawing[0], pickDrawing[1], 0.0);
        segment.startAngle = arcData.limits[0];
        segment.endAngle = arcData.limits[1];
        arcSegments.push_back(segment);
    }

    std::vector<bool> consumed(arcSegments.size(), false);
    int arcGroups = 0;
    int fullCircleGroups = 0;
    for (size_t index = 0; index < arcSegments.size(); ++index)
    {
        if (consumed[index])
        {
            continue;
        }

        std::vector<size_t> groupIndexes;
        std::vector<HoleArcSegmentCandidate> groupSegments;
        for (size_t candidateIndex = index; candidateIndex < arcSegments.size(); ++candidateIndex)
        {
            if (consumed[candidateIndex])
            {
                continue;
            }
            const double radiusTolerance = std::max(0.02, arcSegments[index].modelRadius * 0.002);
            const double centerTolerance = std::max(0.1, arcSegments[index].drawingRadius * 0.25);
            if (std::abs(arcSegments[candidateIndex].modelRadius - arcSegments[index].modelRadius) <= radiusTolerance &&
                CentersAreClose(arcSegments[candidateIndex].centerPoint, arcSegments[index].centerPoint, centerTolerance))
            {
                groupIndexes.push_back(candidateIndex);
                groupSegments.push_back(arcSegments[candidateIndex]);
            }
        }

        for (size_t groupIndex : groupIndexes)
        {
            consumed[groupIndex] = true;
        }
        ++arcGroups;
        if (!ArcSegmentsCoverFullCircle(groupSegments))
        {
            ++skippedNotFullCircle;
            continue;
        }
        ++fullCircleGroups;

        const HoleArcSegmentCandidate* representative = &groupSegments.front();
        for (const HoleArcSegmentCandidate& segment : groupSegments)
        {
            if (segment.centerPoint.X < representative->centerPoint.X ||
                (std::abs(segment.centerPoint.X - representative->centerPoint.X) <= 0.01 &&
                 segment.centerPoint.Y > representative->centerPoint.Y))
            {
                representative = &segment;
            }
        }

        bool duplicate = false;
        for (const HoleCircleCandidate& existing : holes)
        {
            if (std::abs(existing.modelRadius - representative->modelRadius) <= 0.02 &&
                CentersAreClose(existing.centerPoint, representative->centerPoint, std::max(0.1, representative->drawingRadius * 0.25)))
            {
                duplicate = true;
                break;
            }
        }
        if (duplicate)
        {
            ++skippedDuplicate;
            continue;
        }

        HoleCircleCandidate hole;
        hole.curve = representative->extent.curve;
        hole.modelRadius = representative->modelRadius;
        hole.drawingRadius = representative->drawingRadius;
        hole.centerPoint = representative->centerPoint;
        hole.pickPoint = representative->pickPoint;
        holes.push_back(hole);
    }

    std::ostringstream log;
    log << "AutoCreateThreeViews: hole candidates from drafting view"
        << ", extents=" << extents.size()
        << ", arcSegments=" << arcSegments.size()
        << ", arcGroups=" << arcGroups
        << ", fullCircleGroups=" << fullCircleGroups
        << ", count=" << holes.size()
        << ", skippedInvalid=" << skippedInvalid
        << ", skippedEval=" << skippedEval
        << ", skippedNotArc=" << skippedNotArc
        << ", skippedNotFullCircle=" << skippedNotFullCircle
        << ", skippedMap=" << skippedMap
        << ", skippedRadius=" << skippedRadius
        << ", skippedOutside=" << skippedOutside
        << ", skippedDuplicate=" << skippedDuplicate << ".";
    WriteLine(session, log.str());
    return holes;
}

bool CreateHoleDiameterDimension(
    NXOpen::Session* session,
    NXOpen::Part* workPart,
    NXOpen::Drawings::DraftingView* view,
    const HoleCircleCandidate& hole,
    int groupCount,
    double originOffset,
    const std::string& beforeText)
{
    if (workPart == nullptr || view == nullptr || hole.curve == nullptr)
    {
        return false;
    }

    NXOpen::Annotations::Dimension* nullDimension = nullptr;
    NXOpen::Annotations::RadialDimensionBuilder* builder =
        workPart->Dimensions()->CreateRadialDimensionBuilder(nullDimension);
    if (builder == nullptr)
    {
        WriteLine(session, "AutoCreateThreeViews: hole diameter dimension failed; builder not available.");
        return false;
    }

    try
    {
        NXOpen::View* nullView = nullptr;
        builder->FirstAssociativity()->SetValue(hole.curve, view, hole.pickPoint);
        builder->SetHoleStyle(true);
        builder->SetRadiusToCenter(false);
        builder->Style()->DimensionStyle()->SetNarrowDisplayType(NXOpen::Annotations::NarrowDisplayOptionNone);
        if (!beforeText.empty())
        {
            std::vector<NXOpen::NXString> beforeLines;
            beforeLines.emplace_back(beforeText.c_str(), NXOpen::NXString::UTF8);
            builder->AppendedText()->SetBefore(beforeLines);
        }
        builder->Origin()->Origin()->SetValue(
            nullptr,
            nullView,
            NXOpen::Point3d(hole.centerPoint.X - originOffset * 1.2, hole.centerPoint.Y + originOffset * 0.9, 0.0));
        NXOpen::NXObject* object = builder->Commit();
        builder->Destroy();

        std::ostringstream log;
        log << "AutoCreateThreeViews: hole diameter dimension created"
            << " radius=" << hole.modelRadius
            << ", drawingRadius=" << hole.drawingRadius
            << ", groupCount=" << groupCount
            << ", beforeText=" << beforeText
            << ", centerX=" << hole.centerPoint.X
            << ", centerY=" << hole.centerPoint.Y
            << ".";
        WriteLine(session, log.str());
        return object != nullptr;
    }
    catch (const NXOpen::NXException& ex)
    {
        builder->Destroy();
        WriteLine(session, std::string("AutoCreateThreeViews: hole diameter dimension failed, NXException: ") + ex.Message() + ".");
        return false;
    }
    catch (...)
    {
        builder->Destroy();
        WriteLine(session, "AutoCreateThreeViews: hole diameter dimension failed, unknown exception.");
        return false;
    }
}

void CreateFrontHoleDiameterDimensions(
    NXOpen::Session* session,
    NXOpen::Part* workPart,
    ShallowDetailFilterCache& shallowCache,
    NXOpen::Drawings::DraftingView* view,
    const std::vector<DraftingCurveExtent>& extents,
    const LayoutBounds& bounds,
    double offset,
    std::vector<double>& annotatedTappedHoleDiameters)
{
    std::vector<HoleCircleCandidate> holes = CollectHoleCircleCandidates(session, workPart, shallowCache, view, extents, bounds);
    if (holes.empty())
    {
        WriteLine(session, "AutoCreateThreeViews: hole diameter dimensions skipped; no circular hole curves found.");
        return;
    }

    std::sort(holes.begin(), holes.end(), [](const HoleCircleCandidate& a, const HoleCircleCandidate& b) {
        if (std::abs(a.modelRadius - b.modelRadius) > 0.02)
        {
            return a.modelRadius < b.modelRadius;
        }
        if (std::abs(a.centerPoint.Y - b.centerPoint.Y) > 0.01)
        {
            return a.centerPoint.Y > b.centerPoint.Y;
        }
        return a.centerPoint.X < b.centerPoint.X;
    });

    const double radiusTolerance = 0.05;
    size_t index = 0;
    int createdCount = 0;
    int tappedCount = 0;
    int plainCount = 0;
    int skippedRepeatedCount = 0;
    while (index < holes.size())
    {
        const double radius = holes[index].modelRadius;
        const double diameter = radius * 2.0;
        size_t next = index + 1;
        while (next < holes.size() && std::abs(holes[next].modelRadius - radius) <= radiusTolerance)
        {
            ++next;
        }

        int groupCount = static_cast<int>(next - index);
        const HoleCircleCandidate* representative = &holes[index];
        for (size_t i = index + 1; i < next; ++i)
        {
            if (holes[i].centerPoint.X < representative->centerPoint.X ||
                (std::abs(holes[i].centerPoint.X - representative->centerPoint.X) <= 0.01 &&
                 holes[i].centerPoint.Y > representative->centerPoint.Y))
            {
                representative = &holes[i];
            }
        }

        std::ostringstream log;
        log << "AutoCreateThreeViews: hole group"
            << " radius=" << radius
            << ", diameter=" << diameter
            << ", count=" << groupCount
            << ", representativeCenterX=" << representative->centerPoint.X
            << ", representativeCenterY=" << representative->centerPoint.Y << ".";
        WriteLine(session, log.str());

        bool alreadyAnnotated = false;
        for (const double annotatedDiameter : annotatedTappedHoleDiameters)
        {
            if (std::abs(annotatedDiameter - diameter) <= 0.02)
            {
                alreadyAnnotated = true;
                break;
            }
        }
        if (alreadyAnnotated)
        {
            ++skippedRepeatedCount;
            index = next;
            continue;
        }

        bool tapped = false;
        const std::string beforeText = BuildHoleDiameterBeforeText(session, diameter, groupCount, tapped);
        if (CreateHoleDiameterDimension(session, workPart, view, *representative, groupCount, offset, beforeText))
        {
            ++createdCount;
            annotatedTappedHoleDiameters.push_back(diameter);
            if (tapped)
            {
                ++tappedCount;
            }
            else
            {
                ++plainCount;
            }
        }
        index = next;
    }

    std::ostringstream log;
    log << "AutoCreateThreeViews: hole diameter dimensions finished"
        << ", createdGroups=" << createdCount
        << ", tappedGroups=" << tappedCount
        << ", plainGroups=" << plainCount
        << ", skippedRepeatedGroups=" << skippedRepeatedCount << ".";
    WriteLine(session, log.str());
}

bool CreateHoleCenterDimension(
    NXOpen::Session* session,
    NXOpen::Part* workPart,
    NXOpen::Drawings::DraftingView* view,
    const HoleCircleCandidate& first,
    const HoleCircleCandidate& second,
    bool horizontal,
    const NXOpen::Point3d& origin,
    const std::string& logLabel)
{
    if (workPart == nullptr || view == nullptr || first.curve == nullptr || second.curve == nullptr)
    {
        return false;
    }
    const double drawingDistance = horizontal
        ? std::abs(first.centerPoint.X - second.centerPoint.X)
        : std::abs(first.centerPoint.Y - second.centerPoint.Y);
    if (IsZeroDimensionValue(drawingDistance))
    {
        WriteLine(session, "AutoCreateThreeViews: hole " + logLabel + " dimension skipped; dimension value is zero.");
        return false;
    }

    NXOpen::View* nullView = nullptr;
    NXOpen::Point3d assistPoint(0.0, 0.0, 0.0);
    NXOpen::Annotations::Dimension* nullDimension = nullptr;
    NXOpen::Annotations::RapidDimensionBuilder* builder = workPart->Dimensions()->CreateRapidDimensionBuilder(nullDimension);
    if (builder == nullptr)
    {
        WriteLine(session, "AutoCreateThreeViews: hole center dimension failed; builder not available.");
        return false;
    }

    try
    {
        builder->FirstAssociativity()->SetValue(
            NXOpen::InferSnapType::SnapTypeCenter,
            first.curve,
            view,
            first.centerPoint,
            nullptr,
            nullView,
            assistPoint);
        builder->SecondAssociativity()->SetValue(
            NXOpen::InferSnapType::SnapTypeCenter,
            second.curve,
            view,
            second.centerPoint,
            nullptr,
            nullView,
            assistPoint);
        builder->Style()->DimensionStyle()->SetTextCentered(true);
        builder->Style()->DimensionStyle()->SetNarrowDisplayType(NXOpen::Annotations::NarrowDisplayOptionNone);
        builder->Measurement()->SetMethod(horizontal
            ? NXOpen::Annotations::DimensionMeasurementBuilder::MeasurementMethodHorizontal
            : NXOpen::Annotations::DimensionMeasurementBuilder::MeasurementMethodVertical);
        builder->Origin()->Origin()->SetValue(nullptr, nullView, origin);
        NXOpen::NXObject* object = builder->Commit();
        builder->Destroy();

        std::ostringstream log;
        log << "AutoCreateThreeViews: hole " << logLabel << " dimension created"
            << " method=" << (horizontal ? "horizontal" : "vertical")
            << ", firstCenter=(" << first.centerPoint.X << "," << first.centerPoint.Y << ")"
            << ", secondCenter=(" << second.centerPoint.X << "," << second.centerPoint.Y << ").";
        WriteLine(session, log.str());
        return object != nullptr;
    }
    catch (const NXOpen::NXException& ex)
    {
        builder->Destroy();
        WriteLine(session, std::string("AutoCreateThreeViews: hole ") + logLabel + " dimension failed, NXException: " + ex.Message() + ".");
        return false;
    }
    catch (...)
    {
        builder->Destroy();
        WriteLine(session, std::string("AutoCreateThreeViews: hole ") + logLabel + " dimension failed, unknown exception.");
        return false;
    }
}

bool CreateHoleBoundaryDimension(
    NXOpen::Session* session,
    NXOpen::Part* workPart,
    NXOpen::Drawings::DraftingView* view,
    const CurveAssocCandidate& boundary,
    const HoleCircleCandidate& hole,
    bool horizontal,
    const NXOpen::Point3d& origin,
    const std::string& logLabel)
{
    if (workPart == nullptr || view == nullptr || boundary.curve == nullptr || hole.curve == nullptr)
    {
        return false;
    }
    double boundaryDrawing[2] = {boundary.modelPoint.X, boundary.modelPoint.Y};
    double boundaryModel[3] = {boundary.modelPoint.X, boundary.modelPoint.Y, boundary.modelPoint.Z};
    if (view->Tag() != NULL_TAG)
    {
        UF_VIEW_map_model_to_drawing(view->Tag(), boundaryModel, boundaryDrawing);
    }
    const double drawingDistance = horizontal
        ? std::abs(boundaryDrawing[0] - hole.centerPoint.X)
        : std::abs(boundaryDrawing[1] - hole.centerPoint.Y);
    if (IsZeroDimensionValue(drawingDistance))
    {
        WriteLine(session, "AutoCreateThreeViews: hole " + logLabel + " boundary dimension skipped; dimension value is zero.");
        return false;
    }

    NXOpen::View* nullView = nullptr;
    NXOpen::Point3d assistPoint(0.0, 0.0, 0.0);
    NXOpen::Annotations::Dimension* nullDimension = nullptr;
    NXOpen::Annotations::RapidDimensionBuilder* builder = workPart->Dimensions()->CreateRapidDimensionBuilder(nullDimension);
    if (builder == nullptr)
    {
        WriteLine(session, "AutoCreateThreeViews: hole boundary dimension failed; builder not available.");
        return false;
    }

    try
    {
        builder->FirstAssociativity()->SetValue(
            boundary.snapType,
            boundary.curve,
            view,
            boundary.modelPoint,
            nullptr,
            nullView,
            assistPoint);
        builder->SecondAssociativity()->SetValue(
            NXOpen::InferSnapType::SnapTypeCenter,
            hole.curve,
            view,
            hole.centerPoint,
            nullptr,
            nullView,
            assistPoint);
        builder->Style()->DimensionStyle()->SetTextCentered(true);
        builder->Style()->DimensionStyle()->SetNarrowDisplayType(NXOpen::Annotations::NarrowDisplayOptionNone);
        builder->Measurement()->SetMethod(horizontal
            ? NXOpen::Annotations::DimensionMeasurementBuilder::MeasurementMethodHorizontal
            : NXOpen::Annotations::DimensionMeasurementBuilder::MeasurementMethodVertical);
        builder->Origin()->Origin()->SetValue(nullptr, nullView, origin);
        NXOpen::NXObject* object = builder->Commit();
        builder->Destroy();

        std::ostringstream log;
        log << "AutoCreateThreeViews: hole " << logLabel << " boundary dimension created"
            << " method=" << (horizontal ? "horizontal" : "vertical")
            << ", holeCenter=(" << hole.centerPoint.X << "," << hole.centerPoint.Y << ").";
        WriteLine(session, log.str());
        return object != nullptr;
    }
    catch (const NXOpen::NXException& ex)
    {
        builder->Destroy();
        WriteLine(session, std::string("AutoCreateThreeViews: hole ") + logLabel + " boundary dimension failed, NXException: " + ex.Message() + ".");
        return false;
    }
    catch (...)
    {
        builder->Destroy();
        WriteLine(session, std::string("AutoCreateThreeViews: hole ") + logLabel + " boundary dimension failed, unknown exception.");
        return false;
    }
}

std::vector<std::vector<HoleCircleCandidate>> GroupHolesByAxis(
    const std::vector<HoleCircleCandidate>& holes,
    bool groupRows,
    double tolerance)
{
    std::vector<HoleCircleCandidate> sorted = holes;
    std::sort(sorted.begin(), sorted.end(), [groupRows](const HoleCircleCandidate& a, const HoleCircleCandidate& b) {
        const double primaryA = groupRows ? a.centerPoint.Y : a.centerPoint.X;
        const double primaryB = groupRows ? b.centerPoint.Y : b.centerPoint.X;
        if (std::abs(primaryA - primaryB) > 0.01)
        {
            return groupRows ? primaryA > primaryB : primaryA < primaryB;
        }
        return groupRows ? a.centerPoint.X < b.centerPoint.X : a.centerPoint.Y > b.centerPoint.Y;
    });

    std::vector<std::vector<HoleCircleCandidate>> groups;
    for (const HoleCircleCandidate& hole : sorted)
    {
        bool added = false;
        for (std::vector<HoleCircleCandidate>& group : groups)
        {
            const double groupAxis = groupRows ? group.front().centerPoint.Y : group.front().centerPoint.X;
            const double holeAxis = groupRows ? hole.centerPoint.Y : hole.centerPoint.X;
            if (std::abs(groupAxis - holeAxis) <= tolerance)
            {
                group.push_back(hole);
                added = true;
                break;
            }
        }
        if (!added)
        {
            groups.push_back({hole});
        }
    }

    for (std::vector<HoleCircleCandidate>& group : groups)
    {
        std::sort(group.begin(), group.end(), [groupRows](const HoleCircleCandidate& a, const HoleCircleCandidate& b) {
            return groupRows ? a.centerPoint.X < b.centerPoint.X : a.centerPoint.Y > b.centerPoint.Y;
        });
    }
    return groups;
}

const std::vector<HoleCircleCandidate>* PickLargestHoleGroup(const std::vector<std::vector<HoleCircleCandidate>>& groups)
{
    const std::vector<HoleCircleCandidate>* best = nullptr;
    for (const std::vector<HoleCircleCandidate>& group : groups)
    {
        if (group.size() < 2)
        {
            continue;
        }
        if (best == nullptr || group.size() > best->size())
        {
            best = &group;
        }
    }
    return best;
}

double ClampDimensionOrigin(double value, double low, double high)
{
    if (low > high)
    {
        return value;
    }
    return std::max(low, std::min(value, high));
}

std::string MakeHoleLocationDimensionKey(
    const std::string& datumKey,
    bool horizontal,
    double drawingValue)
{
    const long long roundedValue = static_cast<long long>(std::llround(drawingValue * 100.0));
    return datumKey + "|" + (horizontal ? "H" : "V") + "|" + std::to_string(roundedValue);
}

bool GetBoundaryDrawingPoint(
    NXOpen::Drawings::DraftingView* view,
    const CurveAssocCandidate& boundary,
    double drawingPoint[2])
{
    if (boundary.curve == nullptr)
    {
        return false;
    }

    drawingPoint[0] = boundary.modelPoint.X;
    drawingPoint[1] = boundary.modelPoint.Y;
    double boundaryModel[3] = {boundary.modelPoint.X, boundary.modelPoint.Y, boundary.modelPoint.Z};
    if (view != nullptr && view->Tag() != NULL_TAG)
    {
        UF_VIEW_map_model_to_drawing(view->Tag(), boundaryModel, drawingPoint);
    }
    return true;
}

double HoleBoundaryDrawingDistance(
    NXOpen::Drawings::DraftingView* view,
    const CurveAssocCandidate& boundary,
    const HoleCircleCandidate& hole,
    bool horizontal)
{
    double boundaryDrawing[2] = {boundary.modelPoint.X, boundary.modelPoint.Y};
    GetBoundaryDrawingPoint(view, boundary, boundaryDrawing);
    return horizontal
        ? std::abs(boundaryDrawing[0] - hole.centerPoint.X)
        : std::abs(boundaryDrawing[1] - hole.centerPoint.Y);
}

void CreateFrontHoleLocationDimensions(
    NXOpen::Session* session,
    NXOpen::Part* workPart,
    ShallowDetailFilterCache& shallowCache,
    NXOpen::Drawings::DraftingView* view,
    const std::vector<DraftingCurveExtent>& extents,
    const LayoutBounds& bounds,
    double offset)
{
    std::vector<HoleCircleCandidate> holes = CollectHoleCircleCandidates(session, workPart, shallowCache, view, extents, bounds);
    if (holes.empty())
    {
        WriteLine(session, "AutoCreateThreeViews: hole location dimensions skipped; no holes.");
        return;
    }

    const double maxDrawingRadius = std::max_element(holes.begin(), holes.end(), [](const HoleCircleCandidate& a, const HoleCircleCandidate& b) {
        return a.drawingRadius < b.drawingRadius;
    })->drawingRadius;
    const double groupTolerance = std::max(0.6, maxDrawingRadius * 1.5);

    std::vector<std::vector<HoleCircleCandidate>> rows = GroupHolesByAxis(holes, true, groupTolerance);
    std::vector<std::vector<HoleCircleCandidate>> columns = GroupHolesByAxis(holes, false, groupTolerance);

    std::ostringstream startLog;
    startLog << "AutoCreateThreeViews: hole location groups"
             << " holes=" << holes.size()
             << ", rows=" << rows.size()
             << ", columns=" << columns.size()
             << ", tolerance=" << groupTolerance << ".";
    WriteLine(session, startLog.str());

    int createdCount = 0;
    int usableRows = 0;
    int usableColumns = 0;
    const double insideMargin = std::max(2.0, offset * 0.35);
    std::set<std::string> createdLocationDimensionKeys;

    for (size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex)
    {
        const std::vector<HoleCircleCandidate>& row = rows[rowIndex];
        if (row.size() < 2)
        {
            std::ostringstream rowLog;
            rowLog << "AutoCreateThreeViews: hole x pitch row skipped"
                   << " index=" << rowIndex
                   << ", count=" << row.size()
                   << ", reason=less than 2 holes.";
            WriteLine(session, rowLog.str());
            continue;
        }

        ++usableRows;
        const int pairCount = static_cast<int>(row.size()) - 1;
        double rowDimY = row.front().centerPoint.Y + offset * 0.7;
        for (const HoleCircleCandidate& hole : row)
        {
            rowDimY = std::max(rowDimY, hole.centerPoint.Y + offset * 0.7);
        }

        rowDimY = ClampDimensionOrigin(rowDimY, bounds.minY + insideMargin, bounds.maxY - insideMargin);
        for (int i = 0; i < pairCount; ++i)
        {
            const HoleCircleCandidate& first = row[i];
            const HoleCircleCandidate& second = row[i + 1];
            const double drawingValue = std::abs(first.centerPoint.X - second.centerPoint.X);
            const std::string duplicateKey = MakeHoleLocationDimensionKey("center-pitch", true, drawingValue);
            if (createdLocationDimensionKeys.find(duplicateKey) != createdLocationDimensionKeys.end())
            {
                std::ostringstream skipLog;
                skipLog << "AutoCreateThreeViews: hole x pitch skipped; same datum and value already dimensioned"
                        << ", row=" << rowIndex
                        << ", value=" << drawingValue << ".";
                WriteLine(session, skipLog.str());
                continue;
            }

            const NXOpen::Point3d origin(
                (first.centerPoint.X + second.centerPoint.X) * 0.5,
                rowDimY,
                0.0);
            if (CreateHoleCenterDimension(session, workPart, view, first, second, true, origin, "x pitch"))
            {
                createdLocationDimensionKeys.insert(duplicateKey);
                ++createdCount;
            }
        }
    }

    for (size_t columnIndex = 0; columnIndex < columns.size(); ++columnIndex)
    {
        const std::vector<HoleCircleCandidate>& column = columns[columnIndex];
        if (column.size() < 2)
        {
            std::ostringstream columnLog;
            columnLog << "AutoCreateThreeViews: hole y pitch column skipped"
                      << " index=" << columnIndex
                      << ", count=" << column.size()
                      << ", reason=less than 2 holes.";
            WriteLine(session, columnLog.str());
            continue;
        }

        ++usableColumns;
        const int pairCount = static_cast<int>(column.size()) - 1;
        double columnDimX = column.front().centerPoint.X - offset * 0.8;
        for (const HoleCircleCandidate& hole : column)
        {
            columnDimX = std::min(columnDimX, hole.centerPoint.X - offset * 0.8);
        }

        columnDimX = ClampDimensionOrigin(columnDimX, bounds.minX + insideMargin, bounds.maxX - insideMargin);
        for (int i = 0; i < pairCount; ++i)
        {
            const HoleCircleCandidate& first = column[i];
            const HoleCircleCandidate& second = column[i + 1];
            const double drawingValue = std::abs(first.centerPoint.Y - second.centerPoint.Y);
            const std::string duplicateKey = MakeHoleLocationDimensionKey("center-pitch", false, drawingValue);
            if (createdLocationDimensionKeys.find(duplicateKey) != createdLocationDimensionKeys.end())
            {
                std::ostringstream skipLog;
                skipLog << "AutoCreateThreeViews: hole y pitch skipped; same datum and value already dimensioned"
                        << ", column=" << columnIndex
                        << ", value=" << drawingValue << ".";
                WriteLine(session, skipLog.str());
                continue;
            }

            const NXOpen::Point3d origin(
                columnDimX,
                (first.centerPoint.Y + second.centerPoint.Y) * 0.5,
                0.0);
            if (CreateHoleCenterDimension(session, workPart, view, first, second, false, origin, "y pitch"))
            {
                createdLocationDimensionKeys.insert(duplicateKey);
                ++createdCount;
            }
        }
    }

    CurveAssocCandidate rightBoundary;
    CurveAssocCandidate leftBoundary;
    const int locatableRows = static_cast<int>(std::count_if(rows.begin(), rows.end(), [](const std::vector<HoleCircleCandidate>& row) {
        return !row.empty();
    }));
    if (locatableRows > 0 &&
        SelectOverallDimensionCurves(view, extents, bounds, true, (bounds.minY + bounds.maxY) * 0.5, rightBoundary, leftBoundary))
    {
        for (size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex)
        {
            const std::vector<HoleCircleCandidate>& row = rows[rowIndex];
            if (row.empty())
            {
                continue;
            }

            const HoleCircleCandidate& leftHole = row.front();
            const HoleCircleCandidate& rightHole = row.back();
            const double leftValue = HoleBoundaryDrawingDistance(view, leftBoundary, leftHole, true);
            const double rightValue = HoleBoundaryDrawingDistance(view, rightBoundary, rightHole, true);
            const bool useRightBoundary = rightValue < leftValue;
            const CurveAssocCandidate& datumBoundary = useRightBoundary ? rightBoundary : leftBoundary;
            const HoleCircleCandidate& datumHole = useRightBoundary ? rightHole : leftHole;
            const double drawingValue = useRightBoundary ? rightValue : leftValue;
            const std::string datumKey =
                std::string(useRightBoundary ? "right-boundary:" : "left-boundary:") +
                std::to_string(static_cast<unsigned long long>(datumBoundary.curve != nullptr ? datumBoundary.curve->Tag() : NULL_TAG));
            const std::string duplicateKey = MakeHoleLocationDimensionKey(datumKey, true, drawingValue);
            if (createdLocationDimensionKeys.find(duplicateKey) != createdLocationDimensionKeys.end())
            {
                std::ostringstream skipLog;
                skipLog << "AutoCreateThreeViews: hole x locate skipped; same datum and value already dimensioned"
                        << ", row=" << rowIndex
                        << ", value=" << drawingValue << ".";
                WriteLine(session, skipLog.str());
                continue;
            }

            double boundaryDrawing[2] = {0.0, 0.0};
            GetBoundaryDrawingPoint(view, datumBoundary, boundaryDrawing);
            const NXOpen::Point3d origin(
                (boundaryDrawing[0] + datumHole.centerPoint.X) * 0.5,
                ClampDimensionOrigin(datumHole.centerPoint.Y - offset * 0.8, bounds.minY + insideMargin, bounds.maxY - insideMargin),
                0.0);
            if (CreateHoleBoundaryDimension(session, workPart, view, datumBoundary, datumHole, true, origin, "x locate"))
            {
                createdLocationDimensionKeys.insert(duplicateKey);
                ++createdCount;
            }
        }
    }
    else if (locatableRows > 0)
    {
        WriteLine(session, "AutoCreateThreeViews: hole x locate skipped; horizontal datum boundary curve not found.");
    }

    CurveAssocCandidate topBoundary;
    CurveAssocCandidate bottomBoundary;
    const int locatableColumns = static_cast<int>(std::count_if(columns.begin(), columns.end(), [](const std::vector<HoleCircleCandidate>& column) {
        return !column.empty();
    }));
    if (locatableColumns > 0 &&
        SelectOverallDimensionCurves(view, extents, bounds, false, (bounds.minX + bounds.maxX) * 0.5, topBoundary, bottomBoundary))
    {
        for (size_t columnIndex = 0; columnIndex < columns.size(); ++columnIndex)
        {
            const std::vector<HoleCircleCandidate>& column = columns[columnIndex];
            if (column.empty())
            {
                continue;
            }

            const HoleCircleCandidate& topHole = column.front();
            const HoleCircleCandidate& bottomHole = column.back();
            const double topValue = HoleBoundaryDrawingDistance(view, topBoundary, topHole, false);
            const double bottomValue = HoleBoundaryDrawingDistance(view, bottomBoundary, bottomHole, false);
            const bool useTopBoundary = topValue < bottomValue;
            const CurveAssocCandidate& datumBoundary = useTopBoundary ? topBoundary : bottomBoundary;
            const HoleCircleCandidate& datumHole = useTopBoundary ? topHole : bottomHole;
            const double drawingValue = useTopBoundary ? topValue : bottomValue;
            const std::string datumKey =
                std::string(useTopBoundary ? "top-boundary:" : "bottom-boundary:") +
                std::to_string(static_cast<unsigned long long>(datumBoundary.curve != nullptr ? datumBoundary.curve->Tag() : NULL_TAG));
            const std::string duplicateKey = MakeHoleLocationDimensionKey(datumKey, false, drawingValue);
            if (createdLocationDimensionKeys.find(duplicateKey) != createdLocationDimensionKeys.end())
            {
                std::ostringstream skipLog;
                skipLog << "AutoCreateThreeViews: hole y locate skipped; same datum and value already dimensioned"
                        << ", column=" << columnIndex
                        << ", value=" << drawingValue << ".";
                WriteLine(session, skipLog.str());
                continue;
            }

            double boundaryDrawing[2] = {0.0, 0.0};
            GetBoundaryDrawingPoint(view, datumBoundary, boundaryDrawing);
            const NXOpen::Point3d origin(
                ClampDimensionOrigin(datumHole.centerPoint.X - offset * 0.8, bounds.minX + insideMargin, bounds.maxX - insideMargin),
                (boundaryDrawing[1] + datumHole.centerPoint.Y) * 0.5,
                0.0);
            if (CreateHoleBoundaryDimension(session, workPart, view, datumBoundary, datumHole, false, origin, "y locate"))
            {
                createdLocationDimensionKeys.insert(duplicateKey);
                ++createdCount;
            }
        }
    }
    else if (locatableColumns > 0)
    {
        WriteLine(session, "AutoCreateThreeViews: hole y locate skipped; vertical datum boundary curve not found.");
    }

    std::ostringstream doneLog;
    doneLog << "AutoCreateThreeViews: hole location dimensions finished"
            << ", created=" << createdCount
            << ", usableRows=" << usableRows
            << ", usableColumns=" << usableColumns
            << ", locatableRows=" << locatableRows
            << ", locatableColumns=" << locatableColumns
            << ", uniqueLocationKeys=" << createdLocationDimensionKeys.size() << ".";
    WriteLine(session, doneLog.str());
}

std::vector<LineSegmentCandidate> CollectLineSegments(
    NXOpen::Drawings::DraftingView* view,
    const std::vector<DraftingCurveExtent>& extents)
{
    std::vector<LineSegmentCandidate> lines;
    if (view == nullptr)
    {
        return lines;
    }

    for (const DraftingCurveExtent& extent : extents)
    {
        if (!extent.initialized || extent.curve == nullptr || extent.tag == NULL_TAG)
        {
            continue;
        }

        UF_EVAL_p_t evaluator = nullptr;
        if (UF_EVAL_initialize(extent.tag, &evaluator) != 0 || evaluator == nullptr)
        {
            continue;
        }

        bool isLine = false;
        if (UF_EVAL_is_line(evaluator, &isLine) != 0 || !isLine)
        {
            UF_EVAL_free(evaluator);
            continue;
        }
        UF_EVAL_free(evaluator);

        UF_CURVE_line_t lineData{};
        if (UF_CURVE_ask_line_data(extent.tag, &lineData) != 0)
        {
            continue;
        }

        double p1[2] = {0.0, 0.0};
        double p2[2] = {0.0, 0.0};
        if (UF_VIEW_map_model_to_drawing(view->Tag(), lineData.start_point, p1) != 0 ||
            UF_VIEW_map_model_to_drawing(view->Tag(), lineData.end_point, p2) != 0)
        {
            continue;
        }

        LineSegmentCandidate line;
        line.curve = extent.curve;
        line.startModel = NXOpen::Point3d(lineData.start_point[0], lineData.start_point[1], lineData.start_point[2]);
        line.endModel = NXOpen::Point3d(lineData.end_point[0], lineData.end_point[1], lineData.end_point[2]);
        line.startX = p1[0];
        line.startY = p1[1];
        line.endX = p2[0];
        line.endY = p2[1];
        line.minX = std::min(p1[0], p2[0]);
        line.maxX = std::max(p1[0], p2[0]);
        line.minY = std::min(p1[1], p2[1]);
        line.maxY = std::max(p1[1], p2[1]);
        line.length = std::sqrt((p1[0] - p2[0]) * (p1[0] - p2[0]) + (p1[1] - p2[1]) * (p1[1] - p2[1]));
        lines.push_back(line);
    }

    return lines;
}

CurveAssocCandidate PickLineEndpointNearX(const LineSegmentCandidate& line, double x)
{
    const double startDistance = std::abs(line.startX - x);
    const double endDistance = std::abs(line.endX - x);
    CurveAssocCandidate candidate;
    candidate.curve = line.curve;
    if (startDistance <= endDistance)
    {
        candidate.snapType = NXOpen::InferSnapType::SnapTypeStart;
        candidate.modelPoint = line.startModel;
    }
    else
    {
        candidate.snapType = NXOpen::InferSnapType::SnapTypeEnd;
        candidate.modelPoint = line.endModel;
    }
    return candidate;
}

bool CreateLocalHeightDimension(
    NXOpen::Session* session,
    NXOpen::Part* workPart,
    NXOpen::Drawings::DraftingView* view,
    const LineSegmentCandidate& lower,
    const LineSegmentCandidate& upper,
    const NXOpen::Point3d& origin,
    double pickX,
    const std::string& label)
{
    if (workPart == nullptr || view == nullptr || lower.curve == nullptr || upper.curve == nullptr)
    {
        return false;
    }
    if (IsZeroDimensionValue(std::abs(upper.minY - lower.minY)))
    {
        WriteLine(session, "AutoCreateThreeViews: local height dimension skipped; dimension value is zero.");
        return false;
    }

    CurveAssocCandidate lowerPoint = PickLineEndpointNearX(lower, pickX);
    CurveAssocCandidate upperPoint = PickLineEndpointNearX(upper, pickX);
    NXOpen::View* nullView = nullptr;
    NXOpen::Point3d assistPoint(0.0, 0.0, 0.0);
    NXOpen::Annotations::Dimension* nullDimension = nullptr;
    NXOpen::Annotations::RapidDimensionBuilder* builder = workPart->Dimensions()->CreateRapidDimensionBuilder(nullDimension);
    if (builder == nullptr)
    {
        WriteLine(session, "AutoCreateThreeViews: local height dimension failed; builder not available.");
        return false;
    }

    try
    {
        builder->FirstAssociativity()->SetValue(
            lowerPoint.snapType,
            lowerPoint.curve,
            view,
            lowerPoint.modelPoint,
            nullptr,
            nullView,
            assistPoint);
        builder->SecondAssociativity()->SetValue(
            upperPoint.snapType,
            upperPoint.curve,
            view,
            upperPoint.modelPoint,
            nullptr,
            nullView,
            assistPoint);
        builder->Style()->DimensionStyle()->SetTextCentered(true);
        builder->Style()->DimensionStyle()->SetNarrowDisplayType(NXOpen::Annotations::NarrowDisplayOptionNone);
        builder->Measurement()->SetMethod(NXOpen::Annotations::DimensionMeasurementBuilder::MeasurementMethodVertical);
        builder->Origin()->Origin()->SetValue(nullptr, nullView, origin);
        NXOpen::NXObject* object = builder->Commit();
        builder->Destroy();

        std::ostringstream log;
        log << "AutoCreateThreeViews: local height dimension created"
            << " view=" << label
            << ", lowerY=" << lower.minY
            << ", upperY=" << upper.minY
            << ", pickX=" << pickX
            << ", originX=" << origin.X
            << ", originY=" << origin.Y << ".";
        WriteLine(session, log.str());
        return object != nullptr;
    }
    catch (const NXOpen::NXException& ex)
    {
        builder->Destroy();
        WriteLine(session, std::string("AutoCreateThreeViews: local height dimension failed, NXException: ") + ex.Message() + ".");
        return false;
    }
    catch (...)
    {
        builder->Destroy();
        WriteLine(session, "AutoCreateThreeViews: local height dimension failed, unknown exception.");
        return false;
    }
}

CurveAssocCandidate MakeLineEndpointCandidate(const LineSegmentCandidate& line, bool useStart)
{
    CurveAssocCandidate candidate;
    candidate.curve = line.curve;
    candidate.snapType = useStart ? NXOpen::InferSnapType::SnapTypeStart : NXOpen::InferSnapType::SnapTypeEnd;
    candidate.modelPoint = useStart ? line.startModel : line.endModel;
    return candidate;
}

bool CreateCurvePointDimension(
    NXOpen::Session* session,
    NXOpen::Part* workPart,
    NXOpen::Drawings::DraftingView* view,
    const CurveAssocCandidate& first,
    const CurveAssocCandidate& second,
    bool horizontal,
    const NXOpen::Point3d& origin,
    const std::string& logLabel)
{
    if (workPart == nullptr || view == nullptr || first.curve == nullptr || second.curve == nullptr)
    {
        return false;
    }
    double firstDrawing[2] = {first.modelPoint.X, first.modelPoint.Y};
    double secondDrawing[2] = {second.modelPoint.X, second.modelPoint.Y};
    if (view->Tag() != NULL_TAG)
    {
        double firstModel[3] = {first.modelPoint.X, first.modelPoint.Y, first.modelPoint.Z};
        double secondModel[3] = {second.modelPoint.X, second.modelPoint.Y, second.modelPoint.Z};
        UF_VIEW_map_model_to_drawing(view->Tag(), firstModel, firstDrawing);
        UF_VIEW_map_model_to_drawing(view->Tag(), secondModel, secondDrawing);
    }
    const double drawingDistance = horizontal
        ? std::abs(firstDrawing[0] - secondDrawing[0])
        : std::abs(firstDrawing[1] - secondDrawing[1]);
    if (IsZeroDimensionValue(drawingDistance))
    {
        WriteLine(session, "AutoCreateThreeViews: " + logLabel + " dimension skipped; dimension value is zero.");
        return false;
    }

    NXOpen::View* nullView = nullptr;
    NXOpen::Point3d assistPoint(0.0, 0.0, 0.0);
    NXOpen::Annotations::Dimension* nullDimension = nullptr;
    NXOpen::Annotations::RapidDimensionBuilder* builder = workPart->Dimensions()->CreateRapidDimensionBuilder(nullDimension);
    if (builder == nullptr)
    {
        WriteLine(session, "AutoCreateThreeViews: " + logLabel + " dimension failed; builder not available.");
        return false;
    }

    try
    {
        builder->FirstAssociativity()->SetValue(
            first.snapType,
            first.curve,
            view,
            first.modelPoint,
            nullptr,
            nullView,
            assistPoint);
        builder->SecondAssociativity()->SetValue(
            second.snapType,
            second.curve,
            view,
            second.modelPoint,
            nullptr,
            nullView,
            assistPoint);
        builder->Style()->DimensionStyle()->SetTextCentered(true);
        builder->Style()->DimensionStyle()->SetNarrowDisplayType(NXOpen::Annotations::NarrowDisplayOptionNone);
        builder->Measurement()->SetMethod(horizontal
            ? NXOpen::Annotations::DimensionMeasurementBuilder::MeasurementMethodHorizontal
            : NXOpen::Annotations::DimensionMeasurementBuilder::MeasurementMethodVertical);
        builder->Origin()->Origin()->SetValue(nullptr, nullView, origin);
        NXOpen::NXObject* object = builder->Commit();
        builder->Destroy();

        std::ostringstream log;
        log << "AutoCreateThreeViews: " << logLabel << " dimension created"
            << " method=" << (horizontal ? "horizontal" : "vertical")
            << ", originX=" << origin.X
            << ", originY=" << origin.Y << ".";
        WriteLine(session, log.str());
        return object != nullptr;
    }
    catch (const NXOpen::NXException& ex)
    {
        builder->Destroy();
        WriteLine(session, "AutoCreateThreeViews: " + logLabel + " dimension failed, NXException: " + ex.Message() + ".");
        return false;
    }
    catch (...)
    {
        builder->Destroy();
        WriteLine(session, "AutoCreateThreeViews: " + logLabel + " dimension failed, unknown exception.");
        return false;
    }
}

CurveAssocCandidate PickLineEndpointNearY(const LineSegmentCandidate& line, double y)
{
    const double startDistance = std::abs(line.startY - y);
    const double endDistance = std::abs(line.endY - y);
    CurveAssocCandidate candidate;
    candidate.curve = line.curve;
    if (startDistance <= endDistance)
    {
        candidate.snapType = NXOpen::InferSnapType::SnapTypeStart;
        candidate.modelPoint = line.startModel;
    }
    else
    {
        candidate.snapType = NXOpen::InferSnapType::SnapTypeEnd;
        candidate.modelPoint = line.endModel;
    }
    return candidate;
}

bool CreateLocalWidthDimension(
    NXOpen::Session* session,
    NXOpen::Part* workPart,
    NXOpen::Drawings::DraftingView* view,
    const LineSegmentCandidate& left,
    const LineSegmentCandidate& right,
    const NXOpen::Point3d& origin,
    double pickY,
    const std::string& label)
{
    if (workPart == nullptr || view == nullptr || left.curve == nullptr || right.curve == nullptr)
    {
        return false;
    }

    CurveAssocCandidate leftPoint = PickLineEndpointNearY(left, pickY);
    CurveAssocCandidate rightPoint = PickLineEndpointNearY(right, pickY);
    return CreateCurvePointDimension(
        session,
        workPart,
        view,
        leftPoint,
        rightPoint,
        true,
        origin,
        label + " face width");
}

NXOpen::Point3d LineModelMidPoint(const LineSegmentCandidate& line)
{
    return NXOpen::Point3d(
        (line.startModel.X + line.endModel.X) * 0.5,
        (line.startModel.Y + line.endModel.Y) * 0.5,
        (line.startModel.Z + line.endModel.Z) * 0.5);
}

NXOpen::Point3d LineModelPointNearDimensionOrigin(
    const LineSegmentCandidate& line,
    NXOpen::Annotations::DimensionMeasurementBuilder::MeasurementMethod method,
    const NXOpen::Point3d& origin)
{
    if (method == NXOpen::Annotations::DimensionMeasurementBuilder::MeasurementMethodHorizontal)
    {
        return std::abs(line.startY - origin.Y) <= std::abs(line.endY - origin.Y)
            ? line.startModel
            : line.endModel;
    }

    if (method == NXOpen::Annotations::DimensionMeasurementBuilder::MeasurementMethodVertical)
    {
        return std::abs(line.startX - origin.X) <= std::abs(line.endX - origin.X)
            ? line.startModel
            : line.endModel;
    }

    return LineModelMidPoint(line);
}

NXOpen::Face* FaceObjectFromTag(tag_t faceTag)
{
    if (faceTag == NULL_TAG)
    {
        return nullptr;
    }

    return dynamic_cast<NXOpen::Face*>(NXOpen::NXObjectManager::Get(faceTag));
}

bool CreateFacePairDimension(
    NXOpen::Session* session,
    NXOpen::Part* workPart,
    NXOpen::Drawings::DraftingView* view,
    const LineProjectionFaceCandidate& first,
    const LineProjectionFaceCandidate& second,
    NXOpen::Annotations::DimensionMeasurementBuilder::MeasurementMethod method,
    const NXOpen::Point3d& origin,
    const std::string& label)
{
    if (workPart == nullptr || view == nullptr ||
        first.faceTag == NULL_TAG || second.faceTag == NULL_TAG ||
        first.line.curve == nullptr || second.line.curve == nullptr)
    {
        return false;
    }

    NXOpen::Face* firstFace = FaceObjectFromTag(first.faceTag);
    NXOpen::Face* secondFace = FaceObjectFromTag(second.faceTag);
    if (firstFace == nullptr || secondFace == nullptr)
    {
        WriteLine(session, "AutoCreateThreeViews: " + label + " face dimension failed; face object not available.");
        return false;
    }

    double drawingDistance = 0.0;
    if (method == NXOpen::Annotations::DimensionMeasurementBuilder::MeasurementMethodHorizontal)
    {
        drawingDistance = std::abs(first.centerX - second.centerX);
    }
    else if (method == NXOpen::Annotations::DimensionMeasurementBuilder::MeasurementMethodVertical)
    {
        drawingDistance = std::abs(first.centerY - second.centerY);
    }
    else
    {
        const double secondMidX = (second.line.startX + second.line.endX) * 0.5;
        const double secondMidY = (second.line.startY + second.line.endY) * 0.5;
        const double lineDx = first.line.endX - first.line.startX;
        const double lineDy = first.line.endY - first.line.startY;
        const double lineLength = std::max(1.0e-9, std::sqrt(lineDx * lineDx + lineDy * lineDy));
        drawingDistance = std::abs(
            lineDx * (first.line.startY - secondMidY) -
            (first.line.startX - secondMidX) * lineDy) / lineLength;
    }
    if (IsZeroDimensionValue(drawingDistance))
    {
        WriteLine(session, "AutoCreateThreeViews: " + label + " face dimension skipped; dimension value is zero.");
        return false;
    }

    NXOpen::View* nullView = nullptr;
    NXOpen::Annotations::Dimension* nullDimension = nullptr;
    NXOpen::Annotations::RapidDimensionBuilder* builder = workPart->Dimensions()->CreateRapidDimensionBuilder(nullDimension);
    if (builder == nullptr)
    {
        WriteLine(session, "AutoCreateThreeViews: " + label + " face dimension failed; builder not available.");
        return false;
    }

    try
    {
        builder->FirstAssociativity()->SetValue(firstFace, view, LineModelPointNearDimensionOrigin(first.line, method, origin));
        builder->SecondAssociativity()->SetValue(secondFace, view, LineModelPointNearDimensionOrigin(second.line, method, origin));
        builder->Style()->DimensionStyle()->SetTextCentered(true);
        builder->Style()->DimensionStyle()->SetNarrowDisplayType(NXOpen::Annotations::NarrowDisplayOptionNone);
        builder->Measurement()->SetMethod(method);
        builder->Origin()->Origin()->SetValue(nullptr, nullView, origin);
        NXOpen::NXObject* object = builder->Commit();
        builder->Destroy();

        std::ostringstream log;
        log << "AutoCreateThreeViews: " << label << " face dimension created"
            << " firstFace=" << static_cast<unsigned long long>(first.faceTag)
            << ", secondFace=" << static_cast<unsigned long long>(second.faceTag)
            << ", value=" << drawingDistance
            << ", originX=" << origin.X
            << ", originY=" << origin.Y << ".";
        WriteLine(session, log.str());
        return object != nullptr;
    }
    catch (const NXOpen::NXException& ex)
    {
        builder->Destroy();
        WriteLine(session, "AutoCreateThreeViews: " + label + " face dimension failed, NXException: " + ex.Message() + ".");
        return false;
    }
    catch (...)
    {
        builder->Destroy();
        WriteLine(session, "AutoCreateThreeViews: " + label + " face dimension failed, unknown exception.");
        return false;
    }
}

bool CreateLocalPerpendicularLineDimension(
    NXOpen::Session* session,
    NXOpen::Part* workPart,
    NXOpen::Drawings::DraftingView* view,
    const LineSegmentCandidate& first,
    const LineSegmentCandidate& second,
    const NXOpen::Point3d& origin,
    const std::string& label)
{
    if (workPart == nullptr || view == nullptr || first.curve == nullptr || second.curve == nullptr)
    {
        return false;
    }
    const double secondMidX = (second.startX + second.endX) * 0.5;
    const double secondMidY = (second.startY + second.endY) * 0.5;
    const double lineDx = first.endX - first.startX;
    const double lineDy = first.endY - first.startY;
    const double lineLength = std::max(1.0e-9, std::sqrt(lineDx * lineDx + lineDy * lineDy));
    const double perpendicularDistance = std::abs(
        lineDx * (first.startY - secondMidY) -
        (first.startX - secondMidX) * lineDy) / lineLength;
    if (IsZeroDimensionValue(perpendicularDistance))
    {
        WriteLine(session, "AutoCreateThreeViews: local angled face dimension skipped; dimension value is zero.");
        return false;
    }

    NXOpen::View* nullView = nullptr;
    NXOpen::Point3d assistPoint(0.0, 0.0, 0.0);
    NXOpen::Annotations::Dimension* nullDimension = nullptr;
    NXOpen::Annotations::RapidDimensionBuilder* builder = workPart->Dimensions()->CreateRapidDimensionBuilder(nullDimension);
    if (builder == nullptr)
    {
        WriteLine(session, "AutoCreateThreeViews: local angled face dimension failed; builder not available.");
        return false;
    }

    try
    {
        builder->FirstAssociativity()->SetValue(
            NXOpen::InferSnapType::SnapTypeCurve,
            first.curve,
            view,
            LineModelMidPoint(first),
            nullptr,
            nullView,
            assistPoint);
        builder->SecondAssociativity()->SetValue(
            NXOpen::InferSnapType::SnapTypeCurve,
            second.curve,
            view,
            LineModelMidPoint(second),
            nullptr,
            nullView,
            assistPoint);
        builder->Style()->DimensionStyle()->SetTextCentered(true);
        builder->Style()->DimensionStyle()->SetNarrowDisplayType(NXOpen::Annotations::NarrowDisplayOptionNone);
        builder->Measurement()->SetMethod(NXOpen::Annotations::DimensionMeasurementBuilder::MeasurementMethodPerpendicular);
        builder->Origin()->Origin()->SetValue(nullptr, nullView, origin);
        NXOpen::NXObject* object = builder->Commit();
        builder->Destroy();

        std::ostringstream log;
        log << "AutoCreateThreeViews: local angled face dimension created"
            << " view=" << label
            << ", originX=" << origin.X
            << ", originY=" << origin.Y << ".";
        WriteLine(session, log.str());
        return object != nullptr;
    }
    catch (const NXOpen::NXException& ex)
    {
        builder->Destroy();
        WriteLine(session, std::string("AutoCreateThreeViews: local angled face dimension failed, NXException: ") + ex.Message() + ".");
        return false;
    }
    catch (...)
    {
        builder->Destroy();
        WriteLine(session, "AutoCreateThreeViews: local angled face dimension failed, unknown exception.");
        return false;
    }
}

double DrawingPointDistance(double ax, double ay, double bx, double by)
{
    const double dx = ax - bx;
    const double dy = ay - by;
    return std::sqrt(dx * dx + dy * dy);
}

double NormalizeDrawingDirection(double& x, double& y)
{
    const double length = std::sqrt(x * x + y * y);
    if (length > 1.0e-9)
    {
        x /= length;
        y /= length;
    }
    return length;
}

bool ContainsTag(const std::vector<tag_t>& tags, tag_t tag)
{
    for (tag_t existing : tags)
    {
        if (existing == tag)
        {
            return true;
        }
    }
    return false;
}

bool TryIntersectDrawingLines(
    const LineSegmentCandidate& first,
    const LineSegmentCandidate& second,
    double& intersectionX,
    double& intersectionY)
{
    const double x1 = first.startX;
    const double y1 = first.startY;
    const double x2 = first.endX;
    const double y2 = first.endY;
    const double x3 = second.startX;
    const double y3 = second.startY;
    const double x4 = second.endX;
    const double y4 = second.endY;
    const double denominator = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
    if (std::abs(denominator) < 1.0e-9)
    {
        return false;
    }

    intersectionX =
        ((x1 * y2 - y1 * x2) * (x3 - x4) - (x1 - x2) * (x3 * y4 - y3 * x4)) / denominator;
    intersectionY =
        ((x1 * y2 - y1 * x2) * (y3 - y4) - (y1 - y2) * (x3 * y4 - y3 * x4)) / denominator;
    return true;
}

NXOpen::Point3d PickLineEndpointNearest(
    const LineSegmentCandidate& line,
    double x,
    double y,
    bool& startIsNear)
{
    const double startDistance = DrawingPointDistance(line.startX, line.startY, x, y);
    const double endDistance = DrawingPointDistance(line.endX, line.endY, x, y);
    startIsNear = startDistance <= endDistance;
    return startIsNear ? line.startModel : line.endModel;
}

void DirectionFromIntersectionToLineInterior(
    const LineSegmentCandidate& line,
    double intersectionX,
    double intersectionY,
    bool startIsNear,
    double& directionX,
    double& directionY)
{
    directionX = (startIsNear ? line.endX : line.startX) - intersectionX;
    directionY = (startIsNear ? line.endY : line.startY) - intersectionY;
    if (NormalizeDrawingDirection(directionX, directionY) <= 1.0e-9)
    {
        directionX = line.endX - line.startX;
        directionY = line.endY - line.startY;
        NormalizeDrawingDirection(directionX, directionY);
    }
}

bool IsPlanarFaceType(tag_t faceTag, int& faceType)
{
    faceType = 0;
    if (faceTag == NULL_TAG)
    {
        return false;
    }

    double point[3] = {0.0, 0.0, 0.0};
    double direction[3] = {0.0, 0.0, 0.0};
    double box[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    double radius = 0.0;
    double radData = 0.0;
    int normDir = 1;
    if (UF_MODL_ask_face_data(faceTag, &faceType, point, direction, box, &radius, &radData, &normDir) != 0)
    {
        faceType = 0;
        return false;
    }

    return faceType == 22;
}

bool IsRoundTransitionFaceType(int faceType)
{
    return faceType == UF_MODL_CYLINDRICAL_FACE ||
           faceType == UF_MODL_CONICAL_FACE ||
           faceType == UF_MODL_TOROIDAL_FACE ||
           faceType == UF_MODL_BLENDING_FACE ||
           faceType == UF_MODL_SWEPT_FACE;
}

std::vector<tag_t> AskAdjacentFaceTags(tag_t faceTag)
{
    std::vector<tag_t> adjacentFaces;
    if (faceTag == NULL_TAG)
    {
        return adjacentFaces;
    }

    uf_list_p_t adjacentList = nullptr;
    if (UF_MODL_ask_adjac_faces(faceTag, &adjacentList) != 0 || adjacentList == nullptr)
    {
        return adjacentFaces;
    }

    int adjacentCount = 0;
    UF_MODL_ask_list_count(adjacentList, &adjacentCount);
    for (int index = 0; index < adjacentCount; ++index)
    {
        tag_t adjacentTag = NULL_TAG;
        if (UF_MODL_ask_list_item(adjacentList, index, &adjacentTag) == 0 && adjacentTag != NULL_TAG)
        {
            adjacentFaces.push_back(adjacentTag);
        }
    }

    UF_MODL_delete_list(&adjacentList);
    return adjacentFaces;
}

bool FacesDirectlyAdjacent(tag_t firstFace, tag_t secondFace)
{
    const std::vector<tag_t> adjacentFaces = AskAdjacentFaceTags(firstFace);
    return ContainsTag(adjacentFaces, secondFace);
}

bool FacesConnectedThroughRoundTransition(tag_t firstFace, tag_t secondFace)
{
    const std::vector<tag_t> firstAdjacentFaces = AskAdjacentFaceTags(firstFace);
    const std::vector<tag_t> secondAdjacentFaces = AskAdjacentFaceTags(secondFace);
    for (tag_t firstAdjacent : firstAdjacentFaces)
    {
        if (!ContainsTag(secondAdjacentFaces, firstAdjacent))
        {
            continue;
        }

        int faceType = 0;
        if (!IsPlanarFaceType(firstAdjacent, faceType) && IsRoundTransitionFaceType(faceType))
        {
            return true;
        }
    }
    return false;
}

bool FacesConnectedForAngle(tag_t firstFace, tag_t secondFace, bool& viaRoundTransition)
{
    viaRoundTransition = false;
    if (firstFace == NULL_TAG || secondFace == NULL_TAG || firstFace == secondFace)
    {
        return false;
    }

    if (FacesDirectlyAdjacent(firstFace, secondFace))
    {
        return true;
    }

    viaRoundTransition = FacesConnectedThroughRoundTransition(firstFace, secondFace);
    return viaRoundTransition;
}

bool CreateLocalAngleDimension(
    NXOpen::Session* session,
    NXOpen::Part* workPart,
    NXOpen::Drawings::DraftingView* view,
    const LineSegmentCandidate& first,
    const LineSegmentCandidate& second,
    double intersectionX,
    double intersectionY,
    const std::string& label)
{
    if (workPart == nullptr || view == nullptr || first.curve == nullptr || second.curve == nullptr ||
        first.curve == second.curve)
    {
        return false;
    }

    bool firstStartIsNear = false;
    bool secondStartIsNear = false;
    NXOpen::Point3d firstAnchor = PickLineEndpointNearest(first, intersectionX, intersectionY, firstStartIsNear);
    NXOpen::Point3d secondAnchor = PickLineEndpointNearest(second, intersectionX, intersectionY, secondStartIsNear);

    double firstDirectionX = 0.0;
    double firstDirectionY = 0.0;
    double secondDirectionX = 0.0;
    double secondDirectionY = 0.0;
    DirectionFromIntersectionToLineInterior(first, intersectionX, intersectionY, firstStartIsNear, firstDirectionX, firstDirectionY);
    DirectionFromIntersectionToLineInterior(second, intersectionX, intersectionY, secondStartIsNear, secondDirectionX, secondDirectionY);
    const double directionDot = std::max(-1.0, std::min(1.0, firstDirectionX * secondDirectionX + firstDirectionY * secondDirectionY));
    if (std::abs(1.0 - std::abs(directionDot)) <= 1.0e-6)
    {
        WriteLine(session, "AutoCreateThreeViews: local angle dimension skipped; dimension value is zero.");
        return false;
    }

    double bisectorX = firstDirectionX + secondDirectionX;
    double bisectorY = firstDirectionY + secondDirectionY;
    if (NormalizeDrawingDirection(bisectorX, bisectorY) <= 1.0e-9)
    {
        bisectorX = -firstDirectionY;
        bisectorY = firstDirectionX;
        NormalizeDrawingDirection(bisectorX, bisectorY);
    }

    const double originRadius = ClampDouble(std::min(first.length, second.length) * 0.18, 6.0, 14.0);
    NXOpen::Point3d origin(
        intersectionX + bisectorX * originRadius,
        intersectionY + bisectorY * originRadius,
        0.0);

    NXOpen::Annotations::BaseAngularDimension* nullAngularDimension = nullptr;
    NXOpen::Annotations::BaseAngularDimensionBuilder* builder =
        static_cast<NXOpen::Annotations::BaseAngularDimensionBuilder*>(
            workPart->Dimensions()->CreateMinorAngularDimensionBuilder(nullAngularDimension));
    if (builder == nullptr)
    {
        WriteLine(session, "AutoCreateThreeViews: local angle dimension failed; builder not available.");
        return false;
    }

    try
    {
        NXOpen::View* nullView = nullptr;
        NXOpen::Point3d assistPoint(0.0, 0.0, 0.0);
        builder->Origin()->Plane()->SetPlaneMethod(NXOpen::Annotations::PlaneBuilder::PlaneMethodTypeXyPlane);
        builder->Origin()->SetInferRelativeToGeometry(false);
        builder->Origin()->SetAnchor(NXOpen::Annotations::OriginBuilder::AlignmentPositionMidCenter);
        builder->Style()->DimensionStyle()->SetDimensionReferenceIncludeType(NXOpen::Annotations::ReferenceIncludeTypeOnlyValue);
        builder->Style()->DimensionStyle()->SetNarrowDisplayType(NXOpen::Annotations::NarrowDisplayOptionNone);
        builder->Style()->DimensionStyle()->SetTextCentered(true);
        builder->FirstAssociativity()->SetValue(
            NXOpen::InferSnapType::SnapTypeNone,
            first.curve,
            view,
            firstAnchor,
            nullptr,
            nullView,
            assistPoint);
        builder->SecondAssociativity()->SetValue(
            NXOpen::InferSnapType::SnapTypeNone,
            second.curve,
            view,
            secondAnchor,
            nullptr,
            nullView,
            assistPoint);
        builder->Origin()->Origin()->SetValue(nullptr, nullView, origin);
        NXOpen::NXObject* object = builder->Commit();
        builder->Destroy();

        std::ostringstream log;
        log << "AutoCreateThreeViews: local angle dimension created"
            << " view=" << label
            << ", intersectionX=" << intersectionX
            << ", intersectionY=" << intersectionY
            << ", originX=" << origin.X
            << ", originY=" << origin.Y << ".";
        WriteLine(session, log.str());
        return object != nullptr;
    }
    catch (const NXOpen::NXException& ex)
    {
        builder->Destroy();
        WriteLine(session, std::string("AutoCreateThreeViews: local angle dimension failed, NXException: ") + ex.Message() + ".");
        return false;
    }
    catch (...)
    {
        builder->Destroy();
        WriteLine(session, "AutoCreateThreeViews: local angle dimension failed, unknown exception.");
        return false;
    }
}

double RangeOverlap(double firstMin, double firstMax, double secondMin, double secondMax)
{
    return std::min(firstMax, secondMax) - std::max(firstMin, secondMin);
}

bool SameDraftingCurve(const LineSegmentCandidate& line, const LineSegmentCandidate& other)
{
    return line.curve != nullptr && line.curve == other.curve;
}

bool BetweenWithTolerance(double value, double first, double second, double tolerance)
{
    return value >= std::min(first, second) - tolerance &&
           value <= std::max(first, second) + tolerance;
}

bool PointNearSegmentEnd(
    double x,
    double y,
    double firstX,
    double firstY,
    double secondX,
    double secondY,
    double tolerance)
{
    return (std::abs(x - firstX) <= tolerance && std::abs(y - firstY) <= tolerance) ||
           (std::abs(x - secondX) <= tolerance && std::abs(y - secondY) <= tolerance);
}

bool PointNearLineEnd(double x, double y, const LineSegmentCandidate& line, double tolerance)
{
    return (std::abs(x - line.startX) <= tolerance && std::abs(y - line.startY) <= tolerance) ||
           (std::abs(x - line.endX) <= tolerance && std::abs(y - line.endY) <= tolerance);
}

int CountDimensionExtensionCrossings(
    const std::vector<LineSegmentCandidate>& lines,
    const LineSegmentCandidate& firstMeasured,
    const LineSegmentCandidate& secondMeasured,
    double startX,
    double startY,
    double endX,
    double endY,
    double tolerance)
{
    const bool extensionHorizontal = std::abs(startY - endY) <= tolerance;
    const bool extensionVertical = std::abs(startX - endX) <= tolerance;
    if (!extensionHorizontal && !extensionVertical)
    {
        return 0;
    }

    int crossings = 0;
    for (const LineSegmentCandidate& line : lines)
    {
        if (line.curve == nullptr ||
            SameDraftingCurve(line, firstMeasured) ||
            SameDraftingCurve(line, secondMeasured))
        {
            continue;
        }

        const bool lineHorizontal = std::abs(line.startY - line.endY) <= tolerance;
        const bool lineVertical = std::abs(line.startX - line.endX) <= tolerance;
        if (extensionHorizontal)
        {
            const double y = startY;
            const double extensionMinX = std::min(startX, endX);
            const double extensionMaxX = std::max(startX, endX);
            if (lineHorizontal && std::abs(line.startY - y) <= tolerance)
            {
                const double overlap = RangeOverlap(extensionMinX, extensionMaxX, line.minX, line.maxX);
                if (overlap > tolerance * 2.0)
                {
                    crossings += 3;
                }
                continue;
            }
            if (lineVertical &&
                BetweenWithTolerance(line.startX, extensionMinX, extensionMaxX, tolerance) &&
                BetweenWithTolerance(y, line.startY, line.endY, tolerance))
            {
                if (!PointNearSegmentEnd(line.startX, y, startX, startY, endX, endY, tolerance * 2.0) &&
                    !PointNearLineEnd(line.startX, y, line, tolerance * 2.0))
                {
                    ++crossings;
                }
            }
            continue;
        }

        const double x = startX;
        const double extensionMinY = std::min(startY, endY);
        const double extensionMaxY = std::max(startY, endY);
        if (lineVertical && std::abs(line.startX - x) <= tolerance)
        {
            const double overlap = RangeOverlap(extensionMinY, extensionMaxY, line.minY, line.maxY);
            if (overlap > tolerance * 2.0)
            {
                crossings += 3;
            }
            continue;
        }
        if (lineHorizontal &&
            BetweenWithTolerance(x, line.startX, line.endX, tolerance) &&
            BetweenWithTolerance(line.startY, extensionMinY, extensionMaxY, tolerance))
        {
            if (!PointNearSegmentEnd(x, line.startY, startX, startY, endX, endY, tolerance * 2.0) &&
                !PointNearLineEnd(x, line.startY, line, tolerance * 2.0))
            {
                ++crossings;
            }
        }
    }

    return crossings;
}

bool ContainsFaceTag(const std::vector<tag_t>& tags, tag_t faceTag)
{
    for (tag_t usedTag : tags)
    {
        if (usedTag == faceTag)
        {
            return true;
        }
    }
    return false;
}

FacePlaneSignature MakeFacePlaneSignature(
    const NXOpen::Vector3d& normalInput,
    const NXOpen::Point3d& point)
{
    FacePlaneSignature signature;
    NXOpen::Vector3d normal = NormalizeVector(normalInput);
    if (VectorLength(normal) < 1.0e-6)
    {
        return signature;
    }

    if (normal.X < -1.0e-8 ||
        (std::abs(normal.X) <= 1.0e-8 && normal.Y < -1.0e-8) ||
        (std::abs(normal.X) <= 1.0e-8 && std::abs(normal.Y) <= 1.0e-8 && normal.Z < -1.0e-8))
    {
        normal.X = -normal.X;
        normal.Y = -normal.Y;
        normal.Z = -normal.Z;
    }

    const double distance = normal.X * point.X + normal.Y * point.Y + normal.Z * point.Z;
    const double normalScale = 1000000.0;
    const double distanceTolerance = 0.05;
    signature.nx = static_cast<long long>(std::llround(normal.X * normalScale));
    signature.ny = static_cast<long long>(std::llround(normal.Y * normalScale));
    signature.nz = static_cast<long long>(std::llround(normal.Z * normalScale));
    signature.distance = static_cast<long long>(std::llround(distance / distanceTolerance));
    signature.valid = true;
    return signature;
}

int CompareFacePlaneSignature(const FacePlaneSignature& a, const FacePlaneSignature& b)
{
    if (a.valid != b.valid)
    {
        return a.valid ? 1 : -1;
    }
    if (a.nx != b.nx)
    {
        return a.nx < b.nx ? -1 : 1;
    }
    if (a.ny != b.ny)
    {
        return a.ny < b.ny ? -1 : 1;
    }
    if (a.nz != b.nz)
    {
        return a.nz < b.nz ? -1 : 1;
    }
    if (a.distance != b.distance)
    {
        return a.distance < b.distance ? -1 : 1;
    }
    return 0;
}

bool SameFacePlaneSignature(const FacePlaneSignature& a, const FacePlaneSignature& b)
{
    return a.valid && b.valid && CompareFacePlaneSignature(a, b) == 0;
}

bool ContainsFacePlaneSignature(
    const std::vector<FacePlaneSignature>& signatures,
    const FacePlaneSignature& signature)
{
    for (const FacePlaneSignature& existing : signatures)
    {
        if (SameFacePlaneSignature(existing, signature))
        {
            return true;
        }
    }
    return false;
}

FacePairKey MakeFacePairKey(tag_t first, tag_t second)
{
    FacePairKey key;
    if (first <= second)
    {
        key.first = first;
        key.second = second;
    }
    else
    {
        key.first = second;
        key.second = first;
    }
    return key;
}

FacePairKey MakeFacePairKey(
    const LineProjectionFaceCandidate& first,
    const LineProjectionFaceCandidate& second)
{
    FacePairKey key;
    key.first = first.faceTag;
    key.second = second.faceTag;
    key.firstPlane = MakeFacePlaneSignature(first.normal, first.planePoint);
    key.secondPlane = MakeFacePlaneSignature(second.normal, second.planePoint);
    key.hasPlane = key.firstPlane.valid && key.secondPlane.valid;

    if (key.hasPlane)
    {
        const int planeOrder = CompareFacePlaneSignature(key.firstPlane, key.secondPlane);
        if (planeOrder > 0 || (planeOrder == 0 && key.first > key.second))
        {
            std::swap(key.first, key.second);
            std::swap(key.firstPlane, key.secondPlane);
        }
    }
    else if (key.first > key.second)
    {
        std::swap(key.first, key.second);
    }

    return key;
}

bool ContainsFacePairKey(const std::vector<FacePairKey>& keys, const FacePairKey& key)
{
    for (const FacePairKey& existing : keys)
    {
        if (existing.hasPlane && key.hasPlane &&
            SameFacePlaneSignature(existing.firstPlane, key.firstPlane) &&
            SameFacePlaneSignature(existing.secondPlane, key.secondPlane))
        {
            return true;
        }
        if ((existing.first == key.first && existing.second == key.second) ||
            (existing.first == key.second && existing.second == key.first))
        {
            return true;
        }
    }
    return false;
}

std::vector<tag_t> AskEdgeFaceTags(tag_t edgeTag);

bool TryMakeFacePlaneSignatureFromTag(tag_t faceTag, FacePlaneSignature& signature)
{
    signature = FacePlaneSignature();
    if (faceTag == NULL_TAG)
    {
        return false;
    }

    int faceType = 0;
    double point[3] = {0.0, 0.0, 0.0};
    double normalData[3] = {0.0, 0.0, 0.0};
    double box[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    double radius = 0.0;
    double radData = 0.0;
    int normDir = 1;
    if (UF_MODL_ask_face_data(faceTag, &faceType, point, normalData, box, &radius, &radData, &normDir) != 0 ||
        faceType != 22)
    {
        return false;
    }

    NXOpen::Vector3d normal(
        normalData[0] * static_cast<double>(normDir),
        normalData[1] * static_cast<double>(normDir),
        normalData[2] * static_cast<double>(normDir));
    normal = NormalizeVector(normal);
    if (VectorLength(normal) < 1.0e-6)
    {
        return false;
    }

    signature = MakeFacePlaneSignature(normal, NXOpen::Point3d(point[0], point[1], point[2]));
    return signature.valid;
}

void AppendFacePlaneSignatureFromTag(
    std::vector<FacePlaneSignature>& signatures,
    tag_t faceTag)
{
    FacePlaneSignature signature;
    if (TryMakeFacePlaneSignatureFromTag(faceTag, signature) &&
        !ContainsFacePlaneSignature(signatures, signature))
    {
        signatures.push_back(signature);
    }
}

std::vector<FacePlaneSignature> CollectClosedCurveParentFacePlanes(
    const std::vector<ClosedCurveLoopCandidate>& loops)
{
    std::vector<FacePlaneSignature> signatures;
    for (const ClosedCurveLoopCandidate& loop : loops)
    {
        for (const DraftingCurveExtent& extent : loop.extents)
        {
            if (extent.tag == NULL_TAG)
            {
                continue;
            }

            int parentCount = 0;
            tag_t* parents = nullptr;
            if (UF_DRAW_ask_drafting_curve_parents(extent.tag, &parentCount, &parents) != 0 ||
                parentCount <= 0 ||
                parents == nullptr)
            {
                continue;
            }

            for (int index = 0; index < parentCount; ++index)
            {
                const tag_t parentTag = parents[index];
                if (parentTag == NULL_TAG)
                {
                    continue;
                }

                const std::vector<tag_t> edgeFaces = AskEdgeFaceTags(parentTag);
                for (tag_t faceTag : edgeFaces)
                {
                    AppendFacePlaneSignatureFromTag(signatures, faceTag);
                }
            }

            UF_free(parents);
        }
    }

    return signatures;
}

struct InnerLoopAdjacentFaces
{
    std::vector<tag_t> faceTags;
    int innerLoopEdges = 0;
    int parentFacesWithInnerLoops = 0;
};

InnerLoopAdjacentFaces CollectModelInnerLoopAdjacentFaceTags(NXOpen::Part* part)
{
    InnerLoopAdjacentFaces result;
    if (part == nullptr)
    {
        return result;
    }

    const std::vector<tag_t> bodyTags = CollectVisibleSolidBodyTags(part);
    for (tag_t bodyTag : bodyTags)
    {
        uf_list_p_t faceList = nullptr;
        if (UF_MODL_ask_body_faces(bodyTag, &faceList) != 0 || faceList == nullptr)
        {
            continue;
        }

        int faceCount = 0;
        UF_MODL_ask_list_count(faceList, &faceCount);
        for (int faceIndex = 0; faceIndex < faceCount; ++faceIndex)
        {
            tag_t faceTag = NULL_TAG;
            if (UF_MODL_ask_list_item(faceList, faceIndex, &faceTag) != 0 || faceTag == NULL_TAG)
            {
                continue;
            }

            uf_loop_p_t loopList = nullptr;
            if (UF_MODL_ask_face_loops(faceTag, &loopList) != 0 || loopList == nullptr)
            {
                continue;
            }

            int loopCount = 0;
            UF_MODL_ask_loop_list_count(loopList, &loopCount);
            bool hasInnerLoop = false;
            for (int loopIndex = 0; loopIndex < loopCount; ++loopIndex)
            {
                int loopType = 0;
                uf_list_p_t edgeList = nullptr;
                if (UF_MODL_ask_loop_list_item(loopList, loopIndex, &loopType, &edgeList) != 0 ||
                    edgeList == nullptr ||
                    loopType != 2)
                {
                    continue;
                }

                hasInnerLoop = true;
                int edgeCount = 0;
                UF_MODL_ask_list_count(edgeList, &edgeCount);
                for (int edgeIndex = 0; edgeIndex < edgeCount; ++edgeIndex)
                {
                    tag_t edgeTag = NULL_TAG;
                    if (UF_MODL_ask_list_item(edgeList, edgeIndex, &edgeTag) != 0 || edgeTag == NULL_TAG)
                    {
                        continue;
                    }

                    ++result.innerLoopEdges;
                    const std::vector<tag_t> adjacentFaces = AskEdgeFaceTags(edgeTag);
                    for (tag_t adjacentFace : adjacentFaces)
                    {
                        if (adjacentFace != NULL_TAG &&
                            adjacentFace != faceTag &&
                            !ContainsTag(result.faceTags, adjacentFace))
                        {
                            result.faceTags.push_back(adjacentFace);
                        }
                    }
                }
            }

            if (hasInnerLoop)
            {
                ++result.parentFacesWithInnerLoops;
            }

            UF_MODL_delete_loop_list(&loopList);
        }

        UF_MODL_delete_list(&faceList);
    }

    return result;
}

MainViewFacePairRules BuildMainViewFacePairRules(
    NXOpen::Session* session,
    NXOpen::Part* workPart,
    const AutoViewDirection& frontDirection)
{
    MainViewFacePairRules rules;
    NXOpen::Vector3d viewX = NormalizeVector(frontDirection.xDirection);
    NXOpen::Vector3d viewZ = NormalizeVector(frontDirection.normal);
    viewX = NormalizeVector(ProjectPerpendicular(viewX, viewZ));
    NXOpen::Vector3d viewY = NormalizeVector(CrossVector(viewZ, viewX));
    if (VectorLength(viewX) < 1.0e-6 || VectorLength(viewY) < 1.0e-6 || VectorLength(viewZ) < 1.0e-6)
    {
        WriteLine(session, "AutoCreateThreeViews: main-view face-pair rules disabled; invalid front direction basis.");
        return rules;
    }

    rules.axes[0] = viewX;
    rules.axes[1] = viewY;
    rules.axes[2] = viewZ;
    rules.valid = true;
    if (!AskModelBoundsCenter(workPart, rules.center) &&
        !AskSolidBodyTagBoundsCenter(workPart, rules.center))
    {
        rules.center = NXOpen::Point3d(0.0, 0.0, 0.0);
        WriteLine(session, "AutoCreateThreeViews: main-view face-pair rules use origin as model center; body bounds unavailable.");
    }

    std::ostringstream log;
    log << "AutoCreateThreeViews: main-view face-pair rules enabled"
        << " centerX=" << rules.center.X
        << ", centerY=" << rules.center.Y
        << ", centerZ=" << rules.center.Z
        << ", xAxis=(" << rules.axes[0].X << "," << rules.axes[0].Y << "," << rules.axes[0].Z << ")"
        << ", yAxis=(" << rules.axes[1].X << "," << rules.axes[1].Y << "," << rules.axes[1].Z << ")"
        << ", zAxis=(" << rules.axes[2].X << "," << rules.axes[2].Y << "," << rules.axes[2].Z << ").";
    WriteLine(session, log.str());
    return rules;
}

bool TryClassifyMainViewAxisSide(
    const MainViewFacePairRules& rules,
    const LineProjectionFaceCandidate& face,
    int& axisIndex,
    int& side)
{
    if (!rules.valid)
    {
        return false;
    }

    const NXOpen::Vector3d normal = NormalizeVector(face.normal);
    if (VectorLength(normal) < 1.0e-6)
    {
        return false;
    }

    double bestAlignment = 0.0;
    int bestAxis = -1;
    for (int index = 0; index < 3; ++index)
    {
        const double alignment = std::abs(DotVector(normal, rules.axes[index]));
        if (alignment > bestAlignment)
        {
            bestAlignment = alignment;
            bestAxis = index;
        }
    }
    if (bestAxis < 0 || bestAlignment < 0.995)
    {
        return false;
    }

    const NXOpen::Vector3d centerToPlane(
        face.planePoint.X - rules.center.X,
        face.planePoint.Y - rules.center.Y,
        face.planePoint.Z - rules.center.Z);
    const double signedOffset = DotVector(centerToPlane, rules.axes[bestAxis]);
    const double zoneTolerance = 0.05;
    if (std::abs(signedOffset) <= zoneTolerance)
    {
        return false;
    }

    axisIndex = bestAxis;
    side = signedOffset > 0.0 ? 1 : -1;
    return true;
}

bool IsAllowedByMainViewFacePairRules(
    const MainViewFacePairRules* rules,
    const LineProjectionFaceCandidate& first,
    const LineProjectionFaceCandidate& second,
    bool pairMeasuresX,
    double drawingZoneCenter,
    std::string* rejectReason = nullptr)
{
    if (rules == nullptr || !rules->valid)
    {
        return true;
    }

    int firstAxis = -1;
    int secondAxis = -1;
    int firstSide = 0;
    int secondSide = 0;
    if (!TryClassifyMainViewAxisSide(*rules, first, firstAxis, firstSide) ||
        !TryClassifyMainViewAxisSide(*rules, second, secondAxis, secondSide))
    {
        if (rejectReason != nullptr)
        {
            *rejectReason = "notMainAxisParallelOrCenterPlane";
        }
        return false;
    }
    if (firstAxis != secondAxis)
    {
        if (rejectReason != nullptr)
        {
            *rejectReason = "differentMainAxis";
        }
        return false;
    }
    if (firstSide != secondSide)
    {
        // Main-view pairing is partitioned by the drawing zone. Opposite faces
        // can lie on different model-center sides while still sharing a valid
        // X-/Y-zone datum in the view.
    }

    const double firstDrawingOffset = pairMeasuresX
        ? first.centerX - drawingZoneCenter
        : first.centerY - drawingZoneCenter;
    const double secondDrawingOffset = pairMeasuresX
        ? second.centerX - drawingZoneCenter
        : second.centerY - drawingZoneCenter;
    const double drawingZoneTolerance = 0.05;
    if (std::abs(firstDrawingOffset) <= drawingZoneTolerance ||
        std::abs(secondDrawingOffset) <= drawingZoneTolerance)
    {
        if (rejectReason != nullptr)
        {
            *rejectReason = pairMeasuresX ? "nearDrawingXCenter" : "nearDrawingYCenter";
        }
        return false;
    }

    const int firstDrawingSide = firstDrawingOffset > 0.0 ? 1 : -1;
    const int secondDrawingSide = secondDrawingOffset > 0.0 ? 1 : -1;
    if (firstDrawingSide != secondDrawingSide)
    {
        if (rejectReason != nullptr)
        {
            *rejectReason = pairMeasuresX ? "differentDrawingXZone" : "differentDrawingYZone";
        }
        return false;
    }

    const double normalDot = DotVector(NormalizeVector(first.normal), NormalizeVector(second.normal));
    if (normalDot > -0.995)
    {
        if (rejectReason != nullptr)
        {
            *rejectReason = "notOppositeNormals";
        }
        return false;
    }

    return true;
}

bool UsesGloballyPairedFacePlane(
    const MainViewFacePairRules* rules,
    const LineProjectionFaceCandidate& first,
    const LineProjectionFaceCandidate& second)
{
    if (rules == nullptr || !rules->valid)
    {
        return false;
    }

    const FacePlaneSignature firstPlane = MakeFacePlaneSignature(first.normal, first.planePoint);
    const FacePlaneSignature secondPlane = MakeFacePlaneSignature(second.normal, second.planePoint);
    return ContainsFacePlaneSignature(rules->usedPairedPlanes, firstPlane) ||
           ContainsFacePlaneSignature(rules->usedPairedPlanes, secondPlane);
}

void RememberGloballyPairedFacePlanes(
    MainViewFacePairRules* rules,
    const LineProjectionFaceCandidate& first,
    const LineProjectionFaceCandidate& second)
{
    if (rules == nullptr || !rules->valid)
    {
        return;
    }

    const FacePlaneSignature firstPlane = MakeFacePlaneSignature(first.normal, first.planePoint);
    const FacePlaneSignature secondPlane = MakeFacePlaneSignature(second.normal, second.planePoint);
    if (firstPlane.valid && !ContainsFacePlaneSignature(rules->usedPairedPlanes, firstPlane))
    {
        rules->usedPairedPlanes.push_back(firstPlane);
    }
    if (secondPlane.valid && !ContainsFacePlaneSignature(rules->usedPairedPlanes, secondPlane))
    {
        rules->usedPairedPlanes.push_back(secondPlane);
    }
}

void MarkOuterContourDatumFaces(
    std::vector<LineProjectionFaceCandidate>& faces,
    const LayoutBounds& bounds,
    double axisTolerance)
{
    for (LineProjectionFaceCandidate& face : faces)
    {
        face.outerContourDatum = false;
    }

    if (faces.empty())
    {
        return;
    }

    const double sideTolerance = std::max(0.01, std::min(axisTolerance * 0.25, 0.05));
    const auto isNearValue = [sideTolerance](double a, double b) {
        return std::abs(a - b) <= sideTolerance;
    };

    bool hasVertical = false;
    bool hasHorizontal = false;
    bool hasAngled = false;
    double minVerticalX = std::numeric_limits<double>::max();
    double maxVerticalX = -std::numeric_limits<double>::max();
    double minHorizontalY = std::numeric_limits<double>::max();
    double maxHorizontalY = -std::numeric_limits<double>::max();
    double minAngledX = std::numeric_limits<double>::max();
    double maxAngledX = -std::numeric_limits<double>::max();
    double minAngledY = std::numeric_limits<double>::max();
    double maxAngledY = -std::numeric_limits<double>::max();

    for (const LineProjectionFaceCandidate& face : faces)
    {
        if (face.verticalLine)
        {
            hasVertical = true;
            minVerticalX = std::min(minVerticalX, face.centerX);
            maxVerticalX = std::max(maxVerticalX, face.centerX);
        }
        else if (face.horizontalLine)
        {
            hasHorizontal = true;
            minHorizontalY = std::min(minHorizontalY, face.centerY);
            maxHorizontalY = std::max(maxHorizontalY, face.centerY);
        }
        else if (face.angledLine)
        {
            hasAngled = true;
            minAngledX = std::min(minAngledX, face.minX);
            maxAngledX = std::max(maxAngledX, face.maxX);
            minAngledY = std::min(minAngledY, face.minY);
            maxAngledY = std::max(maxAngledY, face.maxY);
        }
    }

    for (LineProjectionFaceCandidate& face : faces)
    {
        if (face.verticalLine)
        {
            const bool leftMostFace = hasVertical && isNearValue(face.centerX, minVerticalX) && isNearValue(face.centerX, bounds.minX);
            const bool rightMostFace = hasVertical && isNearValue(face.centerX, maxVerticalX) && isNearValue(face.centerX, bounds.maxX);
            face.outerContourDatum = leftMostFace || rightMostFace;
        }
        else if (face.horizontalLine)
        {
            const bool bottomMostFace = hasHorizontal && isNearValue(face.centerY, minHorizontalY) && isNearValue(face.centerY, bounds.minY);
            const bool topMostFace = hasHorizontal && isNearValue(face.centerY, maxHorizontalY) && isNearValue(face.centerY, bounds.maxY);
            face.outerContourDatum = bottomMostFace || topMostFace;
        }
        else if (face.angledLine)
        {
            const bool touchesCandidateOuter =
                (hasAngled && isNearValue(face.minX, minAngledX) && isNearValue(face.minX, bounds.minX)) ||
                (hasAngled && isNearValue(face.maxX, maxAngledX) && isNearValue(face.maxX, bounds.maxX)) ||
                (hasAngled && isNearValue(face.minY, minAngledY) && isNearValue(face.minY, bounds.minY)) ||
                (hasAngled && isNearValue(face.maxY, maxAngledY) && isNearValue(face.maxY, bounds.maxY));
            face.outerContourDatum = touchesCandidateOuter;
        }
    }
}

double Dot2D(double ax, double ay, double bx, double by)
{
    return ax * bx + ay * by;
}

double Normalize2D(double& x, double& y)
{
    const double length = std::sqrt(x * x + y * y);
    if (length > 1.0e-9)
    {
        x /= length;
        y /= length;
    }
    return length;
}

bool PreferSamePlaneFaceCandidate(
    const LineProjectionFaceCandidate& current,
    const LineProjectionFaceCandidate& candidate)
{
    if (current.outerContourDatum != candidate.outerContourDatum)
    {
        return candidate.outerContourDatum;
    }
    if (std::abs(current.length - candidate.length) > 0.01)
    {
        return candidate.length > current.length;
    }
    const double currentSpan = BoundsWidth(LayoutBounds{current.minX, current.minY, current.maxX, current.maxY}) +
        BoundsHeight(LayoutBounds{current.minX, current.minY, current.maxX, current.maxY});
    const double candidateSpan = BoundsWidth(LayoutBounds{candidate.minX, candidate.minY, candidate.maxX, candidate.maxY}) +
        BoundsHeight(LayoutBounds{candidate.minX, candidate.minY, candidate.maxX, candidate.maxY});
    return candidateSpan > currentSpan;
}

bool TryComputeModelFaceGap(
    const LineProjectionFaceCandidate& first,
    const LineProjectionFaceCandidate& second,
    double& gap)
{
    NXOpen::Vector3d normal = NormalizeVector(first.normal);
    if (VectorLength(normal) < 1.0e-6)
    {
        return false;
    }

    gap = std::abs(
        normal.X * (second.planePoint.X - first.planePoint.X) +
        normal.Y * (second.planePoint.Y - first.planePoint.Y) +
        normal.Z * (second.planePoint.Z - first.planePoint.Z));
    return gap > 1.0e-6;
}

bool IsPlateThicknessGap(double modelGap, double sheetMetalThickness)
{
    if (modelGap <= 1.0e-6 || sheetMetalThickness <= 1.0e-6)
    {
        return false;
    }

    const double tolerance = std::max(0.03, sheetMetalThickness * 0.03);
    return std::abs(modelGap - sheetMetalThickness) <= tolerance;
}

bool IsSmallDimensionGap(double modelGap, double sheetMetalThickness)
{
    if (modelGap <= 1.0e-6)
    {
        return false;
    }

    return modelGap < 5.0;
}

bool IsPlateThicknessOffsetFromUsedFace(
    const LineProjectionFaceCandidate& candidate,
    const std::vector<LineProjectionFaceCandidate>& usedFaces,
    double sheetMetalThickness,
    bool allowOuterDatumCandidate = false)
{
    if ((!allowOuterDatumCandidate && candidate.outerContourDatum) || sheetMetalThickness <= 1.0e-6)
    {
        return false;
    }

    for (const LineProjectionFaceCandidate& usedFace : usedFaces)
    {
        const double normalAlignment =
            std::abs(DotVector(NormalizeVector(candidate.normal), NormalizeVector(usedFace.normal)));
        if (normalAlignment < 0.995)
        {
            continue;
        }

        double modelGap = 0.0;
        if (TryComputeModelFaceGap(candidate, usedFace, modelGap) &&
            IsPlateThicknessGap(modelGap, sheetMetalThickness))
        {
            return true;
        }
    }

    return false;
}

bool ContainsSamePlaneFaceCandidate(
    const std::vector<LineProjectionFaceCandidate>& faces,
    const LineProjectionFaceCandidate& candidate)
{
    const FacePlaneSignature candidateSignature = MakeFacePlaneSignature(candidate.normal, candidate.planePoint);
    if (!candidateSignature.valid)
    {
        return false;
    }

    for (const LineProjectionFaceCandidate& face : faces)
    {
        const FacePlaneSignature faceSignature = MakeFacePlaneSignature(face.normal, face.planePoint);
        if (SameFacePlaneSignature(faceSignature, candidateSignature))
        {
            return true;
        }
    }
    return false;
}

void AppendUniquePlaneFaceCandidate(
    std::vector<LineProjectionFaceCandidate>& faces,
    const LineProjectionFaceCandidate& candidate)
{
    if (!ContainsSamePlaneFaceCandidate(faces, candidate))
    {
        faces.push_back(candidate);
    }
}

std::vector<LineProjectionFaceCandidate> CollectOverallBoundaryDatumFaces(
    const std::vector<LineProjectionFaceCandidate>& faces,
    const LayoutBounds& bounds,
    bool horizontalMeasurement,
    double axisTolerance)
{
    std::vector<LineProjectionFaceCandidate> datumFaces;
    const double span = std::max(BoundsWidth(bounds), BoundsHeight(bounds));
    const double boundaryTolerance =
        std::max(0.03, std::min(0.08, std::max(axisTolerance * 0.25, span * 0.0015)));
    const double lowTarget = horizontalMeasurement ? bounds.minX : bounds.minY;
    const double highTarget = horizontalMeasurement ? bounds.maxX : bounds.maxY;

    for (const LineProjectionFaceCandidate& face : faces)
    {
        if (horizontalMeasurement)
        {
            if (!face.verticalLine)
            {
                continue;
            }
        }
        else if (!face.horizontalLine)
        {
            continue;
        }

        const double coord = horizontalMeasurement ? face.centerX : face.centerY;
        if (std::abs(coord - lowTarget) <= boundaryTolerance ||
            std::abs(coord - highTarget) <= boundaryTolerance)
        {
            AppendUniquePlaneFaceCandidate(datumFaces, face);
        }
    }

    return datumFaces;
}

std::vector<LineProjectionFaceCandidate> CollectPlateThicknessOffsetFaces(
    const std::vector<LineProjectionFaceCandidate>& faces,
    const std::vector<LineProjectionFaceCandidate>& datumFaces,
    double sheetMetalThickness)
{
    std::vector<LineProjectionFaceCandidate> excludedFaces;
    if (sheetMetalThickness <= 1.0e-6 || datumFaces.empty())
    {
        return excludedFaces;
    }

    for (const LineProjectionFaceCandidate& candidate : faces)
    {
        if (ContainsSamePlaneFaceCandidate(datumFaces, candidate))
        {
            continue;
        }

        for (const LineProjectionFaceCandidate& datumFace : datumFaces)
        {
            const double normalAlignment =
                std::abs(DotVector(NormalizeVector(candidate.normal), NormalizeVector(datumFace.normal)));
            if (normalAlignment < 0.995)
            {
                continue;
            }

            double modelGap = 0.0;
            if (TryComputeModelFaceGap(candidate, datumFace, modelGap) &&
                IsPlateThicknessGap(modelGap, sheetMetalThickness))
            {
                AppendUniquePlaneFaceCandidate(excludedFaces, candidate);
                break;
            }
        }
    }

    return excludedFaces;
}

bool TryReadPlanarFacePlane(tag_t faceTag, NXOpen::Vector3d& normal, NXOpen::Point3d& pointOnPlane)
{
    return TryReadPlanarFacePointAndNormal(faceTag, &pointOnPlane, normal);
}

std::vector<tag_t> AskBodyFaceTagsForFace(tag_t faceTag)
{
    std::vector<tag_t> faceTags;
    if (faceTag == NULL_TAG)
    {
        return faceTags;
    }

    tag_t bodyTag = NULL_TAG;
    if (UF_MODL_ask_face_body(faceTag, &bodyTag) != 0 || bodyTag == NULL_TAG)
    {
        return faceTags;
    }

    uf_list_p_t faceList = nullptr;
    if (UF_MODL_ask_body_faces(bodyTag, &faceList) != 0 || faceList == nullptr)
    {
        return faceTags;
    }

    int faceCount = 0;
    UF_MODL_ask_list_count(faceList, &faceCount);
    for (int index = 0; index < faceCount; ++index)
    {
        tag_t bodyFaceTag = NULL_TAG;
        if (UF_MODL_ask_list_item(faceList, index, &bodyFaceTag) == 0 &&
            bodyFaceTag != NULL_TAG &&
            !ContainsTag(faceTags, bodyFaceTag))
        {
            faceTags.push_back(bodyFaceTag);
        }
    }

    UF_MODL_delete_list(&faceList);
    return faceTags;
}

std::vector<tag_t> AskEdgeFaceTags(tag_t edgeTag)
{
    std::vector<tag_t> faceTags;
    if (edgeTag == NULL_TAG)
    {
        return faceTags;
    }

    uf_list_p_t faceList = nullptr;
    if (UF_MODL_ask_edge_faces(edgeTag, &faceList) != 0 || faceList == nullptr)
    {
        return faceTags;
    }

    int faceCount = 0;
    UF_MODL_ask_list_count(faceList, &faceCount);
    for (int index = 0; index < faceCount; ++index)
    {
        tag_t faceTag = NULL_TAG;
        if (UF_MODL_ask_list_item(faceList, index, &faceTag) == 0 &&
            faceTag != NULL_TAG &&
            !ContainsTag(faceTags, faceTag))
        {
            faceTags.push_back(faceTag);
        }
    }

    UF_MODL_delete_list(&faceList);
    return faceTags;
}

struct CachedPlanarFaceInfo
{
    tag_t faceTag = NULL_TAG;
    NXOpen::Vector3d normal = NXOpen::Vector3d(0.0, 0.0, 0.0);
    NXOpen::Point3d point = NXOpen::Point3d(0.0, 0.0, 0.0);
    double area = 0.0;
};

bool IsShallowDetailBottomFaceCached(
    const CachedPlanarFaceInfo& candidate,
    const std::vector<CachedPlanarFaceInfo>& bodyFaces)
{
    if (candidate.faceTag == NULL_TAG || candidate.area <= 1.0e-6)
    {
        return false;
    }

    const double shallowDepthLimit = 1.0;
    const double minDepth = 0.02;
    const NXOpen::Vector3d candidateNormal = NormalizeVector(candidate.normal);
    for (const CachedPlanarFaceInfo& other : bodyFaces)
    {
        if (other.faceTag == NULL_TAG || other.faceTag == candidate.faceTag || other.area <= 1.0e-6)
        {
            continue;
        }
        if (std::abs(DotVector(candidateNormal, NormalizeVector(other.normal))) < 0.995)
        {
            continue;
        }

        const double depth = std::abs(
            candidateNormal.X * (other.point.X - candidate.point.X) +
            candidateNormal.Y * (other.point.Y - candidate.point.Y) +
            candidateNormal.Z * (other.point.Z - candidate.point.Z));
        if (depth > minDepth &&
            depth < shallowDepthLimit &&
            other.area >= candidate.area * 1.20)
        {
            return true;
        }
    }

    return false;
}

bool IsShallowDetailSideWallFaceCached(
    const CachedPlanarFaceInfo& candidate,
    const std::map<tag_t, CachedPlanarFaceInfo>& bodyFaceMap)
{
    if (candidate.faceTag == NULL_TAG || candidate.area <= 1.0e-6)
    {
        return false;
    }

    const std::vector<tag_t> adjacentFaces = AskAdjacentFaceTags(candidate.faceTag);
    if (adjacentFaces.size() < 2)
    {
        return false;
    }

    std::vector<CachedPlanarFaceInfo> adjacentPlanes;
    for (tag_t adjacentTag : adjacentFaces)
    {
        const auto it = bodyFaceMap.find(adjacentTag);
        if (it != bodyFaceMap.end())
        {
            adjacentPlanes.push_back(it->second);
        }
    }
    if (adjacentPlanes.size() < 2)
    {
        return false;
    }

    const NXOpen::Vector3d candidateNormal = NormalizeVector(candidate.normal);
    const double shallowDepthLimit = 1.0;
    const double minDepth = 0.02;
    for (size_t i = 0; i < adjacentPlanes.size(); ++i)
    {
        for (size_t j = i + 1; j < adjacentPlanes.size(); ++j)
        {
            const CachedPlanarFaceInfo& first = adjacentPlanes[i];
            const CachedPlanarFaceInfo& second = adjacentPlanes[j];
            const NXOpen::Vector3d firstNormal = NormalizeVector(first.normal);
            const NXOpen::Vector3d secondNormal = NormalizeVector(second.normal);
            if (std::abs(DotVector(firstNormal, secondNormal)) < 0.995)
            {
                continue;
            }
            if (std::abs(DotVector(candidateNormal, firstNormal)) > 0.35)
            {
                continue;
            }

            const double depth = std::abs(
                firstNormal.X * (second.point.X - first.point.X) +
                firstNormal.Y * (second.point.Y - first.point.Y) +
                firstNormal.Z * (second.point.Z - first.point.Z));
            if (depth > minDepth &&
                depth < shallowDepthLimit &&
                std::max(first.area, second.area) >= std::max(candidate.area * 1.50, 5.0))
            {
                return true;
            }
        }
    }

    return false;
}

void EnsureShallowDetailFilterCache(NXOpen::Part* part, ShallowDetailFilterCache& cache)
{
    if (cache.initialized && cache.part == part)
    {
        return;
    }

    cache.part = part;
    cache.initialized = true;
    cache.shallowFaceTags.clear();
    cache.edgeFaceTags.clear();
    if (part == nullptr || part->Bodies() == nullptr)
    {
        return;
    }

    for (NXOpen::BodyCollection::iterator it = part->Bodies()->begin(); it != part->Bodies()->end(); ++it)
    {
        NXOpen::Body* body = *it;
        if (body == nullptr || body->IsBlanked())
        {
            continue;
        }

        uf_list_p_t faceList = nullptr;
        if (UF_MODL_ask_body_faces(body->Tag(), &faceList) != 0 || faceList == nullptr)
        {
            continue;
        }

        std::vector<CachedPlanarFaceInfo> bodyFaces;
        std::map<tag_t, CachedPlanarFaceInfo> bodyFaceMap;
        int faceCount = 0;
        UF_MODL_ask_list_count(faceList, &faceCount);
        for (int faceIndex = 0; faceIndex < faceCount; ++faceIndex)
        {
            tag_t faceTag = NULL_TAG;
            if (UF_MODL_ask_list_item(faceList, faceIndex, &faceTag) != 0 || faceTag == NULL_TAG)
            {
                continue;
            }

            CachedPlanarFaceInfo info;
            info.faceTag = faceTag;
            if (!TryReadPlanarFacePlane(faceTag, info.normal, info.point))
            {
                continue;
            }
            info.area = AskPlanarFaceArea(part, faceTag);
            if (info.area <= 1.0e-6)
            {
                continue;
            }
            bodyFaces.push_back(info);
            bodyFaceMap[faceTag] = info;
        }
        UF_MODL_delete_list(&faceList);

        if (bodyFaces.size() < 2)
        {
            continue;
        }

        const CachedPlanarFaceInfo* largestFace = &bodyFaces.front();
        for (const CachedPlanarFaceInfo& face : bodyFaces)
        {
            if (face.area > largestFace->area)
            {
                largestFace = &face;
            }
        }

        const NXOpen::Vector3d largestNormal = NormalizeVector(largestFace->normal);
        double bodyThickness = 0.0;
        const double minimumParallelArea = largestFace->area * 0.5;
        for (const CachedPlanarFaceInfo& face : bodyFaces)
        {
            if (face.faceTag == largestFace->faceTag || face.area < minimumParallelArea)
            {
                continue;
            }

            if (std::abs(DotVector(largestNormal, NormalizeVector(face.normal))) < 0.995)
            {
                continue;
            }

            const double gap = std::abs(
                largestNormal.X * (face.point.X - largestFace->point.X) +
                largestNormal.Y * (face.point.Y - largestFace->point.Y) +
                largestNormal.Z * (face.point.Z - largestFace->point.Z));
            if (gap <= 1.0e-6)
            {
                continue;
            }

            if (bodyThickness <= 1.0e-6 || gap < bodyThickness)
            {
                bodyThickness = gap;
            }
        }

        if (bodyThickness <= 1.0e-6)
        {
            continue;
        }

        std::set<tag_t> shallowBottomFaceTags;
        std::set<tag_t> outerSurfaceFaceTags;
        const double shallowDepthLimit = 1.0;
        const double minDepth = 0.02;
        for (const CachedPlanarFaceInfo& candidate : bodyFaces)
        {
            const NXOpen::Vector3d candidateNormal = NormalizeVector(candidate.normal);
            if (VectorLength(candidateNormal) < 1.0e-6)
            {
                continue;
            }

            for (const CachedPlanarFaceInfo& outerFace : bodyFaces)
            {
                if (outerFace.faceTag == candidate.faceTag ||
                    outerFace.area < std::max(candidate.area * 1.20, 1.0))
                {
                    continue;
                }

                const NXOpen::Vector3d outerNormal = NormalizeVector(outerFace.normal);
                if (std::abs(DotVector(candidateNormal, outerNormal)) < 0.995)
                {
                    continue;
                }

                const double depth = std::abs(
                    outerNormal.X * (candidate.point.X - outerFace.point.X) +
                    outerNormal.Y * (candidate.point.Y - outerFace.point.Y) +
                    outerNormal.Z * (candidate.point.Z - outerFace.point.Z));
                if (depth > minDepth &&
                    depth < shallowDepthLimit &&
                    std::abs(depth - bodyThickness) > std::max(0.03, bodyThickness * 0.08))
                {
                    shallowBottomFaceTags.insert(candidate.faceTag);
                    outerSurfaceFaceTags.insert(outerFace.faceTag);
                    break;
                }
            }
        }

        for (tag_t bottomFaceTag : shallowBottomFaceTags)
        {
            cache.shallowFaceTags.insert(bottomFaceTag);

            uf_list_p_t edgeList = nullptr;
            if (UF_MODL_ask_face_edges(bottomFaceTag, &edgeList) != 0 || edgeList == nullptr)
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

                std::vector<tag_t> edgeFaces = AskEdgeFaceTags(edgeTag);
                cache.edgeFaceTags[edgeTag] = edgeFaces;
                for (tag_t connectedFaceTag : edgeFaces)
                {
                    if (connectedFaceTag == NULL_TAG ||
                        outerSurfaceFaceTags.find(connectedFaceTag) != outerSurfaceFaceTags.end())
                    {
                        continue;
                    }

                    if (bodyFaceMap.find(connectedFaceTag) != bodyFaceMap.end())
                    {
                        cache.shallowFaceTags.insert(connectedFaceTag);
                    }
                }
            }

            UF_MODL_delete_list(&edgeList);
        }
    }
}

std::vector<tag_t> AskEdgeFaceTagsCached(tag_t edgeTag, ShallowDetailFilterCache& cache)
{
    const auto found = cache.edgeFaceTags.find(edgeTag);
    if (found != cache.edgeFaceTags.end())
    {
        return found->second;
    }

    std::vector<tag_t> faces = AskEdgeFaceTags(edgeTag);
    cache.edgeFaceTags[edgeTag] = faces;
    return faces;
}

bool IsCachedShallowDetailFace(NXOpen::Part* part, ShallowDetailFilterCache& cache, tag_t faceTag)
{
    EnsureShallowDetailFilterCache(part, cache);
    return faceTag != NULL_TAG && cache.shallowFaceTags.find(faceTag) != cache.shallowFaceTags.end();
}

bool IsShallowDetailBottomFace(
    NXOpen::Part* part,
    tag_t faceTag,
    const NXOpen::Vector3d& faceNormal,
    const NXOpen::Point3d& pointOnPlane,
    double faceArea,
    const std::vector<tag_t>& bodyFaceTags)
{
    if (part == nullptr || faceTag == NULL_TAG || faceArea <= 1.0e-6)
    {
        return false;
    }

    const NXOpen::Vector3d candidateNormal = NormalizeVector(faceNormal);
    if (VectorLength(candidateNormal) < 1.0e-6)
    {
        return false;
    }

    const double shallowDepthLimit = 1.0;
    const double minDepth = 0.02;
    for (tag_t otherFaceTag : bodyFaceTags)
    {
        if (otherFaceTag == NULL_TAG || otherFaceTag == faceTag)
        {
            continue;
        }

        NXOpen::Vector3d otherNormal(0.0, 0.0, 0.0);
        NXOpen::Point3d otherPoint(0.0, 0.0, 0.0);
        if (!TryReadPlanarFacePlane(otherFaceTag, otherNormal, otherPoint))
        {
            continue;
        }

        if (std::abs(DotVector(candidateNormal, NormalizeVector(otherNormal))) < 0.995)
        {
            continue;
        }

        const double depth = std::abs(
            candidateNormal.X * (otherPoint.X - pointOnPlane.X) +
            candidateNormal.Y * (otherPoint.Y - pointOnPlane.Y) +
            candidateNormal.Z * (otherPoint.Z - pointOnPlane.Z));
        if (depth <= minDepth || depth >= shallowDepthLimit)
        {
            continue;
        }

        const double otherArea = AskPlanarFaceArea(part, otherFaceTag);
        if (otherArea >= faceArea * 1.20)
        {
            return true;
        }
    }

    return false;
}

bool IsShallowDetailSideWallFace(
    NXOpen::Part* part,
    tag_t faceTag,
    const NXOpen::Vector3d& faceNormal,
    double faceArea)
{
    if (part == nullptr || faceTag == NULL_TAG || faceArea <= 1.0e-6)
    {
        return false;
    }

    const NXOpen::Vector3d candidateNormal = NormalizeVector(faceNormal);
    if (VectorLength(candidateNormal) < 1.0e-6)
    {
        return false;
    }

    const std::vector<tag_t> adjacentFaces = AskAdjacentFaceTags(faceTag);
    if (adjacentFaces.size() < 2)
    {
        return false;
    }

    struct AdjacentPlane
    {
        tag_t faceTag = NULL_TAG;
        NXOpen::Vector3d normal = NXOpen::Vector3d(0.0, 0.0, 0.0);
        NXOpen::Point3d point = NXOpen::Point3d(0.0, 0.0, 0.0);
        double area = 0.0;
    };

    std::vector<AdjacentPlane> planes;
    for (tag_t adjacentTag : adjacentFaces)
    {
        AdjacentPlane plane;
        plane.faceTag = adjacentTag;
        if (!TryReadPlanarFacePlane(adjacentTag, plane.normal, plane.point))
        {
            continue;
        }
        plane.area = AskPlanarFaceArea(part, adjacentTag);
        if (plane.area <= 1.0e-6)
        {
            continue;
        }
        planes.push_back(plane);
    }

    const double shallowDepthLimit = 1.0;
    const double minDepth = 0.02;
    for (size_t i = 0; i < planes.size(); ++i)
    {
        for (size_t j = i + 1; j < planes.size(); ++j)
        {
            const AdjacentPlane& first = planes[i];
            const AdjacentPlane& second = planes[j];
            const NXOpen::Vector3d firstNormal = NormalizeVector(first.normal);
            const NXOpen::Vector3d secondNormal = NormalizeVector(second.normal);
            if (std::abs(DotVector(firstNormal, secondNormal)) < 0.995)
            {
                continue;
            }

            if (std::abs(DotVector(candidateNormal, firstNormal)) > 0.35)
            {
                continue;
            }

            const double depth = std::abs(
                firstNormal.X * (second.point.X - first.point.X) +
                firstNormal.Y * (second.point.Y - first.point.Y) +
                firstNormal.Z * (second.point.Z - first.point.Z));
            if (depth <= minDepth || depth >= shallowDepthLimit)
            {
                continue;
            }

            const double largerAdjacentArea = std::max(first.area, second.area);
            if (largerAdjacentArea >= std::max(faceArea * 1.50, 5.0))
            {
                return true;
            }
        }
    }

    return false;
}

bool IsShallowGrooveOrEngravingFaceTag(
    NXOpen::Part* part,
    tag_t faceTag,
    const std::vector<tag_t>& bodyFaceTags)
{
    if (part == nullptr || faceTag == NULL_TAG)
    {
        return false;
    }

    NXOpen::Vector3d normal(0.0, 0.0, 0.0);
    NXOpen::Point3d pointOnPlane(0.0, 0.0, 0.0);
    if (!TryReadPlanarFacePlane(faceTag, normal, pointOnPlane))
    {
        return false;
    }

    const double area = AskPlanarFaceArea(part, faceTag);
    if (area <= 1.0e-6)
    {
        return false;
    }

    return IsShallowDetailBottomFace(part, faceTag, normal, pointOnPlane, area, bodyFaceTags) ||
        IsShallowDetailSideWallFace(part, faceTag, normal, area);
}

bool IsShallowGrooveOrEngravingFaceTag(NXOpen::Part* part, tag_t faceTag)
{
    if (part == nullptr || faceTag == NULL_TAG)
    {
        return false;
    }

    const std::vector<tag_t> bodyFaceTags = AskBodyFaceTagsForFace(faceTag);
    return IsShallowGrooveOrEngravingFaceTag(part, faceTag, bodyFaceTags);
}

bool IsShallowGrooveOrEngravingFace(
    NXOpen::Part* part,
    const LineProjectionFaceCandidate& candidate,
    const std::vector<tag_t>& bodyFaceTags)
{
    if (part == nullptr || candidate.faceTag == NULL_TAG)
    {
        return false;
    }

    const double candidateArea = AskPlanarFaceArea(part, candidate.faceTag);
    if (candidateArea <= 1.0e-6)
    {
        return false;
    }

    return IsShallowDetailBottomFace(
            part,
            candidate.faceTag,
            candidate.normal,
            candidate.planePoint,
            candidateArea,
            bodyFaceTags) ||
        IsShallowDetailSideWallFace(part, candidate.faceTag, candidate.normal, candidateArea);
}

bool DraftingCurveHasShallowDetailParent(
    NXOpen::Part* part,
    ShallowDetailFilterCache& cache,
    const DraftingCurveExtent& extent)
{
    if (part == nullptr || extent.tag == NULL_TAG)
    {
        return false;
    }

    int parentCount = 0;
    tag_t* parents = nullptr;
    if (UF_DRAW_ask_drafting_curve_parents(extent.tag, &parentCount, &parents) != 0 ||
        parentCount <= 0 ||
        parents == nullptr)
    {
        return false;
    }

    bool matched = false;
    for (int index = 0; index < parentCount && !matched; ++index)
    {
        const tag_t parentTag = parents[index];
        if (parentTag == NULL_TAG)
        {
            continue;
        }

        if (IsCachedShallowDetailFace(part, cache, parentTag))
        {
            matched = true;
            break;
        }

        const std::vector<tag_t> edgeFaces = AskEdgeFaceTagsCached(parentTag, cache);
        for (tag_t faceTag : edgeFaces)
        {
            if (IsCachedShallowDetailFace(part, cache, faceTag))
            {
                matched = true;
                break;
            }
        }
    }

    UF_free(parents);
    return matched;
}

std::vector<LineProjectionFaceCandidate> CollapseSamePlaneFaceCandidates(
    const std::vector<LineProjectionFaceCandidate>& faces)
{
    std::vector<LineProjectionFaceCandidate> collapsed;
    for (const LineProjectionFaceCandidate& face : faces)
    {
        const FacePlaneSignature signature = MakeFacePlaneSignature(face.normal, face.planePoint);
        if (!signature.valid)
        {
            collapsed.push_back(face);
            continue;
        }

        bool merged = false;
        for (LineProjectionFaceCandidate& existing : collapsed)
        {
            const FacePlaneSignature existingSignature =
                MakeFacePlaneSignature(existing.normal, existing.planePoint);
            if (!SameFacePlaneSignature(existingSignature, signature))
            {
                continue;
            }
            const double overlapTolerance = 0.05;
            const bool projectedRegionsOverlap =
                existing.maxX >= face.minX - overlapTolerance &&
                face.maxX >= existing.minX - overlapTolerance &&
                existing.maxY >= face.minY - overlapTolerance &&
                face.maxY >= existing.minY - overlapTolerance;
            if (!projectedRegionsOverlap)
            {
                continue;
            }

            if (PreferSamePlaneFaceCandidate(existing, face))
            {
                existing = face;
            }
            merged = true;
            break;
        }

        if (!merged)
        {
            collapsed.push_back(face);
        }
    }
    return collapsed;
}

bool DraftingCurveHasParentTag(tag_t draftingCurveTag, tag_t faceTag, const std::vector<tag_t>& edgeTags)
{
    if (draftingCurveTag == NULL_TAG || faceTag == NULL_TAG)
    {
        return false;
    }

    int parentCount = 0;
    tag_t* parents = nullptr;
    if (UF_DRAW_ask_drafting_curve_parents(draftingCurveTag, &parentCount, &parents) != 0 ||
        parentCount <= 0 ||
        parents == nullptr)
    {
        return false;
    }

    bool matched = false;
    for (int index = 0; index < parentCount; ++index)
    {
        const tag_t parentTag = parents[index];
        if (parentTag == faceTag || ContainsTag(edgeTags, parentTag))
        {
            matched = true;
            break;
        }

        const std::vector<tag_t> parentEdgeFaces = AskEdgeFaceTags(parentTag);
        if (ContainsTag(parentEdgeFaces, faceTag))
        {
            matched = true;
            break;
        }
    }

    UF_free(parents);
    return matched;
}

bool MatchLineProjectionFaceToCurve(
    const LineProjectionFaceCandidate& faceCandidate,
    const std::vector<LineSegmentCandidate>& lines,
    double axisTolerance,
    LineSegmentCandidate& matchedLine)
{
    double bestScore = -std::numeric_limits<double>::max();
    bool matched = false;
    const double centerTolerance = std::max(0.6, axisTolerance * 4.0);
    const double minimumOverlap = std::max(1.0, faceCandidate.length * 0.35);

    for (const LineSegmentCandidate& line : lines)
    {
        if (line.length <= 1.0e-6)
        {
            continue;
        }
        double lineDirectionX = line.endX - line.startX;
        double lineDirectionY = line.endY - line.startY;
        if (Normalize2D(lineDirectionX, lineDirectionY) <= 1.0e-6)
        {
            continue;
        }

        const double directionAlignment =
            std::abs(Dot2D(lineDirectionX, lineDirectionY, faceCandidate.directionX, faceCandidate.directionY));
        if (directionAlignment < 0.996)
        {
            continue;
        }

        const double lineCenterX = (line.startX + line.endX) * 0.5;
        const double lineCenterY = (line.startY + line.endY) * 0.5;
        const double centerDistance = std::abs(
            faceCandidate.normalX * (lineCenterX - faceCandidate.centerX) +
            faceCandidate.normalY * (lineCenterY - faceCandidate.centerY));
        if (centerDistance > centerTolerance)
        {
            continue;
        }

        const double lineStartAlong =
            line.startX * faceCandidate.directionX + line.startY * faceCandidate.directionY;
        const double lineEndAlong =
            line.endX * faceCandidate.directionX + line.endY * faceCandidate.directionY;
        const double lineMinAlong = std::min(lineStartAlong, lineEndAlong);
        const double lineMaxAlong = std::max(lineStartAlong, lineEndAlong);
        const double overlap = RangeOverlap(lineMinAlong, lineMaxAlong, faceCandidate.minAlong, faceCandidate.maxAlong);
        const double lengthDelta = std::abs(line.length - faceCandidate.length);

        if (overlap < minimumOverlap)
        {
            continue;
        }

        const double score = overlap - centerDistance * 10.0 - lengthDelta * 0.03;
        if (score > bestScore)
        {
            bestScore = score;
            matchedLine = line;
            matched = true;
        }
    }

    return matched;
}

bool TryBuildLineProjectionFaceCandidate(
    NXOpen::Drawings::DraftingView* view,
    tag_t faceTag,
    const std::vector<LineSegmentCandidate>& lines,
    double axisTolerance,
    LineProjectionFaceCandidate& candidate)
{
    if (view == nullptr || faceTag == NULL_TAG)
    {
        return false;
    }

    NXOpen::Vector3d faceNormal(0.0, 0.0, 0.0);
    NXOpen::Point3d facePoint(0.0, 0.0, 0.0);
    if (!TryReadPlanarFacePlane(faceTag, faceNormal, facePoint))
    {
        return false;
    }

    uf_list_p_t edgeList = nullptr;
    if (UF_MODL_ask_face_edges(faceTag, &edgeList) != 0 || edgeList == nullptr)
    {
        return false;
    }

    bool initialized = false;
    LayoutBounds projectedBounds;
    std::vector<NXOpen::Point3d> projectedPoints;
    std::vector<tag_t> faceEdgeTags;
    int mappedVertexCount = 0;
    int edgeCount = 0;
    UF_MODL_ask_list_count(edgeList, &edgeCount);
    for (int edgeIndex = 0; edgeIndex < edgeCount; ++edgeIndex)
    {
        tag_t edgeTag = NULL_TAG;
        if (UF_MODL_ask_list_item(edgeList, edgeIndex, &edgeTag) != 0 || edgeTag == NULL_TAG)
        {
            continue;
        }

        faceEdgeTags.push_back(edgeTag);

        double start[3] = {0.0, 0.0, 0.0};
        double end[3] = {0.0, 0.0, 0.0};
        int vertexCount = 0;
        if (UF_MODL_ask_edge_verts(edgeTag, start, end, &vertexCount) != 0 || vertexCount < 2)
        {
            continue;
        }

        double drawingStart[2] = {0.0, 0.0};
        double drawingEnd[2] = {0.0, 0.0};
        if (UF_VIEW_map_model_to_drawing(view->Tag(), start, drawingStart) == 0)
        {
            projectedPoints.push_back(NXOpen::Point3d(drawingStart[0], drawingStart[1], 0.0));
            if (!initialized)
            {
                projectedBounds.minX = projectedBounds.maxX = drawingStart[0];
                projectedBounds.minY = projectedBounds.maxY = drawingStart[1];
                initialized = true;
            }
            else
            {
                projectedBounds.minX = std::min(projectedBounds.minX, drawingStart[0]);
                projectedBounds.maxX = std::max(projectedBounds.maxX, drawingStart[0]);
                projectedBounds.minY = std::min(projectedBounds.minY, drawingStart[1]);
                projectedBounds.maxY = std::max(projectedBounds.maxY, drawingStart[1]);
            }
            ++mappedVertexCount;
        }
        if (UF_VIEW_map_model_to_drawing(view->Tag(), end, drawingEnd) == 0)
        {
            projectedPoints.push_back(NXOpen::Point3d(drawingEnd[0], drawingEnd[1], 0.0));
            if (!initialized)
            {
                projectedBounds.minX = projectedBounds.maxX = drawingEnd[0];
                projectedBounds.minY = projectedBounds.maxY = drawingEnd[1];
                initialized = true;
            }
            else
            {
                projectedBounds.minX = std::min(projectedBounds.minX, drawingEnd[0]);
                projectedBounds.maxX = std::max(projectedBounds.maxX, drawingEnd[0]);
                projectedBounds.minY = std::min(projectedBounds.minY, drawingEnd[1]);
                projectedBounds.maxY = std::max(projectedBounds.maxY, drawingEnd[1]);
            }
            ++mappedVertexCount;
        }
    }

    UF_MODL_delete_list(&edgeList);

    if (!initialized || mappedVertexCount < 2)
    {
        return false;
    }

    const double projectedWidth = BoundsWidth(projectedBounds);
    const double projectedHeight = BoundsHeight(projectedBounds);
    const double minimumLength = std::max(1.0, axisTolerance * 5.0);
    double directionX = 0.0;
    double directionY = 0.0;
    double bestLength = 0.0;
    for (size_t i = 0; i < projectedPoints.size(); ++i)
    {
        for (size_t j = i + 1; j < projectedPoints.size(); ++j)
        {
            const double dx = projectedPoints[j].X - projectedPoints[i].X;
            const double dy = projectedPoints[j].Y - projectedPoints[i].Y;
            const double length = std::sqrt(dx * dx + dy * dy);
            if (length > bestLength)
            {
                bestLength = length;
                directionX = dx;
                directionY = dy;
            }
        }
    }
    if (Normalize2D(directionX, directionY) < minimumLength)
    {
        return false;
    }
    if (directionX < -1.0e-8 || (std::abs(directionX) <= 1.0e-8 && directionY < -1.0e-8))
    {
        directionX = -directionX;
        directionY = -directionY;
    }

    const double normalX = -directionY;
    const double normalY = directionX;
    const double centerX = (projectedBounds.minX + projectedBounds.maxX) * 0.5;
    const double centerY = (projectedBounds.minY + projectedBounds.maxY) * 0.5;
    const double lineOffset = normalX * centerX + normalY * centerY;
    double minAlong = std::numeric_limits<double>::max();
    double maxAlong = -std::numeric_limits<double>::max();
    double maxLineDistance = 0.0;
    for (const NXOpen::Point3d& projectedPoint : projectedPoints)
    {
        const double along = projectedPoint.X * directionX + projectedPoint.Y * directionY;
        minAlong = std::min(minAlong, along);
        maxAlong = std::max(maxAlong, along);
        maxLineDistance = std::max(maxLineDistance, std::abs(normalX * projectedPoint.X + normalY * projectedPoint.Y - lineOffset));
    }

    const double projectedLength = maxAlong - minAlong;
    if (projectedLength < minimumLength || maxLineDistance > std::max(0.25, axisTolerance * 2.5))
    {
        return false;
    }

    const bool horizontalLine = std::abs(directionY) <= std::max(0.002, axisTolerance / std::max(projectedLength, 1.0));
    const bool verticalLine = std::abs(directionX) <= std::max(0.002, axisTolerance / std::max(projectedLength, 1.0));

    LineProjectionFaceCandidate faceCandidate;
    faceCandidate.faceTag = faceTag;
    faceCandidate.normal = faceNormal;
    faceCandidate.planePoint = facePoint;
    faceCandidate.horizontalLine = horizontalLine;
    faceCandidate.verticalLine = verticalLine;
    faceCandidate.angledLine = !horizontalLine && !verticalLine;
    faceCandidate.directionX = directionX;
    faceCandidate.directionY = directionY;
    faceCandidate.normalX = normalX;
    faceCandidate.normalY = normalY;
    faceCandidate.lineOffset = lineOffset;
    faceCandidate.minAlong = minAlong;
    faceCandidate.maxAlong = maxAlong;
    faceCandidate.minX = projectedBounds.minX;
    faceCandidate.maxX = projectedBounds.maxX;
    faceCandidate.minY = projectedBounds.minY;
    faceCandidate.maxY = projectedBounds.maxY;
    faceCandidate.centerX = centerX;
    faceCandidate.centerY = centerY;
    faceCandidate.length = projectedLength;

    LineSegmentCandidate matchedLine;
    if (!MatchLineProjectionFaceToCurve(faceCandidate, lines, axisTolerance, matchedLine))
    {
        return false;
    }
    if (!DraftingCurveHasParentTag(matchedLine.curve != nullptr ? matchedLine.curve->Tag() : NULL_TAG, faceTag, faceEdgeTags))
    {
        return false;
    }

    faceCandidate.line = matchedLine;
    candidate = faceCandidate;
    return true;
}

std::vector<LineProjectionFaceCandidate> CollectLineProjectionFaceCandidates(
    NXOpen::Session* session,
    NXOpen::Part* part,
    NXOpen::Drawings::DraftingView* view,
    const std::string& label,
    const std::vector<LineSegmentCandidate>& lines,
    double axisTolerance,
    ShallowDetailFilterCache* shallowCache = nullptr,
    bool skipShallowDetailFaces = true)
{
    std::vector<LineProjectionFaceCandidate> candidates;
    if (part == nullptr || view == nullptr || lines.empty())
    {
        return candidates;
    }

    ShallowDetailFilterCache localShallowCache;
    ShallowDetailFilterCache& activeShallowCache = shallowCache != nullptr ? *shallowCache : localShallowCache;
    if (skipShallowDetailFaces)
    {
        EnsureShallowDetailFilterCache(part, activeShallowCache);
    }

    int planarFaces = 0;
    int matchedFaces = 0;
    int skippedShallowDetailFaces = 0;
    int skippedFastenerBodies = 0;
    int edgeParentFaces = 0;
    int skippedDuplicateEdgeParentFaces = 0;
    const std::vector<tag_t> bodyTags = CollectVisibleSolidBodyTags(part);
    std::set<tag_t> matchedFaceTags;
    for (tag_t bodyTag : bodyTags)
    {
        if (ShouldSkipFastenerBodyForParallelDimensions(bodyTag))
        {
            ++skippedFastenerBodies;
            continue;
        }

        uf_list_p_t faceList = nullptr;
        if (UF_MODL_ask_body_faces(bodyTag, &faceList) != 0 || faceList == nullptr)
        {
            continue;
        }

        int faceCount = 0;
        UF_MODL_ask_list_count(faceList, &faceCount);
        std::vector<tag_t> bodyFaceTags;
        for (int faceIndex = 0; faceIndex < faceCount; ++faceIndex)
        {
            tag_t faceTag = NULL_TAG;
            if (UF_MODL_ask_list_item(faceList, faceIndex, &faceTag) != 0 || faceTag == NULL_TAG)
            {
                continue;
            }
            bodyFaceTags.push_back(faceTag);
        }

        for (tag_t faceTag : bodyFaceTags)
        {
            NXOpen::Vector3d faceNormal(0.0, 0.0, 0.0);
            if (!TryReadPlanarFaceNormal(faceTag, faceNormal))
            {
                continue;
            }
            ++planarFaces;

            LineProjectionFaceCandidate candidate;
            if (!TryBuildLineProjectionFaceCandidate(view, faceTag, lines, axisTolerance, candidate))
            {
                continue;
            }
            if (skipShallowDetailFaces &&
                IsCachedShallowDetailFace(part, activeShallowCache, candidate.faceTag))
            {
                ++skippedShallowDetailFaces;
                continue;
            }

            candidate.bodyTag = bodyTag;
            candidates.push_back(candidate);
            matchedFaceTags.insert(candidate.faceTag);
            ++matchedFaces;
        }

        UF_MODL_delete_list(&faceList);
    }

    const double minimumLineLength = std::max(1.0, axisTolerance * 5.0);
    for (const LineSegmentCandidate& line : lines)
    {
        if (line.curve == nullptr || line.length < minimumLineLength)
        {
            continue;
        }

        double directionX = line.endX - line.startX;
        double directionY = line.endY - line.startY;
        if (Normalize2D(directionX, directionY) <= 1.0e-6)
        {
            continue;
        }
        if (directionX < -1.0e-8 ||
            (std::abs(directionX) <= 1.0e-8 && directionY < -1.0e-8))
        {
            directionX = -directionX;
            directionY = -directionY;
        }

        const double normalX = -directionY;
        const double normalY = directionX;
        const double centerX = (line.startX + line.endX) * 0.5;
        const double centerY = (line.startY + line.endY) * 0.5;
        const double lineOffset = normalX * centerX + normalY * centerY;
        const double startAlong = line.startX * directionX + line.startY * directionY;
        const double endAlong = line.endX * directionX + line.endY * directionY;
        const double minAlong = std::min(startAlong, endAlong);
        const double maxAlong = std::max(startAlong, endAlong);
        const bool horizontalLine =
            std::abs(directionY) <= std::max(0.002, axisTolerance / std::max(line.length, 1.0));
        const bool verticalLine =
            std::abs(directionX) <= std::max(0.002, axisTolerance / std::max(line.length, 1.0));

        std::vector<tag_t> parentFaceTags;
        int parentCount = 0;
        tag_t* parents = nullptr;
        if (UF_DRAW_ask_drafting_curve_parents(line.curve->Tag(), &parentCount, &parents) != 0 ||
            parentCount <= 0 ||
            parents == nullptr)
        {
            continue;
        }

        for (int parentIndex = 0; parentIndex < parentCount; ++parentIndex)
        {
            const tag_t parentTag = parents[parentIndex];
            if (parentTag == NULL_TAG)
            {
                continue;
            }
            NXOpen::Vector3d parentNormal(0.0, 0.0, 0.0);
            if (TryReadPlanarFaceNormal(parentTag, parentNormal) &&
                !ContainsTag(parentFaceTags, parentTag))
            {
                parentFaceTags.push_back(parentTag);
            }

            const std::vector<tag_t> edgeFaces = AskEdgeFaceTagsCached(parentTag, activeShallowCache);
            for (tag_t edgeFaceTag : edgeFaces)
            {
                if (edgeFaceTag != NULL_TAG && !ContainsTag(parentFaceTags, edgeFaceTag))
                {
                    parentFaceTags.push_back(edgeFaceTag);
                }
            }
        }
        UF_free(parents);

        for (tag_t faceTag : parentFaceTags)
        {
            tag_t bodyTag = NULL_TAG;
            if (UF_MODL_ask_face_body(faceTag, &bodyTag) != 0 || bodyTag == NULL_TAG)
            {
                continue;
            }
            if (ShouldSkipFastenerBodyForParallelDimensions(bodyTag))
            {
                ++skippedFastenerBodies;
                continue;
            }
            if (matchedFaceTags.find(faceTag) != matchedFaceTags.end())
            {
                ++skippedDuplicateEdgeParentFaces;
                continue;
            }
            if (skipShallowDetailFaces &&
                IsCachedShallowDetailFace(part, activeShallowCache, faceTag))
            {
                ++skippedShallowDetailFaces;
                continue;
            }

            NXOpen::Vector3d faceNormal(0.0, 0.0, 0.0);
            NXOpen::Point3d facePoint(0.0, 0.0, 0.0);
            if (!TryReadPlanarFacePlane(faceTag, faceNormal, facePoint))
            {
                continue;
            }

            LineProjectionFaceCandidate candidate;
            candidate.faceTag = faceTag;
            candidate.bodyTag = bodyTag;
            candidate.normal = faceNormal;
            candidate.planePoint = facePoint;
            candidate.line = line;
            candidate.horizontalLine = horizontalLine;
            candidate.verticalLine = verticalLine;
            candidate.angledLine = !horizontalLine && !verticalLine;
            candidate.directionX = directionX;
            candidate.directionY = directionY;
            candidate.normalX = normalX;
            candidate.normalY = normalY;
            candidate.lineOffset = lineOffset;
            candidate.minAlong = minAlong;
            candidate.maxAlong = maxAlong;
            candidate.minX = line.minX;
            candidate.maxX = line.maxX;
            candidate.minY = line.minY;
            candidate.maxY = line.maxY;
            candidate.centerX = centerX;
            candidate.centerY = centerY;
            candidate.length = line.length;
            candidates.push_back(candidate);
            matchedFaceTags.insert(faceTag);
            ++edgeParentFaces;
        }
    }

    std::ostringstream log;
    log << "AutoCreateThreeViews: " << label << " face-to-face candidate collection"
        << " planarFaces=" << planarFaces
        << ", matchedFaces=" << matchedFaces
        << ", edgeParentFaces=" << edgeParentFaces
        << ", skippedDuplicateEdgeParentFaces=" << skippedDuplicateEdgeParentFaces
        << ", skippedShallowDetailFaces=" << skippedShallowDetailFaces
        << ", skippedFastenerBodies=" << skippedFastenerBodies
        << ", bodies=" << bodyTags.size()
        << ", axisTolerance=" << axisTolerance << ".";
    WriteLine(session, log.str());
    return candidates;
}

bool CreateViewPlateThicknessDimension(
    NXOpen::Session* session,
    NXOpen::Part* workPart,
    NXOpen::Drawings::DraftingView* view,
    const std::string& label,
    const std::vector<LineProjectionFaceCandidate>& inputFaces,
    const LayoutBounds& bounds,
    double offset,
    double sheetMetalThickness)
{
    if (workPart == nullptr || view == nullptr || sheetMetalThickness <= 1.0e-6 || inputFaces.size() < 2)
    {
        return false;
    }

    std::vector<LineProjectionFaceCandidate> faces = CollapseSamePlaneFaceCandidates(inputFaces);
    if (faces.size() < 2)
    {
        return false;
    }

    struct ThicknessPair
    {
        size_t first = 0;
        size_t second = 0;
        bool verticalMeasurement = true;
        bool angledMeasurement = false;
        double overlapMin = 0.0;
        double overlapMax = 0.0;
        double modelGap = 0.0;
        double drawingGap = 0.0;
        double score = -std::numeric_limits<double>::max();
    };

    const double axisTolerance = std::max(0.15, std::max(BoundsWidth(bounds), BoundsHeight(bounds)) * 0.004);
    const double minimumGap = std::max(0.03, axisTolerance * 0.35);
    ThicknessPair best;
    bool found = false;
    for (size_t i = 0; i < faces.size(); ++i)
    {
        for (size_t j = i + 1; j < faces.size(); ++j)
        {
            const LineProjectionFaceCandidate& a = faces[i];
            const LineProjectionFaceCandidate& b = faces[j];
            if (a.faceTag == b.faceTag)
            {
                continue;
            }
            if (std::abs(Dot2D(a.directionX, a.directionY, b.directionX, b.directionY)) < 0.996 ||
                std::abs(DotVector(a.normal, b.normal)) < 0.995)
            {
                continue;
            }

            double modelGap = 0.0;
            if (!TryComputeModelFaceGap(a, b, modelGap) ||
                !IsPlateThicknessGap(modelGap, sheetMetalThickness))
            {
                continue;
            }

            const double drawingGap = std::abs(a.lineOffset - b.lineOffset);
            const double overlapMin = std::max(a.minAlong, b.minAlong);
            const double overlapMax = std::min(a.maxAlong, b.maxAlong);
            const double overlap = overlapMax - overlapMin;
            if (drawingGap < minimumGap || overlap < std::max(1.0, std::min(a.length, b.length) * 0.20))
            {
                continue;
            }

            const double score = overlap * 10.0 - std::abs(modelGap - sheetMetalThickness) * 1000.0 - drawingGap * 0.1;
            if (!found || score > best.score)
            {
                best.first = i;
                best.second = j;
                best.verticalMeasurement = a.horizontalLine;
                best.angledMeasurement = a.angledLine || b.angledLine;
                best.overlapMin = overlapMin;
                best.overlapMax = overlapMax;
                best.modelGap = modelGap;
                best.drawingGap = drawingGap;
                best.score = score;
                found = true;
            }
        }
    }

    const std::string viewLabel = label.empty() ? "view" : label;
    if (!found)
    {
        WriteLine(session, "AutoCreateThreeViews: " + viewLabel + " plate thickness dimension skipped; no visible thickness face pair found.");
        return false;
    }

    const LineProjectionFaceCandidate& first = faces[best.first];
    const LineProjectionFaceCandidate& second = faces[best.second];
    const double placeOffset = std::max(5.0, offset * 0.65);
    bool created = false;
    if (best.angledMeasurement)
    {
        const double midAlong = (best.overlapMin + best.overlapMax) * 0.5;
        const double midOffset = (first.lineOffset + second.lineOffset) * 0.5;
        const double baseX = first.directionX * midAlong + first.normalX * midOffset;
        const double baseY = first.directionY * midAlong + first.normalY * midOffset;
        const double boundsCenterX = (bounds.minX + bounds.maxX) * 0.5;
        const double boundsCenterY = (bounds.minY + bounds.maxY) * 0.5;
        const double sideSign =
            Dot2D(baseX - boundsCenterX, baseY - boundsCenterY, first.normalX, first.normalY) >= 0.0 ? 1.0 : -1.0;
        const NXOpen::Point3d origin(baseX + first.normalX * sideSign * placeOffset, baseY + first.normalY * sideSign * placeOffset, 0.0);
        created = CreateFacePairDimension(
            session,
            workPart,
            view,
            first,
            second,
            NXOpen::Annotations::DimensionMeasurementBuilder::MeasurementMethodPerpendicular,
            origin,
            viewLabel + " plate thickness");
    }
    else if (best.verticalMeasurement)
    {
        const LineProjectionFaceCandidate& lower = first.centerY <= second.centerY ? first : second;
        const LineProjectionFaceCandidate& upper = first.centerY <= second.centerY ? second : first;
        const double pickX = (best.overlapMin + best.overlapMax) * 0.5;
        const double originX = pickX <= (bounds.minX + bounds.maxX) * 0.5
            ? bounds.minX - placeOffset
            : bounds.maxX + placeOffset;
        created = CreateFacePairDimension(
            session,
            workPart,
            view,
            lower,
            upper,
            NXOpen::Annotations::DimensionMeasurementBuilder::MeasurementMethodVertical,
            NXOpen::Point3d(originX, (lower.centerY + upper.centerY) * 0.5, 0.0),
            viewLabel + " plate thickness");
    }
    else
    {
        const LineProjectionFaceCandidate& left = first.centerX <= second.centerX ? first : second;
        const LineProjectionFaceCandidate& right = first.centerX <= second.centerX ? second : first;
        const double pickY = (best.overlapMin + best.overlapMax) * 0.5;
        const double originY = pickY <= (bounds.minY + bounds.maxY) * 0.5
            ? bounds.minY - placeOffset
            : bounds.maxY + placeOffset;
        created = CreateFacePairDimension(
            session,
            workPart,
            view,
            left,
            right,
            NXOpen::Annotations::DimensionMeasurementBuilder::MeasurementMethodHorizontal,
            NXOpen::Point3d((left.centerX + right.centerX) * 0.5, originY, 0.0),
            viewLabel + " plate thickness");
    }

    std::ostringstream log;
    log << "AutoCreateThreeViews: " << viewLabel << " plate thickness dimension"
        << " created=" << (created ? "yes" : "no")
        << ", modelGap=" << best.modelGap
        << ", drawingGap=" << best.drawingGap << ".";
    WriteLine(session, log.str());
    return created;
}

bool TryFindOverallFacePair(
    const std::vector<LineProjectionFaceCandidate>& faces,
    const LayoutBounds& bounds,
    bool horizontalMeasurement,
    double axisTolerance,
    FacePairKey& pairKey,
    std::vector<LineProjectionFaceCandidate>* pairFaces = nullptr,
    NXOpen::Session* session = nullptr,
    const std::string& label = std::string())
{
    const LineProjectionFaceCandidate* lowFace = nullptr;
    const LineProjectionFaceCandidate* highFace = nullptr;
    double lowBestLength = -std::numeric_limits<double>::max();
    double highBestLength = -std::numeric_limits<double>::max();
    double lowCoord = 0.0;
    double highCoord = 0.0;
    double lowDistance = std::numeric_limits<double>::max();
    double highDistance = std::numeric_limits<double>::max();
    const double sameExtremeTolerance = std::max(0.03, axisTolerance * 0.5);
    const double boundaryTolerance = std::max(0.10, axisTolerance * 2.0);
    const double lowTarget = horizontalMeasurement ? bounds.minX : bounds.minY;
    const double highTarget = horizontalMeasurement ? bounds.maxX : bounds.maxY;

    for (const LineProjectionFaceCandidate& face : faces)
    {
        if (horizontalMeasurement)
        {
            if (!face.verticalLine)
            {
                continue;
            }
        }
        else if (!face.horizontalLine)
        {
            continue;
        }

        const double coord = horizontalMeasurement ? face.centerX : face.centerY;
        const double distanceToLow = std::abs(coord - lowTarget);
        const double distanceToHigh = std::abs(coord - highTarget);
        if (distanceToLow < lowDistance - sameExtremeTolerance ||
            (std::abs(distanceToLow - lowDistance) <= sameExtremeTolerance && face.length > lowBestLength))
        {
            lowDistance = distanceToLow;
            lowCoord = coord;
            lowBestLength = face.length;
            lowFace = &face;
        }
        if (distanceToHigh < highDistance - sameExtremeTolerance ||
            (std::abs(distanceToHigh - highDistance) <= sameExtremeTolerance && face.length > highBestLength))
        {
            highDistance = distanceToHigh;
            highCoord = coord;
            highBestLength = face.length;
            highFace = &face;
        }
    }

    if (lowFace == nullptr ||
        highFace == nullptr ||
        lowFace->faceTag == highFace->faceTag ||
        std::abs(highCoord - lowCoord) <= std::max(0.03, axisTolerance))
    {
        return false;
    }
    if (lowDistance > boundaryTolerance || highDistance > boundaryTolerance)
    {
        if (session != nullptr)
        {
            std::ostringstream log;
            log << "AutoCreateThreeViews: " << (label.empty() ? "view" : label)
                << (horizontalMeasurement ? " horizontal" : " vertical")
                << " body-box overall datum faces rejected; not on true view extrema"
                << ", lowTarget=" << lowTarget
                << ", highTarget=" << highTarget
                << ", lowCoord=" << lowCoord
                << ", highCoord=" << highCoord
                << ", lowDistance=" << lowDistance
                << ", highDistance=" << highDistance
                << ", tolerance=" << boundaryTolerance << ".";
            WriteLine(session, log.str());
        }
        return false;
    }

    pairKey = MakeFacePairKey(*lowFace, *highFace);
    if (session != nullptr)
    {
        std::ostringstream log;
        log << "AutoCreateThreeViews: " << (label.empty() ? "view" : label)
            << (horizontalMeasurement ? " horizontal" : " vertical")
            << " body-box overall datum faces"
            << " lowFace=" << static_cast<unsigned long long>(lowFace->faceTag)
            << ", highFace=" << static_cast<unsigned long long>(highFace->faceTag)
            << ", lowTarget=" << lowTarget
            << ", highTarget=" << highTarget
            << ", lowCoord=" << lowCoord
            << ", highCoord=" << highCoord
            << ", lowDistance=" << lowDistance
            << ", highDistance=" << highDistance
            << ", lowLength=" << lowBestLength
            << ", highLength=" << highBestLength << ".";
        WriteLine(session, log.str());
    }
    if (pairFaces != nullptr)
    {
        pairFaces->push_back(*lowFace);
        pairFaces->push_back(*highFace);
    }
    return true;
}

bool TryFindClosedCurveFacePair(
    const std::vector<LineProjectionFaceCandidate>& faceCandidates,
    const ClosedCurveLoopCandidate& loop,
    bool horizontalMeasurement,
    double axisTolerance,
    FacePairKey& pairKey,
    std::vector<LineProjectionFaceCandidate>* pairFaces)
{
    const double loopWidth = BoundsWidth(loop.bounds);
    const double loopHeight = BoundsHeight(loop.bounds);
    if (loopWidth <= 1.0e-6 || loopHeight <= 1.0e-6)
    {
        return false;
    }

    const double lowTarget = horizontalMeasurement ? loop.bounds.minX : loop.bounds.minY;
    const double highTarget = horizontalMeasurement ? loop.bounds.maxX : loop.bounds.maxY;
    const double boundaryTolerance = std::max({axisTolerance * 5.0, 0.6, (horizontalMeasurement ? loopWidth : loopHeight) * 0.08});
    const double minCrossOverlap = std::max(axisTolerance * 3.0, (horizontalMeasurement ? loopHeight : loopWidth) * 0.30);

    const LineProjectionFaceCandidate* lowFace = nullptr;
    const LineProjectionFaceCandidate* highFace = nullptr;
    double lowBestScore = std::numeric_limits<double>::max();
    double highBestScore = std::numeric_limits<double>::max();

    for (const LineProjectionFaceCandidate& face : faceCandidates)
    {
        if (horizontalMeasurement)
        {
            if (!face.verticalLine)
            {
                continue;
            }
        }
        else if (!face.horizontalLine)
        {
            continue;
        }

        const double coord = horizontalMeasurement ? face.centerX : face.centerY;
        const double crossMin = horizontalMeasurement ? face.minY : face.minX;
        const double crossMax = horizontalMeasurement ? face.maxY : face.maxX;
        const double loopCrossMin = horizontalMeasurement ? loop.bounds.minY : loop.bounds.minX;
        const double loopCrossMax = horizontalMeasurement ? loop.bounds.maxY : loop.bounds.maxX;
        const double crossOverlap = RangeOverlap(crossMin, crossMax, loopCrossMin, loopCrossMax);
        if (crossOverlap < minCrossOverlap)
        {
            continue;
        }

        const double lowDistance = std::abs(coord - lowTarget);
        if (lowDistance <= boundaryTolerance)
        {
            const double score = lowDistance - crossOverlap * 0.01;
            if (score < lowBestScore)
            {
                lowBestScore = score;
                lowFace = &face;
            }
        }

        const double highDistance = std::abs(coord - highTarget);
        if (highDistance <= boundaryTolerance)
        {
            const double score = highDistance - crossOverlap * 0.01;
            if (score < highBestScore)
            {
                highBestScore = score;
                highFace = &face;
            }
        }
    }

    if (lowFace == nullptr || highFace == nullptr || lowFace->faceTag == highFace->faceTag)
    {
        return false;
    }

    pairKey = MakeFacePairKey(*lowFace, *highFace);
    if (pairFaces != nullptr)
    {
        pairFaces->push_back(*lowFace);
        pairFaces->push_back(*highFace);
    }
    return true;
}

struct ProjectedViewLinearDatumRule
{
    bool valid = false;
    bool verticalMeasurement = false;
    bool useHighDatum = false;
};

ProjectedViewLinearDatumRule GetProjectedViewLinearDatumRule(const std::string& viewLabel, bool firstAngle)
{
    ProjectedViewLinearDatumRule rule;
    if (viewLabel == "bottom" || viewLabel == "back bottom")
    {
        rule.valid = true;
        rule.verticalMeasurement = true;
        rule.useHighDatum = firstAngle;
    }
    else if (viewLabel == "top")
    {
        rule.valid = true;
        rule.verticalMeasurement = true;
        rule.useHighDatum = !firstAngle;
    }
    else if (viewLabel == "right")
    {
        rule.valid = true;
        rule.verticalMeasurement = false;
        rule.useHighDatum = firstAngle;
    }
    else if (viewLabel == "left")
    {
        rule.valid = true;
        rule.verticalMeasurement = false;
        rule.useHighDatum = !firstAngle;
    }
    return rule;
}

void CreateViewFaceToFaceDimensions(
    NXOpen::Session* session,
    NXOpen::Part* workPart,
    NXOpen::Drawings::DraftingView* view,
    const std::string& label,
    const std::vector<DraftingCurveExtent>& extents,
    const LayoutBounds& bounds,
    double overallOffset,
    std::vector<FacePairKey>& usedFacePairs,
    double sheetMetalThickness,
    bool& smallPlateThicknessDimensionCreated,
    const std::vector<LineProjectionFaceCandidate>& overallDimensionDatumFaces,
    const std::vector<LineProjectionFaceCandidate>& overallDimensionThicknessFaces,
    const std::vector<ClosedCurveDimensionRecord>& closedCurveDimensionRecords,
    bool useClosedCurveLoopSkip,
    bool firstAngle,
    MainViewFacePairRules* mainViewFacePairRules)
{
    const double width = BoundsWidth(bounds);
    const double height = BoundsHeight(bounds);
    if (workPart == nullptr || view == nullptr || width <= 1.0 || height <= 1.0)
    {
        return;
    }

    const std::string viewLabel = label.empty() ? "view" : label;
    const std::vector<LineSegmentCandidate> lines = CollectLineSegments(view, extents);
    const double axisTolerance = std::max(0.15, std::max(width, height) * 0.004);
    std::vector<LineProjectionFaceCandidate> faces =
        CollectLineProjectionFaceCandidates(session, workPart, view, viewLabel, lines, axisTolerance, nullptr, false);
    if (faces.size() < 2)
    {
        WriteLine(session, "AutoCreateThreeViews: " + viewLabel + " face-to-face dimensions skipped; less than 2 usable planar faces.");
        return;
    }

    MarkOuterContourDatumFaces(faces, bounds, axisTolerance);

    int outerDatumFaces = 0;
    for (const LineProjectionFaceCandidate& face : faces)
    {
        if (face.outerContourDatum)
        {
            ++outerDatumFaces;
        }
    }

    const size_t rawFaceCount = faces.size();
    faces = CollapseSamePlaneFaceCandidates(faces);
    MarkOuterContourDatumFaces(faces, bounds, axisTolerance);
    outerDatumFaces = 0;
    for (const LineProjectionFaceCandidate& face : faces)
    {
        if (face.outerContourDatum)
        {
            ++outerDatumFaces;
        }
    }
    if (faces.size() < 2)
    {
        std::ostringstream log;
        log << "AutoCreateThreeViews: " << viewLabel
            << " face-to-face dimensions skipped; less than 2 unique planar faces"
            << " rawFaces=" << rawFaceCount
            << ", uniqueFaces=" << faces.size() << ".";
        WriteLine(session, log.str());
        return;
    }

    ShallowDetailFilterCache localShallowCache;
    std::vector<ClosedCurveLoopCandidate> closedCurveSkipLoops;
    if (useClosedCurveLoopSkip)
    {
        closedCurveSkipLoops =
            CollectClosedCurveLoops(session, workPart, localShallowCache, view, extents, bounds, viewLabel + " linear skip");
    }
    const InnerLoopAdjacentFaces innerLoopAdjacentFaces =
        CollectModelInnerLoopAdjacentFaceTags(workPart);
    auto isInnerLoopAdjacentFace = [&](const LineProjectionFaceCandidate& face) {
        return ContainsTag(innerLoopAdjacentFaces.faceTags, face.faceTag);
    };

    struct FacePair
    {
        size_t first = 0;
        size_t second = 0;
        bool verticalMeasurement = true;
        bool angledMeasurement = false;
        double gap = 0.0;
        double overlapMin = 0.0;
        double overlapMax = 0.0;
        double modelGap = 0.0;
        double centerAlignment = 0.0;
        double lengthDelta = 0.0;
        double viewCenterOffset = 0.0;
        double symmetryPenalty = 0.0;
        double score = 0.0;
    };

    std::vector<FacePair> pairs;
    const double minimumGap = std::max(0.03, axisTolerance * 0.35);
    const double boundsCenterX = (bounds.minX + bounds.maxX) * 0.5;
    const double boundsCenterY = (bounds.minY + bounds.maxY) * 0.5;
    int rejectedByMainAxisRules = 0;
    int rejectedInnerClosedCurveFaces = 0;
    int skippedNonOverallDatumPairs = 0;
    struct MainViewDatumGroup
    {
        bool measuresX = true;
        int axis = -1;
        int mainSide = 0;
        int drawingSide = 0;
        double distance = 0.0;
        double length = 0.0;
        tag_t faceTag = NULL_TAG;
        double minX = 0.0;
        double minY = 0.0;
        double maxX = 0.0;
        double maxY = 0.0;
        FacePlaneSignature plane;
    };

    std::vector<MainViewDatumGroup> mainViewDatumGroups;
    const bool requireMainViewDatumPair =
        viewLabel == "front" &&
        mainViewFacePairRules != nullptr &&
        mainViewFacePairRules->valid;
    const ProjectedViewLinearDatumRule projectedDatumRule =
        viewLabel == "front" ? ProjectedViewLinearDatumRule{} : GetProjectedViewLinearDatumRule(viewLabel, firstAngle);
    const double projectedDatumTolerance =
        std::max(0.08, std::max(width, height) * 0.002);
    double projectedDatumCoordinate = 0.0;
    bool hasProjectedDatumCoordinate = false;
    double projectedDatumBoundaryCoordinate = 0.0;
    bool hasProjectedDatumBoundaryCoordinate = false;
    if (requireMainViewDatumPair)
    {
        const double drawingZoneTolerance = 0.05;
        for (const LineProjectionFaceCandidate& face : overallDimensionDatumFaces)
        {
            if (face.angledLine)
            {
                continue;
            }
            int axis = -1;
            int mainSide = 0;
            if (!TryClassifyMainViewAxisSide(*mainViewFacePairRules, face, axis, mainSide))
            {
                continue;
            }
            const bool measuresX = !face.horizontalLine;
            const double drawingOffset = measuresX
                ? face.centerX - boundsCenterX
                : face.centerY - boundsCenterY;
            if (std::abs(drawingOffset) <= drawingZoneTolerance)
            {
                continue;
            }

            const int drawingSide = drawingOffset > 0.0 ? 1 : -1;
            const double distance = std::abs(drawingOffset);
            const FacePlaneSignature plane = MakeFacePlaneSignature(face.normal, face.planePoint);
            if (!plane.valid)
            {
                continue;
            }

            bool merged = false;
            for (MainViewDatumGroup& group : mainViewDatumGroups)
            {
                const bool samePlane = SameFacePlaneSignature(group.plane, plane);
                if (group.measuresX != measuresX ||
                    group.axis != axis ||
                    group.mainSide != mainSide ||
                    group.drawingSide != drawingSide ||
                    !samePlane)
                {
                    continue;
                }

                if (face.length > group.length)
                {
                    group.distance = distance;
                    group.length = face.length;
                    group.faceTag = face.faceTag;
                    group.minX = face.minX;
                    group.minY = face.minY;
                    group.maxX = face.maxX;
                    group.maxY = face.maxY;
                    group.plane = plane;
                }
                merged = true;
                break;
            }
            if (!merged)
            {
                MainViewDatumGroup group;
                group.measuresX = measuresX;
                group.axis = axis;
                group.mainSide = mainSide;
                group.drawingSide = drawingSide;
                group.distance = distance;
                group.length = face.length;
                group.faceTag = face.faceTag;
                group.minX = face.minX;
                group.minY = face.minY;
                group.maxX = face.maxX;
                group.maxY = face.maxY;
                group.plane = plane;
                mainViewDatumGroups.push_back(group);
            }
        }

        std::ostringstream datumLog;
        datumLog << "AutoCreateThreeViews: " << viewLabel
            << " main-view max-outline datum groups"
            << " sourceDatumFaces=" << overallDimensionDatumFaces.size()
            << ", groups=" << mainViewDatumGroups.size();
        for (const MainViewDatumGroup& group : mainViewDatumGroups)
        {
            datumLog << " [face=" << static_cast<unsigned long long>(group.faceTag)
                << ", measures=" << (group.measuresX ? "X" : "Y")
                << ", axis=" << group.axis
                << ", mainSide=" << group.mainSide
                << ", drawingSide=" << group.drawingSide
                << ", distance=" << group.distance
                << ", length=" << group.length
                << ", bounds=(" << group.minX << "," << group.minY << "," << group.maxX << "," << group.maxY << ")]";
        }
        datumLog << ".";
        WriteLine(session, datumLog.str());
    }

    auto findMainViewDatumPlane = [&](const LineProjectionFaceCandidate& face, bool measuresX) {
        FacePlaneSignature empty;
        if (!requireMainViewDatumPair)
        {
            return empty;
        }
        int axis = -1;
        int mainSide = 0;
        if (!TryClassifyMainViewAxisSide(*mainViewFacePairRules, face, axis, mainSide))
        {
            return empty;
        }
        const double drawingOffset = measuresX
            ? face.centerX - boundsCenterX
            : face.centerY - boundsCenterY;
        if (std::abs(drawingOffset) <= 0.05)
        {
            return empty;
        }
        const int drawingSide = drawingOffset > 0.0 ? 1 : -1;
        for (const MainViewDatumGroup& group : mainViewDatumGroups)
        {
            if (group.measuresX == measuresX &&
                group.axis == axis &&
                group.mainSide == mainSide &&
                group.drawingSide == drawingSide)
            {
                return group.plane;
            }
        }
        return empty;
    };

    auto isMainViewDatumFace = [&](const LineProjectionFaceCandidate& face) {
        const FacePlaneSignature signature = MakeFacePlaneSignature(face.normal, face.planePoint);
        if (!signature.valid)
        {
            return false;
        }
        for (const MainViewDatumGroup& group : mainViewDatumGroups)
        {
            if (SameFacePlaneSignature(group.plane, signature))
            {
                return true;
            }
        }
        return false;
    };
    if (projectedDatumRule.valid)
    {
        projectedDatumBoundaryCoordinate = projectedDatumRule.verticalMeasurement
            ? (projectedDatumRule.useHighDatum ? bounds.maxY : bounds.minY)
            : (projectedDatumRule.useHighDatum ? bounds.maxX : bounds.minX);
        for (const LineProjectionFaceCandidate& face : faces)
        {
            if (projectedDatumRule.verticalMeasurement)
            {
                if (!face.horizontalLine)
                {
                    continue;
                }
                const double coord = face.centerY;
                if (!hasProjectedDatumCoordinate ||
                    (projectedDatumRule.useHighDatum
                        ? coord > projectedDatumCoordinate
                        : coord < projectedDatumCoordinate))
                {
                    projectedDatumCoordinate = coord;
                    hasProjectedDatumCoordinate = true;
                }
            }
            else
            {
                if (!face.verticalLine)
                {
                    continue;
                }
                const double coord = face.centerX;
                if (!hasProjectedDatumCoordinate ||
                    (projectedDatumRule.useHighDatum
                        ? coord > projectedDatumCoordinate
                        : coord < projectedDatumCoordinate))
                {
                    projectedDatumCoordinate = coord;
                    hasProjectedDatumCoordinate = true;
                }
            }
        }
        if (hasProjectedDatumCoordinate)
        {
            const double datumBoundaryDelta = std::abs(projectedDatumCoordinate - projectedDatumBoundaryCoordinate);
            if (datumBoundaryDelta > std::max(projectedDatumTolerance * 2.0, axisTolerance))
            {
                std::ostringstream datumMismatchLog;
                datumMismatchLog << "AutoCreateThreeViews: " << viewLabel
                    << " projected datum candidate is not on view outer boundary"
                    << " measurement=" << (projectedDatumRule.verticalMeasurement ? "Y" : "X")
                    << ", side=" << (projectedDatumRule.useHighDatum ? "max" : "min")
                    << ", candidateCoord=" << projectedDatumCoordinate
                    << ", boundaryCoord=" << projectedDatumBoundaryCoordinate
                    << ", delta=" << datumBoundaryDelta
                    << ", tolerance=" << projectedDatumTolerance
                    << ".";
                WriteLine(session, datumMismatchLog.str());
            }
        }
    }
    auto isProjectedDatumFace = [&](const LineProjectionFaceCandidate& face) {
        if (!projectedDatumRule.valid || !hasProjectedDatumCoordinate)
        {
            return false;
        }
        if (projectedDatumRule.verticalMeasurement)
        {
            if (!face.horizontalLine)
            {
                return false;
            }
            return std::abs(face.centerY - projectedDatumCoordinate) <= projectedDatumTolerance;
        }
        if (!face.verticalLine)
        {
            return false;
        }
        return std::abs(face.centerX - projectedDatumCoordinate) <= projectedDatumTolerance;
    };
    std::map<std::string, int> mainAxisRejectReasons;
    int skippedProjectedViewWrongDirection = 0;
    int skippedProjectedViewNoDatum = 0;
    int diagnosticRejectLogs = 0;
    const int maxDiagnosticRejectLogs = 80;
    auto isDiagnosticGap = [&](double modelGap, double drawingGap) {
        if (std::abs(modelGap - 15.0) <= 0.35 ||
            std::abs(modelGap - 10.0) <= 0.35)
        {
            return true;
        }
        const double drawing15 = 15.0 / 4.0;
        const double drawing10 = 10.0 / 4.0;
        return std::abs(drawingGap - drawing15) <= std::max(0.08, axisTolerance * 0.5) ||
            std::abs(drawingGap - drawing10) <= std::max(0.08, axisTolerance * 0.5);
    };
    auto logDiagnosticPairReject = [&](const char* stage,
                                       const LineProjectionFaceCandidate& a,
                                       const LineProjectionFaceCandidate& b,
                                       double drawingGap,
                                       double modelGap,
                                       const std::string& reason) {
        if (diagnosticRejectLogs >= maxDiagnosticRejectLogs ||
            !isDiagnosticGap(modelGap, drawingGap))
        {
            return;
        }
        ++diagnosticRejectLogs;
        std::ostringstream pairLog;
        pairLog << "AutoCreateThreeViews: " << viewLabel
            << " diagnostic face pair rejected"
            << " stage=" << stage
            << ", reason=" << reason
            << ", firstFace=" << static_cast<unsigned long long>(a.faceTag)
            << ", secondFace=" << static_cast<unsigned long long>(b.faceTag)
            << ", firstBody=" << static_cast<unsigned long long>(a.bodyTag)
            << ", secondBody=" << static_cast<unsigned long long>(b.bodyTag)
            << ", firstLine=" << (a.horizontalLine ? "H" : (a.verticalLine ? "V" : "A"))
            << ", secondLine=" << (b.horizontalLine ? "H" : (b.verticalLine ? "V" : "A"))
            << ", normalDot=" << DotVector(NormalizeVector(a.normal), NormalizeVector(b.normal))
            << ", drawingGap=" << drawingGap
            << ", modelGap=" << modelGap
            << ", firstCenter=(" << a.centerX << "," << a.centerY << ")"
            << ", secondCenter=(" << b.centerX << "," << b.centerY << ")"
            << ", firstBounds=(" << a.minX << "," << a.minY << "," << a.maxX << "," << a.maxY << ")"
            << ", secondBounds=(" << b.minX << "," << b.minY << "," << b.maxX << "," << b.maxY << ")"
            << ", firstOuterDatum=" << (a.outerContourDatum ? "yes" : "no")
            << ", secondOuterDatum=" << (b.outerContourDatum ? "yes" : "no")
            << ".";
        WriteLine(session, pairLog.str());
    };
    for (size_t i = 0; i < faces.size(); ++i)
    {
        for (size_t j = i + 1; j < faces.size(); ++j)
        {
            const LineProjectionFaceCandidate& a = faces[i];
            const LineProjectionFaceCandidate& b = faces[j];
            if (a.faceTag == b.faceTag)
            {
                continue;
            }
            if (a.bodyTag == NULL_TAG || b.bodyTag == NULL_TAG || a.bodyTag != b.bodyTag)
            {
                logDiagnosticPairReject("body", a, b, std::abs(a.lineOffset - b.lineOffset), 0.0, "differentBody");
                continue;
            }

            if (isInnerLoopAdjacentFace(a) || isInnerLoopAdjacentFace(b))
            {
                ++rejectedInnerClosedCurveFaces;
                continue;
            }

            const double lineDirectionAlignment =
                std::abs(Dot2D(a.directionX, a.directionY, b.directionX, b.directionY));
            if (lineDirectionAlignment < 0.996)
            {
                continue;
            }

            const double normalDot = DotVector(NormalizeVector(a.normal), NormalizeVector(b.normal));
            const double diagnosticDrawingGap = std::abs(a.lineOffset - b.lineOffset);
            double diagnosticModelGap = 0.0;
            TryComputeModelFaceGap(a, b, diagnosticModelGap);
            if (normalDot > -0.995)
            {
                logDiagnosticPairReject("normal", a, b, diagnosticDrawingGap, diagnosticModelGap, "notOppositeNormals");
                continue;
            }

            const bool pairMeasuresX = !a.horizontalLine;
            const double drawingZoneCenter = pairMeasuresX ? boundsCenterX : boundsCenterY;
            std::string mainAxisRejectReason;
            if (requireMainViewDatumPair &&
                !IsAllowedByMainViewFacePairRules(mainViewFacePairRules, a, b, pairMeasuresX, drawingZoneCenter, &mainAxisRejectReason))
            {
                ++rejectedByMainAxisRules;
                if (!mainAxisRejectReason.empty())
                {
                    ++mainAxisRejectReasons[mainAxisRejectReason];
                }
                logDiagnosticPairReject("mainAxisRule", a, b, diagnosticDrawingGap, diagnosticModelGap, mainAxisRejectReason);
                continue;
            }
            if (projectedDatumRule.valid)
            {
                const bool pairVerticalMeasurement = a.horizontalLine;
                if (pairVerticalMeasurement != projectedDatumRule.verticalMeasurement)
                {
                    ++skippedProjectedViewWrongDirection;
                    logDiagnosticPairReject("projectedDirection", a, b, diagnosticDrawingGap, diagnosticModelGap, "wrongMeasurementDirection");
                    continue;
                }
                const bool aDatum = isProjectedDatumFace(a);
                const bool bDatum = isProjectedDatumFace(b);
                if (aDatum == bDatum)
                {
                    ++skippedProjectedViewNoDatum;
                    logDiagnosticPairReject("projectedDatum", a, b, diagnosticDrawingGap, diagnosticModelGap, aDatum ? "bothDatum" : "noDatum");
                    continue;
                }
            }

            const FacePlaneSignature mainViewDatumPlane = findMainViewDatumPlane(a, pairMeasuresX);
            if (requireMainViewDatumPair)
            {
                if (!mainViewDatumPlane.valid)
                {
                    ++skippedNonOverallDatumPairs;
                    logDiagnosticPairReject("mainDatum", a, b, diagnosticDrawingGap, diagnosticModelGap, "noDatumPlaneForZone");
                    continue;
                }
                const FacePlaneSignature aPlane = MakeFacePlaneSignature(a.normal, a.planePoint);
                const FacePlaneSignature bPlane = MakeFacePlaneSignature(b.normal, b.planePoint);
                const bool aDatum = SameFacePlaneSignature(aPlane, mainViewDatumPlane);
                const bool bDatum = SameFacePlaneSignature(bPlane, mainViewDatumPlane);
                if (aDatum == bDatum)
                {
                    ++skippedNonOverallDatumPairs;
                    logDiagnosticPairReject("mainDatum", a, b, diagnosticDrawingGap, diagnosticModelGap, aDatum ? "bothDatumPlanes" : "neitherDatumPlane");
                    continue;
                }
            }

            FacePair pair;
            pair.first = i;
            pair.second = j;
            pair.verticalMeasurement = a.horizontalLine;
            pair.angledMeasurement = a.angledLine || b.angledLine;
            pair.gap = std::abs(a.lineOffset - b.lineOffset);
            pair.overlapMin = std::max(a.minAlong, b.minAlong);
            pair.overlapMax = std::min(a.maxAlong, b.maxAlong);

            const double overlap = pair.overlapMax - pair.overlapMin;
            const double requiredOverlap = std::max(1.0, std::min(a.length, b.length) * 0.20);
            const double allowedSeparatedOverlapGap = std::max(1.2, axisTolerance * 8.0);
            const bool allowProjectedDatumSeparatedPair =
                projectedDatumRule.valid &&
                overlap >= -allowedSeparatedOverlapGap;
            if (pair.gap < minimumGap || (!allowProjectedDatumSeparatedPair && overlap < requiredOverlap))
            {
                logDiagnosticPairReject("gapOverlap", a, b, pair.gap, pair.modelGap, "smallGapOrInsufficientOverlap");
                continue;
            }
            if (allowProjectedDatumSeparatedPair && overlap < requiredOverlap)
            {
                if (a.maxAlong < b.minAlong)
                {
                    const double nearestMid = (a.maxAlong + b.minAlong) * 0.5;
                    pair.overlapMin = nearestMid;
                    pair.overlapMax = nearestMid;
                }
                else if (b.maxAlong < a.minAlong)
                {
                    const double nearestMid = (b.maxAlong + a.minAlong) * 0.5;
                    pair.overlapMin = nearestMid;
                    pair.overlapMax = nearestMid;
                }
            }

            double modelGap = 0.0;
            if (TryComputeModelFaceGap(a, b, modelGap))
            {
                pair.modelGap = modelGap;
            }

            const double aCenterAlong = (a.minAlong + a.maxAlong) * 0.5;
            const double bCenterAlong = (b.minAlong + b.maxAlong) * 0.5;
            const double pairOffsetCenter = (a.lineOffset + b.lineOffset) * 0.5;
            const double viewOffsetCenter = a.normalX * boundsCenterX + a.normalY * boundsCenterY;
            pair.centerAlignment = std::abs(aCenterAlong - bCenterAlong);
            pair.lengthDelta = std::abs(a.length - b.length);
            pair.viewCenterOffset = std::abs(pairOffsetCenter - viewOffsetCenter);
            pair.symmetryPenalty = pair.centerAlignment + pair.lengthDelta * 0.5;
            const double nearestGap = pair.modelGap > 1.0e-6 ? pair.modelGap : pair.gap;
            pair.score = nearestGap * 1000.0 + pair.symmetryPenalty * 10.0 + pair.viewCenterOffset - overlap * 0.2;
            pairs.push_back(pair);
        }
    }

    if (pairs.empty())
    {
        std::ostringstream log;
        log << "AutoCreateThreeViews: " << viewLabel << " face-to-face dimensions skipped; no valid face pairs"
            << " faces=" << faces.size()
            << ", minimumGap=" << minimumGap << ".";
        WriteLine(session, log.str());
        return;
    }

    std::sort(pairs.begin(), pairs.end(), [](const FacePair& a, const FacePair& b) {
        const double aGap = a.modelGap > 1.0e-6 ? a.modelGap : a.gap;
        const double bGap = b.modelGap > 1.0e-6 ? b.modelGap : b.gap;
        if (std::abs(aGap - bGap) > 0.01)
        {
            return aGap < bGap;
        }
        if (std::abs(a.gap - b.gap) > 0.01)
        {
            return a.gap < b.gap;
        }
        if (std::abs(a.symmetryPenalty - b.symmetryPenalty) > 0.01)
        {
            return a.symmetryPenalty < b.symmetryPenalty;
        }
        if (std::abs(a.viewCenterOffset - b.viewCenterOffset) > 0.01)
        {
            return a.viewCenterOffset < b.viewCenterOffset;
        }
        return (a.overlapMax - a.overlapMin) > (b.overlapMax - b.overlapMin);
    });

    std::vector<FacePlaneSignature> usedPlanes;
    std::vector<FacePlaneSignature> usedDimensionFacePlanes;
    std::vector<LineProjectionFaceCandidate> usedDimensionFaces = overallDimensionDatumFaces;
    std::vector<FacePairKey> usedViewFacePairs;
    const double outsideOffset = 5.0;
    const double laneStep = 7.0;
    int created = 0;
    std::vector<ClosedCurveDimensionRecord> closedCurveSkipRecords = closedCurveDimensionRecords;
    for (const ClosedCurveLoopCandidate& loop : closedCurveSkipLoops)
    {
        ClosedCurveDimensionRecord horizontalRecord;
        horizontalRecord.measuresX = true;
        horizontalRecord.minMeasure = loop.bounds.minX;
        horizontalRecord.maxMeasure = loop.bounds.maxX;
        horizontalRecord.minCross = loop.bounds.minY;
        horizontalRecord.maxCross = loop.bounds.maxY;
        closedCurveSkipRecords.push_back(horizontalRecord);

        ClosedCurveDimensionRecord verticalRecord;
        verticalRecord.measuresX = false;
        verticalRecord.minMeasure = loop.bounds.minY;
        verticalRecord.maxMeasure = loop.bounds.maxY;
        verticalRecord.minCross = loop.bounds.minX;
        verticalRecord.maxCross = loop.bounds.maxX;
        closedCurveSkipRecords.push_back(verticalRecord);
    }

    struct DimensionFootprint
    {
        bool verticalMeasurement = true;
        int side = 0;
        double first = 0.0;
        double second = 0.0;
        double pick = 0.0;
    };

    std::vector<DimensionFootprint> usedFootprints;
    auto isDuplicateFootprint = [&](const DimensionFootprint& footprint) {
        for (const DimensionFootprint& used : usedFootprints)
        {
            if (used.verticalMeasurement != footprint.verticalMeasurement || used.side != footprint.side)
            {
                continue;
            }
            if (std::abs(used.first - footprint.first) <= axisTolerance &&
                std::abs(used.second - footprint.second) <= axisTolerance &&
                std::abs(used.pick - footprint.pick) <= axisTolerance * 3.0)
            {
                return true;
            }
        }
        return false;
    };

    struct LaneInterval
    {
        double min = 0.0;
        double max = 0.0;
    };

    std::vector<std::vector<LaneInterval>> leftLanes;
    std::vector<std::vector<LaneInterval>> rightLanes;
    std::vector<std::vector<LaneInterval>> bottomLanes;
    std::vector<std::vector<LaneInterval>> topLanes;
    const double lanePadding = std::max(0.8, axisTolerance * 2.0);
    int skippedDuplicatePlateThickness = 0;
    int skippedSmallNonThickness = 0;
    int skippedPlateThicknessOffsetPairs = 0;
    int skippedOverallDatumThicknessOffsetPairs = 0;
    int skippedOverallDatumThicknessFaces = 0;
    int skippedOppositeDatumNearOverallPairs = 0;
    int skippedProjectionLineReuse = 0;
    int skippedGlobalPairedFaceReuse = 0;
    int skippedClosedCurveParallelPairs = 0;
    int skippedInnerClosedCurveFaces = 0;
    int insidePlacedToAvoidCrossing = 0;

    struct FaceProjectionLineSignature
    {
        long long direction = 0;
        long long offset = 0;
    };

    const double projectionLineTolerance = std::max(0.05, axisTolerance * 2.0);
    auto makeProjectionLineSignature = [&](const LineProjectionFaceCandidate& face) {
        FaceProjectionLineSignature signature;
        double directionX = face.directionX;
        double directionY = face.directionY;
        if (Normalize2D(directionX, directionY) <= 1.0e-9)
        {
            return signature;
        }
        if (directionX < -1.0e-8 ||
            (std::abs(directionX) <= 1.0e-8 && directionY < -1.0e-8))
        {
            directionX = -directionX;
            directionY = -directionY;
        }

        const double pi = 3.14159265358979323846;
        double directionAngle = std::atan2(directionY, directionX);
        while (directionAngle < 0.0)
        {
            directionAngle += pi;
        }
        while (directionAngle >= pi)
        {
            directionAngle -= pi;
        }

        signature.direction = static_cast<long long>(std::llround(directionAngle / (pi / 360.0)));
        signature.offset = static_cast<long long>(std::llround(face.lineOffset / projectionLineTolerance));
        return signature;
    };

    std::vector<FaceProjectionLineSignature> usedProjectionLines;
    std::map<tag_t, int> faceDimensionUseCounts;
    auto containsProjectionLineSignature = [&](const FaceProjectionLineSignature& signature) {
        for (const FaceProjectionLineSignature& used : usedProjectionLines)
        {
            if (used.direction == signature.direction && used.offset == signature.offset)
            {
                return true;
            }
        }
        return false;
    };

    auto rememberProjectionLine = [&](const LineProjectionFaceCandidate& face) {
        if (face.outerContourDatum)
        {
            return;
        }
        const FaceProjectionLineSignature signature = makeProjectionLineSignature(face);
        if (!containsProjectionLineSignature(signature))
        {
            usedProjectionLines.push_back(signature);
        }
    };

    auto rememberDimensionFace = [&](const LineProjectionFaceCandidate& face) {
        const FacePlaneSignature signature = MakeFacePlaneSignature(face.normal, face.planePoint);
        if (signature.valid && !ContainsFacePlaneSignature(usedDimensionFacePlanes, signature))
        {
            usedDimensionFacePlanes.push_back(signature);
            usedDimensionFaces.push_back(face);
        }
    };

    auto isInnerClosedCurveFace = [&](const LineProjectionFaceCandidate& face) {
        return isInnerLoopAdjacentFace(face);
    };

    auto isCoveredByClosedCurveDimension = [&](const FacePair& pair,
                                               const LineProjectionFaceCandidate& first,
                                               const LineProjectionFaceCandidate& second) {
        if (pair.angledMeasurement)
        {
            return false;
        }

        const bool pairMeasuresX = !pair.verticalMeasurement;
        const double firstMeasure = pairMeasuresX ? first.centerX : first.centerY;
        const double secondMeasure = pairMeasuresX ? second.centerX : second.centerY;
        const double pairMinMeasure = std::min(firstMeasure, secondMeasure);
        const double pairMaxMeasure = std::max(firstMeasure, secondMeasure);
        const double pairGap = pairMaxMeasure - pairMinMeasure;
        const double pairCenter = (pairMinMeasure + pairMaxMeasure) * 0.5;
        const double spanTolerance = std::max(axisTolerance * 2.0, 0.08);
        const double minCrossOverlap = std::max(axisTolerance * 2.0, 0.2);

        for (const ClosedCurveDimensionRecord& record : closedCurveSkipRecords)
        {
            if (record.measuresX != pairMeasuresX)
            {
                continue;
            }

            const double recordGap = record.maxMeasure - record.minMeasure;
            const double recordCenter = (record.minMeasure + record.maxMeasure) * 0.5;
            const double crossOverlap = RangeOverlap(pair.overlapMin, pair.overlapMax, record.minCross, record.maxCross);
            if (crossOverlap < minCrossOverlap)
            {
                continue;
            }

            const double containTolerance = std::max(axisTolerance * 3.0, 0.25);
            const bool pairInsideClosedLoop =
                pairMinMeasure >= record.minMeasure - containTolerance &&
                pairMaxMeasure <= record.maxMeasure + containTolerance &&
                pair.overlapMin >= record.minCross - containTolerance &&
                pair.overlapMax <= record.maxCross + containTolerance &&
                pairGap <= recordGap + containTolerance;
            if (pairInsideClosedLoop)
            {
                return true;
            }

            const bool sameSpan =
                std::abs(pairMinMeasure - record.minMeasure) <= spanTolerance &&
                std::abs(pairMaxMeasure - record.maxMeasure) <= spanTolerance;
            const bool sameGapAndCenter =
                std::abs(pairGap - recordGap) <= spanTolerance &&
                std::abs(pairCenter - recordCenter) <= spanTolerance;
            if (sameSpan || sameGapAndCenter)
            {
                return true;
            }
        }

        return false;
    };

    auto isOppositeDatumNearOverallPair = [&](const FacePair& pair,
                                              const LineProjectionFaceCandidate& first,
                                              const LineProjectionFaceCandidate& second) {
        if (pair.angledMeasurement)
        {
            return false;
        }

        const double overallSpan = pair.verticalMeasurement ? height : width;
        if (overallSpan <= 1.0e-6 || pair.gap < overallSpan * 0.82)
        {
            return false;
        }
        if (projectedDatumRule.valid)
        {
            const double sameOverallGapTolerance = std::max(0.05, axisTolerance * 0.1);
            if (std::abs(pair.gap - overallSpan) > sameOverallGapTolerance)
            {
                return false;
            }
        }

        const double lowBoundary = pair.verticalMeasurement ? bounds.minY : bounds.minX;
        const double highBoundary = pair.verticalMeasurement ? bounds.maxY : bounds.maxX;
        const double boundaryTolerance = std::max(axisTolerance * 3.0, overallSpan * 0.08);
        const auto coordinate = [&](const LineProjectionFaceCandidate& face) {
            return pair.verticalMeasurement ? face.centerY : face.centerX;
        };
        const auto isLowDatum = [&](const LineProjectionFaceCandidate& face) {
            return face.outerContourDatum && std::abs(coordinate(face) - lowBoundary) <= boundaryTolerance;
        };
        const auto isHighDatum = [&](const LineProjectionFaceCandidate& face) {
            return face.outerContourDatum && std::abs(coordinate(face) - highBoundary) <= boundaryTolerance;
        };
        const auto isNearLowInterior = [&](const LineProjectionFaceCandidate& face) {
            return !face.outerContourDatum &&
                std::abs(coordinate(face) - lowBoundary) <= boundaryTolerance &&
                std::abs(coordinate(face) - highBoundary) > boundaryTolerance;
        };
        const auto isNearHighInterior = [&](const LineProjectionFaceCandidate& face) {
            return !face.outerContourDatum &&
                std::abs(coordinate(face) - highBoundary) <= boundaryTolerance &&
                std::abs(coordinate(face) - lowBoundary) > boundaryTolerance;
        };

        return (isLowDatum(first) && isNearHighInterior(second)) ||
               (isLowDatum(second) && isNearHighInterior(first)) ||
               (isHighDatum(first) && isNearLowInterior(second)) ||
               (isHighDatum(second) && isNearLowInterior(first));
    };

    auto intervalsConflict = [&](const LaneInterval& a, const LaneInterval& b) {
        return RangeOverlap(a.min, a.max, b.min, b.max) > -lanePadding;
    };

    auto assignLaneDistance = [&](std::vector<std::vector<LaneInterval>>& lanes,
                                  double intervalMin,
                                  double intervalMax,
                                  bool avoidOverallLane,
                                  double overallMin,
                                  double overallMax) {
        LaneInterval interval;
        interval.min = std::min(intervalMin, intervalMax);
        interval.max = std::max(intervalMin, intervalMax);

        for (size_t laneIndex = 0;; ++laneIndex)
        {
            if (laneIndex >= lanes.size())
            {
                lanes.push_back(std::vector<LaneInterval>());
            }

            const double distance = outsideOffset + static_cast<double>(laneIndex) * laneStep;
            bool occupied = false;
            if (avoidOverallLane && std::abs(distance - overallOffset) <= laneStep * 0.55)
            {
                LaneInterval overall;
                overall.min = std::min(overallMin, overallMax);
                overall.max = std::max(overallMin, overallMax);
                occupied = intervalsConflict(interval, overall);
            }

            if (!occupied)
            {
                for (const LaneInterval& used : lanes[laneIndex])
                {
                    if (intervalsConflict(interval, used))
                    {
                        occupied = true;
                        break;
                    }
                }
            }

            if (!occupied)
            {
                lanes[laneIndex].push_back(interval);
                return distance;
            }
        }
    };

    for (const FacePair& pair : pairs)
    {
        const LineProjectionFaceCandidate& first = faces[pair.first];
        const LineProjectionFaceCandidate& second = faces[pair.second];
        const FacePairKey pairKey = MakeFacePairKey(first, second);
        const bool usesCurrentViewDatum = first.outerContourDatum || second.outerContourDatum;
        const bool bothCurrentViewDatum = first.outerContourDatum && second.outerContourDatum;
        const bool firstOverallDatum = isMainViewDatumFace(first) || isProjectedDatumFace(first);
        const bool secondOverallDatum = isMainViewDatumFace(second) || isProjectedDatumFace(second);
        const FacePlaneSignature firstPlane = MakeFacePlaneSignature(first.normal, first.planePoint);
        const FacePlaneSignature secondPlane = MakeFacePlaneSignature(second.normal, second.planePoint);
        if (ContainsFacePairKey(usedViewFacePairs, pairKey))
        {
            logDiagnosticPairReject("createSkip", first, second, pair.gap, pair.modelGap, "usedViewFacePair");
            continue;
        }
        if (ContainsFacePairKey(usedFacePairs, pairKey))
        {
            logDiagnosticPairReject("createSkip", first, second, pair.gap, pair.modelGap, "usedGlobalFacePair");
            continue;
        }
        auto isGloballyPairedPlane = [&](const FacePlaneSignature& plane) {
            return mainViewFacePairRules != nullptr &&
                plane.valid &&
                ContainsFacePlaneSignature(mainViewFacePairRules->usedPairedPlanes, plane);
        };
        const bool firstGloballyPaired = isGloballyPairedPlane(firstPlane);
        const bool secondGloballyPaired = isGloballyPairedPlane(secondPlane);
        if (requireMainViewDatumPair &&
            ((!firstOverallDatum && firstGloballyPaired) ||
             (!secondOverallDatum && secondGloballyPaired)))
        {
            ++skippedGlobalPairedFaceReuse;
            logDiagnosticPairReject("createSkip", first, second, pair.gap, pair.modelGap, "globallyPairedPlaneReuse");
            continue;
        }

        const bool isPlateThicknessDimension =
            IsPlateThicknessGap(pair.modelGap, sheetMetalThickness);
        if (isPlateThicknessDimension && smallPlateThicknessDimensionCreated)
        {
            ++skippedDuplicatePlateThickness;
            logDiagnosticPairReject("createSkip", first, second, pair.gap, pair.modelGap, "duplicatePlateThickness");
            continue;
        }
        if (!isPlateThicknessDimension &&
            IsSmallDimensionGap(pair.modelGap, sheetMetalThickness))
        {
            ++skippedSmallNonThickness;
            logDiagnosticPairReject("createSkip", first, second, pair.gap, pair.modelGap, "smallNonThickness");
            continue;
        }
        if (ContainsSamePlaneFaceCandidate(overallDimensionThicknessFaces, first) ||
            ContainsSamePlaneFaceCandidate(overallDimensionThicknessFaces, second))
        {
            ++skippedOverallDatumThicknessFaces;
            logDiagnosticPairReject("createSkip", first, second, pair.gap, pair.modelGap, "overallDatumThicknessFace");
            continue;
        }
        if (bothCurrentViewDatum)
        {
            const double overallSpan = pair.verticalMeasurement ? height : width;
            const double sameOverallGapTolerance = std::max(0.05, axisTolerance * 0.1);
            if (overallSpan > 1.0e-6 &&
                std::abs(pair.gap - overallSpan) <= sameOverallGapTolerance)
            {
                ++skippedOppositeDatumNearOverallPairs;
                logDiagnosticPairReject("createSkip", first, second, pair.gap, pair.modelGap, "duplicateOverallDatumPair");
                continue;
            }
        }
        if (isCoveredByClosedCurveDimension(pair, first, second))
        {
            ++skippedClosedCurveParallelPairs;
            logDiagnosticPairReject("createSkip", first, second, pair.gap, pair.modelGap, "coveredByClosedCurveDimension");
            continue;
        }
        if (isInnerClosedCurveFace(first) || isInnerClosedCurveFace(second))
        {
            ++skippedInnerClosedCurveFaces;
            logDiagnosticPairReject("createSkip", first, second, pair.gap, pair.modelGap, "innerClosedCurveFace");
            continue;
        }
        if (isOppositeDatumNearOverallPair(pair, first, second))
        {
            ++skippedOppositeDatumNearOverallPairs;
            logDiagnosticPairReject("createSkip", first, second, pair.gap, pair.modelGap, "oppositeDatumNearOverall");
            continue;
        }
        if (IsPlateThicknessOffsetFromUsedFace(first, overallDimensionDatumFaces, sheetMetalThickness, true) ||
            IsPlateThicknessOffsetFromUsedFace(second, overallDimensionDatumFaces, sheetMetalThickness, true))
        {
            ++skippedOverallDatumThicknessOffsetPairs;
            logDiagnosticPairReject("createSkip", first, second, pair.gap, pair.modelGap, "overallDatumThicknessOffset");
            continue;
        }
        if (IsPlateThicknessOffsetFromUsedFace(first, usedDimensionFaces, sheetMetalThickness) ||
            IsPlateThicknessOffsetFromUsedFace(second, usedDimensionFaces, sheetMetalThickness))
        {
            ++skippedPlateThicknessOffsetPairs;
            logDiagnosticPairReject("createSkip", first, second, pair.gap, pair.modelGap, "usedFaceThicknessOffset");
            continue;
        }
        if ((!first.outerContourDatum && !firstOverallDatum && containsProjectionLineSignature(makeProjectionLineSignature(first))) ||
            (!second.outerContourDatum && !secondOverallDatum && containsProjectionLineSignature(makeProjectionLineSignature(second))))
        {
            ++skippedProjectionLineReuse;
            logDiagnosticPairReject("createSkip", first, second, pair.gap, pair.modelGap, "projectionLineReuse");
            continue;
        }
        if ((!first.outerContourDatum && !firstOverallDatum && ContainsFacePlaneSignature(usedPlanes, firstPlane)) ||
            (!second.outerContourDatum && !secondOverallDatum && ContainsFacePlaneSignature(usedPlanes, secondPlane)))
        {
            logDiagnosticPairReject("createSkip", first, second, pair.gap, pair.modelGap, "usedPlaneReuse");
            continue;
        }

        bool dimensionCreated = false;
        DimensionFootprint footprint;
        footprint.verticalMeasurement = pair.verticalMeasurement;
        if (pair.angledMeasurement)
        {
            const double midAlong = (pair.overlapMin + pair.overlapMax) * 0.5;
            const double midOffset = (first.lineOffset + second.lineOffset) * 0.5;
            const double baseX = first.directionX * midAlong + first.normalX * midOffset;
            const double baseY = first.directionY * midAlong + first.normalY * midOffset;
            double sideSign =
                Dot2D(baseX - boundsCenterX, baseY - boundsCenterY, first.normalX, first.normalY) >= 0.0 ? 1.0 : -1.0;
            if (std::abs(Dot2D(baseX - boundsCenterX, baseY - boundsCenterY, first.normalX, first.normalY)) <= axisTolerance)
            {
                sideSign = first.normalY >= 0.0 ? 1.0 : -1.0;
            }

            footprint.verticalMeasurement = false;
            footprint.side = sideSign >= 0.0 ? 1 : -1;
            footprint.first = std::min(first.lineOffset, second.lineOffset);
            footprint.second = std::max(first.lineOffset, second.lineOffset);
            footprint.pick = midAlong;
            if (isDuplicateFootprint(footprint))
            {
                continue;
            }

            const double originDistance = outsideOffset + laneStep * 0.5;
            const double originX = baseX + first.normalX * sideSign * originDistance;
            const double originY = baseY + first.normalY * sideSign * originDistance;
            const NXOpen::Point3d origin(originX, originY, 0.0);
            dimensionCreated = CreateFacePairDimension(
                session,
                workPart,
                view,
                first,
                second,
                NXOpen::Annotations::DimensionMeasurementBuilder::MeasurementMethodPerpendicular,
                origin,
                viewLabel);
        }
        else if (pair.verticalMeasurement)
        {
            const LineProjectionFaceCandidate& lower = first.centerY <= second.centerY ? first : second;
            const LineProjectionFaceCandidate& upper = first.centerY <= second.centerY ? second : first;
            const double overlapWidth = std::max(0.0, pair.overlapMax - pair.overlapMin);
            const double pickMargin = std::min(std::max(0.5, axisTolerance * 2.0), overlapWidth * 0.25);
            const double leftPickX = pair.overlapMin + pickMargin;
            const double rightPickX = pair.overlapMax - pickMargin;
            const double insidePickX = (pair.overlapMin + pair.overlapMax) * 0.5;
            const int leftCrossings =
                CountDimensionExtensionCrossings(lines, lower.line, upper.line, leftPickX, lower.centerY, bounds.minX - outsideOffset, lower.centerY, axisTolerance) +
                CountDimensionExtensionCrossings(lines, lower.line, upper.line, leftPickX, upper.centerY, bounds.minX - outsideOffset, upper.centerY, axisTolerance);
            const int rightCrossings =
                CountDimensionExtensionCrossings(lines, lower.line, upper.line, rightPickX, lower.centerY, bounds.maxX + outsideOffset, lower.centerY, axisTolerance) +
                CountDimensionExtensionCrossings(lines, lower.line, upper.line, rightPickX, upper.centerY, bounds.maxX + outsideOffset, upper.centerY, axisTolerance);
            const bool preferLeftSide = insidePickX <= boundsCenterX;
            const bool useInside = isPlateThicknessDimension && leftCrossings > 0 && rightCrossings > 0;
            bool useLeftSide = !useInside &&
                (leftCrossings < rightCrossings ||
                 (leftCrossings == rightCrossings && preferLeftSide));
            if (!useInside && projectedDatumRule.valid && projectedDatumRule.verticalMeasurement)
            {
                useLeftSide = false;
            }
            const double pickX = useInside ? insidePickX : (useLeftSide ? leftPickX : rightPickX);
            footprint.side = useLeftSide ? -1 : 1;
            if (useInside)
            {
                footprint.side = 0;
            }
            footprint.first = lower.centerY;
            footprint.second = upper.centerY;
            footprint.pick = pickX;
            if (isDuplicateFootprint(footprint))
            {
                continue;
            }

            const double intervalMin = lower.centerY;
            const double intervalMax = upper.centerY;
            double originX = pickX;
            if (!useInside)
            {
                const double distance = useLeftSide
                    ? assignLaneDistance(leftLanes, intervalMin, intervalMax, true, bounds.minY, bounds.maxY)
                    : assignLaneDistance(rightLanes, intervalMin, intervalMax, false, 0.0, 0.0);
                originX = useLeftSide
                    ? bounds.minX - distance
                    : bounds.maxX + distance;
            }
            else
            {
                ++insidePlacedToAvoidCrossing;
            }
            const double originY = (lower.centerY + upper.centerY) * 0.5;
            const NXOpen::Point3d origin(originX, originY, 0.0);
            dimensionCreated = CreateFacePairDimension(
                session,
                workPart,
                view,
                lower,
                upper,
                NXOpen::Annotations::DimensionMeasurementBuilder::MeasurementMethodVertical,
                origin,
                viewLabel);
        }
        else
        {
            const LineProjectionFaceCandidate& left = first.centerX <= second.centerX ? first : second;
            const LineProjectionFaceCandidate& right = first.centerX <= second.centerX ? second : first;
            const double overlapHeight = std::max(0.0, pair.overlapMax - pair.overlapMin);
            const double pickMargin = std::min(std::max(0.5, axisTolerance * 2.0), overlapHeight * 0.25);
            const double bottomPickY = pair.overlapMin + pickMargin;
            const double topPickY = pair.overlapMax - pickMargin;
            const double insidePickY = (pair.overlapMin + pair.overlapMax) * 0.5;
            const int bottomCrossings =
                CountDimensionExtensionCrossings(lines, left.line, right.line, left.centerX, bottomPickY, left.centerX, bounds.minY - outsideOffset, axisTolerance) +
                CountDimensionExtensionCrossings(lines, left.line, right.line, right.centerX, bottomPickY, right.centerX, bounds.minY - outsideOffset, axisTolerance);
            const int topCrossings =
                CountDimensionExtensionCrossings(lines, left.line, right.line, left.centerX, topPickY, left.centerX, bounds.maxY + outsideOffset, axisTolerance) +
                CountDimensionExtensionCrossings(lines, left.line, right.line, right.centerX, topPickY, right.centerX, bounds.maxY + outsideOffset, axisTolerance);
            const bool preferBottomSide = insidePickY <= boundsCenterY;
            const bool useInside = isPlateThicknessDimension && bottomCrossings > 0 && topCrossings > 0;
            bool useBottomSide = !useInside &&
                (bottomCrossings < topCrossings ||
                 (bottomCrossings == topCrossings && preferBottomSide));
            if (!useInside && projectedDatumRule.valid && !projectedDatumRule.verticalMeasurement)
            {
                useBottomSide = !projectedDatumRule.useHighDatum;
            }
            const double pickY = useInside ? insidePickY : (useBottomSide ? bottomPickY : topPickY);
            footprint.side = useBottomSide ? -1 : 1;
            if (useInside)
            {
                footprint.side = 0;
            }
            footprint.first = left.centerX;
            footprint.second = right.centerX;
            footprint.pick = pickY;
            if (isDuplicateFootprint(footprint))
            {
                continue;
            }

            const double intervalMin = left.centerX;
            const double intervalMax = right.centerX;
            double originY = pickY;
            if (!useInside)
            {
                const double distance = useBottomSide
                    ? assignLaneDistance(bottomLanes, intervalMin, intervalMax, false, 0.0, 0.0)
                    : assignLaneDistance(topLanes, intervalMin, intervalMax, true, bounds.minX, bounds.maxX);
                originY = useBottomSide
                    ? bounds.minY - distance
                    : bounds.maxY + distance;
            }
            else
            {
                ++insidePlacedToAvoidCrossing;
            }
            const double originX = (left.centerX + right.centerX) * 0.5;
            const NXOpen::Point3d origin(originX, originY, 0.0);
            dimensionCreated = CreateFacePairDimension(
                session,
                workPart,
                view,
                left,
                right,
                NXOpen::Annotations::DimensionMeasurementBuilder::MeasurementMethodHorizontal,
                origin,
                viewLabel);
        }

        if (dimensionCreated)
        {
            const int firstUseCount = ++faceDimensionUseCounts[first.faceTag];
            const int secondUseCount = ++faceDimensionUseCounts[second.faceTag];
            if (!first.outerContourDatum && !firstOverallDatum && firstPlane.valid &&
                !ContainsFacePlaneSignature(usedPlanes, firstPlane))
            {
                usedPlanes.push_back(firstPlane);
            }
            if (!second.outerContourDatum && !secondOverallDatum && secondPlane.valid &&
                !ContainsFacePlaneSignature(usedPlanes, secondPlane))
            {
                usedPlanes.push_back(secondPlane);
            }
            rememberDimensionFace(first);
            rememberDimensionFace(second);
            if (!firstOverallDatum)
            {
                rememberProjectionLine(first);
            }
            if (!secondOverallDatum)
            {
                rememberProjectionLine(second);
            }
            if (mainViewFacePairRules != nullptr)
            {
                if (!firstOverallDatum && firstPlane.valid &&
                    !ContainsFacePlaneSignature(mainViewFacePairRules->usedPairedPlanes, firstPlane))
                {
                    mainViewFacePairRules->usedPairedPlanes.push_back(firstPlane);
                }
                if (!secondOverallDatum && secondPlane.valid &&
                    !ContainsFacePlaneSignature(mainViewFacePairRules->usedPairedPlanes, secondPlane))
                {
                    mainViewFacePairRules->usedPairedPlanes.push_back(secondPlane);
                }
            }
            usedFacePairs.push_back(pairKey);
            usedViewFacePairs.push_back(pairKey);
            usedFootprints.push_back(footprint);
            if (isPlateThicknessDimension)
            {
                smallPlateThicknessDimensionCreated = true;
            }
            ++created;

            std::ostringstream createdLog;
            createdLog << "AutoCreateThreeViews: " << viewLabel << " face-to-face dimension created"
                << " firstFace=" << static_cast<unsigned long long>(first.faceTag)
                << ", firstUseCount=" << firstUseCount
                << ", secondFace=" << static_cast<unsigned long long>(second.faceTag)
                << ", secondUseCount=" << secondUseCount
                << ", normalDot=" << DotVector(NormalizeVector(first.normal), NormalizeVector(second.normal))
                << ", modelGap=" << pair.modelGap
                << ", drawingGap=" << pair.gap
                << ", angled=" << (pair.angledMeasurement ? "yes" : "no")
                << ", firstOuterDatum=" << (first.outerContourDatum ? "yes" : "no")
                << ", secondOuterDatum=" << (second.outerContourDatum ? "yes" : "no") << ".";
            WriteLine(session, createdLog.str());
        }
    }

    std::ostringstream repeatedFaces;
    repeatedFaces << "AutoCreateThreeViews: " << viewLabel << " repeated dimension face use";
    bool hasRepeatedFace = false;
    for (const std::map<tag_t, int>::value_type& entry : faceDimensionUseCounts)
    {
        if (entry.second <= 1)
        {
            continue;
        }
        if (!hasRepeatedFace)
        {
            repeatedFaces << " ";
            hasRepeatedFace = true;
        }
        else
        {
            repeatedFaces << ", ";
        }
        repeatedFaces << static_cast<unsigned long long>(entry.first) << ":" << entry.second;
    }
    if (!hasRepeatedFace)
    {
        repeatedFaces << " none";
    }
    repeatedFaces << ".";
    WriteLine(session, repeatedFaces.str());

    if (!mainAxisRejectReasons.empty())
    {
        std::ostringstream reasons;
        reasons << "AutoCreateThreeViews: " << viewLabel << " main-axis reject reasons";
        for (const auto& entry : mainAxisRejectReasons)
        {
            reasons << " " << entry.first << "=" << entry.second;
        }
        reasons << ".";
        WriteLine(session, reasons.str());
    }

    std::ostringstream log;
    log << "AutoCreateThreeViews: " << viewLabel << " face-to-face dimensions finished"
        << " rawFaces=" << rawFaceCount
        << ", uniqueFaces=" << faces.size()
        << ", outerDatumFaces=" << outerDatumFaces
        << ", pairs=" << pairs.size()
        << ", created=" << created
        << ", rejectedByMainAxisRules=" << rejectedByMainAxisRules
        << ", rejectedInnerClosedCurveFaces=" << rejectedInnerClosedCurveFaces
        << ", skippedNonOverallDatumPairs=" << skippedNonOverallDatumPairs
        << ", skippedProjectedViewWrongDirection=" << skippedProjectedViewWrongDirection
        << ", skippedProjectedViewNoDatum=" << skippedProjectedViewNoDatum
        << ", projectedDatumRule=" << (projectedDatumRule.valid ? "yes" : "no")
        << ", projectedDatumMeasurement=" << (projectedDatumRule.verticalMeasurement ? "Y" : "X")
        << ", projectedDatumSide=" << (projectedDatumRule.useHighDatum ? "max" : "min")
        << ", projectedDatumCoord=" << (hasProjectedDatumCoordinate ? projectedDatumCoordinate : 0.0)
        << ", sheetMetalThickness=" << sheetMetalThickness
        << ", smallPlateThicknessCreated=" << (smallPlateThicknessDimensionCreated ? "yes" : "no")
        << ", skippedDuplicatePlateThickness=" << skippedDuplicatePlateThickness
        << ", skippedSmallNonThickness=" << skippedSmallNonThickness
        << ", skippedPlateThicknessOffsetPairs=" << skippedPlateThicknessOffsetPairs
        << ", skippedOverallDatumThicknessOffsetPairs=" << skippedOverallDatumThicknessOffsetPairs
        << ", skippedOverallDatumThicknessFaces=" << skippedOverallDatumThicknessFaces
        << ", skippedOppositeDatumNearOverallPairs=" << skippedOppositeDatumNearOverallPairs
        << ", skippedProjectionLineReuse=" << skippedProjectionLineReuse
        << ", skippedGlobalPairedFaceReuse=" << skippedGlobalPairedFaceReuse
        << ", skippedClosedCurveParallelPairs=" << skippedClosedCurveParallelPairs
        << ", skippedInnerClosedCurveFaces=" << skippedInnerClosedCurveFaces
        << ", insidePlacedToAvoidCrossing=" << insidePlacedToAvoidCrossing
        << ", usedViewFacePairs=" << usedViewFacePairs.size()
        << ", usedPlanes=" << usedPlanes.size()
        << ", usedDimensionFaces=" << usedDimensionFaces.size()
        << ", overallDatumFaces=" << overallDimensionDatumFaces.size()
        << ", overallThicknessFaces=" << overallDimensionThicknessFaces.size()
        << ", usedProjectionLines=" << usedProjectionLines.size()
        << ", globalPairedPlanes=" << (mainViewFacePairRules != nullptr ? mainViewFacePairRules->usedPairedPlanes.size() : 0)
        << ", innerLoopParentFaces=" << innerLoopAdjacentFaces.parentFacesWithInnerLoops
        << ", innerLoopEdges=" << innerLoopAdjacentFaces.innerLoopEdges
        << ", innerLoopAdjacentFaceTags=" << innerLoopAdjacentFaces.faceTags.size()
        << ", faceUseCountEntries=" << faceDimensionUseCounts.size()
        << ", globalFacePairs=" << usedFacePairs.size()
        << ", pairOrder=nearestFaceFirst"
        << ", outsideOffset=" << outsideOffset
        << ", laneStep=" << laneStep << ".";
    WriteLine(session, log.str());
}

double PointToSegmentDistance(
    const LineSegmentCandidate& line,
    double x,
    double y)
{
    const double dx = line.endX - line.startX;
    const double dy = line.endY - line.startY;
    const double lengthSquared = dx * dx + dy * dy;
    if (lengthSquared <= 1.0e-12)
    {
        return DrawingPointDistance(x, y, line.startX, line.startY);
    }

    const double t = ClampDouble(
        ((x - line.startX) * dx + (y - line.startY) * dy) / lengthSquared,
        0.0,
        1.0);
    const double projectionX = line.startX + dx * t;
    const double projectionY = line.startY + dy * t;
    return DrawingPointDistance(x, y, projectionX, projectionY);
}

double PointToNearestLineEndpointDistance(
    const LineSegmentCandidate& line,
    double x,
    double y)
{
    return std::min(
        DrawingPointDistance(x, y, line.startX, line.startY),
        DrawingPointDistance(x, y, line.endX, line.endY));
}

struct AngleDirectionSignature
{
    long long firstDirection = 0;
    long long secondDirection = 0;
    long long includedAngle = 0;
};

long long QuantizeUnorientedDrawingDirection(double x, double y)
{
    if (Normalize2D(x, y) <= 1.0e-9)
    {
        return 0;
    }

    const double pi = 3.14159265358979323846;
    double angle = std::atan2(y, x);
    while (angle < 0.0)
    {
        angle += pi;
    }
    while (angle >= pi)
    {
        angle -= pi;
    }

    const double directionTolerance = pi / 180.0;
    return static_cast<long long>(std::llround(angle / directionTolerance));
}

AngleDirectionSignature MakeAngleDirectionSignature(
    const LineProjectionFaceCandidate& first,
    const LineProjectionFaceCandidate& second,
    double drawingAngleDeg)
{
    AngleDirectionSignature signature;
    signature.firstDirection = QuantizeUnorientedDrawingDirection(first.directionX, first.directionY);
    signature.secondDirection = QuantizeUnorientedDrawingDirection(second.directionX, second.directionY);
    if (signature.firstDirection > signature.secondDirection)
    {
        std::swap(signature.firstDirection, signature.secondDirection);
    }
    signature.includedAngle = static_cast<long long>(std::llround(drawingAngleDeg));
    return signature;
}

bool SameAngleDirectionSignature(
    const AngleDirectionSignature& first,
    const AngleDirectionSignature& second)
{
    return first.firstDirection == second.firstDirection &&
           first.secondDirection == second.secondDirection &&
           first.includedAngle == second.includedAngle;
}

bool ContainsAngleDirectionSignature(
    const std::vector<AngleDirectionSignature>& signatures,
    const AngleDirectionSignature& signature)
{
    for (const AngleDirectionSignature& existing : signatures)
    {
        if (SameAngleDirectionSignature(existing, signature))
        {
            return true;
        }
    }
    return false;
}

void CreateViewAngleDimensions(
    NXOpen::Session* session,
    NXOpen::Part* workPart,
    ShallowDetailFilterCache& shallowCache,
    NXOpen::Drawings::DraftingView* view,
    const std::string& label,
    const std::vector<DraftingCurveExtent>& extents,
    const LayoutBounds& bounds,
    std::vector<FacePairKey>& usedAngleFacePairs)
{
    const double width = BoundsWidth(bounds);
    const double height = BoundsHeight(bounds);
    if (workPart == nullptr || view == nullptr || width <= 1.0 || height <= 1.0)
    {
        return;
    }

    const std::string viewLabel = label.empty() ? "view" : label;
    const std::vector<LineSegmentCandidate> lines = CollectLineSegments(view, extents);
    const double axisTolerance = std::max(0.15, std::max(width, height) * 0.004);
    std::vector<LineProjectionFaceCandidate> faces =
        CollectLineProjectionFaceCandidates(session, workPart, view, viewLabel + " angle", lines, axisTolerance, &shallowCache);
    const size_t rawFaceCount = faces.size();
    faces = CollapseSamePlaneFaceCandidates(faces);
    const size_t uniqueFaceCount = faces.size();
    int closeParallelFacePairs = 0;
    int closeParallelExcludedFaces = 0;
    if (faces.size() >= 2)
    {
        std::vector<bool> excludeFromAngle(faces.size(), false);
        for (size_t i = 0; i < faces.size(); ++i)
        {
            for (size_t j = i + 1; j < faces.size(); ++j)
            {
                const double normalAlignment =
                    std::abs(DotVector(NormalizeVector(faces[i].normal), NormalizeVector(faces[j].normal)));
                if (normalAlignment < 0.995)
                {
                    continue;
                }

                double modelGap = 0.0;
                if (!TryComputeModelFaceGap(faces[i], faces[j], modelGap) || modelGap >= 1.0)
                {
                    continue;
                }

                if (!excludeFromAngle[i])
                {
                    excludeFromAngle[i] = true;
                    ++closeParallelExcludedFaces;
                }
                if (!excludeFromAngle[j])
                {
                    excludeFromAngle[j] = true;
                    ++closeParallelExcludedFaces;
                }
                ++closeParallelFacePairs;
            }
        }

        if (closeParallelExcludedFaces > 0)
        {
            std::vector<LineProjectionFaceCandidate> filteredFaces;
            filteredFaces.reserve(faces.size() - static_cast<size_t>(closeParallelExcludedFaces));
            for (size_t index = 0; index < faces.size(); ++index)
            {
                if (!excludeFromAngle[index])
                {
                    filteredFaces.push_back(faces[index]);
                }
            }
            faces.swap(filteredFaces);
        }
    }
    if (faces.size() < 2)
    {
        std::ostringstream log;
        log << "AutoCreateThreeViews: " << viewLabel
            << " angle dimensions skipped; less than 2 usable planar faces"
            << " rawFaces=" << rawFaceCount
            << ", uniqueFaces=" << uniqueFaceCount
            << ", closeParallelFacePairs=" << closeParallelFacePairs
            << ", closeParallelExcludedFaces=" << closeParallelExcludedFaces
            << ", usableFaces=" << faces.size() << ".";
        WriteLine(session, log.str());
        return;
    }

    struct AnglePair
    {
        size_t first = 0;
        size_t second = 0;
        bool viaRoundTransition = false;
        double intersectionX = 0.0;
        double intersectionY = 0.0;
        double faceAngleDeg = 0.0;
        double drawingAngleDeg = 0.0;
        double extensionDistance = 0.0;
        double score = 0.0;
        bool visibleVirtualIntersection = false;
    };

    std::vector<AnglePair> pairs;
    const double radiansToDegrees = 180.0 / 3.14159265358979323846;
    const double minimumAngleDeg = 2.0;
    int skippedDisconnected = 0;
    int skippedRightAngle = 0;
    int skippedParallel = 0;
    int skippedFarIntersection = 0;
    int acceptedVisibleVirtualIntersection = 0;

    for (size_t i = 0; i < faces.size(); ++i)
    {
        for (size_t j = i + 1; j < faces.size(); ++j)
        {
            const LineProjectionFaceCandidate& first = faces[i];
            const LineProjectionFaceCandidate& second = faces[j];
            if (first.faceTag == second.faceTag || first.line.curve == nullptr || second.line.curve == nullptr ||
                first.line.curve == second.line.curve)
            {
                continue;
            }

            bool viaRoundTransition = false;
            const bool connectedForAngle =
                FacesConnectedForAngle(first.faceTag, second.faceTag, viaRoundTransition);

            const double faceAlignment =
                ClampDouble(std::abs(DotVector(NormalizeVector(first.normal), NormalizeVector(second.normal))), 0.0, 1.0);
            const double faceAngleDeg = std::acos(faceAlignment) * radiansToDegrees;
            if (faceAngleDeg < minimumAngleDeg)
            {
                ++skippedParallel;
                continue;
            }
            if (std::abs(faceAngleDeg - 90.0) < minimumAngleDeg)
            {
                ++skippedRightAngle;
                continue;
            }

            const double lineAlignment =
                ClampDouble(std::abs(Dot2D(first.directionX, first.directionY, second.directionX, second.directionY)), 0.0, 1.0);
            const double drawingAngleDeg = std::acos(lineAlignment) * radiansToDegrees;
            if (drawingAngleDeg < minimumAngleDeg)
            {
                ++skippedParallel;
                continue;
            }
            if (std::abs(drawingAngleDeg - 90.0) < minimumAngleDeg)
            {
                ++skippedRightAngle;
                continue;
            }

            double intersectionX = 0.0;
            double intersectionY = 0.0;
            if (!TryIntersectDrawingLines(first.line, second.line, intersectionX, intersectionY))
            {
                ++skippedParallel;
                continue;
            }

            const double firstDistance = PointToSegmentDistance(first.line, intersectionX, intersectionY);
            const double secondDistance = PointToSegmentDistance(second.line, intersectionX, intersectionY);
            const double extensionDistance = std::max(firstDistance, secondDistance);
            const double endpointDistance = std::min(
                PointToNearestLineEndpointDistance(first.line, intersectionX, intersectionY),
                PointToNearestLineEndpointDistance(second.line, intersectionX, intersectionY));
            const bool visibleVirtualIntersection =
                !connectedForAngle &&
                extensionDistance <= std::max(0.35, axisTolerance * 2.0) &&
                endpointDistance <= std::max(2.0, axisTolerance * 8.0);
            if (!connectedForAngle && !visibleVirtualIntersection)
            {
                ++skippedDisconnected;
                continue;
            }

            const double allowedExtension = viaRoundTransition
                ? std::max(12.0, std::min(first.line.length, second.line.length) * 0.45)
                : std::max(5.0, axisTolerance * 8.0);
            if (!visibleVirtualIntersection && extensionDistance > allowedExtension)
            {
                ++skippedFarIntersection;
                continue;
            }

            AnglePair pair;
            pair.first = i;
            pair.second = j;
            pair.viaRoundTransition = viaRoundTransition;
            pair.intersectionX = intersectionX;
            pair.intersectionY = intersectionY;
            pair.faceAngleDeg = faceAngleDeg;
            pair.drawingAngleDeg = drawingAngleDeg;
            pair.extensionDistance = extensionDistance;
            pair.visibleVirtualIntersection = visibleVirtualIntersection;
            pair.score = extensionDistance * 100.0 - std::min(first.length, second.length);
            pairs.push_back(pair);
            if (visibleVirtualIntersection)
            {
                ++acceptedVisibleVirtualIntersection;
            }
        }
    }

    if (pairs.empty())
    {
        std::ostringstream log;
        log << "AutoCreateThreeViews: " << viewLabel << " angle dimensions skipped; no valid non-90 face pairs"
            << " rawFaces=" << rawFaceCount
            << ", uniqueFaces=" << uniqueFaceCount
            << ", closeParallelFacePairs=" << closeParallelFacePairs
            << ", closeParallelExcludedFaces=" << closeParallelExcludedFaces
            << ", usableFaces=" << faces.size()
            << ", skippedDisconnected=" << skippedDisconnected
            << ", skippedRightAngle=" << skippedRightAngle
            << ", skippedParallel=" << skippedParallel
            << ", skippedFarIntersection=" << skippedFarIntersection
            << ", acceptedVisibleVirtualIntersection=" << acceptedVisibleVirtualIntersection << ".";
        WriteLine(session, log.str());
        return;
    }

    std::sort(pairs.begin(), pairs.end(), [](const AnglePair& a, const AnglePair& b) {
        if (std::abs(a.score - b.score) > 0.01)
        {
            return a.score < b.score;
        }
        return a.drawingAngleDeg > b.drawingAngleDeg;
    });

    int created = 0;
    int skippedDuplicate = 0;
    int skippedParallelAngleDuplicate = 0;
    int directPairs = 0;
    int transitionPairs = 0;
    int visibleVirtualPairs = 0;
    std::vector<AngleDirectionSignature> usedAngleDirectionSignatures;
    for (const AnglePair& pair : pairs)
    {
        const LineProjectionFaceCandidate& first = faces[pair.first];
        const LineProjectionFaceCandidate& second = faces[pair.second];
        const FacePairKey pairKey = MakeFacePairKey(first, second);
        if (ContainsFacePairKey(usedAngleFacePairs, pairKey))
        {
            ++skippedDuplicate;
            continue;
        }

        const AngleDirectionSignature directionSignature =
            MakeAngleDirectionSignature(first, second, pair.drawingAngleDeg);
        if (ContainsAngleDirectionSignature(usedAngleDirectionSignatures, directionSignature))
        {
            ++skippedParallelAngleDuplicate;
            continue;
        }

        if (CreateLocalAngleDimension(
                session,
                workPart,
                view,
                first.line,
                second.line,
                pair.intersectionX,
                pair.intersectionY,
                viewLabel))
        {
            usedAngleFacePairs.push_back(pairKey);
            usedAngleDirectionSignatures.push_back(directionSignature);
            ++created;
            if (pair.viaRoundTransition)
            {
                ++transitionPairs;
            }
            else if (pair.visibleVirtualIntersection)
            {
                ++visibleVirtualPairs;
            }
            else
            {
                ++directPairs;
            }
        }
    }

    std::ostringstream log;
    log << "AutoCreateThreeViews: " << viewLabel << " angle dimensions finished"
        << " rawFaces=" << rawFaceCount
        << ", uniqueFaces=" << uniqueFaceCount
        << ", closeParallelFacePairs=" << closeParallelFacePairs
        << ", closeParallelExcludedFaces=" << closeParallelExcludedFaces
        << ", usableFaces=" << faces.size()
        << ", pairs=" << pairs.size()
        << ", created=" << created
        << ", directCreated=" << directPairs
        << ", transitionCreated=" << transitionPairs
        << ", visibleVirtualCreated=" << visibleVirtualPairs
        << ", skippedDuplicate=" << skippedDuplicate
        << ", skippedParallelAngleDuplicate=" << skippedParallelAngleDuplicate
        << ", skippedDisconnected=" << skippedDisconnected
        << ", skippedRightAngle=" << skippedRightAngle
        << ", skippedParallel=" << skippedParallel
        << ", skippedFarIntersection=" << skippedFarIntersection
        << ", acceptedVisibleVirtualIntersection=" << acceptedVisibleVirtualIntersection
        << ", globalAnglePairs=" << usedAngleFacePairs.size() << ".";
    WriteLine(session, log.str());
}

void CreateFrontSlotDimensions(
    NXOpen::Session* session,
    NXOpen::Part* workPart,
    NXOpen::Drawings::DraftingView* view,
    const std::vector<DraftingCurveExtent>& extents,
    const LayoutBounds& bounds,
    double offset)
{
    const double width = BoundsWidth(bounds);
    const double height = BoundsHeight(bounds);
    if (width <= 1.0 || height <= 1.0)
    {
        return;
    }

    const std::vector<LineSegmentCandidate> lines = CollectLineSegments(view, extents);
    std::vector<LineSegmentCandidate> horizontalLines;
    const double axisTolerance = std::max(0.12, std::max(width, height) * 0.004);
    for (const LineSegmentCandidate& line : lines)
    {
        if (std::abs(line.startY - line.endY) <= axisTolerance && line.length >= std::max(1.0, width * 0.015))
        {
            horizontalLines.push_back(line);
        }
    }

    struct SlotCandidate
    {
        LineSegmentCandidate lower;
        LineSegmentCandidate upper;
        double slotWidth = 0.0;
        double slotLength = 0.0;
        double overlapMinX = 0.0;
        double overlapMaxX = 0.0;
        double score = 0.0;
    };

    std::vector<SlotCandidate> slots;
    for (size_t i = 0; i < horizontalLines.size(); ++i)
    {
        for (size_t j = i + 1; j < horizontalLines.size(); ++j)
        {
            const LineSegmentCandidate& a = horizontalLines[i];
            const LineSegmentCandidate& b = horizontalLines[j];
            const LineSegmentCandidate& lower = a.minY <= b.minY ? a : b;
            const LineSegmentCandidate& upper = a.minY <= b.minY ? b : a;
            const double gap = std::abs(upper.minY - lower.minY);
            const double overlapMinX = std::max(lower.minX, upper.minX);
            const double overlapMaxX = std::min(lower.maxX, upper.maxX);
            const double overlap = overlapMaxX - overlapMinX;
            if (gap < std::max(0.3, height * 0.015) ||
                gap > std::max(2.0, height * 0.35) ||
                overlap < std::max(2.0, gap * 1.8) ||
                overlap > width * 0.75)
            {
                continue;
            }

            const double centerY = (lower.minY + upper.minY) * 0.5;
            if (centerY <= bounds.minY + gap * 0.5 || centerY >= bounds.maxY - gap * 0.5)
            {
                continue;
            }

            SlotCandidate slot;
            slot.lower = lower;
            slot.upper = upper;
            slot.slotWidth = gap;
            slot.slotLength = overlap;
            slot.overlapMinX = overlapMinX;
            slot.overlapMaxX = overlapMaxX;
            slot.score = gap * 1000.0 - overlap;
            slots.push_back(slot);
        }
    }

    if (slots.empty())
    {
        WriteLine(session, "AutoCreateThreeViews: slot dimensions skipped; no slot line pairs found.");
        return;
    }

    std::sort(slots.begin(), slots.end(), [](const SlotCandidate& a, const SlotCandidate& b) {
        return a.score < b.score;
    });

    const SlotCandidate& slot = slots.front();
    const double insideMargin = std::max(2.0, offset * 0.35);
    const double pickX = (slot.overlapMinX + slot.overlapMaxX) * 0.5;
    int created = 0;

    const double widthOriginX = ClampDimensionOrigin(slot.overlapMinX - offset * 0.5, bounds.minX + insideMargin, bounds.maxX - insideMargin);
    const double widthOriginY = (slot.lower.minY + slot.upper.minY) * 0.5;
    if (CreateLocalHeightDimension(
            session,
            workPart,
            view,
            slot.lower,
            slot.upper,
            NXOpen::Point3d(widthOriginX, widthOriginY, 0.0),
            pickX,
            "slot width"))
    {
        ++created;
    }

    const bool lowerStartLeft = slot.lower.startX <= slot.lower.endX;
    CurveAssocCandidate leftPoint = MakeLineEndpointCandidate(slot.lower, lowerStartLeft);
    CurveAssocCandidate rightPoint = MakeLineEndpointCandidate(slot.lower, !lowerStartLeft);
    const double lengthOriginY = ClampDimensionOrigin(slot.lower.minY - offset * 0.65, bounds.minY + insideMargin, bounds.maxY - insideMargin);
    if (CreateCurvePointDimension(
            session,
            workPart,
            view,
            leftPoint,
            rightPoint,
            true,
            NXOpen::Point3d((slot.overlapMinX + slot.overlapMaxX) * 0.5, lengthOriginY, 0.0),
            "slot length"))
    {
        ++created;
    }

    std::ostringstream log;
    log << "AutoCreateThreeViews: slot dimensions finished"
        << " candidates=" << slots.size()
        << ", created=" << created
        << ", drawingWidth=" << slot.slotWidth
        << ", drawingLength=" << slot.slotLength << ".";
    WriteLine(session, log.str());
}

double PointDistance2d(double ax, double ay, double bx, double by)
{
    return std::sqrt((ax - bx) * (ax - bx) + (ay - by) * (ay - by));
}

bool LineEndpointNear(
    const LineSegmentCandidate& line,
    double x,
    double y,
    double tolerance,
    bool& useStart)
{
    const double startDistance = PointDistance2d(line.startX, line.startY, x, y);
    const double endDistance = PointDistance2d(line.endX, line.endY, x, y);
    if (startDistance <= tolerance || endDistance <= tolerance)
    {
        useStart = startDistance <= endDistance;
        return true;
    }
    return false;
}

void CreateFrontRectangularNotchDimensions(
    NXOpen::Session* session,
    NXOpen::Part* workPart,
    NXOpen::Drawings::DraftingView* view,
    const std::vector<DraftingCurveExtent>& extents,
    const LayoutBounds& bounds,
    double offset)
{
    const double width = BoundsWidth(bounds);
    const double height = BoundsHeight(bounds);
    if (width <= 1.0 || height <= 1.0)
    {
        return;
    }

    const std::vector<LineSegmentCandidate> lines = CollectLineSegments(view, extents);
    std::vector<LineSegmentCandidate> horizontalLines;
    std::vector<LineSegmentCandidate> verticalLines;
    const double axisTolerance = std::max(0.12, std::max(width, height) * 0.004);
    const double minFeature = std::max(1.0, std::min(width, height) * 0.015);
    const double maxFeatureWidth = width * 0.35;
    const double maxFeatureHeight = height * 0.60;
    for (const LineSegmentCandidate& line : lines)
    {
        const bool horizontal = std::abs(line.startY - line.endY) <= axisTolerance;
        const bool vertical = std::abs(line.startX - line.endX) <= axisTolerance;
        if (horizontal && line.length >= minFeature && line.length <= maxFeatureWidth)
        {
            horizontalLines.push_back(line);
        }
        if (vertical && line.length >= minFeature && line.length <= maxFeatureHeight)
        {
            verticalLines.push_back(line);
        }
    }

    struct NotchCandidate
    {
        LineSegmentCandidate horizontal;
        LineSegmentCandidate vertical;
        bool horizontalUseStart = true;
        bool verticalUseStart = true;
        double featureWidth = 0.0;
        double featureHeight = 0.0;
        double score = 0.0;
    };

    std::vector<NotchCandidate> notches;
    const double connectTolerance = std::max(0.25, axisTolerance * 2.0);
    const double edgeRejectMargin = std::max(offset * 0.75, std::min(width, height) * 0.08);
    auto testEndpoint = [&](const LineSegmentCandidate& horizontal, bool hStart, const LineSegmentCandidate& vertical) {
        const double hx = hStart ? horizontal.startX : horizontal.endX;
        const double hy = hStart ? horizontal.startY : horizontal.endY;
        bool vStart = true;
        if (!LineEndpointNear(vertical, hx, hy, connectTolerance, vStart))
        {
            return;
        }

        const double centerX = (std::min(horizontal.minX, vertical.minX) + std::max(horizontal.maxX, vertical.maxX)) * 0.5;
        const double centerY = (std::min(horizontal.minY, vertical.minY) + std::max(horizontal.maxY, vertical.maxY)) * 0.5;
        if (centerX <= bounds.minX + edgeRejectMargin || centerX >= bounds.maxX - edgeRejectMargin ||
            centerY <= bounds.minY + edgeRejectMargin || centerY >= bounds.maxY - edgeRejectMargin)
        {
            return;
        }

        NotchCandidate notch;
        notch.horizontal = horizontal;
        notch.vertical = vertical;
        notch.horizontalUseStart = hStart;
        notch.verticalUseStart = vStart;
        notch.featureWidth = horizontal.length;
        notch.featureHeight = vertical.length;
        notch.score = -(horizontal.length * vertical.length);
        notches.push_back(notch);
    };

    for (const LineSegmentCandidate& horizontal : horizontalLines)
    {
        for (const LineSegmentCandidate& vertical : verticalLines)
        {
            testEndpoint(horizontal, true, vertical);
            testEndpoint(horizontal, false, vertical);
        }
    }

    if (notches.empty())
    {
        WriteLine(session, "AutoCreateThreeViews: rectangular notch dimensions skipped; no connected short line pairs found.");
        return;
    }

    std::sort(notches.begin(), notches.end(), [](const NotchCandidate& a, const NotchCandidate& b) {
        return a.score < b.score;
    });

    const NotchCandidate& notch = notches.front();
    const double insideMargin = std::max(2.0, offset * 0.35);
    int created = 0;

    const bool hStartLeft = notch.horizontal.startX <= notch.horizontal.endX;
    const CurveAssocCandidate hLeft = MakeLineEndpointCandidate(notch.horizontal, hStartLeft);
    const CurveAssocCandidate hRight = MakeLineEndpointCandidate(notch.horizontal, !hStartLeft);
    const double widthOriginY = ClampDimensionOrigin(
        notch.horizontal.minY + offset * 0.45,
        bounds.minY + insideMargin,
        bounds.maxY - insideMargin);
    if (CreateCurvePointDimension(
            session,
            workPart,
            view,
            hLeft,
            hRight,
            true,
            NXOpen::Point3d((notch.horizontal.minX + notch.horizontal.maxX) * 0.5, widthOriginY, 0.0),
            "rectangular notch width"))
    {
        ++created;
    }

    const bool vStartBottom = notch.vertical.startY <= notch.vertical.endY;
    const CurveAssocCandidate vBottom = MakeLineEndpointCandidate(notch.vertical, vStartBottom);
    const CurveAssocCandidate vTop = MakeLineEndpointCandidate(notch.vertical, !vStartBottom);
    const double heightOriginX = ClampDimensionOrigin(
        notch.vertical.minX - offset * 0.45,
        bounds.minX + insideMargin,
        bounds.maxX - insideMargin);
    if (CreateCurvePointDimension(
            session,
            workPart,
            view,
            vBottom,
            vTop,
            false,
            NXOpen::Point3d(heightOriginX, (notch.vertical.minY + notch.vertical.maxY) * 0.5, 0.0),
            "rectangular notch height"))
    {
        ++created;
    }

    std::ostringstream log;
    log << "AutoCreateThreeViews: rectangular notch dimensions finished"
        << " candidates=" << notches.size()
        << ", created=" << created
        << ", drawingWidth=" << notch.featureWidth
        << ", drawingHeight=" << notch.featureHeight << ".";
    WriteLine(session, log.str());
}

void CreateLocalBendHeightDimensions(
    NXOpen::Session* session,
    NXOpen::Part* workPart,
    NXOpen::Drawings::DraftingView* view,
    const std::vector<DraftingCurveExtent>& extents,
    const LayoutBounds& bounds,
    double offset,
    const std::string& label)
{
    const double width = BoundsWidth(bounds);
    const double height = BoundsHeight(bounds);
    if (width <= 1.0 || height <= 1.0)
    {
        return;
    }

    std::vector<LineSegmentCandidate> lines = CollectLineSegments(view, extents);
    std::vector<LineSegmentCandidate> horizontalLines;
    const double axisTolerance = std::max(0.15, std::max(width, height) * 0.006);
    const double minLength = std::max(2.0, width * 0.08);
    for (const LineSegmentCandidate& line : lines)
    {
        if (std::abs(line.startY - line.endY) <= axisTolerance && line.length >= minLength)
        {
            horizontalLines.push_back(line);
        }
    }

    if (horizontalLines.size() < 2)
    {
        WriteLine(session, "AutoCreateThreeViews: local height skipped; " + label + " has less than 2 horizontal lines.");
        return;
    }

    struct HeightPair
    {
        LineSegmentCandidate lower;
        LineSegmentCandidate upper;
        double gap = 0.0;
        double overlapMinX = 0.0;
        double overlapMaxX = 0.0;
        double score = 0.0;
    };

    std::vector<HeightPair> pairs;
    for (size_t i = 0; i < horizontalLines.size(); ++i)
    {
        for (size_t j = i + 1; j < horizontalLines.size(); ++j)
        {
            const LineSegmentCandidate& a = horizontalLines[i];
            const LineSegmentCandidate& b = horizontalLines[j];
            const LineSegmentCandidate& lower = a.minY <= b.minY ? a : b;
            const LineSegmentCandidate& upper = a.minY <= b.minY ? b : a;
            const double gap = std::abs(upper.minY - lower.minY);
            if (gap <= axisTolerance || gap >= height * 0.75)
            {
                continue;
            }

            const double overlapMinX = std::max(lower.minX, upper.minX);
            const double overlapMaxX = std::min(lower.maxX, upper.maxX);
            const double overlap = overlapMaxX - overlapMinX;
            if (overlap < std::max(1.0, width * 0.02))
            {
                continue;
            }

            HeightPair pair;
            pair.lower = lower;
            pair.upper = upper;
            pair.gap = gap;
            pair.overlapMinX = overlapMinX;
            pair.overlapMaxX = overlapMaxX;
            pair.score = gap * 1000.0 - overlap;
            pairs.push_back(pair);
        }
    }

    if (pairs.empty())
    {
        WriteLine(session, "AutoCreateThreeViews: local height skipped; " + label + " has no short overlapping height pairs.");
        return;
    }

    std::sort(pairs.begin(), pairs.end(), [](const HeightPair& a, const HeightPair& b) {
        return a.score < b.score;
    });

    const int maxDimensions = 2;
    int created = 0;
    std::vector<double> usedGaps;
    for (const HeightPair& pair : pairs)
    {
        if (created >= maxDimensions)
        {
            break;
        }
        bool duplicateGap = false;
        for (double gap : usedGaps)
        {
            if (std::abs(gap - pair.gap) <= axisTolerance)
            {
                duplicateGap = true;
                break;
            }
        }
        if (duplicateGap)
        {
            continue;
        }

        const double pickX = (pair.overlapMinX + pair.overlapMaxX) * 0.5;
        const double originX = ClampDimensionOrigin(pair.overlapMinX - offset * 0.7, bounds.minX + offset * 0.25, bounds.maxX - offset * 0.25);
        const double originY = (pair.lower.minY + pair.upper.minY) * 0.5;
        if (CreateLocalHeightDimension(
                session,
                workPart,
                view,
                pair.lower,
                pair.upper,
                NXOpen::Point3d(originX, originY, 0.0),
                pickX,
                label))
        {
            usedGaps.push_back(pair.gap);
            ++created;
        }
    }

    std::ostringstream log;
    log << "AutoCreateThreeViews: local height finished"
        << " view=" << label
        << ", candidates=" << pairs.size()
        << ", created=" << created << ".";
    WriteLine(session, log.str());
}

bool HasHorizontalLineEndpointNear(
    const std::vector<LineSegmentCandidate>& lines,
    const LineSegmentCandidate& verticalLine,
    double x,
    double y,
    double axisTolerance,
    double minHorizontalLength)
{
    for (const LineSegmentCandidate& line : lines)
    {
        if (line.curve == verticalLine.curve)
        {
            continue;
        }
        if (std::abs(line.startY - line.endY) > axisTolerance)
        {
            continue;
        }
        if (line.length < minHorizontalLength)
        {
            continue;
        }

        bool useStart = true;
        if (LineEndpointNear(line, x, y, axisTolerance * 2.5, useStart))
        {
            return true;
        }
    }

    return false;
}

bool IsVerticalStepLineConnectedToHorizontalLines(
    const std::vector<LineSegmentCandidate>& lines,
    const LineSegmentCandidate& verticalLine,
    double axisTolerance,
    double minHorizontalLength)
{
    const double lowerX = verticalLine.startY <= verticalLine.endY ? verticalLine.startX : verticalLine.endX;
    const double lowerY = verticalLine.minY;
    const double upperX = verticalLine.startY <= verticalLine.endY ? verticalLine.endX : verticalLine.startX;
    const double upperY = verticalLine.maxY;

    return HasHorizontalLineEndpointNear(lines, verticalLine, lowerX, lowerY, axisTolerance, minHorizontalLength) &&
           HasHorizontalLineEndpointNear(lines, verticalLine, upperX, upperY, axisTolerance, minHorizontalLength);
}

void CreateLocalVerticalStepDimensions(
    NXOpen::Session* session,
    NXOpen::Part* workPart,
    NXOpen::Drawings::DraftingView* view,
    const std::vector<DraftingCurveExtent>& extents,
    const LayoutBounds& bounds,
    double offset,
    const std::string& label)
{
    const double width = BoundsWidth(bounds);
    const double height = BoundsHeight(bounds);
    if (width <= 1.0 || height <= 1.0)
    {
        return;
    }

    std::vector<LineSegmentCandidate> lines = CollectLineSegments(view, extents);
    std::vector<LineSegmentCandidate> verticalLines;
    const double axisTolerance = std::max(0.15, std::max(width, height) * 0.006);
    const double minLength = std::max(2.0, height * 0.08);
    const double maxLength = height * 0.75;
    const double minConnectedHorizontalLength = std::max(1.5, width * 0.01);
    for (const LineSegmentCandidate& line : lines)
    {
        if (std::abs(line.startX - line.endX) > axisTolerance)
        {
            continue;
        }
        if (line.length < minLength || line.length > maxLength)
        {
            continue;
        }

        const bool onLeftOuter = std::abs(line.minX - bounds.minX) <= axisTolerance * 2.0;
        const bool onRightOuter = std::abs(line.maxX - bounds.maxX) <= axisTolerance * 2.0;
        const bool spansMostHeight = line.minY <= bounds.minY + axisTolerance * 2.0 &&
                                     line.maxY >= bounds.maxY - axisTolerance * 2.0;
        if ((onLeftOuter || onRightOuter) && spansMostHeight)
        {
            continue;
        }

        if (!IsVerticalStepLineConnectedToHorizontalLines(lines, line, axisTolerance, minConnectedHorizontalLength))
        {
            continue;
        }

        verticalLines.push_back(line);
    }

    if (verticalLines.empty())
    {
        WriteLine(session, "AutoCreateThreeViews: vertical step skipped; " + label + " has no local vertical segments.");
        return;
    }

    std::sort(verticalLines.begin(), verticalLines.end(), [&](const LineSegmentCandidate& a, const LineSegmentCandidate& b) {
        const double aInternal = std::min(std::abs(a.minX - bounds.minX), std::abs(bounds.maxX - a.maxX));
        const double bInternal = std::min(std::abs(b.minX - bounds.minX), std::abs(bounds.maxX - b.maxX));
        if (std::abs(aInternal - bInternal) > axisTolerance)
        {
            return aInternal > bInternal;
        }
        return a.length > b.length;
    });

    const int maxDimensions = 1;
    int created = 0;
    for (const LineSegmentCandidate& line : verticalLines)
    {
        if (created >= maxDimensions)
        {
            break;
        }

        const bool startBottom = line.startY <= line.endY;
        CurveAssocCandidate bottomPoint = MakeLineEndpointCandidate(line, startBottom);
        CurveAssocCandidate topPoint = MakeLineEndpointCandidate(line, !startBottom);
        const double leftSpace = line.minX - bounds.minX;
        const double rightSpace = bounds.maxX - line.maxX;
        const double originX = leftSpace >= rightSpace
            ? ClampDimensionOrigin(line.minX - offset * 0.6, bounds.minX + offset * 0.25, bounds.maxX - offset * 0.25)
            : ClampDimensionOrigin(line.maxX + offset * 0.6, bounds.minX + offset * 0.25, bounds.maxX - offset * 0.25);
        const double originY = (line.minY + line.maxY) * 0.5;

        if (CreateCurvePointDimension(
                session,
                workPart,
                view,
                bottomPoint,
                topPoint,
                false,
                NXOpen::Point3d(originX, originY, 0.0),
                "vertical step"))
        {
            ++created;
        }
    }

    std::ostringstream log;
    log << "AutoCreateThreeViews: vertical step finished"
        << " view=" << label
        << ", candidates=" << verticalLines.size()
        << ", created=" << created << ".";
    WriteLine(session, log.str());
}

bool ShouldCreateHorizontalOverallForView(const std::string& label)
{
    return label == "front" ||
           label == "left" ||
           label == "right" ||
           label == "back";
}

bool ShouldCreateVerticalOverallForView(const std::string& label)
{
    return label == "front" ||
           label == "top" ||
           label == "bottom" ||
           label == "back bottom";
}

void CreateProjectedOverallDimensions(
    NXOpen::Session* session,
    NXOpen::Part* workPart,
    const RequestValues& request,
    const std::vector<CreatedView>& createdProjectedViews,
    const AutoViewDirection& frontDirection)
{
    if (!request.autoDimensions)
    {
        WriteLine(session, "AutoCreateThreeViews: auto dimension skipped by request.");
        return;
    }
    if (!request.dimensionOverall &&
        !request.dimensionAngle &&
        !request.dimensionHole &&
        !request.dimensionHoleLocation &&
        !request.dimensionInnerClosedCurve)
    {
        WriteLine(session, "AutoCreateThreeViews: auto dimension skipped; all dimension switches are off.");
        return;
    }

    std::vector<FacePairKey> usedFacePairs;
    std::vector<FacePairKey> usedAngleFacePairs;
    std::vector<double> annotatedTappedHoleDiameters;
    ShallowDetailFilterCache shallowCache;
    bool shallowCacheReady = false;
    auto ensureShallowCache = [&]() {
        if (!shallowCacheReady)
        {
            EnsureShallowDetailFilterCache(workPart, shallowCache);
            shallowCacheReady = true;
        }
    };
    for (const CreatedView& created : createdProjectedViews)
    {
        if (created.view == nullptr)
        {
            continue;
        }

        const bool createHorizontal = request.dimensionOverall && ShouldCreateHorizontalOverallForView(created.label);
        const bool createVertical = request.dimensionOverall && ShouldCreateVerticalOverallForView(created.label);

        LayoutBounds bounds;
        if (!AskViewLayoutBounds(created.view, false, bounds))
        {
            WriteLine(session, "AutoCreateThreeViews: auto dimension skipped; " + created.label + " view bounds not available.");
            continue;
        }

        // Feature 08 places overall dimensions at 8 * 2 / 3 sheet units.
        const double offset = request.targetLayer > 0
            ? 8.0 * 2.0 / 3.0
            : std::max(8.0, request.viewSpacing * 0.6);
        const std::vector<DraftingCurveExtent> extents = CollectDraftingCurveExtents(created.view);
        LayoutBounds overallBounds = bounds;
        bool hasVisibleCurveBounds = false;
        bool hasOuterContourBounds = false;
        std::vector<DraftingCurveExtent> overallCurveExtents;
        if (request.dimensionOverall)
        {
            ensureShallowCache();
            LayoutBounds visibleCurveBounds;
            if (TryBuildVisibleCurveBounds(extents, visibleCurveBounds))
            {
                overallBounds = visibleCurveBounds;
                hasVisibleCurveBounds = true;
                std::ostringstream visibleLog;
                visibleLog << "AutoCreateThreeViews: " << created.label
                    << " overall all visible curve bounds"
                    << " minX=" << overallBounds.minX
                    << ", minY=" << overallBounds.minY
                    << ", maxX=" << overallBounds.maxX
                    << ", maxY=" << overallBounds.maxY
                    << ", width=" << BoundsWidth(overallBounds)
                    << ", height=" << BoundsHeight(overallBounds)
                    << ", curves=" << extents.size() << ".";
                WriteLine(session, visibleLog.str());
            }

            LayoutBounds outerBounds;
            if (TryFindOverallVisibleCurveBounds(
                    session,
                    workPart,
                    shallowCache,
                    extents,
                    created.label + " overall",
                    outerBounds,
                    &overallCurveExtents))
            {
                hasOuterContourBounds = true;
                const double visibleExtremaTolerance =
                    std::max(0.10, std::max(BoundsWidth(overallBounds), BoundsHeight(overallBounds)) * 0.002);
                if (hasVisibleCurveBounds &&
                    BoundsMissVisibleExtrema(outerBounds, overallBounds, visibleExtremaTolerance))
                {
                    std::ostringstream fallbackLog;
                    fallbackLog << "AutoCreateThreeViews: " << created.label
                        << " overall filtered outer curve bounds miss visible extrema; use all visible curve bounds"
                        << ", tolerance=" << visibleExtremaTolerance
                        << ", visibleMinX=" << overallBounds.minX
                        << ", visibleMinY=" << overallBounds.minY
                        << ", visibleMaxX=" << overallBounds.maxX
                        << ", visibleMaxY=" << overallBounds.maxY
                        << ", outerMinX=" << outerBounds.minX
                        << ", outerMinY=" << outerBounds.minY
                        << ", outerMaxX=" << outerBounds.maxX
                        << ", outerMaxY=" << outerBounds.maxY << ".";
                    WriteLine(session, fallbackLog.str());
                    hasOuterContourBounds = false;
                    overallCurveExtents.clear();
                }
            }
        }
        const std::vector<DraftingCurveExtent>& overallDimensionExtents =
            extents;
        const double axisTolerance = std::max(0.15, std::max(BoundsWidth(overallBounds), BoundsHeight(overallBounds)) * 0.004);
        std::vector<LineProjectionFaceCandidate> overallFaces;
        std::vector<ClosedCurveDimensionRecord> closedCurveDimensionRecords;
        FacePairKey horizontalOverallPairKey;
        FacePairKey verticalOverallPairKey;
        std::vector<LineProjectionFaceCandidate> horizontalOverallPairFaces;
        std::vector<LineProjectionFaceCandidate> verticalOverallPairFaces;
        bool hasHorizontalOverallPair = false;
        bool hasVerticalOverallPair = false;
        if (request.dimensionInnerClosedCurve)
        {
            ensureShallowCache();
            const std::vector<LineSegmentCandidate> lines = CollectLineSegments(created.view, extents);
            overallFaces =
                CollectLineProjectionFaceCandidates(session, workPart, created.view, created.label + " overall", lines, axisTolerance, &shallowCache);
            overallFaces = CollapseSamePlaneFaceCandidates(overallFaces);
            if (createHorizontal)
            {
                hasHorizontalOverallPair =
                    TryFindOverallFacePair(
                        overallFaces,
                        overallBounds,
                        true,
                        axisTolerance,
                        horizontalOverallPairKey,
                        &horizontalOverallPairFaces,
                        session,
                        created.label);
            }
            if (createVertical)
            {
                hasVerticalOverallPair =
                    TryFindOverallFacePair(
                        overallFaces,
                        overallBounds,
                        false,
                        axisTolerance,
                        verticalOverallPairKey,
                        &verticalOverallPairFaces,
                        session,
                        created.label);
            }
            MarkOuterContourDatumFaces(overallFaces, overallBounds, axisTolerance);
        }
        std::ostringstream log;
        log << "AutoCreateThreeViews: auto dimension " << created.label << " overall bounds"
            << " minX=" << bounds.minX
            << ", minY=" << bounds.minY
            << ", maxX=" << bounds.maxX
            << ", maxY=" << bounds.maxY
            << ", width=" << BoundsWidth(bounds)
            << ", height=" << BoundsHeight(bounds)
            << ", overallMinX=" << overallBounds.minX
            << ", overallMinY=" << overallBounds.minY
            << ", overallMaxX=" << overallBounds.maxX
            << ", overallMaxY=" << overallBounds.maxY
            << ", overallWidth=" << BoundsWidth(overallBounds)
            << ", overallHeight=" << BoundsHeight(overallBounds)
            << ", outerCurveBounds=" << (hasOuterContourBounds ? "yes" : "no")
            << ", curves=" << extents.size()
            << ", overallCurves=" << overallDimensionExtents.size()
            << ", horizontal=" << (createHorizontal ? "yes" : "no")
            << ", vertical=" << (createVertical ? "yes" : "no")
            << ", bodyBoxHorizontal=" << (hasHorizontalOverallPair ? "yes" : "no")
            << ", bodyBoxVertical=" << (hasVerticalOverallPair ? "yes" : "no")
            << ", offset=" << offset << ".";
        WriteLine(session, log.str());

        if (createHorizontal)
        {
            bool dimensionCreated = CreateHorizontalOverallDimension(
                session,
                workPart,
                created.view,
                overallBounds,
                extents,
                offset);
            if (dimensionCreated)
            {
                if (hasHorizontalOverallPair)
                {
                    usedFacePairs.push_back(horizontalOverallPairKey);
                }
            }
        }
        if (createVertical)
        {
            bool dimensionCreated = CreateVerticalOverallDimension(
                session,
                workPart,
                created.view,
                overallBounds,
                extents,
                offset);
            if (dimensionCreated)
            {
                if (hasVerticalOverallPair)
                {
                    usedFacePairs.push_back(verticalOverallPairKey);
                }
            }
        }
        if (request.dimensionHole)
        {
            ensureShallowCache();
            CreateFrontHoleDiameterDimensions(session, workPart, shallowCache, created.view, extents, bounds, offset, annotatedTappedHoleDiameters);
        }
        if (request.dimensionHoleLocation)
        {
            ensureShallowCache();
            CreateFrontHoleLocationDimensions(session, workPart, shallowCache, created.view, extents, bounds, offset);
        }
        if (request.dimensionInnerClosedCurve)
        {
            ensureShallowCache();
            CreateClosedCurveMaxDimensions(
                session,
                workPart,
                shallowCache,
                created.view,
                created.label,
                extents,
                overallFaces,
                bounds,
                offset,
                usedFacePairs,
                closedCurveDimensionRecords);
        }
        if (request.dimensionAngle)
        {
            ensureShallowCache();
            CreateViewAngleDimensions(
                session,
                workPart,
                shallowCache,
                created.view,
                created.label,
                extents,
                bounds,
                usedAngleFacePairs);
        }
    }
}

void CreateFlatPatternOverallDimensions(
    NXOpen::Session* session,
    NXOpen::Part* workPart,
    const RequestValues& request,
    const std::vector<CreatedAuxiliaryView>& auxiliaryViews)
{
    if (!request.autoDimensions || !request.dimensionOverall)
    {
        return;
    }

    for (const CreatedAuxiliaryView& created : auxiliaryViews)
    {
        if (created.label != "flat pattern" || created.view == nullptr)
        {
            continue;
        }

        LayoutBounds bounds;
        if (!AskViewLayoutBounds(created.view, false, bounds))
        {
            WriteLine(session, "AutoCreateThreeViews: flat pattern overall dimension skipped; view bounds not available.");
            continue;
        }

        const double offset = request.targetLayer > 0
            ? 8.0 * 2.0 / 3.0
            : std::max(8.0, request.viewSpacing * 0.6);
        const std::vector<DraftingCurveExtent> extents = CollectDraftingCurveExtents(created.view);
        std::ostringstream log;
        log << "AutoCreateThreeViews: auto dimension flat pattern overall bounds"
            << " minX=" << bounds.minX
            << ", minY=" << bounds.minY
            << ", maxX=" << bounds.maxX
            << ", maxY=" << bounds.maxY
            << ", width=" << BoundsWidth(bounds)
            << ", height=" << BoundsHeight(bounds)
            << ", curves=" << extents.size()
            << ", horizontal=yes"
            << ", vertical=yes"
            << ", offset=" << offset << ".";
        WriteLine(session, log.str());

        CreateHorizontalOverallDimension(session, workPart, created.view, bounds, extents, offset);
        CreateVerticalOverallDimension(session, workPart, created.view, bounds, extents, offset);
    }
}

void ClearDraftingViewHighlight(NXOpen::Drawings::DraftingView* view, int& clearedObjects)
{
    if (view == nullptr)
    {
        return;
    }

    if (view->Tag() != NULL_TAG && UF_DISP_set_highlight(view->Tag(), 0) == 0)
    {
        ++clearedObjects;
    }

    if (view->DraftingBodies() == nullptr)
    {
        return;
    }

    for (NXOpen::Drawings::DraftingBody* draftingBody : *view->DraftingBodies())
    {
        if (draftingBody == nullptr)
        {
            continue;
        }
        if (draftingBody->Tag() != NULL_TAG && UF_DISP_set_highlight(draftingBody->Tag(), 0) == 0)
        {
            ++clearedObjects;
        }
        if (draftingBody->DraftingCurves() == nullptr)
        {
            continue;
        }
        for (NXOpen::Drawings::DraftingCurve* draftingCurve : *draftingBody->DraftingCurves())
        {
            if (draftingCurve != nullptr && draftingCurve->Tag() != NULL_TAG &&
                UF_DISP_set_highlight(draftingCurve->Tag(), 0) == 0)
            {
                ++clearedObjects;
            }
        }
    }
}

void ClearPartModelHighlights(NXOpen::Part* part, int& clearedObjects)
{
    if (part == nullptr || part->Bodies() == nullptr)
    {
        return;
    }

    for (NXOpen::BodyCollection::iterator it = part->Bodies()->begin(); it != part->Bodies()->end(); ++it)
    {
        NXOpen::Body* body = *it;
        if (body == nullptr || body->Tag() == NULL_TAG)
        {
            continue;
        }

        if (UF_DISP_set_highlight(body->Tag(), 0) == 0)
        {
            ++clearedObjects;
        }

        uf_list_p_t faceList = nullptr;
        if (UF_MODL_ask_body_faces(body->Tag(), &faceList) == 0 && faceList != nullptr)
        {
            int faceCount = 0;
            UF_MODL_ask_list_count(faceList, &faceCount);
            for (int faceIndex = 0; faceIndex < faceCount; ++faceIndex)
            {
                tag_t faceTag = NULL_TAG;
                if (UF_MODL_ask_list_item(faceList, faceIndex, &faceTag) == 0 && faceTag != NULL_TAG &&
                    UF_DISP_set_highlight(faceTag, 0) == 0)
                {
                    ++clearedObjects;
                }
            }
            UF_MODL_delete_list(&faceList);
        }

        uf_list_p_t edgeList = nullptr;
        if (UF_MODL_ask_body_edges(body->Tag(), &edgeList) == 0 && edgeList != nullptr)
        {
            int edgeCount = 0;
            UF_MODL_ask_list_count(edgeList, &edgeCount);
            for (int edgeIndex = 0; edgeIndex < edgeCount; ++edgeIndex)
            {
                tag_t edgeTag = NULL_TAG;
                if (UF_MODL_ask_list_item(edgeList, edgeIndex, &edgeTag) == 0 && edgeTag != NULL_TAG &&
                    UF_DISP_set_highlight(edgeTag, 0) == 0)
                {
                    ++clearedObjects;
                }
            }
            UF_MODL_delete_list(&edgeList);
        }
    }
}

void ClearCreatedDrawingSelectionAndHighlights(
    NXOpen::Session* session,
    NXOpen::Part* workPart,
    const std::vector<CreatedView>& createdProjectedViews,
    const std::vector<CreatedAuxiliaryView>& auxiliaryViews)
{
    try
    {
        NXOpen::UI* ui = NXOpen::UI::GetUI();
        if (ui != nullptr && ui->SelectionManager() != nullptr)
        {
            ui->SelectionManager()->ClearGlobalSelectionList();
        }
    }
    catch (...)
    {
    }

    int clearedObjects = 0;
    for (const CreatedView& created : createdProjectedViews)
    {
        ClearDraftingViewHighlight(created.view, clearedObjects);
    }
    for (const CreatedAuxiliaryView& auxiliary : auxiliaryViews)
    {
        ClearDraftingViewHighlight(auxiliary.view, clearedObjects);
    }
    ClearPartModelHighlights(workPart, clearedObjects);

    UF_DISP_make_display_up_to_date();
    if (session != nullptr && session->DisplayManager() != nullptr)
    {
        session->DisplayManager()->MakeUpToDate();
        std::ostringstream log;
        log << "AutoCreateThreeViews: cleared generated drawing selection/highlight objects="
            << clearedObjects << ".";
        WriteLine(session, log.str());
    }
}

void CreateFlatPatternNoteBelowView(
    NXOpen::Session* session,
    NXOpen::Part* workPart,
    const RequestValues& request,
    const std::vector<CreatedAuxiliaryView>& auxiliaryViews)
{
    for (const CreatedAuxiliaryView& created : auxiliaryViews)
    {
        if (created.label != "flat pattern" || created.view == nullptr)
        {
            continue;
        }

        LayoutBounds bounds;
        if (!AskViewLayoutBounds(created.view, false, bounds))
        {
            WriteLine(session, "AutoCreateThreeViews: flat pattern note skipped; view bounds not available.");
            continue;
        }

        const std::string noteText = BuildFlatPatternNoteText(workPart);
        if (noteText.empty())
        {
            WriteLine(session, "AutoCreateThreeViews: flat pattern note skipped; note text is empty.");
            continue;
        }

        NXOpen::Annotations::DraftingNoteBuilder* builder = nullptr;
        try
        {
            NXOpen::Annotations::SimpleDraftingAid* nullAid = nullptr;
            builder = workPart->Annotations()->CreateDraftingNoteBuilder(nullAid);
            builder->Origin()->Plane()->SetPlaneMethod(NXOpen::Annotations::PlaneBuilder::PlaneMethodTypeXyPlane);
            builder->Origin()->SetAnchor(NXOpen::Annotations::OriginBuilder::AlignmentPositionMidCenter);
            builder->Origin()->SetInferRelativeToGeometry(false);
            builder->Style()->LetteringStyle()->SetGeneralTextLineSpaceFactor(1.5);
            const int noteFont = LoadChineseDraftNoteFont(session, workPart);
            if (noteFont > 0)
            {
                builder->Style()->LetteringStyle()->SetGeneralTextFont(noteFont);
            }
            const std::string displayNoteText = "\xE5\xB1\x95\xE5\xBC\x80\xE5\x9B\xBE\n" + noteText;
            builder->Text()->TextBlock()->SetText(BuildDraftNoteLines(displayNoteText));

            NXOpen::Annotations::Annotation::AssociativeOriginData assocOrigin;
            assocOrigin.OriginType = NXOpen::Annotations::AssociativeOriginTypeRelativeToView;
            assocOrigin.View = created.view;
            assocOrigin.StackAlignmentPosition = NXOpen::Annotations::StackAlignmentPositionAbove;
            builder->Origin()->SetAssociativeOrigin(assocOrigin);

            NXOpen::View* nullView = nullptr;
            const double offset = std::max(2.0, request.viewSpacing * 0.15);
            const NXOpen::Point3d notePoint(
                (bounds.minX + bounds.maxX) * 0.5,
                bounds.minY - offset,
                0.0);
            builder->Origin()->Origin()->SetValue(nullptr, nullView, notePoint);
            builder->Commit();
            builder->Destroy();
            builder = nullptr;

            std::ostringstream log;
            log << "AutoCreateThreeViews: flat pattern note created"
                << " x=" << notePoint.X
                << ", y=" << notePoint.Y
                << ", text=" << displayNoteText << ".";
            WriteLine(session, log.str());
        }
        catch (const NXOpen::NXException& ex)
        {
            if (builder != nullptr)
            {
                try { builder->Destroy(); } catch (...) {}
            }
            std::ostringstream log;
            log << "AutoCreateThreeViews: flat pattern note failed, NX "
                << ex.ErrorCode() << ": " << ex.Message();
            WriteLine(session, log.str());
        }
        catch (...)
        {
            if (builder != nullptr)
            {
                try { builder->Destroy(); } catch (...) {}
            }
            WriteLine(session, "AutoCreateThreeViews: flat pattern note failed, unknown exception.");
        }
    }
}

void CreateLayerGroupNote(
    NXOpen::Session* session,
    NXOpen::Part* workPart,
    const RequestValues& request,
    const std::vector<CreatedView>& projectedViews,
    const std::vector<CreatedAuxiliaryView>& auxiliaryViews)
{
    if (request.targetLayer <= 0 || workPart == nullptr || workPart->Annotations() == nullptr)
    {
        return;
    }
    const LayoutBounds bounds = BoundsForAllViews(projectedViews, auxiliaryViews);
    if (bounds.maxX <= bounds.minX || bounds.maxY <= bounds.minY)
    {
        return;
    }

    NXOpen::Annotations::DraftingNoteBuilder* builder = nullptr;
    try
    {
        NXOpen::Annotations::SimpleDraftingAid* nullAid = nullptr;
        builder = workPart->Annotations()->CreateDraftingNoteBuilder(nullAid);
        builder->Origin()->Plane()->SetPlaneMethod(NXOpen::Annotations::PlaneBuilder::PlaneMethodTypeXyPlane);
        builder->Origin()->SetAnchor(NXOpen::Annotations::OriginBuilder::AlignmentPositionMidCenter);
        builder->Origin()->SetInferRelativeToGeometry(false);
        const int noteFont = LoadChineseDraftNoteFont(session, workPart);
        if (noteFont > 0)
        {
            builder->Style()->LetteringStyle()->SetGeneralTextFont(noteFont);
        }
        const std::string noteText = std::string(u8"图层 ") + std::to_string(request.targetLayer);
        builder->Text()->TextBlock()->SetText(BuildDraftNoteLines(noteText));
        NXOpen::View* nullView = nullptr;
        const NXOpen::Point3d notePoint(
            (bounds.minX + bounds.maxX) * 0.5,
            bounds.minY - std::max(5.0, request.viewSpacing * 0.6),
            0.0);
        builder->Origin()->Origin()->SetValue(nullptr, nullView, notePoint);
        builder->Commit();
        builder->Destroy();
        builder = nullptr;
        WriteLine(session, "AutoCreateThreeViews: created layer group note for layer " + std::to_string(request.targetLayer) + ".");
    }
    catch (const NXOpen::NXException& ex)
    {
        if (builder != nullptr)
        {
            try { builder->Destroy(); } catch (...) {}
        }
        WriteLine(session, std::string("AutoCreateThreeViews: layer group note failed, NX ") + std::to_string(ex.ErrorCode()) + ": " + ex.Message());
    }
}

NXOpen::Annotations::OriginBuilder::AlignmentPosition TechnicalRequirementAnchor(const std::string& corner)
{
    if (corner == "TopRight")
    {
        return NXOpen::Annotations::OriginBuilder::AlignmentPositionTopRight;
    }
    if (corner == "BottomLeft")
    {
        return NXOpen::Annotations::OriginBuilder::AlignmentPositionBottomLeft;
    }
    if (corner == "BottomRight")
    {
        return NXOpen::Annotations::OriginBuilder::AlignmentPositionBottomRight;
    }
    return NXOpen::Annotations::OriginBuilder::AlignmentPositionTopLeft;
}

NXOpen::Point3d TechnicalRequirementPoint(
    const RequestValues& request,
    double sheetLength,
    double sheetHeight)
{
    const double inset = std::max(5.0, request.sheetMargin * 0.5);
    const bool rightSide =
        request.technicalRequirementCorner == "TopRight" ||
        request.technicalRequirementCorner == "BottomRight";
    const bool topSide =
        request.technicalRequirementCorner == "TopLeft" ||
        request.technicalRequirementCorner == "TopRight";

    return NXOpen::Point3d(
        rightSide ? std::max(inset, sheetLength - inset) : inset,
        topSide ? std::max(inset, sheetHeight - inset) : inset,
        0.0);
}

void CreateTechnicalRequirementNote(
    NXOpen::Session* session,
    NXOpen::Part* workPart,
    const RequestValues& request,
    double sheetLength,
    double sheetHeight)
{
    if (!request.technicalRequirementEnabled)
    {
        WriteLine(session, "AutoCreateThreeViews: technical requirement note skipped; disabled.");
        return;
    }

    const std::string noteText = Trim(request.technicalRequirementText);
    if (noteText.empty())
    {
        WriteLine(session, "AutoCreateThreeViews: technical requirement note skipped; text is empty.");
        return;
    }

    NXOpen::Annotations::DraftingNoteBuilder* builder = nullptr;
    try
    {
        NXOpen::Annotations::SimpleDraftingAid* nullAid = nullptr;
        builder = workPart->Annotations()->CreateDraftingNoteBuilder(nullAid);
        builder->Origin()->Plane()->SetPlaneMethod(NXOpen::Annotations::PlaneBuilder::PlaneMethodTypeXyPlane);
        builder->Origin()->SetAnchor(TechnicalRequirementAnchor(request.technicalRequirementCorner));
        builder->Origin()->SetInferRelativeToGeometry(false);
        builder->Style()->LetteringStyle()->SetGeneralTextLineSpaceFactor(1.35);
        const int noteFont = LoadChineseDraftNoteFont(session, workPart);
        if (noteFont > 0)
        {
            builder->Style()->LetteringStyle()->SetGeneralTextFont(noteFont);
        }
        builder->Text()->TextBlock()->SetText(BuildDraftNoteLines(noteText, false));

        NXOpen::View* nullView = nullptr;
        const NXOpen::Point3d notePoint = TechnicalRequirementPoint(request, sheetLength, sheetHeight);
        builder->Origin()->Origin()->SetValue(nullptr, nullView, notePoint);
        builder->Commit();
        builder->Destroy();
        builder = nullptr;

        std::ostringstream log;
        log << "AutoCreateThreeViews: technical requirement note created"
            << " corner=" << request.technicalRequirementCorner
            << ", x=" << notePoint.X
            << ", y=" << notePoint.Y
            << ".";
        WriteLine(session, log.str());
    }
    catch (const NXOpen::NXException& ex)
    {
        if (builder != nullptr)
        {
            try { builder->Destroy(); } catch (...) {}
        }
        std::ostringstream log;
        log << "AutoCreateThreeViews: technical requirement note failed, NX "
            << ex.ErrorCode() << ": " << ex.Message();
        WriteLine(session, log.str());
    }
    catch (...)
    {
        if (builder != nullptr)
        {
            try { builder->Destroy(); } catch (...) {}
        }
        WriteLine(session, "AutoCreateThreeViews: technical requirement note failed, unknown exception.");
    }
}

void ArrangeAllViewsInMemory(
    const RequestValues& request,
    const std::vector<CreatedView>& projectedViews,
    const std::vector<CreatedAuxiliaryView>& auxiliaryViews,
    double sheetLength,
    double sheetHeight)
{
    std::vector<ViewTargetBounds> placements;
    LayoutBounds frontBounds;
    bool hasFront = false;
    NXOpen::Point3d frontPoint(0.0, 0.0, 0.0);

    for (const CreatedView& created : projectedViews)
    {
        if (created.label != "front")
        {
            continue;
        }

        hasFront = AskViewLayoutBounds(created.view, false, frontBounds);
        if (hasFront)
        {
            frontPoint = created.plannedPoint;
            placements.push_back({created.view, frontBounds, frontBounds});
        }
        break;
    }

    if (!hasFront)
    {
        return;
    }

    const double frontCenterX = (frontBounds.minX + frontBounds.maxX) * 0.5;
    const double frontCenterY = (frontBounds.minY + frontBounds.maxY) * 0.5;
    double rightStackX = frontBounds.maxX;
    double leftStackX = frontBounds.minX;
    double topStackY = frontBounds.maxY;
    double bottomStackY = frontBounds.minY;
    bool hasProjectedTarget = true;
    LayoutBounds projectedTargetBounds = frontBounds;

    for (const CreatedView& created : projectedViews)
    {
        if (created.label == "front" || created.view == nullptr)
        {
            continue;
        }

        LayoutBounds current;
        if (!AskViewLayoutBounds(created.view, false, current))
        {
            continue;
        }

        const double width = BoundsWidth(current);
        const double height = BoundsHeight(current);
        const double plannedDx = created.plannedPoint.X - frontPoint.X;
        const double plannedDy = created.plannedPoint.Y - frontPoint.Y;

        LayoutBounds target;
        if (std::abs(plannedDx) >= std::abs(plannedDy))
        {
            if (plannedDx > 0.0)
            {
                target = BoundsWithSize(rightStackX + request.viewSpacing, frontCenterY - height * 0.5, width, height);
                rightStackX = target.maxX;
            }
            else
            {
                target = BoundsWithSize(leftStackX - request.viewSpacing - width, frontCenterY - height * 0.5, width, height);
                leftStackX = target.minX;
            }
        }
        else
        {
            if (plannedDy > 0.0)
            {
                target = BoundsWithSize(frontCenterX - width * 0.5, topStackY + request.viewSpacing, width, height);
                topStackY = target.maxY;
            }
            else
            {
                target = BoundsWithSize(frontCenterX - width * 0.5, bottomStackY - request.viewSpacing - height, width, height);
                bottomStackY = target.minY;
            }
        }

        placements.push_back({created.view, current, target});
        projectedTargetBounds = hasProjectedTarget ? MergeBounds(projectedTargetBounds, target) : target;
        hasProjectedTarget = true;
    }

    std::vector<LayoutBounds> projectedReservedZones;
    const double reservedGap = std::max(2.0, request.viewSpacing * 0.5);
    for (size_t i = 0; i < placements.size(); ++i)
    {
        const LayoutBounds viewZone = InflateBounds(placements[i].target, reservedGap);
        projectedReservedZones.push_back(viewZone);
        if (i == 0)
        {
            continue;
        }

        const LayoutBounds& target = placements[i].target;
        LayoutBounds corridor{};
        bool hasCorridor = false;
        if (target.minX >= frontBounds.maxX)
        {
            corridor.minX = frontBounds.maxX;
            corridor.maxX = target.minX;
            corridor.minY = std::min(frontBounds.minY, target.minY);
            corridor.maxY = std::max(frontBounds.maxY, target.maxY);
            hasCorridor = true;
        }
        else if (target.maxX <= frontBounds.minX)
        {
            corridor.minX = target.maxX;
            corridor.maxX = frontBounds.minX;
            corridor.minY = std::min(frontBounds.minY, target.minY);
            corridor.maxY = std::max(frontBounds.maxY, target.maxY);
            hasCorridor = true;
        }
        else if (target.minY >= frontBounds.maxY)
        {
            corridor.minX = std::min(frontBounds.minX, target.minX);
            corridor.maxX = std::max(frontBounds.maxX, target.maxX);
            corridor.minY = frontBounds.maxY;
            corridor.maxY = target.minY;
            hasCorridor = true;
        }
        else if (target.maxY <= frontBounds.minY)
        {
            corridor.minX = std::min(frontBounds.minX, target.minX);
            corridor.maxX = std::max(frontBounds.maxX, target.maxX);
            corridor.minY = target.maxY;
            corridor.maxY = frontBounds.minY;
            hasCorridor = true;
        }

        if (hasCorridor && BoundsWidth(corridor) > 0.0 && BoundsHeight(corridor) > 0.0)
        {
            projectedReservedZones.push_back(InflateBounds(corridor, reservedGap));
        }
    }

    struct AuxEntry
    {
        const CreatedAuxiliaryView* created = nullptr;
        LayoutBounds current;
        double width = 0.0;
        double height = 0.0;
    };

    std::map<std::string, std::vector<AuxEntry>> sideGroups;
    for (const CreatedAuxiliaryView& created : auxiliaryViews)
    {
        LayoutBounds current;
        if (!AskViewLayoutBounds(created.view, true, current))
        {
            continue;
        }

        const std::string corner = EffectiveAuxiliaryCorner(request, created);
        const bool leftSide = corner == "TopLeft" || corner == "BottomLeft";
        sideGroups[leftSide ? "left" : "right"].push_back({&created, current, BoundsWidth(current), BoundsHeight(current)});
    }

    if (request.auxiliaryAutoCompact)
    {
        std::vector<AuxEntry> compactEntries;
        for (auto& group : sideGroups)
        {
            compactEntries.insert(compactEntries.end(), group.second.begin(), group.second.end());
        }
        std::stable_sort(compactEntries.begin(), compactEntries.end(), [](const AuxEntry& first, const AuxEntry& second) {
            const bool firstIso = first.created != nullptr && first.created->label == "isometric";
            const bool secondIso = second.created != nullptr && second.created->label == "isometric";
            if (firstIso != secondIso)
            {
                return firstIso;
            }
            return first.width * first.height > second.width * second.height;
        });

        const double usableWidth = ScaleUsableWidth(request, sheetLength);
        const double usableHeight = ScaleUsableHeight(request, sheetHeight);
        const double preferredFlatSpread = std::max(request.viewSpacing * 3.0, 60.0);
        const double maxFlatSpread = std::min(std::max(request.viewSpacing * 6.0, 120.0), std::max(usableWidth, usableHeight) * 0.45);
        const double flatSpreadWeight = std::max(1.0, std::max(usableWidth, usableHeight) * 0.05);
        auto buildCandidates = [&](const AuxEntry& entry) {
            std::vector<LayoutBounds> candidates;
            const bool isFlatPattern = entry.created != nullptr && entry.created->label == "flat pattern";
            const double centerX = (projectedTargetBounds.minX + projectedTargetBounds.maxX) * 0.5;
            const double centerY = (projectedTargetBounds.minY + projectedTargetBounds.maxY) * 0.5;
            if (isFlatPattern)
            {
                candidates.push_back(BoundsWithSize(projectedTargetBounds.maxX + request.viewSpacing, projectedTargetBounds.maxY - entry.height, entry.width, entry.height));
                candidates.push_back(BoundsWithSize(projectedTargetBounds.maxX - entry.width, projectedTargetBounds.maxY + request.viewSpacing, entry.width, entry.height));
                candidates.push_back(BoundsWithSize(projectedTargetBounds.minX, projectedTargetBounds.minY - request.viewSpacing - entry.height, entry.width, entry.height));
                candidates.push_back(BoundsWithSize(projectedTargetBounds.minX, projectedTargetBounds.minY + request.viewSpacing, entry.width, entry.height));
                candidates.push_back(BoundsWithSize(projectedTargetBounds.maxX - entry.width, projectedTargetBounds.maxY + request.viewSpacing, entry.width, entry.height));
                candidates.push_back(BoundsWithSize((projectedTargetBounds.minX + projectedTargetBounds.maxX) * 0.5 - entry.width * 0.5,
                                                    projectedTargetBounds.minY - request.viewSpacing - entry.height,
                                                    entry.width,
                                                    entry.height));
                candidates.push_back(BoundsWithSize(projectedTargetBounds.maxX - entry.width, projectedTargetBounds.minY - request.viewSpacing - entry.height, entry.width, entry.height));
                candidates.push_back(BoundsWithSize(projectedTargetBounds.maxX + request.viewSpacing, centerY - entry.height * 0.5, entry.width, entry.height));
                candidates.push_back(BoundsWithSize(projectedTargetBounds.minX - request.viewSpacing - entry.width, centerY - entry.height * 0.5, entry.width, entry.height));
                candidates.push_back(BoundsWithSize(centerX - entry.width * 0.5, projectedTargetBounds.maxY + request.viewSpacing, entry.width, entry.height));
                candidates.push_back(BoundsWithSize((projectedTargetBounds.minX + projectedTargetBounds.maxX) * 0.5 - entry.width * 0.5,
                                                    projectedTargetBounds.maxY + request.viewSpacing,
                                                    entry.width,
                                                    entry.height));
            }
            else
            {
                candidates.push_back(BoundsWithSize(projectedTargetBounds.minX, projectedTargetBounds.minY - request.viewSpacing - entry.height, entry.width, entry.height));
                candidates.push_back(BoundsWithSize(projectedTargetBounds.maxX + request.viewSpacing, projectedTargetBounds.maxY - entry.height, entry.width, entry.height));
                candidates.push_back(BoundsWithSize(projectedTargetBounds.minX - request.viewSpacing - entry.width, centerY - entry.height * 0.5, entry.width, entry.height));
                candidates.push_back(BoundsWithSize(projectedTargetBounds.maxX + request.viewSpacing, centerY - entry.height * 0.5, entry.width, entry.height));
                candidates.push_back(BoundsWithSize(centerX - entry.width * 0.5, projectedTargetBounds.maxY + request.viewSpacing, entry.width, entry.height));
                candidates.push_back(BoundsWithSize(centerX - entry.width * 0.5, projectedTargetBounds.minY - request.viewSpacing - entry.height, entry.width, entry.height));
            }
            candidates.push_back(BoundsWithSize(projectedTargetBounds.maxX + request.viewSpacing, projectedTargetBounds.maxY - entry.height, entry.width, entry.height));
            candidates.push_back(BoundsWithSize(projectedTargetBounds.maxX + request.viewSpacing, projectedTargetBounds.minY, entry.width, entry.height));
            candidates.push_back(BoundsWithSize(projectedTargetBounds.minX - request.viewSpacing - entry.width, projectedTargetBounds.maxY - entry.height, entry.width, entry.height));
            candidates.push_back(BoundsWithSize(projectedTargetBounds.minX - request.viewSpacing - entry.width, projectedTargetBounds.minY, entry.width, entry.height));
            return candidates;
        };

        std::vector<std::vector<LayoutBounds>> candidateGroups;
        for (const AuxEntry& entry : compactEntries)
        {
            candidateGroups.push_back(buildCandidates(entry));
        }

        std::vector<LayoutBounds> currentChoice;
        std::vector<LayoutBounds> bestChoice;
        double bestScore = std::numeric_limits<double>::max();
        auto scoreChoice = [&](const std::vector<LayoutBounds>& choice) {
            std::vector<LayoutBounds> occupied;
            occupied.push_back(projectedTargetBounds);
            LayoutBounds merged = projectedTargetBounds;
            for (size_t i = 0; i < choice.size(); ++i)
            {
                for (const LayoutBounds& placed : occupied)
                {
                    if (BoundsOverlapWithGap(choice[i], placed, request.viewSpacing))
                    {
                        return std::numeric_limits<double>::max();
                    }
                }
                for (const LayoutBounds& reserved : projectedReservedZones)
                {
                    if (BoundsOverlapWithGap(choice[i], reserved, 0.0))
                    {
                        return std::numeric_limits<double>::max();
                    }
                }
                occupied.push_back(choice[i]);
                merged = MergeBounds(merged, choice[i]);
            }

            const double mergedWidth = BoundsWidth(merged);
            const double mergedHeight = BoundsHeight(merged);
            const double overflowX = std::max(0.0, mergedWidth - usableWidth);
            const double overflowY = std::max(0.0, mergedHeight - usableHeight);
            const double targetAspect = usableHeight > 1.0e-6 ? usableWidth / usableHeight : 1.0;
            const double mergedAspect = mergedHeight > 1.0e-6 ? mergedWidth / mergedHeight : targetAspect;
            const double aspectDelta = std::abs(std::log(std::max(1.0e-6, mergedAspect / targetAspect)));
            double score = aspectDelta * usableWidth * usableHeight * 2.0 +
                           mergedWidth * mergedHeight * 0.2 +
                           mergedWidth + mergedHeight;
            if (overflowX > 0.0 || overflowY > 0.0)
            {
                const double overflow = overflowX * usableHeight + overflowY * usableWidth + overflowX * overflowX + overflowY * overflowY;
                score += overflow * 1000000.0;
            }

            for (size_t i = 0; i < choice.size(); ++i)
            {
                const AuxEntry& entry = compactEntries[i];
                const bool isFlatPattern = entry.created != nullptr && entry.created->label == "flat pattern";
                const LayoutBounds& candidate = choice[i];
                const double candidateCenterX = (candidate.minX + candidate.maxX) * 0.5;
                const double candidateCenterY = (candidate.minY + candidate.maxY) * 0.5;
                const double projectedCenterX = (projectedTargetBounds.minX + projectedTargetBounds.maxX) * 0.5;
                const double projectedCenterY = (projectedTargetBounds.minY + projectedTargetBounds.maxY) * 0.5;
                const double projectedHeight = BoundsHeight(projectedTargetBounds);
                const double projectedUpperBandY = projectedTargetBounds.minY + projectedHeight * 0.35;
                const bool leftSideUpperBand =
                    candidateCenterX < projectedCenterX &&
                    candidate.maxY > projectedUpperBandY;
                const bool projectedUpperLeftReserve =
                    candidateCenterY > projectedCenterY &&
                    candidateCenterX < projectedTargetBounds.maxX + request.viewSpacing * 0.5;
                if (leftSideUpperBand && overflowX <= 0.0 && overflowY <= 0.0)
                {
                    return std::numeric_limits<double>::max();
                }
                if (projectedUpperLeftReserve && overflowX <= 0.0 && overflowY <= 0.0)
                {
                    return std::numeric_limits<double>::max();
                }
                if (!isFlatPattern || overflowX > 0.0 || overflowY > 0.0)
                {
                    continue;
                }
                double nearestAuxDistance = std::numeric_limits<double>::max();
                for (size_t j = 0; j < choice.size(); ++j)
                {
                    if (i == j)
                    {
                        continue;
                    }
                    const double placedCenterX = (choice[j].minX + choice[j].maxX) * 0.5;
                    const double placedCenterY = (choice[j].minY + choice[j].maxY) * 0.5;
                    const double dx = candidateCenterX - placedCenterX;
                    const double dy = candidateCenterY - placedCenterY;
                    nearestAuxDistance = std::min(nearestAuxDistance, std::sqrt(dx * dx + dy * dy));
                }
                if (nearestAuxDistance < std::numeric_limits<double>::max() * 0.5)
                {
                    score += std::max(0.0, preferredFlatSpread - nearestAuxDistance) * flatSpreadWeight;
                    score += std::max(0.0, nearestAuxDistance - maxFlatSpread) * flatSpreadWeight * 2.0;
                }
                const double projectedDx = candidateCenterX - projectedCenterX;
                const double projectedDy = candidateCenterY - projectedCenterY;
                const double projectedDistance = std::sqrt(projectedDx * projectedDx + projectedDy * projectedDy);
                score += std::max(0.0, projectedDistance - maxFlatSpread) * flatSpreadWeight;

                const bool lowerThanMain = candidateCenterY < projectedCenterY;
                const bool higherThanMain = candidateCenterY > projectedCenterY;
                const bool leftOfMain = candidateCenterX < projectedCenterX;
                const bool rightOfMain = candidateCenterX > projectedCenterX;
                if (candidate.maxY <= projectedTargetBounds.minY - request.viewSpacing * 0.5 &&
                    candidate.minX <= projectedTargetBounds.minX + request.viewSpacing * 0.5)
                {
                    score -= usableWidth * usableHeight * 0.12;
                }
                if (higherThanMain && rightOfMain &&
                    candidate.minY >= projectedTargetBounds.maxY + request.viewSpacing * 0.5 &&
                    candidate.maxX >= projectedTargetBounds.maxX - request.viewSpacing * 0.5)
                {
                    score -= usableWidth * usableHeight * 0.08;
                }
                if (higherThanMain && leftOfMain &&
                    candidate.minX <= projectedTargetBounds.minX + request.viewSpacing * 0.5)
                {
                    score += usableWidth * usableHeight * 0.45;
                }
            }
            return score;
        };

        std::function<void(size_t)> choose = [&](size_t index) {
            if (index >= compactEntries.size())
            {
                const double score = scoreChoice(currentChoice);
                if (score < bestScore)
                {
                    bestScore = score;
                    bestChoice = currentChoice;
                }
                return;
            }

            for (const LayoutBounds& candidate : candidateGroups[index])
            {
                currentChoice.push_back(candidate);
                choose(index + 1);
                currentChoice.pop_back();
            }
        };
        choose(0);

        if (bestChoice.size() != compactEntries.size())
        {
            bestChoice.clear();
            for (const std::vector<LayoutBounds>& group : candidateGroups)
            {
                if (!group.empty())
                {
                    bestChoice.push_back(group.front());
                }
            }
        }

        for (size_t i = 0; i < compactEntries.size() && i < bestChoice.size(); ++i)
        {
            placements.push_back({compactEntries[i].created->view, compactEntries[i].current, bestChoice[i]});
        }
    }
    else for (auto& group : sideGroups)
    {
        std::stable_sort(group.second.begin(), group.second.end(), [](const AuxEntry& first, const AuxEntry& second) {
            const bool firstTop = first.created != nullptr && first.created->label == "isometric";
            const bool secondTop = second.created != nullptr && second.created->label == "isometric";
            return firstTop != secondTop ? firstTop : false;
        });

        double totalHeight = 0.0;
        for (const AuxEntry& entry : group.second)
        {
            if (totalHeight > 0.0)
            {
                totalHeight += request.viewSpacing;
            }
            totalHeight += entry.height;
        }

        const bool leftSide = group.first == "left";
        const bool hasTop = std::any_of(group.second.begin(), group.second.end(), [&request](const AuxEntry& entry) {
            return entry.created != nullptr &&
                (EffectiveAuxiliaryCorner(request, *entry.created) == "TopLeft" ||
                 EffectiveAuxiliaryCorner(request, *entry.created) == "TopRight");
        });
        const bool hasBottom = std::any_of(group.second.begin(), group.second.end(), [&request](const AuxEntry& entry) {
            return entry.created != nullptr &&
                (EffectiveAuxiliaryCorner(request, *entry.created) == "BottomLeft" ||
                 EffectiveAuxiliaryCorner(request, *entry.created) == "BottomRight");
        });

        double currentTop = projectedTargetBounds.maxY;
        if (hasBottom && !hasTop)
        {
            currentTop = projectedTargetBounds.minY + totalHeight;
        }

        for (const AuxEntry& entry : group.second)
        {
            const double minX = leftSide
                ? projectedTargetBounds.minX - request.viewSpacing - entry.width
                : projectedTargetBounds.maxX + request.viewSpacing;
            const double minY = currentTop - entry.height;
            LayoutBounds target = BoundsWithSize(minX, minY, entry.width, entry.height);
            placements.push_back({entry.created->view, entry.current, target});
            currentTop = minY - request.viewSpacing;
        }
    }

    if (placements.empty())
    {
        return;
    }

    LayoutBounds allTargets = placements[0].target;
    for (const ViewTargetBounds& placement : placements)
    {
        allTargets = MergeBounds(allTargets, placement.target);
    }

    const double layoutMargin = EffectiveLayoutMargin(request);
    const double usableMinX = layoutMargin;
    const double usableMaxX = std::max(usableMinX + 20.0, sheetLength - layoutMargin);
    const double usableMinY = layoutMargin;
    const double usableMaxY = std::max(usableMinY + 20.0, sheetHeight - layoutMargin);
    const double targetCenterX = (usableMinX + usableMaxX) * 0.5;
    const double targetCenterY = (usableMinY + usableMaxY) * 0.5 + 10.0;
    double dx = targetCenterX - (allTargets.minX + allTargets.maxX) * 0.5;
    double dy = targetCenterY - (allTargets.minY + allTargets.maxY) * 0.5;
    const double allWidth = BoundsWidth(allTargets);
    const double allHeight = BoundsHeight(allTargets);
    const double usableWidth = usableMaxX - usableMinX;
    const double usableHeight = usableMaxY - usableMinY;

    if (allWidth <= usableWidth && allTargets.minX + dx < usableMinX)
    {
        dx += usableMinX - (allTargets.minX + dx);
    }
    if (allWidth <= usableWidth && allTargets.maxX + dx > usableMaxX)
    {
        dx -= (allTargets.maxX + dx) - usableMaxX;
    }
    if (allHeight <= usableHeight && allTargets.minY + dy < usableMinY)
    {
        dy += usableMinY - (allTargets.minY + dy);
    }
    if (allHeight <= usableHeight && allTargets.maxY + dy > usableMaxY)
    {
        dy -= (allTargets.maxY + dy) - usableMaxY;
    }

    for (ViewTargetBounds& placement : placements)
    {
        OffsetBounds(placement.target, dx, dy);
        MoveViewToTargetBounds(placement);
    }
}

std::vector<PlannedView> BuildProjectedLayout(
    const RequestValues& request,
    const ModelBounds& bounds,
    double scaleDenominator,
    double sheetLength,
    double sheetHeight)
{
    std::vector<PlannedView> views;

    const double viewWidth = std::max(20.0, bounds.width / std::max(1.0, scaleDenominator));
    const double viewHeight = std::max(15.0, bounds.height / std::max(1.0, scaleDenominator));
    const double sideViewWidth = std::max(12.0, std::min(viewWidth, viewHeight) * 0.45);
    const double sideViewHeight = viewHeight;
    const double horizontalGap = viewWidth * 0.5 + sideViewWidth * 0.5 + request.viewSpacing;
    const double verticalGap = viewHeight + request.viewSpacing;
    const double backGap = sideViewWidth + request.viewSpacing;
    const double centerX = 0.0;
    const double centerY = 0.0;

    if (request.front)
    {
        views.push_back({"front", "Front", NXOpen::Point3d(centerX, centerY, 0.0), true});
    }

    if (request.top)
    {
        const double y = centerY + verticalGap;
        views.push_back({"top", "Top", NXOpen::Point3d(centerX, y, 0.0), true});
    }

    if (request.bottom)
    {
        const double y = centerY - verticalGap;
        views.push_back({"bottom", "Bottom", NXOpen::Point3d(centerX, y, 0.0), true});
    }

    if (request.left)
    {
        const double x = centerX - horizontalGap;
        views.push_back({"left", "Left", NXOpen::Point3d(x, centerY, 0.0), true});
    }

    if (request.right)
    {
        const double x = centerX + horizontalGap;
        views.push_back({"right", "Right", NXOpen::Point3d(x, centerY, 0.0), true});
    }

    if (request.back)
    {
        const double x = centerX + horizontalGap + backGap;
        views.push_back({"back", "Back", NXOpen::Point3d(x, centerY, 0.0), true});
    }

    if (request.backBottom)
    {
        const double y = request.bottom ? centerY - verticalGap * 2.0 : centerY - verticalGap;
        views.push_back({"back bottom", "BackBottom", NXOpen::Point3d(centerX, y, 0.0), true});
    }

    if (views.empty())
    {
        return views;
    }

    LayoutBounds layoutBounds;
    bool initialized = false;
    for (const PlannedView& view : views)
    {
        const bool side = view.label == "left" || view.label == "right";
        AddBounds(layoutBounds, view.point, side ? sideViewWidth : viewWidth, side ? sideViewHeight : viewHeight, &initialized);
    }

    const double layoutMargin = EffectiveLayoutMargin(request);
    const double usableMinX = layoutMargin;
    const double usableMaxX = std::max(usableMinX + 20.0, sheetLength - layoutMargin);
    const double usableMinY = layoutMargin;
    const double usableMaxY = std::max(usableMinY + 20.0, sheetHeight - layoutMargin);
    const double targetCenterX = (usableMinX + usableMaxX) * 0.5;
    const double targetCenterY = (usableMinY + usableMaxY) * 0.5;
    const double currentCenterX = (layoutBounds.minX + layoutBounds.maxX) * 0.5;
    const double currentCenterY = (layoutBounds.minY + layoutBounds.maxY) * 0.5;
    ShiftViews(views, targetCenterX - currentCenterX, targetCenterY - currentCenterY);

    return views;
}

}

void BeginAutoCreateThreeViewsRunResults()
{
    g_runResultLines.clear();
}

void AddAutoCreateThreeViewsRunResultLine(const std::string& line)
{
    AppendRunResultLine(line);
}

void ShowAutoCreateThreeViewsRunResults()
{
    try
    {
        const RunResultSummary summary = SummarizeRunResults();
        const int total = summary.success + summary.failed + summary.skipped;
        std::wostringstream text;
        text << L"运行完成\n"
             << L"总数: " << total << L"\n"
             << L"成功: " << summary.success << L"\n"
             << L"失败: " << summary.failed << L"\n"
             << L"跳过: " << summary.skipped;
        if (total == 0 && summary.progress > 0)
        {
            text << L"\n已处理: " << summary.progress;
        }
        MessageBoxW(nullptr, text.str().c_str(), L"自动创建三视图", MB_OK | MB_ICONINFORMATION | MB_SETFOREGROUND);
        return;

        NXOpen::UI* ui = NXOpen::UI::GetUI();
        if (ui != nullptr && ui->NXMessageBox() != nullptr)
        {
            const std::string text = BuildRunResultText();
            ui->NXMessageBox()->Show(u8"自动创建三视图", NXOpen::NXMessageBox::DialogTypeInformation, text.c_str());
        }
    }
    catch (...)
    {
    }
}

AutoCreateThreeViewsDialog::AutoCreateThreeViewsDialog()
    : ui_(NXOpen::UI::GetUI()),
      session_(NXOpen::Session::GetSession()),
      dialog_(nullptr)
{
    const std::string dlxPath = zhihui_embedded_dialog::ExtractDlxToRandomPath(IDR_ZH_DLX_AUTOCREATETHREEVIEWS_DLX);

    if (dlxPath.empty())

    {

        throw std::runtime_error("AutoCreateThreeViews dialog resource is missing.");

    }

    dialog_ = ui_->CreateDialog(dlxPath.c_str());
    dialog_->AddInitializeHandler(NXOpen::make_callback(this, &AutoCreateThreeViewsDialog::initialize_cb));
    dialog_->AddDialogShownHandler(NXOpen::make_callback(this, &AutoCreateThreeViewsDialog::dialog_shown_cb));
    dialog_->AddUpdateHandler(NXOpen::make_callback(this, &AutoCreateThreeViewsDialog::update_cb));
    dialog_->AddApplyHandler(NXOpen::make_callback(this, &AutoCreateThreeViewsDialog::apply_cb));
    dialog_->AddOkHandler(NXOpen::make_callback(this, &AutoCreateThreeViewsDialog::ok_cb));
}

AutoCreateThreeViewsDialog::~AutoCreateThreeViewsDialog()
{
    delete dialog_;
    dialog_ = nullptr;
}

NXOpen::BlockStyler::BlockDialog::DialogResponse AutoCreateThreeViewsDialog::Launch()
{
    return dialog_->Launch();
}

std::string AutoCreateThreeViewsDialog::GetDialogFilePath() const
{
    return (CurrentModuleDirectory() / "AutoCreateThreeViews.dlx").string();
}

void AutoCreateThreeViewsDialog::initialize_cb()
{
    if (dialog_ == nullptr || dialog_->TopBlock() == nullptr)
    {
        throw NXOpen::NXException::Create(1, "AutoCreateThreeViews dialog initialization failed.");
    }

    SetString("templateName", "A3横向-公司标准");
    SetString("projectionMode", "第三角法");
    SetString("mainViewDirection", "自动识别最大面");
    SetString("scaleMode", "自动适配优先1:1");
    SetString("fileNamePattern", "{图号}_{版本}_{日期}");
    SetString("outputDirectory", "");
    SetLogical("useCurrentWorkPart", true);
    SetLogical("viewFront", true);
    SetLogical("viewTop", true);
    SetLogical("viewRight", true);
    SetLogical("viewIso", true);
    SetLogical("viewFlat", false);
    SetLogical("autoDimensions", true);
    SetLogical("exportPdf", true);
    SetLogical("exportDwg", true);
}

void AutoCreateThreeViewsDialog::dialog_shown_cb()
{
    NXOpen::Part* workPart = session_->Parts()->Work();
    if (workPart != nullptr)
    {
        SetString("currentPart", workPart->Leaf().GetText());
    }
    else
    {
        SetString("currentPart", "未打开工作部件");
    }
}

int AutoCreateThreeViewsDialog::update_cb(NXOpen::BlockStyler::UIBlock* block)
{
    (void)block;
    return 0;
}

int AutoCreateThreeViewsDialog::apply_cb()
{
    return ExecuteCreateDrawing();
}

int AutoCreateThreeViewsDialog::ok_cb()
{
    return ExecuteCreateDrawing();
}

AutoCreateThreeViewsDialog::DialogValues AutoCreateThreeViewsDialog::ReadDialogValues() const
{
    DialogValues values;
    values.templateName = ReadString("templateName", "A3横向-公司标准");
    values.projectionMode = ReadString("projectionMode", "第三角法");
    values.mainViewDirection = ReadString("mainViewDirection", "自动识别最大面");
    values.scaleMode = ReadString("scaleMode", "自动适配优先1:1");
    values.outputDirectory = ReadString("outputDirectory", "");
    values.fileNamePattern = ReadString("fileNamePattern", "{图号}_{版本}_{日期}");
    values.useCurrentWorkPart = ReadLogical("useCurrentWorkPart", true);
    values.createFrontView = ReadLogical("viewFront", true);
    values.createTopView = ReadLogical("viewTop", true);
    values.createRightView = ReadLogical("viewRight", true);
    values.createIsoView = ReadLogical("viewIso", true);
    values.createFlatPatternView = ReadLogical("viewFlat", false);
    values.autoDimensions = ReadLogical("autoDimensions", true);
    values.exportPdf = ReadLogical("exportPdf", true);
    values.exportDwg = ReadLogical("exportDwg", true);
    return values;
}

std::string AutoCreateThreeViewsDialog::ReadString(const char* blockId, const std::string& fallback) const
{
    try
    {
        NXOpen::BlockStyler::PropertyList* properties = dialog_->GetBlockProperties(blockId);
        if (properties == nullptr)
        {
            return fallback;
        }

        const NXOpen::NXString value = properties->GetString("Value");
        delete properties;
        return value.GetText();
    }
    catch (...)
    {
        return fallback;
    }
}

bool AutoCreateThreeViewsDialog::ReadLogical(const char* blockId, bool fallback) const
{
    try
    {
        NXOpen::BlockStyler::PropertyList* properties = dialog_->GetBlockProperties(blockId);
        if (properties == nullptr)
        {
            return fallback;
        }

        const bool value = properties->GetLogical("Value");
        delete properties;
        return value;
    }
    catch (...)
    {
        return fallback;
    }
}

void AutoCreateThreeViewsDialog::SetString(const char* blockId, const std::string& value) const
{
    try
    {
        NXOpen::BlockStyler::PropertyList* properties = dialog_->GetBlockProperties(blockId);
        if (properties != nullptr)
        {
            properties->SetString("Value", value.c_str());
            delete properties;
        }
    }
    catch (...)
    {
    }
}

void AutoCreateThreeViewsDialog::SetLogical(const char* blockId, bool value) const
{
    try
    {
        NXOpen::BlockStyler::PropertyList* properties = dialog_->GetBlockProperties(blockId);
        if (properties != nullptr)
        {
            properties->SetLogical("Value", value);
            delete properties;
        }
    }
    catch (...)
    {
    }
}

void AutoCreateThreeViewsDialog::ShowInfo(const std::string& message) const
{
    WriteLine(session_, "AutoCreateThreeViews: " + message);
}

void AutoCreateThreeViewsDialog::ShowError(const std::string& message) const
{
    WriteLine(session_, "AutoCreateThreeViews error: " + message);
}

void AutoCreateThreeViewsDialog::Log(const std::string& message) const
{
    WriteLine(session_, message);
}

int AutoCreateThreeViewsDialog::ExecuteCreateDrawing()
{
    try
    {
        NXOpen::Part* workPart = session_->Parts()->Work();
        if (workPart == nullptr)
        {
            ShowError("请先打开一个工作部件，再执行自动创建三视图。");
            return 1;
        }

        const DialogValues values = ReadDialogValues();
        std::ostringstream stream;
        stream << "自动创建三视图参数已读取：\n"
               << "零件: " << workPart->Leaf().GetText() << "\n"
               << "模板: " << values.templateName << "\n"
               << "投影: " << values.projectionMode << "\n"
               << "主视图: " << values.mainViewDirection << "\n"
               << "比例: " << values.scaleMode << "\n"
               << "视图: "
               << (values.createFrontView ? "正视 " : "")
               << (values.createTopView ? "俯视 " : "")
               << (values.createRightView ? "右视 " : "")
               << (values.createIsoView ? "等轴测 " : "")
               << (values.createFlatPatternView ? "展开 " : "") << "\n"
               << "导出: "
               << (values.exportPdf ? "PDF " : "")
               << (values.exportDwg ? "DWG " : "");

        Log(stream.str());
        ShowInfo("对话框和回调已可用。下一步可以在 ExecuteCreateDrawing() 中接入 NXOpen Drafting 创建图纸页、基本视图和投影视图。");
        return 0;
    }
    catch (const NXOpen::NXException& ex)
    {
        ShowError(ex.Message());
        return ex.ErrorCode();
    }
    catch (const std::exception& ex)
    {
        ShowError(ex.what());
        return 1;
    }
    catch (...)
    {
        ShowError("自动创建三视图发生未知错误。");
        return 1;
    }
}

bool IsDraftingApplicationActive(NXOpen::Session* session)
{
    int moduleId = 0;
    if (UF_ask_application_module(&moduleId) == 0 && moduleId == UF_APP_DRAFTING)
    {
        return true;
    }

    if (session == nullptr)
    {
        return false;
    }

    const NXOpen::NXString applicationNameValue = session->ApplicationName();
    const char* applicationName = applicationNameValue.GetLocaleText();
    if (applicationName == nullptr)
    {
        return false;
    }

    const std::string name(applicationName);
    return name.find("DRAFTING") != std::string::npos ||
           name.find("Drafting") != std::string::npos ||
           name.find("drafting") != std::string::npos;
}

int ExecuteAutoCreateThreeViewsFromRequest(const std::filesystem::path& requestPath)
{
    NXOpen::Session* session = NXOpen::Session::GetSession();

    try
    {
        NXOpen::Part* workPart = session->Parts()->Work();
        if (workPart == nullptr)
        {
            const std::string message = u8"失败：未打开工作部件。";
            WriteLine(session, "AutoCreateThreeViews: " + message);
            AddAutoCreateThreeViewsRunResultLine(message);
            return 1;
        }

        const std::string partLabel = PartResultName(workPart);
        ScopedPartTiming totalTiming(session, partLabel);
        TimingClock::time_point stageStarted = TimingClock::now();
        const RequestValues request = ReadRequestFile(requestPath);
        const bool firstPreferencePassForPart =
            request.targetLayer <= 0 || request.layerIndex == 0;
        if (request.inheritDraftingPreferences && firstPreferencePassForPart)
        {
            const std::filesystem::path preferenceTemplatePath = AutoTemplatePath(workPart, request);
            WriteLine(
                session,
                "AutoCreateThreeViews: follow-template drafting preferences enabled; source=" +
                    LocalPathString(preferenceTemplatePath) + ".");
            if (!InheritDraftingPreferencesFromTemplate(session, workPart, preferenceTemplatePath))
            {
                const std::string message =
                    std::string(u8"失败：") + partLabel + u8"，继承模板制图首选项失败。";
                AddAutoCreateThreeViewsRunResultLine(message);
                return 1;
            }
        }
        else if (!request.inheritDraftingPreferences)
        {
            WriteLine(session, "AutoCreateThreeViews: follow-template drafting preferences disabled.");
        }
        else
        {
            WriteLine(
                session,
                "AutoCreateThreeViews: drafting preferences already inherited for this part; "
                "skip repeated inheritance for layer index " + std::to_string(request.layerIndex) + ".");
        }
        WriteTimingLine(session, partLabel, "inherit_drafting_preferences", stageStarted);
        stageStarted = TimingClock::now();
        const ScopedTargetDrawingLayer targetLayerScope(request.targetLayer);
        const ScopedDrawingLayerIsolation modelLayerIsolation(session, request.targetLayer);
        WriteLine(session, "AutoCreateThreeViews: execute request work part=" + partLabel + ".");
        EnsureLayer230Visible(session, workPart);
        const ModelBounds bounds = AskModelBounds(workPart);
        const std::string frontDirectionMode = NormalizeFrontDirectionMode(request.frontDirectionMode);
        const bool manualFrontDirection = frontDirectionMode == "manualFaceX";
        const bool absoluteFrontDirection = frontDirectionMode == "absoluteCoordinate";
        WriteLine(
            session,
            std::string("AutoCreateThreeViews: request front direction raw=") +
                request.frontDirectionMode +
                ", normalized=" +
                frontDirectionMode +
                ", assemblyDrawing=" +
                (request.assemblyDrawing ? "true" : "false") +
                ".");
        AutoViewDirection frontDirection;
        if (absoluteFrontDirection)
        {
            WriteLine(
                session,
                "AutoCreateThreeViews: absolute coordinate front direction active; use NX Front modeling view.");
        }
        else if (manualFrontDirection)
        {
            WriteLine(session, "AutoCreateThreeViews: manual front direction mode active; switch to modeling before selection.");
            try
            {
                session->ApplicationSwitchImmediate("UG_APP_MODELING");
                UF_DISP_make_display_up_to_date();
            }
            catch (const NXOpen::NXException& ex)
            {
                WriteLine(session, std::string("AutoCreateThreeViews: switch to modeling before manual selection failed, NXException: ") + ex.Message() + ".");
            }
            catch (...)
            {
                WriteLine(session, "AutoCreateThreeViews: switch to modeling before manual selection failed.");
            }
            frontDirection = ComputeFrontDirection(workPart, request);
        }
        else if (!request.assemblyDrawing)
        {
            frontDirection = ComputeFrontDirection(workPart, request);
        }
        else
        {
            if (!TryComputeAutoFrontDirectionFromOverallBoundingBox(workPart, frontDirection))
            {
                TryComputeAutoFrontDirectionFromLeafAssemblyBodies(workPart, frontDirection);
            }
            if (!frontDirection.valid)
            {
                WriteLine(session, "AutoCreateThreeViews: assembly leaf front direction not found; fallback to Top model view as front view.");
            }
        }
        WriteTimingLine(session, partLabel, "model_bounds_and_front_direction", stageStarted);
        if (!frontDirection.valid &&
            !absoluteFrontDirection &&
            (!request.assemblyDrawing || manualFrontDirection))
        {
            const std::string failureMessage = FrontDirectionFailureMessage(frontDirectionMode);
            WriteLine(session, "AutoCreateThreeViews: front view not created; " + failureMessage);
            AddAutoCreateThreeViewsRunResultLine(std::string(u8"失败：") + partLabel + u8"，" + failureMessage);
            return 1;
        }

        if (manualFrontDirection)
        {
            WriteProgressFile(requestPath, 1, 1, "Drawing " + partLabel, false);
        }

        stageStarted = TimingClock::now();
        NXOpen::Drawings::DraftingDrawingSheet* sheet = nullptr;
        if (request.targetLayer > 0 && request.appendToCurrentSheet && workPart->DrawingSheets() != nullptr)
        {
            sheet = dynamic_cast<NXOpen::Drawings::DraftingDrawingSheet*>(
                workPart->DrawingSheets()->CurrentDrawingSheet());
            if (sheet != nullptr)
            {
                WriteLine(
                    session,
                    "AutoCreateThreeViews: append target layer " + std::to_string(request.targetLayer) +
                        " to current drawing sheet.");
            }
        }
        if (sheet == nullptr)
        {
            sheet = CreateDrawingSheet(session, workPart, request, 1.0);
        }
        if (sheet == nullptr)
        {
            const std::string message = std::string(u8"失败：") + partLabel + u8"，创建图纸页失败。";
            WriteLine(session, "AutoCreateThreeViews: " + message);
            AddAutoCreateThreeViewsRunResultLine(message);
            return 1;
        }
        try
        {
            if (!IsDraftingApplicationActive(session))
            {
                session->ApplicationSwitchImmediate("UG_APP_DRAFTING");
                workPart->Drafting()->EnterDraftingApplication();
                WriteLine(session, "AutoCreateThreeViews: entered drafting application after drawing sheet creation.");
            }
            else
            {
                WriteLine(session, "AutoCreateThreeViews: drafting application is already active; skipped duplicate application switch.");
            }
            sheet->Open();
            UF_DRAW_open_drawing(sheet->Tag());
            UF_DISP_make_display_up_to_date();
            WriteLine(session, "AutoCreateThreeViews: current drafting sheet opened before view creation.");
        }
        catch (const NXOpen::NXException& ex)
        {
            WriteLine(session, std::string("AutoCreateThreeViews: open current drafting sheet failed, NXException: ") + ex.Message() + ".");
        }
        catch (...)
        {
            WriteLine(session, "AutoCreateThreeViews: open current drafting sheet failed.");
        }

        double sheetLength = 297.0;
        double sheetHeight = 210.0;
        try
        {
            sheetLength = sheet->Length();
            sheetHeight = sheet->Height();
        }
        catch (...)
        {
        }
        const double actualSheetLength = sheetLength;
        const double actualSheetHeight = sheetHeight;
        double targetCellCenterX = actualSheetLength * 0.5;
        double targetCellCenterY = actualSheetHeight * 0.5;
        int layerCellIndex = 0;
        int layerCellCount = 1;
        int layerColumns = 1;
        int layerRows = 1;
        int layerColumn = 0;
        int layerRow = 0;
        if (request.targetLayer > 0)
        {
            const int cellCount = std::max(1, request.layersPerSheet);
            const int cellIndex = request.layerIndex % cellCount;
            layerCellIndex = cellIndex;
            layerCellCount = cellCount;
            // Use asymmetric page clearances for grouped layer drawings:
            // make fuller use of the top/left/right edges, while reserving
            // extra room above the title block for captions and dimensions.
            const double sidePadding = 4.0;
            const double topPadding = 4.0;
            const double bottomAnnotationClearance = 22.0;
            const double safeMinX = sidePadding;
            const double safeMaxX = std::max(safeMinX + 20.0, actualSheetLength - sidePadding);
            const double safeMinY = BottomTitleBlockReserve(actualSheetHeight) + bottomAnnotationClearance;
            const double safeMaxY = std::max(safeMinY + 20.0, actualSheetHeight - topPadding);
            const double safeWidth = safeMaxX - safeMinX;
            const double safeHeight = safeMaxY - safeMinY;

            // Keep one grid decision for all groups appended to the same
            // sheet.  On the first group, compare every possible row/column
            // arrangement using the target body's projected aspect ratio.
            static tag_t packedPartTag = NULL_TAG;
            static int packedCellCount = 0;
            static int packedColumns = 1;
            static int packedRows = 1;
            static std::string packedLayoutMode;
            std::string requestedLayoutMode = request.layerLayoutMode;
            std::transform(
                requestedLayoutMode.begin(),
                requestedLayoutMode.end(),
                requestedLayoutMode.begin(),
                [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
            if (requestedLayoutMode != "matrix" &&
                requestedLayoutMode != "horizontal" &&
                requestedLayoutMode != "vertical")
            {
                requestedLayoutMode = "auto";
            }
            if (cellIndex == 0 ||
                packedPartTag != workPart->Tag() ||
                packedCellCount != cellCount ||
                packedLayoutMode != requestedLayoutMode)
            {
                double frontWidth = frontDirection.valid && frontDirection.edgeLength > 1.0e-6
                    ? frontDirection.edgeLength
                    : bounds.width;
                double frontHeight = frontDirection.valid && frontDirection.faceArea > 1.0e-6
                    ? frontDirection.faceArea / std::max(1.0e-6, frontWidth)
                    : bounds.height;
                const double sizes[3] = {bounds.sizeX, bounds.sizeY, bounds.sizeZ};
                const int normalAxis = frontDirection.valid ? DominantAxisIndex(frontDirection.normal) : 2;
                const double sideDepth = sizes[std::max(0, std::min(2, normalAxis))];

                double estimatedWidth = std::max(1.0, frontWidth);
                double estimatedHeight = std::max(1.0, frontHeight);
                if (request.left) estimatedWidth += sideDepth;
                if (request.right) estimatedWidth += sideDepth;
                if (request.back) estimatedWidth += frontWidth;
                if (request.top) estimatedHeight += sideDepth;
                if (request.bottom) estimatedHeight += sideDepth;
                if (request.backBottom) estimatedHeight += frontHeight;
                if (request.iso)
                {
                    const double isoSize = std::max(frontWidth, frontHeight) * 0.55;
                    estimatedWidth += isoSize;
                    estimatedHeight = std::max(estimatedHeight, isoSize);
                }

                if (requestedLayoutMode == "horizontal")
                {
                    packedColumns = cellCount;
                    packedRows = 1;
                }
                else if (requestedLayoutMode == "vertical")
                {
                    packedColumns = 1;
                    packedRows = cellCount;
                }
                else if (requestedLayoutMode == "matrix")
                {
                    packedColumns = std::max(
                        1,
                        static_cast<int>(std::ceil(std::sqrt(static_cast<double>(cellCount)))));
                    packedRows = std::max(
                        1,
                        static_cast<int>(std::ceil(static_cast<double>(cellCount) / packedColumns)));
                }
                else
                {
                    double bestScore = std::numeric_limits<double>::max();
                    const int horizontalViewCount =
                        1 +
                        (request.left ? 1 : 0) +
                        (request.right ? 1 : 0) +
                        (request.back ? 1 : 0) +
                        (request.iso ? 1 : 0);
                    const int verticalViewCount =
                        1 +
                        (request.top ? 1 : 0) +
                        (request.bottom ? 1 : 0) +
                        (request.backBottom ? 1 : 0);
                    const double fixedPaperWidth =
                        std::max(0, horizontalViewCount - 1) * request.viewSpacing;
                    const double fixedPaperHeight =
                        std::max(0, verticalViewCount - 1) * request.viewSpacing;
                    for (int candidateColumns = 1; candidateColumns <= cellCount; ++candidateColumns)
                    {
                        const int candidateRows =
                            std::max(1, static_cast<int>(std::ceil(static_cast<double>(cellCount) / candidateColumns)));
                        const int unusedCells = candidateColumns * candidateRows - cellCount;
                        if (unusedCells >= std::min(candidateColumns, candidateRows))
                        {
                            continue;
                        }
                        const double candidateCellWidth = safeWidth / candidateColumns;
                        const double candidateCellHeight = safeHeight / candidateRows;
                        const double cellMargin = EffectiveLayoutMargin(request);
                        const double modelWidthSpace = std::max(
                            1.0,
                            candidateCellWidth - cellMargin * 2.0 - fixedPaperWidth);
                        const double modelHeightSpace = std::max(
                            1.0,
                            candidateCellHeight - cellMargin * 2.0 - fixedPaperHeight);
                        const double continuousDenominator = std::max(
                            estimatedWidth / modelWidthSpace,
                            estimatedHeight / modelHeightSpace);
                        const double integerDenominator = std::max(1.0, std::ceil(continuousDenominator - 1.0e-9));
                        const double gridImbalance =
                            std::abs(candidateColumns - candidateRows) * 0.08;
                        const double score =
                            integerDenominator + unusedCells * 0.03 + gridImbalance;

                        std::ostringstream candidateLog;
                        candidateLog << "AutoCreateThreeViews: smart layer packing candidate"
                                     << ", grid=" << candidateColumns << "x" << candidateRows
                                     << ", cell=" << candidateCellWidth << "x" << candidateCellHeight
                                     << ", fixedPaper=" << fixedPaperWidth << "x" << fixedPaperHeight
                                     << ", estimatedScale=1:" << integerDenominator
                                     << ", unused=" << unusedCells
                                     << ", score=" << score << ".";
                        WriteLine(session, candidateLog.str());
                        if (score < bestScore - 1.0e-9)
                        {
                            bestScore = score;
                            packedColumns = candidateColumns;
                            packedRows = candidateRows;
                        }
                    }
                }
                packedPartTag = workPart->Tag();
                packedCellCount = cellCount;
                packedLayoutMode = requestedLayoutMode;

                std::ostringstream packingLog;
                packingLog << "AutoCreateThreeViews: smart layer packing"
                           << ", estimatedGroup=" << estimatedWidth << "x" << estimatedHeight
                           << ", sheetSpace=" << safeWidth << "x" << safeHeight
                           << ", mode=" << requestedLayoutMode
                           << ", selectedGrid=" << packedColumns << "x" << packedRows
                           << ", groups=" << cellCount << ".";
                WriteLine(session, packingLog.str());
            }

            const int columns = packedColumns;
            const int rows = packedRows;
            const int column = cellIndex % columns;
            const int row = cellIndex / columns;
            layerColumns = columns;
            layerRows = rows;
            layerColumn = column;
            layerRow = row;
            const double cellWidth = (safeMaxX - safeMinX) / columns;
            const double cellHeight = (safeMaxY - safeMinY) / rows;
            const double rawCellCenterX = safeMinX + (static_cast<double>(column) + 0.5) * cellWidth;
            const double rawCellCenterY = safeMaxY - (static_cast<double>(row) + 0.5) * cellHeight;
            const double layoutCenterX = (safeMinX + safeMaxX) * 0.5;
            const double layoutCenterY = (safeMinY + safeMaxY) * 0.5;
            // Equal grid centers spread small body groups across the whole
            // sheet.  Keep the same cell capacity for scale calculation, but
            // pack final group centers toward the sheet center like feature 08.
            const double horizontalPack = columns > 1 ? 0.90 : 1.0;
            const double verticalPack = rows > 1 ? 0.90 : 1.0;
            targetCellCenterX = layoutCenterX + (rawCellCenterX - layoutCenterX) * horizontalPack;
            targetCellCenterY = layoutCenterY + (rawCellCenterY - layoutCenterY) * verticalPack;
            sheetLength = cellWidth;
            sheetHeight = cellHeight;

            std::ostringstream cellLog;
            cellLog << "AutoCreateThreeViews: layer layout targetLayer=" << request.targetLayer
                    << ", cell=" << (cellIndex + 1) << "/" << cellCount
                    << ", grid=" << columns << "x" << rows
                    << ", cellSize=" << cellWidth << "x" << cellHeight
                    << ", rawCenter=(" << rawCellCenterX << "," << rawCellCenterY << ")"
                    << ", targetCenter=(" << targetCellCenterX << "," << targetCellCenterY << ").";
            WriteLine(session, cellLog.str());
        }
        WriteTimingLine(session, partLabel, "create_and_open_drawing_sheet", stageStarted);

        const double temporarySheetScaleDenominator = 1000.0;
        stageStarted = TimingClock::now();
        // Changing the sheet scale while appending another layer group also
        // rescales existing views on that sheet.  NX keeps the projected-view
        // alignment point, so an earlier side view can end up inside its front
        // view.  Appended groups use explicit per-view scales and must leave the
        // established sheet scale untouched.
        if (!request.appendToCurrentSheet)
        {
            ApplyDrawingSheetScale(session, workPart, sheet, temporarySheetScaleDenominator);
        }
        else
        {
            WriteLine(session, "AutoCreateThreeViews: keep existing sheet scale while probing appended layer group.");
        }
        const double viewScaleDenominator = temporarySheetScaleDenominator;
        if ((request.assemblyDrawing && !manualFrontDirection && !absoluteFrontDirection) ||
            frontDirectionMode == "overallBoxMaxArea")
        {
            PreferOverallBoxDirectionWithMostCurves(session, workPart, frontDirection, viewScaleDenominator);
        }
        if (!frontDirection.valid && !request.assemblyDrawing)
        {
            PreferBackSideIfMoreCurves(session, workPart, frontDirection, viewScaleDenominator);
        }
        WriteTimingLine(session, partLabel, "temporary_scale_and_direction_probes", stageStarted);

        stageStarted = TimingClock::now();
        int createdCount = 0;
        const std::vector<PlannedView> plannedViews =
            BuildProjectedLayout(request, bounds, viewScaleDenominator, sheetLength, sheetHeight);
        std::vector<CreatedView> createdProjectedViews;
        std::vector<CreatedAuxiliaryView> auxiliaryViews;
        NXOpen::Drawings::BaseView* frontView = nullptr;
        for (const PlannedView& planned : plannedViews)
        {
            if (planned.label != "front")
            {
                continue;
            }

            frontView = CreateBaseView(
                session,
                workPart,
                planned.label,
                {planned.modelViewName},
                planned.point,
                false,
                viewScaleDenominator,
                frontDirection.valid ? &frontDirection : nullptr);
            if (frontView != nullptr)
            {
                createdProjectedViews.push_back({planned.label, planned.point, frontView});
                ++createdCount;
            }
            break;
        }

        if (frontDirection.valid)
        {
            std::ostringstream autoLine;
            autoLine << "AutoCreateThreeViews: front direction mode="
                     << frontDirectionMode
                     << ", normal="
                     << frontDirection.normalName
                     << ", x="
                     << frontDirection.xName
                     << ", source="
                     << frontDirection.source
                     << ", faceArea="
                     << frontDirection.faceArea
                     << ", edgeLength="
                     << frontDirection.edgeLength
                     << ", faceTag="
                     << static_cast<unsigned long long>(frontDirection.faceTag)
                     << ", edgeTag="
                     << static_cast<unsigned long long>(frontDirection.edgeTag)
                     << ".";
            WriteLine(session, autoLine.str());
        }

        if (frontView != nullptr)
        {
            NXOpen::Drawings::DraftingView* rightViewForBack = nullptr;
            NXOpen::Drawings::DraftingView* bottomViewForBack = nullptr;
            for (const PlannedView& planned : plannedViews)
            {
                if (planned.label == "front" || planned.label == "back" || planned.label == "back bottom")
                {
                    continue;
                }

                NXOpen::Drawings::ProjectedView* projectedView =
                    CreateProjectedView(session, workPart, frontView, planned.label, "front", planned.point, viewScaleDenominator);
                if (projectedView != nullptr)
                {
                    createdProjectedViews.push_back({planned.label, planned.point, projectedView});
                    if (planned.label == "right")
                    {
                        rightViewForBack = projectedView;
                    }
                    else if (planned.label == "bottom")
                    {
                        bottomViewForBack = projectedView;
                    }
                    ++createdCount;
                }
            }

            for (const PlannedView& planned : plannedViews)
            {
                if (planned.label != "back" && planned.label != "back bottom")
                {
                    continue;
                }

                NXOpen::Drawings::DraftingView* parentView = frontView;
                std::string parentLabel = "front";
                if (planned.label == "back" && rightViewForBack != nullptr)
                {
                    parentView = rightViewForBack;
                    parentLabel = "right";
                }
                else if (planned.label == "back bottom" && bottomViewForBack != nullptr)
                {
                    parentView = bottomViewForBack;
                    parentLabel = "bottom";
                }
                NXOpen::Drawings::ProjectedView* projectedView =
                    CreateProjectedView(session, workPart, parentView, planned.label, parentLabel, planned.point, viewScaleDenominator);
                if (projectedView != nullptr)
                {
                    createdProjectedViews.push_back({planned.label, planned.point, projectedView});
                    ++createdCount;
                }
            }

        }
        else
        {
            WriteLine(session, "AutoCreateThreeViews: front view was not created; projected views skipped.");
        }

        if (request.iso)
        {
            NXOpen::Drawings::BaseView* isoView = CreateBaseView(
                session,
                workPart,
                "isometric",
                {"Trimetric", "Isometric"},
                SheetCenterPoint(sheetLength, sheetHeight),
                false,
                viewScaleDenominator,
                nullptr);
            if (isoView != nullptr)
            {
                auxiliaryViews.push_back({"isometric", request.isoCorner, isoView});
                ++createdCount;
            }
        }

        if (request.flat)
        {
            NXOpen::Drawings::BaseView* flatView = CreateFlatPatternView(
                session,
                workPart,
                SheetCenterPoint(sheetLength, sheetHeight),
                viewScaleDenominator);
            if (flatView != nullptr)
            {
                auxiliaryViews.push_back({"flat pattern", request.flatCorner, flatView});
                ++createdCount;
            }
        }
        WriteTimingLine(session, partLabel, "create_projected_and_auxiliary_views", stageStarted);

        stageStarted = TimingClock::now();
        UpdateCreatedDraftingViews(
            session,
            workPart,
            createdProjectedViews,
            auxiliaryViews,
            "after_create");
        ArrangeCreatedProjectedViews(request, createdProjectedViews, sheetLength, sheetHeight);
        WriteTimingLine(session, partLabel, "initial_view_updates_and_arrangement", stageStarted);
        const double measuredScaleDenominator = temporarySheetScaleDenominator;

        std::ostringstream roughLog;
        roughLog << "AutoCreateThreeViews: read temporary 1:" << measuredScaleDenominator
                 << " drafting curves, calculate final scale before final layout.";
        WriteLine(session, roughLog.str());

        stageStarted = TimingClock::now();
        double sheetScaleDenominator = ChooseScaleDenominatorFromOneToOneActualSizes(
            session,
            request,
            createdProjectedViews,
            auxiliaryViews,
            measuredScaleDenominator,
            sheetLength,
            sheetHeight);
        double finalContentWidth = 0.0;
        double finalContentHeight = 0.0;
        if (AllLayoutSizeAtDenominator(
                request,
                createdProjectedViews,
                auxiliaryViews,
                measuredScaleDenominator,
                sheetScaleDenominator,
                finalContentWidth,
                finalContentHeight))
        {
            const double usableWidth = ScaleUsableWidth(request, sheetLength);
            const double usableHeight = ScaleUsableHeight(request, sheetHeight);
            const double usage = std::max(
                finalContentWidth / std::max(1.0, usableWidth),
                finalContentHeight / std::max(1.0, usableHeight));
            if (usage > 0.90)
            {
                sheetScaleDenominator += 1.0;
                WriteLine(session, "AutoCreateThreeViews: final layout usage is tight; enlarge scale before visible layout to 1:" + std::to_string(static_cast<int>(sheetScaleDenominator)) + ".");
            }
        }
        WriteTimingLine(session, partLabel, "measure_curves_and_choose_final_scale", stageStarted);

        stageStarted = TimingClock::now();
        if (!request.appendToCurrentSheet)
        {
            ApplyDrawingSheetScale(session, workPart, sheet, sheetScaleDenominator);
        }
        else
        {
            WriteLine(session, "AutoCreateThreeViews: keep existing sheet scale while applying appended layer view scale.");
        }
        SetAllCreatedViewScales(session, workPart, createdProjectedViews, auxiliaryViews, sheetScaleDenominator);
        UpdateCreatedDraftingViews(
            session,
            workPart,
            createdProjectedViews,
            auxiliaryViews,
            "after_final_scale");
        ArrangeAllViewsInMemory(request, createdProjectedViews, auxiliaryViews, sheetLength, sheetHeight);
        if (request.targetLayer > 0)
        {
            const double fixedLayerGroupGap = 20.0; // Same fixed group gap as feature 08.
            static tag_t placedPartTag = NULL_TAG;
            static int placedCellCount = 0;
            static std::vector<LayoutBounds> placedLayerGroupBounds;
            if (layerCellIndex == 0 ||
                placedPartTag != workPart->Tag() ||
                placedCellCount != layerCellCount)
            {
                placedPartTag = workPart->Tag();
                placedCellCount = layerCellCount;
                placedLayerGroupBounds.assign(static_cast<size_t>(layerCellCount), LayoutBounds());
            }

            const LayoutBounds currentGroupBounds = BoundsForAllViews(createdProjectedViews, auxiliaryViews);
            if (currentGroupBounds.maxX > currentGroupBounds.minX &&
                currentGroupBounds.maxY > currentGroupBounds.minY)
            {
                double fixedCenterX = targetCellCenterX;
                double fixedCenterY = targetCellCenterY;
                const double currentWidth = BoundsWidth(currentGroupBounds);
                const double currentHeight = BoundsHeight(currentGroupBounds);

                if (layerColumn > 0 && layerCellIndex > 0)
                {
                    const LayoutBounds& leftBounds = placedLayerGroupBounds[static_cast<size_t>(layerCellIndex - 1)];
                    if (leftBounds.maxX > leftBounds.minX)
                    {
                        fixedCenterX = leftBounds.maxX + fixedLayerGroupGap + currentWidth * 0.5;
                    }
                }
                if (layerRow > 0 && layerCellIndex >= layerColumns)
                {
                    const LayoutBounds& upperBounds =
                        placedLayerGroupBounds[static_cast<size_t>(layerCellIndex - layerColumns)];
                    if (upperBounds.maxY > upperBounds.minY)
                    {
                        fixedCenterY = upperBounds.minY - fixedLayerGroupGap - currentHeight * 0.5;
                    }
                }

                MoveViewGroupToCenter(
                    createdProjectedViews,
                    auxiliaryViews,
                    fixedCenterX,
                    fixedCenterY);
                placedLayerGroupBounds[static_cast<size_t>(layerCellIndex)] =
                    BoundsForAllViews(createdProjectedViews, auxiliaryViews);

                std::ostringstream fixedGapLog;
                fixedGapLog << "AutoCreateThreeViews: fixed layer group placement"
                            << ", layer=" << request.targetLayer
                            << ", cell=" << (layerCellIndex + 1) << "/" << layerCellCount
                            << ", grid=" << layerColumns << "x" << layerRows
                            << ", fixedGap=" << fixedLayerGroupGap
                            << ", center=(" << fixedCenterX << "," << fixedCenterY << ").";
                WriteLine(session, fixedGapLog.str());
            }
        }
        WriteTimingLine(session, partLabel, "apply_final_scale_update_and_arrange", stageStarted);

        stageStarted = TimingClock::now();
        CreateProjectedOverallDimensions(session, workPart, request, createdProjectedViews, frontDirection);
        CreateFlatPatternOverallDimensions(session, workPart, request, auxiliaryViews);
        CreateFlatPatternNoteBelowView(session, workPart, request, auxiliaryViews);
        CreateLayerGroupNote(session, workPart, request, createdProjectedViews, auxiliaryViews);
        if (request.targetLayer <= 0 || request.layerIndex % request.layersPerSheet == 0)
        {
            CreateTechnicalRequirementNote(
                session,
                workPart,
                request,
                actualSheetLength,
                actualSheetHeight);
        }
        WriteTimingLine(session, partLabel, "dimensions_and_notes", stageStarted);

        stageStarted = TimingClock::now();
        UpdateCreatedDraftingViews(
            session,
            workPart,
            createdProjectedViews,
            auxiliaryViews,
            "after_dimensions");
        WriteTimingLine(session, partLabel, "final_view_update", stageStarted);

        stageStarted = TimingClock::now();
        ClearCreatedDrawingSelectionAndHighlights(session, workPart, createdProjectedViews, auxiliaryViews);
        WriteTimingLine(session, partLabel, "clear_selection_and_highlights", stageStarted);

        std::ostringstream result;
        result << u8"成功：" << partLabel
               << u8"，已创建图纸页，生成 " << createdCount
               << u8" 个视图，初始比例 1:"
               << temporarySheetScaleDenominator
               << u8"，最终图纸比例 1:"
               << sheetScaleDenominator << u8"。";
        WriteLine(session, std::string("AutoCreateThreeViews: ") + result.str());
        AddAutoCreateThreeViewsRunResultLine(result.str());
        return 0;
    }
    catch (const NXOpen::NXException& ex)
    {
        const std::string message = std::string(u8"失败：NX 错误 ") + std::to_string(ex.ErrorCode()) + u8"，" + ex.Message();
        WriteLine(session, "AutoCreateThreeViews: " + message);
        AddAutoCreateThreeViewsRunResultLine(message);
        return ex.ErrorCode();
    }
    catch (const std::exception& ex)
    {
        const std::string message = std::string(u8"失败：") + ex.what();
        WriteLine(session, "AutoCreateThreeViews: " + message);
        AddAutoCreateThreeViewsRunResultLine(message);
        return 1;
    }
    catch (...)
    {
        const std::string message = u8"失败：创建工程图时发生未知错误。";
        WriteLine(session, "AutoCreateThreeViews: " + message);
        AddAutoCreateThreeViewsRunResultLine(message);
        return 1;
    }
}
