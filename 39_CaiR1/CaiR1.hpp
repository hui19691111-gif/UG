#pragma once

#include <NXOpen/BlockStyler_BlockDialog.hxx>
#include <NXOpen/Session.hxx>

#include <string>
#include <utility>
#include <vector>

namespace NXOpen
{
class Body;
class Edge;
class Face;
class Session;
class UI;
namespace Features
{
class CustomFeatureClass;
class CustomFeatureClassManager;
class CustomFeature;
class CustomFeaturePreUpdateEvent;
}
namespace BlockStyler
{
class UIBlock;
}
}

class CaiR1Dialog
{
public:
    CaiR1Dialog();
    ~CaiR1Dialog();

    NXOpen::BlockStyler::BlockDialog::DialogResponse Launch();
    int BuildCustomFeatureConstruction(
        NXOpen::Features::CustomFeaturePreUpdateEvent* event);

private:
    void initialize_cb();
    void dialogShown_cb();
    int update_cb(NXOpen::BlockStyler::UIBlock* block);
    int apply_cb();
    int ok_cb();
    int cancel_cb();

    NXOpen::Face* SelectedFace() const;
    NXOpen::Face* CachedFace() const;
    NXOpen::Point3d PickPoint() const;
    double ExtensionLength() const;
    double Gap() const;
    double BendRadius() const;
    int CreatePreview();
    bool UndoPreview();
    void CommitPreview();
    void ClearSelectionAndUnhighlight(tag_t faceTag);
    void ApplyPreviewTranslucency();
    void RestorePreviewTranslucency();
    bool SubtractPreviewBodies(std::string& error);
    bool CommitCustomFeature(std::string& error);
    void LoadEditedCustomFeatureData();
    bool BuildSplitCorner(NXOpen::Face* cylindricalFace,
                          const NXOpen::Point3d& pickPoint,
                          double extensionLength,
                          double gap,
                          double bendRadius,
                          std::vector<tag_t>& createdFeatures,
                          std::string& error) const;
    bool EstimateCylinderThickness(NXOpen::Body* body,
                                   NXOpen::Face* face,
                                   double& thickness,
                                   std::string& error) const;
    void ShowError(const std::string& message) const;

private:
    NXOpen::UI* ui_;
    NXOpen::Session* session_;
    NXOpen::BlockStyler::BlockDialog* dialog_;
    NXOpen::BlockStyler::UIBlock* faceSelect_;
    NXOpen::BlockStyler::UIBlock* extensionLengthInput_;
    NXOpen::BlockStyler::UIBlock* gapInput_;
    NXOpen::BlockStyler::UIBlock* bendRadiusInput_;
    NXOpen::Features::CustomFeatureClassManager* customFeatureManager_;
    NXOpen::Features::CustomFeature* editedFeature_;
    NXOpen::Features::CustomFeatureClass* featureClass_;
    NXOpen::Session::UndoMarkId previewMark_;
    tag_t selectedFaceTag_;
    tag_t targetBodyTag_;
    NXOpen::Point3d selectedPickPoint_;
    bool hasPreview_;
    bool rebuildingPreview_;
    bool changingSelection_;
    bool buildingCustomFeature_;
    bool loadingEditedFeature_;
    std::vector<tag_t> previewFeatures_;
    std::vector<std::pair<tag_t, int>> previewBodyTranslucencies_;
};
