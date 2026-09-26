#include "render/dx12_renderer.hpp"
#include <imgui.h>
#include <backends/imgui_impl_dx12.h>
#include <backends/imgui_impl_win32.h>

namespace phi::render {
void Dx12Renderer::descriptor_alloc(ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* cpu,
                                    D3D12_GPU_DESCRIPTOR_HANDLE* gpu) {
  auto& self = *static_cast<Dx12Renderer*>(info->UserData);
  const auto step =
      self.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  for (size_t i = 0; i < self.descriptors.size(); ++i) {
    if (self.descriptors[i])
      continue;
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
void Dx12Renderer::descriptor_free(ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE cpu,
                                   D3D12_GPU_DESCRIPTOR_HANDLE) {
  auto& self = *static_cast<Dx12Renderer*>(info->UserData);
  const auto base = self.srv_heap->GetCPUDescriptorHandleForHeapStart().ptr;
  const auto step =
      self.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  if (cpu.ptr < base || !step)
    return;
  const auto offset = cpu.ptr - base;
  if (offset % step == 0 && offset / step < self.descriptors.size())
    self.descriptors[offset / step] = false;
}
bool Dx12Renderer::wait_frame(const Frame& frame) {
  if (!frame.fence_value)
    return true;
  bool warned = false;
  while (fence->GetCompletedValue() < frame.fence_value) {
    if (device->GetDeviceRemovedReason() != S_OK)
      return true;
    const auto event_result = fence->SetEventOnCompletion(frame.fence_value, fence_event);
    const auto result =
        SUCCEEDED(event_result) ? WaitForSingleObject(fence_event, 5000) : WAIT_FAILED;
    if (result == WAIT_OBJECT_0)
      continue;
    if (!warned) {
      log("DX12 fence wait delayed; retaining GPU resources until safe");
      warned = true;
    }
    Sleep(10);
  }
  return true;
}
bool Dx12Renderer::wait_all() {
  if (untracked_submission) {
    log("DX12 queue signal failed; waiting for device removal before resource cleanup");
    while (device && device->GetDeviceRemovedReason() == S_OK)
      Sleep(50);
  }
  for (const auto& frame : frames)
    if (!wait_frame(frame))
      return false;
  return true;
}
void Dx12Renderer::release_buffers() {
  for (auto& frame : frames)
    frame.buffer.Reset();
  frames.clear();
  rtv_heap.Reset();
  list.Reset();
}
bool Dx12Renderer::create_buffers(IDXGISwapChain* target_swap) {
  DXGI_SWAP_CHAIN_DESC desc{};
  if (FAILED(target_swap->GetDesc(&desc)) || !desc.BufferCount || desc.BufferCount > 16)
    return false;
  D3D12_DESCRIPTOR_HEAP_DESC heap{};
  heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  heap.NumDescriptors = desc.BufferCount;
  if (FAILED(device->CreateDescriptorHeap(&heap, IID_PPV_ARGS(&rtv_heap))))
    return false;
  const UINT step = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  auto handle = rtv_heap->GetCPUDescriptorHandleForHeapStart();
  frames.resize(desc.BufferCount);
  for (auto& frame : frames) {
    const UINT index = static_cast<UINT>(&frame - frames.data());
    if (FAILED(target_swap->GetBuffer(index, IID_PPV_ARGS(&frame.buffer))) ||
        FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                              IID_PPV_ARGS(&frame.allocator))))
      return false;
    frame.rtv = handle;
    device->CreateRenderTargetView(frame.buffer.Get(), nullptr, handle);
    handle.ptr += step;
  }
  if (FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, frames[0].allocator.Get(),
                                       nullptr, IID_PPV_ARGS(&list))))
    return false;
  return SUCCEEDED(list->Close());
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
  return imgui;
}

} // namespace phi::render
