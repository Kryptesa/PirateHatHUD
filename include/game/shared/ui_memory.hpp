#pragma once

#include "game/shared/memory_reader.hpp"
#include <map>
#include <string>
#include <string_view>
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

  bool immutable(uintptr_t address, size_t size) const;
};

UiIdentityCache make_ui_identity_cache(uintptr_t module);

// Resolve current UI objects by exact script RTTI, never by a heap address or array index.
uintptr_t find_ui_root(
  uintptr_t module,
  const MemoryReader& read,
  std::string_view name,
  UiIdentityCache* cache = nullptr
);
} // namespace phi::detail
