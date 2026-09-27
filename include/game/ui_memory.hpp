#pragma once
#include "game/observer_state.hpp"
#include "game/address.hpp"
#include <string>
#include <array>
#include <cstring>
#include <map>
#include <vector>

namespace phi::detail {
enum class UiIdentity { other, matched, unknown };
struct UiImageRange {
  uintptr_t begin = 0;
  size_t size = 0;
};
// Only immutable image RTTI is cached. All heap chains and array entries are read every poll.
struct UiCachedType {
  uintptr_t name_address = 0;
  UiIdentity identity = UiIdentity::unknown;
};
struct UiIdentityCache {
  uintptr_t root_slot = 0;
  std::string expected_name;
  std::vector<UiImageRange> immutable_ranges;
  std::map<uintptr_t, UiCachedType> types;
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

UiIdentityCache make_ui_identity_cache(uintptr_t module);

template <typename Reader, size_t N>
UiIdentity identify_ui_script(uintptr_t module, uintptr_t vtable, Reader&& read,
                              const char (&name)[N], UiIdentityCache* cache) {
  if (cache && cache->expected_name != name) {
    cache->types.clear();
    cache->expected_name = name;
  }
  if (vtable < sizeof(uintptr_t)) {
    return UiIdentity::unknown;
  }
  uintptr_t address = 0;
  bool cache_locator = false;
  if (cache) {
    const auto found = cache->types.find(vtable);
    if (found != cache->types.end()) {
      if (found->second.identity != UiIdentity::unknown) {
        return found->second.identity;
      }
      address = found->second.name_address;
    }
  }
  if (!address) {
    uintptr_t col = 0;
    if (!read(vtable - sizeof(uintptr_t), &col, sizeof(col)) || !col) {
      return UiIdentity::unknown;
    }
    std::array<uint32_t, 6> locator{};
    if (!read(col, locator.data(), sizeof(locator))) {
      return UiIdentity::unknown;
    }
    // A readable object without game-module MSVC RTTI is a different identity.
    if (locator[0] != 1 || col < locator[5] || col - locator[5] != module) {
      return UiIdentity::other;
    }
    if (!add_address(module, locator[3], address) || !add_address(address, 16, address)) {
      return UiIdentity::unknown;
    }
    cache_locator = cache && cache->immutable(vtable - sizeof(uintptr_t), sizeof(uintptr_t) * 2) &&
                    cache->immutable(col, sizeof(locator));
  }
  std::array<char, sizeof(name)> actual{};
  if (!read(address, actual.data(), actual.size())) {
    return UiIdentity::unknown;
  }
  const auto identity =
      std::memcmp(actual.data(), name, sizeof(name)) == 0 ? UiIdentity::matched : UiIdentity::other;
  if (cache_locator && cache->types.size() < 4096) {
    // MSVC type descriptors may live in writable .data: cache the immutable locator,
    // but continue reading the name on every poll unless its storage is immutable too.
    cache->types.emplace(vtable, UiCachedType{address, cache->immutable(address, actual.size())
                                                           ? identity
                                                           : UiIdentity::unknown});
  }
  return identity;
}

// Resolve current UI objects by exact MSVC script RTTI, never by a heap address or array index.
template <typename Reader, size_t N>
uintptr_t find_ui_root(uintptr_t module, Reader&& read, const char (&name)[N],
                       UiIdentityCache* cache = nullptr) {
  auto pointer = [&](uintptr_t base, uintptr_t offset, uintptr_t& value) {
    uintptr_t address = 0;
    return add_address(base, offset, address) && read(address, &value, sizeof(value));
  };
  uintptr_t owner = 0;
  if (!module || !cache || !cache->root_slot || !read(cache->root_slot, &owner, sizeof(owner)) ||
      !owner) {
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
    const auto identity = identify_ui_script(module, vtable, read, name, cache);
    if (identity == UiIdentity::unknown) {
      return 0;
    }
    if (identity == UiIdentity::other) {
      continue;
    }
    if (found) {
      return 0;
    }
    found = root;
  }
  return found;
}
} // namespace phi::detail
