#pragma once
#include "game/menu_observer.hpp"
#include "game/minimap_memory.hpp"
#include <array>
#include <cstring>
namespace phi::detail {
// Resolve current UI objects by exact MSVC script RTTI, never by a heap address or array index.
template <typename Reader> uintptr_t find_menu_root(uintptr_t module, Reader&& read) {
  auto pointer = [&](uintptr_t base, uintptr_t offset) {
    uintptr_t address = 0, value = 0;
    return add_address(base, offset, address) && read(address, &value, sizeof(value)) ? value : 0;
  };
  auto owner = pointer(module, kMinimapRootRva);
  for (auto offset : {0x30u, 0x18u, 0x88u, 0x78u, 0u}) {
    owner = pointer(owner, offset);
  }
  const auto array = pointer(owner, 0x30EB8);
  uintptr_t address = 0;
  uint32_t count = 0;
  if (!array || !add_address(owner, 0x30EC0, address) || !read(address, &count, sizeof(count)) ||
      count == 0 || count > 4096) {
    return 0;
  }
  uintptr_t found = 0;
  for (uint32_t i = 0; i < count; ++i) {
    const auto entry = pointer(array, i * sizeof(uintptr_t));
    const auto root = pointer(pointer(entry, 0xA0), 0x10);
    const auto script = pointer(root, 0x118);
    const auto vtable = pointer(script, 0);
    if (vtable < sizeof(uintptr_t)) {
      continue;
    }
    uintptr_t col = 0;
    if (!read(vtable - sizeof(uintptr_t), &col, sizeof(col)) || !col) {
      continue;
    }
    std::array<uint32_t, 6> locator{};
    if (!read(col, locator.data(), sizeof(locator)) || locator[0] != 1 || col < locator[5]) {
      continue;
    }
    const auto image = col - locator[5];
    if (image != module || !add_address(image, locator[3], address) ||
        !add_address(address, 16, address)) {
      continue;
    }
    constexpr char name[] = ".?AVUIGamePlayControl_Root_MainMenu@uiCommonScript@pa@@";
    std::array<char, sizeof(name)> actual{};
    if (!read(address, actual.data(), actual.size()) ||
        std::memcmp(actual.data(), name, sizeof(name)) != 0) {
      continue;
    }
    if (found) {
      return 0;
    }
    found = root;
  }
  return found;
}
template <typename Reader> MenuState sample_menu(uintptr_t root, Reader&& read) {
  uintptr_t address = 0;
  uint8_t value = 0;
  if (!add_address(root, 0x25B, address) || !read(address, &value, sizeof(value))) {
    return MenuState::unknown;
  }
  return value == 0 ? MenuState::closed : value == 1 ? MenuState::open : MenuState::unknown;
}
} // namespace phi::detail
