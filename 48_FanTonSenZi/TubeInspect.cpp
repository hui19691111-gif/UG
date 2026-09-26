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
#include <map>
#include <set>
#include <stdexcept>
namespace tube_straighten {
using namespace NXOpen;
void Check(int n){if(n){char m[1024]={};UF_get_fail_message(n,m);throw std::runtime_error(std::string(m)+" ("+std::to_string(n)+")");}}
double Volume(tag_t body){double acc[11]={.99999999},props[47]={},stats[13]={};Check(UF_MODL_ask_mass_props_3d(&body,1,1,4,1,1,acc,props,stats));return props[1];}
namespace {
Vec V(const double* v){return {v[0],v[1],v[2]};}
struct Eval {
    UF_EVAL_p_t value=nullptr;double limits[2]={};
    explicit Eval(tag_t t){Check(UF_EVAL_initialize_2(t,&value));try{Check(UF_EVAL_ask_limits(value,limits));}catch(...){UF_EVAL_free(value);throw;}}
    ~Eval(){if(value)UF_EVAL_free(value);}
    Vec At(double f){double p[3];Check(UF_EVAL_evaluate(value,0,limits[0]+f*(limits[1]-limits[0]),p,nullptr));return V(p);}
};
struct FaceData {int type=0;Vec point,normal;double radius=0;};
FaceData Data(Face* f){int type,sign;double p[3],n[3],box[6],r,r2;Check(UF_MODL_ask_face_data(f->Tag(),&type,p,n,box,&r,&r2,&sign));return {type,V(p),V(n),r};}
struct Loop {int type=0;std::vector<tag_t> edges;};
std::vector<Loop> Loops(tag_t face){
    struct Owner {uf_loop_p_t p=nullptr;~Owner(){if(p)UF_MODL_delete_loop_list(&p);}} owner;
    Check(UF_MODL_ask_face_loops(face,&owner.p));int count=0;Check(UF_MODL_ask_loop_list_count(owner.p,&count));std::vector<Loop> result;
    for(int i=0;i<count;++i){Loop loop;uf_list_p_t edges=nullptr;int n=0;Check(UF_MODL_ask_loop_list_item(owner.p,i,&loop.type,&edges));Check(UF_MODL_ask_list_count(edges,&n));for(int j=0;j<n;++j){tag_t t=0;Check(UF_MODL_ask_list_item(edges,j,&t));loop.edges.push_back(t);}result.push_back(loop);}return result;
}
Span ReadSpan(tag_t tag,bool closed=false){
    Eval e(tag);Span s;s.a=e.At(0);s.b=e.At(1);logical line=false,arc=false;Check(UF_EVAL_is_line(e.value,&line));
    if(!line){Check(UF_EVAL_is_arc(e.value,&arc));if(!arc)throw std::runtime_error("当前支持直线、圆弧路径及直边/圆弧组成的孔槽轮廓，不支持样条。");
        UF_EVAL_arc_t a;Check(UF_EVAL_ask_arc(e.value,&a));s.center=V(a.center);s.radius=a.radius;s.angle=std::abs(e.limits[1]-e.limits[0]);s.normal=Unit(Cross(s.a-s.center,e.At(.25)-s.center));
        if(!closed&&s.angle>=2*pi-1e-7)throw std::runtime_error("闭环方通须先断开一个端口。");
    }return s;
}
std::vector<Span> Chain(const std::vector<tag_t>& tags,double tol,bool closed){
    std::vector<Span> input;for(auto t:tags)input.push_back(ReadSpan(t,closed));if(input.empty())throw std::runtime_error("空轮廓。");
    size_t first=0;int reverse=0,open=0;
    for(size_t i=0;i<input.size();++i)for(int e=0;e<2;++e){Vec q=e?input[i].b:input[i].a;int n=0;
        for(size_t j=0;j<input.size();++j)if(j!=i)for(auto p:{input[j].a,input[j].b})if(Length(q-p)<tol)++n;
        if(n>1)throw std::runtime_error("路径存在分叉。");if(!n&&Length(input[i].a-input[i].b)>tol){first=i;reverse=e;++open;}
    }
    if((closed&&open)||( !closed&&open!=2))throw std::runtime_error("轮廓未闭合或路径不完整。");
    if(reverse)input[first].Reverse();std::vector<Span> result={input[first]};std::set<size_t> used={first};
    while(used.size()<input.size()){bool found=false;Vec q=result.back().b;for(size_t i=0;i<input.size();++i)if(!used.count(i)){
        if(Length(input[i].a-q)<tol||Length(input[i].b-q)<tol){if(Length(input[i].b-q)<tol)input[i].Reverse();result.push_back(input[i]);used.insert(i);found=true;break;}}
        if(!found)throw std::runtime_error("轮廓中包含不相连的边。");
    }
    if(closed&&Length(result.back().b-result.front().a)>tol)throw std::runtime_error("孔轮廓未闭合。");return result;
}
struct Rect {double ymin=1e100,ymax=-1e100,zmin=1e100,zmax=-1e100,r=0;};
Rect Rectangle(const Loop& loop,Vec origin,Vec y,Vec z,double tol){
    Rect r;int arcs=0,lines=0;std::vector<Span> spans;
    for(auto edge:loop.edges){auto c=ReadSpan(edge);spans.push_back(c);if(c.radius){++arcs;if(std::abs(c.angle-pi/2)>1e-6||r.r&&std::abs(r.r-c.radius)>tol)throw std::runtime_error("端口圆角不一致。");r.r=c.radius;}else ++lines;
        for(auto q:{c.a,c.b}){double yy=Dot(q-origin,y),zz=Dot(q-origin,z);r.ymin=std::min(r.ymin,yy);r.ymax=std::max(r.ymax,yy);r.zmin=std::min(r.zmin,zz);r.zmax=std::max(r.zmax,zz);}}
    if(lines!=4||(arcs!=0&&arcs!=4))throw std::runtime_error("端口不是矩形或等半径圆角矩形。");
    double cy=(r.ymin+r.ymax)/2,cz=(r.zmin+r.zmax)/2,hy=(r.ymax-r.ymin)/2,hz=(r.zmax-r.zmin)/2;
    for(const auto& c:spans)for(double f:{0.,.25,.5,.75,1.}){Vec p=c.Point(f)-origin;double a=std::abs(Dot(p,y)-cy)-(hy-r.r),b=std::abs(Dot(p,z)-cz)-(hz-r.r);double distance=std::hypot(std::max(0.,a),std::max(0.,b))+std::min(0.,std::max(a,b))-r.r;if(std::abs(distance)>tol)throw std::runtime_error("端口轮廓不符合等圆角矩形。");}
    return r;
}
bool LooksLikeCap(Face* face,double tol){
    auto loops=Loops(face->Tag());if(loops.size()!=2)return false;const Loop *outer=nullptr,*inner=nullptr;
    for(const auto& l:loops){if(l.type==1)outer=&l;if(l.type==2)inner=&l;}if(!outer||!inner)return false;
    try{auto edge=ReadSpan(outer->edges.front());Vec y=edge.Tangent(0),z=Cross(Data(face).normal,y);auto a=Rectangle(*outer,edge.a,y,z,tol),b=Rectangle(*inner,edge.a,y,z,tol);double t=b.ymin-a.ymin;
        return t>tol&&std::abs(a.ymax-b.ymax-t)<tol&&std::abs(b.zmin-a.zmin-t)<tol&&std::abs(a.zmax-b.zmax-t)<tol&&std::abs(b.r-std::max(0.,a.r-t))<tol;
    }catch(const std::exception&){return false;}
}
// STEP and sweep bodies often keep a separate planar face at each tangent
// join. Follow only shared-edge, coplanar faces on this body. Their common
// edges are subdivision seams, not boundaries of the tube's side wall.
std::vector<tag_t> SideBoundary(Face* selected,double tol){
    const auto plane=Data(selected);const Vec normal=Unit(plane.normal);
    struct PatchFace{tag_t tag;std::vector<tag_t> edges;};std::vector<PatchFace> candidates;
    for(auto* face:selected->GetBody()->GetFaces()){
        const auto fd=Data(face);
        if(fd.type!=22||std::abs(Dot(Unit(fd.normal),normal))<1-1e-7||std::abs(Dot(fd.point-plane.point,normal))>tol)continue;
        PatchFace item{face->Tag(),{}};for(auto* edge:face->GetEdges())item.edges.push_back(edge->Tag());candidates.push_back(std::move(item));
    }
    std::set<tag_t> included{selected->Tag()},edges;for(auto* edge:selected->GetEdges())edges.insert(edge->Tag());
    bool changed=true;while(changed){changed=false;for(const auto& face:candidates)if(!included.count(face.tag)){
        if(std::any_of(face.edges.begin(),face.edges.end(),[&](tag_t tag){return edges.count(tag)!=0;})){
            included.insert(face.tag);edges.insert(face.edges.begin(),face.edges.end());changed=true;
        }
    }}
    std::map<tag_t,int> uses;for(const auto& face:candidates)if(included.count(face.tag))for(auto tag:face.edges)++uses[tag];
    std::vector<tag_t> boundary;for(auto [tag,count]:uses){if(count==1)boundary.push_back(tag);else if(count!=2)throw std::runtime_error("侧面分面边界不完整，无法识别整条方通。");}
    return boundary;
}
struct Cap {double w=0,d=0,t=0,r=0,shift=0;Vec width;};
Cap InspectCap(Body* body,Vec origin,Vec tangent,Vec normal,double u){
    double tol=1e-4*u;Vec inside=Cross(normal,tangent);std::vector<Cap> candidates;
    for(auto* face:body->GetFaces()){
        auto fd=Data(face);if(fd.type!=22||std::abs(Dot(Unit(fd.normal),tangent))<1-1e-7||std::abs(Dot(fd.point-origin,tangent))>tol)continue;
        auto loops=Loops(face->Tag());if(loops.size()!=2)continue;
        try{const Loop* outer=nullptr,*inner=nullptr;for(const auto& l:loops){if(l.type==1)outer=&l;if(l.type==2)inner=&l;}if(!outer||!inner)continue;
            bool hasOrigin=false;for(auto edge:outer->edges){Eval e(edge);hasOrigin=hasOrigin||Length(e.At(0)-origin)<tol||Length(e.At(1)-origin)<tol;}if(!hasOrigin)continue;
            auto a=Rectangle(*outer,origin,inside,normal,tol),b=Rectangle(*inner,origin,inside,normal,tol);Vec width=normal;
            if(std::abs(a.zmax)<tol)width=normal*(-1);else if(std::abs(a.zmin)>tol)continue;
            double t=b.ymin-a.ymin;if(t<=tol||std::abs(a.ymax-b.ymax-t)>tol||std::abs(b.zmin-a.zmin-t)>tol||std::abs(a.zmax-b.zmax-t)>tol||std::abs(b.r-std::max(0.,a.r-t))>tol)continue;
            candidates.push_back({a.zmax-a.zmin,a.ymax-a.ymin,t,a.r,a.ymin,width});
        }catch(const std::exception&){}
    }
    if(candidates.size()!=1)throw std::runtime_error("未识别到等壁厚方通端口；支持矩形及等半径圆角矩形截面。");return candidates.front();
}
void OffsetPath(std::vector<Span>& path,Vec normal,double offset){
    auto old=path;for(size_t i=0;i<path.size();++i){auto& s=path[i];s.a=old[i].a+Cross(normal,old[i].Tangent(0))*offset;s.b=old[i].b+Cross(normal,old[i].Tangent(1))*offset;if(s.radius)s.radius-=Dot(s.normal,normal)*offset;}
    for(size_t i=1;i<path.size();++i){auto a=old[i-1].Tangent(1),b=old[i].Tangent(0);if(Length(a-b)>1e-7){if(old[i-1].radius||old[i].radius)throw std::runtime_error("圆弧与直段须相切。");Vec q=old[i].a+(Cross(normal,a)+Cross(normal,b))*(offset/(1+Dot(a,b)));path[i-1].b=q;path[i].a=q;}}
}
void ReadHoles(Source& s,Body* body){
    double tol=1e-4*s.unitsPerMm;Vec origin=s.spans.front().a;
    for(auto* face:body->GetFaces()){
        auto fd=Data(face);if(fd.type!=22)continue;Vec dir;double z=Dot(fd.point-origin,s.widthDirection);
        if(std::abs(Dot(fd.normal,s.widthDirection))>1-1e-7){if(std::abs(z)<tol)dir=s.widthDirection;else if(std::abs(z-s.width)<tol)dir=s.widthDirection*(-1);}
        else for(const auto& span:s.spans)if(!span.radius){Vec inside=Cross(s.normal,span.Tangent(0));double y=Dot(fd.point-span.a,inside);if(std::abs(Dot(fd.normal,inside))>1-1e-7){if(std::abs(y)<tol)dir=inside;else if(std::abs(y-s.depth)<tol)dir=inside*(-1);}}
        if(Length(dir)<.9)continue;
        for(auto loop:Loops(face->Tag()))if(loop.type==2){Hole h;h.direction=dir;h.length=s.thickness;h.profile=Chain(loop.edges,tol,true);double area=0;Vec base=h.profile.front().a;
            for(const auto& c:h.profile){area+=Dot(Cross(c.a-base,c.b-base),dir)/2;if(c.radius)area+=Dot(c.normal,dir)*c.radius*c.radius*(c.angle-std::sin(c.angle))/2;}
            h.area=std::abs(area);if(h.area<=tol*tol)throw std::runtime_error("孔槽轮廓面积无效。");
            // Both skins must have the same opening: each complete rim must
            // continue through the wall, not be a blind pocket or countersink.
            for(const auto& c:h.profile)for(double f:{.125,.375,.625,.875}){Vec q=c.Point(f)+dir*s.thickness;double point[]={q.x,q.y,q.z};int where=0;Check(UF_MODL_ask_point_containment(point,s.body,&where));if(where!=3)throw std::runtime_error("孔槽不是贯穿单层壁厚的等截面开口；盲孔和沉孔暂不支持。");}
            s.holes.push_back(h);
        }
    }
}
}
Source Inspect(const std::vector<tag_t>& tags){
    auto* part=Session::GetSession()->Parts()->Work();if(!part)throw std::runtime_error("请先打开零件。");
    if(tags.empty()||tags.size()>360)throw std::runtime_error("未找到完整方通路径。");Source s;int units=0;Check(UF_PART_ask_units(part->Tag(),&units));s.unitsPerMm=units==ENGLISH?1/25.4:1;
    double tol=1e-4*s.unitsPerMm;Body* body=nullptr;std::set<tag_t> seen;
    for(auto tag:tags){if(!seen.insert(tag).second)throw std::runtime_error("路径中存在重复边。");auto* edge=dynamic_cast<Edge*>(NXObjectManager::Get(tag));if(!edge||edge->IsOccurrence()||edge->OwningPart()!=part)throw std::runtime_error("请选择当前工作零件中的实体。");if(!body)body=edge->GetBody();if(edge->GetBody()!=body||!body->IsSolidBody())throw std::runtime_error("输入必须属于同一实体方通。");}
    s.body=body->Tag();s.spans=Chain(tags,tol,false);Vec normal;
    for(size_t i=0;i<s.spans.size();++i){auto& span=s.spans[i];if(span.radius){normal=span.normal;break;}if(i){auto cross=Cross(s.spans[i-1].Tangent(1),span.Tangent(0));if(Length(cross)>1e-6){normal=Unit(cross);break;}}}
    if(Length(normal)<.9)throw std::runtime_error("方通没有圆弧或转角，无需伸直。");s.normal=normal;Vec origin=s.spans.front().a;
    for(const auto& span:s.spans)for(double f:{0.,.25,.5,.75,1.})if(std::abs(Dot(span.Point(f)-origin,normal))>tol||(span.radius&&std::abs(Dot(span.normal,normal))<1-1e-7))throw std::runtime_error("当前支持同一平面内的连续弯曲和 S 形方通；空间弯曲暂不支持。");
    auto cap=InspectCap(body,origin,s.spans.front().Tangent(0),normal,s.unitsPerMm),end=InspectCap(body,s.spans.back().b,s.spans.back().Tangent(1),normal,s.unitsPerMm);
    if(std::abs(cap.w-end.w)>tol||std::abs(cap.d-end.d)>tol||std::abs(cap.t-end.t)>tol||std::abs(cap.r-end.r)>tol||std::abs(cap.shift-end.shift)>tol||Dot(cap.width,end.width)<1-1e-7)throw std::runtime_error("方通两端截面不一致或路径换边。");
    s.width=cap.w;s.depth=cap.d;s.thickness=cap.t;s.cornerRadius=cap.r;s.widthDirection=cap.width;OffsetPath(s.spans,normal,cap.shift);
    double centerLength=0;for(size_t i=0;i<s.spans.size();++i){auto& span=s.spans[i];centerLength+=span.radius?(span.radius-Dot(span.normal,normal)*cap.d/2)*span.angle:Length(span.b-span.a);if(i){auto a=s.spans[i-1].Tangent(1),b=span.Tangent(0);double angle=std::atan2(Dot(Cross(a,b),normal),Dot(a,b));if(std::abs(angle)>pi*.75)throw std::runtime_error("单个转角须不超过 135°。");centerLength-=cap.d*std::tan(angle/2);}}
    ReadHoles(s,body);
    for(auto* face:body->GetFaces()){int type=Data(face).type;if(type!=16&&type!=19&&type!=22)throw std::runtime_error("存在不支持的自由曲面、倒角或孔槽形状。");}
    auto area=[](double w,double d,double r){return w*d-(4-pi)*r*r;};
    double expected=(area(cap.w,cap.d,cap.r)-area(cap.w-2*cap.t,cap.d-2*cap.t,std::max(0.,cap.r-cap.t)))*centerLength;
    for(const auto& h:s.holes)expected-=h.area*h.length;
    double actual=Volume(s.body)*1e9*std::pow(s.unitsPerMm,3);
    if(expected<=0||std::abs(actual-expected)>std::max(expected*1e-6,std::pow(tol,3)))throw std::runtime_error("截面或孔槽核对不一致。当前支持平直管壁上的贯穿圆孔/闭合槽；盲孔、斜孔、曲壁孔或变截面不能直接展开。");
    return s;
}
static Source InspectSideFace(tag_t tag){
    auto* face=dynamic_cast<Face*>(NXObjectManager::Get(tag));auto* part=Session::GetSession()->Parts()->Work();if(!face||face->IsOccurrence()||face->OwningPart()!=part||!face->GetBody()->IsSolidBody())throw std::runtime_error("请选择当前工作零件方通上的一个外侧平面。");
    auto data=Data(face);if(data.type!=22||IsRoundCap(tag))return InspectRoundFace(tag);
    int units=0;Check(UF_PART_ask_units(part->Tag(),&units));const double tol=1e-5*(units==ENGLISH?1/25.4:1);
    const auto ring=SideBoundary(face,tol);
    std::set<tag_t> ends;
    for(auto* other:face->GetBody()->GetFaces()){auto fd=Data(other);if(fd.type!=22||std::abs(Dot(data.normal,fd.normal))>1e-6)continue;if(!LooksLikeCap(other,tol))continue;
        for(auto* e:other->GetEdges())if(std::find(ring.begin(),ring.end(),e->Tag())!=ring.end())ends.insert(e->Tag());}
    if(ends.size()!=2)throw std::runtime_error("所选面及相邻共面侧面未连到两个端口。请选择弯曲平面两侧的大侧面，直段或圆弧上的分面均可；不要选端面、内壁或圆角面。");
    std::vector<tag_t> remaining;for(auto e:ring)if(!ends.count(e))remaining.push_back(e);std::string failure="未找到完整方通路径。";
    while(!remaining.empty()){std::vector<tag_t> chain={remaining.back()};remaining.pop_back();bool changed=true;while(changed){changed=false;for(auto it=remaining.begin();it!=remaining.end();){auto e=ReadSpan(*it,true);bool match=false;for(auto t:chain){auto c=ReadSpan(t,true);for(auto a:{e.a,e.b})for(auto b:{c.a,c.b})if(Length(a-b)<tol)match=true;}if(match){chain.push_back(*it);it=remaining.erase(it);changed=true;}else ++it;}}
        try{auto result=Inspect(chain);if(std::abs(Dot(result.normal,data.normal))<1-1e-7)throw std::runtime_error("所选面不是弯曲路径所在平面的侧面。");return result;}catch(const std::exception& e){failure=e.what();}}
    throw std::runtime_error(failure);
}
static Source InspectSpatial(Body* body){
    int units=0;Check(UF_PART_ask_units(body->OwningPart()->Tag(),&units));double u=units==ENGLISH?1/25.4:1,tol=1e-4*u;
    for(auto* face:body->GetFaces()){int type=Data(face).type;if(type!=16&&type!=19&&type!=22)throw std::runtime_error("空间方通包含自由曲面、变形截面或不支持的孔槽形状。");}
    std::vector<Face*> caps;for(auto* f:body->GetFaces())if(Data(f).type==22&&LooksLikeCap(f,tol))caps.push_back(f);
    if(caps.size()!=2)throw std::runtime_error("空间方通须有两个完整等壁厚矩形端口。");
    struct Rail{tag_t tag;Span curve;};std::vector<Rail> rails;std::set<tag_t> capEdges;
    for(auto* f:caps)for(auto* e:f->GetEdges())capEdges.insert(e->Tag());
    for(auto* e:body->GetEdges())if(!capEdges.count(e->Tag())){try{auto span=ReadSpan(e->Tag());if(Length(span.a-span.b)>tol)rails.push_back({e->Tag(),span});}catch(const std::exception&){}}
    auto loops=Loops(caps[0]->Tag());Loop outer,inner;for(auto l:loops){if(l.type==1)outer=l;else if(l.type==2)inner=l;}
    auto finishLoops=Loops(caps[1]->Tag());Loop endOuter,endInner;for(auto l:finishLoops){if(l.type==1)endOuter=l;else if(l.type==2)endInner=l;}
    std::vector<Vec> starts,finishes;for(auto tag:outer.edges){auto c=ReadSpan(tag);starts.push_back(c.a);starts.push_back(c.b);}for(auto tag:endOuter.edges){auto c=ReadSpan(tag);finishes.push_back(c.a);finishes.push_back(c.b);}
    std::string failure="没有找到连续相切的空间方通纵向边。";
    for(Vec start:starts)try{
        std::vector<Span> path;std::set<tag_t> used;Vec position=start,axis;bool finished=false;
        for(size_t step=0;step<=rails.size();++step){
            if(!path.empty()&&std::any_of(finishes.begin(),finishes.end(),[&](Vec p){return Length(p-position)<tol;})){finished=true;break;}
            std::vector<Rail> next;for(auto rail:rails)if(!used.count(rail.tag)){
                if(Length(rail.curve.b-position)<tol)rail.curve.Reverse();else if(Length(rail.curve.a-position)>=tol)continue;
                const auto tangent=rail.curve.Tangent(0);if(path.empty()?std::abs(Dot(tangent,Unit(Data(caps[0]).normal)))>1-1e-7:Dot(axis,tangent)>1-1e-7)next.push_back(rail);
            }
            if(next.size()!=1)throw std::runtime_error("空间管段须连续相切，不能包含分支或非相切折角。");
            used.insert(next[0].tag);path.push_back(next[0].curve);position=path.back().b;axis=path.back().Tangent(1);
        }
        if(!finished)throw std::runtime_error("空间方通纵向边未贯通两端。");
        Source s;s.body=body->Tag();s.spatial=true;s.unitsPerMm=u;Vec y;
        for(auto tag:outer.edges){auto c=ReadSpan(tag);if(!c.radius){y=c.Tangent(0);break;}}
        axis=path.front().Tangent(0);Vec z=Unit(Cross(axis,y));auto section=Rectangle(outer,start,y,z,tol),bore=Rectangle(inner,start,y,z,tol);
        s.depth=section.ymax-section.ymin;s.width=section.zmax-section.zmin;s.thickness=bore.ymin-section.ymin;s.cornerRadius=section.r;
        if(s.thickness<=tol||std::abs(section.ymax-bore.ymax-s.thickness)>tol||std::abs(bore.zmin-section.zmin-s.thickness)>tol||std::abs(section.zmax-bore.zmax-s.thickness)>tol)throw std::runtime_error("空间方通壁厚不一致。");
        Vec center=start+y*((section.ymin+section.ymax)/2)+z*((section.zmin+section.zmax)/2),offset=start-center;s.normal=s.widthDirection=z;
        double totalLength=0;for(const auto& rail:path){Span span=rail;span.frameY=y;span.frameZ=z;span.a=rail.a-offset;
            if(rail.radius){span.center=rail.center-rail.normal*Dot(offset,rail.normal);span.radius=Length(span.a-span.center);offset=Rotate(offset,rail.normal,rail.angle);y=Rotate(y,rail.normal,rail.angle);z=Rotate(z,rail.normal,rail.angle);if(span.radius<=std::hypot(s.depth,s.width)/2)throw std::runtime_error("空间弯管半径过小。");}
            span.b=rail.b-offset;if(Length(span.Tangent(0)-rail.Tangent(0))>1e-7)throw std::runtime_error("空间管件截面发生扭转或变形。");totalLength+=span.radius?span.radius*span.angle:Length(span.b-span.a);s.spans.push_back(span);
        }
        auto last=s.spans.back();Vec endCenter=last.b;auto end=Rectangle(endOuter,endCenter,y,z,tol),endBore=Rectangle(endInner,endCenter,y,z,tol);
        if(std::abs(Dot(last.Tangent(1),Unit(Data(caps[1]).normal)))<1-1e-7||std::abs(end.ymin+s.depth/2)>tol||std::abs(end.ymax-s.depth/2)>tol||std::abs(end.zmin+s.width/2)>tol||std::abs(end.zmax-s.width/2)>tol||std::abs(end.r-s.cornerRadius)>tol||std::abs(endBore.ymin-end.ymin-s.thickness)>tol||std::abs(end.ymax-endBore.ymax-s.thickness)>tol||std::abs(endBore.zmin-end.zmin-s.thickness)>tol||std::abs(end.zmax-endBore.zmax-s.thickness)>tol)throw std::runtime_error("空间管两端截面方向或尺寸不一致。");
        for(auto* face:body->GetFaces()){auto fd=Data(face);if(fd.type!=22)continue;Vec direction;
            for(const auto& span:s.spans)if(!span.radius){Vec q=fd.point-span.a;for(auto pair:std::vector<std::pair<Vec,double>>{{span.frameY,s.depth},{span.frameZ,s.width}}){double at=Dot(q,pair.first);if(std::abs(Dot(Unit(fd.normal),pair.first))>1-1e-7){if(std::abs(at-pair.second/2)<tol)direction=pair.first*(-1);else if(std::abs(at+pair.second/2)<tol)direction=pair.first;}}}
            if(Length(direction)<.9)continue;for(const auto& loop:Loops(face->Tag()))if(loop.type==2){Hole h;h.direction=direction;h.length=s.thickness;h.profile=Chain(loop.edges,tol,true);Vec base=h.profile.front().a;double area=0;for(const auto& c:h.profile){area+=Dot(Cross(c.a-base,c.b-base),direction)/2;if(c.radius)area+=Dot(c.normal,direction)*c.radius*c.radius*(c.angle-sin(c.angle))/2;}h.area=std::abs(area);for(const auto& c:h.profile)for(double f:{.125,.375,.625,.875}){auto q=c.Point(f)+direction*s.thickness;double xyz[]={q.x,q.y,q.z};int where=0;Check(UF_MODL_ask_point_containment(xyz,s.body,&where));if(where!=3)throw std::runtime_error("空间方通只支持平壁等截面贯穿孔槽。");}s.holes.push_back(h);}
        }
        auto area=[](double d,double w,double r){return d*w-(4-pi)*r*r;};double expected=(area(s.depth,s.width,s.cornerRadius)-area(s.depth-2*s.thickness,s.width-2*s.thickness,std::max(0.,s.cornerRadius-s.thickness)))*totalLength;for(const auto& h:s.holes)expected-=h.area*h.length;
        double actual=Volume(s.body)*1e9*std::pow(u,3);if(expected<=0||std::abs(expected-actual)>expected*1e-6)throw std::runtime_error("空间路径、截面或孔槽体积核对不一致，已取消生成。");
        return s;
    }catch(const std::exception& e){failure=e.what();}
    throw std::runtime_error(failure);
}
Source InspectFace(tag_t tag){
    auto* face=dynamic_cast<Face*>(NXObjectManager::Get(tag));auto* part=Session::GetSession()->Parts()->Work();
    if(!face||!part||face->IsOccurrence()||face->OwningPart()!=part||!face->GetBody()->IsSolidBody())throw std::runtime_error("请选择当前工作零件管件上的一个面。");
    if(Data(face).type!=22||IsRoundCap(tag))return InspectRoundFace(tag);
    std::string failure;try{return InspectSideFace(tag);}catch(const std::exception& e){failure=e.what();}
    // The selected planar face is the placement reference; an exterior side
    // patch elsewhere on this same body supplies the complete bending path.
    for(auto* other:face->GetBody()->GetFaces())if(other!=face&&Data(other).type==22){try{return InspectSideFace(other->Tag());}catch(const std::exception&){}}
    try{return InspectSpatial(face->GetBody());}catch(const std::exception& e){failure=e.what();}
    throw std::runtime_error("未能从所选平面所在实体识别完整等壁厚方通。"+failure);
}
}
