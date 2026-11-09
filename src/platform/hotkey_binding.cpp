#include "platform/hotkeys.hpp"
#include <windows.h>
#include <string>

namespace phi {
namespace {
std::wstring_view trim(std::wstring_view text) {
  const auto first = text.find_first_not_of(L" \t\r\n");
  if (first == std::wstring_view::npos) {
    return {};
  }
  return text.substr(first, text.find_last_not_of(L" \t\r\n") - first + 1);
}

std::optional<int> number(std::wstring_view text, int base) {
  if (text.empty()) {
    return {};
  }
  int value = 0;
  for (const auto c : text) {
    if (!((c >= L'0' && c <= L'9') || (c >= L'A' && c <= L'F'))) {
      return {};
    }
    const int digit = c >= L'0' && c <= L'9' ? c - L'0' : c - L'A' + 10;
    if (digit < 0 || digit >= base || value > (0xFE - digit) / base) {
      return {};
    }
    value = value * base + digit;
  }
  return value;
}

std::optional<int> key_from_name(std::wstring_view name) {
  if (name.starts_with(L"VK_")) {
    name.remove_prefix(3);
  }
  if (name.size() == 1) {
    const auto c = name.front();
    if ((c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9')) {
      return c;
    }
  }
  if (name.starts_with(L"0X")) {
    const auto value = number(name.substr(2), 16);
    return value && *value != 0 ? value : std::nullopt;
  }
  if (name.starts_with(L"F")) {
    const auto value = number(name.substr(1), 10);
    if (value && *value >= 1 && *value <= 24) {
      return VK_F1 + *value - 1;
    }
  }
  if (name.size() == 7 && name.starts_with(L"NUMPAD") && name[6] >= L'0' && name[6] <= L'9') {
    return VK_NUMPAD0 + name[6] - L'0';
  }
  struct NamedKey {
    std::wstring_view name;
    int key;
  };
  static constexpr NamedKey keys[] = {
    {L"BACKSPACE", VK_BACK},
    {L"BACK", VK_BACK},
    {L"TAB", VK_TAB},
    {L"CLEAR", VK_CLEAR},
    {L"ENTER", VK_RETURN},
    {L"RETURN", VK_RETURN},
    {L"SHIFT", VK_SHIFT},
    {L"CTRL", VK_CONTROL},
    {L"CONTROL", VK_CONTROL},
    {L"ALT", VK_MENU},
    {L"MENU", VK_MENU},
    {L"LSHIFT", VK_LSHIFT},
    {L"RSHIFT", VK_RSHIFT},
    {L"LCTRL", VK_LCONTROL},
    {L"RCTRL", VK_RCONTROL},
    {L"LCONTROL", VK_LCONTROL},
    {L"RCONTROL", VK_RCONTROL},
    {L"LALT", VK_LMENU},
    {L"RALT", VK_RMENU},
    {L"LMENU", VK_LMENU},
    {L"RMENU", VK_RMENU},
    {L"WIN", VK_LWIN},
    {L"LWIN", VK_LWIN},
    {L"RWIN", VK_RWIN},
    {L"APPS", VK_APPS},
    {L"PAUSE", VK_PAUSE},
    {L"CAPSLOCK", VK_CAPITAL},
    {L"CAPITAL", VK_CAPITAL},
    {L"ESC", VK_ESCAPE},
    {L"ESCAPE", VK_ESCAPE},
    {L"SPACE", VK_SPACE},
    {L"PAGEUP", VK_PRIOR},
    {L"PGUP", VK_PRIOR},
    {L"PRIOR", VK_PRIOR},
    {L"PAGEDOWN", VK_NEXT},
    {L"PGDN", VK_NEXT},
    {L"NEXT", VK_NEXT},
    {L"END", VK_END},
    {L"HOME", VK_HOME},
    {L"LEFT", VK_LEFT},
    {L"UP", VK_UP},
    {L"RIGHT", VK_RIGHT},
    {L"DOWN", VK_DOWN},
    {L"PRINTSCREEN", VK_SNAPSHOT},
    {L"SNAPSHOT", VK_SNAPSHOT},
    {L"INSERT", VK_INSERT},
    {L"INS", VK_INSERT},
    {L"DELETE", VK_DELETE},
    {L"DEL", VK_DELETE},
    {L"NUMLOCK", VK_NUMLOCK},
    {L"SCROLLLOCK", VK_SCROLL},
    {L"SCROLL", VK_SCROLL},
    {L"MULTIPLY", VK_MULTIPLY},
    {L"ADD", VK_ADD},
    {L"SEPARATOR", VK_SEPARATOR},
    {L"SUBTRACT", VK_SUBTRACT},
    {L"DECIMAL", VK_DECIMAL},
    {L"DIVIDE", VK_DIVIDE},
    {L"OEM_1", VK_OEM_1},
    {L"OEM_PLUS", VK_OEM_PLUS},
    {L"OEM_COMMA", VK_OEM_COMMA},
    {L"OEM_MINUS", VK_OEM_MINUS},
    {L"OEM_PERIOD", VK_OEM_PERIOD},
    {L"OEM_2", VK_OEM_2},
    {L"OEM_3", VK_OEM_3},
    {L"OEM_4", VK_OEM_4},
    {L"OEM_5", VK_OEM_5},
    {L"OEM_6", VK_OEM_6},
    {L"OEM_7", VK_OEM_7},
    {L"OEM_8", VK_OEM_8},
    {L"OEM_102", VK_OEM_102},
    {L"VOLUME_MUTE", VK_VOLUME_MUTE},
    {L"VOLUME_DOWN", VK_VOLUME_DOWN},
    {L"VOLUME_UP", VK_VOLUME_UP},
    {L"MEDIA_NEXT_TRACK", VK_MEDIA_NEXT_TRACK},
    {L"MEDIA_PREV_TRACK", VK_MEDIA_PREV_TRACK},
    {L"MEDIA_STOP", VK_MEDIA_STOP},
    {L"MEDIA_PLAY_PAUSE", VK_MEDIA_PLAY_PAUSE},
    {L"MOUSE1", VK_LBUTTON},
    {L"LBUTTON", VK_LBUTTON},
    {L"MOUSE2", VK_RBUTTON},
    {L"RBUTTON", VK_RBUTTON},
    {L"MOUSE3", VK_MBUTTON},
    {L"MBUTTON", VK_MBUTTON},
    {L"MOUSE4", VK_XBUTTON1},
    {L"XBUTTON1", VK_XBUTTON1},
    {L"MOUSE5", VK_XBUTTON2},
    {L"XBUTTON2", VK_XBUTTON2},
  };
  for (const auto& entry : keys) {
    if (entry.name == name) {
      return entry.key;
    }
  }
  return {};
}
} // namespace

std::optional<HotkeyBinding> parse_hotkey(std::wstring_view text) {
  std::wstring upper(trim(text));
  for (auto& c : upper) {
    if (c >= L'a' && c <= L'z') {
      c -= L'a' - L'A';
    }
  }
  text = upper;
  if (text.empty() || text == L"NONE" || text == L"OFF" || text == L"DISABLED") {
    return HotkeyBinding{};
  }
  unsigned modifiers = 0;
  for (auto separator = text.find(L'+'); separator != text.npos; separator = text.find(L'+')) {
    const auto token = trim(text.substr(0, separator));
    unsigned modifier = 0;
    if (token == L"CTRL" || token == L"CONTROL") {
      modifier = kHotkeyCtrl;
    } else if (token == L"ALT") {
      modifier = kHotkeyAlt;
    } else if (token == L"SHIFT") {
      modifier = kHotkeyShift;
    } else if (token == L"WIN") {
      modifier = kHotkeyWin;
    }
    if (modifier == 0 || (modifiers & modifier) != 0) {
      return {};
    }
    modifiers |= modifier;
    text.remove_prefix(separator + 1);
  }
  const auto key = key_from_name(trim(text));
  if (!key || (hotkey_key_modifier(*key) & modifiers) != 0) {
    return {};
  }
  return HotkeyBinding{*key, modifiers};
}
} // namespace phi
