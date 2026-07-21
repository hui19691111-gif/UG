#include "TwoPointSiBianShared.hpp"

#include <NXOpen/Expression.hxx>
#include <NXOpen/Direction.hxx>
#include <NXOpen/Features_ConstructionFeatureData.hxx>
#include <NXOpen/Features_CustomDoubleAttribute.hxx>
#include <NXOpen/Features_CustomFeature.hxx>
#include <NXOpen/Features_CustomFeatureClass.hxx>
#include <NXOpen/Features_CustomFeatureClassManager.hxx>
#include <NXOpen/Features_CustomFeatureData.hxx>
#include <NXOpen/Features_CustomFeatureInformationEvent.hxx>
#include <NXOpen/Features_CustomFeatureInternalFeaturePreUpdateEvent.hxx>
#include <NXOpen/Features_CustomFeaturePreUpdateEvent.hxx>
#include <NXOpen/Features_CustomStringAttribute.hxx>
#include <NXOpen/Features_EdgeRip.hxx>
#include <NXOpen/Features_Extrude.hxx>
#include <NXOpen/Features_ExtrudeBuilder.hxx>
#include <NXOpen/Features_Feature.hxx>
#include <NXOpen/Features_FeatureCollection.hxx>
#include <NXOpen/Features_OffsetFace.hxx>
#include <NXOpen/Features_OffsetFaceBuilder.hxx>
#include <NXOpen/Features_SketchFeature.hxx>
#include <NXOpen/GeometricUtilities_Extend.hxx>
#include <NXOpen/GeometricUtilities_FeatureOffset.hxx>
#include <NXOpen/GeometricUtilities_Limits.hxx>
#include <NXOpen/Features_SheetMetal_EdgeRipBuilder.hxx>
#include <NXOpen/Features_SheetMetal_SheetmetalManager.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXMessageBox.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/Sketch.hxx>
#include <NXOpen/Line.hxx>
#include <NXOpen/UI.hxx>
#include <NXOpen/Update.hxx>

#include <uf.h>
#include <uf_curve.h>
#include <uf_modl.h>
#include <uf_modl_udf.h>

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#ifndef DllExport
#define DllExport __declspec(dllexport)
#endif

using namespace NXOpen;

namespace
{
const char* NxErrorText(const NXException& ex, char message[133])
{
    UF_get_fail_message(ex.ErrorCode(), message);
    return message[0] != '\0' ? message : "2P_SiBian core NXOpen exception.";
}

void AppendCoreLog(const std::string& message)
{
    wchar_t tempPath[MAX_PATH] = {};
    const DWORD length = GetTempPathW(MAX_PATH, tempPath);
    if (length == 0 || length >= MAX_PATH)
    {
        return;
    }
    std::wstring path(tempPath);
    path += L"TwoPointSiBian_udf.log";
    std::ofstream output(path, std::ios::app);
    if (output)
    {
        output << "[Core] " << message << '\n';
    }
}

std::string TrimAscii(const std::string& value)
{
    const auto first = std::find_if_not(
        value.begin(), value.end(),
        [](unsigned char character) { return std::isspace(character) != 0; });
    const auto last = std::find_if_not(
        value.rbegin(), value.rend(),
        [](unsigned char character) { return std::isspace(character) != 0; })
                          .base();
    return first < last ? std::string(first, last) : std::string();
}

std::string ToLowerAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char character)
                   { return static_cast<char>(std::tolower(character)); });
    return value;
}

std::string CompactFormula(std::string value)
{
    value.erase(std::remove_if(value.begin(), value.end(),
                               [](unsigned char character)
                               { return std::isspace(character) != 0; }),
                value.end());
    return ToLowerAscii(value);
}

bool EquivalentFormula(const std::string& current,
                       const std::string& requested)
{
    const std::string compactCurrent = CompactFormula(current);
    const std::string compactRequested = CompactFormula(requested);
    if (compactCurrent == compactRequested)
    {
        return true;
    }

    const auto parseSimpleNumber = [](std::string value, double& result)
    {
        value.erase(std::remove(value.begin(), value.end(), '('), value.end());
        value.erase(std::remove(value.begin(), value.end(), ')'), value.end());
        char* end = nullptr;
        result = std::strtod(value.c_str(), &end);
        return end != value.c_str() && end != nullptr && *end == '\0' &&
               std::isfinite(result);
    };
    double currentNumber = 0.0;
    double requestedNumber = 0.0;
    return parseSimpleNumber(compactCurrent, currentNumber) &&
           parseSimpleNumber(compactRequested, requestedNumber) &&
           std::abs(currentNumber - requestedNumber) <= 1.0e-10;
}

std::string Number(double value)
{
    std::ostringstream stream;
    stream.precision(15);
    stream << value;
    return stream.str();
}

std::string StringValue(Features::CustomFeatureData* data, const char* name)
{
    if (data == nullptr)
    {
        return {};
    }
    const NXString value = data->CustomStringAttributeByName(name)->Value();
    const char* text = value.GetUTF8Text();
    return text != nullptr ? text : "";
}

double DoubleValue(Features::CustomFeatureData* data, const char* name)
{
    return data->CustomDoubleAttributeByName(name)->Value();
}

std::string FeatureType(Features::Feature* feature)
{
    if (feature == nullptr)
    {
        return {};
    }
    char* type = nullptr;
    if (UF_MODL_ask_feat_type(feature->Tag(), &type) != 0 || type == nullptr)
    {
        return {};
    }
    const std::string result(type);
    UF_free(type);
    return result;
}

std::string FeatureName(Features::Feature* feature)
{
    if (feature == nullptr)
    {
        return {};
    }
    const NXString name = feature->Name();
    const char* text = name.GetLocaleText();
    return text != nullptr ? text : "";
}

std::string FeatureRole(Features::Feature* feature)
{
    if (feature == nullptr)
    {
        return {};
    }
    try
    {
        if (feature->HasUserAttribute(
                zhihui_twopoint_sibian::kConstructionRoleAttribute,
                NXObject::AttributeTypeString,
                -1))
        {
            const NXString role = feature->GetStringUserAttribute(
                zhihui_twopoint_sibian::kConstructionRoleAttribute,
                -1);
            const char* text = role.GetUTF8Text();
            if (text != nullptr && *text != '\0')
            {
                return text;
            }
        }
    }
    catch (...)
    {
        // Schema-v1 nodes have only the role stored in Feature::Name().
    }
    return FeatureName(feature);
}

std::string ExpressionSide(tag_t expression, bool leftSide)
{
    if (expression == NULL_TAG)
    {
        return {};
    }
    Expression* nxExpression = nullptr;
    try
    {
        nxExpression = dynamic_cast<Expression*>(NXObjectManager::Get(expression));
    }
    catch (...)
    {
        nxExpression = nullptr;
    }
    if (nxExpression == nullptr)
    {
        return {};
    }
    const NXString expressionString = nxExpression->ExpressionString();
    const char* expressionText = expressionString.GetUTF8Text();
    if (expressionText == nullptr)
    {
        return {};
    }
    std::string text(expressionText);
    const std::size_t equals = text.find('=');
    if (leftSide)
    {
        return TrimAscii(equals == std::string::npos ? text
                                                      : text.substr(0, equals));
    }
    return TrimAscii(equals == std::string::npos ? text
                                                  : text.substr(equals + 1));
}

bool IsThicknessExpression(const std::string& key)
{
    return key == "p7" || key == "p16" || key == "p33" ||
           key == "p42" || key.find("thickness") != std::string::npos ||
           key.find("sheet") != std::string::npos ||
           key.find("banhou") != std::string::npos;
}

bool IsRadiusExpression(const std::string& key)
{
    return key == "p8" || key == "p24" || key == "p34" ||
           key == "p43" || key == "r" ||
           key.find("radius") != std::string::npos ||
           key.find("bend") != std::string::npos;
}

bool IsClearanceExpression(const std::string& key)
{
    return key == "p9" || key == "p17" || key == "p36" ||
           key == "p45" || key == "p52" || key == "p61" ||
           key.find("clearance") != std::string::npos ||
           key.find("gap") != std::string::npos;
}

bool AllocateUdfExpressionValues(UF_MODL_udf_exp_data_t& expressionData,
                                 const std::string& thickness,
                                 const std::string& bendRadius,
                                 const std::string& clearance,
                                 bool& changed)
{
    changed = false;
    if (expressionData.num_exps <= 0)
    {
        return true;
    }
    int allocationError = 0;
    expressionData.new_exp_values = static_cast<char**>(UF_allocate_memory(
        static_cast<unsigned int>(sizeof(char*) * expressionData.num_exps),
        &allocationError));
    if (allocationError != 0 || expressionData.new_exp_values == nullptr)
    {
        return false;
    }
    std::memset(expressionData.new_exp_values,
                0,
                sizeof(char*) * expressionData.num_exps);

    std::vector<std::string> expressionKeys;
    expressionKeys.reserve(expressionData.num_exps);
    for (int index = 0; index < expressionData.num_exps; ++index)
    {
        const tag_t oldExpression = expressionData.old_exps != nullptr
                                        ? expressionData.old_exps[index]
                                        : NULL_TAG;
        expressionKeys.push_back(ToLowerAscii(
            TrimAscii(ExpressionSide(oldExpression, true))));
    }
    const auto hasKey = [&expressionKeys](const char* expected)
    {
        return std::find(expressionKeys.begin(), expressionKeys.end(),
                         expected) != expressionKeys.end();
    };
    // The right-angle templates expose the same three inputs in the order
    // p43, p45, p42 rather than [thickness, R, clearance].  Match this exact
    // signature before applying the legacy index contract used by the
    // chamfer template (p16, p17, p24).
    const bool isRightAngleExpressionSet =
        hasKey("p42") && hasKey("p43") && hasKey("p45");

    for (int index = 0; index < expressionData.num_exps; ++index)
    {
        const tag_t oldExpression = expressionData.old_exps != nullptr
                                        ? expressionData.old_exps[index]
                                        : NULL_TAG;
        const std::string key = ToLowerAscii(
            TrimAscii(ExpressionSide(oldExpression, true)));
        std::string value;
        // NX renames the exposed right-angle expressions after UDF
        // instantiation (for example p43 -> 折弯R_301).  Those descriptive
        // names are more reliable than either generated p-numbers or array
        // order and must therefore take first priority during editing.
        const bool namedThickness =
            key.find("板厚") != std::string::npos ||
            key.find("thickness") != std::string::npos ||
            key.find("sheet") != std::string::npos ||
            key.find("banhou") != std::string::npos;
        const bool namedRadius =
            key.find("折弯r") != std::string::npos ||
            key.find("radius") != std::string::npos ||
            key.find("bend") != std::string::npos;
        const bool namedClearance =
            key.find("间隙") != std::string::npos ||
            key.find("clearance") != std::string::npos ||
            key.find("gap") != std::string::npos;

        if (namedThickness)
        {
            value = thickness;
        }
        else if (namedRadius)
        {
            value = bendRadius;
        }
        else if (namedClearance)
        {
            value = clearance;
        }
        else if (isRightAngleExpressionSet && key == "p42")
        {
            value = thickness;
        }
        else if (isRightAngleExpressionSet && key == "p43")
        {
            value = bendRadius;
        }
        else if (isRightAngleExpressionSet && key == "p45")
        {
            value = clearance;
        }
        // Initial chamfer UDF creation feeds its exposed values by the stable
        // order [thickness, bend R, clearance].  Its generated names
        // p16/p17/p24 do not describe their current template meanings.
        else if (index == 0)
        {
            value = thickness;
        }
        else if (index == 1)
        {
            value = bendRadius;
        }
        else if (index == 2)
        {
            value = clearance;
        }
        else if (IsThicknessExpression(key))
        {
            value = thickness;
        }
        else if (IsRadiusExpression(key))
        {
            value = bendRadius;
        }
        else if (IsClearanceExpression(key))
        {
            value = clearance;
        }
        else
        {
            value = ExpressionSide(oldExpression, false);
        }
        if (value.empty())
        {
            value = "0";
        }

        const std::string currentValue =
            ExpressionSide(oldExpression, false);
        changed = changed || !EquivalentFormula(currentValue, value);

        AppendCoreLog("UDF expression update index=" +
                      std::to_string(index) + ", key=" + key +
                      ", rightAngleSet=" +
                      (isRightAngleExpressionSet ? "true" : "false") +
                      ", named=" +
                      (namedThickness
                           ? "thickness"
                           : (namedRadius
                                  ? "bendRadius"
                                  : (namedClearance ? "clearance" : "none"))) +
                      ", value=" + value);

        char* storedValue = static_cast<char*>(UF_allocate_memory(
            static_cast<unsigned int>(value.size() + 1), &allocationError));
        if (allocationError != 0 || storedValue == nullptr)
        {
            return false;
        }
        std::memcpy(storedValue, value.c_str(), value.size() + 1);
        expressionData.new_exp_values[index] = storedValue;
    }
    return true;
}

int UpdateInstantiatedUdf(Features::Feature* feature,
                          const std::string& thickness,
                          const std::string& bendRadius,
                          const std::string& clearance)
{
    UF_MODL_udf_exp_data_t expressionData;
    UF_MODL_udf_ref_data_t referenceData;
    UF_MODL_udf_init_exp_data(&expressionData);
    UF_MODL_udf_init_ref_data(&referenceData);

    int error = UF_MODL_ask_instantiated_udf(
        feature->Tag(), &expressionData, &referenceData);
    bool changed = false;
    if (error == 0 &&
        !AllocateUdfExpressionValues(expressionData,
                                     thickness,
                                     bendRadius,
                                     clearance,
                                     changed))
    {
        error = 1;
    }
    if (error == 0 && !changed)
    {
        AppendCoreLog("skipped unchanged UDF expressions tag=" +
                      std::to_string(feature->Tag()));
        UF_MODL_udf_free_exp_data(&expressionData);
        UF_MODL_udf_free_ref_data(&referenceData);
        return 0;
    }
    if (error == 0)
    {
        // Keep the UDF parents exactly as they were resolved during initial
        // creation.  UF_MODL_edit_instantiated_udf re-submits every parent
        // reference even when new_refs is only a copy of old_refs.  On a
        // rebuilt sheet-metal corner NX can then rematch the directed P2 edge
        // to its other endpoint (or to a neighbouring edge), which moves the
        // tool body and makes the final subtract fail.  The other successful
        // internal-feature branches already edit their owned expressions in
        // place; use the same transaction model for UDFs and let the enclosing
        // CustomFeature update regenerate the complete chain once.
        try
        {
            if (expressionData.old_exps == nullptr ||
                expressionData.new_exp_values == nullptr)
            {
                error = 1;
            }
            for (int index = 0;
                 error == 0 && index < expressionData.num_exps;
                 ++index)
            {
                const tag_t expressionTag = expressionData.old_exps[index];
                const char* formula = expressionData.new_exp_values[index];
                Expression* expression = dynamic_cast<Expression*>(
                    NXObjectManager::Get(expressionTag));
                if (expression == nullptr || formula == nullptr)
                {
                    error = 1;
                    break;
                }
                expression->SetFormula(formula);
            }
            if (error == 0)
            {
                AppendCoreLog(
                    "updated UDF expressions in place without remapping "
                    "parent references tag=" +
                    std::to_string(feature->Tag()) +
                    ", refs=" + std::to_string(referenceData.num_refs));
            }
        }
        catch (const NXException& ex)
        {
            error = ex.ErrorCode() != 0 ? ex.ErrorCode() : 1;
            AppendCoreLog("failed in-place UDF expression update tag=" +
                          std::to_string(feature->Tag()) +
                          ", error=" + std::to_string(error));
        }
        catch (...)
        {
            error = 1;
            AppendCoreLog("failed in-place UDF expression update tag=" +
                          std::to_string(feature->Tag()) +
                          ", unknown exception");
        }
    }
    UF_MODL_udf_free_exp_data(&expressionData);
    UF_MODL_udf_free_ref_data(&referenceData);
    return error;
}

int UpdateEdgeRip(Features::Feature* feature,
                   const std::string& clearance,
                   const std::string& bendRadius)
{
    Part* workPart = Session::GetSession()->Parts()->Work();
    if (workPart == nullptr || feature == nullptr)
    {
        return 1;
    }
    Features::SheetMetal::EdgeRipBuilder* builder = nullptr;
    try
    {
        builder = workPart->Features()->SheetmetalManager()
                      ->CreateEdgeRipFeatureBuilder(feature);
        Expression* width = builder->Width();
        Expression* blendRadius = builder->BlendRadius();
        if (width == nullptr || blendRadius == nullptr)
        {
            builder->Destroy();
            return 1;
        }

        const NXString currentWidth = width->GetFormula();
        const NXString currentBlendRadius = blendRadius->GetFormula();
        const char* currentWidthText = currentWidth.GetUTF8Text();
        const char* currentBlendRadiusText = currentBlendRadius.GetUTF8Text();
        if (EquivalentFormula(currentWidthText != nullptr ? currentWidthText : "",
                              clearance) &&
            EquivalentFormula(currentBlendRadiusText != nullptr
                                  ? currentBlendRadiusText
                                  : "",
                              bendRadius))
        {
            AppendCoreLog("skipped unchanged EdgeRip expressions tag=" +
                          std::to_string(feature->Tag()));
            builder->Destroy();
            return 0;
        }

        // This function runs inside CustomFeatureInternalFeaturePreUpdate.
        // Committing the EdgeRip builder here starts a nested update while its
        // directional OffsetFace children still depend on the old rip faces.
        // NX then rejects the 90-left chain (typically error 3270008).  Change
        // the feature-owned expressions in place and let the enclosing custom
        // feature update regenerate the rip exactly once after this callback.
        width->SetFormula(clearance.c_str());
        blendRadius->SetFormula(bendRadius.c_str());
        builder->Destroy();
        return 0;
    }
    catch (const NXException& ex)
    {
        if (builder != nullptr)
        {
            builder->Destroy();
        }
        return ex.ErrorCode() != 0 ? ex.ErrorCode() : 1;
    }
    catch (...)
    {
        if (builder != nullptr)
        {
            builder->Destroy();
        }
        return 1;
    }
}

std::string OffsetFormulaForFeature(Features::OffsetFace* feature,
                                    const std::string& clearance,
                                    const std::string& thickness)
{
    const std::string name = FeatureRole(feature);
    if (name == zhihui_twopoint_sibian::kRoleOffsetNegativeClearance)
    {
        return "-(" + clearance + ")";
    }
    if (name == zhihui_twopoint_sibian::kRoleOffsetSmallDirectional)
    {
        return "(0.02-(" + clearance + "))";
    }
    if (name == zhihui_twopoint_sibian::kRoleOffsetThicknessPlus002)
    {
        return "(" + thickness + "+0.02)";
    }
    if (name == zhihui_twopoint_sibian::kRoleOffsetThicknessPlusClearance)
    {
        return "(" + thickness + "+(" + clearance + "))";
    }
    if (name == zhihui_twopoint_sibian::kRoleOffsetNegative60)
    {
        return "-60";
    }

    // Compatibility for nodes created before stable role names were added.
    const std::vector<Expression*> expressions = feature->GetExpressions();
    std::string formula;
    if (!expressions.empty() && expressions.front() != nullptr)
    {
        const NXString currentFormula = expressions.front()->GetFormula();
        const char* currentText = currentFormula.GetUTF8Text();
        if (currentText != nullptr)
        {
            formula = CompactFormula(currentText);
        }
    }
    if (formula.find("0.02-") != std::string::npos)
    {
        return "(0.02-(" + clearance + "))";
    }
    if (formula.find("+0.02") != std::string::npos)
    {
        return "(" + thickness + "+0.02)";
    }
    if (formula.find("-60") != std::string::npos)
    {
        return "-60";
    }
    if (!formula.empty() && formula.find('+') != std::string::npos)
    {
        return "(" + thickness + "+(" + clearance + "))";
    }
    if (!formula.empty() && formula.find('-') != std::string::npos)
    {
        return "-(" + clearance + ")";
    }
    return {};
}

int UpdateOffsetFace(Features::OffsetFace* feature,
                     const std::string& clearance,
                     const std::string& thickness)
{
    Part* workPart = Session::GetSession()->Parts()->Work();
    if (workPart == nullptr || feature == nullptr)
    {
        return 1;
    }
    const std::string formula =
        OffsetFormulaForFeature(feature, clearance, thickness);
    if (formula.empty())
    {
        AppendCoreLog("skipped unclassified OffsetFace tag=" +
                      std::to_string(feature->Tag()));
        return 0;
    }

    Features::OffsetFaceBuilder* builder = nullptr;
    try
    {
        builder = workPart->Features()->CreateOffsetFaceBuilder(feature);
        const NXString currentFormula = builder->Distance()->GetFormula();
        const char* currentText = currentFormula.GetUTF8Text();
        if (EquivalentFormula(currentText != nullptr ? currentText : "",
                              formula))
        {
            AppendCoreLog("skipped unchanged OffsetFace expression tag=" +
                          std::to_string(feature->Tag()));
            builder->Destroy();
            return 0;
        }
        builder->Distance()->SetFormula(formula.c_str());
        // The owning CustomFeature update commits the complete internal chain.
        // A nested OffsetFace commit here can start a partial second traversal
        // before the downstream cutter/subtract members are ready.
        builder->Destroy();
        return 0;
    }
    catch (const NXException& ex)
    {
        if (builder != nullptr)
        {
            builder->Destroy();
        }
        return ex.ErrorCode() != 0 ? ex.ErrorCode() : 1;
    }
    catch (...)
    {
        if (builder != nullptr)
        {
            builder->Destroy();
        }
        return 1;
    }
}

double PointDistance(const Point3d& first, const Point3d& second)
{
    const double x = first.X - second.X;
    const double y = first.Y - second.Y;
    const double z = first.Z - second.Z;
    return std::sqrt(x * x + y * y + z * z);
}

Vector3d PointVector(const Point3d& from, const Point3d& to)
{
    return Vector3d(to.X - from.X, to.Y - from.Y, to.Z - from.Z);
}

double VectorLength(const Vector3d& value)
{
    return std::sqrt(value.X * value.X + value.Y * value.Y +
                     value.Z * value.Z);
}

bool NormalizeVector(Vector3d& value)
{
    const double length = VectorLength(value);
    if (length <= 1.0e-9)
    {
        return false;
    }
    value.X /= length;
    value.Y /= length;
    value.Z /= length;
    return true;
}

Point3d MovePoint(const Point3d& point,
                  const Vector3d& direction,
                  double distance)
{
    return Point3d(point.X + direction.X * distance,
                   point.Y + direction.Y * distance,
                   point.Z + direction.Z * distance);
}

double DotVector(const Vector3d& first, const Vector3d& second)
{
    return first.X * second.X + first.Y * second.Y +
           first.Z * second.Z;
}

bool ParsePositiveNumber(const std::string& text, double& value)
{
    try
    {
        std::size_t consumed = 0;
        value = std::fabs(std::stod(text, &consumed));
        return consumed > 0 && std::isfinite(value);
    }
    catch (...)
    {
        value = 0.0;
        return false;
    }
}

int EditLineEndpoints(Line* line,
                      const Point3d& startPoint,
                      const Point3d& endPoint)
{
    if (line == nullptr || PointDistance(startPoint, endPoint) <= 1.0e-9)
    {
        return 1;
    }
    UF_CURVE_line_t lineData;
    lineData.start_point[0] = startPoint.X;
    lineData.start_point[1] = startPoint.Y;
    lineData.start_point[2] = startPoint.Z;
    lineData.end_point[0] = endPoint.X;
    lineData.end_point[1] = endPoint.Y;
    lineData.end_point[2] = endPoint.Z;
    return UF_CURVE_edit_line_data(line->Tag(), &lineData);
}

bool OrderedSketchLineLoop(Features::Feature* extrudeFeature,
                           Sketch*& sketch,
                           std::vector<Line*>& orderedLines,
                           std::vector<Point3d>& orderedVertices,
                           std::size_t requiredLineCount = 6)
{
    sketch = nullptr;
    orderedLines.clear();
    orderedVertices.clear();
    if (extrudeFeature == nullptr)
    {
        return false;
    }

    for (Features::Feature* parent : extrudeFeature->GetParents())
    {
        Features::SketchFeature* sketchFeature =
            dynamic_cast<Features::SketchFeature*>(parent);
        if (sketchFeature != nullptr && sketchFeature->Sketch() != nullptr)
        {
            sketch = sketchFeature->Sketch();
            break;
        }
    }
    if (sketch == nullptr)
    {
        return false;
    }

    std::vector<Line*> lines;
    for (NXObject* geometry : sketch->GetAllGeometry())
    {
        Line* line = dynamic_cast<Line*>(geometry);
        if (line != nullptr)
        {
            lines.push_back(line);
        }
    }
    if (lines.size() != requiredLineCount)
    {
        return false;
    }

    constexpr double connectionTolerance = 1.0e-3;
    std::vector<bool> used(lines.size(), false);
    orderedLines.push_back(lines.front());
    orderedVertices.push_back(lines.front()->StartPoint());
    orderedVertices.push_back(lines.front()->EndPoint());
    used.front() = true;

    while (orderedLines.size() < lines.size())
    {
        const Point3d current = orderedVertices.back();
        std::size_t bestIndex = lines.size();
        bool reverse = false;
        double bestDistance = std::numeric_limits<double>::max();
        for (std::size_t index = 0; index < lines.size(); ++index)
        {
            if (used[index])
            {
                continue;
            }
            const double startDistance =
                PointDistance(current, lines[index]->StartPoint());
            const double endDistance =
                PointDistance(current, lines[index]->EndPoint());
            if (startDistance < bestDistance)
            {
                bestDistance = startDistance;
                bestIndex = index;
                reverse = false;
            }
            if (endDistance < bestDistance)
            {
                bestDistance = endDistance;
                bestIndex = index;
                reverse = true;
            }
        }
        if (bestIndex >= lines.size() ||
            bestDistance > connectionTolerance)
        {
            orderedLines.clear();
            orderedVertices.clear();
            return false;
        }
        used[bestIndex] = true;
        orderedLines.push_back(lines[bestIndex]);
        orderedVertices.push_back(reverse ? lines[bestIndex]->StartPoint()
                                          : lines[bestIndex]->EndPoint());
    }

    if (orderedVertices.size() != lines.size() + 1 ||
        PointDistance(orderedVertices.front(), orderedVertices.back()) >
            connectionTolerance)
    {
        orderedLines.clear();
        orderedVertices.clear();
        return false;
    }
    orderedVertices.pop_back();
    return true;
}

int UpdateClearanceRectangle(Features::Feature* feature,
                             const std::string& clearanceText)
{
    Part* workPart = Session::GetSession()->Parts()->Work();
    double clearance = 0.0;
    if (workPart == nullptr || feature == nullptr ||
        !ParsePositiveNumber(clearanceText, clearance))
    {
        return 1;
    }

    Features::ExtrudeBuilder* builder = nullptr;
    Sketch* sketch = nullptr;
    std::vector<Line*> loopLines;
    std::vector<Point3d> loopVertices;
    std::vector<Line*> orientedLines;
    std::vector<Point3d> orientedVertices;
    std::vector<std::array<Point3d, 2>> originalEndpoints;
    try
    {
        builder = workPart->Features()->CreateExtrudeBuilder(feature);
        if (!OrderedSketchLineLoop(feature,
                                   sketch,
                                   loopLines,
                                   loopVertices,
                                   4) ||
            builder->Direction() == nullptr)
        {
            builder->Destroy();
            return 1;
        }

        const Point3d profileOrigin = builder->Direction()->Origin();
        std::size_t originIndex = loopVertices.size();
        double originDistance = std::numeric_limits<double>::max();
        for (std::size_t index = 0; index < loopVertices.size(); ++index)
        {
            const double distance =
                PointDistance(profileOrigin, loopVertices[index]);
            if (distance < originDistance)
            {
                originDistance = distance;
                originIndex = index;
            }
        }
        builder->Destroy();
        builder = nullptr;
        if (originIndex >= loopVertices.size() || originDistance > 1.0e-2)
        {
            return 1;
        }

        const std::size_t count = loopVertices.size();
        const std::size_t nextIndex = (originIndex + 1) % count;
        const std::size_t previousIndex = (originIndex + count - 1) % count;
        const bool forwardLong =
            PointDistance(loopVertices[originIndex], loopVertices[nextIndex]) >=
            PointDistance(loopVertices[originIndex], loopVertices[previousIndex]);
        orientedVertices.reserve(count);
        orientedLines.reserve(count);
        for (std::size_t offset = 0; offset < count; ++offset)
        {
            const std::size_t vertexIndex =
                forwardLong ? (originIndex + offset) % count
                            : (originIndex + count - offset) % count;
            const std::size_t lineIndex =
                forwardLong ? vertexIndex
                            : (vertexIndex + count - 1) % count;
            orientedVertices.push_back(loopVertices[vertexIndex]);
            orientedLines.push_back(loopLines[lineIndex]);
        }

        Vector3d sideDirection =
            PointVector(orientedVertices[0], orientedVertices[3]);
        if (!NormalizeVector(sideDirection))
        {
            return 1;
        }
        const std::vector<Point3d> updatedPoints = {
            orientedVertices[0],
            orientedVertices[1],
            MovePoint(orientedVertices[1], sideDirection, clearance),
            MovePoint(orientedVertices[0], sideDirection, clearance),
            orientedVertices[0]};

        originalEndpoints.reserve(orientedLines.size());
        for (Line* line : orientedLines)
        {
            originalEndpoints.push_back(
                {line->StartPoint(), line->EndPoint()});
        }
        for (std::size_t index = 0; index < orientedLines.size(); ++index)
        {
            const int error = EditLineEndpoints(orientedLines[index],
                                                updatedPoints[index],
                                                updatedPoints[index + 1]);
            if (error != 0)
            {
                for (std::size_t restore = 0;
                     restore <= index && restore < originalEndpoints.size();
                     ++restore)
                {
                    static_cast<void>(EditLineEndpoints(
                        orientedLines[restore],
                        originalEndpoints[restore][0],
                        originalEndpoints[restore][1]));
                }
                return error;
            }
        }
        AppendCoreLog("updated right-angle clearance rectangle extrude=" +
                      std::to_string(feature->Tag()) +
                      ", clearance=" + Number(clearance));
        return 0;
    }
    catch (const NXException& ex)
    {
        if (builder != nullptr)
        {
            builder->Destroy();
        }
        AppendCoreLog("right-angle clearance rectangle update failed: " +
                      std::to_string(ex.ErrorCode()));
        return ex.ErrorCode() != 0 ? ex.ErrorCode() : 1;
    }
    catch (...)
    {
        if (builder != nullptr)
        {
            builder->Destroy();
        }
        AppendCoreLog("right-angle clearance rectangle update failed: unknown exception");
        return 1;
    }
}

bool BuildFarTopProfile(const Point3d& origin,
                        Vector3d firstDirection,
                        Vector3d secondDirection,
                        double cutLength,
                        double clearance,
                        std::vector<Point3d>& points)
{
    points.clear();
    if (!NormalizeVector(firstDirection) ||
        !NormalizeVector(secondDirection) ||
        cutLength <= 1.0e-6 || clearance <= 1.0e-6)
    {
        return false;
    }
    Vector3d centerDirection(firstDirection.X + secondDirection.X,
                             firstDirection.Y + secondDirection.Y,
                             firstDirection.Z + secondDirection.Z);
    if (!NormalizeVector(centerDirection))
    {
        return false;
    }

    auto evaluate = [&](double centerDistance,
                        std::vector<Point3d>* result,
                        double* gap) -> bool
    {
        const Point3d center =
            MovePoint(origin, centerDirection, centerDistance);
        const Vector3d originToCenter = PointVector(origin, center);
        const double firstParameter =
            DotVector(originToCenter, firstDirection);
        const double secondParameter =
            DotVector(originToCenter, secondDirection);
        if (firstParameter <= 1.0e-6 || secondParameter <= 1.0e-6)
        {
            return false;
        }
        const Point3d firstBoundary =
            MovePoint(origin, firstDirection, firstParameter);
        const Point3d secondBoundary =
            MovePoint(origin, secondDirection, secondParameter);
        Vector3d firstInward = PointVector(firstBoundary, center);
        Vector3d secondInward = PointVector(secondBoundary, center);
        if (VectorLength(firstInward) <= cutLength + 1.0e-6 ||
            VectorLength(secondInward) <= cutLength + 1.0e-6 ||
            !NormalizeVector(firstInward) ||
            !NormalizeVector(secondInward))
        {
            return false;
        }
        const Point3d firstInner =
            MovePoint(firstBoundary, firstInward, cutLength);
        const Point3d secondInner =
            MovePoint(secondBoundary, secondInward, cutLength);
        if (gap != nullptr)
        {
            *gap = PointDistance(firstInner, secondInner);
        }
        if (result != nullptr)
        {
            *result = {origin,
                       firstBoundary,
                       firstInner,
                       center,
                       secondInner,
                       secondBoundary,
                       origin};
        }
        return true;
    };

    double lowDistance = cutLength + 1.0e-4;
    double highDistance = lowDistance;
    double highGap = 0.0;
    for (int index = 0; index < 80; ++index)
    {
        if (evaluate(highDistance, nullptr, &highGap) &&
            highGap >= clearance)
        {
            break;
        }
        highDistance *= 1.25;
        highGap = 0.0;
    }
    if (highGap < clearance)
    {
        return false;
    }
    for (int index = 0; index < 80; ++index)
    {
        const double middle = (lowDistance + highDistance) * 0.5;
        double middleGap = 0.0;
        if (evaluate(middle, nullptr, &middleGap) &&
            middleGap >= clearance)
        {
            highDistance = middle;
        }
        else
        {
            lowDistance = middle;
        }
    }
    return evaluate(highDistance, &points, nullptr);
}

int UpdateCornerEdgeCut(Features::Feature* feature,
                        const std::string& clearanceText,
                        double thickness,
                        bool allowLegacyDetection)
{
    Part* workPart = Session::GetSession()->Parts()->Work();
    double clearance = 0.0;
    thickness = std::fabs(thickness);
    if (workPart == nullptr || feature == nullptr ||
        !ParsePositiveNumber(clearanceText, clearance) ||
        thickness <= 1.0e-9)
    {
        return 1;
    }

    Features::ExtrudeBuilder* builder = nullptr;
    try
    {
        if (allowLegacyDetection)
        {
            // The edge-based cutter has no sketch parent.  This separates it
            // from the UDF extrusion and the sketch-driven far corner cut.
            for (Features::Feature* parent : feature->GetParents())
            {
                if (dynamic_cast<Features::SketchFeature*>(parent) != nullptr)
                {
                    return 0;
                }
            }
        }

        builder = workPart->Features()->CreateExtrudeBuilder(feature);
        const double oldStart =
            builder->Limits()->StartExtend()->Value()->NumberValue();
        const double oldEnd =
            builder->Limits()->EndExtend()->Value()->NumberValue();
        const double oldStartOffset =
            builder->Offset()->StartOffset()->NumberValue();
        const double oldEndOffset =
            builder->Offset()->EndOffset()->NumberValue();

        if (allowLegacyDetection)
        {
            const double tolerance = std::max(1.0e-3, thickness * 1.0e-3);
            if (std::fabs(oldStart - thickness) > tolerance ||
                std::fabs(oldStartOffset) > tolerance ||
                std::fabs(std::fabs(oldEndOffset) - thickness) > tolerance ||
                oldEnd <= oldStart + 1.0e-9)
            {
                builder->Destroy();
                return 0;
            }
        }

        builder->Limits()->StartExtend()->Value()->SetFormula(
            Number(thickness).c_str());
        builder->Limits()->EndExtend()->Value()->SetFormula(
            Number(thickness + clearance).c_str());
        // This callback already runs inside the owning CustomFeature update.
        // Committing an internal extrusion here starts a nested model update;
        // for the right-angle chain NX then performs a second, incomplete
        // traversal that stops before the two corner cutters.  Edit the
        // feature-owned expressions only and let the enclosing update commit
        // the complete construction chain once.
        builder->Destroy();
        builder = nullptr;

        if (allowLegacyDetection)
        {
            feature->SetUserAttribute(
                zhihui_twopoint_sibian::kConstructionRoleAttribute,
                -1,
                zhihui_twopoint_sibian::kRoleExtrudeCornerEdgeCut,
                Update::OptionLater);
            feature->SetName(
                zhihui_twopoint_sibian::kRoleExtrudeCornerEdgeCut);
        }
        AppendCoreLog("updated corner-edge cut extrude=" +
                      std::to_string(feature->Tag()) +
                      ", oldStart=" + Number(oldStart) +
                      ", oldEnd=" + Number(oldEnd) +
                      ", newStart=" + Number(thickness) +
                      ", newEnd=" + Number(thickness + clearance) +
                      ", startOffset=" + Number(oldStartOffset) +
                      ", endOffset=" + Number(oldEndOffset) +
                      ", clearance=" + Number(clearance));
        return 0;
    }
    catch (const NXException& ex)
    {
        if (builder != nullptr)
        {
            builder->Destroy();
        }
        AppendCoreLog("corner-edge cut update failed: " +
                      std::to_string(ex.ErrorCode()));
        return ex.ErrorCode() != 0 ? ex.ErrorCode() : 1;
    }
    catch (...)
    {
        if (builder != nullptr)
        {
            builder->Destroy();
        }
        AppendCoreLog("corner-edge cut update failed: unknown exception");
        return 1;
    }
}

int UpdateFarTopCut(Features::Feature* feature,
                    const std::string& clearanceText,
                    const std::string& bendRadiusText,
                    double thickness,
                    bool allowLegacyDetection)
{
    Part* workPart = Session::GetSession()->Parts()->Work();
    if (workPart == nullptr || feature == nullptr)
    {
        return 1;
    }
    double clearance = 0.0;
    double bendRadius = 0.0;
    if (!ParsePositiveNumber(clearanceText, clearance) ||
        !ParsePositiveNumber(bendRadiusText, bendRadius))
    {
        return 1;
    }

    Features::ExtrudeBuilder* builder = nullptr;
    Sketch* sketch = nullptr;
    std::vector<Line*> loopLines;
    std::vector<Point3d> loopVertices;
    std::vector<Line*> orientedLines;
    std::vector<std::array<Point3d, 2>> originalEndpoints;
    std::string updateStage = "initialization";
    try
    {
        updateStage = "extrude builder creation";
        builder = workPart->Features()->CreateExtrudeBuilder(feature);
        if (!OrderedSketchLineLoop(feature,
                                   sketch,
                                   loopLines,
                                   loopVertices) ||
            builder->Direction() == nullptr)
        {
            builder->Destroy();
            return allowLegacyDetection ? 0 : 1;
        }

        const Point3d profileOrigin = builder->Direction()->Origin();
        std::size_t originIndex = loopVertices.size();
        double originDistance = std::numeric_limits<double>::max();
        for (std::size_t index = 0; index < loopVertices.size(); ++index)
        {
            const double distance =
                PointDistance(profileOrigin, loopVertices[index]);
            if (distance < originDistance)
            {
                originDistance = distance;
                originIndex = index;
            }
        }
        if (originIndex >= loopVertices.size() || originDistance > 1.0e-2)
        {
            builder->Destroy();
            return allowLegacyDetection ? 0 : 1;
        }

        const std::size_t count = loopVertices.size();
        std::vector<Point3d> orientedVertices;
        orientedVertices.reserve(count);
        orientedLines.reserve(count);
        for (std::size_t offset = 0; offset < count; ++offset)
        {
            const std::size_t vertexIndex = (originIndex + offset) % count;
            orientedVertices.push_back(loopVertices[vertexIndex]);
            orientedLines.push_back(loopLines[vertexIndex]);
        }

        const double currentCutLength =
            std::fabs(builder->Limits()
                          ->EndExtend()
                          ->Value()
                          ->NumberValue());
        const double profileTolerance =
            std::max(0.05, currentCutLength * 0.10);
        const double profileError =
            std::fabs(PointDistance(orientedVertices[1],
                                    orientedVertices[2]) -
                      currentCutLength) +
            std::fabs(PointDistance(orientedVertices[4],
                                    orientedVertices[5]) -
                      currentCutLength);
        if (allowLegacyDetection &&
            profileError > profileTolerance * 2.0)
        {
            builder->Destroy();
            return 0;
        }

        Vector3d firstDirection =
            PointVector(orientedVertices[0], orientedVertices[1]);
        Vector3d secondDirection =
            PointVector(orientedVertices[0], orientedVertices[5]);
        const double newCutLength = std::fabs(thickness) + bendRadius;
        std::vector<Point3d> updatedPoints;
        if (!BuildFarTopProfile(orientedVertices[0],
                                firstDirection,
                                secondDirection,
                                newCutLength,
                                clearance,
                                updatedPoints) ||
            updatedPoints.size() != count + 1)
        {
            builder->Destroy();
            return 1;
        }

        originalEndpoints.reserve(orientedLines.size());
        for (Line* line : orientedLines)
        {
            originalEndpoints.push_back(
                {line->StartPoint(), line->EndPoint()});
        }
        updateStage = "UF sketch-line editing";
        for (std::size_t index = 0; index < orientedLines.size(); ++index)
        {
            const int lineError = EditLineEndpoints(orientedLines[index],
                                                    updatedPoints[index],
                                                    updatedPoints[index + 1]);
            if (lineError != 0)
            {
                for (std::size_t restoreIndex = 0;
                     restoreIndex <= index &&
                     restoreIndex < originalEndpoints.size();
                     ++restoreIndex)
                {
                    static_cast<void>(EditLineEndpoints(
                        orientedLines[restoreIndex],
                        originalEndpoints[restoreIndex][0],
                        originalEndpoints[restoreIndex][1]));
                }
                builder->Destroy();
                builder = nullptr;
                AppendCoreLog("far top cut line edit failed at index=" +
                              std::to_string(index) +
                              ", error=" + std::to_string(lineError));
                return lineError;
            }
        }
        // Do not update the sketch or the whole model inside an internal
        // feature callback. NX updates the internal parent/child chain after
        // this callback returns, just as it does for an edited UDF instance.
        updateStage = "extrude limit edit";
        builder->Limits()->EndExtend()->Value()->SetFormula(
            Number(newCutLength).c_str());
        // Do not commit an internal builder from an internal-feature callback.
        // The enclosing CustomFeature update owns the transaction and will
        // regenerate this extrusion after the edited sketch lines and limit
        // expression have been returned to NX.
        updateStage = "extrude builder release";
        builder->Destroy();
        builder = nullptr;
        if (allowLegacyDetection)
        {
            feature->SetUserAttribute(
                zhihui_twopoint_sibian::kConstructionRoleAttribute,
                -1,
                zhihui_twopoint_sibian::kRoleExtrudeFarTopCut,
                Update::OptionLater);
            feature->SetName(
                zhihui_twopoint_sibian::kRoleExtrudeFarTopCut);
        }
        AppendCoreLog("updated far top cut extrude=" +
                      std::to_string(feature->Tag()) +
                      ", cutLength=" + Number(newCutLength) +
                      ", clearance=" + Number(clearance));
        return 0;
    }
    catch (const NXException& ex)
    {
        if (!originalEndpoints.empty())
        {
            for (std::size_t index = 0;
                 index < originalEndpoints.size() &&
                 index < orientedLines.size();
                 ++index)
            {
                static_cast<void>(EditLineEndpoints(
                    orientedLines[index],
                    originalEndpoints[index][0],
                    originalEndpoints[index][1]));
            }
        }
        if (builder != nullptr)
        {
            builder->Destroy();
        }
        char message[133] = {0};
        AppendCoreLog("far top cut NXException at " + updateStage +
                      ", code=" + std::to_string(ex.ErrorCode()) +
                      ", message=" + NxErrorText(ex, message));
        return ex.ErrorCode() != 0 ? ex.ErrorCode() : 1;
    }
    catch (...)
    {
        if (!originalEndpoints.empty())
        {
            for (std::size_t index = 0;
                 index < originalEndpoints.size() &&
                 index < orientedLines.size();
                 ++index)
            {
                static_cast<void>(EditLineEndpoints(
                    orientedLines[index],
                    originalEndpoints[index][0],
                    originalEndpoints[index][1]));
            }
        }
        if (builder != nullptr)
        {
            builder->Destroy();
        }
        AppendCoreLog("far top cut unknown exception at " + updateStage);
        return 1;
    }
}

int PreUpdateCallback(Features::CustomFeaturePreUpdateEvent* event)
{
    if (event == nullptr || event->GetCustomFeature() == nullptr)
    {
        return 1;
    }

    const std::vector<Features::ConstructionFeatureData*> existing =
        event->GetConstructionFeatures();
    if (!existing.empty())
    {
        // Construction features are created once.  Supplying the complete
        // keep-list prevents NX from deleting them; their parameters are
        // updated later in InternalFeaturePreUpdateCallback.
        event->SetConstructionFeatures(existing);
        AppendCoreLog("PreUpdate retained construction count=" +
                      std::to_string(existing.size()));
        return 0;
    }

    // Initial creation is still performed by the UI implementation because it
    // owns the complete branch-selection and topology inference workflow.
    using BuildCallback = int(__cdecl*)(void*);
    HMODULE uiModule = GetModuleHandleW(L"TwoPointSiBianUI.dll");
    if (uiModule != nullptr)
    {
        BuildCallback build = reinterpret_cast<BuildCallback>(
            GetProcAddress(uiModule,
                           "ZhihuiTwoPointSiBianBuildCustomFeature"));
        if (build != nullptr)
        {
            return build(event);
        }
    }

    event->GetCustomFeature()->LogDiagnostic(
        1,
        "The 2P_SiBian initial construction callback is unavailable.",
        Features::Feature::DiagnosticTypeWarning);
    return 1;
}

int InternalFeaturePreUpdateCallback(
    Features::CustomFeatureInternalFeaturePreUpdateEvent* event)
{
    if (event == nullptr || event->GetCustomFeature() == nullptr ||
        event->GetFeature() == nullptr)
    {
        return 1;
    }
    Features::CustomFeature* customFeature = event->GetCustomFeature();
    Features::Feature* internalFeature = event->GetFeature();
    Features::CustomFeatureData* data = customFeature->FeatureData();
    if (data == nullptr)
    {
        return 1;
    }

    const std::string clearance =
        StringValue(data, zhihui_twopoint_sibian::kAttrClearance);
    const std::string bendRadius =
        StringValue(data, zhihui_twopoint_sibian::kAttrBendRadius);
    const std::string thickness =
        Number(DoubleValue(data, zhihui_twopoint_sibian::kAttrThickness));
    const std::string type = ToLowerAscii(FeatureType(internalFeature));
    const std::string role = FeatureRole(internalFeature);

    int error = 0;
    if (role == zhihui_twopoint_sibian::kRoleUdf ||
        type.find("udf") != std::string::npos)
    {
        error = UpdateInstantiatedUdf(internalFeature,
                                      thickness,
                                      bendRadius,
                                      clearance);
    }
    else if (role == zhihui_twopoint_sibian::kRoleEdgeRip ||
             dynamic_cast<Features::EdgeRip*>(internalFeature) != nullptr ||
             type.find("edge rip") != std::string::npos ||
             type.find("edgerip") != std::string::npos)
    {
        error = UpdateEdgeRip(internalFeature, clearance, bendRadius);
    }
    else if (Features::OffsetFace* offset =
                 dynamic_cast<Features::OffsetFace*>(internalFeature))
    {
        error = UpdateOffsetFace(offset, clearance, thickness);
    }
    else if (role == zhihui_twopoint_sibian::kRoleExtrudeFarTopCut)
    {
        error = UpdateFarTopCut(internalFeature,
                                clearance,
                                bendRadius,
                                DoubleValue(data,
                                            zhihui_twopoint_sibian::kAttrThickness),
                                false);
    }
    else if (role == zhihui_twopoint_sibian::kRoleExtrudeCornerEdgeCut)
    {
        error = UpdateCornerEdgeCut(
            internalFeature,
            clearance,
            DoubleValue(data, zhihui_twopoint_sibian::kAttrThickness),
            false);
    }
    else if (role ==
                 zhihui_twopoint_sibian::kRoleExtrudeRightClearanceRectangle ||
             role ==
                 zhihui_twopoint_sibian::kRoleExtrudeLeftClearanceRectangle)
    {
        error = UpdateClearanceRectangle(internalFeature, clearance);
    }
    else if (role.empty() &&
             dynamic_cast<Features::Extrude*>(internalFeature) != nullptr)
    {
        // Compatibility for features made before either extrusion role was
        // stored.  The edge cutter is the only qualifying no-sketch,
        // thickness-offset extrusion; the far cutter is the distinctive
        // internal six-line sketch profile.
        error = UpdateCornerEdgeCut(
            internalFeature,
            clearance,
            DoubleValue(data, zhihui_twopoint_sibian::kAttrThickness),
            true);
        if (error == 0 && FeatureRole(internalFeature).empty())
        {
            error = UpdateFarTopCut(
                internalFeature,
                clearance,
                bendRadius,
                DoubleValue(data,
                            zhihui_twopoint_sibian::kAttrThickness),
                true);
        }
    }

    AppendCoreLog("InternalFeaturePreUpdate tag=" +
                  std::to_string(internalFeature->Tag()) +
                  ", type=" + type +
                  ", role=" + role +
                  ", result=" + std::to_string(error));
    if (error != 0)
    {
        customFeature->LogDiagnostic(
            error,
            "A 2P_SiBian internal construction feature could not be edited in place.",
            Features::Feature::DiagnosticTypeWarning);
    }
    return error;
}

int InformationCallback(Features::CustomFeatureInformationEvent* event)
{
    event->SetInformation(
        "2P_SiBian: one editable custom feature. Double-click to reopen the program dialog.");
    return 0;
}
}

extern "C" DllExport void ufusr(char* param, int* returnCode, int rlen)
{
    (void)param;
    (void)rlen;
    if (returnCode != nullptr)
    {
        *returnCode = 0;
    }

    const int initStatus = UF_initialize();
    if (initStatus != 0)
    {
        if (returnCode != nullptr)
        {
            *returnCode = initStatus;
        }
        return;
    }

    try
    {
        Features::CustomFeatureClassManager* manager =
            Session::GetSession()->CustomFeatureClassManager();
        Features::CustomFeatureClass* featureClass =
            manager->GetClassFromName(
                zhihui_twopoint_sibian::kFeatureClassName);
        featureClass->AddInternalFeaturePreUpdateHandler(
            make_callback(&InternalFeaturePreUpdateCallback));
        featureClass->AddPreUpdateHandler(make_callback(&PreUpdateCallback));
        featureClass->AddInformationHandler(make_callback(&InformationCallback));
        AppendCoreLog("registered PreUpdate and InternalFeaturePreUpdate handlers");
    }
    catch (const NXException& ex)
    {
        char message[133] = {0};
        UI::GetUI()->NXMessageBox()->Show("2P_SiBian Core",
                                          NXMessageBox::DialogTypeError,
                                          NxErrorText(ex, message));
        if (returnCode != nullptr)
        {
            *returnCode = ex.ErrorCode();
        }
    }
    catch (...)
    {
        UI::GetUI()->NXMessageBox()->Show(
            "2P_SiBian Core",
            NXMessageBox::DialogTypeError,
            "2P_SiBian core registration failed.");
        if (returnCode != nullptr)
        {
            *returnCode = -1;
        }
    }
    UF_terminate();
}

extern "C" DllExport int ufusr_ask_unload(void)
{
    return Session::LibraryUnloadOptionAtTermination;
}
