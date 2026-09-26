#include "game/menu_memory.hpp"
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
  auto read = [&](uintptr_t address, void* out, size_t size) {
    auto it = memory.find(address);
    if (address == failed || it == memory.end() || it->second.size() != size) {
      return false;
    }
    std::memcpy(out, it->second.data(), size);
    return true;
  };
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
  CHECK(detail::find_menu_root(module, read) == 0x500000);
  CHECK(detail::sample_menu(0x500000, read) == MenuState::open);
  put(0x50025B, uint8_t{0});
  CHECK(detail::sample_menu(0x500000, read) == MenuState::closed);
  put(0x50025B, uint8_t{2});
  CHECK(detail::sample_menu(0x500000, read) == MenuState::unknown);
  CHECK(detail::sample_menu(0, read) == MenuState::unknown);
  CHECK(detail::sample_menu(UINTPTR_MAX, read) == MenuState::unknown);
  for (const auto& [address, bytes] : memory) {
    if (address == 0x50025B) {
      continue;
    }
    failed = address;
    CHECK(detail::find_menu_root(module, read) == 0);
  }
  failed = 0;
  put(owner + 0x30EC0, uint32_t{2});
  ptr(0x200008, 0x300000);
  CHECK(detail::find_menu_root(module, read) == 0); // Ambiguous identity.
  put(owner + 0x30EC0, uint32_t{4097});
  CHECK(detail::find_menu_root(module, read) == 0);
  MenuObserver observer;
  CHECK(observer.state() == MenuState::unknown);
  CHECK(!observer.start());
  observer.poll();
  CHECK(observer.state() == MenuState::unknown);
  CHECK(!observer.stop().module_must_remain_loaded);
  CHECK(!observer.start());
}
