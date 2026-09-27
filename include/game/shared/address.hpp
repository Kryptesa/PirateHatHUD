#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace phi::detail {
inline bool add_address(uintptr_t base, uintptr_t offset, uintptr_t& result) {
  if (!base || offset > (std::numeric_limits<uintptr_t>::max)() - base) {
    return false;
  }

  result = base + offset;

  return true;
}

// Offsets are relative to the candidate instruction. Reject address wraparound.
bool resolve_rip_address(
  std::span<const uint8_t> code,
  uintptr_t base,
  size_t candidate_offset,
  size_t displacement_offset,
  size_t next_instruction_offset,
  uintptr_t& result
);
} // namespace phi::detail
