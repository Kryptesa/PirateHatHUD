#pragma once

#include "render/dxgi_diagnostics.hpp"
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <mutex>

namespace phi::render {
struct DxgiMethods {
  void* present{};
  void* resize{};
  void* resize1{};
  void* create{};
  void* create_hwnd{};
  void* create_core{};
  void* create_composition{};
  void* execute{};
  void* color{};
};

class DxgiProbe {
public:
  DxgiProbe() = default;
  DxgiProbe(const DxgiProbe&) = delete;
  DxgiProbe& operator=(const DxgiProbe&) = delete;
  ~DxgiProbe();
  bool initialize(
    Microsoft::WRL::ComPtr<ID3D12Device>& retained_device,
    std::mutex& graphics_mutex,
    const DxgiDiagnostics& diagnostics
  );
  bool registered() const {
    return registered_;
  }
  DxgiMethods methods() const;

private:
  bool registered_{};
  HWND window_{};
  HINSTANCE instance_{};
  Microsoft::WRL::ComPtr<ID3D12Device> device_;
  Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue_;
  Microsoft::WRL::ComPtr<IDXGIFactory2> factory_;
  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_;
  Microsoft::WRL::ComPtr<IDXGISwapChain3> swap3_;
};
} // namespace phi::render
