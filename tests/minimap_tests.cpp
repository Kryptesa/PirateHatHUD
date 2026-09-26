#include "game/minimap_memory.hpp"
#include <cstring>
#include <map>
#include <vector>
#define CHECK(c)                                                                                   \
  do {                                                                                             \
    if (!(c)) {                                                                                    \
      return __LINE__;                                                                             \
    }                                                                                              \
  } while (false)

int main() {
  using namespace phi;
  // Independent literal CE offsets: tests every dereference, including zero slots.
  constexpr uintptr_t module = 0x100000;
  constexpr uintptr_t offsets[] = {0x30, 0x18, 0x88, 0x78, 0,     0x30EB8, 0x28,
                                   0xA0, 0x10, 0x48, 0,    0x290, 0x18};
  std::map<uintptr_t, uintptr_t> pointers;
  std::vector<uintptr_t> locations;
  uintptr_t current = 0x200000;
  pointers[module + 0x6C8CC00] = current;
  locations.push_back(module + 0x6C8CC00);
  for (auto offset : offsets) {
    locations.push_back(current + offset);
    pointers[current + offset] = current + 0x100000;
    current += 0x100000;
  }
  const auto byte_address = current + 0xBE;
  uint8_t value = 1;
  uintptr_t failure = 0;
  auto read = [&](uintptr_t address, void* out, size_t size) {
    if (address == failure) {
      return false;
    }
    if (size == 1 && address == byte_address) {
      std::memcpy(out, &value, size);
      return true;
    }
    auto it = pointers.find(address);
    if (it == pointers.end() || size != sizeof(uintptr_t)) {
      return false;
    }
    std::memcpy(out, &it->second, size);
    return true;
  };
  CHECK(detail::sample_minimap(module, read) == MinimapState::visible);
  value = 0;
  CHECK(detail::sample_minimap(module, read) == MinimapState::hidden);
  value = 2;
  CHECK(detail::sample_minimap(module, read) == MinimapState::unknown);
  value = 1;
  for (auto address : locations) {
    failure = address;
    CHECK(detail::sample_minimap(module, read) == MinimapState::unknown);
  }
  failure = byte_address;
  CHECK(detail::sample_minimap(module, read) == MinimapState::unknown);
  failure = 0;
  CHECK(detail::sample_minimap(0, read) == MinimapState::unknown);
  CHECK(detail::sample_minimap(UINTPTR_MAX, read) == MinimapState::unknown);
  pointers[locations[0]] = UINTPTR_MAX;
  CHECK(detail::sample_minimap(module, read) == MinimapState::unknown);
  pointers[locations[0]] = 0;
  CHECK(detail::sample_minimap(module, read) == MinimapState::unknown);
  MinimapObserver observer;
  CHECK(observer.state() == MinimapState::unknown);
  unsigned changes = 0;
  auto subscription = observer.subscribe([&](const auto&) { ++changes; });
  CHECK(!observer.start()); // No CrimsonDesert.exe in this standalone test process.
  observer.poll();
  CHECK(observer.state() == MinimapState::unknown);
  CHECK(changes == 0);
  observer.stop();
  observer.stop();
  observer.poll();
  CHECK(!observer.start());
}
