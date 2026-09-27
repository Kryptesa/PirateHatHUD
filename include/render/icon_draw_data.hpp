#pragma once

#include <imgui.h>

namespace phi::render {
inline bool icon_only(const ImDrawData& data, ImTextureID icon) {
  for (const auto* commands : data.CmdLists) {
    for (const auto& command : commands->CmdBuffer) {
      if (
        command.UserCallback ||
        (command.ElemCount && (command.TexRef._TexData || command.GetTexID() != icon))
      ) {
        return false;
      }
    }
  }

  return true;
}

// ImGui's DX12 backend explicitly supports disabling texture updates by clearing
// this list. Only use after icon_only(): uploads otherwise include infinite waits.
struct ExternalTextureDraw {
  ImDrawData& data;
  decltype(ImDrawData::Textures) textures;

  explicit ExternalTextureDraw(ImDrawData& target)
    : data(target),
      textures(target.Textures) {
    data.Textures = nullptr;
  }

  ~ExternalTextureDraw() {
    data.Textures = textures;
  }
};
} // namespace phi::render
