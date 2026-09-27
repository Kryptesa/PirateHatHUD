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
  CHECK(!sound.update(TreasureState::unknown, true, t));
  CHECK(sound.update(TreasureState::active, true, t)); // First detection after loading notifies.
  CHECK(!sound.update(TreasureState::active, true, t));
  CHECK(!sound.update(TreasureState::inactive, true, t + 1s));
  CHECK(sound.update(TreasureState::active, true, t + 1s));
  CHECK(!sound.update(TreasureState::active, true, t + 2s)); // No repeats while active.
  CHECK(!sound.update(TreasureState::unknown, true, t + 3s));
  CHECK(!sound.update(TreasureState::active, true, t + 3s)); // Recovery is silent too.
  CHECK(!sound.update(TreasureState::inactive, true, t + 3s));
  CHECK(sound.update(TreasureState::active, true, t + 3s));

  TreasureSound loading;
  CHECK(!loading.update(TreasureState::unknown, false, t));
  CHECK(!loading.update(TreasureState::active, false, t)); // Loading/menu still suppress audio.
  CHECK(!loading.update(TreasureState::active, false, t + 1s));
  CHECK(loading.update(TreasureState::active, true, t + 2s)); // First eligible gameplay sample.
  CHECK(!loading.update(TreasureState::active, false, t + 3s));
  CHECK(!loading.update(TreasureState::active, true, t + 4s)); // No replay on later menu exits.

  TreasureSound empty_loading;
  CHECK(!empty_loading.update(TreasureState::active, false, t));
  CHECK(!empty_loading.update(TreasureState::inactive, true, t + 1s));
  CHECK(!empty_loading.update(TreasureState::inactive, true, t + 2s));
  CHECK(empty_loading.update(TreasureState::active, true, t + 3s));

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
