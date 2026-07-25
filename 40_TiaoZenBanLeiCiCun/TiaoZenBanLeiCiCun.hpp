#pragma once

#include <NXOpen/BlockStyler_BlockDialog.hxx>
#include <NXOpen/NXObject.hxx>
#include <NXOpen/Session.hxx>

#include <string>
#include <vector>

void TiaoZenWriteLog(const std::string& level,
                     const std::string& message) noexcept;
std::string TiaoZenLogFilePath() noexcept;

namespace NXOpen
{
class Body;
class Face;
class UI;
namespace BlockStyler
{
class LinearDimension;
class TabControl;
class UIBlock;
}
}

class PanelSizeDialog
{
public:
    PanelSizeDialog();
    ~PanelSizeDialog();

    NXOpen::BlockStyler::BlockDialog::DialogResponse Launch();

private:
    enum class AdjustMode
    {
        Independent = 0,
        TargetSize = 1,
        Round = 2
    };

    enum class RoundPolicy
    {
        Nearest = 0,
        Up = 1,
        Down = 2
    };

    struct PanelFrame
    {
        tag_t bodyTag = NULL_TAG;
        tag_t faceTag = NULL_TAG;
        NXOpen::Point3d origin;
        NXOpen::Vector3d normal;
        NXOpen::Vector3d lengthDirection;
        NXOpen::Vector3d widthDirection;
        double minLength = 0.0;
        double maxLength = 0.0;
        double minWidth = 0.0;
        double maxWidth = 0.0;
        double minNormal = 0.0;
        double maxNormal = 0.0;
        double faceMinLength = 0.0;
        double faceMaxLength = 0.0;
        double faceMinWidth = 0.0;
        double faceMaxWidth = 0.0;
        double thickness = 0.0;
        bool valid = false;

        double Length() const
        {
            return maxLength - minLength;
        }

        double Width() const
        {
            return maxWidth - minWidth;
        }

        double NormalDepth() const
        {
            return maxNormal - minNormal;
        }

        double LeftSkirt() const
        {
            return faceMinLength - minLength;
        }

        double RightSkirt() const
        {
            return maxLength - faceMaxLength;
        }

        double BottomSkirt() const
        {
            return faceMinWidth - minWidth;
        }

        double TopSkirt() const
        {
            return maxWidth - faceMaxWidth;
        }
    };

    struct SideOffsets
    {
        double left = 0.0;
        double right = 0.0;
        double bottom = 0.0;
        double top = 0.0;

        bool IsZero(double tolerance) const;
    };

private:
    void initialize_cb();
    void dialogShown_cb();
    int update_cb(NXOpen::BlockStyler::UIBlock* block);
    int apply_cb();
    int ok_cb();
    int cancel_cb();
    int filter_cb(NXOpen::BlockStyler::UIBlock* block,
                  NXOpen::TaggedObject* selectedObject);

    NXOpen::Face* SelectedFace() const;
    NXOpen::Face* CachedFace() const;
    bool AnalyzeSelectedFace(NXOpen::Face* face, std::string& error);
    bool AnalyzeBodyExtents(NXOpen::Body* body, PanelFrame& frame,
                            std::string& error) const;
    bool AnalyzeFaceExtents(NXOpen::Face* face, PanelFrame& frame,
                            std::string& error) const;
    bool FindPanelDirections(NXOpen::Face* face, PanelFrame& frame,
                             std::string& error) const;
    double EstimatePanelThickness(NXOpen::Body* body,
                                  const PanelFrame& frame) const;
    bool ValidateSkirtDimensions(NXOpen::Body* body,
                                 std::string& error) const;

    AdjustMode CurrentMode() const;
    RoundPolicy CurrentRoundPolicy() const;
    int CurrentAnchor() const;
    bool LivePreviewEnabled() const;
    double DoubleValue(NXOpen::BlockStyler::UIBlock* block) const;
    bool LogicalValue(NXOpen::BlockStyler::UIBlock* block) const;
    void SetDoubleValue(NXOpen::BlockStyler::UIBlock* block, double value);
    void SetLogicalValue(NXOpen::BlockStyler::UIBlock* block, bool value);
    void SetEnumValue(NXOpen::BlockStyler::UIBlock* block, int value);
    void SetLabel(NXOpen::BlockStyler::UIBlock* block,
                  const std::string& value);
    void ClearSelection();
    void LoadSettings();
    void SaveSettings() const;

    SideOffsets CalculateOffsets(std::string& error) const;
    void TargetSizeToOffsets(double targetLength, double targetWidth,
                             SideOffsets& offsets) const;
    double RoundDimension(double value, double step,
                          RoundPolicy policy) const;
    bool ValidateOffsets(const SideOffsets& offsets,
                         std::string& error) const;
    void RefreshModeVisibility();
    void RefreshDimensionText();
    void RefreshPreview();
    void ConfigureDragHandles();
    void HideDragHandles();
    bool IsDragHandle(NXOpen::BlockStyler::UIBlock* block) const;
    void ReadDragHandleValues();
    void SetDragHandleValues(const SideOffsets& offsets);

    bool CreatePreview(std::string& error);
    bool UndoPreview();
    void CommitPreview();
    bool ApplyOffsets(const SideOffsets& offsets,
                      std::vector<tag_t>& createdFeatures,
                      std::string& error) const;
    bool PullBoundaryFaces(NXOpen::Body* body,
                           const char* sideName,
                           const NXOpen::Vector3d& outwardDirection,
                           double boundaryCoordinate,
                           double distance,
                           std::vector<tag_t>& createdFeatures,
                           std::string& error) const;
    std::vector<NXOpen::Face*> FindBoundaryFaces(
        NXOpen::Body* body,
        const NXOpen::Vector3d& outwardDirection,
        double boundaryCoordinate) const;

    void ShowError(const std::string& message) const;

private:
    NXOpen::Session* session_;
    NXOpen::UI* ui_;
    NXOpen::BlockStyler::BlockDialog* dialog_;

    NXOpen::BlockStyler::UIBlock* planeSelect_;
    NXOpen::BlockStyler::UIBlock* swapDirection_;
    NXOpen::BlockStyler::UIBlock* currentSize_;
    NXOpen::BlockStyler::TabControl* adjustTabs_;
    NXOpen::BlockStyler::UIBlock* independentGroup_;
    NXOpen::BlockStyler::UIBlock* uniformGroup_;
    NXOpen::BlockStyler::UIBlock* roundGroup_;
    NXOpen::BlockStyler::UIBlock* topOffset_;
    NXOpen::BlockStyler::UIBlock* bottomOffset_;
    NXOpen::BlockStyler::UIBlock* leftOffset_;
    NXOpen::BlockStyler::UIBlock* rightOffset_;
    NXOpen::BlockStyler::UIBlock* targetLength_;
    NXOpen::BlockStyler::UIBlock* targetWidth_;
    NXOpen::BlockStyler::UIBlock* roundLength_;
    NXOpen::BlockStyler::UIBlock* roundWidth_;
    NXOpen::BlockStyler::UIBlock* lengthStep_;
    NXOpen::BlockStyler::UIBlock* widthStep_;
    NXOpen::BlockStyler::UIBlock* roundPolicy_;
    NXOpen::BlockStyler::UIBlock* anchor_;
    NXOpen::BlockStyler::UIBlock* anchorGroup_;
    NXOpen::BlockStyler::UIBlock* resultLength_;
    NXOpen::BlockStyler::UIBlock* resultWidth_;
    NXOpen::BlockStyler::UIBlock* livePreview_;
    NXOpen::BlockStyler::LinearDimension* leftHandle_;
    NXOpen::BlockStyler::LinearDimension* rightHandle_;
    NXOpen::BlockStyler::LinearDimension* bottomHandle_;
    NXOpen::BlockStyler::LinearDimension* topHandle_;

    PanelFrame frame_;
    NXOpen::Session::UndoMarkId previewMark_;
    bool hasPreview_;
    bool rebuilding_;
    bool changingUi_;
    bool configuringHandles_;
    bool targetValuesInitialized_;
    std::vector<tag_t> previewFeatures_;
};
