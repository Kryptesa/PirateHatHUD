#include "game/minimap_memory.hpp"
#include "game/address.hpp"
#include <array>
#include <cmath>
#include <cstring>

namespace phi::detail {
// Follow the native canvas binding and its live ancestors, not a sibling status icon.
MinimapState sample_minimap(uintptr_t module, const MemoryReader& read, UiIdentityCache* cache) {
  constexpr char root_type[] = ".?AVUIGamePlayControlRootMiniMap@uiCommonScript@pa@@";
  const auto root = find_ui_root(module, read, root_type, cache);
  auto field = [&](uintptr_t base, uintptr_t offset, auto& value) {
    uintptr_t address = 0;
    return base && add_address(base, offset, address) && read(address, &value, sizeof(value));
  };
  auto named = [&](uintptr_t definition, const char* expected) {
    uintptr_t name = 0;
    if (!field(definition, 0xF8, name) || !name) {
      return false;
    }
    const auto length = std::strlen(expected);
    for (size_t i = 0; i <= length; ++i) {
      char value = 0;
      if (!field(name, i, value) || value != expected[i]) {
        return false;
      }
    }
    return true;
  };

  uintptr_t script = 0, view = 0, body = 0, canvas = 0, definition = 0, backlink = 0;
  if (
    !field(root, 0x118, script) ||
    !field(root, 0x120, view) ||
    !named(view, "MinimapView") ||
    !field(script, 8, body) ||
    !named(body, "MinimapHudBody") ||
    !field(script, 0x3F0, canvas) ||
    !field(canvas, 8, definition) ||
    !named(definition, "MinimapCanvas") ||
    !field(definition, 0xD8, backlink) ||
    backlink != canvas
  ) {
    return MinimapState::unknown;
  }

  std::array<uintptr_t, 24> visited{};
  bool hidden = false, found_body = false;
  for (size_t i = 0; i < visited.size(); ++i) {
    if (!definition) {
      return MinimapState::unknown;
    }
    for (size_t j = 0; j < i; ++j) {
      if (visited[j] == definition) {
        return MinimapState::unknown;
      }
    }
    visited[i] = definition;
    found_body = found_body || definition == body;

    uint8_t flags = 0, clip = 0;
    uintptr_t properties = 0;
    float opacity = 0;
    if (
      !field(definition, 0xB0, flags) ||
      !field(definition, 0x97, clip) ||
      !field(definition, 0xC0, properties) ||
      !field(properties, 0x40, opacity) ||
      !std::isfinite(opacity) ||
      opacity < 0 ||
      opacity > 1
    ) {
      return MinimapState::unknown;
    }
    // Native draw admission requires these bits and positive computed opacity.
    hidden = hidden || (flags & 0x60) != 0x40 || !(flags & 0x80) || (clip & 0x20) || opacity == 0;
    if (definition == view) {
      return found_body ? (hidden ? MinimapState::hidden : MinimapState::visible)
                        : MinimapState::unknown;
    }
    if (!field(definition, 0x38, definition)) {
      return MinimapState::unknown;
    }
  }
  return MinimapState::unknown;
}
} // namespace phi::detail
