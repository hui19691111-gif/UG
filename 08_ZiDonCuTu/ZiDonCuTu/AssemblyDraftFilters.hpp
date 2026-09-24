#pragma once
#include <windows.h>
#include <cwctype>
#include <map>
#include <string>

namespace assembly_draft_filters
{
enum Rule
{
    WithDrawing, WithoutDrawing, SheetMetal, NonSheetMetal, Hidden,
    KeywordMatches, KeywordNonMatches, HasAttribute, MissingAttribute,
    AttributeEquals, WithoutAttributeValue, RuleCount
};

struct Metadata
{
    bool hasDrawing = false;
    bool sheetMetal = false;
    bool hidden = false;
    std::wstring partName;
    std::map<std::wstring, std::wstring> attributes;
};

struct Rules
{
    bool enabled[RuleCount] = {};
    std::wstring text[RuleCount];
    std::wstring equalsValue;
};

inline std::wstring Normalize(std::wstring text)
{
    const size_t first = text.find_first_not_of(L" \t\r\n");
    if (first == std::wstring::npos) return L"";
    text = text.substr(first, text.find_last_not_of(L" \t\r\n") - first + 1);
    if (!text.empty()) CharLowerBuffW(&text[0], static_cast<DWORD>(text.size()));
    return text;
}

inline int InvalidRule(const Rules& rules)
{
    for (int i = KeywordMatches; i < RuleCount; ++i)
        if (rules.enabled[i] && Normalize(rules.text[i]).empty()) return i;
    if (rules.enabled[AttributeEquals] && Normalize(rules.equalsValue).empty())
        return AttributeEquals;
    return -1;
}

inline bool FindAttribute(const Metadata& data, const std::wstring& name, std::wstring* value = nullptr)
{
    const std::wstring key = Normalize(name);
    if (key.empty()) return false;
    for (const auto& attribute : data.attributes)
        if (Normalize(attribute.first) == key)
        {
            if (value != nullptr) *value = attribute.second;
            return true;
        }
    return false;
}

inline bool Remove(const Metadata& data, const Rules& rules)
{
    if ((rules.enabled[WithDrawing] && data.hasDrawing) ||
        (rules.enabled[WithoutDrawing] && !data.hasDrawing) ||
        (rules.enabled[SheetMetal] && data.sheetMetal) ||
        (rules.enabled[NonSheetMetal] && !data.sheetMetal) ||
        (rules.enabled[Hidden] && data.hidden)) return true;
    const std::wstring name = Normalize(data.partName);
    if (rules.enabled[KeywordMatches] && !Normalize(rules.text[KeywordMatches]).empty() &&
        name.find(Normalize(rules.text[KeywordMatches])) != std::wstring::npos) return true;
    if (rules.enabled[KeywordNonMatches] &&
        name.find(Normalize(rules.text[KeywordNonMatches])) == std::wstring::npos) return true;
    if (rules.enabled[HasAttribute] && FindAttribute(data, rules.text[HasAttribute])) return true;
    if (rules.enabled[MissingAttribute] && !FindAttribute(data, rules.text[MissingAttribute])) return true;
    std::wstring actualValue;
    if (rules.enabled[AttributeEquals] && FindAttribute(data, rules.text[AttributeEquals], &actualValue) &&
        Normalize(actualValue) == Normalize(rules.equalsValue)) return true;
    if (rules.enabled[WithoutAttributeValue])
    {
        for (const auto& attribute : data.attributes)
            if (Normalize(attribute.second) == Normalize(rules.text[WithoutAttributeValue])) return false;
        return true;
    }
    return false;
}
}
