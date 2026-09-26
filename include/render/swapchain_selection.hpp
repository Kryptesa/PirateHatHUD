#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <unordered_map>

namespace phi::render {
// IDs are stored on the DXGI object, rather than inferred from reusable addresses.
// Selection happens on Present; creating an auxiliary chain cannot replace the HUD.
class SwapchainSelection {
public:
  uintptr_t window() const {
    return window_;
  }
  uint64_t identity() const {
    return identity_;
  }
  bool present(uint64_t identity, uintptr_t window, bool eligible, bool creation_observed = true,
               bool queue_valid = true) {
    if (!identity || !window || !eligible || !queue_valid) {
      return false;
    }
    if (!window_) {
      window_ = window;
      identity_ = identity;
    } else if (window == window_ && creation_observed && identity > identity_) {
      identity_ = identity;
    }
    return window == window_ && identity == identity_;
  }

private:
  uintptr_t window_{};
  uint64_t identity_{};
};

template <class Record, size_t Capacity = 64> class SwapchainRecords {
  static_assert(Capacity >= 2);

public:
  Record& remember(uint64_t identity, uint64_t selected) {
    if (!entries_.contains(identity) && entries_.size() >= Capacity) {
      auto oldest = entries_.end();
      for (auto it = entries_.begin(); it != entries_.end(); ++it) {
        if (it->first != selected && (oldest == entries_.end() || it->first < oldest->first)) {
          oldest = it;
        }
      }
      watermark_ = (std::max)(watermark_, oldest->first);
      entries_.erase(oldest);
    }
    return entries_[identity];
  }
  const Record* find(uint64_t identity) const {
    const auto it = entries_.find(identity);
    return it == entries_.end() ? nullptr : &it->second;
  }
  bool evicted(uint64_t identity) const {
    return identity <= watermark_ && !entries_.contains(identity);
  }
  size_t size() const {
    return entries_.size();
  }

private:
  std::unordered_map<uint64_t, Record> entries_;
  uint64_t watermark_{};
};

// A device's fallback is usable only after observing exactly one direct queue.
// Overflow fails closed instead of evicting evidence of ambiguity.
template <class Queue, size_t Capacity = 16> class DirectQueueFallback {
public:
  void observe(uintptr_t device, Queue queue) {
    if (!device || !queue || overflow_) {
      return;
    }
    for (auto& entry : entries_) {
      if (entry.device == device) {
        if (entry.queue != queue) {
          entry.ambiguous = true;
        }
        return;
      }
      if (!entry.device) {
        entry = {device, queue, false};
        return;
      }
    }
    overflow_ = true;
  }
  Queue find(uintptr_t device) const {
    if (!overflow_) {
      for (const auto& entry : entries_) {
        if (entry.device == device && !entry.ambiguous) {
          return entry.queue;
        }
      }
    }
    return {};
  }

private:
  struct Entry {
    uintptr_t device{};
    Queue queue{};
    bool ambiguous{};
  };
  std::array<Entry, Capacity> entries_{};
  bool overflow_{};
};
} // namespace phi::render
