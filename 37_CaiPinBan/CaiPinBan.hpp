#pragma once

#include <NXOpen/BlockStyler_BlockDialog.hxx>
#include <NXOpen/BlockStyler_ObjectColorPicker.hxx>
#include <NXOpen/BlockStyler_UIBlock.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/Session.hxx>
#include <uf_defs.h>

#include <string>
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

class CaiPinBanDialog
{
public:
    CaiPinBanDialog();
    ~CaiPinBanDialog();

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
    void UndoPreview();
    void CommitPreview();

    NXOpen::Face* SelectedFace() const;
    void UpdateColorControlVisibility() const;
    bool RecolorEnabled() const;
    int RequestedColor() const;
    int ColorMode() const;
    int FixedColor() const;
    void LoadDialogState() const;
    void SaveDialogState() const;
    void LoadEditedCustomFeatureData();
    bool CommitEditableCustomFeature(std::string& error);
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
    NXOpen::BlockStyler::UIBlock* faceSelect_;
    NXOpen::Features::CustomFeatureClassManager* customFeatureManager_;
    NXOpen::Features::CustomFeature* editedFeature_;
    NXOpen::Features::CustomFeatureClass* featureClass_;
    NXOpen::Session::UndoMarkId previewUndoMark_;
    bool hasPreview_;
    bool loadingEditedFeature_;
    bool buildingCustomFeature_;
    int previewColor_;
    tag_t previewTargetBodyTag_;
    tag_t previewSelectedFaceTag_;
    std::vector<tag_t> previewCreatedFeatureTags_;
};
