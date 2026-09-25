#include "pattern_scan.hpp"
#include "patterns.hpp"
#include <algorithm>
#include <cstring>
#include <limits>

namespace phi {
namespace {
template <std::size_t N>
bool instruction_at(std::span<const std::uint8_t> bytes, std::size_t offset,
                    const std::uint8_t (&opcode)[N]) {
  return offset <= bytes.size() && N <= bytes.size() - offset &&
         std::memcmp(bytes.data() + offset, opcode, N) == 0;
}
}

ScanResult scan_code(std::span<const std::uint8_t> bytes, std::uintptr_t base) {
  ScanResult result{};
  result.status = ScanStatus::no_match;
  if (bytes.size() < patterns::kExpectedDelta + sizeof(patterns::kLeave) ||
      base > std::numeric_limits<std::uintptr_t>::max() - bytes.size()) return result;
  for (std::size_t offset = 0;
       offset + patterns::kExpectedDelta + sizeof(patterns::kLeave) <= bytes.size(); ++offset) {
    if (!instruction_at(bytes, offset, patterns::kEnter) ||
        !instruction_at(bytes, offset + patterns::kExpectedDelta, patterns::kLeave)) continue;
    ++result.candidate_pairs;
    if (result.candidate_pairs == 1) {
      result.sites.enter = base + offset;
      result.sites.leave = base + offset + patterns::kExpectedDelta;
    } else {
      result.sites = {};
      result.status = ScanStatus::ambiguous;
      return result;
    }
  }
  if (result.candidate_pairs == 1) result.status = ScanStatus::found;
  return result;
}

ScanResult find_hook_sites(HMODULE game) {
  if (!game) return {};
  const auto* image = reinterpret_cast<const std::uint8_t*>(game);
  const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
  if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 0 || dos->e_lfanew > 0x1000) return {};
  const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(image + dos->e_lfanew);
  if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
      nt->FileHeader.NumberOfSections == 0 || nt->FileHeader.NumberOfSections > 96 ||
      nt->OptionalHeader.SizeOfImage < 0x1000) return {};
  const auto* sections = IMAGE_FIRST_SECTION(nt);
  ScanResult total{};
  total.status = ScanStatus::no_match;
  bool executable = false;
  for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
    const auto& section = sections[i];
    if (!(section.Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
    executable = true;
    if (section.VirtualAddress >= nt->OptionalHeader.SizeOfImage) return {};
    const auto size = std::min<std::size_t>(section.Misc.VirtualSize,
        nt->OptionalHeader.SizeOfImage - section.VirtualAddress);
    const auto part = scan_code({image + section.VirtualAddress, size},
        reinterpret_cast<std::uintptr_t>(image + section.VirtualAddress));
    total.candidate_pairs += part.candidate_pairs;
    if (part.status == ScanStatus::ambiguous || total.candidate_pairs > 1) {
      total.sites = {};
      total.status = ScanStatus::ambiguous;
      return total;
    }
    if (part.status == ScanStatus::found) total.sites = part.sites;
  }
  if (!executable) return {};
  if (total.candidate_pairs == 1) total.status = ScanStatus::found;
  return total;
}
}