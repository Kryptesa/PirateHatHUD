#include "game/ui_memory.hpp"
#include <Windows.h>
#include "game/memory_reader.hpp"
namespace phi::detail {
// The game's loaded image survives this observer. Cache only non-writable image sections.
UiIdentityCache make_ui_identity_cache(uintptr_t module) {
  UiIdentityCache cache;
  IMAGE_DOS_HEADER dos{};
  IMAGE_NT_HEADERS64 nt{};
  uintptr_t nt_address = 0, section_address = 0;
  if (!read_memory(module, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE ||
      dos.e_lfanew <= 0 || dos.e_lfanew > 0x1000 ||
      !add_address(module, dos.e_lfanew, nt_address) || !read_memory(nt_address, &nt, sizeof(nt)) ||
      nt.Signature != IMAGE_NT_SIGNATURE ||
      nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
      nt.FileHeader.NumberOfSections == 0 || nt.FileHeader.NumberOfSections > 96 ||
      !add_address(nt_address,
                   offsetof(IMAGE_NT_HEADERS64, OptionalHeader) +
                       nt.FileHeader.SizeOfOptionalHeader,
                   section_address)) {
    return cache;
  }
  for (unsigned i = 0; i < nt.FileHeader.NumberOfSections; ++i) {
    IMAGE_SECTION_HEADER section{};
    uintptr_t address = 0;
    if (!add_address(section_address, i * sizeof(section), address) ||
        !read_memory(address, &section, sizeof(section))) {
      return {};
    }
    if (!(section.Characteristics & IMAGE_SCN_MEM_READ) ||
        (section.Characteristics & IMAGE_SCN_MEM_WRITE) ||
        section.VirtualAddress >= nt.OptionalHeader.SizeOfImage ||
        section.Misc.VirtualSize > nt.OptionalHeader.SizeOfImage - section.VirtualAddress) {
      continue;
    }
    uintptr_t begin = 0;
    if (!add_address(module, section.VirtualAddress, begin)) {
      continue;
    }
    MEMORY_BASIC_INFORMATION region{};
    if (VirtualQuery(reinterpret_cast<const void*>(begin), &region, sizeof(region)) &&
        region.State == MEM_COMMIT &&
        (region.Protect == PAGE_READONLY || region.Protect == PAGE_EXECUTE_READ) &&
        begin >= reinterpret_cast<uintptr_t>(region.BaseAddress) &&
        begin - reinterpret_cast<uintptr_t>(region.BaseAddress) <= region.RegionSize &&
        section.Misc.VirtualSize <=
            region.RegionSize - (begin - reinterpret_cast<uintptr_t>(region.BaseAddress))) {
      cache.immutable_ranges.push_back({begin, section.Misc.VirtualSize});
    }
  }
  return cache;
}
} // namespace phi::detail
