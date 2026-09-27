#include "game/treasure/memory.hpp"
#include "game/treasure/patterns.hpp"

#include <cstring>
#include <limits>

#define CHECK(c)                                                                                   \
  do {                                                                                             \
    if (!(c)) {                                                                                    \
      return __LINE__;                                                                             \
    }                                                                                              \
  } while (false)

int main() {
  using namespace phi;
  constexpr std::uintptr_t base = 0x100000;
  std::uint32_t counter = 0;
  bool available = true;
  bool partial_failure = false;
  unsigned reads = 0;
  detail::MemoryReader read = [&](std::uintptr_t address, void* destination, std::size_t size) {
    ++reads;

    if (!available || address != base + patterns::kStateOffset || size != sizeof(counter)) {
      return false;
    }

    std::memcpy(destination, &counter, sizeof(counter));
    return !partial_failure;
  };

  CHECK(detail::sample_treasure_state(base, read) == TreasureState::inactive);
  counter = 1;
  CHECK(detail::sample_treasure_state(base, read) == TreasureState::active);
  counter = (std::numeric_limits<std::uint32_t>::max)();
  CHECK(detail::sample_treasure_state(base, read) == TreasureState::active);

  available = false;
  CHECK(detail::sample_treasure_state(base, read) == TreasureState::unknown);
  available = true;
  partial_failure = true;
  CHECK(detail::sample_treasure_state(base, read) == TreasureState::unknown);
  partial_failure = false;
  counter = 0;
  CHECK(detail::sample_treasure_state(base, read) == TreasureState::inactive);
  CHECK(detail::sample_treasure_state(base + 0x100, read) == TreasureState::unknown);

  const auto before_invalid = reads;
  CHECK(detail::sample_treasure_state(0, read) == TreasureState::unknown);
  CHECK(detail::sample_treasure_state(UINTPTR_MAX, read) == TreasureState::unknown);
  CHECK(
    detail::sample_treasure_state(
      UINTPTR_MAX - patterns::kStateOffset - sizeof(counter) + 1,
      read
    ) == TreasureState::unknown
  );
  CHECK(reads == before_invalid);
  CHECK(detail::sample_treasure_state(base, {}) == TreasureState::unknown);

  return 0;
}
