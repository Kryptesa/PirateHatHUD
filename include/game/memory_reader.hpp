#pragma once

#include <cstddef>
#include <cstdint>

namespace phi::detail {

// Reads only a nonempty range inside one committed, readable memory region.
// ReadProcessMemory handles protection changes after the preliminary query.
// Callers must discard the destination on failure; a failed read may be partial.
bool read_memory(uintptr_t address, void* destination, size_t size) noexcept;

} // namespace phi::detail
