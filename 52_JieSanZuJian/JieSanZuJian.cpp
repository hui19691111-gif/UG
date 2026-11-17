#include "JieSanZuJian.hpp"
#include "../../common/ZhihuiContextHelp.hpp"

#ifdef CreateDialog
#undef CreateDialog
#endif

#include <NXOpen/Assemblies_Component.hxx>
#include <NXOpen/BlockStyler_PropertyList.hxx>
#include <NXOpen/BlockStyler_SelectObject.hxx>
#include <NXOpen/BlockStyler_UIBlock.hxx>
#include <NXOpen/LogFile.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/UI.hxx>

#include <uf.h>
#include <uf_assem.h>
#include <uf_cfi_types.h>
#include <uf_defs.h>
#include <uf_disp.h>
#include <uf_obj.h>
#include <uf_object_types.h>
#include <uf_part.h>
#include <uf_ui.h>

#include <NXOpen/Selection.hxx>

#include <cmath>
#include <filesystem>
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
struct Pose {
    double csys[9] = {1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0};
    double origin[3] = {0.0, 0.0, 0.0};
    // Unit factor from this component's coordinates into its owning part.
    // 1 = same units, 25.4 = component mm / parent inch, 0.03937 = the reverse.
    double scale = 1.0;
};

struct LeafPlacement {
    tag_t occurrence = NULL_TAG;
    Pose pose;
    std::string partPath;
    std::string referenceSet;
    std::string instanceName;
};

std::wstring WideFromLocale(const char* text) {
    if (text == nullptr || text[0] == '\0') return std::wstring();
    const int size = MultiByteToWideChar(CP_ACP, 0, text, -1, nullptr, 0);
    if (size <= 1) return std::wstring();
    std::wstring result(static_cast<std::size_t>(size - 1), L'\0');
    MultiByteToWideChar(CP_ACP, 0, text, -1, &result[0], size);
    return result;
}

std::wstring UfMessage(int code) {
    if (code == 0) return std::wstring();
    char text[133]{};
    if (UF_get_fail_message(code, text) != 0 || text[0] == '\0') {
        return L"错误码 " + std::to_wstring(code);
    }
    return WideFromLocale(text);
}

std::string DlxPath() {
    wchar_t module[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(
        reinterpret_cast<HMODULE>(&__ImageBase), module, MAX_PATH);
    if (length > 0 && length < MAX_PATH) {
        return (std::filesystem::path(module).parent_path() / L"JieSanZuJian.dlx").string();
    }
    return "JieSanZuJian.dlx";
}

void Rotate(const double csys[9], const double vector[3], double out[3]) {
    out[0] = csys[0] * vector[0] + csys[3] * vector[1] + csys[6] * vector[2];
    out[1] = csys[1] * vector[0] + csys[4] * vector[1] + csys[7] * vector[2];
    out[2] = csys[2] * vector[0] + csys[5] * vector[1] + csys[8] * vector[2];
}

Pose IdentityPose() {
    return Pose();
}

// map takes a point in the lower part and writes it in the higher part.
Pose Compose(const Pose& map, const Pose& inner) {
    const double factor = std::fabs(map.scale) > 1.0e-12 ? map.scale : 1.0;
    double scaled[3] = {
        inner.origin[0] / factor,
        inner.origin[1] / factor,
        inner.origin[2] / factor};
    Pose result;
    Rotate(map.csys, scaled, result.origin);
    result.origin[0] += map.origin[0];
    result.origin[1] += map.origin[1];
    result.origin[2] += map.origin[2];
    const double axes[3][3] = {
        {inner.csys[0], inner.csys[1], inner.csys[2]},
        {inner.csys[3], inner.csys[4], inner.csys[5]},
        {inner.csys[6], inner.csys[7], inner.csys[8]}};
    for (int axis = 0; axis < 3; ++axis) {
        double rotated[3]{};
        Rotate(map.csys, axes[axis], rotated);
        result.csys[axis * 3] = rotated[0];
        result.csys[axis * 3 + 1] = rotated[1];
        result.csys[axis * 3 + 2] = rotated[2];
    }
    result.scale = map.scale * inner.scale;
    return result;
}

// uf_assem.h: UF_ASSEM_ask_comp_position returns the total transform of a part
// occurrence relative to the root part occurrence. Multiplying each parent's
// UF_ASSEM_ask_component_data on top of that moved every leaf.
bool ReadRootTransform(tag_t occurrence, double transform[4][4]) {
    if (UF_ASSEM_ask_comp_position(NULL_TAG, occurrence, transform) == 0) return true;
    return UF_ASSEM_ask_transform_of_occ(occurrence, transform) == 0;
}

void PoseFromRootTransform(const double transform[4][4], Pose& pose) {
    pose = IdentityPose();
    double scale = transform[3][3];
    if (!(std::fabs(scale) > 1.0e-12)) scale = 1.0;
    // Origin is column 3. Divide only when the part units differ from its parent.
    const double unit = std::fabs(scale - 1.0) > 1.0e-6 ? scale : 1.0;
    pose.origin[0] = transform[0][3] / unit;
    pose.origin[1] = transform[1][3] / unit;
    pose.origin[2] = transform[2][3] / unit;
    pose.csys[0] = transform[0][0];
    pose.csys[1] = transform[1][0];
    pose.csys[2] = transform[2][0];
    pose.csys[3] = transform[0][1];
    pose.csys[4] = transform[1][1];
    pose.csys[5] = transform[2][1];
    pose.csys[6] = transform[0][2];
    pose.csys[7] = transform[1][2];
    pose.csys[8] = transform[2][2];
}

Pose InvertRigid(const Pose& pose) {
    Pose inverse;
    inverse.csys[0] = pose.csys[0];
    inverse.csys[1] = pose.csys[3];
    inverse.csys[2] = pose.csys[6];
    inverse.csys[3] = pose.csys[1];
    inverse.csys[4] = pose.csys[4];
    inverse.csys[5] = pose.csys[7];
    inverse.csys[6] = pose.csys[2];
    inverse.csys[7] = pose.csys[5];
    inverse.csys[8] = pose.csys[8];
    double rotated[3]{};
    Rotate(inverse.csys, pose.origin, rotated);
    inverse.origin[0] = -rotated[0];
    inverse.origin[1] = -rotated[1];
    inverse.origin[2] = -rotated[2];
    return inverse;
}

bool PoseInWorkPart(tag_t leaf, tag_t workOccurrence, Pose& pose) {
    double leafTransform[4][4]{};
    if (!ReadRootTransform(leaf, leafTransform)) return false;
    Pose leafPose;
    PoseFromRootTransform(leafTransform, leafPose);
    if (workOccurrence == NULL_TAG) {
        pose = leafPose;
        return true;
    }
    double workTransform[4][4]{};
    if (!ReadRootTransform(workOccurrence, workTransform)) return false;
    Pose workPose;
    PoseFromRootTransform(workTransform, workPose);
    pose = Compose(InvertRigid(workPose), leafPose);
    return true;
}

int ChildCount(tag_t occurrence) {
    tag_t* children = nullptr;
    const int count = UF_ASSEM_ask_all_part_occ_children(occurrence, &children);
    UF_free(children);
    return count;
}

bool CollectLeaves(tag_t occurrence, std::vector<tag_t>& leaves) {
    tag_t* children = nullptr;
    const int count = UF_ASSEM_ask_all_part_occ_children(occurrence, &children);
    if (count < 0) {
        UF_free(children);
        return false;
    }
    if (count == 0) {
        UF_free(children);
        leaves.push_back(occurrence);
        return true;
    }
    for (int index = 0; index < count; ++index) {
        if (!CollectLeaves(children[index], leaves)) {
            UF_free(children);
            return false;
        }
    }
    UF_free(children);
    return true;
}

bool ContainsTag(const std::vector<tag_t>& items, tag_t tag) {
    for (tag_t item : items) {
        if (item == tag) return true;
    }
    return false;
}

bool TreeContains(tag_t root, tag_t target) {
    if (root == NULL_TAG || target == NULL_TAG) return false;
    std::vector<tag_t> pending;
    pending.push_back(root);
    for (std::size_t index = 0; index < pending.size(); ++index) {
        if (pending[index] == target) return true;
        tag_t* children = nullptr;
        const int count = UF_ASSEM_ask_all_part_occ_children(pending[index], &children);
        if (count > 0) pending.insert(pending.end(), children, children + count);
        UF_free(children);
        if (pending.size() > 100000) break;
    }
    return false;
}

// One click selects a single assembly-navigator node. Other instances of that
// same part in the same parent are separate instance tags, so gather them here.
bool CollectGroupInstances(
    tag_t selected, std::vector<tag_t>& instances, std::wstring& error) {
    const tag_t instance = UF_ASSEM_ask_inst_of_part_occ(selected);
    if (instance == NULL_TAG) {
        error = L"找不到所选组件的装配实例。";
        return false;
    }
    const tag_t prototype = UF_ASSEM_ask_prototype_of_occ(selected);
    const tag_t parentPart = UF_ASSEM_ask_parent_of_instance(instance);
    if (prototype == NULL_TAG || parentPart == NULL_TAG) {
        instances.push_back(instance);
        return true;
    }
    tag_t current = NULL_TAG;
    for (int guard = 0; guard < 10000; ++guard) {
        current = UF_ASSEM_cycle_inst_of_part(parentPart, current);
        if (current == NULL_TAG) break;
        if (UF_ASSEM_ask_child_of_instance(current) != prototype) continue;
        instances.push_back(current);
    }
    if (instances.empty()) instances.push_back(instance);
    return true;
}

bool CollectGroupOccurrences(
    const std::vector<tag_t>& instances, tag_t selected, tag_t scopeRoot,
    std::vector<tag_t>& groups, std::wstring& error) {
    for (tag_t instance : instances) {
        tag_t* occurrences = nullptr;
        const int count = UF_ASSEM_ask_part_occs_of_inst(instance, &occurrences);
        if (count < 0) {
            UF_free(occurrences);
            error = L"读取组件实例失败。";
            return false;
        }
        for (int index = 0; index < count; ++index) {
            const tag_t occurrence = occurrences[index];
            if (occurrence == NULL_TAG || ChildCount(occurrence) <= 0) continue;
            if (scopeRoot != NULL_TAG && !TreeContains(scopeRoot, occurrence)) continue;
            if (!ContainsTag(groups, occurrence)) groups.push_back(occurrence);
        }
        UF_free(occurrences);
    }
    if (!ContainsTag(groups, selected)) groups.insert(groups.begin(), selected);
    return true;
}

bool PrototypeHasBody(tag_t part) {
    tag_t object = NULL_TAG;
    while (UF_OBJ_cycle_objs_in_part(part, UF_solid_type, &object) == 0 &&
           object != NULL_TAG) {
        int type = 0;
        int subtype = 0;
        if (UF_OBJ_ask_type_and_subtype(object, &type, &subtype) != 0) continue;
        if (type != UF_solid_type || subtype != UF_solid_body_subtype) continue;
        if (UF_ASSEM_is_occurrence(object)) continue;
        if (UF_OBJ_ask_status(object) != UF_OBJ_ALIVE) continue;
        return true;
    }
    return false;
}

std::wstring PartFileName(tag_t part) {
    char specification[MAX_FSPEC_BUFSIZE]{};
    if (UF_PART_ask_part_name(part, specification) != 0 || specification[0] == '\0') {
        return L"未命名.prt";
    }
    const std::wstring path = WideFromLocale(specification);
    if (path.empty()) return L"未命名.prt";
    return std::filesystem::path(path).filename().wstring();
}

// Bodies modeled in an assembly PRT are not leaf components. Removing that
// component drops them from the current assembly, so name those parts first.
bool CollectAssemblyBodyParts(
    tag_t occurrence, std::vector<std::wstring>& names, std::set<tag_t>& seen) {
    tag_t* children = nullptr;
    const int count = UF_ASSEM_ask_all_part_occ_children(occurrence, &children);
    if (count < 0) {
        UF_free(children);
        return false;
    }
    if (count == 0) {
        UF_free(children);
        return true;
    }
    const tag_t prototype = UF_ASSEM_ask_prototype_of_occ(occurrence);
    if (prototype != NULL_TAG && seen.insert(prototype).second &&
        PrototypeHasBody(prototype)) {
        names.push_back(PartFileName(prototype));
    }
    for (int index = 0; index < count; ++index) {
        if (!CollectAssemblyBodyParts(children[index], names, seen)) {
            UF_free(children);
            return false;
        }
    }
    UF_free(children);
    return true;
}

std::wstring OccurrenceFileName(tag_t occurrence) {
    char partName[MAX_FSPEC_BUFSIZE]{};
    char refset[UF_OBJ_NAME_BUFSIZE]{};
    char instance[UF_CFI_MAX_FILE_NAME_BUFSIZE]{};
    double origin[3]{};
    double csys[9]{};
    double transform[4][4]{};
    if (UF_ASSEM_ask_component_data(
            occurrence, partName, refset, instance, origin, csys, transform) == 0 &&
        partName[0] != '\0') {
        const std::wstring path = WideFromLocale(partName);
        if (!path.empty()) return std::filesystem::path(path).filename().wstring();
    }
    const tag_t prototype = UF_ASSEM_ask_prototype_of_occ(occurrence);
    if (prototype != NULL_TAG) return PartFileName(prototype);
    return L"未加载的抑制组件";
}

bool QuerySuppressState(tag_t occurrence, bool& suppressed, bool& parentSuppressed) {
    logical self = 0;
    logical parent = 0;
    if (UF_ASSEM_ask_part_occ_suppress_state(occurrence, &self, &parent) != 0) return false;
    suppressed = self != 0;
    parentSuppressed = parent != 0;
    return true;
}

bool EffectivelySuppressed(tag_t occurrence) {
    bool suppressed = false;
    bool parentSuppressed = false;
    if (!QuerySuppressState(occurrence, suppressed, parentSuppressed)) return false;
    return suppressed || parentSuppressed;
}

void CollectDirectlySuppressed(
    tag_t occurrence, std::vector<std::wstring>& names, std::set<std::wstring>& seen) {
    bool suppressed = false;
    bool parentSuppressed = false;
    if (QuerySuppressState(occurrence, suppressed, parentSuppressed) && suppressed) {
        const std::wstring name = OccurrenceFileName(occurrence);
        if (seen.insert(name).second) names.push_back(name);
    }
    tag_t* children = nullptr;
    const int count = UF_ASSEM_ask_all_part_occ_children(occurrence, &children);
    if (count > 0) {
        for (int index = 0; index < count; ++index) {
            CollectDirectlySuppressed(children[index], names, seen);
        }
    }
    UF_free(children);
}

enum class SuppressedChoice { Dissolve, Delete, Cancel };

SuppressedChoice AskSuppressedChoice(const std::vector<std::wstring>& names) {
    std::wstring content;
    const std::size_t limit = names.size() < 12 ? names.size() : 12;
    for (std::size_t index = 0; index < limit; ++index) content += names[index] + L"\n";
    if (names.size() > limit) {
        content += L"等 " + std::to_wstring(names.size()) + L" 个抑制组件\n";
    }
    content += L"\n是：解散抑制，把这些抑制组件里的叶子按原位置装配到当前工作部件。\n";
    content += L"否：删除抑制文档，不装配这些抑制组件，它们随所选装配一起去掉。零件文件仍留在磁盘上。\n";
    content += L"取消：不解散。";
    const int answer = MessageBoxW(
        nullptr, content.c_str(), L"解散组件",
        MB_YESNOCANCEL | MB_ICONWARNING | MB_DEFBUTTON3);
    if (answer == IDYES) return SuppressedChoice::Dissolve;
    if (answer == IDNO) return SuppressedChoice::Delete;
    return SuppressedChoice::Cancel;
}

bool ConfirmAssemblyBodyLoss(const std::vector<std::wstring>& names) {
    std::wstring text;
    if (names.size() == 1) {
        text = names.front() + L" 里存在实体，删除装配后这些体也会一起删除。";
    } else {
        text = L"以下装配 PRT 里存在实体，删除装配后这些体也会一起删除：\n";
        for (const std::wstring& name : names) text += name + L"\n";
    }
    text += L"\n\n要继续解散吗？";
    const int answer = MessageBoxW(
        nullptr, text.c_str(), L"解散组件",
        MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
    return answer == IDYES;
}

bool EntirePartReference(const std::string& referenceSet) {
    return referenceSet.empty() ||
        _stricmp(referenceSet.c_str(), "None") == 0 ||
        _stricmp(referenceSet.c_str(), "Entire Part") == 0;
}

std::string TrimmedName(const char* text, std::size_t limit) {
    if (text == nullptr) return std::string();
    std::string name(text);
    if (name.size() > limit) name.resize(limit);
    while (!name.empty() && static_cast<unsigned char>(name.back()) >= 0x80 &&
           (static_cast<unsigned char>(name.back()) & 0xC0) == 0x80) {
        // Keep the locale name from ending on a trailing multibyte fragment.
        bool lead = false;
        for (std::size_t index = name.size(); index > 0; --index) {
            const auto byte = static_cast<unsigned char>(name[index - 1]);
            if ((byte & 0xC0) != 0x80) {
                lead = byte >= 0x80;
                break;
            }
        }
        if (!lead) break;
        name.pop_back();
    }
    return name;
}

std::string UniqueInstanceName(
    tag_t workPart, const char* baseName, std::set<std::string>& used) {
    std::string root = TrimmedName(baseName, 100);
    if (root.empty()) root = "LEAF";
    std::string candidate = root;
    for (int suffix = 2; suffix < 10000; ++suffix) {
        if (used.count(candidate) == 0 &&
            UF_ASSEM_ask_instance_of_name(workPart, candidate.c_str()) == NULL_TAG) {
            used.insert(candidate);
            return candidate;
        }
        candidate = root + "_" + std::to_string(suffix);
        if (candidate.size() > 120) candidate.resize(120);
    }
    used.insert(candidate);
    return candidate;
}

bool ReadLeaf(
    tag_t leaf, tag_t workPart, tag_t workOccurrence, LeafPlacement& placement,
    std::wstring& error) {
    char partName[MAX_FSPEC_BUFSIZE]{};
    char refset[UF_OBJ_NAME_BUFSIZE]{};
    char instance[UF_CFI_MAX_FILE_NAME_BUFSIZE]{};
    double origin[3]{};
    double csys[9]{};
    double transform[4][4]{};
    const int code = UF_ASSEM_ask_component_data(
        leaf, partName, refset, instance, origin, csys, transform);
    if (code != 0 || partName[0] == '\0') {
        error = L"读取叶子零件失败：" + UfMessage(code);
        return false;
    }
    const tag_t prototype = UF_ASSEM_ask_prototype_of_occ(leaf);
    if (prototype != NULL_TAG &&
        UF_ASSEM_part_is_descendant(prototype, workPart)) {
        error = L"叶子零件包含当前工作部件，不能装配到自身。";
        return false;
    }
    if (!PoseInWorkPart(leaf, workOccurrence, placement.pose)) {
        error = L"读取不到叶子零件在当前工作部件中的位置。";
        return false;
    }
    placement.occurrence = leaf;
    placement.partPath = partName;
    placement.referenceSet = refset;
    placement.instanceName = instance;
    return true;
}
}

DissolveComponentDialog::DissolveComponentDialog() {
    zhihui_context_help::EnsureGlobalHelpLoaded();
    session_ = NXOpen::Session::GetSession();
    dialog_ = NXOpen::UI::GetUI()->CreateDialog(DlxPath().c_str());
    dialog_->AddInitializeHandler(
        NXOpen::make_callback(this, &DissolveComponentDialog::Initialize));
    dialog_->AddDialogShownHandler(
        NXOpen::make_callback(this, &DissolveComponentDialog::DialogShown));
    dialog_->AddUpdateHandler(
        NXOpen::make_callback(this, &DissolveComponentDialog::Update));
    dialog_->AddApplyHandler(
        NXOpen::make_callback(this, &DissolveComponentDialog::Apply));
    dialog_->AddOkHandler(
        NXOpen::make_callback(this, &DissolveComponentDialog::Ok));
    dialog_->AddFilterHandler(
        NXOpen::make_callback(this, &DissolveComponentDialog::Filter));
}

DissolveComponentDialog::~DissolveComponentDialog() {
    delete dialog_;
}

NXOpen::BlockStyler::BlockDialog::DialogResponse DissolveComponentDialog::Launch() {
    return dialog_->Launch();
}

void DissolveComponentDialog::Log(const std::string& message) const {
    try { session_->LogFile()->WriteLine(message.c_str()); } catch (...) {}
    try {
        std::string line = message;
        if (line.empty() || line.back() != '\n') line.push_back('\n');
        UF_print_syslog(line.data(), false);
    } catch (...) {}
}

void DissolveComponentDialog::Notify(const std::wstring& message, bool error) const {
    MessageBoxW(
        nullptr, message.c_str(), L"解散组件",
        MB_OK | (error ? MB_ICONERROR : MB_ICONINFORMATION));
}

void DissolveComponentDialog::Initialize() {
    try {
        selection_ = dynamic_cast<NXOpen::BlockStyler::SelectObject*>(
            dialog_->TopBlock()->FindBlock("component_selection"));
        if (selection_ == nullptr) {
            throw std::runtime_error("JieSanZuJian dialog block missing.");
        }
        ConfigureSelection();
    } catch (const NXOpen::NXException& ex) {
        Log(ex.Message() ? ex.Message() : "JieSanZuJian initialize NX exception.");
    } catch (const std::exception& ex) {
        Log(ex.what());
    } catch (...) {
        Log("JieSanZuJian initialize failed.");
    }
}

void DissolveComponentDialog::ConfigureSelection() {
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

void DissolveComponentDialog::DialogShown() {
    try {
        if (selection_ != nullptr) selection_->Focus();
    } catch (const NXOpen::NXException& ex) {
        Log(ex.Message() ? ex.Message() : "JieSanZuJian dialogShown NX exception.");
    } catch (const std::exception& ex) {
        Log(ex.what());
    } catch (...) {
        Log("JieSanZuJian dialogShown failed.");
    }
}

int DissolveComponentDialog::Update(NXOpen::BlockStyler::UIBlock* block) {
    try {
        if (block != selection_ || expandingSelection_) return 0;
        ExpandToSiblingInstances();
    } catch (const NXOpen::NXException& ex) {
        Log(ex.Message() ? ex.Message() : "JieSanZuJian update NX exception.");
    } catch (const std::exception& ex) {
        Log(ex.what());
    } catch (...) {
        Log("JieSanZuJian update failed.");
    }
    return 0;
}

void DissolveComponentDialog::ExpandToSiblingInstances() {
    if (selection_ == nullptr || session_ == nullptr) return;
    std::vector<tag_t> seeds;
    for (NXOpen::TaggedObject* object : selection_->GetSelectedObjects()) {
        if (object == nullptr || object->Tag() == NULL_TAG) continue;
        if (dynamic_cast<NXOpen::Assemblies::Component*>(object) == nullptr) continue;
        if (ChildCount(object->Tag()) <= 0) continue;
        if (!ContainsTag(seeds, object->Tag())) seeds.push_back(object->Tag());
    }
    if (seeds.empty()) return;

    NXOpen::Part* display = session_->Parts()->Display();
    if (display == nullptr) return;
    const tag_t scopeRoot = UF_ASSEM_ask_root_part_occ(display->Tag());
    std::vector<tag_t> instances;
    for (tag_t seed : seeds) {
        std::vector<tag_t> found;
        std::wstring error;
        if (!CollectGroupInstances(seed, found, error)) continue;
        for (tag_t instance : found) {
            if (!ContainsTag(instances, instance)) instances.push_back(instance);
        }
    }
    std::vector<tag_t> groups;
    std::wstring error;
    if (!CollectGroupOccurrences(instances, seeds.front(), scopeRoot, groups, error)) return;
    bool same = groups.size() == seeds.size();
    for (tag_t tag : groups) {
        if (!ContainsTag(seeds, tag)) same = false;
    }
    std::ostringstream line;
    line << "JieSanZuJian: selection seeds=" << seeds.size()
         << " instances=" << instances.size()
         << " groups=" << groups.size();
    Log(line.str());
    if (same || groups.size() < 2) return;

    std::vector<NXOpen::TaggedObject*> objects;
    for (tag_t tag : groups) {
        NXOpen::TaggedObject* object = NXOpen::NXObjectManager::Get(tag);
        if (dynamic_cast<NXOpen::Assemblies::Component*>(object) == nullptr) continue;
        objects.push_back(object);
    }
    if (objects.size() < 2) return;
    expandingSelection_ = true;
    try {
        selection_->SetSelectedObjects(objects);
    } catch (...) {
        expandingSelection_ = false;
        throw;
    }
    expandingSelection_ = false;
}

int DissolveComponentDialog::Filter(
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

int DissolveComponentDialog::Apply() {
    return 0;
}

int DissolveComponentDialog::Ok() {
    try {
        std::wstring message;
        if (!Dissolve(message)) {
            if (!message.empty()) Notify(message, true);
            return 1;
        }
        if (!message.empty()) Notify(message, false);
        return 0;
    } catch (const NXOpen::NXException& ex) {
        Log(ex.Message() ? ex.Message() : "JieSanZuJian ok NX exception.");
        Notify(L"解散组件失败，模型已恢复到执行前。", true);
        return 1;
    } catch (const std::exception& ex) {
        Log(ex.what());
        Notify(L"解散组件失败，模型已恢复到执行前。", true);
        return 1;
    } catch (...) {
        Log("JieSanZuJian ok failed.");
        Notify(L"解散组件失败，模型已恢复到执行前。", true);
        return 1;
    }
}

bool DissolveComponentDialog::Dissolve(std::wstring& message) {
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
    const tag_t workPart = work->Tag();
    tag_t workOccurrence = NULL_TAG;
    if (NXOpen::Assemblies::Component* workComponent = session_->Parts()->WorkComponent()) {
        workOccurrence = workComponent->Tag();
    }
    if (workPart != display->Tag() && workOccurrence == NULL_TAG) {
        message = L"请把要接收零件的装配设为工作部件。";
        return false;
    }

    std::vector<tag_t> seeds;
    for (NXOpen::TaggedObject* object : selection_->GetSelectedObjects()) {
        if (object == nullptr || object->Tag() == NULL_TAG) continue;
        if (dynamic_cast<NXOpen::Assemblies::Component*>(object) == nullptr) continue;
        if (ChildCount(object->Tag()) <= 0) continue;
        if (!ContainsTag(seeds, object->Tag())) seeds.push_back(object->Tag());
    }
    if (seeds.empty()) {
        message = L"请先选择一个带下级零件的装配组件。";
        return false;
    }

    std::vector<tag_t> instances;
    for (tag_t seed : seeds) {
        std::vector<tag_t> found;
        if (!CollectGroupInstances(seed, found, message)) return false;
        for (tag_t instance : found) {
            if (!ContainsTag(instances, instance)) instances.push_back(instance);
        }
    }
    const tag_t scopeRoot = UF_ASSEM_ask_root_part_occ(display->Tag());
    std::vector<tag_t> groups;
    if (!CollectGroupOccurrences(instances, seeds.front(), scopeRoot, groups, message)) {
        return false;
    }
    {
        std::ostringstream line;
        line << "JieSanZuJian: sibling instances=" << instances.size()
             << " groups=" << groups.size();
        Log(line.str());
    }

    std::vector<tag_t> leaves;
    for (tag_t group : groups) {
        if (!CollectLeaves(group, leaves)) {
            message = L"读取下级组件失败。";
            return false;
        }
    }
    if (leaves.empty()) {
        message = L"所选组件里没有叶子零件。";
        return false;
    }

    std::vector<std::wstring> suppressedNames;
    std::set<std::wstring> seenSuppressed;
    for (tag_t group : groups) {
        CollectDirectlySuppressed(group, suppressedNames, seenSuppressed);
    }
    bool deleteSuppressed = false;
    if (!suppressedNames.empty()) {
        std::ostringstream line;
        line << "JieSanZuJian: suppressed parts=" << suppressedNames.size();
        Log(line.str());
        const SuppressedChoice choice = AskSuppressedChoice(suppressedNames);
        if (choice == SuppressedChoice::Cancel) {
            message.clear();
            return false;
        }
        deleteSuppressed = choice == SuppressedChoice::Delete;
        if (deleteSuppressed) {
            std::vector<tag_t> kept;
            for (tag_t leaf : leaves) {
                if (!EffectivelySuppressed(leaf)) kept.push_back(leaf);
            }
            leaves.swap(kept);
        }
    }

    std::vector<LeafPlacement> placements;
    placements.reserve(leaves.size());
    for (tag_t leaf : leaves) {
        if (EffectivelySuppressed(leaf)) {
            const tag_t leafInstance = UF_ASSEM_ask_inst_of_part_occ(leaf);
            if (leafInstance != NULL_TAG) {
                UF_PART_load_status_t loadStatus{};
                UF_ASSEM_ensure_child_loaded(leafInstance, &loadStatus);
                UF_PART_free_load_status(&loadStatus);
            }
        }
        LeafPlacement placement;
        if (!ReadLeaf(leaf, workPart, workOccurrence, placement, message)) return false;
        placements.push_back(std::move(placement));
    }

    std::vector<std::wstring> bodyParts;
    std::set<tag_t> seenPrototypes;
    for (tag_t group : groups) {
        if (!CollectAssemblyBodyParts(group, bodyParts, seenPrototypes)) {
            message = L"读取装配零件中的实体失败。";
            return false;
        }
    }
    if (!bodyParts.empty()) {
        std::ostringstream line;
        line << "JieSanZuJian: assembly parts with bodies=" << bodyParts.size();
        Log(line.str());
        if (!ConfirmAssemblyBodyLoss(bodyParts)) {
            message.clear();
            return false;
        }
    }

    const NXOpen::Session::UndoMarkId mark = session_->SetUndoMark(
        NXOpen::Session::MarkVisibilityVisible,
        NXOpen::NXString("解散组件", NXOpen::NXString::UTF8));
    const NXOpen::NXString undoName("解散组件", NXOpen::NXString::UTF8);
    auto undo = [&]() {
        try {
            if (session_->DoesUndoMarkExist(mark, undoName)) {
                session_->UndoToMark(mark, undoName);
            }
        } catch (...) {}
    };

    try {
        std::set<std::string> usedNames;
        int added = 0;
        for (std::size_t index = 0; index < placements.size(); ++index) {
            const LeafPlacement& placement = placements[index];
            const std::string instanceName = UniqueInstanceName(
                workPart, placement.instanceName.c_str(), usedNames);
            const char* referenceSet = EntirePartReference(placement.referenceSet)
                ? nullptr : placement.referenceSet.c_str();
            tag_t newInstance = NULL_TAG;
            UF_PART_load_status_t loadStatus{};
            const int code = UF_ASSEM_add_part_to_assembly2(
                workPart,
                placement.partPath.c_str(),
                referenceSet,
                instanceName.c_str(),
                const_cast<double*>(placement.pose.origin),
                const_cast<double*>(placement.pose.csys),
                -1,
                &newInstance,
                &loadStatus);
            UF_PART_free_load_status(&loadStatus);
            std::ostringstream line;
            line << "JieSanZuJian: add index=" << (index + 1)
                 << " code=" << code
                 << " instance=" << newInstance
                 << " origin=" << placement.pose.origin[0]
                 << "," << placement.pose.origin[1]
                 << "," << placement.pose.origin[2];
            Log(line.str());
            if (code != 0 || newInstance == NULL_TAG) {
                undo();
                message = L"装配第 " + std::to_wstring(index + 1) +
                    L" 个叶子零件失败：" + UfMessage(code);
                return false;
            }
            ++added;
        }

        std::vector<tag_t> removeInstances;
        for (tag_t group : groups) {
            const tag_t groupInstance = UF_ASSEM_ask_inst_of_part_occ(group);
            if (groupInstance != NULL_TAG && !ContainsTag(removeInstances, groupInstance)) {
                removeInstances.push_back(groupInstance);
            }
        }
        for (std::size_t index = 0; index < removeInstances.size(); ++index) {
            const int removeCode = UF_ASSEM_remove_instance(removeInstances[index]);
            if (removeCode != 0) {
                undo();
                message = L"叶子零件已准备装配，但删除第 " +
                    std::to_wstring(index + 1) +
                    L" 个装配实例失败，模型已恢复：" + UfMessage(removeCode);
                return false;
            }
        }
        UF_DISP_regenerate_display();
        try { selection_->SetSelectedObjects({}); } catch (...) {}

        std::ostringstream summary;
        summary << "JieSanZuJian: leaves=" << placements.size()
                << " added=" << added
                << " removedInstances=" << removeInstances.size();
        Log(summary.str());
        if (deleteSuppressed && added == 0) {
            message = L"已删除 " + std::to_wstring(removeInstances.size()) +
                L" 个装配实例。抑制组件没有装配到工作部件。\n"
                L"零件文件仍留在磁盘上。";
        } else {
            message = L"已把 " + std::to_wstring(added) +
                L" 个叶子零件按原位置装配到当前工作部件，并删除 " +
                std::to_wstring(removeInstances.size()) +
                L" 个装配实例。\n"
                L"叶子零件文件仍是原来的文件，装配组件的零件文件也留在磁盘上。";
            if (deleteSuppressed) {
                message += L"\n抑制组件没有装配到工作部件，已随所选装配去掉。";
            }
        }
        return true;
    } catch (...) {
        undo();
        message = L"解散组件失败，模型已恢复到执行前。";
        throw;
    }
}
