#include "platform/sound.hpp"
#include "platform/wave_pcm.hpp"
#include <cmath>
#include <Windows.h>
#include <mmsystem.h>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <span>

namespace phi {

namespace {
constexpr std::size_t kMaxWaveBytes = 8 * 1024 * 1024;

bool play_wave(const std::uint8_t* bytes) noexcept {
  if (!bytes) {
    return PlaySoundW(nullptr, nullptr, 0) != FALSE;
  }

  const auto played = PlaySoundW(
    reinterpret_cast<LPCWSTR>(bytes),
    nullptr,
    SND_MEMORY | SND_ASYNC | SND_NODEFAULT | SND_NOSTOP
  );

  return played != FALSE;
}

} // namespace

SoundPlayer::~SoundPlayer() {
  finish_worker();
}

bool SoundPlayer::prepare_embedded() {
  finish_worker();
  wave_.clear();
  HMODULE module = nullptr;

  if (!GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&play_wave),
        &module
      )) {
    return false;
  }

  const auto resource = FindResourceW(module, MAKEINTRESOURCEW(102), MAKEINTRESOURCEW(10));

  if (!resource) {
    return false;
  }

  const auto size = SizeofResource(module, resource);
  const auto loaded = LoadResource(module, resource);
  const auto* bytes = static_cast<const std::uint8_t*>(LockResource(loaded));

  if (!bytes || size > kMaxWaveBytes || !detail::parse_pcm_wave({bytes, size})) {
    return false;
  }

  wave_.assign(bytes, bytes + size);
  start_worker();

  return true;
}

bool SoundPlayer::prepare(const std::wstring& path) {
  finish_worker();
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

  if (
    !input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size)) ||
    !detail::parse_pcm_wave(bytes)
  ) {
    return false;
  }

  wave_ = std::move(bytes);
  start_worker();

  return true;
}

bool SoundPlayer::play(float gain) noexcept {
  if (!worker_.joinable() || !std::isfinite(gain) || gain < 0 || gain > 1) {
    return false;
  }

  {
    std::lock_guard lock(mutex_);
    pending_ = gain == 0 ? Command::stop : Command::play;
    pending_gain_ = gain;
  }

  wake_.notify_one();

  return true;
}

void SoundPlayer::stop() noexcept {
  if (worker_.joinable()) {
    {
      std::lock_guard lock(mutex_);
      pending_ = Command::stop;
    }

    wake_.notify_one();
  }
}

void SoundPlayer::start_worker() {
  pending_ = Command::none;
  worker_ = std::thread([this] { run_worker(); });
}

void SoundPlayer::finish_worker() noexcept {
  if (worker_.joinable()) {
    {
      std::lock_guard lock(mutex_);
      pending_ = Command::shutdown;
    }

    wake_.notify_one();
    worker_.join();
  }
}

void SoundPlayer::run_worker() noexcept {
  const auto playback = playback_ ? playback_ : play_wave;
  bool started = false;
  std::vector<uint8_t> playing_wave;
  const auto format = detail::parse_pcm_wave(wave_);

  for (;;) {
    Command command;
    float gain = 1;
    {
      std::unique_lock lock(mutex_);
      wake_.wait(lock, [this] { return pending_ != Command::none; });
      command = pending_;
      gain = pending_gain_;
      pending_ = Command::none;
    }

    // Never hold the command mutex across device operations. Only the latest
    // pending command is kept, so a stop cancels any play not yet started.
    if (command == Command::play) {
      try {
        auto candidate = wave_;
        if (
          format && detail::scale_pcm_wave(candidate, *format, gain) && playback(candidate.data())
        ) {
          playing_wave = std::move(candidate);
          started = true;
        }
      } catch (...) {
        // Allocation failure drops this request while retaining any active buffer.
      }
    } else {
      if (started) {
        playback(nullptr);
        started = false;
        playing_wave.clear();
      }

      if (command == Command::shutdown) {
        return;
      }
    }
  }
}
} // namespace phi
