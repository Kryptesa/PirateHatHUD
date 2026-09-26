#include "platform/hotkeys.hpp"
#include <windows.h>
namespace phi {
bool game_is_foreground() {
  const auto window = GetForegroundWindow();
  DWORD process_id = 0;
  return window && GetWindowThreadProcessId(window, &process_id) &&
         process_id == GetCurrentProcessId();
}
HotkeyActions poll_hotkeys(int toggle_key, int unload_key) {
  static HotkeyPress toggle;
  static HotkeyPress unload;
  return {toggle.sample(toggle_key, GetAsyncKeyState(toggle_key)),
          unload.sample(unload_key, GetAsyncKeyState(unload_key))};
}
} // namespace phi
