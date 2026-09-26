#include "platform/hotkeys.hpp"
#include <windows.h>
namespace phi {
HotkeyActions poll_hotkeys(int toggle_key, int unload_key) {
  static HotkeyPress toggle;
  static HotkeyPress unload;
  return {toggle.sample(toggle_key, GetAsyncKeyState(toggle_key)),
          unload.sample(unload_key, GetAsyncKeyState(unload_key))};
}
} // namespace phi
