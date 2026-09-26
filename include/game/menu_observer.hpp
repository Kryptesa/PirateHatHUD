#pragma once
#include "core/log.hpp"
#include "core/signal.hpp"
#include "game/treasure_observer.hpp"
#include <functional>
#include <memory>
namespace phi {
enum class MenuState { unknown, open, closed };
struct MenuStateChanged {
  MenuState previous;
  MenuState current;
};
class MenuObserver {
public:
  explicit MenuObserver(LogCallback logger = nullptr);
  ~MenuObserver();
  MenuObserver(const MenuObserver&) = delete;
  MenuObserver& operator=(const MenuObserver&) = delete;
  bool start();
  void poll();
  ObserverStopResult stop() noexcept;
  MenuState state() const;
  Subscription subscribe(std::function<void(const MenuStateChanged&)> callback);

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
} // namespace phi
