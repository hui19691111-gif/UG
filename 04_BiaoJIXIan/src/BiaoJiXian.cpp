#include <cfloat>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>
#include <windows.h>

#ifdef CreateDialog
#undef CreateDialog
#endif

#include <uf.h>
#include <uf_curve.h>
#include <uf_eval.h>
#include <uf_group.h>
#include <uf_modl.h>
#include <uf_modl_curves.h>
#include <uf_modl_types.h>
#include <uf_modl_utilities.h>
#include <uf_modl_sweep.h>
#include <uf_obj.h>
#include <uf_obj_types.h>
#include <uf_part.h>
#include <uf_view.h>

#include <NXOpen/BlockStyler_BlockDialog.hxx>
#include <NXOpen/BlockStyler_BodyCollector.hxx>
#include <NXOpen/BlockStyler_CompositeBlock.hxx>
#include <NXOpen/BlockStyler_Enumeration.hxx>
#include <NXOpen/BlockStyler_SelectObject.hxx>
#include <NXOpen/BlockStyler_UIBlock.hxx>
#include <NXOpen/Body.hxx>
#include <NXOpen/Edge.hxx>
#include <NXOpen/BodyDumbRule.hxx>
#include <NXOpen/Curve.hxx>
#include <NXOpen/Direction.hxx>
#include <NXOpen/DirectionCollection.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/FaceTangentRule.hxx>
#include <NXOpen/Features_CurveFeatureCollection.hxx>
#include <NXOpen/Features_Feature.hxx>
#include <NXOpen/Features_FeatureCollection.hxx>
#include <NXOpen/Features_ShadowCurve.hxx>
#include <NXOpen/Features_ShadowCurveBuilder.hxx>
#include <NXOpen/ListingWindow.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/NXString.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/ScCollector.hxx>
#include <NXOpen/ScCollectorCollection.hxx>
#include <NXOpen/ScRuleFactory.hxx>
#include <NXOpen/SelectionIntentRule.hxx>
#include <NXOpen/SelectionIntentRuleOptions.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/SmartObject.hxx>
#include <NXOpen/TaggedObject.hxx>
#include <NXOpen/UI.hxx>

using namespace NXOpen;

namespace
{
// 瀵硅瘽妗嗘枃浠跺悕鍜屾帶浠?ID锛岄渶涓?DLX 鏂囦欢淇濇寔涓€鑷淬€?
constexpr const char* kDlxFileName = "BiaoJiXian.dlx";
constexpr const char* kSelectBodyBlockId = "SourceBody";
constexpr const char* kSelectEdgeBlockId = "SourceEdges";
constexpr const char* kModeBlockId = "MarkLineMode";

// 浣撲笌浣撱€侀潰涓庨潰鍒ゅ畾鈥滅浉璐粹€濇椂浣跨敤鐨勮窛绂诲宸€?
constexpr double kContactTolerance = 0.01;

// 姝ｅ父鎵ц鏃跺叧闂俊鎭獥鍙ｆ棩蹇楋紝浠呭湪璋冭瘯鎴栧紓甯告椂杈撳嚭銆?
constexpr bool kEnableInfoLog = false;

// UF 鍒楄〃鍙ユ焺鐨勮嚜鍔ㄩ噴鏀惧寘瑁咃紝閬垮厤寮傚父鏃舵硠婕忋€?
struct UfListHandle
{
    uf_list_p_t list = nullptr;

    ~UfListHandle()
    {
        if (list != nullptr)
        {
            UF_MODL_delete_list(&list);
        }
    }
};

// UF 闈㈠惊鐜摼琛ㄧ殑鑷姩閲婃斁鍖呰銆?
struct UfLoopListHandle
{
    uf_loop_p_t list = nullptr;

    ~UfLoopListHandle()
    {
        if (list != nullptr)
        {
            UF_MODL_delete_loop_list(&list);
        }
    }
};

// 淇濆瓨涓€缁勬渶浼樻帴瑙﹀叧绯伙細
// 婧愪綋鐨勬帴瑙﹂潰銆佺洰鏍囧ぇ浣撶殑鎺ヨЕ闈紝浠ュ強涓よ€呮渶灏忚窛绂汇€?
struct ContactMatch
{
    tag_t targetBody = NULL_TAG;
    tag_t sourceFace = NULL_TAG;
    tag_t targetFace = NULL_TAG;
    double distance = DBL_MAX;
    double sourcePoint[3] = {0.0, 0.0, 0.0};
    double targetPoint[3] = {0.0, 0.0, 0.0};
};

// 淇濆瓨涓棿鏇茬嚎锛?
// helperCurves 涓鸿竟杞洸绾垮悗鐨勮緟鍔╂洸绾匡紝
// projectedCurves 涓烘姇褰卞埌鐩爣闈㈠悗鐨勬洸绾裤€?
struct ProjectionCurveData
{
    std::vector<tag_t> helperCurves;
    std::vector<tag_t> projectedCurves;
};

// 淇濆瓨涓€涓棴鐜強鍏剁敤浜庢瘮杈冨ぇ灏忕殑璇勫垎銆?
struct OutlineLoopData
{
    std::vector<tag_t> curves;
    double areaScore = 0.0;
};

// 淇濆瓨鏇茬嚎棣栧熬鐐癸紝鐢ㄤ簬鍚庣画鎸夎繛鎺ュ叧绯绘媶鍒嗛棴鐜€?
struct CurveEndData
{
    double start[3] = {0.0, 0.0, 0.0};
    double end[3] = {0.0, 0.0, 0.0};
};

// 搴曢潰妯″紡鍜岃疆寤撴ā寮忔渶缁堥兘浼氳惤鍒板悓涓€濂椻€滄媺浼稿伐鍏蜂綋 -> 姹傚樊鈥濈殑寮€妲芥祦绋嬨€?
void CreateBottomGrooveCut_Test12(const ProjectionCurveData& projectionData, const ContactMatch& match);

std::string WideToLocal(const std::wstring& text)
{
    if (text.empty())
    {
        return std::string();
    }

    const int localLength = WideCharToMultiByte(CP_ACP, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (localLength <= 0)
    {
        return std::string();
    }

    std::string localText(static_cast<size_t>(localLength), '\0');
    WideCharToMultiByte(CP_ACP, 0, text.c_str(), -1, &localText[0], localLength, nullptr, nullptr);
    localText.resize(strlen(localText.c_str()));
    return localText;
}

std::wstring Utf8ToWide(const std::string& text)
{
    if (text.empty())
    {
        return std::wstring();
    }

    const int wideLength = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    if (wideLength <= 0)
    {
        return std::wstring(text.begin(), text.end());
    }

    std::wstring wideText(static_cast<size_t>(wideLength), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, &wideText[0], wideLength);
    wideText.resize(wcslen(wideText.c_str()));
    return wideText;
}

ListingWindow* GetListingWindow()
{
    ListingWindow* listingWindow = Session::GetSession()->ListingWindow();
    if (!listingWindow->IsOpen())
    {
        listingWindow->Open();
    }
    return listingWindow;
}

// 淇℃伅杈撳嚭缁熶竴鍏ュ彛銆俧orce=true 鏃跺嵆浣垮叧闂櫘閫氭棩蹇椾篃浼氬己鍒惰緭鍑恒€?
void Log(const std::wstring& text, bool force = false)
{
    if (!force && !kEnableInfoLog)
    {
        return;
    }
    GetListingWindow()->WriteLine(WideToLocal(text).c_str());
}

// 鑾峰彇褰撳墠 DLL 鎵€鍦ㄧ洰褰曪紝鐢ㄦ潵瀹氫綅鍚岀洰褰曚笅鐨?DLX 鏂囦欢銆?
std::string GetCurrentDllDirectory()
{
    HMODULE currentModule = nullptr;
    if (!GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(&GetCurrentDllDirectory),
            &currentModule))
    {
        throw std::runtime_error("鏃犳硶鑾峰彇褰撳墠 DLL 妯″潡鍙ユ焺銆?);
    }

    char fullPath[MAX_PATH] = {0};
    const DWORD length = GetModuleFileNameA(currentModule, fullPath, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
    {
        throw std::runtime_error("鏃犳硶璇诲彇褰撳墠 DLL 璺緞銆?);
    }

    std::string path(fullPath);
    const size_t pos = path.find_last_of("\\/");
    if (pos == std::string::npos)
    {
        return ".";
    }

    return path.substr(0, pos);
}

// UFUN 璋冪敤妫€鏌ワ細澶辫触鏃剁粺涓€鎶涘紓甯革紝渚夸簬涓婂眰闆嗕腑澶勭悊銆?
void UfCall(int errorCode, const char* apiName)
{
    if (errorCode == 0)
    {
        return;
    }

    char errorMessage[133] = {0};
    UF_get_fail_message(errorCode, errorMessage);
    throw std::runtime_error(std::string(apiName) + " failed: " + errorMessage);
}

// 鎵归噺鍒犻櫎瀵硅薄锛岃嚜鍔ㄥ拷鐣ョ┖ tag銆?
void DeleteObjects(const std::vector<tag_t>& objectTags)
{
    if (objectTags.empty())
    {
        return;
    }

    std::vector<tag_t> validTags;
    validTags.reserve(objectTags.size());
    for (tag_t objectTag : objectTags)
    {
        if (objectTag != NULL_TAG)
        {
            validTags.push_back(objectTag);
        }
    }

    if (!validTags.empty())
    {
        UF_OBJ_delete_array_of_objects(static_cast<int>(validTags.size()), validTags.data(), nullptr);
    }
}

// 鍘婚櫎瀵硅薄鍙傛暟锛屾妸鍏宠仈鐗瑰緛杞垚鏅€氬嚑浣曪紝渚夸簬鍚庣画姹傚樊鍜屽垹闄ゃ€?
void RemoveObjectParameters(const std::vector<tag_t>& objectTags)
{
    if (objectTags.empty())
    {
        return;
    }

    UfListHandle objectList;
    UfCall(UF_MODL_create_list(&objectList.list), "UF_MODL_create_list");
    for (tag_t objectTag : objectTags)
    {
        if (objectTag != NULL_TAG)
        {
            UfCall(UF_MODL_put_list_item(objectList.list, objectTag), "UF_MODL_put_list_item");
        }
    }

    UfCall(UF_MODL_delete_object_parms(objectList.list), "UF_MODL_delete_object_parms");
}

// UF 閾捐〃杞?vector锛屾柟渚垮悗缁敤鏍囧噯瀹瑰櫒澶勭悊銆?
std::vector<tag_t> ListToVector(uf_list_p_t list)
{
    std::vector<tag_t> tags;
    if (list == nullptr)
    {
        return tags;
    }

    int count = 0;
    UfCall(UF_MODL_ask_list_count(list, &count), "UF_MODL_ask_list_count");
    tags.reserve(static_cast<size_t>(count));

    for (int index = 0; index < count; ++index)
    {
        tag_t objectTag = NULL_TAG;
        UfCall(UF_MODL_ask_list_item(list, index, &objectTag), "UF_MODL_ask_list_item");
        if (objectTag != NULL_TAG)
        {
            tags.push_back(objectTag);
        }
    }

    return tags;
}

double Dot(const double lhs[3], const double rhs[3])
{
    return lhs[0] * rhs[0] + lhs[1] * rhs[1] + lhs[2] * rhs[2];
}

double DistanceSquared(const double lhs[3], const double rhs[3])
{
    const double dx = lhs[0] - rhs[0];
    const double dy = lhs[1] - rhs[1];
    const double dz = lhs[2] - rhs[2];
    return dx * dx + dy * dy + dz * dz;
}

// 鍚戦噺鍗曚綅鍖栵紝闀垮害杩囧皬鏃惰繑鍥?false銆?
bool Normalize(double vector[3])
{
    const double length = std::sqrt(Dot(vector, vector));
    if (length <= 1.0e-12)
    {
        return false;
    }

    vector[0] /= length;
    vector[1] /= length;
    vector[2] /= length;
    return true;
}

// 閫氳繃鍖呭洿鐩掍綋绉揩閫熷垽鏂摢涓綋鈥滄洿澶р€濄€?
double AskBoundingBoxVolume(tag_t bodyTag)
{
    double box[6] = {0.0};
    UfCall(UF_MODL_ask_bounding_box(bodyTag, box), "UF_MODL_ask_bounding_box");
    return (box[3] - box[0]) * (box[4] - box[1]) * (box[5] - box[2]);
}

// 鏋氫妇褰撳墠鏄剧ず閮ㄤ欢涓殑鎵€鏈夊疄浣撲綋銆?
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

// 褰撳墠绋嬪簭浠呭鐞嗗钩闈㈢浉璐达紝鍥犳杩欓噷鍙彁鍙栧钩闈㈤潰銆?
std::vector<tag_t> AskPlanarFaces(tag_t bodyTag)
{
    UfListHandle faceList;
    UfCall(UF_MODL_ask_body_faces(bodyTag, &faceList.list), "UF_MODL_ask_body_faces");

    std::vector<tag_t> planarFaces;
    for (tag_t faceTag : ListToVector(faceList.list))
    {
        int faceType = 0;
        UfCall(UF_MODL_ask_face_type(faceTag, &faceType), "UF_MODL_ask_face_type");
        if (faceType == UF_MODL_PLANAR_FACE)
        {
            planarFaces.push_back(faceTag);
        }
    }

    return planarFaces;
}

// 璇诲彇骞抽潰闈㈢殑娉曞悜锛屼緵鎺ヨЕ鍒ゆ柇鍜屾媺浼告柟鍚戝鐢ㄣ€?
bool AskFaceNormal(tag_t faceTag, double normal[3])
{
    int faceType = 0;
    double point[3] = {0.0};
    double direction[3] = {0.0};
    double box[6] = {0.0};
    double radius = 0.0;
    double radData = 0.0;
    int normalDir = 0;

    if (UF_MODL_ask_face_data(faceTag, &faceType, point, direction, box, &radius, &radData, &normalDir) != 0)
    {
        return false;
    }

    if (faceType != UF_MODL_PLANAR_FACE)
    {
        return false;
    }

    normal[0] = direction[0];
    normal[1] = direction[1];
    normal[2] = direction[2];
    return Normalize(normal);
}

// 鏌ユ壘涓庢簮浣撶浉璐淬€佷笖姣旀簮浣撴洿澶х殑鐩爣浣撳強鍏舵帴瑙﹂潰銆?
// 閫昏緫锛氬厛绛涘ぇ浣擄紝鍐嶅仛浣?浣撴渶灏忚窛绂伙紝鍐嶅仛闈?闈㈡渶灏忚窛绂诲拰娉曞悜鍙嶅悜鍒ゆ柇銆?
ContactMatch FindBestBottomContact(tag_t sourceBodyTag)
{
    ContactMatch bestMatch;
    const double sourceVolume = AskBoundingBoxVolume(sourceBodyTag);
    const std::vector<tag_t> sourceFaces = AskPlanarFaces(sourceBodyTag);

    for (tag_t candidateBodyTag : AskSolidBodiesInDisplayPart())
    {
        if (candidateBodyTag == sourceBodyTag)
        {
            continue;
        }

        if (AskBoundingBoxVolume(candidateBodyTag) <= sourceVolume)
        {
            continue;
        }

        double bodyDistance = 0.0;
        double sourcePoint[3] = {0.0};
        double targetPoint[3] = {0.0};
        if (UF_MODL_ask_minimum_dist(sourceBodyTag, candidateBodyTag, 0, nullptr, 0, nullptr, &bodyDistance, sourcePoint, targetPoint) != 0)
        {
            continue;
        }

        if (bodyDistance > kContactTolerance)
        {
            continue;
        }

        const std::vector<tag_t> targetFaces = AskPlanarFaces(candidateBodyTag);
        for (tag_t sourceFaceTag : sourceFaces)
        {
            double sourceNormal[3] = {0.0};
            if (!AskFaceNormal(sourceFaceTag, sourceNormal))
            {
                continue;
            }

            for (tag_t targetFaceTag : targetFaces)
            {
                double targetNormal[3] = {0.0};
                if (!AskFaceNormal(targetFaceTag, targetNormal))
                {
                    continue;
                }

                if (Dot(sourceNormal, targetNormal) > -0.95)
                {
                    continue;
                }

                double faceDistance = 0.0;
                double facePoint1[3] = {0.0};
                double facePoint2[3] = {0.0};
                if (UF_MODL_ask_minimum_dist(sourceFaceTag, targetFaceTag, 0, nullptr, 0, nullptr, &faceDistance, facePoint1, facePoint2) != 0)
                {
                    continue;
                }

                if (faceDistance <= kContactTolerance && faceDistance < bestMatch.distance)
                {
                    bestMatch.targetBody = candidateBodyTag;
                    bestMatch.sourceFace = sourceFaceTag;
                    bestMatch.targetFace = targetFaceTag;
                    bestMatch.distance = faceDistance;
                    for (int axis = 0; axis < 3; ++axis)
                    {
                        bestMatch.sourcePoint[axis] = facePoint1[axis];
                        bestMatch.targetPoint[axis] = facePoint2[axis];
                    }
                }
            }
        }
    }

    return bestMatch;
}

// 鍙栨帴瑙﹂潰鐨勫鐜竟锛屼綔涓哄簳闈㈡ā寮忕殑鍘熷杞粨銆?
std::vector<tag_t> AskPeripheralLoopEdges(tag_t faceTag)
{
    UfLoopListHandle loopList;
    UfCall(UF_MODL_ask_face_loops(faceTag, &loopList.list), "UF_MODL_ask_face_loops");

    for (uf_loop_p_t loop = loopList.list; loop != nullptr; loop = loop->next)
    {
        if (loop->type == 1 && loop->edge_list != nullptr)
        {
            return ListToVector(loop->edge_list);
        }
    }

    return std::vector<tag_t>();
}

// 鐢ㄦ洸绾垮寘鍥寸洅鐨勪袱涓富灏哄杩戜技璇勪环闂幆澶у皬锛岀敤浜庢寫鈥滄渶澶ц疆寤撯€濄€?
double AskCurveLoopAreaScore(const std::vector<tag_t>& curveTags)
{
    if (curveTags.empty())
    {
        return 0.0;
    }

    double minCorner[3] = {DBL_MAX, DBL_MAX, DBL_MAX};
    double maxCorner[3] = {-DBL_MAX, -DBL_MAX, -DBL_MAX};

    for (tag_t curveTag : curveTags)
    {
        double box[6] = {0.0};
        UfCall(UF_MODL_ask_bounding_box(curveTag, box), "UF_MODL_ask_bounding_box");
        for (int axis = 0; axis < 3; ++axis)
        {
            if (box[axis] < minCorner[axis])
            {
                minCorner[axis] = box[axis];
            }
            if (box[axis + 3] > maxCorner[axis])
            {
                maxCorner[axis] = box[axis + 3];
            }
        }
    }

    double lengths[3] = {
        maxCorner[0] - minCorner[0],
        maxCorner[1] - minCorner[1],
        maxCorner[2] - minCorner[2]};

    if (lengths[0] < lengths[1])
    {
        std::swap(lengths[0], lengths[1]);
    }
    if (lengths[1] < lengths[2])
    {
        std::swap(lengths[1], lengths[2]);
    }
    if (lengths[0] < lengths[1])
    {
        std::swap(lengths[0], lengths[1]);
    }

    return lengths[0] * lengths[1];
}

// 鏃х殑绮剧‘杞粨鏂规锛屽綋鍓嶄富娴佺▼宸叉敼鐢?ShadowCurveBuilder銆?
std::vector<tag_t> CreateLargestPreciseOutlineCurves(tag_t sourceBodyTag)
{
    tag_t workView = NULL_TAG;
    UfCall(UF_VIEW_ask_work_view(&workView), "UF_VIEW_ask_work_view");
    if (workView == NULL_TAG)
    {
        throw std::runtime_error("鏈兘鑾峰彇褰撳墠宸ヤ綔瑙嗗浘銆?);
    }

    tag_t bodyArray[1] = {sourceBodyTag};
    int loopCount = 0;
    int* countArray = nullptr;
    tag_t** curveArray = nullptr;
    double tolerance[2] = {0.01, 5.0};

    UfCall(
        UF_CURVE_create_precise_outline(
            1,
            bodyArray,
            workView,
            &loopCount,
            &countArray,
            &curveArray,
            tolerance),
        "UF_CURVE_create_precise_outline");

    if (loopCount <= 0 || countArray == nullptr || curveArray == nullptr)
    {
        throw std::runtime_error("鏈敓鎴愬彲鐢ㄧ殑杞粨鏇茬嚎銆?);
    }

    std::vector<OutlineLoopData> loops;
    loops.reserve(static_cast<size_t>(loopCount));
    for (int loopIndex = 0; loopIndex < loopCount; ++loopIndex)
    {
        OutlineLoopData loopData;
        for (int curveIndex = 0; curveIndex < countArray[loopIndex]; ++curveIndex)
        {
            if (curveArray[loopIndex][curveIndex] != NULL_TAG)
            {
                loopData.curves.push_back(curveArray[loopIndex][curveIndex]);
            }
        }

        loopData.areaScore = AskCurveLoopAreaScore(loopData.curves);
        if (!loopData.curves.empty())
        {
            loops.push_back(loopData);
        }
    }

    for (int loopIndex = 0; loopIndex < loopCount; ++loopIndex)
    {
        UF_free(curveArray[loopIndex]);
    }
    UF_free(curveArray);
    UF_free(countArray);

    if (loops.empty())
    {
        throw std::runtime_error("杞粨鍛戒护鏈繑鍥炴湁鏁堥棴鐜€?);
    }

    size_t bestIndex = 0;
    for (size_t index = 1; index < loops.size(); ++index)
    {
        if (loops[index].areaScore > loops[bestIndex].areaScore)
        {
            bestIndex = index;
        }
    }

    std::vector<tag_t> deleteCurves;
    for (size_t index = 0; index < loops.size(); ++index)
    {
        if (index == bestIndex)
        {
            continue;
        }
        deleteCurves.insert(deleteCurves.end(), loops[index].curves.begin(), loops[index].curves.end());
    }
    DeleteObjects(deleteCurves);

    Log(std::wstring(L"杞粨妯″紡鐢熸垚闂幆鏁伴噺锛?) + std::to_wstring(loops.size()));
    Log(std::wstring(L"杞粨妯″紡淇濈暀鏈€澶ц疆寤撴洸绾挎暟閲忥細") + std::to_wstring(loops[bestIndex].curves.size()));
    return loops[bestIndex].curves;
}

// 璇诲彇鏇茬嚎棣栧熬鐐癸紝鐢ㄤ簬鍒ゆ柇澶氭潯鏇茬嚎鏄惁灞炰簬鍚屼竴涓棴鐜€?
CurveEndData AskCurveEndData(tag_t curveTag)
{
    UF_EVAL_p_t evaluator = nullptr;
    UfCall(UF_EVAL_initialize(curveTag, &evaluator), "UF_EVAL_initialize");

    double limits[2] = {0.0, 0.0};
    UfCall(UF_EVAL_ask_limits(evaluator, limits), "UF_EVAL_ask_limits");

    CurveEndData endData;
    double derivatives[9] = {0.0};
    UfCall(UF_EVAL_evaluate(evaluator, 0, limits[0], endData.start, derivatives), "UF_EVAL_evaluate");
    UfCall(UF_EVAL_evaluate(evaluator, 0, limits[1], endData.end, derivatives), "UF_EVAL_evaluate");
    UfCall(UF_EVAL_free(evaluator), "UF_EVAL_free");
    return endData;
}

// 鎸夋洸绾跨鐐硅繛鎺ュ叧绯伙紝鎶婁竴缁勬洸绾挎媶鍒嗘垚澶氫釜闂幆銆?
std::vector<OutlineLoopData> SplitCurvesIntoConnectedLoops(const std::vector<tag_t>& curveTags)
{
    std::vector<OutlineLoopData> loops;
    if (curveTags.empty())
    {
        return loops;
    }

    std::vector<CurveEndData> endDataList;
    endDataList.reserve(curveTags.size());
    for (tag_t curveTag : curveTags)
    {
        endDataList.push_back(AskCurveEndData(curveTag));
    }

    const double toleranceSquared = 1.0e-6;
    std::vector<bool> visited(curveTags.size(), false);

    for (size_t seedIndex = 0; seedIndex < curveTags.size(); ++seedIndex)
    {
        if (visited[seedIndex])
        {
            continue;
        }

        OutlineLoopData loopData;
        std::vector<size_t> stack(1, seedIndex);
        visited[seedIndex] = true;

        while (!stack.empty())
        {
            const size_t currentIndex = stack.back();
            stack.pop_back();
            loopData.curves.push_back(curveTags[currentIndex]);

            const CurveEndData& current = endDataList[currentIndex];
            for (size_t otherIndex = 0; otherIndex < curveTags.size(); ++otherIndex)
            {
                if (visited[otherIndex])
                {
                    continue;
                }

                const CurveEndData& other = endDataList[otherIndex];
                const bool connected =
                    DistanceSquared(current.start, other.start) <= toleranceSquared ||
                    DistanceSquared(current.start, other.end) <= toleranceSquared ||
                    DistanceSquared(current.end, other.start) <= toleranceSquared ||
                    DistanceSquared(current.end, other.end) <= toleranceSquared;

                if (connected)
                {
                    visited[otherIndex] = true;
                    stack.push_back(otherIndex);
                }
            }
        }

        loopData.areaScore = AskCurveLoopAreaScore(loopData.curves);
        loops.push_back(loopData);
    }

    return loops;
}

// 杞粨妯″紡鏍稿績锛?
// 鐢ㄦ帴瑙﹀ぇ闈㈢殑娉曞悜鐢熸垚闃村奖鏇茬嚎锛屽啀绛涘嚭鏈€澶у杞粨銆?
std::vector<tag_t> CreateLargestShadowOutlineCurves(tag_t sourceBodyTag)
{
    const ContactMatch match = FindBestBottomContact(sourceBodyTag);
    if (match.targetBody == NULL_TAG || match.sourceFace == NULL_TAG || match.targetFace == NULL_TAG)
    {
        throw std::runtime_error("鏈壘鍒扮敤浜庤疆寤撴ā寮忕殑鐩歌创鐩爣浣撳拰鎺ヨЕ闈€?);
    }

    Body* sourceBody = dynamic_cast<Body*>(NXObjectManager::Get(sourceBodyTag));
    Face* targetFace = dynamic_cast<Face*>(NXObjectManager::Get(match.targetFace));
    if (sourceBody == nullptr || targetFace == nullptr)
    {
        throw std::runtime_error("鏃犳硶鑾峰彇杞粨妯″紡鎵€闇€鐨勪綋鎴栭潰瀵硅薄銆?);
    }

    double normal[3] = {0.0, 0.0, 0.0};
    if (!AskFaceNormal(match.sourceFace, normal))
    {
        throw std::runtime_error("鏃犳硶纭畾杞粨妯″紡鐨勬姇褰辨柟鍚戙€?);
    }

    Part* workPart = Session::GetSession()->Parts()->Work();
    if (workPart == nullptr)
    {
        throw std::runtime_error("鏃犳硶鑾峰彇褰撳墠宸ヤ綔閮ㄤ欢銆?);
    }

    Direction* rayDirection = workPart->Directions()->CreateDirection(
        Point3d(match.targetPoint[0], match.targetPoint[1], match.targetPoint[2]),
        Vector3d(normal[0], normal[1], normal[2]),
        SmartObject::UpdateOptionWithinModeling);

    SelectionIntentRuleOptions* bodyRuleOptions = workPart->ScRuleFactory()->CreateRuleOptions();
    bodyRuleOptions->SetSelectedFromInactive(false);
    BodyDumbRule* bodyRule =
        workPart->ScRuleFactory()->CreateRuleBodyDumb(std::vector<Body*>{sourceBody}, true, bodyRuleOptions);
    delete bodyRuleOptions;

    SelectionIntentRuleOptions* faceRuleOptions = workPart->ScRuleFactory()->CreateRuleOptions();
    faceRuleOptions->SetSelectedFromInactive(false);
    FaceTangentRule* faceRule =
        workPart->ScRuleFactory()->CreateRuleFaceTangent(targetFace, std::vector<Face*>(), 0.5, faceRuleOptions);
    delete faceRuleOptions;

    Features::ShadowCurveBuilder* builder =
        workPart->Features()->CurveFeatureCollection()->CreateShadowCurveBuilder(nullptr);
    builder->SetCurveLocationType(Features::ShadowCurveBuilder::CurveLocationTypesShadowonFace);
    builder->SetRayDirection(rayDirection);
    builder->SetAccuracyType(Features::ShadowCurveBuilder::AccuracyTypesStandard);
    builder->CurveSettings()->CurveFitData()->SetTolerance(0.0001);
    builder->CurveSettings()->CurveFitData()->SetAngleTolerance(0.5);
    builder->MaskingCurves()->SetDistanceTolerance(0.0001);
    builder->MaskingCurves()->SetChainingTolerance(0.0001);
    builder->MaskingCurves()->SetAngleTolerance(0.5);
    builder->SetDistanceThreshold(0.001);
    builder->SetOptimizeCurveFlag(true);
    builder->MaskingBodies()->ReplaceRules(std::vector<SelectionIntentRule*>{bodyRule}, false);
    builder->CurveLocationFaces()->ReplaceRules(std::vector<SelectionIntentRule*>{faceRule}, false);

    Features::ShadowCurve* shadowFeature = dynamic_cast<Features::ShadowCurve*>(builder->Commit());
    if (shadowFeature == nullptr)
    {
        builder->Destroy();
        throw std::runtime_error("闃村奖鏇茬嚎鐗瑰緛鍒涘缓澶辫触銆?);
    }

    std::vector<tag_t> createdCurveTags;
    for (NXObject* entity : shadowFeature->GetEntities())
    {
        Curve* curve = dynamic_cast<Curve*>(entity);
        if (curve != nullptr && curve->Tag() != NULL_TAG)
        {
            createdCurveTags.push_back(curve->Tag());
        }
    }

    builder->Destroy();

    if (createdCurveTags.empty())
    {
        throw std::runtime_error("闃村奖鏇茬嚎鍛戒护鏈繑鍥炴湁鏁堟洸绾裤€?);
    }

    // 鍏堟妸闃村奖鏇茬嚎鍘诲弬鏁帮紝杞垚鏅€氭洸绾匡紝鍐嶅仛闂幆绛涢€夊拰鍒犻櫎鍐呯幆銆?
    // 灏嗛槾褰辨洸绾垮幓鍙傛暟锛岄伩鍏嶅悗缁粛淇濈暀鍜岄槾褰辩壒寰佺殑鍏宠仈銆?
    RemoveObjectParameters(createdCurveTags);

    std::vector<OutlineLoopData> loops = SplitCurvesIntoConnectedLoops(createdCurveTags);
    if (loops.empty())
    {
        throw std::runtime_error("闃村奖鏇茬嚎鏈舰鎴愭湁鏁堥棴鐜€?);
    }

    size_t bestIndex = 0;
    for (size_t index = 1; index < loops.size(); ++index)
    {
        if (loops[index].areaScore > loops[bestIndex].areaScore)
        {
            bestIndex = index;
        }
    }

    std::vector<tag_t> deleteCurves;
    for (size_t index = 0; index < loops.size(); ++index)
    {
        if (index == bestIndex)
        {
            continue;
        }
        deleteCurves.insert(deleteCurves.end(), loops[index].curves.begin(), loops[index].curves.end());
    }
    DeleteObjects(deleteCurves);

    Log(std::wstring(L"杞粨妯″紡鐩爣浣?Tag=") + std::to_wstring(match.targetBody));
    Log(std::wstring(L"杞粨妯″紡鎺ヨЕ闈?Tag=") + std::to_wstring(match.targetFace));
    Log(std::wstring(L"杞粨妯″紡鐢熸垚闂幆鏁伴噺锛?) + std::to_wstring(loops.size()));
    Log(std::wstring(L"杞粨妯″紡鍘诲弬鏁版洸绾挎暟閲忥細") + std::to_wstring(createdCurveTags.size()));
    Log(std::wstring(L"杞粨妯″紡鍒犻櫎鍐呯幆鏇茬嚎鏁伴噺锛?) + std::to_wstring(deleteCurves.size()));
    Log(std::wstring(L"杞粨妯″紡淇濈暀鏈€澶ц疆寤撴洸绾挎暟閲忥細") + std::to_wstring(loops[bestIndex].curves.size()));
    return loops[bestIndex].curves;
}

// 杞粨妯″紡鍏ュ彛锛氬厛鐢熸垚鏈€澶у杞粨锛屽啀澶嶇敤搴曢潰妯″紡鐨勫紑妲介€昏緫銆?
void CreateLargestOutlineGroove_Test12(tag_t sourceBodyTag)
{
    Log(L"寮€濮嬫墽琛岃疆寤撴ā寮?..");

    const ContactMatch match = FindBestBottomContact(sourceBodyTag);
    if (match.targetBody == NULL_TAG || match.sourceFace == NULL_TAG || match.targetFace == NULL_TAG)
    {
        throw std::runtime_error("鏈壘鍒扮敤浜庤疆寤撴ā寮忕殑鐩歌创鐩爣浣撳拰鎺ヨЕ闈€?);
    }

    ProjectionCurveData projectionData;
    projectionData.projectedCurves = CreateLargestShadowOutlineCurves(sourceBodyTag);
    if (projectionData.projectedCurves.empty())
    {
        throw std::runtime_error("杞粨妯″紡鏈敓鎴愬彲鐢ㄤ簬鎷変几鐨勬洸绾裤€?);
    }

    CreateBottomGrooveCut_Test12(projectionData, match);
}

// 搴曢潰妯″紡鍓嶅崐娈碉細澶栧洿杈?-> 杈呭姪鏇茬嚎 -> 鎶曞奖鏇茬嚎銆?
ProjectionCurveData CreateProjectedProfileCurves(const std::vector<tag_t>& profileEdges, tag_t targetFace)
{
    if (profileEdges.empty())
    {
        throw std::runtime_error("鐢ㄤ簬鎶曞奖鐨勫鍥磋疆寤撹竟涓虹┖銆?);
    }

    if (targetFace == NULL_TAG)
    {
        throw std::runtime_error("鐢ㄤ簬鎶曞奖鐨勭洰鏍囬潰涓虹┖銆?);
    }

    ProjectionCurveData projectionData;
    projectionData.helperCurves.reserve(profileEdges.size());
    for (tag_t edgeTag : profileEdges)
    {
        tag_t curveTag = NULL_TAG;
        UfCall(UF_MODL_create_curve_from_edge(edgeTag, &curveTag), "UF_MODL_create_curve_from_edge");
        if (curveTag != NULL_TAG)
        {
            projectionData.helperCurves.push_back(curveTag);
        }
    }

    if (projectionData.helperCurves.empty())
    {
        throw std::runtime_error("鏃犳硶浠庡鍥磋疆寤撹竟鐢熸垚鎶曞奖杈撳叆鏇茬嚎銆?);
    }

    UF_CURVE_proj1_t projData{};
    UfCall(UF_CURVE_init_proj_curves_data1(&projData), "UF_CURVE_init_proj_curves_data1");
    projData.proj_data.proj_type = 1;
    projData.join_type = UF_CURVE_NO_JOIN;

    tag_t targetFaces[1] = {targetFace};
    tag_t projectionGroup = NULL_TAG;
    UfCall(
        UF_CURVE_create_proj_curves1(
            static_cast<int>(projectionData.helperCurves.size()),
            projectionData.helperCurves.data(),
            1,
            targetFaces,
            1,
            &projData,
            &projectionGroup),
        "UF_CURVE_create_proj_curves1");

    tag_t* groupMembers = nullptr;
    int memberCount = 0;
    UfCall(UF_GROUP_ask_group_data(projectionGroup, &groupMembers, &memberCount), "UF_GROUP_ask_group_data");

    projectionData.projectedCurves.reserve(static_cast<size_t>(memberCount));
    for (int index = 0; index < memberCount; ++index)
    {
        if (groupMembers[index] != NULL_TAG)
        {
            projectionData.projectedCurves.push_back(groupMembers[index]);
        }
    }

    if (groupMembers != nullptr)
    {
        UF_free(groupMembers);
    }

    UfCall(UF_GROUP_ungroup_top(projectionGroup), "UF_GROUP_ungroup_top");

    if (projectionData.projectedCurves.empty())
    {
        throw std::runtime_error("鏈敓鎴愬彲鐢ㄤ簬鎷変几鐨勬姇褰辨洸绾裤€?);
    }

    Log(std::wstring(L"宸茬敓鎴愯緟鍔╂洸绾挎暟閲忥細") + std::to_wstring(projectionData.helperCurves.size()));
    Log(std::wstring(L"宸茬敓鎴愭姇褰辨洸绾挎暟閲忥細") + std::to_wstring(projectionData.projectedCurves.size()));
    return projectionData;
}

// 涓ょ妯″紡鍏辩敤鐨勫嚬妲藉垱寤烘祦绋嬶細
// 鎶曞奖鏇茬嚎 -> 鎷変几宸ュ叿浣?-> 鍘诲弬鏁?-> 姹傚樊 -> 鍒犻櫎涓棿瀵硅薄銆?
void CreateBottomGrooveCut_Test12(const ProjectionCurveData& projectionData, const ContactMatch& match)
{
    if (projectionData.projectedCurves.empty())
    {
        throw std::runtime_error("鐢ㄤ簬鍒涘缓鍑规Ы鐨勬姇褰辨洸绾夸负绌恒€?);
    }

    if (match.targetBody == NULL_TAG)
    {
        throw std::runtime_error("鏈壘鍒板嚬妲芥眰宸殑鐩爣浣撱€?);
    }

    double direction[3] = {0.0, 0.0, 0.0};
    if (!AskFaceNormal(match.sourceFace, direction))
    {
        throw std::runtime_error("鏃犳硶纭畾搴曢潰鎷変几鏂瑰悜銆?);
    }

    UfListHandle curveList;
    UfCall(UF_MODL_create_list(&curveList.list), "UF_MODL_create_list");
    for (tag_t curveTag : projectionData.projectedCurves)
    {
        UfCall(UF_MODL_put_list_item(curveList.list, curveTag), "UF_MODL_put_list_item");
    }

    char taperAngle[] = "0.0";
    char startLimit[] = "0.0";
    char endLimit[] = "0.001";
    char* limits[2] = {startLimit, endLimit};
    double point[3] = {0.0, 0.0, 0.0};

    UfListHandle extrudeFeatureList;
    UfCall(
        UF_MODL_create_extruded(
            curveList.list,
            taperAngle,
            limits,
            point,
            direction,
            UF_NULLSIGN,
            &extrudeFeatureList.list),
        "UF_MODL_create_extruded");

    const std::vector<tag_t> extrudeFeatures = ListToVector(extrudeFeatureList.list);
    if (extrudeFeatures.empty())
    {
        throw std::runtime_error("鏈兘鍒涘缓鐢ㄤ簬姹傚樊鐨勬媺浼镐綋鐗瑰緛銆?);
    }

    tag_t toolBody = NULL_TAG;
    UfCall(UF_MODL_ask_feat_body(extrudeFeatures.front(), &toolBody), "UF_MODL_ask_feat_body");
    if (toolBody == NULL_TAG)
    {
        throw std::runtime_error("鏈兘鑾峰彇鎷変几浣撳搴旂殑宸ュ叿浣撱€?);
    }

    UfListHandle bodyList;
    UfCall(UF_MODL_create_list(&bodyList.list), "UF_MODL_create_list");
    UfCall(UF_MODL_put_list_item(bodyList.list, toolBody), "UF_MODL_put_list_item");
    UfCall(UF_MODL_delete_body_parms(bodyList.list), "UF_MODL_delete_body_parms");

    // 鍘婚櫎鍙傛暟鍚庨噸鏂颁粠鍒楄〃閲屽彇涓€娆′綋鏍囩锛屽悗缁眰宸拰鍒犻櫎閮芥寜鏂版爣绛惧鐞嗐€?
    tag_t dumbToolBody = NULL_TAG;
    // 鍘诲弬鏁板悗宸ュ叿浣撶殑 tag 鍙兘鍙戠敓鍙樺寲锛屽洜姝よ閲嶆柊浠庡垪琛ㄤ腑鍙栦竴娆°€?
    UfCall(UF_MODL_ask_list_item(bodyList.list, 0, &dumbToolBody), "UF_MODL_ask_list_item");
    if (dumbToolBody == NULL_TAG)
    {
        throw std::runtime_error("鍘婚櫎鍙傛暟鍚庢湭鑳借幏鍙栨柊鐨勫伐鍏蜂綋鏍囩銆?);
    }

    int resultCount = 0;
    tag_t* resultingBodies = nullptr;
    UfCall(
        UF_MODL_subtract_bodies(match.targetBody, dumbToolBody, &resultCount, &resultingBodies),
        "UF_MODL_subtract_bodies");

    if (resultingBodies != nullptr)
    {
        UF_free(resultingBodies);
    }

    // 姹傚樊瀹屾垚鍚庡垹闄や腑闂存洸绾垮拰宸ュ叿浣擄紝鎭㈠姝ｅ紡娴佺▼銆?
    UfCall(UF_OBJ_set_color(dumbToolBody, 6), "UF_OBJ_set_color");

    // 姹傚樊瀹屾垚鍚庡垹闄ゆ墍鏈変腑闂村璞★紝鍙繚鐣欐渶缁堝嚬妲界粨鏋溿€?
    DeleteObjects(projectionData.projectedCurves);
    DeleteObjects(projectionData.helperCurves);
    UF_OBJ_delete_object(dumbToolBody);

    Log(std::wstring(L"宸插垱寤哄簳闈㈠嚬妲斤紝缁撴灉浣撴暟閲忥細") + std::to_wstring(resultCount));
    Log(L"宸插垹闄よ緟鍔╂洸绾裤€佹姇褰辨洸绾垮拰宸ュ叿浣撱€?);
}

// 搴曢潰妯″紡鍏ュ彛锛氭寜鎺ヨЕ搴曢潰鐨勫鍥磋疆寤撳垱寤哄嚬妲姐€?
void CreateBottomProfileGroove_Test12(tag_t sourceBodyTag)
{
    Log(L"寮€濮嬫墽琛屽簳闈㈡ā寮?..");
    const ContactMatch match = FindBestBottomContact(sourceBodyTag);
    if (match.targetBody == NULL_TAG || match.sourceFace == NULL_TAG)
    {
        throw std::runtime_error("鏈壘鍒版瘮鎵€閫夊疄浣撴洿澶т笖鐩歌创鐨勭洰鏍囦綋銆?);
    }

    const std::vector<tag_t> profileEdges = AskPeripheralLoopEdges(match.sourceFace);
    if (profileEdges.empty())
    {
        throw std::runtime_error("鏈壘鍒扮浉璐撮潰鐨勫鍥磋疆寤撹竟銆?);
    }

    Log(std::wstring(L"搴曢潰妯″紡瀹屾垚锛岀洰鏍囦綋 Tag=") + std::to_wstring(match.targetBody));
    Log(std::wstring(L"宸茶瘑鍒鍥磋疆寤撹竟鏁伴噺锛?) + std::to_wstring(profileEdges.size()));
    const ProjectionCurveData projectionData = CreateProjectedProfileCurves(profileEdges, match.targetFace);
    CreateBottomGrooveCut_Test12(projectionData, match);
}

// 閫夎竟妯″紡鍏ュ彛锛?
// 鐢ㄦ埛鎵嬪伐鎸囧畾涓€缁勮竟锛岀▼搴忔妸杩欎簺杈规姇褰卞埌鐩歌创澶ч潰涓婏紝鍐嶅鐢ㄥ簳闈㈡ā寮忓悗鍗婃鍒涘缓鏍囪妲姐€?
void CreateSelectedEdgesGroove_Test12(tag_t sourceBodyTag, const std::vector<tag_t>& selectedEdgeTags)
{
    Log(L"寮€濮嬫墽琛岄€夎竟妯″紡...");

    if (selectedEdgeTags.empty())
    {
        throw std::runtime_error("閫夎竟妯″紡鏈€夋嫨浠讳綍杈广€?);
    }

    const ContactMatch match = FindBestBottomContact(sourceBodyTag);
    if (match.targetBody == NULL_TAG || match.sourceFace == NULL_TAG || match.targetFace == NULL_TAG)
    {
        throw std::runtime_error("鏈壘鍒扮敤浜庨€夎竟妯″紡鐨勭浉璐寸洰鏍囦綋鍜屾帴瑙﹂潰銆?);
    }

    Log(std::wstring(L"閫夎竟妯″紡瀹屾垚锛岀洰鏍囦綋 Tag=") + std::to_wstring(match.targetBody));
    Log(std::wstring(L"宸查€夋嫨杈规暟閲忥細") + std::to_wstring(selectedEdgeTags.size()));

    const ProjectionCurveData projectionData = CreateProjectedProfileCurves(selectedEdgeTags, match.targetFace);
    CreateBottomGrooveCut_Test12(projectionData, match);
}

class ParameterDialog
{
public:
    // 鍒涘缓瀵硅瘽妗嗗苟娉ㄥ唽鍥炶皟銆?
    ParameterDialog()
    {
        const std::string dialogPath = GetCurrentDllDirectory() + "\\" + kDlxFileName;

        ui_ = UI::GetUI();
        dialog_ = ui_->CreateDialog(dialogPath.c_str());
        if (dialog_ == nullptr)
        {
            throw std::runtime_error("鍒涘缓 DLX 瀵硅瘽妗嗗け璐ャ€?);
        }

        dialog_->AddInitializeHandler(make_callback(this, &ParameterDialog::InitializeCb));
        dialog_->AddDialogShownHandler(make_callback(this, &ParameterDialog::DialogShownCb));
        dialog_->AddUpdateHandler(make_callback(this, &ParameterDialog::UpdateCb));
        dialog_->AddOkHandler(make_callback(this, &ParameterDialog::OkBatchCb));
    }

    ~ParameterDialog()
    {
        if (dialog_ != nullptr)
        {
            delete dialog_;
            dialog_ = nullptr;
        }
    }

    void Show()
    {
        dialog_->Launch();
    }

private:
    // 鍒濆鍖栨帶浠讹紝骞堕檺鍒堕€夋嫨鍣ㄥ彧鑳介€夋嫨宸ヤ綔閮ㄤ欢涓殑瀹炰綋浣撱€?
    void InitializeCb()
    {
        BlockStyler::CompositeBlock* topBlock = dialog_->TopBlock();
        if (topBlock == nullptr)
        {
            throw std::runtime_error("鏃犳硶鑾峰彇瀵硅瘽妗嗛《灞傚潡銆?);
        }

        sourceBodyBlock_ = dynamic_cast<BlockStyler::BodyCollector*>(topBlock->FindBlock(kSelectBodyBlockId));
        sourceEdgeBlock_ = dynamic_cast<BlockStyler::SelectObject*>(topBlock->FindBlock(kSelectEdgeBlockId));
        modeBlock_ = dynamic_cast<BlockStyler::Enumeration*>(topBlock->FindBlock(kModeBlockId));

        if (sourceBodyBlock_ == nullptr || sourceEdgeBlock_ == nullptr || modeBlock_ == nullptr)
        {
            throw std::runtime_error("鏃犳硶鑾峰彇瀵硅瘽妗嗘帶浠躲€?);
        }

        sourceBodyBlock_->SetSelectModeAsString("Multiple");
        sourceBodyBlock_->SetMaximumScopeAsString("Within Work Part Only");
        sourceBodyBlock_->SetAutomaticProgression(false);
        sourceBodyBlock_->SetAllowConvergentObject(false);
        sourceBodyBlock_->SetDefaultBodyRulesAsString("Single Body");

        // 閫夎竟妯″紡浣跨敤鍗曠嫭鐨勫閫夎竟鎺т欢锛岄檺鍒跺彧鑳介€夋嫨宸ヤ綔閮ㄤ欢鍐呯殑杈瑰璞°€?
        sourceEdgeBlock_->SetSelectModeAsString("Multiple");
        sourceEdgeBlock_->SetMaximumScopeAsString("Within Work Part Only");
        sourceEdgeBlock_->SetAutomaticProgression(false);
        sourceEdgeBlock_->ResetFilter();
        sourceEdgeBlock_->AddFilter(BlockStyler::SelectObject::FilterTypeEdges);
    }

    void DialogShownCb()
    {
    }

    int UpdateCb(BlockStyler::UIBlock*)
    {
        return 0;
    }

    // 鐐瑰嚮纭畾鍚庢寜褰撳墠妯″紡鎵归噺澶勭悊鎵€鏈夐€変腑鐨勫疄浣撱€?
    int OkBatchCb()
    {
        try
        {
            if (sourceBodyBlock_ == nullptr || sourceEdgeBlock_ == nullptr || modeBlock_ == nullptr)
            {
                Log(L"瀵硅瘽妗嗘帶浠跺皻鏈纭垵濮嬪寲銆?);
                return 1;
            }

            const std::vector<TaggedObject*> selectedObjects = sourceBodyBlock_->GetSelectedObjects();
            if (selectedObjects.empty())
            {
                Log(L"璇烽€夋嫨涓€涓垨澶氫釜瀹炰綋銆?);
                return 1;
            }

            const NXString modeValue = modeBlock_->ValueAsString();
            const std::wstring modeText = Utf8ToWide(modeValue.GetText());

            // 鍏堟妸鎵€鏈夐€変腑瀹炰綋鐨?tag_t 缂撳瓨涓嬫潵锛岄伩鍏嶅墠涓€涓綋鎵ц姹傚樊鍚庯紝
            // 鍚庣画 TaggedObject 鍙ユ焺澶辨晥锛屽鑷磋鍒?Tag=0銆?
            std::vector<tag_t> selectedBodyTags;
            // 鍏堢紦瀛樻墍鏈夐€変腑浣撶殑 tag_t锛岄伩鍏嶅墠涓€涓綋姹傚樊鍚庯紝
            // 鍚庣画 TaggedObject 鍙ユ焺澶辨晥锛屽鑷村啀璇诲彇鏃跺嚭鐜扮┖瀵硅薄銆?
            selectedBodyTags.reserve(selectedObjects.size());
            for (TaggedObject* selectedObject : selectedObjects)
            {
                Body* selectedBody = dynamic_cast<Body*>(selectedObject);
                if (selectedBody == nullptr)
                {
                    continue;
                }

                const tag_t bodyTag = selectedBody->Tag();
                if (bodyTag != NULL_TAG)
                {
                    selectedBodyTags.push_back(bodyTag);
                }
            }

            if (selectedBodyTags.empty())
            {
                Log(L"鏈鍙栧埌鏈夋晥瀹炰綋鏍囩銆?);
                return 1;
            }

            Log(std::wstring(L"褰撳墠鍒涘缓鏂瑰紡锛?) + modeText);
            Log(std::wstring(L"鏈閫夋嫨瀹炰綋鏁伴噺锛?) + std::to_wstring(selectedBodyTags.size()));

            std::vector<tag_t> selectedEdgeTags;
            if (modeText == L"閫夎竟")
            {
                const std::vector<TaggedObject*> selectedEdgeObjects = sourceEdgeBlock_->GetSelectedObjects();
                selectedEdgeTags.reserve(selectedEdgeObjects.size());
                for (TaggedObject* selectedObject : selectedEdgeObjects)
                {
                    Edge* selectedEdge = dynamic_cast<Edge*>(selectedObject);
                    if (selectedEdge != nullptr && selectedEdge->Tag() != NULL_TAG)
                    {
                        selectedEdgeTags.push_back(selectedEdge->Tag());
                    }
                }

                if (selectedBodyTags.size() != 1)
                {
                    Log(L"閫夎竟妯″紡涓嬭閫夋嫨涓斾粎閫夋嫨涓€涓疄浣撱€?);
                    return 1;
                }

                if (selectedEdgeTags.empty())
                {
                    Log(L"閫夎竟妯″紡涓嬭鑷冲皯閫夋嫨涓€鏉¤竟銆?);
                    return 1;
                }

                Log(std::wstring(L"鏈閫夋嫨杈规暟閲忥細") + std::to_wstring(selectedEdgeTags.size()));
            }

            int successCount = 0;
            int failCount = 0;
            for (tag_t bodyTag : selectedBodyTags)
            {
                Log(std::wstring(L"寮€濮嬪鐞嗗疄浣擄紝Tag=") + std::to_wstring(bodyTag));
                try
                {
                    if (modeText == L"搴曢潰")
                    {
                        CreateBottomProfileGroove_Test12(bodyTag);
                    }
                    else if (modeText == L"杞粨")
                    {
                        CreateLargestOutlineGroove_Test12(bodyTag);
                    }
                    else if (modeText == L"閫夎竟")
                    {
                        CreateSelectedEdgesGroove_Test12(bodyTag, selectedEdgeTags);
                    }
                    else
                    {
                        Log(L"褰撳墠鍒涘缓鏂瑰紡鏈疄鐜般€?);
                    }
                    ++successCount;
                }
                catch (const NXException& exception)
                {
                    ++failCount;
                    Log(std::wstring(L"瀹炰綋澶勭悊澶辫触锛孴ag=") + std::to_wstring(bodyTag) + L"锛屽師鍥狅細" +
                        Utf8ToWide(exception.Message()), true);
                }
                catch (const std::exception& exception)
                {
                    ++failCount;
                    Log(std::wstring(L"瀹炰綋澶勭悊澶辫触锛孴ag=") + std::to_wstring(bodyTag) + L"锛屽師鍥狅細" +
                        Utf8ToWide(exception.what()), true);
                }
            }

            Log(std::wstring(L"鎵归噺鎵ц瀹屾垚锛屾垚鍔熸暟閲忥細") + std::to_wstring(successCount) +
                L"锛屽け璐ユ暟閲忥細" + std::to_wstring(failCount));
            return 0;
        }
        catch (const NXException& exception)
        {
            Log(std::wstring(L"鎵ц澶辫触锛?) + Utf8ToWide(exception.Message()), true);
            return 1;
        }
        catch (const std::exception& exception)
        {
            Log(std::wstring(L"鎵ц澶辫触锛?) + Utf8ToWide(exception.what()), true);
            return 1;
        }
        catch (...)
        {
            Log(L"鎵ц澶辫触锛氬彂鐢熸湭璇嗗埆寮傚父銆?, true);
            return 1;
        }
    }

private:
    UI* ui_ = nullptr;
    BlockStyler::BlockDialog* dialog_ = nullptr;
    BlockStyler::BodyCollector* sourceBodyBlock_ = nullptr;
    BlockStyler::SelectObject* sourceEdgeBlock_ = nullptr;
    BlockStyler::Enumeration* modeBlock_ = nullptr;
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

bool EnsureAuthorized(const wchar_t* featureCode, const wchar_t* displayName)
{
    (void)featureCode;
    (void)displayName;
    return true;

    wchar_t message[1024] = { 0 };
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
extern "C" DllExport void ufusr(char* param, int* returnCode, int paramLen)
{
    if (!zhihui_license_guard::EnsureAuthorized(L"ZHIHUI.BIAOJIXIAN", L"BiaoJiXian"))
    {
        return;
    }

    // NX 鎻掍欢鏍囧噯鍏ュ彛锛氬垵濮嬪寲 UF锛屽脊鍑哄璇濇锛岀粨鏉熸椂鍐嶇粓姝?UF銆?
    (void)param;
    (void)paramLen;
    *returnCode = 0;

    bool ufInitialized = false;

    try
    {
        UfCall(UF_initialize(), "UF_initialize");
        ufInitialized = true;

        ParameterDialog dialog;
        dialog.Show();
    }
    catch (const NXException& exception)
    {
        *returnCode = 1;
        Log(std::wstring(L"NX 寮傚父锛?) + Utf8ToWide(exception.Message()), true);
    }
    catch (const std::exception& exception)
    {
        *returnCode = 1;
        Log(std::wstring(L"绋嬪簭寮傚父锛?) + Utf8ToWide(exception.what()), true);
    }
    catch (...)
    {
        *returnCode = 1;
        Log(L"绋嬪簭寮傚父锛氬彂鐢熸湭璇嗗埆寮傚父銆?, true);
    }

    if (ufInitialized)
    {
        const int terminateCode = UF_terminate();
        if (terminateCode != 0)
        {
            char errorMessage[133] = {0};
            UF_get_fail_message(terminateCode, errorMessage);
            Log(std::wstring(L"UF_terminate 澶辫触锛?) + Utf8ToWide(errorMessage), true);
        }
    }
}

extern "C" DllExport int ufusr_ask_unload()
{
    // 姣忔鎵ц瀹岀珛鍗冲嵏杞斤紝閬垮厤璋冭瘯鏃舵棫 DLL 甯搁┗銆?
    return static_cast<int>(Session::LibraryUnloadOptionImmediately);
}
