#include "core/signal.hpp"
#include <stdexcept>
#include <type_traits>
#include <vector>

#define CHECK(c)                                                                                   \
  do {                                                                                             \
    if (!(c)) {                                                                                    \
      return __LINE__;                                                                             \
    }                                                                                              \
  } while (false)

static_assert(!std::is_copy_constructible_v<phi::Subscription>);
static_assert(std::is_nothrow_move_constructible_v<phi::Subscription>);

int main() {
  phi::Signal<int> signal;
  std::vector<int> calls;
  auto first = signal.subscribe([&](const int& value) { calls.push_back(value); });
  auto second = signal.subscribe([&](const int& value) { calls.push_back(value + 10); });
  signal.publish(1);
  CHECK((calls == std::vector<int>{1, 11}));

  first.reset();
  calls.clear();
  signal.publish(2);
  CHECK((calls == std::vector<int>{12}));

  {
    auto temporary = signal.subscribe([&](const int&) { calls.push_back(99); });
  }

  calls.clear();
  signal.publish(3);
  CHECK((calls == std::vector<int>{13}));

  phi::Subscription moved = std::move(second);
  CHECK(!second);
  CHECK(moved);

  phi::Subscription replacement = signal.subscribe([&](const int&) { calls.push_back(99); });
  replacement = std::move(moved);
  replacement = std::move(replacement);
  CHECK(replacement);
  CHECK(!moved);

  calls.clear();
  signal.publish(4);
  CHECK((calls == std::vector<int>{14}));

  phi::Subscription survivor;
  {
    phi::Signal<int> temporary;
    survivor = temporary.subscribe([](const int&) {});
    CHECK(survivor);
  }

  CHECK(!survivor);

  survivor.reset();
  CHECK(!signal.subscribe({}));

  phi::Signal<int> changing;
  phi::Subscription self;
  phi::Subscription skipped;
  phi::Subscription added;
  self = changing.subscribe([&](const int&) {
    calls.push_back(1);
    self.reset();
    skipped.reset();
    added = changing.subscribe([&](const int&) { calls.push_back(3); });
  });

  skipped = changing.subscribe([&](const int&) { calls.push_back(2); });
  calls.clear();
  changing.publish(0);
  CHECK((calls == std::vector<int>{1}));

  changing.publish(0);
  CHECK((calls == std::vector<int>{1, 3}));

  phi::Signal<int> recursive;
  phi::Subscription nested_addition;
  auto recursive_first = recursive.subscribe([&](const int& value) {
    calls.push_back(value);

    if (value == 1) {
      nested_addition =
        recursive.subscribe([&](const int& nested) { calls.push_back(nested + 20); });
      recursive.publish(2);
    }
  });

  auto recursive_second =
    recursive.subscribe([&](const int& value) { calls.push_back(value + 10); });
  calls.clear();
  recursive.publish(1);
  CHECK((calls == std::vector<int>{1, 2, 12, 22, 11}));

  phi::Signal<int> throwing;
  auto thrower = throwing.subscribe([](const int&) { throw std::runtime_error("callback"); });
  auto after_throw = throwing.subscribe([&](const int&) { calls.push_back(5); });
  calls.clear();
  bool caught = false;

  try {
    throwing.publish(0);
  } catch (const std::runtime_error&) {
    caught = true;
  }

  CHECK(caught);
  CHECK(calls.empty());

  thrower.reset();
  throwing.publish(0);
  CHECK((calls == std::vector<int>{5}));
}
