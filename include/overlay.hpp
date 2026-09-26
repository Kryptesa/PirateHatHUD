#pragma once
#include <windows.h>
namespace phi {
void set_overlay_log(void (*logger)(const char*));
bool start_overlay();
void stop_overlay();
void set_overlay_enabled(bool enabled);
void set_overlay_force(bool force);
bool prepare_overlay_icon(const wchar_t* path);
void set_overlay_position(int x, int y, float scale);
void set_overlay_state(bool active);
} // namespace phi
