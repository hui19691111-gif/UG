#pragma once
#include <NXOpen/BlockStyler_BlockDialog.hxx>
#include <NXOpen/BlockStyler_UIBlock.hxx>
#include "ZeWanFuZuGeometry.hpp"
#include <string>
namespace NXOpen { class Session; class TaggedObject; }
namespace NXOpen { namespace Features {class CustomFeature;} }
class ZeWanFuZuDialog {
public:
    ZeWanFuZuDialog();
    ~ZeWanFuZuDialog();
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
    bend_assist::Settings ReadSettings() const;
    std::vector<bend_assist::FaceInfo> ReadFaces() const;
    NXOpen::BlockStyler::BlockDialog* dialog_=nullptr;
    NXOpen::BlockStyler::UIBlock* faces_=nullptr;
    NXOpen::BlockStyler::UIBlock* reference_=nullptr;
    NXOpen::BlockStyler::UIBlock* numbers_[6]={};
    NXOpen::BlockStyler::UIBlock* align_=nullptr;
    NXOpen::BlockStyler::UIBlock* reverse_=nullptr;
    NXOpen::BlockStyler::UIBlock* status_=nullptr;
    bool initialized_=false,shown_=false,updating_=false;
    std::vector<tag_t> selectedFaceTags_;
    NXOpen::Features::CustomFeature* edited_=nullptr;
};
