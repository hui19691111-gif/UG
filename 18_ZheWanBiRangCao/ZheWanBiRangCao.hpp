#pragma once

#include <NXOpen/BlockStyler_BlockDialog.hxx>
#include <NXOpen/BlockStyler_UIBlock.hxx>
#include <NXOpen/Body.hxx>
#include <NXOpen/DisplayableObject.hxx>
#include <NXOpen/Edge.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/UI.hxx>

#include <string>
#include <vector>

#include <uf_defs.h>

class ZheWanBiRangCaoDialog
{
public:
    ZheWanBiRangCaoDialog();
    ~ZheWanBiRangCaoDialog();

    NXOpen::BlockStyler::BlockDialog::DialogResponse Launch();

private:
    struct SlotParameters
    {
        double slotWidth;
        double slotDepth;
        double xPositiveExtra;
        double xNegativeExtra;
        double zPositiveExtra;
    };

    struct SlotReferenceEdge
    {
        NXOpen::Edge* edge;
        NXOpen::Point3d startPoint;
        NXOpen::Point3d endPoint;
    };

    struct SlotFeatureRecord
    {
        NXOpen::Point3d pointA;
        NXOpen::Point3d pointB;
        NXOpen::Vector3d widthDirection;
        NXOpen::Vector3d depthDirection;
        tag_t lines[5];
    };

    void initialize_cb();
    bool enable_ok_cb();
    int update_cb(NXOpen::BlockStyler::UIBlock* block);
    int apply_cb();
    int ok_cb();
    int cancel_cb();

    std::string GetDialogFilePath() const;
    NXOpen::Face* GetSelectedFace() const;
    NXOpen::Point3d GetSelectionPickPoint() const;
    void ClearSelectedEdge() const;
    double GetSlotWidthY() const;
    double GetSlotDepthZ() const;
    void SetDoubleValue(NXOpen::BlockStyler::UIBlock* block, double value) const;
    void LoadDialogState() const;
    void SaveDialogState() const;
    void ShowError(const std::string& message) const;
    void ColorFaceBlue(NXOpen::Face* face) const;

    NXOpen::Edge* FindClosestLinearEdgeOnFace(NXOpen::Face* face, const NXOpen::Point3d& pickPoint) const;
    double EstimateThickness(NXOpen::Body* body, NXOpen::Face* referenceFace) const;
    double ComputeInnerEdgeDistance(NXOpen::Edge* selectedEdge) const;
    std::vector<SlotReferenceEdge> FindInnerReferenceEdges(NXOpen::Edge* selectedEdge,
                                                           NXOpen::Face* face,
                                                           const NXOpen::Point3d& selectedPlanePoint,
                                                           const NXOpen::Vector3d& selectedPlaneNormal) const;
    NXOpen::Point3d FindNearestOuterPoint(const NXOpen::Point3d& innerPoint,
                                          const NXOpen::Point3d& outerStart,
                                          const NXOpen::Point3d& outerEnd) const;
    void GetPickedEndPoints(NXOpen::Edge* edge,
                            const NXOpen::Point3d& pickPoint,
                            NXOpen::Point3d& pointA,
                            NXOpen::Point3d& pointB) const;
    NXOpen::Face* FindInnerSlotCarrierFace(NXOpen::Edge* edge,
                                           const NXOpen::Vector3d& yDirection,
                                           double thickness) const;
    double ComputeDepthChainLengthFromPoint(NXOpen::Body* body,
                                            NXOpen::Edge* selectedEdge,
                                            const NXOpen::Point3d& startPoint,
                                            const NXOpen::Vector3d& zDirection,
                                            double thickness) const;
    bool ShouldCreateSlotAtEnd(const NXOpen::Point3d& innerPoint,
                               const NXOpen::Point3d& outerPoint,
                               const NXOpen::Vector3d& outerEdgeDirection,
                               NXOpen::Body* body,
                               double thickness) const;
    bool CreateSlotOutlineOnSelectedFace(NXOpen::Edge* selectedEdge,
                                         NXOpen::Face* selectedFace,
                                         const NXOpen::Point3d& innerPoint,
                                         const NXOpen::Point3d& selectedPlanePoint,
                                         const NXOpen::Vector3d& selectedPlaneNormal,
                                         double slotWidth,
                                         double slotDepth);
    bool EditSlotOutline(SlotFeatureRecord& record, double slotWidth, double slotDepth) const;
    NXOpen::Vector3d AskSelectedFaceInnerNormal(NXOpen::Face* face,
                                                NXOpen::Body* body,
                                                const NXOpen::Point3d& referencePoint,
                                                double thickness) const;
    bool ExtrudeSubtractAndDeleteCurves(NXOpen::Body* targetBody,
                                        NXOpen::Face* selectedFace,
                                        const NXOpen::Point3d& referencePoint,
                                        const std::vector<tag_t>& curveTags,
                                        double thickness);
    void HideObjects(const std::vector<tag_t>& objectTags) const;
    void HideObject(tag_t objectTag) const;
    void CleanupHiddenTemporaryObjects();
    void CleanupPreviewObjects();
    bool UpdateAllSlots();
    int Execute();

private:
    NXOpen::UI* ui_;
    NXOpen::Session* session_;
    NXOpen::BlockStyler::BlockDialog* dialog_;
    NXOpen::BlockStyler::UIBlock* mainGroup_;
    NXOpen::BlockStyler::UIBlock* edgeSelectBlock_;
    NXOpen::BlockStyler::UIBlock* slotWidthBlock_;
    NXOpen::BlockStyler::UIBlock* slotDepthBlock_;
    std::vector<SlotFeatureRecord> slotRecords_;
    std::vector<tag_t> hiddenTemporaryTags_;
    std::vector<tag_t> previewFeatureTags_;
};
