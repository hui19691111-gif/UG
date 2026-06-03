#pragma once

#include <NXOpen/BlockStyler_BlockDialog.hxx>
#include <NXOpen/BlockStyler_UIBlock.hxx>
#include <NXOpen/Body.hxx>
#include <NXOpen/DisplayableObject.hxx>
#include <NXOpen/Edge.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/Features_ToolingBox.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/UI.hxx>

#include <string>
#include <vector>

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
        NXOpen::Features::ToolingBox* toolingBox;
        NXOpen::Edge* edge;
        NXOpen::Point3d boundedPoint;
        NXOpen::Vector3d xDirection;
        NXOpen::Vector3d yDirection;
        NXOpen::Vector3d zDirection;
        double offsetPositiveX;
        double offsetNegativeX;
        double offsetPositiveY;
        double offsetNegativeY;
        double offsetPositiveZ;
        double offsetNegativeZ;
        double baseNegativeZ;
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
    int GetSlotMode() const;
    double GetSlotWidthY() const;
    double GetSlotDepthZ() const;
    void ShowError(const std::string& message) const;
    void ColorFaceBlue(NXOpen::Face* face) const;

    NXOpen::Edge* FindClosestLinearEdgeOnFace(NXOpen::Face* face, const NXOpen::Point3d& pickPoint) const;
    double EstimateThickness(NXOpen::Body* body, NXOpen::Face* referenceFace) const;
    double ComputeInnerEdgeDistance(NXOpen::Edge* selectedEdge) const;
    std::vector<SlotReferenceEdge> FindInnerReferenceEdges(NXOpen::Edge* selectedEdge, NXOpen::Face* face) const;
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
    bool CreateBoundedSlot(NXOpen::Edge* edge,
                           const NXOpen::Point3d& boundedPoint,
                           const NXOpen::Vector3d& xDirection,
                           const NXOpen::Vector3d& yDirection,
                           const NXOpen::Vector3d& zDirection,
                           double offsetPositiveX,
                           double offsetNegativeX,
                           double offsetPositiveY,
                           double offsetNegativeY,
                           double offsetPositiveZ,
                           double offsetNegativeZ,
                           NXOpen::Features::ToolingBox** createdToolingBox = nullptr,
                           bool useDefaultMatrix = false) const;
    bool EditSlotFeature(SlotFeatureRecord& record);
    bool UpdateAllSlots();
    bool CreateOrEditSlot(NXOpen::Edge* edge,
                          const NXOpen::Point3d& boundedPoint,
                          const NXOpen::Vector3d& xDirection,
                          const NXOpen::Vector3d& yDirection,
                          const NXOpen::Vector3d& zDirection,
                          double offsetPositiveX,
                          double offsetNegativeX,
                          double offsetPositiveY,
                          double offsetNegativeY,
                          double offsetPositiveZ,
                          double offsetNegativeZ,
                          double baseNegativeZ,
                          bool useDefaultMatrix = false);
    int Execute();
    bool CreateSlotAtEnd(NXOpen::Edge* edge,
                         NXOpen::Face* largerFace,
                         const NXOpen::Point3d& innerPoint,
                         const NXOpen::Point3d& outerPoint,
                         const NXOpen::Vector3d& yDirection,
                         const SlotParameters& parameters);

private:
    NXOpen::UI* ui_;
    NXOpen::Session* session_;
    NXOpen::BlockStyler::BlockDialog* dialog_;
    NXOpen::BlockStyler::UIBlock* mainGroup_;
    NXOpen::BlockStyler::UIBlock* slotModeBlock_;
    NXOpen::BlockStyler::UIBlock* edgeSelectBlock_;
    NXOpen::BlockStyler::UIBlock* slotWidthBlock_;
    NXOpen::BlockStyler::UIBlock* slotDepthBlock_;
    std::vector<SlotFeatureRecord> slotRecords_;
};
