#pragma once

#include "game/range/memory.hpp"
#include "pattern_scan.hpp"

namespace phi {
PatternMatch scan_treasure_range_code(std::span<const uint8_t> code, uintptr_t base);
detail::RangeLocation find_treasure_range_location(HMODULE game);
} // namespace phi
