#pragma once

#include "core/log.hpp"
#include "render/hud_state.hpp"

namespace phi {
// Lifecycle/configuration calls belong to the application thread. Configure before start;

// stop before clearing the logger. HUD snapshots may be published while rendering.
void set_overlay_log(LogCallback logger);

bool start_overlay();

struct OverlayStopResult {
  bool hooks_disabled = true;
  bool callbacks_drained = true;
  bool gpu_resources_released = true;
  bool module_must_remain_loaded = false;
};

// A retained result forbids restart and physical DLL unload, including after partial start.
OverlayStopResult stop_overlay() noexcept;

void set_overlay_hud(const HudState& hud);

// A null path selects the PNG embedded in this module; a path selects a file override.
bool prepare_overlay_icon(const wchar_t* path = nullptr);
} // namespace phi
