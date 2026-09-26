#include "features/treasure_sound.hpp"

namespace phi {
TreasureSound::TreasureSound(bool enabled, int cooldown_ms)
    : enabled_(enabled), cooldown_(cooldown_ms < 0 ? 1000 : cooldown_ms) {}

bool TreasureSound::update(TreasureState treasure, bool allowed, Clock::time_point now) {
  const bool activated = previous_ == TreasureState::inactive && treasure == TreasureState::active;
  previous_ = treasure;
  if (!activated || !enabled_ || !allowed ||
      (last_notification_ && now - *last_notification_ < cooldown_)) {
    return false;
  }
  last_notification_ = now;
  return true;
}
} // namespace phi
