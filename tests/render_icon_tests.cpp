#include "render/icon_draw_data.hpp"

#define CHECK(c)                                                                                   \
  do {                                                                                             \
    if (!(c))                                                                                      \
      return __LINE__;                                                                             \
  } while (false)

int main() {
  using namespace phi::render;
  ImGui::CreateContext();
  auto& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.DisplaySize = {640, 480};
  io.DeltaTime = 1.0f / 60.0f;
  io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
  constexpr ImTextureID icon = 123;
  ImGui::NewFrame();
  ImGui::GetForegroundDrawList()->AddImage(ImTextureRef(icon), {10, 10}, {54, 54});
  ImGui::Render();
  auto& data = *ImGui::GetDrawData();
  CHECK(data.TotalVtxCount > 0 && icon_only(data, icon));
  auto* textures = data.Textures;
  CHECK(textures != nullptr);
  {
    ExternalTextureDraw external(data);
    CHECK(data.Textures == nullptr);
  }
  CHECK(data.Textures == textures);
  CHECK(!icon_only(data, icon + 1));
  data.CmdLists[0]->CmdBuffer[0].UserCallback = ImDrawCallback_ResetRenderState;
  CHECK(!icon_only(data, icon));
  ImGui::NewFrame();
  ImGui::GetForegroundDrawList()->AddText({10, 10}, IM_COL32_WHITE, "Unexpected text");
  ImGui::Render();
  CHECK(!icon_only(*ImGui::GetDrawData(), icon));
  ImGui::DestroyContext();
  return 0;
}
