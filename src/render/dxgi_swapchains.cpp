#include "render/dxgi_swapchains.hpp"
#include "render/dx12_renderer.hpp"
#include <cstdio>
#include <utility>

namespace phi::render {
namespace {
// Private data expires with the swapchain, so a recycled COM address cannot inherit
// a previous queue or color space. No swapchain reference is retained by this registry.
constexpr GUID kSwapIdentity =
  {0x47a58295, 0x87b7, 0x458f, {0xb1, 0x7b, 0x53, 0xa8, 0x91, 0x75, 0x7d, 0x2c}};

} // namespace

SwapIdentity DxgiSwapchains::identity_of(IDXGISwapChain* swap, bool created, bool associated) {
  SwapIdentity identity{};
  UINT size = sizeof(identity);
  bool changed = created || associated;

  if (
    FAILED(swap->GetPrivateData(kSwapIdentity, &size, &identity)) ||
    size != sizeof(identity) ||
    !identity.generation
  ) {
    identity.generation = ++next_identity_;
    changed = true;
  }

  if (created) {
    identity.creation_observed = 1;
  }

  if (created || associated) {
    identity.queue_observed = 1;
  }

  if (changed && FAILED(swap->SetPrivateData(kSwapIdentity, sizeof(identity), &identity))) {
    return {};
  }

  return identity;
}

SwapRecord& DxgiSwapchains::record_for(uint64_t identity) {
  return records_.remember(identity, selection_.identity());
}

void DxgiSwapchains::created(IDXGISwapChain* swap, ID3D12CommandQueue* queue) {
  if (const auto identity = identity_of(swap, true); identity.generation) {
    record_for(identity.generation).queue = queue;
  }
}

void DxgiSwapchains::observe_queue(ID3D12Device* device, ID3D12CommandQueue* queue) {
  fallback_.observe(reinterpret_cast<uintptr_t>(device), ComPtr<ID3D12CommandQueue>(queue));
}

SwapchainTarget DxgiSwapchains::select(IDXGISwapChain* swap, const DxgiDiagnostics& diagnostics) {
  DXGI_SWAP_CHAIN_DESC desc{};
  DWORD process{};
  const bool described = SUCCEEDED(swap->GetDesc(&desc));

  if (described && desc.OutputWindow) {
    GetWindowThreadProcessId(desc.OutputWindow, &process);
  }

  const auto metadata = identity_of(swap);
  const auto identity = metadata.generation;
  const bool eligible = described &&
    process == GetCurrentProcessId() &&
    IsWindowVisible(desc.OutputWindow) &&
    GetAncestor(desc.OutputWindow, GA_ROOT) == desc.OutputWindow &&
    !GetWindow(desc.OutputWindow, GW_OWNER);

  if (!eligible || !identity) {
    return {};
  }

  ComPtr<ID3D12Device> device;

  if (FAILED(swap->GetDevice(IID_PPV_ARGS(&device)))) {
    return {};
  }

  ComPtr<ID3D12CommandQueue> queue;

  if (const auto* record = records_.find(identity)) {
    queue = record->queue;
  }

  if (!queue) {
    // Lost associations/color spaces must not silently fall back to a queue or
    // SDR interpretation. Captured chains always require their explicit queue.
    if (metadata.queue_observed || records_.evicted(identity)) {
      return {};
    }

    queue = fallback_.find(reinterpret_cast<uintptr_t>(device.Get()));

    if (queue && !fallback_logged_) {
      diagnostics.log(LogLevel::info, "DX12 using single observed direct queue for this device");
      fallback_logged_ = true;
    }
  }

  if (selection_.window() && !IsWindow(reinterpret_cast<HWND>(selection_.window()))) {
    selection_ = {};
  }

  // The first chain must present to the foreground game window. Once selected,
  // keep that window while unfocused; auxiliary windows cannot steal selection.
  const bool selectable =
    selection_.window() || GetAncestor(GetForegroundWindow(), GA_ROOT) == desc.OutputWindow;

  const auto previous_selection = selection_.identity();
  if (!selection_.present(
        identity,
        reinterpret_cast<uintptr_t>(desc.OutputWindow),
        selectable,
        metadata.creation_observed != 0,
        same_device(queue.Get(), device.Get())
      )) {
    return {};
  }
  if (previous_selection != selection_.identity()) {
    char message[128]{};
    std::snprintf(
      message,
      sizeof(message),
      "DX12 HUD swapchain selected: generation=%llu",
      static_cast<unsigned long long>(identity)
    );
    diagnostics.log(LogLevel::debug, message);
  }

  // Keep an active identity record even for a pre-existing fallback chain;

  // auxiliary churn must not classify it as evicted. Do not turn an inferred
  // queue into an explicit association: later ambiguity must still suppress it.
  record_for(identity);

  // FP16 defaults to scRGB. A pre-existing 10-bit chain is ambiguous;

  // retain SDR until a successful SetColorSpace1 is observed.
  auto space = desc.BufferDesc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT
    ? DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709
    : DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;

  if (const auto* record = records_.find(identity); record && record->has_space) {
    space = record->space;
  }

  return {std::move(queue), space};
}

void DxgiSwapchains::color_changed(
  IDXGISwapChain3* swap,
  DXGI_COLOR_SPACE_TYPE space,
  HRESULT hr,
  const DxgiDiagnostics& diagnostics
) {
  if (const auto identity = identity_of(swap); identity.generation) {
    auto& record = record_for(identity.generation);
    if (
      (SUCCEEDED(hr) && (!record.has_space || record.space != space)) ||
      (FAILED(hr) && record.color_error != hr)
    ) {
      char message[160]{};
      std::snprintf(
        message,
        sizeof(message),
        "DX12 SetColorSpace1: generation=%llu; color space=%u; HRESULT=0x%08lX",
        static_cast<unsigned long long>(identity.generation),
        static_cast<unsigned>(space),
        static_cast<unsigned long>(hr)
      );
      diagnostics.log(SUCCEEDED(hr) ? LogLevel::debug : LogLevel::error, message);
    }
    record.color_error = FAILED(hr) ? hr : S_OK;
    if (SUCCEEDED(hr)) {
      record.space = space;
      record.has_space = true;
    }
  }
}

void DxgiSwapchains::resized(IDXGISwapChain* swap, UINT count, IUnknown* const* queues) {
  ComPtr<ID3D12CommandQueue> queue;
  bool coherent = true;

  for (UINT index = 0; index < count; ++index) {
    ComPtr<ID3D12CommandQueue> candidate;

    if (
      !queues[index] ||
      FAILED(queues[index]->QueryInterface(IID_PPV_ARGS(&candidate))) ||
      candidate->GetDesc().Type != D3D12_COMMAND_LIST_TYPE_DIRECT ||
      (queue && queue != candidate)
    ) {
      coherent = false;
      break;
    }

    queue = candidate;
  }

  if (const auto identity = identity_of(swap, false, true); identity.generation) {
    if (!coherent) {
      queue.Reset();
    }

    record_for(identity.generation).queue = queue;
  }
}

void DxgiSwapchains::clear() {
  records_ = {};
  selection_ = {};
  fallback_ = {};
  fallback_logged_ = false;
}
} // namespace phi::render
