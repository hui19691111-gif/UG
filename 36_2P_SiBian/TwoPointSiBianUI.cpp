#include "TwoPointSiBianUI.hpp"

#include <NXOpen/BlockStyler_Enumeration.hxx>
#include <NXOpen/BlockStyler_PropertyList.hxx>
#include <NXOpen/BlockStyler_StringBlock.hxx>
#include <NXOpen/BlockStyler_Toggle.hxx>
#include <NXOpen/CartesianCoordinateSystem.hxx>
#include <NXOpen/CoordinateSystemCollection.hxx>
#include <NXOpen/Curve.hxx>
#include <NXOpen/CurveCollection.hxx>
#include <NXOpen/CurveFeatureRule.hxx>
#include <NXOpen/Direction.hxx>
#include <NXOpen/DirectionCollection.hxx>
#include <NXOpen/DisplayableObject.hxx>
#include <NXOpen/Features_CustomAttribute.hxx>
#include <NXOpen/Features_CustomAttributeCollection.hxx>
#include <NXOpen/Features_CustomDoubleAttribute.hxx>
#include <NXOpen/Features_CustomIntegerAttribute.hxx>
#include <NXOpen/Features_CustomLogicalAttribute.hxx>
#include <NXOpen/Features_CustomStringAttribute.hxx>
#include <NXOpen/Features_BooleanBuilder.hxx>
#include <NXOpen/Features_ConstructionFeatureData.hxx>
#include <NXOpen/Features_CustomFeatureBuilder.hxx>
#include <NXOpen/Features_CustomFeatureData.hxx>
#include <NXOpen/Features_CustomFeatureDataCollection.hxx>
#include <NXOpen/Features_CustomFeaturePreUpdateEvent.hxx>
#include <NXOpen/Features_CustomTagAttribute.hxx>
#include <NXOpen/Features_EditWithRollbackManager.hxx>
#include <NXOpen/Features_ExtrudeBuilder.hxx>
#include <NXOpen/Features_Feature.hxx>
#include <NXOpen/Features_FeatureCollection.hxx>
#include <NXOpen/Features_FeatureGroup.hxx>
#include <NXOpen/Features_OffsetFaceBuilder.hxx>
#include <NXOpen/Features_SketchFeature.hxx>
#include <NXOpen/Features_SheetMetal_EdgeRipBuilder.hxx>
#include <NXOpen/Features_SheetMetal_SheetmetalManager.hxx>
#include <NXOpen/EdgeDumbRule.hxx>
#include <NXOpen/FaceDumbRule.hxx>
#include <NXOpen/FaceFeatureRule.hxx>
#include <NXOpen/MeasureDistance.hxx>
#include <NXOpen/MeasureFaces.hxx>
#include <NXOpen/MeasureManager.hxx>
#include <NXOpen/GeometricUtilities_BooleanOperation.hxx>
#include <NXOpen/GeometricUtilities_Extend.hxx>
#include <NXOpen/GeometricUtilities_FeatureOffset.hxx>
#include <NXOpen/GeometricUtilities_FeatureOptions.hxx>
#include <NXOpen/GeometricUtilities_Limits.hxx>
#include <NXOpen/GeometricUtilities_SmartVolumeProfileBuilder.hxx>
#include <NXOpen/Line.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXMatrix.hxx>
#include <NXOpen/NXMessageBox.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/BodyCollection.hxx>
#include <NXOpen/BodyDumbRule.hxx>
#include <NXOpen/Point.hxx>
#include <NXOpen/PointCollection.hxx>
#include <NXOpen/Plane.hxx>
#include <NXOpen/PlaneCollection.hxx>
#include <NXOpen/Section.hxx>
#include <NXOpen/SectionCollection.hxx>
#include <NXOpen/ScRuleFactory.hxx>
#include <NXOpen/ScCollector.hxx>
#include <NXOpen/ScCollectorCollection.hxx>
#include <NXOpen/SelectionIntentRule.hxx>
#include <NXOpen/SelectionIntentRuleOptions.hxx>
#include <NXOpen/SmartObject.hxx>
#include <NXOpen/Sketch.hxx>
#include <NXOpen/SketchCollection.hxx>
#include <NXOpen/SimpleSketchInPlaceBuilder.hxx>
#include <NXOpen/SketchInPlaceBuilder.hxx>
#include <NXOpen/TaggedObject.hxx>
#include <NXOpen/Unit.hxx>
#include <NXOpen/UnitCollection.hxx>
#include <NXOpen/Update.hxx>
#include <NXOpen/Expression.hxx>
#include <NXOpen/UserDefinedTemplate_Collection.hxx>
#include <NXOpen/UserDefinedTemplate_Definition.hxx>
#include <NXOpen/UserDefinedTemplate_DefinitionBuilder.hxx>
#include <NXOpen/UserDefinedTemplate_Instantiation.hxx>
#include <NXOpen/UserDefinedTemplate_InstantiationBuilder.hxx>

#include <uf.h>
#include <uf_assem.h>
#include <uf_curve.h>
#include <uf_disp.h>
#include <uf_eval.h>
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
#include <utility>
#include <vector>

using namespace NXOpen;

namespace
{
TwoPointSiBianUI* gActiveTwoPointSiBianDialog = nullptr;

constexpr double kPointTolerance = 1.0e-4;
constexpr double kPlaneTolerance = 1.0e-3;
constexpr double kSmartInnerLoopMinimumAreaRatio = 0.15;
constexpr double kCornerExtensionDistance = 0.05;
constexpr double kThicknessMinimumAreaRatio = 0.60;
constexpr double kThicknessProjectionAreaErrorRatio = 0.05;
constexpr double kEndpointPairMinimumAngleDegrees = 150.0;
constexpr double kEndpointPairMaximumAngleDegrees = 180.0;
constexpr double kConvexCornerMaximumAngleDegrees = 180.0;
constexpr const char* kTemplatePartName = "2p_SiBian_1.prt";
constexpr const char* kTemplate90LeftPartName = "2P_SiBian_90.prt";
constexpr const char* kTemplate90RightPartName = "2P_SiBian_90R.prt";
constexpr const char* kTemplate90ClearanceGroovePartName = "90JianXiCao.prt";
constexpr const char* kTemplate90ClearanceGrooveRightPartName = "90JianXiCaoR.prt";
constexpr const char* kFeatureTemplatePartName = "2p_SiBian_1_FT.prt";
constexpr const char* kFeatureTemplate90LeftPartName = "2P_SiBian_90_FT.prt";
constexpr const char* kFeatureTemplate90RightPartName = "2P_SiBian_90R_FT.prt";
constexpr const char* kFeatureTemplate90ClearanceGroovePartName = "90JianXiCao_FT.prt";
constexpr const char* kFeatureTemplate90ClearanceGrooveRightPartName = "90JianXiCaoR_FT.prt";
constexpr const wchar_t* kTempTemplateRoot = L"ZhihuiSheetMetal\\UDF\\36_2P_SiBian";

void SetConstructionRole(Features::Feature* feature, const char* role)
{
    if (feature == nullptr || role == nullptr || *role == '\0')
    {
        return;
    }
    try
    {
        feature->SetUserAttribute(
            zhihui_twopoint_sibian::kConstructionRoleAttribute,
            -1,
            role,
            Update::OptionLater);
    }
    catch (...)
    {
        // The readable feature name below remains a compatibility fallback.
    }
    try
    {
        feature->SetName(role);
    }
    catch (...)
    {
        // Stable metadata improves deterministic editing, but its failure must
        // not invalidate otherwise-valid initial geometry creation.
    }
}

std::string ConstructionRole(Features::Feature* feature)
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
    }
    const NXString name = feature->Name();
    const char* text = name.GetLocaleText();
    return text != nullptr ? text : "";
}

bool ResolvePrimaryUdfDirectedP2(Features::CustomFeature* customFeature,
                                 Point3d& p2,
                                 tag_t& edgeTag)
{
    edgeTag = NULL_TAG;
    if (customFeature == nullptr)
    {
        return false;
    }

    // The primary UDF is the last UDF construction member.  Its third
    // external reference is a(P2,Y+); the stored reverse flag identifies
    // which natural edge endpoint was used as the directed P2 origin.
    Features::Feature* primaryUdf = nullptr;
    const std::vector<Features::ConstructionFeatureData*> construction =
        customFeature->GetConstructionFeatures();
    for (auto iterator = construction.rbegin();
         iterator != construction.rend();
         ++iterator)
    {
        Features::Feature* feature =
            *iterator != nullptr ? (*iterator)->GetFeature() : nullptr;
        if (feature == nullptr)
        {
            continue;
        }
        const char* typeText = feature->FeatureType().GetLocaleText();
        std::string featureType = typeText != nullptr ? typeText : "";
        std::transform(featureType.begin(),
                       featureType.end(),
                       featureType.begin(),
                       [](unsigned char value)
                       {
                           return static_cast<char>(std::tolower(value));
                       });
        if (featureType.find("udf") != std::string::npos)
        {
            primaryUdf = feature;
            break;
        }
    }
    if (primaryUdf == nullptr)
    {
        return false;
    }

    UF_MODL_udf_exp_data_t expressionData;
    UF_MODL_udf_ref_data_t referenceData;
    UF_MODL_udf_init_exp_data(&expressionData);
    UF_MODL_udf_init_ref_data(&referenceData);
    const int askResult = UF_MODL_ask_instantiated_udf(
        primaryUdf->Tag(), &expressionData, &referenceData);
    bool resolved = false;
    if (askResult == 0 && referenceData.num_refs > 2 &&
        referenceData.old_refs != nullptr)
    {
        edgeTag = referenceData.old_refs[2];
        try
        {
            Edge* edge = dynamic_cast<Edge*>(NXObjectManager::Get(edgeTag));
            if (edge != nullptr && edge->SolidEdgeType() == Edge::EdgeTypeLinear)
            {
                Point3d naturalStart;
                Point3d naturalEnd;
                edge->GetVertices(&naturalStart, &naturalEnd);
                const bool reversed =
                    referenceData.reverse_refs_dir != nullptr &&
                    referenceData.reverse_refs_dir[2] == UF_MODL_UDF_REVERSE_DIR;
                p2 = reversed ? naturalEnd : naturalStart;
                resolved = true;
            }
        }
        catch (...)
        {
            resolved = false;
        }
    }
    UF_MODL_udf_free_exp_data(&expressionData);
    UF_MODL_udf_free_ref_data(&referenceData);
    return resolved;
}

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
    double normalDot = 0.0;
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

UdfTemplateSpec TemplateSpecForMode(TwoPointSiBianUI::FeatureMode mode,
                                    bool useNinetyClearanceGrooveTemplate,
                                    bool useNinetyClearanceGrooveRightTemplate)
{
    if (useNinetyClearanceGrooveRightTemplate)
    {
        return {IDR_UDF_TEMPLATE_90_JIAN_XI_CAO_R_PRT,
                L"90JianXiCaoR_",
                kTemplate90ClearanceGrooveRightPartName};
    }
    if (useNinetyClearanceGrooveTemplate)
    {
        return {IDR_UDF_TEMPLATE_90_JIAN_XI_CAO_PRT,
                L"90JianXiCao_",
                kTemplate90ClearanceGroovePartName};
    }
    if (mode == TwoPointSiBianUI::FeatureMode::NinetyLeft)
    {
        return {IDR_UDF_TEMPLATE_90L_PRT, L"2P_SiBian_90_", kTemplate90LeftPartName};
    }
    if (mode == TwoPointSiBianUI::FeatureMode::NinetyRight)
    {
        return {IDR_UDF_TEMPLATE_90R_PRT, L"2P_SiBian_90R_", kTemplate90RightPartName};
    }

    return {IDR_UDF_TEMPLATE_PRT, L"2p_SiBian_1_", kTemplatePartName};
}

UdfTemplateSpec FeatureTemplateSpecForMode(
    TwoPointSiBianUI::FeatureMode mode,
    bool useNinetyClearanceGrooveTemplate,
    bool useNinetyClearanceGrooveRightTemplate)
{
    if (useNinetyClearanceGrooveRightTemplate)
    {
        return {IDR_FEATURE_TEMPLATE_90_JIAN_XI_CAO_R_PRT,
                L"90JianXiCaoR_FT_",
                kFeatureTemplate90ClearanceGrooveRightPartName};
    }
    if (useNinetyClearanceGrooveTemplate)
    {
        return {IDR_FEATURE_TEMPLATE_90_JIAN_XI_CAO_PRT,
                L"90JianXiCao_FT_",
                kFeatureTemplate90ClearanceGroovePartName};
    }
    if (mode == TwoPointSiBianUI::FeatureMode::NinetyLeft)
    {
        return {IDR_FEATURE_TEMPLATE_90L_PRT,
                L"2P_SiBian_90_FT_",
                kFeatureTemplate90LeftPartName};
    }
    if (mode == TwoPointSiBianUI::FeatureMode::NinetyRight)
    {
        return {IDR_FEATURE_TEMPLATE_90R_PRT,
                L"2P_SiBian_90R_FT_",
                kFeatureTemplate90RightPartName};
    }

    return {IDR_FEATURE_TEMPLATE_PRT,
            L"2p_SiBian_1_FT_",
            kFeatureTemplatePartName};
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

    bool Extract(TwoPointSiBianUI::FeatureMode mode,
                 bool useNinetyClearanceGrooveTemplate,
                 bool useNinetyClearanceGrooveRightTemplate,
                 std::string& path,
                 std::string& trace,
                 bool featureTemplate = false)
    {
        Cleanup();
        const UdfTemplateSpec spec = featureTemplate
            ? FeatureTemplateSpecForMode(mode,
                                         useNinetyClearanceGrooveTemplate,
                                         useNinetyClearanceGrooveRightTemplate)
            : TemplateSpecForMode(mode,
                                  useNinetyClearanceGrooveTemplate,
                                  useNinetyClearanceGrooveRightTemplate);

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

Point3d AddVector(const Point3d& point, const Vector3d& vector)
{
    return Point3d(point.X + vector.X,
                   point.Y + vector.Y,
                   point.Z + vector.Z);
}

Vector3d ScaleVector(const Vector3d& vector, double scale)
{
    return Vector3d(vector.X * scale,
                    vector.Y * scale,
                    vector.Z * scale);
}

Vector3d ProjectVectorToPlane(const Vector3d& vector, const Vector3d& normal)
{
    return Vector3d(vector.X - normal.X * Dot(vector, normal),
                    vector.Y - normal.Y * Dot(vector, normal),
                    vector.Z - normal.Z * Dot(vector, normal));
}

Vector3d InwardOffsetDirectionInPlane(const Vector3d& lineDirection,
                                      const Vector3d& planeNormal,
                                      const Vector3d& inwardHint)
{
    Vector3d direction = Cross(planeNormal, lineDirection);
    if (!Normalize(direction))
    {
        return Vector3d(0.0, 0.0, 0.0);
    }
    if (Dot(direction, inwardHint) < 0.0)
    {
        direction = ScaleVector(direction, -1.0);
    }
    return direction;
}

bool IntersectCoplanarLines(const Point3d& firstOrigin,
                            const Vector3d& firstDirection,
                            const Point3d& secondOrigin,
                            const Vector3d& secondDirection,
                            const Vector3d& planeNormal,
                            Point3d& intersection)
{
    const double denominator = Dot(Cross(firstDirection, secondDirection), planeNormal);
    if (std::fabs(denominator) <= 1.0e-9)
    {
        return false;
    }
    const double firstParameter =
        Dot(Cross(Subtract(secondOrigin, firstOrigin), secondDirection), planeNormal) /
        denominator;
    intersection = AddVector(firstOrigin, ScaleVector(firstDirection, firstParameter));
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

double SquareMillimetersToPartSquareUnits(Part* part,
                                          double squareMillimeters)
{
    if (part == nullptr || squareMillimeters <= 0.0)
    {
        return squareMillimeters;
    }

    double millimetersPerPartUnit = 1.0;
    switch (part->PartUnits())
    {
    case BasePart::UnitsInches:
        millimetersPerPartUnit = 25.4;
        break;
    case BasePart::UnitsMeters:
        millimetersPerPartUnit = 1000.0;
        break;
    case BasePart::UnitsMicrometers:
        millimetersPerPartUnit = 0.001;
        break;
    case BasePart::UnitsMillimeters:
    case BasePart::UnitsMix:
    default:
        millimetersPerPartUnit = 1.0;
        break;
    }
    return squareMillimeters /
           (millimetersPerPartUnit * millimetersPerPartUnit);
}

bool SampleLoopEdge(tag_t edgeTag, std::vector<Point3d>& samples)
{
    samples.clear();
    if (edgeTag == NULL_TAG)
    {
        return false;
    }

    UF_EVAL_p_t evaluator = nullptr;
    if (UF_EVAL_initialize(edgeTag, &evaluator) == 0 && evaluator != nullptr)
    {
        bool evaluationSucceeded = false;
        double limits[2] = {0.0, 0.0};
        if (UF_EVAL_ask_limits(evaluator, limits) == 0 &&
            limits[1] > limits[0])
        {
            evaluationSucceeded = true;
            constexpr int sampleCount = 128;
            for (int sampleIndex = 0; sampleIndex <= sampleCount; ++sampleIndex)
            {
                const double parameter =
                    limits[0] +
                    (limits[1] - limits[0]) *
                        static_cast<double>(sampleIndex) /
                        static_cast<double>(sampleCount);
                double evaluatedPoint[3] = {0.0, 0.0, 0.0};
                if (UF_EVAL_evaluate(evaluator,
                                     0,
                                     parameter,
                                     evaluatedPoint,
                                     nullptr) != 0)
                {
                    evaluationSucceeded = false;
                    samples.clear();
                    break;
                }
                const Point3d point(evaluatedPoint[0],
                                    evaluatedPoint[1],
                                    evaluatedPoint[2]);
                if (samples.empty() ||
                    !AlmostSamePoint(samples.back(), point))
                {
                    samples.push_back(point);
                }
            }
        }
        UF_EVAL_free(evaluator);
        return evaluationSucceeded && samples.size() >= 2;
    }

    samples.clear();
    try
    {
        Edge* edge =
            dynamic_cast<Edge*>(NXObjectManager::Get(edgeTag));
        if (edge == nullptr ||
            edge->SolidEdgeType() != Edge::EdgeTypeLinear)
        {
            return false;
        }
        Point3d first;
        Point3d second;
        edge->GetVertices(&first, &second);
        samples.push_back(first);
        if (!AlmostSamePoint(first, second))
        {
            samples.push_back(second);
        }
    }
    catch (...)
    {
        samples.clear();
    }
    return samples.size() >= 2;
}

bool BuildOrderedLoopPolyline(uf_list_p_t edgeList,
                              std::vector<Point3d>& orderedPoints)
{
    orderedPoints.clear();
    if (edgeList == nullptr)
    {
        return false;
    }

    int edgeCount = 0;
    if (UF_MODL_ask_list_count(edgeList, &edgeCount) != 0 ||
        edgeCount <= 0)
    {
        return false;
    }

    std::vector<std::vector<Point3d>> edgeSamples;
    edgeSamples.reserve(static_cast<std::size_t>(edgeCount));
    for (int edgeIndex = 0; edgeIndex < edgeCount; ++edgeIndex)
    {
        tag_t edgeTag = NULL_TAG;
        std::vector<Point3d> samples;
        if (UF_MODL_ask_list_item(edgeList,
                                  edgeIndex,
                                  &edgeTag) == 0 &&
            SampleLoopEdge(edgeTag, samples))
        {
            edgeSamples.push_back(std::move(samples));
        }
    }
    if (edgeSamples.size() != static_cast<std::size_t>(edgeCount))
    {
        return false;
    }

    orderedPoints = edgeSamples.front();
    std::vector<bool> used(edgeSamples.size(), false);
    used.front() = true;
    for (std::size_t appendedCount = 1;
         appendedCount < edgeSamples.size();
         ++appendedCount)
    {
        const Point3d currentEnd = orderedPoints.back();
        std::size_t bestIndex = edgeSamples.size();
        bool reverseBest = false;
        double bestDistance = std::numeric_limits<double>::max();
        for (std::size_t candidateIndex = 0;
             candidateIndex < edgeSamples.size();
             ++candidateIndex)
        {
            if (used[candidateIndex] ||
                edgeSamples[candidateIndex].empty())
            {
                continue;
            }
            const double frontDistance =
                Distance(currentEnd,
                         edgeSamples[candidateIndex].front());
            const double backDistance =
                Distance(currentEnd,
                         edgeSamples[candidateIndex].back());
            if (frontDistance < bestDistance)
            {
                bestDistance = frontDistance;
                bestIndex = candidateIndex;
                reverseBest = false;
            }
            if (backDistance < bestDistance)
            {
                bestDistance = backDistance;
                bestIndex = candidateIndex;
                reverseBest = true;
            }
        }
        if (bestIndex >= edgeSamples.size() ||
            bestDistance > kPlaneTolerance * 10.0)
        {
            orderedPoints.clear();
            return false;
        }

        std::vector<Point3d>& samples = edgeSamples[bestIndex];
        if (reverseBest)
        {
            std::reverse(samples.begin(), samples.end());
        }
        for (std::size_t pointIndex = 1;
             pointIndex < samples.size();
             ++pointIndex)
        {
            if (!AlmostSamePoint(orderedPoints.back(),
                                 samples[pointIndex]))
            {
                orderedPoints.push_back(samples[pointIndex]);
            }
        }
        used[bestIndex] = true;
    }

    if (orderedPoints.size() < 3 ||
        !AlmostSamePoint(orderedPoints.front(),
                         orderedPoints.back()))
    {
        orderedPoints.clear();
        return false;
    }
    orderedPoints.pop_back();
    return orderedPoints.size() >= 3;
}

bool PlanarLoopArea(Face* face,
                    uf_list_p_t edgeList,
                    double& area)
{
    area = 0.0;
    Point3d planeOrigin;
    Vector3d planeNormal;
    std::vector<Point3d> loopPoints;
    if (!FacePlaneData(face, planeOrigin, planeNormal) ||
        !BuildOrderedLoopPolyline(edgeList, loopPoints))
    {
        return false;
    }

    Vector3d seed(std::fabs(planeNormal.X) < 0.9 ? 1.0 : 0.0,
                  std::fabs(planeNormal.X) < 0.9 ? 0.0 : 1.0,
                  0.0);
    Vector3d xAxis = Cross(seed, planeNormal);
    if (!Normalize(xAxis))
    {
        return false;
    }
    Vector3d yAxis = Cross(planeNormal, xAxis);
    if (!Normalize(yAxis))
    {
        return false;
    }

    double twiceArea = 0.0;
    for (std::size_t pointIndex = 0;
         pointIndex < loopPoints.size();
         ++pointIndex)
    {
        const Point3d& first = loopPoints[pointIndex];
        const Point3d& second =
            loopPoints[(pointIndex + 1) % loopPoints.size()];
        const Vector3d firstOffset =
            Subtract(first, planeOrigin);
        const Vector3d secondOffset =
            Subtract(second, planeOrigin);
        const double firstX = Dot(firstOffset, xAxis);
        const double firstY = Dot(firstOffset, yAxis);
        const double secondX = Dot(secondOffset, xAxis);
        const double secondY = Dot(secondOffset, yAxis);
        twiceArea +=
            firstX * secondY - secondX * firstY;
    }
    area = std::fabs(twiceArea) * 0.5;
    return area > kPointTolerance;
}

bool FaceBoundaryPoints(
    Face* face,
    std::vector<Point3d>& points,
    double minimumAcceptedInnerLoopArea =
        std::numeric_limits<double>::quiet_NaN())
{
    points.clear();
    if (face == nullptr)
    {
        return false;
    }

    // Every caller uses the same smart-selection rule unless it supplies an
    // already measured threshold: ignore inner loops smaller than 15% of the
    // face area and retain larger inner loops as valid local boundaries.
    // Historically the no-argument calls used infinity, which silently
    // discarded every inner loop and made Q/Q3/Q4 and helper geometry behave
    // differently from the original P1/P2 selection.
    if (std::isnan(minimumAcceptedInnerLoopArea))
    {
        try
        {
            Session* session = Session::GetSession();
            Part* workPart =
                session != nullptr ? session->Parts()->Work() : nullptr;
            if (workPart != nullptr)
            {
                Unit* areaUnit =
                    workPart->UnitCollection()->FindObject(
                        "SquareMilliMeter");
                Unit* lengthUnit =
                    workPart->UnitCollection()->FindObject(
                        "MilliMeter");
                if (areaUnit != nullptr && lengthUnit != nullptr)
                {
                    std::vector<IParameterizedSurface*> measuredFaces;
                    measuredFaces.push_back(face);
                    MeasureFaces* measurement =
                        workPart->MeasureManager()->NewFaceProperties(
                            areaUnit,
                            lengthUnit,
                            0.99,
                            measuredFaces);
                    if (measurement != nullptr)
                    {
                        const double faceArea =
                            SquareMillimetersToPartSquareUnits(
                                workPart,
                                measurement->Area());
                        delete measurement;
                        minimumAcceptedInnerLoopArea =
                            faceArea > kPointTolerance
                                ? faceArea *
                                      kSmartInnerLoopMinimumAreaRatio
                                : std::numeric_limits<double>::infinity();
                    }
                }
            }
        }
        catch (const NXException& ex)
        {
            AppendDebugLog(
                "FaceBoundaryPoints automatic inner-loop area measurement NXException: " +
                UfMessage(ex.ErrorCode()));
        }
        catch (...)
        {
            AppendDebugLog(
                "FaceBoundaryPoints automatic inner-loop area measurement unknown exception.");
        }
        if (std::isnan(minimumAcceptedInnerLoopArea))
        {
            minimumAcceptedInnerLoopArea =
                std::numeric_limits<double>::infinity();
        }
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

    uf_loop_p_t loopList = nullptr;
    int loopResult = 1;
    auto releaseLoopList = [&loopList]()
    {
        if (loopList != nullptr)
        {
            UF_MODL_delete_loop_list(&loopList);
            loopList = nullptr;
        }
    };

    try
    {
        loopResult =
            UF_MODL_ask_face_loops(face->Tag(), &loopList);
        if (loopResult == 0 && loopList != nullptr)
        {
            int loopCount = 0;
            UF_MODL_ask_loop_list_count(loopList, &loopCount);
            int acceptedInnerLoopCount = 0;
            for (int loopIndex = 0; loopIndex < loopCount; ++loopIndex)
            {
                int loopType = 0;
                uf_list_p_t edgeList = nullptr;
                if (UF_MODL_ask_loop_list_item(loopList,
                                               loopIndex,
                                               &loopType,
                                               &edgeList) != 0 ||
                    edgeList == nullptr)
                {
                    continue;
                }

                bool acceptLoop = loopType == 1;
                if (loopType == 2 &&
                    std::isfinite(minimumAcceptedInnerLoopArea))
                {
                    double loopArea = 0.0;
                    const bool measured =
                        PlanarLoopArea(face, edgeList, loopArea);
                    const double selectedFaceArea =
                        minimumAcceptedInnerLoopArea /
                        kSmartInnerLoopMinimumAreaRatio;
                    const double areaRatio =
                        selectedFaceArea > kPointTolerance
                            ? loopArea / selectedFaceArea
                            : 0.0;
                    acceptLoop =
                        measured &&
                        areaRatio + 1.0e-9 >=
                            kSmartInnerLoopMinimumAreaRatio;
                    std::ostringstream loopTrace;
                    loopTrace
                        << "FaceBoundaryPoints inner loop: face="
                        << face->Tag()
                        << ", loopIndex=" << loopIndex
                        << ", area=" << loopArea
                        << ", ratioToSelectedFace=" << areaRatio
                        << ", minimumArea="
                        << minimumAcceptedInnerLoopArea
                        << ", accepted="
                        << (acceptLoop ? "true" : "false")
                        << ", measured="
                        << (measured ? "true" : "false");
                    AppendDebugLog(loopTrace.str());
                }
                if (!acceptLoop)
                {
                    continue;
                }

                std::vector<Point3d> loopEndpoints;
                auto addUniqueLoopEndpoint =
                    [&loopEndpoints](const Point3d& point)
                {
                    for (const Point3d& existing :
                         loopEndpoints)
                    {
                        if (AlmostSamePoint(existing, point))
                        {
                            return;
                        }
                    }
                    loopEndpoints.push_back(point);
                };
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
                    addUniqueLoopEndpoint(first);
                    addUniqueLoopEndpoint(second);
                }
                if (loopType == 2 &&
                    loopEndpoints.size() < 2)
                {
                    AppendDebugLog(
                        "FaceBoundaryPoints accepted inner loop ignored because it has fewer than two distinct topology endpoints: face=" +
                        std::to_string(face->Tag()) +
                        ", loopIndex=" +
                        std::to_string(loopIndex));
                    continue;
                }
                for (const Point3d& endpoint : loopEndpoints)
                {
                    addUniquePoint(endpoint);
                }
                if (loopType == 2)
                {
                    ++acceptedInnerLoopCount;
                }
            }

            releaseLoopList();
            if (points.size() >= 2)
            {
                AppendDebugLog(
                    "FaceBoundaryPoints used accepted loop points=" +
                    std::to_string(points.size()) +
                    " face=" +
                    std::to_string(face->Tag()) +
                    ", acceptedInnerLoops=" +
                    std::to_string(acceptedInnerLoopCount));
                return true;
            }
        }

        releaseLoopList();
        AppendDebugLog("FaceBoundaryPoints peripheral loop unavailable; falling back to all face edges. result=" +
                       std::to_string(loopResult));
    }
    catch (const NXException& ex)
    {
        releaseLoopList();
        AppendDebugLog("FaceBoundaryPoints peripheral loop NXException: " + UfMessage(ex.ErrorCode()));
        points.clear();
    }
    catch (...)
    {
        releaseLoopList();
        AppendDebugLog("FaceBoundaryPoints peripheral loop unknown exception.");
        points.clear();
    }

    if (std::isfinite(minimumAcceptedInnerLoopArea))
    {
        AppendDebugLog(
            "FaceBoundaryPoints filtered smart selection stopped because classified face loops were unavailable; all-edge fallback was intentionally disabled.");
        return false;
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

bool FindFaceInteriorSamplePoint(Face* face, Point3d& samplePoint)
{
    Point3d planeOrigin;
    Vector3d planeNormal;
    if (face == nullptr || !FacePlaneData(face, planeOrigin, planeNormal))
    {
        return false;
    }

    auto isStrictlyInsideFace = [face](const Point3d& candidate) {
        int status = 0;
        return PointContainmentOnFace(face, candidate, status) &&
               status == 1;
    };

    // The untrimmed planar origin can fall outside a concave face or inside
    // one of its holes, so accept it only after trimmed-face containment.
    if (isStrictlyInsideFace(planeOrigin))
    {
        samplePoint = planeOrigin;
        return true;
    }

    std::vector<Point3d> boundaryPoints;
    if (!FaceBoundaryPoints(face, boundaryPoints) || boundaryPoints.empty())
    {
        return false;
    }

    Point3d boundaryCentroid(0.0, 0.0, 0.0);
    for (const Point3d& point : boundaryPoints)
    {
        boundaryCentroid.X += point.X;
        boundaryCentroid.Y += point.Y;
        boundaryCentroid.Z += point.Z;
    }
    const double pointCount = static_cast<double>(boundaryPoints.size());
    boundaryCentroid.X /= pointCount;
    boundaryCentroid.Y /= pointCount;
    boundaryCentroid.Z /= pointCount;
    if (isStrictlyInsideFace(boundaryCentroid))
    {
        samplePoint = boundaryCentroid;
        return true;
    }

    // Move inward from outer-loop vertices. Containment status 1 guarantees
    // that the accepted point is in the trimmed face region, not on an edge
    // and not in a hole.
    constexpr std::array<double, 12> inwardFractions = {
        0.002, 0.005, 0.01, 0.02, 0.04, 0.08,
        0.15, 0.25, 0.40, 0.55, 0.70, 0.85};
    for (const Point3d& boundaryPoint : boundaryPoints)
    {
        const Vector3d towardCentroid =
            Subtract(boundaryCentroid, boundaryPoint);
        for (double fraction : inwardFractions)
        {
            const Point3d candidate =
                AddVector(boundaryPoint,
                          ScaleVector(towardCentroid, fraction));
            if (isStrictlyInsideFace(candidate))
            {
                samplePoint = candidate;
                return true;
            }
        }
    }

    // Final fallback for strongly concave faces: sample the planar bounding
    // rectangle and let NX's trimmed-face containment reject holes/outside.
    Vector3d xAxis;
    Vector3d yAxis;
    if (!FaceProjectionAxes(face, planeNormal, xAxis, yAxis))
    {
        return false;
    }
    double minimumX = std::numeric_limits<double>::max();
    double maximumX = -std::numeric_limits<double>::max();
    double minimumY = std::numeric_limits<double>::max();
    double maximumY = -std::numeric_limits<double>::max();
    for (const Point3d& point : boundaryPoints)
    {
        const ProjectionPoint2d projected =
            ProjectToPlane2d(point, planeOrigin, xAxis, yAxis);
        minimumX = std::min(minimumX, projected.x);
        maximumX = std::max(maximumX, projected.x);
        minimumY = std::min(minimumY, projected.y);
        maximumY = std::max(maximumY, projected.y);
    }
    if (maximumX - minimumX <= kPointTolerance ||
        maximumY - minimumY <= kPointTolerance)
    {
        return false;
    }

    struct GridCandidate
    {
        Point3d point;
        double centerDistanceSquared = 0.0;
    };
    std::vector<GridCandidate> gridCandidates;
    constexpr int gridDivisionCount = 20;
    const double centerX = (minimumX + maximumX) * 0.5;
    const double centerY = (minimumY + maximumY) * 0.5;
    for (int xIndex = 1; xIndex < gridDivisionCount; ++xIndex)
    {
        const double x = minimumX +
                         (maximumX - minimumX) *
                             static_cast<double>(xIndex) /
                             static_cast<double>(gridDivisionCount);
        for (int yIndex = 1; yIndex < gridDivisionCount; ++yIndex)
        {
            const double y = minimumY +
                             (maximumY - minimumY) *
                                 static_cast<double>(yIndex) /
                                 static_cast<double>(gridDivisionCount);
            GridCandidate candidate;
            candidate.point =
                AddVector(AddVector(planeOrigin, ScaleVector(xAxis, x)),
                          ScaleVector(yAxis, y));
            const double deltaX = x - centerX;
            const double deltaY = y - centerY;
            candidate.centerDistanceSquared =
                deltaX * deltaX + deltaY * deltaY;
            gridCandidates.push_back(candidate);
        }
    }
    std::sort(gridCandidates.begin(), gridCandidates.end(),
              [](const GridCandidate& first,
                 const GridCandidate& second) {
                  return first.centerDistanceSquared <
                         second.centerDistanceSquared;
              });
    for (const GridCandidate& candidate : gridCandidates)
    {
        if (isStrictlyInsideFace(candidate.point))
        {
            samplePoint = candidate.point;
            return true;
        }
    }

    return false;
}

bool BuildFaceInteriorGridSamples(Face* face,
                                  const Point3d& planeOrigin,
                                  const Vector3d& xAxis,
                                  const Vector3d& yAxis,
                                  int divisionCount,
                                  std::vector<Point3d>& samples)
{
    samples.clear();
    if (face == nullptr || divisionCount < 2)
    {
        return false;
    }

    std::vector<Point3d> boundaryPoints;
    if (!FaceBoundaryPoints(face, boundaryPoints) ||
        boundaryPoints.size() < 2)
    {
        return false;
    }

    double minimumX = std::numeric_limits<double>::max();
    double maximumX = -std::numeric_limits<double>::max();
    double minimumY = std::numeric_limits<double>::max();
    double maximumY = -std::numeric_limits<double>::max();
    for (const Point3d& point : boundaryPoints)
    {
        const ProjectionPoint2d projected =
            ProjectToPlane2d(point, planeOrigin, xAxis, yAxis);
        minimumX = std::min(minimumX, projected.x);
        maximumX = std::max(maximumX, projected.x);
        minimumY = std::min(minimumY, projected.y);
        maximumY = std::max(maximumY, projected.y);
    }
    if (maximumX - minimumX <= kPointTolerance ||
        maximumY - minimumY <= kPointTolerance)
    {
        return false;
    }

    auto addUniqueSample = [&samples](const Point3d& point) {
        for (const Point3d& existing : samples)
        {
            if (AlmostSamePoint(existing, point))
            {
                return;
            }
        }
        samples.push_back(point);
    };

    // Cell-center sampling avoids deliberately placing points on trimming
    // edges. NX face containment removes concave cut-outs and all inner holes.
    for (int xIndex = 0; xIndex < divisionCount; ++xIndex)
    {
        const double x = minimumX +
                         (maximumX - minimumX) *
                             (static_cast<double>(xIndex) + 0.5) /
                             static_cast<double>(divisionCount);
        for (int yIndex = 0; yIndex < divisionCount; ++yIndex)
        {
            const double y = minimumY +
                             (maximumY - minimumY) *
                                 (static_cast<double>(yIndex) + 0.5) /
                                 static_cast<double>(divisionCount);
            const Point3d candidate =
                AddVector(AddVector(planeOrigin, ScaleVector(xAxis, x)),
                          ScaleVector(yAxis, y));
            int status = 0;
            if (PointContainmentOnFace(face, candidate, status) &&
                status == 1)
            {
                addUniqueSample(candidate);
            }
        }
    }

    Point3d guaranteedInteriorPoint;
    if (FindFaceInteriorSamplePoint(face, guaranteedInteriorPoint))
    {
        addUniqueSample(guaranteedInteriorPoint);
    }
    return !samples.empty();
}

double ProjectedFaceGroupCoverage(
    const std::vector<Point3d>& baseSamples,
    const Vector3d& baseNormal,
    const std::vector<ThicknessCandidate>& groupCandidates,
    std::size_t* coveredSampleCount = nullptr)
{
    if (coveredSampleCount != nullptr)
    {
        *coveredSampleCount = 0;
    }
    if (baseSamples.empty() || groupCandidates.empty())
    {
        return 0.0;
    }

    std::size_t covered = 0;
    for (const Point3d& baseSample : baseSamples)
    {
        bool sampleCovered = false;
        for (const ThicknessCandidate& candidate : groupCandidates)
        {
            if (candidate.face == nullptr)
            {
                continue;
            }
            const Point3d projected =
                AddVector(baseSample,
                          ScaleVector(baseNormal,
                                      candidate.signedPlaneDistance));
            int status = 0;
            if (PointContainmentOnFace(candidate.face, projected, status) &&
                (status == 1 || status == 3))
            {
                sampleCovered = true;
                break;
            }
        }
        if (sampleCovered)
        {
            ++covered;
        }
    }

    if (coveredSampleCount != nullptr)
    {
        *coveredSampleCount = covered;
    }
    return static_cast<double>(covered) /
           static_cast<double>(baseSamples.size());
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

Features::CustomStringAttribute* CreateStringAttribute(
    Features::CustomAttributeCollection* attrs,
    const char* name,
    const std::string& value)
{
    std::vector<Features::CustomAttribute::Property> props;
    Features::CustomStringAttribute* attr =
        attrs->CreateCustomStringAttribute(name, props);
    attr->SetValue(NXString(value.c_str(), NXString::UTF8));
    return attr;
}

Features::CustomIntegerAttribute* CreateIntegerAttribute(
    Features::CustomAttributeCollection* attrs,
    const char* name,
    int value)
{
    std::vector<Features::CustomAttribute::Property> props;
    Features::CustomIntegerAttribute* attr =
        attrs->CreateCustomIntegerAttribute(name, props);
    attr->SetValue(value);
    return attr;
}

Features::CustomLogicalAttribute* CreateLogicalAttribute(
    Features::CustomAttributeCollection* attrs,
    const char* name,
    bool value)
{
    std::vector<Features::CustomAttribute::Property> props;
    Features::CustomLogicalAttribute* attr =
        attrs->CreateCustomLogicalAttribute(name, props);
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
        if ((featureMode == TwoPointSiBianUI::FeatureMode::NinetyLeft ||
             featureMode == TwoPointSiBianUI::FeatureMode::NinetyRight) &&
            expressionKey == "p42")
        {
            value = sheetThicknessValue;
        }
        else if ((featureMode == TwoPointSiBianUI::FeatureMode::NinetyLeft ||
                  featureMode == TwoPointSiBianUI::FeatureMode::NinetyRight) &&
                 expressionKey == "p43")
        {
            value = bendRadiusValue;
        }
        else if ((featureMode == TwoPointSiBianUI::FeatureMode::NinetyLeft ||
                  featureMode == TwoPointSiBianUI::FeatureMode::NinetyRight) &&
                 expressionKey == "p45")
        {
            value = clearanceValue;
        }
        else if (expressionKey == "p7")
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
        if (ContainsAny(expressionName, {"板厚"}) ||
            ContainsAny(expressionKey, {"banhou", "thickness", "sheet"}))
        {
            value = sheetThicknessValue;
        }
        else if (ContainsAny(expressionName, {"折弯r", "折弯R"}) ||
                 ContainsAny(expressionKey, {"bend", "radius"}) ||
                 expressionKey == "r")
        {
            value = bendRadiusValue;
        }
        else if (ContainsAny(expressionName, {"间隙"}) ||
                 ContainsAny(expressionKey, {"gap", "clearance"}))
        {
            value = clearanceValue;
        }
        else if (featureMode == TwoPointSiBianUI::FeatureMode::Chamfer ||
                 featureMode == TwoPointSiBianUI::FeatureMode::NinetyLeft ||
                 featureMode == TwoPointSiBianUI::FeatureMode::NinetyRight)
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
      activeSmartSelectionBlock_(nullptr),
      clearanceBlock_(nullptr),
      bendRadiusBlock_(nullptr),
      smartModeBlock_(nullptr),
      chamferEdgeToggleBlock_(nullptr),
      reverseCutButton_(nullptr),
      featureModeBlock_(nullptr),
      customFeatureManager_(nullptr),
      editedFeature_(nullptr),
      featureClass_(nullptr),
      editRollbackManager_(nullptr),
      editRollbackMark_(static_cast<Session::UndoMarkId>(0)),
      loadingEditedFeature_(false),
      editedTargetBodyTag_(NULL_TAG),
      editedBaseFaceTag_(NULL_TAG),
      editedStartEdgeTag_(NULL_TAG),
      editedEndEdgeTag_(NULL_TAG),
      hasEditedEndpointCache_(false),
      editedCachedP1_(),
      editedCachedP2_(),
      previewUndoMark_(static_cast<Session::UndoMarkId>(0)),
      previewUdfTag_(NULL_TAG),
      previewTargetBodyTag_(NULL_TAG),
      previewReferenceTags_(),
      hasSmartEndpointCache_(false),
      smartEndpointBodyTag_(NULL_TAG),
      smartEndpointFaceTag_(NULL_TAG),
      smartCachedP1_(),
      smartCachedP2_(),
      retainSmartEndpointCacheOnUndo_(false),
      hasPreview_(false),
      previewCommitted_(false),
      isUpdatingPreview_(false),
      reverseChamfer270Cut_(false),
      hasLastPreviewInputs_(false),
      lastPreviewInputs_(),
      hasResolvedPrimaryP2ForPersistence_(false),
      resolvedPrimaryP2ForPersistence_(),
      hasEditedBaselineInputs_(false),
      editedBaselineInputs_(),
      editedLivePreviewDirty_(false),
      buildingCustomFeature_(false),
      customFeatureConstructionRebuilt_(false)
{
    customFeatureManager_ = session_->CustomFeatureClassManager();
    editedFeature_ = customFeatureManager_->GetEditedCustomFeature();
    featureClass_ = customFeatureManager_->GetClassFromName(zhihui_twopoint_sibian::kFeatureClassName);
    gActiveTwoPointSiBianDialog = this;

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
    if (editRollbackManager_ != nullptr)
    {
        std::string rollbackError;
        static_cast<void>(FinishEditedFeatureRollback(true, rollbackError));
    }
    if (gActiveTwoPointSiBianDialog == this)
    {
        gActiveTwoPointSiBianDialog = nullptr;
    }
    delete dialog_;
}

NXOpen::BlockStyler::BlockDialog::DialogResponse TwoPointSiBianUI::Launch()
{
    return dialog_->LaunchInDialogMode(
        editedFeature_ != nullptr
            ? NXOpen::BlockStyler::BlockDialog::DialogModeEdit
            : NXOpen::BlockStyler::BlockDialog::DialogModeCreate);
}

void TwoPointSiBianUI::initialize_cb()
{
    startPointBlock_ = dynamic_cast<NXOpen::BlockStyler::SelectObject*>(dialog_->TopBlock()->FindBlock("selection0"));
    endPointBlock_ = dynamic_cast<NXOpen::BlockStyler::SelectObject*>(dialog_->TopBlock()->FindBlock("selection01"));
    activeSmartSelectionBlock_ = startPointBlock_;
    clearanceBlock_ = dynamic_cast<NXOpen::BlockStyler::StringBlock*>(dialog_->TopBlock()->FindBlock("string0"));
    bendRadiusBlock_ = dynamic_cast<NXOpen::BlockStyler::StringBlock*>(dialog_->TopBlock()->FindBlock("string01"));
    smartModeBlock_ = dynamic_cast<NXOpen::BlockStyler::Toggle*>(dialog_->TopBlock()->FindBlock("smartModeToggle"));
    chamferEdgeToggleBlock_ = dynamic_cast<NXOpen::BlockStyler::Toggle*>(dialog_->TopBlock()->FindBlock("gapOnlyToggle"));
    reverseCutButton_ = dynamic_cast<NXOpen::BlockStyler::Button*>(dialog_->TopBlock()->FindBlock("reverseCutButton"));
    featureModeBlock_ = dynamic_cast<NXOpen::BlockStyler::Enumeration*>(dialog_->TopBlock()->FindBlock("wrapCornerMode"));

    if (chamferEdgeToggleBlock_ != nullptr)
    {
        chamferEdgeToggleBlock_->SetValue(true);
        AppendDebugLog("chamfer-edge toggle initialized: enabled=true.");
    }
    else
    {
        AppendDebugLog("chamfer-edge toggle block missing; defaulting to enabled.");
    }

    if (featureModeBlock_ != nullptr)
    {
        std::vector<NXString> members;
        members.emplace_back("斜角", NXString::UTF8);
        members.emplace_back("直角左", NXString::UTF8);
        members.emplace_back("直角右", NXString::UTF8);
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

    if (editedFeature_ != nullptr)
    {
        loadingEditedFeature_ = true;
        const bool loaded = LoadEditedFeatureState();
        loadingEditedFeature_ = false;
        if (loaded)
        {
            // Do not roll the model back and create a second UDF/rip chain
            // merely to preview an existing CustomFeature.  NX owns the
            // registered construction features; Apply/OK edits those members
            // in place through InternalFeaturePreUpdateCallback.
            AppendDebugLog(
                "edit initialization loaded persisted inputs; the existing "
                "construction chain remains active and no real-feature preview "
                "was created.");
        }
    }
}

bool TwoPointSiBianUI::LoadEditedFeatureState()
{
    try
    {
        if (editedFeature_ == nullptr)
        {
            return false;
        }
        Features::CustomFeatureData* data = editedFeature_->FeatureData();
        if (data == nullptr)
        {
            return false;
        }

        editedCachedP1_ = Point3d(
            data->CustomDoubleAttributeByName(
                    zhihui_twopoint_sibian::kAttrStartX)
                ->Value(),
            data->CustomDoubleAttributeByName(
                    zhihui_twopoint_sibian::kAttrStartY)
                ->Value(),
            data->CustomDoubleAttributeByName(
                    zhihui_twopoint_sibian::kAttrStartZ)
                ->Value());
        editedCachedP2_ = Point3d(
            data->CustomDoubleAttributeByName(
                    zhihui_twopoint_sibian::kAttrEndX)
                ->Value(),
            data->CustomDoubleAttributeByName(
                    zhihui_twopoint_sibian::kAttrEndY)
                ->Value(),
            data->CustomDoubleAttributeByName(
                    zhihui_twopoint_sibian::kAttrEndZ)
                ->Value());
        hasEditedEndpointCache_ =
            Distance(editedCachedP1_, editedCachedP2_) > kPointTolerance;

        auto tagValue = [&](const char* name) -> tag_t
        {
            TaggedObject* value = data->CustomTagAttributeByName(name)->Value();
            return value != nullptr ? value->Tag() : NULL_TAG;
        };
        editedTargetBodyTag_ = tagValue(zhihui_twopoint_sibian::kAttrTargetBody);
        editedBaseFaceTag_ = tagValue(zhihui_twopoint_sibian::kAttrBaseFace);
        editedStartEdgeTag_ = tagValue(zhihui_twopoint_sibian::kAttrStartEdge);
        editedEndEdgeTag_ = tagValue(zhihui_twopoint_sibian::kAttrEndEdge);

        if (clearanceBlock_ != nullptr)
        {
            clearanceBlock_->SetValue(
                data->CustomStringAttributeByName(
                        zhihui_twopoint_sibian::kAttrClearance)
                    ->Value());
        }
        if (bendRadiusBlock_ != nullptr)
        {
            bendRadiusBlock_->SetValue(
                data->CustomStringAttributeByName(
                        zhihui_twopoint_sibian::kAttrBendRadius)
                    ->Value());
        }
        if (chamferEdgeToggleBlock_ != nullptr)
        {
            chamferEdgeToggleBlock_->SetValue(
                data->CustomLogicalAttributeByName(
                        zhihui_twopoint_sibian::kAttrChamferEdgeMode)
                    ->Value());
        }
        reverseChamfer270Cut_ =
            data->CustomLogicalAttributeByName(
                    zhihui_twopoint_sibian::kAttrReverseCut)
                ->Value();

        const int mode = data->CustomIntegerAttributeByName(
                                  zhihui_twopoint_sibian::kAttrFeatureMode)
                             ->Value();
        if (mode == static_cast<int>(FeatureMode::NinetyLeft) ||
            mode == static_cast<int>(FeatureMode::NinetyRight))
        {
            Point3d directedPrimaryP2;
            tag_t directedPrimaryP2Edge = NULL_TAG;
            if (ResolvePrimaryUdfDirectedP2(editedFeature_,
                                            directedPrimaryP2,
                                            directedPrimaryP2Edge) &&
                Distance(directedPrimaryP2, editedCachedP2_) >
                    kPointTolerance)
            {
                const Point3d persistedP2 = editedCachedP2_;
                editedCachedP2_ = directedPrimaryP2;
                hasEditedEndpointCache_ =
                    Distance(editedCachedP1_, editedCachedP2_) >
                    kPointTolerance;
                data->CustomDoubleAttributeByName(
                        zhihui_twopoint_sibian::kAttrEndX)
                    ->SetValue(editedCachedP2_.X);
                data->CustomDoubleAttributeByName(
                        zhihui_twopoint_sibian::kAttrEndY)
                    ->SetValue(editedCachedP2_.Y);
                data->CustomDoubleAttributeByName(
                        zhihui_twopoint_sibian::kAttrEndZ)
                    ->SetValue(editedCachedP2_.Z);
                AppendDebugLog(
                    "LoadEditedFeatureState repaired persisted right-angle P2 from the primary UDF directed edge: edge=" +
                    std::to_string(directedPrimaryP2Edge) +
                    ", savedP2=" + FormatPoint(persistedP2) +
                    ", actualP2=" + FormatPoint(editedCachedP2_));
            }
        }
        if (featureModeBlock_ != nullptr)
        {
            const char* modeText =
                mode == static_cast<int>(FeatureMode::NinetyLeft)
                    ? "直角左"
                    : (mode == static_cast<int>(FeatureMode::NinetyRight)
                           ? "直角右"
                           : "斜角");
            featureModeBlock_->SetValueAsString(
                NXString(modeText, NXString::UTF8));
        }

        lastPreviewInputs_ = InferredInputs();
        lastPreviewInputs_.startPoint = editedCachedP1_;
        lastPreviewInputs_.endPoint = editedCachedP2_;
        auto safeObject = [](tag_t objectTag) -> TaggedObject*
        {
            if (objectTag == NULL_TAG ||
                UF_OBJ_ask_status(objectTag) != UF_OBJ_ALIVE)
            {
                return nullptr;
            }
            try
            {
                return NXObjectManager::Get(objectTag);
            }
            catch (...)
            {
                return nullptr;
            }
        };
        lastPreviewInputs_.targetBody =
            dynamic_cast<Body*>(safeObject(editedTargetBodyTag_));
        lastPreviewInputs_.baseFace =
            dynamic_cast<Face*>(safeObject(editedBaseFaceTag_));
        lastPreviewInputs_.startEdge =
            dynamic_cast<Edge*>(safeObject(editedStartEdgeTag_));
        lastPreviewInputs_.endEdge =
            dynamic_cast<Edge*>(safeObject(editedEndEdgeTag_));
        lastPreviewInputs_.startObject = lastPreviewInputs_.startEdge;
        lastPreviewInputs_.endObject = lastPreviewInputs_.endEdge;
        lastPreviewInputs_.thickness =
            data->CustomDoubleAttributeByName(
                    zhihui_twopoint_sibian::kAttrThickness)
                ->Value();
        lastPreviewInputs_.spanLength =
            data->CustomDoubleAttributeByName(
                    zhihui_twopoint_sibian::kAttrSpanLength)
                ->Value();
        const NXString clearanceValue =
            data->CustomStringAttributeByName(
                    zhihui_twopoint_sibian::kAttrClearance)
                ->Value();
        const NXString bendRadiusValue =
            data->CustomStringAttributeByName(
                    zhihui_twopoint_sibian::kAttrBendRadius)
                ->Value();
        lastPreviewInputs_.clearanceValue =
            clearanceValue.GetUTF8Text() != nullptr
                ? clearanceValue.GetUTF8Text()
                : "0.2";
        lastPreviewInputs_.bendRadiusValue =
            bendRadiusValue.GetUTF8Text() != nullptr
                ? bendRadiusValue.GetUTF8Text()
                : "0.2";
        lastPreviewInputs_.featureMode = static_cast<FeatureMode>(mode);
        lastPreviewInputs_.smartMode =
            data->CustomLogicalAttributeByName(
                    zhihui_twopoint_sibian::kAttrSmartMode)
                ->Value();
        lastPreviewInputs_.chamferEdgeMode =
            data->CustomLogicalAttributeByName(
                    zhihui_twopoint_sibian::kAttrChamferEdgeMode)
                ->Value();
        lastPreviewInputs_.reverseChamfer270Cut = reverseChamfer270Cut_;
        lastPreviewInputs_.inferredFromSingleClick =
            data->CustomLogicalAttributeByName(
                    zhihui_twopoint_sibian::kAttrInferredFromSingleClick)
                ->Value();
        lastPreviewInputs_.selectionClickPoint = Point3d(
            data->CustomDoubleAttributeByName(
                    zhihui_twopoint_sibian::kAttrClickX)
                ->Value(),
            data->CustomDoubleAttributeByName(
                    zhihui_twopoint_sibian::kAttrClickY)
                ->Value(),
            data->CustomDoubleAttributeByName(
                    zhihui_twopoint_sibian::kAttrClickZ)
                ->Value());
        // Construction features can replace the original face/edge topology.
        // Editing numeric attributes does not need those post-update wrappers;
        // the existing construction members already own their references.
        hasLastPreviewInputs_ = hasEditedEndpointCache_;
        editedBaselineInputs_ = lastPreviewInputs_;
        hasEditedBaselineInputs_ = hasLastPreviewInputs_;
        editedLivePreviewDirty_ = false;

        // Editing is driven by the persisted resolved P1/P2. The point
        // controls stay empty, but parameter/mode changes rebuild the same
        // associative target when Apply or OK is pressed.
        if (smartModeBlock_ != nullptr)
        {
            smartModeBlock_->SetValue(false);
        }
        ConfigureInputMode(false, false);
        if (smartModeBlock_ != nullptr)
        {
            smartModeBlock_->SetEnable(false);
        }
        if (featureModeBlock_ != nullptr)
        {
            featureModeBlock_->SetEnable(false);
        }
        if (chamferEdgeToggleBlock_ != nullptr)
        {
            chamferEdgeToggleBlock_->SetEnable(false);
        }
        if (reverseCutButton_ != nullptr)
        {
            reverseCutButton_->SetEnable(false);
        }
        AppendDebugLog("loaded editable CustomFeature data: feature=" +
                       std::to_string(editedFeature_->Tag()) +
                       ", targetBody=" +
                       std::to_string(editedTargetBodyTag_) +
                       ", P1=" + FormatPoint(editedCachedP1_) +
                       ", P2=" + FormatPoint(editedCachedP2_));
        return hasEditedEndpointCache_ && editedTargetBodyTag_ != NULL_TAG;
    }
    catch (const NXException& ex)
    {
        AppendDebugLog("failed to load edited CustomFeature data: " +
                       NxExceptionText(ex));
    }
    catch (...)
    {
        AppendDebugLog("failed to load edited CustomFeature data: unknown exception.");
    }
    return false;
}

bool TwoPointSiBianUI::ReadCurrentEditedInputs(InferredInputs& inputs) const
{
    if (editedFeature_ == nullptr || !hasLastPreviewInputs_)
    {
        return false;
    }

    inputs = lastPreviewInputs_;
    inputs.clearanceValue =
        ReadStringBlockValue(clearanceBlock_, "string0", "0.2");
    inputs.bendRadiusValue =
        ReadStringBlockValue(bendRadiusBlock_, "string01", "0.2");
    // Double-click editing deliberately exposes only clearance and bend R.
    // Preserve every topology-driving value recorded at creation time.
    return true;
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
        if (mode == "直角左")
        {
            return FeatureMode::NinetyLeft;
        }
        if (mode == "直角右")
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

bool TwoPointSiBianUI::IsSmartModeEnabled() const
{
    if (smartModeBlock_ == nullptr)
    {
        return false;
    }
    try
    {
        return smartModeBlock_->Value();
    }
    catch (const NXException& ex)
    {
        AppendDebugLog("failed to read smart mode toggle: " + UfMessage(ex.ErrorCode()));
        return false;
    }
}

void TwoPointSiBianUI::dialogShown_cb()
{
    try
    {
        ConfigureInputMode(IsSmartModeEnabled(), false);
        if (editedFeature_ == nullptr && startPointBlock_ != nullptr)
        {
            startPointBlock_->Focus();
        }
    }
    catch (const NXException& ex)
    {
        AppendDebugLog("dialogShown input setup NXException: " + UfMessage(ex.ErrorCode()));
    }
    catch (...)
    {
        AppendDebugLog("dialogShown input setup unknown exception.");
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
    if (IsSmartModeEnabled())
    {
        return hasStart || hasEnd;
    }
    if (!hasStart && !hasEnd && hasEditedEndpointCache_)
    {
        return Distance(editedCachedP1_, editedCachedP2_) > kPointTolerance;
    }
    if (hasStart && hasEnd && Distance(startPoint, endPoint) > kPointTolerance)
    {
        return true;
    }
    return false;
}

int TwoPointSiBianUI::update_cb(NXOpen::BlockStyler::UIBlock* block)
{
    if (loadingEditedFeature_ || isUpdatingPreview_)
    {
        return 0;
    }
    if (block == startPointBlock_ || block == endPointBlock_)
    {
        activeSmartSelectionBlock_ = static_cast<NXOpen::BlockStyler::SelectObject*>(block);
        // Selection blocks retain their objects for input, but the selected
        // face itself should not remain painted in the graphics window.
        UnhighlightSelectionObjects();

        // In manual mode, completing the first endpoint must immediately
        // transfer input to the second endpoint control.  Do this explicitly
        // instead of relying only on Block Styler automatic progression.
        if (block == startPointBlock_ && !IsSmartModeEnabled() && endPointBlock_ != nullptr)
        {
            TaggedObject* selectedObject = nullptr;
            Point3d selectedPoint;
            if (ReadSelectedPoint(startPointBlock_, selectedObject, selectedPoint))
            {
                try
                {
                    endPointBlock_->Focus();
                    AppendDebugLog("manual first endpoint accepted; focus moved to second endpoint.");
                }
                catch (const NXException& ex)
                {
                    AppendDebugLog("failed to focus second endpoint: " + UfMessage(ex.ErrorCode()));
                }
                catch (...)
                {
                    AppendDebugLog("failed to focus second endpoint: unknown exception.");
                }
            }
        }
    }
    if (block == smartModeBlock_)
    {
        const bool smartMode = IsSmartModeEnabled();
        hasSmartEndpointCache_ = false;
        smartEndpointBodyTag_ = NULL_TAG;
        smartEndpointFaceTag_ = NULL_TAG;
        retainSmartEndpointCacheOnUndo_ = false;
        try
        {
            ConfigureInputMode(smartMode, true);
        }
        catch (const NXException& ex)
        {
            AppendDebugLog("update ConfigureInputMode NXException: " + UfMessage(ex.ErrorCode()));
            return 0;
        }
        catch (...)
        {
            AppendDebugLog("update ConfigureInputMode unknown exception.");
            return 0;
        }
        AppendDebugLog(std::string("update_cb smart mode=") +
                       (smartMode ? "true; manual enum disabled." : "false; manual enum enabled."));
    }
    if (block == featureModeBlock_)
    {
        AppendDebugLog(previewCommitted_
                           ? "update_cb feature mode changed after Apply; keeping the applied chain and starting a new preview."
                           : "update_cb feature mode changed; replacing the complete uncommitted preview chain.");
    }
    if (block == reverseCutButton_)
    {
        reverseChamfer270Cut_ = !reverseChamfer270Cut_;
        AppendDebugLog(std::string("cut-direction reverse button clicked: chamfer >180 P2Q+2T reverse=") +
                       (reverseChamfer270Cut_ ? "true" : "false") +
                       "; all other branches remain unchanged.");
    }
    if (editedFeature_ != nullptr)
    {
        const bool affectsGeometry =
            block == clearanceBlock_ || block == bendRadiusBlock_;
        if (affectsGeometry)
        {
            InferredInputs previewInputs;
            double previewClearance = 0.0;
            double previewRadius = 0.0;
            if (!ReadCurrentEditedInputs(previewInputs) ||
                !TryParseExpressionNumber(previewInputs.clearanceValue,
                                          previewClearance) ||
                !TryParseExpressionNumber(previewInputs.bendRadiusValue,
                                          previewRadius))
            {
                AppendDebugLog(
                    "update_cb edit mode: waiting for valid numeric clearance and bend-R values.");
                return 0;
            }

            const InferredInputs previousInputs = lastPreviewInputs_;
            std::string previewError;
            isUpdatingPreview_ = true;
            const bool previewUpdated =
                CommitCustomFeature(previewInputs, previewError);
            isUpdatingPreview_ = false;
            if (previewUpdated)
            {
                editedLivePreviewDirty_ = true;
                AppendDebugLog(
                    "update_cb edit mode: live in-place CustomFeature preview updated.");
            }
            else
            {
                std::string restoreError;
                isUpdatingPreview_ = true;
                static_cast<void>(CommitCustomFeature(previousInputs,
                                                      restoreError));
                isUpdatingPreview_ = false;
                AppendDebugLog(
                    "update_cb edit mode: live preview failed: " +
                    previewError +
                    (restoreError.empty()
                         ? std::string()
                         : "; restore warning: " + restoreError));
                ShowError(previewError);
            }
        }
        else
        {
            AppendDebugLog(
                "update_cb edit mode: non-geometric control changed; no rebuild required.");
        }
        UnhighlightSelectionObjects();
        return 0;
    }
    if (enable_ok_cb())
    {
        if (!isUpdatingPreview_)
        {
            isUpdatingPreview_ = true;
            const bool ok = CreatePreview();
            isUpdatingPreview_ = false;
            UnhighlightSelectionObjects();
            if (!ok)
            {
                AppendDebugLog("update_cb preview failed; keeping dialog input accepted.");
            }
        }
    }
    else
    {
        if (!UndoPreview())
        {
            ShowError("The previous preview could not be restored completely. "
                      "The new preview was stopped to protect the model.");
            return false;
        }
    }
    return 0;
}

int TwoPointSiBianUI::apply_cb()
{
    BeginDebugLogSection();
    AppendDebugLog("apply_cb entered; committing one opaque CustomFeature node");

    try
    {
        InferredInputs inputs;
        if (editedFeature_ != nullptr)
        {
            if (!ReadCurrentEditedInputs(inputs))
            {
                throw std::runtime_error(
                    "The saved 2P_SiBian endpoints could not be restored for editing.");
            }
            if (inputs.featureMode != lastPreviewInputs_.featureMode ||
                inputs.chamferEdgeMode != lastPreviewInputs_.chamferEdgeMode ||
                inputs.reverseChamfer270Cut !=
                    lastPreviewInputs_.reverseChamfer270Cut)
            {
                throw std::runtime_error(
                    "The current editable feature supports in-place changes to clearance and bend radius. "
                    "Changing chamfer/right-angle branch, chamfer-edge creation, or cut direction changes "
                    "the construction topology and requires a separate branch-rebuild implementation.");
            }
        }
        else
        {
            if (!hasPreview_ && !CreatePreview())
            {
                throw std::runtime_error("Preview creation failed. See " +
                                         DebugLogPath());
            }
            if (!hasLastPreviewInputs_)
            {
                throw std::runtime_error(
                    "The resolved 2P_SiBian inputs were not retained from preview.");
            }
            inputs = lastPreviewInputs_;
            if (!UndoPreview())
            {
                throw std::runtime_error(
                    "The temporary preview could not be restored before creating the CustomFeature.");
            }
        }

        // In edit mode a numeric block change has already committed the live
        // in-place preview from update_cb.  Committing the identical data once
        // more from Apply/OK starts a second CustomFeature update before NX has
        // finished displaying the first one.  On the 90-left two-offset chain
        // that duplicate pass stops at an intermediate subtraction and leaves
        // the node failed until the user edits it again.  Accept the proven
        // live preview directly; only commit here when no live update ran.
        const bool acceptExistingLivePreview =
            editedFeature_ != nullptr && editedLivePreviewDirty_;
        std::string commitError;
        if (!acceptExistingLivePreview &&
            !CommitCustomFeature(inputs, commitError))
        {
            throw std::runtime_error(commitError);
        }
        if (editedFeature_ != nullptr)
        {
            editedBaselineInputs_ = inputs;
            hasEditedBaselineInputs_ = true;
            editedLivePreviewDirty_ = false;
            AppendDebugLog(
                acceptExistingLivePreview
                    ? "apply_cb edit mode: accepted the already committed live preview without a duplicate CustomFeature update."
                    : "apply_cb edit mode: committed current values and accepted them as the new cancel baseline.");
        }
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
    const int result = apply_cb();
    if (result == 0)
    {
        FinalizeCommittedPreview();
    }
    return result;
}

int TwoPointSiBianUI::cancel_cb()
{
    if (editedFeature_ != nullptr)
    {
        std::string restoreError;
        if (!RestoreEditedFeatureBaseline(restoreError))
        {
            AppendDebugLog("cancel_cb edit restore failed: " + restoreError);
            ShowError(restoreError);
            return 1;
        }
        AppendDebugLog(
            "cancel_cb edit mode: restored the last applied CustomFeature values.");
        return 0;
    }
    if (previewCommitted_)
    {
        AppendDebugLog("cancel_cb entered after Apply; keeping the committed feature and finalizing its undo mark.");
        FinalizeCommittedPreview();
    }
    else
    {
        AppendDebugLog("cancel_cb entered; undoing preview");
        UndoPreview();
    }
    return 0;
}

int TwoPointSiBianUI::close_cb()
{
    if (editedFeature_ != nullptr)
    {
        std::string restoreError;
        if (!RestoreEditedFeatureBaseline(restoreError))
        {
            AppendDebugLog("close_cb edit restore failed: " + restoreError);
            ShowError(restoreError);
            return 1;
        }
        AppendDebugLog(
            "close_cb edit mode: restored the last applied CustomFeature values.");
        return 0;
    }
    if (previewCommitted_)
    {
        AppendDebugLog("close_cb entered after Apply; keeping the committed feature and finalizing its undo mark.");
        FinalizeCommittedPreview();
    }
    else
    {
        AppendDebugLog("close_cb entered; undoing uncommitted preview");
        UndoPreview();
    }
    return 0;
}

bool TwoPointSiBianUI::CreatePreview(const InferredInputs* forcedInputs)
{
    AppendDebugLog(std::string("CreatePreview entered, source=") +
                   (forcedInputs != nullptr ? "CustomFeature data" : "dialog"));

    // A face selected while the old preview exists may belong to the preview's
    // boolean topology.  Save only its stable body tag and cursor position
    // before rollback, then resolve the face again on the restored body.
    tag_t rollbackSafeBodyTag = NULL_TAG;
    Point3d rollbackSafeClickPoint;
    bool hasRollbackSafeSingleClick = false;
    if (forcedInputs == nullptr && IsSmartModeEnabled())
    {
        TaggedObject* clickedObject = nullptr;
        BlockStyler::SelectObject* clickedBlock = activeSmartSelectionBlock_;
        if (clickedBlock == nullptr)
        {
            clickedBlock = endPointBlock_ != nullptr ? endPointBlock_ : startPointBlock_;
        }
        if (ReadSelectedPoint(clickedBlock, clickedObject, rollbackSafeClickPoint))
        {
            Body* clickedBody = FindBody(clickedObject);
            if (clickedBody != nullptr)
            {
                rollbackSafeBodyTag = clickedBody->Tag();
                hasRollbackSafeSingleClick = true;
                AppendDebugLog("CreatePreview captured rollback-safe smart click: body=" +
                               std::to_string(rollbackSafeBodyTag) +
                               ", point=" + FormatPoint(rollbackSafeClickPoint));
            }
        }
    }
    // Apply is a true commit boundary.  A later face selection starts another
    // operation and must not remove the chain that the user already applied.
    // Only an uncommitted live preview is replaceable (for example while
    // switching Left/Right/Chamfer before pressing Apply).
    if (previewCommitted_)
    {
        AppendDebugLog("CreatePreview preserving the previously applied feature chain.");
        FinalizeCommittedPreview();
    }
    else
    {
        UndoPreview();
    }

    // NX can discard our undo mark while a UDF is being instantiated.  Keep a
    // feature snapshot as an independent rollback boundary so enumeration
    // changes also remove UDF-owned extrudes, rips, offsets and booleans.
    previewBaselineFeatureTags_ = CurrentWorkPartFeatureTags();
    previewCreatedFeatureTags_.clear();
    AppendDebugLog("CreatePreview captured feature baseline count=" +
                   std::to_string(previewBaselineFeatureTags_.size()));

    previewUndoMark_ = session_->SetUndoMark(Session::MarkVisibilityVisible, "2P_SiBian Preview");

    TaggedObject* rollbackSafeClickObject = nullptr;
    if (hasRollbackSafeSingleClick)
    {
        try
        {
            rollbackSafeClickObject = dynamic_cast<TaggedObject*>(
                NXObjectManager::Get(rollbackSafeBodyTag));
        }
        catch (...)
        {
            rollbackSafeClickObject = nullptr;
        }
    }

    InferredInputs inputs;
    if (forcedInputs != nullptr)
    {
        inputs = *forcedInputs;
        // Undoing the previous preview or rolling the model back can replace
        // edge wrapper objects even when the body, face and endpoint
        // coordinates remain stable.  Resolve every endpoint edge again on
        // the now-current original topology before any branch examines it.
        if (!CompleteInputsForEndpoints(inputs))
        {
            AppendDebugLog(
                "CreatePreview could not refresh forced P1/P2 inputs on rollback topology.");
            UndoPreview();
            return false;
        }
    }
    else if (!ReadInputs(inputs,
                         rollbackSafeClickObject,
                         hasRollbackSafeSingleClick ? &rollbackSafeClickPoint : nullptr))
    {
        AppendDebugLog("CreatePreview ReadInputs failed.");
        UndoPreview();
        return false;
    }
    // Preserve the endpoints resolved on the unmodified body.  Later rips and
    // offsets may move/split P2, but a replacement preview must restart from
    // this original pair instead of a face exposed by the preview topology.
    const InferredInputs originalPreviewInputs = inputs;
    lastPreviewInputs_ = originalPreviewInputs;
    hasLastPreviewInputs_ = true;
    hasResolvedPrimaryP2ForPersistence_ = false;
    Edge* referenceCornerEdge = nullptr;
    if (inputs.chamferEdgeMode)
    {
        referenceCornerEdge = FindReferenceCornerEdge(inputs);
        AppendDebugLog("CreatePreview reference-project chamfer edge prepared: edge=" +
                       std::to_string(referenceCornerEdge != nullptr
                                          ? referenceCornerEdge->Tag()
                                          : NULL_TAG));
    }

    std::vector<tag_t> allToolBodyTags;
    std::vector<tag_t> allReferenceTags;
    std::vector<std::pair<tag_t, double>> deferredRightAngleRipPlans;
    std::vector<InferredInputs> deferredSecondUdfInputsList;
    std::vector<tag_t> rightAngleOffsetFeatureTags;
    bool hasRightAngle90SecondFeaturePath = false;
    bool primaryUdfCreatedBeforeRip = false;
    bool earlyPrimaryHasPendingSecondTools = false;
    tag_t earlyPrimarySubtractTag = NULL_TAG;
    if (inputs.inferredFromSingleClick)
    {
        constexpr int kMaximumContinuationCount = 16;
        std::vector<std::pair<Point3d, Point3d>> processedPointPairs;
        InferredInputs iterationInputs = inputs;
        bool allowContinuationInputs = false;
        bool anyRipCreated = false;
        for (int iteration = 0; iteration < kMaximumContinuationCount; ++iteration)
        {
            processedPointPairs.push_back(
                std::make_pair(iterationInputs.startPoint, iterationInputs.endPoint));
            bool ripCreated = false;
            tag_t secondUdfTag = NULL_TAG;
            std::vector<tag_t> secondToolBodyTags;
            std::vector<tag_t> secondReferenceTags;
            bool continuationCreated = false;
            InferredInputs continuationInputs;
            bool deferredSecondUdfRequested = false;
            InferredInputs deferredSecondUdfInputs;
            tag_t deferredRightAngleRipTag = NULL_TAG;
            double deferredRightAngleRipAngle = 0.0;
            std::vector<tag_t> createdRightAngleOffsetTags;
            bool createdRightAngle90SecondFeaturePath = false;
            bool iterationPrimaryUdfCreatedBeforeRip = false;
            tag_t iterationPrimarySubtractTag = NULL_TAG;
            std::vector<tag_t> iterationPrimaryReferenceTags;
            std::string ripError;
            AppendDebugLog("CreatePreview continuation iteration=" + std::to_string(iteration + 1) +
                           ", start=" + FormatPoint(iterationInputs.startPoint) +
                           ", end=" + FormatPoint(iterationInputs.endPoint));
            if (!TryCreateSecondPointRip(iterationInputs,
                                         allowContinuationInputs,
                                         ripCreated,
                                         secondUdfTag,
                                         secondToolBodyTags,
                                         secondReferenceTags,
                                         continuationCreated,
                                         continuationInputs,
                                         deferredSecondUdfRequested,
                                         deferredSecondUdfInputs,
                                         deferredRightAngleRipTag,
                                         deferredRightAngleRipAngle,
                                         createdRightAngleOffsetTags,
                                         createdRightAngle90SecondFeaturePath,
                                         iterationPrimaryUdfCreatedBeforeRip,
                                         iterationPrimarySubtractTag,
                                         iterationPrimaryReferenceTags,
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
            anyRipCreated = anyRipCreated || ripCreated;
            rightAngleOffsetFeatureTags.insert(rightAngleOffsetFeatureTags.end(),
                                               createdRightAngleOffsetTags.begin(),
                                               createdRightAngleOffsetTags.end());
            hasRightAngle90SecondFeaturePath =
                hasRightAngle90SecondFeaturePath ||
                createdRightAngle90SecondFeaturePath;
            if (iterationPrimaryUdfCreatedBeforeRip)
            {
                primaryUdfCreatedBeforeRip = true;
                earlyPrimaryHasPendingSecondTools =
                    earlyPrimaryHasPendingSecondTools ||
                    !secondToolBodyTags.empty();
                earlyPrimarySubtractTag = iterationPrimarySubtractTag;
                allReferenceTags.insert(allReferenceTags.end(),
                                        iterationPrimaryReferenceTags.begin(),
                                        iterationPrimaryReferenceTags.end());
            }
            if (deferredSecondUdfRequested)
            {
                bool alreadyDeferred = false;
                for (const InferredInputs& existingDeferred : deferredSecondUdfInputsList)
                {
                    const bool sameDirection =
                        Distance(existingDeferred.startPoint,
                                 deferredSecondUdfInputs.startPoint) <= 0.01 &&
                        Distance(existingDeferred.endPoint,
                                 deferredSecondUdfInputs.endPoint) <= 0.01;
                    const bool reverseDirection =
                        Distance(existingDeferred.startPoint,
                                 deferredSecondUdfInputs.endPoint) <= 0.01 &&
                        Distance(existingDeferred.endPoint,
                                 deferredSecondUdfInputs.startPoint) <= 0.01;
                    if (sameDirection || reverseDirection)
                    {
                        alreadyDeferred = true;
                        break;
                    }
                }
                if (!alreadyDeferred)
                {
                    deferredSecondUdfInputsList.push_back(deferredSecondUdfInputs);
                    AppendDebugLog("CreatePreview queued second UDF until continuation edge search completes"
                                   ", start=" + FormatPoint(deferredSecondUdfInputs.startPoint) +
                                   ", end=" + FormatPoint(deferredSecondUdfInputs.endPoint));
                }
            }
            if (deferredRightAngleRipTag != NULL_TAG)
            {
                const auto existingPlan = std::find_if(
                    deferredRightAngleRipPlans.begin(),
                    deferredRightAngleRipPlans.end(),
                    [deferredRightAngleRipTag](const std::pair<tag_t, double>& plan)
                    {
                        return plan.first == deferredRightAngleRipTag;
                    });
                if (existingPlan == deferredRightAngleRipPlans.end())
                {
                    deferredRightAngleRipPlans.emplace_back(deferredRightAngleRipTag,
                                                            deferredRightAngleRipAngle);
                }
            }
            if (!continuationCreated)
            {
                AppendDebugLog(secondUdfTag != NULL_TAG
                                   ? "CreatePreview continuation stopped: the second UDF endpoint has a thickness-length edge."
                                   : "CreatePreview continuation stopped: no further second UDF was created.");
                break;
            }

            bool alreadyProcessed = false;
            for (const auto& processedPair : processedPointPairs)
            {
                const bool sameDirection =
                    Distance(processedPair.first, continuationInputs.startPoint) <= 0.01 &&
                    Distance(processedPair.second, continuationInputs.endPoint) <= 0.01;
                const bool reverseDirection =
                    Distance(processedPair.first, continuationInputs.endPoint) <= 0.01 &&
                    Distance(processedPair.second, continuationInputs.startPoint) <= 0.01;
                if (sameDirection || reverseDirection)
                {
                    alreadyProcessed = true;
                    break;
                }
            }
            if (alreadyProcessed)
            {
                AppendDebugLog("CreatePreview continuation stopped: the next UDF point pair was already processed"
                               ", start=" + FormatPoint(continuationInputs.startPoint) +
                               ", end=" + FormatPoint(continuationInputs.endPoint));
                break;
            }
            iterationInputs = continuationInputs;
            allowContinuationInputs = true;
            if (iteration == kMaximumContinuationCount - 1)
            {
                AppendDebugLog("CreatePreview continuation stopped at the safety limit=" +
                               std::to_string(kMaximumContinuationCount));
            }
        }

        if (anyRipCreated && !primaryUdfCreatedBeforeRip)
        {
            AppendDebugLog("CreatePreview continuation rips committed; refreshing the original endpoints without recalculating sheet thickness.");
            const double lockedSheetThickness = inputs.thickness;
            InferredInputs refreshedInputs;
            refreshedInputs.thickness = lockedSheetThickness;
            const bool refreshed = inputs.smartMode && inputs.inferredFromSingleClick
                                       ? RefreshSmartInputsAfterRips(inputs, refreshedInputs)
                                       : ReadInputs(refreshedInputs);
            if (!refreshed)
            {
                UndoPreview();
                ShowError("The original second endpoint could not be recalculated after creating the sheet-metal rips.");
                return false;
            }
            AppendDebugLog("CreatePreview reused the one-time sheet thickness=" +
                           FormatExpressionNumber(lockedSheetThickness) +
                           " while refreshing endpoints.");
            inputs = refreshedInputs;
        }
    }

    std::string errorMessage;
    for (const InferredInputs& deferredSecondInputs : deferredSecondUdfInputsList)
    {
        tag_t deferredSecondUdfTag = NULL_TAG;
        std::vector<tag_t> deferredSecondReferenceTags;
        std::vector<tag_t> deferredSecondToolBodyTags;
        if (!CreateUserDefinedFeature(deferredSecondInputs,
                                      errorMessage,
                                      &deferredSecondUdfTag,
                                      &deferredSecondReferenceTags,
                                      &deferredSecondToolBodyTags))
        {
            UndoPreview();
            ShowError(errorMessage);
            return false;
        }
        allToolBodyTags.insert(allToolBodyTags.end(),
                               deferredSecondToolBodyTags.begin(),
                               deferredSecondToolBodyTags.end());
        allReferenceTags.insert(allReferenceTags.end(),
                                deferredSecondReferenceTags.begin(),
                                deferredSecondReferenceTags.end());
        AppendDebugLog("CreatePreview created deferred second UDF after edge search and before primary UDF/face offsets"
                       ", tag=" + std::to_string(deferredSecondUdfTag) +
                       ", start=" + FormatPoint(deferredSecondInputs.startPoint) +
                       ", end=" + FormatPoint(deferredSecondInputs.endPoint));
    }

    if (hasRightAngle90SecondFeaturePath)
    {
        InferredInputs constrainedInputs;
        if (!ConstrainRightAnglePrimaryP2ToOffsetSharedEdges(originalPreviewInputs,
                                                             inputs,
                                                             rightAngleOffsetFeatureTags,
                                                             constrainedInputs))
        {
            UndoPreview();
            ShowError("The right-angle primary feature P2 could not be resolved from endpoints shared by the plane and offset faces.");
            return false;
        }
        inputs = constrainedInputs;
        // The editable node must persist the P2 actually fed to the primary
        // right-angle UDF, not the original click-side endpoint captured
        // before the rip offsets.  Otherwise edit mode reopens with the old
        // point and the primary tool moves to a different end of the offset
        // edge when clearance or bend R changes.
        hasResolvedPrimaryP2ForPersistence_ = true;
        resolvedPrimaryP2ForPersistence_ = inputs.endPoint;
        AppendDebugLog(
            "CreatePreview captured constrained primary P2 for persistence: P2=" +
            FormatPoint(inputs.endPoint));
    }

    tag_t firstUdfTag = NULL_TAG;
    std::vector<tag_t> firstReferenceTags;
    std::vector<tag_t> firstToolBodyTags;
    tag_t finalSubtractTag = earlyPrimarySubtractTag;
    if (!primaryUdfCreatedBeforeRip)
    {
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

        if (!SubtractToolBodies(inputs.targetBody,
                                allToolBodyTags,
                                finalSubtractTag,
                                errorMessage))
        {
            UndoPreview();
            ShowError(errorMessage);
            return false;
        }
    }
    else
    {
        AppendDebugLog("CreatePreview retained the early primary-UDF subtraction: subtract=" +
                       std::to_string(finalSubtractTag));
        if (earlyPrimaryHasPendingSecondTools && !allToolBodyTags.empty())
        {
            tag_t secondFeatureSubtractTag = NULL_TAG;
            if (!SubtractToolBodies(inputs.targetBody,
                                    allToolBodyTags,
                                    secondFeatureSubtractTag,
                                    errorMessage))
            {
                UndoPreview();
                ShowError(errorMessage);
                return false;
            }
            finalSubtractTag = secondFeatureSubtractTag;
            AppendDebugLog(
                "CreatePreview subtracted the pending Q-first-equal second-UDF tool bodies after the early primary subtraction: toolBodyCount=" +
                std::to_string(allToolBodyTags.size()) +
                ", subtract=" + std::to_string(finalSubtractTag));
        }
    }

    tag_t finalResultTag = finalSubtractTag;
    for (const std::pair<tag_t, double>& deferredRightAngleRipPlan : deferredRightAngleRipPlans)
    {
        const tag_t deferredRightAngleRipTag = deferredRightAngleRipPlan.first;
        Features::Feature* ripFeature = dynamic_cast<Features::Feature*>(
            NXObjectManager::Get(deferredRightAngleRipTag));
        tag_t firstOffsetTag = NULL_TAG;
        tag_t secondOffsetTag = NULL_TAG;
        if (!OffsetRightAngleRipFeature(inputs,
                                        ripFeature,
                                        deferredRightAngleRipPlan.second,
                                        firstOffsetTag,
                                        secondOffsetTag,
                                        errorMessage))
        {
            UndoPreview();
            ShowError(errorMessage);
            return false;
        }
        finalResultTag = secondOffsetTag;
        AppendDebugLog("CreatePreview applied deferred 90/270-degree offsets after final subtraction: rip=" +
                       std::to_string(deferredRightAngleRipTag) +
                       ", subtract=" + std::to_string(finalSubtractTag) +
                       ", firstOffset=" + std::to_string(firstOffsetTag) +
                       ", secondOffset=" + std::to_string(secondOffsetTag));
    }

    if (inputs.chamferEdgeMode && referenceCornerEdge != nullptr)
    {
        InferredInputs cornerCutInputs = originalPreviewInputs;
        cornerCutInputs.targetBody = inputs.targetBody;
        cornerCutInputs.thickness = inputs.thickness;
        std::string cornerCutError;
        if (!CreateReferenceCornerEdgeCut(cornerCutInputs,
                                          referenceCornerEdge,
                                          cornerCutError))
        {
            // 26_2P_BiLanCao treats this additional cut as optional and keeps
            // the already-created main slot if its corner cut cannot resolve.
            AppendDebugLog("CreatePreview reference-project chamfer edge cut skipped/failed: " +
                           cornerCutError);
        }
    }

    previewUdfTag_ = finalResultTag;
    previewTargetBodyTag_ =
        inputs.targetBody != nullptr ? inputs.targetBody->Tag() : NULL_TAG;
    previewReferenceTags_ = allReferenceTags;
    if (inputs.smartMode && inputs.inferredFromSingleClick && inputs.targetBody != nullptr)
    {
        // Keep the point pair resolved on the untouched body for every smart
        // preview mode.  A chamfer/left/right replacement first rolls the old
        // preview back; caching the refreshed post-rip pair made the following
        // click resolve on an exposed opposite sheet face and reversed both
        // the start direction and the left/right result.
        retainSmartEndpointCacheOnUndo_ = true;
        const InferredInputs& cacheInputs = originalPreviewInputs;
        smartCachedP1_ = cacheInputs.startPoint;
        smartCachedP2_ = cacheInputs.endPoint;
        smartEndpointBodyTag_ = cacheInputs.targetBody != nullptr
                                    ? cacheInputs.targetBody->Tag()
                                    : inputs.targetBody->Tag();
        smartEndpointFaceTag_ = cacheInputs.baseFace != nullptr
                                    ? cacheInputs.baseFace->Tag()
                                    : NULL_TAG;
        hasSmartEndpointCache_ = true;
        AppendDebugLog(std::string("CreatePreview retained smart endpoints: policy=") +
                       "all-smart original pre-rip" +
                       ", body=" +
                       std::to_string(smartEndpointBodyTag_) +
                       ", face=" +
                       std::to_string(smartEndpointFaceTag_) +
                       ", P1=" + FormatPoint(smartCachedP1_) +
                       ", P2=" + FormatPoint(smartCachedP2_));
    }
    CapturePreviewCreatedFeatureTags();
    hasPreview_ = true;
    AppendDebugLog("CreatePreview OK, undoMark=" + std::to_string(static_cast<int>(previewUndoMark_)) +
                   ", previewUdfTag=" + std::to_string(previewUdfTag_) +
                   ", previewReferenceCount=" + std::to_string(previewReferenceTags_.size()));
    return true;
}

std::vector<tag_t> TwoPointSiBianUI::CurrentWorkPartFeatureTags() const
{
    std::vector<tag_t> tags;
    Part* workPart = session_ != nullptr ? session_->Parts()->Work() : nullptr;
    if (workPart == nullptr || workPart->Features() == nullptr)
    {
        return tags;
    }

    Features::FeatureCollection* features = workPart->Features();
    for (auto iterator = features->begin(); iterator != features->end(); ++iterator)
    {
        Features::Feature* feature = *iterator;
        if (feature != nullptr && feature->Tag() != NULL_TAG)
        {
            tags.push_back(feature->Tag());
        }
    }
    return tags;
}

void TwoPointSiBianUI::CapturePreviewCreatedFeatureTags()
{
    const std::set<tag_t> baseline(previewBaselineFeatureTags_.begin(),
                                   previewBaselineFeatureTags_.end());
    previewCreatedFeatureTags_.clear();
    for (tag_t featureTag : CurrentWorkPartFeatureTags())
    {
        if (baseline.find(featureTag) == baseline.end())
        {
            previewCreatedFeatureTags_.push_back(featureTag);
        }
    }
    AppendDebugLog("CreatePreview captured created feature count=" +
                   std::to_string(previewCreatedFeatureTags_.size()));
}

bool TwoPointSiBianUI::UndoPreview(bool includeCommitted)
{
    if (!retainSmartEndpointCacheOnUndo_)
    {
        hasSmartEndpointCache_ = false;
        smartEndpointBodyTag_ = NULL_TAG;
        smartEndpointFaceTag_ = NULL_TAG;
    }

    if (previewCommitted_ && !includeCommitted)
    {
        return true;
    }
    if (!hasPreview_ &&
        !previewCommitted_ &&
        previewUdfTag_ == NULL_TAG &&
        previewTargetBodyTag_ == NULL_TAG &&
        previewReferenceTags_.empty() &&
        previewBaselineFeatureTags_.empty() &&
        previewCreatedFeatureTags_.empty() &&
        previewUndoMark_ == static_cast<Session::UndoMarkId>(0))
    {
        return true;
    }

    bool undoSucceeded = false;
    try
    {
        if (previewUndoMark_ != static_cast<Session::UndoMarkId>(0))
        {
            const bool markExists =
                session_->DoesUndoMarkExist(previewUndoMark_, "2P_SiBian Preview");
            AppendDebugLog("UndoPreview mark=" +
                           std::to_string(static_cast<int>(previewUndoMark_)) +
                           ", exists=" + (markExists ? "true" : "false"));
            if (markExists)
            {
                AppendDebugLog("UndoPreview undoing mark=" +
                               std::to_string(static_cast<int>(previewUndoMark_)));
                session_->UndoToMark(previewUndoMark_, "2P_SiBian Preview");
                if (session_->DoesUndoMarkExist(previewUndoMark_, "2P_SiBian Preview"))
                {
                    session_->DeleteUndoMark(previewUndoMark_, "2P_SiBian Preview");
                }
                undoSucceeded = true;
            }
            else
            {
                AppendDebugLog("UndoPreview mark was recycled by NX; using dependency-aware feature cleanup.");
            }
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

    bool dependencyCleanupSucceeded = false;
    if (!undoSucceeded)
    {
        try
        {
            Part* workPart = session_ != nullptr ? session_->Parts()->Work() : nullptr;
            NXOpen::Update* updateManager =
                session_ != nullptr ? session_->UpdateManager() : nullptr;
            if (workPart == nullptr || workPart->Features() == nullptr ||
                updateManager == nullptr)
            {
                throw std::runtime_error("No work part or NX update manager is available.");
            }

            // The previous raw UF_OBJ_delete_object fallback could delete a
            // boolean before its internal sketch/UDF children had been
            // reconciled.  That left a partially-modified sheet body and made
            // the recorded P2 topologically invalid.  Queue every live feature
            // created after the baseline in one NX update transaction so NX
            // resolves parent/child deletion in dependency order.
            const std::set<tag_t> baseline(previewBaselineFeatureTags_.begin(),
                                           previewBaselineFeatureTags_.end());
            std::set<tag_t> queuedTags;
            std::vector<TaggedObject*> deleteObjects;
            Features::FeatureCollection* features = workPart->Features();
            for (auto iterator = features->begin(); iterator != features->end(); ++iterator)
            {
                Features::Feature* feature = *iterator;
                if (feature == nullptr || feature->Tag() == NULL_TAG ||
                    baseline.find(feature->Tag()) != baseline.end() ||
                    !queuedTags.insert(feature->Tag()).second)
                {
                    continue;
                }
                deleteObjects.push_back(feature);
            }

            auto queueLiveTaggedObject = [&](tag_t objectTag)
            {
                if (objectTag == NULL_TAG || !queuedTags.insert(objectTag).second)
                {
                    return;
                }
                try
                {
                    TaggedObject* object =
                        dynamic_cast<TaggedObject*>(NXObjectManager::Get(objectTag));
                    if (object != nullptr)
                    {
                        deleteObjects.push_back(object);
                    }
                }
                catch (...)
                {
                    // The object may already have been removed with an owning
                    // feature.  Do not call UF_OBJ_delete_object on a stale tag.
                }
            };
            for (tag_t referenceTag : previewReferenceTags_)
            {
                queueLiveTaggedObject(referenceTag);
            }

            AppendDebugLog("UndoPreview dependency cleanup queued object count=" +
                           std::to_string(deleteObjects.size()));
            if (deleteObjects.empty())
            {
                dependencyCleanupSucceeded = true;
            }
            else
            {
                const Session::UndoMarkId cleanupMark =
                    session_->SetUndoMark(Session::MarkVisibilityInvisible,
                                          "2P_SiBian Preview Cleanup");
                updateManager->ClearDeleteList();
                updateManager->ClearErrorList();
                const int addErrors =
                    updateManager->AddObjectsToDeleteList(deleteObjects);
                const int updateErrors = updateManager->DoUpdate(cleanupMark);
                if (session_->DoesUndoMarkExist(cleanupMark,
                                                "2P_SiBian Preview Cleanup"))
                {
                    session_->DeleteUndoMark(cleanupMark,
                                             "2P_SiBian Preview Cleanup");
                }

                std::vector<tag_t> remainingFeatureTags;
                for (tag_t featureTag : CurrentWorkPartFeatureTags())
                {
                    if (baseline.find(featureTag) == baseline.end())
                    {
                        remainingFeatureTags.push_back(featureTag);
                    }
                }
                dependencyCleanupSucceeded = remainingFeatureTags.empty();
                AppendDebugLog("UndoPreview dependency cleanup completed: addErrors=" +
                               std::to_string(addErrors) +
                               ", updateErrors=" + std::to_string(updateErrors) +
                               ", remainingCreatedFeatures=" +
                               std::to_string(remainingFeatureTags.size()) +
                               ", restored=" +
                               (dependencyCleanupSucceeded ? "true" : "false"));
            }
        }
        catch (const NXException& ex)
        {
            AppendDebugLog("UndoPreview dependency cleanup NXException: " +
                           NxExceptionText(ex));
        }
        catch (const std::exception& ex)
        {
            AppendDebugLog(std::string("UndoPreview dependency cleanup exception: ") +
                           ex.what());
        }
        catch (...)
        {
            AppendDebugLog("UndoPreview dependency cleanup unknown exception");
        }
    }

    const bool rollbackSucceeded = undoSucceeded || dependencyCleanupSucceeded;
    hasPreview_ = false;
    previewCommitted_ = false;
    previewUndoMark_ = static_cast<Session::UndoMarkId>(0);
    previewUdfTag_ = NULL_TAG;
    previewTargetBodyTag_ = NULL_TAG;
    previewReferenceTags_.clear();
    previewBaselineFeatureTags_.clear();
    previewCreatedFeatureTags_.clear();
    return rollbackSucceeded;
}

bool TwoPointSiBianUI::FlattenPreviewTargetBody(std::string& errorMessage)
{
    errorMessage.clear();
    if (!hasPreview_ || previewTargetBodyTag_ == NULL_TAG)
    {
        errorMessage =
            "The completed preview has no valid target body to convert into a single feature.";
        return false;
    }
    if (UF_OBJ_ask_status(previewTargetBodyTag_) != UF_OBJ_ALIVE)
    {
        errorMessage =
            "The target body is no longer alive, so it cannot be converted into a single feature.";
        return false;
    }

    int objectType = 0;
    int objectSubtype = 0;
    const int typeResult =
        UF_OBJ_ask_type_and_subtype(previewTargetBodyTag_, &objectType, &objectSubtype);
    if (typeResult != 0 || objectType != UF_solid_type)
    {
        errorMessage =
            "The preview target is not a live solid or sheet body: " +
            UfMessage(typeResult);
        return false;
    }

    uf_list_p_t bodyList = nullptr;
    int result = UF_MODL_create_list(&bodyList);
    if (result == 0)
    {
        result = UF_MODL_put_list_item(bodyList, previewTargetBodyTag_);
    }
    if (result == 0)
    {
        AppendDebugLog("FlattenPreviewTargetBody deleting body parameters: body=" +
                       std::to_string(previewTargetBodyTag_));
        result = UF_MODL_delete_body_parms(bodyList);
    }
    if (bodyList != nullptr)
    {
        UF_MODL_delete_list(&bodyList);
    }
    if (result != 0)
    {
        errorMessage =
            "Failed to convert the completed result into one non-parametric feature: " +
            UfMessage(result);
        return false;
    }

    uf_list_p_t featureList = nullptr;
    result = UF_MODL_ask_body_feats(previewTargetBodyTag_, &featureList);
    int featureCount = 0;
    if (result == 0 && featureList != nullptr)
    {
        result = UF_MODL_ask_list_count(featureList, &featureCount);
    }

    tag_t resultFeatureTag = NULL_TAG;
    if (result == 0 && featureCount == 1)
    {
        result = UF_MODL_ask_list_item(featureList, 0, &resultFeatureTag);
    }
    if (featureList != nullptr)
    {
        UF_MODL_delete_list(&featureList);
    }
    if (result != 0 || featureCount != 1 || resultFeatureTag == NULL_TAG)
    {
        errorMessage =
            "NX converted the body parameters, but the result did not resolve to exactly one feature.";
        AppendDebugLog("FlattenPreviewTargetBody verification failed: result=" +
                       std::to_string(result) +
                       ", featureCount=" + std::to_string(featureCount) +
                       ", featureTag=" + std::to_string(resultFeatureTag));
        return false;
    }

    result = UF_OBJ_set_name(resultFeatureTag, "2P_SiBian");
    if (result != 0)
    {
        errorMessage =
            "The single result feature was created, but it could not be named 2P_SiBian: " +
            UfMessage(result);
        return false;
    }

    previewUdfTag_ = resultFeatureTag;
    previewReferenceTags_.clear();
    previewCreatedFeatureTags_.clear();
    previewCreatedFeatureTags_.push_back(resultFeatureTag);
    AppendDebugLog("FlattenPreviewTargetBody OK: body=" +
                   std::to_string(previewTargetBodyTag_) +
                   ", feature=" + std::to_string(resultFeatureTag) +
                   ", name=2P_SiBian");
    return true;
}

void TwoPointSiBianUI::AssignCustomFeatureData(
    Features::CustomFeatureData* data,
    const InferredInputs& inputs) const
{
    if (data == nullptr)
    {
        throw std::runtime_error("The 2P_SiBian CustomFeature data is unavailable.");
    }

    data->CustomTagAttributeByName(
            zhihui_twopoint_sibian::kAttrTargetBody)
        ->SetValue(inputs.targetBody);
    data->CustomTagAttributeByName(
            zhihui_twopoint_sibian::kAttrBaseFace)
        ->SetValue(inputs.baseFace);
    data->CustomTagAttributeByName(
            zhihui_twopoint_sibian::kAttrStartEdge)
        ->SetValue(inputs.startEdge);
    data->CustomTagAttributeByName(
            zhihui_twopoint_sibian::kAttrEndEdge)
        ->SetValue(inputs.endEdge);
    data->CustomDoubleAttributeByName(
            zhihui_twopoint_sibian::kAttrStartX)
        ->SetValue(inputs.startPoint.X);
    data->CustomDoubleAttributeByName(
            zhihui_twopoint_sibian::kAttrStartY)
        ->SetValue(inputs.startPoint.Y);
    data->CustomDoubleAttributeByName(
            zhihui_twopoint_sibian::kAttrStartZ)
        ->SetValue(inputs.startPoint.Z);
    data->CustomDoubleAttributeByName(
            zhihui_twopoint_sibian::kAttrEndX)
        ->SetValue(inputs.endPoint.X);
    data->CustomDoubleAttributeByName(
            zhihui_twopoint_sibian::kAttrEndY)
        ->SetValue(inputs.endPoint.Y);
    data->CustomDoubleAttributeByName(
            zhihui_twopoint_sibian::kAttrEndZ)
        ->SetValue(inputs.endPoint.Z);
    data->CustomDoubleAttributeByName(
            zhihui_twopoint_sibian::kAttrThickness)
        ->SetValue(inputs.thickness);
    data->CustomDoubleAttributeByName(
            zhihui_twopoint_sibian::kAttrSpanLength)
        ->SetValue(inputs.spanLength);
    data->CustomStringAttributeByName(
            zhihui_twopoint_sibian::kAttrClearance)
        ->SetValue(NXString(inputs.clearanceValue.c_str(), NXString::UTF8));
    data->CustomStringAttributeByName(
            zhihui_twopoint_sibian::kAttrBendRadius)
        ->SetValue(NXString(inputs.bendRadiusValue.c_str(), NXString::UTF8));
    data->CustomIntegerAttributeByName(
            zhihui_twopoint_sibian::kAttrFeatureMode)
        ->SetValue(static_cast<int>(inputs.featureMode));
    data->CustomLogicalAttributeByName(
            zhihui_twopoint_sibian::kAttrSmartMode)
        ->SetValue(inputs.smartMode);
    data->CustomLogicalAttributeByName(
            zhihui_twopoint_sibian::kAttrChamferEdgeMode)
        ->SetValue(inputs.chamferEdgeMode);
    data->CustomLogicalAttributeByName(
            zhihui_twopoint_sibian::kAttrReverseCut)
        ->SetValue(inputs.reverseChamfer270Cut);
    data->CustomLogicalAttributeByName(
            zhihui_twopoint_sibian::kAttrInferredFromSingleClick)
        ->SetValue(inputs.inferredFromSingleClick);
    data->CustomDoubleAttributeByName(
            zhihui_twopoint_sibian::kAttrClickX)
        ->SetValue(inputs.selectionClickPoint.X);
    data->CustomDoubleAttributeByName(
            zhihui_twopoint_sibian::kAttrClickY)
        ->SetValue(inputs.selectionClickPoint.Y);
    data->CustomDoubleAttributeByName(
            zhihui_twopoint_sibian::kAttrClickZ)
        ->SetValue(inputs.selectionClickPoint.Z);
}

bool TwoPointSiBianUI::BeginEditedFeatureRollback(std::string& errorMessage)
{
    errorMessage.clear();
    if (editRollbackManager_ != nullptr)
    {
        return true;
    }
    if (editedFeature_ == nullptr || session_ == nullptr)
    {
        errorMessage = "No editable 2P_SiBian CustomFeature is active.";
        return false;
    }
    Part* workPart = session_->Parts()->Work();
    if (workPart == nullptr)
    {
        errorMessage = "No work part is active while starting rollback edit.";
        return false;
    }
    try
    {
        editRollbackMark_ = session_->SetUndoMark(
            // NX EditWithRollbackManager explicitly requires a visible mark;
            // an invisible mark is rejected before the model is rolled back.
            Session::MarkVisibilityVisible,
            "2P_SiBian Edit With Rollback");
        editRollbackManager_ =
            workPart->Features()->StartEditWithRollbackManager(
                editedFeature_, editRollbackMark_);
        if (editRollbackManager_ == nullptr)
        {
            throw std::runtime_error(
                "NX did not return an edit-with-rollback manager.");
        }
        AppendDebugLog(
            "edit-with-rollback started before the 2P_SiBian CustomFeature; "
            "saved P1/P2 now resolve on the original body topology.");
        return true;
    }
    catch (const NXException& ex)
    {
        errorMessage = "Failed to start rollback editing for 2P_SiBian: " +
                       NxExceptionText(ex);
    }
    catch (const std::exception& ex)
    {
        errorMessage = std::string(
                           "Failed to start rollback editing for 2P_SiBian: ") +
                       ex.what();
    }
    catch (...)
    {
        errorMessage = "Failed to start rollback editing for 2P_SiBian.";
    }
    editRollbackManager_ = nullptr;
    if (editRollbackMark_ != static_cast<Session::UndoMarkId>(0))
    {
        try
        {
            session_->DeleteUndoMark(editRollbackMark_,
                                     "2P_SiBian Edit With Rollback");
        }
        catch (...)
        {
        }
        editRollbackMark_ = static_cast<Session::UndoMarkId>(0);
    }
    AppendDebugLog(errorMessage);
    return false;
}

bool TwoPointSiBianUI::FinishEditedFeatureRollback(
    bool errorDuringEdit,
    std::string& errorMessage)
{
    errorMessage.clear();
    if (editRollbackManager_ == nullptr)
    {
        return true;
    }
    try
    {
        editRollbackManager_->UpdateFeature(errorDuringEdit);
        editRollbackManager_->Stop();
        editRollbackManager_->Destroy();
        editRollbackManager_ = nullptr;
        if (editRollbackMark_ != static_cast<Session::UndoMarkId>(0))
        {
            session_->DeleteUndoMark(editRollbackMark_,
                                     "2P_SiBian Edit With Rollback");
            editRollbackMark_ = static_cast<Session::UndoMarkId>(0);
        }
        AppendDebugLog(
            std::string("edit-with-rollback finished: ") +
            (errorDuringEdit
                 ? "original CustomFeature restored."
                 : "edited CustomFeature accepted and model rolled forward."));
        return true;
    }
    catch (const NXException& ex)
    {
        errorMessage = "Failed to finish rollback editing for 2P_SiBian: " +
                       NxExceptionText(ex);
    }
    catch (const std::exception& ex)
    {
        errorMessage = std::string(
                           "Failed to finish rollback editing for 2P_SiBian: ") +
                       ex.what();
    }
    catch (...)
    {
        errorMessage = "Failed to finish rollback editing for 2P_SiBian.";
    }
    AppendDebugLog(errorMessage);
    return false;
}

bool TwoPointSiBianUI::CommitCustomFeature(const InferredInputs& inputs,
                                           std::string& errorMessage)
{
    errorMessage.clear();
    Part* workPart = session_ != nullptr ? session_->Parts()->Work() : nullptr;
    const bool editingExistingFeature = editedFeature_ != nullptr;
    if (workPart == nullptr || featureClass_ == nullptr ||
        (!editingExistingFeature &&
         (inputs.targetBody == nullptr || inputs.baseFace == nullptr ||
          inputs.startEdge == nullptr || inputs.endEdge == nullptr)))
    {
        errorMessage =
            "The CustomFeature class or its resolved body/face/edge inputs are unavailable.";
        return false;
    }

    Features::CustomFeatureBuilder* builder = nullptr;
    try
    {
        builder = workPart->Features()->CreateCustomFeatureBuilder(editedFeature_);
        Features::CustomFeatureData* data = nullptr;
        if (editedFeature_ == nullptr)
        {
            Features::CustomAttributeCollection* attrs =
                workPart->Features()->CustomAttributeCollection();
            std::vector<Features::CustomAttribute*> values;
            values.push_back(CreateTagAttribute(
                attrs,
                zhihui_twopoint_sibian::kAttrTargetBody,
                inputs.targetBody,
                true,
                true));
            values.push_back(CreateTagAttribute(
                attrs,
                zhihui_twopoint_sibian::kAttrBaseFace,
                inputs.baseFace,
                false));
            values.push_back(CreateTagAttribute(
                attrs,
                zhihui_twopoint_sibian::kAttrStartEdge,
                inputs.startEdge,
                false));
            values.push_back(CreateTagAttribute(
                attrs,
                zhihui_twopoint_sibian::kAttrEndEdge,
                inputs.endEdge,
                false));
            values.push_back(CreateDoubleAttribute(
                attrs, zhihui_twopoint_sibian::kAttrStartX, inputs.startPoint.X));
            values.push_back(CreateDoubleAttribute(
                attrs, zhihui_twopoint_sibian::kAttrStartY, inputs.startPoint.Y));
            values.push_back(CreateDoubleAttribute(
                attrs, zhihui_twopoint_sibian::kAttrStartZ, inputs.startPoint.Z));
            values.push_back(CreateDoubleAttribute(
                attrs, zhihui_twopoint_sibian::kAttrEndX, inputs.endPoint.X));
            values.push_back(CreateDoubleAttribute(
                attrs, zhihui_twopoint_sibian::kAttrEndY, inputs.endPoint.Y));
            values.push_back(CreateDoubleAttribute(
                attrs, zhihui_twopoint_sibian::kAttrEndZ, inputs.endPoint.Z));
            values.push_back(CreateDoubleAttribute(
                attrs, zhihui_twopoint_sibian::kAttrThickness, inputs.thickness));
            values.push_back(CreateDoubleAttribute(
                attrs, zhihui_twopoint_sibian::kAttrSpanLength, inputs.spanLength));
            values.push_back(CreateStringAttribute(
                attrs,
                zhihui_twopoint_sibian::kAttrClearance,
                inputs.clearanceValue));
            values.push_back(CreateStringAttribute(
                attrs,
                zhihui_twopoint_sibian::kAttrBendRadius,
                inputs.bendRadiusValue));
            values.push_back(CreateIntegerAttribute(
                attrs,
                zhihui_twopoint_sibian::kAttrFeatureMode,
                static_cast<int>(inputs.featureMode)));
            values.push_back(CreateLogicalAttribute(
                attrs,
                zhihui_twopoint_sibian::kAttrSmartMode,
                inputs.smartMode));
            values.push_back(CreateLogicalAttribute(
                attrs,
                zhihui_twopoint_sibian::kAttrChamferEdgeMode,
                inputs.chamferEdgeMode));
            values.push_back(CreateLogicalAttribute(
                attrs,
                zhihui_twopoint_sibian::kAttrReverseCut,
                inputs.reverseChamfer270Cut));
            values.push_back(CreateLogicalAttribute(
                attrs,
                zhihui_twopoint_sibian::kAttrInferredFromSingleClick,
                inputs.inferredFromSingleClick));
            values.push_back(CreateDoubleAttribute(
                attrs,
                zhihui_twopoint_sibian::kAttrClickX,
                inputs.selectionClickPoint.X));
            values.push_back(CreateDoubleAttribute(
                attrs,
                zhihui_twopoint_sibian::kAttrClickY,
                inputs.selectionClickPoint.Y));
            values.push_back(CreateDoubleAttribute(
                attrs,
                zhihui_twopoint_sibian::kAttrClickZ,
                inputs.selectionClickPoint.Z));
            values.push_back(CreateIntegerAttribute(
                attrs,
                zhihui_twopoint_sibian::kAttrSchemaVersion,
                2));
            data = workPart->Features()
                       ->CustomFeatureDataCollection()
                       ->CreateData(featureClass_, values);
        }
        else
        {
            data = editedFeature_->FeatureData();
        }

        if (editingExistingFeature)
        {
            // Preserve the original target/face/edge references and P1/P2.
            // Those objects may have been replaced by the completed rip and
            // offset topology, but the registered construction members retain
            // their own associative inputs.
            data->CustomStringAttributeByName(
                    zhihui_twopoint_sibian::kAttrClearance)
                ->SetValue(NXString(inputs.clearanceValue.c_str(), NXString::UTF8));
            data->CustomStringAttributeByName(
                    zhihui_twopoint_sibian::kAttrBendRadius)
                ->SetValue(NXString(inputs.bendRadiusValue.c_str(), NXString::UTF8));
        }
        else
        {
            AssignCustomFeatureData(data, inputs);
        }

        lastPreviewInputs_ = inputs;
        hasLastPreviewInputs_ = true;
        // Only a brand-new CustomFeature may create construction members in
        // PreUpdate.  Editing must preserve the existing list and let the
        // Core InternalFeaturePreUpdate callback edit each member in place.
        buildingCustomFeature_ = !editingExistingFeature;
        customFeatureConstructionRebuilt_ = false;
        builder->SetFeatureData(data);
        Features::Feature* committed = builder->CommitFeature();
        builder->Destroy();
        builder = nullptr;
        buildingCustomFeature_ = false;

        if (editingExistingFeature)
        {
            // CommitFeature schedules the internal-feature callbacks.  Finish
            // that one scheduled pass before Block Styler reports a successful
            // preview.  This is the same edit contract used by feature 18 and
            // prevents Apply/OK from observing a half-updated construction
            // chain that only succeeds after a second double-click.
            const int updateResult = UF_MODL_update();
            if (updateResult != 0)
            {
                throw NXException::Create(
                    updateResult,
                    ("Failed to finish the edited 2P_SiBian model update: " +
                     UfMessage(updateResult))
                        .c_str());
            }
            AppendDebugLog(
                "CommitCustomFeature edit: scheduled internal-feature update completed synchronously.");
        }

        Features::CustomFeature* customFeature =
            dynamic_cast<Features::CustomFeature*>(committed);
        if (customFeature == nullptr)
        {
            errorMessage = "NX did not return the committed 2P_SiBian CustomFeature.";
            return false;
        }
        customFeature->SetName(zhihui_twopoint_sibian::kFeatureDisplayName);
        if (editingExistingFeature)
        {
            editedFeature_ = customFeature;
        }
        AppendDebugLog("CommitCustomFeature OK: node=" +
                       std::to_string(customFeature->Tag()) +
                       ", one opaque node with construction features.");
        return true;
    }
    catch (const NXException& ex)
    {
        buildingCustomFeature_ = false;
        if (builder != nullptr)
        {
            builder->Destroy();
        }
        errorMessage = "Failed to create/update the 2P_SiBian CustomFeature: " +
                       NxExceptionText(ex);
    }
    catch (const std::exception& ex)
    {
        buildingCustomFeature_ = false;
        if (builder != nullptr)
        {
            builder->Destroy();
        }
        errorMessage =
            std::string("Failed to create/update the 2P_SiBian CustomFeature: ") +
            ex.what();
    }
    catch (...)
    {
        buildingCustomFeature_ = false;
        if (builder != nullptr)
        {
            builder->Destroy();
        }
        errorMessage = "Failed to create/update the 2P_SiBian CustomFeature.";
    }
    AppendDebugLog(errorMessage);
    return false;
}

bool TwoPointSiBianUI::RestoreEditedFeatureBaseline(
    std::string& errorMessage)
{
    errorMessage.clear();
    if (editedFeature_ == nullptr || !editedLivePreviewDirty_)
    {
        return true;
    }
    if (!hasEditedBaselineInputs_)
    {
        errorMessage =
            "The original 2P_SiBian edit values were not retained for Cancel.";
        return false;
    }

    isUpdatingPreview_ = true;
    const bool restored =
        CommitCustomFeature(editedBaselineInputs_, errorMessage);
    isUpdatingPreview_ = false;
    if (restored)
    {
        lastPreviewInputs_ = editedBaselineInputs_;
        hasLastPreviewInputs_ = true;
        editedLivePreviewDirty_ = false;
    }
    return restored;
}

int TwoPointSiBianUI::BuildCustomFeatureConstruction(
    Features::CustomFeaturePreUpdateEvent* event)
{
    if (event == nullptr)
    {
        return 1;
    }
    const std::vector<Features::ConstructionFeatureData*> existingConstruction =
        event->GetConstructionFeatures();
    if (!existingConstruction.empty())
    {
        // Siemens' CustomFeature contract requires existing construction
        // features to be retained here.  Their parameters are edited later by
        // InternalFeaturePreUpdateCallback; never recreate a second rip chain.
        event->SetConstructionFeatures(existingConstruction);
        AppendDebugLog(
            "CustomFeature pre-update retained existing construction chain: count=" +
            std::to_string(existingConstruction.size()));
        return 0;
    }

    if (!buildingCustomFeature_ || !hasLastPreviewInputs_)
    {
        AppendDebugLog(
            "CustomFeature pre-update has no existing construction chain and no authorized initial build context.");
        return 1;
    }

    AppendDebugLog("CustomFeature pre-update: creating the initial internal branch chain.");
    customFeatureConstructionRebuilt_ = true;
    InferredInputs buildInputs = lastPreviewInputs_;
    if (!CompleteInputsForEndpoints(buildInputs))
    {
        AppendDebugLog(
            "CustomFeature pre-update could not resolve the persisted P1/P2 on the input body.");
        return 1;
    }
    lastPreviewInputs_ = buildInputs;
    if (!CreatePreview(&buildInputs))
    {
        AppendDebugLog("CustomFeature pre-update failed while creating the internal chain.");
        return 1;
    }

    // CreatePreview may replace the original right-angle P2 with the endpoint
    // shared by the selected plane and the newly offset rip face.  Persist
    // that actual primary-UDF point on the opaque node.  Only numeric data is
    // updated here; storing the child offset edge itself would create a
    // circular dependency from the CustomFeature to its construction member.
    if (event->GetCustomFeature() != nullptr &&
        hasResolvedPrimaryP2ForPersistence_ && hasLastPreviewInputs_ &&
        (lastPreviewInputs_.featureMode == FeatureMode::NinetyLeft ||
         lastPreviewInputs_.featureMode == FeatureMode::NinetyRight))
    {
        Features::CustomFeatureData* featureData =
            event->GetCustomFeature()->FeatureData();
        if (featureData != nullptr)
        {
            featureData->CustomDoubleAttributeByName(
                    zhihui_twopoint_sibian::kAttrEndX)
                ->SetValue(resolvedPrimaryP2ForPersistence_.X);
            featureData->CustomDoubleAttributeByName(
                    zhihui_twopoint_sibian::kAttrEndY)
                ->SetValue(resolvedPrimaryP2ForPersistence_.Y);
            featureData->CustomDoubleAttributeByName(
                    zhihui_twopoint_sibian::kAttrEndZ)
                ->SetValue(resolvedPrimaryP2ForPersistence_.Z);
            featureData->CustomDoubleAttributeByName(
                    zhihui_twopoint_sibian::kAttrSpanLength)
                ->SetValue(Distance(lastPreviewInputs_.startPoint,
                                    resolvedPrimaryP2ForPersistence_));
            AppendDebugLog(
                "CustomFeature persisted the actual primary right-angle P2: " +
                FormatPoint(resolvedPrimaryP2ForPersistence_));
        }
    }

    std::vector<Features::ConstructionFeatureData*> constructionData;
    std::set<tag_t> seen;
    const tag_t customFeatureTag =
        event->GetCustomFeature() != nullptr
            ? event->GetCustomFeature()->Tag()
            : NULL_TAG;
    for (tag_t featureTag : previewCreatedFeatureTags_)
    {
        if (featureTag == NULL_TAG || featureTag == customFeatureTag ||
            !seen.insert(featureTag).second ||
            UF_OBJ_ask_status(featureTag) != UF_OBJ_ALIVE)
        {
            continue;
        }
        try
        {
            Features::Feature* feature = dynamic_cast<Features::Feature*>(
                NXObjectManager::Get(featureTag));
            if (feature == nullptr)
            {
                continue;
            }
            // An extrusion already owns its sketch through
            // SetParentFeatureInternal/MakeSketchInternal. Registering that
            // sketch again as a CustomFeature construction member flattens
            // the navigator relationship. Keep internal UDF/sketch children
            // with their immediate owning feature and register only the
            // top-level construction features here.
            if (feature->IsInternal())
            {
                AppendDebugLog(
                    "CustomFeature retained internal child under its owning feature: tag=" +
                    std::to_string(featureTag));
                continue;
            }
            Features::ConstructionFeatureData* item =
                event->CreateConstructionFeatureData(feature);
            // This flag controls graphics, not Part Navigator disclosure.
            // Keep it true so body-modifying rips/offsets/booleans display
            // their final target body while NX hides the internal history.
            item->SetShowInGraphicView(true);
            constructionData.push_back(item);
        }
        catch (...)
        {
            AppendDebugLog("CustomFeature skipped an unavailable construction feature tag=" +
                           std::to_string(featureTag));
        }
    }
    if (constructionData.empty())
    {
        UndoPreview();
        AppendDebugLog("CustomFeature pre-update produced no construction features.");
        return 1;
    }

    event->SetConstructionFeatures(constructionData);
    const size_t constructionCount = constructionData.size();
    CommitPreview();
    FinalizeCommittedPreview();
    AppendDebugLog("CustomFeature pre-update registered constructionFeatureCount=" +
                   std::to_string(constructionCount));
    return 0;
}

extern "C" __declspec(dllexport) int ZhihuiTwoPointSiBianBuildCustomFeature(
    void* eventPointer)
{
    if (gActiveTwoPointSiBianDialog == nullptr || eventPointer == nullptr)
    {
        return 1;
    }
    return gActiveTwoPointSiBianDialog->BuildCustomFeatureConstruction(
        static_cast<Features::CustomFeaturePreUpdateEvent*>(eventPointer));
}

bool TwoPointSiBianUI::ConsolidatePreviewFeatureTemplate(
    std::string& errorMessage)
{
    errorMessage.clear();
    if (!hasPreview_ || previewCreatedFeatureTags_.empty())
    {
        errorMessage =
            "The completed preview has no feature chain to package as 2P_SiBian.";
        return false;
    }

    std::vector<tag_t> liveFeatureTags;
    std::set<tag_t> uniqueTags;
    for (tag_t featureTag : previewCreatedFeatureTags_)
    {
        if (featureTag == NULL_TAG ||
            UF_OBJ_ask_status(featureTag) != UF_OBJ_ALIVE ||
            !uniqueTags.insert(featureTag).second)
        {
            continue;
        }
        int objectType = 0;
        int objectSubtype = 0;
        if (UF_OBJ_ask_type_and_subtype(featureTag,
                                       &objectType,
                                       &objectSubtype) == 0 &&
            objectType == UF_feature_type)
        {
            liveFeatureTags.push_back(featureTag);
        }
    }
    if (liveFeatureTags.empty())
    {
        errorMessage =
            "None of the preview features remained available for feature-template packaging.";
        return false;
    }

    Part* workPart = session_ != nullptr ? session_->Parts()->Work() : nullptr;
    if (workPart == nullptr || workPart->UserDefinedTemplates() == nullptr)
    {
        errorMessage =
            "No work part is available for creation of the complete branch feature template.";
        return false;
    }

    // Build a genuine UDF definition from the exact feature chain selected by
    // the current branch, including any continuation iterations. Converting
    // that definition gives NX a real UDT Feature Group instead of the plain
    // feature set previously created by UF_MODL_create_set_of_feature.
    tag_t targetBodyReference = previewTargetBodyTag_;
    char targetPrompt[] = "Target sheet body";
    char* referencePrompts[] = {targetPrompt};
    UF_MODL_udfs_def_data_t definitionData{};
    definitionData.frecs = liveFeatureTags.data();
    definitionData.num_frecs = static_cast<int>(liveFeatureTags.size());
    definitionData.exp_defs = nullptr;
    definitionData.num_exp = 0;
    definitionData.refs = &targetBodyReference;
    definitionData.ref_prompts = referencePrompts;
    definitionData.num_ref = 1;
    std::strncpy(definitionData.name_str_data,
                 "2P_SiBian",
                 sizeof(definitionData.name_str_data) - 1);
    definitionData.name_str_data[sizeof(definitionData.name_str_data) - 1] = '\0';
    definitionData.explosion_flag = false;

    const std::vector<tag_t> featureTagsBeforeConversion =
        CurrentWorkPartFeatureTags();
    const std::set<tag_t> featuresBeforeConversion(
        featureTagsBeforeConversion.begin(),
        featureTagsBeforeConversion.end());
    tag_t udfDefinitionTag = NULL_TAG;
    const int createDefinitionResult =
        UF_MODL_udfs_create_def(&udfDefinitionTag, &definitionData);
    if (createDefinitionResult != 0 || udfDefinitionTag == NULL_TAG)
    {
        errorMessage =
            "NX could not create a complete UDF definition for the selected branch: " +
            UfMessage(createDefinitionResult);
        AppendDebugLog(errorMessage);
        return false;
    }

    try
    {
        Features::Feature* definitionFeature =
            dynamic_cast<Features::Feature*>(NXObjectManager::Get(udfDefinitionTag));
        if (definitionFeature == nullptr)
        {
            throw std::runtime_error(
                "The complete UDF definition feature could not be resolved.");
        }
        workPart->UserDefinedTemplates()->ConvertUdfToFeaturetemplate(
            definitionFeature);
    }
    catch (const NXException& ex)
    {
        errorMessage =
            "NX could not convert the complete branch definition into a Feature Template: " +
            NxExceptionText(ex);
        AppendDebugLog(errorMessage);
        return false;
    }
    catch (const std::exception& ex)
    {
        errorMessage =
            std::string("NX could not convert the complete branch definition into a Feature Template: ") +
            ex.what();
        AppendDebugLog(errorMessage);
        return false;
    }

    tag_t templateGroupTag = NULL_TAG;
    for (auto iterator = workPart->Features()->begin();
         iterator != workPart->Features()->end();
         ++iterator)
    {
        Features::Feature* feature = *iterator;
        if (feature == nullptr ||
            featuresBeforeConversion.find(feature->Tag()) !=
                featuresBeforeConversion.end())
        {
            continue;
        }
        const char* featureTypeText = feature->FeatureType().GetText();
        const std::string featureType =
            ToLowerAscii(featureTypeText != nullptr ? featureTypeText : "");
        if (featureType.find("udt feature group") != std::string::npos)
        {
            templateGroupTag = feature->Tag();
        }
    }
    if (templateGroupTag == NULL_TAG)
    {
        errorMessage =
            "NX converted the complete branch, but its UDT Feature Group could not be resolved.";
        AppendDebugLog(errorMessage +
                       " definition=" + std::to_string(udfDefinitionTag));
        return false;
    }

    UF_OBJ_set_name(templateGroupTag, "2P_SiBian");
    previewUdfTag_ = templateGroupTag;
    previewCreatedFeatureTags_.push_back(templateGroupTag);
    AppendDebugLog("ConsolidatePreviewFeatureTemplate OK: genuineFeatureTemplate=true"
                   ", definition=" + std::to_string(udfDefinitionTag) +
                   ", group=" + std::to_string(templateGroupTag) +
                   ", branchMemberCount=" +
                   std::to_string(liveFeatureTags.size()) +
                   ", targetBodyReference=" +
                   std::to_string(previewTargetBodyTag_) +
                   ", variableContinuationSupported=true");
    return true;
}

void TwoPointSiBianUI::CommitPreview()
{
    if (!hasPreview_)
    {
        return;
    }

    AppendDebugLog("CommitPreview retaining replaceable mark=" +
                   std::to_string(static_cast<int>(previewUndoMark_)) +
                   " until mode/input change or dialog finalization.");
    hasPreview_ = false;
    previewCommitted_ = true;
    hasSmartEndpointCache_ = false;
    smartEndpointBodyTag_ = NULL_TAG;
    smartEndpointFaceTag_ = NULL_TAG;
    retainSmartEndpointCacheOnUndo_ = false;
}

void TwoPointSiBianUI::FinalizeCommittedPreview()
{
    if (!previewCommitted_)
    {
        return;
    }
    try
    {
        if (previewUndoMark_ != static_cast<Session::UndoMarkId>(0))
        {
            AppendDebugLog("FinalizeCommittedPreview deleting retained mark=" +
                           std::to_string(static_cast<int>(previewUndoMark_)));
            session_->DeleteUndoMark(previewUndoMark_, "2P_SiBian Preview");
        }
    }
    catch (const NXException& ex)
    {
        AppendDebugLog("FinalizeCommittedPreview NXException: " + UfMessage(ex.ErrorCode()));
    }
    catch (...)
    {
        AppendDebugLog("FinalizeCommittedPreview unknown exception");
    }

    previewCommitted_ = false;
    previewUndoMark_ = static_cast<Session::UndoMarkId>(0);
    previewUdfTag_ = NULL_TAG;
    previewTargetBodyTag_ = NULL_TAG;
    previewReferenceTags_.clear();
    previewBaselineFeatureTags_.clear();
    previewCreatedFeatureTags_.clear();
}
bool TwoPointSiBianUI::ReadInputs(InferredInputs& inputs,
                                  TaggedObject* singleClickObjectOverride,
                                  const Point3d* singleClickPointOverride) const
{
    inputs.smartMode = IsSmartModeEnabled();
    inputs.featureMode = ReadFeatureMode();
    inputs.reverseChamfer270Cut = reverseChamfer270Cut_;

    TaggedObject* selectedStartObject = nullptr;
    TaggedObject* selectedEndObject = nullptr;
    Point3d selectedStartPoint;
    Point3d selectedEndPoint;
    bool hasStart = ReadSelectedPoint(startPointBlock_, selectedStartObject, selectedStartPoint);
    bool hasEnd = ReadSelectedPoint(endPointBlock_, selectedEndObject, selectedEndPoint);

    if (!inputs.smartMode && !hasStart && !hasEnd && hasEditedEndpointCache_)
    {
        selectedStartPoint = editedCachedP1_;
        selectedEndPoint = editedCachedP2_;
        hasStart = true;
        hasEnd = true;
        try
        {
            selectedStartObject = editedStartEdgeTag_ != NULL_TAG
                                      ? dynamic_cast<TaggedObject*>(
                                            NXObjectManager::Get(editedStartEdgeTag_))
                                      : nullptr;
            selectedEndObject = editedEndEdgeTag_ != NULL_TAG
                                    ? dynamic_cast<TaggedObject*>(
                                          NXObjectManager::Get(editedEndEdgeTag_))
                                    : nullptr;
            inputs.targetBody = editedTargetBodyTag_ != NULL_TAG
                                    ? dynamic_cast<Body*>(
                                          NXObjectManager::Get(editedTargetBodyTag_))
                                    : nullptr;
            inputs.baseFace = editedBaseFaceTag_ != NULL_TAG
                                  ? dynamic_cast<Face*>(
                                        NXObjectManager::Get(editedBaseFaceTag_))
                                  : nullptr;
        }
        catch (...)
        {
            selectedStartObject = nullptr;
            selectedEndObject = nullptr;
            inputs.targetBody = nullptr;
            inputs.baseFace = nullptr;
        }
        AppendDebugLog("ReadInputs restored CustomFeature endpoints: P1=" +
                       FormatPoint(selectedStartPoint) +
                       ", P2=" + FormatPoint(selectedEndPoint) +
                       ", targetBody=" +
                       std::to_string(editedTargetBodyTag_));
    }

    if (singleClickObjectOverride != nullptr && singleClickPointOverride != nullptr)
    {
        selectedStartObject = nullptr;
        hasStart = false;
        selectedEndObject = singleClickObjectOverride;
        selectedEndPoint = *singleClickPointOverride;
        hasEnd = true;
        AppendDebugLog("ReadInputs using rollback-safe smart click body=" +
                       std::to_string(singleClickObjectOverride->Tag()) +
                       ", point=" + FormatPoint(selectedEndPoint));
    }
    else if (inputs.smartMode && hasStart && hasEnd)
    {
        // Automatic progression can leave both selection blocks populated
        // after several face clicks. Smart mode is intentionally one-click;
        // use the block that generated the latest update.
        if (activeSmartSelectionBlock_ == startPointBlock_)
        {
            hasEnd = false;
            selectedEndObject = nullptr;
        }
        else
        {
            hasStart = false;
            selectedStartObject = nullptr;
        }
        AppendDebugLog("ReadInputs smart mode ignored the older populated selection block.");
    }

    if (!inputs.smartMode)
    {
        if (!hasStart || !hasEnd ||
            Distance(selectedStartPoint, selectedEndPoint) <= kPointTolerance)
        {
            AppendDebugLog("ReadInputs manual mode requires two distinct edge endpoints; single-face inference is disabled.");
            return false;
        }
        inputs.startObject = selectedStartObject;
        inputs.endObject = selectedEndObject;
        inputs.startPoint = selectedStartPoint;
        inputs.endPoint = selectedEndPoint;
    }
    else
    {
        TaggedObject* clickObject = hasEnd ? selectedEndObject : selectedStartObject;
        if (clickObject == nullptr)
        {
            AppendDebugLog("ReadInputs smart mode requires one selected face point.");
            return false;
        }
        Point3d clickPoint = hasEnd ? selectedEndPoint : selectedStartPoint;
        inputs.inferredFromSingleClick = true;
        inputs.selectionClickPoint = clickPoint;

        Body* clickBody = FindBody(clickObject);
        const bool canReuseSmartEndpoints =
            inputs.smartMode && hasSmartEndpointCache_ && clickBody != nullptr &&
            clickBody->Tag() == smartEndpointBodyTag_;
        if (canReuseSmartEndpoints)
        {
            inputs.startObject = clickObject;
            inputs.endObject = clickObject;
            inputs.startPoint = smartCachedP1_;
            inputs.endPoint = smartCachedP2_;
            inputs.targetBody = clickBody;
            inputs.baseFace = nullptr;
            if (smartEndpointFaceTag_ != NULL_TAG)
            {
                try
                {
                    Face* cachedFace = dynamic_cast<Face*>(
                        NXObjectManager::Get(smartEndpointFaceTag_));
                    if (cachedFace != nullptr &&
                        cachedFace->GetBody() != nullptr &&
                        cachedFace->GetBody()->Tag() == clickBody->Tag() &&
                        cachedFace->SolidFaceType() == Face::FaceTypePlanar &&
                        PointOnFacePlane(cachedFace, inputs.startPoint) &&
                        PointOnFacePlane(cachedFace, inputs.endPoint))
                    {
                        inputs.baseFace = cachedFace;
                        AppendDebugLog(
                            "ReadInputs reused recorded smart face: face=" +
                            std::to_string(cachedFace->Tag()));
                    }
                }
                catch (const NXException& ex)
                {
                    AppendDebugLog(
                        "ReadInputs recorded smart face is unavailable: " +
                        UfMessage(ex.ErrorCode()));
                }
                catch (...)
                {
                    AppendDebugLog(
                        "ReadInputs recorded smart face is unavailable: unknown exception.");
                }
            }
            if (inputs.baseFace == nullptr)
            {
                inputs.baseFace = FindPlanarFaceContainingPoints(clickBody,
                                                                 inputs.startPoint,
                                                                 inputs.endPoint);
            }
            if (inputs.baseFace == nullptr)
            {
                AppendDebugLog("ReadInputs smart endpoint cache rejected: cached P1/P2 no longer share a planar face.");
                if (!InferEndpointsFromFaceClick(clickObject, clickPoint, inputs))
                {
                    return false;
                }
            }
            else
            {
                Point3d planePoint;
                Vector3d planeNormal;
                if (FacePlaneData(inputs.baseFace, planePoint, planeNormal) &&
                    Normalize(planeNormal))
                {
                    const double normalDistance = Dot(Subtract(clickPoint, planePoint),
                                                      planeNormal);
                    clickPoint = Point3d(clickPoint.X - planeNormal.X * normalDistance,
                                         clickPoint.Y - planeNormal.Y * normalDistance,
                                         clickPoint.Z - planeNormal.Z * normalDistance);
                    inputs.selectionClickPoint = clickPoint;
                }
                AppendDebugLog("ReadInputs reused recorded smart endpoints: body=" +
                               std::to_string(clickBody->Tag()) +
                               ", P1=" + FormatPoint(inputs.startPoint) +
                               ", P2=" + FormatPoint(inputs.endPoint) +
                               ", projectedClick=" + FormatPoint(inputs.selectionClickPoint));
            }
        }
        else if (!InferEndpointsFromFaceClick(clickObject, clickPoint, inputs))
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

    if (inputs.smartMode && inputs.inferredFromSingleClick)
    {
        const Vector3d clickToP1 = Subtract(inputs.startPoint, inputs.selectionClickPoint);
        const Vector3d clickToP2 = Subtract(inputs.endPoint, inputs.selectionClickPoint);
        const double clickAngle = AngleDegrees(clickToP1, clickToP2);
        const double signedMouseY = Dot(Subtract(inputs.selectionClickPoint, inputs.startPoint),
                                        yDirection);
        if (clickAngle < kEndpointPairMinimumAngleDegrees - 1.0e-6)
        {
            inputs.featureMode = signedMouseY >= 0.0
                                     ? FeatureMode::NinetyLeft
                                     : FeatureMode::NinetyRight;
        }
        else
        {
            inputs.featureMode = FeatureMode::Chamfer;
        }
        const char* smartModeName = inputs.featureMode == FeatureMode::NinetyLeft
                                        ? "90-left"
                                        : (inputs.featureMode == FeatureMode::NinetyRight
                                               ? "90-right"
                                               : "chamfer");
        AppendDebugLog("smart mode decision: clickAngle=" +
                       FormatExpressionNumber(clickAngle) +
                       ", signedMouseY=" + FormatExpressionNumber(signedMouseY) +
                       ", result=" + smartModeName);
    }
    else if (inputs.smartMode)
    {
        AppendDebugLog("smart mode skipped because two explicit endpoints were selected; using manual enum value.");
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
    inputs.chamferEdgeMode =
        chamferEdgeToggleBlock_ == nullptr || chamferEdgeToggleBlock_->Value();

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
          << ", bendRadiusValue=" << inputs.bendRadiusValue
          << ", chamferEdge=" << (inputs.chamferEdgeMode ? 1 : 0)
          << ", reverseChamfer270Cut=" << (inputs.reverseChamfer270Cut ? 1 : 0);
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

bool TwoPointSiBianUI::RefreshSmartInputsAfterRips(const InferredInputs& originalInputs,
                                                   InferredInputs& refreshedInputs) const
{
    Body* body = originalInputs.targetBody;
    if (body == nullptr)
    {
        return false;
    }

    Face* face = originalInputs.baseFace;
    bool recordedFaceIsUsable = false;
    if (face != nullptr)
    {
        try
        {
            recordedFaceIsUsable =
                face->GetBody() != nullptr &&
                face->GetBody()->Tag() == body->Tag() &&
                face->SolidFaceType() == Face::FaceTypePlanar &&
                PointOnFacePlane(face, originalInputs.startPoint) &&
                PointOnFacePlane(face, originalInputs.endPoint);
        }
        catch (...)
        {
            recordedFaceIsUsable = false;
        }
    }
    if (!recordedFaceIsUsable)
    {
        face = FindPlanarFaceContainingPoints(body,
                                              originalInputs.startPoint,
                                              originalInputs.endPoint);
    }
    else
    {
        AppendDebugLog(
            "RefreshSmartInputsAfterRips retained recorded selection face=" +
            std::to_string(face->Tag()));
    }
    if (face == nullptr || face->SolidFaceType() != Face::FaceTypePlanar)
    {
        AppendDebugLog("RefreshSmartInputsAfterRips failed: the original endpoint plane is unavailable.");
        return false;
    }

    const double selectedFaceAreaSquareMillimeters =
        MeasureFaceArea(face);
    Part* workPart = session_->Parts()->Work();
    const double selectedFaceArea =
        SquareMillimetersToPartSquareUnits(
            workPart,
            selectedFaceAreaSquareMillimeters);
    const double minimumAcceptedInnerLoopArea =
        selectedFaceArea > kPointTolerance
            ? selectedFaceArea *
                  kSmartInnerLoopMinimumAreaRatio
            : std::numeric_limits<double>::infinity();
    AppendDebugLog(
        "RefreshSmartInputsAfterRips inner-loop filter: face=" +
        std::to_string(face->Tag()) +
        ", faceAreaSquareMillimeters=" +
        FormatExpressionNumber(
            selectedFaceAreaSquareMillimeters) +
        ", faceAreaPartUnits=" +
        FormatExpressionNumber(selectedFaceArea) +
        ", minimumAcceptedInnerLoopArea=" +
        FormatExpressionNumber(
            minimumAcceptedInnerLoopArea));

    std::vector<Point3d> boundaryPoints;
    if (!FaceBoundaryPoints(face,
                            boundaryPoints,
                            minimumAcceptedInnerLoopArea) ||
        boundaryPoints.empty())
    {
        AppendDebugLog("RefreshSmartInputsAfterRips failed: no updated peripheral endpoints were found.");
        return false;
    }

    const double maximumSnapDistance =
        std::max(5.0, originalInputs.thickness * 5.0);
    const std::size_t filteredBoundaryPointCount =
        boundaryPoints.size();
    auto addUniqueBoundaryPoint = [&boundaryPoints](const Point3d& point)
    {
        for (const Point3d& existing : boundaryPoints)
        {
            if (Distance(existing, point) <= kPointTolerance)
            {
                return false;
            }
        }
        boundaryPoints.push_back(point);
        return true;
    };

    // Smart selection intentionally rejects small inner loops so holes near the
    // mouse do not replace the true peripheral endpoints.  A sheet-metal rip,
    // however, can turn the original P2 into a small new relief loop.  After the
    // rip only, admit topology endpoints local to the recorded P1/P2.  The
    // bounded search keeps the original smart-selection rule intact and avoids
    // snapping to an unrelated distant hole.
    std::size_t localPostRipPointCount = 0;
    try
    {
        for (Edge* edge : body->GetEdges())
        {
            if (edge == nullptr)
            {
                continue;
            }
            Point3d first;
            Point3d second;
            edge->GetVertices(&first, &second);
            for (const Point3d& endpoint :
                 std::array<Point3d, 2>{first, second})
            {
                const bool nearRecordedEndpoint =
                    Distance(endpoint, originalInputs.startPoint) <=
                        maximumSnapDistance ||
                    Distance(endpoint, originalInputs.endPoint) <=
                        maximumSnapDistance;
                const bool onOriginalEndpointPlane =
                    PointOnFacePlane(face, endpoint);
                if (nearRecordedEndpoint &&
                    onOriginalEndpointPlane &&
                    addUniqueBoundaryPoint(endpoint))
                {
                    ++localPostRipPointCount;
                    AppendDebugLog(
                        "RefreshSmartInputsAfterRips added local body-edge endpoint: edge=" +
                        std::to_string(edge->Tag()) +
                        ", point=" + FormatPoint(endpoint) +
                        ", distanceToP1=" +
                        FormatExpressionNumber(
                            Distance(endpoint,
                                     originalInputs.startPoint)) +
                        ", distanceToP2=" +
                        FormatExpressionNumber(
                            Distance(endpoint,
                                     originalInputs.endPoint)));
                }
            }
        }
    }
    catch (const NXException& ex)
    {
        AppendDebugLog(
            "RefreshSmartInputsAfterRips local post-rip endpoint scan NXException: " +
            UfMessage(ex.ErrorCode()));
    }
    catch (...)
    {
        AppendDebugLog(
            "RefreshSmartInputsAfterRips local post-rip endpoint scan unknown exception.");
    }
    AppendDebugLog(
        "RefreshSmartInputsAfterRips endpoint pool: filteredBoundaryPoints=" +
        std::to_string(filteredBoundaryPointCount) +
        ", addedLocalPostRipPoints=" +
        std::to_string(localPostRipPointCount) +
        ", maximumSnapDistance=" +
        FormatExpressionNumber(maximumSnapDistance));

    struct EndpointCandidate
    {
        Point3d point;
        double displacement = 0.0;
    };
    auto buildCandidates = [&](const Point3d& originalPoint)
    {
        std::vector<EndpointCandidate> candidates;
        candidates.push_back({originalPoint, 0.0});
        for (const Point3d& point : boundaryPoints)
        {
            const double displacement = Distance(point, originalPoint);
            bool duplicate = false;
            for (const EndpointCandidate& existing : candidates)
            {
                if (Distance(existing.point, point) <= kPointTolerance)
                {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate)
            {
                candidates.push_back({point, displacement});
            }
        }
        std::sort(candidates.begin(), candidates.end(), [](const EndpointCandidate& lhs,
                                                            const EndpointCandidate& rhs)
        {
            return lhs.displacement < rhs.displacement;
        });
        if (candidates.size() > 8)
        {
            candidates.resize(8);
        }
        return candidates;
    };

    const std::vector<EndpointCandidate> startCandidates =
        buildCandidates(originalInputs.startPoint);
    const std::vector<EndpointCandidate> endCandidates =
        buildCandidates(originalInputs.endPoint);
    double bestScore = std::numeric_limits<double>::max();
    bool found = false;

    for (const EndpointCandidate& start : startCandidates)
    {
        if (start.displacement > maximumSnapDistance)
        {
            continue;
        }
        for (const EndpointCandidate& end : endCandidates)
        {
            if (end.displacement > maximumSnapDistance ||
                Distance(start.point, end.point) <= kPointTolerance)
            {
                continue;
            }

            InferredInputs candidate = originalInputs;
            candidate.startPoint = start.point;
            candidate.endPoint = end.point;
            candidate.baseFace =
                PointOnFacePlane(face, candidate.startPoint) &&
                        PointOnFacePlane(face, candidate.endPoint)
                    ? face
                    : FindPlanarFaceContainingPoints(body,
                                                     candidate.startPoint,
                                                     candidate.endPoint);
            candidate.startEdge = nullptr;
            candidate.endEdge = nullptr;
            candidate.startPositiveYEdge = nullptr;
            candidate.startNegativeYEdge = nullptr;
            candidate.endPositiveYEdge = nullptr;
            candidate.endNegativeYEdge = nullptr;
            candidate.thickness = originalInputs.thickness;
            if (candidate.baseFace == nullptr || !CompleteInputsForEndpoints(candidate))
            {
                continue;
            }

            // P1 is expected to remain stable.  Give its displacement a larger
            // weight, then choose the valid updated P2 nearest the original P2.
            const double score = start.displacement * 10.0 + end.displacement;
            if (score < bestScore)
            {
                bestScore = score;
                refreshedInputs = candidate;
                found = true;
            }
        }
    }

    if (!found)
    {
        AppendDebugLog("RefreshSmartInputsAfterRips failed: no valid updated edge pair exists near the recorded P1/P2.");
        return false;
    }

    AppendDebugLog("RefreshSmartInputsAfterRips OK: recordedP1=" +
                   FormatPoint(originalInputs.startPoint) +
                   ", recordedP2=" + FormatPoint(originalInputs.endPoint) +
                   ", updatedP1=" + FormatPoint(refreshedInputs.startPoint) +
                   ", updatedP2=" + FormatPoint(refreshedInputs.endPoint) +
                   ", score=" + FormatExpressionNumber(bestScore));
    return true;
}

bool TwoPointSiBianUI::ConstrainRightAnglePrimaryP2ToOffsetSharedEdges(
    const InferredInputs& originalInputs,
    const InferredInputs& currentInputs,
    const std::vector<tag_t>& offsetFeatureTags,
    InferredInputs& constrainedInputs) const
{
    constrainedInputs = currentInputs;
    if (currentInputs.targetBody == nullptr || currentInputs.baseFace == nullptr ||
        offsetFeatureTags.empty())
    {
        return false;
    }

    std::set<tag_t> offsetFaceTags;
    for (tag_t featureTag : offsetFeatureTags)
    {
        if (featureTag == NULL_TAG)
        {
            continue;
        }
        Features::Feature* offsetFeature = dynamic_cast<Features::Feature*>(
            NXObjectManager::Get(featureTag));
        if (offsetFeature == nullptr)
        {
            continue;
        }
        for (Face* offsetFace : offsetFeature->GetFaces())
        {
            if (offsetFace != nullptr)
            {
                offsetFaceTags.insert(offsetFace->Tag());
            }
        }
    }
    if (offsetFaceTags.empty())
    {
        AppendDebugLog("Right-angle final P2 constraint failed: the offset features returned no faces.");
        return false;
    }

    struct SharedEndpointCandidate
    {
        Point3d point;
        tag_t edgeTag = NULL_TAG;
        double distanceToRecordedP2 = 0.0;
    };
    std::vector<SharedEndpointCandidate> candidates;
    for (Edge* planeEdge : currentInputs.baseFace->GetEdges())
    {
        if (planeEdge == nullptr)
        {
            continue;
        }
        bool sharedWithOffsetFace = false;
        for (Face* adjacentFace : planeEdge->GetFaces())
        {
            if (adjacentFace != nullptr &&
                adjacentFace != currentInputs.baseFace &&
                offsetFaceTags.find(adjacentFace->Tag()) != offsetFaceTags.end())
            {
                sharedWithOffsetFace = true;
                break;
            }
        }
        if (!sharedWithOffsetFace)
        {
            continue;
        }

        Point3d first;
        Point3d second;
        planeEdge->GetVertices(&first, &second);
        for (const Point3d& endpoint : std::array<Point3d, 2>{first, second})
        {
            bool duplicate = false;
            for (const SharedEndpointCandidate& existing : candidates)
            {
                if (Distance(existing.point, endpoint) <= kPointTolerance)
                {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate)
            {
                candidates.push_back({endpoint,
                                      planeEdge->Tag(),
                                      Distance(endpoint, originalInputs.endPoint)});
            }
        }
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const SharedEndpointCandidate& first,
                 const SharedEndpointCandidate& second)
              {
                  return first.distanceToRecordedP2 < second.distanceToRecordedP2;
              });
    const bool hasMovedOffsetEndpoint =
        std::any_of(candidates.begin(),
                    candidates.end(),
                    [](const SharedEndpointCandidate& candidate)
                    {
                        return candidate.distanceToRecordedP2 > kPointTolerance;
                    });
    for (const SharedEndpointCandidate& endpoint : candidates)
    {
        // Offset features can retain an unchanged edge at the recorded P2 in
        // their result-face set.  That zero-distance endpoint is not the end
        // of the moved rip-offset edge.  Prefer the nearest endpoint that was
        // actually displaced whenever one exists.
        // Ninety-left needs the displaced end of the rip-offset edge.  For
        // ninety-right the recorded P2 is the stable end of that same shared
        // edge; discarding it makes the UDF reference follow the moving end
        // when clearance is edited.
        if (currentInputs.featureMode == FeatureMode::NinetyLeft &&
            hasMovedOffsetEndpoint &&
            endpoint.distanceToRecordedP2 <= kPointTolerance)
        {
            AppendDebugLog("Right-angle final P2 skipped unchanged recorded endpoint: edge=" +
                           std::to_string(endpoint.edgeTag) +
                           ", point=" + FormatPoint(endpoint.point));
            continue;
        }
        if (Distance(currentInputs.startPoint, endpoint.point) <= kPointTolerance)
        {
            continue;
        }
        InferredInputs candidate = currentInputs;
        candidate.endPoint = endpoint.point;
        candidate.startEdge = nullptr;
        candidate.endEdge = nullptr;
        candidate.startPositiveYEdge = nullptr;
        candidate.startNegativeYEdge = nullptr;
        candidate.endPositiveYEdge = nullptr;
        candidate.endNegativeYEdge = nullptr;
        candidate.thickness = originalInputs.thickness;
        if (!CompleteInputsForEndpoints(candidate))
        {
            continue;
        }
        // Keep the actual plane/offset common edge as the right-angle UDF's
        // P2 parent.  CompleteInputsForEndpoints classifies signed edges and
        // may otherwise replace this edge with an unrelated outer Y+ edge
        // that happens to meet the same point.
        candidate.endEdge = dynamic_cast<Edge*>(
            NXObjectManager::Get(endpoint.edgeTag));
        constrainedInputs = candidate;
        AppendDebugLog("Right-angle final P2 constrained to plane/offset shared-edge endpoint: mode=" +
                       std::string(currentInputs.featureMode == FeatureMode::NinetyLeft
                                       ? "90-left"
                                       : "90-right") +
                       ", plane=" +
                       std::to_string(currentInputs.baseFace->Tag()) +
                       ", sharedEdge=" + std::to_string(endpoint.edgeTag) +
                       ", recordedP2=" + FormatPoint(originalInputs.endPoint) +
                       ", constrainedP2=" + FormatPoint(endpoint.point) +
                       ", distance=" +
                       FormatExpressionNumber(endpoint.distanceToRecordedP2) +
                       ", candidateCount=" + std::to_string(candidates.size()));
        return true;
    }

    AppendDebugLog("Right-angle final P2 constraint failed: no valid endpoint belongs to an edge shared by the plane and offset faces"
                   ", plane=" + std::to_string(currentInputs.baseFace->Tag()) +
                   ", offsetFeatureCount=" + std::to_string(offsetFeatureTags.size()) +
                   ", candidateCount=" + std::to_string(candidates.size()));
    return false;
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

    const double selectedFaceAreaSquareMillimeters =
        MeasureFaceArea(face);
    Part* workPart = session_->Parts()->Work();
    const double selectedFaceArea =
        SquareMillimetersToPartSquareUnits(
            workPart,
            selectedFaceAreaSquareMillimeters);
    const double minimumAcceptedInnerLoopArea =
        selectedFaceArea > kPointTolerance
            ? selectedFaceArea *
                  kSmartInnerLoopMinimumAreaRatio
            : std::numeric_limits<double>::infinity();
    AppendDebugLog(
        "InferEndpointsFromFaceClick inner-loop filter: face=" +
        std::to_string(face->Tag()) +
        ", faceAreaSquareMillimeters=" +
        FormatExpressionNumber(
            selectedFaceAreaSquareMillimeters) +
        ", faceAreaPartUnits=" +
        FormatExpressionNumber(selectedFaceArea) +
        ", minimumAcceptedInnerLoopArea=" +
        FormatExpressionNumber(
            minimumAcceptedInnerLoopArea));

    std::vector<Point3d> boundaryPoints;
    if (!FaceBoundaryPoints(face,
                            boundaryPoints,
                            minimumAcceptedInnerLoopArea) ||
        boundaryPoints.size() < 2)
    {
        AppendDebugLog("InferEndpointsFromFaceClick failed: not enough face boundary points.");
        return false;
    }

    const double expectedThickness =
        inputs.thickness > kPointTolerance
            ? inputs.thickness
            : EstimateSheetThickness(body, face);
    if (inputs.thickness <= kPointTolerance &&
        expectedThickness > kPointTolerance)
    {
        // Keep the first valid measurement. ReadInputs and all post-rip
        // refreshes must reuse this value instead of measuring a topologically
        // changed body again.
        inputs.thickness = expectedThickness;
        AppendDebugLog("InferEndpointsFromFaceClick cached the one-time "
                       "sheet thickness=" +
                       FormatExpressionNumber(inputs.thickness));
    }
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
            if ((!inputs.smartMode &&
                 (pairAngle + 1.0e-6 < kEndpointPairMinimumAngleDegrees ||
                  pairAngle - 1.0e-6 > kEndpointPairMaximumAngleDegrees)) ||
                (inputs.smartMode && pairAngle <= 1.0e-6))
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
        AppendDebugLog(inputs.smartMode
                           ? "InferEndpointsFromFaceClick failed: no usable nearest boundary point pair was found for smart mode."
                           : "InferEndpointsFromFaceClick failed: no nearest boundary point pair has a click angle in [150,180] degrees.");
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
          << (inputs.smartMode
                  ? " (smart mode: below 150 selects a 90-degree template)"
                  : " (manual mode required 150-180)")
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
    Face* bestContainingFace = nullptr;
    double bestContainingDistance = std::numeric_limits<double>::max();
    int bestContainmentStatus = 0;
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
        if (distance > kPlaneTolerance)
        {
            continue;
        }

        const double signedDistance =
            Dot(Subtract(point, planePoint), normal);
        const Point3d projectedPoint(
            point.X - normal.X * signedDistance,
            point.Y - normal.Y * signedDistance,
            point.Z - normal.Z * signedDistance);
        int containmentStatus = 0;
        if (!PointContainmentOnFace(face,
                                    projectedPoint,
                                    containmentStatus) ||
            (containmentStatus != 1 && containmentStatus != 3))
        {
            continue;
        }

        const bool betterContainment =
            bestContainingFace == nullptr ||
            (containmentStatus == 1 && bestContainmentStatus != 1) ||
            (containmentStatus == bestContainmentStatus &&
             distance < bestContainingDistance);
        if (betterContainment)
        {
            bestContainingFace = face;
            bestContainingDistance = distance;
            bestContainmentStatus = containmentStatus;
        }
    }

    if (bestContainingFace != nullptr)
    {
        AppendDebugLog(
            "FindPlanarFaceAtPoint selected containing face=" +
            std::to_string(bestContainingFace->Tag()) +
            ", containmentStatus=" +
            std::to_string(bestContainmentStatus) +
            ", planeDistance=" +
            FormatExpressionNumber(bestContainingDistance) +
            ", click=" + FormatPoint(point));
        return bestContainingFace;
    }
    if (bestDistance <= kPlaneTolerance && bestFace != nullptr)
    {
        AppendDebugLog(
            "FindPlanarFaceAtPoint containment unavailable; using nearest-plane fallback face=" +
            std::to_string(bestFace->Tag()) +
            ", planeDistance=" +
            FormatExpressionNumber(bestDistance) +
            ", click=" + FormatPoint(point));
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
                                                     double thickness,
                                                     const Point3d& qPoint) const
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
    double bestPointDistance = std::numeric_limits<double>::max();
    double bestThicknessError = std::numeric_limits<double>::max();
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
        if (error > tolerance)
        {
            continue;
        }

        // Several trimmed faces can lie on the same parallel sheet plane.
        // Choose the face whose accepted outer/large-inner-loop point is
        // geometrically closest to Q instead of relying on body face order.
        std::vector<Point3d> boundaryPoints;
        if (!FaceBoundaryPoints(candidate, boundaryPoints) ||
            boundaryPoints.empty())
        {
            continue;
        }
        double nearestPointDistance =
            std::numeric_limits<double>::max();
        for (const Point3d& boundaryPoint : boundaryPoints)
        {
            nearestPointDistance =
                std::min(nearestPointDistance,
                         Distance(boundaryPoint, qPoint));
        }

        std::ostringstream candidateTrace;
        candidateTrace
            << "FindParallelFaceAtThickness candidate=" << candidate->Tag()
            << ", planeDistance=" << distance
            << ", thicknessError=" << error
            << ", nearestBoundaryDistanceToQ=" << nearestPointDistance
            << ", boundaryPointCount=" << boundaryPoints.size();
        AppendDebugLog(candidateTrace.str());

        const bool closerToQ =
            nearestPointDistance + kPointTolerance <
            bestPointDistance;
        const bool sameQDistance =
            std::fabs(nearestPointDistance -
                      bestPointDistance) <= kPointTolerance;
        const bool betterThickness =
            error + kPointTolerance < bestThicknessError;
        const bool deterministicTie =
            sameQDistance &&
            std::fabs(error - bestThicknessError) <=
                kPointTolerance &&
            (best == nullptr || candidate->Tag() < best->Tag());
        if (best == nullptr || closerToQ ||
            (sameQDistance && betterThickness) ||
            deterministicTie)
        {
            best = candidate;
            bestPointDistance = nearestPointDistance;
            bestThicknessError = error;
        }
    }
    AppendDebugLog(
        "FindParallelFaceAtThickness selected face=" +
        std::to_string(best != nullptr ? best->Tag() : NULL_TAG) +
        ", Q=" + FormatPoint(qPoint) +
        ", nearestBoundaryDistanceToQ=" +
        FormatExpressionNumber(
            bestPointDistance <
                    std::numeric_limits<double>::max()
                ? bestPointDistance
                : -1.0) +
        ", thicknessError=" +
        FormatExpressionNumber(
            bestThicknessError <
                    std::numeric_limits<double>::max()
                ? bestThicknessError
                : -1.0));
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
                                                      sourceInputs.thickness,
                                                      qPoint);
    if (commonFace == nullptr || parallelFace == nullptr)
    {
        return false;
    }

    // Apply the same inner-loop rule used by the original P1/P2 smart
    // selection.  Small inner loops are ignored, while an inner loop whose
    // area is at least 15% of the selected parallel face remains eligible as
    // a local boundary.  Previously this call used the default argument,
    // which discarded every inner loop regardless of size.
    const double parallelFaceAreaSquareMillimeters =
        MeasureFaceArea(parallelFace);
    Part* workPart = session_->Parts()->Work();
    const double parallelFaceArea =
        SquareMillimetersToPartSquareUnits(
            workPart,
            parallelFaceAreaSquareMillimeters);
    const double minimumAcceptedInnerLoopArea =
        parallelFaceArea > kPointTolerance
            ? parallelFaceArea *
                  kSmartInnerLoopMinimumAreaRatio
            : std::numeric_limits<double>::infinity();
    AppendDebugLog(
        "P2 fallback parallel-face inner-loop filter: face=" +
        std::to_string(parallelFace->Tag()) +
        ", faceAreaSquareMillimeters=" +
        FormatExpressionNumber(
            parallelFaceAreaSquareMillimeters) +
        ", faceAreaPartUnits=" +
        FormatExpressionNumber(parallelFaceArea) +
        ", minimumAcceptedInnerLoopArea=" +
        FormatExpressionNumber(
            minimumAcceptedInnerLoopArea));

    std::vector<Point3d> boundaryPoints;
    if (!FaceBoundaryPoints(parallelFace,
                            boundaryPoints,
                            minimumAcceptedInnerLoopArea) ||
        boundaryPoints.size() < 2)
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
        if (distance > kPointTolerance && distance < q2Distance)
        {
            q2 = boundaryPoints[index];
            q2Distance = distance;
            foundQ2 = true;
        }
    }
    if (!foundQ2)
    {
        return false;
    }
    PointHasThicknessLengthEdge(sourceInputs.targetBody,
                                q2,
                                sourceInputs.thickness,
                                nullptr,
                                nullptr,
                                &q2ThicknessEdge);

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
                   std::to_string(q2ThicknessEdge != nullptr ? q2ThicknessEdge->Tag() : NULL_TAG) +
                   ", Q2Selection=nearest endpoint without thickness-edge requirement");
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
    const bool q3HasThicknessEdge =
        PointHasThicknessLengthEdge(sourceInputs.targetBody,
                                    q3,
                                    sourceInputs.thickness,
                                    nullptr,
                                    nullptr,
                                    &q3ThicknessEdge);

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
                    ", Q3ThicknessEdge=" +
                    std::to_string(q3ThicknessEdge != nullptr ? q3ThicknessEdge->Tag() : NULL_TAG) +
                     ", Q3HasThicknessEdge=" + (q3HasThicknessEdge ? "true" : "false") +
                     ", Q4=" + FormatPoint(q4) +
                     ", Q4DistanceFromQ3=" + FormatExpressionNumber(nearestQ4Distance) +
                     ", Q4Selection=provisional nearest peripheral endpoint excluding only Q3" +
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

std::vector<Edge*> TwoPointSiBianUI::FindReferenceConnectedEdges(
    Body* body,
    const Point3d& point) const
{
    std::vector<Edge*> result;
    std::set<tag_t> tags;
    if (body == nullptr)
    {
        return result;
    }
    for (Edge* edge : body->GetEdges())
    {
        if (edge != nullptr && EdgeTouchesPoint(edge, point) && tags.insert(edge->Tag()).second)
        {
            result.push_back(edge);
        }
    }
    return result;
}

Edge* TwoPointSiBianUI::FindReferenceCornerEdge(const InferredInputs& inputs) const
{
    if (inputs.targetBody == nullptr || inputs.baseFace == nullptr)
    {
        return nullptr;
    }

    struct PlaneCandidate
    {
        Edge* edge = nullptr;
        Vector3d direction;
    };
    std::vector<PlaneCandidate> planeCandidates;
    for (Edge* edge : inputs.baseFace->GetEdges())
    {
        if (edge == nullptr || !EdgeTouchesPoint(edge, inputs.startPoint))
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
        Vector3d direction = Distance(inputs.startPoint, first) <= Distance(inputs.startPoint, second)
                                 ? Subtract(second, inputs.startPoint)
                                 : Subtract(first, inputs.startPoint);
        if (Normalize(direction))
        {
            planeCandidates.push_back({edge, direction});
        }
    }
    if (planeCandidates.size() < 2)
    {
        AppendDebugLog("reference chamfer edge rejected: fewer than two base-face edges meet P1.");
        return nullptr;
    }

    std::size_t firstIndex = 0;
    std::size_t secondIndex = 1;
    double bestPairScore = -1.0;
    for (std::size_t first = 0; first < planeCandidates.size(); ++first)
    {
        for (std::size_t second = first + 1; second < planeCandidates.size(); ++second)
        {
            const double score = 1.0 - std::fabs(Dot(planeCandidates[first].direction,
                                                     planeCandidates[second].direction));
            if (score > bestPairScore)
            {
                bestPairScore = score;
                firstIndex = first;
                secondIndex = second;
            }
        }
    }

    const tag_t planeEdge1 = planeCandidates[firstIndex].edge->Tag();
    const tag_t planeEdge2 = planeCandidates[secondIndex].edge->Tag();
    Edge* bestEdge = nullptr;
    double bestLength = -1.0;
    for (Edge* edge : FindReferenceConnectedEdges(inputs.targetBody, inputs.startPoint))
    {
        if (edge == nullptr || edge->Tag() == planeEdge1 || edge->Tag() == planeEdge2)
        {
            continue;
        }
        const double length = edge->GetLength();
        if (length > bestLength)
        {
            bestLength = length;
            bestEdge = edge;
        }
    }
    AppendDebugLog("reference chamfer edge search: P1=" + FormatPoint(inputs.startPoint) +
                   ", planeEdge1=" + std::to_string(planeEdge1) +
                   ", planeEdge2=" + std::to_string(planeEdge2) +
                   ", cornerEdge=" +
                   std::to_string(bestEdge != nullptr ? bestEdge->Tag() : NULL_TAG));
    return bestEdge;
}

std::vector<Face*> TwoPointSiBianUI::FindReferencePlanarFaces(Edge* edge) const
{
    std::vector<Face*> result;
    std::set<tag_t> tags;
    if (edge == nullptr)
    {
        return result;
    }
    for (Face* face : edge->GetFaces())
    {
        try
        {
            if (face != nullptr && face->SolidFaceType() == Face::FaceTypePlanar &&
                tags.insert(face->Tag()).second)
            {
                result.push_back(face);
            }
        }
        catch (...)
        {
        }
    }
    std::sort(result.begin(), result.end(), [this](Face* left, Face* right) {
        return MeasureFaceArea(left) > MeasureFaceArea(right);
    });
    if (result.size() > 2)
    {
        result.resize(2);
    }
    return result;
}

bool TwoPointSiBianUI::ComputeReferenceInwardNormal(Body* body,
                                                     Face* face,
                                                     Vector3d& inwardNormal) const
{
    Point3d planeOrigin;
    Vector3d planeNormal;
    if (body == nullptr || !FacePlaneData(face, planeOrigin, planeNormal))
    {
        return false;
    }

    Point3d samplePoint;
    if (!FindFaceInteriorSamplePoint(face, samplePoint))
    {
        AppendDebugLog("reference inward-normal containment failed to find "
                       "a point strictly inside the trimmed face: body=" +
                       std::to_string(body->Tag()) +
                       ", face=" + std::to_string(face->Tag()));
        return false;
    }

    Vector3d surfaceNormal;
    if (!FaceNormalAtPoint(face, samplePoint, surfaceNormal))
    {
        surfaceNormal = planeNormal;
    }
    if (!Normalize(surfaceNormal))
    {
        return false;
    }

    constexpr double probeLength = 0.05;
    const Point3d positiveProbe =
        AddVector(samplePoint,
                  ScaleVector(surfaceNormal, probeLength));
    const Point3d negativeProbe =
        AddVector(samplePoint,
                  ScaleVector(surfaceNormal, -probeLength));
    double positiveCoordinates[3] = {
        positiveProbe.X, positiveProbe.Y, positiveProbe.Z};
    double negativeCoordinates[3] = {
        negativeProbe.X, negativeProbe.Y, negativeProbe.Z};
    int positiveStatus = 0;
    int negativeStatus = 0;
    const int positiveResult =
        UF_MODL_ask_point_containment(positiveCoordinates,
                                      body->Tag(),
                                      &positiveStatus);
    const int negativeResult =
        UF_MODL_ask_point_containment(negativeCoordinates,
                                      body->Tag(),
                                      &negativeStatus);
    const bool positiveInside =
        positiveResult == 0 && positiveStatus == 1;
    const bool negativeInside =
        negativeResult == 0 && negativeStatus == 1;

    std::ostringstream trace;
    trace << "reference inward-normal containment:"
          << " body=" << body->Tag()
          << ", face=" << face->Tag()
          << ", sample=" << FormatPoint(samplePoint)
          << ", surfaceNormal=" << FormatVector(surfaceNormal)
          << ", probeLength=" << probeLength
          << ", positiveProbe=" << FormatPoint(positiveProbe)
          << ", positiveResult=" << positiveResult
          << ", positiveStatus=" << positiveStatus
          << ", negativeProbe=" << FormatPoint(negativeProbe)
          << ", negativeResult=" << negativeResult
          << ", negativeStatus=" << negativeStatus;

    if (positiveInside != negativeInside)
    {
        inwardNormal = positiveInside
                           ? surfaceNormal
                           : ScaleVector(surfaceNormal, -1.0);
        trace << ", selected="
              << (positiveInside ? "positive" : "negative")
              << ", inwardNormal=" << FormatVector(inwardNormal);
        AppendDebugLog(trace.str());
        return true;
    }

    trace << ", selected=ambiguous; inward normal rejected";
    AppendDebugLog(trace.str());
    return false;
}

bool TwoPointSiBianUI::ExtrudeReferenceCornerProfile(
    const InferredInputs& inputs,
    const std::vector<Point3d>& profilePoints,
    const Point3d& origin,
    const Vector3d& direction,
    double startLimitValue,
    double endLimitValue,
    const char* operationName,
    std::string& errorMessage) const
{
    Part* workPart = session_ != nullptr ? session_->Parts()->Work() : nullptr;
    if (workPart == nullptr || profilePoints.size() < 4 ||
        inputs.targetBody == nullptr)
    {
        errorMessage = std::string(operationName) + ": invalid profile or target body.";
        return false;
    }

    Vector3d profileNormal = direction;
    if (!Normalize(profileNormal))
    {
        errorMessage = std::string(operationName) + ": the profile normal is invalid.";
        return false;
    }

    Vector3d fixedPlaneXAxis;
    bool hasFixedPlaneXAxis = false;
    for (std::size_t pointIndex = 1;
         pointIndex < profilePoints.size();
         ++pointIndex)
    {
        fixedPlaneXAxis = ProjectVectorToPlane(
            Subtract(profilePoints[pointIndex], profilePoints.front()),
            profileNormal);
        if (Normalize(fixedPlaneXAxis))
        {
            hasFixedPlaneXAxis = true;
            break;
        }
    }
    if (!hasFixedPlaneXAxis)
    {
        Vector3d seed(std::fabs(profileNormal.X) < 0.9 ? 1.0 : 0.0,
                      std::fabs(profileNormal.X) < 0.9 ? 0.0 : 1.0,
                      0.0);
        fixedPlaneXAxis = Cross(seed, profileNormal);
        hasFixedPlaneXAxis = Normalize(fixedPlaneXAxis);
    }
    Vector3d fixedPlaneYAxis = Cross(profileNormal, fixedPlaneXAxis);
    if (!hasFixedPlaneXAxis || !Normalize(fixedPlaneYAxis))
    {
        errorMessage = std::string(operationName) +
                       ": the fixed sketch-plane axes are invalid.";
        return false;
    }
    fixedPlaneXAxis = Cross(fixedPlaneYAxis, profileNormal);
    if (!Normalize(fixedPlaneXAxis))
    {
        errorMessage = std::string(operationName) +
                       ": the fixed sketch-plane X axis is invalid.";
        return false;
    }
    Matrix3x3 fixedPlaneMatrix;
    fixedPlaneMatrix.Xx = fixedPlaneXAxis.X;
    fixedPlaneMatrix.Xy = fixedPlaneXAxis.Y;
    fixedPlaneMatrix.Xz = fixedPlaneXAxis.Z;
    fixedPlaneMatrix.Yx = fixedPlaneYAxis.X;
    fixedPlaneMatrix.Yy = fixedPlaneYAxis.Y;
    fixedPlaneMatrix.Yz = fixedPlaneYAxis.Z;
    fixedPlaneMatrix.Zx = profileNormal.X;
    fixedPlaneMatrix.Zy = profileNormal.Y;
    fixedPlaneMatrix.Zz = profileNormal.Z;

    SketchInPlaceBuilder* sketchBuilder = nullptr;
    SimpleSketchInPlaceBuilder* simpleSketchBuilder = nullptr;
    Sketch* profileSketch = nullptr;
    Features::Feature* sketchFeature = nullptr;
    Features::ExtrudeBuilder* extrudeBuilder = nullptr;
    Features::Feature* extrudeFeature = nullptr;
    tag_t sketchFeatureTag = NULL_TAG;
    tag_t extrudeFeatureTag = NULL_TAG;
    tag_t placementPointTag = NULL_TAG;
    tag_t placementPlaneTag = NULL_TAG;
    tag_t placementCsysTag = NULL_TAG;
    tag_t sketchAxisDirectionTag = NULL_TAG;
    tag_t extrusionDirectionTag = NULL_TAG;
    std::vector<tag_t> sketchCurveTags;
    Session::UndoMarkId localUndoMark = static_cast<Session::UndoMarkId>(0);
    std::string failureStage = "initialization";
    try
    {
        localUndoMark = session_->SetUndoMark(
            Session::MarkVisibilityInvisible,
            operationName);
    }
    catch (...)
    {
        localUndoMark = static_cast<Session::UndoMarkId>(0);
    }

    auto deleteFeatureIfAlive = [](tag_t featureTag) -> bool
    {
        if (featureTag == NULL_TAG || UF_OBJ_ask_status(featureTag) != UF_OBJ_ALIVE)
        {
            return true;
        }
        UF_OBJ_delete_object(featureTag);
        return UF_OBJ_ask_status(featureTag) != UF_OBJ_ALIVE;
    };
    auto deleteLocalUndoMark = [&]()
    {
        if (localUndoMark == static_cast<Session::UndoMarkId>(0))
        {
            return;
        }
        try
        {
            session_->DeleteUndoMark(localUndoMark, operationName);
        }
        catch (...)
        {
        }
        localUndoMark = static_cast<Session::UndoMarkId>(0);
    };
    auto cleanupFailedFeatures = [&]() -> bool
    {
        if (extrudeBuilder != nullptr)
        {
            try
            {
                extrudeBuilder->Destroy();
            }
            catch (...)
            {
            }
            extrudeBuilder = nullptr;
        }
        if (sketchBuilder != nullptr)
        {
            try
            {
                sketchBuilder->Destroy();
            }
            catch (...)
            {
            }
            sketchBuilder = nullptr;
        }
        if (simpleSketchBuilder != nullptr)
        {
            try
            {
                simpleSketchBuilder->Destroy();
            }
            catch (...)
            {
            }
            simpleSketchBuilder = nullptr;
        }
        if (localUndoMark != static_cast<Session::UndoMarkId>(0))
        {
            try
            {
                session_->UndoToMark(localUndoMark, operationName);
                session_->DeleteUndoMark(localUndoMark, operationName);
                localUndoMark = static_cast<Session::UndoMarkId>(0);
                return true;
            }
            catch (const NXException& ex)
            {
                AppendDebugLog(std::string(operationName) +
                               " local rollback warning: " + UfMessage(ex.ErrorCode()));
                localUndoMark = static_cast<Session::UndoMarkId>(0);
            }
            catch (...)
            {
                AppendDebugLog(std::string(operationName) +
                               " local rollback warning: unknown exception.");
                localUndoMark = static_cast<Session::UndoMarkId>(0);
            }
        }
        const bool extrudeDeleted = deleteFeatureIfAlive(extrudeFeatureTag);
        const bool sketchDeleted = extrudeDeleted && deleteFeatureIfAlive(sketchFeatureTag);
        if (!sketchDeleted)
        {
            AppendDebugLog(std::string(operationName) +
                           " fallback cleanup retained live dependent sketch history to avoid dangling parents.");
            return false;
        }
        bool cleanupComplete = true;
        for (tag_t curveTag : sketchCurveTags)
        {
            if (curveTag != NULL_TAG && UF_OBJ_ask_status(curveTag) == UF_OBJ_ALIVE)
            {
                UF_OBJ_delete_object(curveTag);
                cleanupComplete = cleanupComplete &&
                                  UF_OBJ_ask_status(curveTag) != UF_OBJ_ALIVE;
            }
        }
        if (extrusionDirectionTag != NULL_TAG &&
            UF_OBJ_ask_status(extrusionDirectionTag) == UF_OBJ_ALIVE)
        {
            UF_OBJ_delete_object(extrusionDirectionTag);
            cleanupComplete = cleanupComplete &&
                              UF_OBJ_ask_status(extrusionDirectionTag) != UF_OBJ_ALIVE;
        }
        if (placementPointTag != NULL_TAG &&
            UF_OBJ_ask_status(placementPointTag) == UF_OBJ_ALIVE)
        {
            UF_OBJ_delete_object(placementPointTag);
            cleanupComplete = cleanupComplete &&
                              UF_OBJ_ask_status(placementPointTag) != UF_OBJ_ALIVE;
        }
        if (sketchAxisDirectionTag != NULL_TAG &&
            UF_OBJ_ask_status(sketchAxisDirectionTag) == UF_OBJ_ALIVE)
        {
            UF_OBJ_delete_object(sketchAxisDirectionTag);
            cleanupComplete = cleanupComplete &&
                              UF_OBJ_ask_status(sketchAxisDirectionTag) != UF_OBJ_ALIVE;
        }
        if (placementPlaneTag != NULL_TAG &&
            UF_OBJ_ask_status(placementPlaneTag) == UF_OBJ_ALIVE)
        {
            UF_OBJ_delete_object(placementPlaneTag);
            cleanupComplete = cleanupComplete &&
                              UF_OBJ_ask_status(placementPlaneTag) != UF_OBJ_ALIVE;
        }
        if (placementCsysTag != NULL_TAG &&
            UF_OBJ_ask_status(placementCsysTag) == UF_OBJ_ALIVE)
        {
            UF_OBJ_delete_object(placementCsysTag);
            cleanupComplete = cleanupComplete &&
                              UF_OBJ_ask_status(placementCsysTag) != UF_OBJ_ALIVE;
        }
        return cleanupComplete;
    };

    try
    {
        failureStage = "fixed sketch plane creation";
        const bool isFarTopCut =
            operationName != nullptr &&
            std::strcmp(operationName,
                        "reference far-end top-corner cut") == 0;
        Plane* placementPlane = nullptr;
        CartesianCoordinateSystem* placementCsys = nullptr;
        if (isFarTopCut)
        {
            // The far-cut sketch must survive opaque CustomFeature updates.
            // A temporary CSYS is not saved, and a Plane smart object derived
            // from a datum feature can still be condemned when the sketch is
            // made internal.  Use a non-temporary saved CSYS directly as the
            // SimpleSketch placement object, avoiding an intermediate Plane.
            placementCsys = workPart->CoordinateSystems()->CreateCoordinateSystem(
                origin,
                fixedPlaneMatrix,
                false);
            if (placementCsys == nullptr)
            {
                throw std::runtime_error(
                    "NX did not create the persistent far-cut sketch coordinate system.");
            }
            placementCsysTag = placementCsys->Tag();
            placementCsys->SetVisibility(
                SmartObject::VisibilityOptionInvisible);
            AppendDebugLog(
                "far-end top-cut sketch uses saved non-temporary coordinate system: csys=" +
                std::to_string(placementCsysTag));
        }
        else
        {
            placementPlane = workPart->Planes()->CreatePlane(
                origin,
                profileNormal,
                SmartObject::UpdateOptionWithinModeling);
        }
        Direction* sketchAxisDirection = workPart->Directions()->CreateDirection(
            origin,
            fixedPlaneXAxis,
            SmartObject::UpdateOptionWithinModeling);
        Point* placementPoint = workPart->Points()->CreatePoint(origin);
        if ((!isFarTopCut && placementPlane == nullptr) ||
            sketchAxisDirection == nullptr ||
            placementPoint == nullptr)
        {
            throw std::runtime_error("NX did not create the corner-profile sketch placement.");
        }
        placementPlaneTag = placementPlane != nullptr
                                ? placementPlane->Tag()
                                : NULL_TAG;
        sketchAxisDirectionTag = sketchAxisDirection->Tag();
        placementPointTag = placementPoint->Tag();
        if (placementPlane != nullptr)
        {
            placementPlane->SetVisibility(SmartObject::VisibilityOptionInvisible);
        }
        sketchAxisDirection->SetVisibility(SmartObject::VisibilityOptionInvisible);
        placementPoint->SetVisibility(SmartObject::VisibilityOptionInvisible);

        failureStage = "fixed sketch commit";
        NXObject* committedSketchObject = nullptr;
        std::vector<NXObject*> committedSketchObjects;
        if (isFarTopCut)
        {
            simpleSketchBuilder =
                workPart->Sketches()->CreateSimpleSketchInPlaceBuilder();
            simpleSketchBuilder->SetUseWorkPartOrigin(false);
            simpleSketchBuilder->SetCoordinateSystem(placementCsys);
            simpleSketchBuilder->SetSketchOrigin(placementPoint);
            committedSketchObject = simpleSketchBuilder->Commit();
            committedSketchObjects = simpleSketchBuilder->GetCommittedObjects();
        }
        else
        {
            sketchBuilder = workPart->Sketches()->CreateSketchInPlaceBuilder2(nullptr);
            sketchBuilder->SetPlaneOption(Sketch::PlaneOptionExistingPlane);
            sketchBuilder->SetPlaneReference(placementPlane);
            sketchBuilder->SetAxisReference(sketchAxisDirection);
            sketchBuilder->SetOriginOption(OriginMethodSpecifyPoint);
            sketchBuilder->SetSketchOrigin(placementPoint);
            committedSketchObject = sketchBuilder->Commit();
            committedSketchObjects = sketchBuilder->GetCommittedObjects();
        }
        profileSketch = dynamic_cast<Sketch*>(committedSketchObject);
        Features::SketchFeature* committedSketchFeature =
            dynamic_cast<Features::SketchFeature*>(committedSketchObject);
        if (committedSketchFeature != nullptr)
        {
            sketchFeature = committedSketchFeature;
            profileSketch = committedSketchFeature->Sketch();
        }
        if (sketchFeature == nullptr && profileSketch != nullptr)
        {
            sketchFeature = profileSketch->Feature();
        }
        if (sketchFeature != nullptr)
        {
            sketchFeatureTag = sketchFeature->Tag();
        }
        for (NXObject* committedObject : committedSketchObjects)
        {
            if (profileSketch == nullptr)
            {
                profileSketch = dynamic_cast<Sketch*>(committedObject);
            }
            if (sketchFeature == nullptr)
            {
                Features::SketchFeature* candidate =
                    dynamic_cast<Features::SketchFeature*>(committedObject);
                if (candidate != nullptr)
                {
                    sketchFeature = candidate;
                    sketchFeatureTag = candidate->Tag();
                    profileSketch = candidate->Sketch();
                }
            }
        }
        if (simpleSketchBuilder != nullptr)
        {
            simpleSketchBuilder->Destroy();
            simpleSketchBuilder = nullptr;
        }
        if (sketchBuilder != nullptr)
        {
            sketchBuilder->Destroy();
            sketchBuilder = nullptr;
        }
        if (profileSketch == nullptr)
        {
            throw std::runtime_error("NX did not create the corner-profile sketch.");
        }
        if (sketchFeature == nullptr)
        {
            sketchFeature = profileSketch->Feature();
        }
        if (sketchFeature == nullptr)
        {
            throw std::runtime_error("NX did not return the corner-profile sketch feature.");
        }
        sketchFeatureTag = sketchFeature->Tag();

        failureStage = "sketch-plane coordinate resolution";
        const Point3d sketchOrigin = profileSketch->Origin();
        NXMatrix* sketchOrientationObject = profileSketch->Orientation();
        if (sketchOrientationObject == nullptr)
        {
            throw std::runtime_error("NX did not return the corner-profile sketch orientation.");
        }
        const Matrix3x3 sketchOrientation = sketchOrientationObject->Element();
        Vector3d sketchXAxis(sketchOrientation.Xx,
                             sketchOrientation.Xy,
                             sketchOrientation.Xz);
        Vector3d sketchYAxis(sketchOrientation.Yx,
                             sketchOrientation.Yy,
                             sketchOrientation.Yz);
        Vector3d sketchNormal(sketchOrientation.Zx,
                              sketchOrientation.Zy,
                              sketchOrientation.Zz);
        AppendDebugLog(std::string(operationName) +
                       " committed sketch orientation: origin=" +
                       FormatPoint(sketchOrigin) +
                       ", X=" + FormatVector(sketchXAxis) +
                       ", Y=" + FormatVector(sketchYAxis) +
                       ", Z=" + FormatVector(sketchNormal) +
                       ", requestedZ=" + FormatVector(profileNormal));
        if (!Normalize(sketchXAxis) ||
            !Normalize(sketchYAxis) ||
            !Normalize(sketchNormal) ||
            std::fabs(Dot(sketchNormal, profileNormal)) < 0.999999)
        {
            throw std::runtime_error("NX created a sketch plane that does not match the corner-profile plane.");
        }

        std::vector<Point3d> projectedProfilePoints;
        projectedProfilePoints.reserve(profilePoints.size());
        double maximumInputPlaneOffset = 0.0;
        double maximumProjectedPlaneOffset = 0.0;
        for (const Point3d& profilePoint : profilePoints)
        {
            const Vector3d fromSketchOrigin = Subtract(profilePoint, sketchOrigin);
            maximumInputPlaneOffset =
                std::max(maximumInputPlaneOffset,
                         std::fabs(Dot(fromSketchOrigin, sketchNormal)));
            const Point3d projectedPoint = AddVector(
                AddVector(sketchOrigin,
                          ScaleVector(sketchXAxis,
                                      Dot(fromSketchOrigin, sketchXAxis))),
                ScaleVector(sketchYAxis,
                            Dot(fromSketchOrigin, sketchYAxis)));
            maximumProjectedPlaneOffset =
                std::max(maximumProjectedPlaneOffset,
                         std::fabs(Dot(Subtract(projectedPoint, sketchOrigin),
                                       sketchNormal)));
            projectedProfilePoints.push_back(projectedPoint);
        }
        AppendDebugLog(std::string(operationName) +
                       " sketch-plane projection: requestedOrigin=" + FormatPoint(origin) +
                       ", actualOrigin=" + FormatPoint(sketchOrigin) +
                       ", requestedNormal=" + FormatVector(profileNormal) +
                       ", actualNormal=" + FormatVector(sketchNormal) +
                       ", normalDot=" +
                       FormatExpressionNumber(Dot(sketchNormal, profileNormal)) +
                       ", maximumInputOffset=" +
                       FormatExpressionNumber(maximumInputPlaneOffset) +
                       ", maximumProjectedOffset=" +
                       FormatExpressionNumber(maximumProjectedPlaneOffset));

        failureStage = "sketch geometry creation";
        profileSketch->Activate(Sketch::ViewReorientFalse);
        for (std::size_t index = 0; index + 1 < projectedProfilePoints.size(); ++index)
        {
            failureStage = "sketch geometry line " + std::to_string(index + 1);
            Line* line = workPart->Curves()->CreateLine(projectedProfilePoints[index],
                                                        projectedProfilePoints[index + 1]);
            if (line == nullptr)
            {
                throw std::runtime_error("NX did not create a corner-profile sketch line.");
            }
            sketchCurveTags.push_back(line->Tag());
            profileSketch->AddGeometry(line,
                                       Sketch::InferConstraintsOptionInferNoConstraints);
        }
        failureStage = "sketch update";
        profileSketch->Update();
        profileSketch->UpdateNavigator();
        profileSketch->Deactivate(Sketch::ViewReorientFalse,
                                  Sketch::UpdateLevelModel);

        std::vector<Curve*> profileCurves;
        for (NXObject* geometry : profileSketch->GetAllGeometry())
        {
            Curve* curve = dynamic_cast<Curve*>(geometry);
            if (curve != nullptr)
            {
                profileCurves.push_back(curve);
            }
        }
        if (profileCurves.empty())
        {
            throw std::runtime_error("The corner-profile sketch contains no curves.");
        }

        failureStage = "extrusion setup";
        extrudeBuilder = workPart->Features()->CreateExtrudeBuilder(nullptr);
        Section* section = workPart->Sections()->CreateSection(9.5e-05, 0.0001, 0.5);
        extrudeBuilder->SetSection(section);
        extrudeBuilder->AllowSelfIntersectingSection(true);
        extrudeBuilder->SetDistanceTolerance(0.0001);
        extrudeBuilder->BooleanOperation()->SetType(
            GeometricUtilities::BooleanOperation::BooleanTypeCreate);
        extrudeBuilder->SmartVolumeProfile()->SetOpenProfileSmartVolumeOption(false);
        extrudeBuilder->SmartVolumeProfile()->SetCloseProfileRule(
            GeometricUtilities::SmartVolumeProfileBuilder::CloseProfileRuleTypeFci);
        extrudeBuilder->Limits()->SetSymmetricOption(false);
        extrudeBuilder->Limits()->StartExtend()->Value()->SetFormula(
            FormatExpressionNumber(startLimitValue).c_str());
        extrudeBuilder->Limits()->EndExtend()->Value()->SetFormula(
            FormatExpressionNumber(endLimitValue).c_str());
        extrudeBuilder->Limits()->StartExtend()->SetTrimType(
            GeometricUtilities::Extend::ExtendTypeValue);
        extrudeBuilder->Limits()->EndExtend()->SetTrimType(
            GeometricUtilities::Extend::ExtendTypeValue);
        extrudeBuilder->Offset()->SetOption(GeometricUtilities::TypeNoOffset);
        extrudeBuilder->Offset()->StartOffset()->SetFormula("0");
        extrudeBuilder->Offset()->EndOffset()->SetFormula("0");
        extrudeBuilder->FeatureOptions()->SetBodyType(
            GeometricUtilities::FeatureOptions::BodyStyleSolid);

        section->SetDistanceTolerance(0.0001);
        section->SetChainingTolerance(9.5e-05);
        section->SetAllowedEntityTypes(Section::AllowTypesOnlyCurves);
        section->AllowSelfIntersection(true);
        section->AllowDegenerateCurves(false);

        CurveFeatureRule* profileRule =
            workPart->ScRuleFactory()->CreateRuleCurveFeature(
                std::vector<Features::Feature*>{sketchFeature});
        section->AddToSection(
            std::vector<SelectionIntentRule*>{profileRule},
            profileCurves.front(),
            nullptr,
            nullptr,
            projectedProfilePoints.front(),
            Section::ModeCreate,
            false);

        Direction* extrusionDirection = workPart->Directions()->CreateDirection(
            origin,
            profileNormal,
            SmartObject::UpdateOptionWithinModeling);
        if (extrusionDirection == nullptr)
        {
            throw std::runtime_error("NX did not create the corner-profile extrusion direction.");
        }
        extrusionDirectionTag = extrusionDirection->Tag();
        extrudeBuilder->SetDirection(extrusionDirection);
        extrudeBuilder->SetParentFeatureInternal(sketchFeature);

        failureStage = "extrusion commit";
        extrudeFeature = extrudeBuilder->CommitFeature();
        if (extrudeFeature != nullptr)
        {
            extrudeFeatureTag = extrudeFeature->Tag();
        }
        extrudeBuilder->Destroy();
        extrudeBuilder = nullptr;
        if (extrudeFeature == nullptr)
        {
            throw std::runtime_error("NX did not create the corner-profile extrusion.");
        }
        try
        {
            if (!sketchFeature->IsInternal())
            {
                extrudeFeature->MakeSketchInternal();
            }
        }
        catch (const NXException& ex)
        {
            AppendDebugLog(std::string(operationName) +
                           " MakeSketchInternal warning: " + UfMessage(ex.ErrorCode()));
        }
        if (!sketchFeature->IsInternal())
        {
            throw std::runtime_error("NX did not place the profile sketch inside the extrusion.");
        }

        if (operationName != nullptr &&
            std::strcmp(operationName,
                        "reference far-end top-corner cut") == 0)
        {
            SetConstructionRole(
                extrudeFeature,
                zhihui_twopoint_sibian::kRoleExtrudeFarTopCut);
            AppendDebugLog(
                "tagged the far-end top-corner extrusion for bend-R/clearance editing: extrude=" +
                std::to_string(extrudeFeatureTag));
        }
        else if (operationName != nullptr &&
                 std::strcmp(operationName,
                             "reference 90-degree closed-profile corner-edge cut") == 0)
        {
            SetConstructionRole(
                extrudeFeature,
                zhihui_twopoint_sibian::kRoleExtrudeCornerEdgeCut);
            AppendDebugLog(
                "tagged the closed-profile reference corner-edge extrusion for clearance editing: extrude=" +
                std::to_string(extrudeFeatureTag));
        }
        else if (operationName != nullptr &&
                 std::strcmp(operationName,
                             "90-right oblique clearance rectangle cut") == 0)
        {
            SetConstructionRole(
                extrudeFeature,
                zhihui_twopoint_sibian::kRoleExtrudeRightClearanceRectangle);
            AppendDebugLog(
                "tagged the 90-right clearance rectangle extrusion for clearance editing: extrude=" +
                std::to_string(extrudeFeatureTag));
        }
        else if (operationName != nullptr &&
                 std::strcmp(operationName,
                             "90-left oblique clearance rectangle cut") == 0)
        {
            SetConstructionRole(
                extrudeFeature,
                zhihui_twopoint_sibian::kRoleExtrudeLeftClearanceRectangle);
            AppendDebugLog(
                "tagged the 90-left clearance rectangle extrusion for clearance editing: extrude=" +
                std::to_string(extrudeFeatureTag));
        }

        failureStage = "extrusion body resolution";
        tag_t toolBodyTag = NULL_TAG;
        const int bodyStatus = UF_MODL_ask_feat_body(extrudeFeatureTag, &toolBodyTag);
        if (bodyStatus != 0 || toolBodyTag == NULL_TAG)
        {
            throw NXException::Create(bodyStatus != 0 ? bodyStatus : 1);
        }

        failureStage = "corner-profile subtraction";
        tag_t subtractTag = NULL_TAG;
        if (!SubtractToolBodies(inputs.targetBody,
                                std::vector<tag_t>{toolBodyTag},
                                subtractTag,
                                errorMessage))
        {
            if (!cleanupFailedFeatures())
            {
                errorMessage = "Internal corner-profile rollback failed; canceling the entire preview.\n" +
                               errorMessage;
            }
            return false;
        }
        deleteLocalUndoMark();
        AppendDebugLog(std::string(operationName) +
                       " completed with internal sketch: sketch=" +
                       std::to_string(sketchFeatureTag) +
                       ", extrude=" + std::to_string(extrudeFeatureTag) +
                       ", subtract=" + std::to_string(subtractTag));
        return true;
    }
    catch (const NXException& ex)
    {
        errorMessage = std::string(operationName) +
                       ": failed to create the internal-sketch extrusion at " +
                       failureStage + ".\n" +
                       NxExceptionText(ex);
    }
    catch (const std::exception& ex)
    {
        errorMessage = std::string(operationName) +
                       ": failed to create the internal-sketch extrusion at " +
                       failureStage + ".\n" + ex.what();
    }
    catch (...)
    {
        errorMessage = std::string(operationName) +
                       ": failed to create the internal-sketch extrusion at " +
                       failureStage + ".";
    }
    try
    {
        if (profileSketch != nullptr && profileSketch->IsActive())
        {
            profileSketch->Deactivate(Sketch::ViewReorientFalse,
                                      Sketch::UpdateLevelModel);
        }
    }
    catch (...)
    {
    }
    if (!cleanupFailedFeatures())
    {
        errorMessage = "Internal corner-profile rollback failed; canceling the entire preview.\n" +
                       errorMessage;
    }
    AppendDebugLog(errorMessage);
    return false;
}

bool TwoPointSiBianUI::CreateReferenceNonRightCornerEdgeCut(
    const InferredInputs& inputs,
    Edge* cornerEdge,
    double angleDegrees,
    const std::vector<Face*>& principalFaces,
    const std::vector<Vector3d>& inwardNormals,
    std::string& errorMessage) const
{
    if (inputs.targetBody == nullptr || cornerEdge == nullptr ||
        principalFaces.size() < 2 || inwardNormals.size() < 2)
    {
        errorMessage = "Reference chamfer: incomplete non-right-angle inputs.";
        return false;
    }
    double clearance = 0.0;
    double bendRadius = 0.0;
    if (!TryParseExpressionNumber(inputs.clearanceValue, clearance) ||
        !TryParseExpressionNumber(inputs.bendRadiusValue, bendRadius))
    {
        errorMessage = "Reference chamfer: clearance or bend radius is not numeric.";
        return false;
    }
    clearance = std::fabs(clearance);
    bendRadius = std::fabs(bendRadius);

    Point3d first;
    Point3d second;
    try
    {
        cornerEdge->GetVertices(&first, &second);
    }
    catch (...)
    {
        errorMessage = "Reference chamfer: the corner-edge endpoints are unavailable.";
        return false;
    }
    Point3d profileOrigin = first;
    Point3d otherEndpoint = second;
    if (Distance(second, inputs.startPoint) < Distance(first, inputs.startPoint))
    {
        profileOrigin = second;
        otherEndpoint = first;
    }
    Vector3d edgeDirection = Subtract(otherEndpoint, profileOrigin);
    const double edgeLength = Length(edgeDirection);
    if (!Normalize(edgeDirection) || edgeLength <= kPointTolerance)
    {
        errorMessage = "Reference chamfer: the corner-edge direction is invalid.";
        return false;
    }

    Point3d facePoint1;
    Point3d facePoint2;
    Vector3d faceNormal1;
    Vector3d faceNormal2;
    if (!FacePlaneData(principalFaces[0], facePoint1, faceNormal1) ||
        !FacePlaneData(principalFaces[1], facePoint2, faceNormal2))
    {
        errorMessage = "Reference chamfer: the corner-edge face planes are unavailable.";
        return false;
    }
    Vector3d faceDirection1 = ProjectVectorToPlane(Subtract(facePoint1, profileOrigin), edgeDirection);
    Vector3d faceDirection2 = ProjectVectorToPlane(Subtract(facePoint2, profileOrigin), edgeDirection);
    if (!Normalize(faceDirection1))
    {
        faceDirection1 = Cross(edgeDirection, faceNormal1);
    }
    if (!Normalize(faceDirection2))
    {
        faceDirection2 = Cross(edgeDirection, faceNormal2);
    }
    if (!Normalize(faceDirection1) || !Normalize(faceDirection2))
    {
        errorMessage = "Reference chamfer: the two face directions are invalid.";
        return false;
    }

    std::vector<Point3d> profilePoints;
    const double offsetDistance = inputs.thickness + bendRadius + clearance * 0.5;
    bool explicitProfileCreated = false;
    Vector3d centerDirection =
        ProjectVectorToPlane(Subtract(inputs.endPoint, inputs.startPoint), edgeDirection);
    if (Normalize(centerDirection))
    {
        if (Dot(centerDirection, faceDirection1) < 0.0)
        {
            faceDirection1 = ScaleVector(faceDirection1, -1.0);
        }
        if (Dot(centerDirection, faceDirection2) < 0.0)
        {
            faceDirection2 = ScaleVector(faceDirection2, -1.0);
        }
        auto evaluateGapProfile = [&](double centerDistance,
                                      std::vector<Point3d>* points,
                                      double* gap) -> bool {
            const Point3d cPoint = AddVector(profileOrigin,
                                              ScaleVector(centerDirection, centerDistance));
            const double edgeParameterA = Dot(Subtract(cPoint, profileOrigin), faceDirection1);
            const double edgeParameterE = Dot(Subtract(cPoint, profileOrigin), faceDirection2);
            if (edgeParameterA <= kPointTolerance || edgeParameterE <= kPointTolerance)
            {
                return false;
            }
            const Point3d aPoint = AddVector(profileOrigin,
                                              ScaleVector(faceDirection1, edgeParameterA));
            const Point3d ePoint = AddVector(profileOrigin,
                                              ScaleVector(faceDirection2, edgeParameterE));
            Vector3d aToC = Subtract(cPoint, aPoint);
            Vector3d eToC = Subtract(cPoint, ePoint);
            if (Length(aToC) <= inputs.thickness + kPointTolerance ||
                Length(eToC) <= inputs.thickness + kPointTolerance ||
                !Normalize(aToC) || !Normalize(eToC))
            {
                return false;
            }
            const Point3d bPoint = AddVector(aPoint, ScaleVector(aToC, inputs.thickness));
            const Point3d dPoint = AddVector(ePoint, ScaleVector(eToC, inputs.thickness));
            if (gap != nullptr) *gap = Distance(bPoint, dPoint);
            if (points != nullptr)
            {
                *points = {profileOrigin, aPoint, cPoint, ePoint, profileOrigin};
            }
            return true;
        };

        double lowDistance = std::max(inputs.thickness, clearance) + kPointTolerance;
        double lowGap = 0.0;
        for (int index = 0; index < 80; ++index)
        {
            if (evaluateGapProfile(lowDistance, nullptr, &lowGap)) break;
            lowDistance *= 1.25;
        }
        double highDistance = lowDistance;
        double highGap = lowGap;
        for (int index = 0; index < 80 && highGap < clearance; ++index)
        {
            highDistance *= 1.5;
            if (!evaluateGapProfile(highDistance, nullptr, &highGap)) highGap = 0.0;
        }
        if (highGap >= clearance && clearance > kPointTolerance)
        {
            for (int index = 0; index < 80; ++index)
            {
                const double middle = (lowDistance + highDistance) * 0.5;
                double middleGap = 0.0;
                if (evaluateGapProfile(middle, nullptr, &middleGap) && middleGap >= clearance)
                {
                    highDistance = middle;
                }
                else
                {
                    lowDistance = middle;
                }
            }
            double finalGap = 0.0;
            explicitProfileCreated = evaluateGapProfile(highDistance, &profilePoints, &finalGap);
            if (explicitProfileCreated)
            {
                AppendDebugLog("reference non-right chamfer profile=OACEO, targetGap=" +
                               FormatExpressionNumber(clearance) +
                               ", actualGap=" + FormatExpressionNumber(finalGap));
            }
        }
    }

    if (!explicitProfileCreated && angleDegrees < 179.0)
    {
        Vector3d bisector(inwardNormals[0].X + inwardNormals[1].X,
                          inwardNormals[0].Y + inwardNormals[1].Y,
                          inwardNormals[0].Z + inwardNormals[1].Z);
        bisector = ProjectVectorToPlane(bisector, edgeDirection);
        if (!Normalize(bisector))
        {
            bisector = Vector3d(faceDirection1.X + faceDirection2.X,
                                faceDirection1.Y + faceDirection2.Y,
                                faceDirection1.Z + faceDirection2.Z);
        }
        if (!Normalize(bisector))
        {
            errorMessage = "Reference chamfer: the acute-angle bisector is invalid.";
            return false;
        }
        const Vector3d offset1 = InwardOffsetDirectionInPlane(faceDirection1,
                                                               edgeDirection,
                                                               bisector);
        const Vector3d offset2 = InwardOffsetDirectionInPlane(faceDirection2,
                                                               edgeDirection,
                                                               bisector);
        const Point3d offsetOrigin1 = AddVector(profileOrigin,
                                                 ScaleVector(offset1, offsetDistance));
        const Point3d offsetOrigin2 = AddVector(profileOrigin,
                                                 ScaleVector(offset2, offsetDistance));
        Point3d topPoint;
        if (Length(offset1) <= kPointTolerance || Length(offset2) <= kPointTolerance ||
            !IntersectCoplanarLines(offsetOrigin1,
                                    faceDirection1,
                                    offsetOrigin2,
                                    faceDirection2,
                                    edgeDirection,
                                    topPoint))
        {
            errorMessage = "Reference chamfer: the acute-angle profile could not be intersected.";
            return false;
        }
        const Point3d leftPoint = AddVector(topPoint, ScaleVector(offset1, -offsetDistance));
        const Point3d rightPoint = AddVector(topPoint, ScaleVector(offset2, -offsetDistance));
        const double currentGap = std::min(Distance(profileOrigin, leftPoint),
                                           Distance(profileOrigin, rightPoint));
        const double scale = clearance > kPointTolerance && currentGap > kPointTolerance
                                 ? clearance / currentGap
                                 : 1.0;
        profilePoints = {
            profileOrigin,
            AddVector(profileOrigin, ScaleVector(Subtract(leftPoint, profileOrigin), scale)),
            AddVector(profileOrigin, ScaleVector(Subtract(topPoint, profileOrigin), scale)),
            AddVector(profileOrigin, ScaleVector(Subtract(rightPoint, profileOrigin), scale)),
            profileOrigin};
        AppendDebugLog("reference non-right chamfer profile=acute-4-line fallback.");
    }

    if (profilePoints.empty())
    {
        errorMessage = "Reference chamfer: no valid non-right-angle profile was produced.";
        return false;
    }
    return ExtrudeReferenceCornerProfile(inputs,
                                         profilePoints,
                                         profileOrigin,
                                         edgeDirection,
                                         -inputs.thickness,
                                         edgeLength + inputs.thickness,
                                         "reference non-right corner-edge cut",
                                         errorMessage);
}

bool TwoPointSiBianUI::CreateReferenceRightCornerEdgeCut(
    const InferredInputs& inputs,
    Edge* cornerEdge,
    Face* referenceFace,
    std::string& errorMessage) const
{
    Part* workPart = session_ != nullptr ? session_->Parts()->Work() : nullptr;
    Vector3d inwardNormal;
    if (workPart == nullptr || inputs.targetBody == nullptr || cornerEdge == nullptr ||
        !ComputeReferenceInwardNormal(inputs.targetBody, referenceFace, inwardNormal))
    {
        errorMessage = "Reference chamfer: the 90-degree reference edge or face is unavailable.";
        return false;
    }
    double clearance = 0.0;
    if (!TryParseExpressionNumber(inputs.clearanceValue, clearance))
    {
        errorMessage = "Reference chamfer: clearance is not numeric.";
        return false;
    }
    clearance = std::fabs(clearance);
    Point3d first;
    Point3d second;
    try
    {
        cornerEdge->GetVertices(&first, &second);
    }
    catch (...)
    {
        errorMessage = "Reference chamfer: the 90-degree corner-edge endpoints are unavailable.";
        return false;
    }
    const Point3d helpPoint((first.X + second.X) * 0.5,
                            (first.Y + second.Y) * 0.5,
                            (first.Z + second.Z) * 0.5);
    Features::ExtrudeBuilder* builder = nullptr;
    try
    {
        builder = workPart->Features()->CreateExtrudeBuilder(nullptr);
        Section* section = workPart->Sections()->CreateSection(9.5e-05, 0.0001, 0.5);
        builder->SetSection(section);
        builder->AllowSelfIntersectingSection(true);
        builder->SetDistanceTolerance(0.0001);
        builder->BooleanOperation()->SetType(
            GeometricUtilities::BooleanOperation::BooleanTypeCreate);
        builder->SmartVolumeProfile()->SetOpenProfileSmartVolumeOption(false);
        builder->SmartVolumeProfile()->SetCloseProfileRule(
            GeometricUtilities::SmartVolumeProfileBuilder::CloseProfileRuleTypeFci);
        section->SetDistanceTolerance(0.0001);
        section->SetChainingTolerance(9.5e-05);
        section->SetAllowedEntityTypes(Section::AllowTypesOnlyCurves);
        section->AllowSelfIntersection(true);
        section->AllowDegenerateCurves(false);

        SelectionIntentRuleOptions* ruleOptions =
            workPart->ScRuleFactory()->CreateRuleOptions();
        ruleOptions->SetSelectedFromInactive(false);
        std::vector<Edge*> seeds(1, cornerEdge);
        EdgeDumbRule* edgeRule =
            workPart->ScRuleFactory()->CreateRuleEdgeDumb(seeds, ruleOptions);
        delete ruleOptions;
        std::vector<SelectionIntentRule*> rules(1, edgeRule);
        section->AddToSection(rules,
                              cornerEdge,
                              nullptr,
                              nullptr,
                              helpPoint,
                              Section::ModeCreate,
                              false);
        Direction* direction = workPart->Directions()->CreateDirection(
            helpPoint,
            inwardNormal,
            SmartObject::UpdateOptionWithinModeling);
        builder->SetDirection(direction);
        builder->Limits()->StartExtend()->Value()->SetFormula(
            FormatExpressionNumber(inputs.thickness).c_str());
        builder->Limits()->EndExtend()->Value()->SetFormula(
            FormatExpressionNumber(inputs.thickness + clearance).c_str());
        builder->Limits()->StartExtend()->SetTrimType(
            GeometricUtilities::Extend::ExtendTypeValue);
        builder->Limits()->EndExtend()->SetTrimType(
            GeometricUtilities::Extend::ExtendTypeValue);
        builder->Offset()->SetOption(GeometricUtilities::TypeSymmetricOffset);
        builder->Offset()->StartOffset()->SetFormula("0.0");
        builder->Offset()->EndOffset()->SetFormula(
            FormatExpressionNumber(inputs.thickness).c_str());
        builder->FeatureOptions()->SetBodyType(
            GeometricUtilities::FeatureOptions::BodyStyleSolid);

        Features::Feature* feature = builder->CommitFeature();
        builder->Destroy();
        builder = nullptr;
        SetConstructionRole(
            feature,
            zhihui_twopoint_sibian::kRoleExtrudeCornerEdgeCut);
        if (feature != nullptr)
        {
            AppendDebugLog(
                "tagged the reference corner-edge extrusion for clearance editing: extrude=" +
                std::to_string(feature->Tag()));
        }
        tag_t toolBodyTag = NULL_TAG;
        if (feature == nullptr ||
            UF_MODL_ask_feat_body(feature->Tag(), &toolBodyTag) != 0 ||
            toolBodyTag == NULL_TAG)
        {
            errorMessage = "Reference chamfer: the 90-degree edge extrusion produced no tool body.";
            return false;
        }
        tag_t subtractTag = NULL_TAG;
        if (!SubtractToolBodies(inputs.targetBody,
                                std::vector<tag_t>{toolBodyTag},
                                subtractTag,
                                errorMessage))
        {
            return false;
        }
        AppendDebugLog("reference 90-degree corner-edge cut completed: edge=" +
                       std::to_string(cornerEdge->Tag()) +
                       ", face=" + std::to_string(referenceFace->Tag()) +
                       ", subtract=" + std::to_string(subtractTag));
        return true;
    }
    catch (const NXException& ex)
    {
        errorMessage = "Reference chamfer: 90-degree edge cut failed.\n" + NxExceptionText(ex);
    }
    catch (...)
    {
        errorMessage = "Reference chamfer: 90-degree edge cut failed with an unknown exception.";
    }
    if (builder != nullptr)
    {
        try
        {
            builder->Destroy();
        }
        catch (...)
        {
        }
    }

    // NX occasionally rejects an open edge with symmetric offset even though
    // the edge, direction and distances are valid ("section values and
    // direction may be incompatible").  Preserve the reference-project
    // geometry by replacing that representation with its exact closed-profile
    // equivalent: the corner edge offset by one sheet thickness on both sides,
    // then extruded with the same start/end limits.
    Vector3d edgeDirection = Subtract(second, first);
    Vector3d lateralDirection = Cross(inwardNormal, edgeDirection);
    if (Normalize(edgeDirection) && Normalize(lateralDirection))
    {
        const Vector3d lateralOffset =
            ScaleVector(lateralDirection, inputs.thickness);
        const std::vector<Point3d> fallbackProfile{
            Point3d(first.X - lateralOffset.X,
                    first.Y - lateralOffset.Y,
                    first.Z - lateralOffset.Z),
            Point3d(second.X - lateralOffset.X,
                    second.Y - lateralOffset.Y,
                    second.Z - lateralOffset.Z),
            Point3d(second.X + lateralOffset.X,
                    second.Y + lateralOffset.Y,
                    second.Z + lateralOffset.Z),
            Point3d(first.X + lateralOffset.X,
                    first.Y + lateralOffset.Y,
                    first.Z + lateralOffset.Z)};
        std::string fallbackError;
        AppendDebugLog(
            "reference 90-degree corner-edge cut retrying with equivalent closed profile: edge=" +
            std::to_string(cornerEdge->Tag()) +
            ", face=" + std::to_string(referenceFace->Tag()) +
            ", inwardNormal=" + FormatVector(inwardNormal) +
            ", lateralHalfWidth=" +
            FormatExpressionNumber(inputs.thickness) +
            ", startLimit=" +
            FormatExpressionNumber(inputs.thickness) +
            ", endLimit=" +
            FormatExpressionNumber(inputs.thickness + clearance));
        if (ExtrudeReferenceCornerProfile(
                inputs,
                fallbackProfile,
                helpPoint,
                inwardNormal,
                inputs.thickness,
                inputs.thickness + clearance,
                "reference 90-degree closed-profile corner-edge cut",
                fallbackError))
        {
            AppendDebugLog(
                "reference 90-degree corner-edge cut completed with closed-profile fallback: edge=" +
                std::to_string(cornerEdge->Tag()) +
                ", face=" + std::to_string(referenceFace->Tag()));
            return true;
        }
        errorMessage +=
            "\nClosed-profile fallback failed: " + fallbackError;
    }
    else
    {
        errorMessage +=
            "\nClosed-profile fallback failed: the edge/lateral direction is invalid.";
    }
    return false;
}

bool TwoPointSiBianUI::CreateReferenceTopCornerCut(
    const InferredInputs& inputs,
    const Point3d& point,
    Vector3d cornerEdgeDirection,
    Vector3d edge1Direction,
    Vector3d edge2Direction,
    std::string& errorMessage) const
{
    double clearance = 0.0;
    double bendRadius = 0.0;
    if (!TryParseExpressionNumber(inputs.clearanceValue, clearance) ||
        !TryParseExpressionNumber(inputs.bendRadiusValue, bendRadius) ||
        !Normalize(cornerEdgeDirection) ||
        !Normalize(edge1Direction) ||
        !Normalize(edge2Direction))
    {
        errorMessage = "Reference chamfer: invalid top-corner inputs.";
        return false;
    }
    clearance = std::fabs(clearance);
    bendRadius = std::fabs(bendRadius);
    const double cutLength = inputs.thickness + bendRadius;
    Vector3d selectionDirection =
        ProjectVectorToPlane(Subtract(inputs.endPoint, inputs.startPoint), cornerEdgeDirection);
    edge1Direction = ProjectVectorToPlane(edge1Direction, cornerEdgeDirection);
    edge2Direction = ProjectVectorToPlane(edge2Direction, cornerEdgeDirection);
    if (!Normalize(selectionDirection) ||
        !Normalize(edge1Direction) ||
        !Normalize(edge2Direction) ||
        cutLength <= kPointTolerance)
    {
        errorMessage = "Reference chamfer: invalid top-corner center direction.";
        return false;
    }
    if (Dot(selectionDirection, edge1Direction) < 0.0)
    {
        edge1Direction = ScaleVector(edge1Direction, -1.0);
    }
    if (Dot(selectionDirection, edge2Direction) < 0.0)
    {
        edge2Direction = ScaleVector(edge2Direction, -1.0);
    }
    // P1->P2 only identifies which rays point into the selected region.  It
    // is not generally the angle bisector of the two far-end boundary edges.
    // Using it as the profile center line leaves a nonzero minimum B-D gap
    // (1.500225 in the reported case), so a requested 0.1 gap is impossible.
    // Build the profile on the actual inward angular bisector instead.
    Vector3d centerDirection(edge1Direction.X + edge2Direction.X,
                             edge1Direction.Y + edge2Direction.Y,
                             edge1Direction.Z + edge2Direction.Z);
    centerDirection = ProjectVectorToPlane(centerDirection, cornerEdgeDirection);
    if (!Normalize(centerDirection))
    {
        errorMessage = "Reference chamfer: the two far-end edges have no valid inward angular bisector.";
        return false;
    }
    if (Dot(centerDirection, selectionDirection) < 0.0)
    {
        centerDirection = ScaleVector(centerDirection, -1.0);
    }
    AppendDebugLog("reference top-corner directions: selection=" +
                   FormatVector(selectionDirection) +
                   ", edge1=" + FormatVector(edge1Direction) +
                   ", edge2=" + FormatVector(edge2Direction) +
                   ", bisector=" + FormatVector(centerDirection) +
                   ", cutLength=" + FormatExpressionNumber(cutLength));

    std::vector<Point3d> profilePoints;
    auto evaluate = [&](double centerDistance,
                        std::vector<Point3d>* points,
                        double* gap) -> bool {
        const Point3d cPoint = AddVector(point, ScaleVector(centerDirection, centerDistance));
        const double parameterA = Dot(Subtract(cPoint, point), edge1Direction);
        const double parameterE = Dot(Subtract(cPoint, point), edge2Direction);
        if (parameterA <= kPointTolerance || parameterE <= kPointTolerance)
        {
            return false;
        }
        const Point3d aPoint = AddVector(point, ScaleVector(edge1Direction, parameterA));
        const Point3d ePoint = AddVector(point, ScaleVector(edge2Direction, parameterE));
        Vector3d aToC = Subtract(cPoint, aPoint);
        Vector3d eToC = Subtract(cPoint, ePoint);
        if (Length(aToC) <= cutLength + kPointTolerance ||
            Length(eToC) <= cutLength + kPointTolerance ||
            !Normalize(aToC) || !Normalize(eToC))
        {
            return false;
        }
        const Point3d bPoint = AddVector(aPoint, ScaleVector(aToC, cutLength));
        const Point3d dPoint = AddVector(ePoint, ScaleVector(eToC, cutLength));
        if (gap != nullptr) *gap = Distance(bPoint, dPoint);
        if (points != nullptr)
        {
            *points = {point, aPoint, bPoint, cPoint, dPoint, ePoint, point};
        }
        return true;
    };

    // Keep the lower bound at the first geometrically-too-small distance.
    // The previous loop advanced lowDistance until a valid profile existed,
    // then copied that same value to highDistance.  When its first valid gap
    // already exceeded the requested clearance, both bounds were identical
    // and the binary search returned an oversized far-end cut.
    double lowDistance = cutLength + kPointTolerance;
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
    if (highGap < clearance || clearance <= kPointTolerance)
    {
        errorMessage = "Reference chamfer: no valid far-end top-corner profile was found.";
        return false;
    }
    for (int index = 0; index < 80; ++index)
    {
        const double middle = (lowDistance + highDistance) * 0.5;
        double middleGap = 0.0;
        if (evaluate(middle, nullptr, &middleGap) && middleGap >= clearance)
        {
            highDistance = middle;
        }
        else
        {
            lowDistance = middle;
        }
    }
    double finalGap = 0.0;
    if (!evaluate(highDistance, &profilePoints, &finalGap))
    {
        errorMessage = "Reference chamfer: far-end top-corner profile evaluation failed.";
        return false;
    }
    AppendDebugLog("reference top-corner profile=OABCDEO, targetGap=" +
                   FormatExpressionNumber(clearance) +
                   ", actualGap=" + FormatExpressionNumber(finalGap) +
                   ", lowerDistance=" + FormatExpressionNumber(lowDistance) +
                   ", upperDistance=" + FormatExpressionNumber(highDistance));
    return ExtrudeReferenceCornerProfile(inputs,
                                         profilePoints,
                                         point,
                                         cornerEdgeDirection,
                                         0.0,
                                         cutLength,
                                         "reference far-end top-corner cut",
                                         errorMessage);
}

bool TwoPointSiBianUI::CreateReferenceCornerEdgeCut(const InferredInputs& inputs,
                                                     Edge* cornerEdge,
                                                     std::string& errorMessage) const
{
    std::vector<Face*> faces = FindReferencePlanarFaces(cornerEdge);
    if (inputs.targetBody == nullptr || cornerEdge == nullptr || faces.size() < 2)
    {
        errorMessage = "Reference chamfer: the selected corner edge does not have two planar faces.";
        return false;
    }
    std::vector<Vector3d> inwardNormals;
    for (Face* face : faces)
    {
        Vector3d normal;
        if (!ComputeReferenceInwardNormal(inputs.targetBody, face, normal))
        {
            errorMessage = "Reference chamfer: an inward face normal could not be calculated.";
            return false;
        }
        inwardNormals.push_back(normal);
    }
    double cosine = std::max(-1.0, std::min(1.0, Dot(inwardNormals[0], inwardNormals[1])));
    const double angleDegrees = std::acos(cosine) * 180.0 / 3.14159265358979323846;
    const bool isRightAngle = std::fabs(angleDegrees - 90.0) <= 1.0;

    // The reference project chooses the larger or smaller adjacent face from
    // its wrap-side enumeration.  The current dialog's left path maps to the
    // larger face and its right path maps to the smaller face.
    Face* preferredFace = inputs.featureMode == FeatureMode::NinetyRight
                              ? faces.back()
                              : faces.front();

    struct PendingTopCut
    {
        bool create = false;
        Point3d point;
        Vector3d cornerDirection;
        Vector3d firstDirection;
        Vector3d secondDirection;
    } pending;
    Point3d cornerFirst;
    Point3d cornerSecond;
    try
    {
        cornerEdge->GetVertices(&cornerFirst, &cornerSecond);
    }
    catch (...)
    {
        errorMessage = "Reference chamfer: corner-edge endpoints are unavailable.";
        return false;
    }
    pending.point = Distance(cornerFirst, inputs.endPoint) >
                            Distance(cornerSecond, inputs.endPoint) + 0.05
                        ? cornerFirst
                        : cornerSecond;
    const bool endpointTie =
        std::fabs(Distance(cornerFirst, inputs.endPoint) -
                  Distance(cornerSecond, inputs.endPoint)) <= 0.05;
    if (!endpointTie && angleDegrees > kPointTolerance && angleDegrees < 180.0 - kPointTolerance)
    {
        double nearestDistance = std::numeric_limits<double>::max();
        for (Edge* bodyEdge : inputs.targetBody->GetEdges())
        {
            if (bodyEdge == nullptr) continue;
            Point3d first;
            Point3d second;
            try
            {
                bodyEdge->GetVertices(&first, &second);
            }
            catch (...)
            {
                continue;
            }
            for (const Point3d& candidate : {first, second})
            {
                const double distance = Distance(pending.point, candidate);
                if (distance > 0.05 && distance < nearestDistance)
                {
                    nearestDistance = distance;
                }
            }
        }
        auto theoreticalScore = [&](double effectiveAngle,
                                    double& planarDistance,
                                    double& topDistance) -> double {
            const double sineHalf = std::sin(effectiveAngle * 0.5 *
                                             3.14159265358979323846 / 180.0);
            if (std::fabs(sineHalf) <= 1.0e-8)
            {
                return std::numeric_limits<double>::max();
            }
            planarDistance = inputs.thickness / sineHalf;
            topDistance = std::sqrt(planarDistance * planarDistance +
                                    inputs.thickness * inputs.thickness);
            return std::min(std::fabs(nearestDistance - planarDistance),
                            std::fabs(nearestDistance - topDistance));
        };
        double planarDistance = 0.0;
        double topDistance = 0.0;
        double score = theoreticalScore(angleDegrees, planarDistance, topDistance);
        double supplementPlanar = 0.0;
        double supplementTop = 0.0;
        const double supplementScore = theoreticalScore(180.0 - angleDegrees,
                                                         supplementPlanar,
                                                         supplementTop);
        if (supplementScore < score)
        {
            planarDistance = supplementPlanar;
            topDistance = supplementTop;
        }
        const double tolerance = std::max(0.05, inputs.thickness * 0.15);
        const double planarDelta = std::fabs(nearestDistance - planarDistance);
        const double topDelta = std::fabs(nearestDistance - topDistance);
        const bool isTopCorner = planarDelta > tolerance &&
                                 (topDelta <= tolerance || topDelta < planarDelta);
        if (isTopCorner)
        {
            std::vector<Edge*> connected =
                FindReferenceConnectedEdges(inputs.targetBody, pending.point);
            connected.erase(std::remove_if(connected.begin(), connected.end(),
                                           [cornerEdge](Edge* edge) {
                                               return edge == nullptr || edge->Tag() == cornerEdge->Tag();
                                           }),
                            connected.end());
            if (connected.size() >= 2)
            {
                auto directionFromPoint = [&](Edge* edge, Vector3d& direction) -> bool {
                    Point3d first;
                    Point3d second;
                    try
                    {
                        edge->GetVertices(&first, &second);
                    }
                    catch (...)
                    {
                        return false;
                    }
                    direction = Distance(pending.point, first) <= Distance(pending.point, second)
                                    ? Subtract(second, pending.point)
                                    : Subtract(first, pending.point);
                    return Normalize(direction);
                };
                pending.cornerDirection =
                    Distance(pending.point, cornerFirst) <= Distance(pending.point, cornerSecond)
                        ? Subtract(cornerSecond, pending.point)
                        : Subtract(cornerFirst, pending.point);
                pending.create = Normalize(pending.cornerDirection) &&
                                 directionFromPoint(connected[0], pending.firstDirection) &&
                                 directionFromPoint(connected[1], pending.secondDirection);
                if (pending.create &&
                    Dot(Cross(pending.firstDirection, pending.secondDirection),
                        pending.cornerDirection) < 0.0)
                {
                    std::swap(pending.firstDirection, pending.secondDirection);
                }
            }
        }
    }

    AppendDebugLog("reference chamfer route: edge=" + std::to_string(cornerEdge->Tag()) +
                   ", angle=" + FormatExpressionNumber(angleDegrees) +
                   ", right=" + (isRightAngle ? "true" : "false") +
                   ", preferredFace=" + std::to_string(preferredFace->Tag()) +
                   ", farTopCut=" + (pending.create ? "true" : "false"));
    const bool created = isRightAngle
                             ? CreateReferenceRightCornerEdgeCut(inputs,
                                                                 cornerEdge,
                                                                 preferredFace,
                                                                 errorMessage)
                             : CreateReferenceNonRightCornerEdgeCut(inputs,
                                                                    cornerEdge,
                                                                    angleDegrees,
                                                                    faces,
                                                                    inwardNormals,
                                                                    errorMessage);
    if (!created)
    {
        return false;
    }
    if (pending.create)
    {
        std::string topError;
        if (!CreateReferenceTopCornerCut(inputs,
                                         pending.point,
                                         pending.cornerDirection,
                                         pending.firstDirection,
                                         pending.secondDirection,
                                         topError))
        {
            // The reference project treats the far-end top cut as optional.
            AppendDebugLog("reference optional far-end top-corner cut skipped: " + topError);
        }
    }
    return true;
}

bool TwoPointSiBianUI::CreateObliqueClearanceCut(
    const InferredInputs& inputs,
    Edge* referenceBEdge,
    Edge* p2QRipEdge,
    Face* b1B2Plane,
    const Point3d& originalQ,
    const Vector3d& inputPlaneNormal,
    tag_t& subtractFeatureTag,
    std::string& errorMessage) const
{
    subtractFeatureTag = NULL_TAG;
    if (inputs.targetBody == nullptr || referenceBEdge == nullptr || b1B2Plane == nullptr)
    {
        errorMessage = "The reference B edge or B1/B2 plane is unavailable for the oblique clearance cut.";
        return false;
    }

    double clearance = 0.0;
    try
    {
        clearance = std::fabs(std::stod(inputs.clearanceValue));
    }
    catch (...)
    {
        errorMessage = "The dialog clearance value is invalid for the oblique clearance rectangle.";
        return false;
    }
    if (clearance <= kPointTolerance)
    {
        errorMessage = "The dialog clearance must be positive for the oblique clearance rectangle.";
        return false;
    }

    Point3d b1First;
    Point3d b1Second;
    if (!EdgeNaturalStartEnd(referenceBEdge, b1First, b1Second))
    {
        errorMessage = "The reference B-edge endpoints could not be read after the P2-Q rip.";
        return false;
    }
    const Point3d b1RipEndpoint =
        Distance(b1First, originalQ) <= Distance(b1Second, originalQ)
            ? b1First
            : b1Second;
    const Point3d b1OtherEndpoint =
        Distance(b1First, originalQ) <= Distance(b1Second, originalQ)
            ? b1Second
            : b1First;

    Edge* shortestDirectionEdge = nullptr;
    Point3d shortestOtherEndpoint;
    double shortestLength = std::numeric_limits<double>::max();
    for (Edge* edge : inputs.targetBody->GetEdges())
    {
        if (edge == nullptr || edge == referenceBEdge || edge == p2QRipEdge ||
            !EdgeTouchesPoint(edge, b1RipEndpoint))
        {
            continue;
        }
        Point3d otherEndpoint;
        if (!EdgeOtherPoint(edge, b1RipEndpoint, otherEndpoint))
        {
            continue;
        }
        const double length = edge->GetLength();
        if (length > kPointTolerance && length < shortestLength)
        {
            shortestDirectionEdge = edge;
            shortestOtherEndpoint = otherEndpoint;
            shortestLength = length;
        }
    }
    if (shortestDirectionEdge == nullptr)
    {
        errorMessage = "No shortest direction edge was found at the reference B endpoint nearest the rip.";
        return false;
    }

    Vector3d lineDirection = Subtract(shortestOtherEndpoint, b1RipEndpoint);
    Vector3d planeNormal = inputPlaneNormal;
    if (!Normalize(lineDirection) || !Normalize(planeNormal))
    {
        errorMessage = "The oblique rectangle direction or B1/B2 plane normal is invalid.";
        return false;
    }
    Vector3d sideDirection = Cross(planeNormal, lineDirection);
    if (!Normalize(sideDirection))
    {
        errorMessage = "The oblique rectangle width direction is invalid.";
        return false;
    }

    // Put the complete rectangle width on the side opposite B1 so the
    // clearance rectangle shares only its long boundary with the B1 side and
    // does not overlap B1.
    const Vector3d b1Direction = Subtract(b1OtherEndpoint, b1RipEndpoint);
    if (Dot(b1Direction, sideDirection) > 0.0)
    {
        sideDirection.X = -sideDirection.X;
        sideDirection.Y = -sideDirection.Y;
        sideDirection.Z = -sideDirection.Z;
    }

    double furthestIntersection = -1.0;
    for (Edge* boundaryEdge : b1B2Plane->GetEdges())
    {
        Point3d edgeStart;
        Point3d edgeEnd;
        if (boundaryEdge == nullptr ||
            !EdgeNaturalStartEnd(boundaryEdge, edgeStart, edgeEnd))
        {
            continue;
        }
        const Vector3d startVector = Subtract(edgeStart, b1RipEndpoint);
        const Vector3d endVector = Subtract(edgeEnd, b1RipEndpoint);
        const double startX = Dot(startVector, lineDirection);
        const double startY = Dot(startVector, sideDirection);
        const double endX = Dot(endVector, lineDirection);
        const double endY = Dot(endVector, sideDirection);

        if (std::fabs(startY) <= kPlaneTolerance &&
            std::fabs(endY) <= kPlaneTolerance)
        {
            furthestIntersection = std::max(furthestIntersection,
                                            std::max(startX, endX));
            continue;
        }
        const double denominator = startY - endY;
        if (std::fabs(denominator) <= kPlaneTolerance)
        {
            continue;
        }
        const double segmentParameter = startY / denominator;
        if (segmentParameter < -kPointTolerance ||
            segmentParameter > 1.0 + kPointTolerance)
        {
            continue;
        }
        const double rayParameter =
            startX + segmentParameter * (endX - startX);
        if (rayParameter >= -kPointTolerance)
        {
            furthestIntersection = std::max(furthestIntersection,
                                            rayParameter);
        }
    }
    if (furthestIntersection <= kPointTolerance)
    {
        errorMessage = "The shortest-edge ray did not reach the B1/B2 plane perimeter.";
        return false;
    }

    const double rectangleLength = furthestIntersection + 1.0;
    const Point3d lineEnd(b1RipEndpoint.X + lineDirection.X * rectangleLength,
                          b1RipEndpoint.Y + lineDirection.Y * rectangleLength,
                          b1RipEndpoint.Z + lineDirection.Z * rectangleLength);
    const Point3d corners[] = {
        b1RipEndpoint,
        lineEnd,
        Point3d(lineEnd.X + sideDirection.X * clearance,
                lineEnd.Y + sideDirection.Y * clearance,
                lineEnd.Z + sideDirection.Z * clearance),
        Point3d(b1RipEndpoint.X + sideDirection.X * clearance,
                b1RipEndpoint.Y + sideDirection.Y * clearance,
                b1RipEndpoint.Z + sideDirection.Z * clearance)};

    // Both directional right-angle routes use an internal four-line sketch so
    // their clearance rectangle can be edited in place.  Keep only the
    // established chamfer route on the raw-curve implementation below.
    if (inputs.featureMode == FeatureMode::NinetyRight ||
        inputs.featureMode == FeatureMode::NinetyLeft)
    {
        const bool rightMode =
            inputs.featureMode == FeatureMode::NinetyRight;
        const std::vector<Point3d> sketchProfile = {
            corners[0], corners[1], corners[2], corners[3], corners[0]};
        if (!ExtrudeReferenceCornerProfile(
                inputs,
                sketchProfile,
                b1RipEndpoint,
                planeNormal,
                -inputs.thickness,
                inputs.thickness,
                rightMode ? "90-right oblique clearance rectangle cut"
                          : "90-left oblique clearance rectangle cut",
                errorMessage))
        {
            return false;
        }
        AppendDebugLog(
            std::string(rightMode ? "90-right" : "90-left") +
            " oblique clearance rectangle completed with internal sketch: referenceBEdge=" +
            std::to_string(referenceBEdge->Tag()) +
            ", B2RipEndpoint=" + FormatPoint(b1RipEndpoint) +
            ", shortestDirectionEdge=" +
            std::to_string(shortestDirectionEdge->Tag()) +
            ", shortestLength=" +
            FormatExpressionNumber(shortestLength) +
            ", perimeterIntersection=" +
            FormatExpressionNumber(furthestIntersection) +
            ", extensionBeyondPerimeter=1" +
            ", rectangleLength=" +
            FormatExpressionNumber(rectangleLength) +
            ", rectangleWidth=" +
            FormatExpressionNumber(clearance));
        return true;
    }

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
        lineData.start_point[0] = start.X;
        lineData.start_point[1] = start.Y;
        lineData.start_point[2] = start.Z;
        lineData.end_point[0] = end.X;
        lineData.end_point[1] = end.Y;
        lineData.end_point[2] = end.Z;
        tag_t lineTag = NULL_TAG;
        const int lineResult = UF_CURVE_create_line(&lineData, &lineTag);
        if (lineResult != 0 || lineTag == NULL_TAG)
        {
            errorMessage = "Failed to create the oblique clearance rectangle profile.\n" +
                           UfMessage(lineResult);
            cleanupLists();
            return false;
        }
        curveTags.push_back(lineTag);
    }
    if (UF_MODL_create_list(&curveList) != 0 || curveList == nullptr)
    {
        errorMessage = "Failed to allocate the oblique clearance rectangle profile list.";
        return false;
    }
    for (tag_t curveTag : curveTags)
    {
        UF_MODL_put_list_item(curveList, curveTag);
    }

    std::string startLimit = "-" + FormatExpressionNumber(inputs.thickness);
    std::string endLimit = FormatExpressionNumber(inputs.thickness);
    char taper[] = "0.0";
    char* limits[2] = {const_cast<char*>(startLimit.c_str()),
                       const_cast<char*>(endLimit.c_str())};
    double origin[3] = {b1RipEndpoint.X, b1RipEndpoint.Y, b1RipEndpoint.Z};
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
        UF_MODL_ask_list_count(featureList, &featureCount) == 0 &&
        featureCount > 0)
    {
        UF_MODL_ask_list_item(featureList, 0, &extrudeFeatureTag);
        UF_MODL_ask_feat_body(extrudeFeatureTag, &toolBodyTag);
    }
    cleanupLists();
    if (extrudeResult != 0 || toolBodyTag == NULL_TAG)
    {
        errorMessage = "Failed to extrude the oblique clearance rectangle.\n" +
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

    AppendDebugLog("oblique clearance rectangle cut completed: referenceBEdge=" +
                   std::to_string(referenceBEdge->Tag()) +
                   ", B1RipEndpoint=" + FormatPoint(b1RipEndpoint) +
                   ", shortestDirectionEdge=" +
                   std::to_string(shortestDirectionEdge->Tag()) +
                   ", shortestLength=" +
                   FormatExpressionNumber(shortestLength) +
                   ", perimeterIntersection=" +
                   FormatExpressionNumber(furthestIntersection) +
                   ", extensionBeyondPerimeter=1" +
                   ", rectangleLength=" +
                   FormatExpressionNumber(rectangleLength) +
                   ", rectangleWidth=" +
                   FormatExpressionNumber(clearance) +
                   ", extrudeFeature=" +
                   std::to_string(extrudeFeatureTag) +
                   ", subtractFeature=" +
                   std::to_string(subtractFeatureTag));
    return true;
}

bool TwoPointSiBianUI::OffsetConcaveClearanceFace(
    const InferredInputs& inputs,
    Face* commonFace,
    const std::vector<tag_t>& offsetFeatureTags,
    tag_t& offsetFeatureTag,
    std::string& errorMessage,
    bool resolveCurrentCoplanarFragments) const
{
    offsetFeatureTag = NULL_TAG;
    errorMessage.clear();
    Part* workPart = session_->Parts()->Work();
    if (workPart == nullptr || inputs.targetBody == nullptr || commonFace == nullptr ||
        offsetFeatureTags.empty())
    {
        errorMessage = "The B1/B2 plane or rip-offset feature is unavailable for the -60 face offset.";
        return false;
    }

    double clearance = 0.0;
    try
    {
        clearance = std::fabs(std::stod(inputs.clearanceValue));
    }
    catch (...)
    {
        errorMessage = "The dialog clearance value could not be read for the -60 face-offset edge check.";
        return false;
    }
    const double clearanceTolerance = std::max(0.01, clearance * 0.10);
    const double requiredFacePerimeter =
        2.0 * (clearance + inputs.thickness);
    const double perimeterTolerance =
        std::max(0.02, requiredFacePerimeter * 0.01);

    std::set<tag_t> offsetFaceTags;
    for (tag_t featureTag : offsetFeatureTags)
    {
        Features::Feature* feature = featureTag == NULL_TAG
                                         ? nullptr
                                         : dynamic_cast<Features::Feature*>(NXObjectManager::Get(featureTag));
        if (feature == nullptr)
        {
            continue;
        }
        for (Face* face : feature->GetFaces())
        {
            if (face != nullptr)
            {
                offsetFaceTags.insert(face->Tag());
            }
        }
    }

    Face* originalOffsetFace = nullptr;
    Edge* clearanceSharedEdge = nullptr;
    double bestClearanceError = std::numeric_limits<double>::max();
    double selectedFacePerimeter = 0.0;
    double bestPerimeterError = std::numeric_limits<double>::max();
    std::vector<Face*> commonPlaneFaces(1, commonFace);
    if (resolveCurrentCoplanarFragments)
    {
        Point3d originalPlanePoint;
        Vector3d originalPlaneNormal;
        if (FacePlaneData(commonFace,
                          originalPlanePoint,
                          originalPlaneNormal) &&
            Normalize(originalPlaneNormal))
        {
            commonPlaneFaces.clear();
            for (Face* candidateFace : inputs.targetBody->GetFaces())
            {
                if (candidateFace == nullptr ||
                    candidateFace->SolidFaceType() != Face::FaceTypePlanar)
                {
                    continue;
                }
                Point3d candidatePlanePoint;
                Vector3d candidatePlaneNormal;
                if (!FacePlaneData(candidateFace,
                                   candidatePlanePoint,
                                   candidatePlaneNormal) ||
                    !Normalize(candidatePlaneNormal))
                {
                    continue;
                }
                const double normalAlignment =
                    std::fabs(Dot(originalPlaneNormal,
                                  candidatePlaneNormal));
                const double planeDistance =
                    std::fabs(Dot(Subtract(candidatePlanePoint,
                                           originalPlanePoint),
                                  originalPlaneNormal));
                if (normalAlignment >= 1.0 - 1.0e-6 &&
                    planeDistance <= kPlaneTolerance)
                {
                    commonPlaneFaces.push_back(candidateFace);
                    AppendDebugLog(
                        "OffsetConcaveClearanceFace current coplanar fragment for chamfer-270: originalFace=" +
                        std::to_string(commonFace->Tag()) +
                        ", currentFace=" +
                        std::to_string(candidateFace->Tag()) +
                        ", planeDistance=" +
                        FormatExpressionNumber(planeDistance));
                }
            }
        }
        if (commonPlaneFaces.empty())
        {
            commonPlaneFaces.push_back(commonFace);
        }
    }

    std::set<tag_t> commonPlaneFaceTags;
    for (Face* planeFace : commonPlaneFaces)
    {
        if (planeFace != nullptr)
        {
            commonPlaneFaceTags.insert(planeFace->Tag());
        }
    }
    std::set<tag_t> visitedPlaneEdges;
    tag_t nearestPlaneEdgeTag = NULL_TAG;
    double nearestPlaneEdgeLength = 0.0;
    double nearestPlaneEdgeError = std::numeric_limits<double>::max();
    for (Face* currentPlaneFace : commonPlaneFaces)
    {
        if (currentPlaneFace == nullptr)
        {
            continue;
        }
        for (Edge* planeEdge : currentPlaneFace->GetEdges())
        {
            if (planeEdge == nullptr ||
                !visitedPlaneEdges.insert(planeEdge->Tag()).second)
            {
                continue;
            }
            const double edgeLength = planeEdge->GetLength();
            const double error = std::fabs(edgeLength - clearance);
            if (error < nearestPlaneEdgeError)
            {
                nearestPlaneEdgeError = error;
                nearestPlaneEdgeTag = planeEdge->Tag();
                nearestPlaneEdgeLength = edgeLength;
            }
            if (error > clearanceTolerance || error >= bestClearanceError)
            {
                continue;
            }
            for (Face* adjacentFace : planeEdge->GetFaces())
            {
                const bool adjacentIsCommonPlane =
                    adjacentFace != nullptr &&
                    commonPlaneFaceTags.find(adjacentFace->Tag()) !=
                        commonPlaneFaceTags.end();
                const bool isReportedOffsetFace =
                    adjacentFace != nullptr &&
                    offsetFaceTags.find(adjacentFace->Tag()) !=
                        offsetFaceTags.end();
                if (adjacentFace == nullptr || adjacentIsCommonPlane ||
                    (!resolveCurrentCoplanarFragments &&
                     !isReportedOffsetFace))
                {
                    continue;
                }
                double facePerimeter = 0.0;
                for (Edge* faceEdge : adjacentFace->GetEdges())
                {
                    if (faceEdge != nullptr)
                    {
                        facePerimeter += faceEdge->GetLength();
                    }
                }
                const double perimeterError =
                    std::fabs(facePerimeter - requiredFacePerimeter);
                AppendDebugLog(
                    "OffsetConcaveClearanceFace chamfer-270 candidate: face=" +
                    std::to_string(adjacentFace->Tag()) +
                    ", sharedEdge=" +
                    std::to_string(planeEdge->Tag()) +
                    ", sharedEdgeLength=" +
                    FormatExpressionNumber(edgeLength) +
                    ", perimeter=" +
                    FormatExpressionNumber(facePerimeter) +
                    ", requiredPerimeter=" +
                    FormatExpressionNumber(requiredFacePerimeter) +
                    ", reportedByOffsetFeature=" +
                    (isReportedOffsetFace ? "true" : "false"));
                if (perimeterError > perimeterTolerance)
                {
                    AppendDebugLog("OffsetConcaveClearanceFace rejected face by perimeter: face=" +
                                   std::to_string(adjacentFace->Tag()) +
                                   ", sharedEdge=" + std::to_string(planeEdge->Tag()) +
                                   ", sharedEdgeLength=" +
                                   FormatExpressionNumber(edgeLength) +
                                   ", perimeter=" +
                                   FormatExpressionNumber(facePerimeter) +
                                   ", requiredPerimeter=" +
                                   FormatExpressionNumber(requiredFacePerimeter) +
                                   ", tolerance=" +
                                   FormatExpressionNumber(perimeterTolerance));
                    continue;
                }
                originalOffsetFace = adjacentFace;
                clearanceSharedEdge = planeEdge;
                bestClearanceError = error;
                selectedFacePerimeter = facePerimeter;
                bestPerimeterError = perimeterError;
                break;
            }
        }
    }
    if (originalOffsetFace == nullptr || clearanceSharedEdge == nullptr)
    {
        errorMessage = "No rip-offset face satisfied both the clearance-edge length and the clearance/thickness perimeter.";
        AppendDebugLog("OffsetConcaveClearanceFace failed: commonFace=" +
                       std::to_string(commonFace->Tag()) +
                       ", clearance=" + FormatExpressionNumber(clearance) +
                       ", requiredPerimeter=" +
                       FormatExpressionNumber(requiredFacePerimeter) +
                       ", offsetFaceCount=" + std::to_string(offsetFaceTags.size()) +
                       ", currentCommonPlaneFaceCount=" +
                       std::to_string(commonPlaneFaces.size()) +
                       ", nearestPlaneEdge=" +
                       std::to_string(nearestPlaneEdgeTag) +
                       ", nearestPlaneEdgeLength=" +
                       FormatExpressionNumber(nearestPlaneEdgeLength) +
                       ", nearestPlaneEdgeError=" +
                       FormatExpressionNumber(nearestPlaneEdgeError));
        return false;
    }

    Features::OffsetFaceBuilder* offsetBuilder = nullptr;
    const tag_t cachedCommonFaceTag = commonFace->Tag();
    const tag_t cachedSharedEdgeTag = clearanceSharedEdge->Tag();
    const double cachedSharedEdgeLength = clearanceSharedEdge->GetLength();
    const tag_t cachedOffsetFaceTag = originalOffsetFace->Tag();
    const double cachedFacePerimeter = selectedFacePerimeter;
    const double cachedPerimeterError = bestPerimeterError;
    try
    {
        offsetBuilder = workPart->Features()->CreateOffsetFaceBuilder(nullptr);
        offsetBuilder->Distance()->SetFormula("-60");
        offsetBuilder->SetDirection(false);
        FaceDumbRule* offsetRule = workPart->ScRuleFactory()->CreateRuleFaceDumb(
            std::vector<Face*>{originalOffsetFace});
        offsetBuilder->FaceCollector()->ReplaceRules(
            std::vector<SelectionIntentRule*>{offsetRule}, false);
        Features::Feature* offsetFeature = offsetBuilder->CommitFeature();
        offsetFeatureTag = offsetFeature != nullptr ? offsetFeature->Tag() : NULL_TAG;
        SetConstructionRole(
            offsetFeature,
            zhihui_twopoint_sibian::kRoleOffsetNegative60);
        offsetBuilder->Destroy();
        offsetBuilder = nullptr;
        if (offsetFeatureTag == NULL_TAG)
        {
            errorMessage = "NX did not return a feature after offsetting the clearance face by -60.";
            return false;
        }
        AppendDebugLog("OffsetConcaveClearanceFace completed: commonFace=" +
                       std::to_string(cachedCommonFaceTag) +
                       ", clearanceSharedEdge=" + std::to_string(cachedSharedEdgeTag) +
                       ", clearanceEdgeLength=" +
                       FormatExpressionNumber(cachedSharedEdgeLength) +
                       ", clearanceOffsetFace=" + std::to_string(cachedOffsetFaceTag) +
                       ", facePerimeter=" +
                       FormatExpressionNumber(cachedFacePerimeter) +
                       ", requiredPerimeter=" +
                       FormatExpressionNumber(requiredFacePerimeter) +
                       ", perimeterError=" +
                       FormatExpressionNumber(cachedPerimeterError) +
                       ", distance=-60" +
                       ", offsetFeature=" + std::to_string(offsetFeatureTag));
        return true;
    }
    catch (const NXException& ex)
    {
        if (offsetBuilder != nullptr)
        {
            offsetBuilder->Destroy();
        }
        errorMessage = "Failed to offset the rip clearance face by -60.\n" +
                       NxExceptionText(ex);
        AppendDebugLog(errorMessage);
        return false;
    }

#if 0 // Legacy replacement-face implementation intentionally disabled.

    Point3d originalPlanePoint;
    Vector3d originalNormal;
    if (!FacePlaneData(originalOffsetFace, originalPlanePoint, originalNormal))
    {
        errorMessage = "The rip-offset face is not planar for the replace-face operation.";
        return false;
    }

    auto faceHasThicknessWidth = [&](Face* candidate,
                                     double& bestWidthError,
                                     tag_t& firstWidthEdge,
                                     tag_t& secondWidthEdge)
    {
        bestWidthError = std::numeric_limits<double>::max();
        firstWidthEdge = NULL_TAG;
        secondWidthEdge = NULL_TAG;
        const std::vector<Edge*> edges = candidate->GetEdges();
        for (std::size_t firstIndex = 0; firstIndex < edges.size(); ++firstIndex)
        {
            Point3d firstStart;
            Point3d firstEnd;
            if (edges[firstIndex] == nullptr ||
                !EdgeNaturalStartEnd(edges[firstIndex], firstStart, firstEnd))
            {
                continue;
            }
            Vector3d firstDirection = Subtract(firstEnd, firstStart);
            if (!Normalize(firstDirection))
            {
                continue;
            }
            for (std::size_t secondIndex = firstIndex + 1; secondIndex < edges.size(); ++secondIndex)
            {
                Point3d secondStart;
                Point3d secondEnd;
                if (edges[secondIndex] == nullptr ||
                    !EdgeNaturalStartEnd(edges[secondIndex], secondStart, secondEnd))
                {
                    continue;
                }
                Vector3d secondDirection = Subtract(secondEnd, secondStart);
                if (!Normalize(secondDirection) ||
                    std::fabs(Dot(firstDirection, secondDirection)) < 0.999)
                {
                    continue;
                }
                try
                {
                    MeasureDistance* measurement = workPart->MeasureManager()->NewDistance(
                        nullptr,
                        MeasureManager::MeasureTypeMinimum,
                        edges[firstIndex],
                        edges[secondIndex]);
                    if (measurement == nullptr)
                    {
                        continue;
                    }
                    const double width = measurement->Value();
                    delete measurement;
                    const double error = std::fabs(width - inputs.thickness);
                    if (error < bestWidthError)
                    {
                        bestWidthError = error;
                        firstWidthEdge = edges[firstIndex]->Tag();
                        secondWidthEdge = edges[secondIndex]->Tag();
                    }
                }
                catch (...)
                {
                }
            }
        }
        const double thicknessTolerance = std::max(0.02, inputs.thickness * 0.10);
        return firstWidthEdge != NULL_TAG && bestWidthError <= thicknessTolerance;
    };

    Face* replacementThicknessFace = nullptr;
    Edge* connectionEdge = nullptr;
    double nearestPlaneDistance = std::numeric_limits<double>::max();
    double selectedWidthError = std::numeric_limits<double>::max();
    tag_t selectedWidthEdge1 = NULL_TAG;
    tag_t selectedWidthEdge2 = NULL_TAG;
    std::set<tag_t> visitedFaces;
    for (Edge* planeEdge : commonFace->GetEdges())
    {
        if (planeEdge == nullptr)
        {
            continue;
        }
        for (Face* candidate : planeEdge->GetFaces())
        {
            if (candidate == nullptr || candidate == commonFace ||
                candidate == originalOffsetFace ||
                candidate->SolidFaceType() != Face::FaceTypePlanar ||
                !visitedFaces.insert(candidate->Tag()).second)
            {
                continue;
            }
            Point3d candidatePlanePoint;
            Vector3d candidateNormal;
            if (!FacePlaneData(candidate, candidatePlanePoint, candidateNormal) ||
                std::fabs(Dot(originalNormal, candidateNormal)) < 0.995)
            {
                continue;
            }
            double widthError = 0.0;
            tag_t widthEdge1 = NULL_TAG;
            tag_t widthEdge2 = NULL_TAG;
            if (!faceHasThicknessWidth(candidate, widthError, widthEdge1, widthEdge2))
            {
                continue;
            }
            const double planeDistance =
                std::fabs(Dot(Subtract(candidatePlanePoint, originalPlanePoint), originalNormal));
            if (planeDistance < nearestPlaneDistance)
            {
                replacementThicknessFace = candidate;
                connectionEdge = planeEdge;
                nearestPlaneDistance = planeDistance;
                selectedWidthError = widthError;
                selectedWidthEdge1 = widthEdge1;
                selectedWidthEdge2 = widthEdge2;
            }
        }
    }
    if (replacementThicknessFace == nullptr)
    {
        errorMessage = "No connected thickness face parallel to and nearest the rip-offset face was found.";
        return false;
    }

    Features::ReplaceFaceBuilder* builder = nullptr;
    try
    {
        builder = workPart->Features()->CreateReplaceFaceBuilder(nullptr);
        builder->SetType(Features::ReplaceFaceBuilder::ReplaceTypesReplace);
        builder->OffsetDistance()->SetFormula("0");
        builder->ResetReplaceFaceMethod();
        builder->ResetFreeEdgeProjectionOption();
        builder->SetReverseDirection(false);

        FaceDumbRule* originalRule = workPart->ScRuleFactory()->CreateRuleFaceDumb(
            std::vector<Face*>{originalOffsetFace});
        builder->FaceToReplace()->ReplaceRules(
            std::vector<SelectionIntentRule*>{originalRule}, false);
        FaceDumbRule* replacementRule = workPart->ScRuleFactory()->CreateRuleFaceDumb(
            std::vector<Face*>{replacementThicknessFace});
        builder->ReplacementFaces()->ReplaceRules(
            std::vector<SelectionIntentRule*>{replacementRule}, false);

        Features::Feature* replaceFeature = builder->CommitFeature();
        replaceFeatureTag = replaceFeature != nullptr ? replaceFeature->Tag() : NULL_TAG;
        builder->Destroy();
        builder = nullptr;
        if (replaceFeatureTag == NULL_TAG)
        {
            errorMessage = "NX did not return a feature after replacing the rip-offset face.";
            return false;
        }
        AppendDebugLog("CreateConcaveReplaceFace completed: commonFace=" +
                       std::to_string(commonFace->Tag()) +
                       ", clearanceSharedEdge=" + std::to_string(clearanceSharedEdge->Tag()) +
                       ", clearanceEdgeLength=" +
                       FormatExpressionNumber(clearanceSharedEdge->GetLength()) +
                       ", originalOffsetFace=" + std::to_string(originalOffsetFace->Tag()) +
                       ", connectionEdge=" + std::to_string(connectionEdge->Tag()) +
                       ", replacementThicknessFace=" +
                       std::to_string(replacementThicknessFace->Tag()) +
                       ", thicknessWidthEdges=(" + std::to_string(selectedWidthEdge1) +
                       "," + std::to_string(selectedWidthEdge2) + ")" +
                       ", thicknessWidthError=" +
                       FormatExpressionNumber(selectedWidthError) +
                       ", faceDistance=" +
                       FormatExpressionNumber(nearestPlaneDistance) +
                       ", replaceFeature=" + std::to_string(replaceFeatureTag));
        return true;
    }
    catch (const NXException& ex)
    {
        if (builder != nullptr)
        {
            builder->Destroy();
        }
        errorMessage = "Failed to replace the rip-offset face with the connected thickness face.\n" +
                       NxExceptionText(ex);
        AppendDebugLog(errorMessage);
        return false;
    }
#endif
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
        SetConstructionRole(
            ripFeature,
            zhihui_twopoint_sibian::kRoleEdgeRip);
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
            SetConstructionRole(
                offsetFeature,
                zhihui_twopoint_sibian::kRoleOffsetNegativeClearance);
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

bool TwoPointSiBianUI::OffsetRightAngleRipFeature(const InferredInputs& inputs,
                                                   Features::Feature* ripFeature,
                                                   double cornerInteriorAngle,
                                                   tag_t& firstOffsetTag,
                                                   tag_t& secondOffsetTag,
                                                   std::string& errorMessage,
                                                   bool swapDirectionalOffsetGroups,
                                                   bool forceDirectionalOffsetGroups,
                                                   bool largestOnlyForSecondDirectionalGroup) const
{
    firstOffsetTag = NULL_TAG;
    secondOffsetTag = NULL_TAG;
    Part* workPart = session_->Parts()->Work();
    if (workPart == nullptr || ripFeature == nullptr)
    {
        errorMessage = "The right-angle rip feature is unavailable for face offset.";
        return false;
    }

    Features::OffsetFaceBuilder* firstBuilder = nullptr;
    Features::OffsetFaceBuilder* secondBuilder = nullptr;
    try
    {
        const bool useDirectionalRightAngleOffsets =
            (inputs.featureMode == FeatureMode::NinetyLeft ||
             inputs.featureMode == FeatureMode::NinetyRight ||
             forceDirectionalOffsetGroups) &&
            (std::fabs(cornerInteriorAngle - 90.0) <= 0.1 ||
             std::fabs(cornerInteriorAngle - 270.0) <= 0.1);

        if (useDirectionalRightAngleOffsets)
        {
            // The chamfer branch uses B1 just like the left branch elsewhere
            // in this workflow, so its two directional groups follow the
            // left-side assignment.  Only NinetyRight uses the right mapping.
            const bool isNinetyLeft =
                inputs.featureMode != FeatureMode::NinetyRight;
            const bool isReflex270 =
                std::fabs(cornerInteriorAngle - 270.0) <= 0.1;
            // A reflex 270-degree corner reverses the normal 90-degree side
            // assignment for both left and right modes.
            // left 90: Y+ small;  left 270: Y- small
            // right 90: Y- small; right 270: Y+ small
            bool smallOffsetOnPositiveY = isNinetyLeft != isReflex270;
            if (swapDirectionalOffsetGroups)
            {
                smallOffsetOnPositiveY = !smallOffsetOnPositiveY;
            }
            const char* modeName =
                inputs.featureMode == FeatureMode::Chamfer
                    ? "chamfer-90/270"
                    : (isNinetyLeft ? "90-left" : "90-right");
            Vector3d xDirection = Subtract(inputs.endPoint, inputs.startPoint);
            const Point3d midpoint((inputs.startPoint.X + inputs.endPoint.X) * 0.5,
                                   (inputs.startPoint.Y + inputs.endPoint.Y) * 0.5,
                                   (inputs.startPoint.Z + inputs.endPoint.Z) * 0.5);
            Vector3d faceNormal;
            if (!Normalize(xDirection) ||
                !FaceNormalAtPoint(inputs.baseFace, midpoint, faceNormal))
            {
                errorMessage = "The local X/Y directions for the directional right-angle rip offsets could not be resolved.";
                return false;
            }
            OrientNormalAwayFromOppositeFace(inputs.targetBody,
                                             inputs.baseFace,
                                             midpoint,
                                             faceNormal);
            Vector3d yDirection = Cross(faceNormal, xDirection);
            if (!Normalize(yDirection))
            {
                errorMessage = "The local Y direction for the directional right-angle rip offsets is invalid.";
                return false;
            }

            auto facePositionSignedY = [&](Face* face, double& signedY)
            {
                std::vector<Point3d> points;
                if (face == nullptr || !FaceBoundaryPoints(face, points) || points.empty())
                {
                    return false;
                }
                Point3d center(0.0, 0.0, 0.0);
                for (const Point3d& point : points)
                {
                    center.X += point.X;
                    center.Y += point.Y;
                    center.Z += point.Z;
                }
                const double count = static_cast<double>(points.size());
                center.X /= count;
                center.Y /= count;
                center.Z /= count;
                // Classify by geometric position relative to the P1->P2
                // local X axis.  The face normal is intentionally not used:
                // Y+ / Y- means which side of that axis the rip face lies on.
                signedY = Dot(Subtract(center, inputs.startPoint), yDirection);
                return true;
            };

            auto collectFacesOnSide = [&](const std::vector<Face*>& faces,
                                           bool positiveSide,
                                           const char* stage) -> std::vector<Face*>
            {
                std::vector<Face*> selectedFaces;
                for (Face* face : faces)
                {
                    double signedY = 0.0;
                    if (!facePositionSignedY(face, signedY) ||
                        (positiveSide ? signedY <= kPlaneTolerance
                                      : signedY >= -kPlaneTolerance))
                    {
                        continue;
                    }
                    const double area = MeasureFaceArea(face);
                    AppendDebugLog(std::string(modeName) + " rip face candidate stage=" + stage +
                                   ", face=" + std::to_string(face->Tag()) +
                                    ", signedY=" + FormatExpressionNumber(signedY) +
                                    ", area=" + FormatExpressionNumber(area));
                    selectedFaces.push_back(face);
                }
                return selectedFaces;
            };

            std::vector<Face*> positiveYFaces = collectFacesOnSide(ripFeature->GetFaces(),
                                                                    true,
                                                                    "rip-positive");
            if (positiveYFaces.empty())
            {
                errorMessage = "No Y-positive rip feature face was found for the directional right-angle offset.";
                return false;
            }

            // Resolve both directional faces before changing either one. The
            // Offset Face feature normally reports only the face it moved, so
            // searching firstOffsetFeature->GetFaces() cannot find the
            // untouched opposite rip face.
            std::vector<Face*> negativeYFaces = collectFacesOnSide(ripFeature->GetFaces(),
                                                                    false,
                                                                    "rip-negative");
            if (negativeYFaces.empty())
            {
                errorMessage = "No Y-negative rip feature face was found for the directional right-angle offset.";
                return false;
            }

            const std::vector<Face*>& smallOffsetFaces = smallOffsetOnPositiveY
                                                              ? positiveYFaces
                                                              : negativeYFaces;
            const std::vector<Face*>& largeOffsetFaces = smallOffsetOnPositiveY
                                                              ? negativeYFaces
                                                              : positiveYFaces;
            std::vector<Face*> selectedLargeOffsetFaces = largeOffsetFaces;
            if (largestOnlyForSecondDirectionalGroup)
            {
                Face* largestSecondGroupFace = nullptr;
                double largestSecondGroupArea = -1.0;
                for (Face* face : largeOffsetFaces)
                {
                    const double area = MeasureFaceArea(face);
                    if (face != nullptr && area > largestSecondGroupArea)
                    {
                        largestSecondGroupFace = face;
                        largestSecondGroupArea = area;
                    }
                }
                if (largestSecondGroupFace == nullptr)
                {
                    errorMessage = "No largest face was available in the second directional rip-offset group.";
                    return false;
                }
                selectedLargeOffsetFaces.assign(1, largestSecondGroupFace);
                AppendDebugLog(std::string(modeName) +
                               " selected the largest face from the second directional group: face=" +
                               std::to_string(largestSecondGroupFace->Tag()) +
                               ", area=" +
                               FormatExpressionNumber(largestSecondGroupArea));
            }
            const char* smallSideName = smallOffsetOnPositiveY ? "Y-positive" : "Y-negative";
            const char* largeSideName = smallOffsetOnPositiveY ? "Y-negative" : "Y-positive";

            firstBuilder = workPart->Features()->CreateOffsetFaceBuilder(nullptr);
            const std::string smallOffsetFormula = "(0.02-(" + inputs.clearanceValue + "))";
            firstBuilder->Distance()->SetFormula(smallOffsetFormula.c_str());
            firstBuilder->SetDirection(false);
            FaceDumbRule* smallOffsetRule =
                workPart->ScRuleFactory()->CreateRuleFaceDumb(smallOffsetFaces);
            std::vector<SelectionIntentRule*> smallOffsetRules(1, smallOffsetRule);
            firstBuilder->FaceCollector()->ReplaceRules(smallOffsetRules, false);
            Features::Feature* firstOffsetFeature = firstBuilder->CommitFeature();
            firstOffsetTag = firstOffsetFeature != nullptr ? firstOffsetFeature->Tag() : NULL_TAG;
            SetConstructionRole(
                firstOffsetFeature,
                zhihui_twopoint_sibian::kRoleOffsetSmallDirectional);
            firstBuilder->Destroy();
            firstBuilder = nullptr;
            if (firstOffsetFeature == nullptr)
            {
                errorMessage = std::string("The ") + smallSideName +
                               " rip faces could not be offset by 0.02 minus clearance.";
                return false;
            }

            secondBuilder = workPart->Features()->CreateOffsetFaceBuilder(nullptr);
            const std::string largeOffsetFormula =
                "(" + FormatExpressionNumber(inputs.thickness) + "+0.02)";
            secondBuilder->Distance()->SetFormula(largeOffsetFormula.c_str());
            secondBuilder->SetDirection(false);
            FaceDumbRule* largeOffsetRule =
                workPart->ScRuleFactory()->CreateRuleFaceDumb(selectedLargeOffsetFaces);
            std::vector<SelectionIntentRule*> largeOffsetRules(1, largeOffsetRule);
            secondBuilder->FaceCollector()->ReplaceRules(largeOffsetRules, false);
            Features::Feature* secondOffsetFeature = secondBuilder->CommitFeature();
            secondOffsetTag = secondOffsetFeature != nullptr ? secondOffsetFeature->Tag() : NULL_TAG;
            SetConstructionRole(
                secondOffsetFeature,
                zhihui_twopoint_sibian::kRoleOffsetThicknessPlus002);
            secondBuilder->Destroy();
            secondBuilder = nullptr;
            if (secondOffsetFeature == nullptr)
            {
                errorMessage = std::string("The ") + largeSideName +
                               " rip faces could not be offset by thickness plus 0.02.";
                return false;
            }

            AppendDebugLog(std::string(modeName) + " directional rip offsets completed: rip=" +
                           std::to_string(ripFeature->Tag()) +
                           ", positiveFaceCount=" + std::to_string(positiveYFaces.size()) +
                           ", negativeFaceCount=" + std::to_string(negativeYFaces.size()) +
                           ", groupsSwapped=" +
                           (swapDirectionalOffsetGroups ? "true" : "false") +
                           ", smallSide=" + smallSideName +
                           ", smallDistance=" + smallOffsetFormula +
                           ", firstOffset=" + std::to_string(firstOffsetTag) +
                           ", largeSide=" + largeSideName +
                           ", largeDistance=" + largeOffsetFormula +
                           ", secondOffset=" + std::to_string(secondOffsetTag));
            return true;
        }

        firstBuilder = workPart->Features()->CreateOffsetFaceBuilder(nullptr);
        const std::string clearanceFormula = "-(" + inputs.clearanceValue + ")";
        firstBuilder->Distance()->SetFormula(clearanceFormula.c_str());
        firstBuilder->SetDirection(false);
        std::vector<Features::Feature*> ripFeatures(1, ripFeature);
        FaceFeatureRule* ripFaceRule =
            workPart->ScRuleFactory()->CreateRuleFaceFeature(ripFeatures);
        std::vector<SelectionIntentRule*> ripRules(1, ripFaceRule);
        firstBuilder->FaceCollector()->ReplaceRules(ripRules, false);
        Features::Feature* firstOffsetFeature = firstBuilder->CommitFeature();
        firstOffsetTag = firstOffsetFeature != nullptr ? firstOffsetFeature->Tag() : NULL_TAG;
        SetConstructionRole(
            firstOffsetFeature,
            zhihui_twopoint_sibian::kRoleOffsetNegativeClearance);
        firstBuilder->Destroy();
        firstBuilder = nullptr;
        if (firstOffsetFeature == nullptr)
        {
            errorMessage = "The 90/270-degree rip faces could not be offset by the clearance value.";
            return false;
        }

        Face* largestFace = nullptr;
        double largestArea = -1.0;
        for (Face* face : firstOffsetFeature->GetFaces())
        {
            const double area = MeasureFaceArea(face);
            if (face != nullptr && area > largestArea)
            {
                largestFace = face;
                largestArea = area;
            }
        }
        if (largestFace == nullptr)
        {
            errorMessage = "No feature face was available after the clearance offset.";
            return false;
        }

        secondBuilder = workPart->Features()->CreateOffsetFaceBuilder(nullptr);
        const std::string thicknessAndClearanceFormula =
            "(" + FormatExpressionNumber(inputs.thickness) + "+(" +
            inputs.clearanceValue + "))";
        secondBuilder->Distance()->SetFormula(thicknessAndClearanceFormula.c_str());
        secondBuilder->SetDirection(false);
        std::vector<Face*> largestFaces(1, largestFace);
        FaceDumbRule* largestFaceRule =
            workPart->ScRuleFactory()->CreateRuleFaceDumb(largestFaces);
        std::vector<SelectionIntentRule*> largestRules(1, largestFaceRule);
        secondBuilder->FaceCollector()->ReplaceRules(largestRules, false);
        Features::Feature* secondOffsetFeature = secondBuilder->CommitFeature();
        secondOffsetTag = secondOffsetFeature != nullptr ? secondOffsetFeature->Tag() : NULL_TAG;
        SetConstructionRole(
            secondOffsetFeature,
            zhihui_twopoint_sibian::kRoleOffsetThicknessPlusClearance);
        secondBuilder->Destroy();
        secondBuilder = nullptr;
        if (secondOffsetFeature == nullptr)
        {
            errorMessage = "The largest offset-feature face could not be offset by thickness plus clearance.";
            return false;
        }

        AppendDebugLog("90/270-degree rip offsets completed: rip=" +
                       std::to_string(ripFeature->Tag()) +
                       ", firstOffset=" + std::to_string(firstOffsetTag) +
                       ", firstDistance=" + clearanceFormula +
                       ", largestFace=" + std::to_string(largestFace->Tag()) +
                       ", largestArea=" + FormatExpressionNumber(largestArea) +
                       ", secondOffset=" + std::to_string(secondOffsetTag) +
                       ", secondDistance=" + thicknessAndClearanceFormula);
        return true;
    }
    catch (const NXException& ex)
    {
        if (firstBuilder != nullptr) firstBuilder->Destroy();
        if (secondBuilder != nullptr) secondBuilder->Destroy();
        errorMessage = "Failed to offset the 90/270-degree rip feature.\n" + NxExceptionText(ex);
        AppendDebugLog(errorMessage);
        return false;
    }
    catch (const std::exception& ex)
    {
        if (firstBuilder != nullptr) firstBuilder->Destroy();
        if (secondBuilder != nullptr) secondBuilder->Destroy();
        errorMessage = std::string("Failed to offset the 90/270-degree rip feature.\n") + ex.what();
        AppendDebugLog(errorMessage);
        return false;
    }
}

bool TwoPointSiBianUI::TryCreateSecondPointRip(const InferredInputs& inputs,
                                                bool allowContinuationInputs,
                                                bool& ripCreated,
                                                tag_t& secondUdfTag,
                                                std::vector<tag_t>& secondToolBodyTags,
                                                std::vector<tag_t>& secondReferenceTags,
                                                 bool& continuationCreated,
                                                 InferredInputs& continuationInputs,
                                                 bool& deferredSecondUdfRequested,
                                                 InferredInputs& deferredSecondUdfInputs,
                                                 tag_t& deferredRightAngleRipTag,
                                                 double& deferredRightAngleRipAngle,
                                                 std::vector<tag_t>& createdRightAngleOffsetTags,
                                                 bool& createdRightAngle90SecondFeaturePath,
                                                 bool& primaryUdfCreatedBeforeRip,
                                                 tag_t& primarySubtractTag,
                                                 std::vector<tag_t>& primaryReferenceTags,
                                                 std::string& errorMessage) const
{
    ripCreated = false;
    secondUdfTag = NULL_TAG;
    secondToolBodyTags.clear();
    secondReferenceTags.clear();
    continuationCreated = false;
    deferredSecondUdfRequested = false;
    deferredRightAngleRipTag = NULL_TAG;
    deferredRightAngleRipAngle = 0.0;
    createdRightAngleOffsetTags.clear();
    createdRightAngle90SecondFeaturePath = false;
    primaryUdfCreatedBeforeRip = false;
    primarySubtractTag = NULL_TAG;
    primaryReferenceTags.clear();
    errorMessage.clear();
    if ((!inputs.inferredFromSingleClick && !allowContinuationInputs) ||
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

    Vector3d bAxisX = Subtract(inputs.endPoint, inputs.startPoint);
    const Point3d bAxisSample((inputs.startPoint.X + inputs.endPoint.X) * 0.5,
                              (inputs.startPoint.Y + inputs.endPoint.Y) * 0.5,
                              (inputs.startPoint.Z + inputs.endPoint.Z) * 0.5);
    Vector3d bAxisZ;
    if (!Normalize(bAxisX) ||
        !FaceNormalAtPoint(inputs.baseFace, bAxisSample, bAxisZ))
    {
        errorMessage = "The P1-P2 X axis or selected-face Z axis could not be resolved for B1/B2 ordering.";
        return false;
    }
    OrientNormalAwayFromOppositeFace(inputs.targetBody,
                                     inputs.baseFace,
                                     bAxisSample,
                                     bAxisZ);
    Vector3d bAxisY = Cross(bAxisZ, bAxisX);
    if (!Normalize(bAxisY))
    {
        errorMessage = "The P1-P2 local Y axis could not be resolved for B1/B2 ordering.";
        return false;
    }
    auto orderB1PositiveB2Negative = [&](Edge* firstCandidate,
                                         Edge* secondCandidate,
                                         const Point3d& qPoint,
                                         Edge*& b1,
                                         Edge*& b2,
                                         double& b1SignedY,
                                         double& b2SignedY)
    {
        b1 = nullptr;
        b2 = nullptr;
        Point3d firstOther;
        Point3d secondOther;
        if (!EdgeOtherPoint(firstCandidate, qPoint, firstOther) ||
            !EdgeOtherPoint(secondCandidate, qPoint, secondOther))
        {
            return false;
        }
        const double firstSignedY =
            Dot(Subtract(firstOther, qPoint), bAxisY);
        const double secondSignedY =
            Dot(Subtract(secondOther, qPoint), bAxisY);
        if (firstSignedY > kPlaneTolerance &&
            secondSignedY < -kPlaneTolerance)
        {
            b1 = firstCandidate;
            b2 = secondCandidate;
            b1SignedY = firstSignedY;
            b2SignedY = secondSignedY;
            return true;
        }
        if (secondSignedY > kPlaneTolerance &&
            firstSignedY < -kPlaneTolerance)
        {
            b1 = secondCandidate;
            b2 = firstCandidate;
            b1SignedY = secondSignedY;
            b2SignedY = firstSignedY;
            return true;
        }
        return false;
    };
    AppendDebugLog("B1/B2 local axes fixed from P1-P2: X=(" +
                   FormatExpressionNumber(bAxisX.X) + "," +
                   FormatExpressionNumber(bAxisX.Y) + "," +
                   FormatExpressionNumber(bAxisX.Z) + "), Y=(" +
                   FormatExpressionNumber(bAxisY.X) + "," +
                   FormatExpressionNumber(bAxisY.Y) + "," +
                   FormatExpressionNumber(bAxisY.Z) + "), Z=(" +
                   FormatExpressionNumber(bAxisZ.X) + "," +
                   FormatExpressionNumber(bAxisZ.Y) + "," +
                   FormatExpressionNumber(bAxisZ.Z) + ")");

    Edge* ripEdge = nullptr;
    Edge* confirmingEdge = nullptr;
    Edge* secondaryQEdge = nullptr;
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
    bool concaveQ3HasThicknessEdge = false;
    InferredInputs concaveSecondInputs;
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
                    Edge* candidateB1 = nullptr;
                    Edge* candidateB2 = nullptr;
                    double candidateB1SignedY = 0.0;
                    double candidateB2SignedY = 0.0;
                    if (!orderB1PositiveB2Negative(remainingEdgesAtQ[firstIndex],
                                                   remainingEdgesAtQ[secondIndex],
                                                   candidateFarEndpoint,
                                                   candidateB1,
                                                   candidateB2,
                                                   candidateB1SignedY,
                                                   candidateB2SignedY))
                    {
                        continue;
                    }
                    Face* commonFace = FindPlanarFaceContainingEdges(
                        inputs.targetBody,
                        candidateB1,
                        candidateB2);
                    if (commonFace == nullptr)
                    {
                        continue;
                    }
                    AppendDebugLog("ordered Q edges by P1-P2 local Y: Q=" +
                                   FormatPoint(candidateFarEndpoint) +
                                   ", B1=" + std::to_string(candidateB1->Tag()) +
                                   ", B1SignedY=" +
                                   FormatExpressionNumber(candidateB1SignedY) +
                                   ", B2=" + std::to_string(candidateB2->Tag()) +
                                   ", B2SignedY=" +
                                   FormatExpressionNumber(candidateB2SignedY) +
                                   ", commonFace=" +
                                   std::to_string(commonFace->Tag()));
                    Edge* candidateAuxiliaryRip = nullptr;
                    Edge* candidateAuxiliaryParallel = nullptr;
                    if (FindAuxiliaryRipPair(inputs.targetBody,
                                             commonFace,
                                             candidateB1,
                                             candidateB2,
                                             inputs.thickness,
                                             candidateAuxiliaryRip,
                                             candidateAuxiliaryParallel))
                    {
                        fallbackFirstEdge = candidateB1;
                        fallbackSecondEdge = candidateB2;
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
                                               candidateB1,
                                               candidateB2,
                                               candidateFarEndpoint,
                                               candidateQFirstInputs);
                    bool candidateHasConcaveStripPlan =
                        BuildConcaveStripPlan(inputs,
                                              candidateB1,
                                              candidateB2,
                                              candidateFarEndpoint,
                                              candidateQ3,
                                              candidateQ4,
                                              candidatePlaneNormal);
                    Edge* candidateQ3ThicknessEdge = nullptr;
                    const bool candidateQ3HasThicknessEdge =
                        candidateHasConcaveStripPlan &&
                        PointHasThicknessLengthEdge(inputs.targetBody,
                                                    candidateQ3,
                                                    inputs.thickness,
                                                    nullptr,
                                                    nullptr,
                                                    &candidateQ3ThicknessEdge);
                    InferredInputs candidateConcaveSecondInputs;
                    if (candidateHasConcaveStripPlan)
                    {
                        // Store only the Q3/face seed. Q4 is always resolved
                        // after P2-Q is ripped, from the updated face boundary.
                        candidateConcaveSecondInputs = inputs;
                        candidateConcaveSecondInputs.inferredFromSingleClick = false;
                        candidateConcaveSecondInputs.startObject = commonFace;
                        candidateConcaveSecondInputs.endObject = commonFace;
                        candidateConcaveSecondInputs.startPoint = candidateQ3;
                        candidateConcaveSecondInputs.endPoint = candidateQ3;
                        candidateConcaveSecondInputs.baseFace = commonFace;
                        candidateConcaveSecondInputs.startEdge = nullptr;
                        candidateConcaveSecondInputs.endEdge = nullptr;
                        candidateConcaveSecondInputs.startPositiveYEdge = nullptr;
                        candidateConcaveSecondInputs.startNegativeYEdge = nullptr;
                        candidateConcaveSecondInputs.endPositiveYEdge = nullptr;
                        candidateConcaveSecondInputs.endNegativeYEdge = nullptr;
                        if (!candidateQ3HasThicknessEdge && !candidateHasQFirstInputs)
                        {
                            AppendDebugLog("concave Q3 continuation rejected: Q-to-Q3 search inputs are unavailable"
                                           ", commonFace=" +
                                           std::to_string(commonFace != nullptr
                                                              ? commonFace->Tag()
                                                              : NULL_TAG) +
                                           ", Q3=" + FormatPoint(candidateQ3));
                            candidateHasConcaveStripPlan = false;
                        }
                    }
                    if (candidateHasQFirstInputs || candidateHasConcaveStripPlan)
                    {
                        fallbackFirstEdge = candidateB1;
                        fallbackSecondEdge = candidateB2;
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
                            concaveQ3HasThicknessEdge = candidateQ3HasThicknessEdge;
                            concaveSecondInputs = candidateConcaveSecondInputs;
                            hasConcaveStripCandidate = true;
                        }
                        break;
                    }
                    if (BuildFallbackSecondInputs(inputs,
                                                  candidateB1,
                                                  candidateB2,
                                                  candidateFarEndpoint,
                                                  candidateFallbackInputs))
                    {
                        fallbackFirstEdge = candidateB1;
                        fallbackSecondEdge = candidateB2;
                        break;
                    }
                }
            }
        }
        if (ripEdge == nullptr && fallbackFirstEdge != nullptr && fallbackSecondEdge != nullptr)
        {
            ripEdge = candidateRipEdge;
            confirmingEdge = fallbackFirstEdge;
            secondaryQEdge = fallbackSecondEdge;
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

    bool qCornerRequiresRipOffsets = false;
    double qCornerInteriorAngle = 0.0;
    tag_t qCornerFirstEdgeTag = NULL_TAG;
    tag_t qCornerSecondEdgeTag = NULL_TAG;
    std::vector<Edge*> qConnectedEdges;
    for (Edge* edge : inputs.targetBody->GetEdges())
    {
        if (edge != nullptr && edge != ripEdge && EdgeTouchesPoint(edge, farEndpoint))
        {
            qConnectedEdges.push_back(edge);
        }
    }
    for (std::size_t firstIndex = 0;
         firstIndex < qConnectedEdges.size() && !qCornerRequiresRipOffsets;
         ++firstIndex)
    {
        for (std::size_t secondIndex = firstIndex + 1;
             secondIndex < qConnectedEdges.size();
             ++secondIndex)
        {
            Edge* orderedB1 = nullptr;
            Edge* orderedB2 = nullptr;
            double orderedB1SignedY = 0.0;
            double orderedB2SignedY = 0.0;
            if (!orderB1PositiveB2Negative(qConnectedEdges[firstIndex],
                                           qConnectedEdges[secondIndex],
                                           farEndpoint,
                                           orderedB1,
                                           orderedB2,
                                           orderedB1SignedY,
                                           orderedB2SignedY))
            {
                continue;
            }
            Face* commonFace = FindPlanarFaceContainingEdges(inputs.targetBody,
                                                             orderedB1,
                                                             orderedB2);
            double candidateAngle = 0.0;
            if (commonFace != nullptr &&
                FaceInteriorCornerAngle(commonFace, farEndpoint, candidateAngle) &&
                (std::fabs(candidateAngle - 90.0) <= 0.1 ||
                 std::fabs(candidateAngle - 270.0) <= 0.1))
            {
                qCornerRequiresRipOffsets = true;
                qCornerInteriorAngle = candidateAngle;
                qCornerFirstEdgeTag = orderedB1->Tag();
                qCornerSecondEdgeTag = orderedB2->Tag();
                AppendDebugLog("90/270 Q-corner ordered B1/B2: B1SignedY=" +
                               FormatExpressionNumber(orderedB1SignedY) +
                               ", B2SignedY=" +
                               FormatExpressionNumber(orderedB2SignedY));
                break;
            }
        }
    }
    AppendDebugLog("P2-Q rip 90/270-degree offset check: Q=" + FormatPoint(farEndpoint) +
                   ", angle=" + FormatExpressionNumber(qCornerInteriorAngle) +
                   ", B1=" + std::to_string(qCornerFirstEdgeTag) +
                   ", B2=" + std::to_string(qCornerSecondEdgeTag) +
                   ", execute=" + (qCornerRequiresRipOffsets ? "true" : "false"));
    const bool isNinetyRightMode =
        inputs.featureMode == FeatureMode::NinetyRight;
    const bool isNinetyLeftMode =
        inputs.featureMode == FeatureMode::NinetyLeft;
    const bool useRightAngleRipOffsets =
        qCornerRequiresRipOffsets && (isNinetyLeftMode || isNinetyRightMode);
    const bool qCornerIs90 =
        std::fabs(qCornerInteriorAngle - 90.0) <= 0.1;
    const bool qCornerIs270 =
        std::fabs(qCornerInteriorAngle - 270.0) <= 0.1;

    // The second clearance-groove UDF belongs to the originally selected P2
    // corner.  Do not classify that UDF from the far Q end of P2-Q: on a
    // thickness wall the two ends commonly report complementary 270/90
    // angles, which swaps JianXiCao and JianXiCaoR.
    double secondFeatureCornerAngle = qCornerInteriorAngle;
    const bool hasSelectedP2CornerAngle =
        FaceInteriorCornerAngle(inputs.baseFace,
                                inputs.endPoint,
                                secondFeatureCornerAngle);
    const bool secondFeatureCornerIs90 =
        std::fabs(secondFeatureCornerAngle - 90.0) <= 0.1;
    const bool secondFeatureCornerIs270 =
        std::fabs(secondFeatureCornerAngle - 270.0) <= 0.1;
    AppendDebugLog("second UDF corner classification: selectedP2=" +
                   FormatPoint(inputs.endPoint) +
                   ", selectedP2Angle=" +
                   FormatExpressionNumber(secondFeatureCornerAngle) +
                   ", hasSelectedP2Angle=" +
                   (hasSelectedP2CornerAngle ? "true" : "false") +
                   ", farQAngle=" +
                   FormatExpressionNumber(qCornerInteriorAngle));

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
    const bool confirmedByQThicknessPair =
        confirmingEdge != nullptr && confirmingParallelEdge != nullptr;
    if (confirmedByQThicknessPair)
    {
        // B1 or B2 already has an overlapping parallel mate whose minimum
        // distance equals the sheet thickness.  That pair confirms the sheet
        // corner, so P2-Q and its rip mate do not also need equal lengths.
        requiredPairedRipLength = pairedRipEdgeLength;
        selectedLengthRule = "Q-edge parallel at thickness: P2-Q equality not required";
    }
    else if (hasQFirstEqualCandidate || hasConcaveStripCandidate)
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
        InferredInputs secondUdfCreationInputs = fallbackSecondInputs;
        if ((isNinetyLeftMode && secondFeatureCornerIs90) ||
            (isNinetyRightMode && secondFeatureCornerIs270))
        {
            secondUdfCreationInputs.useNinetyClearanceGrooveTemplate = true;
            AppendDebugLog("TryCreateSecondPointRip selected 90JianXiCao for the second UDF, mode=" +
                           std::string(isNinetyRightMode ? "90-right" : "90-left") +
                           ", cornerAngle=" + FormatExpressionNumber(secondFeatureCornerAngle));
        }
        else if ((isNinetyRightMode && secondFeatureCornerIs90) ||
                 (isNinetyLeftMode && secondFeatureCornerIs270))
        {
            secondUdfCreationInputs.useNinetyClearanceGrooveRightTemplate = true;
            AppendDebugLog("TryCreateSecondPointRip selected 90JianXiCaoR for the second UDF, mode=" +
                           std::string(isNinetyRightMode ? "90-right" : "90-left") +
                           ", cornerAngle=" + FormatExpressionNumber(secondFeatureCornerAngle));
        }
        if (!CreateUserDefinedFeature(secondUdfCreationInputs,
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
        continuationInputs = fallbackSecondInputs;
        Edge* terminalThicknessEdge = nullptr;
        const bool endpointHasThicknessEdge =
            PointHasThicknessLengthEdge(inputs.targetBody,
                                        fallbackSecondInputs.endPoint,
                                        inputs.thickness,
                                        nullptr,
                                        nullptr,
                                        &terminalThicknessEdge);
        continuationCreated = !endpointHasThicknessEdge;
        AppendDebugLog("TryCreateSecondPointRip continuation endpoint check: endpoint=" +
                       FormatPoint(fallbackSecondInputs.endPoint) +
                       ", thicknessEdge=" +
                       std::to_string(terminalThicknessEdge != nullptr
                                          ? terminalThicknessEdge->Tag()
                                          : NULL_TAG) +
                       ", continue=" + (continuationCreated ? "true" : "false"));
    }

    // A reflex chamfer primary UDF must keep the original P2 topology during
    // later parameter edits.  This was already required by the P2Q+2T path;
    // the Q-first-equal path has the same requirement.  If its primary UDF is
    // created after the rip, RefreshSmartInputsAfterRips moves P2 onto a short
    // rip edge (for example by 0.02), and that endpoint then moves whenever
    // the clearance changes.
    const bool reverseConcaveChamferP2Q2T =
        useConcaveStripBranch &&
        inputs.featureMode == FeatureMode::Chamfer &&
        secondFeatureCornerAngle >
            kConvexCornerMaximumAngleDegrees + 1.0e-6 &&
        inputs.reverseChamfer270Cut;
    AppendDebugLog(
        std::string("concave P2Q+2T reverse-Y decision: requested=") +
        (inputs.reverseChamfer270Cut ? "true" : "false") +
        ", branch=" + (useConcaveStripBranch ? "true" : "false") +
        ", selectedP2Angle=" +
        FormatExpressionNumber(secondFeatureCornerAngle) +
        ", angleGreaterThan180=" +
        (secondFeatureCornerAngle >
                 kConvexCornerMaximumAngleDegrees + 1.0e-6
             ? "true"
             : "false") +
        ", effective=" +
        (reverseConcaveChamferP2Q2T ? "true" : "false"));
    const bool useEarlyPrimaryChamfer270P2Q2T =
        useConcaveStripBranch &&
        concaveQ3HasThicknessEdge &&
        inputs.featureMode == FeatureMode::Chamfer &&
        qCornerIs270;
    const bool useEarlyPrimaryChamfer270Equal =
        useFallbackSecondUdf &&
        inputs.featureMode == FeatureMode::Chamfer &&
        secondFeatureCornerAngle >
            kConvexCornerMaximumAngleDegrees + 1.0e-6;
    const bool useEarlyPrimaryChamfer =
        useEarlyPrimaryChamfer270P2Q2T ||
        useEarlyPrimaryChamfer270Equal;
    tag_t earlyPrimaryUdfTag = NULL_TAG;
    std::vector<tag_t> earlyPrimaryToolBodyTags;
    if (useEarlyPrimaryChamfer)
    {
        if (!CreateUserDefinedFeature(inputs,
                                      errorMessage,
                                      &earlyPrimaryUdfTag,
                                      &primaryReferenceTags,
                                      &earlyPrimaryToolBodyTags))
        {
            return false;
        }
        AppendDebugLog("TryCreateSecondPointRip created the primary UDF from original P1/P2 before the chamfer-270 P2-Q rip: branch=" +
                       std::string(useEarlyPrimaryChamfer270Equal
                                       ? "Q-first-equal"
                                       : "P2Q+2T") +
                       ", udf=" +
                       std::to_string(earlyPrimaryUdfTag) +
                       ", toolBodies=" +
                       std::to_string(earlyPrimaryToolBodyTags.size()));
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
        SetConstructionRole(
            createdRip,
            zhihui_twopoint_sibian::kRoleEdgeRip);
        builder->Destroy();
        builder = nullptr;
        section->Destroy();
        section = nullptr;
        if (createdRipTag == NULL_TAG)
        {
            errorMessage = "NX did not return a feature after creating the sheet-metal rip.";
            return false;
        }
        if (useEarlyPrimaryChamfer)
        {
            if (!SubtractToolBodies(inputs.targetBody,
                                    earlyPrimaryToolBodyTags,
                                    primarySubtractTag,
                                    errorMessage))
            {
                return false;
            }
            primaryUdfCreatedBeforeRip = true;
            AppendDebugLog("TryCreateSecondPointRip subtracted the original-P2 primary UDF immediately after the chamfer-270 P2-Q rip: branch=" +
                           std::string(useEarlyPrimaryChamfer270Equal
                                           ? "Q-first-equal"
                                           : "P2Q+2T") +
                           ", udf=" +
                           std::to_string(earlyPrimaryUdfTag) +
                           ", rip=" + std::to_string(createdRipTag) +
                           ", subtract=" +
                           std::to_string(primarySubtractTag));
        }
        const bool forceImmediateConcaveChamferOffsets =
            useConcaveStripBranch &&
            inputs.featureMode == FeatureMode::Chamfer &&
            qCornerRequiresRipOffsets;
        if ((useRightAngleRipOffsets || forceImmediateConcaveChamferOffsets) &&
            (std::fabs(qCornerInteriorAngle - 90.0) <= 0.1 ||
             std::fabs(qCornerInteriorAngle - 270.0) <= 0.1))
        {
            tag_t firstOffsetTag = NULL_TAG;
            tag_t secondOffsetTag = NULL_TAG;
            // The dialog reversal is intentionally limited to the chamfer
            // P2Q+2T branch when B1/B2 form a reflex 270-degree corner.
            // A 90-degree B1/B2 corner and every straight-left/right branch
            // retain the established directional assignment.
            const bool reverseOnlyChamfer270 =
                useConcaveStripBranch &&
                inputs.featureMode == FeatureMode::Chamfer &&
                qCornerIs270 &&
                inputs.reverseChamfer270Cut;
            const bool swapDirectionalOffsetGroups =
                (useConcaveStripBranch && qCornerIs270) != reverseOnlyChamfer270;
            AppendDebugLog(std::string("chamfer-270 cut-direction decision: requested=") +
                           (inputs.reverseChamfer270Cut ? "true" : "false") +
                           ", applicable=" +
                           ((useConcaveStripBranch &&
                             inputs.featureMode == FeatureMode::Chamfer &&
                             qCornerIs270)
                                ? "true"
                                : "false") +
                           ", effectiveSwap=" +
                           (swapDirectionalOffsetGroups ? "true" : "false") +
                           ", cornerAngle=" +
                           FormatExpressionNumber(qCornerInteriorAngle));
            if (!OffsetRightAngleRipFeature(inputs,
                                            createdRip,
                                            qCornerInteriorAngle,
                                            firstOffsetTag,
                                            secondOffsetTag,
                                            errorMessage,
                                            swapDirectionalOffsetGroups,
                                            forceImmediateConcaveChamferOffsets,
                                            useEarlyPrimaryChamfer270P2Q2T))
            {
                return false;
            }
            AppendDebugLog("TryCreateSecondPointRip applied two-group 90/270-degree rip-face offsets immediately after rip: angle=" +
                           FormatExpressionNumber(qCornerInteriorAngle) +
                           ", mode=" +
                           (inputs.featureMode == FeatureMode::Chamfer
                                ? "chamfer"
                                : (inputs.featureMode == FeatureMode::NinetyRight
                                       ? "90-right"
                                       : "90-left")) +
                           ", rip=" + std::to_string(createdRipTag) +
                           ", firstOffset=" + std::to_string(firstOffsetTag) +
                           ", secondOffset=" + std::to_string(secondOffsetTag));
            createdRightAngleOffsetTags.push_back(firstOffsetTag);
            createdRightAngleOffsetTags.push_back(secondOffsetTag);
        }
        else if (qCornerRequiresRipOffsets)
        {
            // Preserve the original behavior for every other chamfer path.
            deferredRightAngleRipTag = createdRipTag;
            deferredRightAngleRipAngle = qCornerInteriorAngle;
            AppendDebugLog("TryCreateSecondPointRip retained legacy deferred 90/270 rip-face offsets outside the concave P2Q+2T branch: angle=" +
                           FormatExpressionNumber(qCornerInteriorAngle) +
                           ", rip=" + std::to_string(createdRipTag));
        }
        if (useConcaveStripBranch)
        {
            // P2-Q has now been ripped. Resolve Q4 only from the updated
            // peripheral endpoints. The only excluded point is Q3 itself.
            std::vector<Point3d> updatedBoundaryPoints;
            if (!FaceBoundaryPoints(concaveSecondInputs.baseFace,
                                    updatedBoundaryPoints))
            {
                errorMessage = "The Q3 plane boundary could not be read after the P2-Q rip.";
                return false;
            }
            Point3d resolvedQ4;
            double resolvedQ4Distance = std::numeric_limits<double>::max();
            bool foundResolvedQ4 = false;
            for (const Point3d& boundaryPoint : updatedBoundaryPoints)
            {
                const double distance = Distance(concaveQ3, boundaryPoint);
                if (distance > kPointTolerance && distance < resolvedQ4Distance)
                {
                    resolvedQ4 = boundaryPoint;
                    resolvedQ4Distance = distance;
                    foundResolvedQ4 = true;
                }
            }
            if (!foundResolvedQ4)
            {
                errorMessage = "No peripheral Q4 point could be found from Q3 after the P2-Q rip.";
                return false;
            }
            concaveQ4 = resolvedQ4;
            AppendDebugLog("TryCreateSecondPointRip resolved Q4 after P2-Q rip"
                           ", Q3=" + FormatPoint(concaveQ3) +
                           ", Q4=" + FormatPoint(concaveQ4) +
                           ", Q4DistanceFromQ3=" +
                           FormatExpressionNumber(resolvedQ4Distance) +
                           ", selection=nearest updated peripheral endpoint excluding only Q3");
            if (concaveQ3HasThicknessEdge)
            {
                if (useRightAngleRipOffsets || forceImmediateConcaveChamferOffsets)
                {
                    // 90/270-degree modes create their directional offset
                    // groups immediately.  The P2Q+2T concave chamfer branch
                    // now uses the same offset results for its -60 clearance
                    // face and must not fall through to rectangle extrusion.
                    tag_t clearanceOffsetFeatureTag = NULL_TAG;
                    if (!OffsetConcaveClearanceFace(inputs,
                                                    concaveSecondInputs.baseFace,
                                                    createdRightAngleOffsetTags,
                                                    clearanceOffsetFeatureTag,
                                                    errorMessage,
                                                    useEarlyPrimaryChamfer270P2Q2T))
                    {
                        return false;
                    }
                    AppendDebugLog("TryCreateSecondPointRip applied -60 clearance-face offset for 90/270-degree branch, feature=" +
                                   std::to_string(clearanceOffsetFeatureTag));
                }
                else
                {
                    tag_t stripSubtractTag = NULL_TAG;
                    const bool useNegativeYReference =
                        inputs.featureMode == FeatureMode::NinetyRight ||
                        reverseConcaveChamferP2Q2T;
                    Edge* rectangleReferenceBEdge =
                        useNegativeYReference
                            ? secondaryQEdge
                            : confirmingEdge;
                    if (rectangleReferenceBEdge == nullptr)
                    {
                        errorMessage = "The ordered B edge required for the oblique clearance rectangle is unavailable.";
                        return false;
                    }
                    if (!CreateObliqueClearanceCut(
                            inputs,
                            rectangleReferenceBEdge,
                            ripEdge,
                            concaveSecondInputs.baseFace,
                            farEndpoint,
                            concavePlaneNormal,
                            stripSubtractTag,
                            errorMessage))
                    {
                        return false;
                    }
                    AppendDebugLog("TryCreateSecondPointRip created B-edge-directed clearance rectangle for non-90/270 P2Q+2T branch, mode=" +
                                    std::string(inputs.featureMode == FeatureMode::NinetyLeft
                                                    ? "90-left"
                                                    : (inputs.featureMode == FeatureMode::NinetyRight
                                                           ? "90-right"
                                                           : "chamfer")) +
                                    ", reference=" +
                                    (useNegativeYReference
                                         ? "B2"
                                         : "B1") +
                                    ", reverseGreaterThan180=" +
                                    (reverseConcaveChamferP2Q2T
                                         ? "true"
                                         : "false") +
                                    ", referenceEdge=" +
                                   std::to_string(rectangleReferenceBEdge->Tag()) +
                                   ", feature=" +
                                   std::to_string(stripSubtractTag));
                }
                AppendDebugLog("TryCreateSecondPointRip concave chain stopped at Q3: Q3 has a thickness-length edge"
                               ", Q3=" + FormatPoint(concaveQ3));
            }
            else
            {
                AppendDebugLog("TryCreateSecondPointRip concave strip extrusion skipped: Q3 has no thickness-length edge"
                               ", Q3=" + FormatPoint(concaveQ3) +
                               ", deferSecondUdf=true");
                concaveSecondInputs.startPoint = concaveQ3;
                concaveSecondInputs.endPoint = resolvedQ4;
                concaveSecondInputs.startEdge = nullptr;
                concaveSecondInputs.endEdge = nullptr;
                concaveSecondInputs.startPositiveYEdge = nullptr;
                concaveSecondInputs.startNegativeYEdge = nullptr;
                concaveSecondInputs.endPositiveYEdge = nullptr;
                concaveSecondInputs.endNegativeYEdge = nullptr;
                if (!CompleteInputsForEndpoints(concaveSecondInputs))
                {
                    errorMessage = "The deferred Q3-Q4 custom feature inputs could not be completed after the P2-Q rip.";
                    AppendDebugLog("TryCreateSecondPointRip deferred Q3-Q4 inputs rejected after P2-Q rip"
                                   ", Q3=" + FormatPoint(concaveQ3) +
                                   ", Q4=" + FormatPoint(resolvedQ4) +
                                   ", Q4DistanceFromQ3=" +
                                   FormatExpressionNumber(resolvedQ4Distance));
                    return false;
                }
                deferredSecondUdfRequested = true;
                deferredSecondUdfInputs = concaveSecondInputs;
                if ((isNinetyLeftMode && secondFeatureCornerIs90) ||
                    (isNinetyRightMode && secondFeatureCornerIs270))
                {
                    deferredSecondUdfInputs.useNinetyClearanceGrooveTemplate = true;
                    AppendDebugLog("TryCreateSecondPointRip selected 90JianXiCao for the deferred second UDF, mode=" +
                                   std::string(isNinetyRightMode ? "90-right" : "90-left") +
                                   ", cornerAngle=" + FormatExpressionNumber(secondFeatureCornerAngle));
                }
                else if ((isNinetyRightMode && secondFeatureCornerIs90) ||
                         (isNinetyLeftMode && secondFeatureCornerIs270))
                {
                    deferredSecondUdfInputs.useNinetyClearanceGrooveRightTemplate = true;
                    AppendDebugLog("TryCreateSecondPointRip selected 90JianXiCaoR for the deferred second UDF, mode=" +
                                   std::string(isNinetyRightMode ? "90-right" : "90-left") +
                                   ", cornerAngle=" + FormatExpressionNumber(secondFeatureCornerAngle));
                }

                // The next iteration still evaluates Q3, not Q4.
                continuationInputs = fallbackSecondInputs;
                continuationCreated = true;
                AppendDebugLog("TryCreateSecondPointRip concave continuation prepared without second UDF"
                               ", Q3=" + FormatPoint(concaveQ3) +
                               ", Q4=" + FormatPoint(resolvedQ4) +
                               ", Q4DistanceFromQ3=" +
                               FormatExpressionNumber(resolvedQ4Distance) +
                               ", continuationStart=Q" +
                               ", continuationSearchEndpoint=Q3" +
                               ", continue=true");
            }
        }
        createdRightAngle90SecondFeaturePath =
            (inputs.featureMode == FeatureMode::NinetyLeft ||
             inputs.featureMode == FeatureMode::NinetyRight) &&
            ((qCornerIs90 &&
              (secondUdfTag != NULL_TAG || deferredSecondUdfRequested)) ||
             (qCornerIs270 && useConcaveStripBranch));
        if (createdRightAngle90SecondFeaturePath)
        {
            AppendDebugLog("TryCreateSecondPointRip marked right-angle path for offset-face-edge constrained primary P2 recalculation, mode=" +
                           std::string(inputs.featureMode == FeatureMode::NinetyLeft
                                           ? "90-left"
                                           : "90-right") +
                           ", cornerAngle=" + FormatExpressionNumber(qCornerInteriorAngle) +
                           ", concaveStrip=" + (useConcaveStripBranch ? "true" : "false"));
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

        const double baseArea = MeasureFaceArea(baseFace);
        if (baseArea <= kPointTolerance)
        {
            AppendDebugLog("EstimateSheetThickness failed: the selected face area is zero.");
            return 0.0;
        }

        std::ostringstream trace;
        trace << "EstimateSheetThickness baseFace=" << baseFace->Tag()
              << ", baseArea=" << baseArea
              << ", selectionMethod=nearest parallel plane group by unsigned normal distance"
              << ", minimumAreaRatio=" << kThicknessMinimumAreaRatio
              << ", overlapValidation=disabled"
              << ", basePlanePoint=" << FormatPoint(basePlanePoint);

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
            const double normalDot = Dot(baseNormal, faceNormal);
            const double parallel = std::fabs(normalDot);
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

            // Do not use the selected face's normal sign to decide which side
            // can contain the thickness face. NX/UF face normals may be
            // reversed by imports and boolean operations. Parallel faces on
            // both sides remain candidates. The smallest unsigned normal
            // distance whose parallel face group has enough total area is
            // the sheet thickness; no projected-overlap test is performed.
            candidates.push_back(
                {face, planeDistance, signedPlaneDistance, normalDot});
        }

        std::sort(candidates.begin(), candidates.end(), [](const ThicknessCandidate& first, const ThicknessCandidate& second) {
            return first.planeDistance < second.planeDistance;
        });

        struct ThicknessPlaneGroup
        {
            double signedPlaneDistance = 0.0;
            double planeDistance = 0.0;
            double totalArea = 0.0;
            std::vector<ThicknessCandidate> candidates;
        };

        std::vector<ThicknessPlaneGroup> groups;
        const double groupDistanceTolerance =
            std::max(kPlaneTolerance * 5.0, 1.0e-3);
        for (const ThicknessCandidate& candidate : candidates)
        {
            ThicknessPlaneGroup* matchingGroup = nullptr;
            for (ThicknessPlaneGroup& group : groups)
            {
                if (std::fabs(group.signedPlaneDistance -
                              candidate.signedPlaneDistance) <=
                    groupDistanceTolerance)
                {
                    matchingGroup = &group;
                    break;
                }
            }
            if (matchingGroup == nullptr)
            {
                ThicknessPlaneGroup group;
                group.signedPlaneDistance =
                    candidate.signedPlaneDistance;
                group.planeDistance = candidate.planeDistance;
                groups.push_back(group);
                matchingGroup = &groups.back();
            }
            matchingGroup->candidates.push_back(candidate);
            matchingGroup->planeDistance =
                std::min(matchingGroup->planeDistance,
                         candidate.planeDistance);
            matchingGroup->totalArea +=
                MeasureFaceArea(candidate.face);
        }

        std::sort(groups.begin(), groups.end(),
                  [](const ThicknessPlaneGroup& first,
                     const ThicknessPlaneGroup& second) {
                      return first.planeDistance <
                             second.planeDistance;
                  });
        trace << "\n  oppositeParallelFaces=" << candidates.size()
              << ", planeGroups=" << groups.size();

        for (const ThicknessPlaneGroup& group : groups)
        {
            const double groupToBaseAreaRatio =
                group.totalArea / baseArea;
            if (groupToBaseAreaRatio + 1.0e-9 <
                kThicknessMinimumAreaRatio)
            {
                trace << "\n  skip group distance="
                      << group.planeDistance
                      << " signedDistance="
                      << group.signedPlaneDistance
                      << " faceCount="
                      << group.candidates.size()
                      << " totalArea=" << group.totalArea
                      << " groupToBaseAreaRatio="
                      << groupToBaseAreaRatio
                      << " reason=combined parallel-face area is below 60 percent of selected-face area";
                continue;
            }
            trace << "\n  group distance=" << group.planeDistance
                  << " signedDistance=" << group.signedPlaneDistance
                  << " faceCount=" << group.candidates.size()
                  << " totalArea=" << group.totalArea
                  << " groupToBaseAreaRatio=" << groupToBaseAreaRatio
                  << " selectionRule=nearest qualifying parallel-face group";
            bestDistance = group.planeDistance;
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

bool TwoPointSiBianUI::CreateFeatureTemplateFeature(
    const InferredInputs& inputs,
    std::string& errorMessage,
    tag_t* createdTemplateGroupTag,
    std::vector<tag_t>* createdToolBodyTags) const
{
    errorMessage.clear();
    if (createdTemplateGroupTag != nullptr)
    {
        *createdTemplateGroupTag = NULL_TAG;
    }
    if (createdToolBodyTags != nullptr)
    {
        createdToolBodyTags->clear();
    }

    Part* workPart = session_ != nullptr ? session_->Parts()->Work() : nullptr;
    if (workPart == nullptr || inputs.targetBody == nullptr ||
        inputs.baseFace == nullptr || inputs.startPositiveYEdge == nullptr ||
        inputs.endPositiveYEdge == nullptr || inputs.startNegativeYEdge == nullptr)
    {
        errorMessage = "The feature-template inputs are incomplete.";
        return false;
    }

    const tag_t workPartTag = workPart->Tag();
    const std::vector<tag_t> featureTagsBeforeVector =
        CurrentWorkPartFeatureTags();
    const std::set<tag_t> featureTagsBefore(featureTagsBeforeVector.begin(),
                                            featureTagsBeforeVector.end());
    std::set<tag_t> bodyTagsBefore;
    for (Body* body : *workPart->Bodies())
    {
        if (body != nullptr)
        {
            bodyTagsBefore.insert(body->Tag());
        }
    }

    ExtractedTemplatePart extractedTemplate;
    std::string templatePath;
    std::string extractTrace = "embedded feature-template extraction:\n";
    if (!extractedTemplate.Extract(inputs.featureMode,
                                   inputs.useNinetyClearanceGrooveTemplate,
                                   inputs.useNinetyClearanceGrooveRightTemplate,
                                   templatePath,
                                   extractTrace,
                                   true))
    {
        errorMessage = "Failed to extract the embedded NX feature template.";
        AppendDebugLog(extractTrace + errorMessage);
        return false;
    }
    AppendDebugLog(extractTrace);

    const Session::UndoMarkId attemptMark =
        session_->SetUndoMark(Session::MarkVisibilityInvisible,
                              "2P_SiBian Feature Template Attempt");
    UserDefinedTemplate::InstantiationBuilder* builder = nullptr;
    Part* authoringPart = nullptr;
    auto destroyBuilder = [&]()
    {
        if (builder != nullptr)
        {
            try
            {
                builder->Destroy();
            }
            catch (...)
            {
            }
            builder = nullptr;
        }
        // NX destroys/unloads the authoring part together with the
        // InstantiationBuilder.  Touching the Part wrapper afterwards is a
        // use-after-close and can raise an NX memory access violation.
        authoringPart = nullptr;
    };
    auto rollbackAttempt = [&]() -> bool
    {
        destroyBuilder();
        try
        {
            if (session_->DoesUndoMarkExist(attemptMark,
                                            "2P_SiBian Feature Template Attempt"))
            {
                session_->UndoToMark(attemptMark,
                                     "2P_SiBian Feature Template Attempt");
                if (session_->DoesUndoMarkExist(attemptMark,
                                                "2P_SiBian Feature Template Attempt"))
                {
                    session_->DeleteUndoMark(attemptMark,
                                             "2P_SiBian Feature Template Attempt");
                }
            }
        }
        catch (...)
        {
        }

        auto collectRemainingFeatures = [&]()
        {
            std::vector<Features::Feature*> remaining;
            for (auto iterator = workPart->Features()->begin();
                 iterator != workPart->Features()->end();
                 ++iterator)
            {
                Features::Feature* feature = *iterator;
                if (feature != nullptr &&
                    featureTagsBefore.find(feature->Tag()) == featureTagsBefore.end())
                {
                    remaining.push_back(feature);
                }
            }
            return remaining;
        };

        std::vector<Features::Feature*> remaining = collectRemainingFeatures();
        if (!remaining.empty())
        {
            try
            {
                std::vector<TaggedObject*> objectsToDelete(
                    remaining.begin(), remaining.end());
                Update* updateManager = session_->UpdateManager();
                const Session::UndoMarkId cleanupMark =
                    session_->SetUndoMark(Session::MarkVisibilityInvisible,
                                          "2P_SiBian Feature Template Cleanup");
                updateManager->ClearDeleteList();
                updateManager->ClearErrorList();
                const int addErrors =
                    updateManager->AddObjectsToDeleteList(objectsToDelete);
                const int updateErrors = updateManager->DoUpdate(cleanupMark);
                if (session_->DoesUndoMarkExist(
                        cleanupMark,
                        "2P_SiBian Feature Template Cleanup"))
                {
                    session_->DeleteUndoMark(
                        cleanupMark,
                        "2P_SiBian Feature Template Cleanup");
                }
                AppendDebugLog("feature-template fallback cleanup: objects=" +
                               std::to_string(objectsToDelete.size()) +
                               ", addErrors=" + std::to_string(addErrors) +
                               ", updateErrors=" + std::to_string(updateErrors));
            }
            catch (const NXException& ex)
            {
                AppendDebugLog("feature-template fallback cleanup failed: " +
                               NxExceptionText(ex));
            }
            catch (...)
            {
                AppendDebugLog("feature-template fallback cleanup failed with an unknown error.");
            }
            remaining = collectRemainingFeatures();
        }
        return remaining.empty();
    };

    try
    {
        builder = workPart->UserDefinedTemplates()->CreateInstantiationBuilder(nullptr);
        authoringPart = builder->LoadAuthoringPart(templatePath.c_str());
        if (authoringPart == nullptr)
        {
            throw std::runtime_error("NX did not load the feature-template authoring part.");
        }
        builder->SetLayerOption(
            UserDefinedTemplate::InstantiationBuilder::
                JaUserdefinedtemplateinstantiationLayerOptionWork);

        std::ostringstream mappingTrace;
        mappingTrace << "==== 2P_SiBian feature-template insert ====\n"
                     << "template=" << templatePath;
        for (Expression* originalExpression : builder->GetExpressions())
        {
            if (originalExpression == nullptr)
            {
                continue;
            }
            bool canBeEdited = false;
            Expression* matchedExpression =
                builder->GetMatchedExpression(originalExpression, &canBeEdited);
            if (matchedExpression == nullptr)
            {
                throw std::runtime_error("A feature-template expression could not be matched.");
            }

            const char* nameText = originalExpression->Name().GetLocaleText();
            const std::string expressionName =
                ToLowerAscii(nameText != nullptr ? nameText : "");
            std::string value;
            if (expressionName == "p16" || expressionName == "p42" ||
                expressionName == "p33")
            {
                value = FormatExpressionNumber(inputs.thickness);
            }
            else if (expressionName == "p24" || expressionName == "p43" ||
                     expressionName == "p34")
            {
                value = inputs.bendRadiusValue;
            }
            else if (expressionName == "p17" || expressionName == "p45" ||
                     expressionName == "p36" || expressionName == "p52" ||
                     expressionName == "p61")
            {
                value = inputs.clearanceValue;
            }
            else
            {
                continue;
            }
            matchedExpression->SetRightHandSide(value.c_str());
            mappingTrace << "\n  expression " << expressionName
                         << "=" << value
                         << ", editable=" << (canBeEdited ? "true" : "false");
        }

        const std::array<Edge*, 3> edgeMatches = {
            inputs.startPositiveYEdge,
            inputs.endPositiveYEdge,
            inputs.startNegativeYEdge};
        const std::array<bool, 3> edgeFlips = {
            EdgeDirectionWithStartPoint(inputs.startPositiveYEdge,
                                        inputs.startPoint,
                                        "FT A(P1,Y+) start=P1") == UF_MODL_UDF_REVERSE_DIR,
            EdgeDirectionWithStartPoint(inputs.endPositiveYEdge,
                                        inputs.endPoint,
                                        "FT a(P2,Y+) start=P2") == UF_MODL_UDF_REVERSE_DIR,
            EdgeDirectionWithEndPoint(inputs.startNegativeYEdge,
                                      inputs.startPoint,
                                      "FT B(P1,Y-) end=P1") == UF_MODL_UDF_REVERSE_DIR};
        int edgeOrdinal = 0;
        for (NXObject* originalReference : builder->GetReferences())
        {
            if (dynamic_cast<Face*>(originalReference) != nullptr)
            {
                builder->SetMatchedReference(originalReference,
                                             inputs.baseFace,
                                             false);
                mappingTrace << "\n  face -> " << inputs.baseFace->Tag();
            }
            else if (dynamic_cast<Edge*>(originalReference) != nullptr)
            {
                if (edgeOrdinal >= static_cast<int>(edgeMatches.size()) ||
                    edgeMatches[edgeOrdinal] == nullptr)
                {
                    throw std::runtime_error(
                        "The feature template requested an unexpected edge reference.");
                }
                builder->SetMatchedReference(originalReference,
                                             edgeMatches[edgeOrdinal],
                                             edgeFlips[edgeOrdinal]);
                mappingTrace << "\n  edge[" << edgeOrdinal << "] -> "
                             << edgeMatches[edgeOrdinal]->Tag()
                             << ", flip="
                             << (edgeFlips[edgeOrdinal] ? "true" : "false");
                ++edgeOrdinal;
            }
            else
            {
                bool automaticFlip = false;
                NXObject* automaticMatch =
                    builder->GetMatchedReference(originalReference, &automaticFlip);
                mappingTrace << "\n  internal reference -> "
                             << (automaticMatch != nullptr
                                     ? std::to_string(automaticMatch->Tag())
                                     : std::string("automatic"));
            }
        }
        if (edgeOrdinal != static_cast<int>(edgeMatches.size()))
        {
            throw std::runtime_error(
                "The feature template did not expose exactly three edge references.");
        }
        AppendDebugLog(mappingTrace.str());

        NXObject* committed = builder->Commit();
        AppendDebugLog("feature-template builder committed object=" +
                       std::to_string(committed != nullptr ? committed->Tag() : NULL_TAG));
        Features::Feature* templateGroup = nullptr;
        if (UserDefinedTemplate::Instantiation* instantiation =
                dynamic_cast<UserDefinedTemplate::Instantiation*>(committed))
        {
#pragma warning(push)
#pragma warning(disable : 4996)
            templateGroup = instantiation->GetFeature();
#pragma warning(pop)
            AppendDebugLog("feature-template group resolved from committed instantiation: group=" +
                           std::to_string(templateGroup != nullptr
                                              ? templateGroup->Tag()
                                              : NULL_TAG));
        }
        // Instantiation is owned by the builder. Resolve the persistent
        // work-part feature before Destroy(), which invalidates that wrapper.
        destroyBuilder();
        for (auto iterator = workPart->Features()->begin();
             iterator != workPart->Features()->end();
             ++iterator)
        {
            Features::Feature* feature = *iterator;
            if (feature == nullptr ||
                featureTagsBefore.find(feature->Tag()) != featureTagsBefore.end())
            {
                continue;
            }
            const char* featureTypeText = feature->FeatureType().GetLocaleText();
            const std::string featureType =
                ToLowerAscii(featureTypeText != nullptr ? featureTypeText : "");
            if (templateGroup == nullptr &&
                featureType.find("udt feature group") != std::string::npos)
            {
                templateGroup = feature;
            }
        }
        if (templateGroup == nullptr)
        {
            throw std::runtime_error(
                "NX created the template objects but no UDT feature group was found.");
        }

        std::vector<tag_t> createdBodies;
        for (Body* body : *workPart->Bodies())
        {
            if (body != nullptr && body != inputs.targetBody &&
                bodyTagsBefore.find(body->Tag()) == bodyTagsBefore.end())
            {
                createdBodies.push_back(body->Tag());
            }
        }
        if (createdBodies.empty())
        {
            throw std::runtime_error(
                "The feature template was created, but its independent tool body was not found.");
        }

        templateGroup->SetName("2P_SiBian");
        if (createdTemplateGroupTag != nullptr)
        {
            *createdTemplateGroupTag = templateGroup->Tag();
        }
        if (createdToolBodyTags != nullptr)
        {
            *createdToolBodyTags = createdBodies;
        }
        if (session_->DoesUndoMarkExist(attemptMark,
                                        "2P_SiBian Feature Template Attempt"))
        {
            session_->DeleteUndoMark(attemptMark,
                                     "2P_SiBian Feature Template Attempt");
        }
        AppendDebugLog("feature-template instantiation OK: group=" +
                       std::to_string(templateGroup->Tag()) +
                       ", toolBodyCount=" + std::to_string(createdBodies.size()));
        return true;
    }
    catch (const NXException& ex)
    {
        errorMessage = "NX feature-template instantiation failed: " +
                       NxExceptionText(ex);
    }
    catch (const std::exception& ex)
    {
        errorMessage = std::string("NX feature-template instantiation failed: ") +
                       ex.what();
    }
    catch (...)
    {
        errorMessage = "NX feature-template instantiation failed with an unknown error.";
    }

    AppendDebugLog(errorMessage + "; rolling back and using the legacy UDF fallback.");
    if (!rollbackAttempt())
    {
        errorMessage =
            "FT_ROLLBACK_FAILED: " + errorMessage +
            " The partial feature-template objects could not be removed safely.";
        AppendDebugLog(errorMessage);
    }
    return false;
}

bool TwoPointSiBianUI::CreateUserDefinedFeature(const InferredInputs& sourceInputs,
                                                std::string& errorMessage,
                                                tag_t* createdUdfTag,
                                                std::vector<tag_t>* createdReferenceTags,
                                                std::vector<tag_t>* createdToolBodyTags) const
{
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

    // CustomFeature editing rolls the model back to the state immediately
    // before the opaque node. Edge objects saved on the node still carry the
    // old post-feature topology and can become tag 0 as soon as the preview
    // starts modifying the body. Never feed those cached wrappers to a UDF.
    // Re-resolve all four signed straight edges from the stable face and P1/P2
    // immediately before every UDF insertion.
    InferredInputs inputs = sourceInputs;
    inputs.startEdge = nullptr;
    inputs.endEdge = nullptr;
    inputs.startPositiveYEdge = nullptr;
    inputs.startNegativeYEdge = nullptr;
    inputs.endPositiveYEdge = nullptr;
    inputs.endNegativeYEdge = nullptr;
    if (!CompleteInputsForEndpoints(inputs))
    {
        errorMessage =
            "The current straight-edge references could not be resolved from the saved P1/P2 and face.";
        AppendDebugLog(
            "CreateUserDefinedFeature rejected stale cached edges: body=" +
            std::to_string(inputs.targetBody != nullptr
                               ? inputs.targetBody->Tag()
                               : NULL_TAG) +
            ", face=" +
            std::to_string(inputs.baseFace != nullptr
                               ? inputs.baseFace->Tag()
                               : NULL_TAG) +
            ", P1=" + FormatPoint(inputs.startPoint) +
            ", P2=" + FormatPoint(inputs.endPoint));
        return false;
    }
    AppendDebugLog(
        "CreateUserDefinedFeature refreshed live straight-edge references: "
        "P1Y+=" +
        std::to_string(inputs.startPositiveYEdge->Tag()) +
        ", P1Y-=" +
        std::to_string(inputs.startNegativeYEdge->Tag()) +
        ", P2Y+=" +
        std::to_string(inputs.endPositiveYEdge->Tag()) +
        ", P2Y-=" +
        std::to_string(inputs.endNegativeYEdge->Tag()));

    // The individual tool-generating UDFs remain stable construction members.
    // The CustomFeature pre-update event owns the complete completed branch,
    // so nested Feature Template instances are neither required nor used.
    AppendDebugLog(
        "CreateUserDefinedFeature using a UDF construction member owned by the opaque CustomFeature node.");

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
    if (!extractedTemplate.Extract(inputs.featureMode,
                                   inputs.useNinetyClearanceGrooveTemplate,
                                   inputs.useNinetyClearanceGrooveRightTemplate,
                                   openedTemplatePath,
                                   extractTrace))
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
        const UdfTemplateSpec currentTemplateSpec =
            TemplateSpecForMode(inputs.featureMode,
                                inputs.useNinetyClearanceGrooveTemplate,
                                inputs.useNinetyClearanceGrooveRightTemplate);
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

    if ((inputs.featureMode == FeatureMode::Chamfer ||
         inputs.featureMode == FeatureMode::NinetyLeft ||
         inputs.featureMode == FeatureMode::NinetyRight) &&
        refData.num_refs != 4)
    {
        UF_MODL_udf_free_exp_data(&expData);
        UF_MODL_udf_free_ref_data(&refData);
        closeTemplate();
        errorMessage = "The chamfer/90-left UDF template must have exactly 4 references in order: A edge, face, a edge, B edge. Actual reference count: " +
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

        if ((inputs.featureMode == FeatureMode::Chamfer ||
             inputs.featureMode == FeatureMode::NinetyLeft ||
             inputs.featureMode == FeatureMode::NinetyRight) &&
            index >= 0 && index <= 3)
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

    try
    {
        SetConstructionRole(
            dynamic_cast<Features::Feature*>(NXObjectManager::Get(newUdf)),
            zhihui_twopoint_sibian::kRoleUdf);
    }
    catch (...)
    {
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
        // A UDF may return several disconnected tool bodies.  Creating one
        // subtract feature per body makes the feature chain fragile: after an
        // edit, one small tool can move completely outside the target and its
        // standalone subtract then fails even though the other tools still
        // produce the intended cut.  Submit all compatible tools in one
        // Boolean feature.  Solid and sheet bodies must remain in separate
        // groups because NX does not allow them in the same Boolean feature.
        std::vector<Body*> solidTools;
        std::vector<Body*> sheetTools;
        for (tag_t toolTag : toolBodyTags)
        {
            Body* toolBody = dynamic_cast<Body*>(NXObjectManager::Get(toolTag));
            if (toolBody == nullptr || toolBody == targetBody)
            {
                errorMessage = "A deferred UDF tool body is no longer available for subtraction.";
                return false;
            }
            (toolBody->IsSolidBody() ? solidTools : sheetTools).push_back(toolBody);
        }

        const std::array<std::vector<Body*>*, 2> groups = {&solidTools, &sheetTools};
        for (const std::vector<Body*>* group : groups)
        {
            if (group == nullptr || group->empty())
            {
                continue;
            }

            Features::BooleanBuilder* builder =
                workPart->Features()->CreateBooleanBuilderUsingCollector(nullptr);
            builder->SetOperation(Features::Feature::BooleanTypeSubtract);
            builder->SetRetainTarget(false);
            builder->SetRetainTool(false);

            SelectionIntentRuleOptions* targetOptions =
                workPart->ScRuleFactory()->CreateRuleOptions();
            targetOptions->SetSelectedFromInactive(false);
            BodyDumbRule* targetRule = workPart->ScRuleFactory()->CreateRuleBodyDumb(
                std::vector<Body*>{targetBody}, true, targetOptions);
            delete targetOptions;
            ScCollector* targetCollector = workPart->ScCollectors()->CreateCollector();
            targetCollector->ReplaceRules(
                std::vector<SelectionIntentRule*>{targetRule}, false);
            builder->SetTargetBodyCollector(targetCollector);

            SelectionIntentRuleOptions* toolOptions =
                workPart->ScRuleFactory()->CreateRuleOptions();
            toolOptions->SetSelectedFromInactive(false);
            BodyDumbRule* toolRule = workPart->ScRuleFactory()->CreateRuleBodyDumb(
                *group, true, toolOptions);
            delete toolOptions;
            ScCollector* toolCollector = workPart->ScCollectors()->CreateCollector();
            toolCollector->ReplaceRules(
                std::vector<SelectionIntentRule*>{toolRule}, false);
            builder->SetToolBodyCollector(toolCollector);

            NXObject* result = builder->Commit();
            resultFeatureTag = result != nullptr ? result->Tag() : NULL_TAG;
            builder->Destroy();
            if (resultFeatureTag == NULL_TAG)
            {
                errorMessage = "NX returned no feature from the grouped final Boolean Subtract.";
                return false;
            }

            std::string tags;
            for (Body* toolBody : *group)
            {
                if (!tags.empty())
                {
                    tags += ",";
                }
                tags += std::to_string(toolBody->Tag());
            }
            AppendDebugLog("grouped final deferred subtraction: target=" +
                           std::to_string(targetBody->Tag()) +
                           ", toolType=" +
                           std::string(group == &solidTools ? "solid" : "sheet") +
                           ", tools=[" + tags +
                           "], resultFeature=" + std::to_string(resultFeatureTag));
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

void TwoPointSiBianUI::ConfigureInputMode(bool smartMode, bool clearSelections)
{
    AppendDebugLog(std::string("ConfigureInputMode begin mode=") +
                   (smartMode ? "smart" : "manual"));

    if (editedFeature_ != nullptr)
    {
        // Match the proven editable-CustomFeature pattern used by feature 18:
        // editing changes parameters only; selection and topology controls are
        // not available while NX owns the existing construction chain.
        if (smartModeBlock_ != nullptr)
        {
            smartModeBlock_->SetShow(false);
            smartModeBlock_->SetEnable(false);
        }
        if (featureModeBlock_ != nullptr)
        {
            featureModeBlock_->SetShow(false);
            featureModeBlock_->SetEnable(false);
        }
        if (startPointBlock_ != nullptr)
        {
            startPointBlock_->SetShow(false);
            startPointBlock_->SetEnable(false);
        }
        if (endPointBlock_ != nullptr)
        {
            endPointBlock_->SetShow(false);
            endPointBlock_->SetEnable(false);
        }
        if (chamferEdgeToggleBlock_ != nullptr)
        {
            chamferEdgeToggleBlock_->SetShow(false);
            chamferEdgeToggleBlock_->SetEnable(false);
        }
        if (reverseCutButton_ != nullptr)
        {
            reverseCutButton_->SetShow(false);
            reverseCutButton_->SetEnable(false);
        }
        if (clearanceBlock_ != nullptr)
        {
            clearanceBlock_->SetShow(true);
            clearanceBlock_->SetEnable(true);
        }
        if (bendRadiusBlock_ != nullptr)
        {
            bendRadiusBlock_->SetShow(true);
            bendRadiusBlock_->SetEnable(true);
        }
        activeSmartSelectionBlock_ = nullptr;
        AppendDebugLog(
            "ConfigureInputMode edit mode: only clearance and bend-R controls are visible and editable.");
        return;
    }

    const std::vector<TaggedObject*> noSelection;
    if (clearSelections)
    {
        try
        {
            if (startPointBlock_ != nullptr)
            {
                startPointBlock_->SetSelectedObjects(noSelection);
            }
            if (endPointBlock_ != nullptr)
            {
                endPointBlock_->SetSelectedObjects(noSelection);
            }
        }
        catch (...)
        {
        }
    }

    if (featureModeBlock_ != nullptr)
    {
        featureModeBlock_->SetShow(!smartMode);
        featureModeBlock_->SetEnable(!smartMode);
    }
    if (startPointBlock_ != nullptr)
    {
        startPointBlock_->SetShow(true);
        startPointBlock_->SetEnable(true);
        startPointBlock_->SetAutomaticProgression(!smartMode);
        startPointBlock_->SetLabelString(smartMode ? "选择面上点" : "第一条边端点");
        std::vector<Selection::MaskTriple> masks;
        if (smartMode)
        {
            masks.emplace_back(UF_solid_type,
                               UF_solid_face_subtype,
                               UF_UI_SEL_FEATURE_ANY_FACE);
        }
        else
        {
            masks.emplace_back(UF_point_type, UF_point_subtype, 0);
            masks.emplace_back(UF_solid_type,
                               UF_solid_body_subtype,
                               UF_UI_SEL_FEATURE_ANY_EDGE);
        }
        startPointBlock_->SetSelectionFilter(
            Selection::SelectionActionClearAndEnableSpecific,
            masks);
    }
    if (endPointBlock_ != nullptr)
    {
        endPointBlock_->SetShow(!smartMode);
        endPointBlock_->SetEnable(!smartMode);
        endPointBlock_->SetAutomaticProgression(false);
        endPointBlock_->SetLabelString("第二条边端点");
        if (!smartMode)
        {
            std::vector<Selection::MaskTriple> masks;
            masks.emplace_back(UF_point_type, UF_point_subtype, 0);
            masks.emplace_back(UF_solid_type,
                               UF_solid_body_subtype,
                               UF_UI_SEL_FEATURE_ANY_EDGE);
            endPointBlock_->SetSelectionFilter(
                Selection::SelectionActionClearAndEnableSpecific,
                masks);
        }
    }
    activeSmartSelectionBlock_ = startPointBlock_;
    AppendDebugLog(std::string("ConfigureInputMode mode=") +
                   (smartMode
                        ? "smart; one face-point control visible, enum disabled."
                        : "manual; two edge-endpoint controls visible, face inference disabled."));
}

void TwoPointSiBianUI::UnhighlightSelectionObjects() const
{
    const std::array<BlockStyler::SelectObject*, 2> blocks = {
        startPointBlock_, endPointBlock_};
    std::set<tag_t> clearedTags;
    for (BlockStyler::SelectObject* block : blocks)
    {
        if (block == nullptr)
        {
            continue;
        }
        try
        {
            for (TaggedObject* selectedObject : block->GetSelectedObjects())
            {
                if (selectedObject == nullptr || selectedObject->Tag() == NULL_TAG ||
                    !clearedTags.insert(selectedObject->Tag()).second)
                {
                    continue;
                }
                if (DisplayableObject* displayable = dynamic_cast<DisplayableObject*>(selectedObject))
                {
                    displayable->Unhighlight();
                }
                UF_DISP_set_highlight(selectedObject->Tag(), 0);
            }
        }
        catch (...)
        {
            // A previous preview face can already be invalid at this point.
        }
    }
}

void TwoPointSiBianUI::ShowError(const std::string& message) const
{
    AppendDebugLog("ShowError: " + message);
    ui_->NXMessageBox()->Show(zhihui_twopoint_sibian::kFeatureDisplayName,
                              NXMessageBox::DialogTypeError,
                              message.c_str());
}

