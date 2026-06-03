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
#include <NXOpen/Body.hxx>
#include <NXOpen/BodyCollection.hxx>
#include <NXOpen/Builder.hxx>
#include <NXOpen/ColorManager.hxx>
#include <NXOpen/Direction.hxx>
#include <NXOpen/DirectionCollection.hxx>
#include <NXOpen/DisplayableObject.hxx>
#include <NXOpen/Drafting_BaseEditSettingsBuilder.hxx>
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
#include <cmath>
#include <cstdio>
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
static const char* kDefaultBodyNoteFormat = "{\xE7\xBC\x96\xE5\x8F\xB7=}{\xE6\x9D\x90\xE6\x96\x99} T={\xE5\x8E\x9A\xE5\xBA\xA6} {\xE6\x95\xB0\xE9\x87\x8F}PCS{\xE9\x95\x9C\xE5\x83\x8F}";
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

struct FlatPatternItem
{
    NXOpen::Part* part;
    NXOpen::Body* body;
    NXOpen::Features::Feature* feature;
    NXOpen::Face* upwardFace;
    std::string viewName;
    std::string partKey;
    int quantity;
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
    try
    {
        std::time_t rawTime = std::time(NULL);
        std::tm localTime = {};
        localtime_s(&localTime, &rawTime);
        char prefix[128] = {};
        sprintf(prefix, "%04d-%02d-%02d %02d:%02d:%02d ",
            localTime.tm_year + 1900, localTime.tm_mon + 1, localTime.tm_mday,
            localTime.tm_hour, localTime.tm_min, localTime.tm_sec);
        std::ofstream file("C:\\PILianDaoCuZKT_debug.log", std::ios::app);
        file << prefix << text << std::endl;
    }
    catch (...)
    {
    }

    try
    {
        ListingWindow* lw = Session::GetSession()->ListingWindow();
        if (!lw->IsOpen())
        {
            lw->Open();
        }
        lw->WriteLine(text.c_str());
    }
    catch (...)
    {
    }
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

static int BodyNoteFormatLineCount()
{
    const std::string format = LoadBodyNoteFormat();
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

    std::string note = LoadBodyNoteFormat();
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
    std::string text = LoadBodyNoteFormat();
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
    const std::string format = LoadBodyNoteFormat();
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
            items.push_back(item);
            LogLine("[CollectPartFlatPatterns] accepted view=" + item.viewName
                + " bodyTag=" + std::to_string(static_cast<unsigned long long>(item.body != NULL ? item.body->Tag() : NULL_TAG)));

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

static void EnsurePartFullyLoaded(NXOpen::Part* part)
{
    if (part == NULL)
    {
        return;
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
}

static void AddAssemblyPrototypePartCount(NXOpen::Assemblies::Component* component, std::map<tag_t, std::pair<NXOpen::Part*, int> >& counts)
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
            EnsurePartFullyLoaded(part);
            std::pair<NXOpen::Part*, int>& entry = counts[part->Tag()];
            entry.first = part;
            entry.second += 1;
            LogLine("[AddAssemblyPrototypePartCount] part=" + LocaleText(part->Name())
                + " count=" + std::to_string(entry.second));
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

static void CollectAssemblyPartCounts(NXOpen::Assemblies::Component* component, std::map<tag_t, std::pair<NXOpen::Part*, int> >& counts)
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
    std::map<tag_t, std::pair<NXOpen::Part*, int> > partCounts;
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
        partCounts[displayPart->Tag()] = std::make_pair(displayPart, 1);
        LogLine("[CollectFlatPatternItems] no assembly leaf counts, use display part");
    }

    LogLine("[CollectFlatPatternItems] unique part count=" + std::to_string(partCounts.size()));
    for (std::map<tag_t, std::pair<NXOpen::Part*, int> >::iterator it = partCounts.begin(); it != partCounts.end(); ++it)
    {
        std::vector<FlatPatternItem> partItems = CollectPartFlatPatterns(it->second.first, it->second.second);
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

static NXOpen::Drawings::DraftingDrawingSheet* CreateCustomSheet(NXOpen::Part* part, const CommandOptions& options)
{
    LogLine("[CreateCustomSheet] begin width=" + FormatReal(options.sheetWidth)
        + " height=" + FormatReal(options.sheetHeight));
    NXOpen::Drawings::DraftingDrawingSheet* nullSheet = NULL;
    NXOpen::Drawings::DraftingDrawingSheetBuilder* builder =
        part->DraftingDrawingSheets()->CreateDraftingDrawingSheetBuilder(nullSheet);

    builder->SetOption(NXOpen::Drawings::DrawingSheetBuilder::SheetOptionCustomSize);
    builder->SetUnits(NXOpen::Drawings::DrawingSheetBuilder::SheetUnitsMetric);
    builder->SetLength(options.sheetWidth);
    builder->SetHeight(options.sheetHeight);
    builder->SetName("PILianDaoCuZKT");
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

static NXOpen::Drawings::BaseView* CreateFlatBaseView(NXOpen::Part* workPart, const FlatPatternItem& item, const CommandOptions& options, const NXOpen::Point3d& point)
{
    LogLine("[CreateFlatBaseView] begin part=" + LocaleText(item.part != NULL ? item.part->Name() : NXOpen::NXString(""))
        + " viewName=" + item.viewName
        + " workPart=" + LocaleText(workPart != NULL ? workPart->Name() : NXOpen::NXString(""))
        + " scaleDenominator=" + FormatReal(options.viewScaleDenominator));
    NXOpen::Drawings::BaseView* nullView = NULL;
    NXOpen::Drawings::BaseViewBuilder* builder = NULL;
    try
    {
        if (workPart == NULL)
        {
            return NULL;
        }

        builder = workPart->DraftingViews()->CreateBaseViewBuilder(nullView);
        builder->Scale()->SetNumerator(1.0);
        builder->Scale()->SetDenominator(options.viewScaleDenominator);
        try
        {
            builder->SecondaryComponents()->SetObjectType(NXOpen::Drawings::DraftingComponentSelectionBuilder::GeometryPrimaryGeometry);
            if (item.part != NULL)
            {
                builder->SecondaryComponents()->SetPart(item.part);
            }
        }
        catch (const NXOpen::NXException& ex)
        {
            LogLine("[CreateFlatBaseView] set secondary/primary component style ignored code="
                + std::to_string(ex.ErrorCode()) + " message=" + std::string(ex.Message()));
        }
        catch (...)
        {
            LogLine("[CreateFlatBaseView] set secondary/primary component style ignored");
        }

        if (item.part != NULL)
        {
            try
            {
                builder->Style()->ViewStyleBase()->SetPart(item.part);
                builder->Style()->ViewStyleBase()->SetPartName(item.part->FullPath());
                builder->Style()->ViewStyleBase()->Arrangement()->SetSelectedArrangement(NULL);
                builder->Style()->ViewStyleBase()->Arrangement()->SetInheritArrangementFromParent(false);
                LogLine("[CreateFlatBaseView] view style part set to child part=" + item.partKey);
            }
            catch (const NXOpen::NXException& ex)
            {
                LogLine("[CreateFlatBaseView] set view style part NXException code=" + std::to_string(ex.ErrorCode())
                    + " message=" + std::string(ex.Message())
                    + " part=" + item.partKey);
            }
            catch (...)
            {
                LogLine("[CreateFlatBaseView] set view style part exception part=" + item.partKey);
            }
        }

        NXOpen::ModelingView* modelView = FindModelingView(item.part, item.viewName);
        if (modelView != NULL)
        {
            LogLine("[CreateFlatBaseView] selected model view=" + item.viewName
                + " sourcePart=" + LocaleText(item.part != NULL ? item.part->Name() : NXOpen::NXString("")));
            builder->SelectModelView()->SetSelectedView(modelView);
        }
        else
        {
            LogLine("[CreateFlatBaseView] model view not found, commit with default");
        }

        builder->Placement()->Placement()->SetValue(NULL, workPart->Views()->WorkView(), point);
        NXOpen::NXObject* object = builder->Commit();
        builder->Destroy();
        builder = NULL;
        NXOpen::Drawings::BaseView* view = dynamic_cast<NXOpen::Drawings::BaseView*>(object);
        LogLine(std::string("[CreateFlatBaseView] commit ") + (view != NULL ? "ok" : "returned null"));
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
    std::vector<NXOpen::Drawings::DraftingCurve*> curves = CollectDraftingCurves(view);
    bool initialized = false;
    for (size_t i = 0; i < curves.size(); ++i)
    {
        NXOpen::DisplayableObject* displayableCurve = dynamic_cast<NXOpen::DisplayableObject*>(curves[i]);
        if (displayableCurve != NULL && displayableCurve->IsBlanked())
        {
            continue;
        }
        ExpandRectWithCurveInDrawing(view, curves[i], rect, initialized);
    }

    if (initialized)
    {
        LogLine("[RectFromMappedVisibleCurves] drawing min=("
            + FormatReal(rect.minX) + "," + FormatReal(rect.minY) + ") max=("
            + FormatReal(rect.maxX) + "," + FormatReal(rect.maxY)
            + ") width=" + FormatReal(RectWidth(rect))
            + " height=" + FormatReal(RectHeight(rect)));
    }
    else
    {
        LogLine("[RectFromMappedVisibleCurves] no visible curve points");
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
        std::string text = BuildBodyNoteText(item.part, item.body, item.quantity);
        std::string ufInitialText = BuildBodyNoteTextForUf(item.part, item.body, item.quantity);
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
        const int createStatus = UF_DRF_create_note(
            static_cast<int>(linePointers.size()),
            linePointers.data(),
            origin,
            0,
            &createdTag);
        int type = -1;
        int subtype = -1;
        int attachStatus = -1;
        tag_t attachedViewTag = NULL_TAG;
        bool linkedText = false;
        int editStatus = -1;
        if (createdTag != NULL_TAG)
        {
            UF_OBJ_ask_type_and_subtype(createdTag, &type, &subtype);
            if (view != NULL)
            {
                attachStatus = UF_DRAW_attach_note_to_view(createdTag, view->Tag());
                UF_DRAW_ask_view_of_note(createdTag, &attachedViewTag);
            }
            NXOpen::TaggedObject* tagged = NXOpen::NXObjectManager::Get(createdTag);
            NXOpen::DisplayableObject* displayable = dynamic_cast<NXOpen::DisplayableObject*>(tagged);
            NXOpen::Annotations::SimpleDraftingAid* draftingAid = dynamic_cast<NXOpen::Annotations::SimpleDraftingAid*>(tagged);
            if (draftingAid != NULL)
            {
                NXOpen::Annotations::DraftingNoteBuilder* editBuilder = NULL;
                try
                {
                    editBuilder = workPart->Annotations()->CreateDraftingNoteBuilder(draftingAid);
                    editBuilder->Style()->LetteringStyle()->SetGeneralTextSize(textSize);
                    editBuilder->Style()->LetteringStyle()->SetGeneralTextLineSpaceFactor(1.5);
                    try
                    {
                        editBuilder->Style()->LetteringStyle()->SetGeneralTextColor(workPart->Colors()->Find("Emerald"));
                    }
                    catch (...)
                    {
                    }
                    NXOpen::NXObject* edited = editBuilder->Commit();
                    editStatus = edited != NULL ? 0 : 1;
                    editBuilder->Destroy();
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
            if (displayable != NULL)
            {
                try { displayable->SetLayer(1); } catch (...) {}
                try { displayable->Unblank(); } catch (...) {}
                try { displayable->RedisplayObject(); } catch (...) {}
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

static bool CreateOverallDimension(NXOpen::Part* workPart, NXOpen::Drawings::BaseView* view, const DraftRect& rect, bool horizontal, double dimensionGap)
{
    std::vector<NXOpen::Drawings::DraftingCurve*> curves = CollectDraftingCurves(view);
    if (workPart == NULL || view == NULL || curves.empty())
    {
        LogLine("[CreateOverallDimension] skipped empty inputs");
        return false;
    }

    CurveAssocCandidate first = {};
    CurveAssocCandidate second = {};
    if (!FindOverallDimensionCurvePair(view, curves, rect, horizontal, first, second))
    {
        if (!FindOverallDimensionCurvePairFallback(view, curves, rect, horizontal, first, second))
        {
            LogLine(std::string("[CreateOverallDimension] no curve pair direction=") + (horizontal ? "H" : "V"));
            return false;
        }
        LogLine(std::string("[CreateOverallDimension] used fallback direction=") + (horizontal ? "H" : "V"));
    }

    NXOpen::Annotations::Dimension* nullDimension = NULL;
    NXOpen::Annotations::RapidDimensionBuilder* builder = workPart->Dimensions()->CreateRapidDimensionBuilder(nullDimension);
    if (builder == NULL)
    {
        return false;
    }

    NXOpen::View* nullView = NULL;
    NXOpen::Point3d assistPoint(0.0, 0.0, 0.0);
    builder->Style()->DimensionStyle()->SetTextCentered(true);
    builder->Style()->DimensionStyle()->SetNarrowDisplayType(NXOpen::Annotations::NarrowDisplayOptionNone);
    builder->Measurement()->SetMethod(horizontal ?
        NXOpen::Annotations::DimensionMeasurementBuilder::MeasurementMethodHorizontal :
        NXOpen::Annotations::DimensionMeasurementBuilder::MeasurementMethodVertical);
    builder->FirstAssociativity()->SetValue(first.snapType, first.curve, view, first.modelPoint, NULL, nullView, assistPoint);
    builder->SecondAssociativity()->SetValue(second.snapType, second.curve, view, second.modelPoint, NULL, nullView, assistPoint);

    NXOpen::Point3d origin = horizontal ?
        NXOpen::Point3d(rect.maxX - dimensionGap, rect.maxY + dimensionGap, 0.0) :
        NXOpen::Point3d(rect.minX - dimensionGap, rect.minY - dimensionGap, 0.0);
    builder->Origin()->Origin()->SetValue(NULL, nullView, origin);

    try
    {
        builder->Commit();
        builder->Destroy();
        LogLine(std::string("[CreateOverallDimension] created direction=") + (horizontal ? "H" : "V")
            + " origin=(" + FormatReal(origin.X) + "," + FormatReal(origin.Y) + ")");
        return true;
    }
    catch (const NXOpen::NXException& ex)
    {
        LogLine("[CreateOverallDimension] commit failed: " + LocaleText(ex.Message()));
        builder->Destroy();
        return false;
    }
    catch (...)
    {
        LogLine("[CreateOverallDimension] commit failed");
        builder->Destroy();
        return false;
    }
}

static void DoUpdateNow(const char* reason)
{
    try
    {
        Session* session = Session::GetSession();
        Session::UndoMarkId mark = session->SetUndoMark(Session::MarkVisibilityInvisible, reason != NULL ? reason : "PILianDaoCuZKT update");
        session->UpdateManager()->DoUpdate(mark);
    }
    catch (...)
    {
        LogLine(std::string("[DoUpdateNow] update failed/ignored: ") + (reason != NULL ? reason : ""));
    }
}

static void LayoutAndAnnotateViews(NXOpen::Part* workPart, const std::vector<FlatPatternItem>& items, const CommandOptions& options)
{
    LogLine("[LayoutAndAnnotateViews] begin itemCount=" + std::to_string(items.size()));
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

    std::vector<CreatedView> createdViews;
    createdViews.reserve(items.size());

    const NXOpen::Point3d sheetCenter(options.sheetWidth * 0.5, options.sheetHeight * 0.5, 0.0);
    LogLine("[LayoutAndAnnotateViews] phase1 create all views at sheet center");
    for (size_t i = 0; i < items.size(); ++i)
    {
        LogLine("[LayoutAndAnnotateViews] create item index=" + std::to_string(i)
            + " part=" + LocaleText(items[i].part != NULL ? items[i].part->Name() : NXOpen::NXString(""))
            + " view=" + items[i].viewName);

        NXOpen::Drawings::BaseView* view = CreateFlatBaseView(workPart, items[i], options, sheetCenter);
        if (view == NULL)
        {
            LogLine("[PILianDaoCuZKT] create view failed: " + items[i].partKey + " view=" + items[i].viewName);
            continue;
        }

        CreatedView created;
        created.item = items[i];
        created.view = view;
        created.width = 50.0;
        created.height = 40.0;
        created.material = BodyMaterialText(items[i].part, items[i].body);
        created.thickness = BodyThicknessText(items[i].part, items[i].body);
        created.layoutKey = BodyLayoutKey(created.material, created.thickness);
        created.valid = false;
        created.placed = false;
        createdViews.push_back(created);
    }

    if (createdViews.empty())
    {
        LogLine("[LayoutAndAnnotateViews] no views created");
        return;
    }

    DoUpdateNow("PILianDaoCuZKT create views");
    LogLine("[LayoutAndAnnotateViews] phase1b center every view by boundary before measuring");
    for (size_t i = 0; i < createdViews.size(); ++i)
    {
        MoveViewBoundaryCenterTo(createdViews[i].view, sheetCenter);
    }
    DoUpdateNow("PILianDaoCuZKT center views before layout");

    LogLine("[LayoutAndAnnotateViews] phase1c apply bend line visibility show="
        + std::to_string(options.showBendLines ? 1 : 0));
    for (size_t i = 0; i < createdViews.size(); ++i)
    {
        ApplyFlatPatternBendCurveVisibility(workPart, createdViews[i].view, options.showBendLines);
    }
    DoUpdateNow("PILianDaoCuZKT apply bend line visibility");

    LogLine("[LayoutAndAnnotateViews] phase2 measure view rectangles");
    for (size_t i = 0; i < createdViews.size(); ++i)
    {
        DraftRect rect;
        const bool hasRect = RectForLayout(createdViews[i].view, rect);
        if (!hasRect)
        {
            rect.minX = sheetCenter.X - 25.0;
            rect.maxX = sheetCenter.X + 25.0;
            rect.minY = sheetCenter.Y - 20.0;
            rect.maxY = sheetCenter.Y + 20.0;
        }
        createdViews[i].rect = rect;
        createdViews[i].width = std::max(50.0, RectWidth(rect));
        createdViews[i].height = std::max(40.0, RectHeight(rect));
        createdViews[i].valid = hasRect;
        LogLine("[LayoutAndAnnotateViews] measured index=" + std::to_string(i)
            + " valid=" + std::to_string(hasRect ? 1 : 0)
            + " width=" + FormatReal(createdViews[i].width)
            + " height=" + FormatReal(createdViews[i].height)
            + " material=" + createdViews[i].material
            + " thickness=" + createdViews[i].thickness);
    }

    LogLine("[LayoutAndAnnotateViews] phase3 sort rows by material and thickness");
    const double margin = std::max(10.0, options.viewSpacing);
    const double dimensionGap = options.annotateMaxDimension ? std::max(3.0, options.noteTextSize * 2.0) : 0.0;
    const double dimensionTopReserve = options.annotateMaxDimension ? dimensionGap + options.noteTextSize * 4.0 : 0.0;
    const double dimensionRightReserve = options.annotateMaxDimension ? dimensionGap + options.noteTextSize * 6.0 : 0.0;
    const double noteGap = std::max(2.0, options.noteTextSize * 1.5);
    const double noteReserve = noteGap + options.noteTextSize * 4.0;
    const double usableWidth = options.sheetWidth - margin * 2.0;

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
    for (size_t i = 0; i < createdViews.size(); ++i)
    {
        groupedIndices[createdViews[i].layoutKey].push_back(i);
    }

    std::vector<LayoutRow> rows;
    for (std::map<std::string, std::vector<size_t> >::const_iterator git = groupedIndices.begin(); git != groupedIndices.end(); ++git)
    {
        LayoutRow row;
        row.key = git->first;
        row.totalWidth = 0.0;
        row.maxBlockHeight = 0.0;

        for (size_t j = 0; j < git->second.size(); ++j)
        {
            const size_t viewIndex = git->second[j];
            const double blockWidth = createdViews[viewIndex].width + dimensionRightReserve;
            const double blockHeight = dimensionTopReserve + createdViews[viewIndex].height + noteReserve;
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
                row.material = createdViews[viewIndex].material;
                row.thickness = createdViews[viewIndex].thickness;
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

    LogLine("[LayoutAndAnnotateViews] phase3 move views by material/thickness rows rowCount=" + std::to_string(rows.size()));
    double y = options.sheetHeight - margin;

    for (size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex)
    {
        const LayoutRow& row = rows[rowIndex];
        if (y - row.maxBlockHeight < margin)
        {
            LogLine("[PILianDaoCuZKT] sheet is full, remaining rows skipped layout row=" + std::to_string(rowIndex)
                + " material=" + row.material + " thickness=" + row.thickness
                + " rowHeight=" + FormatReal(row.maxBlockHeight));
            break;
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
            const double width = createdViews[i].width;
            const double height = createdViews[i].height;
            const double blockWidth = width + dimensionRightReserve;
            const double blockHeight = dimensionTopReserve + height + noteReserve;

            const double viewLeft = x;
            const double viewTop = y - dimensionTopReserve;
            NXOpen::Point3d center(viewLeft + width * 0.5, viewTop - height * 0.5, 0.0);
            MoveViewBoundaryCenterTo(createdViews[i].view, center);
            DoUpdateNow("PILianDaoCuZKT move one view");
            createdViews[i].placed = true;
            LogLine("[LayoutAndAnnotateViews] moved index=" + std::to_string(i)
                + " row=" + std::to_string(rowIndex)
                + " center=(" + FormatReal(center.X) + "," + FormatReal(center.Y)
                + ") blockWidth=" + FormatReal(blockWidth)
                + " blockHeight=" + FormatReal(blockHeight)
                + " rowHeight=" + FormatReal(row.maxBlockHeight));

            x += blockWidth + options.viewSpacing;
        }

        y -= row.maxBlockHeight + options.rowSpacing;
    }

    DoUpdateNow("PILianDaoCuZKT layout views");

    LogLine("[LayoutAndAnnotateViews] phase3b reapply bend line visibility after layout show="
        + std::to_string(options.showBendLines ? 1 : 0));
    for (size_t i = 0; i < createdViews.size(); ++i)
    {
        if (createdViews[i].placed)
        {
            ApplyFlatPatternBendCurveVisibility(workPart, createdViews[i].view, options.showBendLines);
        }
    }
    DoUpdateNow("PILianDaoCuZKT reapply bend line visibility");

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
            CreateOverallDimension(workPart, createdViews[i].view, displayedRect, true, dimensionGap);
            CreateOverallDimension(workPart, createdViews[i].view, displayedRect, false, dimensionGap);
        }

        const double bodyNoteGap = dimensionGap + std::max(noteGap, options.noteTextSize * 2.0);
        NXOpen::Point3d notePoint((displayedRect.minX + displayedRect.maxX) * 0.5, displayedRect.minY - bodyNoteGap, 0.0);
        CreateBodyNote(workPart, createdViews[i].item, createdViews[i].view, notePoint, options.noteTextSize);
        LogLine("[LayoutAndAnnotateViews] note index=" + std::to_string(i)
            + " point=(" + FormatReal(notePoint.X) + "," + FormatReal(notePoint.Y)
            + ") displayedMinY=" + FormatReal(displayedRect.minY)
            + " bodyNoteGap=" + FormatReal(bodyNoteGap));
    }
    DoUpdateNow("PILianDaoCuZKT annotate views");
    LogLine("[LayoutAndAnnotateViews] done");
}

static void RunBatchFlatPatternDrawing(const CommandOptions& options)
{
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
    CreateCustomSheet(workPart, options);
    LayoutAndAnnotateViews(workPart, items, options);
    LogLine("[RunBatchFlatPatternDrawing] layout complete");

    try
    {
        session->UpdateManager()->DoUpdate(session->SetUndoMark(Session::MarkVisibilityInvisible, "PILianDaoCuZKT Update"));
    }
    catch (...)
    {
        LogLine("[RunBatchFlatPatternDrawing] update exception ignored");
    }
    LogLine("[RunBatchFlatPatternDrawing] done");
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
        if (PILianDaoCuZKTDialog::theUI != NULL)
        {
            PILianDaoCuZKTDialog::theUI->NXMessageBox()->Show("PILianDaoCuZKT", NXOpen::NXMessageBox::DialogTypeError, ex.what());
        }
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
        PILianDaoCuZKTDialog::theUI->NXMessageBox()->Show("PILianDaoCuZKT", NXOpen::NXMessageBox::DialogTypeError, ex.what());
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
        PILianDaoCuZKTDialog::theUI->NXMessageBox()->Show("PILianDaoCuZKT", NXOpen::NXMessageBox::DialogTypeError, ex.what());
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
            + " viewScaleDenominator=" + FormatReal(options.viewScaleDenominator));

        RunBatchFlatPatternDrawing(options);
        PILianDaoCuZKTDialog::theUI->NXMessageBox()->Show("PILianDaoCuZKT", NXOpen::NXMessageBox::DialogTypeInformation, "批量展开图出图完成。");
    }
    catch (const std::exception& ex)
    {
        LogLine(std::string("[ok_cb] exception: ") + ex.what());
        PILianDaoCuZKTDialog::theUI->NXMessageBox()->Show("PILianDaoCuZKT", NXOpen::NXMessageBox::DialogTypeError, ex.what());
        return 1;
    }
    catch (...)
    {
        LogLine("[ok_cb] unknown exception");
        PILianDaoCuZKTDialog::theUI->NXMessageBox()->Show("PILianDaoCuZKT", NXOpen::NXMessageBox::DialogTypeError, "Unknown error.");
        return 1;
    }
    return 0;
}

PropertyList* PILianDaoCuZKTDialog::GetBlockProperties(const char* blockID)
{
    return theDialog->GetBlockProperties(blockID);
}
