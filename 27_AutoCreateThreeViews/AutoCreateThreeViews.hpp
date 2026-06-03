#pragma once

#include <NXOpen/BlockStyler_BlockDialog.hxx>
#include <NXOpen/BlockStyler_UIBlock.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/UI.hxx>

#include <filesystem>
#include <string>

class AutoCreateThreeViewsDialog
{
public:
    AutoCreateThreeViewsDialog();
    ~AutoCreateThreeViewsDialog();

    NXOpen::BlockStyler::BlockDialog::DialogResponse Launch();

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
        bool createRightView;
        bool createIsoView;
        bool createFlatPatternView;
        bool autoDimensions;
        bool exportPdf;
        bool exportDwg;
    };

    void initialize_cb();
    void dialog_shown_cb();
    int update_cb(NXOpen::BlockStyler::UIBlock* block);
    int apply_cb();
    int ok_cb();

    std::string GetDialogFilePath() const;
    DialogValues ReadDialogValues() const;
    std::string ReadString(const char* blockId, const std::string& fallback) const;
    bool ReadLogical(const char* blockId, bool fallback) const;
    void SetString(const char* blockId, const std::string& value) const;
    void SetLogical(const char* blockId, bool value) const;
    void ShowInfo(const std::string& message) const;
    void ShowError(const std::string& message) const;
    void Log(const std::string& message) const;
    int ExecuteCreateDrawing();

private:
    NXOpen::UI* ui_;
    NXOpen::Session* session_;
    NXOpen::BlockStyler::BlockDialog* dialog_;
};

int ExecuteAutoCreateThreeViewsFromRequest(const std::filesystem::path& requestPath);
void BeginAutoCreateThreeViewsRunResults();
void AddAutoCreateThreeViewsRunResultLine(const std::string& line);
void ShowAutoCreateThreeViewsRunResults();
