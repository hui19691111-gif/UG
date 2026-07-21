#pragma once

namespace zhihui_zhewan_birangcao
{
constexpr const char* kFeatureClassName = "NXOpen::CustomFeature::ZheWanBiRangCao";
constexpr const char* kFeatureDisplayName = "\xE6\x8A\x98\xE5\xBC\xAF\xE9\x81\xBF\xE8\xAE\xA9\xE6\xA7\xBD";

constexpr const char* kAttrTargetBody = "targetBody";
constexpr const char* kAttrSelectedFace = "selectedFace";
constexpr const char* kAttrSelectedEdge = "selectedEdge";
constexpr const char* kAttrSlotWidth = "slotWidth";
constexpr const char* kAttrSlotDepth = "slotDepth";
constexpr const char* kAttrThickness = "sheetThickness";
constexpr const char* kAttrPickPoint = "pickPoint";
constexpr const char* kAttrToolTransforms = "toolTransforms";

// Transient hand-off from the dialog DLL to the custom-feature Core DLL.
// The value is a comma-separated list of preview feature tags and is consumed
// only while CommitFeature is running.
constexpr const char* kPreviewConstructionEnvironment =
    "ZHIHUI_ZHEWANBIRANGCAO_PREVIEW_CONSTRUCTION";

constexpr int kTransformValueCount = 12;
}
