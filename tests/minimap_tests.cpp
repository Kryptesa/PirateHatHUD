#include "game/minimap/memory.hpp"
#include "game/minimap_observer.hpp"
#include <array>
#include <cstring>
#include <limits>
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
  constexpr uintptr_t module = 0x140000000;
  std::map<uintptr_t, std::vector<uint8_t>> memory;
  auto put = [&](uintptr_t address, const auto& value) {
    const auto* bytes = reinterpret_cast<const uint8_t*>(&value);
    memory[address] = {bytes, bytes + sizeof(value)};
  };
  auto ptr = [&](uintptr_t address, uintptr_t value) { put(address, value); };
  auto text = [&](uintptr_t address, const char* value) {
    for (size_t i = 0; i <= std::strlen(value); ++i) {
      put(address + i, value[i]);
    }
  };
  uintptr_t failed = 0;
  size_t reads = 0;
  auto read = [&](uintptr_t address, void* out, size_t size) {
    ++reads;
    const auto it = memory.find(address);
    if (address == failed || it == memory.end() || it->second.size() < size) {
      return false;
    }
    std::memcpy(out, it->second.data(), size);
    return true;
  };
  detail::UiIdentityCache location;
  location.root_slot = module + 0x6C8CC00;
  uintptr_t owner = 0x100000;
  ptr(location.root_slot, owner);
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
  const char root_type[] = ".?AVUIGamePlayControlRootMiniMap@uiCommonScript@pa@@";
  put(module + 0x2010, root_type);

  constexpr uintptr_t canvas = 0x800000, first = 0x900000;
  constexpr uintptr_t body = first + 7 * 0x1000, view = first + 8 * 0x1000;
  ptr(0x500120, view);
  ptr(0x600008, body);
  ptr(0x6003F0, canvas);
  ptr(canvas + 8, first);
  ptr(first + 0xD8, canvas);
  ptr(first + 0xF8, 0xD00000);
  text(0xD00000, "MinimapCanvas");
  ptr(body + 0xF8, 0xD01000);
  text(0xD01000, "MinimapHudBody");
  ptr(view + 0xF8, 0xD02000);
  text(0xD02000, "MinimapView");
  for (size_t i = 0; i < 9; ++i) {
    const auto definition = first + i * 0x1000;
    const auto properties = 0xB00000 + i * 0x1000;
    put(definition + 0xB0, uint8_t{0xD0});
    put(definition + 0x97, uint8_t{0x59});
    ptr(definition + 0xC0, properties);
    put(properties + 0x40, 1.0f);
    if (i < 8) {
      ptr(definition + 0x38, definition + 0x1000);
    }
  }
  auto sample = [&] { return detail::sample_minimap(module, read, &location); };
  CHECK(sample() == MinimapState::visible);

  // Every sampled link and field is required, even when a previous ancestor is hidden.
  for (const auto& [address, bytes] : memory) {
    failed = address;
    CHECK(sample() == MinimapState::unknown);
  }
  failed = 0;
  for (size_t i = 0; i < 9; ++i) {
    const auto definition = first + i * 0x1000;
    const auto properties = 0xB00000 + i * 0x1000;
    // Setting: cleared draw bit. Menu/cutscene: rejected display mode despite positive opacity.
    for (auto flags : {0x50u, 0x90u, 0xF0u, 0x40u}) {
      put(definition + 0xB0, static_cast<uint8_t>(flags));
      CHECK(sample() == MinimapState::hidden);
    }
    put(definition + 0xB0, uint8_t{0xD0});
    put(definition + 0x97, uint8_t{0x79});
    CHECK(sample() == MinimapState::hidden);
    put(definition + 0x97, uint8_t{0x59});
    put(properties + 0x40, 0.0f);
    CHECK(sample() == MinimapState::hidden);
    for (float opacity : {0.0885f, 0.5f, 1.0f}) {
      put(properties + 0x40, opacity);
      CHECK(sample() == MinimapState::visible);
    }
    for (float opacity : {
           -0.1f,
           1.1f,
           std::numeric_limits<float>::infinity(),
           std::numeric_limits<float>::quiet_NaN(),
         }) {
      put(properties + 0x40, opacity);
      CHECK(sample() == MinimapState::unknown);
    }
    put(properties + 0x40, 1.0f);
  }
  put(first + 0xB0, uint8_t{0x50});
  failed = view + 0xC0;
  CHECK(sample() == MinimapState::unknown);
  failed = 0;
  put(first + 0xB0, uint8_t{0xD0});

  ptr(first + 0x38, first);
  CHECK(sample() == MinimapState::unknown);
  ptr(first + 0x38, 0);
  CHECK(sample() == MinimapState::unknown);
  ptr(first + 0x38, view); // An unrelated view cannot bypass the owning script body.
  CHECK(sample() == MinimapState::unknown);
  ptr(first + 0x38, first + 0x1000);
  ptr(first + 0xD8, canvas + 0x100);
  CHECK(sample() == MinimapState::unknown);
  ptr(first + 0xD8, canvas);
  text(0xD00000, "MinimapCanvasExtra");
  CHECK(sample() == MinimapState::unknown);
  text(0xD00000, "MinimapCanvas");
  ptr(first + 0xC0, 0);
  CHECK(sample() == MinimapState::unknown);
  ptr(first + 0xC0, 0xB00000);
  ptr(canvas + 8, UINTPTR_MAX - 4);
  CHECK(sample() == MinimapState::unknown);
  ptr(canvas + 8, first);

  // Outer order and replaced render roots do not change identity resolution.
  put(owner + 0x30EC0, uint32_t{2});
  ptr(0x200000, 0);
  ptr(0x200008, 0x300000);
  CHECK(sample() == MinimapState::visible);
  ptr(0x200000, 0x300000);
  CHECK(sample() == MinimapState::unknown);
  ptr(0x200000, 0);
  auto wrong_type = std::array<char, sizeof(root_type)>{};
  std::memcpy(wrong_type.data(), ".?AVUIGamePlayControlRootStatusGauge", 34);
  put(module + 0x2010, wrong_type);
  CHECK(sample() == MinimapState::unknown);
  put(module + 0x2010, root_type);
  ptr(0x400010, 0x520000);
  ptr(0x520118, 0x600000);
  ptr(0x520120, view);
  CHECK(sample() == MinimapState::visible);

  // A replacement canvas/definition/properties is read immediately without a heap cache.
  ptr(0x6003F0, 0x810000);
  ptr(0x810008, 0xA00000);
  ptr(0xA000D8, 0x810000);
  ptr(0xA000F8, 0xD00000);
  ptr(0xA00038, first + 0x1000);
  ptr(0xA000C0, 0xC00000);
  put(0xA000B0, uint8_t{0xD0});
  put(0xA00097, uint8_t{0x59});
  put(0xC00040, 0.0f);
  CHECK(sample() == MinimapState::hidden);
  put(0xC00040, 1.0f);
  CHECK(sample() == MinimapState::visible);
  ptr(0x6003F0, canvas);

  // Long/cyclic parent structures have a strict read budget.
  ptr(first + 0x38, 0xE00000);
  for (size_t i = 0; i < 24; ++i) {
    const auto node = 0xE00000 + i * 0x1000;
    ptr(node + 0x38, node + 0x1000);
    ptr(node + 0xC0, 0xB00000);
    put(node + 0xB0, uint8_t{0xD0});
    put(node + 0x97, uint8_t{0x59});
  }
  reads = 0;
  CHECK(sample() == MinimapState::unknown);
  CHECK(reads < 200);
  ptr(first + 0x38, first + 0x1000);
  CHECK(sample() == MinimapState::visible);
  ptr(0x6003F0, 0);
  CHECK(sample() == MinimapState::unknown);
  CHECK(detail::sample_minimap(0, read, &location) == MinimapState::unknown);
  CHECK(detail::sample_minimap(module, read, nullptr) == MinimapState::unknown);

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
