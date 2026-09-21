#pragma once
#include "BendSimulation.hpp"
#include <NXOpen/BlockStyler_BlockDialog.hxx>
#include <NXOpen/BlockStyler_UIBlock.hxx>
#include <NXOpen/ugmath.hxx>
namespace NXOpen {class TaggedObject;class ModelingView;namespace BlockStyler {class Tree;class Node;}}
class ZeWanMoNiDialog {
public:
    ZeWanMoNiDialog();
    ~ZeWanMoNiDialog();
    void Launch();
private:
    void Initialize();
    void Shown();
    int Update(NXOpen::BlockStyler::UIBlock*);
    int Filter(NXOpen::BlockStyler::UIBlock*,NXOpen::TaggedObject*);
    int Apply();
    int Cancel();
    void Preview();
    void RunCheck();
    void LoadTools();
    void PopulateTools();
    void ShowToolProfile();
    void ToolSelected(NXOpen::BlockStyler::Tree*,NXOpen::BlockStyler::Node*,int,bool);
    void Draw(const bend_sim::Placement&,const bend_sim::Result&);
    void DescribePlacement(const bend_sim::Placement&);
    void Status(const std::string&);
    void Error(const std::string&) noexcept;
    void RestoreView();
    bend_sim::Settings Settings() const;
    bend_sim::Placement Placement() const;
    NXOpen::BlockStyler::BlockDialog* dialog_=nullptr;
    NXOpen::BlockStyler::UIBlock *selection_=nullptr,*tool_=nullptr,*reverse_=nullptr,*check_=nullptr,*reload_=nullptr,*folder_=nullptr,*status_=nullptr,*detail_=nullptr;
    NXOpen::BlockStyler::UIBlock *toolImage_=nullptr,*toolInfo_=nullptr;
    NXOpen::ModelingView* view_=nullptr;
    NXOpen::Matrix3x3 originalMatrix_={};
    std::vector<bend_sim::Tool> tools_;
    std::vector<NXOpen::BlockStyler::Node*> toolNodes_;
    int activeTool_=0;
    std::vector<NXOpen::TaggedObject*> applySelection_;
    std::string checkedStatus_;
    bend_sim::Result checkedResult_;
    bool initialized_=false,shown_=false,updating_=false,viewChanged_=false;
};
