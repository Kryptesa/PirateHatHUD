#pragma once

#include "game/observer_state.hpp"
#include "game/shared/memory_reader.hpp"

#include <cstdint>

namespace phi::detail {

std::uintptr_t treasure_descriptor_stack_offset(std::uint32_t timestamp, std::uint32_t image_size);

// Capture-only SEH read of the saved SearchNearLevelGimmick descriptor argument.
// This identifies the hat's effect, not the current equipment state.
bool is_hat_treasure_source(std::uintptr_t frame, std::uintptr_t offset) noexcept;

// Samples the post-instruction counter on the owner thread. Unknown means the
// captured base cannot currently be read, not that the counter is inactive.
TreasureState sample_treasure_state(std::uintptr_t base, const MemoryReader& read);

} // namespace phi::detail
