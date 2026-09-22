// Test bootstrap loads the installed licensing gate, then the exact protected Release DLL.
#include <Windows.h>
#include <filesystem>
extern "C" __declspec(dllexport) void ufusr(char* param,int* code,int length){
    HMODULE gate=LoadLibraryW(L"D:\\UG智辉钣金插件\\application\\ZhaoFuNxLicenseGate.dll"),self=nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&ufusr),&self);
    wchar_t path[MAX_PATH]={};GetModuleFileNameW(self,path,MAX_PATH);
    auto module=gate?LoadLibraryW((std::filesystem::path(path).parent_path()/"FanTonSenZi.dll").c_str()):nullptr;
    auto entry=module?reinterpret_cast<void(*)(char*,int*,int)>(GetProcAddress(module,"ufusr")):nullptr;
    if(entry)entry(param,code,length);else if(code)*code=-1;
    if(module)FreeLibrary(module);if(gate)FreeLibrary(gate);
}
extern "C" __declspec(dllexport) int ufusr_ask_unload(){return 1;}
