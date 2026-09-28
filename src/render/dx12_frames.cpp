#include "render/dx12_renderer.hpp"
#include <imgui.h>
#include <backends/imgui_impl_dx12.h>
#include <backends/imgui_impl_win32.h>

namespace phi::render {
void Dx12Renderer::descriptor_alloc(
  ImGui_ImplDX12_InitInfo* info,
  D3D12_CPU_DESCRIPTOR_HANDLE* cpu,
  D3D12_GPU_DESCRIPTOR_HANDLE* gpu
) {
  auto& self = *static_cast<Dx12Renderer*>(info->UserData);
  const auto step =
    self.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

  for (size_t i = 0; i < self.descriptors.size(); ++i) {
    if (self.descriptors[i]) {
      continue;
    }

    self.descriptors[i] = true;
    *cpu = info->SrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    *gpu = info->SrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
    cpu->ptr += i * step;
    gpu->ptr += i * step;
    return;
  }

  *cpu = {};
  *gpu = {};
}

void Dx12Renderer::descriptor_free(
  ImGui_ImplDX12_InitInfo* info,
  D3D12_CPU_DESCRIPTOR_HANDLE cpu,
  D3D12_GPU_DESCRIPTOR_HANDLE
) {
  auto& self = *static_cast<Dx12Renderer*>(info->UserData);
  const auto base = self.srv_heap->GetCPUDescriptorHandleForHeapStart().ptr;
  const auto step =
    self.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

  if (cpu.ptr < base || !step) {
    return;
  }

  const auto offset = cpu.ptr - base;

  if (offset % step == 0 && offset / step < self.descriptors.size()) {
    self.descriptors[offset / step] = false;
  }
}

WaitResult Dx12Renderer::wait_fence(UINT64 value, ULONGLONG deadline) {
  struct Adapter {
    ID3D12Device* device;
    ID3D12Fence* fence;
    HANDLE event;
    const Dx12Renderer* renderer;

    std::uint64_t completed() {
      return fence->GetCompletedValue();
    }

    bool device_lost() {
      return device->GetDeviceRemovedReason() != S_OK;
    }

    std::uint64_t now() {
      return GetTickCount64();
    }

    bool wait(std::uint64_t value, std::uint64_t remaining) {
      if (!renderer->check_result(
            fence->SetEventOnCompletion(value, event),
            "Fence SetEventOnCompletion"
          )) {
        return false;
      }

      const auto result = WaitForSingleObject(event, static_cast<DWORD>(remaining));

      if (result == WAIT_FAILED) {
        renderer->check_result(HRESULT_FROM_WIN32(GetLastError()), "WaitForSingleObject");
      }
      return result == WAIT_OBJECT_0 || result == WAIT_TIMEOUT;
    }
  } adapter{device.Get(), fence.Get(), fence_event, this};

  return wait_for_fence(adapter, value, deadline);
}

WaitResult Dx12Renderer::wait_all() {
  if (!device || device->GetDeviceRemovedReason() != S_OK) {
    return WaitResult::device_lost;
  }

  if (untracked_submission) {
    return WaitResult::failed;
  }

  const auto deadline = GetTickCount64() + 1000;

  for (const auto& frame : frames) {
    const auto result = wait_fence(frame.fence_value, deadline);

    if (result != WaitResult::completed) {
      return result;
    }
  }

  return WaitResult::completed;
}

void Dx12Renderer::release_buffers() {
  for (auto& frame : frames) {
    frame.buffer.Reset();
  }

  frames.clear();
  rtv_heap.Reset();
  backend_frames.reset(0);
}

bool Dx12Renderer::create_buffers(IDXGISwapChain* target_swap) {
  DXGI_SWAP_CHAIN_DESC desc{};

  if (!check_result(target_swap->GetDesc(&desc), "GetDesc for frame resources")) {
    return false;
  }
  if (!desc.BufferCount || desc.BufferCount > 16) {
    log(LogLevel::error, "DX12 unsupported backbuffer count");
    return false;
  }

  D3D12_DESCRIPTOR_HEAP_DESC heap{};
  heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  heap.NumDescriptors = desc.BufferCount;

  if (!check_result(
        device->CreateDescriptorHeap(&heap, IID_PPV_ARGS(&rtv_heap)),
        "CreateDescriptorHeap(RTV)"
      )) {
    return false;
  }

  const UINT step = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  auto handle = rtv_heap->GetCPUDescriptorHandleForHeapStart();
  frames.resize(desc.BufferCount);

  for (auto& frame : frames) {
    const UINT index = static_cast<UINT>(&frame - frames.data());

    if (
      !check_result(target_swap->GetBuffer(index, IID_PPV_ARGS(&frame.buffer)), "GetBuffer") ||
      !check_result(
        device
          ->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&frame.allocator)),
        "CreateCommandAllocator"
      )
    ) {
      return false;
    }

    frame.rtv = handle;
    device->CreateRenderTargetView(frame.buffer.Get(), nullptr, handle);
    handle.ptr += step;

    if (
      !check_result(
        device->CreateCommandList(
          0,
          D3D12_COMMAND_LIST_TYPE_DIRECT,
          frame.allocator.Get(),
          nullptr,
          IID_PPV_ARGS(&frame.list)
        ),
        "CreateCommandList"
      ) ||
      !check_result(frame.list->Close(), "CommandList Close(new)")
    ) {
      return false;
    }
  }

  return true;
}

bool Dx12Renderer::initialize_backend(const DXGI_SWAP_CHAIN_DESC& desc) {
  ImGui_ImplDX12_InitInfo init{};
  init.UserData = this;
  init.Device = device.Get();
  init.CommandQueue = queue.Get();
  init.NumFramesInFlight = static_cast<int>(frames.size());
  init.RTVFormat = desc.BufferDesc.Format;
  init.SrvDescriptorHeap = srv_heap.Get();
  init.SrvDescriptorAllocFn = descriptor_alloc;
  init.SrvDescriptorFreeFn = descriptor_free;
  imgui = ImGui_ImplDX12_Init(&init);

  if (imgui) {
    backend_frames.reset(frames.size());
  }

  if (!imgui) {
    log(LogLevel::error, "DX12 ImGui backend initialization failed");
    return false;
  }
  return initialize_hdr(desc.BufferDesc.Format);
}

} // namespace phi::render
