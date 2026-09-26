#pragma once
#include <windows.h>
#include <cstddef>
#include <cstdint>
#include <span>
namespace phi {
struct HookSites {
  std::uintptr_t enter = 0;
  std::uintptr_t leave = 0;
};
enum class ScanStatus { found, invalid_image, no_match, ambiguous };
struct ScanResult {
  HookSites sites{};
  ScanStatus status = ScanStatus::invalid_image;
  std::size_t candidate_pairs = 0;
};
ScanResult find_hook_sites(HMODULE game, bool menu = false);
ScanResult scan_code(std::span<const std::uint8_t> code, std::uintptr_t base, bool menu = false);
} // namespace phi
