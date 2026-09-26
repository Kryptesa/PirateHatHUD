#include "game/menu_observer.hpp"
#include "game/menu_memory.hpp"
#include "game/observer_hooks.hpp"
#include "pattern_scan.hpp"
#include <Windows.h>
#include <safetyhook.hpp>
#include <atomic>
#include <limits>
namespace phi {
namespace {
std::atomic<bool> g_claimed{false};
std::atomic<bool> g_capturing{false};
std::atomic<uintptr_t> g_root{0};
// Latch every opening, including an open/close pair between owner-thread polls.
std::atomic<bool> g_opened{false};
std::atomic<bool> g_changed{false};
bool read_memory(uintptr_t address, void* destination, size_t size) {
  if (!address || size > std::numeric_limits<uintptr_t>::max() - address) {
    return false;
  }
  MEMORY_BASIC_INFORMATION region{};
  if (!VirtualQuery(reinterpret_cast<const void*>(address), &region, sizeof(region)) ||
      region.State != MEM_COMMIT || (region.Protect & (PAGE_GUARD | PAGE_NOACCESS))) {
    return false;
  }
  SIZE_T copied = 0;
  return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address), destination,
                           size, &copied) &&
         copied == size;
}
void capture_open(safetyhook::Context& ctx) {
  if (g_capturing.load(std::memory_order_acquire) &&
      ctx.rbx == g_root.load(std::memory_order_acquire)) {
    g_opened.store(true, std::memory_order_release);
    g_changed.store(true, std::memory_order_release);
  }
}
void capture_close(safetyhook::Context& ctx) {
  if (g_capturing.load(std::memory_order_acquire) &&
      ctx.rcx == g_root.load(std::memory_order_acquire)) {
    g_changed.store(true, std::memory_order_release);
  }
}
} // namespace
struct MenuObserver::Impl {
  detail::ObserverHooks<safetyhook::MidHook> hooks;
  Signal<MenuStateChanged> changes;
  MenuState current = MenuState::unknown;
  MenuState published = MenuState::unknown;
  uintptr_t module = 0;
  bool claimed = false;
  bool running = false;
  void (*logger)(const char*) = nullptr;
  void publish(MenuState value) {
    current = value;
    if (value != published) {
      auto previous = published;
      published = value;
      if (logger) {
        logger(value == MenuState::open     ? "Menu open"
               : value == MenuState::closed ? "Menu closed"
                                            : "Menu unknown; icon hidden");
      }
      changes.publish({previous, value});
    }
  }
};
MenuObserver::MenuObserver(void (*logger)(const char*)) : impl_(std::make_unique<Impl>()) {
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
  auto scan = find_hook_sites(module, true);
  if (scan.status != ScanStatus::found) {
    if (impl.logger) {
      impl.logger("Menu hooks unavailable: instruction pair missing, ambiguous or invalid image");
    }
    stop();
    return false;
  }
  impl.module = reinterpret_cast<uintptr_t>(module);
  auto root = detail::find_menu_root(impl.module, read_memory);
  g_root.store(root, std::memory_order_release);
  impl.current = detail::sample_menu(root, read_memory);
  auto clear = safetyhook::MidHook::create(reinterpret_cast<void*>(scan.sites.enter), capture_close,
                                           safetyhook::MidHook::StartDisabled);
  if (!clear) {
    stop();
    return false;
  }
  impl.hooks.hooks().enter = std::move(*clear);
  auto set = safetyhook::MidHook::create(reinterpret_cast<void*>(scan.sites.leave), capture_open,
                                         safetyhook::MidHook::StartDisabled);
  if (!set) {
    stop();
    return false;
  }
  impl.hooks.hooks().leave = std::move(*set);
  g_opened.store(false);
  g_changed.store(false);
  g_capturing.store(true, std::memory_order_release);
  if (!impl.hooks.enable()) {
    stop();
    return false;
  }
  impl.running = true;
  if (impl.logger) {
    impl.logger("Menu hooks active (2.03.02 UI root slot); identity resolved by script RTTI");
  }
  return true;
}
void MenuObserver::poll() {
  auto& impl = *impl_;
  if (!impl.running) {
    impl.publish(MenuState::unknown);
    return;
  }
  const auto root = detail::find_menu_root(impl.module, read_memory);
  g_root.store(root, std::memory_order_release);
  // Do not accept a closed read racing a pre-instruction hook; settle on the next poll.
  g_changed.exchange(false, std::memory_order_acq_rel);
  const auto sampled = detail::sample_menu(root, read_memory);
  const auto opened = g_opened.exchange(false, std::memory_order_acq_rel);
  if (opened) {
    impl.publish(MenuState::open);
  }
  if (g_changed.load(std::memory_order_acquire)) {
    return;
  }
  impl.publish(opened && sampled == MenuState::closed ? MenuState::open : sampled);
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
