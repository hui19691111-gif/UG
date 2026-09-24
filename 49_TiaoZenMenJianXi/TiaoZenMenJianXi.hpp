#pragma once

#include <NXOpen/BlockStyler_BlockDialog.hxx>
#include <NXOpen/Session.hxx>
#include <uf_defs.h>

#include <array>
#include <string>

namespace NXOpen {
class Body;
class Face;
class Session;
class UI;
namespace BlockStyler { class UIBlock; class LinearDimension; }
}
class DoorGapOverlay;

class DoorGapDialog final {
public:
    DoorGapDialog();
    ~DoorGapDialog();
    NXOpen::BlockStyler::BlockDialog::DialogResponse Launch();

private:
    struct Side {
        const char* name = "";
        NXOpen::Vector3d outward;
        double boundary = 0;
        double gap = 0;
        tag_t referenceFace = NULL_TAG;
        tag_t referenceBody = NULL_TAG;
        NXOpen::Point3d handleOrigin;
        bool measured = false;
    };

    void Initialize();
    void DialogShown();
    int Update(NXOpen::BlockStyler::UIBlock* block);
    int Apply();
    int Ok();
    int Filter(NXOpen::BlockStyler::UIBlock* block, NXOpen::TaggedObject* object);
    bool Analyze(NXOpen::Face* face, std::string& error);
    bool MeasureSide(Side& side, const std::array<double, 4>& extents,
                     double thickness, std::string& error);
    bool BodyExtents(NXOpen::Body* body, std::array<double, 4>& extents) const;
    bool MoveSide(int index, double distance, std::string& error);
    bool ValidateResult(const std::array<double, 4>& desired,
                        std::string& error);
    void ShowValues();
    void ShowHandles();
    void HideHandles() noexcept;
    void SetLabel(NXOpen::BlockStyler::UIBlock* block,
                  const std::string& value);
    double Value(NXOpen::BlockStyler::UIBlock* block) const;
    void SetValue(NXOpen::BlockStyler::UIBlock* block, double value);
    NXOpen::Face* SelectedFace() const;
    void Error(const std::string& message) const noexcept;
    void Log(const std::string& message) const noexcept;

    NXOpen::Session* session_ = nullptr;
    NXOpen::UI* ui_ = nullptr;
    NXOpen::BlockStyler::BlockDialog* dialog_ = nullptr;
    DoorGapOverlay* overlay_ = nullptr;
    NXOpen::BlockStyler::UIBlock* selection_ = nullptr;
    NXOpen::BlockStyler::UIBlock* status_ = nullptr;
    std::array<NXOpen::BlockStyler::UIBlock*, 4> inputs_{};
    std::array<NXOpen::BlockStyler::UIBlock*, 4> labels_{};
    std::array<NXOpen::BlockStyler::LinearDimension*, 4> handles_{};
    std::array<Side, 4> sides_{};
    NXOpen::Point3d origin_;
    NXOpen::Vector3d normal_;
    NXOpen::Vector3d u_;
    NXOpen::Vector3d v_;
    tag_t selectedFace_ = NULL_TAG;
    tag_t prototypeFace_ = NULL_TAG;
    tag_t selectedBody_ = NULL_TAG;
    tag_t prototypeBody_ = NULL_TAG;
    tag_t selectedComponent_ = NULL_TAG;
    double thickness_ = 0;
    double transform_[4][4]{};
    bool valid_ = false;
    bool changingUi_ = false;
    bool updating_ = false;
};
