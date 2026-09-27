#include "game/hook_scan.hpp"
#include "game/patterns.hpp"
#include <cstring>
namespace phi {
namespace {
constexpr std::uint8_t clear[] = {0xC6, 0x81, 0x5B, 0x02, 0, 0, 0, 0x84, 0xD2, 0x74, 0x1C};
constexpr std::uint8_t set[] = {0xC6, 0x83, 0x5B, 0x02, 0, 0, 1, 0x48, 0x8B, 1, 0xFF, 0x50, 0x30};
constexpr uint8_t clear_mask[] = {1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1};
constexpr uint8_t set_mask[] = {1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1};
constexpr size_t kMenuDelta = 0x141;
PatternQuery menu_query() {
  return {{clear, clear_mask}, {set, set_mask}, kMenuDelta};
}
PatternQuery treasure_query() {
  return {{patterns::kEnter}, {patterns::kLeave}, patterns::kExpectedDelta};
}
uint32_t displacement(std::span<const uint8_t> bytes, size_t offset) {
  uint32_t value = 0;
  std::memcpy(&value, bytes.data() + offset + 2, 4);
  return value;
}
bool valid_menu(std::span<const uint8_t> bytes, size_t offset, uintptr_t) {
  // Exact C6 /0 encodings retain byte width, RCX/RBX bases, no index and immediate 0/1.
  const auto value = displacement(bytes, offset);
  return value && value <= 0x10000 && value == displacement(bytes, offset + kMenuDelta);
}
HookScanResult resolve(PatternMatch match, size_t delta) {
  HookScanResult result{};
  result.status = match.status;
  result.candidate_pairs = match.candidates;
  if (match.status == ScanStatus::found) {
    result.sites = {match.address, match.address + delta};
  }
  return result;
}
} // namespace
HookScanResult scan_treasure_code(std::span<const uint8_t> code, uintptr_t base) {
  return resolve(scan_pattern(code, base, treasure_query()), patterns::kExpectedDelta);
}
HookScanResult scan_menu_code(std::span<const uint8_t> code, uintptr_t base) {
  auto result = resolve(scan_pattern(code, base, menu_query(), valid_menu), kMenuDelta);
  if (result.status == ScanStatus::found) {
    result.state_offset = displacement(code, result.sites.enter - base);
  }
  return result;
}
HookScanResult find_treasure_hook_sites(HMODULE game) {
  return resolve(find_pattern(game, treasure_query()), patterns::kExpectedDelta);
}
HookScanResult find_menu_hook_sites(HMODULE game) {
  auto result = resolve(find_pattern(game, menu_query(), valid_menu), kMenuDelta);
  if (result.status == ScanStatus::found) {
    result.state_offset =
        displacement({reinterpret_cast<const uint8_t*>(result.sites.enter), 7}, 0);
  }
  return result;
}
} // namespace phi
