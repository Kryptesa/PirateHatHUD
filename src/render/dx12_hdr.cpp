#include "render/dx12_renderer.hpp"
#include "render/hdr_shader.hpp"
#include <d3dcompiler.h>
#include <cstring>

namespace phi::render {

bool Dx12Renderer::initialize_hdr(DXGI_FORMAT format) {
  D3D12_DESCRIPTOR_RANGE range{};
  range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
  range.NumDescriptors = 1;
  D3D12_ROOT_PARAMETER parameters[2]{};
  parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
  parameters[0].Constants.Num32BitValues = 5;
  parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  parameters[1].DescriptorTable.NumDescriptorRanges = 1;
  parameters[1].DescriptorTable.pDescriptorRanges = &range;
  parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  D3D12_STATIC_SAMPLER_DESC sampler{};
  sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
  sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
  sampler.MaxAnisotropy = 1;
  sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
  sampler.MaxLOD = D3D12_FLOAT32_MAX;
  sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  D3D12_ROOT_SIGNATURE_DESC root{};
  root.NumParameters = 2;
  root.pParameters = parameters;
  root.NumStaticSamplers = 1;
  root.pStaticSamplers = &sampler;
  ComPtr<ID3DBlob> serialized, vs, ps;
  if (FAILED(
          D3D12SerializeRootSignature(&root, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, nullptr)) ||
      FAILED(device->CreateRootSignature(0, serialized->GetBufferPointer(),
                                         serialized->GetBufferSize(), IID_PPV_ARGS(&hdr_root))) ||
      FAILED(D3DCompile(kShader, std::strlen(kShader), nullptr, nullptr, nullptr, "vs_main",
                        "vs_5_0", D3DCOMPILE_ENABLE_STRICTNESS, 0, &vs, nullptr)) ||
      FAILED(D3DCompile(kShader, std::strlen(kShader), nullptr, nullptr, nullptr, "ps_main",
                        "ps_5_0", D3DCOMPILE_ENABLE_STRICTNESS, 0, &ps, nullptr))) {
    log("HDR icon shader initialization failed");
    return false;
  }
  D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline{};
  pipeline.pRootSignature = hdr_root.Get();
  pipeline.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
  pipeline.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
  auto& blend = pipeline.BlendState.RenderTarget[0];
  blend.BlendEnable = TRUE;
  blend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
  blend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
  blend.BlendOp = blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
  blend.SrcBlendAlpha = D3D12_BLEND_ONE;
  blend.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
  blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
  pipeline.SampleMask = UINT_MAX;
  pipeline.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
  pipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
  pipeline.RasterizerState.DepthClipEnable = TRUE;
  pipeline.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
  pipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  pipeline.NumRenderTargets = 1;
  pipeline.RTVFormats[0] = format;
  pipeline.SampleDesc.Count = 1;
  return SUCCEEDED(device->CreateGraphicsPipelineState(&pipeline, IID_PPV_ARGS(&hdr_pipeline)));
}

void Dx12Renderer::draw_hdr(ID3D12GraphicsCommandList* list, const HudState& hud, UINT width,
                            UINT height, DXGI_COLOR_SPACE_TYPE color_space) {
  if (!width || !height) {
    return;
  }
  const float size = 56.0f * hud.scale;
  const float x = static_cast<float>(hud.x);
  const float y = hud.y < 0 ? static_cast<float>(height) + hud.y - size : static_cast<float>(hud.y);
  struct Constants {
    float rect[4];
    UINT mode;
  } constants{{2 * x / width - 1, 1 - 2 * y / height, 2 * (x + size) / width - 1,
               1 - 2 * (y + size) / height},
              color_space == DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709 ? 1u : 2u};
  D3D12_VIEWPORT viewport{0, 0, static_cast<float>(width), static_cast<float>(height), 0, 1};
  D3D12_RECT scissor{0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
  list->RSSetViewports(1, &viewport);
  list->RSSetScissorRects(1, &scissor);
  list->SetPipelineState(hdr_pipeline.Get());
  list->SetGraphicsRootSignature(hdr_root.Get());
  list->SetGraphicsRoot32BitConstants(0, 5, &constants, 0);
  list->SetGraphicsRootDescriptorTable(1, icon_gpu);
  list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
  list->DrawInstanced(4, 1, 0, 0);
}
} // namespace phi::render
