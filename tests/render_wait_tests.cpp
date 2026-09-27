#include "render/wait_policy.hpp"
#include "render/frame_ring.hpp"
#include <limits>

#define CHECK(c)                                                                                   \
  do {                                                                                             \
    if (!(c))                                                                                      \
      return __LINE__;                                                                             \
  } while (false)

struct Fence {
  std::uint64_t value{}, time{}, requested{};
  unsigned waits{};
  bool lost{}, failure{}, complete_on_wake{};

  std::uint64_t completed() {
    return value;
  }

  bool device_lost() {
    return lost;
  }

  std::uint64_t now() {
    return time;
  }

  bool wait(std::uint64_t target, std::uint64_t remaining) {
    requested = remaining;
    ++waits;

    if (failure) {
      return false;
    }

    ++time; // Repeated spurious wakes must not reset the deadline.
    if (complete_on_wake) {
      value = target;
    }

    return true;
  }
};

int main() {
  using namespace phi::render;
  Fence busy;
  CHECK(wait_for_fence(busy, 1, 0) == WaitResult::timeout);
  CHECK(busy.waits == 0); // Present never enters the event wait.
  CHECK(wait_for_fence(busy, 1, 3) == WaitResult::timeout);
  CHECK(busy.waits == 3 && busy.requested == 1);

  Fence completed;
  completed.complete_on_wake = true;
  CHECK(wait_for_fence(completed, 7, 10) == WaitResult::completed);
  CHECK(completed.waits == 1);

  Fence failed;
  failed.failure = true;
  CHECK(wait_for_fence(failed, 1, 10) == WaitResult::failed);
  CHECK(!can_release(WaitResult::failed) && !can_release(WaitResult::timeout));

  Fence removed;
  removed.value = std::numeric_limits<std::uint64_t>::max();
  CHECK(wait_for_fence(removed, 1, 10) == WaitResult::device_lost);
  CHECK(removed.waits == 0);

  Fence removed_reason;
  removed_reason.lost = true;
  CHECK(wait_for_fence(removed_reason, 0, 0) == WaitResult::device_lost);
  CHECK(can_release(WaitResult::device_lost) && can_release(WaitResult::completed));

  // Multiple waits share one deadline; completion of one never replenishes it.
  Fence shared;
  shared.complete_on_wake = true;
  CHECK(wait_for_fence(shared, 1, 1) == WaitResult::completed);

  shared.complete_on_wake = false;
  CHECK(wait_for_fence(shared, 2, 1) == WaitResult::timeout);
  CHECK(shared.waits == 1);

  FrameRing ring;
  ring.reset(3);

  // Three submissions can be in flight without waiting for unrelated slots.
  CHECK(ring.required_fence(0) == 0);

  ring.submitted(1);
  CHECK(ring.required_fence(0) == 0);

  ring.submitted(2);
  CHECK(ring.required_fence(0) == 0);

  ring.submitted(3);

  // Present can skip or repeat swapchain indices: the backend ring stays separate.
  CHECK(ring.required_fence(2) == 2);
  CHECK(ring.required_fence(0) == 1);

  Fence first_done;
  first_done.value = 1; // Submissions 2/3 are still busy, but slot 0 is reusable.
  CHECK(wait_for_fence(first_done, ring.required_fence(0), 0) == WaitResult::completed);

  ring.submitted(4);
  CHECK(ring.required_fence(0) == 2);
  CHECK(wait_for_fence(first_done, ring.required_fence(0), 0) == WaitResult::timeout);
  CHECK(ring.required_fence(0) == 2); // Skipping must not advance the backend ring.
  CHECK(ring.required_fence(4) == 4); // Reusing an allocator can require a newer fence.
  ring.reset(2);                      // Backend recreation starts at slot zero with no old fences.
  CHECK(ring.required_fence(0) == 0);

  ring.submitted(5);
  ring.submitted(6);
  CHECK(ring.required_fence(0) == 5);

  return 0;
}
