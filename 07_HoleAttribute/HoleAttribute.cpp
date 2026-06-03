#include "HoleAttribute.hpp"
#include "..\09_KonBiaoZu\src\ExcelRuleManager.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <windows.h>
#include <shellapi.h>
#include <oleauto.h>

#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "ole32.lib")

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#ifdef CreateDialog
#undef CreateDialog
#endif

using namespace NXOpen;
using namespace NXOpen::BlockStyler;

Session* HoleAttribute::theSession = NULL;
UI* HoleAttribute::theUI = NULL;

static const double DiameterTolerance = 0.001;
static const double CounterboreHeightDifference = 1.0;
static std::string nxStringToString(const NXString& value)
{
    const char* text = value.GetUTF8Text();
    return text == NULL ? std::string() : std::string(text);
}

static std::string trim(const std::string& text)
{
    size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return std::string();

    size_t last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

static std::string toUpperAscii(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
        return (char)std::toupper(ch);
    });
    return text;
}

static bool isSheetMetalFeatureType(const std::string& featureType)
{
    std::string type = toUpperAscii(featureType);
    static const char* SheetMetalTokens[] = {
        "SHEET_METAL",
        "SHEET METAL",
        "SMD",
        "NXSM",
        "SMTAB",
        "SMFLANGE",
        "SMBEND",
        "SMRELIEF",
        "SMCUTOUT",
        "SMHOLE",
        "SMSLOT",
        "SMBRACKET",
        "FLANGE",
        "BEND",
        "JOGGLE",
        "CONTOUR_FLANGE",
        "NORMAL_CUTOUT"
    };

    for (size_t i = 0; i < sizeof(SheetMetalTokens) / sizeof(SheetMetalTokens[0]); ++i)
    {
        if (type.find(SheetMetalTokens[i]) != std::string::npos)
            return true;
    }

    return false;
}

static bool containsText(const std::string& text, const std::string& needle)
{
    return !needle.empty() && text.find(needle) != std::string::npos;
}

static bool isTapHoleType(const std::string& type)
{
    return containsText(type, "攻牙") || containsText(type, "攻丝");
}

static bool isPemHoleType(const std::string& type)
{
    return containsText(type, "压铆");
}

static bool isTapHoleRule(const HoleAttribute::SpecRule& rule)
{
    return containsText(rule.sourceTitle, "螺牙") ||
           containsText(rule.sourceTitle, "攻牙") ||
           containsText(rule.sourceTitle, "攻丝") ||
           isTapHoleType(rule.type) ||
           containsText(rule.spec, "攻牙") ||
           containsText(rule.spec, "攻丝") ||
           containsText(rule.remark, "攻牙") ||
           containsText(rule.remark, "攻丝");
}

static bool isPemHoleRule(const HoleAttribute::SpecRule& rule)
{
    return containsText(rule.sourceTitle, "压铆") ||
           isPemHoleType(rule.type) ||
           containsText(rule.spec, "压铆") ||
           containsText(rule.remark, "压铆");
}

static bool isValidUtf8(const std::string& text)
{
    int remaining = 0;
    for (size_t i = 0; i < text.size(); ++i)
    {
        unsigned char ch = (unsigned char)text[i];
        if (remaining == 0)
        {
            if ((ch & 0x80) == 0)
                continue;
            if ((ch & 0xE0) == 0xC0)
                remaining = 1;
            else if ((ch & 0xF0) == 0xE0)
                remaining = 2;
            else if ((ch & 0xF8) == 0xF0)
                remaining = 3;
            else
                return false;
        }
        else
        {
            if ((ch & 0xC0) != 0x80)
                return false;
            --remaining;
        }
    }

    return remaining == 0;
}

static std::string ansiToUtf8(const std::string& text)
{
    if (text.empty())
        return text;

    int wideLength = MultiByteToWideChar(CP_ACP, 0, text.c_str(), -1, NULL, 0);
    if (wideLength <= 0)
        return text;

    std::vector<wchar_t> wide((size_t)wideLength);
    MultiByteToWideChar(CP_ACP, 0, text.c_str(), -1, &wide[0], wideLength);

    int utf8Length = WideCharToMultiByte(CP_UTF8, 0, &wide[0], -1, NULL, 0, NULL, NULL);
    if (utf8Length <= 0)
        return text;

    std::vector<char> utf8((size_t)utf8Length);
    WideCharToMultiByte(CP_UTF8, 0, &wide[0], -1, &utf8[0], utf8Length, NULL, NULL);
    return std::string(&utf8[0]);
}

static std::wstring utf8ToWide(const std::string& text)
{
    if (text.empty())
        return std::wstring();

    int wideLength = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, NULL, 0);
    if (wideLength <= 0)
        return std::wstring();

    std::vector<wchar_t> wide((size_t)wideLength);
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, &wide[0], wideLength);
    return std::wstring(&wide[0]);
}

static std::string wideToUtf8(const wchar_t* text)
{
    if (text == NULL || text[0] == L'\0')
        return std::string();

    int utf8Length = WideCharToMultiByte(CP_UTF8, 0, text, -1, NULL, 0, NULL, NULL);
    if (utf8Length <= 0)
        return std::string();

    std::vector<char> utf8((size_t)utf8Length);
    WideCharToMultiByte(CP_UTF8, 0, text, -1, &utf8[0], utf8Length, NULL, NULL);
    return std::string(&utf8[0]);
}

static std::string csvTextToUtf8(const std::string& text)
{
    std::string value = text;
    if (value.size() >= 3 &&
        (unsigned char)value[0] == 0xEF &&
        (unsigned char)value[1] == 0xBB &&
        (unsigned char)value[2] == 0xBF)
    {
        value.erase(0, 3);
    }

    return isValidUtf8(value) ? value : ansiToUtf8(value);
}

struct JsonRuleRecord
{
    std::string annotationType;
    std::string series;
    std::string size;
    std::string threadSpec;
    std::string lengthText;
    double bottomHole;
    std::string displayText;
    std::string standardComment;
    std::string note;
};

static void skipJsonWhitespace(const std::string& text, size_t& position)
{
    while (position < text.size() && std::isspace((unsigned char)text[position]))
        ++position;
}

static bool parseJsonStringValue(const std::string& text, size_t& position, std::string& value)
{
    skipJsonWhitespace(text, position);
    if (position >= text.size() || text[position] != '"')
        return false;
    ++position;

    value.clear();
    while (position < text.size())
    {
        char ch = text[position++];
        if (ch == '"')
            return true;

        if (ch != '\\')
        {
            value.push_back(ch);
            continue;
        }

        if (position >= text.size())
            return false;
        char escaped = text[position++];
        switch (escaped)
        {
        case '"':
        case '\\':
        case '/':
            value.push_back(escaped);
            break;
        case 'n':
            value.push_back('\n');
            break;
        case 'r':
            value.push_back('\r');
            break;
        case 't':
            value.push_back('\t');
            break;
        default:
            return false;
        }
    }

    return false;
}

static bool parseJsonNumberValue(const std::string& text, size_t& position, double& value)
{
    skipJsonWhitespace(text, position);
    size_t begin = position;
    if (position < text.size() && (text[position] == '-' || text[position] == '+'))
        ++position;
    while (position < text.size() && std::isdigit((unsigned char)text[position]))
        ++position;
    if (position < text.size() && text[position] == '.')
    {
        ++position;
        while (position < text.size() && std::isdigit((unsigned char)text[position]))
            ++position;
    }

    if (begin == position)
        return false;

    try
    {
        value = std::stod(text.substr(begin, position - begin));
        return true;
    }
    catch (...)
    {
        return false;
    }
}

static bool skipJsonValue(const std::string& text, size_t& position);

static bool skipJsonObjectOrArray(const std::string& text, size_t& position, char open, char close)
{
    skipJsonWhitespace(text, position);
    if (position >= text.size() || text[position] != open)
        return false;
    ++position;

    while (position < text.size())
    {
        skipJsonWhitespace(text, position);
        if (position < text.size() && text[position] == close)
        {
            ++position;
            return true;
        }
        if (!skipJsonValue(text, position))
            return false;
        skipJsonWhitespace(text, position);
        if (position < text.size() && text[position] == ',')
            ++position;
    }

    return false;
}

static bool skipJsonValue(const std::string& text, size_t& position)
{
    skipJsonWhitespace(text, position);
    if (position >= text.size())
        return false;
    if (text[position] == '"')
    {
        std::string ignored;
        return parseJsonStringValue(text, position, ignored);
    }
    if (text[position] == '{')
        return skipJsonObjectOrArray(text, position, '{', '}');
    if (text[position] == '[')
        return skipJsonObjectOrArray(text, position, '[', ']');
    if (std::isdigit((unsigned char)text[position]) || text[position] == '-' || text[position] == '+')
    {
        double ignored = 0.0;
        return parseJsonNumberValue(text, position, ignored);
    }

    const char* literals[] = { "true", "false", "null" };
    for (size_t i = 0; i < sizeof(literals) / sizeof(literals[0]); ++i)
    {
        size_t length = std::strlen(literals[i]);
        if (text.compare(position, length, literals[i]) == 0)
        {
            position += length;
            return true;
        }
    }
    return false;
}

static bool parseJsonRuleObject(const std::string& text, size_t& position, JsonRuleRecord& record)
{
    record.bottomHole = 0.0;
    skipJsonWhitespace(text, position);
    if (position >= text.size() || text[position] != '{')
        return false;
    ++position;

    while (position < text.size())
    {
        skipJsonWhitespace(text, position);
        if (position < text.size() && text[position] == '}')
        {
            ++position;
            return !record.annotationType.empty();
        }

        std::string key;
        if (!parseJsonStringValue(text, position, key))
            return false;
        skipJsonWhitespace(text, position);
        if (position >= text.size() || text[position] != ':')
            return false;
        ++position;

        if (key == "bottomHole")
        {
            if (!parseJsonNumberValue(text, position, record.bottomHole))
                return false;
        }
        else if (key == "annotationType" || key == "series" || key == "size" ||
                 key == "threadSpec" || key == "lengthText" || key == "displayText" ||
                 key == "standardComment" || key == "note")
        {
            std::string value;
            if (!parseJsonStringValue(text, position, value))
                return false;
            if (key == "annotationType") record.annotationType = value;
            else if (key == "series") record.series = value;
            else if (key == "size") record.size = value;
            else if (key == "threadSpec") record.threadSpec = value;
            else if (key == "lengthText") record.lengthText = value;
            else if (key == "displayText") record.displayText = value;
            else if (key == "standardComment") record.standardComment = value;
            else if (key == "note") record.note = value;
        }
        else if (!skipJsonValue(text, position))
        {
            return false;
        }

        skipJsonWhitespace(text, position);
        if (position < text.size() && text[position] == ',')
            ++position;
    }

    return false;
}

static bool loadJsonRuleRecords(const std::string& path, std::vector<JsonRuleRecord>& records, std::string& error)
{
    records.clear();
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open())
    {
        error = "鏃犳硶鎵撳紑瑙勫垯閰嶇疆鏂囦欢: " + path;
        return false;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    std::string json = buffer.str();
    if (json.size() >= 3 &&
        (unsigned char)json[0] == 0xEF &&
        (unsigned char)json[1] == 0xBB &&
        (unsigned char)json[2] == 0xBF)
    {
        json.erase(0, 3);
    }

    size_t rulesKey = json.find("\"rules\"");
    size_t position = rulesKey == std::string::npos ? std::string::npos : json.find('[', rulesKey);
    if (position == std::string::npos)
    {
        error = "瑙勫垯閰嶇疆鏂囦欢缂哄皯 rules 鏁扮粍: " + path;
        return false;
    }
    ++position;

    while (position < json.size())
    {
        skipJsonWhitespace(json, position);
        if (position < json.size() && json[position] == ']')
            return true;

        JsonRuleRecord record;
        if (!parseJsonRuleObject(json, position, record))
        {
            error = "瑙勫垯閰嶇疆鏂囦欢鏍煎紡閿欒: " + path;
            return false;
        }
        records.push_back(record);

        skipJsonWhitespace(json, position);
        if (position < json.size() && json[position] == ',')
            ++position;
    }

    return true;
}

static void replaceAllText(std::string& text, const std::string& from, const std::string& to)
{
    if (from.empty())
        return;

    size_t position = 0;
    while ((position = text.find(from, position)) != std::string::npos)
    {
        text.replace(position, from.size(), to);
        position += to.size();
    }
}

static std::string formatRuleNumber(double value)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(2) << value;
    return out.str();
}

static std::string applyJsonRuleTemplate(const std::string& templateText, const JsonRuleRecord& record)
{
    std::string text = templateText;
    replaceAllText(text, u8"{子类型}", record.series);
    replaceAllText(text, u8"{规格}", record.size);
    replaceAllText(text, u8"{螺纹}", record.threadSpec);
    replaceAllText(text, u8"{底孔}", formatRuleNumber(record.bottomHole));
    replaceAllText(text, u8"{长度}", record.lengthText);
    return trim(text);
}

static std::string holeAttributeTypeForRecord(const JsonRuleRecord& record)
{
    if (record.annotationType == u8"孔标注")
        return u8"孔";
    if (record.annotationType == u8"螺牙标注")
        return u8"攻牙";
    if (record.annotationType == u8"焊接螺母标注")
        return u8"焊接螺母";
    if (record.annotationType == u8"沉孔标注")
        return u8"沉孔";
    if (record.annotationType == u8"压铆螺母标注" ||
        record.annotationType == u8"压铆螺钉标注" ||
        record.annotationType == u8"压铆螺母柱标注")
    {
        return record.series.empty() ? record.annotationType : record.series;
    }

    const std::string suffix = u8"标注";
    if (record.annotationType.size() > suffix.size() &&
        record.annotationType.substr(record.annotationType.size() - suffix.size()) == suffix)
    {
        return record.annotationType.substr(0, record.annotationType.size() - suffix.size());
    }
    return record.annotationType.empty() ? u8"孔" : record.annotationType;
}

static bool isPemNutRecord(const JsonRuleRecord& record)
{
    return record.annotationType == u8"压铆螺母标注";
}

static int ruleLengthLevelIndex(const std::string& lengthText)
{
    std::string text = trim(lengthText);
    if (text.empty())
        return -1;
    if (text[0] == '-')
        text.erase(0, 1);
    if (text.size() == 1 && text[0] >= '1' && text[0] <= '3')
        return text[0] - '1';
    return -1;
}

static void addUniqueString(std::vector<std::string>& values, const std::string& value);

static void mergeJsonRule(std::vector<HoleAttribute::SpecRule>& specRules,
                          std::vector<std::string>& specRuleTypes,
                          const JsonRuleRecord& record)
{
    if (record.bottomHole <= 0.0)
        return;

    std::string type = holeAttributeTypeForRecord(record);
    if (type.empty())
        return;

    addUniqueString(specRuleTypes, type);

    std::string spec;
    std::string remark;
    if (type != u8"孔")
    {
        spec = applyJsonRuleTemplate(record.displayText.empty() ? record.threadSpec : record.displayText, record);
        remark = applyJsonRuleTemplate(record.standardComment.empty() ? record.note : record.standardComment, record);
    }

    int levelIndex = ruleLengthLevelIndex(record.lengthText);
    for (size_t i = 0; i < specRules.size(); ++i)
    {
        HoleAttribute::SpecRule& existing = specRules[i];
        if (existing.type == type &&
            existing.sourceTitle == record.annotationType &&
            !existing.isRange &&
            std::fabs(existing.diameter - record.bottomHole) <= DiameterTolerance)
        {
            if (levelIndex >= 0)
            {
                if (existing.levelSpecs.size() < 3)
                    existing.levelSpecs.resize(3);
                existing.levelSpecs[(size_t)levelIndex] = spec;
                return;
            }
        }
    }

    HoleAttribute::SpecRule rule;
    rule.diameter = record.bottomHole;
    rule.minDiameter = record.bottomHole;
    rule.maxDiameter = record.bottomHole;
    rule.isRange = false;
    rule.minHeight = 0.0;
    rule.maxHeight = 0.0;
    rule.hasHeightRange = false;
    rule.type = type;
    rule.spec = spec;
    rule.remark = remark;
    rule.sourceTitle = record.annotationType;
    if (levelIndex >= 0)
    {
        rule.levelSpecs.resize(3);
        rule.levelSpecs[(size_t)levelIndex] = spec;
    }
    else if (isPemNutRecord(record) && !spec.empty())
    {
        rule.levelSpecs.push_back(spec + "-1");
        rule.levelSpecs.push_back(spec + "-2");
        rule.levelSpecs.push_back(spec + "-3");
    }
    specRules.push_back(rule);
}

static void addUniqueString(std::vector<std::string>& values, const std::string& value)
{
    if (value.empty())
        return;

    for (size_t i = 0; i < values.size(); ++i)
    {
        if (values[i] == value)
            return;
    }

    values.push_back(value);
}

static std::vector<std::string> splitCsvLine(const std::string& line)
{
    std::vector<std::string> values;
    std::string current;
    bool inQuotes = false;

    for (size_t i = 0; i < line.size(); ++i)
    {
        char ch = line[i];
        if (ch == '"')
        {
            if (inQuotes && i + 1 < line.size() && line[i + 1] == '"')
            {
                current.push_back('"');
                ++i;
            }
            else
            {
                inQuotes = !inQuotes;
            }
        }
        else if (ch == ',' && !inQuotes)
        {
            values.push_back(trim(current));
            current.clear();
        }
        else
        {
            current.push_back(ch);
        }
    }

    values.push_back(trim(current));
    return values;
}

static std::vector<double> extractNumbers(const std::string& text)
{
    std::vector<double> numbers;
    for (size_t i = 0; i < text.size();)
    {
        bool canStartNumber = (text[i] >= '0' && text[i] <= '9') || text[i] == '.';
        if ((text[i] == '+' || text[i] == '-') && i + 1 < text.size())
            canStartNumber = (text[i + 1] >= '0' && text[i + 1] <= '9') || text[i + 1] == '.';

        if (!canStartNumber)
        {
            ++i;
            continue;
        }

        char* end = NULL;
        double value = std::strtod(text.c_str() + i, &end);
        if (end == text.c_str() + i)
        {
            ++i;
            continue;
        }

        numbers.push_back(value);
        i = (size_t)(end - text.c_str());
    }

    return numbers;
}

static bool parseDiameterRule(const std::string& text, double* diameter, double* minDiameter, double* maxDiameter, bool* isRange)
{
    std::string value = trim(text);
    std::vector<double> numbers = extractNumbers(value);
    if (numbers.empty())
        return false;

    bool hasRangeMark = value.find('<') != std::string::npos ||
                        value.find('>') != std::string::npos ||
                        value.find('~') != std::string::npos ||
                        value.find('-') != std::string::npos;
    if (numbers.size() >= 2 && hasRangeMark)
    {
        double low = numbers[0] < numbers[1] ? numbers[0] : numbers[1];
        double high = numbers[0] < numbers[1] ? numbers[1] : numbers[0];
        if (high <= low)
            return false;

        *diameter = 0.0;
        *minDiameter = low;
        *maxDiameter = high;
        *isRange = true;
        return true;
    }

    if (numbers[0] <= 0.0)
        return false;

    *diameter = numbers[0];
    *minDiameter = numbers[0];
    *maxDiameter = numbers[0];
    *isRange = false;
    return true;
}

static bool parseOptionalPositiveDouble(const std::string& text, double* value)
{
    std::string trimmed = trim(text);
    if (trimmed.empty() || trimmed == "*")
        return false;

    std::vector<double> numbers = extractNumbers(trimmed);
    if (numbers.empty() || numbers[0] <= 0.0)
        return false;

    *value = numbers[0];
    return true;
}

static int thicknessLevelIndex(double height)
{
    if (height >= 0.8 - DiameterTolerance && height <= 1.4 + DiameterTolerance)
        return 0;
    if (height > 1.4 - DiameterTolerance && height <= 2.3 + DiameterTolerance)
        return 1;
    if (height > 2.3 - DiameterTolerance && height <= 3.2 + DiameterTolerance)
        return 2;
    return -1;
}

static std::string specForLevelText(const std::string& text, int levelIndex)
{
    if (levelIndex < 0)
        return std::string();

    std::string key = "-" + std::to_string(levelIndex + 1);
    std::vector<std::string> parts;
    std::string current;
    for (size_t i = 0; i < text.size(); ++i)
    {
        char ch = text[i];
        if (ch == ';' || ch == ',' || ch == '\n' || ch == '\r')
        {
            parts.push_back(trim(current));
            current.clear();
        }
        else
        {
            current.push_back(ch);
        }
    }
    parts.push_back(trim(current));

    for (size_t i = 0; i < parts.size(); ++i)
    {
        std::string part = parts[i];
        if (part.empty())
            continue;

        size_t separator = part.find('=');
        if (separator == std::string::npos)
            separator = part.find(':');
        if (separator == std::string::npos)
            separator = part.find(u8"：");

        if (separator != std::string::npos)
        {
            if (trim(part.substr(0, separator)) == key)
                return trim(part.substr(separator + 1));
        }
        else if (part.size() >= 2 && part.substr(part.size() - 2) == key)
        {
            return part;
        }
    }

    if (parts.size() == 3 && levelIndex < (int)parts.size())
        return parts[(size_t)levelIndex];

    return std::string();
}

static std::vector<std::string> splitLines(const std::string& text)
{
    std::vector<std::string> values;
    std::string current;
    for (size_t i = 0; i < text.size(); ++i)
    {
        char ch = text[i];
        if (ch == '\r')
            continue;
        if (ch == '\n')
        {
            values.push_back(trim(current));
            current.clear();
        }
        else
        {
            current.push_back(ch);
        }
    }
    values.push_back(trim(current));
    return values;
}

static std::vector<std::string> splitTypeCell(const std::string& text)
{
    std::vector<std::string> values;
    std::string current;
    for (size_t i = 0; i < text.size(); ++i)
    {
        char ch = text[i];
        if (ch == '\r')
            continue;
        if (ch == '\n' || ch == ';' || ch == ',')
        {
            std::string value = trim(current);
            if (!value.empty())
                values.push_back(value);
            current.clear();
        }
        else
        {
            current.push_back(ch);
        }
    }

    std::string value = trim(current);
    if (!value.empty())
        values.push_back(value);
    return values;
}

static bool excelInvoke(IDispatch* object, const wchar_t* name, WORD flags, VARIANT* result, VARIANT* args, int argCount)
{
    if (object == NULL)
        return false;

    DISPID dispId = 0;
    LPOLESTR oleName = const_cast<LPOLESTR>(name);
    if (FAILED(object->GetIDsOfNames(IID_NULL, &oleName, 1, LOCALE_USER_DEFAULT, &dispId)))
        return false;

    DISPPARAMS params;
    params.rgvarg = args;
    params.rgdispidNamedArgs = NULL;
    params.cArgs = argCount;
    params.cNamedArgs = 0;

    EXCEPINFO excepInfo;
    memset(&excepInfo, 0, sizeof(excepInfo));
    UINT argError = 0;
    return SUCCEEDED(object->Invoke(dispId, IID_NULL, LOCALE_USER_DEFAULT, flags, &params, result, &excepInfo, &argError));
}

static bool excelInvoke(IDispatch* object, const wchar_t* name, WORD flags, VARIANT* result)
{
    return excelInvoke(object, name, flags, result, NULL, 0);
}

static IDispatch* variantDispatch(VARIANT& value)
{
    if (value.vt == VT_DISPATCH)
        return value.pdispVal;
    return NULL;
}

static std::string variantToString(VARIANT& value)
{
    if (value.vt == VT_EMPTY || value.vt == VT_NULL)
        return std::string();

    VARIANT copy;
    VariantInit(&copy);
    if (SUCCEEDED(VariantChangeType(&copy, &value, 0, VT_BSTR)))
    {
        std::string text = wideToUtf8(copy.bstrVal);
        VariantClear(&copy);
        return trim(text);
    }

    return std::string();
}

static std::string excelCellText(IDispatch* sheet, long row, long column)
{
    if (sheet == NULL)
        return std::string();

    VARIANT args[2];
    VariantInit(&args[0]);
    VariantInit(&args[1]);
    args[0].vt = VT_I4;
    args[0].lVal = column;
    args[1].vt = VT_I4;
    args[1].lVal = row;

    VARIANT cellValue;
    VariantInit(&cellValue);
    if (!excelInvoke(sheet, L"Cells", DISPATCH_PROPERTYGET, &cellValue, args, 2))
        return std::string();

    IDispatch* cell = variantDispatch(cellValue);
    if (cell == NULL)
    {
        VariantClear(&cellValue);
        return std::string();
    }

    VARIANT value;
    VariantInit(&value);
    std::string text;
    if (excelInvoke(cell, L"Value", DISPATCH_PROPERTYGET, &value))
        text = variantToString(value);

    VariantClear(&value);
    VariantClear(&cellValue);
    return text;
}

static bool addSpecRuleFromValues(const std::vector<std::string>& values, std::vector<HoleAttribute::SpecRule>& specRules)
{
    if (values.size() < 3)
        return false;

    HoleAttribute::SpecRule rule;
    if (!parseDiameterRule(values[0], &rule.diameter, &rule.minDiameter, &rule.maxDiameter, &rule.isRange))
        return false;

    std::vector<std::string> typeValues = splitTypeCell(csvTextToUtf8(values[1]));
    if (typeValues.empty())
        return false;

    rule.minHeight = 0.0;
    rule.maxHeight = 0.0;
    rule.hasHeightRange = false;
    rule.levelSpecs.clear();

    if (values.size() >= 7)
    {
        rule.spec = csvTextToUtf8(values[2]);
        for (size_t i = 3; i <= 5; ++i)
            rule.levelSpecs.push_back(csvTextToUtf8(values[i]));
        rule.remark = csvTextToUtf8(values[6]);
    }
    else
    {
        size_t specIndex = 2;
        size_t remarkIndex = 3;
        if (values.size() >= 6)
        {
            specIndex = 4;
            remarkIndex = 5;
            double minHeight = 0.0;
            double maxHeight = 0.0;
            if (parseOptionalPositiveDouble(values[2], &minHeight) &&
                parseOptionalPositiveDouble(values[3], &maxHeight) &&
                maxHeight >= minHeight)
            {
                rule.minHeight = minHeight;
                rule.maxHeight = maxHeight;
                rule.hasHeightRange = true;
            }
        }

        rule.spec = values.size() > specIndex ? csvTextToUtf8(values[specIndex]) : std::string();
        rule.remark = values.size() > remarkIndex ? csvTextToUtf8(values[remarkIndex]) : std::string();
    }

    std::vector<std::string> specValues = splitLines(rule.spec);
    std::vector<std::string> remarkValues = splitLines(rule.remark);

    bool addedAny = false;
    for (size_t typeIndex = 0; typeIndex < typeValues.size(); ++typeIndex)
    {
        HoleAttribute::SpecRule expandedRule = rule;
        expandedRule.type = typeValues[typeIndex];
        if (typeValues.size() > 1 && specValues.size() == typeValues.size())
            expandedRule.spec = specValues[typeIndex];
        if (typeValues.size() > 1 && remarkValues.size() == typeValues.size())
            expandedRule.remark = remarkValues[typeIndex];

        expandedRule.levelSpecs.clear();
        if (!expandedRule.spec.empty())
        {
            for (int levelIndex = 0; levelIndex < 3; ++levelIndex)
                expandedRule.levelSpecs.push_back(specForLevelText(expandedRule.spec, levelIndex));
        }

        bool hasLevelSpec = false;
        for (size_t i = 0; i < expandedRule.levelSpecs.size(); ++i)
            hasLevelSpec = hasLevelSpec || !expandedRule.levelSpecs[i].empty();

        if (expandedRule.type.empty() || (expandedRule.spec.empty() && !hasLevelSpec))
            continue;

        specRules.push_back(expandedRule);
        addedAny = true;
    }

    return addedAny;
}

static std::string replaceAll(std::string text, const std::string& from, const std::string& to)
{
    size_t pos = 0;
    while ((pos = text.find(from, pos)) != std::string::npos)
    {
        text.replace(pos, from.size(), to);
        pos += to.size();
    }
    return text;
}

static std::string matrixSpecBase(const std::string& tableTitle, const std::string& category, const std::string& threadText)
{
    if (tableTitle.find("孔标注") != std::string::npos)
        return std::string();
    if (tableTitle.find("螺牙标注") != std::string::npos)
        return threadText;
    if (tableTitle.find("焊接螺母") != std::string::npos)
        return "PEM " + category + "-" + threadText;
    if (tableTitle.find("压铆螺钉") != std::string::npos)
        return "PEM " + category + "-" + threadText;
    if (tableTitle.find("压铆螺母柱") != std::string::npos)
        return "PEM " + category + "-" + threadText;
    if (tableTitle.find("沉孔") != std::string::npos)
        return threadText + "螺丝沉孔";
    return category + "-" + threadText;
}

static std::string renderMatrixText(std::string text, const std::string& spec, const std::string& threadText, const std::string& diameterText, const std::string& category)
{
    text = replaceAll(text, "{规格}", spec);
    text = replaceAll(text, "{螺纹}", threadText);
    text = replaceAll(text, "{底孔}", diameterText);
    text = replaceAll(text, "{子类型}", category);
    text = replaceAll(text, "{长度}", "");
    return trim(text);
}

static bool addMatrixRule(const std::string& diameterText, const std::string& tableTitle, const std::string& category, const std::string& threadText, const std::string& labelText, const std::string& description, std::vector<HoleAttribute::SpecRule>& specRules)
{
    if (diameterText.empty() || category.empty() || threadText.empty())
        return false;

    HoleAttribute::SpecRule rule;
    if (!parseDiameterRule(diameterText, &rule.diameter, &rule.minDiameter, &rule.maxDiameter, &rule.isRange))
        return false;

    rule.minHeight = 0.0;
    rule.maxHeight = 0.0;
    rule.hasHeightRange = false;
    rule.type = category;
    rule.sourceTitle = tableTitle;
    std::string baseSpec = matrixSpecBase(tableTitle, category, threadText);
    rule.spec = labelText.empty()
        ? renderMatrixText(baseSpec, baseSpec, threadText, diameterText, category)
        : renderMatrixText(labelText, threadText, threadText, diameterText, category);
    rule.levelSpecs.clear();
    if (tableTitle.find("压铆螺母") != std::string::npos &&
        tableTitle.find("压铆螺母柱") == std::string::npos)
    {
        rule.levelSpecs.push_back(rule.spec + "-1");
        rule.levelSpecs.push_back(rule.spec + "-2");
        rule.levelSpecs.push_back(rule.spec + "-3");
    }
    rule.remark = trim(description);

    specRules.push_back(rule);
    return true;
}

static long findMatrixHeaderRow(IDispatch* sheet, const std::string& headerText)
{
    for (long row = 1; row <= 30; ++row)
    {
        if (excelCellText(sheet, row, 1) == headerText)
            return row;
    }
    return 0;
}

static bool loadMatrixSheet(IDispatch* sheet, std::vector<HoleAttribute::SpecRule>& specRules, std::vector<std::string>& specRuleTypes)
{
    std::string title = excelCellText(sheet, 1, 1);
    long categoryRow = findMatrixHeaderRow(sheet, "绫诲埆");
    long labelRow = findMatrixHeaderRow(sheet, "鏍囨敞鏂囨湰");
    long descriptionRow = findMatrixHeaderRow(sheet, "璇存槑");
    if (categoryRow <= 0 || labelRow <= 0 || descriptionRow <= 0)
        return false;

    std::vector<long> categoryColumns;
    for (long column = 2; column <= 20; ++column)
    {
        std::string category = excelCellText(sheet, categoryRow, column);
        if (!category.empty())
        {
            categoryColumns.push_back(column);
            addUniqueString(specRuleTypes, category);
        }
    }

    if (categoryColumns.empty())
        return false;

    for (long row = descriptionRow + 1; row <= 1000; ++row)
    {
        std::string diameterText = excelCellText(sheet, row, 1);
        if (diameterText.empty())
            break;

        for (size_t i = 0; i < categoryColumns.size(); ++i)
        {
            long column = categoryColumns[i];
            std::string category = excelCellText(sheet, categoryRow, column);
            std::string threadText = excelCellText(sheet, row, column);
            std::string labelText = excelCellText(sheet, labelRow, column);
            std::string description = excelCellText(sheet, descriptionRow, column);
            addMatrixRule(diameterText, title, category, threadText, labelText, description, specRules);
        }
    }

    return true;
}

static NXString utf8NxString(const std::string& text)
{
    return NXString(text.c_str(), NXString::UTF8);
}

static NXString utf8NxString(const char* text)
{
    return NXString(text, NXString::UTF8);
}

static void setNodeColumnText(Node* node, int columnID, const std::string& text)
{
    if (node != NULL)
        node->SetColumnDisplayText(columnID, utf8NxString(text));
}

static void setFaceStringAttribute(Face* face, const char* title, const std::string& value)
{
    if (face == NULL)
        return;

    face->SetUserAttribute(utf8NxString(title), -1, utf8NxString(value), Update::OptionNow);
}

static void deleteFaceStringAttributeIfExists(Face* face, const char* title)
{
    if (face == NULL)
        return;

    try
    {
        face->DeleteUserAttribute(NXObject::AttributeType::AttributeTypeString, utf8NxString(title), true, Update::OptionNow);
    }
    catch (...)
    {
    }
}

static void refreshDisplay()
{
    UF_DISP_make_display_up_to_date();
}

static void checkUf(int code, const char* apiName)
{
    if (code == 0)
        return;

    char message[133] = "";
    UF_get_fail_message(code, message);
    std::ostringstream stream;
    stream << apiName << " 调用失败：" << message << " (" << code << ")";
    throw std::runtime_error(stream.str());
}

static double dot3(const double a[3], const double b[3])
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static bool normalize3(double vector[3])
{
    double length = std::sqrt(dot3(vector, vector));
    if (length <= 1.0e-9)
        return false;

    vector[0] /= length;
    vector[1] /= length;
    vector[2] /= length;
    return true;
}

static void cross3(const double a[3], const double b[3], double result[3])
{
    result[0] = a[1] * b[2] - a[2] * b[1];
    result[1] = a[2] * b[0] - a[0] * b[2];
    result[2] = a[0] * b[1] - a[1] * b[0];
}

static double distance3(const double a[3], const double b[3])
{
    double dx = a[0] - b[0];
    double dy = a[1] - b[1];
    double dz = a[2] - b[2];
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

struct PlanarFaceMeasure
{
    tag_t tag;
    Face* face;
    double area;
    double point[3];
    double normal[3];
};

static double projectedBoxArea(const double box[6], const double normal[3])
{
    double reference[3] = { 1.0, 0.0, 0.0 };
    if (std::fabs(dot3(normal, reference)) > 0.9)
    {
        reference[0] = 0.0;
        reference[1] = 1.0;
    }

    double axisU[3] = { 0.0, 0.0, 0.0 };
    cross3(normal, reference, axisU);
    if (!normalize3(axisU))
        return 0.0;

    double axisV[3] = { 0.0, 0.0, 0.0 };
    cross3(normal, axisU, axisV);
    if (!normalize3(axisV))
        return 0.0;

    double minU = std::numeric_limits<double>::max();
    double maxU = -std::numeric_limits<double>::max();
    double minV = std::numeric_limits<double>::max();
    double maxV = -std::numeric_limits<double>::max();

    for (int ix = 0; ix < 2; ++ix)
    {
        for (int iy = 0; iy < 2; ++iy)
        {
            for (int iz = 0; iz < 2; ++iz)
            {
                double point[3] = {
                    box[ix == 0 ? 0 : 3],
                    box[iy == 0 ? 1 : 4],
                    box[iz == 0 ? 2 : 5]
                };
                double u = dot3(point, axisU);
                double v = dot3(point, axisV);
                minU = std::min(minU, u);
                maxU = std::max(maxU, u);
                minV = std::min(minV, v);
                maxV = std::max(maxV, v);
            }
        }
    }

    double width = maxU - minU;
    double height = maxV - minV;
    return width > 0.0 && height > 0.0 ? width * height : 0.0;
}

static bool askFaceTrueArea(Face* face, double* area)
{
    if (area == NULL)
        return false;

    *area = 0.0;
    if (face == NULL)
        return false;

    Part* workPart = Session::GetSession()->Parts()->Work();
    if (workPart == NULL || workPart->MeasureManager() == NULL)
        return false;

    MeasureFaces* measureFaces = NULL;
    try
    {
        Unit* areaUnit = NULL;
        Unit* lengthUnit = NULL;
        if (workPart->UnitCollection() != NULL)
        {
            areaUnit = workPart->UnitCollection()->GetBase("Area");
            lengthUnit = workPart->UnitCollection()->GetBase("Length");
        }

        std::vector<IParameterizedSurface*> faces;
        faces.push_back(face);
        measureFaces = workPart->MeasureManager()->NewFaceProperties(areaUnit, lengthUnit, 0.99, faces);
        if (measureFaces == NULL)
            return false;

        *area = measureFaces->Area();
        delete measureFaces;
        measureFaces = NULL;
        return *area > DiameterTolerance;
    }
    catch (...)
    {
        if (measureFaces != NULL)
            delete measureFaces;
        return false;
    }
}

static bool askPlanarFaceMeasure(tag_t faceTag, PlanarFaceMeasure* measure)
{
    if (measure == NULL || faceTag == NULL_TAG)
        return false;

    int faceType = 0;
    double point[3] = { 0.0, 0.0, 0.0 };
    double direction[3] = { 0.0, 0.0, 0.0 };
    double box[6] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
    double radius = 0.0;
    double radiusData = 0.0;
    int normalDirection = 0;
    if (UF_MODL_ask_face_data(faceTag, &faceType, point, direction, box, &radius, &radiusData, &normalDirection) != 0 ||
        faceType != UF_MODL_PLANAR_FACE)
    {
        return false;
    }

    if (!normalize3(direction))
        return false;

    TaggedObject* taggedFace = NXObjectManager::Get(faceTag);
    Face* face = dynamic_cast<Face*>(taggedFace);
    if (face == NULL)
        return false;

    double area = 0.0;
    if (!askFaceTrueArea(face, &area))
        area = projectedBoxArea(box, direction);

    if (area <= DiameterTolerance)
        return false;

    measure->tag = faceTag;
    measure->face = face;
    measure->area = area;
    measure->point[0] = point[0];
    measure->point[1] = point[1];
    measure->point[2] = point[2];
    measure->normal[0] = direction[0];
    measure->normal[1] = direction[1];
    measure->normal[2] = direction[2];
    return true;
}

static std::string moduleDirectory()
{
    HMODULE module = NULL;
    GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&moduleDirectory),
        &module);

    std::vector<wchar_t> path(32768);
    DWORD length = GetModuleFileNameW(module, &path[0], (DWORD)path.size());
    if (length == 0 || length >= path.size())
        return std::string();

    std::string fullPath = wideToUtf8(&path[0]);
    size_t slash = fullPath.find_last_of("\\/");
    if (slash == std::string::npos)
        return std::string();

    return fullPath.substr(0, slash);
}

static bool fileExists(const std::string& path)
{
    if (path.empty())
        return false;

    std::wstring widePath = utf8ToWide(path);
    if (widePath.empty())
        return false;

    DWORD attributes = GetFileAttributesW(widePath.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static std::string parentDirectory(const std::string& path)
{
    size_t slash = path.find_last_of("\\/");
    if (slash == std::string::npos)
        return std::string();
    return path.substr(0, slash);
}

static std::string resolveRuleWorkbookPath(const std::string& directory)
{
    std::vector<std::string> candidates;
    if (!directory.empty())
    {
        candidates.push_back(directory + "\\DATA\\HoleAttributeRules.ini");
        candidates.push_back(directory + "\\data\\HoleAttributeRules.ini");
        candidates.push_back(parentDirectory(directory) + "\\DATA\\HoleAttributeRules.ini");
        candidates.push_back(parentDirectory(directory) + "\\data\\HoleAttributeRules.ini");
        candidates.push_back(directory + "\\HoleAttributeRules.ini");
    }
    candidates.push_back("D:\\UG智辉钣金插件\\DATA\\HoleAttributeRules.ini");
    candidates.push_back("HoleAttributeRules.ini");

    for (size_t i = 0; i < candidates.size(); ++i)
    {
        if (fileExists(candidates[i]))
            return candidates[i];
    }

    return directory.empty() ? "HoleAttributeRules.ini" : directory + "\\DATA\\HoleAttributeRules.ini";
}

static void trySetStringProperty(UIBlock* block, const char* propertyName, const char* value)
{
    if (block == NULL)
        return;

    PropertyList* props = NULL;
    try
    {
        props = block->GetProperties();
        props->SetString(propertyName, value);
    }
    catch (...)
    {
    }

    if (props != NULL)
        delete props;
}

static void trySetVisible(UIBlock* block, bool visible)
{
    if (block == NULL)
        return;

    const char* names[] = { "Show", "Visibility" };
    for (size_t i = 0; i < 2; ++i)
    {
        PropertyList* props = NULL;
        try
        {
            props = block->GetProperties();
            props->SetLogical(names[i], visible);
        }
        catch (...)
        {
        }

        if (props != NULL)
            delete props;
    }
}

static bool tryGetLogicalProperty(UIBlock* block, const char* propertyName, bool fallback)
{
    if (block == NULL)
        return fallback;

    PropertyList* props = NULL;
    try
    {
        props = block->GetProperties();
        bool value = props->GetLogical(propertyName);
        delete props;
        return value;
    }
    catch (...)
    {
        if (props != NULL)
            delete props;
    }

    return fallback;
}

HoleAttribute::HoleAttribute()
    : theDialog(NULL),
      selection0(NULL),
      tree_control0(NULL),
      addCrossSelectionNodeButton(NULL),
      openRuleTableButton(NULL),
      autoTapHoleToggle(NULL),
      autoPemHoleToggle(NULL),
      autoCounterboreHoleToggle(NULL),
      ruleManager(NULL),
      highlightedGroupIndex(-1),
      columnsInserted(false),
      isEditingDiameter(false),
      specRulesLoaded(false),
      autoTapHoleEnabled(false),
      autoPemHoleEnabled(false),
      autoCounterboreHoleEnabled(false)
{
    HoleAttribute::theSession = Session::GetSession();
    HoleAttribute::theUI = UI::GetUI();
    std::string directory = moduleDirectory();
    theDlxFileName = directory.empty() ? "HoleAttribute.dlx" : directory + "\\HoleAttribute.dlx";
    ruleManager = new KonBiaoZu::ExcelRuleManager("HoleAttributeRules.ini");
    ruleFileName = static_cast<KonBiaoZu::ExcelRuleManager*>(ruleManager)->WorkbookPath();
    if (ruleFileName.empty())
        ruleFileName = resolveRuleWorkbookPath(directory);
    theDialog = HoleAttribute::theUI->CreateDialog(theDlxFileName.c_str());

    theDialog->AddApplyHandler(make_callback(this, &HoleAttribute::apply_cb));
    theDialog->AddOkHandler(make_callback(this, &HoleAttribute::ok_cb));
    theDialog->AddUpdateHandler(make_callback(this, &HoleAttribute::update_cb));
    theDialog->AddInitializeHandler(make_callback(this, &HoleAttribute::initialize_cb));
    theDialog->AddDialogShownHandler(make_callback(this, &HoleAttribute::dialogShown_cb));
}

HoleAttribute::~HoleAttribute()
{
    clearHighlight();
    if (theDialog != NULL)
    {
        delete theDialog;
        theDialog = NULL;
    }
    if (ruleManager != NULL)
    {
        delete static_cast<KonBiaoZu::ExcelRuleManager*>(ruleManager);
        ruleManager = NULL;
    }
}


#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

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
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&LoadProtectedLicenseGate), &selfModule))
    {
        wchar_t localPath[MAX_PATH] = { 0 };
        DWORD length = GetModuleFileNameW(selfModule, localPath, MAX_PATH);
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

    HMODULE fixedModule = LoadLibraryW(L"D:\\UG鏅鸿緣閽ｉ噾鎻掍欢\\application\\ZhaoFuNxLicenseGate.dll");
    if (fixedModule != NULL)
    {
        return fixedModule;
    }

    return LoadLibraryW(moduleName);
}

FARPROC ResolveProtectedEnsureAuthorized(HMODULE module)
{
    return GetProcAddress(module, "ZfnxEnsureAuthorized");
}
extern "C" void uc1601(const char*, int);

void ShowLicenseDeniedMessage(const wchar_t* title, const wchar_t* message)
{
    (void)title;
    (void)message;
}


#ifdef ZH_PROTECTED_BUILD
bool ValidateOwnModuleChecksum()
{
    HMODULE module = NULL;
    if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&ValidateOwnModuleChecksum),
            &module))
    {
        return false;
    }

    wchar_t path[MAX_PATH] = { 0 };
    const DWORD pathLength = GetModuleFileNameW(module, path, MAX_PATH);
    if (pathLength == 0 || pathLength >= MAX_PATH)
    {
        return false;
    }

    HANDLE file = CreateFileW(
        path,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    LARGE_INTEGER fileSize = {};
    if (!GetFileSizeEx(file, &fileSize) || fileSize.QuadPart <= 0 || fileSize.QuadPart > 256LL * 1024LL * 1024LL)
    {
        CloseHandle(file);
        return false;
    }

    const DWORD size = static_cast<DWORD>(fileSize.QuadPart);
    BYTE* bytes = static_cast<BYTE*>(HeapAlloc(GetProcessHeap(), 0, size));
    if (bytes == NULL)
    {
        CloseHandle(file);
        return false;
    }

    DWORD bytesRead = 0;
    const BOOL readOk = ReadFile(file, bytes, size, &bytesRead, NULL);
    CloseHandle(file);
    if (!readOk || bytesRead != size)
    {
        HeapFree(GetProcessHeap(), 0, bytes);
        return false;
    }

    bool valid = false;
    if (size > sizeof(IMAGE_DOS_HEADER))
    {
        IMAGE_DOS_HEADER* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(bytes);
        if (dos->e_magic == IMAGE_DOS_SIGNATURE && dos->e_lfanew > 0 &&
            static_cast<DWORD>(dos->e_lfanew) + sizeof(IMAGE_NT_HEADERS) < size)
        {
            IMAGE_NT_HEADERS* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(bytes + dos->e_lfanew);
            if (nt->Signature == IMAGE_NT_SIGNATURE)
            {
                const DWORD storedChecksum = nt->OptionalHeader.CheckSum;
                const DWORD checksumOffset = static_cast<DWORD>(
                    reinterpret_cast<BYTE*>(&nt->OptionalHeader.CheckSum) - bytes);

                DWORD checksum = 0;
                for (DWORD offset = 0; offset < size; offset += 2)
                {
                    if (offset == checksumOffset || offset == checksumOffset + 2)
                    {
                        continue;
                    }

                    DWORD word = bytes[offset];
                    if (offset + 1 < size)
                    {
                        word |= static_cast<DWORD>(bytes[offset + 1]) << 8;
                    }
                    checksum += word;
                    checksum = (checksum & 0xffff) + (checksum >> 16);
                }
                checksum = (checksum & 0xffff) + (checksum >> 16);
                checksum += size;

                valid = storedChecksum != 0 && checksum == storedChecksum;
            }
        }
    }

    HeapFree(GetProcessHeap(), 0, bytes);
    return valid;
}
#else
bool ValidateOwnModuleChecksum()
{
    return true;
}
#endif
bool EnsureAuthorized(const wchar_t* featureCode, const wchar_t* displayName)
{
    (void)featureCode;
    (void)displayName;
    return true;

    wchar_t message[1024] = { 0 };
    
    if (!ValidateOwnModuleChecksum())
    {
        OutputDebugStringW(L"Protected module checksum validation failed.\n");
        return false;
    }
HMODULE module = LoadProtectedLicenseGate();
    if (module == NULL)
    {
        ShowLicenseDeniedMessage(displayName, L"License component was not found.");
        return false;
    }

    EnsureAuthorizedProc ensureAuthorized = reinterpret_cast<EnsureAuthorizedProc>(ResolveProtectedEnsureAuthorized(module));
    if (ensureAuthorized == NULL)
    {
        ShowLicenseDeniedMessage(displayName, L"License component entry is invalid.");
        return false;
    }

        const int ok = ensureAuthorized(featureCode, displayName, message, static_cast<int>(sizeof(message) / sizeof(message[0])));
    const int ok2 = ok == 1 ? ensureAuthorized(featureCode, displayName, message, static_cast<int>(sizeof(message) / sizeof(message[0]))) : ok;
    if (ok != 1 || ok2 != 1)
    {
        ShowLicenseDeniedMessage(displayName, message[0] != L'\0' ? message : L"License check failed.");
        return false;
    }

    return true;
}
}
extern "C" DllExport void ufusr(char* param, int* retcod, int param_len)
{
    if (!zhihui_license_guard::EnsureAuthorized(L"ZHIHUI.CUANJIANKONSUXIN", L"CuanJianKonSuXin"))
    {
        return;
    }

    HoleAttribute* dialog = NULL;
    try
    {
        checkUf(UF_initialize(), "UF_initialize");
        dialog = new HoleAttribute();
        dialog->Show();
    }
    catch (std::exception& ex)
    {
        if (HoleAttribute::theUI != NULL)
        {
            HoleAttribute::theUI->NXMessageBox()->Show("孔属性", NXMessageBox::DialogTypeError, ex.what());
        }
    }

    if (dialog != NULL)
        delete dialog;

    UF_terminate();
}

extern "C" DllExport int ufusr_ask_unload()
{
    return UF_UNLOAD_IMMEDIATELY;
}

extern "C" DllExport void ufusr_cleanup(void)
{
}

BlockDialog::DialogResponse HoleAttribute::Show()
{
    return theDialog->Launch();
}

void HoleAttribute::initialize_cb()
{
    try
    {
        selection0 = dynamic_cast<UIBlock*>(theDialog->TopBlock()->FindBlock("selection0"));
        tree_control0 = dynamic_cast<Tree*>(theDialog->TopBlock()->FindBlock("tree_control0"));
        addCrossSelectionNodeButton = dynamic_cast<UIBlock*>(theDialog->TopBlock()->FindBlock("addCrossSelectionNodeButton"));
        openRuleTableButton = dynamic_cast<UIBlock*>(theDialog->TopBlock()->FindBlock("addNodeButton"));
        autoTapHoleToggle = dynamic_cast<UIBlock*>(theDialog->TopBlock()->FindBlock("autoTapHoleToggle"));
        autoPemHoleToggle = dynamic_cast<UIBlock*>(theDialog->TopBlock()->FindBlock("autoPemHoleToggle"));
        autoCounterboreHoleToggle = dynamic_cast<UIBlock*>(theDialog->TopBlock()->FindBlock("autoCounterboreHoleToggle"));

        configureSelectionBlock();
        localizeDialogBlocks();
        refreshAutoRecognitionToggles();
        specRulesLoaded = false;
        loadSpecRules();

        if (tree_control0 != NULL)
        {
            tree_control0->SetOnSelectHandler(make_callback(this, &HoleAttribute::OnSelectCallback));
            tree_control0->SetOnBeginLabelEditHandler(make_callback(this, &HoleAttribute::OnBeginLabelEditCallback));
            tree_control0->SetOnEndLabelEditHandler(make_callback(this, &HoleAttribute::OnEndLabelEditCallback));
            tree_control0->SetAskEditControlHandler(make_callback(this, &HoleAttribute::AskEditControlCallback));
            tree_control0->SetOnEditOptionSelectedHandler(make_callback(this, &HoleAttribute::OnEditOptionSelectedCallback));
        }
    }
    catch (std::exception& ex)
    {
        showError(ex.what());
    }
}

void HoleAttribute::dialogShown_cb()
{
    try
    {
        if (tree_control0 != NULL && !columnsInserted)
        {
            tree_control0->InsertColumn(ColumnDiameter, utf8NxString("直径"), 80);
            tree_control0->InsertColumn(ColumnCount, utf8NxString("数量"), 60);
            tree_control0->InsertColumn(ColumnType, utf8NxString("类型"), 90);
            tree_control0->InsertColumn(ColumnSpec, utf8NxString("规格"), 105);
            tree_control0->InsertColumn(ColumnRemark, utf8NxString("备注"), 130);
            tree_control0->SetColumnResizePolicy(ColumnDiameter, Tree::ColumnResizePolicyConstantWidth);
            tree_control0->SetColumnResizePolicy(ColumnCount, Tree::ColumnResizePolicyConstantWidth);
            tree_control0->SetColumnResizePolicy(ColumnType, Tree::ColumnResizePolicyConstantWidth);
            tree_control0->SetColumnResizePolicy(ColumnSpec, Tree::ColumnResizePolicyConstantWidth);
            tree_control0->SetColumnResizePolicy(ColumnRemark, Tree::ColumnResizePolicyResizeWithTree);
            columnsInserted = true;
        }
    }
    catch (std::exception& ex)
    {
        showError(ex.what());
    }
}

int HoleAttribute::apply_cb()
{
    writeAllFaceAttributes();
    return 0;
}

int HoleAttribute::ok_cb()
{
    int errorCode = apply_cb();
    clearHighlight();
    return errorCode;
}

int HoleAttribute::update_cb(UIBlock* block)
{
    try
    {
        if (isEditingDiameter)
            return 0;

        if (block == selection0 || block == addCrossSelectionNodeButton)
        {
            refreshAutoRecognitionToggles();
            rebuildFromSelection();
        }
        else if (block == autoTapHoleToggle || block == autoPemHoleToggle || block == autoCounterboreHoleToggle)
        {
            refreshAutoRecognitionToggles();
            ensureSpecRulesLoaded();
            if (autoTapHoleEnabled || autoPemHoleEnabled || autoCounterboreHoleEnabled)
            {
                if (autoTapHoleEnabled || autoPemHoleEnabled)
                    applyAutoRecognitionToAll();
                applyCounterboreRecognitionToAll();
                writeAllFaceAttributes();
                rebuildTree();
            }
        }
        else if (block == openRuleTableButton)
        {
            std::string errorMessage;
            KonBiaoZu::ExcelRuleManager* manager = static_cast<KonBiaoZu::ExcelRuleManager*>(ruleManager);
            if (manager == NULL || !manager->OpenWorkbook(errorMessage))
            {
                showError(errorMessage.empty() ? ("鏃犳硶鎵撳紑瑙勫垯琛細" + ruleFileName) : errorMessage);
            }
            else
            {
                ruleFileName = manager->WorkbookPath();
                specRulesLoaded = false;
                loadSpecRules();
                if (autoTapHoleEnabled || autoPemHoleEnabled)
                    applyAutoRecognitionToAll();
                applyCounterboreRecognitionToAll();
                writeAllFaceAttributes();
                rebuildTree();
            }
        }
    }
    catch (std::exception& ex)
    {
        showError(ex.what());
        return 1;
    }
    return 0;
}

void HoleAttribute::OnSelectCallback(Tree* tree, Node* node, int columnID, bool selected)
{
    std::map<Node*, size_t>::const_iterator it = nodeToGroup.find(node);
    if (selected && it != nodeToGroup.end())
    {
        highlightGroup(it->second);
        return;
    }

    if (!selected && it != nodeToGroup.end() && highlightedGroupIndex == (int)it->second)
        clearHighlight();
}

Tree::BeginLabelEditState HoleAttribute::OnBeginLabelEditCallback(Tree* tree, Node* node, int columnID)
{
    if (columnID == ColumnDiameter || columnID == ColumnType || columnID == ColumnSpec)
        return Tree::BeginLabelEditStateAllow;

    return Tree::BeginLabelEditStateDisallow;
}

Tree::EndLabelEditState HoleAttribute::OnEndLabelEditCallback(Tree* tree, Node* node, int columnID, NXString editedText)
{
    try
    {
        std::map<Node*, size_t>::iterator it = nodeToGroup.find(node);
        if (it == nodeToGroup.end())
            return Tree::EndLabelEditStateRejectText;

        HoleGroup& group = groups[it->second];
        std::string text = nxStringToString(editedText);

        if (columnID == ColumnDiameter)
        {
            double newDiameter = std::stod(text);
            if (newDiameter <= 0.0)
                return Tree::EndLabelEditStateRejectText;

            isEditingDiameter = true;
            applyDiameterChange(it->second, newDiameter);
            setNodeColumnText(node, ColumnDiameter, formatDiameter(groups[it->second].diameter));
            setNodeColumnText(node, ColumnCount, formatCount(groups[it->second].faces.size()));
            setNodeColumnText(node, ColumnType, groups[it->second].type);
            setNodeColumnText(node, ColumnSpec, groups[it->second].spec);
            setNodeColumnText(node, ColumnRemark, groups[it->second].remark);
            writeGroupFaceAttributes(it->second);
            isEditingDiameter = false;
            return Tree::EndLabelEditStateAcceptText;
        }

        if (columnID == ColumnSpec)
        {
            group.spec = text;
            setNodeColumnText(node, ColumnSpec, group.spec);
            writeGroupFaceAttributes(it->second);
            return Tree::EndLabelEditStateAcceptText;
        }

        if (columnID == ColumnType)
            return Tree::EndLabelEditStateAcceptText;
    }
    catch (std::exception& ex)
    {
        isEditingDiameter = false;
        showError(ex.what());
    }

    return Tree::EndLabelEditStateRejectText;
}

Tree::ControlType HoleAttribute::AskEditControlCallback(Tree* tree, Node* node, int columnID)
{
    if (columnID == ColumnType)
    {
        std::vector<std::string> typeNames = typeOptions();
        std::vector<NXString> options;
        for (size_t i = 0; i < typeNames.size(); ++i)
            options.push_back(utf8NxString(typeNames[i]));
        tree->SetEditOptions(options, 0);
        return Tree::ControlTypeComboBox;
    }

    return Tree::ControlTypeNone;
}

Tree::EditControlOption HoleAttribute::OnEditOptionSelectedCallback(Tree* tree, Node* node, int columnID, int selectedOptionID, NXString selectedOptionText, Tree::ControlType type)
{
    std::map<Node*, size_t>::iterator it = nodeToGroup.find(node);
    if (it == nodeToGroup.end() || columnID != ColumnType)
        return Tree::EditControlOptionReject;

    HoleGroup& group = groups[it->second];
    std::vector<std::string> typeNames = typeOptions();
    if (selectedOptionID >= 0 && selectedOptionID < (int)typeNames.size())
        group.type = typeNames[(size_t)selectedOptionID];
    else
        group.type = nxStringToString(selectedOptionText);
    group.spec = specFor(group.type, group.diameter, group.height);
    group.remark = remarkFor(group.type, group.diameter, group.height);
    setNodeColumnText(node, ColumnType, group.type);
    setNodeColumnText(node, ColumnSpec, group.spec);
    setNodeColumnText(node, ColumnRemark, group.remark);
    writeGroupFaceAttributes(it->second);
    return Tree::EditControlOptionAccept;
}

void HoleAttribute::loadSpecRules()
{
    specRulesLoaded = true;
    specRules.clear();
    specRuleTypes.clear();

    KonBiaoZu::ExcelRuleManager* manager = static_cast<KonBiaoZu::ExcelRuleManager*>(ruleManager);
    if (manager != NULL)
    {
        std::string errorMessage;
        if (!manager->LoadRules(errorMessage) && !errorMessage.empty())
        {
            showError(errorMessage);
            return;
        }
        ruleFileName = manager->WorkbookPath();

        const std::vector<KonBiaoZu::RuleRecord>& rules = manager->Rules();
        for (size_t i = 0; i < rules.size(); ++i)
        {
            JsonRuleRecord record;
            record.annotationType = rules[i].annotationType;
            record.series = rules[i].series;
            record.size = rules[i].size;
            record.threadSpec = rules[i].threadSpec;
            record.lengthText = rules[i].lengthText;
            record.bottomHole = rules[i].bottomHole;
            record.displayText = rules[i].displayText;
            record.standardComment = rules[i].standardComment;
            record.note = rules[i].note;
            mergeJsonRule(specRules, specRuleTypes, record);
        }
        return;
    }

    if (ruleFileName.empty())
        return;

    std::vector<JsonRuleRecord> records;
    std::string error;
    if (!loadJsonRuleRecords(ruleFileName, records, error))
    {
        showError(error);
        return;
    }

    for (size_t i = 0; i < records.size(); ++i)
        mergeJsonRule(specRules, specRuleTypes, records[i]);
}
void HoleAttribute::ensureSpecRulesLoaded()
{
    if (!specRulesLoaded)
        loadSpecRules();
}

void HoleAttribute::configureSelectionBlock()
{
    if (selection0 == NULL)
        return;

    PropertyList* props = selection0->GetProperties();
    Selection::SelectionAction action = Selection::SelectionActionClearAndEnableSpecific;
    std::vector<Selection::MaskTriple> masks(1);
    masks[0] = Selection::MaskTriple(UF_solid_type, 0, UF_UI_SEL_FEATURE_CYLINDRICAL_FACE);
    props->SetSelectionFilter("SelectionFilter", action, masks);
    try
    {
        props->SetEnum("SelectMode", 1);
    }
    catch (...)
    {
    }
    delete props;
}

void HoleAttribute::localizeDialogBlocks()
{
    trySetStringProperty(theDialog->TopBlock(), "Label", "孔属性");
    trySetStringProperty(theDialog->TopBlock(), "LabelString", "孔属性");
    trySetStringProperty(theDialog->TopBlock(), "Title", "孔属性");

    trySetStringProperty(selection0, "LabelString", "选择圆柱面");
    trySetStringProperty(selection0, "Label", "选择圆柱面");
    trySetStringProperty(selection0, "Cue", "框选或点选圆柱面");
    trySetStringProperty(addCrossSelectionNodeButton, "Label", "刷新列表");
    trySetStringProperty(addCrossSelectionNodeButton, "LabelString", "刷新列表");
    trySetStringProperty(openRuleTableButton, "Label", "打开规则表");
    trySetStringProperty(openRuleTableButton, "LabelString", "打开规则表");
    trySetStringProperty(autoTapHoleToggle, "Label", "自动识别攻牙孔");
    trySetStringProperty(autoTapHoleToggle, "LabelString", "自动识别攻牙孔");
    trySetStringProperty(autoPemHoleToggle, "Label", "自动识别压铆孔");
    trySetStringProperty(autoPemHoleToggle, "LabelString", "自动识别压铆孔");
    trySetStringProperty(autoCounterboreHoleToggle, "Label", "自动识别沉孔");
    trySetStringProperty(autoCounterboreHoleToggle, "LabelString", "自动识别沉孔");

    UIBlock* nodeDataGroup = dynamic_cast<UIBlock*>(theDialog->TopBlock()->FindBlock("nodeDataGroup"));
    trySetVisible(nodeDataGroup, true);
    trySetStringProperty(nodeDataGroup, "Label", "选择对象");
    trySetStringProperty(nodeDataGroup, "LabelString", "选择对象");

    UIBlock* holeListGroup = dynamic_cast<UIBlock*>(theDialog->TopBlock()->FindBlock("group0"));
    trySetVisible(holeListGroup, true);
    trySetStringProperty(holeListGroup, "Label", "孔属性列表");
    trySetStringProperty(holeListGroup, "LabelString", "孔属性列表");

    UIBlock* addDeleteNodeGroup = dynamic_cast<UIBlock*>(theDialog->TopBlock()->FindBlock("addDeleteNodeGroup"));
    trySetVisible(addDeleteNodeGroup, true);
    trySetStringProperty(addDeleteNodeGroup, "Label", "规则表");
    trySetStringProperty(addDeleteNodeGroup, "LabelString", "规则表");
    trySetVisible(openRuleTableButton, true);
    UIBlock* obsoleteReloadRuleTableButton = dynamic_cast<UIBlock*>(theDialog->TopBlock()->FindBlock("deleteNodeButton"));
    trySetVisible(obsoleteReloadRuleTableButton, false);
    trySetVisible(autoTapHoleToggle, true);
    trySetVisible(autoPemHoleToggle, true);
    trySetVisible(autoCounterboreHoleToggle, true);

    const char* hideBlockIds[] = {
        "stateIconGroup",
        "NodeEditGroup",
        "menuGroup",
        "dragDropGroup",
        "defaultActionGroup",
        "redrawGroup",
        "listingWindowGroup",
        "instructions",
        "nodeString",
        "stateIconOptions",
        "nodeToolTip",
        "nodeEditOptions",
        "showMenuToggle",
        "disallowDragToggle",
        "dropOptions",
        "defaultActionToggle",
        "redrawInstruction",
        "redrawToggle",
        "listingWindowToggle"
    };

    for (size_t i = 0; i < sizeof(hideBlockIds) / sizeof(hideBlockIds[0]); ++i)
    {
        UIBlock* block = dynamic_cast<UIBlock*>(theDialog->TopBlock()->FindBlock(hideBlockIds[i]));
        trySetVisible(block, false);
    }
}

void HoleAttribute::refreshAutoRecognitionToggles()
{
    autoTapHoleEnabled = tryGetLogicalProperty(autoTapHoleToggle, "Value", autoTapHoleEnabled);
    autoPemHoleEnabled = tryGetLogicalProperty(autoPemHoleToggle, "Value", autoPemHoleEnabled);
    autoCounterboreHoleEnabled = tryGetLogicalProperty(autoCounterboreHoleToggle, "Value", autoCounterboreHoleEnabled);
}

void HoleAttribute::applyAutoRecognition(HoleGroup& group)
{
    ensureSpecRulesLoaded();
    const SpecRule* rule = autoRuleFor(group.diameter, group.height);
    if (rule == NULL)
    {
        group.type = defaultType();
        group.spec.clear();
        group.remark.clear();
        return;
    }

    group.type = rule->type;
    group.spec = rule->spec;
    if (!rule->levelSpecs.empty())
    {
        int levelIndex = thicknessLevelIndex(group.height);
        if (levelIndex >= 0 &&
            levelIndex < (int)rule->levelSpecs.size() &&
            !rule->levelSpecs[(size_t)levelIndex].empty())
        {
            group.spec = rule->levelSpecs[(size_t)levelIndex];
        }
    }
    group.remark = rule->remark;
}

void HoleAttribute::applyAutoRecognitionToAll()
{
    for (size_t i = 0; i < groups.size(); ++i)
        applyAutoRecognition(groups[i]);
}

void HoleAttribute::applyCounterboreRecognitionToAll()
{
    if (!autoCounterboreHoleEnabled)
        return;

    std::map<tag_t, double> thicknessByBody;

    for (size_t i = 0; i < groups.size(); ++i)
    {
        HoleGroup& group = groups[i];
        bool isCounterbore = false;
        for (size_t faceIndex = 0; faceIndex < group.faces.size() && !isCounterbore; ++faceIndex)
        {
            Face* face = group.faces[faceIndex];
            if (face == NULL)
                continue;

            tag_t bodyTag = NULL_TAG;
            if (UF_MODL_ask_face_body(face->Tag(), &bodyTag) != 0 || bodyTag == NULL_TAG)
                continue;

            double sheetThickness = 0.0;
            std::map<tag_t, double>::const_iterator found = thicknessByBody.find(bodyTag);
            if (found != thicknessByBody.end())
            {
                sheetThickness = found->second;
            }
            else
            {
                sheetThickness = sheetBodyThicknessForFace(face->Tag());
                thicknessByBody[bodyTag] = sheetThickness;
            }

            if (sheetThickness > DiameterTolerance &&
                group.height > DiameterTolerance &&
                sheetThickness - group.height >= CounterboreHeightDifference - DiameterTolerance)
            {
                isCounterbore = true;
            }
        }

        if (isCounterbore)
        {
            group.type = "娌夊瓟";
            group.spec = specFor(group.type, group.diameter, group.height);
            group.remark = remarkFor(group.type, group.diameter, group.height);
        }
        else if (group.type == "娌夊瓟")
        {
            group.type = defaultType();
            group.spec.clear();
            group.remark.clear();
            if (autoTapHoleEnabled || autoPemHoleEnabled)
                applyAutoRecognition(group);
        }
    }
}

const HoleAttribute::SpecRule* HoleAttribute::autoRuleFor(double diameter, double height) const
{
    if (!autoTapHoleEnabled && !autoPemHoleEnabled)
        return NULL;

    const bool preferTapFirst = autoTapHoleEnabled;
    for (int pass = 0; pass < 2; ++pass)
    {
        bool wantTap = preferTapFirst ? (pass == 0) : false;
        bool wantPem = preferTapFirst ? (pass == 1) : (pass == 0);
        if ((wantTap && !autoTapHoleEnabled) || (wantPem && !autoPemHoleEnabled))
            continue;

        for (size_t i = 0; i < specRules.size(); ++i)
        {
            const SpecRule& rule = specRules[i];
            if (rule.type == "孔")
                continue;
            if (wantTap && !isTapHoleRule(rule))
                continue;
            if (wantPem && !isPemHoleRule(rule))
                continue;
            if (rule.hasHeightRange &&
                (height < rule.minHeight - DiameterTolerance ||
                 height > rule.maxHeight + DiameterTolerance))
                continue;
            if (rule.isRange)
            {
                if (diameter > rule.minDiameter + DiameterTolerance && diameter < rule.maxDiameter - DiameterTolerance)
                    return &rule;
            }
            else if (std::fabs(rule.diameter - diameter) <= DiameterTolerance)
            {
                return &rule;
            }
        }
    }

    return NULL;
}

void HoleAttribute::rebuildFromSelection()
{
    if (selection0 == NULL)
        return;

    std::vector<Face*> previousHighlightedFaces = highlightedFaces;
    groups.clear();

    PropertyList* props = selection0->GetProperties();
    std::vector<TaggedObject*> selectedObjects = props->GetTaggedObjectVector("SelectedObjects");
    delete props;

    std::vector<TaggedObject*> acceptedObjects;
    for (size_t i = 0; i < selectedObjects.size(); ++i)
    {
        Face* face = NULL;
        double diameter = 0.0;
        double height = 0.0;
        if (!askCylindricalFace(selectedObjects[i], &face, &diameter, &height) || face == NULL)
            continue;

        acceptedObjects.push_back(face);

        int index = findGroupForDiameter(diameter, height);
        if (index < 0)
        {
            HoleGroup group;
            group.diameter = diameter;
            group.height = height;
            group.type = defaultType();
            group.spec.clear();
            group.remark.clear();
            if (autoTapHoleEnabled || autoPemHoleEnabled)
                applyAutoRecognition(group);
            group.node = NULL;
            groups.push_back(group);
            index = (int)groups.size() - 1;
        }
        groups[(size_t)index].faces.push_back(face);
    }

    bool needsSelectionRewrite = acceptedObjects.size() != selectedObjects.size();
    if (!needsSelectionRewrite)
    {
        for (size_t i = 0; i < selectedObjects.size(); ++i)
        {
            if (selectedObjects[i] != acceptedObjects[i])
            {
                needsSelectionRewrite = true;
                break;
            }
        }
    }

    if (needsSelectionRewrite)
    {
        props = selection0->GetProperties();
        props->SetTaggedObjectVector("SelectedObjects", acceptedObjects);
        delete props;
    }

    highlightedFaces.clear();
    for (size_t i = 0; i < previousHighlightedFaces.size(); ++i)
    {
        bool stillSelected = false;
        for (size_t j = 0; j < acceptedObjects.size(); ++j)
        {
            if (previousHighlightedFaces[i] == acceptedObjects[j])
            {
                stillSelected = true;
                break;
            }
        }

        if (stillSelected)
            highlightedFaces.push_back(previousHighlightedFaces[i]);
        else if (previousHighlightedFaces[i] != NULL)
            previousHighlightedFaces[i]->Unhighlight();
    }

    std::sort(groups.begin(), groups.end(), [](const HoleGroup& a, const HoleGroup& b) {
        return a.diameter < b.diameter;
    });

    applyCounterboreRecognitionToAll();
    writeAllFaceAttributes();
    rebuildTree();
}

void HoleAttribute::rebuildTree()
{
    if (tree_control0 == NULL)
        return;

    for (std::map<Node*, size_t>::iterator it = nodeToGroup.begin(); it != nodeToGroup.end(); ++it)
    {
        try
        {
            tree_control0->DeleteNode(it->first);
        }
        catch (...)
        {
        }
    }

    nodeToGroup.clear();

    for (size_t i = 0; i < groups.size(); ++i)
    {
        Node* node = tree_control0->CreateNode(utf8NxString(formatDiameter(groups[i].diameter)));
        tree_control0->InsertNode(node, NULL, NULL, Tree::NodeInsertOptionAlwaysLast);
        setNodeColumnText(node, ColumnDiameter, formatDiameter(groups[i].diameter));
        setNodeColumnText(node, ColumnCount, formatCount(groups[i].faces.size()));
        setNodeColumnText(node, ColumnType, groups[i].type);
        setNodeColumnText(node, ColumnSpec, groups[i].spec);
        setNodeColumnText(node, ColumnRemark, groups[i].remark);
        groups[i].node = node;
        nodeToGroup[node] = i;
    }
}

bool HoleAttribute::askCylindricalFace(TaggedObject* object, Face** face, double* diameter, double* height)
{
    Face* candidate = dynamic_cast<Face*>(object);
    if (candidate == NULL)
        return false;

    int faceType = 0;
    double point[3] = { 0.0, 0.0, 0.0 };
    double direction[3] = { 0.0, 0.0, 0.0 };
    double box[6] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
    double radius = 0.0;
    double radiusData = 0.0;
    int normalDirection = 0;

    int code = UF_MODL_ask_face_data(candidate->Tag(), &faceType, point, direction, box, &radius, &radiusData, &normalDirection);
    if (code != 0 || faceType != 16 || radius <= 0.0)
        return false;

    if (!isWholeCylindricalFace(candidate->Tag()))
        return false;

    if (!isHoleCylindricalFace(candidate->Tag(), point, direction))
        return false;

    double faceHeight = 0.0;
    if (!askCylindricalFaceHeight(candidate->Tag(), &faceHeight))
        return false;

    if (!isSheetMetalBodyFace(candidate->Tag(), faceHeight))
        return false;

    *face = candidate;
    *diameter = radius * 2.0;
    *height = faceHeight;
    return true;
}

bool HoleAttribute::isWholeCylindricalFace(tag_t faceTag) const
{
    int topology = 0;
    if (UF_MODL_ask_face_topology(faceTag, &topology) != 0)
        return false;

    if (topology != UF_MODL_CYLINDRICAL_TOPOLOGY)
        return false;

    int uStatus = UF_MODL_NON_PERIODIC;
    int vStatus = UF_MODL_NON_PERIODIC;
    double uPeriod = 0.0;
    double vPeriod = 0.0;
    if (UF_MODL_ask_face_periodicity(faceTag, &uStatus, &uPeriod, &vStatus, &vPeriod) != 0)
        return false;

    return (uStatus == UF_MODL_PERIODIC && uPeriod > 0.0) ||
           (vStatus == UF_MODL_PERIODIC && vPeriod > 0.0);
}

bool HoleAttribute::isHoleCylindricalFace(tag_t faceTag, const double axisPoint[3], const double axisDirection[3]) const
{
    double uv[4] = { 0.0, 0.0, 0.0, 0.0 };
    if (UF_MODL_ask_face_uv_minmax(faceTag, uv) != 0)
        return false;

    double axis[3] = { axisDirection[0], axisDirection[1], axisDirection[2] };
    double axisLength = std::sqrt(dot3(axis, axis));
    if (axisLength <= 1.0e-9)
        return false;

    axis[0] /= axisLength;
    axis[1] /= axisLength;
    axis[2] /= axisLength;

    double param[2] = {
        (uv[0] + uv[1]) * 0.5,
        (uv[2] + uv[3]) * 0.5
    };
    double samplePoint[3] = { 0.0, 0.0, 0.0 };
    double u1[3] = { 0.0, 0.0, 0.0 };
    double v1[3] = { 0.0, 0.0, 0.0 };
    double u2[3] = { 0.0, 0.0, 0.0 };
    double v2[3] = { 0.0, 0.0, 0.0 };
    double normal[3] = { 0.0, 0.0, 0.0 };
    double radii[2] = { 0.0, 0.0 };
    if (UF_MODL_ask_face_props(faceTag, param, samplePoint, u1, v1, u2, v2, normal, radii) != 0)
        return false;

    double fromAxis[3] = {
        samplePoint[0] - axisPoint[0],
        samplePoint[1] - axisPoint[1],
        samplePoint[2] - axisPoint[2]
    };
    double projection = dot3(fromAxis, axis);
    double radial[3] = {
        fromAxis[0] - projection * axis[0],
        fromAxis[1] - projection * axis[1],
        fromAxis[2] - projection * axis[2]
    };
    double radialLength = std::sqrt(dot3(radial, radial));
    if (radialLength <= 1.0e-9)
        return false;

    radial[0] /= radialLength;
    radial[1] /= radialLength;
    radial[2] /= radialLength;

    return dot3(normal, radial) < -0.1;
}

bool HoleAttribute::isSheetMetalBodyFace(tag_t faceTag, double holeHeight) const
{
    TaggedObject* taggedFace = NXObjectManager::Get(faceTag);
    Face* face = dynamic_cast<Face*>(taggedFace);
    Body* body = face == NULL ? NULL : face->GetBody();
    Part* workPart = theSession == NULL ? NULL : theSession->Parts()->Work();
    Features::SheetMetal::SheetmetalManager* sheetmetalManager =
        workPart == NULL || workPart->Features() == NULL ? NULL : workPart->Features()->SheetmetalManager();

    if (body != NULL && sheetmetalManager != NULL)
    {
        try
        {
            if (sheetmetalManager->IsSheetmetalBody(body))
                return true;
        }
        catch (const NXException&)
        {
        }
        catch (...)
        {
        }
    }

    tag_t bodyTag = body == NULL ? NULL_TAG : body->Tag();
    if (bodyTag == NULL_TAG && (UF_MODL_ask_face_body(faceTag, &bodyTag) != 0 || bodyTag == NULL_TAG))
        return false;

    uf_list_p_t formableFeatures = NULL;
    if (UF_SMD_ask_formable_feats(bodyTag, &formableFeatures) == 0 && formableFeatures != NULL)
    {
        int formableCount = 0;
        UF_MODL_ask_list_count(formableFeatures, &formableCount);
        UF_MODL_delete_list(&formableFeatures);
        if (formableCount > 0)
            return true;
    }

    uf_list_p_t bodyFeatures = NULL;
    if (UF_MODL_ask_body_feats(bodyTag, &bodyFeatures) != 0 || bodyFeatures == NULL)
        return false;

    int featureCount = 0;
    bool isSheetMetal = false;
    if (UF_MODL_ask_list_count(bodyFeatures, &featureCount) == 0)
    {
        for (int i = 0; i < featureCount && !isSheetMetal; ++i)
        {
            tag_t featureTag = NULL_TAG;
            if (UF_MODL_ask_list_item(bodyFeatures, i, &featureTag) != 0 || featureTag == NULL_TAG)
                continue;

            char* featureType = NULL;
            if (UF_MODL_ask_feat_type(featureTag, &featureType) == 0 && featureType != NULL)
            {
                isSheetMetal = isSheetMetalFeatureType(featureType);
                UF_free(featureType);
            }
        }
    }

    UF_MODL_delete_list(&bodyFeatures);
    if (isSheetMetal)
        return true;

    const double sheetThickness = sheetBodyThicknessForFace(faceTag);
    if (sheetThickness <= DiameterTolerance || holeHeight <= DiameterTolerance)
        return false;

    double tolerance = holeHeight * 0.08;
    if (tolerance < 0.25)
        tolerance = 0.25;
    const bool heightMatchesThickness = std::fabs(sheetThickness - holeHeight) <= tolerance;
    const bool isShortCounterboreFace = sheetThickness - holeHeight >= CounterboreHeightDifference - DiameterTolerance;
    return heightMatchesThickness || isShortCounterboreFace;
}

double HoleAttribute::sheetBodyThicknessForFace(tag_t faceTag) const
{
    TaggedObject* taggedFace = NXObjectManager::Get(faceTag);
    Face* face = dynamic_cast<Face*>(taggedFace);
    Body* body = face == NULL ? NULL : face->GetBody();
    tag_t bodyTag = body == NULL ? NULL_TAG : body->Tag();
    if (bodyTag == NULL_TAG && (UF_MODL_ask_face_body(faceTag, &bodyTag) != 0 || bodyTag == NULL_TAG))
        return 0.0;

    uf_list_p_t faceList = NULL;
    if (UF_MODL_ask_body_faces(bodyTag, &faceList) != 0 || faceList == NULL)
        return 0.0;

    std::vector<PlanarFaceMeasure> planarFaces;
    int faceCount = 0;
    if (UF_MODL_ask_list_count(faceList, &faceCount) == 0)
    {
        for (int i = 0; i < faceCount; ++i)
        {
            tag_t candidateTag = NULL_TAG;
            if (UF_MODL_ask_list_item(faceList, i, &candidateTag) != 0 || candidateTag == NULL_TAG)
                continue;

            PlanarFaceMeasure measure;
            if (askPlanarFaceMeasure(candidateTag, &measure))
                planarFaces.push_back(measure);
        }
    }
    UF_MODL_delete_list(&faceList);

    if (planarFaces.size() < 2)
        return 0.0;

    size_t largestIndex = 0;
    for (size_t i = 1; i < planarFaces.size(); ++i)
    {
        if (planarFaces[i].area > planarFaces[largestIndex].area)
            largestIndex = i;
    }

    const PlanarFaceMeasure& largest = planarFaces[largestIndex];
    double minDistance = std::numeric_limits<double>::max();
    for (size_t i = 0; i < planarFaces.size(); ++i)
    {
        if (i == largestIndex)
            continue;

        const PlanarFaceMeasure& candidate = planarFaces[i];
        if (candidate.area <= largest.area * 0.5)
            continue;

        if (std::fabs(dot3(largest.normal, candidate.normal)) < 0.95)
            continue;

        double offset[3] = {
            candidate.point[0] - largest.point[0],
            candidate.point[1] - largest.point[1],
            candidate.point[2] - largest.point[2]
        };
        double distance = std::fabs(dot3(offset, largest.normal));
        if (distance <= DiameterTolerance)
            continue;

        minDistance = std::min(minDistance, distance);
    }

    if (minDistance == std::numeric_limits<double>::max())
        return 0.0;

    return minDistance;
}

bool HoleAttribute::askCylindricalFaceHeight(tag_t faceTag, double* height) const
{
    double uv[4] = { 0.0, 0.0, 0.0, 0.0 };
    if (UF_MODL_ask_face_uv_minmax(faceTag, uv) != 0)
        return false;

    int uStatus = UF_MODL_NON_PERIODIC;
    int vStatus = UF_MODL_NON_PERIODIC;
    double uPeriod = 0.0;
    double vPeriod = 0.0;
    if (UF_MODL_ask_face_periodicity(faceTag, &uStatus, &uPeriod, &vStatus, &vPeriod) != 0)
        return false;

    bool uIsAxis = (vStatus == UF_MODL_PERIODIC && uStatus != UF_MODL_PERIODIC);
    bool vIsAxis = (uStatus == UF_MODL_PERIODIC && vStatus != UF_MODL_PERIODIC);
    if (!uIsAxis && !vIsAxis)
        vIsAxis = (uStatus == UF_MODL_PERIODIC);

    double paramA[2] = { (uv[0] + uv[1]) * 0.5, (uv[2] + uv[3]) * 0.5 };
    double paramB[2] = { paramA[0], paramA[1] };
    if (uIsAxis)
    {
        paramA[0] = uv[0];
        paramB[0] = uv[1];
    }
    else
    {
        paramA[1] = uv[2];
        paramB[1] = uv[3];
    }

    double pointA[3] = { 0.0, 0.0, 0.0 };
    double pointB[3] = { 0.0, 0.0, 0.0 };
    double u1[3] = { 0.0, 0.0, 0.0 };
    double v1[3] = { 0.0, 0.0, 0.0 };
    double u2[3] = { 0.0, 0.0, 0.0 };
    double v2[3] = { 0.0, 0.0, 0.0 };
    double normal[3] = { 0.0, 0.0, 0.0 };
    double radii[2] = { 0.0, 0.0 };
    if (UF_MODL_ask_face_props(faceTag, paramA, pointA, u1, v1, u2, v2, normal, radii) != 0)
        return false;
    if (UF_MODL_ask_face_props(faceTag, paramB, pointB, u1, v1, u2, v2, normal, radii) != 0)
        return false;

    *height = distance3(pointA, pointB);
    return *height > DiameterTolerance;
}

int HoleAttribute::findGroupForDiameter(double diameter, double height) const
{
    for (size_t i = 0; i < groups.size(); ++i)
    {
        if (std::fabs(groups[i].diameter - diameter) <= DiameterTolerance &&
            std::fabs(groups[i].height - height) <= DiameterTolerance)
            return (int)i;
    }
    return -1;
}

void HoleAttribute::applyDiameterChange(size_t groupIndex, double newDiameter)
{
    if (groupIndex >= groups.size())
        return;

    HoleGroup& group = groups[groupIndex];
    if (std::fabs(group.diameter - newDiameter) <= DiameterTolerance)
        return;

    std::vector<tag_t> targetFaces;
    for (size_t i = 0; i < group.faces.size(); ++i)
        targetFaces.push_back(group.faces[i]->Tag());

    if (targetFaces.empty())
        return;

    std::ostringstream value;
    value << std::setprecision(16) << newDiameter;

    Session::UndoMarkId mark = theSession->SetUndoMark(Session::MarkVisibilityVisible, "修改圆柱面直径");
    tag_t featureTag = NULL_TAG;
    checkUf(UF_MODL_create_resize_face(&targetFaces[0], (int)targetFaces.size(), NULL, 0, const_cast<char*>(value.str().c_str()), &featureTag), "UF_MODL_create_resize_face");
    theSession->UpdateManager()->DoUpdate(mark);

    group.diameter = newDiameter;
    if (autoTapHoleEnabled || autoPemHoleEnabled)
    {
        applyAutoRecognition(group);
    }
    else
    {
        group.spec = specFor(group.type, group.diameter, group.height);
        group.remark = remarkFor(group.type, group.diameter, group.height);
    }
    applyCounterboreRecognitionToAll();
}

void HoleAttribute::writeGroupFaceAttributes(size_t groupIndex)
{
    if (groupIndex >= groups.size())
        return;

    HoleGroup& group = groups[groupIndex];
    for (size_t i = 0; i < group.faces.size(); ++i)
    {
        deleteFaceStringAttributeIfExists(group.faces[i], "绫诲瀷");
        deleteFaceStringAttributeIfExists(group.faces[i], "瑙勬牸");
        setFaceStringAttribute(group.faces[i], "类型", group.type);
        setFaceStringAttribute(group.faces[i], "规格", group.spec);
    }
}

void HoleAttribute::writeAllFaceAttributes()
{
    for (size_t i = 0; i < groups.size(); ++i)
        writeGroupFaceAttributes(i);
}

void HoleAttribute::highlightGroup(size_t groupIndex)
{
    clearHighlight(false);
    if (groupIndex >= groups.size())
    {
        refreshDisplay();
        return;
    }

    highlightedGroupIndex = (int)groupIndex;
    for (size_t i = 0; i < groups[groupIndex].faces.size(); ++i)
    {
        if (groups[groupIndex].faces[i] != NULL)
        {
            UF_DISP_set_highlight(groups[groupIndex].faces[i]->Tag(), 1);
            highlightedFaces.push_back(groups[groupIndex].faces[i]);
        }
    }
    refreshDisplay();
}

void HoleAttribute::clearHighlight(bool refresh)
{
    for (size_t groupIndex = 0; groupIndex < groups.size(); ++groupIndex)
    {
        for (size_t faceIndex = 0; faceIndex < groups[groupIndex].faces.size(); ++faceIndex)
        {
            if (groups[groupIndex].faces[faceIndex] != NULL)
                UF_DISP_set_highlight(groups[groupIndex].faces[faceIndex]->Tag(), 0);
        }
    }

    for (size_t i = 0; i < highlightedFaces.size(); ++i)
    {
        if (highlightedFaces[i] != NULL)
            UF_DISP_set_highlight(highlightedFaces[i]->Tag(), 0);
    }
    highlightedFaces.clear();
    highlightedGroupIndex = -1;
    if (refresh)
        refreshDisplay();
}

void HoleAttribute::writeLine(const std::string& text)
{
    ListingWindow* lw = theSession->ListingWindow();
    lw->Open();
    lw->WriteLine(text);
}

void HoleAttribute::showError(const std::string& text)
{
    if (theUI != NULL)
        theUI->NXMessageBox()->Show("孔属性", NXMessageBox::DialogTypeError, text);
}

std::string HoleAttribute::specFor(const std::string& type, double diameter, double height)
{
    ensureSpecRulesLoaded();

    if (type == "孔")
        return std::string();

    for (size_t i = 0; i < specRules.size(); ++i)
    {
        if (specRules[i].type != type)
            continue;
        if (specRules[i].hasHeightRange &&
            (height < specRules[i].minHeight - DiameterTolerance ||
             height > specRules[i].maxHeight + DiameterTolerance))
            continue;

        if (!specRules[i].levelSpecs.empty())
        {
            int levelIndex = thicknessLevelIndex(height);
            if (levelIndex >= 0 && levelIndex < (int)specRules[i].levelSpecs.size() && !specRules[i].levelSpecs[(size_t)levelIndex].empty())
            {
                if (specRules[i].isRange)
                {
                    if (diameter > specRules[i].minDiameter + DiameterTolerance && diameter < specRules[i].maxDiameter - DiameterTolerance)
                        return specRules[i].levelSpecs[(size_t)levelIndex];
                }
                else if (std::fabs(specRules[i].diameter - diameter) <= DiameterTolerance)
                {
                    return specRules[i].levelSpecs[(size_t)levelIndex];
                }
            }

            continue;
        }

        if (specRules[i].isRange)
        {
            if (diameter > specRules[i].minDiameter + DiameterTolerance && diameter < specRules[i].maxDiameter - DiameterTolerance)
                return specRules[i].spec;
        }
        else if (std::fabs(specRules[i].diameter - diameter) <= DiameterTolerance)
        {
            return specRules[i].spec;
        }
    }

    return std::string();
}

std::string HoleAttribute::remarkFor(const std::string& type, double diameter, double height)
{
    ensureSpecRulesLoaded();

    if (type == "孔")
        return std::string();

    for (size_t i = 0; i < specRules.size(); ++i)
    {
        if (specRules[i].type != type)
            continue;
        if (specRules[i].hasHeightRange &&
            (height < specRules[i].minHeight - DiameterTolerance ||
             height > specRules[i].maxHeight + DiameterTolerance))
            continue;

        if (specRules[i].isRange)
        {
            if (diameter > specRules[i].minDiameter + DiameterTolerance && diameter < specRules[i].maxDiameter - DiameterTolerance)
                return specRules[i].remark;
        }
        else if (std::fabs(specRules[i].diameter - diameter) <= DiameterTolerance)
        {
            return specRules[i].remark;
        }
    }

    return std::string();
}

std::vector<std::string> HoleAttribute::typeOptions()
{
    ensureSpecRulesLoaded();

    std::vector<std::string> values;
    addUniqueString(values, "孔");
    addUniqueString(values, "沉孔");
    for (size_t i = 0; i < specRuleTypes.size(); ++i)
        addUniqueString(values, specRuleTypes[i]);

    return values;
}

std::string HoleAttribute::defaultType()
{
    return "孔";
}

std::string HoleAttribute::formatDiameter(double value) const
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(3) << value;
    return stream.str();
}

std::string HoleAttribute::formatCount(size_t value) const
{
    std::ostringstream stream;
    stream << value;
    return stream.str();
}

std::string HoleAttribute::defaultSpec(const std::string& type, double diameter)
{
    std::ostringstream stream;
    if (type.empty() || type == "孔")
        stream << "直径" << std::fixed << std::setprecision(1) << diameter;
    else
        stream << type;

    return stream.str();
}
