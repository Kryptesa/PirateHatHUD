#pragma once

#include "game/shared/hook_sites.hpp"
#include "pattern_scan.hpp"

namespace phi {
struct MenuHookScanResult {
  HookSites sites{};
  uintptr_t state_offset = 0;
  ScanStatus status = ScanStatus::invalid_image;
  size_t candidate_pairs = 0;
};

MenuHookScanResult scan_menu_code(std::span<const uint8_t> code, uintptr_t base);

MenuHookScanResult find_menu_hook_sites(HMODULE game);
} // namespace phi
