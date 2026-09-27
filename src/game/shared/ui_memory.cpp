#include "game/shared/ui_memory.hpp"
#include <Windows.h>
#include "game/shared/address.hpp"
#include <array>
#include <cstring>

namespace phi::detail {
bool UiIdentityCache::immutable(uintptr_t address, size_t size) const {
  for (const auto& range : immutable_ranges) {
    if (
      address >= range.begin &&
      address - range.begin <= range.size &&
      size <= range.size - (address - range.begin)
    ) {
      return true;
    }
  }

  return false;
}

UiIdentity identify_ui_script(
  uintptr_t module,
  uintptr_t vtable,
  const MemoryReader& read,
  std::string_view name,
  UiIdentityCache* cache
) {
  if (name.empty() || name.size() >= 256) {
    return UiIdentity::unknown;
  }

  if (cache && cache->expected_name != name) {
    cache->types.clear();
    cache->expected_name = name;
  }

  if (vtable < sizeof(uintptr_t)) {
    return UiIdentity::unknown;
  }

  uintptr_t address = 0;
  bool cache_locator = false;

  if (cache) {
    const auto found = cache->types.find(vtable);

    if (found != cache->types.end()) {
      if (found->second.identity != UiIdentity::unknown) {
        return found->second.identity;
      }

      address = found->second.name_address;
    }
  }

  if (!address) {
    uintptr_t col = 0;

    if (!read(vtable - sizeof(uintptr_t), &col, sizeof(col)) || !col) {
      return UiIdentity::unknown;
    }

    std::array<uint32_t, 6> locator{};

    if (!read(col, locator.data(), sizeof(locator))) {
      return UiIdentity::unknown;
    }

    // A readable object without game-module MSVC RTTI is a different identity.
    if (locator[0] != 1 || col < locator[5] || col - locator[5] != module) {
      return UiIdentity::other;
    }

    if (!add_address(module, locator[3], address) || !add_address(address, 16, address)) {
      return UiIdentity::unknown;
    }

    cache_locator = cache &&
      cache->immutable(vtable - sizeof(uintptr_t), sizeof(uintptr_t) * 2) &&
      cache->immutable(col, sizeof(locator));
  }

  std::array<char, 256> actual{};
  const auto size = name.size() + 1;

  if (!read(address, actual.data(), size)) {
    return UiIdentity::unknown;
  }

  const auto identity =
    std::memcmp(actual.data(), name.data(), name.size()) == 0 && actual[name.size()] == '\0'
      ? UiIdentity::matched
      : UiIdentity::other;

  if (cache_locator && cache->types.size() < 4096) {
    // MSVC type descriptors may live in writable .data: cache the immutable locator,
    // but continue reading the name on every poll unless its storage is immutable too.
    cache->types.emplace(
      vtable,
      UiCachedType{address, cache->immutable(address, size) ? identity : UiIdentity::unknown}
    );
  }

  return identity;
}

// Resolve current UI objects by exact MSVC script RTTI, never by a heap address or array index.
uintptr_t find_ui_root(
  uintptr_t module,
  const MemoryReader& read,
  std::string_view name,
  UiIdentityCache* cache
) {
  auto pointer = [&](uintptr_t base, uintptr_t offset, uintptr_t& value) {
    return read_field(read, base, offset, value);
  };

  uintptr_t owner = 0;

  if (
    !module ||
    !cache ||
    !cache->root_slot ||
    !read(cache->root_slot, &owner, sizeof(owner)) ||
    !owner
  ) {
    return 0;
  }

  for (auto offset : {0x30u, 0x18u, 0x88u, 0x78u, 0u}) {
    if (!pointer(owner, offset, owner) || !owner) {
      return 0;
    }
  }

  uintptr_t array = 0, address = 0;
  uint32_t count = 0;

  if (
    !pointer(owner, 0x30EB8, array) ||
    !array ||
    !add_address(owner, 0x30EC0, address) ||
    !read(address, &count, sizeof(count)) ||
    count == 0 ||
    count > 4096
  ) {
    return 0;
  }

  uintptr_t found = 0;

  for (uint32_t i = 0; i < count; ++i) {
    uintptr_t entry = 0, node = 0, root = 0, script = 0, vtable = 0;

    if (!pointer(array, i * sizeof(uintptr_t), entry)) {
      return 0;
    }

    // Null entries/links are legitimate empty objects, but failed reads invalidate uniqueness.
    if (!entry) {
      continue;
    }

    if (!pointer(entry, 0xA0, node)) {
      return 0;
    }

    if (!node) {
      continue;
    }

    if (!pointer(node, 0x10, root)) {
      return 0;
    }

    if (!root) {
      continue;
    }

    if (!pointer(root, 0x118, script)) {
      return 0;
    }

    if (!script) {
      continue;
    }

    if (!pointer(script, 0, vtable)) {
      return 0;
    }

    const auto identity = identify_ui_script(module, vtable, read, name, cache);

    if (identity == UiIdentity::unknown) {
      return 0;
    }

    if (identity == UiIdentity::other) {
      continue;
    }

    if (found) {
      return 0;
    }

    found = root;
  }

  return found;
}

// The game's loaded image survives this observer. Cache only non-writable image sections.
UiIdentityCache make_ui_identity_cache(uintptr_t module) {
  UiIdentityCache cache;
  IMAGE_DOS_HEADER dos{};
  IMAGE_NT_HEADERS64 nt{};
  uintptr_t nt_address = 0, section_address = 0;

  if (
    !read_memory(module, &dos, sizeof(dos)) ||
    dos.e_magic != IMAGE_DOS_SIGNATURE ||
    dos.e_lfanew <= 0 ||
    dos.e_lfanew > 0x1000 ||
    !add_address(module, dos.e_lfanew, nt_address) ||
    !read_memory(nt_address, &nt, sizeof(nt)) ||
    nt.Signature != IMAGE_NT_SIGNATURE ||
    nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
    nt.FileHeader.NumberOfSections == 0 ||
    nt.FileHeader.NumberOfSections > 96 ||
    !add_address(
      nt_address,
      offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader,
      section_address
    )
  ) {
    return cache;
  }

  for (unsigned i = 0; i < nt.FileHeader.NumberOfSections; ++i) {
    IMAGE_SECTION_HEADER section{};
    uintptr_t address = 0;

    if (
      !add_address(section_address, i * sizeof(section), address) ||
      !read_memory(address, &section, sizeof(section))
    ) {
      return {};
    }

    if (
      !(section.Characteristics & IMAGE_SCN_MEM_READ) ||
      (section.Characteristics & IMAGE_SCN_MEM_WRITE) ||
      section.VirtualAddress >= nt.OptionalHeader.SizeOfImage ||
      section.Misc.VirtualSize > nt.OptionalHeader.SizeOfImage - section.VirtualAddress
    ) {
      continue;
    }

    uintptr_t begin = 0;

    if (!add_address(module, section.VirtualAddress, begin)) {
      continue;
    }

    MEMORY_BASIC_INFORMATION region{};

    if (
      VirtualQuery(reinterpret_cast<const void*>(begin), &region, sizeof(region)) &&
      region.State == MEM_COMMIT &&
      (region.Protect == PAGE_READONLY || region.Protect == PAGE_EXECUTE_READ) &&
      begin >= reinterpret_cast<uintptr_t>(region.BaseAddress) &&
      begin - reinterpret_cast<uintptr_t>(region.BaseAddress) <= region.RegionSize &&
      section.Misc.VirtualSize <=
        region.RegionSize - (begin - reinterpret_cast<uintptr_t>(region.BaseAddress))
    ) {
      cache.immutable_ranges.push_back({begin, section.Misc.VirtualSize});
    }
  }

  return cache;
}
} // namespace phi::detail
