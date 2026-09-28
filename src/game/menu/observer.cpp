#include "game/menu_observer.hpp"
#include "game/menu/memory.hpp"
#include "game/menu/capture.hpp"
#include "game/shared/memory_reader.hpp"
#include "game/shared/observer_hooks.hpp"
#include "game/menu/scan.hpp"
#include "game/shared/ui_root_scan.hpp"
#include <Windows.h>
#include <safetyhook.hpp>
#include <atomic>

namespace phi {

namespace {
std::atomic<bool> g_claimed{false};
std::atomic<bool> g_capturing{false};
std::atomic<uintptr_t> g_root{0};
detail::MenuCapture g_events;

void capture_open(safetyhook::Context& ctx) {
  if (
    g_capturing.load(std::memory_order_acquire) && ctx.rbx == g_root.load(std::memory_order_acquire)
  ) {
    g_events.record(true);
  }
}

void capture_close(safetyhook::Context& ctx) {
  if (
    g_capturing.load(std::memory_order_acquire) && ctx.rcx == g_root.load(std::memory_order_acquire)
  ) {
    g_events.record(false);
  }
}
} // namespace

struct MenuObserver::Impl {
  detail::ObserverHooks<safetyhook::MidHook> hooks;
  Signal<MenuStateChanged> changes;
  MenuState current = MenuState::unknown;
  MenuState published = MenuState::unknown;
  uintptr_t module = 0;
  uintptr_t state_offset = 0;
  detail::UiIdentityCache identities;
  bool claimed = false;
  bool running = false;
  LogCallback logger = nullptr;

  void publish(MenuState value) {
    current = value;

    if (value != published) {
      auto previous = published;
      published = value;

      if (logger) {
        logger(
          LogLevel::debug,
          value == MenuState::open
            ? "Menu open"
            : value == MenuState::closed
              ? "Menu closed"
              : "Menu unknown; icon hidden"
        );
      }

      changes.publish({previous, value});
    }
  }

  void log_debug(const char* message) const {
    if (logger) {
      logger(LogLevel::debug, message);
    }
  }
};

MenuObserver::MenuObserver(LogCallback logger)
  : impl_(std::make_unique<Impl>()) {
  impl_->logger = logger;
}

MenuObserver::~MenuObserver() {
  stop();
}

bool MenuObserver::start() {
  auto& impl = *impl_;

  if (impl.running) {
    return true;
  }

  if (impl.hooks.result().module_must_remain_loaded) {
    return false;
  }

  bool available = false;

  if (!g_claimed.compare_exchange_strong(available, true)) {
    return false;
  }

  impl.claimed = true;
  auto module = GetModuleHandleW(L"CrimsonDesert.exe");
  impl.log_debug("Menu instruction scan begin");
  auto scan = find_menu_hook_sites(module);
  impl.log_debug("Menu instruction scan returned");

  if (scan.status != ScanStatus::found) {
    if (impl.logger) {
      impl.logger(
        LogLevel::warn,
        "Menu hooks unavailable: instruction pair missing, ambiguous or invalid image"
      );
    }

    stop();

    return false;
  }

  impl.module = reinterpret_cast<uintptr_t>(module);
  impl.log_debug("Menu UI slot scan begin");
  auto root_scan = find_ui_root_slot(module);
  impl.log_debug("Menu UI slot scan returned");

  if (root_scan.status != ScanStatus::found) {
    stop();

    return false;
  }

  impl.identities = detail::make_ui_identity_cache(reinterpret_cast<uintptr_t>(module));
  impl.identities.root_slot = root_scan.root_slot;
  impl.state_offset = scan.state_offset;
  impl.log_debug("Menu initial state sample begin");
  auto root = detail::find_menu_root(impl.module, detail::read_memory, &impl.identities);
  g_root.store(root, std::memory_order_release);
  impl.current = detail::sample_menu(root, impl.state_offset, detail::read_memory);
  impl.log_debug("Menu initial state sample ready");
  impl.log_debug("Menu enter hook creation begin");
  auto clear = safetyhook::MidHook::create(
    reinterpret_cast<void*>(scan.sites.enter),
    capture_close,
    safetyhook::MidHook::StartDisabled
  );

  if (!clear) {
    impl.log_debug("Menu enter hook creation failed");
    stop();

    return false;
  }

  impl.hooks.hooks().enter = std::move(*clear);
  impl.log_debug("Menu enter hook creation ready");
  impl.log_debug("Menu leave hook creation begin");
  auto set = safetyhook::MidHook::create(
    reinterpret_cast<void*>(scan.sites.leave),
    capture_open,
    safetyhook::MidHook::StartDisabled
  );

  if (!set) {
    impl.log_debug("Menu leave hook creation failed");
    stop();

    return false;
  }

  impl.hooks.hooks().leave = std::move(*set);
  impl.log_debug("Menu leave hook creation ready");
  g_events.reset();
  g_capturing.store(true, std::memory_order_release);

  if (!impl.hooks.enable(impl.logger, "Menu")) {
    stop();

    return false;
  }

  impl.running = true;

  if (impl.logger) {
    impl.logger(LogLevel::info, "Menu hooks active; UI slot and state offset resolved from code");
  }

  return true;
}

void MenuObserver::poll() {
  auto& impl = *impl_;

  if (!impl.running) {
    impl.publish(MenuState::unknown);
    return;
  }

  const auto root = detail::find_menu_root(impl.module, detail::read_memory, &impl.identities);
  g_root.store(root, std::memory_order_release);
  const auto before = g_events.take();
  const auto sampled = detail::sample_menu(root, impl.state_offset, detail::read_memory);
  const auto after = g_events.take();

  if (const auto value = detail::menu_poll_state(sampled, before, after)) {
    impl.publish(*value);
  }
}

ObserverStopResult MenuObserver::stop() noexcept {
  auto& impl = *impl_;

  if (!impl.claimed) {
    return impl.hooks.result();
  }

  g_capturing.store(false, std::memory_order_release);
  const auto result = impl.hooks.stop();

  if (!result.module_must_remain_loaded) {
    g_claimed.store(false, std::memory_order_release);
  }

  g_root.store(0);
  impl.running = false;
  impl.claimed = false;
  impl.current = MenuState::unknown;

  return result;
}

MenuState MenuObserver::state() const {
  return impl_->current;
}

Subscription MenuObserver::subscribe(std::function<void(const MenuStateChanged&)> callback) {
  return impl_->changes.subscribe(std::move(callback));
}
} // namespace phi
