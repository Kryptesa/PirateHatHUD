#include "overlay.hpp"
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <safetyhook.hpp>
#include <imgui.h>
#include <backends/imgui_impl_dx12.h>
#include <backends/imgui_impl_win32.h>
#include <atomic>
#include <array>
#include <mutex>
#include <vector>
#include <cstdint>

namespace phi {
namespace {
using Microsoft::WRL::ComPtr;
using PresentFn = HRESULT(WINAPI*)(IDXGISwapChain*, UINT, UINT);
using ResizeFn = HRESULT(WINAPI*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
using CreateFn = HRESULT(WINAPI*)(IDXGIFactory*, IUnknown*, DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**);
using CreateHwndFn = HRESULT(WINAPI*)(IDXGIFactory2*, IUnknown*, HWND, const DXGI_SWAP_CHAIN_DESC1*, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*, IDXGIOutput*, IDXGISwapChain1**);
using CreateCoreFn = HRESULT(WINAPI*)(IDXGIFactory2*, IUnknown*, IUnknown*, const DXGI_SWAP_CHAIN_DESC1*, IDXGIOutput*, IDXGISwapChain1**);
using CreateCompositionFn = HRESULT(WINAPI*)(IDXGIFactory2*, IUnknown*, const DXGI_SWAP_CHAIN_DESC1*, IDXGIOutput*, IDXGISwapChain1**);
using ExecuteFn = void(WINAPI*)(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);
safetyhook::InlineHook present_hook, resize_hook, create_hook, create_hwnd_hook, create_core_hook, create_composition_hook, execute_hook;
std::atomic<unsigned> callbacks{0};
std::atomic<bool> stopping{false}, enabled{true}, force_icon{false}, active{false};
std::atomic<int> x_pos{40}, y_pos{140};
std::atomic<float> scale{1.0f};
std::mutex mutex;
thread_local bool overlay_submit = false;
IDXGISwapChain* selected_swap{};
ComPtr<ID3D12CommandQueue> selected_queue;
ComPtr<ID3D12CommandQueue> fallback_queue;
bool fallback_ambiguous{};
bool fallback_logged{};
void (*logger)(const char*){};
void overlay_log(const char* message) { if (logger) logger(message); }
struct Frame {
  ComPtr<ID3D12Resource> buffer;
  ComPtr<ID3D12CommandAllocator> allocator;
  D3D12_CPU_DESCRIPTOR_HANDLE rtv{};
  UINT64 fence_value{};
};
struct Graphics {
  IDXGISwapChain* swap{};
  HWND window{};
  ComPtr<ID3D12Device> device;
  ComPtr<ID3D12CommandQueue> queue;
  ComPtr<ID3D12DescriptorHeap> rtv_heap, srv_heap;
  ComPtr<ID3D12GraphicsCommandList> list;
  ComPtr<ID3D12Fence> fence;
  HANDLE fence_event{};
  UINT64 fence_next{1};
  std::vector<Frame> frames;
  bool imgui{};
  bool context{};
  bool win32{};
  bool untracked_submission{};
  std::array<bool, 64> descriptors{};
} gfx;
bool same_device(ID3D12CommandQueue* queue, ID3D12Device* device) {
  if (!queue || !device) return false;
  ComPtr<ID3D12Device> candidate;
  return SUCCEEDED(queue->GetDevice(IID_PPV_ARGS(&candidate))) && candidate.Get() == device;
}
void remember_swap(IDXGISwapChain* swap, IUnknown* object) {
  if (!swap || !object) return;
  ComPtr<ID3D12CommandQueue> queue;
  if (FAILED(object->QueryInterface(IID_PPV_ARGS(&queue)))) return;
  if (queue->GetDesc().Type != D3D12_COMMAND_LIST_TYPE_DIRECT) return;
  std::lock_guard lock(mutex);
  selected_swap = swap;
  selected_queue = queue;
  overlay_log("DX12 swapchain and present queue captured");
}
HRESULT WINAPI on_create(IDXGIFactory* factory, IUnknown* device, DXGI_SWAP_CHAIN_DESC* desc, IDXGISwapChain** out) {
  callbacks.fetch_add(1);
  const auto hr = create_hook.call<HRESULT>(factory, device, desc, out);
  if (!stopping && SUCCEEDED(hr) && out) remember_swap(*out, device);
  callbacks.fetch_sub(1);
  return hr;
}
HRESULT WINAPI on_create_hwnd(IDXGIFactory2* factory, IUnknown* device, HWND hwnd, const DXGI_SWAP_CHAIN_DESC1* desc, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* fullscreen, IDXGIOutput* output, IDXGISwapChain1** out) {
  callbacks.fetch_add(1);
  const auto hr = create_hwnd_hook.call<HRESULT>(factory, device, hwnd, desc, fullscreen, output, out);
  if (!stopping && SUCCEEDED(hr) && out) remember_swap(*out, device);
  callbacks.fetch_sub(1);
  return hr;
}
HRESULT WINAPI on_create_core(IDXGIFactory2* factory, IUnknown* device, IUnknown* window, const DXGI_SWAP_CHAIN_DESC1* desc, IDXGIOutput* output, IDXGISwapChain1** out) {
  callbacks.fetch_add(1);
  const auto hr = create_core_hook.call<HRESULT>(factory, device, window, desc, output, out);
  if (!stopping && SUCCEEDED(hr) && out) remember_swap(*out, device);
  callbacks.fetch_sub(1);
  return hr;
}
HRESULT WINAPI on_create_composition(IDXGIFactory2* factory, IUnknown* device, const DXGI_SWAP_CHAIN_DESC1* desc, IDXGIOutput* output, IDXGISwapChain1** out) {
  callbacks.fetch_add(1);
  const auto hr = create_composition_hook.call<HRESULT>(factory, device, desc, output, out);
  if (!stopping && SUCCEEDED(hr) && out) remember_swap(*out, device);
  callbacks.fetch_sub(1);
  return hr;
}void WINAPI on_execute(ID3D12CommandQueue* queue, UINT count, ID3D12CommandList* const* lists) {
  callbacks.fetch_add(1);
  if (!stopping && !overlay_submit && queue && queue->GetDesc().Type == D3D12_COMMAND_LIST_TYPE_DIRECT) {
    std::lock_guard lock(mutex);
    if (fallback_queue && fallback_queue.Get() != queue) fallback_ambiguous = true;
    else if (!fallback_ambiguous) fallback_queue = queue;
  }
  execute_hook.call<void>(queue, count, lists);
  callbacks.fetch_sub(1);
}
void descriptor_alloc(ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* cpu, D3D12_GPU_DESCRIPTOR_HANDLE* gpu) {
  const auto step = gfx.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  for (size_t i = 0; i < gfx.descriptors.size(); ++i) {
    if (gfx.descriptors[i]) continue;
    gfx.descriptors[i] = true;
    *cpu = info->SrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    *gpu = info->SrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
    cpu->ptr += i * step;
    gpu->ptr += i * step;
    return;
  }
  *cpu = {};
  *gpu = {};
}
void descriptor_free(ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpu, D3D12_GPU_DESCRIPTOR_HANDLE) {
  const auto base = gfx.srv_heap->GetCPUDescriptorHandleForHeapStart().ptr;
  const auto step = gfx.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  if (cpu.ptr < base || !step) return;
  const auto offset = cpu.ptr - base;
  if (offset % step == 0 && offset / step < gfx.descriptors.size()) gfx.descriptors[offset / step] = false;
}
bool wait_frame(const Frame& frame) {
  if (!frame.fence_value) return true;
  bool warned = false;
  while (gfx.fence->GetCompletedValue() < frame.fence_value) {
    if (gfx.device->GetDeviceRemovedReason() != S_OK) return true;
    const auto event_result = gfx.fence->SetEventOnCompletion(frame.fence_value, gfx.fence_event);
    const auto result = SUCCEEDED(event_result) ? WaitForSingleObject(gfx.fence_event, 5000) : WAIT_FAILED;
    if (result == WAIT_OBJECT_0) continue;
    if (!warned) { overlay_log("DX12 fence wait delayed; retaining GPU resources until safe"); warned = true; }
    Sleep(10);
  }
  return true;
}
bool wait_all() {
  if (gfx.untracked_submission) {
    overlay_log("DX12 queue signal failed; waiting for device removal before resource cleanup");
    while (gfx.device && gfx.device->GetDeviceRemovedReason() == S_OK) Sleep(50);
  }
  for (const auto& frame : gfx.frames) if (!wait_frame(frame)) return false;
  return true;
}void release_buffers() {
  for (auto& frame : gfx.frames) frame.buffer.Reset();
  gfx.frames.clear();
  gfx.rtv_heap.Reset();
  gfx.list.Reset();
}
void shutdown_graphics() {
  if (gfx.fence) (void)wait_all();
  if (gfx.imgui) { ImGui_ImplDX12_Shutdown(); gfx.imgui = false; }
  if (gfx.win32) { ImGui_ImplWin32_Shutdown(); gfx.win32 = false; }
  if (gfx.context) { ImGui::DestroyContext(); gfx.context = false; }
  release_buffers();
  gfx.srv_heap.Reset();
  gfx.fence.Reset();
  gfx.queue.Reset();
  gfx.device.Reset();
  if (gfx.fence_event) { CloseHandle(gfx.fence_event); gfx.fence_event = nullptr; }
  gfx.swap = nullptr;
  gfx.window = nullptr;
  gfx.untracked_submission = false;
  gfx.descriptors.fill(false);
}bool create_buffers(IDXGISwapChain* swap) {
  DXGI_SWAP_CHAIN_DESC desc{};
  if (FAILED(swap->GetDesc(&desc)) || !desc.BufferCount || desc.BufferCount > 16) return false;
  D3D12_DESCRIPTOR_HEAP_DESC heap{};
  heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  heap.NumDescriptors = desc.BufferCount;
  if (FAILED(gfx.device->CreateDescriptorHeap(&heap, IID_PPV_ARGS(&gfx.rtv_heap)))) return false;
  const UINT step = gfx.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  auto handle = gfx.rtv_heap->GetCPUDescriptorHandleForHeapStart();
  gfx.frames.resize(desc.BufferCount);
  for (auto& frame : gfx.frames) {
    const UINT index = static_cast<UINT>(&frame - gfx.frames.data());
    if (FAILED(swap->GetBuffer(index, IID_PPV_ARGS(&frame.buffer))) ||
        FAILED(gfx.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&frame.allocator)))) return false;
    frame.rtv = handle;
    gfx.device->CreateRenderTargetView(frame.buffer.Get(), nullptr, handle);
    handle.ptr += step;
  }
  if (FAILED(gfx.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        gfx.frames[0].allocator.Get(), nullptr, IID_PPV_ARGS(&gfx.list)))) return false;
  return SUCCEEDED(gfx.list->Close());
}
bool initialize_backend(const DXGI_SWAP_CHAIN_DESC& desc) {
  ImGui_ImplDX12_InitInfo init{};
  init.Device = gfx.device.Get();
  init.CommandQueue = gfx.queue.Get();
  init.NumFramesInFlight = static_cast<int>(gfx.frames.size());
  init.RTVFormat = desc.BufferDesc.Format;
  init.SrvDescriptorHeap = gfx.srv_heap.Get();
  init.SrvDescriptorAllocFn = descriptor_alloc;
  init.SrvDescriptorFreeFn = descriptor_free;
  gfx.imgui = ImGui_ImplDX12_Init(&init);
  return gfx.imgui;
}
bool initialize(IDXGISwapChain* swap, ID3D12CommandQueue* queue) {
  if (FAILED(swap->GetDevice(IID_PPV_ARGS(&gfx.device))) || !same_device(queue, gfx.device.Get())) return false;
  gfx.queue = queue;
  DXGI_SWAP_CHAIN_DESC desc{};
  if (FAILED(swap->GetDesc(&desc)) || !desc.OutputWindow) return false;
  gfx.window = desc.OutputWindow;
  gfx.fence_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  if (!gfx.fence_event || FAILED(gfx.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&gfx.fence)))) return false;
  D3D12_DESCRIPTOR_HEAP_DESC heap{};
  heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  heap.NumDescriptors = static_cast<UINT>(gfx.descriptors.size());
  heap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  if (FAILED(gfx.device->CreateDescriptorHeap(&heap, IID_PPV_ARGS(&gfx.srv_heap))) || !create_buffers(swap)) return false;
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  gfx.context = true;
  if (!ImGui_ImplWin32_Init(gfx.window)) return false;
  gfx.win32 = true;
  if (!initialize_backend(desc)) return false;
  gfx.swap = swap;
  overlay_log("DX12 overlay initialized");
  return true;
}void draw_icon() {
  if (!enabled || (!force_icon && !active)) return;
  auto* draw = ImGui::GetForegroundDrawList();
  const float x = static_cast<float>(x_pos.load()), y = static_cast<float>(y_pos.load()), s = scale.load();
  draw->AddRectFilled({x,y}, {x+44*s,y+44*s}, IM_COL32(12,20,28,210), 8*s);
  draw->AddRect({x,y}, {x+44*s,y+44*s}, IM_COL32(238,193,85,255), 8*s, 0, 2*s);
  draw->AddRectFilled({x+9*s,y+19*s}, {x+35*s,y+34*s}, IM_COL32(181,113,39,255), 2*s);
  draw->AddRect({x+9*s,y+13*s}, {x+35*s,y+24*s}, IM_COL32(246,209,112,255), 3*s, 0, 2*s);
  draw->AddLine({x+22*s,y+19*s}, {x+22*s,y+34*s}, IM_COL32(255,224,127,255), 3*s);
  draw->AddCircleFilled({x+22*s,y+26*s}, 2.5f*s, IM_COL32(39,30,20,255));
}
void render(IDXGISwapChain* swap) {
  if (!gfx.imgui || gfx.swap != swap || !gfx.queue || gfx.untracked_submission) return;
  ComPtr<IDXGISwapChain3> swap3;
  if (FAILED(swap->QueryInterface(IID_PPV_ARGS(&swap3)))) return;
  const UINT index = swap3->GetCurrentBackBufferIndex();
  if (index >= gfx.frames.size()) return;
  auto& frame = gfx.frames[index];
  if (!wait_frame(frame) || FAILED(frame.allocator->Reset()) ||
      FAILED(gfx.list->Reset(frame.allocator.Get(), nullptr))) return;
  ImGui_ImplDX12_NewFrame();
  ImGui_ImplWin32_NewFrame();
  ImGui::NewFrame();
  draw_icon();
  ImGui::Render();
  D3D12_RESOURCE_BARRIER barrier{};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Transition.pResource = frame.buffer.Get();
  barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
  gfx.list->ResourceBarrier(1, &barrier);
  gfx.list->OMSetRenderTargets(1, &frame.rtv, FALSE, nullptr);
  auto* srv = gfx.srv_heap.Get();
  gfx.list->SetDescriptorHeaps(1, &srv);
  ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), gfx.list.Get());
  std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
  gfx.list->ResourceBarrier(1, &barrier);
  if (FAILED(gfx.list->Close())) return;
  ID3D12CommandList* lists[] = {gfx.list.Get()};
  gfx.queue->ExecuteCommandLists(1, lists);
  const UINT64 value = gfx.fence_next++;
  if (SUCCEEDED(gfx.queue->Signal(gfx.fence.Get(), value))) frame.fence_value = value;
  else { gfx.untracked_submission = true; overlay_log("DX12 queue signal failed; rendering disabled"); }
}
HRESULT WINAPI on_present(IDXGISwapChain* swap, UINT interval, UINT flags) {
  callbacks.fetch_add(1);
  if (!stopping && !(flags & DXGI_PRESENT_TEST)) {
    overlay_submit = true;
    std::lock_guard lock(mutex);
    if (!gfx.imgui) {
      ComPtr<ID3D12Device> device;
      if (SUCCEEDED(swap->GetDevice(IID_PPV_ARGS(&device)))) {
        ID3D12CommandQueue* queue = (swap == selected_swap && same_device(selected_queue.Get(), device.Get()))
          ? selected_queue.Get() : (!selected_swap && !fallback_ambiguous ? fallback_queue.Get() : nullptr);
        if (queue && !selected_swap && !fallback_logged) { overlay_log("DX12 using single observed direct queue fallback"); fallback_logged = true; }
        if (same_device(queue, device.Get()) && !initialize(swap, queue)) { overlay_log("DX12 overlay initialization failed"); shutdown_graphics(); }
      }
    }
    if (gfx.imgui && gfx.swap == swap) render(swap);
    overlay_submit = false;
  }
  const auto result = present_hook.call<HRESULT>(swap, interval, flags);
  callbacks.fetch_sub(1);
  return result;
}
HRESULT WINAPI on_resize(IDXGISwapChain* swap, UINT count, UINT width, UINT height, DXGI_FORMAT format, UINT flags) {
  callbacks.fetch_add(1);
  {
    std::lock_guard lock(mutex);
    if (gfx.swap == swap) {
      (void)wait_all();
      if (gfx.imgui) { ImGui_ImplDX12_Shutdown(); gfx.imgui = false; }
      gfx.descriptors.fill(false);
      release_buffers();
    }
  }
  const auto hr = resize_hook.call<HRESULT>(swap, count, width, height, format, flags);
  {
    std::lock_guard lock(mutex);
    if (gfx.swap == swap) {
      DXGI_SWAP_CHAIN_DESC desc{};
      if (FAILED(swap->GetDesc(&desc)) || !create_buffers(swap) || !initialize_backend(desc)) {
        overlay_log("DX12 resize recreation failed; overlay disabled");
        shutdown_graphics();
      }
    }
  }
  callbacks.fetch_sub(1);
  return hr;
}void* method(void* object, size_t index) { return (*reinterpret_cast<void***>(object))[index]; }
}
bool start_overlay() {
  stopping = false;
  WNDCLASSW cls{}; cls.lpfnWndProc = DefWindowProcW; cls.hInstance = GetModuleHandleW(nullptr);
  cls.lpszClassName = L"PirateHatHUDDx12Probe";
  if (!RegisterClassW(&cls)) return false;
  HWND window = CreateWindowW(cls.lpszClassName, L"", WS_OVERLAPPEDWINDOW, 0,0,64,64,nullptr,nullptr,cls.hInstance,nullptr);
  ComPtr<ID3D12Device> device;
  ComPtr<ID3D12CommandQueue> queue;
  ComPtr<IDXGIFactory2> factory;
  ComPtr<IDXGISwapChain1> swap;
  bool ok = false;
  if (window && SUCCEEDED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)))) {
    D3D12_COMMAND_QUEUE_DESC q{};
    q.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    if (SUCCEEDED(device->CreateCommandQueue(&q, IID_PPV_ARGS(&queue))) &&
        SUCCEEDED(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)))) {
      DXGI_SWAP_CHAIN_DESC1 desc{};
      desc.BufferCount = 2; desc.Width = 64; desc.Height = 64;
      desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
      desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
      desc.SampleDesc.Count = 1;
      desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
      if (SUCCEEDED(factory->CreateSwapChainForHwnd(queue.Get(), window, &desc, nullptr, nullptr, &swap))) {
        using Flags = safetyhook::InlineHook::Flags;
        auto p = safetyhook::InlineHook::create(method(swap.Get(), 8), on_present, Flags::StartDisabled);
        auto r = safetyhook::InlineHook::create(method(swap.Get(), 13), on_resize, Flags::StartDisabled);
        auto c = safetyhook::InlineHook::create(method(factory.Get(), 10), on_create, Flags::StartDisabled);
        auto h = safetyhook::InlineHook::create(method(factory.Get(), 15), on_create_hwnd, Flags::StartDisabled);
        auto k = safetyhook::InlineHook::create(method(factory.Get(), 16), on_create_core, Flags::StartDisabled);
        auto m = safetyhook::InlineHook::create(method(factory.Get(), 24), on_create_composition, Flags::StartDisabled);
        auto e = safetyhook::InlineHook::create(method(queue.Get(), 10), on_execute, Flags::StartDisabled);
        if (p && r && c && h && k && m && e) {
          present_hook = std::move(*p); resize_hook = std::move(*r);
          create_hook = std::move(*c); create_hwnd_hook = std::move(*h); create_core_hook = std::move(*k); create_composition_hook = std::move(*m); execute_hook = std::move(*e);
          ok = create_hook.enable() && create_hwnd_hook.enable() && create_core_hook.enable() && create_composition_hook.enable() && execute_hook.enable() && resize_hook.enable() && present_hook.enable();
        } else {
          if (p) p->reset(); if (r) r->reset(); if (c) c->reset(); if (h) h->reset(); if (k) k->reset(); if (m) m->reset(); if (e) e->reset();
        }
      }
    }
  }
  swap.Reset(); factory.Reset(); queue.Reset(); device.Reset();
  if (window) DestroyWindow(window);
  UnregisterClassW(cls.lpszClassName, cls.hInstance);
  if (!ok) stop_overlay();
  return ok;
}
void stop_overlay() {
  stopping = true;
  if (create_hook) (void)create_hook.disable();
  if (create_hwnd_hook) (void)create_hwnd_hook.disable();
  if (create_core_hook) (void)create_core_hook.disable();
  if (create_composition_hook) (void)create_composition_hook.disable();
  if (execute_hook) (void)execute_hook.disable();
  if (resize_hook) (void)resize_hook.disable();
  if (present_hook) (void)present_hook.disable();
  while (callbacks.load()) Sleep(1);
  std::lock_guard lock(mutex);
  shutdown_graphics();
  selected_queue.Reset(); fallback_queue.Reset(); selected_swap = nullptr; fallback_ambiguous = false; fallback_logged = false;
  present_hook.reset(); resize_hook.reset(); create_hook.reset(); create_hwnd_hook.reset(); create_core_hook.reset(); create_composition_hook.reset(); execute_hook.reset();
}
void set_overlay_log(void (*callback)(const char*)) { logger = callback; }
void set_overlay_enabled(bool value) { enabled = value; }
void set_overlay_force(bool value) { force_icon = value; }
void set_overlay_position(int x, int y, float s) { x_pos = x; y_pos = y; scale = s; }
void set_overlay_state(bool value) { active = value; }
}
