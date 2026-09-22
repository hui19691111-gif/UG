#pragma once
#include "TubeGeometry.hpp"
#include <NXOpen/BlockStyler_BlockDialog.hxx>
#include <NXOpen/BlockStyler_UIBlock.hxx>
namespace NXOpen {class TaggedObject;}
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
    void Preview();void Status(const std::string&);void Error(const std::string&)noexcept;
    NXOpen::BlockStyler::BlockDialog* dialog_=nullptr;
    NXOpen::BlockStyler::UIBlock *edges_=nullptr,*numbers_[5]={},*hide_=nullptr,*cutSource_=nullptr,*status_=nullptr,*detail_=nullptr;
    bool initialized_=false,shown_=false,updating_=false;
};
