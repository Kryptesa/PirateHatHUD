#pragma once

#include "core/log.hpp"
#include "render/dx12_renderer.hpp"

namespace phi::render {
using HudSnapshot = HudState (*)();

bool start_hooks(Dx12Renderer& renderer, HudSnapshot snapshot, LogCallback logger);

struct HooksStopResult {
  bool hooks_disabled;
  bool callbacks_drained;
  bool gpu_resources_released;
  bool module_must_remain_loaded;
};

HooksStopResult stop_hooks() noexcept;
} // namespace phi::render
