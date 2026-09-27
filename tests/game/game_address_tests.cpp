#include "game/shared/address.hpp"
#include "game/shared/memory_reader.hpp"
#include <array>
#include <cstring>

#define CHECK(c)                                                                                   \
  do {                                                                                             \
    if (!(c)) {                                                                                    \
      return __LINE__;                                                                             \
    }                                                                                              \
  } while (false)

int main() {
  using namespace phi::detail;
  std::array<uint8_t, 32> code{};
  uintptr_t result = 0;
  auto operand = [&](int32_t value) { std::memcpy(code.data() + 5 + 3, &value, sizeof(value)); };
  operand(64);
  CHECK(resolve_rip_address(code, 0x1000, 5, 3, 7, result));
  CHECK(result == 0x104C);
  operand(-64);
  CHECK(resolve_rip_address(code, 0x1000, 5, 3, 7, result));
  CHECK(result == 0xFCC);
  operand(INT32_MIN);
  CHECK(resolve_rip_address(code, 0x80001000, 5, 3, 7, result));
  CHECK(result == 0x100C);
  CHECK(!resolve_rip_address(code, 0x1000, 5, 3, 7, result));
  operand(INT32_MAX);
  CHECK(!resolve_rip_address(code, UINTPTR_MAX - 100, 5, 3, 7, result));
  operand(0);
  CHECK(!resolve_rip_address(code, UINTPTR_MAX - 10, 5, 3, 7, result));
  CHECK(!resolve_rip_address(code, 0x1000, SIZE_MAX, 3, 7, result));
  CHECK(!resolve_rip_address(code, 0x1000, 26, 3, 7, result));
  CHECK(!resolve_rip_address(code, 0x1000, 0, SIZE_MAX, 7, result));
  CHECK(!resolve_rip_address(code, 0x1000, 0, 4, 7, result));

  unsigned reads = 0;
  const MemoryReader read = [&](uintptr_t address, void* destination, size_t size) {
    ++reads;
    if (address != 0x1010 || size != sizeof(uint32_t)) {
      return false;
    }
    const uint32_t value = 42;
    std::memcpy(destination, &value, size);
    return true;
  };
  uint32_t value = 0;
  CHECK(read_field(read, 0x1000, 0x10, value));
  CHECK(value == 42 && reads == 1);
  CHECK(!read_field(read, 0, 0x1010, value));
  CHECK(!read_field(read, UINTPTR_MAX - 8, 9, value));
  CHECK(!read_field(read, UINTPTR_MAX - 2, 0, value));
  CHECK(!read_field(MemoryReader{}, 0x1000, 0x10, value));
  CHECK(reads == 1);
  CHECK(!read_field(read, 0x1000, 0x20, value));
  CHECK(reads == 2);
  return 0;
}
