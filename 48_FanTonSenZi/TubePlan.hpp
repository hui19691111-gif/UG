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
    Vec frameY,frameZ; // transported section frame for spatial centerline spans
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
    bool spatial=false;
    std::vector<Span> spans;
    Vec normal,widthDirection;
    // Original planar round-tube end normals. Zero means a square end.
    Vec startCutNormal,endCutNormal;
    double width=0,depth=0,thickness=0,cornerRadius=0,unitsPerMm=1;
    std::vector<Hole> holes;
};
struct Settings {
    int divisions=12;
    double radiusMm=1,kFactor=.4,gapMm=.2;
    double bridgeWidthMm=6;
    double tubeKFactor=.5; // neutral radius = inside path radius + K * section depth
    bool hideSource=false,cutSource=false,segmentArcs=true;
    bool useAnchor=false;
    Vec anchorPoint;
};
struct Bend {double angle=0,start=0,allowance=0,setback=0;Vec vertex,incoming,outgoing;bool reversed=false,acrossZ=false;Vec normal,widthDirection;};
struct Segment {Vec origin,axis;double start=0,length=0;int beforeBend=-1,afterBend=-1;Vec frameY,frameZ;};
struct MachineArc {Span source;double start=0,length=0,neutralRadius=0;};
// A transverse gap in the actual curved source, not in its tangent polygon.
struct SourceSlot {
    Vec origin,axis,center,incoming,outgoing;
    double pathRadius=0,cornerCos=1;
    bool reversed=false;
    Vec normal,widthDirection;
    double depth=0,width=0;
};
struct Plan {
    Source source;Settings settings;
    std::vector<Vec> polygon;
    std::vector<Bend> bends;
    std::vector<Segment> segments;
    std::vector<MachineArc> machineArcs;
    std::vector<size_t> holeSegments;
    std::vector<SourceSlot> sourceSlots;
    bool adjustedCuts=false;
    double length=0,errorMm=0,radius=0,gap=0;
    Vec flatOrigin,flatAxis,flatInside,flatWidth;
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
Vec RoundEndNormal(const Plan&,bool end);
double RoundEndX(const Plan&,bool end,double y,double z);
double RoundEndExtent(const Plan&,bool end);
Vec SourceSlotPoint(const Plan&,const SourceSlot&,double x,double y,double z);
Vec SourceSlotNormal(const Plan&,const SourceSlot&);
Vec SourceSlotWidth(const Plan&,const SourceSlot&);
double BendDepth(const Plan&,const Bend&);
Vec CutPoint(const Plan&,const Bend&,double x,double y,double z);
Vec Rotate(Vec vector,Vec axis,double angle);
Vec SectionY(const Source&,const Span&,double fraction);
Vec SectionZ(const Source&,const Span&,double fraction);
double SourceSlotTop(const Plan&,const SourceSlot&,double x);
std::pair<double,double> ProjectRange(const Hole&,Vec direction);
}
