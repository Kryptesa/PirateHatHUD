#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace phi::detail {
struct PcmWave {
  size_t data_offset = 0;
  size_t data_size = 0;
  uint16_t bits = 0;
};

std::optional<PcmWave> parse_pcm_wave(std::span<const uint8_t> bytes);
bool scale_pcm_wave(std::span<uint8_t> bytes, const PcmWave& format, float gain) noexcept;
} // namespace phi::detail
