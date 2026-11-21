#pragma once

#include <NXOpen/BlockStyler_BlockDialog.hxx>
#include <NXOpen/Callback.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/ugmath.hxx>

#include <array>
#include <string>
#include <vector>

namespace NXOpen
{
namespace BlockStyler
{
class SelectObject;
class SpecifyPoint;
class UIBlock;
}
}

class GonCaBiaoZuDialog final
{
public:
    GonCaBiaoZuDialog();
    ~GonCaBiaoZuDialog();
    NXOpen::BlockStyler::BlockDialog::DialogResponse Launch();

private:
    void Initialize();
    void DialogShown();
    int Update(NXOpen::BlockStyler::UIBlock* block);
    int Apply();
    int Ok();
    int Cancel();
    int Filter(NXOpen::BlockStyler::UIBlock* block, NXOpen::TaggedObject* object);

    void ConfigureSelectionFilters();
    void LoadMemory();
    void SaveMemory() const;
    void SwitchMainTab(int newTab);
    void UpdateUiVisibility();
    void SyncFromSheetPreset();
    void SyncFromGdtPreset();
    void SyncGb1184GdtValue();
    void UpdatePreviewAndStatus();

    bool ExecuteDimensionTolerance(bool clearSelectionAfter);
    bool CreateOrUpdateGdtPreview(bool keepAsFinal, bool clearAfter);
    void DeleteGdtPreview();
    bool CreateOrUpdateDatumPreview(bool keepAsFinal, bool clearAfter);
    void DeleteDatumPreview();

    int GetColorPickerValue(NXOpen::BlockStyler::UIBlock* block, int fallback) const;
    void SetColorPickerValue(NXOpen::BlockStyler::UIBlock* block, int colorId) const;
    void SetLabelText(NXOpen::BlockStyler::UIBlock* block, const std::string& utf8Text) const;
    std::string GetStringBlockValue(NXOpen::BlockStyler::UIBlock* block) const;
    void SetStringBlockValue(NXOpen::BlockStyler::UIBlock* block, const std::string& utf8Text) const;
    void SetBlockShow(NXOpen::BlockStyler::UIBlock* block, bool show) const;
    void SetBlockEnable(NXOpen::BlockStyler::UIBlock* block, bool enable) const;
    void Log(const std::string& message) const;
    void Notify(const std::wstring& message, bool error) const;

    NXOpen::Session* session_ = nullptr;
    NXOpen::BlockStyler::BlockDialog* dialog_ = nullptr;

    // Navigation buttons + highlight bars
    NXOpen::BlockStyler::UIBlock* tabBtnDim_ = nullptr;
    NXOpen::BlockStyler::UIBlock* tabBtnGdt_ = nullptr;
    NXOpen::BlockStyler::UIBlock* tabBtnDatum_ = nullptr;
    NXOpen::BlockStyler::UIBlock* tabBarDim_ = nullptr;
    NXOpen::BlockStyler::UIBlock* tabBarGdt_ = nullptr;
    NXOpen::BlockStyler::UIBlock* tabBarDatum_ = nullptr;

    // Groups
    NXOpen::BlockStyler::UIBlock* dimGroup_ = nullptr;
    NXOpen::BlockStyler::UIBlock* dimExtraGroup_ = nullptr;
    NXOpen::BlockStyler::UIBlock* gdtSymGroup_ = nullptr;
    NXOpen::BlockStyler::UIBlock* gdtPickGroup_ = nullptr;
    NXOpen::BlockStyler::UIBlock* gdtParamGroup_ = nullptr;
    NXOpen::BlockStyler::UIBlock* gdtStyleGroup_ = nullptr;
    NXOpen::BlockStyler::UIBlock* datumGroup_ = nullptr;
    NXOpen::BlockStyler::UIBlock* datumStyleGroup_ = nullptr;
    NXOpen::BlockStyler::UIBlock* infoGroup_ = nullptr;

    // Tab 1: Dimension Tolerance
    NXOpen::BlockStyler::SelectObject* dimSelect_ = nullptr;
    NXOpen::BlockStyler::UIBlock* dimSubmode_ = nullptr;
    NXOpen::BlockStyler::UIBlock* dimSheetPreset_ = nullptr;
    NXOpen::BlockStyler::UIBlock* dimHoleFit_ = nullptr;
    NXOpen::BlockStyler::UIBlock* dimShaftFit_ = nullptr;
    NXOpen::BlockStyler::UIBlock* dimFitStyle_ = nullptr;
    NXOpen::BlockStyler::UIBlock* dimGb1804Grade_ = nullptr;
    NXOpen::BlockStyler::UIBlock* dimUpperTol_ = nullptr;
    NXOpen::BlockStyler::UIBlock* dimLowerTol_ = nullptr;
    NXOpen::BlockStyler::UIBlock* dimPrefixMod_ = nullptr;
    NXOpen::BlockStyler::UIBlock* dimSuffixMod_ = nullptr;
    NXOpen::BlockStyler::UIBlock* dimRefFlag_ = nullptr;
    NXOpen::BlockStyler::UIBlock* dimAutoApply_ = nullptr;

    // Tab 2: GD&T Feature Control Frame
    std::array<NXOpen::BlockStyler::UIBlock*, 14> gdtCharBtns_{};
    NXOpen::BlockStyler::UIBlock* gdtPreset_ = nullptr;
    NXOpen::BlockStyler::UIBlock* gdtZoneShape_ = nullptr;
    NXOpen::BlockStyler::UIBlock* gdtGb1184Grade_ = nullptr;
    NXOpen::BlockStyler::UIBlock* gdtValue_ = nullptr;
    NXOpen::BlockStyler::UIBlock* gdtMatMod_ = nullptr;
    NXOpen::BlockStyler::UIBlock* gdtDatum1_ = nullptr;
    NXOpen::BlockStyler::UIBlock* gdtDatum2_ = nullptr;
    NXOpen::BlockStyler::UIBlock* gdtDatum3_ = nullptr;
    NXOpen::BlockStyler::UIBlock* gdtTextHeight_ = nullptr;
    NXOpen::BlockStyler::UIBlock* gdtFrameColor_ = nullptr;
    NXOpen::BlockStyler::UIBlock* gdtTextColor_ = nullptr;
    NXOpen::BlockStyler::SelectObject* gdtTargetSelect_ = nullptr;
    NXOpen::BlockStyler::SpecifyPoint* gdtPlacePoint_ = nullptr;

    // Tab 3: Datum Feature Symbol
    NXOpen::BlockStyler::UIBlock* datumLetter_ = nullptr;
    NXOpen::BlockStyler::UIBlock* datumAutoInc_ = nullptr;
    NXOpen::BlockStyler::SelectObject* datumTargetSelect_ = nullptr;
    NXOpen::BlockStyler::SpecifyPoint* datumPlacePoint_ = nullptr;
    NXOpen::BlockStyler::UIBlock* datumTextHeight_ = nullptr;
    NXOpen::BlockStyler::UIBlock* datumFrameColor_ = nullptr;
    NXOpen::BlockStyler::UIBlock* datumTextColor_ = nullptr;

    // Info & Config
    NXOpen::BlockStyler::UIBlock* previewLabel_ = nullptr;
    NXOpen::BlockStyler::UIBlock* statusLabel_ = nullptr;
    NXOpen::BlockStyler::UIBlock* openConfigBtn_ = nullptr;

    int activeTab_ = 0;       // 0=尺寸公差, 1=形位公差, 2=基准符号
    int selectedGdtChar_ = 1; // 0..13, 默认 1=平面度
    bool dialogShown_ = false;
    bool internalUpdating_ = false;
    bool hasGdtPointPicked_ = false;
    bool hasDatumPointPicked_ = false;
    tag_t gdtPreviewTag_ = NULL_TAG;
    tag_t datumPreviewTag_ = NULL_TAG;
};
