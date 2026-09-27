#include "game/memory_reader.hpp"

#include <Windows.h>
#include <cstdint>
#include <limits>

#define CHECK(c)                                                                                   \
  do {                                                                                             \
    if (!(c)) {                                                                                    \
      return __LINE__;                                                                             \
    }                                                                                              \
  } while (false)

int main() {
  SYSTEM_INFO system{};
  GetSystemInfo(&system);
  const auto page_size = static_cast<size_t>(system.dwPageSize);

  struct Allocation {
    void* memory;
    ~Allocation() {
      if (memory) {
        VirtualFree(memory, 0, MEM_RELEASE);
      }
    }
  } allocation{VirtualAlloc(nullptr, page_size * 3, MEM_RESERVE, PAGE_NOACCESS)};
  CHECK(allocation.memory);

  auto* bytes = static_cast<uint8_t*>(allocation.memory);
  CHECK(VirtualAlloc(bytes, page_size * 2, MEM_COMMIT, PAGE_READWRITE));

  bytes[0] = 0x31;
  bytes[page_size - 1] = 0x42;
  bytes[page_size] = 0x53;
  const auto address = reinterpret_cast<uintptr_t>(bytes);
  uint8_t result[2]{};
  CHECK(phi::detail::read_memory(address, result, 1) && result[0] == 0x31);
  CHECK(phi::detail::read_memory(address + page_size - 1, result, 2));
  CHECK(result[0] == 0x42 && result[1] == 0x53);
  CHECK(!phi::detail::read_memory(0, result, 1));
  CHECK(!phi::detail::read_memory(address, nullptr, 1));
  CHECK(!phi::detail::read_memory(address, result, 0));
  CHECK(!phi::detail::read_memory(std::numeric_limits<uintptr_t>::max(), result, 2));
  CHECK(!phi::detail::read_memory(address + page_size * 2, result, 1));

  DWORD previous = 0;
  CHECK(VirtualProtect(bytes + page_size, page_size, PAGE_READONLY, &previous));
  CHECK(phi::detail::read_memory(address + page_size, result, 1) && result[0] == 0x53);

  // Both pages are readable, but a read must stay inside the queried region.
  CHECK(!phi::detail::read_memory(address + page_size - 1, result, 2));
  CHECK(VirtualProtect(bytes + page_size, page_size, PAGE_NOACCESS, &previous));
  CHECK(!phi::detail::read_memory(address + page_size, result, 1));
  CHECK(VirtualProtect(bytes + page_size, page_size, PAGE_READWRITE | PAGE_GUARD, &previous));
  CHECK(!phi::detail::read_memory(address + page_size, result, 1));

  // A rejected guard read must not consume the game's one-shot guard protection.
  MEMORY_BASIC_INFORMATION region{};
  CHECK(VirtualQuery(bytes + page_size, &region, sizeof(region)));
  CHECK(region.Protect & PAGE_GUARD);
  CHECK(VirtualProtect(bytes + page_size, page_size, PAGE_EXECUTE, &previous));
  CHECK(!phi::detail::read_memory(address + page_size, result, 1));
  CHECK(VirtualProtect(bytes + page_size, page_size, PAGE_EXECUTE_READ, &previous));
  CHECK(phi::detail::read_memory(address + page_size, result, 1) && result[0] == 0x53);
}
