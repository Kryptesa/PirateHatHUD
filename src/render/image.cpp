#include "render/image.hpp"
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <utility>

namespace phi::render {
using Microsoft::WRL::ComPtr;
namespace {
struct ComInitialization {
  HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  ~ComInitialization() {
    if (SUCCEEDED(result)) {
      CoUninitialize();
    }
  }
};
} // namespace
bool decode_image(const wchar_t* path, Image& image) {
  image = {};
  if (path && !*path) {
    return false;
  }
  const ComInitialization com;
  if (FAILED(com.result) && com.result != RPC_E_CHANGED_MODE) {
    return false;
  }
  UINT width = 0, height = 0;
  std::vector<std::uint8_t> pixels;
  const bool decoded = [&]() {
    ComPtr<IWICImagingFactory> factory;
    ComPtr<IWICBitmapDecoder> decoder;
    ComPtr<IWICStream> stream;
    ComPtr<IWICBitmapFrameDecode> frame;
    ComPtr<IWICFormatConverter> converter;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&factory)))) {
      return false;
    }
    if (path) {
      if (FAILED(factory->CreateDecoderFromFilename(path, nullptr, GENERIC_READ,
                                                    WICDecodeMetadataCacheOnLoad, &decoder))) {
        return false;
      }
    } else {
      HMODULE module = nullptr;
      if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                  GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                              reinterpret_cast<LPCWSTR>(&decode_image), &module)) {
        return false;
      }
      const HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(101), MAKEINTRESOURCEW(10));
      if (!resource) {
        return false;
      }
      const DWORD size = SizeofResource(module, resource);
      const HGLOBAL loaded = LoadResource(module, resource);
      auto* bytes = static_cast<BYTE*>(LockResource(loaded));
      if (!size || !bytes || FAILED(factory->CreateStream(&stream)) ||
          FAILED(stream->InitializeFromMemory(bytes, size)) ||
          FAILED(factory->CreateDecoderFromStream(stream.Get(), nullptr,
                                                  WICDecodeMetadataCacheOnLoad, &decoder))) {
        return false;
      }
    }
    if (FAILED(decoder->GetFrame(0, &frame)) || FAILED(frame->GetSize(&width, &height)) || !width ||
        !height || width > 4096 || height > 4096 ||
        FAILED(factory->CreateFormatConverter(&converter)) ||
        FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
                                     WICBitmapDitherTypeNone, nullptr, 0.0,
                                     WICBitmapPaletteTypeCustom))) {
      return false;
    }
    pixels.resize(static_cast<size_t>(width) * height * 4);
    return SUCCEEDED(
        converter->CopyPixels(nullptr, width * 4, static_cast<UINT>(pixels.size()), pixels.data()));
  }();
  if (!decoded) {
    return false;
  }
  image.width = width;
  image.height = height;
  image.pixels = std::move(pixels);
  return true;
}

} // namespace phi::render
