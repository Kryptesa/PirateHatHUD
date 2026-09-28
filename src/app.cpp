#include "app.hpp"
#include "features/treasure_indicator.hpp"
#include "features/treasure_sound.hpp"
#include "game/treasure_observer.hpp"
#include "game/minimap_observer.hpp"
#include "game/menu_observer.hpp"
#include "game/audio_volume_observer.hpp"
#include "overlay.hpp"
#include "platform/config.hpp"
#include "platform/hotkeys.hpp"
#include "platform/logger.hpp"
#include "platform/sound.hpp"
#include <exception>
#include <cstdio>
#include <string>
#include <vector>

namespace phi {

namespace {
struct ExitState {
  bool retain = false;
};

std::wstring module_path(HMODULE module) {
  std::wstring path(32768, L'\0');
  const auto length = GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
  if (!length || length >= path.size()) {
    return {};
  }

  path.resize(length);
  return path;
}

std::string module_name_utf8(HMODULE module) {
  auto path = module_path(module);
  if (path.empty()) {
    return "<unavailable>";
  }
  const auto separator = path.find_last_of(L"\\/");
  if (separator != std::wstring::npos) {
    path.erase(0, separator + 1);
  }

  const auto size = WideCharToMultiByte(
    CP_UTF8,
    0,
    path.data(),
    static_cast<int>(path.size()),
    nullptr,
    0,
    nullptr,
    nullptr
  );
  if (!size) {
    return "<unavailable>";
  }

  std::string result(size, '\0');
  WideCharToMultiByte(
    CP_UTF8,
    0,
    path.data(),
    static_cast<int>(path.size()),
    result.data(),
    size,
    nullptr,
    nullptr
  );
  return result;
}

std::string executable_version() {
  const auto path = module_path(nullptr);
  const auto size = GetFileVersionInfoSizeW(path.c_str(), nullptr);
  if (!size) {
    return "<unavailable>";
  }
  std::vector<std::uint8_t> bytes(size);
  if (!GetFileVersionInfoW(path.c_str(), 0, size, bytes.data())) {
    return "<unavailable>";
  }
  VS_FIXEDFILEINFO* info = nullptr;
  UINT info_size = 0;
  if (
    !VerQueryValueW(bytes.data(), L"\\", reinterpret_cast<void**>(&info), &info_size) ||
    !info ||
    info_size < sizeof(*info) ||
    info->dwSignature != 0xFEEF04BD
  ) {
    return "<unavailable>";
  }
  return std::to_string(HIWORD(info->dwFileVersionMS)) +
    "." +
    std::to_string(LOWORD(info->dwFileVersionMS)) +
    "." +
    std::to_string(HIWORD(info->dwFileVersionLS)) +
    "." +
    std::to_string(LOWORD(info->dwFileVersionLS));
}

void log_environment() {
  char message[192]{};
  std::snprintf(
    message,
    sizeof(message),
    "Build: source=%s; configuration=%s; MSVC=%d; x64",
    PHI_BUILD_ID,
    PHI_BUILD_CONFIG,
    _MSC_FULL_VER
  );
  log(LogLevel::info, message);

  using GetVersion = LONG(WINAPI*)(OSVERSIONINFOW*);
  const auto get_version =
    reinterpret_cast<GetVersion>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion"));
  OSVERSIONINFOW version{};
  version.dwOSVersionInfoSize = sizeof(version);
  if (get_version && get_version(&version) >= 0) {
    std::snprintf(
      message,
      sizeof(message),
      "Windows version: %lu.%lu; build=%lu",
      version.dwMajorVersion,
      version.dwMinorVersion,
      version.dwBuildNumber
    );
    log(LogLevel::info, message);
  } else {
    log(LogLevel::debug, "Windows version unavailable");
  }
}

void log_settings(const Config& config) {
  char message[320]{};
  std::snprintf(
    message,
    sizeof(message),
    "Mod settings: enabled=%s; force show=%s; position=%d,%d; scale=%.3f; show delay=%d ms; "
    "sound=%s; sound cooldown=%d ms; sound volume=%d%%; toggle key=0x%X; unload key=0x%X",
    config.enabled ? "yes" : "no",
    config.force_show ? "yes" : "no",
    config.x,
    config.y,
    static_cast<double>(config.scale),
    config.show_delay_ms,
    config.sound_enabled ? "yes" : "no",
    config.sound_cooldown_ms,
    config.sound_volume_percent,
    static_cast<unsigned>(config.toggle_key),
    static_cast<unsigned>(config.unload_key)
  );
  log(LogLevel::debug, message);
}

void report_exception(const char* message) noexcept {
  try {
    log(LogLevel::error, message);
  } catch (...) {
    // Diagnostics must never interrupt cleanup.
  }
}

class LogSession {
public:
  explicit LogSession(ExitState& exit)
    : exit_(exit) {}

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
    : exit_(exit),
      indicator_(
        config.enabled,
        config.force_show,
        config.x,
        config.y,
        config.scale,
        config.show_delay_ms
      ),
      sound_policy_(config.sound_enabled, config.sound_cooldown_ms),
      enabled_(config.enabled),
      observer_(log),
      minimap_(log),
      menu_(log),
      audio_volume_(log) {}

  ~AppSession() noexcept {
    finish();
  }

  void run(const Config& config, const std::wstring& folder, const std::wstring& config_path) {
    SoundVolumeConfig sound_volume(config_path, config.sound_volume_percent);
    const auto sound_path = folder + L"PirateHatHUD_treasure.wav";
    const bool custom_sound = GetFileAttributesW(sound_path.c_str()) != INVALID_FILE_ATTRIBUTES;

    if (
      config.sound_enabled &&
      !(custom_sound ? sound_.prepare(sound_path) : sound_.prepare_embedded())
    ) {
      log(LogLevel::warn, "Embedded or custom treasure WAV invalid; sound unavailable");
    }

    subscription_ = observer_.subscribe([this](const TreasureStateChanged& event) {
      treasure_state_ = event.current;
      indicator_.set_treasure_state(event.current);
    });

    minimap_subscription_ = minimap_.subscribe([this](const MinimapStateChanged& event) {
      minimap_state_ = event.current;
      indicator_.set_minimap_state(event.current);
    });

    menu_subscription_ = menu_.subscribe([this](const MenuStateChanged& event) {
      menu_state_ = event.current;
      indicator_.set_menu_state(event.current);
    });

    menu_state_ = menu_.state();
    minimap_state_ = minimap_.state();
    treasure_state_ = observer_.state();
    indicator_.set_menu_state(menu_.state());
    indicator_.set_minimap_state(minimap_.state());
    indicator_.set_treasure_state(observer_.state());
    set_overlay_hud(indicator_.hud_state());

    logger_configured_ = true;
    set_overlay_log(log);
    log(LogLevel::debug, "Startup: graphics hooks begin");
    const bool overlay_started = start_overlay();
    exit_.retain |= overlay_started;
    log(
      overlay_started ? LogLevel::info : LogLevel::error,
      overlay_started
        ? "DX12 hooks installed; waiting for swapchain"
        : "DX12 hooks unavailable; overlay disabled"
    );

    log(LogLevel::debug, "Startup: treasure observer begin");
    const bool treasure_started = observer_.start();
    exit_.retain |= treasure_started;
    log(
      LogLevel::debug,
      treasure_started
        ? "Startup: treasure observer ready"
        : "Startup: treasure observer unavailable"
    );
    log(LogLevel::debug, "Startup: minimap observer begin");
    const bool minimap_started = minimap_.start();
    log(
      LogLevel::debug,
      minimap_started ? "Startup: minimap observer ready" : "Startup: minimap observer unavailable"
    );
    log(LogLevel::debug, "Startup: menu observer begin");
    const bool menu_started = menu_.start();
    exit_.retain |= menu_started;
    log(
      LogLevel::debug,
      menu_started ? "Startup: menu observer ready" : "Startup: menu observer unavailable"
    );
    if (config.sound_enabled) {
      log(LogLevel::debug, "Startup: audio volume observer begin");
      const bool audio_started = audio_volume_.start();
      log(
        LogLevel::debug,
        audio_started
          ? "Startup: audio volume observer ready"
          : "Startup: audio volume observer unavailable"
      );
    }

    log(LogLevel::debug, "Startup: polling loop begin");

    for (;;) {
      observer_.poll();
      minimap_.poll();
      menu_.poll();
      if (config.sound_enabled) {
        audio_volume_.poll();
      }
      indicator_.update();

      const auto actions = poll_hotkeys(config.toggle_key, config.unload_key);

      if (actions.toggle) {
        indicator_.toggle();
        enabled_ = !enabled_;
        log(LogLevel::info, enabled_ ? "Mod enabled by hotkey" : "Mod disabled by hotkey");
      }

      set_overlay_hud(indicator_.hud_state());

      if (actions.unload) {
        log(LogLevel::info, "Stop requested by hotkey");
        break;
      }

      const bool sound_allowed = enabled_ &&
        minimap_state_ == MinimapState::visible &&
        menu_state_ == MenuState::closed &&
        game_is_foreground();

      const auto volume = audio_volume_.state();
      const auto volume_percent = sound_volume.poll();
      const float gain = volume.known
        ? static_cast<float>(volume.master_percent * volume.effects_percent) /
          10000.0f *
          (volume_percent / 100.0f)
        : 0.0f;
      if (!sound_allowed || gain == 0.0f) {
        sound_.stop();
      }

      if (sound_policy_.update(treasure_state_, sound_allowed) && gain > 0.0f) {
        sound_.play(gain);
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
    stop_hotkeys();
    const auto observer_stop = observer_.stop();
    exit_.retain |= observer_stop.module_must_remain_loaded || !observer_stop.hooks_disabled;

    minimap_.stop();
    audio_volume_.stop();
    const auto menu_stop = menu_.stop();
    exit_.retain |= menu_stop.module_must_remain_loaded || !menu_stop.hooks_disabled;

    subscription_.reset();
    minimap_subscription_.reset();
    menu_subscription_.reset();
    sound_.stop();

    // Failed starts can leave partially installed hooks; always ask both owners to stop.
    const auto overlay_stop = stop_overlay();
    exit_.retain |= overlay_stop.module_must_remain_loaded ||
      !overlay_stop.hooks_disabled ||
      !overlay_stop.callbacks_drained ||
      !overlay_stop.gpu_resources_released;

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
  TreasureSound sound_policy_;
  SoundPlayer sound_;
  bool enabled_;
  TreasureState treasure_state_ = TreasureState::unknown;
  MinimapState minimap_state_ = MinimapState::unknown;
  MenuState menu_state_ = MenuState::unknown;
  TreasureObserver observer_;
  MinimapObserver minimap_;
  MenuObserver menu_;
  AudioVolumeObserver audio_volume_;
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
      log_environment();
      log_settings(config);
      log(LogLevel::info, ("Game EXE version: " + executable_version()).c_str());
      log(LogLevel::info, ("Process EXE: " + module_name_utf8(nullptr)).c_str());
      log(LogLevel::info, ("Mod ASI: " + module_name_utf8(module)).c_str());

      if (legacy_config) {
        log(
          LogLevel::warn,
          "Using legacy config.ini; rename it to PirateHatHUD.ini before installing an update"
        );
      }

      const auto icon_path = folder + L"PirateHatHUD_treasure.png";
      const bool custom_icon = GetFileAttributesW(icon_path.c_str()) != INVALID_FILE_ATTRIBUTES;

      log(LogLevel::debug, "Startup: icon preparation begin");
      if (!prepare_overlay_icon(custom_icon ? icon_path.c_str() : nullptr)) {
        log(LogLevel::error, "Embedded icon or PirateHatHUD_treasure.png invalid; mod not started");
      } else {
        log(LogLevel::debug, "Startup: icon preparation ready");
        AppSession session(config, exit);
        session.run(config, folder, legacy_config ? legacy_config_path : config_path);
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
