#pragma once
#include "game/ui_memory.hpp"
#include <utility>

namespace phi::detail {
// Root identified by controller type; child[0] and leaf[3] remain historical slots.
template <typename Reader>
MinimapState sample_minimap(uintptr_t module, Reader&& read, UiIdentityCache* cache = nullptr) {
  constexpr char name[] = ".?AVUIGamePlayControlRootStatusGauge@uiCommonScript@pa@@";
  uintptr_t current = find_ui_root(module, read, name, cache);
  uintptr_t address = 0;
  for (auto offset : {0x48u, 0u, 0x290u, 0x18u}) {
    if (!current || !add_address(current, offset, address) ||
        !read(address, &current, sizeof(current))) {
      return MinimapState::unknown;
    }
  }
  uint8_t value = 0;
  if (!current || !add_address(current, 0xBE, address) || !read(address, &value, sizeof(value))) {
    return MinimapState::unknown;
  }
  return value == 0   ? MinimapState::hidden
         : value == 1 ? MinimapState::visible
                      : MinimapState::unknown;
}
} // namespace phi::detail
