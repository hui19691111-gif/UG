#include "GonCaBiaoZu.hpp"
#include "GonCaBiaoZuTables.hpp"
#include "../../common/ZhihuiContextHelp.hpp"
#include "../../common/ZhihuiDialogMemory.hpp"

#ifdef CreateDialog
#undef CreateDialog
#endif

#include <NXOpen/Annotations.hxx>
#include <NXOpen/Annotations_Annotation.hxx>
#include <NXOpen/Annotations_AnnotationManager.hxx>
#include <NXOpen/Annotations_AppendedText.hxx>
#include <NXOpen/Annotations_DatumFeatureSymbolBuilder.hxx>
#include <NXOpen/Annotations_DatumReferenceBuilder.hxx>
#include <NXOpen/Annotations_Dimension.hxx>
#include <NXOpen/Annotations_DimensionPreferences.hxx>
#include <NXOpen/Annotations_DraftingDatum.hxx>
#include <NXOpen/Annotations_DraftingDatumFeatureSymbolBuilder.hxx>
#include <NXOpen/Annotations_DraftingFeatureControlFrameBuilder.hxx>
#include <NXOpen/Annotations_FeatureControlFrameBuilder.hxx>
#include <NXOpen/Annotations_FeatureControlFrameDataBuilder.hxx>
#include <NXOpen/Annotations_FeatureControlFrameDataBuilderList.hxx>
#include <NXOpen/Annotations_Gdt.hxx>
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
#include <NXOpen/BlockStyler_PropertyList.hxx>
#include <NXOpen/BlockStyler_SelectObject.hxx>
#include <NXOpen/BlockStyler_SpecifyPoint.hxx>
#include <NXOpen/BlockStyler_UIBlock.hxx>
#include <NXOpen/ColorManager.hxx>
#include <NXOpen/Dimensions.hxx>
#include <NXOpen/DisplayableObject.hxx>
#include <NXOpen/Drawings_BaseView.hxx>
#include <NXOpen/Drawings_DraftingCurve.hxx>
#include <NXOpen/Drawings_DraftingCurveInfo.hxx>
#include <NXOpen/Drawings_DraftingView.hxx>
#include <NXOpen/Edge.hxx>
#include <NXOpen/Face.hxx>
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
#include <uf_curve.h>
#include <uf_defs.h>
#include <uf_eval.h>
#include <uf_obj.h>
#include <uf_object_types.h>
#include <uf_ui.h>
#include <uf_view.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <iomanip>
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

namespace
{
using namespace zhihui::gcbz;

constexpr const wchar_t* kMemoryFile = L"GonCaBiaoZu_State.ini";

NXOpen::NXString U8(const std::string& utf8)
{
    return NXOpen::NXString(utf8.c_str(), NXOpen::NXString::UTF8);
}

NXOpen::NXString U8(const char* utf8)
{
    return NXOpen::NXString(utf8 ? utf8 : "", NXOpen::NXString::UTF8);
}

std::string DlxPath()
{
    wchar_t module[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(
        reinterpret_cast<HMODULE>(&__ImageBase), module, MAX_PATH);
    if (length > 0 && length < MAX_PATH)
    {
        std::filesystem::path dir = std::filesystem::path(module).parent_path();
        std::filesystem::path p1 = dir / L"GonCaBiaoZu.dlx";
        if (std::filesystem::exists(p1))
        {
            const std::wstring w = p1.wstring();
            const int sz = WideCharToMultiByte(CP_ACP, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
            if (sz > 1)
            {
                std::string ansi(static_cast<std::size_t>(sz - 1), '\0');
                WideCharToMultiByte(CP_ACP, 0, w.c_str(), -1, &ansi[0], sz, nullptr, nullptr);
                return ansi;
            }
        }
    }
    return "GonCaBiaoZu.dlx";
}

void WriteDebugLog(const std::string& line)
{
    CreateDirectoryW(L"D:\\UG\u667A\u8F89\u94A3\u91D1\u63D2\u4EF6", nullptr);
    CreateDirectoryW(L"D:\\UG\u667A\u8F89\u94A3\u91D1\u63D2\u4EF6\\logs", nullptr);
    FILE* fp = _wfopen(L"D:\\UG\u667A\u8F89\u94A3\u91D1\u63D2\u4EF6\\logs\\GonCaBiaoZu_debug.log", L"ab");
    if (!fp) return;
    SYSTEMTIME st{};
    GetLocalTime(&st);
    char prefix[64]{};
    std::snprintf(prefix, sizeof(prefix), "[%04d-%02d-%02d %02d:%02d:%02d.%03d] ",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    std::fwrite(prefix, 1, std::strlen(prefix), fp);
    std::fwrite(line.data(), 1, line.size(), fp);
    std::fwrite("\r\n", 1, 2, fp);
    std::fclose(fp);
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

    // 1. Check if it is a DraftingCurve and resolve its view + parent model edge/face
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
                    if (!parent) continue;
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

    // 2. Otherwise check VDE data for view tag
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

int StringToDatumEnum(const std::string& s)
{
    if (s == "A") return 1;
    if (s == "B") return 2;
    if (s == "C") return 3;
    if (s == "D") return 4;
    if (s == "A-B") return 5;
    return 0;
}

std::string NextDatumLetter(const std::string& current)
{
    if (current.empty()) return "A";
    std::string next = current;
    char last = next.back();
    if (last >= 'A' && last < 'Z')
    {
        next.back() = static_cast<char>(last + 1);
        //の国标/ISO 基准字母通常跳过 I, O, Q
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

} // namespace

GonCaBiaoZuDialog::GonCaBiaoZuDialog()
    : session_(NXOpen::Session::GetSession())
{
    zhihui_context_help::EnsureGlobalHelpLoaded();
    EnsureDefaultConfigIni();

    dialog_ = NXOpen::UI::GetUI()->CreateDialog(DlxPath().c_str());
    dialog_->AddInitializeHandler(NXOpen::make_callback(this, &GonCaBiaoZuDialog::Initialize));
    dialog_->AddDialogShownHandler(NXOpen::make_callback(this, &GonCaBiaoZuDialog::DialogShown));
    dialog_->AddUpdateHandler(NXOpen::make_callback(this, &GonCaBiaoZuDialog::Update));
    dialog_->AddApplyHandler(NXOpen::make_callback(this, &GonCaBiaoZuDialog::Apply));
    dialog_->AddOkHandler(NXOpen::make_callback(this, &GonCaBiaoZuDialog::Ok));
    dialog_->AddCancelHandler(NXOpen::make_callback(this, &GonCaBiaoZuDialog::Cancel));
    dialog_->AddFilterHandler(NXOpen::make_callback(this, &GonCaBiaoZuDialog::Filter));
}

GonCaBiaoZuDialog::~GonCaBiaoZuDialog()
{
    DeleteGdtPreview();
    DeleteDatumPreview();
    delete dialog_;
    dialog_ = nullptr;
}

NXOpen::BlockStyler::BlockDialog::DialogResponse GonCaBiaoZuDialog::Launch()
{
    return dialog_->Launch();
}

void GonCaBiaoZuDialog::Initialize()
{
    try
    {
        auto* top = dialog_->TopBlock();
        tabBtnDim_ = top->FindBlock("tab_btn_dim");
        tabBtnGdt_ = top->FindBlock("tab_btn_gdt");
        tabBtnDatum_ = top->FindBlock("tab_btn_datum");
        tabBarDim_ = top->FindBlock("tab_bar_dim");
        tabBarGdt_ = top->FindBlock("tab_bar_gdt");
        tabBarDatum_ = top->FindBlock("tab_bar_datum");

        dimGroup_ = top->FindBlock("dim_group");
        dimExtraGroup_ = top->FindBlock("dim_extra_group");
        gdtSymGroup_ = top->FindBlock("gdt_sym_group");
        gdtPickGroup_ = top->FindBlock("gdt_pick_group");
        gdtParamGroup_ = top->FindBlock("gdt_param_group");
        gdtStyleGroup_ = top->FindBlock("gdt_style_group");
        datumGroup_ = top->FindBlock("datum_group");
        datumStyleGroup_ = top->FindBlock("datum_style_group");
        infoGroup_ = top->FindBlock("info_group");

        dimSelect_ = dynamic_cast<NXOpen::BlockStyler::SelectObject*>(top->FindBlock("dim_select"));
        dimSubmode_ = top->FindBlock("dim_submode");
        dimSheetPreset_ = top->FindBlock("dim_sheet_preset");
        dimHoleFit_ = top->FindBlock("dim_hole_fit");
        dimShaftFit_ = top->FindBlock("dim_shaft_fit");
        dimFitStyle_ = top->FindBlock("dim_fit_style");
        dimGb1804Grade_ = top->FindBlock("dim_gb1804_grade");
        dimUpperTol_ = top->FindBlock("dim_upper_tol");
        dimLowerTol_ = top->FindBlock("dim_lower_tol");
        dimPrefixMod_ = top->FindBlock("dim_prefix_mod");
        dimSuffixMod_ = top->FindBlock("dim_suffix_mod");
        dimRefFlag_ = top->FindBlock("dim_ref_flag");
        dimAutoApply_ = top->FindBlock("dim_auto_apply");

        for (int i = 0; i < 14; ++i)
        {
            char btnId[32]{};
            std::snprintf(btnId, sizeof(btnId), "gdt_btn_%02d", i);
            gdtCharBtns_[static_cast<std::size_t>(i)] = top->FindBlock(btnId);
        }
        gdtPreset_ = top->FindBlock("gdt_preset");
        gdtZoneShape_ = top->FindBlock("gdt_zone_shape");
        gdtGb1184Grade_ = top->FindBlock("gdt_gb1184_grade");
        gdtValue_ = top->FindBlock("gdt_value");
        gdtMatMod_ = top->FindBlock("gdt_mat_mod");
        gdtDatum1_ = top->FindBlock("gdt_datum1");
        gdtDatum2_ = top->FindBlock("gdt_datum2");
        gdtDatum3_ = top->FindBlock("gdt_datum3");
        gdtTextHeight_ = top->FindBlock("gdt_text_height");
        gdtFrameColor_ = top->FindBlock("gdt_frame_color");
        gdtTextColor_ = top->FindBlock("gdt_text_color");
        gdtTargetSelect_ = dynamic_cast<NXOpen::BlockStyler::SelectObject*>(top->FindBlock("gdt_target_select"));
        gdtPlacePoint_ = dynamic_cast<NXOpen::BlockStyler::SpecifyPoint*>(top->FindBlock("gdt_place_point"));

        datumLetter_ = top->FindBlock("datum_letter");
        datumAutoInc_ = top->FindBlock("datum_auto_inc");
        datumTargetSelect_ = dynamic_cast<NXOpen::BlockStyler::SelectObject*>(top->FindBlock("datum_target_select"));
        datumPlacePoint_ = dynamic_cast<NXOpen::BlockStyler::SpecifyPoint*>(top->FindBlock("datum_place_point"));
        datumTextHeight_ = top->FindBlock("datum_text_height");
        datumFrameColor_ = top->FindBlock("datum_frame_color");
        datumTextColor_ = top->FindBlock("datum_text_color");

        previewLabel_ = top->FindBlock("preview_label");
        statusLabel_ = top->FindBlock("status_label");
        openConfigBtn_ = top->FindBlock("open_config_btn");

        ConfigureSelectionFilters();
        LoadMemory();
    }
    catch (const NXOpen::NXException& ex)
    {
        Log(std::string("Initialize NXException: ") + (ex.Message() ? ex.Message() : ""));
    }
    catch (const std::exception& ex)
    {
        Log(std::string("Initialize std::exception: ") + ex.what());
    }
    catch (...)
    {
        Log("Initialize unknown exception.");
    }
}

void GonCaBiaoZuDialog::DialogShown()
{
    try
    {
        dialogShown_ = true;
        UpdateUiVisibility();
        UpdatePreviewAndStatus();

        if (activeTab_ == 0 && dimSelect_)
        {
            dimSelect_->Focus();
        }
        else if (activeTab_ == 1 && gdtTargetSelect_)
        {
            gdtTargetSelect_->Focus();
        }
        else if (activeTab_ == 2 && datumTargetSelect_)
        {
            datumTargetSelect_->Focus();
        }
    }
    catch (const NXOpen::NXException& ex)
    {
        Log(std::string("DialogShown NXException: ") + (ex.Message() ? ex.Message() : ""));
    }
    catch (const std::exception& ex)
    {
        Log(std::string("DialogShown std::exception: ") + ex.what());
    }
    catch (...)
    {
        Log("DialogShown unknown exception.");
    }
}

void GonCaBiaoZuDialog::SwitchMainTab(int newTab)
{
    newTab = std::clamp(newTab, 0, 2);
    if (activeTab_ != newTab)
    {
        DeleteGdtPreview();
        DeleteDatumPreview();
        activeTab_ = newTab;
    }
    UpdateUiVisibility();
    UpdatePreviewAndStatus();
    SaveMemory();

    if (dialogShown_)
    {
        if (activeTab_ == 0 && dimSelect_) dimSelect_->Focus();
        else if (activeTab_ == 1 && gdtTargetSelect_) gdtTargetSelect_->Focus();
        else if (activeTab_ == 2 && datumTargetSelect_) datumTargetSelect_->Focus();
    }
}

void GonCaBiaoZuDialog::ConfigureSelectionFilters()
{
    if (dimSelect_)
    {
        std::vector<NXOpen::Selection::MaskTriple> dimMasks;
        dimMasks.emplace_back(UF_dimension_type, UF_all_subtype, 0);
        dimSelect_->SetSelectionFilter(NXOpen::Selection::SelectionActionClearAndEnableSpecific, dimMasks);
    }

    std::vector<NXOpen::Selection::MaskTriple> leaderMasks;
    leaderMasks.emplace_back(UF_dimension_type, UF_all_subtype, 0);
    leaderMasks.emplace_back(UF_line_type, UF_all_subtype, 0);
    leaderMasks.emplace_back(UF_circle_type, UF_all_subtype, 0);
    leaderMasks.emplace_back(UF_conic_type, UF_all_subtype, 0);
    leaderMasks.emplace_back(UF_spline_type, UF_all_subtype, 0);
    leaderMasks.emplace_back(UF_solid_type, UF_solid_edge_subtype, 0);
    leaderMasks.emplace_back(UF_solid_type, UF_solid_face_subtype, 0);
    leaderMasks.emplace_back(UF_drafting_entity_type, UF_all_subtype, 0);

    if (gdtTargetSelect_)
    {
        gdtTargetSelect_->SetSelectionFilter(NXOpen::Selection::SelectionActionClearAndEnableSpecific, leaderMasks);
    }
    if (datumTargetSelect_)
    {
        datumTargetSelect_->SetSelectionFilter(NXOpen::Selection::SelectionActionClearAndEnableSpecific, leaderMasks);
    }
}

int GonCaBiaoZuDialog::Filter(NXOpen::BlockStyler::UIBlock* block, NXOpen::TaggedObject* object)
{
    if (!object) return UF_UI_SEL_REJECT;
    if (block == dimSelect_)
    {
        return dynamic_cast<NXOpen::Annotations::Dimension*>(object) != nullptr
            ? UF_UI_SEL_ACCEPT : UF_UI_SEL_REJECT;
    }
    return UF_UI_SEL_ACCEPT;
}

int GonCaBiaoZuDialog::Update(NXOpen::BlockStyler::UIBlock* block)
{
    if (internalUpdating_) return 0;
    try
    {
        if (block == openConfigBtn_)
        {
            EnsureDefaultConfigIni();
            const std::wstring iniPath = ConfigIniPath();
            ShellExecuteW(nullptr, L"open", L"notepad.exe", iniPath.c_str(), nullptr, SW_SHOWNORMAL);
            SetLabelText(statusLabel_, "已打开公差预设配置文件：GonCaBiaoZu_Presets.ini");
            return 0;
        }

        if (block == tabBtnDim_)
        {
            SwitchMainTab(0);
            return 0;
        }
        if (block == tabBtnGdt_)
        {
            SwitchMainTab(1);
            return 0;
        }
        if (block == tabBtnDatum_)
        {
            SwitchMainTab(2);
            return 0;
        }

        for (int i = 0; i < 14; ++i)
        {
            if (block == gdtCharBtns_[static_cast<std::size_t>(i)])
            {
                selectedGdtChar_ = i;
                internalUpdating_ = true;
                zhihui_dialog_memory::TrySetEnum(gdtPreset_, 0);
                internalUpdating_ = false;
                SyncGb1184GdtValue();
                UpdateUiVisibility();
                UpdatePreviewAndStatus();
                SaveMemory();
                if (hasGdtPointPicked_)
                {
                    CreateOrUpdateGdtPreview(false, false);
                }
                return 0;
            }
        }

        if (block == dimSubmode_)
        {
            int sub = 0;
            zhihui_dialog_memory::TryGetEnum(dimSubmode_, &sub);
            if (sub == 0) SyncFromSheetPreset();
            UpdateUiVisibility();
            UpdatePreviewAndStatus();
            SaveMemory();
            return 0;
        }

        if (block == dimSheetPreset_)
        {
            SyncFromSheetPreset();
            UpdatePreviewAndStatus();
            SaveMemory();
            return 0;
        }

        if (block == gdtPreset_)
        {
            SyncFromGdtPreset();
            UpdateUiVisibility();
            UpdatePreviewAndStatus();
            SaveMemory();
            if (hasGdtPointPicked_)
            {
                CreateOrUpdateGdtPreview(false, false);
            }
            return 0;
        }

        if (block == gdtTargetSelect_)
        {
            SyncGb1184GdtValue();
            UpdateUiVisibility();
            UpdatePreviewAndStatus();
            SaveMemory();
            if (gdtTargetSelect_ && !gdtTargetSelect_->GetSelectedObjects().empty() && gdtPlacePoint_)
            {
                gdtPlacePoint_->Focus();
                if (hasGdtPointPicked_)
                {
                    CreateOrUpdateGdtPreview(false, false);
                }
            }
            return 0;
        }

        if (block == gdtPlacePoint_)
        {
            hasGdtPointPicked_ = true;
            UpdatePreviewAndStatus();
            CreateOrUpdateGdtPreview(false, false);
            return 0;
        }

        if (block == gdtGb1184Grade_ || block == gdtZoneShape_ ||
            block == gdtValue_ || block == gdtMatMod_ || block == gdtDatum1_ ||
            block == gdtDatum2_ || block == gdtDatum3_ || block == gdtTextHeight_ ||
            block == gdtFrameColor_ || block == gdtTextColor_)
        {
            if (block == gdtGb1184Grade_)
            {
                SyncGb1184GdtValue();
            }
            UpdateUiVisibility();
            UpdatePreviewAndStatus();
            SaveMemory();
            if (hasGdtPointPicked_)
            {
                CreateOrUpdateGdtPreview(false, false);
            }
            return 0;
        }

        if (block == datumTargetSelect_)
        {
            UpdatePreviewAndStatus();
            if (datumTargetSelect_ && !datumTargetSelect_->GetSelectedObjects().empty() && datumPlacePoint_)
            {
                datumPlacePoint_->Focus();
                if (hasDatumPointPicked_)
                {
                    CreateOrUpdateDatumPreview(false, false);
                }
            }
            return 0;
        }

        if (block == datumPlacePoint_)
        {
            hasDatumPointPicked_ = true;
            UpdatePreviewAndStatus();
            CreateOrUpdateDatumPreview(false, false);
            return 0;
        }

        if (block == datumLetter_ || block == datumTextHeight_ ||
            block == datumFrameColor_ || block == datumTextColor_)
        {
            UpdatePreviewAndStatus();
            SaveMemory();
            if (hasDatumPointPicked_)
            {
                CreateOrUpdateDatumPreview(false, false);
            }
            return 0;
        }

        if (block == dimSelect_)
        {
            UpdatePreviewAndStatus();
            bool autoApply = false;
            zhihui_dialog_memory::TryGetLogical(dimAutoApply_, &autoApply);
            if (autoApply && dimSelect_ && !dimSelect_->GetSelectedObjects().empty())
            {
                ExecuteDimensionTolerance(true);
            }
            return 0;
        }

        UpdatePreviewAndStatus();
        SaveMemory();
    }
    catch (const NXOpen::NXException& ex)
    {
        Log(std::string("Update NXException: ") + (ex.Message() ? ex.Message() : ""));
    }
    catch (const std::exception& ex)
    {
        Log(std::string("Update std::exception: ") + ex.what());
    }
    catch (...)
    {
        Log("Update unknown exception.");
    }
    return 0;
}

int GonCaBiaoZuDialog::Apply()
{
    try
    {
        SaveMemory();
        if (activeTab_ == 0)
        {
            return ExecuteDimensionTolerance(true) ? 0 : 1;
        }
        if (activeTab_ == 1)
        {
            return CreateOrUpdateGdtPreview(true, true) ? 0 : 1;
        }
        if (activeTab_ == 2)
        {
            return CreateOrUpdateDatumPreview(true, true) ? 0 : 1;
        }
    }
    catch (const NXOpen::NXException& ex)
    {
        Log(std::string("Apply NXException: ") + (ex.Message() ? ex.Message() : ""));
        Notify(L"执行公差标注失败，请查看日志。", true);
        return 1;
    }
    catch (const std::exception& ex)
    {
        Log(std::string("Apply std::exception: ") + ex.what());
        Notify(L"执行公差标注失败，请查看日志。", true);
        return 1;
    }
    catch (...)
    {
        Log("Apply unknown exception.");
        Notify(L"执行公差标注时发生未知错误。", true);
        return 1;
    }
    return 0;
}

int GonCaBiaoZuDialog::Ok()
{
    try
    {
        SaveMemory();
        if (activeTab_ == 0)
        {
            if (dimSelect_ && !dimSelect_->GetSelectedObjects().empty())
            {
                return ExecuteDimensionTolerance(false) ? 0 : 1;
            }
            return 0;
        }
        if (activeTab_ == 1)
        {
            if (gdtPreviewTag_ != NULL_TAG)
            {
                gdtPreviewTag_ = NULL_TAG;
                return 0;
            }
            if (hasGdtPointPicked_ || (gdtTargetSelect_ && !gdtTargetSelect_->GetSelectedObjects().empty()))
            {
                return CreateOrUpdateGdtPreview(true, false) ? 0 : 1;
            }
            return 0;
        }
        if (activeTab_ == 2)
        {
            if (datumPreviewTag_ != NULL_TAG)
            {
                datumPreviewTag_ = NULL_TAG;
                bool autoInc = true;
                zhihui_dialog_memory::TryGetLogical(datumAutoInc_, &autoInc);
                if (autoInc)
                {
                    std::string letter = GetStringBlockValue(datumLetter_);
                    if (letter.empty()) letter = "A";
                    internalUpdating_ = true;
                    SetStringBlockValue(datumLetter_, NextDatumLetter(letter));
                    internalUpdating_ = false;
                    SaveMemory();
                }
                return 0;
            }
            if (hasDatumPointPicked_ || (datumTargetSelect_ && !datumTargetSelect_->GetSelectedObjects().empty()))
            {
                return CreateOrUpdateDatumPreview(true, false) ? 0 : 1;
            }
            return 0;
        }
    }
    catch (const NXOpen::NXException& ex)
    {
        Log(std::string("Ok NXException: ") + (ex.Message() ? ex.Message() : ""));
        return 1;
    }
    catch (...)
    {
        Log("Ok unknown exception.");
        return 1;
    }
    return 0;
}

int GonCaBiaoZuDialog::Cancel()
{
    try
    {
        DeleteGdtPreview();
        DeleteDatumPreview();
    }
    catch (...) {}
    return 0;
}

void GonCaBiaoZuDialog::SyncFromSheetPreset()
{
    int presetIdx = 0;
    if (!zhihui_dialog_memory::TryGetEnum(dimSheetPreset_, &presetIdx)) return;
    const auto presets = DefaultDimPresets();
    if (presetIdx < 0 || static_cast<std::size_t>(presetIdx) >= presets.size()) return;

    internalUpdating_ = true;
    zhihui_dialog_memory::TrySetDouble(dimUpperTol_, presets[static_cast<std::size_t>(presetIdx)].upper);
    zhihui_dialog_memory::TrySetDouble(dimLowerTol_, presets[static_cast<std::size_t>(presetIdx)].lower);
    internalUpdating_ = false;
}

void GonCaBiaoZuDialog::SyncFromGdtPreset()
{
    int presetIdx = 0;
    if (!zhihui_dialog_memory::TryGetEnum(gdtPreset_, &presetIdx)) return;
    const auto presets = DefaultGdtPresets();
    if (presetIdx <= 0 || static_cast<std::size_t>(presetIdx) >= presets.size()) return;

    const auto& p = presets[static_cast<std::size_t>(presetIdx)];
    selectedGdtChar_ = std::clamp(p.characteristic, 0, 13);
    internalUpdating_ = true;
    zhihui_dialog_memory::TrySetEnum(gdtZoneShape_, p.zoneShape);
    zhihui_dialog_memory::TrySetEnum(gdtGb1184Grade_, 0);
    zhihui_dialog_memory::TrySetDouble(gdtValue_, p.value);
    zhihui_dialog_memory::TrySetEnum(gdtMatMod_, p.materialMod);
    zhihui_dialog_memory::TrySetEnum(gdtDatum1_, StringToDatumEnum(p.datumA));
    zhihui_dialog_memory::TrySetEnum(gdtDatum2_, StringToDatumEnum(p.datumB));
    zhihui_dialog_memory::TrySetEnum(gdtDatum3_, StringToDatumEnum(p.datumC));
    internalUpdating_ = false;
}

void GonCaBiaoZuDialog::SyncGb1184GdtValue()
{
    int gbGrade = 0;
    if (!zhihui_dialog_memory::TryGetEnum(gdtGb1184Grade_, &gbGrade) || gbGrade <= 0)
    {
        return;
    }

    const int ch = std::clamp(selectedGdtChar_, 0, 13);
    double refLen = 50.0;
    if (gdtTargetSelect_)
    {
        const auto objs = gdtTargetSelect_->GetSelectedObjects();
        if (!objs.empty())
        {
            refLen = MeasureObjectLengthOrSize(objs[0]);
        }
    }

    const double val = LookupGb1184Tolerance(ch, gbGrade - 1, refLen);
    internalUpdating_ = true;
    zhihui_dialog_memory::TrySetDouble(gdtValue_, val);
    internalUpdating_ = false;
}

void GonCaBiaoZuDialog::UpdateUiVisibility()
{
    if (!dialogShown_) return;

    SetBlockShow(tabBarDim_, activeTab_ == 0);
    SetBlockShow(tabBarGdt_, activeTab_ == 1);
    SetBlockShow(tabBarDatum_, activeTab_ == 2);

    SetBlockShow(dimGroup_, activeTab_ == 0);
    SetBlockShow(dimExtraGroup_, activeTab_ == 0);

    SetBlockShow(gdtSymGroup_, activeTab_ == 1);
    SetBlockShow(gdtPickGroup_, activeTab_ == 1);
    SetBlockShow(gdtParamGroup_, activeTab_ == 1);
    SetBlockShow(gdtStyleGroup_, activeTab_ == 1);

    SetBlockShow(datumGroup_, activeTab_ == 2);
    SetBlockShow(datumStyleGroup_, activeTab_ == 2);

    if (activeTab_ == 0)
    {
        int sub = 0;
        zhihui_dialog_memory::TryGetEnum(dimSubmode_, &sub);
        // 0=钣金常用快捷, 1=孔配合, 2=轴配合, 3=装配配合, 4=GB1804, 5=手动偏差, 6=清除公差
        SetBlockShow(dimSheetPreset_, sub == 0);
        SetBlockShow(dimHoleFit_, sub == 1 || sub == 3);
        SetBlockShow(dimShaftFit_, sub == 2 || sub == 3);
        SetBlockShow(dimFitStyle_, sub == 1 || sub == 2 || sub == 3);
        SetBlockShow(dimGb1804Grade_, sub == 4);
        SetBlockShow(dimUpperTol_, sub == 0 || sub == 5);
        SetBlockShow(dimLowerTol_, sub == 0 || sub == 5);
        SetBlockEnable(dimUpperTol_, sub == 5);
        SetBlockEnable(dimLowerTol_, sub == 5);
    }
    else if (activeTab_ == 1)
    {
        int gbGrade = 0;
        zhihui_dialog_memory::TryGetEnum(gdtGb1184Grade_, &gbGrade);
        SetBlockEnable(gdtValue_, gbGrade == 0);

        static const char* kCharTitles[14] = {
            "直线度", "平面度", "圆度", "圆柱度",
            "线轮廓度", "面轮廓度", "倾斜度", "垂直度",
            "平行度", "位置度", "同轴度", "对称度",
            "圆跳动", "全跳动"
        };
        const int ch = std::clamp(selectedGdtChar_, 0, 13);
        SetLabelText(gdtSymGroup_, std::string("几何特征符号  [当前：") + kCharTitles[ch] + "]");
    }
}

void GonCaBiaoZuDialog::UpdatePreviewAndStatus()
{
    if (activeTab_ == 0)
    {
        int sub = 0;
        zhihui_dialog_memory::TryGetEnum(dimSubmode_, &sub);

        double sampleSize = 25.0;
        int selectedCount = 0;
        if (dimSelect_)
        {
            const auto objs = dimSelect_->GetSelectedObjects();
            selectedCount = static_cast<int>(objs.size());
            if (!objs.empty())
            {
                if (auto* firstDim = dynamic_cast<NXOpen::Annotations::Dimension*>(objs[0]))
                {
                    const double cs = firstDim->ComputedSize();
                    if (cs > 1.0e-4) sampleSize = cs;
                }
            }
        }

        std::ostringstream preview;
        preview << "实时预览 (以尺寸 " << FormatUnsignedTol(sampleSize) << " 为例)：";

        if (sub == 0)
        {
            int idx = 0;
            zhihui_dialog_memory::TryGetEnum(dimSheetPreset_, &idx);
            const auto presets = DefaultDimPresets();
            if (idx >= 0 && static_cast<std::size_t>(idx) < presets.size())
            {
                const auto& p = presets[static_cast<std::size_t>(idx)];
                if (p.symmetric)
                {
                    preview << FormatUnsignedTol(sampleSize) << " ±" << FormatUnsignedTol(p.upper);
                }
                else
                {
                    preview << FormatUnsignedTol(sampleSize) << " (" << FormatSignedTol(p.upper) << " / " << FormatSignedTol(p.lower) << ")";
                }
            }
        }
        else if (sub == 1 || sub == 2)
        {
            const bool isHole = (sub == 1);
            int fitIdx = 0;
            zhihui_dialog_memory::TryGetEnum(isHole ? dimHoleFit_ : dimShaftFit_, &fitIdx);
            std::string code, dev;
            int grade = 7;
            double up = 0.0, low = 0.0;
            if (LookupFitByIndex(isHole, fitIdx, sampleSize, code, dev, grade, up, low))
            {
                preview << "∅" << FormatUnsignedTol(sampleSize) << " " << code
                        << " (" << FormatSignedTol(up) << " / " << FormatSignedTol(low) << ")";
            }
        }
        else if (sub == 3)
        {
            int hIdx = 1, sIdx = 5;
            zhihui_dialog_memory::TryGetEnum(dimHoleFit_, &hIdx);
            zhihui_dialog_memory::TryGetEnum(dimShaftFit_, &sIdx);
            std::string hCode, hDev, sCode, sDev;
            int hG = 7, sG = 6;
            double hUp = 0.0, hLow = 0.0, sUp = 0.0, sLow = 0.0;
            LookupFitByIndex(true, hIdx, sampleSize, hCode, hDev, hG, hUp, hLow);
            LookupFitByIndex(false, sIdx, sampleSize, sCode, sDev, sG, sUp, sLow);
            preview << "∅" << FormatUnsignedTol(sampleSize) << " " << hCode << "/" << sCode
                    << " [孔:" << FormatSignedTol(hUp) << "/" << FormatSignedTol(hLow)
                    << " 轴:" << FormatSignedTol(sUp) << "/" << FormatSignedTol(sLow) << "]";
        }
        else if (sub == 4)
        {
            int gIdx = 1;
            zhihui_dialog_memory::TryGetEnum(dimGb1804Grade_, &gIdx);
            static const char* kGNames[4] = {"f", "m", "c", "v"};
            double pm = 0.2;
            LookupGb1804Linear(sampleSize, gIdx, pm);
            preview << "GB/T 1804-" << kGNames[std::clamp(gIdx, 0, 3)] << " -> "
                    << FormatUnsignedTol(sampleSize) << " ±" << FormatUnsignedTol(pm);
        }
        else if (sub == 5)
        {
            double up = 0.1, low = -0.1;
            zhihui_dialog_memory::TryGetDouble(dimUpperTol_, &up);
            zhihui_dialog_memory::TryGetDouble(dimLowerTol_, &low);
            if (std::fabs(up + low) < 1.0e-7 && up > 0.0)
            {
                preview << FormatUnsignedTol(sampleSize) << " ±" << FormatUnsignedTol(up);
            }
            else
            {
                preview << FormatUnsignedTol(sampleSize) << " (" << FormatSignedTol(up) << " / " << FormatSignedTol(low) << ")";
            }
        }
        else if (sub == 6)
        {
            preview << "清除所选尺寸的公差及参考括号";
        }

        SetLabelText(previewLabel_, preview.str());

        bool autoApply = false;
        zhihui_dialog_memory::TryGetLogical(dimAutoApply_, &autoApply);
        std::ostringstream status;
        if (autoApply)
        {
            status << "当前已开启【连续格式刷】：点选或框选尺寸将立即赋公差。";
        }
        else
        {
            status << "当前已选 " << selectedCount << " 个尺寸，点击【应用】或【确定】批量赋公差。";
        }
        SetLabelText(statusLabel_, status.str());
    }
    else if (activeTab_ == 1)
    {
        static const char* kCharSymbols[14] = {
            "— 直线度", "▱ 平面度", "○ 圆度", "⌭ 圆柱度",
            "⌒ 线轮廓", "⌓ 面轮廓", "∠ 倾斜度", "⊥ 垂直度",
            "∥ 平行度", "⌖ 位置度", "◎ 同轴度", "≡ 对称度",
            "↗ 圆跳动", "↗↗ 全跳动"
        };
        static const char* kZoneNames[4] = {"", "∅", "S∅", "□"};
        static const char* kMatNames[3] = {"", " Ⓜ", " Ⓛ"};

        const int ch = std::clamp(selectedGdtChar_, 0, 13);
        int zs = 0, mm = 0, d1 = 0, d2 = 0, d3 = 0;
        double val = 0.1;
        zhihui_dialog_memory::TryGetEnum(gdtZoneShape_, &zs);
        zhihui_dialog_memory::TryGetDouble(gdtValue_, &val);
        zhihui_dialog_memory::TryGetEnum(gdtMatMod_, &mm);
        zhihui_dialog_memory::TryGetEnum(gdtDatum1_, &d1);
        zhihui_dialog_memory::TryGetEnum(gdtDatum2_, &d2);
        zhihui_dialog_memory::TryGetEnum(gdtDatum3_, &d3);

        std::ostringstream preview;
        preview << "公差框预览：[ " << kCharSymbols[ch]
                << " | " << kZoneNames[std::clamp(zs, 0, 3)] << FormatUnsignedTol(val)
                << kMatNames[std::clamp(mm, 0, 2)];
        const std::string s1 = DatumEnumToString(d1);
        const std::string s2 = DatumEnumToString(d2);
        const std::string s3 = DatumEnumToString(d3);
        if (!s1.empty()) preview << " | " << s1;
        if (!s2.empty()) preview << " | " << s2;
        if (!s3.empty()) preview << " | " << s3;
        preview << " ]";

        SetLabelText(previewLabel_, preview.str());
        SetLabelText(statusLabel_, "操作：1.选边（自动跳选点） -> 2.点位置立即出预览 -> 满意点【应用】确认。");
    }
    else if (activeTab_ == 2)
    {
        std::string letter = GetStringBlockValue(datumLetter_);
        if (letter.empty()) letter = "A";
        double h = 3.5;
        zhihui_dialog_memory::TryGetDouble(datumTextHeight_, &h);
        std::ostringstream preview;
        preview << "基准符号预览：[ " << letter << " ]  字高 " << FormatUnsignedTol(h) << " mm";
        SetLabelText(previewLabel_, preview.str());
        SetLabelText(statusLabel_, "操作：1.选基准边线（自动跳选点） -> 2.点位置立即出预览 -> 满意点【应用】确认。");
    }
}

bool GonCaBiaoZuDialog::ExecuteDimensionTolerance(bool clearSelectionAfter)
{
    if (!dimSelect_) return false;
    const std::vector<NXOpen::TaggedObject*> selected = dimSelect_->GetSelectedObjects();
    if (selected.empty())
    {
        SetLabelText(statusLabel_, "请先在工程图中点选或框选至少一个尺寸！");
        return false;
    }

    int sub = 0, sheetIdx = 0, holeIdx = 1, shaftIdx = 5, fitStyle = 0, gb1804Grade = 1;
    int prefixMod = 0, suffixMod = 0;
    double manualUp = 0.1, manualLow = -0.1;
    bool refFlag = false;

    zhihui_dialog_memory::TryGetEnum(dimSubmode_, &sub);
    zhihui_dialog_memory::TryGetEnum(dimSheetPreset_, &sheetIdx);
    zhihui_dialog_memory::TryGetEnum(dimHoleFit_, &holeIdx);
    zhihui_dialog_memory::TryGetEnum(dimShaftFit_, &shaftIdx);
    zhihui_dialog_memory::TryGetEnum(dimFitStyle_, &fitStyle);
    zhihui_dialog_memory::TryGetEnum(dimGb1804Grade_, &gb1804Grade);
    zhihui_dialog_memory::TryGetDouble(dimUpperTol_, &manualUp);
    zhihui_dialog_memory::TryGetDouble(dimLowerTol_, &manualLow);
    zhihui_dialog_memory::TryGetEnum(dimPrefixMod_, &prefixMod);
    zhihui_dialog_memory::TryGetEnum(dimSuffixMod_, &suffixMod);
    zhihui_dialog_memory::TryGetLogical(dimRefFlag_, &refFlag);

    const NXOpen::Session::UndoMarkId markId =
        session_->SetUndoMark(NXOpen::Session::MarkVisibilityVisible, "GonCaBiaoZu Dimension Tolerance");

    int updatedCount = 0;
    std::string lastSummary;

    for (NXOpen::TaggedObject* obj : selected)
    {
        auto* dim = dynamic_cast<NXOpen::Annotations::Dimension*>(obj);
        if (!dim) continue;

        try
        {
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
                    dim->SetToleranceType(NXOpen::Annotations::ToleranceTypeLimitsAndFits);
                    dim->SetLimitFitEnhancedDisplayType(
                        isHole ? NXOpen::Annotations::FitAnsiHoleTypeHole : NXOpen::Annotations::FitAnsiHoleTypeShaft);
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

                    if (fitStyle == 1)
                    {
                        dim->SetLimitFitDisplayStyle(NXOpen::Annotations::FitDisplayStyleFitSymbolsAndTolerances);
                    }
                    else if (fitStyle == 2)
                    {
                        dim->SetLimitFitDisplayStyle(NXOpen::Annotations::FitDisplayStyleFitSymbols);
                    }
                    else
                    {
                        dim->SetLimitFitDisplayStyle(NXOpen::Annotations::FitDisplayStyleFitSymbolsAndLimits);
                    }

                    NXOpen::Annotations::DimensionPreferences* prefs = dim->GetDimensionPreferences();
                    if (prefs)
                    {
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
                dim->SetLimitFitDeviation(U8(hDev));
                dim->SetLimitFitGrade(hGrade);
                dim->SetLimitFitShaftDeviation(U8(sDev));
                dim->SetLimitFitShaftGrade(sGrade);

                if (fitStyle == 2)
                {
                    dim->SetLimitFitDisplayStyle(NXOpen::Annotations::FitDisplayStyleFitSymbols);
                }
                else if (fitStyle == 3)
                {
                    dim->SetLimitFitDisplayStyle(NXOpen::Annotations::FitDisplayStyleFitSymbolsAndLimits);
                }
                else
                {
                    dim->SetLimitFitDisplayStyle(NXOpen::Annotations::FitDisplayStyleFitSymbolsAndTolerances);
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
                lastSummary = "已清除公差";
            }
            else if (!useNativeLimitFit)
            {
                const int dec = std::max(CountDecimals(upVal), CountDecimals(lowVal));
                dim->SetMetricToleranceDecimalPlaces(dec);
                dim->SetToleranceDecimalPlaces(dec);

                if (isSymmetric)
                {
                    dim->SetToleranceType(NXOpen::Annotations::ToleranceTypeBilateralOneLine);
                    dim->SetUpperMetricToleranceValue( std::fabs(upVal));
                    dim->SetLowerMetricToleranceValue(-std::fabs(upVal));
                    lastSummary = "±" + FormatUnsignedTol(upVal);
                }
                else
                {
                    dim->SetToleranceType(NXOpen::Annotations::ToleranceTypeBilateralTwoLines);
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
                    lastSummary = FormatSignedTol(upVal) + " / " + FormatSignedTol(lowVal);
                }
                dim->SetReferenceDimensionFlag(refFlag);
            }
            else
            {
                const int dec = std::max(CountDecimals(upVal), CountDecimals(lowVal));
                dim->SetMetricToleranceDecimalPlaces(dec);
                dim->SetToleranceDecimalPlaces(dec);
                dim->SetReferenceDimensionFlag(refFlag);
                lastSummary = fitCodeText + " (" + FormatSignedTol(upVal) + "/" + FormatSignedTol(lowVal) + ")";
            }

            // 前后缀修饰
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
                        std::vector<NXOpen::NXString> lines;
                        lines.push_back(U8(kPrefixes[std::clamp(prefixMod, 2, 6)]));
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
                        std::vector<NXOpen::NXString> lines;
                        lines.push_back(U8(kSuffixes[std::clamp(suffixMod, 2, 5)]));
                        app->SetAfterText(lines);
                    }

                    dim->SetAppendedText(app);
                    delete app;
                }
            }

            ++updatedCount;
        }
        catch (const NXOpen::NXException& ex)
        {
            Log(std::string("Dimension update failed: ") + (ex.Message() ? ex.Message() : ""));
        }
    }

    try
    {
        session_->UpdateManager()->DoUpdate(markId);
    }
    catch (const NXOpen::NXException& ex)
    {
        Log(std::string("DoUpdate NXException: ") + (ex.Message() ? ex.Message() : ""));
    }

    if (clearSelectionAfter && dimSelect_)
    {
        internalUpdating_ = true;
        dimSelect_->SetSelectedObjects(std::vector<NXOpen::TaggedObject*>{});
        internalUpdating_ = false;
    }

    std::ostringstream msg;
    msg << "已成功更新 " << updatedCount << " 个尺寸公差 (" << lastSummary << ")。";
    SetLabelText(statusLabel_, msg.str());
    Log(msg.str());
    return updatedCount > 0;
}

void GonCaBiaoZuDialog::DeleteGdtPreview()
{
    if (gdtPreviewTag_ == NULL_TAG) return;
    try
    {
        if (NXOpen::TaggedObject* obj = NXOpen::NXObjectManager::Get(gdtPreviewTag_))
        {
            const NXOpen::Session::UndoMarkId delMark =
                session_->SetUndoMark(NXOpen::Session::MarkVisibilityInvisible, "GonCaBiaoZu Delete FCF Preview");
            std::vector<NXOpen::TaggedObject*> delList{obj};
            session_->UpdateManager()->AddObjectsToDeleteList(delList);
            session_->UpdateManager()->DoUpdate(delMark);
            if (session_->DoesUndoMarkExist(delMark, nullptr))
            {
                session_->DeleteUndoMark(delMark, nullptr);
            }
        }
    }
    catch (...)
    {
        try { UF_OBJ_delete_object(gdtPreviewTag_); } catch (...) {}
    }
    gdtPreviewTag_ = NULL_TAG;
}

bool GonCaBiaoZuDialog::CreateOrUpdateGdtPreview(bool keepAsFinal, bool clearAfter)
{
    NXOpen::Part* workPart = session_->Parts()->Work();
    if (!workPart) return false;

    // 若只是点「应用」且已有实时预览，直接保留当前预览为最终结果并重置选择
    if (keepAsFinal && gdtPreviewTag_ != NULL_TAG)
    {
        gdtPreviewTag_ = NULL_TAG;
        if (clearAfter)
        {
            hasGdtPointPicked_ = false;
            if (gdtTargetSelect_)
            {
                internalUpdating_ = true;
                gdtTargetSelect_->SetSelectedObjects(std::vector<NXOpen::TaggedObject*>{});
                internalUpdating_ = false;
                gdtTargetSelect_->Focus();
            }
        }
        SetLabelText(statusLabel_, "已确认当前形位公差！请继续选择下一条边线。");
        return true;
    }

    DeleteGdtPreview();

    const int ch = std::clamp(selectedGdtChar_, 0, 13);
    int zs = 0, mm = 0, d1 = 0, d2 = 0, d3 = 0;
    double val = 0.1;
    double textHeight = 3.5;
    zhihui_dialog_memory::TryGetEnum(gdtZoneShape_, &zs);
    zhihui_dialog_memory::TryGetDouble(gdtValue_, &val);
    zhihui_dialog_memory::TryGetEnum(gdtMatMod_, &mm);
    zhihui_dialog_memory::TryGetEnum(gdtDatum1_, &d1);
    zhihui_dialog_memory::TryGetEnum(gdtDatum2_, &d2);
    zhihui_dialog_memory::TryGetEnum(gdtDatum3_, &d3);
    zhihui_dialog_memory::TryGetDouble(gdtTextHeight_, &textHeight);
    if (textHeight < 0.5) textHeight = 0.5;

    const int frameColorId = GetColorPickerValue(gdtFrameColor_, 216);
    const int textColorId = GetColorPickerValue(gdtTextColor_, 216);

    NXOpen::Point3d placePt(100.0, 100.0, 0.0);
    if (gdtPlacePoint_)
    {
        placePt = gdtPlacePoint_->Point();
    }

    NXOpen::TaggedObject* targetObj = nullptr;
    NXOpen::Point3d pickPt = placePt;
    if (gdtTargetSelect_)
    {
        const auto objs = gdtTargetSelect_->GetSelectedObjects();
        if (!objs.empty())
        {
            targetObj = objs[0];
            pickPt = gdtTargetSelect_->PickPoint();
            if (!hasGdtPointPicked_)
            {
                placePt = NXOpen::Point3d(pickPt.X + 12.0, pickPt.Y + 12.0, 0.0);
            }
        }
    }

    const NXOpen::Session::UndoMarkId markId = session_->SetUndoMark(
        keepAsFinal ? NXOpen::Session::MarkVisibilityVisible : NXOpen::Session::MarkVisibilityInvisible,
        keepAsFinal ? "GonCaBiaoZu Create FCF" : "GonCaBiaoZu Preview FCF");

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

        builder->SetCharacteristic(kCharEnums[std::clamp(ch, 0, 13)]);
        builder->SetFrameStyle(NXOpen::Annotations::FeatureControlFrameBuilder::FcfFrameStyleSingleFrame);

        // 自定义字体高度、字体颜色、外框与引线颜色
        if (NXOpen::Annotations::StyleBuilder* style = builder->Style())
        {
            if (NXOpen::Annotations::LetteringStyleBuilder* lettering = style->LetteringStyle())
            {
                lettering->SetGeneralTextSize(textHeight);
                if (NXOpen::NXColor* txtClr = workPart->Colors()->Find(textColorId))
                {
                    lettering->SetGeneralTextColor(txtClr);
                }
            }
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

        NXOpen::Annotations::FeatureControlFrameDataBuilder* frame =
            builder->FeatureControlFrameDataList()->FindItem(0);
        if (frame)
        {
            static const NXOpen::Annotations::FeatureControlFrameDataBuilder::ToleranceZoneShape kZoneEnums[4] = {
                NXOpen::Annotations::FeatureControlFrameDataBuilder::ToleranceZoneShapeNone,
                NXOpen::Annotations::FeatureControlFrameDataBuilder::ToleranceZoneShapeDiameter,
                NXOpen::Annotations::FeatureControlFrameDataBuilder::ToleranceZoneShapeSphericalDiameter,
                NXOpen::Annotations::FeatureControlFrameDataBuilder::ToleranceZoneShapeSquare
            };
            frame->SetZoneShape(kZoneEnums[std::clamp(zs, 0, 3)]);
            frame->SetToleranceValue(U8(FormatUnsignedTol(val)));

            static const NXOpen::Annotations::FeatureControlFrameDataBuilder::ToleranceMaterialModifier kMatEnums[3] = {
                NXOpen::Annotations::FeatureControlFrameDataBuilder::ToleranceMaterialModifierNone,
                NXOpen::Annotations::FeatureControlFrameDataBuilder::ToleranceMaterialModifierMaximumMaterialCondition,
                NXOpen::Annotations::FeatureControlFrameDataBuilder::ToleranceMaterialModifierLeastMaterialCondition
            };
            frame->SetMaterialModifier(kMatEnums[std::clamp(mm, 0, 2)]);

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
                pickPt,
                nullptr,
                nullView,
                pickPt);
        }

        NXOpen::View* nullView = nullptr;
        builder->Origin()->Origin()->SetValue(nullptr, nullView, placePt);
        NXOpen::NXObject* committed = builder->Commit();
        builder->Destroy();
        builder = nullptr;

        if (!keepAsFinal && committed != nullptr)
        {
            gdtPreviewTag_ = committed->Tag();
            SetLabelText(statusLabel_, "已生成实时预览！可继续微调颜色/字高/位置，满意后点【应用】确认。");
        }
        else
        {
            gdtPreviewTag_ = NULL_TAG;
            if (clearAfter)
            {
                hasGdtPointPicked_ = false;
                if (gdtTargetSelect_)
                {
                    internalUpdating_ = true;
                    gdtTargetSelect_->SetSelectedObjects(std::vector<NXOpen::TaggedObject*>{});
                    internalUpdating_ = false;
                    gdtTargetSelect_->Focus();
                }
            }
            SetLabelText(statusLabel_, "已成功生成形位公差框！可继续点选下一条边线。");
        }
        Log(keepAsFinal ? "Created FCF annotation." : "Updated FCF live preview.");
        return true;
    }
    catch (const NXOpen::NXException& ex)
    {
        if (builder)
        {
            try { builder->Destroy(); } catch (...) {}
        }
        Log(std::string("CreateOrUpdateGdtPreview NXException: ") + (ex.Message() ? ex.Message() : ""));
        SetLabelText(statusLabel_, "形位公差创建失败，请检查所选引线位置是否在图纸区域内。");
        return false;
    }
}

void GonCaBiaoZuDialog::DeleteDatumPreview()
{
    if (datumPreviewTag_ == NULL_TAG) return;
    try
    {
        if (NXOpen::TaggedObject* obj = NXOpen::NXObjectManager::Get(datumPreviewTag_))
        {
            const NXOpen::Session::UndoMarkId delMark =
                session_->SetUndoMark(NXOpen::Session::MarkVisibilityInvisible, "GonCaBiaoZu Delete Datum Preview");
            std::vector<NXOpen::TaggedObject*> delList{obj};
            session_->UpdateManager()->AddObjectsToDeleteList(delList);
            session_->UpdateManager()->DoUpdate(delMark);
            if (session_->DoesUndoMarkExist(delMark, nullptr))
            {
                session_->DeleteUndoMark(delMark, nullptr);
            }
        }
    }
    catch (...)
    {
        try { UF_OBJ_delete_object(datumPreviewTag_); } catch (...) {}
    }
    datumPreviewTag_ = NULL_TAG;
}

bool GonCaBiaoZuDialog::CreateOrUpdateDatumPreview(bool keepAsFinal, bool clearAfter)
{
    NXOpen::Part* workPart = session_->Parts()->Work();
    if (!workPart) return false;

    std::string letter = GetStringBlockValue(datumLetter_);
    if (letter.empty()) letter = "A";

    bool autoInc = true;
    zhihui_dialog_memory::TryGetLogical(datumAutoInc_, &autoInc);

    // 若只是点「应用」且已有实时预览，直接保留当前预览为最终结果并自动递增字母、重置选择
    if (keepAsFinal && datumPreviewTag_ != NULL_TAG)
    {
        datumPreviewTag_ = NULL_TAG;
        if (autoInc)
        {
            const std::string nextLetter = NextDatumLetter(letter);
            internalUpdating_ = true;
            SetStringBlockValue(datumLetter_, nextLetter);
            internalUpdating_ = false;
            SaveMemory();
        }
        if (clearAfter)
        {
            hasDatumPointPicked_ = false;
            if (datumTargetSelect_)
            {
                internalUpdating_ = true;
                datumTargetSelect_->SetSelectedObjects(std::vector<NXOpen::TaggedObject*>{});
                internalUpdating_ = false;
                datumTargetSelect_->Focus();
            }
        }
        UpdatePreviewAndStatus();
        SetLabelText(statusLabel_, "已确认基准 [" + letter + "]！请继续选择下一处基准边线。");
        return true;
    }

    DeleteDatumPreview();

    double textHeight = 3.5;
    zhihui_dialog_memory::TryGetDouble(datumTextHeight_, &textHeight);
    if (textHeight < 0.5) textHeight = 0.5;

    const int frameColorId = GetColorPickerValue(datumFrameColor_, 216);
    const int textColorId = GetColorPickerValue(datumTextColor_, 216);

    NXOpen::Point3d placePt(100.0, 100.0, 0.0);
    if (datumPlacePoint_)
    {
        placePt = datumPlacePoint_->Point();
    }

    NXOpen::TaggedObject* targetObj = nullptr;
    NXOpen::Point3d pickPt = placePt;
    if (datumTargetSelect_)
    {
        const auto objs = datumTargetSelect_->GetSelectedObjects();
        if (!objs.empty())
        {
            targetObj = objs[0];
            pickPt = datumTargetSelect_->PickPoint();
            if (!hasDatumPointPicked_)
            {
                placePt = NXOpen::Point3d(pickPt.X + 10.0, pickPt.Y + 10.0, 0.0);
            }
        }
    }

    const NXOpen::Session::UndoMarkId markId = session_->SetUndoMark(
        keepAsFinal ? NXOpen::Session::MarkVisibilityVisible : NXOpen::Session::MarkVisibilityInvisible,
        keepAsFinal ? "GonCaBiaoZu Create Datum" : "GonCaBiaoZu Preview Datum");

    NXOpen::Annotations::DraftingDatumFeatureSymbolBuilder* builder = nullptr;
    try
    {
        builder = workPart->Annotations()->Datums()->CreateDraftingDatumFeatureSymbolBuilder(nullptr);
        builder->SetLetter(U8(letter));

        // 自定义基准字高、字体颜色、外框与三角/引线颜色
        if (NXOpen::Annotations::StyleBuilder* style = builder->Style())
        {
            if (NXOpen::Annotations::LetteringStyleBuilder* lettering = style->LetteringStyle())
            {
                lettering->SetGeneralTextSize(textHeight);
                if (NXOpen::NXColor* txtClr = workPart->Colors()->Find(textColorId))
                {
                    lettering->SetGeneralTextColor(txtClr);
                }
            }
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
                pickPt,
                nullptr,
                nullView,
                pickPt);
        }

        NXOpen::View* nullView = nullptr;
        builder->Origin()->Origin()->SetValue(nullptr, nullView, placePt);
        NXOpen::NXObject* committed = builder->Commit();
        builder->Destroy();
        builder = nullptr;

        if (!keepAsFinal && committed != nullptr)
        {
            datumPreviewTag_ = committed->Tag();
            SetLabelText(statusLabel_, "已生成基准 [" + letter + "] 实时预览！可继续微调字高/颜色/位置，满意后点【应用】确认。");
        }
        else
        {
            datumPreviewTag_ = NULL_TAG;
            if (autoInc)
            {
                const std::string nextLetter = NextDatumLetter(letter);
                internalUpdating_ = true;
                SetStringBlockValue(datumLetter_, nextLetter);
                internalUpdating_ = false;
                SaveMemory();
            }

            if (clearAfter)
            {
                hasDatumPointPicked_ = false;
                if (datumTargetSelect_)
                {
                    internalUpdating_ = true;
                    datumTargetSelect_->SetSelectedObjects(std::vector<NXOpen::TaggedObject*>{});
                    internalUpdating_ = false;
                    datumTargetSelect_->Focus();
                }
            }

            UpdatePreviewAndStatus();
            SetLabelText(statusLabel_, "已成功生成基准 [" + letter + "]！可继续点选下一处基准。");
        }
        Log(keepAsFinal ? ("Created Datum symbol: " + letter) : ("Updated Datum live preview: " + letter));
        return true;
    }
    catch (const NXOpen::NXException& ex)
    {
        if (builder)
        {
            try { builder->Destroy(); } catch (...) {}
        }
        Log(std::string("CreateOrUpdateDatumPreview NXException: ") + (ex.Message() ? ex.Message() : ""));
        SetLabelText(statusLabel_, "基准符号创建失败，请检查所选边线和放置点。");
        return false;
    }
}

void GonCaBiaoZuDialog::LoadMemory()
{
    activeTab_ = std::clamp(zhihui_dialog_memory::ReadInt(kMemoryFile, L"active_tab", 0), 0, 2);
    zhihui_dialog_memory::LoadEnum(kMemoryFile, L"dim_submode", dimSubmode_);
    zhihui_dialog_memory::LoadEnum(kMemoryFile, L"dim_sheet_preset", dimSheetPreset_);
    zhihui_dialog_memory::LoadEnum(kMemoryFile, L"dim_hole_fit", dimHoleFit_);
    zhihui_dialog_memory::LoadEnum(kMemoryFile, L"dim_shaft_fit", dimShaftFit_);
    zhihui_dialog_memory::LoadEnum(kMemoryFile, L"dim_fit_style", dimFitStyle_);
    zhihui_dialog_memory::LoadEnum(kMemoryFile, L"dim_gb1804_grade", dimGb1804Grade_);
    zhihui_dialog_memory::LoadDouble(kMemoryFile, L"dim_upper_tol", dimUpperTol_);
    zhihui_dialog_memory::LoadDouble(kMemoryFile, L"dim_lower_tol", dimLowerTol_);
    zhihui_dialog_memory::LoadEnum(kMemoryFile, L"dim_prefix_mod", dimPrefixMod_);
    zhihui_dialog_memory::LoadEnum(kMemoryFile, L"dim_suffix_mod", dimSuffixMod_);
    zhihui_dialog_memory::LoadLogical(kMemoryFile, L"dim_ref_flag", dimRefFlag_);
    zhihui_dialog_memory::LoadLogical(kMemoryFile, L"dim_auto_apply", dimAutoApply_);

    zhihui_dialog_memory::LoadEnum(kMemoryFile, L"gdt_preset", gdtPreset_);
    selectedGdtChar_ = std::clamp(zhihui_dialog_memory::ReadInt(kMemoryFile, L"selected_gdt_char", 1), 0, 13);
    zhihui_dialog_memory::LoadEnum(kMemoryFile, L"gdt_zone_shape", gdtZoneShape_);
    zhihui_dialog_memory::LoadEnum(kMemoryFile, L"gdt_gb1184_grade", gdtGb1184Grade_);
    zhihui_dialog_memory::LoadDouble(kMemoryFile, L"gdt_value", gdtValue_);
    zhihui_dialog_memory::LoadEnum(kMemoryFile, L"gdt_mat_mod", gdtMatMod_);
    zhihui_dialog_memory::LoadEnum(kMemoryFile, L"gdt_datum1", gdtDatum1_);
    zhihui_dialog_memory::LoadEnum(kMemoryFile, L"gdt_datum2", gdtDatum2_);
    zhihui_dialog_memory::LoadEnum(kMemoryFile, L"gdt_datum3", gdtDatum3_);
    zhihui_dialog_memory::LoadDouble(kMemoryFile, L"gdt_text_height", gdtTextHeight_);
    SetColorPickerValue(gdtFrameColor_, zhihui_dialog_memory::ReadInt(kMemoryFile, L"gdt_frame_color", 216));
    SetColorPickerValue(gdtTextColor_, zhihui_dialog_memory::ReadInt(kMemoryFile, L"gdt_text_color", 216));

    const std::string savedDatum = zhihui_dialog_memory::ReadString(kMemoryFile, L"datum_letter", "A");
    SetStringBlockValue(datumLetter_, savedDatum);
    zhihui_dialog_memory::LoadLogical(kMemoryFile, L"datum_auto_inc", datumAutoInc_);
    zhihui_dialog_memory::LoadDouble(kMemoryFile, L"datum_text_height", datumTextHeight_);
    SetColorPickerValue(datumFrameColor_, zhihui_dialog_memory::ReadInt(kMemoryFile, L"datum_frame_color", 216));
    SetColorPickerValue(datumTextColor_, zhihui_dialog_memory::ReadInt(kMemoryFile, L"datum_text_color", 216));
}

void GonCaBiaoZuDialog::SaveMemory() const
{
    zhihui_dialog_memory::WriteInt(kMemoryFile, L"active_tab", activeTab_);
    zhihui_dialog_memory::SaveEnum(kMemoryFile, L"dim_submode", dimSubmode_);
    zhihui_dialog_memory::SaveEnum(kMemoryFile, L"dim_sheet_preset", dimSheetPreset_);
    zhihui_dialog_memory::SaveEnum(kMemoryFile, L"dim_hole_fit", dimHoleFit_);
    zhihui_dialog_memory::SaveEnum(kMemoryFile, L"dim_shaft_fit", dimShaftFit_);
    zhihui_dialog_memory::SaveEnum(kMemoryFile, L"dim_fit_style", dimFitStyle_);
    zhihui_dialog_memory::SaveEnum(kMemoryFile, L"dim_gb1804_grade", dimGb1804Grade_);
    zhihui_dialog_memory::SaveDouble(kMemoryFile, L"dim_upper_tol", dimUpperTol_);
    zhihui_dialog_memory::SaveDouble(kMemoryFile, L"dim_lower_tol", dimLowerTol_);
    zhihui_dialog_memory::SaveEnum(kMemoryFile, L"dim_prefix_mod", dimPrefixMod_);
    zhihui_dialog_memory::SaveEnum(kMemoryFile, L"dim_suffix_mod", dimSuffixMod_);
    zhihui_dialog_memory::SaveLogical(kMemoryFile, L"dim_ref_flag", dimRefFlag_);
    zhihui_dialog_memory::SaveLogical(kMemoryFile, L"dim_auto_apply", dimAutoApply_);

    zhihui_dialog_memory::SaveEnum(kMemoryFile, L"gdt_preset", gdtPreset_);
    zhihui_dialog_memory::WriteInt(kMemoryFile, L"selected_gdt_char", selectedGdtChar_);
    zhihui_dialog_memory::SaveEnum(kMemoryFile, L"gdt_zone_shape", gdtZoneShape_);
    zhihui_dialog_memory::SaveEnum(kMemoryFile, L"gdt_gb1184_grade", gdtGb1184Grade_);
    zhihui_dialog_memory::SaveDouble(kMemoryFile, L"gdt_value", gdtValue_);
    zhihui_dialog_memory::SaveEnum(kMemoryFile, L"gdt_mat_mod", gdtMatMod_);
    zhihui_dialog_memory::SaveEnum(kMemoryFile, L"gdt_datum1", gdtDatum1_);
    zhihui_dialog_memory::SaveEnum(kMemoryFile, L"gdt_datum2", gdtDatum2_);
    zhihui_dialog_memory::SaveEnum(kMemoryFile, L"gdt_datum3", gdtDatum3_);
    zhihui_dialog_memory::SaveDouble(kMemoryFile, L"gdt_text_height", gdtTextHeight_);
    zhihui_dialog_memory::WriteInt(kMemoryFile, L"gdt_frame_color", GetColorPickerValue(gdtFrameColor_, 216));
    zhihui_dialog_memory::WriteInt(kMemoryFile, L"gdt_text_color", GetColorPickerValue(gdtTextColor_, 216));

    zhihui_dialog_memory::WriteString(kMemoryFile, L"datum_letter", GetStringBlockValue(datumLetter_));
    zhihui_dialog_memory::SaveLogical(kMemoryFile, L"datum_auto_inc", datumAutoInc_);
    zhihui_dialog_memory::SaveDouble(kMemoryFile, L"datum_text_height", datumTextHeight_);
    zhihui_dialog_memory::WriteInt(kMemoryFile, L"datum_frame_color", GetColorPickerValue(datumFrameColor_, 216));
    zhihui_dialog_memory::WriteInt(kMemoryFile, L"datum_text_color", GetColorPickerValue(datumTextColor_, 216));
}

int GonCaBiaoZuDialog::GetColorPickerValue(NXOpen::BlockStyler::UIBlock* block, int fallback) const
{
    if (!block) return fallback;
    NXOpen::BlockStyler::PropertyList* props = nullptr;
    try
    {
        props = block->GetProperties();
        const std::vector<int> vals = props->GetIntegerVector("Value");
        delete props;
        if (!vals.empty() && vals[0] >= 1 && vals[0] <= 216)
        {
            return vals[0];
        }
    }
    catch (...)
    {
        if (props) delete props;
    }
    return fallback;
}

void GonCaBiaoZuDialog::SetColorPickerValue(NXOpen::BlockStyler::UIBlock* block, int colorId) const
{
    if (!block) return;
    const int clamped = std::clamp(colorId, 1, 216);
    NXOpen::BlockStyler::PropertyList* props = nullptr;
    try
    {
        props = block->GetProperties();
        std::vector<int> vals{clamped};
        props->SetIntegerVector("Value", vals);
        delete props;
    }
    catch (...)
    {
        if (props) delete props;
    }
}

void GonCaBiaoZuDialog::SetLabelText(NXOpen::BlockStyler::UIBlock* block, const std::string& utf8Text) const
{
    if (!block) return;
    NXOpen::BlockStyler::PropertyList* props = nullptr;
    try
    {
        props = block->GetProperties();
        props->SetString("Label", U8(utf8Text));
        delete props;
    }
    catch (...)
    {
        if (props) delete props;
    }
}

std::string GonCaBiaoZuDialog::GetStringBlockValue(NXOpen::BlockStyler::UIBlock* block) const
{
    if (!block) return {};
    NXOpen::BlockStyler::PropertyList* props = nullptr;
    try
    {
        props = block->GetProperties();
        const NXOpen::NXString val = props->GetString("Value");
        delete props;
        return val.GetUTF8Text() ? std::string(val.GetUTF8Text()) : std::string();
    }
    catch (...)
    {
        if (props) delete props;
        return {};
    }
}

void GonCaBiaoZuDialog::SetStringBlockValue(NXOpen::BlockStyler::UIBlock* block, const std::string& utf8Text) const
{
    if (!block) return;
    NXOpen::BlockStyler::PropertyList* props = nullptr;
    try
    {
        props = block->GetProperties();
        props->SetString("Value", U8(utf8Text));
        delete props;
    }
    catch (...)
    {
        if (props) delete props;
    }
}

void GonCaBiaoZuDialog::SetBlockShow(NXOpen::BlockStyler::UIBlock* block, bool show) const
{
    if (!block) return;
    NXOpen::BlockStyler::PropertyList* props = nullptr;
    try
    {
        props = block->GetProperties();
        props->SetLogical("Show", show);
        delete props;
    }
    catch (...)
    {
        if (props) delete props;
    }
}

void GonCaBiaoZuDialog::SetBlockEnable(NXOpen::BlockStyler::UIBlock* block, bool enable) const
{
    if (!block) return;
    NXOpen::BlockStyler::PropertyList* props = nullptr;
    try
    {
        props = block->GetProperties();
        props->SetLogical("Enable", enable);
        delete props;
    }
    catch (...)
    {
        if (props) delete props;
    }
}

void GonCaBiaoZuDialog::Log(const std::string& message) const
{
    WriteDebugLog(message);
    try
    {
        if (session_ && session_->LogFile())
        {
            session_->LogFile()->WriteLine(("GonCaBiaoZu: " + message).c_str());
        }
    }
    catch (...) {}
}

void GonCaBiaoZuDialog::Notify(const std::wstring& message, bool error) const
{
    MessageBoxW(nullptr, message.c_str(), L"公差标注",
        MB_OK | (error ? MB_ICONERROR : MB_ICONINFORMATION));
}
