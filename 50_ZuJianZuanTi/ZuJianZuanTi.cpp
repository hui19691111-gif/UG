#include "ZuJianZuanTi.hpp"

#include "ZhihuiDialogMemory.hpp"
#ifdef CreateDialog
#undef CreateDialog
#endif

#include <NXOpen/Assemblies_Component.hxx>
#include <NXOpen/BlockStyler_PropertyList.hxx>
#include <NXOpen/BlockStyler_SelectObject.hxx>
#include <NXOpen/BlockStyler_UIBlock.hxx>
#include <NXOpen/Body.hxx>
#include <NXOpen/Features_ExtractFaceBuilder.hxx>
#include <NXOpen/Features_Feature.hxx>
#include <NXOpen/Features_FeatureCollection.hxx>
#include <NXOpen/LogFile.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/SelectObjectList.hxx>
#include <NXOpen/Selection.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/UI.hxx>
#include <NXOpen/Update.hxx>

#include <uf.h>
#include <uf_assem.h>
#include <uf_layer.h>
#include <uf_modl.h>
#include <uf_obj.h>
#include <uf_object_types.h>
#include <uf_part.h>
#include <uf_so.h>
#include <uf_ui.h>
#include <uf_wave.h>

#include <filesystem>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#ifdef CreateDialog
#undef CreateDialog
#endif

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace {
constexpr const wchar_t* kMemoryFile = L"ZuJianZuanTi.ini";
constexpr int kLayerOriginal = 0;
constexpr int kLayerWork = 1;
constexpr int kLayerSpecified = 2;

std::wstring WideFromLocale(const char* text) {
    if (text == nullptr || text[0] == '\0') return std::wstring();
    const int size = MultiByteToWideChar(CP_ACP, 0, text, -1, nullptr, 0);
    if (size <= 1) return std::wstring();
    std::wstring result(static_cast<std::size_t>(size - 1), L'\0');
    MultiByteToWideChar(CP_ACP, 0, text, -1, &result[0], size);
    return result;
}

std::wstring WideFromUtf8(const char* text) {
    if (text == nullptr || text[0] == '\0') return std::wstring();
    const int size = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
    if (size <= 1) return std::wstring();
    std::wstring result(static_cast<std::size_t>(size - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text, -1, &result[0], size);
    return result;
}

std::string DlxPath() {
    wchar_t module[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(
        reinterpret_cast<HMODULE>(&__ImageBase), module, MAX_PATH);
    if (length > 0 && length < MAX_PATH) {
        return (std::filesystem::path(module).parent_path() / L"ZuJianZuanTi.dlx").string();
    }
    return "ZuJianZuanTi.dlx";
}

void SetShow(NXOpen::BlockStyler::UIBlock* block, bool show) {
    if (block == nullptr) return;
    NXOpen::BlockStyler::PropertyList* props = nullptr;
    try {
        props = block->GetProperties();
        props->SetLogical("Show", show);
        delete props;
    } catch (...) {
        delete props;
    }
}

void SetEnable(NXOpen::BlockStyler::UIBlock* block, bool enable) {
    if (block == nullptr) return;
    NXOpen::BlockStyler::PropertyList* props = nullptr;
    try {
        props = block->GetProperties();
        props->SetLogical("Enable", enable);
        delete props;
    } catch (...) {
        delete props;
    }
}

int ReadLayerNumber(NXOpen::BlockStyler::UIBlock* block) {
    double value = 1.0;
    zhihui_dialog_memory::TryGetDouble(block, &value);
    int layer = static_cast<int>(value + (value >= 0.0 ? 0.5 : -0.5));
    if (layer < 1) layer = 1;
    if (layer > 256) layer = 256;
    return layer;
}

bool IsSolidBody(tag_t body) {
    if (body == NULL_TAG) return false;
    int type = 0;
    int subtype = 0;
    if (UF_OBJ_ask_type_and_subtype(body, &type, &subtype) != 0) return false;
    if (type != UF_solid_type || subtype != UF_solid_body_subtype) return false;
    int bodyType = 0;
    const tag_t prototype = UF_ASSEM_is_occurrence(body)
        ? UF_ASSEM_ask_prototype_of_occ(body) : body;
    if (prototype == NULL_TAG || UF_MODL_ask_body_type(prototype, &bodyType) != 0) {
        return false;
    }
    return bodyType == UF_MODL_SOLID_BODY;
}

std::vector<tag_t> ChildComponents(tag_t partOccurrence) {
    std::vector<tag_t> components;
    if (partOccurrence == NULL_TAG) return components;
    tag_t* children = nullptr;
    const int count = UF_ASSEM_ask_part_occ_children(partOccurrence, &children);
    for (int index = 0; index < count; ++index) {
        components.push_back(children[index]);
        const std::vector<tag_t> nested = ChildComponents(children[index]);
        components.insert(components.end(), nested.begin(), nested.end());
    }
    UF_free(children);
    return components;
}

std::vector<tag_t> AssemblyComponents(tag_t part) {
    return ChildComponents(UF_ASSEM_ask_root_part_occ(part));
}

std::wstring ComponentLabel(tag_t component) {
    char partName[MAX_FSPEC_BUFSIZE + 1]{};
    char refset[UF_OBJ_NAME_BUFSIZE]{};
    char instance[UF_OBJ_NAME_BUFSIZE]{};
    double origin[3]{};
    double csys[9]{};
    double transform[4][4]{};
    if (UF_ASSEM_ask_component_data(
            component, partName, refset, instance, origin, csys, transform) == 0 &&
        instance[0] != '\0') {
        return WideFromLocale(instance);
    }
    const char* slash = std::strrchr(partName, '\\');
    if (slash == nullptr) slash = std::strrchr(partName, '/');
    return WideFromLocale(slash != nullptr ? slash + 1 : partName);
}

void CopyAttributes(NXOpen::NXObject* source, NXOpen::NXObject* target) {
    if (source == nullptr || target == nullptr || source == target) return;
    std::vector<NXOpen::NXObject::AttributeInformation> attributes;
    try {
        attributes = source->GetUserAttributes();
    } catch (...) {
        return;
    }
    for (NXOpen::NXObject::AttributeInformation info : attributes) {
        if (info.Type == NXOpen::NXObject::AttributeTypeNull ||
            info.Type == NXOpen::NXObject::AttributeTypeTime) {
            continue;
        }
        info.Inherited = false;
        info.IsOverride = false;
        info.Locked = false;
        info.OwnedBySystem = false;
        info.Required = false;
        info.Unset = false;
        info.Expression = nullptr;
        info.ExpressionNotLoaded = false;
        if (info.Type == NXOpen::NXObject::AttributeTypeReference) {
            info.Type = NXOpen::NXObject::AttributeTypeString;
        }
        info.ReferenceValue = NXOpen::NXString();
        try {
            target->SetUserAttribute(info, NXOpen::Update::OptionNow);
        } catch (...) {}
    }
}

void NameBody(tag_t body, const std::wstring& label, int index, int count) {
    std::wstring name = label.empty() ? L"SOLID" : label;
    if (count > 1) name += L"_" + std::to_wstring(index + 1);
    if (name.size() > 120) name.resize(120);
    const int bytes = WideCharToMultiByte(
        CP_ACP, 0, name.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (bytes <= 1) return;
    std::string locale(static_cast<std::size_t>(bytes - 1), '\0');
    WideCharToMultiByte(
        CP_ACP, 0, name.c_str(), -1, &locale[0], bytes, nullptr, nullptr);
    if (UF_OBJ_set_name(body, locale.c_str()) == 0) return;
    locale += "_" + std::to_string(static_cast<int>(body & 0xFFFF));
    UF_OBJ_set_name(body, locale.c_str());
}

int OriginalLayer(tag_t body) {
    const tag_t prototype = UF_ASSEM_is_occurrence(body)
        ? UF_ASSEM_ask_prototype_of_occ(body) : body;
    UF_OBJ_disp_props_t props{};
    if (prototype != NULL_TAG &&
        UF_OBJ_ask_display_properties(prototype, &props) == 0 &&
        props.layer >= 1 && props.layer <= 256) {
        return props.layer;
    }
    int layer = 1;
    UF_LAYER_ask_work_layer(&layer);
    return layer >= 1 && layer <= 256 ? layer : 1;
}

void ApplyLayer(tag_t body, int mode, int specified, int sourceBody) {
    int layer = 1;
    if (mode == kLayerOriginal) layer = OriginalLayer(sourceBody);
    else if (mode == kLayerSpecified) layer = specified;
    else UF_LAYER_ask_work_layer(&layer);
    if (layer < 1 || layer > 256) layer = 1;
    UF_OBJ_set_layer(body, layer);
    UF_OBJ_set_blank_status(body, UF_OBJ_NOT_BLANKED);
}

tag_t ExtractOccurrenceBody(NXOpen::Part* workPart, tag_t sourceBody) {
    NXOpen::Body* body = dynamic_cast<NXOpen::Body*>(
        NXOpen::NXObjectManager::Get(sourceBody));
    if (body == nullptr) return NULL_TAG;
    NXOpen::Features::ExtractFaceBuilder* builder =
        workPart->Features()->CreateExtractFaceBuilder(nullptr);
    if (builder == nullptr) return NULL_TAG;
    tag_t copied = NULL_TAG;
    try {
        builder->SetType(NXOpen::Features::ExtractFaceBuilder::ExtractTypeBody);
        builder->SetParentPart(
            UF_ASSEM_is_occurrence(sourceBody)
                ? NXOpen::Features::ExtractFaceBuilder::ParentPartTypeOtherPart
                : NXOpen::Features::ExtractFaceBuilder::ParentPartTypeWorkPart);
        builder->SetAssociative(false);
        builder->SetHideOriginal(false);
        builder->SetInheritDisplayProperties(true);
        builder->SetMakePositionIndependent(false);
#pragma warning(push)
#pragma warning(disable: 4996)
        builder->BodyToExtract()->Add(body);
#pragma warning(pop)
        NXOpen::Features::Feature* feature = builder->CommitFeature();
        if (feature != nullptr) {
            const std::vector<NXOpen::Body*> bodies = feature->GetBodies();
            if (bodies.size() == 1 && bodies[0] != nullptr) copied = bodies[0]->Tag();
        }
        builder->Destroy();
    } catch (...) {
        try { builder->Destroy(); } catch (...) {}
        throw;
    }
    return copied;
}

tag_t LinkPrototypeBody(
    tag_t workPart, tag_t component, tag_t workOccurrence, tag_t prototypeBody) {
    tag_t xform = NULL_TAG;
    // to_part_occ is the work component. NULL means the displayed assembly root.
    if (UF_SO_create_xform_assy_ctxt(workPart, component, workOccurrence, &xform) != 0 ||
        xform == NULL_TAG) {
        return NULL_TAG;
    }
    tag_t feature = NULL_TAG;
    if (UF_WAVE_create_linked_body(prototypeBody, xform, workPart, 0, &feature) != 0 ||
        feature == NULL_TAG) {
        return NULL_TAG;
    }
    tag_t body = NULL_TAG;
    UF_MODL_ask_feat_body(feature, &body);
    return body;
}

struct CopiedBody {
    tag_t body = NULL_TAG;
    tag_t source = NULL_TAG;
    tag_t component = NULL_TAG;
    tag_t prototypePart = NULL_TAG;
};

bool WantedBody(tag_t prototype, const std::set<tag_t>* onlyPrototypes) {
    return onlyPrototypes == nullptr || onlyPrototypes->count(prototype) != 0;
}

std::vector<CopiedBody> CopyComponentSolids(
    NXOpen::Part* workPart,
    tag_t component,
    tag_t workOccurrence,
    const std::set<tag_t>* onlyPrototypes) {
    std::vector<CopiedBody> copies;
    const tag_t prototypePart = UF_ASSEM_ask_prototype_of_occ(component);
    if (prototypePart == NULL_TAG || prototypePart == workPart->Tag()) return copies;

    std::set<tag_t> seenPrototypes;
    tag_t occurrence = NULL_TAG;
    while ((occurrence = UF_ASSEM_cycle_ents_in_part_occ(component, occurrence)) != NULL_TAG) {
        if (!IsSolidBody(occurrence)) continue;
        const tag_t prototype = UF_ASSEM_ask_prototype_of_occ(occurrence);
        if (prototype == NULL_TAG || !WantedBody(prototype, onlyPrototypes) ||
            !seenPrototypes.insert(prototype).second) {
            continue;
        }
        const tag_t copied = ExtractOccurrenceBody(workPart, occurrence);
        if (copied == NULL_TAG) {
            throw NXOpen::NXException::Create("extract body failed");
        }
        copies.push_back({copied, occurrence, component, prototypePart});
    }

    tag_t prototypeBody = NULL_TAG;
    while (UF_OBJ_cycle_objs_in_part(prototypePart, UF_solid_type, &prototypeBody) == 0 &&
           prototypeBody != NULL_TAG) {
        if (!IsSolidBody(prototypeBody) || !WantedBody(prototypeBody, onlyPrototypes) ||
            !seenPrototypes.insert(prototypeBody).second) {
            continue;
        }
        const tag_t copied = LinkPrototypeBody(
            workPart->Tag(), component, workOccurrence, prototypeBody);
        if (copied == NULL_TAG) {
            throw NXOpen::NXException::Create("wave link body failed");
        }
        copies.push_back({copied, prototypeBody, component, prototypePart});
    }
    return copies;
}

void RemoveParameters(const std::vector<tag_t>& bodies) {
    if (bodies.empty()) return;
    uf_list_p_t list = nullptr;
    if (UF_MODL_create_list(&list) != 0) return;
    for (tag_t body : bodies) {
        if (body != NULL_TAG) UF_MODL_put_list_item(list, body);
    }
    UF_MODL_delete_body_parms(list);
    UF_MODL_delete_list(&list);
}

bool FileStillReferenced(tag_t prototype, const std::vector<tag_t>& remaining) {
    for (tag_t component : remaining) {
        if (UF_ASSEM_ask_prototype_of_occ(component) == prototype) return true;
    }
    return false;
}

std::wstring PartPath(tag_t part) {
    auto* object = dynamic_cast<NXOpen::Part*>(NXOpen::NXObjectManager::Get(part));
    if (object == nullptr) return std::wstring();
    const std::wstring utf8 = WideFromUtf8(object->FullPath().GetUTF8Text());
    if (!utf8.empty() && GetFileAttributesW(utf8.c_str()) != INVALID_FILE_ATTRIBUTES) {
        return utf8;
    }
    return WideFromLocale(object->FullPath().GetLocaleText());
}

bool DeletePartFile(tag_t part, tag_t displayPart) {
    if (part == NULL_TAG || part == displayPart) return false;
    const std::wstring path = PartPath(part);
    if (path.empty() || GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    if (UF_PART_close(part, 0, 1) != 0) return false;
    return DeleteFileW(path.c_str()) != FALSE;
}
}

ComponentToSolidDialog::ComponentToSolidDialog() {
    session_ = NXOpen::Session::GetSession();
    dialog_ = NXOpen::UI::GetUI()->CreateDialog(DlxPath().c_str());
    dialog_->AddInitializeHandler(
        NXOpen::make_callback(this, &ComponentToSolidDialog::Initialize));
    dialog_->AddDialogShownHandler(
        NXOpen::make_callback(this, &ComponentToSolidDialog::DialogShown));
    dialog_->AddUpdateHandler(
        NXOpen::make_callback(this, &ComponentToSolidDialog::Update));
    dialog_->AddApplyHandler(
        NXOpen::make_callback(this, &ComponentToSolidDialog::Apply));
    dialog_->AddOkHandler(NXOpen::make_callback(this, &ComponentToSolidDialog::Ok));
    dialog_->AddFilterHandler(
        NXOpen::make_callback(this, &ComponentToSolidDialog::Filter));
}

ComponentToSolidDialog::~ComponentToSolidDialog() {
    delete dialog_;
}

NXOpen::BlockStyler::BlockDialog::DialogResponse ComponentToSolidDialog::Launch() {
    return dialog_->Launch();
}

void ComponentToSolidDialog::Log(const std::string& message) const {
    try { session_->LogFile()->WriteLine(message.c_str()); } catch (...) {}
    try {
        std::string line = message;
        if (line.empty() || line.back() != '\n') line.push_back('\n');
        UF_print_syslog(line.data(), false);
    } catch (...) {}
}

void ComponentToSolidDialog::Notify(const std::wstring& message, bool error) const {
    MessageBoxW(nullptr, message.c_str(), L"组件转实体",
        MB_OK | (error ? MB_ICONERROR : MB_ICONINFORMATION));
}

void ComponentToSolidDialog::Initialize() {
    try {
        auto* top = dialog_->TopBlock();
        selection_ = dynamic_cast<NXOpen::BlockStyler::SelectObject*>(
            top->FindBlock("component_selection"));
        findDuplicates_ = top->FindBlock("find_duplicates");
        layerMode_ = top->FindBlock("layer_mode");
        layerNumber_ = top->FindBlock("layer_number");
        inheritPartAttributes_ = top->FindBlock("inherit_part_attributes");
        inheritBodyAttributes_ = top->FindBlock("inherit_body_attributes");
        deleteComponent_ = top->FindBlock("delete_component");
        keepFile_ = top->FindBlock("keep_file");
        if (selection_ == nullptr || findDuplicates_ == nullptr || layerMode_ == nullptr ||
            layerNumber_ == nullptr || inheritPartAttributes_ == nullptr ||
            inheritBodyAttributes_ == nullptr || deleteComponent_ == nullptr ||
            keepFile_ == nullptr) {
            throw std::runtime_error("ZuJianZuanTi dialog block missing.");
        }
        ConfigureSelection();
    } catch (const NXOpen::NXException& ex) {
        Log(ex.Message() ? ex.Message() : "ZuJianZuanTi initialize NX exception.");
    } catch (const std::exception& ex) {
        Log(ex.what());
    } catch (...) {
        Log("ZuJianZuanTi initialize failed.");
    }
}

void ComponentToSolidDialog::ConfigureSelection() {
    selection_->SetMaximumScopeAsString("Entire Assembly");
    selection_->SetInterpartSelectionAsString("Simple");
    selection_->SetSelectModeAsString("Multiple");
    selection_->SetCreateInterpartLink(false);
    NXOpen::BlockStyler::PropertyList* props = selection_->GetProperties();
    try {
        std::vector<NXOpen::Selection::MaskTriple> masks;
        masks.emplace_back(UF_component_type, UF_component_subtype, 0);
        props->SetEnum("MaximumScope", 3);
        props->SetEnum("InterpartSelection", 0);
        props->SetEnum("SelectMode", 1);
        props->SetLogical("AutomaticProgression", false);
        props->SetEnum("StepStatus", 0);
        props->SetSelectionFilter(
            "SelectionFilter",
            NXOpen::Selection::SelectionActionClearAndEnableSpecific,
            masks);
    } catch (...) {
        delete props;
        throw;
    }
    delete props;
}

void ComponentToSolidDialog::DialogShown() {
    try {
        LoadMemory();
        SyncOptions();
        if (selection_ != nullptr) selection_->Focus();
    } catch (const NXOpen::NXException& ex) {
        Log(ex.Message() ? ex.Message() : "ZuJianZuanTi dialogShown NX exception.");
    } catch (const std::exception& ex) {
        Log(ex.what());
    } catch (...) {
        Log("ZuJianZuanTi dialogShown failed.");
    }
}

void ComponentToSolidDialog::LoadMemory() {
    zhihui_dialog_memory::LoadEnum(kMemoryFile, L"LayerMode", layerMode_);
    zhihui_dialog_memory::LoadDouble(kMemoryFile, L"LayerNumber", layerNumber_);
    zhihui_dialog_memory::LoadLogical(kMemoryFile, L"InheritPartAttributes", inheritPartAttributes_);
    zhihui_dialog_memory::LoadLogical(kMemoryFile, L"InheritBodyAttributes", inheritBodyAttributes_);
    zhihui_dialog_memory::LoadLogical(kMemoryFile, L"DeleteComponent", deleteComponent_);
    zhihui_dialog_memory::LoadLogical(kMemoryFile, L"KeepFile", keepFile_);
}

void ComponentToSolidDialog::SaveMemory() {
    zhihui_dialog_memory::SaveEnum(kMemoryFile, L"LayerMode", layerMode_);
    zhihui_dialog_memory::SaveDouble(kMemoryFile, L"LayerNumber", layerNumber_);
    zhihui_dialog_memory::SaveLogical(kMemoryFile, L"InheritPartAttributes", inheritPartAttributes_);
    zhihui_dialog_memory::SaveLogical(kMemoryFile, L"InheritBodyAttributes", inheritBodyAttributes_);
    zhihui_dialog_memory::SaveLogical(kMemoryFile, L"DeleteComponent", deleteComponent_);
    zhihui_dialog_memory::SaveLogical(kMemoryFile, L"KeepFile", keepFile_);
}

void ComponentToSolidDialog::SyncOptions() {
    int mode = kLayerOriginal;
    zhihui_dialog_memory::TryGetEnum(layerMode_, &mode);
    SetShow(layerNumber_, mode == kLayerSpecified);
    bool deleteSelected = true;
    zhihui_dialog_memory::TryGetLogical(deleteComponent_, &deleteSelected);
    SetEnable(keepFile_, deleteSelected);
}

int ComponentToSolidDialog::Update(NXOpen::BlockStyler::UIBlock* block) {
    if (updating_) return 0;
    try {
        if (block == findDuplicates_) {
            updating_ = true;
            FindDuplicateComponents();
            updating_ = false;
            return 0;
        }
        if (block == layerMode_ || block == deleteComponent_) SyncOptions();
    } catch (const NXOpen::NXException& ex) {
        updating_ = false;
        Log(ex.Message() ? ex.Message() : "ZuJianZuanTi update NX exception.");
    } catch (const std::exception& ex) {
        updating_ = false;
        Log(ex.what());
    } catch (...) {
        updating_ = false;
        Log("ZuJianZuanTi update failed.");
    }
    return 0;
}

int ComponentToSolidDialog::Filter(
    NXOpen::BlockStyler::UIBlock* block, NXOpen::TaggedObject* object) {
    try {
        if (block != selection_ || object == nullptr) return UF_UI_SEL_REJECT;
        const tag_t tag = object->Tag();
        if (tag == NULL_TAG || !UF_ASSEM_is_occurrence(tag)) return UF_UI_SEL_REJECT;
        if (dynamic_cast<NXOpen::Assemblies::Component*>(object) == nullptr) {
            return UF_UI_SEL_REJECT;
        }
        NXOpen::Part* work = session_->Parts()->Work();
        const tag_t workPart = work != nullptr ? work->Tag() : NULL_TAG;
        const tag_t prototype = UF_ASSEM_ask_prototype_of_occ(tag);
        return prototype != NULL_TAG && prototype != workPart
            ? UF_UI_SEL_ACCEPT : UF_UI_SEL_REJECT;
    } catch (...) {
        return UF_UI_SEL_REJECT;
    }
}

void ComponentToSolidDialog::FindDuplicateComponents() {
    NXOpen::Part* display = session_->Parts()->Display();
    if (display == nullptr || UF_ASSEM_ask_root_part_occ(display->Tag()) == NULL_TAG) {
        Notify(L"请先打开装配，再查找重复装配零件。", true);
        return;
    }
    NXOpen::Part* work = session_->Parts()->Work();
    const tag_t workPart = work != nullptr ? work->Tag() : NULL_TAG;
    std::set<tag_t> prototypes;
    for (NXOpen::TaggedObject* object : selection_->GetSelectedObjects()) {
        if (object == nullptr || object->Tag() == NULL_TAG) continue;
        if (dynamic_cast<NXOpen::Assemblies::Component*>(object) == nullptr) continue;
        const tag_t prototype = UF_ASSEM_ask_prototype_of_occ(object->Tag());
        if (prototype != NULL_TAG && prototype != workPart) prototypes.insert(prototype);
    }
    if (prototypes.empty()) {
        Notify(L"请先选择组件，再查找它在装配中的全部实例。", true);
        return;
    }

    std::map<tag_t, std::vector<tag_t>> matchesByPrototype;
    for (tag_t component : AssemblyComponents(display->Tag())) {
        const tag_t prototype = UF_ASSEM_ask_prototype_of_occ(component);
        if (prototypes.count(prototype) == 0) continue;
        matchesByPrototype[prototype].push_back(component);
    }
    std::vector<NXOpen::TaggedObject*> matches;
    bool foundAnother = false;
    for (const auto& item : matchesByPrototype) {
        if (item.second.size() > 1) foundAnother = true;
        for (tag_t component : item.second) {
            NXOpen::TaggedObject* object = NXOpen::NXObjectManager::Get(component);
            if (object != nullptr) matches.push_back(object);
        }
    }
    selection_->SetSelectedObjects(matches);
    if (selection_ != nullptr) selection_->Focus();
    std::ostringstream line;
    line << "ZuJianZuanTi: selected prototypes=" << prototypes.size()
         << " matched components=" << matches.size();
    Log(line.str());
    if (!foundAnother) {
        Notify(L"所选零件在装配中没有其他实例。", false);
    }
}

int ComponentToSolidDialog::Apply() {
    return 0;
}

int ComponentToSolidDialog::Ok() {
    try {
        SaveMemory();
        std::wstring message;
        if (!Convert(message)) {
            if (!message.empty()) Notify(message, true);
            return 1;
        }
        if (!message.empty()) Notify(message, false);
        return 0;
    } catch (const NXOpen::NXException& ex) {
        Log(ex.Message() ? ex.Message() : "ZuJianZuanTi ok NX exception.");
        Notify(L"组件转实体失败，模型已恢复到执行前。", true);
        return 1;
    } catch (const std::exception& ex) {
        Log(ex.what());
        Notify(L"组件转实体失败，模型已恢复到执行前。", true);
        return 1;
    } catch (...) {
        Log("ZuJianZuanTi ok failed.");
        Notify(L"组件转实体失败，模型已恢复到执行前。", true);
        return 1;
    }
}

bool ComponentToSolidDialog::Convert(std::wstring& message) {
    if (selection_ == nullptr) {
        message = L"对话框未正确初始化。";
        return false;
    }
    NXOpen::Part* display = session_->Parts()->Display();
    NXOpen::Part* work = session_->Parts()->Work();
    if (display == nullptr || work == nullptr) {
        message = L"没有打开的工作部件。";
        return false;
    }
    if (UF_ASSEM_ask_root_part_occ(display->Tag()) == NULL_TAG) {
        message = L"请在装配中使用组件转实体。";
        return false;
    }

    std::vector<tag_t> components;
    const std::vector<NXOpen::TaggedObject*> selected = selection_->GetSelectedObjects();
    for (NXOpen::TaggedObject* object : selected) {
        if (object == nullptr || object->Tag() == NULL_TAG) continue;
        if (dynamic_cast<NXOpen::Assemblies::Component*>(object) == nullptr) continue;
        if (UF_ASSEM_ask_prototype_of_occ(object->Tag()) == work->Tag()) continue;
        components.push_back(object->Tag());
    }
    if (components.empty()) {
        message = L"请先选择组件。";
        return false;
    }
    tag_t workOccurrence = NULL_TAG;
    if (NXOpen::Assemblies::Component* workComponent = session_->Parts()->WorkComponent()) {
        workOccurrence = workComponent->Tag();
    }

    int layerMode = kLayerOriginal;
    bool inheritPartAttributes = true;
    bool inheritBodyAttributes = true;
    bool deleteComponent = true;
    bool keepFile = false;
    zhihui_dialog_memory::TryGetEnum(layerMode_, &layerMode);
    zhihui_dialog_memory::TryGetLogical(inheritPartAttributes_, &inheritPartAttributes);
    zhihui_dialog_memory::TryGetLogical(inheritBodyAttributes_, &inheritBodyAttributes);
    zhihui_dialog_memory::TryGetLogical(deleteComponent_, &deleteComponent);
    zhihui_dialog_memory::TryGetLogical(keepFile_, &keepFile);
    const int specifiedLayer = ReadLayerNumber(layerNumber_);

    const NXOpen::Session::UndoMarkId mark = session_->SetUndoMark(
        NXOpen::Session::MarkVisibilityVisible,
        NXOpen::NXString("组件转实体", NXOpen::NXString::UTF8));
    const NXOpen::NXString undoName("组件转实体", NXOpen::NXString::UTF8);

    try {
        std::vector<CopiedBody> copies;
        std::vector<std::wstring> emptyComponents;
        std::set<tag_t> converted;
        for (tag_t component : components) {
            if (!converted.insert(component).second) continue;
            const std::vector<CopiedBody> solids =
                CopyComponentSolids(work, component, workOccurrence, nullptr);
            if (solids.empty()) {
                emptyComponents.push_back(ComponentLabel(component));
                continue;
            }
            copies.insert(copies.end(), solids.begin(), solids.end());
        }
        if (copies.empty()) {
            session_->UndoToMark(mark, undoName);
            message = L"所选组件中没有可转换的实体。";
            return false;
        }

        std::vector<tag_t> newBodies;
        newBodies.reserve(copies.size());
        for (const CopiedBody& copy : copies) newBodies.push_back(copy.body);
        RemoveParameters(newBodies);

        std::map<tag_t, int> nameIndex;
        std::map<tag_t, int> nameCount;
        for (const CopiedBody& copy : copies) nameCount[copy.component] += 1;
        for (const CopiedBody& copy : copies) {
            if (UF_OBJ_ask_status(copy.body) != UF_OBJ_ALIVE) continue;
            ApplyLayer(copy.body, layerMode, specifiedLayer, copy.source);
            const int index = nameIndex[copy.component]++;
            NameBody(copy.body, ComponentLabel(copy.component), index, nameCount[copy.component]);
            if (!inheritPartAttributes && !inheritBodyAttributes) continue;
            auto* target = dynamic_cast<NXOpen::NXObject*>(
                NXOpen::NXObjectManager::Get(copy.body));
            if (inheritPartAttributes) {
                CopyAttributes(dynamic_cast<NXOpen::NXObject*>(
                    NXOpen::NXObjectManager::Get(copy.prototypePart)), target);
            }
            if (inheritBodyAttributes) {
                tag_t sourceBody = copy.source;
                if (UF_ASSEM_is_occurrence(sourceBody)) {
                    const tag_t prototypeBody = UF_ASSEM_ask_prototype_of_occ(sourceBody);
                    if (prototypeBody != NULL_TAG) sourceBody = prototypeBody;
                }
                CopyAttributes(dynamic_cast<NXOpen::NXObject*>(
                    NXOpen::NXObjectManager::Get(sourceBody)), target);
            }
        }
        UF_MODL_update();

        int deletedComponents = 0;
        int skippedInstances = 0;
        std::set<tag_t> prototypesToClose;
        if (deleteComponent) {
            std::map<tag_t, std::set<tag_t>> selectedByInstance;
            for (tag_t component : converted) {
                if (nameCount[component] == 0) continue;
                const tag_t instance = UF_ASSEM_ask_inst_of_part_occ(component);
                if (instance != NULL_TAG) selectedByInstance[instance].insert(component);
            }
            const std::vector<tag_t> allComponents = AssemblyComponents(display->Tag());
            std::map<tag_t, int> instanceCounts;
            for (tag_t component : allComponents) {
                const tag_t instance = UF_ASSEM_ask_inst_of_part_occ(component);
                if (instance != NULL_TAG) instanceCounts[instance] += 1;
            }
            for (const auto& item : selectedByInstance) {
                if (static_cast<int>(item.second.size()) < instanceCounts[item.first]) {
                    ++skippedInstances;
                    continue;
                }
                tag_t prototype = NULL_TAG;
                if (!item.second.empty()) {
                    prototype = UF_ASSEM_ask_prototype_of_occ(*item.second.begin());
                }
                if (UF_ASSEM_remove_instance(item.first) != 0) {
                    throw NXOpen::NXException::Create("remove component instance failed");
                }
                deletedComponents += static_cast<int>(item.second.size());
                if (prototype != NULL_TAG) prototypesToClose.insert(prototype);
            }
            UF_MODL_update();
        }

        int deletedFiles = 0;
        int keptFiles = 0;
        if (deleteComponent && !keepFile) {
            const std::vector<tag_t> remaining = AssemblyComponents(display->Tag());
            for (tag_t prototype : prototypesToClose) {
                if (FileStillReferenced(prototype, remaining)) {
                    ++keptFiles;
                    continue;
                }
                if (DeletePartFile(prototype, display->Tag())) ++deletedFiles;
                else ++keptFiles;
            }
        }

        selection_->SetSelectedObjects({});
        std::wostringstream text;
        text << L"已转换 " << converted.size() - emptyComponents.size()
             << L" 个组件，生成 " << copies.size() << L" 个实体。";
        if (deleteComponent) text << L"\n已删除 " << deletedComponents << L" 个组件。";
        if (deleteComponent && !keepFile) {
            text << L"\n已删除 " << deletedFiles << L" 个磁盘文件。";
            if (keptFiles > 0) text << L"\n" << keptFiles << L" 个零件文件仍被引用或无法删除，已保留。";
        }
        if (skippedInstances > 0) {
            text << L"\n" << skippedInstances
                 << L" 个组件阵列还有未选中的实例，这些组件未删除。";
        }
        if (!emptyComponents.empty()) {
            text << L"\n跳过 " << emptyComponents.size() << L" 个没有实体的组件。";
        }
        message = text.str();
        std::ostringstream line;
        line << "ZuJianZuanTi: bodies=" << copies.size()
             << " deletedComponents=" << deletedComponents
             << " deletedFiles=" << deletedFiles;
        Log(line.str());
        return true;
    } catch (...) {
        try { session_->UndoToMark(mark, undoName); } catch (...) {}
        message = L"组件转实体失败，模型已恢复到执行前。";
        throw;
    }
}
