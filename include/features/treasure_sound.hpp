#pragma once

#include "game/observer_state.hpp"
#include <chrono>
#include <optional>

namespace phi {
// Sampled on the application thread. Initial gameplay state may notify once;

// subsequent suppressed activations are consumed, never replayed.
class TreasureSound {
public:
  using Clock = std::chrono::steady_clock;

  explicit TreasureSound(bool enabled = true, int cooldown_ms = 1000);

  bool update(TreasureState treasure, bool allowed, Clock::time_point now = Clock::now());

private:
  bool enabled_;
  bool initialized_ = false;
  TreasureState previous_ = TreasureState::unknown;
  std::chrono::milliseconds cooldown_;
  std::optional<Clock::time_point> last_notification_;
};
} // namespace phi
