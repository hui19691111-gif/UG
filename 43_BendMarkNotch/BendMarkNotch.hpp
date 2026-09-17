#pragma once

#include <NXOpen/BlockStyler_BlockDialog.hxx>
#include <NXOpen/BlockStyler_UIBlock.hxx>
#include <NXOpen/NXObject.hxx>
#include <NXOpen/Session.hxx>
#include <uf_defs.h>

#include <string>
#include <vector>

namespace NXOpen
{
class Body;
class Edge;
class Face;
class TaggedObject;
class UI;
namespace Features
{
class Feature;
}
}

class BendMarkNotchDialog
{
public:
    BendMarkNotchDialog();
    ~BendMarkNotchDialog();
    NXOpen::BlockStyler::BlockDialog::DialogResponse Launch();

private:
    struct BendRecord
    {
        NXOpen::Face* innerFace = nullptr;
        NXOpen::Face* outerFace = nullptr;
        NXOpen::Edge* firstBoundary = nullptr;
        NXOpen::Edge* secondBoundary = nullptr;
        double innerRadius = 0.0;
    };

    struct FlatBend
    {
        NXOpen::Point3d firstEnd;
        NXOpen::Point3d secondEnd;
        bool upward = true;
    };

    struct NotchControls
    {
        NXOpen::BlockStyler::UIBlock* type = nullptr;
        NXOpen::BlockStyler::UIBlock* diameter = nullptr;
        NXOpen::BlockStyler::UIBlock* angle = nullptr;
        NXOpen::BlockStyler::UIBlock* depth = nullptr;
        NXOpen::BlockStyler::UIBlock* rectangleWidth = nullptr;
        NXOpen::BlockStyler::UIBlock* rectangleDepth = nullptr;
    };

    struct NotchSettings
    {
        int type = 0;
        double diameter = 1.0;
        double angle = 60.0;
        double depth = 1.0;
        double width = 1.0;
    };

    struct ToolRecord
    {
        tag_t featureTag = NULL_TAG;
        std::vector<tag_t> bodyTags;
    };

    struct LargeBendSettings
    {
        double radiusThicknessRatio = 5.0;
        // One interval means tangent boundaries only; N adds N-1 interior marks.
        int segments = 1;
    };

    struct NotchProfile
    {
        NXOpen::Point3d center;
        std::vector<NXOpen::Point3d> polygon;
        double circleRadius = 0.0;
    };

    void initialize_cb();
    void dialogShown_cb();
    int update_cb(NXOpen::BlockStyler::UIBlock* block);
    int filter_cb(NXOpen::BlockStyler::UIBlock* block,
                  NXOpen::TaggedObject* selectedObject);
    int apply_cb();
    int ok_cb();
    int cancel_cb();

    std::string DialogPath() const;
    void UpdateControlState();
    std::vector<NXOpen::Body*> TargetBodies() const;
    bool IsSheetMetalBody(NXOpen::Body* body) const;
    NotchControls FindNotchControls(const std::string& prefix) const;
    int NotchType(const NotchControls& controls) const;
    NotchSettings ReadNotchSettings(const NotchControls& controls) const;
    void ValidateNotchSettings(const NotchSettings& settings,
                              const std::string& label) const;
    LargeBendSettings ReadLargeBendSettings() const;
    double DoubleValue(NXOpen::BlockStyler::UIBlock* block) const;
    bool ToggleValue(NXOpen::BlockStyler::UIBlock* block) const;
    void SetStatus(const std::string& text) const;
    void ShowError(const std::string& text) const noexcept;

    int Execute();
    bool CreateCustomFeatureNode();
    NXOpen::Features::Feature* FindFlatPatternFeature(
        NXOpen::Body* body) const;
    NXOpen::Features::Feature* FindFeatureBefore(
        NXOpen::Features::Feature* feature) const;
    NXOpen::Features::Feature* FindBodyInsertionFeature(
        const std::vector<NXOpen::Body*>& bodies) const;
    NXOpen::Face* FlatPatternUpwardFace(NXOpen::Features::Feature* feature) const;
    int ProcessBody(NXOpen::Body* body, NXOpen::Face* upwardFace);
    NXOpen::Face* FindReferenceFace(NXOpen::Body* body) const;
    std::vector<BendRecord> CollectBends(NXOpen::Body* body,
                                         double thickness) const;
    NXOpen::Face* FindOppositeBendFace(NXOpen::Body* body,
                                       NXOpen::Face* innerFace,
                                       double thickness) const;
    bool FindBoundaryEdges(NXOpen::Face* bendFace,
                           NXOpen::Edge*& first,
                           NXOpen::Edge*& second) const;
    FlatBend ResolveFlatBend(const BendRecord& bend) const;
    std::vector<FlatBend> ResolveLargeBendMarks(
        const BendRecord& bend, int segments) const;
    NXOpen::Vector3d ReferenceNormal(NXOpen::Face* referenceFace) const;
    bool CreateInternalSketchExtrudeTool(
        const NXOpen::Vector3d& normal,
        const std::vector<NotchProfile>& profiles,
        double thickness, ToolRecord& tool) const;
    bool SubtractToolsOnce(NXOpen::Body* body,
                           const std::vector<ToolRecord>& tools,
                           NXOpen::Features::Feature*& booleanFeature) const;

private:
    NXOpen::UI* ui_;
    NXOpen::Session* session_;
    NXOpen::BlockStyler::BlockDialog* dialog_;
    NXOpen::BlockStyler::UIBlock* autoSelect_;
    NXOpen::BlockStyler::UIBlock* bodySelect_;
    NXOpen::BlockStyler::UIBlock* distinguishBends_;
    NXOpen::BlockStyler::UIBlock* directionHelp_;
    NXOpen::BlockStyler::UIBlock* largeBendRatio_;
    NXOpen::BlockStyler::UIBlock* divideLargeBend_;
    NXOpen::BlockStyler::UIBlock* largeBendSegments_;
    NXOpen::BlockStyler::UIBlock* largeBendHelp_;
    NotchControls commonNotch_;
    NotchControls upNotch_;
    NotchControls downNotch_;
    NXOpen::BlockStyler::UIBlock* status_;
    bool initialized_;
    bool shown_;
    bool updating_;
    std::vector<tag_t> pendingInternalFeatureTags_;
};
