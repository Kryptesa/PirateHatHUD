#pragma once
#include "render/hud_state.hpp"
namespace phi {
void set_overlay_log(void (*logger)(const char*));
bool start_overlay();
void stop_overlay();
void set_overlay_hud(const HudState& hud);
bool prepare_overlay_icon(const wchar_t* path);
} // namespace phi
