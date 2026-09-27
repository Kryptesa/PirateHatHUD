#pragma once

#include "game/observer_state.hpp"
#include "game/ui_memory.hpp"

namespace phi::detail {
MinimapState
sample_minimap(uintptr_t module, const MemoryReader& read, UiIdentityCache* cache = nullptr);
} // namespace phi::detail
