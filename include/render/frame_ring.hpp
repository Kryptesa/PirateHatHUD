#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

namespace phi::render {
// Mirrors the DX12 backend's uint32 frame counter, independent of swapchain indices.
class FrameRing {
public:
  void reset(std::size_t count) {
    fences_.assign(count, 0);
    next_ = 0;
  }

  std::size_t next_index() const {
    return next_ % fences_.size();
  }

  std::uint64_t required_fence() const {
    return fences_[next_index()];
  }

  std::uint64_t latest_fence() const {
    return fences_.empty() ? 0 : *std::max_element(fences_.begin(), fences_.end());
  }

  // Call only after RenderDrawData consumed a slot and its submission was fenced.
  void submitted(std::uint64_t fence) {
    fences_[next_index()] = fence;
    ++next_;
  }

private:
  std::vector<std::uint64_t> fences_;
  std::uint32_t next_{};
};
} // namespace phi::render
