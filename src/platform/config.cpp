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
  config.scale = GetPrivateProfileIntW(L"indicator", L"scale_percent",
                                       static_cast<int>(defaults.scale * 100), path.c_str()) /
                 100.0f;
  if (config.scale < 0.25f || config.scale > 4.0f) {
    config.scale = defaults.scale;
  }
  config.enabled =
      GetPrivateProfileIntW(L"indicator", L"enabled", defaults.enabled, path.c_str()) != 0;
  config.force_show =
      GetPrivateProfileIntW(L"indicator", L"force_show", defaults.force_show, path.c_str()) != 0;
  wchar_t key[16]{};
  GetPrivateProfileStringW(L"hotkeys", L"toggle", L"", key, 16, path.c_str());
  config.toggle_key = key_from_name(key, config.toggle_key);
  GetPrivateProfileStringW(L"hotkeys", L"unload", L"", key, 16, path.c_str());
  config.unload_key = key_from_name(key, config.unload_key);
  return config;
}
} // namespace phi
