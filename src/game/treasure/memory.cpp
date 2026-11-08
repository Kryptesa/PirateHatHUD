#include "game/treasure/memory.hpp"
#include "game/treasure/patterns.hpp"
#include "game/shared/image_build.hpp"
#include <windows.h>

namespace phi::detail {

std::uintptr_t treasure_descriptor_stack_offset(std::uint32_t timestamp, std::uint32_t image_size) {
  return is_exe_1_0_0_3576(timestamp, image_size) ? 0x200 : 0x1F0;
}

bool is_hat_treasure_source(std::uintptr_t frame, std::uintptr_t offset) noexcept {
  if (
    !frame ||
    !offset ||
    offset > UINTPTR_MAX - sizeof(std::uintptr_t) ||
    frame > UINTPTR_MAX - offset - sizeof(std::uintptr_t)
  ) {
    return false;
  }

  // Use the saved descriptor argument: the register holding it can be clobbered
  // by event dispatch before the enter instruction executes.
  __try {
    const auto descriptor = *reinterpret_cast<volatile const std::uintptr_t*>(frame + offset);
    if (!descriptor || descriptor > UINTPTR_MAX - 0x80) {
      return false;
    }
    return *reinterpret_cast<volatile const std::uint32_t*>(descriptor + 0x40) == 0x52029BCF &&
      *reinterpret_cast<volatile const std::uint8_t*>(descriptor + 0x4C) == 0xC0 &&
      *reinterpret_cast<volatile const std::uint8_t*>(descriptor + 0x4E) == 2 &&
      *reinterpret_cast<volatile const std::uint8_t*>(descriptor + 0x58) == 1 &&
      *reinterpret_cast<volatile const std::uint32_t*>(descriptor + 0x74) == 0x9B28835C &&
      *reinterpret_cast<volatile const std::uint32_t*>(descriptor + 0x78) == 0xCC908146 &&
      *reinterpret_cast<volatile const std::uint8_t*>(descriptor + 0x7C) == 1;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

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
