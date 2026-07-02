#ifndef PILIAN_ZUAN_BAN_JIN_HPP_INCLUDED
#define PILIAN_ZUAN_BAN_JIN_HPP_INCLUDED

#include <uf.h>
#include <uf_assem.h>
#include <uf_attr.h>
#include <uf_curve.h>
#include <uf_disp.h>
#include <uf_eval.h>
#include <uf_modl.h>
#include <uf_modl_types.h>
#include <uf_modl_utilities.h>
#include <uf_obj.h>
#include <uf_ui.h>

#include <NXOpen/BlockStyler_BlockDialog.hxx>
#include <NXOpen/BlockStyler_Button.hxx>
#include <NXOpen/BlockStyler_DoubleBlock.hxx>
#include <NXOpen/BlockStyler_Enumeration.hxx>
#include <NXOpen/BlockStyler_Group.hxx>
#include <NXOpen/BlockStyler_IntegerBlock.hxx>
#include <NXOpen/BlockStyler_ObjectColorPicker.hxx>
#include <NXOpen/BlockStyler_PropertyList.hxx>
#include <NXOpen/BlockStyler_SelectObject.hxx>
#include <NXOpen/BlockStyler_Toggle.hxx>
#include <NXOpen/BlockStyler_Tree.hxx>
#include <NXOpen/BlockStyler_Node.hxx>
#include <NXOpen/Assemblies_Component.hxx>
#include <NXOpen/Assemblies_ComponentAssembly.hxx>
#include <NXOpen/BasePart.hxx>
#include <NXOpen/Body.hxx>
#include <NXOpen/BodyDumbRule.hxx>
#include <NXOpen/BodyCollection.hxx>
#include <NXOpen/Callback.hxx>
#include <NXOpen/DisplayManager.hxx>
#include <NXOpen/DisplayModification.hxx>
#include <NXOpen/DisplayableObject.hxx>
#include <NXOpen/Direction.hxx>
#include <NXOpen/DirectionCollection.hxx>
#include <NXOpen/Edge.hxx>
#include <NXOpen/Expression.hxx>
#include <NXOpen/ExpressionCollection.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/Features_Feature.hxx>
#include <NXOpen/Features_FeatureCollection.hxx>
#include <NXOpen/Features_FlatPattern.hxx>
#include <NXOpen/Features_ToolingBox.hxx>
#include <NXOpen/Features_ToolingBoxBuilder.hxx>
#include <NXOpen/Features_ToolingFeatureCollection.hxx>
#include <NXOpen/Features_CurveFeatureCollection.hxx>
#include <NXOpen/Features_ShadowCurveBuilder.hxx>
#include <NXOpen/GeometricUtilities_CurveFitData.hxx>
#include <NXOpen/GeometricUtilities_CurveOptions.hxx>
#include <NXOpen/Features_SheetMetal_ConvertToSheetmetalBuilder.hxx>
#include <NXOpen/Features_SheetMetal_CornerTreatmentBuilder.hxx>
#include <NXOpen/Features_SheetMetal_FlatPatternBuilder.hxx>
#include <NXOpen/Features_SheetMetal_FlatSolidBuilder.hxx>
#include <NXOpen/Features_SheetMetal_ResizeBendRadiusBuilder.hxx>
#include <NXOpen/Features_SheetMetal_ResizeNeutralFactorBuilder.hxx>
#include <NXOpen/Features_SheetMetal_SheetmetalManager.hxx>
#include <NXOpen/FaceDumbRule.hxx>
#include <NXOpen/ListingWindow.hxx>
#include <NXOpen/MeasureFaces.hxx>
#include <NXOpen/MeasureManager.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXMessageBox.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/Plane.hxx>
#include <NXOpen/PlaneCollection.hxx>
#include <NXOpen/PlaneTypes.hxx>
#include <NXOpen/Point.hxx>
#include <NXOpen/PointCollection.hxx>
#include <NXOpen/Preferences_PartPreferences.hxx>
#include <NXOpen/Preferences_SheetMetalPreferencesBuilder.hxx>
#include <NXOpen/Preferences_SheetMetalPreferencesManager.hxx>
#include <NXOpen/ScRuleFactory.hxx>
#include <NXOpen/ScCollector.hxx>
#include <NXOpen/SelectEdge.hxx>
#include <NXOpen/SelectFace.hxx>
#include <NXOpen/SelectNXObjectList.hxx>
#include <NXOpen/Selection.hxx>
#include <NXOpen/SelectionIntentRule.hxx>
#include <NXOpen/SelectionIntentRuleOptions.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/SmartObject.hxx>
#include <NXOpen/UI.hxx>
#include <NXOpen/Tooling_ToolingSession.hxx>
#include <NXOpen/Tooling_ToolingManager.hxx>
#include <NXOpen/Tooling_StockSizeBuilder.hxx>
#include <NXOpen/Tooling_StockSizeCollection.hxx>
#include <NXOpen/SelectBodyList.hxx>
#include <NXOpen/Unit.hxx>
#include <NXOpen/UnitCollection.hxx>
#include <NXOpen/Update.hxx>

#include <map>
#include <set>
#include <string>
#include <vector>

struct AutoConvertOptions
{
    bool markerLineFaceUp = false;
    bool autoSaveAfterRun = true;
    bool preferUpBends = false;
    bool manualBaseXAxis = false;
    bool manualOnly = false;
    bool applyFixedFaceColor = true;
    int fixedFaceColor = 6;
    double reliefDepth = 0.2;
    double reliefWidth = 0.2;
    double innerRadius = 0.0;
    bool skipSmallBody = false;
    double minLength = 0.0;
    double minWidth = 0.0;
    bool skipFasteners = false;
    bool filterLayerRange = true;
    int startLayer = 1;
    int endLayer = 99;
    bool largestBodyOnlyPerLayer = false;
    int failureAction = 1;
};

class PiLianZuanBanJinDialog
{
public:
    PiLianZuanBanJinDialog();
    ~PiLianZuanBanJinDialog();

    int Show();

private:
    static NXOpen::Session* theSession;
    static NXOpen::UI* theUI;

    NXOpen::BlockStyler::BlockDialog* theDialog;
    const char* theDlxFileName;

    NXOpen::BlockStyler::Group* strategyGroup;
    NXOpen::BlockStyler::Group* advancedGroup;
    NXOpen::BlockStyler::Toggle* markerLineFaceUpToggle;
    NXOpen::BlockStyler::Toggle* autoSaveAfterRunToggle;
    NXOpen::BlockStyler::Button* manualBaseXAxisButton;
    NXOpen::BlockStyler::Enumeration* facePreferenceEnum;
    NXOpen::BlockStyler::Enumeration* failureActionEnum;
    NXOpen::BlockStyler::Toggle* fixedFaceColorToggle;
    NXOpen::BlockStyler::ObjectColorPicker* fixedFaceColorPicker;
    NXOpen::BlockStyler::DoubleBlock* reliefDepthDouble;
    NXOpen::BlockStyler::DoubleBlock* reliefWidthDouble;
    NXOpen::BlockStyler::DoubleBlock* innerRadiusDouble;
    NXOpen::BlockStyler::Toggle* skipSmallBodyToggle;
    NXOpen::BlockStyler::DoubleBlock* minLengthDouble;
    NXOpen::BlockStyler::DoubleBlock* minWidthDouble;
    NXOpen::BlockStyler::Toggle* skipTallCylinderToggle;
    NXOpen::BlockStyler::Toggle* layerRangeToggle;
    NXOpen::BlockStyler::IntegerBlock* startLayerInteger;
    NXOpen::BlockStyler::IntegerBlock* endLayerInteger;
    NXOpen::BlockStyler::Toggle* largestBodyOnlyToggle;
    NXOpen::BlockStyler::Button* settingsButton;
    bool assemblySelectionActive;
    std::vector<NXOpen::Part*> selectedAssemblyParts;
    bool runAfterDialog_;
    AutoConvertOptions pendingOptions_;
    std::set<tag_t> manualButtonProcessedBodyTags_;

    void initialize_cb();
    int update_cb(NXOpen::BlockStyler::UIBlock* block);
    int ok_cb();
    int apply_cb();

    void InitializeValues();
    void UpdateSensitivity();
    AutoConvertOptions CollectOptions() const;
    void Run(const AutoConvertOptions& options);
};

#endif



