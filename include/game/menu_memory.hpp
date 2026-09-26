#pragma once
#include "game/observer_state.hpp"
#include "game/minimap_memory.hpp"
#include <array>
#include <cstring>
#include <map>
#include <vector>

namespace phi::detail {
enum class MenuIdentity { other, menu, unknown };
struct MenuImageRange {
  uintptr_t begin = 0;
  size_t size = 0;
};
// Only immutable image RTTI is cached. All heap chains and array entries are read every poll.
struct MenuCachedType {
  uintptr_t name_address = 0;
  MenuIdentity identity = MenuIdentity::unknown;
};
struct MenuIdentityCache {
  std::vector<MenuImageRange> immutable_ranges;
  std::map<uintptr_t, MenuCachedType> types;
  bool immutable(uintptr_t address, size_t size) const {
    for (const auto& range : immutable_ranges) {
      if (address >= range.begin && address - range.begin <= range.size &&
          size <= range.size - (address - range.begin)) {
        return true;
      }
    }
    return false;
  }
};

template <typename Reader>
MenuIdentity identify_menu_script(uintptr_t module, uintptr_t vtable, Reader&& read,
                                  MenuIdentityCache* cache) {
  if (vtable < sizeof(uintptr_t)) {
    return MenuIdentity::unknown;
  }
  uintptr_t address = 0;
  bool cache_locator = false;
  if (cache) {
    const auto found = cache->types.find(vtable);
    if (found != cache->types.end()) {
      if (found->second.identity != MenuIdentity::unknown) {
        return found->second.identity;
      }
      address = found->second.name_address;
    }
  }
  if (!address) {
    uintptr_t col = 0;
    if (!read(vtable - sizeof(uintptr_t), &col, sizeof(col)) || !col) {
      return MenuIdentity::unknown;
    }
    std::array<uint32_t, 6> locator{};
    if (!read(col, locator.data(), sizeof(locator))) {
      return MenuIdentity::unknown;
    }
    // A readable object without game-module MSVC RTTI is a different identity.
    if (locator[0] != 1 || col < locator[5] || col - locator[5] != module) {
      return MenuIdentity::other;
    }
    if (!add_address(module, locator[3], address) || !add_address(address, 16, address)) {
      return MenuIdentity::unknown;
    }
    cache_locator = cache && cache->immutable(vtable - sizeof(uintptr_t), sizeof(uintptr_t) * 2) &&
                    cache->immutable(col, sizeof(locator));
  }
  constexpr char name[] = ".?AVUIGamePlayControl_Root_MainMenu@uiCommonScript@pa@@";
  std::array<char, sizeof(name)> actual{};
  if (!read(address, actual.data(), actual.size())) {
    return MenuIdentity::unknown;
  }
  const auto identity = std::memcmp(actual.data(), name, sizeof(name)) == 0 ? MenuIdentity::menu
                                                                            : MenuIdentity::other;
  if (cache_locator && cache->types.size() < 4096) {
    // MSVC type descriptors may live in writable .data: cache the immutable locator,
    // but continue reading the name on every poll unless its storage is immutable too.
    cache->types.emplace(vtable, MenuCachedType{address, cache->immutable(address, actual.size())
                                                             ? identity
                                                             : MenuIdentity::unknown});
  }
  return identity;
}

// Resolve current UI objects by exact MSVC script RTTI, never by a heap address or array index.
template <typename Reader>
uintptr_t find_menu_root(uintptr_t module, Reader&& read, MenuIdentityCache* cache = nullptr) {
  auto pointer = [&](uintptr_t base, uintptr_t offset, uintptr_t& value) {
    uintptr_t address = 0;
    return add_address(base, offset, address) && read(address, &value, sizeof(value));
  };
  uintptr_t owner = 0;
  if (!pointer(module, kMinimapRootRva, owner) || !owner) {
    return 0;
  }
  for (auto offset : {0x30u, 0x18u, 0x88u, 0x78u, 0u}) {
    if (!pointer(owner, offset, owner) || !owner) {
      return 0;
    }
  }
  uintptr_t array = 0, address = 0;
  uint32_t count = 0;
  if (!pointer(owner, 0x30EB8, array) || !array || !add_address(owner, 0x30EC0, address) ||
      !read(address, &count, sizeof(count)) || count == 0 || count > 4096) {
    return 0;
  }
  uintptr_t found = 0;
  for (uint32_t i = 0; i < count; ++i) {
    uintptr_t entry = 0, node = 0, root = 0, script = 0, vtable = 0;
    if (!pointer(array, i * sizeof(uintptr_t), entry)) {
      return 0;
    }
    // Null entries/links are legitimate empty objects, but failed reads invalidate uniqueness.
    if (!entry) {
      continue;
    }
    if (!pointer(entry, 0xA0, node)) {
      return 0;
    }
    if (!node) {
      continue;
    }
    if (!pointer(node, 0x10, root)) {
      return 0;
    }
    if (!root) {
      continue;
    }
    if (!pointer(root, 0x118, script)) {
      return 0;
    }
    if (!script) {
      continue;
    }
    if (!pointer(script, 0, vtable)) {
      return 0;
    }
    const auto identity = identify_menu_script(module, vtable, read, cache);
    if (identity == MenuIdentity::unknown) {
      return 0;
    }
    if (identity == MenuIdentity::other) {
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
