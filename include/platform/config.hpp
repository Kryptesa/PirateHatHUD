#pragma once

#include "platform/logger.hpp"
#include <string>
#include <chrono>
#include <cstdint>
#include <filesystem>

namespace phi {
struct Config {
  LogConfig logging;
  bool enabled = true;
  bool force_show = false;
  int show_delay_ms = 1000;
  bool sound_enabled = true;
  int sound_cooldown_ms = 1000;
  int sound_volume_percent = 100;
  int x = 350;
  int y = -310;
  float scale = 1.0f;
  int toggle_key = 0x78; // F9
  int unload_key = 0x79; // F10
};

Config read_config(const std::wstring& path);

// Owner-thread polling; only notification volume is reloaded.
class SoundVolumeConfig {
public:
  SoundVolumeConfig(std::wstring path, int initial_volume);
  int poll(std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());

private:
  std::wstring path_;
  int volume_;
  std::chrono::steady_clock::time_point next_check_{};
  std::filesystem::file_time_type modified_{};
  uintmax_t size_ = 0;
  bool observed_ = false;
};
} // namespace phi
