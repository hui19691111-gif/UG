#include <NXOpen/Assemblies_Component.hxx>
#include <NXOpen/Assemblies_ComponentAssembly.hxx>
#include <NXOpen/Annotations_AnnotationManager.hxx>
#include <NXOpen/Annotations_AssociativeText.hxx>
#include <NXOpen/BasePart.hxx>
#include <NXOpen/Body.hxx>
#include <NXOpen/BodyCollection.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/Session.hxx>

#include <uf.h>
#include <uf_assem.h>
#include <uf_attr.h>
#include <uf_defs.h>
#include <uf_obj.h>
#include <uf_object_types.h>
#include <uf_part.h>
#include <uf_tabnot.h>
#include <uf_ui.h>
#include <uf_ui_types.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace
{
    constexpr size_t kAttributeColumnCount = 8;
    constexpr const char* kBodyNameAttributeName = "名称";
    constexpr int kExcludedDuplicateBodyLayer = 271;

    struct ColumnDef
    {
        std::string attributeName;
        std::string headerTitle;
    };

    struct TableOptions
    {
        bool headerBelow = false;
        double textHeight = 3.5;
    };

    struct BodyValueRow
    {
        tag_t bodyTag = NULL_TAG;
        std::string bodyName;
        std::vector<std::string> values;
    };

    struct ManualRowValues
    {
        std::string bodyName;
        std::vector<std::string> values;
    };

    struct BodyRecord
    {
        tag_t tag = NULL_TAG;
        tag_t attributeTag = NULL_TAG;
        std::string displayName;
        bool fromAssemblyComponent = false;
    };

    std::string Utf8ToSystem(const std::string& utf8)
    {
        if (utf8.empty())
        {
            return {};
        }
        const int wideSize = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
        if (wideSize <= 0)
        {
            return utf8;
        }
        std::wstring wide(static_cast<size_t>(wideSize), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, wide.data(), wideSize);
        const int ansiSize = WideCharToMultiByte(CP_ACP, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (ansiSize <= 0)
        {
            return utf8;
        }
        std::string ansi(static_cast<size_t>(ansiSize), '\0');
        WideCharToMultiByte(CP_ACP, 0, wide.c_str(), -1, ansi.data(), ansiSize, nullptr, nullptr);
        if (!ansi.empty() && ansi.back() == '\0')
        {
            ansi.pop_back();
        }
        return ansi;
    }

    std::string WideToUtf8(const wchar_t* text)
    {
        if (text == nullptr || text[0] == L'\0')
        {
            return {};
        }
        const int utf8Size = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
        if (utf8Size <= 0)
        {
            return {};
        }
        std::string utf8(static_cast<size_t>(utf8Size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text, -1, utf8.data(), utf8Size, nullptr, nullptr);
        if (!utf8.empty() && utf8.back() == '\0')
        {
            utf8.pop_back();
        }
        return utf8;
    }

    bool IsValidUtf8(const std::string& text)
    {
        int expectedContinuation = 0;
        for (unsigned char ch : text)
        {
            if (expectedContinuation == 0)
            {
                if ((ch & 0x80) == 0)
                {
                    continue;
                }
                if ((ch & 0xE0) == 0xC0)
                {
                    expectedContinuation = 1;
                }
                else if ((ch & 0xF0) == 0xE0)
                {
                    expectedContinuation = 2;
                }
                else if ((ch & 0xF8) == 0xF0)
                {
                    expectedContinuation = 3;
                }
                else
                {
                    return false;
                }
            }
            else if ((ch & 0xC0) == 0x80)
            {
                --expectedContinuation;
            }
            else
            {
                return false;
            }
        }
        return expectedContinuation == 0;
    }

    std::string SystemToUtf8(const char* text)
    {
        if (text == nullptr || text[0] == '\0')
        {
            return {};
        }
        std::string raw(text);
        if (IsValidUtf8(raw))
        {
            return raw;
        }

        int wideSize = MultiByteToWideChar(CP_ACP, 0, text, -1, nullptr, 0);
        if (wideSize <= 0)
        {
            return raw;
        }
        std::wstring wide(static_cast<size_t>(wideSize), L'\0');
        MultiByteToWideChar(CP_ACP, 0, text, -1, wide.data(), wideSize);

        int utf8Size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (utf8Size <= 0)
        {
            return raw;
        }
        std::string utf8(static_cast<size_t>(utf8Size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, utf8.data(), utf8Size, nullptr, nullptr);
        if (!utf8.empty() && utf8.back() == '\0')
        {
            utf8.pop_back();
        }
        return utf8;
    }

    std::string Trim(const std::string& text)
    {
        size_t begin = 0;
        while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin])))
        {
            ++begin;
        }
        size_t end = text.size();
        while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])))
        {
            --end;
        }
        return text.substr(begin, end - begin);
    }

    std::string ToLowerAscii(std::string text)
    {
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        return text;
    }

    std::string ToUtf8(const NXOpen::NXString& value)
    {
        const char* utf8 = value.GetUTF8Text();
        if (utf8 != nullptr && utf8[0] != '\0')
        {
            return utf8;
        }
        const char* locale = value.GetLocaleText();
        return locale != nullptr ? locale : std::string();
    }

    NXOpen::NXString Utf8NxString(const std::string& text)
    {
        return NXOpen::NXString(text.c_str(), NXOpen::NXString::UTF8);
    }

    NXOpen::NXString Utf8NxString(const char* text)
    {
        return NXOpen::NXString(text == nullptr ? "" : text, NXOpen::NXString::UTF8);
    }

    std::filesystem::path CurrentModuleDirectory()
    {
        wchar_t buffer[MAX_PATH] = {};
        const HMODULE module = reinterpret_cast<HMODULE>(&__ImageBase);
        const DWORD length = GetModuleFileNameW(module, buffer, MAX_PATH);
        if (length == 0 || length >= MAX_PATH)
        {
            return std::filesystem::current_path();
        }
        return std::filesystem::path(buffer).parent_path();
    }

    std::string NowText()
    {
        SYSTEMTIME now = {};
        GetLocalTime(&now);
        std::ostringstream out;
        out << std::setfill('0')
            << now.wYear << '-'
            << std::setw(2) << now.wMonth << '-'
            << std::setw(2) << now.wDay << ' '
            << std::setw(2) << now.wHour << ':'
            << std::setw(2) << now.wMinute << ':'
            << std::setw(2) << now.wSecond << '.'
            << std::setw(3) << now.wMilliseconds;
        return out.str();
    }

    void LogBodyScan(const std::string& message)
    {
        try
        {
            std::ofstream out(CurrentModuleDirectory() / "MinXiBiao.body-scan.log", std::ios::app | std::ios::binary);
            out << NowText() << " " << message << "\r\n";
        }
        catch (...)
        {
        }
    }

    std::string PartInfo(NXOpen::Part* part)
    {
        if (part == nullptr)
        {
            return "<null>";
        }
        std::ostringstream out;
        out << "tag=" << part->Tag();
        try
        {
            out << ", name=" << ToUtf8(part->Name());
        }
        catch (const std::exception& ex)
        {
            out << ", name-ex=" << ex.what();
        }
        catch (...)
        {
            out << ", name-ex=<unknown>";
        }
        try
        {
            out << ", fullPath=" << ToUtf8(part->FullPath());
        }
        catch (const std::exception& ex)
        {
            out << ", fullPath-ex=" << ex.what();
        }
        catch (...)
        {
            out << ", fullPath-ex=<unknown>";
        }
        return out.str();
    }

    void ShowNxMessage(const std::string& utf8Message)
    {
        std::string systemMessage = Utf8ToSystem(utf8Message);
        if (systemMessage.empty())
        {
            return;
        }
        uc1601(systemMessage.c_str(), 1);
    }

    void ShowNxMessage(const char* message)
    {
        ShowNxMessage(std::string(message == nullptr ? "" : message));
    }

    std::string FormatRealValue(double value)
    {
        std::ostringstream out;
        out << std::fixed << std::setprecision(1) << value;
        return out.str();
    }

    bool TryParsePlainDecimalText(const std::string& text, double& value)
    {
        const std::string trimmed = Trim(text);
        if (trimmed.empty() || trimmed.find('.') == std::string::npos)
        {
            return false;
        }

        char* end = nullptr;
        value = std::strtod(trimmed.c_str(), &end);
        return end != nullptr && *end == '\0';
    }

    std::string FormatDecimalTextIfNeeded(const std::string& text)
    {
        double value = 0.0;
        if (!TryParsePlainDecimalText(text, value))
        {
            return text;
        }
        return FormatRealValue(value);
    }

    std::string ToStringValue(const NXOpen::NXObject::AttributeInformation& info)
    {
        using Type = NXOpen::NXObject::AttributeType;
        switch (info.Type)
        {
        case Type::AttributeTypeString:
            return ToUtf8(info.StringValue);
        case Type::AttributeTypeInteger:
            return std::to_string(info.IntegerValue);
        case Type::AttributeTypeReal:
            return FormatRealValue(info.RealValue);
        case Type::AttributeTypeTime:
            return ToUtf8(info.TimeValue);
        case Type::AttributeTypeBoolean:
            return info.BooleanValue ? "True" : "False";
        case Type::AttributeTypeNull:
            return {};
        default:
            return ToUtf8(info.StringValue);
        }
    }

    bool HasMeaningfulValue(const NXOpen::NXObject::AttributeInformation& info)
    {
        const std::string text = Trim(ToStringValue(info));
        return !text.empty();
    }

    std::map<std::string, std::string> CollectBodyAttributeMap(tag_t bodyTag);
    bool IsFlatPatternGeneratedBody(tag_t bodyTag);

    tag_t UniqueBodyKey(tag_t bodyTag)
    {
        return bodyTag;
    }

    void AddBodyIfMissing(std::vector<BodyRecord>& bodies, tag_t bodyTag, std::set<tag_t>& seen)
    {
        const tag_t uniqueKey = UniqueBodyKey(bodyTag);
        if (bodyTag == NULL_TAG || uniqueKey == NULL_TAG || !seen.insert(uniqueKey).second)
        {
            return;
        }
        if (IsFlatPatternGeneratedBody(bodyTag))
        {
            LogBodyScan("skip flat pattern generated body tag=" + std::to_string(bodyTag));
            return;
        }
        BodyRecord record;
        record.tag = bodyTag;
        record.attributeTag = bodyTag;
        bodies.push_back(record);
    }

    bool IsSolidBodyTag(tag_t objectTag)
    {
        if (objectTag == NULL_TAG)
        {
            return false;
        }

        int type = 0;
        int subtype = 0;
        if (UF_OBJ_ask_type_and_subtype(objectTag, &type, &subtype) != 0)
        {
            return false;
        }
        return type == UF_solid_type && subtype == UF_solid_body_subtype;
    }

    bool TryAskBodyDisplayProperties(tag_t bodyTag, UF_OBJ_disp_props_t& props)
    {
        props = {};
        return bodyTag != NULL_TAG && UF_OBJ_ask_display_properties(bodyTag, &props) == 0;
    }

    bool IsExcludedDuplicateLayerBody(tag_t bodyTag)
    {
        UF_OBJ_disp_props_t props = {};
        return TryAskBodyDisplayProperties(bodyTag, props) && props.layer == kExcludedDuplicateBodyLayer;
    }

    bool IsEligibleBodyTag(tag_t bodyTag)
    {
        return IsSolidBodyTag(bodyTag) && !IsFlatPatternGeneratedBody(bodyTag) && !IsExcludedDuplicateLayerBody(bodyTag);
    }

    std::string DescribeBodyTagForLog(tag_t bodyTag)
    {
        int type = 0;
        int subtype = 0;
        UF_OBJ_ask_type_and_subtype(bodyTag, &type, &subtype);

        UF_OBJ_disp_props_t props = {};
        const bool hasDisplayProperties = TryAskBodyDisplayProperties(bodyTag, props);

        char name[MAX_LINE_BUFSIZE] = {};
        std::string bodyName;
        if (UF_OBJ_ask_name(bodyTag, name) == 0)
        {
            bodyName = Trim(SystemToUtf8(name));
        }

        std::ostringstream text;
        text << "tag=" << bodyTag
             << " type=" << type
             << " subtype=" << subtype;
        if (hasDisplayProperties)
        {
            text << " layer=" << props.layer
                 << " color=" << props.color
                 << " blank=" << props.blank_status;
        }
        else
        {
            text << " displayStatus=failed";
        }
        if (!bodyName.empty())
        {
            text << " name=" << bodyName;
        }
        return text.str();
    }

    void AddBodyTagIfMissing(std::vector<BodyRecord>& bodies, tag_t bodyTag, std::set<tag_t>& seen, const char* source)
    {
        if (!IsSolidBodyTag(bodyTag))
        {
            int type = 0;
            int subtype = 0;
            UF_OBJ_ask_type_and_subtype(bodyTag, &type, &subtype);
            LogBodyScan(std::string(source == nullptr ? "UF" : source) + ": skip tag=" + std::to_string(bodyTag) +
                        " type=" + std::to_string(type) + " subtype=" + std::to_string(subtype));
            return;
        }
        if (IsExcludedDuplicateLayerBody(bodyTag))
        {
            LogBodyScan(std::string(source == nullptr ? "UF" : source) + ": skip excluded layer body " + DescribeBodyTagForLog(bodyTag));
            return;
        }

        const size_t before = bodies.size();
        AddBodyIfMissing(bodies, bodyTag, seen);
        LogBodyScan(std::string(source == nullptr ? "UF" : source) + ": body " + DescribeBodyTagForLog(bodyTag) +
                    " added=" + (bodies.size() > before ? "yes" : "no"));
    }

    std::string PartDisplayName(NXOpen::Part* part)
    {
        if (part == nullptr)
        {
            return {};
        }
        try
        {
            const std::string name = Trim(ToUtf8(part->Name()));
            if (!name.empty())
            {
                return name;
            }
        }
        catch (...)
        {
        }
        try
        {
            const std::filesystem::path fullPath(ToUtf8(part->FullPath()));
            const std::string stem = fullPath.stem().u8string();
            if (!stem.empty())
            {
                return stem;
            }
        }
        catch (...)
        {
        }
        return {};
    }

    void AddComponentRecordIfMissing(std::vector<BodyRecord>& records, NXOpen::Assemblies::Component* component, std::set<tag_t>& seen)
    {
        if (component == nullptr)
        {
            return;
        }

        try
        {
            NXOpen::Part* prototypePart = dynamic_cast<NXOpen::Part*>(component->Prototype());
            if (prototypePart == nullptr)
            {
                LogBodyScan("assembly component skipped: prototype is not part");
                return;
            }

            const tag_t uniqueKey = component->Tag();
            if (uniqueKey == NULL_TAG || !seen.insert(uniqueKey).second)
            {
                return;
            }

            BodyRecord record;
            record.tag = component->Tag();
            record.attributeTag = prototypePart->Tag();
            record.displayName = PartDisplayName(prototypePart);
            record.fromAssemblyComponent = true;
            records.push_back(record);
            LogBodyScan("assembly component added componentTag=" + std::to_string(record.tag) +
                        " partTag=" + std::to_string(record.attributeTag) +
                        " name=" + record.displayName);
        }
        catch (const std::exception& ex)
        {
            LogBodyScan(std::string("AddComponentRecordIfMissing exception ") + ex.what());
        }
        catch (...)
        {
            LogBodyScan("AddComponentRecordIfMissing unknown exception");
        }
    }

    void CollectComponentRecords(NXOpen::Assemblies::Component* component, std::vector<BodyRecord>& records, std::set<tag_t>& seen)
    {
        if (component == nullptr)
        {
            return;
        }

        std::vector<NXOpen::Assemblies::Component*> children = component->GetChildren();
        if (children.empty())
        {
            AddComponentRecordIfMissing(records, component, seen);
            return;
        }

        for (NXOpen::Assemblies::Component* child : children)
        {
            CollectComponentRecords(child, records, seen);
        }
    }

    size_t CollectBodiesFromPart(NXOpen::Part* part, std::vector<BodyRecord>& bodies, std::set<tag_t>& seen, const char* source)
    {
        const size_t before = bodies.size();
        if (part == nullptr || part->Bodies() == nullptr)
        {
            LogBodyScan(std::string(source == nullptr ? "part" : source) + ": part/bodies null " + PartInfo(part));
            return 0;
        }

        try
        {
            size_t iterated = 0;
            for (NXOpen::BodyCollection::iterator it = part->Bodies()->begin(); it != part->Bodies()->end(); ++it)
            {
                ++iterated;
                NXOpen::Body* body = *it;
                AddBodyTagIfMissing(bodies, body != nullptr ? body->Tag() : NULL_TAG, seen, source);
            }
            LogBodyScan(std::string(source == nullptr ? "part" : source) + ": " + PartInfo(part) +
                        ", iterated=" + std::to_string(iterated) +
                        ", added=" + std::to_string(bodies.size() - before));
        }
        catch (const std::exception& ex)
        {
            LogBodyScan(std::string(source == nullptr ? "part" : source) + ": exception " + PartInfo(part) + " " + ex.what());
        }
        catch (...)
        {
            LogBodyScan(std::string(source == nullptr ? "part" : source) + ": unknown exception " + PartInfo(part));
        }
        return bodies.size() - before;
    }

    void CollectBodiesFromComponent(NXOpen::Assemblies::Component* component, std::vector<BodyRecord>& bodies, std::set<tag_t>& seen)
    {
        if (component == nullptr)
        {
            return;
        }

        NXOpen::Part* prototypePart = dynamic_cast<NXOpen::Part*>(component->Prototype());
        if (prototypePart != nullptr && prototypePart->Bodies() != nullptr)
        {
            for (NXOpen::BodyCollection::iterator it = prototypePart->Bodies()->begin(); it != prototypePart->Bodies()->end(); ++it)
            {
                NXOpen::NXObject* occurrenceObject = component->FindOccurrence(*it);
                NXOpen::Body* occurrenceBody = dynamic_cast<NXOpen::Body*>(occurrenceObject);
                NXOpen::Body* body = occurrenceBody != nullptr ? occurrenceBody : *it;
                AddBodyTagIfMissing(bodies, body != nullptr ? body->Tag() : NULL_TAG, seen, "NXOpen component");
            }
        }

        std::vector<NXOpen::Assemblies::Component*> children = component->GetChildren();
        for (NXOpen::Assemblies::Component* child : children)
        {
            CollectBodiesFromComponent(child, bodies, seen);
        }
    }

    void CollectBodiesFromPartOccurrence(tag_t occurrence, std::vector<BodyRecord>& bodies, std::set<tag_t>& seen)
    {
        if (occurrence == NULL_TAG)
        {
            return;
        }

        tag_t objectOccurrence = NULL_TAG;
        while ((objectOccurrence = UF_ASSEM_cycle_ents_in_part_occ(occurrence, objectOccurrence)) != NULL_TAG)
        {
            AddBodyTagIfMissing(bodies, objectOccurrence, seen, "UF occurrence entity");
        }

        const tag_t prototypeTag = UF_ASSEM_ask_prototype_of_occ(occurrence);
        if (prototypeTag != NULL_TAG)
        {
            tag_t prototypeBody = NULL_TAG;
            while (UF_OBJ_cycle_objs_in_part(prototypeTag, UF_solid_type, &prototypeBody) == 0 &&
                   prototypeBody != NULL_TAG)
            {
                const tag_t mappedOccurrence = UF_ASSEM_find_occurrence(occurrence, prototypeBody);
                AddBodyTagIfMissing(bodies, mappedOccurrence != NULL_TAG ? mappedOccurrence : prototypeBody, seen, "UF occurrence prototype");
            }
        }

        int childCount = 0;
        tag_t* children = nullptr;
        childCount = UF_ASSEM_ask_part_occ_children(occurrence, &children);
        if (childCount > 0 && children != nullptr)
        {
            for (int index = 0; index < childCount; ++index)
            {
                CollectBodiesFromPartOccurrence(children[index], bodies, seen);
            }
        }
        if (children != nullptr)
        {
            UF_free(children);
        }
    }

    void CollectUfAssemblyBodiesFromPartTag(tag_t partTag, std::vector<BodyRecord>& bodies, std::set<tag_t>& seen)
    {
        if (partTag == NULL_TAG)
        {
            return;
        }

        try
        {
            tag_t objectTag = NULL_TAG;
            while (UF_OBJ_cycle_objs_in_part(partTag, UF_solid_type, &objectTag) == 0 && objectTag != NULL_TAG)
            {
                AddBodyTagIfMissing(bodies, objectTag, seen, "UF part solid cycle");
            }

            const tag_t rootOccurrence = UF_ASSEM_ask_root_part_occ(partTag);
            if (rootOccurrence == NULL_TAG)
            {
                return;
            }

            int childCount = 0;
            tag_t* children = nullptr;
            childCount = UF_ASSEM_ask_part_occ_children(rootOccurrence, &children);
            if (childCount > 0 && children != nullptr)
            {
                for (int index = 0; index < childCount; ++index)
                {
                    CollectBodiesFromPartOccurrence(children[index], bodies, seen);
                }
            }
            if (children != nullptr)
            {
                UF_free(children);
            }
        }
        catch (const std::exception& ex)
        {
            LogBodyScan("CollectUfAssemblyBodiesFromPartTag exception partTag=" + std::to_string(partTag) + " " + ex.what());
        }
        catch (...)
        {
            LogBodyScan("CollectUfAssemblyBodiesFromPartTag unknown exception partTag=" + std::to_string(partTag));
        }
    }

    void CollectBodiesFromLoadedParts(NXOpen::PartCollection* parts, std::vector<BodyRecord>& bodies, std::set<tag_t>& seen)
    {
        if (parts == nullptr)
        {
            return;
        }

        try
        {
            for (NXOpen::PartCollection::iterator it = parts->begin(); it != parts->end(); ++it)
            {
                NXOpen::Part* part = dynamic_cast<NXOpen::Part*>(*it);
                CollectBodiesFromPart(part, bodies, seen, "loaded part");
            }
        }
        catch (const std::exception& ex)
        {
            LogBodyScan(std::string("loaded parts iterator exception ") + ex.what());
        }
        catch (...)
        {
            LogBodyScan("loaded parts iterator unknown exception");
        }
    }

    std::vector<BodyRecord> CollectWorkPartBodies()
    {
        std::vector<BodyRecord> bodies;
        std::set<tag_t> seen;
        NXOpen::PartCollection* sessionParts = nullptr;
        LogBodyScan("---- CollectWorkPartBodies begin ----");

        try
        {
            NXOpen::Session* session = NXOpen::Session::GetSession();
            sessionParts = session != nullptr ? session->Parts() : nullptr;
            NXOpen::Part* workPart = sessionParts != nullptr ? sessionParts->Work() : nullptr;
            NXOpen::Part* displayPart = sessionParts != nullptr ? sessionParts->Display() : nullptr;
            LogBodyScan(std::string("session=") + (session != nullptr ? "yes" : "no") +
                        ", parts=" + (sessionParts != nullptr ? "yes" : "no"));
            LogBodyScan("workPart: " + PartInfo(workPart));
            LogBodyScan("displayPart: " + PartInfo(displayPart));

            NXOpen::Part* assemblyPart = displayPart != nullptr ? displayPart : workPart;
            if (assemblyPart != nullptr && assemblyPart->ComponentAssembly() != nullptr)
            {
                NXOpen::Assemblies::Component* root = assemblyPart->ComponentAssembly()->RootComponent();
                LogBodyScan(std::string("assembly root=") + (root != nullptr ? "yes" : "no") + " part=" + PartInfo(assemblyPart));
                if (root != nullptr)
                {
                    std::vector<NXOpen::Assemblies::Component*> children = root->GetChildren();
                    LogBodyScan("assembly child count=" + std::to_string(children.size()));
                    if (!children.empty())
                    {
                        std::set<tag_t> componentSeen;
                        for (NXOpen::Assemblies::Component* child : children)
                        {
                            CollectComponentRecords(child, bodies, componentSeen);
                        }
                        if (!bodies.empty())
                        {
                            LogBodyScan("return assembly component records count=" + std::to_string(bodies.size()));
                            return bodies;
                        }
                    }
                }
            }

            CollectBodiesFromPart(workPart, bodies, seen, "NXOpen workPart");
            if (displayPart != workPart)
            {
                CollectBodiesFromPart(displayPart, bodies, seen, "NXOpen displayPart");
            }
        }
        catch (const std::exception& ex)
        {
            LogBodyScan(std::string("NXOpen collection exception ") + ex.what());
        }
        catch (...)
        {
            LogBodyScan("NXOpen collection unknown exception");
        }

        if (!bodies.empty())
        {
            LogBodyScan("return after NXOpen count=" + std::to_string(bodies.size()));
            return bodies;
        }

        const tag_t displayPartTag = UF_PART_ask_display_part();
        const tag_t workPartTag = UF_ASSEM_ask_work_part();
        LogBodyScan("UF displayPartTag=" + std::to_string(displayPartTag) + ", workPartTag=" + std::to_string(workPartTag));
        CollectUfAssemblyBodiesFromPartTag(displayPartTag, bodies, seen);
        if (workPartTag != displayPartTag)
        {
            CollectUfAssemblyBodiesFromPartTag(workPartTag, bodies, seen);
        }

        if (bodies.empty())
        {
            LogBodyScan("try loaded parts fallback");
            CollectBodiesFromLoadedParts(sessionParts, bodies, seen);
        }

        LogBodyScan("CollectWorkPartBodies end count=" + std::to_string(bodies.size()));
        return bodies;
    }

    std::vector<BodyRecord> FilterEligibleBodies(const std::vector<BodyRecord>& bodies, const char* source)
    {
        std::vector<BodyRecord> filtered;
        std::set<tag_t> seen;
        for (const BodyRecord& body : bodies)
        {
            if (body.tag == NULL_TAG)
            {
                continue;
            }
            if (body.fromAssemblyComponent)
            {
                if (seen.insert(body.tag).second)
                {
                    filtered.push_back(body);
                }
                continue;
            }
            if (!IsEligibleBodyTag(body.tag))
            {
                int type = 0;
                int subtype = 0;
                UF_OBJ_ask_type_and_subtype(body.tag, &type, &subtype);
                LogBodyScan(std::string(source == nullptr ? "filter" : source) + ": remove tag=" + std::to_string(body.tag) +
                            " type=" + std::to_string(type) + " subtype=" + std::to_string(subtype));
                continue;
            }
            if (seen.insert(body.tag).second)
            {
                filtered.push_back(body);
            }
        }
        LogBodyScan(std::string(source == nullptr ? "filter" : source) + ": input=" + std::to_string(bodies.size()) +
                    " output=" + std::to_string(filtered.size()));
        return filtered;
    }

    std::vector<std::string> CollectAttributeNames(const std::vector<BodyRecord>& bodies)
    {
        std::set<std::string> names;
        names.insert(kBodyNameAttributeName);
        for (const BodyRecord& body : bodies)
        {
            const auto values = CollectBodyAttributeMap(body.attributeTag != NULL_TAG ? body.attributeTag : body.tag);
            for (const auto& item : values)
            {
                names.insert(item.first);
            }
        }
        return std::vector<std::string>(names.begin(), names.end());
    }

    std::string JsonEscape(const std::string& text)
    {
        std::ostringstream out;
        for (unsigned char ch : text)
        {
            switch (ch)
            {
            case '\\': out << "\\\\"; break;
            case '"': out << "\\\""; break;
            case '\b': out << "\\b"; break;
            case '\f': out << "\\f"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (ch < 0x20)
                {
                    out << "\\u";
                    const char* hex = "0123456789abcdef";
                    out << '0' << '0' << hex[(ch >> 4) & 0x0F] << hex[ch & 0x0F];
                }
                else
                {
                    out << static_cast<char>(ch);
                }
                break;
            }
        }
        return out.str();
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

    std::string ToStringValue(const UF_ATTR_info_t& info)
    {
        if (info.string_value != nullptr && Trim(SystemToUtf8(info.string_value)).size() > 0)
        {
            return Trim(SystemToUtf8(info.string_value));
        }

        switch (info.type)
        {
        case UF_ATTR_bool:
            return info.bool_value ? "True" : "False";
        case UF_ATTR_integer:
            return std::to_string(info.integer_value);
        case UF_ATTR_real:
            return FormatRealValue(info.real_value);
        case UF_ATTR_time:
            return info.time_string != nullptr ? SystemToUtf8(info.time_string) : std::string();
        case UF_ATTR_string:
        case UF_ATTR_reference:
            return info.string_value != nullptr ? SystemToUtf8(info.string_value) : std::string();
        default:
            return {};
        }
    }

    std::map<std::string, std::string> CollectBodyAttributeMap(tag_t bodyTag)
    {
        std::map<std::string, std::string> values;
        if (bodyTag == NULL_TAG)
        {
            return values;
        }

        UF_ATTR_iterator_t iter;
        if (UF_ATTR_init_user_attribute_iterator(&iter) != 0)
        {
            return values;
        }
        iter.type = UF_ATTR_any;
        iter.include_also_unset = false;
        iter.include_only_unset = false;

        int count = 0;
        UF_ATTR_info_t* infos = nullptr;
        const int status = UF_ATTR_get_user_attributes(bodyTag, &iter, &count, &infos);
        if (status == 0 && infos != nullptr)
        {
            for (int i = 0; i < count; ++i)
            {
                if (infos[i].unset || infos[i].title == nullptr)
                {
                    continue;
                }
                const std::string key = Trim(SystemToUtf8(infos[i].title));
                const std::string value = Trim(FormatDecimalTextIfNeeded(ToStringValue(infos[i])));
                if (!key.empty() && !value.empty())
                {
                    values[key] = value;
                }
            }
            UF_ATTR_free_user_attribute_info_array(count, infos);
        }
        else if (status != 0)
        {
            LogBodyScan("UF_ATTR_get_user_attributes failed tag=" + std::to_string(bodyTag) + " status=" + std::to_string(status));
        }
        UF_ATTR_release_user_attribute_iterator(&iter);
        return values;
    }

    bool LooksLikeFlatPatternText(const std::string& text)
    {
        const std::string lowered = ToLowerAscii(text);
        return lowered.find("flat pattern") != std::string::npos ||
               lowered.find("flat_pattern") != std::string::npos ||
               lowered.find("flat-pattern") != std::string::npos ||
               lowered.find("flatpattern") != std::string::npos ||
               lowered.find("unbend") != std::string::npos ||
               text.find("展平") != std::string::npos ||
               text.find("展开") != std::string::npos;
    }

    bool IsFlatPatternGeneratedBody(tag_t bodyTag)
    {
        (void)bodyTag;
        return false;
    }

    std::string MakeBodyRecordName(size_t bodyIndex)
    {
        return "Body" + std::to_string(bodyIndex + 1);
    }

    std::string ReadBodyDisplayName(const BodyRecord& body, size_t bodyIndex)
    {
        if (!Trim(body.displayName).empty())
        {
            return Trim(body.displayName);
        }
        if (body.tag != NULL_TAG)
        {
            char name[MAX_LINE_BUFSIZE] = {};
            if (UF_OBJ_ask_name(body.tag, name) == 0)
            {
                const std::string text = Trim(SystemToUtf8(name));
                if (!text.empty())
                {
                    return text;
                }
            }
        }
        return MakeBodyRecordName(bodyIndex);
    }

    bool WriteWpfInputFile(const std::filesystem::path& path, const std::vector<BodyRecord>& bodies, const std::vector<std::string>& attributes)
    {
        std::ofstream out(path, std::ios::binary);
        if (!out)
        {
            return false;
        }

        out << "{\n  \"attributes\": [";
        for (size_t i = 0; i < attributes.size(); ++i)
        {
            if (i > 0) out << ", ";
            out << "\"" << JsonEscape(attributes[i]) << "\"";
        }
        out << "],\n  \"selectedColumns\": [";
        for (size_t i = 0; i < kAttributeColumnCount; ++i)
        {
            if (i > 0) out << ", ";
            out << "\"\"";
        }
        out << "],\n  \"bodies\": [\n";

        bool firstBody = true;
        size_t bodyIndex = 0;
        for (const BodyRecord& body : bodies)
        {
            const auto values = CollectBodyAttributeMap(body.attributeTag != NULL_TAG ? body.attributeTag : body.tag);
            const std::string bodyDisplayName = ReadBodyDisplayName(body, bodyIndex);
            if (!firstBody) out << ",\n";
            firstBody = false;
            out << "    {\"name\": \"" << MakeBodyRecordName(bodyIndex++) << "\", \"attributes\": {";
            bool firstAttr = false;
            out << "\"" << JsonEscape(kBodyNameAttributeName) << "\": \"" << JsonEscape(bodyDisplayName) << "\"";
            for (const auto& item : values)
            {
                if (item.first == kBodyNameAttributeName)
                {
                    continue;
                }
                out << ", ";
                out << "\"" << JsonEscape(item.first) << "\": \"" << JsonEscape(item.second) << "\"";
            }
            out << "}}";
        }
        out << "\n  ]\n}\n";
        return true;
    }

    std::string ReadAllText(const std::filesystem::path& path)
    {
        std::ifstream in(path, std::ios::binary);
        if (!in)
        {
            return {};
        }
        std::ostringstream buffer;
        buffer << in.rdbuf();
        return buffer.str();
    }

    bool ParseJsonStringArray(const std::string& json, const std::string& propertyName, std::vector<std::string>& values, size_t maxCount = 0)
    {
        const std::string key = "\"" + propertyName + "\"";
        size_t pos = json.find(key);
        if (pos == std::string::npos)
        {
            return false;
        }
        pos = json.find('[', pos);
        if (pos == std::string::npos)
        {
            return false;
        }
        ++pos;
        values.clear();
        while (pos < json.size() && (maxCount == 0 || values.size() < maxCount))
        {
            while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) ++pos;
            if (pos >= json.size() || json[pos] == ']') break;
            if (json[pos] != '"') return false;
            ++pos;
            std::string value;
            while (pos < json.size())
            {
                char ch = json[pos++];
                if (ch == '"') break;
                if (ch == '\\' && pos < json.size())
                {
                    char esc = json[pos++];
                    switch (esc)
                    {
                    case '"': value.push_back('"'); break;
                    case '\\': value.push_back('\\'); break;
                    case '/': value.push_back('/'); break;
                    case 'b': value.push_back('\b'); break;
                    case 'f': value.push_back('\f'); break;
                    case 'n': value.push_back('\n'); break;
                    case 'r': value.push_back('\r'); break;
                    case 't': value.push_back('\t'); break;
                    default: break;
                    }
                }
                else
                {
                    value.push_back(ch);
                }
            }
            values.push_back(Trim(value));
            while (pos < json.size() && json[pos] != ',' && json[pos] != ']') ++pos;
            if (pos < json.size() && json[pos] == ',') ++pos;
        }
        return true;
    }

    bool ParseJsonStringValue(const std::string& json, const std::string& propertyName, std::string& value);

    size_t FindMatchingJsonBracket(const std::string& json, size_t openPos, char openChar, char closeChar)
    {
        if (openPos >= json.size() || json[openPos] != openChar)
        {
            return std::string::npos;
        }

        int depth = 0;
        bool inString = false;
        bool escaped = false;
        for (size_t pos = openPos; pos < json.size(); ++pos)
        {
            const char ch = json[pos];
            if (inString)
            {
                if (escaped)
                {
                    escaped = false;
                }
                else if (ch == '\\')
                {
                    escaped = true;
                }
                else if (ch == '"')
                {
                    inString = false;
                }
                continue;
            }

            if (ch == '"')
            {
                inString = true;
            }
            else if (ch == openChar)
            {
                ++depth;
            }
            else if (ch == closeChar)
            {
                --depth;
                if (depth == 0)
                {
                    return pos;
                }
            }
        }
        return std::string::npos;
    }

    bool ParseManualRows(const std::string& json, std::map<std::string, std::vector<std::string>>& manualRows)
    {
        manualRows.clear();
        const std::string key = "\"manualRows\"";
        size_t pos = json.find(key);
        if (pos == std::string::npos)
        {
            return false;
        }

        pos = json.find('[', pos);
        if (pos == std::string::npos)
        {
            return false;
        }
        const size_t end = FindMatchingJsonBracket(json, pos, '[', ']');
        if (end == std::string::npos)
        {
            return false;
        }

        ++pos;
        while (pos < end)
        {
            pos = json.find('{', pos);
            if (pos == std::string::npos || pos >= end)
            {
                break;
            }
            const size_t objectEnd = FindMatchingJsonBracket(json, pos, '{', '}');
            if (objectEnd == std::string::npos || objectEnd > end)
            {
                break;
            }

            const std::string objectJson = json.substr(pos, objectEnd - pos + 1);
            std::string bodyName;
            std::vector<std::string> values;
            if (ParseJsonStringValue(objectJson, "bodyName", bodyName) &&
                ParseJsonStringArray(objectJson, "values", values, kAttributeColumnCount) &&
                !bodyName.empty())
            {
                manualRows[bodyName] = std::move(values);
            }
            pos = objectEnd + 1;
        }
        return true;
    }

    bool ParseJsonStringValue(const std::string& json, const std::string& propertyName, std::string& value)
    {
        const std::string key = "\"" + propertyName + "\"";
        size_t pos = json.find(key);
        if (pos == std::string::npos)
        {
            return false;
        }
        pos = json.find(':', pos);
        if (pos == std::string::npos)
        {
            return false;
        }
        ++pos;
        while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) ++pos;
        if (pos >= json.size() || json[pos] != '"')
        {
            return false;
        }
        ++pos;

        value.clear();
        while (pos < json.size())
        {
            char ch = json[pos++];
            if (ch == '"') break;
            if (ch == '\\' && pos < json.size())
            {
                char esc = json[pos++];
                switch (esc)
                {
                case '"': value.push_back('"'); break;
                case '\\': value.push_back('\\'); break;
                case '/': value.push_back('/'); break;
                case 'b': value.push_back('\b'); break;
                case 'f': value.push_back('\f'); break;
                case 'n': value.push_back('\n'); break;
                case 'r': value.push_back('\r'); break;
                case 't': value.push_back('\t'); break;
                default: break;
                }
            }
            else
            {
                value.push_back(ch);
            }
        }
        value = Trim(value);
        return true;
    }

    bool ParseJsonDoubleValue(const std::string& json, const std::string& propertyName, double& value)
    {
        const std::string key = "\"" + propertyName + "\"";
        size_t pos = json.find(key);
        if (pos == std::string::npos)
        {
            return false;
        }
        pos = json.find(':', pos);
        if (pos == std::string::npos)
        {
            return false;
        }
        ++pos;
        while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) ++pos;

        size_t end = pos;
        while (end < json.size() && (std::isdigit(static_cast<unsigned char>(json[end])) || json[end] == '.' || json[end] == '-' || json[end] == '+'))
        {
            ++end;
        }
        if (end == pos)
        {
            return false;
        }

        try
        {
            value = std::stod(json.substr(pos, end - pos));
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    double ClampDouble(double value, double minValue, double maxValue)
    {
        return std::max(minValue, std::min(maxValue, value));
    }

    bool ParseWpfSelectedColumns(const std::string& json, std::vector<ColumnDef>& columns, std::vector<std::string>& includedBodyNames, std::map<std::string, std::vector<std::string>>& manualRows, TableOptions& options)
    {
        if (json.find("\"confirmed\"") == std::string::npos || json.find("true") == std::string::npos)
        {
            return false;
        }

        std::vector<std::string> selectedColumns;
        if (!ParseJsonStringArray(json, "selectedColumns", selectedColumns, kAttributeColumnCount))
        {
            return false;
        }

        std::vector<std::string> headerTitles;
        ParseJsonStringArray(json, "headerTitles", headerTitles, kAttributeColumnCount);

        std::string headerLocation;
        if (ParseJsonStringValue(json, "headerLocation", headerLocation))
        {
            std::transform(headerLocation.begin(), headerLocation.end(), headerLocation.begin(),
                [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            options.headerBelow = headerLocation == "below";
        }

        double parsedTextHeight = options.textHeight;
        if (ParseJsonDoubleValue(json, "textHeight", parsedTextHeight))
        {
            options.textHeight = ClampDouble(parsedTextHeight, 1.0, 20.0);
        }

        columns.clear();
        for (size_t i = 0; i < kAttributeColumnCount; ++i)
        {
            ColumnDef column;
            column.attributeName = i < selectedColumns.size() ? selectedColumns[i] : "";
            column.headerTitle = i < headerTitles.size() ? headerTitles[i] : column.attributeName;
            if (!Trim(column.headerTitle).empty())
            {
                columns.push_back(column);
            }
        }

        std::vector<std::string> bodyNames;
        if (ParseJsonStringArray(json, "includedBodyNames", bodyNames))
        {
            includedBodyNames = std::move(bodyNames);
        }
        ParseManualRows(json, manualRows);
        return true;
    }

    bool LaunchWpfConfigurator(const std::vector<BodyRecord>& bodies, std::vector<ColumnDef>& columns, std::vector<std::string>& includedBodyNames, std::map<std::string, std::vector<std::string>>& manualRows, TableOptions& options, std::string& error)
    {
        const std::filesystem::path uiDir = CurrentModuleDirectory() / "MinXiBiaoUI";
        const std::filesystem::path exePath = uiDir / "MinXiBiaoUI.exe";
        if (!std::filesystem::exists(exePath))
        {
            error = "未找到 WPF 配置器：MinXiBiaoUI\\MinXiBiaoUI.exe";
            return false;
        }

        std::error_code ignored;
        std::filesystem::create_directories(uiDir, ignored);
        const std::filesystem::path inputPath = uiDir / "MinXiBiao.input.json";
        const std::filesystem::path outputPath = uiDir / "MinXiBiao.output.json";
        std::filesystem::remove(outputPath, ignored);

        if (!WriteWpfInputFile(inputPath, bodies, CollectAttributeNames(bodies)))
        {
            error = "写入 WPF 配置数据失败。";
            return false;
        }

        std::wstring commandLine = QuoteArgument(exePath) + L" --input " + QuoteArgument(inputPath) + L" --output " + QuoteArgument(outputPath);
        STARTUPINFOW startup = {};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process = {};
        std::wstring mutableCommandLine = commandLine;
        const BOOL created = CreateProcessW(
            exePath.c_str(),
            mutableCommandLine.data(),
            nullptr,
            nullptr,
            FALSE,
            0,
            nullptr,
            uiDir.c_str(),
            &startup,
            &process);
        if (!created)
        {
            error = "启动 WPF 配置器失败。";
            return false;
        }

        WaitForSingleObject(process.hProcess, INFINITE);
        DWORD exitCode = 0;
        GetExitCodeProcess(process.hProcess, &exitCode);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);

        if (exitCode != 0 && !std::filesystem::exists(outputPath))
        {
            error = "WPF 配置器异常退出。";
            return false;
        }

        const std::string outputJson = ReadAllText(outputPath);
        if (outputJson.empty())
        {
            return false;
        }
        return ParseWpfSelectedColumns(outputJson, columns, includedBodyNames, manualRows, options);
    }

    bool TryReadAttributeFromMap(tag_t objectTag, const std::string& name, std::string& value)
    {
        if (objectTag == NULL_TAG || Trim(name).empty())
        {
            return false;
        }

        const auto values = CollectBodyAttributeMap(objectTag);
        const auto it = values.find(name);
        if (it != values.end() && !Trim(it->second).empty())
        {
            value = Trim(FormatDecimalTextIfNeeded(it->second));
            return true;
        }
        return false;
    }

    bool TryReadAttribute(const BodyRecord& body, size_t bodyIndex, const std::string& name, std::string& value)
    {
        if (Trim(name) == kBodyNameAttributeName)
        {
            value = ReadBodyDisplayName(body, bodyIndex);
            return !Trim(value).empty();
        }
        return TryReadAttributeFromMap(body.attributeTag != NULL_TAG ? body.attributeTag : body.tag, name, value);
    }

    std::vector<BodyValueRow> BuildRows(const std::vector<BodyRecord>& bodies, const std::vector<std::string>* includedBodyNames, const std::map<std::string, std::vector<std::string>>& manualRows, const std::vector<ColumnDef>& columns)
    {
        std::vector<BodyValueRow> collectedRows;
        std::map<std::string, size_t> rowIndexByName;
        for (size_t bodyIndex = 0; bodyIndex < bodies.size(); ++bodyIndex)
        {
            const BodyRecord& body = bodies[bodyIndex];
            if (body.tag == NULL_TAG)
            {
                continue;
            }

            const std::string bodyName = MakeBodyRecordName(bodyIndex);
            BodyValueRow row;
            row.bodyTag = body.attributeTag != NULL_TAG ? body.attributeTag : body.tag;
            row.bodyName = bodyName;
            bool keep = false;
            for (const ColumnDef& column : columns)
            {
                std::string value;
                const size_t columnIndex = row.values.size();
                const auto manual = manualRows.find(bodyName);
                if (Trim(column.attributeName).empty() &&
                    manual != manualRows.end() &&
                    columnIndex < manual->second.size())
                {
                    value = manual->second[columnIndex];
                    keep = keep || !Trim(value).empty();
                }
                else if (TryReadAttribute(body, bodyIndex, column.attributeName, value))
                {
                    keep = true;
                }
                row.values.push_back(value);
            }

            if (keep)
            {
                rowIndexByName[row.bodyName] = collectedRows.size();
                collectedRows.push_back(std::move(row));
            }
        }

        if (includedBodyNames == nullptr || includedBodyNames->empty())
        {
            return collectedRows;
        }

        std::vector<BodyValueRow> orderedRows;
        std::set<std::string> addedNames;
        for (const std::string& bodyName : *includedBodyNames)
        {
            const auto found = rowIndexByName.find(bodyName);
            if (found != rowIndexByName.end() && addedNames.insert(bodyName).second)
            {
                orderedRows.push_back(collectedRows[found->second]);
            }
        }
        return orderedRows;
    }

    bool AskTableOrigin(double origin[3], std::string& error)
    {
        int oldCursorView = 1;
        UF_UI_ask_cursor_view(&oldCursorView);
        UF_UI_set_cursor_view(1);

        tag_t viewTag = NULL_TAG;
        int response = 0;
        std::string cue = Utf8ToSystem("单击工程图位置放置明细表");
        const int status = UF_UI_specify_screen_position(cue.data(), nullptr, nullptr, origin, &viewTag, &response);

        UF_UI_set_cursor_view(oldCursorView);

        if (status != 0)
        {
            error = "取放置点失败。";
            return false;
        }
        if (response == UF_UI_CANCEL || response == UF_UI_BACK)
        {
            error.clear();
            return false;
        }
        if (response != UF_UI_PICK_RESPONSE)
        {
            error = "未取得有效放置点。";
            return false;
        }
        return true;
    }

    std::string NormalizeCellText(const std::string& text)
    {
        std::string normalized = Trim(text);
        for (char& ch : normalized)
        {
            if (ch == '\r' || ch == '\n' || ch == '\t')
            {
                ch = ' ';
            }
        }
        return Trim(normalized);
    }

    std::string MakeColumnHeaderText(const ColumnDef& column, size_t index)
    {
        if (!Trim(column.headerTitle).empty())
        {
            return column.headerTitle;
        }
        if (!Trim(column.attributeName).empty())
        {
            return column.attributeName;
        }
        return "属性" + std::to_string(index + 1);
    }

    void ClearDefaultTabularNoteRows(tag_t tabnote)
    {
        int count = 0;
        if (UF_TABNOT_ask_nm_rows(tabnote, &count) != 0 || count <= 0)
        {
            return;
        }

        std::vector<tag_t> tags;
        tags.reserve(static_cast<size_t>(count));
        for (int i = count - 1; i >= 0; --i)
        {
            tag_t row = NULL_TAG;
            if (UF_TABNOT_ask_nth_row(tabnote, i, &row) == 0 && row != NULL_TAG)
            {
                tags.push_back(row);
            }
        }

        for (tag_t row : tags)
        {
            UF_TABNOT_remove_row(row);
            UF_OBJ_delete_object(row);
        }
    }

    void ClearDefaultTabularNoteColumns(tag_t tabnote)
    {
        int count = 0;
        if (UF_TABNOT_ask_nm_columns(tabnote, &count) != 0 || count <= 0)
        {
            return;
        }

        std::vector<tag_t> tags;
        tags.reserve(static_cast<size_t>(count));
        for (int i = count - 1; i >= 0; --i)
        {
            tag_t column = NULL_TAG;
            if (UF_TABNOT_ask_nth_column(tabnote, i, &column) == 0 && column != NULL_TAG)
            {
                tags.push_back(column);
            }
        }

        for (tag_t column : tags)
        {
            UF_TABNOT_remove_column(column);
            UF_OBJ_delete_object(column);
        }
    }

    double EstimateTextWidth(const std::string& text, double textHeight)
    {
        const std::string trimmed = Trim(text);
        size_t asciiCount = 0;
        size_t wideCount = 0;
        for (unsigned char ch : trimmed)
        {
            if (ch < 0x80)
            {
                ++asciiCount;
            }
            else if ((ch & 0xC0) != 0x80)
            {
                ++wideCount;
            }
        }
        const double units = static_cast<double>(asciiCount) * 0.85 + static_cast<double>(wideCount) * 1.25;
        return std::max(12.0, units * textHeight + textHeight * 4.0);
    }

    std::vector<double> EstimateColumnWidths(const std::vector<ColumnDef>& columns, const std::vector<BodyValueRow>& rows, double textHeight)
    {
        std::vector<double> widths(columns.size() + 1, EstimateTextWidth("序号", textHeight));
        widths[0] = std::max(widths[0], EstimateTextWidth(std::to_string(rows.size()), textHeight));

        for (size_t c = 0; c < columns.size(); ++c)
        {
            double width = EstimateTextWidth(MakeColumnHeaderText(columns[c], c), textHeight);
            for (const BodyValueRow& row : rows)
            {
                if (c < row.values.size())
                {
                    width = std::max(width, EstimateTextWidth(row.values[c], textHeight));
                }
            }
            widths[c + 1] = ClampDouble(width, 14.0, 160.0);
        }
        widths[0] = ClampDouble(widths[0], 12.0, 32.0);
        return widths;
    }

    bool ApplyCellText(tag_t cell, const std::string& text, double textHeight)
    {
        if (textHeight > 0.0)
        {
            UF_TABNOT_cell_prefs_t prefs;
            if (UF_TABNOT_ask_cell_prefs(cell, &prefs) == 0)
            {
                prefs.text_height = textHeight;
                prefs.nm_fit_methods = 1;
                prefs.fit_methods[0] = UF_TABNOT_fit_method_auto_size_col;
                UF_TABNOT_set_cell_prefs(cell, &prefs);
            }
        }

        const std::string systemText = Utf8ToSystem(NormalizeCellText(text));
        return UF_TABNOT_set_cell_text(cell, systemText.c_str()) == 0;
    }

    bool ApplyAssociativeCellText(tag_t cell, const std::string& text, double textHeight)
    {
        if (textHeight > 0.0)
        {
            UF_TABNOT_cell_prefs_t prefs;
            if (UF_TABNOT_ask_cell_prefs(cell, &prefs) == 0)
            {
                prefs.text_height = textHeight;
                prefs.nm_fit_methods = 1;
                prefs.fit_methods[0] = UF_TABNOT_fit_method_auto_size_col;
                UF_TABNOT_set_cell_prefs(cell, &prefs);
            }
        }

        const std::string systemText = Utf8ToSystem(text);
        return UF_TABNOT_set_cell_text(cell, systemText.c_str()) == 0;
    }

    bool TryMakeAssociativeAttributeText(tag_t bodyTag, const std::string& attributeName, std::string& text)
    {
        text.clear();
        if (bodyTag == NULL_TAG || Trim(attributeName).empty())
        {
            return false;
        }

        try
        {
            NXOpen::Session* session = NXOpen::Session::GetSession();
            if (session == nullptr || session->Parts() == nullptr || session->Parts()->Work() == nullptr)
            {
                return false;
            }

            NXOpen::NXObject* object = dynamic_cast<NXOpen::NXObject*>(NXOpen::NXObjectManager::Get(bodyTag));
            if (object == nullptr)
            {
                return false;
            }

            NXOpen::Annotations::AnnotationManager* annotations = session->Parts()->Work()->Annotations();
            if (annotations == nullptr)
            {
                return false;
            }

            NXOpen::Annotations::AssociativeText* associativeText = annotations->CreateAssociativeText();
            if (associativeText == nullptr)
            {
                return false;
            }

            NXOpen::NXString attributeTitle(attributeName.c_str(), NXOpen::NXString::UTF8);
            NXOpen::NXString nxText;
            try
            {
                nxText = associativeText->GetObjectAttributeTextFormatted(object, attributeTitle, 1);
            }
            catch (...)
            {
                nxText = associativeText->GetObjectAttributeText(object, attributeTitle);
            }
            delete associativeText;

            const char* utf8 = nxText.GetUTF8Text();
            if (utf8 != nullptr && utf8[0] != '\0')
            {
                text = utf8;
                return true;
            }

            const char* locale = nxText.GetLocaleText();
            if (locale != nullptr && locale[0] != '\0')
            {
                text = SystemToUtf8(locale);
                return true;
            }
        }
        catch (const NXOpen::NXException& ex)
        {
            LogBodyScan(std::string("TryMakeAssociativeAttributeText NXException: ") + ex.Message());
        }
        catch (const std::exception& ex)
        {
            LogBodyScan(std::string("TryMakeAssociativeAttributeText exception: ") + ex.what());
        }
        catch (...)
        {
            LogBodyScan("TryMakeAssociativeAttributeText unknown exception");
        }
        return false;
    }

    bool CreateTabularNoteAtPoint(const std::vector<ColumnDef>& columns, const std::vector<BodyValueRow>& rows, const TableOptions& options, const double origin[3], std::string& error)
    {
        tag_t tabnote = NULL_TAG;
        UF_TABNOT_section_prefs_t sectionPrefs;
        UF_TABNOT_cell_prefs_t cellPrefs;
        UF_TABNOT_ask_default_section_prefs(&sectionPrefs);
        UF_TABNOT_ask_default_cell_prefs(&cellPrefs);
        const double textHeight = ClampDouble(options.textHeight, 1.0, 20.0);
        sectionPrefs.header_location = options.headerBelow ? UF_TABNOT_header_location_below : UF_TABNOT_header_location_above;
        sectionPrefs.attach_point = UF_TABNOT_attach_point_top_left;
        sectionPrefs.max_height = 1000.0;
        sectionPrefs.overflow_direction = UF_TABNOT_overflow_right;
        sectionPrefs.overflow_spacing = 5.0;
        sectionPrefs.use_double_width_border = FALSE;
        sectionPrefs.border_width = 0.0;
        sectionPrefs.display_continuation_note = UF_TABNOT_display_continuation_note_none;
        sectionPrefs.continuation_note = nullptr;

        if (UF_TABNOT_create(&sectionPrefs, const_cast<double*>(origin), &tabnote) != 0 || tabnote == NULL_TAG)
        {
            error = "无法创建工程图表格。";
            return false;
        }

        ClearDefaultTabularNoteRows(tabnote);
        ClearDefaultTabularNoteColumns(tabnote);

        const int totalColumns = static_cast<int>(columns.size()) + 1;
        const std::vector<double> columnWidths = EstimateColumnWidths(columns, rows, textHeight);
        std::vector<tag_t> columnTags;
        columnTags.reserve(static_cast<size_t>(totalColumns));
        for (int i = 0; i < totalColumns; ++i)
        {
            tag_t columnTag = NULL_TAG;
            const double width = i < static_cast<int>(columnWidths.size()) ? columnWidths[static_cast<size_t>(i)] : 18.0;
            if (UF_TABNOT_create_column(width, &columnTag) != 0 || columnTag == NULL_TAG)
            {
                error = "创建表格列失败。";
                return false;
            }
            if (UF_TABNOT_add_column(tabnote, columnTag, UF_TABNOT_APPEND) != 0)
            {
                error = "添加表格列失败。";
                return false;
            }
            columnTags.push_back(columnTag);
        }

        std::vector<tag_t> rowTags;
        const int totalRows = static_cast<int>(rows.size()) + 1;
        rowTags.reserve(static_cast<size_t>(totalRows));
        const double rowHeight = ClampDouble(textHeight * 2.0, 4.0, 40.0);
        for (int i = 0; i < totalRows; ++i)
        {
            tag_t rowTag = NULL_TAG;
            if (UF_TABNOT_create_row(rowHeight, &rowTag) != 0 || rowTag == NULL_TAG)
            {
                error = "创建表格行失败。";
                return false;
            }
            if (UF_TABNOT_add_row(tabnote, rowTag, UF_TABNOT_APPEND) != 0)
            {
                error = "添加表格行失败。";
                return false;
            }
            rowTags.push_back(rowTag);
        }

        const size_t headerRowIndex = options.headerBelow ? rowTags.size() - 1 : 0;
        const size_t firstDataRowIndex = options.headerBelow ? 0 : 1;
        for (int col = 0; col < totalColumns; ++col)
        {
            tag_t cell = NULL_TAG;
            if (UF_TABNOT_ask_cell_at_row_col(rowTags[headerRowIndex], columnTags[static_cast<size_t>(col)], &cell) != 0 || cell == NULL_TAG)
            {
                error = "获取表头单元格失败。";
                return false;
            }
            const std::string text = (col == 0) ? "序号" : MakeColumnHeaderText(columns[static_cast<size_t>(col - 1)], static_cast<size_t>(col - 1));
            ApplyCellText(cell, text, textHeight);
        }

        for (size_t r = 0; r < rows.size(); ++r)
        {
            const size_t rowIndex = options.headerBelow ? (rows.size() - 1 - r) : (firstDataRowIndex + r);
            tag_t seqCell = NULL_TAG;
            if (UF_TABNOT_ask_cell_at_row_col(rowTags[rowIndex], columnTags[0], &seqCell) != 0 || seqCell == NULL_TAG)
            {
                error = "获取序号单元格失败。";
                return false;
            }
            const std::string seqText = std::to_string(r + 1);
            ApplyCellText(seqCell, seqText, textHeight);

            for (size_t c = 0; c < columns.size(); ++c)
            {
                tag_t cell = NULL_TAG;
                if (UF_TABNOT_ask_cell_at_row_col(rowTags[rowIndex], columnTags[c + 1], &cell) != 0 || cell == NULL_TAG)
                {
                    error = "获取数据单元格失败。";
                    return false;
                }
                const std::string text = c < rows[r].values.size() ? NormalizeCellText(rows[r].values[c]) : std::string();
                std::string associativeText;
                if (!Trim(columns[c].attributeName).empty() &&
                    TryMakeAssociativeAttributeText(rows[r].bodyTag, columns[c].attributeName, associativeText))
                {
                    ApplyAssociativeCellText(cell, associativeText, textHeight);
                }
                else
                {
                    ApplyCellText(cell, text, textHeight);
                }
            }
        }

        return true;
    }
}

namespace MinXiBiao
{
    bool RunWpfWorkflow()
    {
        LogBodyScan("RunWpfWorkflow begin");
        const tag_t displayPartTag = UF_PART_ask_display_part();
        const tag_t workPartTag = UF_ASSEM_ask_work_part();
        if (displayPartTag == NULL_TAG && workPartTag == NULL_TAG)
        {
            LogBodyScan("RunWpfWorkflow no part");
            return true;
        }

        std::vector<BodyRecord> bodies = FilterEligibleBodies(CollectWorkPartBodies(), "RunWpfWorkflow eligible bodies");
        LogBodyScan("RunWpfWorkflow bodies=" + std::to_string(bodies.size()));
        if (bodies.empty())
        {
            return true;
        }

        std::vector<ColumnDef> columns;
        std::vector<std::string> includedBodyNames;
        std::map<std::string, std::vector<std::string>> manualRows;
        TableOptions tableOptions;
        std::string error;
        LogBodyScan("RunWpfWorkflow launch WPF");
        if (!LaunchWpfConfigurator(bodies, columns, includedBodyNames, manualRows, tableOptions, error))
        {
            if (!error.empty())
            {
                LogBodyScan("LaunchWpfConfigurator error: " + error);
                ShowNxMessage(error);
                return true;
            }
            LogBodyScan("LaunchWpfConfigurator canceled/no output");
            return true;
        }
        LogBodyScan("RunWpfWorkflow WPF columns=" + std::to_string(columns.size()));

        bool hasColumn = false;
        for (const auto& column : columns)
        {
            if (!Trim(column.headerTitle).empty())
            {
                hasColumn = true;
                break;
            }
        }
        if (!hasColumn)
        {
            return true;
        }

        const std::vector<BodyValueRow> rows = BuildRows(bodies, &includedBodyNames, manualRows, columns);
        LogBodyScan("RunWpfWorkflow rows=" + std::to_string(rows.size()));
        if (rows.empty())
        {
            return true;
        }

        double origin[3] = {0.0, 0.0, 0.0};
        LogBodyScan("RunWpfWorkflow ask origin");
        if (!AskTableOrigin(origin, error))
        {
            if (!error.empty())
            {
                LogBodyScan("AskTableOrigin error: " + error);
                ShowNxMessage(error);
            }
            return true;
        }

        LogBodyScan("RunWpfWorkflow create tabular note");
        if (!CreateTabularNoteAtPoint(columns, rows, tableOptions, origin, error))
        {
            LogBodyScan("CreateTabularNoteAtPoint error: " + error);
            ShowNxMessage(error);
        }
        LogBodyScan("RunWpfWorkflow end");
        return true;
    }
}

namespace zhihui_license_guard
{
    typedef int (__stdcall *EnsureAuthorizedProc)(const wchar_t*, const wchar_t*, wchar_t*, int);

    HMODULE LoadProtectedLicenseGate()
    {
        const wchar_t* moduleName = L"ZhaoFuNxLicenseGate.dll";
        HMODULE existing = GetModuleHandleW(moduleName);
        if (existing != NULL)
        {
            return existing;
        }

        HMODULE selfModule = NULL;
        if (GetModuleHandleExW(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(&LoadProtectedLicenseGate),
                &selfModule))
        {
            wchar_t localPath[MAX_PATH] = { 0 };
            const DWORD length = GetModuleFileNameW(selfModule, localPath, MAX_PATH);
            if (length > 0 && length < MAX_PATH)
            {
                DWORD slash = length;
                while (slash > 0 && localPath[slash - 1] != L'\\' && localPath[slash - 1] != L'/')
                {
                    --slash;
                }
                if (slash > 0)
                {
                    DWORD pos = slash;
                    for (DWORD i = 0; moduleName[i] != L'\0' && pos + 1 < MAX_PATH; ++i, ++pos)
                    {
                        localPath[pos] = moduleName[i];
                    }
                    localPath[pos] = L'\0';
                    HMODULE localModule = LoadLibraryW(localPath);
                    if (localModule != NULL)
                    {
                        return localModule;
                    }
                }
            }
        }

        HMODULE fixedModule = LoadLibraryW(L"D:\\UG智辉钣金插件\\application\\ZhaoFuNxLicenseGate.dll");
        if (fixedModule != NULL)
        {
            return fixedModule;
        }
        return LoadLibraryW(moduleName);
    }

    bool EnsureAuthorized(const wchar_t* featureCode, const wchar_t* displayName, std::string& error)
    {
        wchar_t message[1024] = { 0 };
        HMODULE module = LoadProtectedLicenseGate();
        if (module == NULL)
        {
            error = "未加载授权网关 ZhaoFuNxLicenseGate.dll。";
            LogBodyScan("EnsureAuthorized failed: gate module not loaded");
            return false;
        }
        LogBodyScan("EnsureAuthorized gate module loaded");
        EnsureAuthorizedProc ensureAuthorized =
            reinterpret_cast<EnsureAuthorizedProc>(GetProcAddress(module, "ZfnxEnsureAuthorized"));
        if (ensureAuthorized == NULL)
        {
            error = "授权网关缺少 ZfnxEnsureAuthorized 导出。";
            LogBodyScan("EnsureAuthorized failed: export ZfnxEnsureAuthorized missing");
            return false;
        }

        const int ok = ensureAuthorized(featureCode, displayName, message, static_cast<int>(sizeof(message) / sizeof(message[0])));
        LogBodyScan("EnsureAuthorized first result=" + std::to_string(ok) + " message=" + WideToUtf8(message));
        const int ok2 = ok == 1 ? ensureAuthorized(featureCode, displayName, message, static_cast<int>(sizeof(message) / sizeof(message[0]))) : ok;
        LogBodyScan("EnsureAuthorized second result=" + std::to_string(ok2) + " message=" + WideToUtf8(message));
        if (ok == 1 && ok2 == 1)
        {
            return true;
        }
        error = WideToUtf8(message);
        if (error.empty())
        {
            error = "授权或防篡改校验未通过。";
        }
        return false;
    }
}

extern "C" DllExport void ufusr(char*, int*, int)
{
    LogBodyScan("ufusr entry");
    const int ufStatus = UF_initialize();
    if (ufStatus != 0)
    {
        ShowNxMessage("UFUN 初始化失败。");
        return;
    }
    std::string authError;
    if (!zhihui_license_guard::EnsureAuthorized(L"ZHIHUI.MINXIBIAO", L"明细表", authError))
    {
        LogBodyScan("ufusr authorization failed: " + authError);
        ShowNxMessage(authError);
        return;
    }

    try
    {
        MinXiBiao::RunWpfWorkflow();
    }
    catch (const std::exception& ex)
    {
        LogBodyScan(std::string("ufusr exception: ") + ex.what());
    }
    catch (...)
    {
        LogBodyScan("ufusr unknown exception");
    }
}

extern "C" DllExport int ufusr_ask_unload()
{
    return UF_UNLOAD_IMMEDIATELY;
}

