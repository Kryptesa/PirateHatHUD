#pragma once
#include <windows.h>
#include <cstddef>
#include <cstdint>
#include <span>
namespace phi {
enum class ScanStatus { found, invalid_image, no_match, ambiguous };
struct BytePattern {
  std::span<const uint8_t> bytes;
  // Empty mask means exact matching; otherwise 0 ignores a byte, 1 matches it.
  std::span<const uint8_t> mask{};
};
struct PatternQuery {
  BytePattern first;
  BytePattern second{};
  size_t delta = 0;
};
struct PatternMatch {
  uintptr_t address = 0;
  ScanStatus status = ScanStatus::invalid_image;
  size_t candidates = 0;
  uintptr_t image_base = 0;
  size_t image_size = 0;
};
using CandidateValidator = bool (*)(std::span<const uint8_t> code, size_t offset, uintptr_t base);
PatternMatch scan_pattern(std::span<const uint8_t> code, uintptr_t base, const PatternQuery& query,
                          CandidateValidator validate = nullptr);
PatternMatch find_pattern(HMODULE image, const PatternQuery& query,
                          CandidateValidator validate = nullptr);
} // namespace phi
