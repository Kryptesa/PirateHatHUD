#pragma once

#include "core/log.hpp"
#include "core/signal.hpp"
#include "game/observer_state.hpp"
#include <functional>
#include <memory>

namespace phi {
// Owner-thread sampling and publication. Unknown volumes suppress sound; no hooks.
class AudioVolumeObserver {
public:
  explicit AudioVolumeObserver(LogCallback logger = nullptr);
  ~AudioVolumeObserver();
  AudioVolumeObserver(const AudioVolumeObserver&) = delete;
  AudioVolumeObserver& operator=(const AudioVolumeObserver&) = delete;
  bool start();
  void poll();
  void stop() noexcept;
  AudioVolumeState state() const;
  Subscription subscribe(std::function<void(const AudioVolumeChanged&)> callback);

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
} // namespace phi
