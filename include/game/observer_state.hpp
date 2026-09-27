#pragma once

namespace phi {

enum class TreasureState { unknown, inactive, active };

enum class MinimapState { unknown, hidden, visible };

enum class MenuState { unknown, open, closed };

struct AudioVolumeState {
  bool known = false;
  unsigned master_percent = 0;
  unsigned effects_percent = 0;
  bool operator==(const AudioVolumeState&) const = default;
};

struct AudioVolumeChanged {
  AudioVolumeState previous;
  AudioVolumeState current;
};

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
