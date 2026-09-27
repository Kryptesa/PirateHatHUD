#include "game/ui_root_scan.hpp"
#include <cstring>
namespace phi {
namespace {
// Verified launcher constructor; RIP-relative operands alone are masked.
constexpr uint8_t context[] = {0x90, 0x48, 0x8D, 0x05, 0,    0,    0,    0,    0x48, 0x89,
                               0x03, 0x48, 0x89, 0x1D, 0,    0,    0,    0,    0xC6, 0x05,
                               0,    0,    0,    0,    3,    0x48, 0xC7, 0x44, 0x24, 0x40,
                               0,    0,    0,    0,    0x48, 0x85, 0xDB};
constexpr uint8_t context_mask[] = {1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1,
                                    1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
PatternQuery query() {
  return {{context, context_mask}};
}
uintptr_t slot(std::span<const uint8_t> code, size_t offset, uintptr_t base) {
  int32_t displacement = 0;
  std::memcpy(&displacement, code.data() + offset + 14, 4);
  const auto next = base + offset + 18;
  const auto distance =
      displacement < 0 ? uint64_t(-int64_t(displacement)) : uint64_t(displacement);
  return displacement < 0 ? next - distance : next + distance;
}
bool valid_operand(std::span<const uint8_t> code, size_t offset, uintptr_t base) {
  int32_t displacement = 0;
  std::memcpy(&displacement, code.data() + offset + 14, 4);
  const auto next = base + offset + 18;
  const auto distance =
      displacement < 0 ? uint64_t(-int64_t(displacement)) : uint64_t(displacement);
  return displacement < 0 ? next >= distance : next <= UINTPTR_MAX - distance;
}
} // namespace
UiRootScanResult scan_ui_root_code(std::span<const uint8_t> code, uintptr_t base) {
  const auto match = scan_pattern(code, base, query(), valid_operand);
  return {match.status == ScanStatus::found ? slot(code, match.address - base, base) : 0,
          match.status, match.candidates};
}
UiRootScanResult find_ui_root_slot(HMODULE game) {
  const auto match = find_pattern(game, query(), valid_operand);
  UiRootScanResult result{0, match.status, match.candidates};
  if (match.status != ScanStatus::found) {
    return result;
  }
  result.root_slot =
      slot({reinterpret_cast<const uint8_t*>(match.address), sizeof(context)}, 0, match.address);
  if (result.root_slot < match.image_base ||
      result.root_slot - match.image_base > match.image_size - sizeof(uintptr_t) ||
      result.root_slot % alignof(uintptr_t)) {
    return {};
  }
  return result;
}
} // namespace phi
