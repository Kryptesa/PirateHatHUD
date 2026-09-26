#include "render/swapchain_selection.hpp"

#define CHECK(c)                                                                                   \
  do {                                                                                             \
    if (!(c)) {                                                                                    \
      return __LINE__;                                                                             \
    }                                                                                              \
  } while (false)

int main() {
  using namespace phi::render;
  SwapchainSelection selection;
  // Creation alone never selects a chain. An auxiliary window's Present is
  // ineligible initially, even when it was created after the game chain.
  CHECK(!selection.present(2, 200, true, true, false));
  CHECK(selection.window() == 0); // Missing/wrong-device queue does not claim a window.
  CHECK(!selection.present(2, 200, false));
  CHECK(selection.window() == 0);
  CHECK(selection.present(1, 100, true));
  CHECK(!selection.present(2, 200, true));
  CHECK(selection.present(1, 100, true));
  // Replacement requires an actual Present for the same window. A later Present
  // on the old chain cannot switch back, even when its COM address gets recycled.
  CHECK(!selection.present(3, 100, true, true, false));
  CHECK(selection.present(1, 100, true));
  CHECK(selection.present(3, 100, true));
  CHECK(!selection.present(1, 100, true));
  CHECK(selection.present(4, 100, true));
  CHECK(!selection.present(3, 100, true));
  CHECK(!selection.present(0, 100, true));
  CHECK(!selection.present(5, 0, true));
  CHECK(!selection.present(5, 100, false));
  CHECK(selection.present(4, 100, true));
  CHECK(!selection.present(6, 100, true, false)); // Late discovery is not creation evidence.
  CHECK(selection.present(4, 100, true));
  SwapchainSelection preexisting;
  CHECK(preexisting.present(7, 100, true, false));
  CHECK(!preexisting.present(8, 100, true, false));
  CHECK(preexisting.present(9, 100, true, true));

  SwapchainRecords<int, 2> records;
  records.remember(1, 1) = 100;
  records.remember(2, 1) = 200;
  records.remember(3, 1) = 300;
  CHECK(records.size() == 2);
  CHECK(*records.find(1) == 100); // Keep active queue/color when auxiliaries churn.
  CHECK(records.find(2) == nullptr);
  CHECK(records.evicted(2));
  CHECK(!records.evicted(1));
  CHECK(!records.evicted(4));
  records.remember(4, 3) = 400;
  CHECK(records.size() == 2);
  CHECK(records.evicted(1));
  CHECK(*records.find(3) == 300);

  struct Record {
    int queue{}, color{};
  };
  SwapchainRecords<Record, 64> associations;
  associations.remember(1, 1) = {10, 20};
  for (uint64_t id = 2; id <= 100; ++id) {
    associations.remember(id, 1) = {static_cast<int>(id), 30};
  }
  CHECK(associations.size() == 64);
  CHECK(associations.find(1)->queue == 10);
  CHECK(associations.find(1)->color == 20);
  CHECK(associations.evicted(2));
  CHECK(associations.find(2) == nullptr);
  // A later color notification for an evicted generation does not resurrect its
  // old queue; captured creation metadata still requires an explicit association.
  associations.remember(2, 1).color = 40;
  CHECK(associations.find(2)->queue == 0);
  CHECK(associations.find(2)->color == 40);
  CHECK(associations.find(1)->queue == 10);
  CHECK(associations.find(100)->queue == 100); // Fresh generation has its own state.

  SwapchainRecords<Record, 2> sentinels;
  DirectQueueFallback<int> inferred;
  inferred.observe(10, 100);
  sentinels.remember(1, 1); // Pre-existing selected chain stores identity, not guessed queue.
  for (uint64_t id = 2; id <= 100; ++id) {
    sentinels.remember(id, 1);
  }
  CHECK(!sentinels.evicted(1));
  CHECK(sentinels.find(1)->queue == 0);
  CHECK(inferred.find(10) == 100);
  inferred.observe(10, 101);
  CHECK(inferred.find(10) == 0); // Active sentinel cannot conceal new queue ambiguity.

  DirectQueueFallback<int, 2> fallback;
  CHECK(fallback.find(10) == 0);
  fallback.observe(10, 1);
  fallback.observe(10, 1);
  fallback.observe(20, 2);
  CHECK(fallback.find(10) == 1);
  CHECK(fallback.find(20) == 2);
  fallback.observe(10, 3);
  CHECK(fallback.find(10) == 0);
  CHECK(fallback.find(20) == 2); // A different device does not introduce ambiguity.
  fallback.observe(10, 1);
  CHECK(fallback.find(10) == 0); // Ambiguity cannot be forgotten by reuse.
  fallback.observe(0, 4);
  fallback.observe(30, 0);
  CHECK(fallback.find(20) == 2);
  fallback.observe(30, 4); // Bounded storage fails closed instead of losing evidence.
  CHECK(fallback.find(20) == 0);
  CHECK(fallback.find(30) == 0);
  return 0;
}
