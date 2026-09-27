#include "overlay.hpp"
#include "render/dxgi_hooks.hpp"
#include "render/image.hpp"
#include <memory>
#include <mutex>
#include <utility>

namespace phi {

namespace {
std::unique_ptr<render::Dx12Renderer> g_renderer;
std::mutex g_hud_mutex;
HudState g_hud;
LogCallback g_logger{};
bool g_started = false;
bool g_start_attempted = false;
OverlayStopResult g_stop_result;

HudState snapshot() {
  std::lock_guard lock(g_hud_mutex);

  return g_hud;
}
} // namespace

bool start_overlay() {
  if (g_started) {
    return true;
  }

  if (g_stop_result.module_must_remain_loaded || !g_renderer) {
    return false;
  }

  if (!g_renderer->prepare_shaders()) {
    return false;
  }

  g_start_attempted = true;
  g_started = render::start_hooks(*g_renderer, snapshot, g_logger);

  return g_started;
}

OverlayStopResult stop_overlay() noexcept {
  if (!g_start_attempted) {
    return g_stop_result;
  }

  const auto result = render::stop_hooks();
  g_stop_result = {
    result.hooks_disabled,
    result.callbacks_drained,
    result.gpu_resources_released,
    result.module_must_remain_loaded
  };

  if (!result.hooks_disabled || !result.callbacks_drained || !result.gpu_resources_released) {
    // Outstanding callbacks/GPU work may still reference this renderer. Its allocation
    // deliberately survives the application and static destruction until process exit.
    (void)g_renderer.release();
    g_stop_result.module_must_remain_loaded = true;
  }

  g_started = false;
  g_start_attempted = false;

  return g_stop_result;
}

void set_overlay_log(LogCallback logger) {
  g_logger = logger;

  if (g_renderer) {
    g_renderer->set_logger(logger);
  }
}

void set_overlay_hud(const HudState& hud) {
  std::lock_guard lock(g_hud_mutex);
  g_hud = hud;
}

bool prepare_overlay_icon(const wchar_t* path) {
  if (g_started || g_stop_result.module_must_remain_loaded) {
    return false;
  }

  if (!g_renderer) {
    g_renderer = std::make_unique<render::Dx12Renderer>();
    g_renderer->set_logger(g_logger);
  }

  render::Image image;
  const bool decoded = render::decode_image(path, image);
  g_renderer->set_image(std::move(image));

  return decoded;
}
} // namespace phi
