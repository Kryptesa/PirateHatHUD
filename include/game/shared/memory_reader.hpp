#pragma once

#include "game/shared/address.hpp"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <type_traits>

namespace phi::detail {

// Internal read seam shared by production sampling and simulated-memory tests.
using MemoryReader = std::function<bool(uintptr_t, void*, size_t)>;

// Interpretation and pointer alignment remain the responsibility of each sampler.
// Discard value when reading fails.
template <typename T>
bool read_field(const MemoryReader& read, uintptr_t object, size_t offset, T& value) {
  static_assert(std::is_trivially_copyable_v<T>);
  uintptr_t address = 0;
  return read &&
    add_address(object, offset, address) &&
    sizeof(T) <= UINTPTR_MAX - address &&
    read(address, &value, sizeof(value));
}

// Reads only a nonempty range inside one committed, readable memory region.
// ReadProcessMemory handles protection changes after the preliminary query.
// Callers must discard the destination on failure; a failed read may be partial.
bool read_memory(uintptr_t address, void* destination, size_t size) noexcept;

} // namespace phi::detail
