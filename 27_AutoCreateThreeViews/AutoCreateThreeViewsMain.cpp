#include "AutoCreateThreeViews.hpp"

#include <NXOpen/BasePart.hxx>
#include <NXOpen/Body.hxx>
#include <NXOpen/BodyCollection.hxx>
#include <NXOpen/DisplayableObject.hxx>
#include <NXOpen/DisplayManager.hxx>
#include <NXOpen/Drawings_DraftingDrawingSheet.hxx>
#include <NXOpen/Drawings_DrawingSheetCollection.hxx>
#include <NXOpen/Features_FeatureCollection.hxx>
#include <NXOpen/Features_SheetMetal_SheetmetalManager.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXMessageBox.hxx>
#include <NXOpen/NXObject.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/PartLoadStatus.hxx>
#include <NXOpen/PartTypes.hxx>
#include <NXOpen/DraftingManager.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/View.hxx>
#include <NXOpen/ViewCollection.hxx>
#include <NXOpen/Selection.hxx>
#include <NXOpen/UI.hxx>

#include <uf.h>
#include <uf_assem.h>
#include <uf_defs.h>
#include <uf_disp.h>
#include <uf_draw.h>
#include <uf_obj.h>
#include <uf_part.h>
#include <uf_view.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#include <wincrypt.h>

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
    bool showRunResults = false;
};

UiMonitorState g_uiMonitor;
std::vector<tag_t> g_assemblyOccurrenceTags;
std::set<tag_t> g_knownHighlightedOccurrences;
tag_t g_highlightedOccurrence = NULL_TAG;

struct AssemblyFilterMetadata
{
    bool hasDrawingSheets = false;
    bool isSheetMetal = false;
    std::string attributes;
};

std::map<tag_t, AssemblyFilterMetadata> g_assemblyFilterMetadataByPart;

struct SelectedDrawingPart
{
    int index = 0;
    tag_t occurrence = NULL_TAG;
    tag_t prototypePart = NULL_TAG;
    bool assemblyDrawing = false;
};

struct SelectedLayerDrawingTarget
{
    SelectedDrawingPart part;
    int targetLayer = 0;
    int layerIndex = 0;
    int layersPerSheet = 1;
};

struct AsyncDrawingBatch
{
    std::filesystem::path requestPath;
    std::map<std::string, std::string> values;
    std::vector<SelectedLayerDrawingTarget> targets;
    size_t nextTarget = 0;
    bool layerDrawingMode = false;
    bool directSingleRequest = false;
    bool showRunResults = false;
    bool targetContextReady = false;
    bool completedSheetNeedsPresentation = false;
    tag_t completedDrawingSheetTag = NULL_TAG;
    std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
};

std::unique_ptr<AsyncDrawingBatch> g_asyncDrawingBatch;

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

using LauncherTimingClock = std::chrono::steady_clock;

void WriteLauncherTiming(
    const std::string& stage,
    const LauncherTimingClock::time_point& started,
    const std::string& detail = std::string())
{
    const double elapsedMs =
        std::chrono::duration<double, std::milli>(LauncherTimingClock::now() - started).count();
    std::ostringstream line;
    line << std::fixed << std::setprecision(1)
         << "AutoCreateThreeViews: [timing] stage=" << stage
         << ", elapsedMs=" << elapsedMs;
    if (!detail.empty())
    {
        line << ", " << detail;
    }
    line << ".";
    WriteLauncherLog(line.str());
}

std::string PartNameForLog(tag_t partTag)
{
    if (partTag == NULL_TAG)
    {
        return std::string();
    }

    char partName[MAX_FSPEC_BUFSIZE] = {0};
    if (UF_PART_ask_part_name(partTag, partName) == 0)
    {
        return std::string(partName);
    }
    return std::string();
}

void LogDrawingContext(const std::string& prefix)
{
    const tag_t displayPart = UF_PART_ask_display_part();
    const tag_t workPart = UF_ASSEM_ask_work_part();
    const tag_t workOccurrence = UF_ASSEM_ask_work_occurrence();

    std::ostringstream log;
    log << "AutoCreateThreeViews: " << prefix
        << ", actualDisplay=" << static_cast<unsigned long long>(displayPart)
        << ", actualDisplayName=" << PartNameForLog(displayPart)
        << ", actualWorkPart=" << static_cast<unsigned long long>(workPart)
        << ", actualWorkPartName=" << PartNameForLog(workPart)
        << ", actualWorkOccurrence=" << static_cast<unsigned long long>(workOccurrence)
        << ".";
    WriteLauncherLog(log.str());
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

bool LooksLikeUtf8(const std::string& value)
{
    int remaining = 0;
    for (unsigned char ch : value)
    {
        if (remaining == 0)
        {
            if ((ch & 0x80) == 0)
            {
                continue;
            }
            if ((ch & 0xE0) == 0xC0)
            {
                remaining = 1;
            }
            else if ((ch & 0xF0) == 0xE0)
            {
                remaining = 2;
            }
            else if ((ch & 0xF8) == 0xF0)
            {
                remaining = 3;
            }
            else
            {
                return false;
            }
        }
        else
        {
            if ((ch & 0xC0) != 0x80)
            {
                return false;
            }
            --remaining;
        }
    }
    return remaining == 0;
}

std::string AcpToUtf8(const std::string& value)
{
    if (value.empty() || LooksLikeUtf8(value))
    {
        return value;
    }

    const int wideLength = MultiByteToWideChar(CP_ACP, 0, value.c_str(), -1, nullptr, 0);
    if (wideLength <= 0)
    {
        return value;
    }

    std::wstring wide(static_cast<size_t>(wideLength), L'\0');
    MultiByteToWideChar(CP_ACP, 0, value.c_str(), -1, &wide[0], wideLength);

    const int utf8Length = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (utf8Length <= 0)
    {
        return value;
    }

    std::string utf8(static_cast<size_t>(utf8Length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, &utf8[0], utf8Length, nullptr, nullptr);
    if (!utf8.empty() && utf8.back() == '\0')
    {
        utf8.pop_back();
    }
    return utf8;
}

std::string EscapeManifestValue(const std::string& value)
{
    const std::string utf8Value = AcpToUtf8(value);
    std::ostringstream escaped;
    escaped << std::hex;
    for (unsigned char ch : utf8Value)
    {
        escaped << '%' << std::uppercase;
        escaped.width(2);
        escaped.fill('0');
        escaped << static_cast<int>(ch);
        escaped << std::nouppercase;
    }
    return escaped.str();
}

std::string NxStringToUtf8(const NXOpen::NXString& value)
{
    const char* text = value.GetUTF8Text();
    return text == nullptr ? std::string() : std::string(text);
}

std::string FormatAttributeValue(const NXOpen::NXObject::AttributeInformation& attribute)
{
    std::ostringstream value;
    switch (attribute.Type)
    {
    case NXOpen::NXObject::AttributeTypeBoolean:
        return attribute.BooleanValue ? "true" : "false";
    case NXOpen::NXObject::AttributeTypeInteger:
        value << attribute.IntegerValue;
        return value.str();
    case NXOpen::NXObject::AttributeTypeReal:
        value << std::setprecision(15) << attribute.RealValue;
        return value.str();
    case NXOpen::NXObject::AttributeTypeString:
        return NxStringToUtf8(attribute.StringValue);
    case NXOpen::NXObject::AttributeTypeTime:
        return NxStringToUtf8(attribute.TimeValue);
    default:
        return "";
    }
}

NXOpen::Part* ResolvePartFromTag(tag_t partTag)
{
    if (partTag == NULL_TAG)
    {
        return nullptr;
    }

    try
    {
        return dynamic_cast<NXOpen::Part*>(NXOpen::NXObjectManager::Get(partTag));
    }
    catch (...)
    {
        return nullptr;
    }
}

bool EnsurePartFullyLoaded(NXOpen::Part* part)
{
    if (part == nullptr)
    {
        return false;
    }

    try
    {
        if (part->IsFullyLoaded())
        {
            return true;
        }

        NXOpen::PartLoadStatus* status = part->LoadFully();
        delete status;
        return part->IsFullyLoaded();
    }
    catch (...)
    {
        return false;
    }
}

AssemblyFilterMetadata ClassifyPartForAssemblyFilter(tag_t partTag)
{
    const auto cached = g_assemblyFilterMetadataByPart.find(partTag);
    if (cached != g_assemblyFilterMetadataByPart.end())
    {
        return cached->second;
    }

    AssemblyFilterMetadata metadata;
    NXOpen::Part* part = ResolvePartFromTag(partTag);
    if (part == nullptr)
    {
        g_assemblyFilterMetadataByPart[partTag] = metadata;
        return metadata;
    }

    EnsurePartFullyLoaded(part);

    try
    {
        NXOpen::Drawings::DrawingSheetCollection* sheets = part->DrawingSheets();
        metadata.hasDrawingSheets = sheets != nullptr && sheets->begin() != sheets->end();
    }
    catch (...)
    {
    }

    try
    {
        if (part->Bodies() != nullptr &&
            part->Features() != nullptr &&
            part->Features()->SheetmetalManager() != nullptr)
        {
            NXOpen::Features::SheetMetal::SheetmetalManager* manager =
                part->Features()->SheetmetalManager();
            for (NXOpen::BodyCollection::iterator it = part->Bodies()->begin();
                 it != part->Bodies()->end();
                 ++it)
            {
                NXOpen::Body* body = *it;
                if (body == nullptr || !body->IsSolidBody() || body->IsSheetBody())
                {
                    continue;
                }

                try
                {
                    if (manager->GetBodyThickness(body) > 1.0e-6)
                    {
                        metadata.isSheetMetal = true;
                        break;
                    }
                }
                catch (...)
                {
                }
            }
        }
    }
    catch (...)
    {
    }

    try
    {
        std::ostringstream attributes;
        for (const NXOpen::NXObject::AttributeInformation& attribute : part->GetUserAttributes())
        {
            if (attribute.Unset)
            {
                continue;
            }

            const std::string title = TrimText(NxStringToUtf8(attribute.Title));
            if (title.empty())
            {
                continue;
            }

            attributes << title << '\x1f' << FormatAttributeValue(attribute) << '\x1e';
        }
        metadata.attributes = attributes.str();
    }
    catch (...)
    {
    }

    g_assemblyFilterMetadataByPart[partTag] = metadata;
    return metadata;
}

bool IsOccurrenceHidden(tag_t occurrence)
{
    if (occurrence == NULL_TAG)
    {
        return false;
    }

    try
    {
        NXOpen::DisplayableObject* displayable =
            dynamic_cast<NXOpen::DisplayableObject*>(NXOpen::NXObjectManager::Get(occurrence));
        return displayable != nullptr && displayable->IsBlanked();
    }
    catch (...)
    {
        return false;
    }
}

std::string BaseNameFromPathText(const std::string& pathText)
{
    // NX returns part names as narrow text.  Converting that text through a
    // std::filesystem::path on Windows makes the CRT interpret valid UTF-8 as
    // the active code page, which turns Chinese names into mojibake.  A part
    // display name only needs byte-preserving path/extension removal.
    const size_t slash = pathText.find_last_of("\\/");
    std::string filename = slash == std::string::npos ? pathText : pathText.substr(slash + 1);
    const size_t dot = filename.find_last_of('.');
    if (dot != std::string::npos && dot > 0)
    {
        filename.erase(dot);
    }
    return filename;
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

    return AcpToUtf8(partName);
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
            return AcpToUtf8(childName);
        }
    }

    return "";
}

std::string DisplayNameFromOccurrence(tag_t occurrence, const std::string& partName)
{
    char objectName[UF_OBJ_NAME_BUFSIZE] = {0};
    if (occurrence != NULL_TAG && UF_OBJ_ask_name(occurrence, objectName) == 0)
    {
        std::string name = TrimText(AcpToUtf8(objectName));
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
    const std::string& partName,
    bool isAssembly)
{
    const tag_t prototypePart =
        occurrence == NULL_TAG ? UF_PART_ask_display_part() : UF_ASSEM_ask_prototype_of_occ(occurrence);
    const AssemblyFilterMetadata metadata = ClassifyPartForAssemblyFilter(prototypePart);
    output << "id=" << static_cast<unsigned long long>(occurrence)
           << "\tparent=" << static_cast<unsigned long long>(parentOccurrence)
           << "\tdepth=" << depth
           << "\tname=" << EscapeManifestValue(displayName)
           << "\tpart=" << EscapeManifestValue(partName)
           << "\tassembly=" << (isAssembly ? "true" : "false")
           << "\tdrawing=" << (metadata.hasDrawingSheets ? "true" : "false")
           << "\tsheetMetal=" << (metadata.isSheetMetal ? "true" : "false")
           << "\thidden=" << (IsOccurrenceHidden(occurrence) ? "true" : "false")
           << "\tattributes=" << EscapeManifestValue(metadata.attributes)
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
        tag_t* grandchildren = nullptr;
        const int grandchildCount = UF_ASSEM_ask_part_occ_children(child, &grandchildren);
        if (grandchildren != nullptr)
        {
            UF_free(grandchildren);
        }
        WriteAssemblyManifestLine(
            output,
            child,
            parentOccurrence,
            depth,
            displayName,
            partName,
            grandchildCount > 0);
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
    const LauncherTimingClock::time_point started = LauncherTimingClock::now();
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
    g_assemblyFilterMetadataByPart.clear();
    g_knownHighlightedOccurrences.clear();
    g_highlightedOccurrence = NULL_TAG;

    std::ofstream output(manifestPath, std::ios::binary);
    if (!output)
    {
        WriteLauncherLog("AutoCreateThreeViews: cannot create assembly manifest=" + manifestPath.string() + ".");
        WriteLauncherTiming("build_assembly_manifest_failed", started);
        return false;
    }

    const tag_t displayPart = UF_PART_ask_display_part();
    if (displayPart == NULL_TAG)
    {
        WriteLauncherTiming("build_assembly_manifest_failed", started, "reason=no_display_part");
        return false;
    }

    const std::string rootPartName = PartNameFromPartTag(displayPart);
    const std::string rootDisplayName = DisplayNameFromOccurrence(NULL_TAG, rootPartName);
    const tag_t rootOccurrence = UF_ASSEM_ask_root_part_occ(displayPart);
    tag_t* rootChildren = nullptr;
    const int rootChildCount =
        rootOccurrence == NULL_TAG ? 0 : UF_ASSEM_ask_part_occ_children(rootOccurrence, &rootChildren);
    if (rootChildren != nullptr)
    {
        UF_free(rootChildren);
    }
    WriteAssemblyManifestLine(
        output,
        rootOccurrence,
        NULL_TAG,
        0,
        rootDisplayName,
        rootPartName,
        rootChildCount > 0);
    if (rootOccurrence != NULL_TAG)
    {
        WriteAssemblyChildren(output, rootOccurrence, 1);
    }

    WriteLauncherTiming(
        "build_assembly_manifest",
        started,
        "occurrences=" + std::to_string(g_assemblyOccurrenceTags.size()) +
            ", uniqueParts=" + std::to_string(g_assemblyFilterMetadataByPart.size()));
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

std::vector<std::pair<int, int>> ParseLayerRanges(std::string text)
{
    std::vector<std::pair<int, int>> ranges;
    std::replace(text.begin(), text.end(), ';', ',');
    size_t start = 0;
    while (start <= text.size())
    {
        const size_t comma = text.find(',', start);
        const std::string token = TrimText(text.substr(start, comma == std::string::npos ? std::string::npos : comma - start));
        if (!token.empty())
        {
            const size_t dash = token.find('-');
            int first = ParseIntValue(dash == std::string::npos ? token : token.substr(0, dash), 0);
            int last = ParseIntValue(dash == std::string::npos ? token : token.substr(dash + 1), 0);
            first = std::max(1, std::min(256, first));
            last = std::max(1, std::min(256, last));
            if (first > last)
            {
                std::swap(first, last);
            }
            if (first > 0 && last > 0)
            {
                ranges.push_back({first, last});
            }
        }
        if (comma == std::string::npos)
        {
            break;
        }
        start = comma + 1;
    }
    if (ranges.empty())
    {
        ranges.push_back({1, 256});
    }
    return ranges;
}

bool LayerInRanges(int layer, const std::vector<std::pair<int, int>>& ranges)
{
    return std::any_of(ranges.begin(), ranges.end(), [layer](const std::pair<int, int>& range) {
        return layer >= range.first && layer <= range.second;
    });
}

std::vector<int> CollectDrawablePartLayers(
    tag_t partTag,
    const std::vector<std::pair<int, int>>& ranges)
{
    std::set<int> layers;
    NXOpen::Part* part = ResolvePartFromTag(partTag);
    if (part == nullptr)
    {
        return {};
    }
    EnsurePartFullyLoaded(part);
    try
    {
        if (part->Bodies() != nullptr)
        {
            for (NXOpen::BodyCollection::iterator it = part->Bodies()->begin(); it != part->Bodies()->end(); ++it)
            {
                NXOpen::Body* body = *it;
                if (body == nullptr || !body->IsSolidBody() || body->IsSheetBody() || body->IsBlanked())
                {
                    continue;
                }
                const int layer = body->Layer();
                if (layer >= 1 && layer <= 256 && LayerInRanges(layer, ranges))
                {
                    layers.insert(layer);
                }
            }
        }
    }
    catch (...)
    {
    }
    return std::vector<int>(layers.begin(), layers.end());
}

bool IsManualFrontDirectionModeText(std::string value)
{
    value = TrimText(value);
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value == "manualfacex" || value == "manual" || value == "selectedfacex";
}

bool IsManualPartFrontDirection(
    const std::map<std::string, std::string>& values,
    int index)
{
    return IsManualFrontDirectionModeText(FindPartValue(values, index, "frontDirectionMode", FindValue(values, "frontDirectionMode")));
}

std::string DecodeBase64TextOrOriginal(const std::string& text)
{
    const std::string trimmed = TrimText(text);
    if (trimmed.empty())
    {
        return "";
    }

    DWORD binaryLength = 0;
    if (!CryptStringToBinaryA(
            trimmed.c_str(),
            static_cast<DWORD>(trimmed.size()),
            CRYPT_STRING_BASE64,
            nullptr,
            &binaryLength,
            nullptr,
            nullptr) ||
        binaryLength == 0)
    {
        return LooksLikeUtf8(trimmed) ? trimmed : AcpToUtf8(trimmed);
    }

    std::string decoded(static_cast<size_t>(binaryLength), '\0');
    if (!CryptStringToBinaryA(
            trimmed.c_str(),
            static_cast<DWORD>(trimmed.size()),
            CRYPT_STRING_BASE64,
            reinterpret_cast<BYTE*>(&decoded[0]),
            &binaryLength,
            nullptr,
            nullptr))
    {
        return LooksLikeUtf8(trimmed) ? trimmed : AcpToUtf8(trimmed);
    }

    decoded.resize(static_cast<size_t>(binaryLength));
    return LooksLikeUtf8(decoded) ? decoded : AcpToUtf8(decoded);
}

std::string ProgressPartDisplayName(
    const std::map<std::string, std::string>& values,
    int index,
    tag_t prototypePart)
{
    std::string name = TrimText(DecodeBase64TextOrOriginal(FindPartValue(values, index, "name")));
    if (name.empty())
    {
        name = TrimText(DecodeBase64TextOrOriginal(FindPartValue(values, index, "partPath")));
        if (!name.empty())
        {
            name = BaseNameFromPathText(name);
        }
    }
    if (name.empty())
    {
        name = BaseNameFromPathText(AcpToUtf8(PartNameForLog(prototypePart)));
    }
    if (name.empty())
    {
        std::ostringstream fallback;
        fallback << "part " << (index + 1);
        name = fallback.str();
    }
    return name;
}

void WriteSinglePartRequest(
    const std::filesystem::path& requestPath,
    const std::map<std::string, std::string>& values,
    int index,
    bool assemblyDrawing,
    int targetLayer = 0,
    int layerIndex = 0,
    int layersPerSheet = 1,
    const std::string& executionPhase = "full")
{
    static const std::vector<std::string> settingKeys = {
        "templatePath",
        "inheritDraftingPreferences",
        "layerLayoutMode",
        "projection",
        "frontDirectionMode",
        "viewSpacing",
        "viewGroupSpacing",
        "sheetMargin",
        "showHiddenLines",
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
    output << "executionPhase=" << executionPhase << '\n';
    output << "selectedOccurrenceTag=" << FindPartValue(values, index, "occurrenceTag") << '\n';
    output << "assemblyDrawing=" << (assemblyDrawing ? "true" : "false") << '\n';
    output << "drawingTargetMode=" << (targetLayer > 0 ? "partLayers" : "partOrAssembly") << '\n';
    output << "layerRange=" << FindValue(values, "layerRange", "1-256") << '\n';
    output << "layersPerSheet=" << std::max(1, layersPerSheet) << '\n';
    output << "targetLayer=" << targetLayer << '\n';
    output << "layerIndex=" << layerIndex << '\n';
    output << "appendToCurrentSheet=" << (targetLayer > 0 && layerIndex % std::max(1, layersPerSheet) != 0 ? "true" : "false") << '\n';
    for (const std::string& key : settingKeys)
    {
        output << key << '=' << FindPartValue(values, index, key, FindValue(values, key)) << '\n';
    }
}

std::filesystem::path ProgressPathFromRequest(const std::filesystem::path& requestPath)
{
    std::filesystem::path directory = requestPath.parent_path();
    if (directory.empty())
    {
        directory = CurrentModuleDirectory();
    }
    const std::string fileName = requestPath.filename().string();
    if (fileName.find(".request.part") != std::string::npos)
    {
        return directory / "AutoCreateThreeViews.progress";
    }
    return directory / "AutoCreateThreeViews.progress";
}

void WriteProgressFile(
    const std::filesystem::path& requestPath,
    int current,
    int total,
    const std::string& message,
    bool done)
{
    try
    {
        const std::filesystem::path progressPath = ProgressPathFromRequest(requestPath);
        std::filesystem::create_directories(progressPath.parent_path());
        std::ofstream output(progressPath, std::ios::binary | std::ios::trunc);
        output << "current=" << current << '\n'
               << "total=" << total << '\n'
               << "message=" << message << '\n'
               << "done=" << (done ? "1" : "0") << '\n';
    }
    catch (...)
    {
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

    int nxDisplayStatus = -1;
    int displayStatus = 0;
    int workStatus = 0;
    bool nxWorkSet = false;
    LogDrawingContext("before set selected drawing part");
    try
    {
        NXOpen::Session* session = NXOpen::Session::GetSession();
        NXOpen::Part* part = dynamic_cast<NXOpen::Part*>(NXOpen::NXObjectManager::Get(partTag));
        if (session != nullptr && session->Parts() != nullptr && part != nullptr)
        {
            NXOpen::PartLoadStatus* loadStatus = nullptr;
            nxDisplayStatus = static_cast<int>(
                session->Parts()->SetActiveDisplay(
                    part,
                    NXOpen::DisplayPartOptionReplaceExisting,
                    NXOpen::PartDisplayPartWorkPartOptionSameAsDisplay,
                    &loadStatus));
            if (loadStatus != nullptr)
            {
                delete loadStatus;
            }
            NXOpen::Part* workPart = session->Parts()->Work();
            NXOpen::Part* displayPart = session->Parts()->Display();
            nxWorkSet = true;

            std::ostringstream switchLog;
            switchLog << "AutoCreateThreeViews: simple display switch, workPart="
                      << static_cast<unsigned long long>(workPart != nullptr ? workPart->Tag() : NULL_TAG)
                      << ", displayPart="
                      << static_cast<unsigned long long>(displayPart != nullptr ? displayPart->Tag() : NULL_TAG)
                      << ".";
            WriteLauncherLog(switchLog.str());
        }
    }
    catch (const NXOpen::NXException& ex)
    {
        WriteLauncherLog(std::string("AutoCreateThreeViews: NXOpen set active display/work part failed, NXException: ") + ex.Message() + ".");
    }
    catch (...)
    {
        WriteLauncherLog("AutoCreateThreeViews: NXOpen set active display/work part failed, unknown exception.");
    }

    UF_DISP_make_display_up_to_date();

    const tag_t actualDisplayPart = UF_PART_ask_display_part();
    const tag_t actualWorkPart = UF_ASSEM_ask_work_part();
    const tag_t actualWorkOccurrence = UF_ASSEM_ask_work_occurrence();

    std::ostringstream log;
    log << "AutoCreateThreeViews: set selected drawing part, occurrence="
        << static_cast<unsigned long long>(sourceOccurrence)
        << ", part=" << static_cast<unsigned long long>(partTag)
        << ", nxDisplayStatus=" << nxDisplayStatus
        << ", nxWorkSet=" << (nxWorkSet ? "yes" : "no")
        << ", displayStatus=" << displayStatus
        << ", workStatus=" << workStatus
        << ", name=" << PartNameForLog(partTag)
        << ", actualDisplay=" << static_cast<unsigned long long>(actualDisplayPart)
        << ", actualWorkPart=" << static_cast<unsigned long long>(actualWorkPart)
        << ", actualWorkOccurrence=" << static_cast<unsigned long long>(actualWorkOccurrence)
        << ".";
    WriteLauncherLog(log.str());
    LogDrawingContext("after set selected drawing part");
    return nxDisplayStatus >= 0 && displayStatus == 0 && workStatus == 0 && actualDisplayPart == partTag && actualWorkPart == partTag;
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
    LogDrawingContext("after restore drawing context");
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

bool ApplySelectedOccurrenceFromRequest(const std::filesystem::path& requestPath)
{
    const std::map<std::string, std::string> values = ReadSimpleKeyValueFile(requestPath);
    const auto occurrence = values.find("selectedOccurrenceTag");
    if (occurrence == values.end())
    {
        return false;
    }

    tag_t occurrenceTag = NULL_TAG;
    if (ParseTagValue(occurrence->second, occurrenceTag))
    {
        DisplaySelectedOccurrence(occurrenceTag);
        if (SetDrawingDisplayPart(PrototypePartFromOccurrence(occurrenceTag), occurrenceTag))
        {
            return true;
        }
    }
    return false;
}

bool PreselectManualDirectionsBeforeDrawing(
    const std::map<std::string, std::string>& values,
    const std::vector<SelectedLayerDrawingTarget>& targets)
{
    ClearAutoCreateThreeViewsManualDirectionCache();

    int manualTargetCount = 0;
    for (const SelectedLayerDrawingTarget& target : targets)
    {
        if (IsManualPartFrontDirection(values, target.part.index))
        {
            ++manualTargetCount;
        }
    }
    if (manualTargetCount == 0)
    {
        return true;
    }

    int selectionIndex = 0;
    WriteLauncherLog(
        "AutoCreateThreeViews: begin manual direction preselection before drawing, targets=" +
            std::to_string(manualTargetCount) + ".");
    for (const SelectedLayerDrawingTarget& target : targets)
    {
        if (!IsManualPartFrontDirection(values, target.part.index))
        {
            continue;
        }

        ++selectionIndex;
        int clearedHighlights = 0;
        UnhighlightKnownOccurrences(clearedHighlights);
        UnhighlightDisplayedAssemblyTree(clearedHighlights);
        g_highlightedOccurrence = NULL_TAG;
        g_knownHighlightedOccurrences.clear();

        if (!SetDrawingDisplayPart(target.part.prototypePart, target.part.occurrence))
        {
            AddAutoCreateThreeViewsRunResultLine(u8"失败：手动选择方向前无法切换到目标部件。");
            ClearAutoCreateThreeViewsManualDirectionCache();
            return false;
        }

        WriteLauncherLog(
            "AutoCreateThreeViews: select manual direction " +
                std::to_string(selectionIndex) + "/" + std::to_string(manualTargetCount) +
                ", part=" + std::to_string(static_cast<unsigned long long>(target.part.prototypePart)) +
                ", layer=" + std::to_string(target.targetLayer) + ".");
        if (!PreselectAutoCreateThreeViewsManualDirection(
                target.part.prototypePart,
                target.targetLayer))
        {
            AddAutoCreateThreeViewsRunResultLine(
                std::string(u8"取消：图层 ") + std::to_string(target.targetLayer) +
                u8" 的主视图方向未选择，尚未开始出图。");
            ClearAutoCreateThreeViewsManualDirectionCache();
            return false;
        }
    }

    WriteLauncherLog(
        "AutoCreateThreeViews: all manual directions selected; start drawing batch.");
    return true;
}

void ExecuteUiRequestParts(const std::filesystem::path& requestPath)
{
    const LauncherTimingClock::time_point batchStarted = LauncherTimingClock::now();
    BeginAutoCreateThreeViewsRunResults();
    g_uiMonitor.showRunResults = false;

    const std::map<std::string, std::string> values = ReadSimpleKeyValueFile(requestPath);
    const int partCount = ParseIntValue(FindValue(values, "selectedPartCount"), 0);
    const bool layerDrawingMode =
        TrimText(FindValue(values, "drawingTargetMode", "partOrAssembly")) == "partLayers";
    const tag_t previousDisplayPart = UF_PART_ask_display_part();
    const tag_t previousWorkOccurrence = UF_ASSEM_ask_work_occurrence();
    const tag_t previousWorkPart = UF_ASSEM_ask_work_part();

    if (partCount <= 0 && !layerDrawingMode)
    {
        if (ApplySelectedOccurrenceFromRequest(requestPath))
        {
            WriteLauncherLog("AutoCreateThreeViews: selected drawing part is active; defer drafting application entry until the sheet is created.");
        }
        ExecuteAutoCreateThreeViewsFromRequest(requestPath);
        WriteLauncherTiming("ui_request_total", batchStarted, "parts=1");
        return;
    }

    std::vector<SelectedDrawingPart> drawingParts;
    if (partCount <= 0)
    {
        tag_t occurrenceTag = NULL_TAG;
        tag_t prototypePart = previousWorkPart != NULL_TAG ? previousWorkPart : previousDisplayPart;
        if (ParseTagValue(FindValue(values, "selectedOccurrenceTag"), occurrenceTag))
        {
            const tag_t occurrencePrototype = PrototypePartFromOccurrence(occurrenceTag);
            if (occurrencePrototype != NULL_TAG)
            {
                prototypePart = occurrencePrototype;
            }
        }
        if (prototypePart != NULL_TAG)
        {
            drawingParts.push_back({0, occurrenceTag, prototypePart, false});
        }
    }
    for (int index = 0; index < partCount; ++index)
    {
        tag_t occurrenceTag = NULL_TAG;
        if (ParseTagValue(FindPartValue(values, index, "occurrenceTag"), occurrenceTag))
        {
            const bool isRootOccurrence = IsRootOccurrence(occurrenceTag, previousDisplayPart);
            const bool hasChildren = OccurrenceHasChildren(occurrenceTag);

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

            drawingParts.push_back({index, occurrenceTag, prototypePart, isRootOccurrence || hasChildren});
        }
    }

    std::vector<SelectedLayerDrawingTarget> drawingTargets;
    if (layerDrawingMode)
    {
        const std::vector<std::pair<int, int>> ranges =
            ParseLayerRanges(FindValue(values, "layerRange", "1-256"));
        const int layersPerSheet = std::max(1, ParseIntValue(FindValue(values, "layersPerSheet", "1"), 1));
        std::set<tag_t> seenPrototypeParts;
        for (const SelectedDrawingPart& selected : drawingParts)
        {
            if (!seenPrototypeParts.insert(selected.prototypePart).second)
            {
                WriteLauncherLog("AutoCreateThreeViews: layer drawing skipped duplicate prototype part.");
                continue;
            }
            const std::vector<int> layers = CollectDrawablePartLayers(selected.prototypePart, ranges);
            if (layers.empty())
            {
                AddAutoCreateThreeViewsRunResultLine(
                    std::string(u8"跳过：") + ProgressPartDisplayName(values, selected.index, selected.prototypePart) +
                    u8"，指定图层范围内没有可出图的可见实体。");
                continue;
            }
            for (size_t layerIndex = 0; layerIndex < layers.size(); ++layerIndex)
            {
                const size_t pageStart = (layerIndex / static_cast<size_t>(layersPerSheet)) *
                    static_cast<size_t>(layersPerSheet);
                const int actualLayersOnPage = static_cast<int>(std::min(
                    static_cast<size_t>(layersPerSheet),
                    layers.size() - pageStart));
                const int indexOnPage = static_cast<int>(layerIndex - pageStart);
                drawingTargets.push_back(
                    {selected, layers[layerIndex], indexOnPage, actualLayersOnPage});
            }
        }
    }
    else
    {
        for (const SelectedDrawingPart& selected : drawingParts)
        {
            drawingTargets.push_back({selected, 0, 0, 1});
        }
    }

    if (drawingTargets.empty())
    {
        AddAutoCreateThreeViewsRunResultLine(u8"失败：没有可出图的选中零件。");
    }

    if (!drawingTargets.empty() &&
        !PreselectManualDirectionsBeforeDrawing(values, drawingTargets))
    {
        WriteProgressFile(requestPath, 0, 0, "Manual direction selection canceled.", true);
        WriteLauncherLog("AutoCreateThreeViews: drawing batch canceled before creating any drawing sheet.");
        return;
    }

    if (!drawingTargets.empty())
    {
        g_uiMonitor.showRunResults =
            layerDrawingMode ||
            drawingTargets.size() > 1 ||
            (drawingTargets.size() == 1 && drawingTargets.front().part.assemblyDrawing);
    }

    int progressIndex = 0;
    const int progressTotal = static_cast<int>(drawingTargets.size());
    bool hasManualFrontDirection = false;
    for (const SelectedLayerDrawingTarget& target : drawingTargets)
    {
        if (IsManualPartFrontDirection(values, target.part.index))
        {
            hasManualFrontDirection = true;
            break;
        }
    }
    if (!hasManualFrontDirection || progressTotal == 0)
    {
        WriteProgressFile(requestPath, 0, progressTotal, progressTotal == 0 ? "No drawable parts." : "Starting drawing...", progressTotal == 0);
    }
    for (const SelectedLayerDrawingTarget& target : drawingTargets)
    {
        const SelectedDrawingPart& selected = target.part;
        const LauncherTimingClock::time_point partStarted = LauncherTimingClock::now();
        ++progressIndex;
        std::string progressPartName = ProgressPartDisplayName(values, selected.index, selected.prototypePart);
        if (target.targetLayer > 0)
        {
            progressPartName += " L" + std::to_string(target.targetLayer);
        }
        const bool manualFrontDirection = IsManualPartFrontDirection(values, selected.index);
        std::ostringstream progressLog;
        progressLog << "AutoCreateThreeViews: drawing progress "
                    << progressIndex << "/" << progressTotal
                    << ", part=" << progressPartName
                    << ", targetLayer=" << target.targetLayer
                    << ".";
        WriteLauncherLog(progressLog.str());
        AddAutoCreateThreeViewsRunResultLine(
            std::string("Progress: ") + std::to_string(progressIndex) + "/" + std::to_string(progressTotal) +
            ", drawing part");
        std::string progressMessage = "Drawing " + progressPartName;
        if (!manualFrontDirection)
        {
            WriteProgressFile(
                requestPath,
                progressIndex,
                progressTotal,
                progressMessage,
                false);
        }

        if (!manualFrontDirection)
        {
            DisplaySelectedOccurrence(selected.occurrence);
        }
        else
        {
            int clearedHighlights = 0;
            UnhighlightKnownOccurrences(clearedHighlights);
            UnhighlightDisplayedAssemblyTree(clearedHighlights);
            g_highlightedOccurrence = NULL_TAG;
            g_knownHighlightedOccurrences.clear();
            WriteLauncherLog("AutoCreateThreeViews: manual front direction skips occurrence highlight before selection.");
        }
        if (!SetDrawingDisplayPart(selected.prototypePart, selected.occurrence))
        {
            AddAutoCreateThreeViewsRunResultLine(u8"失败：无法切换到选中的零件。");
            continue;
        }

        WriteLauncherLog("AutoCreateThreeViews: drawing part is active; defer drafting application entry until the sheet is created.");

        std::filesystem::path partRequestPath = requestPath;
        partRequestPath += ".part";
        partRequestPath += std::to_string(progressIndex);
        partRequestPath += ".request";
        WriteSinglePartRequest(
            partRequestPath,
            values,
            selected.index,
            layerDrawingMode ? false : selected.assemblyDrawing,
            target.targetLayer,
            target.layerIndex,
            target.layersPerSheet);
        ExecuteAutoCreateThreeViewsFromRequest(partRequestPath);
        WriteLauncherTiming(
            "batch_part_total",
            partStarted,
            "index=" + std::to_string(progressIndex) +
                "/" + std::to_string(progressTotal) +
                ", part=" + progressPartName);

        std::error_code ignored;
        std::filesystem::remove(partRequestPath, ignored);
    }

    WriteProgressFile(requestPath, progressTotal, progressTotal, "Drawing finished.", true);
    WriteLauncherTiming(
        "ui_request_total",
        batchStarted,
        "parts=" + std::to_string(progressTotal));
    WriteLauncherLog("AutoCreateThreeViews: keep last drawing part displayed after UI request.");
}

std::unique_ptr<AsyncDrawingBatch> PrepareAsyncDrawingBatch(
    const std::filesystem::path& requestPath)
{
    auto batch = std::make_unique<AsyncDrawingBatch>();
    batch->requestPath = requestPath;
    batch->values = ReadSimpleKeyValueFile(requestPath);
    batch->started = std::chrono::steady_clock::now();
    BeginAutoCreateThreeViewsRunResults();

    const int partCount = ParseIntValue(FindValue(batch->values, "selectedPartCount"), 0);
    batch->layerDrawingMode =
        TrimText(FindValue(batch->values, "drawingTargetMode", "partOrAssembly")) == "partLayers";
    const tag_t previousDisplayPart = UF_PART_ask_display_part();
    const tag_t previousWorkPart = UF_ASSEM_ask_work_part();

    if (partCount <= 0 && !batch->layerDrawingMode)
    {
        batch->directSingleRequest = true;
        WriteProgressFile(requestPath, 0, 1, "Starting drawing...", false);
        return batch;
    }

    std::vector<SelectedDrawingPart> drawingParts;
    if (partCount <= 0)
    {
        tag_t occurrenceTag = NULL_TAG;
        tag_t prototypePart = previousWorkPart != NULL_TAG ? previousWorkPart : previousDisplayPart;
        if (ParseTagValue(FindValue(batch->values, "selectedOccurrenceTag"), occurrenceTag))
        {
            const tag_t occurrencePrototype = PrototypePartFromOccurrence(occurrenceTag);
            if (occurrencePrototype != NULL_TAG)
                prototypePart = occurrencePrototype;
        }
        if (prototypePart != NULL_TAG)
            drawingParts.push_back({0, occurrenceTag, prototypePart, false});
    }
    for (int index = 0; index < partCount; ++index)
    {
        tag_t occurrenceTag = NULL_TAG;
        if (!ParseTagValue(FindPartValue(batch->values, index, "occurrenceTag"), occurrenceTag))
            continue;

        const tag_t prototypePart = PrototypePartFromOccurrence(occurrenceTag);
        if (prototypePart == NULL_TAG)
        {
            AddAutoCreateThreeViewsRunResultLine(
                u8"已选组件无法获取原型部件，已跳过。");
            continue;
        }
        const bool isRootOccurrence = IsRootOccurrence(occurrenceTag, previousDisplayPart);
        drawingParts.push_back(
            {index, occurrenceTag, prototypePart, isRootOccurrence || OccurrenceHasChildren(occurrenceTag)});
    }

    if (batch->layerDrawingMode)
    {
        const std::vector<std::pair<int, int>> ranges =
            ParseLayerRanges(FindValue(batch->values, "layerRange", "1-256"));
        const int layersPerSheet = std::max(
            1,
            ParseIntValue(FindValue(batch->values, "layersPerSheet", "1"), 1));
        std::set<tag_t> seenPrototypeParts;
        for (const SelectedDrawingPart& selected : drawingParts)
        {
            if (!seenPrototypeParts.insert(selected.prototypePart).second)
                continue;
            const std::vector<int> layers = CollectDrawablePartLayers(selected.prototypePart, ranges);
            for (size_t layerIndex = 0; layerIndex < layers.size(); ++layerIndex)
            {
                const size_t pageStart = (layerIndex / static_cast<size_t>(layersPerSheet)) *
                    static_cast<size_t>(layersPerSheet);
                const int actualLayersOnPage = static_cast<int>(std::min(
                    static_cast<size_t>(layersPerSheet),
                    layers.size() - pageStart));
                const int indexOnPage = static_cast<int>(layerIndex - pageStart);
                batch->targets.push_back(
                    {selected,
                     layers[layerIndex],
                     indexOnPage,
                     actualLayersOnPage});
            }
        }
    }
    else
    {
        for (const SelectedDrawingPart& selected : drawingParts)
            batch->targets.push_back({selected, 0, 0, 1});
    }

    if (batch->targets.empty())
        AddAutoCreateThreeViewsRunResultLine(u8"没有找到可用于出图的部件或图层。");

    if (!batch->targets.empty() &&
        !PreselectManualDirectionsBeforeDrawing(batch->values, batch->targets))
    {
        batch->targets.clear();
        AddAutoCreateThreeViewsRunResultLine(
            u8"手动方向选择未完成，未创建任何图纸页。");
    }

    batch->showRunResults =
        batch->layerDrawingMode ||
        batch->targets.size() > 1 ||
        (batch->targets.size() == 1 && batch->targets.front().part.assemblyDrawing);
    WriteProgressFile(
        requestPath,
        0,
        static_cast<int>(batch->targets.size()),
        batch->targets.empty() ? "No drawable parts." : "Starting drawing...",
        batch->targets.empty());
    return batch;
}

bool ProcessNextAsyncDrawingTarget(AsyncDrawingBatch& batch)
{
    if (batch.directSingleRequest)
    {
        if (batch.nextTarget > 0)
            return false;
        batch.nextTarget = 1;
        WriteProgressFile(batch.requestPath, 1, 1, "Drawing current part", false);
        ApplySelectedOccurrenceFromRequest(batch.requestPath);
        ExecuteAutoCreateThreeViewsFromRequest(batch.requestPath);
        return false;
    }

    if (batch.completedSheetNeedsPresentation)
    {
        batch.completedSheetNeedsPresentation = false;
        const tag_t displayPart = UF_PART_ask_display_part();
        const tag_t workPart = UF_ASSEM_ask_work_part();
        const tag_t requestedDrawing = batch.completedDrawingSheetTag;
        batch.completedDrawingSheetTag = NULL_TAG;
        int openDrawingStatus = 0;
        try
        {
            NXOpen::Session* session = NXOpen::Session::GetSession();
            if (requestedDrawing != NULL_TAG)
            {
                NXOpen::TaggedObject* taggedObject = NXOpen::NXObjectManager::Get(requestedDrawing);
                NXOpen::Drawings::DraftingDrawingSheet* exactSheet =
                    dynamic_cast<NXOpen::Drawings::DraftingDrawingSheet*>(taggedObject);
                if (exactSheet != nullptr)
                    exactSheet->Open();
                openDrawingStatus = UF_DRAW_open_drawing(requestedDrawing);
            }
            UF_DISP_make_display_up_to_date();
            if (session != nullptr && session->DisplayManager() != nullptr)
                session->DisplayManager()->MakeUpToDate();
        }
        catch (const NXOpen::NXException& ex)
        {
            WriteLauncherLog(
                "AutoCreateThreeViews: completed-sheet presentation update warning, NX " +
                std::to_string(ex.ErrorCode()) + ", " + ex.Message() + ".");
        }
        catch (...)
        {
            WriteLauncherLog(
                "AutoCreateThreeViews: completed-sheet presentation update warning, unknown exception.");
        }

        tag_t currentDrawing = NULL_TAG;
        UF_DRAW_ask_current_drawing(&currentDrawing);

        WriteLauncherLog(
            "AutoCreateThreeViews: completed drawing sheet presented before next part switch, displayPart=" +
            std::to_string(static_cast<unsigned long long>(displayPart)) +
            ", workPart=" + std::to_string(static_cast<unsigned long long>(workPart)) +
            ", requestedDrawing=" + std::to_string(static_cast<unsigned long long>(requestedDrawing)) +
            ", openStatus=" + std::to_string(openDrawingStatus) +
            ", drawing=" + std::to_string(static_cast<unsigned long long>(currentDrawing)) + ".");
        return batch.nextTarget < batch.targets.size();
    }

    if (batch.nextTarget >= batch.targets.size())
        return false;

    const size_t targetIndex = batch.nextTarget;
    const SelectedLayerDrawingTarget& target = batch.targets[targetIndex];
    const SelectedDrawingPart& selected = target.part;
    const int progressIndex = static_cast<int>(targetIndex + 1);
    const int progressTotal = static_cast<int>(batch.targets.size());
    const LauncherTimingClock::time_point partStarted = LauncherTimingClock::now();
    std::string progressPartName =
        ProgressPartDisplayName(batch.values, selected.index, selected.prototypePart);
    if (target.targetLayer > 0)
        progressPartName += " L" + std::to_string(target.targetLayer);

    if (!batch.targetContextReady)
    {
    WriteLauncherLog(
        "AutoCreateThreeViews: async drawing progress " +
        std::to_string(progressIndex) + "/" + std::to_string(progressTotal) +
        ", part=" + progressPartName + ".");
    WriteProgressFile(
        batch.requestPath,
        progressIndex,
        progressTotal,
        "Drawing " + progressPartName,
        false);

    const bool manualFrontDirection =
        IsManualPartFrontDirection(batch.values, selected.index);
    if (!manualFrontDirection)
    {
        DisplaySelectedOccurrence(selected.occurrence);
    }
    else
    {
        int clearedHighlights = 0;
        UnhighlightKnownOccurrences(clearedHighlights);
        UnhighlightDisplayedAssemblyTree(clearedHighlights);
        g_highlightedOccurrence = NULL_TAG;
        g_knownHighlightedOccurrences.clear();
    }
    }

    const bool contextWasPrepared = batch.targetContextReady;
    const bool requiresPartSwitch =
        !batch.targetContextReady &&
        (UF_PART_ask_display_part() != selected.prototypePart ||
         UF_ASSEM_ask_work_part() != selected.prototypePart);
    if (requiresPartSwitch)
    {
        const LauncherTimingClock::time_point switchStarted = LauncherTimingClock::now();
        if (!SetDrawingDisplayPart(selected.prototypePart, selected.occurrence))
        {
            AddAutoCreateThreeViewsRunResultLine(
                u8"无法切换到目标部件，已跳过该出图项。");
            ++batch.nextTarget;
            return batch.nextTarget < batch.targets.size();
        }
        batch.targetContextReady = true;
        WriteLauncherTiming(
            "async_display_work_part_switch",
            switchStarted,
            "index=" + std::to_string(progressIndex) + "/" + std::to_string(progressTotal) +
                ", part=" + progressPartName);
        WriteLauncherLog(
            "AutoCreateThreeViews: display/work part changed; return to NX before drawing creation.");
        return true;
    }

    const bool drawingPartReady =
        contextWasPrepared || SetDrawingDisplayPart(selected.prototypePart, selected.occurrence);

    if (drawingPartReady)
    {
        std::filesystem::path partRequestPath = batch.requestPath;
        partRequestPath += ".part" + std::to_string(progressIndex) + ".request";

        batch.targetContextReady = false;
        ++batch.nextTarget;
        WriteSinglePartRequest(
            partRequestPath,
            batch.values,
            selected.index,
            batch.layerDrawingMode ? false : selected.assemblyDrawing,
            target.targetLayer,
            target.layerIndex,
            target.layersPerSheet,
            "full");
        ExecuteAutoCreateThreeViewsFromRequest(partRequestPath);
        std::error_code ignored;
        std::filesystem::remove(partRequestPath, ignored);
        batch.completedDrawingSheetTag = AskLastAutoCreateThreeViewsDrawingSheetTag();
        batch.completedSheetNeedsPresentation =
            batch.completedDrawingSheetTag != NULL_TAG;
        if (batch.completedSheetNeedsPresentation)
        {
            WriteLauncherLog(
                std::string("AutoCreateThreeViews: completed ") +
                (batch.layerDrawingMode ? "layer group" : "sheet") +
                " awaits a dedicated NX presentation callback, exactTag=" +
                std::to_string(static_cast<unsigned long long>(batch.completedDrawingSheetTag)) + ".");
            return true;
        }
    }
    else
    {
        batch.targetContextReady = false;
        ++batch.nextTarget;
        AddAutoCreateThreeViewsRunResultLine(
            u8"无法切换到目标部件，已跳过该出图项。");
    }

    WriteLauncherTiming(
        "async_batch_part_total",
        partStarted,
        "index=" + std::to_string(progressIndex) + "/" + std::to_string(progressTotal) +
            ", part=" + progressPartName);
    return batch.nextTarget < batch.targets.size();
}

void FinalizeAsyncDrawingBatch(AsyncDrawingBatch& batch)
{
    const int total = batch.directSingleRequest
        ? 1
        : static_cast<int>(batch.targets.size());
    WriteProgressFile(batch.requestPath, total, total, "Drawing finished.", true);
    WriteLauncherTiming("async_ui_request_total", batch.started, "parts=" + std::to_string(total));
    WriteLauncherLog("AutoCreateThreeViews: async drawing batch completed.");
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
    g_uiMonitor.showRunResults = false;
    g_uiMonitor.active = false;
    g_asyncDrawingBatch.reset();
    CompleteAutoCreateThreeViewsNativeProgress();
}

void ScheduleDeferredMainDllUnload()
{
    const std::filesystem::path helperPath =
        CurrentModuleDirectory() / L"AutoCreateThreeViewsUnloadHelper.dll";
    if (!std::filesystem::exists(helperPath))
    {
        WriteLauncherLog(
            "AutoCreateThreeViews: deferred unload helper is missing, path=" +
            helperPath.string() + ".");
        return;
    }

    UF_load_f_p_t scheduleUnload = nullptr;
    std::string helper = helperPath.string();
    const int loadStatus = UF_load_library(
        helper.data(),
        "ScheduleAutoCreateThreeViewsUnload",
        &scheduleUnload);
    if (loadStatus != 0 || scheduleUnload == nullptr)
    {
        WriteLauncherLog(
            "AutoCreateThreeViews: deferred unload helper load failed, status=" +
            std::to_string(loadStatus) + ".");
        return;
    }

    scheduleUnload();
    WriteLauncherLog("AutoCreateThreeViews: handed main DLL unload to the 500 ms helper.");
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

    if (g_asyncDrawingBatch != nullptr)
    {
        try
        {
            const bool hasMore = ProcessNextAsyncDrawingTarget(*g_asyncDrawingBatch);
            if (!hasMore)
            {
                FinalizeAsyncDrawingBatch(*g_asyncDrawingBatch);
                ClearSelectionAndOccurrenceHighlights(true);
                if (g_asyncDrawingBatch->showRunResults)
                    ShowAutoCreateThreeViewsRunResults();
                std::error_code ignored;
                std::filesystem::remove(g_asyncDrawingBatch->requestPath, ignored);
                CompleteAutoCreateThreeViewsNativeProgress();
                g_asyncDrawingBatch.reset();
                shouldStop = true;
            }
        }
        catch (const std::exception& ex)
        {
            WriteLauncherLog(std::string("AutoCreateThreeViews: async batch exception: ") + ex.what() + ".");
            CompleteAutoCreateThreeViewsNativeProgress();
            g_asyncDrawingBatch.reset();
            shouldStop = true;
        }
        catch (...)
        {
            WriteLauncherLog("AutoCreateThreeViews: async batch unknown exception.");
            CompleteAutoCreateThreeViewsNativeProgress();
            g_asyncDrawingBatch.reset();
            shouldStop = true;
        }

        if (shouldStop)
        {
            StopUiMonitor();
            ScheduleDeferredMainDllUnload();
        }
        UF_terminate();
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
            if (g_uiMonitor.showRunResults)
            {
                ShowAutoCreateThreeViewsRunResults();
            }
            else
            {
                WriteLauncherLog("AutoCreateThreeViews: run result UI skipped for single part drawing.");
            }
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

int ExecuteAutoCreateThreeViewsUiRequest(const std::filesystem::path& requestPath)
{
    ExecuteUiRequestParts(requestPath);
    return 0;
}

int ScheduleAutoCreateThreeViewsUiRequest(const std::filesystem::path& requestPath)
{
    if (g_uiMonitor.active || g_asyncDrawingBatch != nullptr)
    {
        WriteLauncherLog("AutoCreateThreeViews: async drawing schedule rejected; another batch is active.");
        return 1;
    }

    g_asyncDrawingBatch = PrepareAsyncDrawingBatch(requestPath);
    if (g_asyncDrawingBatch == nullptr)
        return 1;

    g_uiMonitor.requestPath = requestPath;
    g_uiMonitor.displayCommandPath.clear();
    g_uiMonitor.processInfo = {};
    g_uiMonitor.processing = false;
    g_uiMonitor.showRunResults = g_asyncDrawingBatch->showRunResults;
    g_uiMonitor.active = true;
    // The refresh phase runs in its own callback.  A modest interval gives NX
    // time to present the completed sheet before work on the next target starts.
    g_uiMonitor.timerId = SetTimer(nullptr, 0, 200, UiMonitorTimerProc);
    if (g_uiMonitor.timerId == 0)
    {
        WriteLauncherLog("AutoCreateThreeViews: async drawing SetTimer failed.");
        StopUiMonitor();
        return 1;
    }

    WriteLauncherLog(
        "AutoCreateThreeViews: async drawing scheduled; each timer callback processes one target.");
    return 0;
}

extern "C" DllExport void ufusr(char* param, int* returnCode, int rlen)
{
    (void)rlen;

    if (!zhihui_license_guard::EnsureAuthorized(L"ZHIHUI.AUTOCREATETHREEVIEWS", L"AutoCreateThreeViews"))
    {
        return;
    }

    WriteLauncherLog(
        std::string("AutoCreateThreeViews: DLL entry, build ") + __DATE__ + " " + __TIME__ +
        ", unload policy=native immediate or retained during asynchronous drawing.");

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
        const std::string rawParam = param != nullptr ? std::string(param) : std::string();
        const std::string requestPrefix = "request=";
        if (rawParam.rfind(requestPrefix, 0) == 0)
        {
            const std::filesystem::path requestPath = rawParam.substr(requestPrefix.size());
            WriteLauncherLog("AutoCreateThreeViews: direct request parameter received, path=" + requestPath.string() + ".");
            const std::map<std::string, std::string> requestValues = ReadSimpleKeyValueFile(requestPath);
            const int partCount = ParseIntValue(FindValue(requestValues, "selectedPartCount"), 0);
            int status = 0;
            if (partCount > 0)
            {
                ExecuteUiRequestParts(requestPath);
            }
            else
            {
                ApplySelectedOccurrenceFromRequest(requestPath);
                status = ExecuteAutoCreateThreeViewsFromRequest(requestPath);
                WriteLauncherLog("AutoCreateThreeViews: keep drawing part displayed after direct request.");
            }
            if (returnCode != nullptr)
            {
                *returnCode = status;
            }
            UF_terminate();
            return;
        }

        bool nativeDialogFailed = false;
        try
        {
            AutoCreateThreeViewsDialog dialog;
            dialog.Launch();
            WriteLauncherLog("AutoCreateThreeViews: UG native dialog closed.");
            if (dialog.HasPendingDrawing())
            {
                WriteLauncherLog("AutoCreateThreeViews: execute drawing after native dialog returned.");
                const int drawingStatus = dialog.ExecutePendingDrawing();
                if (returnCode != nullptr)
                    *returnCode = drawingStatus;
            }
        }
        catch (const NXOpen::NXException& ex)
        {
            nativeDialogFailed = true;
            WriteLauncherLog(
                std::string("AutoCreateThreeViews: UG native dialog failed, NX ") +
                std::to_string(ex.ErrorCode()) + ", " + ex.Message() + ".");
        }
        catch (const std::exception& ex)
        {
            nativeDialogFailed = true;
            WriteLauncherLog(
                std::string("AutoCreateThreeViews: UG native dialog failed: ") + ex.what() + ".");
        }
        catch (...)
        {
            nativeDialogFailed = true;
            WriteLauncherLog("AutoCreateThreeViews: UG native dialog failed with unknown exception.");
        }

        if (nativeDialogFailed)
        {
            throw std::runtime_error("UG native dialog failed to launch.");
        }
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
    if (g_uiMonitor.active)
    {
        WriteLauncherLog(
            "AutoCreateThreeViews: ufusr_ask_unload called while asynchronous drawing is active; "
            "return UF_UNLOAD_SEL_DIALOG.");
        return UF_UNLOAD_SEL_DIALOG;
    }

    WriteLauncherLog(
        "AutoCreateThreeViews: ufusr_ask_unload called after native dialog closed; "
        "return UF_UNLOAD_IMMEDIATELY for hot replacement.");
    return UF_UNLOAD_IMMEDIATELY;
}

extern "C" DllExport void ufusr_cleanup(void)
{
    WriteLauncherLog(
        std::string("AutoCreateThreeViews: ufusr_cleanup begin, monitorActive=") +
        (g_uiMonitor.active ? "true" : "false") + ".");
    StopUiMonitor();
    ClearSelectionAndOccurrenceHighlights(false);
    WriteLauncherLog("AutoCreateThreeViews: ufusr_cleanup completed; DLL is ready to unload.");
}
