#pragma once
#include "TubeGeometry.hpp"
#include <NXOpen/BlockStyler_BlockDialog.hxx>
#include <NXOpen/BlockStyler_UIBlock.hxx>
namespace NXOpen {class TaggedObject;namespace Features {class CustomFeature;}}
class FanTonSenZiDialog {
public:
    FanTonSenZiDialog();
    ~FanTonSenZiDialog();
    void Launch();
private:
    void Initialize();void Shown();
    int Update(NXOpen::BlockStyler::UIBlock*);
    int Filter(NXOpen::BlockStyler::UIBlock*,NXOpen::TaggedObject*);
    int Apply();int Cancel();
    tube_straighten::Plan ReadPlan();
    tube_straighten::Settings ReadSettings();
    void Preview();void Controls(bool segmented,bool hasCuts,bool round);void Status(const std::string&);void Error(const std::string&)noexcept;
    NXOpen::BlockStyler::BlockDialog* dialog_=nullptr;
    NXOpen::BlockStyler::UIBlock *edges_=nullptr,*numbers_[6]={},*hide_=nullptr,*cutSource_=nullptr,*segmentArcs_=nullptr,*status_=nullptr,*detail_=nullptr;
    NXOpen::Features::CustomFeature* edited_=nullptr;
    bool initialized_=false,shown_=false,updating_=false;
    bool anchored_=false;
    tube_straighten::Vec anchor_;
};
