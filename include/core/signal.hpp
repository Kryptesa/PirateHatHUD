#pragma once

#include <algorithm>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace phi {
namespace detail {
struct Connection {
  bool active = true;
};
} // namespace detail

// All operations, including token destruction, belong to the signal's owner thread.
class Subscription {
public:
  Subscription() = default;
  Subscription(const Subscription&) = delete;
  Subscription& operator=(const Subscription&) = delete;
  Subscription(Subscription&& other) noexcept : connection_(std::move(other.connection_)) {}
  Subscription& operator=(Subscription&& other) noexcept {
    if (this != &other) {
      reset();
      connection_ = std::move(other.connection_);
    }
    return *this;
  }
  ~Subscription() {
    reset();
  }

  void reset() noexcept {
    if (auto connection = connection_.lock()) {
      connection->active = false;
    }
    connection_.reset();
  }

  explicit operator bool() const noexcept {
    const auto connection = connection_.lock();
    return connection && connection->active;
  }

private:
  template <typename Event> friend class Signal;
  explicit Subscription(const std::shared_ptr<detail::Connection>& connection)
      : connection_(connection) {}
  std::weak_ptr<detail::Connection> connection_;
};

// Synchronous, single-thread signal. Each publish snapshots subscriptions in insertion order.
// Unsubscription takes effect immediately; new subscriptions start with the next publish.
// Reentrant publish is allowed and takes its own snapshot. Callback exceptions propagate.
template <typename Event> class Signal {
public:
  Signal() = default;
  Signal(const Signal&) = delete;
  Signal& operator=(const Signal&) = delete;
  Signal(Signal&&) = delete;
  Signal& operator=(Signal&&) = delete;
  ~Signal() {
    for (const auto& slot : slots_) {
      slot->active = false;
    }
  }

  [[nodiscard]] Subscription subscribe(std::function<void(const Event&)> callback) {
    if (!callback) {
      return {};
    }
    remove_inactive();
    auto slot = std::make_shared<Slot>(std::move(callback));
    slots_.push_back(slot);
    return Subscription(slot);
  }

  void publish(const Event& event) {
    remove_inactive();
    const auto snapshot = slots_;
    for (const auto& slot : snapshot) {
      if (slot->active) {
        slot->callback(event);
      }
    }
  }

private:
  struct Slot : detail::Connection {
    explicit Slot(std::function<void(const Event&)> fn) : callback(std::move(fn)) {}
    std::function<void(const Event&)> callback;
  };

  void remove_inactive() {
    std::erase_if(slots_, [](const auto& slot) { return !slot->active; });
  }

  std::vector<std::shared_ptr<Slot>> slots_;
};
} // namespace phi
