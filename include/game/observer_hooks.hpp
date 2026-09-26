#pragma once

#include "game/treasure_observer.hpp"

#include <memory>

namespace phi::detail {

// Shared production/test lifecycle. Hook must provide bool conversion, enable(),
// disable(), and reset(). Allocation precedes activation; retirement allocates nothing.
template <typename Hook> class ObserverHooks {
public:
  struct Pair {
    Hook enter;
    Hook leave;
  };

  ObserverHooks() : hooks_(std::make_unique<Pair>()) {}
  ~ObserverHooks() {
    stop();
  }
  ObserverHooks(const ObserverHooks&) = delete;
  ObserverHooks& operator=(const ObserverHooks&) = delete;

  Pair& hooks() {
    return *hooks_;
  }
  ObserverStopResult result() const noexcept {
    return result_;
  }

  bool enable() {
    if (result_.module_must_remain_loaded) {
      return false;
    }
    // Even a failed call can have partially activated instructions.
    result_.module_must_remain_loaded = true;
    return static_cast<bool>(hooks_->enter.enable()) && static_cast<bool>(hooks_->leave.enable());
  }

  ObserverStopResult stop() noexcept {
    if (!hooks_) {
      return result_;
    }
    bool disabled = true;
    disable(hooks_->enter, disabled);
    disable(hooks_->leave, disabled);
    result_.hooks_disabled = disabled;
    result_.module_must_remain_loaded |= !disabled;
    if (!result_.module_must_remain_loaded) {
      try {
        hooks_->enter.reset();
        hooks_->leave.reset();
      } catch (...) {
        result_.hooks_disabled = false;
        result_.module_must_remain_loaded = true;
      }
    }
    if (result_.module_must_remain_loaded) {
      // No counter proves completion of generated stub or callback return paths.
      // Deliberately retain both allocations and the containing DLL until process exit.
      (void)hooks_.release();
    }
    return result_;
  }

private:
  static void disable(Hook& hook, bool& disabled) noexcept {
    try {
      if (hook) {
        disabled = static_cast<bool>(hook.disable()) && disabled;
      }
    } catch (...) {
      disabled = false;
    }
  }

  std::unique_ptr<Pair> hooks_;
  ObserverStopResult result_{true, false};
};

} // namespace phi::detail
