#pragma once

#include <NXOpen/BlockStyler_BlockDialog.hxx>
#include <NXOpen/BlockStyler_LinearDimension.hxx>
#include <NXOpen/BlockStyler_ObjectColorPicker.hxx>
#include <NXOpen/BlockStyler_UIBlock.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/Session.hxx>
#include <uf_defs.h>

#include <array>
#include <string>
#include <utility>
#include <vector>

namespace NXOpen
{
class Body;
class Face;
class Session;
class TaggedObject;
class UI;
namespace Features
{
class CustomFeature;
class CustomFeatureClass;
class CustomFeatureClassManager;
class CustomFeaturePreUpdateEvent;
}
}

class CaiRBanDialog
{
public:
    CaiRBanDialog();
    ~CaiRBanDialog();

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

    int CreatePreview();
    bool UndoPreview();
    void CommitPreview();

    NXOpen::Face* SelectedFace() const;
    void UpdateColorControlVisibility() const;
    bool RecolorEnabled() const;
    int RequestedColor() const;
    int ColorMode() const;
    int FixedColor() const;
    double ExtensionLength() const;
    void ConfigureDimensionHandles(
        const std::array<NXOpen::Point3d, 4>& origins,
        const std::array<NXOpen::Vector3d, 4>& directions);
    int WrapMode() const;
    void LoadDialogState() const;
    void SaveDialogState() const;
    void LoadEditedCustomFeatureData();
    bool CommitEditableCustomFeature(std::string& error);
    bool EstimateCylinderThickness(NXOpen::Body* body,
                                   NXOpen::Face* face,
                                   double& diameter,
                                   double& thickness,
                                   bool& reverseThicken,
                                   int wrapMode,
                                   std::string& error) const;
    bool CreateArcPanel(NXOpen::Face* face,
                        double thickness,
                        bool reverseThicken,
                        const std::array<double, 2>& cutDistances,
                        const std::array<double, 2>& offsetDistances,
                        int wrapMode,
                        std::string& error,
                        bool recolor,
                        int color,
                        std::vector<tag_t>* createdFeatureTags,
                        std::array<NXOpen::Point3d, 4>* handleOrigins,
                        std::array<NXOpen::Vector3d, 4>* handleDirections) const;
    bool CommitPreviewSubtract(std::string& error);
    void ApplyPreviewTranslucency(NXOpen::Body* previewBody);
    void RestorePreviewTranslucency();
    // Kept private only to preserve the proven feature-37 construction code;
    // the arc workflow below does not call these planar helpers.
    bool EstimateThicknessAndInnerNormal(NXOpen::Body* body,
                                         NXOpen::Face* face,
                                         double& thickness,
                                         NXOpen::Vector3d& innerNormal,
                                         NXOpen::Point3d& helpPoint,
                                         std::string& error) const;
    bool CreateAndSubtract(NXOpen::Face* face,
                           double thickness,
                           const NXOpen::Vector3d& innerNormal,
                           const NXOpen::Point3d& helpPoint,
                           std::string& error,
                           bool recolor,
                           int color,
                           std::vector<tag_t>* createdFeatureTags) const;
    int Execute();
    void ShowError(const std::string& message) const;

private:
    NXOpen::UI* ui_;
    NXOpen::Session* session_;
    NXOpen::BlockStyler::BlockDialog* dialog_;
    NXOpen::BlockStyler::UIBlock* recolorToggle_;
    NXOpen::BlockStyler::UIBlock* colorMode_;
    NXOpen::BlockStyler::ObjectColorPicker* fixedColor_;
    NXOpen::BlockStyler::UIBlock* extensionLength_;
    NXOpen::BlockStyler::UIBlock* wrapMode_;
    NXOpen::BlockStyler::UIBlock* faceSelect_;
    NXOpen::BlockStyler::LinearDimension* cutDimension0_;
    NXOpen::BlockStyler::LinearDimension* cutDimension1_;
    NXOpen::BlockStyler::LinearDimension* offsetDimension0_;
    NXOpen::BlockStyler::LinearDimension* offsetDimension1_;
    NXOpen::Features::CustomFeatureClassManager* customFeatureManager_;
    NXOpen::Features::CustomFeature* editedFeature_;
    NXOpen::Features::CustomFeatureClass* featureClass_;
    NXOpen::Session::UndoMarkId previewUndoMark_;
    bool hasPreview_;
    bool loadingEditedFeature_;
    bool rebuildingPreview_;
    bool configuringDimensionHandles_;
    bool dimensionValuesInitialized_;
    bool buildingCustomFeature_;
    bool previewSubtractCreated_;
    int previewColor_;
    tag_t previewTargetBodyTag_;
    tag_t previewSelectedFaceTag_;
    tag_t dimensionFaceTag_;
    std::array<double, 2> cutDistances_;
    std::array<double, 2> offsetDistances_;
    double currentThickness_;
    std::vector<tag_t> previewCreatedFeatureTags_;
    std::vector<std::pair<tag_t, int>> previewBodyTranslucencies_;
};
