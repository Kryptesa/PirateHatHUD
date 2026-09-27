#pragma once

#include "game/shared/memory_reader.hpp"
#include "game/observer_state.hpp"

namespace phi::detail {
struct AudioVolumeLocation {
  uintptr_t engine_slot = 0;
  uintptr_t image_base = 0;
  size_t image_size = 0;
};

AudioVolumeState sample_audio_volume(const AudioVolumeLocation& location, const MemoryReader& read);
} // namespace phi::detail
