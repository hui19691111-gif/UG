#include <NXOpen/BlockStyler_BlockDialog.hxx>
#include <NXOpen/BlockStyler_Button.hxx>
#include <NXOpen/BlockStyler_DoubleBlock.hxx>
#include <NXOpen/BlockStyler_Enumeration.hxx>
#include <NXOpen/BlockStyler_IntegerBlock.hxx>
#include <NXOpen/BlockStyler_ListBox.hxx>
#include <NXOpen/BlockStyler_PropertyList.hxx>
#include <NXOpen/BlockStyler_SelectObject.hxx>
#include <NXOpen/BlockStyler_StringBlock.hxx>
#include <NXOpen/BlockStyler_Toggle.hxx>
#include <NXOpen/BlockStyler_Node.hxx>
#include <NXOpen/BlockStyler_Tree.hxx>
#include <NXOpen/BlockStyler_UIBlock.hxx>
#include <NXOpen/Body.hxx>
#include <NXOpen/Callback.hxx>
#include <NXOpen/Edge.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/ListingWindow.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXMessageBox.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/NXString.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/Selection.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/TaggedObject.hxx>
#include <NXOpen/UI.hxx>

#include <uf.h>
#include <uf_defs.h>
#include <uf_disp.h>
#include <uf_exit.h>
#include <uf_ui.h>
#include <uf_curve.h>
#include <uf_layer.h>
#include <uf_modl.h>
#include <uf_modl_utilities.h>
#include <uf_obj.h>
#include <uf_obj_types.h>
#include <uf_object_types.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <fstream>
#include <cstdlib>
#include <cmath>
#include <cfloat>
#include <ctime>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#include <commdlg.h>
#ifdef CreateDialog
#undef CreateDialog
#endif

#ifndef DllExport
#define DllExport __declspec(dllexport)
#endif

namespace tube_same_body
{
std::vector<std::vector<tag_t> > RunSameBodySearch(
    NXOpen::Session* session,
    NXOpen::UI* ui,
    const std::vector<NXOpen::Body*>& selectedBodies);
}

namespace
{
enum TubeKind
{
    TubeKindSquare = 0,
    TubeKindRectangular = 1,
    TubeKindRound = 2,
    TubeKindUnknown = 3
};

struct TubeRecord
{
    NXOpen::Body* body;
    TubeKind kind;
    double length;
    double width;
    double height;
    double thickness;
    std::string name;
    std::string spec;
};

struct StatRow
{
    TubeKind kind;
    std::string name;
    std::string spec;
    std::string lengthText;
    int quantity;
    double totalLength;
    std::vector<tag_t> bodyTags;
};

struct CutPiece
{
    double length;
    int sourceQuantity;
};

struct CutStock
{
    double usedLength;
    std::vector<double> pieces;
};

enum ResultColumnId
{
    ResultColumnIndex = 0,
    ResultColumnType = 1,
    ResultColumnName = 2,
    ResultColumnMaterial = 3,
    ResultColumnSpec = 4,
    ResultColumnLength = 5,
    ResultColumnQuantity = 6
};

std::string KindName(TubeKind kind)
{
    switch (kind)
    {
    case TubeKindSquare: return "方管";
    case TubeKindRectangular: return "扁管";
    case TubeKindRound: return "圆管";
    default: return "未识别";
    }
}

std::wstring Utf8ToWide(const std::string& text)
{
    if (text.empty())
    {
        return L"";
    }
    const int needed = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, NULL, 0);
    if (needed <= 0)
    {
        return L"";
    }
    std::wstring result(static_cast<std::size_t>(needed - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, &result[0], needed);
    return result;
}

std::string WideToUtf8(const std::wstring& text)
{
    if (text.empty())
    {
        return "";
    }
    const int needed = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, NULL, 0, NULL, NULL);
    if (needed <= 0)
    {
        return "";
    }
    std::string result(static_cast<std::size_t>(needed - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, &result[0], needed, NULL, NULL);
    return result;
}

std::string NxStringToUtf8(const NXOpen::NXString& text)
{
    const char* utf8 = text.GetUTF8Text();
    return utf8 == NULL ? std::string() : std::string(utf8);
}

std::wstring QuoteArgument(const std::wstring& text)
{
    std::wstring result = L"\"";
    for (std::size_t index = 0; index < text.size(); ++index)
    {
        if (text[index] == L'"')
        {
            result += L"\\\"";
        }
        else
        {
            result += text[index];
        }
    }
    result += L"\"";
    return result;
}

double RoundTo(double value, double step)
{
    if (step <= 0.0)
    {
        return value;
    }
    return std::floor(value / step + 0.5) * step;
}

std::string FormatDouble(double value, int precision)
{
    std::ostringstream stream;
    stream.setf(std::ios::fixed);
    stream.precision(precision);
    stream << value;
    return stream.str();
}

std::string FormatSpecValue(double value)
{
    const double rounded = RoundTo(value, 1.0);
    const double integerValue = std::floor(rounded + 0.5);
    if (std::fabs(rounded - integerValue) < 0.01)
    {
        std::ostringstream stream;
        stream << static_cast<int>(integerValue);
        return stream.str();
    }
    return FormatDouble(rounded, 1);
}

std::string FormatSectionSpecValue(double value)
{
    const double rounded = RoundTo(value, 5.0);
    std::ostringstream stream;
    stream << static_cast<int>(std::floor(rounded + 0.5));
    return stream.str();
}

std::string BuildSectionSpec(double width, double height)
{
    return FormatSectionSpecValue(width) + "x" + FormatSectionSpecValue(height);
}

std::string BuildTubeSpec(double width, double height, double thickness)
{
    std::ostringstream stream;
    stream << BuildSectionSpec(width, height);
    if (thickness > 0.0)
    {
        stream << "x" << FormatDouble(thickness, 1);
    }
    else
    {
        stream << "x-";
    }
    return stream.str();
}

std::string BuildRoundTubeSpec(double diameter, double thickness)
{
    std::ostringstream stream;
    stream << "D" << FormatSpecValue(diameter);
    if (thickness > 0.0)
    {
        stream << "x" << FormatDouble(thickness, 1);
    }
    else
    {
        stream << "x-";
    }
    return stream.str();
}

std::string Trim(const std::string& text)
{
    const std::string whitespace = " \t\r\n";
    const std::string::size_type begin = text.find_first_not_of(whitespace);
    if (begin == std::string::npos)
    {
        return "";
    }
    const std::string::size_type end = text.find_last_not_of(whitespace);
    return text.substr(begin, end - begin + 1);
}

std::string RemoveUtf8Bom(const std::string& text)
{
    if (text.size() >= 3 &&
        static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB &&
        static_cast<unsigned char>(text[2]) == 0xBF)
    {
        return text.substr(3);
    }
    return text;
}

void AddUniqueText(std::vector<std::string>& values, const std::string& text)
{
    if (text.empty())
    {
        return;
    }
    if (std::find(values.begin(), values.end(), text) == values.end())
    {
        values.push_back(text);
    }
}

std::pair<int, int> NormalizeSpecPair(int first, int second)
{
    return std::make_pair(std::max(first, second), std::min(first, second));
}

void AddTubeSpec(std::set<std::pair<int, int> >& specs, int first, int second)
{
    if (first > 0 && second > 0)
    {
        specs.insert(NormalizeSpecPair(first, second));
    }
}

void AddFallbackTubeSpecs(std::set<std::pair<int, int> >& specs)
{
    const int squareSpecs[] = {10, 15, 20, 25, 30, 40, 50, 60, 70, 75, 80, 90, 100, 120, 125, 140, 150, 160, 180, 200, 250, 300};
    for (int index = 0; index < static_cast<int>(sizeof(squareSpecs) / sizeof(squareSpecs[0])); ++index)
    {
        AddTubeSpec(specs, squareSpecs[index], squareSpecs[index]);
    }

    const int rectangularSpecs[][2] =
    {
        {20, 10}, {25, 15}, {30, 10}, {30, 15}, {30, 20}, {40, 20}, {40, 25}, {40, 30},
        {50, 25}, {50, 30}, {50, 40}, {60, 30}, {60, 40}, {60, 50}, {70, 40}, {75, 45},
        {80, 40}, {80, 50}, {80, 60}, {90, 50}, {100, 50}, {100, 60}, {100, 80},
        {120, 60}, {120, 80}, {120, 100}, {125, 75}, {140, 80}, {150, 75}, {150, 100},
        {160, 80}, {160, 100}, {180, 80}, {180, 100}, {200, 100}, {200, 120}, {200, 150},
        {250, 100}, {250, 150}, {250, 200}, {300, 150}, {300, 200}, {400, 200}
    };
    for (int index = 0; index < static_cast<int>(sizeof(rectangularSpecs) / sizeof(rectangularSpecs[0])); ++index)
    {
        AddTubeSpec(specs, rectangularSpecs[index][0], rectangularSpecs[index][1]);
    }
}

std::string GetModuleDirectory()
{
    HMODULE selfModule = NULL;
    if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&GetModuleDirectory),
            &selfModule))
    {
        return "";
    }

    wchar_t widePath[MAX_PATH] = {0};
    const DWORD length = GetModuleFileNameW(selfModule, widePath, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
    {
        return "";
    }

    char path[MAX_PATH] = {0};
    WideCharToMultiByte(CP_ACP, 0, widePath, -1, path, MAX_PATH, NULL, NULL);
    std::string directory(path);
    const std::string::size_type slash = directory.find_last_of("\\/");
    return slash == std::string::npos ? "" : directory.substr(0, slash);
}

void LoadTubeSpecsFromFile(const std::string& path, std::set<std::pair<int, int> >& specs)
{
    std::ifstream input(path.c_str());
    if (!input)
    {
        return;
    }

    std::string line;
    while (std::getline(input, line))
    {
        const std::string::size_type comment = line.find('#');
        if (comment != std::string::npos)
        {
            line = line.substr(0, comment);
        }
        line = Trim(line);
        if (line.empty())
        {
            continue;
        }
        std::replace(line.begin(), line.end(), 'X', 'x');
        std::replace(line.begin(), line.end(), '*', 'x');
        std::replace(line.begin(), line.end(), ',', 'x');
        const std::string::size_type separator = line.find('x');
        if (separator == std::string::npos)
        {
            continue;
        }
        AddTubeSpec(specs,
                    std::atoi(Trim(line.substr(0, separator)).c_str()),
                    std::atoi(Trim(line.substr(separator + 1)).c_str()));
    }
}

std::string SharedSpecTablePath()
{
    const std::string moduleDirectory = GetModuleDirectory();
    if (moduleDirectory.empty())
    {
        return "";
    }
    return moduleDirectory + "\\config\\FangTongKaKou_specs.txt";
}

std::string MaterialConfigPath()
{
    const std::string moduleDirectory = GetModuleDirectory();
    if (moduleDirectory.empty())
    {
        return "";
    }
    return moduleDirectory + "\\Write_Prat_Attr.ini";
}

std::vector<std::string> LoadMaterialOptions()
{
    std::vector<std::string> materials;
    const std::string path = MaterialConfigPath();
    if (!path.empty())
    {
        std::ifstream input(path.c_str(), std::ios::binary);
        if (input)
        {
            bool inMaterialSection = false;
            bool firstLine = true;
            std::string line;
            while (std::getline(input, line))
            {
                if (firstLine)
                {
                    line = RemoveUtf8Bom(line);
                    firstLine = false;
                }

                const std::string value = Trim(line);
                if (value.empty() || value[0] == '#' || value[0] == ';')
                {
                    continue;
                }
                if (value[0] == '[' && value[value.size() - 1] == ']')
                {
                    const std::string sectionName = Trim(value.substr(1, value.size() - 2));
                    inMaterialSection = sectionName == "材料";
                    continue;
                }
                if (inMaterialSection)
                {
                    AddUniqueText(materials, value);
                }
            }
        }
    }
    if (materials.empty())
    {
        materials.push_back("-");
    }
    return materials;
}

const std::set<std::pair<int, int> >& GetTubeSpecs()
{
    static std::set<std::pair<int, int> > specs;
    static bool loaded = false;
    if (loaded)
    {
        return specs;
    }

    loaded = true;
    const std::string moduleDirectory = GetModuleDirectory();
    if (!moduleDirectory.empty())
    {
        LoadTubeSpecsFromFile(moduleDirectory + "\\config\\FangTongKaKou_specs.txt", specs);
        LoadTubeSpecsFromFile(moduleDirectory + "\\FangTongKaKou_specs.txt", specs);
    }
    if (specs.empty())
    {
        AddFallbackTubeSpecs(specs);
    }
    return specs;
}

bool ReclassifyTubeDimensionsBySpec(double& length, double& width, double& height)
{
    double dimensions[3] = {length, width, height};
    const std::set<std::pair<int, int> >& specs = GetTubeSpecs();

    struct Match
    {
        int firstIndex;
        int secondIndex;
        int lengthIndex;
        int firstSpec;
        int secondSpec;
        double score;
    };

    std::vector<Match> matches;
    const int pairs[3][2] = {{0, 1}, {0, 2}, {1, 2}};
    for (int pairIndex = 0; pairIndex < 3; ++pairIndex)
    {
        const int firstIndex = pairs[pairIndex][0];
        const int secondIndex = pairs[pairIndex][1];
        const int firstSpec = static_cast<int>(std::floor(RoundTo(dimensions[firstIndex], 5.0) + 0.5));
        const int secondSpec = static_cast<int>(std::floor(RoundTo(dimensions[secondIndex], 5.0) + 0.5));
        if (std::fabs(dimensions[firstIndex] - firstSpec) > 0.1 ||
            std::fabs(dimensions[secondIndex] - secondSpec) > 0.1 ||
            specs.find(NormalizeSpecPair(firstSpec, secondSpec)) == specs.end())
        {
            continue;
        }

        int lengthIndex = 0;
        for (int index = 0; index < 3; ++index)
        {
            if (index != firstIndex && index != secondIndex)
            {
                lengthIndex = index;
                break;
            }
        }
        if (dimensions[lengthIndex] < std::max(5.0, std::min(firstSpec, secondSpec) * 0.5))
        {
            continue;
        }

        Match match = {};
        match.firstIndex = firstIndex;
        match.secondIndex = secondIndex;
        match.lengthIndex = lengthIndex;
        match.firstSpec = firstSpec;
        match.secondSpec = secondSpec;
        match.score = dimensions[lengthIndex];
        matches.push_back(match);
    }

    if (matches.empty())
    {
        return false;
    }

    const Match* best = &matches.front();
    for (std::size_t index = 1; index < matches.size(); ++index)
    {
        if (matches[index].score > best->score)
        {
            best = &matches[index];
        }
    }

    length = dimensions[best->lengthIndex];
    width = static_cast<double>(std::max(best->firstSpec, best->secondSpec));
    height = static_cast<double>(std::min(best->firstSpec, best->secondSpec));
    return true;
}

int GetLayerValue(NXOpen::BlockStyler::IntegerBlock* block, int fallback)
{
    if (block == NULL)
    {
        return fallback;
    }

    const int value = block->Value();
    if (value < 1)
    {
        return 1;
    }
    if (value > 256)
    {
        return 256;
    }
    return value;
}

double GetStandardLength(NXOpen::BlockStyler::StringBlock* block)
{
    if (block == NULL)
    {
        return 0.0;
    }

    const std::string text = block->Value().GetText();
    char* end = NULL;
    const double value = std::strtod(text.c_str(), &end);
    if (end == text.c_str())
    {
        return 0.0;
    }
    return std::max(0.0, value) * 1000.0;
}

std::string FormatStandardLength(double standardLength)
{
    if (standardLength <= 0.0)
    {
        return "-";
    }
    return FormatDouble(standardLength / 1000.0, 1) + "米";
}

std::string ObjectName(tag_t objectTag)
{
    char name[UF_OBJ_NAME_BUFSIZE] = {0};
    if (objectTag != NULL_TAG && UF_OBJ_ask_name(objectTag, name) == 0 && name[0] != '\0')
    {
        const int wideCount = MultiByteToWideChar(CP_ACP, 0, name, -1, NULL, 0);
        if (wideCount > 0)
        {
            std::wstring wideName(static_cast<std::size_t>(wideCount - 1), L'\0');
            MultiByteToWideChar(CP_ACP, 0, name, -1, &wideName[0], wideCount);
            return WideToUtf8(wideName);
        }
        return name;
    }
    return "-";
}

int GetSetCount(NXOpen::BlockStyler::IntegerBlock* block)
{
    if (block == NULL)
    {
        return 1;
    }
    return std::max(1, block->Value());
}

void SetDefaultText(NXOpen::BlockStyler::StringBlock* block, const char* value)
{
    if (block != NULL && block->Value().GetText()[0] == '\0')
    {
        block->SetValue(value);
    }
}

std::vector<tag_t> UfListToTags(uf_list_p_t list)
{
    std::vector<tag_t> tags;
    int count = 0;
    if (list == NULL || UF_MODL_ask_list_count(list, &count) != 0)
    {
        return tags;
    }

    for (int index = 0; index < count; ++index)
    {
        tag_t tag = NULL_TAG;
        if (UF_MODL_ask_list_item(list, index, &tag) == 0 && tag != NULL_TAG)
        {
            tags.push_back(tag);
        }
    }
    return tags;
}

bool HasCylindricalFace(tag_t bodyTag)
{
    uf_list_p_t faceList = NULL;
    if (UF_MODL_ask_body_faces(bodyTag, &faceList) != 0 || faceList == NULL)
    {
        return false;
    }

    const std::vector<tag_t> faces = UfListToTags(faceList);
    UF_MODL_delete_list(&faceList);
    for (std::size_t index = 0; index < faces.size(); ++index)
    {
        int type = 0;
        double point[3] = {0.0, 0.0, 0.0};
        double direction[3] = {0.0, 0.0, 0.0};
        double box[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
        double radius = 0.0;
        double radData = 0.0;
        int normDir = 0;
        if (UF_MODL_ask_face_data(faces[index], &type, point, direction, box, &radius, &radData, &normDir) == 0 &&
            type == 16 && radius > 0.0)
        {
            return true;
        }
    }
    return false;
}

bool AskBodyDimensions(tag_t bodyTag, double dimensions[3])
{
    double box[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    if (UF_MODL_ask_bounding_box(bodyTag, box) != 0)
    {
        return false;
    }

    dimensions[0] = std::fabs(box[3] - box[0]);
    dimensions[1] = std::fabs(box[4] - box[1]);
    dimensions[2] = std::fabs(box[5] - box[2]);
    std::sort(dimensions, dimensions + 3);
    return dimensions[2] > 0.001;
}

double Distance3(const double lhs[3], const double rhs[3])
{
    const double dx = rhs[0] - lhs[0];
    const double dy = rhs[1] - lhs[1];
    const double dz = rhs[2] - lhs[2];
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

double Dot3(const double lhs[3], const double rhs[3])
{
    return lhs[0] * rhs[0] + lhs[1] * rhs[1] + lhs[2] * rhs[2];
}

bool Normalize3(double vector[3])
{
    const double length = std::sqrt(Dot3(vector, vector));
    if (length < 1.0e-9)
    {
        return false;
    }

    vector[0] /= length;
    vector[1] /= length;
    vector[2] /= length;
    return true;
}

void CanonicalizeDirection(double direction[3])
{
    for (int index = 0; index < 3; ++index)
    {
        if (std::fabs(direction[index]) < 1.0e-6)
        {
            continue;
        }

        if (direction[index] < 0.0)
        {
            direction[0] = -direction[0];
            direction[1] = -direction[1];
            direction[2] = -direction[2];
        }
        return;
    }
}

struct EdgeEndpointPair
{
    double first[3];
    double second[3];
};

struct DirectionCluster
{
    double direction[3];
    double totalLength;
    int count;
};

void FaceBoxProjectionRange(const double box[6], const double axis[3], double& minProjection, double& maxProjection);

bool EstimateMainLengthAxisFromEdges(tag_t bodyTag, double axis[3])
{
    axis[0] = 0.0;
    axis[1] = 0.0;
    axis[2] = 0.0;

    uf_list_p_t edgeList = NULL;
    if (UF_MODL_ask_body_edges(bodyTag, &edgeList) != 0 || edgeList == NULL)
    {
        return false;
    }

    const std::vector<tag_t> edgeTags = UfListToTags(edgeList);
    UF_MODL_delete_list(&edgeList);

    std::vector<DirectionCluster> clusters;
    const double parallelToleranceCos = std::cos(5.0 * 3.14159265358979323846 / 180.0);
    for (std::size_t index = 0; index < edgeTags.size(); ++index)
    {
        double point1[3] = {0.0, 0.0, 0.0};
        double point2[3] = {0.0, 0.0, 0.0};
        int vertexCount = 0;
        if (UF_MODL_ask_edge_verts(edgeTags[index], point1, point2, &vertexCount) != 0 || vertexCount != 2)
        {
            continue;
        }

        const double edgeLength = Distance3(point1, point2);
        if (edgeLength < 0.5)
        {
            continue;
        }

        double direction[3] =
        {
            point2[0] - point1[0],
            point2[1] - point1[1],
            point2[2] - point1[2]
        };
        if (!Normalize3(direction))
        {
            continue;
        }
        CanonicalizeDirection(direction);

        bool addedToCluster = false;
        for (std::size_t clusterIndex = 0; clusterIndex < clusters.size(); ++clusterIndex)
        {
            if (std::fabs(Dot3(direction, clusters[clusterIndex].direction)) >= parallelToleranceCos)
            {
                clusters[clusterIndex].totalLength += edgeLength;
                ++clusters[clusterIndex].count;
                addedToCluster = true;
                break;
            }
        }

        if (!addedToCluster)
        {
            DirectionCluster cluster = {};
            cluster.direction[0] = direction[0];
            cluster.direction[1] = direction[1];
            cluster.direction[2] = direction[2];
            cluster.totalLength = edgeLength;
            cluster.count = 1;
            clusters.push_back(cluster);
        }
    }

    if (clusters.empty())
    {
        return false;
    }

    std::size_t bestIndex = 0;
    for (std::size_t index = 1; index < clusters.size(); ++index)
    {
        if (clusters[index].totalLength > clusters[bestIndex].totalLength)
        {
            bestIndex = index;
        }
    }

    axis[0] = clusters[bestIndex].direction[0];
    axis[1] = clusters[bestIndex].direction[1];
    axis[2] = clusters[bestIndex].direction[2];
    return Normalize3(axis);
}

bool AskBodyProjectionRangeFromEdges(tag_t bodyTag, const double axis[3], double& minProjection, double& maxProjection)
{
    minProjection = DBL_MAX;
    maxProjection = -DBL_MAX;

    uf_list_p_t edgeList = NULL;
    if (UF_MODL_ask_body_edges(bodyTag, &edgeList) != 0 || edgeList == NULL)
    {
        return false;
    }

    const std::vector<tag_t> edgeTags = UfListToTags(edgeList);
    UF_MODL_delete_list(&edgeList);
    for (std::size_t index = 0; index < edgeTags.size(); ++index)
    {
        double point1[3] = {0.0, 0.0, 0.0};
        double point2[3] = {0.0, 0.0, 0.0};
        int vertexCount = 0;
        if (UF_MODL_ask_edge_verts(edgeTags[index], point1, point2, &vertexCount) != 0 || vertexCount != 2)
        {
            continue;
        }

        const double firstProjection = Dot3(point1, axis);
        const double secondProjection = Dot3(point2, axis);
        minProjection = std::min(minProjection, std::min(firstProjection, secondProjection));
        maxProjection = std::max(maxProjection, std::max(firstProjection, secondProjection));
    }

    return maxProjection > minProjection;
}

bool AskBodyProjectionRangeFromBox(tag_t bodyTag, const double axis[3], double& minProjection, double& maxProjection)
{
    double box[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    if (UF_MODL_ask_bounding_box(bodyTag, box) != 0)
    {
        return false;
    }

    FaceBoxProjectionRange(box, axis, minProjection, maxProjection);
    return maxProjection > minProjection;
}

int SixSegmentHitIndex(double minProjection, double maxProjection, double position)
{
    const double span = maxProjection - minProjection;
    if (span <= 1.0e-6 || position < minProjection - 1.0 || position > maxProjection + 1.0)
    {
        return -1;
    }

    int segment = static_cast<int>(std::floor((position - minProjection) * 6.0 / span));
    if (segment < 0)
    {
        segment = 0;
    }
    if (segment > 5)
    {
        segment = 5;
    }
    return segment;
}

void FaceBoxProjectionRange(const double box[6], const double axis[3], double& minProjection, double& maxProjection)
{
    minProjection = DBL_MAX;
    maxProjection = -DBL_MAX;
    for (int x = 0; x < 2; ++x)
    {
        for (int y = 0; y < 2; ++y)
        {
            for (int z = 0; z < 2; ++z)
            {
                const double corner[3] =
                {
                    box[x ? 3 : 0],
                    box[y ? 4 : 1],
                    box[z ? 5 : 2]
                };
                const double projection = Dot3(corner, axis);
                minProjection = std::min(minProjection, projection);
                maxProjection = std::max(maxProjection, projection);
            }
        }
    }
}

bool FaceHasInnerLoop(tag_t faceTag)
{
    uf_loop_p_t loopList = NULL;
    if (UF_MODL_ask_face_loops(faceTag, &loopList) != 0 || loopList == NULL)
    {
        return false;
    }

    int loopCount = 0;
    int peripheralCount = 0;
    int holeCount = 0;
    if (UF_MODL_ask_loop_list_count(loopList, &loopCount) == 0)
    {
        for (int index = 0; index < loopCount; ++index)
        {
            int type = 0;
            uf_list_p_t edgeList = NULL;
            if (UF_MODL_ask_loop_list_item(loopList, index, &type, &edgeList) != 0)
            {
                continue;
            }

            if (type == 1)
            {
                ++peripheralCount;
            }
            else if (type == 2)
            {
                ++holeCount;
            }
        }
    }
    UF_MODL_delete_loop_list(&loopList);
    return peripheralCount >= 1 && holeCount >= 1;
}

bool HasClosedRectangularTubeSections(tag_t bodyTag)
{
    double lengthAxis[3] = {0.0, 0.0, 0.0};
    if (!EstimateMainLengthAxisFromEdges(bodyTag, lengthAxis))
    {
        return false;
    }

    double bodyMinProjection = 0.0;
    double bodyMaxProjection = 0.0;
    if (!AskBodyProjectionRangeFromEdges(bodyTag, lengthAxis, bodyMinProjection, bodyMaxProjection))
    {
        return false;
    }

    uf_list_p_t faceList = NULL;
    if (UF_MODL_ask_body_faces(bodyTag, &faceList) != 0 || faceList == NULL)
    {
        return false;
    }

    const std::vector<tag_t> faces = UfListToTags(faceList);
    UF_MODL_delete_list(&faceList);

    std::vector<double> sectionPositions;
    bool sectionSegments[6] = {false, false, false, false, false, false};
    const double normalParallelTolerance = std::cos(20.0 * 3.14159265358979323846 / 180.0);
    for (std::size_t index = 0; index < faces.size(); ++index)
    {
        int faceType = 0;
        if (UF_MODL_ask_face_type(faces[index], &faceType) != 0 || faceType != UF_MODL_PLANAR_FACE)
        {
            continue;
        }

        int type = 0;
        double point[3] = {0.0, 0.0, 0.0};
        double direction[3] = {0.0, 0.0, 0.0};
        double box[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
        double radius = 0.0;
        double radData = 0.0;
        int normDir = 0;
        if (UF_MODL_ask_face_data(faces[index], &type, point, direction, box, &radius, &radData, &normDir) != 0)
        {
            continue;
        }
        if (!Normalize3(direction) || std::fabs(Dot3(direction, lengthAxis)) < normalParallelTolerance)
        {
            continue;
        }

        if (!FaceHasInnerLoop(faces[index]))
        {
            continue;
        }

        const double position = Dot3(point, lengthAxis);
        bool alreadyCounted = false;
        for (std::size_t posIndex = 0; posIndex < sectionPositions.size(); ++posIndex)
        {
            if (std::fabs(sectionPositions[posIndex] - position) < 1.0)
            {
                alreadyCounted = true;
                break;
            }
        }

        if (!alreadyCounted)
        {
            sectionPositions.push_back(position);
            const int segment = SixSegmentHitIndex(bodyMinProjection, bodyMaxProjection, position);
            if (segment >= 0)
            {
                sectionSegments[segment] = true;
            }
        }
    }

    for (int segment = 0; segment < 6; ++segment)
    {
        if (sectionSegments[segment])
        {
            return true;
        }
    }
    return false;
}

bool CylindricalFaceHitsSixSegments(
    const double faceBox[6],
    const double axis[3],
    double bodyMinProjection,
    double bodyMaxProjection,
    bool segmentHits[6])
{
    double faceMinProjection = 0.0;
    double faceMaxProjection = 0.0;
    FaceBoxProjectionRange(faceBox, axis, faceMinProjection, faceMaxProjection);
    const double bodySpan = bodyMaxProjection - bodyMinProjection;
    if (bodySpan <= 1.0e-6)
    {
        return false;
    }

    bool hit = false;
    for (int segment = 0; segment < 6; ++segment)
    {
        const double segmentCenter = bodyMinProjection + bodySpan * (static_cast<double>(segment) + 0.5) / 6.0;
        if (segmentCenter >= faceMinProjection - 1.0 && segmentCenter <= faceMaxProjection + 1.0)
        {
            segmentHits[segment] = true;
            hit = true;
        }
    }
    return hit;
}

struct CylindricalFaceInfo
{
    tag_t faceTag;
    double point[3];
    double axis[3];
    double box[6];
    double radius;
    double minProjection;
    double maxProjection;
};

bool AskCylindricalFaceInfo(tag_t faceTag, CylindricalFaceInfo& info)
{
    int type = 0;
    double radData = 0.0;
    int normDir = 0;
    if (UF_MODL_ask_face_data(faceTag, &type, info.point, info.axis, info.box, &info.radius, &radData, &normDir) != 0 ||
        type != 16 || info.radius <= 0.0)
    {
        return false;
    }

    if (!Normalize3(info.axis))
    {
        return false;
    }

    info.faceTag = faceTag;
    FaceBoxProjectionRange(info.box, info.axis, info.minProjection, info.maxProjection);
    return info.maxProjection > info.minProjection;
}

std::vector<CylindricalFaceInfo> CollectCylindricalFaces(tag_t bodyTag)
{
    std::vector<CylindricalFaceInfo> cylinders;
    uf_list_p_t faceList = NULL;
    if (UF_MODL_ask_body_faces(bodyTag, &faceList) != 0 || faceList == NULL)
    {
        return cylinders;
    }

    const std::vector<tag_t> faces = UfListToTags(faceList);
    UF_MODL_delete_list(&faceList);
    for (std::size_t index = 0; index < faces.size(); ++index)
    {
        CylindricalFaceInfo info = {};
        if (AskCylindricalFaceInfo(faces[index], info))
        {
            cylinders.push_back(info);
        }
    }
    return cylinders;
}

bool HasFullCircularEdge(tag_t bodyTag)
{
    uf_list_p_t edgeList = NULL;
    if (UF_MODL_ask_body_edges(bodyTag, &edgeList) != 0 || edgeList == NULL)
    {
        return false;
    }

    const std::vector<tag_t> edgeTags = UfListToTags(edgeList);
    UF_MODL_delete_list(&edgeList);
    for (std::size_t index = 0; index < edgeTags.size(); ++index)
    {
        int edgeType = 0;
        if (UF_MODL_ask_edge_type(edgeTags[index], &edgeType) != 0 || edgeType != UF_MODL_CIRCULAR_EDGE)
        {
            continue;
        }

        double point1[3] = {0.0, 0.0, 0.0};
        double point2[3] = {0.0, 0.0, 0.0};
        int vertexCount = 0;
        if (UF_MODL_ask_edge_verts(edgeTags[index], point1, point2, &vertexCount) == 0 && vertexCount == 0)
        {
            return true;
        }

        UF_CURVE_arc_t arcData = {};
        if (UF_CURVE_ask_arc_data(edgeTags[index], &arcData) == 0 &&
            std::fabs(std::fabs(arcData.end_angle - arcData.start_angle) - 2.0 * 3.14159265358979323846) < 0.05 &&
            arcData.radius > 0.0)
        {
            return true;
        }
    }
    return false;
}

bool EstimateTubeDimensionsFromEdges(tag_t bodyTag, double& length, double& width, double& height)
{
    length = 0.0;
    width = 0.0;
    height = 0.0;

    uf_list_p_t edgeList = NULL;
    if (UF_MODL_ask_body_edges(bodyTag, &edgeList) != 0 || edgeList == NULL)
    {
        return false;
    }

    const std::vector<tag_t> edgeTags = UfListToTags(edgeList);
    UF_MODL_delete_list(&edgeList);

    std::vector<DirectionCluster> clusters;
    std::vector<EdgeEndpointPair> endpointPairs;
    const double parallelToleranceCos = std::cos(5.0 * 3.14159265358979323846 / 180.0);
    for (std::size_t index = 0; index < edgeTags.size(); ++index)
    {
        double point1[3] = {0.0, 0.0, 0.0};
        double point2[3] = {0.0, 0.0, 0.0};
        int vertexCount = 0;
        if (UF_MODL_ask_edge_verts(edgeTags[index], point1, point2, &vertexCount) != 0 || vertexCount != 2)
        {
            continue;
        }

        const double edgeLength = Distance3(point1, point2);
        if (edgeLength < 0.5)
        {
            continue;
        }

        EdgeEndpointPair endpoints = {};
        for (int axis = 0; axis < 3; ++axis)
        {
            endpoints.first[axis] = point1[axis];
            endpoints.second[axis] = point2[axis];
        }
        endpointPairs.push_back(endpoints);

        double direction[3] =
        {
            point2[0] - point1[0],
            point2[1] - point1[1],
            point2[2] - point1[2]
        };
        if (!Normalize3(direction))
        {
            continue;
        }
        CanonicalizeDirection(direction);

        bool addedToCluster = false;
        for (std::size_t clusterIndex = 0; clusterIndex < clusters.size(); ++clusterIndex)
        {
            if (std::fabs(Dot3(direction, clusters[clusterIndex].direction)) >= parallelToleranceCos)
            {
                clusters[clusterIndex].totalLength += edgeLength;
                ++clusters[clusterIndex].count;
                addedToCluster = true;
                break;
            }
        }

        if (!addedToCluster)
        {
            DirectionCluster cluster = {};
            cluster.direction[0] = direction[0];
            cluster.direction[1] = direction[1];
            cluster.direction[2] = direction[2];
            cluster.totalLength = edgeLength;
            cluster.count = 1;
            clusters.push_back(cluster);
        }
    }

    if (clusters.size() < 3 || endpointPairs.empty())
    {
        return false;
    }

    double bestScore = -1.0;
    double bestDimensions[3] = {0.0, 0.0, 0.0};
    const double perpendicularTolerance = 0.20;
    for (std::size_t i = 0; i < clusters.size(); ++i)
    {
        for (std::size_t j = i + 1; j < clusters.size(); ++j)
        {
            if (std::fabs(Dot3(clusters[i].direction, clusters[j].direction)) > perpendicularTolerance)
            {
                continue;
            }

            for (std::size_t k = j + 1; k < clusters.size(); ++k)
            {
                if (std::fabs(Dot3(clusters[i].direction, clusters[k].direction)) > perpendicularTolerance ||
                    std::fabs(Dot3(clusters[j].direction, clusters[k].direction)) > perpendicularTolerance)
                {
                    continue;
                }

                const double* axes[3] = {clusters[i].direction, clusters[j].direction, clusters[k].direction};
                double minProjection[3] = {DBL_MAX, DBL_MAX, DBL_MAX};
                double maxProjection[3] = {-DBL_MAX, -DBL_MAX, -DBL_MAX};
                for (std::size_t endpointIndex = 0; endpointIndex < endpointPairs.size(); ++endpointIndex)
                {
                    const double* points[2] = {endpointPairs[endpointIndex].first, endpointPairs[endpointIndex].second};
                    for (int pointIndex = 0; pointIndex < 2; ++pointIndex)
                    {
                        for (int axisIndex = 0; axisIndex < 3; ++axisIndex)
                        {
                            const double projection = Dot3(points[pointIndex], axes[axisIndex]);
                            minProjection[axisIndex] = std::min(minProjection[axisIndex], projection);
                            maxProjection[axisIndex] = std::max(maxProjection[axisIndex], projection);
                        }
                    }
                }

                double dimensions[3] =
                {
                    maxProjection[0] - minProjection[0],
                    maxProjection[1] - minProjection[1],
                    maxProjection[2] - minProjection[2]
                };
                std::sort(dimensions, dimensions + 3);
                if (dimensions[0] <= 0.5 || dimensions[1] <= 0.5 || dimensions[2] <= 0.5)
                {
                    continue;
                }

                const double clusterScore =
                    clusters[i].totalLength + clusters[j].totalLength + clusters[k].totalLength +
                    static_cast<double>(clusters[i].count + clusters[j].count + clusters[k].count) * 10.0;
                const double dimensionScore = dimensions[2] * 0.01 + dimensions[1] * 0.001;
                const double score = clusterScore + dimensionScore;
                if (score > bestScore)
                {
                    bestScore = score;
                    bestDimensions[0] = dimensions[0];
                    bestDimensions[1] = dimensions[1];
                    bestDimensions[2] = dimensions[2];
                }
            }
        }
    }

    if (bestScore < 0.0)
    {
        return false;
    }

    height = bestDimensions[0];
    width = bestDimensions[1];
    length = bestDimensions[2];
    return length > 0.0 && width > 0.0 && height > 0.0;
}

bool EstimateRoundTubeDimensions(tag_t bodyTag, double& length, double& diameter, double& thickness)
{
    length = 0.0;
    diameter = 0.0;
    thickness = 0.0;

    const std::vector<CylindricalFaceInfo> cylinders = CollectCylindricalFaces(bodyTag);
    if (cylinders.empty())
    {
        return false;
    }

    const bool hasFullCircularEdge = HasFullCircularEdge(bodyTag);
    std::vector<double> radii;
    double axis[3] = {0.0, 0.0, 0.0};
    double bodyMinProjection = 0.0;
    double bodyMaxProjection = 0.0;

    std::size_t bestCylinderIndex = 0;
    double bestScore = -1.0;
    for (std::size_t index = 0; index < cylinders.size(); ++index)
    {
        const double faceSpan = cylinders[index].maxProjection - cylinders[index].minProjection;
        const double score = faceSpan * std::max(1.0, cylinders[index].radius);
        if (score > bestScore)
        {
            bestScore = score;
            bestCylinderIndex = index;
        }
    }

    axis[0] = cylinders[bestCylinderIndex].axis[0];
    axis[1] = cylinders[bestCylinderIndex].axis[1];
    axis[2] = cylinders[bestCylinderIndex].axis[2];
    if (!AskBodyProjectionRangeFromEdges(bodyTag, axis, bodyMinProjection, bodyMaxProjection))
    {
        AskBodyProjectionRangeFromBox(bodyTag, axis, bodyMinProjection, bodyMaxProjection);
    }
    const double bodySpan = bodyMaxProjection - bodyMinProjection;
    const double bestCylinderSpan = cylinders[bestCylinderIndex].maxProjection - cylinders[bestCylinderIndex].minProjection;
    if (!hasFullCircularEdge && (bodySpan <= 0.0 || bestCylinderSpan < bodySpan * 0.50))
    {
        return false;
    }

    const double parallelTolerance = std::cos(15.0 * 3.14159265358979323846 / 180.0);
    for (std::size_t index = 0; index < cylinders.size(); ++index)
    {
        if (std::fabs(Dot3(cylinders[index].axis, axis)) >= parallelTolerance)
        {
            radii.push_back(cylinders[index].radius);
        }
    }
    std::sort(radii.begin(), radii.end());
    const double outerRadius = radii.back();
    double innerRadius = 0.0;
    for (int index = static_cast<int>(radii.size()) - 2; index >= 0; --index)
    {
        if (outerRadius - radii[index] > 0.05)
        {
            innerRadius = radii[index];
            break;
        }
    }

    uf_list_p_t edgeList = NULL;
    if (UF_MODL_ask_body_edges(bodyTag, &edgeList) == 0 && edgeList != NULL)
    {
        const std::vector<tag_t> edgeTags = UfListToTags(edgeList);
        UF_MODL_delete_list(&edgeList);

        double minProjection = DBL_MAX;
        double maxProjection = -DBL_MAX;
        for (std::size_t index = 0; index < edgeTags.size(); ++index)
        {
            double point1[3] = {0.0, 0.0, 0.0};
            double point2[3] = {0.0, 0.0, 0.0};
            int vertexCount = 0;
            if (UF_MODL_ask_edge_verts(edgeTags[index], point1, point2, &vertexCount) != 0 || vertexCount != 2)
            {
                continue;
            }

            const double projection1 = Dot3(point1, axis);
            const double projection2 = Dot3(point2, axis);
            minProjection = std::min(minProjection, std::min(projection1, projection2));
            maxProjection = std::max(maxProjection, std::max(projection1, projection2));
        }

        if (maxProjection > minProjection)
        {
            length = maxProjection - minProjection;
        }
    }

    if (length <= 0.0)
    {
        double dimensions[3] = {0.0, 0.0, 0.0};
        if (!AskBodyDimensions(bodyTag, dimensions))
        {
            return false;
        }
        length = dimensions[2];
    }

    diameter = outerRadius * 2.0;
    thickness = innerRadius > 0.0 ? outerRadius - innerRadius : 0.0;
    return diameter > 0.0 && length > 0.0;
}

double EstimateWallThicknessFromEdges(tag_t bodyTag, double width, double height)
{
    uf_list_p_t edgeList = NULL;
    if (UF_MODL_ask_body_edges(bodyTag, &edgeList) != 0 || edgeList == NULL)
    {
        return 0.0;
    }

    const std::vector<tag_t> edgeTags = UfListToTags(edgeList);
    UF_MODL_delete_list(&edgeList);

    const double maxThickness = std::max(1.0, std::min(width, height) * 0.35);
    std::map<int, int> roundedCounts;
    for (std::size_t index = 0; index < edgeTags.size(); ++index)
    {
        double point1[3] = {0.0, 0.0, 0.0};
        double point2[3] = {0.0, 0.0, 0.0};
        int vertexCount = 0;
        if (UF_MODL_ask_edge_verts(edgeTags[index], point1, point2, &vertexCount) != 0 || vertexCount != 2)
        {
            continue;
        }

        const double dx = point2[0] - point1[0];
        const double dy = point2[1] - point1[1];
        const double dz = point2[2] - point1[2];
        const double edgeLength = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (edgeLength < 0.3 || edgeLength > maxThickness)
        {
            continue;
        }

        const int roundedTenths = static_cast<int>(std::floor(edgeLength * 10.0 + 0.5));
        ++roundedCounts[roundedTenths];
    }

    int bestTenths = 0;
    int bestCount = 0;
    for (std::map<int, int>::const_iterator it = roundedCounts.begin(); it != roundedCounts.end(); ++it)
    {
        if (it->second > bestCount || (it->second == bestCount && (bestTenths == 0 || it->first < bestTenths)))
        {
            bestTenths = it->first;
            bestCount = it->second;
        }
    }

    return bestTenths > 0 ? static_cast<double>(bestTenths) / 10.0 : 0.0;
}

TubeRecord ClassifyBody(NXOpen::Body* body)
{
    TubeRecord record = {};
    record.body = body;
    record.kind = TubeKindUnknown;
    record.length = 0.0;
    record.width = 0.0;
    record.height = 0.0;
    record.thickness = 0.0;

    double dimensions[3] = {0.0, 0.0, 0.0};
    if (body == NULL || !AskBodyDimensions(body->Tag(), dimensions))
    {
        record.spec = "无法读取尺寸";
        return record;
    }

    record.name = ObjectName(body->Tag());
    record.height = dimensions[0];
    record.width = dimensions[1];
    record.length = dimensions[2];

    if (!EstimateTubeDimensionsFromEdges(body->Tag(), record.length, record.width, record.height))
    {
        record.height = dimensions[0];
        record.width = dimensions[1];
        record.length = dimensions[2];
    }

    if (ReclassifyTubeDimensionsBySpec(record.length, record.width, record.height) &&
        HasClosedRectangularTubeSections(body->Tag()))
    {
        record.thickness = EstimateWallThicknessFromEdges(body->Tag(), record.width, record.height);
        record.kind = std::fabs(record.width - record.height) <= 1.0 ? TubeKindSquare : TubeKindRectangular;
        record.spec = BuildTubeSpec(record.width, record.height, record.thickness);
        return record;
    }

    double roundDiameter = 0.0;
    double roundThickness = 0.0;
    if (EstimateRoundTubeDimensions(body->Tag(), record.length, roundDiameter, roundThickness))
    {
        record.kind = TubeKindRound;
        record.width = roundDiameter;
        record.height = roundDiameter;
        record.thickness = roundThickness;
        record.spec = BuildRoundTubeSpec(roundDiameter, roundThickness);
        return record;
    }

    record.kind = TubeKindUnknown;
    record.spec = BuildTubeSpec(record.width, record.height, 0.0);
    return record;
}

void OpenSpecTable()
{
    const std::string path = SharedSpecTablePath();
    if (path.empty())
    {
        MessageBoxW(NULL, L"Could not resolve application directory.", L"XuanZeGuanJian", MB_ICONERROR);
        return;
    }

    const DWORD attributes = GetFileAttributesA(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES)
    {
        MessageBoxA(NULL, path.c_str(), "Tube spec table not found", MB_ICONERROR);
        return;
    }

    HINSTANCE result = ShellExecuteA(NULL, "open", path.c_str(), NULL, NULL, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(result) <= 32)
    {
        ShellExecuteA(NULL, "open", "notepad.exe", path.c_str(), NULL, SW_SHOWNORMAL);
    }
}

int RequiredPieces(double totalLength, double standardLength)
{
    if (totalLength <= 0.0 || standardLength <= 0.0)
    {
        return 0;
    }
    return static_cast<int>(std::ceil(totalLength / standardLength));
}

std::vector<StatRow> BuildStats(const std::vector<TubeRecord>& records)
{
    std::map<std::string, StatRow> rows;
    for (std::size_t index = 0; index < records.size(); ++index)
    {
        if (records[index].kind == TubeKindUnknown)
        {
            continue;
        }

        const std::string lengthText = FormatSpecValue(records[index].length);
        std::ostringstream keyStream;
        keyStream << static_cast<int>(records[index].kind) << "|" << records[index].name << "|" << records[index].spec << "|" << lengthText;
        const std::string key = keyStream.str();
        StatRow& row = rows[key];
        row.kind = records[index].kind;
        row.name = records[index].name.empty() ? "-" : records[index].name;
        row.spec = records[index].spec;
        row.lengthText = lengthText;
        row.quantity += 1;
        row.totalLength += records[index].length;
        if (records[index].body != NULL)
        {
            row.bodyTags.push_back(records[index].body->Tag());
        }
    }

    std::vector<StatRow> result;
    for (std::map<std::string, StatRow>::const_iterator it = rows.begin(); it != rows.end(); ++it)
    {
        result.push_back(it->second);
    }
    return result;
}

class XuanZeGuanJianDialog
{
public:
    XuanZeGuanJianDialog()
        : session(NXOpen::Session::GetSession()),
          ui(NXOpen::UI::GetUI()),
          dialog(NULL),
          bodySelection(NULL),
          squareToggle(NULL),
          rectangularToggle(NULL),
          roundToggle(NULL),
          countButton(NULL),
          materialEnum(NULL),
          sameBodyToggle(NULL),
          materialButton(NULL),
          specButton(NULL),
          resultList(NULL),
          resultTree(NULL),
          resultTreeInitialized(false),
          clearingBodySelectionDisplay(false),
          ignoreNextBodySelectionUpdate(false),
          layerToggle(NULL),
          squareLayerBlock(NULL),
          rectangularLayerBlock(NULL),
          roundLayerBlock(NULL),
          totalSetCountBlock(NULL),
          squareStandardLengthBlock(NULL),
          rectangularStandardLengthBlock(NULL),
          roundStandardLengthBlock(NULL)
    {
        dialog = ui->CreateDialog("XuanZeGuanJian.dlx");
        dialog->AddInitializeHandler(NXOpen::make_callback(this, &XuanZeGuanJianDialog::Initialize));
        dialog->AddDialogShownHandler(NXOpen::make_callback(this, &XuanZeGuanJianDialog::DialogShown));
        dialog->AddUpdateHandler(NXOpen::make_callback(this, &XuanZeGuanJianDialog::Update));
        dialog->AddEnableOKButtonHandler(NXOpen::make_callback(this, &XuanZeGuanJianDialog::EnableOkApply));
        dialog->AddOkHandler(NXOpen::make_callback(this, &XuanZeGuanJianDialog::Ok));
        dialog->AddApplyHandler(NXOpen::make_callback(this, &XuanZeGuanJianDialog::Apply));
    }

    ~XuanZeGuanJianDialog()
    {
        if (dialog != NULL)
        {
            delete dialog;
            dialog = NULL;
        }
    }

    int Show()
    {
        return dialog->Show();
    }

private:
    NXOpen::Session* session;
    NXOpen::UI* ui;
    NXOpen::BlockStyler::BlockDialog* dialog;
    NXOpen::BlockStyler::SelectObject* bodySelection;
    NXOpen::BlockStyler::Toggle* squareToggle;
    NXOpen::BlockStyler::Toggle* rectangularToggle;
    NXOpen::BlockStyler::Toggle* roundToggle;
    NXOpen::BlockStyler::Button* countButton;
    NXOpen::BlockStyler::Enumeration* materialEnum;
    NXOpen::BlockStyler::Toggle* sameBodyToggle;
    NXOpen::BlockStyler::Button* materialButton;
    NXOpen::BlockStyler::Button* specButton;
    NXOpen::BlockStyler::ListBox* resultList;
    NXOpen::BlockStyler::Tree* resultTree;
    bool resultTreeInitialized;
    bool clearingBodySelectionDisplay;
    bool ignoreNextBodySelectionUpdate;
    std::map<NXOpen::BlockStyler::Node*, std::vector<tag_t> > resultNodeBodies;
    std::vector<NXOpen::BlockStyler::Node*> resultNodes;
    std::vector<tag_t> highlightedBodies;
    std::vector<std::vector<tag_t> > sameBodyGroups;
    NXOpen::BlockStyler::Toggle* layerToggle;
    NXOpen::BlockStyler::IntegerBlock* squareLayerBlock;
    NXOpen::BlockStyler::IntegerBlock* rectangularLayerBlock;
    NXOpen::BlockStyler::IntegerBlock* roundLayerBlock;
    NXOpen::BlockStyler::IntegerBlock* totalSetCountBlock;
    NXOpen::BlockStyler::StringBlock* squareStandardLengthBlock;
    NXOpen::BlockStyler::StringBlock* rectangularStandardLengthBlock;
    NXOpen::BlockStyler::StringBlock* roundStandardLengthBlock;
    std::vector<std::string> materialOptions;
    std::vector<TubeRecord> lastRecords;
    std::vector<NXOpen::Body*> cachedBodies;

    void Initialize()
    {
        bodySelection = dynamic_cast<NXOpen::BlockStyler::SelectObject*>(dialog->TopBlock()->FindBlock("bodySelection"));
        squareToggle = dynamic_cast<NXOpen::BlockStyler::Toggle*>(dialog->TopBlock()->FindBlock("squareToggle"));
        rectangularToggle = dynamic_cast<NXOpen::BlockStyler::Toggle*>(dialog->TopBlock()->FindBlock("rectangularToggle"));
        roundToggle = dynamic_cast<NXOpen::BlockStyler::Toggle*>(dialog->TopBlock()->FindBlock("roundToggle"));
        countButton = dynamic_cast<NXOpen::BlockStyler::Button*>(dialog->TopBlock()->FindBlock("countButton"));
        materialEnum = dynamic_cast<NXOpen::BlockStyler::Enumeration*>(dialog->TopBlock()->FindBlock("materialEnum"));
        sameBodyToggle = dynamic_cast<NXOpen::BlockStyler::Toggle*>(dialog->TopBlock()->FindBlock("sameBodyToggle"));
        materialButton = dynamic_cast<NXOpen::BlockStyler::Button*>(dialog->TopBlock()->FindBlock("materialButton"));
        specButton = dynamic_cast<NXOpen::BlockStyler::Button*>(dialog->TopBlock()->FindBlock("specButton"));
        resultList = dynamic_cast<NXOpen::BlockStyler::ListBox*>(dialog->TopBlock()->FindBlock("resultList"));
        resultTree = dynamic_cast<NXOpen::BlockStyler::Tree*>(dialog->TopBlock()->FindBlock("tree_control0"));
        if (resultTree != NULL)
        {
            resultTree->SetOnSelectHandler(NXOpen::make_callback(this, &XuanZeGuanJianDialog::OnResultTreeSelect));
        }
        layerToggle = dynamic_cast<NXOpen::BlockStyler::Toggle*>(dialog->TopBlock()->FindBlock("layerToggle"));
        squareLayerBlock = dynamic_cast<NXOpen::BlockStyler::IntegerBlock*>(dialog->TopBlock()->FindBlock("squareLayer"));
        rectangularLayerBlock = dynamic_cast<NXOpen::BlockStyler::IntegerBlock*>(dialog->TopBlock()->FindBlock("rectangularLayer"));
        roundLayerBlock = dynamic_cast<NXOpen::BlockStyler::IntegerBlock*>(dialog->TopBlock()->FindBlock("roundLayer"));
        totalSetCountBlock = dynamic_cast<NXOpen::BlockStyler::IntegerBlock*>(dialog->TopBlock()->FindBlock("totalSetCount"));
        squareStandardLengthBlock = dynamic_cast<NXOpen::BlockStyler::StringBlock*>(dialog->TopBlock()->FindBlock("squareStandardLength"));
        rectangularStandardLengthBlock = dynamic_cast<NXOpen::BlockStyler::StringBlock*>(dialog->TopBlock()->FindBlock("rectangularStandardLength"));
        roundStandardLengthBlock = dynamic_cast<NXOpen::BlockStyler::StringBlock*>(dialog->TopBlock()->FindBlock("roundStandardLength"));
        SetDefaultText(squareStandardLengthBlock, "6.0");
        SetDefaultText(rectangularStandardLengthBlock, "6.0");
        SetDefaultText(roundStandardLengthBlock, "6.0");
        LoadMaterialEnum();

        if (bodySelection != NULL)
        {
            std::vector<NXOpen::Selection::MaskTriple> masks;
            NXOpen::Selection::MaskTriple mask;
            mask.Type = UF_solid_type;
            mask.Subtype = UF_solid_body_subtype;
            mask.SolidBodySubtype = 0;
            masks.push_back(mask);
            bodySelection->SetSelectionFilter(NXOpen::Selection::SelectionActionClearAndEnableSpecific, masks);
        }

        UpdateLayerControls();
    }

    void DialogShown()
    {
        SafeInitializeResultTree();
        UpdateLayerControls();
        SafeRefreshList();
    }

    int Update(NXOpen::BlockStyler::UIBlock* block)
    {
        if (block == countButton)
        {
            SafeExportTable();
        }
        else if (block == bodySelection)
        {
            const std::vector<NXOpen::Body*> bodies = GetSelectedBodies();
            if (clearingBodySelectionDisplay || (ignoreNextBodySelectionUpdate && bodies.empty()))
            {
                ignoreNextBodySelectionUpdate = false;
            }
            else if (bodies.empty())
            {
                sameBodyGroups.clear();
                cachedBodies.clear();
                lastRecords.clear();
                SafeRefreshList();
            }
            else
            {
                SafeRebuildListForCurrentMode(true);
            }
        }
        else if (block == sameBodyToggle)
        {
            ClearHighlightedBodies();
            sameBodyGroups.clear();
            SafeRebuildListForCurrentMode(true);
        }
        else if (block == materialEnum)
        {
            SafeRefreshList();
        }
        else if (block == materialButton)
        {
            SafeCalculateMaterialUsage();
        }
        else if (block == specButton)
        {
            OpenSpecTable();
        }
        else if (block == layerToggle)
        {
            UpdateLayerControls();
        }
        else if (block == squareToggle || block == rectangularToggle || block == roundToggle ||
                 block == squareStandardLengthBlock || block == rectangularStandardLengthBlock ||
                 block == roundStandardLengthBlock || block == totalSetCountBlock ||
                 block == squareLayerBlock || block == rectangularLayerBlock || block == roundLayerBlock)
        {
            SafeRebuildListForCurrentMode(block == squareToggle || block == rectangularToggle || block == roundToggle);
        }
        return 0;
    }

    int Ok()
    {
        ClearHighlightedBodies();
        SafeCountSelectedBodies();
        if (IsLayeringEnabled())
        {
            SafeApplyLayering();
        }
        return 0;
    }

    int Apply()
    {
        ClearHighlightedBodies();
        SafeCountSelectedBodies();
        return 0;
    }

    bool EnableOkApply()
    {
        return !GetSelectedBodies().empty() || !cachedBodies.empty();
    }

    bool IsLayeringEnabled() const
    {
        return layerToggle != NULL && layerToggle->Value();
    }

    bool IsKindEnabled(TubeKind kind) const
    {
        if (kind == TubeKindSquare)
        {
            return squareToggle == NULL || squareToggle->Value();
        }
        if (kind == TubeKindRectangular)
        {
            return rectangularToggle == NULL || rectangularToggle->Value();
        }
        if (kind == TubeKindRound)
        {
            return roundToggle == NULL || roundToggle->Value();
        }
        return false;
    }

    void LoadMaterialEnum()
    {
        materialOptions = LoadMaterialOptions();
        if (materialEnum == NULL)
        {
            return;
        }

        std::vector<NXOpen::NXString> members;
        for (std::size_t index = 0; index < materialOptions.size(); ++index)
        {
            members.push_back(NXOpen::NXString(materialOptions[index], NXOpen::NXString::UTF8));
        }
        if (!members.empty())
        {
            materialEnum->SetEnumMembers(members);
            materialEnum->SetValueAsString(members.front());
        }
    }

    std::string GetSelectedMaterial() const
    {
        if (materialEnum != NULL)
        {
            try
            {
                const std::string value = NxStringToUtf8(materialEnum->ValueAsString());
                if (!value.empty())
                {
                    return value;
                }
            }
            catch (...)
            {
            }
        }
        return materialOptions.empty() ? std::string("-") : materialOptions.front();
    }

    bool IsSameBodyMode() const
    {
        return sameBodyToggle != NULL && sameBodyToggle->Value();
    }

    double StandardLengthForKind(TubeKind kind) const
    {
        if (kind == TubeKindSquare)
        {
            return GetStandardLength(squareStandardLengthBlock);
        }
        if (kind == TubeKindRectangular)
        {
            return GetStandardLength(rectangularStandardLengthBlock);
        }
        if (kind == TubeKindRound)
        {
            return GetStandardLength(roundStandardLengthBlock);
        }
        return 0.0;
    }

    int LayerForKind(TubeKind kind) const
    {
        if (kind == TubeKindSquare)
        {
            return GetLayerValue(squareLayerBlock, 21);
        }
        if (kind == TubeKindRectangular)
        {
            return GetLayerValue(rectangularLayerBlock, 22);
        }
        if (kind == TubeKindRound)
        {
            return GetLayerValue(roundLayerBlock, 23);
        }
        return 1;
    }

    void LogInternalError(const char* stage, const char* message)
    {
        NXOpen::ListingWindow* listing = session != NULL ? session->ListingWindow() : NULL;
        if (listing == NULL)
        {
            return;
        }
        if (!listing->IsOpen())
        {
            listing->Open();
        }
        std::ostringstream line;
        line << "XuanZeGuanJian " << stage << ": " << (message != NULL ? message : "");
        listing->WriteLine(line.str().c_str());
    }

    void SafeInitializeResultTree()
    {
        try
        {
            InitializeResultTree();
        }
        catch (const NXOpen::NXException& ex)
        {
            resultTreeInitialized = false;
            resultTree = NULL;
            LogInternalError("tree init failed", ex.Message());
        }
        catch (const std::exception& ex)
        {
            resultTreeInitialized = false;
            resultTree = NULL;
            LogInternalError("tree init failed", ex.what());
        }
    }

    void SafeCountSelectedBodies()
    {
        try
        {
            CountSelectedBodies();
        }
        catch (const NXOpen::NXException& ex)
        {
            LogInternalError("count failed", ex.Message());
        }
        catch (const std::exception& ex)
        {
            LogInternalError("count failed", ex.what());
        }
    }

    void SafeRefreshList()
    {
        try
        {
            RefreshList();
        }
        catch (const NXOpen::NXException& ex)
        {
            if (resultTree != NULL && resultTreeInitialized)
            {
                try { resultTree->Redraw(true); } catch (...) {}
            }
            LogInternalError("refresh failed", ex.Message());
        }
        catch (const std::exception& ex)
        {
            if (resultTree != NULL && resultTreeInitialized)
            {
                try { resultTree->Redraw(true); } catch (...) {}
            }
            LogInternalError("refresh failed", ex.what());
        }
    }

    void SafeRebuildListForCurrentMode(bool reclassifyBodies)
    {
        try
        {
            RebuildListForCurrentMode(reclassifyBodies);
        }
        catch (const NXOpen::NXException& ex)
        {
            LogInternalError("rebuild list failed", ex.Message());
        }
        catch (const std::exception& ex)
        {
            LogInternalError("rebuild list failed", ex.what());
        }
    }

    void SafeExportTable()
    {
        try
        {
            CountSelectedBodies();
            ExportTable();
        }
        catch (const NXOpen::NXException& ex)
        {
            LogInternalError("export failed", ex.Message());
            ui->NXMessageBox()->Show("导出表格", NXOpen::NXMessageBox::DialogTypeError, ex.Message());
        }
        catch (const std::exception& ex)
        {
            LogInternalError("export failed", ex.what());
            ui->NXMessageBox()->Show("导出表格", NXOpen::NXMessageBox::DialogTypeError, ex.what());
        }
    }

    void SafeCalculateMaterialUsage()
    {
        try
        {
            CountSelectedBodies(false);
            CalculateMaterialUsage();
            RefreshList();
        }
        catch (const NXOpen::NXException& ex)
        {
            LogInternalError("material usage failed", ex.Message());
            ui->NXMessageBox()->Show("计算管材用量", NXOpen::NXMessageBox::DialogTypeError, ex.Message());
        }
        catch (const std::exception& ex)
        {
            LogInternalError("material usage failed", ex.what());
            ui->NXMessageBox()->Show("计算管材用量", NXOpen::NXMessageBox::DialogTypeError, ex.what());
        }
    }

    void SafeRunSameBodySearch()
    {
        try
        {
            RunSameBodySearch();
        }
        catch (const NXOpen::NXException& ex)
        {
            LogInternalError("same body search failed", ex.Message());
            ui->NXMessageBox()->Show("找相同", NXOpen::NXMessageBox::DialogTypeError, ex.Message());
        }
        catch (const std::exception& ex)
        {
            LogInternalError("same body search failed", ex.what());
            ui->NXMessageBox()->Show("找相同", NXOpen::NXMessageBox::DialogTypeError, ex.what());
        }
    }

    void SafeApplyLayering()
    {
        try
        {
            if (lastRecords.empty())
            {
                CountSelectedBodies(false);
            }
            ApplyLayering();
        }
        catch (const NXOpen::NXException& ex)
        {
            LogInternalError("layering failed", ex.Message());
        }
        catch (const std::exception& ex)
        {
            LogInternalError("layering failed", ex.what());
        }
    }

    void ClearHighlightedBodies()
    {
        bool changed = false;
        for (std::size_t index = 0; index < highlightedBodies.size(); ++index)
        {
            if (highlightedBodies[index] != NULL_TAG)
            {
                UF_DISP_set_highlight(highlightedBodies[index], 0);
                changed = true;
            }
        }
        highlightedBodies.clear();
        if (changed)
        {
            UF_DISP_refresh();
        }
    }

    void CollectKnownBodyTags(std::set<tag_t>& tags)
    {
        for (std::size_t index = 0; index < cachedBodies.size(); ++index)
        {
            if (cachedBodies[index] != NULL)
            {
                tags.insert(cachedBodies[index]->Tag());
            }
        }
        for (std::size_t index = 0; index < lastRecords.size(); ++index)
        {
            if (lastRecords[index].body != NULL)
            {
                tags.insert(lastRecords[index].body->Tag());
            }
        }
        for (std::size_t groupIndex = 0; groupIndex < sameBodyGroups.size(); ++groupIndex)
        {
            const std::vector<tag_t>& groupTags = sameBodyGroups[groupIndex];
            for (std::size_t tagIndex = 0; tagIndex < groupTags.size(); ++tagIndex)
            {
                if (groupTags[tagIndex] != NULL_TAG)
                {
                    tags.insert(groupTags[tagIndex]);
                }
            }
        }

        const std::vector<NXOpen::Body*> selectedBodies = GetSelectedBodies();
        for (std::size_t index = 0; index < selectedBodies.size(); ++index)
        {
            if (selectedBodies[index] != NULL)
            {
                tags.insert(selectedBodies[index]->Tag());
            }
        }
    }

    void ClearKnownBodyDisplayHighlights()
    {
        std::set<tag_t> tags;
        CollectKnownBodyTags(tags);
        bool changed = false;
        for (std::set<tag_t>::const_iterator it = tags.begin(); it != tags.end(); ++it)
        {
            if (*it != NULL_TAG)
            {
                UF_DISP_set_highlight(*it, 0);
                changed = true;
            }
        }
        if (changed)
        {
            UF_DISP_refresh();
        }
    }

    void HighlightBodies(const std::vector<tag_t>& bodyTags)
    {
        ClearKnownBodyDisplayHighlights();
        ClearHighlightedBodies();
        highlightedBodies = bodyTags;
        bool changed = false;
        for (std::size_t index = 0; index < highlightedBodies.size(); ++index)
        {
            if (highlightedBodies[index] != NULL_TAG)
            {
                UF_DISP_set_highlight(highlightedBodies[index], 1);
                changed = true;
            }
        }
        if (changed)
        {
            UF_DISP_refresh();
        }
    }

    void ClearBodySelectionDisplay()
    {
        if (bodySelection == NULL)
        {
            return;
        }

        clearingBodySelectionDisplay = true;
        ignoreNextBodySelectionUpdate = true;
        try
        {
            std::vector<NXOpen::TaggedObject*> emptySelection;
            bodySelection->SetSelectedObjects(emptySelection);
        }
        catch (...)
        {
            clearingBodySelectionDisplay = false;
            ignoreNextBodySelectionUpdate = false;
            throw;
        }
        clearingBodySelectionDisplay = false;
        ClearKnownBodyDisplayHighlights();
        UF_DISP_refresh();
    }

    void SetBodySelectionToBodies(const std::vector<NXOpen::Body*>& bodies)
    {
        if (bodySelection == NULL)
        {
            return;
        }

        std::vector<NXOpen::TaggedObject*> selectedObjects;
        selectedObjects.reserve(bodies.size());
        for (std::size_t index = 0; index < bodies.size(); ++index)
        {
            if (bodies[index] != NULL)
            {
                selectedObjects.push_back(bodies[index]);
            }
        }

        clearingBodySelectionDisplay = true;
        ignoreNextBodySelectionUpdate = true;
        try
        {
            bodySelection->SetSelectedObjects(selectedObjects);
        }
        catch (...)
        {
            clearingBodySelectionDisplay = false;
            ignoreNextBodySelectionUpdate = false;
            throw;
        }
        clearingBodySelectionDisplay = false;
        UF_DISP_refresh();
    }

    void OnResultTreeSelect(NXOpen::BlockStyler::Tree*, NXOpen::BlockStyler::Node* node, int, bool selected)
    {
        if (!selected)
        {
            ClearHighlightedBodies();
            return;
        }

        try
        {
            std::map<NXOpen::BlockStyler::Node*, std::vector<tag_t> >::const_iterator found = resultNodeBodies.find(node);
            if (found == resultNodeBodies.end())
            {
                ClearHighlightedBodies();
                return;
            }
            ClearBodySelectionDisplay();
            HighlightBodies(found->second);
        }
        catch (const NXOpen::NXException& ex)
        {
            LogInternalError("highlight failed", ex.Message());
        }
        catch (const std::exception& ex)
        {
            LogInternalError("highlight failed", ex.what());
        }
    }

    void InitializeResultTree()
    {
        if (resultTree == NULL || resultTreeInitialized)
        {
            return;
        }

        resultTree->InsertColumn(ResultColumnIndex, "序号", 50);
        resultTree->InsertColumn(ResultColumnType, "类型", 70);
        resultTree->InsertColumn(ResultColumnName, "名称", 110);
        resultTree->InsertColumn(ResultColumnMaterial, "材料", 80);
        resultTree->InsertColumn(ResultColumnSpec, "规格", 95);
        resultTree->InsertColumn(ResultColumnLength, "长度", 70);
        resultTree->InsertColumn(ResultColumnQuantity, "数量", 55);
        resultTree->SetColumnResizePolicy(ResultColumnIndex, NXOpen::BlockStyler::Tree::ColumnResizePolicyConstantWidth);
        resultTree->SetColumnResizePolicy(ResultColumnType, NXOpen::BlockStyler::Tree::ColumnResizePolicyConstantWidth);
        resultTree->SetColumnResizePolicy(ResultColumnName, NXOpen::BlockStyler::Tree::ColumnResizePolicyConstantWidth);
        resultTree->SetColumnResizePolicy(ResultColumnMaterial, NXOpen::BlockStyler::Tree::ColumnResizePolicyConstantWidth);
        resultTree->SetColumnResizePolicy(ResultColumnSpec, NXOpen::BlockStyler::Tree::ColumnResizePolicyConstantWidth);
        resultTree->SetColumnResizePolicy(ResultColumnLength, NXOpen::BlockStyler::Tree::ColumnResizePolicyConstantWidth);
        resultTree->SetColumnResizePolicy(ResultColumnQuantity, NXOpen::BlockStyler::Tree::ColumnResizePolicyConstantWidth);
        resultTree->SetSortRootNodes(false);
        resultTree->SetColumnSortable(ResultColumnIndex, false);
        resultTree->SetColumnSortable(ResultColumnType, false);
        resultTree->SetColumnSortable(ResultColumnName, false);
        resultTree->SetColumnSortable(ResultColumnMaterial, false);
        resultTree->SetColumnSortable(ResultColumnSpec, false);
        resultTree->SetColumnSortable(ResultColumnLength, false);
        resultTree->SetColumnSortable(ResultColumnQuantity, false);
        resultTreeInitialized = true;
    }

    void ClearResultTree()
    {
        if (resultTree == NULL || !resultTreeInitialized)
        {
            return;
        }

        ClearHighlightedBodies();
        for (std::size_t index = resultNodes.size(); index > 0; --index)
        {
            NXOpen::BlockStyler::Node* node = resultNodes[index - 1];
            if (node != NULL)
            {
                try
                {
                    resultTree->DeleteNode(node);
                }
                catch (...)
                {
                }
            }
        }
        resultNodes.clear();
        resultNodeBodies.clear();
    }

    void PrepareResultTreeForRefresh()
    {
        if (resultTree != NULL && resultTreeInitialized)
        {
            resultTree->Redraw(false);
            ClearResultTree();
        }
        if (resultList != NULL)
        {
            std::vector<NXOpen::NXString> headerOnly;
            headerOnly.push_back("Index | Type | Name | Material | Spec | Length | Qty");
            resultList->SetListItems(headerOnly);
        }
    }

    void FinishResultTreeRefresh()
    {
        if (resultTree != NULL && resultTreeInitialized)
        {
            resultTree->Redraw(true);
        }
    }

    void AddResultTreeRow(
        const std::string& rowIndex,
        const std::string& type,
        const std::string& name,
        const std::string& material,
        const std::string& spec,
        const std::string& length,
        const std::string& quantity,
        const std::vector<tag_t>& bodyTags)
    {
        if (resultTree == NULL || !resultTreeInitialized)
        {
            return;
        }

        NXOpen::BlockStyler::Node* node = resultTree->CreateNode(rowIndex.c_str());
        resultTree->InsertNode(node, NULL, NULL, NXOpen::BlockStyler::Tree::NodeInsertOptionLast);
        resultNodes.push_back(node);
        node->SetColumnDisplayText(ResultColumnType, type.c_str());
        node->SetColumnDisplayText(ResultColumnName, name.c_str());
        node->SetColumnDisplayText(ResultColumnMaterial, material.c_str());
        node->SetColumnDisplayText(ResultColumnSpec, spec.c_str());
        node->SetColumnDisplayText(ResultColumnLength, length.c_str());
        node->SetColumnDisplayText(ResultColumnQuantity, quantity.c_str());
        resultNodeBodies[node] = bodyTags;
    }

    std::vector<NXOpen::Body*> GetSelectedBodies()
    {
        std::vector<NXOpen::Body*> bodies;
        if (bodySelection == NULL)
        {
            return bodies;
        }

        NXOpen::BlockStyler::PropertyList* properties = bodySelection->GetProperties();
        std::vector<NXOpen::TaggedObject*> selectedObjects = properties->GetTaggedObjectVector("SelectedObjects");
        delete properties;

        std::set<tag_t> seen;
        for (std::size_t index = 0; index < selectedObjects.size(); ++index)
        {
            NXOpen::Body* body = dynamic_cast<NXOpen::Body*>(selectedObjects[index]);
            if (body != NULL && seen.insert(body->Tag()).second)
            {
                bodies.push_back(body);
            }
        }
        return bodies;
    }

    void CountSelectedBodies(bool refreshAfterCount = true)
    {
        lastRecords.clear();
        std::vector<NXOpen::Body*> bodies = GetSelectedBodies();
        const bool usingCachedBodies = bodies.empty() && !cachedBodies.empty();
        if (usingCachedBodies)
        {
            bodies = cachedBodies;
        }

        std::vector<NXOpen::Body*> acceptedBodies;
        std::set<tag_t> acceptedTags;
        for (std::size_t index = 0; index < bodies.size(); ++index)
        {
            TubeRecord record = ClassifyBody(bodies[index]);
            if (record.kind == TubeKindUnknown)
            {
                continue;
            }

            if (acceptedTags.insert(bodies[index]->Tag()).second)
            {
                acceptedBodies.push_back(bodies[index]);
            }

            lastRecords.push_back(record);
        }

        cachedBodies = acceptedBodies;
        sameBodyGroups.clear();
        if (!usingCachedBodies && !bodies.empty())
        {
            SetBodySelectionToBodies(acceptedBodies);
        }
        if (refreshAfterCount)
        {
            RefreshList();
        }
    }

    void RebuildListForCurrentMode(bool reclassifyBodies)
    {
        PrepareResultTreeForRefresh();
        if (reclassifyBodies || lastRecords.empty())
        {
            CountSelectedBodies(false);
        }
        else
        {
            sameBodyGroups.clear();
        }

        if (IsSameBodyMode())
        {
            RunSameBodySearch();
        }
        else
        {
            sameBodyGroups.clear();
            RefreshList();
        }
    }

    void RunSameBodySearch()
    {
        std::vector<NXOpen::Body*> tubeBodies;
        std::set<tag_t> seenTags;
        for (std::size_t index = 0; index < lastRecords.size(); ++index)
        {
            if (lastRecords[index].body == NULL || lastRecords[index].kind == TubeKindUnknown)
            {
                continue;
            }

            if (seenTags.insert(lastRecords[index].body->Tag()).second)
            {
                tubeBodies.push_back(lastRecords[index].body);
            }
        }

        if (tubeBodies.size() < 2)
        {
            sameBodyGroups.clear();
            RefreshList();
            return;
        }

        sameBodyGroups = tube_same_body::RunSameBodySearch(session, ui, tubeBodies);
        RefreshList();
    }

    const TubeRecord* FindRecordByTag(tag_t tag) const
    {
        for (std::size_t index = 0; index < lastRecords.size(); ++index)
        {
            if (lastRecords[index].body != NULL && lastRecords[index].body->Tag() == tag)
            {
                return &lastRecords[index];
            }
        }
        return NULL;
    }

    bool AppendSameBodyRows(std::vector<NXOpen::NXString>& items, int setCount)
    {
        if (!IsSameBodyMode())
        {
            return false;
        }

        int displayIndex = 1;
        const std::string material = GetSelectedMaterial();
        for (std::size_t groupIndex = 0; groupIndex < sameBodyGroups.size(); ++groupIndex)
        {
            const std::vector<tag_t>& groupTags = sameBodyGroups[groupIndex];
            if (groupTags.empty())
            {
                continue;
            }

            const TubeRecord* record = FindRecordByTag(groupTags[0]);
            if (record == NULL || !IsKindEnabled(record->kind))
            {
                continue;
            }

            const int totalQuantity = static_cast<int>(groupTags.size()) * setCount;
            const std::string rowIndex = FormatSpecValue(static_cast<double>(displayIndex++));
            const std::string lengthText = FormatSpecValue(record->length);
            std::ostringstream line;
            line << rowIndex
                 << " | " << KindName(record->kind)
                 << " | " << record->name
                 << " | " << material
                 << " | " << record->spec
                 << " | " << lengthText
                 << " | " << totalQuantity;
            items.push_back(line.str().c_str());
            AddResultTreeRow(
                rowIndex,
                KindName(record->kind),
                record->name,
                material,
                record->spec,
                lengthText,
                FormatSpecValue(static_cast<double>(totalQuantity)),
                groupTags);
        }

        if (items.size() == 1)
        {
            return true;
        }
        return true;
    }

    void RefreshList()
    {
        if (resultList == NULL && resultTree == NULL)
        {
            return;
        }

        std::vector<NXOpen::NXString> items;
        const int setCount = GetSetCount(totalSetCountBlock);
        const std::string material = GetSelectedMaterial();
        items.push_back("Index | Type | Name | Material | Spec | Length | Qty");
        PrepareResultTreeForRefresh();

        if (AppendSameBodyRows(items, setCount))
        {
            FinishResultTreeRefresh();
            if (resultList != NULL)
            {
                resultList->SetListItems(items);
            }
            return;
        }

        const std::vector<StatRow> rows = BuildStats(lastRecords);
        int displayIndex = 1;
        for (std::size_t index = 0; index < rows.size(); ++index)
        {
            if (!IsKindEnabled(rows[index].kind))
            {
                continue;
            }

            const int totalQuantity = rows[index].quantity * setCount;
            const std::string rowIndex = FormatSpecValue(static_cast<double>(displayIndex++));
            std::ostringstream line;
            line << rowIndex
                 << " | " << KindName(rows[index].kind)
                 << " | " << rows[index].name
                 << " | " << material
                 << " | " << rows[index].spec
                 << " | " << rows[index].lengthText
                 << " | " << totalQuantity;
            items.push_back(line.str().c_str());
            AddResultTreeRow(
                rowIndex,
                KindName(rows[index].kind),
                rows[index].name,
                material,
                rows[index].spec,
                rows[index].lengthText,
                FormatSpecValue(static_cast<double>(totalQuantity)),
                rows[index].bodyTags);
        }

        int unknownCount = 0;
        for (std::size_t index = 0; index < lastRecords.size(); ++index)
        {
            if (lastRecords[index].kind == TubeKindUnknown)
            {
                ++unknownCount;
            }
        }
        if (items.size() == 1)
        {
            FinishResultTreeRefresh();
            if (resultList != NULL)
            {
                resultList->SetListItems(items);
            }
            return;
        }
        if (unknownCount > 0)
        {
            std::ostringstream line;
            line << "- | Unknown | - | " << material << " | - | - | " << unknownCount * setCount;
            items.push_back(line.str().c_str());
            AddResultTreeRow("-", "Unknown", "-", material, "-", "-", FormatSpecValue(static_cast<double>(unknownCount * setCount)), std::vector<tag_t>());
        }

        FinishResultTreeRefresh();
        if (resultList != NULL)
        {
            resultList->SetListItems(items);
        }
    }

    std::wstring WorkPartDirectory() const
    {
        if (session != NULL && session->Parts() != NULL && session->Parts()->Work() != NULL)
        {
            const char* fullPathText = session->Parts()->Work()->FullPath().GetUTF8Text();
            if (fullPathText != NULL && fullPathText[0] != '\0')
            {
                std::wstring fullPath = Utf8ToWide(fullPathText);
                const std::wstring::size_type slash = fullPath.find_last_of(L"\\/");
                if (slash != std::wstring::npos)
                {
                    return fullPath.substr(0, slash);
                }
            }
        }
        return L"D:\\";
    }

    std::wstring DefaultExportPath() const
    {
        wchar_t stamp[64] = {0};
        std::time_t now = std::time(NULL);
        std::tm localTime = {};
        localtime_s(&localTime, &now);
        std::wcsftime(stamp, sizeof(stamp) / sizeof(stamp[0]), L"%Y%m%d_%H%M%S", &localTime);
        return WorkPartDirectory() + L"\\激光切管清单_" + stamp + L".xlsx";
    }

    bool AskExportPath(std::wstring& path) const
    {
        path = DefaultExportPath();
        wchar_t buffer[MAX_PATH] = {0};
        wcsncpy_s(buffer, path.c_str(), _TRUNCATE);
        const std::wstring initialDir = WorkPartDirectory();

        OPENFILENAMEW ofn = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.lpstrFilter = L"Excel 工作簿 (*.xlsx)\0*.xlsx\0\0";
        ofn.lpstrFile = buffer;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrInitialDir = initialDir.c_str();
        ofn.lpstrDefExt = L"xlsx";
        ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (!GetSaveFileNameW(&ofn))
        {
            return false;
        }

        path = buffer;
        if (path.size() < 5 || _wcsicmp(path.substr(path.size() - 5).c_str(), L".xlsx") != 0)
        {
            path += L".xlsx";
        }
        return true;
    }

    std::string CsvEscape(const std::string& text) const
    {
        const bool quote = text.find_first_of(",\"\r\n") != std::string::npos;
        std::string result;
        if (quote)
        {
            result += '"';
        }
        for (std::size_t index = 0; index < text.size(); ++index)
        {
            if (text[index] == '"')
            {
                result += "\"\"";
            }
            else
            {
                result += text[index];
            }
        }
        if (quote)
        {
            result += '"';
        }
        return result;
    }

    void WriteUtf8File(const std::wstring& path, const std::string& content, bool bom) const
    {
        FILE* file = NULL;
        if (_wfopen_s(&file, path.c_str(), L"wb") != 0 || file == NULL)
        {
            throw std::runtime_error("无法写入临时导出文件");
        }
        if (bom)
        {
            const unsigned char bytes[] = {0xEF, 0xBB, 0xBF};
            fwrite(bytes, 1, sizeof(bytes), file);
        }
        fwrite(content.data(), 1, content.size(), file);
        fclose(file);
    }

    std::wstring TempPathWithExtension(const wchar_t* extension) const
    {
        wchar_t tempDir[MAX_PATH] = {0};
        wchar_t tempFile[MAX_PATH] = {0};
        GetTempPathW(MAX_PATH, tempDir);
        GetTempFileNameW(tempDir, L"xzg", 0, tempFile);
        std::wstring path = tempFile;
        DeleteFileW(path.c_str());
        path += extension;
        return path;
    }

    void WriteCsvForExport(const std::wstring& csvPath, const std::vector<StatRow>& rows) const
    {
        std::ostringstream csv;
        csv << "Type,Name,Material,Spec,Length,Quantity\n";
        const int setCount = GetSetCount(totalSetCountBlock);
        const std::string material = GetSelectedMaterial();
        for (std::size_t index = 0; index < rows.size(); ++index)
        {
            if (!IsKindEnabled(rows[index].kind))
            {
                continue;
            }
            csv << CsvEscape(KindName(rows[index].kind)) << ","
                << CsvEscape(rows[index].name) << ","
                << CsvEscape(material) << ","
                << CsvEscape(rows[index].spec) << ","
                << CsvEscape(rows[index].lengthText) << ","
                << rows[index].quantity * setCount << "\n";
        }
        WriteUtf8File(csvPath, csv.str(), true);
    }

    int RunProcessAndWait(const std::wstring& commandLine) const
    {
        std::vector<wchar_t> buffer(commandLine.begin(), commandLine.end());
        buffer.push_back(L'\0');
        STARTUPINFOW startup = {};
        PROCESS_INFORMATION process = {};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESHOWWINDOW;
        startup.wShowWindow = SW_HIDE;
        if (!CreateProcessW(NULL, &buffer[0], NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &startup, &process))
        {
            return -1;
        }
        WaitForSingleObject(process.hProcess, INFINITE);
        DWORD exitCode = 1;
        GetExitCodeProcess(process.hProcess, &exitCode);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        return static_cast<int>(exitCode);
    }

    void ExportRowsToWorkbook(const std::wstring& outputPath, const std::vector<StatRow>& rows) const
    {
        const std::wstring templatePath = L"D:\\UG智辉钣金插件\\DATA\\激光切管清单.xlsx";
        if (GetFileAttributesW(templatePath.c_str()) == INVALID_FILE_ATTRIBUTES)
        {
            throw std::runtime_error("未找到模板: D:\\UG智辉钣金插件\\DATA\\激光切管清单.xlsx");
        }

        const std::wstring csvPath = TempPathWithExtension(L".csv");
        const std::wstring psPath = TempPathWithExtension(L".ps1");
        WriteCsvForExport(csvPath, rows);

        std::ostringstream script;
        script << "param([string]$Template,[string]$Csv,[string]$Out)\n"
               << "$ErrorActionPreference='Stop'\n"
               << "Add-Type -AssemblyName System.IO.Compression.FileSystem\n"
               << "$tmp=Join-Path ([IO.Path]::GetTempPath()) ([guid]::NewGuid().ToString('N'))\n"
               << "[IO.Directory]::CreateDirectory($tmp) | Out-Null\n"
               << "[IO.Compression.ZipFile]::ExtractToDirectory($Template,$tmp)\n"
               << "$sheetPath=Join-Path $tmp 'xl\\worksheets\\sheet1.xml'\n"
               << "[xml]$xml=Get-Content -LiteralPath $sheetPath -Encoding UTF8\n"
               << "$ns=$xml.DocumentElement.NamespaceURI\n"
               << "$nsm=New-Object System.Xml.XmlNamespaceManager($xml.NameTable)\n"
               << "$nsm.AddNamespace('d',$ns)\n"
               << "$sheetData=$xml.SelectSingleNode('//d:sheetData',$nsm)\n"
               << "@($sheetData.SelectNodes('d:row',$nsm) | Where-Object { [int]$_.r -ge 4 }) | ForEach-Object { [void]$sheetData.RemoveChild($_) }\n"
               << "$rows=Import-Csv -LiteralPath $Csv -Encoding UTF8\n"
               << "function Cell([xml]$doc,[string]$ref,[object]$value,[bool]$number){\n"
               << "  $c=$doc.CreateElement('c',$ns); $c.SetAttribute('r',$ref)\n"
               << "  if($number){ $v=$doc.CreateElement('v',$ns); $v.InnerText=[string]$value; [void]$c.AppendChild($v) }\n"
               << "  else { $c.SetAttribute('t','inlineStr'); $is=$doc.CreateElement('is',$ns); $t=$doc.CreateElement('t',$ns); $t.InnerText=[string]$value; [void]$is.AppendChild($t); [void]$c.AppendChild($is) }\n"
               << "  return $c\n"
               << "}\n"
               << "$r=4; $i=1\n"
               << "foreach($row in $rows){\n"
               << "  $rowNode=$xml.CreateElement('row',$ns); $rowNode.SetAttribute('r',[string]$r)\n"
               << "  [void]$rowNode.AppendChild((Cell $xml ('A'+$r) $i $true))\n"
               << "  [void]$rowNode.AppendChild((Cell $xml ('B'+$r) $row.Type $false))\n"
               << "  [void]$rowNode.AppendChild((Cell $xml ('C'+$r) $row.Name $false))\n"
               << "  [void]$rowNode.AppendChild((Cell $xml ('D'+$r) $row.Spec $false))\n"
               << "  [void]$rowNode.AppendChild((Cell $xml ('E'+$r) ([double]$row.Length) $true))\n"
               << "  [void]$rowNode.AppendChild((Cell $xml ('F'+$r) $row.Material $false))\n"
               << "  [void]$rowNode.AppendChild((Cell $xml ('G'+$r) ([int]$row.Quantity) $true))\n"
               << "  [void]$rowNode.AppendChild((Cell $xml ('H'+$r) '' $false))\n"
               << "  [void]$sheetData.AppendChild($rowNode)\n"
               << "  $r++; $i++\n"
               << "}\n"
               << "$dim=$xml.SelectSingleNode('//d:dimension',$nsm); if($dim -ne $null){ $dim.SetAttribute('ref', ('A1:H'+[Math]::Max(4,$r-1))) }\n"
               << "$settings=New-Object System.Xml.XmlWriterSettings; $settings.Encoding=New-Object System.Text.UTF8Encoding($false); $settings.Indent=$false\n"
               << "$writer=[System.Xml.XmlWriter]::Create($sheetPath,$settings); $xml.Save($writer); $writer.Close()\n"
               << "if(Test-Path -LiteralPath $Out){ Remove-Item -LiteralPath $Out -Force }\n"
               << "[IO.Compression.ZipFile]::CreateFromDirectory($tmp,$Out)\n"
               << "Remove-Item -LiteralPath $tmp -Recurse -Force\n";
        WriteUtf8File(psPath, script.str(), true);

        const std::wstring command = L"powershell.exe -NoProfile -ExecutionPolicy Bypass -File " +
            QuoteArgument(psPath) + L" " + QuoteArgument(templatePath) + L" " +
            QuoteArgument(csvPath) + L" " + QuoteArgument(outputPath);
        const int exitCode = RunProcessAndWait(command);
        DeleteFileW(csvPath.c_str());
        DeleteFileW(psPath.c_str());
        if (exitCode != 0)
        {
            throw std::runtime_error("表格导出失败，请确认目标文件未被打开且保存目录可写");
        }
    }

    void ExportTable()
    {
        const std::vector<StatRow> rows = BuildStats(lastRecords);
        if (rows.empty())
        {
            return;
        }

        std::wstring outputPath;
        if (!AskExportPath(outputPath))
        {
            return;
        }

        ExportRowsToWorkbook(outputPath, rows);
        HINSTANCE result = ShellExecuteW(NULL, L"open", outputPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
        if (reinterpret_cast<INT_PTR>(result) <= 32)
        {
            const std::string message = "导出完成，但打开失败:\n" + WideToUtf8(outputPath);
            OutputDebugStringA(message.c_str());
        }
    }

    std::vector<CutStock> PackCutPieces(const std::vector<double>& pieceLengths, double standardLength) const
    {
        std::vector<double> pieces = pieceLengths;
        std::sort(pieces.begin(), pieces.end(), std::greater<double>());

        std::vector<CutStock> stocks;
        for (std::size_t pieceIndex = 0; pieceIndex < pieces.size(); ++pieceIndex)
        {
            const double piece = pieces[pieceIndex];
            std::size_t bestIndex = stocks.size();
            double bestRemain = DBL_MAX;
            for (std::size_t stockIndex = 0; stockIndex < stocks.size(); ++stockIndex)
            {
                const double remain = standardLength - stocks[stockIndex].usedLength - piece;
                if (remain >= -0.001 && remain < bestRemain)
                {
                    bestRemain = remain;
                    bestIndex = stockIndex;
                }
            }

            if (bestIndex == stocks.size())
            {
                CutStock stock = {};
                stock.usedLength = 0.0;
                stocks.push_back(stock);
                bestIndex = stocks.size() - 1;
            }

            stocks[bestIndex].pieces.push_back(piece);
            stocks[bestIndex].usedLength += piece;
        }

        return stocks;
    }

    std::wstring MaterialUsageOutputPath() const
    {
        wchar_t tempPath[MAX_PATH] = {0};
        wchar_t tempFile[MAX_PATH] = {0};
        if (GetTempPathW(MAX_PATH, tempPath) == 0 ||
            GetTempFileNameW(tempPath, L"tube", 0, tempFile) == 0)
        {
            return WorkPartDirectory() + L"\\管材用量.txt";
        }

        std::wstring path = tempFile;
        DeleteFileW(path.c_str());
        return path + L".txt";
    }

    void WriteUtf8TextFileWithBom(const std::wstring& path, const std::string& text) const
    {
        std::ofstream file(path.c_str(), std::ios::binary);
        if (!file)
        {
            throw std::runtime_error("无法创建管材用量文本文件");
        }

        const unsigned char bom[] = {0xEF, 0xBB, 0xBF};
        file.write(reinterpret_cast<const char*>(bom), sizeof(bom));
        file.write(text.c_str(), static_cast<std::streamsize>(text.size()));
    }

    void CalculateMaterialUsage()
    {
        const std::vector<StatRow> rows = BuildStats(lastRecords);
        if (rows.empty())
        {
            return;
        }

        const int setCount = GetSetCount(totalSetCountBlock);
        std::map<std::string, std::vector<double> > piecesByGroup;
        std::map<std::string, TubeKind> kindByGroup;
        std::map<std::string, std::string> titleByGroup;
        for (std::size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex)
        {
            if (!IsKindEnabled(rows[rowIndex].kind))
            {
                continue;
            }

            std::ostringstream keyStream;
            keyStream << static_cast<int>(rows[rowIndex].kind) << "|" << rows[rowIndex].spec;
            const std::string key = keyStream.str();
            kindByGroup[key] = rows[rowIndex].kind;
            titleByGroup[key] = std::string(KindName(rows[rowIndex].kind)) + " 规格 " + rows[rowIndex].spec;

            const int quantity = std::max(0, rows[rowIndex].quantity * setCount);
            for (int count = 0; count < quantity; ++count)
            {
                piecesByGroup[key].push_back(rows[rowIndex].bodyTags.empty() ? 0.0 : rows[rowIndex].totalLength / rows[rowIndex].quantity);
            }
        }

        std::ostringstream report;
        report << "选择管件-管材用量计算\r\n";
        report << "总套数: " << setCount << "\r\n\r\n";

        int groupIndex = 1;
        double grandStockLength = 0.0;
        double grandUsedLength = 0.0;
        for (std::map<std::string, std::vector<double> >::const_iterator it = piecesByGroup.begin(); it != piecesByGroup.end(); ++it)
        {
            std::vector<double> pieces;
            for (std::size_t pieceIndex = 0; pieceIndex < it->second.size(); ++pieceIndex)
            {
                if (it->second[pieceIndex] > 0.0)
                {
                    pieces.push_back(it->second[pieceIndex]);
                }
            }
            if (pieces.empty())
            {
                continue;
            }

            const TubeKind kind = kindByGroup[it->first];
            const double standardLength = StandardLengthForKind(kind);
            if (standardLength <= 0.0)
            {
                continue;
            }

            std::vector<CutStock> stocks = PackCutPieces(pieces, standardLength);
            double usedLength = 0.0;
            for (std::size_t pieceIndex = 0; pieceIndex < pieces.size(); ++pieceIndex)
            {
                usedLength += pieces[pieceIndex];
            }
            const double stockLength = stocks.size() * standardLength;
            grandStockLength += stockLength;
            grandUsedLength += usedLength;

            std::map<int, int> pieceSummary;
            for (std::size_t pieceIndex = 0; pieceIndex < pieces.size(); ++pieceIndex)
            {
                ++pieceSummary[static_cast<int>(std::floor(RoundTo(pieces[pieceIndex], 1.0) + 0.5))];
            }

            report << groupIndex++ << ". " << titleByGroup[it->first] << "\r\n";
            report << "   标准长度: " << FormatStandardLength(standardLength)
                   << "，需整管: " << stocks.size() << " 根"
                   << "，用料: " << FormatDouble(stockLength / 1000.0, 1) << "米"
                   << "，净长: " << FormatDouble(usedLength / 1000.0, 1) << "米"
                   << "，余料: " << FormatDouble((stockLength - usedLength) / 1000.0, 1) << "米\r\n";
            report << "   件长汇总: ";
            bool firstPiece = true;
            for (std::map<int, int>::const_iterator pieceIt = pieceSummary.begin(); pieceIt != pieceSummary.end(); ++pieceIt)
            {
                if (!firstPiece)
                {
                    report << "，";
                }
                firstPiece = false;
                report << pieceIt->first << "mm x " << pieceIt->second;
            }
            report << "\r\n";

            for (std::size_t stockIndex = 0; stockIndex < stocks.size(); ++stockIndex)
            {
                report << "   第 " << (stockIndex + 1) << " 根: ";
                for (std::size_t pieceIndex = 0; pieceIndex < stocks[stockIndex].pieces.size(); ++pieceIndex)
                {
                    if (pieceIndex != 0)
                    {
                        report << " + ";
                    }
                    report << FormatSpecValue(stocks[stockIndex].pieces[pieceIndex]);
                }
                report << " = " << FormatSpecValue(stocks[stockIndex].usedLength)
                       << "mm，余 " << FormatSpecValue(standardLength - stocks[stockIndex].usedLength) << "mm\r\n";
            }
            report << "\r\n";
        }

        report << "合计整管长度: " << FormatDouble(grandStockLength / 1000.0, 1) << "米\r\n";
        report << "合计净长度: " << FormatDouble(grandUsedLength / 1000.0, 1) << "米\r\n";
        report << "合计余料: " << FormatDouble((grandStockLength - grandUsedLength) / 1000.0, 1) << "米\r\n";

        const std::wstring path = MaterialUsageOutputPath();
        WriteUtf8TextFileWithBom(path, report.str());
        ShellExecuteW(NULL, L"open", L"notepad.exe", path.c_str(), NULL, SW_SHOWNORMAL);
    }

    void WriteListing(const std::vector<StatRow>& rows)
    {
        NXOpen::ListingWindow* listing = session->ListingWindow();
        if (listing == NULL)
        {
            return;
        }
        if (!listing->IsOpen())
        {
            listing->Open();
        }

        listing->WriteLine("选择管件统计结果:");
        const int setCount = GetSetCount(totalSetCountBlock);
        for (std::size_t index = 0; index < rows.size(); ++index)
        {
            if (!IsKindEnabled(rows[index].kind))
            {
                continue;
            }
            const double standardLength = StandardLengthForKind(rows[index].kind);
            const int totalQuantity = rows[index].quantity * setCount;
            const double totalLength = rows[index].totalLength * setCount;
            std::ostringstream line;
            line << KindName(rows[index].kind) << " "
                 << rows[index].spec << " 长度=" << rows[index].lengthText
                 << " 数量=" << totalQuantity
                 << " 总长=" << FormatDouble(totalLength, 1)
                 << " 标准长=" << FormatStandardLength(standardLength)
                 << " 用量=" << (standardLength > 0.0 ? RequiredPieces(totalLength, standardLength) : 0);
            listing->WriteLine(line.str().c_str());
        }
    }

    void ApplyLayering()
    {
        int movedCount = 0;
        for (std::size_t index = 0; index < lastRecords.size(); ++index)
        {
            if (lastRecords[index].body == NULL || !IsKindEnabled(lastRecords[index].kind))
            {
                continue;
            }
            const int layer = LayerForKind(lastRecords[index].kind);
            int layerStatus = UF_LAYER_INACTIVE_LAYER;
            if (UF_LAYER_ask_status(layer, &layerStatus) == 0 && layerStatus != UF_LAYER_WORK_LAYER)
            {
                UF_LAYER_set_status(layer, UF_LAYER_ACTIVE_LAYER);
            }
            if (UF_OBJ_set_layer(lastRecords[index].body->Tag(), layer) == 0)
            {
                ++movedCount;
            }
        }

        (void)movedCount;
    }

    void SetBlockVisible(NXOpen::BlockStyler::UIBlock* block, bool visible)
    {
        if (block == NULL)
        {
            return;
        }
        NXOpen::BlockStyler::PropertyList* properties = block->GetProperties();
        properties->SetLogical("Show", visible);
        delete properties;
    }

    void UpdateLayerControls()
    {
        const bool show = IsLayeringEnabled();
        SetBlockVisible(squareLayerBlock, show);
        SetBlockVisible(rectangularLayerBlock, show);
        SetBlockVisible(roundLayerBlock, show);
    }
};
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
HMODULE module = LoadLibraryW(L"ZhaoFuNxLicenseGate.dll");
    if (module == NULL)
    {
        OutputDebugStringW(L"ZhaoFuNxLicenseGate.dll was not found in the UG application directory.\n");
        return false;
    }

    EnsureAuthorizedProc ensureAuthorized = reinterpret_cast<EnsureAuthorizedProc>(GetProcAddress(module, "ZfnxEnsureAuthorized"));
    if (ensureAuthorized == NULL)
    {
        OutputDebugStringW(L"ZhaoFuNxLicenseGate.dll does not export ZfnxEnsureAuthorized.\n");
        return false;
    }

    const int ok = ensureAuthorized(featureCode, displayName, message, static_cast<int>(sizeof(message) / sizeof(message[0])));
    
    const int ok2 = ok == 1 ? ensureAuthorized(featureCode, displayName, message, static_cast<int>(sizeof(message) / sizeof(message[0]))) : ok;
    if (ok != 1 || ok2 != 1)
    {
        OutputDebugStringW(message[0] != L'\0' ? message : L"License check failed.\n");
        return false;
    }

    return true;
}
}
extern "C" DllExport void ufusr(char* param, int* retcode, int param_len)
{
    if (!zhihui_license_guard::EnsureAuthorized(L"ZHIHUI.XUANZEGUANJIAN", L"XuanZeGuanJian"))
    {
        return;
    }

    (void)param;
    (void)param_len;
    if (retcode != NULL)
    {
        *retcode = 0;
    }

    try
    {
        UF_initialize();
        XuanZeGuanJianDialog commandDialog;
        commandDialog.Show();
        UF_terminate();
    }
    catch (const NXOpen::NXException& ex)
    {
        NXOpen::UI::GetUI()->NXMessageBox()->Show("选择管件", NXOpen::NXMessageBox::DialogTypeError, ex.Message());
        if (retcode != NULL)
        {
            *retcode = ex.ErrorCode();
        }
        UF_terminate();
    }
    catch (const std::exception& ex)
    {
        NXOpen::UI::GetUI()->NXMessageBox()->Show("选择管件", NXOpen::NXMessageBox::DialogTypeError, ex.what());
        if (retcode != NULL)
        {
            *retcode = 1;
        }
        UF_terminate();
    }
}

extern "C" DllExport int ufusr_ask_unload()
{
    return UF_UNLOAD_IMMEDIATELY;
}

