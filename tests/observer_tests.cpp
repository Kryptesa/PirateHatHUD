#include "game/treasure_observer.hpp"

#include <memory>
#include <string>
#include <vector>

#define CHECK(c)                                                                                   \
  do {                                                                                             \
    if (!(c)) {                                                                                    \
      return __LINE__;                                                                             \
    }                                                                                              \
  } while (false)

namespace {
std::vector<std::string> logs;

void capture_log(phi::LogLevel, const char* text) {
  logs.emplace_back(text);
}
} // namespace

// This executable runs outside CrimsonDesert.exe, exercising lifecycle failure paths
// without installing hooks or depending on a particular game build.
int main() {
  phi::TreasureObserver first(capture_log);

  phi::TreasureObserver second(capture_log);
  unsigned callbacks = 0;
  auto subscription = first.subscribe([&](const phi::TreasureStateChanged&) { ++callbacks; });
  CHECK(subscription);
  CHECK(!first.subscribe({}));
  CHECK(first.state() == phi::TreasureState::unknown);

  const auto initial_stop = first.stop();
  CHECK(initial_stop.hooks_disabled);
  CHECK(!initial_stop.module_must_remain_loaded);

  const auto repeated_stop = first.stop();
  CHECK(repeated_stop.hooks_disabled);
  CHECK(!repeated_stop.module_must_remain_loaded);

  first.poll();
  CHECK(callbacks == 0);

  CHECK(!first.start());
  CHECK(first.state() == phi::TreasureState::unknown);
  CHECK(!logs.empty());
  CHECK(logs.back() == "State hooks disabled: invalid image");

  first.poll();
  const auto failed_start_stop = first.stop();
  CHECK(failed_start_stop.hooks_disabled);
  CHECK(!failed_start_stop.module_must_remain_loaded);
  CHECK(callbacks == 0);

  // A failed start must release the global ownership claim for retries and other instances.
  CHECK(!first.start());
  CHECK(logs.back() == "State hooks disabled: invalid image");
  CHECK(!second.start());
  CHECK(logs.back() == "State hooks disabled: invalid image");

  second.stop();
  second.poll();
  CHECK(second.state() == phi::TreasureState::unknown);
  CHECK(callbacks == 0);

  subscription.reset();
  CHECK(!subscription);

  first.poll();
  CHECK(callbacks == 0);

  phi::Subscription surviving_subscription;
  {
    auto temporary = std::make_unique<phi::TreasureObserver>();
    surviving_subscription = temporary->subscribe([](const phi::TreasureStateChanged&) {});
    CHECK(surviving_subscription);
    CHECK(!temporary->start());
  }

  CHECK(!surviving_subscription);

  surviving_subscription.reset();

  return 0;
}
