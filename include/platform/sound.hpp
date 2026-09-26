#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace phi {
// Owner-thread utility. Prepared PCM WAVE bytes remain owned until playback is stopped.
class SoundPlayer {
public:
  SoundPlayer() = default;
  ~SoundPlayer();
  SoundPlayer(const SoundPlayer&) = delete;
  SoundPlayer& operator=(const SoundPlayer&) = delete;

  bool prepare(const std::wstring& path);
  bool prepare_embedded();
  bool play() noexcept;
  void stop() noexcept;

private:
  std::vector<std::uint8_t> wave_;
  bool started_ = false;
};
} // namespace phi
