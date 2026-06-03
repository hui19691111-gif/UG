#include "ExcelRuleManager.hpp"
#include "RulesEditorDialog.hpp"

#include <Windows.h>
#include <CommCtrl.h>
#include <commdlg.h>

#include <algorithm>
#include <cctype>
#include <cwctype>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <unordered_set>
#include <vector>

#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Comdlg32.lib")
#pragma comment(lib, "Gdi32.lib")

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace
{
    HINSTANCE DllInstance()
    {
        return reinterpret_cast<HINSTANCE>(&__ImageBase);
    }

    struct RuleTemplate
    {
        const char* annotationType;
        const char* series;
        const char* size;
        const char* threadSpec;
        const char* lengthText;
        double bottomHole;
        const char* displayText;
        const char* standardComment;
        const char* note;
    };

    const RuleTemplate kDefaultRules[] = {
        {"孔标注", "通孔", "D3", "", "", 3.00, "", "", "普通通孔"},
        {"螺牙标注", "攻牙", "M3", "M3x0.5", "", 2.50, "攻牙 {规格}", "标准公制螺纹底孔", "攻牙"},
        {"螺牙标注", "攻牙", "M4", "M4x0.7", "", 3.30, "攻牙 {规格}", "标准公制螺纹底孔", "攻牙"},
        {"压铆螺母标注", "S", "M3", "S-M3", "", 4.20, "S-{规格}", "压铆螺母", "压铆"},
        {"沉孔标注", "沉孔", "D5", "", "", 5.00, "沉孔 {规格}", "沉孔", "沉孔"},
    };
    std::string Trim(const std::string& text)
    {
        const std::string whitespace = " \t\r\n";
        const size_t begin = text.find_first_not_of(whitespace);
        if (begin == std::string::npos)
        {
            return {};
        }

        const size_t end = text.find_last_not_of(whitespace);
        return text.substr(begin, end - begin + 1);
    }

    bool HasRuleContent(const KonBiaoZu::RuleRecord& rule)
    {
        return !Trim(rule.series).empty() ||
               !Trim(rule.size).empty() ||
               !Trim(rule.threadSpec).empty() ||
               !Trim(rule.lengthText).empty() ||
               rule.bottomHole != 0.0 ||
               !Trim(rule.displayText).empty() ||
               !Trim(rule.standardComment).empty() ||
               !Trim(rule.note).empty();
    }

    std::filesystem::path GetModuleDirectory()
    {
        wchar_t buffer[MAX_PATH] = {};
        const HMODULE module = reinterpret_cast<HMODULE>(&__ImageBase);
        const DWORD length = GetModuleFileNameW(module, buffer, MAX_PATH);
        if (length == 0 || length >= MAX_PATH)
        {
            return {};
        }
        return std::filesystem::path(buffer).parent_path();
    }

    std::vector<std::filesystem::path> BuildSearchRoots()
    {
        const std::filesystem::path moduleDirectory = GetModuleDirectory();
        std::vector<std::filesystem::path> roots;
        std::unordered_set<std::wstring> seen;

        const auto pushRoot = [&roots, &seen](const std::filesystem::path& root) {
            if (root.empty())
            {
                return;
            }

            std::error_code error;
            const std::filesystem::path normalized = std::filesystem::weakly_canonical(root, error);
            const std::filesystem::path resolved = error ? root.lexically_normal() : normalized;
            const std::wstring key = resolved.wstring();
            if (seen.insert(key).second)
            {
                roots.push_back(resolved);
            }
        };

        pushRoot(moduleDirectory);
        if (!moduleDirectory.empty())
        {
            pushRoot(moduleDirectory.parent_path());
            pushRoot(moduleDirectory.parent_path().parent_path());
        }

        std::error_code currentPathError;
        pushRoot(std::filesystem::current_path(currentPathError));
        return roots;
    }

    std::filesystem::path FindExistingRelativePath(std::initializer_list<std::filesystem::path> candidates)
    {
        const std::vector<std::filesystem::path> roots = BuildSearchRoots();
        for (const std::filesystem::path& root : roots)
        {
            for (const std::filesystem::path& candidate : candidates)
            {
                const std::filesystem::path fullPath = root / candidate;
                if (std::filesystem::exists(fullPath))
                {
                    return fullPath;
                }
            }
        }
        return {};
    }

    std::filesystem::path GetDataDirectory()
    {
        std::filesystem::path dataDirectory = FindExistingRelativePath({"DATA"});
        if (!dataDirectory.empty())
        {
            return dataDirectory;
        }

        dataDirectory = FindExistingRelativePath({"data", "KonBiaoZu/data"});
        if (!dataDirectory.empty())
        {
            return dataDirectory;
        }

        const std::filesystem::path moduleDirectory = GetModuleDirectory();
        if (!moduleDirectory.empty())
        {
            return moduleDirectory.parent_path() / "DATA";
        }
        return std::filesystem::current_path() / "DATA";
    }

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

    std::wstring Utf8ToWide(const std::string& utf8)
    {
        if (utf8.empty())
        {
            return {};
        }

        const int wideSize = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
        if (wideSize <= 0)
        {
            return {};
        }

        std::wstring wide(static_cast<size_t>(wideSize), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, wide.data(), wideSize);
        if (!wide.empty() && wide.back() == L'\0')
        {
            wide.pop_back();
        }
        return wide;
    }

    std::string WideToUtf8(const std::wstring& wide)
    {
        if (wide.empty())
        {
            return {};
        }

        const int utf8Size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (utf8Size <= 0)
        {
            return {};
        }

        std::string utf8(static_cast<size_t>(utf8Size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, utf8.data(), utf8Size, nullptr, nullptr);
        if (!utf8.empty() && utf8.back() == '\0')
        {
            utf8.pop_back();
        }
        return utf8;
    }

    std::string FormatDouble(double value)
    {
        std::ostringstream out;
        out << std::fixed << std::setprecision(2) << value;
        std::string text = out.str();
        while (!text.empty() && text.back() == '0')
        {
            text.pop_back();
        }
        if (!text.empty() && text.back() == '.')
        {
            text.pop_back();
        }
        return text;
    }

    std::string FormatHole(double value)
    {
        std::ostringstream out;
        out << std::fixed << std::setprecision(2) << value;
        return out.str();
    }

    void ReplaceAll(std::string& text, const std::string& from, const std::string& to)
    {
        if (from.empty())
        {
            return;
        }

        size_t position = 0;
        while ((position = text.find(from, position)) != std::string::npos)
        {
            text.replace(position, from.size(), to);
            position += to.size();
        }
    }

    std::string ApplyTemplate(const std::string& templateText, const KonBiaoZu::RuleRecord& record)
    {
        std::string result = templateText;
        ReplaceAll(result, "{子类型}", record.series);
        ReplaceAll(result, "{规格}", record.size);
        ReplaceAll(result, "{螺纹}", record.threadSpec);
        ReplaceAll(result, "{底孔}", FormatHole(record.bottomHole));
        ReplaceAll(result, "{长度}", record.lengthText);
        return Trim(result);
    }

    bool RuleLess(const KonBiaoZu::RuleRecord& left, const KonBiaoZu::RuleRecord& right)
    {
        if (left.bottomHole != right.bottomHole)
        {
            return left.bottomHole < right.bottomHole;
        }
        if (left.series != right.series)
        {
            return left.series < right.series;
        }
        return left.size < right.size;
    }

    std::string EscapeJson(const std::string& value)
    {
        std::string escaped;
        for (const char ch : value)
        {
            switch (ch)
            {
            case '\\':
                escaped += "\\\\";
                break;
            case '"':
                escaped += "\\\"";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                escaped += "\\r";
                break;
            case '\t':
                escaped += "\\t";
                break;
            default:
                escaped.push_back(ch);
                break;
            }
        }
        return escaped;
    }

    std::string EscapeIni(const std::string& value)
    {
        std::string escaped;
        for (const char ch : value)
        {
            switch (ch)
            {
            case '\\':
                escaped += "\\\\";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                escaped += "\\r";
                break;
            default:
                escaped.push_back(ch);
                break;
            }
        }
        return escaped;
    }

    std::string UnescapeIni(const std::string& value)
    {
        std::string text;
        for (size_t index = 0; index < value.size(); ++index)
        {
            const char ch = value[index];
            if (ch != '\\' || index + 1 >= value.size())
            {
                text.push_back(ch);
                continue;
            }

            const char escaped = value[++index];
            switch (escaped)
            {
            case 'n':
                text.push_back('\n');
                break;
            case 'r':
                text.push_back('\r');
                break;
            case '\\':
                text.push_back('\\');
                break;
            default:
                text.push_back(escaped);
                break;
            }
        }
        return text;
    }

    bool SetIniRuleField(KonBiaoZu::RuleRecord& record, const std::string& key, const std::string& value)
    {
        if (key == "annotationType") record.annotationType = value;
        else if (key == "series") record.series = value;
        else if (key == "size") record.size = value;
        else if (key == "threadSpec") record.threadSpec = value;
        else if (key == "lengthText") record.lengthText = value;
        else if (key == "displayText") record.displayText = value;
        else if (key == "standardComment") record.standardComment = value;
        else if (key == "note") record.note = value;
        else if (key == "bottomHole")
        {
            const std::string trimmed = Trim(value);
            if (trimmed.empty())
            {
                record.bottomHole = 0.0;
                return true;
            }
            char* end = nullptr;
            const double parsed = std::strtod(trimmed.c_str(), &end);
            if (end == trimmed.c_str() || *end != '\0')
            {
                return false;
            }
            record.bottomHole = parsed;
        }
        return true;
    }

    void WriteIniRule(std::ostream& output, size_t index, const KonBiaoZu::RuleRecord& rule)
    {
        output << "[Rule" << std::setw(3) << std::setfill('0') << index << std::setfill(' ') << "]\n";
        output << "annotationType=" << EscapeIni(rule.annotationType) << "\n";
        output << "series=" << EscapeIni(rule.series) << "\n";
        output << "size=" << EscapeIni(rule.size) << "\n";
        output << "threadSpec=" << EscapeIni(rule.threadSpec) << "\n";
        output << "lengthText=" << EscapeIni(rule.lengthText) << "\n";
        output << "bottomHole=" << std::fixed << std::setprecision(2) << rule.bottomHole << "\n";
        output << "displayText=" << EscapeIni(rule.displayText) << "\n";
        output << "standardComment=" << EscapeIni(rule.standardComment) << "\n";
        output << "note=" << EscapeIni(rule.note) << "\n\n";
    }

    void SkipWhitespace(const std::string& text, size_t& position)
    {
        while (position < text.size() && std::isspace(static_cast<unsigned char>(text[position])))
        {
            ++position;
        }
    }

    bool ParseJsonString(const std::string& text, size_t& position, std::string& value)
    {
        SkipWhitespace(text, position);
        if (position >= text.size() || text[position] != '"')
        {
            return false;
        }
        ++position;

        value.clear();
        while (position < text.size())
        {
            const char ch = text[position++];
            if (ch == '"')
            {
                return true;
            }

            if (ch != '\\')
            {
                value.push_back(ch);
                continue;
            }

            if (position >= text.size())
            {
                return false;
            }

            const char escaped = text[position++];
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

    bool ParseJsonNumber(const std::string& text, size_t& position, double& value)
    {
        SkipWhitespace(text, position);
        const size_t begin = position;
        if (position < text.size() && (text[position] == '-' || text[position] == '+'))
        {
            ++position;
        }
        while (position < text.size() && std::isdigit(static_cast<unsigned char>(text[position])))
        {
            ++position;
        }
        if (position < text.size() && text[position] == '.')
        {
            ++position;
            while (position < text.size() && std::isdigit(static_cast<unsigned char>(text[position])))
            {
                ++position;
            }
        }

        if (begin == position)
        {
            return false;
        }

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

    bool SkipJsonValue(const std::string& text, size_t& position);

    bool SkipJsonObjectOrArray(const std::string& text, size_t& position, char open, char close)
    {
        SkipWhitespace(text, position);
        if (position >= text.size() || text[position] != open)
        {
            return false;
        }
        ++position;

        while (position < text.size())
        {
            SkipWhitespace(text, position);
            if (position < text.size() && text[position] == close)
            {
                ++position;
                return true;
            }

            if (!SkipJsonValue(text, position))
            {
                return false;
            }

            SkipWhitespace(text, position);
            if (position < text.size() && text[position] == ',')
            {
                ++position;
            }
        }

        return false;
    }

    bool SkipJsonValue(const std::string& text, size_t& position)
    {
        SkipWhitespace(text, position);
        if (position >= text.size())
        {
            return false;
        }

        if (text[position] == '"')
        {
            std::string ignored;
            return ParseJsonString(text, position, ignored);
        }
        if (text[position] == '{')
        {
            return SkipJsonObjectOrArray(text, position, '{', '}');
        }
        if (text[position] == '[')
        {
            return SkipJsonObjectOrArray(text, position, '[', ']');
        }
        if (std::isdigit(static_cast<unsigned char>(text[position])) || text[position] == '-' || text[position] == '+')
        {
            double ignored = 0.0;
            return ParseJsonNumber(text, position, ignored);
        }

        const char* literals[] = {"true", "false", "null"};
        for (const char* literal : literals)
        {
            const size_t length = std::strlen(literal);
            if (text.compare(position, length, literal) == 0)
            {
                position += length;
                return true;
            }
        }
        return false;
    }

    bool ParseRuleObject(const std::string& text, size_t& position, KonBiaoZu::RuleRecord& record)
    {
        SkipWhitespace(text, position);
        if (position >= text.size() || text[position] != '{')
        {
            return false;
        }
        ++position;

        while (position < text.size())
        {
            SkipWhitespace(text, position);
            if (position < text.size() && text[position] == '}')
            {
                ++position;
                return !record.annotationType.empty();
            }

            std::string key;
            if (!ParseJsonString(text, position, key))
            {
                return false;
            }
            SkipWhitespace(text, position);
            if (position >= text.size() || text[position] != ':')
            {
                return false;
            }
            ++position;

            if (key == "bottomHole")
            {
                if (!ParseJsonNumber(text, position, record.bottomHole))
                {
                    return false;
                }
            }
            else if (key == "annotationType" ||
                     key == "series" ||
                     key == "size" ||
                     key == "threadSpec" ||
                     key == "lengthText" ||
                     key == "displayText" ||
                     key == "standardComment" ||
                     key == "note")
            {
                std::string value;
                if (!ParseJsonString(text, position, value))
                {
                    return false;
                }

                if (key == "annotationType") record.annotationType = value;
                else if (key == "series") record.series = value;
                else if (key == "size") record.size = value;
                else if (key == "threadSpec") record.threadSpec = value;
                else if (key == "lengthText") record.lengthText = value;
                else if (key == "displayText") record.displayText = value;
                else if (key == "standardComment") record.standardComment = value;
                else if (key == "note") record.note = value;
            }
            else if (!SkipJsonValue(text, position))
            {
                return false;
            }

            SkipWhitespace(text, position);
            if (position < text.size() && text[position] == ',')
            {
                ++position;
            }
        }

        return false;
    }

    std::vector<KonBiaoZu::RuleRecord> BuildDefaultRules()
    {
        std::vector<KonBiaoZu::RuleRecord> rules;
        for (const RuleTemplate& ruleTemplate : kDefaultRules)
        {
            KonBiaoZu::RuleRecord record;
            record.annotationType = ruleTemplate.annotationType;
            record.series = ruleTemplate.series;
            record.size = ruleTemplate.size;
            record.threadSpec = ruleTemplate.threadSpec;
            record.lengthText = ruleTemplate.lengthText;
            record.bottomHole = ruleTemplate.bottomHole;
            record.displayText = ruleTemplate.displayText;
            record.standardComment = ruleTemplate.standardComment;
            record.note = ruleTemplate.note;
            rules.push_back(record);
        }
        return rules;
    }

    bool LoadRawRulesFromConfig(
        const std::string& configPath,
        std::vector<KonBiaoZu::RuleRecord>& loadedRules,
        std::string& errorMessage)
    {
        loadedRules.clear();

        std::ifstream input(configPath, std::ios::binary);
        if (!input.is_open())
        {
            errorMessage = Utf8ToSystem("无法打开规则配置文件: " + configPath);
            return false;
        }

        std::ostringstream buffer;
        buffer << input.rdbuf();
        std::string text = buffer.str();
        if (text.size() >= 3 &&
            static_cast<unsigned char>(text[0]) == 0xEF &&
            static_cast<unsigned char>(text[1]) == 0xBB &&
            static_cast<unsigned char>(text[2]) == 0xBF)
        {
            text.erase(0, 3);
        }

        const std::string trimmedText = Trim(text);
        std::wstring extension = std::filesystem::path(configPath).extension().wstring();
        std::transform(extension.begin(), extension.end(), extension.begin(), [](wchar_t ch) {
            return static_cast<wchar_t>(std::towlower(ch));
        });
        if (extension == L".ini")
        {
            std::istringstream lines(text);
            std::string line;
            KonBiaoZu::RuleRecord record;
            bool inRule = false;

            const auto flushRecord = [&loadedRules, &record, &inRule]() {
                if (inRule && HasRuleContent(record))
                {
                    loadedRules.push_back(record);
                }
                record = KonBiaoZu::RuleRecord();
            };

            while (std::getline(lines, line))
            {
                if (!line.empty() && line.back() == '\r')
                {
                    line.pop_back();
                }

                const std::string trimmedLine = Trim(line);
                if (trimmedLine.empty() || trimmedLine[0] == ';' || trimmedLine[0] == '#')
                {
                    continue;
                }

                if (trimmedLine.front() == '[' && trimmedLine.back() == ']')
                {
                    flushRecord();
                    inRule = true;
                    continue;
                }

                if (!inRule)
                {
                    continue;
                }

                const size_t separator = trimmedLine.find('=');
                if (separator == std::string::npos)
                {
                    errorMessage = Utf8ToSystem("瑙勫垯閰嶇疆鏂囦欢 INI 鏍煎紡閿欒: " + configPath);
                    return false;
                }

                const std::string key = Trim(trimmedLine.substr(0, separator));
                const std::string value = UnescapeIni(Trim(trimmedLine.substr(separator + 1)));
                if (!SetIniRuleField(record, key, value))
                {
                    errorMessage = Utf8ToSystem("瑙勫垯閰嶇疆鏂囦欢 INI 瀛楁閿欒: " + configPath);
                    return false;
                }
            }

            flushRecord();
            return true;
        }

        std::string json = text;
        if (json.size() >= 3 &&
            static_cast<unsigned char>(json[0]) == 0xEF &&
            static_cast<unsigned char>(json[1]) == 0xBB &&
            static_cast<unsigned char>(json[2]) == 0xBF)
        {
            json.erase(0, 3);
        }

        const size_t rulesKey = json.find("\"rules\"");
        size_t position = rulesKey == std::string::npos ? std::string::npos : json.find('[', rulesKey);
        if (position == std::string::npos)
        {
            errorMessage = Utf8ToSystem("瑙勫垯閰嶇疆鏂囦欢缂哄皯 rules 鏁扮粍: " + configPath);
            return false;
        }
        ++position;

        while (position < json.size())
        {
            SkipWhitespace(json, position);
            if (position < json.size() && json[position] == ']')
            {
                break;
            }

            KonBiaoZu::RuleRecord record;
            if (!ParseRuleObject(json, position, record))
            {
                errorMessage = Utf8ToSystem("瑙勫垯閰嶇疆鏂囦欢鏍煎紡閿欒: " + configPath);
                return false;
            }
            if (HasRuleContent(record))
            {
                loadedRules.push_back(record);
            }

            SkipWhitespace(json, position);
            if (position < json.size() && json[position] == ',')
            {
                ++position;
            }
        }

        return true;
    }

    void ApplyTemplates(std::vector<KonBiaoZu::RuleRecord>& rules)
    {
        for (KonBiaoZu::RuleRecord& record : rules)
        {
            record.displayText = ApplyTemplate(record.displayText, record);
            record.standardComment = ApplyTemplate(record.standardComment, record);
        }
    }

    void RebuildRuleIndex(
        std::vector<KonBiaoZu::RuleRecord>& rules,
        std::unordered_map<std::string, std::vector<KonBiaoZu::RuleRecord>>& rulesByType)
    {
        rulesByType.clear();
        for (const KonBiaoZu::RuleRecord& rule : rules)
        {
            rulesByType[rule.annotationType].push_back(rule);
        }
    }

    constexpr int kIdTypeTabs = 1001;
    constexpr int kIdRuleList = 1002;
    constexpr int kIdAdd = 1003;
    constexpr int kIdDelete = 1004;
    constexpr int kIdSave = 1005;
    constexpr int kIdClose = 1006;
    constexpr int kIdImport = 1007;
    constexpr int kIdExport = 1008;
    constexpr int kIdFirstEdit = 1100;

    const char* kDefaultRuleTypes[] = {
        "孔标注",
        "螺牙标注",
        "焊接螺母标注",
        "沉孔标注",
        "压铆螺母标注",
        "压铆螺钉标注",
        "压铆螺母柱标注"
    };

    struct RulesEditorContext
    {
        std::string configPath;
        std::vector<KonBiaoZu::RuleRecord> rules;
        std::vector<KonBiaoZu::RuleRecord>* liveRules {};
        std::unordered_map<std::string, std::vector<KonBiaoZu::RuleRecord>>* liveRulesByType {};
        void** ownerWindowSlot {};
        HWND window {};
        HWND tabs {};
        HWND list {};
        HWND edits[9] {};
        HFONT uiFont {};
        std::vector<std::string> ruleTypes;
        int selectedRule {-1};
        int sortColumn {-1};
        bool sortAscending {true};
        bool dirty {};
        bool loadingSelection {};
    };

    std::wstring RuleFieldText(const KonBiaoZu::RuleRecord& rule, int field)
    {
        switch (field)
        {
        case 0: return Utf8ToWide(rule.annotationType);
        case 1: return Utf8ToWide(rule.series);
        case 2: return Utf8ToWide(rule.size);
        case 3: return Utf8ToWide(rule.threadSpec);
        case 4: return Utf8ToWide(rule.lengthText);
        case 5: return Utf8ToWide(FormatDouble(rule.bottomHole));
        case 6: return Utf8ToWide(rule.displayText);
        case 7: return Utf8ToWide(rule.standardComment);
        case 8: return Utf8ToWide(rule.note);
        default: return {};
        }
    }

    const int kEditorFields[] = {0, 2, 4, 5, 6, 7};
    const int kListFields[] = {2, 4, 5, 6, 7};
    const wchar_t* kEditorFieldLabels[] = {L"类别", L"规格", L"标准长度", L"底孔", L"显示标注", L"说明"};

    int FieldForListColumn(int column)
    {
        if (column <= 0)
        {
            return 0;
        }
        if (column > static_cast<int>(std::size(kListFields)))
        {
            return 0;
        }
        return kListFields[column - 1];
    }

    bool TryParseNumber(const std::string& text, double& value)
    {
        try
        {
            size_t parsed = 0;
            value = std::stod(text, &parsed);
            while (parsed < text.size() && std::isspace(static_cast<unsigned char>(text[parsed])) != 0)
            {
                ++parsed;
            }
            return parsed == text.size();
        }
        catch (...)
        {
            return false;
        }
    }

    int CompareRuleField(const KonBiaoZu::RuleRecord& left, const KonBiaoZu::RuleRecord& right, int field)
    {
        if (field == 5)
        {
            if (left.bottomHole < right.bottomHole)
            {
                return -1;
            }
            if (left.bottomHole > right.bottomHole)
            {
                return 1;
            }
            return 0;
        }

        const std::string leftText = WideToUtf8(RuleFieldText(left, field));
        const std::string rightText = WideToUtf8(RuleFieldText(right, field));
        double leftNumber = 0.0;
        double rightNumber = 0.0;
        if (TryParseNumber(leftText, leftNumber) && TryParseNumber(rightText, rightNumber))
        {
            if (leftNumber < rightNumber)
            {
                return -1;
            }
            if (leftNumber > rightNumber)
            {
                return 1;
            }
            return 0;
        }

        return leftText.compare(rightText);
    }

    std::string GetWindowUtf8(HWND control)
    {
        const int length = GetWindowTextLengthW(control);
        std::wstring text(static_cast<size_t>(length) + 1, L'\0');
        if (length > 0)
        {
            GetWindowTextW(control, text.data(), length + 1);
        }
        text.resize(static_cast<size_t>(length));
        return WideToUtf8(text);
    }

    void SetWindowUtf8(HWND control, const std::string& text)
    {
        SetWindowTextW(control, Utf8ToWide(text).c_str());
    }

    std::string GetComboSelectionUtf8(HWND combo)
    {
        const LRESULT selected = SendMessageW(combo, CB_GETCURSEL, 0, 0);
        if (selected == CB_ERR)
        {
            return GetWindowUtf8(combo);
        }

        const LRESULT length = SendMessageW(combo, CB_GETLBTEXTLEN, static_cast<WPARAM>(selected), 0);
        if (length == CB_ERR || length < 0)
        {
            return {};
        }

        std::wstring text(static_cast<size_t>(length) + 1, L'\0');
        SendMessageW(combo, CB_GETLBTEXT, static_cast<WPARAM>(selected), reinterpret_cast<LPARAM>(text.data()));
        text.resize(static_cast<size_t>(length));
        return WideToUtf8(text);
    }

    void AddUniqueRuleType(std::vector<std::string>& values, const std::string& value)
    {
        const std::string trimmed = Trim(value);
        if (trimmed.empty())
        {
            return;
        }
        if (std::find(values.begin(), values.end(), trimmed) == values.end())
        {
            values.push_back(trimmed);
        }
    }

    void RebuildRuleTypes(RulesEditorContext* context)
    {
        if (context == nullptr)
        {
            return;
        }

        context->ruleTypes.clear();
        for (const char* type : kDefaultRuleTypes)
        {
            AddUniqueRuleType(context->ruleTypes, type);
        }
        for (const KonBiaoZu::RuleRecord& rule : context->rules)
        {
            AddUniqueRuleType(context->ruleTypes, rule.annotationType);
        }
        if (context->ruleTypes.empty())
        {
            context->ruleTypes.push_back("孔标注");
        }
    }

    void RefreshTypeCombo(RulesEditorContext* context)
    {
        if (context == nullptr || context->edits[0] == nullptr)
        {
            return;
        }

        SendMessageW(context->edits[0], CB_RESETCONTENT, 0, 0);
        for (const std::string& type : context->ruleTypes)
        {
            const std::wstring text = Utf8ToWide(type);
            SendMessageW(context->edits[0], CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
        }
    }

    void RefreshTypeTabs(RulesEditorContext* context)
    {
        if (context == nullptr || context->tabs == nullptr)
        {
            return;
        }

        LRESULT current = SendMessageW(context->tabs, CB_GETCURSEL, 0, 0);
        std::string currentType;
        if (current > 0 && current - 1 < static_cast<LRESULT>(context->ruleTypes.size()))
        {
            currentType = context->ruleTypes[static_cast<size_t>(current - 1)];
        }
        SendMessageW(context->tabs, CB_RESETCONTENT, 0, 0);
        SendMessageW(context->tabs, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"全部"));

        for (int index = 0; index < static_cast<int>(context->ruleTypes.size()); ++index)
        {
            std::wstring text = Utf8ToWide(context->ruleTypes[static_cast<size_t>(index)]);
            SendMessageW(context->tabs, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
        }

        current = 0;
        if (!currentType.empty())
        {
            for (int index = 0; index < static_cast<int>(context->ruleTypes.size()); ++index)
            {
                if (context->ruleTypes[static_cast<size_t>(index)] == currentType)
                {
                    current = index + 1;
                    break;
                }
            }
        }
        if (current < 0 || current > static_cast<int>(context->ruleTypes.size()))
        {
            current = 0;
        }
        SendMessageW(context->tabs, CB_SETCURSEL, static_cast<WPARAM>(current), 0);
        RefreshTypeCombo(context);
    }

    std::string GetSelectedTabType(RulesEditorContext* context)
    {
        if (context == nullptr || context->tabs == nullptr)
        {
            return {};
        }

        const LRESULT selection = SendMessageW(context->tabs, CB_GETCURSEL, 0, 0);
        if (selection <= 0 || selection > static_cast<int>(context->ruleTypes.size()))
        {
            return {};
        }
        return context->ruleTypes[static_cast<size_t>(selection - 1)];
    }

    KonBiaoZu::RuleRecord ReadEditorRule(RulesEditorContext* context)
    {
        KonBiaoZu::RuleRecord rule;
        if (context == nullptr)
        {
            return rule;
        }

        rule.annotationType = GetComboSelectionUtf8(context->edits[0]);
        rule.series = GetWindowUtf8(context->edits[1]);
        rule.size = GetWindowUtf8(context->edits[2]);
        rule.threadSpec = GetWindowUtf8(context->edits[3]);
        rule.lengthText = GetWindowUtf8(context->edits[4]);
        rule.displayText = GetWindowUtf8(context->edits[6]);
        rule.standardComment = GetWindowUtf8(context->edits[7]);
        rule.note = GetWindowUtf8(context->edits[8]);

        try
        {
            rule.bottomHole = std::stod(GetWindowUtf8(context->edits[5]));
        }
        catch (...)
        {
            rule.bottomHole = 0.0;
        }

        if (rule.annotationType.empty())
        {
            rule.annotationType = GetSelectedTabType(context);
        }
        if (rule.annotationType.empty())
        {
            rule.annotationType = context->ruleTypes.empty() ? "孔标注" : context->ruleTypes[0];
        }
        if (rule.series.empty())
        {
            rule.series = "<未指定>";
        }
        if (rule.size.empty())
        {
            rule.size = "M3";
        }
        if (rule.threadSpec.empty())
        {
            rule.threadSpec = rule.series + "-" + rule.size;
        }
        if (rule.displayText.empty())
        {
            rule.displayText = "{规格}";
        }

        return rule;
    }

    void SaveEditorSelection(RulesEditorContext* context)
    {
        (void)context;
    }

    void LoadEditorSelection(RulesEditorContext* context)
    {
        (void)context;
    }

    void RefreshEditorList(RulesEditorContext* context)
    {
        if (context == nullptr)
        {
            return;
        }

        ListView_DeleteAllItems(context->list);
        const std::string filter = GetSelectedTabType(context);
        int itemIndex = 0;
        for (size_t typeIndex = 0; typeIndex < context->ruleTypes.size(); ++typeIndex)
        {
            const std::string& typeName = context->ruleTypes[typeIndex];
            if (!filter.empty() && typeName != filter)
            {
                continue;
            }

            std::vector<size_t> childRules;
            for (size_t ruleIndex = 0; ruleIndex < context->rules.size(); ++ruleIndex)
            {
                if (context->rules[ruleIndex].annotationType == typeName)
                {
                    childRules.push_back(ruleIndex);
                }
            }
            if (childRules.empty())
            {
                continue;
            }

            if (context->sortColumn > 0)
            {
                const int sortField = FieldForListColumn(context->sortColumn);
                const bool ascending = context->sortAscending;
                std::stable_sort(childRules.begin(), childRules.end(), [&](size_t leftIndex, size_t rightIndex) {
                    const int compare = CompareRuleField(context->rules[leftIndex], context->rules[rightIndex], sortField);
                    if (compare == 0)
                    {
                        return leftIndex < rightIndex;
                    }
                    return ascending ? compare < 0 : compare > 0;
                });
            }

            LVITEMW item {};
            item.mask = LVIF_TEXT | LVIF_PARAM;
            item.iItem = itemIndex;
            item.lParam = -1;
            std::wstring type = L"- " + Utf8ToWide(typeName);
            item.pszText = type.data();
            ListView_InsertItem(context->list, &item);
            ++itemIndex;

            for (size_t ruleIndex : childRules)
            {
                const KonBiaoZu::RuleRecord& rule = context->rules[ruleIndex];
                LVITEMW child {};
                child.mask = LVIF_TEXT | LVIF_PARAM;
                child.iItem = itemIndex;
                child.lParam = static_cast<LPARAM>(ruleIndex);
                std::wstring childType = L"   └ " + Utf8ToWide(rule.size);
                child.pszText = childType.data();
                ListView_InsertItem(context->list, &child);

                for (int column = 1; column < 6; ++column)
                {
                    std::wstring text = RuleFieldText(rule, kListFields[column - 1]);
                    ListView_SetItemText(context->list, itemIndex, column, text.data());
                }
                ++itemIndex;
            }
        }
    }

    void SelectEditorListRule(RulesEditorContext* context, int ruleIndex)
    {
        if (context == nullptr)
        {
            return;
        }

        const int itemCount = ListView_GetItemCount(context->list);
        for (int itemIndex = 0; itemIndex < itemCount; ++itemIndex)
        {
            LVITEMW item {};
            item.mask = LVIF_PARAM;
            item.iItem = itemIndex;
            if (!ListView_GetItem(context->list, &item) || item.lParam < 0 || static_cast<int>(item.lParam) != ruleIndex)
            {
                continue;
            }

            ListView_SetItemState(context->list, itemIndex, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            ListView_EnsureVisible(context->list, itemIndex, FALSE);
            return;
        }
    }

    void UpdateEditorListRow(RulesEditorContext* context)
    {
        if (context == nullptr || context->selectedRule < 0 || static_cast<size_t>(context->selectedRule) >= context->rules.size())
        {
            return;
        }

        const int selectedRule = context->selectedRule;
        RebuildRuleTypes(context);
        RefreshTypeTabs(context);
        RefreshEditorList(context);
        SelectEditorListRule(context, selectedRule);
    }

    bool SaveEditorRules(RulesEditorContext* context, std::string& errorMessage)
    {
        SaveEditorSelection(context);

        const std::filesystem::path configPath(context->configPath);
        std::error_code createError;
        std::filesystem::create_directories(configPath.parent_path(), createError);
        if (createError)
        {
            errorMessage = Utf8ToSystem("无法创建规则配置目录: " + configPath.parent_path().string());
            return false;
        }

        std::ofstream output(context->configPath, std::ios::binary | std::ios::trunc);
        if (!output.is_open())
        {
            errorMessage = Utf8ToSystem("无法写入规则配置文件: " + context->configPath);
            return false;
        }

        output << "; KonBiaoZu rule table, UTF-8\n";
        output << "version=1\n\n";
        size_t writtenCount = 0;
        for (size_t index = 0; index < context->rules.size(); ++index)
        {
            const KonBiaoZu::RuleRecord& rule = context->rules[index];
            if (!HasRuleContent(rule))
            {
                continue;
            }

            ++writtenCount;
            WriteIniRule(output, writtenCount, rule);
        }

        if (context->liveRules != nullptr && context->liveRulesByType != nullptr)
        {
            std::vector<KonBiaoZu::RuleRecord> appliedRules;
            appliedRules.reserve(context->rules.size());
            for (const KonBiaoZu::RuleRecord& rule : context->rules)
            {
                if (HasRuleContent(rule))
                {
                    appliedRules.push_back(rule);
                }
            }
            ApplyTemplates(appliedRules);
            *context->liveRules = std::move(appliedRules);
            RebuildRuleIndex(*context->liveRules, *context->liveRulesByType);
        }

        context->dirty = false;
        return true;
    }

    std::string EscapeCsv(const std::string& value)
    {
        bool needsQuotes = value.find_first_of(",\"\r\n") != std::string::npos;
        std::string escaped;
        for (char ch : value)
        {
            if (ch == '"')
            {
                escaped += "\"\"";
                needsQuotes = true;
            }
            else
            {
                escaped.push_back(ch);
            }
        }
        return needsQuotes ? "\"" + escaped + "\"" : escaped;
    }

    std::vector<std::string> ParseCsvLine(const std::string& line)
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
                values.push_back(current);
                current.clear();
            }
            else
            {
                current.push_back(ch);
            }
        }
        values.push_back(current);
        return values;
    }

    void WriteCsvRule(std::ostream& output, const KonBiaoZu::RuleRecord& rule)
    {
        output << EscapeCsv(rule.annotationType) << ','
               << EscapeCsv(rule.series) << ','
               << EscapeCsv(rule.size) << ','
               << EscapeCsv(rule.threadSpec) << ','
               << EscapeCsv(rule.lengthText) << ','
               << std::fixed << std::setprecision(2) << rule.bottomHole << ','
               << EscapeCsv(rule.displayText) << ','
               << EscapeCsv(rule.standardComment) << ','
               << EscapeCsv(rule.note) << "\n";
    }

    bool ExportEditorRulesCsv(RulesEditorContext* context, const std::wstring& path, std::string& errorMessage)
    {
        SaveEditorSelection(context);
        std::ofstream output(std::filesystem::path(path), std::ios::binary | std::ios::trunc);
        if (!output.is_open())
        {
            errorMessage = Utf8ToSystem("无法写入导出表格: " + WideToUtf8(path));
            return false;
        }

        output << "\xEF\xBB\xBF";
        output << "type,series,size,threadSpec,lengthText,bottomHole,displayText,standardComment,note\n";
        for (const KonBiaoZu::RuleRecord& rule : context->rules)
        {
            if (HasRuleContent(rule))
            {
                WriteCsvRule(output, rule);
            }
        }
        return true;
    }

    bool ImportEditorRulesCsv(RulesEditorContext* context, const std::wstring& path, std::string& errorMessage)
    {
        std::ifstream input(std::filesystem::path(path), std::ios::binary);
        if (!input.is_open())
        {
            errorMessage = Utf8ToSystem("无法打开导入表格: " + WideToUtf8(path));
            return false;
        }

        std::ostringstream buffer;
        buffer << input.rdbuf();
        std::string text = buffer.str();
        if (text.size() >= 3 &&
            static_cast<unsigned char>(text[0]) == 0xEF &&
            static_cast<unsigned char>(text[1]) == 0xBB &&
            static_cast<unsigned char>(text[2]) == 0xBF)
        {
            text.erase(0, 3);
        }

        std::vector<KonBiaoZu::RuleRecord> imported;
        std::istringstream lines(text);
        std::string line;
        bool firstLine = true;
        while (std::getline(lines, line))
        {
            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }
            if (Trim(line).empty())
            {
                continue;
            }

            std::vector<std::string> values = ParseCsvLine(line);
            if (firstLine)
            {
                firstLine = false;
                if (!values.empty() && Trim(values[0]) == "type")
                {
                    continue;
                }
            }
            if (values.size() < 6)
            {
                errorMessage = Utf8ToSystem("导入表格列数不足: " + WideToUtf8(path));
                return false;
            }

            KonBiaoZu::RuleRecord rule;
            rule.annotationType = Trim(values[0]);
            rule.series = values.size() > 1 ? Trim(values[1]) : std::string();
            rule.size = values.size() > 2 ? Trim(values[2]) : std::string();
            rule.threadSpec = values.size() > 3 ? Trim(values[3]) : std::string();
            rule.lengthText = values.size() > 4 ? Trim(values[4]) : std::string();
            rule.bottomHole = std::atof(values[5].c_str());
            rule.displayText = values.size() > 6 ? Trim(values[6]) : std::string();
            rule.standardComment = values.size() > 7 ? Trim(values[7]) : std::string();
            rule.note = values.size() > 8 ? Trim(values[8]) : std::string();
            if (HasRuleContent(rule))
            {
                imported.push_back(rule);
            }
        }

        if (imported.empty())
        {
            errorMessage = Utf8ToSystem("导入表格没有有效规则: " + WideToUtf8(path));
            return false;
        }

        context->rules = std::move(imported);
        context->selectedRule = -1;
        RebuildRuleTypes(context);
        RefreshTypeTabs(context);
        RefreshEditorList(context);
        LoadEditorSelection(context);
        context->dirty = true;
        return true;
    }

    bool ChooseCsvFile(HWND owner, bool saveDialog, std::wstring& path)
    {
        wchar_t fileName[MAX_PATH] = L"rules.csv";
        OPENFILENAMEW dialog {};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = owner;
        dialog.lpstrFilter = L"CSV 表格 (*.csv)\0*.csv\0所有文件 (*.*)\0*.*\0";
        dialog.lpstrFile = fileName;
        dialog.nMaxFile = MAX_PATH;
        dialog.lpstrDefExt = L"csv";
        dialog.Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | (saveDialog ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);

        const BOOL ok = saveDialog ? GetSaveFileNameW(&dialog) : GetOpenFileNameW(&dialog);
        if (!ok)
        {
            return false;
        }
        path = fileName;
        return true;
    }

    LRESULT CALLBACK RulesEditorProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        RulesEditorContext* context = reinterpret_cast<RulesEditorContext*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (message == WM_NCCREATE)
        {
            auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
            context = reinterpret_cast<RulesEditorContext*>(create->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(context));
            context->window = hwnd;
            return TRUE;
        }

        switch (message)
        {
        case WM_COMMAND:
            if (context == nullptr)
            {
                break;
            }
            if (LOWORD(wParam) >= kIdFirstEdit && LOWORD(wParam) < kIdFirstEdit + 9 &&
                (HIWORD(wParam) == EN_CHANGE || HIWORD(wParam) == CBN_EDITCHANGE || HIWORD(wParam) == CBN_SELCHANGE))
            {
                if (!context->loadingSelection)
                {
                    context->dirty = true;
                    SaveEditorSelection(context);
                    UpdateEditorListRow(context);
                }
                return 0;
            }
            if (LOWORD(wParam) == kIdTypeTabs && HIWORD(wParam) == CBN_SELCHANGE)
            {
                context->selectedRule = -1;
                RefreshEditorList(context);
                LoadEditorSelection(context);
                return 0;
            }
            if (LOWORD(wParam) == kIdAdd)
            {
                KonBiaoZu::RuleRecord rule;
                rule.annotationType = GetSelectedTabType(context);
                if (rule.annotationType.empty())
                {
                    rule.annotationType = context->ruleTypes.empty() ? "孔标注" : context->ruleTypes[0];
                }
                rule.series = "<未指定>";
                rule.size = "M3";
                rule.threadSpec = rule.series + "-" + rule.size;
                rule.bottomHole = 0.0;
                rule.displayText = "{规格}";
                context->rules.push_back(rule);
                context->selectedRule = static_cast<int>(context->rules.size() - 1);
                RefreshEditorList(context);
                SelectEditorListRule(context, context->selectedRule);
                LoadEditorSelection(context);
                context->dirty = true;
                return 0;
            }
            if (LOWORD(wParam) == kIdDelete)
            {
                if (context->selectedRule >= 0 && static_cast<size_t>(context->selectedRule) < context->rules.size())
                {
                    context->rules.erase(context->rules.begin() + context->selectedRule);
                    context->selectedRule = -1;
                    RefreshEditorList(context);
                    LoadEditorSelection(context);
                    context->dirty = true;
                }
                return 0;
            }
            if (LOWORD(wParam) == kIdSave)
            {
                const int savedRule = context->selectedRule;
                std::string error;
                if (!SaveEditorRules(context, error))
                {
                    MessageBoxW(hwnd, Utf8ToWide(error).c_str(), L"保存失败", MB_ICONERROR);
                }
                RebuildRuleTypes(context);
                RefreshTypeTabs(context);
                RefreshEditorList(context);
                SelectEditorListRule(context, savedRule);
                LoadEditorSelection(context);
                return 0;
            }
            if (LOWORD(wParam) == kIdImport)
            {
                std::wstring path;
                if (ChooseCsvFile(hwnd, false, path))
                {
                    std::string error;
                    if (!ImportEditorRulesCsv(context, path, error))
                    {
                        MessageBoxW(hwnd, Utf8ToWide(error).c_str(), L"导入失败", MB_ICONERROR);
                    }
                    else
                    {
                        MessageBoxW(hwnd, L"导入完成，请点击保存写入 INI。", L"导入规则", MB_ICONINFORMATION);
                    }
                }
                return 0;
            }
            if (LOWORD(wParam) == kIdExport)
            {
                std::wstring path;
                if (ChooseCsvFile(hwnd, true, path))
                {
                    std::string error;
                    if (!ExportEditorRulesCsv(context, path, error))
                    {
                        MessageBoxW(hwnd, Utf8ToWide(error).c_str(), L"导出失败", MB_ICONERROR);
                    }
                    else
                    {
                        MessageBoxW(hwnd, L"导出完成。", L"导出规则", MB_ICONINFORMATION);
                    }
                }
                return 0;
            }
            if (LOWORD(wParam) == kIdClose)
            {
                DestroyWindow(hwnd);
                return 0;
            }
            break;

        case WM_NOTIFY:
            if (context != nullptr && reinterpret_cast<NMHDR*>(lParam)->idFrom == kIdRuleList)
            {
                const NMHDR* header = reinterpret_cast<NMHDR*>(lParam);
                if (header->code == LVN_ITEMCHANGED)
                {
                    const NMLISTVIEW* changed = reinterpret_cast<NMLISTVIEW*>(lParam);
                    if ((changed->uChanged & LVIF_STATE) != 0 &&
                        (changed->uNewState & LVIS_SELECTED) != 0)
                    {
                        LVITEMW item {};
                        item.mask = LVIF_PARAM;
                        item.iItem = changed->iItem;
                        if (ListView_GetItem(context->list, &item))
                        {
                            if (item.lParam < 0)
                            {
                                context->selectedRule = -1;
                                LoadEditorSelection(context);
                                return 0;
                            }
                            context->selectedRule = static_cast<int>(item.lParam);
                            LoadEditorSelection(context);
                        }
                    }
                }
                else if (header->code == LVN_COLUMNCLICK)
                {
                    const NMLISTVIEW* clicked = reinterpret_cast<NMLISTVIEW*>(lParam);
                    if (context->sortColumn == clicked->iSubItem)
                    {
                        context->sortAscending = !context->sortAscending;
                    }
                    else
                    {
                        context->sortColumn = clicked->iSubItem;
                        context->sortAscending = true;
                    }

                    const int selectedRule = context->selectedRule;
                    RefreshEditorList(context);
                    SelectEditorListRule(context, selectedRule);
                    return 0;
                }
            }
            break;

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            return 0;

        case WM_NCDESTROY:
            if (context != nullptr)
            {
                if (context->ownerWindowSlot != nullptr && reinterpret_cast<HWND>(*context->ownerWindowSlot) == hwnd)
                {
                    *context->ownerWindowSlot = nullptr;
                }
                if (context->uiFont != nullptr)
                {
                    DeleteObject(context->uiFont);
                    context->uiFont = nullptr;
                }
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
                delete context;
            }
            return 0;
        }

        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    void AddListColumn(HWND list, int index, const wchar_t* title, int width)
    {
        LVCOLUMNW column {};
        column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        column.pszText = const_cast<LPWSTR>(title);
        column.cx = width;
        column.iSubItem = index;
        ListView_InsertColumn(list, index, &column);
    }

    HMENU ControlId(int id)
    {
        return reinterpret_cast<HMENU>(static_cast<INT_PTR>(id));
    }

    BOOL CALLBACK ApplyEditorFontToChild(HWND child, LPARAM lParam)
    {
        SendMessageW(child, WM_SETFONT, static_cast<WPARAM>(lParam), TRUE);
        return TRUE;
    }

    void ApplyEditorFont(RulesEditorContext& context)
    {
        if (context.uiFont == nullptr)
        {
            context.uiFont = CreateFontW(
                -12,
                0,
                0,
                0,
                FW_NORMAL,
                FALSE,
                FALSE,
                FALSE,
                DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS,
                CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY,
                FF_DONTCARE,
                L"宋体");
        }
        if (context.uiFont == nullptr)
        {
            return;
        }

        SendMessageW(context.window, WM_SETFONT, reinterpret_cast<WPARAM>(context.uiFont), TRUE);
        EnumChildWindows(context.window, ApplyEditorFontToChild, reinterpret_cast<LPARAM>(context.uiFont));
    }

    void CreateRulesEditorControls(RulesEditorContext& context)
    {
        HINSTANCE instance = DllInstance();
        CreateWindowW(L"BUTTON", L"选择规则", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 12, 8, 940, 70, context.window, nullptr, instance, nullptr);
        CreateWindowW(L"STATIC", L"选择规则", WS_CHILD | WS_VISIBLE, 28, 36, 70, 22, context.window, nullptr, instance, nullptr);
        context.tabs = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            L"COMBOBOX",
            L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
            100,
            32,
            220,
            180,
            context.window,
            ControlId(kIdTypeTabs),
            instance,
            nullptr);

        RebuildRuleTypes(&context);
        RefreshTypeTabs(&context);

        CreateWindowW(L"BUTTON", L"规则表", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 12, 88, 940, 500, context.window, nullptr, instance, nullptr);

        context.list = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            WC_LISTVIEWW,
            L"",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
            24,
            118,
            916,
            448,
            context.window,
            ControlId(kIdRuleList),
            instance,
            nullptr);
        ListView_SetExtendedListViewStyle(context.list, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
        AddListColumn(context.list, 0, L"类别", 130);
        AddListColumn(context.list, 1, L"规格", 105);
        AddListColumn(context.list, 2, L"标准长度", 90);
        AddListColumn(context.list, 3, L"底孔", 75);
        AddListColumn(context.list, 4, L"显示标注", 280);
        AddListColumn(context.list, 5, L"说明", 210);

        CreateWindowW(L"BUTTON", L"保存", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 12, 596, 940, 58, context.window, nullptr, instance, nullptr);
        CreateWindowW(L"BUTTON", L"保存", WS_CHILD | WS_VISIBLE, 24, 620, 64, 28, context.window, ControlId(kIdSave), instance, nullptr);
        CreateWindowW(L"BUTTON", L"新增规格", WS_CHILD | WS_VISIBLE, 96, 620, 86, 28, context.window, ControlId(kIdAdd), instance, nullptr);
        CreateWindowW(L"BUTTON", L"导入EXCEL数据", WS_CHILD | WS_VISIBLE, 190, 620, 118, 28, context.window, ControlId(kIdImport), instance, nullptr);
        CreateWindowW(L"BUTTON", L"导出EXCEL数据", WS_CHILD | WS_VISIBLE, 316, 620, 118, 28, context.window, ControlId(kIdExport), instance, nullptr);
        CreateWindowW(L"BUTTON", L"删除选中", WS_CHILD | WS_VISIBLE, 442, 620, 86, 28, context.window, ControlId(kIdDelete), instance, nullptr);
        CreateWindowW(L"BUTTON", L"关闭", WS_CHILD | WS_VISIBLE, 884, 620, 56, 28, context.window, ControlId(kIdClose), instance, nullptr);

        RefreshEditorList(&context);
        ApplyEditorFont(context);
    }

    bool ShowRulesEditor(
        const std::string& configPath,
        std::vector<KonBiaoZu::RuleRecord> rules,
        std::vector<KonBiaoZu::RuleRecord>& liveRules,
        std::unordered_map<std::string, std::vector<KonBiaoZu::RuleRecord>>& liveRulesByType,
        void** editorWindowSlot,
        std::string& errorMessage)
    {
        HWND existingWindow = editorWindowSlot != nullptr ? reinterpret_cast<HWND>(*editorWindowSlot) : nullptr;
        if (existingWindow != nullptr && IsWindow(existingWindow))
        {
            ShowWindow(existingWindow, SW_SHOWNORMAL);
            SetForegroundWindow(existingWindow);
            return true;
        }

        INITCOMMONCONTROLSEX controls {};
        controls.dwSize = sizeof(controls);
        controls.dwICC = ICC_LISTVIEW_CLASSES;
        InitCommonControlsEx(&controls);

        HINSTANCE instance = DllInstance();
        const wchar_t* className = L"KonBiaoZuRulesEditorWindow";
        WNDCLASSW windowClass {};
        windowClass.lpfnWndProc = RulesEditorProc;
        windowClass.hInstance = instance;
        windowClass.lpszClassName = className;
        windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
        windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
        RegisterClassW(&windowClass);

        auto* context = new RulesEditorContext();
        context->configPath = configPath;
        context->rules = std::move(rules);
        context->liveRules = &liveRules;
        context->liveRulesByType = &liveRulesByType;
        context->ownerWindowSlot = editorWindowSlot;

        HWND window = CreateWindowExW(
            WS_EX_APPWINDOW,
            className,
            L"规则表",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            1000,
            720,
            nullptr,
            nullptr,
            instance,
            context);
        if (window == nullptr)
        {
            delete context;
            errorMessage = Utf8ToSystem("无法创建规则表窗口。");
            return false;
        }
        if (editorWindowSlot != nullptr)
        {
            *editorWindowSlot = window;
        }

        CreateRulesEditorControls(*context);
        ShowWindow(window, SW_SHOWNORMAL);
        UpdateWindow(window);

        MSG message {};
        while (IsWindow(window))
        {
            const BOOL result = GetMessageW(&message, nullptr, 0, 0);
            if (result <= 0)
            {
                if (result == 0)
                {
                    PostQuitMessage(static_cast<int>(message.wParam));
                }
                break;
            }

            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        if (editorWindowSlot != nullptr && reinterpret_cast<HWND>(*editorWindowSlot) == window)
        {
            *editorWindowSlot = nullptr;
        }
        UnregisterClassW(className, instance);

        return true;
    }
}

namespace KonBiaoZu
{
    ExcelRuleManager::ExcelRuleManager()
        : ExcelRuleManager("KonBiaoZuRules.ini")
    {
    }

    ExcelRuleManager::ExcelRuleManager(const std::string& configFileName)
    {
        const std::filesystem::path dataDirectory = GetDataDirectory();
        configPath_ = (dataDirectory / configFileName).string();
    }

    ExcelRuleManager::~ExcelRuleManager()
    {
        CloseEditor();
    }

    void ExcelRuleManager::CloseEditor()
    {
        HWND editorWindow = reinterpret_cast<HWND>(editorWindow_);
        if (editorWindow != nullptr && IsWindow(editorWindow))
        {
            DestroyWindow(editorWindow);
        }
        editorWindow_ = nullptr;
    }

    const std::string& ExcelRuleManager::WorkbookPath() const
    {
        return configPath_;
    }

    const std::vector<RuleRecord>& ExcelRuleManager::Rules() const
    {
        return rules_;
    }

    bool ExcelRuleManager::SaveRules(const std::vector<RuleRecord>& rules, std::string& errorMessage) const
    {
        const std::filesystem::path configPath(configPath_);
        std::error_code createError;
        std::filesystem::create_directories(configPath.parent_path(), createError);
        if (createError)
        {
            errorMessage = Utf8ToSystem("无法创建规则配置目录: " + configPath.parent_path().string());
            return false;
        }

        std::ofstream output(configPath_, std::ios::binary | std::ios::trunc);
        if (!output.is_open())
        {
            errorMessage = Utf8ToSystem("无法写入规则配置文件: " + configPath_);
            return false;
        }

        output << "; KonBiaoZu rule table, UTF-8\n";
        output << "version=1\n\n";
        size_t writtenCount = 0;
        for (size_t index = 0; index < rules.size(); ++index)
        {
            const RuleRecord& rule = rules[index];
            if (!HasRuleContent(rule))
            {
                continue;
            }

            ++writtenCount;
            WriteIniRule(output, writtenCount, rule);
        }
        return true;
    }

    bool ExcelRuleManager::LoadRules(std::string& errorMessage)
    {
        rules_.clear();
        rulesByType_.clear();

        if (!std::filesystem::exists(configPath_))
        {
            std::vector<RuleRecord> defaults = BuildDefaultRules();
            if (!SaveRules(defaults, errorMessage))
            {
                return false;
            }
        }

        std::vector<RuleRecord> loadedRules;
        if (!LoadRawRulesFromConfig(configPath_, loadedRules, errorMessage))
        {
            return false;
        }

        if (loadedRules.empty())
        {
            loadedRules = BuildDefaultRules();
            if (!SaveRules(loadedRules, errorMessage))
            {
                return false;
            }
        }

        ApplyTemplates(loadedRules);
        rules_ = std::move(loadedRules);
        RebuildRuleIndex(rules_, rulesByType_);

        return true;
    }

    bool ExcelRuleManager::RefreshRules(std::string& errorMessage)
    {
        return LoadRules(errorMessage);
    }

    std::vector<RuleRecord> ExcelRuleManager::FindRules(
        AnnotationType type,
        const std::string& subtype,
        double bottomHole,
        int lengthValue,
        double tolerance) const
    {
        std::vector<RuleRecord> matches;
        const std::string key = ToRuleKey(type);
        const auto rulesIt = rulesByType_.find(key);
        if (rulesIt == rulesByType_.end())
        {
            return matches;
        }

        for (const RuleRecord& rule : rulesIt->second)
        {
            if (!subtype.empty() && rule.series != subtype)
            {
                continue;
            }

            if (std::abs(rule.bottomHole - bottomHole) > tolerance)
            {
                continue;
            }

            if (lengthValue > 0 && !rule.lengthText.empty() && rule.lengthText != "按输入")
            {
                if (rule.lengthText != std::to_string(lengthValue))
                {
                    continue;
                }
            }

            matches.push_back(rule);
        }

        std::sort(matches.begin(), matches.end(), RuleLess);
        return matches;
    }

    bool ExcelRuleManager::HasRules() const
    {
        return !rules_.empty();
    }

    bool ExcelRuleManager::OpenWorkbook(std::string& errorMessage)
    {
        if (!std::filesystem::exists(configPath_))
        {
            std::vector<RuleRecord> defaults = BuildDefaultRules();
            if (!SaveRules(defaults, errorMessage))
            {
                return false;
            }
        }

        std::vector<RuleRecord> editableRules;
        if (!LoadRawRulesFromConfig(configPath_, editableRules, errorMessage))
        {
            return false;
        }

        return ShowNxRulesEditor(configPath_, std::move(editableRules), rules_, rulesByType_, errorMessage);
    }
}


