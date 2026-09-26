#pragma once

namespace phi::render {
// PNG is sRGB. HDR UI white is fixed at 203 cd/m2. scRGB unit white is 80 cd/m2;
// HDR10 uses Rec.2020 primaries and ST.2084 with a 10000 cd/m2 reference.
constexpr char kShader[] = R"(
cbuffer Placement : register(b0) { float4 rect; uint mode; };
Texture2D icon : register(t0);
SamplerState linear_sampler : register(s0);
struct Vertex { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };
Vertex vs_main(uint id : SV_VertexID) {
  float2 uv = float2(id & 1, id >> 1);
  Vertex v;
  v.pos = float4(lerp(rect.xy, rect.zw, uv), 0, 1);
  v.uv = uv;
  return v;
}
float3 srgb_linear(float3 c) {
  return float3(c.r <= 0.04045 ? c.r / 12.92 : pow(max((c.r + 0.055) / 1.055, 0), 2.4),
                c.g <= 0.04045 ? c.g / 12.92 : pow(max((c.g + 0.055) / 1.055, 0), 2.4),
                c.b <= 0.04045 ? c.b / 12.92 : pow(max((c.b + 0.055) / 1.055, 0), 2.4));
}
float3 pq(float3 c) {
  float3 p = pow(max(c, 0), 2610.0 / 16384.0);
  return pow((3424.0 / 4096.0 + (2413.0 / 128.0) * p) /
             (1 + (2392.0 / 128.0) * p), 2523.0 / 32.0);
}
float4 ps_main(Vertex v) : SV_TARGET {
  float4 c = icon.Sample(linear_sampler, v.uv);
  float3 rgb = srgb_linear(c.rgb);
  if (mode == 1) {
    rgb *= 203.0 / 80.0;
  } else {
    rgb = mul(float3x3(0.627404, 0.329283, 0.043313,
                      0.069097, 0.919540, 0.011362,
                      0.016391, 0.088013, 0.895595), rgb);
    rgb = pq(rgb * (203.0 / 10000.0));
  }
  return float4(rgb, c.a);
}
)";
} // namespace phi::render
