#include "game/memory_reader.hpp"

#include <Windows.h>
#include <limits>

namespace phi::detail {

bool read_memory(uintptr_t address, void* destination, size_t size) noexcept {
  if (!address || !destination || !size || size > std::numeric_limits<uintptr_t>::max() - address) {
    return false;
  }

  MEMORY_BASIC_INFORMATION region{};

  if (
    !VirtualQuery(reinterpret_cast<const void*>(address), &region, sizeof(region)) ||
    region.State != MEM_COMMIT ||
    (region.Protect & (PAGE_GUARD | PAGE_NOACCESS))
  ) {
    return false;
  }

  const auto protection = region.Protect & 0xFF;

  if (
    protection != PAGE_READONLY &&
    protection != PAGE_READWRITE &&
    protection != PAGE_WRITECOPY &&
    protection != PAGE_EXECUTE_READ &&
    protection != PAGE_EXECUTE_READWRITE &&
    protection != PAGE_EXECUTE_WRITECOPY
  ) {
    return false;
  }

  const auto base = reinterpret_cast<uintptr_t>(region.BaseAddress);

  if (
    address < base ||
    address - base > region.RegionSize ||
    size > region.RegionSize - (address - base)
  ) {
    return false;
  }

  SIZE_T copied = 0;

  const auto succeeded = ReadProcessMemory(
    GetCurrentProcess(),
    reinterpret_cast<const void*>(address),
    destination,
    size,
    &copied
  );

  return succeeded && copied == size;
}

} // namespace phi::detail
