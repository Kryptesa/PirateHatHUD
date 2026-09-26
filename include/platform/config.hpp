#pragma once
#include "platform/logger.hpp"
#include <string>
namespace phi {
struct Config {
  LogConfig logging;
  bool enabled = true;
  bool force_show = false;
  int show_delay_ms = 1000;
  int x = 350;
  int y = -310;
  float scale = 1.0f;
  int toggle_key = 0x78; // F9
  int unload_key = 0x79; // F10
};
Config read_config(const std::wstring& path);
} // namespace phi
