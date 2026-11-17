// Runs in an independent NX process and only writes parts under the given directory.
#include "../TransitionGeometry.hpp"

#include <NXOpen/Body.hxx>
#include <NXOpen/BodyCollection.hxx>
#include <NXOpen/Curve.hxx>
#include <NXOpen/CurveCollection.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/Features_FeatureCollection.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/Sketch.hxx>
#include <NXOpen/SketchCollection.hxx>
#include <uf.h>
#include <uf_modl.h>
#include <uf_obj.h>
#include <uf_part.h>
#include <uf_csys.h>

#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace NXOpen;

void Require(bool ok, const char* message)
{
    if (!ok) throw std::runtime_error(message);
}

void Check(int code)
{
    if (code == 0) return;
    char message[1024] = {};
    UF_get_fail_message(code, message);
    throw std::runtime_error(std::string(message) + " (" + std::to_string(code) + ")");
}

int SolidCount(Part* part)
{
    int count = 0;
    for (auto* body : *part->Bodies()) {
        if (!body->IsSolidBody()) continue;
        UF_OBJ_disp_props_t props = {};
        if (UF_OBJ_ask_display_properties(body->Tag(), &props) == 0 && props.blank_status == UF_OBJ_BLANKED) continue;
        ++count;
    }
    return count;
}

int LooseCurves(Part* part)
{
    int count = 0;
    for (auto* curve : *part->Curves())
        if (part->Sketches()->GetOwningSketch(curve) == nullptr) ++count;
    return count;
}

bool BlankCurves(Part* part)
{
    for (auto* curve : *part->Curves()) {
        if (part->Sketches()->GetOwningSketch(curve) != nullptr) continue;
        UF_OBJ_disp_props_t props = {};
        if (UF_OBJ_ask_display_properties(curve->Tag(), &props) != 0 || props.blank_status != UF_OBJ_BLANKED)
            return false;
    }
    return true;
}

int Containment(const std::vector<tag_t>& bodies, double point[3])
{
    int result = 2;
    for (auto body : bodies) {
        int contains = 0;
        Check(UF_MODL_ask_point_containment(point, body, &contains));
        if (contains == 1 || contains == 3) return contains;
        result = contains;
    }
    return result;
}

void ClosedDuct(double length, double width, double height, double thickness, const std::vector<tag_t>& bodies, double units)
{
    const double halfX = length * units / 2.0;
    const double halfY = width * units / 2.0;
    const double z = height * units / 2.0;
    const double offset = 0.35 * thickness * units;
    const double nearSquare = std::min(thickness * units * 0.3, z);
    double wall[3] = {halfX * 0.35, -(halfY - offset), nearSquare};
    int contains = Containment(bodies, wall);
    std::cout << "bottom-wall containment " << contains << std::endl;
    Require(contains == 1 || contains == 3, "lower wall is missing");

    double mirrored[3] = {halfX * 0.35, halfY - offset, nearSquare};
    contains = Containment(bodies, mirrored);
    std::cout << "mirrored-wall containment " << contains << std::endl;
    Require(contains == 1 || contains == 3, "mirror did not close the upper wall");

    double sharp[3] = {halfX, halfY, std::min(thickness * units * 0.3, z)};
    contains = Containment(bodies, sharp);
    std::cout << "sharp-corner containment " << contains << std::endl;
    Require(contains == 2, "square corner is filled solid");

    double center[3] = {0, 0, z};
    contains = Containment(bodies, center);
    std::cout << "center containment " << contains << std::endl;
    Require(contains == 2, "center of the duct is solid");
}

bool Named(Part* part, const char* name)
{
    for (auto* feature : part->Features()->GetFeatures())
        if (std::string(feature->Name().GetUTF8Text()) == name) return true;
    return false;
}

bool Overlap(const double* a, const double* b)
{
    return !(a[3] < b[0] || b[3] < a[0] || a[4] < b[1] || b[4] < a[1] || a[5] < b[2] || b[5] < a[2]);
}

void FlatThickness(tag_t body, double thickness, double units)
{
    auto* solid = dynamic_cast<Body*>(NXObjectManager::Get(body));
    Require(solid != nullptr, "flat body missing");
    struct Plane {
        double point[3];
        double normal[3];
    };
    std::vector<Plane> planes;
    for (auto* face : solid->GetFaces()) {
        int type = 0;
        int sign = 0;
        Plane plane = {};
        double box[6] = {};
        double radius = 0;
        double minor = 0;
        Check(UF_MODL_ask_face_data(face->Tag(), &type, plane.point, plane.normal, box, &radius, &minor, &sign));
        if (type != 22) continue;
        const double length = std::sqrt(plane.normal[0] * plane.normal[0] + plane.normal[1] * plane.normal[1] + plane.normal[2] * plane.normal[2]);
        if (length < 1e-9) continue;
        plane.normal[0] /= length;
        plane.normal[1] /= length;
        plane.normal[2] /= length;
        planes.push_back(plane);
    }
    Require(!planes.empty(), "flat body has no planar face");
    const double limit = std::max(0.05 * units, thickness * 0.02);
    for (size_t i = 0; i < planes.size(); ++i) {
        for (size_t j = i + 1; j < planes.size(); ++j) {
            const double aligned = planes[i].normal[0] * planes[j].normal[0] + planes[i].normal[1] * planes[j].normal[1] + planes[i].normal[2] * planes[j].normal[2];
            if (aligned > -0.99) continue;
            const double delta[3] = {
                planes[j].point[0] - planes[i].point[0],
                planes[j].point[1] - planes[i].point[1],
                planes[j].point[2] - planes[i].point[2]};
            const double distance = std::abs(delta[0] * planes[i].normal[0] + delta[1] * planes[i].normal[1] + delta[2] * planes[i].normal[2]);
            if (std::abs(distance - thickness) < limit) {
                std::cout << " flat thickness " << distance << "\n";
                return;
            }
        }
    }
    Require(false, "flat thickness does not match sheet thickness");
}

void Case(const std::string& path, bool inches, double diameter, double length, double width, double height, double thickness, double seam, int split = 0)
{
    std::cout << "CASE " << path << std::endl;
    std::filesystem::remove(path);
    tag_t partTag = NULL_TAG;
    Check(UF_PART_new(path.c_str(), inches ? UF_PART_ENGLISH : UF_PART_METRIC, &partTag));
    auto* session = Session::GetSession();
    auto* part = session->Parts()->Work();
    const double units = inches ? 1.0 / 25.4 : 1.0;
    const int featuresBefore = static_cast<int>(part->Features()->GetFeatures().size());
    const int curvesBefore = LooseCurves(part);
    std::vector<tag_t> curvesBeforeTags;
    for (auto* curve : *part->Curves()) curvesBeforeTags.push_back(curve->Tag());
    tianyuan::Input input;
    input.diameterMm = diameter;
    input.lengthMm = length;
    input.widthMm = width;
    input.split = split;
    input.heightMm = height;
    input.thicknessMm = thickness;
    input.seamMm = seam;

    auto mark = session->SetUndoMark(Session::MarkVisibilityVisible, "TianYuanDiFan test");
    tianyuan::Result result;
    try {
        result = tianyuan::Create(input);
    } catch (...) {
        try { UF_PART_save_as((path + "-failed.prt").c_str()); } catch (...) {}
        throw;
    }
    Require(result.body && result.loft && result.flat, "missing feature or body");
    const int pieces = (split == 2 || split == 4) ? 4 : ((split == 1 || split == 3) ? 2 : 1);
    Require(static_cast<int>(result.formedBodies.size()) == pieces, "expected one united body");
    Require(Named(part, "天圆地方") && Named(part, "天圆地方展开"), "feature name mismatch");
    const int solids = SolidCount(part);
    int ducts = 0;
    int flats = 0;
    for (auto* body : *part->Bodies()) {
        if (!body->IsSolidBody()) continue;
        UF_OBJ_disp_props_t props = {};
        if (UF_OBJ_ask_display_properties(body->Tag(), &props) == 0 && props.blank_status == UF_OBJ_BLANKED) continue;
        double box[6] = {};
        Check(UF_MODL_ask_bounding_box(body->Tag(), box));
        const double depth = box[5] - box[2];
        if (depth > height * units / 2.0) ++ducts;
        else if (depth < thickness * units * 4.0) ++flats;
        std::cout << " body " << body->Tag() << " depth " << depth << std::endl;
    }
    std::cout << "solids " << solids << " ducts " << ducts << " flat-sheets " << flats << std::endl;
    Require(ducts == pieces, "formed piece count");
    Require(flats >= 1 && solids == ducts + flats, "flat pattern did not stay a sheet");
    for (auto tag : curvesBeforeTags) {
        UF_OBJ_disp_props_t props = {};
        if (UF_OBJ_ask_display_properties(tag, &props) != 0) continue;
        Require(props.blank_status == UF_OBJ_BLANKED, "profile curves are visible");
    }

    double formed[6] = {};
    Check(UF_MODL_ask_bounding_box(result.formedBodies.front(), formed));
    for (auto tag : result.formedBodies) {
        double box[6] = {};
        Check(UF_MODL_ask_bounding_box(tag, box));
        for (int i = 0; i < 3; ++i) formed[i] = std::min(formed[i], box[i]);
        for (int i = 3; i < 6; ++i) formed[i] = std::max(formed[i], box[i]);
    }
    std::cout << path << " formed " << formed[0] << " " << formed[1] << " " << formed[2]
              << " -> " << formed[3] << " " << formed[4] << " " << formed[5] << std::endl;
    const double reachX = std::max(length, diameter) / 2.0 * units;
    const double reachY = std::max(width, diameter) / 2.0 * units;
    const double outerTol = std::max(0.3 * units, thickness * units * 0.25);
    Require(formed[3] <= reachX + outerTol, "body extends outside the outer size");
    Require(formed[3] >= reachX - thickness * units, "outer length is short");
    Require(formed[1] >= -(reachY + outerTol), "body extends outside the outer width");
    Require(formed[4] <= reachY + outerTol, "body extends outside the outer width");
    Require(formed[1] <= -(reachY - thickness * units), "outer width is short");
    Require(formed[4] >= reachY - thickness * units, "outer width is short");
    Require(std::abs(formed[5] - height * units) < units, "height mismatch");
    Require(formed[2] > -(thickness + 0.5) * units, "formed body extends below the square");
    ClosedDuct(length, width, height, thickness, result.formedBodies, units);
    for (auto tag : result.flatBodies) FlatThickness(tag, thickness * units, units);

    session->UndoToMark(mark, nullptr);
    session->DeleteUndoMark(mark, nullptr);
    Require(SolidCount(part) == 0, "undo left a solid");
    Require(static_cast<int>(part->Features()->GetFeatures().size()) == featuresBefore, "undo left features");
    Require(LooseCurves(part) == curvesBefore, "undo left curves");

    int rejected = 0;
    tianyuan::Input bad = input;
    bad.diameterMm = 0;
    try { tianyuan::Create(bad); } catch (...) { ++rejected; }
    bad = input;
    bad.seamMm = std::min(length, width);
    try { tianyuan::Create(bad); } catch (...) { ++rejected; }
    bad = input;
    bad.thicknessMm = std::min(length, width);
    try { tianyuan::Create(bad); } catch (...) { ++rejected; }
    bad = input;
    bad.cornerRadiusMm = input.thicknessMm;
    try { tianyuan::Create(bad); } catch (...) { ++rejected; }
    Require(rejected == 4, "invalid input was accepted");
    Require(static_cast<int>(part->Features()->GetFeatures().size()) == featuresBefore, "invalid input changed the part");

    auto saveMark = session->SetUndoMark(Session::MarkVisibilityVisible, "TianYuanDiFan save");
    result = tianyuan::Create(input);
    Require(SolidCount(part) == solids, "saved model body count");
    Check(UF_PART_save());
    Check(UF_PART_close(partTag, 0, 1));
    UF_PART_load_status_t status = {};
    Check(UF_PART_open(path.c_str(), &partTag, &status));
    UF_PART_free_load_status(&status);
    part = session->Parts()->Work();
    Require(SolidCount(part) == solids, "reopened body count");
    Require(Named(part, "天圆地方") && Named(part, "天圆地方展开"), "reopened feature names");
    Check(UF_PART_close(partTag, 0, 1));
    std::cout << "PASS " << path << "\n";
    (void)saveMark;
}

int main(int argc, char** argv)
{
    try {
        Require(argc == 2, "Provide a new output directory");
        std::filesystem::create_directories(argv[1]);
        Check(UF_initialize());
        const std::string out = argv[1];
        Case(out + "/square-larger-mm.prt", false, 100, 200, 200, 150, 1.5, 1);
        Case(out + "/circle-larger-mm.prt", false, 300, 100, 100, 120, 1, 1);
        Case(out + "/rectangle-mm.prt", false, 100, 240, 140, 150, 1.5, 1);
        Case(out + "/halves-mm.prt", false, 100, 200, 200, 150, 1.5, 1, 1);
        Case(out + "/quarters-mm.prt", false, 100, 200, 200, 150, 1.5, 1, 2);
        Case(out + "/diagonal-halves-mm.prt", false, 100, 200, 200, 150, 1.5, 1, 3);
        Case(out + "/diagonal-quarters-mm.prt", false, 100, 240, 140, 150, 1.5, 1, 4);
        Case(out + "/square-larger-inch.prt", true, 100, 200, 200, 150, 1.5, 1);
        UF_terminate();
        return 0;
    } catch (const NXException& exception) {
        std::cerr << "NX FAIL " << exception.ErrorCode() << " " << exception.Message() << "\n";
    } catch (const std::exception& exception) {
        std::cerr << "FAIL " << exception.what() << "\n";
    }
    return 1;
}
