#pragma once

#include <NXOpen/BlockStyler_BlockDialog.hxx>
#include <NXOpen/BlockStyler_UIBlock.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/UI.hxx>
#include <NXOpen/NXObject.hxx>

#include <uf_defs.h>

#include <string>
#include <utility>
#include <vector>

namespace NXOpen
{
class Body;
class Edge;
class Face;
}

class KonFanLaLiaoDialog
{
public:
    KonFanLaLiaoDialog();
    ~KonFanLaLiaoDialog();

    NXOpen::BlockStyler::BlockDialog::DialogResponse Launch();

private:
    struct AnalysisResult
    {
        struct SlotCandidate
        {
            NXOpen::Body* body = nullptr;
            NXOpen::Face* bendFace = nullptr;
            NXOpen::Face* referenceFace = nullptr;
            NXOpen::Edge* tangentEdge = nullptr;
            NXOpen::Point3d startPoint;
            NXOpen::Point3d bendPoint;
            NXOpen::Point3d holeCenter;
            NXOpen::Point3d slotPositionPoint;
            bool hasSlotPosition = false;
            double automaticSlotLength = 0.0;
            NXOpen::Vector3d carrierNormal;
        };
        int bodyCount = 0;
        int bendCount = 0;
        int largeArcExcludedCount = 0;
        int bendEdgeCount = 0;
        int holeCount = 0;
        int riskHoleCount = 0;
        std::vector<NXOpen::Face*> riskFaces;
        std::vector<SlotCandidate> slotCandidates;
    };

    void initialize_cb();
    void dialogShown_cb();
    int update_cb(NXOpen::BlockStyler::UIBlock* block);
    int apply_cb();
    int ok_cb();
    int cancel_cb();

    std::string DialogPath() const;
    std::vector<NXOpen::Body*> TargetBodies() const;
    double DoubleValue(NXOpen::BlockStyler::UIBlock* block) const;
    void SetDoubleValue(NXOpen::BlockStyler::UIBlock* block, double value) const;
    void SetStatus(const std::string& text) const;
    void LoadState() const;
    void SaveState() const;

    bool IsBendFace(NXOpen::Face* face) const;
    bool MinimumDistance(tag_t first, tag_t second, double& distance,
                         NXOpen::Point3d* firstPoint = nullptr,
                         NXOpen::Point3d* secondPoint = nullptr) const;
    AnalysisResult Analyze() const;
    int CreateReliefSlots(const AnalysisResult& result);
    int CreateReliefSlotsForBody(
        const std::vector<const AnalysisResult::SlotCandidate*>& candidates,
        const std::vector<double>& lengths, double width);
    bool CreateReliefSlot(const AnalysisResult::SlotCandidate& candidate,
                          double length, double width);
    bool CreateThroughSlot(NXOpen::Body* body,
                           const NXOpen::Point3d& center,
                           const NXOpen::Vector3d& lengthDirection,
                           const NXOpen::Vector3d& widthDirection,
                           const NXOpen::Vector3d& normal,
                           double length, double width);
    void RefreshDisplay(const AnalysisResult& result);
    void RestoreDisplay();
    void RunAnalysis(bool showErrors);
    void ShowError(const std::string& message) const;

private:
    NXOpen::UI* ui_;
    NXOpen::Session* session_;
    NXOpen::BlockStyler::BlockDialog* dialog_;
    NXOpen::BlockStyler::UIBlock* bodySelect_;
    NXOpen::BlockStyler::UIBlock* formulaSummary_;
    NXOpen::BlockStyler::UIBlock* configButton_;
    NXOpen::BlockStyler::UIBlock* safetyDistanceMode_;
    NXOpen::BlockStyler::UIBlock* riskDistance_;
    NXOpen::BlockStyler::UIBlock* reliefLengthMode_;
    NXOpen::BlockStyler::UIBlock* reliefLength_;
    NXOpen::BlockStyler::UIBlock* slotWidth_;
    NXOpen::BlockStyler::UIBlock* status_;
    bool initialized_;
    bool refreshing_;
    bool slotsCreated_;
    std::vector<std::pair<tag_t, int>> originalTranslucencies_;
    std::vector<std::pair<tag_t, int>> originalColors_;
    std::vector<tag_t> highlightedFaces_;
};
