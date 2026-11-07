#pragma once

#include <cstdint>

namespace phi::detail {
constexpr bool is_exe_1_0_0_3576(uint32_t timestamp, uint32_t image_size) {
  return timestamp == 0x6AED2E85 && image_size == 0x13CC4000;
}
} // namespace phi::detail
