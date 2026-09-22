#include "../TubePlan.hpp"
#include <iostream>
#include <stdexcept>
using namespace tube_straighten;
void Require(bool x,const char* s){if(!x)throw std::runtime_error(s);}
int main(){try{
    Source s;s.width=40;s.depth=30;s.thickness=2;s.normal={0,0,1};s.widthDirection={0,0,1};
    s.spans={{{0,0,0},{200,0,0}},{{200,0,0},{200,150,0}}};Settings opt;opt.gapMm=0;
    auto p=MakePlan(s,opt);Require(p.bends.size()==1,"miter bend count");
    Require(std::abs(p.length-(350-6+1.8*pi/2))<1e-10,"bend allowance and setback");
    for(double depth:{3.,5.,20.,30.}){
        auto b=p.bends[0];double slope=std::tan(b.angle/2);
        Vec left={200-b.setback+(3-depth)*slope,depth,0};
        Vec right={200-depth,b.setback+(depth-3)*slope,0};
        Require(Length(left-right)<1e-10,"cut faces fail to meet when folded");
    }
    s.spans={{{200,0,0},{0,200,0},{0,0,0},{0,0,1},200,pi/2}};
    auto arc=MakePlan(s,opt);Require(arc.bends.size()==12,"arc subdivision count");
    Require(Length(arc.polygon.front()-s.spans[0].a)<1e-9&&Length(arc.polygon.back()-s.spans[0].b)<1e-9,"arc end points changed");
    Require(Length(Unit(arc.polygon[1]-arc.polygon[0])-s.spans[0].Tangent(0))<1e-9,"arc start tangent changed");
    opt.divisions=24;auto fine=MakePlan(s,opt);Require(fine.errorMm<arc.errorMm/3.9,"subdivision error not converging");
    // A hole straddling the uniform arc boundary forces a change of cut
    // stations; its exact profile must stay fixed in the source coordinates.
    opt.divisions=12;double alpha=pi/2*(5.5/12.);Vec center={185*cos(alpha),185*sin(alpha),0};
    Hole h;h.direction={0,0,1};h.length=2;h.area=pi*9;h.profile={{{center.x+3,center.y,0},{center.x+3,center.y,0},center,{0,0,1},3,2*pi}};s.holes={h};
    auto adjusted=MakePlan(s,opt);Require(adjusted.adjustedCuts,"arc did not avoid hole");auto flat=FlatHole(adjusted,0);
    for(double f:{0.,.125,.25,.375,.5,.625,.75,.875})Require(Length(ToFolded(adjusted,adjusted.holeSegments[0],flat.profile[0].Point(f))-h.profile[0].Point(f))<1e-8,"hole profile moved on fold back");
    auto range=ProjectRange(h,{1,0,0});Require(std::abs(range.first-(center.x-3))<1e-10&&std::abs(range.second-(center.x+3))<1e-10,"full-circle exact bounds");
    Source corner;corner.width=40;corner.depth=30;corner.thickness=2;corner.normal={0,0,1};corner.widthDirection={0,0,1};corner.spans={{{0,0,0},{200,0,0}},{{200,0,0},{200,150,0}}};
    Hole conflict=h;conflict.profile={{{188,15,0},{188,15,0},{185,15,0},{0,0,1},3,2*pi}};corner.holes={conflict};bool blocked=false;try{MakePlan(corner,opt);}catch(...){blocked=true;}Require(blocked,"corner cut consumed hole");s.holes.clear();
    int rejected=0;opt.divisions=1;try{MakePlan(s,opt);}catch(...){++rejected;}opt.divisions=12;opt.kFactor=0;try{MakePlan(s,opt);}catch(...){++rejected;}
    opt.kFactor=.4;opt.radiusMm=40;try{MakePlan(s,opt);}catch(...){++rejected;}
    opt.radiusMm=1;s.spans={{{0,0,0},{10,0,0}},{{10,0,0},{10,10,0}}};try{MakePlan(s,opt);}catch(...){++rejected;}
    Require(rejected==4,"invalid plans accepted");
    std::cout<<"PASS: allowance, setbacks, physical miter closure, arc endpoint/tangent, convergence, invalid inputs\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
