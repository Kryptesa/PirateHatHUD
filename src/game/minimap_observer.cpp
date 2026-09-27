#include "game/minimap_observer.hpp"
#include "game/minimap_memory.hpp"
#include "game/memory_reader.hpp"
#include <Windows.h>
#include "game/ui_root_scan.hpp"

namespace phi {
struct MinimapObserver::Impl {
  Signal<MinimapStateChanged> changes;
  MinimapState sampled = MinimapState::unknown;
  MinimapState published = MinimapState::unknown;
  uintptr_t module = 0;
  detail::UiIdentityCache identities;
  bool running = false;
  bool logged_sample = false;
  LogCallback logger = nullptr;
};

MinimapObserver::MinimapObserver(LogCallback logger)
  : impl_(std::make_unique<Impl>()) {
  impl_->logger = logger;
}

MinimapObserver::~MinimapObserver() {
  stop();
}

bool MinimapObserver::start() {
  if (impl_->running) {
    return true;
  }

  impl_->module = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"CrimsonDesert.exe"));
  impl_->logged_sample = false;
  auto module = reinterpret_cast<HMODULE>(impl_->module);
  auto scan = find_ui_root_slot(module);
  impl_->running = scan.status == ScanStatus::found;

  if (impl_->running) {
    impl_->identities = detail::make_ui_identity_cache(reinterpret_cast<uintptr_t>(module));
    impl_->identities.root_slot = scan.root_slot;
  }

  if (impl_->logger) {
    impl_->logger(
      impl_->running ? LogLevel::info : LogLevel::warn,
      impl_->running ? "Minimap observer started; native canvas and ancestor draw gates"
                     : "Minimap observer unavailable: executable/root slot invalid"
    );
  }

  return impl_->running;
}

void MinimapObserver::poll() {
  impl_->sampled = impl_->running
    ? detail::sample_minimap(impl_->module, detail::read_memory, &impl_->identities)
    : MinimapState::unknown;

  if (impl_->logger && (!impl_->logged_sample || impl_->sampled != impl_->published)) {
    impl_->logged_sample = true;
    impl_->logger(
      LogLevel::debug,
      impl_->sampled == MinimapState::unknown     ? "Minimap sample unavailable; icon hidden"
        : impl_->sampled == MinimapState::visible ? "Minimap visible"
                                                  : "Minimap hidden"
    );
  }

  if (impl_->sampled != impl_->published) {
    const auto previous = impl_->published;
    impl_->published = impl_->sampled;
    impl_->changes.publish({previous, impl_->sampled});
  }
}

void MinimapObserver::stop() noexcept {
  impl_->running = false;
  impl_->module = 0;
  impl_->sampled = MinimapState::unknown;
}

MinimapState MinimapObserver::state() const {
  return impl_->sampled;
}

Subscription MinimapObserver::subscribe(std::function<void(const MinimapStateChanged&)> callback) {
  return impl_->changes.subscribe(std::move(callback));
}
} // namespace phi
