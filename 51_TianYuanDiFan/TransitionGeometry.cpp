#include "TransitionGeometry.hpp"

#include <NXOpen/Body.hxx>
#include <NXOpen/BodyCollection.hxx>
#include <NXOpen/BodyDumbRule.hxx>
#include <NXOpen/Curve.hxx>
#include <NXOpen/CurveDumbRule.hxx>
#include <NXOpen/Edge.hxx>
#include <NXOpen/EdgeDumbRule.hxx>
#include <NXOpen/FaceDumbRule.hxx>
#include <NXOpen/Features_SheetMetal_BendOptions.hxx>
#include <NXOpen/Features_SheetMetal_SheetMetalFromSolidBuilder.hxx>
#include <NXOpen/Expression.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/Features_BaseFeatureCollection.hxx>
#include <NXOpen/Features_BoundedPlaneBuilder.hxx>
#include <NXOpen/Features_BooleanBuilder.hxx>
#include <NXOpen/Features_FeatureCollection.hxx>
#include <NXOpen/Features_GeomcopyBuilder.hxx>
#include <NXOpen/Features_OffsetFaceBuilder.hxx>
#include <NXOpen/Features_Feature.hxx>
#include <NXOpen/Features_MoveObjectBuilder.hxx>
#include <NXOpen/Features_SewBuilder.hxx>
#include <NXOpen/Features_SplitBodyBuilder.hxx>
#include <NXOpen/GeometricUtilities_BooleanToolBuilder.hxx>
#include <NXOpen/GeometricUtilities_FacePlaneToolBuilder.hxx>
#include <NXOpen/Features_SheetMetal_ConvertToSheetmetalBuilder.hxx>
#include <NXOpen/Features_SheetMetal_FlatSolidBuilder.hxx>
#include <NXOpen/Features_SheetMetal_FlatPatternBuilder.hxx>
#include <NXOpen/Features_SheetMetal_LoftedFlangeBuilder.hxx>
#include <NXOpen/Features_SketchFeature.hxx>
#include <NXOpen/Direction.hxx>
#include <NXOpen/DirectionCollection.hxx>
#include <NXOpen/Point.hxx>
#include <NXOpen/PointCollection.hxx>
#include <NXOpen/Sketch.hxx>
#include <NXOpen/SketchCollection.hxx>
#include <NXOpen/SketchInPlaceBuilder.hxx>
#include <NXOpen/Plane.hxx>
#include <NXOpen/PlaneCollection.hxx>
#include <NXOpen/PlaneTypes.hxx>
#include <NXOpen/ScCollectorCollection.hxx>
#include <NXOpen/Features_SheetMetal_SheetmetalManager.hxx>
#include <NXOpen/Features_ThickenBuilder.hxx>
#include <NXOpen/GeometricUtilities_BooleanOperation.hxx>
#include <NXOpen/GeometricUtilities_ModlMotion.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/ScCollector.hxx>
#include <NXOpen/ScRuleFactory.hxx>
#include <NXOpen/SelectEdge.hxx>
#include <NXOpen/SelectObjectList.hxx>
#include <NXOpen/SheetmetalFeatureProperty.hxx>
#include <NXOpen/Section.hxx>
#include <NXOpen/SectionCollection.hxx>
#include <NXOpen/SelectFace.hxx>
#include <NXOpen/SelectNXObjectList.hxx>
#include <NXOpen/Session.hxx>
#include <uf.h>
#include <uf_curve.h>
#include <uf_csys.h>
#include <uf_modl.h>
#include <uf_obj.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <locale>
#include <map>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace tianyuan {
namespace {
using namespace NXOpen;

constexpr double kPi = 3.14159265358979323846;

void Check(int code)
{
    if (code == 0) return;
    char message[1024] = {};
    UF_get_fail_message(code, message);
    throw std::runtime_error(std::string(message) + " (" + std::to_string(code) + ")");
}

std::string Number(double value)
{
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(16) << value;
    return out.str();
}

void ThrowIfErrors(Features::Feature* feature, const char* prefix)
{
    const auto errors = feature->GetFeatureErrorMessages();
    if (!errors.empty())
        throw std::runtime_error(std::string(prefix) + errors.front().GetUTF8Text());
}

void SoftBlank(tag_t object)
{
    if (object != NULL_TAG) UF_OBJ_set_blank_status(object, UF_OBJ_BLANKED);
}

struct Vec {
    double x = 0;
    double y = 0;
    double z = 0;
};

tag_t Line(Vec start, Vec end)
{
    UF_CURVE_line_t line = {{start.x, start.y, start.z}, {end.x, end.y, end.z}};
    tag_t curve = NULL_TAG;
    Check(UF_CURVE_create_line(&line, &curve));
    return curve;
}

tag_t OpenArc(Vec center, double radius, double startAngle, double endAngle)
{
    double matrixValues[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    tag_t matrix = NULL_TAG;
    Check(UF_CSYS_create_matrix(matrixValues, &matrix));
    UF_CURVE_arc_t arc = {};
    arc.matrix_tag = matrix;
    arc.start_angle = startAngle;
    arc.end_angle = endAngle;
    arc.arc_center[0] = center.x;
    arc.arc_center[1] = center.y;
    arc.arc_center[2] = center.z;
    arc.radius = radius;
    tag_t curve = NULL_TAG;
    Check(UF_CURVE_create_arc(&arc, &curve));
    SoftBlank(matrix);
    return curve;
}

Features::SketchFeature* SlaveSketch(Part* part, const std::vector<tag_t>& curves, double z, const char* name)
{
    SketchInPlaceBuilder* builder = nullptr;
    Sketch* sketch = nullptr;
    try {
        builder = part->Sketches()->CreateSketchInPlaceBuilder2(nullptr);
        Matrix3x3 matrix = {};
        matrix.Xx = 1;
        matrix.Yy = 1;
        matrix.Zz = 1;
        const Point3d origin(0, 0, z);
        auto* plane = part->Planes()->CreateFixedPlane(origin, matrix);
        builder->SetPlaneOption(Sketch::PlaneOptionExistingPlane);
        builder->SetPlaneReference(plane);
        builder->SetOriginOption(OriginMethodSpecifyPoint);
        builder->SetSketchOrigin(part->Points()->CreatePoint(origin));
        builder->SetAxisReference(part->Directions()->CreateDirection(origin, Vector3d(1, 0, 0), SmartObject::UpdateOptionWithinModeling));
        builder->SetAxisOrientation(AxisOrientationHorizontal);
        NXObject* committed = nullptr;
        try {
            committed = builder->Commit();
        } catch (const NXException& exception) {
            throw std::runtime_error(std::string("天圆地方草图失败：") + exception.Message());
        }
        builder->Destroy();
        builder = nullptr;
        sketch = dynamic_cast<Sketch*>(committed);
        if (sketch == nullptr) throw std::runtime_error("天圆地方草图没有生成。");
        sketch->Activate(Sketch::ViewReorientFalse);
        for (auto tag : curves) {
            auto* curve = dynamic_cast<Curve*>(NXObjectManager::Get(tag));
            if (curve == nullptr) throw std::runtime_error("天圆地方截面曲线无效。");
            sketch->AddGeometry(curve, Sketch::InferConstraintsOptionInferNoConstraints);
            if (part->Sketches()->GetOwningSketch(curve) == nullptr)
                throw std::runtime_error("天圆地方曲线没有进入草图。");
        }
        sketch->Deactivate(Sketch::ViewReorientFalse, Sketch::UpdateLevelSketchOnly);
        auto* feature = dynamic_cast<Features::SketchFeature*>(sketch->Feature());
        if (feature == nullptr) throw std::runtime_error("天圆地方草图没有特征。");
        feature->SetName(NXString(name, NXString::UTF8));
        return feature;
    } catch (...) {
        if (sketch != nullptr && sketch->IsActive()) {
            try { sketch->Deactivate(Sketch::ViewReorientFalse, Sketch::UpdateLevelSketchOnly); } catch (...) {}
        }
        if (builder != nullptr) builder->Destroy();
        throw;
    }
}

void FillChain(Section* section, const std::vector<tag_t>& curves, Vec help, double units, bool chain = true)
{
    auto* part = Session::GetSession()->Parts()->Work();
    section->SetAllowedEntityTypes(Section::AllowTypesOnlyCurves);
    section->AllowSelfIntersection(false);
    section->SetChainingTolerance(1e-5 * units);
    section->SetDistanceTolerance(1e-4 * units);
    std::vector<Curve*> nxCurves;
    for (auto tag : curves) {
        auto* curve = dynamic_cast<Curve*>(NXObjectManager::Get(tag));
        if (curve == nullptr) throw std::runtime_error("天圆地方截面曲线无效。");
        nxCurves.push_back(curve);
    }
    auto* rule = part->ScRuleFactory()->CreateRuleCurveDumb(nxCurves);
    section->AddToSection(std::vector<SelectionIntentRule*>{rule}, nxCurves.front(), nullptr, nullptr,
                          {help.x, help.y, help.z}, Section::ModeCreate, chain);
}

Face* LargestFace(Body* body);

Features::Feature* LoftChain(Part* part, const std::vector<tag_t>& startCurves, const std::vector<tag_t>& endCurves,
    Vec startPoint, Vec endPoint, Vec startHelp, Vec endHelp, double units, double thickness,
    bool segmented, int segments, double indexMark)
{
    Features::SheetMetal::LoftedFlangeBuilder* loft = nullptr;
    try {
        loft = part->Features()->SheetmetalManager()->CreateLoftedFlangeFeatureBuilder(nullptr);
        loft->SetApplicationContext(Features::SheetMetal::ApplicationContextNxSheetMetal);
        loft->SetSecondary(false);
        auto* start = part->Sections()->CreateSection(1e-5 * units, 1e-4 * units, 0.5);
        auto* end = part->Sections()->CreateSection(1e-5 * units, 1e-4 * units, 0.5);
        loft->SetStartSection(start);
        loft->SetEndSection(end);
        loft->SetStartSketch(SlaveSketch(part, startCurves, startPoint.z, "天圆地方圆口"));
        loft->SetEndSketch(SlaveSketch(part, endCurves, endPoint.z, "天圆地方方口"));
        FillChain(start, startCurves, startHelp, units, false);
        FillChain(end, endCurves, endHelp, units, false);
        loft->SetStartSectionPoint({startPoint.x, startPoint.y, startPoint.z});
        loft->SetEndSectionPoint({endPoint.x, endPoint.y, endPoint.z});
        loft->GetThickness()->SetFormula(Number(thickness).c_str());
        loft->SetThicknessSide(Features::SheetMetal::LoftedFlangeBuilder::SectionSideOptionsLeft);
        loft->SetBendingMethod(Features::SheetMetal::LoftedFlangeBuilder::BendingMethodsFormed);
        loft->SetUseSegmentedBends(segmented);
        loft->SetNumberOfBendSegments(segmented ? segments : 1);
        if (segmented) {
            loft->SetBendDivideParameter(Features::SheetMetal::LoftedFlangeBuilder::BendDivideParametersNumberOfBendSegments);
            loft->IndexMarkLength()->SetFormula(Number(indexMark).c_str());
        }
        loft->MaximumChordHeight()->SetFormula(Number(6.5 * units).c_str());
        loft->MaximumSegmentLength()->SetFormula(Number(6.5 * units).c_str());
        loft->MaximumSegmentAngle()->SetFormula("20");
        loft->SetAutoReliefType(Features::SheetMetal::LoftedFlangeBuilder::AutoReliefTypesLinear);
        auto* bend = loft->BendOptions();
        bend->SetUseGlobalBendRadius(false);
        bend->BendRadius()->SetFormula(Number(0.2 * units).c_str());
        loft->SetTrimEndPlates(true);
        Features::Feature* feature = nullptr;
        try {
            feature = loft->CommitFeature();
        } catch (const NXException& exception) {
            throw std::runtime_error(std::string("天圆地方放样失败：") + exception.Message());
        }
        loft->Destroy();
        loft = nullptr;
        feature->SetName(NXString("天圆地方", NXString::UTF8));
        ThrowIfErrors(feature, "天圆地方放样失败：");
        return feature;
    } catch (...) {
        if (loft != nullptr) loft->Destroy();
        throw;
    }
}

Body* FlatOf(Part* part, Body* formed, tag_t* featureTag)
{
    Features::SheetMetal::FlatSolidBuilder* flat = nullptr;
    try {
        flat = part->Features()->SheetmetalManager()->CreateFlatSolidFeatureBuilder(nullptr);
        flat->StationaryFace()->SetValue(LargestFace(formed));
        flat->SetAssociative(true);
        flat->SetOrientation(Features::SheetMetal::FlatSolidBuilder::OrientationTypeDefault);
        Features::Feature* feature = nullptr;
        try {
            feature = flat->CommitFeature();
        } catch (const NXException& exception) {
            throw std::runtime_error(std::string("天圆地方展开失败：") + exception.Message());
        }
        flat->Destroy();
        flat = nullptr;
        feature->SetName(NXString("天圆地方展开", NXString::UTF8));
        ThrowIfErrors(feature, "天圆地方展开失败：");
        if (featureTag != nullptr) *featureTag = feature->Tag();
        for (auto* body : feature->GetBodies()) {
            if (body != nullptr && body->Tag() != formed->Tag() && body->IsSolidBody()) return body;
        }
        throw std::runtime_error("天圆地方展开未生成实体。");
    } catch (...) {
        if (flat != nullptr) flat->Destroy();
        throw;
    }
}

Vec operator-(Vec a, Vec b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec operator+(Vec a, Vec b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec operator*(Vec a, double scale) { return {a.x * scale, a.y * scale, a.z * scale}; }
Vec Cross(Vec a, Vec b)
{
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

void OrientOutward(Vec& a, Vec& b, Vec& c)
{
    const Vec normal = Cross(b - a, c - a);
    const Vec mid = (a + b + c) * (1.0 / 3.0);
    if (normal.x * mid.x + normal.y * mid.y < 0) {
        const Vec swap = b;
        b = c;
        c = swap;
    }
}

struct SheetPatch {
    Body* body = nullptr;
    tag_t feature = NULL_TAG;
};

void OrientUp(Vec& a, Vec& b, Vec& c)
{
    const Vec normal = Cross(b - a, c - a);
    if (normal.z < 0) {
        const Vec swap = b;
        b = c;
        c = swap;
    }
}

SheetPatch TriangleSheet(Part* part, Vec a, Vec b, Vec c, double units, bool up)
{
    if (up) OrientUp(a, b, c);
    else OrientOutward(a, b, c);
    const tag_t lines[3] = {Line(a, b), Line(b, c), Line(c, a)};
    std::vector<Curve*> curves;
    for (auto tag : lines) curves.push_back(dynamic_cast<Curve*>(NXObjectManager::Get(tag)));
    auto* builder = part->Features()->CreateBoundedPlaneBuilder(nullptr);
    try {
        auto* section = builder->BoundingCurves();
        section->SetAllowedEntityTypes(Section::AllowTypesOnlyCurves);
        section->AllowSelfIntersection(false);
        section->SetChainingTolerance(1e-5 * units);
        section->SetDistanceTolerance(1e-4 * units);
        auto* rule = part->ScRuleFactory()->CreateRuleCurveDumb(curves);
        section->AddToSection(std::vector<SelectionIntentRule*>{rule}, curves.front(), nullptr, nullptr, {a.x, a.y, a.z}, Section::ModeCreate, true);
        auto* feature = builder->CommitFeature();
        builder->Destroy();
        builder = nullptr;
        for (auto tag : lines) SoftBlank(tag);
        auto bodies = feature->GetBodies();
        if (bodies.size() != 1 || bodies[0] == nullptr)
            throw std::runtime_error("天圆地方三角面没有生成片体。");
        return {bodies[0], feature->Tag()};
    } catch (...) {
        if (builder != nullptr) builder->Destroy();
        throw;
    }
}

Body* SewSheets(Part* part, const std::vector<Body*>& sheets, double units, tag_t& featureTag)
{
    auto* sew = part->Features()->CreateSewBuilder(nullptr);
    try {
        sew->SetTolerance(1e-3 * units);
        std::vector<Body*> tools(sheets.begin() + 1, sheets.end());
        auto* targetRule = part->ScRuleFactory()->CreateRuleBodyDumb({sheets.front()}, true);
        auto* toolRule = part->ScRuleFactory()->CreateRuleBodyDumb(tools, true);
        sew->TargetBodiesCollector()->ReplaceRules(std::vector<SelectionIntentRule*>{targetRule}, false);
        sew->ToolBodiesCollector()->ReplaceRules(std::vector<SelectionIntentRule*>{toolRule}, false);
        auto* feature = sew->CommitFeature();
        sew->Destroy();
        sew = nullptr;
        featureTag = feature->Tag();
        ThrowIfErrors(feature, "天圆地方曲面缝合失败：");
        auto bodies = feature->GetBodies();
        if (bodies.size() != 1 || bodies[0] == nullptr)
            throw std::runtime_error("天圆地方曲面没有缝成一张片体。");
        return bodies[0];
    } catch (...) {
        if (sew != nullptr) sew->Destroy();
        throw;
    }
}

double Distance(Vec a, Vec b)
{
    const Vec delta = a - b;
    return std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
}

struct Vec2 { double x; double y; };

Vec2 PlaceThird(Vec2 a, Vec2 b, double radiusA, double radiusB, Vec2 avoid)
{
    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    const double dist = std::sqrt(dx * dx + dy * dy);
    if (dist < 1e-12) throw std::runtime_error("天圆地方展开边长无效。");
    const double along = (radiusA * radiusA - radiusB * radiusB + dist * dist) / (2.0 * dist);
    double height2 = radiusA * radiusA - along * along;
    if (height2 < 0 && height2 > -1e-8 * dist * dist) height2 = 0;
    if (height2 < 0) throw std::runtime_error("天圆地方三角面不能按真实边长展开。");
    const double height = std::sqrt(height2);
    const Vec2 mid{a.x + dx * along / dist, a.y + dy * along / dist};
    const Vec2 side{-dy / dist, dx / dist};
    const Vec2 first{mid.x + side.x * height, mid.y + side.y * height};
    const Vec2 second{mid.x - side.x * height, mid.y - side.y * height};
    const double firstDistance = (first.x - avoid.x) * (first.x - avoid.x) + (first.y - avoid.y) * (first.y - avoid.y);
    const double secondDistance = (second.x - avoid.x) * (second.x - avoid.x) + (second.y - avoid.y) * (second.y - avoid.y);
    return firstDistance >= secondDistance ? first : second;
}

std::vector<std::array<Vec, 3>> Develop(const std::vector<std::array<Vec, 3>>& triangles, double tolerance)
{
    if (triangles.empty()) throw std::runtime_error("天圆地方没有可展开的三角面。");
    std::vector<std::array<Vec2, 3>> flat(triangles.size());
    flat[0][0] = {0, 0};
    flat[0][1] = {Distance(triangles[0][0], triangles[0][1]), 0};
    flat[0][2] = PlaceThird(flat[0][0], flat[0][1], Distance(triangles[0][0], triangles[0][2]),
                             Distance(triangles[0][1], triangles[0][2]), {0, -1});
    if (flat[0][2].y < 0) flat[0][2].y = -flat[0][2].y;

    for (size_t index = 1; index < triangles.size(); ++index) {
        int previous[2] = {-1, -1};
        int current[2] = {-1, -1};
        int found = 0;
        for (int from = 0; from < 3 && found < 2; ++from) {
            for (int to = 0; to < 3; ++to) {
                if (Distance(triangles[index - 1][from], triangles[index][to]) > tolerance) continue;
                bool used = false;
                for (int slot = 0; slot < found; ++slot) used = used || previous[slot] == from || current[slot] == to;
                if (used) continue;
                previous[found] = from;
                current[found] = to;
                ++found;
                break;
            }
        }
        if (found != 2) throw std::runtime_error("天圆地方展开没有找到相邻边。");
        const int third = 3 - current[0] - current[1];
        const int previousThird = 3 - previous[0] - previous[1];
        flat[index][current[0]] = flat[index - 1][previous[0]];
        flat[index][current[1]] = flat[index - 1][previous[1]];
        flat[index][third] = PlaceThird(
            flat[index][current[0]], flat[index][current[1]],
            Distance(triangles[index][third], triangles[index][current[0]]),
            Distance(triangles[index][third], triangles[index][current[1]]),
            flat[index - 1][previousThird]);
    }

    std::vector<std::array<Vec, 3>> result;
    result.reserve(flat.size());
    for (const auto& triangle : flat)
        result.push_back({Vec{triangle[0].x, triangle[0].y, 0}, Vec{triangle[1].x, triangle[1].y, 0}, Vec{triangle[2].x, triangle[2].y, 0}});
    return result;
}

Face* LargestFace(Body* body)
{
    Face* best = nullptr;
    double bestScore = -1;
    for (auto* face : body->GetFaces()) {
        double box[6] = {};
        if (UF_MODL_ask_bounding_box(face->Tag(), box) != 0) continue;
        const double dx = box[3] - box[0];
        const double dy = box[4] - box[1];
        const double dz = box[5] - box[2];
        const double score = dx * dy + dy * dz + dz * dx;
        if (score > bestScore) {
            bestScore = score;
            best = face;
        }
    }
    if (best == nullptr) throw std::runtime_error("天圆地方钣金体没有可用面，无法展开。");
    return best;
}

void OffsetFlatBody(Part* part, tag_t formed, tag_t flatBody, double margin)
{
    double formedBox[6] = {};
    double flatBox[6] = {};
    Check(UF_MODL_ask_bounding_box(formed, formedBox));
    Check(UF_MODL_ask_bounding_box(flatBody, flatBox));
    const bool separated =
        formedBox[3] < flatBox[0] || flatBox[3] < formedBox[0] ||
        formedBox[4] < flatBox[1] || flatBox[4] < formedBox[1] ||
        formedBox[5] < flatBox[2] || flatBox[5] < formedBox[2];
    if (separated) return;

    const double dx = (formedBox[3] - flatBox[0]) + margin;
    auto* move = part->BaseFeatures()->CreateMoveObjectBuilder(nullptr);
    try {
        auto* body = dynamic_cast<NXObject*>(NXObjectManager::Get(flatBody));
        if (body == nullptr) throw std::runtime_error("找不到天圆地方展开实体。");
        move->ObjectToMoveObject()->Add(body);
        move->SetMoveObjectResult(Features::MoveObjectBuilder::MoveObjectResultOptionsMoveOriginal);
        move->SetAssociative(true);
        move->SetMoveParents(false);
        auto* motion = move->TransformMotion();
        motion->SetOption(GeometricUtilities::ModlMotion::OptionsDeltaXyz);
        motion->DeltaXc()->SetFormula(Number(dx).c_str());
        motion->DeltaYc()->SetFormula("0");
        motion->DeltaZc()->SetFormula("0");
        NXObject* committed = nullptr;
        try {
            committed = move->Commit();
        } catch (const NXException& exception) {
            throw std::runtime_error(std::string("展开实体移位失败：") + exception.Message());
        }
        move->Destroy();
        move = nullptr;
        if (auto* feature = dynamic_cast<Features::Feature*>(committed)) {
            feature->SetName(NXString("天圆地方展开位置", NXString::UTF8));
            ThrowIfErrors(feature, "展开实体移位失败：");
        }
    } catch (...) {
        if (move != nullptr) move->Destroy();
        throw;
    }
}

void MoveByX(Part* part, tag_t bodyTag, double dx)
{
    if (std::abs(dx) < 1e-6) return;
    auto* move = part->BaseFeatures()->CreateMoveObjectBuilder(nullptr);
    try {
        auto* body = dynamic_cast<NXObject*>(NXObjectManager::Get(bodyTag));
        if (body == nullptr) throw std::runtime_error("找不到天圆地方展开实体。");
        move->ObjectToMoveObject()->Add(body);
        move->SetMoveObjectResult(Features::MoveObjectBuilder::MoveObjectResultOptionsMoveOriginal);
        move->SetAssociative(true);
        move->SetMoveParents(false);
        auto* motion = move->TransformMotion();
        motion->SetOption(GeometricUtilities::ModlMotion::OptionsDeltaXyz);
        motion->DeltaXc()->SetFormula(Number(dx).c_str());
        motion->DeltaYc()->SetFormula("0");
        motion->DeltaZc()->SetFormula("0");
        try {
            NXObject* committed = move->Commit();
            move->Destroy();
            move = nullptr;
            if (auto* feature = dynamic_cast<Features::Feature*>(committed)) {
                feature->SetName(NXString("天圆地方展开位置", NXString::UTF8));
                ThrowIfErrors(feature, "展开实体移位失败：");
            }
        } catch (const NXException& exception) {
            throw std::runtime_error(std::string("展开实体移位失败：") + exception.Message());
        }
    } catch (...) {
        if (move != nullptr) move->Destroy();
        throw;
    }
}

double UnitsPerMm(Part* part)
{
    return part->PartUnits() == BasePart::UnitsInches ? 1.0 / 25.4 : 1.0;
}

std::vector<Face*> CapFaces(Body* body, double signX, double yLimit)
{
    std::vector<Face*> faces;
    for (auto* face : body->GetFaces()) {
        int type = 0;
        int sign = 0;
        double point[3] = {};
        double direction[3] = {};
        double box[6] = {};
        double radius = 0;
        double minor = 0;
        if (UF_MODL_ask_face_data(face->Tag(), &type, point, direction, box, &radius, &minor, &sign) != 0) continue;
        if (type != 22) continue;
        const double length = std::sqrt(direction[0] * direction[0] + direction[1] * direction[1] + direction[2] * direction[2]);
        if (length < 1e-9 || std::abs(direction[1] / length) < 0.8) continue;
        if (std::abs(point[1]) > yLimit) continue;
        if (signX > 0 && point[0] <= 0) continue;
        if (signX < 0 && point[0] >= 0) continue;
        faces.push_back(face);
    }
    return faces;
}

Body* MirrorAcrossFixed(Part* part, Body* body, bool acrossX)
{
    Features::GeomcopyBuilder* builder = nullptr;
    try {
        builder = part->Features()->CreateGeomcopyBuilder(nullptr);
        const Point3d origin(0, 0, 0);
        auto* plane = part->Planes()->CreatePlane(origin, Vector3d(0, 0, 1), SmartObject::UpdateOptionWithinModeling);
        builder->SetMirrorPlane(plane);
        builder->SetType(Features::GeomcopyBuilder::TransformTypesMirror);
        builder->NumberOfCopies()->SetFormula("1");
        builder->SetCsysMirrorOption(acrossX
            ? Features::GeomcopyBuilder::CsysMirrorOptionsMirrorZAndX
            : Features::GeomcopyBuilder::CsysMirrorOptionsMirrorYAndZ);
        builder->SetAssociative(true);
        builder->SetHideOriginal(false);
        plane->SetMethod(acrossX ? PlaneTypes::MethodTypeFixedX : PlaneTypes::MethodTypeFixedY);
        plane->SetOrigin(origin);
        Matrix3x3 matrix = {};
        if (acrossX) {
            matrix.Xx = 0;
            matrix.Xy = 0;
            matrix.Xz = 1;
            matrix.Yx = 0;
            matrix.Yy = 1;
            matrix.Yz = 0;
            matrix.Zx = 1;
            matrix.Zy = 0;
            matrix.Zz = 0;
        } else {
            matrix.Xx = 0;
            matrix.Xy = 0;
            matrix.Xz = 1;
            matrix.Yx = 1;
            matrix.Yy = 0;
            matrix.Yz = 0;
            matrix.Zx = 0;
            matrix.Zy = 1;
            matrix.Zz = 0;
        }
        plane->SetMatrix(matrix);
        plane->SetAlternate(PlaneTypes::AlternateTypeOne);
        plane->Evaluate();
        builder->GeometryToInstance()->Add(body);
        Features::Feature* feature = nullptr;
        try {
            feature = builder->CommitFeature();
        } catch (const NXException& exception) {
            throw std::runtime_error(std::string("天圆地方镜像失败：") + exception.Message());
        }
        builder->Destroy();
        builder = nullptr;
        feature->SetName(NXString("天圆地方镜像", NXString::UTF8));
        ThrowIfErrors(feature, "天圆地方镜像失败：");
        Body* copy = nullptr;
        for (auto* candidate : feature->GetBodies()) {
            if (candidate != nullptr && candidate->Tag() != body->Tag()) copy = candidate;
        }
        if (copy == nullptr) throw std::runtime_error("天圆地方镜像没有生成实体。");
        return copy;
    } catch (...) {
        if (builder != nullptr) builder->Destroy();
        throw;
    }
}

void OffsetJoinFaces(Part* part, const std::vector<Body*>& bodies, double distance, double yLimit)
{
    std::vector<Face*> faces;
    for (auto* body : bodies) {
        auto caps = CapFaces(body, 1, yLimit);
        faces.insert(faces.end(), caps.begin(), caps.end());
    }
    if (faces.empty()) throw std::runtime_error("天圆地方没有找到镜像接缝端面。");
    Features::OffsetFaceBuilder* offset = nullptr;
    try {
        offset = part->Features()->CreateOffsetFaceBuilder(nullptr);
        offset->Distance()->SetFormula(Number(distance).c_str());
        auto* rule = part->ScRuleFactory()->CreateRuleFaceDumb(faces);
        offset->FaceCollector()->ReplaceRules(std::vector<SelectionIntentRule*>{rule}, false);
        Features::Feature* feature = nullptr;
        try {
            feature = offset->CommitFeature();
        } catch (const NXException& exception) {
            throw std::runtime_error(std::string("天圆地方偏置面失败：") + exception.Message());
        }
        offset->Destroy();
        offset = nullptr;
        feature->SetName(NXString("天圆地方偏置", NXString::UTF8));
        ThrowIfErrors(feature, "天圆地方偏置面失败：");
    } catch (...) {
        if (offset != nullptr) offset->Destroy();
        throw;
    }
}

Body* UniteBodies(Part* part, Body* target, Body* tool)
{
    Features::BooleanBuilder* boolean = nullptr;
    try {
        boolean = part->Features()->CreateBooleanBuilderUsingCollector(nullptr);
        boolean->SetOperation(Features::Feature::BooleanTypeUnite);
        boolean->SetRetainTarget(false);
        boolean->SetRetainTool(false);
        boolean->SetTolerance(0.0001);
        boolean->SetConvertToSew(false);
        auto* targetCollector = part->ScCollectors()->CreateCollector();
        auto* targetRule = part->ScRuleFactory()->CreateRuleBodyDumb(std::vector<Body*>{target}, true);
        targetCollector->ReplaceRules(std::vector<SelectionIntentRule*>{targetRule}, false);
        boolean->SetTargetBodyCollector(targetCollector);
        auto* toolCollector = part->ScCollectors()->CreateCollector();
        auto* toolRule = part->ScRuleFactory()->CreateRuleBodyDumb(std::vector<Body*>{tool}, true);
        toolCollector->ReplaceRules(std::vector<SelectionIntentRule*>{toolRule}, false);
        boolean->SetToolBodyCollector(toolCollector);
        Features::Feature* feature = nullptr;
        try {
            feature = boolean->CommitFeature();
        } catch (const NXException& exception) {
            throw std::runtime_error(std::string("天圆地方合并失败：") + exception.Message());
        }
        boolean->Destroy();
        boolean = nullptr;
        feature->SetName(NXString("天圆地方合并", NXString::UTF8));
        ThrowIfErrors(feature, "天圆地方合并失败：");
        for (auto* body : feature->GetBodies()) {
            if (body != nullptr && body->IsSolidBody()) return body;
        }
        if (target != nullptr && UF_OBJ_ask_status(target->Tag()) == UF_OBJ_ALIVE) return target;
        throw std::runtime_error("天圆地方合并没有生成实体。");
    } catch (...) {
        if (boolean != nullptr) boolean->Destroy();
        throw;
    }
}

std::vector<Body*> SplitByPlane(Part* part, const std::vector<Body*>& targets, double normalX, double normalY)
{
    const double length = std::hypot(normalX, normalY);
    if (length < 1e-9) throw std::runtime_error("天圆地方分割平面无效。");
    Features::SplitBodyBuilder* split = nullptr;
    try {
        split = part->Features()->CreateSplitBodyBuilderUsingCollector(nullptr);
        auto* collector = part->ScCollectors()->CreateCollector();
        auto* rule = part->ScRuleFactory()->CreateRuleBodyDumb(targets, true);
        collector->ReplaceRules(std::vector<SelectionIntentRule*>{rule}, false);
        split->SetTargetBodyCollector(collector);
        auto* tool = split->BooleanTool();
        tool->SetToolOption(GeometricUtilities::BooleanToolBuilder::BooleanToolTypeNewPlane);
        const Vector3d normal(normalX / length, normalY / length, 0);
        Matrix3x3 matrix = {};
        matrix.Xx = 0;
        matrix.Xy = 0;
        matrix.Xz = 1;
        matrix.Yx = normal.Y;
        matrix.Yy = -normal.X;
        matrix.Yz = 0;
        matrix.Zx = normal.X;
        matrix.Zy = normal.Y;
        matrix.Zz = 0;
        auto* plane = part->Planes()->CreateFixedPlane(Point3d(0, 0, 0), matrix);
        tool->FacePlaneTool()->SetToolPlane(plane);
        split->SetTolerance(0.0001);
        Features::Feature* feature = nullptr;
        try {
            feature = split->CommitFeature();
        } catch (const NXException& exception) {
            throw std::runtime_error(std::string("天圆地方分割失败：") + exception.Message());
        }
        split->Destroy();
        split = nullptr;
        feature->SetName(NXString("天圆地方分割", NXString::UTF8));
        ThrowIfErrors(feature, "天圆地方分割失败：");
        std::vector<Body*> result;
        for (auto* body : feature->GetBodies()) {
            if (body != nullptr && body->IsSolidBody() && UF_OBJ_ask_status(body->Tag()) == UF_OBJ_ALIVE)
                result.push_back(body);
        }
        if (result.size() < 2) {
            result.clear();
            for (auto* body : *part->Bodies()) {
                if (body != nullptr && body->IsSolidBody() && UF_OBJ_ask_status(body->Tag()) == UF_OBJ_ALIVE)
                    result.push_back(body);
            }
        }
        if (result.size() < static_cast<size_t>(targets.size()) + 1)
            throw std::runtime_error("天圆地方分割没有生成预期的实体。");
        return result;
    } catch (...) {
        if (split != nullptr) split->Destroy();
        throw;
    }
}

int PlaneSide(Body* body, double normalX, double normalY)
{
    tag_t tag = body->Tag();
    double accuracy[11] = {};
    accuracy[0] = 0.9;
    double properties[47] = {};
    double statistics[13] = {};
    Check(UF_MODL_ask_mass_props_3d(&tag, 1, 1, 3, 1.0, 1, accuracy, properties, statistics));
    const double signedDistance = normalX * properties[3] + normalY * properties[4];
    if (std::abs(signedDistance) < 1e-8) throw std::runtime_error("天圆地方分割后的实体落在对角面上。");
    return signedDistance > 0 ? 1 : 0;
}

std::vector<Body*> JoinSameSide(Part* part, const std::vector<Body*>& bodies, const std::vector<std::pair<double, double>>& planes)
{
    std::map<int, std::vector<Body*>> bins;
    for (auto* body : bodies) {
        int key = 0;
        for (size_t index = 0; index < planes.size(); ++index) {
            if (PlaneSide(body, planes[index].first, planes[index].second) != 0) key |= 1 << static_cast<int>(index);
        }
        bins[key].push_back(body);
    }
    const int expected = 1 << static_cast<int>(planes.size());
    if (static_cast<int>(bins.size()) != expected)
        throw std::runtime_error("天圆地方对角分割没有分成预期的块数。");
    std::vector<Body*> result;
    for (auto& entry : bins) {
        Body* joined = entry.second.front();
        for (size_t index = 1; index < entry.second.size(); ++index)
            joined = UniteBodies(part, joined, entry.second[index]);
        result.push_back(joined);
    }
    std::ostringstream extra;
    int leftovers = 0;
    for (auto* body : *part->Bodies()) {
        if (body == nullptr || !body->IsSolidBody() || UF_OBJ_ask_status(body->Tag()) != UF_OBJ_ALIVE) continue;
        if (std::find(result.begin(), result.end(), body) != result.end()) continue;
        double box[6] = {};
        UF_MODL_ask_bounding_box(body->Tag(), box);
        extra << " [" << box[0] << "," << box[1] << "," << box[2] << " -> " << box[3] << "," << box[4] << "," << box[5]
              << " side " << PlaneSide(body, planes.front().first, planes.front().second) << "]";
        ++leftovers;
    }
    if (leftovers != 0)
        throw std::runtime_error(
            "天圆地方对角合并后仍有多余实体。输入 " + std::to_string(bodies.size())
            + " 输出 " + std::to_string(result.size()) + extra.str());
    return result;
}

Features::Feature* FlatPatternOnFace(Part* part, Face* upward)
{
    Edge* axis = nullptr;
    double bestLength = -1;
    double vertex[3] = {};
    for (auto* edge : upward->GetEdges()) {
        double first[3] = {};
        double second[3] = {};
        int count = 0;
        if (UF_MODL_ask_edge_verts(edge->Tag(), first, second, &count) != 0) continue;
        const double dx = second[0] - first[0];
        const double dy = second[1] - first[1];
        const double dz = second[2] - first[2];
        const double length = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (length <= bestLength) continue;
        bestLength = length;
        axis = edge;
        vertex[0] = first[0];
        vertex[1] = first[1];
        vertex[2] = first[2];
    }
    if (axis == nullptr) throw std::runtime_error("天圆地方展平图样没有参考边。");

    Features::SheetMetal::FlatPatternBuilder* flat = nullptr;
    try {
        flat = part->Features()->SheetmetalManager()->CreateFlatPatternBuilder(nullptr);
        flat->SetApplicationContext(Features::SheetMetal::ApplicationContextNxSheetMetal);
        flat->SetKeepFlatSolidExternal(false);
        flat->SetAssociative(true);
        flat->SetShowInteriorFeatureCurves(true);
        flat->SetOrientation(Features::SheetMetal::FlatSolidBuilder::OrientationTypeEdge);
        flat->UpwardFace()->SetValue(upward);
        flat->XAxisEdge()->SetValue(axis);
        flat->SetReferenceVertex({vertex[0], vertex[1], vertex[2]});
        Features::Feature* feature = nullptr;
        try {
            feature = flat->CommitFeature();
        } catch (const NXException& exception) {
            throw std::runtime_error(std::string("天圆地方展开失败：") + exception.Message());
        }
        flat->Destroy();
        flat = nullptr;
        feature->SetName(NXString("天圆地方展开", NXString::UTF8));
        ThrowIfErrors(feature, "天圆地方展开失败：");
        return feature;
    } catch (...) {
        if (flat != nullptr) flat->Destroy();
        throw;
    }
}

Features::Feature* FlatPatternOf(Part* part, Body* body, double units)
{
    std::vector<Face*> faces;
    faces.push_back(LargestFace(body));
    for (double sign : {-1.0, 1.0}) {
        for (auto* face : CapFaces(body, sign, 5.0 * units)) {
            if (std::find(faces.begin(), faces.end(), face) == faces.end()) faces.push_back(face);
        }
    }
    std::runtime_error last("天圆地方展开失败。");
    for (auto* face : faces) {
        try {
            return FlatPatternOnFace(part, face);
        } catch (const std::runtime_error& error) {
            last = error;
        }
    }
    throw last;
}
}

void Validate(const Input& input)
{
    for (double value : {input.diameterMm, input.lengthMm, input.widthMm, input.heightMm, input.thicknessMm, input.seamMm, input.cornerRadiusMm, input.indexMarkMm}) {
        if (!std::isfinite(value)) throw std::runtime_error("尺寸必须是有限数值。");
    }
    if (input.diameterMm < 1.0 || input.lengthMm < 1.0 || input.widthMm < 1.0 || input.heightMm < 1.0)
        throw std::runtime_error("圆直径、底边长、底边宽和高度必须不小于 1 mm。");
    if (input.thicknessMm < 0.05)
        throw std::runtime_error("板厚必须不小于 0.05 mm。");
    if (input.thicknessMm >= input.diameterMm / 4.0 || input.thicknessMm >= input.lengthMm / 4.0 || input.thicknessMm >= input.widthMm / 4.0)
        throw std::runtime_error("板厚过大。板厚须小于圆直径、底边长和底边宽的四分之一。");
    if (input.seamMm < 0.05 || input.seamMm >= input.diameterMm || input.seamMm >= input.lengthMm || input.seamMm >= input.widthMm)
        throw std::runtime_error("接缝宽度必须不小于 0.05 mm，且小于圆直径、底边长和底边宽。");
    const double shorterSide = std::min(input.lengthMm, input.widthMm);
    if (input.cornerRadiusMm < 0.5)
        throw std::runtime_error("方口圆角必须不小于 0.5 mm。");
    if (input.cornerRadiusMm <= input.thicknessMm)
        throw std::runtime_error("方口圆角必须大于板厚。");
    if (input.cornerRadiusMm >= shorterSide / 2.0 - input.seamMm / 2.0)
        throw std::runtime_error("方口圆角过大，直边长度不足。");
    if (input.segmentedBends) {
        if (input.bendSegments < 1 || input.bendSegments > 99)
            throw std::runtime_error("折弯段的数目必须在 1 到 99 之间。");
        if (input.indexMarkMm < 0.1)
            throw std::runtime_error("索引标记长度必须不小于 0.1 mm。");
    }
    if (input.split < 0 || input.split > 4)
        throw std::runtime_error("分段方式无效。");
}

Result Create(const Input& input)
{
    Validate(input);
    auto* session = Session::GetSession();
    auto* part = session->Parts()->Work();
    if (part == nullptr) throw std::runtime_error("请先打开一个工作零件。");
    const double units = UnitsPerMm(part);
    const double halfX = input.lengthMm * units / 2.0;
    const double halfY = input.widthMm * units / 2.0;
    const double radius = input.diameterMm * units / 2.0;
    const double height = input.heightMm * units;
    const double thickness = input.thicknessMm * units;
    (void)input.seamMm;
    const double fillet = input.cornerRadiusMm * units;
    const double insetX = halfX - fillet;
    const double insetY = halfY - fillet;
    Result result;
    Features::Feature* loft = nullptr;
    auto loftPiece = [&](const std::vector<tag_t>& circle, const std::vector<tag_t>& square,
                         Vec circleStart, Vec squareStart, Vec circleHelp, Vec squareHelp) {
        Features::Feature* feature = LoftChain(part, circle, square, circleStart, squareStart,
            circleHelp, squareHelp, units, thickness,
            input.segmentedBends, input.bendSegments, input.indexMarkMm * units);
        if (loft == nullptr) loft = feature;
        auto bodies = feature->GetBodies();
        if (bodies.size() != 1 || bodies[0] == nullptr || !bodies[0]->IsSolidBody())
            throw std::runtime_error("天圆地方没有生成钣金实体。");
        return bodies[0];
    };
    std::vector<Body*> pieces;
    {
        // Section curves stay on the center split. Diagonal modes cut those halves, then join each side.
        const bool quarters = input.split == 2;
        const std::vector<tag_t> circle = {OpenArc({0, 0, height}, radius,
            quarters ? 3.0 * kPi / 2.0 : kPi, 2.0 * kPi)};
        const std::vector<tag_t> square = quarters
            ? std::vector<tag_t>{
                Line({0, -halfY, 0}, {insetX, -halfY, 0}),
                OpenArc({insetX, -insetY, 0}, fillet, 3.0 * kPi / 2.0, 2.0 * kPi),
                Line({halfX, -insetY, 0}, {halfX, 0, 0})}
            : std::vector<tag_t>{
                Line({-halfX, 0, 0}, {-halfX, -insetY, 0}),
                OpenArc({-insetX, -insetY, 0}, fillet, kPi, 3.0 * kPi / 2.0),
                Line({-insetX, -halfY, 0}, {insetX, -halfY, 0}),
                OpenArc({insetX, -insetY, 0}, fillet, 3.0 * kPi / 2.0, 2.0 * kPi),
                Line({halfX, -insetY, 0}, {halfX, 0, 0})};
        const Vec circleStart = quarters ? Vec{0, -radius, height} : Vec{-radius, 0, height};
        const Vec squareStart = quarters ? Vec{0, -halfY, 0} : Vec{-halfX, 0, 0};
        const Vec circleHelp = quarters ? Vec{radius * 0.5, -radius * 0.5, height} : Vec{0, -radius, height};
        const Vec squareHelp = quarters ? Vec{halfX * 0.5, -halfY, 0} : Vec{0, -halfY, 0};
        Body* seed = loftPiece(circle, square, circleStart, squareStart, circleHelp, squareHelp);
        if (quarters) {
            Body* acrossY = MirrorAcrossFixed(part, seed, false);
            Body* acrossX = MirrorAcrossFixed(part, seed, true);
            pieces = {seed, acrossY, acrossX, MirrorAcrossFixed(part, acrossY, true)};
        } else {
            Body* acrossY = MirrorAcrossFixed(part, seed, false);
            if (input.split == 1) {
                pieces = {seed, acrossY};
            } else if (input.split >= 3) {
                // Keep the center-split sections. Cut those halves on the diagonals,
                // then join the pieces that lie on the same side of the cut.
                const std::pair<double, double> firstPlane{halfY, -halfX};
                std::vector<Body*> cut = SplitByPlane(part, {seed}, firstPlane.first, firstPlane.second);
                auto upper = SplitByPlane(part, {acrossY}, firstPlane.first, firstPlane.second);
                cut.insert(cut.end(), upper.begin(), upper.end());
                if (input.split == 4) {
                    const std::pair<double, double> secondPlane{halfY, halfX};
                    cut = SplitByPlane(part, cut, secondPlane.first, secondPlane.second);
                    pieces = JoinSameSide(part, cut, {firstPlane, secondPlane});
                } else {
                    pieces = JoinSameSide(part, cut, {firstPlane});
                }
            } else {
                OffsetJoinFaces(part, {seed, acrossY}, -0.1 * units, 5.0 * units);
                pieces = {UniteBodies(part, seed, acrossY)};
            }
        }
    }
    result.loft = loft->Tag();
    for (auto* body : pieces) {
        Check(UF_OBJ_set_color(body->Tag(), 186));
        result.formedBodies.push_back(body->Tag());
        if (result.body == 0) result.body = body->Tag();
        Features::Feature* flat = FlatPatternOf(part, body, units);
        if (result.flat == 0) result.flat = flat->Tag();
        for (auto* flatBody : flat->GetBodies()) {
            if (flatBody == nullptr || !flatBody->IsSolidBody() || flatBody->Tag() == body->Tag()) continue;
            if (result.flatBody == 0) result.flatBody = flatBody->Tag();
            result.flatBodies.push_back(flatBody->Tag());
            Check(UF_OBJ_set_color(flatBody->Tag(), 70));
        }
    }
    return result;
}
}
