#include "features/treasure_indicator.hpp"

namespace phi {
TreasureIndicator::TreasureIndicator(bool enabled, bool force_show, int x, int y, float scale)
    : enabled_(enabled), force_show_(force_show), hud_{false, x, y, scale} {}

void TreasureIndicator::set_treasure_state(TreasureState state) {
  treasure_state_ = state;
}

void TreasureIndicator::set_minimap_state(MinimapState state) {
  minimap_state_ = state;
}

void TreasureIndicator::toggle() {
  enabled_ = !enabled_;
}

HudState TreasureIndicator::hud_state() const {
  auto hud = hud_;
  hud.visible = enabled_ && (force_show_ || treasure_state_ == TreasureState::active) &&
                minimap_state_ == MinimapState::visible;
  return hud;
}
} // namespace phi
