#pragma once

#include "game/ui_memory.hpp"
#include <utility>

namespace phi::detail {
template <typename Reader>
uintptr_t find_menu_root(uintptr_t module, Reader&& read, UiIdentityCache* cache = nullptr) {
  constexpr char name[] = ".?AVUIGamePlayControl_Root_MainMenu@uiCommonScript@pa@@";

  return find_ui_root(module, std::forward<Reader>(read), name, cache);
}

template <typename Reader>
MenuState sample_menu(uintptr_t root, uintptr_t state_offset, Reader&& read) {
  uintptr_t address = 0;
  uint8_t value = 0;

  if (
    !state_offset ||
    !add_address(root, state_offset, address) ||
    !read(address, &value, sizeof(value))
  ) {
    return MenuState::unknown;
  }

  return value == 0 ? MenuState::closed : value == 1 ? MenuState::open : MenuState::unknown;
}
} // namespace phi::detail
