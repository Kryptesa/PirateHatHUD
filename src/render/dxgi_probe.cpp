#include "render/dxgi_probe.hpp"

namespace phi::render {
namespace {
constexpr auto kProbeClass = L"PirateHatHUDDx12Probe";
void* method(void* object, size_t index) {
  return (*reinterpret_cast<void***>(object))[index];
}
} // namespace

DxgiProbe::~DxgiProbe() {
  swap_.Reset();
  factory_.Reset();
  queue_.Reset();
  device_.Reset();
  if (window_) {
    DestroyWindow(window_);
  }
  if (registered_) {
    UnregisterClassW(kProbeClass, instance_);
  }
}

bool DxgiProbe::initialize(
  Microsoft::WRL::ComPtr<ID3D12Device>& retained_device,
  std::mutex& graphics_mutex,
  const DxgiDiagnostics& diagnostics
) {
  WNDCLASSW cls{};
  cls.lpfnWndProc = DefWindowProcW;
  cls.hInstance = GetModuleHandleW(nullptr);
  cls.lpszClassName = kProbeClass;
  diagnostics.log(LogLevel::debug, "DX12 hook probe initialization begin");

  if (!RegisterClassW(&cls)) {
    diagnostics.result(HRESULT_FROM_WIN32(GetLastError()), "probe RegisterClass");
    return false;
  }

  registered_ = true;
  instance_ = cls.hInstance;
  window_ = CreateWindowW(
    cls.lpszClassName,
    L"",
    WS_OVERLAPPEDWINDOW,
    0,
    0,
    64,
    64,
    nullptr,
    nullptr,
    cls.hInstance,
    nullptr
  );
  if (!window_) {
    diagnostics.result(HRESULT_FROM_WIN32(GetLastError()), "probe CreateWindow");
    return false;
  }
  if (!diagnostics.result(
        D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device_)),
        "probe D3D12CreateDevice"
      )) {
    return false;
  }
  {
    std::lock_guard lock(graphics_mutex);
    retained_device = device_;
  }
  diagnostics.log(LogLevel::debug, "DX12 probe device retained until game renderer is ready");
  D3D12_COMMAND_QUEUE_DESC q{};
  q.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
  if (
    !diagnostics
      .result(device_->CreateCommandQueue(&q, IID_PPV_ARGS(&queue_)), "probe CreateCommandQueue") ||
    !diagnostics.result(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory_)), "probe CreateDXGIFactory2")
  ) {
    return false;
  }
  DXGI_SWAP_CHAIN_DESC1 desc{};
  desc.BufferCount = 2;
  desc.Width = 64;
  desc.Height = 64;
  desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  desc.SampleDesc.Count = 1;
  desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
  return diagnostics.result(
           factory_->CreateSwapChainForHwnd(queue_.Get(), window_, &desc, nullptr, nullptr, &swap_),
           "probe CreateSwapChainForHwnd"
         ) &&
    diagnostics.result(swap_.As(&swap3_), "probe QueryInterface(SwapChain3)");
}

DxgiMethods DxgiProbe::methods() const {
  return {
    .present = method(swap_.Get(), 8),
    .resize = method(swap_.Get(), 13),
    .resize1 = method(swap3_.Get(), 39),
    .create = method(factory_.Get(), 10),
    .create_hwnd = method(factory_.Get(), 15),
    .create_core = method(factory_.Get(), 16),
    .create_composition = method(factory_.Get(), 24),
    .execute = method(queue_.Get(), 10),
    .color = method(swap3_.Get(), 38)
  };
}
} // namespace phi::render
