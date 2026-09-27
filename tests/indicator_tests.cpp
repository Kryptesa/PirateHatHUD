#include "features/treasure_indicator.hpp"

#define CHECK(c)                                                                                   \
  do {                                                                                             \
    if (!(c)) {                                                                                    \
      return __LINE__;                                                                             \
    }                                                                                              \
  } while (false)

int main() {
  phi::TreasureIndicator indicator(true, false, 123, -45, 1.5f, 0);
  indicator.set_menu_state(phi::MenuState::closed);
  CHECK(!(indicator.update(), indicator.hud_state().visible));

  indicator.set_treasure_state(phi::TreasureState::inactive);
  CHECK(!(indicator.update(), indicator.hud_state().visible));

  indicator.set_treasure_state(phi::TreasureState::active);
  CHECK(!(indicator.update(), indicator.hud_state().visible));

  indicator.set_minimap_state(phi::MinimapState::visible);
  CHECK((indicator.update(), indicator.hud_state().visible));

  indicator.toggle();
  CHECK(!(indicator.update(), indicator.hud_state().visible));

  indicator.toggle();
  CHECK((indicator.update(), indicator.hud_state().visible));

  indicator.set_treasure_state(phi::TreasureState::unknown);
  CHECK(!(indicator.update(), indicator.hud_state().visible));

  indicator.set_treasure_state(phi::TreasureState::active);
  indicator.set_minimap_state(phi::MinimapState::hidden);
  CHECK(!(indicator.update(), indicator.hud_state().visible));

  indicator.set_minimap_state(phi::MinimapState::unknown);
  CHECK(!(indicator.update(), indicator.hud_state().visible));

  indicator.set_minimap_state(phi::MinimapState::visible);
  CHECK((indicator.update(), indicator.hud_state().visible));

  const auto hud = indicator.hud_state();
  CHECK(hud.x == 123);
  CHECK(hud.y == -45);
  CHECK(hud.scale == 1.5f);

  phi::TreasureIndicator forced(true, true, 0, 0, 1.0f, 0);
  forced.set_menu_state(phi::MenuState::closed);
  CHECK(!(forced.update(), forced.hud_state().visible));

  forced.set_minimap_state(phi::MinimapState::hidden);
  CHECK(!(forced.update(), forced.hud_state().visible));

  forced.set_minimap_state(phi::MinimapState::visible);
  CHECK((forced.update(), forced.hud_state().visible));

  forced.set_treasure_state(phi::TreasureState::inactive);
  CHECK((forced.update(), forced.hud_state().visible));

  forced.toggle();
  CHECK(!(forced.update(), forced.hud_state().visible));

  forced.set_treasure_state(phi::TreasureState::active);
  CHECK(!(forced.update(), forced.hud_state().visible));

  forced.toggle();
  CHECK((forced.update(), forced.hud_state().visible));

  phi::TreasureIndicator disabled(false, false, 350, -310, 1.0f, 0);
  disabled.set_menu_state(phi::MenuState::closed);
  disabled.set_treasure_state(phi::TreasureState::active);
  CHECK(!(disabled.update(), disabled.hud_state().visible));

  disabled.set_minimap_state(phi::MinimapState::visible);
  disabled.toggle();
  CHECK((disabled.update(), disabled.hud_state().visible));

  using Clock = phi::TreasureIndicator::Clock;
  using namespace std::chrono_literals;
  const auto t = Clock::time_point{};

  for (bool menu_first : {false, true}) {
    phi::TreasureIndicator timed(true, true, 0, 0, 1);
    timed.update(t);
    CHECK(!timed.hud_state().visible);

    if (menu_first) {
      timed.set_menu_state(phi::MenuState::closed);
    } else {
      timed.set_minimap_state(phi::MinimapState::visible);
    }

    timed.update(t + 2s);
    CHECK(!timed.hud_state().visible);

    if (menu_first) {
      timed.set_minimap_state(phi::MinimapState::visible);
    } else {
      timed.set_menu_state(phi::MenuState::closed);
    }

    timed.update(t + 3s);
    timed.update(t + 3999ms);
    CHECK(!timed.hud_state().visible);

    timed.update(t + 4s);
    CHECK(timed.hud_state().visible);

    timed.set_menu_state(phi::MenuState::open);
    CHECK(!timed.hud_state().visible);

    timed.set_menu_state(phi::MenuState::closed);
    timed.update(t + 5s);
    timed.set_minimap_state(phi::MinimapState::hidden);
    timed.set_minimap_state(phi::MinimapState::visible);
    timed.update(t + 5500ms);
    timed.update(t + 6s);
    CHECK(!timed.hud_state().visible);

    timed.update(t + 6500ms);
    CHECK(timed.hud_state().visible);

    for (bool menu_unknown : {false, true}) {
      if (menu_unknown) {
        timed.set_menu_state(phi::MenuState::unknown);
      } else {
        timed.set_minimap_state(phi::MinimapState::unknown);
      }

      timed.update(t + 10s);
      CHECK(!timed.hud_state().visible);

      timed.set_menu_state(phi::MenuState::closed);
      timed.set_minimap_state(phi::MinimapState::visible);
      timed.update(t + 11s);
      CHECK(!timed.hud_state().visible);

      timed.update(t + 12s);
      CHECK(timed.hud_state().visible);
    }
  }
}
