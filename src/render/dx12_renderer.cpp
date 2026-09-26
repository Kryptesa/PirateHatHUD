#include "render/dx12_renderer.hpp"
#include <imgui.h>
#include <backends/imgui_impl_dx12.h>
#include <backends/imgui_impl_win32.h>
#include <utility>

namespace phi::render {
bool same_device(ID3D12CommandQueue* queue, ID3D12Device* device) {
  if (!queue || !device)
    return false;
  ComPtr<ID3D12Device> candidate;
  return SUCCEEDED(queue->GetDevice(IID_PPV_ARGS(&candidate))) && candidate.Get() == device;
}
void Dx12Renderer::shutdown() {
  if (fence)
    (void)wait_all();
  if (imgui) {
    ImGui_ImplDX12_Shutdown();
    imgui = false;
  }
  if (win32) {
    ImGui_ImplWin32_Shutdown();
    win32 = false;
  }
  if (context) {
    ImGui::DestroyContext();
    context = false;
  }
  release_buffers();
  icon_texture.Reset();
  icon_upload.Reset();
  icon_gpu = {};
  icon_pending = false;
  srv_heap.Reset();
  fence.Reset();
  queue.Reset();
  device.Reset();
  if (fence_event) {
    CloseHandle(fence_event);
    fence_event = nullptr;
  }
  swap = nullptr;
  window = nullptr;
  untracked_submission = false;
  descriptors.fill(false);
}
bool Dx12Renderer::initialize(IDXGISwapChain* target_swap, ID3D12CommandQueue* target_queue) {
  if (FAILED(target_swap->GetDevice(IID_PPV_ARGS(&device))) ||
      !same_device(target_queue, device.Get()))
    return false;
  this->queue = target_queue;
  DXGI_SWAP_CHAIN_DESC desc{};
  if (FAILED(target_swap->GetDesc(&desc)) || !desc.OutputWindow)
    return false;
  window = desc.OutputWindow;
  fence_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  if (!fence_event || FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))))
    return false;
  D3D12_DESCRIPTOR_HEAP_DESC heap{};
  heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  heap.NumDescriptors = static_cast<UINT>(descriptors.size());
  heap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  if (FAILED(device->CreateDescriptorHeap(&heap, IID_PPV_ARGS(&srv_heap))) ||
      !create_buffers(target_swap))
    return false;
  descriptors[0] = true; // Reserve the first SRV for icon.png.
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  context = true;
  if (!ImGui_ImplWin32_Init(window))
    return false;
  win32 = true;
  if (!initialize_backend(desc))
    return false;
  if (!load_icon())
    return false;
  this->swap = target_swap;
  log("DX12 overlay initialized");
  return true;
}
void Dx12Renderer::render(IDXGISwapChain* target_swap, const HudState& hud) {
  if (!imgui || this->swap != target_swap || !queue || untracked_submission)
    return;
  ComPtr<IDXGISwapChain3> swap3;
  if (FAILED(target_swap->QueryInterface(IID_PPV_ARGS(&swap3))))
    return;
  const UINT index = swap3->GetCurrentBackBufferIndex();
  if (index >= frames.size())
    return;
  auto& frame = frames[index];
  if (!wait_frame(frame) || FAILED(frame.allocator->Reset()) ||
      FAILED(list->Reset(frame.allocator.Get(), nullptr)))
    return;
  const bool upload_icon = record_icon_upload();
  ImGui_ImplDX12_NewFrame();
  ImGui_ImplWin32_NewFrame();
  ImGui::NewFrame();
  draw_hud(hud, icon_gpu);
  ImGui::Render();
  D3D12_RESOURCE_BARRIER barrier{};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Transition.pResource = frame.buffer.Get();
  barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
  list->ResourceBarrier(1, &barrier);
  list->OMSetRenderTargets(1, &frame.rtv, FALSE, nullptr);
  auto* srv = srv_heap.Get();
  list->SetDescriptorHeaps(1, &srv);
  ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), list.Get());
  std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
  list->ResourceBarrier(1, &barrier);
  if (FAILED(list->Close()))
    return;
  ID3D12CommandList* lists[] = {list.Get()};
  queue->ExecuteCommandLists(1, lists);
  if (upload_icon)
    icon_pending = false;
  const UINT64 value = fence_next++;
  if (SUCCEEDED(queue->Signal(fence.Get(), value)))
    frame.fence_value = value;
  else {
    untracked_submission = true;
    log("DX12 queue signal failed; rendering disabled");
  }
}
void Dx12Renderer::before_resize(IDXGISwapChain* candidate) {
  if (!handles(candidate)) {
    return;
  }
  (void)wait_all();
  if (imgui) {
    ImGui_ImplDX12_Shutdown();
    imgui = false;
  }
  descriptors.fill(false);
  descriptors[0] = true;
  release_buffers();
}
void Dx12Renderer::after_resize(IDXGISwapChain* candidate) {
  if (!handles(candidate)) {
    return;
  }
  DXGI_SWAP_CHAIN_DESC desc{};
  if (FAILED(candidate->GetDesc(&desc)) || !create_buffers(candidate) ||
      !initialize_backend(desc)) {
    log("DX12 resize recreation failed; overlay disabled");
    shutdown();
  }
}
void Dx12Renderer::set_image(Image image) {
  image_ = std::move(image);
}
void Dx12Renderer::set_logger(void (*logger)(const char*)) {
  logger_ = logger;
}
void Dx12Renderer::log(const char* message) const {
  if (logger_) {
    logger_(message);
  }
}

} // namespace phi::render
