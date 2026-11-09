#include "platform/hotkeys.hpp"
#define CHECK(c)                                                                                   \
  do {                                                                                             \
    if (!(c)) {                                                                                    \
      return __LINE__;                                                                             \
    }                                                                                              \
  } while (false)
int main() {
  phi::HotkeyPress toggle;
  phi::HotkeyPress unload;
  constexpr phi::HotkeyBinding f9{0x78}, f10{0x79};
  constexpr auto held = static_cast<std::int16_t>(-32768);
  CHECK(!toggle.sample(f9, 0));
  CHECK(!toggle.sample(f9, 1));   // Stale low bit without a held key is ignored.
  CHECK(toggle.sample(f9, held)); // Works when another caller consumed the low bit.
  for (int i = 0; i < 100; ++i) {
    CHECK(!toggle.sample(f9, held)); // Holding never repeats.
  }

  CHECK(unload.sample(f10, held)); // Independent action state.
  CHECK(!unload.sample(f10, held));
  CHECK(!toggle.sample(f9, 0));
  CHECK(toggle.sample(f9, static_cast<std::int16_t>(-32767))); // Both bits set.
  CHECK(!toggle.sample(f9, held));
  CHECK(!toggle.sample(f9, 0));
  CHECK(toggle.sample(f9, held)); // Release and repress.
  CHECK(!unload.sample(f10, 0));
  CHECK(unload.sample(f10, held));
  CHECK(!toggle.sample({0x77}, 0)); // A new binding gets independent initial state.
  CHECK(toggle.sample({0x77}, held));

  CHECK(!toggle.sample({}, held)); // Disabled never triggers, even with a held state.
  CHECK(!toggle.sample({}, held, phi::kHotkeyCtrl));
  CHECK(toggle.sample(f9, held)); // Re-enabling resets the edge state.

  const phi::HotkeyBinding ctrl_k{'K', phi::kHotkeyCtrl};
  CHECK(!toggle.sample(ctrl_k, held)); // Pressing K without Ctrl consumes the key edge.
  CHECK(!toggle.sample(ctrl_k, held, phi::kHotkeyCtrl)); // Adding Ctrl cannot retrigger K.
  CHECK(!toggle.sample(ctrl_k, 0, phi::kHotkeyCtrl));
  CHECK(toggle.sample(ctrl_k, held, phi::kHotkeyCtrl));
  CHECK(!toggle.sample(ctrl_k, held, phi::kHotkeyCtrl));
  CHECK(!toggle.sample(ctrl_k, 0));
  CHECK(!toggle.sample(ctrl_k, held, phi::kHotkeyCtrl | phi::kHotkeyShift));
  CHECK(!toggle.sample(ctrl_k, 0));
  CHECK(!toggle.sample(ctrl_k, held, phi::kHotkeyAlt));
  CHECK(!toggle.sample({'K'}, held, phi::kHotkeyCtrl)); // Bare keys reject extra modifiers.
  CHECK(!toggle.sample({'K'}, 0));
  CHECK(toggle.sample({'K'}, held));
  CHECK(toggle.sample({0xA2}, held, phi::kHotkeyCtrl)); // Standalone left Ctrl.
  CHECK(!toggle.sample({0xA2}, held, phi::kHotkeyCtrl));
  CHECK(!toggle.sample({0xA2, phi::kHotkeyShift}, 0));
  CHECK(toggle.sample({0xA2, phi::kHotkeyShift}, held, phi::kHotkeyCtrl | phi::kHotkeyShift));
  CHECK(toggle.sample({0x05, phi::kHotkeyAlt}, held, phi::kHotkeyAlt)); // Mouse4.

  struct Case {
    std::wstring_view text;
    phi::HotkeyBinding expected;
  };
  for (const auto& test : {
         Case{L"", {}},
         Case{L" \t ", {}},
         Case{L"None", {}},
         Case{L"oFf", {}},
         Case{L"disabled", {}},
         Case{L"f1", {0x70}},
         Case{L"F8", {0x77}},
         Case{L"F9", f9},
         Case{L"F10", f10},
         Case{L"F12", {0x7B}},
         Case{L"F24", {0x87}},
         Case{L"k", {'K'}},
         Case{L"0", {'0'}},
         Case{L"9", {'9'}},
         Case{L"Numpad0", {0x60}},
         Case{L"Numpad9", {0x69}},
         Case{L"ADD", {0x6B}},
         Case{L"Esc", {0x1B}},
         Case{L"Return", {0x0D}},
         Case{L"PgUp", {0x21}},
         Case{L"PageDown", {0x22}},
         Case{L"Space", {0x20}},
         Case{L"Insert", {0x2D}},
         Case{L"Delete", {0x2E}},
         Case{L"Home", {0x24}},
         Case{L"End", {0x23}},
         Case{L"Left", {0x25}},
         Case{L"PrintScreen", {0x2C}},
         Case{L"CapsLock", {0x14}},
         Case{L"OEM_3", {0xC0}},
         Case{L"VK_OEM_PLUS", {0xBB}},
         Case{L"MEDIA_PLAY_PAUSE", {0xB3}},
         Case{L"VK_VOLUME_MUTE", {0xAD}},
         Case{L"Ctrl", {0x11}},
         Case{L"RCtrl", {0xA3}},
         Case{L"Alt", {0x12}},
         Case{L"LShift", {0xA0}},
         Case{L"RWin", {0x5C}},
         Case{L"Mouse1", {0x01}},
         Case{L"Mouse5", {0x06}},
         Case{L"VK_XBUTTON1", {0x05}},
         Case{L"  Ctrl + k  ", ctrl_k},
         Case{L"Alt+F4", {0x73, phi::kHotkeyAlt}},
         Case{L"Shift+Mouse4", {0x05, phi::kHotkeyShift}},
         Case{L"Win+Home", {0x24, phi::kHotkeyWin}},
         Case{L"Control+Alt+Shift+Win+VK_F24", {0x87, 0xF}},
         Case{L"0xBA", {0xBA}},
         Case{L"Ctrl+0xfe", {0xFE, phi::kHotkeyCtrl}},
       }) {
    const auto parsed = phi::parse_hotkey(test.text);
    CHECK(parsed && *parsed == test.expected);
  }
  for (const auto* text : {
         L"F0",
         L"F25",
         L"F-1",
         L"F:1",
         L"F1junk",
         L"Numpad10",
         L"Banana",
         L"Ctrl+",
         L"+K",
         L"Ctrl++K",
         L"K+Ctrl",
         L"Ctrl+Ctrl+K",
         L"Ctrl+Control+K",
         L"Ctrl+Ctrl",
         L"Shift+LShift",
         L"Alt+None",
         L"Ctrl+K+F9",
         L"0x",
         L"0x00",
         L"0xFF",
         L"0x100",
         L"0xFFFFFFFFFFFFFFFF",
         L"0x:G",
         L"0x1z",
         L"0x-1",
         L"++",
       }) {
    CHECK(!phi::parse_hotkey(text));
  }
}
