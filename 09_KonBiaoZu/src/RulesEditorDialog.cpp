#include "RulesEditorDialog.hpp"

#include <NXOpen/BlockStyler_BlockDialog.hxx>
#include <NXOpen/BlockStyler_Enumeration.hxx>
#include <NXOpen/BlockStyler_Node.hxx>
#include <NXOpen/BlockStyler_PropertyList.hxx>
#include <NXOpen/BlockStyler_Tree.hxx>
#include <NXOpen/BlockStyler_UIBlock.hxx>
#include <NXOpen/Callback.hxx>
#include <NXOpen/NXMessageBox.hxx>
#include <NXOpen/NXString.hxx>
#include <NXOpen/UI.hxx>

#include <Windows.h>
#include <commdlg.h>
#ifdef CreateDialog
#undef CreateDialog
#endif

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <optional>
#include <sstream>
#include <unordered_set>

#pragma comment(lib, "Comdlg32.lib")

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace
{
    using NXOpen::NXString;
    using NXOpen::BlockStyler::BlockDialog;
    using NXOpen::BlockStyler::Enumeration;
    using NXOpen::BlockStyler::Node;
    using NXOpen::BlockStyler::PropertyList;
    using NXOpen::BlockStyler::Tree;
    using NXOpen::BlockStyler::UIBlock;

    enum RuleEditorColumns
    {
        ColumnIndex = 0,
        ColumnAnnotationType = 1,
        ColumnSeries = 2,
        ColumnSize = 3,
        ColumnThreadSpec = 4,
        ColumnLengthText = 5,
        ColumnBottomHole = 6,
        ColumnDisplayText = 7,
        ColumnStandardComment = 8,
        ColumnNote = 9
    };

    std::string Utf8ToSystem(const std::string& utf8);
    std::string SystemToUtf8(const std::string& systemText);
    std::string WideToUtf8(const std::wstring& text);
    bool IsValidUtf8Bytes(const std::string& text);
    std::string NormalizeTextEncoding(const std::string& text);
    void ReplaceAll(std::string& text, const std::string& from, const std::string& to);

    NXString Utf8NxString(const std::string& text)
    {
        return NXString(text.c_str(), NXString::UTF8);
    }

    NXString Utf8NxString(const char* text)
    {
        return NXString(text == nullptr ? "" : text, NXString::UTF8);
    }

    NXString UiNxString(const std::string& text)
    {
        return NXString(Utf8ToSystem(text), NXString::Locale);
    }

    NXString UiNxString(const char* text)
    {
        return UiNxString(std::string(text == nullptr ? "" : text));
    }

    std::string NxStringToUtf8(const NXString& text)
    {
        const char* rawValue = text.GetText();
        if (rawValue != nullptr)
        {
            const std::string rawText(rawValue);
            if (IsValidUtf8Bytes(rawText))
            {
                return NormalizeTextEncoding(rawText);
            }
        }

        const char* localeValue = text.GetLocaleText();
        if (localeValue != nullptr)
        {
            const std::string localeText(localeValue);
            if (IsValidUtf8Bytes(localeText))
            {
                return NormalizeTextEncoding(localeText);
            }
            return NormalizeTextEncoding(SystemToUtf8(localeText));
        }

        const char* utf8Value = text.GetUTF8Text();
        if (utf8Value != nullptr)
        {
            return NormalizeTextEncoding(std::string(utf8Value));
        }

        return {};
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

    std::string SystemToUtf8(const std::string& systemText)
    {
        if (systemText.empty())
        {
            return {};
        }

        const int wideSize = MultiByteToWideChar(CP_ACP, 0, systemText.c_str(), -1, nullptr, 0);
        if (wideSize <= 0)
        {
            return systemText;
        }

        std::wstring wide(static_cast<size_t>(wideSize), L'\0');
        MultiByteToWideChar(CP_ACP, 0, systemText.c_str(), -1, wide.data(), wideSize);

        const int utf8Size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (utf8Size <= 0)
        {
            return systemText;
        }

        std::string utf8(static_cast<size_t>(utf8Size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, utf8.data(), utf8Size, nullptr, nullptr);
        if (!utf8.empty() && utf8.back() == '\0')
        {
            utf8.pop_back();
        }
        return utf8;
    }

    std::string WideToUtf8(const std::wstring& text)
    {
        if (text.empty())
        {
            return {};
        }

        const int utf8Size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (utf8Size <= 0)
        {
            return {};
        }

        std::string utf8(static_cast<size_t>(utf8Size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, utf8.data(), utf8Size, nullptr, nullptr);
        if (!utf8.empty() && utf8.back() == '\0')
        {
            utf8.pop_back();
        }
        return utf8;
    }

    bool IsValidUtf8Bytes(const std::string& text)
    {
        if (text.empty())
        {
            return true;
        }

        const int wideSize = MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            text.data(),
            static_cast<int>(text.size()),
            nullptr,
            0);
        return wideSize > 0;
    }

    std::wstring Utf8ToWideStrict(const std::string& text)
    {
        if (text.empty())
        {
            return {};
        }

        const int wideSize = MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            text.data(),
            static_cast<int>(text.size()),
            nullptr,
            0);
        if (wideSize <= 0)
        {
            return {};
        }

        std::wstring wide(static_cast<size_t>(wideSize), L'\0');
        MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            text.data(),
            static_cast<int>(text.size()),
            wide.data(),
            wideSize);
        return wide;
    }

    std::string WideToSystemBytes(const std::wstring& text)
    {
        if (text.empty())
        {
            return {};
        }

        const int byteSize = WideCharToMultiByte(
            CP_ACP,
            0,
            text.data(),
            static_cast<int>(text.size()),
            nullptr,
            0,
            nullptr,
            nullptr);
        if (byteSize <= 0)
        {
            return {};
        }

        std::string bytes(static_cast<size_t>(byteSize), '\0');
        WideCharToMultiByte(
            CP_ACP,
            0,
            text.data(),
            static_cast<int>(text.size()),
            bytes.data(),
            byteSize,
            nullptr,
            nullptr);
        return bytes;
    }

    bool ContainsMojibakeMarker(const std::string& text)
    {
        const char* markers[] = {
            "锟", "拷", "鎴", "鐨", "锛", "锘", "閾", "閿", "璁", "勫", "绨", "瀵", "彬",
            "铻", "烘", "瘝", "搴", "纰", "抽", "挗", "浠", "讹", "級", "紙"
        };
        for (const char* marker : markers)
        {
            if (text.find(marker) != std::string::npos)
            {
                return true;
            }
        }
        return false;
    }

    std::string NormalizeTextEncoding(const std::string& text)
    {
        if (text.empty() || !ContainsMojibakeMarker(text))
        {
            return text;
        }

        const std::wstring wide = Utf8ToWideStrict(text);
        if (wide.empty())
        {
            return text;
        }

        const std::string systemBytes = WideToSystemBytes(wide);
        if (IsValidUtf8Bytes(systemBytes))
        {
            return systemBytes;
        }

        const int wideSize = MultiByteToWideChar(
            CP_UTF8,
            0,
            systemBytes.data(),
            static_cast<int>(systemBytes.size()),
            nullptr,
            0);
        if (wideSize <= 0)
        {
            return text;
        }

        std::wstring recovered(static_cast<size_t>(wideSize), L'\0');
        MultiByteToWideChar(
            CP_UTF8,
            0,
            systemBytes.data(),
            static_cast<int>(systemBytes.size()),
            recovered.data(),
            wideSize);
        return WideToUtf8(recovered);
    }

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

    struct NumberToken
    {
        size_t begin;
        size_t end;
        double value;
        int decimals;
    };

    std::optional<NumberToken> FindLastNumberToken(const std::string& text)
    {
        std::optional<NumberToken> result;
        for (size_t i = 0; i < text.size(); ++i)
        {
            if (!std::isdigit(static_cast<unsigned char>(text[i])))
            {
                continue;
            }

            const size_t begin = i;
            while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i])))
            {
                ++i;
            }

            int decimals = 0;
            if (i < text.size() && text[i] == '.' && i + 1 < text.size() &&
                std::isdigit(static_cast<unsigned char>(text[i + 1])))
            {
                ++i;
                const size_t decimalBegin = i;
                while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i])))
                {
                    ++i;
                }
                decimals = static_cast<int>(i - decimalBegin);
            }

            const std::string tokenText = text.substr(begin, i - begin);
            result = NumberToken{begin, i, std::atof(tokenText.c_str()), decimals};
            if (i > begin)
            {
                --i;
            }
        }
        return result;
    }

    std::string FormatIncrementedNumber(double value, int decimals)
    {
        std::ostringstream output;
        output << std::fixed << std::setprecision(decimals) << value;
        return output.str();
    }

    std::string ReplaceNumberToken(
        const std::string& text,
        const NumberToken& token,
        double newValue)
    {
        std::string result = text;
        result.replace(
            token.begin,
            token.end - token.begin,
            FormatIncrementedNumber(newValue, token.decimals));
        return result;
    }

    std::string ReplaceAllCopy(std::string text, const std::string& from, const std::string& to)
    {
        ReplaceAll(text, from, to);
        return text;
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

    void WriteIniRule(std::ostream& output, size_t index, const KonBiaoZu::RuleRecord& rule)
    {
        KonBiaoZu::RuleRecord normalized = rule;
        normalized.annotationType = NormalizeTextEncoding(normalized.annotationType);
        normalized.series = NormalizeTextEncoding(normalized.series);
        normalized.size = NormalizeTextEncoding(normalized.size);
        normalized.threadSpec = NormalizeTextEncoding(normalized.threadSpec);
        normalized.lengthText = NormalizeTextEncoding(normalized.lengthText);
        normalized.displayText = NormalizeTextEncoding(normalized.displayText);
        normalized.standardComment = NormalizeTextEncoding(normalized.standardComment);
        normalized.note = NormalizeTextEncoding(normalized.note);

        output << "[Rule" << std::setw(3) << std::setfill('0') << index << std::setfill(' ') << "]\n";
        output << "annotationType=" << EscapeIni(normalized.annotationType) << "\n";
        output << "series=" << EscapeIni(normalized.series) << "\n";
        output << "size=" << EscapeIni(normalized.size) << "\n";
        output << "threadSpec=" << EscapeIni(normalized.threadSpec) << "\n";
        output << "lengthText=" << EscapeIni(normalized.lengthText) << "\n";
        output << "bottomHole=" << std::fixed << std::setprecision(2) << normalized.bottomHole << "\n";
        output << "displayText=" << EscapeIni(normalized.displayText) << "\n";
        output << "standardComment=" << EscapeIni(normalized.standardComment) << "\n";
        output << "note=" << EscapeIni(normalized.note) << "\n\n";
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
        ReplaceAll(result, "{底孔}", FormatDouble(record.bottomHole));
        ReplaceAll(result, "{长度}", record.lengthText);
        return Trim(result);
    }

    void ApplyTemplates(std::vector<KonBiaoZu::RuleRecord>& rules)
    {
        for (KonBiaoZu::RuleRecord& rule : rules)
        {
            rule.displayText = ApplyTemplate(rule.displayText, rule);
            rule.standardComment = ApplyTemplate(rule.standardComment, rule);
        }
    }

    void RebuildRuleIndex(
        const std::vector<KonBiaoZu::RuleRecord>& rules,
        std::unordered_map<std::string, std::vector<KonBiaoZu::RuleRecord>>& rulesByType)
    {
        rulesByType.clear();
        for (const KonBiaoZu::RuleRecord& rule : rules)
        {
            rulesByType[rule.annotationType].push_back(rule);
        }
    }

    std::filesystem::path ModuleDirectory()
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

    std::filesystem::path DebugLogPath()
    {
        const std::filesystem::path moduleDirectory = ModuleDirectory();
        if (!moduleDirectory.empty())
        {
            return moduleDirectory.parent_path() / "DATA" / "KonBiaoZuRulesEditor.log";
        }
        return std::filesystem::temp_directory_path() / "KonBiaoZuRulesEditor.log";
    }

    void DebugLog(const std::string& message)
    {
        try
        {
            const std::filesystem::path path = DebugLogPath();
            std::error_code createError;
            std::filesystem::create_directories(path.parent_path(), createError);
            std::ofstream output(path, std::ios::binary | std::ios::app);
            if (!output.is_open())
            {
                return;
            }

            SYSTEMTIME now {};
            GetLocalTime(&now);
            output << '['
                   << std::setw(4) << std::setfill('0') << now.wYear << '-'
                   << std::setw(2) << now.wMonth << '-'
                   << std::setw(2) << now.wDay << ' '
                   << std::setw(2) << now.wHour << ':'
                   << std::setw(2) << now.wMinute << ':'
                   << std::setw(2) << now.wSecond << '.'
                   << std::setw(3) << now.wMilliseconds
                   << std::setfill(' ') << "] "
                   << message << "\n";
        }
        catch (...)
        {
        }
    }

    std::string BytesToHex(const char* text)
    {
        if (text == nullptr)
        {
            return "<null>";
        }

        std::ostringstream output;
        const unsigned char* bytes = reinterpret_cast<const unsigned char*>(text);
        for (size_t i = 0; bytes[i] != 0 && i < 128; ++i)
        {
            if (i > 0)
            {
                output << ' ';
            }
            output << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
                   << static_cast<int>(bytes[i]);
        }
        return output.str();
    }

    std::vector<std::filesystem::path> BuildSearchRoots()
    {
        std::vector<std::filesystem::path> roots;
        std::unordered_set<std::wstring> seen;

        const auto pushRoot = [&roots, &seen](const std::filesystem::path& root) {
            if (root.empty())
            {
                return;
            }

            std::error_code error;
            const std::filesystem::path canonical = std::filesystem::weakly_canonical(root, error);
            const std::filesystem::path resolved = error ? root.lexically_normal() : canonical;
            if (seen.insert(resolved.wstring()).second)
            {
                roots.push_back(resolved);
            }
        };

        const std::filesystem::path moduleDirectory = ModuleDirectory();
        pushRoot(moduleDirectory);

        std::filesystem::path parent = moduleDirectory;
        for (int i = 0; i < 4 && !parent.empty(); ++i)
        {
            parent = parent.parent_path();
            pushRoot(parent);
        }

        std::error_code currentPathError;
        pushRoot(std::filesystem::current_path(currentPathError));
        return roots;
    }

    std::string FindRulesEditorDlxPath()
    {
        const std::vector<std::filesystem::path> candidates = {
            "KonBiaoZuRulesEditor.dlx",
            "application/KonBiaoZuRulesEditor.dlx",
            "dialog/KonBiaoZuRulesEditor.dlx",
            "KonBiaoZu/dialog/KonBiaoZuRulesEditor.dlx"
        };

        for (const std::filesystem::path& root : BuildSearchRoots())
        {
            for (const std::filesystem::path& candidate : candidates)
            {
                const std::filesystem::path fullPath = root / candidate;
                if (std::filesystem::exists(fullPath))
                {
                    return fullPath.string();
                }
            }
        }

        return {};
    }

    void TrySetStringProperty(UIBlock* block, const char* propertyName, const char* value)
    {
        if (block == nullptr)
        {
            return;
        }

        PropertyList* properties = nullptr;
        try
        {
            properties = block->GetProperties();
            properties->SetString(propertyName, Utf8ToSystem(value).c_str());
        }
        catch (...)
        {
        }

        delete properties;
    }

    void TrySetVisible(UIBlock* block, bool visible)
    {
        if (block == nullptr)
        {
            return;
        }

        const char* propertyNames[] = {"Show", "Visibility"};
        for (const char* propertyName : propertyNames)
        {
            PropertyList* properties = nullptr;
            try
            {
                properties = block->GetProperties();
                properties->SetLogical(propertyName, visible);
            }
            catch (...)
            {
            }

            delete properties;
        }
    }

    void TrySetLogicalProperty(UIBlock* block, const char* propertyName, bool value)
    {
        if (block == nullptr)
        {
            return;
        }

        PropertyList* properties = nullptr;
        try
        {
            properties = block->GetProperties();
            properties->SetLogical(propertyName, value);
        }
        catch (...)
        {
        }

        delete properties;
    }

    void SetNodeText(Node* node, int column, const std::string& text)
    {
        if (node != nullptr)
        {
            node->SetColumnDisplayText(column, Utf8NxString(NormalizeTextEncoding(text)));
        }
    }

    std::string RuleText(const KonBiaoZu::RuleRecord& rule, int column)
    {
        switch (column)
        {
        case ColumnAnnotationType:
            return NormalizeTextEncoding(rule.annotationType);
        case ColumnSeries:
            return NormalizeTextEncoding(rule.series);
        case ColumnSize:
            return NormalizeTextEncoding(rule.size);
        case ColumnThreadSpec:
            return NormalizeTextEncoding(rule.threadSpec);
        case ColumnLengthText:
            return NormalizeTextEncoding(rule.lengthText);
        case ColumnBottomHole:
            return FormatDouble(rule.bottomHole);
        case ColumnDisplayText:
            return NormalizeTextEncoding(rule.displayText);
        case ColumnStandardComment:
            return NormalizeTextEncoding(rule.standardComment);
        case ColumnNote:
            return NormalizeTextEncoding(rule.note);
        default:
            return {};
        }
    }

    bool SetRuleText(KonBiaoZu::RuleRecord& rule, int column, const std::string& text)
    {
        const std::string normalizedText = NormalizeTextEncoding(text);
        switch (column)
        {
        case ColumnAnnotationType:
            rule.annotationType = Trim(normalizedText);
            return !rule.annotationType.empty();
        case ColumnSeries:
            rule.series = Trim(normalizedText);
            return true;
        case ColumnSize:
            rule.size = Trim(normalizedText);
            return true;
        case ColumnThreadSpec:
            rule.threadSpec = Trim(normalizedText);
            return true;
        case ColumnLengthText:
            rule.lengthText = Trim(normalizedText);
            return true;
        case ColumnBottomHole:
        {
            const std::string trimmed = Trim(normalizedText);
            if (trimmed.empty())
            {
                rule.bottomHole = 0.0;
                return true;
            }

            char* end = nullptr;
            const double value = std::strtod(trimmed.c_str(), &end);
            if (end == trimmed.c_str() || *end != '\0')
            {
                return false;
            }
            rule.bottomHole = value;
            return true;
        }
        case ColumnDisplayText:
            rule.displayText = Trim(normalizedText);
            return true;
        case ColumnStandardComment:
            rule.standardComment = Trim(normalizedText);
            return true;
        case ColumnNote:
            rule.note = Trim(normalizedText);
            return true;
        default:
            return false;
        }
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
            const char ch = line[i];
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

    bool ChooseExcelTableFile(bool saveDialog, std::wstring& path)
    {
        wchar_t fileName[MAX_PATH] = L"rules.csv";
        OPENFILENAMEW dialog {};
        dialog.lStructSize = sizeof(dialog);
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

    std::string DecodeTableText(const std::string& bytes)
    {
        if (bytes.size() >= 3 &&
            static_cast<unsigned char>(bytes[0]) == 0xEF &&
            static_cast<unsigned char>(bytes[1]) == 0xBB &&
            static_cast<unsigned char>(bytes[2]) == 0xBF)
        {
            return bytes.substr(3);
        }

        auto wideToUtf8 = [](const std::wstring& wide) {
            return WideToUtf8(wide);
        };

        if (bytes.size() >= 2 &&
            static_cast<unsigned char>(bytes[0]) == 0xFF &&
            static_cast<unsigned char>(bytes[1]) == 0xFE)
        {
            std::wstring wide;
            for (size_t i = 2; i + 1 < bytes.size(); i += 2)
            {
                const wchar_t ch = static_cast<wchar_t>(
                    static_cast<unsigned char>(bytes[i]) |
                    (static_cast<unsigned char>(bytes[i + 1]) << 8));
                wide.push_back(ch);
            }
            return wideToUtf8(wide);
        }

        if (bytes.size() >= 2 &&
            static_cast<unsigned char>(bytes[0]) == 0xFE &&
            static_cast<unsigned char>(bytes[1]) == 0xFF)
        {
            std::wstring wide;
            for (size_t i = 2; i + 1 < bytes.size(); i += 2)
            {
                const wchar_t ch = static_cast<wchar_t>(
                    (static_cast<unsigned char>(bytes[i]) << 8) |
                    static_cast<unsigned char>(bytes[i + 1]));
                wide.push_back(ch);
            }
            return wideToUtf8(wide);
        }

        const int strictWideSize = MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            bytes.data(),
            static_cast<int>(bytes.size()),
            nullptr,
            0);
        if (strictWideSize > 0)
        {
            std::wstring wide(static_cast<size_t>(strictWideSize), L'\0');
            MultiByteToWideChar(
                CP_UTF8,
                MB_ERR_INVALID_CHARS,
                bytes.data(),
                static_cast<int>(bytes.size()),
                wide.data(),
                strictWideSize);
            return wideToUtf8(wide);
        }

        const int ansiWideSize = MultiByteToWideChar(
            CP_ACP,
            0,
            bytes.data(),
            static_cast<int>(bytes.size()),
            nullptr,
            0);
        if (ansiWideSize <= 0)
        {
            return bytes;
        }

        std::wstring wide(static_cast<size_t>(ansiWideSize), L'\0');
        MultiByteToWideChar(
            CP_ACP,
            0,
            bytes.data(),
            static_cast<int>(bytes.size()),
            wide.data(),
            ansiWideSize);
        return wideToUtf8(wide);
    }

    class RulesEditorDialog
    {
    public:
        RulesEditorDialog(
            const std::string& configPath,
            std::vector<KonBiaoZu::RuleRecord> rules,
            std::vector<KonBiaoZu::RuleRecord>& liveRules,
            std::unordered_map<std::string, std::vector<KonBiaoZu::RuleRecord>>& liveRulesByType)
            : ui_(NXOpen::UI::GetUI()),
              dialog_(nullptr),
              tree_(nullptr),
              addButton_(nullptr),
              deleteButton_(nullptr),
              saveButton_(nullptr),
              prevPageButton_(nullptr),
              nextPageButton_(nullptr),
              sheetTypeEnum_(nullptr),
              treeGroup_(nullptr),
              configPath_(configPath),
              rules_(std::move(rules)),
              liveRules_(liveRules),
              liveRulesByType_(liveRulesByType),
              selectedIndex_(-1),
              activeSheetIndex_(0),
              newRuleNode_(nullptr),
              nextNodeOrder_(0),
              columnsInserted_(false),
              suppressToggleUpdate_(false),
              suppressSheetEnumUpdate_(false),
              dirty_(false)
        {
        }

        ~RulesEditorDialog()
        {
            if (dialog_ != nullptr)
            {
                delete dialog_;
                dialog_ = nullptr;
            }
        }

        bool Launch(std::string& errorMessage)
        {
            const std::string dlxPath = FindRulesEditorDlxPath();
            if (dlxPath.empty())
            {
                errorMessage = Utf8ToSystem("找不到规则表编辑器界面文件 KonBiaoZuRulesEditor.dlx");
                return false;
            }

            dialog_ = ui_->CreateDialog(dlxPath.c_str());
            dialog_->AddInitializeHandler(NXOpen::make_callback(this, &RulesEditorDialog::initialize_cb));
            dialog_->AddDialogShownHandler(NXOpen::make_callback(this, &RulesEditorDialog::dialogShown_cb));
            dialog_->AddUpdateHandler(NXOpen::make_callback(this, &RulesEditorDialog::update_cb));
            dialog_->AddApplyHandler(NXOpen::make_callback(this, &RulesEditorDialog::apply_cb));
            dialog_->AddOkHandler(NXOpen::make_callback(this, &RulesEditorDialog::ok_cb));
            dialog_->Launch();
            return true;
        }

        void initialize_cb()
        {
            tree_ = dynamic_cast<Tree*>(dialog_->TopBlock()->FindBlock("tree_control0"));
            saveButton_ = dynamic_cast<UIBlock*>(dialog_->TopBlock()->FindBlock("addCrossSelectionNodeButton"));
            addButton_ = dynamic_cast<UIBlock*>(dialog_->TopBlock()->FindBlock("addNodeButton"));
            deleteButton_ = dynamic_cast<UIBlock*>(dialog_->TopBlock()->FindBlock("deleteNodeButton"));
            prevPageButton_ = dynamic_cast<UIBlock*>(dialog_->TopBlock()->FindBlock("autoTapHoleToggle"));
            nextPageButton_ = dynamic_cast<UIBlock*>(dialog_->TopBlock()->FindBlock("autoPemHoleToggle"));
            sheetTypeEnum_ = dynamic_cast<Enumeration*>(dialog_->TopBlock()->FindBlock("sheetTypeEnum"));
            treeGroup_ = dynamic_cast<UIBlock*>(dialog_->TopBlock()->FindBlock("group0"));

            LocalizeBlocks();

            if (tree_ != nullptr)
            {
                tree_->SetOnSelectHandler(NXOpen::make_callback(this, &RulesEditorDialog::OnSelectCallback));
                tree_->SetOnBeginLabelEditHandler(NXOpen::make_callback(this, &RulesEditorDialog::OnBeginLabelEditCallback));
                tree_->SetOnEndLabelEditHandler(NXOpen::make_callback(this, &RulesEditorDialog::OnEndLabelEditCallback));
                tree_->SetAskEditControlHandler(NXOpen::make_callback(this, &RulesEditorDialog::AskEditControlCallback));
                tree_->SetOnEditOptionSelectedHandler(NXOpen::make_callback(this, &RulesEditorDialog::OnEditOptionSelectedCallback));
                tree_->SetColumnSortHandler(NXOpen::make_callback(this, &RulesEditorDialog::OnColumnSortCallback));
            }
        }

        void dialogShown_cb()
        {
            InsertColumns();
            RebuildTree();
        }

        int update_cb(UIBlock* block)
        {
            try
            {
                if (suppressToggleUpdate_)
                {
                    return 0;
                }
                if (block == sheetTypeEnum_ && suppressSheetEnumUpdate_)
                {
                    return 0;
                }

                if (block == addButton_)
                {
                    AddRule();
                }
                else if (block == deleteButton_)
                {
                    DeleteSelectedRule();
                }
                else if (block == prevPageButton_)
                {
                    ImportExcelTable();
                    ResetPagingToggle(prevPageButton_);
                }
                else if (block == nextPageButton_)
                {
                    ExportExcelTable();
                    ResetPagingToggle(nextPageButton_);
                }
                else if (block == sheetTypeEnum_)
                {
                    SwitchToSelectedSheetType();
                }
                else if (block == saveButton_)
                {
                    std::string error;
                    if (!Save(error))
                    {
                        ShowError(error);
                    }
                    else
                    {
                        ShowInfo("规则表已保存");
                    }
                }
            }
            catch (const std::exception& ex)
            {
                ShowError(ex.what());
            }
            catch (...)
            {
                ShowError("规则表操作失败");
            }
            return 0;
        }

        int apply_cb()
        {
            std::string error;
            if (!Save(error))
            {
                ShowError(error);
                return 1;
            }
            return 0;
        }

        int ok_cb()
        {
            std::string error;
            if (!Save(error))
            {
                ShowError(error);
                return 1;
            }
            return 0;
        }

        void OnSelectCallback(Tree*, Node* node, int, bool selected)
        {
            if (!selected)
            {
                return;
            }

            if (IsNewRuleNode(node))
            {
                selectedIndex_ = -1;
                return;
            }

            const auto it = nodeToRule_.find(node);
            selectedIndex_ = it == nodeToRule_.end() ? -1 : static_cast<int>(it->second);
            if (it != nodeToRule_.end() && it->second < rules_.size())
            {
                RefreshNodeText(node, rules_[it->second]);
            }
        }

        Tree::BeginLabelEditState OnBeginLabelEditCallback(Tree*, Node* node, int columnID)
        {
            DebugLog("BeginLabelEdit column=" + std::to_string(columnID) +
                     " isNew=" + std::to_string(IsNewRuleNode(node) ? 1 : 0));
            if (!IsNewRuleNode(node) && nodeToRule_.find(node) == nodeToRule_.end())
            {
                return Tree::BeginLabelEditStateDisallow;
            }
            return Tree::BeginLabelEditStateAllow;
        }

        Tree::EndLabelEditState OnEndLabelEditCallback(Tree*, Node* node, int columnID, NXString editedText)
        {
            const int targetColumnID = columnID == ColumnIndex ? ColumnSize : columnID;
            DebugLog("EndLabelEdit column=" + std::to_string(columnID) +
                     " target=" + std::to_string(targetColumnID) +
                     " mode=" + std::to_string(static_cast<int>(editedText.GetMode())) +
                     " textHex=" + BytesToHex(editedText.GetText()) +
                     " utf8Hex=" + BytesToHex(editedText.GetUTF8Text()) +
                     " localeHex=" + BytesToHex(editedText.GetLocaleText()) +
                     " value=" + NxStringToUtf8(editedText));
            if (IsNewRuleNode(node))
            {
                return AddRuleFromBlankRow(node, targetColumnID, NxStringToUtf8(editedText), false);
            }

            const auto it = nodeToRule_.find(node);
            if (it == nodeToRule_.end() || it->second >= rules_.size())
            {
                return Tree::EndLabelEditStateRejectText;
            }

            KonBiaoZu::RuleRecord& rule = rules_[it->second];
            const std::string text = NxStringToUtf8(editedText);
            if (!SetRuleText(rule, targetColumnID, text))
            {
                DebugLog("EndLabelEdit rejected by SetRuleText text=" + text);
                return Tree::EndLabelEditStateRejectText;
            }

            selectedIndex_ = static_cast<int>(it->second);
            dirty_ = true;
            if (targetColumnID == ColumnAnnotationType)
            {
                SetActiveSheetByName(rule.annotationType);
            }
            DebugLog("EndLabelEdit stored index=" + std::to_string(selectedIndex_) +
                     " series=" + rule.series +
                     " displayText=" + rule.displayText +
                     " comment=" + rule.standardComment);
            RefreshNodeText(node, rule);
            // The value is stored and the row is redrawn from normalized text; reject NX's edit buffer.
            return Tree::EndLabelEditStateRejectText;
        }

        Tree::ControlType AskEditControlCallback(Tree* tree, Node* node, int columnID)
        {
            return Tree::ControlTypeNone;
        }

        Tree::EditControlOption OnEditOptionSelectedCallback(
            Tree*,
            Node* node,
            int columnID,
            int selectedOptionID,
            NXString selectedOptionText,
            Tree::ControlType)
        {
            if (columnID != ColumnAnnotationType)
            {
                return Tree::EditControlOptionReject;
            }

            std::vector<std::string> typeNames = AnnotationTypeOptions(node);
            std::string value;
            if (selectedOptionID >= 0 && static_cast<size_t>(selectedOptionID) < typeNames.size())
            {
                value = typeNames[static_cast<size_t>(selectedOptionID)];
            }
            else
            {
                value = NxStringToUtf8(selectedOptionText);
            }

            if (Trim(value).empty())
            {
                return Tree::EditControlOptionReject;
            }

            if (IsNewRuleNode(node))
            {
                return AddRuleFromBlankRow(node, ColumnAnnotationType, value, true) == Tree::EndLabelEditStateAcceptText
                    ? Tree::EditControlOptionAccept
                    : Tree::EditControlOptionReject;
            }

            const auto it = nodeToRule_.find(node);
            if (it == nodeToRule_.end() || it->second >= rules_.size())
            {
                return Tree::EditControlOptionReject;
            }

            rules_[it->second].annotationType = Trim(value);
            SetNodeText(node, ColumnAnnotationType, rules_[it->second].annotationType);
            dirty_ = true;
            SetActiveSheetByName(rules_[it->second].annotationType);
            RebuildTree();
            return Tree::EditControlOptionAccept;
        }

        int OnColumnSortCallback(Tree*, int, Node* firstNode, Node* secondNode)
        {
            const auto first = nodeOrder_.find(firstNode);
            const auto second = nodeOrder_.find(secondNode);
            if (first == nodeOrder_.end() || second == nodeOrder_.end())
            {
                return 0;
            }

            if (first->second == second->second)
            {
                return 0;
            }

            return first->second < second->second ? -1 : 1;
        }

    private:
        void LocalizeBlocks()
        {
            UIBlock* selection = dynamic_cast<UIBlock*>(dialog_->TopBlock()->FindBlock("selection0"));
            UIBlock* nodeDataGroup = dynamic_cast<UIBlock*>(dialog_->TopBlock()->FindBlock("nodeDataGroup"));
            UIBlock* buttonGroup = dynamic_cast<UIBlock*>(dialog_->TopBlock()->FindBlock("addDeleteNodeGroup"));

            TrySetStringProperty(dialog_->TopBlock(), "Label", "规则表");
            TrySetStringProperty(dialog_->TopBlock(), "LabelString", "规则表");

            TrySetVisible(selection, false);
            TrySetVisible(prevPageButton_, true);
            TrySetVisible(nextPageButton_, true);
            TrySetVisible(sheetTypeEnum_, true);
            TrySetLogicalProperty(prevPageButton_, "Value", false);
            TrySetLogicalProperty(nextPageButton_, "Value", false);

            TrySetVisible(nodeDataGroup, true);
            TrySetStringProperty(nodeDataGroup, "Label", "规则操作");
            TrySetStringProperty(nodeDataGroup, "LabelString", "规则操作");

            TrySetVisible(treeGroup_, true);
            TrySetStringProperty(treeGroup_, "Label", "规则列表");
            TrySetStringProperty(treeGroup_, "LabelString", "规则列表");

            TrySetVisible(buttonGroup, true);
            TrySetStringProperty(buttonGroup, "Label", "规则操作");
            TrySetStringProperty(buttonGroup, "LabelString", "规则操作");

            TrySetStringProperty(saveButton_, "Label", "保存规则");
            TrySetStringProperty(saveButton_, "LabelString", "保存规则");
            TrySetStringProperty(addButton_, "Label", "新增规则");
            TrySetStringProperty(addButton_, "LabelString", "新增规则");
            TrySetStringProperty(deleteButton_, "Label", "删除规则");
            TrySetStringProperty(deleteButton_, "LabelString", "删除规则");
            TrySetStringProperty(prevPageButton_, "Label", "导入EXCEL表");
            TrySetStringProperty(prevPageButton_, "LabelString", "导入EXCEL表");
            TrySetStringProperty(nextPageButton_, "Label", "导出EXCEL表");
            TrySetStringProperty(nextPageButton_, "LabelString", "导出EXCEL表");
        }

        void InsertColumns()
        {
            if (tree_ == nullptr || columnsInserted_)
            {
                return;
            }

            tree_->InsertColumn(ColumnIndex, Utf8NxString("类别/规格"), 150);
            tree_->InsertColumn(ColumnAnnotationType, Utf8NxString("类别"), 110);
            tree_->InsertColumn(ColumnSeries, Utf8NxString("子类型"), 95);
            tree_->InsertColumn(ColumnSize, Utf8NxString("规格"), 75);
            tree_->InsertColumn(ColumnLengthText, Utf8NxString("长度"), 70);
            tree_->InsertColumn(ColumnBottomHole, Utf8NxString("底孔"), 70);
            tree_->InsertColumn(ColumnDisplayText, Utf8NxString("标注文本"), 150);
            tree_->InsertColumn(ColumnStandardComment, Utf8NxString("说明"), 150);

            tree_->SetColumnResizePolicy(ColumnIndex, Tree::ColumnResizePolicyConstantWidth);
            tree_->SetColumnResizePolicy(ColumnAnnotationType, Tree::ColumnResizePolicyConstantWidth);
            tree_->SetColumnResizePolicy(ColumnSeries, Tree::ColumnResizePolicyConstantWidth);
            tree_->SetColumnResizePolicy(ColumnSize, Tree::ColumnResizePolicyConstantWidth);
            tree_->SetColumnResizePolicy(ColumnLengthText, Tree::ColumnResizePolicyConstantWidth);
            tree_->SetColumnResizePolicy(ColumnBottomHole, Tree::ColumnResizePolicyConstantWidth);
            tree_->SetColumnResizePolicy(ColumnDisplayText, Tree::ColumnResizePolicyResizeWithTree);
            tree_->SetColumnResizePolicy(ColumnStandardComment, Tree::ColumnResizePolicyConstantWidth);
            tree_->SetColumnVisible(ColumnAnnotationType, false);
            tree_->SetShowExpandCollapseMarker(true);

            tree_->SetSortRootNodes(false);
            for (const int columnID : {
                     ColumnIndex,
                     ColumnAnnotationType,
                     ColumnSeries,
                     ColumnSize,
                     ColumnLengthText,
                     ColumnBottomHole,
                     ColumnDisplayText,
                     ColumnStandardComment})
            {
                tree_->SetColumnSortable(columnID, false);
            tree_->SetColumnSortOption(columnID, Tree::ColumnSortOptionUnsorted);
            }
            columnsInserted_ = true;
        }

        void RefreshNodeText(Node* node, const KonBiaoZu::RuleRecord& rule)
        {
            if (node == nullptr)
            {
                return;
            }

            DebugLog("RefreshNodeText size=" + rule.size +
                     " series=" + NormalizeTextEncoding(rule.series) +
                     " displayText=" + NormalizeTextEncoding(rule.displayText) +
                     " comment=" + NormalizeTextEncoding(rule.standardComment));
            SetNodeText(node, ColumnIndex, rule.size);
            SetNodeText(node, ColumnAnnotationType, rule.annotationType);
            SetNodeText(node, ColumnSeries, rule.series);
            SetNodeText(node, ColumnSize, rule.size);
            SetNodeText(node, ColumnLengthText, rule.lengthText);
            SetNodeText(node, ColumnBottomHole, FormatDouble(rule.bottomHole));
            SetNodeText(node, ColumnDisplayText, rule.displayText);
            SetNodeText(node, ColumnStandardComment, rule.standardComment);
        }

        void RebuildTree()
        {
            if (tree_ == nullptr)
            {
                return;
            }

            DebugLog("RebuildTree begin rules=" + std::to_string(rules_.size()) +
                     " activeIndex=" + std::to_string(activeSheetIndex_));
            RebuildSheetNames();
            ClampActiveSheet();
            UpdateSheetTypeEnum();

            int deletedRootCount = 0;
            tree_->Redraw(false);
            for (int guard = 0; guard < 1000; ++guard)
            {
                Node* rootNode = nullptr;
                try
                {
                    rootNode = tree_->RootNode();
                }
                catch (...)
                {
                    rootNode = nullptr;
                }
                if (rootNode == nullptr)
                {
                    break;
                }

                try
                {
                    tree_->DeleteNode(rootNode);
                    ++deletedRootCount;
                }
                catch (...)
                {
                    DebugLog("RebuildTree DeleteNode(root) threw at count=" + std::to_string(deletedRootCount));
                    break;
                }
            }
            DebugLog("RebuildTree deletedRoots=" + std::to_string(deletedRootCount));
            nodeToRule_.clear();
            nodeOrder_.clear();
            newRuleNode_ = nullptr;
            nextNodeOrder_ = 0;

            if (selectedIndex_ >= static_cast<int>(rules_.size()))
            {
                selectedIndex_ = static_cast<int>(rules_.empty() ? -1 : rules_.size() - 1);
            }

            Node* selectedNode = nullptr;
            const std::string sheetName = ActiveSheetName();
            Node* parentNode = tree_->CreateNode(Utf8NxString(sheetName));
            nodeOrder_[parentNode] = nextNodeOrder_++;
            tree_->InsertNode(parentNode, nullptr, nullptr, Tree::NodeInsertOptionLast);
            SetNodeText(parentNode, ColumnIndex, sheetName);
            parentNode->Expand(Node::ExpandOptionExpand);

            size_t sheetRowNumber = 1;
            for (size_t i = 0; i < rules_.size(); ++i)
            {
                if (rules_[i].annotationType != sheetName)
                {
                    continue;
                }

                const std::string rowText = rules_[i].size.empty() ? std::to_string(sheetRowNumber) : rules_[i].size;
                ++sheetRowNumber;
                Node* node = tree_->CreateNode(Utf8NxString(rowText));
                InsertNodeInDisplayOrder(node, parentNode);
                RefreshNodeText(node, rules_[i]);
                nodeToRule_[node] = i;

                if (static_cast<int>(i) == selectedIndex_)
                {
                    selectedNode = node;
                }
            }

            if (selectedNode != nullptr)
            {
                tree_->SelectNode(selectedNode, true, true);
            }

            UpdateSheetLabel();
            tree_->Redraw(true);
            DebugLog("RebuildTree end sheet=" + sheetName +
                     " visibleRows=" + std::to_string(sheetRowNumber - 1));
        }

        void InsertNodeInDisplayOrder(Node* node, Node* parentNode)
        {
            if (tree_ == nullptr || node == nullptr)
            {
                return;
            }

            nodeOrder_[node] = nextNodeOrder_++;
            tree_->InsertNode(node, parentNode, nullptr, Tree::NodeInsertOptionLast);
        }

        bool IsNewRuleNode(Node* node) const
        {
            return node != nullptr && node == newRuleNode_;
        }

        void InsertBlankRuleRow(size_t rowNumber, Node* parentNode)
        {
            if (tree_ == nullptr)
            {
                return;
            }

            (void)rowNumber;
            newRuleNode_ = tree_->CreateNode(Utf8NxString("<新增规格>"));
            InsertNodeInDisplayOrder(newRuleNode_, parentNode);
            SetNodeText(newRuleNode_, ColumnIndex, "<新增规格>");
        }

        Tree::EndLabelEditState AddRuleFromBlankRow(Node* node, int columnID, const std::string& editedText, bool allowAutomaticTextCommit)
        {
            if (node == nullptr || columnID == ColumnIndex)
            {
                return Tree::EndLabelEditStateRejectText;
            }

            if (Trim(editedText).empty())
            {
                return Tree::EndLabelEditStateRejectText;
            }

            KonBiaoZu::RuleRecord rule;
            rule.annotationType = ActiveSheetName();
            if (!SetRuleText(rule, columnID, editedText))
            {
                return Tree::EndLabelEditStateRejectText;
            }

            if (rule.annotationType.empty())
            {
                rule.annotationType = ActiveSheetName();
            }

            rules_.push_back(rule);
            selectedIndex_ = static_cast<int>(rules_.size() - 1);
            dirty_ = true;
            RebuildTree();
            UpdateSheetLabel();
            UpdateSheetTypeEnum();
            // Label edits are committed manually above; option-menu edits still need NX's normal accept path.
            return allowAutomaticTextCommit ? Tree::EndLabelEditStateAcceptText : Tree::EndLabelEditStateRejectText;
        }

        void AddRule()
        {
            RefreshSelectedIndexFromTree();

            KonBiaoZu::RuleRecord rule = BuildNextRuleForActiveSheet();
            rules_.push_back(rule);
            selectedIndex_ = static_cast<int>(rules_.size() - 1);
            dirty_ = true;
            DebugLog("AddRule sheet=" + rule.annotationType +
                     " series=" + rule.series +
                     " size=" + rule.size +
                     " threadSpec=" + rule.threadSpec);
            RebuildTree();
        }

        KonBiaoZu::RuleRecord BuildNextRuleForActiveSheet() const
        {
            const std::string sheetName = ActiveSheetName();
            std::string preferredSeries;
            if (selectedIndex_ >= 0 && static_cast<size_t>(selectedIndex_) < rules_.size() &&
                rules_[static_cast<size_t>(selectedIndex_)].annotationType == sheetName)
            {
                preferredSeries = rules_[static_cast<size_t>(selectedIndex_)].series;
            }

            int templateIndex = -1;
            const auto chooseCandidate = [&](bool requirePreferredSeries) {
                int chosen = -1;
                for (size_t i = 0; i < rules_.size(); ++i)
                {
                    const KonBiaoZu::RuleRecord& candidate = rules_[i];
                    if (candidate.annotationType != sheetName)
                    {
                        continue;
                    }
                    if (requirePreferredSeries && candidate.series != preferredSeries)
                    {
                        continue;
                    }
                    if (HasRuleContent(candidate))
                    {
                        chosen = static_cast<int>(i);
                    }
                }
                return chosen;
            };

            if (!preferredSeries.empty())
            {
                templateIndex = chooseCandidate(true);
            }
            if (templateIndex < 0)
            {
                templateIndex = chooseCandidate(false);
            }

            KonBiaoZu::RuleRecord rule;
            if (templateIndex >= 0)
            {
                rule = rules_[static_cast<size_t>(templateIndex)];
            }
            else
            {
                rule.annotationType = sheetName.empty() ? "螺牙标注" : sheetName;
                rule.series = preferredSeries;
                rule.size = "M1";
                rule.threadSpec = "M1";
                rule.displayText = "{规格}";
            }

            rule.annotationType = sheetName;
            const std::string oldSize = rule.size;
            rule.size = NextSizeForSeries(rule);
            if (!oldSize.empty() && oldSize != rule.size)
            {
                rule.threadSpec = ReplaceAllCopy(rule.threadSpec, oldSize, rule.size);
                rule.displayText = ReplaceAllCopy(rule.displayText, oldSize, rule.size);
                rule.standardComment = ReplaceAllCopy(rule.standardComment, oldSize, rule.size);
                rule.note = ReplaceAllCopy(rule.note, oldSize, rule.size);
            }
            else
            {
                rule.threadSpec = IncrementTextNumber(rule.threadSpec);
            }
            return rule;
        }

        std::string NextSizeForSeries(const KonBiaoZu::RuleRecord& templateRule) const
        {
            const std::optional<NumberToken> templateToken = FindLastNumberToken(templateRule.size);
            if (!templateToken)
            {
                return templateRule.size.empty() ? "规格1" : templateRule.size + "_1";
            }

            const std::string prefix = templateRule.size.substr(0, templateToken->begin);
            double maxValue = templateToken->value;
            int decimals = templateToken->decimals;
            for (const KonBiaoZu::RuleRecord& candidate : rules_)
            {
                if (candidate.annotationType != templateRule.annotationType ||
                    candidate.series != templateRule.series)
                {
                    continue;
                }

                const std::optional<NumberToken> candidateToken = FindLastNumberToken(candidate.size);
                if (!candidateToken)
                {
                    continue;
                }
                if (candidate.size.substr(0, candidateToken->begin) != prefix)
                {
                    continue;
                }
                if (candidateToken->value > maxValue)
                {
                    maxValue = candidateToken->value;
                    decimals = candidateToken->decimals;
                }
            }

            return prefix + FormatIncrementedNumber(maxValue + 1.0, decimals);
        }

        std::string IncrementTextNumber(const std::string& text) const
        {
            const std::optional<NumberToken> token = FindLastNumberToken(text);
            if (!token)
            {
                return text;
            }
            return ReplaceNumberToken(text, *token, token->value + 1.0);
        }

        void DeleteSelectedRule()
        {
            RefreshSelectedIndexFromTree();

            if (selectedIndex_ < 0 || static_cast<size_t>(selectedIndex_) >= rules_.size())
            {
                return;
            }

            rules_.erase(rules_.begin() + selectedIndex_);
            if (rules_.empty())
            {
                selectedIndex_ = -1;
            }
            else if (selectedIndex_ >= static_cast<int>(rules_.size()))
            {
                selectedIndex_ = static_cast<int>(rules_.size() - 1);
            }

            dirty_ = true;
            RebuildTree();
        }

        void RebuildSheetNames()
        {
            const std::string current = ActiveSheetName();
            sheetNames_.clear();
            std::unordered_set<std::string> seen;

            const auto pushSheet = [this, &seen](const std::string& name) {
                const std::string trimmed = Trim(name);
                if (!trimmed.empty() && seen.insert(trimmed).second)
                {
                    sheetNames_.push_back(trimmed);
                }
            };

            for (const KonBiaoZu::RuleRecord& rule : rules_)
            {
                pushSheet(rule.annotationType);
            }

            if (sheetNames_.empty())
            {
                sheetNames_.push_back("螺牙标注");
            }

            if (!current.empty())
            {
                for (size_t i = 0; i < sheetNames_.size(); ++i)
                {
                    if (sheetNames_[i] == current)
                    {
                        activeSheetIndex_ = i;
                        return;
                    }
                }
            }
        }

        std::string ActiveSheetName() const
        {
            if (sheetNames_.empty())
            {
                return "螺牙标注";
            }

            const size_t index = activeSheetIndex_ < sheetNames_.size() ? activeSheetIndex_ : 0;
            return sheetNames_[index];
        }

        void SetActiveSheetByName(const std::string& name)
        {
            const std::string trimmed = Trim(name);
            if (trimmed.empty())
            {
                return;
            }

            RebuildSheetNames();
            for (size_t i = 0; i < sheetNames_.size(); ++i)
            {
                if (sheetNames_[i] == trimmed)
                {
                    activeSheetIndex_ = i;
                    return;
                }
            }

            sheetNames_.push_back(trimmed);
            activeSheetIndex_ = sheetNames_.size() - 1;
        }

        size_t ActiveSheetRuleCount() const
        {
            const std::string sheetName = ActiveSheetName();
            size_t count = 0;
            for (const KonBiaoZu::RuleRecord& rule : rules_)
            {
                if (rule.annotationType == sheetName)
                {
                    ++count;
                }
            }
            return count;
        }

        void ClampActiveSheet()
        {
            if (sheetNames_.empty())
            {
                sheetNames_.push_back("螺牙标注");
            }

            if (activeSheetIndex_ >= sheetNames_.size())
            {
                activeSheetIndex_ = sheetNames_.size() - 1;
            }
        }

        void GoToPreviousSheet()
        {
            RefreshSelectedIndexFromTree();
            RebuildSheetNames();
            if (activeSheetIndex_ == 0)
            {
                return;
            }
            --activeSheetIndex_;
            SelectFirstRuleOnActiveSheet();
            RebuildTree();
        }

        void GoToNextSheet()
        {
            RefreshSelectedIndexFromTree();
            RebuildSheetNames();
            if (activeSheetIndex_ + 1 >= sheetNames_.size())
            {
                return;
            }

            ++activeSheetIndex_;
            SelectFirstRuleOnActiveSheet();
            RebuildTree();
        }

        void SelectFirstRuleOnActiveSheet()
        {
            const std::string sheetName = ActiveSheetName();
            selectedIndex_ = -1;
            for (size_t i = 0; i < rules_.size(); ++i)
            {
                if (rules_[i].annotationType == sheetName)
                {
                    selectedIndex_ = static_cast<int>(i);
                    return;
                }
            }
        }

        void ResetPagingToggle(UIBlock* block)
        {
            suppressToggleUpdate_ = true;
            TrySetLogicalProperty(block, "Value", false);
            suppressToggleUpdate_ = false;
        }

        void UpdateSheetTypeEnum()
        {
            if (sheetTypeEnum_ == nullptr)
            {
                return;
            }

            std::vector<NXString> items;
            items.reserve(sheetNames_.size());
            for (const std::string& sheetName : sheetNames_)
            {
                items.push_back(Utf8NxString(sheetName));
            }

            if (items.empty())
            {
                items.push_back(Utf8NxString(ActiveSheetName()));
            }

            suppressSheetEnumUpdate_ = true;
            try
            {
                sheetTypeEnum_->SetEnumMembers(items);
                sheetTypeEnum_->SetValueAsString(Utf8NxString(ActiveSheetName()));
            }
            catch (...)
            {
            }
            suppressSheetEnumUpdate_ = false;
        }

        void SwitchToSelectedSheetType()
        {
            if (sheetTypeEnum_ == nullptr)
            {
                return;
            }

            const std::string selectedType = Trim(NxStringToUtf8(sheetTypeEnum_->ValueAsString()));
            if (selectedType.empty() || selectedType == ActiveSheetName())
            {
                return;
            }

            SetActiveSheetByName(selectedType);
            SelectFirstRuleOnActiveSheet();
            RebuildTree();
        }

        void UpdateSheetLabel()
        {
            ClampActiveSheet();

            std::ostringstream label;
            label << "规则列表  工作表：" << ActiveSheetName()
                  << "（" << (activeSheetIndex_ + 1) << "/" << sheetNames_.size()
                  << "，" << ActiveSheetRuleCount() << "条）";

            const std::string text = label.str();
            TrySetStringProperty(treeGroup_, "Label", text.c_str());
            TrySetStringProperty(treeGroup_, "LabelString", text.c_str());
        }

        void ExportExcelTable()
        {
            std::wstring path;
            if (!ChooseExcelTableFile(true, path))
            {
                return;
            }

            std::ofstream output(std::filesystem::path(path), std::ios::binary | std::ios::trunc);
            if (!output.is_open())
            {
                ShowError("无法写入导出表格: " + WideToUtf8(path));
                return;
            }

            output << "\xEF\xBB\xBF";
            output << "类别,子类型,规格,螺纹,长度,底孔,标注文本,说明,备注\n";
            for (const KonBiaoZu::RuleRecord& rule : rules_)
            {
                if (!HasRuleContent(rule))
                {
                    continue;
                }

                output << EscapeCsv(NormalizeTextEncoding(rule.annotationType)) << ','
                       << EscapeCsv(NormalizeTextEncoding(rule.series)) << ','
                       << EscapeCsv(NormalizeTextEncoding(rule.size)) << ','
                       << EscapeCsv(NormalizeTextEncoding(rule.threadSpec)) << ','
                       << EscapeCsv(NormalizeTextEncoding(rule.lengthText)) << ','
                       << std::fixed << std::setprecision(2) << rule.bottomHole << ','
                       << EscapeCsv(NormalizeTextEncoding(rule.displayText)) << ','
                       << EscapeCsv(NormalizeTextEncoding(rule.standardComment)) << ','
                       << EscapeCsv(NormalizeTextEncoding(rule.note)) << "\n";
            }

            ShowInfo("导出EXCEL表完成");
        }

        void ImportExcelTable()
        {
            std::wstring path;
            if (!ChooseExcelTableFile(false, path))
            {
                return;
            }

            std::ifstream input(std::filesystem::path(path), std::ios::binary);
            if (!input.is_open())
            {
                ShowError("无法打开导入表格: " + WideToUtf8(path));
                return;
            }

            std::ostringstream buffer;
            buffer << input.rdbuf();
            std::string text = DecodeTableText(buffer.str());

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
                    if (!values.empty() && (Trim(values[0]) == "类别" || Trim(values[0]) == "type"))
                    {
                        continue;
                    }
                }
                if (values.size() < 6)
                {
                    ShowError("导入表格列数不足: " + WideToUtf8(path));
                    return;
                }

                KonBiaoZu::RuleRecord rule;
                rule.annotationType = NormalizeTextEncoding(Trim(values[0]));
                rule.series = values.size() > 1 ? NormalizeTextEncoding(Trim(values[1])) : std::string();
                rule.size = values.size() > 2 ? NormalizeTextEncoding(Trim(values[2])) : std::string();
                rule.threadSpec = values.size() > 3 ? NormalizeTextEncoding(Trim(values[3])) : std::string();
                rule.lengthText = values.size() > 4 ? NormalizeTextEncoding(Trim(values[4])) : std::string();
                rule.bottomHole = std::atof(values[5].c_str());
                rule.displayText = values.size() > 6 ? NormalizeTextEncoding(Trim(values[6])) : std::string();
                rule.standardComment = values.size() > 7 ? NormalizeTextEncoding(Trim(values[7])) : std::string();
                rule.note = values.size() > 8 ? NormalizeTextEncoding(Trim(values[8])) : std::string();
                if (rule.annotationType.empty())
                {
                    rule.annotationType = ActiveSheetName();
                }
                if (HasRuleContent(rule))
                {
                    imported.push_back(rule);
                }
            }

            if (imported.empty())
            {
                ShowError("导入表格没有有效规则: " + WideToUtf8(path));
                return;
            }

            rules_ = std::move(imported);
            selectedIndex_ = -1;
            activeSheetIndex_ = 0;
            RebuildSheetNames();
            RebuildTree();
            dirty_ = true;
            ShowInfo("导入EXCEL表完成，请点击保存规则写入 INI");
        }

        void RefreshSelectedIndexFromTree()
        {
            if (tree_ == nullptr)
            {
                return;
            }

            Node* selectedNode = nullptr;
            try
            {
                selectedNode = tree_->FirstSelectedNode();
            }
            catch (...)
            {
                selectedNode = nullptr;
            }

            if (IsNewRuleNode(selectedNode))
            {
                selectedIndex_ = -1;
                return;
            }

            const auto it = nodeToRule_.find(selectedNode);
            if (it != nodeToRule_.end() && it->second < rules_.size())
            {
                selectedIndex_ = static_cast<int>(it->second);
            }
            else
            {
                selectedIndex_ = -1;
            }
        }

        bool Save(std::string& errorMessage)
        {
            const std::filesystem::path configPath(configPath_);
            const std::filesystem::path parentPath = configPath.parent_path();
            if (!parentPath.empty())
            {
                std::error_code createError;
                std::filesystem::create_directories(parentPath, createError);
                if (createError)
                {
                    errorMessage = Utf8ToSystem("无法创建规则表目录: " + parentPath.string());
                    return false;
                }
            }

            std::ofstream output(configPath_, std::ios::binary | std::ios::trunc);
            if (!output.is_open())
            {
                errorMessage = Utf8ToSystem("无法写入规则表: " + configPath_);
                return false;
            }

            output << "; KonBiaoZu rule table, UTF-8\n";
            output << "version=1\n\n";
            size_t writtenCount = 0;
            for (size_t index = 0; index < rules_.size(); ++index)
            {
                const KonBiaoZu::RuleRecord& rule = rules_[index];
                if (!HasRuleContent(rule))
                {
                    continue;
                }

                ++writtenCount;
                WriteIniRule(output, writtenCount, rule);
            }

            if (!output.good())
            {
                errorMessage = Utf8ToSystem("规则表保存失败: " + configPath_);
                return false;
            }

            std::vector<KonBiaoZu::RuleRecord> cleanedRules;
            cleanedRules.reserve(rules_.size());
            for (const KonBiaoZu::RuleRecord& rule : rules_)
            {
                if (HasRuleContent(rule))
                {
                    cleanedRules.push_back(rule);
                }
            }

            rules_ = cleanedRules;
            liveRules_ = cleanedRules;
            ApplyTemplates(liveRules_);
            RebuildRuleIndex(liveRules_, liveRulesByType_);
            dirty_ = false;
            return true;
        }

        std::vector<std::string> AnnotationTypeOptions(Node* node) const
        {
            std::vector<std::string> typeNames;
            std::unordered_set<std::string> seen;

            const auto pushType = [&typeNames, &seen](const std::string& typeName) {
                const std::string trimmed = Trim(typeName);
                if (!trimmed.empty() && seen.insert(trimmed).second)
                {
                    typeNames.push_back(trimmed);
                }
            };

            if (IsNewRuleNode(node))
            {
                pushType(ActiveSheetName());
            }

            const auto it = nodeToRule_.find(node);
            if (it != nodeToRule_.end() && it->second < rules_.size())
            {
                pushType(rules_[it->second].annotationType);
            }

            for (const KonBiaoZu::RuleRecord& rule : rules_)
            {
                pushType(rule.annotationType);
            }

            if (typeNames.empty())
            {
                typeNames.push_back("螺牙标注");
            }

            return typeNames;
        }

        int DefaultAnnotationTypeIndex(Node* node, const std::vector<std::string>& typeNames) const
        {
            if (IsNewRuleNode(node))
            {
                const std::string current = ActiveSheetName();
                for (size_t i = 0; i < typeNames.size(); ++i)
                {
                    if (typeNames[i] == current)
                    {
                        return static_cast<int>(i);
                    }
                }
                return 0;
            }

            const auto it = nodeToRule_.find(node);
            if (it == nodeToRule_.end() || it->second >= rules_.size())
            {
                return 0;
            }

            const std::string& current = rules_[it->second].annotationType;
            for (size_t i = 0; i < typeNames.size(); ++i)
            {
                if (typeNames[i] == current)
                {
                    return static_cast<int>(i);
                }
            }
            return 0;
        }

        void ShowError(const std::string& text)
        {
            ui_->NXMessageBox()->Show(Utf8ToSystem("规则表").c_str(), NXOpen::NXMessageBox::DialogTypeError, Utf8ToSystem(text));
        }

        void ShowInfo(const std::string& text)
        {
            ui_->NXMessageBox()->Show(Utf8ToSystem("规则表").c_str(), NXOpen::NXMessageBox::DialogTypeInformation, Utf8ToSystem(text));
        }

        NXOpen::UI* ui_;
        BlockDialog* dialog_;
        Tree* tree_;
        UIBlock* addButton_;
        UIBlock* deleteButton_;
        UIBlock* saveButton_;
        UIBlock* prevPageButton_;
        UIBlock* nextPageButton_;
        Enumeration* sheetTypeEnum_;
        UIBlock* treeGroup_;
        std::string configPath_;
        std::vector<KonBiaoZu::RuleRecord> rules_;
        std::vector<std::string> sheetNames_;
        std::vector<KonBiaoZu::RuleRecord>& liveRules_;
        std::unordered_map<std::string, std::vector<KonBiaoZu::RuleRecord>>& liveRulesByType_;
        std::unordered_map<Node*, size_t> nodeToRule_;
        std::unordered_map<Node*, size_t> nodeOrder_;
        int selectedIndex_;
        size_t activeSheetIndex_;
        Node* newRuleNode_;
        size_t nextNodeOrder_;
        bool columnsInserted_;
        bool suppressToggleUpdate_;
        bool suppressSheetEnumUpdate_;
        bool dirty_;
    };
}

namespace KonBiaoZu
{
    bool ShowNxRulesEditor(
        const std::string& configPath,
        std::vector<RuleRecord> rules,
        std::vector<RuleRecord>& liveRules,
        std::unordered_map<std::string, std::vector<RuleRecord>>& liveRulesByType,
        std::string& errorMessage)
    {
        try
        {
            RulesEditorDialog dialog(configPath, std::move(rules), liveRules, liveRulesByType);
            return dialog.Launch(errorMessage);
        }
        catch (const std::exception& ex)
        {
            errorMessage = Utf8ToSystem(ex.what());
            return false;
        }
        catch (...)
        {
            errorMessage = Utf8ToSystem("规则表编辑器启动失败");
            return false;
        }
    }
}
