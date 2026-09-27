#pragma once

#include <cstdint>
#include <limits>

namespace phi::detail {
inline bool add_address(uintptr_t base, uintptr_t offset, uintptr_t& result) {
  if (!base || offset > (std::numeric_limits<uintptr_t>::max)() - base) {
    return false;
  }

  result = base + offset;

  return true;
}
} // namespace phi::detail
