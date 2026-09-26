#pragma once
namespace phi {
struct HotkeyActions {
  bool toggle = false;
  bool unload = false;
};
HotkeyActions poll_hotkeys(int toggle_key, int unload_key);
} // namespace phi
