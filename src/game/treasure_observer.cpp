#include "game/treasure_observer.hpp"
#include "game/observer_hooks.hpp"
#include "game/hook_scan.hpp"
#include "game/patterns.hpp"
#include <windows.h>

#include <safetyhook.hpp>

#include <atomic>
#include <cstdint>
#include <sstream>
#include <string>
#include <utility>

namespace phi {

namespace {

// Hook callbacks never dereference an observer. Storage outlives every Impl and is shared
// by the single active observer in this linked library instance.
std::atomic<bool> g_observer_claimed{false};
std::atomic<bool> g_capturing{false};
std::atomic<std::uintptr_t> g_state_base{0};
std::atomic<std::uint64_t> g_hook_events{0};
std::atomic<std::uint32_t> g_pre_state{UINT32_MAX};

std::uint32_t read_pre_state(std::uintptr_t base) {
  if (!base || base > UINTPTR_MAX - patterns::kStateOffset - sizeof(std::uint32_t)) {
    return UINT32_MAX;
  }

  __try {
    return *reinterpret_cast<volatile const std::uint32_t*>(base + patterns::kStateOffset);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return UINT32_MAX;
  }
}

void capture_state(safetyhook::Context& ctx) {
  if (g_capturing.load(std::memory_order_acquire)) {
    g_pre_state.store(read_pre_state(ctx.rsi), std::memory_order_relaxed);
    g_state_base.store(ctx.rsi, std::memory_order_release);
    g_hook_events.fetch_add(1, std::memory_order_release);
  }
}

TreasureState read_state() {
  auto base = g_state_base.load(std::memory_order_acquire);

  if (!base || base > UINTPTR_MAX - patterns::kStateOffset - sizeof(std::uint32_t)) {
    return TreasureState::unknown;
  }

  const auto address = base + patterns::kStateOffset;
  MEMORY_BASIC_INFORMATION info{};

  if (
    !VirtualQuery(reinterpret_cast<void*>(address), &info, sizeof info) ||
    info.State != MEM_COMMIT ||
    (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
    address < reinterpret_cast<std::uintptr_t>(info.BaseAddress) ||
    address - reinterpret_cast<std::uintptr_t>(info.BaseAddress) > info.RegionSize ||
    info.RegionSize - (address - reinterpret_cast<std::uintptr_t>(info.BaseAddress)) <
      sizeof(std::uint32_t)
  ) {
    g_state_base.compare_exchange_strong(base, 0);

    return TreasureState::unknown;
  }

  __try {
    return *reinterpret_cast<volatile const std::uint32_t*>(address) > 0
      ? TreasureState::active
      : TreasureState::inactive;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    g_state_base.compare_exchange_strong(base, 0);

    return TreasureState::unknown;
  }
}

const char* state_name(TreasureState state) {
  switch (state) {
  case TreasureState::active:
    return "1";
  case TreasureState::inactive:
    return "0";
  default:
    return "unknown";
  }
}

} // namespace

struct TreasureObserver::Impl {
  LogCallback logger;
  detail::ObserverHooks<safetyhook::MidHook> hooks;
  Signal<TreasureStateChanged> changes;
  bool claimed = false;
  bool running = false;
  TreasureState current = TreasureState::unknown;
  TreasureState published = TreasureState::unknown;
  std::uint64_t last_events = 0;
  std::uintptr_t last_base = 0;
  ULONGLONG last_base_log = 0;

  explicit Impl(LogCallback callback)
    : logger(callback) {}

  void log(LogLevel level, const char* text) const {
    if (logger) {
      logger(level, text);
    }
  }
};

TreasureObserver::TreasureObserver(LogCallback logger)
  : impl_(std::make_unique<Impl>(logger)) {}

TreasureObserver::~TreasureObserver() {
  stop();
}

bool TreasureObserver::start() {
  auto& impl = *impl_;

  if (impl.running) {
    return true;
  }

  if (impl.hooks.result().module_must_remain_loaded) {
    impl.log(LogLevel::warn, "State hooks disabled: observer retired until process exit");

    return false;
  }

  bool unclaimed = false;

  if (!g_observer_claimed.compare_exchange_strong(unclaimed, true)) {
    impl.log(LogLevel::warn, "State hooks disabled: another treasure observer is active");

    return false;
  }

  impl.claimed = true;
  const auto module = GetModuleHandleW(L"CrimsonDesert.exe");
  const auto scan = find_treasure_hook_sites(module);

  if (scan.status != ScanStatus::found) {
    const char* reason = scan.status == ScanStatus::ambiguous
      ? "ambiguous"
      : scan.status == ScanStatus::no_match
        ? "no pair"
        : "invalid image";
    impl.log(LogLevel::warn, (std::string("State hooks disabled: ") + reason).c_str());
    stop();

    return false;
  }

  std::ostringstream found;
  found << "Unique instruction pair at RVA 0x" << std::hex
        << (scan.sites.enter - reinterpret_cast<std::uintptr_t>(module)) << " and RVA 0x"
        << (scan.sites.leave - reinterpret_cast<std::uintptr_t>(module));
  impl.log(LogLevel::debug, found.str().c_str());

  // Prepare both hooks before allowing either to execute callbacks.
  auto enter = safetyhook::MidHook::create(
    reinterpret_cast<void*>(scan.sites.enter),
    capture_state,
    safetyhook::MidHook::StartDisabled
  );

  if (!enter) {
    impl.log(LogLevel::error, "Enter mid-hook failed");
    stop();

    return false;
  }

  impl.hooks.hooks().enter = std::move(*enter);
  auto leave = safetyhook::MidHook::create(
    reinterpret_cast<void*>(scan.sites.leave),
    capture_state,
    safetyhook::MidHook::StartDisabled
  );

  if (!leave) {
    impl.log(LogLevel::error, "Leave mid-hook failed");
    stop();

    return false;
  }

  impl.hooks.hooks().leave = std::move(*leave);
  g_state_base.store(0);
  g_hook_events.store(0);
  g_pre_state.store(UINT32_MAX);
  impl.last_events = 0;
  impl.last_base = 0;
  impl.last_base_log = 0;
  g_capturing.store(true, std::memory_order_release);

  if (!impl.hooks.enable()) {
    impl.log(LogLevel::error, "State mid-hook activation failed");
    stop();

    return false;
  }

  impl.running = true;
  impl.log(LogLevel::info, "State mid-hooks active; waiting for RSI/state transitions");

  return true;
}

void TreasureObserver::poll() {
  auto& impl = *impl_;

  if (impl.running) {
    impl.current = read_state();
    const auto base = g_state_base.load(std::memory_order_acquire);
    const auto events = g_hook_events.load(std::memory_order_acquire);
    const auto now = GetTickCount64();

    if (events != impl.last_events) {
      const auto pre = g_pre_state.load(std::memory_order_relaxed);

      if (pre != UINT32_MAX) {
        std::ostringstream line;
        line << "Treasure pre-instruction state " << pre << " base=0x" << std::hex << base
             << std::dec << " hook_events=" << events;
        impl.log(LogLevel::debug, line.str().c_str());
      }

      impl.last_events = events;
    }

    if (
      impl.current != impl.published ||
      (base && base != impl.last_base && now - impl.last_base_log >= 1000)
    ) {
      std::ostringstream line;
      line << "Treasure state " << state_name(impl.current) << " base=0x" << std::hex << base
           << " state_address=0x" << (base ? base + patterns::kStateOffset : 0) << std::dec
           << " hook_events=" << events;
      impl.log(LogLevel::debug, line.str().c_str());
      impl.last_base_log = now;
    }

    impl.last_base = base;
  }

  if (impl.current != impl.published) {
    const TreasureStateChanged event{impl.published, impl.current};
    impl.published = impl.current;
    impl.changes.publish(event);
  }
}

ObserverStopResult TreasureObserver::stop() noexcept {
  auto& impl = *impl_;

  if (!impl.claimed) {
    return impl.hooks.result();
  }

  g_capturing.store(false, std::memory_order_release);
  const auto result = impl.hooks.stop();

  if (!result.module_must_remain_loaded) {
    g_observer_claimed.store(false, std::memory_order_release);
  }

  // Retired stubs must not capture into a new observer session; retain the claim.
  g_state_base.store(0);
  impl.running = false;
  impl.current = TreasureState::unknown;
  impl.claimed = false;

  return impl.hooks.result();
}

TreasureState TreasureObserver::state() const {
  return impl_->current;
}

Subscription
TreasureObserver::subscribe(std::function<void(const TreasureStateChanged&)> callback) {
  return impl_->changes.subscribe(std::move(callback));
}

} // namespace phi
