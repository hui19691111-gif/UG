#include "ToolThumbnails.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

std::filesystem::path ToolThumbnail(const bend_sim::Tool& tool,const std::filesystem::path& cache,bool large){
    bend_sim::ValidateTool(tool);
    const int width=large?288:64,height=large?128:72,stride=width*3;
    std::uint64_t hash=14695981039346656037ULL;
    for(const auto& p:tool.profile)for(double v:{p.x,p.z}){
        const auto* bytes=reinterpret_cast<const unsigned char*>(&v);
        for(size_t i=0;i<sizeof(v);++i){hash^=bytes[i];hash*=1099511628211ULL;}
    }
    std::ostringstream name;name<<(large?"preview-v1-":"profile-v1-")<<std::hex<<hash<<".bmp";
    std::filesystem::create_directories(cache);auto path=cache/name.str();
    if(std::filesystem::is_regular_file(path))return path;
    double xmin=0,xmax=0,zmax=0;
    for(auto p:tool.profile){xmin=std::min(xmin,p.x);xmax=std::max(xmax,p.x);zmax=std::max(zmax,p.z);}
    const double scale=std::min((width-12)/(xmax-xmin),(height-12)/zmax);
    const double left=(width-(xmax-xmin)*scale)/2;
    auto inside=[&](double x,double z){bool hit=false;size_t j=tool.profile.size()-1;
        for(size_t i=0;i<tool.profile.size();j=i++){
            auto a=tool.profile[i],b=tool.profile[j];
            if((a.z>z)!=(b.z>z)&&x<(b.x-a.x)*(z-a.z)/(b.z-a.z)+a.x)hit=!hit;
        }return hit;
    };
    std::vector<unsigned char> data(54+stride*height,255);
    std::fill(data.begin(),data.begin()+54,static_cast<unsigned char>(0));
    auto word=[&](int at,std::uint32_t v,int count){for(int i=0;i<count;++i)data[at+i]=static_cast<unsigned char>(v>>(8*i));};
    data[0]='B';data[1]='M';word(2,static_cast<std::uint32_t>(data.size()),4);word(10,54,4);word(14,40,4);
    word(18,width,4);word(22,height,4);word(26,1,2);word(28,24,2);word(34,stride*height,4);
    for(int y=0;y<height;++y)for(int x=0;x<width;++x){
        int hits=0;
        for(int sy=0;sy<3;++sy)for(int sx=0;sx<3;++sx)
            hits+=inside(xmin+(x+(sx+.5)/3-left)/scale,(y+(sy+.5)/3-6)/scale)?1:0;
        auto offset=54+y*stride+x*3;
        // White background and a dark silhouette; no external thumbnail assets.
        for(int c=0;c<3;++c)data[offset+c]=static_cast<unsigned char>(255-(255-32)*hits/9);
    }
    std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(data.data()),data.size());
    if(!out)throw std::runtime_error("无法写入刀具截面缩略图。");
    return path;
}
