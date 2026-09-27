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
  constexpr int f9 = 0x78, f10 = 0x79;
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
  CHECK(!toggle.sample(0x77, 0)); // A new binding gets independent initial state.
  CHECK(toggle.sample(0x77, held));
}
