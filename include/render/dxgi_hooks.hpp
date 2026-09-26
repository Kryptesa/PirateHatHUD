#pragma once
#include "render/dx12_renderer.hpp"

namespace phi::render {
using HudSnapshot = HudState (*)();
bool start_hooks(Dx12Renderer& renderer, HudSnapshot snapshot, void (*logger)(const char*));
void stop_hooks();
} // namespace phi::render
