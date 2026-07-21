#pragma once

namespace zhihui_twopoint_sibian
{
constexpr const char* kFeatureClassName = "NXOpen::CustomFeature::TwoPointSiBian";
constexpr const char* kFeatureDisplayName = "2P_SiBian";

constexpr const char* kAttrStartPoint = "startPoint";
constexpr const char* kAttrEndPoint = "endPoint";
constexpr const char* kAttrTargetBody = "targetBody";
constexpr const char* kAttrBaseFace = "baseFace";
constexpr const char* kAttrStartEdge = "startEdge";
constexpr const char* kAttrEndEdge = "endEdge";
constexpr const char* kAttrStartX = "startX";
constexpr const char* kAttrStartY = "startY";
constexpr const char* kAttrStartZ = "startZ";
constexpr const char* kAttrEndX = "endX";
constexpr const char* kAttrEndY = "endY";
constexpr const char* kAttrEndZ = "endZ";
constexpr const char* kAttrThickness = "sheetThickness";
constexpr const char* kAttrSpanLength = "spanLength";
constexpr const char* kAttrClearance = "clearance";
constexpr const char* kAttrBendRadius = "bendRadius";
constexpr const char* kAttrFeatureMode = "featureMode";
constexpr const char* kAttrSmartMode = "smartMode";
constexpr const char* kAttrChamferEdgeMode = "chamferEdgeMode";
constexpr const char* kAttrReverseCut = "reverseCut";
constexpr const char* kAttrInferredFromSingleClick = "inferredFromSingleClick";
constexpr const char* kAttrClickX = "clickX";
constexpr const char* kAttrClickY = "clickY";
constexpr const char* kAttrClickZ = "clickZ";
constexpr const char* kAttrSchemaVersion = "schemaVersion";
constexpr const char* kConstructionRoleAttribute = "ZHIHUI_2P_ROLE";

// Stable construction-member roles.  CustomFeature edits use these names to
// change the existing members in place instead of rebuilding the feature
// chain and invalidating sheet-metal topology.
constexpr const char* kRoleUdf = "2P_SiBian.Internal.UDF";
constexpr const char* kRoleEdgeRip = "2P_SiBian.Internal.EdgeRip";
constexpr const char* kRoleOffsetNegativeClearance =
    "2P_SiBian.Internal.Offset.NegativeClearance";
constexpr const char* kRoleOffsetSmallDirectional =
    "2P_SiBian.Internal.Offset.SmallDirectional";
constexpr const char* kRoleOffsetThicknessPlus002 =
    "2P_SiBian.Internal.Offset.ThicknessPlus002";
constexpr const char* kRoleOffsetThicknessPlusClearance =
    "2P_SiBian.Internal.Offset.ThicknessPlusClearance";
constexpr const char* kRoleOffsetNegative60 =
    "2P_SiBian.Internal.Offset.Negative60";
constexpr const char* kRoleExtrudeFarTopCut =
    "2P_SiBian.Internal.Extrude.FarTopCut";
constexpr const char* kRoleExtrudeCornerEdgeCut =
    "2P_SiBian.Internal.Extrude.CornerEdgeCut";
constexpr const char* kRoleExtrudeRightClearanceRectangle =
    "2P_SiBian.Internal.Extrude.RightClearanceRectangle";
constexpr const char* kRoleExtrudeLeftClearanceRectangle =
    "2P_SiBian.Internal.Extrude.LeftClearanceRectangle";
}
