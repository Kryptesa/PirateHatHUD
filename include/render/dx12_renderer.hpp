#pragma once
#include "render/image.hpp"
#include "render/hud_state.hpp"
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <array>
#include <vector>

struct ImGui_ImplDX12_InitInfo;

namespace phi::render {
using Microsoft::WRL::ComPtr;
bool same_device(ID3D12CommandQueue* queue, ID3D12Device* device);
void draw_hud(const HudState& hud, D3D12_GPU_DESCRIPTOR_HANDLE texture);

// Called under the hook module's graphics mutex. Image/logger are set before hooks start.
class Dx12Renderer {
public:
  Dx12Renderer() = default;
  Dx12Renderer(const Dx12Renderer&) = delete;
  Dx12Renderer& operator=(const Dx12Renderer&) = delete;
  void set_image(Image image);
  void set_logger(void (*logger)(const char*));
  bool ready() const {
    return imgui;
  }
  bool handles(IDXGISwapChain* candidate) const {
    return swap == candidate;
  }
  bool initialize(IDXGISwapChain* swap, ID3D12CommandQueue* queue);
  void render(IDXGISwapChain* swap, const HudState& hud);
  void before_resize(IDXGISwapChain* swap);
  void after_resize(IDXGISwapChain* swap);
  void shutdown();

private:
  struct Frame {
    ComPtr<ID3D12Resource> buffer;
    ComPtr<ID3D12CommandAllocator> allocator;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv{};
    UINT64 fence_value{};
  };
  static void descriptor_alloc(ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* cpu,
                               D3D12_GPU_DESCRIPTOR_HANDLE* gpu);
  static void descriptor_free(ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE cpu,
                              D3D12_GPU_DESCRIPTOR_HANDLE gpu);
  bool wait_frame(const Frame& frame);
  bool wait_all();
  void release_buffers();
  bool create_buffers(IDXGISwapChain* swap);
  bool initialize_backend(const DXGI_SWAP_CHAIN_DESC& desc);
  bool load_icon();
  bool record_icon_upload();
  void log(const char* message) const;

  Image image_;
  void (*logger_)(const char*){};
  IDXGISwapChain* swap{};
  HWND window{};
  ComPtr<ID3D12Device> device;
  ComPtr<ID3D12CommandQueue> queue;
  ComPtr<ID3D12DescriptorHeap> rtv_heap, srv_heap;
  ComPtr<ID3D12GraphicsCommandList> list;
  ComPtr<ID3D12Fence> fence;
  ComPtr<ID3D12Resource> icon_texture, icon_upload;
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT icon_footprint{};
  D3D12_GPU_DESCRIPTOR_HANDLE icon_gpu{};
  bool icon_pending{};
  HANDLE fence_event{};
  UINT64 fence_next{1};
  std::vector<Frame> frames;
  bool imgui{};
  bool context{};
  bool win32{};
  bool untracked_submission{};
  std::array<bool, 64> descriptors{};
};
} // namespace phi::render
