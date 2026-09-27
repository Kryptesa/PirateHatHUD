#include "render/dx12_renderer.hpp"
#include "render/icon_draw_data.hpp"
#include <imgui.h>
#include <backends/imgui_impl_dx12.h>
#include <backends/imgui_impl_win32.h>
#include <cstdio>
#include <utility>

namespace phi::render {
bool same_device(ID3D12CommandQueue* queue, ID3D12Device* device) {
  if (!queue || !device) {
    return false;
  }

  ComPtr<ID3D12Device> candidate;

  return SUCCEEDED(queue->GetDevice(IID_PPV_ARGS(&candidate))) && candidate.Get() == device;
}

ReleaseResult Dx12Renderer::shutdown() noexcept {
  state_ = RendererState::stopped;

  if (fence) {
    const auto result = wait_all();

    if (!can_release(result)) {
      return ReleaseResult::retained;
    }
  }

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
  hdr_pipeline.Reset();
  hdr_root.Reset();
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

  return ReleaseResult::released;
}

bool Dx12Renderer::initialize(IDXGISwapChain* target_swap, ID3D12CommandQueue* target_queue) {
  if (state_ != RendererState::waiting) {
    return false;
  }

  if (
    FAILED(target_swap->GetDevice(IID_PPV_ARGS(&device))) ||
    !same_device(target_queue, device.Get())
  ) {
    return false;
  }

  this->queue = target_queue;
  DXGI_SWAP_CHAIN_DESC desc{};

  if (FAILED(target_swap->GetDesc(&desc)) || !desc.OutputWindow) {
    return false;
  }

  window = desc.OutputWindow;
  fence_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);

  if (!fence_event || FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)))) {
    return false;
  }

  D3D12_DESCRIPTOR_HEAP_DESC heap{};
  heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  heap.NumDescriptors = static_cast<UINT>(descriptors.size());
  heap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

  if (
    FAILED(device->CreateDescriptorHeap(&heap, IID_PPV_ARGS(&srv_heap))) ||
    !create_buffers(target_swap)
  ) {
    return false;
  }

  descriptors[0] = true; // Reserve the first SRV for icon.png.
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  context = true;

  if (!ImGui_ImplWin32_Init(window)) {
    return false;
  }

  win32 = true;

  if (!initialize_backend(desc)) {
    return false;
  }

  if (!load_icon()) {
    return false;
  }

  this->swap = target_swap;
  state_ = RendererState::ready;
  log(LogLevel::info, "DX12 overlay initialized");

  return true;
}

bool Dx12Renderer::replace_swapchain(
  IDXGISwapChain* candidate,
  ID3D12CommandQueue* candidate_queue
) {
  // Only migrate to a captured swapchain for the same game window. Never release
  // resources still used by an outstanding submission or restart after stop.
  DXGI_SWAP_CHAIN_DESC desc{};
  ComPtr<ID3D12Device> candidate_device;

  if (
    state_ == RendererState::stopped ||
    !window ||
    !candidate ||
    candidate == swap ||
    FAILED(candidate->GetDesc(&desc)) ||
    desc.OutputWindow != window ||
    FAILED(candidate->GetDevice(IID_PPV_ARGS(&candidate_device))) ||
    !same_device(candidate_queue, candidate_device.Get())
  ) {
    return false;
  }

  log(LogLevel::info, "DX12 replacement swapchain detected; releasing old renderer");

  if (shutdown() != ReleaseResult::released) {
    log(LogLevel::error, "DX12 swapchain replacement blocked; GPU resources retained");

    return false;
  }

  state_ = RendererState::waiting;

  if (!initialize(candidate, candidate_queue)) {
    log(LogLevel::error, "DX12 replacement swapchain initialization failed");
    shutdown();

    return false;
  }

  log(LogLevel::info, "DX12 overlay recovered on replacement swapchain");

  return true;
}

void Dx12Renderer::render(
  IDXGISwapChain* target_swap,
  const HudState& hud,
  DXGI_COLOR_SPACE_TYPE color_space
) {
  if (!ready() || this->swap != target_swap || !queue || untracked_submission) {
    return;
  }

  ComPtr<IDXGISwapChain3> swap3;

  if (FAILED(target_swap->QueryInterface(IID_PPV_ARGS(&swap3)))) {
    return;
  }

  const UINT index = swap3->GetCurrentBackBufferIndex();

  if (index >= frames.size()) {
    return;
  }

  auto& frame = frames[index];

  // Only wait for resources reused by this submission: the selected backbuffer
  // allocator/list and the backend's next vertex/index buffer slot.
  const auto waited =
    wait_fence(backend_frames.required_fence(frame.fence_value), GetTickCount64());

  if (waited == WaitResult::timeout) {
    return;
  }

  if (waited != WaitResult::completed) {
    fault();
    return;
  }

  auto& list = frame.list;

  if (FAILED(frame.allocator->Reset()) || FAILED(list->Reset(frame.allocator.Get(), nullptr))) {
    fault();
    return;
  }

  const bool upload_icon = record_icon_upload(list.Get());
  const bool hdr = color_space == DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709 ||
    color_space == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
  ImGui_ImplDX12_NewFrame();
  ImGui_ImplWin32_NewFrame();
  ImGui::NewFrame();

  if (!hdr) {
    draw_hud(hud, icon_gpu);
  }

  ImGui::Render();
  auto* draw_data = ImGui::GetDrawData();

  // The backend returns before consuming a ring slot for a minimized window.
  if (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f) {
    if (FAILED(list->Close())) {
      fault();
    }

    return;
  }

  // This HUD only draws the externally owned icon. Reject new font/texture users
  // before bypassing the backend's synchronous (unbounded) texture upload path.
  if (!icon_only(*draw_data, static_cast<ImTextureID>(icon_gpu.ptr))) {
    fault();
    return;
  }

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

  ExternalTextureDraw external_textures(*draw_data);
  ImGui_ImplDX12_RenderDrawData(draw_data, list.Get());

  if (hdr && hud.visible) {
    const auto buffer_desc = frame.buffer->GetDesc();
    draw_hdr(
      list.Get(),
      hud,
      static_cast<UINT>(buffer_desc.Width),
      buffer_desc.Height,
      color_space
    );
  }

  std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
  list->ResourceBarrier(1, &barrier);

  if (FAILED(list->Close())) {
    fault(); // The backend consumed a slot; never continue with a divergent ring.
    return;
  }

  ID3D12CommandList* lists[] = {list.Get()};
  untracked_submission = true;
  queue->ExecuteCommandLists(1, lists);

  if (upload_icon) {
    icon_pending = false;
  }

  const UINT64 value = fence_next++;

  if (SUCCEEDED(queue->Signal(fence.Get(), value))) {
    frame.fence_value = value;
    backend_frames.submitted(value);
    untracked_submission = false;
  } else {
    untracked_submission = true;
    fault();
    log(LogLevel::error, "DX12 queue signal failed; rendering disabled");
  }
}

bool Dx12Renderer::before_resize(IDXGISwapChain* candidate) {
  if (!handles(candidate)) {
    return true;
  }

  if (state_ == RendererState::stopped) {
    return false;
  }

  const auto result = wait_all();

  if (result != WaitResult::completed && result != WaitResult::device_lost) {
    fault();
    log(LogLevel::error, "DX12 resize wait failed; overlay resources retained");

    return false;
  }

  if (imgui) {
    ImGui_ImplDX12_Shutdown();
    imgui = false;
  }

  descriptors.fill(false);
  descriptors[0] = true;
  release_buffers();
  log(LogLevel::debug, "DX12 backbuffers released for resize");
  state_ = result == WaitResult::completed ? RendererState::resizing : RendererState::faulted;

  return result == WaitResult::completed;
}

void Dx12Renderer::after_resize(
  IDXGISwapChain* candidate,
  HRESULT result,
  UINT count,
  IUnknown* const* queues
) {
  if (!handles(candidate) || state_ != RendererState::resizing) {
    return;
  }

  DXGI_SWAP_CHAIN_DESC desc{};

  if (FAILED(result) || FAILED(candidate->GetDesc(&desc))) {
    fault();
    char message[128]{};
    std::snprintf(
      message,
      sizeof(message),
      "DX12 swapchain resize failed (HRESULT 0x%08lX); overlay disabled",
      static_cast<unsigned long>(result)
    );
    log(LogLevel::error, message);
    return;
  }

  if (queues) {
    ComPtr<ID3D12CommandQueue> replacement;

    if (!count || count != desc.BufferCount) {
      fault();
      log(LogLevel::error, "DX12 resize queue count unsupported; overlay disabled");
      return;
    }

    for (UINT i = 0; i < count; ++i) {
      ComPtr<ID3D12CommandQueue> current;

      if (
        !queues[i] ||
        FAILED(queues[i]->QueryInterface(IID_PPV_ARGS(&current))) ||
        current->GetDesc().Type != D3D12_COMMAND_LIST_TYPE_DIRECT ||
        !same_device(current.Get(), device.Get()) ||
        (replacement && replacement.Get() != current.Get())
      ) {
        fault();
        log(LogLevel::error, "DX12 resize queues unsupported; overlay disabled");
        return;
      }

      replacement = current;
    }

    queue = replacement;
  }

  if (!create_buffers(candidate) || !initialize_backend(desc)) {
    fault();
    log(LogLevel::error, "DX12 resize recreation failed; overlay disabled");
    return;
  }

  state_ = RendererState::ready;
  log(LogLevel::info, "DX12 overlay recreated after resize");
}

void Dx12Renderer::set_image(Image image) {
  image_ = std::move(image);
}

void Dx12Renderer::set_logger(LogCallback logger) {
  logger_ = logger;
}

void Dx12Renderer::log(LogLevel level, const char* message) const {
  if (logger_) {
    logger_(level, message);
  }
}

} // namespace phi::render
