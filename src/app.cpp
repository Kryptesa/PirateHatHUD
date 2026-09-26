#include "app.hpp"
#include "features/treasure_indicator.hpp"
#include "game/treasure_observer.hpp"
#include "overlay.hpp"
#include "platform/config.hpp"
#include "platform/hotkeys.hpp"
#include "platform/logger.hpp"
#include <string>
namespace phi {
void run_app(HMODULE module) {
  wchar_t path[MAX_PATH]{};
  GetModuleFileNameW(module, path, MAX_PATH);
  std::wstring folder = path;
  folder.resize(folder.find_last_of(L"\\/") + 1);
  open_log(folder + L"PirateHatHUD.log");
  log("0.3.0-dx12-preview");
  const auto config = read_config(folder + L"config.ini");
  if (!prepare_overlay_icon((folder + L"icon.png").c_str())) {
    log("Required icon.png missing or invalid; mod not started");
    close_log();
    return;
  }
  {
    TreasureIndicator indicator(config.enabled, config.force_show, config.x, config.y,
                                config.scale);
    TreasureObserver observer(log);
    auto subscription = observer.subscribe([&indicator](const TreasureStateChanged& event) {
      indicator.set_treasure_state(event.current);
    });
    indicator.set_treasure_state(observer.state());
    set_overlay_hud(indicator.hud_state());
    set_overlay_log(log);
    const bool overlay_started = start_overlay();
    log(overlay_started ? "DX12 hooks installed; waiting for swapchain"
                        : "DX12 hooks unavailable; overlay disabled");
    observer.start();
    for (;;) {
      observer.poll();
      const auto actions = poll_hotkeys(config.toggle_key, config.unload_key);
      if (actions.toggle) {
        indicator.toggle();
      }
      set_overlay_hud(indicator.hud_state());
      if (actions.unload) {
        break;
      }
      Sleep(30);
    }
    observer.stop();
    subscription.reset();
    if (overlay_started) {
      stop_overlay();
    }
    set_overlay_log(nullptr);
  }
  log("Unloaded");
  close_log();
}
} // namespace phi
