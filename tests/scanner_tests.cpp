#include "game/hook_scan.hpp"
#include "game/ui_root_scan.hpp"
#include "game/patterns.hpp"
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
  // Generic scanner must work without game signatures or decoded game fields.
  const uint8_t sample[] = {0xA1, 0x10, 0, 0xA1, 0x20};
  const uint8_t signature[] = {0xA1, 0};
  const uint8_t mask[] = {1, 0};
  phi::PatternQuery query{{signature, mask}};
  CHECK(phi::scan_pattern(sample, 0x1000, query).status == phi::ScanStatus::ambiguous);

  auto only_second = [](std::span<const uint8_t>, size_t offset, uintptr_t) { return offset == 3; };
  CHECK(phi::scan_pattern(sample, 0x1000, query, only_second).address == 0x1003);

  const uint8_t bad_mask[] = {1};
  CHECK(
    phi::scan_pattern(sample, 0x1000, {{signature, bad_mask}}).status ==
    phi::ScanStatus::invalid_image
  );
  CHECK(phi::scan_pattern(sample, 0x1000, {}).status == phi::ScanStatus::invalid_image);
  CHECK(phi::scan_pattern(sample, UINTPTR_MAX, query).status == phi::ScanStatus::no_match);
  CHECK(
    phi::scan_pattern(sample, 0x1000, {query.first, query.first, SIZE_MAX}).status ==
    phi::ScanStatus::invalid_image
  );

  // Executable-section uniqueness is global; pairs may not cross section boundaries.
  std::vector<uint8_t> image(0x3000);
  auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(image.data());
  dos->e_magic = IMAGE_DOS_SIGNATURE;
  dos->e_lfanew = 0x80;
  auto nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(image.data() + 0x80);
  nt->Signature = IMAGE_NT_SIGNATURE;
  nt->OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
  nt->OptionalHeader.SizeOfImage = static_cast<DWORD>(image.size());
  nt->FileHeader.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER64);
  nt->FileHeader.NumberOfSections = 2;
  auto sections = IMAGE_FIRST_SECTION(nt);

  for (unsigned i = 0; i < 2; ++i) {
    sections[i].Characteristics = IMAGE_SCN_MEM_EXECUTE;
    sections[i].VirtualAddress = 0x1000 + i * 0x1000;
    sections[i].Misc.VirtualSize = 16;
  }

  image[0x1000] = 0xA1;
  auto module = reinterpret_cast<HMODULE>(image.data());
  CHECK(phi::find_pattern(module, query).status == phi::ScanStatus::found);

  image[0x2000] = 0xA1;
  CHECK(phi::find_pattern(module, query).status == phi::ScanStatus::ambiguous);

  image[0x1000] = 0;
  image[0x100C] = 0xA1;
  CHECK(
    phi::find_pattern(module, {query.first, query.first, 0xFF4}).status == phi::ScanStatus::no_match
  );
  CHECK(
    phi::find_pattern(module, {{signature, bad_mask}}).status == phi::ScanStatus::invalid_image
  );
  CHECK(phi::find_pattern(nullptr, query).status == phi::ScanStatus::invalid_image);

  std::array<std::uint8_t, 128> bytes{};
  auto empty = phi::scan_treasure_code(bytes, 0x1000);
  CHECK(empty.status == phi::ScanStatus::no_match);

  for (unsigned i = 0; i < sizeof(phi::patterns::kEnter); ++i)
    bytes[8 + i] = phi::patterns::kEnter[i];

  for (unsigned i = 0; i < sizeof(phi::patterns::kLeave); ++i)
    bytes[8 + phi::patterns::kExpectedDelta + i] = phi::patterns::kLeave[i];
  auto unique = phi::scan_treasure_code(bytes, 0x1000);
  CHECK(unique.status == phi::ScanStatus::found);
  CHECK(unique.sites.enter == 0x1008);
  CHECK(unique.sites.leave == 0x1034);

  for (unsigned i = 0; i < sizeof(phi::patterns::kEnter); ++i)
    bytes[70 + i] = phi::patterns::kEnter[i];

  for (unsigned i = 0; i < sizeof(phi::patterns::kLeave); ++i)
    bytes[70 + phi::patterns::kExpectedDelta + i] = phi::patterns::kLeave[i];
  CHECK(phi::scan_treasure_code(bytes, 0x1000).status == phi::ScanStatus::ambiguous);

  bytes[70 + phi::patterns::kExpectedDelta] = 0;
  CHECK(phi::scan_treasure_code(bytes, 0x1000).status == phi::ScanStatus::found);

  bytes[8 + phi::patterns::kExpectedDelta] = 0;
  CHECK(phi::scan_treasure_code(bytes, 0x1000).status == phi::ScanStatus::no_match);

  std::vector<uint8_t> menu(1024);
  const uint8_t clear[] = {0xC6, 0x81, 0x5B, 0x02, 0, 0, 0, 0x84, 0xD2, 0x74, 0x1C};
  const uint8_t set[] = {0xC6, 0x83, 0x5B, 0x02, 0, 0, 1, 0x48, 0x8B, 1, 0xFF, 0x50, 0x30};
  CHECK(phi::scan_menu_code(menu, 0x1000).status == phi::ScanStatus::no_match);

  std::copy(std::begin(clear), std::end(clear), menu.begin() + 8);
  std::copy(std::begin(set), std::end(set), menu.begin() + 8 + 0x141);
  auto pair = phi::scan_menu_code(menu, 0x1000);
  CHECK(pair.status == phi::ScanStatus::found);
  CHECK(pair.sites.enter == 0x1008 && pair.sites.leave == 0x1149);
  CHECK(pair.state_offset == 0x25B);

  menu[10] = menu[10 + 0x141] = 0x60;
  CHECK(phi::scan_menu_code(menu, 0x1000).state_offset == 0x260);

  menu[10 + 0x141] = 0x61;
  CHECK(phi::scan_menu_code(menu, 0x1000).status == phi::ScanStatus::no_match);

  menu[10 + 0x141] = 0x60;
  menu[13] = menu[13 + 0x141] = 0xFF;
  CHECK(phi::scan_menu_code(menu, 0x1000).status == phi::ScanStatus::no_match);

  menu[13] = menu[13 + 0x141] = 0;
  menu[9] = 0x82;
  CHECK(phi::scan_menu_code(menu, 0x1000).status == phi::ScanStatus::no_match);

  menu[9] = 0x81;
  std::copy(std::begin(clear), std::end(clear), menu.begin() + 512);
  std::copy(std::begin(set), std::end(set), menu.begin() + 512 + 0x141);
  CHECK(phi::scan_menu_code(menu, 0x1000).status == phi::ScanStatus::ambiguous);

  menu[512] = 0;
  menu[8 + 0x141 + 6] = 0;
  CHECK(phi::scan_menu_code(menu, 0x1000).status == phi::ScanStatus::no_match);

  const uint8_t root[] = {
    0x90,
    0x48,
    0x8D,
    0x05,
    0,
    0,
    0,
    0,
    0x48,
    0x89,
    0x03,
    0x48,
    0x89,
    0x1D,
    0xEE,
    0xFF,
    0xFF,
    0xFF,
    0xC6,
    0x05,
    0,
    0,
    0,
    0,
    3,
    0x48,
    0xC7,
    0x44,
    0x24,
    0x40,
    0,
    0,
    0,
    0,
    0x48,
    0x85,
    0xDB
  };

  std::vector<uint8_t> roots(std::begin(root), std::end(root));
  CHECK(phi::scan_ui_root_code(roots, 0x1000).root_slot == 0x1000);
  CHECK(phi::scan_ui_root_code(roots, 0).status == phi::ScanStatus::found);
  CHECK(
    phi::scan_ui_root_code(std::span(roots).first(20), 0x1000).status == phi::ScanStatus::no_match
  );
  auto negative = roots;
  negative[14] = 0xED; // -19: next IP at 18 cannot resolve a nonnegative target from base 0.
  CHECK(phi::scan_ui_root_code(negative, 0).status == phi::ScanStatus::no_match);

  auto positive = roots;
  positive[14] = 0x6E;
  positive[15] = positive[16] = positive[17] = 0;
  CHECK(phi::scan_ui_root_code(positive, 0x1000).root_slot == 0x1080);
  CHECK(
    phi::scan_ui_root_code(positive, UINTPTR_MAX - roots.size()).status == phi::ScanStatus::no_match
  );
  roots.insert(roots.end(), std::begin(root), std::end(root));
  CHECK(phi::scan_ui_root_code(roots, 0x1000).status == phi::ScanStatus::ambiguous);
}
