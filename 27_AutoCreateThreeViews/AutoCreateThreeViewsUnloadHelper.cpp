#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <uf.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace
{
HMODULE g_module = nullptr;
UINT_PTR g_unloadTimer = 0;

std::filesystem::path ModuleDirectory()
{
    wchar_t path[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(g_module, path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
        return {};
    return std::filesystem::path(path).parent_path();
}

void WriteUnloadLog(const std::string& message)
{
    const std::filesystem::path logPath = ModuleDirectory() / L"AutoCreateThreeViews.log";
    std::ofstream output(logPath, std::ios::app | std::ios::binary);
    if (output)
        output << message << '\n';
}

std::string LocalePath(const std::filesystem::path& path)
{
    const std::wstring wide = path.wstring();
    const int count = WideCharToMultiByte(CP_ACP, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (count <= 1)
        return {};
    std::string result(static_cast<size_t>(count), '\0');
    WideCharToMultiByte(CP_ACP, 0, wide.c_str(), -1, result.data(), count, nullptr, nullptr);
    result.pop_back();
    return result;
}

void CALLBACK DeferredUnloadTimerProc(HWND, UINT, UINT_PTR timerId, DWORD)
{
    KillTimer(nullptr, timerId);
    g_unloadTimer = 0;

    const int initializeStatus = UF_initialize();
    if (initializeStatus != 0)
    {
        WriteUnloadLog(
            "AutoCreateThreeViews: deferred unload UF_initialize failed, status=" +
            std::to_string(initializeStatus) + ".");
        return;
    }

    const std::filesystem::path targetPath = ModuleDirectory() / L"AutoCreateThreeViews.dll";
    std::string target = LocalePath(targetPath);
    const int unloadStatus = target.empty() ? -1 : UF_unload_library(target.data());
    WriteUnloadLog(
        "AutoCreateThreeViews: deferred main DLL unload after 500 ms, status=" +
        std::to_string(unloadStatus) + ", path=" + target + ".");
    UF_terminate();
}
}

extern "C" __declspec(dllexport) void ScheduleAutoCreateThreeViewsUnload()
{
    if (g_unloadTimer != 0)
        KillTimer(nullptr, g_unloadTimer);

    g_unloadTimer = SetTimer(nullptr, 0, 500, DeferredUnloadTimerProc);
    WriteUnloadLog(
        g_unloadTimer != 0
            ? "AutoCreateThreeViews: deferred main DLL unload scheduled for 500 ms."
            : "AutoCreateThreeViews: deferred main DLL unload SetTimer failed.");
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        g_module = instance;
        DisableThreadLibraryCalls(instance);
    }
    else if (reason == DLL_PROCESS_DETACH && g_unloadTimer != 0)
    {
        KillTimer(nullptr, g_unloadTimer);
        g_unloadTimer = 0;
    }
    return TRUE;
}
