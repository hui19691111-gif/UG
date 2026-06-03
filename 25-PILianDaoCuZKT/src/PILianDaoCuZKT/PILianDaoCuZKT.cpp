//==============================================================================
// PILianDaoCuZKT - batch flat-pattern drawing sheet exporter.
//==============================================================================

#include "PILianDaoCuZKTDialog.hpp"

#include <NXOpen/Annotations_AnnotationManager.hxx>
#include <NXOpen/Annotations_DimensionCollection.hxx>
#include <NXOpen/Annotations_DimensionMeasurementBuilder.hxx>
#include <NXOpen/Annotations_DimensionStyleBuilder.hxx>
#include <NXOpen/Annotations_DraftingNoteBuilder.hxx>
#include <NXOpen/Annotations_LetteringStyleBuilder.hxx>
#include <NXOpen/Annotations_LineArrowStyleBuilder.hxx>
#include <NXOpen/Annotations_Note.hxx>
#include <NXOpen/Annotations_OriginBuilder.hxx>
#include <NXOpen/Annotations_PlaneBuilder.hxx>
#include <NXOpen/Annotations_RapidDimensionBuilder.hxx>
#include <NXOpen/Annotations_SimpleDraftingAid.hxx>
#include <NXOpen/Annotations_StyleBuilder.hxx>
#include <NXOpen/Annotations_TextWithEditControlsBuilder.hxx>
#include <NXOpen/Annotations_TextWithSymbolsBuilder.hxx>
#include <NXOpen/Assemblies_Component.hxx>
#include <NXOpen/Assemblies_ComponentAssembly.hxx>
#include <NXOpen/BasePart.hxx>
#include <NXOpen/Body.hxx>
#include <NXOpen/BodyCollection.hxx>
#include <NXOpen/Builder.hxx>
#include <NXOpen/ColorManager.hxx>
#include <NXOpen/Direction.hxx>
#include <NXOpen/DirectionCollection.hxx>
#include <NXOpen/DisplayableObject.hxx>
#include <NXOpen/Drafting_BaseEditSettingsBuilder.hxx>
#include <NXOpen/Drafting_PreferencesBuilder.hxx>
#include <NXOpen/Drafting_SettingsManager.hxx>
#include <NXOpen/Drawings_BaseView.hxx>
#include <NXOpen/Drawings_BaseViewBuilder.hxx>
#include <NXOpen/Drawings_DraftingComponentSelectionBuilder.hxx>
#include <NXOpen/Drawings_DraftingBody.hxx>
#include <NXOpen/Drawings_DraftingBodyCollection.hxx>
#include <NXOpen/Drawings_DraftingCurve.hxx>
#include <NXOpen/Drawings_DraftingCurveCollection.hxx>
#include <NXOpen/Drawings_DraftingDrawingSheet.hxx>
#include <NXOpen/Drawings_DraftingDrawingSheetBuilder.hxx>
#include <NXOpen/Drawings_DraftingDrawingSheetCollection.hxx>
#include <NXOpen/Drawings_DraftingView.hxx>
#include <NXOpen/Drawings_DraftingViewCollection.hxx>
#include <NXOpen/Drawings_DrawingSheet.hxx>
#include <NXOpen/Drawings_DrawingSheetCollection.hxx>
#include <NXOpen/Drawings_EditViewSettingsBuilder.hxx>
#include <NXOpen/Drawings_SelectModelViewBuilder.hxx>
#include <NXOpen/Drawings_ViewPlacementBuilder.hxx>
#include <NXOpen/Drawings_ViewScaleBuilder.hxx>
#include <NXOpen/Drawings_ViewStyleBaseBuilder.hxx>
#include <NXOpen/Drawings_ViewStyleBuilder.hxx>
#include <NXOpen/Drawings_ViewStyleFPCalloutConfigBuilder.hxx>
#include <NXOpen/Drawings_ViewStyleFPCalloutsBuilder.hxx>
#include <NXOpen/Drawings_ViewStyleFPCurvesBuilder.hxx>
#include <NXOpen/Edge.hxx>
#include <NXOpen/Expression.hxx>
#include <NXOpen/ExpressionCollection.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/Features_Feature.hxx>
#include <NXOpen/Features_FeatureCollection.hxx>
#include <NXOpen/Features_FlatPattern.hxx>
#include <NXOpen/Features_SheetMetal_FlatPatternBuilder.hxx>
#include <NXOpen/Features_SheetMetal_SheetmetalManager.hxx>
#include <NXOpen/ListingWindow.hxx>
#include <NXOpen/ModelingView.hxx>
#include <NXOpen/ModelingViewCollection.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/Preferences_PartDrafting.hxx>
#include <NXOpen/Preferences_PartPreferences.hxx>
#include <NXOpen/DraftingManager.hxx>
#include <NXOpen/SelectEdge.hxx>
#include <NXOpen/SelectFace.hxx>
#include <NXOpen/SelectDisplayableObject.hxx>
#include <NXOpen/SelectNXObject.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/SheetMetal_FlatPatternSettings.hxx>
#include <NXOpen/Unit.hxx>
#include <NXOpen/UnitCollection.hxx>
#include <NXOpen/View.hxx>
#include <NXOpen/ViewCollection.hxx>

#include <uf.h>
#include <uf_curve.h>
#include <uf_drf.h>
#include <uf_draw.h>
#include <uf_eval.h>
#include <uf_modl.h>
#include <uf_obj.h>
#include <uf_part.h>
#include <uf_view.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <io.h>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace NXOpen;
using namespace NXOpen::BlockStyler;

extern "C" __declspec(dllimport) int __stdcall MultiByteToWideChar(
    unsigned int CodePage,
    unsigned long dwFlags,
    const char* lpMultiByteStr,
    int cbMultiByte,
    wchar_t* lpWideCharStr,
    int cchWideChar);
extern "C" __declspec(dllimport) int __stdcall WideCharToMultiByte(
    unsigned int CodePage,
    unsigned long dwFlags,
    const wchar_t* lpWideCharStr,
    int cchWideChar,
    char* lpMultiByteStr,
    int cbMultiByte,
    const char* lpDefaultChar,
    int* lpUsedDefaultChar);

Session* PILianDaoCuZKTDialog::theSession = NULL;
UI* PILianDaoCuZKTDialog::theUI = NULL;

namespace
{
static const unsigned int kCodePageAcp = 0;
static const unsigned int kCodePageUtf8 = 65001;
static const char* kPluginSheetNamePrefix = "PILianDaoCuZKT";
static const char* kDefaultBodyNoteFormat = "{\xE7\xBC\x96\xE5\x8F\xB7=}{\xE6\x9D\x90\xE6\x96\x99} T={\xE5\x8E\x9A\xE5\xBA\xA6} {\xE6\x95\xB0\xE9\x87\x8F}PCS{\xE9\x95\x9C\xE5\x83\x8F}";
static const double kSheetMargin = 40.0;
static const double kDimensionPlacementGap = 7.0;
struct CommandOptions
{
    bool categoryLayout;
    bool annotateMaxDimension;
    bool showBendLines;
    double sheetHeight;
    double sheetWidth;
    double viewSpacing;
    double rowSpacing;
    double noteTextSize;
    double viewScaleDenominator;
};

struct FlatPatternCreateFailure
{
    std::string partKey;
    std::string viewName;
};

struct DrawingRunSummary
{
    size_t totalCount;
    size_t createdCount;
    size_t sheetCount;
    std::vector<FlatPatternCreateFailure> failures;

    DrawingRunSummary()
        : totalCount(0),
          createdCount(0),
          sheetCount(0)
    {
    }
};

static std::string FileNameOnly(const std::string& path)
{
    const size_t pos = path.find_last_of("\\/");
    return pos == std::string::npos ? path : path.substr(pos + 1);
}

static std::string BuildRunSummaryMessage(const DrawingRunSummary& summary)
{
    std::ostringstream oss;
    oss << "批量展开图出图完成。\n"
        << "展开图总数: " << summary.totalCount << "\n"
        << "成功创建: " << summary.createdCount << "\n"
        << "失败: " << summary.failures.size() << "\n"
        << "图纸页: " << summary.sheetCount;

    if (!summary.failures.empty())
    {
        oss << "\n\n失败列表:";
        for (size_t i = 0; i < summary.failures.size(); ++i)
        {
            oss << "\n" << (i + 1) << ". 文件: " << FileNameOnly(summary.failures[i].partKey)
                << "  展开: " << summary.failures[i].viewName;
        }
    }

    return oss.str();
}

struct FlatPatternItem
{
    NXOpen::Part* part;
    NXOpen::Body* body;
    NXOpen::Features::Feature* feature;
    NXOpen::Face* upwardFace;
    std::string viewName;
    std::string partKey;
    std::string material;
    std::string thickness;
    std::string layoutKey;
    std::string noteText;
    std::string ufNoteText;
    int quantity;
    double flatWidth;
    double flatHeight;
    double flatArea;
    bool hasFlatSize;
};

struct DraftRect
{
    double minX;
    double minY;
    double maxX;
    double maxY;
};

static std::string LocaleText(const NXOpen::NXString& value)
{
    const char* text = value.GetLocaleText();
    return text != NULL ? std::string(text) : std::string();
}

static void LogLine(const std::string& text)
{
    (void)text;
}

static long long ElapsedMilliseconds(const std::chrono::steady_clock::time_point& start)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
}

static double ClampPositive(double value, double fallback)
{
    return value > 1.0e-6 ? value : fallback;
}

static bool GetToggleValue(Toggle* block, bool fallback)
{
    if (block == NULL)
    {
        return fallback;
    }
    PropertyList* props = block->GetProperties();
    bool value = fallback;
    try
    {
        value = props->GetLogical("Value");
    }
    catch (...)
    {
    }
    delete props;
    return value;
}

static double GetDoubleValue(DoubleBlock* block, double fallback)
{
    if (block == NULL)
    {
        return fallback;
    }
    PropertyList* props = block->GetProperties();
    double value = fallback;
    try
    {
        value = props->GetDouble("Value");
    }
    catch (...)
    {
    }
    delete props;
    return value;
}

static std::string FormatReal(double value)
{
    std::ostringstream stream;
    if (std::fabs(value - std::round(value)) < 1.0e-6)
    {
        stream << static_cast<long long>(std::llround(value));
        return stream.str();
    }

    stream.setf(std::ios::fixed);
    stream.precision(3);
    stream << value;
    std::string text = stream.str();
    while (!text.empty() && text[text.size() - 1] == '0')
    {
        text.erase(text.size() - 1);
    }
    if (!text.empty() && text[text.size() - 1] == '.')
    {
        text.erase(text.size() - 1);
    }
    return text;
}

static std::string Trim(const std::string& text)
{
    const size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
    {
        return "";
    }
    const size_t last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

static void ReplaceAllText(std::string& text, const std::string& from, const std::string& to)
{
    if (from.empty())
    {
        return;
    }

    size_t pos = 0;
    while ((pos = text.find(from, pos)) != std::string::npos)
    {
        text.replace(pos, from.size(), to);
        pos += to.size();
    }
}

static std::string RemoveUtf8Bom(const std::string& value)
{
    if (value.size() >= 3 &&
        static_cast<unsigned char>(value[0]) == 0xEF &&
        static_cast<unsigned char>(value[1]) == 0xBB &&
        static_cast<unsigned char>(value[2]) == 0xBF)
    {
        return value.substr(3);
    }
    return value;
}

static std::string ConvertCodePage(const std::string& text, unsigned int fromCodePage, unsigned int toCodePage)
{
    if (text.empty())
    {
        return "";
    }

    const int wideLength = MultiByteToWideChar(
        fromCodePage,
        0,
        text.c_str(),
        static_cast<int>(text.size()),
        NULL,
        0);
    if (wideLength <= 0)
    {
        return text;
    }

    std::wstring wideText(static_cast<size_t>(wideLength), L'\0');
    if (MultiByteToWideChar(
        fromCodePage,
        0,
        text.c_str(),
        static_cast<int>(text.size()),
        &wideText[0],
        wideLength) <= 0)
    {
        return text;
    }

    const int targetLength = WideCharToMultiByte(
        toCodePage,
        0,
        wideText.c_str(),
        wideLength,
        NULL,
        0,
        NULL,
        NULL);
    if (targetLength <= 0)
    {
        return text;
    }

    std::string converted(static_cast<size_t>(targetLength), '\0');
    if (WideCharToMultiByte(
        toCodePage,
        0,
        wideText.c_str(),
        wideLength,
        &converted[0],
        targetLength,
        NULL,
        NULL) <= 0)
    {
        return text;
    }
    return converted;
}

static std::string Utf8ToLocaleEncoding(const std::string& text)
{
    return ConvertCodePage(text, kCodePageUtf8, kCodePageAcp);
}

static std::wstring PluginDirectory()
{
    return L"D:\\\x667A\x8F89\\application";
}

static std::wstring BodyNoteConfigFilePath()
{
    return PluginDirectory() + L"\\ZiDonCuTu_note_format.ini";
}

static void EnsureDefaultBodyNoteConfigFile(const std::wstring& path)
{
    if (_waccess(path.c_str(), 0) == 0)
    {
        return;
    }

    std::ofstream file(path, std::ios::binary);
    if (!file)
    {
        LogLine("[BodyNoteConfig] cannot create config file");
        return;
    }

    const unsigned char bom[] = { 0xEF, 0xBB, 0xBF };
    file.write(reinterpret_cast<const char*>(bom), sizeof(bom));
    file
        << "# ZiDonCuTu note format configuration\r\n"
        << "# Tokens: {文件名}, {编号}, {编号=}, {材料}, {厚度}, {数量}, {镜像}\r\n"
        << "# Multi-line example: 编号:{编号}\\n材料:{材料}\\n数量:{数量}PCS\r\n\r\n"
        << "[标注注释]\r\n"
        << kDefaultBodyNoteFormat << "\r\n";
}

static std::string DecodeConfigEscapes(const std::string& value)
{
    std::string result;
    bool escape = false;
    for (size_t i = 0; i < value.size(); ++i)
    {
        const char ch = value[i];
        if (escape)
        {
            switch (ch)
            {
            case 'n':
                result += '\n';
                break;
            case 'r':
                break;
            case 't':
                result += '\t';
                break;
            case '\\':
                result += '\\';
                break;
            case '"':
                result += '"';
                break;
            default:
                result += ch;
                break;
            }
            escape = false;
            continue;
        }

        if (ch == '\\')
        {
            escape = true;
            continue;
        }
        result += ch;
    }

    if (escape)
    {
        result += '\\';
    }
    return result;
}

static bool TryReadBodyNoteFormatLine(const std::string& rawLine, std::string& formatText)
{
    std::string line = Trim(RemoveUtf8Bom(rawLine));
    if (line.empty() || line[0] == '#' || line[0] == ';')
    {
        return false;
    }
    if (line.front() == '[' && line.back() == ']')
    {
        return false;
    }

    const std::string key = "body_note_format";
    if (line.compare(0, key.size(), key) == 0)
    {
        const size_t equalPos = line.find('=');
        if (equalPos != std::string::npos)
        {
            line = Trim(line.substr(equalPos + 1));
        }
    }

    if (line.empty())
    {
        return false;
    }

    formatText = DecodeConfigEscapes(line);
    return !formatText.empty();
}

static std::string ReadAttributeText(NXOpen::NXObject* object, const char* title)
{
    if (object == NULL || title == NULL)
    {
        return "";
    }
    try
    {
        if (object->HasUserAttribute(title, NXOpen::NXObject::AttributeTypeString, -1))
        {
            return Trim(LocaleText(object->GetStringAttribute(title)));
        }
        if (object->HasUserAttribute(title, NXOpen::NXObject::AttributeTypeInteger, -1))
        {
            return std::to_string(object->GetIntegerAttribute(title));
        }
        if (object->HasUserAttribute(title, NXOpen::NXObject::AttributeTypeReal, -1))
        {
            return FormatReal(object->GetRealAttribute(title));
        }
    }
    catch (...)
    {
    }
    return "";
}

static bool HasAnyUserAttribute(NXOpen::NXObject* object, const char* title)
{
    if (object == NULL || title == NULL)
    {
        return false;
    }

    try
    {
        return object->HasUserAttribute(title, NXOpen::NXObject::AttributeTypeString, -1) ||
            object->HasUserAttribute(title, NXOpen::NXObject::AttributeTypeInteger, -1) ||
            object->HasUserAttribute(title, NXOpen::NXObject::AttributeTypeReal, -1);
    }
    catch (...)
    {
        return false;
    }
}

static bool SetStringAttribute(NXOpen::NXObject* object, const char* title, const std::string& value)
{
    if (object == NULL || title == NULL)
    {
        return false;
    }
    try
    {
        object->SetUserAttribute(title, -1, value.c_str(), NXOpen::Update::OptionNow);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

static std::string ReadFirstAttribute(NXOpen::NXObject* first, NXOpen::NXObject* second, const char* const* titles, size_t count)
{
    for (size_t i = 0; i < count; ++i)
    {
        std::string value = ReadAttributeText(first, titles[i]);
        if (!value.empty())
        {
            return value;
        }
        value = ReadAttributeText(second, titles[i]);
        if (!value.empty())
        {
            return value;
        }
    }
    return "";
}

static std::vector<double> ParsePositiveNumbers(const std::string& text)
{
    std::string normalized = text;
    for (size_t i = 0; i < normalized.size(); ++i)
    {
        char c = normalized[i];
        if (c == ',')
        {
            normalized[i] = '.';
        }
        else if (!((c >= '0' && c <= '9') || c == '.' || c == '-' || c == '+' || c == 'e' || c == 'E'))
        {
            normalized[i] = ' ';
        }
    }

    std::vector<double> values;
    std::stringstream stream(normalized);
    std::string token;
    while (stream >> token)
    {
        char* endPtr = NULL;
        const double value = std::strtod(token.c_str(), &endPtr);
        if (endPtr != token.c_str() && value > 1.0e-6)
        {
            values.push_back(value);
        }
    }
    return values;
}

static bool TryParseRealText(const std::string& text, double& value)
{
    const std::vector<double> values = ParsePositiveNumbers(text);
    if (values.empty())
    {
        return false;
    }
    value = values[0];
    return true;
}

static bool TryParseSizePairText(const std::string& text, double& width, double& height)
{
    const std::vector<double> values = ParsePositiveNumbers(text);
    if (values.size() < 2)
    {
        return false;
    }
    width = values[0];
    height = values[1];
    return width > 1.0e-6 && height > 1.0e-6;
}

static bool ReadRealAttributeFromTitles(NXOpen::NXObject* first, NXOpen::NXObject* second, const char* const* titles, size_t count, double& value)
{
    for (size_t i = 0; i < count; ++i)
    {
        std::string text = ReadAttributeText(first, titles[i]);
        if (!text.empty() && TryParseRealText(text, value))
        {
            return true;
        }
        text = ReadAttributeText(second, titles[i]);
        if (!text.empty() && TryParseRealText(text, value))
        {
            return true;
        }
    }
    return false;
}

static bool ReadSizePairAttributeFromTitles(NXOpen::NXObject* first, NXOpen::NXObject* second, const char* const* titles, size_t count, double& width, double& height)
{
    for (size_t i = 0; i < count; ++i)
    {
        std::string text = ReadAttributeText(first, titles[i]);
        if (!text.empty() && TryParseSizePairText(text, width, height))
        {
            return true;
        }
        text = ReadAttributeText(second, titles[i]);
        if (!text.empty() && TryParseSizePairText(text, width, height))
        {
            return true;
        }
    }
    return false;
}

static bool ReadFlatPatternSizeFromAttributes(NXOpen::Features::Feature* feature, NXOpen::Body* body, double& width, double& height)
{
    width = 0.0;
    height = 0.0;

    static const char* const widthTitles[] = {
        "Minimum X", "MinimumX", "MINIMUM X", "Min X", "MIN X",
        "WIDTH", "Width", "width", "W", "w", "X", "x",
        "FLAT_WIDTH", "flat_width", "FlatWidth", "Kuan", "kuan",
        "\xE5\xB1\x95\xE5\xBC\x80\xE5\xAE\xBD\xE5\xBA\xA6",
        "\xE5\xAE\xBD\xE5\xBA\xA6",
        "\xE5\xAE\xBD",
        "\xE5\xB1\x95\xE5\xBC\x80\xE5\x9B\xBE\xE5\xAE\xBD\xE5\xBA\xA6",
        "\xE4\xB8\x8B\xE6\x96\x99\xE5\xAE\xBD\xE5\xBA\xA6"
    };
    static const char* const heightTitles[] = {
        "Minimum Y", "MinimumY", "MINIMUM Y", "Min Y", "MIN Y",
        "HEIGHT", "Height", "height", "H", "h", "Y", "y", "L", "l",
        "LENGTH", "Length", "length", "FLAT_LENGTH", "flat_length", "FlatLength", "Chang", "chang",
        "\xE5\xB1\x95\xE5\xBC\x80\xE9\x95\xBF\xE5\xBA\xA6",
        "\xE9\x95\xBF\xE5\xBA\xA6",
        "\xE9\x95\xBF",
        "\xE5\xB1\x95\xE5\xBC\x80\xE5\x9B\xBE\xE9\x95\xBF\xE5\xBA\xA6",
        "\xE4\xB8\x8B\xE6\x96\x99\xE9\x95\xBF\xE5\xBA\xA6"
    };
    static const char* const sizeTitles[] = {
        "SIZE", "Size", "size", "DIM", "Dim", "dim", "FLAT_SIZE", "flat_size", "FlatSize",
        "\xE8\xA7\x84\xE6\xA0\xBC",
        "\xE5\xB0\xBA\xE5\xAF\xB8",
        "\xE5\xB1\x95\xE5\xBC\x80\xE5\xB0\xBA\xE5\xAF\xB8",
        "\xE5\xB1\x95\xE5\xBC\x80\xE5\x9B\xBE\xE5\xB0\xBA\xE5\xAF\xB8"
    };

    const bool hasWidth = ReadRealAttributeFromTitles(feature, body, widthTitles, sizeof(widthTitles) / sizeof(widthTitles[0]), width);
    const bool hasHeight = ReadRealAttributeFromTitles(feature, body, heightTitles, sizeof(heightTitles) / sizeof(heightTitles[0]), height);
    if (hasWidth && hasHeight)
    {
        return true;
    }

    if (ReadSizePairAttributeFromTitles(feature, body, sizeTitles, sizeof(sizeTitles) / sizeof(sizeTitles[0]), width, height))
    {
        return true;
    }

    return false;
}

static std::string ReadPartFileName(NXOpen::Part* part)
{
    if (part == NULL)
    {
        return "";
    }
    try
    {
        std::string fullName = LocaleText(part->FullPath());
        if (fullName.empty())
        {
            fullName = LocaleText(part->Name());
        }
        const size_t slash = fullName.find_last_of("\\/");
        if (slash != std::string::npos)
        {
            fullName = fullName.substr(slash + 1);
        }
        const size_t dot = fullName.find_last_of('.');
        if (dot != std::string::npos)
        {
            fullName = fullName.substr(0, dot);
        }
        return fullName;
    }
    catch (...)
    {
        return "";
    }
}

static std::string ReadQuantityText(NXOpen::Part* part, NXOpen::Body* body, int occurrenceQuantity)
{
    if (occurrenceQuantity > 1)
    {
        return std::to_string(occurrenceQuantity);
    }

    try
    {
        if (part != NULL && body != NULL)
        {
            const int bodyId = body->GetIntegerAttribute("BodyID");
            char expressionName[256] = {};
            sprintf(expressionName, "ZSuLian_%d", bodyId);
            NXOpen::Expression* expression = part->Expressions()->FindObject(expressionName);
            if (expression != NULL)
            {
                return FormatReal(expression->Value());
            }
        }
    }
    catch (...)
    {
    }

    const char* titles[] = { "sulian", "\xE6\x95\xB0\xE9\x87\x8F" };
    std::string value = ReadFirstAttribute(body, part, titles, sizeof(titles) / sizeof(titles[0]));
    return value.empty() ? "1" : value;
}

static std::string LoadBodyNoteFormat()
{
    try
    {
        const std::wstring path = BodyNoteConfigFilePath();
        EnsureDefaultBodyNoteConfigFile(path);

        std::ifstream file(path, std::ios::binary);
        if (!file)
        {
            LogLine("[BodyNoteConfig] cannot open config, use default");
            return kDefaultBodyNoteFormat;
        }

        std::string line;
        std::string formatText;
        std::string multiLineFormat;
        while (std::getline(file, line))
        {
            if (TryReadBodyNoteFormatLine(line, formatText))
            {
                if (!multiLineFormat.empty())
                {
                    multiLineFormat += '\n';
                }
                multiLineFormat += formatText;
            }
        }

        if (!multiLineFormat.empty())
        {
            LogLine("[BodyNoteConfig] format=" + multiLineFormat);
            return multiLineFormat;
        }

        LogLine("[BodyNoteConfig] empty config, use default");
    }
    catch (const std::exception& ex)
    {
        LogLine("[BodyNoteConfig] exception: " + std::string(ex.what()));
    }
    catch (...)
    {
        LogLine("[BodyNoteConfig] unknown exception");
    }

    return kDefaultBodyNoteFormat;
}

static const std::string& CachedBodyNoteFormat()
{
    static const std::string format = LoadBodyNoteFormat();
    return format;
}

static int BodyNoteFormatLineCount()
{
    const std::string& format = CachedBodyNoteFormat();
    int lineCount = 1;
    for (size_t i = 0; i < format.size(); ++i)
    {
        if (format[i] == '\n')
        {
            ++lineCount;
        }
    }
    return lineCount < 1 ? 1 : lineCount;
}

static double BodyNoteExtraDownOffset(double textSize)
{
    const int lineCount = BodyNoteFormatLineCount();
    if (lineCount <= 1)
    {
        return 0.0;
    }
    return static_cast<double>(lineCount - 1) * std::max(1.0, textSize);
}

static NXOpen::Point3d AdjustBodyNotePointForLineCount(const NXOpen::Point3d& point, double textSize)
{
    NXOpen::Point3d adjusted = point;
    adjusted.Y -= BodyNoteExtraDownOffset(textSize);
    return adjusted;
}

static std::string BodyMaterialText(NXOpen::Part* part, NXOpen::Body* body)
{
    const char* materialTitles[] = {
        "cailiao",
        "\xE6\x9D\x90\xE6\x96\x99",
        "\xE6\x9D\x90\xE8\xB4\xA8"
    };
    return ReadFirstAttribute(body, part, materialTitles, sizeof(materialTitles) / sizeof(materialTitles[0]));
}

static bool PartHasRequiredAssemblyExportAttributes(NXOpen::Part* part, std::string& material, std::string& quantity)
{
    material.clear();
    quantity.clear();
    if (part == NULL)
    {
        return false;
    }

    const char* materialTitles[] = {
        "cailiao",
        "\xE6\x9D\x90\xE6\x96\x99",
        "\xE6\x9D\x90\xE8\xB4\xA8"
    };
    const char* quantityTitles[] = {
        "sulian",
        "\xE6\x95\xB0\xE9\x87\x8F"
    };

    material = ReadFirstAttribute(part, NULL, materialTitles, sizeof(materialTitles) / sizeof(materialTitles[0]));
    quantity = ReadFirstAttribute(part, NULL, quantityTitles, sizeof(quantityTitles) / sizeof(quantityTitles[0]));
    return !Trim(material).empty() && !Trim(quantity).empty();
}

static std::string BodyThicknessText(NXOpen::Part* part, NXOpen::Body* body)
{
    std::string thickness = ReadAttributeText(body, "Z");
    if (thickness.empty() && part != NULL && body != NULL)
    {
        try
        {
            const double value = part->Features()->SheetmetalManager()->GetBodyThickness(body);
            if (value > 1.0e-6)
            {
                thickness = FormatReal(value);
                SetStringAttribute(body, "Z", thickness);
            }
        }
        catch (...)
        {
        }
    }
    return thickness;
}

static std::string NormalizeLayoutKeyText(const std::string& value)
{
    std::string text = Trim(value);
    for (size_t i = 0; i < text.size(); ++i)
    {
        if (text[i] >= 'A' && text[i] <= 'Z')
        {
            text[i] = static_cast<char>(text[i] - 'A' + 'a');
        }
    }
    return text.empty() ? "~" : text;
}

static std::string BodyLayoutKey(const std::string& material, const std::string& thickness)
{
    return NormalizeLayoutKeyText(material) + "\t" + NormalizeLayoutKeyText(thickness);
}

static std::string BuildBodyNoteText(NXOpen::Part* part, NXOpen::Body* body, int occurrenceQuantity)
{
    const std::string material = BodyMaterialText(part, body);
    const std::string thickness = BodyThicknessText(part, body);

    const std::string quantity = ReadQuantityText(part, body, occurrenceQuantity);
    const std::string mirror = ReadAttributeText(body, "MIRR");
    const std::string number = ReadAttributeText(body, "bianhao");

    std::string note = CachedBodyNoteFormat();
    ReplaceAllText(note, "{\xE6\x96\x87\xE4\xBB\xB6\xE5\x90\x8D}", ReadPartFileName(part));
    ReplaceAllText(note, "{\xE7\xBC\x96\xE5\x8F\xB7=}", number.empty() ? "" : number + "=");
    ReplaceAllText(note, "{\xE7\xBC\x96\xE5\x8F\xB7}", number);
    ReplaceAllText(note, "{\xE6\x9D\x90\xE6\x96\x99}", material);
    ReplaceAllText(note, "{\xE5\x8E\x9A\xE5\xBA\xA6}", thickness);
    ReplaceAllText(note, "{\xE6\x95\xB0\xE9\x87\x8F}", quantity.empty() ? "1" : quantity);
    ReplaceAllText(note, "{\xE9\x95\x9C\xE5\x83\x8F}", mirror);
    note = Trim(note);
    if (note.empty())
    {
        note = ReadPartFileName(part);
    }
    note = note.empty() ? "FLAT PATTERN" : note;
    LogLine("[BuildBodyNoteText] part=" + LocaleText(part != NULL ? part->Name() : NXOpen::NXString(""))
        + " bodyTag=" + std::to_string(static_cast<unsigned long long>(body != NULL ? body->Tag() : NULL_TAG))
        + " material=" + material
        + " thickness=" + thickness
        + " quantity=" + (quantity.empty() ? "1" : quantity)
        + " number=" + number
        + " mirror=" + mirror
        + " note=" + note);
    return note;
}

static std::string BuildBodyNoteStaticSkeletonText()
{
    std::string text = CachedBodyNoteFormat();
    ReplaceAllText(text, "{\xE6\x96\x87\xE4\xBB\xB6\xE5\x90\x8D}", "");
    ReplaceAllText(text, "{\xE7\xBC\x96\xE5\x8F\xB7=}", "");
    ReplaceAllText(text, "{\xE7\xBC\x96\xE5\x8F\xB7}", "");
    ReplaceAllText(text, "{\xE6\x9D\x90\xE6\x96\x99}", "");
    ReplaceAllText(text, "{\xE5\x8E\x9A\xE5\xBA\xA6}", "");
    ReplaceAllText(text, "{\xE6\x95\xB0\xE9\x87\x8F}", "");
    ReplaceAllText(text, "{\xE9\x95\x9C\xE5\x83\x8F}", "");
    text = Trim(text);
    return text.empty() ? " " : text;
}

static std::string BodyNoteTokenValueForUf(
    const std::string& token,
    NXOpen::Part* part,
    NXOpen::Body* body,
    int occurrenceQuantity)
{
    if (token == "{\xE6\x96\x87\xE4\xBB\xB6\xE5\x90\x8D}")
    {
        return ReadPartFileName(part);
    }
    if (token == "{\xE7\xBC\x96\xE5\x8F\xB7=}")
    {
        const std::string number = ReadAttributeText(body, "bianhao");
        return number.empty() ? "" : number + "=";
    }
    if (token == "{\xE7\xBC\x96\xE5\x8F\xB7}")
    {
        return ReadAttributeText(body, "bianhao");
    }
    if (token == "{\xE6\x9D\x90\xE6\x96\x99}")
    {
        return BodyMaterialText(part, body);
    }
    if (token == "{\xE5\x8E\x9A\xE5\xBA\xA6}")
    {
        return BodyThicknessText(part, body);
    }
    if (token == "{\xE6\x95\xB0\xE9\x87\x8F}")
    {
        const std::string quantity = ReadQuantityText(part, body, occurrenceQuantity);
        return quantity.empty() ? "1" : quantity;
    }
    if (token == "{\xE9\x95\x9C\xE5\x83\x8F}")
    {
        return ReadAttributeText(body, "MIRR");
    }
    return "";
}

static bool MatchBodyNoteToken(const std::string& format, size_t offset, std::string& token);

static std::string BuildBodyNoteTextForUf(
    NXOpen::Part* part,
    NXOpen::Body* body,
    int occurrenceQuantity)
{
    const std::string& format = CachedBodyNoteFormat();
    std::string result;
    std::string staticText;
    for (size_t i = 0; i < format.size();)
    {
        std::string token;
        if (MatchBodyNoteToken(format, i, token))
        {
            if (!staticText.empty())
            {
                result += Utf8ToLocaleEncoding(staticText);
                staticText.clear();
            }
            result += BodyNoteTokenValueForUf(token, part, body, occurrenceQuantity);
            i += token.size();
            continue;
        }

        staticText += format[i];
        ++i;
    }

    if (!staticText.empty())
    {
        result += Utf8ToLocaleEncoding(staticText);
    }
    result = Trim(result);
    return result.empty() ? "FLAT PATTERN" : result;
}

static std::vector<NXOpen::NXString> MakeNoteLines(const std::string& noteText)
{
    std::vector<NXOpen::NXString> lines;
    std::string current;
    for (size_t i = 0; i < noteText.size(); ++i)
    {
        const char ch = noteText[i];
        if (ch == '\r')
        {
            continue;
        }
        if (ch == '\n')
        {
            lines.push_back(NXOpen::NXString(current.c_str(), NXOpen::NXString::UTF8));
            current.clear();
            continue;
        }
        current += ch;
    }
    lines.push_back(NXOpen::NXString(current.c_str(), NXOpen::NXString::UTF8));
    return lines;
}

static int Utf8CodePointCount(const std::string& text)
{
    int count = 0;
    for (size_t i = 0; i < text.size(); ++i)
    {
        const unsigned char ch = static_cast<unsigned char>(text[i]);
        if ((ch & 0xC0) != 0x80)
        {
            ++count;
        }
    }
    return count;
}

struct DraftNoteReferenceInsert
{
    enum Kind
    {
        KindAttribute,
        KindExpression
    };

    Kind kind;
    NXOpen::NXObject* owner;
    std::string title;
    std::string expressionName;
    int lineNo;
    int cursorPos;
};

static bool MatchBodyNoteToken(const std::string& format, size_t offset, std::string& token)
{
    const char* tokens[] = {
        "{\xE6\x96\x87\xE4\xBB\xB6\xE5\x90\x8D}",
        "{\xE7\xBC\x96\xE5\x8F\xB7=}",
        "{\xE7\xBC\x96\xE5\x8F\xB7}",
        "{\xE6\x9D\x90\xE6\x96\x99}",
        "{\xE5\x8E\x9A\xE5\xBA\xA6}",
        "{\xE6\x95\xB0\xE9\x87\x8F}",
        "{\xE9\x95\x9C\xE5\x83\x8F}"
    };
    for (size_t i = 0; i < sizeof(tokens) / sizeof(tokens[0]); ++i)
    {
        const std::string candidate(tokens[i]);
        if (format.compare(offset, candidate.size(), candidate) == 0)
        {
            token = candidate;
            return true;
        }
    }
    return false;
}

static bool FindAttributeOwner(NXOpen::NXObject* first, NXOpen::NXObject* second, const char* const* titles, size_t count, NXOpen::NXObject*& owner, std::string& title)
{
    for (size_t i = 0; i < count; ++i)
    {
        if (HasAnyUserAttribute(first, titles[i]))
        {
            owner = first;
            title = titles[i];
            return true;
        }
    }
    for (size_t i = 0; i < count; ++i)
    {
        if (HasAnyUserAttribute(second, titles[i]))
        {
            owner = second;
            title = titles[i];
            return true;
        }
    }
    return false;
}

static bool FindQuantityExpression(NXOpen::Part* part, NXOpen::Body* body, std::string& expressionName)
{
    if (part == NULL || body == NULL)
    {
        return false;
    }
    try
    {
        const int bodyId = body->GetIntegerAttribute("BodyID");
        char name[256] = {};
        sprintf(name, "ZSuLian_%d", bodyId);
        if (part->Expressions()->FindObject(name) != NULL)
        {
            expressionName = name;
            return true;
        }
    }
    catch (...)
    {
    }
    return false;
}

static bool PrepareBodyNoteReference(
    const std::string& token,
    NXOpen::Part* part,
    NXOpen::Body* body,
    int occurrenceQuantity,
    DraftNoteReferenceInsert& insert,
    std::string& fallback,
    bool& appendEqual)
{
    insert.owner = NULL;
    insert.title.clear();
    insert.expressionName.clear();
    insert.lineNo = 1;
    insert.cursorPos = 1;
    fallback.clear();
    appendEqual = false;

    if (token == "{\xE6\x96\x87\xE4\xBB\xB6\xE5\x90\x8D}")
    {
        const char* titles[] = { "DB_PART_NAME", "part_name", "\xE6\x96\x87\xE4\xBB\xB6\xE5\x90\x8D" };
        if (FindAttributeOwner(part, NULL, titles, sizeof(titles) / sizeof(titles[0]), insert.owner, insert.title))
        {
            insert.kind = DraftNoteReferenceInsert::KindAttribute;
            return true;
        }
        fallback = ReadPartFileName(part);
        return false;
    }

    if (token == "{\xE7\xBC\x96\xE5\x8F\xB7=}" || token == "{\xE7\xBC\x96\xE5\x8F\xB7}")
    {
        const char* titles[] = { "bianhao" };
        if (FindAttributeOwner(body, NULL, titles, sizeof(titles) / sizeof(titles[0]), insert.owner, insert.title))
        {
            insert.kind = DraftNoteReferenceInsert::KindAttribute;
            appendEqual = (token == "{\xE7\xBC\x96\xE5\x8F\xB7=}");
            return true;
        }
        const std::string number = ReadAttributeText(body, "bianhao");
        fallback = (token == "{\xE7\xBC\x96\xE5\x8F\xB7=}" && !number.empty()) ? number + "=" : number;
        return false;
    }

    if (token == "{\xE6\x9D\x90\xE6\x96\x99}")
    {
        const char* titles[] = { "cailiao", "\xE6\x9D\x90\xE6\x96\x99", "\xE6\x9D\x90\xE8\xB4\xA8" };
        if (FindAttributeOwner(body, part, titles, sizeof(titles) / sizeof(titles[0]), insert.owner, insert.title))
        {
            insert.kind = DraftNoteReferenceInsert::KindAttribute;
            return true;
        }
        fallback = ReadFirstAttribute(body, part, titles, sizeof(titles) / sizeof(titles[0]));
        return false;
    }

    if (token == "{\xE5\x8E\x9A\xE5\xBA\xA6}")
    {
        if (!HasAnyUserAttribute(body, "Z") && part != NULL && body != NULL)
        {
            try
            {
                const double value = part->Features()->SheetmetalManager()->GetBodyThickness(body);
                if (value > 1.0e-6)
                {
                    SetStringAttribute(body, "Z", FormatReal(value));
                }
            }
            catch (...)
            {
            }
        }
        const char* titles[] = { "Z" };
        if (FindAttributeOwner(body, NULL, titles, sizeof(titles) / sizeof(titles[0]), insert.owner, insert.title))
        {
            insert.kind = DraftNoteReferenceInsert::KindAttribute;
            return true;
        }
        fallback = ReadAttributeText(body, "Z");
        return false;
    }

    if (token == "{\xE6\x95\xB0\xE9\x87\x8F}")
    {
        const char* titles[] = { "sulian", "\xE6\x95\xB0\xE9\x87\x8F" };
        if (FindAttributeOwner(body, part, titles, sizeof(titles) / sizeof(titles[0]), insert.owner, insert.title))
        {
            insert.kind = DraftNoteReferenceInsert::KindAttribute;
            return true;
        }
        if (FindQuantityExpression(part, body, insert.expressionName))
        {
            insert.kind = DraftNoteReferenceInsert::KindExpression;
            return true;
        }
        fallback = ReadQuantityText(part, body, occurrenceQuantity);
        return false;
    }

    if (token == "{\xE9\x95\x9C\xE5\x83\x8F}")
    {
        const char* titles[] = { "MIRR" };
        if (FindAttributeOwner(body, NULL, titles, sizeof(titles) / sizeof(titles[0]), insert.owner, insert.title))
        {
            insert.kind = DraftNoteReferenceInsert::KindAttribute;
            return true;
        }
        fallback = ReadAttributeText(body, "MIRR");
        return false;
    }

    fallback = token;
    return false;
}

static bool SetLinkedBodyNoteText(
    NXOpen::Annotations::TextWithSymbolsBuilder* textBlock,
    NXOpen::Part* part,
    NXOpen::Body* body,
    int occurrenceQuantity)
{
    if (textBlock == NULL)
    {
        return false;
    }

    std::vector<NXOpen::NXString> emptyText(1);
    emptyText[0] = "";
    textBlock->SetText(emptyText);

    std::vector<NXOpen::NXString> lines;
    std::vector<DraftNoteReferenceInsert> inserts;
    std::string currentLine;
    int currentLineNo = 1;
    const std::string format = LoadBodyNoteFormat();

    for (size_t i = 0; i < format.size();)
    {
        if (format[i] == '\r')
        {
            ++i;
            continue;
        }
        if (format[i] == '\n')
        {
            lines.push_back(NXOpen::NXString(currentLine.c_str(), NXOpen::NXString::UTF8));
            currentLine.clear();
            ++currentLineNo;
            ++i;
            continue;
        }

        std::string token;
        if (MatchBodyNoteToken(format, i, token))
        {
            DraftNoteReferenceInsert insert;
            std::string fallback;
            bool appendEqual = false;
            if (PrepareBodyNoteReference(token, part, body, occurrenceQuantity, insert, fallback, appendEqual))
            {
                insert.lineNo = currentLineNo;
                insert.cursorPos = Utf8CodePointCount(currentLine) + 1;
                inserts.push_back(insert);
                if (appendEqual)
                {
                    currentLine += "=";
                }
            }
            else
            {
                currentLine += fallback;
            }
            i += token.size();
            continue;
        }

        currentLine += format[i];
        ++i;
    }

    lines.push_back(NXOpen::NXString(currentLine.c_str(), NXOpen::NXString::UTF8));
    if (lines.empty())
    {
        lines.push_back("");
    }
    textBlock->SetText(lines);

    std::sort(inserts.begin(), inserts.end(), [](const DraftNoteReferenceInsert& a, const DraftNoteReferenceInsert& b) {
        if (a.lineNo != b.lineNo)
        {
            return a.lineNo > b.lineNo;
        }
        return a.cursorPos > b.cursorPos;
    });

    int applied = 0;
    for (size_t i = 0; i < inserts.size(); ++i)
    {
        try
        {
            const DraftNoteReferenceInsert& insert = inserts[i];
            if (insert.kind == DraftNoteReferenceInsert::KindAttribute)
            {
                textBlock->AddAttributeReference(insert.owner, insert.title.c_str(), false, insert.lineNo, insert.cursorPos);
            }
            else
            {
                textBlock->AddExpressionReference(insert.expressionName.c_str(), "0.2", insert.lineNo, insert.cursorPos);
            }
            ++applied;
        }
        catch (const NXOpen::NXException& ex)
        {
            LogLine("[SetLinkedBodyNoteText] reference NXException code=" + std::to_string(ex.ErrorCode())
                + " message=" + std::string(ex.Message()));
        }
        catch (...)
        {
            LogLine("[SetLinkedBodyNoteText] reference exception");
        }
    }

    LogLine("[SetLinkedBodyNoteText] inserts=" + std::to_string(inserts.size())
        + " applied=" + std::to_string(applied));
    return applied > 0;
}

static bool FeatureTypeEquals(NXOpen::Features::Feature* feature, const char* type)
{
    if (feature == NULL || type == NULL)
    {
        return false;
    }
    try
    {
        return strcmp(feature->FeatureType().GetLocaleText(), type) == 0;
    }
    catch (...)
    {
        return false;
    }
}

static std::string AskUfFeatureTypeText(tag_t featureTag)
{
    if (featureTag == NULL_TAG)
    {
        return "";
    }

    char* featureType = NULL;
    if (UF_MODL_ask_feat_type(featureTag, &featureType) != 0 || featureType == NULL)
    {
        return "";
    }

    std::string text(featureType);
    UF_free(featureType);
    return text;
}

static bool TextContainsCaseInsensitive(const std::string& text, const std::string& expected)
{
    if (expected.empty())
    {
        return false;
    }

    std::string loweredText = text;
    std::string loweredExpected = expected;
    for (size_t i = 0; i < loweredText.size(); ++i)
    {
        if (loweredText[i] >= 'A' && loweredText[i] <= 'Z')
        {
            loweredText[i] = static_cast<char>(loweredText[i] - 'A' + 'a');
        }
    }
    for (size_t i = 0; i < loweredExpected.size(); ++i)
    {
        if (loweredExpected[i] >= 'A' && loweredExpected[i] <= 'Z')
        {
            loweredExpected[i] = static_cast<char>(loweredExpected[i] - 'A' + 'a');
        }
    }
    return loweredText.find(loweredExpected) != std::string::npos;
}

static bool TextLooksLikeBendReference(const std::string& text)
{
    return TextContainsCaseInsensitive(text, "bend") ||
        TextContainsCaseInsensitive(text, "joggle") ||
        text.find("\xE6\x8A\x98\xE5\xBC\xAF") != std::string::npos;
}

static bool IsSolidBody(NXOpen::Body* body)
{
    if (body == NULL)
    {
        return false;
    }
    try
    {
        return body->IsSolidBody();
    }
    catch (...)
    {
        return false;
    }
}

static bool FeatureOutputReferencesBody(NXOpen::Features::Feature* feature, NXOpen::Body* body)
{
    if (feature == NULL || body == NULL)
    {
        return false;
    }
    try
    {
        std::vector<NXOpen::Body*> bodies = feature->GetBodies();
        for (size_t i = 0; i < bodies.size(); ++i)
        {
            if (bodies[i] == body)
            {
                return true;
            }
        }
    }
    catch (...)
    {
    }
    try
    {
        std::vector<NXOpen::Face*> faces = feature->GetFaces();
        for (size_t i = 0; i < faces.size(); ++i)
        {
            if (faces[i] != NULL && faces[i]->GetBody() == body)
            {
                return true;
            }
        }
    }
    catch (...)
    {
    }
    return false;
}

static bool ParentsReferenceBody(NXOpen::Features::Feature* feature, NXOpen::Body* body, int depth, std::set<tag_t>& visited)
{
    if (feature == NULL || body == NULL || depth <= 0)
    {
        return false;
    }
    if (!visited.insert(feature->Tag()).second)
    {
        return false;
    }
    try
    {
        std::vector<NXOpen::Features::Feature*> parents = feature->GetParents();
        for (size_t i = 0; i < parents.size(); ++i)
        {
            if (FeatureOutputReferencesBody(parents[i], body) || ParentsReferenceBody(parents[i], body, depth - 1, visited))
            {
                return true;
            }
        }
    }
    catch (...)
    {
    }
    return false;
}

static bool ParentsReferenceBody(NXOpen::Features::Feature* feature, NXOpen::Body* body, int depth)
{
    std::set<tag_t> visited;
    return ParentsReferenceBody(feature, body, depth, visited);
}

static NXOpen::Body* ChooseBodyForFlatPattern(NXOpen::Part* part, NXOpen::Features::Feature* feature, NXOpen::Face* upwardFace)
{
    if (part == NULL || feature == NULL)
    {
        return NULL;
    }

    NXOpen::Body* upwardBody = upwardFace != NULL ? upwardFace->GetBody() : NULL;
    NXOpen::Body* best = upwardBody;
    int bestScore = best != NULL ? 10 : -1;

    try
    {
        for (NXOpen::BodyCollection::iterator it = part->Bodies()->begin(); it != part->Bodies()->end(); ++it)
        {
            NXOpen::Body* body = *it;
            if (!IsSolidBody(body))
            {
                continue;
            }
            int score = 0;
            if (body == upwardBody)
            {
                score += 80;
            }
            if (FeatureOutputReferencesBody(feature, body))
            {
                score += 60;
            }
            if (ParentsReferenceBody(feature, body, 2))
            {
                score += 40;
            }
            if (!ReadAttributeText(body, "bianhao").empty())
            {
                score += 10;
            }
            if (score > bestScore)
            {
                bestScore = score;
                best = body;
            }
        }
    }
    catch (...)
    {
    }

    return best;
}

static std::vector<FlatPatternItem> CollectPartFlatPatterns(NXOpen::Part* part, int quantity)
{
    std::vector<FlatPatternItem> items;
    if (part == NULL || part->Features() == NULL || part->Features()->SheetmetalManager() == NULL)
    {
        LogLine("[CollectPartFlatPatterns] skipped null part/features/sheetmetal manager");
        return items;
    }

    LogLine("[CollectPartFlatPatterns] scan part=" + LocaleText(part->Name()) + " quantity=" + std::to_string(quantity));
    std::set<std::string> seenViews;
    std::vector<NXOpen::Features::Feature*> features = part->Features()->GetFeatures();
    for (size_t i = 0; i < features.size(); ++i)
    {
        NXOpen::Features::Feature* feature = features[i];
        if (!FeatureTypeEquals(feature, "FLAT_PATTERN"))
        {
            continue;
        }

        LogLine("[CollectPartFlatPatterns] FLAT_PATTERN feature tag=" + std::to_string(static_cast<unsigned long long>(feature->Tag())));
        NXOpen::Features::SheetMetal::FlatPatternBuilder* builder = NULL;
        try
        {
            builder = part->Features()->SheetmetalManager()->CreateFlatPatternBuilder(feature);
            NXOpen::Face* upwardFace = (builder != NULL && builder->UpwardFace() != NULL) ? builder->UpwardFace()->Value() : NULL;
            std::string viewName = builder != NULL ? LocaleText(builder->FlatPatternViewName()) : "";
            if (viewName.empty())
            {
                viewName = "FLAT-PATTERN";
            }
            if (!seenViews.insert(viewName).second)
            {
                if (builder != NULL)
                {
                    builder->Destroy();
                }
                continue;
            }

            FlatPatternItem item;
            item.part = part;
            item.body = ChooseBodyForFlatPattern(part, feature, upwardFace);
            item.feature = feature;
            item.upwardFace = upwardFace;
            item.viewName = viewName;
            item.partKey = LocaleText(part->FullPath());
            item.quantity = quantity;
            item.material = BodyMaterialText(part, item.body);
            item.thickness = BodyThicknessText(part, item.body);
            item.layoutKey = BodyLayoutKey(item.material, item.thickness);
            item.noteText = BuildBodyNoteText(part, item.body, item.quantity);
            item.ufNoteText = BuildBodyNoteTextForUf(part, item.body, item.quantity);
            item.flatWidth = 0.0;
            item.flatHeight = 0.0;
            item.flatArea = 0.0;
            item.hasFlatSize = ReadFlatPatternSizeFromAttributes(feature, item.body, item.flatWidth, item.flatHeight);
            if (item.hasFlatSize)
            {
                item.flatArea = item.flatWidth * item.flatHeight;
            }
            items.push_back(item);
            LogLine("[CollectPartFlatPatterns] accepted view=" + item.viewName
                + " bodyTag=" + std::to_string(static_cast<unsigned long long>(item.body != NULL ? item.body->Tag() : NULL_TAG))
                + " material=" + item.material
                + " thickness=" + item.thickness
                + " layoutKey=" + item.layoutKey
                + " hasFlatSize=" + std::to_string(item.hasFlatSize ? 1 : 0)
                + " flatWidth=" + FormatReal(item.flatWidth)
                + " flatHeight=" + FormatReal(item.flatHeight)
                + " flatArea=" + FormatReal(item.flatArea));

            if (builder != NULL)
            {
                builder->Destroy();
            }
        }
        catch (...)
        {
            LogLine("[CollectPartFlatPatterns] exception while reading flat pattern");
            if (builder != NULL)
            {
                try
                {
                    builder->Destroy();
                }
                catch (...)
                {
                }
            }
        }
    }
    return items;
}

static bool IsAssemblyPart(NXOpen::Part* part)
{
    if (part == NULL || part->ComponentAssembly() == NULL)
    {
        return false;
    }
    try
    {
        NXOpen::Assemblies::Component* root = part->ComponentAssembly()->RootComponent();
        return root != NULL && !root->GetChildren().empty();
    }
    catch (...)
    {
        return false;
    }
}

struct AssemblyPartCountInfo
{
    NXOpen::Part* part;
    int count;
    bool loadedByPlugin;
};

static bool EnsurePartFullyLoaded(NXOpen::Part* part)
{
    if (part == NULL)
    {
        return false;
    }
    try
    {
        if (!part->IsFullyLoaded())
        {
            NXOpen::PartLoadStatus* status = part->LoadFully();
            LogLine("[EnsurePartFullyLoaded] loaded part=" + LocaleText(part->Name())
                + " status=" + std::string(status != NULL ? "ok" : "null"));
            if (status != NULL)
            {
                delete status;
            }
            return true;
        }
    }
    catch (const NXOpen::NXException& ex)
    {
        LogLine("[EnsurePartFullyLoaded] NXException part=" + LocaleText(part->Name())
            + " code=" + std::to_string(ex.ErrorCode())
            + " message=" + std::string(ex.Message()));
    }
    catch (...)
    {
        LogLine("[EnsurePartFullyLoaded] exception part=" + LocaleText(part->Name()));
    }
    return false;
}

static void ClosePluginLoadedPartIfUnused(NXOpen::Part* part, const std::string& reason)
{
    if (part == NULL)
    {
        return;
    }

    try
    {
        part->Close(
            NXOpen::BasePart::CloseWholeTreeFalse,
            NXOpen::BasePart::CloseModifiedDontCloseModified,
            NULL);
        LogLine("[ClosePluginLoadedPartIfUnused] closed part=" + LocaleText(part->Name())
            + " reason=" + reason);
    }
    catch (const NXOpen::NXException& ex)
    {
        LogLine("[ClosePluginLoadedPartIfUnused] NXException part=" + LocaleText(part->Name())
            + " reason=" + reason
            + " code=" + std::to_string(ex.ErrorCode())
            + " message=" + std::string(ex.Message()));
    }
    catch (...)
    {
        LogLine("[ClosePluginLoadedPartIfUnused] exception part=" + LocaleText(part->Name())
            + " reason=" + reason);
    }
}

static void AddAssemblyPrototypePartCount(NXOpen::Assemblies::Component* component, std::map<tag_t, AssemblyPartCountInfo>& counts)
{
    if (component == NULL)
    {
        return;
    }
    try
    {
        NXOpen::Part* part = dynamic_cast<NXOpen::Part*>(component->Prototype());
        if (part != NULL)
        {
            try
            {
                if (!component->GetChildren().empty())
                {
                    LogLine("[AddAssemblyPrototypePartCount] skip assembly component before load part="
                        + LocaleText(part->Name()));
                    return;
                }
            }
            catch (...)
            {
            }

            if (IsAssemblyPart(part))
            {
                LogLine("[AddAssemblyPrototypePartCount] skip assembly component part="
                    + LocaleText(part->Name()));
                return;
            }

            std::string material;
            std::string quantity;
            if (!PartHasRequiredAssemblyExportAttributes(part, material, quantity))
            {
                LogLine("[AddAssemblyPrototypePartCount] skip part without required part attributes part="
                    + LocaleText(part->Name())
                    + " material=" + material
                    + " quantity=" + quantity);
                return;
            }

            const bool loadedByPlugin = EnsurePartFullyLoaded(part);

            AssemblyPartCountInfo& entry = counts[part->Tag()];
            entry.part = part;
            entry.count += 1;
            entry.loadedByPlugin = entry.loadedByPlugin || loadedByPlugin;
            LogLine("[AddAssemblyPrototypePartCount] part=" + LocaleText(part->Name())
                + " count=" + std::to_string(entry.count)
                + " material=" + material
                + " quantity=" + quantity
                + " loadedByPlugin=" + std::to_string(entry.loadedByPlugin ? 1 : 0));
        }
    }
    catch (const NXOpen::NXException& ex)
    {
        LogLine("[AddAssemblyPrototypePartCount] NXException code=" + std::to_string(ex.ErrorCode())
            + " message=" + std::string(ex.Message()));
    }
    catch (...)
    {
        LogLine("[AddAssemblyPrototypePartCount] exception");
    }
}

static void CollectAssemblyPartCounts(NXOpen::Assemblies::Component* component, std::map<tag_t, AssemblyPartCountInfo>& counts)
{
    if (component == NULL)
    {
        return;
    }

    std::vector<NXOpen::Assemblies::Component*> children = component->GetChildren();
    for (size_t i = 0; i < children.size(); ++i)
    {
        NXOpen::Assemblies::Component* child = children[i];
        try
        {
            if (child != NULL && child->IsComponentOrAncestorSuppressed())
            {
                continue;
            }
        }
        catch (...)
        {
        }

        AddAssemblyPrototypePartCount(child, counts);
        CollectAssemblyPartCounts(child, counts);
    }
}

static std::vector<FlatPatternItem> CollectFlatPatternItems(NXOpen::Part* displayPart)
{
    std::vector<FlatPatternItem> result;
    if (displayPart == NULL)
    {
        LogLine("[CollectFlatPatternItems] displayPart is null");
        return result;
    }

    LogLine("[CollectFlatPatternItems] displayPart=" + LocaleText(displayPart->Name()));
    std::map<tag_t, AssemblyPartCountInfo> partCounts;
    try
    {
        NXOpen::Assemblies::Component* root = displayPart->ComponentAssembly()->RootComponent();
        if (root != NULL && !root->GetChildren().empty())
        {
            CollectAssemblyPartCounts(root, partCounts);
        }
    }
    catch (...)
    {
    }

    if (partCounts.empty())
    {
        AssemblyPartCountInfo& displayEntry = partCounts[displayPart->Tag()];
        displayEntry.part = displayPart;
        displayEntry.count = 1;
        displayEntry.loadedByPlugin = false;
        LogLine("[CollectFlatPatternItems] no assembly leaf counts, use display part");
    }

    LogLine("[CollectFlatPatternItems] unique part count=" + std::to_string(partCounts.size()));
    for (std::map<tag_t, AssemblyPartCountInfo>::iterator it = partCounts.begin(); it != partCounts.end(); ++it)
    {
        std::vector<FlatPatternItem> partItems = CollectPartFlatPatterns(it->second.part, it->second.count);
        if (partItems.empty() && it->second.loadedByPlugin && it->second.part != displayPart)
        {
            ClosePluginLoadedPartIfUnused(it->second.part, "no flat pattern after candidate scan");
        }
        result.insert(result.end(), partItems.begin(), partItems.end());
    }
    LogLine("[CollectFlatPatternItems] flat pattern item count=" + std::to_string(result.size()));
    return result;
}

static NXOpen::ModelingView* FindModelingView(NXOpen::Part* part, const std::string& preferredName)
{
    if (part == NULL)
    {
        return NULL;
    }
    const char* names[] = { preferredName.c_str(), "Top", "TOP", "Trimetric" };
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
    {
        if (names[i] == NULL || names[i][0] == '\0')
        {
            continue;
        }
        try
        {
            NXOpen::ModelingView* view = dynamic_cast<NXOpen::ModelingView*>(part->ModelingViews()->FindObject(names[i]));
            if (view != NULL)
            {
                return view;
            }
        }
        catch (...)
        {
        }
    }
    return NULL;
}

static NXOpen::Drawings::DraftingDrawingSheet* CreateCustomSheet(NXOpen::Part* part, const CommandOptions& options, const std::string& sheetName)
{
    LogLine("[CreateCustomSheet] begin width=" + FormatReal(options.sheetWidth)
        + " height=" + FormatReal(options.sheetHeight)
        + " name=" + sheetName);
    NXOpen::Drawings::DraftingDrawingSheet* nullSheet = NULL;
    NXOpen::Drawings::DraftingDrawingSheetBuilder* builder =
        part->DraftingDrawingSheets()->CreateDraftingDrawingSheetBuilder(nullSheet);

    builder->SetOption(NXOpen::Drawings::DrawingSheetBuilder::SheetOptionCustomSize);
    builder->SetUnits(NXOpen::Drawings::DrawingSheetBuilder::SheetUnitsMetric);
    builder->SetLength(options.sheetWidth);
    builder->SetHeight(options.sheetHeight);
    builder->SetName(sheetName.c_str());
    builder->SetProjectionAngle(NXOpen::Drawings::DrawingSheetBuilder::SheetProjectionAngleFirst);
    builder->SetStandardMetricScale(NXOpen::Drawings::DrawingSheetBuilder::SheetStandardMetricScaleCustom);
    builder->SetScaleNumerator(1.0);
    builder->SetScaleDenominator(1.0);
    builder->SetAutoStartViewCreation(false);

    NXOpen::NXObject* object = builder->Commit();
    builder->Destroy();
    try
    {
        part->Drafting()->SetTemplateInstantiationIsComplete(true);
    }
    catch (...)
    {
    }
    NXOpen::Drawings::DraftingDrawingSheet* sheet = dynamic_cast<NXOpen::Drawings::DraftingDrawingSheet*>(object);
    if (sheet != NULL)
    {
        try
        {
            sheet->Open();
            LogLine("[CreateCustomSheet] sheet opened");
            NXOpen::Drawings::DraftingDrawingSheet* currentSheet = part->DraftingDrawingSheets()->CurrentDrawingSheet();
            LogLine(std::string("[CreateCustomSheet] current drafting sheet=") + (currentSheet != NULL ? "ok" : "null"));
        }
        catch (const NXOpen::NXException& ex)
        {
            LogLine("[CreateCustomSheet] sheet open NXException code=" + std::to_string(ex.ErrorCode())
                + " message=" + std::string(ex.Message()));
        }
        catch (...)
        {
            LogLine("[CreateCustomSheet] sheet open exception");
        }
    }
    LogLine(std::string("[CreateCustomSheet] done sheet=") + (sheet != NULL ? "ok" : "null"));
    return sheet;
}

static void DeleteExistingPluginSheets(NXOpen::Part* part)
{
    if (part == NULL || part->DraftingDrawingSheets() == NULL)
    {
        return;
    }

    std::vector<NXOpen::TaggedObject*> deleteObjects;
    try
    {
        NXOpen::Drawings::DraftingDrawingSheetCollection* sheets = part->DraftingDrawingSheets();
        for (NXOpen::Drawings::DraftingDrawingSheetCollection::iterator it = sheets->begin(); it != sheets->end(); ++it)
        {
            NXOpen::Drawings::DraftingDrawingSheet* sheet = *it;
            if (sheet == NULL)
            {
                continue;
            }

            const std::string sheetName = LocaleText(sheet->Name());
            const std::string prefix(kPluginSheetNamePrefix);
            if (sheetName == prefix ||
                (sheetName.size() > prefix.size() &&
                 sheetName.compare(0, prefix.size(), prefix) == 0 &&
                 sheetName[prefix.size()] == '_'))
            {
                deleteObjects.push_back(sheet);
                LogLine("[DeleteExistingPluginSheets] queued sheet name=" + sheetName
                    + " tag=" + std::to_string(static_cast<unsigned long long>(sheet->Tag())));
            }
        }
    }
    catch (const NXOpen::NXException& ex)
    {
        LogLine("[DeleteExistingPluginSheets] collect NXException code="
            + std::to_string(ex.ErrorCode()) + " message=" + std::string(ex.Message()));
    }
    catch (...)
    {
        LogLine("[DeleteExistingPluginSheets] collect exception");
    }

    if (deleteObjects.empty())
    {
        LogLine("[DeleteExistingPluginSheets] no old plugin sheets");
        return;
    }

    try
    {
        Session* session = Session::GetSession();
        NXOpen::Update* update = session->UpdateManager();
        update->ClearDeleteList();
        const int addErrors = update->AddObjectsToDeleteList(deleteObjects);
        Session::UndoMarkId mark = session->SetUndoMark(Session::MarkVisibilityInvisible, "PILianDaoCuZKT delete old sheets");
        const int updateErrors = update->DoUpdate(mark);
        LogLine("[DeleteExistingPluginSheets] deleted count=" + std::to_string(deleteObjects.size())
            + " addErrors=" + std::to_string(addErrors)
            + " updateErrors=" + std::to_string(updateErrors));
    }
    catch (const NXOpen::NXException& ex)
    {
        LogLine("[DeleteExistingPluginSheets] delete NXException code="
            + std::to_string(ex.ErrorCode()) + " message=" + std::string(ex.Message()));
    }
    catch (...)
    {
        LogLine("[DeleteExistingPluginSheets] delete exception");
    }
}

static std::string PluginSheetName(size_t pageIndex)
{
    char name[64] = {};
    sprintf(name, "%s_%03u", kPluginSheetNamePrefix, static_cast<unsigned int>(pageIndex + 1));
    return std::string(name);
}

static std::string FlatPatternPartGroupKey(const FlatPatternItem& item)
{
    if (!item.partKey.empty())
    {
        return item.partKey;
    }
    if (item.part != NULL)
    {
        return std::to_string(static_cast<unsigned long long>(item.part->Tag()));
    }
    return item.viewName;
}

static double FlatPatternSortArea(const FlatPatternItem& item)
{
    if (item.hasFlatSize && item.flatArea > 1.0e-6)
    {
        return item.flatArea;
    }
    if (item.hasFlatSize && item.flatWidth > 1.0e-6 && item.flatHeight > 1.0e-6)
    {
        return item.flatWidth * item.flatHeight;
    }
    return 0.0;
}

static double FlatPatternSortMaxSize(const FlatPatternItem& item)
{
    if (item.hasFlatSize)
    {
        return std::max(item.flatWidth, item.flatHeight);
    }
    return 0.0;
}

static void SortFlatPatternItemsForClassBatches(std::vector<FlatPatternItem>& items)
{
    std::stable_sort(items.begin(), items.end(), [](const FlatPatternItem& a, const FlatPatternItem& b) {
        if (a.layoutKey != b.layoutKey)
        {
            return a.layoutKey < b.layoutKey;
        }
        if (a.hasFlatSize != b.hasFlatSize)
        {
            return a.hasFlatSize;
        }

        if (fabs(a.flatHeight - b.flatHeight) > 1.0e-6)
        {
            return a.flatHeight > b.flatHeight;
        }

        if (fabs(a.flatWidth - b.flatWidth) > 1.0e-6)
        {
            return a.flatWidth > b.flatWidth;
        }

        if (a.partKey != b.partKey)
        {
            return a.partKey < b.partKey;
        }
        return a.viewName < b.viewName;
    });

    size_t classCount = 0;
    std::string lastKey;
    for (size_t i = 0; i < items.size(); ++i)
    {
        if (i == 0 || items[i].layoutKey != lastKey)
        {
            ++classCount;
            lastKey = items[i].layoutKey;
        }
    }
    LogLine("[SortFlatPatternItemsForClassBatches] sorted classCount=" + std::to_string(classCount)
        + " itemCount=" + std::to_string(items.size())
        + " batchMode=whole-class"
        + " classOrder=layoutKey,MinimumY desc,MinimumX desc");
}

static NXOpen::Drawings::BaseView* CreateFlatBaseView(NXOpen::Part* workPart, const FlatPatternItem& item, const CommandOptions& options, const NXOpen::Point3d& point)
{
    const std::chrono::steady_clock::time_point totalStart = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point stepStart = totalStart;
    LogLine("[CreateFlatBaseView] begin part=" + LocaleText(item.part != NULL ? item.part->Name() : NXOpen::NXString(""))
        + " viewName=" + item.viewName
        + " workPart=" + LocaleText(workPart != NULL ? workPart->Name() : NXOpen::NXString(""))
        + " viewScale=1"
        + " dimensionScale=" + FormatReal(options.viewScaleDenominator));
    NXOpen::Drawings::BaseView* nullView = NULL;
    NXOpen::Drawings::BaseViewBuilder* builder = NULL;
    try
    {
        if (workPart == NULL)
        {
            return NULL;
        }

        stepStart = std::chrono::steady_clock::now();
        builder = workPart->DraftingViews()->CreateBaseViewBuilder(nullView);
        const long long builderCreateMs = ElapsedMilliseconds(stepStart);
        stepStart = std::chrono::steady_clock::now();
        builder->Scale()->SetNumerator(1.0);
        builder->Scale()->SetDenominator(1.0);
        const long long scaleMs = ElapsedMilliseconds(stepStart);
        long long secondaryMs = 0;
        try
        {
            stepStart = std::chrono::steady_clock::now();
            builder->SecondaryComponents()->SetObjectType(NXOpen::Drawings::DraftingComponentSelectionBuilder::GeometryPrimaryGeometry);
            if (item.part != NULL)
            {
                builder->SecondaryComponents()->SetPart(item.part);
            }
            secondaryMs = ElapsedMilliseconds(stepStart);
        }
        catch (const NXOpen::NXException& ex)
        {
            secondaryMs = ElapsedMilliseconds(stepStart);
            LogLine("[CreateFlatBaseView] set secondary/primary component style ignored code="
                + std::to_string(ex.ErrorCode()) + " message=" + std::string(ex.Message()));
        }
        catch (...)
        {
            secondaryMs = ElapsedMilliseconds(stepStart);
            LogLine("[CreateFlatBaseView] set secondary/primary component style ignored");
        }

        long long viewStylePartMs = 0;
        if (item.part != NULL)
        {
            try
            {
                stepStart = std::chrono::steady_clock::now();
                builder->Style()->ViewStyleBase()->SetPart(item.part);
                builder->Style()->ViewStyleBase()->SetPartName(item.part->FullPath());
                builder->Style()->ViewStyleBase()->Arrangement()->SetSelectedArrangement(NULL);
                builder->Style()->ViewStyleBase()->Arrangement()->SetInheritArrangementFromParent(false);
                viewStylePartMs = ElapsedMilliseconds(stepStart);
                LogLine("[CreateFlatBaseView] view style part set to child part=" + item.partKey);
            }
            catch (const NXOpen::NXException& ex)
            {
                viewStylePartMs = ElapsedMilliseconds(stepStart);
                LogLine("[CreateFlatBaseView] set view style part NXException code=" + std::to_string(ex.ErrorCode())
                    + " message=" + std::string(ex.Message())
                    + " part=" + item.partKey);
            }
            catch (...)
            {
                viewStylePartMs = ElapsedMilliseconds(stepStart);
                LogLine("[CreateFlatBaseView] set view style part exception part=" + item.partKey);
            }
        }

        stepStart = std::chrono::steady_clock::now();
        NXOpen::ModelingView* modelView = FindModelingView(item.part, item.viewName);
        const long long findModelViewMs = ElapsedMilliseconds(stepStart);
        long long selectModelViewMs = 0;
        if (modelView != NULL)
        {
            LogLine("[CreateFlatBaseView] selected model view=" + item.viewName
                + " sourcePart=" + LocaleText(item.part != NULL ? item.part->Name() : NXOpen::NXString("")));
            stepStart = std::chrono::steady_clock::now();
            builder->SelectModelView()->SetSelectedView(modelView);
            selectModelViewMs = ElapsedMilliseconds(stepStart);
        }
        else
        {
            LogLine("[CreateFlatBaseView] model view not found, commit with default");
        }

        stepStart = std::chrono::steady_clock::now();
        builder->Placement()->Placement()->SetValue(NULL, workPart->Views()->WorkView(), point);
        const long long placementMs = ElapsedMilliseconds(stepStart);
        stepStart = std::chrono::steady_clock::now();
        NXOpen::NXObject* object = builder->Commit();
        const long long commitMs = ElapsedMilliseconds(stepStart);
        stepStart = std::chrono::steady_clock::now();
        builder->Destroy();
        const long long destroyMs = ElapsedMilliseconds(stepStart);
        builder = NULL;
        NXOpen::Drawings::BaseView* view = dynamic_cast<NXOpen::Drawings::BaseView*>(object);
        LogLine(std::string("[CreateFlatBaseView] commit ") + (view != NULL ? "ok" : "returned null")
            + " builderCreateMs=" + std::to_string(builderCreateMs)
            + " scaleMs=" + std::to_string(scaleMs)
            + " secondaryMs=" + std::to_string(secondaryMs)
            + " viewStylePartMs=" + std::to_string(viewStylePartMs)
            + " findModelViewMs=" + std::to_string(findModelViewMs)
            + " selectModelViewMs=" + std::to_string(selectModelViewMs)
            + " placementMs=" + std::to_string(placementMs)
            + " commitMs=" + std::to_string(commitMs)
            + " destroyMs=" + std::to_string(destroyMs)
            + " totalMs=" + std::to_string(ElapsedMilliseconds(totalStart)));
        return view;
    }
    catch (const NXOpen::NXException& ex)
    {
        if (builder != NULL)
        {
            try { builder->Destroy(); } catch (...) {}
        }
        LogLine("[CreateFlatBaseView] NXException code=" + std::to_string(ex.ErrorCode())
            + " message=" + std::string(ex.Message())
            + " part=" + item.partKey
            + " viewName=" + item.viewName);
        return NULL;
    }
    catch (...)
    {
        if (builder != NULL)
        {
            try { builder->Destroy(); } catch (...) {}
        }
        LogLine("[CreateFlatBaseView] exception part=" + item.partKey
            + " viewName=" + item.viewName);
        return NULL;
    }
}

static bool RectFromViewCurves(NXOpen::Drawings::BaseView* view, DraftRect& rect)
{
    if (view == NULL)
    {
        return false;
    }

    bool hasPoint = false;
    rect.minX = rect.minY = 1.0e100;
    rect.maxX = rect.maxY = -1.0e100;

    try
    {
        NXOpen::Drawings::DraftingBodyCollection* bodies = view->DraftingBodies();
        for (NXOpen::Drawings::DraftingBodyCollection::iterator bit = bodies->begin(); bit != bodies->end(); ++bit)
        {
            NXOpen::Drawings::DraftingCurveCollection* curves = (*bit)->DraftingCurves();
            for (NXOpen::Drawings::DraftingCurveCollection::iterator cit = curves->begin(); cit != curves->end(); ++cit)
            {
                NXOpen::Drawings::DraftingCurve* curve = *cit;
                if (curve == NULL)
                {
                    continue;
                }

                double box[6] = {};
                if (UF_MODL_ask_bounding_box(curve->Tag(), box) != 0)
                {
                    continue;
                }
                rect.minX = std::min(rect.minX, box[0]);
                rect.minY = std::min(rect.minY, box[1]);
                rect.maxX = std::max(rect.maxX, box[3]);
                rect.maxY = std::max(rect.maxY, box[4]);
                hasPoint = true;
            }
        }
    }
    catch (...)
    {
    }

    if (!hasPoint)
    {
        NXOpen::Drawings::DraftingView* draftingView = dynamic_cast<NXOpen::Drawings::DraftingView*>(view);
        if (draftingView != NULL)
        {
            NXOpen::Point3d center = draftingView->GetDrawingReferencePoint();
            rect.minX = center.X - 25.0;
            rect.maxX = center.X + 25.0;
            rect.minY = center.Y - 20.0;
            rect.maxY = center.Y + 20.0;
            hasPoint = true;
        }
    }
    return hasPoint;
}

static bool RectFromViewBorders(NXOpen::Drawings::BaseView* view, DraftRect& rect)
{
    if (view == NULL)
    {
        return false;
    }

    double borders[4] = {};
    const int status = UF_DRAW_ask_view_borders(view->Tag(), borders);
    if (status != 0)
    {
        LogLine("[RectFromViewBorders] UF_DRAW_ask_view_borders failed status=" + std::to_string(status));
        return false;
    }

    rect.minX = std::min(borders[0], borders[2]);
    rect.minY = std::min(borders[1], borders[3]);
    rect.maxX = std::max(borders[0], borders[2]);
    rect.maxY = std::max(borders[1], borders[3]);
    const double width = rect.maxX - rect.minX;
    const double height = rect.maxY - rect.minY;
    if (!std::isfinite(width) || !std::isfinite(height) || width <= 0.001 || height <= 0.001)
    {
        LogLine("[RectFromViewBorders] invalid borders min=("
            + FormatReal(rect.minX) + "," + FormatReal(rect.minY) + ") max=("
            + FormatReal(rect.maxX) + "," + FormatReal(rect.maxY) + ")");
        return false;
    }

    LogLine("[RectFromViewBorders] drawing min=("
        + FormatReal(rect.minX) + "," + FormatReal(rect.minY) + ") max=("
        + FormatReal(rect.maxX) + "," + FormatReal(rect.maxY)
        + ") width=" + FormatReal(width)
        + " height=" + FormatReal(height));
    return true;
}

static bool RectFromViewBoundary(NXOpen::Drawings::BaseView* view, DraftRect& rect)
{
    if (view == NULL)
    {
        return false;
    }

    NXOpen::Drawings::DraftingView* draftingView = dynamic_cast<NXOpen::Drawings::DraftingView*>(view);
    if (draftingView != NULL)
    {
        try
        {
            draftingView->UpdateAutomaticViewBound(true);
        }
        catch (...)
        {
            LogLine("[RectFromViewBoundary] UpdateAutomaticViewBound failed/ignored");
        }

        try
        {
            NXOpen::Point3d minimumPoint;
            NXOpen::Point3d maximumPoint;
            draftingView->CalculateMinMaxBox(&minimumPoint, &maximumPoint);

            const double minX = std::min(minimumPoint.X, maximumPoint.X);
            const double minY = std::min(minimumPoint.Y, maximumPoint.Y);
            const double maxX = std::max(minimumPoint.X, maximumPoint.X);
            const double maxY = std::max(minimumPoint.Y, maximumPoint.Y);
            const double width = maxX - minX;
            const double height = maxY - minY;
            if (std::isfinite(width) && std::isfinite(height) && width > 0.001 && height > 0.001)
            {
                rect.minX = minX;
                rect.minY = minY;
                rect.maxX = maxX;
                rect.maxY = maxY;
                LogLine("[RectFromViewBoundary] use CalculateMinMaxBox min=("
                    + FormatReal(rect.minX) + "," + FormatReal(rect.minY) + ") max=("
                    + FormatReal(rect.maxX) + "," + FormatReal(rect.maxY) + ")");
                return true;
            }

            LogLine("[RectFromViewBoundary] CalculateMinMaxBox invalid width="
                + FormatReal(width) + " height=" + FormatReal(height));
        }
        catch (...)
        {
            LogLine("[RectFromViewBoundary] CalculateMinMaxBox failed");
        }
    }

    if (RectFromViewCurves(view, rect))
    {
        LogLine("[RectFromViewBoundary] fallback to drafting curves min=("
            + FormatReal(rect.minX) + "," + FormatReal(rect.minY) + ") max=("
            + FormatReal(rect.maxX) + "," + FormatReal(rect.maxY) + ")");
        return true;
    }

    LogLine("[RectFromViewBoundary] no usable boundary");
    return false;
}

static bool RectFromViewBoundaryOnSheet(NXOpen::Drawings::BaseView* view, DraftRect& rect)
{
    DraftRect localRect;
    if (!RectFromViewBoundary(view, localRect))
    {
        return false;
    }

    NXOpen::Drawings::DraftingView* draftingView = dynamic_cast<NXOpen::Drawings::DraftingView*>(view);
    if (draftingView == NULL)
    {
        rect = localRect;
        return true;
    }

    NXOpen::Point3d reference = draftingView->GetDrawingReferencePoint();
    rect.minX = localRect.minX + reference.X;
    rect.maxX = localRect.maxX + reference.X;
    rect.minY = localRect.minY + reference.Y;
    rect.maxY = localRect.maxY + reference.Y;
    LogLine("[RectFromViewBoundaryOnSheet] ref=("
        + FormatReal(reference.X) + "," + FormatReal(reference.Y) + ") sheet min=("
        + FormatReal(rect.minX) + "," + FormatReal(rect.minY) + ") max=("
        + FormatReal(rect.maxX) + "," + FormatReal(rect.maxY) + ")");
    return true;
}

static bool RectFromViewCurvesOnSheet(NXOpen::Drawings::BaseView* view, DraftRect& rect)
{
    if (!RectFromViewCurves(view, rect))
    {
        return false;
    }

    LogLine("[RectFromViewCurvesOnSheet] min=("
        + FormatReal(rect.minX) + "," + FormatReal(rect.minY) + ") max=("
        + FormatReal(rect.maxX) + "," + FormatReal(rect.maxY) + ")");
    return true;
}

static bool RectForLayout(NXOpen::Drawings::BaseView* view, DraftRect& rect)
{
    if (RectFromViewBorders(view, rect))
    {
        return true;
    }

    if (RectFromViewCurvesOnSheet(view, rect))
    {
        LogLine("[RectForLayout] fallback to drafting curves");
        return true;
    }

    if (RectFromViewBoundaryOnSheet(view, rect))
    {
        LogLine("[RectForLayout] fallback to CalculateMinMaxBox");
        return true;
    }

    return false;
}

static double RectWidth(const DraftRect& rect)
{
    return std::max(0.0, rect.maxX - rect.minX);
}

static double RectHeight(const DraftRect& rect)
{
    return std::max(0.0, rect.maxY - rect.minY);
}

static NXOpen::Point3d RectCenterPoint(const DraftRect& rect)
{
    return NXOpen::Point3d((rect.minX + rect.maxX) * 0.5, (rect.minY + rect.maxY) * 0.5, 0.0);
}

static void TranslateRect(DraftRect& rect, double dx, double dy)
{
    rect.minX += dx;
    rect.maxX += dx;
    rect.minY += dy;
    rect.maxY += dy;
}

static bool MapModelToDrawing(NXOpen::Drawings::BaseView* view, const double modelPoint[3], double drawingPoint[2]);
static std::vector<NXOpen::Drawings::DraftingCurve*> CollectDraftingCurves(NXOpen::Drawings::BaseView* view);

static void AccumulateRectPoint(DraftRect& rect, double x, double y, bool& initialized)
{
    if (!initialized)
    {
        rect.minX = x;
        rect.maxX = x;
        rect.minY = y;
        rect.maxY = y;
        initialized = true;
        return;
    }

    rect.minX = std::min(rect.minX, x);
    rect.maxX = std::max(rect.maxX, x);
    rect.minY = std::min(rect.minY, y);
    rect.maxY = std::max(rect.maxY, y);
}

static bool ExpandRectWithCurveInDrawing(NXOpen::Drawings::BaseView* view, NXOpen::Drawings::DraftingCurve* curve, DraftRect& rect, bool& initialized)
{
    if (view == NULL || curve == NULL)
    {
        return false;
    }

    UF_EVAL_p_t evaluator = NULL;
    if (UF_EVAL_initialize(curve->Tag(), &evaluator) != 0 || evaluator == NULL)
    {
        return false;
    }

    bool expanded = false;
    double limits[2] = {};
    if (UF_EVAL_ask_limits(evaluator, limits) == 0)
    {
        const int sampleCount = 32;
        double derivatives[3] = {};
        for (int i = 0; i <= sampleCount; ++i)
        {
            const double t = static_cast<double>(i) / static_cast<double>(sampleCount);
            const double parm = limits[0] + (limits[1] - limits[0]) * t;
            double modelPoint[3] = {};
            if (UF_EVAL_evaluate(evaluator, 0, parm, modelPoint, derivatives) == 0)
            {
                double drawingPoint[2] = {};
                if (MapModelToDrawing(view, modelPoint, drawingPoint))
                {
                    AccumulateRectPoint(rect, drawingPoint[0], drawingPoint[1], initialized);
                    expanded = true;
                }
            }
        }
    }

    UF_EVAL_free(evaluator);
    return expanded;
}

static bool RectFromMappedVisibleCurves(NXOpen::Drawings::BaseView* view, DraftRect& rect)
{
    const std::chrono::steady_clock::time_point totalStart = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point stepStart = totalStart;
    std::vector<NXOpen::Drawings::DraftingCurve*> curves = CollectDraftingCurves(view);
    const long long collectMs = ElapsedMilliseconds(stepStart);
    stepStart = std::chrono::steady_clock::now();
    bool initialized = false;
    size_t visibleCurveCount = 0;
    for (size_t i = 0; i < curves.size(); ++i)
    {
        NXOpen::DisplayableObject* displayableCurve = dynamic_cast<NXOpen::DisplayableObject*>(curves[i]);
        if (displayableCurve != NULL && displayableCurve->IsBlanked())
        {
            continue;
        }
        ++visibleCurveCount;
        ExpandRectWithCurveInDrawing(view, curves[i], rect, initialized);
    }
    const long long scanMs = ElapsedMilliseconds(stepStart);

    if (initialized)
    {
        LogLine("[RectFromMappedVisibleCurves] drawing min=("
            + FormatReal(rect.minX) + "," + FormatReal(rect.minY) + ") max=("
            + FormatReal(rect.maxX) + "," + FormatReal(rect.maxY)
            + ") width=" + FormatReal(RectWidth(rect))
            + " height=" + FormatReal(RectHeight(rect))
            + " curveCount=" + std::to_string(curves.size())
            + " visibleCurveCount=" + std::to_string(visibleCurveCount)
            + " collectMs=" + std::to_string(collectMs)
            + " scanMs=" + std::to_string(scanMs)
            + " totalMs=" + std::to_string(ElapsedMilliseconds(totalStart)));
    }
    else
    {
        LogLine("[RectFromMappedVisibleCurves] no visible curve points"
            + std::string(" curveCount=") + std::to_string(curves.size())
            + " visibleCurveCount=" + std::to_string(visibleCurveCount)
            + " collectMs=" + std::to_string(collectMs)
            + " scanMs=" + std::to_string(scanMs)
            + " totalMs=" + std::to_string(ElapsedMilliseconds(totalStart)));
    }
    return initialized;
}

static void MoveViewBoundaryCenterTo(NXOpen::Drawings::BaseView* view, const NXOpen::Point3d& target)
{
    NXOpen::Drawings::DraftingView* draftingView = dynamic_cast<NXOpen::Drawings::DraftingView*>(view);
    if (draftingView != NULL)
    {
        DraftRect rect;
        if (RectForLayout(view, rect))
        {
            NXOpen::Point3d boundaryCenter = RectCenterPoint(rect);
            NXOpen::Point3d currentReference = draftingView->GetDrawingReferencePoint();
            NXOpen::Point3d newReference(
                currentReference.X + target.X - boundaryCenter.X,
                currentReference.Y + target.Y - boundaryCenter.Y,
                currentReference.Z);
            try
            {
                draftingView->MoveView(newReference);
            }
            catch (const NXOpen::NXException& ex)
            {
                LogLine("[MoveViewBoundaryCenterTo] MoveView failed: " + LocaleText(ex.Message()));
                return;
            }
            catch (...)
            {
                LogLine("[MoveViewBoundaryCenterTo] MoveView failed");
                return;
            }
            LogLine("[MoveViewBoundaryCenterTo] boundaryCenter=("
                + FormatReal(boundaryCenter.X) + "," + FormatReal(boundaryCenter.Y)
                + ") target=(" + FormatReal(target.X) + "," + FormatReal(target.Y)
                + ") delta=(" + FormatReal(target.X - boundaryCenter.X) + "," + FormatReal(target.Y - boundaryCenter.Y)
                + ") newReference=(" + FormatReal(newReference.X) + "," + FormatReal(newReference.Y) + ")");
            return;
        }

        try
        {
            draftingView->MoveView(target);
            LogLine("[MoveViewBoundaryCenterTo] boundary failed, moved reference directly");
        }
        catch (const NXOpen::NXException& ex)
        {
            LogLine("[MoveViewBoundaryCenterTo] fallback MoveView failed: " + LocaleText(ex.Message()));
        }
        catch (...)
        {
            LogLine("[MoveViewBoundaryCenterTo] fallback MoveView failed");
        }
    }
}

static bool MoveViewKnownBoundaryCenterTo(NXOpen::Drawings::BaseView* view, const DraftRect& rect, const NXOpen::Point3d& target, DraftRect* movedRect)
{
    NXOpen::Drawings::DraftingView* draftingView = dynamic_cast<NXOpen::Drawings::DraftingView*>(view);
    if (draftingView == NULL)
    {
        return false;
    }

    NXOpen::Point3d boundaryCenter = RectCenterPoint(rect);
    NXOpen::Point3d currentReference = draftingView->GetDrawingReferencePoint();
    const double dx = target.X - boundaryCenter.X;
    const double dy = target.Y - boundaryCenter.Y;
    NXOpen::Point3d newReference(
        currentReference.X + dx,
        currentReference.Y + dy,
        currentReference.Z);
    double ufReference[2] = { newReference.X, newReference.Y };
    const int moveStatus = UF_DRAW_move_view(view->Tag(), ufReference);
    if (moveStatus != 0)
    {
        LogLine("[MoveViewKnownBoundaryCenterTo] UF_DRAW_move_view failed status=" + std::to_string(moveStatus));
        try
        {
            draftingView->MoveView(newReference);
        }
        catch (const NXOpen::NXException& ex)
        {
            LogLine("[MoveViewKnownBoundaryCenterTo] MoveView failed: " + LocaleText(ex.Message()));
            return false;
        }
        catch (...)
        {
            LogLine("[MoveViewKnownBoundaryCenterTo] MoveView failed");
            return false;
        }
    }

    if (movedRect != NULL)
    {
        *movedRect = rect;
        TranslateRect(*movedRect, dx, dy);
    }

    LogLine("[MoveViewKnownBoundaryCenterTo] boundaryCenter=("
        + FormatReal(boundaryCenter.X) + "," + FormatReal(boundaryCenter.Y)
        + ") target=(" + FormatReal(target.X) + "," + FormatReal(target.Y)
        + ") delta=(" + FormatReal(dx) + "," + FormatReal(dy)
        + ") newReference=(" + FormatReal(newReference.X) + "," + FormatReal(newReference.Y) + ")");
    return true;
}

static NXOpen::Point3d ViewCenter(NXOpen::Drawings::BaseView* view)
{
    NXOpen::Drawings::DraftingView* draftingView = dynamic_cast<NXOpen::Drawings::DraftingView*>(view);
    if (draftingView != NULL)
    {
        return draftingView->GetDrawingReferencePoint();
    }
    return NXOpen::Point3d(0.0, 0.0, 0.0);
}

static void CreateBodyNote(NXOpen::Part* workPart, const FlatPatternItem& item, NXOpen::Drawings::BaseView* view, const NXOpen::Point3d& point, double textSize)
{
    try
    {
        const std::chrono::steady_clock::time_point totalStart = std::chrono::steady_clock::now();
        std::chrono::steady_clock::time_point stepStart = totalStart;
        std::string text = item.noteText.empty() ? BuildBodyNoteText(item.part, item.body, item.quantity) : item.noteText;
        std::string ufInitialText = item.ufNoteText.empty() ? BuildBodyNoteTextForUf(item.part, item.body, item.quantity) : item.ufNoteText;
        const NXOpen::Point3d adjustedPoint = AdjustBodyNotePointForLineCount(point, textSize);
        std::vector<std::string> lineTexts;
        std::string current;
        for (size_t i = 0; i < ufInitialText.size(); ++i)
        {
            const char ch = ufInitialText[i];
            if (ch == '\r')
            {
                continue;
            }
            if (ch == '\n')
            {
                lineTexts.push_back(current);
                current.clear();
                continue;
            }
            current += ch;
        }
        lineTexts.push_back(current.empty() ? ufInitialText : current);
        std::vector<std::string> localeLineTexts;
        localeLineTexts.reserve(lineTexts.size());
        std::vector<char*> linePointers;
        for (size_t i = 0; i < lineTexts.size(); ++i)
        {
            localeLineTexts.push_back(lineTexts[i].empty() ? std::string(" ") : lineTexts[i]);
            linePointers.push_back(const_cast<char*>(localeLineTexts.back().c_str()));
        }
        double origin[3] = { adjustedPoint.X, adjustedPoint.Y, adjustedPoint.Z };
        tag_t createdTag = NULL_TAG;
        const long long prepareMs = ElapsedMilliseconds(stepStart);
        stepStart = std::chrono::steady_clock::now();
        const int createStatus = UF_DRF_create_note(
            static_cast<int>(linePointers.size()),
            linePointers.data(),
            origin,
            0,
            &createdTag);
        const long long createNoteMs = ElapsedMilliseconds(stepStart);
        int type = -1;
        int subtype = -1;
        int attachStatus = -1;
        tag_t attachedViewTag = NULL_TAG;
        bool linkedText = false;
        int editStatus = -1;
        long long typeMs = 0;
        long long attachMs = 0;
        long long managerGetMs = 0;
        long long builderCreateMs = 0;
        long long builderStyleMs = 0;
        long long builderCommitMs = 0;
        long long builderDestroyMs = 0;
        long long displayMs = 0;
        if (createdTag != NULL_TAG)
        {
            stepStart = std::chrono::steady_clock::now();
            UF_OBJ_ask_type_and_subtype(createdTag, &type, &subtype);
            typeMs = ElapsedMilliseconds(stepStart);
            if (view != NULL)
            {
                stepStart = std::chrono::steady_clock::now();
                attachStatus = UF_DRAW_attach_note_to_view(createdTag, view->Tag());
                UF_DRAW_ask_view_of_note(createdTag, &attachedViewTag);
                attachMs = ElapsedMilliseconds(stepStart);
            }
            stepStart = std::chrono::steady_clock::now();
            NXOpen::TaggedObject* tagged = NXOpen::NXObjectManager::Get(createdTag);
            managerGetMs = ElapsedMilliseconds(stepStart);
            NXOpen::Annotations::SimpleDraftingAid* draftingAid = dynamic_cast<NXOpen::Annotations::SimpleDraftingAid*>(tagged);
            if (draftingAid != NULL)
            {
                NXOpen::Annotations::DraftingNoteBuilder* editBuilder = NULL;
                try
                {
                    stepStart = std::chrono::steady_clock::now();
                    editBuilder = workPart->Annotations()->CreateDraftingNoteBuilder(draftingAid);
                    builderCreateMs = ElapsedMilliseconds(stepStart);
                    stepStart = std::chrono::steady_clock::now();
                    editBuilder->Style()->LetteringStyle()->SetGeneralTextSize(textSize);
                    editBuilder->Style()->LetteringStyle()->SetGeneralTextLineSpaceFactor(1.5);
                    try
                    {
                        editBuilder->Style()->LetteringStyle()->SetGeneralTextColor(workPart->Colors()->Find("Emerald"));
                    }
                    catch (...)
                    {
                    }
                    builderStyleMs = ElapsedMilliseconds(stepStart);
                    stepStart = std::chrono::steady_clock::now();
                    NXOpen::NXObject* edited = editBuilder->Commit();
                    builderCommitMs = ElapsedMilliseconds(stepStart);
                    editStatus = edited != NULL ? 0 : 1;
                    stepStart = std::chrono::steady_clock::now();
                    editBuilder->Destroy();
                    builderDestroyMs = ElapsedMilliseconds(stepStart);
                    editBuilder = NULL;
                }
                catch (const NXOpen::NXException& ex)
                {
                    editStatus = ex.ErrorCode();
                    LogLine("[CreateBodyNote] edit linked text NXException code=" + std::to_string(ex.ErrorCode())
                        + " message=" + std::string(ex.Message()));
                    if (editBuilder != NULL)
                    {
                        try { editBuilder->Destroy(); } catch (...) {}
                    }
                }
                catch (...)
                {
                    editStatus = 999999;
                    LogLine("[CreateBodyNote] edit linked text exception");
                    if (editBuilder != NULL)
                    {
                        try { editBuilder->Destroy(); } catch (...) {}
                    }
                }
            }
            if (createdTag != NULL_TAG)
            {
                stepStart = std::chrono::steady_clock::now();
                UF_OBJ_set_layer(createdTag, 1);
                displayMs = ElapsedMilliseconds(stepStart);
            }
        }
        LogLine("[CreateBodyNote] created-by-UF_DRF_create_note status=" + std::to_string(createStatus)
            + " tag=" + std::to_string(createdTag)
            + " type=" + std::to_string(type)
            + " subtype=" + std::to_string(subtype)
            + " attachStatus=" + std::to_string(attachStatus)
            + " attachedViewTag=" + std::to_string(static_cast<unsigned long long>(attachedViewTag))
            + " linkedText=" + std::to_string(linkedText ? 1 : 0)
            + " editStatus=" + std::to_string(editStatus)
            + " timingMs prepare=" + std::to_string(prepareMs)
            + " createNote=" + std::to_string(createNoteMs)
            + " type=" + std::to_string(typeMs)
            + " attach=" + std::to_string(attachMs)
            + " managerGet=" + std::to_string(managerGetMs)
            + " builderCreate=" + std::to_string(builderCreateMs)
            + " builderStyle=" + std::to_string(builderStyleMs)
            + " builderCommit=" + std::to_string(builderCommitMs)
            + " builderDestroy=" + std::to_string(builderDestroyMs)
            + " display=" + std::to_string(displayMs)
            + " total=" + std::to_string(ElapsedMilliseconds(totalStart))
            + " text=" + text
            + " ufInitialText=" + ufInitialText
            + " point=(" + FormatReal(adjustedPoint.X) + "," + FormatReal(adjustedPoint.Y) + ")");
    }
    catch (const NXOpen::NXException& ex)
    {
        LogLine("[CreateBodyNote] NXException code=" + std::to_string(ex.ErrorCode())
            + " message=" + std::string(ex.Message()));
    }
    catch (const std::exception& ex)
    {
        LogLine("[CreateBodyNote] exception message=" + std::string(ex.what()));
    }
    catch (...)
    {
        LogLine("[CreateBodyNote] unknown exception");
    }
}

static std::vector<NXOpen::Drawings::DraftingCurve*> CollectDraftingCurves(NXOpen::Drawings::BaseView* view)
{
    std::vector<NXOpen::Drawings::DraftingCurve*> curves;
    if (view == NULL)
    {
        return curves;
    }
    try
    {
        NXOpen::Drawings::DraftingBodyCollection* bodies = view->DraftingBodies();
        for (NXOpen::Drawings::DraftingBodyCollection::iterator bit = bodies->begin(); bit != bodies->end(); ++bit)
        {
            NXOpen::Drawings::DraftingCurveCollection* bodyCurves = (*bit)->DraftingCurves();
            for (NXOpen::Drawings::DraftingCurveCollection::iterator cit = bodyCurves->begin(); cit != bodyCurves->end(); ++cit)
            {
                curves.push_back(*cit);
            }
        }
    }
    catch (...)
    {
    }
    return curves;
}

static bool IsBendDraftingCurve(NXOpen::Drawings::DraftingCurve* curve, std::string& reason)
{
    reason.clear();
    if (curve == NULL)
    {
        return false;
    }

    try
    {
        if (curve->IsReference())
        {
            reason = "draftingCurveReference";
            return true;
        }
    }
    catch (...)
    {
    }

    try
    {
        const NXOpen::DisplayableObject::ObjectFont font = curve->LineFont();
        if (font != NXOpen::DisplayableObject::ObjectFontSolid)
        {
            reason = "lineFont=" + std::to_string(static_cast<int>(font));
            return true;
        }
    }
    catch (...)
    {
    }

    try
    {
        NXOpen::Drawings::DraftingCurveInfo* info = curve->GetDraftingCurveInfo();
        if (info != NULL)
        {
            std::vector<NXOpen::NXObject*> parents = info->GetParents();
            for (size_t i = 0; i < parents.size(); ++i)
            {
                NXOpen::NXObject* parent = parents[i];
                if (parent == NULL)
                {
                    continue;
                }

                const std::string parentName = LocaleText(parent->Name());
                if (TextLooksLikeBendReference(parentName))
                {
                    reason = "nxParentName=" + parentName;
                    return true;
                }

                tag_t featureTag = NULL_TAG;
                if (UF_MODL_ask_object_feat(parent->Tag(), &featureTag) == 0 && featureTag != NULL_TAG)
                {
                    const std::string featureType = AskUfFeatureTypeText(featureTag);
                    if (TextLooksLikeBendReference(featureType))
                    {
                        reason = "nxParentFeatureType=" + featureType;
                        return true;
                    }
                }
            }
        }
    }
    catch (...)
    {
    }

    int parentCount = 0;
    tag_t* parentTags = NULL;
    if (UF_DRAW_ask_drafting_curve_parents(curve->Tag(), &parentCount, &parentTags) == 0 && parentTags != NULL)
    {
        bool found = false;
        for (int i = 0; i < parentCount && !found; ++i)
        {
            tag_t featureTag = NULL_TAG;
            if (parentTags[i] != NULL_TAG &&
                UF_MODL_ask_object_feat(parentTags[i], &featureTag) == 0 &&
                featureTag != NULL_TAG)
            {
                const std::string featureType = AskUfFeatureTypeText(featureTag);
                if (TextLooksLikeBendReference(featureType))
                {
                    reason = "ufParentFeatureType=" + featureType;
                    found = true;
                }
            }
        }
        UF_free(parentTags);
        if (found)
        {
            return true;
        }
    }

    return false;
}

static void ApplyBendLineVisibility(NXOpen::Drawings::BaseView* view, bool showBendLines)
{
    std::vector<NXOpen::Drawings::DraftingCurve*> curves = CollectDraftingCurves(view);
    int bendCount = 0;
    int hiddenCount = 0;
    for (size_t i = 0; i < curves.size(); ++i)
    {
        NXOpen::Drawings::DraftingCurve* curve = curves[i];
        std::string reason;
        if (!IsBendDraftingCurve(curve, reason))
        {
            continue;
        }

        ++bendCount;
        if (!showBendLines)
        {
            try
            {
                curve->Blank();
                ++hiddenCount;
            }
            catch (const NXOpen::NXException& ex)
            {
                LogLine("[ApplyBendLineVisibility] blank bend curve NXException code="
                    + std::to_string(ex.ErrorCode()) + " message=" + std::string(ex.Message())
                    + " reason=" + reason);
            }
            catch (...)
            {
                LogLine("[ApplyBendLineVisibility] blank bend curve exception reason=" + reason);
            }
        }
    }
    LogLine("[ApplyBendLineVisibility] show=" + std::to_string(showBendLines ? 1 : 0)
        + " curveCount=" + std::to_string(curves.size())
        + " bendCount=" + std::to_string(bendCount)
        + " hiddenCount=" + std::to_string(hiddenCount));
}

static void SetFlatPatternCurveState(
    NXOpen::Drawings::EditViewSettingsBuilder* builder,
    NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectType objectType,
    bool state,
    const std::string& label)
{
    if (builder == NULL)
    {
        return;
    }

    try
    {
        NXOpen::Drawings::ViewStyleFPCurvesBuilder* curveBuilder =
            builder->ViewStyle()->GetViewStyleFPCurve(objectType);
        if (curveBuilder == NULL)
        {
            LogLine("[ApplyFlatPatternBendCurveVisibility] " + label + " builder is null");
            return;
        }

        const bool oldState = curveBuilder->State();
        curveBuilder->SetState(state);
        LogLine("[ApplyFlatPatternBendCurveVisibility] " + label
            + " old=" + std::to_string(oldState ? 1 : 0)
            + " new=" + std::to_string(state ? 1 : 0));
    }
    catch (const NXOpen::NXException& ex)
    {
        LogLine("[ApplyFlatPatternBendCurveVisibility] " + label
            + " NXException code=" + std::to_string(ex.ErrorCode())
            + " message=" + std::string(ex.Message()));
    }
    catch (...)
    {
        LogLine("[ApplyFlatPatternBendCurveVisibility] " + label + " exception");
    }
}

static void SetFlatPatternPreferenceCurveState(
    NXOpen::Drawings::ViewStyleBuilder* viewStyle,
    NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectType objectType,
    bool state,
    const std::string& label)
{
    if (viewStyle == NULL)
    {
        return;
    }

    try
    {
        NXOpen::Drawings::ViewStyleFPCurvesBuilder* curveBuilder =
            viewStyle->GetViewStyleFPCurve(objectType);
        if (curveBuilder == NULL)
        {
            LogLine("[ApplyFlatPatternDefaultBendCurveVisibility] " + label + " builder is null");
            return;
        }

        const bool oldState = curveBuilder->State();
        curveBuilder->SetState(state);
        LogLine("[ApplyFlatPatternDefaultBendCurveVisibility] " + label
            + " old=" + std::to_string(oldState ? 1 : 0)
            + " new=" + std::to_string(state ? 1 : 0));
    }
    catch (const NXOpen::NXException& ex)
    {
        LogLine("[ApplyFlatPatternDefaultBendCurveVisibility] " + label
            + " NXException code=" + std::to_string(ex.ErrorCode())
            + " message=" + std::string(ex.Message()));
    }
    catch (...)
    {
        LogLine("[ApplyFlatPatternDefaultBendCurveVisibility] " + label + " exception");
    }
}

static void LogFlatPatternPreferenceCurveState(
    NXOpen::Drawings::ViewStyleBuilder* viewStyle,
    NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectType objectType,
    const std::string& label,
    const std::string& phase)
{
    if (viewStyle == NULL)
    {
        LogLine("[FlatPatternPreferenceState] " + phase + " " + label + " builder=null");
        return;
    }

    try
    {
        NXOpen::Drawings::ViewStyleFPCurvesBuilder* curveBuilder =
            viewStyle->GetViewStyleFPCurve(objectType);
        if (curveBuilder == NULL)
        {
            LogLine("[FlatPatternPreferenceState] " + phase + " " + label + " builder=null");
            return;
        }
        LogLine("[FlatPatternPreferenceState] " + phase + " " + label
            + " state=" + std::to_string(curveBuilder->State() ? 1 : 0));
    }
    catch (const NXOpen::NXException& ex)
    {
        LogLine("[FlatPatternPreferenceState] " + phase + " " + label
            + " NXException code=" + std::to_string(ex.ErrorCode())
            + " message=" + std::string(ex.Message()));
    }
    catch (...)
    {
        LogLine("[FlatPatternPreferenceState] " + phase + " " + label + " exception");
    }
}

static std::string FlatPatternObjectTypeName(NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectType objectType)
{
    switch (objectType)
    {
    case NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeBendCenterLine:
        return "BendCenterLine";
    case NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeBendUpCenterLine:
        return "BendUpCenterLine";
    case NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeBendDownCenterLine:
        return "BendDownCenterLine";
    case NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeBendTangentLine:
        return "BendTangentLine";
    case NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeOuterMoldLine:
        return "OuterMoldLine";
    case NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeInnerMoldLine:
        return "InnerMoldLine";
    case NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeExteriorCurves:
        return "ExteriorCurves";
    case NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeInteriorCurves:
        return "InteriorCurves";
    case NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeInteriorCutoutCurves:
        return "InteriorCutoutCurves";
    case NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeInteriorFeatureCurves:
        return "InteriorFeatureCurves";
    case NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeLighteningHoleCenter:
        return "LighteningHoleCenter";
    case NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeJoggleLine:
        return "JoggleLine";
    case NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeAddedTopGeometry:
        return "AddedTopGeometry";
    case NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeAddedBottomGeometry:
        return "AddedBottomGeometry";
    case NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeToolMarker:
        return "ToolMarker";
    case NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeHole:
        return "Hole";
    case NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeCentermark:
        return "Centermark";
    default:
        return "Unknown";
    }
}

static void LogAllFlatPatternCurveBuilders(NXOpen::Drawings::ViewStyleBuilder* viewStyle, const std::string& phase)
{
    if (viewStyle == NULL)
    {
        LogLine("[FlatPatternPreferenceAllCurves] " + phase + " viewStyle=null");
        return;
    }

    try
    {
        std::vector<NXOpen::Drawings::ViewStyleFPCurvesBuilder*> curves = viewStyle->GetAllViewStyleFPCurves();
        LogLine("[FlatPatternPreferenceAllCurves] " + phase + " count=" + std::to_string(curves.size()));
        for (size_t i = 0; i < curves.size(); ++i)
        {
            NXOpen::Drawings::ViewStyleFPCurvesBuilder* curve = curves[i];
            if (curve == NULL)
            {
                LogLine("[FlatPatternPreferenceAllCurves] " + phase + " index=" + std::to_string(i) + " builder=null");
                continue;
            }
            NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectType type = curve->Type();
            LogLine("[FlatPatternPreferenceAllCurves] " + phase
                + " index=" + std::to_string(i)
                + " typeInt=" + std::to_string(static_cast<int>(type))
                + " type=" + FlatPatternObjectTypeName(type)
                + " state=" + std::to_string(curve->State() ? 1 : 0)
                + " font=" + std::to_string(static_cast<int>(curve->Font()))
                + " width=" + std::to_string(static_cast<int>(curve->Width())));
        }
    }
    catch (const NXOpen::NXException& ex)
    {
        LogLine("[FlatPatternPreferenceAllCurves] " + phase + " NXException code="
            + std::to_string(ex.ErrorCode()) + " message=" + std::string(ex.Message()));
    }
    catch (...)
    {
        LogLine("[FlatPatternPreferenceAllCurves] " + phase + " exception");
    }
}

static void LogFlatPatternPreferenceStateInUiOrder(NXOpen::Drawings::ViewStyleBuilder* viewStyle, const std::string& phase)
{
    struct FlatPatternCurveLogItem
    {
        NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectType objectType;
        const char* label;
    };

    const FlatPatternCurveLogItem items[] = {
        { NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeBendUpCenterLine, "上折弯中心线/BendUpCenterLine" },
        { NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeBendDownCenterLine, "折弯相切(录制对应)/BendDownCenterLine" },
        { NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeBendTangentLine, "折弯相切/BendTangentLine" },
        { NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeOuterMoldLine, "外模/OuterMoldLine" },
        { NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeInnerMoldLine, "外部轮廓(录制对应)/InnerMoldLine" },
        { NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeExteriorCurves, "ExteriorCurves(NX枚举)" },
        { NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeInteriorCutoutCurves, "内部开孔/InteriorCutoutCurves" },
        { NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeInteriorFeatureCurves, "内部特征/InteriorFeatureCurves" },
        { NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeLighteningHoleCenter, "减轻孔/LighteningHoleCenter" },
        { NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeJoggleLine, "槽接/JoggleLine" },
        { NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeAddedTopGeometry, "添加的顶部/AddedTopGeometry" },
        { NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeAddedBottomGeometry, "添加的底部/AddedBottomGeometry" },
        { NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeToolMarker, "中心线(录制对应)/ToolMarker" },
        { NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeHole, "孔特征/Hole" },
        { NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeCentermark, "中心标记/Centermark" }
    };

    for (size_t i = 0; i < sizeof(items) / sizeof(items[0]); ++i)
    {
        LogFlatPatternPreferenceCurveState(viewStyle, items[i].objectType, items[i].label, phase);
    }
    LogAllFlatPatternCurveBuilders(viewStyle, phase);

    if (viewStyle == NULL)
    {
        LogLine("[FlatPatternPreferenceState] " + phase + " 标注/FPCallouts viewStyle=null");
        LogLine("[FlatPatternPreferenceState] " + phase + " 标注配置/FPCalloutConfig viewStyle=null");
        return;
    }

    try
    {
        std::vector<NXOpen::Drawings::ViewStyleFPCalloutsBuilder*> callouts =
            viewStyle->GetAllViewStyleFPCallouts();
        LogLine("[FlatPatternPreferenceState] " + phase + " 标注/FPCallouts count="
            + std::to_string(callouts.size()));
        for (size_t i = 0; i < callouts.size(); ++i)
        {
            NXOpen::Drawings::ViewStyleFPCalloutsBuilder* callout = callouts[i];
            if (callout == NULL)
            {
                LogLine("[FlatPatternPreferenceState] " + phase + " 标注/FPCallout[" + std::to_string(i) + "] builder=null");
                continue;
            }
            LogLine("[FlatPatternPreferenceState] " + phase + " 标注/FPCallout[" + std::to_string(i)
                + "] type=" + LocaleText(callout->Type())
                + " state=" + std::to_string(callout->State() ? 1 : 0));
        }
    }
    catch (const NXOpen::NXException& ex)
    {
        LogLine("[FlatPatternPreferenceState] " + phase + " 标注/FPCallouts NXException code="
            + std::to_string(ex.ErrorCode()) + " message=" + std::string(ex.Message()));
    }
    catch (...)
    {
        LogLine("[FlatPatternPreferenceState] " + phase + " 标注/FPCallouts exception");
    }

    try
    {
        NXOpen::Drawings::ViewStyleFPCalloutConfigBuilder* config = viewStyle->GetViewStyleFPCalloutConfig();
        if (config == NULL)
        {
            LogLine("[FlatPatternPreferenceState] " + phase + " 标注配置/FPCalloutConfig builder=null");
        }
        else
        {
            LogLine("[FlatPatternPreferenceState] " + phase + " 标注配置/FPCalloutConfig orientation="
                + std::to_string(static_cast<int>(config->GetOrientationType()))
                + " offset=" + FormatReal(config->GetCalloutOffsetDistance()));
        }
    }
    catch (const NXOpen::NXException& ex)
    {
        LogLine("[FlatPatternPreferenceState] " + phase + " 标注配置/FPCalloutConfig NXException code="
            + std::to_string(ex.ErrorCode()) + " message=" + std::string(ex.Message()));
    }
    catch (...)
    {
        LogLine("[FlatPatternPreferenceState] " + phase + " 标注配置/FPCalloutConfig exception");
    }
}

static void ApplyFlatPatternDefaultBendCurveVisibility(NXOpen::Part* workPart, bool showBendLines)
{
    if (workPart == NULL)
    {
        LogLine("[ApplyFlatPatternDefaultBendCurveVisibility] skip null workPart");
        return;
    }

    struct FlatPatternCurveVisibility
    {
        NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectType objectType;
        bool state;
        const char* label;
    };

    NXOpen::Drafting::PreferencesBuilder* builder = NULL;
    try
    {
        builder = workPart->SettingsManager()->CreatePreferencesBuilder();
        NXOpen::Drawings::ViewStyleBuilder* viewStyle = builder != NULL ? builder->ViewStyle() : NULL;
        LogFlatPatternPreferenceStateInUiOrder(viewStyle, "before-set");

        const FlatPatternCurveVisibility bendControlled[] = {
            { NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeBendCenterLine, showBendLines, "BendCenterLine" },
            { NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeBendUpCenterLine, showBendLines, "BendUpCenterLine" }
        };
        for (size_t i = 0; i < sizeof(bendControlled) / sizeof(bendControlled[0]); ++i)
        {
            SetFlatPatternPreferenceCurveState(viewStyle,
                bendControlled[i].objectType,
                bendControlled[i].state,
                bendControlled[i].label);
        }
        LogFlatPatternPreferenceStateInUiOrder(viewStyle, "after-set-before-commit");

        builder->Commit();
        builder->Destroy();
        builder = NULL;

        LogLine("[ApplyFlatPatternDefaultBendCurveVisibility] committed show="
            + std::to_string(showBendLines ? 1 : 0));

        NXOpen::Drafting::PreferencesBuilder* verifyBuilder = NULL;
        try
        {
            verifyBuilder = workPart->SettingsManager()->CreatePreferencesBuilder();
            LogFlatPatternPreferenceStateInUiOrder(verifyBuilder != NULL ? verifyBuilder->ViewStyle() : NULL, "after-commit");
            if (verifyBuilder != NULL)
            {
                verifyBuilder->Destroy();
                verifyBuilder = NULL;
            }
        }
        catch (const NXOpen::NXException& ex)
        {
            LogLine("[ApplyFlatPatternDefaultBendCurveVisibility] verify NXException code="
                + std::to_string(ex.ErrorCode()) + " message=" + std::string(ex.Message()));
            if (verifyBuilder != NULL)
            {
                try { verifyBuilder->Destroy(); } catch (...) {}
            }
        }
        catch (...)
        {
            LogLine("[ApplyFlatPatternDefaultBendCurveVisibility] verify exception");
            if (verifyBuilder != NULL)
            {
                try { verifyBuilder->Destroy(); } catch (...) {}
            }
        }
    }
    catch (const NXOpen::NXException& ex)
    {
        LogLine("[ApplyFlatPatternDefaultBendCurveVisibility] NXException code="
            + std::to_string(ex.ErrorCode()) + " message=" + std::string(ex.Message()));
        if (builder != NULL)
        {
            try { builder->Destroy(); } catch (...) {}
        }
    }
    catch (...)
    {
        LogLine("[ApplyFlatPatternDefaultBendCurveVisibility] exception");
        if (builder != NULL)
        {
            try { builder->Destroy(); } catch (...) {}
        }
    }
}

static void ApplyFlatPatternBendCurveVisibility(
    NXOpen::Part* workPart,
    NXOpen::Drawings::BaseView* view,
    bool showBendLines)
{
    if (workPart == NULL || view == NULL)
    {
        LogLine("[ApplyFlatPatternBendCurveVisibility] skip null workPart/view");
        return;
    }

    NXOpen::Drawings::EditViewSettingsBuilder* builder = NULL;
    try
    {
        std::vector<NXOpen::View*> views(1);
        views[0] = view;
        builder = workPart->SettingsManager()->CreateDrawingEditViewSettingsBuilder(views);

        std::vector<NXOpen::Drafting::BaseEditSettingsBuilder*> editBuilders(1);
        editBuilders[0] = builder;
        workPart->SettingsManager()->ProcessForMultipleObjectsSettings(editBuilders);

        SetFlatPatternCurveState(builder,
            NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeBendCenterLine,
            showBendLines,
            "BendCenterLine");
        SetFlatPatternCurveState(builder,
            NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeBendUpCenterLine,
            showBendLines,
            "BendUpCenterLine");
        SetFlatPatternCurveState(builder,
            NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeBendDownCenterLine,
            showBendLines,
            "BendDownCenterLine");
        SetFlatPatternCurveState(builder,
            NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeExteriorCurves,
            true,
            "ExteriorCurves");
        SetFlatPatternCurveState(builder,
            NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeInteriorCurves,
            true,
            "InteriorCurves");
        SetFlatPatternCurveState(builder,
            NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeInteriorCutoutCurves,
            true,
            "InteriorCutoutCurves");
        SetFlatPatternCurveState(builder,
            NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeInteriorFeatureCurves,
            true,
            "InteriorFeatureCurves");
        SetFlatPatternCurveState(builder,
            NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeLighteningHoleCenter,
            true,
            "LighteningHoleCenter");
        SetFlatPatternCurveState(builder,
            NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeJoggleLine,
            true,
            "JoggleLine");
        SetFlatPatternCurveState(builder,
            NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeAddedTopGeometry,
            true,
            "AddedTopGeometry");
        SetFlatPatternCurveState(builder,
            NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeAddedBottomGeometry,
            true,
            "AddedBottomGeometry");
        SetFlatPatternCurveState(builder,
            NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeToolMarker,
            true,
            "ToolMarker");
        SetFlatPatternCurveState(builder,
            NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeHole,
            true,
            "Hole");
        SetFlatPatternCurveState(builder,
            NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeBendTangentLine,
            false,
            "BendTangentLine");
        SetFlatPatternCurveState(builder,
            NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeOuterMoldLine,
            false,
            "OuterMoldLine");
        SetFlatPatternCurveState(builder,
            NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeInnerMoldLine,
            false,
            "InnerMoldLine");
        SetFlatPatternCurveState(builder,
            NXOpen::SheetMetal::FlatPatternSettings::FlatPatternObjectTypeCentermark,
            false,
            "Centermark");

        builder->Commit();
        builder->Destroy();
        builder = NULL;

        LogLine("[ApplyFlatPatternBendCurveVisibility] committed show="
            + std::to_string(showBendLines ? 1 : 0)
            + " viewTag=" + std::to_string(view->Tag()));
    }
    catch (const NXOpen::NXException& ex)
    {
        LogLine("[ApplyFlatPatternBendCurveVisibility] NXException code="
            + std::to_string(ex.ErrorCode()) + " message=" + std::string(ex.Message()));
        if (builder != NULL)
        {
            try { builder->Destroy(); } catch (...) {}
        }
    }
    catch (...)
    {
        LogLine("[ApplyFlatPatternBendCurveVisibility] exception");
        if (builder != NULL)
        {
            try { builder->Destroy(); } catch (...) {}
        }
    }
}

struct CurveAssocCandidate
{
    NXOpen::Drawings::DraftingCurve* curve;
    NXOpen::InferSnapType::SnapType snapType;
    NXOpen::Point3d modelPoint;
};

static bool MapModelToDrawing(NXOpen::Drawings::BaseView* view, const double modelPoint[3], double drawingPoint[2])
{
    if (view == NULL || modelPoint == NULL || drawingPoint == NULL)
    {
        return false;
    }

    double mutablePoint[3] = { modelPoint[0], modelPoint[1], modelPoint[2] };
    return UF_VIEW_map_model_to_drawing(view->Tag(), mutablePoint, drawingPoint) == 0;
}

static bool FindOverallDimensionCurvePair(
    NXOpen::Drawings::BaseView* view,
    const std::vector<NXOpen::Drawings::DraftingCurve*>& curves,
    const DraftRect& rect,
    bool horizontalDimension,
    CurveAssocCandidate& first,
    CurveAssocCandidate& second)
{
    if (view == NULL || curves.empty())
    {
        return false;
    }

    const double tolerance = std::max(0.2, std::max(RectWidth(rect), RectHeight(rect)) * 0.002);
    bool firstFound = false;
    bool secondFound = false;
    double firstMetric = horizontalDimension ? -1.0e99 : 1.0e99;
    double secondMetric = horizontalDimension ? -1.0e99 : 1.0e99;

    for (size_t i = 0; i < curves.size(); ++i)
    {
        NXOpen::Drawings::DraftingCurve* curve = curves[i];
        if (curve == NULL)
        {
            continue;
        }

        UF_EVAL_p_t evaluator = NULL;
        if (UF_EVAL_initialize(curve->Tag(), &evaluator) != 0 || evaluator == NULL)
        {
            continue;
        }

        bool isLine = false;
        if (UF_EVAL_is_line(evaluator, &isLine) == 0 && isLine)
        {
            UF_CURVE_line_t lineData;
            if (UF_CURVE_ask_line_data(curve->Tag(), &lineData) == 0)
            {
                double p1[2] = {};
                double p2[2] = {};
                if (!MapModelToDrawing(view, lineData.start_point, p1) ||
                    !MapModelToDrawing(view, lineData.end_point, p2))
                {
                    UF_EVAL_free(evaluator);
                    continue;
                }

                const bool verticalLine = fabs(p1[0] - p2[0]) < tolerance;
                const bool horizontalLine = fabs(p1[1] - p2[1]) < tolerance;
                const bool diagonalLine = fabs(p1[0] - p2[0]) > tolerance && fabs(p1[1] - p2[1]) > tolerance;

                if (horizontalDimension)
                {
                    if (verticalLine && fabs(p1[0] - rect.maxX) < tolerance)
                    {
                        const bool useStart = p1[1] > p2[1];
                        const double metric = useStart ? p1[1] : p2[1];
                        if (!firstFound || metric > firstMetric)
                        {
                            firstFound = true;
                            firstMetric = metric;
                            first.curve = curve;
                            first.snapType = useStart ? NXOpen::InferSnapType::SnapTypeStart : NXOpen::InferSnapType::SnapTypeEnd;
                            first.modelPoint = useStart ?
                                NXOpen::Point3d(lineData.start_point[0], lineData.start_point[1], lineData.start_point[2]) :
                                NXOpen::Point3d(lineData.end_point[0], lineData.end_point[1], lineData.end_point[2]);
                        }
                    }
                    else if (!firstFound && diagonalLine)
                    {
                        if (fabs(p1[0] - rect.maxX) < tolerance)
                        {
                            firstFound = true;
                            first.curve = curve;
                            first.snapType = NXOpen::InferSnapType::SnapTypeStart;
                            first.modelPoint = NXOpen::Point3d(lineData.start_point[0], lineData.start_point[1], lineData.start_point[2]);
                        }
                        if (fabs(p2[0] - rect.maxX) < tolerance)
                        {
                            firstFound = true;
                            first.curve = curve;
                            first.snapType = NXOpen::InferSnapType::SnapTypeEnd;
                            first.modelPoint = NXOpen::Point3d(lineData.end_point[0], lineData.end_point[1], lineData.end_point[2]);
                        }
                    }

                    if (verticalLine && fabs(p1[0] - rect.minX) < tolerance)
                    {
                        const bool useStart = p1[1] > p2[1];
                        const double metric = useStart ? p1[1] : p2[1];
                        if (!secondFound || metric > secondMetric)
                        {
                            secondFound = true;
                            secondMetric = metric;
                            second.curve = curve;
                            second.snapType = useStart ? NXOpen::InferSnapType::SnapTypeStart : NXOpen::InferSnapType::SnapTypeEnd;
                            second.modelPoint = useStart ?
                                NXOpen::Point3d(lineData.start_point[0], lineData.start_point[1], lineData.start_point[2]) :
                                NXOpen::Point3d(lineData.end_point[0], lineData.end_point[1], lineData.end_point[2]);
                        }
                    }
                    else if (!secondFound && diagonalLine)
                    {
                        if (fabs(p1[0] - rect.minX) < tolerance)
                        {
                            secondFound = true;
                            second.curve = curve;
                            second.snapType = NXOpen::InferSnapType::SnapTypeStart;
                            second.modelPoint = NXOpen::Point3d(lineData.start_point[0], lineData.start_point[1], lineData.start_point[2]);
                        }
                        if (fabs(p2[0] - rect.minX) < tolerance)
                        {
                            secondFound = true;
                            second.curve = curve;
                            second.snapType = NXOpen::InferSnapType::SnapTypeEnd;
                            second.modelPoint = NXOpen::Point3d(lineData.end_point[0], lineData.end_point[1], lineData.end_point[2]);
                        }
                    }
                }
                else
                {
                    if (horizontalLine && fabs(p1[1] - rect.maxY) < tolerance)
                    {
                        const bool useStart = p1[0] < p2[0];
                        const double metric = useStart ? p1[0] : p2[0];
                        if (!firstFound || metric < firstMetric)
                        {
                            firstFound = true;
                            firstMetric = metric;
                            first.curve = curve;
                            first.snapType = useStart ? NXOpen::InferSnapType::SnapTypeStart : NXOpen::InferSnapType::SnapTypeEnd;
                            first.modelPoint = useStart ?
                                NXOpen::Point3d(lineData.start_point[0], lineData.start_point[1], lineData.start_point[2]) :
                                NXOpen::Point3d(lineData.end_point[0], lineData.end_point[1], lineData.end_point[2]);
                        }
                    }
                    else if (!firstFound && diagonalLine)
                    {
                        if (fabs(p1[1] - rect.maxY) < tolerance)
                        {
                            firstFound = true;
                            first.curve = curve;
                            first.snapType = NXOpen::InferSnapType::SnapTypeStart;
                            first.modelPoint = NXOpen::Point3d(lineData.start_point[0], lineData.start_point[1], lineData.start_point[2]);
                        }
                        if (fabs(p2[1] - rect.maxY) < tolerance)
                        {
                            firstFound = true;
                            first.curve = curve;
                            first.snapType = NXOpen::InferSnapType::SnapTypeEnd;
                            first.modelPoint = NXOpen::Point3d(lineData.end_point[0], lineData.end_point[1], lineData.end_point[2]);
                        }
                    }

                    if (horizontalLine && fabs(p1[1] - rect.minY) < tolerance)
                    {
                        const bool useStart = p1[0] < p2[0];
                        const double metric = useStart ? p1[0] : p2[0];
                        if (!secondFound || metric < secondMetric)
                        {
                            secondFound = true;
                            secondMetric = metric;
                            second.curve = curve;
                            second.snapType = useStart ? NXOpen::InferSnapType::SnapTypeStart : NXOpen::InferSnapType::SnapTypeEnd;
                            second.modelPoint = useStart ?
                                NXOpen::Point3d(lineData.start_point[0], lineData.start_point[1], lineData.start_point[2]) :
                                NXOpen::Point3d(lineData.end_point[0], lineData.end_point[1], lineData.end_point[2]);
                        }
                    }
                    else if (!secondFound && diagonalLine)
                    {
                        if (fabs(p1[1] - rect.minY) < tolerance)
                        {
                            secondFound = true;
                            second.curve = curve;
                            second.snapType = NXOpen::InferSnapType::SnapTypeStart;
                            second.modelPoint = NXOpen::Point3d(lineData.start_point[0], lineData.start_point[1], lineData.start_point[2]);
                        }
                        if (fabs(p2[1] - rect.minY) < tolerance)
                        {
                            secondFound = true;
                            second.curve = curve;
                            second.snapType = NXOpen::InferSnapType::SnapTypeEnd;
                            second.modelPoint = NXOpen::Point3d(lineData.end_point[0], lineData.end_point[1], lineData.end_point[2]);
                        }
                    }
                }
            }
        }
        UF_EVAL_free(evaluator);
    }

    if (firstFound && secondFound)
    {
        return true;
    }

    double firstFallbackMetric = horizontalDimension ? 1.0e99 : -1.0e99;
    double secondFallbackMetric = horizontalDimension ? 1.0e99 : -1.0e99;
    bool firstArcFound = false;
    bool secondArcFound = false;

    for (size_t i = 0; i < curves.size(); ++i)
    {
        NXOpen::Drawings::DraftingCurve* curve = curves[i];
        if (curve == NULL)
        {
            continue;
        }

        UF_EVAL_p_t evaluator = NULL;
        if (UF_EVAL_initialize(curve->Tag(), &evaluator) != 0 || evaluator == NULL)
        {
            continue;
        }

        bool isArc = false;
        if (UF_EVAL_is_arc(evaluator, &isArc) == 0 && isArc)
        {
            UF_EVAL_arc_t arcData;
            if (UF_EVAL_ask_arc(evaluator, &arcData) == 0)
            {
                double center2d[2] = {};
                MapModelToDrawing(view, arcData.center, center2d);
                const double radius2d = arcData.radius;
                const double aMaxX = center2d[0] + radius2d;
                const double aMinX = center2d[0] - radius2d;
                const double aMaxY = center2d[1] + radius2d;
                const double aMinY = center2d[1] - radius2d;

                double limits[2] = {};
                double p1[3] = {};
                double p2[3] = {};
                double d1[2] = {};
                double d2[2] = {};
                if (UF_EVAL_ask_limits(evaluator, limits) == 0)
                {
                    double deriv[9] = {};
                    UF_EVAL_evaluate(evaluator, 0, limits[0], p1, deriv);
                    UF_EVAL_evaluate(evaluator, 0, limits[1], p2, deriv);
                    MapModelToDrawing(view, p1, d1);
                    MapModelToDrawing(view, p2, d2);
                }

                if (horizontalDimension)
                {
                    if ((!firstFound) &&
                        ((fabs(aMaxX - rect.maxX) < tolerance && center2d[1] > firstFallbackMetric) ||
                         fabs(d1[0] - rect.maxX) < tolerance ||
                         fabs(d2[0] - rect.maxX) < tolerance))
                    {
                        firstArcFound = true;
                        firstFallbackMetric = center2d[1];
                        first.curve = curve;
                        first.snapType = NXOpen::InferSnapType::SnapTypeDrfTangent;
                        first.modelPoint = NXOpen::Point3d(999999.0, center2d[1], 0.0);
                    }
                    if ((!secondFound) &&
                        ((fabs(aMinX - rect.minX) < tolerance && center2d[1] > secondFallbackMetric) ||
                         fabs(d1[0] - rect.minX) < tolerance ||
                         fabs(d2[0] - rect.minX) < tolerance))
                    {
                        secondArcFound = true;
                        secondFallbackMetric = center2d[1];
                        second.curve = curve;
                        second.snapType = NXOpen::InferSnapType::SnapTypeDrfTangent;
                        second.modelPoint = NXOpen::Point3d(-999999.0, center2d[1], 0.0);
                    }
                }
                else
                {
                    if ((!firstFound) && fabs(aMaxY - rect.maxY) < tolerance && center2d[0] < firstFallbackMetric)
                    {
                        firstArcFound = true;
                        firstFallbackMetric = center2d[0];
                        first.curve = curve;
                        first.snapType = NXOpen::InferSnapType::SnapTypeDrfTangent;
                        first.modelPoint = NXOpen::Point3d(center2d[0], 999999.0, 0.0);
                    }
                    if ((!secondFound) && fabs(aMinY - rect.minY) < tolerance && center2d[0] < secondFallbackMetric)
                    {
                        secondArcFound = true;
                        secondFallbackMetric = center2d[0];
                        second.curve = curve;
                        second.snapType = NXOpen::InferSnapType::SnapTypeDrfTangent;
                        second.modelPoint = NXOpen::Point3d(center2d[0], -999999.0, 0.0);
                    }
                }
            }
        }
        UF_EVAL_free(evaluator);
    }

    return (firstFound || firstArcFound) && (secondFound || secondArcFound);
}

static bool FindOverallDimensionCurvePairFallback(
    NXOpen::Drawings::BaseView* view,
    const std::vector<NXOpen::Drawings::DraftingCurve*>& curves,
    const DraftRect& rect,
    bool horizontalDimension,
    CurveAssocCandidate& first,
    CurveAssocCandidate& second)
{
    if (view == NULL || curves.empty())
    {
        return false;
    }

    const double tolerance = std::max(0.3, std::max(RectWidth(rect), RectHeight(rect)) * 0.003);
    bool firstFound = false;
    bool secondFound = false;
    double firstScore = -1.0e99;
    double secondScore = -1.0e99;

    for (size_t i = 0; i < curves.size(); ++i)
    {
        NXOpen::Drawings::DraftingCurve* curve = curves[i];
        if (curve == NULL)
        {
            continue;
        }

        UF_EVAL_p_t evaluator = NULL;
        if (UF_EVAL_initialize(curve->Tag(), &evaluator) != 0 || evaluator == NULL)
        {
            continue;
        }

        bool isLine = false;
        if (UF_EVAL_is_line(evaluator, &isLine) == 0 && isLine)
        {
            UF_CURVE_line_t lineData;
            if (UF_CURVE_ask_line_data(curve->Tag(), &lineData) == 0)
            {
                double p1[2] = {};
                double p2[2] = {};
                if (!MapModelToDrawing(view, lineData.start_point, p1) ||
                    !MapModelToDrawing(view, lineData.end_point, p2))
                {
                    UF_EVAL_free(evaluator);
                    continue;
                }

                if (horizontalDimension)
                {
                    if (fabs(p1[0] - rect.maxX) < tolerance || fabs(p2[0] - rect.maxX) < tolerance)
                    {
                        const bool useStart = fabs(p1[0] - rect.maxX) <= fabs(p2[0] - rect.maxX);
                        const double score = useStart ? p1[1] : p2[1];
                        if (!firstFound || score > firstScore)
                        {
                            firstFound = true;
                            firstScore = score;
                            first.curve = curve;
                            first.snapType = useStart ? NXOpen::InferSnapType::SnapTypeStart : NXOpen::InferSnapType::SnapTypeEnd;
                            first.modelPoint = useStart ?
                                NXOpen::Point3d(lineData.start_point[0], lineData.start_point[1], lineData.start_point[2]) :
                                NXOpen::Point3d(lineData.end_point[0], lineData.end_point[1], lineData.end_point[2]);
                        }
                    }
                    if (fabs(p1[0] - rect.minX) < tolerance || fabs(p2[0] - rect.minX) < tolerance)
                    {
                        const bool useStart = fabs(p1[0] - rect.minX) <= fabs(p2[0] - rect.minX);
                        const double score = useStart ? p1[1] : p2[1];
                        if (!secondFound || score > secondScore)
                        {
                            secondFound = true;
                            secondScore = score;
                            second.curve = curve;
                            second.snapType = useStart ? NXOpen::InferSnapType::SnapTypeStart : NXOpen::InferSnapType::SnapTypeEnd;
                            second.modelPoint = useStart ?
                                NXOpen::Point3d(lineData.start_point[0], lineData.start_point[1], lineData.start_point[2]) :
                                NXOpen::Point3d(lineData.end_point[0], lineData.end_point[1], lineData.end_point[2]);
                        }
                    }
                }
                else
                {
                    if (fabs(p1[1] - rect.maxY) < tolerance || fabs(p2[1] - rect.maxY) < tolerance)
                    {
                        const bool useStart = fabs(p1[1] - rect.maxY) <= fabs(p2[1] - rect.maxY);
                        const double score = useStart ? -p1[0] : -p2[0];
                        if (!firstFound || score > firstScore)
                        {
                            firstFound = true;
                            firstScore = score;
                            first.curve = curve;
                            first.snapType = useStart ? NXOpen::InferSnapType::SnapTypeStart : NXOpen::InferSnapType::SnapTypeEnd;
                            first.modelPoint = useStart ?
                                NXOpen::Point3d(lineData.start_point[0], lineData.start_point[1], lineData.start_point[2]) :
                                NXOpen::Point3d(lineData.end_point[0], lineData.end_point[1], lineData.end_point[2]);
                        }
                    }
                    if (fabs(p1[1] - rect.minY) < tolerance || fabs(p2[1] - rect.minY) < tolerance)
                    {
                        const bool useStart = fabs(p1[1] - rect.minY) <= fabs(p2[1] - rect.minY);
                        const double score = useStart ? -p1[0] : -p2[0];
                        if (!secondFound || score > secondScore)
                        {
                            secondFound = true;
                            secondScore = score;
                            second.curve = curve;
                            second.snapType = useStart ? NXOpen::InferSnapType::SnapTypeStart : NXOpen::InferSnapType::SnapTypeEnd;
                            second.modelPoint = useStart ?
                                NXOpen::Point3d(lineData.start_point[0], lineData.start_point[1], lineData.start_point[2]) :
                                NXOpen::Point3d(lineData.end_point[0], lineData.end_point[1], lineData.end_point[2]);
                        }
                    }
                }
            }
        }
        UF_EVAL_free(evaluator);
    }

    return firstFound && secondFound;
}

static bool CreateOverallDimension(NXOpen::Part* workPart, NXOpen::Drawings::BaseView* view, const DraftRect& rect, bool horizontal, double dimensionGap, double dimensionScale)
{
    const std::chrono::steady_clock::time_point totalStart = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point stepStart = totalStart;
    std::vector<NXOpen::Drawings::DraftingCurve*> curves = CollectDraftingCurves(view);
    const long long collectMs = ElapsedMilliseconds(stepStart);
    if (workPart == NULL || view == NULL || curves.empty())
    {
        LogLine(std::string("[CreateOverallDimension] skipped empty inputs direction=")
            + (horizontal ? "H" : "V")
            + " curveCount=" + std::to_string(curves.size())
            + " collectMs=" + std::to_string(collectMs)
            + " totalMs=" + std::to_string(ElapsedMilliseconds(totalStart)));
        return false;
    }

    CurveAssocCandidate first = {};
    CurveAssocCandidate second = {};
    stepStart = std::chrono::steady_clock::now();
    const bool foundPrimary = FindOverallDimensionCurvePair(view, curves, rect, horizontal, first, second);
    const long long findPrimaryMs = ElapsedMilliseconds(stepStart);
    bool usedFallback = false;
    long long findFallbackMs = 0;
    if (!foundPrimary)
    {
        stepStart = std::chrono::steady_clock::now();
        const bool foundFallback = FindOverallDimensionCurvePairFallback(view, curves, rect, horizontal, first, second);
        findFallbackMs = ElapsedMilliseconds(stepStart);
        if (!foundFallback)
        {
            LogLine(std::string("[CreateOverallDimension] no curve pair direction=") + (horizontal ? "H" : "V")
                + " curveCount=" + std::to_string(curves.size())
                + " collectMs=" + std::to_string(collectMs)
                + " findPrimaryMs=" + std::to_string(findPrimaryMs)
                + " findFallbackMs=" + std::to_string(findFallbackMs)
                + " totalMs=" + std::to_string(ElapsedMilliseconds(totalStart)));
            return false;
        }
        usedFallback = true;
    }

    NXOpen::Annotations::Dimension* nullDimension = NULL;
    stepStart = std::chrono::steady_clock::now();
    NXOpen::Annotations::RapidDimensionBuilder* builder = workPart->Dimensions()->CreateRapidDimensionBuilder(nullDimension);
    const long long builderCreateMs = ElapsedMilliseconds(stepStart);
    if (builder == NULL)
    {
        LogLine(std::string("[CreateOverallDimension] builder null direction=") + (horizontal ? "H" : "V")
            + " curveCount=" + std::to_string(curves.size())
            + " collectMs=" + std::to_string(collectMs)
            + " findPrimaryMs=" + std::to_string(findPrimaryMs)
            + " findFallbackMs=" + std::to_string(findFallbackMs)
            + " builderCreateMs=" + std::to_string(builderCreateMs)
            + " totalMs=" + std::to_string(ElapsedMilliseconds(totalStart)));
        return false;
    }

    NXOpen::View* nullView = NULL;
    NXOpen::Point3d assistPoint(0.0, 0.0, 0.0);
    stepStart = std::chrono::steady_clock::now();
    const double styleScale = ClampPositive(dimensionScale, 1.0);
    builder->Style()->DimensionStyle()->SetTextCentered(true);
    builder->Style()->DimensionStyle()->SetNarrowDisplayType(NXOpen::Annotations::NarrowDisplayOptionNone);
    NXOpen::Annotations::LetteringStyleBuilder* letteringStyle = builder->Style()->LetteringStyle();
    if (letteringStyle != NULL)
    {
        const double baseDimensionTextSize = ClampPositive(letteringStyle->DimensionTextSize(), 3.5);
        const double scaledDimensionTextSize = baseDimensionTextSize * styleScale;
        letteringStyle->SetDimensionTextSize(scaledDimensionTextSize);
        letteringStyle->SetAppendedTextSize(scaledDimensionTextSize);
        letteringStyle->SetToleranceTextSize(scaledDimensionTextSize);
        letteringStyle->SetTwoLineToleranceTextSize(scaledDimensionTextSize);
    }
    NXOpen::Annotations::LineArrowStyleBuilder* lineArrowStyle = builder->Style()->LineArrowStyle();
    if (lineArrowStyle != NULL)
    {
        lineArrowStyle->SetArrowheadLength(ClampPositive(lineArrowStyle->ArrowheadLength(), 3.0) * styleScale);
        lineArrowStyle->SetDotArrowheadDiameter(ClampPositive(lineArrowStyle->DotArrowheadDiameter(), 1.0) * styleScale);
        lineArrowStyle->SetTextToLineDistance(ClampPositive(lineArrowStyle->TextToLineDistance(), 1.0) * styleScale);
        lineArrowStyle->SetLinePastArrowDistance(ClampPositive(lineArrowStyle->LinePastArrowDistance(), 1.0) * styleScale);
        lineArrowStyle->SetLinePastArrowDistance2(ClampPositive(lineArrowStyle->LinePastArrowDistance2(), 1.0) * styleScale);
    }
    builder->Measurement()->SetMethod(horizontal ?
        NXOpen::Annotations::DimensionMeasurementBuilder::MeasurementMethodHorizontal :
        NXOpen::Annotations::DimensionMeasurementBuilder::MeasurementMethodVertical);
    const long long styleMs = ElapsedMilliseconds(stepStart);
    stepStart = std::chrono::steady_clock::now();
    builder->FirstAssociativity()->SetValue(first.snapType, first.curve, view, first.modelPoint, NULL, nullView, assistPoint);
    builder->SecondAssociativity()->SetValue(second.snapType, second.curve, view, second.modelPoint, NULL, nullView, assistPoint);
    const long long assocMs = ElapsedMilliseconds(stepStart);

    NXOpen::Point3d origin = horizontal ?
        NXOpen::Point3d(rect.maxX - dimensionGap, rect.maxY + dimensionGap, 0.0) :
        NXOpen::Point3d(rect.minX - dimensionGap, rect.minY - dimensionGap, 0.0);
    stepStart = std::chrono::steady_clock::now();
    builder->Origin()->Origin()->SetValue(NULL, nullView, origin);
    const long long originMs = ElapsedMilliseconds(stepStart);

    try
    {
        stepStart = std::chrono::steady_clock::now();
        builder->Commit();
        const long long commitMs = ElapsedMilliseconds(stepStart);
        stepStart = std::chrono::steady_clock::now();
        builder->Destroy();
        const long long destroyMs = ElapsedMilliseconds(stepStart);
        LogLine(std::string("[CreateOverallDimension] created direction=") + (horizontal ? "H" : "V")
            + " origin=(" + FormatReal(origin.X) + "," + FormatReal(origin.Y) + ")"
            + " dimensionScale=" + FormatReal(styleScale)
            + " curveCount=" + std::to_string(curves.size())
            + " usedFallback=" + std::to_string(usedFallback ? 1 : 0)
            + " collectMs=" + std::to_string(collectMs)
            + " findPrimaryMs=" + std::to_string(findPrimaryMs)
            + " findFallbackMs=" + std::to_string(findFallbackMs)
            + " builderCreateMs=" + std::to_string(builderCreateMs)
            + " styleMs=" + std::to_string(styleMs)
            + " assocMs=" + std::to_string(assocMs)
            + " originMs=" + std::to_string(originMs)
            + " commitMs=" + std::to_string(commitMs)
            + " destroyMs=" + std::to_string(destroyMs)
            + " totalMs=" + std::to_string(ElapsedMilliseconds(totalStart)));
        return true;
    }
    catch (const NXOpen::NXException& ex)
    {
        const long long commitFailMs = ElapsedMilliseconds(stepStart);
        LogLine("[CreateOverallDimension] commit failed direction=" + std::string(horizontal ? "H" : "V")
            + " message=" + LocaleText(ex.Message())
            + " curveCount=" + std::to_string(curves.size())
            + " collectMs=" + std::to_string(collectMs)
            + " findPrimaryMs=" + std::to_string(findPrimaryMs)
            + " findFallbackMs=" + std::to_string(findFallbackMs)
            + " builderCreateMs=" + std::to_string(builderCreateMs)
            + " styleMs=" + std::to_string(styleMs)
            + " assocMs=" + std::to_string(assocMs)
            + " originMs=" + std::to_string(originMs)
            + " commitFailMs=" + std::to_string(commitFailMs)
            + " totalMs=" + std::to_string(ElapsedMilliseconds(totalStart)));
        builder->Destroy();
        return false;
    }
    catch (...)
    {
        const long long commitFailMs = ElapsedMilliseconds(stepStart);
        LogLine("[CreateOverallDimension] commit failed direction=" + std::string(horizontal ? "H" : "V")
            + " curveCount=" + std::to_string(curves.size())
            + " collectMs=" + std::to_string(collectMs)
            + " findPrimaryMs=" + std::to_string(findPrimaryMs)
            + " findFallbackMs=" + std::to_string(findFallbackMs)
            + " builderCreateMs=" + std::to_string(builderCreateMs)
            + " styleMs=" + std::to_string(styleMs)
            + " assocMs=" + std::to_string(assocMs)
            + " originMs=" + std::to_string(originMs)
            + " commitFailMs=" + std::to_string(commitFailMs)
            + " totalMs=" + std::to_string(ElapsedMilliseconds(totalStart)));
        builder->Destroy();
        return false;
    }
}

static void DoUpdateNow(const char* reason)
{
    try
    {
        Session* session = Session::GetSession();
        const std::chrono::steady_clock::time_point updateStart = std::chrono::steady_clock::now();
        Session::UndoMarkId mark = session->SetUndoMark(Session::MarkVisibilityInvisible, reason != NULL ? reason : "PILianDaoCuZKT update");
        session->UpdateManager()->DoUpdate(mark);
        LogLine(std::string("[DoUpdateNow] update ok reason=")
            + (reason != NULL ? reason : "")
            + " ms=" + std::to_string(ElapsedMilliseconds(updateStart)));
    }
    catch (...)
    {
        LogLine(std::string("[DoUpdateNow] update failed/ignored: ") + (reason != NULL ? reason : ""));
    }
}

class UfSuppressViewUpdateGuard
{
public:
    UfSuppressViewUpdateGuard()
        : oldSuppressViewUpdate_(false),
          active_(false)
    {
        logical oldValue = false;
        const int askStatus = UF_DRAW_ask_suppress_view_updat(&oldValue);
        if (askStatus != 0)
        {
            LogLine("[UfSuppressViewUpdateGuard] ask failed status=" + std::to_string(askStatus));
            return;
        }

        oldSuppressViewUpdate_ = oldValue;
        const int setStatus = UF_DRAW_set_suppress_view_updat(true);
        if (setStatus == 0)
        {
            active_ = true;
            LogLine("[UfSuppressViewUpdateGuard] enabled old="
                + std::to_string(oldSuppressViewUpdate_ ? 1 : 0));
        }
        else
        {
            LogLine("[UfSuppressViewUpdateGuard] enable failed status=" + std::to_string(setStatus));
        }
    }

    ~UfSuppressViewUpdateGuard()
    {
        Restore();
    }

    void Restore()
    {
        if (!active_)
        {
            return;
        }

        const int restoreStatus = UF_DRAW_set_suppress_view_updat(oldSuppressViewUpdate_);
        LogLine("[UfSuppressViewUpdateGuard] restored status="
            + std::to_string(restoreStatus)
            + " value=" + std::to_string(oldSuppressViewUpdate_ ? 1 : 0));
        active_ = false;
    }

private:
    logical oldSuppressViewUpdate_;
    bool active_;
};

class DraftingDelayUpdateGuard
{
public:
    explicit DraftingDelayUpdateGuard(NXOpen::Part* part)
        : part_(part),
          drafting_(NULL),
          oldDelayViewUpdate_(false),
          oldDelayUpdateOnCreation_(false),
          oldUpdateViewWithoutLwData_(NXOpen::Preferences::PartDrafting::UpdateViewWithoutLwDataOptionIgnore),
          active_(false)
    {
        try
        {
            drafting_ = (part_ != NULL && part_->Preferences() != NULL) ? part_->Preferences()->Drafting() : NULL;
            if (drafting_ == NULL)
            {
                LogLine("[DraftingDelayUpdateGuard] drafting preferences unavailable");
                return;
            }

            oldDelayViewUpdate_ = drafting_->DelayViewUpdate();
            oldDelayUpdateOnCreation_ = drafting_->DelayUpdateOnCreation();
            oldUpdateViewWithoutLwData_ = drafting_->UpdateViewWithoutLwData();
            drafting_->SetDelayViewUpdate(false);
            drafting_->SetDelayUpdateOnCreation(true);
            drafting_->SetUpdateViewWithoutLwData(NXOpen::Preferences::PartDrafting::UpdateViewWithoutLwDataOptionGenerate);
            active_ = true;
            LogLine("[DraftingDelayUpdateGuard] enabled oldDelayViewUpdate="
                + std::to_string(oldDelayViewUpdate_ ? 1 : 0)
                + " oldDelayUpdateOnCreation=" + std::to_string(oldDelayUpdateOnCreation_ ? 1 : 0)
                + " oldUpdateViewWithoutLwData=" + std::to_string(static_cast<int>(oldUpdateViewWithoutLwData_))
                + " newDelayViewUpdate=0 newDelayUpdateOnCreation=1 newUpdateViewWithoutLwData=Generate");
        }
        catch (const NXOpen::NXException& ex)
        {
            LogLine("[DraftingDelayUpdateGuard] enable NXException code=" + std::to_string(ex.ErrorCode())
                + " message=" + std::string(ex.Message()));
        }
        catch (...)
        {
            LogLine("[DraftingDelayUpdateGuard] enable exception");
        }
    }

    ~DraftingDelayUpdateGuard()
    {
        Restore();
    }

    void Restore()
    {
        if (!active_ || drafting_ == NULL)
        {
            return;
        }

        try
        {
            LogLine("[DraftingDelayUpdateGuard] final preferences left unchanged by request"
                + std::string(" delayViewUpdate=") + std::to_string(drafting_->DelayViewUpdate() ? 1 : 0)
                + " delayUpdateOnCreation=" + std::to_string(drafting_->DelayUpdateOnCreation() ? 1 : 0)
                + " updateViewWithoutLwData=" + std::to_string(static_cast<int>(drafting_->UpdateViewWithoutLwData()))
                + " oldDelayViewUpdate=" + std::to_string(oldDelayViewUpdate_ ? 1 : 0)
                + " oldDelayUpdateOnCreation=" + std::to_string(oldDelayUpdateOnCreation_ ? 1 : 0)
                + " oldUpdateViewWithoutLwData=" + std::to_string(static_cast<int>(oldUpdateViewWithoutLwData_)));
        }
        catch (...)
        {
            LogLine("[DraftingDelayUpdateGuard] final preferences left unchanged by request, read current state failed");
        }
        active_ = false;
    }

private:
    NXOpen::Part* part_;
    NXOpen::Preferences::PartDrafting* drafting_;
    bool oldDelayViewUpdate_;
    bool oldDelayUpdateOnCreation_;
    NXOpen::Preferences::PartDrafting::UpdateViewWithoutLwDataOption oldUpdateViewWithoutLwData_;
    bool active_;
};

static void UpdateOneDraftingView(NXOpen::Part* workPart, NXOpen::Drawings::BaseView* view, const std::string& reason)
{
    if (view == NULL)
    {
        return;
    }

    const std::chrono::steady_clock::time_point updateStart = std::chrono::steady_clock::now();
    NXOpen::Drawings::DraftingView* draftingView = dynamic_cast<NXOpen::Drawings::DraftingView*>(view);
    if (draftingView != NULL)
    {
        try
        {
            draftingView->Update();
            LogLine("[UpdateOneDraftingView] NXOpen ok reason=" + reason
                + " viewTag=" + std::to_string(static_cast<unsigned long long>(view->Tag()))
                + " ms=" + std::to_string(ElapsedMilliseconds(updateStart)));
            return;
        }
        catch (const NXOpen::NXException& ex)
        {
            LogLine("[UpdateOneDraftingView] NXOpen failed reason=" + reason
                + " code=" + std::to_string(ex.ErrorCode())
                + " message=" + std::string(ex.Message()));
        }
        catch (...)
        {
            LogLine("[UpdateOneDraftingView] NXOpen failed reason=" + reason);
        }
    }

    try
    {
        NXOpen::Drawings::DraftingDrawingSheet* sheet =
            (workPart != NULL && workPart->DraftingDrawingSheets() != NULL) ?
            workPart->DraftingDrawingSheets()->CurrentDrawingSheet() : NULL;
        const int status = (sheet != NULL) ? UF_DRAW_update_one_view(sheet->Tag(), view->Tag()) : -1;
        LogLine("[UpdateOneDraftingView] UF reason=" + reason
            + " status=" + std::to_string(status)
            + " sheetTag=" + std::to_string(static_cast<unsigned long long>(sheet != NULL ? sheet->Tag() : NULL_TAG))
            + " viewTag=" + std::to_string(static_cast<unsigned long long>(view->Tag()))
            + " ms=" + std::to_string(ElapsedMilliseconds(updateStart)));
    }
    catch (...)
    {
        LogLine("[UpdateOneDraftingView] UF exception reason=" + reason
            + " viewTag=" + std::to_string(static_cast<unsigned long long>(view->Tag()))
            + " ms=" + std::to_string(ElapsedMilliseconds(updateStart)));
    }
}

static void DeleteTaggedObjects(const std::vector<NXOpen::TaggedObject*>& objects, const std::string& reason)
{
    if (objects.empty())
    {
        return;
    }

    try
    {
        Session* session = Session::GetSession();
        NXOpen::Update* update = session->UpdateManager();
        update->ClearDeleteList();
        const int addErrors = update->AddObjectsToDeleteList(objects);
        Session::UndoMarkId mark = session->SetUndoMark(Session::MarkVisibilityInvisible, reason.c_str());
        const int updateErrors = update->DoUpdate(mark);
        LogLine("[DeleteTaggedObjects] reason=" + reason
            + " count=" + std::to_string(objects.size())
            + " addErrors=" + std::to_string(addErrors)
            + " updateErrors=" + std::to_string(updateErrors));
    }
    catch (const NXOpen::NXException& ex)
    {
        LogLine("[DeleteTaggedObjects] NXException reason=" + reason
            + " code=" + std::to_string(ex.ErrorCode())
            + " message=" + std::string(ex.Message()));
    }
    catch (...)
    {
        LogLine("[DeleteTaggedObjects] exception reason=" + reason);
    }
}

struct LayoutBatchResult
{
    std::vector<FlatPatternItem> overflowItems;
    std::vector<FlatPatternCreateFailure> failures;
    double nextY;
    size_t createdCount;
    bool placedAny;
};

static LayoutBatchResult LayoutAndAnnotateViews(NXOpen::Part* workPart, const std::vector<FlatPatternItem>& items, const CommandOptions& options, double startY)
{
    LayoutBatchResult result;
    result.nextY = startY;
    result.createdCount = 0;
    result.placedAny = false;
    LogLine("[LayoutAndAnnotateViews] begin itemCount=" + std::to_string(items.size())
        + " startY=" + FormatReal(startY));
    struct CreatedView
    {
        FlatPatternItem item;
        NXOpen::Drawings::BaseView* view;
        DraftRect rect;
        double width;
        double height;
        std::string material;
        std::string thickness;
        std::string layoutKey;
        bool valid;
        bool placed;
    };

    const double margin = kSheetMargin;
    const double dimensionScale = ClampPositive(options.viewScaleDenominator, 1.0);
    const double dimensionGap = options.annotateMaxDimension ? kDimensionPlacementGap * dimensionScale : 0.0;
    const double dimensionTopReserve = options.annotateMaxDimension ? dimensionGap + 14.0 * dimensionScale : 0.0;
    const double dimensionRightReserve = options.annotateMaxDimension ? dimensionGap + 21.0 * dimensionScale : 0.0;
    const double noteGap = std::max(2.0, options.noteTextSize * 1.5);
    const double noteReserve = noteGap + options.noteTextSize * 4.0;
    const double usableWidth = options.sheetWidth - margin * 2.0;

    struct PlannedView
    {
        FlatPatternItem item;
        double width;
        double height;
        double blockWidth;
        double blockHeight;
        size_t rowIndex;
        NXOpen::Point3d center;
        bool placed;
    };

    std::vector<PlannedView> plannedViews;
    plannedViews.reserve(items.size());
    for (size_t i = 0; i < items.size(); ++i)
    {
        PlannedView planned;
        planned.item = items[i];
        planned.width = items[i].hasFlatSize ? std::max(1.0, items[i].flatWidth) : 50.0;
        planned.height = items[i].hasFlatSize ? std::max(1.0, items[i].flatHeight) : 40.0;
        planned.blockWidth = planned.width + dimensionRightReserve;
        planned.blockHeight = dimensionTopReserve + planned.height + noteReserve;
        planned.rowIndex = static_cast<size_t>(-1);
        planned.center = NXOpen::Point3d(0.0, 0.0, 0.0);
        planned.placed = false;
        plannedViews.push_back(planned);
        LogLine("[LayoutAndAnnotateViews] planned index=" + std::to_string(i)
            + " hasFlatSize=" + std::to_string(items[i].hasFlatSize ? 1 : 0)
            + " minimumX=" + FormatReal(items[i].flatWidth)
            + " minimumY=" + FormatReal(items[i].flatHeight)
            + " estWidth=" + FormatReal(planned.width)
            + " estHeight=" + FormatReal(planned.height)
            + " layoutKey=" + items[i].layoutKey);
    }

    LogLine("[LayoutAndAnnotateViews] phase1 calculate final view positions before creation");

    struct LayoutRow
    {
        std::string key;
        std::string material;
        std::string thickness;
        std::vector<size_t> indices;
        double totalWidth;
        double maxBlockHeight;
    };

    std::map<std::string, std::vector<size_t> > groupedIndices;
    for (size_t i = 0; i < plannedViews.size(); ++i)
    {
        const std::string key = plannedViews[i].item.layoutKey.empty()
            ? BodyLayoutKey(plannedViews[i].item.material, plannedViews[i].item.thickness)
            : plannedViews[i].item.layoutKey;
        groupedIndices[key].push_back(i);
    }

    std::vector<LayoutRow> rows;
    for (std::map<std::string, std::vector<size_t> >::const_iterator git = groupedIndices.begin(); git != groupedIndices.end(); ++git)
    {
        std::vector<size_t> sortedIndices = git->second;
        std::sort(sortedIndices.begin(), sortedIndices.end(), [&plannedViews](size_t a, size_t b) {
            const double sortYA = plannedViews[a].item.hasFlatSize ? plannedViews[a].item.flatHeight : plannedViews[a].height;
            const double sortYB = plannedViews[b].item.hasFlatSize ? plannedViews[b].item.flatHeight : plannedViews[b].height;
            if (fabs(sortYA - sortYB) > 1.0e-6)
            {
                return sortYA > sortYB;
            }
            const double sortXA = plannedViews[a].item.hasFlatSize ? plannedViews[a].item.flatWidth : plannedViews[a].width;
            const double sortXB = plannedViews[b].item.hasFlatSize ? plannedViews[b].item.flatWidth : plannedViews[b].width;
            if (fabs(sortXA - sortXB) > 1.0e-6)
            {
                return sortXA > sortXB;
            }
            return a < b;
        });

        LayoutRow row;
        row.key = git->first;
        row.totalWidth = 0.0;
        row.maxBlockHeight = 0.0;

        for (size_t j = 0; j < sortedIndices.size(); ++j)
        {
            const size_t viewIndex = sortedIndices[j];
            const double blockWidth = plannedViews[viewIndex].blockWidth;
            const double blockHeight = plannedViews[viewIndex].blockHeight;
            const double nextTotalWidth = row.indices.empty() ? blockWidth : row.totalWidth + options.viewSpacing + blockWidth;

            if (!row.indices.empty() && nextTotalWidth > usableWidth)
            {
                LogLine("[LayoutAndAnnotateViews] split material/thickness row because width exceeds sheet key="
                    + row.key + " rowWidth=" + FormatReal(row.totalWidth)
                    + " nextBlockWidth=" + FormatReal(blockWidth)
                    + " usableWidth=" + FormatReal(usableWidth));
                rows.push_back(row);
                row.indices.clear();
                row.totalWidth = 0.0;
                row.maxBlockHeight = 0.0;
            }

            if (row.indices.empty())
            {
                row.material = plannedViews[viewIndex].item.material;
                row.thickness = plannedViews[viewIndex].item.thickness;
                row.totalWidth = blockWidth;
            }
            else
            {
                row.totalWidth += options.viewSpacing + blockWidth;
            }
            row.indices.push_back(viewIndex);
            row.maxBlockHeight = std::max(row.maxBlockHeight, blockHeight);
        }

        if (!row.indices.empty())
        {
            rows.push_back(row);
        }
    }

    LogLine("[LayoutAndAnnotateViews] phase2 assign direct create positions rowCount=" + std::to_string(rows.size()));
    double y = startY;
    const bool freshSheetCursor = startY >= options.sheetHeight - margin - 1.0e-6;
    for (size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex)
    {
        const LayoutRow& row = rows[rowIndex];
        if (y - row.maxBlockHeight < margin)
        {
            if (rowIndex > 0 || !freshSheetCursor)
            {
                LogLine("[PILianDaoCuZKT] sheet is full, remaining rows overflow to next sheet row=" + std::to_string(rowIndex)
                    + " material=" + row.material + " thickness=" + row.thickness
                    + " y=" + FormatReal(y)
                    + " rowHeight=" + FormatReal(row.maxBlockHeight));
                break;
            }

            LogLine("[PILianDaoCuZKT] first row exceeds sheet height, force placing to avoid endless overflow rowHeight="
                + FormatReal(row.maxBlockHeight));
        }

        LogLine("[LayoutAndAnnotateViews] row=" + std::to_string(rowIndex)
            + " material=" + row.material
            + " thickness=" + row.thickness
            + " count=" + std::to_string(row.indices.size())
            + " rowWidth=" + FormatReal(row.totalWidth)
            + " rowHeight=" + FormatReal(row.maxBlockHeight));

        double x = margin;
        for (size_t j = 0; j < row.indices.size(); ++j)
        {
            const size_t i = row.indices[j];
            const double width = plannedViews[i].width;
            const double height = plannedViews[i].height;
            const double blockWidth = plannedViews[i].blockWidth;
            const double blockHeight = plannedViews[i].blockHeight;

            const double viewLeft = x;
            const double viewTop = y - dimensionTopReserve;
            NXOpen::Point3d center(viewLeft + width * 0.5, viewTop - height * 0.5, 0.0);
            plannedViews[i].center = center;
            plannedViews[i].rowIndex = rowIndex;
            plannedViews[i].placed = true;
            result.placedAny = true;
            LogLine("[LayoutAndAnnotateViews] planned position index=" + std::to_string(i)
                + " row=" + std::to_string(rowIndex)
                + " center=(" + FormatReal(center.X) + "," + FormatReal(center.Y)
                + ") blockWidth=" + FormatReal(blockWidth)
                + " blockHeight=" + FormatReal(blockHeight)
                + " rowHeight=" + FormatReal(row.maxBlockHeight));

            x += blockWidth + options.viewSpacing;
        }

        y -= row.maxBlockHeight + options.rowSpacing;
    }
    result.nextY = y;

    std::vector<CreatedView> createdViews;
    createdViews.reserve(items.size());
    LogLine("[LayoutAndAnnotateViews] phase3 create placed views directly at final positions");
    for (size_t i = 0; i < plannedViews.size(); ++i)
    {
        if (!plannedViews[i].placed)
        {
            result.overflowItems.push_back(plannedViews[i].item);
            LogLine("[LayoutAndAnnotateViews] overflow item index=" + std::to_string(i)
                + " part=" + plannedViews[i].item.partKey
                + " view=" + plannedViews[i].item.viewName);
            continue;
        }

        LogLine("[LayoutAndAnnotateViews] create item index=" + std::to_string(i)
            + " part=" + LocaleText(plannedViews[i].item.part != NULL ? plannedViews[i].item.part->Name() : NXOpen::NXString(""))
            + " view=" + plannedViews[i].item.viewName
            + " center=(" + FormatReal(plannedViews[i].center.X) + "," + FormatReal(plannedViews[i].center.Y) + ")");
        NXOpen::Drawings::BaseView* view = CreateFlatBaseView(workPart, plannedViews[i].item, options, plannedViews[i].center);
        if (view == NULL)
        {
            LogLine("[PILianDaoCuZKT] create view failed: " + plannedViews[i].item.partKey + " view=" + plannedViews[i].item.viewName);
            FlatPatternCreateFailure failure;
            failure.partKey = plannedViews[i].item.partKey;
            failure.viewName = plannedViews[i].item.viewName;
            result.failures.push_back(failure);
            const double shift = plannedViews[i].blockWidth + options.viewSpacing;
            for (size_t j = i + 1; j < plannedViews.size(); ++j)
            {
                if (plannedViews[j].placed && plannedViews[j].rowIndex == plannedViews[i].rowIndex)
                {
                    plannedViews[j].center.X -= shift;
                    LogLine("[LayoutAndAnnotateViews] compact row after failed view failedIndex="
                        + std::to_string(i)
                        + " shiftedIndex=" + std::to_string(j)
                        + " shift=" + FormatReal(shift)
                        + " newCenter=(" + FormatReal(plannedViews[j].center.X)
                        + "," + FormatReal(plannedViews[j].center.Y) + ")");
                }
            }
            continue;
        }
        CreatedView created;
        created.item = plannedViews[i].item;
        created.view = view;
        created.width = plannedViews[i].width;
        created.height = plannedViews[i].height;
        created.material = plannedViews[i].item.material;
        created.thickness = plannedViews[i].item.thickness;
        created.layoutKey = plannedViews[i].item.layoutKey.empty() ? BodyLayoutKey(created.material, created.thickness) : plannedViews[i].item.layoutKey;
        created.valid = false;
        created.placed = true;
        created.rect.minX = plannedViews[i].center.X - plannedViews[i].width * 0.5;
        created.rect.maxX = plannedViews[i].center.X + plannedViews[i].width * 0.5;
        created.rect.minY = plannedViews[i].center.Y - plannedViews[i].height * 0.5;
        created.rect.maxY = plannedViews[i].center.Y + plannedViews[i].height * 0.5;
        createdViews.push_back(created);
        ++result.createdCount;
    }

    if (createdViews.empty())
    {
        LogLine("[LayoutAndAnnotateViews] no views created on this sheet");
        return result;
    }

    LogLine("[LayoutAndAnnotateViews] phase3a measure directly created view rectangles");
    for (size_t i = 0; i < createdViews.size(); ++i)
    {
        DraftRect rect;
        const bool hasRect = RectForLayout(createdViews[i].view, rect);
        if (hasRect)
        {
            createdViews[i].rect = rect;
            createdViews[i].width = std::max(1.0, RectWidth(rect));
            createdViews[i].height = std::max(1.0, RectHeight(rect));
        }
        createdViews[i].valid = hasRect;
        LogLine("[LayoutAndAnnotateViews] measured direct index=" + std::to_string(i)
            + " valid=" + std::to_string(hasRect ? 1 : 0)
            + " width=" + FormatReal(createdViews[i].width)
            + " height=" + FormatReal(createdViews[i].height)
            + " itemMinimumY=" + FormatReal(createdViews[i].item.flatHeight));
    }

    LogLine("[LayoutAndAnnotateViews] skip layout move/update; views created at final positions");

    LogLine("[LayoutAndAnnotateViews] phase3b reapply bend line visibility after layout show="
        + std::to_string(options.showBendLines ? 1 : 0));
    LogLine("[LayoutAndAnnotateViews] phase3b skipped per-view bend line reapply; using drafting view defaults");

    LogLine("[LayoutAndAnnotateViews] phase4 annotate and dimension after final layout");
    for (size_t i = 0; i < createdViews.size(); ++i)
    {
        if (!createdViews[i].placed)
        {
            LogLine("[LayoutAndAnnotateViews] skip annotation for unplaced view index=" + std::to_string(i)
                + " part=" + createdViews[i].item.partKey);
            continue;
        }

        DraftRect rect;
        if (!RectForLayout(createdViews[i].view, rect))
        {
            rect = createdViews[i].rect;
            LogLine("[LayoutAndAnnotateViews] final rect fallback index=" + std::to_string(i));
        }

        DraftRect displayedRect = rect;
        const bool hasDisplayedRect = RectFromMappedVisibleCurves(createdViews[i].view, displayedRect);
        if (!hasDisplayedRect)
        {
            LogLine("[LayoutAndAnnotateViews] displayed curve rect fallback to view border index=" + std::to_string(i));
        }

        if (options.annotateMaxDimension)
        {
            LogLine("[LayoutAndAnnotateViews] create max dimensions index=" + std::to_string(i));
            std::chrono::steady_clock::time_point dimensionStart = std::chrono::steady_clock::now();
            CreateOverallDimension(workPart, createdViews[i].view, displayedRect, true, dimensionGap, dimensionScale);
            const long long horizontalDimensionMs = ElapsedMilliseconds(dimensionStart);
            dimensionStart = std::chrono::steady_clock::now();
            CreateOverallDimension(workPart, createdViews[i].view, displayedRect, false, dimensionGap, dimensionScale);
            const long long verticalDimensionMs = ElapsedMilliseconds(dimensionStart);
            LogLine("[LayoutAndAnnotateViews] dimension timing index=" + std::to_string(i)
                + " horizontalMs=" + std::to_string(horizontalDimensionMs)
                + " verticalMs=" + std::to_string(verticalDimensionMs));
        }

        const double bodyNoteGap = dimensionGap + std::max(noteGap, options.noteTextSize * 2.0);
        NXOpen::Point3d notePoint((displayedRect.minX + displayedRect.maxX) * 0.5, displayedRect.minY - bodyNoteGap, 0.0);
        std::chrono::steady_clock::time_point noteStart = std::chrono::steady_clock::now();
        CreateBodyNote(workPart, createdViews[i].item, createdViews[i].view, notePoint, options.noteTextSize);
        LogLine("[LayoutAndAnnotateViews] note index=" + std::to_string(i)
            + " point=(" + FormatReal(notePoint.X) + "," + FormatReal(notePoint.Y)
            + ") displayedMinY=" + FormatReal(displayedRect.minY)
            + " bodyNoteGap=" + FormatReal(bodyNoteGap)
            + " noteMs=" + std::to_string(ElapsedMilliseconds(noteStart)));
    }
    LogLine("[LayoutAndAnnotateViews] skip annotation update; final update runs once after all pages");
    LogLine("[LayoutAndAnnotateViews] done overflowCount=" + std::to_string(result.overflowItems.size())
        + " nextY=" + FormatReal(result.nextY)
        + " placedAny=" + std::to_string(result.placedAny ? 1 : 0));
    return result;
}

static DrawingRunSummary RunBatchFlatPatternDrawing(const CommandOptions& options)
{
    DrawingRunSummary summary;
    LogLine("[RunBatchFlatPatternDrawing] begin");
    Session* session = Session::GetSession();
    NXOpen::Part* displayPart = session->Parts()->Display();
    NXOpen::Part* workPart = session->Parts()->Work();
    if (displayPart == NULL || workPart == NULL)
    {
        LogLine("[RunBatchFlatPatternDrawing] no active part");
        throw std::runtime_error("No active part.");
    }

    LogLine("[RunBatchFlatPatternDrawing] displayPart=" + LocaleText(displayPart->Name())
        + " workPart=" + LocaleText(workPart->Name()));

    if (IsAssemblyPart(displayPart) && workPart != displayPart)
    {
        LogLine("[RunBatchFlatPatternDrawing] display part is assembly, switch work part to assembly for drawing sheet");
        try
        {
            session->Parts()->SetWork(displayPart);
            workPart = displayPart;
            LogLine("[RunBatchFlatPatternDrawing] work part switched to display assembly");
        }
        catch (const NXOpen::NXException& ex)
        {
            LogLine("[RunBatchFlatPatternDrawing] SetWork assembly NXException code=" + std::to_string(ex.ErrorCode())
                + " message=" + std::string(ex.Message()));
            workPart = displayPart;
        }
        catch (...)
        {
            LogLine("[RunBatchFlatPatternDrawing] SetWork assembly exception, use display assembly pointer");
            workPart = displayPart;
        }
    }

    std::vector<FlatPatternItem> items = CollectFlatPatternItems(displayPart);
    if (items.empty())
    {
        LogLine("[RunBatchFlatPatternDrawing] no flat pattern items");
        throw std::runtime_error("No flat pattern features were found in the displayed part or assembly leaf parts.");
    }

    LogLine("[PILianDaoCuZKT] flat pattern count: " + std::to_string(items.size()));
    summary.totalCount = items.size();
    SortFlatPatternItemsForClassBatches(items);
    DeleteExistingPluginSheets(workPart);
    ApplyFlatPatternDefaultBendCurveVisibility(workPart, options.showBendLines);
    DraftingDelayUpdateGuard draftingDelayGuard(workPart);

    size_t nextItemIndex = 0;
    size_t pageIndex = 0;
    std::vector<FlatPatternItem> carryItems;
    const double margin = kSheetMargin;
    const double minimumUsefulCursorY = margin + std::max(40.0, options.noteTextSize * 8.0) + options.rowSpacing;
    double pageCursorY = options.sheetHeight - margin;
    bool sheetActive = false;
    std::string currentSheetName;
    size_t sheetPlacedCount = 0;
    while (nextItemIndex < items.size() || !carryItems.empty())
    {
        if (!carryItems.empty())
        {
            sheetActive = false;
        }
        else if (sheetActive && pageCursorY < minimumUsefulCursorY)
        {
            LogLine("[RunBatchFlatPatternDrawing] current sheet remaining height is low, start next sheet sheet="
                + currentSheetName
                + " placedCount=" + std::to_string(sheetPlacedCount)
                + " cursorY=" + FormatReal(pageCursorY)
                + " minimumUsefulCursorY=" + FormatReal(minimumUsefulCursorY));
            sheetActive = false;
        }

        if (!sheetActive)
        {
            if (!currentSheetName.empty())
            {
                LogLine("[RunBatchFlatPatternDrawing] page close sheet=" + currentSheetName
                    + " placedCount=" + std::to_string(sheetPlacedCount)
                    + " nextItemIndex=" + std::to_string(nextItemIndex)
                    + " carryCount=" + std::to_string(carryItems.size()));
            }

            currentSheetName = PluginSheetName(pageIndex);
            LogLine("[RunBatchFlatPatternDrawing] page begin index=" + std::to_string(pageIndex + 1)
                + " sheet=" + currentSheetName
                + " carryCount=" + std::to_string(carryItems.size())
                + " nextItemIndex=" + std::to_string(nextItemIndex));
            CreateCustomSheet(workPart, options, currentSheetName);
            ++pageIndex;
            summary.sheetCount = pageIndex;
            pageCursorY = options.sheetHeight - margin;
            sheetPlacedCount = 0;
            sheetActive = true;
        }

        std::vector<FlatPatternItem> batchItems;
        const size_t carryCount = carryItems.size();
        std::string batchLayoutKey;
        if (!carryItems.empty())
        {
            batchLayoutKey = carryItems[0].layoutKey;
            batchItems.swap(carryItems);
        }
        else
        {
            batchLayoutKey = items[nextItemIndex].layoutKey;
            while (nextItemIndex < items.size()
                && items[nextItemIndex].layoutKey == batchLayoutKey)
            {
                batchItems.push_back(items[nextItemIndex]);
                ++nextItemIndex;
            }
        }

        if (batchItems.empty())
        {
            break;
        }

        const size_t batchInputCount = batchItems.size();
        const bool sheetHadContentBeforeBatch = sheetPlacedCount > 0;
        LogLine("[RunBatchFlatPatternDrawing] batch layout begin sheet=" + currentSheetName
            + " layoutKey=" + batchLayoutKey
            + " inputCount=" + std::to_string(batchInputCount)
            + " carryCount=" + std::to_string(carryCount)
            + " cursorY=" + FormatReal(pageCursorY)
            + " nextItemIndex=" + std::to_string(nextItemIndex));
        LayoutBatchResult layoutResult = LayoutAndAnnotateViews(workPart, batchItems, options, pageCursorY);
        std::vector<FlatPatternItem>().swap(batchItems);

        const size_t batchPlacedCount = layoutResult.createdCount;
        summary.createdCount += layoutResult.createdCount;
        summary.failures.insert(summary.failures.end(), layoutResult.failures.begin(), layoutResult.failures.end());
        pageCursorY = layoutResult.nextY;
        const bool pageFull = !layoutResult.overflowItems.empty();
        carryItems.swap(layoutResult.overflowItems);
        sheetPlacedCount += batchPlacedCount;

        LogLine("[RunBatchFlatPatternDrawing] batch done sheet=" + currentSheetName
            + " layoutKey=" + batchLayoutKey
            + " placedCount=" + std::to_string(batchPlacedCount)
            + " sheetPlacedCount=" + std::to_string(sheetPlacedCount)
            + " carryCount=" + std::to_string(carryItems.size())
            + " pageFull=" + std::to_string(pageFull ? 1 : 0)
            + " nextItemIndex=" + std::to_string(nextItemIndex)
            + " nextCursorY=" + FormatReal(pageCursorY));
        LogLine("[RunBatchFlatPatternDrawing] batch update skipped; final update runs once after all pages");
        if (batchPlacedCount == 0 && !carryItems.empty())
        {
            if (sheetHadContentBeforeBatch)
            {
                LogLine("[RunBatchFlatPatternDrawing] no room for next row on current sheet, continue on new sheet sheet="
                    + currentSheetName
                    + " carryCount=" + std::to_string(carryItems.size())
                    + " cursorY=" + FormatReal(pageCursorY));
                sheetActive = false;
                continue;
            }

            LogLine("[RunBatchFlatPatternDrawing] batch made no progress on an empty sheet, stop pagination to avoid endless loop sheet=" + currentSheetName);
            break;
        }

        if (pageFull)
        {
            sheetActive = false;
        }
    }
    if (!currentSheetName.empty())
    {
        LogLine("[RunBatchFlatPatternDrawing] page close sheet=" + currentSheetName
            + " placedCount=" + std::to_string(sheetPlacedCount)
            + " nextItemIndex=" + std::to_string(nextItemIndex)
            + " carryCount=" + std::to_string(carryItems.size()));
    }
    LogLine("[RunBatchFlatPatternDrawing] layout complete");

    DoUpdateNow("PILianDaoCuZKT final update after all pages");
    try
    {
        session->Parts()->SetWork(workPart);
        NXOpen::Part* finalDisplayPart = session->Parts()->Display();
        NXOpen::Part* finalWorkPart = session->Parts()->Work();
        LogLine("[RunBatchFlatPatternDrawing] final active parts displayPart="
            + LocaleText(finalDisplayPart != NULL ? finalDisplayPart->Name() : NXOpen::NXString(""))
            + " workPart=" + LocaleText(finalWorkPart != NULL ? finalWorkPart->Name() : NXOpen::NXString("")));
    }
    catch (const NXOpen::NXException& ex)
    {
        LogLine("[RunBatchFlatPatternDrawing] final SetWork NXException code="
            + std::to_string(ex.ErrorCode()) + " message=" + std::string(ex.Message()));
    }
    catch (...)
    {
        LogLine("[RunBatchFlatPatternDrawing] final SetWork exception");
    }

    NXOpen::Drafting::PreferencesBuilder* finalPreferenceBuilder = NULL;
    try
    {
        finalPreferenceBuilder = workPart->SettingsManager()->CreatePreferencesBuilder();
        LogFlatPatternPreferenceStateInUiOrder(finalPreferenceBuilder != NULL ? finalPreferenceBuilder->ViewStyle() : NULL, "final-before-exit");
        if (finalPreferenceBuilder != NULL)
        {
            finalPreferenceBuilder->Destroy();
            finalPreferenceBuilder = NULL;
        }
    }
    catch (const NXOpen::NXException& ex)
    {
        LogLine("[RunBatchFlatPatternDrawing] final preference read NXException code="
            + std::to_string(ex.ErrorCode()) + " message=" + std::string(ex.Message()));
        if (finalPreferenceBuilder != NULL)
        {
            try { finalPreferenceBuilder->Destroy(); } catch (...) {}
        }
    }
    catch (...)
    {
        LogLine("[RunBatchFlatPatternDrawing] final preference read exception");
        if (finalPreferenceBuilder != NULL)
        {
            try { finalPreferenceBuilder->Destroy(); } catch (...) {}
        }
    }
    LogLine("[RunBatchFlatPatternDrawing] done");
    return summary;
}
}

PILianDaoCuZKTDialog::PILianDaoCuZKTDialog()
{
    LogLine("[Dialog::ctor] begin");
    PILianDaoCuZKTDialog::theSession = NXOpen::Session::GetSession();
    PILianDaoCuZKTDialog::theUI = UI::GetUI();
    theDlxFileName = "PILianDaoCuZKTDialog.dlx";
    theDialog = PILianDaoCuZKTDialog::theUI->CreateDialog(theDlxFileName);
    theDialog->AddOkHandler(make_callback(this, &PILianDaoCuZKTDialog::ok_cb));
    theDialog->AddUpdateHandler(make_callback(this, &PILianDaoCuZKTDialog::update_cb));
    theDialog->AddInitializeHandler(make_callback(this, &PILianDaoCuZKTDialog::initialize_cb));
    theDialog->AddDialogShownHandler(make_callback(this, &PILianDaoCuZKTDialog::dialogShown_cb));
    LogLine("[Dialog::ctor] callbacks registered");
}

PILianDaoCuZKTDialog::~PILianDaoCuZKTDialog()
{
    if (theDialog != NULL)
    {
        delete theDialog;
        theDialog = NULL;
    }
}

extern "C" DllExport void ufusr(char* param, int* retcod, int param_len)
{
    LogLine("[ufusr] entered");
    int initStatus = UF_initialize();
    LogLine("[ufusr] UF_initialize status=" + std::to_string(initStatus));
    PILianDaoCuZKTDialog* dialog = NULL;
    try
    {
        dialog = new PILianDaoCuZKTDialog();
        LogLine("[ufusr] launch dialog");
        dialog->Launch();
        LogLine("[ufusr] dialog returned");
    }
    catch (const std::exception& ex)
    {
        LogLine(std::string("[ufusr] exception: ") + ex.what());
    }
    if (dialog != NULL)
    {
        delete dialog;
    }
    if (initStatus == 0)
    {
        UF_terminate();
        LogLine("[ufusr] UF_terminate done");
    }
    LogLine("[ufusr] leave");
}

extern "C" DllExport int ufusr_ask_unload()
{
    return static_cast<int>(Session::LibraryUnloadOptionImmediately);
}

extern "C" DllExport void ufusr_cleanup(void)
{
}

NXOpen::BlockStyler::BlockDialog::DialogResponse PILianDaoCuZKTDialog::Launch()
{
    try
    {
        LogLine("[Dialog::Launch] begin");
        NXOpen::BlockStyler::BlockDialog::DialogResponse response = theDialog->Launch();
        LogLine("[Dialog::Launch] response=" + std::to_string(static_cast<int>(response)));
        return response;
    }
    catch (const std::exception& ex)
    {
        LogLine(std::string("[Dialog::Launch] exception: ") + ex.what());
    }
    return NXOpen::BlockStyler::BlockDialog::DialogResponseInvalid;
}

void PILianDaoCuZKTDialog::initialize_cb()
{
    try
    {
        LogLine("[initialize_cb] begin");
        group = dynamic_cast<NXOpen::BlockStyler::Group*>(theDialog->TopBlock()->FindBlock("group"));
        category_layout = dynamic_cast<NXOpen::BlockStyler::Toggle*>(theDialog->TopBlock()->FindBlock("category_layout"));
        annotate_max_dimension = dynamic_cast<NXOpen::BlockStyler::Toggle*>(theDialog->TopBlock()->FindBlock("annotate_max_dimension"));
        show_bend_lines = dynamic_cast<NXOpen::BlockStyler::Toggle*>(theDialog->TopBlock()->FindBlock("show_bend_lines"));
        group1 = dynamic_cast<NXOpen::BlockStyler::Group*>(theDialog->TopBlock()->FindBlock("group1"));
        sheet_height = dynamic_cast<NXOpen::BlockStyler::DoubleBlock*>(theDialog->TopBlock()->FindBlock("sheet_height"));
        sheet_width = dynamic_cast<NXOpen::BlockStyler::DoubleBlock*>(theDialog->TopBlock()->FindBlock("sheet_width"));
        main_group = dynamic_cast<NXOpen::BlockStyler::Group*>(theDialog->TopBlock()->FindBlock("main_group"));
        view_spacing = dynamic_cast<NXOpen::BlockStyler::DoubleBlock*>(theDialog->TopBlock()->FindBlock("view_spacing"));
        row_spacing = dynamic_cast<NXOpen::BlockStyler::DoubleBlock*>(theDialog->TopBlock()->FindBlock("row_spacing"));
        note_text_size = dynamic_cast<NXOpen::BlockStyler::DoubleBlock*>(theDialog->TopBlock()->FindBlock("note_text_size"));
        dimension_global_scale = dynamic_cast<NXOpen::BlockStyler::DoubleBlock*>(theDialog->TopBlock()->FindBlock("dimension_global_scale"));
        LogLine("[initialize_cb] done");
    }
    catch (const std::exception& ex)
    {
        LogLine(std::string("[initialize_cb] exception: ") + ex.what());
    }
}

void PILianDaoCuZKTDialog::dialogShown_cb()
{
    LogLine("[dialogShown_cb] shown");
}

int PILianDaoCuZKTDialog::update_cb(NXOpen::BlockStyler::UIBlock* block)
{
    LogLine("[update_cb] block update");
    return 0;
}

int PILianDaoCuZKTDialog::ok_cb()
{
    try
    {
        LogLine("[PILianDaoCuZKT] OK clicked, batch flat-pattern drawing started.");
        CommandOptions options;
        options.categoryLayout = GetToggleValue(category_layout, true);
        options.annotateMaxDimension = GetToggleValue(annotate_max_dimension, true);
        options.showBendLines = GetToggleValue(show_bend_lines, false);
        options.sheetHeight = ClampPositive(GetDoubleValue(sheet_height, 210.0), 210.0);
        options.sheetWidth = ClampPositive(GetDoubleValue(sheet_width, 297.0), 297.0);
        options.viewSpacing = ClampPositive(GetDoubleValue(view_spacing, 15.0), 15.0);
        options.rowSpacing = ClampPositive(GetDoubleValue(row_spacing, 20.0), 20.0);
        options.noteTextSize = ClampPositive(GetDoubleValue(note_text_size, 3.5), 3.5);
        options.viewScaleDenominator = ClampPositive(GetDoubleValue(dimension_global_scale, 1.0), 1.0);
        LogLine("[ok_cb] options categoryLayout=" + std::to_string(options.categoryLayout ? 1 : 0)
            + " annotateMaxDimension=" + std::to_string(options.annotateMaxDimension ? 1 : 0)
            + " showBendLines=" + std::to_string(options.showBendLines ? 1 : 0)
            + " sheetWidth=" + FormatReal(options.sheetWidth)
            + " sheetHeight=" + FormatReal(options.sheetHeight)
            + " viewSpacing=" + FormatReal(options.viewSpacing)
            + " rowSpacing=" + FormatReal(options.rowSpacing)
            + " noteTextSize=" + FormatReal(options.noteTextSize)
            + " dimensionScale=" + FormatReal(options.viewScaleDenominator)
            + " viewScale=1");

        DrawingRunSummary summary = RunBatchFlatPatternDrawing(options);
        const std::string message = BuildRunSummaryMessage(summary);
        PILianDaoCuZKTDialog::theUI->NXMessageBox()->Show("PILianDaoCuZKT",
            summary.failures.empty() ? NXOpen::NXMessageBox::DialogTypeInformation : NXOpen::NXMessageBox::DialogTypeWarning,
            message.c_str());
    }
    catch (const std::exception& ex)
    {
        LogLine(std::string("[ok_cb] exception: ") + ex.what());
        return 1;
    }
    catch (...)
    {
        LogLine("[ok_cb] unknown exception");
        return 1;
    }
    return 0;
}

PropertyList* PILianDaoCuZKTDialog::GetBlockProperties(const char* blockID)
{
    return theDialog->GetBlockProperties(blockID);
}
