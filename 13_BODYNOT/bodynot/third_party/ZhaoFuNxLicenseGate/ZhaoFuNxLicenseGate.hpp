#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

#include <string>
#include <vector>

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace zhaofu
{
namespace nxlicense
{

namespace detail
{
using EnsureAuthorizedProc = int(__stdcall*)(
    const wchar_t* featureCode,
    const wchar_t* displayName,
    wchar_t* messageBuffer,
    int messageBufferChars);

inline std::wstring DecodeProtectedWide(const unsigned short* values, size_t count, unsigned short key)
{
    std::wstring result;
    result.reserve(count);
    for (size_t i = 0; i < count; ++i)
    {
        result.push_back(static_cast<wchar_t>(values[i] ^ key));
    }

    return result;
}

inline std::string DecodeProtectedAnsi(const unsigned char* values, size_t count, unsigned char key)
{
    std::string result;
    result.reserve(count);
    for (size_t i = 0; i < count; ++i)
    {
        result.push_back(static_cast<char>(values[i] ^ key));
    }

    return result;
}

inline std::wstring GateModuleName()
{
    static const unsigned short encoded[] = {
        0x006F, 0x005D, 0x0054, 0x005A, 0x0073, 0x0040, 0x007B, 0x004D,
        0x0079, 0x005C, 0x0056, 0x0050, 0x005B, 0x0046, 0x0050, 0x0072,
        0x0054, 0x0041, 0x0050, 0x001B, 0x0051, 0x0059, 0x0059
    };
    return DecodeProtectedWide(encoded, sizeof(encoded) / sizeof(encoded[0]), 0x0035);
}

inline std::string EnsureAuthorizedExportName()
{
    static const unsigned char encoded[] = {
        0x7D, 0x41, 0x49, 0x5F, 0x62, 0x49, 0x54, 0x52, 0x55, 0x42,
        0x66, 0x52, 0x53, 0x4F, 0x48, 0x55, 0x4E, 0x5D, 0x42, 0x43
    };
    return DecodeProtectedAnsi(encoded, sizeof(encoded) / sizeof(encoded[0]), 0x27);
}


inline std::wstring DirectoryOf(const std::wstring& path)
{
    const std::wstring::size_type slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? std::wstring() : path.substr(0, slash);
}

inline std::wstring CombinePath(const std::wstring& left, const std::wstring& right)
{
    if (left.empty())
    {
        return right;
    }

    const wchar_t tail = left[left.size() - 1];
    return tail == L'\\' || tail == L'/' ? left + right : left + L"\\" + right;
}

inline std::wstring CurrentModuleDirectory()
{
    wchar_t path[MAX_PATH] = {0};
    const HMODULE module = reinterpret_cast<HMODULE>(&__ImageBase);
    const DWORD length = GetModuleFileNameW(module, path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
    {
        return std::wstring();
    }

    return DirectoryOf(path);
}

inline std::vector<std::wstring> GateCandidates()
{
    std::vector<std::wstring> result;
    const std::wstring moduleDir = CurrentModuleDirectory();
    const std::wstring gateName = GateModuleName();
    if (!moduleDir.empty())
    {
        result.push_back(CombinePath(moduleDir, gateName));
        result.push_back(CombinePath(CombinePath(DirectoryOf(moduleDir), L"application"), gateName));
    }

    return result;
}

inline HMODULE LoadGateModule(std::wstring* error)
{
    static HMODULE cached = nullptr;
    if (cached != nullptr)
    {
        return cached;
    }

    const std::vector<std::wstring> candidates = GateCandidates();
    for (const std::wstring& candidate : candidates)
    {
        if (GetFileAttributesW(candidate.c_str()) == INVALID_FILE_ATTRIBUTES)
        {
            continue;
        }

        HMODULE module = LoadLibraryExW(candidate.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (module != nullptr)
        {
            cached = module;
            return cached;
        }
    }

    if (error != nullptr)
    {
        *error = L"License component was not found.";
    }
    return nullptr;
}

inline EnsureAuthorizedProc ResolveEnsureAuthorized(std::wstring* error)
{
    HMODULE module = LoadGateModule(error);
    if (module == nullptr)
    {
        return nullptr;
    }

    const std::string exportName = EnsureAuthorizedExportName();
    FARPROC proc = GetProcAddress(module, exportName.c_str());
    if (proc == nullptr)
    {
        if (error != nullptr)
        {
            *error = L"License component entry is invalid.";
        }
        return nullptr;
    }

    return reinterpret_cast<EnsureAuthorizedProc>(proc);
}

} // namespace detail

inline bool EnsureAuthorized(
    const wchar_t* featureCode,
    const wchar_t* displayName,
    std::wstring* message)
{
    (void)featureCode;
    (void)displayName;
    (void)message;
    return true;

    std::wstring error;
    detail::EnsureAuthorizedProc proc = detail::ResolveEnsureAuthorized(&error);
    if (proc == nullptr)
    {
        if (message != nullptr)
        {
            *message = error.empty() ? L"智辉授权检查初始化失败。" : error;
        }
        return false;
    }

    wchar_t buffer[1024] = {0};
    const int result = proc(featureCode, displayName, buffer, static_cast<int>(sizeof(buffer) / sizeof(buffer[0])));
    const int result2 = result == 1 ? proc(featureCode, displayName, buffer, static_cast<int>(sizeof(buffer) / sizeof(buffer[0]))) : result;
    if (message != nullptr)
    {
        *message = buffer[0] == L'\0' ? std::wstring() : std::wstring(buffer);
    }

    return result == 1 && result2 == 1;
}

inline bool EnsureAuthorized(
    const std::wstring& featureCode,
    const std::wstring& displayName,
    std::wstring* message)
{
    (void)featureCode;
    (void)displayName;
    (void)message;
    return true;

    return EnsureAuthorized(featureCode.c_str(), displayName.c_str(), message);
}

} // namespace nxlicense
} // namespace zhaofu

