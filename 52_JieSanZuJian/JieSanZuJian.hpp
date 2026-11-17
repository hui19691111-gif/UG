#pragma once

#include <NXOpen/BlockStyler_BlockDialog.hxx>
#include <NXOpen/Callback.hxx>
#include <NXOpen/Session.hxx>

namespace NXOpen {
namespace BlockStyler { class SelectObject; class UIBlock; }
}

class DissolveComponentDialog final {
public:
    DissolveComponentDialog();
    ~DissolveComponentDialog();
    NXOpen::BlockStyler::BlockDialog::DialogResponse Launch();

private:
    void Initialize();
    void DialogShown();
    int Update(NXOpen::BlockStyler::UIBlock* block);
    int Apply();
    int Ok();
    int Filter(NXOpen::BlockStyler::UIBlock* block, NXOpen::TaggedObject* object);
    void ConfigureSelection();
    void ExpandToSiblingInstances();
    bool Dissolve(std::wstring& message);
    void Log(const std::string& message) const;
    void Notify(const std::wstring& message, bool error) const;

    NXOpen::Session* session_ = nullptr;
    NXOpen::BlockStyler::BlockDialog* dialog_ = nullptr;
    NXOpen::BlockStyler::SelectObject* selection_ = nullptr;
    bool expandingSelection_ = false;
};
