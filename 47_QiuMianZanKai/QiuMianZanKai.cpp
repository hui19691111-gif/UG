#include "QiuMianZanKai.hpp"
#include "SphereCustomFeature.hpp"
#include <NXOpen/BlockStyler_PropertyList.hxx>
#include <NXOpen/BlockStyler_Label.hxx>
#include <NXOpen/BlockStyler_SelectObject.hxx>
#include <NXOpen/Callback.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/Features_CustomFeature.hxx>
#include <NXOpen/Features_CustomFeatureClassManager.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXMessageBox.hxx>
#include <NXOpen/Selection.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/UI.hxx>
#include <uf_disp.h>
#include <uf_obj.h>
#include <uf_ui_types.h>
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
struct Guard{bool& v;explicit Guard(bool& b):v(b){v=true;}~Guard(){v=false;}};
const char* ids[]={"petals","gap","relief"};
const wchar_t* keys[]={L"Petals",L"Gap",L"Relief"};
constexpr const wchar_t* memory=L"QiuMianZanKai.ini";
auto Select(NXOpen::BlockStyler::UIBlock* b){return dynamic_cast<NXOpen::BlockStyler::SelectObject*>(b);}
bool Toggle(NXOpen::BlockStyler::UIBlock* b){return Props(b->GetProperties())->GetLogical("Value");}
void Log(const std::string& s)noexcept{try{std::ofstream(std::filesystem::temp_directory_path()/"Zhihui-QiuMianZanKai.log",std::ios::app)<<s<<'\n';}catch(...){}}
std::filesystem::path ModuleDir(){HMODULE module=nullptr;GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&ModuleDir),&module);wchar_t file[MAX_PATH]={};if(!GetModuleFileNameW(module,file,MAX_PATH))throw std::runtime_error("找不到球面展开资源目录。");return std::filesystem::path(file).parent_path();}
struct QuietDisplay {
    int previous=UF_DISP_UNSUPPRESS_DISPLAY;bool active=false;
    QuietDisplay(){sphere_unfold::Check(UF_DISP_ask_display(&previous));sphere_unfold::Check(UF_DISP_set_display(UF_DISP_SUPPRESS_DISPLAY));active=true;}
    ~QuietDisplay(){Restore();}
    void Restore()noexcept{if(active){UF_DISP_set_display(previous);if(previous==UF_DISP_UNSUPPRESS_DISPLAY)UF_DISP_regenerate_display();active=false;}}
};
}
QiuMianZanKaiDialog::QiuMianZanKaiDialog(){
    sphere_unfold::RequireFeatureClass();
    edited_=NXOpen::Session::GetSession()->CustomFeatureClassManager()->GetEditedCustomFeature();
    dialog_=NXOpen::UI::GetUI()->CreateDialog((ModuleDir()/"QiuMianZanKai.dlx").u8string().c_str());
    dialog_->AddInitializeHandler(NXOpen::make_callback(this,&QiuMianZanKaiDialog::Initialize));
    dialog_->AddDialogShownHandler(NXOpen::make_callback(this,&QiuMianZanKaiDialog::Shown));
    dialog_->AddUpdateHandler(NXOpen::make_callback(this,&QiuMianZanKaiDialog::Update));
    dialog_->AddFilterHandler(NXOpen::make_callback(this,&QiuMianZanKaiDialog::Filter));
    dialog_->AddApplyHandler(NXOpen::make_callback(this,&QiuMianZanKaiDialog::Apply));
    dialog_->AddOkHandler(NXOpen::make_callback(this,&QiuMianZanKaiDialog::Apply));
    dialog_->AddCancelHandler(NXOpen::make_callback(this,&QiuMianZanKaiDialog::Cancel));
}
QiuMianZanKaiDialog::~QiuMianZanKaiDialog(){UF_DISP_refresh();delete dialog_;}
void QiuMianZanKaiDialog::Launch(){dialog_->LaunchInDialogMode(edited_?NXOpen::BlockStyler::BlockDialog::DialogModeEdit:NXOpen::BlockStyler::BlockDialog::DialogModeCreate);}
void QiuMianZanKaiDialog::Initialize(){
    shown_=false;initialized_=false;
    try{
        cylinder_=dialog_->TopBlock()->FindBlock("cylinder");sphere_=dialog_->TopBlock()->FindBlock("sphere");flat_=dialog_->TopBlock()->FindBlock("create_flat");hide_=dialog_->TopBlock()->FindBlock("hide_source");status_=dialog_->TopBlock()->FindBlock("result_status");
        detail_=dialog_->TopBlock()->FindBlock("result_detail");
        if(!cylinder_||!sphere_||!flat_||!hide_||!status_||!detail_)throw std::runtime_error("球面展开对话框资源不完整。");
        dynamic_cast<NXOpen::BlockStyler::Label*>(status_)->SetWordWrap(true);
        dynamic_cast<NXOpen::BlockStyler::Label*>(detail_)->SetWordWrap(true);
        for(int i=0;i<3;++i){numbers_[i]=dialog_->TopBlock()->FindBlock(ids[i]);if(!numbers_[i])throw std::runtime_error("缺少分瓣参数。");if(first_&&!edited_)zhihui_dialog_memory::LoadDouble(memory,keys[i],numbers_[i]);}
        if(first_&&edited_){
            auto s=sphere_unfold::ReadFeature(edited_,editCylinder_,editSphere_);
            double values[]={static_cast<double>(s.petals),s.gap,s.relief};for(int i=0;i<3;++i)Props(numbers_[i]->GetProperties())->SetDouble("Value",values[i]);
            Props(flat_->GetProperties())->SetLogical("Value",s.flat);Props(hide_->GetProperties())->SetLogical("Value",s.hideSource);
        }
        for(auto* block:{cylinder_,sphere_}){auto* select=Select(block);if(!select)throw std::runtime_error("选择控件错误。");select->SetSelectionFilter(NXOpen::Selection::SelectionActionClearAndEnableSpecific,{{UF_solid_type,0,UF_UI_SEL_FEATURE_ANY_FACE}});select->SetSelectModeAsString("Single");select->SetAutomaticProgression(false);}
        initialized_=true;first_=false;
    }catch(const NXOpen::NXException& e){Error(e.Message());}catch(const std::exception& e){Error(e.what());}catch(...){Error("初始化球面展开失败。");}
}
void QiuMianZanKaiDialog::Shown(){
    try{if(!initialized_)return;shown_=true;Guard guard(updating_);
        // NX restores its cached dialog values after Initialize. Reapply our
        // keyed settings here so removing controls cannot shift old values and
        // editing always shows the feature's own parameters.
        if(edited_){
            auto s=sphere_unfold::ReadFeature(edited_,editCylinder_,editSphere_);
            double values[]={static_cast<double>(s.petals),s.gap,s.relief};for(int i=0;i<3;++i)Props(numbers_[i]->GetProperties())->SetDouble("Value",values[i]);
            Props(flat_->GetProperties())->SetLogical("Value",s.flat);Props(hide_->GetProperties())->SetLogical("Value",s.hideSource);
            Select(cylinder_)->SetSelectedObjects({NXOpen::NXObjectManager::Get(editCylinder_)});Select(sphere_)->SetSelectedObjects({NXOpen::NXObjectManager::Get(editSphere_)});
        }else{
            double defaults[]={12,.5,3};for(int i=0;i<3;++i)Props(numbers_[i]->GetProperties())->SetDouble("Value",zhihui_dialog_memory::ReadDouble(memory,keys[i],defaults[i]));
        }
        cylinder_->Focus();Preview();}
    catch(const NXOpen::NXException& e){Error(e.Message());}catch(const std::exception& e){Error(e.what());}catch(...){Error("显示球面展开失败。");}
}
int QiuMianZanKaiDialog::Filter(NXOpen::BlockStyler::UIBlock* block,NXOpen::TaggedObject* object){
    try{auto* f=dynamic_cast<NXOpen::Face*>(object);if(f&&!f->IsOccurrence()&&((block==cylinder_&&f->SolidFaceType()==NXOpen::Face::FaceTypeCylindrical)||(block==sphere_&&(f->SolidFaceType()==NXOpen::Face::FaceTypeSpherical||f->SolidFaceType()==NXOpen::Face::FaceTypeSurfaceOfRevolution))))return UF_UI_SEL_ACCEPT;}
    catch(const NXOpen::NXException& e){Log(e.Message());}catch(...){Log("Face filter failed");}return UF_UI_SEL_REJECT;
}
sphere_unfold::Settings QiuMianZanKaiDialog::ReadSettings(){
    sphere_unfold::Settings s;double n=Props(numbers_[0]->GetProperties())->GetDouble("Value");
    if(!std::isfinite(n)||n<2||n>180||std::abs(n-std::round(n))>1e-8)throw std::runtime_error("瓣数必须是 2–180 的整数。");
    s.petals=static_cast<int>(n);s.gap=Props(numbers_[1]->GetProperties())->GetDouble("Value");s.relief=Props(numbers_[2]->GetProperties())->GetDouble("Value");s.flat=Toggle(flat_);s.hideSource=Toggle(hide_);return s;
}
void QiuMianZanKaiDialog::Status(const std::string& s){auto split=s.find('\n');Props(status_->GetProperties())->SetString("Label",s.substr(0,split).c_str());Props(detail_->GetProperties())->SetString("Label",split==std::string::npos?" ":s.substr(split+1).c_str());}
void QiuMianZanKaiDialog::Error(const std::string& s)noexcept{Log(s);try{NXOpen::UI::GetUI()->NXMessageBox()->Show("球面展开",NXOpen::NXMessageBox::DialogTypeError,s.c_str());}catch(...){}}
void QiuMianZanKaiDialog::Preview(){
    UF_DISP_refresh();auto c=Select(cylinder_)->GetSelectedObjects(),f=Select(sphere_)->GetSelectedObjects();
    if(c.empty()||f.empty()){Status("请选择圆柱面及相接的球面或环面。");return;}
    auto plan=sphere_unfold::MakePlan(sphere_unfold::Inspect(c[0]->Tag(),f[0]->Tag()),ReadSettings());
    UF_OBJ_disp_props_t props={};props.color=186;props.font=1;props.line_width=2;
    for(auto line:sphere_unfold::Preview(plan)){double a[]={line.first.x,line.first.y,line.first.z},b[]={line.second.x,line.second.y,line.second.z};UF_DISP_display_temporary_line(NULL_TAG,UF_DISP_USE_ACTIVE_PLUS,a,b,&props);}
    std::ostringstream info;info<<std::fixed<<std::setprecision(3)<<"自动板厚 "<<plan.source.thickness/plan.source.unitsPerMm<<" mm；"<<(plan.source.IsTorus()?"环面":"球面")<<" / "<<(plan.source.innerSurface?"内侧面":"外侧面")<<"\n偏差 ≤ "<<plan.errorMm<<" mm；根部缝 "<<plan.rootGap/plan.source.unitsPerMm<<" mm";Status(info.str());
}
int QiuMianZanKaiDialog::Update(NXOpen::BlockStyler::UIBlock* block){
    if(!initialized_||!shown_||updating_)return 0;Guard guard(updating_);
    try{if(block==cylinder_&&!Select(cylinder_)->GetSelectedObjects().empty()&&Select(sphere_)->GetSelectedObjects().empty())sphere_->Focus();Preview();}
    catch(const NXOpen::NXException& e){Log(e.Message());try{Status(e.Message());}catch(...){}}catch(const std::exception& e){Log(e.what());try{Status(e.what());}catch(...){}}catch(...){Log("Preview failed");}return 0;
}
int QiuMianZanKaiDialog::Apply(){
    if(!initialized_||!shown_||updating_)return 1;Guard guard(updating_);auto* session=NXOpen::Session::GetSession();
    try{
        auto c=Select(cylinder_)->GetSelectedObjects(),f=Select(sphere_)->GetSelectedObjects();if(c.size()!=1||f.size()!=1)throw std::runtime_error("请分别选择一个圆柱面和一个相接的球面或环面。");
        auto settings=ReadSettings();auto plan=sphere_unfold::MakePlan(sphere_unfold::Inspect(c[0]->Tag(),f[0]->Tag()),settings);
        std::ostringstream diagnostic;diagnostic<<"Create: petals="<<settings.petals<<" auto_thickness_mm="<<plan.source.thickness/plan.source.unitsPerMm<<" gap_mm="<<settings.gap<<" relief_mm="<<settings.relief<<" auto_inner="<<plan.source.innerSurface<<" flat="<<settings.flat<<" edit="<<(edited_!=nullptr)<<" torus="<<plan.source.IsTorus()<<" major_radius_mm="<<plan.source.majorRadius/plan.source.unitsPerMm;Log(diagnostic.str());
        auto mark=session->SetUndoMark(NXOpen::Session::MarkVisibilityVisible,"球面分瓣展开");UF_DISP_refresh();QuietDisplay quiet;
        try{sphere_unfold::CreateFeature(plan,edited_,mark);}
        catch(...){try{session->UndoToMark(mark,nullptr);session->DeleteUndoMark(mark,nullptr);}catch(const NXOpen::NXException& e){Log("回滚失败："+std::string(e.Message()));}throw;}
        quiet.Restore();
        if(!edited_){Select(cylinder_)->SetSelectedObjects({});Select(sphere_)->SetSelectedObjects({});}
        else{editCylinder_=c[0]->Tag();editSphere_=f[0]->Tag();}
        for(int i=0;i<3;++i)zhihui_dialog_memory::SaveDouble(memory,keys[i],numbers_[i]);
        Status("已生成球面展开自定义特征。\n"+std::to_string(settings.petals)+" 瓣；NX 原生展开验证通过。");cylinder_->Focus();return 0;
    }catch(const NXOpen::NXException& e){Error(e.Message());}catch(const std::exception& e){Error(e.what());}catch(...){Error("创建球面展开失败。");}
    UF_DISP_refresh();return 1;
}
int QiuMianZanKaiDialog::Cancel(){try{UF_DISP_refresh();return 0;}catch(const NXOpen::NXException& e){Log(e.Message());}catch(...){Log("Cancel failed");}return 0;}
