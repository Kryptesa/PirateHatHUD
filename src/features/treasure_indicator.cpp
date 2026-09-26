#include "features/treasure_indicator.hpp"

namespace phi {
TreasureIndicator::TreasureIndicator(bool enabled, bool force_show, int x, int y, float scale,
                                     int show_delay_ms)
    : enabled_(enabled), force_show_(force_show),
      show_delay_(show_delay_ms < 0 ? 1000 : show_delay_ms), hud_{false, x, y, scale} {}

void TreasureIndicator::set_treasure_state(TreasureState state) {
  treasure_state_ = state;
}

void TreasureIndicator::set_minimap_state(MinimapState state) {
  minimap_state_ = state;
  if (state != MinimapState::visible) {
    allowed_since_.reset();
    interface_ready_ = false;
  }
}

void TreasureIndicator::set_menu_state(MenuState state) {
  menu_state_ = state;
  if (state != MenuState::closed) {
    allowed_since_.reset();
    interface_ready_ = false;
  }
}
void TreasureIndicator::update(Clock::time_point now) {
  if (minimap_state_ != MinimapState::visible || menu_state_ != MenuState::closed) {
    allowed_since_.reset();
    interface_ready_ = false;
    return;
  }
  if (!allowed_since_) {
    allowed_since_ = now;
  }
  interface_ready_ = now - *allowed_since_ >= show_delay_;
}
void TreasureIndicator::toggle() {
  enabled_ = !enabled_;
}

HudState TreasureIndicator::hud_state() const {
  auto hud = hud_;
  hud.visible = enabled_ && (force_show_ || treasure_state_ == TreasureState::active) &&
                minimap_state_ == MinimapState::visible && menu_state_ == MenuState::closed &&
                interface_ready_;
  return hud;
}
} // namespace phi
