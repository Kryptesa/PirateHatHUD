#pragma once

#include "core/log.hpp"
#include "core/signal.hpp"
#include "game/observer_state.hpp"
#include <functional>
#include <memory>

namespace phi {
// Operations and subscriptions belong to the owner thread. poll() samples memory and
// publishes changes synchronously; unknown means no valid sample. No game hooks.
class MinimapObserver {
public:
  explicit MinimapObserver(LogCallback logger = nullptr);
  ~MinimapObserver();
  MinimapObserver(const MinimapObserver&) = delete;
  MinimapObserver& operator=(const MinimapObserver&) = delete;

  bool start();

  void poll();

  void stop() noexcept;

  MinimapState state() const;

  Subscription subscribe(std::function<void(const MinimapStateChanged&)> callback);

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
} // namespace phi
