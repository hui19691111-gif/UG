#pragma once

#include <NXOpen/BlockStyler_BlockDialog.hxx>
#include <NXOpen/BlockStyler_Node.hxx>
#include <NXOpen/BlockStyler_Tree.hxx>
#include <NXOpen/BlockStyler_UIBlock.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/UI.hxx>
#include <uf_defs.h>

#include <filesystem>
#include <map>
#include <string>
#include <vector>

class AutoCreateThreeViewsDialog
{
public:
    AutoCreateThreeViewsDialog();
    ~AutoCreateThreeViewsDialog();

    NXOpen::BlockStyler::BlockDialog::DialogResponse Launch();
    bool ClassicUiRequested() const;
    bool HasPendingDrawing() const;
    int ExecutePendingDrawing();

private:
    struct DialogValues
    {
        std::string templateName;
        std::string projectionMode;
        std::string mainViewDirection;
        std::string scaleMode;
        std::string outputDirectory;
        std::string fileNamePattern;
        bool useCurrentWorkPart;
        bool createFrontView;
        bool createTopView;
        bool createBottomView;
        bool createLeftView;
        bool createRightView;
        bool createBackView;
        bool createBackBottomView;
        bool createIsoView;
        bool createFlatPatternView;
        bool showHiddenLines;
        std::string isoCorner;
        std::string flatCorner;
        bool auxiliaryAutoCompact;
        bool autoDimensions;
        bool dimensionOverall;
        bool dimensionAngle;
        bool dimensionHole;
        bool dimensionHoleLocation;
        bool dimensionInnerClosedCurve;
        bool technicalRequirementEnabled;
        bool technicalRequirementIndexed;
        std::string technicalRequirementCorner;
        std::string technicalRequirementText;
        bool inheritDraftingPreferences;
        bool classicUi;
        bool exportPdf;
        bool exportDwg;
        double viewSpacing;
        double sheetMargin;
        double viewGroupSpacing;
    };

    void initialize_cb();
    void dialog_shown_cb();
    int update_cb(NXOpen::BlockStyler::UIBlock* block);
    int ok_cb();

    std::string GetDialogFilePath() const;
    DialogValues ReadDialogValues() const;
    std::string ReadString(const char* blockId, const std::string& fallback) const;
    std::string ReadMultilineString(const char* blockId) const;
    bool ReadLogical(const char* blockId, bool fallback) const;
    void SetString(const char* blockId, const std::string& value) const;
    void SetMultilineString(const char* blockId, const std::string& value) const;
    void SetLogical(const char* blockId, bool value) const;
    void SetBlockString(const char* blockId, const char* propertyName, const std::string& value) const;
    void ConfigureNativeEnums() const;
    void ConfigureNativeViewBitmaps() const;
    void ApplyNativeProjectionLayout(bool preserveSelection);
    void LoadNativeDialogSettings();
    void SaveNativeDialogSettings() const;
    void InitializeNativeAssemblyList();
    void PopulateNativeAssemblyList();
    void OnNativeAssemblyStateChange(
        NXOpen::BlockStyler::Tree* tree,
        NXOpen::BlockStyler::Node* node,
        int state);
    void ApplyNativeAssemblyFilters();
    std::vector<tag_t> SelectedNativeOccurrenceTags() const;
    void InitializeTechnicalRequirementLibrary();
    void LoadTechnicalRequirementLibrary();
    void SaveTechnicalRequirementLibrary();
    void OnTechnicalRequirementSelect(
        NXOpen::BlockStyler::Tree* tree,
        NXOpen::BlockStyler::Node* node,
        int columnId,
        bool selected);
    void AppendTechnicalRequirement(const std::string& text);
    void AddTechnicalRequirementCategory();
    void AddTechnicalRequirementDetail();
    void UpdateTechnicalRequirementNode();
    void DeleteTechnicalRequirementNode();
    void UpdateNativePreview();
    std::filesystem::path BuildNativePreviewBitmap(const DialogValues& values);
    void ShowInfo(const std::string& message) const;
    void ShowError(const std::string& message) const;
    void Log(const std::string& message) const;
    int ExecuteCreateDrawing();

private:
    NXOpen::UI* ui_;
    NXOpen::Session* session_;
    NXOpen::BlockStyler::BlockDialog* dialog_;
    bool classicUiRequested_;
    unsigned int previewRevision_;
    int previewWidth_;
    int previewHeight_;
    bool projectionLayoutInitialized_;
    bool projectionLayoutThirdAngle_;
    NXOpen::BlockStyler::Tree* assemblyTree_;
    bool assemblyStateUpdateInProgress_;
    std::vector<NXOpen::BlockStyler::Node*> assemblyNodes_;
    std::map<NXOpen::BlockStyler::Node*, tag_t> assemblyNodeOccurrences_;
    NXOpen::BlockStyler::Tree* technicalRequirementTree_;
    NXOpen::BlockStyler::Node* technicalRequirementSelectedNode_;
    std::map<NXOpen::BlockStyler::Node*, bool> technicalRequirementDetailNodes_;
    std::filesystem::path pendingRequestPath_;
};

int ExecuteAutoCreateThreeViewsFromRequest(const std::filesystem::path& requestPath);
tag_t AskLastAutoCreateThreeViewsDrawingSheetTag();
void ClearAutoCreateThreeViewsManualDirectionCache();
bool PreselectAutoCreateThreeViewsManualDirection(tag_t partTag, int targetLayer);
int ExecuteAutoCreateThreeViewsUiRequest(const std::filesystem::path& requestPath);
int ScheduleAutoCreateThreeViewsUiRequest(const std::filesystem::path& requestPath);
void CompleteAutoCreateThreeViewsNativeProgress();
void BeginAutoCreateThreeViewsRunResults();
void AddAutoCreateThreeViewsRunResultLine(const std::string& line);
void ShowAutoCreateThreeViewsRunResults();
