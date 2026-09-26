#pragma once
#include "render/hud_state.hpp"
namespace phi {
// Lifecycle/configuration calls belong to the application thread. Configure before start;
// stop before clearing the logger. HUD snapshots may be published while rendering.
void set_overlay_log(void (*logger)(const char*));
bool start_overlay();
void stop_overlay();
void set_overlay_hud(const HudState& hud);
bool prepare_overlay_icon(const wchar_t* path);
} // namespace phi
