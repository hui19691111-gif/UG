#include "BendSimulation.hpp"
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace bend_sim {
std::string Utf8(const std::wstring& value){
    if(value.empty())return {};
    const int length=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),nullptr,0,nullptr,nullptr);
    if(length<=0)throw std::runtime_error("无法转换 Unicode 刀具名称。");
    std::string result(static_cast<size_t>(length),'\0');
    if(WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),result.data(),length,nullptr,nullptr)!=length)
        throw std::runtime_error("无法转换 Unicode 刀具名称。");
    return result;
}
std::filesystem::path ArchiveTool(const std::filesystem::path& path,const std::filesystem::path& directory){
    if(path.parent_path()!=directory||path.extension()!=L".ztool"||!std::filesystem::is_regular_file(path))
        throw std::runtime_error("刀具文件位置无效，未删除。");
    const auto archive=directory/L"已删除";
    std::filesystem::create_directories(archive);
    auto target=archive/path.filename();
    if(std::filesystem::exists(target)){
        for(int suffix=1;suffix<10000;++suffix){
            auto backup=archive/std::to_wstring(suffix);
            target=backup/path.filename();
            if(!std::filesystem::exists(target)){std::filesystem::create_directories(backup);break;}
            if(suffix==9999)throw std::runtime_error("已删除刀具备份目录已满。");
        }
    }
    std::filesystem::rename(path,target);
    return target;
}
double Dot(Vec a,Vec b){return a.x*b.x+a.y*b.y+a.z*b.z;}
Vec Cross(Vec a,Vec b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
Vec Unit(Vec a){double d=std::sqrt(Dot(a,a));if(d<1e-12)throw std::runtime_error("无法确定方向，请检查折弯区域。");return a*(1/d);}
namespace {
double Turn(Point a,Point b,Point c){return (b.x-a.x)*(c.z-a.z)-(b.z-a.z)*(c.x-a.x);}
bool On(Point a,Point b,Point c){return std::abs(Turn(a,b,c))<1e-9&&c.x>=std::min(a.x,b.x)-1e-9&&c.x<=std::max(a.x,b.x)+1e-9&&c.z>=std::min(a.z,b.z)-1e-9&&c.z<=std::max(a.z,b.z)+1e-9;}
bool Intersects(Point a,Point b,Point c,Point d){
    double x=Turn(a,b,c),y=Turn(a,b,d),z=Turn(c,d,a),w=Turn(c,d,b);
    return (x*y<0&&z*w<0)||On(a,b,c)||On(a,b,d)||On(c,d,a)||On(c,d,b);
}
}
void ValidateTool(const Tool& t){
    if(t.name.empty()||t.name.size()>160||t.profile.size()<3||t.profile.size()>256)throw std::runtime_error("刀具名称或截面点数无效（3～256 点）。");
    double area=0;bool origin=false;
    for(size_t i=0;i<t.profile.size();++i){
        auto a=t.profile[i],b=t.profile[(i+1)%t.profile.size()];
        if(!std::isfinite(a.x)||!std::isfinite(a.z)||std::abs(a.x)>10000||a.z<0||a.z>10000)throw std::runtime_error("刀具截面坐标无效：单位毫米，Z 不得小于零。");
        if(std::hypot(a.x-b.x,a.z-b.z)<1e-6)throw std::runtime_error("刀具截面有重复点，请不要重复首点闭合。");
        origin=origin||(std::abs(a.x)<1e-9&&std::abs(a.z)<1e-9);
        area+=a.x*b.z-b.x*a.z;
        for(size_t j=i+1;j<t.profile.size();++j){
            if(j==i+1||(i==0&&j==t.profile.size()-1))continue;
            if(Intersects(a,b,t.profile[j],t.profile[(j+1)%t.profile.size()]))throw std::runtime_error("刀具截面自交或重叠。");
        }
    }
    if(!origin||std::abs(area)<1e-6)throw std::runtime_error("刀具截面必须包含刀尖原点 (0,0)，且面积不能为零。");
}
std::vector<Tool> BuiltinTools(){
    // Independently designed illustrative sections; replace with measured tooling for production checks.
    return {{"示例直刀 90° / 高60",{{0,0},{8,8},{8,60},{-8,60},{-8,8}}},
            {"示例窄刀 60° / 高60",{{0,0},{6,10.3923048454},{6,60},{-6,60},{-6,10.3923048454}}},
            {"示例偏置刀 / 高80",{{0,0},{5,5},{5,18},{-12,35},{-12,80},{-25,80},{-25,32},{-5,12},{-5,5}}}};
}
Tool ReadTool(const std::filesystem::path& path){
    std::ifstream in(path);if(!in)throw std::runtime_error("无法读取自定义刀具文件。");
    if(std::filesystem::file_size(path)>65536)throw std::runtime_error("刀具文件超过 64 KB。");
    Tool t;std::string line;bool header=false;
    while(std::getline(in,line)){
        if(!line.empty()&&line.back()=='\r')line.pop_back();
        if(line.rfind("\xef\xbb\xbf",0)==0)line.erase(0,3);
        if(line.empty()||line[0]=='#')continue;
        if(!header){if(line!="ZH_TOOL_V1")throw std::runtime_error("刀具文件必须以 ZH_TOOL_V1 开头。");header=true;continue;}
        if(line.rfind("name=",0)==0){if(!t.name.empty())throw std::runtime_error("刀具名称重复。");t.name=line.substr(5);continue;}
        std::replace(line.begin(),line.end(),',',' ');std::istringstream row(line);row.imbue(std::locale::classic());Point p;std::string extra;
        if(!(row>>p.x>>p.z)||(row>>extra))throw std::runtime_error("刀具点格式应为 X,Z；每行一个点。");
        t.profile.push_back(p);if(t.profile.size()>256)throw std::runtime_error("刀具点数超过 256。");
    }
    ValidateTool(t);return t;
}
void RenameTool(const std::filesystem::path& path,const std::string& name){
    if(name.empty()||name.size()>160||name.front()==' '||name.back()==' '||
       std::any_of(name.begin(),name.end(),[](unsigned char c){return c<32||c==127;}))
        throw std::runtime_error("刀具名称不能为空、超过 160 字节或包含首尾空格与控制字符。");
    Tool original=ReadTool(path);
    if(original.name==name)return;
    Tool renamed=original;renamed.name=name;ValidateTool(renamed);
    std::ifstream input(path,std::ios::binary);
    if(!input)throw std::runtime_error("无法读取刀具文件。");
    std::string data((std::istreambuf_iterator<char>(input)),std::istreambuf_iterator<char>());
    input.close();
    size_t valueStart=std::string::npos,valueEnd=std::string::npos;
    for(size_t start=0;start<data.size();){
        size_t end=data.find('\n',start),lineEnd=end==std::string::npos?data.size():end;
        if(lineEnd>start&&data[lineEnd-1]=='\r')--lineEnd;
        if(data.compare(start,5,"name=")==0){
            if(valueStart!=std::string::npos)throw std::runtime_error("刀具文件存在重复名称行。");
            valueStart=start+5;valueEnd=lineEnd;
        }
        if(end==std::string::npos)break;
        start=end+1;
    }
    if(valueStart==std::string::npos)throw std::runtime_error("刀具文件缺少名称行。");
    data.replace(valueStart,valueEnd-valueStart,name);
    auto temporary=path;
    temporary+=L".rename-"+std::to_wstring(GetCurrentProcessId())+L"-"+
               std::to_wstring(GetTickCount64())+L".tmp";
    if(std::filesystem::exists(temporary))throw std::runtime_error("刀具临时文件已存在，请重试。");
    try{
        {
            std::ofstream output(temporary,std::ios::binary|std::ios::trunc);
            output.write(data.data(),static_cast<std::streamsize>(data.size()));
            if(!output)throw std::runtime_error("写入刀具名称失败。");
        }
        Tool checked=ReadTool(temporary);
        if(checked.name!=name||checked.profile.size()!=original.profile.size())
            throw std::runtime_error("刀具名称写入校验失败。");
        for(size_t i=0;i<original.profile.size();++i)
            if(checked.profile[i].x!=original.profile[i].x||
               checked.profile[i].z!=original.profile[i].z)
                throw std::runtime_error("刀具截面写入校验失败。");
        if(!ReplaceFileW(path.c_str(),temporary.c_str(),nullptr,
                         REPLACEFILE_IGNORE_MERGE_ERRORS,nullptr,nullptr))
            throw std::runtime_error("无法替换刀具文件，请检查写入权限。");
    }catch(...){std::error_code ec;std::filesystem::remove(temporary,ec);throw;}
}
Placement Place(const Bend& b,const Tool& t,const Settings& s){
    ValidateTool(t);
    for(double v:{s.length,s.axial,s.lateral,s.lift,s.tilt,s.innerRadius,s.safeGap})if(!std::isfinite(v))throw std::runtime_error("参数必须是有限数值。");
    if(s.length<0||s.length>100000||s.safeGap<0||s.safeGap>1000||std::abs(s.tilt)>90||std::abs(s.axial)>100000||std::abs(s.lateral)>10000||std::abs(s.lift)>10000)throw std::runtime_error("刀具长度、偏移、倾角或间隙超出范围。");
    Placement p;p.bend=b;p.tool=t;p.length=s.length==0?b.length:s.length*b.unitsPerMm;
    if(p.length<=b.tolerance*2)throw std::runtime_error("刀具长度太小。");
    p.origin=b.tip+(b.axis*s.axial+b.x*s.lateral+b.up*s.lift)*b.unitsPerMm;
    double a=s.tilt*3.14159265358979323846/180,c=std::cos(a),d=std::sin(a);
    p.x=b.x*c+b.up*d;p.up=b.up*c-b.x*d;p.axis=b.axis;
    if(s.reverse){p.x=p.x*-1;p.axis=p.axis*-1;}
    return p;
}
std::vector<std::pair<Vec,Vec>> Outline(const Placement& p){
    std::vector<std::pair<Vec,Vec>> lines;
    auto at=[&](Point q,double y){return p.origin+p.x*(q.x*p.bend.unitsPerMm)+p.up*(q.z*p.bend.unitsPerMm)+p.axis*y;};
    for(size_t i=0;i<p.tool.profile.size();++i){auto a=p.tool.profile[i],b=p.tool.profile[(i+1)%p.tool.profile.size()];
        lines.emplace_back(at(a,-p.length/2),at(b,-p.length/2));lines.emplace_back(at(a,p.length/2),at(b,p.length/2));lines.emplace_back(at(a,-p.length/2),at(a,p.length/2));}
    return lines;
}
std::string Describe(const Result& r){
    const char* text=r.status==Status::Interference?"干涉：刀具与零件发生穿透":r.status==Status::Contact?"接触 / 距离在模型公差内":r.status==Status::Near?"间隙不足":"当前姿态无干涉";
    std::ostringstream s;s.imbue(std::locale::classic());s.setf(std::ios::fixed);s.precision(3);s<<text;
    if(r.status!=Status::Interference)s<<"；最小距离 "<<r.distanceMm<<" mm";
    return s.str();
}
}
