#ifndef HOLEATTRIBUTE_H_INCLUDED
#define HOLEATTRIBUTE_H_INCLUDED

#include <uf.h>
#include <uf_defs.h>
#include <uf_disp.h>
#include <uf_modl.h>
#include <uf_modl_dfo.h>
#include <uf_modl_types.h>
#include <uf_modl_utilities.h>
#include <uf_obj.h>
#include <uf_smd.h>
#include <uf_ui_types.h>

#include <NXOpen/BlockStyler_BlockDialog.hxx>
#include <NXOpen/BlockStyler_Node.hxx>
#include <NXOpen/BlockStyler_PropertyList.hxx>
#include <NXOpen/BlockStyler_Tree.hxx>
#include <NXOpen/BlockStyler_UIBlock.hxx>
#include <NXOpen/Body.hxx>
#include <NXOpen/Callback.hxx>
#include <NXOpen/DisplayableObject.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/Features_FeatureCollection.hxx>
#include <NXOpen/Features_SheetMetal_SheetmetalManager.hxx>
#include <NXOpen/ListingWindow.hxx>
#include <NXOpen/IParameterizedSurface.hxx>
#include <NXOpen/MeasureFaces.hxx>
#include <NXOpen/MeasureManager.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXMessageBox.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/NXString.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/UnitCollection.hxx>
#include <NXOpen/Update.hxx>
#include <NXOpen/UI.hxx>

#include <map>
#include <string>
#include <vector>

class DllExport HoleAttribute
{
public:
    static NXOpen::Session* theSession;
    static NXOpen::UI* theUI;

    HoleAttribute();
    ~HoleAttribute();

    NXOpen::BlockStyler::BlockDialog::DialogResponse Show();

    void initialize_cb();
    void dialogShown_cb();
    int apply_cb();
    int ok_cb();
    int update_cb(NXOpen::BlockStyler::UIBlock* block);

    void OnSelectCallback(NXOpen::BlockStyler::Tree* tree, NXOpen::BlockStyler::Node* node, int columnID, bool selected);
    NXOpen::BlockStyler::Tree::BeginLabelEditState OnBeginLabelEditCallback(NXOpen::BlockStyler::Tree* tree, NXOpen::BlockStyler::Node* node, int columnID);
    NXOpen::BlockStyler::Tree::EndLabelEditState OnEndLabelEditCallback(NXOpen::BlockStyler::Tree* tree, NXOpen::BlockStyler::Node* node, int columnID, NXOpen::NXString editedText);
    NXOpen::BlockStyler::Tree::ControlType AskEditControlCallback(NXOpen::BlockStyler::Tree* tree, NXOpen::BlockStyler::Node* node, int columnID);
    NXOpen::BlockStyler::Tree::EditControlOption OnEditOptionSelectedCallback(NXOpen::BlockStyler::Tree* tree, NXOpen::BlockStyler::Node* node, int columnID, int selectedOptionID, NXOpen::NXString selectedOptionText, NXOpen::BlockStyler::Tree::ControlType type);

public:
    enum Columns
    {
        ColumnDiameter = 0,
        ColumnCount = 1,
        ColumnType = 2,
        ColumnSpec = 3,
        ColumnRemark = 4
    };

    struct HoleGroup
    {
        double diameter;
        double height;
        std::string type;
        std::string spec;
        std::string remark;
        std::vector<NXOpen::Face*> faces;
        NXOpen::BlockStyler::Node* node;
    };

    struct SpecRule
    {
        double diameter;
        double minDiameter;
        double maxDiameter;
        bool isRange;
        double minHeight;
        double maxHeight;
        bool hasHeightRange;
        std::string type;
        std::string spec;
        std::vector<std::string> levelSpecs;
        std::string remark;
        std::string sourceTitle;
    };

private:
    std::string theDlxFileName;
    std::string ruleFileName;
    NXOpen::BlockStyler::BlockDialog* theDialog;
    NXOpen::BlockStyler::UIBlock* selection0;
    NXOpen::BlockStyler::Tree* tree_control0;
    NXOpen::BlockStyler::UIBlock* addCrossSelectionNodeButton;
    NXOpen::BlockStyler::UIBlock* openRuleTableButton;
    NXOpen::BlockStyler::UIBlock* autoTapHoleToggle;
    NXOpen::BlockStyler::UIBlock* autoPemHoleToggle;
    NXOpen::BlockStyler::UIBlock* autoCounterboreHoleToggle;
    void* ruleManager;

    std::vector<HoleGroup> groups;
    std::vector<SpecRule> specRules;
    std::vector<std::string> specRuleTypes;
    std::map<NXOpen::BlockStyler::Node*, size_t> nodeToGroup;
    std::vector<NXOpen::Face*> highlightedFaces;
    int highlightedGroupIndex;
    bool columnsInserted;
    bool isEditingDiameter;
    bool specRulesLoaded;
    bool autoTapHoleEnabled;
    bool autoPemHoleEnabled;
    bool autoCounterboreHoleEnabled;

    void configureSelectionBlock();
    void localizeDialogBlocks();
    void loadSpecRules();
    void ensureSpecRulesLoaded();
    void refreshAutoRecognitionToggles();
    void applyAutoRecognition(HoleGroup& group);
    void applyAutoRecognitionToAll();
    const SpecRule* autoRuleFor(double diameter, double height) const;
    void applyCounterboreRecognitionToAll();
    void rebuildFromSelection();
    void rebuildTree();
    bool askCylindricalFace(NXOpen::TaggedObject* object, NXOpen::Face** face, double* diameter, double* height);
    bool isWholeCylindricalFace(tag_t faceTag) const;
    bool isHoleCylindricalFace(tag_t faceTag, const double axisPoint[3], const double axisDirection[3]) const;
    bool isSheetMetalBodyFace(tag_t faceTag, double holeHeight) const;
    double sheetBodyThicknessForFace(tag_t faceTag) const;
    bool askCylindricalFaceHeight(tag_t faceTag, double* height) const;
    int findGroupForDiameter(double diameter, double height) const;
    void applyDiameterChange(size_t groupIndex, double newDiameter);
    void writeGroupFaceAttributes(size_t groupIndex);
    void writeAllFaceAttributes();
    void highlightGroup(size_t groupIndex);
    void clearHighlight(bool refresh = true);
    void writeLine(const std::string& text);
    void showError(const std::string& text);
    std::string specFor(const std::string& type, double diameter, double height);
    std::string remarkFor(const std::string& type, double diameter, double height);
    std::vector<std::string> typeOptions();
    std::string defaultType();
    std::string formatDiameter(double value) const;
    std::string formatCount(size_t value) const;
    static std::string defaultSpec(const std::string& type, double diameter);
};

#endif
