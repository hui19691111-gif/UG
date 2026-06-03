#include "MarkLineBuilder.hpp"

#include <sstream>
#include <stdexcept>

namespace MarkLineBuilder
{
    BuilderService::BuilderService()
    {
    }

    ExecutionSummary BuilderService::Execute(const MarkLineInput& input) const
    {
        ValidateInput(input);

        ExecutionSummary summary = {};
        summary.sourceBodyCount = static_cast<int>(input.sourceBodies.size());
        summary.sourceEdgeCount = static_cast<int>(input.sourceEdges.size());
        summary.targetBodyCount = 0;
        summary.generatedCurveCount = 0;
        summary.generatedGrooveCount = 0;

        std::ostringstream message;
        message << "Feature logic has been cleared."
                << " mark_mode=" << static_cast<int>(input.markLineMode)
                << " output_mode=" << static_cast<int>(input.outputMode)
                << " bodies=" << summary.sourceBodyCount
                << " edges=" << summary.sourceEdgeCount << ".";
        summary.messages.push_back(message.str());

        return summary;
    }

    void BuilderService::ValidateInput(const MarkLineInput& input) const
    {
        if (input.markLineMode == MarkLineMode::SelectedEdges)
        {
            if (input.sourceEdges.empty())
            {
                throw std::runtime_error("Please select at least one edge in edge mode.");
            }
            return;
        }

        if (input.sourceBodies.empty())
        {
            throw std::runtime_error("Please select at least one solid body.");
        }
    }

    std::vector<NXOpen::Body*> BuilderService::ResolveSourceBodies(const MarkLineInput& input) const
    {
        return input.sourceBodies;
    }

    std::vector<BuilderService::ContactTarget> BuilderService::ResolveContactTargets(const MarkLineInput& input) const
    {
        static_cast<void>(input);
        return std::vector<ContactTarget>();
    }

    std::vector<NXOpen::Curve*> BuilderService::BuildProjectedCurves(const ContactTarget& target, const MarkLineInput& input) const
    {
        static_cast<void>(target);
        static_cast<void>(input);
        return std::vector<NXOpen::Curve*>();
    }

    std::vector<NXOpen::Curve*> BuilderService::BuildBodyOutlineCurves(const ContactTarget& target, const MarkLineInput& input) const
    {
        static_cast<void>(target);
        static_cast<void>(input);
        return std::vector<NXOpen::Curve*>();
    }

    std::vector<NXOpen::Curve*> BuilderService::BuildBottomFaceCurves(const ContactTarget& target, const MarkLineInput& input) const
    {
        static_cast<void>(target);
        static_cast<void>(input);
        return std::vector<NXOpen::Curve*>();
    }

    std::vector<NXOpen::Curve*> BuilderService::BuildSelectedEdgeCurves(const ContactTarget& target) const
    {
        static_cast<void>(target);
        return std::vector<NXOpen::Curve*>();
    }

    std::vector<NXOpen::Curve*> BuilderService::KeepLargestOuterLoop(const std::vector<NXOpen::Curve*>& curves) const
    {
        return curves;
    }

    std::vector<NXOpen::Curve*> BuilderService::ApplySegmentRule(const std::vector<NXOpen::Curve*>& curves, const SegmentOptions& options) const
    {
        static_cast<void>(options);
        return curves;
    }

    void BuilderService::CreateCurveMarks(const ContactTarget& target, const std::vector<NXOpen::Curve*>& curves, const MarkLineInput& input, ExecutionSummary& summary) const
    {
        static_cast<void>(target);
        static_cast<void>(curves);
        static_cast<void>(input);
        static_cast<void>(summary);
    }

    void BuilderService::CreateGrooves(const ContactTarget& target, const std::vector<NXOpen::Curve*>& curves, const MarkLineInput& input, ExecutionSummary& summary) const
    {
        static_cast<void>(target);
        static_cast<void>(curves);
        static_cast<void>(input);
        static_cast<void>(summary);
    }

    std::vector<NXOpen::Body*> BuilderService::GetTouchingBodies(NXOpen::Body* sourceBody) const
    {
        static_cast<void>(sourceBody);
        return std::vector<NXOpen::Body*>();
    }

    NXOpen::Face* BuilderService::FindBestContactFace(NXOpen::Body* sourceBody, NXOpen::Body* targetBody, NXOpen::Face** sourceFace) const
    {
        static_cast<void>(sourceBody);
        static_cast<void>(targetBody);
        if (sourceFace != NULL)
        {
            *sourceFace = NULL;
        }
        return NULL;
    }

    bool BuilderService::AreFacesTouching(NXOpen::Face* face1, NXOpen::Face* face2, double tolerance) const
    {
        static_cast<void>(face1);
        static_cast<void>(face2);
        static_cast<void>(tolerance);
        return false;
    }

    double BuilderService::AskFaceArea(NXOpen::Face* face) const
    {
        static_cast<void>(face);
        return 0.0;
    }

    std::string BuilderService::BuildSummaryMessage(const ExecutionSummary& summary) const
    {
        std::ostringstream message;
        message << "Feature logic has been cleared."
                << " bodies=" << summary.sourceBodyCount
                << " edges=" << summary.sourceEdgeCount << ".";
        return message.str();
    }
}
