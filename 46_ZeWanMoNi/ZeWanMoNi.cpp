#include "ZeWanMoNi.hpp"
#include "ToolThumbnails.hpp"
#include <NXOpen/BlockStyler_Tree.hxx>
#include <NXOpen/BlockStyler_Node.hxx>
#include <NXOpen/BlockStyler_DrawingArea.hxx>
#include <NXOpen/BlockStyler_PropertyList.hxx>
#include <NXOpen/BlockStyler_SelectObject.hxx>
#include <NXOpen/Callback.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/Edge.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXMessageBox.hxx>
#include <NXOpen/NXString.hxx>
#include <NXOpen/Selection.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/ModelingView.hxx>
#include <NXOpen/ModelingViewCollection.hxx>
#include <NXOpen/UI.hxx>
#include <uf.h>
#include <uf_disp.h>
#include <uf_ui_types.h>
#include <Windows.h>
#include <shellapi.h>
#ifdef CreateDialog
#undef CreateDialog
#endif
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>

namespace {
using Props=std::unique_ptr<NXOpen::BlockStyler::PropertyList>;
struct Guard{bool& value;Guard(bool& b):value(b){value=true;}~Guard(){value=false;}};
std::filesystem::path ModuleDir(){HMODULE m=nullptr;GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&ModuleDir),&m);wchar_t p[32768]={};if(!GetModuleFileNameW(m,p,32768))throw std::runtime_error("无法定位折弯模拟资源。");return std::filesystem::path(p).parent_path();}
std::filesystem::path ToolDir(){const wchar_t* p=_wgetenv(L"APPDATA");if(!p||!*p)throw std::runtime_error("无法定位用户刀具目录。");return std::filesystem::path(p)/L"Zhihui"/L"ZeWanMoNi"/L"tools";}
void Log(const std::string& s) noexcept {try{wchar_t p[MAX_PATH]={};GetTempPathW(MAX_PATH,p);std::ofstream(std::filesystem::path(p)/L"Zhihui-ZeWanMoNi.log",std::ios::app)<<s<<'\n';}catch(...) {}}
bool Toggle(NXOpen::BlockStyler::UIBlock* b){return Props(b->GetProperties())->GetLogical("Value");}
NXOpen::BlockStyler::SelectObject* Selector(NXOpen::BlockStyler::UIBlock* b){auto* s=dynamic_cast<NXOpen::BlockStyler::SelectObject*>(b);if(!s)throw std::runtime_error("选择控件不可用。");return s;}
NXOpen::BlockStyler::Tree* ToolTree(NXOpen::BlockStyler::UIBlock* b){auto* t=dynamic_cast<NXOpen::BlockStyler::Tree*>(b);if(!t)throw std::runtime_error("刀具列表控件不可用。");return t;}
}
ZeWanMoNiDialog::ZeWanMoNiDialog(){
    auto file=(ModuleDir()/L"ZeWanMoNi.dlx").u8string();dialog_=NXOpen::UI::GetUI()->CreateDialog(file.c_str());
    dialog_->AddInitializeHandler(NXOpen::make_callback(this,&ZeWanMoNiDialog::Initialize));
    dialog_->AddDialogShownHandler(NXOpen::make_callback(this,&ZeWanMoNiDialog::Shown));
    dialog_->AddUpdateHandler(NXOpen::make_callback(this,&ZeWanMoNiDialog::Update));
    dialog_->AddFilterHandler(NXOpen::make_callback(this,&ZeWanMoNiDialog::Filter));
    dialog_->AddApplyHandler(NXOpen::make_callback(this,&ZeWanMoNiDialog::Apply));
    dialog_->AddOkHandler(NXOpen::make_callback(this,&ZeWanMoNiDialog::Apply));
    dialog_->AddCancelHandler(NXOpen::make_callback(this,&ZeWanMoNiDialog::Cancel));
}
ZeWanMoNiDialog::~ZeWanMoNiDialog(){try{RestoreView();UF_DISP_refresh();}catch(const NXOpen::NXException& e){Log(e.Message());}catch(...){Log("cleanup failed");}delete dialog_;}
void ZeWanMoNiDialog::Launch(){dialog_->Launch();}
void ZeWanMoNiDialog::Status(const std::string& s){Props(status_->GetProperties())->SetString("Label",s.c_str());}
void ZeWanMoNiDialog::Error(const std::string& s) noexcept {Log(s);try{NXOpen::UI::GetUI()->NXMessageBox()->Show("折弯模拟",NXOpen::NXMessageBox::DialogTypeError,s.c_str());}catch(...) {}}
void ZeWanMoNiDialog::LoadTools(){
    auto items=bend_sim::BuiltinTools();auto dir=ToolDir();std::vector<std::filesystem::path> paths;
    if(std::filesystem::exists(dir))for(const auto& entry:std::filesystem::directory_iterator(dir))if(entry.is_regular_file()&&entry.path().extension()==L".ztool")paths.push_back(entry.path());
    std::sort(paths.begin(),paths.end());if(paths.size()>100)throw std::runtime_error("自定义刀具最多加载 100 把。");
    for(const auto& path:paths){try{auto t=bend_sim::ReadTool(path);t.name="自定义："+t.name;items.push_back(t);}catch(const std::exception& e){throw std::runtime_error(path.filename().u8string()+"："+e.what());}}
    std::string previous=activeTool_>=0&&static_cast<size_t>(activeTool_)<tools_.size()?tools_[activeTool_].name:"";
    activeTool_=0;for(size_t i=0;i<items.size();++i)if(items[i].name==previous){activeTool_=static_cast<int>(i);break;}
    tools_=std::move(items);if(shown_)PopulateTools();
}
void ZeWanMoNiDialog::PopulateTools(){
    using Tree=NXOpen::BlockStyler::Tree;auto* tree=ToolTree(tool_);
    if(tree->NumberOfColumns()==0){
        tree->InsertColumn(0,NXOpen::NXString("刀具名称",NXOpen::NXString::UTF8),235);
        tree->InsertColumn(1,NXOpen::NXString("截面",NXOpen::NXString::UTF8),90);
        tree->SetColumnDisplayType(1,Tree::ColumnDisplayIcon);
        tree->SetColumnSortable(0,false);tree->SetColumnSortable(1,false);
    }
    // Apply recreates NX's tree; never reuse node handles from the previous UI.
    toolNodes_.clear();while(auto* node=tree->RootNode())tree->DeleteNode(node);
    auto cache=ToolDir().parent_path()/L"thumbnails";
    for(const auto& tool:tools_){
        auto file=ToolThumbnail(tool,cache).u8string();
        auto* node=tree->CreateNode(NXOpen::NXString(tool.name.c_str(),NXOpen::NXString::UTF8));
        tree->InsertNode(node,nullptr,nullptr,Tree::NodeInsertOptionLast);
        toolNodes_.push_back(node);
        node->SetColumnDisplayText(1,NXOpen::NXString(file.c_str(),NXOpen::NXString::UTF8));
    }
    if(!toolNodes_.empty())tree->SelectNode(toolNodes_.at(activeTool_),true,true);
    ShowToolProfile();
}
void ZeWanMoNiDialog::ShowToolProfile(){
    const auto& tool=tools_.at(activeTool_);
    auto* image=dynamic_cast<NXOpen::BlockStyler::DrawingArea*>(toolImage_);
    if(!image)throw std::runtime_error("截面预览控件不可用。");
    auto file=ToolThumbnail(tool,ToolDir().parent_path()/L"thumbnails",true).u8string();
    image->SetImage(NXOpen::NXString(file.c_str(),NXOpen::NXString::UTF8));
    double xmin=0,xmax=0,height=0;for(auto p:tool.profile){xmin=std::min(xmin,p.x);xmax=std::max(xmax,p.x);height=std::max(height,p.z);}
    std::ostringstream info;info<<std::fixed<<std::setprecision(1)<<"截面预览：宽 "<<xmax-xmin<<" × 高 "<<height<<" mm";
    Props(toolInfo_->GetProperties())->SetString("Label",info.str().c_str());
}
void ZeWanMoNiDialog::ToolSelected(NXOpen::BlockStyler::Tree*,NXOpen::BlockStyler::Node* node,int,bool selected){
    if(!initialized_||!shown_||updating_||!selected)return;Guard guard(updating_);
    try{
        auto it=std::find(toolNodes_.begin(),toolNodes_.end(),node);if(it==toolNodes_.end())return;
        activeTool_=static_cast<int>(it-toolNodes_.begin());ShowToolProfile();Preview();
    }catch(const NXOpen::NXException& e){Log(e.Message());try{UF_DISP_refresh();Status(std::string("未完成：")+e.Message());}catch(...) {}}
    catch(const std::exception& e){Log(e.what());try{UF_DISP_refresh();Status(std::string("未完成：")+e.what());}catch(...) {}}
    catch(...){Log("tool selection failed");try{UF_DISP_refresh();Status("刀具选择失败，请重新选择。");}catch(...) {}}
}
void ZeWanMoNiDialog::Initialize(){
    // NX invokes Initialize again after Apply, before the new tree is usable.
    initialized_=false;shown_=false;toolNodes_.clear();
    try{
        auto find=[&](const char* id){auto* b=dialog_->TopBlock()->FindBlock(id);if(!b)throw std::runtime_error(std::string("缺少对话框控件：")+id);return b;};
        selection_=find("bend_selection");tool_=find("tool_choice");reverse_=find("reverse_tool");check_=find("check_button");reload_=find("reload_tools");folder_=find("tool_folder");status_=find("result_status");detail_=find("bend_detail");
        toolImage_=find("tool_profile_image");toolInfo_=find("tool_profile_info");
        ToolTree(tool_)->SetOnSelectHandler(NXOpen::make_callback(this,&ZeWanMoNiDialog::ToolSelected));
        auto* s=Selector(selection_);s->SetSelectionFilter(NXOpen::Selection::SelectionActionClearAndEnableSpecific,{{UF_solid_type,0,UF_UI_SEL_FEATURE_ANY_FACE},{UF_solid_type,0,UF_UI_SEL_FEATURE_ANY_EDGE}});s->SetSelectModeAsString("Single");s->SetAutomaticProgression(false);
        LoadTools();initialized_=true;
    }catch(const NXOpen::NXException& e){Error(e.Message());}catch(const std::exception& e){Error(e.what());}catch(...){Error("初始化失败。");}
}
void ZeWanMoNiDialog::Shown(){
    try{if(!initialized_)return;Guard guard(updating_);
        auto* part=NXOpen::Session::GetSession()->Parts()->Work();if(!part)throw std::runtime_error("请先打开零件。");
        if(!view_){view_=part->ModelingViews()->WorkView();originalMatrix_=view_->Matrix();}
        shown_=true;
        PopulateTools();
        // NX restarts the dialog after Apply and clears its selection collector.
        // Restore only this session's selection and already computed result.
        if(!applySelection_.empty()){
            Selector(selection_)->SetSelectedObjects(applySelection_);applySelection_.clear();
            UF_DISP_refresh();auto p=Placement();DescribePlacement(p);Draw(p,checkedResult_);Status(checkedStatus_);
        }
        selection_->Focus();
    }catch(const NXOpen::NXException& e){Error(e.Message());}catch(const std::exception& e){Error(e.what());}catch(...){Error("显示对话框失败。");}
}
int ZeWanMoNiDialog::Filter(NXOpen::BlockStyler::UIBlock*,NXOpen::TaggedObject* object){
    try{if(!initialized_)return UF_UI_SEL_REJECT;
        if(auto* f=dynamic_cast<NXOpen::Face*>(object))return !f->IsOccurrence()&&f->SolidFaceType()==NXOpen::Face::FaceTypeCylindrical?UF_UI_SEL_ACCEPT:UF_UI_SEL_REJECT;
        auto* e=dynamic_cast<NXOpen::Edge*>(object);
        if(!e||e->IsOccurrence()||e->SolidEdgeType()!=NXOpen::Edge::EdgeTypeLinear)return UF_UI_SEL_REJECT;
        // A round bend's tangent edge is not a sharp bend. Reject it here so
        // NX can pick the narrow cylindrical face underneath instead.
        auto faces=e->GetFaces();
        return faces.size()==2&&faces[0]->SolidFaceType()==NXOpen::Face::FaceTypePlanar&&faces[1]->SolidFaceType()==NXOpen::Face::FaceTypePlanar?UF_UI_SEL_ACCEPT:UF_UI_SEL_REJECT;
    }catch(const NXOpen::NXException& e){Log(e.Message());}catch(...){Log("filter failed");}return UF_UI_SEL_REJECT;
}
bend_sim::Settings ZeWanMoNiDialog::Settings() const {
    // No hidden saved numeric controls: use the selected bend's length and axis.
    // A true sharp edge has R=0; do not invent a radius absent from the model.
    bend_sim::Settings s;s.innerRadius=0;s.reverse=Toggle(reverse_);return s;
}
bend_sim::Placement ZeWanMoNiDialog::Placement() const {
    auto selected=Selector(selection_)->GetSelectedObjects();if(selected.size()!=1)throw std::runtime_error("请选择一处折弯内圆柱面或内侧锐边。");
    auto settings=Settings();auto b=bend_sim::Inspect(selected[0]->Tag(),settings.innerRadius);
    int choice=activeTool_;if(choice<0||static_cast<size_t>(choice)>=tools_.size())throw std::runtime_error("刀具选择无效。");
    return bend_sim::Place(b,tools_[choice],settings);
}
void ZeWanMoNiDialog::Draw(const bend_sim::Placement& p,const bend_sim::Result& result){
    auto lines=[&](const std::vector<std::pair<bend_sim::Vec,bend_sim::Vec>>& edges,UF_DISP_color_name_t color,int width){
        UF_OBJ_disp_props_t props={};bend_sim::Check(UF_DISP_ask_closest_color_in_displayed_part(color,&props.color));props.font=1;props.line_width=width;
        for(const auto& e:edges){double a[]={e.first.x,e.first.y,e.first.z},b[]={e.second.x,e.second.y,e.second.z};bend_sim::Check(UF_DISP_display_temporary_line(NULL_TAG,UF_DISP_USE_WORK_VIEW,a,b,&props));}
    };
    lines(bend_sim::Outline(p),UF_DISP_YELLOW_NAME,UF_OBJ_WIDTH_NORMAL);
    lines(result.interferenceLines,UF_DISP_RED_NAME,UF_OBJ_WIDTH_THICK);
}
void ZeWanMoNiDialog::RestoreView(){if(view_&&viewChanged_){view_->Orient(originalMatrix_);viewChanged_=false;}}
void ZeWanMoNiDialog::DescribePlacement(const bend_sim::Placement& p){
    std::ostringstream info;info<<std::fixed<<std::setprecision(2);
    if(p.bend.sharp)info<<"锐边原位";else info<<"内 R "<<p.bend.radius/p.bend.unitsPerMm;
    info<<" / 自动刀长 "<<p.length/p.bend.unitsPerMm<<" mm";
    Props(detail_->GetProperties())->SetString("Label",info.str().c_str());
}
void ZeWanMoNiDialog::Preview(){
    UF_DISP_refresh();checkedResult_={};checkedStatus_.clear();Status("请选择折弯位置，自动检查干涉。");Props(detail_->GetProperties())->SetString("Label","自动识别内圆柱面或内侧锐边。");
    if(Selector(selection_)->GetSelectedObjects().empty())return;
    auto p=Placement();
    if(view_){auto x=p.x,y=p.up,z=bend_sim::Unit(bend_sim::Cross(x,y));NXOpen::Matrix3x3 m={x.x,x.y,x.z,y.x,y.y,y.z,z.x,z.y,z.z};viewChanged_=true;view_->Orient(m);}
    DescribePlacement(p);Status("正在检查当前姿态…");
    checkedResult_=bend_sim::InspectPlacement(p,Settings().safeGap);
    checkedStatus_=bend_sim::Describe(checkedResult_);Draw(p,checkedResult_);Status(checkedStatus_);Log(p.tool.name+" | "+checkedStatus_);
}
void ZeWanMoNiDialog::RunCheck(){
    if(Selector(selection_)->GetSelectedObjects().empty())throw std::runtime_error("请先选择折弯位置。");Preview();
}
int ZeWanMoNiDialog::Update(NXOpen::BlockStyler::UIBlock* block){
    if(!initialized_||!shown_||updating_||block==tool_)return 0;Guard guard(updating_);
    try{
        if(block==check_){RunCheck();return 0;}
        if(block==folder_){auto dir=ToolDir();std::filesystem::create_directories(dir);auto sample=dir/L"example.ztool";if(!std::filesystem::exists(sample))std::filesystem::copy_file(ModuleDir()/L"ZeWanMoNiExample.ztool",sample);if(reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr,L"open",dir.c_str(),nullptr,nullptr,SW_SHOWNORMAL))<=32)throw std::runtime_error("无法打开刀具目录。");return 0;}
        if(block==reload_)LoadTools();
        Preview();
    }catch(const NXOpen::NXException& e){Log(e.Message());try{UF_DISP_refresh();Status(std::string("未完成：")+e.Message());}catch(...) {}}
    catch(const std::exception& e){Log(e.what());try{UF_DISP_refresh();Status(std::string("未完成：")+e.what());}catch(...) {}}
    catch(...){Log("update failed");try{UF_DISP_refresh();Status("检查失败，请重新选择输入。");}catch(...) {}}
    return 0;
}
int ZeWanMoNiDialog::Apply(){if(!initialized_||!shown_||updating_)return 1;Guard guard(updating_);try{RunCheck();applySelection_=Selector(selection_)->GetSelectedObjects();return 0;}catch(const NXOpen::NXException& e){Error(e.Message());}catch(const std::exception& e){Error(e.what());}catch(...){Error("检查失败。");}try{UF_DISP_refresh();Status("检查失败，结果无效。");}catch(...){}return 1;}
int ZeWanMoNiDialog::Cancel(){try{RestoreView();UF_DISP_refresh();}catch(const NXOpen::NXException& e){Log(e.Message());}catch(...){Log("cancel failed");}return 0;}
