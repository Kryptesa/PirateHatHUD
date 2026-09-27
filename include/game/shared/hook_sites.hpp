#pragma once

#include <cstdint>

namespace phi {
struct HookSites {
  uintptr_t enter = 0;
  uintptr_t leave = 0;
};
} // namespace phi
