#include "features/treasure_sound.hpp"

#define CHECK(c)                                                                                   \
  do {                                                                                             \
    if (!(c)) {                                                                                    \
      return __LINE__;                                                                             \
    }                                                                                              \
  } while (false)

int main() {
  using phi::TreasureSound;
  using phi::TreasureState;
  using namespace std::chrono_literals;
  const auto t = TreasureSound::Clock::time_point{};

  TreasureSound sound;
  CHECK(!sound.update(TreasureState::active, true, t)); // Startup is silent.
  CHECK(!sound.update(TreasureState::inactive, true, t));
  CHECK(sound.update(TreasureState::active, true, t));
  CHECK(!sound.update(TreasureState::active, true, t + 2s)); // No repeats while active.
  CHECK(!sound.update(TreasureState::unknown, true, t + 3s));
  CHECK(!sound.update(TreasureState::active, true, t + 3s)); // Recovery is silent too.
  CHECK(!sound.update(TreasureState::inactive, true, t + 3s));
  CHECK(sound.update(TreasureState::active, true, t + 3s));

  TreasureSound gated;
  CHECK(!gated.update(TreasureState::inactive, true, t));
  CHECK(!gated.update(TreasureState::active, false, t));     // Menu, focus or mod gating.
  CHECK(!gated.update(TreasureState::active, true, t + 2s)); // Do not replay on return.
  CHECK(!gated.update(TreasureState::inactive, false, t + 2s));
  CHECK(gated.update(TreasureState::active, true, t + 2s)); // Suppression uses no cooldown.

  TreasureSound timed;
  CHECK(!timed.update(TreasureState::inactive, true, t));
  CHECK(timed.update(TreasureState::active, true, t));
  CHECK(!timed.update(TreasureState::inactive, true, t + 999ms));
  CHECK(!timed.update(TreasureState::active, true, t + 999ms));
  CHECK(!timed.update(TreasureState::active, true, t + 1s)); // Cooldown expiry is no event.
  CHECK(!timed.update(TreasureState::inactive, true, t + 1s));
  CHECK(timed.update(TreasureState::active, true, t + 1s)); // Exact boundary is allowed.

  TreasureSound disabled(false, 0);
  CHECK(!disabled.update(TreasureState::inactive, true, t));
  CHECK(!disabled.update(TreasureState::active, true, t));
  TreasureSound immediate(true, 0);
  CHECK(!immediate.update(TreasureState::inactive, true, t));
  CHECK(immediate.update(TreasureState::active, true, t));
  CHECK(!immediate.update(TreasureState::inactive, true, t));
  CHECK(immediate.update(TreasureState::active, true, t));

  TreasureSound fallback(true, -1);
  CHECK(!fallback.update(TreasureState::inactive, true, t));
  CHECK(fallback.update(TreasureState::active, true, t));
  CHECK(!fallback.update(TreasureState::inactive, true, t + 999ms));
  CHECK(!fallback.update(TreasureState::active, true, t + 999ms));
}
