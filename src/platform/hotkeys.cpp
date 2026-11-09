#include "platform/hotkeys.hpp"
#include "platform/logger.hpp"
#include <windows.h>

namespace phi {
namespace {
struct Registration {
  ATOM toggle_id = 0;
  ATOM unload_id = 0;
  HotkeyBinding toggle_binding;
  HotkeyBinding unload_binding;
  bool active = false;
  bool toggle_registered = false;
  bool unload_registered = false;
  HotkeyPress toggle;
  HotkeyPress unload;
};

Registration& registration() {
  static thread_local Registration state;
  return state;
}

bool uses_polling(HotkeyBinding binding) {
  return binding.key == VK_F12 ||
    hotkey_key_modifier(binding.key) != 0 ||
    binding.key == VK_LBUTTON ||
    binding.key == VK_RBUTTON ||
    binding.key == VK_MBUTTON ||
    binding.key == VK_XBUTTON1 ||
    binding.key == VK_XBUTTON2;
}

bool register_binding(HotkeyBinding binding, const wchar_t* name, ATOM& id) {
  if (binding.key == 0 || uses_polling(binding)) {
    return false;
  }
  id = GlobalAddAtomW(name);
  return id && RegisterHotKey(nullptr, id, MOD_NOREPEAT | binding.modifiers, binding.key);
}

unsigned held_modifiers() {
  const auto down = [](int key) { return (GetAsyncKeyState(key) & 0x8000) != 0; };
  unsigned modifiers = 0;
  if (down(VK_MENU)) {
    modifiers |= kHotkeyAlt;
  }
  if (down(VK_CONTROL)) {
    modifiers |= kHotkeyCtrl;
  }
  if (down(VK_SHIFT)) {
    modifiers |= kHotkeyShift;
  }
  if (down(VK_LWIN) || down(VK_RWIN)) {
    modifiers |= kHotkeyWin;
  }
  return modifiers;
}

void drain_messages(Registration& state, HotkeyActions* actions) {
  MSG message{};
  while (PeekMessageW(&message, nullptr, WM_HOTKEY, WM_HOTKEY, PM_REMOVE)) {
    if (actions) {
      actions->toggle |= state.toggle_registered && message.wParam == state.toggle_id;
      actions->unload |= state.unload_registered && message.wParam == state.unload_id;
    }
  }
}
} // namespace

void stop_hotkeys() noexcept {
  auto& state = registration();
  if (state.toggle_registered) {
    UnregisterHotKey(nullptr, state.toggle_id);
  }
  if (state.unload_registered) {
    UnregisterHotKey(nullptr, state.unload_id);
  }
  drain_messages(state, nullptr);
  if (state.toggle_id) {
    GlobalDeleteAtom(state.toggle_id);
  }
  if (state.unload_id) {
    GlobalDeleteAtom(state.unload_id);
  }
  state = {};
}

bool game_is_foreground() {
  const auto window = GetForegroundWindow();
  DWORD process_id = 0;

  return window &&
    GetWindowThreadProcessId(window, &process_id) &&
    process_id == GetCurrentProcessId();
}

HotkeyActions poll_hotkeys(HotkeyBinding toggle, HotkeyBinding unload) {
  auto& state = registration();
  if (!game_is_foreground()) {
    if (state.active) {
      stop_hotkeys();
    }
    return {};
  }

  if (!state.active || state.toggle_binding != toggle || state.unload_binding != unload) {
    stop_hotkeys();
    state.active = true;
    state.toggle_binding = toggle;
    state.unload_binding = unload;
    state.toggle_registered = register_binding(toggle, L"PirateHatHUD.Toggle", state.toggle_id);
    state.unload_registered = register_binding(unload, L"PirateHatHUD.Stop", state.unload_id);
    const bool fallback = (toggle.key && !uses_polling(toggle) && !state.toggle_registered) ||
      (unload.key && !uses_polling(unload) && !state.unload_registered);
    log(
      fallback ? LogLevel::warn : LogLevel::debug,
      fallback
        ? "Windows hotkey registration incomplete; using key-state polling for unavailable bindings"
        : "Hotkeys configured while game is focused; disabled bindings skipped"
    );
  }

  HotkeyActions actions;
  drain_messages(state, &actions);
  const bool poll_toggle = toggle.key != 0 && !state.toggle_registered;
  const bool poll_unload = unload.key != 0 && !state.unload_registered;
  const auto modifiers = poll_toggle || poll_unload ? held_modifiers() : 0;
  if (poll_toggle) {
    actions.toggle = state.toggle.sample(toggle, GetAsyncKeyState(toggle.key), modifiers);
  }
  if (poll_unload) {
    actions.unload = state.unload.sample(unload, GetAsyncKeyState(unload.key), modifiers);
  }
  return actions;
}
} // namespace phi
