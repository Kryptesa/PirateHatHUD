#include "platform/hotkeys.hpp"
#include "platform/logger.hpp"
#include <windows.h>

namespace phi {
namespace {
struct Registration {
  ATOM toggle_id = 0;
  ATOM unload_id = 0;
  int toggle_key = -1;
  int unload_key = -1;
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

HotkeyActions poll_hotkeys(int toggle_key, int unload_key) {
  auto& state = registration();
  if (!game_is_foreground()) {
    if (state.active) {
      stop_hotkeys();
    }
    return {};
  }

  if (!state.active || state.toggle_key != toggle_key || state.unload_key != unload_key) {
    stop_hotkeys();
    state.active = true;
    state.toggle_key = toggle_key;
    state.unload_key = unload_key;
    state.toggle_id = GlobalAddAtomW(L"PirateHatHUD.Toggle");
    state.unload_id = GlobalAddAtomW(L"PirateHatHUD.Stop");
    state.toggle_registered =
      state.toggle_id && RegisterHotKey(nullptr, state.toggle_id, MOD_NOREPEAT, toggle_key);
    state.unload_registered =
      state.unload_id && RegisterHotKey(nullptr, state.unload_id, MOD_NOREPEAT, unload_key);
    log(
      state.toggle_registered && state.unload_registered ? LogLevel::debug : LogLevel::warn,
      state.toggle_registered && state.unload_registered
        ? "Windows hotkeys registered while game is focused"
        : "Windows hotkey registration incomplete; using key-state polling for unavailable bindings"
    );
  }

  HotkeyActions actions;
  drain_messages(state, &actions);
  if (!state.toggle_registered) {
    actions.toggle = state.toggle.sample(toggle_key, GetAsyncKeyState(toggle_key));
  }
  if (!state.unload_registered) {
    actions.unload = state.unload.sample(unload_key, GetAsyncKeyState(unload_key));
  }
  return actions;
}
} // namespace phi
