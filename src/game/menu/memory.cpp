#include "game/menu/memory.hpp"

namespace phi::detail {
uintptr_t find_menu_root(uintptr_t module, const MemoryReader& read, UiIdentityCache* cache) {
  constexpr char name[] = ".?AVUIGamePlayControl_Root_MainMenu@uiCommonScript@pa@@";

  return find_ui_root(module, read, name, cache);
}

MenuState sample_menu(uintptr_t root, uintptr_t state_offset, const MemoryReader& read) {
  uint8_t value = 0;

  if (!state_offset || !read_field(read, root, state_offset, value)) {
    return MenuState::unknown;
  }

  return value == 0 ? MenuState::closed : value == 1 ? MenuState::open : MenuState::unknown;
}
} // namespace phi::detail
