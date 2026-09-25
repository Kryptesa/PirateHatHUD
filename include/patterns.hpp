#pragma once
#include <string_view>

namespace phi::patterns {
// Replace BOTH placeholders with unique, verified .text signatures from YOUR game build.
// See README.md. The hook stays disabled while either placeholder remains.
inline constexpr std::string_view kEnter = "PLACEHOLDER_PATTERN";
inline constexpr std::string_view kLeave = "PLACEHOLDER_PATTERN";
inline constexpr unsigned kExpectedDelta = 0x2c; // 0x125BD9A - 0x125BD6E
inline constexpr unsigned kStateOffset = 0x08;
}
