#pragma once
#include "pattern_scan.hpp"
namespace phi {
struct HookSites {
  uintptr_t enter = 0;
  uintptr_t leave = 0;
};
struct HookScanResult {
  HookSites sites{};
  uintptr_t state_offset = 0;
  ScanStatus status = ScanStatus::invalid_image;
  size_t candidate_pairs = 0;
};
HookScanResult scan_treasure_code(std::span<const uint8_t> code, uintptr_t base);
HookScanResult scan_menu_code(std::span<const uint8_t> code, uintptr_t base);
HookScanResult find_treasure_hook_sites(HMODULE game);
HookScanResult find_menu_hook_sites(HMODULE game);
} // namespace phi
