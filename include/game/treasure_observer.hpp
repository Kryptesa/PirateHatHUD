#pragma once
#include "core/log.hpp"

#include "core/signal.hpp"

#include <functional>
#include <memory>

namespace phi {

enum class TreasureState { unknown, inactive, active };

struct TreasureStateChanged {
  TreasureState previous;
  TreasureState current;
};

struct ObserverStopResult {
  bool hooks_disabled;
  bool module_must_remain_loaded;
};

// All public operations, including subscription destruction, belong to one owner thread.
// Game hooks capture data only; subscribers run synchronously from poll().
// This observer reports the latest sampled state, not every intermediate game transition.
// After any activation attempt, stop() retains hook code and prohibits restart.
// Callback counts cannot prove that threads have left the MidHook assembly stub.
// The containing DLL must remain loaded until process exit.
class TreasureObserver {
public:
  explicit TreasureObserver(LogCallback logger = nullptr);
  ~TreasureObserver();

  TreasureObserver(const TreasureObserver&) = delete;
  TreasureObserver& operator=(const TreasureObserver&) = delete;
  TreasureObserver(TreasureObserver&&) = delete;
  TreasureObserver& operator=(TreasureObserver&&) = delete;

  bool start();
  void poll();
  ObserverStopResult stop() noexcept;
  TreasureState state() const;
  Subscription subscribe(std::function<void(const TreasureStateChanged&)> callback);

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace phi
