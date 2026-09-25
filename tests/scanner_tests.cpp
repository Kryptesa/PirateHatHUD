#include "pattern_scan.hpp"
#include "patterns.hpp"
#include <array>
#define CHECK(c) do { if (!(c)) return __LINE__; } while (false)
#include <cstdint>
#include <vector>

int main() {
  std::array<std::uint8_t, 128> bytes{};
  auto empty = phi::scan_code(bytes, 0x1000);
  CHECK(empty.status == phi::ScanStatus::no_match);
  for (unsigned i = 0; i < sizeof(phi::patterns::kEnter); ++i)
    bytes[8 + i] = phi::patterns::kEnter[i];
  for (unsigned i = 0; i < sizeof(phi::patterns::kLeave); ++i)
    bytes[8 + phi::patterns::kExpectedDelta + i] = phi::patterns::kLeave[i];
  auto unique = phi::scan_code(bytes, 0x1000);
  CHECK(unique.status == phi::ScanStatus::found);
  CHECK(unique.sites.enter == 0x1008);
  CHECK(unique.sites.leave == 0x1034);
  for (unsigned i = 0; i < sizeof(phi::patterns::kEnter); ++i)
    bytes[70 + i] = phi::patterns::kEnter[i];
  for (unsigned i = 0; i < sizeof(phi::patterns::kLeave); ++i)
    bytes[70 + phi::patterns::kExpectedDelta + i] = phi::patterns::kLeave[i];
  CHECK(phi::scan_code(bytes, 0x1000).status == phi::ScanStatus::ambiguous);
  bytes[70 + phi::patterns::kExpectedDelta] = 0;
  CHECK(phi::scan_code(bytes, 0x1000).status == phi::ScanStatus::found);
  bytes[8 + phi::patterns::kExpectedDelta] = 0;
  CHECK(phi::scan_code(bytes, 0x1000).status == phi::ScanStatus::no_match);
}