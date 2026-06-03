#pragma once

#include "AnnotationTypes.hpp"
#include "RuleRecord.hpp"

#include <NXOpen/DisplayableObject.hxx>
#include <NXOpen/TaggedObject.hxx>
#include <NXOpen/ugmath.hxx>

#include <string>
#include <vector>

#include <uf_defs.h>

namespace KonBiaoZu
{
    struct DraftingRequest
    {
        AnnotationType type {AnnotationType::Hole};
        double selectedDiameter {};
        bool enableLetterTag {};
        std::string subtype;
        int lengthValue {};
        std::string markerCode;
    };

    struct DraftingCircleMatch
    {
        tag_t curveTag {NULL_TAG};
        NXOpen::Point3d modelCenter {};
        NXOpen::Point3d drawingCenter {};
        double diameter {};
    };

    struct DraftingCircleGroup
    {
        tag_t viewTag {NULL_TAG};
        std::vector<DraftingCircleMatch> matches;
    };

    struct CounterboreCandidate
    {
        bool isValid {};
        tag_t viewTag {NULL_TAG};
        DraftingCircleMatch innerCircle;
        DraftingCircleMatch outerCircle;
    };

    struct CounterboreSideSummary
    {
        int frontCount {};
        int backCount {};
        int unknownCount {};
    };

    enum class CounterboreSide
    {
        Unknown,
        Front,
        Back
    };

    class DraftingAnnotationService
    {
    public:
        [[nodiscard]] double ResolveSelectedDiameter(NXOpen::TaggedObject* selectedObject) const;
        [[nodiscard]] CounterboreCandidate FindCounterboreCandidate(
            NXOpen::TaggedObject* selectedObject,
            double centerTolerance,
            double diameterTolerance) const;
        [[nodiscard]] std::vector<CounterboreCandidate> CollectCounterboreCandidatesInCurrentView(
            NXOpen::TaggedObject* selectedObject,
            double centerTolerance,
            double diameterTolerance) const;
        [[nodiscard]] DraftingCircleGroup CollectCounterboreCirclesInCurrentView(
            NXOpen::TaggedObject* selectedObject,
            double innerDiameter,
            double centerTolerance,
            double diameterTolerance) const;
        [[nodiscard]] int CountSameDiameterInCurrentView(NXOpen::TaggedObject* selectedObject, double diameter, double tolerance) const;
        [[nodiscard]] int CountSameDiameterInWorkPart(double diameter, double tolerance) const;
        [[nodiscard]] bool IsFullCircleObject(NXOpen::TaggedObject* selectedObject) const;
        [[nodiscard]] DraftingCircleGroup CollectSameDiameterCirclesInCurrentView(
            NXOpen::TaggedObject* selectedObject,
            double diameter,
            double tolerance) const;
        void HighlightCircles(const DraftingCircleGroup& group) const;
        tag_t CreateDraftingNote(
            tag_t viewTag,
            const NXOpen::Point3d& drawingPoint,
            const std::string& text,
            double textSize) const;
        tag_t CreateRadialDimensionLabel(
            NXOpen::DisplayableObject* curveObject,
            tag_t viewTag,
            const NXOpen::Point3d& drawingPoint,
            const std::string& beforeText) const;
        [[nodiscard]] std::string BuildCounterboreLabel(double diameter, int sameDiameterCount) const;
        [[nodiscard]] std::string BuildCounterboreComment(double diameter, CounterboreSide side) const;
        [[nodiscard]] std::string BuildCounterboreAnnotation(double diameter, int sameDiameterCount, CounterboreSide side) const;
        [[nodiscard]] CounterboreSide ResolveCounterboreSide(const CounterboreCandidate& candidate) const;
        [[nodiscard]] CounterboreSideSummary SummarizeCounterboreSides(
            const std::vector<CounterboreCandidate>& candidates,
            double outerDiameter,
            double tolerance) const;
        [[nodiscard]] std::string DescribeCounterboreSide(const CounterboreCandidate& candidate) const;
        [[nodiscard]] std::string BuildCounterboreDebugReport(const CounterboreCandidate& candidate) const;
        [[nodiscard]] std::string BuildCounterboreFlatPatternDebugReport(const CounterboreCandidate& candidate) const;
        void ColorCounterboreFlatPatternDebugFaces(const CounterboreCandidate& candidate) const;
        [[nodiscard]] std::string BuildFallbackHoleLabel(
            const DraftingRequest& request,
            int sameDiameterCount) const;
        [[nodiscard]] std::string BuildAnnotationLabel(
            const DraftingRequest& request,
            const RuleRecord& rule,
            int sameDiameterCount) const;
        [[nodiscard]] std::string BuildPreviewText(
            const DraftingRequest& request,
            const std::vector<RuleRecord>& rules,
            int sameDiameterCount) const;
        [[nodiscard]] bool IsDraftingMode() const;
    };
}
