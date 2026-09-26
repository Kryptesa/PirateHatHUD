#pragma once
#include <cstdint>
namespace phi {
struct HotkeyActions {
  bool toggle = false;
  bool unload = false;
};
// Owner-thread edge detection; ignore Windows' shared, unreliable "recent press" bit.
class HotkeyPress {
public:
  bool sample(int key, std::int16_t state) {
    if (key != key_) {
      key_ = key;
      down_ = false;
    }
    const bool down = (static_cast<std::uint16_t>(state) & 0x8000u) != 0;
    const bool pressed = down && !down_;
    down_ = down;
    return pressed;
  }

private:
  int key_ = -1;
  bool down_ = false;
};
HotkeyActions poll_hotkeys(int toggle_key, int unload_key);
bool game_is_foreground();
} // namespace phi
