#pragma once

#include "pattern_scan.hpp"

namespace phi {
struct UiRootScanResult {
  uintptr_t root_slot = 0;
  ScanStatus status = ScanStatus::invalid_image;
  size_t candidates = 0;
};

UiRootScanResult scan_ui_root_code(std::span<const uint8_t> code, uintptr_t base);

UiRootScanResult find_ui_root_slot(HMODULE game);
} // namespace phi
