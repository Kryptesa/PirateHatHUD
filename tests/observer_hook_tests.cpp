#include "game/observer_hooks.hpp"

#include <initializer_list>
#include <stdexcept>

#define CHECK(c)                                                                                   \
  do {                                                                                             \
    if (!(c)) {                                                                                    \
      return __LINE__;                                                                             \
    }                                                                                              \
  } while (false)

namespace {
struct Counts {
  unsigned enable = 0;
  unsigned disable = 0;
  unsigned release = 0;
};
struct FakeHook {
  Counts* counts = nullptr;
  bool live = false;
  bool enable_ok = true;
  bool disable_ok = true;
  bool throw_disable = false;

  ~FakeHook() {
    reset();
  }
  explicit operator bool() const {
    return live;
  }
  bool enable() {
    ++counts->enable;
    return enable_ok;
  }
  bool disable() {
    ++counts->disable;
    if (throw_disable) {
      throw std::runtime_error("disable failure");
    }
    return disable_ok;
  }
  void reset() {
    if (live) {
      ++counts->release;
      live = false;
    }
  }
};
using Hooks = phi::detail::ObserverHooks<FakeHook>;
void prepare(Hooks& hooks, Counts& counts) {
  hooks.hooks().enter.counts = &counts;
  hooks.hooks().leave.counts = &counts;
  hooks.hooks().enter.live = true;
  hooks.hooks().leave.live = true;
}
} // namespace

int main() {
  // Prepared but never activated hooks are actually released, allowing retry.
  Counts prepared;
  {
    Hooks hooks;
    prepare(hooks, prepared);
    const auto result = hooks.stop();
    CHECK(result.hooks_disabled);
    CHECK(!result.module_must_remain_loaded);
    CHECK(prepared.enable == 0);
    CHECK(prepared.release == 2);
    prepare(hooks, prepared);
    CHECK(!hooks.stop().module_must_remain_loaded);
    CHECK(prepared.release == 4);
  }
  CHECK(prepared.release == 4);

  // Successful disable still cannot authorize freeing generated hook code.
  Counts successful;
  {
    Hooks hooks;
    prepare(hooks, successful);
    CHECK(hooks.enable());
    const auto result = hooks.stop();
    CHECK(result.hooks_disabled);
    CHECK(result.module_must_remain_loaded);
    CHECK(successful.enable == 2);
    CHECK(successful.disable == 2);
    CHECK(successful.release == 0);
    CHECK(!hooks.enable());
    CHECK(hooks.stop().module_must_remain_loaded);
    CHECK(successful.disable == 2);
  }
  CHECK(successful.release == 0);

  // Stack unwinding/destruction uses the same retirement policy without an explicit stop.
  Counts unwound;
  {
    Hooks hooks;
    prepare(hooks, unwound);
    CHECK(hooks.enable());
  }
  CHECK(unwound.disable == 2);
  CHECK(unwound.release == 0);

  // Failure enabling the second hook retires both allocations, including the first.
  Counts partial;
  {
    Hooks hooks;
    prepare(hooks, partial);
    hooks.hooks().leave.enable_ok = false;
    CHECK(!hooks.enable());
    CHECK(hooks.stop().module_must_remain_loaded);
    CHECK(partial.enable == 2);
    CHECK(partial.disable == 2);
  }
  CHECK(partial.release == 0);

  // Retention begins before the first enable call, including a first-call failure.
  Counts first_failed;
  {
    Hooks hooks;
    prepare(hooks, first_failed);
    hooks.hooks().enter.enable_ok = false;
    CHECK(!hooks.enable());
    CHECK(first_failed.enable == 1);
    CHECK(hooks.stop().module_must_remain_loaded);
  }
  CHECK(first_failed.release == 0);

  // Failed/throwing disable does not skip cleanup attempts for the other hook.
  for (const bool throws : {false, true}) {
    Counts failed;
    {
      Hooks hooks;
      prepare(hooks, failed);
      CHECK(hooks.enable());
      hooks.hooks().enter.disable_ok = false;
      hooks.hooks().enter.throw_disable = throws;
      const auto result = hooks.stop();
      CHECK(!result.hooks_disabled);
      CHECK(result.module_must_remain_loaded);
      CHECK(failed.disable == 2);
      CHECK(!hooks.stop().hooks_disabled);
    }
    CHECK(failed.release == 0);
  }
  return 0;
}
