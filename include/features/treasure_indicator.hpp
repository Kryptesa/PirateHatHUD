#pragma once
#include "game/treasure_observer.hpp"
#include "render/hud_state.hpp"

namespace phi {
// Updated and queried by the application thread; the renderer receives a copy of hud_state().
class TreasureIndicator {
public:
  TreasureIndicator(bool enabled, bool force_show, int x, int y, float scale);
  void set_treasure_state(TreasureState state);
  void toggle();
  HudState hud_state() const;

private:
  bool enabled_;
  bool force_show_;
  TreasureState treasure_state_ = TreasureState::unknown;
  HudState hud_;
};
} // namespace phi
