#pragma once
#include <cmath>
#include <string>
#include <vector>

namespace tube_straighten {
constexpr double pi=3.14159265358979323846;
struct Vec {
    double x=0,y=0,z=0;
    Vec operator+(Vec b)const{return {x+b.x,y+b.y,z+b.z};}
    Vec operator-(Vec b)const{return {x-b.x,y-b.y,z-b.z};}
    Vec operator*(double s)const{return {x*s,y*s,z*s};}
};
inline double Dot(Vec a,Vec b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline Vec Cross(Vec a,Vec b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline double Length(Vec a){return std::sqrt(Dot(a,a));}
Vec Unit(Vec);
struct Span {
    Vec a,b,center,normal;
    double radius=0,angle=0;
    Vec Point(double fraction)const;
    Vec Tangent(double fraction)const;
    void Reverse();
};
struct Hole {
    std::vector<Span> profile;
    Vec direction;
    double length=0,area=0;
};
struct Source {
    unsigned int body=0;
    bool round=false;
    std::vector<Span> spans;
    Vec normal,widthDirection;
    double width=0,depth=0,thickness=0,cornerRadius=0,unitsPerMm=1;
    std::vector<Hole> holes;
};
struct Settings {
    int divisions=12;
    double radiusMm=1,kFactor=.4,gapMm=.2;
    double bridgeWidthMm=6;
    bool hideSource=false,cutSource=false;
};
struct Bend {double angle=0,start=0,allowance=0,setback=0;Vec vertex;};
struct Segment {Vec origin,axis;double start=0,length=0;};
// A transverse gap in the actual curved source, not in its tangent polygon.
struct SourceSlot {
    Vec origin,axis,center,incoming,outgoing;
    double pathRadius=0,cornerCos=1;
};
struct Plan {
    Source source;Settings settings;
    std::vector<Vec> polygon;
    std::vector<Bend> bends;
    std::vector<Segment> segments;
    std::vector<size_t> holeSegments;
    std::vector<SourceSlot> sourceSlots;
    bool adjustedCuts=false;
    double length=0,errorMm=0,radius=0,gap=0;
};
Plan MakePlan(const Source&,const Settings&);
// Cut outline in flat longitudinal/depth coordinates. y=0 is the retained outer wall.
std::vector<Vec> Notch(const Plan&,const Bend&,double extraDepth=0);
std::vector<std::pair<Vec,Vec>> Preview(const Plan&);
Vec FlatPoint(const Plan&,double x,double y,double z);
Vec ToFlatLocal(const Plan&,size_t segment,Vec point);
Vec ToFolded(const Plan&,size_t segment,Vec local);
Hole FlatHole(const Plan&,size_t hole);
double BridgeHalfAngle(const Plan&);
Vec SourceSlotPoint(const Plan&,const SourceSlot&,double x,double y,double z);
double SourceSlotTop(const Plan&,const SourceSlot&,double x);
std::pair<double,double> ProjectRange(const Hole&,Vec direction);
}
