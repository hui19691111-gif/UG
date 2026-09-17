// Decode image sidecars directly; never invoke a PRT thumbnail provider or
// reuse the Windows shell thumbnail cache after the user replaces a picture.
using Microsoft::WRL::ComPtr;

void CheckPreviewImage(HRESULT result)
{
    if (FAILED(result)) throw std::runtime_error(ToAnsi(L"无法读取或保存图片，请选择有效的 PNG、JPG 或 BMP 文件。"));
}

ComPtr<IWICImagingFactory> PreviewImageFactory()
{
    ComPtr<IWICImagingFactory> factory;
    CheckPreviewImage(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(factory.GetAddressOf())));
    return factory;
}

ComPtr<IWICBitmapFrameDecode> PreviewImageFrame(IWICImagingFactory* factory, const fs::path& path)
{
    ComPtr<IWICBitmapDecoder> decoder;
    CheckPreviewImage(factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
        WICDecodeMetadataCacheOnLoad, &decoder));
    ComPtr<IWICBitmapFrameDecode> frame;
    CheckPreviewImage(decoder->GetFrame(0, &frame));
    UINT width = 0, height = 0;
    CheckPreviewImage(frame->GetSize(&width, &height));
    if (!width || !height || width > 16384 || height > 16384)
        throw std::runtime_error(ToAnsi(L"图片尺寸无效，宽和高不能超过 16384 像素。"));
    return frame;
}

void WritePreviewPng(const fs::path& source, const fs::path& target)
{
    const auto factory = PreviewImageFactory();
    const auto sourceFrame = PreviewImageFrame(factory.Get(), source);
    ComPtr<IWICStream> stream;
    CheckPreviewImage(factory->CreateStream(&stream));
    CheckPreviewImage(stream->InitializeFromFilename(target.c_str(), GENERIC_WRITE));
    ComPtr<IWICBitmapEncoder> encoder;
    CheckPreviewImage(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder));
    CheckPreviewImage(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache));
    ComPtr<IWICBitmapFrameEncode> frame;
    CheckPreviewImage(encoder->CreateNewFrame(&frame, nullptr));
    CheckPreviewImage(frame->Initialize(nullptr));
    CheckPreviewImage(frame->WriteSource(sourceFrame.Get(), nullptr));
    CheckPreviewImage(frame->Commit());
    CheckPreviewImage(encoder->Commit());
}

HBITMAP ReadPreviewBitmap(const fs::path& path)
{
    const auto factory = PreviewImageFactory();
    const auto frame = PreviewImageFrame(factory.Get(), path);
    UINT width = 0, height = 0;
    CheckPreviewImage(frame->GetSize(&width, &height));
    const double scale = std::min(400.0 / width, 240.0 / height);
    width = std::max(1U, static_cast<UINT>(width * scale));
    height = std::max(1U, static_cast<UINT>(height * scale));
    ComPtr<IWICBitmapScaler> scaler;
    CheckPreviewImage(factory->CreateBitmapScaler(&scaler));
    CheckPreviewImage(scaler->Initialize(frame.Get(), width, height, WICBitmapInterpolationModeFant));
    ComPtr<IWICFormatConverter> converter;
    CheckPreviewImage(factory->CreateFormatConverter(&converter));
    CheckPreviewImage(converter->Initialize(scaler.Get(), GUID_WICPixelFormat32bppBGRA,
        WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom));
    std::vector<BYTE> pixels(width * height * 4);
    CheckPreviewImage(converter->CopyPixels(nullptr, width * 4, static_cast<UINT>(pixels.size()), pixels.data()));
    const COLORREF background = GetSysColor(COLOR_WINDOW);
    const BYTE bg[] = {GetBValue(background), GetGValue(background), GetRValue(background)};
    for (std::size_t i = 0; i < pixels.size(); i += 4)
    {
        const unsigned alpha = pixels[i + 3];
        for (int c = 0; c < 3; ++c)
            pixels[i + c] = static_cast<BYTE>((pixels[i + c] * alpha + bg[c] * (255 - alpha) + 127) / 255);
        pixels[i + 3] = 255;
    }
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -static_cast<LONG>(height);
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    const auto bitmap = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bitmap || !bits) throw std::runtime_error(ToAnsi(L"无法创建预览图。"));
    std::copy(pixels.begin(), pixels.end(), static_cast<BYTE*>(bits));
    return bitmap;
}
