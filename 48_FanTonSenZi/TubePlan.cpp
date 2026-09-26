#include "TubePlan.hpp"
#include <algorithm>
#include <stdexcept>
namespace tube_straighten {
Vec Unit(Vec v){double n=Length(v);if(n<1e-12)throw std::runtime_error("无效方向。");return v*(1/n);}
Vec Rotate(Vec v,Vec n,double a){return v*cos(a)+Cross(n,v)*sin(a)+n*(Dot(n,v)*(1-cos(a)));}
Vec SectionY(const Source& s,const Span& span,double f){return s.spatial?(span.radius?Rotate(span.frameY,span.normal,span.angle*f):span.frameY):Cross(s.normal,span.Tangent(f));}
Vec SectionZ(const Source& s,const Span& span,double f){return s.spatial?(span.radius?Rotate(span.frameZ,span.normal,span.angle*f):span.frameZ):s.widthDirection;}
static Vec SegmentY(const Plan& p,const Segment& s){return p.source.spatial?s.frameY:Cross(p.source.normal,s.axis);}
static Vec SegmentZ(const Plan& p,const Segment& s){return p.source.spatial?s.frameZ:p.source.widthDirection;}
double BendDepth(const Plan& p,const Bend& b){return b.acrossZ?p.source.width:p.source.depth;}
Vec CutPoint(const Plan& p,const Bend& b,double x,double y,double z){return b.acrossZ?FlatPoint(p,x,z,y):FlatPoint(p,x,y,z);}
Vec Span::Point(double f)const {if(!radius)return a+(b-a)*f;Vec r=a-center;return center+r*std::cos(angle*f)+Cross(normal,r)*std::sin(angle*f);}
Vec Span::Tangent(double f)const {return radius?Unit(Cross(normal,Point(f)-center)):Unit(b-a);}
void Span::Reverse(){std::swap(a,b);normal=normal*(-1);}
static Plan BasePlan(const Source& source,const Settings& settings,bool hasCuts){
    Plan p;p.source=source;p.settings=settings;
    double u=source.unitsPerMm,t=source.thickness,d=source.depth,w=source.width;
    if(!std::isfinite(u)||u<=0||!std::isfinite(t)||!std::isfinite(d)||!std::isfinite(w)||t<=0||d<=2*t||w<=2*t)throw std::runtime_error("方通截面或壁厚无效。");
    if(source.spans.empty())throw std::runtime_error("请选择方通外侧的连续边。");
    p.flatOrigin=source.spans.front().a;p.flatAxis=source.spans.front().Tangent(0);
    p.flatInside=SectionY(source,source.spans.front(),0);p.flatWidth=SectionZ(source,source.spans.front(),0);
    if(source.spatial)p.flatOrigin=p.flatOrigin-p.flatInside*(d/2)-p.flatWidth*(w/2);
    if(hasCuts&&(!std::isfinite(settings.radiusMm)||settings.radiusMm<0||!std::isfinite(settings.kFactor)||settings.kFactor<=0||settings.kFactor>1||!std::isfinite(settings.gapMm)||settings.gapMm<0))throw std::runtime_error("内弯半径、切缝不得为负；连接壁 K 系数须大于 0 且不大于 1。");
    p.radius=hasCuts?settings.radiusMm*u:0;p.gap=hasCuts?settings.gapMm*u:0;
    if(!hasCuts)p.settings.cutSource=false;
    if(p.settings.cutSource)p.settings.hideSource=false;
    if(source.round){
        if(hasCuts&&(!std::isfinite(settings.bridgeWidthMm)||settings.bridgeWidthMm<=0||settings.bridgeWidthMm*u>pi*d/4))throw std::runtime_error("圆管连接带宽须大于 0，且不超过外圆周长的四分之一。");
        if(!source.holes.empty())throw std::runtime_error("带孔槽圆管暂不支持展开，已取消生成以免丢孔。");
    }
    if(hasCuts&&p.radius+t>=d-t)throw std::runtime_error("内弯半径过大，切口无法在对侧管壁闭合。");
    return p;
}
static Plan BuildSpatialPlan(const Source& source,const Settings& settings,double phase){
    const bool segmented=settings.segmentArcs;auto p=BasePlan(source,settings,segmented);const double d=source.depth,w=source.width,t=source.thickness,u=source.unitsPerMm;
    if(segmented&&(settings.divisions<2||settings.divisions>180))throw std::runtime_error("圆弧分段数须为 2–180。");
    if(!segmented&&(!std::isfinite(settings.tubeKFactor)||settings.tubeKFactor<=0||settings.tubeKFactor>1))throw std::runtime_error("弯管 K 因子须大于 0 且不大于 1。");
    struct Piece{Vec a,b,y,z;};std::vector<Piece> pieces;bool hasArc=false;
    auto append=[&](Piece piece){if(Length(piece.b-piece.a)<1e-6*u)return;if(!pieces.empty()&&Length(Unit(pieces.back().b-pieces.back().a)-Unit(piece.b-piece.a))<1e-7&&Length(pieces.back().y-piece.y)<1e-7&&Length(pieces.back().z-piece.z)<1e-7)pieces.back().b=piece.b;else pieces.push_back(piece);};
    for(const auto& span:source.spans){
        if(!span.radius){if(segmented)append({span.a,span.b,span.frameY,span.frameZ});else{double length=Length(span.b-span.a);p.segments.push_back({span.a-span.frameY*(d/2)-span.frameZ*(w/2),span.Tangent(0),p.length,length,-1,-1,span.frameY,span.frameZ});p.length+=length;}continue;}
        hasArc=true;Vec inside=Cross(span.normal,span.Tangent(0));double cy=Dot(inside,span.frameY),cz=Dot(inside,span.frameZ);
        if(std::max(std::abs(cy),std::abs(cz))<1-1e-7)throw std::runtime_error("空间弯曲方向须与方通截面的一对管壁对应；斜向弯曲或扭管暂不支持。");
        double depth=std::abs(cz)>.9?w:d;if(span.angle<=0||span.angle>pi+1e-7||span.radius<=depth/2)throw std::runtime_error("空间管单段圆弧须不超过 180° 且内侧半径为正。");
        if(!segmented){double r=span.radius+(settings.tubeKFactor-.5)*depth,length=r*span.angle;p.machineArcs.push_back({span,p.length,length,r});p.length+=length;continue;}
        if(p.radius+t>=depth-t)throw std::runtime_error("连接半径超过该方向的管壁间距。");
        std::vector<Vec> points{span.a};std::vector<double> stations{0};double step=span.angle/settings.divisions;
        for(int j=0;j<settings.divisions;++j){double a=j==0?0:(j+phase)*step,b=j+1==settings.divisions?span.angle:(j+1+phase)*step;points.push_back(span.center+(span.Point((a+b)/(2*span.angle))-span.center)*(1/cos((b-a)/2)));stations.push_back(b/span.angle);p.errorMm=std::max(p.errorMm,(span.radius+depth/2)/u*(1/cos((b-a)/2)-1));}points.push_back(span.b);
        for(size_t j=0;j+1<points.size();++j)append({points[j],points[j+1],SectionY(source,span,stations[j]),SectionZ(source,span,stations[j])});
    }
    if(!hasArc)throw std::runtime_error("空间管件没有可伸直的圆弧。");if(!segmented)return p;
    std::vector<double> setbacks(pieces.size()+1,0),allowances(pieces.size()+1,0);std::vector<Bend> joints(pieces.size()+1);
    for(size_t i=1;i<pieces.size();++i){const auto& before=pieces[i-1];const auto& after=pieces[i];Vec a=Unit(before.b-before.a),b=Unit(after.b-after.a),normal=Unit(Cross(a,b)),inside=Cross(normal,a);double angle=atan2(Length(Cross(a,b)),Dot(a,b));if(angle>pi*.75)throw std::runtime_error("分段转角过大，请增加切口数。");
        double cy=Dot(inside,before.y),cz=Dot(inside,before.z);if(std::max(std::abs(cy),std::abs(cz))<1-1e-7)throw std::runtime_error("空间管件存在斜向弯曲或截面扭转。");bool across=std::abs(cz)>.9,reversed=(across?cz:cy)<0;double depth=across?w:d;
        setbacks[i]=(p.radius+t-depth/2)*tan(angle/2);allowances[i]=(p.radius+settings.kFactor*t)*angle;
        joints[i]={angle,0,allowances[i],setbacks[i],before.b,a,b,reversed,across,normal,across?before.y:before.z};
    }
    for(size_t i=0;i<pieces.size();++i){const auto& q=pieces[i];Vec axis=Unit(q.b-q.a);double length=Length(q.b-q.a)-setbacks[i]-setbacks[i+1];
        auto cut=[&](size_t index){const auto& b=joints[index];return b.angle?std::max(0.,BendDepth(p,b)-p.radius-t)*tan(b.angle/2)+p.gap/2:0.;};if(length-cut(i)-cut(i+1)<=.01*u)throw std::runtime_error("空间管切口重叠或越过端部，请减小间隙或切口数。");
        p.segments.push_back({q.a+axis*setbacks[i]-q.y*(d/2)-q.z*(w/2),axis,p.length,length,i?int(i)-1:-1,i+1<pieces.size()?int(i):-1,q.y,q.z});p.length+=length;
        if(i+1<pieces.size()){auto bend=joints[i+1];bend.start=p.length;p.bends.push_back(bend);p.length+=bend.allowance;}
    }return p;
}
static Plan BuildPlan(const Source& source,const Settings& settings,double phase){
    auto p=BasePlan(source,settings,true);double u=source.unitsPerMm,t=source.thickness,d=source.depth;
    if(settings.divisions<2||settings.divisions>180)throw std::runtime_error("圆弧分段数须为 2–180。");
    auto append=[&](Vec v){if(p.polygon.empty()||Length(v-p.polygon.back())>1e-6*u)p.polygon.push_back(v);};
    double totalAngle=0;
    for(const auto& s:source.spans){
        append(s.a);
        if(s.radius){
            double outer=s.radius+(Dot(s.normal,source.normal)<0?d:0);
            if(outer<=d||s.angle<=0||s.angle>pi+1e-7)throw std::runtime_error("圆弧须小于等于 180°，且外侧半径须大于方通高度。");
            double step=s.angle/settings.divisions;
            // Circumscribed tangent polygon keeps both end positions and tangents.
            for(int i=0;i<settings.divisions;++i){
                double a=i==0?0:(i+phase)*step,b=i+1==settings.divisions?s.angle:(i+1+phase)*step;
                append(s.center+(s.Point((a+b)/(2*s.angle))-s.center)*(1/std::cos((b-a)/2)));
                p.errorMm=std::max(p.errorMm,outer/u*(1/std::cos((b-a)/2)-1));
            }
        }
        append(s.b);
    }
    // Tangent joins and split straight edges do not introduce artificial bends.
    for(size_t i=1;i+1<p.polygon.size();){
        auto a=Unit(p.polygon[i]-p.polygon[i-1]),b=Unit(p.polygon[i+1]-p.polygon[i]);
        if(Length(a-b)<1e-7)p.polygon.erase(p.polygon.begin()+i);else ++i;
    }
    if(p.polygon.size()<3)throw std::runtime_error("选择中没有圆弧或转角，无需伸直。");
    std::vector<double> setbacks(p.polygon.size(),0),allowances(p.polygon.size(),0),angles(p.polygon.size(),0);
    for(size_t i=1;i+1<p.polygon.size();++i){
        auto a=Unit(p.polygon[i]-p.polygon[i-1]),b=Unit(p.polygon[i+1]-p.polygon[i]);
        double angle=std::atan2(Dot(Cross(a,b),source.normal),Dot(a,b));
        if(std::abs(angle)<=1e-8||std::abs(angle)>pi*.75)throw std::runtime_error("单个转角须不超过 135°。");
        totalAngle+=std::abs(angle);angles[i]=angle;setbacks[i]=(p.radius+t-(angle<0?d:0))*std::tan(std::abs(angle)/2);allowances[i]=(p.radius+settings.kFactor*t)*std::abs(angle);
    }
    if(source.round&&totalAngle>pi+1e-6)throw std::runtime_error("圆管当前支持总转向不超过 180°。");
    for(size_t i=0;i+1<p.polygon.size();++i){
        double straight=Length(p.polygon[i+1]-p.polygon[i])-setbacks[i]-setbacks[i+1];
        // At the opposite wall adjacent mitres must leave positive material.
        double cuts=(i?std::max(0.,d-p.radius-t)*std::tan(std::abs(angles[i])/2)+p.gap/2:0)+(i+2<p.polygon.size()?std::max(0.,d-p.radius-t)*std::tan(std::abs(angles[i+1])/2)+p.gap/2:0);
        if(straight-cuts<=.01*u)throw std::runtime_error("切口重叠或切到端部；请增大段长、减小转角/切缝，或检查外侧边选择。");
        p.segments.push_back({p.polygon[i]+Unit(p.polygon[i+1]-p.polygon[i])*setbacks[i],Unit(p.polygon[i+1]-p.polygon[i]),p.length,straight,i?int(i)-1:-1,i+2<p.polygon.size()?int(i):-1});
        p.length+=straight;
        if(i+2<p.polygon.size()){
            p.bends.push_back({std::abs(angles[i+1]),p.length,allowances[i+1],setbacks[i+1],p.polygon[i+1],Unit(p.polygon[i+1]-p.polygon[i]),Unit(p.polygon[i+2]-p.polygon[i+1]),angles[i+1]<0});
            p.length+=allowances[i+1];
        }
    }
    return p;
}
static Plan BuildMachinePlan(const Source& input,const Settings& settings){
    Source source=input;source.spans.clear();
    for(const auto& span:input.spans){
        if(!source.spans.empty()&&!span.radius&&!source.spans.back().radius&&Length(source.spans.back().b-span.a)<1e-6*input.unitsPerMm&&Length(source.spans.back().Tangent(1)-span.Tangent(0))<1e-7)source.spans.back().b=span.b;
        else source.spans.push_back(span);
    }
    const size_t count=source.spans.size();double totalAngle=0;bool hasCuts=false,hasArcs=false;
    std::vector<double> angles(count+1,0);std::vector<int> bendIndex(count+1,-1);
    for(size_t i=0;i<count;++i){const auto& s=source.spans[i];
        if(s.radius){double outer=s.radius+(Dot(s.normal,source.normal)<0?source.depth:0);if(!std::isfinite(s.radius)||outer<=source.depth||!std::isfinite(s.angle)||s.angle<=0||s.angle>pi+1e-7)throw std::runtime_error("圆弧须小于等于 180°，且外侧半径须大于管高。");hasArcs=true;totalAngle+=s.angle;}
        if(i){Vec a=source.spans[i-1].Tangent(1),b=s.Tangent(0);double angle=std::atan2(Dot(Cross(a,b),source.normal),Dot(a,b));
            if(std::abs(angle)>pi*.75)throw std::runtime_error("单个转角须不超过 135°。");
            if(Length(a-b)>1e-7){if(s.radius||source.spans[i-1].radius)throw std::runtime_error("圆弧与相邻管段须相切。");angles[i]=angle;totalAngle+=std::abs(angle);hasCuts=true;}
        }
    }
    auto p=BasePlan(source,settings,hasCuts);double u=source.unitsPerMm,t=source.thickness,d=source.depth;
    if(!hasCuts&&!hasArcs)throw std::runtime_error("选择中没有圆弧或转角，无需伸直。");
    if(source.round&&totalAngle>pi+1e-6)throw std::runtime_error("圆管当前支持总转向不超过 180°。");
    if(hasArcs&&(!std::isfinite(settings.tubeKFactor)||settings.tubeKFactor<=0||settings.tubeKFactor>1))throw std::runtime_error("弯管 K 因子须大于 0 且不大于 1；0.5 表示按截面中心线展开。");
    std::vector<double> setbacks(count+1,0),allowances(count+1,0);int nextBend=0;
    for(size_t i=1;i<count;++i)if(angles[i]!=0){setbacks[i]=(p.radius+t-(angles[i]<0?d:0))*std::tan(std::abs(angles[i])/2);allowances[i]=(p.radius+settings.kFactor*t)*std::abs(angles[i]);bendIndex[i]=nextBend++;}
    for(size_t i=0;i<count;++i){const auto& s=source.spans[i];
        if(s.radius){double outer=s.radius+(Dot(s.normal,source.normal)<0?d:0),r=outer-(1-settings.tubeKFactor)*d,length=r*s.angle;p.machineArcs.push_back({s,p.length,length,r});p.length+=length;}
        else{
            double length=Length(s.b-s.a)-setbacks[i]-setbacks[i+1];
            double cuts=(angles[i]!=0?std::max(0.,d-p.radius-t)*std::tan(std::abs(angles[i])/2)+p.gap/2:0)+(angles[i+1]!=0?std::max(0.,d-p.radius-t)*std::tan(std::abs(angles[i+1])/2)+p.gap/2:0);
            if(length-cuts<=.01*u)throw std::runtime_error("切口重叠或切到圆弧/端部；请检查转角附近的直段长度。");
            p.segments.push_back({s.a+s.Tangent(0)*setbacks[i],s.Tangent(0),p.length,length,bendIndex[i],bendIndex[i+1]});p.length+=length;
        }
        if(angles[i+1]!=0){p.bends.push_back({std::abs(angles[i+1]),p.length,allowances[i+1],setbacks[i+1],s.b,s.Tangent(1),source.spans[i+1].Tangent(0),angles[i+1]<0});p.length+=allowances[i+1];}
    }
    return p;
}
// Exact support interval of line/circular profiles (including complete circles),
// extruded through one wall. Used for cut clearance; no sampled hole bounds.
std::pair<double,double> ProjectRange(const Hole& h,Vec direction){
    double lo=1e100,hi=-1e100;
    auto add=[&](Vec q){double v=Dot(q,direction);lo=std::min(lo,v);hi=std::max(hi,v);};
    for(const auto& c:h.profile){add(c.a);add(c.b);if(c.radius){
        Vec x=c.a-c.center,y=Cross(c.normal,x);double a=std::atan2(Dot(y,direction),Dot(x,direction));
        for(int k=-3;k<=5;++k){double t=a+k*pi;if(t>0&&t<c.angle)add(c.Point(t/c.angle));}
    }}
    double delta=Dot(h.direction,direction)*h.length;return {lo+std::min(0.,delta),hi+std::max(0.,delta)};
}
Vec ToFlatLocal(const Plan& p,size_t i,Vec point){const auto& s=p.segments.at(i);Vec q=point-s.origin;return {s.start+Dot(q,s.axis),Dot(q,SegmentY(p,s)),Dot(q,SegmentZ(p,s))};}
Vec ToFolded(const Plan& p,size_t i,Vec q){const auto& s=p.segments.at(i);return s.origin+s.axis*(q.x-s.start)+SegmentY(p,s)*q.y+SegmentZ(p,s)*q.z;}
static Hole MapHole(const Plan& p,const Hole& h,size_t i){
    Hole result=h;const auto& seg=p.segments[i];
    auto vector=[&](Vec v){return Vec{Dot(v,seg.axis),Dot(v,SegmentY(p,seg)),Dot(v,SegmentZ(p,seg))};};
    result.direction=vector(h.direction);
    // The local frame can be left handed when the selected face is reversed.
    double hand=Dot(Cross(seg.axis,SegmentY(p,seg)),SegmentZ(p,seg));
    for(auto& c:result.profile){c.a=ToFlatLocal(p,i,c.a);c.b=ToFlatLocal(p,i,c.b);c.center=ToFlatLocal(p,i,c.center);c.normal=vector(c.normal)*hand;}
    return result;
}
Hole FlatHole(const Plan& p,size_t h){return MapHole(p,p.source.holes.at(h),p.holeSegments.at(h));}
static bool AssignHoles(Plan& p){
    const double clearance=.02*p.source.unitsPerMm;
    for(const auto& original:p.source.holes){bool found=false;
        for(size_t i=0;i<p.segments.size();++i){auto h=MapHole(p,original,i);const auto& seg=p.segments[i];
            auto x=ProjectRange(h,{1,0,0}),y=ProjectRange(h,{0,1,0}),z=ProjectRange(h,{0,0,1});
            if(x.first<seg.start+clearance||x.second>seg.start+seg.length-clearance||y.first< -clearance||y.second>p.source.depth+clearance||z.first< -clearance||z.second>p.source.width+clearance)continue;
            // Keep profiles wholly on the original flat wall, clear of section radii.
            if(std::abs(h.direction.z)>.99&&(y.first<p.source.cornerRadius+clearance||y.second>p.source.depth-p.source.cornerRadius-clearance))continue;
            if(std::abs(h.direction.y)>.99&&(z.first<p.source.cornerRadius+clearance||z.second>p.source.width-p.source.cornerRadius-clearance))continue;
            double root=p.source.thickness+p.radius;
            if(seg.beforeBend>=0){const auto& bend=p.bends[seg.beforeBend];double slope=std::tan(bend.angle/2),sign=bend.reversed?-1.:1.,shift=bend.reversed?BendDepth(p,bend):0;Vec projection=bend.acrossZ?Vec{1,0,-sign*slope}:Vec{1,-sign*slope,0};if(x.first<seg.start+p.gap/2+clearance||ProjectRange(h,projection).first<seg.start+slope*(shift-root)+p.gap/2+clearance)continue;}
            if(seg.afterBend>=0){const auto& bend=p.bends[seg.afterBend];double slope=std::tan(bend.angle/2),sign=bend.reversed?-1.:1.,shift=bend.reversed?BendDepth(p,bend):0;Vec projection=bend.acrossZ?Vec{1,0,sign*slope}:Vec{1,sign*slope,0};if(x.second>seg.start+seg.length-p.gap/2-clearance||ProjectRange(h,projection).second>seg.start+seg.length+slope*(root-shift)-p.gap/2-clearance)continue;}
            p.holeSegments.push_back(i);found=true;break;
        }
        if(!found)return false;
    }
    return true;
}
Vec SourceSlotNormal(const Plan& p,const SourceSlot& s){return Length(s.normal)>.9?s.normal:p.source.normal*(s.reversed?-1.:1.);}
Vec SourceSlotWidth(const Plan& p,const SourceSlot& s){return Length(s.widthDirection)>.9?s.widthDirection:p.source.widthDirection;}
Vec SourceSlotPoint(const Plan& p,const SourceSlot& s,double x,double y,double z){return s.origin+s.axis*x+Cross(SourceSlotNormal(p,s),s.axis)*y+SourceSlotWidth(p,s)*z;}
double SourceSlotTop(const Plan& p,const SourceSlot& s,double x){
    if(s.pathRadius){double r=s.pathRadius-p.source.thickness;return s.pathRadius-std::sqrt(r*r-x*x);}
    return (p.source.thickness+std::abs(x)*std::sqrt(std::max(0.,1-s.cornerCos*s.cornerCos)))/s.cornerCos;
}
static bool AddSourceSlots(Plan& p){
    if(!p.settings.cutSource)return true;
    double u=p.source.unitsPerMm,h=p.gap/2,d=p.source.depth;
    if(p.gap<.01*u)throw std::runtime_error("原管开槽时，切口间隙须至少为 0.01 mm。");
    for(size_t i=0;i<p.bends.size();++i){
        const auto& b=p.bends[i];SourceSlot slot;slot.incoming=b.incoming;slot.outgoing=b.outgoing;slot.reversed=b.reversed;
        if(p.source.spatial){
            slot.normal=b.normal;slot.widthDirection=b.widthDirection;slot.depth=BendDepth(p,b);slot.width=b.acrossZ?p.source.depth:p.source.width;double best=1e100,remaining=0;
            for(const auto& arc:p.source.spans)if(arc.radius&&Dot(arc.normal,b.normal)>1-1e-7){Vec radial=b.vertex-arc.center;radial=radial-arc.normal*Dot(radial,arc.normal);if(Length(radial)<1e-9*u)continue;double angle=atan2(Dot(Cross(arc.a-arc.center,radial),arc.normal),Dot(arc.a-arc.center,radial));if(angle<=1e-9||angle>=arc.angle-1e-9)continue;double distance=Length(arc.center+Unit(radial)*arc.radius-b.vertex);if(distance<best){best=distance;slot.pathRadius=arc.radius+slot.depth/2;slot.center=arc.center-slot.widthDirection*(slot.width/2);slot.origin=slot.center+Unit(radial)*slot.pathRadius;slot.axis=Unit(Cross(arc.normal,radial));remaining=std::min(angle,arc.angle-angle);}}
            if(!slot.pathRadius||best>p.errorMm*u+1e-4*u)throw std::runtime_error("未能对应空间圆弧切口。");if(h+.01*u>=(slot.pathRadius-slot.depth)*sin(std::min(remaining,pi/2)))throw std::runtime_error("空间管间隙槽越过圆弧端部。");
        }else{
        bool corner=false;for(size_t j=1;j<p.source.spans.size();++j)if(Length(b.vertex-p.source.spans[j].a)<1e-5*u&&Length(p.source.spans[j-1].Tangent(1)-p.source.spans[j].Tangent(0))>1e-7){corner=true;break;}
        if(corner){slot.origin=b.vertex;if(b.reversed)slot.origin=slot.origin+(Cross(p.source.normal,b.incoming)+Cross(p.source.normal,b.outgoing))*(d/(1+Dot(b.incoming,b.outgoing)));slot.axis=Unit(slot.incoming+slot.outgoing);slot.cornerCos=std::cos(b.angle/2);}
        else{
            double best=1e100,remaining=0;
            for(const auto& arc:p.source.spans)if(arc.radius&&(Dot(arc.normal,p.source.normal)<0)==b.reversed){
                Vec radial=b.vertex-arc.center;radial=radial-p.source.normal*Dot(radial,p.source.normal);if(Length(radial)<1e-9*u)continue;
                double angle=std::atan2(Dot(Cross(arc.a-arc.center,radial),arc.normal),Dot(arc.a-arc.center,radial));
                if(angle<=1e-9||angle>=arc.angle-1e-9)continue;Vec q=arc.center+Unit(radial)*arc.radius;double distance=Length(q-b.vertex);
                if(distance<best){best=distance;slot.pathRadius=arc.radius+(b.reversed?d:0);slot.origin=arc.center+Unit(radial)*slot.pathRadius;slot.center=arc.center;slot.axis=Unit(Cross(arc.normal,radial));remaining=std::min(angle,arc.angle-angle);}
            }
            if(!slot.pathRadius||best>p.errorMm*u+1e-4*u)throw std::runtime_error("未能将切口对应到原管的圆弧位置。");
            if(h+.01*u>=(slot.pathRadius-d)*std::sin(std::min(remaining,pi/2)))throw std::runtime_error("原管间隙槽越过圆弧端部；请减小间隙或切口数。");
        }
        }
        // Conservative exact hole bounds: never trim a hole to accommodate a slot.
        Vec inside=Cross(SourceSlotNormal(p,slot),slot.axis);bool conflict=false;
        for(const auto& hole:p.source.holes){
            auto width=SourceSlotWidth(p,slot);auto x=ProjectRange(hole,slot.axis),y=ProjectRange(hole,inside),z=ProjectRange(hole,width);
            double ox=Dot(slot.origin,slot.axis),oy=Dot(slot.origin,inside),oz=Dot(slot.origin,width),c=.02*u,depth=slot.depth?slot.depth:d,w=slot.width?slot.width:p.source.width;
            if(x.second>=ox-h-c&&x.first<=ox+h+c&&y.second>=oy-c&&y.first<=oy+depth/slot.cornerCos+u+c&&z.second>=oz-u-c&&z.first<=oz+w+u+c){conflict=true;break;}
        }
        if(conflict)return false;p.sourceSlots.push_back(slot);
    }
    return true;
}
Vec RoundEndNormal(const Plan& p,bool end){
    const auto& s=p.source;Vec normal=end?s.endCutNormal:s.startCutNormal;
    if(Length(normal)<1e-9)return {1,0,0};Vec axis=end?s.spans.back().Tangent(1):s.spans.front().Tangent(0);
    return {Dot(normal,axis),Dot(normal,Cross(s.normal,axis)),Dot(normal,s.widthDirection)};
}
double RoundEndX(const Plan& p,bool end,double y,double z){
    Vec n=RoundEndNormal(p,end);double r=p.source.depth/2;return (end?p.length:0)-(n.y*(y-r)+n.z*(z-r))/n.x;
}
double RoundEndExtent(const Plan& p,bool end){Vec n=RoundEndNormal(p,end);return p.source.depth/2*std::hypot(n.y,n.z)/std::abs(n.x);}
static void ValidateEnds(const Plan& p){
    if(!p.source.round)return;
    for(bool end:{false,true}){double extent=RoundEndExtent(p,end);if(extent<1e-8)continue;
        if(p.segments.empty())throw std::runtime_error("斜端口须位于平直管段。");const auto& seg=end?p.segments.back():p.segments.front();int index=end?seg.beforeBend:seg.afterBend;
        double cut=index<0?0:p.gap/2+std::max(0.,p.source.depth-p.source.thickness-p.radius)*std::tan(p.bends[index].angle/2);
        if(extent+cut+.01*p.source.unitsPerMm>=seg.length)throw std::runtime_error("斜端口与弯曲区或切口相交，请加长端部直段或调整切口参数。");
    }
}
static Plan PlaceAtSelection(Plan p){
    if(!p.settings.useAnchor||p.source.round)return p;
    const auto pick=p.settings.anchorPoint;for(double value:{pick.x,pick.y,pick.z})if(!std::isfinite(value))throw std::runtime_error("固定位置无效，请重新选面。");
    const auto& s=p.source;double best=1e100,fraction=0;size_t index=0;Vec center;
    for(size_t i=0;i<s.spans.size();++i){const auto& span=s.spans[i];std::vector<double> fractions{0,1};
        if(span.radius){Vec radial=pick-span.center;if(!s.spatial)radial=radial-s.widthDirection*(s.width/2);radial=radial-span.normal*Dot(radial,span.normal);double angle=std::atan2(Dot(Cross(span.a-span.center,radial),span.normal),Dot(span.a-span.center,radial));if(angle>=0&&angle<=span.angle)fractions.push_back(angle/span.angle);}
        else fractions.push_back(std::clamp(Dot(pick-span.a,span.Tangent(0))/Length(span.b-span.a),0.,1.));
        for(double f:fractions){Vec q=span.Point(f);if(!s.spatial)q=q+Cross(s.normal,span.Tangent(f))*(s.depth/2)+s.widthDirection*(s.width/2);double distance=Length(q-pick);if(distance<best){best=distance;fraction=f;index=i;center=q;}}
    }
    const auto& span=s.spans[index];double station=0;bool found=false;
    if(span.radius&&!p.settings.segmentArcs)for(const auto& arc:p.machineArcs)if(Length(arc.source.a-span.a)<1e-6*s.unitsPerMm){station=arc.start+arc.length*fraction;found=true;break;}
    if(!found){best=1e100;for(size_t i=0;i<p.segments.size();++i){const auto& seg=p.segments[i];double x=std::clamp(ToFlatLocal(p,i,center).x,seg.start,seg.start+seg.length);double distance=Length(ToFolded(p,i,{x,s.depth/2,s.width/2})-center);if(distance<best){best=distance;station=x;found=true;}}}
    if(!found)throw std::runtime_error("未能确定所选部位的固定截面。");
    p.flatAxis=span.Tangent(fraction);p.flatInside=SectionY(s,span,fraction);p.flatWidth=SectionZ(s,span,fraction);p.flatOrigin=center-p.flatAxis*station-p.flatInside*(s.depth/2)-p.flatWidth*(s.width/2);return p;
}
Plan MakePlan(const Source& source,const Settings& settings){
    if(!settings.segmentArcs){auto p=source.spatial?BuildSpatialPlan(source,settings,0):BuildMachinePlan(source,settings);
        if(!AssignHoles(p))throw std::runtime_error("整体弯管模式仅保留直段内的完整孔槽；弯曲区或跨弯孔槽不能保证成型后孔形孔位，请改用圆弧多段伸直。");
        if(!AddSourceSlots(p))throw std::runtime_error("转角切口与原孔槽冲突，已取消生成。");ValidateEnds(p);return PlaceAtSelection(std::move(p));
    }
    // Keep the requested count and both end tangents. Shift interior tangent
    // stations together only when this avoids a hole without moving that hole.
    for(double phase:{0.,.1,-.1,.2,-.2,.3,-.3,.4,-.4,.48,-.48}){
        auto p=source.spatial?BuildSpatialPlan(source,settings,phase):BuildPlan(source,settings,phase);if(AssignHoles(p)&&AddSourceSlots(p)){p.adjustedCuts=phase!=0;ValidateEnds(p);return PlaceAtSelection(std::move(p));}
    }
    throw std::runtime_error("孔槽与切口、连接折弯区或截面圆角冲突，无法保持原孔位。请减小/调整切口数；程序不会移动或删掉原孔。");
}
std::vector<Vec> Notch(const Plan& p,const Bend& b,double extraDepth){
    double root=p.source.thickness+p.radius,top=p.source.round?-p.source.unitsPerMm:p.source.thickness,deep=BendDepth(p,b)+extraDepth;
    double slope=std::tan(b.angle/2),a=b.start-p.gap/2,c=b.start+b.allowance+p.gap/2;
    std::vector<Vec> v={{a,top,0},{c,top,0}};
    if(root>top+1e-9)v.push_back({c,root,0});
    v.push_back({c+(deep-root)*slope,deep,0});v.push_back({a-(deep-root)*slope,deep,0});
    if(root>top+1e-9)v.push_back({a,root,0});if(b.reversed)for(auto& q:v)q.y=BendDepth(p,b)-q.y;return v;
}
Vec FlatPoint(const Plan& p,double x,double y,double z){
    return p.flatOrigin+p.flatAxis*x+p.flatInside*y+p.flatWidth*z;
}
double BridgeHalfAngle(const Plan& p){return p.settings.bridgeWidthMm*p.source.unitsPerMm/p.source.depth;}
std::vector<std::pair<Vec,Vec>> Preview(const Plan& p){
    std::vector<std::pair<Vec,Vec>> lines;
    for(const auto& slot:p.sourceSlots){
        double h=p.gap/2,d=slot.depth?slot.depth:p.source.depth,w=slot.width?slot.width:p.source.width;
        auto at=[&](double x,double y,double z){return SourceSlotPoint(p,slot,x,y,z);};
        if(!p.source.round){
            for(double x:{-h,h}){double top=SourceSlotTop(p,slot,x),bottom=slot.pathRadius?slot.pathRadius-std::sqrt((slot.pathRadius-d)*(slot.pathRadius-d)-x*x):SourceSlotTop(p,slot,x)+(d-p.source.thickness)/slot.cornerCos;
                for(double z:{0.,w})lines.push_back({at(x,top,z),at(x,bottom,z)});lines.push_back({at(x,bottom,0),at(x,bottom,w)});
            }
            for(double z:{0.,w})for(int i=0;i<16;++i){double a=-h+2*h*i/16,b=-h+2*h*(i+1)/16;lines.push_back({at(a,SourceSlotTop(p,slot,a),z),at(b,SourceSlotTop(p,slot,b),z)});}
        }else{
            double r=d/2,alpha=BridgeHalfAngle(p);
            auto rim=[&](double x,double angle){double radial=r*std::cos(angle),y;
                if(slot.pathRadius){double rho=slot.pathRadius-r+radial;y=slot.pathRadius-std::sqrt(rho*rho-x*x);}
                else y=(r-radial+std::abs(x)*std::sqrt(1-slot.cornerCos*slot.cornerCos))/slot.cornerCos;
                return at(x,y,r+r*std::sin(angle));};
            for(double x:{-h,h})for(int i=0;i<64;++i)lines.push_back({rim(x,alpha+(2*pi-2*alpha)*i/64),rim(x,alpha+(2*pi-2*alpha)*(i+1)/64)});
            for(double a:{alpha,2*pi-alpha})lines.push_back({rim(-h,a),rim(h,a)});
        }
    }
    if(p.source.round){
        double r=p.source.depth/2,ri=r-p.source.thickness,alpha=BridgeHalfAngle(p);
        auto at=[&](double x,double radius,double angle){return FlatPoint(p,x,r-radius*std::cos(angle),r+radius*std::sin(angle));};
        auto endPoint=[&](bool end,double radius,double angle){double y=r-radius*cos(angle),z=r+radius*sin(angle);return FlatPoint(p,RoundEndX(p,end,y,z),y,z);};
        for(double radius:{r,ri})for(bool end:{false,true})for(int i=0;i<64;++i)lines.push_back({endPoint(end,radius,2*pi*i/64),endPoint(end,radius,2*pi*(i+1)/64)});
        for(int i=0;i<8;++i)lines.push_back({endPoint(false,r,2*pi*i/8),endPoint(true,r,2*pi*i/8)});
        for(const auto& b:p.bends)for(double radius:{r,ri}){
            auto x=[&](double angle,bool right){double y=r-radius*cos(angle),spread=std::max(0.,y-p.source.thickness-p.radius)*std::tan(b.angle/2);return right?b.start+b.allowance+p.gap/2+spread:b.start-p.gap/2-spread;};
            for(bool right:{false,true})for(int i=0;i<64;++i){double a=alpha+(2*pi-2*alpha)*i/64,c=alpha+(2*pi-2*alpha)*(i+1)/64;lines.push_back({at(x(a,right),radius,a),at(x(c,right),radius,c)});}
            for(double a:{alpha,2*pi-alpha})lines.push_back({at(x(a,false),radius,a),at(x(a,true),radius,a)});
        }
        return lines;
    }
    for(size_t i=1;i<p.polygon.size();++i)for(double z:{0.,p.source.width})lines.push_back({p.polygon[i-1]+p.source.widthDirection*z,p.polygon[i]+p.source.widthDirection*z});
    for(double y:{0.,p.source.depth})for(double z:{0.,p.source.width})lines.push_back({FlatPoint(p,0,y,z),FlatPoint(p,p.length,y,z)});
    for(double x:{0.,p.length})for(double z:{0.,p.source.width})lines.push_back({FlatPoint(p,x,0,z),FlatPoint(p,x,p.source.depth,z)});
    for(const auto& b:p.bends){auto cut=Notch(p,b);for(double z:{0.,b.acrossZ?p.source.depth:p.source.width})for(size_t i=0;i<cut.size();++i){auto a=cut[i],c=cut[(i+1)%cut.size()];lines.push_back({CutPoint(p,b,a.x,a.y,z),CutPoint(p,b,c.x,c.y,z)});}}
    for(size_t i=0;i<p.source.holes.size();++i){auto h=FlatHole(p,i);for(const auto& c:h.profile){int n=c.radius?48:1;for(int j=0;j<n;++j){auto a=c.Point(double(j)/n),b=c.Point(double(j+1)/n);lines.push_back({FlatPoint(p,a.x,a.y,a.z),FlatPoint(p,b.x,b.y,b.z)});}}}
    return lines;
}
}
