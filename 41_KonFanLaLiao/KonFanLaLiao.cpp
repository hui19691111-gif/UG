#include "KonFanLaLiao.hpp"
#include "KonFanLaLiaoCustomFeatureShared.hpp"

#include "../../common/ZhihuiDialogMemory.hpp"

#include <NXOpen/BlockStyler_PropertyList.hxx>
#include <NXOpen/Body.hxx>
#include <NXOpen/BodyCollection.hxx>
#include <NXOpen/DisplayManager.hxx>
#include <NXOpen/DisplayModification.hxx>
#include <NXOpen/DisplayableObject.hxx>
#include <NXOpen/Edge.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/FaceDumbRule.hxx>
#include <NXOpen/CurveDumbRule.hxx>
#include <NXOpen/IBaseCurve.hxx>
#include <NXOpen/SelectionIntentRule.hxx>
#include <NXOpen/SelectionIntentRuleOptions.hxx>
#include <NXOpen/ScCollector.hxx>
#include <NXOpen/ScCollectorCollection.hxx>
#include <NXOpen/ScRuleFactory.hxx>
#include <NXOpen/Section.hxx>
#include <NXOpen/SectionCollection.hxx>
#include <NXOpen/Features_Feature.hxx>
#include <NXOpen/Features_FeatureCollection.hxx>
#include <NXOpen/Features_CustomAttribute.hxx>
#include <NXOpen/Features_CustomAttributeCollection.hxx>
#include <NXOpen/Features_CustomFeature.hxx>
#include <NXOpen/Features_CustomFeatureBuilder.hxx>
#include <NXOpen/Features_CustomFeatureClass.hxx>
#include <NXOpen/Features_CustomFeatureClassManager.hxx>
#include <NXOpen/Features_CustomFeatureData.hxx>
#include <NXOpen/Features_CustomFeatureDataCollection.hxx>
#include <NXOpen/Features_CustomTagArrayAttribute.hxx>
#include <NXOpen/Features_CustomTagAttribute.hxx>
#include <NXOpen/Features_SheetMetal_SheetmetalManager.hxx>
#include <NXOpen/Features_SheetMetal_UnbendBuilder.hxx>
#include <NXOpen/Features_SheetMetal_RebendBuilder.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXMessageBox.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/Selection.hxx>
#include <NXOpen/Update.hxx>

#include <uf.h>
#include <uf_modl.h>
#include <uf_curve.h>
#include <uf_disp.h>
#include <uf_obj.h>
#include <uf_layer.h>
#include <uf_ui_types.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <Shellapi.h>

#ifdef CreateDialog
#undef CreateDialog
#endif

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace
{
constexpr int kBackgroundTranslucency = 85;
constexpr int kRiskFaceColor = 186;
constexpr double kDistanceTolerance = 1.0e-5;

class DisplaySuppressionGuard
{
public:
    DisplaySuppressionGuard()
    {
        int current = UF_DISP_UNSUPPRESS_DISPLAY;
        active_ = UF_DISP_ask_display(&current) == 0 &&
            current != UF_DISP_SUPPRESS_DISPLAY &&
            UF_DISP_set_display(UF_DISP_SUPPRESS_DISPLAY) == 0;
    }

    ~DisplaySuppressionGuard()
    {
        if (active_)
        {
            UF_DISP_set_display(UF_DISP_UNSUPPRESS_DISPLAY);
            UF_DISP_regenerate_display();
        }
    }

private:
    bool active_ = false;
};

struct SafeDistanceConfig
{
    double largeArcRatio = 3.0;
    double roundThicknessLimit = 2.0;
    double roundThinThicknessFactor = 1.0;
    double roundThickThicknessFactor = 1.5;
    double slotLengthLimit1 = 25.0;
    double slotLengthLimit2 = 50.0;
    double slotShortThicknessFactor = 2.0;
    double slotMediumThicknessFactor = 2.5;
    double slotLongThicknessFactor = 3.0;
};

std::string TrimText(const std::string& value)
{
    std::size_t first = 0;
    while (first < value.size() &&
           static_cast<unsigned char>(value[first]) <= ' ')
    {
        ++first;
    }
    std::size_t last = value.size();
    while (last > first &&
           static_cast<unsigned char>(value[last - 1]) <= ' ')
    {
        --last;
    }
    return value.substr(first, last - first);
}

std::string ReadUtf8File(const wchar_t* path)
{
    HANDLE file = CreateFileW(path, GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return std::string();
    }
    LARGE_INTEGER size = {};
    if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0 ||
        size.QuadPart > 1024 * 1024)
    {
        CloseHandle(file);
        return std::string();
    }
    std::string text(static_cast<std::size_t>(size.QuadPart), '\0');
    DWORD read = 0;
    const BOOL ok = ReadFile(file, &text[0], static_cast<DWORD>(text.size()),
                             &read, nullptr);
    CloseHandle(file);
    if (!ok)
    {
        return std::string();
    }
    text.resize(read);
    if (text.size() >= 3 &&
        static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB &&
        static_cast<unsigned char>(text[2]) == 0xBF)
    {
        text.erase(0, 3);
    }
    return text;
}

double ConfigNumber(const std::map<std::string, std::string>& values,
                    const char* key, double fallback)
{
    const auto item = values.find(key);
    if (item == values.end())
    {
        return fallback;
    }
    char* end = nullptr;
    const double parsed = std::strtod(item->second.c_str(), &end);
    return end == item->second.c_str() || !std::isfinite(parsed)
        ? fallback : parsed;
}

SafeDistanceConfig LoadSafeDistanceConfig()
{
    SafeDistanceConfig config;
    const std::string text = ReadUtf8File(
        L"D:\\UG\u667a\u8f89\u94a3\u91d1\u63d2\u4ef6\\config\\KonFanLaLiao_rules.ini");
    std::map<std::string, std::string> values;
    std::stringstream stream(text);
    std::string line;
    while (std::getline(stream, line))
    {
        line = TrimText(line);
        if (line.empty() || line[0] == '#' || line[0] == ';' ||
            line[0] == '[')
        {
            continue;
        }
        const std::size_t equal = line.find('=');
        if (equal != std::string::npos)
        {
            values[TrimText(line.substr(0, equal))] =
                TrimText(line.substr(equal + 1));
        }
    }
    config.largeArcRatio = ConfigNumber(values, u8"大圆弧R厚比分界",
        ConfigNumber(values, "large_arc_r_t_ratio", config.largeArcRatio));
    config.roundThicknessLimit = ConfigNumber(values, u8"圆孔板厚分界",
        ConfigNumber(values, "round_thickness_limit", config.roundThicknessLimit));
    config.roundThinThicknessFactor = ConfigNumber(values, u8"圆孔薄板板厚倍数",
        ConfigNumber(values, "round_thin_t_factor", config.roundThinThicknessFactor));
    config.roundThickThicknessFactor = ConfigNumber(values, u8"圆孔厚板板厚倍数",
        ConfigNumber(values, "round_thick_t_factor", config.roundThickThicknessFactor));
    config.slotLengthLimit1 = ConfigNumber(values, u8"腰型孔长度第一分界",
        ConfigNumber(values, "slot_length_limit_1", config.slotLengthLimit1));
    config.slotLengthLimit2 = ConfigNumber(values, u8"腰型孔长度第二分界",
        ConfigNumber(values, "slot_length_limit_2", config.slotLengthLimit2));
    config.slotShortThicknessFactor = ConfigNumber(values, u8"腰型孔短孔板厚倍数",
        ConfigNumber(values, "slot_short_t_factor", config.slotShortThicknessFactor));
    config.slotMediumThicknessFactor = ConfigNumber(values, u8"腰型孔中孔板厚倍数",
        ConfigNumber(values, "slot_medium_t_factor", config.slotMediumThicknessFactor));
    config.slotLongThicknessFactor = ConfigNumber(values, u8"腰型孔长孔板厚倍数",
        ConfigNumber(values, "slot_long_t_factor", config.slotLongThicknessFactor));
    return config;
}

void AppendAnalysisLog(const std::string& message)
{
    CreateDirectoryW(L"D:\\UG\u667a\u8f89\u94a3\u91d1\u63d2\u4ef6\\logs", nullptr);
    HANDLE file = CreateFileW(
        L"D:\\UG\u667a\u8f89\u94a3\u91d1\u63d2\u4ef6\\logs\\KonFanLaLiao.log",
        FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
        OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return;
    }
    SYSTEMTIME time = {};
    GetLocalTime(&time);
    char prefix[64] = {};
    sprintf_s(prefix, "[%04u-%02u-%02u %02u:%02u:%02u] ",
        time.wYear, time.wMonth, time.wDay,
        time.wHour, time.wMinute, time.wSecond);
    const std::string line = std::string(prefix) + message + "\r\n";
    DWORD written = 0;
    WriteFile(file, line.data(), static_cast<DWORD>(line.size()),
        &written, nullptr);
    CloseHandle(file);
}

std::string ModuleDirectory()
{
    HMODULE module = nullptr;
    if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&ModuleDirectory), &module))
    {
        return std::string();
    }
    wchar_t path[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(module, path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
    {
        return std::string();
    }
    wchar_t* slash = wcsrchr(path, L'\\');
    if (slash != nullptr)
    {
        *(slash + 1) = L'\0';
    }
    return zhihui_dialog_memory::Utf8FromWide(path);
}

bool IsAlive(tag_t tag)
{
    return tag != NULL_TAG && UF_OBJ_ask_status(tag) == UF_OBJ_ALIVE;
}

double Dot(const NXOpen::Vector3d& a, const NXOpen::Vector3d& b)
{
    return a.X * b.X + a.Y * b.Y + a.Z * b.Z;
}

double Magnitude(const NXOpen::Vector3d& value)
{
    return std::sqrt(Dot(value, value));
}

NXOpen::Vector3d Scale(const NXOpen::Vector3d& value, double factor)
{
    return NXOpen::Vector3d(
        value.X * factor, value.Y * factor, value.Z * factor);
}

NXOpen::Vector3d Normalize(const NXOpen::Vector3d& value)
{
    const double length = Magnitude(value);
    if (length <= 1.0e-9)
    {
        throw std::runtime_error("防拉槽方向计算失败。");
    }
    return Scale(value, 1.0 / length);
}

NXOpen::Vector3d Subtract(
    const NXOpen::Point3d& a, const NXOpen::Point3d& b)
{
    return NXOpen::Vector3d(a.X - b.X, a.Y - b.Y, a.Z - b.Z);
}

NXOpen::Point3d Add(
    const NXOpen::Point3d& point, const NXOpen::Vector3d& value)
{
    return NXOpen::Point3d(
        point.X + value.X, point.Y + value.Y, point.Z + value.Z);
}

NXOpen::Vector3d Cross(
    const NXOpen::Vector3d& a, const NXOpen::Vector3d& b)
{
    return NXOpen::Vector3d(
        a.Y * b.Z - a.Z * b.Y,
        a.Z * b.X - a.X * b.Z,
        a.X * b.Y - a.Y * b.X);
}

tag_t CreateLine(
    const NXOpen::Point3d& start, const NXOpen::Point3d& end)
{
    UF_CURVE_line_t data = {};
    data.start_point[0] = start.X;
    data.start_point[1] = start.Y;
    data.start_point[2] = start.Z;
    data.end_point[0] = end.X;
    data.end_point[1] = end.Y;
    data.end_point[2] = end.Z;
    tag_t line = NULL_TAG;
    return UF_CURVE_create_line(&data, &line) == 0 ? line : NULL_TAG;
}

void DeleteObject(tag_t tag)
{
    if (IsAlive(tag))
    {
        UF_OBJ_delete_object(tag);
    }
}

void RepairLegacyTargetBodyCycles()
{
    NXOpen::Session* session = NXOpen::Session::GetSession();
    NXOpen::Part* workPart = session == nullptr
        ? nullptr : session->Parts()->Work();
    if (workPart == nullptr || workPart->Features() == nullptr)
    {
        return;
    }

    int repaired = 0;
    int customFeatureCount = 0;
    const NXOpen::Session::UndoMarkId mark = session->SetUndoMark(
        NXOpen::Session::MarkVisibilityVisible,
        "修复防拉孔循环依赖");
    for (NXOpen::Features::Feature* feature :
         workPart->Features()->GetFeatures())
    {
        NXOpen::Features::CustomFeature* customFeature =
            dynamic_cast<NXOpen::Features::CustomFeature*>(feature);
        if (customFeature == nullptr)
        {
            continue;
        }
        ++customFeatureCount;
        try
        {
            NXOpen::Features::CustomFeatureData* data =
                customFeature->FeatureData();
            const char* className = data == nullptr
                ? nullptr : data->ClassName().GetLocaleText();
            const char* featureName =
                customFeature->Name().GetLocaleText();
            const bool classMatches = className != nullptr &&
                std::string(className).find("KonFanLaLiao") !=
                    std::string::npos;
            const bool nameMatches = featureName != nullptr &&
                std::string(featureName) ==
                    zhihui_konfan_laliao::kFeatureDisplayName;
            const bool hasInternal = data != nullptr &&
                data->HasCustomAttribute(
                    zhihui_konfan_laliao::kAttrInternalFeatures,
                    NXOpen::Features::CustomAttribute::TypeTagVla);
            const bool hasTarget = data != nullptr &&
                data->HasCustomAttribute(
                    zhihui_konfan_laliao::kAttrTargetBody,
                    NXOpen::Features::CustomAttribute::TypeTag);
            NXOpen::Features::CustomTagArrayAttribute* internalAttribute =
                hasInternal
                    ? data->CustomTagArrayAttributeByName(
                          zhihui_konfan_laliao::kAttrInternalFeatures)
                    : nullptr;
            const bool missingOutputProperty = internalAttribute != nullptr &&
                !internalAttribute->HasProperty(
                    NXOpen::Features::CustomAttribute::PropertyIsOutputAttribute);
            const bool missingOwnedProperty = internalAttribute != nullptr &&
                !internalAttribute->HasProperty(
                    NXOpen::Features::CustomAttribute::PropertyIsOwnedAttribute);
            std::ostringstream scanLog;
            scanLog << "LEGACY_CYCLE_SCAN custom_feature="
                << customFeature->Tag()
                << " class=" << (className == nullptr ? "<null>" : className)
                << " name=" << (featureName == nullptr ? "<null>" : featureName)
                << " has_internal=" << (hasInternal ? 1 : 0)
                << " has_target=" << (hasTarget ? 1 : 0)
                << " missing_output=" << (missingOutputProperty ? 1 : 0)
                << " missing_owned=" << (missingOwnedProperty ? 1 : 0);
            AppendAnalysisLog(scanLog.str());
            if (data == nullptr ||
                (!classMatches && !nameMatches && !hasInternal))
            {
                continue;
            }
            bool changed = false;
            if (missingOutputProperty)
            {
                internalAttribute->AddProperty(
                    NXOpen::Features::CustomAttribute::PropertyIsOutputAttribute);
                changed = true;
            }
            if (missingOwnedProperty)
            {
                internalAttribute->AddProperty(
                    NXOpen::Features::CustomAttribute::PropertyIsOwnedAttribute);
                changed = true;
            }
            if (hasTarget)
            {
                NXOpen::Features::CustomTagAttribute* target =
                    data->CustomTagAttributeByName(
                        zhihui_konfan_laliao::kAttrTargetBody);
                target->SetValue(nullptr);
                data->RemoveCustomAttribute(target);
                changed = true;
            }
            if (changed)
            {
                ++repaired;
                std::ostringstream repairLog;
                repairLog << "LEGACY_CYCLE_REPAIR custom_feature="
                    << customFeature->Tag()
                    << " internal_output_owned=1"
                    << " target_body_removed=" << (hasTarget ? 1 : 0);
                AppendAnalysisLog(repairLog.str());
            }
        }
        catch (const NXOpen::NXException& ex)
        {
            AppendAnalysisLog(
                "LEGACY_CYCLE_REPAIR decision=FAILED nx_error=" +
                std::to_string(ex.ErrorCode()));
        }
        catch (...)
        {
            AppendAnalysisLog(
                "LEGACY_CYCLE_REPAIR decision=FAILED exception=UNKNOWN");
        }
    }
    if (repaired > 0)
    {
        try
        {
            const int errors = session->UpdateManager()->DoUpdate(mark);
            AppendAnalysisLog(
                "LEGACY_CYCLE_REPAIR repaired=" +
                std::to_string(repaired) +
                " update_errors=" + std::to_string(errors));
        }
        catch (...)
        {
            AppendAnalysisLog(
                "LEGACY_CYCLE_REPAIR repaired=" +
                std::to_string(repaired) + " update=FAILED");
        }
    }
    else
    {
        AppendAnalysisLog(
            "LEGACY_CYCLE_REPAIR repaired=0 custom_features_scanned=" +
            std::to_string(customFeatureCount));
        session->DeleteUndoMark(mark, nullptr);
    }
}
}

KonFanLaLiaoDialog::KonFanLaLiaoDialog()
    : ui_(NXOpen::UI::GetUI()),
      session_(NXOpen::Session::GetSession()),
      dialog_(ui_->CreateDialog(DialogPath().c_str())),
      bodySelect_(nullptr),
      formulaSummary_(nullptr),
      configButton_(nullptr),
      safetyDistanceMode_(nullptr),
      riskDistance_(nullptr),
      reliefLengthMode_(nullptr),
      reliefLength_(nullptr),
      slotWidth_(nullptr),
      status_(nullptr),
      initialized_(false),
      refreshing_(false),
      slotsCreated_(false)
{
    RepairLegacyTargetBodyCycles();
    dialog_->AddInitializeHandler(
        NXOpen::make_callback(this, &KonFanLaLiaoDialog::initialize_cb));
    dialog_->AddDialogShownHandler(
        NXOpen::make_callback(this, &KonFanLaLiaoDialog::dialogShown_cb));
    dialog_->AddUpdateHandler(
        NXOpen::make_callback(this, &KonFanLaLiaoDialog::update_cb));
    dialog_->AddApplyHandler(
        NXOpen::make_callback(this, &KonFanLaLiaoDialog::apply_cb));
    dialog_->AddOkHandler(
        NXOpen::make_callback(this, &KonFanLaLiaoDialog::ok_cb));
    dialog_->AddCancelHandler(
        NXOpen::make_callback(this, &KonFanLaLiaoDialog::cancel_cb));
}

KonFanLaLiaoDialog::~KonFanLaLiaoDialog()
{
    RestoreDisplay();
    delete dialog_;
}

NXOpen::BlockStyler::BlockDialog::DialogResponse KonFanLaLiaoDialog::Launch()
{
    return dialog_->Launch();
}

std::string KonFanLaLiaoDialog::DialogPath() const
{
    return ModuleDirectory() + "KonFanLaLiao.dlx";
}

void KonFanLaLiaoDialog::initialize_cb()
{
    bodySelect_ = dialog_->TopBlock()->FindBlock("body_select");
    formulaSummary_ = dialog_->TopBlock()->FindBlock("formula_summary");
    configButton_ = dialog_->TopBlock()->FindBlock("config_button");
    safetyDistanceMode_ = dialog_->TopBlock()->FindBlock("safety_distance_mode");
    riskDistance_ = dialog_->TopBlock()->FindBlock("risk_distance");
    reliefLengthMode_ = dialog_->TopBlock()->FindBlock("relief_length_mode");
    reliefLength_ = dialog_->TopBlock()->FindBlock("relief_length");
    slotWidth_ = dialog_->TopBlock()->FindBlock("slot_width");
    status_ = dialog_->TopBlock()->FindBlock("result_status");
    if (bodySelect_ == nullptr || formulaSummary_ == nullptr ||
        configButton_ == nullptr || riskDistance_ == nullptr ||
        safetyDistanceMode_ == nullptr || reliefLengthMode_ == nullptr ||
        reliefLength_ == nullptr || slotWidth_ == nullptr || status_ == nullptr)
    {
        throw std::runtime_error("KonFanLaLiao.dlx 缺少必要控件。");
    }

    NXOpen::BlockStyler::PropertyList* properties = bodySelect_->GetProperties();
    std::vector<NXOpen::Selection::MaskTriple> masks;
    masks.emplace_back(UF_solid_type, UF_solid_body_subtype, 0);
    properties->SetSelectionFilter(
        "SelectionFilter",
        NXOpen::Selection::SelectionActionClearAndEnableSpecific,
        masks);
    properties->SetEnum("StepStatus", 1);
    delete properties;

    LoadState();
    const SafeDistanceConfig config = LoadSafeDistanceConfig();
    std::ostringstream formula;
    formula << "圆孔：t≤" << config.roundThicknessLimit
            << "，S=" << config.roundThinThicknessFactor
            << "t+r；t>"
            << config.roundThicknessLimit << "，S="
            << config.roundThickThicknessFactor << "t+r。长孔：L≤"
            << config.slotLengthLimit1 << "，S="
            << config.slotShortThicknessFactor << "t+r；L≤"
            << config.slotLengthLimit2 << "，S="
            << config.slotMediumThicknessFactor << "t+r；其余 S="
            << config.slotLongThicknessFactor << "t+r。";
    NXOpen::BlockStyler::PropertyList* formulaProperties =
        formulaSummary_->GetProperties();
    formulaProperties->SetString(
        NXOpen::NXString("Label", NXOpen::NXString::UTF8),
        NXOpen::NXString(formula.str(), NXOpen::NXString::UTF8));
    delete formulaProperties;
    {
        NXOpen::BlockStyler::PropertyList* mode =
            safetyDistanceMode_->GetProperties();
        const bool manual = mode->GetEnum("Value") == 1;
        delete mode;
        NXOpen::BlockStyler::PropertyList* input =
            riskDistance_->GetProperties();
        input->SetLogical("Enable", manual);
        delete input;
    }
    {
        NXOpen::BlockStyler::PropertyList* mode =
            reliefLengthMode_->GetProperties();
        const bool manual = mode->GetEnum("Value") == 1;
        delete mode;
        NXOpen::BlockStyler::PropertyList* input =
            reliefLength_->GetProperties();
        input->SetLogical("Enable", manual);
        delete input;
    }
    initialized_ = true;
}

void KonFanLaLiaoDialog::dialogShown_cb()
{
    RunAnalysis(false);
}

int KonFanLaLiaoDialog::update_cb(NXOpen::BlockStyler::UIBlock* block)
{
    if (!initialized_ || refreshing_)
    {
        return 0;
    }
    if (block == configButton_)
    {
        ShellExecuteW(nullptr, L"open",
            L"D:\\UG\u667a\u8f89\u94a3\u91d1\u63d2\u4ef6\\config\\KonFanLaLiao_rules.ini",
            nullptr, nullptr, SW_SHOWNORMAL);
        return 0;
    }
    if (block == safetyDistanceMode_)
    {
        NXOpen::BlockStyler::PropertyList* mode = block->GetProperties();
        const bool manual = mode->GetEnum("Value") == 1;
        delete mode;
        NXOpen::BlockStyler::PropertyList* input =
            riskDistance_->GetProperties();
        input->SetLogical("Enable", manual);
        delete input;
    }
    if (block == reliefLengthMode_)
    {
        NXOpen::BlockStyler::PropertyList* mode = block->GetProperties();
        const bool manual = mode->GetEnum("Value") == 1;
        delete mode;
        NXOpen::BlockStyler::PropertyList* input =
            reliefLength_->GetProperties();
        input->SetLogical("Enable", manual);
        delete input;
    }
    if (block == bodySelect_ || block == safetyDistanceMode_ ||
        block == riskDistance_ || block == reliefLengthMode_ ||
        block == reliefLength_ || block == slotWidth_)
    {
        RunAnalysis(false);
    }
    return 0;
}

int KonFanLaLiaoDialog::apply_cb()
{
    if (!slotsCreated_)
    {
        refreshing_ = true;
        try
        {
            const AnalysisResult result = Analyze();
            RefreshDisplay(result);
            const int created = CreateReliefSlots(result);
            slotsCreated_ = created > 0;
            RestoreDisplay();
            std::ostringstream message;
            message << "已创建防拉槽 " << created << " 个。";
            SetStatus(message.str());
        }
        catch (const NXOpen::NXException& ex)
        {
            RestoreDisplay();
            ShowError(ex.Message() != nullptr ? ex.Message() : "自动开槽失败。");
        }
        catch (const std::exception& ex)
        {
            RestoreDisplay();
            ShowError(ex.what());
        }
        refreshing_ = false;
    }
    SaveState();
    return 0;
}

int KonFanLaLiaoDialog::ok_cb()
{
    return apply_cb();
}

int KonFanLaLiaoDialog::cancel_cb()
{
    RestoreDisplay();
    return 0;
}

double KonFanLaLiaoDialog::DoubleValue(
    NXOpen::BlockStyler::UIBlock* block) const
{
    NXOpen::BlockStyler::PropertyList* properties = block->GetProperties();
    const double value = properties->GetDouble("Value");
    delete properties;
    return value;
}

void KonFanLaLiaoDialog::SetDoubleValue(
    NXOpen::BlockStyler::UIBlock* block, double value) const
{
    NXOpen::BlockStyler::PropertyList* properties = block->GetProperties();
    properties->SetDouble("Value", value);
    delete properties;
}

void KonFanLaLiaoDialog::SetStatus(const std::string& text) const
{
    NXOpen::BlockStyler::PropertyList* properties = status_->GetProperties();
    std::vector<NXOpen::NXString> lines;
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        lines.emplace_back(line, NXOpen::NXString::UTF8);
    }
    if (lines.empty())
    {
        lines.emplace_back("", NXOpen::NXString::UTF8);
    }
    properties->SetStrings("Value", lines);
    delete properties;
}

void KonFanLaLiaoDialog::LoadState() const
{
    {
        NXOpen::BlockStyler::PropertyList* properties =
            safetyDistanceMode_->GetProperties();
        properties->SetEnum("Value", zhihui_dialog_memory::ReadInt(
            L"KonFanLaLiao_state.ini", L"SafetyDistanceMode", 0));
        delete properties;
    }
    SetDoubleValue(
        riskDistance_,
        zhihui_dialog_memory::ReadDouble(
            L"KonFanLaLiao_state.ini", L"ManualSafetyDistance", 5.0));
    {
        NXOpen::BlockStyler::PropertyList* properties =
            reliefLengthMode_->GetProperties();
        properties->SetEnum("Value", zhihui_dialog_memory::ReadInt(
            L"KonFanLaLiao_state.ini", L"ReliefLengthMode", 0));
        delete properties;
    }
    SetDoubleValue(
        reliefLength_,
        zhihui_dialog_memory::ReadDouble(
            L"KonFanLaLiao_state.ini", L"ReliefLength", 8.0));
    SetDoubleValue(
        slotWidth_,
        zhihui_dialog_memory::ReadDouble(
            L"KonFanLaLiao_state.ini", L"SlotWidth", 0.2));
}

void KonFanLaLiaoDialog::SaveState() const
{
    NXOpen::BlockStyler::PropertyList* safetyMode =
        safetyDistanceMode_->GetProperties();
    zhihui_dialog_memory::WriteInt(
        L"KonFanLaLiao_state.ini", L"SafetyDistanceMode",
        safetyMode->GetEnum("Value"));
    delete safetyMode;
    zhihui_dialog_memory::WriteDouble(
        L"KonFanLaLiao_state.ini", L"ManualSafetyDistance",
        DoubleValue(riskDistance_));
    NXOpen::BlockStyler::PropertyList* lengthMode =
        reliefLengthMode_->GetProperties();
    zhihui_dialog_memory::WriteInt(
        L"KonFanLaLiao_state.ini", L"ReliefLengthMode",
        lengthMode->GetEnum("Value"));
    delete lengthMode;
    zhihui_dialog_memory::WriteDouble(
        L"KonFanLaLiao_state.ini", L"ReliefLength",
        DoubleValue(reliefLength_));
    zhihui_dialog_memory::WriteDouble(
        L"KonFanLaLiao_state.ini", L"SlotWidth",
        DoubleValue(slotWidth_));
}

std::vector<NXOpen::Body*> KonFanLaLiaoDialog::TargetBodies() const
{
    std::vector<NXOpen::Body*> result;
    NXOpen::Part* workPart = session_->Parts()->Work();
    NXOpen::Features::SheetMetal::SheetmetalManager* sheetmetalManager =
        workPart == nullptr || workPart->Features() == nullptr
            ? nullptr : workPart->Features()->SheetmetalManager();
    auto isDisplayedSheetmetalBody = [sheetmetalManager](NXOpen::Body* body)
    {
        if (body == nullptr || !body->IsSolidBody() || body->IsBlanked() ||
            sheetmetalManager == nullptr)
        {
            return false;
        }
        const int layer = body->Layer();
        if (layer < 1 || layer > 256)
        {
            return false;
        }
        int layerStatus = UF_LAYER_INACTIVE_LAYER;
        if (UF_LAYER_ask_status(layer, &layerStatus) != 0 ||
            layerStatus == UF_LAYER_INACTIVE_LAYER)
        {
            return false;
        }
        try
        {
            return sheetmetalManager->IsSheetmetalBody(body);
        }
        catch (...)
        {
            return false;
        }
    };
    NXOpen::BlockStyler::PropertyList* properties = bodySelect_->GetProperties();
    const std::vector<NXOpen::TaggedObject*> selected =
        properties->GetTaggedObjectVector("SelectedObjects");
    delete properties;

    std::set<tag_t> seen;
    for (NXOpen::TaggedObject* object : selected)
    {
        NXOpen::Body* body = dynamic_cast<NXOpen::Body*>(object);
        if (isDisplayedSheetmetalBody(body) &&
            seen.insert(body->Tag()).second)
        {
            result.push_back(body);
        }
    }
    if (!result.empty())
    {
        return result;
    }

    if (workPart == nullptr || workPart->Bodies() == nullptr)
    {
        return result;
    }
    for (NXOpen::Body* body : *workPart->Bodies())
    {
        if (isDisplayedSheetmetalBody(body) &&
            seen.insert(body->Tag()).second)
        {
            result.push_back(body);
        }
    }
    return result;
}

bool KonFanLaLiaoDialog::IsBendFace(NXOpen::Face* face) const
{
    if (face == nullptr)
    {
        return false;
    }
    int faceType = 0;
    if (UF_MODL_ask_face_type(face->Tag(), &faceType) != 0 ||
        faceType != UF_MODL_CYLINDRICAL_FACE)
    {
        return false;
    }

    // A sheet-metal bend cylinder has straight generatrix edges. A normal
    // round hole has circular boundary edges and is therefore not a bend.
    int linearEdges = 0;
    for (NXOpen::Edge* edge : face->GetEdges())
    {
        int edgeType = 0;
        if (edge != nullptr &&
            UF_MODL_ask_edge_type(edge->Tag(), &edgeType) == 0 &&
            edgeType == UF_MODL_LINEAR_EDGE)
        {
            ++linearEdges;
        }
    }
    return linearEdges >= 2;
}

bool KonFanLaLiaoDialog::MinimumDistance(
    tag_t first, tag_t second, double& distance,
    NXOpen::Point3d* firstPoint, NXOpen::Point3d* secondPoint) const
{
    double guess1[3] = {0.0, 0.0, 0.0};
    double guess2[3] = {0.0, 0.0, 0.0};
    double point1[3] = {0.0, 0.0, 0.0};
    double point2[3] = {0.0, 0.0, 0.0};
    double accuracy = 0.0;
    distance = DBL_MAX;
    const bool succeeded = UF_MODL_ask_minimum_dist_3(
               2, first, second, 0, guess1, 0, guess2,
               &distance, point1, point2, &accuracy) == 0;
    if (succeeded && firstPoint != nullptr)
    {
        *firstPoint = NXOpen::Point3d(point1[0], point1[1], point1[2]);
    }
    if (succeeded && secondPoint != nullptr)
    {
        *secondPoint = NXOpen::Point3d(point2[0], point2[1], point2[2]);
    }
    return succeeded;
}

KonFanLaLiaoDialog::AnalysisResult KonFanLaLiaoDialog::Analyze() const
{
    AnalysisResult result;
    const SafeDistanceConfig config = LoadSafeDistanceConfig();
    NXOpen::BlockStyler::PropertyList* safetyModeProperties =
        safetyDistanceMode_->GetProperties();
    const bool manualSafetyDistance =
        safetyModeProperties->GetEnum("Value") == 1;
    delete safetyModeProperties;
    const double manualDistance = DoubleValue(riskDistance_);
    if (manualSafetyDistance && manualDistance <= 0.0)
    {
        throw std::runtime_error("手动安全距离必须大于 0。");
    }

    const std::vector<NXOpen::Body*> bodies = TargetBodies();
    NXOpen::Part* workPart = session_->Parts()->Work();
    NXOpen::Features::SheetMetal::SheetmetalManager* sheetmetalManager =
        workPart == nullptr ? nullptr :
        workPart->Features()->SheetmetalManager();
    result.bodyCount = static_cast<int>(bodies.size());
    std::set<tag_t> seenRiskFaces;

    {
        std::ostringstream log;
        log << "RUN safety_mode="
            << (manualSafetyDistance ? "MANUAL" : "FORMULA")
            << " manual_distance=" << manualDistance
            << " body_count=" << bodies.size()
            << " round_t_limit=" << config.roundThicknessLimit
            << " slot_l_limits=" << config.slotLengthLimit1 << ','
            << config.slotLengthLimit2;
        AppendAnalysisLog(log.str());
    }

    for (NXOpen::Body* body : bodies)
    {
        const std::vector<NXOpen::Face*> faces = body->GetFaces();
        double bodyThickness = 0.0;
        if (sheetmetalManager != nullptr)
        {
            try
            {
                bodyThickness = sheetmetalManager->GetBodyThickness(body);
            }
            catch (...)
            {
                bodyThickness = 0.0;
            }
        }
        {
            std::ostringstream log;
            log << "LARGE_ARC_RULE body=" << body->Tag()
                << " ratio=" << config.largeArcRatio
                << " thickness=" << bodyThickness;
            AppendAnalysisLog(log.str());
        }
        std::set<tag_t> bendFaceTags;
        std::vector<NXOpen::Edge*> bendEdges;
        std::set<tag_t> seenBendEdges;
        std::map<tag_t, NXOpen::Face*> bendEdgeOwners;
        std::map<tag_t, double> bendEdgeInnerRadii;

        for (NXOpen::Face* face : faces)
        {
            if (!IsBendFace(face))
            {
                continue;
            }

            // GetBendParameters returns the true inner R for either the inner
            // or outer cylindrical bend face, so both sides use one rule.
            double bendInnerRadius = 0.0;
            if (sheetmetalManager != nullptr && bodyThickness > 1.0e-9)
            {
                try
                {
                    const NXOpen::Features::SheetMetal::SheetmetalBendParameters
                        bendParameters =
                            sheetmetalManager->GetBendParameters(face);
                    const double innerRadius = bendParameters.InnerRadius;
                    bendInnerRadius = innerRadius;
                    if (config.largeArcRatio > 0.0 && innerRadius >=
                        config.largeArcRatio * bodyThickness)
                    {
                        ++result.largeArcExcludedCount;
                        std::ostringstream log;
                        log << "BEND_EXCLUDED_LARGE_ARC body=" << body->Tag()
                            << " face=" << face->Tag()
                            << " inner_radius=" << innerRadius
                            << " thickness=" << bodyThickness
                            << " ratio=" << innerRadius / bodyThickness
                            << " threshold_ratio="
                            << config.largeArcRatio;
                        AppendAnalysisLog(log.str());
                        continue;
                    }
                }
                catch (...)
                {
                    std::ostringstream log;
                    log << "BEND_PARAMETER_UNAVAILABLE body=" << body->Tag()
                        << " face=" << face->Tag()
                        << " decision=KEEP";
                    AppendAnalysisLog(log.str());
                }
            }
            bendFaceTags.insert(face->Tag());
            ++result.bendCount;
            for (NXOpen::Edge* edge : face->GetEdges())
            {
                int edgeType = 0;
                if (edge != nullptr &&
                    UF_MODL_ask_edge_type(edge->Tag(), &edgeType) == 0 &&
                    edgeType == UF_MODL_LINEAR_EDGE &&
                    seenBendEdges.insert(edge->Tag()).second)
                {
                    bendEdges.push_back(edge);
                    bendEdgeOwners[edge->Tag()] = face;
                    bendEdgeInnerRadii[edge->Tag()] = bendInnerRadius;
                }
            }
        }
        result.bendEdgeCount += static_cast<int>(bendEdges.size());

        struct HoleRecord
        {
            struct InnerFaceRecord
            {
                NXOpen::Face* face = nullptr;
                double closest = DBL_MAX;
                tag_t closestBendEdge = NULL_TAG;
                NXOpen::Point3d closestFacePoint;
                NXOpen::Point3d closestBendPoint;
            };
            std::map<tag_t, InnerFaceRecord> innerFaces;
            std::set<tag_t> carrierFaces;
            std::set<tag_t> loopEdgeTags;
            std::map<tag_t, NXOpen::Edge*> connectedBendEdges;
            std::map<tag_t, NXOpen::Face*> bendCarrierFaces;
            bool roundProfile = true;
            bool profileInitialized = false;
            double profileLength = 0.0;
            double closestLoopDistance = DBL_MAX;
            tag_t closestLoopBendEdge = NULL_TAG;
            NXOpen::Point3d closestLoopPoint;
            NXOpen::Point3d closestLoopBendPoint;
        };
        std::map<std::string, HoleRecord> holes;

        // A qualifying inner loop must be on a planar carrier that is directly
        // connected to a bend edge. The inner side faces themselves may be
        // planar, cylindrical, conical or another surface type.
        for (NXOpen::Face* carrierFace : faces)
        {
            if (carrierFace == nullptr ||
                bendFaceTags.count(carrierFace->Tag()) != 0)
            {
                continue;
            }

            int carrierType = 0;
            if (UF_MODL_ask_face_type(
                    carrierFace->Tag(), &carrierType) != 0 ||
                carrierType != UF_MODL_PLANAR_FACE)
            {
                continue;
            }

            std::vector<NXOpen::Edge*> carrierBendEdges;
            const std::vector<NXOpen::Edge*> carrierEdges =
                carrierFace->GetEdges();
            std::set<tag_t> carrierEdgeTags;
            for (NXOpen::Edge* edge : carrierEdges)
            {
                if (edge != nullptr)
                {
                    carrierEdgeTags.insert(edge->Tag());
                }
            }
            for (NXOpen::Edge* bendEdge : bendEdges)
            {
                if (bendEdge != nullptr &&
                    carrierEdgeTags.count(bendEdge->Tag()) != 0)
                {
                    carrierBendEdges.push_back(bendEdge);
                }
            }
            if (carrierBendEdges.empty())
            {
                continue;
            }

            uf_loop_p_t loops = nullptr;
            if (UF_MODL_ask_face_loops(carrierFace->Tag(), &loops) != 0 ||
                loops == nullptr)
            {
                continue;
            }

            int loopCount = 0;
            if (UF_MODL_ask_loop_list_count(loops, &loopCount) == 0)
            {
                for (int loopIndex = 0;
                     loopIndex < loopCount; ++loopIndex)
                {
                    int loopType = 0;
                    uf_list_p_t edgeList = nullptr;
                    if (UF_MODL_ask_loop_list_item(
                            loops, loopIndex, &loopType, &edgeList) != 0 ||
                        loopType != 2 || edgeList == nullptr)
                    {
                        continue;
                    }

                    int edgeCount = 0;
                    if (UF_MODL_ask_list_count(
                            edgeList, &edgeCount) != 0 || edgeCount <= 0)
                    {
                        continue;
                    }

                    std::vector<NXOpen::Edge*> holeEdges;
                    std::vector<NXOpen::Face*> wallFaces;
                    std::set<tag_t> wallFaceTags;
                    std::vector<tag_t> holeEdgeTags;
                    for (int edgeIndex = 0;
                         edgeIndex < edgeCount; ++edgeIndex)
                    {
                        tag_t edgeTag = NULL_TAG;
                        if (UF_MODL_ask_list_item(
                                edgeList, edgeIndex, &edgeTag) != 0 ||
                            edgeTag == NULL_TAG)
                        {
                            continue;
                        }
                        NXOpen::Edge* edge =
                            dynamic_cast<NXOpen::Edge*>(
                                NXOpen::NXObjectManager::Get(edgeTag));
                        if (edge == nullptr)
                        {
                            continue;
                        }
                        holeEdges.push_back(edge);
                        holeEdgeTags.push_back(edgeTag);
                        for (NXOpen::Face* adjacent : edge->GetFaces())
                        {
                            if (adjacent == nullptr ||
                                adjacent->Tag() == carrierFace->Tag() ||
                                !wallFaceTags.insert(
                                    adjacent->Tag()).second)
                            {
                                continue;
                            }
                            wallFaces.push_back(adjacent);
                        }
                    }
                    if (holeEdges.empty())
                    {
                        continue;
                    }

                    int linearProfileEdges = 0;
                    double longestStraightSegment = 0.0;
                    for (NXOpen::Edge* holeEdge : holeEdges)
                    {
                        int edgeType = 0;
                        if (holeEdge == nullptr ||
                            UF_MODL_ask_edge_type(
                                holeEdge->Tag(), &edgeType) != 0 ||
                            edgeType != UF_MODL_LINEAR_EDGE)
                        {
                            continue;
                        }
                        ++linearProfileEdges;
                        double first[3] = {};
                        double second[3] = {};
                        int vertexCount = 0;
                        if (UF_MODL_ask_edge_verts(
                                holeEdge->Tag(), first, second,
                                &vertexCount) == 0 && vertexCount >= 2)
                        {
                            const double dx = second[0] - first[0];
                            const double dy = second[1] - first[1];
                            const double dz = second[2] - first[2];
                            longestStraightSegment = (std::max)(
                                longestStraightSegment,
                                std::sqrt(dx * dx + dy * dy + dz * dz));
                        }
                    }

                    std::ostringstream key;
                    if (!wallFaceTags.empty())
                    {
                        key << "W:";
                        for (tag_t tag : wallFaceTags) key << tag << ',';
                    }
                    else
                    {
                        std::sort(
                            holeEdgeTags.begin(), holeEdgeTags.end());
                        key << "E:";
                        for (tag_t tag : holeEdgeTags) key << tag << ',';
                    }
                    HoleRecord& record = holes[key.str()];
                    if (!record.profileInitialized)
                    {
                        record.roundProfile = linearProfileEdges == 0;
                        record.profileLength = longestStraightSegment;
                        record.profileInitialized = true;
                    }
                    record.carrierFaces.insert(carrierFace->Tag());
                    record.loopEdgeTags.insert(
                        holeEdgeTags.begin(), holeEdgeTags.end());
                    for (NXOpen::Edge* bendEdge : carrierBendEdges)
                    {
                        record.connectedBendEdges[bendEdge->Tag()] = bendEdge;
                        record.bendCarrierFaces[bendEdge->Tag()] = carrierFace;
                    }
                    for (NXOpen::Face* wallFace : wallFaces)
                    {
                        HoleRecord::InnerFaceRecord& inner =
                            record.innerFaces[wallFace->Tag()];
                        inner.face = wallFace;
                    }
                }
            }
            UF_MODL_delete_loop_list(&loops);
        }

        result.holeCount += static_cast<int>(holes.size());
        for (auto& item : holes)
        {
            HoleRecord& record = item.second;

            // Some slot profiles return only the straight wall faces from
            // the planar inner loop. Complete the closed wall chain through
            // face adjacency, while never crossing either carrier face or a
            // recognized bend face. This adds tangent cylindrical end faces.
            std::vector<NXOpen::Face*> pendingFaces;
            for (const auto& innerItem : record.innerFaces)
            {
                if (innerItem.second.face != nullptr)
                {
                    pendingFaces.push_back(innerItem.second.face);
                }
            }
            for (std::size_t pendingIndex = 0;
                 pendingIndex < pendingFaces.size(); ++pendingIndex)
            {
                NXOpen::Face* currentFace = pendingFaces[pendingIndex];
                for (NXOpen::Edge* edge : currentFace->GetEdges())
                {
                    if (edge == nullptr)
                    {
                        continue;
                    }
                    for (NXOpen::Face* adjacent : edge->GetFaces())
                    {
                        if (adjacent == nullptr ||
                            record.carrierFaces.count(adjacent->Tag()) != 0 ||
                            record.innerFaces.count(adjacent->Tag()) != 0)
                        {
                            continue;
                        }
                        HoleRecord::InnerFaceRecord expanded;
                        expanded.face = adjacent;
                        record.innerFaces[adjacent->Tag()] = expanded;
                        pendingFaces.push_back(adjacent);
                    }
                }
            }

            for (tag_t loopEdgeTag : record.loopEdgeTags)
            {
                for (const auto& bendItem : record.connectedBendEdges)
                {
                    double distance = DBL_MAX;
                    NXOpen::Point3d loopPoint;
                    NXOpen::Point3d bendPoint;
                    if (bendItem.second != nullptr &&
                        MinimumDistance(
                            loopEdgeTag, bendItem.first, distance,
                            &loopPoint, &bendPoint) &&
                        distance < record.closestLoopDistance)
                    {
                        record.closestLoopDistance = distance;
                        record.closestLoopBendEdge = bendItem.first;
                        record.closestLoopPoint = loopPoint;
                        record.closestLoopBendPoint = bendPoint;
                    }
                }
            }

            double bendInnerRadius = 0.0;
            const auto radiusItem = bendEdgeInnerRadii.find(
                record.closestLoopBendEdge);
            if (radiusItem != bendEdgeInnerRadii.end())
            {
                bendInnerRadius = radiusItem->second;
            }
            double thicknessFactor = 0.0;
            if (record.roundProfile)
            {
                thicknessFactor = bodyThickness <= config.roundThicknessLimit
                    ? config.roundThinThicknessFactor
                    : config.roundThickThicknessFactor;
            }
            else
            {
                if (record.profileLength <= config.slotLengthLimit1)
                    thicknessFactor = config.slotShortThicknessFactor;
                else if (record.profileLength <= config.slotLengthLimit2)
                    thicknessFactor = config.slotMediumThicknessFactor;
                else
                    thicknessFactor = config.slotLongThicknessFactor;
            }
            const double formulaSafeDistance =
                thicknessFactor * bodyThickness + bendInnerRadius;
            const double requiredSafeDistance = manualSafetyDistance
                ? manualDistance : formulaSafeDistance;
            const bool holeHit =
                record.closestLoopDistance + kDistanceTolerance <
                requiredSafeDistance;
            {
                std::ostringstream log;
                log << "SAFE_DISTANCE body=" << body->Tag()
                    << " hole=" << item.first
                    << " profile="
                    << (record.roundProfile ? "ROUND" : "SLOT")
                    << " profile_length=" << record.profileLength
                    << " thickness=" << bodyThickness
                    << " inner_radius=" << bendInnerRadius
                    << " t_factor=" << thicknessFactor
                    << " formula_distance=" << formulaSafeDistance
                    << " mode="
                    << (manualSafetyDistance ? "MANUAL" : "FORMULA")
                    << " required=" << requiredSafeDistance
                    << " actual=";
                if (record.closestLoopDistance == DBL_MAX)
                    log << "N/A";
                else
                    log << record.closestLoopDistance;
                log << " bend_edge=" << record.closestLoopBendEdge
                    << " decision=" << (holeHit ? "HIT" : "MISS");
                AppendAnalysisLog(log.str());
            }
            for (const auto& faceItem : record.innerFaces)
            {
                const HoleRecord::InnerFaceRecord& inner = faceItem.second;
                int innerFaceType = 0;
                if (inner.face != nullptr)
                {
                    UF_MODL_ask_face_type(
                        inner.face->Tag(), &innerFaceType);
                }
                std::ostringstream log;
                log << "BODY=" << body->Tag()
                    << " HOLE=" << item.first
                    << " INNER_FACE=" << faceItem.first
                    << " face_type=" << innerFaceType
                    << " carrier_faces=";
                for (tag_t tag : record.carrierFaces) log << tag << ',';
                log << " loop_distance=";
                if (record.closestLoopDistance == DBL_MAX) log << "N/A";
                else log << record.closestLoopDistance;
                log << " required=" << requiredSafeDistance
                    << " bend_edge=" << record.closestLoopBendEdge
                    << " decision=" << (holeHit ? "HIT" : "MISS");
                AppendAnalysisLog(log.str());

                if (holeHit && inner.face != nullptr &&
                    seenRiskFaces.insert(faceItem.first).second)
                {
                    result.riskFaces.push_back(inner.face);
                }
            }
            if (holeHit)
            {
                ++result.riskHoleCount;
                const auto carrierItem = record.bendCarrierFaces.find(
                    record.closestLoopBendEdge);
                if (carrierItem != record.bendCarrierFaces.end() &&
                    carrierItem->second != nullptr)
                {
                    int faceType = 0;
                    double planePointData[3] = {};
                    double normalData[3] = {};
                    double box[6] = {};
                    double radius = 0.0;
                    double radiusData = 0.0;
                    int normalDirection = 0;
                    if (UF_MODL_ask_face_data(
                            carrierItem->second->Tag(), &faceType,
                            planePointData, normalData, box, &radius,
                            &radiusData, &normalDirection) == 0 &&
                        faceType == UF_MODL_PLANAR_FACE)
                    {
                        NXOpen::Vector3d normal = Normalize(
                            NXOpen::Vector3d(
                                normalData[0], normalData[1], normalData[2]));
                        const NXOpen::Point3d planePoint(
                            planePointData[0], planePointData[1],
                            planePointData[2]);
                        const double offset = Dot(
                            Subtract(record.closestLoopPoint,
                                     planePoint), normal);
                        AnalysisResult::SlotCandidate candidate;
                        candidate.body = body;
                        candidate.referenceFace = carrierItem->second;
                        const auto bendOwner = bendEdgeOwners.find(
                            record.closestLoopBendEdge);
                        candidate.bendFace = bendOwner == bendEdgeOwners.end()
                            ? nullptr : bendOwner->second;
                        candidate.tangentEdge =
                            record.connectedBendEdges[
                                record.closestLoopBendEdge];
                        candidate.startPoint = Add(
                            record.closestLoopPoint,
                            Scale(normal, -offset));
                        candidate.bendPoint =
                            record.closestLoopBendPoint;
                        double loopMin[3] = {DBL_MAX, DBL_MAX, DBL_MAX};
                        double loopMax[3] = {-DBL_MAX, -DBL_MAX, -DBL_MAX};
                        bool hasLoopBox = false;
                        for (tag_t edgeTag : record.loopEdgeTags)
                        {
                            double edgeBox[6] = {};
                            if (UF_MODL_ask_bounding_box(
                                    edgeTag, edgeBox) != 0)
                            {
                                continue;
                            }
                            for (int coordinate = 0;
                                 coordinate < 3; ++coordinate)
                            {
                                loopMin[coordinate] = (std::min)(
                                    loopMin[coordinate], edgeBox[coordinate]);
                                loopMax[coordinate] = (std::max)(
                                    loopMax[coordinate], edgeBox[coordinate + 3]);
                            }
                            hasLoopBox = true;
                        }
                        candidate.holeCenter = hasLoopBox
                            ? NXOpen::Point3d(
                                  0.5 * (loopMin[0] + loopMax[0]),
                                  0.5 * (loopMin[1] + loopMax[1]),
                                  0.5 * (loopMin[2] + loopMax[2]))
                            : candidate.startPoint;
                        candidate.carrierNormal = normal;

                        double bendStartData[3] = {};
                        double bendEndData[3] = {};
                        int bendVertexCount = 0;
                        tag_t safeOffsetLine = NULL_TAG;
                        std::vector<NXOpen::Point3d> intersections;
                        if (candidate.tangentEdge != nullptr &&
                            UF_MODL_ask_edge_verts(
                                candidate.tangentEdge->Tag(),
                                bendStartData, bendEndData,
                                &bendVertexCount) == 0 &&
                            bendVertexCount >= 2)
                        {
                            const NXOpen::Point3d bendStart(
                                bendStartData[0], bendStartData[1],
                                bendStartData[2]);
                            const NXOpen::Point3d bendEnd(
                                bendEndData[0], bendEndData[1],
                                bendEndData[2]);
                            const NXOpen::Vector3d bendDirection =
                                Normalize(Subtract(bendEnd, bendStart));
                            const NXOpen::Point3d bendMid(
                                0.5 * (bendStart.X + bendEnd.X),
                                0.5 * (bendStart.Y + bendEnd.Y),
                                0.5 * (bendStart.Z + bendEnd.Z));
                            NXOpen::Vector3d offsetDirection =
                                Normalize(Cross(normal, bendDirection));
                            if (Dot(
                                    Subtract(candidate.holeCenter, bendMid),
                                    offsetDirection) < 0.0)
                            {
                                offsetDirection = Scale(offsetDirection, -1.0);
                            }
                            double bodyBox[6] = {};
                            UF_MODL_ask_bounding_box(body->Tag(), bodyBox);
                            const double bodyDiagonal = std::sqrt(
                                (bodyBox[3] - bodyBox[0]) *
                                    (bodyBox[3] - bodyBox[0]) +
                                (bodyBox[4] - bodyBox[1]) *
                                    (bodyBox[4] - bodyBox[1]) +
                                (bodyBox[5] - bodyBox[2]) *
                                    (bodyBox[5] - bodyBox[2]));
                            const double extension =
                                bodyDiagonal + requiredSafeDistance + 10.0;
                            const NXOpen::Point3d offsetMid =
                                Add(bendMid, Scale(
                                    offsetDirection, requiredSafeDistance));
                            safeOffsetLine = CreateLine(
                                Add(offsetMid,
                                    Scale(bendDirection, -extension)),
                                Add(offsetMid,
                                    Scale(bendDirection, extension)));
                            if (safeOffsetLine != NULL_TAG)
                            {
                                for (tag_t loopEdgeTag : record.loopEdgeTags)
                                {
                                    int intersectionCount = 0;
                                    double* intersectionData = nullptr;
                                    if (UF_MODL_intersect_curve_to_curve(
                                            safeOffsetLine, loopEdgeTag,
                                            &intersectionCount,
                                            &intersectionData) != 0 ||
                                        intersectionData == nullptr)
                                    {
                                        continue;
                                    }
                                    for (int intersectionIndex = 0;
                                         intersectionIndex < intersectionCount;
                                         ++intersectionIndex)
                                    {
                                        const NXOpen::Point3d point(
                                            intersectionData[
                                                5 * intersectionIndex],
                                            intersectionData[
                                                5 * intersectionIndex + 1],
                                            intersectionData[
                                                5 * intersectionIndex + 2]);
                                        bool duplicate = false;
                                        for (const NXOpen::Point3d& existing :
                                             intersections)
                                        {
                                            if (Magnitude(Subtract(
                                                    point, existing)) <=
                                                1.0e-4)
                                            {
                                                duplicate = true;
                                                break;
                                            }
                                        }
                                        if (!duplicate)
                                        {
                                            intersections.push_back(point);
                                        }
                                    }
                                    UF_free(intersectionData);
                                }
                            }
                            if (intersections.size() >= 2)
                            {
                                auto projection = [&](const NXOpen::Point3d& p)
                                {
                                    return Dot(Subtract(p, bendMid),
                                               bendDirection);
                                };
                                const auto minimum = std::min_element(
                                    intersections.begin(), intersections.end(),
                                    [&](const NXOpen::Point3d& first,
                                        const NXOpen::Point3d& second)
                                    {
                                        return projection(first) <
                                               projection(second);
                                    });
                                const auto maximum = std::max_element(
                                    intersections.begin(), intersections.end(),
                                    [&](const NXOpen::Point3d& first,
                                        const NXOpen::Point3d& second)
                                    {
                                        return projection(first) <
                                               projection(second);
                                    });
                                candidate.slotPositionPoint = NXOpen::Point3d(
                                    0.5 * (minimum->X + maximum->X),
                                    0.5 * (minimum->Y + maximum->Y),
                                    0.5 * (minimum->Z + maximum->Z));
                                // Automatic slot length is the distance
                                // between the two intersections, extended
                                // by 1 mm at each end.
                                candidate.automaticSlotLength =
                                    Magnitude(Subtract(*maximum, *minimum)) +
                                    2.0;
                                candidate.hasSlotPosition = true;
                            }
                        }
                        DeleteObject(safeOffsetLine);
                        {
                            std::ostringstream positionLog;
                            positionLog << "SLOT_POSITION intersections="
                                << intersections.size()
                                << " safe_distance=" << requiredSafeDistance
                                << " valid="
                                << (candidate.hasSlotPosition ? 1 : 0);
                            if (candidate.hasSlotPosition)
                            {
                                positionLog << " midpoint="
                                    << candidate.slotPositionPoint.X << ','
                                    << candidate.slotPositionPoint.Y << ','
                                    << candidate.slotPositionPoint.Z
                                    << " automatic_length="
                                    << candidate.automaticSlotLength;
                            }
                            AppendAnalysisLog(positionLog.str());
                        }
                        if (candidate.hasSlotPosition)
                        {
                            result.slotCandidates.push_back(candidate);
                        }
                    }
                }
            }
        }
    }
    {
        std::ostringstream log;
        log << "DONE bodies=" << result.bodyCount
            << " bend_faces=" << result.bendCount
            << " large_arc_faces_excluded="
            << result.largeArcExcludedCount
            << " holes=" << result.holeCount
            << " risk_holes=" << result.riskHoleCount
            << " highlighted_faces=" << result.riskFaces.size();
        AppendAnalysisLog(log.str());
    }
    return result;
}

int KonFanLaLiaoDialog::CreateReliefSlots(const AnalysisResult& result)
{
    if ((GetModuleHandleW(L"KonFanLaLiaoCore.dll") != nullptr ||
         GetModuleHandleW(L"KonFanLaLiaoCoreFix.dll") != nullptr ||
         GetModuleHandleW(L"KonFanLaLiaoCoreFix2.dll") != nullptr) &&
        GetModuleHandleW(L"KonFanLaLiaoCoreFix3.dll") == nullptr)
    {
        throw std::runtime_error(
            "NX 当前仍加载旧版防拉孔特征核心。请先撤销本次操作并重启 NX，防止最终实体被隐藏。");
    }
    DisplaySuppressionGuard displayGuard;
    NXOpen::BlockStyler::PropertyList* modeProperties =
        reliefLengthMode_->GetProperties();
    const bool manualLength = modeProperties->GetEnum("Value") == 1;
    delete modeProperties;
    const double enteredLength = DoubleValue(reliefLength_);
    const double width = DoubleValue(slotWidth_);
    if ((manualLength && enteredLength <= 0.0) || width <= 0.0)
    {
        throw std::runtime_error("防拉槽长度和槽宽必须大于 0。");
    }

    std::map<tag_t,
        std::vector<std::pair<const AnalysisResult::SlotCandidate*, double>>>
        candidatesByBody;
    for (const AnalysisResult::SlotCandidate& candidate :
         result.slotCandidates)
    {
        const double length = manualLength
            ? enteredLength : candidate.automaticSlotLength;
        if (length <= 0.0)
        {
            continue;
        }
        if (candidate.body != nullptr)
        {
            candidatesByBody[candidate.body->Tag()].push_back(
                std::make_pair(&candidate, length));
        }
    }
    int created = 0;
    for (const auto& bodyGroup : candidatesByBody)
    {
        std::vector<const AnalysisResult::SlotCandidate*> candidates;
        std::vector<double> lengths;
        for (const auto& entry : bodyGroup.second)
        {
            candidates.push_back(entry.first);
            lengths.push_back(entry.second);
        }
        created += CreateReliefSlotsForBody(candidates, lengths, width);
    }
    std::ostringstream log;
    log << "SLOT_CREATE requested=" << result.slotCandidates.size()
        << " created=" << created
        << " body_groups=" << candidatesByBody.size()
        << " length_mode=" << (manualLength ? "MANUAL" : "AUTO")
        << " entered_length=" << enteredLength
        << " width=" << width;
    AppendAnalysisLog(log.str());
    return created;
}

int KonFanLaLiaoDialog::CreateReliefSlotsForBody(
    const std::vector<const AnalysisResult::SlotCandidate*>& candidates,
    const std::vector<double>& lengths, double width)
{
    if (candidates.empty() || candidates.size() != lengths.size() ||
        candidates.front() == nullptr || candidates.front()->body == nullptr)
    {
        return 0;
    }

    struct FlatSlotWork
    {
        const AnalysisResult::SlotCandidate* candidate = nullptr;
        double length = 0.0;
        double axialOffset = 0.0;
        double edgeDirectionSign = 1.0;
        NXOpen::Edge* firstBoundary = nullptr;
        NXOpen::Edge* secondBoundary = nullptr;
    };

    NXOpen::Body* body = candidates.front()->body;
    const NXOpen::Session::UndoMarkId bodyMark = session_->SetUndoMark(
        NXOpen::Session::MarkVisibilityVisible, "孔防拉槽（按体）");
    bool unbendCommitted = false;
    int created = 0;
    const char* stage = "准备同体防拉槽";
    try
    {
        NXOpen::Part* workPart = session_->Parts()->Work();
        NXOpen::Features::SheetMetal::SheetmetalManager* manager =
            workPart == nullptr || workPart->Features() == nullptr
                ? nullptr : workPart->Features()->SheetmetalManager();
        if (manager == nullptr)
        {
            throw std::runtime_error("当前工作部件不是可编辑的 NX 钣金件。");
        }

        auto bodyFeatureTags = [](NXOpen::Body* targetBody)
        {
            std::vector<tag_t> tags;
            uf_list_p_t featureList = nullptr;
            if (targetBody == nullptr ||
                UF_MODL_ask_body_feats(targetBody->Tag(), &featureList) != 0 ||
                featureList == nullptr)
            {
                return tags;
            }
            int count = 0;
            if (UF_MODL_ask_list_count(featureList, &count) == 0)
            {
                for (int index = 0; index < count; ++index)
                {
                    tag_t feature = NULL_TAG;
                    if (UF_MODL_ask_list_item(
                            featureList, index, &feature) == 0 &&
                        feature != NULL_TAG)
                    {
                        tags.push_back(feature);
                    }
                }
            }
            UF_MODL_delete_list(&featureList);
            return tags;
        };
        const std::vector<tag_t> featuresBefore = bodyFeatureTags(body);
        const std::set<tag_t> featureTagsBefore(
            featuresBefore.begin(), featuresBefore.end());

        struct CurrentFeatureGuard
        {
            NXOpen::Features::Feature* original = nullptr;
            ~CurrentFeatureGuard()
            {
                if (original != nullptr && IsAlive(original->Tag()))
                {
                    try
                    {
                        original->MakeCurrentFeature();
                    }
                    catch (...)
                    {
                    }
                }
            }
        } currentFeatureGuard;

        // Insert after the latest feature that actually owns the selected
        // sheet-metal body.  This is derived only from the body history; no
        // downstream feature is inspected or moved.
        NXOpen::Features::Feature* bodyInsertionFeature = nullptr;
        for (tag_t featureTag : featuresBefore)
        {
            NXOpen::Features::Feature* feature = dynamic_cast<
                NXOpen::Features::Feature*>(
                    NXOpen::NXObjectManager::Get(featureTag));
            if (feature == nullptr || feature->IsInternal())
            {
                continue;
            }
            bool ownsTargetBody = false;
            try
            {
                for (NXOpen::Body* featureBody : feature->GetBodies())
                {
                    if (featureBody != nullptr &&
                        featureBody->Tag() == body->Tag())
                    {
                        ownsTargetBody = true;
                        break;
                    }
                }
            }
            catch (...)
            {
                ownsTargetBody = false;
            }
            if (ownsTargetBody &&
                (bodyInsertionFeature == nullptr ||
                 feature->Timestamp() > bodyInsertionFeature->Timestamp()))
            {
                bodyInsertionFeature = feature;
            }
        }
        if (bodyInsertionFeature == nullptr)
        {
            throw std::runtime_error(
                "无法确定钣金体最后一个建模特征，已停止创建防拉孔。 ");
        }
        currentFeatureGuard.original = workPart->CurrentFeature();
        if (currentFeatureGuard.original == nullptr ||
            currentFeatureGuard.original->Tag() !=
                bodyInsertionFeature->Tag())
        {
            bodyInsertionFeature->MakeCurrentFeature();
        }
        std::ostringstream insertionLog;
        insertionLog << "BODY_INSERTION_FEATURE body=" << body->Tag()
            << " feature=" << bodyInsertionFeature->Tag()
            << " timestamp=" << bodyInsertionFeature->Timestamp()
            << " original_current="
            << (currentFeatureGuard.original == nullptr
                    ? 0 : currentFeatureGuard.original->Tag())
            << " source=TARGET_BODY_HISTORY";
        AppendAnalysisLog(insertionLog.str());

        auto edgeMidPoint = [](NXOpen::Edge* edge) -> NXOpen::Point3d
        {
            double first[3] = {};
            double second[3] = {};
            int vertexCount = 0;
            if (edge == nullptr || UF_MODL_ask_edge_verts(
                    edge->Tag(), first, second, &vertexCount) != 0 ||
                vertexCount < 2)
            {
                throw std::runtime_error("读取折弯边端点失败。");
            }
            return NXOpen::Point3d(
                0.5 * (first[0] + second[0]),
                0.5 * (first[1] + second[1]),
                0.5 * (first[2] + second[2]));
        };
        auto edgeDirection = [](NXOpen::Edge* edge) -> NXOpen::Vector3d
        {
            double first[3] = {};
            double second[3] = {};
            int vertexCount = 0;
            if (edge == nullptr || UF_MODL_ask_edge_verts(
                    edge->Tag(), first, second, &vertexCount) != 0 ||
                vertexCount < 2)
            {
                throw std::runtime_error("读取折弯边方向失败。");
            }
            return Normalize(NXOpen::Vector3d(
                second[0] - first[0], second[1] - first[1],
                second[2] - first[2]));
        };

        std::vector<FlatSlotWork> workItems;
        std::vector<NXOpen::Face*> allPairedBendFaces;
        std::set<tag_t> pairedFaceTags;
        NXOpen::Face* referenceFace = nullptr;

        for (std::size_t index = 0; index < candidates.size(); ++index)
        {
            const AnalysisResult::SlotCandidate* candidate = candidates[index];
            if (candidate == nullptr || candidate->body != body ||
                candidate->bendFace == nullptr ||
                candidate->referenceFace == nullptr ||
                candidate->tangentEdge == nullptr ||
                !candidate->hasSlotPosition || lengths[index] <= 0.0 ||
                !IsAlive(candidate->bendFace->Tag()) ||
                !IsAlive(candidate->referenceFace->Tag()))
            {
                continue;
            }
            if (referenceFace == nullptr)
            {
                referenceFace = candidate->referenceFace;
            }

            int cylinderType = 0;
            double axisPointData[3] = {};
            double axisData[3] = {};
            double cylinderBox[6] = {};
            double radius = 0.0;
            double radiusData = 0.0;
            int normalDirection = 0;
            if (UF_MODL_ask_face_data(
                    candidate->bendFace->Tag(), &cylinderType,
                    axisPointData, axisData, cylinderBox, &radius,
                    &radiusData, &normalDirection) != 0 ||
                cylinderType != UF_MODL_CYLINDRICAL_FACE || radius <= 0.0)
            {
                continue;
            }
            const NXOpen::Point3d axisPoint(
                axisPointData[0], axisPointData[1], axisPointData[2]);
            const NXOpen::Vector3d axis = Normalize(NXOpen::Vector3d(
                axisData[0], axisData[1], axisData[2]));
            auto radialAt = [&](const NXOpen::Point3d& point)
            {
                const NXOpen::Vector3d delta = Subtract(point, axisPoint);
                return Normalize(NXOpen::Vector3d(
                    delta.X - axis.X * Dot(delta, axis),
                    delta.Y - axis.Y * Dot(delta, axis),
                    delta.Z - axis.Z * Dot(delta, axis)));
            };

            const NXOpen::Vector3d firstRadial =
                radialAt(edgeMidPoint(candidate->tangentEdge));
            NXOpen::Edge* otherTangentEdge = nullptr;
            double smallestDot = DBL_MAX;
            for (NXOpen::Edge* edge : candidate->bendFace->GetEdges())
            {
                int edgeType = 0;
                if (edge == nullptr ||
                    edge->Tag() == candidate->tangentEdge->Tag() ||
                    UF_MODL_ask_edge_type(edge->Tag(), &edgeType) != 0 ||
                    edgeType != UF_MODL_LINEAR_EDGE)
                {
                    continue;
                }
                const double radialDot =
                    Dot(firstRadial, radialAt(edgeMidPoint(edge)));
                if (radialDot < smallestDot)
                {
                    smallestDot = radialDot;
                    otherTangentEdge = edge;
                }
            }
            if (otherTangentEdge == nullptr)
            {
                continue;
            }

            NXOpen::Face* oppositeBendFace = nullptr;
            try
            {
                oppositeBendFace = manager->GetOppositeFace(candidate->bendFace);
                if (oppositeBendFace != nullptr &&
                    (!IsAlive(oppositeBendFace->Tag()) ||
                     !IsBendFace(oppositeBendFace)))
                {
                    oppositeBendFace = nullptr;
                }
            }
            catch (...)
            {
                oppositeBendFace = nullptr;
            }
            if (oppositeBendFace == nullptr)
            {
                double bestRadiusDifference = DBL_MAX;
                for (NXOpen::Face* possible : body->GetFaces())
                {
                    if (possible == nullptr ||
                        possible->Tag() == candidate->bendFace->Tag() ||
                        !IsBendFace(possible))
                    {
                        continue;
                    }
                    int possibleType = 0;
                    double possibleAxisPoint[3] = {};
                    double possibleAxisData[3] = {};
                    double possibleBox[6] = {};
                    double possibleRadius = 0.0;
                    double possibleRadiusData = 0.0;
                    int possibleNormal = 0;
                    if (UF_MODL_ask_face_data(
                            possible->Tag(), &possibleType,
                            possibleAxisPoint, possibleAxisData, possibleBox,
                            &possibleRadius, &possibleRadiusData,
                            &possibleNormal) != 0 ||
                        possibleType != UF_MODL_CYLINDRICAL_FACE ||
                        possibleRadius <= 0.0)
                    {
                        continue;
                    }
                    const NXOpen::Vector3d possibleAxis = Normalize(
                        NXOpen::Vector3d(possibleAxisData[0],
                            possibleAxisData[1], possibleAxisData[2]));
                    const NXOpen::Vector3d axisOffset = Subtract(
                        NXOpen::Point3d(possibleAxisPoint[0],
                            possibleAxisPoint[1], possibleAxisPoint[2]),
                        axisPoint);
                    const NXOpen::Vector3d perpendicularOffset(
                        axisOffset.X - axis.X * Dot(axisOffset, axis),
                        axisOffset.Y - axis.Y * Dot(axisOffset, axis),
                        axisOffset.Z - axis.Z * Dot(axisOffset, axis));
                    const double radiusDifference =
                        std::fabs(possibleRadius - radius);
                    if (std::fabs(Dot(axis, possibleAxis)) >= 0.9999 &&
                        Magnitude(perpendicularOffset) <= 1.0e-3 &&
                        radiusDifference > 1.0e-5 &&
                        radiusDifference < bestRadiusDifference)
                    {
                        bestRadiusDifference = radiusDifference;
                        oppositeBendFace = possible;
                    }
                }
            }
            if (oppositeBendFace == nullptr)
            {
                continue;
            }

            for (NXOpen::Face* face :
                 std::vector<NXOpen::Face*>{candidate->bendFace,
                                            oppositeBendFace})
            {
                if (pairedFaceTags.insert(face->Tag()).second)
                {
                    allPairedBendFaces.push_back(face);
                }
            }
            FlatSlotWork item;
            item.candidate = candidate;
            item.length = lengths[index];
            item.firstBoundary = candidate->tangentEdge;
            item.secondBoundary = otherTangentEdge;
            const NXOpen::Point3d originalFirstMid =
                edgeMidPoint(item.firstBoundary);
            const NXOpen::Point3d originalSecondMid =
                edgeMidPoint(item.secondBoundary);
            const NXOpen::Point3d originalBoundaryCenter(
                0.5 * (originalFirstMid.X + originalSecondMid.X),
                0.5 * (originalFirstMid.Y + originalSecondMid.Y),
                0.5 * (originalFirstMid.Z + originalSecondMid.Z));
            NXOpen::Vector3d originalEdgeDirection =
                edgeDirection(item.firstBoundary);
            if (Dot(originalEdgeDirection, axis) < 0.0)
            {
                item.edgeDirectionSign = -1.0;
                originalEdgeDirection = Scale(originalEdgeDirection, -1.0);
            }
            item.axialOffset = Dot(
                Subtract(candidate->slotPositionPoint,
                         originalBoundaryCenter),
                originalEdgeDirection);
            workItems.push_back(item);
        }

        if (workItems.empty() || referenceFace == nullptr ||
            allPairedBendFaces.size() < 2)
        {
            throw std::runtime_error("同一钣金体没有可展开的有效防拉槽对象。");
        }

        stage = "一次伸直同体全部折弯";
        NXOpen::Features::SheetMetal::UnbendBuilder* unbendBuilder =
            manager->CreateUnbendFeatureBuilder(nullptr);
        unbendBuilder->SetReferenceEntity(referenceFace);
        NXOpen::FaceDumbRule* bendRule =
            workPart->ScRuleFactory()->CreateRuleFaceDumb(allPairedBendFaces);
        NXOpen::ScCollector* bendCollector =
            workPart->ScCollectors()->CreateCollector();
        if (bendCollector == nullptr)
        {
            unbendBuilder->Destroy();
            throw std::runtime_error("创建伸直折弯面收集器失败。");
        }
        bendCollector->ReplaceRules(
            std::vector<NXOpen::SelectionIntentRule*>(1, bendRule), false);
        unbendBuilder->SetFaceCollector(bendCollector);
        NXOpen::Features::Feature* unbendFeature =
            unbendBuilder->CommitFeature();
        unbendBuilder->Destroy();
        if (unbendFeature == nullptr)
        {
            throw std::runtime_error("同一钣金体一次伸直失败。");
        }
        unbendCommitted = true;
        AppendAnalysisLog(
            "BODY_UNBEND_ONCE body=" + std::to_string(body->Tag()) +
            " slots=" + std::to_string(workItems.size()) +
            " bend_faces=" + std::to_string(allPairedBendFaces.size()));

        stage = "在伸直状态创建同体全部槽";
        for (const FlatSlotWork& item : workItems)
        {
            if (!IsAlive(item.firstBoundary->Tag()) ||
                !IsAlive(item.secondBoundary->Tag()))
            {
                AppendAnalysisLog(
                    "BODY_SLOT_SKIP reason=REMEMBERED_EDGE_LOST body=" +
                    std::to_string(body->Tag()));
                continue;
            }
            const NXOpen::Point3d firstFlatMid =
                edgeMidPoint(item.firstBoundary);
            const NXOpen::Point3d secondFlatMid =
                edgeMidPoint(item.secondBoundary);
            const NXOpen::Point3d boundaryCenter(
                0.5 * (firstFlatMid.X + secondFlatMid.X),
                0.5 * (firstFlatMid.Y + secondFlatMid.Y),
                0.5 * (firstFlatMid.Z + secondFlatMid.Z));
            const NXOpen::Vector3d flatBendDirection = Scale(
                edgeDirection(item.firstBoundary), item.edgeDirectionSign);
            const NXOpen::Vector3d flatWidthDirection = Normalize(
                Subtract(secondFlatMid, firstFlatMid));
            const NXOpen::Point3d flatCenter = Add(
                boundaryCenter, Scale(flatBendDirection, item.axialOffset));
            const NXOpen::Vector3d flatNormal = Normalize(
                Cross(flatBendDirection, flatWidthDirection));
            const bool slotCreated = CreateThroughSlot(
                body, flatCenter, flatBendDirection,
                flatWidthDirection, flatNormal, item.length, width);
            std::ostringstream slotLog;
            slotLog << "BODY_SLOT_RESULT body=" << body->Tag()
                << " bend_face=" << item.candidate->bendFace->Tag()
                << " center=" << flatCenter.X << ',' << flatCenter.Y << ','
                << flatCenter.Z << " axial_offset=" << item.axialOffset
                << " created=" << (slotCreated ? 1 : 0);
            AppendAnalysisLog(slotLog.str());
            if (slotCreated)
            {
                ++created;
            }
        }

        stage = "一次重新折弯同体全部折弯";
        std::vector<NXOpen::Face*> rebendFaces;
        for (NXOpen::Face* face : allPairedBendFaces)
        {
            if (face != nullptr && IsAlive(face->Tag()))
            {
                rebendFaces.push_back(face);
            }
        }
        if (rebendFaces.size() != allPairedBendFaces.size())
        {
            throw std::runtime_error("伸直后未保留全部内R、外R折弯面。");
        }
        NXOpen::Features::SheetMetal::RebendBuilder* rebendBuilder =
            manager->CreateRebendFeatureBuilder(nullptr);
        NXOpen::FaceDumbRule* rebendRule =
            workPart->ScRuleFactory()->CreateRuleFaceDumb(rebendFaces);
        NXOpen::ScCollector* rebendCollector =
            workPart->ScCollectors()->CreateCollector();
        if (rebendCollector == nullptr)
        {
            rebendBuilder->Destroy();
            throw std::runtime_error("创建重新折弯面收集器失败。");
        }
        rebendCollector->ReplaceRules(
            std::vector<NXOpen::SelectionIntentRule*>(1, rebendRule), false);
        rebendBuilder->SetFaceCollector(rebendCollector);
        NXOpen::Features::Feature* rebendFeature =
            rebendBuilder->CommitFeature();
        rebendBuilder->Destroy();
        if (rebendFeature == nullptr)
        {
            throw std::runtime_error("同一钣金体一次重新折弯失败。");
        }

        // Complete the body-modifying chain before it is wrapped by the
        // custom feature. This is a normal model update only: downstream
        // features are not searched, referenced, selected or reordered.
        stage = "更新钣金体切槽结果";
        const int bodyUpdateErrors =
            session_->UpdateManager()->DoUpdate(bodyMark);
        AppendAnalysisLog(
            "BODY_MODEL_UPDATE body=" + std::to_string(body->Tag()) +
            " errors=" + std::to_string(bodyUpdateErrors) +
            " downstream_handling=NX_AUTOMATIC");
        if (bodyUpdateErrors != 0)
        {
            throw std::runtime_error(
                "钣金体切槽后更新失败，已停止封装防拉孔特征。");
        }

        stage = "创建同体可编辑自定义特征";
        const std::vector<tag_t> featuresAfter = bodyFeatureTags(body);
        std::vector<tag_t> createdFeatures;
        for (tag_t feature : featuresAfter)
        {
            if (featureTagsBefore.find(feature) == featureTagsBefore.end())
            {
                createdFeatures.push_back(feature);
            }
        }
        // NX does not guarantee the order returned by ask_body_feats.  Keep
        // this body's explicitly committed Rebend as its own final visible
        // member so custom features for other bodies cannot affect it.
        const tag_t rebendTag = rebendFeature->Tag();
        createdFeatures.erase(
            std::remove(createdFeatures.begin(), createdFeatures.end(),
                        rebendTag),
            createdFeatures.end());
        createdFeatures.push_back(rebendTag);
        if (!createdFeatures.empty())
        {
            NXOpen::Features::CustomFeatureClassManager* classManager =
                session_->CustomFeatureClassManager();
            NXOpen::Features::CustomFeatureClass* featureClass =
                classManager->GetClassFromName(
                    zhihui_konfan_laliao::kFeatureClassName);
            if (featureClass == nullptr)
            {
                throw std::runtime_error(
                    "“防拉孔开槽”自定义特征未注册，请重启 NX。");
            }
            NXOpen::Features::CustomAttributeCollection* attributes =
                workPart->Features()->CustomAttributeCollection();
            std::vector<NXOpen::Features::CustomAttribute*> values;
            const std::vector<NXOpen::Features::CustomAttribute::Property>
                internalProperties{
                    NXOpen::Features::CustomAttribute::PropertyIsOutputAttribute,
                    NXOpen::Features::CustomAttribute::PropertyIsOwnedAttribute};
            NXOpen::Features::CustomTagArrayAttribute* internalAttribute =
                attributes->CreateCustomTagArrayAttribute(
                    zhihui_konfan_laliao::kAttrInternalFeatures,
                    internalProperties);
            values.push_back(internalAttribute);
            NXOpen::Features::CustomFeatureData* featureData =
                workPart->Features()->CustomFeatureDataCollection()->CreateData(
                    featureClass, values);
            std::vector<NXOpen::TaggedObject*> internalObjects;
            for (tag_t featureTag : createdFeatures)
            {
                NXOpen::TaggedObject* object =
                    NXOpen::NXObjectManager::Get(featureTag);
                if (dynamic_cast<NXOpen::Features::Feature*>(object) != nullptr)
                {
                    internalObjects.push_back(object);
                }
            }
            if (internalObjects.empty())
            {
                throw std::runtime_error("无法收集防拉孔开槽内部特征。");
            }
            internalAttribute->SetValues(internalObjects);
            NXOpen::Features::CustomFeatureBuilder* customBuilder =
                workPart->Features()->CreateCustomFeatureBuilder(nullptr);
            customBuilder->SetFeatureData(featureData);
            NXOpen::Features::Feature* customFeature =
                customBuilder->CommitFeature();
            customBuilder->Destroy();
            if (customFeature == nullptr)
            {
                throw std::runtime_error("创建“防拉孔开槽”自定义特征失败。");
            }
            customFeature->SetName(
                zhihui_konfan_laliao::kFeatureDisplayName);

            std::ostringstream groupLog;
            groupLog << "BODY_CUSTOM_FEATURE body=" << body->Tag()
                << " members=" << createdFeatures.size()
                << " internal_objects=" << internalObjects.size()
                << " custom_feature=" << customFeature->Tag()
                << " name=防拉孔开槽";
            AppendAnalysisLog(groupLog.str());
        }
        AppendAnalysisLog(
            "BODY_REBEND_ONCE body=" + std::to_string(body->Tag()) +
            " created=" + std::to_string(created) +
            " bend_faces=" + std::to_string(rebendFaces.size()));
        return created;
    }
    catch (const NXOpen::NXException& ex)
    {
        std::ostringstream log;
        log << "BODY_SLOT_FAIL stage=" << stage
            << " body=" << body->Tag()
            << " nx_error=" << ex.ErrorCode()
            << " message=" << (ex.Message() != nullptr ? ex.Message() : "")
            << " preserve_model=1"
            << " unbend_committed=" << (unbendCommitted ? 1 : 0)
            << " created=" << created;
        AppendAnalysisLog(log.str());
        return created;
    }
    catch (const std::exception& ex)
    {
        AppendAnalysisLog(
            std::string("BODY_SLOT_FAIL stage=") + stage +
            " body=" + std::to_string(body->Tag()) +
            " message=" + ex.what() + " preserve_model=1" +
            " unbend_committed=" + (unbendCommitted ? "1" : "0") +
            " created=" + std::to_string(created));
        return created;
    }
}

bool KonFanLaLiaoDialog::CreateReliefSlot(
    const AnalysisResult::SlotCandidate& candidate,
    double length, double width)
{
    if (candidate.body == nullptr || candidate.bendFace == nullptr ||
        candidate.referenceFace == nullptr || candidate.tangentEdge == nullptr ||
        !candidate.hasSlotPosition ||
        !IsAlive(candidate.body->Tag()) ||
        !IsAlive(candidate.bendFace->Tag()) ||
        !IsAlive(candidate.referenceFace->Tag()))
    {
        return false;
    }

    session_->SetUndoMark(
        NXOpen::Session::MarkVisibilityVisible, "孔防拉槽");
    bool unbendCommitted = false;
    bool cutCreated = false;
    const char* stage = "读取折弯几何";
    try
    {
        stage = "读取折弯圆柱面参数";
        int cylinderType = 0;
        double axisPointData[3] = {};
        double axisData[3] = {};
        double cylinderBox[6] = {};
        double radius = 0.0;
        double radiusData = 0.0;
        int normalDirection = 0;
        if (UF_MODL_ask_face_data(
                candidate.bendFace->Tag(), &cylinderType,
                axisPointData, axisData, cylinderBox, &radius,
                &radiusData, &normalDirection) != 0 ||
            cylinderType != UF_MODL_CYLINDRICAL_FACE || radius <= 0.0)
        {
            throw std::runtime_error("读取折弯圆柱面失败。");
        }
        const NXOpen::Point3d axisPoint(
            axisPointData[0], axisPointData[1], axisPointData[2]);
        const NXOpen::Vector3d axis = Normalize(NXOpen::Vector3d(
            axisData[0], axisData[1], axisData[2]));

        auto edgeMidPoint = [](NXOpen::Edge* edge) -> NXOpen::Point3d
        {
            double first[3] = {};
            double second[3] = {};
            int vertexCount = 0;
            if (edge == nullptr || UF_MODL_ask_edge_verts(
                    edge->Tag(), first, second, &vertexCount) != 0 ||
                vertexCount < 2)
            {
                throw std::runtime_error("读取折弯边端点失败。");
            }
            return NXOpen::Point3d(
                0.5 * (first[0] + second[0]),
                0.5 * (first[1] + second[1]),
                0.5 * (first[2] + second[2]));
        };
        auto radialAt = [&](const NXOpen::Point3d& point)
        {
            const NXOpen::Vector3d delta = Subtract(point, axisPoint);
            return Normalize(NXOpen::Vector3d(
                delta.X - axis.X * Dot(delta, axis),
                delta.Y - axis.Y * Dot(delta, axis),
                delta.Z - axis.Z * Dot(delta, axis)));
        };

        stage = "读取风险侧折弯切线边";
        const NXOpen::Point3d tangentEdgeMidPoint =
            edgeMidPoint(candidate.tangentEdge);
        const NXOpen::Vector3d firstRadial =
            radialAt(tangentEdgeMidPoint);
        stage = "查找另一侧折弯切线边";
        bool foundOther = false;
        NXOpen::Edge* otherTangentEdge = nullptr;
        double smallestDot = DBL_MAX;
        for (NXOpen::Edge* edge : candidate.bendFace->GetEdges())
        {
            int edgeType = 0;
            if (edge == nullptr || edge->Tag() == candidate.tangentEdge->Tag() ||
                UF_MODL_ask_edge_type(edge->Tag(), &edgeType) != 0 ||
                edgeType != UF_MODL_LINEAR_EDGE)
            {
                continue;
            }
            const NXOpen::Vector3d radial = radialAt(edgeMidPoint(edge));
            const double radialDot = Dot(firstRadial, radial);
            if (radialDot < smallestDot)
            {
                smallestDot = radialDot;
                otherTangentEdge = edge;
                foundOther = true;
            }
        }
        if (!foundOther)
        {
            throw std::runtime_error("未找到折弯面的另一条切线边。");
        }
        stage = "创建展开构建器";
        NXOpen::Part* workPart = session_->Parts()->Work();
        NXOpen::Features::SheetMetal::SheetmetalManager* manager =
            workPart == nullptr || workPart->Features() == nullptr
                ? nullptr : workPart->Features()->SheetmetalManager();
        if (manager == nullptr)
        {
            throw std::runtime_error("当前工作部件不是可编辑的 NX 钣金件。");
        }

        NXOpen::Features::SheetMetal::UnbendBuilder* unbendBuilder =
            manager->CreateUnbendFeatureBuilder(nullptr);
        stage = "设置展开固定面";
        unbendBuilder->SetReferenceEntity(candidate.referenceFace);
        stage = "查找内R和外R折弯面";
        std::vector<NXOpen::Face*> pairedBendFaces(1, candidate.bendFace);
        NXOpen::Face* oppositeBendFace = nullptr;
        try
        {
            oppositeBendFace = manager->GetOppositeFace(candidate.bendFace);
            if (oppositeBendFace != nullptr &&
                (!IsAlive(oppositeBendFace->Tag()) ||
                 !IsBendFace(oppositeBendFace)))
            {
                oppositeBendFace = nullptr;
            }
        }
        catch (...)
        {
            oppositeBendFace = nullptr;
        }
        if (oppositeBendFace == nullptr)
        {
            double bestRadiusDifference = DBL_MAX;
            for (NXOpen::Face* possible : candidate.body->GetFaces())
            {
                if (possible == nullptr ||
                    possible->Tag() == candidate.bendFace->Tag() ||
                    !IsBendFace(possible))
                {
                    continue;
                }
                int possibleType = 0;
                double possibleAxisPointData[3] = {};
                double possibleAxisData[3] = {};
                double possibleBox[6] = {};
                double possibleRadius = 0.0;
                double possibleRadiusData = 0.0;
                int possibleNormalDirection = 0;
                if (UF_MODL_ask_face_data(
                        possible->Tag(), &possibleType,
                        possibleAxisPointData, possibleAxisData,
                        possibleBox, &possibleRadius,
                        &possibleRadiusData,
                        &possibleNormalDirection) != 0 ||
                    possibleType != UF_MODL_CYLINDRICAL_FACE ||
                    possibleRadius <= 0.0)
                {
                    continue;
                }
                const NXOpen::Vector3d possibleAxis = Normalize(
                    NXOpen::Vector3d(possibleAxisData[0],
                        possibleAxisData[1], possibleAxisData[2]));
                if (std::fabs(Dot(axis, possibleAxis)) < 0.9999)
                {
                    continue;
                }
                const NXOpen::Vector3d axisOffset = Subtract(
                    NXOpen::Point3d(possibleAxisPointData[0],
                        possibleAxisPointData[1],
                        possibleAxisPointData[2]), axisPoint);
                const NXOpen::Vector3d perpendicularOffset =
                    NXOpen::Vector3d(
                        axisOffset.X - axis.X * Dot(axisOffset, axis),
                        axisOffset.Y - axis.Y * Dot(axisOffset, axis),
                        axisOffset.Z - axis.Z * Dot(axisOffset, axis));
                if (Magnitude(perpendicularOffset) > 1.0e-3)
                {
                    continue;
                }
                const double radiusDifference =
                    std::fabs(possibleRadius - radius);
                if (radiusDifference > 1.0e-5 &&
                    radiusDifference < bestRadiusDifference)
                {
                    bestRadiusDifference = radiusDifference;
                    oppositeBendFace = possible;
                }
            }
        }
        if (oppositeBendFace == nullptr)
        {
            throw std::runtime_error("未找到同一折弯的内R和外R两个圆柱面。");
        }
        pairedBendFaces.push_back(oppositeBendFace);
        {
            std::ostringstream log;
            log << "UNBEND_PAIRED_FACES first="
                << candidate.bendFace->Tag()
                << " second=" << oppositeBendFace->Tag()
                << " count=" << pairedBendFaces.size();
            AppendAnalysisLog(log.str());
        }
        stage = "创建展开折弯面规则";
        NXOpen::FaceDumbRule* bendRule =
            workPart->ScRuleFactory()->CreateRuleFaceDumb(
                pairedBendFaces);
        NXOpen::ScCollector* bendCollector =
            workPart->ScCollectors()->CreateCollector();
        if (bendCollector == nullptr)
        {
            throw std::runtime_error("创建展开折弯面收集器失败。");
        }
        bendCollector->ReplaceRules(
            std::vector<NXOpen::SelectionIntentRule*>(1, bendRule), false);
        stage = "设置展开折弯面";
        unbendBuilder->SetFaceCollector(bendCollector);

        stage = "提交展开特征";
        NXOpen::Features::Feature* unbendFeature =
            unbendBuilder->CommitFeature();

        stage = "读取展开后的中心线";
        std::vector<NXOpen::NXObject*> committedObjects =
            unbendBuilder->GetCommittedObjects();
        if (unbendFeature == nullptr)
        {
            unbendBuilder->Destroy();
            throw std::runtime_error("伸直折弯失败。");
        }
        unbendCommitted = true;

        std::vector<NXOpen::Point3d> flatBoundaryMids;
        std::ostringstream entityLog;
        entityLog << "UNBEND_BOUNDARY_EDGES first="
            << candidate.tangentEdge->Tag()
            << " first_alive="
            << (IsAlive(candidate.tangentEdge->Tag()) ? 1 : 0)
            << " second="
            << (otherTangentEdge == nullptr ? 0 : otherTangentEdge->Tag())
            << " second_alive="
            << (otherTangentEdge != nullptr &&
                IsAlive(otherTangentEdge->Tag()) ? 1 : 0);
        for (NXOpen::Edge* rememberedEdge :
             std::vector<NXOpen::Edge*>{
                 candidate.tangentEdge, otherTangentEdge})
        {
            if (rememberedEdge == nullptr ||
                !IsAlive(rememberedEdge->Tag()))
            {
                continue;
            }
            try
            {
                const NXOpen::Point3d midpoint =
                    edgeMidPoint(rememberedEdge);
                flatBoundaryMids.push_back(midpoint);
                entityLog << " midpoint=" << midpoint.X << ','
                    << midpoint.Y << ',' << midpoint.Z;
            }
            catch (...) {}
        }
        AppendAnalysisLog(entityLog.str());
        unbendBuilder->Destroy();
        if (flatBoundaryMids.size() < 2)
        {
            throw std::runtime_error(
                "展开后无法读取记住的两条折弯长直边。");
        }
        const NXOpen::Point3d firstFlatMid = flatBoundaryMids[0];
        const NXOpen::Point3d secondFlatMid = flatBoundaryMids[1];
        const NXOpen::Point3d flatBoundaryCenter(
            0.5 * (firstFlatMid.X + secondFlatMid.X),
            0.5 * (firstFlatMid.Y + secondFlatMid.Y),
            0.5 * (firstFlatMid.Z + secondFlatMid.Z));
        const NXOpen::Vector3d bendLineDirection = axis;
        // The two remembered long-edge midpoints determine the transverse
        // bend center only. Preserve the axial location obtained from the
        // safe-offset-line/inner-loop intersection midpoint.
        const NXOpen::Point3d flatCenter = Add(
            flatBoundaryCenter,
            Scale(bendLineDirection,
                Dot(Subtract(candidate.slotPositionPoint,
                             flatBoundaryCenter),
                    bendLineDirection)));
        const NXOpen::Vector3d flatNormal =
            Normalize(candidate.carrierNormal);
        const NXOpen::Vector3d slotWidthDirection =
            Normalize(Cross(bendLineDirection, flatNormal));
        {
            std::ostringstream geometryLog;
            geometryLog << "SLOT_GEOMETRY bend_center="
                << flatCenter.X << ',' << flatCenter.Y << ',' << flatCenter.Z
                << " boundary_center="
                << flatBoundaryCenter.X << ',' << flatBoundaryCenter.Y << ','
                << flatBoundaryCenter.Z
                << " first_boundary="
                << firstFlatMid.X << ',' << firstFlatMid.Y << ','
                << firstFlatMid.Z
                << " second_boundary="
                << secondFlatMid.X << ',' << secondFlatMid.Y << ','
                << secondFlatMid.Z
                << " boundary_spacing="
                << Magnitude(Subtract(secondFlatMid, firstFlatMid))
                << " curve_source=REMEMBERED_BEND_EDGES"
                << " committed_count=" << committedObjects.size()
                << " hole_center="
                << candidate.holeCenter.X << ','
                << candidate.holeCenter.Y << ','
                << candidate.holeCenter.Z
                << " tangent_edge_midpoint="
                << tangentEdgeMidPoint.X << ','
                << tangentEdgeMidPoint.Y << ','
                << tangentEdgeMidPoint.Z
                << " offset_intersection_midpoint="
                << candidate.slotPositionPoint.X << ','
                << candidate.slotPositionPoint.Y << ','
                << candidate.slotPositionPoint.Z
                << " configured_length=" << length
                << " width=" << width
                << " length_parallel_to_bend=1";
            AppendAnalysisLog(geometryLog.str());
        }
        stage = "创建展开状态贯穿槽";
        if (!CreateThroughSlot(
                candidate.body, flatCenter, bendLineDirection,
                slotWidthDirection, flatNormal, length, width))
        {
            throw std::runtime_error("在展开状态创建贯穿槽失败。");
        }
        cutCreated = true;

        stage = "收集重新折弯面";
        std::vector<NXOpen::Face*> unbentFaces;
        std::vector<NXOpen::Face*> currentBendFaces;
        std::vector<NXOpen::Features::SheetMetal::SheetmetalBendState>
            currentBendStates;
        manager->GetInnerBendFaces(
            candidate.body, currentBendFaces, currentBendStates);
        std::ostringstream rebendFacesLog;
        rebendFacesLog << "REBEND_FACE_QUERY count="
            << currentBendFaces.size();
        NXOpen::Face* nearestBendFace = nullptr;
        double nearestBoxDistance = DBL_MAX;
        for (std::size_t faceIndex = 0;
             faceIndex < currentBendFaces.size(); ++faceIndex)
        {
            NXOpen::Face* face = currentBendFaces[faceIndex];
            const bool hasState = faceIndex < currentBendStates.size();
            const bool isFlat = hasState &&
                currentBendStates[faceIndex] ==
                    NXOpen::Features::SheetMetal::SheetmetalBendStateFlat;
            rebendFacesLog << " face="
                << (face == nullptr ? 0 : face->Tag())
                << " state="
                << (hasState ? static_cast<int>(currentBendStates[faceIndex])
                             : -1)
                << " alive="
                << (face != nullptr && IsAlive(face->Tag()) ? 1 : 0);
            if (face != nullptr && IsAlive(face->Tag()))
            {
                double faceBox[6] = {};
                if (UF_MODL_ask_bounding_box(face->Tag(), faceBox) == 0)
                {
                    double squaredDistance = 0.0;
                    const double point[3] = {
                        flatCenter.X, flatCenter.Y, flatCenter.Z};
                    for (int coordinate = 0; coordinate < 3; ++coordinate)
                    {
                        const double delta = point[coordinate] < faceBox[coordinate]
                            ? faceBox[coordinate] - point[coordinate]
                            : (point[coordinate] > faceBox[coordinate + 3]
                                ? point[coordinate] - faceBox[coordinate + 3]
                                : 0.0);
                        squaredDistance += delta * delta;
                    }
                    rebendFacesLog << " box_distance="
                        << std::sqrt(squaredDistance);
                    if (squaredDistance < nearestBoxDistance)
                    {
                        nearestBoxDistance = squaredDistance;
                        nearestBendFace = face;
                    }
                }
            }
            if (face != nullptr && IsAlive(face->Tag()) && isFlat)
            {
                unbentFaces.push_back(face);
            }
        }
        // Converted bodies can keep reporting the source cylinder as Bent.
        // Locate the actual unfolded planar bend face at sample points just
        // outside both sides of the new slot instead.
        if (unbentFaces.empty())
        {
            auto findUnfoldedPlanarFace =
                [&](const NXOpen::Point3d& sample) -> NXOpen::Face*
            {
                double coordinates[3] = {sample.X, sample.Y, sample.Z};
                tag_t samplePoint = NULL_TAG;
                if (UF_CURVE_create_point(coordinates, &samplePoint) != 0 ||
                    samplePoint == NULL_TAG)
                {
                    return nullptr;
                }
                NXOpen::Face* nearestBendPlane = nullptr;
                NXOpen::Face* nearestAnyPlane = nullptr;
                double nearestBendDistance = DBL_MAX;
                double nearestAnyDistance = DBL_MAX;
                for (NXOpen::Face* face : candidate.body->GetFaces())
                {
                    if (face == nullptr || !IsAlive(face->Tag()) ||
                        face->Tag() == candidate.referenceFace->Tag())
                    {
                        continue;
                    }
                    int type = 0;
                    double pointData[3] = {};
                    double normalData[3] = {};
                    double faceBox[6] = {};
                    double faceRadius = 0.0;
                    double faceRadiusData = 0.0;
                    int faceNormalDirection = 0;
                    if (UF_MODL_ask_face_data(
                            face->Tag(), &type, pointData, normalData,
                            faceBox, &faceRadius, &faceRadiusData,
                            &faceNormalDirection) != 0 ||
                        type != UF_MODL_PLANAR_FACE)
                    {
                        continue;
                    }
                    NXOpen::Vector3d faceNormal(
                        normalData[0], normalData[1], normalData[2]);
                    if (Magnitude(faceNormal) <= 1.0e-9 ||
                        std::fabs(Dot(Normalize(faceNormal), flatNormal)) < 0.9)
                    {
                        continue;
                    }
                    double distance = DBL_MAX;
                    if (!MinimumDistance(
                            face->Tag(), samplePoint, distance, nullptr,
                            nullptr))
                    {
                        continue;
                    }
                    if (distance < nearestAnyDistance)
                    {
                        nearestAnyDistance = distance;
                        nearestAnyPlane = face;
                    }
                    try
                    {
                        if (manager->GetFaceType(face) ==
                                NXOpen::Features::SheetMetal::
                                    SheetmetalFaceTypeBend &&
                            distance < nearestBendDistance)
                        {
                            nearestBendDistance = distance;
                            nearestBendPlane = face;
                        }
                    }
                    catch (...) {}
                }
                DeleteObject(samplePoint);
                NXOpen::Face* selected = nearestBendPlane != nullptr
                    ? nearestBendPlane : nearestAnyPlane;
                rebendFacesLog << " sample=" << sample.X << ','
                    << sample.Y << ',' << sample.Z
                    << " planar_face="
                    << (selected == nullptr ? 0 : selected->Tag())
                    << " distance="
                    << (nearestBendPlane != nullptr
                        ? nearestBendDistance : nearestAnyDistance);
                return selected;
            };
            const double sampleOffset = (std::max)(0.2, width);
            NXOpen::Face* firstPlanar = findUnfoldedPlanarFace(
                Add(flatCenter, Scale(slotWidthDirection, sampleOffset)));
            NXOpen::Face* secondPlanar = findUnfoldedPlanarFace(
                Add(flatCenter, Scale(slotWidthDirection, -sampleOffset)));
            if (firstPlanar != nullptr)
            {
                unbentFaces.push_back(firstPlanar);
            }
            if (secondPlanar != nullptr &&
                (firstPlanar == nullptr ||
                 secondPlanar->Tag() != firstPlanar->Tag()))
            {
                unbentFaces.push_back(secondPlanar);
            }
            if (unbentFaces.empty() && nearestBendFace != nullptr)
            {
                unbentFaces.push_back(nearestBendFace);
                rebendFacesLog << " last_resort_source="
                    << nearestBendFace->Tag();
            }
        }
        // Rebend uses only the same paired inner-R and outer-R bend faces.
        // Do not add the carrier/reference face to the rebend selection.
        unbentFaces.clear();
        rebendFacesLog << " paired_rebend_faces=";
        for (NXOpen::Face* pairedFace : pairedBendFaces)
        {
            if (pairedFace != nullptr && IsAlive(pairedFace->Tag()))
            {
                unbentFaces.push_back(pairedFace);
                int currentType = 0;
                UF_MODL_ask_face_type(pairedFace->Tag(), &currentType);
                rebendFacesLog << pairedFace->Tag()
                    << "(type=" << currentType << "),";
            }
        }
        rebendFacesLog << " selected_count=" << unbentFaces.size();
        AppendAnalysisLog(rebendFacesLog.str());
        if (unbentFaces.size() != 2)
        {
            throw std::runtime_error("展开后未保留内R、外R两个折弯面。");
        }
        stage = "创建重新折弯构建器";
        NXOpen::Features::SheetMetal::RebendBuilder* rebendBuilder =
            manager->CreateRebendFeatureBuilder(nullptr);
        stage = "创建重新折弯面规则";
        NXOpen::FaceDumbRule* rebendRule =
            workPart->ScRuleFactory()->CreateRuleFaceDumb(unbentFaces);
        NXOpen::ScCollector* rebendCollector =
            workPart->ScCollectors()->CreateCollector();
        if (rebendCollector == nullptr)
        {
            throw std::runtime_error("创建重新折弯面收集器失败。");
        }
        rebendCollector->ReplaceRules(
            std::vector<NXOpen::SelectionIntentRule*>(1, rebendRule), false);
        stage = "设置重新折弯面";
        rebendBuilder->SetFaceCollector(rebendCollector);
        stage = "提交重新折弯特征";
        NXOpen::Features::Feature* rebendFeature =
            rebendBuilder->CommitFeature();
        rebendBuilder->Destroy();
        if (rebendFeature == nullptr)
        {
            throw std::runtime_error("重新折弯失败。");
        }
        AppendAnalysisLog(
            "SLOT_OK workflow=UNBEND_CUT_REBEND body=" +
            std::to_string(candidate.body->Tag()));
        return true;
    }
    catch (const NXOpen::NXException& ex)
    {
        std::ostringstream log;
        log << "SLOT_FAIL workflow=UNBEND_CUT_REBEND stage=" << stage
            << " nx_error=" << ex.ErrorCode()
            << " message=" << (ex.Message() != nullptr ? ex.Message() : "")
            << " preserve_model=1"
            << " unbend_committed=" << (unbendCommitted ? 1 : 0)
            << " cut_created=" << (cutCreated ? 1 : 0);
        AppendAnalysisLog(log.str());
        return cutCreated;
    }
    catch (const std::exception& ex)
    {
        AppendAnalysisLog(
            std::string("SLOT_FAIL workflow=UNBEND_CUT_REBEND stage=") +
            stage + " message=" + ex.what() +
            " preserve_model=1 unbend_committed=" +
            (unbendCommitted ? "1" : "0") + " cut_created=" +
            (cutCreated ? "1" : "0"));
        return cutCreated;
    }
}

bool KonFanLaLiaoDialog::CreateThroughSlot(
    NXOpen::Body* body,
    const NXOpen::Point3d& center,
    const NXOpen::Vector3d& lengthDirectionInput,
    const NXOpen::Vector3d& widthDirectionInput,
    const NXOpen::Vector3d& normalInput,
    double length, double width)
{
    if (body == nullptr || !IsAlive(body->Tag()))
    {
        return false;
    }
    const NXOpen::Vector3d lengthDirection = Normalize(lengthDirectionInput);
    const NXOpen::Vector3d widthDirection = Normalize(widthDirectionInput);
    const NXOpen::Vector3d normal = Normalize(normalInput);
    const NXOpen::Vector3d halfLength = Scale(lengthDirection, 0.5 * length);
    const NXOpen::Vector3d halfWidth = Scale(widthDirection, 0.5 * width);
    const NXOpen::Point3d points[4] = {
        Add(Add(center, Scale(halfLength, -1.0)), halfWidth),
        Add(Add(center, halfLength), halfWidth),
        Add(Add(center, halfLength), Scale(halfWidth, -1.0)),
        Add(Add(center, Scale(halfLength, -1.0)), Scale(halfWidth, -1.0))};

    std::vector<tag_t> curves;
    uf_list_p_t curveList = nullptr;
    uf_list_p_t featureList = nullptr;
    tag_t toolFeature = NULL_TAG;
    tag_t toolBody = NULL_TAG;
    try
    {
        for (int index = 0; index < 4; ++index)
        {
            const tag_t curve = CreateLine(points[index], points[(index + 1) % 4]);
            if (curve == NULL_TAG)
            {
                throw std::runtime_error("创建防拉槽轮廓失败。");
            }
            curves.push_back(curve);
        }
        if (UF_MODL_create_list(&curveList) != 0 || curveList == nullptr)
        {
            throw std::runtime_error("创建防拉槽曲线列表失败。");
        }
        for (tag_t curve : curves)
        {
            if (UF_MODL_put_list_item(curveList, curve) != 0)
            {
                throw std::runtime_error("写入防拉槽曲线失败。");
            }
        }

        NXOpen::Part* workPart = session_->Parts()->Work();
        NXOpen::Features::SheetMetal::SheetmetalManager* manager =
            workPart == nullptr || workPart->Features() == nullptr
                ? nullptr : workPart->Features()->SheetmetalManager();
        double thickness = 0.0;
        if (manager != nullptr)
        {
            try
            {
                thickness = manager->GetBodyThickness(body);
            }
            catch (...) {}
        }
        if (thickness <= 1.0e-9)
        {
            throw std::runtime_error("读取钣金板厚失败。");
        }

        NXOpen::Vector3d inwardNormal = normal;
        const double probeDistance = (std::max)(0.01, 0.25 * thickness);
        double positiveProbe[3] = {
            center.X + normal.X * probeDistance,
            center.Y + normal.Y * probeDistance,
            center.Z + normal.Z * probeDistance};
        double negativeProbe[3] = {
            center.X - normal.X * probeDistance,
            center.Y - normal.Y * probeDistance,
            center.Z - normal.Z * probeDistance};
        int positiveStatus = 0;
        int negativeStatus = 0;
        const bool positiveInside =
            UF_MODL_ask_point_containment(
                positiveProbe, body->Tag(), &positiveStatus) == 0 &&
            positiveStatus == 1;
        const bool negativeInside =
            UF_MODL_ask_point_containment(
                negativeProbe, body->Tag(), &negativeStatus) == 0 &&
            negativeStatus == 1;
        if (!positiveInside && negativeInside)
        {
            inwardNormal = Scale(normal, -1.0);
        }
        else if (!positiveInside && !negativeInside)
        {
            double bodyBox[6] = {};
            if (UF_MODL_ask_bounding_box(body->Tag(), bodyBox) == 0)
            {
                const NXOpen::Point3d boxCenter(
                    0.5 * (bodyBox[0] + bodyBox[3]),
                    0.5 * (bodyBox[1] + bodyBox[4]),
                    0.5 * (bodyBox[2] + bodyBox[5]));
                if (Dot(Subtract(boxCenter, center), normal) < 0.0)
                {
                    inwardNormal = Scale(normal, -1.0);
                }
            }
        }
        const double cutDepth = thickness + 0.5;
        const std::string startLimit = "-0.25";
        const std::string endLimit = std::to_string(thickness + 0.25);
        char taper[] = "0";
        char* limits[2] = {
            const_cast<char*>(startLimit.c_str()),
            const_cast<char*>(endLimit.c_str())};
        double origin[3] = {
            center.X, center.Y, center.Z};
        double direction[3] = {
            inwardNormal.X, inwardNormal.Y, inwardNormal.Z};
        {
            std::ostringstream depthLog;
            depthLog << "SLOT_CUT_DEPTH body=" << body->Tag()
                << " thickness=" << thickness
                << " depth=" << cutDepth
                << " positive_status=" << positiveStatus
                << " negative_status=" << negativeStatus
                << " reversed=" << (Dot(inwardNormal, normal) < 0.0 ? 1 : 0);
            AppendAnalysisLog(depthLog.str());
        }
        const int extrudeStatus = UF_MODL_create_extruded(
            curveList, taper, limits, origin, direction,
            UF_NULLSIGN, &featureList);
        UF_MODL_delete_list(&curveList);
        curveList = nullptr;
        if (extrudeStatus != 0 || featureList == nullptr)
        {
            throw std::runtime_error("贯穿拉伸防拉槽失败。");
        }
        int featureCount = 0;
        if (UF_MODL_ask_list_count(featureList, &featureCount) != 0 ||
            featureCount <= 0 ||
            UF_MODL_ask_list_item(featureList, 0, &toolFeature) != 0 ||
            UF_MODL_ask_feat_body(toolFeature, &toolBody) != 0 ||
            toolBody == NULL_TAG)
        {
            throw std::runtime_error("取得防拉槽工具体失败。");
        }
        UF_MODL_delete_list(&featureList);
        featureList = nullptr;

        uf_list_p_t bodyList = nullptr;
        if (UF_MODL_create_list(&bodyList) == 0 && bodyList != nullptr)
        {
            UF_MODL_put_list_item(bodyList, toolBody);
            UF_MODL_delete_body_parms(bodyList);
            UF_MODL_delete_list(&bodyList);
        }
        for (tag_t curve : curves)
        {
            DeleteObject(curve);
        }
        curves.clear();

        int resultCount = 0;
        tag_t* resultingBodies = nullptr;
        const int subtractStatus = UF_MODL_subtract_bodies(
            body->Tag(), toolBody, &resultCount, &resultingBodies);
        if (resultingBodies != nullptr)
        {
            UF_free(resultingBodies);
        }
        if (subtractStatus != 0)
        {
            throw std::runtime_error("防拉槽布尔减失败。");
        }
        DeleteObject(toolBody);
        return true;
    }
    catch (const std::exception& ex)
    {
        if (curveList != nullptr) UF_MODL_delete_list(&curveList);
        if (featureList != nullptr) UF_MODL_delete_list(&featureList);
        for (tag_t curve : curves) DeleteObject(curve);
        DeleteObject(toolBody);
        DeleteObject(toolFeature);
        AppendAnalysisLog(std::string("SLOT_FAIL ") + ex.what());
        return false;
    }
}

void KonFanLaLiaoDialog::RestoreDisplay()
{
    for (tag_t faceTag : highlightedFaces_)
    {
        if (IsAlive(faceTag))
        {
            UF_DISP_set_highlight(faceTag, 0);
        }
    }
    highlightedFaces_.clear();

    for (const std::pair<tag_t, int>& state : originalColors_)
    {
        if (IsAlive(state.first))
        {
            UF_OBJ_set_color(state.first, state.second);
        }
    }
    originalColors_.clear();

    for (const std::pair<tag_t, int>& state : originalTranslucencies_)
    {
        if (!IsAlive(state.first))
        {
            continue;
        }
        UF_OBJ_set_translucency(
            state.first,
            static_cast<UF_OBJ_translucency_t>(state.second));
        NXOpen::DisplayableObject* object =
            dynamic_cast<NXOpen::DisplayableObject*>(
                NXOpen::NXObjectManager::Get(state.first));
        if (object != nullptr)
        {
            object->RedisplayObject();
        }
    }
    originalTranslucencies_.clear();

    if (session_ != nullptr && session_->DisplayManager() != nullptr)
    {
        session_->DisplayManager()->MakeUpToDate();
    }
}

void KonFanLaLiaoDialog::RefreshDisplay(const AnalysisResult& result)
{
    RestoreDisplay();
    std::set<tag_t> riskTags;
    for (NXOpen::Face* face : result.riskFaces)
    {
        if (face != nullptr)
        {
            riskTags.insert(face->Tag());
        }
    }

    for (NXOpen::Body* body : TargetBodies())
    {
        for (NXOpen::Face* face : body->GetFaces())
        {
            if (face == nullptr || !IsAlive(face->Tag()))
            {
                continue;
            }
            UF_OBJ_translucency_t original = 0;
            if (UF_OBJ_ask_translucency(face->Tag(), &original) != 0)
            {
                continue;
            }
            originalTranslucencies_.emplace_back(
                face->Tag(), static_cast<int>(original));
            const int translucency =
                riskTags.count(face->Tag()) != 0
                    ? 0
                    : (std::max)(static_cast<int>(original),
                                 kBackgroundTranslucency);
            UF_OBJ_set_translucency(
                face->Tag(),
                static_cast<UF_OBJ_translucency_t>(translucency));
            face->RedisplayObject();
        }
    }
    std::vector<NXOpen::DisplayableObject*> riskObjects;
    for (NXOpen::Face* face : result.riskFaces)
    {
        if (face != nullptr && IsAlive(face->Tag()))
        {
            UF_OBJ_disp_props_t displayProperties = {};
            if (UF_OBJ_ask_display_properties(
                    face->Tag(), &displayProperties) == 0)
            {
                originalColors_.emplace_back(
                    face->Tag(), displayProperties.color);
            }
            highlightedFaces_.push_back(face->Tag());
            riskObjects.push_back(face);
        }
    }
    if (!riskObjects.empty())
    {
        NXOpen::DisplayModification* modification =
            session_->DisplayManager()->NewDisplayModification();
        modification->SetApplyToAllFaces(false);
        modification->SetApplyToOwningParts(false);
        modification->SetNewColor(kRiskFaceColor);
        modification->SetNewTranslucency(0);
        modification->Apply(riskObjects);
        delete modification;
        for (NXOpen::DisplayableObject* object : riskObjects)
        {
            object->Highlight();
        }
        session_->DisplayManager()->MakeUpToDate();
    }
}

void KonFanLaLiaoDialog::RunAnalysis(bool showErrors)
{
    refreshing_ = true;
    try
    {
        const AnalysisResult result = Analyze();
        RefreshDisplay(result);
        NXOpen::BlockStyler::PropertyList* safetyMode =
            safetyDistanceMode_->GetProperties();
        const bool manualSafety = safetyMode->GetEnum("Value") == 1;
        delete safetyMode;
        NXOpen::BlockStyler::PropertyList* lengthMode =
            reliefLengthMode_->GetProperties();
        const bool manualLength = lengthMode->GetEnum("Value") == 1;
        delete lengthMode;
        std::ostringstream message;
        message << "检测实体：" << result.bodyCount << " 个\n"
                << "普通折弯圆柱面：" << result.bendCount
                << " 个；排除大圆弧面："
                << result.largeArcExcludedCount << " 个\n"
                << "内环孔：" << result.holeCount
                << " 个；风险孔：" << result.riskHoleCount
                << " 个；风险面：" << result.riskFaces.size() << " 个\n"
                << "安全距离：";
        if (manualSafety)
            message << "手动 " << DoubleValue(riskDistance_) << " mm";
        else
            message << "公式自动识别";
        message << "\n防拉槽长度：";
        if (manualLength)
            message << "手动 " << DoubleValue(reliefLength_) << " mm";
        else
            message << "两交点距离+两端各1 mm";
        message << "；槽宽：" << DoubleValue(slotWidth_)
                << " mm\n处理方式：伸直折弯、沿折弯中心线贯穿开槽、重新折弯。";
        if (result.bendCount == 0)
        {
            message << " 未识别到带直母线边的圆柱折弯面。";
        }
        SetStatus(message.str());
    }
    catch (const NXOpen::NXException& ex)
    {
        RestoreDisplay();
        SetStatus("检测失败，请检查所选实体和参数。");
        if (showErrors)
        {
            ShowError(ex.Message() != nullptr ? ex.Message() : "NX 检测失败。");
        }
    }
    catch (const std::exception& ex)
    {
        RestoreDisplay();
        SetStatus(ex.what());
        if (showErrors)
        {
            ShowError(ex.what());
        }
    }
    refreshing_ = false;
}

void KonFanLaLiaoDialog::ShowError(const std::string& message) const
{
    ui_->NXMessageBox()->Show(
        NXOpen::NXString("孔防拉料检查", NXOpen::NXString::UTF8),
        NXOpen::NXMessageBox::DialogTypeError,
        NXOpen::NXString(message, NXOpen::NXString::UTF8));
}
