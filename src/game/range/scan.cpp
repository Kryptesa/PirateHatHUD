#include "game/range/scan.hpp"
#include "game/shared/address.hpp"
#include <array>

namespace phi {
namespace {
constexpr uint8_t pool_type[] =
  {0x41, 0xBC, 0xC2, 0, 0, 0, 0x41, 0xBF, 1, 0, 0, 0, 0x45, 0x39, 0x6E, 0x54};
constexpr uint8_t registry[] = {
  0x41,
  0x8B,
  0xD4,
  0x48,
  0x8D,
  0x0D,
  0,
  0,
  0,
  0,
  0xE8,
  0,
  0,
  0,
  0,
  0x48,
  0x8B,
  0xC8,
  0x48,
  0x85,
  0xC0
};
constexpr uint8_t registry_mask[] = {1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1};
constexpr size_t kRegistryDelta = 0x51;
constexpr size_t kVtableDelta = 0xA9;
constexpr size_t kExtent = kVtableDelta + 10;

bool operands(std::span<const uint8_t> code, size_t offset, uintptr_t base) {
  if (offset > code.size() || kExtent > code.size() - offset) {
    return false;
  }
  const auto vtable = offset + kVtableDelta;
  if (
    code[vtable] != 0x48 ||
    code[vtable + 1] != 0x8D ||
    code[vtable + 2] != 0x05 ||
    code[vtable + 7] != 0x48 ||
    code[vtable + 8] != 0x89 ||
    code[vtable + 9] != 0x03
  ) {
    return false;
  }
  uintptr_t registry_address = 0, data_vtable = 0;
  return detail::resolve_rip_address(
           code,
           base,
           offset + kRegistryDelta,
           6,
           10,
           registry_address
         ) &&
    detail::resolve_rip_address(code, base, vtable, 3, 7, data_vtable) &&
    registry_address % alignof(uintptr_t) == 0 &&
    data_vtable % alignof(uintptr_t) == 0;
}

PatternQuery query() {
  return {{pool_type}, {registry, registry_mask}, kRegistryDelta};
}
} // namespace

PatternMatch scan_treasure_range_code(std::span<const uint8_t> code, uintptr_t base) {
  return scan_pattern(code, base, query(), operands);
}

detail::RangeLocation find_treasure_range_location(HMODULE game) {
  const auto match = find_pattern(game, query(), operands);
  if (match.status != ScanStatus::found || match.image_size < sizeof(uintptr_t)) {
    return {};
  }
  std::array<uint8_t, kExtent> code{};
  if (!detail::read_memory(match.address, code.data(), code.size())) {
    return {};
  }
  uintptr_t registry_address = 0, data_vtable = 0, slot = 0;
  if (
    !detail::resolve_rip_address(code, match.address, kRegistryDelta, 6, 10, registry_address) ||
    !detail::resolve_rip_address(code, match.address, kVtableDelta, 3, 7, data_vtable) ||
    !detail::add_address(registry_address, 0x38 + 0xC2 * sizeof(uintptr_t), slot)
  ) {
    return {};
  }
  const auto in_image = [&](uintptr_t value) {
    return value >= match.image_base &&
      value - match.image_base <= match.image_size - sizeof(uintptr_t);
  };
  if (!in_image(slot) || !in_image(data_vtable)) {
    return {};
  }
  return {slot, match.image_base, match.image_size, data_vtable};
}
} // namespace phi
