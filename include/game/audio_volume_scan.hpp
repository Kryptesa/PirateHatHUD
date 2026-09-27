#pragma once

#include "pattern_scan.hpp"
#include "game/audio_volume_memory.hpp"

namespace phi {
PatternMatch scan_audio_volume_code(std::span<const uint8_t> code, uintptr_t base);
detail::AudioVolumeLocation find_audio_volume_location(HMODULE game);
} // namespace phi
