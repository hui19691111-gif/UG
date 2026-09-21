#pragma once
#include "SphereGeometry.hpp"
#include <NXOpen/BlockStyler_BlockDialog.hxx>
#include <NXOpen/BlockStyler_UIBlock.hxx>
namespace NXOpen {class TaggedObject;namespace Features {class CustomFeature;}}
class QiuMianZanKaiDialog {
public:
    QiuMianZanKaiDialog();
    ~QiuMianZanKaiDialog();
    void Launch();
private:
    void Initialize();
    void Shown();
    int Update(NXOpen::BlockStyler::UIBlock*);
    int Filter(NXOpen::BlockStyler::UIBlock*,NXOpen::TaggedObject*);
    int Apply();
    int Cancel();
    void Preview();
    void Status(const std::string&);
    void Error(const std::string&) noexcept;
    sphere_unfold::Settings ReadSettings();
    NXOpen::BlockStyler::BlockDialog* dialog_=nullptr;
    NXOpen::BlockStyler::UIBlock *cylinder_=nullptr,*sphere_=nullptr,*numbers_[3]={},*flat_=nullptr,*hide_=nullptr,*status_=nullptr,*detail_=nullptr;
    NXOpen::Features::CustomFeature* edited_=nullptr;
    tag_t editCylinder_=0,editSphere_=0;
    bool initialized_=false,shown_=false,updating_=false,first_=true;
};
