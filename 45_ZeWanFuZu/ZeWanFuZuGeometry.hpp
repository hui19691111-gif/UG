#pragma once
#include <uf_defs.h>
#include <vector>

namespace bend_assist {
struct Vec {
    double x=0, y=0, z=0;
    Vec operator+(Vec b) const { return {x+b.x,y+b.y,z+b.z}; }
    Vec operator-(Vec b) const { return {x-b.x,y-b.y,z-b.z}; }
    Vec operator*(double s) const { return {x*s,y*s,z*s}; }
};
double Dot(Vec a, Vec b);
Vec Cross(Vec a, Vec b);
Vec Unit(Vec a);
struct Settings {
    // UI values are always millimetres, including for inch parts.
    double width=10, bridgeWidth=1, inset=3, gap=2, endDistance=5, height=50;
    bool autoAlign=true, reverseEnd=false;
};
struct FaceInfo {
    tag_t face=0, body=0, flange=0, bendEdge=0;
    Vec origin, x, y, z;
    double thickness=0, length=0, slope=0, autoTop=0, unitsPerMm=1;
    bool parallel=false;
    bool circular=false;
    double centerX=0, centerY=0, radius=0, branch=1;
};
struct Segment { Vec start, middle, end; bool circular=false; Vec center; double radius=0; };
struct Plan {
    FaceInfo face;
    std::vector<Segment> segments;
    // Sampled only for temporary display; modeling uses exact lines/arcs.
    std::vector<Vec> outline;
    double x0=0, x1=0, top=0, gap=0, bridgeWidth=0, inset=0;
};
void Check(int code);
FaceInfo Inspect(tag_t face, tag_t preferredBendEdge=0);
double BoundaryHeight(const FaceInfo& face, double x);
// True outward normal offset: parallel line or concentric circle.
double GapHeight(const FaceInfo& face, double x, double gap);
std::vector<Plan> PlanFaces(const std::vector<FaceInfo>& faces, const Settings& settings);
// Caller owns an NX undo mark; any failure must roll back the entire batch.
tag_t Create(const Plan& plan);
struct Construction { tag_t extrude=0, unite=0; std::vector<tag_t> curves; };
Construction CreateConstruction(const Plan& plan);
void EditConstruction(const Plan& plan, Construction& construction);
}
