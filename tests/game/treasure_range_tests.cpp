#include "game/range/memory.hpp"
#include "game/range/scan.hpp"
#include <Windows.h>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <string_view>
#include <vector>

#define CHECK(c)                                                                                   \
  do {                                                                                             \
    if (!(c)) {                                                                                    \
      return __LINE__;                                                                             \
    }                                                                                              \
  } while (false)

namespace {
struct Fixture {
  phi::detail::RangeLocation location{0x100100, 0x100000, 0x10000, 0x101000};
  std::map<uintptr_t, uint8_t> memory;
  unsigned writes = 0;
  bool reject_write = false;

  void put_bytes(uintptr_t address, const void* value, size_t size) {
    const auto* bytes = static_cast<const uint8_t*>(value);
    for (size_t i = 0; i < size; ++i) {
      memory[address + i] = bytes[i];
    }
  }
  template <typename T> void put(uintptr_t address, const T& value) {
    put_bytes(address, &value, sizeof(value));
  }
  bool read(uintptr_t address, void* destination, size_t size) const {
    auto* bytes = static_cast<uint8_t*>(destination);
    for (size_t i = 0; i < size; ++i) {
      const auto it = memory.find(address + i);
      if (it == memory.end()) {
        return false;
      }
      bytes[i] = it->second;
    }
    return true;
  }
  auto reader() {
    return [this](auto address, auto destination, auto size) {
      return read(address, destination, size);
    };
  }
  auto exchange() {
    return [this](uintptr_t address, float expected, float desired) {
      float current = 0;
      if (reject_write || !read(address, &current, sizeof(current)) || current != expected) {
        return false;
      }
      put(address, desired);
      ++writes;
      return true;
    };
  }
  float radius(uintptr_t object = 0x500000) const {
    float value = 0;
    read(object + 0x70, &value, sizeof(value));
    return value;
  }
  void type(uintptr_t vtable, uintptr_t locator, uintptr_t descriptor, const char* name) {
    put(vtable - 8, locator);
    put(
      locator,
      std::array<uint32_t, 6>{
        1,
        0,
        0,
        static_cast<uint32_t>(descriptor - location.image_base),
        0,
        static_cast<uint32_t>(locator - location.image_base)
      }
    );
    put_bytes(descriptor + 16, name, std::strlen(name) + 1);
  }
  void descriptor(uintptr_t object, uintptr_t keys, float radius, bool hat) {
    put(object, location.data_vtable);
    put(object + 0x40, hat ? uint32_t{0x52029BCF} : uint32_t{0});
    put(object + 0x4C, uint8_t{0xC0});
    put(object + 0x4E, uint8_t{2});
    put(object + 0x58, uint8_t{1});
    put(object + 0x60, keys);
    put(object + 0x68, uint32_t{2});
    put(object + 0x6C, uint32_t{2});
    put(object + 0x70, radius);
    put(object + 0x74, uint32_t{0x9B28835C});
    put(object + 0x78, uint32_t{0xCC908146});
    put(object + 0x7C, static_cast<uint8_t>(hat ? 1 : 2));
    put(keys, hat ? std::array<uint32_t, 2>{18, 19} : std::array<uint32_t, 2>{16, 17});
  }
  Fixture() {
    type(0x101000, 0x102000, 0x103000, ".?AVGimmickEventHandlerData_SearchNearLevelGimmick@pa@@");
    type(
      0x104000,
      0x105000,
      0x106000,
      ".?AV?$CompressedObjectMemoryPool@VGimmickEventHandlerData_SearchNearLevelGimmick@pa@@"
      "W4GimmickChartObjectType@2@@pa@@"
    );
    put(location.pool_slot, uintptr_t{0x400000});
    put(0x400000, uintptr_t{0x104000});
    put(0x400008, uint32_t{2});
    put(0x40000C, uint32_t{2});
    put(0x400010, uintptr_t{0x410000});
    put(0x400018, uint32_t{1});
    put(0x400054, uint32_t{2});
    put(0x410000, uintptr_t{0x500000});
    descriptor(0x500000, 0x600000, 15, true);
    descriptor(0x500080, 0x600100, 5, false);
  }
};

std::vector<uint8_t> loader() {
  std::vector<uint8_t> code(0xC0, 0x90);
  constexpr uint8_t anchor[] =
    {0x41, 0xBC, 0xC2, 0, 0, 0, 0x41, 0xBF, 1, 0, 0, 0, 0x45, 0x39, 0x6E, 0x54};
  constexpr uint8_t registry[] = {
    0x41,
    0x8B,
    0xD4,
    0x48,
    0x8D,
    0x0D,
    5,
    0,
    0,
    0,
    0xE8,
    0,
    0,
    0,
    0,
    0x48,
    0x8B,
    0xC8,
    0x48,
    0x85,
    0xC0
  };
  constexpr uint8_t vtable[] = {0x48, 0x8D, 0x05, 8, 0, 0, 0, 0x48, 0x89, 0x03};
  std::memcpy(code.data(), anchor, sizeof(anchor));
  std::memcpy(code.data() + 0x51, registry, sizeof(registry));
  std::memcpy(code.data() + 0xA9, vtable, sizeof(vtable));
  return code;
}
} // namespace

int main() {
  using namespace phi::detail;
  Fixture fixture;
  auto target = find_range_target(fixture.location, fixture.reader());
  CHECK(target && target->object == 0x500000 && target->radius == 15);
  CHECK(fixture.writes == 0);
  for (const auto& field :
    std::array<std::pair<uintptr_t, uint32_t>, 6>{
      {{0x400054, 4097}, {0x400008, 0}, {0x400018, 65}, {0x500040, 0}, {0x500074, 0}, {0x50007C, 2}}
    }) {
    auto broken = fixture;
    broken.put(field.first, field.second);
    CHECK(!find_range_target(broken.location, broken.reader()));
  }
  auto broken = fixture;
  broken.put(0x106010, 'x');
  CHECK(!find_range_target(broken.location, broken.reader()));
  broken = fixture;
  broken.put(0x500070, std::numeric_limits<float>::quiet_NaN());
  CHECK(!find_range_target(broken.location, broken.reader()));
  broken = fixture;
  broken.descriptor(0x500080, 0x600100, 15, true);
  CHECK(!find_range_target(broken.location, broken.reader()));
  RangeOverride ambiguous;
  CHECK(
    ambiguous.update(broken.location, broken.reader(), broken.exchange(), 30) ==
    RangeStatus::waiting
  );
  CHECK(broken.writes == 0);

  // Objects in a subsequent block are found without scanning spare/uninitialized slots.
  broken = fixture;
  broken.put(0x400054, uint32_t{3});
  broken.put(0x400018, uint32_t{2});
  broken.put(0x410008, uintptr_t{0x700000});
  broken.put(0x500040, uint32_t{0});
  broken.descriptor(0x700000, 0x600200, 15, true);
  target = find_range_target(broken.location, broken.reader());
  CHECK(target && target->object == 0x700000);

  // A changed second sample rejects torn chart data before writing.
  unsigned reads = 0;
  CHECK(!find_range_target(fixture.location, [&](auto a, auto d, auto n) {
    if (a == fixture.location.pool_slot && ++reads == 3) {
      fixture.put(0x500070, 16.0f);
    }
    return fixture.read(a, d, n);
  }));
  fixture.put(0x500070, 15.0f);

  RangeOverride override;
  CHECK(
    override.update(fixture.location, fixture.reader(), fixture.exchange(), 0) ==
    RangeStatus::disabled
  );
  CHECK(fixture.writes == 0);
  CHECK(
    override.update(fixture.location, fixture.reader(), fixture.exchange(), 30) ==
    RangeStatus::applied
  );
  CHECK(fixture.radius() == 30 && fixture.radius(0x500080) == 5 && override.original() == 15);
  const auto writes = fixture.writes;
  CHECK(
    override.update(fixture.location, fixture.reader(), fixture.exchange(), 30) ==
    RangeStatus::applied
  );
  CHECK(fixture.writes == writes);
  CHECK(
    override.update(fixture.location, fixture.reader(), fixture.exchange(), 40) ==
    RangeStatus::applied
  );
  CHECK(override.original() == 15);
  CHECK(
    override.update(fixture.location, fixture.reader(), fixture.exchange(), 0) ==
    RangeStatus::disabled
  );
  CHECK(fixture.radius() == 15 && fixture.radius(0x500080) == 5);

  fixture.reject_write = true;
  CHECK(
    override.update(fixture.location, fixture.reader(), fixture.exchange(), 30) ==
    RangeStatus::write_failed
  );
  CHECK(fixture.radius() == 15);
  fixture.reject_write = false;
  CHECK(
    override.update(fixture.location, fixture.reader(), fixture.exchange(), 30) ==
    RangeStatus::applied
  );
  fixture.reject_write = true;
  CHECK(
    override.restore(fixture.location, fixture.reader(), fixture.exchange()) ==
    RangeStatus::write_failed
  );
  fixture.reject_write = false;
  CHECK(
    override.restore(fixture.location, fixture.reader(), fixture.exchange()) ==
    RangeStatus::disabled
  );
  CHECK(fixture.radius() == 15);

  CHECK(
    override.update(fixture.location, fixture.reader(), fixture.exchange(), 30) ==
    RangeStatus::applied
  );
  fixture.put(0x500070, 45.0f); // A third-party write must survive polling and shutdown.
  CHECK(
    override.update(fixture.location, fixture.reader(), fixture.exchange(), 30) ==
    RangeStatus::conflict
  );
  CHECK(
    override.update(fixture.location, fixture.reader(), fixture.exchange(), 30) ==
    RangeStatus::conflict
  );
  CHECK(
    override.restore(fixture.location, fixture.reader(), fixture.exchange()) ==
    RangeStatus::disabled
  );
  CHECK(fixture.radius() == 45);
  CHECK(
    override.update(fixture.location, fixture.reader(), fixture.exchange(), 60) ==
    RangeStatus::applied
  );
  CHECK(override.original() == 45);
  CHECK(
    override.restore(fixture.location, fixture.reader(), fixture.exchange()) ==
    RangeStatus::disabled
  );
  CHECK(fixture.radius() == 45);

  CHECK(
    override.update(fixture.location, fixture.reader(), fixture.exchange(), 30) ==
    RangeStatus::applied
  );
  fixture.memory.erase(0x400054);
  CHECK(
    override.update(fixture.location, fixture.reader(), fixture.exchange(), 0) ==
    RangeStatus::waiting
  );
  fixture.put(0x400054, uint32_t{2});
  CHECK(
    override.update(fixture.location, fixture.reader(), fixture.exchange(), 0) ==
    RangeStatus::disabled
  );
  CHECK(fixture.radius() == 45);

  fixture.put(0x500070, 15.0f);
  CHECK(
    override.update(fixture.location, fixture.reader(), fixture.exchange(), 30) ==
    RangeStatus::applied
  );
  fixture.put(0x410000, uintptr_t{0x700000});
  fixture.descriptor(0x700000, 0x600200, 15, true);
  fixture.descriptor(0x700080, 0x600300, 5, false);
  CHECK(
    override.update(fixture.location, fixture.reader(), fixture.exchange(), 30) ==
    RangeStatus::applied
  );
  CHECK(fixture.radius() == 30 && fixture.radius(0x700000) == 30);
  CHECK(
    override.restore(fixture.location, fixture.reader(), fixture.exchange()) ==
    RangeStatus::disabled
  );
  CHECK(
    fixture.radius() == 30 && fixture.radius(0x700000) == 15
  ); // Old address never written again.

  auto code = loader();
  CHECK(phi::scan_treasure_range_code(code, 0x100000).status == phi::ScanStatus::found);
  auto duplicate = code;
  duplicate.insert(duplicate.end(), code.begin(), code.end());
  CHECK(phi::scan_treasure_range_code(duplicate, 0x100000).status == phi::ScanStatus::ambiguous);
  code.resize(0xB0);
  CHECK(phi::scan_treasure_range_code(code, 0x100000).status == phi::ScanStatus::no_match);
  code = loader();
  code[2] = 0xC1;
  CHECK(phi::scan_treasure_range_code(code, 0x100000).status == phi::ScanStatus::no_match);
  code = loader();
  code[0xAC] = 9; // Misaligned vtable operand.
  CHECK(phi::scan_treasure_range_code(code, 0x100000).status == phi::ScanStatus::no_match);

  // Real guarded writes refuse wrong expected values and read-only memory.
  auto* page =
    static_cast<float*>(VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
  CHECK(page);
  *page = 15;
  const auto address = reinterpret_cast<uintptr_t>(page);
  CHECK(exchange_range_float(address, 15, 30) && *page == 30);
  CHECK(!exchange_range_float(address, 15, 40) && *page == 30);
  CHECK(!exchange_range_float(address + 1, 30, 40));
  DWORD old = 0;
  CHECK(VirtualProtect(page, 4096, PAGE_READONLY, &old));
  CHECK(!exchange_range_float(address, 30, 15) && *page == 30);
  CHECK(VirtualFree(page, 0, MEM_RELEASE));
  CHECK(!exchange_range_float(address, 30, 15));
}
