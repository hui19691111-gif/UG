#pragma once
#include <uf_defs.h>
#include <filesystem>
#include <string>
#include <vector>

namespace bend_sim {
struct Vec {
    double x=0,y=0,z=0;
    Vec operator+(Vec b) const {return {x+b.x,y+b.y,z+b.z};}
    Vec operator-(Vec b) const {return {x-b.x,y-b.y,z-b.z};}
    Vec operator*(double s) const {return {x*s,y*s,z*s};}
};
double Dot(Vec a,Vec b);
Vec Cross(Vec a,Vec b);
Vec Unit(Vec a);
struct Point {double x=0,z=0;};
struct Tool {std::string name; std::vector<Point> profile;};
void ValidateTool(const Tool&);
std::vector<Tool> BuiltinTools();
Tool ReadTool(const std::filesystem::path&);
void RenameTool(const std::filesystem::path&, const std::string& name);
struct Bend {
    tag_t body=0,selection=0;
    Vec tip,x,axis,up;
    double radius=0,length=0,unitsPerMm=1,tolerance=0;
    bool sharp=false;
};
struct Settings {
    double length=0,axial=0,lateral=0,lift=0,tilt=0,innerRadius=1,safeGap=0.5;
    bool reverse=false;
};
struct Placement {Bend bend; Tool tool; Vec origin,x,axis,up; double length=0;};
enum class Status {Interference,Contact,Near,Clear};
struct Result {
    Status status=Status::Contact;
    double distanceMm=0,toleranceMm=0;
    Vec onPart,onTool;
    // Coordinates survive rollback; no temporary NX object handles escape.
    std::vector<std::pair<Vec,Vec>> interferenceLines;
    int interferenceRegions=0;
};
void Check(int);
Bend Inspect(tag_t selected,double sharpInnerRadiusMm);
Placement Place(const Bend&,const Tool&,const Settings&);
std::vector<std::pair<Vec,Vec>> Outline(const Placement&);
// The caller must own an undo mark for this low-level modeling helper.
tag_t CreateToolBody(const Placement&);
Result CheckBodies(tag_t partBody,tag_t toolBody,double unitsPerMm,double safeGapMm,double tolerance);
// Creates the tool under an invisible mark and rolls it back on every path.
Result InspectPlacement(const Placement&,double safeGapMm);
std::string Describe(const Result&);
}
