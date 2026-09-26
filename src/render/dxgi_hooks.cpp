#include "render/dxgi_hooks.hpp"
#include <safetyhook.hpp>
#include <atomic>
#include <mutex>
#include <utility>

namespace phi::render {
namespace {
safetyhook::InlineHook present_hook, resize_hook, create_hook, create_hwnd_hook, create_core_hook,
    create_composition_hook, execute_hook;
std::atomic<unsigned> callbacks{0};
std::atomic<bool> stopping{false};
std::mutex mutex;
thread_local bool overlay_submit = false;
IDXGISwapChain* selected_swap{};
ComPtr<ID3D12CommandQueue> selected_queue;
ComPtr<ID3D12CommandQueue> fallback_queue;
bool fallback_ambiguous{};
bool fallback_logged{};
void (*logger)(const char*){};
void overlay_log(const char* message) {
  if (logger)
    logger(message);
}
Dx12Renderer* renderer{};
HudSnapshot snapshot{};
void remember_swap(IDXGISwapChain* swap, IUnknown* object) {
  if (!swap || !object)
    return;
  ComPtr<ID3D12CommandQueue> queue;
  if (FAILED(object->QueryInterface(IID_PPV_ARGS(&queue))))
    return;
  if (queue->GetDesc().Type != D3D12_COMMAND_LIST_TYPE_DIRECT)
    return;
  std::lock_guard lock(mutex);
  selected_swap = swap;
  selected_queue = queue;
  overlay_log("DX12 swapchain and present queue captured");
}
HRESULT WINAPI on_create(IDXGIFactory* factory, IUnknown* device, DXGI_SWAP_CHAIN_DESC* desc,
                         IDXGISwapChain** out) {
  callbacks.fetch_add(1);
  const auto hr = create_hook.call<HRESULT>(factory, device, desc, out);
  if (!stopping && SUCCEEDED(hr) && out)
    remember_swap(*out, device);
  callbacks.fetch_sub(1);
  return hr;
}
HRESULT WINAPI on_create_hwnd(IDXGIFactory2* factory, IUnknown* device, HWND hwnd,
                              const DXGI_SWAP_CHAIN_DESC1* desc,
                              const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* fullscreen,
                              IDXGIOutput* output, IDXGISwapChain1** out) {
  callbacks.fetch_add(1);
  const auto hr =
      create_hwnd_hook.call<HRESULT>(factory, device, hwnd, desc, fullscreen, output, out);
  if (!stopping && SUCCEEDED(hr) && out)
    remember_swap(*out, device);
  callbacks.fetch_sub(1);
  return hr;
}
HRESULT WINAPI on_create_core(IDXGIFactory2* factory, IUnknown* device, IUnknown* window,
                              const DXGI_SWAP_CHAIN_DESC1* desc, IDXGIOutput* output,
                              IDXGISwapChain1** out) {
  callbacks.fetch_add(1);
  const auto hr = create_core_hook.call<HRESULT>(factory, device, window, desc, output, out);
  if (!stopping && SUCCEEDED(hr) && out)
    remember_swap(*out, device);
  callbacks.fetch_sub(1);
  return hr;
}
HRESULT WINAPI on_create_composition(IDXGIFactory2* factory, IUnknown* device,
                                     const DXGI_SWAP_CHAIN_DESC1* desc, IDXGIOutput* output,
                                     IDXGISwapChain1** out) {
  callbacks.fetch_add(1);
  const auto hr = create_composition_hook.call<HRESULT>(factory, device, desc, output, out);
  if (!stopping && SUCCEEDED(hr) && out)
    remember_swap(*out, device);
  callbacks.fetch_sub(1);
  return hr;
}
void WINAPI on_execute(ID3D12CommandQueue* queue, UINT count, ID3D12CommandList* const* lists) {
  callbacks.fetch_add(1);
  if (!stopping && !overlay_submit && queue &&
      queue->GetDesc().Type == D3D12_COMMAND_LIST_TYPE_DIRECT) {
    std::lock_guard lock(mutex);
    if (fallback_queue && fallback_queue.Get() != queue)
      fallback_ambiguous = true;
    else if (!fallback_ambiguous)
      fallback_queue = queue;
  }
  execute_hook.call<void>(queue, count, lists);
  callbacks.fetch_sub(1);
}
HRESULT WINAPI on_present(IDXGISwapChain* swap, UINT interval, UINT flags) {
  callbacks.fetch_add(1);
  if (!stopping && !(flags & DXGI_PRESENT_TEST)) {
    overlay_submit = true;
    std::lock_guard lock(mutex);
    if (!renderer->ready()) {
      ComPtr<ID3D12Device> device;
      if (SUCCEEDED(swap->GetDevice(IID_PPV_ARGS(&device)))) {
        ID3D12CommandQueue* queue =
            (swap == selected_swap && same_device(selected_queue.Get(), device.Get()))
                ? selected_queue.Get()
                : (!selected_swap && !fallback_ambiguous ? fallback_queue.Get() : nullptr);
        if (queue && !selected_swap && !fallback_logged) {
          overlay_log("DX12 using single observed direct queue fallback");
          fallback_logged = true;
        }
        if (same_device(queue, device.Get()) && !renderer->initialize(swap, queue)) {
          overlay_log("DX12 overlay initialization failed");
          renderer->shutdown();
        }
      }
    }
    if (renderer->ready() && renderer->handles(swap))
      renderer->render(swap, snapshot());
    overlay_submit = false;
  }
  const auto result = present_hook.call<HRESULT>(swap, interval, flags);
  callbacks.fetch_sub(1);
  return result;
}
HRESULT WINAPI on_resize(IDXGISwapChain* swap, UINT count, UINT width, UINT height,
                         DXGI_FORMAT format, UINT flags) {
  callbacks.fetch_add(1);
  {
    std::lock_guard lock(mutex);
    renderer->before_resize(swap);
  }
  const auto hr = resize_hook.call<HRESULT>(swap, count, width, height, format, flags);
  {
    std::lock_guard lock(mutex);
    renderer->after_resize(swap);
  }
  callbacks.fetch_sub(1);
  return hr;
}
void* method(void* object, size_t index) {
  return (*reinterpret_cast<void***>(object))[index];
}
} // namespace
bool start_hooks(Dx12Renderer& target, HudSnapshot hud_snapshot,
                 void (*log_callback)(const char*)) {
  renderer = &target;
  snapshot = hud_snapshot;
  logger = log_callback;
  stopping = false;
  WNDCLASSW cls{};
  cls.lpfnWndProc = DefWindowProcW;
  cls.hInstance = GetModuleHandleW(nullptr);
  cls.lpszClassName = L"PirateHatHUDDx12Probe";
  if (!RegisterClassW(&cls))
    return false;
  HWND window = CreateWindowW(cls.lpszClassName, L"", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr,
                              nullptr, cls.hInstance, nullptr);
  ComPtr<ID3D12Device> device;
  ComPtr<ID3D12CommandQueue> queue;
  ComPtr<IDXGIFactory2> factory;
  ComPtr<IDXGISwapChain1> swap;
  bool ok = false;
  if (window &&
      SUCCEEDED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)))) {
    D3D12_COMMAND_QUEUE_DESC q{};
    q.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    if (SUCCEEDED(device->CreateCommandQueue(&q, IID_PPV_ARGS(&queue))) &&
        SUCCEEDED(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)))) {
      DXGI_SWAP_CHAIN_DESC1 desc{};
      desc.BufferCount = 2;
      desc.Width = 64;
      desc.Height = 64;
      desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
      desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
      desc.SampleDesc.Count = 1;
      desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
      if (SUCCEEDED(factory->CreateSwapChainForHwnd(queue.Get(), window, &desc, nullptr, nullptr,
                                                    &swap))) {
        using Flags = safetyhook::InlineHook::Flags;
        auto p =
            safetyhook::InlineHook::create(method(swap.Get(), 8), on_present, Flags::StartDisabled);
        auto r =
            safetyhook::InlineHook::create(method(swap.Get(), 13), on_resize, Flags::StartDisabled);
        auto c = safetyhook::InlineHook::create(method(factory.Get(), 10), on_create,
                                                Flags::StartDisabled);
        auto h = safetyhook::InlineHook::create(method(factory.Get(), 15), on_create_hwnd,
                                                Flags::StartDisabled);
        auto k = safetyhook::InlineHook::create(method(factory.Get(), 16), on_create_core,
                                                Flags::StartDisabled);
        auto m = safetyhook::InlineHook::create(method(factory.Get(), 24), on_create_composition,
                                                Flags::StartDisabled);
        auto e = safetyhook::InlineHook::create(method(queue.Get(), 10), on_execute,
                                                Flags::StartDisabled);
        if (p && r && c && h && k && m && e) {
          present_hook = std::move(*p);
          resize_hook = std::move(*r);
          create_hook = std::move(*c);
          create_hwnd_hook = std::move(*h);
          create_core_hook = std::move(*k);
          create_composition_hook = std::move(*m);
          execute_hook = std::move(*e);
          ok = create_hook.enable() && create_hwnd_hook.enable() && create_core_hook.enable() &&
               create_composition_hook.enable() && execute_hook.enable() && resize_hook.enable() &&
               present_hook.enable();
        } else {
          if (p)
            p->reset();
          if (r)
            r->reset();
          if (c)
            c->reset();
          if (h)
            h->reset();
          if (k)
            k->reset();
          if (m)
            m->reset();
          if (e)
            e->reset();
        }
      }
    }
  }
  swap.Reset();
  factory.Reset();
  queue.Reset();
  device.Reset();
  if (window)
    DestroyWindow(window);
  UnregisterClassW(cls.lpszClassName, cls.hInstance);
  if (!ok)
    stop_hooks();
  return ok;
}
void stop_hooks() {
  stopping = true;
  if (create_hook)
    (void)create_hook.disable();
  if (create_hwnd_hook)
    (void)create_hwnd_hook.disable();
  if (create_core_hook)
    (void)create_core_hook.disable();
  if (create_composition_hook)
    (void)create_composition_hook.disable();
  if (execute_hook)
    (void)execute_hook.disable();
  if (resize_hook)
    (void)resize_hook.disable();
  if (present_hook)
    (void)present_hook.disable();
  while (callbacks.load())
    Sleep(1);
  std::lock_guard lock(mutex);
  renderer->shutdown();
  selected_queue.Reset();
  fallback_queue.Reset();
  selected_swap = nullptr;
  fallback_ambiguous = false;
  fallback_logged = false;
  present_hook.reset();
  resize_hook.reset();
  create_hook.reset();
  create_hwnd_hook.reset();
  create_core_hook.reset();
  create_composition_hook.reset();
  execute_hook.reset();
  renderer = nullptr;
  snapshot = nullptr;
  logger = nullptr;
}

} // namespace phi::render
