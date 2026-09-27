#pragma once

#include "game/observer_state.hpp"
#include "render/hud_state.hpp"
#include <chrono>
#include <optional>

namespace phi {
// Updated and queried by the application thread; the renderer receives a copy of hud_state().
class TreasureIndicator {
public:
  TreasureIndicator(
    bool enabled,
    bool force_show,
    int x,
    int y,
    float scale,
    int show_delay_ms = 1000
  );

  void set_treasure_state(TreasureState state);

  void set_minimap_state(MinimapState state);
  using Clock = std::chrono::steady_clock;

  void set_menu_state(MenuState state);

  void update(Clock::time_point now = Clock::now());

  void toggle();

  HudState hud_state() const;

private:
  bool enabled_;
  bool force_show_;
  TreasureState treasure_state_ = TreasureState::unknown;
  MinimapState minimap_state_ = MinimapState::unknown;
  MenuState menu_state_ = MenuState::unknown;
  std::chrono::milliseconds show_delay_;
  std::optional<Clock::time_point> allowed_since_;
  bool interface_ready_ = false;
  HudState hud_;
};
} // namespace phi
