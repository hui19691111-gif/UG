#include "TubeGeometry.hpp"
#include <NXOpen/Body.hxx>
#include <NXOpen/Edge.hxx>
#include <NXOpen/Face.hxx>
#include <NXOpen/NXObjectManager.hxx>
#include <NXOpen/Part.hxx>
#include <NXOpen/PartCollection.hxx>
#include <NXOpen/Session.hxx>
#include <uf.h>
#include <uf_eval.h>
#include <uf_modl.h>
#include <uf_part.h>
#include <algorithm>
#include <set>
#include <stdexcept>
namespace tube_straighten {
namespace {
using namespace NXOpen;
Vec V(const double* p){return {p[0],p[1],p[2]};}
struct Surface {int type=0;Vec p,n;double r=0,minor=0;};
Surface Data(tag_t face){int type,sign;double p[3],n[3],box[6],r=0,minor=0;Check(UF_MODL_ask_face_data(face,&type,p,n,box,&r,&minor,&sign));return {type,V(p),Unit(V(n)),r,type==19?minor:0};}
struct Circle {Vec center,normal;double radius=0,angle=0;};
bool CircleEdge(tag_t edge,Circle& c){
    struct Owner{UF_EVAL_p_t p=nullptr;~Owner(){if(p)UF_EVAL_free(p);}} e;
    Check(UF_EVAL_initialize_2(edge,&e.p));logical arc=false;Check(UF_EVAL_is_arc(e.p,&arc));if(!arc)return false;
    UF_EVAL_arc_t a;double limits[2];Check(UF_EVAL_ask_arc(e.p,&a));Check(UF_EVAL_ask_limits(e.p,limits));
    c={V(a.center),Unit(Cross(V(a.x_axis),V(a.y_axis))),a.radius,std::abs(limits[1]-limits[0])};return true;
}
bool CircleLoop(uf_list_p_t edges,Circle& circle,double tol){
    int count=0;Check(UF_MODL_ask_list_count(edges,&count));if(!count)return false;double angle=0;
    for(int i=0;i<count;++i){tag_t edge=0;Circle c;Check(UF_MODL_ask_list_item(edges,i,&edge));if(!CircleEdge(edge,c))return false;
        if(i&&(Length(c.center-circle.center)>tol||std::abs(c.radius-circle.radius)>tol||std::abs(Dot(c.normal,circle.normal))<1-1e-7))return false;
        if(!i)circle=c;angle+=c.angle;
    }return std::abs(angle-2*pi)<1e-6;
}
struct End {tag_t face=0;Vec center,normal;double outer=0,inner=0;};
bool EndCap(tag_t face,double tol,End& cap){
    auto fd=Data(face);if(fd.type!=22)return false;
    struct Owner{uf_loop_p_t p=nullptr;~Owner(){if(p)UF_MODL_delete_loop_list(&p);}} loops;
    Check(UF_MODL_ask_face_loops(face,&loops.p));int count=0;Check(UF_MODL_ask_loop_list_count(loops.p,&count));if(count!=2)return false;
    Circle a,b;bool outer=false,inner=false;
    for(int i=0;i<count;++i){int type=0;uf_list_p_t edges=nullptr;Check(UF_MODL_ask_loop_list_item(loops.p,i,&type,&edges));
        if(type==1)outer=CircleLoop(edges,a,tol);else if(type==2)inner=CircleLoop(edges,b,tol);
    }
    if(!outer||!inner||a.radius-b.radius<=tol||b.radius<=tol||Length(a.center-b.center)>tol||std::abs(Dot(a.normal,fd.n))<1-1e-7)return false;
    cap={face,a.center,fd.n,a.radius,b.radius};return true;
}
bool SameSurface(const Surface& a,const Surface& b,double tol){
    if(a.type!=b.type||std::abs(a.r-b.r)>tol||std::abs(a.minor-b.minor)>tol||std::abs(Dot(a.n,b.n))<1-1e-7)return false;
    return a.type==16?Length(Cross(b.p-a.p,a.n))<tol:Length(b.p-a.p)<tol;
}
struct Node {Surface surface;std::vector<tag_t> faces;std::set<tag_t> edges;int end=-1;};
std::vector<tag_t> Common(const Node& a,const Node& b){std::vector<tag_t> edges;std::set_intersection(a.edges.begin(),a.edges.end(),b.edges.begin(),b.edges.end(),std::back_inserter(edges));return edges;}
Vec Junction(const Node& a,const Node& b,const std::vector<End>& ends,double radius,double tol){
    if(a.end>=0)return ends[a.end].center;if(b.end>=0)return ends[b.end].center;
    Vec center;bool found=false;
    for(auto edge:Common(a,b)){Circle c;if(CircleEdge(edge,c)&&std::abs(c.radius-radius)<tol){if(found&&Length(c.center-center)>tol)throw std::runtime_error("圆管相邻段的截面中心不一致。");center=c.center;found=true;}}
    if(found)return center;
    // A mitred pair of cylinders has elliptical rims. Intersect their axes.
    if(a.surface.type==16&&b.surface.type==16){auto u=a.surface.n,v=b.surface.n,w=b.surface.p-a.surface.p;double dot=Dot(u,v),den=1-dot*dot;
        if(den>1e-10){Vec p=a.surface.p+u*((Dot(w,u)-dot*Dot(w,v))/den),q=b.surface.p+v*((dot*Dot(w,u)-Dot(w,v))/den);
            if(Length(p-q)<tol)return (p+q)*.5;
        }
    }
    throw std::runtime_error("圆管段间连接不是垂直圆截面或相交轴线的转角。");
}
Vec PointOnFace(tag_t face,double u,double v){double range[4],uv[2],p[3],a[3],b[3],c[3],d[3],n[3],r[2];Check(UF_MODL_ask_face_uv_minmax(face,range));uv[0]=range[0]+u*(range[1]-range[0]);uv[1]=range[2]+v*(range[3]-range[2]);Check(UF_MODL_ask_face_props(face,uv,p,a,b,c,d,n,r));return V(p);}
}
bool IsRoundCap(tag_t face){End cap;return EndCap(face,1e-6,cap);}
Source InspectRoundFace(tag_t tag){
    using namespace NXOpen;auto* part=Session::GetSession()->Parts()->Work();auto* face=dynamic_cast<Face*>(NXObjectManager::Get(tag));
    if(!part||!face||face->IsOccurrence()||face->OwningPart()!=part||!face->GetBody()->IsSolidBody())throw std::runtime_error("请选择工作零件内圆管上的一个面。");
    auto* body=face->GetBody();Source s;s.body=body->Tag();s.round=true;int units=0;Check(UF_PART_ask_units(part->Tag(),&units));s.unitsPerMm=units==ENGLISH?1/25.4:1;double tol=1e-4*s.unitsPerMm;
    std::vector<End> ends;for(auto* f:body->GetFaces()){End e;if(EndCap(f->Tag(),tol,e))ends.push_back(e);}
    if(ends.size()!=2)throw std::runtime_error("圆管须有两个完整的圆环端口；暂不支持封口、分支、斜端口或孔槽。");
    if(ends[1].face==tag)std::swap(ends[0],ends[1]);
    double r=ends[0].outer,ri=ends[0].inner;if(std::abs(r-ends[1].outer)>tol||std::abs(ri-ends[1].inner)>tol)throw std::runtime_error("圆管两端外径或壁厚不一致。");
    s.width=s.depth=2*r;s.cornerRadius=r;s.thickness=r-ri;
    std::vector<Node> nodes;std::vector<Surface> inner;
    for(auto* f:body->GetFaces()){
        if(f->Tag()==ends[0].face||f->Tag()==ends[1].face)continue;auto fd=Data(f->Tag());double section=fd.type==16?fd.r:fd.minor;
        if((fd.type!=16&&fd.type!=19)||(std::abs(section-r)>tol&&std::abs(section-ri)>tol))throw std::runtime_error("圆管仅支持等壁厚圆柱/圆环弯管；带孔槽、倒角、变径或自由曲面的圆管暂不支持。");
        if(std::abs(section-ri)<tol){inner.push_back(fd);continue;}
        auto it=std::find_if(nodes.begin(),nodes.end(),[&](const Node& n){return SameSurface(n.surface,fd,tol);});
        if(it==nodes.end()){nodes.push_back({fd});it=nodes.end()-1;}
        it->faces.push_back(f->Tag());for(auto* e:f->GetEdges())it->edges.insert(e->Tag());
    }
    if(nodes.empty()||nodes.size()>360)throw std::runtime_error("没有识别到有效圆管外壁。");
    for(const auto& node:nodes){auto match=node.surface;if(match.type==16)match.r=ri;else match.minor=ri;
        if(std::none_of(inner.begin(),inner.end(),[&](const Surface& in){return SameSurface(match,in,tol);}))throw std::runtime_error("圆管内外壁不同心或壁厚不一致。");
    }
    size_t outerCount=nodes.size();for(int i=0;i<2;++i){Node n;n.end=i;for(auto* e:dynamic_cast<Face*>(NXObjectManager::Get(ends[i].face))->GetEdges())n.edges.insert(e->Tag());nodes.push_back(n);}
    std::vector<std::vector<size_t>> adjacent(nodes.size());for(size_t i=0;i<nodes.size();++i)for(size_t j=i+1;j<nodes.size();++j)if(!Common(nodes[i],nodes[j]).empty()){adjacent[i].push_back(j);adjacent[j].push_back(i);}
    for(size_t i=0;i<nodes.size();++i)if(adjacent[i].size()!=(i<outerCount?2:1))throw std::runtime_error("圆管路径存在分支、断面或不能识别的曲面拼接。");
    std::vector<size_t> chain={outerCount};std::set<size_t> visited={outerCount};size_t current=outerCount;
    while(current!=outerCount+1){size_t next=nodes.size();for(auto candidate:adjacent[current])if(!visited.count(candidate)){next=candidate;break;}if(next==nodes.size())throw std::runtime_error("圆管路径未连通两端。");chain.push_back(next);visited.insert(next);current=next;}
    if(chain.size()!=nodes.size())throw std::runtime_error("圆管包含额外分支或闭环。");
    for(size_t i=1;i+1<chain.size();++i){const auto& n=nodes[chain[i]];const auto& fd=n.surface;Span span;
        span.a=Junction(nodes[chain[i-1]],n,ends,r,tol);span.b=Junction(n,nodes[chain[i+1]],ends,r,tol);
        if(Length(span.b-span.a)<tol)throw std::runtime_error("不支持闭环圆管。");
        if(fd.type==16){for(auto p:{span.a,span.b})if(Length(Cross(p-fd.p,fd.n))>tol)throw std::runtime_error("圆管端口不垂直轴线。");}
        else{
            span.center=fd.p;span.radius=fd.r;span.normal=fd.n;Vec a=span.a-fd.p,b=span.b-fd.p;
            if(std::abs(Length(a)-fd.r)>tol||std::abs(Length(b)-fd.r)>tol||std::abs(Dot(a,fd.n))>tol||std::abs(Dot(b,fd.n))>tol)throw std::runtime_error("弯圆管截面中心不在同一圆弧上。");
            double cross=Dot(Cross(a,b),fd.n);if(std::abs(cross)<tol*tol){Vec sample=PointOnFace(n.faces.front(),.37,.41)-fd.p;cross=Dot(Cross(a,sample),fd.n);}
            if(cross<0)span.normal=span.normal*(-1);span.angle=std::atan2(Dot(Cross(a,b),span.normal),Dot(a,b));if(span.angle<0)span.angle+=2*pi;
            if(span.angle<1e-7||span.angle>pi+1e-7||span.radius<=r)throw std::runtime_error("弯圆管单段须小于等于 180°，中心半径须大于外半径。");
            for(auto f:n.faces)for(double u:{.17,.53,.83})for(double v:{.19,.47,.79}){Vec q=PointOnFace(f,u,v)-fd.p;q=q-fd.n*Dot(q,fd.n);double angle=std::atan2(Dot(Cross(a,q),span.normal),Dot(a,q));if(angle< -1e-7)angle+=2*pi;if(angle>span.angle+1e-7)throw std::runtime_error("不支持大于 180° 的圆环弯管段。");}
        }s.spans.push_back(span);
    }
    Vec normal;for(size_t i=0;i<s.spans.size();++i){if(s.spans[i].radius){normal=s.spans[i].normal;break;}if(i){auto n=Cross(s.spans[i-1].Tangent(1),s.spans[i].Tangent(0));if(Length(n)>1e-7){normal=Unit(n);break;}}}
    if(Length(normal)<.9)throw std::runtime_error("圆管已是直管，无需伸直。");s.normal=s.widthDirection=normal;
    if(std::abs(Dot(ends[0].normal,s.spans.front().Tangent(0)))<1-1e-7||std::abs(Dot(ends[1].normal,s.spans.back().Tangent(1)))<1-1e-7)throw std::runtime_error("圆管端口须垂直轴线。");
    double length=0;for(size_t i=0;i<s.spans.size();++i){const auto& span=s.spans[i];for(double f:{0.,.5,1.})if(std::abs(Dot(span.Point(f)-s.spans.front().a,normal))>tol||(span.radius&&Dot(span.normal,normal)<1-1e-7))throw std::runtime_error("圆管目前支持同一平面同向弯曲，不支持 S 形和空间管。");
        if(i){auto a=s.spans[i-1].Tangent(1),b=span.Tangent(0);if((span.radius||s.spans[i-1].radius)&&Length(a-b)>1e-7)throw std::runtime_error("圆弧与相邻管段须相切。");}
        length+=span.radius?span.radius*span.angle:Length(span.b-span.a);
    }
    double expected=pi*(r*r-ri*ri)*length,actual=Volume(s.body)*1e9*std::pow(s.unitsPerMm,3);
    if(std::abs(actual-expected)>expected*1e-6)throw std::runtime_error("圆管体积与完整等厚管不一致，可能存在孔槽、缺口或重叠管段，已取消展开。");
    // Convert the centerline to the retained outer generatrix; the virtual
    // section corner keeps the circular end cap in its original position.
    auto center=s.spans;for(size_t i=0;i<s.spans.size();++i){auto& span=s.spans[i];span.a=center[i].a-Cross(normal,center[i].Tangent(0))*r-normal*r;span.b=center[i].b-Cross(normal,center[i].Tangent(1))*r-normal*r;span.center=span.center-normal*r;if(span.radius)span.radius+=r;
        if(i&&Length(center[i-1].Tangent(1)-center[i].Tangent(0))>1e-7){Vec a=center[i-1].Tangent(1),b=center[i].Tangent(0);if(Dot(a,b)<-.999999)throw std::runtime_error("圆管转角过大。");Vec joint=center[i].a-(Cross(normal,a)+Cross(normal,b))*(r/(1+Dot(a,b)))-normal*r;s.spans[i-1].b=span.a=joint;}
    }
    return s;
}
}
