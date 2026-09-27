#include "game/audio_volume_observer.hpp"
#include "game/audio_volume_scan.hpp"
#include <chrono>

namespace phi {
struct AudioVolumeObserver::Impl {
  Signal<AudioVolumeChanged> changes;
  detail::AudioVolumeLocation location;
  AudioVolumeState sampled;
  AudioVolumeState published;
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
    impl_->sampled = detail::sample_audio_volume(impl_->location, detail::read_memory);
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
