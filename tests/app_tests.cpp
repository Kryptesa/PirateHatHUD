#include "app.hpp"
#include "game/treasure_observer.hpp"
#include "overlay.hpp"
#include "platform/config.hpp"
#include "platform/hotkeys.hpp"
#include "platform/logger.hpp"
#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

#define CHECK(c)                                                                                   \
  do {                                                                                             \
    if (!(c)) {                                                                                    \
      return __LINE__;                                                                             \
    }                                                                                              \
  } while (false)

namespace {
struct Scenario {
  std::string fail;
  std::vector<std::string> calls;
  bool icon_valid = true;
  bool overlay_started = false;
  bool observer_started = false;
  bool observer_retained = false;
  bool overlay_retained = false;
  bool drained = true;
  bool hooks_disabled = true;
  bool gpu_released = true;
  bool log_open = false;
  bool cleanup_order_valid = true;
  unsigned subscriber_calls = 0;
  phi::Signal<phi::TreasureStateChanged>* signal = nullptr;
} scenario;
void step(const char* name) {
  scenario.calls.emplace_back(name);
  if (scenario.fail == name) {
    throw std::runtime_error(name);
  }
  if (scenario.fail == "unknown" && std::string(name) == "poll") {
    throw 1;
  }
  if (scenario.fail == "diagnostics" &&
      (std::string(name) == "poll" ||
       (std::string(name) == "log" && std::find(scenario.calls.begin(), scenario.calls.end(),
                                                "observer_stop") != scenario.calls.end()))) {
    throw std::runtime_error("diagnostic failure");
  }
  if (scenario.fail == "hud_runtime" && std::string(name) == "hud" &&
      std::count(scenario.calls.begin(), scenario.calls.end(), "hud") > 1) {
    throw std::runtime_error("HUD publication failure");
  }
}
bool has(const char* name) {
  return std::find(scenario.calls.begin(), scenario.calls.end(), name) != scenario.calls.end();
}
bool before(const char* first, const char* second) {
  return std::find(scenario.calls.begin(), scenario.calls.end(), first) <
         std::find(scenario.calls.begin(), scenario.calls.end(), second);
}
phi::AppExitDisposition run() {
  return phi::run_app(GetModuleHandleW(nullptr));
}
} // namespace

namespace phi {
struct TreasureObserver::Impl {
  Signal<TreasureStateChanged> changes;
  bool stopped = false;
};
TreasureObserver::TreasureObserver(void (*)(const char*)) {
  step("observer_construct");
  impl_ = std::make_unique<Impl>();
  scenario.signal = &impl_->changes;
}
TreasureObserver::~TreasureObserver() {
  stop();
  scenario.signal = nullptr;
}
bool TreasureObserver::start() {
  step("observer_start");
  return scenario.observer_started;
}
void TreasureObserver::poll() {
  step("poll");
  impl_->changes.publish({TreasureState::unknown, TreasureState::active});
}
ObserverStopResult TreasureObserver::stop() noexcept {
  if (!impl_->stopped) {
    impl_->stopped = true;
    scenario.calls.emplace_back("observer_stop");
    scenario.cleanup_order_valid &= scenario.log_open;
  }
  return {true, scenario.observer_retained};
}
TreasureState TreasureObserver::state() const {
  return TreasureState::unknown;
}
Subscription
TreasureObserver::subscribe(std::function<void(const TreasureStateChanged&)> callback) {
  step("subscribe");
  return impl_->changes.subscribe([callback = std::move(callback)](const auto& event) {
    ++scenario.subscriber_calls;
    if (scenario.fail == "subscriber") {
      throw std::runtime_error("subscriber");
    }
    callback(event);
  });
}
bool open_log(const std::wstring&) {
  scenario.log_open = true;
  step("open_log");
  return true;
}
void close_log() {
  scenario.calls.emplace_back("close_log");
  scenario.log_open = false;
  if (scenario.fail == "close_log") {
    throw std::runtime_error("close_log");
  }
}
void log(const char*) {
  step("log");
}
Config read_config(const std::wstring&) {
  step("config");
  return {};
}
HotkeyActions poll_hotkeys(int, int) {
  step("hotkeys");
  return {true, true};
}
bool prepare_overlay_icon(const wchar_t*) {
  step("icon");
  return scenario.icon_valid;
}
void set_overlay_hud(const HudState&) {
  step("hud");
}
void set_overlay_log(void (*logger)(const char*)) {
  step(logger ? "set_logger" : "clear_logger");
}
bool start_overlay() {
  step("overlay_start");
  return scenario.overlay_started;
}
OverlayStopResult stop_overlay() noexcept {
  scenario.calls.emplace_back("overlay_stop");
  scenario.cleanup_order_valid &= scenario.log_open && has("observer_stop");
  const auto callbacks = scenario.subscriber_calls;
  if (scenario.signal) {
    // A token reset must take effect before graphics shutdown begins.
    try {
      scenario.signal->publish({TreasureState::active, TreasureState::unknown});
    } catch (...) {
      scenario.cleanup_order_valid = false;
    }
  }
  scenario.cleanup_order_valid &= callbacks == scenario.subscriber_calls;
  return {scenario.hooks_disabled, scenario.drained, scenario.gpu_released,
          scenario.overlay_retained};
}
} // namespace phi

int main() {
  using phi::AppExitDisposition;
  scenario = {};
  CHECK(run() == AppExitDisposition::unload_allowed);
  CHECK(scenario.cleanup_order_valid);
  CHECK(before("observer_stop", "overlay_stop"));
  CHECK(before("overlay_stop", "clear_logger"));
  CHECK(before("clear_logger", "close_log"));
  CHECK(std::count(scenario.calls.begin(), scenario.calls.end(), "observer_stop") == 1);
  CHECK(std::count(scenario.calls.begin(), scenario.calls.end(), "overlay_stop") == 1);

  for (const auto* failure :
       {"observer_construct", "subscribe", "hud", "set_logger", "overlay_start", "observer_start",
        "poll", "subscriber", "hotkeys", "unknown", "diagnostics", "hud_runtime"}) {
    scenario = {};
    scenario.fail = failure;
    CHECK(run() == AppExitDisposition::unload_allowed);
    CHECK(has("close_log"));
    CHECK(scenario.cleanup_order_valid);
    if (scenario.fail != "observer_construct") {
      CHECK(before("observer_stop", "overlay_stop"));
      CHECK(before("overlay_stop", "close_log"));
    }
  }
  for (const auto* failure : {"open_log", "log", "config", "icon"}) {
    scenario = {};
    scenario.fail = failure;
    CHECK(run() == AppExitDisposition::unload_allowed);
    CHECK(has("close_log"));
    CHECK(!has("overlay_start"));
    CHECK(!has("observer_start"));
  }
  scenario = {};
  scenario.icon_valid = false;
  CHECK(run() == AppExitDisposition::unload_allowed);
  CHECK(!has("observer_construct"));
  CHECK(has("close_log"));

  for (int source = 0; source < 4; ++source) {
    scenario = {};
    scenario.overlay_started = source == 0;
    scenario.observer_started = source == 1;
    scenario.overlay_retained = source == 2;
    scenario.observer_retained = source == 3;
    CHECK(run() == AppExitDisposition::retain_module);
    CHECK(scenario.cleanup_order_valid);
  }
  for (const auto* failure : {"overlay_start", "observer_start", "poll", "subscriber"}) {
    scenario = {};
    scenario.fail = failure;
    scenario.overlay_retained = true;
    scenario.observer_retained = true;
    CHECK(run() == AppExitDisposition::retain_module);
    CHECK(scenario.cleanup_order_valid);
    CHECK(has("close_log"));
  }
  scenario = {};
  scenario.drained = false;
  CHECK(run() == AppExitDisposition::retain_module);
  CHECK(!has("clear_logger"));
  CHECK(has("close_log"));
  scenario = {};
  scenario.hooks_disabled = false;
  CHECK(run() == AppExitDisposition::retain_module);
  CHECK(!has("clear_logger"));
  CHECK(has("close_log"));
  scenario = {};
  scenario.gpu_released = false;
  CHECK(run() == AppExitDisposition::retain_module);
  scenario = {};
  scenario.fail = "clear_logger";
  CHECK(run() == AppExitDisposition::retain_module);
  CHECK(has("close_log"));
  scenario = {};
  scenario.fail = "close_log";
  CHECK(run() == AppExitDisposition::retain_module);
}
