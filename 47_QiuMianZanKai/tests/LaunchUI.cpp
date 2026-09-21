// Test-only bootstrap: retain the installed authorization gate at its normal
// location, then call the exact protected Release command from the staging tree.
// This does not skip or replace any authorization check in the command.
#include <Windows.h>
#include <filesystem>
extern "C" __declspec(dllexport) void ufusr(char* param,int* code,int length){
    auto gate=LoadLibraryW(L"D:\\UG智辉钣金插件\\application\\ZhaoFuNxLicenseGate.dll");
    HMODULE self=nullptr;GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&ufusr),&self);
    wchar_t file[MAX_PATH]={};GetModuleFileNameW(self,file,MAX_PATH);
    auto path=std::filesystem::path(file).parent_path().parent_path()/"test-runtime/application/QiuMianZanKai.dll";
    auto module=gate?LoadLibraryW(path.c_str()):nullptr;
    auto entry=module?reinterpret_cast<void(*)(char*,int*,int)>(GetProcAddress(module,"ufusr")):nullptr;
    if(entry)entry(param,code,length);else if(code)*code=-1;
    if(module)FreeLibrary(module);if(gate)FreeLibrary(gate);
}
extern "C" __declspec(dllexport) int ufusr_ask_unload(){return 1;}
