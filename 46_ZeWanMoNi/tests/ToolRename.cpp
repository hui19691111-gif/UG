#include "BendSimulation.hpp"
#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace {
std::string Bytes(const std::filesystem::path& path) {
    std::ifstream in(path,std::ios::binary);
    return {std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};
}
}
int main(int argc,char** argv) {
    if(argc!=2)return 2;
    auto temporary=std::filesystem::temp_directory_path()/
        (L"ZeWanMoNi_Rename_"+std::to_wstring(GetCurrentProcessId())+L"_"+
         std::to_wstring(GetTickCount64()));
    try {
        std::filesystem::create_directory(temporary);
        auto source=std::filesystem::u8path(argv[1]);
        auto target=temporary/L"tool.ztool";
        std::filesystem::copy_file(source,target);
        const auto sourceBytes=Bytes(source);
        const auto original=bend_sim::ReadTool(target);
        const std::string name="验证刀具 RW8867";
        bend_sim::RenameTool(target,name);
        const auto renamed=bend_sim::ReadTool(target);
        if(renamed.name!=name||renamed.profile.size()!=original.profile.size()||
           Bytes(source)!=sourceBytes||Bytes(target).find("# source:")==std::string::npos)
            throw std::runtime_error("name or original DWG metadata changed");
        for(size_t i=0;i<original.profile.size();++i)
            if(original.profile[i].x!=renamed.profile[i].x||
               original.profile[i].z!=renamed.profile[i].z)
                throw std::runtime_error("profile changed");
        const auto stable=Bytes(target);
        for(const std::string bad:{std::string(),std::string("bad\nname=injected")}){
            bool rejected=false;
            try{bend_sim::RenameTool(target,bad);}catch(const std::exception&){rejected=true;}
            if(!rejected||Bytes(target)!=stable)throw std::runtime_error("invalid name altered tool");
        }
        std::filesystem::remove_all(temporary);
        std::cout<<"rename persistence and geometry preservation passed\n";
        return 0;
    }catch(const std::exception& e){
        std::error_code ec;std::filesystem::remove_all(temporary,ec);
        std::cerr<<e.what()<<'\n';return 1;
    }
}
