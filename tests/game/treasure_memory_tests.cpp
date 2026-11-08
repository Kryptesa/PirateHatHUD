#include "game/treasure/memory.hpp"
#include "game/treasure/patterns.hpp"

#include <cstring>
#include <array>
#include <initializer_list>
#include <limits>

#define CHECK(c)                                                                                   \
  do {                                                                                             \
    if (!(c)) {                                                                                    \
      return __LINE__;                                                                             \
    }                                                                                              \
  } while (false)

int main() {
  using namespace phi;
  CHECK(detail::treasure_descriptor_stack_offset(0x6AED2E85, 0x13CC4000) == 0x200);
  CHECK(detail::treasure_descriptor_stack_offset(0, 0) == 0x1F0);
  CHECK(detail::treasure_descriptor_stack_offset(0x6AED2E85, 0x13CC4001) == 0x1F0);
  CHECK(detail::treasure_descriptor_stack_offset(0x6AED2E84, 0x13CC4000) == 0x1F0);
  std::array<std::uint8_t, 0x80> descriptor{};
  auto write_descriptor = [&](std::size_t offset, auto value) {
    std::memcpy(descriptor.data() + offset, &value, sizeof(value));
  };
  // Descriptor fields from the runtime hat-range investigation.
  write_descriptor(0x40, std::uint32_t{0x52029BCF});
  descriptor[0x4C] = 0xC0;
  descriptor[0x4E] = 2;
  descriptor[0x58] = 1;
  write_descriptor(0x74, std::uint32_t{0x9B28835C});
  write_descriptor(0x78, std::uint32_t{0xCC908146});
  descriptor[0x7C] = 1;
  std::array<std::uint8_t, 0x210> frame{};
  const auto frame_address = reinterpret_cast<std::uintptr_t>(frame.data());
  auto source = reinterpret_cast<std::uintptr_t>(descriptor.data());
  for (const auto offset : {0x1F0u, 0x200u}) {
    frame.fill(0);
    std::memcpy(frame.data() + offset, &source, sizeof(source));
    CHECK(detail::is_hat_treasure_source(frame_address, offset));
    CHECK(!detail::is_hat_treasure_source(frame_address, offset == 0x200 ? 0x1F0 : 0x200));

    // A non-hat finder shares both event hashes and the equip condition, but
    // uses drop condition 2. It must not become the source of icon/sound events.
    descriptor[0x7C] = 2;
    CHECK(!detail::is_hat_treasure_source(frame_address, offset));
    descriptor[0x7C] = 1;
    for (const auto field : {0x40u, 0x4Cu, 0x4Eu, 0x58u, 0x74u, 0x78u}) {
      descriptor[field] ^= 1;
      CHECK(!detail::is_hat_treasure_source(frame_address, offset));
      descriptor[field] ^= 1;
    }
    CHECK(detail::is_hat_treasure_source(frame_address, offset));
  }
  CHECK(!detail::is_hat_treasure_source(0, 0x200));
  CHECK(!detail::is_hat_treasure_source(frame_address, 0));
  CHECK(!detail::is_hat_treasure_source(UINTPTR_MAX, 0x200));
  CHECK(!detail::is_hat_treasure_source(frame_address, UINTPTR_MAX));
  CHECK(!detail::is_hat_treasure_source(1, 0x200));
  for (const auto invalid : {std::uintptr_t{0}, std::uintptr_t{1}, UINTPTR_MAX}) {
    std::memcpy(frame.data() + 0x200, &invalid, sizeof(invalid));
    CHECK(!detail::is_hat_treasure_source(frame_address, 0x200));
  }

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
