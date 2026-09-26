#include "app.hpp"
#include "features/treasure_indicator.hpp"
#include "game/treasure_observer.hpp"
#include "game/minimap_observer.hpp"
#include "game/menu_observer.hpp"
#include "overlay.hpp"
#include "platform/config.hpp"
#include "platform/hotkeys.hpp"
#include "platform/logger.hpp"
#include <exception>
#include <string>
namespace phi {
namespace {
struct ExitState {
  bool retain = false;
};
void report_exception(const char* message) noexcept {
  try {
    log(LogLevel::error, message);
  } catch (...) {
    // Diagnostics must never interrupt cleanup.
  }
}
class LogSession {
public:
  explicit LogSession(ExitState& exit) : exit_(exit) {}
  ~LogSession() noexcept {
    if (attempted_) {
      try {
        close_log();
      } catch (...) {
        exit_.retain = true;
      }
    }
  }
  void open(const std::wstring& path, const LogConfig& config) {
    attempted_ = true;
    open_log(path, config);
  }

private:
  ExitState& exit_;
  bool attempted_ = false;
};
class AppSession {
public:
  AppSession(const Config& config, ExitState& exit)
      : exit_(exit), indicator_(config.enabled, config.force_show, config.x, config.y, config.scale,
                                config.show_delay_ms),
        observer_(log), minimap_(log), menu_(log) {}
  ~AppSession() noexcept {
    finish();
  }
  void run(const Config& config) {
    subscription_ = observer_.subscribe([this](const TreasureStateChanged& event) {
      indicator_.set_treasure_state(event.current);
    });
    minimap_subscription_ = minimap_.subscribe(
        [this](const MinimapStateChanged& event) { indicator_.set_minimap_state(event.current); });
    menu_subscription_ = menu_.subscribe(
        [this](const MenuStateChanged& event) { indicator_.set_menu_state(event.current); });
    indicator_.set_menu_state(menu_.state());
    indicator_.set_minimap_state(minimap_.state());
    indicator_.set_treasure_state(observer_.state());
    set_overlay_hud(indicator_.hud_state());
    logger_configured_ = true;
    set_overlay_log(log);
    const bool overlay_started = start_overlay();
    exit_.retain |= overlay_started;
    log(overlay_started ? LogLevel::info : LogLevel::error,
        overlay_started ? "DX12 hooks installed; waiting for swapchain"
                        : "DX12 hooks unavailable; overlay disabled");
    exit_.retain |= observer_.start();
    minimap_.start();
    exit_.retain |= menu_.start();
    for (;;) {
      observer_.poll();
      minimap_.poll();
      menu_.poll();
      indicator_.update();
      const auto actions = poll_hotkeys(config.toggle_key, config.unload_key);
      if (actions.toggle) {
        indicator_.toggle();
        log(LogLevel::info, "Indicator toggled by hotkey");
      }
      set_overlay_hud(indicator_.hud_state());
      if (actions.unload) {
        log(LogLevel::info, "Stop requested by hotkey");
        break;
      }
      // Keep menu visibility sampling responsive without busy-waiting. Windows may
      // round this timeout up according to the system timer resolution.
      Sleep(5);
    }
  }

private:
  void finish() noexcept {
    if (finished_) {
      return;
    }
    finished_ = true;
    const auto observer_stop = observer_.stop();
    exit_.retain |= observer_stop.module_must_remain_loaded || !observer_stop.hooks_disabled;
    minimap_.stop();
    const auto menu_stop = menu_.stop();
    exit_.retain |= menu_stop.module_must_remain_loaded || !menu_stop.hooks_disabled;
    subscription_.reset();
    minimap_subscription_.reset();
    menu_subscription_.reset();
    // Failed starts can leave partially installed hooks; always ask both owners to stop.
    const auto overlay_stop = stop_overlay();
    exit_.retain |= overlay_stop.module_must_remain_loaded || !overlay_stop.hooks_disabled ||
                    !overlay_stop.callbacks_drained || !overlay_stop.gpu_resources_released;
    if (logger_configured_ && overlay_stop.hooks_disabled && overlay_stop.callbacks_drained) {
      try {
        set_overlay_log(nullptr);
      } catch (...) {
        exit_.retain = true;
      }
    }
  }
  ExitState& exit_;
  TreasureIndicator indicator_;
  TreasureObserver observer_;
  MinimapObserver minimap_;
  MenuObserver menu_;
  Subscription menu_subscription_;
  Subscription subscription_;
  Subscription minimap_subscription_;
  bool logger_configured_ = false;
  bool finished_ = false;
};
} // namespace
AppExitDisposition run_app(HMODULE module) noexcept {
  ExitState exit;
  {
    LogSession logging(exit);
    try {
      wchar_t path[MAX_PATH]{};
      const auto length = GetModuleFileNameW(module, path, MAX_PATH);
      if (!length || length >= MAX_PATH) {
        return AppExitDisposition::unload_allowed;
      }
      std::wstring folder(path, length);
      folder.resize(folder.find_last_of(L"\\/") + 1);
      const auto config_path = folder + L"PirateHatHUD.ini";
      const auto legacy_config_path = folder + L"config.ini";
      const bool legacy_config =
          GetFileAttributesW(config_path.c_str()) == INVALID_FILE_ATTRIBUTES &&
          GetFileAttributesW(legacy_config_path.c_str()) != INVALID_FILE_ATTRIBUTES;
      const auto config = read_config(legacy_config ? legacy_config_path : config_path);
      logging.open(folder, config.logging);
      log(LogLevel::info, PHI_VERSION);
      if (legacy_config) {
        log(LogLevel::warn,
            "Using legacy config.ini; rename it to PirateHatHUD.ini before installing an update");
      }
      const auto icon_path = folder + L"PirateHatHUD_treasure.png";
      const bool custom_icon = GetFileAttributesW(icon_path.c_str()) != INVALID_FILE_ATTRIBUTES;
      if (!prepare_overlay_icon(custom_icon ? icon_path.c_str() : nullptr)) {
        log(LogLevel::error, "Embedded icon or PirateHatHUD_treasure.png invalid; mod not started");
      } else {
        AppSession session(config, exit);
        session.run(config);
      }
      log(LogLevel::info, exit.retain ? "Stopped; DLL retained" : "Stopped; DLL unload allowed");
    } catch (const std::exception& error) {
      report_exception("Application stopped after a C++ exception");
      report_exception(error.what());
    } catch (...) {
      report_exception("Application stopped after an unknown C++ exception");
    }
  }
  return exit.retain ? AppExitDisposition::retain_module : AppExitDisposition::unload_allowed;
}
} // namespace phi
