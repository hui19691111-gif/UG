#include "FanTonSenZi.hpp"
#include <NXOpen/BlockStyler_PropertyList.hxx>
#include <NXOpen/BlockStyler_Label.hxx>
#include <NXOpen/BlockStyler_SelectObject.hxx>
#include <NXOpen/Callback.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXMessageBox.hxx>
#include <NXOpen/Selection.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/UI.hxx>
#include <uf_disp.h>
#include <uf_ui_types.h>
#include <uf_object_types.h>
#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>
#include "../../common/ZhihuiDialogMemory.hpp"
#ifdef CreateDialog
#undef CreateDialog
#endif
namespace {
using Props=std::unique_ptr<NXOpen::BlockStyler::PropertyList>;
struct Guard {bool& value;explicit Guard(bool& b):value(b){value=true;}~Guard(){value=false;}};
const char* ids[]={"divisions","radius","kfactor","gap","bridge_width","tube_kfactor"};
const wchar_t* keys[]={L"Divisions",L"Radius",L"KFactor",L"Gap",L"RoundBridgeWidth",L"TubeKFactor"};
const double defaults[]={12,1,.4,.2,6,.5};
constexpr const wchar_t* memory=L"FanTonSenZi.ini";
auto Select(NXOpen::BlockStyler::UIBlock* b){return dynamic_cast<NXOpen::BlockStyler::SelectObject*>(b);}
void Log(const std::string& s)noexcept{try{std::ofstream(std::filesystem::temp_directory_path()/"Zhihui-FanTonSenZi.log",std::ios::app)<<s<<'\n';}catch(...){}}
std::filesystem::path ModuleDir(){HMODULE module=nullptr;GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&ModuleDir),&module);wchar_t file[MAX_PATH]={};if(!GetModuleFileNameW(module,file,MAX_PATH))throw std::runtime_error("找不到方通伸直资源。");return std::filesystem::path(file).parent_path();}
struct QuietDisplay {
    int previous=UF_DISP_UNSUPPRESS_DISPLAY;bool active=false;
    QuietDisplay(){tube_straighten::Check(UF_DISP_ask_display(&previous));tube_straighten::Check(UF_DISP_set_display(UF_DISP_SUPPRESS_DISPLAY));active=true;}
    ~QuietDisplay(){if(active){UF_DISP_set_display(previous);if(previous==UF_DISP_UNSUPPRESS_DISPLAY)UF_DISP_regenerate_display();}}
};
}
FanTonSenZiDialog::FanTonSenZiDialog(){
    dialog_=NXOpen::UI::GetUI()->CreateDialog((ModuleDir()/"FanTonSenZi.dlx").u8string().c_str());
    dialog_->AddInitializeHandler(NXOpen::make_callback(this,&FanTonSenZiDialog::Initialize));
    dialog_->AddDialogShownHandler(NXOpen::make_callback(this,&FanTonSenZiDialog::Shown));
    dialog_->AddUpdateHandler(NXOpen::make_callback(this,&FanTonSenZiDialog::Update));
    dialog_->AddFilterHandler(NXOpen::make_callback(this,&FanTonSenZiDialog::Filter));
    dialog_->AddApplyHandler(NXOpen::make_callback(this,&FanTonSenZiDialog::Apply));
    dialog_->AddOkHandler(NXOpen::make_callback(this,&FanTonSenZiDialog::Apply));
    dialog_->AddCancelHandler(NXOpen::make_callback(this,&FanTonSenZiDialog::Cancel));
}
FanTonSenZiDialog::~FanTonSenZiDialog(){UF_DISP_refresh();delete dialog_;}
void FanTonSenZiDialog::Launch(){dialog_->Launch();}
void FanTonSenZiDialog::Initialize(){
    shown_=false;initialized_=false;
    try{
        edges_=dialog_->TopBlock()->FindBlock("path_edges");hide_=dialog_->TopBlock()->FindBlock("hide_source");cutSource_=dialog_->TopBlock()->FindBlock("cut_source");status_=dialog_->TopBlock()->FindBlock("result_status");detail_=dialog_->TopBlock()->FindBlock("result_detail");
        segmentArcs_=dialog_->TopBlock()->FindBlock("segment_arcs");
        if(!edges_||!hide_||!cutSource_||!segmentArcs_||!status_||!detail_)throw std::runtime_error("方通伸直对话框资源不完整。");
        dynamic_cast<NXOpen::BlockStyler::Label*>(status_)->SetWordWrap(true);
        dynamic_cast<NXOpen::BlockStyler::Label*>(detail_)->SetWordWrap(true);
        for(int i=0;i<6;++i){numbers_[i]=dialog_->TopBlock()->FindBlock(ids[i]);if(!numbers_[i])throw std::runtime_error("缺少参数控件。");}
        Select(edges_)->SetSelectionFilter(NXOpen::Selection::SelectionActionClearAndEnableSpecific,{{UF_solid_type,0,UF_UI_SEL_FEATURE_ANY_FACE}});
        Select(edges_)->SetSelectModeAsString("Single");Select(edges_)->SetAutomaticProgression(false);initialized_=true;
    }catch(const NXOpen::NXException& e){Error(e.Message());}catch(const std::exception& e){Error(e.what());}catch(...){Error("初始化方通伸直失败。");}
}
void FanTonSenZiDialog::Shown(){
    try{if(!initialized_)return;shown_=true;Guard guard(updating_);for(int i=0;i<6;++i)Props(numbers_[i]->GetProperties())->SetDouble("Value",zhihui_dialog_memory::ReadDouble(memory,keys[i],defaults[i]));edges_->Focus();Preview();}
    catch(const NXOpen::NXException& e){Error(e.Message());}catch(const std::exception& e){Error(e.what());}catch(...){Error("显示方通伸直失败。");}
}
int FanTonSenZiDialog::Filter(NXOpen::BlockStyler::UIBlock*,NXOpen::TaggedObject* object){
    try{auto* face=dynamic_cast<NXOpen::Face*>(object);if(face&&!face->IsOccurrence())return UF_UI_SEL_ACCEPT;}
    catch(const NXOpen::NXException& e){Log(e.Message());}catch(...){Log("Face filter failed");}return UF_UI_SEL_REJECT;
}
tube_straighten::Plan FanTonSenZiDialog::ReadPlan(){
    std::vector<tag_t> tags;for(auto* o:Select(edges_)->GetSelectedObjects())tags.push_back(o->Tag());tube_straighten::Settings s;
    s.segmentArcs=Props(segmentArcs_->GetProperties())->GetLogical("Value");
    if(s.segmentArcs){double n=Props(numbers_[0]->GetProperties())->GetDouble("Value");if(!std::isfinite(n)||n<2||n>180||std::abs(n-std::round(n))>1e-8)throw std::runtime_error("圆弧分段数须为 2–180 的整数。");s.divisions=static_cast<int>(n);}
    s.radiusMm=Props(numbers_[1]->GetProperties())->GetDouble("Value");s.kFactor=Props(numbers_[2]->GetProperties())->GetDouble("Value");s.gapMm=Props(numbers_[3]->GetProperties())->GetDouble("Value");s.bridgeWidthMm=Props(numbers_[4]->GetProperties())->GetDouble("Value");s.tubeKFactor=Props(numbers_[5]->GetProperties())->GetDouble("Value");s.cutSource=Props(cutSource_->GetProperties())->GetLogical("Value");s.hideSource=Props(hide_->GetProperties())->GetLogical("Value");if(tags.size()!=1)throw std::runtime_error("请选择方通完整侧平面，或圆管上的一个面。");return tube_straighten::MakePlan(tube_straighten::InspectFace(tags.front()),s);
}
void FanTonSenZiDialog::Status(const std::string& s){auto split=s.find('\n');Props(status_->GetProperties())->SetString("Label",s.substr(0,split).c_str());Props(detail_->GetProperties())->SetString("Label",split==std::string::npos?" ":s.substr(split+1).c_str());}
void FanTonSenZiDialog::Error(const std::string& s)noexcept{Log(s);try{NXOpen::UI::GetUI()->NXMessageBox()->Show("方通伸直",NXOpen::NXMessageBox::DialogTypeError,s.c_str());}catch(...){}}
void FanTonSenZiDialog::Controls(bool segmented,bool hasCuts,bool round){
    numbers_[0]->SetEnable(segmented);numbers_[5]->SetShow(!segmented);
    for(int i=1;i<=3;++i)numbers_[i]->SetEnable(hasCuts);numbers_[4]->SetEnable(hasCuts&&round);
    cutSource_->SetEnable(hasCuts);hide_->SetEnable(!(hasCuts&&Props(cutSource_->GetProperties())->GetLogical("Value")));
}
void FanTonSenZiDialog::Preview(){
    bool segmented=Props(segmentArcs_->GetProperties())->GetLogical("Value"),cutSource=Props(cutSource_->GetProperties())->GetLogical("Value");Controls(segmented,true,true);
    UF_DISP_refresh();if(Select(edges_)->GetSelectedObjects().empty()){Status(!segmented?"圆弧整体伸直供弯管机成型；真实转角仍开槽。\n弯管 K=0.5 按截面中心线计长，圆弧半径从模型读取。":(cutSource?"原管开槽，同时生成伸直下料件。\n请选择未开槽的原管；间隙须至少为 0.01 mm。":"方通选完整侧平面；圆管选管面或圆环端口。\n自动识别管型；圆管选端口可指定伸直起始端。"));return;}auto p=ReadPlan();Controls(segmented,!p.bends.empty(),p.source.round);
    UF_OBJ_disp_props_t props={};props.color=186;props.font=1;props.line_width=2;
    for(auto line:tube_straighten::Preview(p)){double a[]={line.first.x,line.first.y,line.first.z},b[]={line.second.x,line.second.y,line.second.z};tube_straighten::Check(UF_DISP_display_temporary_line(NULL_TAG,UF_DISP_USE_ACTIVE_PLUS,a,b,&props));}
    double u=p.source.unitsPerMm;std::ostringstream info;info<<std::fixed<<std::setprecision(3);
    if(p.source.round){info<<"圆管 Φ"<<p.source.depth/u;if(!p.bends.empty())info<<"；连接带弧宽 "<<p.settings.bridgeWidthMm;}
    else info<<"方通 "<<p.source.width/u<<" × "<<p.source.depth/u;
    info<<"；壁厚 "<<p.source.thickness/u<<" mm\n伸直长度 "<<p.length/u<<" mm；"<<p.bends.size()<<" 切口";
    if(segmented)info<<"；分段偏差 ≤ "<<p.errorMm<<" mm";else info<<"；"<<p.machineArcs.size()<<" 段圆弧整体伸直；弯管 K="<<p.settings.tubeKFactor;
    if(!p.source.round)info<<"；保留 "<<p.source.holes.size()<<" 处管壁孔槽"<<(p.adjustedCuts?"（已避孔调整分段）":"");if(p.settings.cutSource)info<<"；原管同步开槽 "<<p.settings.gapMm<<" mm";Status(info.str());
}
int FanTonSenZiDialog::Update(NXOpen::BlockStyler::UIBlock*){
    if(!initialized_||!shown_||updating_)return 0;Guard guard(updating_);
    try{Preview();}catch(const NXOpen::NXException& e){Log(e.Message());try{Status(e.Message());}catch(...){}}catch(const std::exception& e){Log(e.what());try{Status(e.what());}catch(...){}}catch(...){Log("Preview failed");}return 0;
}
int FanTonSenZiDialog::Apply(){
    if(!initialized_||!shown_||updating_)return 1;Guard guard(updating_);auto* session=NXOpen::Session::GetSession();
    try{
        auto plan=ReadPlan();auto mark=session->SetUndoMark(NXOpen::Session::MarkVisibilityVisible,"方通伸直");
        {QuietDisplay quiet;try{tube_straighten::Create(plan);}catch(...){try{session->UndoToMark(mark,nullptr);session->DeleteUndoMark(mark,nullptr);}catch(const NXOpen::NXException& e){Log("回滚失败："+std::string(e.Message()));}throw;}}
        for(int i=0;i<6;++i)zhihui_dialog_memory::SaveDouble(memory,keys[i],numbers_[i]);Select(edges_)->SetSelectedObjects({});UF_DISP_refresh();Controls(plan.settings.segmentArcs,true,true);
        if(!plan.machineArcs.empty())Status(plan.bends.empty()?"已生成供弯管机成型的连续直管，圆弧区域未开槽。\n原管已保留；起始端与原管重合，直段孔槽保持原尺寸。":(plan.settings.cutSource?"已整体伸直圆弧并切出转角切口，原管仅在转角同步开槽。\n圆弧区域保持连续管壁；整次操作可一次撤销。":"已整体伸直圆弧，真实转角保留原开槽方式。\n圆弧区域保持连续管壁；原模型已保留。"));
        else Status(plan.settings.cutSource?"已在原管切间隙槽，并生成伸直下料件。\n原管保持原形状和连接壁厚；可一次撤销两项结果。":(plan.source.round?"已生成圆管伸直实体，外侧连接带保留完整壁厚。\n起始端与原管重合；连接带宽按外圆弧长计。":"已生成方通伸直实体，保留外侧整面连接壁。\n原模型已保留；孔槽随所在段展开，切口避开孔槽。"));edges_->Focus();return 0;
    }catch(const NXOpen::NXException& e){Error(e.Message());}catch(const std::exception& e){Error(e.what());}catch(...){Error("创建方通伸直失败。");}UF_DISP_refresh();return 1;
}
int FanTonSenZiDialog::Cancel(){try{UF_DISP_refresh();return 0;}catch(const NXOpen::NXException& e){Log(e.Message());}catch(...){Log("Cancel failed");}return 0;}
