#include "render/dxgi_hooks.hpp"
#include <atomic>
#include <cstring>

#define CHECK(c)                                                                                   \
  do {                                                                                             \
    if (!(c)) {                                                                                    \
      return __LINE__;                                                                             \
    }                                                                                              \
  } while (false)

namespace {
std::atomic<unsigned> present_entered{}, present1_entered{}, present1_completed{};

void capture_log(phi::LogLevel, const char* message) {
  if (std::strstr(message, "first non-test Present callback: entered")) {
    ++present_entered;
  }
  if (std::strstr(message, "first non-test Present1 callback: entered")) {
    ++present1_entered;
  }
  if (std::strstr(message, "first non-test Present1 callback: completed")) {
    ++present1_completed;
  }
}

phi::HudState snapshot() {
  return {};
}

struct Session {
  phi::render::Dx12Renderer renderer;
  HWND window{};
  bool hooks_started{};

  ~Session() {
    if (hooks_started) {
      phi::render::stop_hooks();
    }
    renderer.shutdown();
    if (window) {
      DestroyWindow(window);
    }
    UnregisterClassW(L"PirateHatHUDPresentTest", GetModuleHandleW(nullptr));
  }
};
} // namespace

int main() {
  using namespace phi::render;
  Session session;
  WNDCLASSW cls{};
  cls.lpfnWndProc = DefWindowProcW;
  cls.hInstance = GetModuleHandleW(nullptr);
  cls.lpszClassName = L"PirateHatHUDPresentTest";
  CHECK(RegisterClassW(&cls));
  session.window = CreateWindowW(
    cls.lpszClassName,
    L"",
    WS_OVERLAPPEDWINDOW,
    0,
    0,
    128,
    128,
    nullptr,
    nullptr,
    cls.hInstance,
    nullptr
  );
  CHECK(session.window);

  ComPtr<IDXGIFactory4> factory;
  ComPtr<IDXGIAdapter> adapter;
  ComPtr<ID3D12Device> device;
  ComPtr<ID3D12CommandQueue> queue;
  ComPtr<IDXGISwapChain1> swap;
  CHECK(SUCCEEDED(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory))));
  CHECK(SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter))));
  CHECK(SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device))));
  D3D12_COMMAND_QUEUE_DESC queue_desc{};
  CHECK(SUCCEEDED(device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&queue))));
  DXGI_SWAP_CHAIN_DESC1 desc{};
  desc.BufferCount = 2;
  desc.Width = 64;
  desc.Height = 64;
  desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  desc.SampleDesc.Count = 1;
  desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
  CHECK(SUCCEEDED(
    factory->CreateSwapChainForHwnd(queue.Get(), session.window, &desc, nullptr, nullptr, &swap)
  ));

  DXGI_PRESENT_PARAMETERS parameters{};
  const auto baseline_test = swap->Present1(0, DXGI_PRESENT_TEST, &parameters);
  const auto baseline_full = swap->Present1(0, 0, &parameters);
  const auto baseline_invalid = swap->Present1(5, 0, &parameters);
  CHECK(FAILED(baseline_invalid));
  RECT dirty{0, 0, 64, 64};
  DXGI_PRESENT_PARAMETERS partial{1, &dirty, nullptr, nullptr};
  const auto baseline_partial = swap->Present1(0, 0, &partial);

  auto& renderer = session.renderer;
  renderer.set_image({1, 1, {255, 255, 255, 255}});
  CHECK(renderer.prepare_shaders());
  CHECK(renderer.initialize(swap.Get(), queue.Get()));
  // The hidden window intentionally does not qualify for HUD target selection.
  // Real DXGI calls still exercise both hook ABIs, forwarding and callback draining.
  session.hooks_started = start_hooks(renderer, snapshot, capture_log);
  CHECK(session.hooks_started);
  CHECK(swap->Present1(0, DXGI_PRESENT_TEST, &parameters) == baseline_test);
  CHECK(present1_entered == 0);
  CHECK(swap->Present1(0, 0, &parameters) == baseline_full);
  CHECK(present1_entered == 1 && present1_completed == 1);
  CHECK(present_entered == 0); // A nested native Present must not run HUD work twice.
  CHECK(swap->Present1(0, 0, &parameters) == baseline_full);
  CHECK(present1_entered == 1 && present1_completed == 1);
  CHECK(swap->Present1(5, 0, &parameters) == baseline_invalid);
  CHECK(swap->Present1(0, 0, &partial) == baseline_partial);
  CHECK(partial.DirtyRectsCount == 1 && partial.pDirtyRects == &dirty);
  CHECK(dirty.left == 0 && dirty.top == 0 && dirty.right == 64 && dirty.bottom == 64);
  CHECK(parameters.DirtyRectsCount == 0 && !parameters.pDirtyRects);
  CHECK(swap->Present(0, 0) == baseline_full);
  CHECK(present_entered == 1);

  const auto stopped = stop_hooks();
  CHECK(stopped.hooks_disabled && stopped.callbacks_drained);
  CHECK(stopped.module_must_remain_loaded);
  CHECK(swap->Present1(0, 0, &parameters) == baseline_full);
  CHECK(present1_entered == 1 && present1_completed == 1);
  return 0;
}
