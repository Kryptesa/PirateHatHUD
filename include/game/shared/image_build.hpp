#pragma once

#include <cstdint>

namespace phi::detail {
constexpr bool is_exe_1_0_0_3576(uint32_t timestamp, uint32_t image_size) {
  return timestamp == 0x6AED2E85 && image_size == 0x13CC4000;
}

constexpr bool has_expanded_ui_and_audio(uint32_t timestamp, uint32_t image_size) {
  // EXE 1.0.0.3663 shares these two layouts with 3576. Its treasure frame differs.
  return is_exe_1_0_0_3576(timestamp, image_size) ||
    (timestamp == 0x6AF5D198 && image_size == 0x144E9000);
}
} // namespace phi::detail
