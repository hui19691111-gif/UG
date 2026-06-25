#include "PiLianZuanBanJin.hpp"

#include "../../../common/ZhihuiBendRulesIni.hpp"
#include "../../../../common/ZhihuiEmbeddedDialog.hpp"
#include "../../embedded_dialog_resources.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <queue>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <windows.h>
#include <commdlg.h>
#include <commctrl.h>
#pragma comment(lib, "Comdlg32.lib")
#pragma comment(lib, "Comctl32.lib")
#ifdef CreateDialog
#undef CreateDialog
#endif
extern "C" IMAGE_DOS_HEADER __ImageBase;

using namespace NXOpen;
using namespace NXOpen::Assemblies;
using namespace NXOpen::BlockStyler;
using namespace NXOpen::Features;
using namespace NXOpen::Features::SheetMetal;
using namespace NXOpen::Preferences;

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

    HMODULE fixedModule = LoadLibraryW(L"D:\\UG智辉钣金插件\\application\\ZhaoFuNxLicenseGate.dll");
    if (fixedModule != NULL)
    {
        return fixedModule;
    }

    return LoadLibraryW(moduleName);
}

bool EnsureAuthorized(const wchar_t* featureCode, const wchar_t* displayName)
{
    wchar_t message[1024] = { 0 };
    HMODULE module = LoadProtectedLicenseGate();
    if (module == NULL)
    {
        return false;
    }

    EnsureAuthorizedProc ensureAuthorized = reinterpret_cast<EnsureAuthorizedProc>(GetProcAddress(module, "ZfnxEnsureAuthorized"));
    if (ensureAuthorized == NULL)
    {
        return false;
    }

    const int ok = ensureAuthorized(featureCode, displayName, message, static_cast<int>(sizeof(message) / sizeof(message[0])));
    const int ok2 = ok == 1 ? ensureAuthorized(featureCode, displayName, message, static_cast<int>(sizeof(message) / sizeof(message[0]))) : ok;
    return ok == 1 && ok2 == 1;
}
}

namespace
{
    std::vector<Edge*> FaceEdgesByUf(Face* face);
    double Dot3(const double a[3], const double b[3]);
    double Distance3(const double a[3], const double b[3]);
    double DistancePointToAxis(const double point[3], const double axisPoint[3], const double axisDirection[3]);
    bool AskCylinderFaceData(Face* face, double axisPoint[3], double axisDirection[3], double* radiusOut, int* normDirOut);
    bool Normalize3(double v[3]);
    bool WriteAllText(const std::string& path, const std::string& text);
    bool IsOuterCylinderFaceLike08(Face* face);
    bool AskFacePointFromEdges(Face* face, double facePoint[3]);
    bool AskNxOpenCreatedFaceNormal(Face* face, const double facePoint[3], double unitNormal[3]);
    bool AskOuterCylinderMedLike08(Face* face, double* medOut);
    struct CircleProfileStats;
    struct ShadowOutlineInfo;
    bool CylinderFaceHasFullCircleProfile(Face* face, double cylinderRadius, CircleProfileStats* stats);
    bool CollectBodyShadowOutlineByNx(Body* body, const double axisPoint[3], const double axisDirection[3], ShadowOutlineInfo* info);

    const char* PiLianConfigFileName = "PiLianZuanBanJin_bend_factor_config.json";
    const char* PiLianRulesIniFileName = "PiLianZuanBanJin_bend_factor_rules.ini";
    const char* PiLianSettingsArgument = "--pilian-zhuanbanjin-bend-factor";
    const char* MarkedFaceAttributeTitle = "\xE6\xA0\x87\xE8\xAE\xB0\xE9\x9D\xA2";
    const char* MarkedLineAttributeTitle = "\xE6\xA0\x87\xE8\xAE\xB0\xE7\xBA\xBF";

    struct BodyBox
    {
        double length = 0.0;
        double width = 0.0;
        double height = 0.0;
        double score = 0.0;
    };

    struct FaceInfo
    {
        Face* face = NULL;
        int type = 0;
        double areaScore = -1.0;
        double totalScore = -1.0;
    };

    struct FaceCandidate
    {
        Face* face = NULL;
        int type = 0;
        double area = 0.0;
        double center[3] = { 0.0, 0.0, 0.0 };
        double normal[3] = { 0.0, 0.0, 1.0 };
        double score = 0.0;
    };

    struct MarkedChainSelection
    {
        bool found = false;
        std::map<tag_t, bool> chainTags;
        std::string source;
    };

    struct Point2dLite
    {
        double x = 0.0;
        double y = 0.0;
    };

    struct FastenerFilterInfo
    {
        bool matched = false;
        std::string outlineType = "none";
        double height = 0.0;
        double outlineSize = 0.0;
        double threshold = 0.0;
        double cylinderDiameter = 0.0;
        size_t shadowPointCount = 0;
        size_t hullPointCount = 0;
        int shadowCurveCount = 0;
        int shadowLineCount = 0;
        int shadowCircleCount = 0;
        int shadowSplineCount = 0;
        int shadowOtherCount = 0;
        int cylinderFaceCount = 0;
        int outerCylinderFaceCount = 0;
        int fullCircleEdgeCount = 0;
        int halfCircleEdgeCount = 0;
        int circularEdgeCount = 0;
        double outerMed = 0.0;
        std::string reason;
    };

    struct FlatSolidThicknessCheckInfo
    {
        bool passed = false;
        std::string reason;
        Body* flatBody = NULL;
        Face* largestFace = NULL;
        double expectedThickness = 0.0;
        double measuredThickness = 0.0;
        double flatSizeX = 0.0;
        double flatSizeY = 0.0;
        double flatSizeU = 0.0;
        double flatSizeV = 0.0;
        double flatSizeNormal = 0.0;
        double normal[3] = { 0.0, 0.0, 1.0 };
        std::string resolveSource;
    };

    struct CircleProfileStats
    {
        int circularEdgeCount = 0;
        int fullCircleEdgeCount = 0;
        int halfCircleEdgeCount = 0;
        double lastRadius = 0.0;
    };

    struct ShadowOutlineInfo
    {
        bool matched = false;
        std::string outlineType = "none";
        double outlineSize = 0.0;
        size_t hullPointCount = 0;
        int curveCount = 0;
        int lineCount = 0;
        int circleCount = 0;
        int splineCount = 0;
        int otherCount = 0;
        std::string reason;
    };

    std::map<tag_t, bool> BuildScoringTangentChainTags(const std::vector<FaceCandidate>& candidates);

    struct BodyResult
    {
        int layer = 0;
        std::string name;
        bool convertOk = false;
        int neutralFaceCount = 0;
        bool flatOk = false;
        std::string error;
    };

    struct BendRule
    {
        bool enabled = true;
        bool fallback = false;
        std::string name;
        std::string material;
        std::string method = "KFactor";
        std::string note;
        double value = 0.5;
        bool hasAngleMin = false;
        bool hasAngleMax = false;
        bool hasThicknessMin = false;
        bool hasThicknessMax = false;
        bool hasRadiusMin = false;
        bool hasRadiusMax = false;
        bool hasRadiusOverride = false;
        double angleMin = 0.0;
        double angleMax = 0.0;
        double thicknessMin = 0.0;
        double thicknessMax = 0.0;
        double radiusMin = 0.0;
        double radiusMax = 0.0;
        double radiusOverride = 0.0;
    };

    struct RuleConfig
    {
        std::vector<BendRule> rules;
        bool useAbsoluteLargeArc = true;
        double absoluteLargeArcRadius = 7.0;
        bool useRatioLargeArc = false;
        double ratioLargeArc = 5.0;
    };

    struct BendFaceRecord
    {
        Face* face = NULL;
        std::string material;
        double thickness = 0.0;
        double innerRadius = 0.0;
        double angleDeg = 0.0;
        double currentK = 0.0;
        double targetK = 0.5;
        double targetRadius = 0.0;
    };

    class UfGuard
    {
    public:
        UfGuard()
        {
            int error = UF_initialize();
            if (error != 0)
            {
                throw std::runtime_error("UF_initialize failed: " + std::to_string(error));
            }
        }

        ~UfGuard()
        {
            UF_terminate();
        }
    };

    std::string ToUtf8(const NXString& value)
    {
        const char* text = value.GetUTF8Text();
        return text == NULL ? std::string() : std::string(text);
    }

    std::string TrimCopy(const std::string& value)
    {
        size_t first = 0;
        while (first < value.size() && static_cast<unsigned char>(value[first]) <= ' ')
        {
            ++first;
        }

        size_t last = value.size();
        while (last > first && static_cast<unsigned char>(value[last - 1]) <= ' ')
        {
            --last;
        }

        return value.substr(first, last - first);
    }

    bool IsValidUtf8(const std::string& value)
    {
        size_t i = 0;
        while (i < value.size())
        {
            const unsigned char c = static_cast<unsigned char>(value[i]);
            if (c <= 0x7F)
            {
                ++i;
            }
            else if ((c & 0xE0) == 0xC0)
            {
                if (i + 1 >= value.size() || (static_cast<unsigned char>(value[i + 1]) & 0xC0) != 0x80 || c < 0xC2)
                {
                    return false;
                }
                i += 2;
            }
            else if ((c & 0xF0) == 0xE0)
            {
                if (i + 2 >= value.size()
                    || (static_cast<unsigned char>(value[i + 1]) & 0xC0) != 0x80
                    || (static_cast<unsigned char>(value[i + 2]) & 0xC0) != 0x80)
                {
                    return false;
                }
                i += 3;
            }
            else if ((c & 0xF8) == 0xF0)
            {
                if (i + 3 >= value.size()
                    || (static_cast<unsigned char>(value[i + 1]) & 0xC0) != 0x80
                    || (static_cast<unsigned char>(value[i + 2]) & 0xC0) != 0x80
                    || (static_cast<unsigned char>(value[i + 3]) & 0xC0) != 0x80)
                {
                    return false;
                }
                i += 4;
            }
            else
            {
                return false;
            }
        }

        return true;
    }

    std::string AnsiToUtf8(const std::string& value)
    {
        if (value.empty())
        {
            return value;
        }

        int wideLength = MultiByteToWideChar(CP_ACP, 0, value.c_str(), -1, NULL, 0);
        if (wideLength <= 0)
        {
            return value;
        }

        std::wstring wide(static_cast<size_t>(wideLength), L'\0');
        MultiByteToWideChar(CP_ACP, 0, value.c_str(), -1, &wide[0], wideLength);

        int utf8Length = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, NULL, 0, NULL, NULL);
        if (utf8Length <= 0)
        {
            return value;
        }

        std::string utf8(static_cast<size_t>(utf8Length), '\0');
        WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, &utf8[0], utf8Length, NULL, NULL);
        if (!utf8.empty() && utf8[utf8.size() - 1] == '\0')
        {
            utf8.resize(utf8.size() - 1);
        }

        return utf8;
    }

    std::string NormalizeUtf8Message(const std::string& value)
    {
        return IsValidUtf8(value) ? value : AnsiToUtf8(value);
    }

    bool HasNonEmptyStringAttribute(tag_t objectTag, const char* title)
    {
        if (objectTag == NULL_TAG || title == NULL || title[0] == '\0')
        {
            return false;
        }

        UF_ATTR_info_t info;
        UF_ATTR_init_user_attribute_info(&info);
        logical hasAttribute = false;
        const int error = UF_ATTR_get_user_attribute_with_title_and_type(
            objectTag,
            title,
            UF_ATTR_string,
            UF_ATTR_NOT_ARRAY,
            &info,
            &hasAttribute);

        bool ok = false;
        if (error == 0 && hasAttribute && !info.unset && info.string_value != NULL)
        {
            ok = !TrimCopy(info.string_value).empty();
        }

        UF_ATTR_free_user_attribute_info_strings(&info);
        return ok;
    }

    bool HasIntegerAttribute(tag_t objectTag, const char* title)
    {
        if (objectTag == NULL_TAG || title == NULL || title[0] == '\0')
        {
            return false;
        }

        UF_ATTR_info_t info;
        UF_ATTR_init_user_attribute_info(&info);
        logical hasAttribute = false;
        const int error = UF_ATTR_get_user_attribute_with_title_and_type(
            objectTag,
            title,
            UF_ATTR_integer,
            UF_ATTR_NOT_ARRAY,
            &info,
            &hasAttribute);

        const bool ok = error == 0 && hasAttribute && !info.unset;
        UF_ATTR_free_user_attribute_info_strings(&info);
        return ok;
    }

    bool HasUserAttributeTitle(tag_t objectTag, const char* title)
    {
        if (objectTag == NULL_TAG || title == NULL || title[0] == '\0')
        {
            return false;
        }

        logical hasAttribute = false;
        const int error = UF_ATTR_has_user_attribute_with_title_and_type(
            objectTag,
            title,
            UF_ATTR_any,
            UF_ATTR_NOT_ARRAY,
            &hasAttribute);
        if (error == 0 && hasAttribute)
        {
            return true;
        }

        try
        {
            NXObject* object = dynamic_cast<NXObject*>(NXObjectManager::Get(objectTag));
            if (object == NULL)
            {
                return false;
            }

            NXString utf8Title(title, NXString::UTF8);
            if (object->HasUserAttribute(utf8Title, NXObject::AttributeType::AttributeTypeAny, -1))
            {
                return true;
            }

            return object->HasUserAttribute(title, NXObject::AttributeType::AttributeTypeAny, -1);
        }
        catch (...)
        {
            return false;
        }
    }

    bool HasMarkedBaseAttribute(tag_t objectTag)
    {
        return HasUserAttributeTitle(objectTag, MarkedFaceAttributeTitle) ||
            HasUserAttributeTitle(objectTag, MarkedLineAttributeTitle);
    }

    std::string ReadStringUserAttribute(NXObject* object, const char* title)
    {
        if (object == NULL || title == NULL || title[0] == '\0')
        {
            return std::string();
        }

        try
        {
            if (!object->HasUserAttribute(title, NXObject::AttributeType::AttributeTypeString, -1))
            {
                return std::string();
            }

            std::string value = object->GetStringAttribute(title).GetLocaleText();
            return TrimCopy(NormalizeUtf8Message(value));
        }
        catch (...)
        {
            return std::string();
        }
    }

    std::string ReadBodyMaterialText(Part* part, Body* body)
    {
        std::string material = ReadStringUserAttribute(body, "cailiao");
        if (!material.empty())
        {
            return material;
        }

        material = ReadStringUserAttribute(body, "\xE6\x9D\x90\xE6\x96\x99");
        if (!material.empty())
        {
            return material;
        }

        material = ReadStringUserAttribute(body, "\xE6\x9D\x90\xE8\xB4\xA8");
        if (!material.empty())
        {
            return material;
        }

        material = ReadStringUserAttribute(part, "cailiao");
        if (!material.empty())
        {
            return material;
        }

        material = ReadStringUserAttribute(part, "\xE6\x9D\x90\xE6\x96\x99");
        if (!material.empty())
        {
            return material;
        }

        return ReadStringUserAttribute(part, "\xE6\x9D\x90\xE8\xB4\xA8");
    }

    bool PartHasBatchAttributes(Part* part)
    {
        return part != NULL
            && HasNonEmptyStringAttribute(part->Tag(), u8"材料")
            && HasNonEmptyStringAttribute(part->Tag(), u8"数量");
    }

    bool BodyHasBatchAttributes(Body* body)
    {
        return body != NULL
            && HasNonEmptyStringAttribute(body->Tag(), "cailiao")
            && HasIntegerAttribute(body->Tag(), "sulian");
    }

    std::string PartName(Part* part)
    {
        if (part == NULL)
        {
            return std::string();
        }

        try
        {
            return ToUtf8(part->Leaf());
        }
        catch (...)
        {
            return std::string();
        }
    }

    struct BatchPartCandidate
    {
        Part* part;
        std::string partName;
        std::string material;
        std::string quantity;
    };

    NXString U8(const char* text);
    std::wstring PathTextToWide(const std::string& text);
    std::string ReadAllText(const std::string& path);
    bool WriteAllText(const std::string& path, const std::string& text);

    void CollectChildPrototypeParts(Component* component, std::vector<Part*>& parts, std::set<tag_t>& seenParts)
    {
        if (component == NULL)
        {
            return;
        }

        std::vector<Component*> children = component->GetChildren();
        for (size_t i = 0; i < children.size(); ++i)
        {
            Component* child = children[i];
            if (child == NULL)
            {
                continue;
            }

            try
            {
                if (child->IsComponentOrAncestorSuppressed())
                {
                    continue;
                }
            }
            catch (...)
            {
            }

            try
            {
                Part* prototypePart = dynamic_cast<Part*>(child->Prototype());
                if (prototypePart != NULL && seenParts.insert(prototypePart->Tag()).second)
                {
                    parts.push_back(prototypePart);
                }
            }
            catch (...)
            {
            }

            CollectChildPrototypeParts(child, parts, seenParts);
        }
    }

    void EnsurePartFullyLoadedForBatchList(Part* part)
    {
        if (part == NULL)
        {
            return;
        }

        try
        {
            if (part->IsFullyLoaded())
            {
                return;
            }

            PartLoadStatus* loadStatus = part->LoadThisPartFully();
            if (loadStatus != NULL)
            {
                delete loadStatus;
                loadStatus = NULL;
            }
        }
        catch (...)
        {
        }
    }

    std::string ReadPartMaterialText(Part* part)
    {
        std::string value = ReadStringUserAttribute(part, u8"材料");
        if (!value.empty())
        {
            return value;
        }

        value = ReadStringUserAttribute(part, u8"材质");
        if (!value.empty())
        {
            return value;
        }

        return ReadStringUserAttribute(part, "cailiao");
    }

    std::string ReadPartQuantityText(Part* part)
    {
        std::string value = ReadStringUserAttribute(part, u8"数量");
        if (!value.empty())
        {
            return value;
        }

        value = ReadStringUserAttribute(part, "sulian");
        if (!value.empty())
        {
            return value;
        }

        return value;
    }

    void CollectBatchPartCandidates(Component* component, std::vector<BatchPartCandidate>& candidates, std::set<tag_t>& seenParts)
    {
        if (component == NULL)
        {
            return;
        }

        std::vector<Component*> children;
        try
        {
            children = component->GetChildren();
        }
        catch (...)
        {
            return;
        }

        for (size_t i = 0; i < children.size(); ++i)
        {
            Component* child = children[i];
            if (child == NULL)
            {
                continue;
            }

            try
            {
                if (child->IsComponentOrAncestorSuppressed())
                {
                    continue;
                }
            }
            catch (...)
            {
            }

            try
            {
                Part* prototypePart = dynamic_cast<Part*>(child->Prototype());
                if (prototypePart != NULL && seenParts.insert(prototypePart->Tag()).second)
                {
                    EnsurePartFullyLoadedForBatchList(prototypePart);

                    BatchPartCandidate candidate;
                    candidate.part = prototypePart;
                    candidate.partName = ToUtf8(child->DisplayName());
                    if (candidate.partName.empty())
                    {
                        candidate.partName = PartName(prototypePart);
                    }
                    candidate.material = ReadPartMaterialText(prototypePart);
                    candidate.quantity = ReadPartQuantityText(prototypePart);
                    candidates.push_back(candidate);
                }
            }
            catch (...)
            {
            }

            CollectBatchPartCandidates(child, candidates, seenParts);
        }
    }

    std::vector<BatchPartCandidate> CollectBatchPartCandidates(Part* assemblyPart)
    {
        std::vector<BatchPartCandidate> candidates;
        if (assemblyPart == NULL || assemblyPart->ComponentAssembly() == NULL)
        {
            return candidates;
        }

        Component* root = assemblyPart->ComponentAssembly()->RootComponent();
        if (root == NULL || root->GetChildren().empty())
        {
            return candidates;
        }

        try
        {
            std::vector<Component*> topChildren = root->GetChildren();
            if (!topChildren.empty())
            {
                std::vector<ComponentAssembly::OpenComponentStatus> openStatus;
                PartLoadStatus* loadStatus = assemblyPart->ComponentAssembly()->OpenComponents(
                    ComponentAssembly::OpenOptionWholeAssembly,
                    topChildren,
                    openStatus);
                if (loadStatus != NULL)
                {
                    delete loadStatus;
                    loadStatus = NULL;
                }
            }
        }
        catch (...)
        {
        }

        std::set<tag_t> seenParts;
        CollectBatchPartCandidates(root, candidates, seenParts);
        return candidates;
    }

    bool IsAssemblyPart(Part* part)
    {
        if (part == NULL || part->ComponentAssembly() == NULL)
        {
            return false;
        }

        try
        {
            Component* root = part->ComponentAssembly()->RootComponent();
            return root != NULL && !root->GetChildren().empty();
        }
        catch (...)
        {
        }
        return false;
    }

    Part* ResolveCurrentAssemblyPart(Session* session)
    {
        if (session == NULL || session->Parts() == NULL)
        {
            return NULL;
        }

        Part* displayPart = NULL;
        Part* workPart = NULL;
        try
        {
            displayPart = session->Parts()->Display();
        }
        catch (...)
        {
        }
        try
        {
            workPart = session->Parts()->Work();
        }
        catch (...)
        {
        }

        if (IsAssemblyPart(displayPart))
        {
            return displayPart;
        }
        if (IsAssemblyPart(workPart))
        {
            return workPart;
        }
        return NULL;
    }

    std::vector<std::string> BuildBatchPartAttributeNames(const std::vector<BatchPartCandidate>& candidates)
    {
        std::vector<std::string> names;
        names.push_back(std::string());

        for (size_t i = 0; i < candidates.size(); ++i)
        {
            Part* part = candidates[i].part;
            if (part == NULL)
            {
                continue;
            }

            try
            {
                std::vector<NXObject::AttributeInformation> attributes = part->GetUserAttributes();
                for (size_t j = 0; j < attributes.size(); ++j)
                {
                    std::string title = TrimCopy(NormalizeUtf8Message(ToUtf8(attributes[j].Title)));
                    if (title.empty())
                    {
                        continue;
                    }

                    bool exists = false;
                    for (size_t k = 0; k < names.size(); ++k)
                    {
                        if (_stricmp(names[k].c_str(), title.c_str()) == 0)
                        {
                            exists = true;
                            break;
                        }
                    }
                    if (!exists)
                    {
                        names.push_back(title);
                    }
                }
            }
            catch (...)
            {
            }
        }

        return names;
    }

    std::string ReadPartListAttributeText(Part* part, const std::string& attributeName)
    {
        if (part == NULL || attributeName.empty())
        {
            return std::string();
        }

        if (attributeName == u8"材料")
        {
            return ReadPartMaterialText(part);
        }
        if (attributeName == u8"数量")
        {
            return ReadPartQuantityText(part);
        }

        std::string value = ReadStringUserAttribute(part, attributeName.c_str());
        if (!value.empty())
        {
            return value;
        }

        try
        {
            std::vector<NXObject::AttributeInformation> attributes = part->GetUserAttributes();
            for (size_t i = 0; i < attributes.size(); ++i)
            {
                std::string title = TrimCopy(NormalizeUtf8Message(ToUtf8(attributes[i].Title)));
                if (_stricmp(title.c_str(), attributeName.c_str()) != 0 || attributes[i].Unset)
                {
                    continue;
                }

                switch (attributes[i].Type)
                {
                case NXObject::AttributeType::AttributeTypeString:
                    return TrimCopy(NormalizeUtf8Message(ToUtf8(attributes[i].StringValue)));
                case NXObject::AttributeType::AttributeTypeInteger:
                    return std::to_string(attributes[i].IntegerValue);
                case NXObject::AttributeType::AttributeTypeReal:
                {
                    std::ostringstream stream;
                    stream << std::setprecision(12) << attributes[i].RealValue;
                    return stream.str();
                }
                case NXObject::AttributeType::AttributeTypeBoolean:
                    return attributes[i].BooleanValue ? "True" : "False";
                default:
                    break;
                }
            }
        }
        catch (...)
        {
        }

        return std::string();
    }

    const int BatchAttributeColumnCount = 4;
    const int BatchPickerListWidth = 1008;
    const int BatchPickerWindowWidth = 1052;
    const int BatchAttributeComboDropHeight = 260;

    struct BatchPartPickerPersistedState
    {
        std::vector<std::string> attributes;
        bool hasAttributes = false;
        std::set<std::string> checkedPartNames;
        bool hasCheckedPartNames = false;
    };

    struct BatchPartPickerState
    {
        const std::vector<BatchPartCandidate>* candidates;
        const std::vector<std::string>* attributeNames;
        const BatchPartPickerPersistedState* persistedState;
        std::vector<std::string> selectedAttributes;
        std::vector<int> selectedIndices;
        HWND listView;
        HWND comboBoxes[BatchAttributeColumnCount];
        int listX;
        int listY;
        int listWidth;
        int listHeight;
        std::vector<HWND> childControls;
        HFONT dialogFont;
        HBRUSH backgroundBrush;
        COLORREF backgroundColor;
        bool accepted;
    };

    HFONT CreateNxLikeDialogFont()
    {
        HDC screenDc = GetDC(NULL);
        const int dpiY = screenDc != NULL ? GetDeviceCaps(screenDc, LOGPIXELSY) : 96;
        if (screenDc != NULL)
        {
            ReleaseDC(NULL, screenDc);
        }

        return CreateFontW(
            -MulDiv(9, dpiY, 72),
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
            DEFAULT_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE,
            L"Microsoft YaHei UI");
    }

    void AddPickerControl(BatchPartPickerState* state, HWND control)
    {
        if (state == NULL || control == NULL)
        {
            return;
        }

        state->childControls.push_back(control);
        if (state->dialogFont != NULL)
        {
            SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(state->dialogFont), TRUE);
        }
    }

    void ApplyNxLikeListViewStyle(HWND listView)
    {
        if (listView == NULL)
        {
            return;
        }

        ListView_SetExtendedListViewStyle(listView, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_CHECKBOXES | LVS_EX_DOUBLEBUFFER);
        ListView_SetBkColor(listView, RGB(255, 255, 255));
        ListView_SetTextBkColor(listView, RGB(255, 255, 255));
        ListView_SetTextColor(listView, RGB(30, 30, 30));
    }

    void CenterWindowOnParent(HWND hwnd, HWND parent)
    {
        if (hwnd == NULL)
        {
            return;
        }

        RECT windowRect = {};
        RECT parentRect = {};
        GetWindowRect(hwnd, &windowRect);
        if (parent != NULL && IsWindow(parent))
        {
            GetWindowRect(parent, &parentRect);
        }
        else
        {
            SystemParametersInfoW(SPI_GETWORKAREA, 0, &parentRect, 0);
        }

        const int windowWidth = windowRect.right - windowRect.left;
        const int windowHeight = windowRect.bottom - windowRect.top;
        const int parentWidth = parentRect.right - parentRect.left;
        const int parentHeight = parentRect.bottom - parentRect.top;
        const int x = parentRect.left + std::max(0, (parentWidth - windowWidth) / 2);
        const int y = parentRect.top + std::max(0, (parentHeight - windowHeight) / 2);
        SetWindowPos(hwnd, NULL, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
    }

    void AddListViewColumn(HWND listView, int index, const wchar_t* title, int width)
    {
        LVCOLUMNW column = {};
        column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        column.pszText = const_cast<wchar_t*>(title);
        column.cx = width;
        column.iSubItem = index;
        ListView_InsertColumn(listView, index, &column);
    }

    void SetListViewText(HWND listView, int row, int column, const std::wstring& text)
    {
        LVITEMW item = {};
        item.mask = LVIF_TEXT;
        item.iItem = row;
        item.iSubItem = column;
        item.pszText = const_cast<wchar_t*>(text.c_str());
        ListView_SetItem(listView, &item);
    }

    std::wstring ListAttributeTitle(const std::string& attributeName)
    {
        return attributeName.empty() ? L"空白" : PathTextToWide(attributeName);
    }

    void RepositionHeaderCombos(BatchPartPickerState* state)
    {
        if (state == NULL || state->listView == NULL)
        {
            return;
        }

        HWND header = ListView_GetHeader(state->listView);
        RECT headerRect = {};
        if (header == NULL || !GetWindowRect(header, &headerRect))
        {
            return;
        }

        POINT headerOrigin = { headerRect.left, headerRect.top };
        ScreenToClient(GetParent(state->listView), &headerOrigin);
        const int headerHeight = std::max(20L, headerRect.bottom - headerRect.top);

        for (int i = 0; i < BatchAttributeColumnCount; ++i)
        {
            HWND combo = state->comboBoxes[i];
            if (combo == NULL)
            {
                continue;
            }

            RECT itemRect = {};
            if (Header_GetItemRect(header, 3 + i, &itemRect))
            {
                SetWindowPos(
                    combo,
                    HWND_TOP,
                    headerOrigin.x + itemRect.left + 1,
                    headerOrigin.y + 1,
                    std::max(64L, itemRect.right - itemRect.left - 2),
                    headerHeight + BatchAttributeComboDropHeight,
                    SWP_SHOWWINDOW);
            }
        }
    }

    void RefreshBatchPartListAttributeColumns(BatchPartPickerState* state)
    {
        if (state == NULL || state->listView == NULL || state->candidates == NULL)
        {
            return;
        }

        for (int columnOffset = 0; columnOffset < BatchAttributeColumnCount; ++columnOffset)
        {
            const int columnIndex = 3 + columnOffset;
            std::string attributeName;
            if (columnOffset < static_cast<int>(state->selectedAttributes.size()))
            {
                attributeName = state->selectedAttributes[static_cast<size_t>(columnOffset)];
            }

            LVCOLUMNW column = {};
            column.mask = LVCF_TEXT;
            std::wstring title = ListAttributeTitle(attributeName);
            column.pszText = const_cast<wchar_t*>(title.c_str());
            ListView_SetColumn(state->listView, columnIndex, &column);

            for (size_t row = 0; row < state->candidates->size(); ++row)
            {
                const BatchPartCandidate& candidate = (*state->candidates)[row];
                SetListViewText(
                    state->listView,
                    static_cast<int>(row),
                    columnIndex,
                    PathTextToWide(ReadPartListAttributeText(candidate.part, attributeName)));
            }
        }
    }

    void PopulateBatchPartListView(BatchPartPickerState* state)
    {
        if (state == NULL || state->listView == NULL || state->candidates == NULL)
        {
            return;
        }

        AddListViewColumn(state->listView, 0, L"选择", 48);
        AddListViewColumn(state->listView, 1, L"序号", 56);
        AddListViewColumn(state->listView, 2, L"部件名", 220);
        AddListViewColumn(state->listView, 3, L"材料", 170);
        AddListViewColumn(state->listView, 4, L"数量", 170);
        AddListViewColumn(state->listView, 5, L"空白", 170);
        AddListViewColumn(state->listView, 6, L"空白", 154);

        for (size_t i = 0; i < state->candidates->size(); ++i)
        {
            const BatchPartCandidate& candidate = (*state->candidates)[i];
            LVITEMW item = {};
            item.mask = LVIF_TEXT;
            item.iItem = static_cast<int>(i);
            item.iSubItem = 0;
            item.pszText = const_cast<wchar_t*>(L"");
            ListView_InsertItem(state->listView, &item);
            bool checked = true;
            if (state->persistedState != NULL && state->persistedState->hasCheckedPartNames)
            {
                checked = state->persistedState->checkedPartNames.find(candidate.partName) != state->persistedState->checkedPartNames.end();
            }
            ListView_SetCheckState(state->listView, static_cast<int>(i), checked ? TRUE : FALSE);

            SetListViewText(state->listView, static_cast<int>(i), 1, std::to_wstring(i + 1));
            SetListViewText(state->listView, static_cast<int>(i), 2, PathTextToWide(candidate.partName));
        }

        RefreshBatchPartListAttributeColumns(state);
    }

    void PopulateAttributeCombo(HWND comboBox, const std::vector<std::string>& attributeNames, const std::string& selectedValue)
    {
        if (comboBox == NULL)
        {
            return;
        }

        for (size_t i = 0; i < attributeNames.size(); ++i)
        {
            const std::wstring display = ListAttributeTitle(attributeNames[i]);
            SendMessageW(comboBox, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(display.c_str()));
        }

        int selectedIndex = 0;
        for (size_t i = 0; i < attributeNames.size(); ++i)
        {
            if (attributeNames[i] == selectedValue)
            {
                selectedIndex = static_cast<int>(i);
                break;
            }
        }
        SendMessageW(comboBox, CB_SETCURSEL, selectedIndex, 0);
        SendMessageW(comboBox, CB_SETMINVISIBLE, 12, 0);
        InvalidateRect(comboBox, NULL, TRUE);
    }

    std::string AttributeDefaultIfPresent(const std::vector<std::string>& attributeNames, const std::string& preferred)
    {
        for (size_t i = 0; i < attributeNames.size(); ++i)
        {
            if (_stricmp(attributeNames[i].c_str(), preferred.c_str()) == 0)
            {
                return attributeNames[i];
            }
        }
        return std::string();
    }

    std::string BatchPartPickerStatePath()
    {
        return std::string("D:\\UG智辉钣金插件\\config\\PiLianZuanBanJin_part_picker_state.ini");
    }

    std::string EncodePickerStateValue(const std::string& value)
    {
        std::ostringstream stream;
        for (size_t i = 0; i < value.size(); ++i)
        {
            const unsigned char ch = static_cast<unsigned char>(value[i]);
            if (ch == '%' || ch == '\n' || ch == '\r' || ch == '=' || ch == '|')
            {
                stream << '%' << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(ch)
                       << std::nouppercase << std::dec << std::setfill(' ');
            }
            else
            {
                stream << value[i];
            }
        }
        return stream.str();
    }

    int HexDigitValue(char ch)
    {
        if (ch >= '0' && ch <= '9') return ch - '0';
        if (ch >= 'A' && ch <= 'F') return 10 + ch - 'A';
        if (ch >= 'a' && ch <= 'f') return 10 + ch - 'a';
        return -1;
    }

    std::string DecodePickerStateValue(const std::string& value)
    {
        std::string decoded;
        for (size_t i = 0; i < value.size(); ++i)
        {
            if (value[i] == '%' && i + 2 < value.size())
            {
                const int hi = HexDigitValue(value[i + 1]);
                const int lo = HexDigitValue(value[i + 2]);
                if (hi >= 0 && lo >= 0)
                {
                    decoded.push_back(static_cast<char>((hi << 4) | lo));
                    i += 2;
                    continue;
                }
            }
            decoded.push_back(value[i]);
        }
        return decoded;
    }

    bool AttributeNameExistsInList(const std::vector<std::string>& attributeNames, const std::string& value)
    {
        if (value.empty())
        {
            return true;
        }

        for (size_t i = 0; i < attributeNames.size(); ++i)
        {
            if (_stricmp(attributeNames[i].c_str(), value.c_str()) == 0)
            {
                return true;
            }
        }
        return false;
    }

    BatchPartPickerPersistedState LoadBatchPartPickerState(const std::vector<std::string>& attributeNames)
    {
        BatchPartPickerPersistedState state;
        state.attributes.resize(BatchAttributeColumnCount);

        std::string text = ReadAllText(BatchPartPickerStatePath());
        std::stringstream input(text);
        std::string line;
        while (std::getline(input, line))
        {
            line = TrimCopy(line);
            if (line.empty() || line[0] == '#')
            {
                continue;
            }

            size_t equal = line.find('=');
            if (equal == std::string::npos)
            {
                continue;
            }

            std::string key = TrimCopy(line.substr(0, equal));
            std::string value = DecodePickerStateValue(TrimCopy(line.substr(equal + 1)));
            if (key.size() == 5 && key.find("attr") == 0)
            {
                int index = key[4] - '0';
                if (index >= 0 && index < BatchAttributeColumnCount && AttributeNameExistsInList(attributeNames, value))
                {
                    state.attributes[static_cast<size_t>(index)] = value;
                    state.hasAttributes = true;
                }
            }
            else if (key.find("part") == 0)
            {
                state.checkedPartNames.insert(value);
                state.hasCheckedPartNames = true;
            }
        }

        return state;
    }

    void SaveBatchPartPickerState(const BatchPartPickerState* state)
    {
        if (state == NULL || state->listView == NULL || state->candidates == NULL)
        {
            return;
        }

        std::ostringstream output;
        output << "# PiLianZuanBanJin part picker state\n";
        for (int i = 0; i < BatchAttributeColumnCount; ++i)
        {
            std::string value;
            if (i < static_cast<int>(state->selectedAttributes.size()))
            {
                value = state->selectedAttributes[static_cast<size_t>(i)];
            }
            output << "attr" << i << "=" << EncodePickerStateValue(value) << "\n";
        }

        int partIndex = 0;
        const int count = ListView_GetItemCount(state->listView);
        for (int i = 0; i < count && static_cast<size_t>(i) < state->candidates->size(); ++i)
        {
            if (ListView_GetCheckState(state->listView, i))
            {
                output << "part" << partIndex++ << "="
                       << EncodePickerStateValue((*state->candidates)[static_cast<size_t>(i)].partName)
                       << "\n";
            }
        }

        WriteAllText(BatchPartPickerStatePath(), output.str());
    }

    LRESULT CALLBACK BatchPartPickerWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        const int kIdList = 1001;
        const int kIdSelectAll = 1002;
        const int kIdClearAll = 1003;
        const int kIdFirstCombo = 1100;
        const int kIdOk = IDOK;
        const int kIdCancel = IDCANCEL;

        BatchPartPickerState* state = reinterpret_cast<BatchPartPickerState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        switch (message)
        {
        case WM_CREATE:
        {
            CREATESTRUCTW* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
            state = reinterpret_cast<BatchPartPickerState*>(createStruct->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
            if (state != NULL)
            {
                state->backgroundColor = RGB(236, 236, 236);
                state->backgroundBrush = CreateSolidBrush(state->backgroundColor);
                state->dialogFont = CreateNxLikeDialogFont();
                if (state->dialogFont != NULL)
                {
                    SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(state->dialogFont), TRUE);
                }
            }

            HINSTANCE instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(hwnd, GWLP_HINSTANCE));
            AddPickerControl(state, CreateWindowW(L"STATIC", L"选择要自动转钣金的部件：", WS_CHILD | WS_VISIBLE, 14, 12, 250, 22, hwnd, NULL, instance, NULL));
            if (state != NULL)
            {
                state->listX = 14;
                state->listY = 40;
                state->listWidth = BatchPickerListWidth;
                state->listHeight = 326;
            }
            state->listView = CreateWindowExW(
                WS_EX_CLIENTEDGE,
                WC_LISTVIEWW,
                L"",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SINGLESEL,
                state != NULL ? state->listX : 14,
                state != NULL ? state->listY : 40,
                state != NULL ? state->listWidth : BatchPickerListWidth,
                state != NULL ? state->listHeight : 326,
                hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdList)),
                instance,
                NULL);
            AddPickerControl(state, state->listView);
            ApplyNxLikeListViewStyle(state->listView);
            PopulateBatchPartListView(state);
            for (int i = 0; i < BatchAttributeColumnCount; ++i)
            {
                state->comboBoxes[i] = CreateWindowW(
                    WC_COMBOBOXW,
                    L"",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST,
                    0,
                    0,
                    80,
                    BatchAttributeComboDropHeight,
                    hwnd,
                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdFirstCombo + i)),
                    instance,
                    NULL);
                AddPickerControl(state, state->comboBoxes[i]);
                if (state->attributeNames != NULL && i < static_cast<int>(state->selectedAttributes.size()))
                {
                    PopulateAttributeCombo(state->comboBoxes[i], *state->attributeNames, state->selectedAttributes[static_cast<size_t>(i)]);
                }
            }
            RepositionHeaderCombos(state);
            for (int i = 0; i < BatchAttributeColumnCount; ++i)
            {
                if (state != NULL && state->comboBoxes[i] != NULL)
                {
                    BringWindowToTop(state->comboBoxes[i]);
                }
            }

            AddPickerControl(state, CreateWindowW(L"BUTTON", L"全选", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 14, 380, 76, 26, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdSelectAll)), instance, NULL));
            AddPickerControl(state, CreateWindowW(L"BUTTON", L"全不选", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 96, 380, 84, 26, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdClearAll)), instance, NULL));
        AddPickerControl(state, CreateWindowW(L"BUTTON", L"确定", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, 860, 380, 76, 26, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdOk)), instance, NULL));
        AddPickerControl(state, CreateWindowW(L"BUTTON", L"取消", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 946, 380, 76, 26, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdCancel)), instance, NULL));
            return 0;
        }
        case WM_NOTIFY:
        {
            if (state != NULL)
            {
                NMHDR* header = reinterpret_cast<NMHDR*>(lParam);
                if (header != NULL && header->hwndFrom == ListView_GetHeader(state->listView))
                {
                    switch (header->code)
                    {
                    case HDN_ENDTRACKW:
                    case HDN_ENDTRACKA:
                    case HDN_ITEMCHANGEDW:
                    case HDN_ITEMCHANGEDA:
                        RepositionHeaderCombos(state);
                        break;
                    default:
                        break;
                    }
                }
            }
            break;
        }
        case WM_CTLCOLORSTATIC:
        {
            if (state != NULL && state->backgroundBrush != NULL)
            {
                SetBkColor(reinterpret_cast<HDC>(wParam), state->backgroundColor);
                SetTextColor(reinterpret_cast<HDC>(wParam), RGB(30, 30, 30));
                return reinterpret_cast<LRESULT>(state->backgroundBrush);
            }
            break;
        }
        case WM_COMMAND:
        {
            if (state == NULL)
            {
                break;
            }

            const int commandId = LOWORD(wParam);
            const int notifyCode = HIWORD(wParam);
            if (commandId >= kIdFirstCombo && commandId < kIdFirstCombo + BatchAttributeColumnCount && notifyCode == CBN_SELCHANGE)
            {
                const int comboIndex = commandId - kIdFirstCombo;
                const int selectedIndex = static_cast<int>(SendMessageW(state->comboBoxes[comboIndex], CB_GETCURSEL, 0, 0));
                if (state->attributeNames != NULL &&
                    selectedIndex >= 0 &&
                    static_cast<size_t>(selectedIndex) < state->attributeNames->size() &&
                    comboIndex < static_cast<int>(state->selectedAttributes.size()))
                {
                    state->selectedAttributes[static_cast<size_t>(comboIndex)] = (*state->attributeNames)[static_cast<size_t>(selectedIndex)];
                    RefreshBatchPartListAttributeColumns(state);
                }
                return 0;
            }

            if (commandId == kIdSelectAll || commandId == kIdClearAll)
            {
                const BOOL checked = commandId == kIdSelectAll ? TRUE : FALSE;
                const int count = ListView_GetItemCount(state->listView);
                for (int i = 0; i < count; ++i)
                {
                    ListView_SetCheckState(state->listView, i, checked);
                }
                return 0;
            }

            if (commandId == kIdOk)
            {
                state->selectedIndices.clear();
                const int count = ListView_GetItemCount(state->listView);
                for (int i = 0; i < count; ++i)
                {
                    if (ListView_GetCheckState(state->listView, i))
                    {
                        state->selectedIndices.push_back(i);
                    }
                }
                SaveBatchPartPickerState(state);
                state->accepted = true;
                DestroyWindow(hwnd);
                return 0;
            }

            if (commandId == kIdCancel)
            {
                state->accepted = false;
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        }
        case WM_SHOWWINDOW:
            if (wParam)
            {
                RepositionHeaderCombos(state);
                for (int i = 0; state != NULL && i < BatchAttributeColumnCount; ++i)
                {
                    if (state->comboBoxes[i] != NULL)
                    {
                        BringWindowToTop(state->comboBoxes[i]);
                        InvalidateRect(state->comboBoxes[i], NULL, TRUE);
                    }
                }
            }
            break;
        case WM_HSCROLL:
        case WM_SIZE:
            RepositionHeaderCombos(state);
            break;
        case WM_CLOSE:
            if (state != NULL)
            {
                state->accepted = false;
            }
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            if (state != NULL)
            {
                if (state->dialogFont != NULL)
                {
                    DeleteObject(state->dialogFont);
                    state->dialogFont = NULL;
                }
                if (state->backgroundBrush != NULL)
                {
                    DeleteObject(state->backgroundBrush);
                    state->backgroundBrush = NULL;
                }
            }
            return 0;
        default:
            break;
        }

        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    bool ShowBatchPartPicker(const std::vector<BatchPartCandidate>& candidates, std::vector<Part*>& selectedParts)
    {
        selectedParts.clear();
        if (candidates.empty())
        {
            return false;
        }

        INITCOMMONCONTROLSEX controls = {};
        controls.dwSize = sizeof(controls);
        controls.dwICC = ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES;
        InitCommonControlsEx(&controls);

        const wchar_t* className = L"PiLianZuanBanJinBatchPartPicker";
        HINSTANCE instance = reinterpret_cast<HINSTANCE>(&__ImageBase);
        WNDCLASSEXW windowClass = {};
        windowClass.cbSize = sizeof(windowClass);
        windowClass.lpfnWndProc = BatchPartPickerWndProc;
        windowClass.hInstance = instance;
        windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
        windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
        windowClass.lpszClassName = className;
        RegisterClassExW(&windowClass);

        std::vector<std::string> attributeNames = BuildBatchPartAttributeNames(candidates);
        BatchPartPickerPersistedState persistedState = LoadBatchPartPickerState(attributeNames);

        BatchPartPickerState state = {};
        state.candidates = &candidates;
        state.attributeNames = &attributeNames;
        state.persistedState = &persistedState;
        state.selectedAttributes.resize(BatchAttributeColumnCount);
        state.selectedAttributes[0] = persistedState.hasAttributes ? persistedState.attributes[0] : AttributeDefaultIfPresent(attributeNames, u8"材料");
        state.selectedAttributes[1] = persistedState.hasAttributes ? persistedState.attributes[1] : AttributeDefaultIfPresent(attributeNames, u8"数量");
        state.selectedAttributes[2] = persistedState.hasAttributes ? persistedState.attributes[2] : std::string();
        state.selectedAttributes[3] = persistedState.hasAttributes ? persistedState.attributes[3] : std::string();
        state.listView = NULL;
        for (int i = 0; i < BatchAttributeColumnCount; ++i)
        {
            state.comboBoxes[i] = NULL;
        }
        state.dialogFont = NULL;
        state.backgroundBrush = NULL;
        state.backgroundColor = RGB(236, 236, 236);
        state.accepted = false;

        HWND parent = reinterpret_cast<HWND>(UF_UI_get_default_parent());
        HWND hwnd = CreateWindowExW(
            WS_EX_DLGMODALFRAME,
            className,
            L"批量转钣金部件列表",
            WS_CAPTION | WS_SYSMENU | WS_BORDER,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            BatchPickerWindowWidth,
            456,
            parent,
            NULL,
            instance,
            &state);
        if (hwnd == NULL)
        {
            return false;
        }

        CenterWindowOnParent(hwnd, parent);
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);

        MSG msg;
        while (IsWindow(hwnd) && GetMessageW(&msg, NULL, 0, 0) > 0)
        {
            if (!IsDialogMessageW(hwnd, &msg))
            {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }

        if (!state.accepted || state.selectedIndices.empty())
        {
            return false;
        }

        for (size_t i = 0; i < state.selectedIndices.size(); ++i)
        {
            const int index = state.selectedIndices[i];
            if (index >= 0 && static_cast<size_t>(index) < candidates.size() && candidates[static_cast<size_t>(index)].part != NULL)
            {
                selectedParts.push_back(candidates[static_cast<size_t>(index)].part);
            }
        }

        return !selectedParts.empty();
    }

    bool ConfirmAssemblyBatch(UI* ui)
    {
        if (ui == NULL)
        {
            return true;
        }

        const std::string message =
            "准备对本工作装配里的所有子装配进行批量转钣金跟展开处理。\n\n"
            "执行前请确保所有要转钣金的部件都已经设置了材料跟数量，"
            "并且零件已经全部处理成可成功转钣金跟展开的状态。\n\n"
            "继续执行请选择“是”，退出请选择“否”。";
        return ui->NXMessageBox()->Show(
            U8("批量转钣金"),
            NXMessageBox::DialogTypeQuestion,
            U8(message.c_str())) == 1;
    }

    NXString U8(const char* text)
    {
        return NXString(text == NULL ? "" : text, NXString::UTF8);
    }

    NXString U8(const std::string& text)
    {
        return NXString(text, NXString::UTF8);
    }

    std::wstring PathTextToWide(const std::string& text)
    {
        if (text.empty())
        {
            return std::wstring();
        }

        int wideLength = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, NULL, 0);
        UINT codePage = CP_UTF8;
        if (wideLength <= 0)
        {
            codePage = CP_ACP;
            wideLength = MultiByteToWideChar(codePage, 0, text.c_str(), -1, NULL, 0);
        }
        if (wideLength <= 0)
        {
            return std::wstring();
        }

        std::wstring wide(static_cast<size_t>(wideLength), L'\0');
        MultiByteToWideChar(codePage, 0, text.c_str(), -1, &wide[0], wideLength);
        if (!wide.empty() && wide[wide.size() - 1] == L'\0')
        {
            wide.resize(wide.size() - 1);
        }
        return wide;
    }

    std::string WidePathToUtf8(const std::wstring& text)
    {
        if (text.empty())
        {
            return std::string();
        }

        int length = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, NULL, 0, NULL, NULL);
        if (length <= 0)
        {
            return std::string();
        }

        std::string utf8(static_cast<size_t>(length), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, &utf8[0], length, NULL, NULL);
        if (!utf8.empty() && utf8[utf8.size() - 1] == '\0')
        {
            utf8.resize(utf8.size() - 1);
        }
        return utf8;
    }

    std::string ReadAllText(const std::string& path)
    {
        std::wstring widePath = PathTextToWide(path);
        FILE* file = NULL;
        if (widePath.empty() || _wfopen_s(&file, widePath.c_str(), L"rb") != 0 || file == NULL)
        {
            return std::string();
        }

        std::string content;
        char buffer[4096];
        size_t bytesRead = 0;
        while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0)
        {
            content.append(buffer, bytesRead);
        }
        fclose(file);
        return content;
    }

    bool WriteAllText(const std::string& path, const std::string& text)
    {
        std::wstring widePath = PathTextToWide(path);
        FILE* file = NULL;
        if (widePath.empty() || _wfopen_s(&file, widePath.c_str(), L"wb") != 0 || file == NULL)
        {
            return false;
        }

        size_t written = fwrite(text.c_str(), 1, text.size(), file);
        fclose(file);
        return written == text.size();
    }

    void AppendMarkerLineDebugLog(const std::string& text)
    {
        const std::string path = "D:\\UG智辉钣金插件\\logs\\PiLianZuanBanJin_marker_line.log";
        CreateDirectoryW(L"D:\\UG智辉钣金插件\\logs", NULL);
        std::wstring widePath = PathTextToWide(path);
        FILE* file = NULL;
        if (widePath.empty() || _wfopen_s(&file, widePath.c_str(), L"ab") != 0 || file == NULL)
        {
            return;
        }

        SYSTEMTIME time = {};
        GetLocalTime(&time);
        char prefix[64] = { 0 };
        sprintf_s(prefix,
            "[%04d-%02d-%02d %02d:%02d:%02d.%03d] ",
            time.wYear,
            time.wMonth,
            time.wDay,
            time.wHour,
            time.wMinute,
            time.wSecond,
            time.wMilliseconds);

        fwrite(prefix, 1, strlen(prefix), file);
        fwrite(text.c_str(), 1, text.size(), file);
        fwrite("\r\n", 1, 2, file);
        fclose(file);
    }

    std::string ModuleDirectory()
    {
        char path[MAX_PATH] = { 0 };
        HMODULE module = NULL;
        GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(&ModuleDirectory),
            &module);
        GetModuleFileNameA(module, path, MAX_PATH);
        std::string fullPath(path);
        size_t slash = fullPath.find_last_of("\\/");
        return slash == std::string::npos ? std::string(".") : fullPath.substr(0, slash);
    }

    std::string CombinePath(const std::string& left, const std::string& right)
    {
        if (left.empty())
        {
            return right;
        }

        char tail = left[left.size() - 1];
        return (tail == '\\' || tail == '/') ? left + right : left + "\\" + right;
    }

    std::string ParentDirectory(const std::string& path)
    {
        size_t slash = path.find_last_of("\\/");
        return slash == std::string::npos ? std::string() : path.substr(0, slash);
    }

    bool FileExists(const std::string& path)
    {
        std::wstring widePath = PathTextToWide(path);
        if (widePath.empty())
        {
            return false;
        }
        DWORD attrs = GetFileAttributesW(widePath.c_str());
        return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }

    std::string FindConfigPath()
    {
        std::string path = std::string("D:\\UG智辉钣金插件\\config\\") + PiLianConfigFileName;
        return FileExists(path) ? path : std::string();
    }

    std::string FindRulesIniPath()
    {
        std::string path = std::string("D:\\UG智辉钣金插件\\config\\") + PiLianRulesIniFileName;
        return FileExists(path) ? path : std::string();
    }

    std::string FindRulesTableDlxPath()
    {
        return zhihui_embedded_dialog::ExtractDlxToRandomPath(
            IDR_ZH_DLX_PILIANZUANBANJIN_RULES_TABLE_DLX,
            L"PiLianZuanBanJinRulesTable.dlx");
    }


    bool ExtractJsonBool(const std::string& text, const std::string& name, bool defaultValue)
    {
        std::regex pattern("\"" + name + "\"\\s*:\\s*(true|false)", std::regex_constants::icase);
        std::smatch match;
        if (std::regex_search(text, match, pattern))
        {
            return _stricmp(match[1].str().c_str(), "true") == 0;
        }

        return defaultValue;
    }

    double ExtractJsonNumber(const std::string& text, const std::string& name, double defaultValue)
    {
        std::regex pattern("\"" + name + "\"\\s*:\\s*(-?\\d+(?:\\.\\d+)?(?:[eE][+-]?\\d+)?|null)");
        std::smatch match;
        if (std::regex_search(text, match, pattern))
        {
            std::string value = match[1].str();
            if (value != "null" && value != "NULL")
            {
                return std::atof(value.c_str());
            }
        }

        return defaultValue;
    }

    bool TryExtractJsonNumber(const std::string& text, const std::string& name, double* value)
    {
        std::regex pattern("\"" + name + "\"\\s*:\\s*(-?\\d+(?:\\.\\d+)?(?:[eE][+-]?\\d+)?|null)");
        std::smatch match;
        if (std::regex_search(text, match, pattern))
        {
            std::string raw = match[1].str();
            if (raw != "null" && raw != "NULL")
            {
                *value = std::atof(raw.c_str());
                return true;
            }
        }

        return false;
    }

    std::string ExtractJsonString(const std::string& text, const std::string& name, const std::string& defaultValue)
    {
        std::regex pattern("\"" + name + "\"\\s*:\\s*\"([^\"]*)\"");
        std::smatch match;
        return std::regex_search(text, match, pattern) ? match[1].str() : defaultValue;
    }

    std::string ExtractArrayBody(const std::string& text, const std::string& name)
    {
        std::string token = "\"" + name + "\"";
        size_t pos = text.find(token);
        if (pos == std::string::npos)
        {
            return std::string();
        }

        size_t start = text.find('[', pos);
        if (start == std::string::npos)
        {
            return std::string();
        }

        int depth = 0;
        bool inString = false;
        bool escape = false;
        for (size_t i = start; i < text.size(); ++i)
        {
            char c = text[i];
            if (escape)
            {
                escape = false;
                continue;
            }
            if (c == '\\' && inString)
            {
                escape = true;
                continue;
            }
            if (c == '"')
            {
                inString = !inString;
                continue;
            }
            if (inString)
            {
                continue;
            }
            if (c == '[')
            {
                ++depth;
            }
            else if (c == ']')
            {
                --depth;
                if (depth == 0)
                {
                    return text.substr(start + 1, i - start - 1);
                }
            }
        }

        return std::string();
    }

    std::vector<std::string> ExtractObjectBodies(const std::string& arrayBody)
    {
        std::vector<std::string> objects;
        int depth = 0;
        bool inString = false;
        bool escape = false;
        size_t start = std::string::npos;
        for (size_t i = 0; i < arrayBody.size(); ++i)
        {
            char c = arrayBody[i];
            if (escape)
            {
                escape = false;
                continue;
            }
            if (c == '\\' && inString)
            {
                escape = true;
                continue;
            }
            if (c == '"')
            {
                inString = !inString;
                continue;
            }
            if (inString)
            {
                continue;
            }
            if (c == '{')
            {
                if (depth == 0)
                {
                    start = i;
                }
                ++depth;
            }
            else if (c == '}')
            {
                --depth;
                if (depth == 0 && start != std::string::npos)
                {
                    objects.push_back(arrayBody.substr(start, i - start + 1));
                    start = std::string::npos;
                }
            }
        }

        return objects;
    }

    void SetLabel(UIBlock* block, const char* label)
    {
        if (block != NULL)
        {
            block->SetLabel(U8(label));
        }
    }

    void ShowMessage(UI* ui, const char* title, NXMessageBox::DialogType type, const std::string& message)
    {
        if (ui != NULL)
        {
            ui->NXMessageBox()->Show(U8(title), type, U8(message));
        }
    }

    std::string BodyName(Body* body)
    {
        if (body == NULL)
        {
            return std::string();
        }

        try
        {
            std::string name = ToUtf8(body->Name());
            if (!name.empty())
            {
                return name;
            }
        }
        catch (...)
        {
        }

        return "Body_" + std::to_string(static_cast<int>(body->Tag()));
    }

    bool IsAlive(tag_t tag)
    {
        return tag != NULL_TAG && UF_OBJ_ask_status(tag) == UF_OBJ_ALIVE;
    }

    BodyBox MeasureBodyBox(Body* body)
    {
        BodyBox result;
        if (body == NULL)
        {
            return result;
        }

        double box[6] = { 0, 0, 0, 0, 0, 0 };
        if (UF_MODL_ask_bounding_box(body->Tag(), box) != 0)
        {
            return result;
        }

        std::vector<double> dims;
        dims.push_back(std::fabs(box[3] - box[0]));
        dims.push_back(std::fabs(box[4] - box[1]));
        dims.push_back(std::fabs(box[5] - box[2]));
        std::sort(dims.begin(), dims.end(), std::greater<double>());
        result.length = dims[0];
        result.width = dims[1];
        result.height = dims[2];
        result.score = result.length * result.width * std::max(result.height, 0.001);
        return result;
    }

    void AddUniquePoint(std::vector<Point3d>* points, const Point3d& point)
    {
        if (points == NULL)
        {
            return;
        }

        for (size_t i = 0; i < points->size(); ++i)
        {
            if (std::fabs((*points)[i].X - point.X) < 1e-5 &&
                std::fabs((*points)[i].Y - point.Y) < 1e-5 &&
                std::fabs((*points)[i].Z - point.Z) < 1e-5)
            {
                return;
            }
        }

        points->push_back(point);
    }

    std::vector<Point3d> CollectBodyEdgePoints(Body* body)
    {
        std::vector<Point3d> points;
        if (body == NULL)
        {
            return points;
        }

        std::vector<Face*> faces = body->GetFaces();
        std::set<tag_t> seenEdges;
        for (size_t i = 0; i < faces.size(); ++i)
        {
            std::vector<Edge*> edges = FaceEdgesByUf(faces[i]);
            for (size_t j = 0; j < edges.size(); ++j)
            {
                Edge* edge = edges[j];
                if (edge == NULL || !seenEdges.insert(edge->Tag()).second)
                {
                    continue;
                }

                Point3d p1;
                Point3d p2;
                try
                {
                    edge->GetVertices(&p1, &p2);
                }
                catch (...)
                {
                    continue;
                }

                AddUniquePoint(&points, p1);
                AddUniquePoint(&points, p2);
            }
        }

        return points;
    }

    double Cross2(const Point2dLite& origin, const Point2dLite& a, const Point2dLite& b)
    {
        return (a.x - origin.x) * (b.y - origin.y) - (a.y - origin.y) * (b.x - origin.x);
    }

    double Distance2(const Point2dLite& a, const Point2dLite& b)
    {
        const double dx = a.x - b.x;
        const double dy = a.y - b.y;
        return std::sqrt(dx * dx + dy * dy);
    }

    void AddUniquePoint2(std::vector<Point2dLite>* points, const Point2dLite& point)
    {
        if (points == NULL)
        {
            return;
        }

        for (size_t i = 0; i < points->size(); ++i)
        {
            if (std::fabs((*points)[i].x - point.x) < 1e-5 &&
                std::fabs((*points)[i].y - point.y) < 1e-5)
            {
                return;
            }
        }

        points->push_back(point);
    }

    std::vector<Point2dLite> ConvexHull2(std::vector<Point2dLite> points)
    {
        if (points.size() <= 2)
        {
            return points;
        }

        std::sort(points.begin(), points.end(), [](const Point2dLite& a, const Point2dLite& b) {
            if (std::fabs(a.x - b.x) > 1e-7)
            {
                return a.x < b.x;
            }
            return a.y < b.y;
        });

        std::vector<Point2dLite> hull;
        for (size_t i = 0; i < points.size(); ++i)
        {
            while (hull.size() >= 2 && Cross2(hull[hull.size() - 2], hull[hull.size() - 1], points[i]) <= 1e-7)
            {
                hull.pop_back();
            }
            hull.push_back(points[i]);
        }

        const size_t lowerSize = hull.size();
        for (size_t i = points.size(); i-- > 0;)
        {
            while (hull.size() > lowerSize && Cross2(hull[hull.size() - 2], hull[hull.size() - 1], points[i]) <= 1e-7)
            {
                hull.pop_back();
            }
            hull.push_back(points[i]);
        }

        if (!hull.empty())
        {
            hull.pop_back();
        }
        return hull;
    }

    double PolygonMaxPairDistance(const std::vector<Point2dLite>& points)
    {
        double maxDistance = 0.0;
        for (size_t i = 0; i < points.size(); ++i)
        {
            for (size_t j = i + 1; j < points.size(); ++j)
            {
                maxDistance = std::max(maxDistance, Distance2(points[i], points[j]));
            }
        }
        return maxDistance;
    }

    bool IsCircularShadow(const std::vector<Point2dLite>& hull)
    {
        if (hull.size() < 8)
        {
            return false;
        }

        Point2dLite center;
        for (size_t i = 0; i < hull.size(); ++i)
        {
            center.x += hull[i].x;
            center.y += hull[i].y;
        }
        center.x /= static_cast<double>(hull.size());
        center.y /= static_cast<double>(hull.size());

        double minRadius = std::numeric_limits<double>::max();
        double maxRadius = 0.0;
        for (size_t i = 0; i < hull.size(); ++i)
        {
            const double radius = Distance2(center, hull[i]);
            minRadius = std::min(minRadius, radius);
            maxRadius = std::max(maxRadius, radius);
        }

        return maxRadius > 1e-6 && minRadius / maxRadius >= 0.82;
    }

    bool IsHexShadow(const std::vector<Point2dLite>& hull, double* acrossFlatsOut)
    {
        if (hull.size() != 6)
        {
            return false;
        }

        double minSide = std::numeric_limits<double>::max();
        double maxSide = 0.0;
        for (size_t i = 0; i < hull.size(); ++i)
        {
            const double side = Distance2(hull[i], hull[(i + 1) % hull.size()]);
            minSide = std::min(minSide, side);
            maxSide = std::max(maxSide, side);
        }

        if (maxSide <= 1e-6 || minSide / maxSide < 0.90)
        {
            return false;
        }

        Point2dLite center;
        for (size_t i = 0; i < hull.size(); ++i)
        {
            center.x += hull[i].x;
            center.y += hull[i].y;
        }
        center.x /= 6.0;
        center.y /= 6.0;

        double minRadius = std::numeric_limits<double>::max();
        double maxRadius = 0.0;
        for (size_t i = 0; i < hull.size(); ++i)
        {
            const double radius = Distance2(center, hull[i]);
            minRadius = std::min(minRadius, radius);
            maxRadius = std::max(maxRadius, radius);
        }

        if (maxRadius <= 1e-6 || minRadius / maxRadius < 0.90)
        {
            return false;
        }

        if (acrossFlatsOut != NULL)
        {
            *acrossFlatsOut = PolygonMaxPairDistance(hull) * 0.8660254037844386;
        }
        return true;
    }

    bool BuildAxisBasis(const double axisDirection[3], double u[3], double v[3])
    {
        double seed[3] = { 1.0, 0.0, 0.0 };
        if (std::fabs(Dot3(axisDirection, seed)) > 0.9)
        {
            seed[0] = 0.0;
            seed[1] = 1.0;
            seed[2] = 0.0;
        }

        const double projection = Dot3(seed, axisDirection);
        u[0] = seed[0] - projection * axisDirection[0];
        u[1] = seed[1] - projection * axisDirection[1];
        u[2] = seed[2] - projection * axisDirection[2];
        if (!Normalize3(u))
        {
            return false;
        }

        v[0] = axisDirection[1] * u[2] - axisDirection[2] * u[1];
        v[1] = axisDirection[2] * u[0] - axisDirection[0] * u[2];
        v[2] = axisDirection[0] * u[1] - axisDirection[1] * u[0];
        return Normalize3(v);
    }

    struct CircularEdgeRecord
    {
        double center[3] = { 0.0, 0.0, 0.0 };
        double radius = 0.0;
        double arcLength = 0.0;
        bool fullCircle = false;
    };

    bool AskCircularEdgeRecord(Edge* edge, CircularEdgeRecord* record)
    {
        if (edge == NULL || record == NULL)
        {
            return false;
        }

        UF_EVAL_p_t evaluator = NULL;
        if (UF_EVAL_initialize(edge->Tag(), &evaluator) != 0 || evaluator == NULL)
        {
            return false;
        }

        logical isArc = false;
        if (UF_EVAL_is_arc(evaluator, &isArc) != 0 || !isArc)
        {
            UF_EVAL_free(evaluator);
            return false;
        }

        UF_EVAL_arc_t arc = {};
        const int arcStatus = UF_EVAL_ask_arc(evaluator, &arc);
        UF_EVAL_free(evaluator);
        if (arcStatus != 0 || arc.radius <= 1e-6)
        {
            return false;
        }

        record->center[0] = arc.center[0];
        record->center[1] = arc.center[1];
        record->center[2] = arc.center[2];
        record->radius = arc.radius;
        record->fullCircle = arc.is_periodic != 0;
        record->arcLength = 0.0;
        try
        {
            record->arcLength = edge->GetLength();
        }
        catch (...)
        {
            UF_CURVE_ask_arc_length(edge->Tag(), 0.0, 1.0, UF_MODL_UNITS_PART, &record->arcLength);
        }
        return true;
    }

    bool SameCircleRecord(const CircularEdgeRecord& a, const CircularEdgeRecord& b, double radiusTolerance)
    {
        return std::fabs(a.radius - b.radius) <= radiusTolerance &&
            Distance3(a.center, b.center) <= std::max(0.05, radiusTolerance * 2.0);
    }

    bool IsHalfCircleRecord(const CircularEdgeRecord& record)
    {
        if (record.radius <= 1e-6)
        {
            return false;
        }

        const double halfLength = 3.14159265358979323846 * record.radius;
        return std::fabs(record.arcLength - halfLength) <= std::max(0.05, halfLength * 0.08);
    }

    bool CylinderFaceHasFullCircleProfile(Face* face, double cylinderRadius, CircleProfileStats* stats)
    {
        if (stats != NULL)
        {
            *stats = CircleProfileStats();
        }
        if (face == NULL || cylinderRadius <= 1e-6)
        {
            return false;
        }

        const double radiusTolerance = std::max(0.05, cylinderRadius * 0.03);
        std::vector<CircularEdgeRecord> halfCircles;
        std::vector<Edge*> edges = FaceEdgesByUf(face);
        for (size_t i = 0; i < edges.size(); ++i)
        {
            CircularEdgeRecord record;
            if (!AskCircularEdgeRecord(edges[i], &record))
            {
                continue;
            }
            if (stats != NULL)
            {
                ++stats->circularEdgeCount;
                stats->lastRadius = record.radius;
            }
            if (std::fabs(record.radius - cylinderRadius) > radiusTolerance)
            {
                continue;
            }
            if (record.fullCircle)
            {
                if (stats != NULL)
                {
                    ++stats->fullCircleEdgeCount;
                }
                return true;
            }
            if (IsHalfCircleRecord(record))
            {
                if (stats != NULL)
                {
                    ++stats->halfCircleEdgeCount;
                }
                halfCircles.push_back(record);
            }
        }

        for (size_t i = 0; i < halfCircles.size(); ++i)
        {
            for (size_t j = i + 1; j < halfCircles.size(); ++j)
            {
                if (SameCircleRecord(halfCircles[i], halfCircles[j], radiusTolerance))
                {
                    return true;
                }
            }
        }

        return false;
    }

    bool CollectBodyShadowOutlineByNx(Body* body, const double axisPoint[3], const double axisDirection[3], ShadowOutlineInfo* info)
    {
        if (info != NULL)
        {
            *info = ShadowOutlineInfo();
        }
        if (body == NULL || axisPoint == NULL || axisDirection == NULL)
        {
            if (info != NULL) info->reason = "阴影输入为空";
            return false;
        }

        double u[3] = { 1.0, 0.0, 0.0 };
        double v[3] = { 0.0, 1.0, 0.0 };
        if (!BuildAxisBasis(axisDirection, u, v))
        {
            if (info != NULL) info->reason = "圆柱轴向无法创建投影基准";
            return false;
        }

        Session* session = Session::GetSession();
        Part* workPart = session != NULL ? session->Parts()->Work() : NULL;
        if (workPart == NULL)
        {
            if (info != NULL) info->reason = "没有工作部件，无法创建阴影曲线";
            return false;
        }

        int oldDisplayState = UF_DISP_UNSUPPRESS_DISPLAY;
        const bool displayStateKnown = UF_DISP_ask_display(&oldDisplayState) == 0;
        UF_DISP_set_display(UF_DISP_SUPPRESS_DISPLAY);

        Features::ShadowCurveBuilder* builder = NULL;
        Plane* plane = NULL;
        Direction* rayDirection = NULL;
        Direction* upDirection = NULL;
        SelectionIntentRuleOptions* ruleOptions = NULL;
        NXObject* committedObject = NULL;
        std::vector<NXObject*> committedObjects;
        std::vector<tag_t> deleteTags;
        bool ok = false;

        try
        {
            const Point3d origin(axisPoint[0], axisPoint[1], axisPoint[2]);
            const Vector3d normal(axisDirection[0], axisDirection[1], axisDirection[2]);
            plane = workPart->Planes()->CreatePlane(origin, normal, SmartObject::UpdateOptionWithinModeling);
            rayDirection = workPart->Directions()->CreateDirection(origin, normal, SmartObject::UpdateOptionWithinModeling);
            upDirection = workPart->Directions()->CreateDirection(origin, Vector3d(u[0], u[1], u[2]), SmartObject::UpdateOptionWithinModeling);

            builder = workPart->Features()->CurveFeatureCollection()->CreateShadowCurveBuilder(NULL);
            builder->SetLightSourceType(Features::ShadowCurveBuilder::LightSourceTypesVector);
            builder->SetMaskType(Features::ShadowCurveBuilder::MaskTypesBodies);
            builder->SetAccuracyType(Features::ShadowCurveBuilder::AccuracyTypesStandard);
            builder->SetMaskBodyProcessingTypes(Features::ShadowCurveBuilder::MaskBodyProcessingTypeMorePreciseResult);
            builder->SetCurveLocationType(Features::ShadowCurveBuilder::CurveLocationTypesShadowonPlane);
            builder->SetCurveLocationPlane(plane);
            builder->SetRayDirection(rayDirection);
            builder->SetUpVector(upDirection);
            builder->SetEnableShadowRange(false);

            if (builder->CurveSettings() != NULL && builder->CurveSettings()->CurveFitData() != NULL)
            {
                builder->CurveSettings()->CurveFitData()->SetTolerance(0.01);
                builder->CurveSettings()->CurveFitData()->SetAngleTolerance(0.5);
                builder->CurveSettings()->CurveFitData()->SetCurveJoinMethod(GeometricUtilities::CurveFitData::JoinNo);
            }

            if (builder->Angle() != NULL)
            {
                builder->Angle()->SetFormula("40");
            }
            if (builder->WidthAngle() != NULL)
            {
                builder->WidthAngle()->SetFormula("40");
            }
            if (builder->VerticalAngle() != NULL)
            {
                builder->VerticalAngle()->SetFormula("20");
            }
            if (builder->ShadowRangeOnPlane() != NULL)
            {
                builder->ShadowRangeOnPlane()->SetFormula("5000");
            }
            if (builder->SphereDiameter() != NULL)
            {
                builder->SphereDiameter()->SetFormula("10000");
            }
            if (builder->MaskingCurves() != NULL)
            {
                builder->MaskingCurves()->SetDistanceTolerance(0.01);
                builder->MaskingCurves()->SetChainingTolerance(0.0095);
                builder->MaskingCurves()->SetAngleTolerance(0.5);
            }

            ruleOptions = workPart->ScRuleFactory()->CreateRuleOptions();
            ruleOptions->SetSelectedFromInactive(false);
            std::vector<Body*> bodies(1);
            bodies[0] = body;
            BodyDumbRule* bodyRule = workPart->ScRuleFactory()->CreateRuleBodyDumb(bodies, true, ruleOptions);
            std::vector<SelectionIntentRule*> rules(1);
            rules[0] = bodyRule;
            builder->MaskingBodies()->ReplaceRules(rules, false);

            committedObject = builder->Commit();
            committedObjects = builder->GetCommittedObjects();

            std::vector<tag_t> curveTags;
            for (size_t i = 0; i < committedObjects.size(); ++i)
            {
                if (committedObjects[i] != NULL)
                {
                    curveTags.push_back(committedObjects[i]->Tag());
                }
            }

            Features::Feature* feature = dynamic_cast<Features::Feature*>(committedObject);
            if (feature != NULL)
            {
                std::vector<NXObject*> entities = feature->GetEntities();
                for (size_t i = 0; i < entities.size(); ++i)
                {
                    if (entities[i] != NULL)
                    {
                        curveTags.push_back(entities[i]->Tag());
                    }
                }
            }

            std::sort(curveTags.begin(), curveTags.end());
            curveTags.erase(std::unique(curveTags.begin(), curveTags.end()), curveTags.end());

            std::vector<Point2dLite> lineEndpoints;
            double maxCircleDiameter = 0.0;
            int lineCount = 0;
            int circleCount = 0;
            int splineCount = 0;
            int otherCount = 0;
            for (size_t i = 0; i < curveTags.size(); ++i)
            {
                int type = 0;
                int subtype = 0;
                if (UF_OBJ_ask_type_and_subtype(curveTags[i], &type, &subtype) != 0)
                {
                    continue;
                }
                if (type == UF_line_type)
                {
                    ++lineCount;
                    UF_CURVE_line_t lineData = {};
                    if (UF_CURVE_ask_line_data(curveTags[i], &lineData) == 0)
                    {
                        double p1[3] = { lineData.start_point[0] - axisPoint[0], lineData.start_point[1] - axisPoint[1], lineData.start_point[2] - axisPoint[2] };
                        double p2[3] = { lineData.end_point[0] - axisPoint[0], lineData.end_point[1] - axisPoint[1], lineData.end_point[2] - axisPoint[2] };
                        Point2dLite a;
                        a.x = Dot3(p1, u);
                        a.y = Dot3(p1, v);
                        Point2dLite b;
                        b.x = Dot3(p2, u);
                        b.y = Dot3(p2, v);
                        AddUniquePoint2(&lineEndpoints, a);
                        AddUniquePoint2(&lineEndpoints, b);
                    }
                }
                else if (type == UF_circle_type)
                {
                    ++circleCount;
                    UF_CURVE_arc_t arcData = {};
                    if (UF_CURVE_ask_arc_data(curveTags[i], &arcData) == 0)
                    {
                        const double sweep = std::fabs(arcData.end_angle - arcData.start_angle);
                        if (sweep >= 6.20 && arcData.radius > 1e-6)
                        {
                            maxCircleDiameter = std::max(maxCircleDiameter, arcData.radius * 2.0);
                        }
                    }
                }
                else if (type == UF_spline_type)
                {
                    ++splineCount;
                }
                else
                {
                    ++otherCount;
                }
            }

            if (info != NULL)
            {
                info->curveCount = static_cast<int>(curveTags.size());
                info->lineCount = lineCount;
                info->circleCount = circleCount;
                info->splineCount = splineCount;
                info->otherCount = otherCount;
            }

            if (maxCircleDiameter > 1e-6)
            {
                if (info != NULL)
                {
                    info->matched = true;
                    info->outlineType = "circle";
                    info->outlineSize = maxCircleDiameter;
                    info->reason = "阴影曲线最大外形为圆";
                }
                ok = true;
            }
            else
            {
                std::vector<Point2dLite> hull = ConvexHull2(lineEndpoints);
                double acrossFlats = 0.0;
                if (info != NULL)
                {
                    info->hullPointCount = hull.size();
                }
                if (IsHexShadow(hull, &acrossFlats) && acrossFlats > 1e-6)
                {
                    if (info != NULL)
                    {
                        info->matched = true;
                        info->outlineType = "hex";
                        info->outlineSize = acrossFlats;
                        info->reason = "阴影曲线最大外形为等六边形";
                    }
                    ok = true;
                }
                else if (info != NULL)
                {
                    info->outlineType = "other";
                    info->reason = curveTags.empty() ? "阴影曲线没有输出曲线" : "阴影曲线最大外形不是圆或等六边形";
                }
            }
        }
        catch (const NXException& ex)
        {
            if (info != NULL)
            {
                std::ostringstream stream;
                stream << "创建阴影曲线异常 code=" << ex.ErrorCode() << " msg=" << ex.Message();
                if (builder != NULL)
                {
                    stream << " distance_threshold=" << builder->DistanceThreshold();
                    if (builder->MaskingCurves() != NULL)
                    {
                        stream << " distance_tol=" << builder->MaskingCurves()->DistanceTolerance()
                               << " chaining_tol=" << builder->MaskingCurves()->ChainingTolerance();
                    }
                }
                info->reason = stream.str();
            }
            ok = false;
        }
        catch (...)
        {
            if (info != NULL) info->reason = "创建阴影曲线异常";
            ok = false;
        }

        if (builder != NULL)
        {
            try { builder->Destroy(); } catch (...) {}
        }

        for (size_t i = 0; i < committedObjects.size(); ++i)
        {
            if (committedObjects[i] != NULL)
            {
                deleteTags.push_back(committedObjects[i]->Tag());
            }
        }
        if (committedObject != NULL) deleteTags.push_back(committedObject->Tag());
        if (plane != NULL) deleteTags.push_back(plane->Tag());
        if (rayDirection != NULL) deleteTags.push_back(rayDirection->Tag());
        if (upDirection != NULL) deleteTags.push_back(upDirection->Tag());
        std::sort(deleteTags.begin(), deleteTags.end());
        deleteTags.erase(std::unique(deleteTags.begin(), deleteTags.end()), deleteTags.end());
        for (size_t i = 0; i < deleteTags.size(); ++i)
        {
            if (deleteTags[i] != NULL_TAG)
            {
                UF_OBJ_delete_object(deleteTags[i]);
            }
        }

        if (displayStateKnown)
        {
            UF_DISP_set_display(oldDisplayState);
        }

        return ok;
    }

    bool ShouldSkipFastenerBody(Body* body, FastenerFilterInfo* info)
    {
        if (info != NULL)
        {
            *info = FastenerFilterInfo();
        }

        if (body == NULL)
        {
            if (info != NULL) info->reason = "空实体";
            return false;
        }

        FastenerFilterInfo bestInfo;
        bestInfo.reason = "没有外圆柱候选";
        double bestRatio = -1.0;
        std::vector<Face*> faces = body->GetFaces();
        for (size_t i = 0; i < faces.size(); ++i)
        {
            Face* face = faces[i];
            double axisPoint[3] = { 0.0, 0.0, 0.0 };
            double axisDirection[3] = { 0.0, 0.0, 1.0 };
            double radius = 0.0;
            int normDir = 0;
            if (!AskCylinderFaceData(face, axisPoint, axisDirection, &radius, &normDir))
            {
                continue;
            }
            ++bestInfo.cylinderFaceCount;
            double outerMed = 0.0;
            const bool hasOuterMed = AskOuterCylinderMedLike08(face, &outerMed);
            bestInfo.outerMed = outerMed;
            if (!hasOuterMed || outerMed >= 0.0)
            {
                continue;
            }
            ++bestInfo.outerCylinderFaceCount;
            CircleProfileStats circleStats;
            if (!CylinderFaceHasFullCircleProfile(face, radius, &circleStats))
            {
                FastenerFilterInfo circleInfo;
                circleInfo.outlineType = "none";
                circleInfo.cylinderDiameter = radius * 2.0;
                circleInfo.cylinderFaceCount = bestInfo.cylinderFaceCount;
                circleInfo.outerCylinderFaceCount = bestInfo.outerCylinderFaceCount;
                circleInfo.outerMed = outerMed;
                circleInfo.circularEdgeCount = circleStats.circularEdgeCount;
                circleInfo.fullCircleEdgeCount = circleStats.fullCircleEdgeCount;
                circleInfo.halfCircleEdgeCount = circleStats.halfCircleEdgeCount;
                circleInfo.reason = "圆柱直径不是整圆或两个半圆";
                if (bestRatio < 0.0)
                {
                    bestInfo = circleInfo;
                    bestRatio = 0.0;
                }
                continue;
            }

            double minProjection = std::numeric_limits<double>::max();
            double maxProjection = -std::numeric_limits<double>::max();
            std::vector<Point3d> heightPoints = CollectBodyEdgePoints(body);
            for (size_t j = 0; j < heightPoints.size(); ++j)
            {
                double point[3] = { heightPoints[j].X, heightPoints[j].Y, heightPoints[j].Z };
                double delta[3] = {
                    point[0] - axisPoint[0],
                    point[1] - axisPoint[1],
                    point[2] - axisPoint[2]
                };
                const double projection = Dot3(delta, axisDirection);
                minProjection = std::min(minProjection, projection);
                maxProjection = std::max(maxProjection, projection);
            }

            const double diameter = radius * 2.0;
            const double height = minProjection <= maxProjection ? maxProjection - minProjection : 0.0;
            ShadowOutlineInfo shadowInfo;
            if (!CollectBodyShadowOutlineByNx(body, axisPoint, axisDirection, &shadowInfo))
            {
                FastenerFilterInfo shadowFailInfo;
                shadowFailInfo.outlineType = shadowInfo.outlineType;
                shadowFailInfo.cylinderDiameter = diameter;
                shadowFailInfo.height = height;
                shadowFailInfo.outlineSize = shadowInfo.outlineSize;
                shadowFailInfo.threshold = shadowInfo.outlineSize * 0.3;
                shadowFailInfo.hullPointCount = shadowInfo.hullPointCount;
                shadowFailInfo.shadowCurveCount = shadowInfo.curveCount;
                shadowFailInfo.shadowLineCount = shadowInfo.lineCount;
                shadowFailInfo.shadowCircleCount = shadowInfo.circleCount;
                shadowFailInfo.shadowSplineCount = shadowInfo.splineCount;
                shadowFailInfo.shadowOtherCount = shadowInfo.otherCount;
                shadowFailInfo.cylinderFaceCount = bestInfo.cylinderFaceCount;
                shadowFailInfo.outerCylinderFaceCount = bestInfo.outerCylinderFaceCount;
                shadowFailInfo.outerMed = outerMed;
                shadowFailInfo.circularEdgeCount = circleStats.circularEdgeCount;
                shadowFailInfo.fullCircleEdgeCount = circleStats.fullCircleEdgeCount;
                shadowFailInfo.halfCircleEdgeCount = circleStats.halfCircleEdgeCount;
                shadowFailInfo.reason = shadowInfo.reason;
                const double ratio = shadowFailInfo.outlineSize > 1e-6 ? height / shadowFailInfo.outlineSize : 0.0;
                if (ratio > bestRatio)
                {
                    bestInfo = shadowFailInfo;
                    bestRatio = ratio;
                }
                continue;
            }

            FastenerFilterInfo currentInfo;
            currentInfo.outlineType = shadowInfo.outlineType;
            currentInfo.height = height;
            currentInfo.outlineSize = shadowInfo.outlineSize;
            currentInfo.threshold = shadowInfo.outlineSize * 0.3;
            currentInfo.cylinderDiameter = diameter;
            currentInfo.hullPointCount = shadowInfo.hullPointCount;
            currentInfo.shadowCurveCount = shadowInfo.curveCount;
            currentInfo.shadowLineCount = shadowInfo.lineCount;
            currentInfo.shadowCircleCount = shadowInfo.circleCount;
            currentInfo.shadowSplineCount = shadowInfo.splineCount;
            currentInfo.shadowOtherCount = shadowInfo.otherCount;
            currentInfo.cylinderFaceCount = bestInfo.cylinderFaceCount;
            currentInfo.outerCylinderFaceCount = bestInfo.outerCylinderFaceCount;
            currentInfo.outerMed = outerMed;
            currentInfo.circularEdgeCount = circleStats.circularEdgeCount;
            currentInfo.fullCircleEdgeCount = circleStats.fullCircleEdgeCount;
            currentInfo.halfCircleEdgeCount = circleStats.halfCircleEdgeCount;
            currentInfo.reason = "阴影曲线轮廓匹配，高度不足";
            const double ratio = shadowInfo.outlineSize > 1e-6 ? height / shadowInfo.outlineSize : 0.0;
            if (ratio > bestRatio)
            {
                bestRatio = ratio;
                bestInfo = currentInfo;
            }

            if (height > currentInfo.threshold + 1e-4)
            {
                currentInfo.matched = true;
                currentInfo.reason = "阴影曲线轮廓命中跳过";
                if (info != NULL)
                {
                    *info = currentInfo;
                }
                return true;
            }
        }

        if (info != NULL)
        {
            *info = bestInfo;
        }
        return false;
    }

    double FaceBoxAreaScore(Face* face)
    {
        if (face == NULL)
        {
            return -1.0;
        }

        double box[6] = { 0, 0, 0, 0, 0, 0 };
        if (UF_MODL_ask_bounding_box(face->Tag(), box) != 0)
        {
            return -1.0;
        }

        std::vector<double> dims;
        dims.push_back(std::fabs(box[3] - box[0]));
        dims.push_back(std::fabs(box[4] - box[1]));
        dims.push_back(std::fabs(box[5] - box[2]));
        std::sort(dims.begin(), dims.end(), std::greater<double>());
        return dims[0] * dims[1];
    }

    bool TryMeasureFaceArea(Face* face, double* areaOut)
    {
        if (face == NULL || areaOut == NULL)
        {
            return false;
        }

        try
        {
            Session* session = Session::GetSession();
            if (session == NULL || session->Parts() == NULL)
            {
                return false;
            }

            Part* workPart = session->Parts()->Work();
            if (workPart == NULL || workPart->MeasureManager() == NULL || workPart->UnitCollection() == NULL)
            {
                return false;
            }

            Unit* areaUnit = workPart->UnitCollection()->GetBase("Area");
            Unit* lengthUnit = workPart->UnitCollection()->GetBase("Length");
            if (areaUnit == NULL || lengthUnit == NULL)
            {
                return false;
            }

            std::vector<IParameterizedSurface*> faceObjects;
            faceObjects.push_back(face);
            MeasureFaces* measure = workPart->MeasureManager()->NewFaceProperties(areaUnit, lengthUnit, 0.99, faceObjects);
            if (measure == NULL)
            {
                return false;
            }

            const double area = measure->Area();
            if (area <= 0.0)
            {
                return false;
            }

            *areaOut = area;
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    double FaceAreaScore(Face* face)
    {
        double area = -1.0;
        if (TryMeasureFaceArea(face, &area))
        {
            return area;
        }

        if (face == NULL)
        {
            return -1.0;
        }

        try
        {
            return FaceBoxAreaScore(face);
        }
        catch (...)
        {
            return -1.0;
        }
    }

    bool AskFaceType(Face* face, int* type)
    {
        if (face == NULL || type == NULL)
        {
            return false;
        }

        double point[3] = { 0, 0, 0 };
        double direction[3] = { 0, 0, 0 };
        double box[6] = { 0, 0, 0, 0, 0, 0 };
        double radius = 0.0;
        double radiusData = 0.0;
        int normDir = 0;
        return UF_MODL_ask_face_data(face->Tag(), type, point, direction, box, &radius, &radiusData, &normDir) == 0;
    }

    bool AskFaceData(Face* face, int* type, double center[3], double normal[3], double* radiusOut)
    {
        if (face == NULL)
        {
            return false;
        }

        double point[3] = { 0, 0, 0 };
        double direction[3] = { 0, 0, 1 };
        double box[6] = { 0, 0, 0, 0, 0, 0 };
        double radius = 0.0;
        double radiusData = 0.0;
        int normDir = 0;
        if (UF_MODL_ask_face_data(face->Tag(), type, point, direction, box, &radius, &radiusData, &normDir) != 0)
        {
            return false;
        }

        if (center != NULL)
        {
            center[0] = (box[0] + box[3]) * 0.5;
            center[1] = (box[1] + box[4]) * 0.5;
            center[2] = (box[2] + box[5]) * 0.5;
        }
        if (normal != NULL)
        {
            normal[0] = direction[0] * (normDir == -1 ? -1.0 : 1.0);
            normal[1] = direction[1] * (normDir == -1 ? -1.0 : 1.0);
            normal[2] = direction[2] * (normDir == -1 ? -1.0 : 1.0);
        }
        if (radiusOut != NULL)
        {
            *radiusOut = radius;
        }
        return true;
    }

    double Dot3(const double a[3], const double b[3])
    {
        return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
    }

    double Distance3(const double a[3], const double b[3])
    {
        double dx = a[0] - b[0];
        double dy = a[1] - b[1];
        double dz = a[2] - b[2];
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    double Length3(const double v[3])
    {
        return std::sqrt(Dot3(v, v));
    }

    bool Normalize3(double v[3])
    {
        double length = Length3(v);
        if (length <= 1e-12)
        {
            return false;
        }

        v[0] /= length;
        v[1] /= length;
        v[2] /= length;
        return true;
    }

    double CrossLength3(const double a[3], const double b[3])
    {
        double cross[3] = {
            a[1] * b[2] - a[2] * b[1],
            a[2] * b[0] - a[0] * b[2],
            a[0] * b[1] - a[1] * b[0]
        };
        return Length3(cross);
    }

    double DistancePointToAxis(const double point[3], const double axisPoint[3], const double axisDirection[3])
    {
        double delta[3] = {
            point[0] - axisPoint[0],
            point[1] - axisPoint[1],
            point[2] - axisPoint[2]
        };
        return CrossLength3(delta, axisDirection);
    }

    bool AskCylinderFaceData(Face* face, double axisPoint[3], double axisDirection[3], double* radiusOut, int* normDirOut)
    {
        if (face == NULL || axisPoint == NULL || axisDirection == NULL || radiusOut == NULL)
        {
            return false;
        }

        int type = 0;
        double box[6] = { 0, 0, 0, 0, 0, 0 };
        double radiusData = 0.0;
        int normDir = 0;
        if (UF_MODL_ask_face_data(face->Tag(), &type, axisPoint, axisDirection, box, radiusOut, &radiusData, &normDir) != 0 ||
            type != UF_MODL_CYLINDRICAL_FACE ||
            *radiusOut <= 1e-6)
        {
            return false;
        }

        if (!Normalize3(axisDirection))
        {
            return false;
        }

        if (normDirOut != NULL)
        {
            *normDirOut = normDir;
        }
        return true;
    }

    bool AskFacePointFromEdges(Face* face, double facePoint[3])
    {
        if (face == NULL || facePoint == NULL)
        {
            return false;
        }

        const std::vector<Edge*> edges = FaceEdgesByUf(face);
        for (size_t i = 0; i < edges.size(); ++i)
        {
            Edge* edge = edges[i];
            if (edge == NULL)
            {
                continue;
            }

            try
            {
                Point3d vertex1;
                Point3d vertex2;
                edge->GetVertices(&vertex1, &vertex2);
                facePoint[0] = vertex1.X;
                facePoint[1] = vertex1.Y;
                facePoint[2] = vertex1.Z;
                return true;
            }
            catch (...)
            {
            }
        }

        return false;
    }

    bool AskNxOpenCreatedFaceNormal(Face* face, const double facePoint[3], double unitNormal[3])
    {
        if (face == NULL || facePoint == NULL || unitNormal == NULL)
        {
            return false;
        }

        try
        {
            Session* session = Session::GetSession();
            BasePart* owningPart = face->OwningPart();
            if (session == NULL || owningPart == NULL || owningPart->Points() == NULL || owningPart->Directions() == NULL)
            {
                return false;
            }

            Point* point = owningPart->Points()->CreatePoint(Point3d(facePoint[0], facePoint[1], facePoint[2]));
            Direction* direction = owningPart->Directions()->CreateDirection(
                face,
                point,
                SenseForward,
                SmartObject::UpdateOptionWithinModeling);
            if (direction == NULL)
            {
                return false;
            }

            Vector3d vector = direction->Vector();
            unitNormal[0] = vector.X;
            unitNormal[1] = vector.Y;
            unitNormal[2] = vector.Z;
            if (!Normalize3(unitNormal))
            {
                return false;
            }

            std::vector<TaggedObject*> deleteObjects;
            deleteObjects.push_back(direction);
            deleteObjects.push_back(point);
            Update* updateManager = session->UpdateManager();
            if (updateManager != NULL)
            {
                updateManager->AddObjectsToDeleteList(deleteObjects);
                Session::UndoMarkId markId = session->SetUndoMark(Session::MarkVisibilityInvisible, "Delete temporary face normal");
                updateManager->DoUpdate(markId);
            }

            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    bool AskOuterCylinderMedLike08(Face* face, double* medOut)
    {
        if (face == NULL)
        {
            return false;
        }

        int type = 0;
        double axisPoint[3] = { 0.0, 0.0, 0.0 };
        double axisDirection[3] = { 0.0, 0.0, 1.0 };
        double box[6] = { 0, 0, 0, 0, 0, 0 };
        double radius = 0.0;
        double radiusData = 0.0;
        int normDir = 0;
        if (UF_MODL_ask_face_data(face->Tag(), &type, axisPoint, axisDirection, box, &radius, &radiusData, &normDir) != 0 ||
            type != UF_MODL_CYLINDRICAL_FACE)
        {
            return false;
        }

        double facePoint[3] = { 0.0, 0.0, 0.0 };
        double unitNormal[3] = { 0.0, 0.0, 1.0 };
        if (!AskFacePointFromEdges(face, facePoint) ||
            !AskNxOpenCreatedFaceNormal(face, facePoint, unitNormal))
        {
            return false;
        }

        double vectorToAxis[3] = {
            axisPoint[0] - facePoint[0],
            axisPoint[1] - facePoint[1],
            axisPoint[2] - facePoint[2]
        };
        const double med = Dot3(vectorToAxis, unitNormal);
        if (medOut != NULL)
        {
            *medOut = med;
        }
        return true;
    }

    bool IsOuterCylinderFaceLike08(Face* face)
    {
        double med = 0.0;
        if (!AskOuterCylinderMedLike08(face, &med))
        {
            return false;
        }
        return med < 0.0;
    }

    std::vector<tag_t> TagsFromUfList(uf_list_p_t list)
    {
        std::vector<tag_t> tags;
        if (list == NULL)
        {
            return tags;
        }

        int count = 0;
        if (UF_MODL_ask_list_count(list, &count) != 0)
        {
            return tags;
        }

        for (int i = 0; i < count; ++i)
        {
            tag_t tag = NULL_TAG;
            if (UF_MODL_ask_list_item(list, i, &tag) == 0 && tag != NULL_TAG)
            {
                tags.push_back(tag);
            }
        }

        return tags;
    }

    TaggedObject* ObjectFromTag(tag_t tag)
    {
        if (tag == NULL_TAG)
        {
            return NULL;
        }

        try
        {
            return NXObjectManager::Get(tag);
        }
        catch (...)
        {
            return NULL;
        }
    }

    std::vector<Face*> AdjacentFacesByEdge(Edge* edge, Face* owner)
    {
        std::vector<Face*> faces;
        if (edge == NULL)
        {
            return faces;
        }

        uf_list_p_t faceList = NULL;
        if (UF_MODL_ask_edge_faces(edge->Tag(), &faceList) != 0 || faceList == NULL)
        {
            return faces;
        }

        std::vector<tag_t> tags = TagsFromUfList(faceList);
        UF_MODL_delete_list(&faceList);
        for (size_t i = 0; i < tags.size(); ++i)
        {
            if (owner != NULL && tags[i] == owner->Tag())
            {
                continue;
            }
            Face* face = dynamic_cast<Face*>(ObjectFromTag(tags[i]));
            if (face != NULL)
            {
                faces.push_back(face);
            }
        }

        return faces;
    }

    std::vector<Edge*> FaceEdgesByUf(Face* face)
    {
        std::vector<Edge*> edges;
        if (face == NULL)
        {
            return edges;
        }

        uf_list_p_t edgeList = NULL;
        if (UF_MODL_ask_face_edges(face->Tag(), &edgeList) != 0 || edgeList == NULL)
        {
            try
            {
                return face->GetEdges();
            }
            catch (...)
            {
                return edges;
            }
        }

        std::vector<tag_t> tags = TagsFromUfList(edgeList);
        UF_MODL_delete_list(&edgeList);
        for (size_t i = 0; i < tags.size(); ++i)
        {
            Edge* edge = dynamic_cast<Edge*>(ObjectFromTag(tags[i]));
            if (edge != NULL)
            {
                edges.push_back(edge);
            }
        }

        return edges;
    }

    std::vector<FaceCandidate> GetPlanarCandidates(Body* body)
    {
        std::vector<FaceCandidate> candidates;
        if (body == NULL)
        {
            return candidates;
        }

        std::vector<Face*> faces = body->GetFaces();
        for (size_t i = 0; i < faces.size(); ++i)
        {
            int type = 0;
            double radius = 0.0;
            FaceCandidate candidate;
            if (!AskFaceData(faces[i], &type, candidate.center, candidate.normal, &radius) || type != UF_MODL_PLANAR_FACE)
            {
                continue;
            }

            candidate.face = faces[i];
            candidate.type = type;
            candidate.area = FaceBoxAreaScore(faces[i]);
            candidates.push_back(candidate);
        }

        return candidates;
    }

    bool IsPlanarOrCylindricalFace(Face* face)
    {
        int type = 0;
        return AskFaceType(face, &type) &&
            (type == UF_MODL_PLANAR_FACE || type == UF_MODL_CYLINDRICAL_FACE);
    }

    std::vector<Face*> CollectContinuousTangentFaces(Face* startFace)
    {
        std::vector<Face*> result;
        if (startFace == NULL)
        {
            return result;
        }

        std::queue<Face*> queue;
        std::set<tag_t> visited;
        queue.push(startFace);
        visited.insert(startFace->Tag());
        while (!queue.empty())
        {
            Face* current = queue.front();
            queue.pop();
            if (current == NULL)
            {
                continue;
            }

            result.push_back(current);
            std::vector<Edge*> edges = FaceEdgesByUf(current);
            for (size_t i = 0; i < edges.size(); ++i)
            {
                logical isSmooth = false;
                if (UF_MODL_ask_edge_smoothness(edges[i]->Tag(), 18.2, &isSmooth) != 0 || !isSmooth)
                {
                    continue;
                }

                std::vector<Face*> adjacent = AdjacentFacesByEdge(edges[i], current);
                for (size_t j = 0; j < adjacent.size(); ++j)
                {
                    Face* next = adjacent[j];
                    if (next == NULL || visited.count(next->Tag()) || !IsPlanarOrCylindricalFace(next))
                    {
                        continue;
                    }

                    visited.insert(next->Tag());
                    queue.push(next);
                }
            }
        }

        return result;
    }

    std::map<tag_t, bool> FaceTagMap(const std::vector<Face*>& faces)
    {
        std::map<tag_t, bool> tags;
        for (size_t i = 0; i < faces.size(); ++i)
        {
            if (faces[i] != NULL)
            {
                tags[faces[i]->Tag()] = true;
            }
        }
        return tags;
    }

    std::vector<FaceCandidate> FilterCandidatesByTags(
        const std::vector<FaceCandidate>& candidates,
        const std::map<tag_t, bool>& tags)
    {
        std::vector<FaceCandidate> filtered;
        if (tags.empty())
        {
            return filtered;
        }

        for (size_t i = 0; i < candidates.size(); ++i)
        {
            if (candidates[i].face != NULL && tags.count(candidates[i].face->Tag()))
            {
                filtered.push_back(candidates[i]);
            }
        }
        return filtered;
    }

    std::map<tag_t, bool> SubtractFaceTags(
        const std::map<tag_t, bool>& source,
        const std::map<tag_t, bool>& exclude)
    {
        std::map<tag_t, bool> result;
        for (std::map<tag_t, bool>::const_iterator it = source.begin(); it != source.end(); ++it)
        {
            if (it->second && !exclude.count(it->first))
            {
                result[it->first] = true;
            }
        }
        return result;
    }

    std::vector<FaceCandidate> GetLargestPlanarFaceGroup(const std::vector<FaceCandidate>& candidates)
    {
        std::vector<FaceCandidate> group;
        if (candidates.empty())
        {
            return group;
        }

        double maxArea = 0.0;
        for (size_t i = 0; i < candidates.size(); ++i)
        {
            maxArea = std::max(maxArea, candidates[i].area);
        }

        double tolerance = std::max(maxArea * 0.001, 0.01);
        for (size_t i = 0; i < candidates.size(); ++i)
        {
            if (std::fabs(candidates[i].area - maxArea) <= tolerance)
            {
                group.push_back(candidates[i]);
            }
        }

        if (group.empty())
        {
            FaceCandidate largest = candidates[0];
            for (size_t i = 1; i < candidates.size(); ++i)
            {
                if (candidates[i].area > largest.area)
                {
                    largest = candidates[i];
                }
            }
            group.push_back(largest);
        }

        std::sort(group.begin(), group.end(), [](const FaceCandidate& a, const FaceCandidate& b) {
            return a.area > b.area;
        });
        return group;
    }

    FaceCandidate* FindCandidateByTag(std::vector<FaceCandidate>& candidates, tag_t tag)
    {
        for (size_t i = 0; i < candidates.size(); ++i)
        {
            if (candidates[i].face != NULL && candidates[i].face->Tag() == tag)
            {
                return &candidates[i];
            }
        }
        return NULL;
    }

    FaceCandidate FindNearestParallelPlanarFace(
        const std::vector<FaceCandidate>& allCandidates,
        const FaceCandidate& reference,
        double minArea,
        bool* found)
    {
        FaceCandidate best;
        if (found != NULL)
        {
            *found = false;
        }
        if (reference.face == NULL)
        {
            return best;
        }

        double bestDistance = std::numeric_limits<double>::max();
        for (size_t i = 0; i < allCandidates.size(); ++i)
        {
            const FaceCandidate& candidate = allCandidates[i];
            if (candidate.face == NULL || candidate.face->Tag() == reference.face->Tag())
            {
                continue;
            }
            if (candidate.area < minArea || std::fabs(Dot3(candidate.normal, reference.normal)) < 0.995)
            {
                continue;
            }

            double delta[3] = {
                candidate.center[0] - reference.center[0],
                candidate.center[1] - reference.center[1],
                candidate.center[2] - reference.center[2]
            };
            double distance = std::fabs(Dot3(delta, reference.normal));
            if (distance < 1e-6 || distance >= bestDistance)
            {
                continue;
            }

            best = candidate;
            bestDistance = distance;
            if (found != NULL)
            {
                *found = true;
            }
        }

        return best;
    }

    struct CylinderAxisRecord
    {
        double point[3] = { 0.0, 0.0, 0.0 };
        double direction[3] = { 0.0, 0.0, 1.0 };
    };

    bool AskCylinderAxis(Face* face, CylinderAxisRecord* axis)
    {
        if (face == NULL || axis == NULL)
        {
            return false;
        }

        int type = 0;
        double box[6] = { 0, 0, 0, 0, 0, 0 };
        double radius = 0.0;
        double radiusData = 0.0;
        int normDir = 0;
        if (UF_MODL_ask_face_data(face->Tag(), &type, axis->point, axis->direction, box, &radius, &radiusData, &normDir) != 0 ||
            type != UF_MODL_CYLINDRICAL_FACE)
        {
            return false;
        }

        if (!Normalize3(axis->direction))
        {
            return false;
        }

        return true;
    }

    bool IsCoaxialCylinderAxis(const CylinderAxisRecord& a, const CylinderAxisRecord& b)
    {
        const double parallelTolerance = 0.999;
        const double axisDistanceTolerance = 0.05;
        if (std::fabs(Dot3(a.direction, b.direction)) < parallelTolerance)
        {
            return false;
        }

        double delta[3] = {
            b.point[0] - a.point[0],
            b.point[1] - a.point[1],
            b.point[2] - a.point[2]
        };
        return CrossLength3(delta, a.direction) <= axisDistanceTolerance;
    }

    double AdjacentCylinderScore(Face* face, const std::map<tag_t, bool>& sameSideTags)
    {
        if (face == NULL)
        {
            return 0.0;
        }

        std::vector<CylinderAxisRecord> bendAxes;
        std::vector<Edge*> edges = FaceEdgesByUf(face);
        for (size_t i = 0; i < edges.size(); ++i)
        {
            std::vector<Face*> adjacent = AdjacentFacesByEdge(edges[i], face);
            for (size_t j = 0; j < adjacent.size(); ++j)
            {
                if (!sameSideTags.empty() && !sameSideTags.count(adjacent[j]->Tag()))
                {
                    continue;
                }

                CylinderAxisRecord axis;
                if (!AskCylinderAxis(adjacent[j], &axis))
                {
                    continue;
                }

                bool alreadyCounted = false;
                for (size_t k = 0; k < bendAxes.size(); ++k)
                {
                    if (IsCoaxialCylinderAxis(bendAxes[k], axis))
                    {
                        alreadyCounted = true;
                        break;
                    }
                }

                if (!alreadyCounted)
                {
                    bendAxes.push_back(axis);
                }
            }
        }

        int count = static_cast<int>(bendAxes.size());
        if (count <= 1) return 0.0;
        if (count == 2) return 2.0;
        if (count == 3) return 4.0;
        return 6.0;
    }

    double BendDirectionPreferenceScore(const FaceCandidate& candidate, bool preferUpBends)
    {
        if (candidate.face == NULL)
        {
            return 0.0;
        }

        int positiveSide = 0;
        int negativeSide = 0;
        std::map<tag_t, bool> seen;
        std::vector<Edge*> edges = FaceEdgesByUf(candidate.face);
        for (size_t i = 0; i < edges.size(); ++i)
        {
            std::vector<Face*> adjacent = AdjacentFacesByEdge(edges[i], candidate.face);
            for (size_t j = 0; j < adjacent.size(); ++j)
            {
                Face* face = adjacent[j];
                if (face == NULL || seen[face->Tag()])
                {
                    continue;
                }

                int type = 0;
                double center[3] = { 0.0, 0.0, 0.0 };
                double normal[3] = { 0.0, 0.0, 1.0 };
                double radius = 0.0;
                if (!AskFaceData(face, &type, center, normal, &radius) || type != UF_MODL_CYLINDRICAL_FACE)
                {
                    continue;
                }

                seen[face->Tag()] = true;
                double vectorToBend[3] = {
                    center[0] - candidate.center[0],
                    center[1] - candidate.center[1],
                    center[2] - candidate.center[2]
                };
                if (Dot3(vectorToBend, candidate.normal) >= 0.0)
                {
                    ++positiveSide;
                }
                else
                {
                    ++negativeSide;
                }
            }
        }

        int preferred = preferUpBends ? positiveSide : negativeSide;
        int opposite = preferUpBends ? negativeSide : positiveSide;
        return static_cast<double>(preferred - opposite) * 2.0;
    }

    void CountBendSides(const FaceCandidate& candidate, int* positiveSide, int* negativeSide)
    {
        if (positiveSide != NULL)
        {
            *positiveSide = 0;
        }
        if (negativeSide != NULL)
        {
            *negativeSide = 0;
        }
        if (candidate.face == NULL)
        {
            return;
        }

        std::map<tag_t, bool> seen;
        std::vector<Edge*> edges = FaceEdgesByUf(candidate.face);
        for (size_t i = 0; i < edges.size(); ++i)
        {
            std::vector<Face*> adjacent = AdjacentFacesByEdge(edges[i], candidate.face);
            for (size_t j = 0; j < adjacent.size(); ++j)
            {
                Face* face = adjacent[j];
                if (face == NULL || seen[face->Tag()])
                {
                    continue;
                }

                int type = 0;
                double center[3] = { 0.0, 0.0, 0.0 };
                double normal[3] = { 0.0, 0.0, 1.0 };
                double radius = 0.0;
                if (!AskFaceData(face, &type, center, normal, &radius) || type != UF_MODL_CYLINDRICAL_FACE)
                {
                    continue;
                }

                seen[face->Tag()] = true;
                double vectorToBend[3] = {
                    center[0] - candidate.center[0],
                    center[1] - candidate.center[1],
                    center[2] - candidate.center[2]
                };
                if (Dot3(vectorToBend, candidate.normal) >= 0.0)
                {
                    if (positiveSide != NULL) ++(*positiveSide);
                }
                else
                {
                    if (negativeSide != NULL) ++(*negativeSide);
                }
            }
        }
    }

    bool HasInnerLoopScore(Face* face)
    {
        if (face == NULL)
        {
            return false;
        }
        int topology = 0;
        if (UF_MODL_ask_face_topology(face->Tag(), &topology) == 0 && topology != UF_MODL_FLAT_TOPOLOGY)
        {
            return true;
        }
        return FaceEdgesByUf(face).size() > 4;
    }

    std::map<tag_t, double> ChainPositionScores(const std::vector<FaceCandidate>& candidates)
    {
        std::map<tag_t, double> scores;
        if (candidates.empty())
        {
            return scores;
        }

        double minC[3] = { candidates[0].center[0], candidates[0].center[1], candidates[0].center[2] };
        double maxC[3] = { candidates[0].center[0], candidates[0].center[1], candidates[0].center[2] };
        for (size_t i = 1; i < candidates.size(); ++i)
        {
            for (int a = 0; a < 3; ++a)
            {
                minC[a] = std::min(minC[a], candidates[i].center[a]);
                maxC[a] = std::max(maxC[a], candidates[i].center[a]);
            }
        }

        int axis = 0;
        double span = maxC[0] - minC[0];
        for (int a = 1; a < 3; ++a)
        {
            double candidateSpan = maxC[a] - minC[a];
            if (candidateSpan > span)
            {
                span = candidateSpan;
                axis = a;
            }
        }

        double mid = (minC[axis] + maxC[axis]) * 0.5;
        double half = std::max(span * 0.5, 1e-9);
        for (size_t i = 0; i < candidates.size(); ++i)
        {
            double normalizedDistance = std::min(1.0, std::fabs(candidates[i].center[axis] - mid) / half);
            scores[candidates[i].face->Tag()] = 3.0 * (1.0 - normalizedDistance);
        }

        return scores;
    }

    FaceInfo ScoreAndSelectBaseFace(
        std::vector<FaceCandidate> candidates,
        const std::map<tag_t, bool>& scoringChainTags)
    {
        FaceInfo result;
        if (candidates.empty())
        {
            return result;
        }

        double minArea = std::numeric_limits<double>::max();
        for (size_t i = 0; i < candidates.size(); ++i)
        {
            if (candidates[i].area > 1e-9)
            {
                minArea = std::min(minArea, candidates[i].area);
            }
        }
        if (minArea == std::numeric_limits<double>::max())
        {
            minArea = 1.0;
        }

        std::map<tag_t, double> position = ChainPositionScores(candidates);
        for (size_t i = 0; i < candidates.size(); ++i)
        {
            double areaScore = std::max(1.0, candidates[i].area / std::max(minArea, 1e-9));
            double positionScore = position.count(candidates[i].face->Tag()) ? position[candidates[i].face->Tag()] : 0.0;
            double innerLoopScore = HasInnerLoopScore(candidates[i].face) ? 1.0 : 0.0;
            double cylinderScore = AdjacentCylinderScore(candidates[i].face, scoringChainTags);
            candidates[i].score = areaScore + positionScore + innerLoopScore + cylinderScore;
            if (candidates[i].score > result.totalScore ||
                (std::fabs(candidates[i].score - result.totalScore) <= 1e-9 && candidates[i].area > result.areaScore))
            {
                result.face = candidates[i].face;
                result.type = candidates[i].type;
                result.areaScore = candidates[i].area;
                result.totalScore = candidates[i].score;
            }
        }

        return result;
    }

    FaceInfo SelectMarkedFaceBaseFace(Body* body, const std::vector<FaceCandidate>& allCandidates)
    {
        FaceInfo result;
        if (body == NULL || allCandidates.empty())
        {
            return result;
        }

        std::map<tag_t, bool> checkedFaces;
        std::map<tag_t, bool> checkedEdges;
        std::vector<Face*> bodyFaces = body->GetFaces();
        for (size_t i = 0; i < bodyFaces.size(); ++i)
        {
            Face* markedFace = bodyFaces[i];
            if (markedFace == NULL)
            {
                continue;
            }

            std::vector<Edge*> edges = FaceEdgesByUf(markedFace);
            const bool faceIsMarked = HasMarkedBaseAttribute(markedFace->Tag());
            for (size_t edgeIndex = 0; edgeIndex < edges.size(); ++edgeIndex)
            {
                Edge* edge = edges[edgeIndex];
                if (edge == NULL)
                {
                    continue;
                }

                const bool edgeIsMarked = !checkedEdges[edge->Tag()] && HasMarkedBaseAttribute(edge->Tag());
                checkedEdges[edge->Tag()] = true;
                if (!faceIsMarked && !edgeIsMarked)
                {
                    continue;
                }

                std::vector<Face*> adjacentFaces = AdjacentFacesByEdge(edge, faceIsMarked ? markedFace : NULL);
                for (size_t faceIndex = 0; faceIndex < adjacentFaces.size(); ++faceIndex)
                {
                    Face* adjacentFace = adjacentFaces[faceIndex];
                    if (adjacentFace == NULL ||
                        (faceIsMarked && adjacentFace->Tag() == markedFace->Tag()) ||
                        checkedFaces[adjacentFace->Tag()])
                    {
                        continue;
                    }

                    checkedFaces[adjacentFace->Tag()] = true;
                    for (size_t candidateIndex = 0; candidateIndex < allCandidates.size(); ++candidateIndex)
                    {
                        const FaceCandidate& candidate = allCandidates[candidateIndex];
                        if (candidate.face == NULL || candidate.face->Tag() != adjacentFace->Tag())
                        {
                            continue;
                        }

                        if (candidate.area > result.areaScore)
                        {
                            result.face = candidate.face;
                            result.type = candidate.type;
                            result.areaScore = candidate.area;
                            result.totalScore = candidate.area;
                        }
                        break;
                    }
                }
            }
        }

        return result;
    }

    double PointDistanceForMarkerLine(const Point3d& a, const Point3d& b)
    {
        const double dx = a.X - b.X;
        const double dy = a.Y - b.Y;
        const double dz = a.Z - b.Z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    struct MarkerLineScanInfo
    {
        bool matched = false;
        bool planar = false;
        int faceType = 0;
        size_t edgeCount = 0;
        size_t testedPairCount = 0;
        size_t shortEdgeSkipCount = 0;
        size_t parallelPairCount = 0;
        double bestParallel = 0.0;
        double bestDistance = std::numeric_limits<double>::max();
        double closestParallelDistance = std::numeric_limits<double>::max();
        double closestParallel = 0.0;
        double closestLengthA = 0.0;
        double closestLengthB = 0.0;
        double matchedParallel = 0.0;
        double matchedDistance = 0.0;
        double matchedLengthA = 0.0;
        double matchedLengthB = 0.0;
        std::string reason;
    };

    MarkerLineScanInfo ScanMarkerLineOnFace(Face* face, double markerGapTolerance)
    {
        const double markerMinEdgeLength = 2.0;
        MarkerLineScanInfo info;
        if (face == NULL)
        {
            info.reason = "null face";
            return info;
        }

        int type = 0;
        if (!AskFaceType(face, &type) || type != UF_MODL_PLANAR_FACE)
        {
            info.faceType = type;
            info.reason = "not planar";
            return info;
        }
        info.faceType = type;
        info.planar = true;

        std::vector<Edge*> edges = FaceEdgesByUf(face);
        info.edgeCount = edges.size();
        for (size_t i = 0; i < edges.size(); ++i)
        {
            if (edges[i] == NULL)
            {
                continue;
            }

            Point3d a1;
            Point3d a2;
            try
            {
                edges[i]->GetVertices(&a1, &a2);
            }
            catch (...)
            {
                continue;
            }

            double lenA = PointDistanceForMarkerLine(a1, a2);
            if (lenA <= markerMinEdgeLength)
            {
                ++info.shortEdgeSkipCount;
                continue;
            }

            double va[3] = { a2.X - a1.X, a2.Y - a1.Y, a2.Z - a1.Z };
            double vaLen = std::max(lenA, 1e-9);

            for (size_t j = i + 1; j < edges.size(); ++j)
            {
                if (edges[j] == NULL)
                {
                    continue;
                }

                Point3d b1;
                Point3d b2;
                try
                {
                    edges[j]->GetVertices(&b1, &b2);
                }
                catch (...)
                {
                    continue;
                }

                double lenB = PointDistanceForMarkerLine(b1, b2);
                if (lenB <= markerMinEdgeLength)
                {
                    ++info.shortEdgeSkipCount;
                    continue;
                }
                ++info.testedPairCount;

                double vb[3] = { b2.X - b1.X, b2.Y - b1.Y, b2.Z - b1.Z };
                double parallel = std::fabs(Dot3(va, vb) / (vaLen * std::max(lenB, 1e-9)));
                if (parallel < 0.995)
                {
                    continue;
                }
                ++info.parallelPairCount;

                double w[3] = { b1.X - a1.X, b1.Y - a1.Y, b1.Z - a1.Z };
                double cross[3] = {
                    w[1] * va[2] - w[2] * va[1],
                    w[2] * va[0] - w[0] * va[2],
                    w[0] * va[1] - w[1] * va[0]
                };
                double distance = std::sqrt(Dot3(cross, cross)) / vaLen;
                if (parallel > info.bestParallel ||
                    (std::fabs(parallel - info.bestParallel) <= 1e-9 && distance < info.bestDistance))
                {
                    info.bestParallel = parallel;
                    info.bestDistance = distance;
                }
                if (distance > 1e-9 && distance < info.closestParallelDistance)
                {
                    info.closestParallel = parallel;
                    info.closestParallelDistance = distance;
                    info.closestLengthA = lenA;
                    info.closestLengthB = lenB;
                }
                if (distance > 1e-9 && distance <= markerGapTolerance)
                {
                    info.matched = true;
                    info.matchedParallel = parallel;
                    info.matchedDistance = distance;
                    info.matchedLengthA = lenA;
                    info.matchedLengthB = lenB;
                    info.reason = "matched";
                    return info;
                }
            }
        }

        if (info.bestDistance < std::numeric_limits<double>::max())
        {
            std::ostringstream reason;
            reason << "closest positive parallel distance "
                   << (info.closestParallelDistance < std::numeric_limits<double>::max() ? info.closestParallelDistance : -1.0)
                   << " exceeds marker gap tolerance";
            info.reason = reason.str();
        }
        else
        {
            info.reason = "no parallel edge pair passed length/parallel checks";
        }
        return info;
    }

    MarkerLineScanInfo ScanMarkerLineOnFace(Face* face)
    {
        return ScanMarkerLineOnFace(face, 0.01);
    }

    bool HasMarkerLineOnFace(Face* face)
    {
        return ScanMarkerLineOnFace(face).matched;
    }

    bool HasMarkerLineOnFace(Face* face, double markerGapTolerance)
    {
        return ScanMarkerLineOnFace(face, markerGapTolerance).matched;
    }

    bool ChainHasMarkerLineOrMarkedFace(
        const std::map<tag_t, bool>& chainTags,
        const char* chainName,
        double markerGapTolerance)
    {
        std::ostringstream begin;
        begin << "scan marker chain=" << (chainName == NULL ? "" : chainName)
              << " faceCount=" << chainTags.size()
              << " tolerance=" << markerGapTolerance;
        AppendMarkerLineDebugLog(begin.str());

        for (std::map<tag_t, bool>::const_iterator it = chainTags.begin(); it != chainTags.end(); ++it)
        {
            Face* face = dynamic_cast<Face*>(ObjectFromTag(it->first));
            if (face == NULL)
            {
                continue;
            }

            MarkerLineScanInfo scan = ScanMarkerLineOnFace(face, markerGapTolerance);
            const bool faceMarked = HasMarkedBaseAttribute(face->Tag());
            if (scan.matched || faceMarked)
            {
                std::ostringstream hit;
                hit << "marker chain hit chain=" << (chainName == NULL ? "" : chainName)
                    << " face=" << face->Tag()
                    << " by=" << (scan.matched ? "parallel-gap" : "face-attribute")
                    << " matchedDistance=" << scan.matchedDistance
                    << " closestDistance=" << (scan.closestParallelDistance < std::numeric_limits<double>::max() ? scan.closestParallelDistance : -1.0)
                    << " reason=" << scan.reason;
                AppendMarkerLineDebugLog(hit.str());
                return true;
            }

            std::vector<Edge*> edges = FaceEdgesByUf(face);
            for (size_t i = 0; i < edges.size(); ++i)
            {
                std::vector<Face*> adjacentFaces = AdjacentFacesByEdge(edges[i], face);
                for (size_t j = 0; j < adjacentFaces.size(); ++j)
                {
                    Face* adjacentFace = adjacentFaces[j];
                    if (adjacentFace == NULL || adjacentFace->Tag() == face->Tag())
                    {
                        continue;
                    }
                    if (HasMarkedBaseAttribute(adjacentFace->Tag()))
                    {
                        std::ostringstream hit;
                        hit << "marker chain hit chain=" << (chainName == NULL ? "" : chainName)
                            << " face=" << face->Tag()
                            << " adjacentMarkedFace=" << adjacentFace->Tag()
                            << " by=adjacent-face-attribute"
                            << " scanReason=" << scan.reason;
                        AppendMarkerLineDebugLog(hit.str());
                        return true;
                    }
                }
            }

            std::ostringstream miss;
            miss << "marker face miss chain=" << (chainName == NULL ? "" : chainName)
                 << " face=" << face->Tag()
                 << " testedPairs=" << scan.testedPairCount
                 << " parallelPairs=" << scan.parallelPairCount
                 << " closestDistance=" << (scan.closestParallelDistance < std::numeric_limits<double>::max() ? scan.closestParallelDistance : -1.0)
                 << " bestDistance=" << (scan.bestDistance < std::numeric_limits<double>::max() ? scan.bestDistance : -1.0)
                 << " reason=" << scan.reason;
            AppendMarkerLineDebugLog(miss.str());
        }

        std::ostringstream missChain;
        missChain << "marker chain miss chain=" << (chainName == NULL ? "" : chainName)
                  << " faceCount=" << chainTags.size();
        AppendMarkerLineDebugLog(missChain.str());
        return false;
    }

    bool HasMarkerLineInFaceTags(const std::map<tag_t, bool>& faceTags, const char* chainName)
    {
        for (std::map<tag_t, bool>::const_iterator it = faceTags.begin(); it != faceTags.end(); ++it)
        {
            Face* face = dynamic_cast<Face*>(ObjectFromTag(it->first));
            MarkerLineScanInfo scan = ScanMarkerLineOnFace(face);
            if (scan.matched)
            {
                return true;
            }
        }

        return false;
    }

    Face* FindMarkerLineFaceInFaceTags(const std::map<tag_t, bool>& faceTags, const char* chainName)
    {
        for (std::map<tag_t, bool>::const_iterator it = faceTags.begin(); it != faceTags.end(); ++it)
        {
            Face* face = dynamic_cast<Face*>(ObjectFromTag(it->first));
            MarkerLineScanInfo scan = ScanMarkerLineOnFace(face);
            if (scan.matched)
            {
                return face;
            }
        }

        return NULL;
    }

    FaceInfo SelectMarkerLineBaseFaceInChain(
        const std::vector<FaceCandidate>& allCandidates,
        const std::map<tag_t, bool>& chainTags,
        const char* chainName)
    {
        FaceInfo result;
        if (chainTags.empty())
        {
            return result;
        }

        Face* markerFace = FindMarkerLineFaceInFaceTags(chainTags, chainName);
        if (markerFace == NULL)
        {
            return result;
        }

        std::map<tag_t, bool> markerSideTags = FaceTagMap(CollectContinuousTangentFaces(markerFace));
        std::map<tag_t, bool> scoringTags;
        for (std::map<tag_t, bool>::const_iterator it = markerSideTags.begin(); it != markerSideTags.end(); ++it)
        {
            if (chainTags.count(it->first))
            {
                scoringTags[it->first] = true;
            }
        }
        if (scoringTags.empty())
        {
            return result;
        }

        std::vector<FaceCandidate> chainCandidates = FilterCandidatesByTags(allCandidates, scoringTags);
        if (!chainCandidates.empty())
        {
            return ScoreAndSelectBaseFace(chainCandidates, scoringTags);
        }

        return result;
    }

    FaceInfo ScoreAndSelectBaseFaceWithTrace(
        std::vector<FaceCandidate> candidates,
        const std::map<tag_t, bool>& scoringChainTags,
        const char* reason)
    {
        FaceInfo result;
        if (candidates.empty())
        {
            return result;
        }

        double minArea = std::numeric_limits<double>::max();
        for (size_t i = 0; i < candidates.size(); ++i)
        {
            if (candidates[i].area > 1e-9)
            {
                minArea = std::min(minArea, candidates[i].area);
            }
        }
        if (minArea == std::numeric_limits<double>::max())
        {
            minArea = 1.0;
        }

        std::map<tag_t, double> position = ChainPositionScores(candidates);
        for (size_t i = 0; i < candidates.size(); ++i)
        {
            const tag_t faceTag = candidates[i].face == NULL ? 0 : candidates[i].face->Tag();
            double areaScore = std::max(1.0, candidates[i].area / std::max(minArea, 1e-9));
            double positionScore = position.count(faceTag) ? position[faceTag] : 0.0;
            double innerLoopScore = HasInnerLoopScore(candidates[i].face) ? 1.0 : 0.0;
            double cylinderScore = AdjacentCylinderScore(candidates[i].face, scoringChainTags);
            candidates[i].score = areaScore + positionScore + innerLoopScore + cylinderScore;

            if (candidates[i].score > result.totalScore ||
                (std::fabs(candidates[i].score - result.totalScore) <= 1e-9 && candidates[i].area > result.areaScore))
            {
                result.face = candidates[i].face;
                result.type = candidates[i].type;
                result.areaScore = candidates[i].area;
                result.totalScore = candidates[i].score;
            }
        }

        return result;
    }

    std::vector<Face*> CollectContinuousPlanarTangentFaces(Face* startFace)
    {
        std::vector<Face*> result;
        if (startFace == NULL)
        {
            return result;
        }

        int startType = 0;
        if (!AskFaceType(startFace, &startType) || startType != UF_MODL_PLANAR_FACE)
        {
            return result;
        }

        std::queue<Face*> queue;
        std::set<tag_t> visited;
        queue.push(startFace);
        visited.insert(startFace->Tag());
        while (!queue.empty())
        {
            Face* current = queue.front();
            queue.pop();
            if (current == NULL)
            {
                continue;
            }

            result.push_back(current);
            std::vector<Edge*> edges = FaceEdgesByUf(current);
            for (size_t i = 0; i < edges.size(); ++i)
            {
                logical isSmooth = false;
                if (UF_MODL_ask_edge_smoothness(edges[i]->Tag(), 18.2, &isSmooth) != 0 || !isSmooth)
                {
                    continue;
                }

                std::vector<Face*> adjacent = AdjacentFacesByEdge(edges[i], current);
                for (size_t j = 0; j < adjacent.size(); ++j)
                {
                    Face* next = adjacent[j];
                    int nextType = 0;
                    if (next == NULL || visited.count(next->Tag()) ||
                        !AskFaceType(next, &nextType) || nextType != UF_MODL_PLANAR_FACE)
                    {
                        continue;
                    }

                    visited.insert(next->Tag());
                    queue.push(next);
                }
            }
        }

        return result;
    }

    MarkedChainSelection FindMarkedAttributeChain(Body* body)
    {
        MarkedChainSelection selection;
        if (body == NULL)
        {
            return selection;
        }

        std::map<tag_t, bool> checkedEdges;
        std::vector<Face*> bodyFaces = body->GetFaces();
        for (size_t i = 0; i < bodyFaces.size(); ++i)
        {
            Face* face = bodyFaces[i];
            if (face == NULL)
            {
                continue;
            }

            if (HasMarkedBaseAttribute(face->Tag()))
            {
                selection.chainTags = FaceTagMap(CollectContinuousTangentFaces(face));
                selection.found = !selection.chainTags.empty();
                selection.source = "face attribute";
                return selection;
            }

            std::vector<Edge*> edges = FaceEdgesByUf(face);
            for (size_t edgeIndex = 0; edgeIndex < edges.size(); ++edgeIndex)
            {
                Edge* edge = edges[edgeIndex];
                if (edge == NULL || checkedEdges[edge->Tag()])
                {
                    continue;
                }
                checkedEdges[edge->Tag()] = true;
                if (!HasMarkedBaseAttribute(edge->Tag()))
                {
                    continue;
                }

                std::vector<Face*> adjacentFaces = AdjacentFacesByEdge(edge, NULL);
                for (size_t faceIndex = 0; faceIndex < adjacentFaces.size(); ++faceIndex)
                {
                    Face* adjacentFace = adjacentFaces[faceIndex];
                    if (adjacentFace == NULL)
                    {
                        continue;
                    }

                    selection.chainTags = FaceTagMap(CollectContinuousTangentFaces(adjacentFace));
                    selection.found = !selection.chainTags.empty();
                    selection.source = "edge attribute";
                    return selection;
                }
            }
        }

        return selection;
    }

    std::map<tag_t, bool> BuildOppositeTangentChainTagsFromBaseFace(
        const std::vector<FaceCandidate>& allCandidates,
        const FaceCandidate& baseCandidate,
        const std::map<tag_t, bool>& baseChainTags)
    {
        std::map<tag_t, bool> empty;
        bool found = false;
        FaceCandidate oppositeFace = FindNearestParallelPlanarFace(
            allCandidates,
            baseCandidate,
            baseCandidate.area * 0.5,
            &found);
        if (!found || oppositeFace.face == NULL)
        {
            return empty;
        }

        std::map<tag_t, bool> oppositeTags = FaceTagMap(CollectContinuousTangentFaces(oppositeFace.face));
        return SubtractFaceTags(oppositeTags, baseChainTags);
    }

    FaceInfo SelectMarkerLineBaseFaceInBendChains(
        const std::vector<FaceCandidate>& allCandidates,
        const FaceCandidate& baseCandidate,
        const std::map<tag_t, bool>& baseChainTags)
    {
        FaceInfo baseChainMarker = SelectMarkerLineBaseFaceInChain(allCandidates, baseChainTags, "base");
        if (baseChainMarker.face != NULL)
        {
            return baseChainMarker;
        }

        std::map<tag_t, bool> oppositeChainTags = BuildOppositeTangentChainTagsFromBaseFace(allCandidates, baseCandidate, baseChainTags);
        FaceInfo oppositeMarker = SelectMarkerLineBaseFaceInChain(allCandidates, oppositeChainTags, "opposite");
        return oppositeMarker;
    }

    FaceInfo SelectMarkerLineBaseFace(const std::vector<FaceCandidate>& allCandidates)
    {
        FaceInfo result;
        FaceCandidate markerCandidate;
        bool foundMarker = false;
        for (size_t i = 0; i < allCandidates.size(); ++i)
        {
            if (HasMarkerLineOnFace(allCandidates[i].face) &&
                (!foundMarker || allCandidates[i].area > markerCandidate.area))
            {
                markerCandidate = allCandidates[i];
                foundMarker = true;
            }
        }

        if (!foundMarker || markerCandidate.face == NULL)
        {
            return result;
        }

        std::map<tag_t, bool> chainTags = FaceTagMap(CollectContinuousTangentFaces(markerCandidate.face));
        std::vector<FaceCandidate> chainCandidates = FilterCandidatesByTags(allCandidates, chainTags);
        if (!chainCandidates.empty())
        {
            return ScoreAndSelectBaseFace(chainCandidates, chainTags);
        }

        std::vector<FaceCandidate> markerCandidates;
        markerCandidates.push_back(markerCandidate);
        return ScoreAndSelectBaseFace(markerCandidates, std::map<tag_t, bool>());
    }

    FaceInfo SelectMarkedOrMarkerLineBaseFace(Body* body, const std::vector<FaceCandidate>& allCandidates)
    {
        FaceInfo markedFace = SelectMarkedFaceBaseFace(body, allCandidates);
        if (markedFace.face != NULL)
        {
            return markedFace;
        }

        return SelectMarkerLineBaseFace(allCandidates);
    }

    FaceInfo SelectConvertBaseFace(Body* body, const AutoConvertOptions& options)
    {
        FaceInfo result;
        std::vector<FaceCandidate> allCandidates = GetPlanarCandidates(body);
        if (allCandidates.empty())
        {
            if (body == NULL)
            {
                return result;
            }

            std::vector<Face*> faces = body->GetFaces();
            for (size_t i = 0; i < faces.size(); ++i)
            {
                int type = 0;
                if (!AskFaceType(faces[i], &type) || type != UF_MODL_CYLINDRICAL_FACE)
                {
                    continue;
                }

                double score = FaceAreaScore(faces[i]);
                if (score > result.areaScore)
                {
                    result.face = faces[i];
                    result.type = type;
                    result.areaScore = score;
                    result.totalScore = score;
                }
            }
            return result;
        }

        FaceInfo fallbackPlanarByBoxArea;
        double largestPlanarRealArea = -1.0;
        for (size_t i = 0; i < allCandidates.size(); ++i)
        {
            double realArea = -1.0;
            bool hasRealArea = TryMeasureFaceArea(allCandidates[i].face, &realArea);
            if (allCandidates[i].area > fallbackPlanarByBoxArea.areaScore)
            {
                fallbackPlanarByBoxArea.face = allCandidates[i].face;
                fallbackPlanarByBoxArea.type = allCandidates[i].type;
                fallbackPlanarByBoxArea.areaScore = allCandidates[i].area;
                fallbackPlanarByBoxArea.totalScore = allCandidates[i].area;
            }

            if (hasRealArea && realArea > largestPlanarRealArea)
            {
                result.face = allCandidates[i].face;
                result.type = allCandidates[i].type;
                result.areaScore = realArea;
                result.totalScore = realArea;
                largestPlanarRealArea = realArea;
            }
        }

        if (result.face == NULL)
        {
            result = fallbackPlanarByBoxArea;
        }

        FaceInfo bestLargeCylinder;
        if (body != NULL)
        {
            std::vector<Face*> faces = body->GetFaces();
            for (size_t i = 0; i < faces.size(); ++i)
            {
                int type = 0;
                double center[3] = { 0.0, 0.0, 0.0 };
                double normal[3] = { 0.0, 0.0, 1.0 };
                double radius = 0.0;
                if (!AskFaceData(faces[i], &type, center, normal, &radius) ||
                    type != UF_MODL_CYLINDRICAL_FACE ||
                    radius <= 5.0)
                {
                    continue;
                }

                double score = -1.0;
                bool hasRealArea = TryMeasureFaceArea(faces[i], &score);
                if (hasRealArea && score > bestLargeCylinder.areaScore)
                {
                    bestLargeCylinder.face = faces[i];
                    bestLargeCylinder.type = type;
                    bestLargeCylinder.areaScore = score;
                    bestLargeCylinder.totalScore = score;
                }
            }
        }

        if (bestLargeCylinder.face != NULL &&
            largestPlanarRealArea > 0.0 &&
            bestLargeCylinder.areaScore > largestPlanarRealArea)
        {
            return bestLargeCylinder;
        }

        return result;
    }

    bool BuildBendFaceTagSets(
        SheetmetalManager* manager,
        Body* body,
        std::map<tag_t, bool>* innerBendTags,
        std::map<tag_t, bool>* outerBendTags)
    {
        if (innerBendTags != NULL)
        {
            innerBendTags->clear();
        }
        if (outerBendTags != NULL)
        {
            outerBendTags->clear();
        }
        if (manager == NULL || body == NULL)
        {
            return false;
        }

        std::vector<Face*> innerBendFaces;
        std::vector<SheetmetalBendState> states;
        try
        {
            manager->GetInnerBendFaces(body, innerBendFaces, states);
        }
        catch (...)
        {
            return false;
        }

        for (size_t i = 0; i < innerBendFaces.size(); ++i)
        {
            Face* innerFace = innerBendFaces[i];
            if (innerFace == NULL)
            {
                continue;
            }
            if (i < states.size() && states[i] != SheetmetalBendStateBent)
            {
                continue;
            }

            if (innerBendTags != NULL)
            {
                (*innerBendTags)[innerFace->Tag()] = true;
            }

            try
            {
                Face* outerFace = manager->GetOppositeFace(innerFace);
                if (outerFace != NULL && outerBendTags != NULL)
                {
                    (*outerBendTags)[outerFace->Tag()] = true;
                }
            }
            catch (...)
            {
            }
        }

        return (innerBendTags != NULL && !innerBendTags->empty()) ||
            (outerBendTags != NULL && !outerBendTags->empty());
    }

    void AddPlanarChainsAdjacentToBends(
        const std::map<tag_t, bool>& bendTags,
        std::map<tag_t, bool>* sideTags)
    {
        if (sideTags == NULL)
        {
            return;
        }

        for (std::map<tag_t, bool>::const_iterator it = bendTags.begin(); it != bendTags.end(); ++it)
        {
            Face* bendFace = dynamic_cast<Face*>(ObjectFromTag(it->first));
            if (bendFace == NULL)
            {
                continue;
            }

            std::vector<Edge*> edges = FaceEdgesByUf(bendFace);
            for (size_t i = 0; i < edges.size(); ++i)
            {
                logical isSmooth = false;
                if (UF_MODL_ask_edge_smoothness(edges[i]->Tag(), 18.2, &isSmooth) != 0 || !isSmooth)
                {
                    continue;
                }

                std::vector<Face*> adjacentFaces = AdjacentFacesByEdge(edges[i], bendFace);
                for (size_t j = 0; j < adjacentFaces.size(); ++j)
                {
                    Face* adjacentFace = adjacentFaces[j];
                    int adjacentType = 0;
                    if (adjacentFace == NULL ||
                        !AskFaceType(adjacentFace, &adjacentType) ||
                        adjacentType != UF_MODL_PLANAR_FACE)
                    {
                        continue;
                    }

                    std::vector<Face*> planarChain = CollectContinuousPlanarTangentFaces(adjacentFace);
                    for (size_t k = 0; k < planarChain.size(); ++k)
                    {
                        if (planarChain[k] != NULL)
                        {
                            (*sideTags)[planarChain[k]->Tag()] = true;
                        }
                    }
                }
            }
        }
    }

    bool BuildBendSidePlanarTagSets(
        SheetmetalManager* manager,
        Body* body,
        std::map<tag_t, bool>* innerSideTags,
        std::map<tag_t, bool>* outerSideTags)
    {
        if (innerSideTags != NULL) innerSideTags->clear();
        if (outerSideTags != NULL) outerSideTags->clear();

        std::map<tag_t, bool> innerBendTags;
        std::map<tag_t, bool> outerBendTags;
        if (!BuildBendFaceTagSets(manager, body, &innerBendTags, &outerBendTags))
        {
            return false;
        }

        AddPlanarChainsAdjacentToBends(innerBendTags, innerSideTags);
        AddPlanarChainsAdjacentToBends(outerBendTags, outerSideTags);

        return (innerSideTags != NULL && !innerSideTags->empty()) ||
            (outerSideTags != NULL && !outerSideTags->empty());
    }

    std::string FaceTagKey(const std::map<tag_t, bool>& tags)
    {
        std::ostringstream key;
        for (std::map<tag_t, bool>::const_iterator it = tags.begin(); it != tags.end(); ++it)
        {
            if (it->second)
            {
                key << it->first << ",";
            }
        }
        return key.str();
    }

    std::vector<std::map<tag_t, bool> > BuildBendAdjacentPlanarChains(SheetmetalManager* manager, Body* body)
    {
        std::vector<std::map<tag_t, bool> > chains;
        std::set<std::string> seenKeys;

        std::map<tag_t, bool> innerBendTags;
        std::map<tag_t, bool> outerBendTags;
        if (!BuildBendFaceTagSets(manager, body, &innerBendTags, &outerBendTags))
        {
            return chains;
        }

        std::map<tag_t, bool> bendTags = innerBendTags;
        for (std::map<tag_t, bool>::const_iterator it = outerBendTags.begin(); it != outerBendTags.end(); ++it)
        {
            if (it->second)
            {
                bendTags[it->first] = true;
            }
        }

        for (std::map<tag_t, bool>::const_iterator it = bendTags.begin(); it != bendTags.end(); ++it)
        {
            Face* bendFace = dynamic_cast<Face*>(ObjectFromTag(it->first));
            if (bendFace == NULL)
            {
                continue;
            }

            std::vector<Edge*> edges = FaceEdgesByUf(bendFace);
            for (size_t i = 0; i < edges.size(); ++i)
            {
                logical isSmooth = false;
                if (UF_MODL_ask_edge_smoothness(edges[i]->Tag(), 18.2, &isSmooth) != 0 || !isSmooth)
                {
                    continue;
                }

                std::vector<Face*> adjacentFaces = AdjacentFacesByEdge(edges[i], bendFace);
                for (size_t j = 0; j < adjacentFaces.size(); ++j)
                {
                    Face* adjacentFace = adjacentFaces[j];
                    int adjacentType = 0;
                    if (adjacentFace == NULL ||
                        !AskFaceType(adjacentFace, &adjacentType) ||
                        adjacentType != UF_MODL_PLANAR_FACE)
                    {
                        continue;
                    }

                    std::map<tag_t, bool> chainTags = FaceTagMap(CollectContinuousPlanarTangentFaces(adjacentFace));
                    if (chainTags.empty())
                    {
                        continue;
                    }

                    std::string key = FaceTagKey(chainTags);
                    if (!key.empty() && seenKeys.insert(key).second)
                    {
                        chains.push_back(chainTags);
                    }
                }
            }
        }

        return chains;
    }

    FaceInfo SelectMarkerLineFaceFromPlanarChains(
        const std::vector<FaceCandidate>& allCandidates,
        const std::vector<std::map<tag_t, bool> >& chains,
        double markerGapTolerance)
    {
        FaceInfo best;
        for (size_t i = 0; i < chains.size(); ++i)
        {
            std::ostringstream chainName;
            chainName << "planarChain" << i;
            const bool hasMarker = ChainHasMarkerLineOrMarkedFace(chains[i], chainName.str().c_str(), markerGapTolerance);
            if (!hasMarker)
            {
                continue;
            }

            std::vector<FaceCandidate> chainCandidates = FilterCandidatesByTags(allCandidates, chains[i]);
            FaceInfo candidate = ScoreAndSelectBaseFace(chainCandidates, chains[i]);
            {
                std::ostringstream log;
                log << "base select planar chain marker hit chainIndex=" << i
                    << " chainFaces=" << chains[i].size()
                    << " candidateCount=" << chainCandidates.size()
                    << " resultFace=" << (candidate.face == NULL ? 0 : candidate.face->Tag())
                    << " resultScore=" << candidate.totalScore
                    << " resultArea=" << candidate.areaScore;
                AppendMarkerLineDebugLog(log.str());
            }

            if (candidate.face != NULL &&
                (candidate.totalScore > best.totalScore ||
                    (std::fabs(candidate.totalScore - best.totalScore) <= 1e-9 && candidate.areaScore > best.areaScore)))
            {
                best = candidate;
            }
        }

        if (best.face == NULL)
        {
            std::ostringstream log;
            log << "base select planar chain marker miss chainCount=" << chains.size();
            AppendMarkerLineDebugLog(log.str());
        }
        else
        {
            std::ostringstream log;
            log << "base select planar chain marker selected face=" << best.face->Tag()
                << " score=" << best.totalScore
                << " area=" << best.areaScore;
            AppendMarkerLineDebugLog(log.str());
        }

        return best;
    }

    FaceInfo SelectSheetmetalBaseFaceByBendDirection(
        SheetmetalManager* manager,
        Body* body,
        const AutoConvertOptions& options)
    {
        {
            std::ostringstream log;
            log << "base select enter body=" << (body == NULL ? 0 : body->Tag())
                << " markerLineFaceUp=" << (options.markerLineFaceUp ? 1 : 0)
                << " preferUpBends=" << (options.preferUpBends ? 1 : 0);
            AppendMarkerLineDebugLog(log.str());
        }

        FaceInfo fallback = SelectConvertBaseFace(body, options);
        if (fallback.face == NULL)
        {
            AppendMarkerLineDebugLog("base select fallback is null");
            return fallback;
        }

        std::vector<FaceCandidate> allCandidates = GetPlanarCandidates(body);
        {
            std::ostringstream log;
            log << "base select fallbackFace=" << fallback.face->Tag()
                << " fallbackArea=" << fallback.areaScore
                << " allPlanarCandidates=" << allCandidates.size();
            AppendMarkerLineDebugLog(log.str());
        }
        FaceCandidate* selectedCandidate = FindCandidateByTag(allCandidates, fallback.face->Tag());
        if (selectedCandidate == NULL)
        {
            std::ostringstream log;
            log << "base select fallback face not in planar candidates, return fallback face=" << fallback.face->Tag();
            AppendMarkerLineDebugLog(log.str());
            return fallback;
        }

        std::vector<Face*> tangentFaces = CollectContinuousTangentFaces(selectedCandidate->face);
        std::map<tag_t, bool> tangentTags = FaceTagMap(tangentFaces);
        {
            std::ostringstream log;
            log << "base select fallback tangent chain faceCount=" << tangentTags.size();
            AppendMarkerLineDebugLog(log.str());
        }

        const double markerGapTolerance = 0.05;
        if (options.markerLineFaceUp)
        {
            std::vector<std::map<tag_t, bool> > planarChains = BuildBendAdjacentPlanarChains(manager, body);
            {
                std::ostringstream log;
                log << "base select planar chains by tangent chain chainCount=" << planarChains.size();
                AppendMarkerLineDebugLog(log.str());
            }

            FaceInfo markedPlanarChainFace =
                SelectMarkerLineFaceFromPlanarChains(allCandidates, planarChains, markerGapTolerance);
            if (markedPlanarChainFace.face != NULL)
            {
                return markedPlanarChainFace;
            }
        }

        std::map<tag_t, bool> innerSideTags;
        std::map<tag_t, bool> outerSideTags;
        if (BuildBendSidePlanarTagSets(manager, body, &innerSideTags, &outerSideTags))
        {
            const bool innerHasMarker = false;
            const bool outerHasMarker = false;

            std::map<tag_t, bool> selectedSideTags;
            std::string selectedSideReason;
            if (innerHasMarker != outerHasMarker)
            {
                selectedSideTags = innerHasMarker ? innerSideTags : outerSideTags;
                selectedSideReason = innerHasMarker ? "only-inner-has-marker" : "only-outer-has-marker";
            }
            else if (innerHasMarker && outerHasMarker)
            {
                selectedSideTags = options.preferUpBends ? innerSideTags : outerSideTags;
                selectedSideReason = options.preferUpBends ? "both-have-marker-prefer-inner" : "both-have-marker-prefer-outer";
            }
            else
            {
                selectedSideTags = options.preferUpBends ? innerSideTags : outerSideTags;
                selectedSideReason = options.preferUpBends ? "no-marker-prefer-inner" : "no-marker-prefer-outer";
            }

            std::vector<FaceCandidate> sideCandidates = FilterCandidatesByTags(allCandidates, selectedSideTags);
            FaceInfo sideFace = ScoreAndSelectBaseFace(sideCandidates, selectedSideTags);
            {
                std::ostringstream log;
                log << "base select bend side decision innerFaces=" << innerSideTags.size()
                    << " outerFaces=" << outerSideTags.size()
                    << " innerHasMarker=" << (innerHasMarker ? 1 : 0)
                    << " outerHasMarker=" << (outerHasMarker ? 1 : 0)
                    << " selectedReason=" << selectedSideReason
                    << " selectedFaces=" << selectedSideTags.size()
                    << " candidateCount=" << sideCandidates.size()
                    << " resultFace=" << (sideFace.face == NULL ? 0 : sideFace.face->Tag())
                    << " resultScore=" << sideFace.totalScore
                    << " resultArea=" << sideFace.areaScore;
                AppendMarkerLineDebugLog(log.str());
            }
            if (sideFace.face != NULL)
            {
                return sideFace;
            }
        }
        else
        {
            AppendMarkerLineDebugLog("base select bend side chains not built; fallback to tangent chain");
        }

        std::vector<FaceCandidate> chainCandidates = FilterCandidatesByTags(allCandidates, tangentTags);
        if (options.markerLineFaceUp &&
            ChainHasMarkerLineOrMarkedFace(tangentTags, "largestPlanar", 0.05) &&
            !chainCandidates.empty())
        {
            FaceInfo markerFace = ScoreAndSelectBaseFace(chainCandidates, tangentTags);
            {
                std::ostringstream log;
                log << "base select fallback tangent has marker resultFace="
                    << (markerFace.face == NULL ? 0 : markerFace.face->Tag())
                    << " candidateCount=" << chainCandidates.size();
                AppendMarkerLineDebugLog(log.str());
            }
            return markerFace.face == NULL ? fallback : markerFace;
        }

        if (options.markerLineFaceUp)
        {
            bool foundParallel = false;
            FaceCandidate parallelFace = FindNearestParallelPlanarFace(
                allCandidates,
                *selectedCandidate,
                selectedCandidate->area * 0.5,
                &foundParallel);
            std::map<tag_t, bool> parallelTags;
            if (foundParallel && parallelFace.face != NULL)
            {
                parallelTags[parallelFace.face->Tag()] = true;
                {
                    std::ostringstream log;
                    log << "base select nearest parallel scan face=" << parallelFace.face->Tag()
                        << " area=" << parallelFace.area
                        << " minArea=" << (selectedCandidate->area * 0.5);
                    AppendMarkerLineDebugLog(log.str());
                }

                std::vector<FaceCandidate> parallelCandidates = FilterCandidatesByTags(allCandidates, parallelTags);
                if (ChainHasMarkerLineOrMarkedFace(parallelTags, "nearestParallelPlanar", 0.05) &&
                    !parallelCandidates.empty())
                {
                    FaceInfo markerFace = ScoreAndSelectBaseFace(parallelCandidates, parallelTags);
                    {
                        std::ostringstream log;
                        log << "base select nearest parallel has marker resultFace="
                            << (markerFace.face == NULL ? 0 : markerFace.face->Tag())
                            << " candidateCount=" << parallelCandidates.size();
                        AppendMarkerLineDebugLog(log.str());
                    }
                    return markerFace.face == NULL ? fallback : markerFace;
                }
            }
            else
            {
                std::ostringstream log;
                log << "base select nearest parallel scan not found minArea="
                    << (selectedCandidate->area * 0.5);
                AppendMarkerLineDebugLog(log.str());
            }
        }

        FaceInfo largestPlanar = ScoreAndSelectBaseFace(chainCandidates, tangentTags);
        {
            std::ostringstream log;
            log << "base select final no marker resultFace="
                << (largestPlanar.face == NULL ? (fallback.face == NULL ? 0 : fallback.face->Tag()) : largestPlanar.face->Tag())
                << " chainCandidateCount=" << chainCandidates.size()
                << " return=" << (largestPlanar.face == NULL ? "fallback" : "largestPlanar");
            AppendMarkerLineDebugLog(log.str());
        }
        return largestPlanar.face == NULL ? fallback : largestPlanar;
    }

    std::map<tag_t, bool> BuildScoringTangentChainTags(const std::vector<FaceCandidate>& candidates)
    {
        std::map<tag_t, bool> tags;
        for (size_t i = 0; i < candidates.size(); ++i)
        {
            if (candidates[i].face == NULL)
            {
                continue;
            }

            std::vector<Face*> chain = CollectContinuousTangentFaces(candidates[i].face);
            for (size_t j = 0; j < chain.size(); ++j)
            {
                if (chain[j] != NULL)
                {
                    tags[chain[j]->Tag()] = true;
                }
            }
        }

        if (tags.empty())
        {
            for (size_t i = 0; i < candidates.size(); ++i)
            {
                if (candidates[i].face != NULL)
                {
                    tags[candidates[i].face->Tag()] = true;
                }
            }
        }

        return tags;
    }

    FaceInfo SelectScoredBaseFace(Body* body, const AutoConvertOptions& options)
    {
        FaceInfo result;
        std::vector<FaceCandidate> allCandidates = GetPlanarCandidates(body);
        if (allCandidates.empty())
        {
            if (body == NULL)
            {
                return result;
            }
            std::vector<Face*> faces = body->GetFaces();
            for (size_t i = 0; i < faces.size(); ++i)
            {
                int type = 0;
                if (!AskFaceType(faces[i], &type) || type != UF_MODL_CYLINDRICAL_FACE)
                {
                    continue;
                }
                double score = FaceAreaScore(faces[i]);
                if (score > result.areaScore)
                {
                    result.face = faces[i];
                    result.type = type;
                    result.areaScore = score;
                    result.totalScore = score;
                }
            }
            return result;
        }

        if (options.markerLineFaceUp)
        {
            FaceInfo markedFace = SelectMarkedFaceBaseFace(body, allCandidates);
            if (markedFace.face != NULL)
            {
                return markedFace;
            }
        }

        std::vector<FaceCandidate> largestGroup = GetLargestPlanarFaceGroup(allCandidates);
        if (!largestGroup.empty() && largestGroup[0].face != NULL)
        {
            std::vector<Face*> tangentFaces = CollectContinuousTangentFaces(largestGroup[0].face);
            std::map<tag_t, bool> tangentTags = FaceTagMap(tangentFaces);
            if (options.markerLineFaceUp)
            {
                FaceInfo markerLineFace = SelectMarkerLineBaseFaceInBendChains(allCandidates, largestGroup[0], tangentTags);
                if (markerLineFace.face != NULL)
                {
                    return markerLineFace;
                }
            }

            std::vector<FaceCandidate> baseDirectionCandidates = FilterCandidatesByTags(allCandidates, tangentTags);
            if (!baseDirectionCandidates.empty())
            {
                int positiveSide = 0;
                int negativeSide = 0;
                CountBendSides(largestGroup[0], &positiveSide, &negativeSide);
                if (positiveSide != negativeSide)
                {
                    bool baseIsDownBends = negativeSide > positiveSide;
                    bool useParallelSide = baseIsDownBends != !options.preferUpBends;
                    std::vector<FaceCandidate> directionCandidates = baseDirectionCandidates;
                    std::map<tag_t, bool> scoringChainTags = tangentTags;
                    if (useParallelSide)
                    {
                        directionCandidates.clear();
                        std::map<tag_t, bool> seen;
                        for (size_t i = 0; i < baseDirectionCandidates.size(); ++i)
                        {
                            bool found = false;
                            FaceCandidate parallelFace = FindNearestParallelPlanarFace(
                                allCandidates,
                                baseDirectionCandidates[i],
                                baseDirectionCandidates[i].area * 0.5,
                                &found);
                            if (found && parallelFace.face != NULL && !seen[parallelFace.face->Tag()])
                            {
                                directionCandidates.push_back(parallelFace);
                                seen[parallelFace.face->Tag()] = true;
                            }
                        }
                        scoringChainTags = BuildScoringTangentChainTags(directionCandidates);
                    }

                    result = ScoreAndSelectBaseFace(directionCandidates, scoringChainTags);
                    if (result.face != NULL)
                    {
                        return result;
                    }
                }
            }
        }

        return ScoreAndSelectBaseFace(largestGroup.empty() ? allCandidates : largestGroup, std::map<tag_t, bool>());
    }

    RuleConfig LoadRuleConfig()
    {
        RuleConfig config;
        std::string path = FindRulesIniPath();
        std::string ini = path.empty() ? std::string() : ReadAllText(path);
        if (ini.empty())
        {
            return config;
        }

        zhihui_bend_rules_ini::LoadIntoRuleConfig<RuleConfig, BendRule>(ini, &config);
        return config;
    }

    bool WithinRange(double value, bool hasMin, double minValue, bool hasMax, double maxValue)
    {
        if (hasMin && value < minValue)
        {
            return false;
        }
        if (hasMax && value > maxValue)
        {
            return false;
        }
        return true;
    }

    bool ContainsTextNoCase(const std::string& text, const char* token)
    {
        std::string left = text;
        std::string right = token == NULL ? std::string() : std::string(token);
        std::transform(left.begin(), left.end(), left.begin(), ::tolower);
        std::transform(right.begin(), right.end(), right.begin(), ::tolower);
        return left.find(right) != std::string::npos;
    }

    bool IsLargeArcBend(const BendFaceRecord& record, const RuleConfig& config)
    {
        bool byAbsolute = config.useAbsoluteLargeArc &&
            config.absoluteLargeArcRadius > 0.0 &&
            record.innerRadius >= config.absoluteLargeArcRadius;
        bool byRatio = config.useRatioLargeArc &&
            config.ratioLargeArc > 0.0 &&
            record.thickness > 1e-9 &&
            record.innerRadius / record.thickness >= config.ratioLargeArc;
        return byAbsolute || byRatio;
    }

    std::string NormalizeMaterialKey(const std::string& material)
    {
        std::string value = NormalizeUtf8Message(TrimCopy(material));
        for (size_t i = 0; i < value.size(); ++i)
        {
            if (value[i] >= 'a' && value[i] <= 'z')
            {
                value[i] = static_cast<char>(value[i] - 'a' + 'A');
            }
        }
        return value;
    }

    bool IsUnspecifiedMaterial(const std::string& material)
    {
        std::string value = NormalizeMaterialKey(material);
        return value.empty() ||
            value.find("\xE6\x9C\xAA\xE6\x8C\x87\xE5\xAE\x9A") != std::string::npos ||
            value.find("UNSPECIFIED") != std::string::npos;
    }

    bool IsSameMaterial(const std::string& left, const std::string& right)
    {
        if (IsUnspecifiedMaterial(left) || IsUnspecifiedMaterial(right))
        {
            return false;
        }
        return NormalizeMaterialKey(left) == NormalizeMaterialKey(right);
    }

    bool RuleMatchesBendShape(const BendRule& rule, const BendFaceRecord& record, bool largeArc)
    {
        bool pressEdgeRule = ContainsTextNoCase(rule.note, "[PressEdge]");
        bool largeArcRule = ContainsTextNoCase(rule.note, "[LargeArc]");
        bool smallArcRule = ContainsTextNoCase(rule.note, "[SmallArc]");
        if (pressEdgeRule)
        {
            return false;
        }
        if (largeArc && !largeArcRule)
        {
            return false;
        }
        if (!largeArc && largeArcRule)
        {
            return false;
        }
        if (smallArcRule && largeArc)
        {
            return false;
        }
        return WithinRange(record.angleDeg, rule.hasAngleMin, rule.angleMin, rule.hasAngleMax, rule.angleMax) &&
            WithinRange(record.thickness, rule.hasThicknessMin, rule.thicknessMin, rule.hasThicknessMax, rule.thicknessMax) &&
            WithinRange(record.innerRadius, rule.hasRadiusMin, rule.radiusMin, rule.hasRadiusMax, rule.radiusMax);
    }

    const BendRule* MatchRuleByMaterialMode(const RuleConfig& config, const BendFaceRecord& record, bool useUnspecifiedMaterial, bool largeArc)
    {
        for (size_t i = 0; i < config.rules.size(); ++i)
        {
            const BendRule& rule = config.rules[i];
            if (!rule.enabled || rule.fallback)
            {
                continue;
            }

            bool materialOk = useUnspecifiedMaterial ?
                IsUnspecifiedMaterial(rule.material) :
                IsSameMaterial(rule.material, record.material);
            if (!materialOk)
            {
                continue;
            }

            if (RuleMatchesBendShape(rule, record, largeArc))
            {
                return &rule;
            }
        }
        return NULL;
    }

    bool HasMaterialRules(const RuleConfig& config, const BendFaceRecord& record)
    {
        for (size_t i = 0; i < config.rules.size(); ++i)
        {
            const BendRule& rule = config.rules[i];
            if (rule.enabled && !rule.fallback && IsSameMaterial(rule.material, record.material))
            {
                return true;
            }
        }
        return false;
    }

    const BendRule* MatchRule(const RuleConfig& config, const BendFaceRecord& record)
    {
        const BendRule* fallback = NULL;
        bool largeArc = IsLargeArcBend(record, config);
        for (size_t i = 0; i < config.rules.size(); ++i)
        {
            const BendRule& rule = config.rules[i];
            if (rule.enabled && rule.fallback)
            {
                fallback = &rule;
            }
        }

        const BendRule* materialRule = MatchRuleByMaterialMode(config, record, false, largeArc);
        if (materialRule != NULL)
        {
            return materialRule;
        }

        if (!HasMaterialRules(config, record))
        {
            const BendRule* unspecifiedRule = MatchRuleByMaterialMode(config, record, true, largeArc);
            if (unspecifiedRule != NULL)
            {
                return unspecifiedRule;
            }
        }
        return fallback;
    }

    bool SafeAngleRadians(double angleDeg, double* radians)
    {
        if (radians == NULL || angleDeg <= 0.001 || angleDeg >= 179.999)
        {
            return false;
        }

        *radians = angleDeg * 3.14159265358979323846 / 180.0;
        return true;
    }

    class FormulaParser
    {
    public:
        FormulaParser(const std::string& text, const std::map<std::string, double>& variables) :
            text_(text),
            variables_(variables),
            pos_(0)
        {
        }

        bool Evaluate(double* value)
        {
            if (value == NULL)
            {
                return false;
            }

            double result = 0.0;
            if (!ParseExpression(&result))
            {
                return false;
            }
            SkipSpaces();
            if (pos_ != text_.size())
            {
                return false;
            }
            *value = result;
            return !std::isnan(result) && !std::isinf(result);
        }

    private:
        const std::string& text_;
        const std::map<std::string, double>& variables_;
        size_t pos_;

        void SkipSpaces()
        {
            while (pos_ < text_.size() && static_cast<unsigned char>(text_[pos_]) <= ' ')
            {
                ++pos_;
            }
        }

        bool ParseExpression(double* value)
        {
            if (!ParseTerm(value))
            {
                return false;
            }

            while (true)
            {
                SkipSpaces();
                if (pos_ >= text_.size() || (text_[pos_] != '+' && text_[pos_] != '-'))
                {
                    return true;
                }

                char op = text_[pos_++];
                double right = 0.0;
                if (!ParseTerm(&right))
                {
                    return false;
                }
                *value = op == '+' ? *value + right : *value - right;
            }
        }

        bool ParseTerm(double* value)
        {
            if (!ParseFactor(value))
            {
                return false;
            }

            while (true)
            {
                SkipSpaces();
                if (pos_ >= text_.size() || (text_[pos_] != '*' && text_[pos_] != '/'))
                {
                    return true;
                }

                char op = text_[pos_++];
                double right = 0.0;
                if (!ParseFactor(&right))
                {
                    return false;
                }
                if (op == '/')
                {
                    if (std::fabs(right) <= 1.0e-12)
                    {
                        return false;
                    }
                    *value /= right;
                }
                else
                {
                    *value *= right;
                }
            }
        }

        bool ParseFactor(double* value)
        {
            SkipSpaces();
            if (pos_ >= text_.size())
            {
                return false;
            }

            if (text_[pos_] == '+')
            {
                ++pos_;
                return ParseFactor(value);
            }
            if (text_[pos_] == '-')
            {
                ++pos_;
                if (!ParseFactor(value))
                {
                    return false;
                }
                *value = -*value;
                return true;
            }
            if (text_[pos_] == '(')
            {
                ++pos_;
                if (!ParseExpression(value))
                {
                    return false;
                }
                SkipSpaces();
                if (pos_ >= text_.size() || text_[pos_] != ')')
                {
                    return false;
                }
                ++pos_;
                return true;
            }

            if (std::isdigit(static_cast<unsigned char>(text_[pos_])) || text_[pos_] == '.')
            {
                const char* start = text_.c_str() + pos_;
                char* end = NULL;
                double parsed = std::strtod(start, &end);
                if (end == start)
                {
                    return false;
                }
                pos_ += static_cast<size_t>(end - start);
                *value = parsed;
                return true;
            }

            return ParseIdentifier(value);
        }

        bool ParseIdentifier(double* value)
        {
            size_t start = pos_;
            while (pos_ < text_.size())
            {
                unsigned char c = static_cast<unsigned char>(text_[pos_]);
                if (c <= ' ' || text_[pos_] == '+' || text_[pos_] == '-' ||
                    text_[pos_] == '*' || text_[pos_] == '/' ||
                    text_[pos_] == '(' || text_[pos_] == ')')
                {
                    break;
                }
                ++pos_;
            }

            std::string name = TrimCopy(text_.substr(start, pos_ - start));
            if (name.empty())
            {
                return false;
            }

            std::string upper = name;
            std::transform(upper.begin(), upper.end(), upper.begin(),
                [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

            SkipSpaces();
            if (pos_ < text_.size() && text_[pos_] == '(')
            {
                ++pos_;
                double argument = 0.0;
                if (!ParseExpression(&argument))
                {
                    return false;
                }
                SkipSpaces();
                if (pos_ >= text_.size() || text_[pos_] != ')')
                {
                    return false;
                }
                ++pos_;

                if (upper == "TAN")
                {
                    *value = std::tan(argument);
                    return true;
                }
                if (upper == "SIN")
                {
                    *value = std::sin(argument);
                    return true;
                }
                if (upper == "COS")
                {
                    *value = std::cos(argument);
                    return true;
                }
                return false;
            }

            if (upper == "PI")
            {
                *value = 3.14159265358979323846;
                return true;
            }

            std::map<std::string, double>::const_iterator it = variables_.find(name);
            if (it == variables_.end())
            {
                it = variables_.find(upper);
            }
            if (it == variables_.end())
            {
                return false;
            }

            *value = it->second;
            return true;
        }
    };

    bool EvaluateKFormulaText(
        const std::string& formula,
        double deduction,
        double angleDeg,
        double innerRadius,
        double thickness,
        double* targetK)
    {
        std::string expression = TrimCopy(formula);
        if (expression.empty() || thickness <= 0.0)
        {
            return false;
        }

        std::map<std::string, double> variables;
        variables["扣除"] = deduction;
        variables["角度"] = angleDeg;
        variables["内R"] = innerRadius;
        variables["板厚"] = thickness;
        variables["DEDUCTION"] = deduction;
        variables["Q"] = deduction;
        variables["ANGLE"] = angleDeg;
        variables["A"] = angleDeg;
        variables["R"] = innerRadius;
        variables["THICKNESS"] = thickness;
        variables["T"] = thickness;

        double value = 0.0;
        FormulaParser parser(expression, variables);
        if (!parser.Evaluate(&value) || value < 0.0 || value > 1.0)
        {
            return false;
        }
        if (targetK != NULL)
        {
            *targetK = value;
        }
        return true;
    }

    bool CalculateTargetK(const BendRule& rule, BendFaceRecord* record)
    {
        if (record == NULL)
        {
            return false;
        }

        double effectiveRadius = rule.hasRadiusOverride ? rule.radiusOverride : record->innerRadius;
        record->targetRadius = effectiveRadius;
        if (ContainsTextNoCase(rule.method, "KFactor"))
        {
            record->targetK = rule.value;
            return record->targetK >= 0.0 && record->targetK <= 1.0;
        }

        if (!ContainsTextNoCase(rule.method, "Deduction") || record->thickness <= 0.0)
        {
            return false;
        }

        double angleRad = 0.0;
        if (!SafeAngleRadians(record->angleDeg, &angleRad))
        {
            return false;
        }

        double bendAllowance = 2.0 * (effectiveRadius + record->thickness) * std::tan(angleRad / 2.0) - rule.value;
        record->targetK = (bendAllowance / angleRad - effectiveRadius) / record->thickness;
        return !std::isnan(record->targetK) && !std::isinf(record->targetK) &&
            record->targetK >= 0.0 && record->targetK <= 1.0;
    }

    std::string FormatDouble(double value, int precision)
    {
        std::ostringstream stream;
        stream << std::fixed << std::setprecision(precision) << value;
        return stream.str();
    }

    bool UpdateBodyZThicknessAttribute(SheetmetalManager* manager, Body* body)
    {
        if (manager == NULL || body == NULL)
        {
            return false;
        }

        double thickness = 0.0;
        try
        {
            thickness = manager->GetBodyThickness(body);
        }
        catch (...)
        {
            return false;
        }

        if (thickness <= 0.0)
        {
            return false;
        }

        std::string text = FormatDouble(thickness, 1);
        body->SetUserAttribute("Z", -1, text.c_str(), Update::OptionNow);
        return true;
    }

    bool UpdateBodyFlatPatternSizeAttribute(Body* body, const FlatSolidThicknessCheckInfo& flatCheck)
    {
        if (body == NULL || flatCheck.flatSizeX <= 1e-6 || flatCheck.flatSizeY <= 1e-6)
        {
            return false;
        }

        std::string text = FormatDouble(flatCheck.flatSizeX, 1) + "*" + FormatDouble(flatCheck.flatSizeY, 1);
        body->SetUserAttribute(u8"展开尺寸", -1, text.c_str(), Update::OptionNow);
        return true;
    }

    std::string UiTextToUtf8(const NXString& value)
    {
        const char* text = value.GetUTF8Text();
        if (text != NULL && text[0] != '\0')
        {
            return NormalizeUtf8Message(text);
        }

        text = value.GetLocaleText();
        if (text != NULL && text[0] != '\0')
        {
            return NormalizeUtf8Message(text);
        }

        text = value.GetText();
        return text == NULL ? std::string() : NormalizeUtf8Message(text);
    }

    std::string RuleMethodDisplay(const std::string& method)
    {
        return method == "Deduction" ? std::string(u8"扣除") : std::string(u8"K因子");
    }

    std::string RuleBoolDisplay(bool value)
    {
        return value ? std::string(u8"是") : std::string(u8"否");
    }

    bool RuleBoolValue(const std::string& value, bool defaultValue)
    {
        std::string text = TrimCopy(value);
        std::transform(text.begin(), text.end(), text.begin(), ::tolower);
        if (text == u8"是" || text == "true" || text == "1" || text == "yes" || text == "y")
        {
            return true;
        }
        if (text == u8"否" || text == "false" || text == "0" || text == "no" || text == "n")
        {
            return false;
        }
        return defaultValue;
    }

    std::string NumberText(double value)
    {
        return FormatDouble(value, 2);
    }

    std::string OptionalRuleNumber(bool hasValue, double value)
    {
        return hasValue ? NumberText(value) : std::string();
    }

    void AppendIniOptionalNumber(std::ostringstream& stream, const char* name, bool hasValue, double value)
    {
        stream << ";" << name << "=";
        if (hasValue)
        {
            stream << NumberText(value);
        }
    }

    RuleConfig LoadRuleConfigForEditor()
    {
        RuleConfig config;
        std::string path = FindRulesIniPath();
        std::string ini = path.empty() ? std::string() : ReadAllText(path);
        if (ini.empty())
        {
            return config;
        }

        zhihui_bend_rules_ini::LoadIntoRuleConfig<RuleConfig, BendRule>(ini, &config);
        return config;
    }

    std::string JsonEscape(const std::string& value)
    {
        std::ostringstream stream;
        for (size_t i = 0; i < value.size(); ++i)
        {
            unsigned char c = static_cast<unsigned char>(value[i]);
            switch (c)
            {
            case '"': stream << "\\\""; break;
            case '\\': stream << "\\\\"; break;
            case '\b': stream << "\\b"; break;
            case '\f': stream << "\\f"; break;
            case '\n': stream << "\\n"; break;
            case '\r': stream << "\\r"; break;
            case '\t': stream << "\\t"; break;
            default:
                if (c < 0x20)
                {
                    stream << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c) << std::dec << std::setfill(' ');
                }
                else
                {
                    stream << value[i];
                }
                break;
            }
        }
        return stream.str();
    }

    void AppendJsonOptionalNumber(std::ostringstream& stream, const char* name, bool hasValue, double value)
    {
        stream << ",\"" << name << "\":";
        if (hasValue)
        {
            stream << NumberText(value);
        }
        else
        {
            stream << "null";
        }
    }

    std::string BuildRulesJsonArray(const std::vector<BendRule>& rules)
    {
        std::ostringstream stream;
        stream << "[\n";
        for (size_t i = 0; i < rules.size(); ++i)
        {
            const BendRule& rule = rules[i];
            stream << "  {\"Enabled\":" << (rule.enabled ? "true" : "false");
            stream << ",\"RuleName\":\"" << JsonEscape(rule.name) << "\"";
            AppendJsonOptionalNumber(stream, "AngleMinDeg", rule.hasAngleMin, rule.angleMin);
            AppendJsonOptionalNumber(stream, "AngleMaxDeg", rule.hasAngleMax, rule.angleMax);
            AppendJsonOptionalNumber(stream, "ThicknessMin", rule.hasThicknessMin, rule.thicknessMin);
            AppendJsonOptionalNumber(stream, "ThicknessMax", rule.hasThicknessMax, rule.thicknessMax);
            AppendJsonOptionalNumber(stream, "InnerRadiusMin", rule.hasRadiusMin, rule.radiusMin);
            AppendJsonOptionalNumber(stream, "InnerRadiusMax", rule.hasRadiusMax, rule.radiusMax);
            stream << ",\"Material\":\"" << JsonEscape(rule.material) << "\"";
            stream << ",\"Method\":\"" << JsonEscape(rule.method.empty() ? std::string("KFactor") : rule.method) << "\"";
            stream << ",\"Value\":" << NumberText(rule.value);
            AppendJsonOptionalNumber(stream, "RadiusOverride", rule.hasRadiusOverride, rule.radiusOverride);
            stream << ",\"IsFallback\":" << (rule.fallback ? "true" : "false");
            stream << ",\"Note\":\"" << JsonEscape(rule.note) << "\"}";
            if (i + 1 < rules.size())
            {
                stream << ",";
            }
            stream << "\n";
        }
        stream << "]";
        return stream.str();
    }

    bool FindJsonArraySpan(const std::string& json, const std::string& name, size_t* begin, size_t* end)
    {
        std::regex pattern("\\\"" + name + "\\\"\\s*:\\s*\\[");
        std::smatch match;
        if (!std::regex_search(json, match, pattern))
        {
            return false;
        }

        size_t open = static_cast<size_t>(match.position(0) + match.length(0) - 1);
        int depth = 0;
        bool inString = false;
        bool escape = false;
        for (size_t i = open; i < json.size(); ++i)
        {
            char c = json[i];
            if (inString)
            {
                if (escape)
                {
                    escape = false;
                }
                else if (c == '\\')
                {
                    escape = true;
                }
                else if (c == '"')
                {
                    inString = false;
                }
                continue;
            }

            if (c == '"')
            {
                inString = true;
            }
            else if (c == '[')
            {
                ++depth;
            }
            else if (c == ']')
            {
                --depth;
                if (depth == 0)
                {
                    *begin = open;
                    *end = i + 1;
                    return true;
                }
            }
        }

        return false;
    }

    std::string RulesJsonSavePath()
    {
        std::string path = FindRulesIniPath();
        if (!path.empty())
        {
            return path;
        }
        return std::string("D:\\UG智辉钣金插件\\config\\") + PiLianRulesIniFileName;
    }

    bool SaveRuleConfigToJson(const RuleConfig& config, std::string* pathOut, std::string* error)
    {
        std::string path = RulesJsonSavePath();
        std::ostringstream ini;
        ini << "\xEF\xBB\xBF";
        ini << "# 智辉钣金批量转钣金折弯规则表\n";
        ini << "# UG 内置表格保存的明细规则。\n\n";
        ini << u8"[大圆弧判定]\n";
        ini << u8"使用绝对内R=" << RuleBoolDisplay(config.useAbsoluteLargeArc) << "\n";
        ini << u8"最小半径=" << NumberText(config.absoluteLargeArcRadius) << "\n";
        ini << u8"使用R比厚度=" << RuleBoolDisplay(config.useRatioLargeArc) << "\n";
        ini << u8"R比厚度阈值=" << NumberText(config.ratioLargeArc) << "\n\n";
        ini << u8"[规则]\n";
        for (size_t i = 0; i < config.rules.size(); ++i)
        {
            const BendRule& rule = config.rules[i];
            ini << u8"规则" << (i + 1) << u8"=启用=" << RuleBoolDisplay(rule.enabled);
            ini << u8";名称=" << rule.name;
            ini << u8";材料=" << rule.material;
            ini << u8";方法=" << RuleMethodDisplay(rule.method);
            ini << u8";值=" << NumberText(rule.value);
            AppendIniOptionalNumber(ini, u8"角度最小", rule.hasAngleMin, rule.angleMin);
            AppendIniOptionalNumber(ini, u8"角度最大", rule.hasAngleMax, rule.angleMax);
            AppendIniOptionalNumber(ini, u8"厚度最小", rule.hasThicknessMin, rule.thicknessMin);
            AppendIniOptionalNumber(ini, u8"厚度最大", rule.hasThicknessMax, rule.thicknessMax);
            AppendIniOptionalNumber(ini, u8"内R最小", rule.hasRadiusMin, rule.radiusMin);
            AppendIniOptionalNumber(ini, u8"内R最大", rule.hasRadiusMax, rule.radiusMax);
            AppendIniOptionalNumber(ini, u8"半径覆盖", rule.hasRadiusOverride, rule.radiusOverride);
            ini << u8";兜底=" << RuleBoolDisplay(rule.fallback);
            ini << u8";备注=" << rule.note << "\n";
        }

        std::string text = ini.str();
        if (!WriteAllText(path, text))
        {
            if (error != NULL)
            {
                *error = u8"无法写入规则配置：" + path;
            }
            return false;
        }
        if (pathOut != NULL)
        {
            *pathOut = path;
        }
        return true;
    }

    void TrySetBlockString(UIBlock* block, const char* propertyName, const char* value)
    {
        if (block == NULL)
        {
            return;
        }

        PropertyList* props = NULL;
        try
        {
            props = block->GetProperties();
            props->SetString(propertyName, value == NULL ? "" : value);
        }
        catch (...)
        {
        }
        if (props != NULL)
        {
            delete props;
        }
    }

    void TrySetBlockVisible(UIBlock* block, bool visible)
    {
        if (block == NULL)
        {
            return;
        }

        const char* names[] = { "Show", "Visibility" };
        for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
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
            {
                delete props;
            }
        }
    }

    void TrySetBlockEnabled(UIBlock* block, bool enabled)
    {
        if (block == NULL)
        {
            return;
        }

        const char* names[] = { "Enable", "Sensitivity" };
        for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
        {
            PropertyList* props = NULL;
            try
            {
                props = block->GetProperties();
                props->SetLogical(names[i], enabled);
            }
            catch (...)
            {
            }
            if (props != NULL)
            {
                delete props;
            }
        }
    }

    void SetNodeColumnText(Node* node, int columnID, const std::string& text)
    {
        if (node != NULL)
        {
            node->SetColumnDisplayText(columnID, U8(text.empty() ? std::string(" ") : text));
        }
    }

    typedef zhihui_bend_rules_ini::CoefficientRow CoefficientRow;
    typedef zhihui_bend_rules_ini::ConditionRow ConditionRuleRow;

    std::string RulesIniSavePath()
    {
        std::string existing = FindRulesIniPath();
        if (!existing.empty())
        {
            return existing;
        }

        return std::string("D:\\UG智辉钣金插件\\config\\") + PiLianRulesIniFileName;
    }

    std::vector<CoefficientRow> LoadCoefficientRowsFromIni()
    {
        std::string path = FindRulesIniPath();
        std::string ini = path.empty() ? std::string() : ReadAllText(path);
        return ini.empty() ? std::vector<CoefficientRow>() : zhihui_bend_rules_ini::LoadCoefficientRowsFromText(ini);
    }

    std::vector<ConditionRuleRow> LoadConditionRowsFromIni()
    {
        std::string path = FindRulesIniPath();
        std::string ini = path.empty() ? std::string() : ReadAllText(path);
        return ini.empty() ? zhihui_bend_rules_ini::DefaultConditionRows() : zhihui_bend_rules_ini::LoadConditionRowsFromText(ini);
    }

    std::string CoefficientNumberText(double value)
    {
        return FormatDouble(value, 2);
    }

    std::string OptionalCoefficientNumber(bool hasValue, double value)
    {
        return hasValue ? CoefficientNumberText(value) : std::string();
    }

    bool ParseOptionalTextNumber(const std::string& text, bool* hasValue, double* value)
    {
        std::string trimmed = TrimCopy(text);
        if (trimmed.empty())
        {
            if (hasValue != NULL) *hasValue = false;
            if (value != NULL) *value = 0.0;
            return true;
        }

        char* end = NULL;
        double parsed = std::strtod(trimmed.c_str(), &end);
        if (end == trimmed.c_str())
        {
            return false;
        }

        if (hasValue != NULL) *hasValue = true;
        if (value != NULL) *value = parsed;
        return true;
    }

    void FillDefaultCoefficientValues(CoefficientRow* row)
    {
        if (row == NULL)
        {
            return;
        }

        if (!row->hasQ1) { row->hasQ1 = true; row->q1 = 0.50; }
        if (!row->hasQ2) { row->hasQ2 = true; row->q2 = 0.50; }
        if (!row->hasK1) { row->hasK1 = true; row->k1 = 0.35; }
        if (!row->hasK2) { row->hasK2 = true; row->k2 = 0.24; }
        if (!row->hasK3) { row->hasK3 = true; row->k3 = 0.50; }
        if (row->f1.empty()) row->f1 = "f1";
        if (row->f2.empty()) row->f2 = "f1";
    }

    std::wstring EnsureCsvExtension(const std::wstring& path)
    {
        if (path.empty())
        {
            return path;
        }

        const size_t slash = path.find_last_of(L"\\/");
        const size_t dot = path.find_last_of(L'.');
        if (dot != std::wstring::npos && (slash == std::wstring::npos || dot > slash))
        {
            return path;
        }
        return path + L".csv";
    }

    std::string PromptCoefficientCsvPath(bool save)
    {
        wchar_t fileName[MAX_PATH] = L"PiLianZuanBanJin_bend_factor_rules.csv";
        wchar_t initialDir[MAX_PATH] = L"D:\\UG智辉钣金插件\\config";
        OPENFILENAMEW ofn;
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = NULL;
        ofn.lpstrFilter = L"CSV 文件 (*.csv)\0*.csv\0所有文件 (*.*)\0*.*\0\0";
        ofn.lpstrFile = fileName;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrInitialDir = initialDir;
        ofn.lpstrDefExt = L"csv";
        ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
        ofn.lpstrTitle = save ? L"选择导出位置" : L"选择导入文件";

        BOOL ok = save ? GetSaveFileNameW(&ofn) : GetOpenFileNameW(&ofn);
        if (!ok)
        {
            return std::string();
        }

        std::wstring selected = save ? EnsureCsvExtension(fileName) : std::wstring(fileName);
        return WidePathToUtf8(selected);
    }

    std::string CsvEscape(const std::string& value)
    {
        const bool quote = value.find(',') != std::string::npos ||
            value.find('"') != std::string::npos ||
            value.find('\n') != std::string::npos ||
            value.find('\r') != std::string::npos;
        if (!quote)
        {
            return value;
        }

        std::string result = "\"";
        for (size_t i = 0; i < value.size(); ++i)
        {
            result += value[i] == '"' ? std::string("\"\"") : std::string(1, value[i]);
        }
        result += "\"";
        return result;
    }

    struct KFactorCalculatorState
    {
        HWND deductionEdit;
        HWND angleEdit;
        HWND radiusEdit;
        HWND thicknessEdit;
        HWND formulaEdit;
        HWND resultText;
        std::vector<HWND> childControls;
        HFONT dialogFont;
        HBRUSH backgroundBrush;
        COLORREF backgroundColor;
    };

    void AddCalculatorControl(KFactorCalculatorState* state, HWND control)
    {
        if (state == NULL || control == NULL)
        {
            return;
        }

        state->childControls.push_back(control);
        if (state->dialogFont != NULL)
        {
            SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(state->dialogFont), TRUE);
        }
    }

    std::string GetEditUtf8(HWND edit)
    {
        if (edit == NULL)
        {
            return std::string();
        }

        int length = GetWindowTextLengthW(edit);
        std::wstring text(static_cast<size_t>(length + 1), L'\0');
        if (length > 0)
        {
            GetWindowTextW(edit, &text[0], length + 1);
        }
        if (!text.empty() && text[text.size() - 1] == L'\0')
        {
            text.resize(text.size() - 1);
        }
        return WidePathToUtf8(text);
    }

    bool ParseEditDouble(HWND edit, double* value)
    {
        std::string text = TrimCopy(GetEditUtf8(edit));
        if (text.empty())
        {
            return false;
        }

        char* end = NULL;
        double parsed = std::strtod(text.c_str(), &end);
        if (end == text.c_str())
        {
            return false;
        }
        while (end != NULL && *end != '\0' && static_cast<unsigned char>(*end) <= ' ')
        {
            ++end;
        }
        if (end != NULL && *end != '\0')
        {
            return false;
        }
        if (value != NULL)
        {
            *value = parsed;
        }
        return true;
    }

    void SetWindowTextUtf8(HWND hwnd, const std::string& text)
    {
        std::wstring wide = PathTextToWide(text);
        SetWindowTextW(hwnd, wide.c_str());
    }

    void CalculateKFactorFromDialog(HWND hwnd, KFactorCalculatorState* state)
    {
        if (state == NULL)
        {
            return;
        }

        double deduction = 0.0;
        double angle = 0.0;
        double radius = 0.0;
        double thickness = 0.0;
        if (!ParseEditDouble(state->deductionEdit, &deduction) ||
            !ParseEditDouble(state->angleEdit, &angle) ||
            !ParseEditDouble(state->radiusEdit, &radius) ||
            !ParseEditDouble(state->thicknessEdit, &thickness))
        {
            MessageBoxW(hwnd, L"请输入有效的扣除、角度、内R、板厚。", L"计算K因子", MB_ICONWARNING | MB_OK);
            return;
        }

        double result = 0.0;
        if (!EvaluateKFormulaText(GetEditUtf8(state->formulaEdit), deduction, angle, radius, thickness, &result))
        {
            MessageBoxW(hwnd, L"公式计算失败，或结果不在 0 到 1 之间。", L"计算K因子", MB_ICONWARNING | MB_OK);
            return;
        }

        SetWindowTextUtf8(state->resultText, "K = " + FormatDouble(result, 6));
    }

    LRESULT CALLBACK KFactorCalculatorWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        const int kIdCalculate = 2101;
        const int kIdClose = IDCANCEL;

        KFactorCalculatorState* state = reinterpret_cast<KFactorCalculatorState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        switch (message)
        {
        case WM_CREATE:
        {
            CREATESTRUCTW* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
            state = reinterpret_cast<KFactorCalculatorState*>(createStruct->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
            if (state != NULL)
            {
                state->backgroundColor = RGB(236, 236, 236);
                state->backgroundBrush = CreateSolidBrush(state->backgroundColor);
                state->dialogFont = CreateNxLikeDialogFont();
                if (state->dialogFont != NULL)
                {
                    SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(state->dialogFont), TRUE);
                }
            }

            HINSTANCE instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(hwnd, GWLP_HINSTANCE));
            AddCalculatorControl(state, CreateWindowW(L"STATIC", L"扣除", WS_CHILD | WS_VISIBLE, 18, 20, 70, 22, hwnd, NULL, instance, NULL));
            AddCalculatorControl(state, CreateWindowW(L"STATIC", L"角度", WS_CHILD | WS_VISIBLE, 18, 54, 70, 22, hwnd, NULL, instance, NULL));
            AddCalculatorControl(state, CreateWindowW(L"STATIC", L"内R", WS_CHILD | WS_VISIBLE, 18, 88, 70, 22, hwnd, NULL, instance, NULL));
            AddCalculatorControl(state, CreateWindowW(L"STATIC", L"板厚", WS_CHILD | WS_VISIBLE, 18, 122, 70, 22, hwnd, NULL, instance, NULL));
            AddCalculatorControl(state, CreateWindowW(L"STATIC", L"公式", WS_CHILD | WS_VISIBLE, 18, 156, 70, 22, hwnd, NULL, instance, NULL));

            if (state != NULL)
            {
                state->deductionEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"0", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, 90, 18, 120, 24, hwnd, NULL, instance, NULL);
                state->angleEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"90", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, 90, 52, 120, 24, hwnd, NULL, instance, NULL);
                state->radiusEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"0", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, 90, 86, 120, 24, hwnd, NULL, instance, NULL);
                state->thicknessEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"1", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, 90, 120, 120, 24, hwnd, NULL, instance, NULL);
                state->formulaEdit = CreateWindowExW(
                    WS_EX_CLIENTEDGE,
                    L"EDIT",
                    L"((2*(R+T)*tan(A*pi/360)-Q)/(A*pi/180)-R)/T",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                    90,
                    154,
                    430,
                    24,
                    hwnd,
                    NULL,
                    instance,
                    NULL);
                state->resultText = CreateWindowW(L"STATIC", L"K = ", WS_CHILD | WS_VISIBLE, 90, 190, 260, 22, hwnd, NULL, instance, NULL);
                AddCalculatorControl(state, state->deductionEdit);
                AddCalculatorControl(state, state->angleEdit);
                AddCalculatorControl(state, state->radiusEdit);
                AddCalculatorControl(state, state->thicknessEdit);
                AddCalculatorControl(state, state->formulaEdit);
                AddCalculatorControl(state, state->resultText);
            }

            AddCalculatorControl(state, CreateWindowW(L"BUTTON", L"计算", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, 350, 222, 76, 28, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdCalculate)), instance, NULL));
            AddCalculatorControl(state, CreateWindowW(L"BUTTON", L"关闭", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 444, 222, 76, 28, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdClose)), instance, NULL));
            return 0;
        }
        case WM_CTLCOLORSTATIC:
            if (state != NULL && state->backgroundBrush != NULL)
            {
                SetBkColor(reinterpret_cast<HDC>(wParam), state->backgroundColor);
                SetTextColor(reinterpret_cast<HDC>(wParam), RGB(30, 30, 30));
                return reinterpret_cast<LRESULT>(state->backgroundBrush);
            }
            break;
        case WM_COMMAND:
            if (LOWORD(wParam) == kIdCalculate)
            {
                CalculateKFactorFromDialog(hwnd, state);
                return 0;
            }
            if (LOWORD(wParam) == kIdClose)
            {
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            if (state != NULL)
            {
                if (state->dialogFont != NULL)
                {
                    DeleteObject(state->dialogFont);
                    state->dialogFont = NULL;
                }
                if (state->backgroundBrush != NULL)
                {
                    DeleteObject(state->backgroundBrush);
                    state->backgroundBrush = NULL;
                }
            }
            return 0;
        default:
            break;
        }

        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    void ShowKFactorCalculatorDialog()
    {
        INITCOMMONCONTROLSEX controls = {};
        controls.dwSize = sizeof(controls);
        controls.dwICC = ICC_STANDARD_CLASSES;
        InitCommonControlsEx(&controls);

        const wchar_t* className = L"PiLianZuanBanJinKFactorCalculator";
        HINSTANCE instance = reinterpret_cast<HINSTANCE>(&__ImageBase);
        WNDCLASSEXW windowClass = {};
        windowClass.cbSize = sizeof(windowClass);
        windowClass.lpfnWndProc = KFactorCalculatorWndProc;
        windowClass.hInstance = instance;
        windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
        windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
        windowClass.lpszClassName = className;
        RegisterClassExW(&windowClass);

        KFactorCalculatorState state = {};
        HWND parent = reinterpret_cast<HWND>(UF_UI_get_default_parent());
        HWND hwnd = CreateWindowExW(
            WS_EX_DLGMODALFRAME,
            className,
            L"计算K因子",
            WS_CAPTION | WS_SYSMENU | WS_BORDER,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            560,
            310,
            parent,
            NULL,
            instance,
            &state);
        if (hwnd == NULL)
        {
            MessageBoxW(parent, L"无法打开K因子计算窗口。", L"计算K因子", MB_ICONERROR | MB_OK);
            return;
        }

        CenterWindowOnParent(hwnd, parent);
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);

        MSG msg;
        while (IsWindow(hwnd) && GetMessageW(&msg, NULL, 0, 0) > 0)
        {
            if (!IsDialogMessageW(hwnd, &msg))
            {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }
    }

    std::vector<std::string> SplitCsvLine(const std::string& line)
    {
        std::vector<std::string> cells;
        std::string cell;
        bool quoted = false;
        for (size_t i = 0; i < line.size(); ++i)
        {
            char c = line[i];
            if (quoted)
            {
                if (c == '"' && i + 1 < line.size() && line[i + 1] == '"')
                {
                    cell += '"';
                    ++i;
                }
                else if (c == '"')
                {
                    quoted = false;
                }
                else
                {
                    cell += c;
                }
            }
            else if (c == '"')
            {
                quoted = true;
            }
            else if (c == ',')
            {
                cells.push_back(cell);
                cell.clear();
            }
            else
            {
                cell += c;
            }
        }
        cells.push_back(cell);
        return cells;
    }

    std::string NormalizeCoefficientCsvHeader(const std::string& text)
    {
        std::string value = TrimCopy(NormalizeUtf8Message(text));
        if (value == u8"材料") return "Material";
        if (value == u8"厚度") return "Thickness";
        if (value == u8"扣除1") return "Q1";
        if (value == u8"扣除2") return "Q2";
        if (value == u8"扣除3") return "Q3";
        if (value == u8"K因子1") return "K1";
        if (value == u8"K因子2") return "K2";
        if (value == u8"K因子3") return "K3";
        if (value == "A1" || value == "A2" || value == "A3") return value;
        return value;
    }

    void AssignCoefficientNumber(CoefficientRow* row, const std::string& key, const std::string& text)
    {
        if (row == NULL)
        {
            return;
        }

        bool has = false;
        double value = 0.0;
        if (!ParseOptionalTextNumber(text, &has, &value))
        {
            return;
        }

        if (key == "Q1") { row->hasQ1 = has; row->q1 = value; }
        else if (key == "Q2") { row->hasQ2 = has; row->q2 = value; }
        else if (key == "Q3") { row->hasQ3 = has; row->q3 = value; }
        else if (key == "K1") { row->hasK1 = has; row->k1 = value; }
        else if (key == "K2") { row->hasK2 = has; row->k2 = value; }
        else if (key == "K3") { row->hasK3 = has; row->k3 = value; }
        else if (key == "A1") { row->hasA1 = has; row->a1 = value; }
        else if (key == "A2") { row->hasA2 = has; row->a2 = value; }
        else if (key == "A3") { row->hasA3 = has; row->a3 = value; }
    }

    bool SaveCoefficientRowsToCsv(const std::vector<CoefficientRow>& rows, const std::string& path, std::string* error)
    {
        std::ostringstream file;
        file << "\xEF\xBB\xBF";
        file << u8"材料,厚度,扣除1,扣除2,扣除3,K因子1,K因子2,K因子3,A1,A2,A3\n";
        for (size_t i = 0; i < rows.size(); ++i)
        {
            const CoefficientRow& row = rows[i];
            file << CsvEscape(row.material) << ",";
            file << CoefficientNumberText(row.thickness) << ",";
            file << OptionalCoefficientNumber(row.hasQ1, row.q1) << ",";
            file << OptionalCoefficientNumber(row.hasQ2, row.q2) << ",";
            file << OptionalCoefficientNumber(row.hasQ3, row.q3) << ",";
            file << OptionalCoefficientNumber(row.hasK1, row.k1) << ",";
            file << OptionalCoefficientNumber(row.hasK2, row.k2) << ",";
            file << OptionalCoefficientNumber(row.hasK3, row.k3) << ",";
            file << OptionalCoefficientNumber(row.hasA1, row.a1) << ",";
            file << OptionalCoefficientNumber(row.hasA2, row.a2) << ",";
            file << OptionalCoefficientNumber(row.hasA3, row.a3) << "\n";
        }

        std::string text = file.str();
        if (!WriteAllText(path, text))
        {
            if (error != NULL) *error = u8"无法导出EXCEL数据：" + path;
            return false;
        }
        return true;
    }

    bool LoadCoefficientRowsFromCsv(const std::string& path, std::vector<CoefficientRow>* rows, std::string* error)
    {
        if (rows == NULL)
        {
            return false;
        }
        std::string text = ReadAllText(path);
        if (text.empty())
        {
            if (error != NULL) *error = u8"无法导入EXCEL数据：" + path;
            return false;
        }

        std::stringstream stream(zhihui_bend_rules_ini::RemoveUtf8Bom(NormalizeUtf8Message(text)));
        std::string line;
        if (!std::getline(stream, line))
        {
            if (error != NULL) *error = u8"EXCEL数据为空：" + path;
            return false;
        }
        if (!line.empty() && line[line.size() - 1] == '\r') line.erase(line.size() - 1);

        std::vector<std::string> headers = SplitCsvLine(line);
        for (size_t i = 0; i < headers.size(); ++i)
        {
            headers[i] = NormalizeCoefficientCsvHeader(headers[i]);
        }

        std::vector<CoefficientRow> imported;
        size_t lineNumber = 1;
        while (std::getline(stream, line))
        {
            ++lineNumber;
            if (!line.empty() && line[line.size() - 1] == '\r') line.erase(line.size() - 1);
            if (TrimCopy(line).empty())
            {
                continue;
            }

            std::vector<std::string> cells = SplitCsvLine(line);
            CoefficientRow row;
            row.material = u8"材质 <未指定>";
            bool hasThickness = false;
            for (size_t i = 0; i < headers.size() && i < cells.size(); ++i)
            {
                std::string key = headers[i];
                std::string value = TrimCopy(NormalizeUtf8Message(cells[i]));
                if (key == "Material")
                {
                    row.material = value.empty() ? std::string(u8"材质 <未指定>") : value;
                }
                else if (key == "Thickness")
                {
                    bool has = false;
                    double thickness = 0.0;
                    if (ParseOptionalTextNumber(value, &has, &thickness) && has && thickness > 0.0)
                    {
                        row.thickness = thickness;
                        hasThickness = true;
                    }
                }
                else
                {
                    AssignCoefficientNumber(&row, key, value);
                }
            }

            if (!hasThickness)
            {
                if (error != NULL) *error = u8"导入失败：第 " + std::to_string(lineNumber) + u8" 行厚度为空或无效。";
                return false;
            }
            imported.push_back(row);
        }

        if (imported.empty())
        {
            if (error != NULL) *error = u8"导入失败：没有读取到有效厚度数据。";
            return false;
        }

        *rows = imported;
        return true;
    }

    class PiLianRulesTableDialog
    {
    public:
        enum Columns
        {
            ColumnMaterial = 0,
            ColumnThickness = 1,
            ColumnQ1 = 2,
            ColumnQ2 = 3,
            ColumnQ3 = 4,
            ColumnK1 = 5,
            ColumnK2 = 6,
            ColumnK3 = 7,
            ColumnA1 = 8,
            ColumnA2 = 9,
            ColumnA3 = 10
        };

        explicit PiLianRulesTableDialog(UI* ui, const std::string& dlxPath) :
            ui_(ui),
            dialog_(NULL),
            tree_(NULL),
            rulePageEnum_(NULL),
            largeArcEnum_(NULL),
            smallArcEnum_(NULL),
            materialPageEnum_(NULL),
            multiBendMinRadiusDouble_(NULL),
            addButton_(NULL),
            saveButton_(NULL),
            importExcelButton_(NULL),
            exportExcelButton_(NULL),
            calculateKButton_(NULL),
            selectedConditionIndex_(0),
            selectedNode_(NULL),
            columnsInserted_(false)
        {
            if (ui_ != NULL)
            {
                if (dlxPath.empty())
                {
                    throw std::runtime_error("Embedded DLX resource missing: PiLianZuanBanJinRulesTable.dlx");
                }
                dialog_ = ui_->CreateDialog(U8(dlxPath));
                dialog_->AddInitializeHandler(make_callback(this, &PiLianRulesTableDialog::initialize_cb));
                dialog_->AddDialogShownHandler(make_callback(this, &PiLianRulesTableDialog::dialogShown_cb));
                dialog_->AddUpdateHandler(make_callback(this, &PiLianRulesTableDialog::update_cb));
                dialog_->AddApplyHandler(make_callback(this, &PiLianRulesTableDialog::apply_cb));
                dialog_->AddOkHandler(make_callback(this, &PiLianRulesTableDialog::ok_cb));
            }
        }

        ~PiLianRulesTableDialog()
        {
            if (dialog_ != NULL)
            {
                delete dialog_;
                dialog_ = NULL;
            }
        }

        void Show()
        {
            if (dialog_ != NULL)
            {
                try
                {
                    dialog_->Launch();
                }
                catch (const std::exception& ex)
                {
                    ShowMessage(ui_, "折弯系数表", NXMessageBox::DialogTypeError, ex.what());
                }
                catch (...)
                {
                    ShowMessage(ui_, "折弯系数表", NXMessageBox::DialogTypeError, u8"打开规则表时发生未知异常。");
                }
            }
        }

        void initialize_cb()
        {
            tree_ = dynamic_cast<Tree*>(dialog_->TopBlock()->FindBlock("tree_control0"));
            rulePageEnum_ = dynamic_cast<Enumeration*>(dialog_->TopBlock()->FindBlock("rulePageEnum"));
            largeArcEnum_ = dynamic_cast<Enumeration*>(dialog_->TopBlock()->FindBlock("largeArcEnum"));
            smallArcEnum_ = dynamic_cast<Enumeration*>(dialog_->TopBlock()->FindBlock("smallArcEnum"));
            materialPageEnum_ = dynamic_cast<Enumeration*>(dialog_->TopBlock()->FindBlock("materialPageEnum"));
            multiBendMinRadiusDouble_ = dynamic_cast<DoubleBlock*>(dialog_->TopBlock()->FindBlock("multiBendMinDiameterDouble"));
            addButton_ = dynamic_cast<UIBlock*>(dialog_->TopBlock()->FindBlock("addCrossSelectionNodeButton"));
            saveButton_ = dynamic_cast<UIBlock*>(dialog_->TopBlock()->FindBlock("addNodeButton"));
            importExcelButton_ = dynamic_cast<UIBlock*>(dialog_->TopBlock()->FindBlock("importExcelButton"));
            exportExcelButton_ = dynamic_cast<UIBlock*>(dialog_->TopBlock()->FindBlock("exportExcelButton"));
            calculateKButton_ = dynamic_cast<UIBlock*>(dialog_->TopBlock()->FindBlock("deleteNodeButton"));
            LocalizeAndHideUnusedBlocks();
            settings_ = LoadRuleConfigForEditor();
            rows_ = LoadCoefficientRowsFromIni();
            conditionRows_ = LoadConditionRowsFromIni();
            RefreshRulePages();
            RefreshMaterialPages();
            SyncMultiBendRadiusControl();
            if (tree_ != NULL)
            {
                tree_->SetOnSelectHandler(make_callback(this, &PiLianRulesTableDialog::OnSelectCallback));
                tree_->SetOnBeginLabelEditHandler(make_callback(this, &PiLianRulesTableDialog::OnBeginLabelEditCallback));
                tree_->SetOnEndLabelEditHandler(make_callback(this, &PiLianRulesTableDialog::OnEndLabelEditCallback));
            }
        }

        void dialogShown_cb()
        {
            if (tree_ != NULL && !columnsInserted_)
            {
                tree_->InsertColumn(ColumnMaterial, U8("材料"), 110);
                tree_->InsertColumn(ColumnThickness, U8("厚度"), 70);
                tree_->InsertColumn(ColumnQ1, U8("扣除1"), 70);
                tree_->InsertColumn(ColumnQ2, U8("扣除2"), 70);
                tree_->InsertColumn(ColumnQ3, U8("扣除3"), 70);
                tree_->InsertColumn(ColumnK1, U8("K因子1"), 70);
                tree_->InsertColumn(ColumnK2, U8("K因子2"), 70);
                tree_->InsertColumn(ColumnK3, U8("K因子3"), 70);
                tree_->InsertColumn(ColumnA1, U8("A1"), 60);
                tree_->InsertColumn(ColumnA2, U8("A2"), 60);
                tree_->InsertColumn(ColumnA3, U8("A3"), 60);
                for (int i = ColumnMaterial; i <= ColumnA3; ++i)
                {
                    tree_->SetColumnResizePolicy(i, Tree::ColumnResizePolicyConstantWidth);
                }
                columnsInserted_ = true;
            }
            RebuildTree();
        }

        int update_cb(UIBlock* block)
        {
            if (block == addButton_)
            {
                AddRule();
            }
            else if (block == saveButton_)
            {
                Save();
            }
            else if (block == importExcelButton_)
            {
                ImportExcelData();
            }
            else if (block == exportExcelButton_)
            {
                ExportExcelData();
            }
            else if (block == calculateKButton_)
            {
                ShowKFactorCalculatorDialog();
            }
            else if (block == materialPageEnum_)
            {
                selectedMaterialPage_ = UiTextToUtf8(materialPageEnum_->ValueAsString());
                RebuildTree();
            }
            else if (block == rulePageEnum_)
            {
                selectedRulePage_ = UiTextToUtf8(rulePageEnum_->ValueAsString());
                SelectConditionByRulePage();
                RefreshArcEnums();
            }
            else if (block == smallArcEnum_)
            {
                if (selectedConditionIndex_ < conditionRows_.size())
                {
                    conditionRows_[selectedConditionIndex_].smallArcCode = CoefficientCodeFromDisplay(UiTextToUtf8(smallArcEnum_->ValueAsString()));
                }
            }
            else if (block == largeArcEnum_)
            {
                if (selectedConditionIndex_ < conditionRows_.size())
                {
                    conditionRows_[selectedConditionIndex_].largeArcCode = CoefficientCodeFromDisplay(UiTextToUtf8(largeArcEnum_->ValueAsString()));
                }
            }
            return 0;
        }

        int apply_cb()
        {
            return Save() ? 0 : 1;
        }

        int ok_cb()
        {
            return Save() ? 0 : 1;
        }

        void OnSelectCallback(Tree*, Node* node, int, bool selected)
        {
            selectedNode_ = selected ? node : NULL;
        }

        Tree::BeginLabelEditState OnBeginLabelEditCallback(Tree*, Node* node, int columnID)
        {
            return rowNodeToIndex_.find(node) != rowNodeToIndex_.end() && columnID >= ColumnMaterial && columnID <= ColumnA3 ?
                Tree::BeginLabelEditStateAllow :
                Tree::BeginLabelEditStateDisallow;
        }

        Tree::EndLabelEditState OnEndLabelEditCallback(Tree*, Node* node, int columnID, NXString editedText)
        {
            size_t index = 0;
            if (!IndexForNode(node, &index) || index >= rows_.size())
            {
                return Tree::EndLabelEditStateRejectText;
            }
            if (!ApplyCell(rows_[index], columnID, UiTextToUtf8(editedText)))
            {
                return Tree::EndLabelEditStateRejectText;
            }
            FillNode(node, rows_[index]);
            RefreshMaterialPages();
            return Tree::EndLabelEditStateAcceptText;
        }

    private:
        UI* ui_;
        BlockDialog* dialog_;
        Tree* tree_;
        Enumeration* rulePageEnum_;
        Enumeration* largeArcEnum_;
        Enumeration* smallArcEnum_;
        Enumeration* materialPageEnum_;
        DoubleBlock* multiBendMinRadiusDouble_;
        UIBlock* addButton_;
        UIBlock* saveButton_;
        UIBlock* importExcelButton_;
        UIBlock* exportExcelButton_;
        UIBlock* calculateKButton_;
        RuleConfig settings_;
        std::vector<CoefficientRow> rows_;
        std::vector<ConditionRuleRow> conditionRows_;
        std::vector<std::string> rulePages_;
        std::string selectedRulePage_;
        size_t selectedConditionIndex_;
        std::vector<std::string> materialPages_;
        std::string selectedMaterialPage_;
        std::map<Node*, size_t> rowNodeToIndex_;
        std::vector<Node*> allNodes_;
        Node* selectedNode_;
        bool columnsInserted_;

        void LocalizeAndHideUnusedBlocks()
        {
            TrySetBlockString(dialog_->TopBlock(), "Label", "折弯系数表");
            TrySetBlockString(dialog_->TopBlock(), "LabelString", "折弯系数表");
            TrySetBlockString(dialog_->TopBlock(), "Title", "折弯系数表");
            UIBlock* selectRuleGroup = dynamic_cast<UIBlock*>(dialog_->TopBlock()->FindBlock("nodeDataGroup"));
            UIBlock* coefficientGroup = dynamic_cast<UIBlock*>(dialog_->TopBlock()->FindBlock("group0"));
            UIBlock* actionGroup = dynamic_cast<UIBlock*>(dialog_->TopBlock()->FindBlock("addDeleteNodeGroup"));
            TrySetBlockString(selectRuleGroup, "Label", "选择规则");
            TrySetBlockString(selectRuleGroup, "LabelString", "选择规则");
            TrySetBlockString(coefficientGroup, "Label", "折弯系数表");
            TrySetBlockString(coefficientGroup, "LabelString", "折弯系数表");
            TrySetBlockString(actionGroup, "Label", "保存");
            TrySetBlockString(actionGroup, "LabelString", "保存");
            TrySetBlockString(rulePageEnum_, "Label", "选择规则");
            TrySetBlockString(largeArcEnum_, "Label", "多刀折圆");
            TrySetBlockString(smallArcEnum_, "Label", "普通折弯");
            TrySetBlockString(multiBendMinRadiusDouble_, "Label", "多刀折圆最小半径");
            TrySetBlockString(materialPageEnum_, "Label", "材料分页");
            TrySetBlockString(addButton_, "Label", "新增厚度");
            TrySetBlockString(saveButton_, "Label", "保存");
            TrySetBlockString(importExcelButton_, "Label", "导入EXCEL数据");
            TrySetBlockString(exportExcelButton_, "Label", "导出EXCEL数据");
            TrySetBlockString(calculateKButton_, "Label", "计算K因子");

            const char* hideBlockIds[] = {
                "selection0", "conditionTree", "autoTapHoleToggle", "autoPemHoleToggle", "autoCounterboreHoleToggle",
                "autoRecognitionGroup", "stateIconGroup", "NodeEditGroup", "menuGroup", "dragDropGroup", "defaultActionGroup",
                "redrawGroup", "listingWindowGroup", "instructions", "nodeString", "stateIconOptions",
                "nodeToolTip", "nodeEditOptions", "showMenuToggle", "disallowDragToggle", "dropOptions",
                "defaultActionToggle", "redrawInstruction", "redrawToggle", "listingWindowToggle"
            };
            for (size_t i = 0; i < sizeof(hideBlockIds) / sizeof(hideBlockIds[0]); ++i)
            {
                TrySetBlockVisible(dynamic_cast<UIBlock*>(dialog_->TopBlock()->FindBlock(hideBlockIds[i])), false);
            }
            TrySetBlockVisible(rulePageEnum_, true);
            TrySetBlockVisible(largeArcEnum_, true);
            TrySetBlockVisible(smallArcEnum_, true);
            TrySetBlockVisible(multiBendMinRadiusDouble_, true);
            TrySetBlockVisible(materialPageEnum_, true);
            TrySetBlockVisible(saveButton_, true);
            TrySetBlockVisible(importExcelButton_, true);
            TrySetBlockVisible(exportExcelButton_, true);
            TrySetBlockVisible(calculateKButton_, true);
            TrySetBlockEnabled(calculateKButton_, true);
        }

        void SyncMultiBendRadiusControl()
        {
            if (multiBendMinRadiusDouble_ != NULL)
            {
                multiBendMinRadiusDouble_->SetValue(std::max(0.0, settings_.absoluteLargeArcRadius));
            }
        }

        void ReadMultiBendRadiusControl()
        {
            if (multiBendMinRadiusDouble_ == NULL)
            {
                return;
            }

            PropertyList* props = NULL;
            try
            {
                props = multiBendMinRadiusDouble_->GetProperties();
                double radius = props->GetDouble("Value");
                settings_.absoluteLargeArcRadius = std::max(0.0, radius);
                settings_.useAbsoluteLargeArc = settings_.absoluteLargeArcRadius > 0.0;
            }
            catch (...)
            {
            }
            if (props != NULL)
            {
                delete props;
            }
        }

        std::string RulePageText(const ConditionRuleRow& row) const
        {
            std::string label = TrimCopy(row.angleLabel);
            if (label == "90") return u8"90度角折弯";
            if (label == "(0,90)") return u8"小于90度折弯";
            if (label == "(90,180)") return u8"大于90度折弯";
            if (label == "(180,360)") return u8"压死边";
            return label.empty() ? std::string(u8"未命名规则") : label;
        }

        void SelectConditionByRulePage()
        {
            for (size_t i = 0; i < conditionRows_.size(); ++i)
            {
                if (RulePageText(conditionRows_[i]) == selectedRulePage_)
                {
                    selectedConditionIndex_ = i;
                    return;
                }
            }
            selectedConditionIndex_ = 0;
            if (!conditionRows_.empty())
            {
                selectedRulePage_ = RulePageText(conditionRows_[0]);
            }
        }

        void RefreshRulePages()
        {
            rulePages_.clear();
            for (size_t i = 0; i < conditionRows_.size(); ++i)
            {
                rulePages_.push_back(RulePageText(conditionRows_[i]));
            }
            if (selectedRulePage_.empty())
            {
                selectedConditionIndex_ = 0;
                selectedRulePage_ = rulePages_.empty() ? std::string() : rulePages_[0];
            }
            SelectConditionByRulePage();
            if (rulePageEnum_ != NULL)
            {
                std::vector<NXString> members;
                for (size_t i = 0; i < rulePages_.size(); ++i)
                {
                    members.push_back(U8(rulePages_[i]));
                }
                rulePageEnum_->SetEnumMembers(members);
                if (!selectedRulePage_.empty())
                {
                    rulePageEnum_->SetValueAsString(U8(selectedRulePage_));
                }
            }
            RefreshArcEnums();
        }

        bool IsSupportedCoefficientCode(const std::string& code) const
        {
            const std::string key = TrimCopy(code);
            return key == "Q1" || key == "Q2" || key == "Q3" ||
                key == "K1" || key == "K2" || key == "K3" ||
                key == "A1" || key == "A2" || key == "A3";
        }

        std::string CoefficientDisplayName(const std::string& code) const
        {
            if (code == "Q1") return u8"扣除1";
            if (code == "Q2") return u8"扣除2";
            if (code == "Q3") return u8"扣除3";
            if (code == "K1") return u8"K因子1";
            if (code == "K2") return u8"K因子2";
            if (code == "K3") return u8"K因子3";
            if (code == "A1") return "A1";
            if (code == "A2") return "A2";
            if (code == "A3") return "A3";
            return code;
        }

        std::string CoefficientCodeFromDisplay(const std::string& display) const
        {
            if (display == u8"扣除1") return "Q1";
            if (display == u8"扣除2") return "Q2";
            if (display == u8"扣除3") return "Q3";
            if (display == u8"K因子1") return "K1";
            if (display == u8"K因子2") return "K2";
            if (display == u8"K因子3") return "K3";
            if (display == "A1" || display == "A2" || display == "A3") return display;
            return display;
        }

        void RefreshArcEnums()
        {
            const char* codes[] = { "Q1", "Q2", "Q3", "K1", "K2", "K3", "A1", "A2", "A3" };
            std::vector<NXString> members;
            for (size_t i = 0; i < sizeof(codes) / sizeof(codes[0]); ++i)
            {
                members.push_back(U8(CoefficientDisplayName(codes[i])));
            }

            std::string small = "Q1";
            std::string large = "K3";
            if (selectedConditionIndex_ < conditionRows_.size())
            {
                small = IsSupportedCoefficientCode(conditionRows_[selectedConditionIndex_].smallArcCode) ?
                    TrimCopy(conditionRows_[selectedConditionIndex_].smallArcCode) : small;
                large = IsSupportedCoefficientCode(conditionRows_[selectedConditionIndex_].largeArcCode) ?
                    TrimCopy(conditionRows_[selectedConditionIndex_].largeArcCode) : large;
            }

            if (smallArcEnum_ != NULL)
            {
                smallArcEnum_->SetEnumMembers(members);
                smallArcEnum_->SetValueAsString(U8(CoefficientDisplayName(small)));
            }
            if (largeArcEnum_ != NULL)
            {
                largeArcEnum_->SetEnumMembers(members);
                largeArcEnum_->SetValueAsString(U8(CoefficientDisplayName(large)));
            }
        }

        std::string NormalizeMaterialPage(const std::string& material) const
        {
            return TrimCopy(material).empty() ? std::string(u8"材质 <未指定>") : TrimCopy(material);
        }

        void SortRows()
        {
            std::stable_sort(rows_.begin(), rows_.end(),
                [this](const CoefficientRow& left, const CoefficientRow& right)
            {
                if (std::fabs(left.thickness - right.thickness) > 1.0e-9)
                {
                    return left.thickness < right.thickness;
                }
                return NormalizeMaterialPage(left.material) < NormalizeMaterialPage(right.material);
            });
        }

        void RefreshMaterialPages()
        {
            materialPages_.clear();
            materialPages_.push_back(u8"全部");
            std::set<std::string> seen;
            for (size_t i = 0; i < rows_.size(); ++i)
            {
                std::string material = NormalizeMaterialPage(rows_[i].material);
                if (seen.insert(material).second)
                {
                    materialPages_.push_back(material);
                }
            }
            if (selectedMaterialPage_.empty())
            {
                selectedMaterialPage_ = u8"全部";
            }

            bool exists = selectedMaterialPage_ == u8"全部";
            for (size_t i = 1; i < materialPages_.size(); ++i)
            {
                if (materialPages_[i] == selectedMaterialPage_)
                {
                    exists = true;
                    break;
                }
            }
            if (!exists)
            {
                selectedMaterialPage_ = u8"全部";
            }

            if (materialPageEnum_ != NULL)
            {
                std::vector<NXString> members;
                for (size_t i = 0; i < materialPages_.size(); ++i)
                {
                    members.push_back(U8(materialPages_[i]));
                }
                materialPageEnum_->SetEnumMembers(members);
                materialPageEnum_->SetValueAsString(U8(selectedMaterialPage_));
            }
        }

        bool RowVisible(const CoefficientRow& row) const
        {
            return selectedMaterialPage_.empty() ||
                selectedMaterialPage_ == u8"全部" ||
                NormalizeMaterialPage(row.material) == selectedMaterialPage_;
        }

        void RebuildTree()
        {
            if (tree_ == NULL)
            {
                return;
            }
            SortRows();
            RefreshRulePages();
            RefreshMaterialPages();
            for (size_t i = allNodes_.size(); i > 0; --i)
            {
                try { tree_->DeleteNode(allNodes_[i - 1]); } catch (...) {}
            }
            rowNodeToIndex_.clear();
            allNodes_.clear();
            selectedNode_ = NULL;

            for (size_t i = 0; i < rows_.size(); ++i)
            {
                if (!RowVisible(rows_[i]))
                {
                    continue;
                }
                std::string nodeText = NormalizeMaterialPage(rows_[i].material);
                Node* node = tree_->CreateNode(U8(nodeText));
                tree_->InsertNode(node, NULL, NULL, Tree::NodeInsertOptionAlwaysLast);
                allNodes_.push_back(node);
                FillNode(node, rows_[i]);
                rowNodeToIndex_[node] = i;
            }
        }

        void FillNode(Node* node, const CoefficientRow& row)
        {
            SetNodeColumnText(node, ColumnMaterial, NormalizeMaterialPage(row.material));
            SetNodeColumnText(node, ColumnThickness, CoefficientNumberText(row.thickness));
            SetNodeColumnText(node, ColumnQ1, OptionalCoefficientNumber(row.hasQ1, row.q1));
            SetNodeColumnText(node, ColumnQ2, OptionalCoefficientNumber(row.hasQ2, row.q2));
            SetNodeColumnText(node, ColumnQ3, OptionalCoefficientNumber(row.hasQ3, row.q3));
            SetNodeColumnText(node, ColumnK1, OptionalCoefficientNumber(row.hasK1, row.k1));
            SetNodeColumnText(node, ColumnK2, OptionalCoefficientNumber(row.hasK2, row.k2));
            SetNodeColumnText(node, ColumnK3, OptionalCoefficientNumber(row.hasK3, row.k3));
            SetNodeColumnText(node, ColumnA1, OptionalCoefficientNumber(row.hasA1, row.a1));
            SetNodeColumnText(node, ColumnA2, OptionalCoefficientNumber(row.hasA2, row.a2));
            SetNodeColumnText(node, ColumnA3, OptionalCoefficientNumber(row.hasA3, row.a3));
        }

        bool IndexForNode(Node* node, size_t* index) const
        {
            std::map<Node*, size_t>::const_iterator it = rowNodeToIndex_.find(node);
            if (it == rowNodeToIndex_.end())
            {
                return false;
            }
            if (index != NULL)
            {
                *index = it->second;
            }
            return true;
        }

        bool ApplyCell(CoefficientRow& row, int columnID, const std::string& text)
        {
            std::string value = TrimCopy(text);
            if (columnID == ColumnMaterial)
            {
                row.material = value.empty() ? std::string(u8"材质 <未指定>") : value;
                selectedMaterialPage_ = NormalizeMaterialPage(row.material);
                return true;
            }
            if (columnID == ColumnThickness)
            {
                bool has = false;
                double parsed = 0.0;
                if (!ParseOptionalTextNumber(value, &has, &parsed) || !has || parsed <= 0.0)
                {
                    return false;
                }
                row.thickness = parsed;
                return true;
            }

            bool* has = NULL;
            double* target = NULL;
            switch (columnID)
            {
            case ColumnQ1: has = &row.hasQ1; target = &row.q1; break;
            case ColumnQ2: has = &row.hasQ2; target = &row.q2; break;
            case ColumnQ3: has = &row.hasQ3; target = &row.q3; break;
            case ColumnK1: has = &row.hasK1; target = &row.k1; break;
            case ColumnK2: has = &row.hasK2; target = &row.k2; break;
            case ColumnK3: has = &row.hasK3; target = &row.k3; break;
            case ColumnA1: has = &row.hasA1; target = &row.a1; break;
            case ColumnA2: has = &row.hasA2; target = &row.a2; break;
            case ColumnA3: has = &row.hasA3; target = &row.a3; break;
            default: return false;
            }
            return ParseOptionalTextNumber(value, has, target);
        }

        void AddRule()
        {
            CoefficientRow row;
            size_t index = 0;
            if (IndexForNode(selectedNode_, &index) && index < rows_.size())
            {
                row = rows_[index];
                row.thickness += 0.5;
            }
            else
            {
                row.material = (!selectedMaterialPage_.empty() && selectedMaterialPage_ != u8"全部") ?
                    selectedMaterialPage_ : std::string(u8"材质 <未指定>");
                row.thickness = 1.0;
                FillDefaultCoefficientValues(&row);
            }
            row.thickness = NextAvailableThickness(row.material, row.thickness);
            FillDefaultCoefficientValues(&row);
            rows_.push_back(row);
            selectedMaterialPage_ = NormalizeMaterialPage(row.material);
            RebuildTree();
        }

        bool HasMaterialThickness(const std::string& material, double thickness) const
        {
            std::string normalized = NormalizeMaterialPage(material);
            for (size_t i = 0; i < rows_.size(); ++i)
            {
                if (NormalizeMaterialPage(rows_[i].material) == normalized &&
                    std::fabs(rows_[i].thickness - thickness) < 1.0e-6)
                {
                    return true;
                }
            }
            return false;
        }

        double NextAvailableThickness(const std::string& material, double startThickness) const
        {
            double thickness = startThickness > 0.0 ? startThickness : 1.0;
            int guard = 0;
            while (HasMaterialThickness(material, thickness) && guard < 200)
            {
                thickness += 0.5;
                ++guard;
            }
            return thickness;
        }

        bool Save()
        {
            ReadMultiBendRadiusControl();
            std::string path = RulesIniSavePath();
            std::string error;

            char tempDir[MAX_PATH] = { 0 };
            DWORD tempDirLength = GetTempPathA(MAX_PATH, tempDir);
            std::string tempPath = tempDirLength > 0 && tempDirLength < MAX_PATH ?
                CombinePath(tempDir, "zhihui_pilian_rules_table.tmp") :
                std::string("C:\\zhihui_pilian_rules_table.tmp");

            if (!zhihui_bend_rules_ini::SaveCoefficientRowsToIniFile<RuleConfig>(settings_, rows_, conditionRows_, tempPath, &error))
            {
                ShowMessage(ui_, "折弯系数表", NXMessageBox::DialogTypeError, error);
                return false;
            }

            std::string text = ReadAllText(tempPath);
            DeleteFileA(tempPath.c_str());
            if (text.empty() || !WriteAllText(path, text))
            {
                ShowMessage(ui_, "折弯系数表", NXMessageBox::DialogTypeError, u8"无法写入规则表：" + path);
                return false;
            }

            ShowMessage(ui_, "折弯系数表", NXMessageBox::DialogTypeInformation, u8"系数表已保存：" + path);
            return true;
        }

        void ImportExcelData()
        {
            const std::string path = PromptCoefficientCsvPath(false);
            if (path.empty())
            {
                return;
            }

            std::vector<CoefficientRow> imported;
            std::string error;
            if (!LoadCoefficientRowsFromCsv(path, &imported, &error))
            {
                ShowMessage(ui_, "折弯系数表", NXMessageBox::DialogTypeError, error);
                return;
            }

            rows_ = imported;
            RefreshMaterialPages();
            RebuildTree();
            ShowMessage(ui_, "折弯系数表", NXMessageBox::DialogTypeInformation,
                u8"已导入EXCEL数据：\n" + path + u8"\n请点击保存写入规则配置。");
        }

        void ExportExcelData()
        {
            const std::string path = PromptCoefficientCsvPath(true);
            if (path.empty())
            {
                return;
            }

            std::string error;
            if (!SaveCoefficientRowsToCsv(rows_, path, &error))
            {
                ShowMessage(ui_, "折弯系数表", NXMessageBox::DialogTypeError, error);
                return;
            }

            ShowMessage(ui_, "折弯系数表", NXMessageBox::DialogTypeInformation,
                u8"已导出EXCEL数据：\n" + path);
        }
    };

    FaceInfo FindLargestPlanarFace(Body* body)
    {
        FaceInfo best;
        if (body == NULL)
        {
            return best;
        }

        std::vector<Face*> faces = body->GetFaces();
        for (size_t i = 0; i < faces.size(); ++i)
        {
            int type = 0;
            if (!AskFaceType(faces[i], &type) || type != UF_MODL_PLANAR_FACE)
            {
                continue;
            }

            double score = FaceAreaScore(faces[i]);
            if (score > best.areaScore)
            {
                best.face = faces[i];
                best.type = type;
                best.areaScore = score;
            }
        }

        if (best.face != NULL)
        {
            return best;
        }

        for (size_t i = 0; i < faces.size(); ++i)
        {
            int type = 0;
            if (!AskFaceType(faces[i], &type) || type != UF_MODL_CYLINDRICAL_FACE)
            {
                continue;
            }

            double score = FaceAreaScore(faces[i]);
            if (score > best.areaScore)
            {
                best.face = faces[i];
                best.type = type;
                best.areaScore = score;
            }
        }

        return best;
    }

    double Distance(const Point3d& a, const Point3d& b)
    {
        const double dx = a.X - b.X;
        const double dy = a.Y - b.Y;
        const double dz = a.Z - b.Z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    Edge* FindLongestStraightEdge(Face* face, Point3d* startPoint, Point3d* endPoint)
    {
        if (face == NULL)
        {
            return NULL;
        }

        Edge* bestEdge = NULL;
        double bestLength = -1.0;
        std::vector<Edge*> edges = face->GetEdges();
        for (size_t i = 0; i < edges.size(); ++i)
        {
            if (edges[i] == NULL)
            {
                continue;
            }

            Point3d p1;
            Point3d p2;
            try
            {
                if (edges[i]->SolidEdgeType() != NXOpen::Edge::EdgeTypeLinear)
                {
                    continue;
                }

                edges[i]->GetVertices(&p1, &p2);
            }
            catch (...)
            {
                continue;
            }

            double length = Distance(p1, p2);
            if (length > bestLength)
            {
                bestLength = length;
                bestEdge = edges[i];
                if (startPoint != NULL)
                {
                    *startPoint = p1;
                }
                if (endPoint != NULL)
                {
                    *endPoint = p2;
                }
            }
        }

        return bestEdge;
    }

    std::vector<CylinderAxisRecord> CollectUniqueBendAxes(SheetmetalManager* manager, Body* body)
    {
        std::vector<CylinderAxisRecord> axes;
        std::map<tag_t, bool> innerBendTags;
        std::map<tag_t, bool> outerBendTags;
        if (!BuildBendFaceTagSets(manager, body, &innerBendTags, &outerBendTags))
        {
            return axes;
        }

        std::map<tag_t, bool> bendTags = innerBendTags;
        for (std::map<tag_t, bool>::const_iterator it = outerBendTags.begin(); it != outerBendTags.end(); ++it)
        {
            if (it->second)
            {
                bendTags[it->first] = true;
            }
        }

        for (std::map<tag_t, bool>::const_iterator it = bendTags.begin(); it != bendTags.end(); ++it)
        {
            Face* bendFace = dynamic_cast<Face*>(ObjectFromTag(it->first));
            CylinderAxisRecord axis;
            if (!AskCylinderAxis(bendFace, &axis))
            {
                continue;
            }

            bool alreadyCounted = false;
            for (size_t i = 0; i < axes.size(); ++i)
            {
                if (IsCoaxialCylinderAxis(axes[i], axis))
                {
                    alreadyCounted = true;
                    break;
                }
            }
            if (!alreadyCounted)
            {
                axes.push_back(axis);
            }
        }

        return axes;
    }

    Edge* FindXAxisEdgeForSingleBend(
        SheetmetalManager* manager,
        Body* body,
        Face* face,
        Point3d* startPoint,
        Point3d* endPoint)
    {
        if (face == NULL)
        {
            return NULL;
        }

        std::vector<CylinderAxisRecord> bendAxes = CollectUniqueBendAxes(manager, body);
        if (bendAxes.size() != 1)
        {
            return NULL;
        }

        Edge* bestEdge = NULL;
        double bestLength = -1.0;
        const double parallelTolerance = 0.999;
        const double perpendicularTolerance = 0.001;
        std::vector<Edge*> edges = face->GetEdges();
        for (size_t i = 0; i < edges.size(); ++i)
        {
            if (edges[i] == NULL)
            {
                continue;
            }

            Point3d p1;
            Point3d p2;
            try
            {
                if (edges[i]->SolidEdgeType() != NXOpen::Edge::EdgeTypeLinear)
                {
                    continue;
                }

                edges[i]->GetVertices(&p1, &p2);
            }
            catch (...)
            {
                continue;
            }

            double edgeDirection[3] = { p2.X - p1.X, p2.Y - p1.Y, p2.Z - p1.Z };
            if (!Normalize3(edgeDirection))
            {
                continue;
            }

            const double dot = std::fabs(Dot3(edgeDirection, bendAxes[0].direction));
            const bool parallelToBend = dot >= parallelTolerance;
            const bool perpendicularToBend = dot <= perpendicularTolerance;
            if (!parallelToBend && !perpendicularToBend)
            {
                continue;
            }

            double length = Distance(p1, p2);
            if (length > bestLength)
            {
                bestLength = length;
                bestEdge = edges[i];
                if (startPoint != NULL)
                {
                    *startPoint = p1;
                }
                if (endPoint != NULL)
                {
                    *endPoint = p2;
                }
            }
        }

        return bestEdge;
    }

    std::string FeatureName(Feature* feature)
    {
        if (feature == NULL)
        {
            return std::string();
        }

        try
        {
            return ToUtf8(feature->GetFeatureName());
        }
        catch (...)
        {
            return std::string();
        }
    }

    std::string FeatureTypeText(Feature* feature)
    {
        if (feature == NULL)
        {
            return std::string();
        }

        try
        {
            return ToUtf8(feature->FeatureType());
        }
        catch (...)
        {
            return std::string();
        }
    }

    bool IsConvertSheetmetalFeatureName(const std::string& name)
    {
        return ContainsTextNoCase(name, "SB Convert To Sheet Metal") ||
            ContainsTextNoCase(name, "Convert To Sheet Metal") ||
            ContainsTextNoCase(name, "Convert To Sheetmetal");
    }

    bool IsConvertSheetmetalFeature(Feature* feature)
    {
        if (feature == NULL)
        {
            return false;
        }

        return IsConvertSheetmetalFeatureName(FeatureName(feature)) ||
            IsConvertSheetmetalFeatureName(FeatureTypeText(feature));
    }

    bool FeatureOwnsBody(Feature* feature, Body* body)
    {
        if (feature == NULL || body == NULL)
        {
            return false;
        }

        try
        {
            std::vector<Body*> bodies = feature->GetBodies();
            for (size_t i = 0; i < bodies.size(); ++i)
            {
                if (bodies[i] != NULL && bodies[i]->Tag() == body->Tag())
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

    bool IsSheetmetalBody(Part* workPart, SheetmetalManager* manager, Body* body)
    {
        if (body == NULL)
        {
            return false;
        }

        try
        {
            std::vector<Feature*> features = body->GetFeatures();
            for (size_t i = 0; i < features.size(); ++i)
            {
                Feature* feature = features[i];
                if (IsConvertSheetmetalFeature(feature))
                {
                    return true;
                }
            }
        }
        catch (...)
        {
        }

        if (workPart != NULL && workPart->Features() != NULL)
        {
            try
            {
                for (FeatureCollection::iterator it = workPart->Features()->begin(); it != workPart->Features()->end(); ++it)
                {
                    Feature* feature = *it;
                    if (!IsConvertSheetmetalFeature(feature))
                    {
                        continue;
                    }

                    if (FeatureOwnsBody(feature, body))
                    {
                        return true;
                    }
                }
            }
            catch (...)
            {
            }
        }

        if (manager != NULL)
        {
            try
            {
                std::vector<Face*> innerBendFaces;
                std::vector<SheetmetalBendState> states;
                manager->GetInnerBendFaces(body, innerBendFaces, states);
                if (!innerBendFaces.empty())
                {
                    return true;
                }
            }
            catch (...)
            {
            }
        }

        return false;
    }

    bool IsNeutralFactorFeature(Feature* feature)
    {
        if (feature == NULL)
        {
            return false;
        }

        try
        {
            std::string type = ToUtf8(feature->FeatureType());
            return ContainsTextNoCase(type, "Neutral") ||
                ContainsTextNoCase(type, "Factor") ||
                ContainsTextNoCase(type, "Resize Neutral");
        }
        catch (...)
        {
            return false;
        }
    }

    std::map<tag_t, Feature*> BuildNeutralFactorFeatureByFace(Body* body)
    {
        std::map<tag_t, Feature*> featureByFace;
        if (body == NULL)
        {
            return featureByFace;
        }

        try
        {
            std::vector<Feature*> features = body->GetFeatures();
            for (size_t i = 0; i < features.size(); ++i)
            {
                Feature* feature = features[i];
                if (!IsNeutralFactorFeature(feature))
                {
                    continue;
                }

                std::vector<Face*> faces = feature->GetFaces();
                for (size_t j = 0; j < faces.size(); ++j)
                {
                    if (faces[j] != NULL && IsAlive(faces[j]->Tag()))
                    {
                        featureByFace[faces[j]->Tag()] = feature;
                    }
                }
            }
        }
        catch (...)
        {
        }

        return featureByFace;
    }

    int CommitNeutralFactorFeature(Part* workPart, Feature* existingFeature, const std::vector<Face*>& faces, double neutralFactor)
    {
        if (workPart == NULL || faces.empty())
        {
            return 0;
        }

        std::vector<Face*> validFaces;
        std::map<tag_t, bool> seen;
        for (size_t i = 0; i < faces.size(); ++i)
        {
            Face* face = faces[i];
            if (face == NULL || !IsAlive(face->Tag()) || seen[face->Tag()])
            {
                continue;
            }

            validFaces.push_back(face);
            seen[face->Tag()] = true;
        }

        if (validFaces.empty())
        {
            return 0;
        }

        ResizeNeutralFactorBuilder* resizeBuilder = NULL;
        try
        {
            resizeBuilder = workPart->Features()->SheetmetalManager()->CreateResizeNeutralFactorBuilder(existingFeature);
            resizeBuilder->SetUseGlobal(false);
            resizeBuilder->NeutralFactor()->SetFormula(FormatDouble(neutralFactor, 12));

            SelectionIntentRuleOptions* ruleOptions = workPart->ScRuleFactory()->CreateRuleOptions();
            ruleOptions->SetSelectedFromInactive(false);
            FaceDumbRule* faceRule = workPart->ScRuleFactory()->CreateRuleFaceDumb(validFaces, ruleOptions);
            delete ruleOptions;

            std::vector<SelectionIntentRule*> rules(1);
            rules[0] = faceRule;
            resizeBuilder->BendFaces()->ReplaceRules(rules, false);

            Feature* feature = resizeBuilder->CommitFeature();
            resizeBuilder->Destroy();
            resizeBuilder = NULL;
            Session::GetSession()->CleanUpFacetedFacesAndEdges();
            return feature == NULL ? 0 : static_cast<int>(validFaces.size());
        }
        catch (...)
        {
            if (resizeBuilder != NULL)
            {
                try
                {
                    resizeBuilder->Destroy();
                }
                catch (...)
                {
                }
            }

            return 0;
        }
    }

    int ApplyNeutralFactorByRules(Part* workPart, SheetmetalManager* manager, Body* body, const RuleConfig& ruleConfig, const std::string& materialOverride)
    {
        if (workPart == NULL || manager == NULL || body == NULL)
        {
            return 0;
        }

        std::vector<Face*> innerBendFaces;
        std::vector<SheetmetalBendState> states;
        try
        {
            manager->GetInnerBendFaces(body, innerBendFaces, states);
        }
        catch (...)
        {
            return 0;
        }

        double thickness = 0.0;
        try
        {
            thickness = manager->GetBodyThickness(body);
        }
        catch (...)
        {
            thickness = 0.0;
        }

        std::string bodyMaterial = TrimCopy(materialOverride);
        if (bodyMaterial.empty())
        {
            bodyMaterial = ReadBodyMaterialText(workPart, body);
        }

        std::map<tag_t, Feature*> neutralFeatureByFace = BuildNeutralFactorFeatureByFace(body);
        std::map<tag_t, Feature*> neutralFeaturesByTag;
        std::map<std::string, std::vector<Face*> > facesByRule;
        int prepared = 0;
        for (size_t i = 0; i < innerBendFaces.size(); ++i)
        {
            if (i < states.size() && states[i] != SheetmetalBendStateBent)
            {
                continue;
            }

            Face* face = innerBendFaces[i];
            if (face == NULL || !IsAlive(face->Tag()))
            {
                continue;
            }

            BendFaceRecord record;
            record.face = face;
            record.material = bodyMaterial;
            record.thickness = thickness;
            try
            {
                SheetmetalBendParameters parameters = manager->GetBendParameters(face);
                if (parameters.BendState != SheetmetalBendStateBent)
                {
                    continue;
                }

                record.innerRadius = parameters.InnerRadius;
                record.angleDeg = parameters.BendAngle;
                record.currentK = parameters.NeutralFactor;
            }
            catch (...)
            {
                continue;
            }

            const BendRule* rule = MatchRule(ruleConfig, record);
            if (rule == NULL || !CalculateTargetK(*rule, &record))
            {
                record.targetK = 0.5;
            }

            if (std::fabs(record.currentK - record.targetK) <= 1e-6)
            {
                continue;
            }

            Feature* existingNeutralFeature = NULL;
            std::map<tag_t, Feature*>::iterator existingIt = neutralFeatureByFace.find(face->Tag());
            if (existingIt != neutralFeatureByFace.end())
            {
                existingNeutralFeature = existingIt->second;
            }

            tag_t existingFeatureTag = existingNeutralFeature == NULL ? NULL_TAG : existingNeutralFeature->Tag();
            if (existingNeutralFeature != NULL)
            {
                neutralFeaturesByTag[existingFeatureTag] = existingNeutralFeature;
            }

            std::string key = std::to_string(static_cast<unsigned long long>(existingFeatureTag)) +
                "|" + FormatDouble(record.targetRadius > 0.0 ? record.targetRadius : record.innerRadius, 3) +
                "|" + FormatDouble(record.angleDeg, 3) +
                "|" + FormatDouble(record.targetK, 6);
            facesByRule[key].push_back(face);
            ++prepared;
        }

        int appliedFaces = 0;
        for (std::map<std::string, std::vector<Face*> >::iterator it = facesByRule.begin(); it != facesByRule.end(); ++it)
        {
            size_t lastPipe = it->first.find_last_of('|');
            double targetK = lastPipe == std::string::npos ? 0.5 : std::atof(it->first.substr(lastPipe + 1).c_str());
            tag_t existingFeatureTag = NULL_TAG;
            size_t firstPipe = it->first.find('|');
            if (firstPipe != std::string::npos)
            {
                existingFeatureTag = static_cast<tag_t>(std::strtoull(it->first.substr(0, firstPipe).c_str(), NULL, 10));
            }

            Feature* existingNeutralFeature = NULL;
            std::map<tag_t, Feature*>::iterator featureIt = neutralFeaturesByTag.find(existingFeatureTag);
            if (featureIt != neutralFeaturesByTag.end())
            {
                existingNeutralFeature = featureIt->second;
            }

            appliedFaces += CommitNeutralFactorFeature(workPart, existingNeutralFeature, it->second, targetK);
        }

        return appliedFaces == 0 ? prepared : appliedFaces;
    }

    int ApplyBendRadiusToInnerBends(Part* workPart, SheetmetalManager* manager, Body* body, double innerRadius)
    {
        if (workPart == NULL || manager == NULL || body == NULL || innerRadius <= 0.0)
        {
            return 0;
        }

        std::vector<Face*> innerBendFaces;
        std::vector<SheetmetalBendState> states;
        try
        {
            manager->GetInnerBendFaces(body, innerBendFaces, states);
        }
        catch (...)
        {
            return 0;
        }

        std::vector<Face*> validFaces;
        std::map<tag_t, bool> seen;
        for (size_t i = 0; i < innerBendFaces.size(); ++i)
        {
            if (i < states.size() && states[i] != SheetmetalBendStateBent)
            {
                continue;
            }
            Face* face = innerBendFaces[i];
            if (face == NULL || !IsAlive(face->Tag()) || seen[face->Tag()])
            {
                continue;
            }
            validFaces.push_back(face);
            seen[face->Tag()] = true;
        }

        if (validFaces.empty())
        {
            return 0;
        }

        ResizeBendRadiusBuilder* radiusBuilder = NULL;
        try
        {
            NXOpen::Features::Feature* nullFeature(NULL);
            radiusBuilder = manager->CreateResizeBendRadiusFeatureBuilder(nullFeature);
            SelectionIntentRuleOptions* ruleOptions = workPart->ScRuleFactory()->CreateRuleOptions();
            ruleOptions->SetSelectedFromInactive(false);
            FaceDumbRule* faceRule = workPart->ScRuleFactory()->CreateRuleFaceDumb(validFaces, ruleOptions);
            delete ruleOptions;

            std::vector<SelectionIntentRule*> rules(1);
            rules[0] = faceRule;
            radiusBuilder->BendFaces()->ReplaceRules(rules, false);
            radiusBuilder->BendRadius()->SetFormula(FormatDouble(innerRadius, 12));
            Feature* feature = radiusBuilder->CommitFeature();
            radiusBuilder->Destroy();
            radiusBuilder = NULL;
            Session::GetSession()->CleanUpFacetedFacesAndEdges();
            return feature == NULL ? 0 : static_cast<int>(validFaces.size());
        }
        catch (...)
        {
            if (radiusBuilder != NULL)
            {
                try
                {
                    radiusBuilder->Destroy();
                }
                catch (...)
                {
                }
            }
            return 0;
        }
    }

    void ApplySheetmetalPreferencesBeforeConvert(Part* workPart, const AutoConvertOptions& options)
    {
        if (workPart == NULL)
        {
            return;
        }

        SheetMetalPreferencesBuilder* prefsBuilder = NULL;
        try
        {
            SheetMetalPreferencesManager* prefsManager = workPart->Preferences()->SheetMetalPreferences();
            if (prefsManager == NULL)
            {
                return;
            }

            prefsBuilder = prefsManager->CreateSheetMetalPreferencesBuilder();
            prefsBuilder->SetParameterEntryType(SheetMetalPreferencesBuilder::ParameterEntryTypesValue);
            prefsBuilder->SetBendDefinitionMethod(SheetMetalPreferencesBuilder::BendDefinitionMethodOptionsNeutralFactorValue);

            if (options.innerRadius > 0.0)
            {
                prefsBuilder->BendRadius()->SetFormula(FormatDouble(options.innerRadius, 12));
            }
            if (options.reliefDepth > 0.0)
            {
                prefsBuilder->BendReliefDepth()->SetFormula(FormatDouble(options.reliefDepth, 12));
            }
            if (options.reliefWidth > 0.0)
            {
                prefsBuilder->BendReliefWidth()->SetFormula(FormatDouble(options.reliefWidth, 12));
            }

            prefsBuilder->Commit();
            prefsBuilder->Destroy();
            prefsBuilder = NULL;
        }
        catch (...)
        {
            if (prefsBuilder != NULL)
            {
                try
                {
                    prefsBuilder->Destroy();
                }
                catch (...)
                {
                }
            }
        }
    }

    Body* ResolveFeatureBody(Feature* feature, Body* fallback)
    {
        if (feature == NULL)
        {
            return fallback;
        }

        try
        {
            std::vector<Body*> bodies = feature->GetBodies();
            for (size_t i = 0; i < bodies.size(); ++i)
            {
                if (bodies[i] != NULL && IsAlive(bodies[i]->Tag()))
                {
                    return bodies[i];
                }
            }
        }
        catch (...)
        {
        }

        return fallback;
    }

    bool IsFlatPatternFeature(Feature* feature)
    {
        if (feature == NULL)
        {
            return false;
        }

        try
        {
            std::string type = ToUtf8(feature->FeatureType());
            if (ContainsTextNoCase(type, "Flat Pattern") ||
                ContainsTextNoCase(type, "FlatPattern") ||
                ContainsTextNoCase(type, "SB Flat"))
            {
                return true;
            }
        }
        catch (...)
        {
        }

        try
        {
            std::string name = ToUtf8(feature->GetFeatureName());
            if (ContainsTextNoCase(name, "Flat Pattern") ||
                ContainsTextNoCase(name, "FlatPattern") ||
                ContainsTextNoCase(name, "SB Flat"))
            {
                return true;
            }
        }
        catch (...)
        {
            return false;
        }

        return false;
    }

    bool BodyContainsFace(Body* body, Face* targetFace)
    {
        if (body == NULL || targetFace == NULL)
        {
            return false;
        }

        try
        {
            Body* faceBody = targetFace->GetBody();
            if (faceBody != NULL && faceBody->Tag() == body->Tag())
            {
                return true;
            }
        }
        catch (...)
        {
        }

        try
        {
            std::vector<Face*> faces = body->GetFaces();
            for (size_t i = 0; i < faces.size(); ++i)
            {
                if (faces[i] != NULL && faces[i]->Tag() == targetFace->Tag())
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

    Feature* FindExistingFlatPatternFeature(Part* workPart, SheetmetalManager* manager, Body* body)
    {
        if (workPart == NULL || manager == NULL || body == NULL)
        {
            return NULL;
        }

        try
        {
            std::vector<Feature*> features = body->GetFeatures();
            for (size_t i = 0; i < features.size(); ++i)
            {
                Feature* feature = features[i];
                if (IsFlatPatternFeature(feature))
                {
                    return feature;
                }
            }
        }
        catch (...)
        {
        }

        try
        {
            for (FeatureCollection::iterator it = workPart->Features()->begin(); it != workPart->Features()->end(); ++it)
            {
                Feature* feature = *it;

                FlatPatternBuilder* builder = NULL;
                try
                {
                    builder = manager->CreateFlatPatternBuilder(feature);
                    Face* upwardFace = builder->UpwardFace() == NULL ? NULL : builder->UpwardFace()->Value();
                    bool matched = BodyContainsFace(body, upwardFace);
                    builder->Destroy();
                    builder = NULL;
                    if (matched)
                    {
                        return feature;
                    }
                }
                catch (...)
                {
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
        }
        catch (...)
        {
        }

        return NULL;
    }

    Body* ResolveFlatSolidBodyFromFeature(Feature* feature)
    {
        if (feature == NULL) return NULL;
        try
        {
            std::vector<Body*> bodies = feature->GetBodies();
            for (size_t i = 0; i < bodies.size(); ++i)
            {
                if (bodies[i] != NULL && IsAlive(bodies[i]->Tag())) return bodies[i];
            }
        }
        catch (...) {}
        try
        {
            std::vector<Feature*> children = feature->GetAllChildren();
            for (size_t i = 0; i < children.size(); ++i)
            {
                Body* childBody = ResolveFlatSolidBodyFromFeature(children[i]);
                if (childBody != NULL) return childBody;
            }
        }
        catch (...) {}
        return NULL;
    }

    std::string AskUfFeatureTypeText(tag_t featureTag)
    {
        if (featureTag == NULL_TAG) return std::string();
        char* typeText = NULL;
        std::string result;
        if (UF_MODL_ask_feat_type(featureTag, &typeText) == 0 && typeText != NULL)
        {
            result = typeText;
        }
        if (typeText != NULL)
        {
            UF_free(typeText);
        }
        return result;
    }

    Body* BodyFromFeatureBodyTag(tag_t featureTag)
    {
        if (featureTag == NULL_TAG) return NULL;
        tag_t bodyTag = NULL_TAG;
        if (UF_MODL_ask_feat_body(featureTag, &bodyTag) != 0 || bodyTag == NULL_TAG || !IsAlive(bodyTag))
        {
            return NULL;
        }
        try
        {
            return dynamic_cast<Body*>(NXObjectManager::Get(bodyTag));
        }
        catch (...)
        {
            return NULL;
        }
    }

    Body* ResolveFlatSolidBodyLike08(Feature* flatFeature, std::string* resolveSource)
    {
        if (flatFeature == NULL) return NULL;
        std::vector<tag_t> candidateFeatures;
        std::set<tag_t> seen;

        int numParents = 0;
        tag_t* parentFeatures = NULL;
        int numChildren = 0;
        tag_t* childFeatures = NULL;
        if (UF_MODL_ask_feat_relatives(flatFeature->Tag(), &numParents, &parentFeatures, &numChildren, &childFeatures) == 0)
        {
            for (int i = 0; i < numParents; ++i)
            {
                const tag_t featureTag = parentFeatures == NULL ? NULL_TAG : parentFeatures[i];
                const std::string typeText = AskUfFeatureTypeText(featureTag);
                if ((ContainsTextNoCase(typeText, "SB_FLAT_SOLID") || ContainsTextNoCase(typeText, "FLAT_SOLID")) &&
                    seen.insert(featureTag).second)
                {
                    candidateFeatures.push_back(featureTag);
                }
            }
        }
        if (parentFeatures != NULL) UF_free(parentFeatures);
        if (childFeatures != NULL) UF_free(childFeatures);

        for (size_t i = 0; i < candidateFeatures.size(); ++i)
        {
            Body* body = BodyFromFeatureBodyTag(candidateFeatures[i]);
            if (body != NULL)
            {
                if (resolveSource != NULL)
                {
                    *resolveSource = "08_relatives:" + AskUfFeatureTypeText(candidateFeatures[i]);
                }
                return body;
            }
        }

        return NULL;
    }

    void TryMakeFlatSolidExternal(Feature* flatFeature)
    {
        (void)flatFeature;
        // Keep the SB Flat Solid internal; validation resolves it through flat pattern objects.
    }

    void TryMakeFlatSolidInternal(Feature* flatFeature)
    {
        if (flatFeature == NULL) return;
        try
        {
            NXOpen::Features::FlatPattern* flatPattern =
                dynamic_cast<NXOpen::Features::FlatPattern*>(NXObjectManager::Get(flatFeature->Tag()));
            if (flatPattern != NULL)
            {
                flatPattern->MakeFlatSolidInternal();
            }
        }
        catch (...) {}
    }

    Body* BodyFromFlatSolidTaggedObject(TaggedObject* object)
    {
        if (object == NULL) return NULL;
        try
        {
            Edge* edge = dynamic_cast<Edge*>(object);
            if (edge != NULL)
            {
                Body* body = edge->GetBody();
                return body != NULL && IsAlive(body->Tag()) ? body : NULL;
            }
        }
        catch (...) {}
        try
        {
            Face* face = dynamic_cast<Face*>(object);
            if (face != NULL)
            {
                Body* body = face->GetBody();
                return body != NULL && IsAlive(body->Tag()) ? body : NULL;
            }
        }
        catch (...) {}
        return NULL;
    }

    Body* ResolveFlatSolidBodyFromFlatPatternObjects(Feature* flatFeature, std::string* resolveSource)
    {
        if (flatFeature == NULL) return NULL;
        NXOpen::Features::FlatPattern* flatPattern = NULL;
        try
        {
            flatPattern = dynamic_cast<NXOpen::Features::FlatPattern*>(NXObjectManager::Get(flatFeature->Tag()));
        }
        catch (...)
        {
            flatPattern = NULL;
        }
        if (flatPattern == NULL) return NULL;

        int edgeObjectCount = 0;
        int faceObjectCount = 0;
        auto tryEdgeObjects = [&](const std::vector<NXOpen::Features::FlatPattern::ObjectDataEdge>& objects, const char* label) -> Body*
        {
            edgeObjectCount += static_cast<int>(objects.size());
            for (size_t i = 0; i < objects.size(); ++i)
            {
                Body* body = BodyFromFlatSolidTaggedObject(objects[i].FlatSolidObject);
                if (body != NULL)
                {
                    if (resolveSource != NULL)
                    {
                        *resolveSource = std::string("flatpattern_object_edge:") + label +
                            ":edgeObjects=" + std::to_string(edgeObjectCount) +
                            ":faceObjects=" + std::to_string(faceObjectCount);
                    }
                    return body;
                }
            }
            return NULL;
        };
        auto tryFaceObjects = [&](const std::vector<NXOpen::Features::FlatPattern::ObjectDataFace>& objects, const char* label) -> Body*
        {
            faceObjectCount += static_cast<int>(objects.size());
            for (size_t i = 0; i < objects.size(); ++i)
            {
                Body* body = BodyFromFlatSolidTaggedObject(objects[i].FlatSolidObject);
                if (body != NULL)
                {
                    if (resolveSource != NULL)
                    {
                        *resolveSource = std::string("flatpattern_object_face:") + label +
                            ":edgeObjects=" + std::to_string(edgeObjectCount) +
                            ":faceObjects=" + std::to_string(faceObjectCount);
                    }
                    return body;
                }
            }
            return NULL;
        };

        try
        {
            std::vector<NXOpen::Features::FlatPattern::ObjectDataEdge> edgeObjects;
            flatPattern->GetExteriorCurves(edgeObjects);
            Body* body = tryEdgeObjects(edgeObjects, "ExteriorCurves");
            if (body != NULL) return body;

            edgeObjects.clear();
            flatPattern->GetBendTangentLines(edgeObjects);
            body = tryEdgeObjects(edgeObjects, "BendTangentLines");
            if (body != NULL) return body;

            edgeObjects.clear();
            flatPattern->GetInteriorCutoutCurves(edgeObjects);
            body = tryEdgeObjects(edgeObjects, "InteriorCutoutCurves");
            if (body != NULL) return body;

            edgeObjects.clear();
            flatPattern->GetInteriorFeatureCurves(edgeObjects);
            body = tryEdgeObjects(edgeObjects, "InteriorFeatureCurves");
            if (body != NULL) return body;

            edgeObjects.clear();
            flatPattern->GetHoleFeatureCurves(edgeObjects);
            body = tryEdgeObjects(edgeObjects, "HoleFeatureCurves");
            if (body != NULL) return body;
        }
        catch (...) {}

        try
        {
            std::vector<NXOpen::Features::FlatPattern::ObjectDataFace> faceObjects;
            flatPattern->GetBendUpCenterLines(faceObjects);
            Body* body = tryFaceObjects(faceObjects, "BendUpCenterLines");
            if (body != NULL) return body;

            faceObjects.clear();
            flatPattern->GetBendDownCenterLines(faceObjects);
            body = tryFaceObjects(faceObjects, "BendDownCenterLines");
            if (body != NULL) return body;

            faceObjects.clear();
            flatPattern->GetInnerMoldLines(faceObjects);
            body = tryFaceObjects(faceObjects, "InnerMoldLines");
            if (body != NULL) return body;

            faceObjects.clear();
            flatPattern->GetOuterMoldLines(faceObjects);
            body = tryFaceObjects(faceObjects, "OuterMoldLines");
            if (body != NULL) return body;

            faceObjects.clear();
            flatPattern->GetLighteningHoleCenters(faceObjects);
            body = tryFaceObjects(faceObjects, "LighteningHoleCenters");
            if (body != NULL) return body;
        }
        catch (...) {}

        if (resolveSource != NULL)
        {
            *resolveSource = "flatpattern_object:not_found:edgeObjects=" + std::to_string(edgeObjectCount) +
                ":faceObjects=" + std::to_string(faceObjectCount);
        }
        return NULL;
    }

    std::set<tag_t> CollectPartSolidBodyTags(Part* part)
    {
        std::set<tag_t> tags;
        if (part == NULL || part->Bodies() == NULL) return tags;
        try
        {
            for (BodyCollection::iterator it = part->Bodies()->begin(); it != part->Bodies()->end(); ++it)
            {
                Body* body = *it;
                if (body == NULL || !IsAlive(body->Tag())) continue;
                try
                {
                    if (!body->IsSolidBody()) continue;
                }
                catch (...)
                {
                    continue;
                }
                tags.insert(body->Tag());
            }
        }
        catch (...) {}
        return tags;
    }

    Body* ResolveFlatSolidBodyFromPartDelta(Part* part, const std::set<tag_t>& beforeBodyTags, Body* sourceBody)
    {
        if (part == NULL || part->Bodies() == NULL) return NULL;
        Body* fallback = NULL;
        try
        {
            for (BodyCollection::iterator it = part->Bodies()->begin(); it != part->Bodies()->end(); ++it)
            {
                Body* body = *it;
                if (body == NULL || !IsAlive(body->Tag())) continue;
                try
                {
                    if (!body->IsSolidBody()) continue;
                }
                catch (...)
                {
                    continue;
                }
                if (sourceBody != NULL && body->Tag() == sourceBody->Tag()) continue;
                if (beforeBodyTags.find(body->Tag()) != beforeBodyTags.end()) continue;

                const std::string name = BodyName(body);
                if (ContainsTextNoCase(name, "SB Flat") ||
                    ContainsTextNoCase(name, "Flat Solid") ||
                    ContainsTextNoCase(name, "FlatPattern") ||
                    ContainsTextNoCase(name, "Flat Pattern"))
                {
                    return body;
                }
                if (fallback == NULL)
                {
                    fallback = body;
                }
            }
        }
        catch (...) {}
        return fallback;
    }

    Face* FindLargestFace(Body* body)
    {
        if (body == NULL) return NULL;
        Face* bestFace = NULL;
        double bestArea = -1.0;
        try
        {
            std::vector<Face*> faces = body->GetFaces();
            for (size_t i = 0; i < faces.size(); ++i)
            {
                Face* face = faces[i];
                if (face == NULL || !IsAlive(face->Tag())) continue;
                double area = FaceAreaScore(face);
                if (area > bestArea)
                {
                    bestArea = area;
                    bestFace = face;
                }
            }
        }
        catch (...) {}
        return bestFace;
    }

    bool MeasureBodyThicknessAlongLargestFaceNormal(Body* body, double* measuredThickness)
    {
        if (body == NULL || measuredThickness == NULL) return false;
        Face* largestFace = FindLargestFace(body);
        if (largestFace == NULL) return false;
        double facePoint[3] = { 0.0, 0.0, 0.0 };
        double normal[3] = { 0.0, 0.0, 1.0 };
        if (!AskFacePointFromEdges(largestFace, facePoint) || !AskNxOpenCreatedFaceNormal(largestFace, facePoint, normal)) return false;
        std::vector<Point3d> points = CollectBodyEdgePoints(body);
        if (points.empty()) return false;
        double minProjection = std::numeric_limits<double>::max();
        double maxProjection = -std::numeric_limits<double>::max();
        for (size_t i = 0; i < points.size(); ++i)
        {
            const double projection = points[i].X * normal[0] + points[i].Y * normal[1] + points[i].Z * normal[2];
            minProjection = std::min(minProjection, projection);
            maxProjection = std::max(maxProjection, projection);
        }
        *measuredThickness = std::fabs(maxProjection - minProjection);
        return *measuredThickness > 1e-6;
    }

    bool MeasureBodyBoxAlongLargestFaceNormal(Part* workPart, Body* body, FlatSolidThicknessCheckInfo* info)
    {
        if (body == NULL || info == NULL) return false;
        info->flatBody = body;
        if (workPart == NULL || workPart->ToolingManager() == NULL || workPart->ToolingManager()->StockSizes() == NULL)
        {
            info->reason = "work part/stock size prerequisites missing";
            return false;
        }

        NXOpen::Tooling::StockSizeBuilder* stockSizeBuilder = NULL;
        try
        {
            stockSizeBuilder = workPart->ToolingManager()->StockSizes()->CreateStocksizeBuilder();
            if (stockSizeBuilder == NULL)
            {
                info->reason = "stock size builder create failed";
                return false;
            }

            stockSizeBuilder->SelectBody()->Clear();
            const bool added = stockSizeBuilder->SelectBody()->Add(body);
            if (!added)
            {
                info->reason = "stock size select flat body failed";
                stockSizeBuilder->Destroy();
                return false;
            }

            Matrix3x3 identityMatrix;
            identityMatrix.Xx = 1.0; identityMatrix.Xy = 0.0; identityMatrix.Xz = 0.0;
            identityMatrix.Yx = 0.0; identityMatrix.Yy = 1.0; identityMatrix.Yz = 0.0;
            identityMatrix.Zx = 0.0; identityMatrix.Zy = 0.0; identityMatrix.Zz = 1.0;
            stockSizeBuilder->SetManipulatorOrientation(identityMatrix);
            stockSizeBuilder->SetType(NXOpen::Tooling::StockSizeBuilder::TypesBlock);
            stockSizeBuilder->Clearance()->SetFormula("0");
            stockSizeBuilder->SetSizePrecision(6);
            stockSizeBuilder->SetRadialOffset(0.0);
            stockSizeBuilder->SetReferenceCsysType(NXOpen::Tooling::StockSizeBuilder::RefCsysTypeAbsoluteDisplayedPart);

            Point3d minPoint(0.0, 0.0, 0.0);
            std::vector<double> edgeLengths;
            Matrix3x3 boxMatrix;
            stockSizeBuilder->CalculateBoxSize(&minPoint, edgeLengths, &boxMatrix);
            stockSizeBuilder->Destroy();
            stockSizeBuilder = NULL;

            if (edgeLengths.size() < 3)
            {
                info->reason = "stock size returned less than 3 dimensions: count=" + std::to_string(edgeLengths.size());
                return false;
            }

            std::vector<double> dims;
            dims.push_back(std::fabs(edgeLengths[0]));
            dims.push_back(std::fabs(edgeLengths[1]));
            dims.push_back(std::fabs(edgeLengths[2]));
            info->flatSizeX = dims[0];
            info->flatSizeY = dims[1];
            std::sort(dims.begin(), dims.end(), std::greater<double>());
            info->flatSizeU = dims[0];
            info->flatSizeV = dims[1];
            info->flatSizeNormal = dims[2];
            info->normal[0] = boxMatrix.Zx;
            info->normal[1] = boxMatrix.Zy;
            info->normal[2] = boxMatrix.Zz;
            Normalize3(info->normal);

            double minPositive = std::numeric_limits<double>::max();
            for (size_t i = 0; i < dims.size(); ++i)
            {
                if (dims[i] > 1e-6 && dims[i] < minPositive) minPositive = dims[i];
            }
            if (minPositive == std::numeric_limits<double>::max())
            {
                info->reason = "stock size invalid: "
                    + FormatDouble(info->flatSizeU, 6) + " x "
                    + FormatDouble(info->flatSizeV, 6) + " x "
                    + FormatDouble(info->flatSizeNormal, 6);
                return false;
            }

            info->measuredThickness = minPositive;
            info->reason = "stock_size_builder:offset=0";
            return true;
        }
        catch (const NXException& ex)
        {
            info->reason = std::string("stock size measure NXException: ") + ex.Message();
        }
        catch (const std::exception& ex)
        {
            info->reason = std::string("stock size measure exception: ") + ex.what();
        }
        catch (...)
        {
            info->reason = "stock size measure unknown exception";
        }

        if (stockSizeBuilder != NULL)
        {
            try { stockSizeBuilder->Destroy(); } catch (...) {}
        }
        return false;
    }
    bool TryMeasureFlatBodyThicknessOnly(Body* body, double* thickness)
    {
        if (body == NULL || thickness == NULL) return false;
        FlatSolidThicknessCheckInfo info;
        Part* workPart = Session::GetSession() == NULL || Session::GetSession()->Parts() == NULL
            ? NULL
            : Session::GetSession()->Parts()->Work();
        if (!MeasureBodyBoxAlongLargestFaceNormal(workPart, body, &info)) return false;
        *thickness = info.measuredThickness;
        return true;
    }

    Body* ResolveBestFlatSolidBodyFromPartDelta(
        Part* part,
        const std::set<tag_t>& beforeBodyTags,
        Body* sourceBody,
        double expectedThickness,
        std::string* resolveSource)
    {
        if (part == NULL || part->Bodies() == NULL) return NULL;
        Body* bestBody = NULL;
        double bestError = std::numeric_limits<double>::max();
        int candidateCount = 0;
        std::ostringstream debug;
        try
        {
            for (BodyCollection::iterator it = part->Bodies()->begin(); it != part->Bodies()->end(); ++it)
            {
                Body* body = *it;
                if (body == NULL || !IsAlive(body->Tag())) continue;
                try
                {
                    if (!body->IsSolidBody()) continue;
                }
                catch (...)
                {
                    continue;
                }
                if (sourceBody != NULL && body->Tag() == sourceBody->Tag()) continue;
                if (beforeBodyTags.find(body->Tag()) != beforeBodyTags.end()) continue;
                ++candidateCount;
                debug << "{tag=" << static_cast<int>(body->Tag())
                      << ",name=" << BodyName(body);

                double measured = 0.0;
                if (!TryMeasureFlatBodyThicknessOnly(body, &measured))
                {
                    debug << ",measure=failed}";
                    continue;
                }
                debug << ",thickness=" << FormatDouble(measured, 6) << "}";
                const double error = expectedThickness > 0.0 ? std::fabs(measured - expectedThickness) : 0.0;
                if (error < bestError)
                {
                    bestError = error;
                    bestBody = body;
                }
            }
        }
        catch (...) {}
        if (bestBody != NULL && resolveSource != NULL)
        {
            *resolveSource = "part_delta:candidates=" + std::to_string(candidateCount) + ":" + debug.str();
        }
        return bestBody;
    }

    void DeleteCreatedFeatures(Feature* flatFeature, Feature* convertFeature)
    {
        Session* session = Session::GetSession();
        if (session == NULL || session->UpdateManager() == NULL) return;
        std::vector<TaggedObject*> deleteObjects;
        if (flatFeature != NULL && IsAlive(flatFeature->Tag())) deleteObjects.push_back(flatFeature);
        if (convertFeature != NULL && IsAlive(convertFeature->Tag())) deleteObjects.push_back(convertFeature);
        if (deleteObjects.empty()) return;
        try
        {
            Update* updateManager = session->UpdateManager();
            updateManager->AddObjectsToDeleteList(deleteObjects);
            Session::UndoMarkId markId = session->SetUndoMark(Session::MarkVisibilityInvisible, "Delete invalid sheetmetal features");
            updateManager->DoUpdate(markId);
        }
        catch (...) {}
    }

    bool ValidateFlatSolidThickness(
        Part* workPart,
        SheetmetalManager* manager,
        Body* sheetBody,
        Feature* flatFeature,
        const std::set<tag_t>& beforeFlatBodyTags,
        FlatSolidThicknessCheckInfo* info)
    {
        if (info != NULL)
        {
            *info = FlatSolidThicknessCheckInfo();
        }
        if (manager == NULL || sheetBody == NULL || flatFeature == NULL)
        {
            if (info != NULL) info->reason = "manager/body/flat feature is null";
            return false;
        }
        double expected = 0.0;
        try { expected = manager->GetBodyThickness(sheetBody); } catch (...) { expected = 0.0; }
        if (info != NULL) info->expectedThickness = expected;
        if (expected <= 1e-6)
        {
            if (info != NULL) info->reason = "sheetmetal thickness query failed";
            return false;
        }
        std::string resolveSource;
        TryMakeFlatSolidExternal(flatFeature);
        Body* flatBody = ResolveFlatSolidBodyFromFlatPatternObjects(flatFeature, &resolveSource);
        if (flatBody == NULL)
        {
            if (info != NULL)
            {
                info->resolveSource = resolveSource;
                info->reason = "SB Flat Solid body not found from flat pattern objects";
            }
            return false;
        }
        FlatSolidThicknessCheckInfo localInfo;
        localInfo.expectedThickness = expected;
        localInfo.resolveSource = resolveSource;
        if (!MeasureBodyBoxAlongLargestFaceNormal(workPart, flatBody, &localInfo))
        {
            if (info != NULL) *info = localInfo;
            return false;
        }
        const double tolerance = std::max(0.05, expected * 0.05);
        localInfo.passed = std::fabs(localInfo.measuredThickness - expected) <= tolerance;
        localInfo.reason = localInfo.passed ? "ok" : "thickness mismatch";
        TryMakeFlatSolidInternal(flatFeature);
        if (info != NULL) *info = localInfo;
        return localInfo.passed;
    }

    Feature* CommitFlatPattern(
        Part* workPart,
        Feature* existingFlatPattern,
        Face* flatFace,
        Edge* xAxisEdge,
        const Point3d& referenceVertex)
    {
        if (workPart == NULL || flatFace == NULL)
        {
            return NULL;
        }

        NXOpen::Features::SheetMetal::FlatPatternBuilder* flatPatternBuilder = NULL;
        try
        {
            flatPatternBuilder = workPart->Features()->SheetmetalManager()->CreateFlatPatternBuilder(existingFlatPattern);
            if (existingFlatPattern == NULL)
            {
                flatPatternBuilder->SetApplicationContext(NXOpen::Features::SheetMetal::ApplicationContextNxSheetMetal);
                flatPatternBuilder->SetKeepFlatSolidExternal(false);
            }
            flatPatternBuilder->SetOrientation(xAxisEdge == NULL
                ? NXOpen::Features::SheetMetal::FlatSolidBuilder::OrientationTypeDefault
                : NXOpen::Features::SheetMetal::FlatSolidBuilder::OrientationTypeEdge);
            flatPatternBuilder->OuterCornerTreatment()->SetTreatmentType(NXOpen::Features::SheetMetal::CornerTreatmentBuilder::CornerTreatmentTypeRadius);
            flatPatternBuilder->OuterCornerTreatment()->Value()->SetFormula("1");
            flatPatternBuilder->UpwardFace()->SetValue(flatFace);
            if (xAxisEdge != NULL)
            {
                flatPatternBuilder->XAxisEdge()->SetValue(xAxisEdge);
                flatPatternBuilder->SetReferenceVertex(referenceVertex);
            }

            Feature* flatFeature = flatPatternBuilder->CommitFeature();
            flatPatternBuilder->Destroy();
            Session::GetSession()->CleanUpFacetedFacesAndEdges();
            return flatFeature;
        }
        catch (...)
        {
            if (flatPatternBuilder != NULL)
            {
                try
                {
                    flatPatternBuilder->Destroy();
                }
                catch (...)
                {
                }
            }
            throw;
        }
    }

    void ApplyFaceColor(Session* session, Face* face, int color)
    {
        if (session == NULL || face == NULL || color <= 0)
        {
            return;
        }

        DisplayModification* modification = session->DisplayManager()->NewDisplayModification();
        modification->SetApplyToAllFaces(false);
        modification->SetApplyToOwningParts(false);
        modification->SetNewColor(color);
        std::vector<DisplayableObject*> objects;
        objects.push_back(face);
        modification->Apply(objects);
        delete modification;
    }


    std::vector<DisplayableObject*> ResolveDisplayOccurrences(Body* body)
    {
        std::vector<DisplayableObject*> objects;
        if (body == NULL) return objects;
        std::set<tag_t> seen;
        tag_t* occurrences = NULL;
        const int occurrenceCount = UF_ASSEM_ask_occs_of_entity(body->Tag(), &occurrences);
        if (occurrenceCount > 0 && occurrences != NULL)
        {
            for (int i = 0; i < occurrenceCount; ++i)
            {
                tag_t occurrenceTag = occurrences[i];
                if (occurrenceTag != NULL_TAG && seen.insert(occurrenceTag).second)
                {
                    try
                    {
                        DisplayableObject* occurrenceObject =
                            dynamic_cast<DisplayableObject*>(NXObjectManager::Get(occurrenceTag));
                        if (occurrenceObject != NULL)
                        {
                            objects.push_back(occurrenceObject);
                        }
                    }
                    catch (...) {}
                }
            }
            UF_free(occurrences);
        }
        if (objects.empty() && seen.insert(body->Tag()).second)
        {
            objects.push_back(body);
        }
        return objects;
    }

    void ApplyBodyColor(Session* session, Body* body, int color)
    {
        if (session == NULL || body == NULL || color <= 0)
        {
            return;
        }

        DisplayModification* modification = session->DisplayManager()->NewDisplayModification();
        modification->SetApplyToAllFaces(true);
        modification->SetApplyToOwningParts(false);
        modification->SetNewColor(color);
        std::vector<DisplayableObject*> objects = ResolveDisplayOccurrences(body);
        modification->Apply(objects);
        delete modification;
    }

    void HideDisplayBody(Body* body)
    {
        std::vector<DisplayableObject*> objects = ResolveDisplayOccurrences(body);
        for (size_t i = 0; i < objects.size(); ++i)
        {
            if (objects[i] != NULL)
            {
                UF_OBJ_set_blank_status(objects[i]->Tag(), UF_OBJ_BLANKED);
            }
        }
    }

    void HighlightDisplayBody(Body* body)
    {
        std::vector<DisplayableObject*> objects = ResolveDisplayOccurrences(body);
        for (size_t i = 0; i < objects.size(); ++i)
        {
            if (objects[i] != NULL)
            {
                UF_DISP_set_highlight(objects[i]->Tag(), 1);
            }
        }
    }

    int ClampLayer(int layer)
    {
        if (layer < 1)
        {
            return 1;
        }
        return layer > 256 ? 256 : layer;
    }

    bool GetLogicalValue(Toggle* toggle, bool defaultValue)
    {
        if (toggle == NULL)
        {
            return defaultValue;
        }

        PropertyList* props = toggle->GetProperties();
        bool value = props->GetLogical("Value");
        delete props;
        return value;
    }

    int GetIntegerValue(IntegerBlock* block, int defaultValue)
    {
        if (block == NULL)
        {
            return defaultValue;
        }

        PropertyList* props = block->GetProperties();
        int value = props->GetInteger("Value");
        delete props;
        return value;
    }

    double GetDoubleValue(DoubleBlock* block, double defaultValue)
    {
        if (block == NULL)
        {
            return defaultValue;
        }

        PropertyList* props = block->GetProperties();
        double value = props->GetDouble("Value");
        delete props;
        return value;
    }

    int GetColorValue(ObjectColorPicker* picker, int defaultValue)
    {
        if (picker == NULL)
        {
            return defaultValue;
        }

        std::vector<int> values = picker->GetValue();
        return values.empty() ? defaultValue : values[0];
    }

    std::string BuildResultMessage(const std::vector<BodyResult>& results)
    {
        int convertCount = 0;
        int flatCount = 0;
        int failCount = 0;
        for (size_t i = 0; i < results.size(); ++i)
        {
            if (results[i].convertOk)
            {
                ++convertCount;
            }
            if (results[i].flatOk)
            {
                ++flatCount;
            }
            if (!results[i].error.empty())
            {
                ++failCount;
            }
        }

        std::ostringstream builder;
        builder << "C++自动转钣金执行完成\n";
        builder << "成功转钣金: " << convertCount << " 个实体\n";
        builder << "成功展开: " << flatCount << " 个实体\n";
        builder << "失败/跳过: " << failCount << " 个实体\n\n";
        if (!results.empty())
        {
            builder << "[明细]\n";
            for (size_t i = 0; i < results.size(); ++i)
            {
                builder << "- 图层 " << results[i].layer << " / " << results[i].name
                        << "  转钣金=" << (results[i].convertOk ? "成功" : "未执行")
                        << "  调因子面=" << results[i].neutralFaceCount
                        << "  展开=" << (results[i].flatOk ? "成功" : "失败");
                if (!results[i].error.empty())
                {
                    builder << "  原因=" << results[i].error;
                }
                builder << "\n";
            }
        }

        return builder.str();
    }}

Session* PiLianZuanBanJinDialog::theSession = NULL;
UI* PiLianZuanBanJinDialog::theUI = NULL;


    void ShowPiLianRulesTableBuiltin(UI* ui)
    {
        std::string dlxPath = FindRulesTableDlxPath();
        if (dlxPath.empty())
        {
            ShowMessage(ui, "自动转钣金 C++", NXMessageBox::DialogTypeInformation, "未找到UG内置规则表。");
            return;
        }

        try
        {
            PiLianRulesTableDialog rulesDialog(ui, dlxPath);
            rulesDialog.Show();
        }
        catch (const std::exception& ex)
        {
            ShowMessage(ui, "自动转钣金 C++", NXMessageBox::DialogTypeError, ex.what());
        }
        catch (...)
        {
            ShowMessage(ui, "自动转钣金 C++", NXMessageBox::DialogTypeError, "UG内置规则表打开失败。");
        }
    }

extern "C" DllExport void Zhihui_ShowPiLianRulesTable()
{
    ShowPiLianRulesTableBuiltin(UI::GetUI());
}

PiLianZuanBanJinDialog::PiLianZuanBanJinDialog()
    : theDialog(NULL),
      theDlxFileName(NULL),
      strategyGroup(NULL),
      advancedGroup(NULL),
      markerLineFaceUpToggle(NULL),
      autoSaveAfterRunToggle(NULL),
      facePreferenceEnum(NULL),
      failureActionEnum(NULL),
      fixedFaceColorToggle(NULL),
      fixedFaceColorPicker(NULL),
      reliefDepthDouble(NULL),
      reliefWidthDouble(NULL),
      innerRadiusDouble(NULL),
      skipSmallBodyToggle(NULL),
      minLengthDouble(NULL),
      minWidthDouble(NULL),
      skipTallCylinderToggle(NULL),
      layerRangeToggle(NULL),
      startLayerInteger(NULL),
      endLayerInteger(NULL),
      largestBodyOnlyToggle(NULL),
      settingsButton(NULL),
      assemblySelectionActive(false),
      selectedAssemblyParts()
{
    PiLianZuanBanJinDialog::theSession = Session::GetSession();
    PiLianZuanBanJinDialog::theUI = UI::GetUI();
    const std::string dlxPath = zhihui_embedded_dialog::ExtractDlxToRandomPath(
        IDR_ZH_DLX_PILIANZUANBANJIN_DLX,
        L"PiLianZuanBanJin.dlx");
    if (dlxPath.empty())
    {
        throw std::runtime_error("Embedded DLX resource missing: PiLianZuanBanJin.dlx");
    }
    theDialog = theUI->CreateDialog(dlxPath.c_str());
    theDialog->AddInitializeHandler(make_callback(this, &PiLianZuanBanJinDialog::initialize_cb));
    theDialog->AddUpdateHandler(make_callback(this, &PiLianZuanBanJinDialog::update_cb));
    theDialog->AddOkHandler(make_callback(this, &PiLianZuanBanJinDialog::ok_cb));
    theDialog->AddApplyHandler(make_callback(this, &PiLianZuanBanJinDialog::apply_cb));
}

PiLianZuanBanJinDialog::~PiLianZuanBanJinDialog()
{
    if (theDialog != NULL)
    {
        delete theDialog;
        theDialog = NULL;
    }
}

int PiLianZuanBanJinDialog::Show()
{
    assemblySelectionActive = false;
    selectedAssemblyParts.clear();

    Part* assemblyPart = ResolveCurrentAssemblyPart(theSession);
    if (assemblyPart != NULL)
    {
        std::vector<BatchPartCandidate> candidates = CollectBatchPartCandidates(assemblyPart);
        if (candidates.empty())
        {
            ShowMessage(theUI, "自动转钣金 C++", NXMessageBox::DialogTypeInformation, "当前装配没有可选择的子部件。");
            return 0;
        }

        if (!ShowBatchPartPicker(candidates, selectedAssemblyParts))
        {
            return 0;
        }

        assemblySelectionActive = true;
    }

    return theDialog->Launch();
}

void PiLianZuanBanJinDialog::initialize_cb()
{
    strategyGroup = dynamic_cast<NXOpen::BlockStyler::Group*>(theDialog->TopBlock()->FindBlock("strategyGroup"));
    advancedGroup = dynamic_cast<NXOpen::BlockStyler::Group*>(theDialog->TopBlock()->FindBlock("advancedGroup"));
    markerLineFaceUpToggle = dynamic_cast<Toggle*>(theDialog->TopBlock()->FindBlock("markerLineFaceUpToggle"));
    autoSaveAfterRunToggle = dynamic_cast<Toggle*>(theDialog->TopBlock()->FindBlock("autoSaveAfterRunToggle"));
    facePreferenceEnum = dynamic_cast<Enumeration*>(theDialog->TopBlock()->FindBlock("facePreferenceEnum"));
    failureActionEnum = dynamic_cast<Enumeration*>(theDialog->TopBlock()->FindBlock("failureActionEnum"));
    fixedFaceColorToggle = dynamic_cast<Toggle*>(theDialog->TopBlock()->FindBlock("fixedFaceColorToggle"));
    fixedFaceColorPicker = dynamic_cast<ObjectColorPicker*>(theDialog->TopBlock()->FindBlock("fixedFaceColorPicker"));
    reliefDepthDouble = dynamic_cast<DoubleBlock*>(theDialog->TopBlock()->FindBlock("reliefDepthDouble"));
    reliefWidthDouble = dynamic_cast<DoubleBlock*>(theDialog->TopBlock()->FindBlock("reliefWidthDouble"));
    innerRadiusDouble = dynamic_cast<DoubleBlock*>(theDialog->TopBlock()->FindBlock("innerRadiusDouble"));
    skipSmallBodyToggle = dynamic_cast<Toggle*>(theDialog->TopBlock()->FindBlock("skipSmallBodyToggle"));
    minLengthDouble = dynamic_cast<DoubleBlock*>(theDialog->TopBlock()->FindBlock("minLengthDouble"));
    minWidthDouble = dynamic_cast<DoubleBlock*>(theDialog->TopBlock()->FindBlock("minWidthDouble"));
    skipTallCylinderToggle = dynamic_cast<Toggle*>(theDialog->TopBlock()->FindBlock("skipTallCylinderToggle"));
    layerRangeToggle = dynamic_cast<Toggle*>(theDialog->TopBlock()->FindBlock("layerRangeToggle"));
    startLayerInteger = dynamic_cast<IntegerBlock*>(theDialog->TopBlock()->FindBlock("startLayerInteger"));
    endLayerInteger = dynamic_cast<IntegerBlock*>(theDialog->TopBlock()->FindBlock("endLayerInteger"));
    largestBodyOnlyToggle = dynamic_cast<Toggle*>(theDialog->TopBlock()->FindBlock("largestBodyOnlyToggle"));
    settingsButton = dynamic_cast<Button*>(theDialog->TopBlock()->FindBlock("settingsButton"));
    InitializeValues();
    UpdateSensitivity();
}

int PiLianZuanBanJinDialog::update_cb(UIBlock* block)
{
    if (block == settingsButton)
    {
        ShowPiLianRulesTableBuiltin(theUI);
    }
    else if (block == skipSmallBodyToggle || block == layerRangeToggle || block == fixedFaceColorToggle)
    {
        UpdateSensitivity();
    }

    return 0;
}

int PiLianZuanBanJinDialog::ok_cb()
{
    try
    {
        Run(CollectOptions());
        return 0;
    }
    catch (const std::exception& ex)
    {
        ShowMessage(theUI, "自动转钣金 C++", NXMessageBox::DialogTypeError, ex.what());
        return 1;
    }
}

int PiLianZuanBanJinDialog::apply_cb()
{
    return ok_cb();
}

void PiLianZuanBanJinDialog::InitializeValues()
{
    if (theDialog != NULL && theDialog->TopBlock() != NULL)
    {
        theDialog->TopBlock()->SetLabel(U8("自动转钣金"));
    }

    SetLabel(strategyGroup, "系数策略");
    SetLabel(markerLineFaceUpToggle, "按标记面找基面");
    SetLabel(autoSaveAfterRunToggle, "运行完成自动保存");
    SetLabel(facePreferenceEnum, "固定面方向");
    SetLabel(failureActionEnum, "\xE8\xBD\xAC\xE9\x92\xA3\xE9\x87\x91\xE5\xB1\x95\xE5\xBC\x80\xE5\xA4\xB1\xE8\xB4\xA5\xE5\xA4\x84\xE7\x90\x86\xE6\x96\xB9\xE5\xBC\x8F");
    SetLabel(fixedFaceColorToggle, "展开基面颜色");
    SetLabel(fixedFaceColorPicker, "");
    SetLabel(reliefDepthDouble, "避让槽深");
    SetLabel(reliefWidthDouble, "避让槽宽");
    SetLabel(innerRadiusDouble, "内R半径");
    SetLabel(advancedGroup, "高级过滤");
    SetLabel(skipSmallBodyToggle, "跳过过小实体");
    SetLabel(minLengthDouble, "最小长度");
    SetLabel(minWidthDouble, "最小宽度");
    SetLabel(skipTallCylinderToggle, "跳过螺母，螺柱，螺钉");
    SetLabel(layerRangeToggle, "按图层范围查找实体");
    SetLabel(startLayerInteger, "初始图层");
    SetLabel(endLayerInteger, "结束图层");
    SetLabel(largestBodyOnlyToggle, "每层只处理最大实体");
    SetLabel(settingsButton, "规则设置...");

    if (facePreferenceEnum != NULL)
    {
        std::vector<NXString> members;
        members.push_back(U8("下折多"));
        members.push_back(U8("上折多"));
        facePreferenceEnum->SetEnumMembers(members);
        facePreferenceEnum->SetValueAsString(U8("下折多"));
    }

    if (failureActionEnum != NULL)
    {
        std::vector<NXString> members;
        members.push_back(U8("\xE9\x9A\x90\xE8\x97\x8F\xE6\x88\x90\xE5\x8A\x9F\xE7\x9A\x84\xE4\xBD\x93"));
        members.push_back(U8("\xE5\xA4\xB1\xE8\xB4\xA5\xE4\xBD\x93\xE6\x94\xB9\xE7\xBA\xA2\xE8\x89\xB2"));
        members.push_back(U8("\xE9\xAB\x98\xE4\xBA\xAE\xE5\xA4\xB1\xE8\xB4\xA5\xE4\xBD\x93"));
        failureActionEnum->SetEnumMembers(members);
        failureActionEnum->SetValueAsString(members[0]);
    }

    if (fixedFaceColorPicker != NULL)
    {
        fixedFaceColorPicker->SetValue(std::vector<int>(1, 6));
    }

    if (reliefDepthDouble != NULL)
    {
        reliefDepthDouble->SetValue(0.2);
    }
    if (reliefWidthDouble != NULL)
    {
        reliefWidthDouble->SetValue(0.2);
    }
    if (innerRadiusDouble != NULL)
    {
        innerRadiusDouble->SetValue(0.0);
    }

    std::string json = ReadAllText(FindConfigPath());
    if (!json.empty())
    {
        if (markerLineFaceUpToggle != NULL) markerLineFaceUpToggle->SetValue(ExtractJsonBool(json, "MarkerLineFaceUp", false));
        if (autoSaveAfterRunToggle != NULL) autoSaveAfterRunToggle->SetValue(ExtractJsonBool(json, "AutoSaveAfterRun", true));
        if (fixedFaceColorToggle != NULL) fixedFaceColorToggle->SetValue(ExtractJsonBool(json, "ApplyFixedFaceColor", true));
        if (skipSmallBodyToggle != NULL) skipSmallBodyToggle->SetValue(ExtractJsonBool(json, "SkipSmallBodyByWidthHeight", false));
        if (skipTallCylinderToggle != NULL) skipTallCylinderToggle->SetValue(
            ExtractJsonBool(json, "SkipFasteners", ExtractJsonBool(json, "SkipCylinderHeightGreaterThanDiameter", false)));
        if (layerRangeToggle != NULL) layerRangeToggle->SetValue(ExtractJsonBool(json, "FilterBodiesByLayerRange", true));
        if (startLayerInteger != NULL) startLayerInteger->SetValue(static_cast<int>(ExtractJsonNumber(json, "StartBodyLayer", 1)));
        if (endLayerInteger != NULL) endLayerInteger->SetValue(static_cast<int>(ExtractJsonNumber(json, "EndBodyLayer", 99)));
        if (largestBodyOnlyToggle != NULL) largestBodyOnlyToggle->SetValue(ExtractJsonBool(json, "ProcessLargestBodyOnlyPerLayer", false));
        if (failureActionEnum != NULL)
        {
            int failureAction = static_cast<int>(ExtractJsonNumber(json, "FailureAction", 1));
            if (failureAction < 1 || failureAction > 3) failureAction = 1;
            failureActionEnum->SetValueAsString(failureAction == 2
                ? U8("\xE5\xA4\xB1\xE8\xB4\xA5\xE4\xBD\x93\xE6\x94\xB9\xE7\xBA\xA2\xE8\x89\xB2")
                : (failureAction == 3
                    ? U8("\xE9\xAB\x98\xE4\xBA\xAE\xE5\xA4\xB1\xE8\xB4\xA5\xE4\xBD\x93")
                    : U8("\xE9\x9A\x90\xE8\x97\x8F\xE6\x88\x90\xE5\x8A\x9F\xE7\x9A\x84\xE4\xBD\x93")));
        }
        if (minLengthDouble != NULL) minLengthDouble->SetValue(ExtractJsonNumber(json, "MinBodyLengthForProcessing", 0.0));
        if (minWidthDouble != NULL) minWidthDouble->SetValue(ExtractJsonNumber(json, "MinBodyWidthForProcessing", 0.0));

        std::string preference = ExtractJsonString(json, "BaseFaceDirectionPreference", "PreferDownBends");
        if (facePreferenceEnum != NULL)
        {
            facePreferenceEnum->SetValueAsString(ContainsTextNoCase(preference, "Up") ? U8("上折多") : U8("下折多"));
        }
    }
}
void PiLianZuanBanJinDialog::UpdateSensitivity()
{
    const bool smallBodyEnabled = GetLogicalValue(skipSmallBodyToggle, false);
    if (minLengthDouble != NULL)
    {
        PropertyList* props = minLengthDouble->GetProperties();
        props->SetLogical("Enable", smallBodyEnabled);
        delete props;
    }

    if (minWidthDouble != NULL)
    {
        PropertyList* props = minWidthDouble->GetProperties();
        props->SetLogical("Enable", smallBodyEnabled);
        delete props;
    }

    const bool layerEnabled = GetLogicalValue(layerRangeToggle, true);
    if (startLayerInteger != NULL)
    {
        PropertyList* props = startLayerInteger->GetProperties();
        props->SetLogical("Enable", layerEnabled);
        delete props;
    }

    if (endLayerInteger != NULL)
    {
        PropertyList* props = endLayerInteger->GetProperties();
        props->SetLogical("Enable", layerEnabled);
        delete props;
    }

    if (fixedFaceColorPicker != NULL)
    {
        PropertyList* props = fixedFaceColorPicker->GetProperties();
        props->SetLogical("Enable", GetLogicalValue(fixedFaceColorToggle, true));
        delete props;
    }
}

AutoConvertOptions PiLianZuanBanJinDialog::CollectOptions() const
{
    AutoConvertOptions options;
    options.markerLineFaceUp = GetLogicalValue(markerLineFaceUpToggle, false);
    options.autoSaveAfterRun = GetLogicalValue(autoSaveAfterRunToggle, true);
    options.preferUpBends = facePreferenceEnum != NULL &&
        ToUtf8(facePreferenceEnum->ValueAsString()).find("上") != std::string::npos;
    options.applyFixedFaceColor = GetLogicalValue(fixedFaceColorToggle, true);
    options.fixedFaceColor = GetColorValue(fixedFaceColorPicker, 6);
    options.reliefDepth = std::max(0.0, GetDoubleValue(reliefDepthDouble, 0.2));
    options.reliefWidth = std::max(0.0, GetDoubleValue(reliefWidthDouble, 0.2));
    options.innerRadius = std::max(0.0, GetDoubleValue(innerRadiusDouble, 0.0));
    options.skipSmallBody = GetLogicalValue(skipSmallBodyToggle, false);
    options.minLength = GetDoubleValue(minLengthDouble, 0.0);
    options.minWidth = GetDoubleValue(minWidthDouble, 0.0);
    options.skipFasteners = GetLogicalValue(skipTallCylinderToggle, false);
    options.filterLayerRange = GetLogicalValue(layerRangeToggle, true);
    options.startLayer = ClampLayer(GetIntegerValue(startLayerInteger, 1));
    options.endLayer = ClampLayer(GetIntegerValue(endLayerInteger, 99));
    if (options.startLayer > options.endLayer)
    {
        std::swap(options.startLayer, options.endLayer);
    }
    options.largestBodyOnlyPerLayer = GetLogicalValue(largestBodyOnlyToggle, false);
    if (failureActionEnum != NULL)
    {
        std::string failureActionText = ToUtf8(failureActionEnum->ValueAsString());
        if (ContainsTextNoCase(failureActionText, "Red") || failureActionText.find("\xE7\xBA\xA2") != std::string::npos)
        {
            options.failureAction = 2;
        }
        else if (ContainsTextNoCase(failureActionText, "Highlight") || failureActionText.find("\xE9\xAB\x98\xE4\xBA\xAE") != std::string::npos)
        {
            options.failureAction = 3;
        }
        else
        {
            options.failureAction = 1;
        }
    }
    return options;
}

void PiLianZuanBanJinDialog::Run(const AutoConvertOptions& options)
{
    UfGuard ufGuard;
    Session* session = Session::GetSession();
    Part* workPart = session->Parts()->Work();
    if (workPart == NULL)
    {
        throw std::runtime_error("请先打开一个工作部件。");
    }

    RuleConfig ruleConfig = LoadRuleConfig();
    std::vector<Part*> partsToProcess;
    std::set<tag_t> seenParts;

    bool assemblyMode = false;
    if (assemblySelectionActive)
    {
        assemblyMode = true;
        for (size_t i = 0; i < selectedAssemblyParts.size(); ++i)
        {
            Part* selectedPart = selectedAssemblyParts[i];
            if (selectedPart != NULL && seenParts.insert(selectedPart->Tag()).second)
            {
                partsToProcess.push_back(selectedPart);
            }
        }
        if (partsToProcess.empty())
        {
            return;
        }
    }

    if (!assemblyMode)
    {
        partsToProcess.push_back(workPart);
        seenParts.insert(workPart->Tag());
    }

    std::vector<BodyResult> results;
    int attributeSkippedCount = 0;

    for (size_t partIndex = 0; partIndex < partsToProcess.size(); ++partIndex)
    {
        Part* processPart = partsToProcess[partIndex];
        if (processPart == NULL)
        {
            continue;
        }

        try
        {
            session->Parts()->SetWork(processPart);
        }
        catch (...)
        {
        }

        SheetmetalManager* manager = processPart->Features()->SheetmetalManager();
        const bool partAttributesOk = PartHasBatchAttributes(processPart);
        const std::string processPartName = PartName(processPart);
        std::vector<Body*> candidates;
        std::map<int, Body*> largestByLayer;
        std::map<int, double> largestScoreByLayer;
        for (BodyCollection::iterator it = processPart->Bodies()->begin(); it != processPart->Bodies()->end(); ++it)
        {
            Body* body = *it;
            if (body == NULL || !IsAlive(body->Tag()))
            {
                continue;
            }

            try
            {
                if (!body->IsSolidBody())
                {
                    continue;
                }
            }
            catch (...)
            {
                continue;
            }

            if (!partAttributesOk && !BodyHasBatchAttributes(body))
            {
                ++attributeSkippedCount;
                continue;
            }

            const int layer = body->Layer();
            if (options.filterLayerRange && (layer < options.startLayer || layer > options.endLayer))
            {
                continue;
            }

            BodyBox box = MeasureBodyBox(body);
            if (options.skipSmallBody && (box.length < options.minLength || box.width < options.minWidth))
            {
                continue;
            }

            if (options.skipFasteners)
            {
                FastenerFilterInfo fastenerInfo;
                const bool skipFastener = ShouldSkipFastenerBody(body, &fastenerInfo);
                if (skipFastener)
                {
                    continue;
                }
            }
            if (options.largestBodyOnlyPerLayer)
            {
                if (largestByLayer.find(layer) == largestByLayer.end() || box.score > largestScoreByLayer[layer])
                {
                    largestByLayer[layer] = body;
                    largestScoreByLayer[layer] = box.score;
                }
            }
            else
            {
                candidates.push_back(body);
            }
        }

        if (options.largestBodyOnlyPerLayer)
        {
            for (std::map<int, Body*>::iterator it = largestByLayer.begin(); it != largestByLayer.end(); ++it)
            {
                candidates.push_back(it->second);
            }
        }

        const size_t resultStart = results.size();
        for (size_t i = 0; i < candidates.size(); ++i)
        {
            Body* body = candidates[i];
            BodyResult result;
            Body* displayResultBody = body;
            result.layer = body == NULL ? 0 : body->Layer();
            result.name = processPartName.empty() ? BodyName(body) : processPartName + " / " + BodyName(body);
            const std::string bodyMaterial = ReadBodyMaterialText(processPart, body);

            try
            {
                FaceInfo fixedFaceInfo = SelectConvertBaseFace(body, options);
                if (fixedFaceInfo.face == NULL)
                {
                    result.error = "未找到可用固定面";
                    results.push_back(result);
                    continue;
                }

                Face* fixedFace = fixedFaceInfo.face;
                Body* activeBody = body;
                Feature* convertFeature = NULL;
                const bool alreadySheetmetal = IsSheetmetalBody(processPart, manager, body);
                if (!alreadySheetmetal)
                {
                    ApplySheetmetalPreferencesBeforeConvert(processPart, options);
                    ConvertToSheetmetalBuilder* convertBuilder = manager->CreateConvertToSheetmetalFeatureBuilder(NULL);
                    convertBuilder->SetApplicationContext(ApplicationContextNxSheetMetal);
                    convertBuilder->SetBendReliefType(ConvertToSheetmetalBuilder::BendReliefTypeOptionsSquare);
                    convertBuilder->BendReliefDepth()->SetRightHandSide(FormatDouble(options.reliefDepth, 6));
                    convertBuilder->BendReliefWidth()->SetRightHandSide(FormatDouble(options.reliefWidth, 6));
                    convertBuilder->SetMaintainZeroBendRadius(options.innerRadius <= 0.0);
                    convertBuilder->SetBaseFace(NULL);
                    convertBuilder->SetBaseFace(fixedFace);
                    std::vector<Edge*> ripEdges;
                    convertBuilder->SetRipEdges(ripEdges);
                    convertFeature = convertBuilder->CommitFeature();
                    convertBuilder->Destroy();
                    session->CleanUpFacetedFacesAndEdges();
                    activeBody = ResolveFeatureBody(convertFeature, body);
                    result.convertOk = convertFeature != NULL;
                }
                else
                {
                    result.convertOk = true;
                }
                displayResultBody = activeBody != NULL ? activeBody : body;

                UpdateBodyZThicknessAttribute(manager, activeBody);
                result.neutralFaceCount = ApplyNeutralFactorByRules(processPart, manager, activeBody, ruleConfig, bodyMaterial);

                Feature* existingFlatPattern = FindExistingFlatPatternFeature(processPart, manager, activeBody);
                FaceInfo postConvertFaceInfo = SelectSheetmetalBaseFaceByBendDirection(manager, activeBody, options);
                Face* flatFace = postConvertFaceInfo.face == NULL ? fixedFace : postConvertFaceInfo.face;
                if (options.applyFixedFaceColor)
                {
                    ApplyFaceColor(session, flatFace, options.fixedFaceColor);
                }
                Point3d startPoint;
                Point3d endPoint;
                Edge* xAxisEdge = FindXAxisEdgeForSingleBend(manager, activeBody, flatFace, &startPoint, &endPoint);
                if (xAxisEdge == NULL)
                {
                    xAxisEdge = FindLongestStraightEdge(flatFace, &startPoint, &endPoint);
                }
                const std::set<tag_t> beforeFlatBodyTags = CollectPartSolidBodyTags(processPart);
                Feature* flatFeature = CommitFlatPattern(processPart, existingFlatPattern, flatFace, xAxisEdge, endPoint);
                result.flatOk = flatFeature != NULL;
                if (!result.flatOk)
                {
                    result.error = existingFlatPattern == NULL ? "?????????" : "???????????";
                }
                else
                {
                    FlatSolidThicknessCheckInfo flatCheck;
                    const bool flatThicknessOk = ValidateFlatSolidThickness(processPart, manager, activeBody, flatFeature, beforeFlatBodyTags, &flatCheck);
                    if (!flatThicknessOk)
                    {
                        displayResultBody = body;
                        result.convertOk = false;
                        result.flatOk = false;
                        std::ostringstream error;
                        error << "SB Flat Solid thickness validation failed";
                        if (flatCheck.expectedThickness > 0.0 || flatCheck.measuredThickness > 0.0)
                        {
                            error << ", expected=" << FormatDouble(flatCheck.expectedThickness, 6)
                                  << ", measured=" << FormatDouble(flatCheck.measuredThickness, 6);
                        }
                        result.error = error.str();
                    }
                    else
                    {
                        UpdateBodyFlatPatternSizeAttribute(activeBody, flatCheck);
                        if (flatCheck.flatBody != NULL && (activeBody == NULL || flatCheck.flatBody->Tag() != activeBody->Tag()))
                        {
                            UpdateBodyFlatPatternSizeAttribute(flatCheck.flatBody, flatCheck);
                        }
                    }
                }
            }
            catch (const NXException& ex)
            {
                result.error = "NX执行失败: " + NormalizeUtf8Message(ex.Message());
            }
            catch (const std::exception& ex)
            {
                result.error = NormalizeUtf8Message(ex.what());
            }

            if (options.failureAction == 1 && result.error.empty())
            {
                HideDisplayBody(displayResultBody);
            }
            else if (options.failureAction == 2 && !result.error.empty())
            {
                ApplyBodyColor(session, displayResultBody, 186);
            }
            else if (options.failureAction == 3 && !result.error.empty())
            {
                HighlightDisplayBody(displayResultBody);
            }

            results.push_back(result);
        }

        if (options.autoSaveAfterRun && results.size() > resultStart)
        {
            try
            {
                processPart->Save(BasePart::SaveComponentsTrue, BasePart::CloseAfterSaveFalse);
            }
            catch (...)
            {
            }
        }
    }

    try
    {
        session->Parts()->SetWork(workPart);
    }
    catch (...)
    {
    }

    std::string message = BuildResultMessage(results);
    bool hasFailedResult = attributeSkippedCount > 0;
    for (size_t i = 0; i < results.size(); ++i)
    {
        if (!results[i].error.empty())
        {
            hasFailedResult = true;
            break;
        }
    }
    if (assemblyMode)
    {
        message += "\n扫描部件: " + std::to_string(partsToProcess.size()) + " 个";
    }
    if (attributeSkippedCount > 0)
    {
        message += "\n属性不完整跳过: " + std::to_string(attributeSkippedCount) + " 个实体";
    }


    if (hasFailedResult)
    {
        ShowMessage(theUI, "自动转钣金 C++", NXMessageBox::DialogTypeError, message);
    }
}

extern "C" DllExport void ufusr(char* param, int* retcod, int param_len)
{
    if (!zhihui_license_guard::EnsureAuthorized(L"ZHIHUI.PILIANZUANBANJIN", L"PiLianZuanBanJin"))
    {
        if (retcod != NULL)
        {
            *retcod = 1;
        }
        return;
    }

    PiLianZuanBanJinDialog* dialog = NULL;
    try
    {
        dialog = new PiLianZuanBanJinDialog();
        dialog->Show();
    }
    catch (const std::exception& ex)
    {
        ShowMessage(UI::GetUI(), "自动转钣金 C++", NXMessageBox::DialogTypeError, ex.what());
    }

    if (dialog != NULL)
    {
        delete dialog;
        dialog = NULL;
    }
}

extern "C" DllExport int ufusr_ask_unload()
{
    return UF_UNLOAD_IMMEDIATELY;
}




