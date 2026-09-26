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
    Source machine;machine.width=40;machine.depth=30;machine.thickness=2;machine.normal=machine.widthDirection={0,0,1};
    machine.spans={{{200,-100,0},{200,0,0}},{{200,0,0},{0,200,0},{0,0,0},{0,0,1},200,pi/2},{{0,200,0},{-100,200,0}}};
    Settings whole;whole.segmentArcs=false;whole.cutSource=true;whole.gapMm=0;
    auto continuous=MakePlan(machine,whole);Require(continuous.bends.empty()&&continuous.sourceSlots.empty()&&!continuous.settings.cutSource,"whole arcs produced cuts");
    Require(continuous.machineArcs.size()==1&&std::abs(continuous.length-(200+185*pi/2))<1e-10,"whole arc centerline length");
    Require(Length(FlatPoint(continuous,0,0,0)-machine.spans.front().a)<1e-10,"whole start cap moved");
    auto ignored=whole;ignored.divisions=0;ignored.radiusMm=10000;ignored.kFactor=0;ignored.bridgeWidthMm=0;
    Require(std::abs(MakePlan(machine,ignored).length-continuous.length)<1e-10,"irrelevant kerf settings affect whole mode");
    whole.tubeKFactor=.4;auto shifted=MakePlan(machine,whole);Require(std::abs(continuous.length-shifted.length-3*pi/2)<1e-10,"tube K uses wall thickness instead of section height");
    Hole straightHole;straightHole.direction={0,0,1};straightHole.length=2;straightHole.area=pi*9;Vec hc={-60,185,0};straightHole.profile={{{hc.x+3,hc.y,0},{hc.x+3,hc.y,0},hc,{0,0,1},3,2*pi}};machine.holes={straightHole};
    auto withHole=MakePlan(machine,whole);auto fh=FlatHole(withHole,0);for(double f:{0.,.25,.5,.75})Require(Length(ToFolded(withHole,withHole.holeSegments[0],fh.profile[0].Point(f))-straightHole.profile[0].Point(f))<1e-10,"whole straight hole moved on return");
    machine.holes={h};blocked=false;try{MakePlan(machine,whole);}catch(...){blocked=true;}Require(blocked,"deforming arc hole silently treated as rigid");machine.holes.clear();
    whole.tubeKFactor=0;blocked=false;try{MakePlan(machine,whole);}catch(...){blocked=true;}Require(blocked,"zero tube K accepted");whole.tubeKFactor=.5;whole.gapMm=.2;
    // A real corner following a smooth arc keeps one original cut and one source slot.
    machine.spans.push_back({{-100,200,0},{-100,80,0}});auto hybrid=MakePlan(machine,whole);
    Require(hybrid.bends.size()==1&&hybrid.sourceSlots.size()==1&&!hybrid.sourceSlots[0].pathRadius,"hybrid arc slotted or corner lost");
    Require(std::abs(hybrid.length-(320+185*pi/2-6+1.8*pi/2))<1e-10,"hybrid neutral arc plus corner allowance");
    machine.holes={straightHole};auto hybridHole=MakePlan(machine,whole);Require(hybridHole.holeSegments.size()==1,"hybrid hole bend indexing");machine.holes.clear();
    auto segmented=whole;segmented.segmentArcs=true;auto old=MakePlan(machine,segmented);Require(old.bends.size()==13&&old.sourceSlots.size()==13&&old.machineArcs.empty(),"enabled mode no longer uses original cuts");
    Source two;two=machine;two.spans={{{200,0,0},{0,200,0},{0,0,0},{0,0,1},200,pi/2},{{0,200,0},{-100,200,0}},{{-100,200,0},{-220,80,0},{-100,80,0},{0,0,1},120,pi/2}};
    auto multi=MakePlan(two,whole);Require(multi.machineArcs.size()==2&&multi.bends.empty()&&std::abs(multi.length-(100+(185+105)*pi/2))<1e-10,"multiple exact arc radii");
    Source reverse;reverse.width=30;reverse.depth=20;reverse.thickness=2;reverse.normal=reverse.widthDirection={0,0,1};
    reverse.spans={{{-100,0,0},{0,0,0}},{{0,0,0},{100,100,0},{0,100,0},{0,0,1},100,pi/2},{{100,100,0},{100,200,0}},{{100,200,0},{260,360,0},{260,200,0},{0,0,-1},160,pi/2},{{260,360,0},{400,360,0}}};
    Settings rev;rev.gapMm=0;rev.divisions=8;auto rp=MakePlan(reverse,rev);int negative=0;
    for(size_t i=0;i<rp.bends.size();++i){const auto& b=rp.bends[i];negative+=b.reversed;double slope=tan(b.angle/2);
        for(double fromOuter:{4.,10.,19.}){double y=b.reversed?20-fromOuter:fromOuter,spread=(fromOuter-3)*slope;
            auto left=ToFolded(rp,i,{b.start-spread,y,15}),right=ToFolded(rp,i+1,{b.start+b.allowance+spread,y,15});Require(Length(left-right)<1e-8,"reverse notch faces do not meet when refolded");}
    }
    Require(negative==8&&rp.bends.size()==16,"reverse arc cut signs incorrect");
    rev.cutSource=true;rev.gapMm=.2;auto slots=MakePlan(reverse,rev);int reversedSlots=0;for(const auto& slot:slots.sourceSlots){reversedSlots+=slot.reversed;Require(slot.pathRadius==(slot.reversed?180:100),"wrong reverse source-slot radius");}Require(reversedSlots==8,"source-slot retained wall not switched");
    for(bool segmentedMode:{false,true}){rev.segmentArcs=segmentedMode;rev.useAnchor=true;rev.anchorPoint={90,150,30};auto anchored=MakePlan(reverse,rev);
        bool found=false;for(size_t i=0;i<anchored.segments.size();++i)if(Length(anchored.segments[i].axis-Vec{0,1,0})<1e-7){auto local=ToFlatLocal(anchored,i,{90,150,12});if(local.x>anchored.segments[i].start&&local.x<anchored.segments[i].start+anchored.segments[i].length){Require(Length(FlatPoint(anchored,local.x,local.y,local.z)-Vec{90,150,12})<1e-8,"selected middle straight section moved");found=true;}}
        Require(found,"fixed middle straight missing");if(!segmentedMode)Require(std::abs(anchored.length-(340+260*pi/2))<1e-8,"reverse arc centerline length wrong");
    }
    rev.segmentArcs=false;rev.tubeKFactor=.42;rev.anchorPoint={90,150,30};auto reverseK=MakePlan(reverse,rev);Require(std::abs(reverseK.length-(340+(260-3.2)*pi/2))<1e-8,"reverse K must use each bend's inner radius");
    rev.tubeKFactor=.5;const auto& secondArc=reverse.spans[3];Vec arcCenter=secondArc.Point(.4)+Cross(reverse.normal,secondArc.Tangent(.4))*10+Vec{0,0,15};rev.anchorPoint=arcCenter;auto arcAnchor=MakePlan(reverse,rev);double station=arcAnchor.machineArcs[1].start+arcAnchor.machineArcs[1].length*.4;Require(Length(FlatPoint(arcAnchor,station,10,15)-arcCenter)<1e-8&&Length(arcAnchor.flatAxis-secondArc.Tangent(.4))<1e-8,"clicked arc section/tangent moved");
    Source zigzag=reverse;zigzag.spans={{{0,0,0},{150,0,0}},{{150,0,0},{150,150,0}},{{150,150,0},{300,150,0}}};rev.useAnchor=false;rev.cutSource=true;auto zp=MakePlan(zigzag,rev);Require(zp.bends.size()==2&&!zp.bends[0].reversed&&zp.bends[1].reversed&&zp.sourceSlots[1].reversed,"reverse sharp corner lost");
    std::cout<<"PASS: opposite arc directions, alternating retained walls, reverse notch refolding, source-slot radii, middle/arc fixed frames, signed K lengths and reverse corners\n";
    std::cout<<"PASS: whole arc, tube K, mixed corner, multiple radii, straight holes and deforming-hole rejection, enabled-mode regression\n";
    std::cout<<"PASS: allowance, setbacks, physical miter closure, arc endpoint/tangent, convergence, invalid inputs\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
