#include "game/minimap_observer.hpp"
#include "game/minimap_memory.hpp"
#include <Windows.h>
#include <limits>

namespace phi {
namespace {
bool read_memory(uintptr_t address, void* destination, size_t size) {
  if (!address || size > std::numeric_limits<uintptr_t>::max() - address) {
    return false;
  }
  MEMORY_BASIC_INFORMATION region{};
  if (!VirtualQuery(reinterpret_cast<const void*>(address), &region, sizeof(region)) ||
      region.State != MEM_COMMIT || (region.Protect & (PAGE_GUARD | PAGE_NOACCESS))) {
    return false;
  }
  const auto protection = region.Protect & 0xFF;
  if (protection != PAGE_READONLY && protection != PAGE_READWRITE && protection != PAGE_WRITECOPY &&
      protection != PAGE_EXECUTE_READ && protection != PAGE_EXECUTE_READWRITE &&
      protection != PAGE_EXECUTE_WRITECOPY) {
    return false;
  }
  const auto base = reinterpret_cast<uintptr_t>(region.BaseAddress);
  if (address < base || address - base > region.RegionSize ||
      size > region.RegionSize - (address - base)) {
    return false;
  }
  // ReadProcessMemory handles memory becoming inaccessible after VirtualQuery without
  // dereferencing a racing game pointer in our process code.
  SIZE_T copied = 0;
  return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address), destination,
                           size, &copied) &&
         copied == size;
}
} // namespace
struct MinimapObserver::Impl {
  Signal<MinimapStateChanged> changes;
  MinimapState sampled = MinimapState::unknown;
  MinimapState published = MinimapState::unknown;
  uintptr_t module = 0;
  bool running = false;
  bool logged_sample = false;
  void (*logger)(const char*) = nullptr;
};
MinimapObserver::MinimapObserver(void (*logger)(const char*)) : impl_(std::make_unique<Impl>()) {
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
  IMAGE_DOS_HEADER dos{};
  IMAGE_NT_HEADERS64 nt{};
  uintptr_t nt_address = 0;
  impl_->running =
      impl_->module && read_memory(impl_->module, &dos, sizeof(dos)) &&
      dos.e_magic == IMAGE_DOS_SIGNATURE && dos.e_lfanew > 0 &&
      detail::add_address(impl_->module, static_cast<uintptr_t>(dos.e_lfanew), nt_address) &&
      read_memory(nt_address, &nt, sizeof(nt)) && nt.Signature == IMAGE_NT_SIGNATURE &&
      nt.OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC &&
      nt.OptionalHeader.SizeOfImage >= detail::kMinimapRootRva + sizeof(uintptr_t);
  if (impl_->logger) {
    impl_->logger(impl_->running ? "Minimap observer started (RVA/chain tested on 2.03.02 only)"
                                 : "Minimap observer unavailable: executable/root slot invalid");
  }
  return impl_->running;
}
void MinimapObserver::poll() {
  impl_->sampled =
      impl_->running ? detail::sample_minimap(impl_->module, read_memory) : MinimapState::unknown;
  if (impl_->logger && (!impl_->logged_sample || impl_->sampled != impl_->published)) {
    impl_->logged_sample = true;
    impl_->logger(impl_->sampled == MinimapState::unknown
                      ? "Minimap sample unavailable; icon hidden"
                  : impl_->sampled == MinimapState::visible ? "Minimap visible"
                                                            : "Minimap hidden");
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
