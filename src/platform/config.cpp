#include "platform/config.hpp"
#include <windows.h>

namespace phi {

namespace {
int key_from_name(const wchar_t* value, int fallback) {
  if (_wcsicmp(value, L"F8") == 0) {
    return VK_F8;
  }

  if (_wcsicmp(value, L"F9") == 0) {
    return VK_F9;
  }

  if (_wcsicmp(value, L"F10") == 0) {
    return VK_F10;
  }

  if (_wcsicmp(value, L"F11") == 0) {
    return VK_F11;
  }

  return fallback;
}
} // namespace

Config read_config(const std::wstring& path) {
  const Config defaults;
  Config config = defaults;
  config.x = GetPrivateProfileIntW(L"indicator", L"x", config.x, path.c_str());
  config.y = GetPrivateProfileIntW(L"indicator", L"y", config.y, path.c_str());
  config.scale = GetPrivateProfileIntW(
                   L"indicator",
                   L"scale_percent",
                   static_cast<int>(defaults.scale * 100),
                   path.c_str()
                 ) /
    100.0f;

  if (config.scale < 0.25f || config.scale > 4.0f) {
    config.scale = defaults.scale;
  }

  config.enabled =
    GetPrivateProfileIntW(L"indicator", L"enabled", defaults.enabled, path.c_str()) != 0;
  config.force_show =
    GetPrivateProfileIntW(L"indicator", L"force_show", defaults.force_show, path.c_str()) != 0;
  config.show_delay_ms = static_cast<int>(
    GetPrivateProfileIntW(L"indicator", L"show_delay_ms", defaults.show_delay_ms, path.c_str())
  );

  if (config.show_delay_ms < 0 || config.show_delay_ms > 60000) {
    config.show_delay_ms = defaults.show_delay_ms;
  }

  config.sound_enabled =
    GetPrivateProfileIntW(L"sound", L"enabled", defaults.sound_enabled, path.c_str()) != 0;
  config.sound_cooldown_ms = static_cast<int>(
    GetPrivateProfileIntW(L"sound", L"cooldown_ms", defaults.sound_cooldown_ms, path.c_str())
  );

  if (config.sound_cooldown_ms < 0 || config.sound_cooldown_ms > 60000) {
    config.sound_cooldown_ms = defaults.sound_cooldown_ms;
  }

  wchar_t key[16]{};
  GetPrivateProfileStringW(L"hotkeys", L"toggle", L"", key, 16, path.c_str());
  config.toggle_key = key_from_name(key, config.toggle_key);
  GetPrivateProfileStringW(L"hotkeys", L"unload", L"", key, 16, path.c_str());
  config.unload_key = key_from_name(key, config.unload_key);
  wchar_t level[32]{};
  GetPrivateProfileStringW(L"logging", L"level", L"info", level, 32, path.c_str());
  constexpr const wchar_t* levels[] = {L"trace", L"debug", L"info", L"warn", L"error", L"off"};

  for (int i = 0; i < 6; ++i) {
    if (_wcsicmp(level, levels[i]) == 0) {
      config.logging.level = static_cast<LogLevel>(i);
    }
  }

  const auto size = GetPrivateProfileIntW(L"logging", L"max_file_size_mb", 5, path.c_str());

  if (size >= 1 && size <= 100) {
    config.logging.max_file_size = static_cast<std::size_t>(size) * 1024 * 1024;
  }

  const auto count = GetPrivateProfileIntW(L"logging", L"max_files", 3, path.c_str());

  if (count >= 1 && count <= 20) {
    config.logging.max_files = count;
  }

  return config;
}
} // namespace phi
