#pragma once
#include <windows.h>
#include <cstdint>
#include <string_view>

namespace phi {
struct HookSites { std::uintptr_t enter = 0; std::uintptr_t leave = 0; };
// Returns empty on malformed patterns, no/ambiguous match, or layout mismatch.
HookSites find_hook_sites(HMODULE game, std::string_view enter, std::string_view leave);
}
