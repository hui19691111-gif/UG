#ifndef BIAOJIXIAN_MARK_LINE_BUILDER_HPP
#define BIAOJIXIAN_MARK_LINE_BUILDER_HPP

#include <string>
#include <vector>
#include <NXOpen/Body.hxx>
#include <NXOpen/BodyCollection.hxx>
#include <NXOpen/Curve.hxx>
#include <NXOpen/Edge.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/TaggedObject.hxx>

namespace MarkLineBuilder
{
    enum class MarkLineMode
    {
        OuterContour = 0,
        BottomFace = 1,
        SelectedEdges = 2
    };

    enum class MarkOutputMode
    {
        Curve = 0,
        Groove = 1
    };

    enum class ShortCurveRule
    {
        DeleteShortestEdge = 0,
        BreakShortestEdgeMiddle = 1
    };

    enum class CurveLayerMode
    {
        FollowSourceBody = 0,
        Custom = 1
    };

    enum class CurveLineFont
    {
        Solid = 0,
        Dashed = 1,
        Center = 2,
        Phantom = 3
    };

    struct SegmentOptions
    {
        double segmentLength;
        double noBreakThreshold;
        double middleThreshold1;
        double middleThreshold2;
        double middleThreshold3;
        int maxMiddleSegments;
        ShortCurveRule shortCurveRule;
    };

    struct DisplayOptions
    {
        CurveLayerMode layerMode;
        int customLayer;
        int colorIndex;
        CurveLineFont lineFont;
    };

    struct GrooveOptions
    {
        double width;
        double depth;
        double offset;
        double tolerance;
    };

    struct MarkLineInput
    {
        MarkLineMode markLineMode;
        MarkOutputMode outputMode;
        SegmentOptions segmentOptions;
        DisplayOptions displayOptions;
        GrooveOptions grooveOptions;
        std::vector<NXOpen::Body*> sourceBodies;
        std::vector<NXOpen::Edge*> sourceEdges;
    };

    struct ExecutionSummary
    {
        int sourceBodyCount;
        int sourceEdgeCount;
        int targetBodyCount;
        int generatedCurveCount;
        int generatedGrooveCount;
        std::vector<std::string> messages;
    };

    class BuilderService
    {
    public:
        BuilderService();
        ExecutionSummary Execute(const MarkLineInput& input) const;

    private:
        struct ContactTarget
        {
            NXOpen::Body* sourceBody;
            NXOpen::Body* targetBody;
            NXOpen::Face* sourceContactFace;
            NXOpen::Face* targetContactFace;
            std::vector<NXOpen::Edge*> selectedEdges;
        };

        void ValidateInput(const MarkLineInput& input) const;
        std::vector<NXOpen::Body*> ResolveSourceBodies(const MarkLineInput& input) const;
        std::vector<ContactTarget> ResolveContactTargets(const MarkLineInput& input) const;
        std::vector<NXOpen::Curve*> BuildProjectedCurves(const ContactTarget& target, const MarkLineInput& input) const;
        std::vector<NXOpen::Curve*> BuildBodyOutlineCurves(const ContactTarget& target, const MarkLineInput& input) const;
        std::vector<NXOpen::Curve*> BuildBottomFaceCurves(const ContactTarget& target, const MarkLineInput& input) const;
        std::vector<NXOpen::Curve*> BuildSelectedEdgeCurves(const ContactTarget& target) const;
        std::vector<NXOpen::Curve*> KeepLargestOuterLoop(const std::vector<NXOpen::Curve*>& curves) const;
        std::vector<NXOpen::Curve*> ApplySegmentRule(const std::vector<NXOpen::Curve*>& curves, const SegmentOptions& options) const;
        void CreateCurveMarks(const ContactTarget& target, const std::vector<NXOpen::Curve*>& curves, const MarkLineInput& input, ExecutionSummary& summary) const;
        void CreateGrooves(const ContactTarget& target, const std::vector<NXOpen::Curve*>& curves, const MarkLineInput& input, ExecutionSummary& summary) const;
        std::vector<NXOpen::Body*> GetTouchingBodies(NXOpen::Body* sourceBody) const;
        NXOpen::Face* FindBestContactFace(NXOpen::Body* sourceBody, NXOpen::Body* targetBody, NXOpen::Face** sourceFace) const;
        bool AreFacesTouching(NXOpen::Face* face1, NXOpen::Face* face2, double tolerance) const;
        double AskFaceArea(NXOpen::Face* face) const;
        std::string BuildSummaryMessage(const ExecutionSummary& summary) const;
    };
}

#endif
