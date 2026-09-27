#include "platform/wave_pcm.hpp"
#include <cmath>
#include <cstring>

namespace phi::detail {
namespace {
std::uint16_t read_u16(std::span<const std::uint8_t> bytes, std::size_t offset) {
  return static_cast<std::uint16_t>(bytes[offset] | (bytes[offset + 1] << 8));
}

std::uint32_t read_u32(std::span<const std::uint8_t> bytes, std::size_t offset) {
  return static_cast<std::uint32_t>(bytes[offset]) |
    (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
    (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
    (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

} // namespace

std::optional<PcmWave> parse_pcm_wave(std::span<const std::uint8_t> bytes) {
  if (
    bytes.size() < 12 ||
    std::memcmp(bytes.data(), "RIFF", 4) != 0 ||
    std::memcmp(bytes.data() + 8, "WAVE", 4) != 0 ||
    read_u32(bytes, 4) != bytes.size() - 8
  ) {
    return std::nullopt;
  }

  PcmWave layout;
  bool format = false;
  bool data = false;
  std::uint16_t alignment = 0;

  for (std::size_t offset = 12; offset < bytes.size();) {
    if (bytes.size() - offset < 8) {
      return std::nullopt;
    }

    const auto size = read_u32(bytes, offset + 4);
    const auto body = offset + 8;

    if (size > bytes.size() - body) {
      return std::nullopt;
    }

    if (std::memcmp(bytes.data() + offset, "fmt ", 4) == 0) {
      if (format || data || size < 16 || read_u16(bytes, body) != 1) {
        return std::nullopt;
      }

      const auto channels = read_u16(bytes, body + 2);
      const auto rate = read_u32(bytes, body + 4);
      const auto bits = read_u16(bytes, body + 14);
      alignment = read_u16(bytes, body + 12);

      if (
        (channels != 1 && channels != 2) ||
        rate < 8000 ||
        rate > 192000 ||
        (bits != 8 && bits != 16) ||
        alignment != channels * (bits / 8) ||
        read_u32(bytes, body + 8) != rate * alignment
      ) {
        return std::nullopt;
      }

      layout.bits = bits;
      format = true;
    } else if (std::memcmp(bytes.data() + offset, "data", 4) == 0) {
      if (!format || data || size == 0 || size % alignment != 0) {
        return std::nullopt;
      }

      layout.data_offset = body;
      layout.data_size = size;
      data = true;
    }

    const std::size_t padded = static_cast<std::size_t>(size) + (size & 1);

    if (padded > bytes.size() - body) {
      return std::nullopt;
    }

    offset = body + padded;
  }

  return format && data ? std::optional<PcmWave>{layout} : std::nullopt;
}

bool scale_pcm_wave(std::span<uint8_t> bytes, const PcmWave& format, float gain) noexcept {
  if (
    !std::isfinite(gain) ||
    gain < 0 ||
    gain > 1 ||
    (format.bits != 8 && format.bits != 16) ||
    format.data_offset > bytes.size() ||
    format.data_size > bytes.size() - format.data_offset ||
    format.data_size % (format.bits / 8)
  ) {
    return false;
  }
  if (gain == 1) {
    return true;
  }
  const auto end = format.data_offset + format.data_size;
  for (size_t offset = format.data_offset; offset < end; offset += format.bits / 8) {
    if (format.bits == 8) {
      const auto centered = static_cast<int>(bytes[offset]) - 128;
      bytes[offset] = static_cast<uint8_t>(std::lround(centered * gain) + 128);
    } else {
      const auto raw = read_u16(bytes, offset);
      const auto sample = raw < 0x8000 ? static_cast<int>(raw) : static_cast<int>(raw) - 0x10000;
      const auto scaled = static_cast<uint16_t>(std::lround(sample * gain));
      bytes[offset] = static_cast<uint8_t>(scaled);
      bytes[offset + 1] = static_cast<uint8_t>(scaled >> 8);
    }
  }
  return true;
}
} // namespace phi::detail
