#include "TiaoZenMenJianXi.hpp"
#include "DoorGapOverlay.hpp"
#include "DoorGapGeometry.hpp"
#include "../40_TiaoZenBanLeiCiCun/PanelSkirtGeometry.hpp"

#ifdef CreateDialog
#undef CreateDialog
#endif
#include <NXOpen/Assemblies_Component.hxx>
#include <NXOpen/Assemblies_ComponentAssembly.hxx>
#include <NXOpen/BasePart.hxx>
#include <NXOpen/BlockStyler_LinearDimension.hxx>
#include <NXOpen/BlockStyler_PropertyList.hxx>
#include <NXOpen/BlockStyler_UIBlock.hxx>
#include <NXOpen/Body.hxx>
#include <NXOpen/BodyCollection.hxx>
#include <NXOpen/Callback.hxx>
#include <NXOpen/Direction.hxx>
#include <NXOpen/DirectionCollection.hxx>
#include <NXOpen/Edge.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/FaceDumbRule.hxx>
#include <NXOpen/Features_FeatureCollection.hxx>
#include <NXOpen/Features_MoveFaceBuilder.hxx>
#include <NXOpen/LogFile.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXMessageBox.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/PartLoadStatus.hxx>
#include <NXOpen/ScRuleFactory.hxx>
#include <NXOpen/Selection.hxx>
#include <NXOpen/SelectionIntentRuleOptions.hxx>
#include <NXOpen/SmartObject.hxx>
#include <NXOpen/UI.hxx>
#include <uf_assem.h>
#include <uf_disp.h>
#include <uf_modl.h>
#include <uf_obj.h>
#include <uf_object_types.h>
#include <uf_ui_types.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#ifdef CreateDialog
#undef CreateDialog
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <vector>

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace {
constexpr double kTolerance = 1.0e-3;
constexpr double kParallel = 0.995;
constexpr double kMaxGap = 100.0;

double Dot(const NXOpen::Vector3d& a, const NXOpen::Vector3d& b) {
    return a.X*b.X + a.Y*b.Y + a.Z*b.Z;
}
NXOpen::Vector3d Between(const NXOpen::Point3d& a, const NXOpen::Point3d& b) {
    return {a.X-b.X, a.Y-b.Y, a.Z-b.Z};
}
NXOpen::Vector3d Scale(const NXOpen::Vector3d& a, double s) {
    return {a.X*s, a.Y*s, a.Z*s};
}
NXOpen::Vector3d Cross(const NXOpen::Vector3d& a, const NXOpen::Vector3d& b) {
    return {a.Y*b.Z-a.Z*b.Y, a.Z*b.X-a.X*b.Z, a.X*b.Y-a.Y*b.X};
}
double Magnitude(const NXOpen::Vector3d& a) { return std::sqrt(Dot(a,a)); }
bool Normalize(NXOpen::Vector3d& a) {
    const double length = Magnitude(a);
    if (length < 1.0e-8) return false;
    a = Scale(a, 1.0/length);
    return true;
}
NXOpen::Point3d Advance(const NXOpen::Point3d& p, const NXOpen::Vector3d& v, double d) {
    return {p.X+v.X*d,p.Y+v.Y*d,p.Z+v.Z*d};
}
double Coordinate(const NXOpen::Point3d& p, const NXOpen::Point3d& origin,
                  const NXOpen::Vector3d& axis) {
    return Dot(Between(p,origin),axis);
}
std::string Number(double value) {
    std::ostringstream stream;
    stream << std::setprecision(15) << value;
    return stream.str();
}
std::string Fixed(double value) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2) << value;
    return stream.str();
}
bool PlaneData(NXOpen::Face* face, NXOpen::Point3d& point, NXOpen::Vector3d& normal) {
    return door_gap_geometry::Plane(face,point,normal);
}
bool IsAlive(tag_t tag) {
    if (tag==NULL_TAG) return false;
    int type=0, subtype=0;
    return UF_OBJ_ask_type_and_subtype(tag,&type,&subtype)==0;
}
std::string DlxPath() {
    wchar_t module[MAX_PATH]{};
    const DWORD len=GetModuleFileNameW(reinterpret_cast<HMODULE>(&__ImageBase),module,MAX_PATH);
    if (len>0 && len<MAX_PATH)
        return (std::filesystem::path(module).parent_path()/L"TiaoZenMenJianXi.dlx").string();
    return "TiaoZenMenJianXi.dlx";
}
void Identity(double matrix[4][4]) {
    for (int i=0;i<4;++i) for (int j=0;j<4;++j) matrix[i][j]=(i==j?1.0:0.0);
}
NXOpen::Point3d ToLocal(const NXOpen::Point3d& p, const double m[4][4]) {
    const double x[3]{p.X-m[0][3],p.Y-m[1][3],p.Z-m[2][3]};
    return {m[0][0]*x[0]+m[1][0]*x[1]+m[2][0]*x[2],
            m[0][1]*x[0]+m[1][1]*x[1]+m[2][1]*x[2],
            m[0][2]*x[0]+m[1][2]*x[1]+m[2][2]*x[2]};
}
NXOpen::Vector3d ToLocal(const NXOpen::Vector3d& v, const double m[4][4]) {
    return {m[0][0]*v.X+m[1][0]*v.Y+m[2][0]*v.Z,
            m[0][1]*v.X+m[1][1]*v.Y+m[2][1]*v.Z,
            m[0][2]*v.X+m[1][2]*v.Y+m[2][2]*v.Z};
}
NXOpen::Vector3d ToDisplay(const NXOpen::Vector3d& v, const double m[4][4]) {
    return {m[0][0]*v.X+m[0][1]*v.Y+m[0][2]*v.Z,
            m[1][0]*v.X+m[1][1]*v.Y+m[1][2]*v.Z,
            m[2][0]*v.X+m[2][1]*v.Y+m[2][2]*v.Z};
}
void AddBodies(NXOpen::Assemblies::Component* component,
               std::vector<NXOpen::Body*>& bodies, std::set<tag_t>& seen) {
    if (!component) return;
    auto* part=dynamic_cast<NXOpen::Part*>(component->Prototype());
    if (part) for (auto it=part->Bodies()->begin();it!=part->Bodies()->end();++it) {
        auto* occ=dynamic_cast<NXOpen::Body*>(component->FindOccurrence(*it));
        if (occ && seen.insert(occ->Tag()).second) bodies.push_back(occ);
    }
    for (auto* child: component->GetChildren()) AddBodies(child,bodies,seen);
}
std::vector<NXOpen::Body*> VisibleBodies(NXOpen::Session* session) {
    std::vector<NXOpen::Body*> bodies;
    std::set<tag_t> seen;
    auto* displayed=session->Parts()->Display();
    if (!displayed) return bodies;
    for (auto it=displayed->Bodies()->begin();it!=displayed->Bodies()->end();++it)
        if (*it && seen.insert((*it)->Tag()).second) bodies.push_back(*it);
    auto* assembly=displayed->ComponentAssembly();
    auto* root=assembly?assembly->RootComponent():nullptr;
    if (root) for (auto* child: root->GetChildren()) AddBodies(child,bodies,seen);
    return bodies;
}
class WorkComponentScope final {
public:
    WorkComponentScope(NXOpen::Session* session, NXOpen::Part* target,
                       NXOpen::Assemblies::Component* component)
        : parts_(session->Parts()), previousPart_(parts_->BaseWork()),
          previousComponent_(parts_->WorkComponent()) {
        if (!target) throw std::runtime_error("无法取得门板组件的零件。");
        if (parts_->Work()==target) return;
        try {
            changed_=true;
            if (component) {
                NXOpen::PartLoadStatus* status=nullptr;
                try {
                    parts_->SetWorkComponent(component,
                        NXOpen::PartCollection::RefsetOptionCurrent,
                        NXOpen::PartCollection::WorkComponentOptionGiven,&status);
                } catch (...) { delete status; throw; }
                delete status;
            } else parts_->SetWork(target);
            if (parts_->Work()!=target)
                throw std::runtime_error("无法将门板组件临时设为工作部件。");
        } catch (...) {
            try { Restore(); } catch (...) {}
            throw;
        }
    }
    ~WorkComponentScope() { try { Restore(); } catch (...) {} }
    WorkComponentScope(const WorkComponentScope&)=delete;
    WorkComponentScope& operator=(const WorkComponentScope&)=delete;
    void Restore() {
        if (!changed_) return;
        NXOpen::PartLoadStatus* status=nullptr;
        try {
            if (previousComponent_) {
                parts_->SetWorkComponent(previousComponent_,
                    NXOpen::PartCollection::RefsetOptionCurrent,
                    NXOpen::PartCollection::WorkComponentOptionGiven,&status);
            } else if (previousPart_==parts_->BaseDisplay()) {
                parts_->SetWorkComponent(nullptr,&status);
            } else if (previousPart_) parts_->SetWork(previousPart_);
        } catch (...) { delete status; throw; }
        delete status;
        if (previousPart_ && parts_->BaseWork()!=previousPart_)
            throw std::runtime_error("门板已修改，但无法恢复原来的工作部件。");
        changed_=false;
    }
private:
    NXOpen::PartCollection* parts_;
    NXOpen::BasePart* previousPart_;
    NXOpen::Assemblies::Component* previousComponent_;
    bool changed_=false;
};
class CallbackPause final {
public:
    explicit CallbackPause(bool& flag): flag_(flag), previous_(flag) { flag_=true; }
    ~CallbackPause() { Restore(); }
    void Restore() { flag_=previous_; }
private:
    bool& flag_;
    bool previous_;
};
}

DoorGapDialog::DoorGapDialog() {
    session_=NXOpen::Session::GetSession();
    ui_=NXOpen::UI::GetUI();
    dialog_=ui_->CreateDialog(DlxPath().c_str());
    dialog_->AddInitializeHandler(NXOpen::make_callback(this,&DoorGapDialog::Initialize));
    dialog_->AddDialogShownHandler(NXOpen::make_callback(this,&DoorGapDialog::DialogShown));
    dialog_->AddUpdateHandler(NXOpen::make_callback(this,&DoorGapDialog::Update));
    dialog_->AddApplyHandler(NXOpen::make_callback(this,&DoorGapDialog::Apply));
    dialog_->AddOkHandler(NXOpen::make_callback(this,&DoorGapDialog::Ok));
    dialog_->AddFilterHandler(NXOpen::make_callback(this,&DoorGapDialog::Filter));
    Identity(transform_);
}
DoorGapDialog::~DoorGapDialog() { delete overlay_; delete dialog_; }
NXOpen::BlockStyler::BlockDialog::DialogResponse DoorGapDialog::Launch() {
    return dialog_->Launch();
}
void DoorGapDialog::Initialize() {
    try {
        auto* top=dialog_->TopBlock();
        selection_=top->FindBlock("door_face");
        status_=top->FindBlock("status");
        const char* inputIds[4]{"left_gap","right_gap","bottom_gap","top_gap"};
        const char* labelIds[4]{"left_status","right_status","bottom_status","top_status"};
        const char* handleIds[4]{"left_handle","right_handle","bottom_handle","top_handle"};
        for (int i=0;i<4;++i) {
            inputs_[i]=top->FindBlock(inputIds[i]);
            labels_[i]=top->FindBlock(labelIds[i]);
            handles_[i]=dynamic_cast<NXOpen::BlockStyler::LinearDimension*>(top->FindBlock(handleIds[i]));
            if (!inputs_[i] || !labels_[i] || !handles_[i])
                throw std::runtime_error("门间隙对话框缺少控件。");
        }
        if (!selection_ || !status_) throw std::runtime_error("门间隙对话框缺少选面控件。");
        auto* props=selection_->GetProperties();
        std::vector<NXOpen::Selection::MaskTriple> masks;
        masks.emplace_back(UF_solid_type,UF_solid_face_subtype,UF_UI_SEL_FEATURE_PLANAR_FACE);
        props->SetSelectionFilter("SelectionFilter",
            NXOpen::Selection::SelectionActionClearAndEnableSpecific,masks);
        props->SetEnum("StepStatus",0);
        delete props;
    } catch (const NXOpen::NXException& ex) { Error(ex.Message()); }
      catch (const std::exception& ex) { Error(ex.what()); }
      catch (...) { Error("初始化门间隙对话框失败。"); }
}
void DoorGapDialog::DialogShown() {
    try {
        if (!overlay_) overlay_=new DoorGapOverlay(session_);
        HideHandles();
        if (selection_) selection_->Focus();
    } catch (const NXOpen::NXException& ex) { Error(ex.Message()); }
      catch (const std::exception& ex) { Error(ex.what()); }
      catch (...) { Error("显示门间隙对话框失败。"); }
}
int DoorGapDialog::Filter(NXOpen::BlockStyler::UIBlock* block, NXOpen::TaggedObject* object) {
    try {
        if (block!=selection_) return UF_UI_SEL_ACCEPT;
        auto* face=dynamic_cast<NXOpen::Face*>(object);
        return face && face->SolidFaceType()==NXOpen::Face::FaceTypePlanar
            ? UF_UI_SEL_ACCEPT:UF_UI_SEL_REJECT;
    } catch (const NXOpen::NXException& ex) {
        Log(ex.Message()?ex.Message():"选面筛选发生 NX 异常。");
        return UF_UI_SEL_REJECT;
    } catch (const std::exception& ex) {
        Log(ex.what()); return UF_UI_SEL_REJECT;
    } catch (...) {
        Log("选面筛选发生未知异常。"); return UF_UI_SEL_REJECT;
    }
}
NXOpen::Face* DoorGapDialog::SelectedFace() const {
    if (!selection_) return nullptr;
    auto* props=selection_->GetProperties();
    const auto objects=props->GetTaggedObjectVector("SelectedObjects");
    delete props;
    return objects.size()==1 ? dynamic_cast<NXOpen::Face*>(objects.front()):nullptr;
}
double DoorGapDialog::Value(NXOpen::BlockStyler::UIBlock* block) const {
    auto* props=block->GetProperties();
    const double value=props->GetDouble("Value");
    delete props;
    return value;
}
void DoorGapDialog::SetValue(NXOpen::BlockStyler::UIBlock* block, double value) {
    auto* props=block->GetProperties();
    props->SetDouble("Value",value);
    delete props;
}
void DoorGapDialog::SetLabel(NXOpen::BlockStyler::UIBlock* block,
                             const std::string& value) {
    auto* props=block->GetProperties();
    props->SetString("Label",value.c_str());
    delete props;
}
bool DoorGapDialog::BodyExtents(NXOpen::Body* body, std::array<double,4>& bounds) const {
    if (!body) return false;
    bounds={std::numeric_limits<double>::max(),
            -std::numeric_limits<double>::max(),
            std::numeric_limits<double>::max(),
            -std::numeric_limits<double>::max()};
    bool found=false;
    for (auto* edge: body->GetEdges()) {
        if (!edge) continue;
        NXOpen::Point3d a,b;
        edge->GetVertices(&a,&b);
        for (const auto& p: {a,b}) {
            const double u=Coordinate(p,origin_,u_);
            const double v=Coordinate(p,origin_,v_);
            bounds[0]=(std::min)(bounds[0],u);
            bounds[1]=(std::max)(bounds[1],u);
            bounds[2]=(std::min)(bounds[2],v);
            bounds[3]=(std::max)(bounds[3],v);
            found=true;
        }
    }
    return found && bounds[1]-bounds[0]>kTolerance && bounds[3]-bounds[2]>kTolerance;
}
bool DoorGapDialog::MeasureSide(Side& side, const std::array<double,4>& bounds,
                                double thickness, std::string& error) {
    side.measured=false;
    side.gap=0;
    side.referenceFace=NULL_TAG;
    side.referenceBody=NULL_TAG;
    const bool horizontal=(std::abs(Dot(side.outward,u_))>0.9);
    const auto tangent=horizontal?v_:u_;
    const double panelMin=horizontal?bounds[2]:bounds[0];
    const double panelMax=horizontal?bounds[3]:bounds[1];
    const auto reference=door_gap_geometry::FindNearest(
        VisibleBodies(session_),selectedBody_,origin_,normal_,side.outward,
        tangent,side.boundary,panelMin,panelMax,thickness,kMaxGap);
    if (reference.face) {
        side.gap=reference.gap;
        side.referenceFace=reference.face->Tag();
        side.referenceBody=reference.body->Tag();
        side.measured=true;
    }
    if (!side.measured)
        error+=std::string(error.empty()?"":"、")+side.name;
    return side.measured;
}
bool DoorGapDialog::Analyze(NXOpen::Face* face, std::string& error) {
    valid_=false;
    selectedFace_=prototypeFace_=selectedBody_=prototypeBody_=selectedComponent_=NULL_TAG;
    if (!face || !PlaneData(face,origin_,normal_)) {
        error="请选择门板的平面。"; return false;
    }
    auto* work=session_->Parts()->Work();
    auto* display=session_->Parts()->Display();
    if (!work || !display) { error="请先打开门板零件。"; return false; }
    if (display!=work && !face->IsOccurrence()) {
        error="装配中请选择门板组件的显示面。"; return false;
    }
    Identity(transform_);
    tag_t prototypeTag=face->Tag();
    if (face->IsOccurrence()) {
        prototypeTag=UF_ASSEM_ask_prototype_of_occ(face->Tag());
        if (prototypeTag==NULL_TAG ||
            UF_ASSEM_ask_transform_of_occ(face->Tag(),transform_)!=0) {
            error="无法获取门板组件的原型或装配位置。"; return false;
        }
    }
    auto* prototype=dynamic_cast<NXOpen::Face*>(NXOpen::NXObjectManager::Get(prototypeTag));
    auto* target=prototype?dynamic_cast<NXOpen::Part*>(prototype->OwningPart()):nullptr;
    if (!target || !prototype->GetBody()->IsSolidBody() ||
        (!face->IsOccurrence() && target!=work) ||
        (face->IsOccurrence() && (!face->OwningComponent() ||
            face->OwningComponent()->Prototype()!=target))) {
        error="无法确定所选门板组件的可编辑零件。"; return false;
    }
    double longest=0;
    bool horizontalEdgeFound=false;
    for (auto* edge: face->GetEdges()) {
        if (!edge || edge->SolidEdgeType()!=NXOpen::Edge::EdgeTypeLinear) continue;
        NXOpen::Point3d a,b; edge->GetVertices(&a,&b);
        auto dir=Between(b,a);
        const double length=Magnitude(dir);
        if (!Normalize(dir)) continue;
        const bool horizontal=std::abs(dir.Z)<0.25;
        if ((horizontal && !horizontalEdgeFound) ||
            (horizontal==horizontalEdgeFound && length>longest)) {
            u_=dir; longest=length; horizontalEdgeFound=horizontal;
        }
    }
    if (longest<kTolerance) { error="门板面缺少可定位方向的直线边。"; return false; }
    const double components[3]{std::abs(u_.X),std::abs(u_.Y),std::abs(u_.Z)};
    const int dominant=components[1]>components[0]?(components[2]>components[1]?2:1)
        :(components[2]>components[0]?2:0);
    const double signedValue=dominant==0?u_.X:(dominant==1?u_.Y:u_.Z);
    if (signedValue<0) u_=Scale(u_,-1);
    v_=Cross(normal_,u_);
    if (!Normalize(v_)) { error="无法建立门板面坐标系。"; return false; }
    if (std::abs(v_.Z)>0.25 && v_.Z<0) v_=Scale(v_,-1);
    selectedFace_=face->Tag();
    prototypeFace_=prototypeTag;
    selectedBody_=face->GetBody()->Tag();
    prototypeBody_=prototype->GetBody()->Tag();
    if (face->IsOccurrence() && face->OwningComponent())
        selectedComponent_=face->OwningComponent()->Tag();
    std::array<double,4> bounds;
    if (!BodyExtents(face->GetBody(),bounds)) { error="无法计算门板四边范围。"; return false; }
    thickness_=std::numeric_limits<double>::max();
    for (auto* candidate: face->GetBody()->GetFaces()) {
        if (!candidate || candidate->Tag()==face->Tag()) continue;
        NXOpen::Point3d p; NXOpen::Vector3d n;
        if (!PlaneData(candidate,p,n) || std::abs(Dot(n,normal_))<kParallel) continue;
        const double distance=std::abs(Coordinate(p,origin_,normal_));
        if (distance>kTolerance) thickness_=(std::min)(thickness_,distance);
    }
    if (!std::isfinite(thickness_) || thickness_==std::numeric_limits<double>::max()) {
        error="无法识别门板板厚。"; return false;
    }
    sides_[0]={"左",Scale(u_,-1),-bounds[0]};
    sides_[1]={"右",u_,bounds[1]};
    sides_[2]={"下",Scale(v_,-1),-bounds[2]};
    sides_[3]={"上",v_,bounds[3]};
    const double midU=(bounds[0]+bounds[1])*0.5;
    const double midV=(bounds[2]+bounds[3])*0.5;
    sides_[0].handleOrigin=Advance(Advance(origin_,u_,bounds[0]),v_,midV);
    sides_[1].handleOrigin=Advance(Advance(origin_,u_,bounds[1]),v_,midV);
    sides_[2].handleOrigin=Advance(Advance(origin_,u_,midU),v_,bounds[2]);
    sides_[3].handleOrigin=Advance(Advance(origin_,u_,midU),v_,bounds[3]);
    // Use the real straight edge midpoints when corners are trimmed.  The
    // bounding rectangle midpoint remains a fallback for split edges.
    for (int i=0;i<4;++i) {
        double longestSide=0;
        const auto tangent=(i<2)?v_:u_;
        const double boundary=(i==0?bounds[0]:i==1?bounds[1]:i==2?bounds[2]:bounds[3]);
        const auto axis=(i<2)?u_:v_;
        for (auto* edge:face->GetEdges()) {
            if (!edge || edge->SolidEdgeType()!=NXOpen::Edge::EdgeTypeLinear) continue;
            NXOpen::Point3d a,b; edge->GetVertices(&a,&b);
            const auto direction=Between(b,a);
            const double length=Magnitude(direction);
            if (length<kTolerance || std::abs(Dot(direction,tangent))/length<0.99) continue;
            const double da=Coordinate(a,origin_,axis);
            const double db=Coordinate(b,origin_,axis);
            if (std::abs(da-boundary)>0.05 || std::abs(db-boundary)>0.05) continue;
            if (length>longestSide) {
                longestSide=length;
                sides_[i].handleOrigin={(a.X+b.X)*0.5,(a.Y+b.Y)*0.5,(a.Z+b.Z)*0.5};
            }
        }
    }
    std::string missing;
    for (auto& side:sides_) MeasureSide(side,bounds,thickness_,missing);
    valid_=true;
    if (!missing.empty()) error="未找到邻近基准："+missing+"。对应侧仅显示，无法应用修改。";
    return true;
}

void DoorGapDialog::ShowValues() {
    changingUi_=true;
    try {
        for (int i=0;i<4;++i) {
            SetValue(inputs_[i],sides_[i].measured?sides_[i].gap:0.0);
            SetLabel(labels_[i],std::string(sides_[i].name)+"："+
                (sides_[i].measured?Fixed(sides_[i].gap)+" mm":"未找到邻件"));
            inputs_[i]->SetEnable(sides_[i].measured);
        }
        int found=0;
        for (const auto& side:sides_) if (side.measured) ++found;
        SetLabel(status_,"已测得 "+std::to_string(found)+"/4 边；输入目标间隙后单击应用。单位：mm");
    } catch (...) { changingUi_=false; throw; }
    changingUi_=false;
}
void DoorGapDialog::HideHandles() noexcept {
    if (overlay_) overlay_->Hide();
    changingUi_=true;
    for (auto* handle:handles_) if (handle) {
        try { handle->SetShowHandle(false); handle->SetShowFocusHandle(false); }
        catch (...) {}
    }
    changingUi_=false;
}
void DoorGapDialog::ShowHandles() {
    changingUi_=true;
    try {
        for (int i=0;i<4;++i) {
            auto* handle=handles_[i];
            if (!handle) continue;
            if (!sides_[i].measured) { handle->SetShowHandle(false); continue; }
            const double value=Value(inputs_[i]);
            handle->SetFormula(Number(value).c_str());
            handle->SetValue(value);
            handle->SetHandleOrigin(sides_[i].handleOrigin);
            handle->SetHandleOrientation(sides_[i].outward);
            handle->SetAutoReverseDuringDrag(true);
            handle->SetShowFocusHandle(false);
            handle->SetShowHandle(false);
        }
        UF_DISP_refresh();
        if (overlay_) {
            std::array<NXOpen::Point3d,4> anchors{};
            std::array<NXOpen::Vector3d,4> outwards{};
            std::array<bool,4> available{};
            std::array<double,4> values{};
            for (int i=0;i<4;++i) {
                anchors[i]=sides_[i].handleOrigin;
                outwards[i]=sides_[i].outward;
                available[i]=sides_[i].measured;
                values[i]=Value(inputs_[i]);
            }
            overlay_->Show(anchors,outwards,available,values);
        }
    } catch (...) { changingUi_=false; throw; }
    changingUi_=false;
}
int DoorGapDialog::Update(NXOpen::BlockStyler::UIBlock* block) {
    if (changingUi_ || updating_) return 0;
    updating_=true;
    try {
        if (block==selection_) {
            HideHandles();
            std::string detail;
            NXOpen::Face* face=SelectedFace();
            if (!face) {
                valid_=false;
                SetLabel(status_,"请选择门板平面。可在多实体零件或装配中测量邻件。");
                changingUi_=true;
                for (int i=0;i<4;++i) {
                    SetLabel(labels_[i],"--");
                    SetValue(inputs_[i],0);
                    inputs_[i]->SetEnable(false);
                }
                changingUi_=false;
            } else if (!Analyze(face,detail)) {
                Error(detail);
                SetLabel(status_,detail);
                changingUi_=true;
                for (int i=0;i<4;++i) {
                    SetLabel(labels_[i],"--");
                    SetValue(inputs_[i],0);
                    inputs_[i]->SetEnable(false);
                }
                changingUi_=false;
            } else {
                ShowValues();
                ShowHandles();
            }
        } else {
            for (int i=0;i<4;++i) if (block==handles_[i] && sides_[i].measured) {
                changingUi_=true;
                SetValue(inputs_[i],handles_[i]->Value());
                changingUi_=false;
            }
            if (valid_) for (int i=0;i<4;++i)
                if (block==handles_[i] || block==inputs_[i]) { ShowHandles(); break; }
        }
        updating_=false;
        return 0;
    } catch (const NXOpen::NXException& ex) { Error(ex.Message()); }
      catch (const std::exception& ex) { Error(ex.what()); }
      catch (...) { Error("更新门间隙显示失败。"); }
    changingUi_=false;
    updating_=false;
    return 1;
}
bool DoorGapDialog::MoveSide(int index, double distance, std::string& error) {
    if (std::abs(distance)<=kTolerance) return true;
    if (!IsAlive(prototypeFace_)) { error="门板面已改变，请重新选择。"; return false; }
    auto* face=dynamic_cast<NXOpen::Face*>(NXOpen::NXObjectManager::Get(prototypeFace_));
    if (!face) { error="无法获取门板原型面。"; return false; }
    const auto localOrigin=ToLocal(origin_,transform_);
    const auto localNormal=ToLocal(normal_,transform_);
    const auto localOutward=ToLocal(sides_[index].outward,transform_);
    const double selectionTolerance=(std::max)(kTolerance,thickness_*1.0e-5);
    auto selection=panel_skirt::Collect(face,localOrigin,localNormal,
                                       localOutward,thickness_,selectionTolerance);
    if (!selection.error.empty() || selection.faces.empty()) {
        error=std::string(sides_[index].name)+"侧"+
            (selection.error.empty()?"无法识别可移动边界面。":selection.error);
        return false;
    }
    // A second, mirrored occurrence of the same door may be the reference.
    // Move Face edits the shared prototype, so both boundaries then move.
    // Account for the reference occurrence's motion before committing.
    double effectiveDistance=distance;
    const tag_t referenceTag=sides_[index].referenceFace;
    if (referenceTag!=NULL_TAG && UF_ASSEM_is_occurrence(referenceTag)) {
        const tag_t referencePrototype=UF_ASSEM_ask_prototype_of_occ(referenceTag);
        const bool sharedMovingFace=std::any_of(selection.faces.begin(),
            selection.faces.end(),[referencePrototype](NXOpen::Face* candidate) {
                return candidate && candidate->Tag()==referencePrototype;
            });
        if (sharedMovingFace) {
            double referenceTransform[4][4]{};
            if (UF_ASSEM_ask_transform_of_occ(referenceTag,referenceTransform)!=0) {
                error="无法获取共用门板实例的装配方向。"; return false;
            }
            const double referenceProjection=Dot(sides_[index].outward,
                ToDisplay(localOutward,referenceTransform));
            const double gapResponse=1.0-referenceProjection;
            if (std::abs(gapResponse)<0.1) {
                error="两扇共用门板同步移动，当前侧间隙无法单独调整；请将门板组件设为独立零件。";
                return false;
            }
            effectiveDistance=distance/gapResponse;
            Log(std::string(sides_[index].name)+"侧共用原型补偿：间隙响应系数="+
                Number(gapResponse)+"，面移动="+Number(effectiveDistance)+" mm");
        }
    }
    std::vector<panel_skirt::SurfacePosition> original;
    for (auto* movingFace:selection.faces) {
        panel_skirt::SurfacePosition item;
        if (!panel_skirt::Measure(movingFace,localOrigin,localNormal,localOutward,item)) {
            error="无法测量移动前的门板边界。"; return false;
        }
        original.push_back(item);
    }
    auto* work=session_->Parts()->Work();
    NXOpen::Features::MoveFaceBuilder* builder=nullptr;
    try {
        auto* direction=work->Directions()->CreateDirection(localOrigin,localOutward,
            NXOpen::SmartObject::UpdateOptionWithinModeling);
        builder=work->Features()->CreateMoveFaceBuilder(nullptr);
        builder->SetType(NXOpen::Features::MoveFaceBuilder::TypesTranslateDirectionAndDistance);
        builder->SetDirection(direction);
        builder->Distance()->SetFormula(Number(effectiveDistance).c_str());
        auto* options=work->ScRuleFactory()->CreateRuleOptions();
        options->SetSelectedFromInactive(false);
        auto* rule=work->ScRuleFactory()->CreateRuleFaceDumb(selection.faces,options);
        delete options;
        builder->MoveFaceCollector()->ReplaceRules(
            std::vector<NXOpen::SelectionIntentRule*>{rule},false);
        const auto result=panel_skirt::CommitAndVerifyMove(builder,original,
            localOrigin,localNormal,localOutward,effectiveDistance,selectionTolerance);
        builder->Destroy();
        builder=nullptr;
        if (!result.error.empty()) { error=result.error; return false; }
        return true;
    } catch (const NXOpen::NXException& ex) {
        if (builder) builder->Destroy();
        error=ex.Message()?ex.Message():"移动门板边界失败。";
        return false;
    }
}
bool DoorGapDialog::ValidateResult(const std::array<double,4>& desired,
                                    std::string& error) {
    NXOpen::Body* body=IsAlive(selectedBody_) ? dynamic_cast<NXOpen::Body*>(
        NXOpen::NXObjectManager::Get(selectedBody_)):nullptr;
    if (!body && IsAlive(selectedComponent_) && IsAlive(prototypeBody_)) {
        auto* component=dynamic_cast<NXOpen::Assemblies::Component*>(
            NXOpen::NXObjectManager::Get(selectedComponent_));
        auto* prototype=dynamic_cast<NXOpen::Body*>(
            NXOpen::NXObjectManager::Get(prototypeBody_));
        body=component && prototype ? dynamic_cast<NXOpen::Body*>(
            component->FindOccurrence(prototype)):nullptr;
        if (body) selectedBody_=body->Tag();
    }
    if (!body) { error="调整后门板实体失效。"; return false; }
    std::array<double,4> bounds;
    if (!BodyExtents(body,bounds)) { error="调整后无法复测门板尺寸。"; return false; }
    const double boundaries[4]{-bounds[0],bounds[1],-bounds[2],bounds[3]};
    for (int i=0;i<4;++i) {
        if (!sides_[i].measured) continue;
        if (!IsAlive(sides_[i].referenceFace)) {
            error="邻件基准面已失效，无法校验间隙。"; return false;
        }
        auto* reference=dynamic_cast<NXOpen::Face*>(
            NXOpen::NXObjectManager::Get(sides_[i].referenceFace));
        NXOpen::Point3d point; NXOpen::Vector3d normal;
        if (!PlaneData(reference,point,normal)) {
            error="无法复测邻件基准面。"; return false;
        }
        const double actual=Coordinate(point,origin_,sides_[i].outward)-boundaries[i];
        if (std::abs(actual-desired[i])>(std::max)(0.05,std::abs(desired[i])*0.01)) {
            error=std::string(sides_[i].name)+"侧调整后间隙 "+Fixed(actual)+
                " mm，与目标 "+Fixed(desired[i])+" mm 不符，已回滚。";
            return false;
        }
    }
    return true;
}
int DoorGapDialog::Apply() {
    const char* markName="查看调整门间隙";
    auto mark=static_cast<NXOpen::Session::UndoMarkId>(0);
    const auto rollback=[this,markName,&mark]() noexcept {
        if (mark==static_cast<NXOpen::Session::UndoMarkId>(0)) return;
        try {
            session_->UndoToMark(mark,markName);
            if (session_->DoesUndoMarkExist(mark,markName))
                session_->DeleteUndoMark(mark,markName);
        } catch (...) {}
        mark=static_cast<NXOpen::Session::UndoMarkId>(0);
    };
    try {
        if (!valid_ || !IsAlive(prototypeFace_)) {
            Error("请先选择有效门板平面。" ); return 1;
        }
        std::array<double,4> desired{};
        int changed=0;
        for (int i=0;i<4;++i) {
            desired[i]=0;
            if (!sides_[i].measured) continue;
            std::string inputError;
            if (!overlay_ || !overlay_->Read(i,desired[i],inputError)) {
                Error(inputError.empty()?"无法读取模型上的间隙输入框。":inputError); return 1;
            }
            if (std::abs(desired[i]-sides_[i].gap)>kTolerance) ++changed;
        }
        if (!changed) { Error("四边目标间隙与当前值相同。" ); return 1; }
        auto* prototype=dynamic_cast<NXOpen::Face*>(NXOpen::NXObjectManager::Get(prototypeFace_));
        auto* target=prototype?dynamic_cast<NXOpen::Part*>(prototype->OwningPart()):nullptr;
        auto* component=IsAlive(selectedComponent_)?
            dynamic_cast<NXOpen::Assemblies::Component*>(
                NXOpen::NXObjectManager::Get(selectedComponent_)):nullptr;
        CallbackPause callbacks(updating_);
        WorkComponentScope workContext(session_,target,component);
        mark=session_->SetUndoMark(NXOpen::Session::MarkVisibilityVisible,markName);
        std::string error;
        bool success=true;
        for (int i=0;i<4;++i) if (sides_[i].measured &&
            !MoveSide(i,sides_[i].gap-desired[i],error)) { success=false; break; }
        if (success) success=ValidateResult(desired,error);
        if (!success) {
            rollback();
            workContext.Restore();
            callbacks.Restore();
            Error(error.empty()?"门间隙调整失败，已回滚。":error);
            return 1;
        }
        workContext.Restore();
        callbacks.Restore();
        mark=static_cast<NXOpen::Session::UndoMarkId>(0);
        try {
            if (IsAlive(selectedFace_)) {
                std::string detail;
                auto* face=dynamic_cast<NXOpen::Face*>(NXOpen::NXObjectManager::Get(selectedFace_));
                if (face && Analyze(face,detail)) { ShowValues(); ShowHandles(); }
                else { valid_=false; HideHandles(); SetLabel(status_,"调整已完成，请重新选择门板面复测。"); }
            } else { valid_=false; HideHandles(); SetLabel(status_,"调整已完成，请重新选择门板面复测。"); }
        } catch (...) {
            valid_=false;
            HideHandles();
        }
        return 0;
    } catch (const NXOpen::NXException& ex) { rollback(); Error(ex.Message()); }
      catch (const std::exception& ex) { rollback(); Error(ex.what()); }
      catch (...) { rollback(); Error("应用门间隙时发生未知错误。"); }
    return 1;
}
int DoorGapDialog::Ok() { return Apply(); }
void DoorGapDialog::Error(const std::string& message) const noexcept {
    Log(message);
    try { if (ui_) ui_->NXMessageBox()->Show("查看调整门间隙",
        NXOpen::NXMessageBox::DialogTypeError,message.c_str()); }
    catch (...) {}
}
void DoorGapDialog::Log(const std::string& message) const noexcept {
    try {
        if (session_ && session_->LogFile())
            session_->LogFile()->WriteLine(("[查看调整门间隙] " + message).c_str());
    } catch (...) {}
}
