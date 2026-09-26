#include "overlay.hpp"
#include "render/dxgi_hooks.hpp"
#include "render/image.hpp"
#include <mutex>
#include <utility>

namespace phi {
namespace {
render::Dx12Renderer g_renderer;
std::mutex g_hud_mutex;
HudState g_hud;
void (*g_logger)(const char*){};
bool g_started = false;
HudState snapshot() {
  std::lock_guard lock(g_hud_mutex);
  return g_hud;
}
} // namespace
bool start_overlay() {
  if (g_started) {
    return true;
  }
  g_started = render::start_hooks(g_renderer, snapshot, g_logger);
  return g_started;
}
void stop_overlay() {
  if (!g_started) {
    return;
  }
  render::stop_hooks();
  g_started = false;
}
void set_overlay_log(void (*logger)(const char*)) {
  g_logger = logger;
  g_renderer.set_logger(logger);
}
void set_overlay_hud(const HudState& hud) {
  std::lock_guard lock(g_hud_mutex);
  g_hud = hud;
}
bool prepare_overlay_icon(const wchar_t* path) {
  if (g_started) {
    return false;
  }
  render::Image image;
  const bool decoded = render::decode_image(path, image);
  g_renderer.set_image(std::move(image));
  return decoded;
}
} // namespace phi
