#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace phi {
// Modifier bits match Windows MOD_* values without exposing Windows headers.
inline constexpr unsigned kHotkeyAlt = 0x1;
inline constexpr unsigned kHotkeyCtrl = 0x2;
inline constexpr unsigned kHotkeyShift = 0x4;
inline constexpr unsigned kHotkeyWin = 0x8;

struct HotkeyBinding {
  int key = 0; // Zero disables the binding.
  unsigned modifiers = 0;
  bool operator==(const HotkeyBinding&) const = default;
};

// Empty/None/Off disables; malformed input returns nullopt.
std::optional<HotkeyBinding> parse_hotkey(std::wstring_view text);

constexpr unsigned hotkey_key_modifier(int key) {
  switch (key) {
  case 0x10: // Shift, left Shift, right Shift.
  case 0xA0:
  case 0xA1:
    return kHotkeyShift;
  case 0x11: // Ctrl, left Ctrl, right Ctrl.
  case 0xA2:
  case 0xA3:
    return kHotkeyCtrl;
  case 0x12: // Alt, left Alt, right Alt.
  case 0xA4:
  case 0xA5:
    return kHotkeyAlt;
  case 0x5B: // Left Win, right Win.
  case 0x5C:
    return kHotkeyWin;
  default:
    return 0;
  }
}

struct HotkeyActions {
  bool toggle = false;
  bool unload = false;
};

// Owner-thread edge detection; ignore Windows' shared, unreliable "recent press" bit.
class HotkeyPress {
public:
  bool sample(HotkeyBinding binding, std::int16_t state, unsigned modifiers = 0) {
    if (binding != binding_) {
      binding_ = binding;
      down_ = false;
    }

    const bool down = binding.key != 0 && (static_cast<std::uint16_t>(state) & 0x8000u) != 0;
    const bool pressed =
      down && !down_ && (modifiers & ~hotkey_key_modifier(binding.key)) == binding.modifiers;
    down_ = down;

    return pressed;
  }

private:
  HotkeyBinding binding_;
  bool down_ = false;
};

HotkeyActions poll_hotkeys(HotkeyBinding toggle, HotkeyBinding unload);
// Call on the polling thread before shutdown, including exceptional exits.
void stop_hotkeys() noexcept;

bool game_is_foreground();
} // namespace phi
