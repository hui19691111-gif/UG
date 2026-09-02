#pragma once

#include <NXOpen/BlockStyler_BlockDialog.hxx>
#include <NXOpen/Body.hxx>
#include <NXOpen/BlockStyler_UIBlock.hxx>
#include <NXOpen/Features_BooleanFeature.hxx>
#include <NXOpen/TaggedObject.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/UI.hxx>

#include <string>
#include <vector>

class BaoRongQieChuDialog
{
    struct HoleLoopProfile
    {
        NXOpen::Body* targetBody;
        NXOpen::Point3d origin;
        NXOpen::Vector3d normal;
        double depth;
        std::string profileKey;
        std::vector<tag_t> curveTags;
    };

    struct PendingBlendReplacement
    {
        NXOpen::Body* targetBody;
        double maxRadius;
        double sheetThickness;
        bool hasSheetThickness;
    };

public:
    BaoRongQieChuDialog();
    ~BaoRongQieChuDialog();

    NXOpen::BlockStyler::BlockDialog::DialogResponse Launch();

private:
    void initialize_cb();
    void dialogShown_cb();
    int update_cb(NXOpen::BlockStyler::UIBlock* block);
    int apply_cb();
    int ok_cb();
    int cancel_cb();

    int ExecuteFromSelection();
    int ExecuteImmediateCutFromSelection();
    int ExecuteOnlyBlendReplacementFromSelection();
    int ExecutePendingBlendReplacement();
    int ExecutePendingCutFaceRemoval();
    int ExecutePendingConnectedFaceCreation();
    std::vector<NXOpen::TaggedObject*> GetSelectedObjects() const;
    bool GetBooleanSubtractEnabled() const;
    bool GetRemoveBlendEnabled() const;
    bool GetOnlyRemoveBlendEnabled() const;
    bool GetHealRemovedRegionEnabled() const;
    double GetOffsetValue() const;
    double GetBlendRadiusValue() const;
    void ClearSelection();
    void ShowError(const std::string& message) const;
    void SyncOptionalControls();
    void LoadDialogMemory();
    void SaveDialogMemory();
    void RememberPendingBlendReplacement(
        NXOpen::Body* body,
        double maxRadius,
        double sheetThickness,
        bool hasSheetThickness);
    void RememberPendingCutFeature(NXOpen::Features::BooleanFeature* feature);
    void RememberPendingConnectedFaceBody(NXOpen::Body* body);
    void CapturePendingHoleProfiles(const std::vector<NXOpen::TaggedObject*>& selectedObjects);
    void RestorePendingHoleProfiles(NXOpen::Part* workPart);
    void ClearPendingHoleProfiles();

private:
    NXOpen::UI* ui_;
    NXOpen::Session* session_;
    NXOpen::BlockStyler::BlockDialog* dialog_;
    NXOpen::BlockStyler::UIBlock* mainGroup_;
    NXOpen::BlockStyler::UIBlock* objectSelectBlock_;
    NXOpen::BlockStyler::UIBlock* booleanToggleBlock_;
    NXOpen::BlockStyler::UIBlock* removeBlendToggleBlock_;
    NXOpen::BlockStyler::UIBlock* onlyRemoveBlendToggleBlock_;
    NXOpen::BlockStyler::UIBlock* blendRadiusBlock_;
    NXOpen::BlockStyler::UIBlock* healRemovedRegionToggleBlock_;
    NXOpen::BlockStyler::UIBlock* offsetBlock_;
    std::vector<HoleLoopProfile> pendingHoleProfiles_;
    std::vector<PendingBlendReplacement> pendingBlendReplacements_;
    std::vector<NXOpen::Features::BooleanFeature*> pendingCutFeatures_;
    std::vector<NXOpen::Body*> pendingConnectedFaceBodies_;
    bool isInternalUpdate_;
};
