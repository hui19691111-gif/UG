#pragma once

#include <NXOpen/Edge.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/Features_Feature.hxx>
#include <NXOpen/Features_MoveFaceBuilder.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <uf_modl.h>
#include <uf_obj.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <string>
#include <vector>

// Follow a section of the sheet from its selected skin to the opposite skin.
// Both walls, every bend, and the return's free end must move together.
namespace panel_skirt
{
inline double Dot(const NXOpen::Vector3d& a, const NXOpen::Vector3d& b)
{
    return a.X * b.X + a.Y * b.Y + a.Z * b.Z;
}

inline NXOpen::Vector3d Between(const NXOpen::Point3d& a,
                              const NXOpen::Point3d& b)
{
    return NXOpen::Vector3d(a.X - b.X, a.Y - b.Y, a.Z - b.Z);
}

inline bool Line(NXOpen::Edge* edge, NXOpen::Point3d& first,
                 NXOpen::Point3d& second, NXOpen::Vector3d& direction)
{
    if (!edge || edge->SolidEdgeType() != NXOpen::Edge::EdgeTypeLinear)
        return false;
    edge->GetVertices(&first, &second);
    direction = Between(second, first);
    const double length = std::sqrt(Dot(direction, direction));
    if (length <= 1.0e-5)
        return false;
    direction.X /= length;
    direction.Y /= length;
    direction.Z /= length;
    return true;
}

inline bool Surface(NXOpen::Face* face, NXOpen::Point3d& point,
                    NXOpen::Vector3d& direction, double& radius)
{
    int type = 0, sense = 1;
    double p[3] = {}, d[3] = {}, box[6] = {}, radiusData = 0;
    if (!face || UF_MODL_ask_face_data(face->Tag(), &type, p, d, box,
                                      &radius, &radiusData, &sense) != 0)
        return false;
    point = NXOpen::Point3d(p[0], p[1], p[2]);
    direction = NXOpen::Vector3d(d[0], d[1], d[2]);
    return true;
}

struct Selection
{
    std::vector<NXOpen::Face*> faces;
    std::string error;
};

inline Selection Collect(NXOpen::Face* mainFace,
                         const NXOpen::Point3d& origin,
                         const NXOpen::Vector3d& normal,
                         const NXOpen::Vector3d& outward,
                         double thickness, double tolerance)
{
    Selection selection;
    const auto fail = [&selection](const char* message)
    {
        selection.faces.clear();
        selection.error = message;
        return selection;
    };
    if (!mainFace || thickness <= tolerance)
        return fail("无法识别主平面或板厚，不能保持原裙边宽度。");

    double boundary = -std::numeric_limits<double>::max();
    for (NXOpen::Edge* edge : mainFace->GetEdges())
    {
        NXOpen::Point3d a, b;
        edge->GetVertices(&a, &b);
        boundary = (std::max)(boundary, Dot(Between(a, origin), outward));
        boundary = (std::max)(boundary, Dot(Between(b, origin), outward));
    }

    const double skinTolerance = (std::max)(tolerance, thickness * 1.0e-5);
    std::set<tag_t> selected;
    int sectionCount = 0;
    for (NXOpen::Edge* start : mainFace->GetEdges())
    {
        NXOpen::Point3d a, b;
        NXOpen::Vector3d along;
        if (!Line(start, a, b, along) ||
            std::abs(Dot(along, outward)) > 1.0e-5 ||
            std::abs(Dot(Between(a, origin), outward) - boundary) > tolerance ||
            std::abs(Dot(Between(b, origin), outward) - boundary) > tolerance)
            continue;

        ++sectionCount;
        const NXOpen::Point3d sectionPoint(
            (a.X + b.X) * 0.5, (a.Y + b.Y) * 0.5, (a.Z + b.Z) * 0.5);
        NXOpen::Edge* incoming = start;
        NXOpen::Face* previous = mainFace;
        std::set<tag_t> walked{mainFace->Tag()};
        bool reachedBackSkin = false;
        for (int depth = 0; depth < 64; ++depth)
        {
            NXOpen::Face* face = nullptr;
            for (NXOpen::Face* adjacent : incoming->GetFaces())
                if (adjacent && adjacent->Tag() != previous->Tag())
                    face = adjacent;
            if (!face || !walked.insert(face->Tag()).second)
                break;

            NXOpen::Point3d point;
            NXOpen::Vector3d surfaceDirection;
            double radius = 0;
            if (!Surface(face, point, surfaceDirection, radius))
                break;
            const bool planar =
                face->SolidFaceType() == NXOpen::Face::FaceTypePlanar;
            const double height = Dot(Between(point, origin), normal);
            // The opposite main skin can span the entire panel or contain a
            // large opening. Never traverse its farthest parallel edge.
            if (planar && std::abs(Dot(surfaceDirection, normal)) > 0.99999 &&
                std::abs(std::abs(height) - thickness) <= skinTolerance)
            {
                reachedBackSkin = true;
                break;
            }

            const bool bend =
                face->SolidFaceType() == NXOpen::Face::FaceTypeCylindrical &&
                std::abs(Dot(surfaceDirection, along)) > 0.99999;
            if (!planar && !bend)
                return fail("裙边包含无法追踪的曲面，本次调整已取消。");
            // Translating a plane tangent to the motion does not move its
            // surface. Its bounding walls define its width; selecting a merged
            // ring-shaped face would also translate the other three sides.
            if ((bend || std::abs(Dot(surfaceDirection, outward)) > 1.0e-5) &&
                selected.insert(face->Tag()).second)
                selection.faces.push_back(face);

            NXOpen::Point3d inA, inB;
            NXOpen::Vector3d inDirection;
            if (!Line(incoming, inA, inB, inDirection))
                break;
            NXOpen::Edge* next = nullptr;
            double nearest = std::numeric_limits<double>::max();
            for (NXOpen::Edge* candidate : face->GetEdges())
            {
                if (candidate->Tag() == incoming->Tag())
                    continue;
                NXOpen::Point3d c, d;
                NXOpen::Vector3d candidateDirection;
                if (!Line(candidate, c, d, candidateDirection) ||
                    std::abs(Dot(candidateDirection, along)) < 0.99999)
                    continue;
                const double cAlong = Dot(Between(c, sectionPoint), along);
                const double dAlong = Dot(Between(d, sectionPoint), along);
                if ((std::min)(cAlong, dAlong) > tolerance ||
                    (std::max)(cAlong, dAlong) < -tolerance)
                    continue;
                const NXOpen::Vector3d delta = Between(c, inA);
                const double parallel = Dot(delta, along);
                const double squared = (std::max)(0.0,
                    Dot(delta, delta) - parallel * parallel);
                // Use the local next edge, not an edge on the opposite side
                // of a merged return ring. Do not stop at a nearby hole edge
                // merely because its distance happens to equal sheet thickness.
                if (squared > tolerance * tolerance && squared < nearest)
                {
                    nearest = squared;
                    next = candidate;
                }
            }
            if (!next)
                break;
            previous = face;
            incoming = next;
        }
        if (!reachedBackSkin)
            return fail("未能完整识别该侧折回裙边，已取消调整以保持原宽度。");
    }
    if (!sectionCount || selection.faces.empty())
        return fail("未找到可保持裙边宽度的板件边界面。");
    return selection;
}

struct SurfacePosition
{
    tag_t tag = NULL_TAG;
    double outward = 0;
    double height = 0;
    double radius = 0;
    double motionProjection = 1;
    bool bend = false;
};

inline bool Measure(NXOpen::Face* face, const NXOpen::Point3d& origin,
                    const NXOpen::Vector3d& normal,
                    const NXOpen::Vector3d& outward, SurfacePosition& value)
{
    NXOpen::Point3d point;
    NXOpen::Vector3d direction;
    if (!Surface(face, point, direction, value.radius))
        return false;
    value.tag = face->Tag();
    value.bend = face->SolidFaceType() == NXOpen::Face::FaceTypeCylindrical;
    // Plane origins may slide tangentially after a modeling operation. Only
    // their normal projection is invariant. Cylinder axes are parallel to the
    // boundary, so both cross-section coordinates are invariant.
    value.outward = Dot(Between(point, origin), value.bend ? outward : direction);
    value.motionProjection = value.bend ? 1.0 : Dot(direction, outward);
    value.height = value.bend ? Dot(Between(point, origin), normal) : 0.0;
    return true;
}

struct MoveResult
{
    // Legacy MoveFaceBuilder can edit the body without returning a feature.
    // This tag is optional; successful geometry verification is mandatory.
    tag_t featureTag = NULL_TAG;
    std::string error;
};

inline MoveResult CommitAndVerifyMove(
    NXOpen::Features::MoveFaceBuilder* builder,
    const std::vector<SurfacePosition>& before,
    const NXOpen::Point3d& origin, const NXOpen::Vector3d& normal,
    const NXOpen::Vector3d& outward, double distance, double tolerance)
{
    MoveResult result;
    if (!builder || before.empty())
    {
        result.error = "缺少原裙边截面，无法校验移动结果。";
        return result;
    }
    NXOpen::NXObject* committed = builder->Commit();
    auto* feature = dynamic_cast<NXOpen::Features::Feature*>(committed);
    if (!feature)
        for (NXOpen::NXObject* object : builder->GetCommittedObjects())
            if ((feature = dynamic_cast<NXOpen::Features::Feature*>(object)) != nullptr)
                break;
    if (feature)
        result.featureTag = feature->Tag();

    // An empty result is not a failed commit. Verify the actual displacement
    // of every wall, free end and bend before accepting an in-place edit.
    // The caller's undo mark also covers edits with no feature object.
    for (const SurfacePosition& original : before)
    {
        auto* face = UF_OBJ_ask_status(original.tag) == UF_OBJ_ALIVE
            ? dynamic_cast<NXOpen::Face*>(NXOpen::NXObjectManager::Get(original.tag))
            : nullptr;
        SurfacePosition moved;
        if (!face || !Measure(face, origin, normal, outward, moved) ||
            moved.bend != original.bend ||
            std::abs(moved.motionProjection - original.motionProjection) > 1.0e-5 ||
            std::abs(moved.outward - original.outward -
                     distance * original.motionProjection) > tolerance ||
            std::abs(moved.height - original.height) > tolerance ||
            std::abs(moved.radius - original.radius) > tolerance)
        {
            result.error = "移动结果未能保持原裙边宽度或折弯截面，faceTag=" +
                std::to_string(original.tag);
            return result;
        }
    }
    return result;
}
}
