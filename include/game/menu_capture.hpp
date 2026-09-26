#pragma once
#include "game/observer_state.hpp"
#include <atomic>
#include <cstdint>
#include <optional>

namespace phi::detail {
// One atomic transaction records both the transition generation and an opening latch.
// Hook callbacks touch only this process-lifetime capture object.
class MenuCapture {
public:
  void record(bool opening) noexcept {
    auto previous = events_.load(std::memory_order_relaxed);
    while (!events_.compare_exchange_weak(
        previous, ((previous + 2) & ~uint64_t{1}) | (previous & 1) | uint64_t{opening},
        std::memory_order_release, std::memory_order_relaxed)) {
    }
  }
  uint64_t take() noexcept {
    return events_.fetch_and(~uint64_t{1}, std::memory_order_acq_rel);
  }
  void reset() noexcept {
    events_.store(0, std::memory_order_relaxed);
  }

private:
  std::atomic<uint64_t> events_{0};
};
// Unknown must remain fail-closed even if an opening was captured for a replaced root.
// A transition during sampling prevents accepting a potentially pre-instruction closed byte.
inline std::optional<MenuState> menu_poll_state(MenuState sampled, uint64_t before,
                                                uint64_t after) {
  if (sampled == MenuState::unknown) {
    return MenuState::unknown;
  }
  if ((before | after) & 1) {
    return MenuState::open;
  }
  if ((before & ~uint64_t{1}) != (after & ~uint64_t{1})) {
    return std::nullopt;
  }
  return sampled;
}
} // namespace phi::detail
