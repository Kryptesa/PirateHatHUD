#include "render/dxgi_hooks.hpp"
#include <safetyhook.hpp>
#include <atomic>
#include <mutex>
#include <utility>
#include <unordered_map>

namespace phi::render {
namespace {
struct HookStorage {
  safetyhook::InlineHook present_hook, resize_hook, resize1_hook, create_hook, create_hwnd_hook,
      create_core_hook, create_composition_hook, execute_hook, color_hook;
};
// Activated trampoline code can remain on thread stacks beyond the C++ callback
// count. Keep its allocation alive, including through static destruction.
HookStorage& hooks = *new HookStorage;
auto& present_hook = hooks.present_hook;
auto& resize_hook = hooks.resize_hook;
auto& resize1_hook = hooks.resize1_hook;
auto& create_hook = hooks.create_hook;
auto& create_hwnd_hook = hooks.create_hwnd_hook;
auto& create_core_hook = hooks.create_core_hook;
auto& create_composition_hook = hooks.create_composition_hook;
auto& execute_hook = hooks.execute_hook;
auto& color_hook = hooks.color_hook;
bool activation_attempted{};
HooksStopResult stop_result{true, true, true, false};
std::atomic<unsigned> callbacks{0};
std::atomic<bool> stopping{false};
std::mutex mutex;
std::unordered_map<IDXGISwapChain*, DXGI_COLOR_SPACE_TYPE> color_spaces;
thread_local bool overlay_submit = false;
struct CallbackGuard {
  CallbackGuard() {
    callbacks.fetch_add(1);
  }
  ~CallbackGuard() {
    callbacks.fetch_sub(1);
  }
};
struct SubmitGuard {
  bool previous{overlay_submit};
  SubmitGuard() {
    overlay_submit = true;
  }
  ~SubmitGuard() {
    overlay_submit = previous;
  }
};
template <class Function> void own_work(Function&& function) noexcept {
  try {
    function();
  } catch (...) {
    // Never let application callbacks unwind through a COM hook. Stop our work;
    // the original graphics call still executes exactly once.
    stopping = true;
  }
}
IDXGISwapChain* selected_swap{};
ComPtr<ID3D12CommandQueue> selected_queue;
ComPtr<ID3D12CommandQueue> fallback_queue;
bool fallback_ambiguous{};
bool fallback_logged{};
LogCallback logger{};
void overlay_log(LogLevel level, const char* message) {
  if (logger) {
    logger(level, message);
  }
}
Dx12Renderer* renderer{};
HudSnapshot snapshot{};
void remember_swap(IDXGISwapChain* swap, IUnknown* object) {
  if (!swap || !object) {
    return;
  }
  ComPtr<ID3D12CommandQueue> queue;
  if (FAILED(object->QueryInterface(IID_PPV_ARGS(&queue)))) {
    return;
  }
  if (queue->GetDesc().Type != D3D12_COMMAND_LIST_TYPE_DIRECT) {
    return;
  }
  std::lock_guard lock(mutex);
  color_spaces.erase(swap);
  selected_swap = swap;
  selected_queue = queue;
  overlay_log(LogLevel::info, "DX12 swapchain and present queue captured");
}
HRESULT WINAPI on_create(IDXGIFactory* factory, IUnknown* device, DXGI_SWAP_CHAIN_DESC* desc,
                         IDXGISwapChain** out) {
  CallbackGuard callback;
  const auto hr = create_hook.call<HRESULT>(factory, device, desc, out);
  own_work([&] {
    if (!stopping && SUCCEEDED(hr) && out) {
      remember_swap(*out, device);
    }
  });
  return hr;
}
HRESULT WINAPI on_create_hwnd(IDXGIFactory2* factory, IUnknown* device, HWND hwnd,
                              const DXGI_SWAP_CHAIN_DESC1* desc,
                              const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* fullscreen,
                              IDXGIOutput* output, IDXGISwapChain1** out) {
  CallbackGuard callback;
  const auto hr =
      create_hwnd_hook.call<HRESULT>(factory, device, hwnd, desc, fullscreen, output, out);
  own_work([&] {
    if (!stopping && SUCCEEDED(hr) && out) {
      remember_swap(*out, device);
    }
  });
  return hr;
}
HRESULT WINAPI on_create_core(IDXGIFactory2* factory, IUnknown* device, IUnknown* window,
                              const DXGI_SWAP_CHAIN_DESC1* desc, IDXGIOutput* output,
                              IDXGISwapChain1** out) {
  CallbackGuard callback;
  const auto hr = create_core_hook.call<HRESULT>(factory, device, window, desc, output, out);
  own_work([&] {
    if (!stopping && SUCCEEDED(hr) && out) {
      remember_swap(*out, device);
    }
  });
  return hr;
}
HRESULT WINAPI on_create_composition(IDXGIFactory2* factory, IUnknown* device,
                                     const DXGI_SWAP_CHAIN_DESC1* desc, IDXGIOutput* output,
                                     IDXGISwapChain1** out) {
  CallbackGuard callback;
  const auto hr = create_composition_hook.call<HRESULT>(factory, device, desc, output, out);
  own_work([&] {
    if (!stopping && SUCCEEDED(hr) && out) {
      remember_swap(*out, device);
    }
  });
  return hr;
}
void WINAPI on_execute(ID3D12CommandQueue* queue, UINT count, ID3D12CommandList* const* lists) {
  CallbackGuard callback;
  own_work([&] {
    if (!stopping && !overlay_submit && queue &&
        queue->GetDesc().Type == D3D12_COMMAND_LIST_TYPE_DIRECT) {
      std::lock_guard lock(mutex);
      if (fallback_queue && fallback_queue.Get() != queue) {
        fallback_ambiguous = true;
      } else if (!fallback_ambiguous) {
        fallback_queue = queue;
      }
    }
  });
  execute_hook.call<void>(queue, count, lists);
}
HRESULT WINAPI on_color(IDXGISwapChain3* swap, DXGI_COLOR_SPACE_TYPE space) {
  CallbackGuard callback;
  const auto hr = color_hook.call<HRESULT>(swap, space);
  own_work([&] {
    if (!stopping && SUCCEEDED(hr)) {
      std::lock_guard lock(mutex);
      color_spaces[swap] = space;
    }
  });
  return hr;
}
HRESULT WINAPI on_present(IDXGISwapChain* swap, UINT interval, UINT flags) {
  CallbackGuard callback;
  own_work([&] {
    if (!stopping && !(flags & DXGI_PRESENT_TEST)) {
      SubmitGuard submit;
      std::lock_guard lock(mutex);
      if (renderer && swap == selected_swap && !renderer->handles(swap) &&
          renderer->state() != RendererState::waiting) {
        renderer->replace_swapchain(swap, selected_queue.Get());
      }
      if (renderer && renderer->state() == RendererState::waiting) {
        ComPtr<ID3D12Device> device;
        if (SUCCEEDED(swap->GetDevice(IID_PPV_ARGS(&device)))) {
          ID3D12CommandQueue* queue =
              (swap == selected_swap && same_device(selected_queue.Get(), device.Get()))
                  ? selected_queue.Get()
                  : (!selected_swap && !fallback_ambiguous ? fallback_queue.Get() : nullptr);
          if (queue && !selected_swap && !fallback_logged) {
            overlay_log(LogLevel::info, "DX12 using single observed direct queue fallback");
            fallback_logged = true;
          }
          if (same_device(queue, device.Get()) && !renderer->initialize(swap, queue)) {
            overlay_log(LogLevel::error, "DX12 overlay initialization failed");
            renderer->shutdown();
          }
        }
      }
      if (renderer && renderer->ready() && renderer->handles(swap)) {
        DXGI_SWAP_CHAIN_DESC desc{};
        swap->GetDesc(&desc);
        // FP16 defaults to scRGB. A pre-existing 10-bit chain is ambiguous;
        // retain SDR until a successful SetColorSpace1 is observed.
        auto space = desc.BufferDesc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT
                         ? DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709
                         : DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
        if (const auto it = color_spaces.find(swap); it != color_spaces.end()) {
          space = it->second;
        }
        renderer->render(swap, snapshot(), space);
      }
    }
  });
  const auto result = present_hook.call<HRESULT>(swap, interval, flags);
  return result;
}
thread_local bool resize_active = false;
template <class Function>
HRESULT resize_swap(IDXGISwapChain* swap, Function&& original, UINT count = 0,
                    IUnknown* const* queues = nullptr) {
  CallbackGuard callback;
  // A DXGI implementation may route ResizeBuffers1 through ResizeBuffers.
  // Only the outer call releases and recreates renderer resources.
  if (resize_active) {
    return original();
  }
  struct ResizeGuard {
    ResizeGuard() {
      resize_active = true;
    }
    ~ResizeGuard() {
      resize_active = false;
    }
  } guard;
  bool prepared = false;
  own_work([&] {
    if (!stopping) {
      std::lock_guard lock(mutex);
      if (renderer) {
        prepared = renderer->before_resize(swap);
      }
    }
  });
  const auto hr = original();
  own_work([&] {
    if (!stopping && prepared) {
      std::lock_guard lock(mutex);
      if (renderer) {
        renderer->after_resize(swap, hr, count, queues);
      }
    }
  });
  return hr;
}
HRESULT WINAPI on_resize(IDXGISwapChain* swap, UINT count, UINT width, UINT height,
                         DXGI_FORMAT format, UINT flags) {
  return resize_swap(
      swap, [&] { return resize_hook.call<HRESULT>(swap, count, width, height, format, flags); });
}
HRESULT WINAPI on_resize1(IDXGISwapChain3* swap, UINT count, UINT width, UINT height,
                          DXGI_FORMAT format, UINT flags, const UINT* node_masks,
                          IUnknown* const* queues) {
  return resize_swap(
      swap,
      [&] {
        return resize1_hook.call<HRESULT>(swap, count, width, height, format, flags, node_masks,
                                          queues);
      },
      count, queues);
}
void* method(void* object, size_t index) {
  return (*reinterpret_cast<void***>(object))[index];
}
} // namespace
bool start_hooks(Dx12Renderer& target, HudSnapshot hud_snapshot, LogCallback log_callback) {
  if (activation_attempted || renderer) {
    return false;
  }
  renderer = &target;
  snapshot = hud_snapshot;
  logger = log_callback;
  stopping = false;
  WNDCLASSW cls{};
  cls.lpfnWndProc = DefWindowProcW;
  cls.hInstance = GetModuleHandleW(nullptr);
  cls.lpszClassName = L"PirateHatHUDDx12Probe";
  if (!RegisterClassW(&cls)) {
    return false;
  }
  HWND window = CreateWindowW(cls.lpszClassName, L"", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr,
                              nullptr, cls.hInstance, nullptr);
  ComPtr<ID3D12Device> device;
  ComPtr<ID3D12CommandQueue> queue;
  ComPtr<IDXGIFactory2> factory;
  ComPtr<IDXGISwapChain1> swap;
  ComPtr<IDXGISwapChain3> swap3;
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
                                                    &swap)) &&
          SUCCEEDED(swap.As(&swap3))) {
        using Flags = safetyhook::InlineHook::Flags;
        auto p =
            safetyhook::InlineHook::create(method(swap.Get(), 8), on_present, Flags::StartDisabled);
        auto r =
            safetyhook::InlineHook::create(method(swap.Get(), 13), on_resize, Flags::StartDisabled);
        auto r1 = safetyhook::InlineHook::create(method(swap3.Get(), 39), on_resize1,
                                                 Flags::StartDisabled);
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
        auto color =
            safetyhook::InlineHook::create(method(swap3.Get(), 38), on_color, Flags::StartDisabled);
        if (p && r && r1 && c && h && k && m && e && color) {
          present_hook = std::move(*p);
          resize_hook = std::move(*r);
          resize1_hook = std::move(*r1);
          create_hook = std::move(*c);
          create_hwnd_hook = std::move(*h);
          create_core_hook = std::move(*k);
          create_composition_hook = std::move(*m);
          execute_hook = std::move(*e);
          color_hook = std::move(*color);
          activation_attempted = true;
          stop_result.module_must_remain_loaded = true;
          ok = create_hook.enable() && create_hwnd_hook.enable() && create_core_hook.enable() &&
               create_composition_hook.enable() && execute_hook.enable() && resize_hook.enable() &&
               resize1_hook.enable() && color_hook.enable() && present_hook.enable();
        } else {
          if (p) {
            p->reset();
          }
          if (r) {
            r->reset();
          }
          if (r1) {
            r1->reset();
          }
          if (c) {
            c->reset();
          }
          if (h) {
            h->reset();
          }
          if (k) {
            k->reset();
          }
          if (m) {
            m->reset();
          }
          if (e) {
            e->reset();
          }
          if (color) {
            color->reset();
          }
        }
      }
    }
  }
  swap.Reset();
  factory.Reset();
  queue.Reset();
  device.Reset();
  if (window) {
    DestroyWindow(window);
  }
  UnregisterClassW(cls.lpszClassName, cls.hInstance);
  if (!ok) {
    stop_hooks();
  }
  return ok;
}
HooksStopResult stop_hooks() noexcept {
  stopping = true;
  if (!renderer) {
    return stop_result;
  }
  bool disabled = true;
  for (auto* hook : {&create_hook, &create_hwnd_hook, &create_core_hook, &create_composition_hook,
                     &execute_hook, &resize_hook, &resize1_hook, &color_hook, &present_hook}) {
    try {
      if (*hook && !hook->disable()) {
        disabled = false;
      }
    } catch (...) {
      disabled = false;
    }
  }
  stop_result.hooks_disabled = disabled;
  stop_result.module_must_remain_loaded = activation_attempted || !disabled;
  const auto deadline = GetTickCount64() + 1000;
  while (callbacks.load() && GetTickCount64() < deadline) {
    Sleep(1);
  }
  stop_result.callbacks_drained = callbacks.load() == 0;
  if (!disabled || !stop_result.callbacks_drained) {
    stop_result.gpu_resources_released = false;
    return stop_result;
  }
  try {
    std::lock_guard lock(mutex);
    stop_result.gpu_resources_released = renderer->shutdown() == ReleaseResult::released;
    // No further own work is admitted after stopping, including late hook entries.
    renderer->set_logger(nullptr);
    selected_queue.Reset();
    fallback_queue.Reset();
    selected_swap = nullptr;
    color_spaces.clear();
    fallback_ambiguous = false;
    fallback_logged = false;
    renderer = nullptr;
    snapshot = nullptr;
    logger = nullptr;
    if (!activation_attempted) {
      present_hook.reset();
      resize_hook.reset();
      resize1_hook.reset();
      create_hook.reset();
      create_hwnd_hook.reset();
      create_core_hook.reset();
      create_composition_hook.reset();
      execute_hook.reset();
      color_hook.reset();
    }
  } catch (...) {
    stop_result.gpu_resources_released = false;
    stop_result.module_must_remain_loaded = true;
  }
  return stop_result;
}

} // namespace phi::render
