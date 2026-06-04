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

#include <NXOpen/Assemblies_Component.hxx>

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
    "补零位数=2\r\n"
    "字体=Arial\r\n"
    "高度=10\r\n"
    "深度=0.3\r\n"
    "颜色=186\r\n"
    "边界=0\r\n"
    "边界深度=0.3\r\n"
    "边界颜色=186\r\n"
    "长度=100\r\n"
    "W比例=100\r\n"
    "锁定宽高比=1\r\n"
    "剪切=0\r\n"
    "文本层=254\r\n"
    "凸起文本=0\r\n"
    "V形文本=0\r\n"
    "模式=0\r\n"
    "长向居中=0\r\n"
    "短向居中=0\r\n"
    "X长边=0\r\n"
    "X短边=0\r\n";

struct KeZiConfig
{
    std::string textTemplate = "{文本}";
    std::string text;
    std::vector<std::string> attributeNames = {"编号", "PART_NO", "ITEM_NO", "Name"};
    std::string serialPrefix;
    std::string serialStyle = "01";
    int serialStart = 1;
    int serialPad = 2;
    int mode = 0;
    std::string fontName = "Arial";
    double height = 10.0;
    double depth = 0.3;
    int textColor = 186;
    int boundary = 0;
    double boundaryDepth = 0.3;
    int boundaryColor = 186;
    double textLength = 100.0;
    double widthScale = 100.0;
    bool lockAspect = true;
    double shear = 0.0;
    int layer = 254;
    bool embossed = false;
    bool vShape = false;
    bool centerLongSide = false;
    bool centerShortSide = false;
    bool xLongSide = false;
    bool xShortSide = false;
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

double ToDouble(const std::string& value, const double fallback)
{
    try
    {
        return std::stod(value);
    }
    catch (...)
    {
        return fallback;
    }
}

bool ToBool(const std::string& value, const bool fallback)
{
    const std::string text = Trim(value);
    if (text == "1" || text == "true" || text == "True" || text == "TRUE" ||
        text == "yes" || text == "Yes" || text == "YES")
    {
        return true;
    }
    if (text == "0" || text == "false" || text == "False" || text == "FALSE" ||
        text == "no" || text == "No" || text == "NO")
    {
        return false;
    }
    return fallback;
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
    config.mode = ToInt(ValueOr(values, {"模式", "mode"}, std::to_string(config.mode)), config.mode);
    config.fontName = ValueOr(values, {"字体", "fontName"}, config.fontName);
    config.height = ToDouble(ValueOr(values, {"高度", "height"}, FormatNumber(config.height)), config.height);
    config.depth = ToDouble(ValueOr(values, {"深度", "depth"}, FormatNumber(config.depth)), config.depth);
    config.textColor = ToInt(ValueOr(values, {"颜色", "textColor"}, std::to_string(config.textColor)), config.textColor);
    config.boundary = ToInt(ValueOr(values, {"边界", "boundary"}, std::to_string(config.boundary)), config.boundary);
    config.boundaryDepth = ToDouble(ValueOr(values, {"边界深度", "boundaryDepth"}, FormatNumber(config.boundaryDepth)), config.boundaryDepth);
    config.boundaryColor = ToInt(ValueOr(values, {"边界颜色", "boundaryColor"}, std::to_string(config.boundaryColor)), config.boundaryColor);
    config.textLength = ToDouble(ValueOr(values, {"长度", "textLength"}, FormatNumber(config.textLength)), config.textLength);
    config.widthScale = ToDouble(ValueOr(values, {"W比例", "widthScale"}, FormatNumber(config.widthScale)), config.widthScale);
    config.lockAspect = ToBool(ValueOr(values, {"锁定宽高比", "lockAspect"}, config.lockAspect ? "1" : "0"), config.lockAspect);
    config.shear = ToDouble(ValueOr(values, {"剪切", "shear"}, FormatNumber(config.shear)), config.shear);
    config.layer = ToInt(ValueOr(values, {"文本层", "layer"}, std::to_string(config.layer)), config.layer);
    config.embossed = ToBool(ValueOr(values, {"凸起文本", "embossed"}, config.embossed ? "1" : "0"), config.embossed);
    config.vShape = ToBool(ValueOr(values, {"V形文本", "vShape"}, config.vShape ? "1" : "0"), config.vShape);
    config.centerLongSide = ToBool(ValueOr(values, {"长向居中", "centerLongSide"}, config.centerLongSide ? "1" : "0"), config.centerLongSide);
    config.centerShortSide = ToBool(ValueOr(values, {"短向居中", "centerShortSide"}, config.centerShortSide ? "1" : "0"), config.centerShortSide);
    config.xLongSide = ToBool(ValueOr(values, {"X长边", "xLongSide"}, config.xLongSide ? "1" : "0"), config.xLongSide);
    config.xShortSide = ToBool(ValueOr(values, {"X短边", "xShortSide"}, config.xShortSide ? "1" : "0"), config.xShortSide);
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

int ReadColorIndex(ObjectColorPicker* block, const int fallbackColor)
{
    int color = fallbackColor;
    if (block != nullptr)
    {
        const std::vector<int> values = block->GetValue();
        if (!values.empty())
        {
            color = values.front();
        }
    }
    return color;
}

NXColor* ReadColor(Part* part, ObjectColorPicker* block, const int fallbackColor)
{
    if (part == nullptr || part->Colors() == nullptr)
    {
        return nullptr;
    }

    return part->Colors()->Find(ReadColorIndex(block, fallbackColor));
}

void SetEnumValue(Enumeration* block, const int value)
{
    if (block == nullptr)
    {
        return;
    }
    std::unique_ptr<PropertyList> properties(block->GetProperties());
    if (properties)
    {
        properties->SetEnum("Value", value);
    }
}

void SetColorValue(ObjectColorPicker* block, const int color)
{
    if (block != nullptr)
    {
        block->SetValue(std::vector<int>{color});
    }
}

std::string ReadAttribute(NXObject* object, const std::string& name)
{
    if (object == nullptr || name.empty())
    {
        return std::string();
    }

    try
    {
        if (object->HasUserAttribute(name.c_str(), NXObject::AttributeTypeAny, -1))
        {
            return ToString(object->GetUserAttributeAsString(name.c_str(), NXObject::AttributeTypeAny, -1));
        }
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

std::wstring WideFromUtf8(const std::string& text)
{
    if (text.empty())
    {
        return std::wstring();
    }
    const int needed = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    if (needed <= 1)
    {
        return std::wstring();
    }
    std::wstring wide(static_cast<std::size_t>(needed - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, wide.data(), needed);
    return wide;
}

struct MenuDialogState
{
    std::vector<std::wstring> items;
    int selected = -1;
    bool done = false;
    HWND list = nullptr;
};

LRESULT CALLBACK MenuDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    constexpr int kListId = 1001;
    MenuDialogState* state = reinterpret_cast<MenuDialogState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (message)
    {
    case WM_CREATE:
    {
        CREATESTRUCTW* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        state = reinterpret_cast<MenuDialogState*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        state->list = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            L"LISTBOX",
            nullptr,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
            10,
            10,
            300,
            190,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kListId)),
            reinterpret_cast<HINSTANCE>(&__ImageBase),
            nullptr);
        SendMessageW(state->list, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        for (const std::wstring& item : state->items)
        {
            SendMessageW(state->list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item.c_str()));
        }
        SendMessageW(state->list, LB_SETCURSEL, 0, 0);

        HWND ok = CreateWindowExW(
            0,
            L"BUTTON",
            L"确定",
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            150,
            210,
            75,
            26,
            hwnd,
            reinterpret_cast<HMENU>(IDOK),
            reinterpret_cast<HINSTANCE>(&__ImageBase),
            nullptr);
        HWND cancel = CreateWindowExW(
            0,
            L"BUTTON",
            L"取消",
            WS_CHILD | WS_VISIBLE,
            235,
            210,
            75,
            26,
            hwnd,
            reinterpret_cast<HMENU>(IDCANCEL),
            reinterpret_cast<HINSTANCE>(&__ImageBase),
            nullptr);
        SendMessageW(ok, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        SendMessageW(cancel, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        return 0;
    }
    case WM_COMMAND:
        if (state != nullptr && LOWORD(wParam) == kListId && HIWORD(wParam) == LBN_DBLCLK)
        {
            state->selected = static_cast<int>(SendMessageW(state->list, LB_GETCURSEL, 0, 0));
            state->done = true;
            DestroyWindow(hwnd);
            return 0;
        }
        if (state != nullptr && LOWORD(wParam) == IDOK)
        {
            state->selected = static_cast<int>(SendMessageW(state->list, LB_GETCURSEL, 0, 0));
            state->done = true;
            DestroyWindow(hwnd);
            return 0;
        }
        if (state != nullptr && LOWORD(wParam) == IDCANCEL)
        {
            state->selected = -1;
            state->done = true;
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    case WM_CLOSE:
        if (state != nullptr)
        {
            state->selected = -1;
            state->done = true;
        }
        DestroyWindow(hwnd);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

int AskMenuIndex(const std::string& title, const std::vector<std::string>& items)
{
    if (items.empty())
    {
        return -1;
    }

    MenuDialogState state;
    for (const std::string& item : items)
    {
        state.items.push_back(WideFromUtf8(item));
    }

    HINSTANCE instance = reinterpret_cast<HINSTANCE>(&__ImageBase);
    static bool registered = false;
    if (!registered)
    {
        WNDCLASSW wc = {};
        wc.lpfnWndProc = MenuDialogProc;
        wc.hInstance = instance;
        wc.lpszClassName = L"KeZiMenuDialog";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
        RegisterClassW(&wc);
        registered = true;
    }

    HWND owner = GetActiveWindow();
    RECT ownerRect = {};
    GetWindowRect(owner, &ownerRect);
    const int width = 340;
    const int height = 285;
    int x = ownerRect.left + ((ownerRect.right - ownerRect.left) - width) / 2;
    int y = ownerRect.top + ((ownerRect.bottom - ownerRect.top) - height) / 2;
    if (owner == nullptr || x < 0 || y < 0)
    {
        x = CW_USEDEFAULT;
        y = CW_USEDEFAULT;
    }

    HWND hwnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        L"KeZiMenuDialog",
        WideFromUtf8(title).c_str(),
        WS_CAPTION | WS_SYSMENU | WS_POPUP,
        x,
        y,
        width,
        height,
        owner,
        nullptr,
        instance,
        &state);
    if (hwnd == nullptr)
    {
        return -1;
    }

    if (owner != nullptr)
    {
        EnableWindow(owner, FALSE);
    }
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg = {};
    while (!state.done && GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        if (!IsDialogMessageW(hwnd, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (owner != nullptr)
    {
        EnableWindow(owner, TRUE);
        SetActiveWindow(owner);
    }
    return state.selected >= 0 && state.selected < static_cast<int>(items.size()) ? state.selected : -1;
}

std::string AskMenuItemPaged(const std::string& title, const std::vector<std::string>& items)
{
    if (items.empty())
    {
        return std::string();
    }

    constexpr std::size_t kPageSize = 12;
    std::size_t page = 0;
    while (true)
    {
        const std::size_t begin = page * kPageSize;
        const std::size_t end = std::min(begin + kPageSize, items.size());
        std::vector<std::string> pageItems;
        std::vector<int> pageMap;
        for (std::size_t i = begin; i < end; ++i)
        {
            pageItems.push_back(items[i]);
            pageMap.push_back(static_cast<int>(i));
        }
        if (page > 0)
        {
            pageItems.push_back("上一页");
            pageMap.push_back(-2);
        }
        if (end < items.size())
        {
            pageItems.push_back("下一页");
            pageMap.push_back(-3);
        }

        const int selected = AskMenuIndex(title, pageItems);
        if (selected < 0)
        {
            return std::string();
        }
        const int mapped = pageMap[static_cast<std::size_t>(selected)];
        if (mapped == -2)
        {
            --page;
            continue;
        }
        if (mapped == -3)
        {
            ++page;
            continue;
        }
        return items[static_cast<std::size_t>(mapped)];
    }
}

std::vector<std::string> UserAttributeNames(NXObject* object)
{
    std::vector<std::string> names;
    if (object == nullptr)
    {
        return names;
    }

    try
    {
        const std::vector<NXObject::AttributeInformation> attributes = object->GetUserAttributes();
        for (const NXObject::AttributeInformation& attribute : attributes)
        {
            if (attribute.Unset || attribute.OwnedBySystem)
            {
                continue;
            }
            std::string title = Trim(ToString(attribute.Title));
            if (title.empty())
            {
                continue;
            }
            if (std::find(names.begin(), names.end(), title) == names.end())
            {
                names.push_back(title);
            }
        }
    }
    catch (...)
    {
    }

    std::sort(names.begin(), names.end());
    return names;
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

Part* SetWorkPartForFace(Session* session, Face* face)
{
    if (session == nullptr || face == nullptr)
    {
        return nullptr;
    }

    PartCollection* parts = session->Parts();
    if (parts == nullptr)
    {
        return nullptr;
    }

    if (face->IsOccurrence())
    {
        Assemblies::Component* component = face->OwningComponent();
        if (component != nullptr)
        {
            PartLoadStatus* loadStatus = nullptr;
            parts->SetWorkComponent(
                component,
                PartCollection::RefsetOptionCurrent,
                PartCollection::WorkComponentOptionGiven,
                &loadStatus);
            delete loadStatus;
            return parts->Work();
        }
    }

    BasePart* owningBasePart = face->OwningPart();
    Part* owningPart = dynamic_cast<Part*>(owningBasePart);
    if (owningPart != nullptr && parts->Work() != owningPart)
    {
        parts->SetWork(owningPart);
    }
    return parts->Work();
}

void RestoreWorkContext(PartCollection* parts, BasePart* previousWorkPart, Assemblies::Component* previousWorkComponent)
{
    if (parts == nullptr)
    {
        return;
    }

    try
    {
        if (previousWorkComponent != nullptr)
        {
            PartLoadStatus* loadStatus = nullptr;
            parts->SetWorkComponent(
                previousWorkComponent,
                PartCollection::RefsetOptionCurrent,
                PartCollection::WorkComponentOptionGiven,
                &loadStatus);
            delete loadStatus;
        }
        else if (previousWorkPart != nullptr)
        {
            parts->SetWork(previousWorkPart);
        }
        else
        {
            PartLoadStatus* loadStatus = nullptr;
            parts->SetWorkComponent(nullptr, &loadStatus);
            delete loadStatus;
        }
    }
    catch (...)
    {
    }
}

class WorkContextGuard
{
public:
    WorkContextGuard(Session* session, Face* targetFace)
    {
        if (session == nullptr || targetFace == nullptr)
        {
            return;
        }
        parts_ = session->Parts();
        if (parts_ == nullptr)
        {
            return;
        }
        previousWorkPart_ = parts_->BaseWork();
        previousWorkComponent_ = parts_->WorkComponent();
        targetWorkPart_ = SetWorkPartForFace(session, targetFace);
    }

    ~WorkContextGuard()
    {
        Restore();
    }

    WorkContextGuard(const WorkContextGuard&) = delete;
    WorkContextGuard& operator=(const WorkContextGuard&) = delete;

    Part* WorkPart() const
    {
        return targetWorkPart_;
    }

    void Restore()
    {
        if (!restored_)
        {
            RestoreWorkContext(parts_, previousWorkPart_, previousWorkComponent_);
            restored_ = true;
        }
    }

private:
    PartCollection* parts_ = nullptr;
    BasePart* previousWorkPart_ = nullptr;
    Assemblies::Component* previousWorkComponent_ = nullptr;
    Part* targetWorkPart_ = nullptr;
    bool restored_ = false;
};

Face* PrototypeFace(Face* face)
{
    if (face == nullptr || !face->IsOccurrence())
    {
        return face;
    }

    INXObject* prototype = face->Prototype();
    Face* prototypeFace = dynamic_cast<Face*>(prototype);
    return prototypeFace != nullptr ? prototypeFace : face;
}

Point3d ComponentPointToPrototype(Assemblies::Component* component, const Point3d& point)
{
    if (component == nullptr)
    {
        return point;
    }

    Point3d origin;
    Matrix3x3 orientation;
    component->GetPosition(&origin, &orientation);
    const Vector3d delta(point.X - origin.X, point.Y - origin.Y, point.Z - origin.Z);
    return Point3d(
        delta.X * orientation.Xx + delta.Y * orientation.Xy + delta.Z * orientation.Xz,
        delta.X * orientation.Yx + delta.Y * orientation.Yy + delta.Z * orientation.Yz,
        delta.X * orientation.Zx + delta.Y * orientation.Zy + delta.Z * orientation.Zz);
}

Vector3d ComponentVectorToPrototype(Assemblies::Component* component, const Vector3d& vector)
{
    if (component == nullptr)
    {
        return vector;
    }

    Point3d origin;
    Matrix3x3 orientation;
    component->GetPosition(&origin, &orientation);
    return Vector3d(
        vector.X * orientation.Xx + vector.Y * orientation.Xy + vector.Z * orientation.Xz,
        vector.X * orientation.Yx + vector.Y * orientation.Yy + vector.Z * orientation.Yz,
        vector.X * orientation.Zx + vector.Y * orientation.Zy + vector.Z * orientation.Zz);
}

Point3d ProjectPointToFacePlane(const Point3d& facePoint, const Vector3d& faceNormal, const Point3d& point)
{
    const Vector3d normal = Normalize(faceNormal, Vector3d(0.0, 0.0, 1.0));
    const Vector3d offset(point.X - facePoint.X, point.Y - facePoint.Y, point.Z - facePoint.Z);
    const double distance = Dot(offset, normal);
    return Point3d(
        point.X - distance * normal.X,
        point.Y - distance * normal.Y,
        point.Z - distance * normal.Z);
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
        return letters;
    }

    int width = config.serialPad;
    if (!config.serialStyle.empty() &&
        std::all_of(config.serialStyle.begin(), config.serialStyle.end(), [](unsigned char ch) { return std::isdigit(ch); }))
    {
        width = static_cast<int>(config.serialStyle.size());
    }

    std::ostringstream out;
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

bool LooksLikeSerialValue(const std::string& text)
{
    const std::string value = Trim(text);
    if (value.empty())
    {
        return false;
    }

    const bool allDigits = std::all_of(value.begin(), value.end(), [](unsigned char ch) {
        return std::isdigit(ch) != 0;
    });
    if (allDigits)
    {
        return true;
    }

    return std::all_of(value.begin(), value.end(), [](unsigned char ch) {
        return std::isalpha(ch) != 0;
    });
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
        return ruleText.find(token) != std::string::npos;
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

std::string RuleTextFromLabel(const std::string& label)
{
    std::string text = Trim(label);
    if (text.empty())
    {
        return "{文本}";
    }
    if (TextTemplateHasRuleToken(text))
    {
        return text;
    }

    std::string result;
    std::stringstream stream(text);
    std::string item;
    while (std::getline(stream, item, '+'))
    {
        item = Trim(item);
        if (item.empty())
        {
            continue;
        }

        if (item == "文本" || item == "text")
        {
            result += "{文本}";
        }
        else if (item == "流水号" || item == "serial")
        {
            result += "{流水号}";
        }
        else if (item == "体名" || item == "body")
        {
            result += "{体名}";
        }
        else if (item == "文件名" || item == "file")
        {
            result += "{文件名}";
        }
        else if (item == "属性" || item == "attribute")
        {
            result += "{属性}";
        }
        else if (item.rfind("体属性:", 0) == 0)
        {
            result += "{" + item + "}";
        }
        else if (item.rfind("部件属性:", 0) == 0)
        {
            result += "{" + item + "}";
        }
        else if (item.rfind("属性:", 0) == 0)
        {
            result += "{" + item + "}";
        }
        else
        {
            result += item;
        }
    }

    return result.empty() ? std::string("{文本}") : result;
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

bool ExtractTrailingSerial(const std::string& value, std::string* serialPart)
{
    if (serialPart == nullptr)
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
        *serialPart = trimmed.substr(begin, end - begin);
        return true;
    }

    begin = end;
    while (begin > 0 && std::isupper(static_cast<unsigned char>(trimmed[begin - 1])))
    {
        --begin;
    }
    if (begin < end)
    {
        *serialPart = trimmed.substr(begin, end - begin);
        return true;
    }

    return false;
}

struct TemplateToken
{
    enum class Kind
    {
        Text,
        Serial,
        Fixed
    };

    Kind kind = Kind::Fixed;
    std::string value;
};

std::vector<std::string> RawTemplateTokens(const std::string& textTemplate)
{
    std::vector<std::string> tokens;
    std::string::size_type start = 0;
    while ((start = textTemplate.find('{', start)) != std::string::npos)
    {
        const std::string::size_type end = textTemplate.find('}', start);
        if (end == std::string::npos)
        {
            break;
        }
        tokens.push_back(textTemplate.substr(start, end - start + 1));
        start = end + 1;
    }
    return tokens;
}

bool IsTextToken(const std::string& token)
{
    return token == "{文本}" || token == "{text}";
}

bool IsSerialToken(const std::string& token)
{
    return token == "{流水号}" || token == "{serial}";
}

std::string AttributeNameFromToken(const std::string& token, const std::string& prefix)
{
    if (token.rfind(prefix, 0) != 0 || token.empty() || token.back() != '}')
    {
        return std::string();
    }
    return token.substr(prefix.size(), token.size() - prefix.size() - 1);
}

std::string ResolveFixedTokenValue(const std::string& token,
                                   NXObject* body,
                                   NXObject* partObject,
                                   const std::string& bodyName,
                                   const std::string& fileName,
                                   const KeZiConfig& config)
{
    if (token == "{body}" || token == "{体名}")
    {
        return bodyName;
    }
    if (token == "{file}" || token == "{文件名}")
    {
        return fileName;
    }
    if (token == "{attribute}" || token == "{属性}")
    {
        return FirstAvailableAttribute(body, partObject, config.attributeNames);
    }
    if (token == "{属性名}")
    {
        return config.attributeNames.empty() ? std::string() : config.attributeNames.front();
    }

    const std::vector<std::pair<std::string, NXObject*>> attributePrefixes = {
        {"{体属性:", body},
        {"{body_attribute:", body},
        {"{部件属性:", partObject},
        {"{part_attribute:", partObject},
        {"{属性:", body},
        {"{attribute:", body},
    };
    for (const auto& entry : attributePrefixes)
    {
        const std::string name = AttributeNameFromToken(token, entry.first);
        if (!name.empty())
        {
            std::string value = ReadAttribute(entry.second, name);
            if (value.empty() && (entry.first == "{属性:" || entry.first == "{attribute:"))
            {
                value = ReadAttribute(partObject, name);
            }
            return value;
        }
    }

    return token;
}

std::vector<TemplateToken> BuildTemplateTokens(const std::string& textTemplate,
                                               NXObject* body,
                                               NXObject* partObject,
                                               const std::string& bodyName,
                                               const std::string& fileName,
                                               const KeZiConfig& config)
{
    std::vector<TemplateToken> tokens;
    for (const std::string& rawToken : RawTemplateTokens(textTemplate))
    {
        TemplateToken token;
        if (IsTextToken(rawToken))
        {
            token.kind = TemplateToken::Kind::Text;
        }
        else if (IsSerialToken(rawToken))
        {
            token.kind = TemplateToken::Kind::Serial;
        }
        else
        {
            token.kind = TemplateToken::Kind::Fixed;
            token.value = ResolveFixedTokenValue(rawToken, body, partObject, bodyName, fileName, config);
        }
        tokens.push_back(token);
    }
    return tokens;
}

std::string JoinFixedTokens(const std::vector<TemplateToken>& tokens, std::size_t begin, std::size_t end)
{
    std::string result;
    for (std::size_t i = begin; i < end && i < tokens.size(); ++i)
    {
        if (tokens[i].kind == TemplateToken::Kind::Fixed)
        {
            result += tokens[i].value;
        }
    }
    return result;
}

bool StartsWithText(const std::string& value, const std::string& prefix)
{
    return prefix.empty() || value.rfind(prefix, 0) == 0;
}

bool EndsWithText(const std::string& value, const std::string& suffix)
{
    return suffix.empty() ||
           (value.size() >= suffix.size() && value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0);
}

void ResolveEditableValuesFromDisplay(const std::vector<TemplateToken>& tokens,
                                      const std::string& displayText,
                                      const KeZiConfig& config,
                                      std::string* textValue,
                                      std::string* serialValue)
{
    if (textValue == nullptr || serialValue == nullptr)
    {
        return;
    }

    *textValue = displayText;
    *serialValue = SerialNumberText(config);

    int textIndex = -1;
    int serialIndex = -1;
    for (std::size_t i = 0; i < tokens.size(); ++i)
    {
        if (tokens[i].kind == TemplateToken::Kind::Text)
        {
            textIndex = static_cast<int>(i);
        }
        else if (tokens[i].kind == TemplateToken::Kind::Serial)
        {
            serialIndex = static_cast<int>(i);
        }
    }

    if (textIndex < 0 && serialIndex < 0)
    {
        return;
    }

    std::string editable = Trim(displayText);
    const int firstEditable = textIndex >= 0 && serialIndex >= 0 ? std::min(textIndex, serialIndex) : std::max(textIndex, serialIndex);
    const int lastEditable = textIndex >= 0 && serialIndex >= 0 ? std::max(textIndex, serialIndex) : std::max(textIndex, serialIndex);
    const std::string prefix = JoinFixedTokens(tokens, 0, static_cast<std::size_t>(firstEditable));
    const std::string suffix = JoinFixedTokens(tokens, static_cast<std::size_t>(lastEditable + 1), tokens.size());
    if (StartsWithText(editable, prefix))
    {
        editable.erase(0, prefix.size());
    }
    if (EndsWithText(editable, suffix))
    {
        editable.erase(editable.size() - suffix.size());
    }

    if (textIndex >= 0 && serialIndex >= 0)
    {
        const std::string between = textIndex < serialIndex
                                        ? JoinFixedTokens(tokens, static_cast<std::size_t>(textIndex + 1), static_cast<std::size_t>(serialIndex))
                                        : JoinFixedTokens(tokens, static_cast<std::size_t>(serialIndex + 1), static_cast<std::size_t>(textIndex));
        if (!between.empty())
        {
            const std::string::size_type split = editable.find(between);
            if (split != std::string::npos)
            {
                if (textIndex < serialIndex)
                {
                    *textValue = editable.substr(0, split);
                    *serialValue = editable.substr(split + between.size());
                }
                else
                {
                    *serialValue = editable.substr(0, split);
                    *textValue = editable.substr(split + between.size());
                }
                return;
            }
        }

        std::string textPart;
        std::string serialPart;
        if (textIndex < serialIndex && SplitTextAndSerial(editable, &textPart, &serialPart))
        {
            *textValue = textPart;
            *serialValue = serialPart;
            return;
        }
        if (serialIndex < textIndex)
        {
            std::string serialPart;
            if (ExtractTrailingSerial(editable, &serialPart) && editable.size() >= serialPart.size())
            {
                *serialValue = serialPart;
                *textValue = editable.substr(serialPart.size());
                return;
            }
        }
    }
    else if (textIndex >= 0)
    {
        *textValue = editable;
    }
    else if (serialIndex >= 0)
    {
        std::string serialPart;
        if (ExtractTrailingSerial(editable, &serialPart) && LooksLikeSerialValue(serialPart))
        {
            *serialValue = serialPart;
        }
    }
}

std::string ComposeTemplateDisplayText(const std::vector<TemplateToken>& tokens,
                                       const std::string& textValue,
                                       const std::string& serialValue)
{
    std::string result;
    for (const TemplateToken& token : tokens)
    {
        if (token.kind == TemplateToken::Kind::Text)
        {
            result += textValue;
        }
        else if (token.kind == TemplateToken::Kind::Serial)
        {
            result += serialValue;
        }
        else
        {
            result += token.value;
        }
    }
    return result;
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
    const std::vector<TemplateToken> tokens = BuildTemplateTokens(result, body, partObject, bodyName, fileName, config);
    const bool hasTextToken = std::any_of(tokens.begin(), tokens.end(), [](const TemplateToken& token) { return token.kind == TemplateToken::Kind::Text; });
    const bool hasSerialToken = std::any_of(tokens.begin(), tokens.end(), [](const TemplateToken& token) { return token.kind == TemplateToken::Kind::Serial; });
    std::string resolvedUserText = userText;
    std::string serial = SerialNumberText(config);
    if (hasTextToken || hasSerialToken)
    {
        ResolveEditableValuesFromDisplay(tokens, userText, config, &resolvedUserText, &serial);
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
        ruleValue_ = dynamic_cast<StringBlock*>(top->FindBlock("ruleValue"));
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
        ruleText_ = config_.textTemplate;
        if (Trim(ruleText_).empty() || Trim(ruleText_) == "{文本}" || Trim(ruleText_) == "{text}")
        {
            ruleText_ = config_.text;
        }
        if (Trim(ruleText_) == "{流水号}" || Trim(ruleText_) == "{serial}")
        {
            ruleText_ = config_.text;
        }
        ApplyConfigToDialog();

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
        UpdateRuleInputValue();
        UpdateResolvedTextFromRule();
        UpdateUiState();
        Log(session_, "对话框初始化完成");
    }

    void ApplyConfigToDialog()
    {
        SetEnumValue(mode_, config_.mode);
        SetEnumValue(boundary_, config_.boundary);
        if (fontName_ != nullptr)
        {
            fontName_->SetValue(config_.fontName.c_str());
        }
        if (textHeight_ != nullptr)
        {
            textHeight_->SetValue(config_.height);
        }
        if (depth_ != nullptr)
        {
            depth_->SetValue(config_.depth);
        }
        SetColorValue(textColor_, config_.textColor);
        if (boundaryDepth_ != nullptr)
        {
            boundaryDepth_->SetValue(config_.boundaryDepth);
        }
        SetColorValue(boundaryColor_, config_.boundaryColor);
        if (margin_ != nullptr)
        {
            margin_->SetValue(config_.textLength);
        }
        if (wScale_ != nullptr)
        {
            wScale_->SetValue(config_.widthScale);
        }
        if (lockAspect_ != nullptr)
        {
            lockAspect_->SetValue(config_.lockAspect);
        }
        if (shear_ != nullptr)
        {
            shear_->SetValue(config_.shear);
        }
        if (textLayer_ != nullptr)
        {
            textLayer_->SetValue(config_.layer);
        }
        if (embossedText_ != nullptr)
        {
            embossedText_->SetValue(config_.embossed);
        }
        if (verticalText_ != nullptr)
        {
            verticalText_->SetValue(config_.vShape);
        }
        if (centerLongSide_ != nullptr)
        {
            centerLongSide_->SetValue(config_.centerLongSide);
        }
        if (centerShortSide_ != nullptr)
        {
            centerShortSide_->SetValue(config_.centerShortSide);
        }
        if (xLongSide_ != nullptr)
        {
            xLongSide_->SetValue(config_.xLongSide);
        }
        if (xShortSide_ != nullptr)
        {
            xShortSide_->SetValue(config_.xShortSide);
        }
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
            UpdateRuleInputValue();
            UpdateResolvedTextFromRule();
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
        if (block == ruleValue_ && updatingRuleInput_)
        {
            return 0;
        }
        if (block == textValue_ && updatingResolvedText_)
        {
            return 0;
        }

        if (block == ruleValue_)
        {
            ruleText_ = ReadString(ruleValue_);
            SaveCurrentRule();
            UpdateResolvedTextFromRule();
            RefreshPreview();
            return 0;
        }

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
            ChooseAndAppendAttributeToken();
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
        ruleText_ = ReadString(ruleValue_);
        if (RuleAlreadyHasTokenType(ruleText_, token))
        {
            SaveCurrentRule();
            UpdateResolvedTextFromRule();
            return;
        }
        ruleText_ += token;
        UpdateTextRuleLabel();
        SaveCurrentRule();
        UpdateResolvedTextFromRule();
    }

    void UpdateTextRuleLabel()
    {
        if (textValue_ != nullptr)
        {
            textValue_->SetLabel("文本");
        }
    }

    void UpdateRuleInputValue()
    {
        if (ruleValue_ == nullptr)
        {
            return;
        }
        if (ReadString(ruleValue_) == ruleText_)
        {
            return;
        }
        updatingRuleInput_ = true;
        ruleValue_->SetValue(ruleText_.c_str());
        updatingRuleInput_ = false;
    }

    void SaveCurrentRule()
    {
        std::string rule = Trim(ruleText_);
        config_.textTemplate = rule;
        WriteConfigValue("模板", rule);
        UpdateRuleInputValue();
    }

    static std::string BoolText(const bool value)
    {
        return value ? "1" : "0";
    }

    void SaveDialogValues()
    {
        const TextSettings settings = ReadSettings();
        WriteConfigValue("模式", std::to_string(settings.mode));
        WriteConfigValue("文本", settings.text);
        WriteConfigValue("字体", settings.fontName);
        WriteConfigValue("高度", FormatNumber(settings.height));
        WriteConfigValue("深度", FormatNumber(settings.depth));
        WriteConfigValue("颜色", std::to_string(ReadColorIndex(textColor_, config_.textColor)));
        WriteConfigValue("边界", std::to_string(settings.boundary));
        WriteConfigValue("边界深度", FormatNumber(settings.boundaryDepth));
        WriteConfigValue("边界颜色", std::to_string(ReadColorIndex(boundaryColor_, config_.boundaryColor)));
        WriteConfigValue("长度", FormatNumber(settings.textLength));
        WriteConfigValue("W比例", FormatNumber(settings.widthScale));
        WriteConfigValue("锁定宽高比", BoolText(settings.lockAspect));
        WriteConfigValue("剪切", FormatNumber(settings.shear));
        WriteConfigValue("文本层", std::to_string(settings.layer));
        WriteConfigValue("凸起文本", BoolText(settings.embossed));
        WriteConfigValue("V形文本", BoolText(settings.vShape));
        WriteConfigValue("长向居中", BoolText(settings.centerLongSide));
        WriteConfigValue("短向居中", BoolText(settings.centerShortSide));
        WriteConfigValue("X长边", BoolText(settings.xLongSide));
        WriteConfigValue("X短边", BoolText(settings.xShortSide));

        config_.mode = settings.mode;
        config_.text = settings.text;
        config_.fontName = settings.fontName;
        config_.height = settings.height;
        config_.depth = settings.depth;
        config_.textColor = ReadColorIndex(textColor_, config_.textColor);
        config_.boundary = settings.boundary;
        config_.boundaryDepth = settings.boundaryDepth;
        config_.boundaryColor = ReadColorIndex(boundaryColor_, config_.boundaryColor);
        config_.textLength = settings.textLength;
        config_.widthScale = settings.widthScale;
        config_.lockAspect = settings.lockAspect;
        config_.shear = settings.shear;
        config_.layer = settings.layer;
        config_.embossed = settings.embossed;
        config_.vShape = settings.vShape;
        config_.centerLongSide = settings.centerLongSide;
        config_.centerShortSide = settings.centerShortSide;
        config_.xLongSide = settings.xLongSide;
        config_.xShortSide = settings.xShortSide;
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

    void AppendDisplayValueForToken(const std::string& token)
    {
        if (textValue_ == nullptr)
        {
            return;
        }

        if (token == "{流水号}" || token == "{serial}")
        {
            const std::string current = ReadString(textValue_);
            const std::string serialText = SerialNumberText(config_);
            updatingResolvedText_ = true;
            textValue_->SetValue((current + serialText).c_str());
            updatingResolvedText_ = false;
        }
    }

    void SetTextInputResolvedValue(const std::string& value)
    {
        if (textValue_ == nullptr || ReadString(textValue_) == value)
        {
            return;
        }

        updatingResolvedText_ = true;
        textValue_->SetValue(value.c_str());
        updatingResolvedText_ = false;
    }

    void UpdateResolvedTextFromRule()
    {
        if (textValue_ == nullptr)
        {
            return;
        }

        const std::string rule = Trim(ruleText_);
        if (rule.empty())
        {
            SetTextInputResolvedValue("");
            return;
        }

        NXObject* body = lastBody_;
        Part* part = lastWorkPart_;
        if (Face* selectedFace = SelectedFace())
        {
            Face* face = PrototypeFace(selectedFace);
            if (face != nullptr)
            {
                body = ResolveBodyFromFace(face);
                part = dynamic_cast<Part*>(face->OwningPart());
            }
        }
        if (part == nullptr)
        {
            part = session_ != nullptr && session_->Parts() != nullptr ? session_->Parts()->Work() : nullptr;
        }

        const std::string currentText = ReadString(textValue_);
        const std::string resolved = ExpandTextTemplate(rule, currentText, body, part, config_);
        SetTextInputResolvedValue(resolved);
    }

    bool IncrementDisplaySerialByRule(const std::string& displayText, std::string* nextText)
    {
        if (nextText == nullptr || !TextTemplateHasSerial(lastTextTemplate_))
        {
            return false;
        }

        Part* part = lastWorkPart_;
        NXObject* partObject = dynamic_cast<NXObject*>(part);
        const std::string bodyName = lastBody_ == nullptr ? std::string() : ToString(lastBody_->Name());
        const std::string fileName = PartLeafName(part);
        const std::vector<TemplateToken> tokens = BuildTemplateTokens(lastTextTemplate_, lastBody_, partObject, bodyName, fileName, config_);

        std::string textPart;
        std::string serialPart;
        ResolveEditableValuesFromDisplay(tokens, displayText, config_, &textPart, &serialPart);

        std::string nextSerial;
        if (!IncrementTextSerial(serialPart, &nextSerial))
        {
            return false;
        }

        *nextText = ComposeTemplateDisplayText(tokens, textPart, nextSerial);
        return true;
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
        const auto ensurePositive = [](DoubleBlock* block, const double fallback) {
            if (block != nullptr && ReadDouble(block, fallback) <= 0.0)
            {
                block->SetValue(fallback);
            }
        };

        if (textHeight_ != nullptr && depth_ != nullptr)
        {
            const double height = ReadDouble(textHeight_, 10.0);
            const double depth = ReadDouble(depth_, 0.3);
            if (height > 0.0 && height < 1.0 && depth > 1.0)
            {
                textHeight_->SetValue(depth);
                depth_->SetValue(height);
            }
        }
        ensurePositive(textHeight_, 10.0);
        ensurePositive(depth_, 0.3);
        ensurePositive(boundaryDepth_, 0.3);
        ensurePositive(margin_, 100.0);
        ensurePositive(wScale_, 100.0);

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
        if (settings.height > 0.0 && settings.height < 1.0 && settings.depth > 1.0)
        {
            std::swap(settings.height, settings.depth);
        }
        settings.boundaryDepth = ReadDouble(boundaryDepth_, 0.3);
        settings.textLength = ReadDouble(margin_, 0.0);
        settings.widthScale = ReadDouble(wScale_, 100.0);
        if (settings.height <= 0.0)
        {
            settings.height = 10.0;
        }
        if (settings.depth <= 0.0)
        {
            settings.depth = 0.3;
        }
        if (settings.boundaryDepth <= 0.0)
        {
            settings.boundaryDepth = 0.3;
        }
        if (settings.textLength <= 0.0)
        {
            settings.textLength = 100.0;
        }
        if (settings.widthScale <= 0.0)
        {
            settings.widthScale = 100.0;
        }
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
        const std::string dialogRule = Trim(ruleText_);
        if (!dialogRule.empty())
        {
            return ruleText_;
        }

        const std::string configuredRule = Trim(config_.textTemplate);
        if (!configuredRule.empty())
        {
            return config_.textTemplate;
        }

        return settings.text.empty() ? std::string("{文本}") : settings.text;
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

    void ChooseAndAppendAttributeToken()
    {
        Face* selectedFace = SelectedFace();
        if (selectedFace == nullptr)
        {
            ShowError("刻字", "请先选择一个面，再选择属性变量。");
            return;
        }

        const int sourceIndex = AskMenuIndex("选择属性来源", std::vector<std::string>{"体属性", "部件属性"});
        if (sourceIndex < 0)
        {
            return;
        }

        Face* prototypeFace = PrototypeFace(selectedFace);
        NXObject* target = nullptr;
        std::string tokenPrefix;
        std::string sourceName;
        if (sourceIndex == 0)
        {
            target = ResolveBodyFromFace(prototypeFace);
            tokenPrefix = "{体属性:";
            sourceName = "体属性";
        }
        else
        {
            Part* part = nullptr;
            if (prototypeFace != nullptr)
            {
                part = dynamic_cast<Part*>(prototypeFace->OwningPart());
            }
            target = dynamic_cast<NXObject*>(part);
            tokenPrefix = "{部件属性:";
            sourceName = "部件属性";
        }

        std::vector<std::string> names = UserAttributeNames(target);
        if (names.empty())
        {
            ShowError("刻字", sourceName + "列表为空。");
            return;
        }

        const std::string attributeName = AskMenuItemPaged("选择" + sourceName, names);
        if (attributeName.empty())
        {
            return;
        }

        Log(session_, std::string("追加变量: ") + sourceName + ":" + attributeName);
        AppendTextToken(tokenPrefix + attributeName + "}");
        RefreshPreview();
    }

    Matrix3x3 TextMatrixFromDialog(Face* face,
                                   const Vector3d& faceNormal,
                                   const TextSettings& settings,
                                   Assemblies::Component* component) const
    {
        Vector3d xAxis(1.0, 0.0, 0.0);
        Vector3d yAxis(0.0, 1.0, 0.0);
        if (orientation_ != nullptr)
        {
            xAxis = orientation_->XAxis();
            yAxis = orientation_->YAxis();
        }
        xAxis = ComponentVectorToPrototype(component, xAxis);
        yAxis = ComponentVectorToPrototype(component, yAxis);

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

        Face* selectedFace = SelectedFace();
        if (selectedFace == nullptr)
        {
            throw std::runtime_error("Manual mode requires a selected engraving face.");
        }
        Log(session_, "已取得选择面");
        Assemblies::Component* selectedComponent = selectedFace->IsOccurrence() ? selectedFace->OwningComponent() : nullptr;
        Face* face = PrototypeFace(selectedFace);
        if (face == nullptr)
        {
            throw std::runtime_error("Could not resolve prototype face of the selected assembly face.");
        }

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
        if (selectedComponent != nullptr && hasPickedPoint)
        {
            pickedPoint = ComponentPointToPrototype(selectedComponent, pickedPoint);
        }
        if (!AskPlanarFaceData(face, hasPickedPoint ? &pickedPoint : nullptr, &facePoint, &faceNormal))
        {
            throw std::runtime_error("Selected face is not a valid planar engraving face.");
        }

        Point3d originReference = pickedPoint;
        if (orientation_ != nullptr && orientation_->IsOriginSpecified())
        {
            originReference = orientation_->Origin();
            if (selectedComponent != nullptr)
            {
                originReference = ComponentPointToPrototype(selectedComponent, originReference);
            }
        }
        const Point3d pickedOrigin = ProjectPointToFacePlane(facePoint, faceNormal, originReference);

        WorkContextGuard workContext(session_, selectedFace);
        Part* workPart = workContext.WorkPart();
        if (workPart == nullptr)
        {
            throw std::runtime_error("Could not set the selected component as work part.");
        }
        Log(session_, std::string("目标叶子部件: ") + PartLeafName(workPart));

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
            lastWorkPart_ = workPart;
            Log(session_, std::string("解析刻字文本: ") + textRule);
            const Point3d textOrigin = ApplyCenterOptions(face, faceNormal, pickedOrigin, settings);
            const Matrix3x3 textMatrix = TextMatrixFromDialog(face, faceNormal, settings, selectedComponent);

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
            workContext.Restore();
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
            SetTextInputResolvedValue(lastResolvedText_);
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

        ClearPreviewBuilder();
        Face* selectedFace = SelectedFace();
        WorkContextGuard workContext(session_, selectedFace);
        NXObject* textUdo = nullptr;
        builder.reset(CreatePreparedBuilder(&textUdo));

        builder->Commit();
        workContext.Restore();
        Log(session_, "刻字 Commit 完成");
        SaveCurrentRule();
        SaveDialogValues();
        if (lastBody_ != nullptr && !lastResolvedText_.empty())
        {
            WriteStringAttribute(lastBody_, "编号", lastResolvedText_);
            Log(session_, std::string("已写入体属性 编号=") + lastResolvedText_);
        }
        if (TextTemplateHasSerial(lastTextTemplate_) && textValue_ != nullptr)
        {
            std::string nextText;
            const std::string currentText = ReadString(textValue_);
            if (IncrementDisplaySerialByRule(currentText, &nextText))
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
    StringBlock* ruleValue_ = nullptr;
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
    Part* lastWorkPart_ = nullptr;
    std::string lastResolvedText_;
    std::string lastTextTemplate_;
    std::string ruleText_;
    bool refreshingPreview_ = false;
    bool updatingResolvedText_ = false;
    bool updatingRuleInput_ = false;
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
