#include "game/audio/scan.hpp"
#include "game/shared/address.hpp"
#include "game/shared/image_build.hpp"

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
  uintptr_t slot = 0;
  if (!detail::resolve_rip_address(code, base, offset, 3, 7, slot)) {
    return 0;
  }
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
  IMAGE_DOS_HEADER dos{};
  IMAGE_NT_HEADERS64 nt{};
  if (
    !detail::read_memory(match.image_base, &dos, sizeof(dos)) ||
    dos.e_lfanew <= 0 ||
    dos.e_lfanew > 0x1000 ||
    !detail::read_field(detail::read_memory, match.image_base, dos.e_lfanew, nt)
  ) {
    return {};
  }
  const auto manager_offset =
    detail::is_exe_1_0_0_3576(nt.FileHeader.TimeDateStamp, nt.OptionalHeader.SizeOfImage)
      ? 0x1078u
      : 0x1070u;
  return {slot, match.image_base, match.image_size, manager_offset};
}
} // namespace phi
