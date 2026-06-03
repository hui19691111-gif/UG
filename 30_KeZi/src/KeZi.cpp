#include <NXOpen/BasePart.hxx>
#include <NXOpen/Body.hxx>
#include <NXOpen/Builder.hxx>
#include <NXOpen/Callback.hxx>
#include <NXOpen/ColorManager.hxx>
#include <NXOpen/Expression.hxx>
#include <NXOpen/Edge.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/FaceDumbRule.hxx>
#include <NXOpen/Features_TextBuilder.hxx>
#include <NXOpen/NXColor.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/ListingWindow.hxx>
#include <NXOpen/NXMessageBox.hxx>
#include <NXOpen/NXObject.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/NXString.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/ScCollector.hxx>
#include <NXOpen/ScRuleFactory.hxx>
#include <NXOpen/Selection.hxx>
#include <NXOpen/SelectionIntentRule.hxx>
#include <NXOpen/SelectionIntentRuleOptions.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/TaggedObject.hxx>
#include <NXOpen/Tooling_InsertTextBuilder.hxx>
#include <NXOpen/Tooling_MoldwizardManager.hxx>
#include <NXOpen/Tooling_ToolingManager.hxx>
#include <NXOpen/Tooling_ToolingSession.hxx>
#include <NXOpen/UI.hxx>
#include <NXOpen/Update.hxx>
#include <NXOpen/ugmath.hxx>

#include <NXOpen/BlockStyler_BlockDialog.hxx>
#include <NXOpen/BlockStyler_Button.hxx>
#include <NXOpen/BlockStyler_CompositeBlock.hxx>
#include <NXOpen/BlockStyler_DoubleBlock.hxx>
#include <NXOpen/BlockStyler_Enumeration.hxx>
#include <NXOpen/BlockStyler_IntegerBlock.hxx>
#include <NXOpen/BlockStyler_ObjectColorPicker.hxx>
#include <NXOpen/BlockStyler_PropertyList.hxx>
#include <NXOpen/BlockStyler_SelectObject.hxx>
#include <NXOpen/BlockStyler_SpecifyOrientation.hxx>
#include <NXOpen/BlockStyler_StringBlock.hxx>
#include <NXOpen/BlockStyler_Toggle.hxx>
#include <NXOpen/BlockStyler_UIBlock.hxx>

#include <uf.h>
#include <uf_assem.h>
#include <uf_modl.h>
#include <uf_modl_types.h>
#include <uf_object_types.h>
#include <uf_ui_types.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <memory>
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

#ifdef CreateDialog
#undef CreateDialog
#endif

#ifndef DllExport
#define DllExport __declspec(dllexport)
#endif

using namespace NXOpen;
using namespace NXOpen::BlockStyler;

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace
{
constexpr double kVectorTolerance = 1.0e-6;

const char* kDefaultConfigText =
    "\xEF\xBB\xBF; KeZi engraving text template configuration\r\n"
    "; 可用变量: {文件名} {体名} {属性} {部件属性:属性名} {体属性:属性名} {流水号} {文本}\r\n"
    "; 示例: 模板={文件名}-{体名}-{体属性:编号}-{流水号}-{文本}\r\n"
    "; 流水号样式可填 1、01、001 或 A；A 表示 A,B,C... 字母递增\r\n"
    "[KeZi]\r\n"
    "模板={文本}\r\n"
    "文本=\r\n"
    "属性名=编号,PART_NO,ITEM_NO,Name\r\n"
    "流水号前缀=\r\n"
    "流水号样式=01\r\n"
    "起始号=1\r\n"
    "补零位数=2\r\n";

struct KeZiConfig
{
    std::string textTemplate = "{文本}";
    std::string text;
    std::vector<std::string> attributeNames = {"编号", "PART_NO", "ITEM_NO", "Name"};
    std::string serialPrefix;
    std::string serialStyle = "01";
    int serialStart = 1;
    int serialPad = 2;
};

void ApplyPlanarFaceSelectionFilter(BlockStyler::SelectObject* selection)
{
    if (selection == nullptr)
    {
        return;
    }

    selection->AddFilter(BlockStyler::SelectObject::FilterTypeFaces);

    std::vector<NXOpen::Selection::MaskTriple> masks;
    masks.emplace_back(UF_solid_type, UF_solid_body_subtype, UF_UI_SEL_FEATURE_PLANAR_FACE);
    masks.emplace_back(UF_solid_type, UF_solid_face_subtype, UF_UI_SEL_FEATURE_PLANAR_FACE);

    std::unique_ptr<PropertyList> properties(selection->GetProperties());
    if (properties)
    {
        properties->SetSelectionFilter("SelectionFilter", NXOpen::Selection::SelectionActionClearAndEnableSpecific, masks);
    }
    selection->SetSelectionFilter(NXOpen::Selection::SelectionActionClearAndEnableSpecific, masks);
}

std::string ToString(const NXString& value)
{
    const char* text = value.GetText();
    return text == nullptr ? std::string() : std::string(text);
}

std::string Trim(std::string value)
{
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), value.end());
    return value;
}

std::string StripUtf8Bom(std::string value)
{
    if (value.size() >= 3 &&
        static_cast<unsigned char>(value[0]) == 0xEF &&
        static_cast<unsigned char>(value[1]) == 0xBB &&
        static_cast<unsigned char>(value[2]) == 0xBF)
    {
        value.erase(0, 3);
    }
    return value;
}

std::vector<std::string> SplitComma(const std::string& value)
{
    std::vector<std::string> result;
    std::stringstream input(value);
    std::string item;
    while (std::getline(input, item, ','))
    {
        item = Trim(item);
        if (!item.empty())
        {
            result.push_back(item);
        }
    }
    return result;
}

std::string FormatNumber(const double value)
{
    std::ostringstream out;
    out << std::setprecision(12) << value;
    return out.str();
}

double Dot(const Vector3d& a, const Vector3d& b)
{
    return a.X * b.X + a.Y * b.Y + a.Z * b.Z;
}

Vector3d Cross(const Vector3d& a, const Vector3d& b)
{
    return Vector3d(
        a.Y * b.Z - a.Z * b.Y,
        a.Z * b.X - a.X * b.Z,
        a.X * b.Y - a.Y * b.X);
}

double Magnitude(const Vector3d& value)
{
    return std::sqrt(Dot(value, value));
}

Vector3d Normalize(const Vector3d& value, const Vector3d& fallback)
{
    const double length = Magnitude(value);
    if (length < kVectorTolerance)
    {
        return fallback;
    }
    return Vector3d(value.X / length, value.Y / length, value.Z / length);
}

Matrix3x3 MakeMatrix(const Vector3d& xAxis, const Vector3d& yAxis, const Vector3d& zAxis)
{
    Matrix3x3 matrix;
    matrix.Xx = xAxis.X;
    matrix.Xy = xAxis.Y;
    matrix.Xz = xAxis.Z;
    matrix.Yx = yAxis.X;
    matrix.Yy = yAxis.Y;
    matrix.Yz = yAxis.Z;
    matrix.Zx = zAxis.X;
    matrix.Zy = zAxis.Y;
    matrix.Zz = zAxis.Z;
    return matrix;
}

Point3d OffsetPoint(const Point3d& point, const Vector3d& direction, const double distance)
{
    return Point3d(
        point.X + direction.X * distance,
        point.Y + direction.Y * distance,
        point.Z + direction.Z * distance);
}

Vector3d SubtractPoints(const Point3d& a, const Point3d& b)
{
    return Vector3d(a.X - b.X, a.Y - b.Y, a.Z - b.Z);
}

Vector3d ProjectToPlane(const Vector3d& value, const Vector3d& normal)
{
    const double distance = Dot(value, normal);
    return Vector3d(
        value.X - distance * normal.X,
        value.Y - distance * normal.Y,
        value.Z - distance * normal.Z);
}

std::vector<Point3d> FaceVertices(Face* face)
{
    std::vector<Point3d> points;
    if (face == nullptr)
    {
        return points;
    }

    try
    {
        const std::vector<Edge*> edges = face->GetEdges();
        for (Edge* edge : edges)
        {
            if (edge == nullptr)
            {
                continue;
            }

            Point3d a;
            Point3d b;
            edge->GetVertices(&a, &b);
            points.push_back(a);
            points.push_back(b);
        }
    }
    catch (...)
    {
    }
    return points;
}

double ProjectionSpan(const std::vector<Point3d>& points, const Vector3d& axis, double* center = nullptr)
{
    if (points.empty())
    {
        if (center != nullptr)
        {
            *center = 0.0;
        }
        return 0.0;
    }

    double minValue = std::numeric_limits<double>::max();
    double maxValue = -std::numeric_limits<double>::max();
    for (const Point3d& point : points)
    {
        const double value = point.X * axis.X + point.Y * axis.Y + point.Z * axis.Z;
        minValue = std::min(minValue, value);
        maxValue = std::max(maxValue, value);
    }

    if (center != nullptr)
    {
        *center = 0.5 * (minValue + maxValue);
    }
    return maxValue - minValue;
}

bool FaceLongShortAxes(Face* face, const Vector3d& normal, Vector3d* longAxis, Vector3d* shortAxis)
{
    if (face == nullptr || longAxis == nullptr || shortAxis == nullptr)
    {
        return false;
    }

    const std::vector<Point3d> vertices = FaceVertices(face);
    if (vertices.size() < 2)
    {
        return false;
    }

    Vector3d bestAxis(0.0, 0.0, 0.0);
    double bestLength = 0.0;
    try
    {
        const std::vector<Edge*> edges = face->GetEdges();
        for (Edge* edge : edges)
        {
            if (edge == nullptr)
            {
                continue;
            }

            Point3d a;
            Point3d b;
            edge->GetVertices(&a, &b);
            Vector3d edgeAxis = ProjectToPlane(SubtractPoints(b, a), normal);
            const double edgeLength = Magnitude(edgeAxis);
            if (edgeLength > bestLength)
            {
                bestLength = edgeLength;
                bestAxis = edgeAxis;
            }
        }
    }
    catch (...)
    {
    }

    if (bestLength < kVectorTolerance)
    {
        return false;
    }

    Vector3d longDir = Normalize(bestAxis, Vector3d(1.0, 0.0, 0.0));
    Vector3d shortDir = Normalize(Cross(normal, longDir), Vector3d(0.0, 1.0, 0.0));

    const double longSpan = ProjectionSpan(vertices, longDir);
    const double shortSpan = ProjectionSpan(vertices, shortDir);
    if (shortSpan > longSpan)
    {
        std::swap(longDir, shortDir);
    }

    *longAxis = longDir;
    *shortAxis = shortDir;
    return true;
}

class UfGuard
{
public:
    UfGuard() : ok_(UF_initialize() == 0) {}
    ~UfGuard()
    {
        if (ok_)
        {
            UF_terminate();
        }
    }

    bool ok() const { return ok_; }

private:
    bool ok_;
};

void ShowError(const char* title, const std::string& message)
{
    UI::GetUI()->NXMessageBox()->Show(title, NXMessageBox::DialogTypeError, message.c_str());
}

void Log(Session* session, const std::string& message)
{
    try
    {
        if (session == nullptr || session->ListingWindow() == nullptr)
        {
            return;
        }
        ListingWindow* listing = session->ListingWindow();
        listing->Open();
        listing->WriteLine((std::string("KeZi: ") + message).c_str());
    }
    catch (...)
    {
    }
}

std::filesystem::path PluginDirectory()
{
    wchar_t modulePath[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(reinterpret_cast<HMODULE>(&__ImageBase), modulePath, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
    {
        return std::filesystem::current_path();
    }
    return std::filesystem::path(modulePath).parent_path();
}

std::filesystem::path ConfigFilePath()
{
    return PluginDirectory().parent_path() / L"config" / L"KeZi.ini";
}

void EnsureDefaultConfigFile(const std::filesystem::path& path)
{
    if (std::filesystem::exists(path))
    {
        return;
    }

    std::error_code ignored;
    std::filesystem::create_directories(path.parent_path(), ignored);
    std::ofstream output(path, std::ios::binary);
    if (output)
    {
        output << kDefaultConfigText;
    }
}

std::map<std::string, std::string> ReadIniSection(const std::filesystem::path& path, const std::string& sectionName)
{
    std::map<std::string, std::string> values;
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        return values;
    }

    bool inSection = false;
    std::string line;
    while (std::getline(input, line))
    {
        line = StripUtf8Bom(Trim(line));
        if (line.empty() || line[0] == ';' || line[0] == '#')
        {
            continue;
        }
        if (line.front() == '[' && line.back() == ']')
        {
            inSection = (line.substr(1, line.size() - 2) == sectionName);
            continue;
        }
        if (!inSection)
        {
            continue;
        }
        const std::string::size_type eq = line.find('=');
        if (eq == std::string::npos)
        {
            continue;
        }
        values[Trim(line.substr(0, eq))] = Trim(line.substr(eq + 1));
    }
    return values;
}

int ToInt(const std::string& value, const int fallback)
{
    try
    {
        return std::stoi(value);
    }
    catch (...)
    {
        return fallback;
    }
}

std::string ValueOr(const std::map<std::string, std::string>& values,
                    const std::vector<std::string>& keys,
                    const std::string& fallback)
{
    for (const std::string& key : keys)
    {
        const auto found = values.find(key);
        if (found != values.end())
        {
            return found->second;
        }
    }
    return fallback;
}

KeZiConfig LoadConfig()
{
    const std::filesystem::path path = ConfigFilePath();
    EnsureDefaultConfigFile(path);

    KeZiConfig config;
    const std::map<std::string, std::string> values = ReadIniSection(path, "KeZi");
    config.textTemplate = ValueOr(values, {"模板", "template"}, config.textTemplate);
    config.text = ValueOr(values, {"文本", "text"}, config.text);
    const std::string names = ValueOr(values, {"属性名", "attributes", "attributeNames"}, "");
    if (!names.empty())
    {
        config.attributeNames = SplitComma(names);
    }
    config.serialPrefix = ValueOr(values, {"流水号前缀", "prefix"}, config.serialPrefix);
    config.serialStyle = ValueOr(values, {"流水号样式", "serialStyle", "style"}, config.serialStyle);
    config.serialStart = ToInt(ValueOr(values, {"起始号", "startNumber", "serialStart"}, std::to_string(config.serialStart)), config.serialStart);
    config.serialPad = ToInt(ValueOr(values, {"补零位数", "serialPad", "padWidth"}, std::to_string(config.serialPad)), config.serialPad);
    if (config.attributeNames.empty())
    {
        config.attributeNames = {"编号", "PART_NO", "ITEM_NO", "Name"};
    }
    return config;
}

void WriteConfigValue(const std::string& key, const std::string& value)
{
    const std::filesystem::path path = ConfigFilePath();
    EnsureDefaultConfigFile(path);

    std::ifstream input(path, std::ios::binary);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(input, line))
    {
        lines.push_back(StripUtf8Bom(line));
    }

    bool inSection = false;
    bool wrote = false;
    bool sawSection = false;
    for (std::string& current : lines)
    {
        const std::string trimmed = Trim(current);
        if (trimmed.size() >= 2 && trimmed.front() == '[' && trimmed.back() == ']')
        {
            if (inSection && !wrote)
            {
                current = key + "=" + value + "\r\n" + current;
                wrote = true;
            }
            inSection = (trimmed.substr(1, trimmed.size() - 2) == "KeZi");
            sawSection = sawSection || inSection;
            continue;
        }

        if (!inSection)
        {
            continue;
        }

        const std::string::size_type eq = trimmed.find('=');
        if (eq == std::string::npos)
        {
            continue;
        }
        if (Trim(trimmed.substr(0, eq)) == key)
        {
            current = key + "=" + value;
            wrote = true;
        }
    }

    if (!sawSection)
    {
        lines.push_back("[KeZi]");
    }
    if (!wrote)
    {
        lines.push_back(key + "=" + value);
    }

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << "\xEF\xBB\xBF";
    for (const std::string& outputLine : lines)
    {
        output << outputLine << "\r\n";
    }
}

int ReadEnumValue(Enumeration* block)
{
    if (block == nullptr)
    {
        return 0;
    }

    std::unique_ptr<PropertyList> properties(block->GetProperties());
    return properties ? properties->GetEnum("Value") : 0;
}

std::string ReadString(StringBlock* block)
{
    return block == nullptr ? std::string() : Trim(ToString(block->Value()));
}

double ReadDouble(DoubleBlock* block, const double fallback)
{
    return block == nullptr ? fallback : block->Value();
}

int ReadInteger(IntegerBlock* block, const int fallback)
{
    return block == nullptr ? fallback : block->Value();
}

bool ReadToggle(Toggle* block, const bool fallback)
{
    return block == nullptr ? fallback : block->Value();
}

NXColor* ReadColor(Part* part, ObjectColorPicker* block, const int fallbackColor)
{
    if (part == nullptr || part->Colors() == nullptr)
    {
        return nullptr;
    }

    int color = fallbackColor;
    if (block != nullptr)
    {
        const std::vector<int> values = block->GetValue();
        if (!values.empty())
        {
            color = values.front();
        }
    }
    return part->Colors()->Find(color);
}

std::string ReadAttribute(NXObject* object, const std::string& name)
{
    if (object == nullptr || name.empty())
    {
        return std::string();
    }

    try
    {
        if (object->HasUserAttribute(name.c_str(), NXObject::AttributeTypeString, -1))
        {
            return ToString(object->GetStringUserAttribute(name.c_str(), -1));
        }
    }
    catch (...)
    {
    }
    return std::string();
}

void WriteStringAttribute(NXObject* object, const std::string& name, const std::string& value)
{
    if (object == nullptr || name.empty())
    {
        return;
    }

    try
    {
        object->SetUserAttribute(name.c_str(), -1, value.c_str(), Update::OptionNow);
    }
    catch (...)
    {
    }
}

std::string FirstAvailableAttribute(NXObject* primary, NXObject* secondary, const std::vector<std::string>& names)
{
    for (const std::string& name : names)
    {
        std::string value = ReadAttribute(primary, name);
        if (value.empty())
        {
            value = ReadAttribute(secondary, name);
        }
        if (!value.empty())
        {
            return value;
        }
    }
    return std::string();
}

struct TextSettings
{
    int mode = 0;
    int boundary = 0;
    std::string text;
    std::string fontName = "Arial";
    double height = 10.0;
    double depth = 0.3;
    double boundaryDepth = 0.3;
    double textLength = 0.0;
    double widthScale = 100.0;
    double shear = 0.0;
    int layer = 254;
    bool lockAspect = true;
    bool embossed = false;
    bool vShape = false;
    bool centerLongSide = false;
    bool centerShortSide = false;
    bool xLongSide = false;
    bool xShortSide = false;
};

Tooling::InsertTextBuilder::TextBoundaryType BoundaryTypeFromIndex(const int index)
{
    switch (index)
    {
    case 1:
        return Tooling::InsertTextBuilder::TextBoundaryTypeSlot;
    case 2:
        return Tooling::InsertTextBuilder::TextBoundaryTypeRectangle;
    default:
        return Tooling::InsertTextBuilder::TextBoundaryTypeNone;
    }
}

NXObject* ResolveBodyFromFace(Face* face)
{
    if (face == nullptr)
    {
        return nullptr;
    }

    tag_t bodyTag = NULL_TAG;
    if (UF_MODL_ask_face_body(face->Tag(), &bodyTag) != 0 || bodyTag == NULL_TAG)
    {
        return nullptr;
    }
    return dynamic_cast<NXObject*>(NXObjectManager::Get(bodyTag));
}

std::string PartLeafName(Part* part)
{
    if (part == nullptr)
    {
        return std::string();
    }
    std::string name = ToString(part->Leaf());
    const std::string::size_type dot = name.find_last_of('.');
    if (dot != std::string::npos)
    {
        name = name.substr(0, dot);
    }
    return name;
}

bool AskPlanarFaceData(Face* face, const Point3d* referencePoint, Point3d* point, Vector3d* normal)
{
    if (face == nullptr)
    {
        return false;
    }

    int type = 0;
    double origin[3] = {};
    double dir[3] = {};
    double box[6] = {};
    double radius = 0.0;
    double radData = 0.0;
    int normDir = 0;
    if (UF_MODL_ask_face_data(face->Tag(), &type, origin, dir, box, &radius, &radData, &normDir) != 0)
    {
        return false;
    }

    if (type != UF_MODL_PLANAR_FACE)
    {
        return false;
    }

    double refPoint[3] = {origin[0], origin[1], origin[2]};
    if (referencePoint != nullptr &&
        std::isfinite(referencePoint->X) &&
        std::isfinite(referencePoint->Y) &&
        std::isfinite(referencePoint->Z))
    {
        refPoint[0] = referencePoint->X;
        refPoint[1] = referencePoint->Y;
        refPoint[2] = referencePoint->Z;
    }

    double parm[2] = {};
    double facePoint[3] = {};
    if (UF_MODL_ask_face_parm_2(face->Tag(), refPoint, parm, facePoint) != 0)
    {
        facePoint[0] = origin[0];
        facePoint[1] = origin[1];
        facePoint[2] = origin[2];
    }

    if (point != nullptr)
    {
        *point = Point3d(facePoint[0], facePoint[1], facePoint[2]);
    }
    if (normal != nullptr)
    {
        double propsPoint[3] = {};
        double u1[3] = {};
        double v1[3] = {};
        double u2[3] = {};
        double v2[3] = {};
        double unitNormal[3] = {};
        double radii[2] = {};
        Vector3d value(dir[0], dir[1], dir[2]);
        if (UF_MODL_ask_face_props(face->Tag(), parm, propsPoint, u1, v1, u2, v2, unitNormal, radii) == 0)
        {
            value = Vector3d(unitNormal[0], unitNormal[1], unitNormal[2]);
        }
        else if (normDir < 0)
        {
            value = Vector3d(-value.X, -value.Y, -value.Z);
        }
        *normal = Normalize(value, Vector3d(0.0, 0.0, 1.0));
    }
    return true;
}

std::string SerialNumberText(const KeZiConfig& config)
{
    if (config.serialStyle == "A" || config.serialStyle == "a")
    {
        int value = std::max(1, config.serialStart);
        std::string letters;
        while (value > 0)
        {
            --value;
            letters.insert(letters.begin(), static_cast<char>('A' + (value % 26)));
            value /= 26;
        }
        return config.serialPrefix + letters;
    }

    int width = config.serialPad;
    if (!config.serialStyle.empty() &&
        std::all_of(config.serialStyle.begin(), config.serialStyle.end(), [](unsigned char ch) { return std::isdigit(ch); }))
    {
        width = static_cast<int>(config.serialStyle.size());
    }

    std::ostringstream out;
    out << config.serialPrefix;
    if (width > 1)
    {
        out << std::setw(width) << std::setfill('0');
    }
    out << config.serialStart;
    return out.str();
}

bool TextTemplateHasSerial(const std::string& textTemplate)
{
    return textTemplate.find("{流水号}") != std::string::npos ||
           textTemplate.find("{serial}") != std::string::npos;
}

bool TextTemplateHasBodyName(const std::string& textTemplate)
{
    return textTemplate.find("{体名}") != std::string::npos ||
           textTemplate.find("{body}") != std::string::npos;
}

bool TextTemplateHasFileName(const std::string& textTemplate)
{
    return textTemplate.find("{文件名}") != std::string::npos ||
           textTemplate.find("{file}") != std::string::npos;
}

bool TextTemplateHasText(const std::string& textTemplate)
{
    return textTemplate.find("{文本}") != std::string::npos ||
           textTemplate.find("{text}") != std::string::npos;
}

bool TextTemplateHasAttribute(const std::string& textTemplate)
{
    return textTemplate.find("{属性}") != std::string::npos ||
           textTemplate.find("{attribute}") != std::string::npos ||
           textTemplate.find("{属性:") != std::string::npos ||
           textTemplate.find("{体属性:") != std::string::npos ||
           textTemplate.find("{部件属性:") != std::string::npos ||
           textTemplate.find("{attribute:") != std::string::npos ||
           textTemplate.find("{body_attribute:") != std::string::npos ||
           textTemplate.find("{part_attribute:") != std::string::npos;
}

bool TextTemplateHasRuleToken(const std::string& textTemplate)
{
    return textTemplate.find('{') != std::string::npos &&
           textTemplate.find('}') != std::string::npos;
}

bool IncrementTextSerial(const std::string& text, std::string* nextText)
{
    if (nextText == nullptr || text.empty())
    {
        return false;
    }

    std::size_t end = text.size();
    while (end > 0 && std::isspace(static_cast<unsigned char>(text[end - 1])))
    {
        --end;
    }
    if (end == 0)
    {
        return false;
    }

    std::size_t begin = end;
    while (begin > 0 && std::isdigit(static_cast<unsigned char>(text[begin - 1])))
    {
        --begin;
    }

    if (begin < end)
    {
        const std::string numberText = text.substr(begin, end - begin);
        const int width = static_cast<int>(numberText.size());
        int value = 0;
        try
        {
            value = std::stoi(numberText);
        }
        catch (...)
        {
            return false;
        }

        std::ostringstream number;
        number << std::setw(width) << std::setfill('0') << (value + 1);
        *nextText = text.substr(0, begin) + number.str() + text.substr(end);
        return true;
    }

    begin = end;
    while (begin > 0 && std::isalpha(static_cast<unsigned char>(text[begin - 1])))
    {
        --begin;
    }
    if (begin == end)
    {
        return false;
    }

    std::string letters = text.substr(begin, end - begin);
    int carry = 1;
    for (std::size_t i = letters.size(); i > 0 && carry != 0; --i)
    {
        char& ch = letters[i - 1];
        const bool lower = std::islower(static_cast<unsigned char>(ch)) != 0;
        const char base = lower ? 'a' : 'A';
        const char maxCh = lower ? 'z' : 'Z';
        if (ch >= maxCh)
        {
            ch = base;
        }
        else
        {
            ++ch;
            carry = 0;
        }
    }
    if (carry != 0)
    {
        letters.insert(letters.begin(), std::islower(static_cast<unsigned char>(letters.front())) ? 'a' : 'A');
    }

    *nextText = text.substr(0, begin) + letters + text.substr(end);
    return true;
}

bool RuleAlreadyHasTokenType(const std::string& ruleText, const std::string& token)
{
    if (token == "{流水号}" || token == "{serial}")
    {
        return TextTemplateHasSerial(ruleText);
    }
    if (token == "{体名}" || token == "{body}")
    {
        return TextTemplateHasBodyName(ruleText);
    }
    if (token == "{文件名}" || token == "{file}")
    {
        return TextTemplateHasFileName(ruleText);
    }
    if (token == "{文本}" || token == "{text}")
    {
        return TextTemplateHasText(ruleText);
    }
    if (token.find("属性") != std::string::npos || token.find("attribute") != std::string::npos)
    {
        return TextTemplateHasAttribute(ruleText);
    }
    return ruleText.find(token) != std::string::npos;
}

std::string RuleLabelText(std::string textTemplate)
{
    if (Trim(textTemplate).empty())
    {
        return "文本";
    }

    const auto addBetweenAdjacentTokens = [&textTemplate]() {
        std::string::size_type pos = 0;
        while ((pos = textTemplate.find("}{", pos)) != std::string::npos)
        {
            textTemplate.insert(pos + 1, "+");
            pos += 2;
        }
    };
    addBetweenAdjacentTokens();

    const auto replaceAll = [&textTemplate](const std::string& key, const std::string& replacement) {
        std::string::size_type pos = 0;
        while ((pos = textTemplate.find(key, pos)) != std::string::npos)
        {
            textTemplate.replace(pos, key.size(), replacement);
            pos += replacement.size();
        }
    };

    replaceAll("{text}", "文本");
    replaceAll("{文本}", "文本");
    replaceAll("{body}", "体名");
    replaceAll("{体名}", "体名");
    replaceAll("{file}", "文件名");
    replaceAll("{文件名}", "文件名");
    replaceAll("{serial}", "流水号");
    replaceAll("{流水号}", "流水号");
    replaceAll("{attribute}", "属性");
    replaceAll("{属性}", "属性");
    replaceAll("{属性名}", "属性名");

    const auto stripTokenBraces = [&textTemplate](const std::string& prefix) {
        std::string::size_type start = 0;
        while ((start = textTemplate.find(prefix, start)) != std::string::npos)
        {
            const std::string::size_type end = textTemplate.find('}', start);
            if (end == std::string::npos)
            {
                break;
            }
            const std::string body = textTemplate.substr(start + 1, end - start - 1);
            textTemplate.replace(start, end - start + 1, body);
            start += body.size();
        }
    };
    stripTokenBraces("{体属性:");
    stripTokenBraces("{部件属性:");
    stripTokenBraces("{属性:");
    stripTokenBraces("{body_attribute:");
    stripTokenBraces("{part_attribute:");
    stripTokenBraces("{attribute:");

    return Trim(textTemplate);
}

bool SplitTextAndSerial(const std::string& value, std::string* textPart, std::string* serialPart)
{
    if (textPart == nullptr || serialPart == nullptr)
    {
        return false;
    }

    const std::string trimmed = Trim(value);
    if (trimmed.empty())
    {
        return false;
    }

    std::size_t end = trimmed.size();
    std::size_t begin = end;
    while (begin > 0 && std::isdigit(static_cast<unsigned char>(trimmed[begin - 1])))
    {
        --begin;
    }

    if (begin < end)
    {
        *textPart = trimmed.substr(0, begin);
        *serialPart = trimmed.substr(begin, end - begin);
        return true;
    }

    return false;
}

std::string ExpandTextTemplate(const std::string& textTemplate,
                               const std::string& userText,
                               NXObject* body,
                               Part* part,
                               const KeZiConfig& config)
{
    std::string result = textTemplate.empty() ? "{文本}" : textTemplate;
    NXObject* partObject = dynamic_cast<NXObject*>(part);
    const std::string bodyName = body == nullptr ? std::string() : ToString(body->Name());
    const std::string fileName = PartLeafName(part);
    const bool hasTextToken = result.find("{文本}") != std::string::npos || result.find("{text}") != std::string::npos;
    const bool hasSerialToken = TextTemplateHasSerial(result);
    std::string resolvedUserText = userText;
    std::string serial = Trim(userText).empty() ? SerialNumberText(config) : Trim(userText);
    if (hasTextToken && hasSerialToken)
    {
        std::string textPart;
        std::string serialPart;
        if (SplitTextAndSerial(userText, &textPart, &serialPart))
        {
            resolvedUserText = textPart;
            serial = serialPart;
        }
        else
        {
            resolvedUserText = userText;
            serial = SerialNumberText(config);
        }
    }

    const auto replaceAll = [&result](const std::string& key, const std::string& replacement) {
        std::string::size_type pos = 0;
        while ((pos = result.find(key, pos)) != std::string::npos)
        {
            result.replace(pos, key.size(), replacement);
            pos += replacement.size();
        }
    };

    replaceAll("{text}", resolvedUserText);
    replaceAll("{文本}", resolvedUserText);
    replaceAll("{body}", bodyName);
    replaceAll("{体名}", bodyName);
    replaceAll("{file}", fileName);
    replaceAll("{文件名}", fileName);
    replaceAll("{serial}", serial);
    replaceAll("{流水号}", serial);
    replaceAll("{attribute}", FirstAvailableAttribute(body, partObject, config.attributeNames));
    replaceAll("{属性}", FirstAvailableAttribute(body, partObject, config.attributeNames));
    replaceAll("{属性名}", config.attributeNames.empty() ? std::string() : config.attributeNames.front());

    const auto replaceAttributeToken = [&result](const std::string& prefix, NXObject* object) {
        std::string::size_type start = 0;
        while ((start = result.find(prefix, start)) != std::string::npos)
        {
            const std::string::size_type end = result.find('}', start);
            if (end == std::string::npos)
            {
                break;
            }
            const std::string attributeName = result.substr(start + prefix.size(), end - start - prefix.size());
            const std::string attributeValue = ReadAttribute(object, attributeName);
            result.replace(start, end - start + 1, attributeValue);
            start += attributeValue.size();
        }
    };

    const auto replaceCombinedAttributeToken = [&result](const std::string& prefix, NXObject* primary, NXObject* secondary) {
        std::string::size_type start = 0;
        while ((start = result.find(prefix, start)) != std::string::npos)
        {
            const std::string::size_type end = result.find('}', start);
            if (end == std::string::npos)
            {
                break;
            }
            const std::string attributeName = result.substr(start + prefix.size(), end - start - prefix.size());
            std::string attributeValue = ReadAttribute(primary, attributeName);
            if (attributeValue.empty())
            {
                attributeValue = ReadAttribute(secondary, attributeName);
            }
            result.replace(start, end - start + 1, attributeValue);
            start += attributeValue.size();
        }
    };

    replaceAttributeToken("{body_attribute:", body);
    replaceAttributeToken("{体属性:", body);
    replaceAttributeToken("{part_attribute:", partObject);
    replaceAttributeToken("{部件属性:", partObject);
    replaceCombinedAttributeToken("{attribute:", body, partObject);
    replaceCombinedAttributeToken("{属性:", body, partObject);
    if (result.find('{') == std::string::npos && result.find('}') == std::string::npos)
    {
        return Trim(result);
    }
    return Trim(result);
}
}

class KeZiDialog
{
public:
    KeZiDialog()
    {
        session_ = Session::GetSession();
        ui_ = UI::GetUI();
        dialog_ = ui_->CreateDialog("KeZi.dlx");
        dialog_->AddApplyHandler(make_callback(this, &KeZiDialog::ApplyCb));
        dialog_->AddOkHandler(make_callback(this, &KeZiDialog::OkCb));
        dialog_->AddCancelHandler(make_callback(this, &KeZiDialog::CancelCb));
        dialog_->AddCloseHandler(make_callback(this, &KeZiDialog::CancelCb));
        dialog_->AddUpdateHandler(make_callback(this, &KeZiDialog::UpdateCb));
        dialog_->AddInitializeHandler(make_callback(this, &KeZiDialog::InitializeCb));
        dialog_->AddDialogShownHandler(make_callback(this, &KeZiDialog::DialogShownCb));
    }

    ~KeZiDialog()
    {
        ClearPreviewBuilder();
        delete dialog_;
        dialog_ = nullptr;
    }

    int Show()
    {
        dialog_->Show();
        return 0;
    }

private:
    void InitializeCb()
    {
        Log(session_, "初始化对话框");
        config_ = LoadConfig();
        CompositeBlock* top = dialog_->TopBlock();
        mode_ = dynamic_cast<Enumeration*>(top->FindBlock("mode"));
        manualFace_ = dynamic_cast<BlockStyler::SelectObject*>(top->FindBlock("manualFace"));
        orientation_ = dynamic_cast<SpecifyOrientation*>(top->FindBlock("orientation"));
        rotate90Button_ = dynamic_cast<Button*>(top->FindBlock("rotate90Button"));
        centerLongSide_ = dynamic_cast<Toggle*>(top->FindBlock("centerLongSide"));
        centerShortSide_ = dynamic_cast<Toggle*>(top->FindBlock("centerShortSide"));
        xLongSide_ = dynamic_cast<Toggle*>(top->FindBlock("xLongSide"));
        xShortSide_ = dynamic_cast<Toggle*>(top->FindBlock("xShortSide"));
        textValue_ = dynamic_cast<StringBlock*>(top->FindBlock("textValue"));
        appendBodyName_ = dynamic_cast<Button*>(top->FindBlock("appendBodyName"));
        appendFileName_ = dynamic_cast<Button*>(top->FindBlock("appendFileName"));
        appendSerial_ = dynamic_cast<Button*>(top->FindBlock("appendSerial"));
        appendAttribute_ = dynamic_cast<Button*>(top->FindBlock("appendAttribute"));
        fontName_ = dynamic_cast<StringBlock*>(top->FindBlock("fontName"));
        textHeight_ = dynamic_cast<DoubleBlock*>(top->FindBlock("textHeight"));
        depth_ = dynamic_cast<DoubleBlock*>(top->FindBlock("depth"));
        textColor_ = dynamic_cast<ObjectColorPicker*>(top->FindBlock("textColor"));
        boundary_ = dynamic_cast<Enumeration*>(top->FindBlock("boundary"));
        boundaryDepth_ = dynamic_cast<DoubleBlock*>(top->FindBlock("boundaryDepth"));
        boundaryColor_ = dynamic_cast<ObjectColorPicker*>(top->FindBlock("boundaryColor"));
        margin_ = dynamic_cast<DoubleBlock*>(top->FindBlock("margin"));
        wScale_ = dynamic_cast<DoubleBlock*>(top->FindBlock("wScale"));
        lockAspect_ = dynamic_cast<Toggle*>(top->FindBlock("lockAspect"));
        shear_ = dynamic_cast<DoubleBlock*>(top->FindBlock("shear"));
        textLayer_ = dynamic_cast<IntegerBlock*>(top->FindBlock("textLayer"));
        embossedText_ = dynamic_cast<Toggle*>(top->FindBlock("embossedText"));
        verticalText_ = dynamic_cast<Toggle*>(top->FindBlock("verticalText"));
        editConfig_ = dynamic_cast<Button*>(top->FindBlock("editConfig"));
        ruleText_ = Trim(config_.textTemplate).empty() ? std::string("{文本}") : config_.textTemplate;
        if (Trim(ruleText_) == "{流水号}")
        {
            ruleText_ = "{文本}";
        }

        if (manualFace_ != nullptr)
        {
            manualFace_->SetAutomaticProgression(false);
            manualFace_->SetMaximumScopeAsString("Within Work Part and Components");
            manualFace_->SetCreateInterpartLink(false);
            manualFace_->SetInterpartSelectionAsString("Simple");
            ApplyPlanarFaceSelectionFilter(manualFace_);
        }
        if (orientation_ != nullptr)
        {
            orientation_->SetVisibleManipulatorHandles(0x47);
        }
        if (textValue_ != nullptr)
        {
            const std::string currentText = ReadString(textValue_);
            if (currentText.empty() && !config_.text.empty())
            {
                textValue_->SetValue(config_.text.c_str());
            }
            else if (currentText.empty() && TextTemplateHasSerial(ruleText_))
            {
                const std::string serialText = SerialNumberText(config_);
                textValue_->SetValue(serialText.c_str());
            }
            UpdateTextRuleLabel();
        }
        UpdateUiState();
        Log(session_, "对话框初始化完成");
    }

    void DialogShownCb()
    {
        Log(session_, "对话框显示，读取当前控件值");
        UpdateUiState();
        ApplyExclusiveAxisToggle(xLongSide_);
        ApplyExclusiveAxisToggle(xShortSide_);
        if (orientation_ != nullptr)
        {
            orientation_->SetVisibleManipulatorHandles(0x47);
        }
        ApplyAxisModeToCurrentOrientation();
        RefreshPreview();
        if (textValue_ != nullptr)
        {
            UpdateTextRuleLabel();
            textValue_->Focus();
        }
    }

    int CancelCb()
    {
        Log(session_, "取消/关闭对话框");
        SaveCurrentRule();
        ClearPreviewBuilder();
        return 0;
    }

    int UpdateCb(UIBlock* block)
    {
        if (block == appendBodyName_)
        {
            Log(session_, "追加变量: 体名");
            AppendTextToken("{体名}");
            RefreshPreview();
        }
        else if (block == appendFileName_)
        {
            Log(session_, "追加变量: 文件名");
            AppendTextToken("{文件名}");
            RefreshPreview();
        }
        else if (block == appendSerial_)
        {
            Log(session_, "追加变量: 流水号");
            AppendTextToken("{流水号}");
            RefreshPreview();
        }
        else if (block == appendAttribute_)
        {
            const std::string name = config_.attributeNames.empty() ? std::string("编号") : config_.attributeNames.front();
            Log(session_, std::string("追加变量: 体属性:") + name);
            AppendTextToken("{体属性:" + name + "}");
            RefreshPreview();
        }
        else if (block == rotate90Button_)
        {
            Log(session_, "旋转方位 90 度");
            RotateOrientationNinetyDegrees();
            RefreshPreview();
        }
        else if (block == mode_ || block == boundary_)
        {
            UpdateUiState();
            RefreshPreview();
        }
        else if (block == lockAspect_)
        {
            UpdateUiState();
            RefreshPreview();
        }
        else if (block == manualFace_)
        {
            MoveOrientationToSelectedFace();
            if (orientation_ != nullptr)
            {
                orientation_->Focus();
            }
            RefreshPreview();
        }
        else if (block == xLongSide_ || block == xShortSide_)
        {
            ApplyExclusiveAxisToggle(block);
            ApplyAxisModeToCurrentOrientation();
            RefreshPreview();
        }
        else if (block == manualFace_ || block == orientation_ || block == textValue_ || block == fontName_ ||
                 block == textHeight_ || block == depth_ || block == textColor_ || block == boundaryDepth_ ||
                 block == boundaryColor_ || block == margin_ || block == wScale_ || block == lockAspect_ ||
                 block == shear_ || block == textLayer_ || block == embossedText_ || block == verticalText_ ||
                 block == centerLongSide_ || block == centerShortSide_ || block == xLongSide_ || block == xShortSide_)
        {
            if (block == textValue_ && Trim(ReadString(textValue_)).empty())
            {
                ruleText_ = "{文本}";
                UpdateTextRuleLabel();
                SaveCurrentRule();
            }
            if (block == orientation_ || block == centerLongSide_ || block == centerShortSide_)
            {
                SnapOrientationToCenterOptions();
            }
            RefreshPreview();
        }
        return 0;
    }

    int ApplyCb()
    {
        try
        {
            ApplyEngraving();
        }
        catch (const NXException& ex)
        {
            Log(session_, std::string("刻字失败: ") + ex.Message());
            ShowError("KeZi Engrave Text", std::string("Engraving failed: ") + ex.Message());
            return 1;
        }
        catch (const std::exception& ex)
        {
            Log(session_, std::string("刻字失败: ") + ex.what());
            ShowError("KeZi Engrave Text", std::string("Engraving failed: ") + ex.what());
            return 1;
        }
        return 0;
    }

    int OkCb()
    {
        return ApplyCb();
    }

    void AppendTextToken(const std::string& token)
    {
        if (token.empty())
        {
            return;
        }
        if (RuleAlreadyHasTokenType(ruleText_, token))
        {
            UpdateTextRuleLabel();
            EnsureTextInputHasValue();
            SaveCurrentRule();
            return;
        }
        const std::string currentRule = Trim(ruleText_);
        const bool hasUserText = textValue_ != nullptr && !Trim(ReadString(textValue_)).empty();
        if (!TextTemplateHasRuleToken(ruleText_) ||
            ((currentRule == "{文本}" || currentRule == "{text}") && !hasUserText))
        {
            ruleText_.clear();
        }
        ruleText_ += token;
        UpdateTextRuleLabel();
        EnsureTextInputHasValue();
        SaveCurrentRule();
    }

    void UpdateTextRuleLabel()
    {
        if (textValue_ == nullptr)
        {
            return;
        }
        textValue_->SetLabel(RuleLabelText(ruleText_).c_str());
    }

    void SaveCurrentRule()
    {
        std::string rule = Trim(ruleText_);
        if (rule.empty())
        {
            rule = "{文本}";
        }
        config_.textTemplate = rule;
        WriteConfigValue("模板", rule);
    }

    void EnsureTextInputHasValue()
    {
        if (textValue_ == nullptr || !ReadString(textValue_).empty() || !TextTemplateHasSerial(ruleText_))
        {
            return;
        }
        const std::string serialText = SerialNumberText(config_);
        textValue_->SetValue(serialText.c_str());
    }

    void RotateOrientationNinetyDegrees()
    {
        if (orientation_ == nullptr)
        {
            return;
        }

        const Vector3d xAxis = orientation_->XAxis();
        const Vector3d yAxis = orientation_->YAxis();
        orientation_->SetXAxis(yAxis);
        orientation_->SetYAxis(Vector3d(-xAxis.X, -xAxis.Y, -xAxis.Z));
    }

    void ApplyExclusiveAxisToggle(UIBlock* block)
    {
        if (block == xLongSide_ && ReadToggle(xLongSide_, false) && xShortSide_ != nullptr)
        {
            xShortSide_->SetValue(false);
        }
        else if (block == xShortSide_ && ReadToggle(xShortSide_, false) && xLongSide_ != nullptr)
        {
            xLongSide_->SetValue(false);
        }
    }

    void UpdateUiState()
    {
        const int boundaryValue = ReadEnumValue(boundary_);
        if (boundaryDepth_ != nullptr)
        {
            boundaryDepth_->SetEnable(boundaryValue != 0);
        }
        if (boundaryColor_ != nullptr)
        {
            boundaryColor_->SetEnable(boundaryValue != 0);
        }
        const bool lockAspect = ReadToggle(lockAspect_, true);
        if (wScale_ != nullptr)
        {
            wScale_->SetEnable(!lockAspect);
        }
    }

    TextSettings ReadSettings() const
    {
        TextSettings settings;
        settings.mode = ReadEnumValue(mode_);
        settings.boundary = ReadEnumValue(boundary_);
        settings.text = ReadString(textValue_);
        settings.fontName = ReadString(fontName_);
        settings.height = ReadDouble(textHeight_, 10.0);
        settings.depth = ReadDouble(depth_, 0.3);
        settings.boundaryDepth = ReadDouble(boundaryDepth_, 0.3);
        settings.textLength = ReadDouble(margin_, 0.0);
        settings.widthScale = ReadDouble(wScale_, 100.0);
        settings.lockAspect = ReadToggle(lockAspect_, true);
        settings.shear = ReadDouble(shear_, 0.0);
        settings.layer = ReadInteger(textLayer_, 254);
        settings.embossed = ReadToggle(embossedText_, false);
        settings.vShape = ReadToggle(verticalText_, false);
        settings.centerLongSide = ReadToggle(centerLongSide_, false);
        settings.centerShortSide = ReadToggle(centerShortSide_, false);
        settings.xLongSide = ReadToggle(xLongSide_, false);
        settings.xShortSide = ReadToggle(xShortSide_, false);
        if (settings.fontName.empty())
        {
            settings.fontName = "Arial";
        }
        return settings;
    }

    std::string EffectiveTextTemplate(const TextSettings& settings) const
    {
        (void)settings;
        const std::string dialogRule = Trim(ruleText_);
        if (TextTemplateHasRuleToken(dialogRule))
        {
            return ruleText_;
        }

        const std::string configuredRule = Trim(config_.textTemplate);
        if (dialogRule.empty() && TextTemplateHasRuleToken(configuredRule))
        {
            return config_.textTemplate;
        }

        return "{文本}";
    }

    Face* SelectedFace() const
    {
        if (manualFace_ == nullptr)
        {
            return nullptr;
        }

        const std::vector<TaggedObject*> selected = manualFace_->GetSelectedObjects();
        for (TaggedObject* object : selected)
        {
            if (Face* face = dynamic_cast<Face*>(object))
            {
                return face;
            }
        }
        return nullptr;
    }

    Matrix3x3 TextMatrixFromDialog(Face* face, const Vector3d& faceNormal, const TextSettings& settings) const
    {
        Vector3d xAxis(1.0, 0.0, 0.0);
        Vector3d yAxis(0.0, 1.0, 0.0);
        if (orientation_ != nullptr)
        {
            xAxis = orientation_->XAxis();
            yAxis = orientation_->YAxis();
        }

        Vector3d z = Normalize(faceNormal, Vector3d(0.0, 0.0, 1.0));
        Vector3d x = Normalize(ProjectToPlane(xAxis, z), Vector3d(1.0, 0.0, 0.0));
        if (settings.xLongSide || settings.xShortSide)
        {
            Vector3d longAxis;
            Vector3d shortAxis;
            if (FaceLongShortAxes(face, z, &longAxis, &shortAxis))
            {
                x = settings.xShortSide ? shortAxis : longAxis;
                if (Dot(ProjectToPlane(xAxis, z), x) < 0.0)
                {
                    x = Vector3d(-x.X, -x.Y, -x.Z);
                }
            }
        }
        Vector3d y = Normalize(Cross(z, x), Normalize(yAxis, Vector3d(0.0, 1.0, 0.0)));
        x = Normalize(Cross(y, z), x);
        return MakeMatrix(x, y, z);
    }

    Point3d TextOriginFromPickPoint(const Point3d& facePoint, const Vector3d& faceNormal) const
    {
        if (orientation_ != nullptr && orientation_->IsOriginSpecified())
        {
            Point3d origin = orientation_->Origin();
            if (std::isfinite(origin.X) && std::isfinite(origin.Y) && std::isfinite(origin.Z))
            {
                const Vector3d normal = Normalize(faceNormal, Vector3d(0.0, 0.0, 1.0));
                const Vector3d offset(
                    origin.X - facePoint.X,
                    origin.Y - facePoint.Y,
                    origin.Z - facePoint.Z);
                const double distance = Dot(offset, normal);
                return Point3d(
                    origin.X - distance * normal.X,
                    origin.Y - distance * normal.Y,
                    origin.Z - distance * normal.Z);
            }
        }

        if (manualFace_ == nullptr)
        {
            return facePoint;
        }

        Point3d pickPoint = manualFace_->PickPoint();
        if (!std::isfinite(pickPoint.X) || !std::isfinite(pickPoint.Y) || !std::isfinite(pickPoint.Z))
        {
            return facePoint;
        }

        const Vector3d normal = Normalize(faceNormal, Vector3d(0.0, 0.0, 1.0));
        const Vector3d offset(
            pickPoint.X - facePoint.X,
            pickPoint.Y - facePoint.Y,
            pickPoint.Z - facePoint.Z);
        const double distance = Dot(offset, normal);
        return Point3d(
            pickPoint.X - distance * normal.X,
            pickPoint.Y - distance * normal.Y,
            pickPoint.Z - distance * normal.Z);
    }

    Point3d ApplyCenterOptions(Face* face, const Vector3d& faceNormal, const Point3d& origin, const TextSettings& settings) const
    {
        if (face == nullptr || (!settings.centerLongSide && !settings.centerShortSide))
        {
            return origin;
        }

        const std::vector<Point3d> vertices = FaceVertices(face);
        if (vertices.empty())
        {
            return origin;
        }

        const Vector3d z = Normalize(faceNormal, Vector3d(0.0, 0.0, 1.0));
        Vector3d longAxis;
        Vector3d shortAxis;
        if (!FaceLongShortAxes(face, z, &longAxis, &shortAxis))
        {
            return origin;
        }

        Point3d result = origin;
        if (settings.centerLongSide)
        {
            double center = 0.0;
            ProjectionSpan(vertices, longAxis, &center);
            const double current = result.X * longAxis.X + result.Y * longAxis.Y + result.Z * longAxis.Z;
            result = OffsetPoint(result, longAxis, center - current);
        }
        if (settings.centerShortSide)
        {
            double center = 0.0;
            ProjectionSpan(vertices, shortAxis, &center);
            const double current = result.X * shortAxis.X + result.Y * shortAxis.Y + result.Z * shortAxis.Z;
            result = OffsetPoint(result, shortAxis, center - current);
        }
        return result;
    }

    void ClearPreviewBuilder()
    {
        Tooling::InsertTextBuilder* builder = previewBuilder_;
        previewBuilder_ = nullptr;
        previewUdo_ = nullptr;
        if (builder != nullptr)
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

    void MoveOrientationToSelectedFace()
    {
        if (orientation_ == nullptr)
        {
            return;
        }

        Face* face = SelectedFace();
        if (face == nullptr)
        {
            return;
        }

        Point3d pickedPoint = manualFace_ != nullptr ? manualFace_->PickPoint() : Point3d(0.0, 0.0, 0.0);
        Point3d facePoint;
        Vector3d faceNormal;
        if (!AskPlanarFaceData(face, &pickedPoint, &facePoint, &faceNormal))
        {
            return;
        }

        orientation_->SetOriginSpecified(true);
        orientation_->SetOrigin(facePoint);
        orientation_->SetVisibleManipulatorHandles(0x47);
        ApplyTextAxisModeToOrientation(face, faceNormal, ReadSettings());
        SnapOrientationToCenterOptions();
        Log(session_, "选面后切换到指定方位");
    }

    void ApplyTextAxisModeToOrientation(Face* face, const Vector3d& faceNormal, const TextSettings& settings)
    {
        if (orientation_ == nullptr || face == nullptr)
        {
            return;
        }

        if (!settings.xLongSide && !settings.xShortSide)
        {
            return;
        }

        const Vector3d z = Normalize(faceNormal, Vector3d(0.0, 0.0, 1.0));
        Vector3d longAxis;
        Vector3d shortAxis;
        if (!FaceLongShortAxes(face, z, &longAxis, &shortAxis))
        {
            return;
        }

        Vector3d desiredX = settings.xShortSide ? shortAxis : longAxis;
        const Vector3d currentX = Normalize(ProjectToPlane(orientation_->XAxis(), z), desiredX);
        if (Dot(currentX, desiredX) < 0.0)
        {
            desiredX = Vector3d(-desiredX.X, -desiredX.Y, -desiredX.Z);
        }

        const Vector3d desiredY = Normalize(Cross(z, desiredX), shortAxis);
        orientation_->SetXAxis(desiredX);
        orientation_->SetYAxis(desiredY);
    }

    void ApplyAxisModeToCurrentOrientation()
    {
        if (orientation_ == nullptr)
        {
            return;
        }

        const TextSettings settings = ReadSettings();
        if (!settings.xLongSide && !settings.xShortSide)
        {
            return;
        }

        Face* face = SelectedFace();
        if (face == nullptr)
        {
            return;
        }

        Point3d origin = orientation_->IsOriginSpecified() ? orientation_->Origin() : Point3d(0.0, 0.0, 0.0);
        Point3d facePoint;
        Vector3d faceNormal;
        if (!AskPlanarFaceData(face, &origin, &facePoint, &faceNormal))
        {
            return;
        }

        orientation_->SetVisibleManipulatorHandles(0x47);
        ApplyTextAxisModeToOrientation(face, faceNormal, settings);
        Log(session_, "按 X长/X短调整文字方向");
    }

    void SnapOrientationToCenterOptions()
    {
        if (orientation_ == nullptr)
        {
            return;
        }

        const TextSettings settings = ReadSettings();
        if (!settings.centerLongSide && !settings.centerShortSide)
        {
            return;
        }

        Face* face = SelectedFace();
        if (face == nullptr)
        {
            return;
        }

        Point3d pickedPoint = orientation_->IsOriginSpecified() ? orientation_->Origin() : Point3d(0.0, 0.0, 0.0);
        Point3d facePoint;
        Vector3d faceNormal;
        if (!AskPlanarFaceData(face, &pickedPoint, &facePoint, &faceNormal))
        {
            return;
        }

        const std::vector<Point3d> vertices = FaceVertices(face);
        if (vertices.empty())
        {
            return;
        }

        const Vector3d z = Normalize(faceNormal, Vector3d(0.0, 0.0, 1.0));
        Vector3d longAxis;
        Vector3d shortAxis;
        if (!FaceLongShortAxes(face, z, &longAxis, &shortAxis))
        {
            return;
        }

        Point3d result = facePoint;
        if (settings.centerLongSide)
        {
            double center = 0.0;
            ProjectionSpan(vertices, longAxis, &center);
            const double current = result.X * longAxis.X + result.Y * longAxis.Y + result.Z * longAxis.Z;
            result = OffsetPoint(result, longAxis, center - current);
        }
        if (settings.centerShortSide)
        {
            double center = 0.0;
            ProjectionSpan(vertices, shortAxis, &center);
            const double current = result.X * shortAxis.X + result.Y * shortAxis.Y + result.Z * shortAxis.Z;
            result = OffsetPoint(result, shortAxis, center - current);
        }

        orientation_->SetOriginSpecified(true);
        orientation_->SetOrigin(result);
        Log(session_, "按居中开关调整方位原点");
    }

    Tooling::InsertTextBuilder* CreatePreparedBuilder(NXObject** textUdoOut)
    {
        Log(session_, "开始准备注塑模向导刻字 Builder");
        if (textUdoOut != nullptr)
        {
            *textUdoOut = nullptr;
        }

        TextSettings settings = ReadSettings();
        if (settings.height <= 0.0 || settings.depth <= 0.0 || settings.widthScale <= 0.0)
        {
            throw std::runtime_error("Text height, depth, length, and width ratio must be greater than 0.");
        }

        Face* face = SelectedFace();
        if (face == nullptr)
        {
            throw std::runtime_error("Manual mode requires a selected engraving face.");
        }
        Log(session_, "已取得选择面");

        Point3d facePoint;
        Vector3d faceNormal;
        Point3d pickedPoint(0.0, 0.0, 0.0);
        bool hasPickedPoint = false;
        if (manualFace_ != nullptr)
        {
            pickedPoint = manualFace_->PickPoint();
            hasPickedPoint = std::isfinite(pickedPoint.X) &&
                             std::isfinite(pickedPoint.Y) &&
                             std::isfinite(pickedPoint.Z);
        }
        if (!AskPlanarFaceData(face, hasPickedPoint ? &pickedPoint : nullptr, &facePoint, &faceNormal))
        {
            throw std::runtime_error("Selected face is not a valid planar engraving face.");
        }
        const Point3d pickedOrigin = TextOriginFromPickPoint(facePoint, faceNormal);

        Part* workPart = session_->Parts()->Work();
        if (workPart == nullptr)
        {
            throw std::runtime_error("No work part.");
        }

        NXObject* body = ResolveBodyFromFace(face);
        if (body == nullptr)
        {
            throw std::runtime_error("Could not resolve the body of the selected face.");
        }
        Log(session_, std::string("已取得目标体: ") + ToString(body->Name()));

        if (session_->ToolingSession() != nullptr)
        {
            session_->ToolingSession()->SetWizardType(1);
        }

        Tooling::InsertTextBuilder* builder = workPart->ToolingManager()->Moldwizard()->CreateInsertTextBuilder();
        if (builder == nullptr)
        {
            throw std::runtime_error("Could not create InsertTextBuilder.");
        }

        try
        {
            const std::string textTemplate = EffectiveTextTemplate(settings);
            const std::string textRule = ExpandTextTemplate(textTemplate, settings.text, body, workPart, config_);
            if (textRule.empty())
            {
                throw std::runtime_error("Engraving text is empty.");
            }
            lastTextTemplate_ = textTemplate;
            lastResolvedText_ = textRule;
            lastBody_ = body;
            Log(session_, std::string("解析刻字文本: ") + textRule);
            const Point3d textOrigin = ApplyCenterOptions(face, faceNormal, pickedOrigin, settings);
            const Matrix3x3 textMatrix = TextMatrixFromDialog(face, faceNormal, settings);

            builder->SetInsertTextType(Tooling::InsertTextBuilder::InsertTypeThroughPoint);
            builder->SetLockAspectRatio(settings.lockAspect);
            builder->SetTextLayer(settings.layer);
            builder->ComponentTextHeight()->SetFormula(FormatNumber(settings.height).c_str());
            builder->SetTextRule(textRule.c_str());
            builder->FontHeight()->SetFormula(FormatNumber(settings.height).c_str());
            builder->SetTextPosition(Tooling::InsertTextBuilder::TextPositionOptionTopCenter);
            builder->Offset()->SetFormula(FormatNumber(std::max(0.0, settings.textLength)).c_str());
            if (settings.xShortSide)
            {
                builder->SetNumberingDirection(Tooling::InsertTextBuilder::NumberDirectionOptionAlongYAxis);
            }
            else
            {
                builder->SetNumberingDirection(Tooling::InsertTextBuilder::NumberDirectionOptionAlongXAxis);
            }
            builder->NumberingWidth()->SetFormula(FormatNumber(settings.textLength > 0.0 ? settings.textLength : settings.height).c_str());
            builder->LetteringDepth()->SetFormula(FormatNumber(settings.depth).c_str());
            builder->SetTextColor(ReadColor(workPart, textColor_, 186));
            builder->BoundingDepth()->SetFormula(FormatNumber(settings.boundaryDepth).c_str());
            builder->SetBoundaryColor(ReadColor(workPart, boundaryColor_, 186));
            if (settings.textLength > 0.0)
            {
                builder->TextLength()->SetFormula(FormatNumber(settings.textLength).c_str());
            }
            builder->TextShear()->SetFormula(FormatNumber(settings.shear).c_str());
            builder->SetTextRule(textRule.c_str());
            builder->SetFontName(settings.fontName.c_str());
            builder->SetScript(Features::TextBuilder::ScriptOptionsWestern);
            builder->SetTextWScale(settings.widthScale);
            builder->SetCreateEmbossedText(settings.embossed);
            builder->SetCreateVShapeText(settings.vShape);
            builder->SetBoundaryType(BoundaryTypeFromIndex(settings.boundary));
            builder->CleanUpRedundantData();
            builder->SetCsysOrigin(textOrigin);
            builder->SetCsysMatrix(textMatrix);

            NXObject* textUdo = nullptr;
            builder->CreateNewTextUDO(face, "MW_TEXT_SingleTextUDO", &textUdo);
            if (textUdo == nullptr)
            {
                throw std::runtime_error("InsertTextBuilder did not create preview UDO.");
            }
            Log(session_, "已创建刻字预览 UDO");

            builder->SetSingleTextUDO(textUdo);
            builder->SetLastSelectedFace(face);
            builder->SetFontName(settings.fontName.c_str());
            builder->SetScript(Features::TextBuilder::ScriptOptionsWestern);
            builder->UpdateTextPreview(textUdo);

            SelectionIntentRuleOptions* options = workPart->ScRuleFactory()->CreateRuleOptions();
            options->SetSelectedFromInactive(false);
            std::vector<Face*> faces(1, face);
            FaceDumbRule* faceRule = workPart->ScRuleFactory()->CreateRuleFaceDumb(faces, options);
            delete options;

            std::vector<SelectionIntentRule*> rules;
            rules.push_back(faceRule);
            builder->SelectFace()->ReplaceRules(rules, false);

            builder->SetCsysOrigin(textOrigin);
            builder->SetCsysMatrix(textMatrix);
            builder->UpdateTextUDOClientData(9, true);

            if (textUdoOut != nullptr)
            {
                *textUdoOut = textUdo;
            }
            Log(session_, "Builder 准备完成");
            return builder;
        }
        catch (...)
        {
            builder->Destroy();
            throw;
        }
    }

    void RefreshPreview()
    {
        if (refreshingPreview_)
        {
            return;
        }

        refreshingPreview_ = true;
        ClearPreviewBuilder();
        try
        {
            previewBuilder_ = CreatePreparedBuilder(&previewUdo_);
            Log(session_, "预览刷新完成");
        }
        catch (const NXException& ex)
        {
            Log(session_, std::string("预览失败: ") + ex.Message());
            ClearPreviewBuilder();
        }
        catch (const std::exception& ex)
        {
            Log(session_, std::string("预览失败: ") + ex.what());
            ClearPreviewBuilder();
        }
        catch (...)
        {
            Log(session_, "预览失败: 未知错误");
            ClearPreviewBuilder();
        }
        refreshingPreview_ = false;
    }

    void ApplyEngraving()
    {
        Log(session_, "开始提交刻字");
        std::unique_ptr<Tooling::InsertTextBuilder, void (*)(Tooling::InsertTextBuilder*)> builder(
            nullptr,
            [](Tooling::InsertTextBuilder* value) {
                if (value != nullptr)
                {
                    value->Destroy();
                }
            });

        if (previewBuilder_ != nullptr)
        {
            builder.reset(previewBuilder_);
            previewBuilder_ = nullptr;
            previewUdo_ = nullptr;
        }
        else
        {
            NXObject* textUdo = nullptr;
            builder.reset(CreatePreparedBuilder(&textUdo));
        }

        builder->Commit();
        Log(session_, "刻字 Commit 完成");
        SaveCurrentRule();
        if (lastBody_ != nullptr && !lastResolvedText_.empty())
        {
            WriteStringAttribute(lastBody_, "编号", lastResolvedText_);
            Log(session_, std::string("已写入体属性 编号=") + lastResolvedText_);
        }
        if (TextTemplateHasSerial(lastTextTemplate_) && textValue_ != nullptr)
        {
            std::string nextText;
            const std::string currentText = ReadString(textValue_);
            if (IncrementTextSerial(currentText, &nextText))
            {
                textValue_->SetValue(nextText.c_str());
                config_.text = nextText;
                WriteConfigValue("文本", nextText);
                Log(session_, std::string("流水号已递增为: ") + nextText);
            }
        }
    }

private:
    Session* session_ = nullptr;
    UI* ui_ = nullptr;
    BlockDialog* dialog_ = nullptr;

    Enumeration* mode_ = nullptr;
    BlockStyler::SelectObject* manualFace_ = nullptr;
    SpecifyOrientation* orientation_ = nullptr;
    Button* rotate90Button_ = nullptr;
    Toggle* centerLongSide_ = nullptr;
    Toggle* centerShortSide_ = nullptr;
    Toggle* xLongSide_ = nullptr;
    Toggle* xShortSide_ = nullptr;
    StringBlock* textValue_ = nullptr;
    Button* appendBodyName_ = nullptr;
    Button* appendFileName_ = nullptr;
    Button* appendSerial_ = nullptr;
    Button* appendAttribute_ = nullptr;
    StringBlock* fontName_ = nullptr;
    DoubleBlock* textHeight_ = nullptr;
    DoubleBlock* depth_ = nullptr;
    ObjectColorPicker* textColor_ = nullptr;
    Enumeration* boundary_ = nullptr;
    DoubleBlock* boundaryDepth_ = nullptr;
    ObjectColorPicker* boundaryColor_ = nullptr;
    DoubleBlock* margin_ = nullptr;
    DoubleBlock* wScale_ = nullptr;
    Toggle* lockAspect_ = nullptr;
    DoubleBlock* shear_ = nullptr;
    IntegerBlock* textLayer_ = nullptr;
    Toggle* embossedText_ = nullptr;
    Toggle* verticalText_ = nullptr;
    Button* editConfig_ = nullptr;
    Tooling::InsertTextBuilder* previewBuilder_ = nullptr;
    NXObject* previewUdo_ = nullptr;
    NXObject* lastBody_ = nullptr;
    std::string lastResolvedText_;
    std::string lastTextTemplate_;
    std::string ruleText_;
    bool refreshingPreview_ = false;
    KeZiConfig config_;
};

extern "C" DllExport void ufusr(char* /*param*/, int* /*retcod*/, int /*param_len*/)
{
    UfGuard uf;
    if (!uf.ok())
    {
        ShowError("KeZi Engrave Text", "UF_initialize failed.");
        return;
    }

    try
    {
        KeZiDialog dialog;
        dialog.Show();
    }
    catch (const NXException& ex)
    {
        ShowError("KeZi Engrave Text", ex.Message());
    }
    catch (const std::exception& ex)
    {
        ShowError("KeZi Engrave Text", ex.what());
    }
}

extern "C" DllExport int ufusr_ask_unload()
{
    return static_cast<int>(Session::LibraryUnloadOptionImmediately);
}

extern "C" DllExport void ufusr_cleanup(void)
{
}
