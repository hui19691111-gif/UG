#include "KonBiaoZuDialog.hpp"

#include <NXOpen/BlockStyler_BlockDialog.hxx>
#include <NXOpen/BlockStyler_PropertyList.hxx>
#include <NXOpen/DisplayableObject.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXMessageBox.hxx>
#include <NXOpen/NXObjectManager.hxx>

#include <Windows.h>
#include <uf_obj.h>
#include <uf_object_types.h>
#include <uf_ui_types.h>
#ifdef CreateDialog
#undef CreateDialog
#endif

#include <optional>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <vector>

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace
{
    std::string GetDialogFilePath()
    {
        char buffer[MAX_PATH] = {};
        const HMODULE module = reinterpret_cast<HMODULE>(&__ImageBase);
        const DWORD length = GetModuleFileNameA(module, buffer, MAX_PATH);
        std::vector<std::filesystem::path> roots;
        if (length > 0 && length < MAX_PATH)
        {
            const std::filesystem::path modulePath(buffer);
            roots.push_back(modulePath.parent_path());
            roots.push_back(modulePath.parent_path().parent_path());
            roots.push_back(modulePath.parent_path().parent_path().parent_path());
        }

        std::error_code currentPathError;
        const std::filesystem::path currentPath = std::filesystem::current_path(currentPathError);
        if (!currentPath.empty())
        {
            roots.push_back(currentPath);
        }

        const std::vector<std::filesystem::path> candidates = {
            "KonBiaoZu.dlx",
            "application/KonBiaoZu.dlx",
            "dialog/KonBiaoZu.dlx",
            "KonBiaoZu/dialog/KonBiaoZu.dlx"
        };

        for (const std::filesystem::path& root : roots)
        {
            for (const std::filesystem::path& candidate : candidates)
            {
                const std::filesystem::path fullPath = root / candidate;
                if (std::filesystem::exists(fullPath))
                {
                    return fullPath.string();
                }
            }
        }

        return {};
    }

    std::string Utf8ToSystem(const std::string& utf8)
    {
        if (utf8.empty())
        {
            return {};
        }

        const int wideSize = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
        if (wideSize <= 0)
        {
            return utf8;
        }

        std::wstring wide(static_cast<size_t>(wideSize), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, wide.data(), wideSize);

        const int ansiSize = WideCharToMultiByte(CP_ACP, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (ansiSize <= 0)
        {
            return utf8;
        }

        std::string ansi(static_cast<size_t>(ansiSize), '\0');
        WideCharToMultiByte(CP_ACP, 0, wide.c_str(), -1, ansi.data(), ansiSize, nullptr, nullptr);
        if (!ansi.empty() && ansi.back() == '\0')
        {
            ansi.pop_back();
        }
        return ansi;
    }

    std::string Utf8ToSystem(const char* utf8)
    {
        return Utf8ToSystem(std::string(utf8));
    }

    double GetPreviewDiameter(KonBiaoZu::AnnotationType type)
    {
        using KonBiaoZu::AnnotationType;

        switch (type)
        {
        case AnnotationType::Thread:
            return 5.0;
        case AnnotationType::WeldNut:
            return 5.53;
        case AnnotationType::PemNut:
        case AnnotationType::PemScrew:
        case AnnotationType::PemStud:
            return 5.41;
        case AnnotationType::Counterbore:
            return 8.0;
        case AnnotationType::Hole:
        default:
            return 5.0;
        }
    }

    std::string ReadEnumLabel(NXOpen::BlockStyler::UIBlock* block, int value, const std::vector<std::string>& labels)
    {
        if (block == nullptr || value < 0 || static_cast<size_t>(value) >= labels.size())
        {
            return {};
        }
        return labels[static_cast<size_t>(value)];
    }
}

namespace KonBiaoZu
{
    KonBiaoZuDialog::KonBiaoZuDialog()
        : ui_(NXOpen::UI::GetUI()),
          dialog_(nullptr),
          groupMain_(nullptr),
          annotationTypeBlock_(nullptr),
          letterToggleBlock_(nullptr),
          markerStartBlock_(nullptr),
          weldNutSeriesBlock_(nullptr),
          pemNutSeriesBlock_(nullptr),
          pemScrewSeriesBlock_(nullptr),
          pemStudSeriesBlock_(nullptr),
          lengthBlock_(nullptr),
          circleSelectBlock_(nullptr),
          openExcelButton_(nullptr),
          refreshRulesButton_(nullptr),
          createdAnnotationTags_(),
          highlightedCircleTags_(),
          lastSelectedCurveTag_(NULL_TAG),
          lastAppliedMarkerCode_(),
          isInternalUpdate_(false)
    {
        std::string errorMessage;
        rules_.LoadRules(errorMessage);

        dialog_ = ui_->CreateDialog(GetDialogFilePath().c_str());
        dialog_->AddInitializeHandler(NXOpen::make_callback(this, &KonBiaoZuDialog::initialize_cb));
        dialog_->AddUpdateHandler(NXOpen::make_callback(this, &KonBiaoZuDialog::update_cb));
        dialog_->AddApplyHandler(NXOpen::make_callback(this, &KonBiaoZuDialog::apply_cb));
        dialog_->AddOkHandler(NXOpen::make_callback(this, &KonBiaoZuDialog::ok_cb));
        dialog_->AddCancelHandler(NXOpen::make_callback(this, &KonBiaoZuDialog::cancel_cb));
    }

    KonBiaoZuDialog::~KonBiaoZuDialog()
    {
        rules_.CloseEditor();
        ClearHighlightedCircles();
        if (dialog_ != nullptr)
        {
            delete dialog_;
            dialog_ = nullptr;
        }
    }

    NXOpen::BlockStyler::BlockDialog::DialogResponse KonBiaoZuDialog::Launch()
    {
        return dialog_->Launch();
    }

    void KonBiaoZuDialog::initialize_cb()
    {
        groupMain_ = dialog_->TopBlock()->FindBlock("group_main");
        annotationTypeBlock_ = dialog_->TopBlock()->FindBlock("annotation_type");
        letterToggleBlock_ = dialog_->TopBlock()->FindBlock("letter_toggle");
        markerStartBlock_ = dialog_->TopBlock()->FindBlock("marker_start");
        weldNutSeriesBlock_ = dialog_->TopBlock()->FindBlock("weld_nut_series");
        pemNutSeriesBlock_ = dialog_->TopBlock()->FindBlock("pem_nut_series");
        pemScrewSeriesBlock_ = dialog_->TopBlock()->FindBlock("pem_screw_series");
        pemStudSeriesBlock_ = dialog_->TopBlock()->FindBlock("pem_stud_series");
        lengthBlock_ = dialog_->TopBlock()->FindBlock("length_value");
        circleSelectBlock_ = dialog_->TopBlock()->FindBlock("circle_select");
        openExcelButton_ = dialog_->TopBlock()->FindBlock("open_excel");
        refreshRulesButton_ = dialog_->TopBlock()->FindBlock("refresh_rules");
        NXOpen::Selection::SelectionAction action = NXOpen::Selection::SelectionActionClearAndEnableSpecific;
        std::vector<NXOpen::Selection::MaskTriple> selectionMaskArray;
        selectionMaskArray.emplace_back(UF_circle_type, UF_all_subtype, 0);
        selectionMaskArray.emplace_back(UF_conic_type, UF_all_subtype, 0);
        NXOpen::BlockStyler::PropertyList* circleSelectProperties = circleSelectBlock_->GetProperties();
        circleSelectProperties->SetSelectionFilter("SelectionFilter", action, selectionMaskArray);
        circleSelectProperties->SetLogical("AutomaticProgression", true);
        delete circleSelectProperties;
        UpdateConditionalBlocks();
    }

    int KonBiaoZuDialog::update_cb(NXOpen::BlockStyler::UIBlock* block)
    {
        try
        {
            if (block == openExcelButton_)
            {
                std::string errorMessage;
                if (!rules_.OpenWorkbook(errorMessage))
                {
                    ui_->NXMessageBox()->Show("KonBiaoZu", NXOpen::NXMessageBox::DialogTypeError, Utf8ToSystem(errorMessage));
                }
                else
                {
                    WriteStatusLine(Utf8ToSystem("已打开规则配置文件。"));
                }
            }

            if (block == refreshRulesButton_)
            {
                std::string errorMessage;
                if (!rules_.RefreshRules(errorMessage))
                {
                    ui_->NXMessageBox()->Show("KonBiaoZu", NXOpen::NXMessageBox::DialogTypeError, Utf8ToSystem(errorMessage));
                }
                else
                {
                    ui_->NXMessageBox()->Show("KonBiaoZu", NXOpen::NXMessageBox::DialogTypeInformation, Utf8ToSystem("规则数据已更新。"));
                }
                return 0;
            }

            UpdateConditionalBlocks();

            if (!isInternalUpdate_ && block == circleSelectBlock_)
            {
                NXOpen::TaggedObject* selectedObject = GetSelectedCircleObject();
                if (selectedObject != nullptr && !draftingService_.IsFullCircleObject(selectedObject))
                {
                    NXOpen::BlockStyler::PropertyList* properties = circleSelectBlock_->GetProperties();
                    properties->SetTaggedObjectVector("SelectedObjects", std::vector<NXOpen::TaggedObject*> {});
                    delete properties;
                    return 0;
                }

                return ExecuteAnnotation(true);
            }

            if (!isInternalUpdate_ &&
                block != openExcelButton_ &&
                block != refreshRulesButton_ &&
                lastSelectedCurveTag_ != NULL_TAG &&
                !createdAnnotationTags_.empty())
            {
                const bool useMarkerInputForCurrentSelection = (block == markerStartBlock_);
                return ExecuteAnnotation(false, useMarkerInputForCurrentSelection);
            }
        }
        catch (const NXOpen::NXException& ex)
        {
            ui_->NXMessageBox()->Show("KonBiaoZu", NXOpen::NXMessageBox::DialogTypeError, ex.Message());
            return ex.ErrorCode();
        }

        return 0;
    }

    int KonBiaoZuDialog::apply_cb()
    {
        return 0;
    }

    int KonBiaoZuDialog::ExecuteAnnotation(bool advanceMarker, bool useMarkerInputForCurrentSelection)
    {
        AnnotationType type = GetSelectedType();
        const std::string subtype = GetSelectedSubtype();
        const int lengthValue = GetLengthValue();
        const bool enableLetterTag = GetLetterToggle();
        const std::string markerStart = GetMarkerStart();
        NXOpen::TaggedObject* explicitSelectedObject = GetSelectedCircleObject();
        NXOpen::TaggedObject* selectedObject = explicitSelectedObject != nullptr ? explicitSelectedObject : GetEffectiveSelectedCircleObject();
        if (selectedObject == nullptr)
        {
            return 0;
        }

        const bool isFreshCircleSelection =
            explicitSelectedObject != nullptr &&
            (selectedObject->Tag() != lastSelectedCurveTag_ || createdAnnotationTags_.empty());

        std::string effectiveMarkerCode = markerStart;
        if (!lastAppliedMarkerCode_.empty() &&
            selectedObject->Tag() == lastSelectedCurveTag_ &&
            !isFreshCircleSelection &&
            !useMarkerInputForCurrentSelection)
        {
            effectiveMarkerCode = lastAppliedMarkerCode_;
        }

        double selectedDiameter = draftingService_.ResolveSelectedDiameter(selectedObject);
        if (selectedDiameter <= 0.0)
        {
            selectedDiameter = GetPreviewDiameter(type);
        }

        CounterboreCandidate counterboreCandidate;
        counterboreCandidate = draftingService_.FindCounterboreCandidate(selectedObject, 0.02, 0.02);
        if (counterboreCandidate.isValid)
        {
            type = AnnotationType::Counterbore;
        }

        if (type == AnnotationType::Counterbore)
        {
            if (counterboreCandidate.isValid)
            {
                selectedDiameter = counterboreCandidate.innerCircle.diameter;
            }
        }

        int sameDiameterCount = draftingService_.CountSameDiameterInCurrentView(selectedObject, selectedDiameter, 0.02);
        if (sameDiameterCount <= 0)
        {
            sameDiameterCount = draftingService_.CountSameDiameterInWorkPart(selectedDiameter, 0.02);
        }

        DraftingRequest request;
        request.type = type;
        request.selectedDiameter = selectedDiameter;
        request.enableLetterTag = enableLetterTag;
        request.subtype = subtype;
        request.lengthValue = lengthValue;
        request.markerCode = effectiveMarkerCode;

        const std::vector<RuleRecord> rules = rules_.FindRules(type, subtype, selectedDiameter, lengthValue, 0.02);
        DraftingCircleGroup circleGroup;
        if (type == AnnotationType::Counterbore && counterboreCandidate.isValid)
        {
            circleGroup = draftingService_.CollectCounterboreCirclesInCurrentView(selectedObject, selectedDiameter, 0.02, 0.02);
        }
        else
        {
            circleGroup = draftingService_.CollectSameDiameterCirclesInCurrentView(selectedObject, selectedDiameter, 0.02);
        }

        const tag_t preferredAnchorTag =
            (type == AnnotationType::Counterbore && counterboreCandidate.isValid)
                ? counterboreCandidate.innerCircle.curveTag
                : selectedObject->Tag();
        auto anchorIt = std::find_if(
            circleGroup.matches.begin(),
            circleGroup.matches.end(),
            [preferredAnchorTag](const DraftingCircleMatch& match) {
                return match.curveTag == preferredAnchorTag;
            });
        if (anchorIt != circleGroup.matches.end() && anchorIt != circleGroup.matches.begin())
        {
            std::iter_swap(circleGroup.matches.begin(), anchorIt);
        }

        sameDiameterCount = static_cast<int>(circleGroup.matches.size());

        if (!draftingService_.IsDraftingMode())
        {
            ui_->NXMessageBox()->Show(
                "KonBiaoZu",
                NXOpen::NXMessageBox::DialogTypeError,
                Utf8ToSystem("\xE5\xBD\x93\xE5\x89\x8D\xE4\xB8\x8D\xE5\x9C\xA8 Drafting \xE5\x88\xB6\xE5\x9B\xBE\xE7\x8E\xAF\xE5\xA2\x83\xEF\xBC\x8C\xE6\x9A\x82\xE4\xB8\x8D\xE6\x89\xA7\xE8\xA1\x8C\xE6\xA0\x87\xE6\xB3\xA8\xE3\x80\x82"));
            return 1;
        }

        const bool isDynamicCounterbore = (type == AnnotationType::Counterbore && counterboreCandidate.isValid);
        CounterboreSide counterboreSide = CounterboreSide::Unknown;
        CounterboreSideSummary sideSummary;
        std::vector<CounterboreCandidate> sameViewCounterbores;
        if (isDynamicCounterbore)
        {
            counterboreSide = draftingService_.ResolveCounterboreSide(counterboreCandidate);

            sameViewCounterbores =
                draftingService_.CollectCounterboreCandidatesInCurrentView(selectedObject, 0.02, 0.02);
            sideSummary =
                draftingService_.SummarizeCounterboreSides(sameViewCounterbores, counterboreCandidate.outerCircle.diameter, 0.02);

            if (false)
            {
                std::vector<NXOpen::NXString> messages;
                messages.emplace_back(Utf8ToSystem("当前同大径沉孔同时存在正反面，请手动选择本次标注方向。").c_str());

                std::ostringstream diameterLine;
                diameterLine << "沉孔大径: Φ" << counterboreCandidate.outerCircle.diameter;
                messages.emplace_back(Utf8ToSystem(diameterLine.str()).c_str());

                std::ostringstream frontLine;
                frontLine << "正面: " << sideSummary.frontCount << " 个";
                messages.emplace_back(Utf8ToSystem(frontLine.str()).c_str());

                std::ostringstream backLine;
                backLine << "反面: " << sideSummary.backCount << " 个";
                messages.emplace_back(Utf8ToSystem(backLine.str()).c_str());

                if (sideSummary.unknownCount > 0)
                {
                    std::ostringstream unknownLine;
                    unknownLine << "未知: " << sideSummary.unknownCount << " 个";
                    messages.emplace_back(Utf8ToSystem(unknownLine.str()).c_str());
                }

                messages.emplace_back(Utf8ToSystem("选择“是”按正面标注，选择“否”按反面标注。").c_str());

                const int answer = ui_->NXMessageBox()->Show(
                    "KonBiaoZu",
                    NXOpen::NXMessageBox::DialogTypeQuestion,
                    messages);
                if (answer == 1)
                {
                    counterboreSide = CounterboreSide::Front;
                }
                else if (answer == 2)
                {
                    counterboreSide = CounterboreSide::Back;
                }
            }

            if (false) ui_->NXMessageBox()->Show(
                "KonBiaoZu 调试",
                NXOpen::NXMessageBox::DialogTypeInformation,
                Utf8ToSystem(draftingService_.BuildCounterboreFlatPatternDebugReport(counterboreCandidate)));
        }
        const bool hasRule = isDynamicCounterbore || !rules.empty();
        const bool splitCounterboreBySide =
            isDynamicCounterbore &&
            sideSummary.frontCount > 0 &&
            sideSummary.backCount > 0;
        const auto buildMainLabel = [&](int count, CounterboreSide side) {
            if (type == AnnotationType::Counterbore)
            {
                if (!rules.empty())
                {
                    return draftingService_.BuildAnnotationLabel(request, rules.front(), count);
                }
                return draftingService_.BuildCounterboreAnnotation(selectedDiameter, count, side);
            }

            if (hasRule)
            {
                return draftingService_.BuildAnnotationLabel(request, rules.front(), count);
            }
            return draftingService_.BuildFallbackHoleLabel(request, count);
        };
        const std::string finalLabel = buildMainLabel(sameDiameterCount, counterboreSide);
        const std::string annotationText = finalLabel;

        const bool replaceCurrentAnnotations =
            (!advanceMarker) ||
            (selectedObject->Tag() == lastSelectedCurveTag_ && !createdAnnotationTags_.empty());
        if (replaceCurrentAnnotations)
        {
            DeleteTrackedAnnotations();
        }
        else
        {
            createdAnnotationTags_.clear();
        }
        ClearHighlightedCircles();
        draftingService_.HighlightCircles(circleGroup);
        for (const DraftingCircleMatch& match : circleGroup.matches)
        {
            highlightedCircleTags_.push_back(match.curveTag);
        }

        auto buildCounterboreSideGroup = [&](CounterboreSide side) {
            DraftingCircleGroup group;
            group.viewTag = circleGroup.viewTag;
            if (!isDynamicCounterbore)
            {
                return group;
            }

            for (const CounterboreCandidate& candidate : sameViewCounterbores)
            {
                if (!candidate.isValid)
                {
                    continue;
                }

                if (std::abs(candidate.outerCircle.diameter - counterboreCandidate.outerCircle.diameter) > 0.02)
                {
                    continue;
                }

                if (draftingService_.ResolveCounterboreSide(candidate) != side)
                {
                    continue;
                }

                group.matches.push_back(candidate.innerCircle);
            }

            auto selectedIt = std::find_if(
                group.matches.begin(),
                group.matches.end(),
                [preferredAnchorTag](const DraftingCircleMatch& match) {
                    return match.curveTag == preferredAnchorTag;
                });
            if (selectedIt != group.matches.end() && selectedIt != group.matches.begin())
            {
                std::iter_swap(group.matches.begin(), selectedIt);
            }

            return group;
        };

        auto buildMarkedLabel = [](const std::string& markerCode, const std::string& label) {
            if (markerCode.empty())
            {
                return label;
            }
            return markerCode + " " + label;
        };

        int createdNotes = 0;
        std::string nextMarkerValue = markerStart;
        if (splitCounterboreBySide)
        {
            const std::string frontMarker = effectiveMarkerCode.empty() ? "A" : effectiveMarkerCode;
            const std::string backMarker = BuildNextMarkerCode(frontMarker);
            const DraftingCircleGroup frontGroup = buildCounterboreSideGroup(CounterboreSide::Front);
            const DraftingCircleGroup backGroup = buildCounterboreSideGroup(CounterboreSide::Back);

            auto createSideAnnotations = [&](const DraftingCircleGroup& group,
                                             const std::string& markerCode,
                                             CounterboreSide side,
                                             double offsetY) {
                if (group.viewTag == NULL_TAG || group.matches.empty())
                {
                    return;
                }

                const std::string sideLabel = buildMainLabel(static_cast<int>(group.matches.size()), side);
                const std::string markedLabel = buildMarkedLabel(markerCode, sideLabel);

                NXOpen::Point3d mainNotePoint = group.matches.front().drawingCenter;
                mainNotePoint.X += 8.0;
                mainNotePoint.Y += offsetY;
                NXOpen::DisplayableObject* selectedCurve =
                    dynamic_cast<NXOpen::DisplayableObject*>(NXOpen::NXObjectManager::Get(group.matches.front().curveTag));
                const tag_t mainTag =
                    draftingService_.CreateRadialDimensionLabel(selectedCurve, group.viewTag, mainNotePoint, markedLabel);
                if (mainTag != NULL_TAG)
                {
                    ++createdNotes;
                    createdAnnotationTags_.push_back(mainTag);
                }

                for (const DraftingCircleMatch& match : group.matches)
                {
                    NXOpen::Point3d markerPoint = match.drawingCenter;
                    markerPoint.X += 1.5;
                    markerPoint.Y += 1.5;
                    const tag_t noteTag =
                        draftingService_.CreateDraftingNote(group.viewTag, markerPoint, markerCode, 2.0);
                    if (noteTag != NULL_TAG)
                    {
                        ++createdNotes;
                        createdAnnotationTags_.push_back(noteTag);
                    }
                }
            };

            createSideAnnotations(frontGroup, frontMarker, CounterboreSide::Front, 8.0);
            createSideAnnotations(backGroup, backMarker, CounterboreSide::Back, -8.0);
            effectiveMarkerCode = frontMarker;
            nextMarkerValue = BuildNextMarkerCode(backMarker);
        }
        else if (circleGroup.viewTag != NULL_TAG && !circleGroup.matches.empty())
        {
            NXOpen::Point3d mainNotePoint = circleGroup.matches.front().drawingCenter;
            mainNotePoint.X += 8.0;
            mainNotePoint.Y += 8.0;
            NXOpen::DisplayableObject* selectedCurve =
                dynamic_cast<NXOpen::DisplayableObject*>(NXOpen::NXObjectManager::Get(circleGroup.matches.front().curveTag));
            const tag_t mainTag =
                draftingService_.CreateRadialDimensionLabel(selectedCurve, circleGroup.viewTag, mainNotePoint, annotationText);
            if (mainTag != NULL_TAG)
            {
                ++createdNotes;
                createdAnnotationTags_.push_back(mainTag);
            }

            if (enableLetterTag)
            {
                for (const DraftingCircleMatch& match : circleGroup.matches)
                {
                    NXOpen::Point3d markerPoint = match.drawingCenter;
                    markerPoint.X += 1.5;
                    markerPoint.Y += 1.5;
                    const tag_t noteTag =
                        draftingService_.CreateDraftingNote(circleGroup.viewTag, markerPoint, effectiveMarkerCode, 2.0);
                    if (noteTag != NULL_TAG)
                    {
                        ++createdNotes;
                        createdAnnotationTags_.push_back(noteTag);
                    }
                }
            }

            nextMarkerValue = enableLetterTag ? BuildNextMarkerCode(markerStart) : markerStart;
        }
        lastSelectedCurveTag_ = selectedObject->Tag();
        lastAppliedMarkerCode_ = effectiveMarkerCode;

        std::ostringstream status;
        status << "图纸标注已更新: " << finalLabel;
        if (false)
        {
            status << " | " << draftingService_.DescribeCounterboreSide(counterboreCandidate);
        }
        if (!hasRule)
        {
            status << " | 已回退为孔标注";
        }
        if (createdNotes <= 0)
        {
            status << " | 未创建到图纸对象";
        }
        WriteStatusLine(status.str());

        if ((enableLetterTag || splitCounterboreBySide) && advanceMarker && isFreshCircleSelection)
        {
            isInternalUpdate_ = true;
            SetStringValue(markerStartBlock_, nextMarkerValue);
            isInternalUpdate_ = false;
        }

        if (advanceMarker && isFreshCircleSelection && circleSelectBlock_ != nullptr)
        {
            isInternalUpdate_ = true;
            NXOpen::BlockStyler::PropertyList* properties = circleSelectBlock_->GetProperties();
            properties->SetTaggedObjectVector("SelectedObjects", std::vector<NXOpen::TaggedObject*> {});
            delete properties;
            isInternalUpdate_ = false;
        }

        return 0;
    }

    int KonBiaoZuDialog::ok_cb()
    {
        ClearHighlightedCircles();
        return 0;
    }

    int KonBiaoZuDialog::cancel_cb()
    {
        ClearHighlightedCircles();
        return 0;
    }

    void KonBiaoZuDialog::DeleteTrackedAnnotations()
    {
        for (tag_t tag : createdAnnotationTags_)
        {
            if (tag != NULL_TAG)
            {
                UF_OBJ_delete_object(tag);
            }
        }
        createdAnnotationTags_.clear();
    }

    void KonBiaoZuDialog::ClearHighlightedCircles()
    {
        for (tag_t tag : highlightedCircleTags_)
        {
            if (tag == NULL_TAG)
            {
                continue;
            }

            NXOpen::DisplayableObject* object =
                dynamic_cast<NXOpen::DisplayableObject*>(NXOpen::NXObjectManager::Get(tag));
            if (object != nullptr)
            {
                object->Unhighlight();
            }
        }
        highlightedCircleTags_.clear();
    }

    AnnotationType KonBiaoZuDialog::GetSelectedType() const
    {
        NXOpen::BlockStyler::PropertyList* properties = annotationTypeBlock_->GetProperties();
        const int value = properties->GetEnum("Value");
        delete properties;

        switch (value)
        {
        case 1:
            return AnnotationType::Thread;
        case 2:
            return AnnotationType::WeldNut;
        case 3:
            return AnnotationType::PemNut;
        case 4:
            return AnnotationType::PemScrew;
        case 5:
            return AnnotationType::PemStud;
        case 6:
            return AnnotationType::Counterbore;
        default:
            return AnnotationType::Hole;
        }
    }

    bool KonBiaoZuDialog::GetLetterToggle() const
    {
        NXOpen::BlockStyler::PropertyList* properties = letterToggleBlock_->GetProperties();
        const bool value = properties->GetLogical("Value");
        delete properties;
        return value;
    }

    std::string KonBiaoZuDialog::GetMarkerStart() const
    {
        if (markerStartBlock_ == nullptr)
        {
            return "A";
        }

        NXOpen::BlockStyler::PropertyList* properties = markerStartBlock_->GetProperties();
        const NXOpen::NXString nxValue = properties->GetString("Value");
        std::string value = nxValue.GetText();
        delete properties;

        if (value.empty())
        {
            return "A";
        }

        return value;
    }

    std::string KonBiaoZuDialog::GetSelectedSubtype() const
    {
        const AnnotationType type = GetSelectedType();
        NXOpen::BlockStyler::UIBlock* block = nullptr;
        std::vector<std::string> labels;

        if (type == AnnotationType::WeldNut)
        {
            block = weldNutSeriesBlock_;
            labels = {"WN", "WNS"};
        }
        else if (type == AnnotationType::PemNut)
        {
            block = pemNutSeriesBlock_;
            labels = {"S", "CLS", "SP"};
        }
        else if (type == AnnotationType::PemScrew)
        {
            block = pemScrewSeriesBlock_;
            labels = {"FH", "FHS"};
        }
        else if (type == AnnotationType::PemStud)
        {
            block = pemStudSeriesBlock_;
            labels = {"SO", "SOS", "BSO", "BSOS"};
        }

        if (block == nullptr)
        {
            return {};
        }

        NXOpen::BlockStyler::PropertyList* properties = block->GetProperties();
        const int value = properties->GetEnum("Value");
        delete properties;
        return ReadEnumLabel(block, value, labels);
    }

    int KonBiaoZuDialog::GetLengthValue() const
    {
        const AnnotationType type = GetSelectedType();
        if (type != AnnotationType::PemScrew && type != AnnotationType::PemStud)
        {
            return 0;
        }

        NXOpen::BlockStyler::PropertyList* properties = lengthBlock_->GetProperties();
        const int value = properties->GetInteger("Value");
        delete properties;
        return value;
    }

    NXOpen::TaggedObject* KonBiaoZuDialog::GetSelectedCircleObject() const
    {
        if (circleSelectBlock_ == nullptr)
        {
            return nullptr;
        }

        NXOpen::BlockStyler::PropertyList* properties = circleSelectBlock_->GetProperties();
        std::vector<NXOpen::TaggedObject*> selectedObjects;
        try
        {
            selectedObjects = properties->GetTaggedObjectVector("SelectedObjects");
        }
        catch (...)
        {
        }

        if (selectedObjects.empty())
        {
            try
            {
                NXOpen::TaggedObject* selectedObject = properties->GetTaggedObject("SelectedObject");
                delete properties;
                return selectedObject;
            }
            catch (...)
            {
            }
        }

        delete properties;
        return selectedObjects.empty() ? nullptr : selectedObjects.front();
    }

    NXOpen::TaggedObject* KonBiaoZuDialog::GetEffectiveSelectedCircleObject() const
    {
        NXOpen::TaggedObject* selectedObject = GetSelectedCircleObject();
        if (selectedObject != nullptr)
        {
            return selectedObject;
        }

        if (lastSelectedCurveTag_ != NULL_TAG)
        {
            return dynamic_cast<NXOpen::TaggedObject*>(NXOpen::NXObjectManager::Get(lastSelectedCurveTag_));
        }

        return nullptr;
    }


    void KonBiaoZuDialog::UpdateConditionalBlocks() const
    {
        const AnnotationType type = GetSelectedType();
        SetBlockEnable(markerStartBlock_, GetLetterToggle());
        SetBlockVisibility(weldNutSeriesBlock_, type == AnnotationType::WeldNut);
        SetBlockVisibility(pemNutSeriesBlock_, type == AnnotationType::PemNut);
        SetBlockVisibility(pemScrewSeriesBlock_, type == AnnotationType::PemScrew);
        SetBlockVisibility(pemStudSeriesBlock_, type == AnnotationType::PemStud);
        SetBlockVisibility(lengthBlock_, type == AnnotationType::PemScrew || type == AnnotationType::PemStud);
    }

    void KonBiaoZuDialog::WriteStatusLine(const std::string& text) const
    {
        (void)text;
    }

    void KonBiaoZuDialog::SetStringValue(NXOpen::BlockStyler::UIBlock* block, const std::string& value) const
    {
        NXOpen::BlockStyler::PropertyList* properties = block->GetProperties();
        properties->SetString("Value", Utf8ToSystem(value));
        delete properties;
    }

    void KonBiaoZuDialog::SetBlockVisibility(NXOpen::BlockStyler::UIBlock* block, bool visible) const
    {
        if (block == nullptr)
        {
            return;
        }

        NXOpen::BlockStyler::PropertyList* properties = block->GetProperties();
        properties->SetLogical("Show", visible);
        delete properties;
    }

    void KonBiaoZuDialog::SetBlockEnable(NXOpen::BlockStyler::UIBlock* block, bool enable) const
    {
        if (block == nullptr)
        {
            return;
        }

        NXOpen::BlockStyler::PropertyList* properties = block->GetProperties();
        properties->SetLogical("Enable", enable);
        delete properties;
    }

    std::string KonBiaoZuDialog::BuildNextMarkerCode(const std::string& current) const
    {
        if (current.empty())
        {
            return "A";
        }

        std::string next = current;
        char& tail = next.back();

        if (tail >= 'A' && tail < 'Z')
        {
            ++tail;
            return next;
        }
        if (tail == 'Z')
        {
            next.push_back('A');
            return next;
        }
        if (tail >= 'a' && tail < 'z')
        {
            ++tail;
            return next;
        }
        if (tail == 'z')
        {
            next.push_back('a');
            return next;
        }
        if (tail >= '0' && tail < '9')
        {
            ++tail;
            return next;
        }

        return current + "1";
    }
}
