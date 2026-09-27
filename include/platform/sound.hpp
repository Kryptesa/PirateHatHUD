#pragma once

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace phi {
// Owner-thread preparation and commands. Audio calls run only on the player's worker.
class SoundPlayer {
public:
  // A null byte pointer stops playback; callbacks run on the audio worker only.
  using Playback = bool (*)(const std::uint8_t* bytes) noexcept;
  SoundPlayer() = default;

  explicit SoundPlayer(Playback playback)
    : playback_(playback) {}

  ~SoundPlayer();
  SoundPlayer(const SoundPlayer&) = delete;
  SoundPlayer& operator=(const SoundPlayer&) = delete;

  bool prepare(const std::wstring& path);

  bool prepare_embedded();

  // Queues a linear PCM gain in [0, 1]; zero queues a stop.
  // Returns whether a request was queued, not whether the device played it.
  bool play(float gain = 1.0f) noexcept;

  void stop() noexcept;

private:
  enum class Command { none, play, stop, shutdown };

  void start_worker();

  void finish_worker() noexcept;

  void run_worker() noexcept;
  std::vector<std::uint8_t> wave_;
  Playback playback_ = nullptr;
  std::mutex mutex_;
  std::condition_variable wake_;
  Command pending_ = Command::none;
  float pending_gain_ = 1.0f;
  std::thread worker_;
};
} // namespace phi
