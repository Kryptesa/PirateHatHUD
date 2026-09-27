#include "game/shared/address.hpp"
#include <cstring>

namespace phi::detail {

bool resolve_rip_address(
  std::span<const uint8_t> code,
  uintptr_t base,
  size_t candidate_offset,
  size_t displacement_offset,
  size_t next_instruction_offset,
  uintptr_t& result
) {
  if (
    candidate_offset > code.size() ||
    next_instruction_offset > code.size() - candidate_offset ||
    displacement_offset > next_instruction_offset ||
    sizeof(int32_t) > next_instruction_offset - displacement_offset ||
    candidate_offset > UINTPTR_MAX - base ||
    next_instruction_offset > UINTPTR_MAX - (base + candidate_offset)
  ) {
    return false;
  }
  int32_t displacement = 0;
  std::memcpy(
    &displacement,
    code.data() + candidate_offset + displacement_offset,
    sizeof(displacement)
  );
  const auto next = base + candidate_offset + next_instruction_offset;
  const auto distance =
    displacement < 0 ? uintptr_t(-int64_t(displacement)) : uintptr_t(displacement);
  if (
    (displacement < 0 && next < distance) || (displacement >= 0 && distance > UINTPTR_MAX - next)
  ) {
    return false;
  }
  result = displacement < 0 ? next - distance : next + distance;
  return true;
}

} // namespace phi::detail
