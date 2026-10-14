#include "CaiRBan.hpp"
#include "CaiRBanCustomFeatureShared.hpp"
#include "../../common/ZhihuiDialogMemory.hpp"
#include "../../common/ZhihuiContextHelp.hpp"
#ifdef CreateDialog
#undef CreateDialog
#endif

#include <NXOpen/BlockStyler_PropertyList.hxx>
#include <NXOpen/Body.hxx>
#include <NXOpen/BodyCollection.hxx>
#include <NXOpen/BodyDumbRule.hxx>
#include <NXOpen/Callback.hxx>
#include <NXOpen/Curve.hxx>
#include <NXOpen/CurveCollection.hxx>
#include <NXOpen/CurveDumbRule.hxx>
#include <NXOpen/Direction.hxx>
#include <NXOpen/DirectionCollection.hxx>
#include <NXOpen/Edge.hxx>
#include <NXOpen/EdgeBoundaryRule.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/FaceDumbRule.hxx>
#include <NXOpen/Features_BooleanBuilder.hxx>
#include <NXOpen/Features_ConstructionFeatureData.hxx>
#include <NXOpen/Features_CustomAttribute.hxx>
#include <NXOpen/Features_CustomAttributeCollection.hxx>
#include <NXOpen/Features_CustomDoubleAttribute.hxx>
#include <NXOpen/Features_CustomFeature.hxx>
#include <NXOpen/Features_CustomFeatureBuilder.hxx>
#include <NXOpen/Features_CustomFeatureClass.hxx>
#include <NXOpen/Features_CustomFeatureClassManager.hxx>
#include <NXOpen/Features_CustomFeatureData.hxx>
#include <NXOpen/Features_CustomFeatureDataCollection.hxx>
#include <NXOpen/Features_CustomFeaturePreUpdateEvent.hxx>
#include <NXOpen/Features_CustomTagAttribute.hxx>
#include <NXOpen/Features_EditWithRollbackManager.hxx>
#include <NXOpen/Features_ExtrudeBuilder.hxx>
#include <NXOpen/Features_Feature.hxx>
#include <NXOpen/Features_FeatureCollection.hxx>
#include <NXOpen/Features_OffsetFaceBuilder.hxx>
#include <NXOpen/Features_Thicken.hxx>
#include <NXOpen/Features_ThickenBuilder.hxx>
#include <NXOpen/Features_TrimBody2Builder.hxx>
#include <NXOpen/Features_ReplaceFaceBuilder.hxx>
#include <NXOpen/GeometricUtilities_BooleanOperation.hxx>
#include <NXOpen/GeometricUtilities_Extend.hxx>
#include <NXOpen/GeometricUtilities_FeatureOptions.hxx>
#include <NXOpen/GeometricUtilities_BooleanToolBuilder.hxx>
#include <NXOpen/GeometricUtilities_FacePlaneToolBuilder.hxx>
#include <NXOpen/GeometricUtilities_Limits.hxx>
#include <NXOpen/Line.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXMessageBox.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/Plane.hxx>
#include <NXOpen/PlaneCollection.hxx>
#include <NXOpen/ScCollector.hxx>
#include <NXOpen/ScCollectorCollection.hxx>
#include <NXOpen/ScRuleFactory.hxx>
#include <NXOpen/Section.hxx>
#include <NXOpen/SectionCollection.hxx>
#include <NXOpen/Selection.hxx>
#include <NXOpen/SelectionIntentRule.hxx>
#include <NXOpen/SelectionIntentRuleOptions.hxx>
#include <NXOpen/SmartObject.hxx>
#include <NXOpen/TaggedObject.hxx>
#include <NXOpen/UI.hxx>
#include <NXOpen/Update.hxx>

#include <Windows.h>
#ifdef CreateDialog
#undef CreateDialog
#endif

#include <uf_modl.h>
#include <uf_eval.h>
#include <uf_obj.h>
#include <uf_object_types.h>
#include <uf_ui_types.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <vector>

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace
{
constexpr double kTolerance = 1.0e-5;
constexpr double kParallelTolerance = 0.999;
constexpr double kCoverageThreshold = 0.60;
CaiRBanDialog* gActiveCaiRBanDialog = nullptr;
CaiRBanDialog* gDeferredDialog = nullptr;
int gDeferredTries = 0;
bool gKeepLoaded = false;

// Edit-session diagnostics. The custom-feature edit path can terminate ugraf
// before any error box appears, so record every stage to a file first; the
// write is non-throwing and shares the existing deploy log directory so the
// sequence survives a hard crash.
void AppendDebugLog(const std::string& message) noexcept
{
    try
    {
        SYSTEMTIME now = {};
        GetLocalTime(&now);
        char prefix[160] = {};
        sprintf_s(prefix, "[%04u-%02u-%02u %02u:%02u:%02u.%03u] [T%lu] CaiRBan: ",
                  now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute,
                  now.wSecond, now.wMilliseconds, GetCurrentThreadId());
        const std::string line = std::string(prefix) + message + "\r\n";

        CreateDirectoryW(L"D:\\UG智辉钣金插件\\logs", nullptr);
        HANDLE file = CreateFileW(
            L"D:\\UG智辉钣金插件\\logs\\CaiRBan_debug.log", FILE_APPEND_DATA,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
            OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file != INVALID_HANDLE_VALUE)
        {
            DWORD written = 0;
            WriteFile(file, line.data(), static_cast<DWORD>(line.size()),
                      &written, nullptr);
            CloseHandle(file);
        }
    }
    catch (...)
    {
    }
}

// Defined later in this anonymous namespace; declared here for the
// diagnostic snapshot below.
bool FeatureAlive(tag_t tag);

// Diagnostic snapshot of the feature tree. Used to prove whether the
// deferred delete-and-recreate leaves NX consistent: a dangling construction
// reference or a stale current feature is what we need to rule in or out.
std::string FeatureTreeSnapshot(const char* label)
{
    std::string text = std::string(label) + ": ";
    try
    {
        NXOpen::Part* part = NXOpen::Session::GetSession()->Parts()->Work();
        if (part == nullptr)
        {
            return text + "no work part";
        }
        int total = 0;
        int custom = 0;
        int named = 0;
        for (NXOpen::Features::Feature* feature : part->Features()->GetFeatures())
        {
            if (feature == nullptr || !FeatureAlive(feature->Tag()))
            {
                continue;
            }
            ++total;
            if (dynamic_cast<NXOpen::Features::CustomFeature*>(feature) != nullptr)
            {
                ++custom;
            }
            char* featureName = nullptr;
            if (UF_MODL_ask_feat_name(feature->Tag(), &featureName) == 0 &&
                featureName != nullptr &&
                std::string(featureName).find("拆圆弧板") != std::string::npos)
            {
                ++named;
            }
        }
        NXOpen::Features::Feature* current = part->CurrentFeature();
        text += "aliveFeatures=" + std::to_string(total) +
                " customFeatures=" + std::to_string(custom) +
                " namedCaiRBan=" + std::to_string(named) +
                " currentFeature=" +
                std::to_string(current == nullptr ? NULL_TAG : current->Tag());
    }
    catch (const NXOpen::NXException& ex)
    {
        text += std::string("snapshot NXException code=") +
                std::to_string(ex.ErrorCode()) + " message=" +
                (ex.Message() != nullptr ? ex.Message() : "(null)");
    }
    catch (...)
    {
        text += "snapshot failed";
    }
    return text;
}

void SehRunDeferredRebuild(CaiRBanDialog* dialog)
{
    __try
    {
        dialog->RunDeferredRebuild();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

void SehDeleteDialog(CaiRBanDialog* dialog)
{
    __try
    {
        delete dialog;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

void CALLBACK DeferredCaiRBanRebuildTimer(HWND, UINT, UINT_PTR id, DWORD)
{
    KillTimer(nullptr, id);
    CaiRBanDialog* dialog = gDeferredDialog;
    if (dialog == nullptr)
    {
        return;
    }
    if (UF_initialize() != 0)
    {
        gDeferredDialog = nullptr;
        gKeepLoaded = false;
        SehDeleteDialog(dialog);
        return;
    }
    bool finished = true;
    try
    {
        NXOpen::Features::CustomFeature* still = nullptr;
        try
        {
            still = NXOpen::Session::GetSession()
                        ->CustomFeatureClassManager()
                        ->GetEditedCustomFeature();
        }
        catch (const NXOpen::NXException&)
        {
        }
        if (still != nullptr && gDeferredTries < 40 &&
            SetTimer(nullptr, 0, 250, DeferredCaiRBanRebuildTimer) != 0)
        {
            ++gDeferredTries;
            finished = false;
        }
        else
        {
            gDeferredDialog = nullptr;
            gDeferredTries = 0;
            gKeepLoaded = false;
            SehRunDeferredRebuild(dialog);
        }
    }
    catch (...)
    {
        gDeferredDialog = nullptr;
        gKeepLoaded = false;
    }
    if (finished)
    {
        SehDeleteDialog(dialog);
    }
    UF_terminate();
}

struct OppositeFaceCandidate
{
    NXOpen::Face* face = nullptr;
    double signedDistance = 0.0;
    double distance = 0.0;
};

double Dot(const NXOpen::Vector3d& a, const NXOpen::Vector3d& b)
{
    return a.X * b.X + a.Y * b.Y + a.Z * b.Z;
}

double Length(const NXOpen::Vector3d& value)
{
    return std::sqrt(Dot(value, value));
}

bool Normalize(NXOpen::Vector3d& value)
{
    const double length = Length(value);
    if (length <= kTolerance)
    {
        return false;
    }
    value.X /= length;
    value.Y /= length;
    value.Z /= length;
    return true;
}

NXOpen::Vector3d Subtract(const NXOpen::Point3d& to, const NXOpen::Point3d& from)
{
    return NXOpen::Vector3d(to.X - from.X, to.Y - from.Y, to.Z - from.Z);
}

NXOpen::Vector3d Cross(const NXOpen::Vector3d& a, const NXOpen::Vector3d& b)
{
    return NXOpen::Vector3d(a.Y * b.Z - a.Z * b.Y,
                            a.Z * b.X - a.X * b.Z,
                            a.X * b.Y - a.Y * b.X);
}

NXOpen::Point3d Move(const NXOpen::Point3d& point,
                     const NXOpen::Vector3d& direction,
                     double distance)
{
    return NXOpen::Point3d(point.X + direction.X * distance,
                           point.Y + direction.Y * distance,
                           point.Z + direction.Z * distance);
}

bool FacePlaneData(NXOpen::Face* face,
                   NXOpen::Point3d& point,
                   NXOpen::Vector3d& outwardNormal)
{
    if (face == nullptr || face->SolidFaceType() != NXOpen::Face::FaceTypePlanar)
    {
        return false;
    }

    int type = 0;
    double origin[3] = {};
    double direction[3] = {};
    double box[6] = {};
    double radius = 0.0;
    double radiusData = 0.0;
    int normalDirection = 1;
    if (UF_MODL_ask_face_data(face->Tag(), &type, origin, direction, box,
                              &radius, &radiusData, &normalDirection) != 0)
    {
        return false;
    }

    point = NXOpen::Point3d(origin[0], origin[1], origin[2]);
    outwardNormal = NXOpen::Vector3d(direction[0] * normalDirection,
                                     direction[1] * normalDirection,
                                     direction[2] * normalDirection);
    return Normalize(outwardNormal);
}

NXOpen::Point3d FaceBoxCenter(NXOpen::Face* face)
{
    int type = 0;
    double origin[3] = {};
    double direction[3] = {};
    double box[6] = {};
    double radius = 0.0;
    double radiusData = 0.0;
    int normalDirection = 1;
    if (face != nullptr &&
        UF_MODL_ask_face_data(face->Tag(), &type, origin, direction, box,
                              &radius, &radiusData, &normalDirection) == 0)
    {
        return NXOpen::Point3d((box[0] + box[3]) * 0.5,
                               (box[1] + box[4]) * 0.5,
                               (box[2] + box[5]) * 0.5);
    }
    return NXOpen::Point3d(0.0, 0.0, 0.0);
}

bool TryReadPlanarFace(NXOpen::Face* face,
                       NXOpen::Point3d& point,
                       NXOpen::Vector3d& outwardNormal)
{
    if (face == nullptr)
    {
        return false;
    }

    int type = 0;
    double origin[3] = {};
    double direction[3] = {};
    double box[6] = {};
    double radius = 0.0;
    double radiusData = 0.0;
    int normalDirection = 1;
    if (UF_MODL_ask_face_data(face->Tag(), &type, origin, direction, box,
                              &radius, &radiusData, &normalDirection) != 0)
    {
        return false;
    }

    bool planar = type == UF_MODL_PLANAR_FACE;
    if (!planar)
    {
        try
        {
            planar = face->SolidFaceType() == NXOpen::Face::FaceTypePlanar;
        }
        catch (...)
        {
            planar = false;
        }
    }
    if (!planar)
    {
        return false;
    }

    point = NXOpen::Point3d(origin[0], origin[1], origin[2]);
    outwardNormal = NXOpen::Vector3d(direction[0] * normalDirection,
                                     direction[1] * normalDirection,
                                     direction[2] * normalDirection);
    return Normalize(outwardNormal);
}

bool FaceEdgeAxisRange(NXOpen::Face* face,
                       const NXOpen::Point3d& origin,
                       const NXOpen::Vector3d& axis,
                       double& rangeMin,
                       double& rangeMax)
{
    rangeMin = (std::numeric_limits<double>::max)();
    rangeMax = std::numeric_limits<double>::lowest();
    if (face == nullptr)
    {
        return false;
    }

    std::vector<NXOpen::Edge*> edges;
    try
    {
        edges = face->GetEdges();
    }
    catch (...)
    {
        return false;
    }

    bool any = false;
    for (NXOpen::Edge* edge : edges)
    {
        if (edge == nullptr)
        {
            continue;
        }
        NXOpen::Point3d first;
        NXOpen::Point3d second;
        try
        {
            edge->GetVertices(&first, &second);
        }
        catch (...)
        {
            continue;
        }
        for (const NXOpen::Point3d& point : {first, second})
        {
            const double projection = Dot(Subtract(point, origin), axis);
            rangeMin = (std::min)(rangeMin, projection);
            rangeMax = (std::max)(rangeMax, projection);
            any = true;
        }
    }
    return any;
}

// End caps of a thickened cylinder sit at the two axial extremes and do not
// run along the axis. Side walls and the cylinders themselves do. Edge type
// is intentionally not required: thicken often classifies one cap's boundary
// as a spline or intersection edge, which used to leave only one usable face.
bool LocateThickenedEndCaps(
    NXOpen::Body* body,
    const NXOpen::Point3d& origin,
    const NXOpen::Vector3d& axis,
    std::array<std::vector<NXOpen::Face*>, 2>& endFaces,
    std::array<NXOpen::Point3d, 2>& endCenters,
    std::array<NXOpen::Vector3d, 2>& endNormals,
    double& bodyMin,
    double& bodyMax)
{
    endFaces = {};
    endCenters = {};
    endNormals = {};
    bodyMin = (std::numeric_limits<double>::max)();
    bodyMax = std::numeric_limits<double>::lowest();
    if (body == nullptr)
    {
        return false;
    }

    struct Sample
    {
        NXOpen::Face* face = nullptr;
        double rangeMin = 0.0;
        double rangeMax = 0.0;
        double center = 0.0;
        double alignment = 0.0;
        bool planar = false;
        NXOpen::Point3d point{};
        NXOpen::Vector3d normal{};
    };

    std::vector<NXOpen::Face*> faces;
    try
    {
        faces = body->GetFaces();
    }
    catch (...)
    {
        return false;
    }

    std::vector<Sample> samples;
    for (NXOpen::Face* face : faces)
    {
        Sample sample;
        sample.face = face;
        if (!FaceEdgeAxisRange(face, origin, axis, sample.rangeMin,
                               sample.rangeMax))
        {
            continue;
        }
        sample.center = (sample.rangeMin + sample.rangeMax) * 0.5;
        bodyMin = (std::min)(bodyMin, sample.rangeMin);
        bodyMax = (std::max)(bodyMax, sample.rangeMax);
        sample.planar = TryReadPlanarFace(face, sample.point, sample.normal);
        sample.alignment =
            sample.planar ? std::fabs(Dot(sample.normal, axis)) : 0.0;
        samples.push_back(sample);
    }

    const double bodySpan = bodyMax - bodyMin;
    if (samples.empty() || bodySpan <= kTolerance)
    {
        return false;
    }

    const double spanLimit = (std::max)(1.0, bodySpan * 0.85);
    const double mid = (bodyMin + bodyMax) * 0.5;
    std::array<std::vector<Sample>, 2> sides;
    for (const Sample& sample : samples)
    {
        if ((sample.rangeMax - sample.rangeMin) > spanLimit)
        {
            continue;
        }
        const std::size_t side = sample.center <= mid ? 0 : 1;
        sides[side].push_back(sample);
    }

    for (std::size_t side = 0; side < 2; ++side)
    {
        int bestIndex = -1;
        for (std::size_t index = 0; index < sides[side].size(); ++index)
        {
            const Sample& sample = sides[side][index];
            if (!sample.planar || sample.alignment < 0.5 ||
                sample.face == nullptr)
            {
                continue;
            }
            if (bestIndex < 0)
            {
                bestIndex = static_cast<int>(index);
                continue;
            }
            const Sample& best = sides[side][static_cast<std::size_t>(bestIndex)];
            const bool betterAlignment =
                sample.alignment > best.alignment + 1.0e-4;
            const bool sameAlignment =
                std::fabs(sample.alignment - best.alignment) <= 1.0e-4;
            const bool closerToEnd =
                (side == 0 && sample.center < best.center) ||
                (side == 1 && sample.center > best.center);
            if (betterAlignment || (sameAlignment && closerToEnd))
            {
                bestIndex = static_cast<int>(index);
            }
        }
        if (bestIndex < 0)
        {
            continue;
        }

        const Sample& best =
            sides[side][static_cast<std::size_t>(bestIndex)];
        endCenters[side] = FaceBoxCenter(best.face);
        endNormals[side] = best.normal;
        const double centerTolerance = (std::max)(1.0, bodySpan * 0.02);
        for (const Sample& sample : sides[side])
        {
            if (!sample.planar || sample.face == nullptr ||
                sample.alignment < 0.5 ||
                std::fabs(sample.center - best.center) > centerTolerance ||
                Dot(sample.normal, best.normal) < 0.5)
            {
                continue;
            }
            endFaces[side].push_back(sample.face);
        }
    }
    return true;
}

bool PointOnFace(NXOpen::Face* face, const NXOpen::Point3d& point)
{
    double coordinates[3] = {point.X, point.Y, point.Z};
    int status = 0;
    return face != nullptr &&
           UF_MODL_ask_point_containment(coordinates, face->Tag(), &status) == 0 &&
           (status == 1 || status == 3);
}

bool OuterBoundaryEdges(NXOpen::Face* face,
                        std::vector<NXOpen::Edge*>& edges)
{
    edges.clear();
    if (face == nullptr)
    {
        return false;
    }

    uf_loop_p_t loops = nullptr;
    if (UF_MODL_ask_face_loops(face->Tag(), &loops) != 0 || loops == nullptr)
    {
        return false;
    }

    int loopCount = 0;
    UF_MODL_ask_loop_list_count(loops, &loopCount);
    for (int loopIndex = 0; loopIndex < loopCount; ++loopIndex)
    {
        int loopType = 0;
        uf_list_p_t edgeList = nullptr;
        if (UF_MODL_ask_loop_list_item(loops, loopIndex, &loopType, &edgeList) != 0 ||
            loopType != 1 || edgeList == nullptr)
        {
            continue;
        }

        int edgeCount = 0;
        UF_MODL_ask_list_count(edgeList, &edgeCount);
        for (int edgeIndex = 0; edgeIndex < edgeCount; ++edgeIndex)
        {
            tag_t edgeTag = NULL_TAG;
            if (UF_MODL_ask_list_item(edgeList, edgeIndex, &edgeTag) != 0 ||
                edgeTag == NULL_TAG)
            {
                continue;
            }
            NXOpen::Edge* edge =
                dynamic_cast<NXOpen::Edge*>(NXOpen::NXObjectManager::Get(edgeTag));
            if (edge != nullptr)
            {
                edges.push_back(edge);
            }
        }
    }
    UF_MODL_delete_loop_list(&loops);
    return !edges.empty();
}

bool EdgeSamplePoints(NXOpen::Edge* edge,
                      std::vector<NXOpen::Point3d>& samples)
{
    samples.clear();
    if (edge == nullptr)
    {
        return false;
    }

    UF_EVAL_p_t evaluator = nullptr;
    if (UF_EVAL_initialize(edge->Tag(), &evaluator) == 0 && evaluator != nullptr)
    {
        double limits[2] = {};
        if (UF_EVAL_ask_limits(evaluator, limits) == 0 && limits[1] > limits[0])
        {
            for (double fraction : {0.25, 0.50, 0.75})
            {
                const double parameter =
                    limits[0] + (limits[1] - limits[0]) * fraction;
                double point[3] = {};
                if (UF_EVAL_evaluate(evaluator, 0, parameter, point, nullptr) == 0)
                {
                    samples.emplace_back(point[0], point[1], point[2]);
                }
            }
        }
        UF_EVAL_free(evaluator);
    }
    if (samples.size() >= 2)
    {
        return true;
    }

    samples.clear();
    try
    {
        NXOpen::Point3d first;
        NXOpen::Point3d second;
        edge->GetVertices(&first, &second);
        samples.push_back(first);
        samples.emplace_back((first.X + second.X) * 0.5,
                             (first.Y + second.Y) * 0.5,
                             (first.Z + second.Z) * 0.5);
        samples.push_back(second);
    }
    catch (...)
    {
        samples.clear();
    }
    return samples.size() >= 2;
}

NXOpen::Face* AdjacentBodyFace(NXOpen::Edge* edge,
                               NXOpen::Face* selectedFace,
                               NXOpen::Body* targetBody)
{
    if (edge == nullptr)
    {
        return nullptr;
    }
    for (NXOpen::Face* face : edge->GetUnsortedFaces())
    {
        if (face != nullptr && face != selectedFace && face->GetBody() == targetBody)
        {
            return face;
        }
    }
    return nullptr;
}

NXOpen::Face* ExtrudedSideFaceForEdge(
    NXOpen::Edge* sourceEdge,
    const std::vector<NXOpen::Face*>& toolFaces,
    const NXOpen::Vector3d& extrusionDirection)
{
    std::vector<NXOpen::Point3d> samples;
    if (!EdgeSamplePoints(sourceEdge, samples))
    {
        return nullptr;
    }

    NXOpen::Face* match = nullptr;
    for (NXOpen::Face* candidate : toolFaces)
    {
        if (candidate == nullptr)
        {
            continue;
        }

        NXOpen::Point3d planePoint;
        NXOpen::Vector3d planeNormal;
        if (FacePlaneData(candidate, planePoint, planeNormal) &&
            std::fabs(Dot(planeNormal, extrusionDirection)) > kParallelTolerance)
        {
            continue; // Start/end cap, not the side generated by this edge.
        }

        std::size_t contained = 0;
        for (const NXOpen::Point3d& sample : samples)
        {
            contained += PointOnFace(candidate, sample) ? 1U : 0U;
        }
        if (contained >= 2)
        {
            if (match != nullptr)
            {
                return nullptr; // Ambiguous mapping is unsafe for Replace Face.
            }
            match = candidate;
        }
    }
    return match;
}

bool ProjectionAxes(NXOpen::Face* face,
                    const NXOpen::Vector3d& normal,
                    NXOpen::Vector3d& xAxis,
                    NXOpen::Vector3d& yAxis)
{
    if (face != nullptr)
    {
        for (NXOpen::Edge* edge : face->GetEdges())
        {
            NXOpen::Point3d first;
            NXOpen::Point3d second;
            try
            {
                edge->GetVertices(&first, &second);
            }
            catch (...)
            {
                continue;
            }
            xAxis = Subtract(second, first);
            const double component = Dot(xAxis, normal);
            xAxis.X -= normal.X * component;
            xAxis.Y -= normal.Y * component;
            xAxis.Z -= normal.Z * component;
            if (Normalize(xAxis))
            {
                yAxis = Cross(normal, xAxis);
                return Normalize(yAxis);
            }
        }
    }

    const NXOpen::Vector3d seed(std::fabs(normal.X) < 0.9 ? 1.0 : 0.0,
                                std::fabs(normal.X) < 0.9 ? 0.0 : 1.0,
                                0.0);
    xAxis = Cross(seed, normal);
    if (!Normalize(xAxis))
    {
        return false;
    }
    yAxis = Cross(normal, xAxis);
    return Normalize(yAxis);
}

bool BuildInteriorSamples(NXOpen::Face* face,
                          const NXOpen::Point3d& planeOrigin,
                          const NXOpen::Vector3d& xAxis,
                          const NXOpen::Vector3d& yAxis,
                          std::vector<NXOpen::Point3d>& samples)
{
    samples.clear();
    double minimumX = std::numeric_limits<double>::max();
    double maximumX = -minimumX;
    double minimumY = minimumX;
    double maximumY = -minimumX;
    for (NXOpen::Edge* edge : face->GetEdges())
    {
        NXOpen::Point3d points[2];
        try
        {
            edge->GetVertices(&points[0], &points[1]);
        }
        catch (...)
        {
            continue;
        }
        for (const NXOpen::Point3d& point : points)
        {
            const NXOpen::Vector3d offset = Subtract(point, planeOrigin);
            const double x = Dot(offset, xAxis);
            const double y = Dot(offset, yAxis);
            minimumX = std::min(minimumX, x);
            maximumX = std::max(maximumX, x);
            minimumY = std::min(minimumY, y);
            maximumY = std::max(maximumY, y);
        }
    }
    if (!std::isfinite(minimumX) || maximumX - minimumX <= kTolerance ||
        maximumY - minimumY <= kTolerance)
    {
        return false;
    }

    // Same principle as feature 36: sample only the selected trimmed face,
    // so concave boundaries and inner loops do not contaminate thickness.
    constexpr int divisions = 11;
    for (int xIndex = 0; xIndex < divisions; ++xIndex)
    {
        for (int yIndex = 0; yIndex < divisions; ++yIndex)
        {
            const double x = minimumX + (maximumX - minimumX) *
                (static_cast<double>(xIndex) + 0.5) / divisions;
            const double y = minimumY + (maximumY - minimumY) *
                (static_cast<double>(yIndex) + 0.5) / divisions;
            const NXOpen::Point3d candidate(
                planeOrigin.X + xAxis.X * x + yAxis.X * y,
                planeOrigin.Y + xAxis.Y * x + yAxis.Y * y,
                planeOrigin.Z + xAxis.Z * x + yAxis.Z * y);
            if (PointOnFace(face, candidate))
            {
                samples.push_back(candidate);
            }
        }
    }
    return !samples.empty();
}

std::string DialogPath()
{
    wchar_t modulePath[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(
        reinterpret_cast<HMODULE>(&__ImageBase), modulePath, MAX_PATH);
    if (length > 0 && length < MAX_PATH)
    {
        const std::filesystem::path path =
            std::filesystem::path(modulePath).parent_path() / L"CaiRBan.dlx";
        if (std::filesystem::exists(path))
        {
            return path.string();
        }
    }
    return "CaiRBan.dlx";
}

std::string DialogPathWithHelp()
{
    zhihui_context_help::EnsureGlobalHelpLoaded();
    return DialogPath();
}

std::string Number(double value)
{
    std::ostringstream stream;
    stream.precision(15);
    stream << value;
    return stream.str();
}

bool FeatureAlive(tag_t tag)
{
    return tag != NULL_TAG && UF_OBJ_ask_status(tag) == UF_OBJ_ALIVE;
}

void RememberTag(std::vector<tag_t>& tags, tag_t tag)
{
    if (tag == NULL_TAG)
    {
        return;
    }
    if (std::find(tags.begin(), tags.end(), tag) == tags.end())
    {
        tags.push_back(tag);
    }
}

NXOpen::Face* AliveCylindricalFace(const std::vector<tag_t>& tags)
{
    for (tag_t faceTag : tags)
    {
        if (!FeatureAlive(faceTag))
        {
            continue;
        }
        auto* face = dynamic_cast<NXOpen::Face*>(
            NXOpen::NXObjectManager::Get(faceTag));
        if (face == nullptr)
        {
            continue;
        }
        int type = 0;
        double origin[3] = {};
        double direction[3] = {};
        double box[6] = {};
        double radius = 0.0;
        double radiusData = 0.0;
        int normalDirection = 1;
        if (UF_MODL_ask_face_data(
                faceTag, &type, origin, direction, box, &radius, &radiusData,
                &normalDirection) != 0 ||
            type != UF_MODL_CYLINDRICAL_FACE || radius <= kTolerance)
        {
            continue;
        }
        return face;
    }
    return nullptr;
}
}

CaiRBanDialog::CaiRBanDialog()
    : ui_(NXOpen::UI::GetUI()),
      session_(NXOpen::Session::GetSession()),
      dialog_(ui_->CreateDialog(DialogPathWithHelp().c_str())),
      recolorToggle_(nullptr),
      colorMode_(nullptr),
      fixedColor_(nullptr),
      extensionLength_(nullptr),
      wrapMode_(nullptr),
      faceSelect_(nullptr),
      cutDimension0_(nullptr),
      cutDimension1_(nullptr),
      offsetDimension0_(nullptr),
      offsetDimension1_(nullptr),
      customFeatureManager_(nullptr),
      editedFeature_(nullptr),
      featureClass_(nullptr),
      previewUndoMark_(static_cast<NXOpen::Session::UndoMarkId>(0)),
      hasPreview_(false),
      loadingEditedFeature_(false),
      rebuildingPreview_(false),
      configuringDimensionHandles_(false),
      dimensionValuesInitialized_(false),
      buildingCustomFeature_(false),
      rebuildingEditedFeature_(false),
      editRebuildPending_(false),
      useCachedParameters_(false),
      cachedRecolor_(false),
      previewSubtractCreated_(false),
      previewColor_(0),
      previewTargetBodyTag_(NULL_TAG),
      previewSelectedFaceTag_(NULL_TAG),
      dimensionFaceTag_(NULL_TAG),
      editedFeatureTag_(NULL_TAG),
      cachedColorMode_(0),
      cachedFixedColor_(36),
      cachedAppliedColor_(0),
      cachedWrapMode_(0),
      cachedExtension_(0.0),
      cutDistances_{0.0, 0.0},
      offsetDistances_{0.0, 0.0},
      currentThickness_(0.0),
      previewCreatedFeatureTags_(),
      previewBodyTranslucencies_(),
      editPreviewMark_(static_cast<NXOpen::Session::UndoMarkId>(0)),
      editRolledBack_(false),
      currentFeatureBeforeEdit_(NULL_TAG)
{
    customFeatureManager_ = session_->CustomFeatureClassManager();
    editedFeature_ = customFeatureManager_->GetEditedCustomFeature();
    if (editedFeature_ != nullptr)
    {
        editedFeatureTag_ = editedFeature_->Tag();
    }
    try
    {
        featureClass_ = customFeatureManager_->GetClassFromName(
            zhihui_cairban::kFeatureClassName);
    }
    catch (...)
    {
        featureClass_ = nullptr;
    }
    gActiveCaiRBanDialog = this;
    // Keep one line per edit so a crash can be pinned to "which feature, was
    // the class registered". Everything else on the happy path stays quiet.
    AppendDebugLog("edit start edited=" + std::to_string(editedFeatureTag_) +
                   (featureClass_ == nullptr ? " FEATURECLASS-MISSING" : ""));
    dialog_->AddInitializeHandler(NXOpen::make_callback(this, &CaiRBanDialog::initialize_cb));
    dialog_->AddDialogShownHandler(NXOpen::make_callback(this, &CaiRBanDialog::dialogShown_cb));
    dialog_->AddUpdateHandler(NXOpen::make_callback(this, &CaiRBanDialog::update_cb));
    dialog_->AddApplyHandler(NXOpen::make_callback(this, &CaiRBanDialog::apply_cb));
    dialog_->AddOkHandler(NXOpen::make_callback(this, &CaiRBanDialog::ok_cb));
    dialog_->AddCancelHandler(NXOpen::make_callback(this, &CaiRBanDialog::cancel_cb));
}

CaiRBanDialog::~CaiRBanDialog()
{
    try
    {
        if (hasPreview_ ||
            previewUndoMark_ != static_cast<NXOpen::Session::UndoMarkId>(0))
        {
            UndoPreview();
        }
        RestoreEditRollback();
    }
    catch (...)
    {
    }
    RestorePreviewTranslucency();
    if (gActiveCaiRBanDialog == this)
    {
        gActiveCaiRBanDialog = nullptr;
    }
    delete dialog_;
}

bool CaiRBanShouldStayLoaded()
{
    return gKeepLoaded;
}

bool CaiRBanDialog::DeferredRebuildPending() const
{
    return gDeferredDialog == this;
}

bool CaiRBanDialog::ScheduleDeferredRebuild()
{
    if (gDeferredDialog != nullptr)
    {
        return false;
    }
    gDeferredDialog = this;
    gDeferredTries = 0;
    gKeepLoaded = true;
    if (SetTimer(nullptr, 0, 400, DeferredCaiRBanRebuildTimer) == 0)
    {
        gDeferredDialog = nullptr;
        gKeepLoaded = false;
        return false;
    }
    return true;
}

void CaiRBanDialog::RunDeferredRebuild()
{
    NXOpen::Features::CustomFeature* still = nullptr;
    try
    {
        still = session_->CustomFeatureClassManager()->GetEditedCustomFeature();
    }
    catch (const NXOpen::NXException&)
    {
    }
    if (still != nullptr)
    {
        AppendDebugLog("rebuild abandoned: NX still editing tag=" +
                       std::to_string(still->Tag()));
        ShowError("NX 仍处于编辑状态，拆圆弧板未更新。");
        return;
    }
    try
    {
        RebuildEditedFeature();
    }
    catch (const NXOpen::NXException& ex)
    {
        AppendDebugLog("rebuild failed NXException code=" +
                       std::to_string(ex.ErrorCode()) + " message=" +
                       (ex.Message() != nullptr ? ex.Message() : "(null)"));
        ShowError(ex.Message() != nullptr ? ex.Message() : "编辑拆圆弧板失败。");
    }
    catch (const std::exception& ex)
    {
        AppendDebugLog(std::string("rebuild failed: ") + ex.what());
        ShowError(ex.what());
    }
    catch (...)
    {
        AppendDebugLog("RunDeferredRebuild unknown exception");
        ShowError("编辑拆圆弧板失败。");
    }
}

NXOpen::BlockStyler::BlockDialog::DialogResponse CaiRBanDialog::Launch()
{
    const NXOpen::BlockStyler::BlockDialog::DialogResponse response =
        dialog_->LaunchInDialogMode(
            editedFeatureTag_ != NULL_TAG
                ? NXOpen::BlockStyler::BlockDialog::DialogModeEdit
                : NXOpen::BlockStyler::BlockDialog::DialogModeCreate);
    if (!editRebuildPending_)
    {
        return response;
    }
    editRebuildPending_ = false;
    NXOpen::Features::CustomFeature* stillEdited = nullptr;
    try
    {
        stillEdited = customFeatureManager_->GetEditedCustomFeature();
    }
    catch (const NXOpen::NXException&)
    {
    }
    if (stillEdited != nullptr)
    {
        if (!ScheduleDeferredRebuild())
        {
            AppendDebugLog("deferred rebuild scheduling failed for tag=" +
                           std::to_string(editedFeatureTag_));
            ShowError("拆圆弧板未能在编辑结束后重新创建。");
        }
        return response;
    }
    try
    {
        RebuildEditedFeature();
    }
    catch (const NXOpen::NXException& ex)
    {
        AppendDebugLog("rebuild (direct) NXException code=" +
                       std::to_string(ex.ErrorCode()) + " message=" +
                       (ex.Message() != nullptr ? ex.Message() : "(null)"));
        ShowError(ex.Message() != nullptr ? ex.Message() : "编辑拆圆弧板失败。");
    }
    catch (const std::exception& ex)
    {
        AppendDebugLog(std::string("Launch std::exception: ") + ex.what());
        ShowError(ex.what());
    }
    return response;
}

void CaiRBanDialog::initialize_cb()
{
    recolorToggle_ = dialog_->TopBlock()->FindBlock("recolor_toggle");
    colorMode_ = dialog_->TopBlock()->FindBlock("color_mode");
    fixedColor_ = dynamic_cast<NXOpen::BlockStyler::ObjectColorPicker*>(
        dialog_->TopBlock()->FindBlock("fixed_color"));
    extensionLength_ = dialog_->TopBlock()->FindBlock("extension_length");
    wrapMode_ = dialog_->TopBlock()->FindBlock("wrap_mode");
    faceSelect_ = dialog_->TopBlock()->FindBlock("face_select");
    cutDimension0_ = dynamic_cast<NXOpen::BlockStyler::LinearDimension*>(
        dialog_->TopBlock()->FindBlock("cut_dimension_0"));
    cutDimension1_ = dynamic_cast<NXOpen::BlockStyler::LinearDimension*>(
        dialog_->TopBlock()->FindBlock("cut_dimension_1"));
    offsetDimension0_ = dynamic_cast<NXOpen::BlockStyler::LinearDimension*>(
        dialog_->TopBlock()->FindBlock("offset_dimension_0"));
    offsetDimension1_ = dynamic_cast<NXOpen::BlockStyler::LinearDimension*>(
        dialog_->TopBlock()->FindBlock("offset_dimension_1"));
    if (recolorToggle_ == nullptr || colorMode_ == nullptr ||
        fixedColor_ == nullptr || extensionLength_ == nullptr ||
        wrapMode_ == nullptr || faceSelect_ == nullptr ||
        cutDimension0_ == nullptr || cutDimension1_ == nullptr ||
        offsetDimension0_ == nullptr || offsetDimension1_ == nullptr)
    {
        throw std::runtime_error("CaiRBan.dlx is missing a required block.");
    }

    if (fixedColor_->GetValue().empty())
    {
        fixedColor_->SetValue(std::vector<int>{36});
    }
    if (editedFeature_ == nullptr)
    {
        LoadDialogState();
    }
    UpdateColorControlVisibility();

    NXOpen::BlockStyler::PropertyList* properties = faceSelect_->GetProperties();
    std::vector<NXOpen::Selection::MaskTriple> masks;
    masks.emplace_back(UF_solid_type, UF_solid_face_subtype,
                       UF_UI_SEL_FEATURE_CYLINDRICAL_FACE);
    properties->SetSelectionFilter(
        "SelectionFilter",
        NXOpen::Selection::SelectionActionClearAndEnableSpecific,
        masks);
    properties->SetEnum("StepStatus", 0);
    delete properties;

    LoadEditedCustomFeatureData();
}

void CaiRBanDialog::dialogShown_cb()
{
    if (customFeatureManager_ != nullptr)
    {
        NXOpen::Features::CustomFeature* currentEdited =
            customFeatureManager_->GetEditedCustomFeature();
        if (currentEdited != nullptr && currentEdited != editedFeature_)
        {
            editedFeature_ = currentEdited;
            editedFeatureTag_ = currentEdited->Tag();
            LoadEditedCustomFeatureData();
        }
    }
    UpdateColorControlVisibility();
    if (editedFeature_ != nullptr || FeatureAlive(editedFeatureTag_))
    {
        try
        {
            BeginEditPreview();
        }
        catch (const NXOpen::NXException& ex)
        {
            AppendDebugLog("edit preview restore NXException code=" +
                           std::to_string(ex.ErrorCode()) + " message=" +
                           (ex.Message() != nullptr ? ex.Message() : "(null)"));
            ShowError(ex.Message() != nullptr ? ex.Message()
                                             : "无法恢复拆圆弧板预览。");
        }
        catch (const std::exception& ex)
        {
            AppendDebugLog(std::string("dialogShown_cb std::exception: ") +
                           ex.what());
            ShowError(ex.what());
        }
    }
}

int CaiRBanDialog::update_cb(NXOpen::BlockStyler::UIBlock* block)
{
    if (loadingEditedFeature_ || rebuildingPreview_ ||
        configuringDimensionHandles_)
    {
        return 0;
    }

    const bool dimensionChanged =
        block == cutDimension0_ || block == cutDimension1_ ||
        block == offsetDimension0_ || block == offsetDimension1_;
    const bool parameterChanged =
        block == recolorToggle_ || block == colorMode_ ||
        block == fixedColor_ || block == extensionLength_ ||
        block == wrapMode_ || dimensionChanged;
    const bool selectionChanged = block == faceSelect_;
    if (!parameterChanged && !selectionChanged)
    {
        return 0;
    }

    rebuildingPreview_ = true;
    if (block == extensionLength_)
    {
        cutDistances_[0] = ExtensionLength();
        cutDistances_[1] = ExtensionLength();
        dimensionValuesInitialized_ = true;
    }
    else if (block == wrapMode_)
    {
        const double defaultOffset =
            WrapMode() == 1 ? -std::fabs(currentThickness_) : 0.0;
        offsetDistances_[0] = defaultOffset;
        offsetDistances_[1] = defaultOffset;
        dimensionValuesInitialized_ = true;
    }
    else if (dimensionChanged)
    {
        cutDistances_[0] = (std::max)(0.0, cutDimension0_->Value());
        cutDistances_[1] = (std::max)(0.0, cutDimension1_->Value());
        offsetDistances_[0] = offsetDimension0_->Value();
        offsetDistances_[1] = offsetDimension1_->Value();
        dimensionValuesInitialized_ = true;
    }
    if (selectionChanged)
    {
        dimensionValuesInitialized_ = false;
        dimensionFaceTag_ = NULL_TAG;
    }
        if (parameterChanged)
    {
        UpdateColorControlVisibility();
        SaveDialogState();
    }

    // Every result-affecting dialog change follows the same transaction:
    // remove the old preview first, then build a new preview from the current
    // control values. Rebuild even when the preceding preview failed and
    // hasPreview_ is false, so changing a parameter can immediately retry.
    if (hasPreview_ ||
        previewUndoMark_ != static_cast<NXOpen::Session::UndoMarkId>(0) ||
        !previewCreatedFeatureTags_.empty())
    {
        if (!UndoPreview())
        {
            rebuildingPreview_ = false;
            ShowError("旧预览特征未能完全撤销，已停止创建新预览。");
            return 1;
        }
    }
    const int result = SelectedFace() != nullptr ? CreatePreview() : 0;
    rebuildingPreview_ = false;
    return result;
}

void CaiRBanDialog::UpdateColorControlVisibility() const
{
    if (recolorToggle_ == nullptr || colorMode_ == nullptr || fixedColor_ == nullptr)
    {
        return;
    }
    NXOpen::BlockStyler::PropertyList* toggleProperties =
        recolorToggle_->GetProperties();
    const bool enabled = toggleProperties->GetLogical("Value");
    delete toggleProperties;

    colorMode_->SetShow(enabled);
    int mode = 0;
    if (enabled)
    {
        NXOpen::BlockStyler::PropertyList* modeProperties =
            colorMode_->GetProperties();
        mode = modeProperties->GetEnum("Value");
        delete modeProperties;
    }
    fixedColor_->SetShow(enabled && mode == 1);
}

bool CaiRBanDialog::RecolorEnabled() const
{
    if (recolorToggle_ == nullptr)
    {
        return false;
    }
    NXOpen::BlockStyler::PropertyList* properties =
        recolorToggle_->GetProperties();
    const bool value = properties->GetLogical("Value");
    delete properties;
    return value;
}

int CaiRBanDialog::RequestedColor() const
{
    const int mode = ColorMode();
    if (mode == 1 && fixedColor_ != nullptr)
    {
        return FixedColor();
    }

    static std::mt19937 generator(static_cast<unsigned int>(GetTickCount64()));
    static std::uniform_int_distribution<int> colors(1, 216);
    return colors(generator);
}

int CaiRBanDialog::ColorMode() const
{
    if (colorMode_ == nullptr)
    {
        return 0;
    }
    NXOpen::BlockStyler::PropertyList* properties = colorMode_->GetProperties();
    const int value = properties->GetEnum("Value");
    delete properties;
    return value;
}

int CaiRBanDialog::FixedColor() const
{
    if (fixedColor_ != nullptr)
    {
        const std::vector<int> values = fixedColor_->GetValue();
        if (!values.empty())
        {
            return (std::max)(1, (std::min)(216, values.front()));
        }
    }
    return 36;
}

double CaiRBanDialog::ExtensionLength() const
{
    double value = 0.0;
    zhihui_dialog_memory::TryGetDouble(extensionLength_, &value);
    return (std::max)(0.0, value);
}

void CaiRBanDialog::ConfigureDimensionHandles(
    const std::array<NXOpen::Point3d, 4>& origins,
    const std::array<NXOpen::Vector3d, 4>& directions)
{
    std::array<NXOpen::BlockStyler::LinearDimension*, 4> dimensions{
        cutDimension0_, cutDimension1_, offsetDimension0_, offsetDimension1_};
    const std::array<double, 4> values{
        cutDistances_[0], cutDistances_[1],
        offsetDistances_[0], offsetDistances_[1]};
    configuringDimensionHandles_ = true;
    try
    {
        for (std::size_t index = 0; index < dimensions.size(); ++index)
        {
            NXOpen::BlockStyler::LinearDimension* dimension = dimensions[index];
            if (dimension == nullptr)
            {
                continue;
            }
            dimension->SetFormula(Number(values[index]).c_str());
            dimension->SetValue(values[index]);
            dimension->SetShowHandle(true);
            dimension->SetShowFocusHandle(false);
            dimension->SetAutoReverseDuringDrag(true);
            dimension->SetHandleOrigin(origins[index]);
            dimension->SetHandleOrientation(directions[index]);
        }
    }
    catch (...)
    {
        configuringDimensionHandles_ = false;
        throw;
    }
    configuringDimensionHandles_ = false;
}

int CaiRBanDialog::WrapMode() const
{
    int value = 0;
    zhihui_dialog_memory::TryGetEnum(wrapMode_, &value);
    return value == 1 ? 1 : 0;
}

void CaiRBanDialog::LoadDialogState() const
{
    constexpr const wchar_t* fileName = L"CaiRBan_state.ini";
    zhihui_dialog_memory::LoadLogical(
        fileName, L"recolorEnabled", recolorToggle_);
    zhihui_dialog_memory::LoadEnum(fileName, L"colorMode", colorMode_);
    zhihui_dialog_memory::LoadDouble(
        fileName, L"extensionLength", extensionLength_);
    zhihui_dialog_memory::LoadEnum(fileName, L"wrapMode", wrapMode_);
    const int color = (std::max)(
        1,
        (std::min)(216,
                   zhihui_dialog_memory::ReadInt(
                       fileName, L"fixedColor", FixedColor())));
    fixedColor_->SetValue(std::vector<int>{color});
}

void CaiRBanDialog::SaveDialogState() const
{
    constexpr const wchar_t* fileName = L"CaiRBan_state.ini";
    zhihui_dialog_memory::SaveLogical(
        fileName, L"recolorEnabled", recolorToggle_);
    zhihui_dialog_memory::SaveEnum(fileName, L"colorMode", colorMode_);
    zhihui_dialog_memory::SaveDouble(
        fileName, L"extensionLength", extensionLength_);
    zhihui_dialog_memory::SaveEnum(fileName, L"wrapMode", wrapMode_);
    zhihui_dialog_memory::WriteInt(
        fileName, L"fixedColor", FixedColor());
}

void CaiRBanDialog::ReleaseFaceSelection()
{
    if (faceSelect_ == nullptr)
    {
        return;
    }
    loadingEditedFeature_ = true;
    try
    {
        NXOpen::BlockStyler::PropertyList* properties =
            faceSelect_->GetProperties();
        properties->SetTaggedObjectVector(
            "SelectedObjects", std::vector<NXOpen::TaggedObject*>());
        delete properties;
    }
    catch (...)
    {
    }
    loadingEditedFeature_ = false;
}

void CaiRBanDialog::RestoreEditRollback()
{
    if (!editRolledBack_ &&
        editPreviewMark_ == static_cast<NXOpen::Session::UndoMarkId>(0))
    {
        return;
    }
    try
    {
        if (editPreviewMark_ != static_cast<NXOpen::Session::UndoMarkId>(0) &&
            session_->DoesUndoMarkExist(editPreviewMark_, "拆圆弧板编辑预览"))
        {
            session_->UndoToMark(editPreviewMark_, "拆圆弧板编辑预览");
            if (session_->DoesUndoMarkExist(editPreviewMark_, "拆圆弧板编辑预览"))
            {
                session_->DeleteUndoMark(editPreviewMark_, "拆圆弧板编辑预览");
            }
        }
        else if (FeatureAlive(currentFeatureBeforeEdit_))
        {
            UF_MODL_set_current_feature(currentFeatureBeforeEdit_);
        }
    }
    catch (...)
    {
    }
    editPreviewMark_ = static_cast<NXOpen::Session::UndoMarkId>(0);
    editRolledBack_ = false;
    currentFeatureBeforeEdit_ = NULL_TAG;
}

std::vector<tag_t> CaiRBanDialog::CollectEditedInputFaces(
    NXOpen::Features::CustomFeature* node) const
{
    std::vector<tag_t> tags;
    if (node == nullptr)
    {
        return tags;
    }
    RememberTag(tags, previewSelectedFaceTag_);
    RememberTag(tags, dimensionFaceTag_);
    try
    {
        NXOpen::TaggedObject* stored =
            node->FeatureData()
                ->CustomTagAttributeByName(zhihui_cairban::kAttrSelectedFace)
                ->Value();
        if (stored != nullptr)
        {
            RememberTag(tags, stored->Tag());
        }
    }
    catch (...)
    {
    }

    NXOpen::Part* part = session_ == nullptr ? nullptr : session_->Parts()->Work();
    if (part == nullptr)
    {
        return tags;
    }
    try
    {
        for (NXOpen::Features::ConstructionFeatureData* item :
             node->GetConstructionFeatures())
        {
            NXOpen::Features::Feature* feature =
                item == nullptr ? nullptr : item->GetFeature();
            if (feature == nullptr)
            {
                continue;
            }
            NXOpen::Features::ThickenBuilder* builder = nullptr;
            try
            {
                builder = part->Features()->CreateThickenBuilder(feature);
            }
            catch (const NXOpen::NXException&)
            {
                builder = nullptr;
            }
            if (builder == nullptr)
            {
                continue;
            }
            for (NXOpen::TaggedObject* object :
                 builder->FaceCollector()->GetObjects())
            {
                if (object != nullptr)
                {
                    RememberTag(tags, object->Tag());
                }
            }
            builder->Destroy();
            break;
        }
    }
    catch (...)
    {
    }
    return tags;
}

void CaiRBanDialog::BeginEditPreview()
{
    if (hasPreview_ || editRolledBack_)
    {
        return;
    }
    NXOpen::Part* part = session_->Parts()->Work();
    auto* node = dynamic_cast<NXOpen::Features::CustomFeature*>(
        FeatureAlive(editedFeatureTag_)
            ? NXOpen::NXObjectManager::Get(editedFeatureTag_)
            : nullptr);
    if (part == nullptr || node == nullptr)
    {
        return;
    }

    const std::vector<tag_t> recordedFaces = CollectEditedInputFaces(node);

    int boundary = node->Timestamp();
    try
    {
        for (NXOpen::Features::ConstructionFeatureData* item :
             node->GetConstructionFeatures())
        {
            NXOpen::Features::Feature* feature =
                item == nullptr ? nullptr : item->GetFeature();
            if (feature != nullptr)
            {
                boundary = (std::min)(boundary, feature->Timestamp());
            }
        }
    }
    catch (...)
    {
    }

    NXOpen::Features::Feature* insertion = nullptr;
    for (NXOpen::Features::Feature* feature : part->Features()->GetFeatures())
    {
        if (feature == nullptr || feature->IsInternal() ||
            feature->Tag() == editedFeatureTag_ ||
            feature->Timestamp() >= boundary)
        {
            continue;
        }
        if (insertion == nullptr ||
            feature->Timestamp() > insertion->Timestamp())
        {
            insertion = feature;
        }
    }

    NXOpen::Features::Feature* originalCurrent = part->CurrentFeature();
    currentFeatureBeforeEdit_ =
        originalCurrent == nullptr ? NULL_TAG : originalCurrent->Tag();
    editPreviewMark_ = session_->SetUndoMark(
        NXOpen::Session::MarkVisibilityInvisible, "拆圆弧板编辑预览");
    if (insertion != nullptr &&
        insertion->Tag() != currentFeatureBeforeEdit_)
    {
        rebuildingEditedFeature_ = true;
        const int rolled = UF_MODL_set_current_feature(insertion->Tag());
        rebuildingEditedFeature_ = false;
        editRolledBack_ = rolled == 0;
        if (rolled != 0)
        {
            AppendDebugLog("edit preview roll failed status=" +
                           std::to_string(rolled) + " insertion=" +
                           std::to_string(insertion->Tag()));
        }
    }

    NXOpen::Face* face = AliveCylindricalFace(recordedFaces);
    if (face == nullptr || faceSelect_ == nullptr)
    {
        RestoreEditRollback();
        ShowError("无法读取已保存的圆弧面，不能恢复编辑预览。");
        return;
    }

    loadingEditedFeature_ = true;
    try
    {
        NXOpen::BlockStyler::PropertyList* properties =
            faceSelect_->GetProperties();
        properties->SetTaggedObjectVector(
            "SelectedObjects",
            std::vector<NXOpen::TaggedObject*>{face});
        delete properties;
    }
    catch (...)
    {
        loadingEditedFeature_ = false;
        throw;
    }
    loadingEditedFeature_ = false;

    previewSelectedFaceTag_ = face->Tag();
    dimensionFaceTag_ = face->Tag();
    if (face->GetBody() != nullptr)
    {
        previewTargetBodyTag_ = face->GetBody()->Tag();
    }
    dimensionValuesInitialized_ = true;
    CreatePreview();
}

void CaiRBanDialog::LoadEditedCustomFeatureData()
{
    if (editedFeature_ == nullptr || faceSelect_ == nullptr)
    {
        return;
    }
    loadingEditedFeature_ = true;
    try
    {
        NXOpen::Features::CustomFeatureData* data = editedFeature_->FeatureData();
        try
        {
            NXOpen::Face* storedFace = dynamic_cast<NXOpen::Face*>(
                data->CustomTagAttributeByName(
                        zhihui_cairban::kAttrSelectedFace)
                    ->Value());
            if (storedFace != nullptr)
            {
                dimensionFaceTag_ = storedFace->Tag();
                previewSelectedFaceTag_ = storedFace->Tag();
                if (storedFace->GetBody() != nullptr)
                {
                    previewTargetBodyTag_ = storedFace->GetBody()->Tag();
                }
            }
            NXOpen::TaggedObject* storedTarget =
                data->CustomTagAttributeByName(
                        zhihui_cairban::kAttrTargetBody)
                    ->Value();
            if (storedTarget != nullptr)
            {
                previewTargetBodyTag_ = storedTarget->Tag();
            }
        }
        catch (...)
        {
            // Older or topologically consumed faces need not be displayed.
        }
        NXOpen::BlockStyler::PropertyList* toggleProperties =
            recolorToggle_->GetProperties();
        toggleProperties->SetLogical(
            "Value",
            data->CustomDoubleAttributeByName(
                    zhihui_cairban::kAttrRecolorEnabled)
                    ->Value() > 0.5);
        delete toggleProperties;

        NXOpen::BlockStyler::PropertyList* modeProperties =
            colorMode_->GetProperties();
        modeProperties->SetEnum(
            "Value",
            static_cast<int>(data->CustomDoubleAttributeByName(
                zhihui_cairban::kAttrColorMode)->Value()));
        delete modeProperties;

        const int fixedColor = static_cast<int>(
            data->CustomDoubleAttributeByName(
                    zhihui_cairban::kAttrFixedColor)
                ->Value());
        fixedColor_->SetValue(std::vector<int>{
            (std::max)(1, (std::min)(216, fixedColor))});
        zhihui_dialog_memory::TrySetDouble(
            extensionLength_,
            data->CustomDoubleAttributeByName(
                    zhihui_cairban::kAttrExtensionLength)
                ->Value());
        cutDistances_[0] = ExtensionLength();
        cutDistances_[1] = ExtensionLength();
        offsetDistances_[0] = 0.0;
        offsetDistances_[1] = 0.0;
        bool loadedDimensionValues = false;
        try
        {
            cutDistances_[0] = data->CustomDoubleAttributeByName(
                zhihui_cairban::kAttrCutDistance0)->Value();
            cutDistances_[1] = data->CustomDoubleAttributeByName(
                zhihui_cairban::kAttrCutDistance1)->Value();
            offsetDistances_[0] = data->CustomDoubleAttributeByName(
                zhihui_cairban::kAttrOffsetDistance0)->Value();
            offsetDistances_[1] = data->CustomDoubleAttributeByName(
                zhihui_cairban::kAttrOffsetDistance1)->Value();
            loadedDimensionValues = true;
        }
        catch (...)
        {
            // Features created before dimension handles used the single
            // extension value; offset defaults are resolved from thickness.
        }
        dimensionValuesInitialized_ = loadedDimensionValues;
        if (cutDimension0_ != nullptr && cutDimension1_ != nullptr &&
            offsetDimension0_ != nullptr && offsetDimension1_ != nullptr)
        {
            cutDimension0_->SetFormula(Number(cutDistances_[0]).c_str());
            cutDimension0_->SetValue(cutDistances_[0]);
            cutDimension1_->SetFormula(Number(cutDistances_[1]).c_str());
            cutDimension1_->SetValue(cutDistances_[1]);
            offsetDimension0_->SetFormula(Number(offsetDistances_[0]).c_str());
            offsetDimension0_->SetValue(offsetDistances_[0]);
            offsetDimension1_->SetFormula(Number(offsetDistances_[1]).c_str());
            offsetDimension1_->SetValue(offsetDistances_[1]);
        }
        zhihui_dialog_memory::TrySetEnum(
            wrapMode_,
            static_cast<int>(data->CustomDoubleAttributeByName(
                zhihui_cairban::kAttrWrapMode)->Value()));
        UpdateColorControlVisibility();
    }
    catch (...)
    {
        loadingEditedFeature_ = false;
        throw;
    }
    loadingEditedFeature_ = false;
}

bool CaiRBanDialog::CommitEditableCustomFeature(std::string& error)
{
    NXOpen::Part* workPart = session_->Parts()->Work();
    const bool editing = editedFeature_ != nullptr;
    NXOpen::Body* targetBody = nullptr;
    if (!editing && previewTargetBodyTag_ != NULL_TAG &&
        UF_OBJ_ask_status(previewTargetBodyTag_) == UF_OBJ_ALIVE)
    {
        targetBody = dynamic_cast<NXOpen::Body*>(
            NXOpen::NXObjectManager::Get(previewTargetBodyTag_));
    }
    if (workPart == nullptr || featureClass_ == nullptr ||
        (!editing && targetBody == nullptr))
    {
        error = featureClass_ == nullptr
                    ? "拆圆弧板可编辑特征未注册，部署后需重启 UG。"
                    : "没有可用的目标体。";
        return false;
    }

    NXOpen::Features::CustomFeatureBuilder* builder = nullptr;
    try
    {
        builder = workPart->Features()->CreateCustomFeatureBuilder(editedFeature_);
        NXOpen::Features::CustomFeatureData* data = nullptr;
        if (!editing)
        {
            NXOpen::Features::CustomAttributeCollection* attributes =
                workPart->Features()->CustomAttributeCollection();
            std::vector<NXOpen::Features::CustomAttribute*> values;
            const std::vector<NXOpen::Features::CustomAttribute::Property>
                required{NXOpen::Features::CustomAttribute::PropertyMandatoryInput};
            std::vector<NXOpen::Features::CustomAttribute::Property>
                targetProperties = required;
            targetProperties.push_back(
                NXOpen::Features::CustomAttribute::PropertyIsReferencingTargetBody);
            values.push_back(attributes->CreateCustomTagAttribute(
                zhihui_cairban::kAttrTargetBody, targetProperties));
            values.push_back(attributes->CreateCustomTagAttribute(
                zhihui_cairban::kAttrSelectedFace,
                std::vector<NXOpen::Features::CustomAttribute::Property>()));
            const std::vector<NXOpen::Features::CustomAttribute::Property> optional;
            values.push_back(attributes->CreateCustomDoubleAttribute(
                zhihui_cairban::kAttrRecolorEnabled, optional));
            values.push_back(attributes->CreateCustomDoubleAttribute(
                zhihui_cairban::kAttrColorMode, optional));
            values.push_back(attributes->CreateCustomDoubleAttribute(
                zhihui_cairban::kAttrFixedColor, optional));
            values.push_back(attributes->CreateCustomDoubleAttribute(
                zhihui_cairban::kAttrAppliedColor, optional));
            values.push_back(attributes->CreateCustomDoubleAttribute(
                zhihui_cairban::kAttrExtensionLength, optional));
            values.push_back(attributes->CreateCustomDoubleAttribute(
                zhihui_cairban::kAttrCutDistance0, optional));
            values.push_back(attributes->CreateCustomDoubleAttribute(
                zhihui_cairban::kAttrCutDistance1, optional));
            values.push_back(attributes->CreateCustomDoubleAttribute(
                zhihui_cairban::kAttrOffsetDistance0, optional));
            values.push_back(attributes->CreateCustomDoubleAttribute(
                zhihui_cairban::kAttrOffsetDistance1, optional));
            values.push_back(attributes->CreateCustomDoubleAttribute(
                zhihui_cairban::kAttrWrapMode, optional));
            data = workPart->Features()->CustomFeatureDataCollection()->CreateData(
                featureClass_, values);
            data->CustomTagAttributeByName(
                zhihui_cairban::kAttrTargetBody)
                ->SetValue(targetBody);
            if (previewSelectedFaceTag_ != NULL_TAG &&
                UF_OBJ_ask_status(previewSelectedFaceTag_) == UF_OBJ_ALIVE)
            {
                data->CustomTagAttributeByName(
                        zhihui_cairban::kAttrSelectedFace)
                    ->SetValue(NXOpen::NXObjectManager::Get(
                        previewSelectedFaceTag_));
            }
        }
        else
        {
            data = editedFeature_->FeatureData();
        }

        // A new feature has already been coloured by the live preview.  Reuse
        // that exact colour when it is committed; otherwise random mode would
        // draw a second colour here and visibly change on Apply.
        const bool recolor =
            useCachedParameters_ ? cachedRecolor_ : RecolorEnabled();
        const int appliedColor = useCachedParameters_
            ? cachedAppliedColor_
            : (recolor
                   ? (!editing && hasPreview_ && previewColor_ >= 1
                          ? previewColor_
                          : RequestedColor())
                   : 0);
        data->CustomDoubleAttributeByName(
                zhihui_cairban::kAttrRecolorEnabled)
            ->SetValue(recolor ? 1.0 : 0.0);
        data->CustomDoubleAttributeByName(zhihui_cairban::kAttrColorMode)
            ->SetValue(static_cast<double>(
                useCachedParameters_ ? cachedColorMode_ : ColorMode()));
        data->CustomDoubleAttributeByName(zhihui_cairban::kAttrFixedColor)
            ->SetValue(static_cast<double>(
                useCachedParameters_ ? cachedFixedColor_ : FixedColor()));
        data->CustomDoubleAttributeByName(zhihui_cairban::kAttrAppliedColor)
            ->SetValue(static_cast<double>(appliedColor));
        data->CustomDoubleAttributeByName(
                zhihui_cairban::kAttrExtensionLength)
            ->SetValue(useCachedParameters_ ? cachedExtension_
                                            : ExtensionLength());
        data->CustomDoubleAttributeByName(zhihui_cairban::kAttrCutDistance0)
            ->SetValue(cutDistances_[0]);
        data->CustomDoubleAttributeByName(zhihui_cairban::kAttrCutDistance1)
            ->SetValue(cutDistances_[1]);
        data->CustomDoubleAttributeByName(zhihui_cairban::kAttrOffsetDistance0)
            ->SetValue(offsetDistances_[0]);
        data->CustomDoubleAttributeByName(zhihui_cairban::kAttrOffsetDistance1)
            ->SetValue(offsetDistances_[1]);
        data->CustomDoubleAttributeByName(zhihui_cairban::kAttrWrapMode)
            ->SetValue(static_cast<double>(
                useCachedParameters_ ? cachedWrapMode_ : WrapMode()));

        // Edit must adopt the new preview chain. Leaving this false keeps the
        // original construction features, so parameter changes never appear.
        buildingCustomFeature_ = true;
        builder->SetFeatureData(data);
        NXOpen::Features::Feature* committed = builder->CommitFeature();
        builder->Destroy();
        builder = nullptr;
        buildingCustomFeature_ = false;
        NXOpen::Features::CustomFeature* customFeature =
            dynamic_cast<NXOpen::Features::CustomFeature*>(committed);
        if (customFeature == nullptr)
        {
            error = "NX 未返回拆圆弧板自定义特征。";
            return false;
        }
        customFeature->SetName(zhihui_cairban::kFeatureDisplayName);
        if (editing)
        {
            // The commit replaced the feature, so the tag must follow the new
            // node. Leaving editedFeatureTag_ on the deleted feature made every
            // later lookup by tag return null (NX returns a fresh tag here).
            editedFeature_ = customFeature;
            editedFeatureTag_ = customFeature->Tag();
        }
        return true;
    }
    catch (const NXOpen::NXException& ex)
    {
        buildingCustomFeature_ = false;
        if (builder != nullptr) builder->Destroy();
        error = ex.Message();
    }
    catch (const std::exception& ex)
    {
        buildingCustomFeature_ = false;
        if (builder != nullptr) builder->Destroy();
        error = ex.what();
    }
    return false;
}

int CaiRBanDialog::BuildCustomFeatureConstruction(
    NXOpen::Features::CustomFeaturePreUpdateEvent* event)
{
    if (event == nullptr || event->GetCustomFeature() == nullptr)
    {
        return 1;
    }

    std::vector<NXOpen::Features::ConstructionFeatureData*> construction =
        event->GetConstructionFeatures();
    // While the edit dialog has closed and the replacement has not started,
    // keep the existing construction. Replacing it here crashes NX.
    if (rebuildingEditedFeature_ || gDeferredDialog != nullptr)
    {
        if (!construction.empty())
        {
            event->SetConstructionFeatures(construction);
        }
        return 0;
    }
    if (buildingCustomFeature_ && hasPreview_ &&
        !previewCreatedFeatureTags_.empty())
    {
        // Editing replaces the previous internal chain. Creating starts empty.
        construction.clear();
        std::set<tag_t> seen;
        for (tag_t featureTag : previewCreatedFeatureTags_)
        {
            if (featureTag == NULL_TAG || !seen.insert(featureTag).second ||
                UF_OBJ_ask_status(featureTag) != UF_OBJ_ALIVE)
            {
                continue;
            }
            NXOpen::Features::Feature* feature =
                dynamic_cast<NXOpen::Features::Feature*>(
                    NXOpen::NXObjectManager::Get(featureTag));
            if (feature == nullptr || feature->IsInternal())
            {
                continue;
            }
            NXOpen::Features::ConstructionFeatureData* item =
                event->CreateConstructionFeatureData(feature);
            item->SetShowInGraphicView(true);
            construction.push_back(item);
        }
    }
    if (construction.empty())
    {
        return 1;
    }

    NXOpen::Features::CustomFeatureData* data =
        event->GetCustomFeature()->FeatureData();
    const bool recolor =
        data->CustomDoubleAttributeByName(
                zhihui_cairban::kAttrRecolorEnabled)
                ->Value() > 0.5;
    const int color = static_cast<int>(
        data->CustomDoubleAttributeByName(
                zhihui_cairban::kAttrAppliedColor)
            ->Value());
    if (recolor && color >= 1 && color <= 216)
    {
        // The final construction is the commit-time subtract. Its body is the
        // target remainder; the retained tool is produced by the preceding
        // thicken/trim/offset feature and is the body that should be coloured.
        auto item = construction.rbegin();
        if (item != construction.rend())
        {
            ++item;
        }
        for (; item != construction.rend(); ++item)
        {
            NXOpen::Features::Feature* resultFeature = (*item)->GetFeature();
            if (resultFeature != nullptr && !resultFeature->GetBodies().empty())
            {
                UF_OBJ_set_color(resultFeature->GetBodies().front()->Tag(),
                                 color);
                break;
            }
        }
    }
    for (NXOpen::Features::ConstructionFeatureData* item : construction)
    {
        item->SetShowInGraphicView(true);
    }
    event->SetConstructionFeatures(construction);
    if (buildingCustomFeature_ && hasPreview_)
    {
        CommitPreview();
        previewCreatedFeatureTags_.clear();
    }
    return 0;
}

void CaiRBanDialog::CaptureEditParameters()
{
    if (NXOpen::Face* face = SelectedFace())
    {
        previewSelectedFaceTag_ = face->Tag();
        dimensionFaceTag_ = face->Tag();
        if (face->GetBody() != nullptr)
        {
            previewTargetBodyTag_ = face->GetBody()->Tag();
        }
    }
    if (cutDimension0_ != nullptr && cutDimension1_ != nullptr &&
        offsetDimension0_ != nullptr && offsetDimension1_ != nullptr)
    {
        cutDistances_[0] = (std::max)(0.0, cutDimension0_->Value());
        cutDistances_[1] = (std::max)(0.0, cutDimension1_->Value());
        offsetDistances_[0] = offsetDimension0_->Value();
        offsetDistances_[1] = offsetDimension1_->Value();
    }
    cachedRecolor_ = RecolorEnabled();
    cachedColorMode_ = ColorMode();
    cachedFixedColor_ = FixedColor();
    cachedAppliedColor_ = cachedRecolor_ ? RequestedColor() : 0;
    cachedExtension_ = ExtensionLength();
    cachedWrapMode_ = WrapMode();
}

void CaiRBanDialog::RebuildEditedFeature()
{
    if (!FeatureAlive(editedFeatureTag_))
    {
        throw std::runtime_error("找不到要编辑的拆圆弧板特征。");
    }
    auto* node = dynamic_cast<NXOpen::Features::CustomFeature*>(
        NXOpen::NXObjectManager::Get(editedFeatureTag_));
    if (node == nullptr)
    {
        throw std::runtime_error("找不到要编辑的拆圆弧板特征。");
    }

    // The original face stays alive while the dialog closes. Put the committed
    // feature back only after Block Styler has released that selection.
    rebuildingEditedFeature_ = true;
    RestoreEditRollback();
    rebuildingEditedFeature_ = false;
    if (!FeatureAlive(editedFeatureTag_))
    {
        throw std::runtime_error("找不到要编辑的拆圆弧板特征。");
    }
    node = dynamic_cast<NXOpen::Features::CustomFeature*>(
        NXOpen::NXObjectManager::Get(editedFeatureTag_));
    if (node == nullptr)
    {
        throw std::runtime_error("找不到要编辑的拆圆弧板特征。");
    }

    const std::vector<tag_t> recordedFaces = CollectEditedInputFaces(node);

    AppendDebugLog(FeatureTreeSnapshot("RebuildEditedFeature BEFORE delete"));
    std::vector<tag_t> oldChain;
    try
    {
        for (NXOpen::Features::ConstructionFeatureData* item :
             node->GetConstructionFeatures())
        {
            NXOpen::Features::Feature* feature =
                item == nullptr ? nullptr : item->GetFeature();
            if (feature != nullptr && FeatureAlive(feature->Tag()))
            {
                oldChain.push_back(feature->Tag());
            }
        }
    }
    catch (...)
    {
    }
    {
        std::string chain;
        for (tag_t tag : oldChain)
        {
            if (!chain.empty())
            {
                chain += ",";
            }
            chain += std::to_string(tag);
        }
        AppendDebugLog("RebuildEditedFeature old construction chain size=" +
                       std::to_string(oldChain.size()) + " tags=[" + chain + "]");
    }

    // Undo marks are NOT stable here. This code runs from a timer after the
    // edit dialog closed, and NX recycles marks exactly in that window
    // ("UNDO_UG: Recyclable status of mark N set to True" appears immediately
    // before the 0xe0434352 termination). A mark held across the whole
    // delete + rebuild + commit sequence can therefore be dead by the time it
    // is handed to DoUpdate, which surfaces as NXException 3615009
    // ("错误的标记"). Take a fresh mark immediately before each use, verify it,
    // and never let one escape into a long-lived variable.
    // Roll the model back to the position the edited feature occupied BEFORE
    // deleting it. A feature created while the model sits at that point takes
    // the old feature's place in the history instead of being appended to the
    // end, so the tree keeps its order and the timestamps stay right.
    // EditWithRollbackManager is NX's own mechanism for exactly this:
    // "will roll the model to a suitable point before feature edit can begin"
    // and Stop() "rolls forward the model to the original current feature".
    NXOpen::Features::EditWithRollbackManager* rollback = nullptr;
    NXOpen::Features::Feature* rollbackInsertion = nullptr;
    try
    {
        int boundary = node->Timestamp();
        try
        {
            for (NXOpen::Features::ConstructionFeatureData* item :
                 node->GetConstructionFeatures())
            {
                NXOpen::Features::Feature* feature =
                    item == nullptr ? nullptr : item->GetFeature();
                if (feature != nullptr)
                {
                    boundary = (std::min)(boundary, feature->Timestamp());
                }
            }
        }
        catch (...)
        {
        }
        for (NXOpen::Features::Feature* feature :
             session_->Parts()->Work()->Features()->GetFeatures())
        {
            if (feature == nullptr || feature->IsInternal() ||
                feature->Tag() == editedFeatureTag_ ||
                feature->Timestamp() >= boundary)
            {
                continue;
            }
            if (rollbackInsertion == nullptr ||
                feature->Timestamp() > rollbackInsertion->Timestamp())
            {
                rollbackInsertion = feature;
            }
        }
    }
    catch (...)
    {
        rollbackInsertion = nullptr;
    }

    NXOpen::Session::UndoMarkId rollbackMark =
        static_cast<NXOpen::Session::UndoMarkId>(0);
    try
    {
        rollbackMark = session_->SetUndoMark(
            NXOpen::Session::MarkVisibilityVisible, "编辑拆圆弧板-回滚");
        rollback = session_->Parts()->Work()->Features()
                       ->StartEditWithRollbackManager(node, rollbackMark);
        // One compact line: where the model was rolled back to. Enough to tell
        // whether a later failure happened before or after the rollback.
        AppendDebugLog("rebuild rolled back to insertion=" +
                       std::to_string(rollbackInsertion == nullptr
                                           ? NULL_TAG
                                           : rollbackInsertion->Tag()) +
                       " currentNow=" +
                       std::to_string(session_->Parts()->Work()->CurrentFeature() ==
                                              nullptr
                                          ? NULL_TAG
                                          : session_->Parts()
                                                ->Work()
                                                ->CurrentFeature()
                                                ->Tag()));
    }
    catch (const NXOpen::NXException& ex)
    {
        AppendDebugLog("RebuildEditedFeature StartEditWithRollbackManager threw code=" +
                       std::to_string(ex.ErrorCode()) + " message=" +
                       (ex.Message() != nullptr ? ex.Message() : "(null)"));
        rollback = nullptr;
    }

    NXOpen::Session::UndoMarkId deleteMark =
        static_cast<NXOpen::Session::UndoMarkId>(0);
    NXOpen::Session::UndoMarkId commitMark =
        static_cast<NXOpen::Session::UndoMarkId>(0);
    const bool rollbackActive = rollback != nullptr;
    rebuildingEditedFeature_ = true;
    try
    {
        deleteMark = session_->SetUndoMark(
            NXOpen::Session::MarkVisibilityVisible, "编辑拆圆弧板-删除");
        AppendDebugLog("RebuildEditedFeature deleteMark=" +
                       std::to_string(deleteMark) + " exists=" +
                       std::to_string(session_->DoesUndoMarkExist(
                                          deleteMark, "编辑拆圆弧板-删除")
                                          ? 1
                                          : 0));
        std::vector<NXOpen::TaggedObject*> doomed;
        for (auto item = oldChain.rbegin(); item != oldChain.rend(); ++item)
        {
            NXOpen::TaggedObject* object =
                NXOpen::NXObjectManager::Get(*item);
            if (object != nullptr)
            {
                doomed.push_back(object);
            }
        }
        doomed.push_back(node);
        NXOpen::Update* update = session_->UpdateManager();
        update->ClearDeleteList();
        update->ClearErrorList();
        update->AddObjectsToDeleteList(doomed);
        int deleteErrors = 0;
        try
        {
            deleteErrors = update->DoUpdate(deleteMark);
        }
        catch (const NXOpen::NXException& ex)
        {
            AppendDebugLog(
                "RebuildEditedFeature delete DoUpdate threw code=" +
                std::to_string(ex.ErrorCode()) + " message=" +
                (ex.Message() != nullptr ? ex.Message() : "(null)") +
                " markExists=" +
                std::to_string(session_->DoesUndoMarkExist(
                                   deleteMark, "编辑拆圆弧板-删除")
                                   ? 1
                                   : 0));
            throw;
        }
        rebuildingEditedFeature_ = false;
        AppendDebugLog("RebuildEditedFeature DoUpdate deleteErrors=" +
                       std::to_string(deleteErrors));
        {
            int stillAlive = 0;
            std::string alive;
            for (tag_t tag : oldChain)
            {
                if (FeatureAlive(tag))
                {
                    ++stillAlive;
                    alive += " " + std::to_string(tag);
                }
            }
            AppendDebugLog("RebuildEditedFeature old construction still alive=" +
                           std::to_string(stillAlive) + "/" +
                           std::to_string(oldChain.size()) + alive);
        }
        if (deleteErrors != 0 || FeatureAlive(editedFeatureTag_))
        {
            throw std::runtime_error("删除旧的拆圆弧板特征失败。");
        }

        NXOpen::Face* face = AliveCylindricalFace(recordedFaces);
        if (face == nullptr || face->GetBody() == nullptr)
        {
            throw std::runtime_error(
                "无法读取已保存的圆弧面，不能编辑该特征。");
        }

        double diameter = 0.0;
        double thickness = 0.0;
        bool reverseThicken = false;
        std::string error;
        if (!EstimateCylinderThickness(face->GetBody(), face, diameter,
                                       thickness, reverseThicken,
                                       cachedWrapMode_, error))
        {
            throw std::runtime_error(error);
        }
        previewCreatedFeatureTags_.clear();
        previewSubtractCreated_ = false;
        previewTargetBodyTag_ = face->GetBody()->Tag();
        previewSelectedFaceTag_ = face->Tag();
        hasPreview_ = false;
        if (!CreateArcPanel(face, thickness, reverseThicken, cutDistances_,
                            offsetDistances_, cachedWrapMode_, error,
                            cachedRecolor_, cachedAppliedColor_,
                            &previewCreatedFeatureTags_, nullptr, nullptr))
        {
            throw std::runtime_error(error);
        }
        hasPreview_ = true;
        if (!CommitPreviewSubtract(error))
        {
            throw std::runtime_error(error);
        }
        editedFeature_ = nullptr;
        useCachedParameters_ = true;
        buildingCustomFeature_ = true;
        // Fresh mark for the rebuild, taken immediately before the commit that
        // consumes it - see the note on deleteMark above.
        commitMark = session_->SetUndoMark(
            NXOpen::Session::MarkVisibilityVisible, "编辑拆圆弧板-重建");
        AppendDebugLog("RebuildEditedFeature commitMark=" +
                       std::to_string(commitMark) + " exists=" +
                       std::to_string(session_->DoesUndoMarkExist(
                                          commitMark, "编辑拆圆弧板-重建")
                                          ? 1
                                          : 0));
        const bool committed = CommitEditableCustomFeature(error);
        useCachedParameters_ = false;
        buildingCustomFeature_ = false;
        if (!committed)
        {
            throw std::runtime_error(
                error.empty() ? "编辑拆圆弧板失败。" : error);
        }
        if (commitMark != static_cast<NXOpen::Session::UndoMarkId>(0))
        {
            AppendDebugLog("RebuildEditedFeature commitMark before update exists=" +
                           std::to_string(session_->DoesUndoMarkExist(
                                              commitMark, "编辑拆圆弧板-重建")
                                              ? 1
                                              : 0));
            if (session_->DoesUndoMarkExist(commitMark, "编辑拆圆弧板-重建"))
            {
                try
                {
                    NXOpen::Update* commitUpdate = session_->UpdateManager();
                    if (commitUpdate != nullptr)
                    {
                        commitUpdate->ClearDeleteList();
                        commitUpdate->ClearErrorList();
                        const int commitErrors =
                            commitUpdate->DoUpdate(commitMark);
                        AppendDebugLog("RebuildEditedFeature commit DoUpdate errors=" +
                                       std::to_string(commitErrors));
                    }
                }
                catch (const NXOpen::NXException& commitEx)
                {
                    AppendDebugLog("RebuildEditedFeature commit DoUpdate threw code=" +
                                   std::to_string(commitEx.ErrorCode()) +
                                   " message=" +
                                   (commitEx.Message() != nullptr
                                        ? commitEx.Message()
                                        : "(null)"));
                }
            }
            else
            {
                AppendDebugLog("RebuildEditedFeature commit update skipped: mark gone");
            }
        }
        hasPreview_ = false;
        previewCreatedFeatureTags_.clear();
        previewUndoMark_ = static_cast<NXOpen::Session::UndoMarkId>(0);
        previewSubtractCreated_ = false;
        AppendDebugLog(FeatureTreeSnapshot("RebuildEditedFeature AFTER commit"));
        // Release the rollback: Stop() rolls the model forward to the original
        // current feature, Destroy() frees the transient object.
        if (rollback != nullptr)
        {
            bool stopOk = false;
            try
            {
                rollback->UpdateFeature(false);
                rollback->Stop();
                stopOk = true;
                AppendDebugLog("RebuildEditedFeature rollback->Stop ok, currentNow=" +
                               std::to_string(
                                   session_->Parts()->Work()->CurrentFeature() == nullptr
                                       ? NULL_TAG
                                       : session_->Parts()->Work()->CurrentFeature()->Tag()));
            }
            catch (const NXOpen::NXException& rbEx)
            {
                // Stop() can fail to restore the pre-edit current feature after
                // the replacement, which NX reports as "设为当前特征时出错 /
                // Unable to replace current feature". That must not leave the
                // model rolled back, so fall through to the explicit
                // roll-forward below.
                AppendDebugLog("RebuildEditedFeature rollback->Stop threw code=" +
                               std::to_string(rbEx.ErrorCode()) + " message=" +
                               (rbEx.Message() != nullptr ? rbEx.Message() : "(null)"));
            }
            try
            {
                rollback->Destroy();
            }
            catch (...)
            {
            }
            rollback = nullptr;

            // Explicit roll-forward: put the model back at the end of the
            // feature tree (the newest feature), which is the state a finished
            // edit must leave behind.
            NXOpen::Part* workPart = session_->Parts()->Work();
            NXOpen::Features::Feature* newest = nullptr;
            try
            {
                for (NXOpen::Features::Feature* feature :
                     workPart->Features()->GetFeatures())
                {
                    if (feature == nullptr || !FeatureAlive(feature->Tag()))
                    {
                        continue;
                    }
                    if (newest == nullptr ||
                        feature->Timestamp() > newest->Timestamp())
                    {
                        newest = feature;
                    }
                }
            }
            catch (...)
            {
                newest = nullptr;
            }
            NXOpen::Features::Feature* currentBeforeRollback = nullptr;
            if (FeatureAlive(currentFeatureBeforeEdit_))
            {
                currentBeforeRollback = dynamic_cast<NXOpen::Features::Feature*>(
                    NXOpen::NXObjectManager::Get(currentFeatureBeforeEdit_));
            }
            const tag_t target = newest != nullptr
                ? newest->Tag()
                : (currentBeforeRollback != nullptr
                       ? currentBeforeRollback->Tag()
                       : NULL_TAG);
            if (!stopOk && target != NULL_TAG)
            {
                try
                {
                    const int rollForward = UF_MODL_set_current_feature(target);
                    AppendDebugLog("RebuildEditedFeature explicit roll-forward target=" +
                                   std::to_string(target) + " status=" +
                                   std::to_string(rollForward) + " currentNow=" +
                                   std::to_string(
                                       workPart->CurrentFeature() == nullptr
                                           ? NULL_TAG
                                           : workPart->CurrentFeature()->Tag()));
                }
                catch (const NXOpen::NXException& fwdEx)
                {
                    AppendDebugLog("RebuildEditedFeature explicit roll-forward threw code=" +
                                   std::to_string(fwdEx.ErrorCode()));
                }
            }
        }
        AppendDebugLog(FeatureTreeSnapshot("RebuildEditedFeature AFTER reorder"));
    }
    catch (const NXOpen::NXException& ex)
    {
        rebuildingEditedFeature_ = false;
        useCachedParameters_ = false;
        buildingCustomFeature_ = false;
        hasPreview_ = false;
        previewCreatedFeatureTags_.clear();
        previewUndoMark_ = static_cast<NXOpen::Session::UndoMarkId>(0);
        if (rollback != nullptr)
        {
            try
            {
                rollback->UpdateFeature(true);
                rollback->Stop();
                rollback->Destroy();
            }
            catch (...)
            {
            }
            rollback = nullptr;
        }
        AppendDebugLog("RebuildEditedFeature failed NXException code=" +
                       std::to_string(ex.ErrorCode()) + " message=" +
                       (ex.Message() != nullptr ? ex.Message() : "(null)") +
                       " deleteMarkExists=" +
                       std::to_string(
                           deleteMark !=
                                       static_cast<
                                           NXOpen::Session::UndoMarkId>(0) &&
                                   session_->DoesUndoMarkExist(
                                       deleteMark, "编辑拆圆弧板-删除")
                               ? 1
                               : 0));
        // Only roll back when the mark is still valid; a recycled mark makes
        // UndoToMark throw the very "错误的标记" error we are fixing here.
        if (deleteMark != static_cast<NXOpen::Session::UndoMarkId>(0) &&
            session_->DoesUndoMarkExist(deleteMark, "编辑拆圆弧板-删除"))
        {
            try
            {
                session_->UndoToMark(deleteMark, "编辑拆圆弧板失败");
            }
            catch (const NXOpen::NXException& undoEx)
            {
                AppendDebugLog("RebuildEditedFeature rollback failed code=" +
                               std::to_string(undoEx.ErrorCode()));
            }
        }
        else
        {
            AppendDebugLog("RebuildEditedFeature rollback skipped: mark gone");
        }
        throw;
    }
    catch (...)
    {
        rebuildingEditedFeature_ = false;
        useCachedParameters_ = false;
        buildingCustomFeature_ = false;
        hasPreview_ = false;
        previewCreatedFeatureTags_.clear();
        previewUndoMark_ = static_cast<NXOpen::Session::UndoMarkId>(0);
        if (rollback != nullptr)
        {
            try
            {
                rollback->UpdateFeature(true);
                rollback->Stop();
                rollback->Destroy();
            }
            catch (...)
            {
            }
            rollback = nullptr;
        }
        AppendDebugLog("RebuildEditedFeature failed non-NX exception");
        if (deleteMark != static_cast<NXOpen::Session::UndoMarkId>(0) &&
            session_->DoesUndoMarkExist(deleteMark, "编辑拆圆弧板-删除"))
        {
            try
            {
                session_->UndoToMark(deleteMark, "编辑拆圆弧板失败");
            }
            catch (...)
            {
            }
        }
        throw;
    }
}

int CaiRBanDialog::apply_cb()
{
    SaveDialogState();
    if (editedFeature_ != nullptr || FeatureAlive(editedFeatureTag_))
    {
        try
        {
            CaptureEditParameters();
            const tag_t keptFace = previewSelectedFaceTag_;
            const tag_t keptBody = previewTargetBodyTag_;
            ReleaseFaceSelection();
            // Put the model's current feature back before NX closes its own
            // edit session. Leaving the dialog-open rollback in place makes NX
            // fail "Unable to replace current feature" (Error code 1,
            // ModlFeature_CustomFeatureData.cxx) and crash right after OK.
            RestoreEditRollback();
            UndoPreview();
            previewSelectedFaceTag_ = keptFace;
            dimensionFaceTag_ = keptFace;
            previewTargetBodyTag_ = keptBody;
            editRebuildPending_ = true;
            return 0;
        }
        catch (const NXOpen::NXException& ex)
        {
            ShowError(ex.Message() != nullptr ? ex.Message()
                                             : "编辑拆圆弧板失败。");
            return 1;
        }
        catch (const std::exception& ex)
        {
            ShowError(ex.what());
            return 1;
        }
    }
    if (SelectedFace() == nullptr)
    {
        ShowError("请选择一个圆柱面。");
        return 1;
    }
    if (!hasPreview_)
    {
        const int previewResult = CreatePreview();
        if (previewResult != 0)
        {
            return previewResult;
        }
    }
    std::string error;
    if (!CommitPreviewSubtract(error))
    {
        ShowError(error);
        return 1;
    }
    if (!CommitEditableCustomFeature(error))
    {
        ShowError(error);
        return 1;
    }
    NXOpen::BlockStyler::PropertyList* properties =
        faceSelect_->GetProperties();
    loadingEditedFeature_ = true;
    properties->SetTaggedObjectVector(
        "SelectedObjects", std::vector<NXOpen::TaggedObject*>());
    loadingEditedFeature_ = false;
    delete properties;
    previewTargetBodyTag_ = NULL_TAG;
    previewSelectedFaceTag_ = NULL_TAG;
    previewColor_ = 0;
    previewSubtractCreated_ = false;
    dimensionValuesInitialized_ = false;
    dimensionFaceTag_ = NULL_TAG;
    return 0;
}

int CaiRBanDialog::ok_cb()
{
    return apply_cb();
}

int CaiRBanDialog::cancel_cb()
{
    ReleaseFaceSelection();
    UndoPreview();
    return 0;
}

int CaiRBanDialog::CreatePreview()
{
    NXOpen::Face* face = SelectedFace();
    if (face == nullptr)
    {
        return 0;
    }

    previewUndoMark_ = session_->SetUndoMark(
        NXOpen::Session::MarkVisibilityInvisible,
        "拆圆弧板预览");
    if (previewTargetBodyTag_ == NULL_TAG ||
        UF_OBJ_ask_status(previewTargetBodyTag_) != UF_OBJ_ALIVE)
    {
        previewTargetBodyTag_ = face->GetBody()->Tag();
    }
    previewSelectedFaceTag_ = face->Tag();
    std::string error;
    double diameter = 0.0;
    double thickness = 0.0;
    bool reverseThicken = false;
    if (!EstimateCylinderThickness(face->GetBody(), face, diameter, thickness,
                                   reverseThicken, WrapMode(), error))
    {
        UndoPreview();
        ShowError(error);
        return 1;
    }
    if (!dimensionValuesInitialized_ || dimensionFaceTag_ != face->Tag())
    {
        cutDistances_[0] = ExtensionLength();
        cutDistances_[1] = ExtensionLength();
        const double defaultOffset = WrapMode() == 1 ? -thickness : 0.0;
        offsetDistances_[0] = defaultOffset;
        offsetDistances_[1] = defaultOffset;
        dimensionValuesInitialized_ = true;
        dimensionFaceTag_ = face->Tag();
    }
    currentThickness_ = thickness;
    std::array<NXOpen::Point3d, 4> handleOrigins{};
    std::array<NXOpen::Vector3d, 4> handleDirections{};
    previewCreatedFeatureTags_.clear();
    previewSubtractCreated_ = false;
    previewColor_ = RecolorEnabled() ? RequestedColor() : 0;
    if (!CreateArcPanel(face, thickness, reverseThicken, cutDistances_,
                        offsetDistances_,
                        WrapMode(), error, RecolorEnabled(), previewColor_,
                        &previewCreatedFeatureTags_, &handleOrigins,
                        &handleDirections))
    {
        UndoPreview();
        ShowError(error);
        return 1;
    }
    NXOpen::Body* previewBody = nullptr;
    for (auto item = previewCreatedFeatureTags_.rbegin();
         item != previewCreatedFeatureTags_.rend(); ++item)
    {
        if (*item == NULL_TAG || UF_OBJ_ask_status(*item) != UF_OBJ_ALIVE)
        {
            continue;
        }
        NXOpen::Features::Feature* feature =
            dynamic_cast<NXOpen::Features::Feature*>(
                NXOpen::NXObjectManager::Get(*item));
        if (feature != nullptr && !feature->GetBodies().empty())
        {
            previewBody = feature->GetBodies().front();
            break;
        }
    }
    ApplyPreviewTranslucency(previewBody);
    ConfigureDimensionHandles(handleOrigins, handleDirections);
    hasPreview_ = true;
    return 0;
}

bool CaiRBanDialog::UndoPreview()
{
    RestorePreviewTranslucency();
    const std::vector<tag_t> previewFeatureTags =
        previewCreatedFeatureTags_;
    try
    {
        if (previewUndoMark_ !=
                static_cast<NXOpen::Session::UndoMarkId>(0) &&
            session_->DoesUndoMarkExist(previewUndoMark_,
                                        "拆圆弧板预览"))
        {
            session_->UndoToMark(previewUndoMark_, "拆圆弧板预览");
            if (session_->DoesUndoMarkExist(previewUndoMark_, "拆圆弧板预览"))
            {
                session_->DeleteUndoMark(previewUndoMark_, "拆圆弧板预览");
            }
        }
    }
    catch (...)
    {
    }

    // NX can recycle an invisible undo mark while Block Styler is processing
    // model updates. Never assume the old preview disappeared merely because
    // UndoToMark returned. Check every feature created by this preview and
    // remove any survivors in one dependency-aware update transaction.
    try
    {
        std::vector<NXOpen::TaggedObject*> survivingFeatures;
        std::set<tag_t> seen;
        for (auto item = previewFeatureTags.rbegin();
             item != previewFeatureTags.rend(); ++item)
        {
            const tag_t featureTag = *item;
            if (featureTag == NULL_TAG || !seen.insert(featureTag).second ||
                UF_OBJ_ask_status(featureTag) != UF_OBJ_ALIVE)
            {
                continue;
            }
            NXOpen::TaggedObject* object =
                dynamic_cast<NXOpen::TaggedObject*>(
                    NXOpen::NXObjectManager::Get(featureTag));
            if (object != nullptr)
            {
                survivingFeatures.push_back(object);
            }
        }

        if (!survivingFeatures.empty() && session_->UpdateManager() != nullptr)
        {
            const NXOpen::Session::UndoMarkId cleanupMark =
                session_->SetUndoMark(
                    NXOpen::Session::MarkVisibilityInvisible,
                    "拆圆弧板预览清理");
            NXOpen::Update* update = session_->UpdateManager();
            update->ClearDeleteList();
            update->ClearErrorList();
            update->AddObjectsToDeleteList(survivingFeatures);
            try
            {
                update->DoUpdate(cleanupMark);
            }
            catch (const NXOpen::NXException& ex)
            {
                // Previously swallowed by catch(...); log it so a recycled
                // cleanup mark is visible instead of silent.
                AppendDebugLog(
                    "UndoPreview cleanup DoUpdate threw code=" +
                    std::to_string(ex.ErrorCode()) + " message=" +
                    (ex.Message() != nullptr ? ex.Message() : "(null)") +
                    " markExists=" +
                    std::to_string(session_->DoesUndoMarkExist(
                                       cleanupMark, "拆圆弧板预览清理")
                                       ? 1
                                       : 0) +
                    " survivors=" + std::to_string(survivingFeatures.size()));
                throw;
            }
            if (session_->DoesUndoMarkExist(cleanupMark,
                                            "拆圆弧板预览清理"))
            {
                session_->DeleteUndoMark(cleanupMark,
                                         "拆圆弧板预览清理");
            }
        }
    }
    catch (...)
    {
        // State is reset below; CreatePreview will report any remaining
        // dependency conflict instead of silently committing stale tags.
    }

    bool allPreviewFeaturesRemoved = true;
    for (tag_t featureTag : previewFeatureTags)
    {
        if (featureTag != NULL_TAG &&
            UF_OBJ_ask_status(featureTag) == UF_OBJ_ALIVE)
        {
            allPreviewFeaturesRemoved = false;
            break;
        }
    }

    previewUndoMark_ = static_cast<NXOpen::Session::UndoMarkId>(0);
    if (!allPreviewFeaturesRemoved)
    {
        hasPreview_ = true;
        previewCreatedFeatureTags_ = previewFeatureTags;
        return false;
    }
    hasPreview_ = false;
    previewCreatedFeatureTags_.clear();
    previewColor_ = 0;
    previewSubtractCreated_ = false;
    previewTargetBodyTag_ = NULL_TAG;
    previewSelectedFaceTag_ = NULL_TAG;
    return true;
}

void CaiRBanDialog::CommitPreview()
{
    RestorePreviewTranslucency();
    if (!hasPreview_ ||
        previewUndoMark_ == static_cast<NXOpen::Session::UndoMarkId>(0))
    {
        return;
    }
    if (session_->DoesUndoMarkExist(previewUndoMark_, "拆圆弧板预览"))
    {
        session_->SetUndoMarkName(previewUndoMark_, "拆圆弧板");
        session_->SetUndoMarkVisibility(
            previewUndoMark_, "拆圆弧板",
            NXOpen::Session::MarkVisibilityVisible);
    }
    previewUndoMark_ = static_cast<NXOpen::Session::UndoMarkId>(0);
    hasPreview_ = false;
}

NXOpen::Face* CaiRBanDialog::SelectedFace() const
{
    if (faceSelect_ == nullptr)
    {
        return nullptr;
    }
    NXOpen::BlockStyler::PropertyList* properties = faceSelect_->GetProperties();
    const std::vector<NXOpen::TaggedObject*> objects =
        properties->GetTaggedObjectVector("SelectedObjects");
    delete properties;
    if (objects.size() == 1)
    {
        if (NXOpen::Face* face = dynamic_cast<NXOpen::Face*>(objects.front()))
        {
            return face;
        }
    }
    for (tag_t faceTag : {previewSelectedFaceTag_, dimensionFaceTag_})
    {
        if (faceTag == NULL_TAG || UF_OBJ_ask_status(faceTag) != UF_OBJ_ALIVE)
        {
            continue;
        }
        if (NXOpen::Face* face = dynamic_cast<NXOpen::Face*>(
                NXOpen::NXObjectManager::Get(faceTag)))
        {
            return face;
        }
    }
    return nullptr;
}

void CaiRBanDialog::ApplyPreviewTranslucency(NXOpen::Body* previewBody)
{
    RestorePreviewTranslucency();
    NXOpen::Part* workPart = session_->Parts()->Work();
    if (workPart == nullptr || workPart->Bodies() == nullptr ||
        previewBody == nullptr)
    {
        return;
    }

    constexpr int kPreviewTranslucency = 80;
    for (NXOpen::Body* body : *workPart->Bodies())
    {
        if (body == nullptr || body == previewBody ||
            body->Tag() == NULL_TAG ||
            UF_OBJ_ask_status(body->Tag()) != UF_OBJ_ALIVE)
        {
            continue;
        }
        UF_OBJ_translucency_t original = 0;
        if (UF_OBJ_ask_translucency(body->Tag(), &original) != 0)
        {
            continue;
        }
        previewBodyTranslucencies_.push_back(
            std::make_pair(body->Tag(), static_cast<int>(original)));
        UF_OBJ_set_translucency(
            body->Tag(),
            static_cast<UF_OBJ_translucency_t>(
                (std::max)(static_cast<int>(original),
                           kPreviewTranslucency)));
        body->RedisplayObject();
    }
}

void CaiRBanDialog::RestorePreviewTranslucency()
{
    for (const std::pair<tag_t, int>& state : previewBodyTranslucencies_)
    {
        if (state.first == NULL_TAG ||
            UF_OBJ_ask_status(state.first) != UF_OBJ_ALIVE)
        {
            continue;
        }
        UF_OBJ_set_translucency(
            state.first,
            static_cast<UF_OBJ_translucency_t>(state.second));
        NXOpen::Body* body = dynamic_cast<NXOpen::Body*>(
            NXOpen::NXObjectManager::Get(state.first));
        if (body != nullptr)
        {
            body->RedisplayObject();
        }
    }
    previewBodyTranslucencies_.clear();
}

bool CaiRBanDialog::CommitPreviewSubtract(std::string& error)
{
    if (previewSubtractCreated_)
    {
        return true;
    }

    NXOpen::Part* workPart = session_->Parts()->Work();
    NXOpen::Body* targetBody = nullptr;
    if (previewTargetBodyTag_ != NULL_TAG &&
        UF_OBJ_ask_status(previewTargetBodyTag_) == UF_OBJ_ALIVE)
    {
        targetBody = dynamic_cast<NXOpen::Body*>(
            NXOpen::NXObjectManager::Get(previewTargetBodyTag_));
    }

    NXOpen::Body* toolBody = nullptr;
    for (auto item = previewCreatedFeatureTags_.rbegin();
         item != previewCreatedFeatureTags_.rend(); ++item)
    {
        if (*item == NULL_TAG || UF_OBJ_ask_status(*item) != UF_OBJ_ALIVE)
        {
            continue;
        }
        NXOpen::Features::Feature* feature =
            dynamic_cast<NXOpen::Features::Feature*>(
                NXOpen::NXObjectManager::Get(*item));
        if (feature != nullptr && !feature->GetBodies().empty())
        {
            toolBody = feature->GetBodies().front();
            break;
        }
    }

    if (workPart == nullptr || targetBody == nullptr || toolBody == nullptr ||
        targetBody == toolBody)
    {
        error = "提交求差时未找到有效的目标体或保留工具体。";
        return false;
    }

    NXOpen::Features::BooleanBuilder* subtract = nullptr;
    try
    {
        subtract =
            workPart->Features()->CreateBooleanBuilderUsingCollector(nullptr);
        subtract->SetOperation(NXOpen::Features::Feature::BooleanTypeSubtract);
        subtract->SetRetainTarget(false);
        subtract->SetRetainTool(true);

        NXOpen::SelectionIntentRuleOptions* targetOptions =
            workPart->ScRuleFactory()->CreateRuleOptions();
        targetOptions->SetSelectedFromInactive(false);
        NXOpen::BodyDumbRule* targetRule =
            workPart->ScRuleFactory()->CreateRuleBodyDumb(
                std::vector<NXOpen::Body*>{targetBody}, true, targetOptions);
        delete targetOptions;
        NXOpen::ScCollector* targetCollector =
            workPart->ScCollectors()->CreateCollector();
        targetCollector->ReplaceRules(
            std::vector<NXOpen::SelectionIntentRule*>{targetRule}, false);
        subtract->SetTargetBodyCollector(targetCollector);

        NXOpen::SelectionIntentRuleOptions* toolOptions =
            workPart->ScRuleFactory()->CreateRuleOptions();
        toolOptions->SetSelectedFromInactive(false);
        NXOpen::BodyDumbRule* toolRule =
            workPart->ScRuleFactory()->CreateRuleBodyDumb(
                std::vector<NXOpen::Body*>{toolBody}, true, toolOptions);
        delete toolOptions;
        NXOpen::ScCollector* toolCollector =
            workPart->ScCollectors()->CreateCollector();
        toolCollector->ReplaceRules(
            std::vector<NXOpen::SelectionIntentRule*>{toolRule}, false);
        subtract->SetToolBodyCollector(toolCollector);

        NXOpen::Features::Feature* subtractFeature =
            dynamic_cast<NXOpen::Features::Feature*>(subtract->Commit());
        subtract->Destroy();
        subtract = nullptr;
        if (subtractFeature == nullptr)
        {
            error = "NX 未返回提交求差特征。";
            return false;
        }
        previewCreatedFeatureTags_.push_back(subtractFeature->Tag());
        previewSubtractCreated_ = true;
        return true;
    }
    catch (const NXOpen::NXException& ex)
    {
        if (subtract != nullptr)
        {
            subtract->Destroy();
        }
        error = "提交时求差失败：" +
                std::string(ex.Message() != nullptr ? ex.Message()
                                                    : "NXOpen 布尔求差失败。");
    }
    catch (const std::exception& ex)
    {
        if (subtract != nullptr)
        {
            subtract->Destroy();
        }
        error = "提交时求差失败：" + std::string(ex.what());
    }
    return false;
}

bool CaiRBanDialog::EstimateThicknessAndInnerNormal(
    NXOpen::Body* body,
    NXOpen::Face* face,
    double& thickness,
    NXOpen::Vector3d& innerNormal,
    NXOpen::Point3d& helpPoint,
    std::string& error) const
{
    thickness = 0.0;
    NXOpen::Point3d basePoint;
    NXOpen::Vector3d baseNormal;
    if (body == nullptr || !body->IsSolidBody() ||
        !FacePlaneData(face, basePoint, baseNormal))
    {
        error = "请选择实体上的平面。";
        return false;
    }

    NXOpen::Vector3d xAxis;
    NXOpen::Vector3d yAxis;
    std::vector<NXOpen::Point3d> samples;
    if (!ProjectionAxes(face, baseNormal, xAxis, yAxis) ||
        !BuildInteriorSamples(face, basePoint, xAxis, yAxis, samples))
    {
        error = "无法在选择面内生成板厚检测点。";
        return false;
    }
    helpPoint = samples.front();

    std::vector<OppositeFaceCandidate> candidates;
    for (NXOpen::Face* candidateFace : body->GetFaces())
    {
        if (candidateFace == nullptr || candidateFace == face)
        {
            continue;
        }
        NXOpen::Point3d candidatePoint;
        NXOpen::Vector3d candidateNormal;
        if (!FacePlaneData(candidateFace, candidatePoint, candidateNormal) ||
            std::fabs(Dot(baseNormal, candidateNormal)) < kParallelTolerance)
        {
            continue;
        }
        const double signedDistance = Dot(Subtract(candidatePoint, basePoint), baseNormal);
        if (std::fabs(signedDistance) > kTolerance)
        {
            candidates.push_back(
                {candidateFace, signedDistance, std::fabs(signedDistance)});
        }
    }
    std::sort(candidates.begin(), candidates.end(),
              [](const OppositeFaceCandidate& a, const OppositeFaceCandidate& b) {
                  return a.distance < b.distance;
              });

    // Feature 36 groups coplanar split faces before checking projected
    // coverage. This prevents a seam or cutout from hiding the true back face.
    for (std::size_t first = 0; first < candidates.size();)
    {
        std::size_t last = first + 1;
        const double groupTolerance = std::max(1.0e-3, candidates[first].distance * 1.0e-4);
        while (last < candidates.size() &&
               std::fabs(candidates[last].signedDistance -
                         candidates[first].signedDistance) <= groupTolerance)
        {
            ++last;
        }

        std::size_t covered = 0;
        for (const NXOpen::Point3d& sample : samples)
        {
            const NXOpen::Point3d projected =
                Move(sample, baseNormal, candidates[first].signedDistance);
            bool found = false;
            for (std::size_t index = first; index < last; ++index)
            {
                if (PointOnFace(candidates[index].face, projected))
                {
                    found = true;
                    break;
                }
            }
            covered += found ? 1U : 0U;
        }
        const double coverage = static_cast<double>(covered) /
                                static_cast<double>(samples.size());
        if (coverage + 1.0e-9 >= kCoverageThreshold)
        {
            thickness = candidates[first].distance;
            const double sign = candidates[first].signedDistance >= 0.0 ? 1.0 : -1.0;
            innerNormal = NXOpen::Vector3d(baseNormal.X * sign,
                                           baseNormal.Y * sign,
                                           baseNormal.Z * sign);
            return true;
        }
        first = last;
    }

    error = "未找到与选择面投影重合的板厚对面，无法计算板厚。";
    return false;
}

bool CaiRBanDialog::CreateAndSubtract(
    NXOpen::Face* face,
    double thickness,
    const NXOpen::Vector3d& innerNormal,
    const NXOpen::Point3d& helpPoint,
    std::string& error,
    bool recolor,
    int color,
    std::vector<tag_t>* createdFeatureTags) const
{
    NXOpen::Part* workPart = session_->Parts()->Work();
    NXOpen::Body* targetBody = face != nullptr ? face->GetBody() : nullptr;
    if (workPart == nullptr || targetBody == nullptr || thickness <= kTolerance)
    {
        error = "没有可用的工作部件、目标体或板厚。";
        return false;
    }

    NXOpen::Features::ExtrudeBuilder* extrude = nullptr;
    NXOpen::Features::ReplaceFaceBuilder* replace = nullptr;
    NXOpen::Features::BooleanBuilder* subtract = nullptr;
    std::string failureStage = "创建拉伸截面";
    try
    {
        std::vector<NXOpen::Edge*> outerEdges;
        if (!OuterBoundaryEdges(face, outerEdges))
        {
            throw std::runtime_error("无法取得选择面的外围边。");
        }

        std::vector<NXOpen::Face*> originalAdjacentFaces;
        originalAdjacentFaces.reserve(outerEdges.size());
        for (NXOpen::Edge* outerEdge : outerEdges)
        {
            NXOpen::Face* adjacent = AdjacentBodyFace(outerEdge, face, targetBody);
            if (adjacent == nullptr)
            {
                throw std::runtime_error("选择面外围边没有找到原体相邻面。");
            }
            originalAdjacentFaces.push_back(adjacent);
        }

        const std::vector<NXOpen::Edge*> boundaryEdges = face->GetEdges();
        if (boundaryEdges.empty() || boundaryEdges.front() == nullptr)
        {
            throw std::runtime_error("选择面没有可用的边界边。");
        }

        failureStage = "设置拉伸参数";
        extrude = workPart->Features()->CreateExtrudeBuilder(nullptr);
        NXOpen::Section* section =
            workPart->Sections()->CreateSection(0.000095, 0.0001, 0.5);
        extrude->SetSection(section);
        extrude->FeatureOptions()->SetBodyType(
            NXOpen::GeometricUtilities::FeatureOptions::BodyStyleSolid);
        extrude->BooleanOperation()->SetType(
            NXOpen::GeometricUtilities::BooleanOperation::BooleanTypeCreate);

        section->SetDistanceTolerance(0.0001);
        section->SetChainingTolerance(0.000095);
        section->SetAllowedEntityTypes(NXOpen::Section::AllowTypesOnlyCurves);

        NXOpen::SelectionIntentRuleOptions* boundaryOptions =
            workPart->ScRuleFactory()->CreateRuleOptions();
        boundaryOptions->SetSelectedFromInactive(false);
        NXOpen::EdgeBoundaryRule* boundaryRule =
            workPart->ScRuleFactory()->CreateRuleEdgeBoundary(
                std::vector<NXOpen::Face*>{face}, boundaryOptions);
        delete boundaryOptions;

        NXOpen::Point3d edgeStart;
        NXOpen::Point3d edgeEnd;
        boundaryEdges.front()->GetVertices(&edgeStart, &edgeEnd);
        const NXOpen::Point3d boundaryHelpPoint(
            (edgeStart.X + edgeEnd.X) * 0.5,
            (edgeStart.Y + edgeEnd.Y) * 0.5,
            (edgeStart.Z + edgeEnd.Z) * 0.5);
        section->AddToSection(
            std::vector<NXOpen::SelectionIntentRule*>{boundaryRule},
            boundaryEdges.front(),
            nullptr,
            nullptr,
            boundaryHelpPoint,
            NXOpen::Section::ModeCreate,
            false);

        NXOpen::Direction* direction = workPart->Directions()->CreateDirection(
            helpPoint, innerNormal,
            NXOpen::SmartObject::UpdateOptionWithinModeling);
        extrude->SetDirection(direction);
        extrude->Limits()->StartExtend()->SetValue("0");
        extrude->Limits()->EndExtend()->SetValue(Number(thickness).c_str());
        failureStage = "提交拉伸工具体";
        NXOpen::Features::Feature* toolFeature = extrude->CommitFeature();
        extrude->Destroy();
        extrude = nullptr;
        if (toolFeature == nullptr || toolFeature->GetBodies().empty())
        {
            throw std::runtime_error("NX 未返回拉伸工具体。");
        }
        NXOpen::Body* toolBody = toolFeature->GetBodies().front();
        if (createdFeatureTags != nullptr)
        {
            createdFeatureTags->push_back(toolFeature->Tag());
        }

        // Replace one matched pair at a time. A failed pair is skipped while
        // successful pairs remain in the tool body for the final subtraction.
        for (std::size_t index = 0; index < outerEdges.size(); ++index)
        {
            try
            {
                failureStage = "匹配第一对替换面";
                const std::vector<NXOpen::Face*> currentToolFaces =
                    toolBody->GetFaces();
                NXOpen::Face* extrudedSide =
                    ExtrudedSideFaceForEdge(
                        outerEdges[index], currentToolFaces, innerNormal);
                if (extrudedSide == nullptr)
                {
                    continue;
                }

                NXOpen::Face* originalAdjacent = originalAdjacentFaces[index];
                if (originalAdjacent == nullptr)
                {
                    continue;
                }

                failureStage = "逐对替换拉伸侧面";
                replace = workPart->Features()->CreateReplaceFaceBuilder(nullptr);
                replace->SetType(
                    NXOpen::Features::ReplaceFaceBuilder::ReplaceTypesReplace);
                replace->OffsetDistance()->SetFormula("0");
                replace->ResetReplaceFaceMethod();
                replace->ResetFreeEdgeProjectionOption();
                replace->SetReverseDirection(false);

                NXOpen::SelectionIntentRuleOptions* replaceOptions =
                    workPart->ScRuleFactory()->CreateRuleOptions();
                replaceOptions->SetSelectedFromInactive(false);
                NXOpen::FaceDumbRule* targetFaceRule =
                    workPart->ScRuleFactory()->CreateRuleFaceDumb(
                        std::vector<NXOpen::Face*>{extrudedSide}, replaceOptions);
                delete replaceOptions;
                replace->FaceToReplace()->ReplaceRules(
                    std::vector<NXOpen::SelectionIntentRule*>{targetFaceRule},
                    false);

                NXOpen::SelectionIntentRuleOptions* replacementOptions =
                    workPart->ScRuleFactory()->CreateRuleOptions();
                replacementOptions->SetSelectedFromInactive(false);
                NXOpen::FaceDumbRule* replacementFaceRule =
                    workPart->ScRuleFactory()->CreateRuleFaceDumb(
                        std::vector<NXOpen::Face*>{originalAdjacent},
                        replacementOptions);
                delete replacementOptions;
                replace->ReplacementFaces()->ReplaceRules(
                    std::vector<NXOpen::SelectionIntentRule*>{replacementFaceRule},
                    false);

                replace->OnApplyPre();
                NXOpen::Features::Feature* replaceFeature =
                    dynamic_cast<NXOpen::Features::Feature*>(replace->Commit());
                replace->Destroy();
                replace = nullptr;
                if (replaceFeature != nullptr &&
                    !replaceFeature->GetBodies().empty())
                {
                    toolBody = replaceFeature->GetBodies().front();
                    if (createdFeatureTags != nullptr)
                    {
                        createdFeatureTags->push_back(replaceFeature->Tag());
                    }
                }
            }
            catch (...)
            {
                if (replace != nullptr)
                {
                    replace->Destroy();
                    replace = nullptr;
                }
                // Deliberately silent: skip only this pair and continue.
            }
        }

        if (recolor && color >= 1 && color <= 216)
        {
            UF_OBJ_set_color(toolBody->Tag(), color);
        }

        failureStage = "设置布尔减";
        subtract = workPart->Features()->CreateBooleanBuilderUsingCollector(nullptr);
        subtract->SetOperation(NXOpen::Features::Feature::BooleanTypeSubtract);
        subtract->SetRetainTarget(false);
        subtract->SetRetainTool(true);

        NXOpen::SelectionIntentRuleOptions* targetOptions =
            workPart->ScRuleFactory()->CreateRuleOptions();
        targetOptions->SetSelectedFromInactive(false);
        NXOpen::BodyDumbRule* targetRule =
            workPart->ScRuleFactory()->CreateRuleBodyDumb(
                std::vector<NXOpen::Body*>{targetBody}, true, targetOptions);
        delete targetOptions;
        NXOpen::ScCollector* targetCollector =
            workPart->ScCollectors()->CreateCollector();
        targetCollector->ReplaceRules(
            std::vector<NXOpen::SelectionIntentRule*>{targetRule}, false);
        subtract->SetTargetBodyCollector(targetCollector);

        NXOpen::SelectionIntentRuleOptions* toolOptions =
            workPart->ScRuleFactory()->CreateRuleOptions();
        toolOptions->SetSelectedFromInactive(false);
        NXOpen::BodyDumbRule* toolRule =
            workPart->ScRuleFactory()->CreateRuleBodyDumb(
                std::vector<NXOpen::Body*>{toolBody}, true, toolOptions);
        delete toolOptions;
        NXOpen::ScCollector* toolCollector =
            workPart->ScCollectors()->CreateCollector();
        toolCollector->ReplaceRules(
            std::vector<NXOpen::SelectionIntentRule*>{toolRule}, false);
        subtract->SetToolBodyCollector(toolCollector);

        failureStage = "提交布尔减";
        NXOpen::NXObject* result = subtract->Commit();
        subtract->Destroy();
        subtract = nullptr;
        if (result == nullptr)
        {
            throw std::runtime_error("NX 未返回布尔减特征。");
        }
        if (createdFeatureTags != nullptr)
        {
            NXOpen::Features::Feature* booleanFeature =
                dynamic_cast<NXOpen::Features::Feature*>(result);
            if (booleanFeature != nullptr)
            {
                createdFeatureTags->push_back(booleanFeature->Tag());
            }
        }
        return true;
    }
    catch (const NXOpen::NXException& ex)
    {
        if (extrude != nullptr) extrude->Destroy();
        if (replace != nullptr) replace->Destroy();
        if (subtract != nullptr) subtract->Destroy();
        error = failureStage + "：" +
                (ex.Message() != nullptr ? ex.Message() : "NXOpen 建模失败。");
    }
    catch (const std::exception& ex)
    {
        if (extrude != nullptr) extrude->Destroy();
        if (replace != nullptr) replace->Destroy();
        if (subtract != nullptr) subtract->Destroy();
        error = failureStage + "：" + ex.what();
    }
    return false;
}

bool CaiRBanDialog::EstimateCylinderThickness(
    NXOpen::Body* body,
    NXOpen::Face* face,
    double& diameter,
    double& thickness,
    bool& reverseThicken,
    int wrapMode,
    std::string& error) const
{
    (void)wrapMode;
    diameter = 0.0;
    thickness = 0.0;
    reverseThicken = false;
    if (body == nullptr || face == nullptr || !body->IsSolidBody() ||
        face->SolidFaceType() != NXOpen::Face::FaceTypeCylindrical)
    {
        error = "请选择实体上的圆柱面。";
        return false;
    }

    int selectedType = 0;
    double selectedOriginData[3] = {};
    double selectedAxisData[3] = {};
    double selectedBox[6] = {};
    double selectedRadius = 0.0;
    double selectedRadiusData = 0.0;
    int selectedNormalDirection = 1;
    if (UF_MODL_ask_face_data(
            face->Tag(), &selectedType, selectedOriginData, selectedAxisData,
            selectedBox, &selectedRadius, &selectedRadiusData,
            &selectedNormalDirection) != 0 ||
        selectedType != UF_MODL_CYLINDRICAL_FACE ||
        selectedRadius <= kTolerance)
    {
        error = "无法读取所选圆柱面的轴线和直径。";
        return false;
    }

    NXOpen::Point3d selectedOrigin(selectedOriginData[0], selectedOriginData[1],
                                   selectedOriginData[2]);
    NXOpen::Vector3d selectedAxis(selectedAxisData[0], selectedAxisData[1],
                                  selectedAxisData[2]);
    if (!Normalize(selectedAxis))
    {
        error = "所选圆柱面的轴向无效。";
        return false;
    }
    diameter = selectedRadius * 2.0;

    double bestRadius = 0.0;
    double bestDifference = (std::numeric_limits<double>::max)();
    for (NXOpen::Face* candidate : body->GetFaces())
    {
        if (candidate == nullptr || candidate == face ||
            candidate->SolidFaceType() != NXOpen::Face::FaceTypeCylindrical)
        {
            continue;
        }
        int type = 0;
        double originData[3] = {};
        double axisData[3] = {};
        double box[6] = {};
        double radius = 0.0;
        double radiusData = 0.0;
        int normalDirection = 1;
        if (UF_MODL_ask_face_data(candidate->Tag(), &type, originData, axisData,
                                  box, &radius, &radiusData,
                                  &normalDirection) != 0 ||
            type != UF_MODL_CYLINDRICAL_FACE)
        {
            continue;
        }
        NXOpen::Vector3d axis(axisData[0], axisData[1], axisData[2]);
        if (!Normalize(axis) ||
            std::fabs(Dot(axis, selectedAxis)) < kParallelTolerance)
        {
            continue;
        }
        const NXOpen::Point3d origin(originData[0], originData[1], originData[2]);
        const NXOpen::Vector3d between = Subtract(origin, selectedOrigin);
        if (Length(Cross(between, selectedAxis)) >
            (std::max)(1.0e-3, selectedRadius * 1.0e-5))
        {
            continue;
        }

        const double radialDifference = radius - selectedRadius;
        if (std::fabs(radialDifference) > kTolerance &&
            std::fabs(radialDifference) < bestDifference)
        {
            bestDifference = std::fabs(radialDifference);
            bestRadius = radius;
        }
    }

    if (bestDifference == (std::numeric_limits<double>::max)())
    {
        error = "未找到与所选圆柱面同轴的另一圆柱面，无法获得板厚。";
        return false;
    }

    thickness = bestDifference;
    const double desiredRadialSign = bestRadius > selectedRadius ? 1.0 : -1.0;
    // UF face-data normal_direction is the radial surface-normal sense.
    // Thicken's positive first offset follows that face normal.
    reverseThicken = desiredRadialSign *
                         static_cast<double>(selectedNormalDirection) <
                     0.0;
    return true;
}

bool CaiRBanDialog::CreateArcPanel(
    NXOpen::Face* face,
    double thickness,
    bool reverseThicken,
    const std::array<double, 2>& cutDistances,
    const std::array<double, 2>& offsetDistances,
    int wrapMode,
    std::string& error,
    bool recolor,
    int color,
    std::vector<tag_t>* createdFeatureTags,
    std::array<NXOpen::Point3d, 4>* handleOrigins,
    std::array<NXOpen::Vector3d, 4>* handleDirections) const
{
    NXOpen::Part* workPart = session_->Parts()->Work();
    NXOpen::Body* sourceBody = face != nullptr ? face->GetBody() : nullptr;
    if (workPart == nullptr || sourceBody == nullptr ||
        thickness <= kTolerance)
    {
        error = "没有可用的工作部件、目标体或板厚。";
        return false;
    }

    int cylinderType = 0;
    double cylinderOriginData[3] = {};
    double cylinderAxisData[3] = {};
    double cylinderBox[6] = {};
    double cylinderRadius = 0.0;
    double cylinderRadiusData = 0.0;
    int cylinderNormalDirection = 1;
    if (UF_MODL_ask_face_data(
            face->Tag(), &cylinderType, cylinderOriginData, cylinderAxisData,
            cylinderBox, &cylinderRadius, &cylinderRadiusData,
            &cylinderNormalDirection) != 0 ||
        cylinderType != UF_MODL_CYLINDRICAL_FACE)
    {
        error = "无法读取圆柱面数据。";
        return false;
    }
    NXOpen::Vector3d cylinderAxis(cylinderAxisData[0], cylinderAxisData[1],
                                  cylinderAxisData[2]);
    if (!Normalize(cylinderAxis))
    {
        error = "圆柱面的轴向无效。";
        return false;
    }

    struct TangentEnd
    {
        NXOpen::Face* tangentFace;
        NXOpen::Edge* tangentEdge;
        NXOpen::Point3d edgeMidpoint;
        NXOpen::Vector3d awayDirection;
    };
    std::vector<NXOpen::Face*> facesToThicken{face};
    std::vector<TangentEnd> tangentEnds;
    std::array<bool, 2> trimEnd{{false, false}};
    std::array<double, 2> trimOffsets{{0.0, 0.0}};
    std::array<double, 2> trimReach{{0.0, 0.0}};
    {
        for (NXOpen::Edge* edge : face->GetEdges())
        {
            if (edge == nullptr)
            {
                continue;
            }
            NXOpen::Point3d first;
            NXOpen::Point3d second;
            try
            {
                edge->GetVertices(&first, &second);
            }
            catch (...)
            {
                continue;
            }
            NXOpen::Vector3d edgeDirection = Subtract(second, first);
            const double edgeLength = Length(edgeDirection);
            if (edgeLength <= kTolerance)
            {
                continue;
            }
            edgeDirection.X /= edgeLength;
            edgeDirection.Y /= edgeLength;
            edgeDirection.Z /= edgeLength;
            if (std::fabs(Dot(edgeDirection, cylinderAxis)) <
                kParallelTolerance)
            {
                continue;
            }

            NXOpen::Face* tangentFace =
                AdjacentBodyFace(edge, face, sourceBody);
            NXOpen::Point3d planePoint;
            NXOpen::Vector3d planeNormal;
            if (tangentFace == nullptr ||
                !FacePlaneData(tangentFace, planePoint, planeNormal) ||
                std::fabs(Dot(planeNormal, cylinderAxis)) > 0.01)
            {
                continue;
            }
            NXOpen::Vector3d away = Cross(cylinderAxis, planeNormal);
            if (!Normalize(away))
            {
                continue;
            }
            const NXOpen::Point3d midpoint((first.X + second.X) * 0.5,
                                           (first.Y + second.Y) * 0.5,
                                           (first.Z + second.Z) * 0.5);

            // Pick the tangent direction that actually enters the adjacent
            // planar face by inspecting its vertices away from the shared edge.
            double strongestProjection = 0.0;
            for (NXOpen::Edge* adjacentEdge : tangentFace->GetEdges())
            {
                NXOpen::Point3d a;
                NXOpen::Point3d b;
                try
                {
                    adjacentEdge->GetVertices(&a, &b);
                }
                catch (...)
                {
                    continue;
                }
                for (const NXOpen::Point3d& point : {a, b})
                {
                    const double projection = Dot(Subtract(point, midpoint), away);
                    if (std::fabs(projection) > std::fabs(strongestProjection))
                    {
                        strongestProjection = projection;
                    }
                }
            }
            if (strongestProjection < 0.0)
            {
                away.X = -away.X;
                away.Y = -away.Y;
                away.Z = -away.Z;
            }
            if (std::fabs(strongestProjection) <= kTolerance)
            {
                continue;
            }
            const bool alreadyFound = std::find_if(
                tangentEnds.begin(), tangentEnds.end(),
                [tangentFace](const TangentEnd& item)
                {
                    return item.tangentFace == tangentFace;
                }) != tangentEnds.end();
            if (!alreadyFound)
            {
                tangentEnds.push_back(
                    {tangentFace, edge, midpoint, away});
            }
        }
        if (tangentEnds.size() != 2)
        {
            error = "尺寸手柄要求圆弧两端各连接一个相切平面，当前未能准确找到两个相切面。";
            return false;
        }
        std::sort(tangentEnds.begin(), tangentEnds.end(),
                  [](const TangentEnd& left, const TangentEnd& right)
                  {
                      if (std::fabs(left.edgeMidpoint.X - right.edgeMidpoint.X) >
                          kTolerance)
                          return left.edgeMidpoint.X < right.edgeMidpoint.X;
                      if (std::fabs(left.edgeMidpoint.Y - right.edgeMidpoint.Y) >
                          kTolerance)
                          return left.edgeMidpoint.Y < right.edgeMidpoint.Y;
                      return left.edgeMidpoint.Z < right.edgeMidpoint.Z;
                  });
        if (handleOrigins != nullptr && handleDirections != nullptr)
        {
            for (std::size_t index = 0; index < 2; ++index)
            {
                // LinearDimension displays its draggable end at
                // origin + orientation * value, which puts the grip on the
                // trimmed end-face centre.
                (*handleOrigins)[index] = tangentEnds[index].edgeMidpoint;
                (*handleDirections)[index] = tangentEnds[index].awayDirection;
            }
        }

        // A trim plane on the tangent edge, or past the end of that face,
        // does not intersect the solid. NX then rejects the preview and the
        // drag handles disappear. Keep a face only while its cut is positive,
        // and trim it only while the plane stays inside the face.
        constexpr double kTrimClearance = 0.05;
        for (std::size_t index = 0; index < tangentEnds.size() && index < 2;
             ++index)
        {
            const double requested = cutDistances[index];
            if (requested <= kTolerance)
            {
                continue;
            }
            double reach = 0.0;
            for (NXOpen::Edge* adjacentEdge :
                 tangentEnds[index].tangentFace->GetEdges())
            {
                if (adjacentEdge == nullptr)
                {
                    continue;
                }
                NXOpen::Point3d first;
                NXOpen::Point3d second;
                try
                {
                    adjacentEdge->GetVertices(&first, &second);
                }
                catch (...)
                {
                    continue;
                }
                for (const NXOpen::Point3d& point : {first, second})
                {
                    const double distance = Dot(
                        Subtract(point, tangentEnds[index].edgeMidpoint),
                        tangentEnds[index].awayDirection);
                    if (distance > reach)
                    {
                        reach = distance;
                    }
                }
            }
            facesToThicken.push_back(tangentEnds[index].tangentFace);
            if (reach > kTrimClearance &&
                requested < reach - kTrimClearance)
            {
                trimEnd[index] = true;
                trimOffsets[index] = requested;
                trimReach[index] = reach;
            }
        }
    }

    NXOpen::Features::ThickenBuilder* thicken = nullptr;
    NXOpen::Features::ExtrudeBuilder* cutExtrude = nullptr;
    NXOpen::Features::OffsetFaceBuilder* innerOffset = nullptr;
    std::string stage = "创建圆弧板加厚";
    try
    {
        thicken = workPart->Features()->CreateThickenBuilder(nullptr);
        NXOpen::SelectionIntentRuleOptions* faceOptions =
            workPart->ScRuleFactory()->CreateRuleOptions();
        faceOptions->SetSelectedFromInactive(false);
        NXOpen::FaceDumbRule* faceRule =
            workPart->ScRuleFactory()->CreateRuleFaceDumb(
                facesToThicken, faceOptions);
        delete faceOptions;
        thicken->FaceCollector()->ReplaceRules(
            std::vector<NXOpen::SelectionIntentRule*>{faceRule}, false);
        thicken->BooleanOperation()->SetType(
            NXOpen::GeometricUtilities::BooleanOperation::BooleanTypeCreate);
        thicken->FirstOffset()->SetFormula(Number(thickness).c_str());
        thicken->SecondOffset()->SetFormula("0");
        thicken->SetReverseDirection(reverseThicken);
        thicken->SetTolerance(0.001);
        NXOpen::Features::Feature* thickenFeature = thicken->CommitFeature();
        thicken->Destroy();
        thicken = nullptr;
        if (thickenFeature == nullptr || thickenFeature->GetBodies().empty())
        {
            throw std::runtime_error("NX 未返回加厚实体。");
        }
        NXOpen::Body* panelBody = thickenFeature->GetBodies().front();
        if (createdFeatureTags != nullptr)
        {
            createdFeatureTags->push_back(thickenFeature->Tag());
        }

        for (std::size_t index = 0; index < tangentEnds.size() && index < 2;
             ++index)
        {
            if (!trimEnd[index])
            {
                continue;
            }
            stage = "切除第 " + std::to_string(index + 1) +
                    " 端多余延伸";
            const TangentEnd& end = tangentEnds[index];
            NXOpen::Point3d facePoint;
            NXOpen::Vector3d faceNormal;
            if (!FacePlaneData(end.tangentFace, facePoint, faceNormal))
            {
                throw std::runtime_error("无法读取相切平面的方向。");
            }
            const NXOpen::Point3d cutOrigin =
                Move(end.edgeMidpoint, end.awayDirection, trimOffsets[index]);
            double axialMin = (std::numeric_limits<double>::max)();
            double axialMax = std::numeric_limits<double>::lowest();
            for (NXOpen::Edge* adjacentEdge : end.tangentFace->GetEdges())
            {
                if (adjacentEdge == nullptr)
                {
                    continue;
                }
                NXOpen::Point3d first;
                NXOpen::Point3d second;
                try
                {
                    adjacentEdge->GetVertices(&first, &second);
                }
                catch (...)
                {
                    continue;
                }
                for (const NXOpen::Point3d& point : {first, second})
                {
                    const double axial = Dot(Subtract(point, cutOrigin),
                                              cylinderAxis);
                    axialMin = (std::min)(axialMin, axial);
                    axialMax = (std::max)(axialMax, axial);
                }
            }
            constexpr double kToolMargin = 1.0;
            axialMin -= kToolMargin;
            axialMax += kToolMargin;
            const double normalHalf = thickness + kToolMargin;
            const double toolLength =
                (trimReach[index] - trimOffsets[index]) + kToolMargin;
            const std::array<NXOpen::Point3d, 4> corners{
                Move(Move(cutOrigin, cylinderAxis, axialMin), faceNormal,
                     -normalHalf),
                Move(Move(cutOrigin, cylinderAxis, axialMax), faceNormal,
                     -normalHalf),
                Move(Move(cutOrigin, cylinderAxis, axialMax), faceNormal,
                     normalHalf),
                Move(Move(cutOrigin, cylinderAxis, axialMin), faceNormal,
                     normalHalf)};
            std::vector<NXOpen::Curve*> curves;
            curves.reserve(4);
            for (std::size_t corner = 0; corner < corners.size(); ++corner)
            {
                NXOpen::Line* line = workPart->Curves()->CreateLine(
                    corners[corner], corners[(corner + 1) % corners.size()]);
                UF_OBJ_set_blank_status(line->Tag(), UF_OBJ_BLANKED);
                curves.push_back(line);
            }

            cutExtrude = workPart->Features()->CreateExtrudeBuilder(nullptr);
            try
            {
                NXOpen::Section* section = workPart->Sections()->CreateSection(
                    0.000095, 0.0001, 0.5);
                cutExtrude->SetSection(section);
                section->SetDistanceTolerance(0.0001);
                section->SetChainingTolerance(0.000095);
                section->SetAllowedEntityTypes(
                    NXOpen::Section::AllowTypesOnlyCurves);
                NXOpen::CurveDumbRule* curveRule =
                    workPart->ScRuleFactory()->CreateRuleCurveDumb(curves);
                section->AddToSection(
                    std::vector<NXOpen::SelectionIntentRule*>{curveRule},
                    curves.front(), nullptr, nullptr, cutOrigin,
                    NXOpen::Section::ModeCreate, false);
                NXOpen::Direction* direction =
                    workPart->Directions()->CreateDirection(
                        cutOrigin, end.awayDirection,
                        NXOpen::SmartObject::UpdateOptionWithinModeling);
                cutExtrude->SetDirection(direction);
                cutExtrude->Limits()->StartExtend()->SetValue("0");
                cutExtrude->Limits()->EndExtend()->SetValue(
                    Number(toolLength).c_str());
                // The tool starts at this end's cut plane and runs only along
                // that flange. It does not cross the arc onto the other end.
                cutExtrude->BooleanOperation()->SetType(
                    NXOpen::GeometricUtilities::BooleanOperation::
                        BooleanTypeSubtract);
                cutExtrude->BooleanOperation()->SetTargetBodies(
                    std::vector<NXOpen::Body*>{panelBody});
                NXOpen::Features::Feature* cutFeature =
                    cutExtrude->CommitFeature();
                cutExtrude->Destroy();
                cutExtrude = nullptr;
                if (cutFeature == nullptr || cutFeature->GetBodies().empty())
                {
                    throw std::runtime_error("NX 未返回切除后的圆弧板实体。");
                }
                panelBody = cutFeature->GetBodies().front();
                if (createdFeatureTags != nullptr)
                {
                    createdFeatureTags->push_back(cutFeature->Tag());
                }
            }
            catch (const NXOpen::NXException& ex)
            {
                if (cutExtrude != nullptr)
                {
                    cutExtrude->Destroy();
                    cutExtrude = nullptr;
                }
                for (NXOpen::Curve* curve : curves)
                {
                    if (curve != nullptr &&
                        UF_OBJ_ask_status(curve->Tag()) == UF_OBJ_ALIVE)
                    {
                        UF_OBJ_delete_object(curve->Tag());
                    }
                }
                const char* message = ex.Message();
                if (message != nullptr &&
                    std::string(message).find("outside") != std::string::npos)
                {
                    continue;
                }
                throw;
            }
        }

        // 两端偏置手柄均为自由有符号距离。外包默认 0，内包默认
        // 负板厚；拖动后按两端各自的实际数值创建偏置特征。
        // 端面按轴线两端的位置识别。某一端没有可偏置平面时，只有该端
        // 偏置距离不为 0 才失败；外包默认 0，仍可生成圆弧板。
        {
            stage = "定位端面偏置手柄";
            std::array<std::vector<NXOpen::Face*>, 2> endOffsetFaces;
            std::array<NXOpen::Point3d, 2> endCenters{};
            std::array<NXOpen::Vector3d, 2> endNormals{};
            double axialMin = 0.0;
            double axialMax = 0.0;
            const NXOpen::Point3d cylinderOrigin(
                cylinderOriginData[0], cylinderOriginData[1],
                cylinderOriginData[2]);
            if (!LocateThickenedEndCaps(
                    panelBody, cylinderOrigin, cylinderAxis, endOffsetFaces,
                    endCenters, endNormals, axialMin, axialMax))
            {
                throw std::runtime_error(
                    "未能准确找到加厚体两端的偏置平面。");
            }

            for (std::size_t index = 0; index < 2; ++index)
            {
                if (!endOffsetFaces[index].empty())
                {
                    continue;
                }
                if (std::fabs(offsetDistances[index]) > kTolerance)
                {
                    throw std::runtime_error(
                        "未能准确找到加厚体两端的偏置平面。");
                }
                const double position = index == 0 ? axialMin : axialMax;
                endCenters[index] =
                    Move(cylinderOrigin, cylinderAxis, position);
                const double sense = index == 0 ? -1.0 : 1.0;
                endNormals[index] = NXOpen::Vector3d(
                    cylinderAxis.X * sense, cylinderAxis.Y * sense,
                    cylinderAxis.Z * sense);
                const std::size_t other = index == 0 ? 1 : 0;
                const double otherSense = other == 0 ? -1.0 : 1.0;
                if (!endOffsetFaces[other].empty() &&
                    Dot(endNormals[other], cylinderAxis) * otherSense < 0.0)
                {
                    endNormals[index].X = -endNormals[index].X;
                    endNormals[index].Y = -endNormals[index].Y;
                    endNormals[index].Z = -endNormals[index].Z;
                }
            }

            if (handleOrigins != nullptr && handleDirections != nullptr)
            {
                for (std::size_t index = 0; index < 2; ++index)
                {
                    (*handleOrigins)[index + 2] = endCenters[index];
                    (*handleDirections)[index + 2] = endNormals[index];
                }
            }

            for (std::size_t index = 0; index < 2; ++index)
            {
                if (std::fabs(offsetDistances[index]) <= kTolerance ||
                    endOffsetFaces[index].empty())
                {
                    continue;
                }
                stage = "创建第 " + std::to_string(index + 1) +
                        " 个端面偏置";
                innerOffset =
                    workPart->Features()->CreateOffsetFaceBuilder(nullptr);
                innerOffset->Distance()->SetFormula(
                    Number(offsetDistances[index]).c_str());
                innerOffset->SetDirection(false);
                NXOpen::SelectionIntentRuleOptions* offsetOptions =
                    workPart->ScRuleFactory()->CreateRuleOptions();
                offsetOptions->SetSelectedFromInactive(false);
                NXOpen::FaceDumbRule* offsetRule =
                    workPart->ScRuleFactory()->CreateRuleFaceDumb(
                        endOffsetFaces[index], offsetOptions);
                delete offsetOptions;
                innerOffset->FaceCollector()->ReplaceRules(
                    std::vector<NXOpen::SelectionIntentRule*>{offsetRule},
                    false);
                NXOpen::Features::Feature* offsetFeature =
                    innerOffset->CommitFeature();
                innerOffset->Destroy();
                innerOffset = nullptr;
                if (offsetFeature == nullptr ||
                    offsetFeature->GetBodies().empty())
                {
                    throw std::runtime_error(
                        "NX 未返回端面偏置后的实体。");
                }
                panelBody = offsetFeature->GetBodies().front();
                if (createdFeatureTags != nullptr)
                {
                    createdFeatureTags->push_back(offsetFeature->Tag());
                }
            }
        }

        if (recolor && color >= 1 && color <= 216)
        {
            UF_OBJ_set_color(panelBody->Tag(), color);
        }
        return true;
    }
    catch (const NXOpen::NXException& ex)
    {
        if (thicken != nullptr) thicken->Destroy();
        if (cutExtrude != nullptr) cutExtrude->Destroy();
        if (innerOffset != nullptr) innerOffset->Destroy();
        error = stage + "：" +
                (ex.Message() != nullptr ? ex.Message() : "NXOpen 建模失败。");
    }
    catch (const std::exception& ex)
    {
        if (thicken != nullptr) thicken->Destroy();
        if (cutExtrude != nullptr) cutExtrude->Destroy();
        if (innerOffset != nullptr) innerOffset->Destroy();
        error = stage + "：" + ex.what();
    }
    return false;
}

int CaiRBanDialog::Execute()
{
    NXOpen::Face* face = SelectedFace();
    if (face == nullptr)
    {
        ShowError("请选择一个圆柱面。");
        return 1;
    }

    const NXOpen::Session::UndoMarkId mark =
        session_->SetUndoMark(NXOpen::Session::MarkVisibilityVisible, "拆圆弧板");
    std::string error;
    double diameter = 0.0;
    double thickness = 0.0;
    bool reverseThicken = false;
    if (!EstimateCylinderThickness(face->GetBody(), face, diameter, thickness,
                                   reverseThicken, WrapMode(), error) ||
        !CreateArcPanel(face, thickness, reverseThicken, cutDistances_,
                        offsetDistances_,
                        WrapMode(), error, RecolorEnabled(), RequestedColor(),
                        nullptr, nullptr, nullptr))
    {
        session_->UndoToMark(mark, "拆圆弧板失败");
        session_->DeleteUndoMark(mark, "拆圆弧板失败");
        ShowError(error);
        return 1;
    }
    session_->SetUndoMarkName(mark, "拆圆弧板");
    return 0;
}

void CaiRBanDialog::ShowError(const std::string& message) const
{
    ui_->NXMessageBox()->Show("拆圆弧板",
                              NXOpen::NXMessageBox::DialogTypeError,
                              message.c_str());
}

extern "C" __declspec(dllexport) int ZhihuiCaiRBanBuildCustomFeature(
    void* eventPointer)
{
    if (gActiveCaiRBanDialog == nullptr || eventPointer == nullptr)
    {
        return 1;
    }
    return gActiveCaiRBanDialog->BuildCustomFeatureConstruction(
        static_cast<NXOpen::Features::CustomFeaturePreUpdateEvent*>(
            eventPointer));
}
