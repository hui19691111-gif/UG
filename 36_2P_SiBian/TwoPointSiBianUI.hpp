#pragma once

#include "TwoPointSiBianShared.hpp"

#include <NXOpen/BlockStyler_BlockDialog.hxx>
#include <NXOpen/BlockStyler_Enumeration.hxx>
#include <NXOpen/BlockStyler_SelectObject.hxx>
#include <NXOpen/BlockStyler_StringBlock.hxx>
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

    bool ReadInputs(InferredInputs& inputs) const;
    FeatureMode ReadFeatureMode() const;
    bool ReadSelectedPoint(NXOpen::BlockStyler::SelectObject* block,
                           NXOpen::TaggedObject*& selectedObject,
                           NXOpen::Point3d& point) const;
    NXOpen::Body* FindBody(NXOpen::TaggedObject* object) const;
    NXOpen::Body* FindBodyAndFaceContainingPoints(const NXOpen::Point3d& first,
                                                  const NXOpen::Point3d& second,
                                                  NXOpen::Face*& face) const;
    NXOpen::Edge* FindEdgeAtPoint(NXOpen::Body* body, const NXOpen::Point3d& point) const;
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
    bool CreateUserDefinedFeature(const InferredInputs& inputs,
                                  std::string& errorMessage,
                                  tag_t* createdUdfTag = nullptr,
                                  std::vector<tag_t>* createdReferenceTags = nullptr) const;
    bool CreatePreview();
    void UndoPreview();
    void CommitPreview();
    void ConfigurePointSelection(NXOpen::BlockStyler::SelectObject* block) const;
    void ShowError(const std::string& message) const;

private:
    NXOpen::Session* session_;
    NXOpen::UI* ui_;
    NXOpen::BlockStyler::BlockDialog* dialog_;
    NXOpen::BlockStyler::SelectObject* startPointBlock_;
    NXOpen::BlockStyler::SelectObject* endPointBlock_;
    NXOpen::BlockStyler::StringBlock* clearanceBlock_;
    NXOpen::BlockStyler::StringBlock* bendRadiusBlock_;
    NXOpen::BlockStyler::Enumeration* featureModeBlock_;
    NXOpen::Features::CustomFeatureClassManager* customFeatureManager_;
    NXOpen::Features::CustomFeature* editedFeature_;
    NXOpen::Features::CustomFeatureClass* featureClass_;
    NXOpen::Session::UndoMarkId previewUndoMark_;
    tag_t previewUdfTag_;
    std::vector<tag_t> previewReferenceTags_;
    bool hasPreview_;
    bool isUpdatingPreview_;
};
