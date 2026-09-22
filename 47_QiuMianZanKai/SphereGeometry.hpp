#pragma once
#include <uf_defs.h>
#include <cmath>
#include <string>
#include <vector>

namespace sphere_unfold {
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
void Check(int);
struct Source {
    tag_t cylinder=0,sphere=0,body=0;
    Vec center,x,y,z;
    double radius=0,height=0,sweep=0,latitude=0,unitsPerMm=1,thickness=0;
    // radius is the selected cylinder radius; majorRadius is zero for spheres.
    double majorRadius=0,bendRadius=0;
    int bendDirection=1; // +1 outer rim, -1 inner rim of a ring torus
    bool innerSurface=false;
    double RadialAt(double baseRadius,double angle)const{return majorRadius+(baseRadius-majorRadius)*std::cos(angle);}
    double HeightAt(double baseRadius,double angle)const{return bendDirection*(baseRadius-majorRadius)*std::sin(angle);}
    bool IsTorus()const{return majorRadius>0;}
};
struct Settings {
    int petals=12;
    double gap=.5,relief=3;
    bool flat=false,hideSource=true;
};
struct Plan {
    Source source;
    Settings settings;
    double outer=0,inner=0,gap=0,rootGap=0,relief=0,step=0,sweep=0,errorMm=0;
};
struct Result {tag_t body=0,flatBody=0,convert=0,feature=0;std::vector<tag_t> members;};
Source Inspect(tag_t cylinder,tag_t sphere);
Plan MakePlan(const Source&,const Settings&);
std::vector<std::pair<Vec,Vec>> Preview(const Plan&);
// The caller owns the undo transaction. Create always tests native flattening,
// even when the flat body is not requested as an output.
Result Create(const Plan&);
}
