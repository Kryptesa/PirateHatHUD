#include "pattern_scan.hpp"
#include "patterns.hpp"
#include <algorithm>
#include <cstring>
#include <limits>

namespace phi {
namespace {
template <std::size_t N>
bool instruction_at(std::span<const std::uint8_t> bytes, std::size_t offset,
                    const std::uint8_t (&opcode)[N], bool displacement = false) {
  if (offset > bytes.size() || N > bytes.size() - offset) {
    return false;
  }
  for (size_t i = 0; i < N; ++i) {
    if (displacement && i >= 2 && i < 6) {
      continue;
    }
    if (bytes[offset + i] != opcode[i]) {
      return false;
    }
  }
  return true;
}
} // namespace

ScanResult scan_code(std::span<const std::uint8_t> bytes, std::uintptr_t base, bool menu,
                     bool ui_root) {
  ScanResult result{};
  result.status = ScanStatus::no_match;
  constexpr std::uint8_t clear[] = {0xC6, 0x81, 0x5B, 0x02, 0, 0, 0, 0x84, 0xD2, 0x74, 0x1C};
  constexpr std::uint8_t set[] = {0xC6, 0x83, 0x5B, 0x02, 0, 0, 1, 0x48, 0x8B, 1, 0xFF, 0x50, 0x30};
  if (ui_root) {
    // Verified launcher constructor context; only RIP-relative disp32 operands are masked.
    constexpr uint8_t context[] = {0x90, 0x48, 0x8D, 0x05, 0,    0,    0,    0,    0x48, 0x89,
                                   0x03, 0x48, 0x89, 0x1D, 0,    0,    0,    0,    0xC6, 0x05,
                                   0,    0,    0,    0,    3,    0x48, 0xC7, 0x44, 0x24, 0x40,
                                   0,    0,    0,    0,    0x48, 0x85, 0xDB};
    if (base > UINTPTR_MAX - bytes.size()) {
      return result;
    }
    for (size_t offset = 0; offset + sizeof(context) <= bytes.size(); ++offset) {
      bool match = true;
      for (size_t i = 0; i < sizeof(context); ++i) {
        if ((i >= 4 && i < 8) || (i >= 14 && i < 18) || (i >= 20 && i < 24)) {
          continue;
        }
        if (bytes[offset + i] != context[i]) {
          match = false;
          break;
        }
      }
      if (!match) {
        continue;
      }
      int32_t displacement = 0;
      std::memcpy(&displacement, bytes.data() + offset + 14, 4);
      const auto next = base + offset + 18;
      const auto distance =
          displacement < 0 ? uint64_t(-int64_t(displacement)) : uint64_t(displacement);
      if ((displacement < 0 && next < distance) ||
          (displacement >= 0 && next > UINTPTR_MAX - distance)) {
        continue;
      }
      if (++result.candidate_pairs > 1) {
        result.root_slot = 0;
        result.status = ScanStatus::ambiguous;
        return result;
      }
      result.root_slot = displacement < 0 ? next - distance : next + distance;
    }
    if (result.candidate_pairs == 1) {
      result.status = ScanStatus::found;
    }
    return result;
  }
  const auto delta = menu ? 0x141u : patterns::kExpectedDelta;
  const auto tail = menu ? sizeof(set) : sizeof(patterns::kLeave);
  if (bytes.size() < delta + tail ||
      base > std::numeric_limits<std::uintptr_t>::max() - bytes.size()) {
    return result;
  }
  for (std::size_t offset = 0; offset + delta + tail <= bytes.size(); ++offset) {
    const bool match = menu ? instruction_at(bytes, offset, clear, true) &&
                                  instruction_at(bytes, offset + delta, set, true)
                            : instruction_at(bytes, offset, patterns::kEnter) &&
                                  instruction_at(bytes, offset + delta, patterns::kLeave);
    if (!match) {
      continue;
    }
    uint32_t state_offset = 0;
    if (menu) {
      uint32_t other = 0;
      std::memcpy(&state_offset, bytes.data() + offset + 2, 4);
      std::memcpy(&other, bytes.data() + offset + delta + 2, 4);
      // Exact C6 /0 ModRM encodings prove byte MOV, RCX/RBX base, no index, immediate 0/1.
      if (!state_offset || state_offset > 0x10000 || state_offset != other) {
        continue;
      }
    }
    ++result.candidate_pairs;
    if (result.candidate_pairs == 1) {
      result.state_offset = state_offset;
      result.sites.enter = base + offset;
      result.sites.leave = base + offset + delta;
    } else {
      result.state_offset = 0;
      result.sites = {};
      result.status = ScanStatus::ambiguous;
      return result;
    }
  }
  if (result.candidate_pairs == 1)
    result.status = ScanStatus::found;
  return result;
}

ScanResult find_hook_sites(HMODULE game, bool menu, bool ui_root) {
  if (!game)
    return {};
  const auto* image = reinterpret_cast<const std::uint8_t*>(game);
  const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
  if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 0 || dos->e_lfanew > 0x1000)
    return {};
  const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(image + dos->e_lfanew);
  if (nt->Signature != IMAGE_NT_SIGNATURE ||
      nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
      nt->FileHeader.NumberOfSections == 0 || nt->FileHeader.NumberOfSections > 96 ||
      nt->OptionalHeader.SizeOfImage < 0x1000)
    return {};
  const auto* sections = IMAGE_FIRST_SECTION(nt);
  ScanResult total{};
  total.status = ScanStatus::no_match;
  bool executable = false;
  for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
    const auto& section = sections[i];
    if (!(section.Characteristics & IMAGE_SCN_MEM_EXECUTE))
      continue;
    executable = true;
    if (section.VirtualAddress >= nt->OptionalHeader.SizeOfImage)
      return {};
    const auto size = std::min<std::size_t>(
        section.Misc.VirtualSize, nt->OptionalHeader.SizeOfImage - section.VirtualAddress);
    const auto part =
        scan_code({image + section.VirtualAddress, size},
                  reinterpret_cast<std::uintptr_t>(image + section.VirtualAddress), menu, ui_root);
    total.candidate_pairs += part.candidate_pairs;
    if (part.status == ScanStatus::ambiguous || total.candidate_pairs > 1) {
      total.state_offset = 0;
      total.root_slot = 0;
      total.sites = {};
      total.status = ScanStatus::ambiguous;
      return total;
    }
    if (part.status == ScanStatus::found) {
      total.sites = part.sites;
      total.state_offset = part.state_offset;
      total.root_slot = part.root_slot;
    }
  }
  if (!executable)
    return {};
  if (total.candidate_pairs == 1) {
    total.status = ScanStatus::found;
    if (ui_root && (total.root_slot < reinterpret_cast<uintptr_t>(image) ||
                    total.root_slot - reinterpret_cast<uintptr_t>(image) >
                        nt->OptionalHeader.SizeOfImage - sizeof(uintptr_t) ||
                    total.root_slot % alignof(uintptr_t))) {
      return {};
    }
  }
  return total;
}
} // namespace phi
