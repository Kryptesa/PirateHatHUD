#include "render/dxgi_hooks.hpp"
#include "render/dxgi_diagnostics.hpp"
#include "render/dxgi_probe.hpp"
#include "render/dxgi_swapchains.hpp"
#include <safetyhook.hpp>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <utility>

namespace phi::render {
namespace {
struct HookStorage {
  safetyhook::InlineHook present_hook, resize_hook, resize1_hook, create_hook, create_hwnd_hook,
    create_core_hook, create_composition_hook, execute_hook, color_hook;
};

struct HookContext {
  HookStorage hooks;
  bool activation_attempted{};
  HooksStopResult stop_result{true, true, true, false};
  std::atomic<unsigned> callbacks{0};
  std::atomic<bool> stopping{false};
  std::mutex mutex;
  // Protected by mutex; bridges ReShade add-on lifetime until the game renderer is ready.
  // If startup stops first, the reference deliberately survives through process exit.
  ComPtr<ID3D12Device> probe_device;
  // Protected by mutex; suppresses Present-side work during concurrent resize callbacks.
  unsigned resizes_active{};
  DxgiSwapchains swapchains;
  DxgiDiagnostics diagnostics;
  Dx12Renderer* renderer{};
  HudSnapshot snapshot{};
};

// Activated trampolines and an unhanded probe may outlive stop, including DLL detach.
// Never give this context an automatic destructor during process shutdown.
HookContext& context = *new HookContext;

bool sample_callback(unsigned index, bool eligible = true) noexcept {
  return context.diagnostics.sample(
    index,
    eligible && !context.stopping.load(std::memory_order_relaxed)
  );
}

thread_local bool overlay_submit = false;

struct CallbackGuard {
  CallbackGuard() {
    context.callbacks.fetch_add(1);
  }

  ~CallbackGuard() {
    context.callbacks.fetch_sub(1);
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
    context.stopping = true;
  }
}

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

  std::lock_guard lock(context.mutex);

  context.swapchains.created(swap, queue.Get());
}

HRESULT WINAPI on_create(
  IDXGIFactory* factory,
  IUnknown* device,
  DXGI_SWAP_CHAIN_DESC* desc,
  IDXGISwapChain** out
) {
  CallbackGuard callback;
  const bool diagnostic = sample_callback(0);
  context.diagnostics.progress(diagnostic, "CreateSwapChain", CallbackStage::entered);
  context.diagnostics.progress(diagnostic, "CreateSwapChain", CallbackStage::original_begin);
  const auto hr = context.hooks.create_hook.call<HRESULT>(factory, device, desc, out);
  context.diagnostics.progress(diagnostic, "CreateSwapChain", CallbackStage::original_returned, hr);
  own_work([&] {
    if (!context.stopping && SUCCEEDED(hr) && out) {
      remember_swap(*out, device);
    }
  });

  context.diagnostics.progress(diagnostic, "CreateSwapChain", CallbackStage::completed);
  return hr;
}

HRESULT WINAPI on_create_hwnd(
  IDXGIFactory2* factory,
  IUnknown* device,
  HWND hwnd,
  const DXGI_SWAP_CHAIN_DESC1* desc,
  const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* fullscreen,
  IDXGIOutput* output,
  IDXGISwapChain1** out
) {
  CallbackGuard callback;
  const bool diagnostic = sample_callback(1);
  context.diagnostics.progress(diagnostic, "CreateSwapChainForHwnd", CallbackStage::entered);
  context.diagnostics.progress(diagnostic, "CreateSwapChainForHwnd", CallbackStage::original_begin);
  const auto hr = context.hooks.create_hwnd_hook
                    .call<HRESULT>(factory, device, hwnd, desc, fullscreen, output, out);
  context.diagnostics
    .progress(diagnostic, "CreateSwapChainForHwnd", CallbackStage::original_returned, hr);
  own_work([&] {
    if (!context.stopping && SUCCEEDED(hr) && out) {
      remember_swap(*out, device);
    }
  });

  context.diagnostics.progress(diagnostic, "CreateSwapChainForHwnd", CallbackStage::completed);
  return hr;
}

HRESULT WINAPI on_create_core(
  IDXGIFactory2* factory,
  IUnknown* device,
  IUnknown* window,
  const DXGI_SWAP_CHAIN_DESC1* desc,
  IDXGIOutput* output,
  IDXGISwapChain1** out
) {
  CallbackGuard callback;
  const bool diagnostic = sample_callback(2);
  context.diagnostics.progress(diagnostic, "CreateSwapChainForCoreWindow", CallbackStage::entered);
  context.diagnostics
    .progress(diagnostic, "CreateSwapChainForCoreWindow", CallbackStage::original_begin);
  const auto hr =
    context.hooks.create_core_hook.call<HRESULT>(factory, device, window, desc, output, out);
  context.diagnostics
    .progress(diagnostic, "CreateSwapChainForCoreWindow", CallbackStage::original_returned, hr);
  own_work([&] {
    if (!context.stopping && SUCCEEDED(hr) && out) {
      remember_swap(*out, device);
    }
  });

  context.diagnostics
    .progress(diagnostic, "CreateSwapChainForCoreWindow", CallbackStage::completed);
  return hr;
}

HRESULT WINAPI on_create_composition(
  IDXGIFactory2* factory,
  IUnknown* device,
  const DXGI_SWAP_CHAIN_DESC1* desc,
  IDXGIOutput* output,
  IDXGISwapChain1** out
) {
  CallbackGuard callback;
  const bool diagnostic = sample_callback(3);
  context.diagnostics.progress(diagnostic, "CreateSwapChainForComposition", CallbackStage::entered);
  context.diagnostics
    .progress(diagnostic, "CreateSwapChainForComposition", CallbackStage::original_begin);
  const auto hr =
    context.hooks.create_composition_hook.call<HRESULT>(factory, device, desc, output, out);
  context.diagnostics
    .progress(diagnostic, "CreateSwapChainForComposition", CallbackStage::original_returned, hr);
  own_work([&] {
    if (!context.stopping && SUCCEEDED(hr) && out) {
      remember_swap(*out, device);
    }
  });

  context.diagnostics
    .progress(diagnostic, "CreateSwapChainForComposition", CallbackStage::completed);
  return hr;
}

void WINAPI on_execute(ID3D12CommandQueue* queue, UINT count, ID3D12CommandList* const* lists) {
  CallbackGuard callback;
  const bool diagnostic = sample_callback(4, !overlay_submit);
  if (diagnostic) {
    context.diagnostics.progress(diagnostic, "ExecuteCommandLists", CallbackStage::entered);
  }
  own_work([&] {
    if (
      !context.stopping &&
      !overlay_submit &&
      queue &&
      queue->GetDesc().Type == D3D12_COMMAND_LIST_TYPE_DIRECT
    ) {
      std::lock_guard lock(context.mutex);
      ComPtr<ID3D12Device> device;

      if (SUCCEEDED(queue->GetDevice(IID_PPV_ARGS(&device)))) {
        context.swapchains.observe_queue(device.Get(), queue);
      }
    }
  });

  if (diagnostic) {
    context.diagnostics.progress(diagnostic, "ExecuteCommandLists", CallbackStage::original_begin);
  }
  context.hooks.execute_hook.call<void>(queue, count, lists);
  if (diagnostic) {
    // ExecuteCommandLists has no HRESULT; report completion without inventing one.
    context.diagnostics.progress(diagnostic, "ExecuteCommandLists", CallbackStage::completed);
  }
}

HRESULT WINAPI on_color(IDXGISwapChain3* swap, DXGI_COLOR_SPACE_TYPE space) {
  CallbackGuard callback;
  const bool diagnostic = sample_callback(5);
  context.diagnostics.progress(diagnostic, "SetColorSpace1", CallbackStage::entered);
  context.diagnostics.progress(diagnostic, "SetColorSpace1", CallbackStage::original_begin);
  const auto hr = context.hooks.color_hook.call<HRESULT>(swap, space);
  context.diagnostics.progress(diagnostic, "SetColorSpace1", CallbackStage::original_returned, hr);
  own_work([&] {
    if (!context.stopping) {
      std::lock_guard lock(context.mutex);

      context.swapchains.color_changed(swap, space, hr, context.diagnostics);
    }
  });

  context.diagnostics.progress(diagnostic, "SetColorSpace1", CallbackStage::completed);
  return hr;
}

HRESULT WINAPI on_present(IDXGISwapChain* swap, UINT interval, UINT flags) {
  CallbackGuard callback;
  const bool diagnostic = sample_callback(6, !(flags & DXGI_PRESENT_TEST));
  if (diagnostic) {
    context.diagnostics.progress(diagnostic, "non-test Present", CallbackStage::entered);
  }
  own_work([&] {
    if (!context.stopping && !(flags & DXGI_PRESENT_TEST)) {
      SubmitGuard submit;

      std::lock_guard lock(context.mutex);
      if (context.stopping || context.resizes_active) {
        return;
      }
      if (!context.renderer) {
        return;
      }
      auto target = context.swapchains.select(swap, context.diagnostics);
      if (!target) {
        return;
      }
      if (!context.renderer->handles(swap) && context.renderer->state() != RendererState::waiting) {
        context.renderer->replace_swapchain(swap, target.queue.Get());
      }

      if (
        context.renderer->state() == RendererState::waiting &&
        !context.renderer->initialize(swap, target.queue.Get())
      ) {
        context.diagnostics.log(LogLevel::error, "DX12 overlay initialization failed");
        context.renderer->shutdown();
      }

      if (context.renderer && context.renderer->ready() && context.renderer->handles(swap)) {
        context.renderer->render(swap, context.snapshot(), target.space);
      }
    }
  });

  if (diagnostic) {
    context.diagnostics.progress(diagnostic, "non-test Present", CallbackStage::original_begin);
  }
  const auto result = context.hooks.present_hook.call<HRESULT>(swap, interval, flags);
  if (diagnostic) {
    context.diagnostics
      .progress(diagnostic, "non-test Present", CallbackStage::original_returned, result);
  }

  ComPtr<ID3D12Device> retired_probe;
  own_work([&] {
    std::lock_guard lock(context.mutex);
    if (!context.stopping && !context.resizes_active && context.renderer) {
      SubmitGuard submit;
      context.renderer->after_present(swap, result, flags);
      if (
        result == S_OK &&
        !(flags & DXGI_PRESENT_TEST) &&
        context.renderer->ready() &&
        context.renderer->handles(swap)
      ) {
        retired_probe = std::move(context.probe_device);
      }
    }
  });

  if (retired_probe) {
    // Release outside our graphics mutex: device destruction can invoke third-party callbacks.
    retired_probe.Reset();
    context.diagnostics.log(
      LogLevel::debug,
      "DX12 probe device released after game renderer became ready"
    );
  }

  if (diagnostic) {
    context.diagnostics.progress(diagnostic, "non-test Present", CallbackStage::completed);
  }
  return result;
}

thread_local bool resize_active = false;

struct ResizeRequest {
  const char* operation;
  UINT count, width, height;
  DXGI_FORMAT format;
  UINT flags;
};

template <class Function>
HRESULT resize_swap(
  IDXGISwapChain* swap,
  Function&& original,
  const ResizeRequest& request,
  IUnknown* const* queues = nullptr
) {
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
  const unsigned diagnostic_index = std::strcmp(request.operation, "ResizeBuffers1") == 0 ? 8 : 7;
  const bool diagnostic = sample_callback(diagnostic_index);
  context.diagnostics.progress(diagnostic, request.operation, CallbackStage::entered);
  bool prepared = false;
  bool admitted = false;
  const auto count = request.count;
  own_work([&] {
    std::lock_guard lock(context.mutex);
    if (!context.stopping) {
      ++context.resizes_active;
      admitted = true;

      char message[256]{};
      std::snprintf(
        message,
        sizeof(message),
        "DX12 %s begin: requested=%ux%u; format=%u; buffers=%u; flags=0x%X; explicit queues=%s",
        request.operation,
        request.width,
        request.height,
        static_cast<unsigned>(request.format),
        count,
        request.flags,
        queues ? "yes" : "no"
      );
      context.diagnostics.log(LogLevel::debug, message);

      if (context.renderer) {
        prepared = context.renderer->before_resize(swap);
      }
    }
  });

  context.diagnostics.progress(diagnostic, request.operation, CallbackStage::original_begin);
  const auto hr = original();
  context.diagnostics.progress(diagnostic, request.operation, CallbackStage::original_returned, hr);
  own_work([&] {
    std::lock_guard lock(context.mutex);
    if (admitted) {
      --context.resizes_active;
    }

    if (!context.stopping) {
      char message[128]{};
      std::snprintf(
        message,
        sizeof(message),
        "DX12 %s returned; HRESULT=0x%08lX",
        request.operation,
        static_cast<unsigned long>(hr)
      );
      context.diagnostics.log(FAILED(hr) ? LogLevel::error : LogLevel::debug, message);
      if (SUCCEEDED(hr) && count && queues) {
        context.swapchains.resized(swap, count, queues);
      }

      if (prepared && context.renderer) {
        context.renderer->after_resize(swap, hr, count, queues);
      }
    }
  });

  context.diagnostics.progress(diagnostic, request.operation, CallbackStage::completed);
  return hr;
}

HRESULT WINAPI on_resize(
  IDXGISwapChain* swap,
  UINT count,
  UINT width,
  UINT height,
  DXGI_FORMAT format,
  UINT flags
) {
  return resize_swap(
    swap,
    [&] {
      return context.hooks.resize_hook.call<HRESULT>(swap, count, width, height, format, flags);
    },
    {"ResizeBuffers", count, width, height, format, flags}
  );
}

HRESULT WINAPI on_resize1(
  IDXGISwapChain3* swap,
  UINT count,
  UINT width,
  UINT height,
  DXGI_FORMAT format,
  UINT flags,
  const UINT* node_masks,
  IUnknown* const* queues
) {
  return resize_swap(
    swap,
    [&] {
      return context.hooks.resize1_hook
        .call<HRESULT>(swap, count, width, height, format, flags, node_masks, queues);
    },
    {"ResizeBuffers1", count, width, height, format, flags},
    queues
  );
}

bool install_hooks(const DxgiMethods& methods) {
  context.diagnostics.log(LogLevel::debug, "DX12 hook probe ready; hook installation begin");
  using Flags = safetyhook::InlineHook::Flags;
  auto p = safetyhook::InlineHook::create(methods.present, on_present, Flags::StartDisabled);
  auto r = safetyhook::InlineHook::create(methods.resize, on_resize, Flags::StartDisabled);
  auto r1 = safetyhook::InlineHook::create(methods.resize1, on_resize1, Flags::StartDisabled);
  auto c = safetyhook::InlineHook::create(methods.create, on_create, Flags::StartDisabled);
  auto h =
    safetyhook::InlineHook::create(methods.create_hwnd, on_create_hwnd, Flags::StartDisabled);
  auto k =
    safetyhook::InlineHook::create(methods.create_core, on_create_core, Flags::StartDisabled);
  auto m = safetyhook::InlineHook::create(
    methods.create_composition,
    on_create_composition,
    Flags::StartDisabled
  );
  auto e = safetyhook::InlineHook::create(methods.execute, on_execute, Flags::StartDisabled);
  auto color = safetyhook::InlineHook::create(methods.color, on_color, Flags::StartDisabled);

  if (p && r && r1 && c && h && k && m && e && color) {
    context.hooks.present_hook = std::move(*p);
    context.hooks.resize_hook = std::move(*r);
    context.hooks.resize1_hook = std::move(*r1);
    context.hooks.create_hook = std::move(*c);
    context.hooks.create_hwnd_hook = std::move(*h);
    context.hooks.create_core_hook = std::move(*k);
    context.hooks.create_composition_hook = std::move(*m);
    context.hooks.execute_hook = std::move(*e);
    context.hooks.color_hook = std::move(*color);
    context.activation_attempted = true;
    context.stop_result.module_must_remain_loaded = true;
    return context.hooks.create_hook.enable() &&
      context.hooks.create_hwnd_hook.enable() &&
      context.hooks.create_core_hook.enable() &&
      context.hooks.create_composition_hook.enable() &&
      context.hooks.execute_hook.enable() &&
      context.hooks.resize_hook.enable() &&
      context.hooks.resize1_hook.enable() &&
      context.hooks.color_hook.enable() &&
      context.hooks.present_hook.enable();
  } else {
    context.diagnostics.log(LogLevel::error, "DX12 inline hook creation failed");
    for (auto* hook : {&p, &r, &r1, &c, &h, &k, &m, &e, &color}) {
      if (*hook) {
        (*hook)->reset();
      }
    }
  }
  return false;
}
} // namespace

bool start_hooks(Dx12Renderer& target, HudSnapshot hud_snapshot, LogCallback log_callback) {
  if (context.activation_attempted || context.renderer) {
    return false;
  }
  context.renderer = &target;
  context.snapshot = hud_snapshot;
  context.diagnostics.set_logger(log_callback);
  context.stopping = false;
  bool ok = false;
  {
    DxgiProbe probe;
    if (probe.initialize(context.probe_device, context.mutex, context.diagnostics)) {
      ok = install_hooks(probe.methods());
    } else if (!probe.registered()) {
      return false;
    }
  }
  if (!ok) {
    context.diagnostics.log(LogLevel::error, "DX12 graphics hook startup failed");
    stop_hooks();
  }
  return ok;
}

HooksStopResult stop_hooks() noexcept {
  context.stopping = true;

  if (!context.renderer) {
    return context.stop_result;
  }

  bool disabled = true;

  for (auto* hook : {
         &context.hooks.create_hook,
         &context.hooks.create_hwnd_hook,
         &context.hooks.create_core_hook,
         &context.hooks.create_composition_hook,
         &context.hooks.execute_hook,
         &context.hooks.resize_hook,
         &context.hooks.resize1_hook,
         &context.hooks.color_hook,
         &context.hooks.present_hook,
       }) {
    try {
      if (*hook && !hook->disable()) {
        disabled = false;
      }
    } catch (...) {
      disabled = false;
    }
  }

  context.stop_result.hooks_disabled = disabled;
  context.stop_result.module_must_remain_loaded = context.activation_attempted || !disabled;
  const auto deadline = GetTickCount64() + 1000;

  while (context.callbacks.load() && GetTickCount64() < deadline) {
    Sleep(1);
  }

  context.stop_result.callbacks_drained = context.callbacks.load() == 0;

  if (!disabled || !context.stop_result.callbacks_drained) {
    context.stop_result.gpu_resources_released = false;

    return context.stop_result;
  }

  try {
    std::lock_guard lock(context.mutex);
    context.stop_result.gpu_resources_released =
      context.renderer->shutdown() == ReleaseResult::released;
    if (context.probe_device) {
      context.stop_result.gpu_resources_released = false;
      context.stop_result.module_must_remain_loaded = true;
      context.diagnostics.log(LogLevel::debug, "DX12 probe device retained through process exit");
    }

    // No further own work is admitted after stopping, including late hook entries.
    context.renderer->set_logger(nullptr);
    context.swapchains.clear();
    context.renderer = nullptr;
    context.snapshot = nullptr;
    context.diagnostics.set_logger(nullptr);

    if (!context.activation_attempted) {
      context.hooks.present_hook.reset();
      context.hooks.resize_hook.reset();
      context.hooks.resize1_hook.reset();
      context.hooks.create_hook.reset();
      context.hooks.create_hwnd_hook.reset();
      context.hooks.create_core_hook.reset();
      context.hooks.create_composition_hook.reset();
      context.hooks.execute_hook.reset();
      context.hooks.color_hook.reset();
    }
  } catch (...) {
    context.stop_result.gpu_resources_released = false;
    context.stop_result.module_must_remain_loaded = true;
  }

  return context.stop_result;
}

} // namespace phi::render
