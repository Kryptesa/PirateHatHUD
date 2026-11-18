#pragma once

#include "game/shared/memory_reader.hpp"
#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace phi::detail {
enum class UiIdentity { other, matched, unknown };
enum class UiLayout { original, expanded };

class UiLayoutSelection {
public:
  using Clock = std::chrono::steady_clock;

  std::optional<UiLayout> layout() const {
    return layout_;
  }

  template <typename Sample, typename Read>
  Sample sample(Clock::time_point now, Sample unknown, Read read) {
    if (layout_) {
      const auto current = read(*layout_);
      if (current != unknown) {
        return current;
      }
      layout_.reset();
    }

    // The UI can be absent for many polls while the game loads. Bound the cost of probing.
    if (now < next_probe_) {
      return unknown;
    }
    next_probe_ = now + std::chrono::milliseconds(100);

    const auto original = read(UiLayout::original);
    const auto expanded = read(UiLayout::expanded);
    if ((original == unknown) == (expanded == unknown)) {
      return unknown;
    }
    if (original != unknown) {
      layout_ = UiLayout::original;
      return original;
    }
    layout_ = UiLayout::expanded;
    return expanded;
  }

private:
  std::optional<UiLayout> layout_;
  Clock::time_point next_probe_{};
};

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
  uintptr_t image_base = 0;
  std::string expected_name;
  std::vector<UiImageRange> immutable_ranges;
  // Image-relative keys avoid making cache nodes resemble native script objects.
  std::map<uintptr_t, UiCachedType> types;

  bool immutable(uintptr_t address, size_t size) const;
};

UiIdentityCache make_ui_identity_cache(uintptr_t module);

// Resolve current UI objects by exact script RTTI, never by a heap address or array index.
uintptr_t find_ui_root(
  uintptr_t module,
  const MemoryReader& read,
  std::string_view name,
  UiIdentityCache* cache,
  UiLayout layout
);
} // namespace phi::detail
