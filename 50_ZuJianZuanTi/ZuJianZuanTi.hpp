#pragma once

#include <NXOpen/BlockStyler_BlockDialog.hxx>
#include <NXOpen/Callback.hxx>
#include <NXOpen/Session.hxx>

namespace NXOpen {
class Part;
namespace Assemblies { class Component; }
namespace BlockStyler { class SelectObject; class UIBlock; }
}

class ComponentToSolidDialog final {
public:
    ComponentToSolidDialog();
    ~ComponentToSolidDialog();
    NXOpen::BlockStyler::BlockDialog::DialogResponse Launch();

private:
    void Initialize();
    void DialogShown();
    int Update(NXOpen::BlockStyler::UIBlock* block);
    int Apply();
    int Ok();
    int Filter(NXOpen::BlockStyler::UIBlock* block, NXOpen::TaggedObject* object);
    void ConfigureSelection();
    void LoadMemory();
    void SaveMemory();
    void SyncOptions();
    void FindDuplicateComponents();
    bool Convert(std::wstring& message);
    void Log(const std::string& message) const;
    void Notify(const std::wstring& message, bool error) const;

    NXOpen::Session* session_ = nullptr;
    NXOpen::BlockStyler::BlockDialog* dialog_ = nullptr;
    NXOpen::BlockStyler::SelectObject* selection_ = nullptr;
    NXOpen::BlockStyler::UIBlock* findDuplicates_ = nullptr;
    NXOpen::BlockStyler::UIBlock* layerMode_ = nullptr;
    NXOpen::BlockStyler::UIBlock* layerNumber_ = nullptr;
    NXOpen::BlockStyler::UIBlock* inheritPartAttributes_ = nullptr;
    NXOpen::BlockStyler::UIBlock* inheritBodyAttributes_ = nullptr;
    NXOpen::BlockStyler::UIBlock* deleteComponent_ = nullptr;
    NXOpen::BlockStyler::UIBlock* keepFile_ = nullptr;
    bool updating_ = false;
};
