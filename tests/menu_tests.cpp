#include "game/menu_memory.hpp"
#include "game/menu_observer.hpp"
#include "game/menu_capture.hpp"
#include <iostream>
#include <map>
#include <vector>
#include <cstring>
#define CHECK(c)                                                                                   \
  do {                                                                                             \
    if (!(c)) {                                                                                    \
      return __LINE__;                                                                             \
    }                                                                                              \
  } while (false)
int main() {
  using namespace phi;
  constexpr uintptr_t module = 0x140000000;
  std::map<uintptr_t, std::vector<uint8_t>> memory;
  auto put = [&](uintptr_t address, const auto& value) {
    auto bytes = reinterpret_cast<const uint8_t*>(&value);
    memory[address] = {bytes, bytes + sizeof(value)};
  };
  auto ptr = [&](uintptr_t address, uintptr_t value) { put(address, value); };
  uintptr_t failed = 0;
  size_t reads = 0;
  auto read = [&](uintptr_t address, void* out, size_t size) {
    ++reads;
    auto it = memory.find(address);
    if (address == failed || it == memory.end() || it->second.size() != size) {
      return false;
    }
    std::memcpy(out, it->second.data(), size);
    return true;
  };
  detail::UiIdentityCache location;
  location.root_slot = module + 0x6C8CC00;
  uintptr_t owner = 0x100000;
  ptr(module + 0x6C8CC00, owner);
  for (auto offset : {0x30u, 0x18u, 0x88u, 0x78u, 0u}) {
    ptr(owner + offset, owner + 0x10000);
    owner += 0x10000;
  }
  ptr(owner + 0x30EB8, 0x200000);
  put(owner + 0x30EC0, uint32_t{1});
  ptr(0x200000, 0x300000);
  ptr(0x3000A0, 0x400000);
  ptr(0x400010, 0x500000);
  ptr(0x500118, 0x600000);
  ptr(0x600000, 0x700000);
  ptr(0x6FFFF8, module + 0x1000);
  std::array<uint32_t, 6> col{1, 0, 0, 0x2000, 0, 0x1000};
  put(module + 0x1000, col);
  const char name[] = ".?AVUIGamePlayControl_Root_MainMenu@uiCommonScript@pa@@";
  put(module + 0x2010, name);
  put(0x50025B, uint8_t{1});
  CHECK(detail::find_menu_root(module, read, &location) == 0x500000);
  CHECK(detail::sample_menu(0x500000, 0x25B, read) == MenuState::open);
  put(0x50025B, uint8_t{0});
  CHECK(detail::sample_menu(0x500000, 0x25B, read) == MenuState::closed);
  put(0x50025B, uint8_t{2});
  CHECK(detail::sample_menu(0x500000, 0x25B, read) == MenuState::unknown);
  CHECK(detail::sample_menu(0, 0x25B, read) == MenuState::unknown);
  CHECK(detail::sample_menu(UINTPTR_MAX, 0x25B, read) == MenuState::unknown);
  for (const auto& [address, bytes] : memory) {
    if (address == 0x50025B) {
      continue;
    }
    failed = address;
    CHECK(detail::find_menu_root(module, read, &location) == 0);
  }
  failed = 0;
  put(0x500380, uint8_t{1});
  CHECK(detail::sample_menu(0x500000, 0x380, read) == MenuState::open);
  CHECK(detail::sample_menu(0x500000, 0, read) == MenuState::unknown);
  // A moved slot is used directly; the historical RVA can disappear.
  memory[module + 0x8000] = memory[location.root_slot];
  memory.erase(location.root_slot);
  location.root_slot = module + 0x8000;
  CHECK(detail::find_menu_root(module, read, &location) == 0x500000);
  detail::UiIdentityCache unresolved;
  CHECK(detail::find_menu_root(module, read, &unresolved) == 0);
  put(owner + 0x30EC0, uint32_t{2});
  ptr(0x200008, 0x300000);
  CHECK(detail::find_menu_root(module, read, &location) == 0); // Ambiguous identity.
  // A valid menu cannot establish uniqueness when a neighboring identity is unreadable.
  ptr(0x200008, 0x310000);
  CHECK(detail::find_menu_root(module, read, &location) == 0);
  ptr(0x3100A0, 0x410000);
  CHECK(detail::find_menu_root(module, read, &location) == 0);
  ptr(0x410010, 0x510000);
  CHECK(detail::find_menu_root(module, read, &location) == 0);
  ptr(0x510118, 0x610000);
  CHECK(detail::find_menu_root(module, read, &location) == 0);
  ptr(0x610000, 0x710000);
  CHECK(detail::find_menu_root(module, read, &location) == 0);
  ptr(0x70FFF8, module + 0x3000);
  CHECK(detail::find_menu_root(module, read, &location) == 0);
  auto other_col = col;
  other_col[3] = 0x4000;
  other_col[5] = 0x3000;
  put(module + 0x3000, other_col);
  CHECK(detail::find_menu_root(module, read, &location) == 0);
  auto other_name = std::array<char, sizeof(name)>{};
  put(module + 0x4010, other_name);
  CHECK(detail::find_menu_root(module, read, &location) == 0x500000);
  // Successfully read null links are empty slots, rather than read failures.
  for (const auto address :
       {uintptr_t{0x200008}, uintptr_t{0x3100A0}, uintptr_t{0x410010}, uintptr_t{0x510118}}) {
    const auto original = memory[address];
    ptr(address, 0);
    CHECK(detail::find_menu_root(module, read, &location) == 0x500000);
    memory[address] = original;
  }
  for (const auto address :
       {uintptr_t{0x200008}, uintptr_t{0x3100A0}, uintptr_t{0x410010}, uintptr_t{0x510118},
        uintptr_t{0x610000}, uintptr_t{0x70FFF8}, module + 0x3000, module + 0x4010}) {
    failed = address;
    CHECK(detail::find_menu_root(module, read, &location) == 0);
  }
  failed = 0;
  // Warm immutable RTTI cache reduces read calls, while every live heap link is revalidated.
  detail::UiIdentityCache cache;
  cache.root_slot = location.root_slot;
  cache.immutable_ranges = {{0x6FFFF8, 0x10010}, {module + 0x1000, 0x4000}};
  reads = 0;
  CHECK(detail::find_menu_root(module, read, &cache) == 0x500000);
  const auto cold_reads = reads;
  reads = 0;
  CHECK(detail::find_menu_root(module, read, &cache) == 0x500000);
  const auto warm_reads = reads;
  CHECK(cold_reads == warm_reads + 6); // COL pointer, locator and name per distinct vtable.
  std::cout << "Menu identity read calls: cold=" << cold_reads << ", warm=" << warm_reads << '\n';
  failed = 0x510118;
  CHECK(detail::find_menu_root(module, read, &cache) == 0);
  failed = 0;
  ptr(0x610000, 0x700000); // Duplicate introduced after warming the cache.
  CHECK(detail::find_menu_root(module, read, &cache) == 0);
  ptr(0x200008, 0); // Removing the duplicate must restore the unique root.
  CHECK(detail::find_menu_root(module, read, &cache) == 0x500000);
  ptr(0x200000, 0); // Removing the cached menu invalidates its live identity immediately.
  CHECK(detail::find_menu_root(module, read, &cache) == 0);
  ptr(0x200000, 0x300000);
  ptr(0x400010, 0x520000); // Replaced heap root with the same immutable script type.
  ptr(0x520118, 0x600000);
  CHECK(detail::find_menu_root(module, read, &cache) == 0x520000);
  // A writable type name is re-read even when vtable/locator storage is immutable.
  detail::UiIdentityCache partial_cache;
  partial_cache.root_slot = location.root_slot;
  partial_cache.immutable_ranges = {{0x6FFFF8, 0x10010}, {module + 0x1000, sizeof(col)}};
  CHECK(detail::find_menu_root(module, read, &partial_cache) == 0x520000);
  reads = 0;
  CHECK(detail::find_menu_root(module, read, &partial_cache) == 0x520000);
  CHECK(reads == 15); // Eight owner/array reads, six live links, one writable name.
  failed = module + 0x2010;
  CHECK(detail::find_menu_root(module, read, &partial_cache) == 0);
  failed = 0;
  // Writable/unverified RTTI is never cached, so read failures remain visible on later polls.
  detail::UiIdentityCache uncached;
  uncached.root_slot = location.root_slot;
  CHECK(detail::find_menu_root(module, read, &uncached) == 0x520000);
  CHECK(uncached.types.empty());
  failed = module + 0x2010;
  CHECK(detail::find_menu_root(module, read, &uncached) == 0);
  failed = 0;
  // Synthetic larger UI: repeated non-menu types must still inspect every live entry.
  ptr(0x610000, 0x710000);
  constexpr uint32_t other_entries = 256;
  for (uint32_t i = 1; i <= other_entries; ++i) {
    ptr(0x200000 + i * sizeof(uintptr_t), 0x310000);
  }
  put(owner + 0x30EC0, other_entries + 1);
  reads = 0;
  CHECK(detail::find_menu_root(module, read, &location) == 0x520000);
  const auto uncached_reads = reads;
  reads = 0;
  CHECK(detail::find_menu_root(module, read, &cache) == 0x520000);
  CHECK(uncached_reads == reads + (other_entries + 1) * 3);
  std::cout << "257-entry menu identity read calls: uncached=" << uncached_reads
            << ", cached=" << reads << '\n';
  put(owner + 0x30EC0, uint32_t{4097});
  CHECK(detail::find_menu_root(module, read, &location) == 0);
  detail::MenuCapture capture;
  // Startup sampling and settled closing.
  auto before = capture.take();
  auto after = capture.take();
  CHECK(detail::menu_poll_state(MenuState::open, before, after) == MenuState::open);
  CHECK(detail::menu_poll_state(MenuState::closed, before, after) == MenuState::closed);
  // Rapid open/close before a poll still publishes open once and cancels return eligibility.
  capture.record(true);
  capture.record(false);
  before = capture.take();
  after = capture.take();
  CHECK(detail::menu_poll_state(MenuState::closed, before, after) == MenuState::open);
  before = capture.take();
  after = capture.take();
  CHECK(detail::menu_poll_state(MenuState::closed, before, after) == MenuState::closed);
  // Opening during sample, then closing before completion cannot lose the opening latch.
  before = capture.take();
  capture.record(true);
  capture.record(false);
  after = capture.take();
  CHECK(detail::menu_poll_state(MenuState::closed, before, after) == MenuState::open);
  // A closing pre-instruction callback during sample defers publication.
  before = capture.take();
  capture.record(false);
  after = capture.take();
  CHECK(!detail::menu_poll_state(MenuState::closed, before, after));
  // A late callback remains available to the next poll.
  capture.record(true);
  before = capture.take();
  after = capture.take();
  CHECK(detail::menu_poll_state(MenuState::closed, before, after) == MenuState::open);
  CHECK(detail::menu_poll_state(MenuState::unknown, before, after) == MenuState::unknown);
  // Every read phase fails closed when sampling cannot establish a current menu state.
  CHECK(detail::menu_poll_state(MenuState::unknown, 0, 2) == MenuState::unknown);
  capture.reset();
  CHECK(capture.take() == 0);
  MenuObserver observer;
  CHECK(observer.state() == MenuState::unknown);
  CHECK(!observer.start());
  observer.poll();
  CHECK(observer.state() == MenuState::unknown);
  CHECK(!observer.stop().module_must_remain_loaded);
  CHECK(!observer.start());
}
