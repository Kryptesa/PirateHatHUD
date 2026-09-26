#pragma once
#include <string>
namespace phi {
struct Config {
  bool enabled = true;
  bool force_show = false;
  int x = 350;
  int y = -310;
  float scale = 1.0f;
  int toggle_key = 0x78; // F9
  int unload_key = 0x79; // F10
};
Config read_config(const std::wstring& path);
} // namespace phi
