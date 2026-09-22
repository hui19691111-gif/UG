#include "ZeWanFuZu.hpp"
#include "ZeWanFuZuFeature.hpp"
#include <NXOpen/Features_CustomFeature.hxx>
#include <NXOpen/Features_CustomFeatureData.hxx>
#include <NXOpen/Features_CustomFeatureClassManager.hxx>
#include "../../common/ZhihuiDialogMemory.hpp"
#ifdef CreateDialog
#undef CreateDialog
#endif
#include <NXOpen/BlockStyler_PropertyList.hxx>
#include <NXOpen/BlockStyler_SelectObject.hxx>
#include <NXOpen/Callback.hxx>
#include <NXOpen/Edge.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXMessageBox.hxx>
#include <NXOpen/Selection.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/UI.hxx>
#include <uf.h>
#include <uf_disp.h>
#include <uf_obj.h>
#include <uf_ui_types.h>
#include <Windows.h>
#include <array>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>

namespace {
const char* numberIds[]={"plate_width","bridge_width","bridge_inset","middle_gap","end_distance","plate_height"};
const wchar_t* numberKeys[]={L"Width",L"BridgeWidth",L"Inset",L"Gap",L"EndDistance",L"Height"};
constexpr const wchar_t* memoryFile=L"ZeWanFuZu.ini";
std::filesystem::path ModuleDir() {
    HMODULE module=nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&ModuleDir),&module);
    wchar_t path[MAX_PATH]={};
    if(!GetModuleFileNameW(module,path,MAX_PATH)) throw std::runtime_error("无法找到折弯辅助板资源目录。");
    return std::filesystem::path(path).parent_path();
}
struct Guard { bool& flag; Guard(bool& f):flag(f){flag=true;} ~Guard(){flag=false;} };
using Properties=std::unique_ptr<NXOpen::BlockStyler::PropertyList>;
double Double(NXOpen::BlockStyler::UIBlock* b) { return Properties(b->GetProperties())->GetDouble("Value"); }
bool Toggle(NXOpen::BlockStyler::UIBlock* b) { return Properties(b->GetProperties())->GetLogical("Value"); }
std::vector<NXOpen::TaggedObject*> Selected(NXOpen::BlockStyler::UIBlock* b) {
    return dynamic_cast<NXOpen::BlockStyler::SelectObject*>(b)->GetSelectedObjects();
}
void Log(const std::string& message) noexcept {
    try {
        wchar_t temp[MAX_PATH]={}; GetTempPathW(MAX_PATH,temp);
        std::ofstream(std::filesystem::path(temp)/L"Zhihui-ZeWanFuZu.log",std::ios::app)<<message<<'\n';
    } catch(...) {}
}
}
ZeWanFuZuDialog::ZeWanFuZuDialog() {
    bend_assist::RequireFeatureClass();
    edited_=NXOpen::Session::GetSession()->CustomFeatureClassManager()->GetEditedCustomFeature();
    if(edited_ && std::string(edited_->FeatureData()->ClassName().GetText())!=bend_assist::featureClass)
        edited_=nullptr;
    auto path=(ModuleDir()/L"ZeWanFuZu.dlx").u8string();
    dialog_=NXOpen::UI::GetUI()->CreateDialog(path.c_str());
    dialog_->AddInitializeHandler(NXOpen::make_callback(this,&ZeWanFuZuDialog::Initialize));
    dialog_->AddDialogShownHandler(NXOpen::make_callback(this,&ZeWanFuZuDialog::Shown));
    dialog_->AddUpdateHandler(NXOpen::make_callback(this,&ZeWanFuZuDialog::Update));
    dialog_->AddFilterHandler(NXOpen::make_callback(this,&ZeWanFuZuDialog::Filter));
    dialog_->AddApplyHandler(NXOpen::make_callback(this,&ZeWanFuZuDialog::Apply));
    dialog_->AddOkHandler(NXOpen::make_callback(this,&ZeWanFuZuDialog::Apply));
    dialog_->AddCancelHandler(NXOpen::make_callback(this,&ZeWanFuZuDialog::Cancel));
}
ZeWanFuZuDialog::~ZeWanFuZuDialog() { UF_DISP_refresh(); delete dialog_; }
void ZeWanFuZuDialog::Launch() {
    dialog_->LaunchInDialogMode(edited_?NXOpen::BlockStyler::BlockDialog::DialogModeEdit:NXOpen::BlockStyler::BlockDialog::DialogModeCreate);
}
void ZeWanFuZuDialog::Initialize() {
    try {
        faces_=dialog_->TopBlock()->FindBlock("thickness_faces");
        reference_=dialog_->TopBlock()->FindBlock("bend_reference");
        align_=dialog_->TopBlock()->FindBlock("auto_align");
        reverse_=dialog_->TopBlock()->FindBlock("reverse_end");
        status_=dialog_->TopBlock()->FindBlock("result_status");
        if(!faces_ || !reference_ || !align_ || !reverse_ || !status_) throw std::runtime_error("对话框资源不完整。");
        for(int i=0;i<6;++i) {
            numbers_[i]=dialog_->TopBlock()->FindBlock(numberIds[i]);
            if(!numbers_[i]) throw std::runtime_error("对话框缺少辅助板参数。");
            zhihui_dialog_memory::LoadDouble(memoryFile,numberKeys[i],numbers_[i]);
        }
        zhihui_dialog_memory::LoadLogical(memoryFile,L"AutoAlign",align_);
        zhihui_dialog_memory::LoadLogical(memoryFile,L"ReverseEnd",reverse_);
        if(edited_) {
            const auto s=bend_assist::FeatureSettings(edited_);
            const double values[]={s.width,s.bridgeWidth,s.inset,s.gap,s.endDistance,s.height};
            for(int i=0;i<6;++i) Properties(numbers_[i]->GetProperties())->SetDouble("Value",values[i]);
            Properties(align_->GetProperties())->SetLogical("Value",s.autoAlign);
            Properties(reverse_->GetProperties())->SetLogical("Value",s.reverseEnd);
        }
        auto* select=dynamic_cast<NXOpen::BlockStyler::SelectObject*>(faces_);
        auto* reference=dynamic_cast<NXOpen::BlockStyler::SelectObject*>(reference_);
        if(!select || !reference) throw std::runtime_error("选择控件类型错误。");
        select->SetSelectionFilter(NXOpen::Selection::SelectionActionClearAndEnableSpecific,
            {{UF_solid_type,0,UF_UI_SEL_FEATURE_ANY_FACE}});
        select->SetSelectModeAsString("Multiple"); select->SetAutomaticProgression(false);
        reference->SetSelectionFilter(NXOpen::Selection::SelectionActionClearAndEnableSpecific,
            {{UF_solid_type,0,UF_UI_SEL_FEATURE_ANY_EDGE}});
        reference->SetSelectModeAsString("Single"); reference->SetAutomaticProgression(false);
        initialized_=true;
    } catch(const NXOpen::NXException& e){Error(e.Message());}
      catch(const std::exception& e){Error(e.what());}
      catch(...){Error("初始化折弯辅助板时发生未知错误。");}
}
void ZeWanFuZuDialog::Shown() {
    try {
        if(!initialized_) return;
        shown_=true; Guard guard(updating_);
        Properties(numbers_[5]->GetProperties())->SetLogical("Enable",!Toggle(align_));
        if(edited_) {
            Properties(faces_->GetProperties())->SetLogical("Show",false);
            Properties(reference_->GetProperties())->SetLogical("Show",false);
            numbers_[0]->Focus(); Preview();
        } else faces_->Focus();
    } catch(const NXOpen::NXException& e){Error(e.Message());}
      catch(const std::exception& e){Error(e.what());}
      catch(...){Error("显示折弯辅助板时发生未知错误。");}
}
void ZeWanFuZuDialog::Status(const std::string& message) { Properties(status_->GetProperties())->SetString("Label",message.c_str()); }
void ZeWanFuZuDialog::Error(const std::string& message) noexcept {
    Log(message);
    try { NXOpen::UI::GetUI()->NXMessageBox()->Show("折弯辅助板",NXOpen::NXMessageBox::DialogTypeError,message.c_str()); }
    catch(...) {}
}
int ZeWanFuZuDialog::Filter(NXOpen::BlockStyler::UIBlock* block,NXOpen::TaggedObject* object) {
    try {
        if(block==faces_) {
            auto* face=dynamic_cast<NXOpen::Face*>(object);
            return face && !face->IsOccurrence() && (face->SolidFaceType()==NXOpen::Face::FaceTypePlanar ||
                face->SolidFaceType()==NXOpen::Face::FaceTypeCylindrical) ? UF_UI_SEL_ACCEPT : UF_UI_SEL_REJECT;
        }
        auto* edge=dynamic_cast<NXOpen::Edge*>(object);
        return edge && !edge->IsOccurrence() && edge->SolidEdgeType()==NXOpen::Edge::EdgeTypeLinear ? UF_UI_SEL_ACCEPT : UF_UI_SEL_REJECT;
    } catch(const NXOpen::NXException& e){Log(e.Message());}
      catch(const std::exception& e){Log(e.what());}
      catch(...){Log("selection filter failed");}
    return UF_UI_SEL_REJECT;
}
bend_assist::Settings ZeWanFuZuDialog::ReadSettings() const {
    bend_assist::Settings s;
    s.width=Double(numbers_[0]); s.bridgeWidth=Double(numbers_[1]); s.inset=Double(numbers_[2]);
    s.gap=Double(numbers_[3]); s.endDistance=Double(numbers_[4]); s.height=Double(numbers_[5]);
    s.autoAlign=Toggle(align_); s.reverseEnd=Toggle(reverse_); return s;
}
std::vector<bend_assist::FaceInfo> ZeWanFuZuDialog::ReadFaces() const {
    if(edited_) return {bend_assist::FeatureFace(edited_)};
    auto references=Selected(reference_); tag_t edge=references.empty()?0:references.front()->Tag();
    std::vector<bend_assist::FaceInfo> faces;
    for(auto* object:Selected(faces_)) faces.push_back(bend_assist::Inspect(object->Tag(),edge));
    return faces;
}
void ZeWanFuZuDialog::Preview() {
    UF_DISP_refresh();
    if(!edited_ && Selected(faces_).empty()) { Status("选择板厚面；平行于折弯边的面自动跳过。"); return; }
    auto faces=ReadFaces(); auto plans=bend_assist::PlanFaces(faces,ReadSettings());
    UF_OBJ_disp_props_t props={}; props.color=186; props.font=1; props.line_width=2;
    auto line=[&](bend_assist::Vec a,bend_assist::Vec b) {
        double p[]={a.x,a.y,a.z},q[]={b.x,b.y,b.z};
        bend_assist::Check(UF_DISP_display_temporary_line(NULL_TAG,UF_DISP_USE_WORK_VIEW,p,q,&props));
    };
    for(const auto& p:plans) {
        auto dz=p.face.z*p.face.thickness;
        for(size_t i=0;i<p.outline.size();++i) {
            auto a=p.outline[i], b=p.outline[(i+1)%p.outline.size()];
            line(a,b); line(a+dz,b+dz);
        }
        for(const auto& segment:p.segments) line(segment.start,segment.start+dz);
    }
    if(edited_) Status("编辑当前辅助板；确定后更新尺寸。");
    else Status("待创建 "+std::to_string(plans.size())+" 块；平行跳过 "+std::to_string(faces.size()-plans.size())+" 面。");
}
int ZeWanFuZuDialog::Update(NXOpen::BlockStyler::UIBlock* block) {
    if(!initialized_ || !shown_ || updating_) return 0;
    Guard guard(updating_);
    try {
        if(!edited_ && block==faces_) {
            std::vector<tag_t> current;
            bool added=false;
            for(auto* object:Selected(faces_)) {
                current.push_back(object->Tag());
                if(std::find(selectedFaceTags_.begin(),selectedFaceTags_.end(),object->Tag())==selectedFaceTags_.end()) added=true;
            }
            selectedFaceTags_=std::move(current);
            // Advance only after an actual addition. Merely returning to the
            // face selector must leave it active so more faces can be added.
            // Keep focus changes inside the callback re-entry guard, before
            // preview: ambiguous bend directions still need this next input.
            if(added) reference_->Focus();
        }
        Properties(numbers_[5]->GetProperties())->SetLogical("Enable",!Toggle(align_));
        Preview(); return 0;
    } catch(const NXOpen::NXException& e){Log(e.Message());try{Status(e.Message());}catch(...){} }
      catch(const std::exception& e){Log(e.what());try{Status(e.what());}catch(...){} }
      catch(...){Log("preview failed");}
    return 0;
}
int ZeWanFuZuDialog::Apply() {
    if(!initialized_ || !shown_ || updating_) return 1;
    Guard guard(updating_);
    auto* session=NXOpen::Session::GetSession();
    NXOpen::Session::UndoMarkId mark=static_cast<NXOpen::Session::UndoMarkId>(0); bool hasMark=false;
    try {
        auto faces=ReadFaces();
        if(faces.empty()) throw std::runtime_error("请先选择要创建辅助板的板厚面。");
        auto plans=bend_assist::PlanFaces(faces,ReadSettings());
        UF_DISP_refresh();
        if(!plans.empty()) {
            mark=session->SetUndoMark(NXOpen::Session::MarkVisibilityVisible,"折弯辅助板"); hasMark=true;
            if(edited_) bend_assist::EditFeature(edited_,ReadSettings());
            else for(const auto& p:plans) bend_assist::CreateFeature(p,ReadSettings());
        }
        // Clear selection while guarded: consumed face references must never be
        // reused after Apply, and programmatic changes can call Update again.
        dynamic_cast<NXOpen::BlockStyler::SelectObject*>(faces_)->SetSelectedObjects({});
        selectedFaceTags_.clear();
        dynamic_cast<NXOpen::BlockStyler::SelectObject*>(reference_)->SetSelectedObjects({});
        for(int i=0;i<6;++i) zhihui_dialog_memory::SaveDouble(memoryFile,numberKeys[i],numbers_[i]);
        zhihui_dialog_memory::SaveLogical(memoryFile,L"AutoAlign",align_);
        zhihui_dialog_memory::SaveLogical(memoryFile,L"ReverseEnd",reverse_);
        if(edited_) { Status("辅助板参数已更新。"); numbers_[0]->Focus(); }
        else {
            Status("已创建 "+std::to_string(plans.size())+" 块；折弯边平行，跳过 "+std::to_string(faces.size()-plans.size())+" 面。");
            faces_->Focus();
        }
        return 0;
    } catch(const NXOpen::NXException& e){Error(e.Message());}
      catch(const std::exception& e){Error(e.what());}
      catch(...){Error("生成辅助板时发生未知错误。");}
    if(hasMark) {
        try{session->UndoToMark(mark,nullptr);session->DeleteUndoMark(mark,nullptr);}
        catch(const NXOpen::NXException& e){Error("回滚失败："+std::string(e.Message()));}
        catch(...){Error("回滚失败，请立即检查零件撤销记录。");}
    }
    UF_DISP_refresh(); return 1;
}
int ZeWanFuZuDialog::Cancel() { UF_DISP_refresh(); return 0; }
