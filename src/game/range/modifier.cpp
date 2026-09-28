#include "game/treasure_range.hpp"
#include "game/range/scan.hpp"
#include <chrono>
#include <cstdio>
#include <optional>

namespace phi {
struct TreasureRange::Impl {
  LogCallback logger = nullptr;
  detail::RangeLocation location{};
  detail::RangeOverride override;
  std::optional<detail::RangeStatus> status;
  std::chrono::steady_clock::time_point next_check{};
  float requested = 0;
  bool scanned = false;
  bool stopped = false;

  void report(detail::RangeStatus next) {
    if (status == next) {
      return;
    }
    status = next;
    if (!logger) {
      return;
    }
    if (next == detail::RangeStatus::applied) {
      char message[128]{};
      std::snprintf(
        message,
        sizeof(message),
        "Hat detection radius: %.3g (original %.3g)",
        requested,
        override.original()
      );
      logger(LogLevel::info, message);
    } else if (next == detail::RangeStatus::disabled) {
      logger(LogLevel::info, "Hat radius override disabled; native radius restored if owned");
    } else if (next == detail::RangeStatus::waiting) {
      logger(LogLevel::debug, "Hat radius: waiting for a unique validated finder descriptor");
    } else {
      logger(
        LogLevel::warn,
        next == detail::RangeStatus::conflict
          ? "Hat radius: identity/value changed; conflicting state left untouched"
          : "Hat radius: guarded write failed; retrying without changing protection"
      );
    }
  }
};

TreasureRange::TreasureRange(LogCallback logger)
  : impl_(std::make_unique<Impl>()) {
  impl_->logger = logger;
}

TreasureRange::~TreasureRange() {
  stop();
}

void TreasureRange::poll(float radius) {
  auto& impl = *impl_;
  if (impl.stopped) {
    return;
  }
  const auto now = std::chrono::steady_clock::now();
  const bool changed = radius != impl.requested;
  if (!changed && now < impl.next_check) {
    return;
  }
  impl.next_check = now + std::chrono::seconds(1);
  impl.requested = radius;
  if (!impl.scanned && radius == 0) {
    return;
  }
  if (!impl.scanned && radius > 0) {
    impl.scanned = true;
    impl.location = find_treasure_range_location(GetModuleHandleW(nullptr));
    if (!impl.location.pool_slot && impl.logger) {
      impl.logger(
        LogLevel::warn,
        "Hat radius unavailable: loader signature missing, ambiguous or invalid"
      );
    }
  }
  const auto next =
    impl.override.update(impl.location, detail::read_memory, detail::exchange_range_float, radius);
  if (changed && next == detail::RangeStatus::applied) {
    impl.status.reset();
  }
  impl.report(next);
}

void TreasureRange::stop() noexcept {
  auto& impl = *impl_;
  if (impl.stopped) {
    return;
  }
  impl.stopped = true;
  if (!impl.scanned) {
    return;
  }
  try {
    const auto result =
      impl.override.restore(impl.location, detail::read_memory, detail::exchange_range_float);
    if (result == detail::RangeStatus::waiting && impl.logger) {
      impl.logger(
        LogLevel::warn,
        "Hat radius restoration skipped: descriptor unavailable at shutdown"
      );
    } else {
      impl.report(result);
    }
  } catch (...) {
    // Shutdown must continue even if an injected logger fails.
  }
}
} // namespace phi
