#include "app.hpp"
#include "features/treasure_indicator.hpp"
#include "game/treasure_observer.hpp"
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
    log(message);
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
  void open(const std::wstring& path) {
    attempted_ = true;
    open_log(path);
  }

private:
  ExitState& exit_;
  bool attempted_ = false;
};
class AppSession {
public:
  AppSession(const Config& config, ExitState& exit)
      : exit_(exit),
        indicator_(config.enabled, config.force_show, config.x, config.y, config.scale),
        observer_(log) {}
  ~AppSession() noexcept {
    finish();
  }
  void run(const Config& config) {
    subscription_ = observer_.subscribe([this](const TreasureStateChanged& event) {
      indicator_.set_treasure_state(event.current);
    });
    indicator_.set_treasure_state(observer_.state());
    set_overlay_hud(indicator_.hud_state());
    logger_configured_ = true;
    set_overlay_log(log);
    const bool overlay_started = start_overlay();
    exit_.retain |= overlay_started;
    log(overlay_started ? "DX12 hooks installed; waiting for swapchain"
                        : "DX12 hooks unavailable; overlay disabled");
    exit_.retain |= observer_.start();
    for (;;) {
      observer_.poll();
      const auto actions = poll_hotkeys(config.toggle_key, config.unload_key);
      if (actions.toggle) {
        indicator_.toggle();
      }
      set_overlay_hud(indicator_.hud_state());
      if (actions.unload) {
        break;
      }
      Sleep(30);
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
    subscription_.reset();
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
  Subscription subscription_;
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
      logging.open(folder + L"PirateHatHUD.log");
      log("0.3.0-dx12-preview");
      const auto config = read_config(folder + L"config.ini");
      if (!prepare_overlay_icon((folder + L"icon.png").c_str())) {
        log("Required icon.png missing or invalid; mod not started");
      } else {
        AppSession session(config, exit);
        session.run(config);
      }
      log(exit.retain ? "Stopped; DLL retained" : "Stopped; DLL unload allowed");
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
