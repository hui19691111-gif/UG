#pragma once

#include "AnnotationTypes.hpp"
#include "DraftingAnnotationService.hpp"
#include "ExcelRuleManager.hpp"

#include <NXOpen/BlockStyler_BlockDialog.hxx>
#include <NXOpen/BlockStyler_PropertyList.hxx>
#include <NXOpen/BlockStyler_UIBlock.hxx>
#include <NXOpen/Callback.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/UI.hxx>

#include <uf_defs.h>

#include <string>
#include <vector>

namespace KonBiaoZu
{
    class KonBiaoZuDialog
    {
    public:
        KonBiaoZuDialog();
        ~KonBiaoZuDialog();

        NXOpen::BlockStyler::BlockDialog::DialogResponse Launch();

    private:
        void initialize_cb();
        int update_cb(NXOpen::BlockStyler::UIBlock* block);
        int apply_cb();
        int ok_cb();
        int cancel_cb();
        int ExecuteAnnotation(bool advanceMarker, bool useMarkerInputForCurrentSelection = false);
        void DeleteTrackedAnnotations();
        void ClearHighlightedCircles();
        AnnotationType GetSelectedType() const;
        bool GetLetterToggle() const;
        std::string GetMarkerStart() const;
        std::string GetSelectedSubtype() const;
        int GetLengthValue() const;
        NXOpen::TaggedObject* GetSelectedCircleObject() const;
        NXOpen::TaggedObject* GetEffectiveSelectedCircleObject() const;
        void UpdateConditionalBlocks() const;
        void WriteStatusLine(const std::string& text) const;
        void SetStringValue(NXOpen::BlockStyler::UIBlock* block, const std::string& value) const;
        void SetBlockVisibility(NXOpen::BlockStyler::UIBlock* block, bool visible) const;
        void SetBlockEnable(NXOpen::BlockStyler::UIBlock* block, bool enable) const;
        std::string BuildNextMarkerCode(const std::string& current) const;

    private:
        NXOpen::UI* ui_;
        ExcelRuleManager rules_;
        DraftingAnnotationService draftingService_;
        NXOpen::BlockStyler::BlockDialog* dialog_;

        NXOpen::BlockStyler::UIBlock* groupMain_;
        NXOpen::BlockStyler::UIBlock* annotationTypeBlock_;
        NXOpen::BlockStyler::UIBlock* letterToggleBlock_;
        NXOpen::BlockStyler::UIBlock* markerStartBlock_;
        NXOpen::BlockStyler::UIBlock* weldNutSeriesBlock_;
        NXOpen::BlockStyler::UIBlock* pemNutSeriesBlock_;
        NXOpen::BlockStyler::UIBlock* pemScrewSeriesBlock_;
        NXOpen::BlockStyler::UIBlock* pemStudSeriesBlock_;
        NXOpen::BlockStyler::UIBlock* lengthBlock_;
        NXOpen::BlockStyler::UIBlock* circleSelectBlock_;
        NXOpen::BlockStyler::UIBlock* openExcelButton_;
        NXOpen::BlockStyler::UIBlock* refreshRulesButton_;
        std::vector<tag_t> createdAnnotationTags_;
        std::vector<tag_t> highlightedCircleTags_;
        tag_t lastSelectedCurveTag_;
        std::string lastAppliedMarkerCode_;
        bool isInternalUpdate_;
    };
}
