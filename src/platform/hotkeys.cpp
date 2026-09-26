#include "platform/hotkeys.hpp"
#include <windows.h>
namespace phi {
HotkeyActions poll_hotkeys(int toggle_key, int unload_key) {
  return {(GetAsyncKeyState(toggle_key) & 1) != 0, (GetAsyncKeyState(unload_key) & 1) != 0};
}
} // namespace phi
