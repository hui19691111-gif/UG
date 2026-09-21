#include "ZeWanFuZuGeometry.hpp"
#include <NXOpen/Body.hxx>
#include <NXOpen/Edge.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/Session.hxx>
#include <NXOpen/Features_SheetMetal_SheetmetalManager.hxx>
#include <NXOpen/Features_FeatureCollection.hxx>
#include <NXOpen/Features_Extrude.hxx>
#include <NXOpen/Features_ExtrudeBuilder.hxx>
#include <NXOpen/Curve.hxx>
#include <NXOpen/CurveDumbRule.hxx>
#include <NXOpen/ScRuleFactory.hxx>
#include <NXOpen/Section.hxx>
#include <NXOpen/SectionCollection.hxx>
#include <NXOpen/Direction.hxx>
#include <NXOpen/DirectionCollection.hxx>
#include <NXOpen/Expression.hxx>
#include <NXOpen/GeometricUtilities_Limits.hxx>
#include <NXOpen/GeometricUtilities_Extend.hxx>
#include <NXOpen/GeometricUtilities_BooleanOperation.hxx>
#include <uf.h>
#include <uf_curve.h>
#include <uf_csys.h>
#include <uf_eval.h>
#include <uf_modl.h>
#include <uf_obj.h>
#include <uf_part.h>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>

namespace bend_assist {
namespace {
constexpr double angularTolerance=1e-6;
constexpr double pi=3.14159265358979323846;
Vec V(const NXOpen::Point3d& p) { return {p.X,p.Y,p.Z}; }
Vec V(const double* p) { return {p[0],p[1],p[2]}; }
struct Evaluator {
    UF_EVAL_p_t value=nullptr;
    explicit Evaluator(tag_t edge) { Check(UF_EVAL_initialize_2(edge,&value)); }
    ~Evaluator() { if(value) UF_EVAL_free(value); }
};
bool Arc(tag_t edge,UF_EVAL_arc_t& arc) {
    Evaluator eval(edge); logical isArc=false;
    Check(UF_EVAL_is_arc(eval.value,&isArc));
    if(isArc) Check(UF_EVAL_ask_arc(eval.value,&arc));
    return isArc;
}
Vec ArcPoint(const UF_EVAL_arc_t& arc,double t) {
    return V(arc.center)+(V(arc.x_axis)*std::cos(t)+V(arc.y_axis)*std::sin(t))*arc.radius;
}
// Critical parameters of an arc's projection, without tessellating geometry.
std::vector<double> Critical(const UF_EVAL_arc_t& arc,Vec direction) {
    std::vector<double> values={arc.limits[0],arc.limits[1]};
    double t=std::atan2(Dot(V(arc.y_axis),direction),Dot(V(arc.x_axis),direction));
    int begin=static_cast<int>(std::floor((arc.limits[0]-t)/pi));
    int end=static_cast<int>(std::ceil((arc.limits[1]-t)/pi));
    for(int k=begin;k<=end;++k) {
        double u=t+k*pi;
        if(u>arc.limits[0]+1e-10 && u<arc.limits[1]-1e-10) values.push_back(u);
    }
    return values;
}
Vec Outward(tag_t face,Vec point) {
    double uv[2],p[]={point.x,point.y,point.z},on[3],u1[3],v1[3],u2[3],v2[3],n[3],r[2];
    Check(UF_MODL_ask_face_parm(face,p,uv,on));
    Check(UF_MODL_ask_face_props(face,uv,on,u1,v1,u2,v2,n,r));
    return Unit(V(n));
}
bool OuterEdge(tag_t face,tag_t edge) {
    struct Loops { uf_loop_p_t value=nullptr; ~Loops(){if(value)UF_MODL_delete_loop_list(&value);} } loops;
    Check(UF_MODL_ask_face_loops(face,&loops.value));
    int count=0; Check(UF_MODL_ask_loop_list_count(loops.value,&count));
    for(int i=0;i<count;++i) {
        int type=0,n=0; uf_list_p_t edges=nullptr;
        Check(UF_MODL_ask_loop_list_item(loops.value,i,&type,&edges));
        Check(UF_MODL_ask_list_count(edges,&n));
        for(int j=0;j<n;++j) { tag_t item=0; Check(UF_MODL_ask_list_item(edges,j,&item)); if(item==edge) return type==1; }
    }
    return false;
}
struct Surface { int type=0; Vec point,dir; double radius=0; };
Surface ReadSurface(tag_t face) {
    Surface s; double p[3],d[3],box[6],r2; int sign;
    Check(UF_MODL_ask_face_data(face,&s.type,p,d,box,&s.radius,&r2,&sign));
    s.point={p[0],p[1],p[2]}; s.dir=Unit({d[0],d[1],d[2]});
    if(s.type==22) {
        // ask_face_props explicitly returns the outward SOLID face normal;
        // ask_face_data's direction/sign convention differs after booleans.
        double uv[2],onFace[3],u1[3],v1[3],u2[3],v2[3],outward[3],radii[2];
        Check(UF_MODL_ask_face_parm(face,p,uv,onFace));
        Check(UF_MODL_ask_face_props(face,uv,onFace,u1,v1,u2,v2,outward,radii));
        s.dir=Unit({outward[0],outward[1],outward[2]});
    }
    return s;
}
std::vector<Vec> Vertices(NXOpen::Face* face) {
    std::vector<Vec> result;
    for(auto* e:face->GetEdges()) {
        NXOpen::Point3d a,b; e->GetVertices(&a,&b);
        for(Vec p:{V(a),V(b)}) {
            bool exists=false;
            for(Vec q:result) if(Dot(p-q,p-q)<1e-14) exists=true;
            if(!exists) result.push_back(p);
        }
    }
    return result;
}
UF_EVAL_arc_t CylinderBoundary(NXOpen::Edge*,const Surface&,Vec,Vec,double);
double Highest(NXOpen::Face* face,Vec origin,Vec direction) {
    double top=-1e100;
    const auto skin=ReadSurface(face->Tag());double tol=0;Check(UF_MODL_ask_distance_tolerance_of_part(face->Tag(),&tol));
    for(auto* edge:face->GetEdges()) {
        UF_EVAL_arc_t arc={};
        bool circular=Arc(edge->Tag(),arc),linear=edge->SolidEdgeType()==NXOpen::Edge::EdgeTypeLinear;
        if(!circular && !linear) for(auto* adjacent:edge->GetFaces()) if(adjacent!=face) {
            auto other=ReadSurface(adjacent->Tag());
            if(other.type==16 && std::abs(Dot(other.dir,skin.dir))>1-angularTolerance) {
                arc=CylinderBoundary(edge,other,skin.point,skin.dir,tol);circular=true;break;
            }
            if(other.type==22 && std::abs(Dot(other.dir,skin.dir))<1-angularTolerance) linear=true;
        }
        if(circular) {
            for(double t:Critical(arc,direction)) top=std::max(top,Dot(ArcPoint(arc,t)-origin,direction));
        } else if(linear) {
            NXOpen::Point3d a,b; edge->GetVertices(&a,&b);
            top=std::max({top,Dot(V(a)-origin,direction),Dot(V(b)-origin,direction)});
        } else throw std::runtime_error("自动对齐目前要求折边轮廓由直线和圆弧组成。");
    }
    return top;
}
bool Straight(NXOpen::Edge* edge) { return edge->SolidEdgeType()==NXOpen::Edge::EdgeTypeLinear; }
UF_EVAL_arc_t CylinderBoundary(NXOpen::Edge* edge,const Surface& cylinder,Vec plane,Vec normal,double tol) {
    UF_EVAL_arc_t result={};
    if(Arc(edge->Tag(),result)) return result;
    // Boolean trims can expose a procedural/spline edge on an analytic
    // cylinder-plane intersection. Recover that exact circle from its faces.
    Vec center=cylinder.point+normal*Dot(plane-cylinder.point,normal);
    Evaluator eval(edge->Tag());double limits[2];Check(UF_EVAL_ask_limits(eval.value,limits));
    Vec x,y;double previous=0,total=0;
    for(int i=0;i<=32;++i) {
        double point[3];Check(UF_EVAL_evaluate(eval.value,0,limits[0]+(limits[1]-limits[0])*i/32,point,nullptr));
        Vec radial=V(point)-center;
        if(std::abs(Dot(radial,normal))>tol || std::abs(std::sqrt(Dot(radial,radial))-cylinder.radius)>tol)
            throw std::runtime_error("圆弧板厚面边界与圆柱/平面交线不一致。");
        if(i==0) {x=Unit(radial-normal*Dot(radial,normal));y=Cross(normal,x);}
        double angle=std::atan2(Dot(radial,y),Dot(radial,x));
        double delta=angle-previous;while(delta>pi)delta-=2*pi;while(delta<-pi)delta+=2*pi;
        if(i) total+=delta;
        previous=angle;
    }
    if(total<0) {y=y*(-1);total=-total;}
    result.radius=cylinder.radius;result.limits[0]=0;result.limits[1]=total;result.is_periodic=total>2*pi-angularTolerance;
    double c[]={center.x,center.y,center.z},a[]={x.x,x.y,x.z},b[]={y.x,y.y,y.z};
    std::copy(c,c+3,result.center);std::copy(a,a+3,result.x_axis);std::copy(b,b+3,result.y_axis);
    return result;
}
bool Generator(NXOpen::Edge* edge,Vec normal,double tol) {
    if(Straight(edge)) return true;
    Evaluator eval(edge->Tag());double limits[2];Check(UF_EVAL_ask_limits(eval.value,limits));
    Vec origin;
    for(int i=0;i<=8;++i) {
        double p[3];Check(UF_EVAL_evaluate(eval.value,0,limits[0]+(limits[1]-limits[0])*i/8,p,nullptr));
        if(i==0) origin=V(p);
        Vec d=V(p)-origin;d=d-normal*Dot(d,normal);
        if(Dot(d,d)>tol*tol) return false;
    }
    return true;
}
int Containment(tag_t body,Vec p) {
    double a[3]={p.x,p.y,p.z}; int status=0;
    Check(UF_MODL_ask_point_containment(a,body,&status)); return status;
}
std::string Number(double v) { std::ostringstream s; s.imbue(std::locale::classic()); s<<std::setprecision(16)<<v; return s.str(); }
struct List {
    uf_list_p_t value=nullptr;
    ~List() { if(value) UF_MODL_delete_list(&value); }
};
}
double Dot(Vec a,Vec b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
Vec Cross(Vec a,Vec b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
Vec Unit(Vec a) { double n=std::sqrt(Dot(a,a)); if(n<1e-12) throw std::runtime_error("无法确定辅助板方向。"); return a*(1/n); }
double BoundaryHeight(const FaceInfo& f,double x) {
    if(!f.circular) return f.slope*x;
    double q=f.radius*f.radius-(x-f.centerX)*(x-f.centerX);
    if(q < -1e-8*f.radius*f.radius) throw std::runtime_error("辅助板位置超出圆弧范围。");
    return f.centerY+f.branch*std::sqrt(std::max(0.0,q));
}
double GapHeight(const FaceInfo& f,double x,double gap) {
    if(!f.circular) return f.slope*x+gap*std::sqrt(1+f.slope*f.slope);
    double radius=f.radius+f.branch*gap;
    if(radius<=1e-4*f.unitsPerMm) throw std::runtime_error("凹圆弧的间隙必须小于圆弧半径。");
    double q=radius*radius-(x-f.centerX)*(x-f.centerX);
    if(q < -1e-8*radius*radius)
        throw std::runtime_error("等距间隙圆弧超出可用范围，请增大距端点、减小宽度或减小间隙。");
    return f.centerY+f.branch*std::sqrt(std::max(0.0,q));
}
void Check(int code) {
    if(!code) return;
    char error[133]={}; UF_get_fail_message(code,error);
    throw std::runtime_error("NX("+std::to_string(code)+"): "+error);
}

FaceInfo Inspect(tag_t tag,tag_t preferredBendEdge) {
    auto* face=dynamic_cast<NXOpen::Face*>(NXOpen::NXObjectManager::Get(tag));
    auto* part=NXOpen::Session::GetSession()->Parts()->Work();
    if(!face || !part || face->IsOccurrence() || face->OwningPart()!=part)
        throw std::runtime_error("请选择当前工作零件中的板厚面。");
    auto* body=face->GetBody();
    if(!body || !body->IsSolidBody()) throw std::runtime_error("请选择实体钣金的板厚面。");
    const auto side=ReadSurface(tag);
    const bool circular=side.type==16;
    if(side.type!=22 && !circular) throw std::runtime_error("请选择直线或圆弧外轮廓上的板厚面。");
    auto vertices=Vertices(face);
    if(vertices.size()<4) throw std::runtime_error("板厚面边界不完整。");
    for(auto* edge:face->GetEdges()) if(!circular && !Straight(edge))
        throw std::runtime_error("板厚面包含曲线，请选择直线板厚面。");

    // Find the two parallel skin faces, both perpendicular to the side face.
    std::vector<NXOpen::Face*> neighbors;
    std::set<tag_t> seen;
    for(auto* edge:face->GetEdges()) for(auto* adjacent:edge->GetFaces()) {
        if(adjacent==face || !seen.insert(adjacent->Tag()).second) continue;
        auto s=ReadSurface(adjacent->Tag());
        if(s.type==22 && (circular ? std::abs(Dot(s.dir,side.dir))>1-angularTolerance :
            std::abs(Dot(s.dir,side.dir))<angularTolerance)) neighbors.push_back(adjacent);
    }
    NXOpen::Face* skin=nullptr; Vec normal;
    double thickness=std::numeric_limits<double>::max();
    for(size_t i=0;i<neighbors.size();++i) for(size_t j=i+1;j<neighbors.size();++j) {
        auto a=ReadSurface(neighbors[i]->Tag()), b=ReadSurface(neighbors[j]->Tag());
        if(Dot(a.dir,b.dir)>-1+angularTolerance) continue;
        double t=std::abs(Dot(b.point-a.point,a.dir));
        if(t>1e-6 && t<thickness) { thickness=t; skin=neighbors[i]; normal=a.dir*(-1); }
    }
    if(!skin) throw std::runtime_error("未找到板厚面两侧平行的钣金大面。");
    int units=0; Check(UF_PART_ask_units(part->Tag(),&units));
    FaceInfo info; info.unitsPerMm=units==UF_PART_ENGLISH ? 1.0/25.4 : 1.0;
    const double tol=1e-4*info.unitsPerMm;
    auto skinSurface=ReadSurface(skin->Tag());
    double low=1e100,high=-1e100;
    UF_EVAL_arc_t boundary={}; tag_t boundaryEdge=0; int arcs=0;
    if(circular) {
        double modelTolerance=0;Check(UF_MODL_ask_distance_tolerance_of_part(tag,&modelTolerance));
        for(auto* edge:face->GetEdges()) {
            UF_EVAL_arc_t arc={};
            bool skinBoundary=false;
            for(auto* adjacent:edge->GetFaces()) if(adjacent!=face) {
                auto adjacentSurface=ReadSurface(adjacent->Tag());
                if(adjacentSurface.type==22 && std::abs(Dot(adjacentSurface.dir,normal))>1-angularTolerance) {
                    arc=CylinderBoundary(edge,side,adjacentSurface.point,normal,std::max(tol,modelTolerance));skinBoundary=true;break;
                }
            }
            if(skinBoundary) {
                ++arcs;
                if(arc.is_periodic || arc.limits[1]-arc.limits[0]>pi+angularTolerance)
                    throw std::runtime_error("请选择不回绕的外轮廓圆弧段，不支持整圆孔壁。");
                if(std::abs(Dot(V(arc.center)-skinSurface.point,normal))<tol) { boundary=arc; boundaryEdge=edge->Tag(); }
                if(std::abs(arc.radius-side.radius)>tol || std::abs(Dot(Unit(Cross(V(arc.x_axis),V(arc.y_axis))),normal))<1-angularTolerance)
                    throw std::runtime_error("圆弧板厚面两侧轮廓不一致。");
            } else if(!Generator(edge,normal,std::max(tol,modelTolerance))) throw std::runtime_error("圆弧板厚面包含非等厚方向的裁剪边，请先分割。");
        }
        if(arcs!=2 || !boundaryEdge || face->GetEdges().size()!=4)
            throw std::runtime_error("请选择两侧为同半径圆弧的完整等厚板厚面。");
        if(!OuterEdge(skin->Tag(),boundaryEdge)) throw std::runtime_error("请选择外轮廓圆弧，不能选择内部孔壁。");
    }
    Vec along=circular ? Unit(ArcPoint(boundary,boundary.limits[1])-ArcPoint(boundary,boundary.limits[0])) : Unit(Cross(normal,side.dir));
    for(Vec p:vertices) {
        double h=Dot(p-skinSurface.point,normal);
        if(std::abs(h)>tol && std::abs(h-thickness)>tol)
            throw std::runtime_error("所选面不是等厚的板厚面。");
        low=std::min(low,Dot(p,along)); high=std::max(high,Dot(p,along));
    }
    if(high-low < thickness*1.5) throw std::runtime_error("请选择狭长的板厚面，不能选择钣金大面。");
    auto* sm=part->Features()->SheetmetalManager();
    if(sm->IsSheetmetalBody(body) && std::abs(sm->GetBodyThickness(body)-thickness)>tol*10)
        throw std::runtime_error("所选面宽度与钣金板厚不符。");
    // A bend is a cylindrical neighbor of the flange with its axis IN the
    // flange plane. Hole walls have axes normal to the flange and are excluded.
    struct Bend { tag_t edge; Vec axis; double length; };
    std::vector<Bend> bends;
    for(auto* edge:skin->GetEdges()) {
        if(!Straight(edge)) continue;
        NXOpen::Point3d a,b; edge->GetVertices(&a,&b);
        Vec delta=V(b)-V(a); double len=std::sqrt(Dot(delta,delta));
        if(len<tol) continue;
        for(auto* other:edge->GetFaces()) {
            if(other==skin) continue;
            auto s=ReadSurface(other->Tag());
            if(s.type!=16 || std::abs(Dot(s.dir,normal))>angularTolerance ||
               std::abs(Dot(s.dir,Unit(delta)))<1-angularTolerance) continue;
            // Require a second planar flange tangent to the same cylinder;
            // rounded free edges are not sheet-metal bends.
            bool secondFlange=false;
            for(auto* ce:other->GetEdges()) for(auto* cf:ce->GetFaces()) {
                if(cf==skin || cf==other) continue;
                auto cs=ReadSurface(cf->Tag());
                if(cs.type==22 && std::abs(Dot(cs.dir,normal))<1-angularTolerance &&
                   std::abs(Dot(cs.dir,s.dir))<angularTolerance) secondFlange=true;
            }
            if(secondFlange) bends.push_back({edge->Tag(),s.dir,len});
        }
    }
    if(bends.empty()) throw std::runtime_error("未找到与板厚面相邻折边的圆柱折弯面及折弯边。");
    auto chosen=std::max_element(bends.begin(),bends.end(),[](const Bend&a,const Bend&b){return a.length<b.length;});
    if(preferredBendEdge) {
        auto found=std::find_if(bends.begin(),bends.end(),[&](const Bend& b){return b.edge==preferredBendEdge;});
        if(found==bends.end()) throw std::runtime_error("参考边必须是该板厚面所在折边的折弯切线边。");
        chosen=found;
    } else {
        for(const auto& b:bends) if(std::abs(Dot(b.axis,chosen->axis))<1-angularTolerance)
            throw std::runtime_error("检测到多个不同方向的折弯，请选择需要对齐的折弯参考边。");
    }
    info.face=tag; info.body=body->Tag(); info.flange=skin->Tag(); info.bendEdge=chosen->edge;
    info.thickness=thickness; info.z=normal;
    // Line-plane parallel test, independent of WCS or model orientation.
    // A nonzero circular arc cannot be parallel along its entire length.
    // In particular, a parallel tangent at its crest does not skip the arc.
    info.circular=circular;
    info.parallel=!circular && std::abs(Dot(chosen->axis,side.dir))<=angularTolerance;
    if(info.parallel) return info;
    info.x=chosen->axis; info.y=Unit(Cross(normal,info.x));
    Vec outward=circular ? Outward(tag,ArcPoint(boundary,(boundary.limits[0]+boundary.limits[1])/2)+normal*(thickness/2)) : side.dir;
    if(Dot(info.y,outward)<0) info.y=info.y*(-1);
    std::vector<Vec> bottom;
    for(Vec p:vertices) if(std::abs(Dot(p-skinSurface.point,normal))<tol) bottom.push_back(p);
    if(circular) bottom={ArcPoint(boundary,boundary.limits[0]),ArcPoint(boundary,boundary.limits[1])};
    auto ends=std::minmax_element(bottom.begin(),bottom.end(),[&](Vec a,Vec b){return Dot(a,info.y)<Dot(b,info.y);});
    Vec start=*ends.first, end=*ends.second;
    if(Dot(end-start,info.x)<0) info.x=info.x*(-1);
    info.origin=start; info.length=Dot(end-start,info.x);
    if(info.length<tol) throw std::runtime_error("板厚面与折弯边接近垂直，无法形成有效的辅助定位板。");
    info.slope=Dot(end-start,info.y)/info.length;
    if(circular) {
        if(Critical(boundary,info.x).size()>2)
            throw std::runtime_error("圆弧沿折弯方向存在回绕，请先分割为不回绕的圆弧段。");
        info.radius=boundary.radius;
        info.centerX=Dot(V(boundary.center)-start,info.x);
        info.centerY=Dot(V(boundary.center)-start,info.y);
        double midY=Dot(ArcPoint(boundary,(boundary.limits[0]+boundary.limits[1])/2)-V(boundary.center),info.y);
        if(std::abs(midY)<tol) throw std::runtime_error("圆弧方向无法形成有效的辅助定位板。");
        info.branch=midY>0?1:-1;
    }
    info.autoTop=Highest(skin,start,info.y);
    // Both long boundaries must describe a straight rectangular thickness face.
    for(Vec p:vertices) if(!circular && std::abs(Dot(p-start,info.y)-info.slope*Dot(p-start,info.x))>tol)
        throw std::runtime_error("板厚面边界不是直线等厚边界。");
    return info;
}

std::vector<Plan> PlanFaces(const std::vector<FaceInfo>& faces,const Settings& settings) {
    for(double v:{settings.width,settings.bridgeWidth,settings.inset,settings.gap,settings.endDistance,settings.height})
        if(!std::isfinite(v)) throw std::runtime_error("参数必须是有限数值。");
    if(settings.width<=0 || settings.bridgeWidth<=0 || settings.gap<=0 || settings.height<=0 || settings.inset<0 || settings.endDistance<0)
        throw std::runtime_error("宽度、微连接宽度、间隙和高度必须大于零；缩进和距端点不能为负数。");
    if(settings.width<=2*(settings.inset+settings.bridgeWidth))
        throw std::runtime_error("辅助板宽度必须大于 2 ×（微连接缩进 + 微连接宽度）。");
    std::vector<Plan> plans;
    std::set<tag_t> seen;
    for(const auto& f:faces) {
        if(f.parallel || !seen.insert(f.face).second) continue;
        const double s=f.unitsPerMm, tol=1e-4*s;
        Plan p; p.face=f; p.gap=settings.gap*s; p.inset=settings.inset*s; p.bridgeWidth=settings.bridgeWidth*s;
        p.x0=settings.reverseEnd ? f.length-(settings.endDistance+settings.width)*s : settings.endDistance*s;
        p.x1=p.x0+settings.width*s;
        if(p.x0<-tol || p.x1>f.length+tol) throw std::runtime_error("辅助板宽度与距端点之和超出所选板厚面，请减小尺寸。");
        p.top=BoundaryHeight(f,p.x0)+settings.height*s;
        if(settings.autoAlign) {
            p.top=f.autoTop;
            // Align only coplanar, co-directed flanges of the SAME body.
            for(const auto& other:faces) if(!other.parallel && other.body==f.body &&
                Dot(f.y,other.y)>1-angularTolerance && std::abs(Dot(f.z,other.z))>1-angularTolerance &&
                std::abs(Dot(other.origin-f.origin,f.z))<tol)
                p.top=std::max(p.top,Dot(other.origin-f.origin,f.y)+other.autoTop);
        }
        double highest=std::max(GapHeight(f,p.x0,p.gap),GapHeight(f,p.x1,p.gap));
        if(f.circular && f.centerX>p.x0 && f.centerX<p.x1) highest=std::max(highest,GapHeight(f,f.centerX,p.gap));
        if(p.top<=highest+tol)
            throw std::runtime_error("辅助板定位边低于间隙边界。请减小宽度/距端点，或取消自动对齐并增加高度。");
        auto point=[&](double x,double y){return f.origin+f.x*x+f.y*y;};
        // Tiny overlap is wholly inside the source flange, avoiding a
        // tolerance-only tangent boolean. It does not change visible size.
        const double overlap=std::min(0.01*s,f.thickness*0.01);
        auto line=[&](Vec a,Vec b) {
            if(Dot(a-b,a-b)<tol*tol) return;
            p.segments.push_back({a,(a+b)*0.5,b,false}); p.outline.push_back(a);
        };
        auto boundary=[&](double a,double b,bool gapBoundary) {
            if(b-a<tol) return;
            auto at=[&](double x){return point(x,gapBoundary?GapHeight(f,x,p.gap):BoundaryHeight(f,x)-overlap);};
            // Visible gap arcs share the source circle's center. Only the
            // hidden bridge root is translated slightly into the source.
            Vec center=point(f.centerX,f.centerY+(gapBoundary?0:-overlap));
            double radius=f.radius+(gapBoundary?f.branch*p.gap:0);
            p.segments.push_back({at(a),at((a+b)/2),at(b),f.circular,center,radius});
            int steps=f.circular?32:1;
            for(int i=0;i<steps;++i) p.outline.push_back(at(a+(b-a)*i/steps));
        };
        double last=p.x0;
        for(double x:{p.x0+p.inset,p.x1-p.inset-p.bridgeWidth}) {
            boundary(last,x,true);
            line(point(x,GapHeight(f,x,p.gap)),point(x,BoundaryHeight(f,x)-overlap));
            boundary(x,x+p.bridgeWidth,false);
            line(point(x+p.bridgeWidth,BoundaryHeight(f,x+p.bridgeWidth)-overlap),point(x+p.bridgeWidth,GapHeight(f,x+p.bridgeWidth,p.gap)));
            last=x+p.bridgeWidth;
            for(int i=0;i<=8;++i) {
                double a=x+p.bridgeWidth*i/8;
                Vec q=point(a,BoundaryHeight(f,a)-overlap)+f.z*(f.thickness*0.5);
                if(Containment(f.body,q)!=1) throw std::runtime_error("微连接位置未落在原板实体内，请调整距端点或缩进。");
            }
        }
        boundary(last,p.x1,true);
        line(point(p.x1,GapHeight(f,p.x1,p.gap)),point(p.x1,p.top));
        line(point(p.x1,p.top),point(p.x0,p.top));
        line(point(p.x0,p.top),point(p.x0,GapHeight(f,p.x0,p.gap)));
        plans.push_back(p);
    }
    return plans;
}

static std::vector<tag_t> Profile(const Plan& plan) {
    std::vector<tag_t> curves;
    for(const auto& segment:plan.segments) {
        Vec a=segment.start,b=segment.end; tag_t curve=0;
        if(segment.circular) {
            // Preserve the planned exact circle; do not fit a circle through
            // three nearly collinear points at short bridge/gap segments.
            Vec x=plan.face.x,y=plan.face.y,z=Cross(x,y);
            double matrix[]={x.x,x.y,x.z,y.x,y.y,y.z,z.x,z.y,z.z}; tag_t matrixTag=0;
            Check(UF_CSYS_create_matrix(matrix,&matrixTag));
            UF_CURVE_arc_t arc={}; arc.matrix_tag=matrixTag; arc.radius=segment.radius;
            arc.arc_center[0]=Dot(segment.center,x);arc.arc_center[1]=Dot(segment.center,y);arc.arc_center[2]=Dot(segment.center,z);
            arc.start_angle=std::atan2(Dot(a-segment.center,y),Dot(a-segment.center,x));
            arc.end_angle=std::atan2(Dot(b-segment.center,y),Dot(b-segment.center,x));
            if(plan.face.branch>0) std::swap(arc.start_angle,arc.end_angle);
            while(arc.end_angle<arc.start_angle) arc.end_angle+=2*pi;
            Check(UF_CURVE_create_arc(&arc,&curve));
        } else {
            UF_CURVE_line_t line={{a.x,a.y,a.z},{b.x,b.y,b.z}};
            Check(UF_CURVE_create_line(&line,&curve));
        }
        curves.push_back(curve);
        Check(UF_OBJ_set_blank_status(curve,UF_OBJ_BLANKED));
    }
    return curves;
}

Construction CreateConstruction(const Plan& plan) {
    Construction result; result.curves=Profile(plan);
    auto* part=NXOpen::Session::GetSession()->Parts()->Work();
    auto* builder=part->Features()->CreateExtrudeBuilder(nullptr);
    tag_t feature=0,tool=0;
    try {
        auto* section=part->Sections()->CreateSection(1e-5*plan.face.unitsPerMm,1e-4*plan.face.unitsPerMm,0.5);
        std::vector<NXOpen::Curve*> objects;
        for(auto tag:result.curves) objects.push_back(dynamic_cast<NXOpen::Curve*>(NXOpen::NXObjectManager::Get(tag)));
        auto* rule=part->ScRuleFactory()->CreateRuleCurveDumb(objects);
        Vec p=plan.segments.front().start,z=plan.face.z;
        section->AddToSection({rule},objects.front(),nullptr,nullptr,{p.x,p.y,p.z},NXOpen::Section::ModeCreate);
        builder->SetSection(section);
        builder->SetDirection(part->Directions()->CreateDirection({p.x,p.y,p.z},{z.x,z.y,z.z},NXOpen::SmartObject::UpdateOptionWithinModeling));
        builder->SetDistanceTolerance(1e-4*plan.face.unitsPerMm);
        builder->BooleanOperation()->SetType(NXOpen::GeometricUtilities::BooleanOperation::BooleanTypeCreate);
        builder->Limits()->StartExtend()->Value()->SetFormula("0");
        builder->Limits()->EndExtend()->Value()->SetFormula(Number(plan.face.thickness).c_str());
        feature=builder->CommitFeature()->Tag();builder->Destroy();builder=nullptr;
    } catch(...) {if(builder) builder->Destroy();throw;}
    Check(UF_MODL_ask_feat_body(feature,&tool));
    if(!tool) throw std::runtime_error("辅助板拉伸失败。");
    tag_t united=0;
    Check(UF_MODL_unite_bodies_with_retained_options(plan.face.body,tool,false,false,&united));
    if(!united) throw std::runtime_error("辅助板与原板求和失败。");
    result.extrude=feature; result.unite=united;
    return result;
}

tag_t Create(const Plan& plan) { return CreateConstruction(plan).unite; }

void EditConstruction(const Plan& plan,Construction& construction) {
    auto* part=NXOpen::Session::GetSession()->Parts()->Work();
    auto* feature=dynamic_cast<NXOpen::Features::Extrude*>(NXOpen::NXObjectManager::Get(construction.extrude));
    if(!feature) throw std::runtime_error("辅助板内部拉伸特征丢失，无法编辑。");
    auto curves=Profile(plan);
    auto* builder=part->Features()->CreateExtrudeBuilder(feature);
    try {
        // Replace the section rather than the feature, preserving downstream
        // boolean references even when zero inset changes the segment count.
        auto* section=part->Sections()->CreateSection(1e-5*plan.face.unitsPerMm,1e-4*plan.face.unitsPerMm,0.5);
        std::vector<NXOpen::Curve*> objects;
        for(auto tag:curves) objects.push_back(dynamic_cast<NXOpen::Curve*>(NXOpen::NXObjectManager::Get(tag)));
        auto* rule=part->ScRuleFactory()->CreateRuleCurveDumb(objects);
        Vec p=plan.segments.front().start;
        section->AddToSection({rule},objects.front(),nullptr,nullptr,{p.x,p.y,p.z},NXOpen::Section::ModeCreate);
        builder->SetSection(section);
        builder->CommitFeature();
        builder->Destroy(); builder=nullptr;
        Check(UF_MODL_update());
        construction.curves=curves;
    } catch(...) { if(builder) builder->Destroy(); throw; }
}
}
