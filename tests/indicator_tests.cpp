#include "features/treasure_indicator.hpp"

#define CHECK(c)                                                                                   \
  do {                                                                                             \
    if (!(c)) {                                                                                    \
      return __LINE__;                                                                             \
    }                                                                                              \
  } while (false)

int main() {
  phi::TreasureIndicator indicator(true, false, 123, -45, 1.5f);
  CHECK(!indicator.hud_state().visible);
  indicator.set_treasure_state(phi::TreasureState::inactive);
  CHECK(!indicator.hud_state().visible);
  indicator.set_treasure_state(phi::TreasureState::active);
  CHECK(!indicator.hud_state().visible);
  indicator.set_minimap_state(phi::MinimapState::visible);
  CHECK(indicator.hud_state().visible);
  indicator.toggle();
  CHECK(!indicator.hud_state().visible);
  indicator.toggle();
  CHECK(indicator.hud_state().visible);
  indicator.set_treasure_state(phi::TreasureState::unknown);
  CHECK(!indicator.hud_state().visible);
  indicator.set_treasure_state(phi::TreasureState::active);
  indicator.set_minimap_state(phi::MinimapState::hidden);
  CHECK(!indicator.hud_state().visible);
  indicator.set_minimap_state(phi::MinimapState::unknown);
  CHECK(!indicator.hud_state().visible);
  indicator.set_minimap_state(phi::MinimapState::visible);
  CHECK(indicator.hud_state().visible);
  const auto hud = indicator.hud_state();
  CHECK(hud.x == 123);
  CHECK(hud.y == -45);
  CHECK(hud.scale == 1.5f);

  phi::TreasureIndicator forced(true, true, 0, 0, 1.0f);
  CHECK(!forced.hud_state().visible);
  forced.set_minimap_state(phi::MinimapState::hidden);
  CHECK(!forced.hud_state().visible);
  forced.set_minimap_state(phi::MinimapState::visible);
  CHECK(forced.hud_state().visible);
  forced.set_treasure_state(phi::TreasureState::inactive);
  CHECK(forced.hud_state().visible);
  forced.toggle();
  CHECK(!forced.hud_state().visible);
  forced.set_treasure_state(phi::TreasureState::active);
  CHECK(!forced.hud_state().visible);
  forced.toggle();
  CHECK(forced.hud_state().visible);

  phi::TreasureIndicator disabled(false, false, 350, -310, 1.0f);
  disabled.set_treasure_state(phi::TreasureState::active);
  CHECK(!disabled.hud_state().visible);
  disabled.set_minimap_state(phi::MinimapState::visible);
  disabled.toggle();
  CHECK(disabled.hud_state().visible);
}
