#include "render/dx12_renderer.hpp"
#include <imgui.h>

namespace phi::render {
void draw_hud(const HudState& hud, D3D12_GPU_DESCRIPTOR_HANDLE texture) {
  if (!hud.visible)
    return;
  auto* draw = ImGui::GetForegroundDrawList();
  constexpr float kIconSize = 56.0f;
  const float x = static_cast<float>(hud.x), s = hud.scale;
  const int configured_y = hud.y;
  const float y = configured_y < 0
                      ? ImGui::GetIO().DisplaySize.y + static_cast<float>(configured_y) - kIconSize * s
                      : static_cast<float>(configured_y);
  draw->AddImage(ImTextureRef(static_cast<ImTextureID>(texture.ptr)), {x, y},
                 {x + kIconSize * s, y + kIconSize * s});
}

} // namespace phi::render
