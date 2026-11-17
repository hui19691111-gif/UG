#pragma once
#include <NXOpen/BlockStyler_BlockDialog.hxx>
#include <NXOpen/BlockStyler_UIBlock.hxx>

class TianYuanDiFanDialog {
public:
    TianYuanDiFanDialog();
    ~TianYuanDiFanDialog();
    void Launch();

private:
    void Initialize();
    void Shown();
    int Apply();
    int Update(NXOpen::BlockStyler::UIBlock* block);
    int Cancel();
    void Status(const char* text);
    void Report(const char* text) noexcept;

    void ShowSegmentFields();

    NXOpen::BlockStyler::BlockDialog* dialog_ = nullptr;
    NXOpen::BlockStyler::UIBlock* split_ = nullptr;
    NXOpen::BlockStyler::UIBlock* diameter_ = nullptr;
    NXOpen::BlockStyler::UIBlock* length_ = nullptr;
    NXOpen::BlockStyler::UIBlock* width_ = nullptr;
    NXOpen::BlockStyler::UIBlock* height_ = nullptr;
    NXOpen::BlockStyler::UIBlock* thickness_ = nullptr;
    NXOpen::BlockStyler::UIBlock* corner_ = nullptr;
    NXOpen::BlockStyler::UIBlock* seam_ = nullptr;
    NXOpen::BlockStyler::UIBlock* segmented_ = nullptr;
    NXOpen::BlockStyler::UIBlock* bendCount_ = nullptr;
    NXOpen::BlockStyler::UIBlock* indexMark_ = nullptr;
    NXOpen::BlockStyler::UIBlock* status_ = nullptr;
    bool initialized_ = false;
    bool shown_ = false;
    bool updating_ = false;
};
