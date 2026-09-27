#include "game/audio_volume_scan.hpp"
#include <cstring>
#include <limits>

namespace phi {
namespace {
constexpr uint8_t context[] = {
  0x48,
  0x8B,
  0x05,
  0,
  0,
  0,
  0,
  0xC5,
  0xFB,
  0x10,
  0xB0,
  0xC8,
  0,
  0,
  0,
  0x8B,
  0x98,
  0xD0,
  0,
  0,
  0,
  0x48,
  0x8B,
  0xCE
};
constexpr uint8_t mask[] = {1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

uintptr_t resolve_slot(std::span<const uint8_t> code, size_t offset, uintptr_t base) {
  if (
    offset > code.size() ||
    code.size() - offset < sizeof(context) ||
    base > UINTPTR_MAX - offset ||
    base + offset > UINTPTR_MAX - 7
  ) {
    return 0;
  }
  int32_t displacement = 0;
  std::memcpy(&displacement, code.data() + offset + 3, sizeof(displacement));
  const auto next = base + offset + 7;
  const auto distance =
    displacement < 0 ? uint64_t(-int64_t(displacement)) : uint64_t(displacement);
  if (
    (displacement < 0 && next < distance) || (displacement >= 0 && next > UINTPTR_MAX - distance)
  ) {
    return 0;
  }
  const auto slot = displacement < 0 ? next - distance : next + distance;
  return slot && slot % alignof(uintptr_t) == 0 ? slot : 0;
}

bool valid_operand(std::span<const uint8_t> code, size_t offset, uintptr_t base) {
  return resolve_slot(code, offset, base) != 0;
}
} // namespace

PatternMatch scan_audio_volume_code(std::span<const uint8_t> code, uintptr_t base) {
  return scan_pattern(code, base, {{context, mask}}, valid_operand);
}

detail::AudioVolumeLocation find_audio_volume_location(HMODULE game) {
  const auto match = find_pattern(game, {{context, mask}}, valid_operand);
  if (match.status != ScanStatus::found || match.image_size < sizeof(uintptr_t)) {
    return {};
  }
  const auto slot = resolve_slot(
    {reinterpret_cast<const uint8_t*>(match.address), sizeof(context)},
    0,
    match.address
  );
  if (slot < match.image_base || slot - match.image_base > match.image_size - sizeof(uintptr_t)) {
    return {};
  }
  return {slot, match.image_base, match.image_size};
}
} // namespace phi
