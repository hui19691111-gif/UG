#include <NXOpen/Body.hxx>
#include <NXOpen/BlockStyler_BlockDialog.hxx>
#include <NXOpen/BlockStyler_Button.hxx>
#include <NXOpen/BlockStyler_DoubleBlock.hxx>
#include <NXOpen/BlockStyler_Enumeration.hxx>
#include <NXOpen/BlockStyler_PropertyList.hxx>
#include <NXOpen/BlockStyler_SelectObject.hxx>
#include <NXOpen/BlockStyler_Toggle.hxx>
#include <NXOpen/BlockStyler_UIBlock.hxx>
#include <NXOpen/Callback.hxx>
#include <NXOpen/DisplayableObject.hxx>
#include <NXOpen/Direction.hxx>
#include <NXOpen/DirectionCollection.hxx>
#include <NXOpen/Edge.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/ListingWindow.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXMessageBox.hxx>
#include <NXOpen/NXString.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/Selection.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/SmartObject.hxx>
#include <NXOpen/TaggedObject.hxx>
#include <NXOpen/UI.hxx>

#include <uf.h>
#include <uf_defs.h>
#include <uf_exit.h>
#include <uf_modl.h>
#include <uf_modl_curves.h>
#include <uf_modl_sweep.h>
#include <uf_modl_utilities.h>
#include <uf_obj.h>
#include <uf_object_types.h>
#include <uf_part.h>
#include <uf_curve.h>
#include <uf_disp.h>
#include <uf_ui_types.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef DllExport
#define DllExport __declspec(dllexport)
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#ifdef CreateDialog
#undef CreateDialog
#endif

namespace zhihui_license_guard
{
typedef int(__stdcall* EnsureAuthorizedProc)(const wchar_t*, const wchar_t*, wchar_t*, int);

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
        wchar_t localPath[MAX_PATH] = {0};
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

    HMODULE fixedModule = LoadLibraryW(L"D:\\UG??????\\application\\ZhaoFuNxLicenseGate.dll");
    if (fixedModule != NULL)
    {
        return fixedModule;
    }

    return LoadLibraryW(moduleName);
}

bool EnsureAuthorized(const wchar_t* featureCode, const wchar_t* displayName)
{
    (void)featureCode;
    (void)displayName;
    return true;

    wchar_t message[1024] = {0};
    HMODULE module = LoadProtectedLicenseGate();
    if (module == NULL)
    {
        return false;
    }

    EnsureAuthorizedProc ensureAuthorized =
        reinterpret_cast<EnsureAuthorizedProc>(GetProcAddress(module, "ZfnxEnsureAuthorized"));
    if (ensureAuthorized == NULL)
    {
        return false;
    }

    const int ok = ensureAuthorized(
        featureCode,
        displayName,
        message,
        static_cast<int>(sizeof(message) / sizeof(message[0])));
    return ok == 1;
}
}

namespace
{
struct SquareTubeCandidate
{
    NXOpen::Body* body;
    double length;
    double width;
    double height;
    int planarFaceCount;
    bool accepted;
    std::string reason;
};

struct TubeStat
{
    std::string spec;
    std::string length;
    int quantity;
    double totalLength;
};

struct TubeSpec
{
    int width;
    int height;
};

std::string FormatDouble(double value)
{
    std::ostringstream stream;
    stream.setf(std::ios::fixed);
    stream.precision(2);
    stream << value;
    return stream.str();
}

std::string FormatVector3(const double value[3])
{
    std::ostringstream stream;
    stream.setf(std::ios::fixed);
    stream.precision(3);
    stream << "(" << value[0] << "," << value[1] << "," << value[2] << ")";
    return stream.str();
}

double RoundTo(double value, double step)
{
    if (step <= 0.0)
    {
        return value;
    }

    return std::floor(value / step + 0.5) * step;
}

std::string FormatSpecValue(double value)
{
    double rounded = RoundTo(value, 0.1);
    double integerValue = std::floor(rounded + 0.5);
    if (std::fabs(rounded - integerValue) < 0.01)
    {
        std::ostringstream stream;
        stream << static_cast<int>(integerValue);
        return stream.str();
    }

    std::ostringstream stream;
    stream.setf(std::ios::fixed);
    stream.precision(1);
    stream << rounded;
    return stream.str();
}

std::string FormatSectionSpecValue(double value)
{
    const double rounded = RoundTo(value, 5.0);
    std::ostringstream stream;
    stream << static_cast<int>(std::floor(rounded + 0.5));
    return stream.str();
}

std::string BuildSpec(double width, double height)
{
    return FormatSectionSpecValue(width) + "x" + FormatSectionSpecValue(height);
}

std::string BuildLengthKey(double length)
{
    return FormatSpecValue(length);
}

void ThrowUfError(int rc, const char* operation)
{
    if (rc == 0)
    {
        return;
    }

    std::ostringstream message;
    message << operation << " failed, UF error " << rc;
    throw std::runtime_error(message.str());
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

std::pair<int, int> NormalizeSpecPair(int first, int second)
{
    return std::make_pair(std::max(first, second), std::min(first, second));
}

void AddSpec(std::set<std::pair<int, int> >& specs, int first, int second)
{
    if (first > 0 && second > 0)
    {
        specs.insert(NormalizeSpecPair(first, second));
    }
}

void AddFallbackSpecs(std::set<std::pair<int, int> >& specs)
{
    const int squareSpecs[] = {10, 15, 20, 25, 30, 40, 50, 60, 70, 75, 80, 90, 100, 120, 125, 140, 150, 160, 180, 200, 250, 300};
    for (int index = 0; index < static_cast<int>(sizeof(squareSpecs) / sizeof(squareSpecs[0])); ++index)
    {
        AddSpec(specs, squareSpecs[index], squareSpecs[index]);
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
        AddSpec(specs, rectangularSpecs[index][0], rectangularSpecs[index][1]);
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
    DWORD length = GetModuleFileNameW(selfModule, widePath, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
    {
        return "";
    }

    char path[MAX_PATH] = {0};
    WideCharToMultiByte(CP_ACP, 0, widePath, -1, path, MAX_PATH, NULL, NULL);
    std::string directory(path);
    const std::string::size_type slash = directory.find_last_of("\\/");
    if (slash == std::string::npos)
    {
        return "";
    }

    return directory.substr(0, slash);
}

void LoadSpecsFromFile(const std::string& path, std::set<std::pair<int, int> >& specs)
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

        const int first = std::atoi(Trim(line.substr(0, separator)).c_str());
        const int second = std::atoi(Trim(line.substr(separator + 1)).c_str());
        AddSpec(specs, first, second);
    }
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
        LoadSpecsFromFile(moduleDirectory + "\\config\\FangTongKaKou_specs.txt", specs);
        LoadSpecsFromFile(moduleDirectory + "\\FangTongKaKou_specs.txt", specs);
    }

    if (specs.empty())
    {
        AddFallbackSpecs(specs);
    }
    return specs;
}

void OpenSpecTable()
{
    const std::string moduleDirectory = GetModuleDirectory();
    if (moduleDirectory.empty())
    {
        MessageBoxW(NULL, L"Could not resolve FangTongKaKou application directory.", L"FangTongKaKou", MB_ICONERROR);
        return;
    }

    const std::string path = moduleDirectory + "\\config\\FangTongKaKou_specs.txt";
    DWORD attributes = GetFileAttributesA(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES)
    {
        MessageBoxA(NULL, path.c_str(), "FangTongKaKou spec table not found", MB_ICONERROR);
        return;
    }

    HINSTANCE result = ShellExecuteA(NULL, "open", path.c_str(), NULL, NULL, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(result) <= 32)
    {
        ShellExecuteA(NULL, "open", "notepad.exe", path.c_str(), NULL, SW_SHOWNORMAL);
    }
}

void AppendDebugLog(const std::string& message)
{
    (void)message;
}

class DisplaySuppressionGuard
{
public:
    DisplaySuppressionGuard()
        : originalDisplayCode(UF_DISP_UNSUPPRESS_DISPLAY),
          suppressed(false)
    {
        if (UF_DISP_ask_display(&originalDisplayCode) == 0 &&
            originalDisplayCode == UF_DISP_UNSUPPRESS_DISPLAY &&
            UF_DISP_set_display(UF_DISP_SUPPRESS_DISPLAY) == 0)
        {
            suppressed = true;
        }
    }

    ~DisplaySuppressionGuard()
    {
        if (suppressed)
        {
            UF_DISP_set_display(originalDisplayCode);
        }
    }

private:
    int originalDisplayCode;
    bool suppressed;
};

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

double DistancePointToSegment3(const double point[3], const double segmentStart[3], const double segmentEnd[3])
{
    double segment[3] =
    {
        segmentEnd[0] - segmentStart[0],
        segmentEnd[1] - segmentStart[1],
        segmentEnd[2] - segmentStart[2]
    };
    const double lengthSquared = Dot3(segment, segment);
    if (lengthSquared < 1.0e-9)
    {
        return Distance3(point, segmentStart);
    }

    double startToPoint[3] =
    {
        point[0] - segmentStart[0],
        point[1] - segmentStart[1],
        point[2] - segmentStart[2]
    };
    double parameter = Dot3(startToPoint, segment) / lengthSquared;
    if (parameter < 0.0)
    {
        parameter = 0.0;
    }
    else if (parameter > 1.0)
    {
        parameter = 1.0;
    }

    double closest[3] =
    {
        segmentStart[0] + segment[0] * parameter,
        segmentStart[1] + segment[1] * parameter,
        segmentStart[2] + segment[2] * parameter
    };
    return Distance3(point, closest);
}

void Cross3(const double lhs[3], const double rhs[3], double result[3])
{
    result[0] = lhs[1] * rhs[2] - lhs[2] * rhs[1];
    result[1] = lhs[2] * rhs[0] - lhs[0] * rhs[2];
    result[2] = lhs[0] * rhs[1] - lhs[1] * rhs[0];
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

bool IsNearMultipleOfFive(double value)
{
    if (value <= 0.0)
    {
        return false;
    }

    const double nearest = RoundTo(value, 5.0);
    return std::fabs(value - nearest) <= 0.1;
}

bool ReclassifyTubeDimensionsBySectionStep(double& length, double& width, double& height)
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
            std::fabs(dimensions[secondIndex] - secondSpec) > 0.1)
        {
            continue;
        }

        if (specs.find(NormalizeSpecPair(firstSpec, secondSpec)) == specs.end())
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

        const double sectionMin = std::min(static_cast<double>(firstSpec), static_cast<double>(secondSpec));
        if (dimensions[lengthIndex] < std::max(5.0, sectionMin * 0.5))
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

    const Match* bestMatch = &matches.front();
    for (std::size_t index = 1; index < matches.size(); ++index)
    {
        if (matches[index].score > bestMatch->score)
        {
            bestMatch = &matches[index];
        }
    }

    length = dimensions[bestMatch->lengthIndex];
    width = static_cast<double>(std::max(bestMatch->firstSpec, bestMatch->secondSpec));
    height = static_cast<double>(std::min(bestMatch->firstSpec, bestMatch->secondSpec));
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

struct EdgeEndpointPair
{
    double first[3];
    double second[3];
};

struct FacePlacement
{
    tag_t faceTag;
    tag_t bodyTag;
    double center[3];
    double normal[3];
    double widthAxis[3];
    double lengthAxis[3];
    double lengthMin;
    double lengthMax;
    double widthMin;
    double widthMax;
    double endCoord;
    double inwardSign;
    double wallThickness;
    double toolDepth;
};

struct TouchingPortPlacement
{
    FacePlacement malePlacement;
    tag_t femaleBodyTag;
};

struct SelectedFaceInfo
{
    NXOpen::Face* face;
    double pickPoint[3];
};

bool AskFaceEdgeEndpointPairs(tag_t faceTag, std::vector<EdgeEndpointPair>& endpointPairs)
{
    endpointPairs.clear();
    uf_list_p_t edgeList = NULL;
    if (UF_MODL_ask_face_edges(faceTag, &edgeList) != 0)
    {
        return false;
    }

    std::vector<tag_t> edgeTags = UfListToTags(edgeList);
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

        EdgeEndpointPair endpoints = {};
        for (int axis = 0; axis < 3; ++axis)
        {
            endpoints.first[axis] = point1[axis];
            endpoints.second[axis] = point2[axis];
        }
        endpointPairs.push_back(endpoints);
    }
    return !endpointPairs.empty();
}

bool ComputeFaceCenterFromEndpoints(const std::vector<EdgeEndpointPair>& endpointPairs, double center[3])
{
    if (endpointPairs.empty())
    {
        return false;
    }

    center[0] = 0.0;
    center[1] = 0.0;
    center[2] = 0.0;
    int pointCount = 0;
    for (std::size_t index = 0; index < endpointPairs.size(); ++index)
    {
        for (int axis = 0; axis < 3; ++axis)
        {
            center[axis] += endpointPairs[index].first[axis] + endpointPairs[index].second[axis];
        }
        pointCount += 2;
    }

    if (pointCount <= 0)
    {
        return false;
    }

    center[0] /= static_cast<double>(pointCount);
    center[1] /= static_cast<double>(pointCount);
    center[2] /= static_cast<double>(pointCount);
    return true;
}

bool ComputeFaceNormalFromEdges(const std::vector<EdgeEndpointPair>& endpointPairs, double normal[3])
{
    double bestNormal[3] = {0.0, 0.0, 0.0};
    double bestScore = 0.0;
    for (std::size_t i = 0; i < endpointPairs.size(); ++i)
    {
        double firstDirection[3] =
        {
            endpointPairs[i].second[0] - endpointPairs[i].first[0],
            endpointPairs[i].second[1] - endpointPairs[i].first[1],
            endpointPairs[i].second[2] - endpointPairs[i].first[2]
        };
        const double firstLength = std::sqrt(Dot3(firstDirection, firstDirection));
        if (!Normalize3(firstDirection))
        {
            continue;
        }

        for (std::size_t j = i + 1; j < endpointPairs.size(); ++j)
        {
            double secondDirection[3] =
            {
                endpointPairs[j].second[0] - endpointPairs[j].first[0],
                endpointPairs[j].second[1] - endpointPairs[j].first[1],
                endpointPairs[j].second[2] - endpointPairs[j].first[2]
            };
            const double secondLength = std::sqrt(Dot3(secondDirection, secondDirection));
            if (!Normalize3(secondDirection))
            {
                continue;
            }

            double candidate[3] = {0.0, 0.0, 0.0};
            Cross3(firstDirection, secondDirection, candidate);
            const double sine = std::sqrt(Dot3(candidate, candidate));
            const double score = sine * firstLength * secondLength;
            if (score > bestScore && Normalize3(candidate))
            {
                bestScore = score;
                bestNormal[0] = candidate[0];
                bestNormal[1] = candidate[1];
                bestNormal[2] = candidate[2];
            }
        }
    }

    if (bestScore < 1.0e-6)
    {
        return false;
    }

    normal[0] = bestNormal[0];
    normal[1] = bestNormal[1];
    normal[2] = bestNormal[2];
    return true;
}

bool AskFaceNormalFromDirectionCollection(tag_t faceTag, double normal[3])
{
    NXOpen::Face* face =
        dynamic_cast<NXOpen::Face*>(NXOpen::NXObjectManager::Get(faceTag));
    if (face == NULL)
    {
        return false;
    }

    NXOpen::Session* session = NXOpen::Session::GetSession();
    if (session == NULL || session->Parts() == NULL || session->Parts()->Work() == NULL)
    {
        return false;
    }

    NXOpen::Direction* direction =
        session->Parts()->Work()->Directions()->CreateDumbDirectionFace(
            face,
            NXOpen::SenseForward,
            NXOpen::SmartObject::UpdateOptionWithinModeling);
    if (direction == NULL)
    {
        return false;
    }

    NXOpen::Vector3d vector = direction->Vector();
    normal[0] = vector.X;
    normal[1] = vector.Y;
    normal[2] = vector.Z;
    return Normalize3(normal);
}

bool AskPlanarFaceCenterAndEdgeNormal(tag_t faceTag, double center[3], double normal[3])
{
    int faceType = 0;
    double point[3] = {0.0, 0.0, 0.0};
    double direction[3] = {0.0, 0.0, 0.0};
    double faceBox[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    double radius = 0.0;
    double radData = 0.0;
    int normalDir = 0;
    if (UF_MODL_ask_face_data(faceTag, &faceType, point, direction, faceBox, &radius, &radData, &normalDir) != 0 ||
        faceType != UF_MODL_PLANAR_FACE)
    {
        return false;
    }

    std::vector<EdgeEndpointPair> endpointPairs;
    return AskFaceEdgeEndpointPairs(faceTag, endpointPairs) &&
        ComputeFaceCenterFromEndpoints(endpointPairs, center) &&
        AskFaceNormalFromDirectionCollection(faceTag, normal);
}

void AddScaledVector(const double origin[3], const double direction[3], double scale, double result[3])
{
    result[0] = origin[0] + direction[0] * scale;
    result[1] = origin[1] + direction[1] * scale;
    result[2] = origin[2] + direction[2] * scale;
}

void PointAtFaceCoords(const FacePlacement& placement, double lengthCoord, double widthCoord, double point[3])
{
    const double centerLength = Dot3(placement.center, placement.lengthAxis);
    const double centerWidth = Dot3(placement.center, placement.widthAxis);
    for (int axis = 0; axis < 3; ++axis)
    {
        point[axis] =
            placement.center[axis] +
            placement.lengthAxis[axis] * (lengthCoord - centerLength) +
            placement.widthAxis[axis] * (widthCoord - centerWidth);
    }
}

bool AskPlanarFacePlacement(NXOpen::Face* face, const double pickPoint[3], FacePlacement& placement, bool useWholeBodyEnds = true)
{
    if (face == NULL)
    {
        return false;
    }

    placement.faceTag = face->Tag();
    ThrowUfError(UF_MODL_ask_face_body(placement.faceTag, &placement.bodyTag), "UF_MODL_ask_face_body");
    if (placement.bodyTag == NULL_TAG)
    {
        return false;
    }

    int faceType = 0;
    double point[3] = {0.0, 0.0, 0.0};
    double direction[3] = {0.0, 0.0, 0.0};
    double faceBox[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    double radius = 0.0;
    double radData = 0.0;
    int normalDir = 0;
    ThrowUfError(
        UF_MODL_ask_face_data(placement.faceTag, &faceType, point, direction, faceBox, &radius, &radData, &normalDir),
        "UF_MODL_ask_face_data");
    if (faceType != UF_MODL_PLANAR_FACE)
    {
        return false;
    }

    std::vector<EdgeEndpointPair> endpointPairs;
    if (!AskFaceEdgeEndpointPairs(placement.faceTag, endpointPairs) ||
        !AskFaceNormalFromDirectionCollection(placement.faceTag, placement.normal))
    {
        return false;
    }

    double bestLength = 0.0;
    double bestAxis[3] = {0.0, 0.0, 0.0};
    for (std::size_t index = 0; index < endpointPairs.size(); ++index)
    {
        double edgeDirection[3] =
        {
            endpointPairs[index].second[0] - endpointPairs[index].first[0],
            endpointPairs[index].second[1] - endpointPairs[index].first[1],
            endpointPairs[index].second[2] - endpointPairs[index].first[2]
        };
        const double length = Distance3(endpointPairs[index].first, endpointPairs[index].second);
        if (length > bestLength && Normalize3(edgeDirection))
        {
            bestLength = length;
            bestAxis[0] = edgeDirection[0];
            bestAxis[1] = edgeDirection[1];
            bestAxis[2] = edgeDirection[2];
        }
    }

    if (!ComputeFaceCenterFromEndpoints(endpointPairs, placement.center))
    {
        placement.center[0] = (faceBox[0] + faceBox[3]) * 0.5;
        placement.center[1] = (faceBox[1] + faceBox[4]) * 0.5;
        placement.center[2] = (faceBox[2] + faceBox[5]) * 0.5;
    }

    if (bestLength > 1.0e-6)
    {
            placement.lengthAxis[0] = bestAxis[0];
            placement.lengthAxis[1] = bestAxis[1];
            placement.lengthAxis[2] = bestAxis[2];
    }
    else
    {
        double worldX[3] = {1.0, 0.0, 0.0};
        Cross3(placement.normal, worldX, placement.lengthAxis);
        if (!Normalize3(placement.lengthAxis))
        {
            double worldY[3] = {0.0, 1.0, 0.0};
            Cross3(placement.normal, worldY, placement.lengthAxis);
        }
    }

    if (!Normalize3(placement.lengthAxis))
    {
        return false;
    }

    double bestOppositeDistance = DBL_MAX;
    double bestOppositeSignedDistance = 0.0;
    uf_list_p_t orientationFaceList = NULL;
    ThrowUfError(UF_MODL_ask_body_faces(placement.bodyTag, &orientationFaceList), "UF_MODL_ask_body_faces");
    std::vector<tag_t> orientationFaces = UfListToTags(orientationFaceList);
    UF_MODL_delete_list(&orientationFaceList);
    for (std::size_t index = 0; index < orientationFaces.size(); ++index)
    {
        if (orientationFaces[index] == placement.faceTag)
        {
            continue;
        }

        double otherCenter[3] = {0.0, 0.0, 0.0};
        double otherNormal[3] = {0.0, 0.0, 0.0};
        if (!AskPlanarFaceCenterAndEdgeNormal(orientationFaces[index], otherCenter, otherNormal))
        {
            continue;
        }
        if (std::fabs(Dot3(otherNormal, placement.normal)) < 0.95)
        {
            continue;
        }

        double delta[3] =
        {
            otherCenter[0] - placement.center[0],
            otherCenter[1] - placement.center[1],
            otherCenter[2] - placement.center[2]
        };
        const double signedDistance = Dot3(delta, placement.normal);
        const double distance = std::fabs(signedDistance);
        if (distance > 0.05 && distance < bestOppositeDistance)
        {
            bestOppositeDistance = distance;
            bestOppositeSignedDistance = signedDistance;
        }
    }
    if (bestOppositeDistance < DBL_MAX && bestOppositeSignedDistance > 0.0)
    {
        placement.normal[0] = -placement.normal[0];
        placement.normal[1] = -placement.normal[1];
        placement.normal[2] = -placement.normal[2];
    }

    Cross3(placement.normal, placement.lengthAxis, placement.widthAxis);
    if (!Normalize3(placement.widthAxis))
    {
        return false;
    }

    double faceLengthMin = DBL_MAX;
    double faceLengthMax = -DBL_MAX;
    placement.widthMin = DBL_MAX;
    placement.widthMax = -DBL_MAX;
    if (!endpointPairs.empty())
    {
        for (std::size_t index = 0; index < endpointPairs.size(); ++index)
        {
            const double lengthProjection1 = Dot3(endpointPairs[index].first, placement.lengthAxis);
            const double lengthProjection2 = Dot3(endpointPairs[index].second, placement.lengthAxis);
            faceLengthMin = std::min(faceLengthMin, std::min(lengthProjection1, lengthProjection2));
            faceLengthMax = std::max(faceLengthMax, std::max(lengthProjection1, lengthProjection2));

            const double widthProjection1 = Dot3(endpointPairs[index].first, placement.widthAxis);
            const double widthProjection2 = Dot3(endpointPairs[index].second, placement.widthAxis);
            placement.widthMin = std::min(placement.widthMin, std::min(widthProjection1, widthProjection2));
            placement.widthMax = std::max(placement.widthMax, std::max(widthProjection1, widthProjection2));
        }
    }
    else
    {
        return false;
    }

    placement.lengthMin = faceLengthMin;
    placement.lengthMax = faceLengthMax;
    if (useWholeBodyEnds)
    {
        // Auto mode works on whole tubes, so use the body ends along the tube length.
        double bodyLengthMin = DBL_MAX;
        double bodyLengthMax = -DBL_MAX;
        uf_list_p_t bodyEdgeList = NULL;
        ThrowUfError(UF_MODL_ask_body_edges(placement.bodyTag, &bodyEdgeList), "UF_MODL_ask_body_edges");
        std::vector<tag_t> bodyEdgeTags = UfListToTags(bodyEdgeList);
        UF_MODL_delete_list(&bodyEdgeList);
        for (std::size_t index = 0; index < bodyEdgeTags.size(); ++index)
        {
            double point1[3] = {0.0, 0.0, 0.0};
            double point2[3] = {0.0, 0.0, 0.0};
            int vertexCount = 0;
            ThrowUfError(UF_MODL_ask_edge_verts(bodyEdgeTags[index], point1, point2, &vertexCount), "UF_MODL_ask_edge_verts");
            if (vertexCount != 2)
            {
                continue;
            }

            const double bodyProjection1 = Dot3(point1, placement.lengthAxis);
            const double bodyProjection2 = Dot3(point2, placement.lengthAxis);
            bodyLengthMin = std::min(bodyLengthMin, std::min(bodyProjection1, bodyProjection2));
            bodyLengthMax = std::max(bodyLengthMax, std::max(bodyProjection1, bodyProjection2));
        }
        if (bodyLengthMin != DBL_MAX && bodyLengthMax != -DBL_MAX)
        {
            placement.lengthMin = bodyLengthMin;
            placement.lengthMax = bodyLengthMax;
        }
    }

    const double pickProjection = Dot3(pickPoint, placement.lengthAxis);
    bool pickedEndEdgeFound = false;
    double pickedEndCoord = 0.0;
    if (!useWholeBodyEnds)
    {
        double bestDistance = DBL_MAX;
        for (std::size_t index = 0; index < endpointPairs.size(); ++index)
        {
            double edgeDirection[3] =
            {
                endpointPairs[index].second[0] - endpointPairs[index].first[0],
                endpointPairs[index].second[1] - endpointPairs[index].first[1],
                endpointPairs[index].second[2] - endpointPairs[index].first[2]
            };
            if (!Normalize3(edgeDirection))
            {
                continue;
            }

            const double lengthSpan = std::fabs(
                Dot3(endpointPairs[index].second, placement.lengthAxis) -
                Dot3(endpointPairs[index].first, placement.lengthAxis));
            const double widthSpan = std::fabs(
                Dot3(endpointPairs[index].second, placement.widthAxis) -
                Dot3(endpointPairs[index].first, placement.widthAxis));
            const double lengthAlignment = std::fabs(Dot3(edgeDirection, placement.lengthAxis));
            const bool isEndEdge =
                widthSpan > 0.1 &&
                lengthSpan <= std::max(0.5, widthSpan * 0.35) &&
                lengthAlignment < 0.65;
            if (!isEndEdge)
            {
                continue;
            }

            const double distance = DistancePointToSegment3(pickPoint, endpointPairs[index].first, endpointPairs[index].second);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                pickedEndCoord =
                    (Dot3(endpointPairs[index].first, placement.lengthAxis) +
                     Dot3(endpointPairs[index].second, placement.lengthAxis)) * 0.5;
                pickedEndEdgeFound = true;
            }
        }
    }

    if (pickedEndEdgeFound)
    {
        placement.endCoord = pickedEndCoord;
        if (std::fabs(pickedEndCoord - faceLengthMin) <= std::fabs(pickedEndCoord - faceLengthMax))
        {
            placement.inwardSign = 1.0;
        }
        else
        {
            placement.inwardSign = -1.0;
        }
    }
    else if (std::fabs(pickProjection - placement.lengthMin) <= std::fabs(pickProjection - placement.lengthMax))
    {
        placement.endCoord = placement.lengthMin;
        placement.inwardSign = 1.0;
    }
    else
    {
        placement.endCoord = placement.lengthMax;
        placement.inwardSign = -1.0;
    }

    double bodyBox[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    ThrowUfError(UF_MODL_ask_bounding_box(placement.bodyTag, bodyBox), "UF_MODL_ask_bounding_box");

    placement.wallThickness = bestOppositeDistance;
    uf_list_p_t faceList = NULL;
    ThrowUfError(UF_MODL_ask_body_faces(placement.bodyTag, &faceList), "UF_MODL_ask_body_faces");
    std::vector<tag_t> bodyFaces = UfListToTags(faceList);
    UF_MODL_delete_list(&faceList);
    for (std::size_t index = 0; index < bodyFaces.size(); ++index)
    {
        if (bodyFaces[index] == placement.faceTag)
        {
            continue;
        }

        double otherCenter[3] = {0.0, 0.0, 0.0};
        double otherNormal[3] = {0.0, 0.0, 0.0};
        if (!AskPlanarFaceCenterAndEdgeNormal(bodyFaces[index], otherCenter, otherNormal) ||
            std::fabs(Dot3(otherNormal, placement.normal)) < 0.95)
        {
            continue;
        }

        double delta[3] =
        {
            otherCenter[0] - placement.center[0],
            otherCenter[1] - placement.center[1],
            otherCenter[2] - placement.center[2]
        };
        const double distance = std::fabs(Dot3(delta, placement.normal));
        if (distance > 0.05 && distance < placement.wallThickness)
        {
            placement.wallThickness = distance;
        }
    }

    if (placement.wallThickness == DBL_MAX)
    {
        const double dx = bodyBox[3] - bodyBox[0];
        const double dy = bodyBox[4] - bodyBox[1];
        const double dz = bodyBox[5] - bodyBox[2];
        placement.wallThickness = std::max(0.5, std::min(dx, std::min(dy, dz)) * 0.05);
    }
    placement.toolDepth = placement.wallThickness + 1.0;
    return true;
}

std::vector<tag_t> CreateRectangleProfile(const FacePlacement& placement, double lengthStart, double lengthEnd, double widthStart, double widthEnd)
{
    double corners[4][3] = {};
    PointAtFaceCoords(placement, lengthStart, widthStart, corners[0]);
    PointAtFaceCoords(placement, lengthStart, widthEnd, corners[1]);
    PointAtFaceCoords(placement, lengthEnd, widthEnd, corners[2]);
    PointAtFaceCoords(placement, lengthEnd, widthStart, corners[3]);

    std::vector<tag_t> curveTags;
    for (int index = 0; index < 4; ++index)
    {
        UF_CURVE_line_t lineData = {};
        for (int axis = 0; axis < 3; ++axis)
        {
            lineData.start_point[axis] = corners[index][axis];
            lineData.end_point[axis] = corners[(index + 1) % 4][axis];
        }

        tag_t lineTag = NULL_TAG;
        ThrowUfError(UF_CURVE_create_line(&lineData, &lineTag), "UF_CURVE_create_line");
        curveTags.push_back(lineTag);
    }
    return curveTags;
}

struct FaceProfilePoint
{
    double length;
    double width;
};

std::vector<tag_t> CreateFaceProfile(const FacePlacement& placement, const std::vector<FaceProfilePoint>& points)
{
    std::vector<tag_t> curveTags;
    if (points.size() < 3)
    {
        return curveTags;
    }

    std::vector<std::vector<double> > worldPoints(points.size(), std::vector<double>(3, 0.0));
    for (std::size_t index = 0; index < points.size(); ++index)
    {
        PointAtFaceCoords(placement, points[index].length, points[index].width, &worldPoints[index][0]);
    }

    for (std::size_t index = 0; index < points.size(); ++index)
    {
        const std::size_t next = (index + 1) % points.size();
        UF_CURVE_line_t lineData = {};
        for (int axis = 0; axis < 3; ++axis)
        {
            lineData.start_point[axis] = worldPoints[index][axis];
            lineData.end_point[axis] = worldPoints[next][axis];
        }

        tag_t lineTag = NULL_TAG;
        ThrowUfError(UF_CURVE_create_line(&lineData, &lineTag), "UF_CURVE_create_line");
        curveTags.push_back(lineTag);
    }
    return curveTags;
}

double DegreesToRadians(double degrees)
{
    const double pi = 3.1415926535897932384626433832795;
    return degrees * pi / 180.0;
}

double ComputeTaperTipWidth(double baseWidth, double height, double angleDegrees)
{
    return baseWidth - 2.0 * height * std::tan(DegreesToRadians(angleDegrees) * 0.5);
}

std::vector<tag_t> CreateTrapezoidProfile(
    const FacePlacement& placement,
    double lengthStart,
    double lengthEnd,
    double widthCenter,
    double baseWidth,
    double tipWidth)
{
    if (baseWidth <= 0.0 || tipWidth <= 0.0)
    {
        throw std::runtime_error("Taper port width is invalid.");
    }

    std::vector<FaceProfilePoint> points;
    FaceProfilePoint point = {};
    point.length = lengthStart;
    point.width = widthCenter - baseWidth * 0.5;
    points.push_back(point);
    point.length = lengthStart;
    point.width = widthCenter + baseWidth * 0.5;
    points.push_back(point);
    point.length = lengthEnd;
    point.width = widthCenter + tipWidth * 0.5;
    points.push_back(point);
    point.length = lengthEnd;
    point.width = widthCenter - tipWidth * 0.5;
    points.push_back(point);
    return CreateFaceProfile(placement, points);
}

bool OffsetFaceProfile(
    const std::vector<FaceProfilePoint>& profile,
    double offset,
    std::vector<FaceProfilePoint>& offsetProfile)
{
    offsetProfile.clear();
    if (profile.size() < 3)
    {
        return false;
    }

    double signedArea = 0.0;
    for (std::size_t index = 0; index < profile.size(); ++index)
    {
        const std::size_t next = (index + 1) % profile.size();
        signedArea += profile[index].width * profile[next].length -
            profile[next].width * profile[index].length;
    }
    if (std::fabs(signedArea) < 1.0e-9)
    {
        return false;
    }

    struct OffsetLine
    {
        double nx;
        double ny;
        double c;
    };

    std::vector<OffsetLine> lines(profile.size());
    for (std::size_t index = 0; index < profile.size(); ++index)
    {
        const std::size_t next = (index + 1) % profile.size();
        const double x1 = profile[index].width;
        const double y1 = profile[index].length;
        const double x2 = profile[next].width;
        const double y2 = profile[next].length;
        const double dx = x2 - x1;
        const double dy = y2 - y1;
        const double length = std::sqrt(dx * dx + dy * dy);
        if (length < 1.0e-9)
        {
            return false;
        }

        if (signedArea > 0.0)
        {
            lines[index].nx = dy / length;
            lines[index].ny = -dx / length;
        }
        else
        {
            lines[index].nx = -dy / length;
            lines[index].ny = dx / length;
        }
        lines[index].c = lines[index].nx * x1 + lines[index].ny * y1 + offset;
    }

    for (std::size_t index = 0; index < profile.size(); ++index)
    {
        const std::size_t previous = (index + profile.size() - 1) % profile.size();
        const OffsetLine& first = lines[previous];
        const OffsetLine& second = lines[index];
        const double det = first.nx * second.ny - second.nx * first.ny;
        if (std::fabs(det) < 1.0e-9)
        {
            return false;
        }

        FaceProfilePoint point = {};
        point.width = (first.c * second.ny - second.c * first.ny) / det;
        point.length = (first.nx * second.c - second.nx * first.c) / det;
        offsetProfile.push_back(point);
    }

    return true;
}

void DeleteObjects(const std::vector<tag_t>& tags)
{
    for (std::size_t index = 0; index < tags.size(); ++index)
    {
        if (tags[index] != NULL_TAG)
        {
            UF_OBJ_delete_object(tags[index]);
        }
    }
}

tag_t CreateExtrudedToolBody(const std::vector<tag_t>& profileCurves, const double direction[3], double startLimitValue, double endLimitValue)
{
    uf_list_p_t curveList = NULL;
    ThrowUfError(UF_MODL_create_list(&curveList), "UF_MODL_create_list");
    for (std::size_t index = 0; index < profileCurves.size(); ++index)
    {
        ThrowUfError(UF_MODL_put_list_item(curveList, profileCurves[index]), "UF_MODL_put_list_item");
    }

    std::string startLimit = FormatDouble(startLimitValue);
    std::string endLimit = FormatDouble(endLimitValue);
    char taperAngle[] = "0.0";
    char* limits[2] =
    {
        const_cast<char*>(startLimit.c_str()),
        const_cast<char*>(endLimit.c_str())
    };
    double unusedPoint[3] = {0.0, 0.0, 0.0};
    double extrudeDirection[3] = {direction[0], direction[1], direction[2]};

    uf_list_p_t featureList = NULL;
    ThrowUfError(
        UF_MODL_create_extruded(
            curveList,
            taperAngle,
            limits,
            unusedPoint,
            extrudeDirection,
            UF_NULLSIGN,
            &featureList),
        "UF_MODL_create_extruded");
    UF_MODL_delete_list(&curveList);

    std::vector<tag_t> featureTags = UfListToTags(featureList);
    UF_MODL_delete_list(&featureList);
    if (featureTags.empty())
    {
        throw std::runtime_error("No tool body was created.");
    }

    tag_t toolBody = NULL_TAG;
    ThrowUfError(UF_MODL_ask_feat_body(featureTags.front(), &toolBody), "UF_MODL_ask_feat_body");
    if (toolBody == NULL_TAG)
    {
        throw std::runtime_error("Failed to resolve tool body.");
    }

    uf_list_p_t bodyList = NULL;
    ThrowUfError(UF_MODL_create_list(&bodyList), "UF_MODL_create_list");
    ThrowUfError(UF_MODL_put_list_item(bodyList, toolBody), "UF_MODL_put_list_item");
    ThrowUfError(UF_MODL_delete_body_parms(bodyList), "UF_MODL_delete_body_parms");
    ThrowUfError(UF_MODL_ask_list_item(bodyList, 0, &toolBody), "UF_MODL_ask_list_item");
    UF_MODL_delete_list(&bodyList);
    return toolBody;
}

void SubtractToolBody(tag_t targetBody, tag_t toolBody)
{
    int resultCount = 0;
    tag_t* resultingBodies = NULL;
    ThrowUfError(
        UF_MODL_subtract_bodies(targetBody, toolBody, &resultCount, &resultingBodies),
        "UF_MODL_subtract_bodies");
    if (resultingBodies != NULL)
    {
        UF_free(resultingBodies);
    }
}

FacePlacement OppositeEndPlacement(const FacePlacement& placement)
{
    FacePlacement opposite = placement;
    if (placement.inwardSign > 0.0)
    {
        opposite.endCoord = placement.lengthMax;
        opposite.inwardSign = -1.0;
    }
    else
    {
        opposite.endCoord = placement.lengthMin;
        opposite.inwardSign = 1.0;
    }
    return opposite;
}

double AskTubeOuterCornerRadiusForBody(tag_t bodyTag, const FacePlacement& referencePlacement)
{
    uf_list_p_t faceList = NULL;
    if (bodyTag == NULL_TAG || UF_MODL_ask_body_faces(bodyTag, &faceList) != 0 || faceList == NULL)
    {
        return 0.0;
    }

    std::vector<tag_t> faceTags = UfListToTags(faceList);
    UF_MODL_delete_list(&faceList);

    const double faceWidth = referencePlacement.widthMax - referencePlacement.widthMin;
    const double maxReasonableRadius = std::max(referencePlacement.wallThickness * 12.0, faceWidth * 0.75 + 2.0);
    double outerRadius = 0.0;
    int candidateCount = 0;
    for (std::size_t index = 0; index < faceTags.size(); ++index)
    {
        int faceType = 0;
        double point[3] = {0.0, 0.0, 0.0};
        double direction[3] = {0.0, 0.0, 0.0};
        double box[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
        double radius = 0.0;
        double radData = 0.0;
        int normalDir = 0;
        if (UF_MODL_ask_face_data(faceTags[index], &faceType, point, direction, box, &radius, &radData, &normalDir) != 0)
        {
            continue;
        }

        if (faceType != UF_MODL_CYLINDRICAL_FACE && faceType != UF_MODL_BLENDING_FACE)
        {
            continue;
        }
        if (radius <= 0.05 || radius > maxReasonableRadius || !Normalize3(direction))
        {
            continue;
        }

        ++candidateCount;
        outerRadius = std::max(outerRadius, radius);
    }

    std::ostringstream log;
    log << "AutoTubeR"
        << " body=" << bodyTag
        << " referenceBody=" << referencePlacement.bodyTag
        << " candidates=" << candidateCount
        << " outerR=" << FormatDouble(outerRadius)
        << " faceWidth=" << FormatDouble(faceWidth)
        << " wallThickness=" << FormatDouble(referencePlacement.wallThickness);
    AppendDebugLog(log.str());
    return outerRadius;
}

double AskTubeOuterCornerRadius(const FacePlacement& placement)
{
    return AskTubeOuterCornerRadiusForBody(placement.bodyTag, placement);
}

double ResolvePortHeight(double inputHeight, double outerRadius, bool autoRecognizeTubeR)
{
    const double resolvedHeight = inputHeight + outerRadius;
    std::ostringstream log;
    log << "ResolvePortHeight"
        << " autoR=" << (autoRecognizeTubeR ? 1 : 0)
        << " inputH=" << FormatDouble(inputHeight)
        << " outerR=" << FormatDouble(outerRadius)
        << " resolvedH=" << FormatDouble(resolvedHeight);
    AppendDebugLog(log.str());
    return resolvedHeight;
}

bool CreateMalePortAtPlacement(const FacePlacement& placement, double width, double height)
{
    const double faceWidth = placement.widthMax - placement.widthMin;
    if (width <= 0.0 || height <= 0.0 || width >= faceWidth - 0.1)
    {
        throw std::runtime_error("W must be smaller than the selected face width.");
    }

    const double widthCenter = (placement.widthMin + placement.widthMax) * 0.5;
    const double widthStart = widthCenter - width * 0.5;
    const double widthEnd = widthCenter + width * 0.5;
    const double lengthStart = placement.endCoord;
    const double lengthEnd = placement.endCoord - placement.inwardSign * height;
    const double tabThickness = placement.wallThickness;

    {
        std::ostringstream log;
        log << "MalePort"
            << " W=" << FormatDouble(width)
            << " H=" << FormatDouble(height)
            << " wallThickness=" << FormatDouble(placement.wallThickness)
            << " tabThickness=" << FormatDouble(tabThickness)
            << " faceWidth=" << FormatDouble(faceWidth)
            << " endCoord=" << FormatDouble(placement.endCoord)
            << " inwardSign=" << FormatDouble(placement.inwardSign);
        AppendDebugLog(log.str());
    }

    std::vector<tag_t> profile = CreateRectangleProfile(placement, lengthStart, lengthEnd, widthStart, widthEnd);
    tag_t toolBody = NULL_TAG;
    try
    {
        toolBody = CreateExtrudedToolBody(profile, placement.normal, -tabThickness, 0.0);
        const int uniteRc = UF_MODL_unite_bodies(placement.bodyTag, toolBody);
        if (uniteRc != 0)
        {
            std::ostringstream log;
            log << "MalePortUniteSkipped"
                << " body=" << placement.bodyTag
                << " toolBody=" << toolBody
                << " error=" << uniteRc
                << " W=" << FormatDouble(width)
                << " H=" << FormatDouble(height)
                << " endCoord=" << FormatDouble(placement.endCoord)
                << " inwardSign=" << FormatDouble(placement.inwardSign);
            AppendDebugLog(log.str());
            DeleteObjects(profile);
            return false;
        }
        {
            std::ostringstream log;
            log << "MalePortUniteSucceeded"
                << " body=" << placement.bodyTag
                << " W=" << FormatDouble(width)
                << " H=" << FormatDouble(height)
                << " endCoord=" << FormatDouble(placement.endCoord);
            AppendDebugLog(log.str());
        }
        DeleteObjects(profile);
        return true;
    }
    catch (...)
    {
        DeleteObjects(profile);
        throw;
    }
}

bool CreateTaperMalePortAtPlacement(const FacePlacement& placement, double width, double height, double angle)
{
    const double faceWidth = placement.widthMax - placement.widthMin;
    const double tipWidth = ComputeTaperTipWidth(width, height, angle);
    if (width <= 0.0 || height <= 0.0 || width >= faceWidth - 0.1 || tipWidth <= 0.1)
    {
        throw std::runtime_error("Taper W, H or angle is invalid for the selected face.");
    }

    const double widthCenter = (placement.widthMin + placement.widthMax) * 0.5;
    const double lengthStart = placement.endCoord;
    const double lengthEnd = placement.endCoord - placement.inwardSign * height;
    const double tabThickness = placement.wallThickness;

    {
        std::ostringstream log;
        log << "TaperMalePort"
            << " W=" << FormatDouble(width)
            << " H=" << FormatDouble(height)
            << " angle=" << FormatDouble(angle)
            << " tipWidth=" << FormatDouble(tipWidth)
            << " wallThickness=" << FormatDouble(placement.wallThickness)
            << " faceWidth=" << FormatDouble(faceWidth)
            << " endCoord=" << FormatDouble(placement.endCoord)
            << " inwardSign=" << FormatDouble(placement.inwardSign);
        AppendDebugLog(log.str());
    }

    std::vector<tag_t> profile =
        CreateTrapezoidProfile(placement, lengthStart, lengthEnd, widthCenter, width, tipWidth);
    tag_t toolBody = NULL_TAG;
    try
    {
        toolBody = CreateExtrudedToolBody(profile, placement.normal, -tabThickness, 0.0);
        const int uniteRc = UF_MODL_unite_bodies(placement.bodyTag, toolBody);
        if (uniteRc != 0)
        {
            std::ostringstream log;
            log << "TaperMalePortUniteSkipped"
                << " body=" << placement.bodyTag
                << " toolBody=" << toolBody
                << " error=" << uniteRc
                << " W=" << FormatDouble(width)
                << " H=" << FormatDouble(height)
                << " angle=" << FormatDouble(angle)
                << " endCoord=" << FormatDouble(placement.endCoord)
                << " inwardSign=" << FormatDouble(placement.inwardSign);
            AppendDebugLog(log.str());
            DeleteObjects(profile);
            return false;
        }
        {
            std::ostringstream log;
            log << "TaperMalePortUniteSucceeded"
                << " body=" << placement.bodyTag
                << " W=" << FormatDouble(width)
                << " H=" << FormatDouble(height)
                << " angle=" << FormatDouble(angle)
                << " endCoord=" << FormatDouble(placement.endCoord);
            AppendDebugLog(log.str());
        }
        DeleteObjects(profile);
        return true;
    }
    catch (...)
    {
        DeleteObjects(profile);
        throw;
    }
}

void CreateFemalePortAtPlacement(const FacePlacement& placement, tag_t targetBodyTag, double width, double height, double femaleExtrudeDepth, double clearance)
{
    if (targetBodyTag == NULL_TAG)
    {
        throw std::runtime_error("Female slot target body was not found.");
    }

    const double clearanceWidth = width + 2.0 * clearance;
    const double clearanceLength = height + 2.0 * clearance;
    const double faceWidth = placement.widthMax - placement.widthMin;
    if (clearanceWidth <= 0.0 || clearanceLength <= 0.0 || clearanceWidth >= faceWidth + placement.wallThickness)
    {
        throw std::runtime_error("Female slot dimensions are invalid for the selected face.");
    }

    const double widthCenter = (placement.widthMin + placement.widthMax) * 0.5;
    const double widthStart = widthCenter - clearanceWidth * 0.5;
    const double widthEnd = widthCenter + clearanceWidth * 0.5;
    const double lengthStart = placement.endCoord + placement.inwardSign * clearance;
    const double lengthEnd = placement.endCoord - placement.inwardSign * (height + clearance);
    const double tabThickness = femaleExtrudeDepth;
    const double thicknessStart = -femaleExtrudeDepth;
    const double thicknessEnd = 0.0;

    {
        std::ostringstream log;
        log << "FemalePort"
            << " W=" << FormatDouble(width)
            << " H=" << FormatDouble(height)
            << " femaleExtrudeDepth=" << FormatDouble(femaleExtrudeDepth)
            << " C=" << FormatDouble(clearance)
            << " sourceBody=" << placement.bodyTag
            << " targetBody=" << targetBodyTag
            << " clearanceWidth=" << FormatDouble(clearanceWidth)
            << " clearanceLength=" << FormatDouble(clearanceLength)
            << " wallThickness=" << FormatDouble(placement.wallThickness)
            << " tabThickness=" << FormatDouble(tabThickness)
            << " thicknessStart=" << FormatDouble(thicknessStart)
            << " thicknessEnd=" << FormatDouble(thicknessEnd)
            << " subtract=1"
            << " faceWidth=" << FormatDouble(faceWidth)
            << " endCoord=" << FormatDouble(placement.endCoord)
            << " inwardSign=" << FormatDouble(placement.inwardSign);
        AppendDebugLog(log.str());
    }

    std::vector<tag_t> profile = CreateRectangleProfile(placement, lengthStart, lengthEnd, widthStart, widthEnd);
    tag_t toolBody = NULL_TAG;
    try
    {
        toolBody = CreateExtrudedToolBody(profile, placement.normal, thicknessStart, thicknessEnd);
        SubtractToolBody(targetBodyTag, toolBody);
        UF_OBJ_delete_object(toolBody);
        toolBody = NULL_TAG;
        DeleteObjects(profile);
    }
    catch (...)
    {
        DeleteObjects(profile);
        throw;
    }
}

void CreateTaperFemalePortAtPlacement(const FacePlacement& placement, tag_t targetBodyTag, double width, double height, double femaleExtrudeDepth, double clearance, double angle)
{
    if (targetBodyTag == NULL_TAG)
    {
        throw std::runtime_error("Taper female slot target body was not found.");
    }

    const double maleTipWidth = ComputeTaperTipWidth(width, height, angle);
    const double faceWidth = placement.widthMax - placement.widthMin;
    if (maleTipWidth <= 0.1)
    {
        throw std::runtime_error("Taper female slot dimensions are invalid for the selected face.");
    }

    const double widthCenter = (placement.widthMin + placement.widthMax) * 0.5;
    const double lengthStart = placement.endCoord;
    const double lengthEnd = placement.endCoord - placement.inwardSign * height;
    const double tabThickness = femaleExtrudeDepth;
    const double thicknessStart = -femaleExtrudeDepth;
    const double thicknessEnd = 0.0;
    std::vector<FaceProfilePoint> maleProfile;
    FaceProfilePoint point = {};
    point.length = lengthStart;
    point.width = widthCenter - width * 0.5;
    maleProfile.push_back(point);
    point.length = lengthStart;
    point.width = widthCenter + width * 0.5;
    maleProfile.push_back(point);
    point.length = lengthEnd;
    point.width = widthCenter + maleTipWidth * 0.5;
    maleProfile.push_back(point);
    point.length = lengthEnd;
    point.width = widthCenter - maleTipWidth * 0.5;
    maleProfile.push_back(point);

    std::vector<FaceProfilePoint> clearanceProfile;
    if (!OffsetFaceProfile(maleProfile, clearance, clearanceProfile))
    {
        throw std::runtime_error("Failed to offset taper female profile.");
    }

    double profileWidthMin = DBL_MAX;
    double profileWidthMax = -DBL_MAX;
    double profileLengthMin = DBL_MAX;
    double profileLengthMax = -DBL_MAX;
    for (std::size_t index = 0; index < clearanceProfile.size(); ++index)
    {
        profileWidthMin = std::min(profileWidthMin, clearanceProfile[index].width);
        profileWidthMax = std::max(profileWidthMax, clearanceProfile[index].width);
        profileLengthMin = std::min(profileLengthMin, clearanceProfile[index].length);
        profileLengthMax = std::max(profileLengthMax, clearanceProfile[index].length);
    }
    const double clearanceWidth = profileWidthMax - profileWidthMin;
    const double clearanceLength = profileLengthMax - profileLengthMin;
    if (clearanceWidth <= 0.0 || clearanceLength <= 0.0 || clearanceWidth >= faceWidth + placement.wallThickness)
    {
        throw std::runtime_error("Taper female slot dimensions are invalid for the selected face.");
    }

    {
        std::ostringstream log;
        log << "TaperFemalePort"
            << " W=" << FormatDouble(width)
            << " H=" << FormatDouble(height)
            << " femaleExtrudeDepth=" << FormatDouble(femaleExtrudeDepth)
            << " C=" << FormatDouble(clearance)
            << " angle=" << FormatDouble(angle)
            << " sourceBody=" << placement.bodyTag
            << " targetBody=" << targetBodyTag
            << " maleTipWidth=" << FormatDouble(maleTipWidth)
            << " clearanceWidth=" << FormatDouble(clearanceWidth)
            << " clearanceLength=" << FormatDouble(clearanceLength)
            << " offsetProfile=1"
            << " wallThickness=" << FormatDouble(placement.wallThickness)
            << " tabThickness=" << FormatDouble(tabThickness)
            << " thicknessStart=" << FormatDouble(thicknessStart)
            << " thicknessEnd=" << FormatDouble(thicknessEnd)
            << " subtract=1"
            << " faceWidth=" << FormatDouble(faceWidth)
            << " endCoord=" << FormatDouble(placement.endCoord)
            << " inwardSign=" << FormatDouble(placement.inwardSign);
        AppendDebugLog(log.str());
    }

    std::vector<tag_t> profile = CreateFaceProfile(placement, clearanceProfile);
    tag_t toolBody = NULL_TAG;
    try
    {
        toolBody = CreateExtrudedToolBody(profile, placement.normal, thicknessStart, thicknessEnd);
        SubtractToolBody(targetBodyTag, toolBody);
        UF_OBJ_delete_object(toolBody);
        toolBody = NULL_TAG;
        DeleteObjects(profile);
    }
    catch (...)
    {
        DeleteObjects(profile);
        throw;
    }
}

int CountPlanarFaces(tag_t bodyTag)
{
    uf_list_p_t faceList = NULL;
    int rc = UF_MODL_ask_body_faces(bodyTag, &faceList);
    if (rc != 0)
    {
        throw NXOpen::NXException::Create(rc);
    }

    std::vector<tag_t> faceTags = UfListToTags(faceList);
    UF_MODL_delete_list(&faceList);

    int planarCount = 0;
    for (std::size_t index = 0; index < faceTags.size(); ++index)
    {
        int faceType = 0;
        double point[3] = {0.0, 0.0, 0.0};
        double dir[3] = {0.0, 0.0, 0.0};
        double box[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
        double radius = 0.0;
        double radData = 0.0;
        int normDir = 0;
        rc = UF_MODL_ask_face_data(faceTags[index], &faceType, point, dir, box, &radius, &radData, &normDir);
        if (rc != 0)
        {
            throw NXOpen::NXException::Create(rc);
        }

        if (faceType == 22)
        {
            ++planarCount;
        }
    }

    return planarCount;
}

struct DirectionCluster
{
    double direction[3];
    double totalLength;
    int count;
};

struct Point2
{
    double x;
    double y;
};

struct ProjectedSegment
{
    Point2 first;
    Point2 second;
};

struct ProjectedLineGroup
{
    Point2 origin;
    Point2 direction;
    std::vector<std::pair<double, double> > intervals;
};

struct LoopGraphStats
{
    int inputSegments;
    int shortSegments;
    int sameNodeSegments;
    int duplicateSegments;
    int uniqueSegments;
    int nodes;
    int components;
    int smallComponents;
    int deadEndComponents;
    int maxComponentCycleRank;
    int closedLoops;
};

double Distance2(const Point2& first, const Point2& second)
{
    const double dx = second.x - first.x;
    const double dy = second.y - first.y;
    return std::sqrt(dx * dx + dy * dy);
}

double Dot2(const Point2& first, const Point2& second)
{
    return first.x * second.x + first.y * second.y;
}

double Cross2(const Point2& first, const Point2& second)
{
    return first.x * second.y - first.y * second.x;
}

Point2 Subtract2(const Point2& first, const Point2& second)
{
    Point2 result = {first.x - second.x, first.y - second.y};
    return result;
}

Point2 AddScaled2(const Point2& point, const Point2& direction, double scale)
{
    Point2 result = {point.x + direction.x * scale, point.y + direction.y * scale};
    return result;
}

bool Normalize2(Point2& vector)
{
    const double length = std::sqrt(vector.x * vector.x + vector.y * vector.y);
    if (length <= 1.0e-9)
    {
        return false;
    }

    vector.x /= length;
    vector.y /= length;
    return true;
}

void CanonicalizeDirection2(Point2& direction)
{
    if (direction.x < -1.0e-9 || (std::fabs(direction.x) <= 1.0e-9 && direction.y < 0.0))
    {
        direction.x = -direction.x;
        direction.y = -direction.y;
    }
}

int FindOrAddProjectedNode(std::vector<Point2>& nodes, const Point2& point, double tolerance)
{
    const double toleranceSquared = tolerance * tolerance;
    for (std::size_t index = 0; index < nodes.size(); ++index)
    {
        const double dx = nodes[index].x - point.x;
        const double dy = nodes[index].y - point.y;
        if (dx * dx + dy * dy <= toleranceSquared)
        {
            return static_cast<int>(index);
        }
    }

    nodes.push_back(point);
    return static_cast<int>(nodes.size() - 1);
}

std::vector<ProjectedSegment> MergeCollinearProjectedSegments(const std::vector<ProjectedSegment>& segments, double lineTolerance, double gapTolerance)
{
    std::vector<ProjectedLineGroup> groups;
    const double parallelTolerance = std::sin(2.0 * 3.14159265358979323846 / 180.0);

    for (std::size_t index = 0; index < segments.size(); ++index)
    {
        Point2 direction = Subtract2(segments[index].second, segments[index].first);
        if (!Normalize2(direction))
        {
            continue;
        }
        CanonicalizeDirection2(direction);

        bool added = false;
        for (std::size_t groupIndex = 0; groupIndex < groups.size(); ++groupIndex)
        {
            ProjectedLineGroup& group = groups[groupIndex];
            if (std::fabs(Cross2(direction, group.direction)) > parallelTolerance)
            {
                continue;
            }

            const double distance1 = std::fabs(Cross2(Subtract2(segments[index].first, group.origin), group.direction));
            const double distance2 = std::fabs(Cross2(Subtract2(segments[index].second, group.origin), group.direction));
            if (distance1 > lineTolerance || distance2 > lineTolerance)
            {
                continue;
            }

            double intervalStart = Dot2(Subtract2(segments[index].first, group.origin), group.direction);
            double intervalEnd = Dot2(Subtract2(segments[index].second, group.origin), group.direction);
            if (intervalStart > intervalEnd)
            {
                std::swap(intervalStart, intervalEnd);
            }
            group.intervals.push_back(std::make_pair(intervalStart, intervalEnd));
            added = true;
            break;
        }

        if (!added)
        {
            ProjectedLineGroup group = {};
            group.origin = segments[index].first;
            group.direction = direction;
            double intervalStart = 0.0;
            double intervalEnd = Dot2(Subtract2(segments[index].second, group.origin), group.direction);
            if (intervalStart > intervalEnd)
            {
                std::swap(intervalStart, intervalEnd);
            }
            group.intervals.push_back(std::make_pair(intervalStart, intervalEnd));
            groups.push_back(group);
        }
    }

    std::vector<ProjectedSegment> mergedSegments;
    for (std::size_t groupIndex = 0; groupIndex < groups.size(); ++groupIndex)
    {
        ProjectedLineGroup& group = groups[groupIndex];
        if (group.intervals.empty())
        {
            continue;
        }

        std::sort(group.intervals.begin(), group.intervals.end());
        double currentStart = group.intervals[0].first;
        double currentEnd = group.intervals[0].second;
        for (std::size_t intervalIndex = 1; intervalIndex < group.intervals.size(); ++intervalIndex)
        {
            const double nextStart = group.intervals[intervalIndex].first;
            const double nextEnd = group.intervals[intervalIndex].second;
            if (nextStart <= currentEnd + gapTolerance)
            {
                currentEnd = std::max(currentEnd, nextEnd);
                continue;
            }

            if (currentEnd - currentStart > 1.0e-6)
            {
                ProjectedSegment segment = {};
                segment.first = AddScaled2(group.origin, group.direction, currentStart);
                segment.second = AddScaled2(group.origin, group.direction, currentEnd);
                mergedSegments.push_back(segment);
            }
            currentStart = nextStart;
            currentEnd = nextEnd;
        }

        if (currentEnd - currentStart > 1.0e-6)
        {
            ProjectedSegment segment = {};
            segment.first = AddScaled2(group.origin, group.direction, currentStart);
            segment.second = AddScaled2(group.origin, group.direction, currentEnd);
            mergedSegments.push_back(segment);
        }
    }

    return mergedSegments;
}

LoopGraphStats CountProjectedClosedLoops(const std::vector<ProjectedSegment>& segments, double nodeTolerance, double minSegmentLength, int closedCurveLoopCount)
{
    LoopGraphStats stats = {};
    stats.inputSegments = static_cast<int>(segments.size());
    stats.closedLoops = closedCurveLoopCount;

    std::vector<Point2> nodes;
    std::set<std::pair<int, int> > uniqueEdges;
    std::vector<std::vector<int> > adjacency;
    for (std::size_t index = 0; index < segments.size(); ++index)
    {
        if (Distance2(segments[index].first, segments[index].second) < minSegmentLength)
        {
            ++stats.shortSegments;
            continue;
        }

        const int node1 = FindOrAddProjectedNode(nodes, segments[index].first, nodeTolerance);
        const int node2 = FindOrAddProjectedNode(nodes, segments[index].second, nodeTolerance);
        if (node1 == node2)
        {
            ++stats.sameNodeSegments;
            continue;
        }

        const std::pair<int, int> edgeKey = std::make_pair(std::min(node1, node2), std::max(node1, node2));
        if (!uniqueEdges.insert(edgeKey).second)
        {
            ++stats.duplicateSegments;
            continue;
        }
        ++stats.uniqueSegments;

        if (adjacency.size() < nodes.size())
        {
            adjacency.resize(nodes.size());
        }
        adjacency[node1].push_back(node2);
        adjacency[node2].push_back(node1);
    }

    stats.nodes = static_cast<int>(nodes.size());
    std::vector<int> visited(nodes.size(), 0);
    for (std::size_t index = 0; index < nodes.size(); ++index)
    {
        if (visited[index] || adjacency[index].empty())
        {
            continue;
        }

        std::vector<int> stack;
        std::vector<int> component;
        stack.push_back(static_cast<int>(index));
        visited[index] = 1;
        while (!stack.empty())
        {
            const int node = stack.back();
            stack.pop_back();
            component.push_back(node);
            for (std::size_t nextIndex = 0; nextIndex < adjacency[node].size(); ++nextIndex)
            {
                const int next = adjacency[node][nextIndex];
                if (!visited[next])
                {
                    visited[next] = 1;
                    stack.push_back(next);
                }
            }
        }
        ++stats.components;

        bool closed = component.size() >= 4;
        if (component.size() < 4)
        {
            ++stats.smallComponents;
        }

        int componentEdgeDegreeSum = 0;
        bool hasDeadEnd = false;
        for (std::size_t componentIndex = 0; componentIndex < component.size(); ++componentIndex)
        {
            const std::size_t degree = adjacency[component[componentIndex]].size();
            componentEdgeDegreeSum += static_cast<int>(degree);
            if (degree < 2)
            {
                hasDeadEnd = true;
            }
        }
        if (hasDeadEnd)
        {
            ++stats.deadEndComponents;
        }

        const int componentEdgeCount = componentEdgeDegreeSum / 2;
        const int componentCycleRank = componentEdgeCount - static_cast<int>(component.size()) + 1;
        stats.maxComponentCycleRank = std::max(stats.maxComponentCycleRank, componentCycleRank);
        closed = closed && !hasDeadEnd && componentCycleRank > 0;
        if (closed)
        {
            stats.closedLoops += componentCycleRank;
        }
    }

    return stats;
}

bool FindLengthAxisFromEdges(tag_t bodyTag, double expectedLength, double lengthAxis[3])
{
    uf_list_p_t edgeList = NULL;
    int rc = UF_MODL_ask_body_edges(bodyTag, &edgeList);
    if (rc != 0)
    {
        throw NXOpen::NXException::Create(rc);
    }

    std::vector<tag_t> edgeTags = UfListToTags(edgeList);
    UF_MODL_delete_list(&edgeList);

    std::vector<DirectionCluster> clusters;
    const double parallelToleranceCos = std::cos(5.0 * 3.14159265358979323846 / 180.0);
    const double lengthTolerance = std::max(1.0, expectedLength * 0.03);
    for (std::size_t index = 0; index < edgeTags.size(); ++index)
    {
        double point1[3] = {0.0, 0.0, 0.0};
        double point2[3] = {0.0, 0.0, 0.0};
        int vertexCount = 0;
        rc = UF_MODL_ask_edge_verts(edgeTags[index], point1, point2, &vertexCount);
        if (rc != 0)
        {
            throw NXOpen::NXException::Create(rc);
        }
        if (vertexCount != 2)
        {
            continue;
        }

        const double edgeLength = Distance3(point1, point2);
        if (edgeLength < 0.5 || std::fabs(edgeLength - expectedLength) > lengthTolerance)
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
    double bestScore = -1.0;
    for (std::size_t index = 0; index < clusters.size(); ++index)
    {
        const double score = clusters[index].totalLength + static_cast<double>(clusters[index].count) * expectedLength;
        if (score > bestScore)
        {
            bestScore = score;
            bestIndex = index;
        }
    }

    lengthAxis[0] = clusters[bestIndex].direction[0];
    lengthAxis[1] = clusters[bestIndex].direction[1];
    lengthAxis[2] = clusters[bestIndex].direction[2];
    return Normalize3(lengthAxis);
}

bool AskBodyLengthRangeFromEdges(tag_t bodyTag, const double lengthAxis[3], double& minCoord, double& maxCoord)
{
    minCoord = DBL_MAX;
    maxCoord = -DBL_MAX;

    uf_list_p_t edgeList = NULL;
    int rc = UF_MODL_ask_body_edges(bodyTag, &edgeList);
    if (rc != 0)
    {
        throw NXOpen::NXException::Create(rc);
    }

    std::vector<tag_t> edgeTags = UfListToTags(edgeList);
    UF_MODL_delete_list(&edgeList);
    for (std::size_t index = 0; index < edgeTags.size(); ++index)
    {
        double point1[3] = {0.0, 0.0, 0.0};
        double point2[3] = {0.0, 0.0, 0.0};
        int vertexCount = 0;
        rc = UF_MODL_ask_edge_verts(edgeTags[index], point1, point2, &vertexCount);
        if (rc != 0)
        {
            throw NXOpen::NXException::Create(rc);
        }
        if (vertexCount != 2)
        {
            continue;
        }

        const double coord1 = Dot3(point1, lengthAxis);
        const double coord2 = Dot3(point2, lengthAxis);
        minCoord = std::min(minCoord, std::min(coord1, coord2));
        maxCoord = std::max(maxCoord, std::max(coord1, coord2));
    }

    return minCoord < maxCoord && minCoord < DBL_MAX && maxCoord > -DBL_MAX;
}

bool AskCurveEndpointSegment2D(tag_t curveTag, const double sectionU[3], const double sectionV[3], ProjectedSegment& segment, bool& isClosedCurve)
{
    isClosedCurve = false;
    int periodicity = UF_MODL_OPEN_CURVE;
    if (UF_MODL_ask_curve_periodicity(curveTag, &periodicity) == 0 &&
        periodicity != UF_MODL_OPEN_CURVE)
    {
        isClosedCurve = true;
        return true;
    }

    double point1[3] = {0.0, 0.0, 0.0};
    double tangent[3] = {0.0, 0.0, 0.0};
    double principalNormal[3] = {0.0, 0.0, 0.0};
    double binormal[3] = {0.0, 0.0, 0.0};
    double torsion = 0.0;
    double radiusOfCurvature = 0.0;
    int rc = UF_MODL_ask_curve_props(curveTag, 0.0, point1, tangent, principalNormal, binormal, &torsion, &radiusOfCurvature);
    if (rc != 0)
    {
        return false;
    }

    double point2[3] = {0.0, 0.0, 0.0};
    rc = UF_MODL_ask_curve_props(curveTag, 1.0, point2, tangent, principalNormal, binormal, &torsion, &radiusOfCurvature);
    if (rc != 0)
    {
        return false;
    }

    segment.first.x = Dot3(point1, sectionU);
    segment.first.y = Dot3(point1, sectionV);
    segment.second.x = Dot3(point2, sectionU);
    segment.second.y = Dot3(point2, sectionV);
    if (Distance2(segment.first, segment.second) < 0.25)
    {
        isClosedCurve = true;
    }
    return true;
}

bool HasClosedHollowAtSection(
    tag_t bodyTag,
    const double planePoint[3],
    const double lengthAxis[3],
    const double sectionU[3],
    const double sectionV[3],
    int& closedLoops,
    int& curveCount,
    LoopGraphStats& graphStats)
{
    closedLoops = 0;
    curveCount = 0;
    graphStats = {};

    tag_t planeTag = NULL_TAG;
    int rc = UF_MODL_create_plane(const_cast<double*>(planePoint), const_cast<double*>(lengthAxis), &planeTag);
    if (rc != 0 || planeTag == NULL_TAG)
    {
        return false;
    }

    int intersectionCount = 0;
    UF_MODL_intersect_info_p_t* intersections = NULL;
    rc = UF_MODL_intersect_objects(bodyTag, planeTag, 0.001, &intersectionCount, &intersections);
    UF_OBJ_delete_object(planeTag);
    if (rc != 0)
    {
        if (intersections != NULL)
        {
            UF_free(intersections);
        }
        return false;
    }

    std::vector<tag_t> createdCurveTags;
    std::vector<ProjectedSegment> sectionSegments;
    int closedCurveLoopCount = 0;
    for (int index = 0; index < intersectionCount; ++index)
    {
        UF_MODL_intersect_info_p_t info = intersections[index];
        if (info == NULL)
        {
            continue;
        }

        if (info->intersect_type == UF_MODL_INTERSECT_CURVE)
        {
            const tag_t curveTag = info->intersect.curve.identifier;
            if (curveTag != NULL_TAG)
            {
                createdCurveTags.push_back(curveTag);
                ++curveCount;
                ProjectedSegment segment = {};
                bool isClosedCurve = false;
                if (AskCurveEndpointSegment2D(curveTag, sectionU, sectionV, segment, isClosedCurve))
                {
                    if (isClosedCurve)
                    {
                        ++closedCurveLoopCount;
                    }
                    else
                    {
                        sectionSegments.push_back(segment);
                    }
                }
            }
        }

        UF_free(info);
    }
    if (intersections != NULL)
    {
        UF_free(intersections);
    }

    std::vector<ProjectedSegment> mergedSegments = MergeCollinearProjectedSegments(sectionSegments, 0.5, 0.5);
    graphStats = CountProjectedClosedLoops(mergedSegments, 0.5, 0.25, closedCurveLoopCount);
    closedLoops = graphStats.closedLoops;

    for (std::size_t index = 0; index < createdCurveTags.size(); ++index)
    {
        UF_OBJ_delete_object(createdCurveTags[index]);
    }

    return curveCount > 0;
}

bool HasClosedHollowSampledSections(tag_t bodyTag, double expectedLength, const double lengthAxis[3], const double sectionU[3], const double sectionV[3], std::string* status)
{
    DisplaySuppressionGuard displayGuard;

    double minCoord = 0.0;
    double maxCoord = 0.0;
    if (!AskBodyLengthRangeFromEdges(bodyTag, lengthAxis, minCoord, maxCoord))
    {
        if (status != NULL)
        {
            *status = "sampleSectionStatus=lengthRangeMissing";
        }
        return false;
    }

    const double bodyLength = maxCoord - minCoord;
    if (bodyLength < std::max(5.0, expectedLength * 0.5))
    {
        if (status != NULL)
        {
            std::ostringstream stream;
            stream << "sampleSectionStatus=lengthRangeTooSmall"
                << " bodyLength=" << FormatDouble(bodyLength)
                << " expectedLength=" << FormatDouble(expectedLength);
            *status = stream.str();
        }
        return false;
    }

    const double sampleFractions[] = {0.12, 0.20, 0.32, 0.44, 0.56, 0.68, 0.80, 0.88};
    const int sampleCount = static_cast<int>(sizeof(sampleFractions) / sizeof(sampleFractions[0]));
    int attemptedSections = 0;
    int validSections = 0;
    int bestClosedLoops = 0;
    int bestCurveCount = 0;
    LoopGraphStats bestStats = {};
    double bestFraction = 0.0;
    std::ostringstream sampleSummary;

    for (int index = 0; index < sampleCount; ++index)
    {
        const double fraction = sampleFractions[index];
        const double coord = minCoord + bodyLength * fraction;
        double planePoint[3] =
        {
            lengthAxis[0] * coord,
            lengthAxis[1] * coord,
            lengthAxis[2] * coord
        };

        int closedLoops = 0;
        int curveCount = 0;
        LoopGraphStats stats = {};
        ++attemptedSections;
        const bool hasSection = HasClosedHollowAtSection(
            bodyTag,
            planePoint,
            lengthAxis,
            sectionU,
            sectionV,
            closedLoops,
            curveCount,
            stats);
        if (!hasSection)
        {
            sampleSummary << " f" << index << "=noSection";
            continue;
        }

        ++validSections;
        sampleSummary << " f" << index
            << "@" << FormatDouble(fraction)
            << ":curves=" << curveCount
            << ",loops=" << closedLoops
            << ",nodes=" << stats.nodes
            << ",edges=" << stats.uniqueSegments;

        if (closedLoops > bestClosedLoops)
        {
            bestClosedLoops = closedLoops;
            bestCurveCount = curveCount;
            bestStats = stats;
            bestFraction = fraction;
        }
        if (closedLoops >= 2)
        {
            if (status != NULL)
            {
                std::ostringstream stream;
                stream << "sampleSectionStatus=closedHollow"
                    << " samplesAttempted=" << attemptedSections
                    << " validSections=" << validSections
                    << " acceptedFraction=" << FormatDouble(fraction)
                    << " closedLoops=" << closedLoops
                    << " curveCount=" << curveCount
                    << " graphNodes=" << stats.nodes
                    << " graphEdges=" << stats.uniqueSegments
                    << " maxComponentCycleRank=" << stats.maxComponentCycleRank
                    << sampleSummary.str();
                *status = stream.str();
            }
            return true;
        }
    }

    if (status != NULL)
    {
        std::ostringstream stream;
        stream << "sampleSectionStatus=closedLoopCountTooSmall"
            << " samplesAttempted=" << attemptedSections
            << " validSections=" << validSections
            << " bestFraction=" << FormatDouble(bestFraction)
            << " bestClosedLoops=" << bestClosedLoops
            << " bestCurveCount=" << bestCurveCount
            << " bestGraphNodes=" << bestStats.nodes
            << " bestGraphEdges=" << bestStats.uniqueSegments
            << " bestMaxComponentCycleRank=" << bestStats.maxComponentCycleRank
            << sampleSummary.str();
        *status = stream.str();
    }
    return false;
}

bool HasClosedHollowSampledSectionOnly(tag_t bodyTag, double expectedLength, std::string* status)
{
    double lengthAxis[3] = {0.0, 0.0, 0.0};
    if (!FindLengthAxisFromEdges(bodyTag, expectedLength, lengthAxis))
    {
        if (status != NULL)
        {
            std::ostringstream stream;
            stream << "sampleSectionStatus=lengthAxisMissing"
                << " expectedLength=" << FormatDouble(expectedLength);
            *status = stream.str();
        }
        return false;
    }

    double reference[3] = {0.0, 0.0, 1.0};
    if (std::fabs(Dot3(lengthAxis, reference)) > 0.9)
    {
        reference[0] = 1.0;
        reference[1] = 0.0;
        reference[2] = 0.0;
    }

    double sectionU[3] = {0.0, 0.0, 0.0};
    double sectionV[3] = {0.0, 0.0, 0.0};
    Cross3(lengthAxis, reference, sectionU);
    if (!Normalize3(sectionU))
    {
        if (status != NULL)
        {
            std::ostringstream stream;
            stream << "sampleSectionStatus=sectionUFailed"
                << " lengthAxis=" << FormatVector3(lengthAxis);
            *status = stream.str();
        }
        return false;
    }

    Cross3(lengthAxis, sectionU, sectionV);
    if (!Normalize3(sectionV))
    {
        if (status != NULL)
        {
            std::ostringstream stream;
            stream << "sampleSectionStatus=sectionVFailed"
                << " lengthAxis=" << FormatVector3(lengthAxis)
                << " sectionU=" << FormatVector3(sectionU);
            *status = stream.str();
        }
        return false;
    }

    return HasClosedHollowSampledSections(bodyTag, expectedLength, lengthAxis, sectionU, sectionV, status);
}

bool HasClosedHollowSectionProjection(tag_t bodyTag, double expectedLength, std::string* status)
{
    double lengthAxis[3] = {0.0, 0.0, 0.0};
    if (!FindLengthAxisFromEdges(bodyTag, expectedLength, lengthAxis))
    {
        if (status != NULL)
        {
            std::ostringstream stream;
            stream << "projectionStatus=lengthAxisMissing"
                << " expectedLength=" << FormatDouble(expectedLength);
            *status = stream.str();
        }
        return false;
    }

    double reference[3] = {0.0, 0.0, 1.0};
    if (std::fabs(Dot3(lengthAxis, reference)) > 0.9)
    {
        reference[0] = 1.0;
        reference[1] = 0.0;
        reference[2] = 0.0;
    }

    double sectionU[3] = {0.0, 0.0, 0.0};
    double sectionV[3] = {0.0, 0.0, 0.0};
    Cross3(lengthAxis, reference, sectionU);
    if (!Normalize3(sectionU))
    {
        if (status != NULL)
        {
            std::ostringstream stream;
            stream << "projectionStatus=sectionUFailed"
                << " lengthAxis=" << FormatVector3(lengthAxis);
            *status = stream.str();
        }
        return false;
    }
    Cross3(lengthAxis, sectionU, sectionV);
    if (!Normalize3(sectionV))
    {
        if (status != NULL)
        {
            std::ostringstream stream;
            stream << "projectionStatus=sectionVFailed"
                << " lengthAxis=" << FormatVector3(lengthAxis)
                << " sectionU=" << FormatVector3(sectionU);
            *status = stream.str();
        }
        return false;
    }

    std::string sampledSectionStatus;
    if (HasClosedHollowSampledSections(bodyTag, expectedLength, lengthAxis, sectionU, sectionV, &sampledSectionStatus))
    {
        if (status != NULL)
        {
            std::ostringstream stream;
            stream << "projectionStatus=sampledSectionClosedHollow"
                << " expectedLength=" << FormatDouble(expectedLength)
                << " lengthAxis=" << FormatVector3(lengthAxis)
                << " sectionU=" << FormatVector3(sectionU)
                << " sectionV=" << FormatVector3(sectionV)
                << " " << sampledSectionStatus;
            *status = stream.str();
        }
        return true;
    }

    uf_list_p_t edgeList = NULL;
    int rc = UF_MODL_ask_body_edges(bodyTag, &edgeList);
    if (rc != 0)
    {
        throw NXOpen::NXException::Create(rc);
    }

    std::vector<tag_t> edgeTags = UfListToTags(edgeList);
    UF_MODL_delete_list(&edgeList);

    std::vector<Point2> nodes;
    std::set<std::pair<int, int> > uniqueEdges;
    std::vector<std::vector<int> > adjacency;
    std::vector<ProjectedSegment> rawProjectedSegments;
    const double nodeTolerance = 0.5;
    const double minProjectedSegment = 0.25;
    int vertexEdgeCount = 0;
    int shortEdgeCount = 0;
    int lengthDirectionEdgeCount = 0;
    int shortProjectedEdgeCount = 0;
    int sameProjectedNodeEdgeCount = 0;
    int duplicateProjectedEdgeCount = 0;
    int projectedEdgeCount = 0;
    int rawProjectedEdgeCount = 0;
    for (std::size_t index = 0; index < edgeTags.size(); ++index)
    {
        double point1[3] = {0.0, 0.0, 0.0};
        double point2[3] = {0.0, 0.0, 0.0};
        int vertexCount = 0;
        rc = UF_MODL_ask_edge_verts(edgeTags[index], point1, point2, &vertexCount);
        if (rc != 0)
        {
            throw NXOpen::NXException::Create(rc);
        }
        if (vertexCount != 2)
        {
            continue;
        }
        ++vertexEdgeCount;

        const double edgeLength = Distance3(point1, point2);
        if (edgeLength < 0.2)
        {
            ++shortEdgeCount;
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

        if (std::fabs(Dot3(direction, lengthAxis)) > 0.90)
        {
            ++lengthDirectionEdgeCount;
            continue;
        }

        Point2 projected1 = {Dot3(point1, sectionU), Dot3(point1, sectionV)};
        Point2 projected2 = {Dot3(point2, sectionU), Dot3(point2, sectionV)};
        const double dx = projected2.x - projected1.x;
        const double dy = projected2.y - projected1.y;
        if (std::sqrt(dx * dx + dy * dy) < minProjectedSegment)
        {
            ++shortProjectedEdgeCount;
            continue;
        }

        ProjectedSegment segment = {};
        segment.first = projected1;
        segment.second = projected2;
        rawProjectedSegments.push_back(segment);
        ++rawProjectedEdgeCount;
    }

    std::vector<ProjectedSegment> mergedProjectedSegments = MergeCollinearProjectedSegments(rawProjectedSegments, nodeTolerance, nodeTolerance);
    for (std::size_t index = 0; index < mergedProjectedSegments.size(); ++index)
    {
        if (Distance2(mergedProjectedSegments[index].first, mergedProjectedSegments[index].second) < minProjectedSegment)
        {
            ++shortProjectedEdgeCount;
            continue;
        }

        const int node1 = FindOrAddProjectedNode(nodes, mergedProjectedSegments[index].first, nodeTolerance);
        const int node2 = FindOrAddProjectedNode(nodes, mergedProjectedSegments[index].second, nodeTolerance);
        if (node1 == node2)
        {
            ++sameProjectedNodeEdgeCount;
            continue;
        }

        const std::pair<int, int> edgeKey = std::make_pair(std::min(node1, node2), std::max(node1, node2));
        if (!uniqueEdges.insert(edgeKey).second)
        {
            ++duplicateProjectedEdgeCount;
            continue;
        }
        ++projectedEdgeCount;

        if (adjacency.size() < nodes.size())
        {
            adjacency.resize(nodes.size());
        }
        adjacency[node1].push_back(node2);
        adjacency[node2].push_back(node1);
    }

    int closedLoops = 0;
    int componentCount = 0;
    int smallComponentCount = 0;
    int deadEndComponentCount = 0;
    int maxComponentCycleRank = 0;
    std::vector<int> visited(nodes.size(), 0);
    for (std::size_t index = 0; index < nodes.size(); ++index)
    {
        if (visited[index] || adjacency[index].empty())
        {
            continue;
        }

        std::vector<int> stack;
        std::vector<int> component;
        stack.push_back(static_cast<int>(index));
        visited[index] = 1;
        while (!stack.empty())
        {
            const int node = stack.back();
            stack.pop_back();
            component.push_back(node);
            for (std::size_t nextIndex = 0; nextIndex < adjacency[node].size(); ++nextIndex)
            {
                const int next = adjacency[node][nextIndex];
                if (!visited[next])
                {
                    visited[next] = 1;
                    stack.push_back(next);
                }
            }
        }
        ++componentCount;

        bool closed = component.size() >= 4;
        if (component.size() < 4)
        {
            ++smallComponentCount;
        }
        int componentEdgeDegreeSum = 0;
        bool hasDeadEnd = false;
        for (std::size_t componentIndex = 0; componentIndex < component.size(); ++componentIndex)
        {
            const std::size_t degree = adjacency[component[componentIndex]].size();
            componentEdgeDegreeSum += static_cast<int>(degree);
            if (degree < 2)
            {
                hasDeadEnd = true;
            }
        }
        if (hasDeadEnd)
        {
            ++deadEndComponentCount;
        }
        const int componentEdgeCount = componentEdgeDegreeSum / 2;
        const int componentCycleRank = componentEdgeCount - static_cast<int>(component.size()) + 1;
        maxComponentCycleRank = std::max(maxComponentCycleRank, componentCycleRank);
        closed = closed && !hasDeadEnd && componentCycleRank > 0;
        if (closed)
        {
            closedLoops += componentCycleRank;
        }
    }

    const bool accepted = closedLoops >= 2;
    if (status != NULL)
    {
        std::ostringstream stream;
        stream << "projectionStatus=" << (accepted ? "closedHollow" : "closedLoopCountTooSmall")
            << " expectedLength=" << FormatDouble(expectedLength)
            << " lengthAxis=" << FormatVector3(lengthAxis)
            << " sectionU=" << FormatVector3(sectionU)
            << " sectionV=" << FormatVector3(sectionV)
            << " totalEdges=" << edgeTags.size()
            << " vertexEdges=" << vertexEdgeCount
            << " shortEdges=" << shortEdgeCount
            << " lengthDirectionEdges=" << lengthDirectionEdgeCount
            << " shortProjectedEdges=" << shortProjectedEdgeCount
            << " sameProjectedNodeEdges=" << sameProjectedNodeEdgeCount
            << " duplicateProjectedEdges=" << duplicateProjectedEdgeCount
            << " rawProjectedEdges=" << rawProjectedEdgeCount
            << " mergedProjectedEdges=" << mergedProjectedSegments.size()
            << " projectedEdges=" << projectedEdgeCount
            << " projectedNodes=" << nodes.size()
            << " uniqueProjectedEdges=" << uniqueEdges.size()
            << " components=" << componentCount
            << " smallComponents=" << smallComponentCount
            << " deadEndComponents=" << deadEndComponentCount
            << " maxComponentCycleRank=" << maxComponentCycleRank
            << " closedLoops=" << closedLoops
            << " requiredClosedLoops=2"
            << " " << sampledSectionStatus;
        *status = stream.str();
    }
    return accepted;
}
bool HasTubeLikeEndFaces(tag_t bodyTag, double expectedLength)
{
    double lengthAxis[3] = {0.0, 0.0, 0.0};
    if (!FindLengthAxisFromEdges(bodyTag, expectedLength, lengthAxis))
    {
        return false;
    }

    uf_list_p_t faceList = NULL;
    int rc = UF_MODL_ask_body_faces(bodyTag, &faceList);
    if (rc != 0)
    {
        throw NXOpen::NXException::Create(rc);
    }

    std::vector<tag_t> faceTags = UfListToTags(faceList);
    UF_MODL_delete_list(&faceList);

    int tubeLikeEndFaceCount = 0;
    for (std::size_t index = 0; index < faceTags.size(); ++index)
    {
        int faceType = 0;
        double point[3] = {0.0, 0.0, 0.0};
        double dir[3] = {0.0, 0.0, 0.0};
        double box[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
        double radius = 0.0;
        double radData = 0.0;
        int normDir = 0;
        rc = UF_MODL_ask_face_data(faceTags[index], &faceType, point, dir, box, &radius, &radData, &normDir);
        if (rc != 0)
        {
            throw NXOpen::NXException::Create(rc);
        }

        if (faceType != 22 || std::fabs(Dot3(dir, lengthAxis)) < 0.85)
        {
            continue;
        }

        uf_loop_p_t loopList = NULL;
        rc = UF_MODL_ask_face_loops(faceTags[index], &loopList);
        if (rc != 0)
        {
            throw NXOpen::NXException::Create(rc);
        }
        if (loopList == NULL)
        {
            continue;
        }

        int loopCount = 0;
        rc = UF_MODL_ask_loop_list_count(loopList, &loopCount);
        if (rc != 0)
        {
            UF_MODL_delete_loop_list(&loopList);
            throw NXOpen::NXException::Create(rc);
        }

        bool hasPeripheralLoop = false;
        bool hasHoleLoop = false;
        for (int loopIndex = 0; loopIndex < loopCount; ++loopIndex)
        {
            int loopType = 0;
            uf_list_p_t edgeLoop = NULL;
            rc = UF_MODL_ask_loop_list_item(loopList, loopIndex, &loopType, &edgeLoop);
            if (rc != 0)
            {
                UF_MODL_delete_loop_list(&loopList);
                throw NXOpen::NXException::Create(rc);
            }

            int edgeCount = 0;
            if (edgeLoop != NULL)
            {
                UF_MODL_ask_list_count(edgeLoop, &edgeCount);
            }

            if (loopType == 1 && edgeCount >= 4)
            {
                hasPeripheralLoop = true;
            }
            else if (loopType == 2 && edgeCount >= 4)
            {
                hasHoleLoop = true;
            }
        }

        UF_MODL_delete_loop_list(&loopList);

        if (hasPeripheralLoop && hasHoleLoop)
        {
            ++tubeLikeEndFaceCount;
        }
    }

    return tubeLikeEndFaceCount >= 1;
}

bool EstimateTubeDimensionsFromEdges(tag_t bodyTag, double& length, double& width, double& height)
{
    length = 0.0;
    width = 0.0;
    height = 0.0;

    uf_list_p_t edgeList = NULL;
    int rc = UF_MODL_ask_body_edges(bodyTag, &edgeList);
    if (rc != 0)
    {
        throw NXOpen::NXException::Create(rc);
    }

    std::vector<tag_t> edgeTags = UfListToTags(edgeList);
    UF_MODL_delete_list(&edgeList);

    std::vector<DirectionCluster> clusters;
    std::vector<EdgeEndpointPair> endpointPairs;
    const double parallelToleranceCos = std::cos(5.0 * 3.14159265358979323846 / 180.0);
    for (std::size_t index = 0; index < edgeTags.size(); ++index)
    {
        double point1[3] = {0.0, 0.0, 0.0};
        double point2[3] = {0.0, 0.0, 0.0};
        int vertexCount = 0;
        rc = UF_MODL_ask_edge_verts(edgeTags[index], point1, point2, &vertexCount);
        if (rc != 0)
        {
            throw NXOpen::NXException::Create(rc);
        }

        if (vertexCount != 2)
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

SquareTubeCandidate AnalyzeBody(NXOpen::Body* body)
{
    SquareTubeCandidate candidate = {};
    candidate.body = body;
    candidate.accepted = false;

    if (body == NULL)
    {
        candidate.reason = "empty body";
        return candidate;
    }

    if (!EstimateTubeDimensionsFromEdges(body->Tag(), candidate.length, candidate.width, candidate.height))
    {
        double box[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
        int rc = UF_MODL_ask_bounding_box(body->Tag(), box);
        if (rc != 0)
        {
            throw NXOpen::NXException::Create(rc);
        }

        double dims[3] =
        {
            std::fabs(box[3] - box[0]),
            std::fabs(box[4] - box[1]),
            std::fabs(box[5] - box[2])
        };
        std::sort(dims, dims + 3);
        candidate.height = dims[0];
        candidate.width = dims[1];
        candidate.length = dims[2];
    }
    candidate.planarFaceCount = CountPlanarFaces(body->Tag());
    const bool specMatched = ReclassifyTubeDimensionsBySectionStep(candidate.length, candidate.width, candidate.height);

    if (candidate.height <= 1.0e-6 || candidate.width <= 1.0e-6 || candidate.length <= 1.0e-6)
    {
        candidate.reason = "invalid bounding box";
        return candidate;
    }

    if (!specMatched)
    {
        candidate.reason = "section is not in spec table";
        return candidate;
    }

    std::string sampledSectionStatus;
    if (!HasClosedHollowSampledSectionOnly(body->Tag(), candidate.length, &sampledSectionStatus))
    {
        candidate.reason = "section is not closed hollow tube; " + sampledSectionStatus;
        return candidate;
    }

    candidate.accepted = true;
    candidate.reason = std::fabs(candidate.width - candidate.height) <= 1.0
        ? "square tube candidate"
        : "flat tube candidate";
    return candidate;
}

SquareTubeCandidate AnalyzeBodyCached(
    NXOpen::Body* body,
    std::map<tag_t, SquareTubeCandidate>* candidateCache)
{
    if (body == NULL || candidateCache == NULL)
    {
        return AnalyzeBody(body);
    }

    const tag_t bodyTag = body->Tag();
    std::map<tag_t, SquareTubeCandidate>::const_iterator found = candidateCache->find(bodyTag);
    if (found != candidateCache->end())
    {
        return found->second;
    }

    SquareTubeCandidate candidate = AnalyzeBody(body);
    (*candidateCache)[bodyTag] = candidate;
    return candidate;
}

std::vector<tag_t> AskSolidBodiesInDisplayPart()
{
    std::vector<tag_t> bodies;
    const tag_t partTag = UF_PART_ask_display_part();
    if (partTag == NULL_TAG)
    {
        return bodies;
    }

    tag_t objectTag = NULL_TAG;
    while (UF_OBJ_cycle_objs_in_part(partTag, UF_solid_type, &objectTag) == 0 && objectTag != NULL_TAG)
    {
        int bodyType = 0;
        if (UF_MODL_ask_body_type(objectTag, &bodyType) == 0 && bodyType == UF_MODL_SOLID_BODY)
        {
            bodies.push_back(objectTag);
        }
    }
    return bodies;
}

bool AskPlanarFacePlane(tag_t faceTag, double point[3], double normal[3])
{
    int faceType = 0;
    double box[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    double radius = 0.0;
    double radData = 0.0;
    int normalDir = 0;
    int rc = UF_MODL_ask_face_data(faceTag, &faceType, point, normal, box, &radius, &radData, &normalDir);
    if (rc != 0 || faceType != UF_MODL_PLANAR_FACE)
    {
        return false;
    }
    std::vector<EdgeEndpointPair> endpointPairs;
    if (AskFaceEdgeEndpointPairs(faceTag, endpointPairs))
    {
        double center[3] = {0.0, 0.0, 0.0};
        if (ComputeFaceCenterFromEndpoints(endpointPairs, center))
        {
            point[0] = center[0];
            point[1] = center[1];
            point[2] = center[2];
        }
    }
    return AskFaceNormalFromDirectionCollection(faceTag, normal);
}

bool IsCoplanarFace(const FacePlacement& referencePlacement, tag_t faceTag)
{
    double point[3] = {0.0, 0.0, 0.0};
    double normal[3] = {0.0, 0.0, 0.0};
    if (!AskPlanarFacePlane(faceTag, point, normal))
    {
        return false;
    }

    const double normalCosTolerance = 0.999;
    if (std::fabs(Dot3(referencePlacement.normal, normal)) < normalCosTolerance)
    {
        return false;
    }

    double delta[3] =
    {
        point[0] - referencePlacement.center[0],
        point[1] - referencePlacement.center[1],
        point[2] - referencePlacement.center[2]
    };
    const double planeDistanceTolerance = 0.25;
    return std::fabs(Dot3(delta, referencePlacement.normal)) <= planeDistanceTolerance;
}

struct ProjectionRange
{
    double minValue;
    double maxValue;
};

bool AddProjectionPointToRange(const double point[3], const double axis[3], ProjectionRange& range, bool& hasPoint)
{
    const double projection = Dot3(point, axis);
    if (!hasPoint)
    {
        range.minValue = projection;
        range.maxValue = projection;
        hasPoint = true;
    }
    else
    {
        range.minValue = std::min(range.minValue, projection);
        range.maxValue = std::max(range.maxValue, projection);
    }
    return true;
}

bool AskFaceProjectionRange(tag_t faceTag, const double axis[3], ProjectionRange& range)
{
    uf_list_p_t edgeList = NULL;
    if (UF_MODL_ask_face_edges(faceTag, &edgeList) != 0 || edgeList == NULL)
    {
        return false;
    }

    std::vector<tag_t> edgeTags = UfListToTags(edgeList);
    UF_MODL_delete_list(&edgeList);

    bool hasPoint = false;
    for (std::size_t index = 0; index < edgeTags.size(); ++index)
    {
        double point1[3] = {0.0, 0.0, 0.0};
        double point2[3] = {0.0, 0.0, 0.0};
        int vertexCount = 0;
        if (UF_MODL_ask_edge_verts(edgeTags[index], point1, point2, &vertexCount) != 0 || vertexCount != 2)
        {
            continue;
        }

        AddProjectionPointToRange(point1, axis, range, hasPoint);
        AddProjectionPointToRange(point2, axis, range, hasPoint);
    }
    return hasPoint;
}

bool AskBodyProjectionRange(tag_t bodyTag, const double axis[3], ProjectionRange& range)
{
    uf_list_p_t edgeList = NULL;
    if (UF_MODL_ask_body_edges(bodyTag, &edgeList) != 0 || edgeList == NULL)
    {
        return false;
    }

    std::vector<tag_t> edgeTags = UfListToTags(edgeList);
    UF_MODL_delete_list(&edgeList);

    bool hasPoint = false;
    for (std::size_t index = 0; index < edgeTags.size(); ++index)
    {
        double point1[3] = {0.0, 0.0, 0.0};
        double point2[3] = {0.0, 0.0, 0.0};
        int vertexCount = 0;
        if (UF_MODL_ask_edge_verts(edgeTags[index], point1, point2, &vertexCount) != 0 || vertexCount != 2)
        {
            continue;
        }

        AddProjectionPointToRange(point1, axis, range, hasPoint);
        AddProjectionPointToRange(point2, axis, range, hasPoint);
    }
    return hasPoint;
}

double RangeOverlap(const ProjectionRange& first, const ProjectionRange& second)
{
    return std::min(first.maxValue, second.maxValue) - std::max(first.minValue, second.minValue);
}

bool FindTouchingTubeAtEnd(
    const FacePlacement& endPlacement,
    const std::vector<tag_t>& candidateBodyTags,
    tag_t* touchingBodyTag)
{
    if (touchingBodyTag != NULL)
    {
        *touchingBodyTag = NULL_TAG;
    }

    ProjectionRange currentWidthRange = {endPlacement.widthMin, endPlacement.widthMax};
    ProjectionRange currentNormalRange = {};
    if (!AskBodyProjectionRange(endPlacement.bodyTag, endPlacement.normal, currentNormalRange))
    {
        return false;
    }

    const double planeTolerance = std::max(0.35, endPlacement.wallThickness * 0.15);
    const double overlapTolerance = 0.5;
    tag_t bestTouchingBodyTag = NULL_TAG;
    double bestScore = -DBL_MAX;
    tag_t closestPlaneBodyTag = NULL_TAG;
    tag_t closestPlaneFaceTag = NULL_TAG;
    double closestPlaneDistance = DBL_MAX;
    double closestPlaneNormalCos = 0.0;
    double closestWidthOverlap = -DBL_MAX;
    double closestNormalOverlap = -DBL_MAX;
    for (std::size_t tubeIndex = 0; tubeIndex < candidateBodyTags.size(); ++tubeIndex)
    {
        if (candidateBodyTags[tubeIndex] == endPlacement.bodyTag)
        {
            continue;
        }

        uf_list_p_t faceList = NULL;
        if (UF_MODL_ask_body_faces(candidateBodyTags[tubeIndex], &faceList) != 0 || faceList == NULL)
        {
            continue;
        }

        std::vector<tag_t> faceTags = UfListToTags(faceList);
        UF_MODL_delete_list(&faceList);
        for (std::size_t faceIndex = 0; faceIndex < faceTags.size(); ++faceIndex)
        {
            double facePoint[3] = {0.0, 0.0, 0.0};
            double faceNormal[3] = {0.0, 0.0, 0.0};
            if (!AskPlanarFacePlane(faceTags[faceIndex], facePoint, faceNormal))
            {
                continue;
            }

            if (std::fabs(Dot3(faceNormal, endPlacement.lengthAxis)) < 0.98)
            {
                continue;
            }

            const double planeNormalCos = std::fabs(Dot3(faceNormal, endPlacement.lengthAxis));
            const double planeDistance =
                std::fabs(Dot3(facePoint, endPlacement.lengthAxis) - endPlacement.endCoord);
            ProjectionRange otherWidthRange = {};
            ProjectionRange otherNormalRange = {};
            if (!AskFaceProjectionRange(faceTags[faceIndex], endPlacement.widthAxis, otherWidthRange) ||
                !AskFaceProjectionRange(faceTags[faceIndex], endPlacement.normal, otherNormalRange))
            {
                continue;
            }

            const double widthOverlap = RangeOverlap(currentWidthRange, otherWidthRange);
            const double normalOverlap = RangeOverlap(currentNormalRange, otherNormalRange);
            if (planeDistance < closestPlaneDistance)
            {
                closestPlaneDistance = planeDistance;
                closestPlaneNormalCos = planeNormalCos;
                closestWidthOverlap = widthOverlap;
                closestNormalOverlap = normalOverlap;
                closestPlaneBodyTag = candidateBodyTags[tubeIndex];
                closestPlaneFaceTag = faceTags[faceIndex];
            }
            if (planeDistance > planeTolerance)
            {
                continue;
            }

            if (widthOverlap > overlapTolerance && normalOverlap > overlapTolerance)
            {
                const double score = widthOverlap + normalOverlap - planeDistance;
                if (score > bestScore)
                {
                    bestScore = score;
                    bestTouchingBodyTag = candidateBodyTags[tubeIndex];
                }
            }
        }
    }

    if (bestTouchingBodyTag != NULL_TAG && touchingBodyTag != NULL)
    {
        *touchingBodyTag = bestTouchingBodyTag;
    }
    if (bestTouchingBodyTag != NULL_TAG)
    {
        std::ostringstream log;
        log << "TouchingTube"
            << " sourceBody=" << endPlacement.bodyTag
            << " targetBody=" << bestTouchingBodyTag
            << " score=" << FormatDouble(bestScore)
            << " endCoord=" << FormatDouble(endPlacement.endCoord);
        AppendDebugLog(log.str());
    }
    else
    {
        std::ostringstream log;
        log << "NoTouchingTube"
            << " sourceBody=" << endPlacement.bodyTag
            << " endCoord=" << FormatDouble(endPlacement.endCoord)
            << " planeTol=" << FormatDouble(planeTolerance)
            << " widthRange=" << FormatDouble(currentWidthRange.minValue)
            << ".." << FormatDouble(currentWidthRange.maxValue)
            << " normalRange=" << FormatDouble(currentNormalRange.minValue)
            << ".." << FormatDouble(currentNormalRange.maxValue)
            << " closestBody=" << closestPlaneBodyTag
            << " closestFace=" << closestPlaneFaceTag
            << " closestPlaneDist=" << FormatDouble(closestPlaneDistance)
            << " closestPlaneNormalCos=" << FormatDouble(closestPlaneNormalCos)
            << " closestWidthOverlap=" << FormatDouble(closestWidthOverlap)
            << " closestNormalOverlap=" << FormatDouble(closestNormalOverlap);
        AppendDebugLog(log.str());
    }
    return bestTouchingBodyTag != NULL_TAG;
}

std::vector<tag_t> CollectTouchCandidateBodyTags(
    const FacePlacement& referencePlacement,
    std::map<tag_t, SquareTubeCandidate>* candidateCache = NULL)
{
    std::vector<tag_t> candidateBodyTags;
    std::vector<tag_t> bodyTags = AskSolidBodiesInDisplayPart();
    int excludedPerpendicularTubeCount = 0;
    for (std::size_t bodyIndex = 0; bodyIndex < bodyTags.size(); ++bodyIndex)
    {
        bool excludeBody = false;
        NXOpen::Body* body =
            dynamic_cast<NXOpen::Body*>(NXOpen::NXObjectManager::Get(bodyTags[bodyIndex]));
        if (body != NULL)
        {
            try
            {
                SquareTubeCandidate candidate = AnalyzeBodyCached(body, candidateCache);
                const bool hasTubeLikeDimensions =
                    candidate.height > 1.0e-6 &&
                    candidate.width > 1.0e-6 &&
                    candidate.length > 1.0e-6;
                double lengthAxis[3] = {0.0, 0.0, 0.0};
                if (hasTubeLikeDimensions &&
                    FindLengthAxisFromEdges(bodyTags[bodyIndex], candidate.length, lengthAxis))
                {
                    const double lengthNormalDot = std::fabs(Dot3(lengthAxis, referencePlacement.normal));
                    if (lengthNormalDot > 0.85)
                    {
                        excludeBody = true;
                        ++excludedPerpendicularTubeCount;
                        std::ostringstream detailLog;
                        detailLog << "TouchCandidateBodyExcluded"
                                  << " body=" << bodyTags[bodyIndex]
                                  << " reason=length axis aligned with selected face normal"
                                  << " dims(L/W/H)=" << FormatDouble(candidate.length)
                                  << "/" << FormatDouble(candidate.width)
                                  << "/" << FormatDouble(candidate.height)
                                  << " lengthAxis=" << FormatVector3(lengthAxis)
                                  << " lengthNormalDot=" << FormatDouble(lengthNormalDot);
                        AppendDebugLog(detailLog.str());
                    }
                }
            }
            catch (...)
            {
            }
        }

        if (!excludeBody)
        {
            candidateBodyTags.push_back(bodyTags[bodyIndex]);
        }
    }

    std::ostringstream log;
    log << "TouchCandidateBodies count=" << candidateBodyTags.size()
        << " sourceBodies=" << bodyTags.size()
        << " excludedPerpendicularTubes=" << excludedPerpendicularTubeCount;
    AppendDebugLog(log.str());
    return candidateBodyTags;
}

void RedisplayTag(tag_t objectTag)
{
    NXOpen::DisplayableObject* displayObject =
        dynamic_cast<NXOpen::DisplayableObject*>(NXOpen::NXObjectManager::Get(objectTag));
    if (displayObject != NULL)
    {
        displayObject->RedisplayObject();
    }
}

std::vector<FacePlacement> CollectCoplanarTubeFacePlacements(
    const FacePlacement& referencePlacement,
    std::map<tag_t, SquareTubeCandidate>* candidateCache = NULL)
{
    int coplanarFaceCount = 0;
    std::vector<FacePlacement> placements;
    std::vector<tag_t> bodyTags = AskSolidBodiesInDisplayPart();
    {
        std::ostringstream log;
        log << "CoplanarScanBegin"
            << " totalBodies=" << bodyTags.size()
            << " referenceFace=" << referencePlacement.faceTag
            << " referenceBody=" << referencePlacement.bodyTag
            << " referenceCenter=" << FormatVector3(referencePlacement.center)
            << " referenceNormal=" << FormatVector3(referencePlacement.normal);
        AppendDebugLog(log.str());
    }

    for (std::size_t bodyIndex = 0; bodyIndex < bodyTags.size(); ++bodyIndex)
    {
        NXOpen::Body* body =
            dynamic_cast<NXOpen::Body*>(NXOpen::NXObjectManager::Get(bodyTags[bodyIndex]));
        if (body == NULL)
        {
            std::ostringstream log;
            log << "CoplanarBody rejected"
                << " body=" << bodyTags[bodyIndex]
                << " reason=null NXOpen body";
            AppendDebugLog(log.str());
            continue;
        }

        bool acceptedTube = false;
        SquareTubeCandidate candidate = {};
        try
        {
            candidate = AnalyzeBodyCached(body, candidateCache);
            acceptedTube = candidate.accepted;
        }
        catch (...)
        {
            acceptedTube = false;
            candidate.reason = "AnalyzeBody threw";
        }
        if (!acceptedTube)
        {
            std::ostringstream log;
            log << "CoplanarBody rejected"
                << " body=" << bodyTags[bodyIndex]
                << " reason=not accepted tube"
                << " analyzeReason=" << candidate.reason
                << " dims(L/W/H)=" << FormatDouble(candidate.length)
                << "/" << FormatDouble(candidate.width)
                << "/" << FormatDouble(candidate.height)
                << " planarFaces=" << candidate.planarFaceCount;
            AppendDebugLog(log.str());
            continue;
        }

        double tubeLengthAxis[3] = {0.0, 0.0, 0.0};
        const bool hasTubeLengthAxis = FindLengthAxisFromEdges(bodyTags[bodyIndex], candidate.length, tubeLengthAxis);
        const double tubeLengthNormalDot = hasTubeLengthAxis
            ? std::fabs(Dot3(tubeLengthAxis, referencePlacement.normal))
            : -1.0;
        if (hasTubeLengthAxis && tubeLengthNormalDot > 0.85)
        {
            std::ostringstream log;
            log << "CoplanarBody rejected"
                << " body=" << bodyTags[bodyIndex]
                << " reason=length axis aligned with selected face normal"
                << " dims(L/W/H)=" << FormatDouble(candidate.length)
                << "/" << FormatDouble(candidate.width)
                << "/" << FormatDouble(candidate.height)
                << " planarFaces=" << candidate.planarFaceCount
                << " lengthAxis=" << FormatVector3(tubeLengthAxis)
                << " lengthNormalDot=" << FormatDouble(tubeLengthNormalDot);
            AppendDebugLog(log.str());
            continue;
        }

        uf_list_p_t faceList = NULL;
        if (UF_MODL_ask_body_faces(bodyTags[bodyIndex], &faceList) != 0 || faceList == NULL)
        {
            std::ostringstream log;
            log << "CoplanarBody rejected"
                << " body=" << bodyTags[bodyIndex]
                << " reason=failed to ask body faces"
                << " dims(L/W/H)=" << FormatDouble(candidate.length)
                << "/" << FormatDouble(candidate.width)
                << "/" << FormatDouble(candidate.height);
            AppendDebugLog(log.str());
            continue;
        }

        std::vector<tag_t> faceTags = UfListToTags(faceList);
        UF_MODL_delete_list(&faceList);
        bool placementAddedForBody = false;
        int matchedFaceCount = 0;
        int planarFaceScanCount = 0;
        double bestNormalCos = -1.0;
        tag_t bestNormalFace = NULL_TAG;
        double minPlaneDistance = DBL_MAX;
        tag_t minDistanceFace = NULL_TAG;
        tag_t firstMatchedFace = NULL_TAG;
        const double normalCosTolerance = 0.999;
        const double planeDistanceTolerance = 0.25;
        for (std::size_t faceIndex = 0; faceIndex < faceTags.size(); ++faceIndex)
        {
            double facePoint[3] = {0.0, 0.0, 0.0};
            double faceNormal[3] = {0.0, 0.0, 0.0};
            if (!AskPlanarFacePlane(faceTags[faceIndex], facePoint, faceNormal))
            {
                continue;
            }

            ++planarFaceScanCount;
            const double normalCos = std::fabs(Dot3(referencePlacement.normal, faceNormal));
            if (normalCos > bestNormalCos)
            {
                bestNormalCos = normalCos;
                bestNormalFace = faceTags[faceIndex];
            }

            double delta[3] =
            {
                facePoint[0] - referencePlacement.center[0],
                facePoint[1] - referencePlacement.center[1],
                facePoint[2] - referencePlacement.center[2]
            };
            const double planeDistance = std::fabs(Dot3(delta, referencePlacement.normal));
            if (planeDistance < minPlaneDistance)
            {
                minPlaneDistance = planeDistance;
                minDistanceFace = faceTags[faceIndex];
            }

            if (normalCos < normalCosTolerance || planeDistance > planeDistanceTolerance)
            {
                continue;
            }

            ++coplanarFaceCount;
            ++matchedFaceCount;
            if (firstMatchedFace == NULL_TAG)
            {
                firstMatchedFace = faceTags[faceIndex];
            }

            if (!placementAddedForBody)
            {
                NXOpen::Face* face =
                    dynamic_cast<NXOpen::Face*>(NXOpen::NXObjectManager::Get(faceTags[faceIndex]));
                FacePlacement placement = {};
                if (face != NULL &&
                    AskPlanarFacePlacement(face, facePoint, placement))
                {
                    placements.push_back(placement);
                    placementAddedForBody = true;
                }
            }
        }

        std::ostringstream log;
        log << "CoplanarBody " << (placementAddedForBody ? "accepted" : "rejected")
            << " body=" << bodyTags[bodyIndex]
            << " dims(L/W/H)=" << FormatDouble(candidate.length)
            << "/" << FormatDouble(candidate.width)
            << "/" << FormatDouble(candidate.height)
            << " analyzeReason=" << candidate.reason
            << " planarFaces=" << candidate.planarFaceCount
            << " scannedPlanarFaces=" << planarFaceScanCount
            << " matchedFaces=" << matchedFaceCount;
        if (hasTubeLengthAxis)
        {
            log << " lengthAxis=" << FormatVector3(tubeLengthAxis)
                << " lengthNormalDot=" << FormatDouble(tubeLengthNormalDot);
        }
        else
        {
            log << " lengthAxis=missing";
        }
        log << " bestNormalCos=" << FormatDouble(bestNormalCos)
            << " bestNormalFace=" << bestNormalFace;
        if (minPlaneDistance == DBL_MAX)
        {
            log << " minPlaneDistance=NA minDistanceFace=0";
        }
        else
        {
            log << " minPlaneDistance=" << FormatDouble(minPlaneDistance)
                << " minDistanceFace=" << minDistanceFace;
        }
        log << " firstMatchedFace=" << firstMatchedFace;
        if (!placementAddedForBody)
        {
            if (planarFaceScanCount == 0)
            {
                log << " rejectReason=no planar faces";
            }
            else if (bestNormalCos < normalCosTolerance)
            {
                log << " rejectReason=normal not parallel enough";
            }
            else if (minPlaneDistance > planeDistanceTolerance)
            {
                log << " rejectReason=plane distance too large";
            }
            else
            {
                log << " rejectReason=face placement failed";
            }
        }
        AppendDebugLog(log.str());
    }

    std::ostringstream log;
    log << "CoplanarTubeFacesCollected count=" << coplanarFaceCount
        << " placementCount=" << placements.size()
        << " referenceFace=" << referencePlacement.faceTag;
    AppendDebugLog(log.str());
    return placements;
}

int MarkCreatedPortFaces(const std::vector<TouchingPortPlacement>& touchingEndPlacements)
{
    const int greenColor = 36;
    int markedCount = 0;
    std::set<tag_t> markedFaces;
    for (std::size_t index = 0; index < touchingEndPlacements.size(); ++index)
    {
        const tag_t faceTag = touchingEndPlacements[index].malePlacement.faceTag;
        if (faceTag == NULL_TAG || markedFaces.find(faceTag) != markedFaces.end())
        {
            continue;
        }

        if (UF_OBJ_set_color(faceTag, greenColor) == 0)
        {
            RedisplayTag(faceTag);
            markedFaces.insert(faceTag);
            ++markedCount;
        }
    }

    std::ostringstream log;
    log << "CreatedPortFacesMarked count=" << markedCount
        << " touchingEnds=" << touchingEndPlacements.size();
    AppendDebugLog(log.str());
    return markedCount;
}

NXOpen::Body* BodyFromSelectedObject(NXOpen::TaggedObject* object)
{
    if (object == NULL)
    {
        return NULL;
    }

    NXOpen::Body* body = dynamic_cast<NXOpen::Body*>(object);
    if (body != NULL)
    {
        return body;
    }

    NXOpen::Face* face = dynamic_cast<NXOpen::Face*>(object);
    if (face != NULL)
    {
        return dynamic_cast<NXOpen::Body*>(face->GetBody());
    }

    NXOpen::Edge* edge = dynamic_cast<NXOpen::Edge*>(object);
    if (edge != NULL)
    {
        uf_list_p_t faceList = NULL;
        if (UF_MODL_ask_edge_faces(edge->Tag(), &faceList) == 0 && faceList != NULL)
        {
            std::vector<tag_t> faceTags = UfListToTags(faceList);
            UF_MODL_delete_list(&faceList);
            if (!faceTags.empty())
            {
                tag_t bodyTag = NULL_TAG;
                if (UF_MODL_ask_face_body(faceTags.front(), &bodyTag) == 0 && bodyTag != NULL_TAG)
                {
                    return dynamic_cast<NXOpen::Body*>(NXOpen::NXObjectManager::Get(bodyTag));
                }
            }
        }
    }

    return NULL;
}

std::vector<NXOpen::Body*> BodiesFromSelectedObjects(const std::vector<NXOpen::TaggedObject*>& selectedObjects)
{
    std::vector<NXOpen::Body*> bodies;
    std::set<tag_t> bodyTags;
    for (std::size_t index = 0; index < selectedObjects.size(); ++index)
    {
        NXOpen::Body* body = BodyFromSelectedObject(selectedObjects[index]);
        if (body != NULL && bodyTags.insert(body->Tag()).second)
        {
            bodies.push_back(body);
        }
    }
    return bodies;
}

void RunFangTongKaKou(const std::vector<NXOpen::Body*>& bodies)
{
    NXOpen::Session* session = NXOpen::Session::GetSession();
    NXOpen::UI* ui = NXOpen::UI::GetUI();
    if (bodies.empty())
    {
        ui->NXMessageBox()->Show(
            "FangTongKaKou",
            NXOpen::NXMessageBox::DialogTypeInformation,
            "No body was selected.");
        return;
    }

    std::vector<SquareTubeCandidate> candidates;
    std::map<std::string, TubeStat> stats;
    int acceptedCount = 0;
    std::vector<tag_t> rejectedBodyTags;
    for (std::size_t index = 0; index < bodies.size(); ++index)
    {
        SquareTubeCandidate candidate = AnalyzeBody(bodies[index]);
        if (candidate.accepted)
        {
            ++acceptedCount;
            const std::string spec = BuildSpec(candidate.width, candidate.height);
            const std::string length = BuildLengthKey(candidate.length);
            const std::string key = spec + "|" + length;
            TubeStat& stat = stats[key];
            if (stat.quantity == 0)
            {
                stat.spec = spec;
                stat.length = length;
                stat.totalLength = 0.0;
            }
            ++stat.quantity;
            stat.totalLength += candidate.length;
        }
        else if (candidate.body != NULL)
        {
            rejectedBodyTags.push_back(candidate.body->Tag());
        }
        candidates.push_back(candidate);
    }

    const int redColor = 186;
    for (std::size_t index = 0; index < rejectedBodyTags.size(); ++index)
    {
        int rc = UF_OBJ_set_color(rejectedBodyTags[index], redColor);
        if (rc != 0)
        {
            throw NXOpen::NXException::Create(rc);
        }
    }

    NXOpen::ListingWindow* listing = session->ListingWindow();
    listing->Open();
    listing->WriteLine("FangTongKaKou - square tube recognition report");
    listing->WriteLine("");
    listing->WriteLine("Rejected dimensions:");
    listing->WriteLine("  Index    Tag    Measured(L/W/H)    PlanarFaces    Reason");
    for (std::size_t index = 0; index < candidates.size(); ++index)
    {
        const SquareTubeCandidate& candidate = candidates[index];
        if (candidate.accepted)
        {
            continue;
        }

        std::ostringstream line;
        line << "  #" << (index + 1)
             << "    " << (candidate.body == NULL ? 0 : candidate.body->Tag())
             << "    " << FormatDouble(candidate.length)
             << "/" << FormatDouble(candidate.width)
             << "/" << FormatDouble(candidate.height)
             << "    " << candidate.planarFaceCount
             << "    " << candidate.reason;
        listing->WriteLine(line.str());
    }

    listing->WriteLine("");
    listing->WriteLine("Tube statistics:");
    listing->WriteLine("  Spec(WxH)    Length    Qty    TotalLength");
    for (std::map<std::string, TubeStat>::const_iterator it = stats.begin(); it != stats.end(); ++it)
    {
        const TubeStat& stat = it->second;
        std::ostringstream line;
        line << "  " << stat.spec
             << "    " << stat.length
             << "    " << stat.quantity
             << "    " << FormatDouble(stat.totalLength);
        listing->WriteLine(line.str());
    }

    std::ostringstream message;
    message << "Square tube recognition finished"
            << "\nSelected bodies: " << bodies.size()
            << "\nCandidate tubes: " << acceptedCount
            << "\nRejected bodies colored red: " << rejectedBodyTags.size()
            << "\nStat rows: " << stats.size()
            << "\n\nDetails were written to the Listing Window.";
    ui->NXMessageBox()->Show(
        "FangTongKaKou",
        NXOpen::NXMessageBox::DialogTypeInformation,
        message.str().c_str());
}

class FangTongKaKouDialog
{
public:
    FangTongKaKouDialog()
        : dialog(NULL),
          createModeBlock(NULL),
          portTypeBlock(NULL),
          guideImageBlock(NULL),
          maleFaceBlock(NULL),
          femaleFaceBlock(NULL),
          widthValueBlock(NULL),
          heightValueBlock(NULL),
          femaleDepthValueBlock(NULL),
          clearanceValueBlock(NULL),
          angleValueBlock(NULL),
          autoRecognizeTubeRToggle(NULL),
          mistakeProofToggle(NULL),
          specButton(NULL)
    {
        ui = NXOpen::UI::GetUI();
        dialog = ui->CreateDialog("FangTongKaKou.dlx");
        dialog->AddInitializeHandler(NXOpen::make_callback(this, &FangTongKaKouDialog::Initialize));
        dialog->AddDialogShownHandler(NXOpen::make_callback(this, &FangTongKaKouDialog::DialogShown));
        dialog->AddUpdateHandler(NXOpen::make_callback(this, &FangTongKaKouDialog::Update));
        dialog->AddOkHandler(NXOpen::make_callback(this, &FangTongKaKouDialog::Ok));
        dialog->AddApplyHandler(NXOpen::make_callback(this, &FangTongKaKouDialog::Apply));
    }

    ~FangTongKaKouDialog()
    {
        tubeCandidateCache.clear();
        if (dialog != NULL)
        {
            delete dialog;
            dialog = NULL;
        }
    }

    int Show()
    {
        return dialog->Launch();
    }

private:
    NXOpen::UI* ui;
    NXOpen::BlockStyler::BlockDialog* dialog;
    NXOpen::BlockStyler::Enumeration* createModeBlock;
    NXOpen::BlockStyler::Enumeration* portTypeBlock;
    NXOpen::BlockStyler::UIBlock* guideImageBlock;
    NXOpen::BlockStyler::SelectObject* maleFaceBlock;
    NXOpen::BlockStyler::SelectObject* femaleFaceBlock;
    NXOpen::BlockStyler::DoubleBlock* widthValueBlock;
    NXOpen::BlockStyler::DoubleBlock* heightValueBlock;
    NXOpen::BlockStyler::DoubleBlock* femaleDepthValueBlock;
    NXOpen::BlockStyler::DoubleBlock* clearanceValueBlock;
    NXOpen::BlockStyler::DoubleBlock* angleValueBlock;
    NXOpen::BlockStyler::Toggle* autoRecognizeTubeRToggle;
    NXOpen::BlockStyler::Toggle* mistakeProofToggle;
    NXOpen::BlockStyler::Button* specButton;
    std::map<tag_t, SquareTubeCandidate> tubeCandidateCache;

    void SetFaceSelectionFilter(NXOpen::BlockStyler::SelectObject* selection)
    {
        if (selection == NULL)
        {
            return;
        }

        NXOpen::Selection::SelectionAction action = NXOpen::Selection::SelectionActionClearAndEnableSpecific;
        std::vector<NXOpen::Selection::MaskTriple> masks;
        masks.push_back(NXOpen::Selection::MaskTriple(
            UF_solid_type,
            UF_solid_body_subtype,
            UF_UI_SEL_FEATURE_ANY_FACE));

        NXOpen::BlockStyler::PropertyList* properties = selection->GetProperties();
        properties->SetSelectionFilter("SelectionFilter", action, masks);
        delete properties;
    }

    void SetBlockVisible(NXOpen::BlockStyler::UIBlock* block, bool visible)
    {
        if (block == NULL)
        {
            return;
        }

        const char* names[] = {"Show", "Visibility"};
        for (std::size_t index = 0; index < 2; ++index)
        {
            NXOpen::BlockStyler::PropertyList* properties = NULL;
            try
            {
                properties = block->GetProperties();
                properties->SetLogical(names[index], visible);
            }
            catch (...)
            {
            }
            if (properties != NULL)
            {
                delete properties;
            }
        }
    }

    void SetBlockEnabled(NXOpen::BlockStyler::UIBlock* block, bool enabled)
    {
        if (block == NULL)
        {
            return;
        }

        const char* names[] = {"Enable", "Sensitivity"};
        for (std::size_t index = 0; index < 2; ++index)
        {
            NXOpen::BlockStyler::PropertyList* properties = NULL;
            try
            {
                properties = block->GetProperties();
                properties->SetLogical(names[index], enabled);
            }
            catch (...)
            {
            }
            if (properties != NULL)
            {
                delete properties;
            }
        }
    }

    void SetBlockLabel(NXOpen::BlockStyler::UIBlock* block, const char* utf8Label)
    {
        if (block == NULL)
        {
            return;
        }

        NXOpen::BlockStyler::PropertyList* properties = NULL;
        try
        {
            properties = block->GetProperties();
            properties->SetString(
                NXOpen::NXString("Label", NXOpen::NXString::UTF8),
                NXOpen::NXString(utf8Label, NXOpen::NXString::UTF8));
        }
        catch (...)
        {
            try
            {
                if (properties != NULL)
                {
                    properties->SetString(
                        NXOpen::NXString("Title", NXOpen::NXString::UTF8),
                        NXOpen::NXString(utf8Label, NXOpen::NXString::UTF8));
                }
            }
            catch (...)
            {
            }
        }
        if (properties != NULL)
        {
            delete properties;
        }
    }

    void UpdateDimensionTitles()
    {
        const bool autoRecognizeTubeR =
            autoRecognizeTubeRToggle != NULL && autoRecognizeTubeRToggle->Value();
        SetBlockLabel(
            heightValueBlock,
            autoRecognizeTubeR ?
                "\x48\xE4\xB8\xBA\x52\x2B" :
                "\xE9\xAB\x98\xE5\xBA\xA6\x20\x48");
        SetBlockLabel(
            femaleDepthValueBlock,
            autoRecognizeTubeR ?
                "\xE6\xAF\x8D\xE6\xA7\xBD\xE6\xB7\xB1\x52\x2B" :
                "\xE6\xAF\x8D\xE6\xA7\xBD\xE6\xB7\xB1");
    }

    void SetGuideImageForPortType(int portType)
    {
        if (guideImageBlock == NULL)
        {
            return;
        }

        const char* imageName =
            portType == 1 ? "FangTongKaKouTaperGuide.bmp" : "FangTongKaKouStraightGuide.bmp";
        std::string imagePath = imageName;
        const std::string moduleDirectory = GetModuleDirectory();
        if (!moduleDirectory.empty())
        {
            imagePath = moduleDirectory + "\\" + imageName;
        }

        NXOpen::BlockStyler::PropertyList* properties = NULL;
        try
        {
            properties = guideImageBlock->GetProperties();
            properties->SetFile("Image", imagePath.c_str());
        }
        catch (...)
        {
        }
        if (properties != NULL)
        {
            delete properties;
        }
    }

    std::string ToUtf8(const NXOpen::NXString& value)
    {
        const char* text = value.GetUTF8Text();
        return text == NULL ? std::string() : std::string(text);
    }

    void SetCreateModeMembers()
    {
        if (createModeBlock == NULL)
        {
            return;
        }

        std::vector<NXOpen::NXString> members;
        members.push_back(NXOpen::NXString(
            "\xE6\x89\x8B\xE5\x8A\xA8",
            NXOpen::NXString::UTF8));
        members.push_back(NXOpen::NXString(
            "\xE8\x87\xAA\xE5\x8A\xA8",
            NXOpen::NXString::UTF8));
        createModeBlock->SetEnumMembers(members);
        createModeBlock->SetValueAsString(members.front());
    }

    void SetPortTypeMembers()
    {
        if (portTypeBlock == NULL)
        {
            return;
        }

        std::vector<NXOpen::NXString> members;
        members.push_back(NXOpen::NXString(
            "\xE7\x9B\xB4\xE8\xA7\x92\xE5\x85\xAC\xE6\xAF\x8D\xE5\x8F\xA3",
            NXOpen::NXString::UTF8));
        members.push_back(NXOpen::NXString(
            "\xE9\x94\xA5\xE5\xBA\xA6\xE5\x85\xAC\xE6\xAF\x8D\xE5\x8F\xA3",
            NXOpen::NXString::UTF8));
        portTypeBlock->SetEnumMembers(members);
        portTypeBlock->SetValueAsString(members.front());
    }

    void UpdatePortTypeControls()
    {
        const int portType = GetPortTypeValue();
        SetGuideImageForPortType(portType);
        SetBlockEnabled(angleValueBlock, portType == 1);
    }

    void UpdateCreateModeControls()
    {
        const bool manualMode = GetCreateModeValue() == 0;
        SetBlockVisible(femaleFaceBlock, manualMode);
        SetBlockEnabled(femaleFaceBlock, manualMode);
        SetBlockEnabled(mistakeProofToggle, !manualMode);
    }

    int GetCreateModeValue()
    {
        if (createModeBlock == NULL)
        {
            return 0;
        }

        try
        {
            const std::string value = ToUtf8(createModeBlock->ValueAsString());
            if (value.find("\xE8\x87\xAA") != std::string::npos)
            {
                return 1;
            }
            if (value.find("\xE6\x89\x8B") != std::string::npos)
            {
                return 0;
            }
        }
        catch (...)
        {
        }

        return GetEnumValue(createModeBlock, 0);
    }

    int GetPortTypeValue()
    {
        if (portTypeBlock == NULL)
        {
            return 0;
        }

        try
        {
            const std::string value = ToUtf8(portTypeBlock->ValueAsString());
            if (value.find("\xE9\x94\xA5") != std::string::npos)
            {
                return 1;
            }
            if (value.find("\xE7\x9B\xB4") != std::string::npos)
            {
                return 0;
            }
        }
        catch (...)
        {
        }

        return GetEnumValue(portTypeBlock, 0);
    }

    int GetEnumValue(NXOpen::BlockStyler::Enumeration* block, int fallback)
    {
        if (block == NULL)
        {
            return fallback;
        }

        NXOpen::BlockStyler::PropertyList* properties = NULL;
        try
        {
            properties = block->GetProperties();
            const int value = properties->GetEnum("Value");
            delete properties;
            return value;
        }
        catch (...)
        {
            if (properties != NULL)
            {
                delete properties;
            }
        }
        return fallback;
    }

    SelectedFaceInfo GetSelectedFace(NXOpen::BlockStyler::SelectObject* selection)
    {
        SelectedFaceInfo result = {};
        result.face = NULL;
        result.pickPoint[0] = 0.0;
        result.pickPoint[1] = 0.0;
        result.pickPoint[2] = 0.0;
        if (selection == NULL)
        {
            return result;
        }

        NXOpen::BlockStyler::PropertyList* properties = selection->GetProperties();
        std::vector<NXOpen::TaggedObject*> selectedObjects =
            properties->GetTaggedObjectVector("SelectedObjects");
        NXOpen::Point3d pickPoint = properties->GetPoint("PickPoint");
        delete properties;

        if (selectedObjects.empty())
        {
            return result;
        }

        result.face = dynamic_cast<NXOpen::Face*>(selectedObjects.front());
        result.pickPoint[0] = pickPoint.X;
        result.pickPoint[1] = pickPoint.Y;
        result.pickPoint[2] = pickPoint.Z;
        return result;
    }

    void Initialize()
    {
        createModeBlock = dynamic_cast<NXOpen::BlockStyler::Enumeration*>(
            dialog->TopBlock()->FindBlock("createMode"));
        portTypeBlock = dynamic_cast<NXOpen::BlockStyler::Enumeration*>(
            dialog->TopBlock()->FindBlock("portType"));
        guideImageBlock = dialog->TopBlock()->FindBlock("guideImage");
        maleFaceBlock = dynamic_cast<NXOpen::BlockStyler::SelectObject*>(
            dialog->TopBlock()->FindBlock("maleFace"));
        femaleFaceBlock = dynamic_cast<NXOpen::BlockStyler::SelectObject*>(
            dialog->TopBlock()->FindBlock("femaleFace"));
        widthValueBlock = dynamic_cast<NXOpen::BlockStyler::DoubleBlock*>(
            dialog->TopBlock()->FindBlock("widthValue"));
        heightValueBlock = dynamic_cast<NXOpen::BlockStyler::DoubleBlock*>(
            dialog->TopBlock()->FindBlock("heightValue"));
        femaleDepthValueBlock = dynamic_cast<NXOpen::BlockStyler::DoubleBlock*>(
            dialog->TopBlock()->FindBlock("femaleDepthValue"));
        clearanceValueBlock = dynamic_cast<NXOpen::BlockStyler::DoubleBlock*>(
            dialog->TopBlock()->FindBlock("clearanceValue"));
        angleValueBlock = dynamic_cast<NXOpen::BlockStyler::DoubleBlock*>(
            dialog->TopBlock()->FindBlock("angleValue"));
        autoRecognizeTubeRToggle = dynamic_cast<NXOpen::BlockStyler::Toggle*>(
            dialog->TopBlock()->FindBlock("autoRecognizeTubeRToggle"));
        mistakeProofToggle = dynamic_cast<NXOpen::BlockStyler::Toggle*>(
            dialog->TopBlock()->FindBlock("mistakeProofToggle"));
        specButton = dynamic_cast<NXOpen::BlockStyler::Button*>(
            dialog->TopBlock()->FindBlock("specButton"));

        SetCreateModeMembers();
        SetPortTypeMembers();
        SetFaceSelectionFilter(maleFaceBlock);
        SetFaceSelectionFilter(femaleFaceBlock);
        UpdateCreateModeControls();
        UpdatePortTypeControls();
        UpdateDimensionTitles();
    }

    void DialogShown()
    {
        UpdateCreateModeControls();
        UpdatePortTypeControls();
        UpdateDimensionTitles();
    }

    int Update(NXOpen::BlockStyler::UIBlock* block)
    {
        if (block == createModeBlock)
        {
            UpdateCreateModeControls();
        }
        if (block == portTypeBlock)
        {
            UpdatePortTypeControls();
        }
        if (block == autoRecognizeTubeRToggle)
        {
            UpdateDimensionTitles();
        }
        if (block == specButton)
        {
            OpenSpecTable();
        }
        return 0;
    }

    int Ok()
    {
        try
        {
            SelectedFaceInfo maleFace = GetSelectedFace(maleFaceBlock);
            if (maleFace.face == NULL)
            {
                ui->NXMessageBox()->Show(
                    "FangTongKaKou",
                    NXOpen::NXMessageBox::DialogTypeInformation,
                    "Please select male face.");
                return 1;
            }

            const int createMode = GetCreateModeValue();
            const int portType = GetPortTypeValue();
            const double width = widthValueBlock != NULL ? widthValueBlock->Value() : 0.0;
            const double height = heightValueBlock != NULL ? heightValueBlock->Value() : 0.0;
            const double femaleDepth =
                femaleDepthValueBlock != NULL ? femaleDepthValueBlock->Value() : height;
            const double clearance = clearanceValueBlock != NULL ? clearanceValueBlock->Value() : 0.0;
            const double angle = angleValueBlock != NULL ? angleValueBlock->Value() : 0.0;
            const bool autoRecognizeTubeR =
                autoRecognizeTubeRToggle != NULL && autoRecognizeTubeRToggle->Value();
            const bool mistakeProof =
                mistakeProofToggle != NULL && mistakeProofToggle->Value();
            if (width <= 0.0 || (!autoRecognizeTubeR && height <= 0.0) ||
                (autoRecognizeTubeR && height < 0.0) ||
                (!autoRecognizeTubeR && femaleDepth <= 0.0) ||
                (autoRecognizeTubeR && femaleDepth < 0.0) || clearance < 0.0)
            {
                ui->NXMessageBox()->Show(
                    "FangTongKaKou",
                    NXOpen::NXMessageBox::DialogTypeInformation,
                    "Please input valid W, H, female depth and C values.");
                return 1;
            }
            if (portType == 1 && (angle <= 0.0 || angle >= 89.0))
            {
                ui->NXMessageBox()->Show(
                    "FangTongKaKou",
                    NXOpen::NXMessageBox::DialogTypeInformation,
                    "Please input a valid taper angle between 0 and 89 degrees.");
                return 1;
            }

            FacePlacement malePlacement = {};
            if (!AskPlanarFacePlacement(maleFace.face, maleFace.pickPoint, malePlacement, createMode != 0))
            {
                ui->NXMessageBox()->Show(
                    "FangTongKaKou",
                    NXOpen::NXMessageBox::DialogTypeInformation,
                    "The selected face must be a planar face.");
                return 1;
            }

            std::vector<TouchingPortPlacement> touchingEndPlacements;
            if (createMode == 0)
            {
                SelectedFaceInfo femaleFace = GetSelectedFace(femaleFaceBlock);
                if (femaleFace.face == NULL)
                {
                    ui->NXMessageBox()->Show(
                        "FangTongKaKou",
                        NXOpen::NXMessageBox::DialogTypeInformation,
                        "Please select female face.");
                    return 1;
                }

                tag_t femaleBodyTag = NULL_TAG;
                ThrowUfError(UF_MODL_ask_face_body(femaleFace.face->Tag(), &femaleBodyTag), "UF_MODL_ask_face_body");
                if (femaleBodyTag == NULL_TAG)
                {
                    ui->NXMessageBox()->Show(
                        "FangTongKaKou",
                        NXOpen::NXMessageBox::DialogTypeInformation,
                        "Could not resolve the female body.");
                    return 1;
                }

                TouchingPortPlacement portPlacement = {};
                portPlacement.malePlacement = malePlacement;
                portPlacement.femaleBodyTag = femaleBodyTag;
                touchingEndPlacements.push_back(portPlacement);
            }
            else
            {
                DisplaySuppressionGuard autoRecognitionDisplayGuard;

                std::vector<FacePlacement> coplanarPlacements =
                    CollectCoplanarTubeFacePlacements(malePlacement, &tubeCandidateCache);
                if (coplanarPlacements.empty())
                {
                    coplanarPlacements.push_back(malePlacement);
                }
                std::vector<tag_t> touchingCandidateBodyTags = CollectTouchCandidateBodyTags(malePlacement, &tubeCandidateCache);

                for (std::size_t index = 0; index < coplanarPlacements.size(); ++index)
                {
                    FacePlacement oppositePlacement = OppositeEndPlacement(coplanarPlacements[index]);
                    tag_t femaleBodyTag = NULL_TAG;
                    if (FindTouchingTubeAtEnd(coplanarPlacements[index], touchingCandidateBodyTags, &femaleBodyTag))
                    {
                        TouchingPortPlacement portPlacement = {};
                        portPlacement.malePlacement = coplanarPlacements[index];
                        portPlacement.femaleBodyTag = femaleBodyTag;
                        touchingEndPlacements.push_back(portPlacement);
                    }

                    femaleBodyTag = NULL_TAG;
                    if (FindTouchingTubeAtEnd(oppositePlacement, touchingCandidateBodyTags, &femaleBodyTag))
                    {
                        TouchingPortPlacement portPlacement = {};
                        portPlacement.malePlacement = oppositePlacement;
                        portPlacement.femaleBodyTag = femaleBodyTag;
                        touchingEndPlacements.push_back(portPlacement);
                    }
                }
            }

            std::vector<double> resolvedWidths(touchingEndPlacements.size(), width);
            if (createMode != 0 && mistakeProof)
            {
                std::map<tag_t, std::vector<std::size_t> > placementsByBody;
                for (std::size_t index = 0; index < touchingEndPlacements.size(); ++index)
                {
                    placementsByBody[touchingEndPlacements[index].malePlacement.bodyTag].push_back(index);
                }

                for (std::map<tag_t, std::vector<std::size_t> >::const_iterator it = placementsByBody.begin();
                     it != placementsByBody.end();
                     ++it)
                {
                    if (it->second.size() < 2)
                    {
                        continue;
                    }

                    std::size_t wideIndex = it->second.front();
                    double maxEndCoord = touchingEndPlacements[wideIndex].malePlacement.endCoord;
                    for (std::size_t groupIndex = 1; groupIndex < it->second.size(); ++groupIndex)
                    {
                        const std::size_t candidateIndex = it->second[groupIndex];
                        const double candidateEndCoord =
                            touchingEndPlacements[candidateIndex].malePlacement.endCoord;
                        if (candidateEndCoord > maxEndCoord)
                        {
                            wideIndex = candidateIndex;
                            maxEndCoord = candidateEndCoord;
                        }
                    }

                    resolvedWidths[wideIndex] = width + 3.0;
                }
            }

            std::vector<double> resolvedMaleHeights;
            resolvedMaleHeights.reserve(touchingEndPlacements.size());
            std::vector<double> resolvedFemaleDepths;
            resolvedFemaleDepths.reserve(touchingEndPlacements.size());
            for (std::size_t index = 0; index < touchingEndPlacements.size(); ++index)
            {
                const double outerRadius = autoRecognizeTubeR ?
                    AskTubeOuterCornerRadiusForBody(
                        touchingEndPlacements[index].femaleBodyTag,
                        touchingEndPlacements[index].malePlacement) :
                    0.0;
                const double resolvedMaleHeight =
                    ResolvePortHeight(height, outerRadius, autoRecognizeTubeR);
                const double resolvedFemaleDepth =
                    ResolvePortHeight(femaleDepth, outerRadius, autoRecognizeTubeR);
                if (resolvedMaleHeight <= 0.0 || resolvedFemaleDepth <= 0.0)
                {
                    ui->NXMessageBox()->Show(
                        "FangTongKaKou",
                        NXOpen::NXMessageBox::DialogTypeInformation,
                        "The resolved H and female depth values must be greater than 0.");
                    return 1;
                }
                resolvedMaleHeights.push_back(resolvedMaleHeight);
                resolvedFemaleDepths.push_back(resolvedFemaleDepth);
            }

            std::vector<int> femaleSucceeded(touchingEndPlacements.size(), 0);
            std::vector<TouchingPortPlacement> createdPortPlacements;

            for (std::size_t index = 0; index < touchingEndPlacements.size(); ++index)
            {
                try
                {
                    if (portType == 1)
                    {
                        CreateTaperFemalePortAtPlacement(
                            touchingEndPlacements[index].malePlacement,
                            touchingEndPlacements[index].femaleBodyTag,
                            resolvedWidths[index],
                            resolvedMaleHeights[index],
                            resolvedFemaleDepths[index],
                            clearance,
                            angle);
                    }
                    else
                    {
                        CreateFemalePortAtPlacement(
                            touchingEndPlacements[index].malePlacement,
                            touchingEndPlacements[index].femaleBodyTag,
                            resolvedWidths[index],
                            resolvedMaleHeights[index],
                            resolvedFemaleDepths[index],
                            clearance);
                    }
                    femaleSucceeded[index] = 1;
                }
                catch (const NXOpen::NXException& ex)
                {
                    (void)ex;
                }
                catch (const std::exception& ex)
                {
                    (void)ex;
                }
            }

            for (std::size_t index = 0; index < touchingEndPlacements.size(); ++index)
            {
                if (femaleSucceeded[index] == 0)
                {
                    continue;
                }

                try
                {
                    bool maleSucceeded = false;
                    if (portType == 1)
                    {
                        maleSucceeded = CreateTaperMalePortAtPlacement(
                            touchingEndPlacements[index].malePlacement,
                            resolvedWidths[index],
                            resolvedMaleHeights[index],
                            angle);
                    }
                    else
                    {
                        maleSucceeded = CreateMalePortAtPlacement(
                            touchingEndPlacements[index].malePlacement,
                            resolvedWidths[index],
                            resolvedMaleHeights[index]);
                    }

                    if (maleSucceeded)
                    {
                        createdPortPlacements.push_back(touchingEndPlacements[index]);
                    }
                    else
                    {
                    }
                }
                catch (const NXOpen::NXException& ex)
                {
                    (void)ex;
                }
                catch (const std::exception& ex)
                {
                    (void)ex;
                }
            }

            MarkCreatedPortFaces(createdPortPlacements);
            return 0;
        }
        catch (const NXOpen::NXException& ex)
        {
            ui->NXMessageBox()->Show("FangTongKaKou", NXOpen::NXMessageBox::DialogTypeError, ex.Message());
            return 1;
        }
        catch (const std::exception& ex)
        {
            ui->NXMessageBox()->Show("FangTongKaKou", NXOpen::NXMessageBox::DialogTypeError, ex.what());
            return 1;
        }
    }

    int Apply()
    {
        return Ok();
    }
};
}

extern "C" DllExport void ufusr(char* param, int* retcode, int param_len)
{
    (void)param;
    (void)param_len;
    if (retcode != NULL)
    {
        *retcode = 0;
    }

    if (!zhihui_license_guard::EnsureAuthorized(L"ZHIHUI.FANGTONGKAKOU", L"FangTongKaKou"))
    {
        return;
    }

    try
    {
        UF_initialize();
        FangTongKaKouDialog commandDialog;
        commandDialog.Show();
        UF_terminate();
    }
    catch (const NXOpen::NXException& ex)
    {
        NXOpen::UI::GetUI()->NXMessageBox()->Show(
            "FangTongKaKou",
            NXOpen::NXMessageBox::DialogTypeError,
            ex.Message());
        if (retcode != NULL)
        {
            *retcode = ex.ErrorCode();
        }
        UF_terminate();
    }
    catch (const std::exception& ex)
    {
        NXOpen::UI::GetUI()->NXMessageBox()->Show(
            "FangTongKaKou",
            NXOpen::NXMessageBox::DialogTypeError,
            ex.what());
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


