#include "pattern_scan.hpp"
#include <algorithm>

namespace phi {

namespace {

bool valid(BytePattern pattern) {
  if (pattern.mask.empty()) {
    return true;
  }

  if (pattern.mask.size() != pattern.bytes.size()) {
    return false;
  }

  return std::ranges::all_of(pattern.mask, [](uint8_t byte) { return byte <= 1; });
}

bool at(std::span<const uint8_t> code, size_t offset, BytePattern pattern) {
  if (offset > code.size() || pattern.bytes.size() > code.size() - offset) {
    return false;
  }

  for (size_t i = 0; i < pattern.bytes.size(); ++i) {
    if ((pattern.mask.empty() || pattern.mask[i]) && code[offset + i] != pattern.bytes[i]) {
      return false;
    }
  }

  return true;
}
} // namespace

PatternMatch scan_pattern(
  std::span<const uint8_t> code,
  uintptr_t base,
  const PatternQuery& query,
  CandidateValidator validate
) {
  PatternMatch result{};

  if (
    query.first.bytes.empty() ||
    !valid(query.first) ||
    !valid(query.second) ||
    query.delta > SIZE_MAX - query.second.bytes.size()
  ) {
    return result;
  }

  result.status = ScanStatus::no_match;
  const auto extent = std::max(query.first.bytes.size(), query.delta + query.second.bytes.size());

  if (extent > code.size() || base > UINTPTR_MAX - code.size()) {
    return result;
  }

  for (size_t offset = 0; offset <= code.size() - extent; ++offset) {
    if (
      !at(code, offset, query.first) ||
      !at(code, offset + query.delta, query.second) ||
      (validate && !validate(code, offset, base))
    ) {
      continue;
    }

    if (++result.candidates > 1) {
      result.address = 0;
      result.status = ScanStatus::ambiguous;

      return result;
    }

    result.address = base + offset;
  }

  if (result.candidates == 1) {
    result.status = ScanStatus::found;
  }

  return result;
}

PatternMatch find_pattern(HMODULE module, const PatternQuery& query, CandidateValidator validate) {
  if (!module) {
    return {};
  }

  const auto* image = reinterpret_cast<const std::uint8_t*>(module);
  const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(image);

  if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 0 || dos->e_lfanew > 0x1000) {
    return {};
  }

  const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(image + dos->e_lfanew);

  if (
    nt->Signature != IMAGE_NT_SIGNATURE ||
    nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
    nt->FileHeader.NumberOfSections == 0 ||
    nt->FileHeader.NumberOfSections > 96 ||
    nt->OptionalHeader.SizeOfImage < 0x1000
  ) {
    return {};
  }

  const auto* sections = IMAGE_FIRST_SECTION(nt);
  PatternMatch total{};
  total.image_base = reinterpret_cast<uintptr_t>(image);
  total.image_size = nt->OptionalHeader.SizeOfImage;
  total.status = ScanStatus::no_match;
  bool executable = false;

  for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
    const auto& section = sections[i];

    if (!(section.Characteristics & IMAGE_SCN_MEM_EXECUTE)) {
      continue;
    }

    executable = true;

    if (section.VirtualAddress >= nt->OptionalHeader.SizeOfImage) {
      return {};
    }

    const auto size = std::min<std::size_t>(
      section.Misc.VirtualSize,
      nt->OptionalHeader.SizeOfImage - section.VirtualAddress
    );
    const auto part = scan_pattern(
      {image + section.VirtualAddress, size},
      reinterpret_cast<std::uintptr_t>(image + section.VirtualAddress),
      query,
      validate
    );

    if (part.status == ScanStatus::invalid_image) {
      return {};
    }

    total.candidates += part.candidates;

    if (part.status == ScanStatus::ambiguous || total.candidates > 1) {
      total.address = 0;
      total.status = ScanStatus::ambiguous;

      return total;
    }

    if (part.status == ScanStatus::found) {
      total.address = part.address;
    }
  }

  if (!executable) {
    return {};
  }

  if (total.candidates == 1) {
    total.status = ScanStatus::found;
  }

  return total;
}
} // namespace phi
