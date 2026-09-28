#pragma once

#include "render/dxgi_diagnostics.hpp"
#include "render/swapchain_selection.hpp"
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

namespace phi::render {
using Microsoft::WRL::ComPtr;

struct SwapRecord {
  ComPtr<ID3D12CommandQueue> queue;
  DXGI_COLOR_SPACE_TYPE space{};
  bool has_space{};
  HRESULT color_error{S_OK};
};

struct SwapIdentity {
  uint64_t generation{};
  uint64_t creation_observed{};
  uint64_t queue_observed{};
};

struct SwapchainTarget {
  ComPtr<ID3D12CommandQueue> queue;
  DXGI_COLOR_SPACE_TYPE space{};
  explicit operator bool() const {
    return queue != nullptr;
  }
};

// Every operation is called under the hook context's graphics mutex.
// Retains queues only; swapchain identity expires with the DXGI object.
class DxgiSwapchains {
public:
  void created(IDXGISwapChain* swap, ID3D12CommandQueue* queue);
  void observe_queue(ID3D12Device* device, ID3D12CommandQueue* queue);
  SwapchainTarget select(IDXGISwapChain* swap, const DxgiDiagnostics& diagnostics);
  void color_changed(
    IDXGISwapChain3* swap,
    DXGI_COLOR_SPACE_TYPE space,
    HRESULT hr,
    const DxgiDiagnostics& diagnostics
  );
  void resized(IDXGISwapChain* swap, UINT count, IUnknown* const* queues);
  void clear();

private:
  SwapIdentity identity_of(IDXGISwapChain* swap, bool created = false, bool associated = false);
  SwapRecord& record_for(uint64_t identity);
  SwapchainRecords<SwapRecord> records_;
  SwapchainSelection selection_;
  DirectQueueFallback<ComPtr<ID3D12CommandQueue>> fallback_;
  uint64_t next_identity_{};
  bool fallback_logged_{};
};
} // namespace phi::render
