#include "DwgImport.hpp"

#include <Windows.h>
#include <atlbase.h>
#include <atlcom.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>
#include <string>

namespace bend_sim {
namespace {
constexpr double joinTol = 1e-5;
constexpr double sagitta = 0.005;
constexpr double pi = 3.14159265358979323846;

struct ComScope {
    HRESULT hr;
    ComScope() : hr(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)) {
        if (FAILED(hr) && hr != RPC_E_CHANGED_MODE)
            throw std::runtime_error("无法初始化 AutoCAD 接口。");
    }
    ~ComScope() { if (SUCCEEDED(hr)) CoUninitialize(); }
};

CComVariant Call(IDispatch* object, const wchar_t* name, WORD flags,
                 std::vector<CComVariant> arguments = {}) {
    if (!object) throw std::runtime_error("AutoCAD 返回了空对象。");
    LPOLESTR method = const_cast<LPOLESTR>(name);
    DISPID id = 0;
    if (FAILED(object->GetIDsOfNames(IID_NULL, &method, 1, LOCALE_USER_DEFAULT, &id)))
        throw std::runtime_error("AutoCAD 接口缺少必要属性或方法。");
    std::reverse(arguments.begin(), arguments.end());
    DISPPARAMS params = {arguments.empty() ? nullptr : arguments.data(), nullptr,
                        static_cast<UINT>(arguments.size()), 0};
    CComVariant result;
    EXCEPINFO exception = {};
    UINT bad = 0;
    HRESULT hr = object->Invoke(id, IID_NULL, LOCALE_USER_DEFAULT, flags,
                                &params, &result, &exception, &bad);
    if (exception.bstrSource) SysFreeString(exception.bstrSource);
    if (exception.bstrDescription) SysFreeString(exception.bstrDescription);
    if (exception.bstrHelpFile) SysFreeString(exception.bstrHelpFile);
    if (FAILED(hr)) throw std::runtime_error("AutoCAD 读取图纸失败，请检查文件及版本。");
    return result;
}
CComVariant Get(IDispatch* object, const wchar_t* name) {
    return Call(object, name, DISPATCH_PROPERTYGET);
}
CComPtr<IDispatch> Dispatch(const CComVariant& value) {
    CComPtr<IDispatch> p;
    if (value.vt == VT_DISPATCH) p = value.pdispVal;
    else if (value.vt == VT_UNKNOWN && value.punkVal)
        value.punkVal->QueryInterface(IID_IDispatch, reinterpret_cast<void**>(&p));
    if (!p) throw std::runtime_error("AutoCAD 对象无效。");
    return p;
}
std::wstring String(const CComVariant& value) {
    CComVariant copy(value);
    if (FAILED(copy.ChangeType(VT_BSTR))) throw std::runtime_error("AutoCAD 文本无效。");
    return copy.bstrVal ? std::wstring(copy.bstrVal) : std::wstring();
}
double Number(const CComVariant& value) {
    CComVariant copy(value);
    if (FAILED(copy.ChangeType(VT_R8))) throw std::runtime_error("AutoCAD 数值无效。");
    return copy.dblVal;
}
std::vector<double> Numbers(const CComVariant& value) {
    if (!(value.vt & VT_ARRAY) || !value.parray) throw std::runtime_error("AutoCAD 坐标无效。");
    SAFEARRAY* array = value.parray;
    if (SafeArrayGetDim(array) != 1) throw std::runtime_error("AutoCAD 坐标维度无效。");
    LONG low = 0, high = -1;
    SafeArrayGetLBound(array, 1, &low);
    SafeArrayGetUBound(array, 1, &high);
    if (high - low > 10000) throw std::runtime_error("图纸坐标过多。");
    std::vector<double> result;
    for (LONG i = low; i <= high; ++i) {
        CComVariant item;
        VARTYPE type = VT_EMPTY;
        SafeArrayGetVartype(array, &type);
        if (type == VT_R8) {
            double number = 0;
            if (FAILED(SafeArrayGetElement(array, &i, &number))) throw std::runtime_error("读取坐标失败。");
            result.push_back(number);
        } else if (type == VT_VARIANT) {
            if (FAILED(SafeArrayGetElement(array, &i, &item))) throw std::runtime_error("读取坐标失败。");
            result.push_back(Number(item));
        } else throw std::runtime_error("不支持的图纸坐标类型。");
    }
    return result;
}
template <typename F> void Each(IDispatch* collection, F action) {
    int count = static_cast<int>(Number(Get(collection, L"Count")));
    if (count < 0 || count > 10000) throw std::runtime_error("DWG 图元数量超出范围。");
    for (int i = 0; i < count; ++i)
        action(Dispatch(Call(collection, L"Item", DISPATCH_METHOD, {CComVariant(i)})));
}
std::string Utf8(const std::wstring& value) {
    int count = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()),
                                    nullptr, 0, nullptr, nullptr);
    std::string result(count, '\0');
    if (count) WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()),
                                   result.data(), count, nullptr, nullptr);
    return result;
}
double Distance(Point a, Point b) { return std::hypot(a.x-b.x, a.z-b.z); }
struct Curve { std::vector<Point> points; int a = -1, b = -1; };
Point Xy(const CComVariant& value) {
    auto p = Numbers(value);
    if (p.size() < 2 || (p.size() > 2 && std::abs(p[2]) > joinTol))
        throw std::runtime_error("只支持 XY 平面中的刀具截面。");
    return {p[0], p[1]};
}
std::vector<Point> Arc(IDispatch* object) {
    auto normal = Numbers(Get(object, L"Normal"));
    if (normal.size() < 3 || std::abs(normal[0]) > joinTol ||
        std::abs(normal[1]) > joinTol || std::abs(normal[2]-1) > joinTol)
        throw std::runtime_error("圆弧不在 XY 平面。");
    const Point center = Xy(Get(object, L"Center"));
    const double radius = Number(Get(object, L"Radius"));
    const double start = Number(Get(object, L"StartAngle"));
    const double finish = Number(Get(object, L"EndAngle"));
    if (!std::isfinite(radius) || radius <= 0 || radius > 10000)
        throw std::runtime_error("圆弧半径无效。");
    double sweep = std::fmod(finish-start, 2*pi);
    if (sweep <= 0) sweep += 2*pi;
    if (sweep >= 2*pi-1e-9) throw std::runtime_error("整圆不能直接作为刀具轮廓。");
    const double step = 2*std::acos(1-std::min(sagitta/radius, 1.0));
    std::vector<double> limits = {start, start+sweep};
    for (int k=-16; k<=16; ++k) {
        const double angle=k*pi/2;
        if (angle>start+1e-9 && angle<start+sweep-1e-9) limits.push_back(angle);
    }
    std::sort(limits.begin(), limits.end());
    std::vector<Point> result;
    for (size_t j=1; j<limits.size(); ++j) {
        int n=static_cast<int>(std::ceil((limits[j]-limits[j-1])/step));
        if (n <= 0 || n > 256) throw std::runtime_error("圆弧离散点超过刀具限制。");
        for (int i=0; i<n; ++i) {
            double angle=limits[j-1]+(limits[j]-limits[j-1])*i/n;
            result.push_back({center.x+radius*std::cos(angle), center.z+radius*std::sin(angle)});
        }
    }
    result.push_back({center.x+radius*std::cos(start+sweep),
                      center.z+radius*std::sin(start+sweep)});
    return result;
}
void AppendPolyline(IDispatch* object, std::vector<Curve>& curves) {
    auto coordinates = Numbers(Get(object, L"Coordinates"));
    if (coordinates.size()%2 || coordinates.size()<4 || coordinates.size()>512)
        throw std::runtime_error("多段线点数无效。");
    const bool closed = Number(Get(object, L"Closed")) != 0;
    size_t count=coordinates.size()/2;
    for (size_t i=0; i<count-(closed ? 0 : 1); ++i) {
        double bulge=Number(Call(object, L"GetBulge", DISPATCH_METHOD,
                                  {CComVariant(static_cast<int>(i))}));
        if (std::abs(bulge)>1e-12) throw std::runtime_error("带圆弧的多段线暂不支持，请先在 CAD 中分解为线与圆弧。");
        size_t next=(i+1)%count;
        curves.push_back({{{coordinates[2*i],coordinates[2*i+1]},
                           {coordinates[2*next],coordinates[2*next+1]}}});
    }
}
Tool Convert(const std::wstring& name, IDispatch* entities) {
    std::vector<Curve> curves;
    Each(entities, [&](CComPtr<IDispatch> entity) {
        auto layer=String(Get(entity, L"Layer"));
        std::transform(layer.begin(), layer.end(), layer.begin(), towupper);
        if (layer==L"CENTER" || layer==L"CENTRE" || layer==L"中心线") return;
        auto type=String(Get(entity, L"ObjectName"));
        if (type==L"AcDbLine")
            curves.push_back({{Xy(Get(entity,L"StartPoint")), Xy(Get(entity,L"EndPoint"))}});
        else if (type==L"AcDbArc") curves.push_back({Arc(entity)});
        else if (type==L"AcDbPolyline") AppendPolyline(entity,curves);
        else if (type==L"AcDbText" || type==L"AcDbMText" || type.find(L"Dimension")!=std::wstring::npos)
            return;
        else throw std::runtime_error("包含不支持的实体类型。");
    });
    if (curves.size()<3 || curves.size()>256) throw std::runtime_error("需要一个 3～256 条边的闭合刀具轮廓。");
    std::vector<Curve> unique;
    for (const auto& c:curves) {
        if (c.points.size()<2 || Distance(c.points.front(),c.points.back())<joinTol)
            throw std::runtime_error("刀具轮廓有零长边。");
        auto same=[&](const Curve& old) {
            if(c.points.size()!=old.points.size())return false;
            bool forward=true,backward=true;
            for(size_t j=0;j<c.points.size();++j){
                forward &= Distance(c.points[j],old.points[j])<joinTol;
                backward &= Distance(c.points[j],old.points[c.points.size()-1-j])<joinTol;
            }
            return forward||backward;
        };
        if (std::none_of(unique.begin(),unique.end(),same)) unique.push_back(c);
    }
    curves=std::move(unique);
    std::vector<Point> vertices;
    std::vector<std::vector<size_t>> graph;
    auto vertex=[&](Point p) {
        for (size_t i=0;i<vertices.size();++i) if (Distance(p,vertices[i])<joinTol)
            return static_cast<int>(i);
        vertices.push_back(p); graph.emplace_back(); return static_cast<int>(vertices.size()-1);
    };
    for (size_t i=0;i<curves.size();++i) {
        auto& c=curves[i]; c.a=vertex(c.points.front());c.b=vertex(c.points.back());
        if (c.a==c.b) throw std::runtime_error("刀具轮廓有零长边。");
        graph[c.a].push_back(i);graph[c.b].push_back(i);
    }
    for (const auto& edges:graph) if (edges.size()!=2)
        throw std::runtime_error("刀具轮廓未闭合、分叉或含有多条轮廓。");
    std::vector<Point> points;
    std::vector<bool> used(curves.size());
    int current=0;
    for (size_t step=0;step<curves.size();++step) {
        size_t i=used[graph[current][0]]?graph[current][1]:graph[current][0];
        if (used[i]) throw std::runtime_error("刀具轮廓不连续。");
        auto seq=curves[i].points;
        if (curves[i].b==current) std::reverse(seq.begin(),seq.end());
        seq.front()=vertices[current];
        int next=curves[i].a==current?curves[i].b:curves[i].a;
        seq.back()=vertices[next];
        points.insert(points.end(),seq.begin(),seq.end()-1);
        used[i]=true;current=next;
    }
    if (current!=0 || points.size()>256) throw std::runtime_error("轮廓未闭合或点数超过 256。");
    double lowest=points.front().z;
    for (Point p:points) lowest=std::min(lowest,p.z);
    std::vector<size_t> bottom;
    for (size_t i=0;i<points.size();++i) if (std::abs(points[i].z-lowest)<joinTol) bottom.push_back(i);
    Point datum;
    if (bottom.size()==1) datum=points[bottom.front()];
    else {
        // For a flat punch, use the midpoint of the widest bottom edge.
        double width=0;size_t edge=points.size();
        for (size_t i=0;i<points.size();++i) {
            Point a=points[i],b=points[(i+1)%points.size()];
            if (std::abs(a.z-lowest)<joinTol && std::abs(b.z-lowest)<joinTol &&
                std::abs(a.x-b.x)>width) {width=std::abs(a.x-b.x);edge=i;}
        }
        if (edge==points.size()) throw std::runtime_error("底部定位点不明确。");
        datum={(points[edge].x+points[(edge+1)%points.size()].x)/2,lowest};
        if (Distance(datum,points[edge])>joinTol &&
            Distance(datum,points[(edge+1)%points.size()])>joinTol)
            points.insert(points.begin()+edge+1,datum);
    }
    Tool tool;
    tool.name="DWG "+Utf8(name);
    for (Point p:points) {
        p.x=std::round((p.x-datum.x)*1e8)/1e8;
        p.z=std::round((p.z-datum.z)*1e8)/1e8;
        if (std::abs(p.x)<1e-8) p.x=0;
        if (std::abs(p.z)<1e-8) p.z=0;
        tool.profile.push_back(p);
    }
    ValidateTool(tool);
    return tool;
}
}

std::vector<DwgCandidate> ReadDwgCandidates(const std::filesystem::path& dwg) {
    if (dwg.extension()!=L".dwg" && dwg.extension()!=L".DWG")
        throw std::runtime_error("请选择 DWG 图纸。");
    if (!std::filesystem::is_regular_file(dwg)) throw std::runtime_error("找不到 DWG 图纸。");
    if (std::filesystem::file_size(dwg)>64*1024*1024)
        throw std::runtime_error("DWG 大于 64 MB，请单独整理刀具轮廓。");
    ComScope com;
    CLSID id;
    if (FAILED(CLSIDFromProgID(L"AutoCAD.Application",&id)))
        throw std::runtime_error("导入 DWG 需要本机安装 AutoCAD。");
    CComPtr<IUnknown> unknown;
    HRESULT hr=GetActiveObject(id,nullptr,&unknown);
    if (FAILED(hr)) hr=CoCreateInstance(id,nullptr,CLSCTX_LOCAL_SERVER,IID_IUnknown,
                                       reinterpret_cast<void**>(&unknown));
    if (FAILED(hr)) throw std::runtime_error("无法启动 AutoCAD，请先打开 AutoCAD 后重试。");
    CComPtr<IDispatch> app;
    if (FAILED(unknown->QueryInterface(IID_IDispatch,reinterpret_cast<void**>(&app))))
        throw std::runtime_error("AutoCAD 接口不可用。");
    auto version=String(Get(app,L"Version"));
    auto major=version.substr(0,version.find(L'.'));
    auto db=Dispatch(Call(app,L"GetInterfaceObject",DISPATCH_METHOD,
                          {CComVariant((L"ObjectDBX.AxDbDocument."+major).c_str())}));
    auto temp=std::filesystem::temp_directory_path()/
              (L"ZeWanMoNi_DWG_"+std::to_wstring(GetCurrentProcessId())+L"_"+
               std::to_wstring(GetTickCount64()));
    std::filesystem::create_directories(temp);
    struct Cleanup {std::filesystem::path path;~Cleanup(){std::error_code ec;std::filesystem::remove_all(path,ec);}} cleanup{temp};
    auto snapshot=temp/L"source.dwg";
    std::filesystem::copy_file(dwg,snapshot);
    Call(db,L"Open",DISPATCH_METHOD,{CComVariant(snapshot.c_str())});
    std::vector<DwgCandidate> results;
    auto blocks=Dispatch(Get(db,L"Blocks"));
    Each(blocks,[&](CComPtr<IDispatch> block) {
        auto name=String(Get(block,L"Name"));
        if (name.empty() || name[0]==L'*') return;
        try {results.push_back({Convert(name,block),name});}
        catch (const std::exception&) { /* The drawing may contain annotations, dies and hardware. */ }
    });
    try {results.push_back({Convert(L"模型空间",Dispatch(Get(db,L"ModelSpace"))),L"模型空间"});}
    catch (const std::exception&) { /* Mixed model space is not a single tool. */ }
    if (results.empty()) throw std::runtime_error("没有识别到单一闭合刀具轮廓；请在 CAD 中把一把上刀整理为独立图块或单个模型空间轮廓，单位为毫米。");
    if (results.size()>100) throw std::runtime_error("候选轮廓超过 100 个，请缩小 DWG 范围。");
    return results;
}
}
