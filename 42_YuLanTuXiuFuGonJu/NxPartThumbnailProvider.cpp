#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <ShObjIdl.h>
#include <Shlwapi.h>
#include <thumbcache.h>
#include <wincodec.h>

#include <algorithm>
#include <atomic>
#include <new>
#include <string>
#include <vector>

// {8C785D42-59FC-42B1-A97A-6A7F9AB11574}
const CLSID CLSID_NxPartThumbnailProvider =
{0x8c785d42, 0x59fc, 0x42b1, {0xa9, 0x7a, 0x6a, 0x7f, 0x9a, 0xb1, 0x15, 0x74}};

namespace
{
std::atomic<long> gObjects{0};
std::atomic<long> gLocks{0};

void Trace(const wchar_t* format, ...)
{
    wchar_t line[1024] = {};
    va_list args; va_start(args, format); _vsnwprintf_s(line, _TRUNCATE, format, args); va_end(args);
    HANDLE file = CreateFileW(L"D:\\UGZhiHuiLogs\\NxPartThumbnailProvider.log", FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE)
    {
        char utf8[3072] = {};
        int count = WideCharToMultiByte(CP_UTF8, 0, line, -1, utf8, sizeof(utf8) - 2, nullptr, nullptr);
        if (count > 0) { utf8[count - 1] = '\r'; utf8[count] = '\n'; DWORD written = 0; WriteFile(file, utf8, count + 1, &written, nullptr); }
        CloseHandle(file);
    }
}

class ComInit
{
public:
    ComInit() : result(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)) {}
    ~ComInit() { if (SUCCEEDED(result)) CoUninitialize(); }
    HRESULT result;
};

bool ReadTail(const std::wstring& path, std::vector<BYTE>& data)
{
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0) { CloseHandle(file); return false; }
    constexpr LONGLONG maxTail = 32LL * 1024 * 1024;
    const LONGLONG count = std::min(size.QuadPart, maxTail);
    LARGE_INTEGER offset{}; offset.QuadPart = size.QuadPart - count;
    SetFilePointerEx(file, offset, nullptr, FILE_BEGIN);
    data.resize(static_cast<size_t>(count));
    DWORD read = 0;
    const bool ok = ReadFile(file, data.data(), static_cast<DWORD>(data.size()), &read, nullptr) != FALSE;
    CloseHandle(file);
    data.resize(read);
    return ok && !data.empty();
}

HRESULT DecodeBestJpeg(const std::vector<BYTE>& data, UINT requested, HBITMAP* bitmap)
{
    IWICImagingFactory* factory = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory));
    if (FAILED(hr)) return hr;

    IWICBitmapSource* best = nullptr;
    UINT bestWidth = 0, bestHeight = 0;
    for (size_t start = 0; start + 16 < data.size(); ++start)
    {
        if (data[start] != 0xFF || data[start + 1] != 0xD8 || data[start + 2] != 0xFF) continue;
        const BYTE endMarker[] = {0xFF, 0xD9};
        auto endIt = std::search(data.begin() + static_cast<ptrdiff_t>(start + 3), data.end(),
            std::begin(endMarker), std::end(endMarker));
        if (endIt == data.end()) continue;
        const size_t length = static_cast<size_t>(endIt - (data.begin() + static_cast<ptrdiff_t>(start))) + 2;
        IStream* stream = SHCreateMemStream(data.data() + start, static_cast<UINT>(length));
        if (!stream) continue;
        IWICBitmapDecoder* decoder = nullptr;
        IWICBitmapFrameDecode* frame = nullptr;
        IWICBitmap* cached = nullptr;
        UINT width = 0, height = 0;
        if (SUCCEEDED(factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnLoad, &decoder)) &&
            SUCCEEDED(decoder->GetFrame(0, &frame)) && SUCCEEDED(frame->GetSize(&width, &height)) &&
            SUCCEEDED(factory->CreateBitmapFromSource(frame, WICBitmapCacheOnLoad, &cached)) &&
            width >= 32 && height >= 32 && static_cast<unsigned long long>(width) * height >
                static_cast<unsigned long long>(bestWidth) * bestHeight)
        {
            if (best) best->Release();
            best = cached; best->AddRef(); bestWidth = width; bestHeight = height;
        }
        if (cached) cached->Release();
        if (frame) frame->Release();
        if (decoder) decoder->Release();
        stream->Release();
        start += length - 1;
    }
    if (!best) { factory->Release(); return HRESULT_FROM_WIN32(ERROR_NOT_FOUND); }
    Trace(L"found native jpeg %ux%u requested=%u", bestWidth, bestHeight, requested);

    const double scale = std::min(1.0, static_cast<double>(requested) / std::max(bestWidth, bestHeight));
    const UINT outWidth = std::max(1U, static_cast<UINT>(bestWidth * scale));
    const UINT outHeight = std::max(1U, static_cast<UINT>(bestHeight * scale));
    IWICBitmapScaler* scaler = nullptr;
    IWICFormatConverter* converter = nullptr;
    hr = factory->CreateBitmapScaler(&scaler);
    if (SUCCEEDED(hr)) hr = scaler->Initialize(best, outWidth, outHeight, WICBitmapInterpolationModeFant);
    if (SUCCEEDED(hr)) hr = factory->CreateFormatConverter(&converter);
    if (SUCCEEDED(hr)) hr = converter->Initialize(scaler, GUID_WICPixelFormat32bppBGRA,
        WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);

    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = static_cast<LONG>(outWidth);
    info.bmiHeader.biHeight = -static_cast<LONG>(outHeight);
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* pixels = nullptr;
    HBITMAP result = nullptr;
    if (SUCCEEDED(hr)) result = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    if (!result) hr = HRESULT_FROM_WIN32(GetLastError());
    if (SUCCEEDED(hr))
    {
        hr = converter->CopyPixels(nullptr, outWidth * 4, outWidth * outHeight * 4,
            static_cast<BYTE*>(pixels));
        if (FAILED(hr)) { DeleteObject(result); result = nullptr; }
    }
    if (converter) converter->Release();
    if (scaler) scaler->Release();
    best->Release();
    factory->Release();
    if (SUCCEEDED(hr)) *bitmap = result;
    Trace(L"decode result hr=0x%08X bitmap=%p", static_cast<unsigned int>(hr), result);
    return hr;
}

class ThumbnailProvider final : public IInitializeWithFile, public IThumbnailProvider
{
public:
    ThumbnailProvider() { ++gObjects; }
    ~ThumbnailProvider() { --gObjects; }
    IFACEMETHODIMP QueryInterface(REFIID iid, void** value) override
    {
        if (!value) return E_POINTER;
        *value = nullptr;
        if (iid == IID_IUnknown || iid == IID_IInitializeWithFile)
            *value = static_cast<IInitializeWithFile*>(this);
        else if (iid == IID_IThumbnailProvider)
            *value = static_cast<IThumbnailProvider*>(this);
        else return E_NOINTERFACE;
        AddRef(); return S_OK;
    }
    IFACEMETHODIMP_(ULONG) AddRef() override { return ++references; }
    IFACEMETHODIMP_(ULONG) Release() override
    {
        const ULONG count = --references; if (!count) delete this; return count;
    }
    IFACEMETHODIMP Initialize(LPCWSTR filePath, DWORD) override
    {
        if (!filePath) return E_INVALIDARG;
        if (!path.empty()) return HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED);
        path = filePath; return S_OK;
    }
    IFACEMETHODIMP GetThumbnail(UINT size, HBITMAP* bitmap, WTS_ALPHATYPE* alpha) override
    {
        if (!bitmap || !alpha || path.empty()) return E_INVALIDARG;
        *bitmap = nullptr; *alpha = WTSAT_RGB;
        Trace(L"request %s size=%u", path.c_str(), size);
        ComInit com;
        std::vector<BYTE> data;
        if (!ReadTail(path, data)) { HRESULT hr = HRESULT_FROM_WIN32(GetLastError()); Trace(L"read failed hr=0x%08X", static_cast<unsigned int>(hr)); return hr; }
        HRESULT hr = DecodeBestJpeg(data, size, bitmap);
        Trace(L"request done bytes=%zu hr=0x%08X", data.size(), static_cast<unsigned int>(hr));
        return hr;
    }
private:
    std::atomic<ULONG> references{1};
    std::wstring path;
};

class ClassFactory final : public IClassFactory
{
public:
    IFACEMETHODIMP QueryInterface(REFIID iid, void** value) override
    {
        if (!value) return E_POINTER; *value = nullptr;
        if (iid != IID_IUnknown && iid != IID_IClassFactory) return E_NOINTERFACE;
        *value = static_cast<IClassFactory*>(this); AddRef(); return S_OK;
    }
    IFACEMETHODIMP_(ULONG) AddRef() override { return ++references; }
    IFACEMETHODIMP_(ULONG) Release() override { ULONG c = --references; if (!c) delete this; return c; }
    IFACEMETHODIMP CreateInstance(IUnknown* outer, REFIID iid, void** value) override
    {
        if (outer) return CLASS_E_NOAGGREGATION;
        auto* provider = new (std::nothrow) ThumbnailProvider();
        if (!provider) return E_OUTOFMEMORY;
        HRESULT hr = provider->QueryInterface(iid, value); provider->Release(); return hr;
    }
    IFACEMETHODIMP LockServer(BOOL lock) override { lock ? ++gLocks : --gLocks; return S_OK; }
private:
    std::atomic<ULONG> references{1};
};
}

STDAPI DllGetClassObject(REFCLSID clsid, REFIID iid, void** value)
{
    if (clsid != CLSID_NxPartThumbnailProvider) return CLASS_E_CLASSNOTAVAILABLE;
    auto* factory = new (std::nothrow) ClassFactory();
    if (!factory) return E_OUTOFMEMORY;
    HRESULT hr = factory->QueryInterface(iid, value); factory->Release(); return hr;
}

STDAPI DllCanUnloadNow()
{
    return gObjects == 0 && gLocks == 0 ? S_OK : S_FALSE;
}
