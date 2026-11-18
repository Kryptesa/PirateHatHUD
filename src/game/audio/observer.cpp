#include "game/audio_volume_observer.hpp"
#include "game/audio/scan.hpp"
#include <chrono>
#include <cstdio>

namespace phi {
struct AudioVolumeObserver::Impl {
  Signal<AudioVolumeChanged> changes;
  detail::AudioVolumeLocation location;
  AudioVolumeState sampled;
  AudioVolumeState published;
  uintptr_t settings_manager_offset = 0;
  LogCallback logger = nullptr;
  std::chrono::steady_clock::time_point next_sample{};
};

AudioVolumeObserver::AudioVolumeObserver(LogCallback logger)
  : impl_(std::make_unique<Impl>()) {
  impl_->logger = logger;
}

AudioVolumeObserver::~AudioVolumeObserver() {
  stop();
}

bool AudioVolumeObserver::start() {
  if (!impl_->location.engine_slot) {
    impl_->location = find_audio_volume_location(GetModuleHandleW(L"CrimsonDesert.exe"));
    impl_->next_sample = {};
    if (impl_->logger) {
      impl_->logger(
        impl_->location.engine_slot ? LogLevel::info : LogLevel::warn,
        impl_->location.engine_slot
          ? "Audio volume observer started"
          : "Audio volume observer unavailable; treasure sound muted"
      );
    }
  }
  return impl_->location.engine_slot != 0;
}

void AudioVolumeObserver::poll() {
  const auto now = std::chrono::steady_clock::now();
  if (impl_->location.engine_slot && now >= impl_->next_sample) {
    const auto sample = detail::sample_audio_volume(impl_->location, detail::read_memory);
    if (
      impl_->logger &&
      sample.settings_manager_offset &&
      sample.settings_manager_offset != impl_->settings_manager_offset
    ) {
      char message[96]{};
      std::snprintf(
        message,
        sizeof(message),
        "Audio volume layout selected: settings manager +0x%zX",
        sample.settings_manager_offset
      );
      impl_->logger(LogLevel::info, message);
    }
    impl_->settings_manager_offset = sample.settings_manager_offset;
    impl_->sampled = sample.volume;
    impl_->next_sample = now + std::chrono::milliseconds(50);
  }
  if (impl_->sampled != impl_->published) {
    const auto previous = impl_->published;
    impl_->published = impl_->sampled;
    if (impl_->logger) {
      impl_->logger(
        LogLevel::debug,
        impl_->sampled.known
          ? "Audio volume sample valid"
          : "Audio volume sample unavailable; treasure sound muted"
      );
    }
    impl_->changes.publish({previous, impl_->sampled});
  }
}

void AudioVolumeObserver::stop() noexcept {
  impl_->location = {};
  impl_->settings_manager_offset = 0;
  impl_->sampled = {};
}

AudioVolumeState AudioVolumeObserver::state() const {
  return impl_->sampled;
}

Subscription
AudioVolumeObserver::subscribe(std::function<void(const AudioVolumeChanged&)> callback) {
  return impl_->changes.subscribe(std::move(callback));
}
} // namespace phi
