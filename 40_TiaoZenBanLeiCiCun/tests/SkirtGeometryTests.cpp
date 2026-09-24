#include "../PanelSkirtGeometry.hpp"
#include <NXOpen/Body.hxx>
#include <NXOpen/Direction.hxx>
#include <NXOpen/DirectionCollection.hxx>
#include <NXOpen/FaceDumbRule.hxx>
#include <NXOpen/Features_FeatureCollection.hxx>
#include <NXOpen/Features_Feature.hxx>
#include <NXOpen/Features_MoveFaceBuilder.hxx>
#include <NXOpen/NXException.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/ScRuleFactory.hxx>
#include <NXOpen/Session.hxx>
#include <uf.h>
#include <uf_part.h>
#include <array>
#include <iostream>
#include <stdexcept>

using namespace NXOpen;

int inPlaceMoves = 0;

void Check(bool value, const std::string& message)
{
    if (!value) throw std::runtime_error(message);
}

void Uf(int status)
{
    if (status)
    {
        char message[256] = {};
        UF_get_fail_message(status, message);
        throw std::runtime_error(std::to_string(status) + ": " + message);
    }
}

std::array<double, 6> Bounds(Body* body)
{
    std::array<double, 6> bounds{};
    Uf(UF_MODL_ask_bounding_box(body->Tag(), bounds.data()));
    return bounds;
}

void CheckBounds(Body* body, const std::array<double, 6>& expected)
{
    const auto actual = Bounds(body);
    for (std::size_t index = 0; index < actual.size(); ++index)
        Check(std::abs(actual[index] - expected[index]) < 0.001,
              "Undo did not restore the original body position/size");
}

tag_t Block(double x, double y, double z, double dx, double dy, double dz)
{
    double corner[] = {x, y, z};
    std::array<std::string, 3> lengths{
        std::to_string(dx), std::to_string(dy), std::to_string(dz)};
    char* dimensions[] = {lengths[0].data(), lengths[1].data(), lengths[2].data()};
    tag_t feature, body;
    Uf(UF_MODL_create_block1(UF_NULLSIGN, corner, dimensions, &feature));
    Uf(UF_MODL_ask_feat_body(feature, &body));
    return body;
}

void Cut(tag_t body, tag_t tool)
{
    int count = 0;
    tag_t* result = nullptr;
    Uf(UF_MODL_subtract_bodies(body, tool, &count, &result));
    UF_free(result);
    Check(count == 1, "Cut split the fixture");
}

void RoundEdge(tag_t body, double x, double z, const char* radius)
{
    uf_list_p_t edges = nullptr;
    Uf(UF_MODL_create_list(&edges));
    for (Edge* edge : dynamic_cast<Body*>(NXObjectManager::Get(body))->GetEdges())
    {
        Point3d a, b;
        Vector3d direction;
        if (panel_skirt::Line(edge, a, b, direction) &&
            std::abs(direction.Y) > 0.99999 &&
            std::abs(a.X - x) < 0.001 && std::abs(a.Z - z) < 0.001)
            Uf(UF_MODL_put_list_item(edges, edge->Tag()));
    }
    int count = 0;
    Uf(UF_MODL_ask_list_count(edges, &count));
    Check(count == 1, "Bend edge missing");
    tag_t feature;
    Uf(UF_MODL_create_blend(radius, edges, 0, 0, 0, 0.001, &feature));
    Uf(UF_MODL_delete_list(&edges));
}

Body* Fixture(int type)
{
    tag_t body = Block(0, 0, 0, 200, 150, 2);
    if (type == 3)
    {
        Uf(UF_MODL_unite_bodies(body, Block(198, 0, 2, 2, 150, 20)));
        Uf(UF_MODL_unite_bodies(body, Block(175, 0, 20, 23, 150, 2)));
        RoundEdge(body, 200, 22, "4");
        RoundEdge(body, 198, 20, "2");
        RoundEdge(body, 200, 0, "4");
        RoundEdge(body, 198, 2, "2");
        return dynamic_cast<Body*>(NXObjectManager::Get(body));
    }
    if (type > 0)
    {
        for (tag_t wall : {Block(0, 0, 2, 2, 150, 20),
                           Block(198, 0, 2, 2, 150, 20),
                           Block(2, 0, 2, 196, 2, 20),
                           Block(2, 148, 2, 196, 2, 20)})
            Uf(UF_MODL_unite_bodies(body, wall));
    }
    if (type > 1)
    {
        // Unequal return widths: L=20, R=25, bottom=15, top=18.
        for (tag_t lip : {Block(2, 2, 20, 18, 146, 2),
                          Block(175, 2, 20, 23, 146, 2),
                          Block(20, 2, 20, 155, 13, 2),
                          Block(20, 132, 20, 155, 16, 2)})
            Uf(UF_MODL_unite_bodies(body, lip));
        // The main skin is a ring, as in the reported part.
        Cut(body, Block(40, 35, -1, 120, 80, 4));
        Cut(body, Block(90, 8, -1, 20, 6, 4));
    }
    return dynamic_cast<Body*>(NXObjectManager::Get(body));
}

Face* MainSkin(Body* body, bool underside)
{
    for (Face* face : body->GetFaces())
    {
        Point3d point;
        Vector3d direction;
        double radius = 0;
        if (face->SolidFaceType() == Face::FaceTypePlanar &&
            panel_skirt::Surface(face, point, direction, radius) &&
            std::abs(direction.Z) > 0.99999 &&
            std::abs(point.Z - (underside ? 0.0 : 2.0)) < 0.001)
            return face;
    }
    throw std::runtime_error("Main skin missing");
}

void CheckWidths(Face* skin, int type, bool underside)
{
    const std::array<Vector3d, 4> directions{
        Vector3d(-1, 0, 0), Vector3d(1, 0, 0),
        Vector3d(0, -1, 0), Vector3d(0, 1, 0)};
    const double widths[] = {20, 25, 15, 18};
    for (int side = 0; side < 4; ++side)
    {
        auto selection = panel_skirt::Collect(skin, Point3d(0, 0, underside ? 0 : 2),
            Vector3d(0, 0, underside ? -1 : 1), directions[side], 2, 0.001);
        Check(selection.error.empty(), selection.error);
        double minimum = 1.0e100, maximum = -1.0e100;
        for (Face* face : selection.faces)
        {
            Point3d point;
            Vector3d normal;
            double radius = 0;
            Check(panel_skirt::Surface(face, point, normal, radius), "Missing profile face");
            const double coordinate = point.X * directions[side].X + point.Y * directions[side].Y;
            minimum = (std::min)(minimum, coordinate);
            maximum = (std::max)(maximum, coordinate);
        }
        const double expected = type == 2 ? widths[side] :
            (type == 3 ? (side == 1 ? 25.0 : 0.0) : type * 2.0);
        Check(std::abs(maximum - minimum - expected) < 0.001,
              "Measured return width changed on side " + std::to_string(side));
    }
}

void MoveSide(Face* skin, const Vector3d& outward, double distance,
              int expectedCount, bool underside, bool expectRejected = false)
{
    Point3d origin(0, 0, underside ? 0 : 2);
    Vector3d normal(0, 0, underside ? -1 : 1);
    auto selection = panel_skirt::Collect(skin, origin, normal, outward, 2, 0.001);
    Check(selection.error.empty(), selection.error);
    Check(static_cast<int>(selection.faces.size()) == expectedCount,
          "Wrong number of moving profile surfaces: " + std::to_string(selection.faces.size()));
    std::vector<panel_skirt::SurfacePosition> before;
    for (Face* face : selection.faces)
    {
        panel_skirt::SurfacePosition value;
        Check(panel_skirt::Measure(face, origin, normal, outward, value), "Measure failed");
        before.push_back(value);
    }
    Part* part = Session::GetSession()->Parts()->Work();
    auto* builder = part->Features()->CreateMoveFaceBuilder(nullptr);
    auto* direction = part->Directions()->CreateDirection(origin, outward,
        SmartObject::UpdateOptionWithinModeling);
    builder->SetType(Features::MoveFaceBuilder::TypesTranslateDirectionAndDistance);
    builder->SetDirection(direction);
    builder->Distance()->SetFormula(std::to_string(distance));
    auto* rule = part->ScRuleFactory()->CreateRuleFaceDumb(selection.faces);
    builder->MoveFaceCollector()->ReplaceRules({rule}, false);
    const auto result = panel_skirt::CommitAndVerifyMove(
        builder, before, origin, normal, outward,
        distance + (expectRejected ? 1.0 : 0.0), 0.001);
    builder->Destroy();
    if (expectRejected)
    {
        Check(!result.error.empty(), "Incorrect displacement was accepted");
        return;
    }
    Check(result.error.empty(), result.error);
    if (result.featureTag == NULL_TAG)
        ++inPlaceMoves;
    for (const auto& original : before)
    {
        auto* face = dynamic_cast<Face*>(NXObjectManager::Get(original.tag));
        panel_skirt::SurfacePosition after;
        Check(panel_skirt::Measure(face, origin, normal, outward, after), "Remeasure failed");
        Check(std::abs(after.outward - original.outward - distance * original.motionProjection) < 0.001,
              "A wall or return end did not translate: skirt width changed");
        Check(std::abs(after.radius - original.radius) < 0.001, "Bend radius changed");
        Check(std::abs(after.height - original.height) < 0.001, "Bend height changed");
    }
}

int main()
{
    try
    {
        Uf(UF_initialize());
        const std::array<Vector3d, 4> directions{
            Vector3d(-1, 0, 0), Vector3d(1, 0, 0),
            Vector3d(0, -1, 0), Vector3d(0, 1, 0)};
        int checks = 0;
        for (int type = 0; type <= 3; ++type)
            for (bool underside : {false, true})
            {
                tag_t part;
                std::string name = "skirt_regression_" + std::to_string(type) +
                    (underside ? "_bottom.prt" : "_top.prt");
                Uf(UF_PART_new(name.c_str(), 1, &part));
                std::cout << "CREATE fixture=" << type << " underside=" << underside << std::endl;
                Body* body = Fixture(type);
                Face* skin = MainSkin(body, underside);
                CheckWidths(skin, type, underside);
                const auto originalBounds = Bounds(body);
                // Null commit results must still fail if geometry is wrong.
                const auto rejected = Session::GetSession()->SetUndoMark(
                    Session::MarkVisibilityInvisible, "rejected move");
                MoveSide(skin, directions[1], 1, type == 3 ? 7 : type + 1,
                         underside, true);
                Session::GetSession()->UndoToMark(rejected, "rejected move");
                Session::GetSession()->DeleteUndoMark(rejected, "rejected move");
                CheckBounds(body, originalBounds);
                const auto mark = Session::GetSession()->SetUndoMark(
                    Session::MarkVisibilityInvisible, "skirt regression");
                for (double delta : {-0.25, 0.25, 5.0, -8.0, 4.0})
                    for (const auto& direction : directions)
                    {
                        const int count = type == 3 ? (direction.X > 0 ? 7 : 1) : type + 1;
                        MoveSide(skin, direction, delta, count, underside);
                        CheckWidths(skin, type, underside);
                        ++checks;
                    }
                Session::GetSession()->UndoToMark(mark, "skirt regression");
                Session::GetSession()->DeleteUndoMark(mark, "skirt regression");
                CheckWidths(skin, type, underside);
                CheckBounds(body, originalBounds);
                // A cancelled preview must leave a usable original profile.
                const auto applied = Session::GetSession()->SetUndoMark(
                    Session::MarkVisibilityInvisible, "apply preview");
                MoveSide(skin, directions[1], 1, type == 3 ? 7 : type + 1, underside);
                CheckWidths(skin, type, underside);
                Session::GetSession()->SetUndoMarkName(applied, "panel resize");
                Session::GetSession()->SetUndoMarkVisibility(
                    applied, "panel resize", Session::MarkVisibilityVisible);
                auto appliedBounds = originalBounds;
                appliedBounds[3] += 1;
                CheckBounds(body, appliedBounds);
                ++checks;
                std::cout << "PASS fixture=" << type << " underside=" << underside << std::endl;
            }
        std::cout << "PASS " << checks << " NX commits and geometry moves; in-place="
                  << inPlaceMoves << std::endl;
        UF_terminate();
        return 0;
    }
    catch (const NXException& ex)
    {
        std::cerr << "NX FAIL " << ex.ErrorCode() << ": " << ex.Message() << std::endl;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "FAIL " << ex.what() << std::endl;
    }
    return 1;
}
