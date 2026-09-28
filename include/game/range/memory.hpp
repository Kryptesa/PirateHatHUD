#pragma once

#include "game/shared/memory_reader.hpp"
#include <functional>
#include <optional>

namespace phi::detail {
struct RangeLocation {
  uintptr_t pool_slot = 0;
  uintptr_t image_base = 0;
  size_t image_size = 0;
  uintptr_t data_vtable = 0;
};

struct RangeTarget {
  uintptr_t pool = 0;
  uintptr_t object = 0;
  uintptr_t keys = 0;
  float radius = 0;
  bool same_object(const RangeTarget& other) const;
};

std::optional<RangeTarget>
find_range_target(const RangeLocation& location, const MemoryReader& read);

// Changes one aligned writable float only if its bits still equal the expected value.
using FloatExchange = std::function<bool(uintptr_t, float, float)>;
bool exchange_range_float(uintptr_t address, float expected, float desired) noexcept;

enum class RangeStatus { disabled, waiting, applied, conflict, write_failed };

class RangeOverride {
public:
  RangeStatus update(
    const RangeLocation& location,
    const MemoryReader& read,
    const FloatExchange& exchange,
    float requested
  );
  RangeStatus
  restore(const RangeLocation& location, const MemoryReader& read, const FloatExchange& exchange);
  float original() const;

private:
  std::optional<RangeTarget> owned_;
  float applied_ = 0;
  float requested_ = 0;
  bool blocked_ = false;
};
} // namespace phi::detail
