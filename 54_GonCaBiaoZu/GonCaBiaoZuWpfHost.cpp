#include "GonCaBiaoZuWpfHost.hpp"
#include "GonCaBiaoZuTables.hpp"
#include "../../common/ZhihuiContextHelp.hpp"

#ifdef CreateDialog
#undef CreateDialog
#endif

#include <NXOpen/Annotations.hxx>
#include <NXOpen/Annotations_AnnotationManager.hxx>
#include <NXOpen/Annotations_AppendedText.hxx>
#include <NXOpen/Annotations_DatumFeatureSymbolBuilder.hxx>
#include <NXOpen/Annotations_DatumReferenceBuilder.hxx>
#include <NXOpen/Annotations_Dimension.hxx>
#include <NXOpen/Annotations_DimensionPreferences.hxx>
#include <NXOpen/Annotations_DraftingDatumFeatureSymbolBuilder.hxx>
#include <NXOpen/Annotations_DraftingFeatureControlFrameBuilder.hxx>
#include <NXOpen/Annotations_FeatureControlFrameBuilder.hxx>
#include <NXOpen/Annotations_FeatureControlFrameDataBuilder.hxx>
#include <NXOpen/Annotations_FeatureControlFrameDataBuilderList.hxx>
#include <NXOpen/Annotations_GdtDatumCollection.hxx>
#include <NXOpen/Annotations_ILinearTolerance.hxx>
#include <NXOpen/Annotations_LeaderBuilder.hxx>
#include <NXOpen/Annotations_LeaderData.hxx>
#include <NXOpen/Annotations_LeaderDataList.hxx>
#include <NXOpen/Annotations_LetteringStyleBuilder.hxx>
#include <NXOpen/Annotations_LineArrowStyleBuilder.hxx>
#include <NXOpen/Annotations_LinearTolerance.hxx>
#include <NXOpen/Annotations_OriginBuilder.hxx>
#include <NXOpen/Annotations_PlaneBuilder.hxx>
#include <NXOpen/Annotations_StyleBuilder.hxx>
#include <NXOpen/Annotations_SymbolStyleBuilder.hxx>
#include <NXOpen/ColorManager.hxx>
#include <NXOpen/DisplayableObject.hxx>
#include <NXOpen/Drawings_DraftingCurve.hxx>
#include <NXOpen/Drawings_DraftingCurveInfo.hxx>
#include <NXOpen/InferSnapType.hxx>
#include <NXOpen/LogFile.hxx>
#include <NXOpen/NXColor.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/SelectDisplayableObject.hxx>
#include <NXOpen/Selection.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/UI.hxx>
#include <NXOpen/Update.hxx>
#include <NXOpen/View.hxx>

#include <uf.h>
#include <uf_defs.h>
#include <uf_disp.h>
#include <uf_eval.h>
#include <uf_obj.h>
#include <uf_object_types.h>
#include <uf_ui.h>
#include <uf_view.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <shellapi.h>
#ifdef CreateDialog
#undef CreateDialog
#endif

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace zhihui::gcbz
{
namespace
{

NXOpen::NXString U8(const std::string& s)
{
    return NXOpen::NXString(s.c_str(), NXOpen::NXString::UTF8);
}

NXOpen::NXString U8(const char* s)
{
    return NXOpen::NXString(s ? s : "", NXOpen::NXString::UTF8);
}

void WriteLog(const std::string& line)
{
    CreateDirectoryW(L"D:\\UG\u667A\u8F89\u94A3\u91D1\u63D2\u4EF6", nullptr);
    CreateDirectoryW(L"D:\\UG\u667A\u8F89\u94A3\u91D1\u63D2\u4EF6\\logs", nullptr);
    FILE* fp = _wfopen(L"D:\\UG\u667A\u8F89\u94A3\u91D1\u63D2\u4EF6\\logs\\GonCaBiaoZu_debug.log", L"ab");
    if (!fp) return;
    SYSTEMTIME st{};
    GetLocalTime(&st);
    char prefix[64]{};
    std::snprintf(prefix, sizeof(prefix), "[%04d-%02d-%02d %02d:%02d:%02d.%03d] [WPF] ",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    std::fwrite(prefix, 1, std::strlen(prefix), fp);
    std::fwrite(line.data(), 1, line.size(), fp);
    std::fwrite("\r\n", 1, 2, fp);
    std::fclose(fp);
}

std::filesystem::path CurrentModuleDir()
{
    wchar_t buf[MAX_PATH]{};
    const DWORD len = GetModuleFileNameW(reinterpret_cast<HMODULE>(&__ImageBase), buf, MAX_PATH);
    if (len > 0 && len < MAX_PATH)
    {
        return std::filesystem::path(buf).parent_path();
    }
    return {};
}

std::map<std::string, std::string> ReadKeyValues(const std::wstring& path)
{
    std::map<std::string, std::string> kv;
    FILE* fp = _wfopen(path.c_str(), L"rb");
    if (!fp) return kv;
    std::string content;
    char buf[4096];
    while (size_t n = std::fread(buf, 1, sizeof(buf), fp))
    {
        content.append(buf, n);
    }
    std::fclose(fp);

    if (content.size() >= 3 &&
        static_cast<unsigned char>(content[0]) == 0xEF &&
        static_cast<unsigned char>(content[1]) == 0xBB &&
        static_cast<unsigned char>(content[2]) == 0xBF)
    {
        content.erase(0, 3);
    }

    std::istringstream iss(content);
    std::string line;
    while (std::getline(iss, line))
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const size_t eq = line.find('=');
        if (eq == std::string::npos || eq == 0) continue;
        std::string k = line.substr(0, eq);
        std::string v = line.substr(eq + 1);
        kv[k] = v;
    }
    return kv;
}

void WriteKeyValues(const std::wstring& path, const std::map<std::string, std::string>& kv)
{
    FILE* fp = _wfopen(path.c_str(), L"wb");
    if (!fp) return;
    for (const auto& [k, v] : kv)
    {
        std::string line = k + "=" + v + "\n";
        std::fwrite(line.data(), 1, line.size(), fp);
    }
    std::fclose(fp);
}

int GetInt(const std::map<std::string, std::string>& kv, const char* key, int fallback)
{
    auto it = kv.find(key);
    if (it == kv.end() || it->second.empty()) return fallback;
    try { return std::stoi(it->second); } catch (...) { return fallback; }
}

double GetDouble(const std::map<std::string, std::string>& kv, const char* key, double fallback)
{
    auto it = kv.find(key);
    if (it == kv.end() || it->second.empty()) return fallback;
    try { return std::stod(it->second); } catch (...) { return fallback; }
}

std::string GetStr(const std::map<std::string, std::string>& kv, const char* key, const std::string& fallback = {})
{
    auto it = kv.find(key);
    if (it == kv.end()) return fallback;
    return it->second;
}

std::string DatumEnumToString(int val)
{
    switch (val)
    {
    case 1: return "A";
    case 2: return "B";
    case 3: return "C";
    case 4: return "D";
    case 5: return "A-B";
    default: return "";
    }
}

std::string NextDatumLetter(const std::string& current)
{
    if (current.empty()) return "A";
    std::string next = current;
    char last = next.back();
    if (last >= 'A' && last < 'Z')
    {
        next.back() = static_cast<char>(last + 1);
        if (next.back() == 'I' || next.back() == 'O' || next.back() == 'Q')
        {
            next.back() = static_cast<char>(next.back() + 1);
        }
        return next;
    }
    if (last >= 'a' && last < 'z')
    {
        next.back() = static_cast<char>(last + 1);
        return next;
    }
    return "A";
}

double MeasureObjectLengthOrSize(NXOpen::TaggedObject* obj)
{
    if (!obj) return 50.0;
    if (auto* dim = dynamic_cast<NXOpen::Annotations::Dimension*>(obj))
    {
        const double s = dim->ComputedSize();
        return s > 1.0e-3 ? s : 50.0;
    }
    const tag_t tag = obj->Tag();
    if (tag == NULL_TAG) return 50.0;

    UF_EVAL_p_t eval = nullptr;
    if (UF_EVAL_initialize(tag, &eval) == 0 && eval != nullptr)
    {
        double limits[2]{0.0, 1.0};
        UF_EVAL_ask_limits(eval, limits);
        double p0[3]{}, p1[3]{};
        if (UF_EVAL_evaluate(eval, 0, limits[0], p0, nullptr) == 0 &&
            UF_EVAL_evaluate(eval, 0, limits[1], p1, nullptr) == 0)
        {
            const double dx = p1[0] - p0[0];
            const double dy = p1[1] - p0[1];
            const double dz = p1[2] - p0[2];
            const double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (dist > 1.0e-3)
            {
                UF_EVAL_free(eval);
                return dist;
            }
        }
        bool isArc = false;
        if (UF_EVAL_is_arc(eval, &isArc) == 0 && isArc)
        {
            UF_EVAL_arc_t arc{};
            if (UF_EVAL_ask_arc(eval, &arc) == 0 && arc.radius > 1.0e-3)
            {
                UF_EVAL_free(eval);
                return arc.radius * 2.0;
            }
        }
        UF_EVAL_free(eval);
    }
    return 50.0;
}

bool ResolveLeaderAttachmentTarget(
    NXOpen::TaggedObject* pickedObject,
    NXOpen::DisplayableObject*& outTarget,
    NXOpen::View*& outView)
{
    outTarget = nullptr;
    outView = nullptr;
    if (!pickedObject) return false;

    outTarget = dynamic_cast<NXOpen::DisplayableObject*>(pickedObject);
    const tag_t tag = pickedObject->Tag();

    if (auto* draftingCurve = dynamic_cast<NXOpen::Drawings::DraftingCurve*>(pickedObject))
    {
        int numEdits = 0;
        UF_VIEW_vde_data_t* vdeData = nullptr;
        if (UF_VIEW_ask_vde_data(tag, &numEdits, &vdeData) == 0 && vdeData != nullptr)
        {
            const tag_t viewTag = vdeData->view_tag;
            UF_free(vdeData);
            if (viewTag != NULL_TAG)
            {
                outView = dynamic_cast<NXOpen::View*>(NXOpen::NXObjectManager::Get(viewTag));
            }
        }
        try
        {
            NXOpen::Drawings::DraftingCurveInfo* info = draftingCurve->GetDraftingCurveInfo();
            if (info != nullptr)
            {
                std::vector<NXOpen::NXObject*> parents = info->GetParents();
                delete info;
                for (NXOpen::NXObject* parent : parents)
                {
                    if (auto* disp = dynamic_cast<NXOpen::DisplayableObject*>(parent))
                    {
                        outTarget = disp;
                        break;
                    }
                }
            }
        }
        catch (...) {}
        return outTarget != nullptr;
    }

    int numEdits = 0;
    UF_VIEW_vde_data_t* vdeData = nullptr;
    if (UF_VIEW_ask_vde_data(tag, &numEdits, &vdeData) == 0 && vdeData != nullptr)
    {
        const tag_t viewTag = vdeData->view_tag;
        UF_free(vdeData);
        if (viewTag != NULL_TAG)
        {
            outView = dynamic_cast<NXOpen::View*>(NXOpen::NXObjectManager::Get(viewTag));
        }
    }
    return outTarget != nullptr;
}

std::vector<NXOpen::Selection::MaskTriple> DimMasks()
{
    return { NXOpen::Selection::MaskTriple(UF_dimension_type, UF_all_subtype, 0) };
}

std::vector<NXOpen::Selection::MaskTriple> LeaderMasks()
{
    return {
        NXOpen::Selection::MaskTriple(UF_dimension_type, UF_all_subtype, 0),
        NXOpen::Selection::MaskTriple(UF_line_type, UF_all_subtype, 0),
        NXOpen::Selection::MaskTriple(UF_circle_type, UF_all_subtype, 0),
        NXOpen::Selection::MaskTriple(UF_conic_type, UF_all_subtype, 0),
        NXOpen::Selection::MaskTriple(UF_spline_type, UF_all_subtype, 0),
        NXOpen::Selection::MaskTriple(UF_solid_type, UF_solid_edge_subtype, 0),
        NXOpen::Selection::MaskTriple(UF_solid_type, UF_solid_face_subtype, 0),
        NXOpen::Selection::MaskTriple(UF_drafting_entity_type, UF_all_subtype, 0)
    };
}

void DeleteTagSafely(NXOpen::Session* session, tag_t& tag)
{
    if (tag == NULL_TAG) return;
    try
    {
        if (NXOpen::TaggedObject* obj = NXOpen::NXObjectManager::Get(tag))
        {
            const NXOpen::Session::UndoMarkId delMark =
                session->SetUndoMark(NXOpen::Session::MarkVisibilityInvisible, "GonCaBiaoZu Replace");
            std::vector<NXOpen::TaggedObject*> delList{obj};
            session->UpdateManager()->AddObjectsToDeleteList(delList);
            session->UpdateManager()->DoUpdate(delMark);
            if (session->DoesUndoMarkExist(delMark, nullptr))
            {
                session->DeleteUndoMark(delMark, nullptr);
            }
        }
    }
    catch (...)
    {
        try { UF_OBJ_delete_object(tag); } catch (...) {}
    }
    tag = NULL_TAG;
}

struct LastPlacementContext
{
    tag_t createdTag = NULL_TAG;
    tag_t targetTag = NULL_TAG;
    NXOpen::Point3d pickPt{0.0, 0.0, 0.0};
    NXOpen::Point3d placePt{100.0, 100.0, 0.0};
};

bool ApplyToleranceToDimension(
    NXOpen::Annotations::Dimension* dim,
    const std::map<std::string, std::string>& req,
    std::string& outSummary)
{
    if (!dim) return false;

    const int sub = GetInt(req, "dim_submode", 0);
    const int sheetIdx = GetInt(req, "dim_preset", 1);
    const int holeIdx = GetInt(req, "dim_hole_fit", 1);
    const int shaftIdx = GetInt(req, "dim_shaft_fit", 5);
    const int fitStyle = GetInt(req, "dim_fit_style", 0);
    const int gb1804Grade = GetInt(req, "dim_gb1804", 1);
    const double manualUp = GetDouble(req, "dim_upper", 0.1);
    const double manualLow = GetDouble(req, "dim_lower", -0.1);
    const int prefixMod = GetInt(req, "dim_prefix", 0);
    const int suffixMod = GetInt(req, "dim_suffix", 0);
    const bool refFlag = GetInt(req, "dim_ref", 0) != 0;

    const double nominal = dim->ComputedSize();
    double upVal = 0.0;
    double lowVal = 0.0;
    bool isSymmetric = false;
    bool clearTol = (sub == 6);
    bool useNativeLimitFit = false;
    std::string fitCodeText;

    if (sub == 0)
    {
        const auto presets = DefaultDimPresets();
        const int idx = std::clamp(sheetIdx, 0, static_cast<int>(presets.size()) - 1);
        upVal = presets[static_cast<std::size_t>(idx)].upper;
        lowVal = presets[static_cast<std::size_t>(idx)].lower;
        isSymmetric = presets[static_cast<std::size_t>(idx)].symmetric;
    }
    else if (sub == 1 || sub == 2)
    {
        const bool isHole = (sub == 1);
        std::string code, dev;
        int grade = 7;
        LookupFitByIndex(isHole, isHole ? holeIdx : shaftIdx, nominal, code, dev, grade, upVal, lowVal);
        fitCodeText = code;
        isSymmetric = (std::fabs(upVal + lowVal) < 1.0e-7 && upVal > 0.0);

        if (fitStyle >= 1)
        {
            useNativeLimitFit = true;
            const auto holeOrShaft = isHole
                ? NXOpen::Annotations::FitAnsiHoleTypeHole
                : NXOpen::Annotations::FitAnsiHoleTypeShaft;
            dim->SetToleranceType(NXOpen::Annotations::ToleranceTypeLimitsAndFits);
            dim->SetLimitFitEnhancedDisplayType(holeOrShaft);
            dim->SetLimitFitAnsiHoleType(holeOrShaft);
            if (isHole)
            {
                dim->SetLimitFitDeviation(U8(dev));
                dim->SetLimitFitGrade(grade);
            }
            else
            {
                dim->SetLimitFitShaftDeviation(U8(dev));
                dim->SetLimitFitShaftGrade(grade);
            }

            const auto dispStyle = (fitStyle == 1)
                ? NXOpen::Annotations::FitDisplayStyleFitSymbolsAndTolerances
                : (fitStyle == 2
                    ? NXOpen::Annotations::FitDisplayStyleFitSymbols
                    : NXOpen::Annotations::FitDisplayStyleFitSymbolsAndLimits);
            dim->SetLimitFitDisplayStyle(dispStyle);

            NXOpen::Annotations::DimensionPreferences* prefs = dim->GetDimensionPreferences();
            if (prefs)
            {
                prefs->SetLimitFitAnsiHoleType(holeOrShaft);
                if (isHole)
                {
                    prefs->SetLimitFitDeviation(U8(dev));
                    prefs->SetLimitFitGrade(grade);
                }
                else
                {
                    prefs->SetLimitFitShaftDeviation(U8(dev));
                    prefs->SetLimitFitShaftGrade(grade);
                }
                prefs->SetLimitFitDisplayStyle(dispStyle);
                prefs->SetDisplayFitParentheses(true);
                dim->SetDimensionPreferences(prefs);
                delete prefs;
            }
        }
    }
    else if (sub == 3)
    {
        std::string hCode, hDev, sCode, sDev;
        int hGrade = 7, sGrade = 6;
        double sUp = 0.0, sLow = 0.0;
        LookupFitByIndex(true, holeIdx, nominal, hCode, hDev, hGrade, upVal, lowVal);
        LookupFitByIndex(false, shaftIdx, nominal, sCode, sDev, sGrade, sUp, sLow);
        fitCodeText = hCode + "/" + sCode;

        useNativeLimitFit = true;
        dim->SetToleranceType(NXOpen::Annotations::ToleranceTypeLimitsAndFits);
        dim->SetLimitFitEnhancedDisplayType(NXOpen::Annotations::FitAnsiHoleTypeFit);
        dim->SetLimitFitAnsiHoleType(NXOpen::Annotations::FitAnsiHoleTypeFit);
        dim->SetLimitFitDeviation(U8(hDev));
        dim->SetLimitFitGrade(hGrade);
        dim->SetLimitFitShaftDeviation(U8(sDev));
        dim->SetLimitFitShaftGrade(sGrade);

        const auto dispStyle = (fitStyle == 2)
            ? NXOpen::Annotations::FitDisplayStyleFitSymbols
            : (fitStyle == 3
                ? NXOpen::Annotations::FitDisplayStyleFitSymbolsAndLimits
                : NXOpen::Annotations::FitDisplayStyleFitSymbolsAndTolerances);
        dim->SetLimitFitDisplayStyle(dispStyle);

        NXOpen::Annotations::DimensionPreferences* prefs = dim->GetDimensionPreferences();
        if (prefs)
        {
            prefs->SetLimitFitAnsiHoleType(NXOpen::Annotations::FitAnsiHoleTypeFit);
            prefs->SetLimitFitDeviation(U8(hDev));
            prefs->SetLimitFitGrade(hGrade);
            prefs->SetLimitFitShaftDeviation(U8(sDev));
            prefs->SetLimitFitShaftGrade(sGrade);
            prefs->SetLimitFitDisplayStyle(dispStyle);
            prefs->SetDisplayFitParentheses(true);
            prefs->SetLimitFitAssemblyNewDisplayStyle(NXOpen::Annotations::FitAssemblyNewDisplayStyleSingleLine);
            dim->SetDimensionPreferences(prefs);
            delete prefs;
        }
    }
    else if (sub == 4)
    {
        double pm = 0.2;
        LookupGb1804Linear(nominal, gb1804Grade, pm);
        upVal = pm;
        lowVal = -pm;
        isSymmetric = true;
    }
    else if (sub == 5)
    {
        upVal = manualUp;
        lowVal = manualLow;
        isSymmetric = (std::fabs(upVal + lowVal) < 1.0e-7 && upVal > 1.0e-7);
    }

    if (clearTol)
    {
        dim->SetToleranceType(NXOpen::Annotations::ToleranceTypeNone);
        dim->SetReferenceDimensionFlag(false);
        outSummary = "已清除公差";
    }
    else if (!useNativeLimitFit)
    {
        const int dec = std::max(CountDecimals(upVal), CountDecimals(lowVal));
        dim->SetMetricToleranceDecimalPlaces(dec);
        dim->SetToleranceDecimalPlaces(dec);

        if (isSymmetric)
        {
            dim->SetToleranceType(NXOpen::Annotations::ToleranceTypeBilateralOneLine);
            dim->SetUpperToleranceValue(std::fabs(upVal));
            dim->SetLowerToleranceValue(-std::fabs(upVal));
            dim->SetUpperMetricToleranceValue(std::fabs(upVal));
            dim->SetLowerMetricToleranceValue(-std::fabs(upVal));
            outSummary = "±" + FormatUnsignedTol(upVal);
        }
        else
        {
            dim->SetToleranceType(NXOpen::Annotations::ToleranceTypeBilateralTwoLines);
            dim->SetUpperToleranceValue(upVal);
            dim->SetLowerToleranceValue(lowVal);
            dim->SetUpperMetricToleranceValue(upVal);
            dim->SetLowerMetricToleranceValue(lowVal);
            if (auto* linTolIf = dynamic_cast<NXOpen::Annotations::ILinearTolerance*>(dim))
            {
                NXOpen::Annotations::LinearTolerance* lt = linTolIf->GetTolerance();
                if (lt)
                {
                    lt->SetZeroToleranceDisplayStyle(NXOpen::Annotations::ZeroToleranceDisplayStyleAsZero);
                    linTolIf->SetTolerance(lt);
                    delete lt;
                }
            }
            outSummary = FormatSignedTol(upVal) + " / " + FormatSignedTol(lowVal);
        }
        dim->SetReferenceDimensionFlag(refFlag);
    }
    else
    {
        const int dec = std::max(CountDecimals(upVal), CountDecimals(lowVal));
        dim->SetMetricToleranceDecimalPlaces(dec);
        dim->SetToleranceDecimalPlaces(dec);
        dim->SetReferenceDimensionFlag(refFlag);
        outSummary = fitCodeText;
    }

    if (prefixMod > 0 || suffixMod > 0)
    {
        NXOpen::Annotations::AppendedText* app = dim->GetAppendedText();
        if (app)
        {
            if (prefixMod == 1)
            {
                std::vector<NXOpen::NXString> emptyLines;
                app->SetBeforeText(emptyLines);
            }
            else if (prefixMod >= 2)
            {
                static const char* kPrefixes[] = {"", "", "2×", "4×", "6×", "8×", "□"};
                std::vector<NXOpen::NXString> lines{U8(kPrefixes[std::clamp(prefixMod, 2, 6)])};
                app->SetBeforeText(lines);
            }

            if (suffixMod == 1)
            {
                std::vector<NXOpen::NXString> emptyLines;
                app->SetAfterText(emptyLines);
            }
            else if (suffixMod >= 2)
            {
                static const char* kSuffixes[] = {"", "", " THRU", " EQS", " TYP", " 配作"};
                std::vector<NXOpen::NXString> lines{U8(kSuffixes[std::clamp(suffixMod, 2, 5)])};
                app->SetAfterText(lines);
            }

            dim->SetAppendedText(app);
            delete app;
        }
    }

    return true;
}

bool CreateGdtAtContext(
    NXOpen::Session* session,
    const std::map<std::string, std::string>& req,
    LastPlacementContext& ctx,
    std::string& outMsg)
{
    NXOpen::Part* workPart = session->Parts()->Work();
    if (!workPart)
    {
        outMsg = "当前没有打开的工作部件。";
        return false;
    }

    const int ch = std::clamp(GetInt(req, "gdt_char", 1), 0, 13);
    const int zs = std::clamp(GetInt(req, "gdt_zone", 0), 0, 3);
    const int mm = std::clamp(GetInt(req, "gdt_mat", 0), 0, 2);
    const int gbGrade = std::clamp(GetInt(req, "gdt_gb1184", 0), 0, 3);
    const int d1 = std::clamp(GetInt(req, "gdt_d1", 0), 0, 5);
    const int d2 = std::clamp(GetInt(req, "gdt_d2", 0), 0, 5);
    const int d3 = std::clamp(GetInt(req, "gdt_d3", 0), 0, 5);
    double val = GetDouble(req, "gdt_val", 0.1);
    double textHeight = GetDouble(req, "gdt_height", 3.5);
    if (textHeight < 0.5) textHeight = 0.5;
    const int frameColorId = std::clamp(GetInt(req, "gdt_frame_color", 0), 0, 216);
    const int textColorId = std::clamp(GetInt(req, "gdt_text_color", 0), 0, 216);

    NXOpen::TaggedObject* targetObj = ctx.targetTag != NULL_TAG ? NXOpen::NXObjectManager::Get(ctx.targetTag) : nullptr;
    if (gbGrade > 0)
    {
        const double refLen = targetObj ? MeasureObjectLengthOrSize(targetObj) : 50.0;
        val = LookupGb1184Tolerance(ch, gbGrade - 1, refLen);
    }

    const NXOpen::Session::UndoMarkId markId =
        session->SetUndoMark(NXOpen::Session::MarkVisibilityVisible, "GonCaBiaoZu Create FCF");

    NXOpen::Annotations::DraftingFeatureControlFrameBuilder* builder = nullptr;
    try
    {
        builder = workPart->Annotations()->CreateDraftingFeatureControlFrameBuilder(nullptr);
        static const NXOpen::Annotations::FeatureControlFrameBuilder::FcfCharacteristic kCharEnums[14] = {
            NXOpen::Annotations::FeatureControlFrameBuilder::FcfCharacteristicStraightness,
            NXOpen::Annotations::FeatureControlFrameBuilder::FcfCharacteristicFlatness,
            NXOpen::Annotations::FeatureControlFrameBuilder::FcfCharacteristicCircularity,
            NXOpen::Annotations::FeatureControlFrameBuilder::FcfCharacteristicCylindricity,
            NXOpen::Annotations::FeatureControlFrameBuilder::FcfCharacteristicProfileOfALine,
            NXOpen::Annotations::FeatureControlFrameBuilder::FcfCharacteristicProfileOfASurface,
            NXOpen::Annotations::FeatureControlFrameBuilder::FcfCharacteristicAngularity,
            NXOpen::Annotations::FeatureControlFrameBuilder::FcfCharacteristicPerpendicularity,
            NXOpen::Annotations::FeatureControlFrameBuilder::FcfCharacteristicParallelism,
            NXOpen::Annotations::FeatureControlFrameBuilder::FcfCharacteristicPosition,
            NXOpen::Annotations::FeatureControlFrameBuilder::FcfCharacteristicConcentricity,
            NXOpen::Annotations::FeatureControlFrameBuilder::FcfCharacteristicSymmetry,
            NXOpen::Annotations::FeatureControlFrameBuilder::FcfCharacteristicCircularRunout,
            NXOpen::Annotations::FeatureControlFrameBuilder::FcfCharacteristicTotalRunout
        };
        builder->SetCharacteristic(kCharEnums[ch]);
        builder->SetFrameStyle(NXOpen::Annotations::FeatureControlFrameBuilder::FcfFrameStyleSingleFrame);

        if (NXOpen::Annotations::StyleBuilder* style = builder->Style())
        {
            if (NXOpen::Annotations::LetteringStyleBuilder* lettering = style->LetteringStyle())
            {
                lettering->SetGeneralTextSize(textHeight);
                if (textColorId > 0)
                {
                    if (NXOpen::NXColor* txtClr = workPart->Colors()->Find(textColorId))
                    {
                        lettering->SetGeneralTextColor(txtClr);
                    }
                }
            }
            if (frameColorId > 0)
            {
                if (NXOpen::NXColor* frmClr = workPart->Colors()->Find(frameColorId))
                {
                    if (NXOpen::Annotations::SymbolStyleBuilder* sym = style->SymbolStyle())
                    {
                        sym->SetGdtSymbolColor(frmClr);
                    }
                    if (NXOpen::Annotations::LineArrowStyleBuilder* lineArrow = style->LineArrowStyle())
                    {
                        lineArrow->SetFirstArrowLineColor(frmClr);
                        lineArrow->SetFirstArrowheadColor(frmClr);
                        lineArrow->SetFirstExtensionLineColor(frmClr);
                    }
                }
            }
        }

        if (NXOpen::Annotations::FeatureControlFrameDataBuilder* frame =
                builder->FeatureControlFrameDataList()->FindItem(0))
        {
            static const NXOpen::Annotations::FeatureControlFrameDataBuilder::ToleranceZoneShape kZoneEnums[4] = {
                NXOpen::Annotations::FeatureControlFrameDataBuilder::ToleranceZoneShapeNone,
                NXOpen::Annotations::FeatureControlFrameDataBuilder::ToleranceZoneShapeDiameter,
                NXOpen::Annotations::FeatureControlFrameDataBuilder::ToleranceZoneShapeSphericalDiameter,
                NXOpen::Annotations::FeatureControlFrameDataBuilder::ToleranceZoneShapeSquare
            };
            static const NXOpen::Annotations::FeatureControlFrameDataBuilder::ToleranceMaterialModifier kMatEnums[3] = {
                NXOpen::Annotations::FeatureControlFrameDataBuilder::ToleranceMaterialModifierNone,
                NXOpen::Annotations::FeatureControlFrameDataBuilder::ToleranceMaterialModifierMaximumMaterialCondition,
                NXOpen::Annotations::FeatureControlFrameDataBuilder::ToleranceMaterialModifierLeastMaterialCondition
            };
            frame->SetZoneShape(kZoneEnums[zs]);
            frame->SetToleranceValue(U8(FormatUnsignedTol(val)));
            frame->SetMaterialModifier(kMatEnums[mm]);
            frame->PrimaryDatumReference()->SetLetter(U8(DatumEnumToString(d1)));
            frame->SecondaryDatumReference()->SetLetter(U8(DatumEnumToString(d2)));
            frame->TertiaryDatumReference()->SetLetter(U8(DatumEnumToString(d3)));
        }

        builder->Origin()->SetAnchor(NXOpen::Annotations::OriginBuilder::AlignmentPositionMidLeft);
        builder->Origin()->Plane()->SetPlaneMethod(NXOpen::Annotations::PlaneBuilder::PlaneMethodTypeXyPlane);
        builder->Origin()->SetInferRelativeToGeometry(true);

        NXOpen::DisplayableObject* leaderTarget = nullptr;
        NXOpen::View* leaderView = nullptr;
        if (targetObj && ResolveLeaderAttachmentTarget(targetObj, leaderTarget, leaderView))
        {
            NXOpen::Annotations::LeaderData* leaderData = workPart->Annotations()->CreateLeaderData();
            builder->Leader()->Leaders()->Append(leaderData);
            leaderData->SetStubSide(NXOpen::Annotations::LeaderSideInferred);
            leaderData->SetArrowhead(NXOpen::Annotations::LeaderData::ArrowheadTypeFilledArrow);
            NXOpen::View* nullView = nullptr;
            leaderData->Leader()->SetValue(
                NXOpen::InferSnapType::SnapTypeExist,
                leaderTarget,
                leaderView,
                ctx.pickPt,
                nullptr,
                nullView,
                ctx.pickPt);
        }

        NXOpen::View* nullView = nullptr;
        builder->Origin()->Origin()->SetValue(nullptr, nullView, ctx.placePt);
        NXOpen::NXObject* committed = builder->Commit();
        builder->Destroy();
        builder = nullptr;

        session->UpdateManager()->DoUpdate(markId);
        ctx.createdTag = committed ? committed->Tag() : NULL_TAG;
        WriteLog("CreateGdtAtContext ok tag=" + std::to_string(ctx.createdTag) +
            " place=(" + std::to_string(ctx.placePt.X) + "," + std::to_string(ctx.placePt.Y) + ")");
        outMsg = "形位公差已生成！可在面板改颜色/字高后点【刷新刚标公差】即时调整。";
        return true;
    }
    catch (const NXOpen::NXException& ex)
    {
        if (builder) { try { builder->Destroy(); } catch (...) {} }
        WriteLog(std::string("CreateGdtAtContext NXException: ") + (ex.Message() ? ex.Message() : ""));
        outMsg = "形位公差创建失败，请检查所选边线与放置点。";
        return false;
    }
}

bool CreateDatumAtContext(
    NXOpen::Session* session,
    const std::map<std::string, std::string>& req,
    LastPlacementContext& ctx,
    std::string& outMsg)
{
    NXOpen::Part* workPart = session->Parts()->Work();
    if (!workPart)
    {
        outMsg = "当前没有打开的工作部件。";
        return false;
    }

    std::string letter = GetStr(req, "datum_letter", "A");
    if (letter.empty()) letter = "A";
    double textHeight = GetDouble(req, "datum_height", 3.5);
    if (textHeight < 0.5) textHeight = 0.5;
    const int frameColorId = std::clamp(GetInt(req, "datum_frame_color", 0), 0, 216);
    const int textColorId = std::clamp(GetInt(req, "datum_text_color", 0), 0, 216);

    NXOpen::TaggedObject* targetObj = ctx.targetTag != NULL_TAG ? NXOpen::NXObjectManager::Get(ctx.targetTag) : nullptr;

    const NXOpen::Session::UndoMarkId markId =
        session->SetUndoMark(NXOpen::Session::MarkVisibilityVisible, "GonCaBiaoZu Create Datum");

    NXOpen::Annotations::DraftingDatumFeatureSymbolBuilder* builder = nullptr;
    try
    {
        builder = workPart->Annotations()->Datums()->CreateDraftingDatumFeatureSymbolBuilder(nullptr);
        builder->SetLetter(U8(letter));

        if (NXOpen::Annotations::StyleBuilder* style = builder->Style())
        {
            if (NXOpen::Annotations::LetteringStyleBuilder* lettering = style->LetteringStyle())
            {
                lettering->SetGeneralTextSize(textHeight);
                if (textColorId > 0)
                {
                    if (NXOpen::NXColor* txtClr = workPart->Colors()->Find(textColorId))
                    {
                        lettering->SetGeneralTextColor(txtClr);
                    }
                }
            }
            if (frameColorId > 0)
            {
                if (NXOpen::NXColor* frmClr = workPart->Colors()->Find(frameColorId))
                {
                    if (NXOpen::Annotations::SymbolStyleBuilder* sym = style->SymbolStyle())
                    {
                        sym->SetGdtSymbolColor(frmClr);
                    }
                    if (NXOpen::Annotations::LineArrowStyleBuilder* lineArrow = style->LineArrowStyle())
                    {
                        lineArrow->SetFirstArrowLineColor(frmClr);
                        lineArrow->SetFirstArrowheadColor(frmClr);
                        lineArrow->SetFirstExtensionLineColor(frmClr);
                    }
                }
            }
        }

        builder->Origin()->SetAnchor(NXOpen::Annotations::OriginBuilder::AlignmentPositionMidCenter);
        builder->Origin()->Plane()->SetPlaneMethod(NXOpen::Annotations::PlaneBuilder::PlaneMethodTypeXyPlane);
        builder->Origin()->SetInferRelativeToGeometry(true);

        NXOpen::DisplayableObject* leaderTarget = nullptr;
        NXOpen::View* leaderView = nullptr;
        if (targetObj && ResolveLeaderAttachmentTarget(targetObj, leaderTarget, leaderView))
        {
            NXOpen::Annotations::LeaderData* leaderData = workPart->Annotations()->CreateLeaderData();
            builder->Leader()->Leaders()->Append(leaderData);
            leaderData->SetTerminatorType(NXOpen::Annotations::LeaderData::LeaderTypeDatum);
            leaderData->SetArrowhead(NXOpen::Annotations::LeaderData::ArrowheadTypeFilledDatum);
            leaderData->SetLeaderPerpendicular(true);
            NXOpen::View* nullView = nullptr;
            leaderData->Leader()->SetValue(
                NXOpen::InferSnapType::SnapTypeExist,
                leaderTarget,
                leaderView,
                ctx.pickPt,
                nullptr,
                nullView,
                ctx.pickPt);
        }

        NXOpen::View* nullView = nullptr;
        builder->Origin()->Origin()->SetValue(nullptr, nullView, ctx.placePt);
        NXOpen::NXObject* committed = builder->Commit();
        builder->Destroy();
        builder = nullptr;

        session->UpdateManager()->DoUpdate(markId);
        ctx.createdTag = committed ? committed->Tag() : NULL_TAG;
        WriteLog("CreateDatumAtContext ok tag=" + std::to_string(ctx.createdTag) +
            " place=(" + std::to_string(ctx.placePt.X) + "," + std::to_string(ctx.placePt.Y) + ")");
        outMsg = "基准 [" + letter + "] 已生成！可在面板改字高/颜色后点【刷新刚标基准】即时调整。";
        return true;
    }
    catch (const NXOpen::NXException& ex)
    {
        if (builder) { try { builder->Destroy(); } catch (...) {} }
        WriteLog(std::string("CreateDatumAtContext NXException: ") + (ex.Message() ? ex.Message() : ""));
        outMsg = "基准符号创建失败，请检查所选边线与放置点。";
        return false;
    }
}

bool PickEdgeAndScreenPoint(
    const char* edgeCue,
    const char* edgeTitle,
    const char* posCue,
    LastPlacementContext& outCtx)
{
    int oldCursorView = 0;
    UF_UI_ask_cursor_view(&oldCursorView);

    outCtx.targetTag = NULL_TAG;
    outCtx.pickPt = NXOpen::Point3d(0.0, 0.0, 0.0);

    // Step 1: 选边（uf_ui.h 规定：0 = Any view，允许选中工程图视图内的边线与尺寸；中键/取消可跳过直接放点）
    UF_UI_set_cursor_view(0);
    NXOpen::TaggedObject* picked = nullptr;
    NXOpen::Point3d cursor(0.0, 0.0, 0.0);
    const auto selResp = NXOpen::UI::GetUI()->SelectionManager()->SelectTaggedObject(
        U8(edgeCue),
        U8(edgeTitle),
        NXOpen::Selection::SelectionScopeWorkPart,
        NXOpen::Selection::SelectionActionClearAndEnableSpecific,
        false,
        false,
        LeaderMasks(),
        &picked,
        &cursor);

    if (selResp == NXOpen::Selection::ResponseCancel ||
        selResp == NXOpen::Selection::ResponseBack)
    {
        UF_UI_set_cursor_view(oldCursorView);
        return false;
    }

    if (selResp == NXOpen::Selection::ResponseObjectSelected ||
        selResp == NXOpen::Selection::ResponseObjectSelectedByName)
    {
        if (picked)
        {
            outCtx.targetTag = picked->Tag();
            outCtx.pickPt = cursor;
        }
    }

    // Step 2: 自动跳到放点（uf_ui.h 规定：1 = Work view，保证 screenPos 始终是图纸坐标而不是 3D 模型坐标）
    UF_UI_set_cursor_view(1);
    double screenPos[3]{0.0, 0.0, 0.0};
    tag_t viewTag = NULL_TAG;
    int posResp = UF_UI_CANCEL;
    UF_UI_specify_screen_position(const_cast<char*>(posCue), nullptr, nullptr, screenPos, &viewTag, &posResp);

    UF_UI_set_cursor_view(oldCursorView);

    WriteLog("PickEdgeAndScreenPoint target=" + std::to_string(outCtx.targetTag) +
        " posResp=" + std::to_string(posResp) +
        " screen=(" + std::to_string(screenPos[0]) + "," + std::to_string(screenPos[1]) + ")");

    if (posResp != UF_UI_PICK_RESPONSE)
    {
        return false;
    }

    outCtx.placePt = NXOpen::Point3d(screenPos[0], screenPos[1], 0.0);
    return true;
}

struct WpfMonitorState
{
    bool active = false;
    bool processing = false;
    bool cancelRequested = false;
    UINT_PTR timerId = 0;
    PROCESS_INFORMATION pi{};
    HANDLE hReqEvent = nullptr;
    HANDLE hResEvent = nullptr;
    HANDLE hCancelEvent = nullptr;
    HWND nxHwnd = nullptr;
    std::filesystem::path ipcDir;
    std::wstring reqPath;
    std::wstring resPath;
    LastPlacementContext lastGdt{};
    LastPlacementContext lastDatum{};
};

WpfMonitorState g_monitor{};

void CleanupMonitorInternal()
{
    if (g_monitor.timerId != 0)
    {
        KillTimer(nullptr, g_monitor.timerId);
        g_monitor.timerId = 0;
    }
    if (g_monitor.pi.hThread)
    {
        CloseHandle(g_monitor.pi.hThread);
        g_monitor.pi.hThread = nullptr;
    }
    if (g_monitor.pi.hProcess)
    {
        CloseHandle(g_monitor.pi.hProcess);
        g_monitor.pi.hProcess = nullptr;
    }
    if (g_monitor.hReqEvent)
    {
        CloseHandle(g_monitor.hReqEvent);
        g_monitor.hReqEvent = nullptr;
    }
    if (g_monitor.hResEvent)
    {
        CloseHandle(g_monitor.hResEvent);
        g_monitor.hResEvent = nullptr;
    }
    if (g_monitor.hCancelEvent)
    {
        CloseHandle(g_monitor.hCancelEvent);
        g_monitor.hCancelEvent = nullptr;
    }
    if (!g_monitor.ipcDir.empty())
    {
        std::error_code ec;
        std::filesystem::remove_all(g_monitor.ipcDir, ec);
        g_monitor.ipcDir.clear();
    }
    g_monitor.reqPath.clear();
    g_monitor.resPath.clear();
    g_monitor.lastGdt = {};
    g_monitor.lastDatum = {};
    g_monitor.active = false;
    g_monitor.processing = false;
}

void HandleWpfRequestOnNxThread()
{
    g_monitor.cancelRequested = false;
    const auto req = ReadKeyValues(g_monitor.reqPath);
    std::map<std::string, std::string> res;
    res["ok"] = "0";
    const std::string action = GetStr(req, "action");
    WriteLog("Timer handling action=" + action);

    if (action == "open_config")
    {
        EnsureDefaultConfigIni();
        ShellExecuteW(nullptr, L"open", L"notepad.exe", ConfigIniPath().c_str(), nullptr, SW_SHOWNORMAL);
        res["ok"] = "1";
        res["message"] = "已打开 GonCaBiaoZu_Presets.ini。";
        WriteKeyValues(g_monitor.resPath, res);
        SetEvent(g_monitor.hResEvent);
        return;
    }

    if (UF_initialize() != 0)
    {
        WriteLog("UF_initialize failed in timer.");
        res["message"] = "NX 环境初始化失败。";
        WriteKeyValues(g_monitor.resPath, res);
        SetEvent(g_monitor.hResEvent);
        return;
    }

    const int lockStatus = UF_UI_lock_ug_access(UF_UI_FROM_CUSTOM);
    const bool locked = (lockStatus == UF_UI_LOCK_SET);
    WriteLog("UF_UI_lock_ug_access status=" + std::to_string(lockStatus));

    try
    {
        NXOpen::Session* session = NXOpen::Session::GetSession();

        if (action == "dim_continuous")
        {
            if (g_monitor.nxHwnd) SetForegroundWindow(g_monitor.nxHwnd);
            int oldCursorView = 0;
            UF_UI_ask_cursor_view(&oldCursorView);
            UF_UI_set_cursor_view(0);

            int count = 0;
            std::string lastSummary;
            while (true)
            {
                NXOpen::TaggedObject* picked = nullptr;
                NXOpen::Point3d cursor(0.0, 0.0, 0.0);
                WriteLog("dim_continuous: entering SelectTaggedObject");
                const auto selResp = NXOpen::UI::GetUI()->SelectionManager()->SelectTaggedObject(
                    U8("请连续点选尺寸赋公差（中键/ESC 结束）"),
                    U8("连续点选尺寸"),
                    NXOpen::Selection::SelectionScopeWorkPart,
                    NXOpen::Selection::SelectionActionClearAndEnableSpecific,
                    false, false, DimMasks(), &picked, &cursor);
                WriteLog("dim_continuous: SelectTaggedObject resp=" + std::to_string(static_cast<int>(selResp)));
                if (selResp != NXOpen::Selection::ResponseObjectSelected &&
                    selResp != NXOpen::Selection::ResponseObjectSelectedByName)
                {
                    break;
                }
                if (auto* dim = dynamic_cast<NXOpen::Annotations::Dimension*>(picked))
                {
                    // 每次点选尺寸前实时读取 WPF 最新公差参数，支持在连续点选期间随时切换公差值立即生效
                    const auto liveReq = ReadKeyValues((g_monitor.ipcDir / L"live_dim.ini").wstring());
                    const auto& activeReq = liveReq.empty() ? req : liveReq;

                    const auto mark = session->SetUndoMark(
                        NXOpen::Session::MarkVisibilityVisible, "GonCaBiaoZu Dimension Tolerance");
                    if (ApplyToleranceToDimension(dim, activeReq, lastSummary))
                    {
                        const int updErrs = session->UpdateManager()->DoUpdate(mark);
                        dim->RedisplayObject();
                        UF_DISP_regenerate_display();
                        UF_DISP_refresh();
                        ++count;
                        WriteLog("dim_continuous: updated count=" + std::to_string(count) +
                            " summary=" + lastSummary +
                            " updErrs=" + std::to_string(updErrs) +
                            " tolType=" + std::to_string(static_cast<int>(dim->ToleranceType())) +
                            " upPri=" + std::to_string(dim->UpperToleranceValue()) +
                            " upMet=" + std::to_string(dim->UpperMetricToleranceValue()));
                    }
                }
            }

            UF_UI_set_cursor_view(oldCursorView);
            res["ok"] = count > 0 ? "1" : "0";
            res["message"] = count > 0
                ? ("已连续更新 " + std::to_string(count) + " 个尺寸公差 (" + lastSummary + ")。")
                : "已结束连续点选。";
        }
        else if (action == "dim_batch")
        {
            if (g_monitor.nxHwnd) SetForegroundWindow(g_monitor.nxHwnd);
            int oldCursorView = 0;
            UF_UI_ask_cursor_view(&oldCursorView);
            UF_UI_set_cursor_view(0);

            std::vector<NXOpen::TaggedObject*> objs;
            const auto selResp = NXOpen::UI::GetUI()->SelectionManager()->SelectTaggedObjects(
                U8("请框选或点选多个尺寸后按确定"),
                U8("批量选择尺寸"),
                NXOpen::Selection::SelectionScopeWorkPart,
                NXOpen::Selection::SelectionActionClearAndEnableSpecific,
                false, false, DimMasks(), objs);
            UF_UI_set_cursor_view(oldCursorView);

            if ((selResp == NXOpen::Selection::ResponseOk || selResp == NXOpen::Selection::ResponseObjectSelected) && !objs.empty())
            {
                const auto mark = session->SetUndoMark(
                    NXOpen::Session::MarkVisibilityVisible, "GonCaBiaoZu Batch Dimension Tolerance");
                int count = 0;
                std::string lastSummary;
                for (auto* obj : objs)
                {
                    if (auto* dim = dynamic_cast<NXOpen::Annotations::Dimension*>(obj))
                    {
                        if (ApplyToleranceToDimension(dim, req, lastSummary))
                        {
                            dim->RedisplayObject();
                            ++count;
                        }
                    }
                }
                session->UpdateManager()->DoUpdate(mark);
                UF_DISP_regenerate_display();
                UF_DISP_refresh();
                res["ok"] = count > 0 ? "1" : "0";
                res["message"] = "已批量更新 " + std::to_string(count) + " 个尺寸公差 (" + lastSummary + ")。";
            }
            else
            {
                res["message"] = "已取消批量选择。";
            }
        }
        else if (action == "gdt_place")
        {
            if (g_monitor.nxHwnd) SetForegroundWindow(g_monitor.nxHwnd);
            int count = 0;
            std::string lastMsg;
            while (!g_monitor.cancelRequested)
            {
                LastPlacementContext ctx{};
                if (!PickEdgeAndScreenPoint(
                        "1. 请选形位公差指向的边线/尺寸（中键跳过无引线，ESC结束）",
                        "选择形位公差目标",
                        "2. 请点击放置形位公差框的位置",
                        ctx))
                {
                    break;
                }
                const auto liveReq = ReadKeyValues((g_monitor.ipcDir / L"live_gdt.ini").wstring());
                const auto& activeReq = liveReq.empty() ? req : liveReq;
                if (CreateGdtAtContext(session, activeReq, ctx, lastMsg))
                {
                    g_monitor.lastGdt = ctx;
                    UF_DISP_regenerate_display();
                    UF_DISP_refresh();
                    ++count;
                }
            }
            res["ok"] = count > 0 ? "1" : "0";
            res["message"] = count > 0
                ? ("已连续标注 " + std::to_string(count) + " 个形位公差。")
                : "已退出形位公差连续标注。";
        }
        else if (action == "gdt_update_last")
        {
            if (g_monitor.lastGdt.createdTag != NULL_TAG)
            {
                DeleteTagSafely(session, g_monitor.lastGdt.createdTag);
                std::string msg;
                if (CreateGdtAtContext(session, req, g_monitor.lastGdt, msg))
                {
                    UF_DISP_regenerate_display();
                    UF_DISP_refresh();
                    res["ok"] = "1";
                    res["message"] = "已实时刷新刚标注的形位公差样式与参数！";
                }
                else
                {
                    res["message"] = msg;
                }
            }
            else
            {
                res["message"] = "当前没有可刷新的形位公差，请先点击【选边放点标注】。";
            }
        }
        else if (action == "datum_place")
        {
            if (g_monitor.nxHwnd) SetForegroundWindow(g_monitor.nxHwnd);
            int count = 0;
            std::string lastLetter;
            std::string nextLetterOut;
            while (!g_monitor.cancelRequested)
            {
                LastPlacementContext ctx{};
                if (!PickEdgeAndScreenPoint(
                        "1. 请选基准吸附的边线/尺寸（中键跳过无引线，ESC结束）",
                        "选择基准边线",
                        "2. 请点击放置基准符号的位置",
                        ctx))
                {
                    break;
                }
                auto liveReq = ReadKeyValues((g_monitor.ipcDir / L"live_datum.ini").wstring());
                if (liveReq.empty()) liveReq = req;
                std::string msg;
                if (CreateDatumAtContext(session, liveReq, ctx, msg))
                {
                    g_monitor.lastDatum = ctx;
                    UF_DISP_regenerate_display();
                    UF_DISP_refresh();
                    ++count;
                    lastLetter = GetStr(liveReq, "datum_letter", "A");
                    if (GetInt(liveReq, "datum_auto_inc", 1) != 0)
                    {
                        nextLetterOut = NextDatumLetter(lastLetter);
                        liveReq["datum_letter"] = nextLetterOut;
                        WriteKeyValues((g_monitor.ipcDir / L"live_datum.ini").wstring(), liveReq);
                    }
                }
            }
            res["ok"] = count > 0 ? "1" : "0";
            if (!nextLetterOut.empty()) res["next_letter"] = nextLetterOut;
            res["message"] = count > 0
                ? ("已连续标注 " + std::to_string(count) + " 个基准符号（最新：" + lastLetter + "）。")
                : "已退出基准符号连续标注。";
        }
        else if (action == "datum_update_last")
        {
            if (g_monitor.lastDatum.createdTag != NULL_TAG)
            {
                DeleteTagSafely(session, g_monitor.lastDatum.createdTag);
                std::string msg;
                if (CreateDatumAtContext(session, req, g_monitor.lastDatum, msg))
                {
                    UF_DISP_regenerate_display();
                    UF_DISP_refresh();
                    res["ok"] = "1";
                    res["message"] = "已实时刷新刚标注的基准符号样式与字母！";
                }
                else
                {
                    res["message"] = msg;
                }
            }
            else
            {
                res["message"] = "当前没有可刷新的基准符号，请先点击【选边放点标注】。";
            }
        }
    }
    catch (const NXOpen::NXException& ex)
    {
        WriteLog(std::string("Action NXException: ") + (ex.Message() ? ex.Message() : ""));
        res["message"] = "执行 NX 操作时出现异常，请查看日志。";
    }
    catch (const std::exception& ex)
    {
        WriteLog(std::string("Action std::exception: ") + ex.what());
        res["message"] = "执行操作时出现异常，请查看日志。";
    }
    catch (...)
    {
        WriteLog("Action unknown exception.");
        res["message"] = "执行操作时发生未知错误。";
    }

    if (locked)
    {
        UF_UI_unlock_ug_access(UF_UI_FROM_CUSTOM);
    }
    UF_terminate();

    WriteKeyValues(g_monitor.resPath, res);
    SetEvent(g_monitor.hResEvent);
}

VOID CALLBACK MonitorTimerProc(HWND, UINT, UINT_PTR, DWORD)
{
    if (!g_monitor.active) return;

    // 当正在执行选尺寸/选边等模态交互时，允许通过 CancelEvent（切换页签/关闭窗口）自动取消当前 NX 选择对话框
    if (g_monitor.processing)
    {
        const bool uiExited = g_monitor.pi.hProcess &&
            WaitForSingleObject(g_monitor.pi.hProcess, 0) == WAIT_OBJECT_0;
        const bool cancelSignaled = g_monitor.hCancelEvent &&
            WaitForSingleObject(g_monitor.hCancelEvent, 0) == WAIT_OBJECT_0;
        if (uiExited || cancelSignaled)
        {
            g_monitor.cancelRequested = true;
            WriteLog(uiExited
                ? "WPF UI exited during active pick; calling UF_UI_cancel_uf_dialog."
                : "Cancel event received during active pick; calling UF_UI_cancel_uf_dialog.");
            UF_UI_cancel_uf_dialog(UF_UI_FROM_CUSTOM);
        }
        return;
    }

    g_monitor.processing = true;

    bool shouldStop = false;
    if (g_monitor.pi.hProcess && WaitForSingleObject(g_monitor.pi.hProcess, 0) == WAIT_OBJECT_0)
    {
        WriteLog("WPF UI process exited; stopping non-modal monitor.");
        shouldStop = true;
    }
    else if (g_monitor.hReqEvent && WaitForSingleObject(g_monitor.hReqEvent, 0) == WAIT_OBJECT_0)
    {
        HandleWpfRequestOnNxThread();
    }

    g_monitor.processing = false;
    if (shouldStop)
    {
        CleanupMonitorInternal();
    }
}

} // namespace

bool IsWpfToleranceSessionActive()
{
    return g_monitor.active;
}

void StopWpfToleranceSession()
{
    if (g_monitor.pi.hProcess && WaitForSingleObject(g_monitor.pi.hProcess, 0) == WAIT_TIMEOUT)
    {
        TerminateProcess(g_monitor.pi.hProcess, 0);
    }
    CleanupMonitorInternal();
}

bool RunWpfToleranceSession()
{
    zhihui_context_help::EnsureGlobalHelpLoaded();
    EnsureDefaultConfigIni();

    const std::filesystem::path exePath = CurrentModuleDir() / L"GonCaBiaoZuUI" / L"GonCaBiaoZuUI.exe";
    if (!std::filesystem::exists(exePath))
    {
        WriteLog("GonCaBiaoZuUI.exe not found, falling back to Block Styler.");
        return false;
    }

    if (g_monitor.active && g_monitor.pi.hProcess)
    {
        if (WaitForSingleObject(g_monitor.pi.hProcess, 0) == WAIT_TIMEOUT)
        {
            WriteLog("WPF UI already running; keeping existing non-modal panel.");
            return true;
        }
        CleanupMonitorInternal();
    }

    wchar_t tempBuf[MAX_PATH]{};
    GetTempPathW(MAX_PATH, tempBuf);
    const unsigned long pid = GetCurrentProcessId();
    const unsigned long tick = GetTickCount();
    wchar_t sessionIdW[64]{};
    std::swprintf(sessionIdW, 64, L"%lu_%lu", pid, tick);

    std::filesystem::path ipcDir = std::filesystem::path(tempBuf) / (std::wstring(L"Zhihui_GCBZ_") + sessionIdW);
    CreateDirectoryW(ipcDir.c_str(), nullptr);

    const std::wstring reqEventName = std::wstring(L"Local\\Zhihui_GCBZ_Req_") + sessionIdW;
    const std::wstring resEventName = std::wstring(L"Local\\Zhihui_GCBZ_Res_") + sessionIdW;
    const std::wstring cancelEventName = std::wstring(L"Local\\Zhihui_GCBZ_Cancel_") + sessionIdW;
    HANDLE hReqEvent = CreateEventW(nullptr, FALSE, FALSE, reqEventName.c_str());
    HANDLE hResEvent = CreateEventW(nullptr, FALSE, FALSE, resEventName.c_str());
    HANDLE hCancelEvent = CreateEventW(nullptr, FALSE, FALSE, cancelEventName.c_str());
    if (!hReqEvent || !hResEvent || !hCancelEvent)
    {
        if (hReqEvent) CloseHandle(hReqEvent);
        if (hResEvent) CloseHandle(hResEvent);
        if (hCancelEvent) CloseHandle(hCancelEvent);
        return false;
    }

    HWND nxHwnd = reinterpret_cast<HWND>(UF_UI_get_default_parent());
    wchar_t cmdLine[2048]{};
    std::swprintf(cmdLine, 2048,
        L"\"%s\" --session %s --dir \"%s\" --nx-hwnd %lld",
        exePath.c_str(), sessionIdW, ipcDir.c_str(),
        static_cast<long long>(reinterpret_cast<intptr_t>(nxHwnd)));

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    const BOOL ok = CreateProcessW(
        exePath.c_str(), cmdLine, nullptr, nullptr, FALSE, 0, nullptr,
        exePath.parent_path().c_str(), &si, &pi);

    if (!ok)
    {
        WriteLog("CreateProcessW failed for GonCaBiaoZuUI.exe");
        CloseHandle(hReqEvent);
        CloseHandle(hResEvent);
        return false;
    }

    const UINT_PTR timerId = SetTimer(nullptr, 0, 80, MonitorTimerProc);
    if (timerId == 0)
    {
        WriteLog("SetTimer failed for non-modal monitor.");
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        CloseHandle(hReqEvent);
        CloseHandle(hResEvent);
        return false;
    }

    g_monitor.active = true;
    g_monitor.processing = false;
    g_monitor.timerId = timerId;
    g_monitor.pi = pi;
    g_monitor.hReqEvent = hReqEvent;
    g_monitor.hResEvent = hResEvent;
    g_monitor.hCancelEvent = hCancelEvent;
    g_monitor.nxHwnd = nxHwnd;
    g_monitor.ipcDir = ipcDir;
    g_monitor.reqPath = (ipcDir / L"request.ini").wstring();
    g_monitor.resPath = (ipcDir / L"response.ini").wstring();

    WriteLog("Non-modal WPF UI started and ufusr returning immediately.");
    return true;
}

} // namespace zhihui::gcbz
