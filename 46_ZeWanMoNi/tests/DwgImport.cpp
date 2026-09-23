#include "DwgImport.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <cmath>

int main(int argc,char** argv) {
    if(argc!=2) return 2;
    try {
        auto candidates=bend_sim::ReadDwgCandidates(std::filesystem::u8path(argv[1]));
        if(candidates.empty()) throw std::runtime_error("no candidates");
        bool flat=false,round=false;
        for(const auto& candidate:candidates) {
            bend_sim::ValidateTool(candidate.tool);
            double minx=0,maxx=0,maxz=0;
            for(const auto& point:candidate.tool.profile){
                minx=std::min(minx,point.x);maxx=std::max(maxx,point.x);
                maxz=std::max(maxz,point.z);
            }
            if(candidate.block==L"004") flat=std::abs(maxx-minx-40)<0.01&&std::abs(maxz-95)<0.01;
            if(candidate.block==L"RZ30104") round=maxz>132&&maxz<133;
            std::cout<<candidate.tool.name<<" "<<candidate.tool.profile.size()<<"\n";
        }
        if(!flat||!round)throw std::runtime_error("known DWG profile dimensions changed");
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
