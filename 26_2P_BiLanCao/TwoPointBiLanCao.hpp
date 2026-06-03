#pragma once

#include <NXOpen/BlockStyler_BlockDialog.hxx>
#include <NXOpen/BlockStyler_UIBlock.hxx>
#include <NXOpen/Body.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/TaggedObject.hxx>
#include <NXOpen/UI.hxx>

#include <string>
#include <vector>

namespace NXOpen
{
    class Edge;
    class Face;
}

class TwoPointBiLanCaoDialog
{
public:
    TwoPointBiLanCaoDialog();
    ~TwoPointBiLanCaoDialog();

    NXOpen::BlockStyler::BlockDialog::DialogResponse Launch();

private:
    struct DialogValues
    {
        NXOpen::Point3d startPoint;
        NXOpen::Point3d endPoint;
        NXOpen::Body* targetBody;
        NXOpen::TaggedObject* startSelectedObject;
        NXOpen::TaggedObject* endSelectedObject;
        NXOpen::Edge* startMatchedEdge;
        NXOpen::Face* containingPlaneFace;
        NXOpen::Face* largestPlanarFace;
        NXOpen::Face* oppositePlanarFace;
        std::string startParentTrace;
        std::string profilePointTrace;
        std::string profileOperationTrace;
        std::vector<tag_t> profileCurveTags;
        std::vector<tag_t> profileFeatureTags;
        std::vector<tag_t> profileToolBodyTags;
        tag_t profileEdge1Tag;
        tag_t profileEdge2Tag;
        tag_t profileSketchTag;
        tag_t profileExtrudeFeatureTag;
        tag_t profileToolBodyTag;
        double profileEdgeAngleDeg;
        double profileABLength;
        double profileGALength;
        double profileExtrudeHeight;
        NXOpen::Point3d slotSidePoint1;
        NXOpen::Point3d slotSidePoint2;
        NXOpen::Vector3d slotSideDirection1;
        NXOpen::Vector3d slotSideDirection2;
        bool hasSlotSideDirections;
        std::string profileExtrudeDirectionTrace;
        double largestPlanarFaceArea;
        double oppositePlanarFaceArea;
        double sheetThickness;
        double slotClearance;
        double bendRadius;
        bool chamferEdgeMode;
        bool gapOnlyMode;
        bool gapOnlyEdgeSelection;
        bool gapOnlyHasOffsetDirection;
        NXOpen::Vector3d gapOnlyOffsetDirection;
        int wrapCornerMode;
    };

    void initialize_cb();
    void dialogShown_cb();
    bool enable_ok_cb();
    int update_cb(NXOpen::BlockStyler::UIBlock* block);
    int apply_cb();
    int ok_cb();
    int cancel_cb();
    void focus_notify_cb(NXOpen::BlockStyler::UIBlock* block, bool focus);
    void keyboard_focus_notify_cb(NXOpen::BlockStyler::UIBlock* block, bool focus);

    std::string GetDialogFilePath() const;
    bool ReadDialogValues(DialogValues& values) const;
    bool ReadSelectedPoint(NXOpen::BlockStyler::UIBlock* block,
                           NXOpen::TaggedObject*& selectedObject,
                           NXOpen::Point3d& point) const;
    bool ReadOptionalVector(NXOpen::BlockStyler::UIBlock* block,
                            NXOpen::Vector3d& vector) const;
    NXOpen::TaggedObject* GetFirstSelectedObject(NXOpen::BlockStyler::UIBlock* block) const;
    NXOpen::Body* GetOwnerBody(NXOpen::TaggedObject* object) const;
    NXOpen::Body* FindBodyBySmartPointParents(NXOpen::TaggedObject* object,
                                              NXOpen::Edge** matchedEdge,
                                              NXOpen::Face** matchedFace,
                                              std::string* parentTrace) const;
    NXOpen::Body* FindBodyByEdgeEndpoint(const NXOpen::Point3d& point, NXOpen::Edge** matchedEdge) const;
    bool GetPlanarFacePlane(NXOpen::Face* face, NXOpen::Point3d& origin, NXOpen::Vector3d& normal) const;
    double MeasureFaceArea(NXOpen::Face* face) const;
    bool FindSheetThicknessByLargestParallelFaces(NXOpen::Body* body,
                                                  NXOpen::Face** largestFace,
                                                  NXOpen::Face** oppositeFace,
                                                  double& largestArea,
                                                  double& oppositeArea,
                                                  double& thickness) const;
    bool FindTwoPlanarEdgesAtPoint(NXOpen::Face* planeFace,
                                   const NXOpen::Point3d& point,
                                   NXOpen::Edge** edge1,
                                   NXOpen::Edge** edge2) const;
    bool EdgeDirectionFromPoint(NXOpen::Edge* edge,
                                const NXOpen::Point3d& point,
                                NXOpen::Vector3d& direction) const;
    std::vector<NXOpen::Edge*> FindEdgesConnectedToPointOnBody(NXOpen::Body* body,
                                                               const NXOpen::Point3d& point) const;
    NXOpen::Edge* FindCornerEdgeAtPoint(NXOpen::Body* body,
                                        const NXOpen::Point3d& point,
                                        NXOpen::Edge* planeEdge1,
                                        NXOpen::Edge* planeEdge2) const;
    std::vector<NXOpen::Face*> FindPlanarFacesOfEdge(NXOpen::Edge* edge) const;
    NXOpen::Face* FindLargerBoundingFace(NXOpen::Edge* edge) const;
    NXOpen::Face* FindSmallerBoundingFace(NXOpen::Edge* edge) const;
    NXOpen::Point3d ComputeBodyBoundsCenter(NXOpen::Body* body) const;
    bool ComputeInwardNormal(NXOpen::Body* body, NXOpen::Face* face, NXOpen::Vector3d& inwardNormal) const;
    bool FindNearestEndpointFromBodyEdges(NXOpen::Body* body,
                                          const NXOpen::Point3d& basePoint,
                                          NXOpen::Point3d& nearestPoint,
                                          double& nearestDistance) const;
    bool ShouldCreateCubeForEdgeEndpoint(NXOpen::Body* body,
                                         const NXOpen::Point3d& point,
                                         double thickness,
                                         double faceAngleDegrees) const;
    bool CreateCubeToolAtPoint(DialogValues& values,
                               const NXOpen::Point3d& point,
                               const std::vector<NXOpen::Edge*>& connectedEdges) const;
    bool CreateCubeToolAtPointWithDirections(DialogValues& values,
                                             const NXOpen::Point3d& point,
                                             NXOpen::Vector3d xDirection,
                                             NXOpen::Vector3d yDirection,
                                             NXOpen::Vector3d zDirection) const;
    bool CreateTopCornerEndpointCutWithDirections(DialogValues& values,
                                                  const NXOpen::Point3d& point,
                                                  NXOpen::Vector3d cornerEdgeDirection,
                                                  NXOpen::Vector3d edge1Direction,
                                                  NXOpen::Vector3d edge2Direction) const;
    bool GetSelectedEdgeFaceAngle(NXOpen::Body* body,
                                  NXOpen::Edge* edge,
                                  double& angleDegrees,
                                  std::vector<NXOpen::Face*>& principalFaces,
                                  std::vector<NXOpen::Vector3d>& inwardNormals) const;
    bool CreateNonRightCornerEdgeCut(DialogValues& values,
                                     NXOpen::Edge* cornerEdge,
                                     double angleDegrees,
                                     const std::vector<NXOpen::Face*>& principalFaces,
                                     const std::vector<NXOpen::Vector3d>& inwardNormals) const;
    bool CreateSecondaryAcuteCornerEdgeCut(DialogValues& values,
                                           NXOpen::Edge* cornerEdge,
                                           double angleDegrees,
                                           const std::vector<NXOpen::Face*>& principalFaces,
                                           const std::vector<NXOpen::Vector3d>& inwardNormals) const;
    bool CreateCornerEdgeCutWithFace(DialogValues& values,
                                     NXOpen::Edge* cornerEdge,
                                     NXOpen::Face* referenceFace) const;
    bool CreateCornerEdgeCut(DialogValues& values, NXOpen::Edge* cornerEdge) const;
    bool CreateReliefSlotProfile(DialogValues& values, bool commitCut) const;
    bool GetFaceInnerNormalDirection(DialogValues& values, NXOpen::Vector3d& direction) const;
    bool CreateProfileSketch(DialogValues& values) const;
    bool ExtrudeReliefSlot(DialogValues& values) const;
    bool ExtrudeReliefSlotFeature(DialogValues& values) const;
    std::string BuildLiveSlotSignature(const DialogValues& values) const;
    void ClearPreviewProfile();
    void RevertLiveSlot();
    void CommitLiveSlot();
    void RefreshLiveSlot();
    void ClearPointSelections();
    NXOpen::Face* FindPlanarFaceContainingPoints(NXOpen::Body* body,
                                                 const NXOpen::Point3d& firstPoint,
                                                 const NXOpen::Point3d& secondPoint) const;
    NXOpen::Face* FindPlanarFaceForSelectedEdgeGap(NXOpen::Edge* edge,
                                                   const NXOpen::Point3d& referencePoint) const;
    void ConfigurePointSelection(NXOpen::BlockStyler::UIBlock* block, bool allowEdges = false) const;
    void SetBlockLogicalProperty(NXOpen::BlockStyler::UIBlock* block,
                                 const char* propertyName,
                                 bool value) const;
    void ConfigurePointSelectionsForCurrentMode() const;
    void CacheStartPointOwner();
    double ReadDouble(NXOpen::BlockStyler::UIBlock* block, double fallback) const;
    bool ReadLogical(NXOpen::BlockStyler::UIBlock* block, bool fallback) const;
    int ReadEnumValue(NXOpen::BlockStyler::UIBlock* block, int fallback) const;
    void SetDouble(NXOpen::BlockStyler::UIBlock* block, double value) const;
    void SetLogical(NXOpen::BlockStyler::UIBlock* block, bool value) const;
    void ShowError(const std::string& message) const;
    int Execute();

private:
    NXOpen::UI* ui_;
    NXOpen::Session* session_;
    NXOpen::BlockStyler::BlockDialog* dialog_;
    NXOpen::BlockStyler::UIBlock* mainGroup_;
    NXOpen::BlockStyler::UIBlock* startPointBlock_;
    NXOpen::BlockStyler::UIBlock* endPointBlock_;
    NXOpen::BlockStyler::UIBlock* slotClearanceBlock_;
    NXOpen::BlockStyler::UIBlock* bendRadiusBlock_;
    NXOpen::BlockStyler::UIBlock* chamferEdgeToggleBlock_;
    NXOpen::BlockStyler::UIBlock* gapOnlyToggleBlock_;
    NXOpen::BlockStyler::UIBlock* normalDirectionBlock_;
    NXOpen::BlockStyler::UIBlock* wrapCornerModeBlock_;
    NXOpen::TaggedObject* cachedStartSelectedObject_;
    NXOpen::Body* cachedTargetBody_;
    std::vector<tag_t> previewProfileCurveTags_;
    int liveSlotUndoMark_;
    bool hasLiveSlot_;
    bool liveSlotRefreshInProgress_;
    std::string liveSlotSignature_;
    std::vector<tag_t> liveSlotFeatureTags_;
    std::vector<tag_t> liveSlotCurveTags_;
    std::vector<tag_t> liveSlotToolBodyTags_;
    tag_t liveSlotToolBodyTag_;
    bool slotCreatedOnSelection_;
};
