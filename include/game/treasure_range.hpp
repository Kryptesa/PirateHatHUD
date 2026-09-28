#pragma once

#include "core/log.hpp"
#include <memory>

namespace phi {
// Owner-thread modifier, independent of treasure observation and presentation.
// Zero restores the captured native radius. No game hooks are installed.
class TreasureRange {
public:
  explicit TreasureRange(LogCallback logger = nullptr);
  ~TreasureRange();
  TreasureRange(const TreasureRange&) = delete;
  TreasureRange& operator=(const TreasureRange&) = delete;

  void poll(float radius);
  void stop() noexcept;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
} // namespace phi
