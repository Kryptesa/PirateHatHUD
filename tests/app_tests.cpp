#include "app.hpp"
#include "game/treasure_observer.hpp"
#include "game/minimap_observer.hpp"
#include "game/menu_observer.hpp"
#include "overlay.hpp"
#include "platform/config.hpp"
#include "platform/hotkeys.hpp"
#include "platform/logger.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
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
  std::wstring config_path;
  std::wstring icon_path;
  bool embedded_icon = false;
  std::vector<std::string> calls;
  bool icon_valid = true;
  phi::Signal<phi::MinimapStateChanged>* minimap_signal = nullptr;
  unsigned minimap_subscriber_calls = 0;
  bool hud_visible = false;
  phi::MinimapState minimap_state = phi::MinimapState::visible;
  bool force_show = false;
  bool overlay_started = false;
  bool observer_started = false;
  bool observer_retained = false;
  bool menu_retained = false;
  phi::MenuState menu_state = phi::MenuState::closed;
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
struct MenuObserver::Impl {
  Signal<MenuStateChanged> changes;
  bool stopped = false;
};
MenuObserver::MenuObserver(LogCallback) {
  step("menu_construct");
  impl_ = std::make_unique<Impl>();
}
MenuObserver::~MenuObserver() {
  stop();
}
bool MenuObserver::start() {
  step("menu_start");
  return false;
}
void MenuObserver::poll() {
  step("menu_poll");
  impl_->changes.publish({MenuState::unknown, scenario.menu_state});
}
ObserverStopResult MenuObserver::stop() noexcept {
  if (!impl_->stopped) {
    impl_->stopped = true;
    scenario.calls.emplace_back("menu_stop");
  }
  return {true, scenario.menu_retained};
}
MenuState MenuObserver::state() const {
  return MenuState::unknown;
}
Subscription MenuObserver::subscribe(std::function<void(const MenuStateChanged&)> callback) {
  step("menu_subscribe");
  return impl_->changes.subscribe(std::move(callback));
}
struct MinimapObserver::Impl {
  Signal<MinimapStateChanged> changes;
  bool stopped = false;
};
MinimapObserver::MinimapObserver(LogCallback) {
  step("minimap_construct");
  impl_ = std::make_unique<Impl>();
  scenario.minimap_signal = &impl_->changes;
}
MinimapObserver::~MinimapObserver() {
  stop();
  scenario.minimap_signal = nullptr;
}
bool MinimapObserver::start() {
  step("minimap_start");
  return true;
}
void MinimapObserver::poll() {
  step("minimap_poll");
  impl_->changes.publish({MinimapState::unknown, scenario.minimap_state});
}
void MinimapObserver::stop() noexcept {
  if (!impl_->stopped) {
    impl_->stopped = true;
    scenario.calls.emplace_back("minimap_stop");
    scenario.cleanup_order_valid &= scenario.log_open;
  }
}
MinimapState MinimapObserver::state() const {
  return MinimapState::unknown;
}
Subscription MinimapObserver::subscribe(std::function<void(const MinimapStateChanged&)> callback) {
  step("minimap_subscribe");
  return impl_->changes.subscribe([callback = std::move(callback)](const auto& event) {
    ++scenario.minimap_subscriber_calls;
    step("minimap_subscriber");
    callback(event);
  });
}
struct TreasureObserver::Impl {
  Signal<TreasureStateChanged> changes;
  bool stopped = false;
};
TreasureObserver::TreasureObserver(LogCallback) {
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
bool open_log(const std::wstring&, const LogConfig&) {
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
void log(LogLevel, const char*) {
  step("log");
}
Config read_config(const std::wstring& path) {
  scenario.config_path = path;
  step("config");
  Config config;
  config.force_show = scenario.force_show;
  config.show_delay_ms = 0;
  return config;
}
HotkeyActions poll_hotkeys(int, int) {
  step("hotkeys");
  return {false, true};
}
bool prepare_overlay_icon(const wchar_t* path) {
  scenario.embedded_icon = path == nullptr;
  scenario.icon_path = path ? path : L"";
  step("icon");
  return scenario.icon_valid;
}
void set_overlay_hud(const HudState& hud) {
  step("hud");
  scenario.hud_visible = hud.visible;
}
void set_overlay_log(LogCallback logger) {
  step(logger ? "set_logger" : "clear_logger");
}
bool start_overlay() {
  step("overlay_start");
  return scenario.overlay_started;
}
OverlayStopResult stop_overlay() noexcept {
  scenario.calls.emplace_back("overlay_stop");
  scenario.cleanup_order_valid &= scenario.log_open && has("observer_stop");
  const auto minimap_callbacks = scenario.minimap_subscriber_calls;
  if (scenario.minimap_signal) {
    try {
      scenario.minimap_signal->publish({MinimapState::visible, MinimapState::unknown});
    } catch (...) {
      scenario.cleanup_order_valid = false;
    }
  }
  scenario.cleanup_order_valid &= minimap_callbacks == scenario.minimap_subscriber_calls;
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
  wchar_t module_path[MAX_PATH]{};
  CHECK(GetModuleFileNameW(nullptr, module_path, MAX_PATH) > 0);
  const auto folder = std::filesystem::path(module_path).parent_path();
  const auto config_path = folder / L"PirateHatHUD.ini";
  const auto legacy_path = folder / L"config.ini";
  const auto icon_path = folder / L"PirateHatHUD.png";
  if (!std::filesystem::exists(config_path) && !std::filesystem::exists(legacy_path) &&
      !std::filesystem::exists(icon_path)) {
    struct Fixtures {
      std::vector<std::filesystem::path> paths;
      ~Fixtures() {
        for (const auto& path : paths) {
          std::error_code error;
          std::filesystem::remove(path, error);
        }
      }
      void create(const std::filesystem::path& path) {
        paths.push_back(path);
        std::ofstream file(path);
        file << "fixture";
      }
    } fixtures;
    scenario = {};
    CHECK(run() == AppExitDisposition::unload_allowed);
    CHECK(scenario.config_path == config_path.wstring());
    CHECK(scenario.embedded_icon);
    fixtures.create(legacy_path);
    scenario = {};
    CHECK(run() == AppExitDisposition::unload_allowed);
    CHECK(scenario.config_path == legacy_path.wstring());
    fixtures.create(config_path);
    fixtures.create(icon_path);
    scenario = {};
    CHECK(run() == AppExitDisposition::unload_allowed);
    CHECK(scenario.config_path == config_path.wstring());
    CHECK(!scenario.embedded_icon && scenario.icon_path == icon_path.wstring());
  }
  scenario = {};
  CHECK(run() == AppExitDisposition::unload_allowed);
  CHECK(scenario.cleanup_order_valid);
  CHECK(scenario.hud_visible);
  CHECK(before("minimap_poll", "hotkeys"));
  CHECK(before("menu_poll", "hotkeys"));
  CHECK(before("menu_stop", "overlay_stop"));
  CHECK(before("minimap_stop", "overlay_stop"));
  CHECK(before("observer_stop", "overlay_stop"));
  CHECK(before("overlay_stop", "clear_logger"));
  CHECK(before("clear_logger", "close_log"));
  CHECK(std::count(scenario.calls.begin(), scenario.calls.end(), "observer_stop") == 1);
  CHECK(std::count(scenario.calls.begin(), scenario.calls.end(), "overlay_stop") == 1);

  for (const auto* failure :
       {"observer_construct", "subscribe", "hud", "set_logger", "overlay_start", "observer_start",
        "poll", "subscriber", "minimap_subscribe", "minimap_start", "minimap_poll",
        "minimap_subscriber", "menu_subscribe", "menu_start", "menu_poll", "hotkeys", "unknown",
        "diagnostics", "hud_runtime"}) {
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
  for (auto state : {phi::MinimapState::hidden, phi::MinimapState::unknown}) {
    scenario = {};
    scenario.minimap_state = state;
    scenario.force_show = true;
    CHECK(run() == AppExitDisposition::unload_allowed);
    CHECK(!scenario.hud_visible);
    CHECK(scenario.cleanup_order_valid);
  }
  for (auto state : {phi::MenuState::open, phi::MenuState::unknown}) {
    scenario = {};
    scenario.menu_state = state;
    scenario.force_show = true;
    CHECK(run() == AppExitDisposition::unload_allowed);
    CHECK(!scenario.hud_visible);
    CHECK(scenario.cleanup_order_valid);
  }
  scenario = {};
  scenario.fail = "minimap_construct";
  CHECK(run() == AppExitDisposition::unload_allowed);
  CHECK(has("observer_stop"));
  CHECK(!has("overlay_stop"));
  CHECK(has("close_log"));
  for (const auto* failure : {"open_log", "log", "config", "icon"}) {
    scenario = {};
    scenario.fail = failure;
    CHECK(run() == AppExitDisposition::unload_allowed);
    CHECK(has("close_log") || scenario.fail == "config");
    CHECK(!has("overlay_start"));
    CHECK(!has("observer_start"));
  }
  scenario = {};
  scenario.icon_valid = false;
  CHECK(run() == AppExitDisposition::unload_allowed);
  CHECK(!has("observer_construct"));
  CHECK(has("close_log"));

  for (int source = 0; source < 5; ++source) {
    scenario = {};
    scenario.overlay_started = source == 0;
    scenario.observer_started = source == 1;
    scenario.overlay_retained = source == 2;
    scenario.observer_retained = source == 3;
    scenario.menu_retained = source == 4;
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
