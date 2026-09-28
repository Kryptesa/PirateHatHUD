#include "platform/config.hpp"
#include <windows.h>
#include <cwchar>
#include <cerrno>
#include <utility>
#include <cmath>
#include <optional>

namespace phi {

namespace {
std::optional<float> read_treasure_radius(const std::wstring& path) {
  wchar_t value[64]{};
  const auto length =
    GetPrivateProfileStringW(L"treasure", L"radius", L"", value, 64, path.c_str());
  wchar_t* end = nullptr;
  errno = 0;
  const auto parsed = std::wcstof(value, &end);
  if (
    length == 0 ||
    length >= 63 ||
    end == value ||
    *end ||
    errno == ERANGE ||
    !std::isfinite(parsed) ||
    parsed < 1 ||
    parsed > 1000
  ) {
    return {};
  }
  return parsed;
}

std::optional<TreasureRangeSettings>
read_treasure_range(const std::wstring& path, TreasureRangeSettings previous) {
  wchar_t enabled[8]{};
  const auto length =
    GetPrivateProfileStringW(L"treasure", L"enabled", L"", enabled, 8, path.c_str());
  if (length != 1 || (enabled[0] != L'0' && enabled[0] != L'1')) {
    return {};
  }
  const bool active = enabled[0] == L'1';
  const auto radius = read_treasure_radius(path);
  if (active && !radius) {
    return {};
  }
  // Startup-disabled sessions do not need a valid override radius.
  return TreasureRangeSettings{active, radius.value_or(previous.radius)};
}

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
  config.treasure_range =
    read_treasure_range(path, defaults.treasure_range).value_or(defaults.treasure_range);
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

  config.sound_volume_percent = static_cast<int>(
    GetPrivateProfileIntW(L"sound", L"volume_percent", defaults.sound_volume_percent, path.c_str())
  );
  if (config.sound_volume_percent < 0 || config.sound_volume_percent > 100) {
    config.sound_volume_percent = defaults.sound_volume_percent;
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

SoundVolumeConfig::SoundVolumeConfig(std::wstring path, int initial_volume)
  : path_(std::move(path)),
    volume_(initial_volume) {}

int SoundVolumeConfig::poll(std::chrono::steady_clock::time_point now) {
  if (now < next_check_) {
    return volume_;
  }
  next_check_ = now + std::chrono::seconds(1);

  std::error_code error;
  const auto modified = std::filesystem::last_write_time(path_, error);
  if (error) {
    return volume_;
  }
  const auto size = std::filesystem::file_size(path_, error);
  if (error || (observed_ && modified == modified_ && size == size_)) {
    return volume_;
  }

  wchar_t value[32]{};
  const auto length =
    GetPrivateProfileStringW(L"sound", L"volume_percent", L"", value, 32, path_.c_str());
  wchar_t* end = nullptr;
  errno = 0;
  const auto parsed = std::wcstol(value, &end, 10);
  if (
    length == 0 ||
    length >= 31 ||
    end == value ||
    *end ||
    errno == ERANGE ||
    parsed < 0 ||
    parsed > 100
  ) {
    // Retry on the next check: editors may temporarily truncate or replace the file.
    return volume_;
  }
  volume_ = static_cast<int>(parsed);
  modified_ = modified;
  size_ = size;
  observed_ = true;
  return volume_;
}

TreasureRadiusConfig::TreasureRadiusConfig(std::wstring path, float initial_radius)
  : path_(std::move(path)),
    radius_(initial_radius) {}

float TreasureRadiusConfig::poll(std::chrono::steady_clock::time_point now) {
  if (now < next_check_) {
    return radius_;
  }
  next_check_ = now + std::chrono::seconds(1);
  std::error_code error;
  const auto modified = std::filesystem::last_write_time(path_, error);
  if (error) {
    return radius_;
  }
  const auto size = std::filesystem::file_size(path_, error);
  if (error || (observed_ && modified == modified_ && size == size_)) {
    return radius_;
  }
  const auto parsed = read_treasure_radius(path_);
  if (!parsed) {
    return radius_; // Keep the last valid radius during incomplete saves or invalid edits.
  }
  radius_ = *parsed;
  modified_ = modified;
  size_ = size;
  observed_ = true;
  return radius_;
}
} // namespace phi
