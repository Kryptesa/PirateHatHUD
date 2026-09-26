#include "render/image.hpp"
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <utility>

namespace phi::render {
using Microsoft::WRL::ComPtr;
bool decode_image(const wchar_t* path, Image& image) {
  image = {};
  if (!path || !*path)
    return false;
  const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  if (FAILED(com) && com != RPC_E_CHANGED_MODE)
    return false;
  UINT width = 0, height = 0;
  std::vector<std::uint8_t> pixels;
  const bool decoded = [&]() {
    ComPtr<IWICImagingFactory> factory;
    ComPtr<IWICBitmapDecoder> decoder;
    ComPtr<IWICBitmapFrameDecode> frame;
    ComPtr<IWICFormatConverter> converter;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&factory))) ||
        FAILED(factory->CreateDecoderFromFilename(path, nullptr, GENERIC_READ,
                                                  WICDecodeMetadataCacheOnLoad, &decoder)) ||
        FAILED(decoder->GetFrame(0, &frame)) || FAILED(frame->GetSize(&width, &height)) || !width ||
        !height || width > 4096 || height > 4096 ||
        FAILED(factory->CreateFormatConverter(&converter)) ||
        FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
                                     WICBitmapDitherTypeNone, nullptr, 0.0,
                                     WICBitmapPaletteTypeCustom)))
      return false;
    pixels.resize(static_cast<size_t>(width) * height * 4);
    return SUCCEEDED(
        converter->CopyPixels(nullptr, width * 4, static_cast<UINT>(pixels.size()), pixels.data()));
  }();
  if (SUCCEEDED(com))
    CoUninitialize();
  if (!decoded)
    return false;
  image.width = width;
  image.height = height;
  image.pixels = std::move(pixels);
  return true;
}

} // namespace phi::render
