#pragma once

#include <NXOpen/Arc.hxx>
#include <NXOpen/Body.hxx>
#include <NXOpen/BodyDumbRule.hxx>
#include <NXOpen/Curve.hxx>
#include <NXOpen/CurveCollection.hxx>
#include <NXOpen/CurveDumbRule.hxx>
#include <NXOpen/Direction.hxx>
#include <NXOpen/DirectionCollection.hxx>
#include <NXOpen/DisplayManager.hxx>
#include <NXOpen/DisplayModification.hxx>
#include <NXOpen/DisplayableObject.hxx>
#include <NXOpen/Edge.hxx>
#include <NXOpen/Expression.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/FaceDumbRule.hxx>
#include <NXOpen/Features_BooleanBuilder.hxx>
#include <NXOpen/Features_DividefaceBuilder.hxx>
#include <NXOpen/Features_Feature.hxx>
#include <NXOpen/Features_FeatureCollection.hxx>
#include <NXOpen/Features_OffsetRegionBuilder.hxx>
#include <NXOpen/Features_ProjectCurveBuilder.hxx>
#include <NXOpen/Features_Text.hxx>
#include <NXOpen/Features_TextBuilder.hxx>
#include <NXOpen/Features_ThickenBuilder.hxx>
#include <NXOpen/Features_TubeBuilder.hxx>
#include <NXOpen/GeometricUtilities_BooleanOperation.hxx>
#include <NXOpen/GeometricUtilities_FrameOnPathBuilder.hxx>
#include <NXOpen/GeometricUtilities_OnPathDimensionBuilder.hxx>
#include <NXOpen/GeometricUtilities_ProjectionOptions.hxx>
#include <NXOpen/GeometricUtilities_SelectDividingObjectBuilder.hxx>
#include <NXOpen/IBaseCurve.hxx>
#include <NXOpen/Line.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/NXString.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/Point.hxx>
#include <NXOpen/PointCollection.hxx>
#include <NXOpen/ScCollector.hxx>
#include <NXOpen/ScCollectorCollection.hxx>
#include <NXOpen/ScRuleFactory.hxx>
#include <NXOpen/Section.hxx>
#include <NXOpen/SectionCollection.hxx>
#include <NXOpen/SelectBodyList.hxx>
#include <NXOpen/SelectDisplayableObjectList.hxx>
#include <NXOpen/SelectObjectList.hxx>
#include <NXOpen/SelectionIntentRule.hxx>
#include <NXOpen/SelectionIntentRuleOptions.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/SmartObject.hxx>
#include <NXOpen/ugmath.hxx>

#include <uf_curve.h>
#include <uf_modl.h>
#include <uf_modl_types.h>
#include <uf_obj.h>
#include <uf_object_types.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <iomanip>
#include <limits>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace zhihui_kezi_curved
{
using namespace NXOpen;

constexpr double kPi = 3.14159265358979323846;
constexpr double kEps = 1.0e-6;

struct CurvedFaceInfo
{
    int faceType = 0;
    bool isPlanar = false;
    bool isCylindrical = false;
    Point3d point = Point3d(0.0, 0.0, 0.0);
    Vector3d normal = Vector3d(0.0, 0.0, 1.0);
    Vector3d uTangent = Vector3d(1.0, 0.0, 0.0);
    Vector3d vTangent = Vector3d(0.0, 1.0, 0.0);
    double uMin = 0.0;
    double uMax = 0.0;
    double vMin = 0.0;
    double vMax = 0.0;
    double uParm = 0.0;
    double vParm = 0.0;
    // Cylinder / Cone data when available
    Point3d axisOrigin = Point3d(0.0, 0.0, 0.0);
    Vector3d axisDir = Vector3d(0.0, 0.0, 1.0);
    double radius = 0.0;
    double axisMin = 0.0;
    double axisMax = 0.0;
    double axisSpan = 0.0;
    bool isOuterCylinder = true;
};

struct CurvedStrokeSegment
{
    Point3d start;
    Point3d end;
};

struct CurvedStrokeArc
{
    Point3d center;
    double radius = 0.0;
    double startAngle = 0.0;
    double endAngle = 0.0;
};

struct CurvedEngravingParams
{
    std::string text;
    std::string fontName;
    double height = 5.0;
    double depth = 0.2;
    double widthScale = 100.0;
    double shear = 0.0;
    double vShapeWidth = 0.4;
    bool lockAspect = true;
    bool embossed = false;
    bool vShape = false;
    int layer = 1;
    int colorId = 186;
};

struct CurvedEngravingOutput
{
    std::vector<tag_t> constructionFeatureTags;
    std::vector<tag_t> previewObjectTags;
    std::vector<tag_t> resultCurveTags;
};

inline double VecDot(const Vector3d& a, const Vector3d& b)
{
    return a.X * b.X + a.Y * b.Y + a.Z * b.Z;
}

inline Vector3d VecCross(const Vector3d& a, const Vector3d& b)
{
    return Vector3d(
        a.Y * b.Z - a.Z * b.Y,
        a.Z * b.X - a.X * b.Z,
        a.X * b.Y - a.Y * b.X);
}

inline double VecMag(const Vector3d& v)
{
    return std::sqrt(v.X * v.X + v.Y * v.Y + v.Z * v.Z);
}

inline Vector3d VecNorm(const Vector3d& v, const Vector3d& fallback = Vector3d(1.0, 0.0, 0.0))
{
    const double m = VecMag(v);
    if (m <= kEps)
    {
        return fallback;
    }
    return Vector3d(v.X / m, v.Y / m, v.Z / m);
}

inline Vector3d PtSub(const Point3d& a, const Point3d& b)
{
    return Vector3d(a.X - b.X, a.Y - b.Y, a.Z - b.Z);
}

inline Point3d PtAdd(const Point3d& p, const Vector3d& v, double s = 1.0)
{
    return Point3d(p.X + v.X * s, p.Y + v.Y * s, p.Z + v.Z * s);
}

inline Vector3d ProjectTangent(const Vector3d& v, const Vector3d& n)
{
    const double d = VecDot(v, n);
    return Vector3d(v.X - n.X * d, v.Y - n.Y * d, v.Z - n.Z * d);
}

inline std::string FmtNum(double v)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(6) << v;
    std::string s = oss.str();
    while (!s.empty() && s.back() == '0') s.pop_back();
    if (!s.empty() && s.back() == '.') s.pop_back();
    if (s.empty() || s == "-0") s = "0";
    return s;
}

inline bool ContainsNonAscii(const std::string& text)
{
    for (unsigned char ch : text)
    {
        if (ch >= 0x80)
        {
            return true;
        }
    }
    return false;
}

inline bool IsPlanarFaceTag(tag_t faceTag)
{
    if (faceTag == NULL_TAG)
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
    if (UF_MODL_ask_face_data(faceTag, &type, origin, dir, box, &radius, &radData, &normDir) != 0)
    {
        return false;
    }
    return type == UF_MODL_PLANAR_FACE;
}

inline bool AskFaceInfo(Face* face, const Point3d* referencePoint, CurvedFaceInfo* outInfo)
{
    if (face == nullptr || outInfo == nullptr)
    {
        return false;
    }
    CurvedFaceInfo info;
    double origin[3] = {};
    double dir[3] = {};
    double box[6] = {};
    double radius = 0.0;
    double radData = 0.0;
    int normDir = 0;
    if (UF_MODL_ask_face_data(face->Tag(), &info.faceType, origin, dir, box, &radius, &radData, &normDir) != 0)
    {
        return false;
    }

    info.isPlanar = (info.faceType == UF_MODL_PLANAR_FACE);
    info.isCylindrical = (info.faceType == UF_MODL_CYLINDRICAL_FACE);
    info.axisOrigin = Point3d(origin[0], origin[1], origin[2]);
    info.axisDir = VecNorm(Vector3d(dir[0], dir[1], dir[2]), Vector3d(0.0, 0.0, 1.0));
    info.radius = radius;

    double uvMinMax[4] = {};
    if (UF_MODL_ask_face_uv_minmax(face->Tag(), uvMinMax) == 0)
    {
        info.uMin = uvMinMax[0];
        info.uMax = uvMinMax[1];
        info.vMin = uvMinMax[2];
        info.vMax = uvMinMax[3];
    }

    double parm[2] = {0.5 * (info.uMin + info.uMax), 0.5 * (info.vMin + info.vMax)};
    double facePt[3] = {origin[0], origin[1], origin[2]};
    bool hasValidPoint = false;
    if (referencePoint != nullptr &&
        std::isfinite(referencePoint->X) &&
        std::isfinite(referencePoint->Y) &&
        std::isfinite(referencePoint->Z))
    {
        double refPt[3] = {referencePoint->X, referencePoint->Y, referencePoint->Z};
        if (UF_MODL_ask_face_parm_2(face->Tag(), refPt, parm, facePt) == 0)
        {
            hasValidPoint = true;
        }
    }

    double propsPt[3] = {};
    double u1[3] = {};
    double v1[3] = {};
    double u2[3] = {};
    double v2[3] = {};
    double unitNormal[3] = {};
    double radii[2] = {};
    if (UF_MODL_ask_face_props(face->Tag(), parm, propsPt, u1, v1, u2, v2, unitNormal, radii) == 0)
    {
        info.point = hasValidPoint
                         ? Point3d(facePt[0], facePt[1], facePt[2])
                         : Point3d(propsPt[0], propsPt[1], propsPt[2]);
        info.normal = VecNorm(Vector3d(unitNormal[0], unitNormal[1], unitNormal[2]), Vector3d(0.0, 0.0, 1.0));
        info.uTangent = VecNorm(ProjectTangent(Vector3d(u1[0], u1[1], u1[2]), info.normal), Vector3d(1.0, 0.0, 0.0));
        info.vTangent = VecNorm(ProjectTangent(Vector3d(v1[0], v1[1], v1[2]), info.normal),
                                VecNorm(VecCross(info.normal, info.uTangent), Vector3d(0.0, 1.0, 0.0)));
        info.uParm = parm[0];
        info.vParm = parm[1];
    }
    else
    {
        info.point = Point3d(facePt[0], facePt[1], facePt[2]);
        Vector3d fallbackN(dir[0], dir[1], dir[2]);
        if (normDir < 0)
        {
            fallbackN = Vector3d(-fallbackN.X, -fallbackN.Y, -fallbackN.Z);
        }
        info.normal = VecNorm(fallbackN, Vector3d(0.0, 0.0, 1.0));
        Vector3d refX = std::fabs(info.normal.Z) < 0.9 ? Vector3d(0.0, 0.0, 1.0) : Vector3d(1.0, 0.0, 0.0);
        info.uTangent = VecNorm(VecCross(info.normal, refX), Vector3d(1.0, 0.0, 0.0));
        info.vTangent = VecNorm(VecCross(info.normal, info.uTangent), Vector3d(0.0, 1.0, 0.0));
    }

    if (info.isCylindrical)
    {
        // Compute axial extent from bounding box / edges
        double minA = std::numeric_limits<double>::max();
        double maxA = -std::numeric_limits<double>::max();
        const double corners[8][3] = {
            {box[0], box[1], box[2]}, {box[3], box[1], box[2]},
            {box[0], box[4], box[2]}, {box[3], box[4], box[2]},
            {box[0], box[1], box[5]}, {box[3], box[1], box[5]},
            {box[0], box[4], box[5]}, {box[3], box[4], box[5]}
        };
        for (int i = 0; i < 8; ++i)
        {
            const double proj = VecDot(PtSub(Point3d(corners[i][0], corners[i][1], corners[i][2]), info.axisOrigin), info.axisDir);
            minA = std::min(minA, proj);
            maxA = std::max(maxA, proj);
        }
        // Refine axial extent using actual face edges if available
        std::vector<Edge*> edges = face->GetEdges();
        if (!edges.empty())
        {
            double edgeMinA = std::numeric_limits<double>::max();
            double edgeMaxA = -std::numeric_limits<double>::max();
            bool anyEdgePt = false;
            for (Edge* edge : edges)
            {
                if (edge == nullptr) continue;
                Point3d v1p, v2p;
                edge->GetVertices(&v1p, &v2p);
                const double p1 = VecDot(PtSub(v1p, info.axisOrigin), info.axisDir);
                const double p2 = VecDot(PtSub(v2p, info.axisOrigin), info.axisDir);
                edgeMinA = std::min(edgeMinA, std::min(p1, p2));
                edgeMaxA = std::max(edgeMaxA, std::max(p1, p2));
                anyEdgePt = true;
            }
            if (anyEdgePt && edgeMaxA > edgeMinA + kEps)
            {
                minA = edgeMinA;
                maxA = edgeMaxA;
            }
        }
        info.axisMin = minA;
        info.axisMax = maxA;
        info.axisSpan = std::max(0.0, maxA - minA);

        // Determine if this cylindrical face is an outer face (normal points away from axis)
        const double projPt = VecDot(PtSub(info.point, info.axisOrigin), info.axisDir);
        const Point3d axisClosest = PtAdd(info.axisOrigin, info.axisDir, projPt);
        const Vector3d radial = PtSub(info.point, axisClosest);
        info.isOuterCylinder = VecDot(radial, info.normal) >= 0.0;
    }

    *outInfo = info;
    return true;
}

inline bool AskCurvedFaceLongShortAxes(Face* face,
                                       const CurvedFaceInfo& info,
                                       Vector3d* longAxisOut,
                                       Vector3d* shortAxisOut)
{
    if (face == nullptr || longAxisOut == nullptr || shortAxisOut == nullptr)
    {
        return false;
    }

    const Vector3d n = VecNorm(info.normal, Vector3d(0.0, 0.0, 1.0));
    if (info.isCylindrical)
    {
        // For cylindrical faces, longAxis is always the tube axis, shortAxis is circumferential.
        Vector3d longAx = VecNorm(ProjectTangent(info.axisDir, n), info.uTangent);
        Vector3d shortAx = VecNorm(VecCross(n, longAx), info.vTangent);
        *longAxisOut = longAx;
        *shortAxisOut = shortAx;
        return true;
    }

    // For general surfaces, compare approximate arc lengths along U and V at current parameter.
    auto evalSurfPt = [&](double u, double v, Point3d* pt) -> bool {
        double p[2] = {u, v};
        double pos[3] = {}, u1[3] = {}, v1[3] = {}, u2[3] = {}, v2[3] = {}, un[3] = {}, r[2] = {};
        if (UF_MODL_ask_face_props(face->Tag(), p, pos, u1, v1, u2, v2, un, r) != 0)
        {
            return false;
        }
        *pt = Point3d(pos[0], pos[1], pos[2]);
        return true;
    };

    double uLen = 0.0;
    double vLen = 0.0;
    const int steps = 8;
    Point3d prev;
    bool hasPrev = false;
    for (int i = 0; i <= steps; ++i)
    {
        const double u = info.uMin + (info.uMax - info.uMin) * (static_cast<double>(i) / steps);
        Point3d cur;
        if (evalSurfPt(u, info.vParm, &cur))
        {
            if (hasPrev) uLen += VecMag(PtSub(cur, prev));
            prev = cur;
            hasPrev = true;
        }
    }
    hasPrev = false;
    for (int i = 0; i <= steps; ++i)
    {
        const double v = info.vMin + (info.vMax - info.vMin) * (static_cast<double>(i) / steps);
        Point3d cur;
        if (evalSurfPt(info.uParm, v, &cur))
        {
            if (hasPrev) vLen += VecMag(PtSub(cur, prev));
            prev = cur;
            hasPrev = true;
        }
    }

    Vector3d primary = (uLen >= vLen) ? info.uTangent : info.vTangent;
    primary = VecNorm(ProjectTangent(primary, n), Vector3d(1.0, 0.0, 0.0));
    Vector3d secondary = VecNorm(VecCross(n, primary), Vector3d(0.0, 1.0, 0.0));
    *longAxisOut = primary;
    *shortAxisOut = secondary;
    return true;
}

inline Point3d ApplyCurvedFaceCenterOptions(Face* face,
                                            const CurvedFaceInfo& info,
                                            bool centerLongSide,
                                            bool centerShortSide)
{
    if (face == nullptr || (!centerLongSide && !centerShortSide))
    {
        return info.point;
    }

    if (info.isCylindrical)
    {
        Point3d pt = info.point;
        if (centerLongSide && info.axisSpan > kEps)
        {
            const double midA = 0.5 * (info.axisMin + info.axisMax);
            const double curA = VecDot(PtSub(pt, info.axisOrigin), info.axisDir);
            pt = PtAdd(pt, info.axisDir, midA - curA);
        }
        // For full 360 cylinder, circumferential center is arbitrary, keep picked angle.
        // Re-project to face to guarantee point lies strictly on surface.
        CurvedFaceInfo snapped;
        if (AskFaceInfo(face, &pt, &snapped))
        {
            return snapped.point;
        }
        return pt;
    }

    // General surface: center along U and/or V mid-parameter
    double targetU = info.uParm;
    double targetV = info.vParm;
    Vector3d longAx, shortAx;
    AskCurvedFaceLongShortAxes(face, info, &longAx, &shortAx);
    const bool uIsLong = std::fabs(VecDot(longAx, info.uTangent)) >= std::fabs(VecDot(longAx, info.vTangent));
    if (centerLongSide)
    {
        if (uIsLong) targetU = 0.5 * (info.uMin + info.uMax);
        else targetV = 0.5 * (info.vMin + info.vMax);
    }
    if (centerShortSide)
    {
        if (uIsLong) targetV = 0.5 * (info.vMin + info.vMax);
        else targetU = 0.5 * (info.uMin + info.uMax);
    }

    double p[2] = {targetU, targetV};
    double pos[3] = {}, u1[3] = {}, v1[3] = {}, u2[3] = {}, v2[3] = {}, un[3] = {}, r[2] = {};
    if (UF_MODL_ask_face_props(face->Tag(), p, pos, u1, v1, u2, v2, un, r) == 0)
    {
        return Point3d(pos[0], pos[1], pos[2]);
    }
    return info.point;
}

inline tag_t CreateSplineThruPoints(const std::vector<Point3d>& pts)
{
    if (pts.size() < 2)
    {
        return NULL_TAG;
    }
    if (pts.size() == 2)
    {
        // Degenerate 2-point spline -> simple line
        UF_CURVE_line_t lineCoords = {};
        lineCoords.start_point[0] = pts[0].X;
        lineCoords.start_point[1] = pts[0].Y;
        lineCoords.start_point[2] = pts[0].Z;
        lineCoords.end_point[0] = pts[1].X;
        lineCoords.end_point[1] = pts[1].Y;
        lineCoords.end_point[2] = pts[1].Z;
        tag_t lineTag = NULL_TAG;
        if (UF_CURVE_create_line(&lineCoords, &lineTag) == 0)
        {
            return lineTag;
        }
        return NULL_TAG;
    }

    std::vector<UF_CURVE_pt_slope_crvatr_t> data(pts.size());
    for (std::size_t i = 0; i < pts.size(); ++i)
    {
        data[i] = {};
        data[i].point[0] = pts[i].X;
        data[i].point[1] = pts[i].Y;
        data[i].point[2] = pts[i].Z;
        data[i].slope_type = (i == 0 || i + 1 == pts.size()) ? UF_CURVE_SLOPE_AUTO : UF_CURVE_SLOPE_NONE;
        data[i].crvatr_type = UF_CURVE_CRVATR_NONE;
    }
    const int degree = pts.size() >= 4 ? 3 : static_cast<int>(pts.size() - 1);
    tag_t splineTag = NULL_TAG;
    if (UF_CURVE_create_spline_thru_pts(
            degree,
            0,
            static_cast<int>(data.size()),
            data.data(),
            nullptr,
            0,
            &splineTag) != 0)
    {
        return NULL_TAG;
    }
    return splineTag;
}

inline Point3d MapPointOntoCylinder(const CurvedFaceInfo& info,
                                    const Point3d& textOrigin,
                                    const Vector3d& xAxis,
                                    const Vector3d& yAxis,
                                    double localX,
                                    double localY)
{
    const Vector3d A = info.axisDir;
    const double originAxial = VecDot(PtSub(textOrigin, info.axisOrigin), A);
    const Point3d C0 = PtAdd(info.axisOrigin, A, originAxial);
    const Vector3d radialVec = PtSub(textOrigin, C0);
    const double R = std::max(VecMag(radialVec), info.radius > kEps ? info.radius : 1.0);
    const Vector3d Rhat = VecNorm(radialVec, info.normal);
    const Vector3d That = VecNorm(VecCross(A, Rhat), Vector3d(1.0, 0.0, 0.0));

    const Vector3d d(
        xAxis.X * localX + yAxis.X * localY,
        xAxis.Y * localX + yAxis.Y * localY,
        xAxis.Z * localX + yAxis.Z * localY);
    const double dz = VecDot(d, A);
    const double ds = VecDot(d, That);
    const double theta = ds / R;

    const double cosT = std::cos(theta);
    const double sinT = std::sin(theta);
    return Point3d(
        C0.X + A.X * dz + R * (cosT * Rhat.X + sinT * That.X),
        C0.Y + A.Y * dz + R * (cosT * Rhat.Y + sinT * That.Y),
        C0.Z + A.Z * dz + R * (cosT * Rhat.Z + sinT * That.Z));
}

inline std::vector<Curve*> ProjectCurvesEqualArcLength(Part* workPart,
                                                       Face* face,
                                                       const std::vector<Curve*>& flatCurves,
                                                       const Point3d& refOrigin,
                                                       const Vector3d& faceNormal,
                                                       const Vector3d& xAxis,
                                                       bool associative,
                                                       tag_t* featureTagOut)
{
    std::vector<Curve*> result;
    if (featureTagOut != nullptr) *featureTagOut = NULL_TAG;
    if (workPart == nullptr || face == nullptr || flatCurves.empty())
    {
        return result;
    }

    auto tryProject = [&](bool equalArcLength) -> std::vector<Curve*> {
        std::vector<Curve*> projected;
        Features::ProjectCurveBuilder* projBuilder = workPart->Features()->CreateProjectCurveBuilder(nullptr);
        if (projBuilder == nullptr)
        {
            return projected;
        }
        try
        {
            projBuilder->SetTolerance(0.01);
            projBuilder->SectionToProject()->SetDistanceTolerance(0.01);
            projBuilder->SectionToProject()->SetChainingTolerance(0.01);
            projBuilder->SectionToProject()->SetAllowedEntityTypes(Section::AllowTypesOnlyCurves);

            SelectionIntentRuleOptions* ruleOpts = workPart->ScRuleFactory()->CreateRuleOptions();
            ruleOpts->SetSelectedFromInactive(false);
            std::vector<IBaseCurve*> baseCurves;
            baseCurves.reserve(flatCurves.size());
            for (Curve* c : flatCurves)
            {
                if (c != nullptr) baseCurves.push_back(c);
            }
            CurveDumbRule* rule = workPart->ScRuleFactory()->CreateRuleBaseCurveDumb(baseCurves, ruleOpts);
            delete ruleOpts;
            std::vector<SelectionIntentRule*> rules(1, rule);
            projBuilder->SectionToProject()->AddToSection(
                rules, flatCurves.front(), nullptr, nullptr, refOrigin, Section::ModeCreate, false);

            projBuilder->FaceToProjectTo()->Add(face);
            projBuilder->InputCurvesOption()->SetAssociative(associative);

            if (equalArcLength)
            {
                projBuilder->SetProjectionDirectionMethod(
                    Features::ProjectCurveBuilder::DirectionTypeAlongVector);
                projBuilder->SetProjectionOption(
                    Features::ProjectCurveBuilder::ProjectionOptionTypeEqualArcLength);
                projBuilder->SetEqualArcLengthMethod(
                    Features::ProjectCurveBuilder::ArcLengthTypeBothXY);

                Vector3d projDir(-faceNormal.X, -faceNormal.Y, -faceNormal.Z);
                Direction* dirObj = workPart->Directions()->CreateDirection(
                    refOrigin, projDir, SmartObject::UpdateOptionWithinModeling);
                projBuilder->SetProjectionVector(dirObj);

                Direction* xDirObj = workPart->Directions()->CreateDirection(
                    refOrigin, xAxis, SmartObject::UpdateOptionWithinModeling);
                projBuilder->SetXVectorForEqualArcLength(xDirObj);

                Point* refPtObj = workPart->Points()->CreatePoint(refOrigin);
                projBuilder->SetReferencePointForEqualArcLength(refPtObj);
            }
            else
            {
                projBuilder->SetProjectionDirectionMethod(
                    Features::ProjectCurveBuilder::DirectionTypeAlongFaceNormal);
                projBuilder->SetProjectionOption(
                    Features::ProjectCurveBuilder::ProjectionOptionTypeNone);
            }

            NXObject* committed = projBuilder->Commit();
            Features::Feature* feat = dynamic_cast<Features::Feature*>(committed);
            if (feat != nullptr)
            {
                if (featureTagOut != nullptr) *featureTagOut = feat->Tag();
                for (NXObject* ent : feat->GetEntities())
                {
                    if (Curve* c = dynamic_cast<Curve*>(ent))
                    {
                        projected.push_back(c);
                    }
                }
            }
            projBuilder->Destroy();
        }
        catch (...)
        {
            try { projBuilder->Destroy(); } catch (...) {}
            projected.clear();
        }
        return projected;
    };

    result = tryProject(true);
    if (result.empty())
    {
        result = tryProject(false);
    }
    return result;
}

inline Curve* BuildGuideCurveOnFace(Part* workPart,
                                    Face* face,
                                    const CurvedFaceInfo& info,
                                    const Point3d& textOrigin,
                                    const Vector3d& xAxis,
                                    const Vector3d& yAxis,
                                    double estimatedTextSpan,
                                    std::vector<tag_t>& tempObjectsToClean)
{
    const double halfSpan = std::max(estimatedTextSpan * 0.75, 10.0);

    if (info.isCylindrical && info.radius > kEps)
    {
        const Vector3d A = info.axisDir;
        const double dotAxis = VecDot(xAxis, A);
        const double originAxial = VecDot(PtSub(textOrigin, info.axisOrigin), A);
        const Point3d C0 = PtAdd(info.axisOrigin, A, originAxial);
        const double R = std::max(VecMag(PtSub(textOrigin, C0)), info.radius);
        const Vector3d Rhat = VecNorm(PtSub(textOrigin, C0), info.normal);

        // Case 1: Axial guide line on cylinder
        if (std::fabs(dotAxis) > 0.995)
        {
            const Vector3d dir = dotAxis >= 0.0 ? A : Vector3d(-A.X, -A.Y, -A.Z);
            // Clamp halfSpan so guide stays within cylinder axial bounds if possible
            double maxHalf = halfSpan;
            if (info.axisSpan > kEps)
            {
                const double distToMax = std::max(0.5, info.axisMax - originAxial);
                const double distToMin = std::max(0.5, originAxial - info.axisMin);
                const double symmetricBound = std::min(distToMax, distToMin);
                if (symmetricBound > 1.0)
                {
                    maxHalf = std::min(halfSpan, symmetricBound * 0.98);
                }
            }
            Point3d pStart = PtAdd(textOrigin, dir, -maxHalf);
            Point3d pEnd = PtAdd(textOrigin, dir, maxHalf);
            Line* line = workPart->Curves()->CreateLine(pStart, pEnd);
            if (line != nullptr)
            {
                tempObjectsToClean.push_back(line->Tag());
                return line;
            }
        }

        // Case 2: Circumferential arc on cylinder
        if (std::fabs(dotAxis) < 0.05)
        {
            const double maxHalfAngle = std::min(halfSpan / R, 150.0 * kPi / 180.0);
            const Vector3d arcX = Rhat;
            const Vector3d arcY = VecNorm(xAxis, VecCross(A, Rhat));
            NXOpen::Arc* arc = workPart->Curves()->CreateArc(
                C0, arcX, arcY, R, -maxHalfAngle, maxHalfAngle);
            if (arc != nullptr)
            {
                tempObjectsToClean.push_back(arc->Tag());
                return arc;
            }
        }

        // Case 3: Helical spline on cylinder
        const int numPts = 21;
        std::vector<Point3d> pts;
        pts.reserve(numPts);
        const double clampedHalf = std::min(halfSpan, R * 2.5);
        for (int i = 0; i < numPts; ++i)
        {
            const double t = -1.0 + 2.0 * static_cast<double>(i) / (numPts - 1);
            pts.push_back(MapPointOntoCylinder(info, textOrigin, xAxis, yAxis, t * clampedHalf, 0.0));
        }
        tag_t spTag = CreateSplineThruPoints(pts);
        if (spTag != NULL_TAG)
        {
            tempObjectsToClean.push_back(spTag);
            return dynamic_cast<Curve*>(NXObjectManager::Get(spTag));
        }
    }

    // General freeform surface: create a short line in the tangent plane and project equal-arc-length
    Point3d pStart = PtAdd(textOrigin, xAxis, -halfSpan);
    Point3d pEnd = PtAdd(textOrigin, xAxis, halfSpan);
    Line* flatLine = workPart->Curves()->CreateLine(pStart, pEnd);
    if (flatLine == nullptr)
    {
        return nullptr;
    }
    std::vector<Curve*> seed(1, flatLine);
    tag_t projFeatTag = NULL_TAG;
    std::vector<Curve*> projected = ProjectCurvesEqualArcLength(
        workPart, face, seed, textOrigin, info.normal, xAxis, false, &projFeatTag);
    try { UF_OBJ_delete_object(flatLine->Tag()); } catch (...) {}
    if (projFeatTag != NULL_TAG)
    {
        tempObjectsToClean.push_back(projFeatTag);
    }
    if (!projected.empty())
    {
        for (Curve* c : projected)
        {
            if (c != nullptr) tempObjectsToClean.push_back(c->Tag());
        }
        return projected.front();
    }
    return nullptr;
}

inline void ApplyLayerAndColorToObjects(Part* workPart,
                                        const std::vector<DisplayableObject*>& objects,
                                        int layer,
                                        int colorId)
{
    if (workPart == nullptr || objects.empty())
    {
        return;
    }
    try
    {
        Session* session = Session::GetSession();
        DisplayModification* mod = session != nullptr ? session->DisplayManager()->NewDisplayModification() : nullptr;
        if (mod != nullptr)
        {
            if (layer >= 1 && layer <= 256)
            {
                mod->SetNewLayer(layer);
            }
            if (colorId > 0)
            {
                mod->SetNewColor(colorId);
            }
            mod->Apply(objects);
            delete mod;
        }
    }
    catch (...)
    {
    }
}

inline void BlankObjectsQuietly(const std::vector<tag_t>& tags)
{
    for (tag_t t : tags)
    {
        if (t != NULL_TAG && UF_OBJ_ask_status(t) == UF_OBJ_ALIVE)
        {
            try { UF_OBJ_set_blank_status(t, UF_OBJ_BLANKED); } catch (...) {}
        }
    }
}

inline std::vector<Face*> IdentifyTextStrokeFacesByTopologyBfs(
    Body* targetBody,
    const std::set<tag_t>& preExistingFaceTags,
    const std::vector<Face*>& dividedNewFaces)
{
    std::vector<Face*> strokeFaces;
    if (targetBody == nullptr || dividedNewFaces.empty())
    {
        return strokeFaces;
    }

    // Collect all candidate faces belonging to the divided region (the original face became one of these)
    std::set<tag_t> candidateTags;
    std::map<tag_t, Face*> tagToFace;
    std::map<tag_t, double> faceAreaApprox;

    for (Face* f : dividedNewFaces)
    {
        if (f != nullptr && UF_OBJ_ask_status(f->Tag()) == UF_OBJ_ALIVE)
        {
            candidateTags.insert(f->Tag());
            tagToFace[f->Tag()] = f;
        }
    }

    // Also include any face on targetBody that shares an edge with dividedNewFaces and wasn't outside
    // First compute approximate bounding-box area for every candidate face to find the outer background face
    auto approxFaceSize = [](Face* f) -> double {
        double box[6] = {};
        if (UF_MODL_ask_bounding_box(f->Tag(), box) != 0)
        {
            return 0.0;
        }
        const double dx = std::max(0.0, box[3] - box[0]);
        const double dy = std::max(0.0, box[4] - box[1]);
        const double dz = std::max(0.0, box[5] - box[2]);
        return dx * dy + dy * dz + dz * dx;
    };

    // Build adjacency among candidate faces via shared edges
    std::map<tag_t, std::set<tag_t>> adjacency;
    std::set<tag_t> touchesPreExistingBodyFaces;
    for (tag_t fTag : candidateTags)
    {
        Face* f = tagToFace[fTag];
        faceAreaApprox[fTag] = approxFaceSize(f);
        for (Edge* e : f->GetEdges())
        {
            if (e == nullptr) continue;
            for (Face* adj : e->GetFaces())
            {
                if (adj == nullptr) continue;
                const tag_t adjTag = adj->Tag();
                if (adjTag == fTag) continue;
                if (candidateTags.count(adjTag) > 0)
                {
                    adjacency[fTag].insert(adjTag);
                }
                else if (preExistingFaceTags.count(adjTag) > 0)
                {
                    touchesPreExistingBodyFaces.insert(fTag);
                }
            }
        }
    }

    // Root(s) at BFS depth 0 = background face(s):
    // Prefer faces touching pre-existing body boundary faces; fallback to the single largest candidate face.
    std::map<tag_t, int> depthMap;
    std::queue<tag_t> q;
    if (!touchesPreExistingBodyFaces.empty())
    {
        for (tag_t bgTag : touchesPreExistingBodyFaces)
        {
            depthMap[bgTag] = 0;
            q.push(bgTag);
        }
    }
    else
    {
        tag_t largestTag = NULL_TAG;
        double largestArea = -1.0;
        for (const auto& kv : faceAreaApprox)
        {
            if (kv.second > largestArea)
            {
                largestArea = kv.second;
                largestTag = kv.first;
            }
        }
        if (largestTag != NULL_TAG)
        {
            depthMap[largestTag] = 0;
            q.push(largestTag);
        }
    }

    while (!q.empty())
    {
        const tag_t cur = q.front();
        q.pop();
        const int curDepth = depthMap[cur];
        for (tag_t nb : adjacency[cur])
        {
            if (depthMap.find(nb) == depthMap.end())
            {
                depthMap[nb] = curDepth + 1;
                q.push(nb);
            }
        }
    }

    // Odd BFS depths (1, 3, 5, ...) are interior text stroke faces;
    // even depths (0 = background, 2 = inner holes of O/A/B/8/0) are excluded.
    for (const auto& kv : depthMap)
    {
        if ((kv.second % 2) == 1)
        {
            strokeFaces.push_back(tagToFace[kv.first]);
        }
    }
    return strokeFaces;
}

inline void CreateCurvedRegularTextEngraving(
    Session* session,
    Part* workPart,
    Face* face,
    Body* targetBody,
    const CurvedFaceInfo& info,
    const Point3d& textOrigin,
    const Matrix3x3& textMatrix,
    const CurvedEngravingParams& params,
    bool commitGeometry,
    const std::function<void(const std::string&)>& logFn,
    CurvedEngravingOutput* out)
{
    if (workPart == nullptr || face == nullptr || targetBody == nullptr || out == nullptr)
    {
        throw std::runtime_error("Curved text engraving requires valid work part, face, and body.");
    }

    const Vector3d xAxis = VecNorm(Vector3d(textMatrix.Xx, textMatrix.Xy, textMatrix.Xz), info.uTangent);
    const Vector3d yAxis = VecNorm(Vector3d(textMatrix.Yx, textMatrix.Yy, textMatrix.Yz), info.vTangent);
    const double estimatedSpan = std::max(
        params.height * std::max<double>(1.0, static_cast<double>(params.text.size())) * (params.widthScale / 100.0),
        params.height * 3.0);

    std::vector<tag_t> guideTempTags;
    Curve* guideCurve = BuildGuideCurveOnFace(
        workPart, face, info, textOrigin, xAxis, yAxis, estimatedSpan, guideTempTags);
    if (guideCurve == nullptr)
    {
        throw std::runtime_error("Could not create an on-face guide curve for curved surface text.");
    }

    auto buildTextFeature = [&](bool reverseApex, Features::Feature** featOut) -> std::vector<Curve*> {
        std::vector<Curve*> curves;
        if (featOut != nullptr) *featOut = nullptr;
        Features::TextBuilder* tb = workPart->Features()->CreateTextBuilder(nullptr);
        if (tb == nullptr)
        {
            throw std::runtime_error("CreateTextBuilder returned NULL.");
        }
        try
        {
            tb->SetType(Features::TextBuilder::TypesOnFace);
            tb->SetOnFacePlacementMethod(Features::TextBuilder::OnFacePlacementMethodOptionsCurvesOnFaces);
            tb->SetAssociative(true);
            tb->SetCanJoinCurves(true);
            tb->SetCanProjectCurves(false);

            // Target face
            SelectionIntentRuleOptions* faceOpts = workPart->ScRuleFactory()->CreateRuleOptions();
            faceOpts->SetSelectedFromInactive(false);
            std::vector<Face*> faces(1, face);
            FaceDumbRule* faceRule = workPart->ScRuleFactory()->CreateRuleFaceDumb(faces, faceOpts);
            delete faceOpts;
            std::vector<SelectionIntentRule*> faceRules(1, faceRule);
            tb->PlacementFaces()->ReplaceRules(faceRules, false);

            // Placement profile (guide curve)
            tb->OnFacePlacementProfile()->SetDistanceTolerance(0.01);
            tb->OnFacePlacementProfile()->SetChainingTolerance(0.01);
            tb->OnFacePlacementProfile()->SetAllowedEntityTypes(Section::AllowTypesOnlyCurves);
            SelectionIntentRuleOptions* crvOpts = workPart->ScRuleFactory()->CreateRuleOptions();
            crvOpts->SetSelectedFromInactive(false);
            std::vector<IBaseCurve*> guideCurves(1, guideCurve);
            CurveDumbRule* crvRule = workPart->ScRuleFactory()->CreateRuleBaseCurveDumb(guideCurves, crvOpts);
            delete crvOpts;
            std::vector<SelectionIntentRule*> crvRules(1, crvRule);
            tb->OnFacePlacementProfile()->AddToSection(
                crvRules, guideCurve, nullptr, nullptr, textOrigin, Section::ModeCreate, false);

            // Font & text
            const std::string fontToUse = params.fontName.empty() ? "Arial" : params.fontName;
            const Features::TextBuilder::ScriptOptions script =
                ContainsNonAscii(params.text)
                    ? Features::TextBuilder::ScriptOptionsGb2312
                    : Features::TextBuilder::ScriptOptionsWestern;
            try
            {
                tb->SelectFont(NXString(fontToUse.c_str(), NXString::UTF8), script);
            }
            catch (...)
            {
                tb->SelectFont("Arial", script);
            }
            tb->SetTextString(NXString(params.text.c_str(), NXString::UTF8));

            // Frame on path
            tb->FrameOnPath()->SetAnchorLocation(
                GeometricUtilities::FrameOnPathBuilder::AnchorLocationTypeCenter);
            tb->FrameOnPath()->AnchorPosition()->SetPercentUsed(true);
            tb->FrameOnPath()->AnchorPosition()->Expression()->SetFormula("50");
            tb->FrameOnPath()->Height()->SetFormula(FmtNum(params.height).c_str());
            tb->FrameOnPath()->SetWScale(params.widthScale);
            tb->FrameOnPath()->SetLockAspectRatio(params.lockAspect);
            tb->FrameOnPath()->Offset()->SetFormula(FmtNum(-params.height * 0.5).c_str());
            tb->FrameOnPath()->SetApexReversed(reverseApex);

            Features::Feature* textFeat = tb->CommitFeature();
            tb->Destroy();
            if (textFeat != nullptr)
            {
                if (featOut != nullptr) *featOut = textFeat;
                for (NXObject* ent : textFeat->GetEntities())
                {
                    if (Curve* c = dynamic_cast<Curve*>(ent))
                    {
                        curves.push_back(c);
                    }
                }
            }
        }
        catch (...)
        {
            try { tb->Destroy(); } catch (...) {}
            throw;
        }
        return curves;
    };

    Features::Feature* textFeature = nullptr;
    std::vector<Curve*> textCurves = buildTextFeature(false, &textFeature);
    if (textCurves.empty())
    {
        for (tag_t t : guideTempTags)
        {
            try { UF_OBJ_delete_object(t); } catch (...) {}
        }
        throw std::runtime_error("On-face TextBuilder produced no curves.");
    }

    // Blank helper guide curve so it doesn't clutter the model
    BlankObjectsQuietly(guideTempTags);

    if (!commitGeometry)
    {
        // Preview mode: keep preview curves on the current visible work layer (layer=0)
        // so they remain visible even when the user's configured textLayer (e.g. 254) is hidden.
        std::vector<DisplayableObject*> dispObjs;
        for (Curve* c : textCurves)
        {
            if (c != nullptr) dispObjs.push_back(c);
        }
        ApplyLayerAndColorToObjects(workPart, dispObjs, 0, params.colorId);
        if (textFeature != nullptr)
        {
            out->previewObjectTags.push_back(textFeature->Tag());
        }
        for (tag_t t : guideTempTags)
        {
            out->previewObjectTags.push_back(t);
        }
        if (logFn) logFn("曲面刻字预览: 已生成在面上文本曲线，数量=" + std::to_string(textCurves.size()));
        return;
    }

    if (textFeature != nullptr)
    {
        out->constructionFeatureTags.push_back(textFeature->Tag());
    }

    // Depth == 0: curve-only mode (for laser tube marking)
    if (params.depth <= 1.0e-6)
    {
        std::vector<DisplayableObject*> dispObjs;
        for (Curve* c : textCurves)
        {
            if (c != nullptr)
            {
                dispObjs.push_back(c);
                out->resultCurveTags.push_back(c->Tag());
            }
        }
        ApplyLayerAndColorToObjects(workPart, dispObjs, commitGeometry ? params.layer : 0, params.colorId);
        if (logFn) logFn("曲面刻字(仅曲线模式): 深度为0，保留贴面曲线数量=" + std::to_string(textCurves.size()));
        return;
    }

    // Depth > 0: DivideFace -> Topology BFS identify text stroke faces -> Thicken (or OffsetRegion fallback)
    std::set<tag_t> preExistingBodyFaces;
    for (Face* bf : targetBody->GetFaces())
    {
        if (bf != nullptr && bf->Tag() != face->Tag())
        {
            preExistingBodyFaces.insert(bf->Tag());
        }
    }

    Features::DividefaceBuilder* divBuilder = workPart->Features()->CreateDividefaceBuilder(nullptr);
    if (divBuilder == nullptr)
    {
        throw std::runtime_error("CreateDividefaceBuilder returned NULL.");
    }
    Features::Feature* divideFeature = nullptr;
    try
    {
        SelectionIntentRuleOptions* faceOpts = workPart->ScRuleFactory()->CreateRuleOptions();
        faceOpts->SetSelectedFromInactive(false);
        std::vector<Face*> faces(1, face);
        FaceDumbRule* faceRule = workPart->ScRuleFactory()->CreateRuleFaceDumb(faces, faceOpts);
        delete faceOpts;
        std::vector<SelectionIntentRule*> faceRules(1, faceRule);
        ScCollector* faceCollector = workPart->ScCollectors()->CreateCollector();
        faceCollector->ReplaceRules(faceRules, false);
        divBuilder->SetFacesToDivide(faceCollector);

        divBuilder->SelectDividingObject()->SetToolOption(
            GeometricUtilities::SelectDividingObjectBuilder::ToolTypeObject);

        // DividefaceBuilder::SelectDividingObject()->DividingObjectsList() requires a Section
        // (or ScCollector/DatumPlane) rather than raw Curve* pointers; adding each curve into
        // a Section ensures all disconnected character loops are included.
        Section* divSection = workPart->Sections()->CreateSection(0.01, 0.01, 0.5);
        divSection->SetAllowedEntityTypes(Section::AllowTypesOnlyCurves);
        divSection->AllowSelfIntersection(true);
        for (Curve* c : textCurves)
        {
            if (c == nullptr) continue;
            SelectionIntentRuleOptions* crvOpts = workPart->ScRuleFactory()->CreateRuleOptions();
            crvOpts->SetSelectedFromInactive(false);
            std::vector<IBaseCurve*> singleCurve(1, c);
            CurveDumbRule* crvRule = workPart->ScRuleFactory()->CreateRuleBaseCurveDumb(singleCurve, crvOpts);
            delete crvOpts;
            std::vector<SelectionIntentRule*> crvRules(1, crvRule);
            divSection->AddToSection(crvRules, c, nullptr, nullptr, textOrigin, Section::ModeCreate, false);
        }
        divBuilder->SelectDividingObject()->DividingObjectsList()->Add(divSection);
        if (logFn)
        {
            logFn("曲面刻字分割对象Section已加入，曲线数=" + std::to_string(textCurves.size()) +
                  ", DividingObjectsList大小=" +
                  std::to_string(divBuilder->SelectDividingObject()->DividingObjectsList()->Size()));
        }
        divBuilder->ProjectionOption()->SetProjectDirectionMethod(
            GeometricUtilities::ProjectionOptions::DirectionTypeFaceNormal);
        divBuilder->SetProjectCurvesThatLieOnFaceOption(true);
        divBuilder->SetBlankOption(true);
        divBuilder->SetExtendOption(false);
        divBuilder->SetTolerance(0.01);

        divideFeature = divBuilder->CommitFeature();
        divBuilder->Destroy();
    }
    catch (...)
    {
        try { divBuilder->Destroy(); } catch (...) {}
        throw;
    }

    if (divideFeature == nullptr)
    {
        throw std::runtime_error("DividefaceBuilder failed to split the curved face with text curves.");
    }
    out->constructionFeatureTags.push_back(divideFeature->Tag());

    // Collect all faces belonging to the original face after division
    std::vector<Face*> postDivideCandidateFaces;
    for (Face* bf : targetBody->GetFaces())
    {
        if (bf != nullptr && preExistingBodyFaces.count(bf->Tag()) == 0)
        {
            postDivideCandidateFaces.push_back(bf);
        }
    }

    std::vector<Face*> strokeFaces = IdentifyTextStrokeFacesByTopologyBfs(
        targetBody, preExistingBodyFaces, postDivideCandidateFaces);
    if (logFn)
    {
        logFn("曲面刻字分割面完成: 候选子面=" + std::to_string(postDivideCandidateFaces.size()) +
              ", 识别文字笔画面=" + std::to_string(strokeFaces.size()));
    }
    if (strokeFaces.empty())
    {
        throw std::runtime_error("Could not identify interior text stroke faces after DivideFace.");
    }

    // Track pre-thicken faces so we can color the newly created engraved/embossed faces
    std::set<tag_t> facesBeforeSolidOp;
    for (Face* bf : targetBody->GetFaces())
    {
        if (bf != nullptr) facesBeforeSolidOp.insert(bf->Tag());
    }

    // Primary: ThickenBuilder along face normals with Subtract (engrave) or Unite (emboss)
    bool solidSucceeded = false;
    try
    {
        Features::ThickenBuilder* thicken = workPart->Features()->CreateThickenBuilder(nullptr);
        if (thicken != nullptr)
        {
            try
            {
                SelectionIntentRuleOptions* ruleOpts = workPart->ScRuleFactory()->CreateRuleOptions();
                ruleOpts->SetSelectedFromInactive(false);
                FaceDumbRule* fRule = workPart->ScRuleFactory()->CreateRuleFaceDumb(strokeFaces, ruleOpts);
                delete ruleOpts;
                std::vector<SelectionIntentRule*> fRules(1, fRule);
                thicken->FaceCollector()->ReplaceRules(fRules, false);

                const double clearance = std::min(0.05, params.depth * 0.25);
                if (params.embossed)
                {
                    // Emboss outward: +depth outward, -clearance inward into body for clean unite
                    thicken->FirstOffset()->SetFormula(FmtNum(params.depth).c_str());
                    thicken->SecondOffset()->SetFormula(FmtNum(-clearance).c_str());
                    thicken->BooleanOperation()->SetType(
                        GeometricUtilities::BooleanOperation::BooleanTypeUnite);
                }
                else
                {
                    // Engrave inward: +clearance outward to avoid coplanar artifacts, -depth inward
                    thicken->FirstOffset()->SetFormula(FmtNum(clearance).c_str());
                    thicken->SecondOffset()->SetFormula(FmtNum(-params.depth).c_str());
                    thicken->BooleanOperation()->SetType(
                        GeometricUtilities::BooleanOperation::BooleanTypeSubtract);
                }
                std::vector<Body*> targets(1, targetBody);
                thicken->BooleanOperation()->SetTargetBodies(targets);
                thicken->SetTolerance(0.01);

                Features::Feature* thickenFeat = thicken->CommitFeature();
                thicken->Destroy();
                if (thickenFeat != nullptr)
                {
                    out->constructionFeatureTags.push_back(thickenFeat->Tag());
                    solidSucceeded = true;
                    if (logFn) logFn("曲面刻字: Thicken 等深法向布尔成功");
                }
            }
            catch (const NXException& ex)
            {
                if (logFn) logFn(std::string("曲面刻字: Thicken 失败，将尝试 OffsetRegion 兜底: ") + ex.Message());
                try { thicken->Destroy(); } catch (...) {}
            }
            catch (...)
            {
                try { thicken->Destroy(); } catch (...) {}
            }
        }
    }
    catch (...)
    {
    }

    // Fallback: OffsetRegionBuilder directly offsets the stroke faces inward/outward
    if (!solidSucceeded)
    {
        Features::OffsetRegionBuilder* offBuilder = workPart->Features()->CreateOffsetRegionBuilder(nullptr);
        if (offBuilder == nullptr)
        {
            throw std::runtime_error("Both ThickenBuilder and OffsetRegionBuilder failed on curved surface.");
        }
        try
        {
            SelectionIntentRuleOptions* ruleOpts = workPart->ScRuleFactory()->CreateRuleOptions();
            ruleOpts->SetSelectedFromInactive(false);
            FaceDumbRule* fRule = workPart->ScRuleFactory()->CreateRuleFaceDumb(strokeFaces, ruleOpts);
            delete ruleOpts;
            std::vector<SelectionIntentRule*> fRules(1, fRule);
            offBuilder->Face()->ReplaceRules(fRules, false);
            offBuilder->Distance()->SetFormula(FmtNum(params.depth).c_str());
            offBuilder->SetReverseDirection(!params.embossed);

            Features::Feature* offFeat = offBuilder->CommitFeature();
            offBuilder->Destroy();
            if (offFeat != nullptr)
            {
                out->constructionFeatureTags.push_back(offFeat->Tag());
                solidSucceeded = true;
                if (logFn) logFn("曲面刻字: OffsetRegion 兜底成功");
            }
        }
        catch (...)
        {
            try { offBuilder->Destroy(); } catch (...) {}
            throw;
        }
    }

    // Color newly created engraved/embossed faces
    if (solidSucceeded && params.colorId > 0)
    {
        std::vector<DisplayableObject*> coloredFaces;
        for (Face* f : strokeFaces)
        {
            if (f != nullptr && UF_OBJ_ask_status(f->Tag()) == UF_OBJ_ALIVE)
            {
                coloredFaces.push_back(f);
            }
        }
        for (Face* bf : targetBody->GetFaces())
        {
            if (bf != nullptr && facesBeforeSolidOp.count(bf->Tag()) == 0)
            {
                coloredFaces.push_back(bf);
            }
        }
        ApplyLayerAndColorToObjects(workPart, coloredFaces, 0, params.colorId);
    }
}

inline void CreateCurvedSingleLineEngraving(
    Session* session,
    Part* workPart,
    Face* face,
    Body* targetBody,
    const CurvedFaceInfo& info,
    const Point3d& textOrigin,
    const Matrix3x3& textMatrix,
    const std::vector<CurvedStrokeSegment>& segments,
    const std::vector<CurvedStrokeArc>& arcs,
    const CurvedEngravingParams& params,
    bool commitGeometry,
    const std::function<void(const std::string&)>& logFn,
    CurvedEngravingOutput* out)
{
    if (workPart == nullptr || face == nullptr || targetBody == nullptr || out == nullptr)
    {
        throw std::runtime_error("Curved single-line engraving requires valid work part, face, and body.");
    }

    double minX = std::numeric_limits<double>::max();
    double maxX = -std::numeric_limits<double>::max();
    double minY = std::numeric_limits<double>::max();
    double maxY = -std::numeric_limits<double>::max();
    auto includePt = [&](const Point3d& p) {
        minX = std::min(minX, p.X); maxX = std::max(maxX, p.X);
        minY = std::min(minY, p.Y); maxY = std::max(maxY, p.Y);
    };
    for (const auto& s : segments) { includePt(s.start); includePt(s.end); }
    for (const auto& a : arcs)
    {
        includePt(Point3d(a.center.X - a.radius, a.center.Y - a.radius, 0.0));
        includePt(Point3d(a.center.X + a.radius, a.center.Y + a.radius, 0.0));
    }
    const double centerX = 0.5 * (minX + maxX);
    const double centerY = 0.5 * (minY + maxY);
    const double xScale = params.height * params.widthScale / 100.0;
    const double yScale = params.height;
    const double shearTan = std::tan(params.shear * kPi / 180.0);
    const Vector3d xAxis = VecNorm(Vector3d(textMatrix.Xx, textMatrix.Xy, textMatrix.Xz), info.uTangent);
    const Vector3d yAxis = VecNorm(Vector3d(textMatrix.Yx, textMatrix.Yy, textMatrix.Yz), info.vTangent);

    auto toLocalXY = [&](const Point3d& p, double* lx, double* ly) {
        const double y = (p.Y - centerY) * yScale;
        const double x = (p.X - centerX) * xScale + y * shearTan;
        *lx = x;
        *ly = y;
    };

    std::vector<Curve*> surfaceCurves;
    if (info.isCylindrical && info.radius > kEps)
    {
        // Exact analytical cylinder wrap: zero distortion, fast, works for any arc span
        for (const auto& seg : segments)
        {
            double x0, y0, x1, y1;
            toLocalXY(seg.start, &x0, &y0);
            toLocalXY(seg.end, &x1, &y1);
            const double segLen = std::hypot(x1 - x0, y1 - y0);
            if (segLen <= kEps) continue;
            const int samples = std::max(2, std::min(25, static_cast<int>(std::ceil(segLen / std::max(0.5, info.radius * 0.05))) + 1));
            std::vector<Point3d> pts;
            pts.reserve(samples);
            for (int i = 0; i < samples; ++i)
            {
                const double t = static_cast<double>(i) / (samples - 1);
                pts.push_back(MapPointOntoCylinder(info, textOrigin, xAxis, yAxis, x0 + (x1 - x0) * t, y0 + (y1 - y0) * t));
            }
            tag_t spTag = CreateSplineThruPoints(pts);
            if (spTag != NULL_TAG)
            {
                if (Curve* c = dynamic_cast<Curve*>(NXObjectManager::Get(spTag)))
                {
                    surfaceCurves.push_back(c);
                }
            }
        }
        for (const auto& arc : arcs)
        {
            double sweep = arc.endAngle - arc.startAngle;
            if (sweep <= 0.0) sweep += 2.0 * kPi;
            const int samples = std::max(5, std::min(33, static_cast<int>(std::ceil(sweep / (10.0 * kPi / 180.0))) + 1));
            std::vector<Point3d> pts;
            pts.reserve(samples);
            for (int i = 0; i < samples; ++i)
            {
                const double ang = arc.startAngle + sweep * static_cast<double>(i) / (samples - 1);
                Point3d lp(arc.center.X + arc.radius * std::cos(ang),
                           arc.center.Y + arc.radius * std::sin(ang), 0.0);
                double lx, ly;
                toLocalXY(lp, &lx, &ly);
                pts.push_back(MapPointOntoCylinder(info, textOrigin, xAxis, yAxis, lx, ly));
            }
            tag_t spTag = CreateSplineThruPoints(pts);
            if (spTag != NULL_TAG)
            {
                if (Curve* c = dynamic_cast<Curve*>(NXObjectManager::Get(spTag)))
                {
                    surfaceCurves.push_back(c);
                }
            }
        }
    }
    else
    {
        // General surface: create flat curves in tangent plane then equal-arc-length project onto face
        std::vector<Curve*> flatCurves;
        auto toFlatWorld = [&](const Point3d& p) {
            double lx, ly;
            toLocalXY(p, &lx, &ly);
            return PtAdd(PtAdd(textOrigin, xAxis, lx), yAxis, ly);
        };
        for (const auto& seg : segments)
        {
            Point3d p0 = toFlatWorld(seg.start);
            Point3d p1 = toFlatWorld(seg.end);
            if (VecMag(PtSub(p1, p0)) > kEps)
            {
                if (Line* l = workPart->Curves()->CreateLine(p0, p1)) flatCurves.push_back(l);
            }
        }
        for (const auto& arc : arcs)
        {
            Point3d c = toFlatWorld(arc.center);
            if (NXOpen::Arc* a = workPart->Curves()->CreateArc(c, xAxis, yAxis, arc.radius * xScale, arc.startAngle, arc.endAngle))
            {
                flatCurves.push_back(a);
            }
        }
        tag_t projFeatTag = NULL_TAG;
        surfaceCurves = ProjectCurvesEqualArcLength(
            workPart, face, flatCurves, textOrigin, info.normal, xAxis, false, &projFeatTag);
        for (Curve* fc : flatCurves)
        {
            if (fc != nullptr)
            {
                try { UF_OBJ_delete_object(fc->Tag()); } catch (...) {}
            }
        }
        if (projFeatTag != NULL_TAG && commitGeometry)
        {
            out->constructionFeatureTags.push_back(projFeatTag);
        }
    }

    if (surfaceCurves.empty())
    {
        throw std::runtime_error("Could not generate wrapped single-line curves on the curved surface.");
    }

    if (!commitGeometry || params.depth <= 1.0e-6)
    {
        std::vector<DisplayableObject*> dispObjs;
        for (Curve* c : surfaceCurves)
        {
            if (c != nullptr)
            {
                dispObjs.push_back(c);
                if (!commitGeometry) out->previewObjectTags.push_back(c->Tag());
                else out->resultCurveTags.push_back(c->Tag());
            }
        }
        ApplyLayerAndColorToObjects(workPart, dispObjs, commitGeometry ? params.layer : 0, params.colorId);
        if (logFn)
        {
            logFn(std::string(commitGeometry ? "曲面单线刻字(仅曲线): " : "曲面单线刻字预览: ") +
                  "贴面曲线数=" + std::to_string(surfaceCurves.size()));
        }
        return;
    }

    // Depth > 0: Sweep a Tube along each surface curve and Subtract (engrave) or Unite (emboss)
    const double tubeDiameter = std::max(params.vShapeWidth, params.depth * 2.0);
    std::vector<Body*> toolBodies;
    std::set<tag_t> facesBeforeBool;
    for (Face* bf : targetBody->GetFaces())
    {
        if (bf != nullptr) facesBeforeBool.insert(bf->Tag());
    }

    for (Curve* crv : surfaceCurves)
    {
        if (crv == nullptr) continue;
        Features::TubeBuilder* tb = workPart->Features()->CreateTubeBuilder(nullptr);
        if (tb == nullptr) continue;
        try
        {
            tb->PathSection()->SetDistanceTolerance(0.01);
            tb->PathSection()->SetChainingTolerance(0.01);
            tb->PathSection()->SetAllowedEntityTypes(Section::AllowTypesOnlyCurves);
            SelectionIntentRuleOptions* opts = workPart->ScRuleFactory()->CreateRuleOptions();
            opts->SetSelectedFromInactive(false);
            std::vector<IBaseCurve*> seeds(1, crv);
            CurveDumbRule* rule = workPart->ScRuleFactory()->CreateRuleBaseCurveDumb(seeds, opts);
            delete opts;
            std::vector<SelectionIntentRule*> rules(1, rule);
            tb->PathSection()->AddToSection(rules, crv, nullptr, nullptr, textOrigin, Section::ModeCreate, false);

            tb->OuterDiameter()->SetFormula(FmtNum(tubeDiameter).c_str());
            tb->InnerDiameter()->SetFormula("0");
            tb->SetOutputOption(Features::TubeBuilder::OutputSingleSegment);
            tb->BooleanOption()->SetType(GeometricUtilities::BooleanOperation::BooleanTypeCreate);
            tb->SetTolerance(0.01);

            Features::Feature* tubeFeat = tb->CommitFeature();
            tb->Destroy();
            if (tubeFeat != nullptr)
            {
                out->constructionFeatureTags.push_back(tubeFeat->Tag());
                for (Body* b : tubeFeat->GetBodies())
                {
                    if (b != nullptr) toolBodies.push_back(b);
                }
            }
        }
        catch (...)
        {
            try { tb->Destroy(); } catch (...) {}
        }
    }

    // Blank the construction stroke curves
    for (Curve* crv : surfaceCurves)
    {
        if (crv != nullptr)
        {
            try { UF_OBJ_set_blank_status(crv->Tag(), UF_OBJ_BLANKED); } catch (...) {}
        }
    }

    if (toolBodies.empty())
    {
        throw std::runtime_error("TubeBuilder did not create any tool bodies for curved single-line engraving.");
    }

    // Boolean Subtract (engrave) or Unite (emboss)
    Features::BooleanBuilder* boolBuilder = workPart->Features()->CreateBooleanBuilderUsingCollector(nullptr);
    if (boolBuilder == nullptr)
    {
        throw std::runtime_error("CreateBooleanBuilderUsingCollector returned NULL.");
    }
    try
    {
        boolBuilder->SetOperation(params.embossed ? Features::Feature::BooleanTypeUnite
                                                  : Features::Feature::BooleanTypeSubtract);
        boolBuilder->SetTolerance(0.01);
        SelectionIntentRuleOptions* targetOpts = workPart->ScRuleFactory()->CreateRuleOptions();
        targetOpts->SetSelectedFromInactive(false);
        std::vector<Body*> targetBodies(1, targetBody);
        BodyDumbRule* targetRule = workPart->ScRuleFactory()->CreateRuleBodyDumb(targetBodies, true, targetOpts);
        delete targetOpts;
        std::vector<SelectionIntentRule*> targetRules(1, targetRule);
        ScCollector* targetCollector = workPart->ScCollectors()->CreateCollector();
        targetCollector->ReplaceRules(targetRules, false);
        boolBuilder->SetTargetBodyCollector(targetCollector);

        SelectionIntentRuleOptions* toolOpts = workPart->ScRuleFactory()->CreateRuleOptions();
        toolOpts->SetSelectedFromInactive(false);
        BodyDumbRule* toolRule = workPart->ScRuleFactory()->CreateRuleBodyDumb(toolBodies, true, toolOpts);
        delete toolOpts;
        std::vector<SelectionIntentRule*> toolRules(1, toolRule);
        ScCollector* toolCollector = workPart->ScCollectors()->CreateCollector();
        toolCollector->ReplaceRules(toolRules, false);
        boolBuilder->SetToolBodyCollector(toolCollector);

        Features::Feature* boolFeat = boolBuilder->CommitFeature();
        boolBuilder->Destroy();
        if (boolFeat != nullptr)
        {
            out->constructionFeatureTags.push_back(boolFeat->Tag());
        }
    }
    catch (...)
    {
        try { boolBuilder->Destroy(); } catch (...) {}
        throw;
    }

    if (params.colorId > 0)
    {
        std::vector<DisplayableObject*> coloredFaces;
        for (Face* bf : targetBody->GetFaces())
        {
            if (bf != nullptr && facesBeforeBool.count(bf->Tag()) == 0)
            {
                coloredFaces.push_back(bf);
            }
        }
        ApplyLayerAndColorToObjects(workPart, coloredFaces, 0, params.colorId);
    }
    if (logFn) logFn("曲面单线刻字完成: 刀具体数=" + std::to_string(toolBodies.size()));
}

struct RoundTubeAutoTarget
{
    Face* face = nullptr;
    Point3d origin = Point3d(0.0, 0.0, 0.0);
    Matrix3x3 matrix = {};
    double screenX = 0.0;
    double screenY = 0.0;
};

inline bool BuildAutomaticRoundTubeTarget(Body* body,
                                          const Vector3d& viewX,
                                          const Vector3d& viewY,
                                          const Vector3d& viewZ,
                                          double textHeight,
                                          RoundTubeAutoTarget* outTarget)
{
    if (body == nullptr || outTarget == nullptr || body->IsBlanked() || !body->IsSolidBody())
    {
        return false;
    }

    Face* bestFace = nullptr;
    CurvedFaceInfo bestInfo;
    double bestScore = -1.0;

    for (Face* face : body->GetFaces())
    {
        if (face == nullptr) continue;
        CurvedFaceInfo info;
        if (!AskFaceInfo(face, nullptr, &info)) continue;
        if (!info.isCylindrical || !info.isOuterCylinder) continue;
        if (info.radius < std::max(1.0, textHeight * 0.35)) continue;
        if (info.axisSpan < std::max(info.radius * 1.5, textHeight * 2.0)) continue;

        const double score = info.radius * info.axisSpan;
        if (score > bestScore)
        {
            bestScore = score;
            bestFace = face;
            bestInfo = info;
        }
    }

    if (bestFace == nullptr)
    {
        return false;
    }

    // Place engraving at axial midpoint, facing the current camera viewZ perpendicular to tube axis
    const double midAxial = 0.5 * (bestInfo.axisMin + bestInfo.axisMax);
    const Point3d midAxisPt = PtAdd(bestInfo.axisOrigin, bestInfo.axisDir, midAxial);
    Vector3d radialDir = ProjectTangent(viewZ, bestInfo.axisDir);
    if (VecMag(radialDir) <= kEps)
    {
        radialDir = ProjectTangent(viewY, bestInfo.axisDir);
    }
    if (VecMag(radialDir) <= kEps)
    {
        radialDir = ProjectTangent(Vector3d(0.0, 0.0, 1.0), bestInfo.axisDir);
    }
    radialDir = VecNorm(radialDir, Vector3d(1.0, 0.0, 0.0));

    const Point3d candidatePt = PtAdd(midAxisPt, radialDir, bestInfo.radius);
    CurvedFaceInfo snappedInfo;
    if (!AskFaceInfo(bestFace, &candidatePt, &snappedInfo))
    {
        return false;
    }

    Vector3d xAxis = bestInfo.axisDir;
    if (VecDot(xAxis, viewX) < 0.0)
    {
        xAxis = Vector3d(-xAxis.X, -xAxis.Y, -xAxis.Z);
    }
    const Vector3d zAxis = VecNorm(snappedInfo.normal, radialDir);
    xAxis = VecNorm(ProjectTangent(xAxis, zAxis), xAxis);
    const Vector3d yAxis = VecNorm(VecCross(zAxis, xAxis), Vector3d(0.0, 1.0, 0.0));

    Matrix3x3 m = {};
    m.Xx = xAxis.X; m.Xy = xAxis.Y; m.Xz = xAxis.Z;
    m.Yx = yAxis.X; m.Yy = yAxis.Y; m.Yz = yAxis.Z;
    m.Zx = zAxis.X; m.Zy = zAxis.Y; m.Zz = zAxis.Z;

    outTarget->face = bestFace;
    outTarget->origin = snappedInfo.point;
    outTarget->matrix = m;
    outTarget->screenX = snappedInfo.point.X * viewX.X + snappedInfo.point.Y * viewX.Y + snappedInfo.point.Z * viewX.Z;
    outTarget->screenY = snappedInfo.point.X * viewY.X + snappedInfo.point.Y * viewY.Y + snappedInfo.point.Z * viewY.Z;
    return true;
}

inline bool FindMatchingCylindricalFaceForSameBody(Body* candidateBody,
                                                   const CurvedFaceInfo& refInfo,
                                                   const Point3d& refOrigin,
                                                   const Matrix3x3& refMatrix,
                                                   const Vector3d& viewX,
                                                   const Vector3d& viewZ,
                                                   Face** outFace,
                                                   Point3d* outOrigin,
                                                   Matrix3x3* outMatrix)
{
    if (candidateBody == nullptr || !refInfo.isCylindrical ||
        outFace == nullptr || outOrigin == nullptr || outMatrix == nullptr)
    {
        return false;
    }

    const double refAxialRatio = refInfo.axisSpan > kEps
                                     ? (VecDot(PtSub(refOrigin, refInfo.axisOrigin), refInfo.axisDir) - refInfo.axisMin) / refInfo.axisSpan
                                     : 0.5;
    const Vector3d refX(refMatrix.Xx, refMatrix.Xy, refMatrix.Xz);
    const bool isAxialText = std::fabs(VecDot(refX, refInfo.axisDir)) >= 0.707;

    Face* matchedFace = nullptr;
    CurvedFaceInfo matchedInfo;
    double bestDiff = std::numeric_limits<double>::max();
    for (Face* f : candidateBody->GetFaces())
    {
        if (f == nullptr) continue;
        CurvedFaceInfo info;
        if (!AskFaceInfo(f, nullptr, &info)) continue;
        if (!info.isCylindrical || info.isOuterCylinder != refInfo.isOuterCylinder) continue;
        const double rDiff = std::fabs(info.radius - refInfo.radius);
        const double lDiff = std::fabs(info.axisSpan - refInfo.axisSpan);
        if (rDiff > 0.1 || lDiff > 0.5) continue;
        const double diff = rDiff + lDiff * 0.1;
        if (diff < bestDiff)
        {
            bestDiff = diff;
            matchedFace = f;
            matchedInfo = info;
        }
    }
    if (matchedFace == nullptr)
    {
        return false;
    }

    const double targetAxial = matchedInfo.axisMin + matchedInfo.axisSpan * std::clamp(refAxialRatio, 0.05, 0.95);
    const Point3d axisPt = PtAdd(matchedInfo.axisOrigin, matchedInfo.axisDir, targetAxial);
    Vector3d radialDir = ProjectTangent(viewZ, matchedInfo.axisDir);
    if (VecMag(radialDir) <= kEps)
    {
        radialDir = ProjectTangent(refInfo.normal, matchedInfo.axisDir);
    }
    radialDir = VecNorm(radialDir, Vector3d(1.0, 0.0, 0.0));
    const Point3d candPt = PtAdd(axisPt, radialDir, matchedInfo.radius);
    CurvedFaceInfo snapped;
    if (!AskFaceInfo(matchedFace, &candPt, &snapped))
    {
        return false;
    }

    const Vector3d zAxis = VecNorm(snapped.normal, radialDir);
    Vector3d xAxis = isAxialText
                         ? VecNorm(ProjectTangent(matchedInfo.axisDir, zAxis), Vector3d(1.0, 0.0, 0.0))
                         : VecNorm(VecCross(zAxis, matchedInfo.axisDir), Vector3d(1.0, 0.0, 0.0));
    if (VecDot(xAxis, viewX) < 0.0)
    {
        xAxis = Vector3d(-xAxis.X, -xAxis.Y, -xAxis.Z);
    }
    const Vector3d yAxis = VecNorm(VecCross(zAxis, xAxis), Vector3d(0.0, 1.0, 0.0));

    Matrix3x3 m = {};
    m.Xx = xAxis.X; m.Xy = xAxis.Y; m.Xz = xAxis.Z;
    m.Yx = yAxis.X; m.Yy = yAxis.Y; m.Yz = yAxis.Z;
    m.Zx = zAxis.X; m.Zy = zAxis.Y; m.Zz = zAxis.Z;

    *outFace = matchedFace;
    *outOrigin = snapped.point;
    *outMatrix = m;
    return true;
}

} // namespace zhihui_kezi_curved
