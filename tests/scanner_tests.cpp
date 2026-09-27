#include "pattern_scan.hpp"
#include "patterns.hpp"
#include <array>
#include <algorithm>
#define CHECK(c)                                                                                   \
  do {                                                                                             \
    if (!(c))                                                                                      \
      return __LINE__;                                                                             \
  } while (false)
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
  std::vector<uint8_t> menu(1024);
  const uint8_t clear[] = {0xC6, 0x81, 0x5B, 0x02, 0, 0, 0, 0x84, 0xD2, 0x74, 0x1C};
  const uint8_t set[] = {0xC6, 0x83, 0x5B, 0x02, 0, 0, 1, 0x48, 0x8B, 1, 0xFF, 0x50, 0x30};
  CHECK(phi::scan_code(menu, 0x1000, true).status == phi::ScanStatus::no_match);
  std::copy(std::begin(clear), std::end(clear), menu.begin() + 8);
  std::copy(std::begin(set), std::end(set), menu.begin() + 8 + 0x141);
  auto pair = phi::scan_code(menu, 0x1000, true);
  CHECK(pair.status == phi::ScanStatus::found);
  CHECK(pair.sites.enter == 0x1008 && pair.sites.leave == 0x1149);
  CHECK(pair.state_offset == 0x25B);
  menu[10] = menu[10 + 0x141] = 0x60;
  CHECK(phi::scan_code(menu, 0x1000, true).state_offset == 0x260);
  menu[10 + 0x141] = 0x61;
  CHECK(phi::scan_code(menu, 0x1000, true).status == phi::ScanStatus::no_match);
  menu[10 + 0x141] = 0x60;
  menu[13] = menu[13 + 0x141] = 0xFF;
  CHECK(phi::scan_code(menu, 0x1000, true).status == phi::ScanStatus::no_match);
  menu[13] = menu[13 + 0x141] = 0;
  menu[9] = 0x82;
  CHECK(phi::scan_code(menu, 0x1000, true).status == phi::ScanStatus::no_match);
  menu[9] = 0x81;
  std::copy(std::begin(clear), std::end(clear), menu.begin() + 512);
  std::copy(std::begin(set), std::end(set), menu.begin() + 512 + 0x141);
  CHECK(phi::scan_code(menu, 0x1000, true).status == phi::ScanStatus::ambiguous);
  menu[512] = 0;
  menu[8 + 0x141 + 6] = 0;
  CHECK(phi::scan_code(menu, 0x1000, true).status == phi::ScanStatus::no_match);
  const uint8_t root[] = {0x90, 0x48, 0x8D, 0x05, 0,    0,    0,    0, 0x48, 0x89, 0x03, 0x48, 0x89,
                          0x1D, 0xEE, 0xFF, 0xFF, 0xFF, 0xC6, 0x05, 0, 0,    0,    0,    3,    0x48,
                          0xC7, 0x44, 0x24, 0x40, 0,    0,    0,    0, 0x48, 0x85, 0xDB};
  std::vector<uint8_t> roots(std::begin(root), std::end(root));
  CHECK(phi::scan_code(roots, 0x1000, false, true).root_slot == 0x1000);
  CHECK(phi::scan_code(roots, 0, false, true).status == phi::ScanStatus::found);
  CHECK(phi::scan_code(std::span(roots).first(20), 0x1000, false, true).status ==
        phi::ScanStatus::no_match);
  auto negative = roots;
  negative[14] = 0xED; // -19: next IP at 18 cannot resolve a nonnegative target from base 0.
  CHECK(phi::scan_code(negative, 0, false, true).status == phi::ScanStatus::no_match);
  auto positive = roots;
  positive[14] = 0x6E;
  positive[15] = positive[16] = positive[17] = 0;
  CHECK(phi::scan_code(positive, 0x1000, false, true).root_slot == 0x1080);
  CHECK(phi::scan_code(positive, UINTPTR_MAX - roots.size(), false, true).status ==
        phi::ScanStatus::no_match);
  roots.insert(roots.end(), std::begin(root), std::end(root));
  CHECK(phi::scan_code(roots, 0x1000, false, true).status == phi::ScanStatus::ambiguous);
}
