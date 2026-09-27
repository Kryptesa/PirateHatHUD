#pragma once

#include "game/observer_state.hpp"
#include "game/ui_memory.hpp"

namespace phi::detail {
uintptr_t
find_menu_root(uintptr_t module, const MemoryReader& read, UiIdentityCache* cache = nullptr);
MenuState sample_menu(uintptr_t root, uintptr_t state_offset, const MemoryReader& read);
} // namespace phi::detail
