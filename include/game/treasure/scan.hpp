#pragma once

#include "game/shared/hook_sites.hpp"
#include "pattern_scan.hpp"

namespace phi {
struct TreasureHookScanResult {
  HookSites sites{};
  ScanStatus status = ScanStatus::invalid_image;
  size_t candidate_pairs = 0;
};

TreasureHookScanResult scan_treasure_code(std::span<const uint8_t> code, uintptr_t base);

TreasureHookScanResult find_treasure_hook_sites(HMODULE game);
} // namespace phi
