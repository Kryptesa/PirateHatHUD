#include "game/treasure/memory.hpp"
#include "game/treasure/patterns.hpp"

namespace phi::detail {

TreasureState sample_treasure_state(std::uintptr_t base, const MemoryReader& read) {
  if (!base || base > UINTPTR_MAX - patterns::kStateOffset - sizeof(std::uint32_t)) {
    return TreasureState::unknown;
  }

  std::uint32_t counter = 0;

  if (!read_field(read, base, patterns::kStateOffset, counter)) {
    return TreasureState::unknown;
  }

  return counter > 0 ? TreasureState::active : TreasureState::inactive;
}

} // namespace phi::detail
