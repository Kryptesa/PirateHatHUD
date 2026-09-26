#pragma once
#include "game/minimap_observer.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace phi::detail {
// Empirically verified on 2.03.02 only. Array slots are not stable object identities.
inline constexpr uintptr_t kMinimapRootRva = 0x6C8CC00;
inline constexpr std::array<uintptr_t, 13> kMinimapOffsets{
    0x30, 0x18, 0x88, 0x78, 0, 0x30EB8, 0x28, 0xA0, 0x10, 0x48, 0, 0x290, 0x18};
inline bool add_address(uintptr_t base, uintptr_t offset, uintptr_t& result) {
  if (!base || offset > std::numeric_limits<uintptr_t>::max() - base) {
    return false;
  }
  result = base + offset;
  return true;
}
template <typename Reader> MinimapState sample_minimap(uintptr_t module, Reader&& read) {
  uintptr_t address = 0;
  uintptr_t current = 0;
  if (!add_address(module, kMinimapRootRva, address) || !read(address, &current, sizeof(current))) {
    return MinimapState::unknown;
  }
  for (auto offset : kMinimapOffsets) {
    if (!add_address(current, offset, address) || !read(address, &current, sizeof(current))) {
      return MinimapState::unknown;
    }
  }
  uint8_t value = 0;
  if (!add_address(current, 0xBE, address) || !read(address, &value, sizeof(value))) {
    return MinimapState::unknown;
  }
  return value == 0   ? MinimapState::hidden
         : value == 1 ? MinimapState::visible
                      : MinimapState::unknown;
}
} // namespace phi::detail
