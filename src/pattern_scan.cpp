#include "pattern_scan.hpp"
#include "patterns.hpp"
#include <algorithm>
#include <charconv>
#include <cstring>
#include <optional>
#include <vector>

namespace phi {
namespace {
using Byte = std::optional<std::uint8_t>;
std::vector<Byte> parse(std::string_view source) {
  std::vector<Byte> result;
  while (!source.empty()) {
    const auto end = source.find(' ');
    auto token = source.substr(0, end);
    if (token == "??" || token == "?") result.emplace_back(std::nullopt);
    else {
      if (token.size() != 2) return {};
      unsigned value = 0;
      auto [p, ec] = std::from_chars(token.data(), token.data() + token.size(), value, 16);
      if (ec != std::errc{} || p != token.data() + token.size() || value > 255) return {};
      result.emplace_back(static_cast<std::uint8_t>(value));
    }
    if (end == std::string_view::npos) break;
    source.remove_prefix(end + 1);
    while (!source.empty() && source.front() == ' ') source.remove_prefix(1);
  }
  if (result.size() < 16 || std::count_if(result.begin(), result.end(), [](const Byte& b) { return b.has_value(); }) < 10) return {};
  return result;
}

struct Match { std::uintptr_t address = 0; unsigned count = 0; };
Match count_matches(const std::uint8_t* base, std::size_t size, const std::vector<Byte>& pat) {
  if (pat.empty() || pat.size() > size) return {};
  Match match{};
  for (std::size_t i = 0; i <= size - pat.size(); ++i) {
    bool equal = true;
    for (std::size_t j = 0; j < pat.size(); ++j) {
      if (pat[j] && base[i + j] != *pat[j]) { equal = false; break; }
    }
    if (equal) {
      ++match.count;
      if (match.count > 1) return match;
      match.address = reinterpret_cast<std::uintptr_t>(base + i);
    }
  }
  return match;
}
}

HookSites find_hook_sites(HMODULE game, std::string_view enter, std::string_view leave) {
  if (!game || enter.find("PLACEHOLDER") != std::string_view::npos ||
      leave.find("PLACEHOLDER") != std::string_view::npos) return {};
  const auto a = parse(enter), b = parse(leave);
  if (a.empty() || b.empty()) return {};
  const auto* image = reinterpret_cast<const std::uint8_t*>(game);
  const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
  if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 0 || dos->e_lfanew > 0x1000) return {};
  const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(image + dos->e_lfanew);
  if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) return {};
  HookSites sites{};
  unsigned enter_count = 0, leave_count = 0;
  const auto* section = IMAGE_FIRST_SECTION(nt);
  for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
    if (!(section[i].Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
    if (section[i].VirtualAddress >= nt->OptionalHeader.SizeOfImage) return {};
    const auto* bytes = image + section[i].VirtualAddress;
    const auto size = std::min<std::size_t>(section[i].Misc.VirtualSize,
      nt->OptionalHeader.SizeOfImage - section[i].VirtualAddress);
    const auto one = count_matches(bytes, size, a), two = count_matches(bytes, size, b);
    enter_count += one.count; leave_count += two.count;
    if (enter_count > 1 || leave_count > 1) return {};
    if (one.count) sites.enter = one.address;
    if (two.count) sites.leave = two.address;
  }
  if (!sites.enter || !sites.leave || sites.leave - sites.enter != patterns::kExpectedDelta) return {};
  constexpr std::uint8_t inc[] = {0xff, 0x46, 0x08};
  constexpr std::uint8_t sub[] = {0x83, 0x6e, 0x08, 0x01};
  if (std::memcmp(reinterpret_cast<void*>(sites.enter), inc, sizeof inc) ||
      std::memcmp(reinterpret_cast<void*>(sites.leave), sub, sizeof sub)) return {};
  return sites;
}
}
