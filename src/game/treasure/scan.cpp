#include "game/treasure/scan.hpp"
#include "game/treasure/patterns.hpp"

namespace phi {

namespace {

PatternQuery treasure_query() {
  return {{patterns::kEnter}, {patterns::kLeave}, patterns::kExpectedDelta};
}

TreasureHookScanResult resolve(PatternMatch match, size_t delta) {
  TreasureHookScanResult result{};
  result.status = match.status;
  result.candidate_pairs = match.candidates;

  if (match.status == ScanStatus::found) {
    result.sites = {match.address, match.address + delta};
  }

  return result;
}
} // namespace

TreasureHookScanResult scan_treasure_code(std::span<const uint8_t> code, uintptr_t base) {
  return resolve(scan_pattern(code, base, treasure_query()), patterns::kExpectedDelta);
}

TreasureHookScanResult find_treasure_hook_sites(HMODULE game) {
  return resolve(find_pattern(game, treasure_query()), patterns::kExpectedDelta);
}
} // namespace phi
