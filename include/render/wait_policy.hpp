#pragma once

#include <cstdint>
#include <limits>

namespace phi::render {
enum class WaitResult { completed, device_lost, timeout, failed };

// Adapter supplies completion/device status, a monotonic clock, and one bounded
// event wait. Spurious wakes always consume the same absolute deadline.
template <class Adapter>
WaitResult wait_for_fence(Adapter& adapter, std::uint64_t target, std::uint64_t deadline) {
  for (;;) {
    const auto completed = adapter.completed();

    if (completed == std::numeric_limits<std::uint64_t>::max() || adapter.device_lost()) {
      return WaitResult::device_lost;
    }

    if (completed >= target) {
      return WaitResult::completed;
    }

    const auto now = adapter.now();

    if (now >= deadline) {
      return WaitResult::timeout;
    }

    if (!adapter.wait(target, deadline - now)) {
      return WaitResult::failed;
    }
  }
}

constexpr bool can_release(WaitResult result) {
  return result == WaitResult::completed || result == WaitResult::device_lost;
}
} // namespace phi::render
