#include "render/dx12_renderer.hpp"
#include <imgui.h>
#include <backends/imgui_impl_dx12.h>
#include <backends/imgui_impl_win32.h>
#include <cstring>

namespace phi::render {
bool Dx12Renderer::load_icon() {
  if (image_.pixels.empty())
    return false;
  const UINT width = image_.width, height = image_.height;
  const auto& pixels = image_.pixels;

  D3D12_HEAP_PROPERTIES default_heap{};
  default_heap.Type = D3D12_HEAP_TYPE_DEFAULT;
  D3D12_RESOURCE_DESC texture{};
  texture.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  texture.Width = width;
  texture.Height = height;
  texture.DepthOrArraySize = 1;
  texture.MipLevels = 1;
  texture.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  texture.SampleDesc.Count = 1;
  if (FAILED(device->CreateCommittedResource(&default_heap, D3D12_HEAP_FLAG_NONE, &texture,
                                             D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                             IID_PPV_ARGS(&icon_texture))))
    return false;

  UINT rows = 0;
  UINT64 row_bytes = 0, upload_size = 0;
  device->GetCopyableFootprints(&texture, 0, 1, 0, &icon_footprint, &rows, &row_bytes,
                                &upload_size);
  D3D12_HEAP_PROPERTIES upload_heap{};
  upload_heap.Type = D3D12_HEAP_TYPE_UPLOAD;
  D3D12_RESOURCE_DESC upload{};
  upload.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  upload.Width = upload_size;
  upload.Height = 1;
  upload.DepthOrArraySize = 1;
  upload.MipLevels = 1;
  upload.SampleDesc.Count = 1;
  upload.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  if (FAILED(device->CreateCommittedResource(&upload_heap, D3D12_HEAP_FLAG_NONE, &upload,
                                             D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                             IID_PPV_ARGS(&icon_upload))))
    return false;
  void* mapped = nullptr;
  if (FAILED(icon_upload->Map(0, nullptr, &mapped)))
    return false;
  auto* destination = static_cast<std::uint8_t*>(mapped);
  for (UINT row = 0; row < rows; ++row)
    std::memcpy(destination + icon_footprint.Offset + row * icon_footprint.Footprint.RowPitch,
                pixels.data() + static_cast<size_t>(row) * width * 4, width * 4);
  icon_upload->Unmap(0, nullptr);

  D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
  srv.Format = texture.Format;
  srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srv.Texture2D.MipLevels = 1;
  device->CreateShaderResourceView(icon_texture.Get(), &srv,
                                   srv_heap->GetCPUDescriptorHandleForHeapStart());
  icon_gpu = srv_heap->GetGPUDescriptorHandleForHeapStart();
  icon_pending = true;
  return true;
}

bool Dx12Renderer::record_icon_upload() {
  const bool upload_icon = icon_pending && icon_texture && icon_upload;
  if (upload_icon) {
    D3D12_TEXTURE_COPY_LOCATION destination{}, source{};
    destination.pResource = icon_texture.Get();
    destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    source.pResource = icon_upload.Get();
    source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    source.PlacedFootprint = icon_footprint;
    list->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
    D3D12_RESOURCE_BARRIER ready{};
    ready.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    ready.Transition.pResource = icon_texture.Get();
    ready.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    ready.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    ready.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    list->ResourceBarrier(1, &ready);
  }
  return upload_icon;
}

} // namespace phi::render
