#pragma once
#include <cstdint>
namespace phi::patterns {
inline constexpr std::uint8_t kEnter[] = {0xFF, 0x46, 0x08};
inline constexpr std::uint8_t kLeave[] = {0x83, 0x6E, 0x08, 0x01};
inline constexpr unsigned kExpectedDelta = 0x2C;
inline constexpr unsigned kStateOffset = 0x08;
}