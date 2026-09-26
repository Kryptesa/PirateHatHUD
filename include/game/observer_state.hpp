#pragma once

namespace phi {

enum class TreasureState { unknown, inactive, active };
enum class MinimapState { unknown, hidden, visible };
enum class MenuState { unknown, open, closed };

struct TreasureStateChanged {
  TreasureState previous;
  TreasureState current;
};
struct MinimapStateChanged {
  MinimapState previous;
  MinimapState current;
};
struct MenuStateChanged {
  MenuState previous;
  MenuState current;
};

struct ObserverStopResult {
  bool hooks_disabled;
  bool module_must_remain_loaded;
};

} // namespace phi
