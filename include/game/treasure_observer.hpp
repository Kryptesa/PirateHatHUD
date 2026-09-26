#pragma once

#include "core/signal.hpp"

#include <functional>
#include <memory>

namespace phi {

enum class TreasureState { unknown, inactive, active };

struct TreasureStateChanged {
  TreasureState previous;
  TreasureState current;
};

// All public operations, including subscription destruction, belong to one owner thread.
// Game hooks capture data only; subscribers run synchronously from poll().
// This observer reports the latest sampled state, not every intermediate game transition.
// stop() disables hooks and drains callback bodies. SafetyHook does not expose a way to
// drain the remaining MidHook assembly stub; this retains the existing DLL unload limitation.
class TreasureObserver {
public:
  explicit TreasureObserver(void (*logger)(const char*) = nullptr);
  ~TreasureObserver();

  TreasureObserver(const TreasureObserver&) = delete;
  TreasureObserver& operator=(const TreasureObserver&) = delete;
  TreasureObserver(TreasureObserver&&) = delete;
  TreasureObserver& operator=(TreasureObserver&&) = delete;

  bool start();
  void poll();
  void stop();
  TreasureState state() const;
  Subscription subscribe(std::function<void(const TreasureStateChanged&)> callback);

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace phi
