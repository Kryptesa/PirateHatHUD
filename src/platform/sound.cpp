#include "platform/sound.hpp"
#include <Windows.h>
#include <mmsystem.h>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <span>

namespace phi {
namespace {
constexpr std::size_t kMaxWaveBytes = 8 * 1024 * 1024;

std::uint16_t read_u16(std::span<const std::uint8_t> bytes, std::size_t offset) {
  return static_cast<std::uint16_t>(bytes[offset] | (bytes[offset + 1] << 8));
}

std::uint32_t read_u32(std::span<const std::uint8_t> bytes, std::size_t offset) {
  return static_cast<std::uint32_t>(bytes[offset]) |
         (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
         (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
         (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

bool valid_wave(std::span<const std::uint8_t> bytes) {
  if (bytes.size() < 12 || std::memcmp(bytes.data(), "RIFF", 4) != 0 ||
      std::memcmp(bytes.data() + 8, "WAVE", 4) != 0 || read_u32(bytes, 4) != bytes.size() - 8) {
    return false;
  }
  bool format = false;
  bool data = false;
  std::uint16_t alignment = 0;
  for (std::size_t offset = 12; offset < bytes.size();) {
    if (bytes.size() - offset < 8) {
      return false;
    }
    const auto size = read_u32(bytes, offset + 4);
    const auto body = offset + 8;
    if (size > bytes.size() - body) {
      return false;
    }
    if (std::memcmp(bytes.data() + offset, "fmt ", 4) == 0) {
      if (format || data || size < 16 || read_u16(bytes, body) != 1) {
        return false;
      }
      const auto channels = read_u16(bytes, body + 2);
      const auto rate = read_u32(bytes, body + 4);
      const auto bits = read_u16(bytes, body + 14);
      alignment = read_u16(bytes, body + 12);
      if ((channels != 1 && channels != 2) || rate < 8000 || rate > 192000 ||
          (bits != 8 && bits != 16) || alignment != channels * (bits / 8) ||
          read_u32(bytes, body + 8) != rate * alignment) {
        return false;
      }
      format = true;
    } else if (std::memcmp(bytes.data() + offset, "data", 4) == 0) {
      if (!format || data || size == 0 || size % alignment != 0) {
        return false;
      }
      data = true;
    }
    const std::size_t padded = static_cast<std::size_t>(size) + (size & 1);
    if (padded > bytes.size() - body) {
      return false;
    }
    offset = body + padded;
  }
  return format && data;
}
} // namespace

SoundPlayer::~SoundPlayer() {
  stop();
}

bool SoundPlayer::prepare_embedded() {
  stop();
  wave_.clear();
  HMODULE module = nullptr;
  if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                              GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          reinterpret_cast<LPCWSTR>(&valid_wave), &module)) {
    return false;
  }
  const auto resource = FindResourceW(module, MAKEINTRESOURCEW(102), MAKEINTRESOURCEW(10));
  if (!resource) {
    return false;
  }
  const auto size = SizeofResource(module, resource);
  const auto loaded = LoadResource(module, resource);
  const auto* bytes = static_cast<const std::uint8_t*>(LockResource(loaded));
  if (!bytes || size > kMaxWaveBytes || !valid_wave({bytes, size})) {
    return false;
  }
  wave_.assign(bytes, bytes + size);
  return true;
}

bool SoundPlayer::prepare(const std::wstring& path) {
  stop();
  wave_.clear();
  std::ifstream input(std::filesystem::path(path), std::ios::binary | std::ios::ate);
  if (!input) {
    return false;
  }
  const auto size = input.tellg();
  if (size < 12 || size > static_cast<std::streamoff>(kMaxWaveBytes)) {
    return false;
  }
  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
  input.seekg(0);
  if (!input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size)) ||
      !valid_wave(bytes)) {
    return false;
  }
  wave_ = std::move(bytes);
  return true;
}

bool SoundPlayer::play() noexcept {
  if (wave_.empty()) {
    return false;
  }
  const bool played = PlaySoundW(reinterpret_cast<LPCWSTR>(wave_.data()), nullptr,
                                 SND_MEMORY | SND_ASYNC | SND_NODEFAULT | SND_NOSTOP) != FALSE;
  started_ = started_ || played;
  return played;
}

void SoundPlayer::stop() noexcept {
  if (started_) {
    // The synchronous stop finishes the asynchronous operation before bytes can be released.
    PlaySoundW(nullptr, nullptr, 0);
    started_ = false;
  }
}
} // namespace phi
