#include "render/dx12_renderer.hpp"
#include <algorithm>
#include <string>
#include <vector>

#define CHECK(c)                                                                                   \
  do {                                                                                             \
    if (!(c)) {                                                                                    \
      return __LINE__;                                                                             \
    }                                                                                              \
  } while (false)

namespace {
std::vector<std::string> messages;

void capture_log(phi::LogLevel, const char* message) {
  messages.emplace_back(message);
}

auto log_count(const char* fragment) {
  return std::count_if(messages.begin(), messages.end(), [fragment](const auto& message) {
    return message.find(fragment) != std::string::npos;
  });
}

bool logged(const char* fragment) {
  return std::any_of(messages.begin(), messages.end(), [fragment](const auto& message) {
    return message.find(fragment) != std::string::npos;
  });
}

struct Session {
  phi::render::Dx12Renderer renderer;
  HWND window{};

  ~Session() {
    renderer.shutdown();
    if (window) {
      DestroyWindow(window);
    }
    UnregisterClassW(L"PirateHatHUDResizeTest", GetModuleHandleW(nullptr));
  }
};
} // namespace

int main() {
  using namespace phi::render;
  Session session;
  WNDCLASSW cls{};
  cls.lpfnWndProc = DefWindowProcW;
  cls.hInstance = GetModuleHandleW(nullptr);
  cls.lpszClassName = L"PirateHatHUDResizeTest";
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
  desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
  CHECK(SUCCEEDED(
    factory->CreateSwapChainForHwnd(queue.Get(), session.window, &desc, nullptr, nullptr, &swap)
  ));

  auto& renderer = session.renderer;
  renderer.set_logger(capture_log);
  renderer.set_image({1, 1, {255, 255, 255, 255}});
  CHECK(renderer.prepare_shaders());
  CHECK(renderer.initialize(swap.Get(), queue.Get()));
  // The game device here is WARP: diagnostics must describe it even if the
  // machine's default adapter is a physical GPU from a different vendor.
  CHECK(logged("DX12 GPU:"));
  CHECK(logged("vendor=0x1414;"));
  CHECK(logged("software=yes"));
  CHECK(logged("DX12 GPU driver UMD version"));
  CHECK(logged("DX12 swapchain: 64x64; format=28; buffers=2;"));
  CHECK(renderer.ready());
  CHECK(renderer.before_resize(swap.Get()));
  auto result = swap->ResizeBuffers(3, 96, 96, DXGI_FORMAT_UNKNOWN, 0);
  CHECK(SUCCEEDED(result));
  renderer.after_resize(swap.Get(), result);
  CHECK(renderer.state() == RendererState::resizing);

  // Neither a test presentation, occlusion nor an error confirms a usable chain.
  renderer.after_present(swap.Get(), S_OK, DXGI_PRESENT_TEST);
  renderer.after_present(swap.Get(), DXGI_STATUS_OCCLUDED, 0);
  renderer.after_present(swap.Get(), E_FAIL, 0);
  renderer.after_present(swap.Get(), E_FAIL, 0);
  CHECK(log_count("DX12 Present failed; HRESULT=0x80004005") == 1);
  CHECK(logged("device removed reason=0x00000000; device=available"));
  CHECK(renderer.state() == RendererState::resizing);

  // Model an outer graphics wrapper performing another resize after the hooked
  // implementation returned. No second before_resize: the overlay must still
  // have no references that would make this real DXGI call fail.
  result = swap->ResizeBuffers(2, 128, 128, DXGI_FORMAT_UNKNOWN, 0);
  CHECK(SUCCEEDED(result));
  renderer.after_resize(swap.Get(), result);
  renderer.after_present(nullptr, S_OK, 0);
  CHECK(renderer.state() == RendererState::resizing);
  result = swap->Present(0, 0);
  CHECK(SUCCEEDED(result));
  // The test window stays hidden, so DXGI may report occlusion. Inject a successful
  // presentation notification to exercise recovery without opening a visible window.
  renderer.after_present(swap.Get(), S_OK, 0);
  CHECK(renderer.ready());
  CHECK(logged("DX12 swapchain: 128x128; format=28; buffers=2;"));

  // Exercise submission and another resize with the recovered backend/resources.
  renderer.render(swap.Get(), {}, DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709);
  renderer.render(swap.Get(), {}, DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709);
  CHECK(log_count("DX12 first overlay frame begin") == 1);
  CHECK(log_count("DX12 first overlay submission begin") == 1);
  CHECK(log_count("DX12 render color space=") == 1);
  CHECK(renderer.before_resize(swap.Get()));
  result = swap->ResizeBuffers(3, 64, 64, DXGI_FORMAT_UNKNOWN, 0);
  CHECK(SUCCEEDED(result));
  renderer.after_resize(swap.Get(), result);
  result = swap->Present(0, 0);
  CHECK(SUCCEEDED(result));
  renderer.after_present(swap.Get(), S_OK, 0);
  CHECK(renderer.ready());
  CHECK(logged("DX12 swapchain: 64x64; format=28; buffers=3;"));

  // A failed resize must stay disabled even after a later successful Present.
  CHECK(renderer.before_resize(swap.Get()));
  renderer.after_resize(swap.Get(), E_INVALIDARG);
  CHECK(logged("DX12 ResizeBuffers failed; HRESULT=0x80070057"));
  CHECK(renderer.state() == RendererState::faulted);
  renderer.after_present(swap.Get(), S_OK, 0);
  CHECK(renderer.state() == RendererState::faulted);
  CHECK(renderer.shutdown() == ReleaseResult::released);
  CHECK(SUCCEEDED(swap->ResizeBuffers(2, 64, 64, DXGI_FORMAT_UNKNOWN, 0)));
  return 0;
}
