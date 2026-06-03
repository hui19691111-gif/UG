#include "AutoCreateThreeViews.hpp"

#include <NXOpen/NXException.hxx>
#include <NXOpen/NXMessageBox.hxx>
#include <NXOpen/Selection.hxx>
#include <NXOpen/UI.hxx>

#include <uf.h>
#include <uf_assem.h>
#include <uf_defs.h>
#include <uf_disp.h>
#include <uf_obj.h>
#include <uf_part.h>
#include <uf_view.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>

#include "../../protection/native/ZhihuiLicenseGuard.hpp"

#ifndef DllExport
#define DllExport __declspec(dllexport)
#endif

namespace
{
struct UiMonitorState
{
    std::filesystem::path requestPath;
    std::filesystem::path displayCommandPath;
    PROCESS_INFORMATION processInfo = {};
    UINT_PTR timerId = 0;
    bool active = false;
    bool processing = false;
};

UiMonitorState g_uiMonitor;
std::vector<tag_t> g_assemblyOccurrenceTags;
std::set<tag_t> g_knownHighlightedOccurrences;
tag_t g_highlightedOccurrence = NULL_TAG;

struct SelectedDrawingPart
{
    int index = 0;
    tag_t occurrence = NULL_TAG;
    tag_t prototypePart = NULL_TAG;
};

std::filesystem::path CurrentModuleDirectory()
{
    HMODULE module = nullptr;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(&CurrentModuleDirectory),
                           &module))
    {
        wchar_t moduleFileName[MAX_PATH] = {0};
        const DWORD length = GetModuleFileNameW(module, moduleFileName, MAX_PATH);
        if (length > 0 && length < MAX_PATH)
        {
            return std::filesystem::path(moduleFileName).parent_path();
        }
    }

    return std::filesystem::path(__FILE__).parent_path();
}

std::wstring QuoteArgument(const std::filesystem::path& path)
{
    std::wstring value = path.wstring();
    std::wstring quoted = L"\"";
    for (wchar_t ch : value)
    {
        if (ch == L'"')
        {
            quoted += L"\\\"";
        }
        else
        {
            quoted += ch;
        }
    }
    quoted += L"\"";
    return quoted;
}

void WriteLauncherLog(const std::string& message)
{
    try
    {
        std::ofstream log(CurrentModuleDirectory() / "AutoCreateThreeViews.log", std::ios::app);
        if (log)
        {
            log << message << '\n';
        }
    }
    catch (...)
    {
    }
}

std::string TrimText(const std::string& value)
{
    const char* whitespace = " \t\r\n";
    const size_t first = value.find_first_not_of(whitespace);
    if (first == std::string::npos)
    {
        return "";
    }

    const size_t last = value.find_last_not_of(whitespace);
    return value.substr(first, last - first + 1);
}

std::string EscapeManifestValue(const std::string& value)
{
    std::ostringstream escaped;
    escaped << std::hex;
    for (unsigned char ch : value)
    {
        if (ch == '%' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '=')
        {
            escaped << '%' << std::uppercase;
            escaped.width(2);
            escaped.fill('0');
            escaped << static_cast<int>(ch);
            escaped << std::nouppercase;
        }
        else
        {
            escaped << static_cast<char>(ch);
        }
    }
    return escaped.str();
}

std::string BaseNameFromPathText(const std::string& pathText)
{
    try
    {
        std::filesystem::path path(pathText);
        std::string stem = path.stem().u8string();
        if (!stem.empty())
        {
            return stem;
        }
        std::string filename = path.filename().u8string();
        if (!filename.empty())
        {
            return filename;
        }
    }
    catch (...)
    {
    }

    const size_t slash = pathText.find_last_of("\\/");
    return slash == std::string::npos ? pathText : pathText.substr(slash + 1);
}

std::string PartNameFromPartTag(tag_t part)
{
    if (part == NULL_TAG)
    {
        return "";
    }

    char partName[MAX_FSPEC_BUFSIZE] = {0};
    if (UF_PART_ask_part_name(part, partName) != 0)
    {
        return "";
    }

    try
    {
        return std::filesystem::path(partName).u8string();
    }
    catch (...)
    {
        return partName;
    }
}

std::string PartNameFromOccurrence(tag_t occurrence)
{
    tag_t prototype = occurrence == NULL_TAG ? NULL_TAG : UF_ASSEM_ask_prototype_of_occ(occurrence);
    std::string partName = PartNameFromPartTag(prototype);
    if (!partName.empty())
    {
        return partName;
    }

    tag_t instance = occurrence == NULL_TAG ? NULL_TAG : UF_ASSEM_ask_inst_of_part_occ(occurrence);
    if (instance != NULL_TAG)
    {
        char childName[MAX_FSPEC_BUFSIZE] = {0};
        if (UF_ASSEM_ask_part_name_of_child(instance, childName) == 0)
        {
            try
            {
                return std::filesystem::path(childName).u8string();
            }
            catch (...)
            {
                return childName;
            }
        }
    }

    return "";
}

std::string DisplayNameFromOccurrence(tag_t occurrence, const std::string& partName)
{
    char objectName[UF_OBJ_NAME_BUFSIZE] = {0};
    if (occurrence != NULL_TAG && UF_OBJ_ask_name(occurrence, objectName) == 0)
    {
        std::string name = TrimText(objectName);
        if (!name.empty())
        {
            return name;
        }
    }

    std::string baseName = BaseNameFromPathText(partName);
    if (!baseName.empty())
    {
        return baseName;
    }

    if (occurrence != NULL_TAG)
    {
        std::ostringstream fallback;
        fallback << "Component " << static_cast<unsigned long long>(occurrence);
        return fallback.str();
    }

    return "Work Part";
}

void WriteAssemblyManifestLine(
    std::ofstream& output,
    tag_t occurrence,
    tag_t parentOccurrence,
    int depth,
    const std::string& displayName,
    const std::string& partName)
{
    output << "id=" << static_cast<unsigned long long>(occurrence)
           << "\tparent=" << static_cast<unsigned long long>(parentOccurrence)
           << "\tdepth=" << depth
           << "\tname=" << EscapeManifestValue(displayName)
           << "\tpart=" << EscapeManifestValue(partName)
           << '\n';

    if (occurrence != NULL_TAG)
    {
        g_assemblyOccurrenceTags.push_back(occurrence);
    }
}

void WriteAssemblyChildren(std::ofstream& output, tag_t parentOccurrence, int depth)
{
    tag_t* children = nullptr;
    const int childCount = UF_ASSEM_ask_part_occ_children(parentOccurrence, &children);
    if (childCount <= 0 || children == nullptr)
    {
        return;
    }

    for (int index = 0; index < childCount; ++index)
    {
        const tag_t child = children[index];
        const std::string partName = PartNameFromOccurrence(child);
        const std::string displayName = DisplayNameFromOccurrence(child, partName);
        WriteAssemblyManifestLine(output, child, parentOccurrence, depth, displayName, partName);
        WriteAssemblyChildren(output, child, depth + 1);
    }

    UF_free(children);
}

void UnhighlightOccurrenceTree(tag_t occurrence, int& clearedCount)
{
    if (occurrence == NULL_TAG)
    {
        return;
    }

    UF_DISP_set_highlight(occurrence, 0);
    ++clearedCount;

    tag_t* children = nullptr;
    const int childCount = UF_ASSEM_ask_part_occ_children(occurrence, &children);
    if (childCount > 0 && children != nullptr)
    {
        for (int index = 0; index < childCount; ++index)
        {
            UnhighlightOccurrenceTree(children[index], clearedCount);
        }
    }

    if (children != nullptr)
    {
        UF_free(children);
    }
}

void UnhighlightKnownOccurrences(int& clearedCount)
{
    if (g_highlightedOccurrence != NULL_TAG)
    {
        UF_DISP_set_highlight(g_highlightedOccurrence, 0);
        ++clearedCount;
    }

    for (tag_t occurrence : g_knownHighlightedOccurrences)
    {
        if (occurrence != NULL_TAG && occurrence != g_highlightedOccurrence)
        {
            UF_DISP_set_highlight(occurrence, 0);
            ++clearedCount;
        }
    }

    for (tag_t occurrence : g_assemblyOccurrenceTags)
    {
        if (occurrence != NULL_TAG &&
            occurrence != g_highlightedOccurrence &&
            g_knownHighlightedOccurrences.find(occurrence) == g_knownHighlightedOccurrences.end())
        {
            UF_DISP_set_highlight(occurrence, 0);
            ++clearedCount;
        }
    }

    g_knownHighlightedOccurrences.clear();
}

void UnhighlightDisplayedAssemblyTree(int& clearedCount)
{
    const tag_t displayPart = UF_PART_ask_display_part();
    if (displayPart == NULL_TAG)
    {
        return;
    }

    const tag_t rootOccurrence = UF_ASSEM_ask_root_part_occ(displayPart);
    UnhighlightOccurrenceTree(rootOccurrence, clearedCount);
}

bool WriteAssemblyManifest(const std::filesystem::path& manifestPath)
{
    std::error_code ignored;
    std::filesystem::create_directories(manifestPath.parent_path(), ignored);
    int clearedCount = 0;
    UnhighlightKnownOccurrences(clearedCount);
    UnhighlightDisplayedAssemblyTree(clearedCount);
    if (clearedCount > 0)
    {
        UF_DISP_make_display_up_to_date();
    }
    g_assemblyOccurrenceTags.clear();
    g_knownHighlightedOccurrences.clear();
    g_highlightedOccurrence = NULL_TAG;

    std::ofstream output(manifestPath, std::ios::binary);
    if (!output)
    {
        WriteLauncherLog("AutoCreateThreeViews: cannot create assembly manifest=" + manifestPath.string() + ".");
        return false;
    }

    const tag_t displayPart = UF_PART_ask_display_part();
    if (displayPart == NULL_TAG)
    {
        return false;
    }

    const std::string rootPartName = PartNameFromPartTag(displayPart);
    const std::string rootDisplayName = DisplayNameFromOccurrence(NULL_TAG, rootPartName);
    const tag_t rootOccurrence = UF_ASSEM_ask_root_part_occ(displayPart);
    WriteAssemblyManifestLine(output, rootOccurrence, NULL_TAG, 0, rootDisplayName, rootPartName);
    if (rootOccurrence != NULL_TAG)
    {
        WriteAssemblyChildren(output, rootOccurrence, 1);
    }

    return true;
}

std::map<std::string, std::string> ReadSimpleKeyValueFile(const std::filesystem::path& path)
{
    std::ifstream input(path);
    std::map<std::string, std::string> values;
    std::string line;
    while (std::getline(input, line))
    {
        const size_t equal = line.find('=');
        if (equal == std::string::npos)
        {
            continue;
        }
        values[TrimText(line.substr(0, equal))] = TrimText(line.substr(equal + 1));
    }
    return values;
}

bool ParseTagValue(const std::string& text, tag_t& tag)
{
    try
    {
        const unsigned long long value = std::stoull(TrimText(text));
        tag = static_cast<tag_t>(value);
        return true;
    }
    catch (...)
    {
        tag = NULL_TAG;
        return false;
    }
}

int ParseIntValue(const std::string& text, int fallback = 0)
{
    try
    {
        return std::stoi(TrimText(text));
    }
    catch (...)
    {
        return fallback;
    }
}

double ParseDoubleValue(const std::string& text, double fallback = 1.0)
{
    try
    {
        return std::stod(TrimText(text));
    }
    catch (...)
    {
        return fallback;
    }
}

std::string FindValue(
    const std::map<std::string, std::string>& values,
    const std::string& key,
    const std::string& fallback = "")
{
    const auto found = values.find(key);
    return found == values.end() ? fallback : found->second;
}

std::string FindPartValue(
    const std::map<std::string, std::string>& values,
    int index,
    const std::string& key,
    const std::string& fallback = "")
{
    std::ostringstream prefixedKey;
    prefixedKey << "part" << index << "." << key;
    const auto found = values.find(prefixedKey.str());
    if (found != values.end())
    {
        return found->second;
    }

    return FindValue(values, key, fallback);
}

void WriteSinglePartRequest(
    const std::filesystem::path& requestPath,
    const std::map<std::string, std::string>& values,
    int index)
{
    static const std::vector<std::string> settingKeys = {
        "templatePath",
        "projection",
        "frontDirectionMode",
        "viewSpacing",
        "sheetMargin",
        "front",
        "top",
        "bottom",
        "left",
        "right",
        "back",
        "backBottom",
        "iso",
        "flat",
        "auxAutoCompact",
        "isoCorner",
        "flatCorner",
        "autoDimensions",
        "dimensionOverall",
        "dimensionLinear",
        "dimensionAngle",
        "dimensionHole",
        "dimensionHoleLocation",
        "dimensionInnerClosedCurve",
        "technicalRequirementEnabled",
        "technicalRequirementIndexed",
        "technicalRequirementCorner",
        "technicalRequirementText"};

    std::ofstream output(requestPath, std::ios::binary);
    output << "action=create\n";
    output << "selectedOccurrenceTag=" << FindPartValue(values, index, "occurrenceTag") << '\n';
    for (const std::string& key : settingKeys)
    {
        output << key << '=' << FindPartValue(values, index, key, FindValue(values, key)) << '\n';
    }
}

void DisplaySelectedOccurrence(tag_t occurrence)
{
    if (occurrence == NULL_TAG)
    {
        return;
    }

    int clearedCount = 0;
    if (g_highlightedOccurrence != NULL_TAG && g_highlightedOccurrence != occurrence)
    {
        UF_DISP_set_highlight(g_highlightedOccurrence, 0);
        ++clearedCount;
    }

    for (tag_t knownOccurrence : g_assemblyOccurrenceTags)
    {
        if (knownOccurrence != NULL_TAG && knownOccurrence != occurrence)
        {
            UF_DISP_set_highlight(knownOccurrence, 0);
            ++clearedCount;
        }
    }

    if (std::find(g_assemblyOccurrenceTags.begin(), g_assemblyOccurrenceTags.end(), occurrence) == g_assemblyOccurrenceTags.end())
    {
        g_assemblyOccurrenceTags.push_back(occurrence);
    }

    const int highlightStatus = UF_DISP_set_highlight(occurrence, 1);
    UF_DISP_make_display_up_to_date();
    g_highlightedOccurrence = occurrence;
    g_knownHighlightedOccurrences.insert(occurrence);

    std::ostringstream log;
    log << "AutoCreateThreeViews: display assembly occurrence tag="
        << static_cast<unsigned long long>(occurrence)
        << ", cleared=" << clearedCount
        << ", highlightStatus=" << highlightStatus
        << ", highlighted without changing work part.";
    WriteLauncherLog(log.str());
}

void ClearSelectionAndOccurrenceHighlights(bool ufAlreadyInitialized)
{
    bool initializedHere = false;
    if (!ufAlreadyInitialized)
    {
        const int initStatus = UF_initialize();
        if (initStatus != 0)
        {
            std::ostringstream log;
            log << "AutoCreateThreeViews: clear selection skipped; UF_initialize failed, status=" << initStatus << ".";
            WriteLauncherLog(log.str());
            return;
        }
        initializedHere = true;
    }

    int clearedHighlights = 0;
    UnhighlightKnownOccurrences(clearedHighlights);
    UnhighlightDisplayedAssemblyTree(clearedHighlights);
    g_highlightedOccurrence = NULL_TAG;
    g_assemblyOccurrenceTags.clear();
    g_knownHighlightedOccurrences.clear();

    bool selectionCleared = false;
    try
    {
        NXOpen::UI* ui = NXOpen::UI::GetUI();
        NXOpen::Selection* selection = ui != nullptr ? ui->SelectionManager() : nullptr;
        if (selection != nullptr)
        {
            selection->ClearGlobalSelectionList();
            selectionCleared = true;
        }
    }
    catch (const NXOpen::NXException& ex)
    {
        WriteLauncherLog(std::string("AutoCreateThreeViews: clear global selection failed, NXException: ") + ex.Message() + ".");
    }
    catch (...)
    {
        WriteLauncherLog("AutoCreateThreeViews: clear global selection failed, unknown exception.");
    }

    UF_DISP_make_display_up_to_date();

    std::ostringstream log;
    log << "AutoCreateThreeViews: exit cleanup cleared selection/highlights"
        << " selectionCleared=" << (selectionCleared ? "yes" : "no")
        << ", highlights=" << clearedHighlights << ".";
    WriteLauncherLog(log.str());

    if (initializedHere)
    {
        UF_terminate();
    }
}

bool SetDrawingWorkOccurrence(tag_t occurrence)
{
    if (occurrence == NULL_TAG)
    {
        return false;
    }

    const int status = UF_ASSEM_set_work_occurrence(occurrence);
    std::ostringstream log;
    log << "AutoCreateThreeViews: set drawing work occurrence tag="
        << static_cast<unsigned long long>(occurrence)
        << ", status=" << status << ".";
    WriteLauncherLog(log.str());
    return status == 0;
}

tag_t PrototypePartFromOccurrence(tag_t occurrence)
{
    if (occurrence == NULL_TAG)
    {
        return NULL_TAG;
    }

    tag_t prototype = UF_ASSEM_ask_prototype_of_occ(occurrence);
    if (prototype != NULL_TAG)
    {
        return prototype;
    }

    const tag_t displayPart = UF_PART_ask_display_part();
    if (displayPart != NULL_TAG && occurrence == UF_ASSEM_ask_root_part_occ(displayPart))
    {
        return displayPart;
    }

    return NULL_TAG;
}

bool OccurrenceHasChildren(tag_t occurrence)
{
    if (occurrence == NULL_TAG)
    {
        return false;
    }

    tag_t* children = nullptr;
    const int childCount = UF_ASSEM_ask_part_occ_children(occurrence, &children);
    if (children != nullptr)
    {
        UF_free(children);
    }

    return childCount > 0;
}

bool IsRootOccurrence(tag_t occurrence, tag_t displayPart)
{
    if (occurrence == NULL_TAG || displayPart == NULL_TAG)
    {
        return false;
    }

    return occurrence == UF_ASSEM_ask_root_part_occ(displayPart);
}

bool SetDrawingDisplayPart(tag_t partTag, tag_t sourceOccurrence)
{
    if (partTag == NULL_TAG)
    {
        WriteLauncherLog("AutoCreateThreeViews: selected drawing part is NULL; skip.");
        return false;
    }

    const int displayStatus = UF_PART_set_display_part(partTag);
    int workStatus = 0;
    if (displayStatus == 0)
    {
        workStatus = UF_ASSEM_set_work_part(partTag);
    }

    char partName[MAX_FSPEC_BUFSIZE] = {0};
    UF_PART_ask_part_name(partTag, partName);

    std::ostringstream log;
    log << "AutoCreateThreeViews: set selected drawing part, occurrence="
        << static_cast<unsigned long long>(sourceOccurrence)
        << ", part=" << static_cast<unsigned long long>(partTag)
        << ", displayStatus=" << displayStatus
        << ", workStatus=" << workStatus
        << ", name=" << partName << ".";
    WriteLauncherLog(log.str());
    return displayStatus == 0;
}

void RestoreDrawingContext(tag_t previousDisplayPart, tag_t previousWorkOccurrence, tag_t previousWorkPart)
{
    int status = 0;
    if (previousDisplayPart != NULL_TAG)
    {
        status = UF_PART_set_display_part(previousDisplayPart);
    }

    int workStatus = 0;
    if (previousWorkOccurrence != NULL_TAG)
    {
        workStatus = UF_ASSEM_set_work_occurrence(previousWorkOccurrence);
    }
    else if (previousWorkPart != NULL_TAG)
    {
        workStatus = UF_ASSEM_set_work_part(previousWorkPart);
    }

    std::ostringstream log;
    log << "AutoCreateThreeViews: restore drawing context, previousDisplay="
        << static_cast<unsigned long long>(previousDisplayPart)
        << ", previousOccurrence="
        << static_cast<unsigned long long>(previousWorkOccurrence)
        << ", previousPart=" << static_cast<unsigned long long>(previousWorkPart)
        << ", displayStatus=" << status
        << ", workStatus=" << workStatus << ".";
    WriteLauncherLog(log.str());
}

void ProcessUiDisplayCommand(const std::filesystem::path& displayCommandPath)
{
    if (!std::filesystem::exists(displayCommandPath))
    {
        return;
    }

    const std::map<std::string, std::string> values = ReadSimpleKeyValueFile(displayCommandPath);
    std::error_code ignored;
    std::filesystem::remove(displayCommandPath, ignored);

    const auto action = values.find("action");
    const auto occurrence = values.find("occurrenceTag");
    if (action == values.end())
    {
        return;
    }

    if (action->second == "clear")
    {
        ClearSelectionAndOccurrenceHighlights(true);
        return;
    }

    if (occurrence != values.end())
    {
        tag_t occurrenceTag = NULL_TAG;
        if (ParseTagValue(occurrence->second, occurrenceTag))
        {
            DisplaySelectedOccurrence(occurrenceTag);
        }
    }

    if (action->second == "fit")
    {
        uc6432("", 1);
        UF_DISP_make_display_up_to_date();
        return;
    }

    if (action->second == "zoom")
    {
        const double factor = ParseDoubleValue(FindValue(values, "value"), 1.0);
        if (factor > 0.0)
        {
            double center[3] = {0.0, 0.0, 0.0};
            double scale = 0.0;
            if (uc6430("", center, &scale) == 0 && scale > 0.0)
            {
                uc6431("", center, scale * factor);
                UF_DISP_make_display_up_to_date();
            }
        }
    }
}

void ApplySelectedOccurrenceFromRequest(const std::filesystem::path& requestPath)
{
    const std::map<std::string, std::string> values = ReadSimpleKeyValueFile(requestPath);
    const auto occurrence = values.find("selectedOccurrenceTag");
    if (occurrence == values.end())
    {
        return;
    }

    tag_t occurrenceTag = NULL_TAG;
    if (ParseTagValue(occurrence->second, occurrenceTag))
    {
        DisplaySelectedOccurrence(occurrenceTag);
        SetDrawingDisplayPart(PrototypePartFromOccurrence(occurrenceTag), occurrenceTag);
    }
}

void ExecuteUiRequestParts(const std::filesystem::path& requestPath)
{
    BeginAutoCreateThreeViewsRunResults();

    const std::map<std::string, std::string> values = ReadSimpleKeyValueFile(requestPath);
    const int partCount = ParseIntValue(FindValue(values, "selectedPartCount"), 0);
    const tag_t previousDisplayPart = UF_PART_ask_display_part();
    const tag_t previousWorkOccurrence = UF_ASSEM_ask_work_occurrence();
    const tag_t previousWorkPart = UF_ASSEM_ask_work_part();

    if (partCount <= 0)
    {
        ApplySelectedOccurrenceFromRequest(requestPath);
        ExecuteAutoCreateThreeViewsFromRequest(requestPath);
        RestoreDrawingContext(previousDisplayPart, previousWorkOccurrence, previousWorkPart);
        return;
    }

    std::vector<SelectedDrawingPart> drawingParts;
    for (int index = 0; index < partCount; ++index)
    {
        tag_t occurrenceTag = NULL_TAG;
        if (ParseTagValue(FindPartValue(values, index, "occurrenceTag"), occurrenceTag))
        {
            const bool isRootOccurrence = IsRootOccurrence(occurrenceTag, previousDisplayPart);
            const bool hasChildren = OccurrenceHasChildren(occurrenceTag);
            if (isRootOccurrence || hasChildren)
            {
                std::ostringstream log;
                log << "AutoCreateThreeViews: selected occurrence is assembly/root; skip drawing index="
                    << index
                    << ", occurrence=" << static_cast<unsigned long long>(occurrenceTag)
                    << ", isRoot=" << (isRootOccurrence ? "yes" : "no")
                    << ", hasChildren=" << (hasChildren ? "yes" : "no") << ".";
                WriteLauncherLog(log.str());
                AddAutoCreateThreeViewsRunResultLine(u8"跳过：装配体或根节点不生成工程图。");
                continue;
            }

            const tag_t prototypePart = PrototypePartFromOccurrence(occurrenceTag);
            if (prototypePart == NULL_TAG)
            {
                std::ostringstream log;
                log << "AutoCreateThreeViews: selected occurrence has no prototype part, skip index="
                    << index
                    << ", occurrence=" << static_cast<unsigned long long>(occurrenceTag) << ".";
                WriteLauncherLog(log.str());
                AddAutoCreateThreeViewsRunResultLine(u8"跳过：选中的组件没有对应的原型零件。");
                continue;
            }

            drawingParts.push_back({index, occurrenceTag, prototypePart});
        }
    }

    if (drawingParts.empty())
    {
        AddAutoCreateThreeViewsRunResultLine(u8"失败：没有可出图的选中零件。");
    }

    for (const SelectedDrawingPart& selected : drawingParts)
    {
        DisplaySelectedOccurrence(selected.occurrence);
        if (!SetDrawingDisplayPart(selected.prototypePart, selected.occurrence))
        {
            AddAutoCreateThreeViewsRunResultLine(u8"失败：无法切换到选中的零件。");
            continue;
        }

        std::filesystem::path partRequestPath = requestPath;
        partRequestPath += ".part";
        partRequestPath += std::to_string(selected.index);
        partRequestPath += ".request";
        WriteSinglePartRequest(partRequestPath, values, selected.index);
        ExecuteAutoCreateThreeViewsFromRequest(partRequestPath);

        std::error_code ignored;
        std::filesystem::remove(partRequestPath, ignored);
    }

    RestoreDrawingContext(previousDisplayPart, previousWorkOccurrence, previousWorkPart);
}

void StopUiMonitor()
{
    if (g_uiMonitor.timerId != 0)
    {
        KillTimer(nullptr, g_uiMonitor.timerId);
        g_uiMonitor.timerId = 0;
    }

    if (g_uiMonitor.processInfo.hThread != nullptr)
    {
        CloseHandle(g_uiMonitor.processInfo.hThread);
        g_uiMonitor.processInfo.hThread = nullptr;
    }

    if (g_uiMonitor.processInfo.hProcess != nullptr)
    {
        CloseHandle(g_uiMonitor.processInfo.hProcess);
        g_uiMonitor.processInfo.hProcess = nullptr;
    }

    g_uiMonitor.requestPath.clear();
    g_uiMonitor.displayCommandPath.clear();
    g_uiMonitor.processing = false;
    g_uiMonitor.active = false;
}

void CALLBACK UiMonitorTimerProc(HWND, UINT, UINT_PTR, DWORD)
{
    if (!g_uiMonitor.active || g_uiMonitor.processing)
    {
        return;
    }

    g_uiMonitor.processing = true;
    bool shouldStop = false;

    const int initStatus = UF_initialize();
    if (initStatus != 0)
    {
        std::ostringstream log;
        log << "AutoCreateThreeViews: UI monitor UF_initialize failed, status=" << initStatus << ".";
        WriteLauncherLog(log.str());
        g_uiMonitor.processing = false;
        return;
    }

    try
    {
        ProcessUiDisplayCommand(g_uiMonitor.displayCommandPath);

        if (std::filesystem::exists(g_uiMonitor.requestPath))
        {
            WriteLauncherLog("AutoCreateThreeViews: UI monitor request received, start drawing creation.");
            ExecuteUiRequestParts(g_uiMonitor.requestPath);
            ClearSelectionAndOccurrenceHighlights(true);
            ShowAutoCreateThreeViewsRunResults();
            std::error_code ignored;
            std::filesystem::remove(g_uiMonitor.requestPath, ignored);
            std::filesystem::remove(g_uiMonitor.displayCommandPath, ignored);
            shouldStop = true;
        }
        else if (g_uiMonitor.processInfo.hProcess != nullptr &&
                 WaitForSingleObject(g_uiMonitor.processInfo.hProcess, 0) == WAIT_OBJECT_0)
        {
            DWORD exitCode = 0;
            GetExitCodeProcess(g_uiMonitor.processInfo.hProcess, &exitCode);
            std::ostringstream log;
            log << "AutoCreateThreeViews: UI closed without drawing request, exitCode=" << exitCode << ".";
            WriteLauncherLog(log.str());
            ClearSelectionAndOccurrenceHighlights(true);
            shouldStop = true;
        }
    }
    catch (const std::exception& ex)
    {
        WriteLauncherLog(std::string("AutoCreateThreeViews: UI monitor exception: ") + ex.what());
        ClearSelectionAndOccurrenceHighlights(true);
        shouldStop = true;
    }
    catch (...)
    {
        WriteLauncherLog("AutoCreateThreeViews: UI monitor unknown exception.");
        ClearSelectionAndOccurrenceHighlights(true);
        shouldStop = true;
    }

    UF_terminate();
    g_uiMonitor.processing = false;

    if (shouldStop)
    {
        StopUiMonitor();
    }
}

bool TryRunWpfDialogAndCreateDrawing()
{
    const std::filesystem::path exePath = CurrentModuleDirectory() / "AutoCreateThreeViewsUI" / "AutoCreateThreeViewsUI.exe";
    WriteLauncherLog("AutoCreateThreeViews: UI exe path=" + exePath.string());
    if (!std::filesystem::exists(exePath))
    {
        WriteLauncherLog("AutoCreateThreeViews: UI exe not found, fallback to Block Styler dialog.");
        return false;
    }

    if (g_uiMonitor.active && g_uiMonitor.processInfo.hProcess != nullptr)
    {
        if (WaitForSingleObject(g_uiMonitor.processInfo.hProcess, 0) == WAIT_TIMEOUT)
        {
            WriteLauncherLog("AutoCreateThreeViews: UI already running, keep existing non-modal dialog.");
            return true;
        }

        StopUiMonitor();
    }

    const std::filesystem::path requestPath = CurrentModuleDirectory() / "AutoCreateThreeViewsUI" / "AutoCreateThreeViews.request";
    const std::filesystem::path assemblyPath = CurrentModuleDirectory() / "AutoCreateThreeViewsUI" / "AutoCreateThreeViews.assembly";
    const std::filesystem::path displayCommandPath = CurrentModuleDirectory() / "AutoCreateThreeViewsUI" / "AutoCreateThreeViews.display";
    WriteLauncherLog("AutoCreateThreeViews: UI request path=" + requestPath.string());
    std::error_code ignored;
    std::filesystem::remove(requestPath, ignored);
    std::filesystem::remove(displayCommandPath, ignored);
    WriteAssemblyManifest(assemblyPath);

    std::wstring commandLine = QuoteArgument(exePath) +
        L" --request " + QuoteArgument(requestPath) +
        L" --assembly " + QuoteArgument(assemblyPath) +
        L" --display " + QuoteArgument(displayCommandPath);
    STARTUPINFOW startupInfo = {};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInfo = {};

    std::wstring mutableCommandLine = commandLine;
    const BOOL created = CreateProcessW(
        exePath.c_str(),
        mutableCommandLine.data(),
        nullptr,
        nullptr,
        FALSE,
        0,
        nullptr,
        exePath.parent_path().c_str(),
        &startupInfo,
        &processInfo);

    if (!created)
    {
        std::ostringstream log;
        log << "AutoCreateThreeViews: UI CreateProcess failed, GetLastError=" << GetLastError() << ".";
        WriteLauncherLog(log.str());
        return false;
    }

    const UINT_PTR timerId = SetTimer(nullptr, 0, 150, UiMonitorTimerProc);
    if (timerId == 0)
    {
        std::ostringstream log;
        log << "AutoCreateThreeViews: SetTimer failed, GetLastError=" << GetLastError() << ".";
        WriteLauncherLog(log.str());
        TerminateProcess(processInfo.hProcess, 1);
        CloseHandle(processInfo.hThread);
        CloseHandle(processInfo.hProcess);
        return false;
    }

    g_uiMonitor.requestPath = requestPath;
    g_uiMonitor.displayCommandPath = displayCommandPath;
    g_uiMonitor.processInfo = processInfo;
    g_uiMonitor.timerId = timerId;
    g_uiMonitor.processing = false;
    g_uiMonitor.active = true;

    WriteLauncherLog("AutoCreateThreeViews: UI process started in non-modal mode.");
    return true;
}
}

extern "C" DllExport void ufusr(char* param, int* returnCode, int rlen)
{
    (void)param;
    (void)rlen;

    if (!zhihui_license_guard::EnsureAuthorized(L"ZHIHUI.AUTOCREATETHREEVIEWS", L"AutoCreateThreeViews"))
    {
        return;
    }

    WriteLauncherLog(std::string("AutoCreateThreeViews: DLL entry, build ") + __DATE__ + " " + __TIME__ + ", unload=UF_UNLOAD_SEL_DIALOG.");

    if (returnCode != nullptr)
    {
        *returnCode = 0;
    }

    const int initStatus = UF_initialize();
    if (initStatus != 0)
    {
        if (returnCode != nullptr)
        {
            *returnCode = initStatus;
        }
        return;
    }

    try
    {
        if (TryRunWpfDialogAndCreateDrawing())
        {
            UF_terminate();
            return;
        }

        AutoCreateThreeViewsDialog dialog;
        dialog.Launch();
    }
    catch (const NXOpen::NXException& ex)
    {
        WriteLauncherLog(std::string("AutoCreateThreeViews: DLL entry NXException: ") + ex.Message());
        if (returnCode != nullptr)
        {
            *returnCode = ex.ErrorCode();
        }
    }
    catch (...)
    {
        WriteLauncherLog("AutoCreateThreeViews: DLL entry unknown exception.");
        if (returnCode != nullptr)
        {
            *returnCode = -1;
        }
    }

    ClearSelectionAndOccurrenceHighlights(true);
    UF_terminate();
}

extern "C" DllExport int ufusr_ask_unload(void)
{
    WriteLauncherLog("AutoCreateThreeViews: ufusr_ask_unload called, return UF_UNLOAD_SEL_DIALOG.");
    return UF_UNLOAD_SEL_DIALOG;
}

extern "C" DllExport void ufusr_cleanup(void)
{
    StopUiMonitor();
    ClearSelectionAndOccurrenceHighlights(false);
}
