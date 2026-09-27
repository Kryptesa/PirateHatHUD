#pragma once

#include "game/observer_state.hpp"
#include "game/shared/memory_reader.hpp"

#include <cstdint>

namespace phi::detail {

// Samples the post-instruction counter on the owner thread. Unknown means the
// captured base cannot currently be read, not that the counter is inactive.
TreasureState sample_treasure_state(std::uintptr_t base, const MemoryReader& read);

} // namespace phi::detail
