#pragma once

#include "TwoPointSiBianShared.hpp"

#include <NXOpen/BlockStyler_BlockDialog.hxx>
#include <NXOpen/BlockStyler_Button.hxx>
#include <NXOpen/BlockStyler_Enumeration.hxx>
#include <NXOpen/BlockStyler_SelectObject.hxx>
#include <NXOpen/BlockStyler_StringBlock.hxx>
#include <NXOpen/BlockStyler_Toggle.hxx>
#include <NXOpen/Body.hxx>
#include <NXOpen/Edge.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/Features_CustomFeature.hxx>
#include <NXOpen/Features_CustomFeatureClass.hxx>
#include <NXOpen/Features_CustomFeatureClassManager.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/TaggedObject.hxx>
#include <NXOpen/UI.hxx>

#include <uf_defs.h>

#include <string>
#include <vector>

namespace NXOpen
{
namespace Features
{
class CustomFeatureData;
class EditWithRollbackManager;
class CustomFeaturePreUpdateEvent;
}
}

class TwoPointSiBianUI
{
public:
    enum class FeatureMode
    {
        Chamfer = 0,
        NinetyLeft = 1,
        NinetyRight = 2
    };

    TwoPointSiBianUI();
    ~TwoPointSiBianUI();

    NXOpen::BlockStyler::BlockDialog::DialogResponse Launch();
    int BuildCustomFeatureConstruction(
        NXOpen::Features::CustomFeaturePreUpdateEvent* event);

private:
    struct InferredInputs
    {
        NXOpen::TaggedObject* startObject = nullptr;
        NXOpen::TaggedObject* endObject = nullptr;
        NXOpen::Point3d startPoint;
        NXOpen::Point3d endPoint;
        NXOpen::Body* targetBody = nullptr;
        NXOpen::Face* baseFace = nullptr;
        NXOpen::Edge* startEdge = nullptr;
        NXOpen::Edge* endEdge = nullptr;
        NXOpen::Edge* startPositiveYEdge = nullptr;
        NXOpen::Edge* startNegativeYEdge = nullptr;
        NXOpen::Edge* endPositiveYEdge = nullptr;
        NXOpen::Edge* endNegativeYEdge = nullptr;
        double thickness = 0.0;
        double spanLength = 0.0;
        bool inferredFromSingleClick = false;
        bool smartMode = false;
        bool chamferEdgeMode = true;
        bool reverseChamfer270Cut = false;
        bool useNinetyClearanceGrooveTemplate = false;
        bool useNinetyClearanceGrooveRightTemplate = false;
        NXOpen::Point3d selectionClickPoint;
        std::string clearanceValue = "0.2";
        std::string bendRadiusValue = "0.2";
        FeatureMode featureMode = FeatureMode::Chamfer;
    };

    void initialize_cb();
    void dialogShown_cb();
    bool enable_ok_cb();
    int update_cb(NXOpen::BlockStyler::UIBlock* block);
    int apply_cb();
    int ok_cb();
    int cancel_cb();
    int close_cb();

    bool ReadInputs(InferredInputs& inputs,
                    NXOpen::TaggedObject* singleClickObjectOverride = nullptr,
                    const NXOpen::Point3d* singleClickPointOverride = nullptr) const;
    FeatureMode ReadFeatureMode() const;
    bool IsSmartModeEnabled() const;
    bool ReadSelectedPoint(NXOpen::BlockStyler::SelectObject* block,
                           NXOpen::TaggedObject*& selectedObject,
                           NXOpen::Point3d& point) const;
    bool InferEndpointsFromFaceClick(NXOpen::TaggedObject* selectedObject,
                                     const NXOpen::Point3d& clickPoint,
                                     InferredInputs& inputs) const;
    bool CompleteInputsForEndpoints(InferredInputs& inputs) const;
    bool RefreshSmartInputsAfterRips(const InferredInputs& originalInputs,
                                     InferredInputs& refreshedInputs) const;
    bool ConstrainRightAnglePrimaryP2ToOffsetSharedEdges(
        const InferredInputs& originalInputs,
        const InferredInputs& currentInputs,
        const std::vector<tag_t>& offsetFeatureTags,
        InferredInputs& constrainedInputs) const;
    NXOpen::Body* FindBody(NXOpen::TaggedObject* object) const;
    NXOpen::Body* FindBodyAndFaceContainingPoints(const NXOpen::Point3d& first,
                                                  const NXOpen::Point3d& second,
                                                  NXOpen::Face*& face) const;
    NXOpen::Edge* FindEdgeAtPoint(NXOpen::Body* body, const NXOpen::Point3d& point) const;
    NXOpen::Face* FindPlanarFaceAtPoint(NXOpen::Body* body,
                                        const NXOpen::Point3d& point) const;
    double ThicknessEdgeScoreAtPoint(NXOpen::Body* body,
                                     NXOpen::Face* baseFace,
                                     const NXOpen::Point3d& point,
                                     const NXOpen::Vector3d& faceNormal,
                                     double expectedThickness) const;
    bool FindSignedEdgesAtPoint(NXOpen::Body* body,
                                 const NXOpen::Point3d& point,
                                 const NXOpen::Vector3d& xDirection,
                                 const NXOpen::Vector3d& yDirection,
                                 const NXOpen::Vector3d& zDirection,
                                 NXOpen::Edge*& positiveEdge,
                                 NXOpen::Edge*& negativeEdge) const;
    NXOpen::Face* FindPlanarFaceContainingPoints(NXOpen::Body* body,
                                                 const NXOpen::Point3d& first,
                                                 const NXOpen::Point3d& second) const;
    void OrientNormalAwayFromOppositeFace(NXOpen::Body* body,
                                          NXOpen::Face* baseFace,
                                          const NXOpen::Point3d& pointOnFace,
                                          NXOpen::Vector3d& normal) const;
    double EstimateSheetThickness(NXOpen::Body* body, NXOpen::Face* baseFace) const;
    double MeasureFaceArea(NXOpen::Face* face) const;
    bool EdgeHasParallelMateAtThickness(NXOpen::Body* body,
                                        NXOpen::Edge* edge,
                                        double thickness,
                                        NXOpen::Edge*& parallelEdge,
                                        double& minimumDistance) const;
    NXOpen::Face* FindPlanarFaceContainingEdges(NXOpen::Body* body,
                                                NXOpen::Edge* first,
                                                NXOpen::Edge* second) const;
    NXOpen::Face* FindParallelFaceAtThickness(NXOpen::Body* body,
                                              NXOpen::Face* sourceFace,
                                              double thickness,
                                              const NXOpen::Point3d& qPoint) const;
    bool BuildFallbackSecondInputs(const InferredInputs& sourceInputs,
                                   NXOpen::Edge* firstEdgeAtQ,
                                   NXOpen::Edge* secondEdgeAtQ,
                                   const NXOpen::Point3d& qPoint,
                                   InferredInputs& secondInputs) const;
    bool BuildQFirstSecondInputs(const InferredInputs& sourceInputs,
                                 NXOpen::Edge* firstEdgeAtQ,
                                 NXOpen::Edge* secondEdgeAtQ,
                                 const NXOpen::Point3d& qPoint,
                                 InferredInputs& secondInputs) const;
    bool BuildConcaveStripPlan(const InferredInputs& sourceInputs,
                               NXOpen::Edge* firstEdgeAtQ,
                               NXOpen::Edge* secondEdgeAtQ,
                               const NXOpen::Point3d& qPoint,
                               NXOpen::Point3d& q3,
                               NXOpen::Point3d& q4,
                               NXOpen::Vector3d& planeNormal) const;
    bool CreateConcaveStripCut(const InferredInputs& inputs,
                               const NXOpen::Point3d& q3,
                               const NXOpen::Point3d& q4,
                               const NXOpen::Vector3d& planeNormal,
                               tag_t& subtractFeatureTag,
                               std::string& errorMessage) const;
    std::vector<NXOpen::Edge*> FindReferenceConnectedEdges(
        NXOpen::Body* body,
        const NXOpen::Point3d& point) const;
    NXOpen::Edge* FindReferenceCornerEdge(
        const InferredInputs& inputs) const;
    std::vector<NXOpen::Face*> FindReferencePlanarFaces(
        NXOpen::Edge* edge) const;
    bool ComputeReferenceInwardNormal(NXOpen::Body* body,
                                      NXOpen::Face* face,
                                      NXOpen::Vector3d& inwardNormal) const;
    bool CreateReferenceCornerEdgeCut(const InferredInputs& inputs,
                                      NXOpen::Edge* cornerEdge,
                                      std::string& errorMessage) const;
    bool CreateReferenceRightCornerEdgeCut(const InferredInputs& inputs,
                                           NXOpen::Edge* cornerEdge,
                                           NXOpen::Face* referenceFace,
                                           std::string& errorMessage) const;
    bool CreateReferenceNonRightCornerEdgeCut(
        const InferredInputs& inputs,
        NXOpen::Edge* cornerEdge,
        double angleDegrees,
        const std::vector<NXOpen::Face*>& principalFaces,
        const std::vector<NXOpen::Vector3d>& inwardNormals,
        std::string& errorMessage) const;
    bool CreateReferenceTopCornerCut(
        const InferredInputs& inputs,
        const NXOpen::Point3d& point,
        NXOpen::Vector3d cornerEdgeDirection,
        NXOpen::Vector3d edge1Direction,
        NXOpen::Vector3d edge2Direction,
        std::string& errorMessage) const;
    bool ExtrudeReferenceCornerProfile(
        const InferredInputs& inputs,
        const std::vector<NXOpen::Point3d>& profilePoints,
        const NXOpen::Point3d& origin,
        const NXOpen::Vector3d& direction,
        double startLimit,
        double endLimit,
        const char* operationName,
        std::string& errorMessage) const;
    bool CreateObliqueClearanceCut(const InferredInputs& inputs,
                                   NXOpen::Edge* referenceBEdge,
                                   NXOpen::Edge* p2QRipEdge,
                                   NXOpen::Face* b1B2Plane,
                                   const NXOpen::Point3d& originalQ,
                                   const NXOpen::Vector3d& planeNormal,
                                   tag_t& subtractFeatureTag,
                                   std::string& errorMessage) const;
    bool OffsetConcaveClearanceFace(const InferredInputs& inputs,
                                    NXOpen::Face* commonFace,
                                    const std::vector<tag_t>& offsetFeatureTags,
                                    tag_t& offsetFeatureTag,
                                    std::string& errorMessage,
                                    bool resolveCurrentCoplanarFragments = false) const;
    bool FindAuxiliaryRipPair(NXOpen::Body* body,
                              NXOpen::Face* commonFace,
                              NXOpen::Edge* b1,
                              NXOpen::Edge* b2,
                              double thickness,
                              NXOpen::Edge*& edgeToRip,
                              NXOpen::Edge*& parallelRipEdge) const;
    bool CreateSheetMetalRip(const InferredInputs& inputs,
                             NXOpen::Edge* firstEdge,
                             NXOpen::Edge* secondEdge,
                             bool offsetCreatedFaces,
                             tag_t& createdRipTag,
                             std::string& errorMessage) const;
    bool OffsetRightAngleRipFeature(const InferredInputs& inputs,
                                    NXOpen::Features::Feature* ripFeature,
                                    double cornerInteriorAngle,
                                    tag_t& firstOffsetTag,
                                    tag_t& secondOffsetTag,
                                    std::string& errorMessage,
                                    bool swapDirectionalOffsetGroups = false,
                                    bool forceDirectionalOffsetGroups = false,
                                    bool largestOnlyForSecondDirectionalGroup = false) const;
    bool TryCreateSecondPointRip(const InferredInputs& inputs,
                                 bool allowContinuationInputs,
                                 bool& ripCreated,
                                 tag_t& secondUdfTag,
                                 std::vector<tag_t>& secondToolBodyTags,
                                 std::vector<tag_t>& secondReferenceTags,
                                 bool& continuationCreated,
                                 InferredInputs& continuationInputs,
                                 bool& deferredSecondUdfRequested,
                                 InferredInputs& deferredSecondUdfInputs,
                                 tag_t& deferredRightAngleRipTag,
                                 double& deferredRightAngleRipAngle,
                                 std::vector<tag_t>& createdRightAngleOffsetTags,
                                 bool& createdRightAngle90SecondFeaturePath,
                                 bool& primaryUdfCreatedBeforeRip,
                                 tag_t& primarySubtractTag,
                                 std::vector<tag_t>& primaryReferenceTags,
                                 std::string& errorMessage) const;
    bool CreateUserDefinedFeature(const InferredInputs& inputs,
                                  std::string& errorMessage,
                                  tag_t* createdUdfTag = nullptr,
                                  std::vector<tag_t>* createdReferenceTags = nullptr,
                                  std::vector<tag_t>* createdToolBodyTags = nullptr) const;
    bool CreateFeatureTemplateFeature(const InferredInputs& inputs,
                                      std::string& errorMessage,
                                      tag_t* createdTemplateGroupTag,
                                      std::vector<tag_t>* createdToolBodyTags) const;
    bool SubtractToolBodies(NXOpen::Body* targetBody,
                            const std::vector<tag_t>& toolBodyTags,
                            tag_t& resultFeatureTag,
                            std::string& errorMessage) const;
    bool CreatePreview(const InferredInputs* forcedInputs = nullptr);
    bool LoadEditedFeatureState();
    bool ReadCurrentEditedInputs(InferredInputs& inputs) const;
    void AssignCustomFeatureData(NXOpen::Features::CustomFeatureData* data,
                                 const InferredInputs& inputs) const;
    bool BeginEditedFeatureRollback(std::string& errorMessage);
    bool FinishEditedFeatureRollback(bool errorDuringEdit,
                                     std::string& errorMessage);
    bool CommitCustomFeature(const InferredInputs& inputs,
                             std::string& errorMessage);
    bool RestoreEditedFeatureBaseline(std::string& errorMessage);
    bool UndoPreview(bool includeCommitted = false);
    bool FlattenPreviewTargetBody(std::string& errorMessage);
    bool ConsolidatePreviewFeatureTemplate(std::string& errorMessage);
    void CommitPreview();
    void FinalizeCommittedPreview();
    std::vector<tag_t> CurrentWorkPartFeatureTags() const;
    void CapturePreviewCreatedFeatureTags();
    void ConfigurePointSelection(NXOpen::BlockStyler::SelectObject* block) const;
    void ConfigureInputMode(bool smartMode, bool clearSelections);
    void UnhighlightSelectionObjects() const;
    void ShowError(const std::string& message) const;

private:
    NXOpen::Session* session_;
    NXOpen::UI* ui_;
    NXOpen::BlockStyler::BlockDialog* dialog_;
    NXOpen::BlockStyler::SelectObject* startPointBlock_;
    NXOpen::BlockStyler::SelectObject* endPointBlock_;
    NXOpen::BlockStyler::SelectObject* activeSmartSelectionBlock_;
    NXOpen::BlockStyler::StringBlock* clearanceBlock_;
    NXOpen::BlockStyler::StringBlock* bendRadiusBlock_;
    NXOpen::BlockStyler::Toggle* smartModeBlock_;
    NXOpen::BlockStyler::Toggle* chamferEdgeToggleBlock_;
    NXOpen::BlockStyler::Button* reverseCutButton_;
    NXOpen::BlockStyler::Enumeration* featureModeBlock_;
    NXOpen::Features::CustomFeatureClassManager* customFeatureManager_;
    NXOpen::Features::CustomFeature* editedFeature_;
    NXOpen::Features::CustomFeatureClass* featureClass_;
    NXOpen::Features::EditWithRollbackManager* editRollbackManager_;
    NXOpen::Session::UndoMarkId editRollbackMark_;
    bool loadingEditedFeature_;
    tag_t editedTargetBodyTag_;
    tag_t editedBaseFaceTag_;
    tag_t editedStartEdgeTag_;
    tag_t editedEndEdgeTag_;
    bool hasEditedEndpointCache_;
    NXOpen::Point3d editedCachedP1_;
    NXOpen::Point3d editedCachedP2_;
    NXOpen::Session::UndoMarkId previewUndoMark_;
    tag_t previewUdfTag_;
    tag_t previewTargetBodyTag_;
    std::vector<tag_t> previewReferenceTags_;
    std::vector<tag_t> previewBaselineFeatureTags_;
    std::vector<tag_t> previewCreatedFeatureTags_;
    bool hasSmartEndpointCache_;
    tag_t smartEndpointBodyTag_;
    tag_t smartEndpointFaceTag_;
    NXOpen::Point3d smartCachedP1_;
    NXOpen::Point3d smartCachedP2_;
    bool retainSmartEndpointCacheOnUndo_;
    bool hasPreview_;
    bool previewCommitted_;
    bool isUpdatingPreview_;
    bool reverseChamfer270Cut_;
    bool hasLastPreviewInputs_;
    InferredInputs lastPreviewInputs_;
    bool hasResolvedPrimaryP2ForPersistence_;
    NXOpen::Point3d resolvedPrimaryP2ForPersistence_;
    bool hasEditedBaselineInputs_;
    InferredInputs editedBaselineInputs_;
    bool editedLivePreviewDirty_;
    bool buildingCustomFeature_;
    bool customFeatureConstructionRebuilt_;
};

extern "C" __declspec(dllexport) int ZhihuiTwoPointSiBianBuildCustomFeature(
    void* eventPointer);
