#include "render/dx12_renderer.hpp"
#include <imgui.h>

namespace phi::render {
void draw_hud(const HudState& hud, D3D12_GPU_DESCRIPTOR_HANDLE texture) {
  if (!hud.visible)
    return;
  auto* draw = ImGui::GetForegroundDrawList();
  const float x = static_cast<float>(hud.x), s = hud.scale;
  const int configured_y = hud.y;
  const float y = configured_y < 0
                      ? ImGui::GetIO().DisplaySize.y + static_cast<float>(configured_y) - 44.0f * s
                      : static_cast<float>(configured_y);
  draw->AddImage(ImTextureRef(static_cast<ImTextureID>(texture.ptr)), {x, y},
                 {x + 44 * s, y + 44 * s});
}

} // namespace phi::render
