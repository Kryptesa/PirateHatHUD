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

    if (result != WaitResult::completed) {
      log_wait_failure(result, "shutdown");
    }
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

  log(LogLevel::debug, "DX12 renderer initialization begin");
  if (!check_result(target_swap->GetDevice(IID_PPV_ARGS(&device)), "swapchain GetDevice")) {
    return false;
  }
  if (!same_device(target_queue, device.Get())) {
    log(LogLevel::error, "DX12 renderer queue/device mismatch");
    return false;
  }

  this->queue = target_queue;
  log_adapter();
  DXGI_SWAP_CHAIN_DESC desc{};

  if (!check_result(target_swap->GetDesc(&desc), "swapchain GetDesc") || !desc.OutputWindow) {
    return false;
  }

  log_swapchain(desc);

  window = desc.OutputWindow;
  fence_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);

  if (!fence_event) {
    check_result(HRESULT_FROM_WIN32(GetLastError()), "CreateEvent");
    return false;
  }
  if (!check_result(
        device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)),
        "CreateFence"
      )) {
    return false;
  }

  D3D12_DESCRIPTOR_HEAP_DESC heap{};
  heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  heap.NumDescriptors = static_cast<UINT>(descriptors.size());
  heap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

  if (
    !check_result(
      device->CreateDescriptorHeap(&heap, IID_PPV_ARGS(&srv_heap)),
      "CreateDescriptorHeap(SRV)"
    ) ||
    !create_buffers(target_swap)
  ) {
    return false;
  }
  log(LogLevel::debug, "DX12 frame resources ready");

  descriptors[0] = true; // Reserve the first SRV for icon.png.
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  context = true;

  if (!ImGui_ImplWin32_Init(window)) {
    log(LogLevel::error, "DX12 ImGui Win32 initialization failed");
    return false;
  }

  win32 = true;

  if (!initialize_backend(desc)) {
    return false;
  }
  log(LogLevel::debug, "DX12 graphics backend ready");

  if (!load_icon()) {
    return false;
  }
  log(LogLevel::debug, "DX12 icon resources ready");

  this->swap = target_swap;
  state_ = RendererState::ready;
  first_frame_pending = true;
  first_frame_started = false;
  selection_error_logged = false;
  last_present_error = S_OK;
  has_color_space = false;
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

  const auto queried = target_swap->QueryInterface(IID_PPV_ARGS(&swap3));
  if (FAILED(queried)) {
    if (!selection_error_logged) {
      check_result(queried, "QueryInterface(SwapChain3)");
      selection_error_logged = true;
    }
    return;
  }

  const UINT index = swap3->GetCurrentBackBufferIndex();

  if (index >= frames.size()) {
    if (!selection_error_logged) {
      log(LogLevel::error, "DX12 current backbuffer index out of range; frame skipped");
      selection_error_logged = true;
    }
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
    log_wait_failure(waited, "render");
    fault();
    return;
  }

  auto& list = frame.list;

  if (
    !check_result(frame.allocator->Reset(), "CommandAllocator Reset") ||
    !check_result(list->Reset(frame.allocator.Get(), nullptr), "CommandList Reset")
  ) {
    fault();
    return;
  }

  if (!has_color_space || last_color_space != color_space) {
    char message[96]{};
    std::snprintf(
      message,
      sizeof(message),
      "DX12 render color space=%u",
      static_cast<unsigned>(color_space)
    );
    log(LogLevel::debug, message);
    last_color_space = color_space;
    has_color_space = true;
  }
  if (!first_frame_started) {
    log(LogLevel::debug, "DX12 first overlay frame begin");
    first_frame_started = true;
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
    if (!check_result(list->Close(), "CommandList Close(minimized)")) {
      fault();
    }

    return;
  }

  // This HUD only draws the externally owned icon. Reject new font/texture users
  // before bypassing the backend's synchronous (unbounded) texture upload path.
  if (!icon_only(*draw_data, static_cast<ImTextureID>(icon_gpu.ptr))) {
    log(LogLevel::error, "DX12 unsupported HUD draw commands; rendering disabled");
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

  if (!check_result(list->Close(), "CommandList Close")) {
    fault(); // The backend consumed a slot; never continue with a divergent ring.
    return;
  }

  ID3D12CommandList* lists[] = {list.Get()};
  untracked_submission = true;
  if (first_frame_pending) {
    log(LogLevel::debug, "DX12 first overlay submission begin");
  }
  queue->ExecuteCommandLists(1, lists);

  if (upload_icon) {
    icon_pending = false;
  }

  const UINT64 value = fence_next++;

  if (check_result(queue->Signal(fence.Get(), value), "CommandQueue Signal")) {
    frame.fence_value = value;
    backend_frames.submitted(value);
    untracked_submission = false;
    if (first_frame_pending) {
      log(
        LogLevel::debug,
        "DX12 first overlay submission completed on CPU; GPU completion pending"
      );
      first_frame_pending = false;
    }
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

  if (result == WaitResult::device_lost) {
    log_wait_failure(result, "resize");
  }

  if (result != WaitResult::completed && result != WaitResult::device_lost) {
    log_wait_failure(result, "resize");
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

  if (
    !check_result(result, "ResizeBuffers") ||
    !check_result(candidate->GetDesc(&desc), "GetDesc after resize")
  ) {
    fault();
    log(LogLevel::error, "DX12 swapchain resize failed; overlay disabled");
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

  // The hooked implementation can be inside an upscaler/frame-generation wrapper.
  // Returning from its resize is not proof that the outer transition has finished.
  log(LogLevel::debug, "DX12 resize completed; waiting for successful Present");
}

void Dx12Renderer::after_present(IDXGISwapChain* candidate, HRESULT result, UINT flags) {
  if (handles(candidate) && !(flags & DXGI_PRESENT_TEST)) {
    if (FAILED(result) && result != last_present_error) {
      check_result(result, "Present");
    }
    last_present_error = FAILED(result) ? result : S_OK;
  }
  if (
    !handles(candidate) ||
    state_ != RendererState::resizing ||
    result != S_OK ||
    (flags & DXGI_PRESENT_TEST)
  ) {
    return;
  }

  DXGI_SWAP_CHAIN_DESC desc{};

  if (
    !check_result(candidate->GetDesc(&desc), "GetDesc before resize recovery") ||
    !create_buffers(candidate) ||
    !initialize_backend(desc)
  ) {
    fault();
    log(LogLevel::error, "DX12 resize recreation failed; overlay disabled");
    return;
  }

  state_ = RendererState::ready;
  first_frame_pending = true;
  first_frame_started = false;
  log_swapchain(desc);
  log(LogLevel::info, "DX12 overlay recreated after resize");
}

void Dx12Renderer::log_adapter() const {
  if (!logger_ || !device) {
    return;
  }

  ComPtr<IDXGIFactory4> factory;
  ComPtr<IDXGIAdapter1> adapter;
  DXGI_ADAPTER_DESC1 desc{};

  // Match the game's actual device, not the default adapter or the hook probe.
  if (
    FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))) ||
    FAILED(factory->EnumAdapterByLuid(device->GetAdapterLuid(), IID_PPV_ARGS(&adapter))) ||
    FAILED(adapter->GetDesc1(&desc))
  ) {
    log(LogLevel::debug, "DX12 GPU description unavailable");
    return;
  }

  desc.Description[127] = L'\0';
  char name[512]{};
  if (!WideCharToMultiByte(
        CP_UTF8,
        0,
        desc.Description,
        -1,
        name,
        sizeof(name),
        nullptr,
        nullptr
      )) {
    log(LogLevel::debug, "DX12 GPU description conversion failed");
    return;
  }

  for (auto& character : name) {
    if (character && (static_cast<unsigned char>(character) < 32 || character == 127)) {
      character = '?';
    }
  }

  char message[768]{};
  std::snprintf(
    message,
    sizeof(message),
    "DX12 GPU: %s; vendor=0x%04X; device=0x%04X; dedicated VRAM=%llu MiB; software=%s",
    name,
    desc.VendorId,
    desc.DeviceId,
    static_cast<unsigned long long>(desc.DedicatedVideoMemory / (1024 * 1024)),
    (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) ? "yes" : "no"
  );
  log(LogLevel::info, message);

  LARGE_INTEGER driver_version{};
  const auto driver_result = adapter->CheckInterfaceSupport(__uuidof(IDXGIDevice), &driver_version);
  if (SUCCEEDED(driver_result)) {
    const auto value = static_cast<unsigned long long>(driver_version.QuadPart);
    std::snprintf(
      message,
      sizeof(message),
      "DX12 GPU driver UMD version: %llu.%llu.%llu.%llu",
      (value >> 48) & 0xFFFF,
      (value >> 32) & 0xFFFF,
      (value >> 16) & 0xFFFF,
      value & 0xFFFF
    );
  } else {
    std::snprintf(
      message,
      sizeof(message),
      "DX12 GPU driver UMD version unavailable; HRESULT=0x%08lX",
      static_cast<unsigned long>(driver_result)
    );
  }
  log(LogLevel::info, message);
}

bool Dx12Renderer::check_result(HRESULT result, const char* operation) const {
  if (SUCCEEDED(result)) {
    return true;
  }
  if (logger_) {
    char message[256]{};
    const auto removed = device ? device->GetDeviceRemovedReason() : S_OK;
    std::snprintf(
      message,
      sizeof(message),
      "DX12 %s failed; HRESULT=0x%08lX; device removed reason=0x%08lX; device=%s",
      operation,
      static_cast<unsigned long>(result),
      static_cast<unsigned long>(removed),
      device ? "available" : "unavailable"
    );
    log(LogLevel::error, message);
  }
  return false;
}

void Dx12Renderer::log_wait_failure(WaitResult result, const char* operation) const {
  if (!logger_) {
    return;
  }
  char message[192]{};
  const char* status = "unknown";
  switch (result) {
  case WaitResult::completed:
    status = "completed";
    break;
  case WaitResult::device_lost:
    status = "device lost";
    break;
  case WaitResult::timeout:
    status = "timeout";
    break;
  case WaitResult::failed:
    status = "failed";
    break;
  }
  std::snprintf(
    message,
    sizeof(message),
    "DX12 %s fence wait failed; result=%s; device removed reason=0x%08lX; untracked submission=%s",
    operation,
    status,
    static_cast<unsigned long>(device ? device->GetDeviceRemovedReason() : S_OK),
    untracked_submission ? "yes" : "no"
  );
  log(LogLevel::error, message);
}

void Dx12Renderer::log_swapchain(const DXGI_SWAP_CHAIN_DESC& desc) const {
  if (!logger_) {
    return;
  }

  char message[256]{};
  std::snprintf(
    message,
    sizeof(message),
    "DX12 swapchain: %ux%u; format=%u; buffers=%u; swap effect=%u; flags=0x%X; windowed=%s",
    desc.BufferDesc.Width,
    desc.BufferDesc.Height,
    static_cast<unsigned>(desc.BufferDesc.Format),
    desc.BufferCount,
    static_cast<unsigned>(desc.SwapEffect),
    desc.Flags,
    desc.Windowed ? "yes" : "no"
  );
  log(LogLevel::debug, message);
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
