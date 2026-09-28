#include "game/range/memory.hpp"
#include <Windows.h>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <string_view>

namespace phi::detail {
namespace {
constexpr size_t kRadiusOffset = 0x70;
constexpr size_t kObjectSize = 0x80;
constexpr std::string_view kDataType = ".?AVGimmickEventHandlerData_SearchNearLevelGimmick@pa@@";
constexpr std::string_view kPoolType =
  ".?AV?$CompressedObjectMemoryPool@VGimmickEventHandlerData_SearchNearLevelGimmick@pa@@"
  "W4GimmickChartObjectType@2@@pa@@";

bool in_image(const RangeLocation& location, uintptr_t value, size_t size) {
  return value >= location.image_base &&
    value - location.image_base <= location.image_size &&
    size <= location.image_size - (value - location.image_base);
}

bool type_matches(
  const RangeLocation& location,
  const MemoryReader& read,
  uintptr_t vtable,
  std::string_view expected
) {
  uintptr_t locator_address = 0;
  std::array<uint32_t, 6> locator{};
  if (
    vtable < sizeof(uintptr_t) ||
    !in_image(location, vtable - sizeof(uintptr_t), 16) ||
    !read(vtable - sizeof(uintptr_t), &locator_address, sizeof(locator_address)) ||
    !in_image(location, locator_address, sizeof(locator)) ||
    !read(locator_address, locator.data(), sizeof(locator)) ||
    locator[0] != 1 ||
    locator_address < locator[5] ||
    locator_address - locator[5] != location.image_base
  ) {
    return false;
  }
  uintptr_t name = 0;
  std::array<char, 256> text{};
  return expected.size() < text.size() &&
    add_address(location.image_base, locator[3], name) &&
    add_address(name, 16, name) &&
    in_image(location, name, expected.size() + 1) &&
    read(name, text.data(), expected.size() + 1) &&
    std::memcmp(text.data(), expected.data(), expected.size()) == 0 &&
    text[expected.size()] == 0;
}

struct PoolHeader {
  uintptr_t pool = 0;
  uintptr_t blocks = 0;
  uint32_t first_capacity = 0;
  uint32_t next_capacity = 0;
  uint32_t block_count = 0;
  uint32_t count = 0;
  bool operator==(const PoolHeader&) const = default;
};

bool header(const RangeLocation& location, const MemoryReader& read, PoolHeader& value) {
  uintptr_t vtable = 0;
  if (
    !location.pool_slot ||
    !read(location.pool_slot, &value.pool, sizeof(value.pool)) ||
    !value.pool ||
    value.pool % alignof(uintptr_t) ||
    !read_field(read, value.pool, 0, vtable) ||
    !type_matches(location, read, vtable, kPoolType) ||
    !read_field(read, value.pool, 8, value.first_capacity) ||
    !read_field(read, value.pool, 0xC, value.next_capacity) ||
    !read_field(read, value.pool, 0x10, value.blocks) ||
    !read_field(read, value.pool, 0x18, value.block_count) ||
    !read_field(read, value.pool, 0x54, value.count)
  ) {
    return false;
  }
  return value.blocks &&
    value.blocks % alignof(uintptr_t) == 0 &&
    value.first_capacity > 0 &&
    value.first_capacity <= 4096 &&
    value.next_capacity > 0 &&
    value.next_capacity <= 4096 &&
    value.block_count > 0 &&
    value.block_count <= 64 &&
    value.count <= 4096 &&
    value.count <= value.first_capacity + (value.block_count - 1) * value.next_capacity;
}

std::optional<RangeTarget> find_once(const RangeLocation& location, const MemoryReader& read) {
  PoolHeader before{};
  if (
    !header(location, read, before) ||
    !type_matches(location, read, location.data_vtable, kDataType)
  ) {
    return {};
  }
  std::optional<RangeTarget> found;
  for (uint32_t i = 0; i < before.count; ++i) {
    const auto block_index =
      i < before.first_capacity ? 0u : 1u + (i - before.first_capacity) / before.next_capacity;
    const auto element_index =
      i < before.first_capacity ? i : (i - before.first_capacity) % before.next_capacity;
    uintptr_t block = 0, object = 0, vtable = 0;
    if (
      !read_field(read, before.blocks, block_index * sizeof(uintptr_t), block) ||
      !block ||
      !add_address(block, element_index * kObjectSize, object) ||
      object % alignof(uintptr_t) ||
      !read_field(read, object, 0, vtable) ||
      vtable != location.data_vtable
    ) {
      return {};
    }
    // Match the hat's equip condition, two treasure kinds and enter/exit events.
    uint32_t condition = 0, count = 0, capacity = 0, detect = 0, exit = 0;
    uint8_t command = 0, category = 0, equipped = 0, drop_condition = 0;
    uintptr_t keys = 0;
    std::array<uint32_t, 2> kinds{};
    float radius = 0;
    if (
      !read_field(read, object, 0x40, condition) ||
      !read_field(read, object, 0x4C, command) ||
      !read_field(read, object, 0x4E, category) ||
      !read_field(read, object, 0x58, equipped) ||
      !read_field(read, object, 0x60, keys) ||
      !read_field(read, object, 0x68, count) ||
      !read_field(read, object, 0x6C, capacity) ||
      !read_field(read, object, kRadiusOffset, radius) ||
      !read_field(read, object, 0x74, detect) ||
      !read_field(read, object, 0x78, exit) ||
      !read_field(read, object, 0x7C, drop_condition)
    ) {
      return {};
    }
    if (
      command != 0xC0 ||
      category != 2 ||
      equipped != 1 ||
      condition != 0x52029BCF ||
      count != 2 ||
      capacity < count ||
      capacity > 4096 ||
      drop_condition != 1 ||
      detect != 0x9B28835C ||
      exit != 0xCC908146
    ) {
      continue;
    }
    if (
      !keys ||
      keys % alignof(uint32_t) ||
      !read(keys, kinds.data(), sizeof(kinds)) ||
      !std::isfinite(radius) ||
      radius <= 0 ||
      radius > 100000
    ) {
      return {};
    }
    if (kinds != std::array<uint32_t, 2>{18, 19}) {
      continue;
    }
    if (found) {
      return {}; // Never choose between ambiguous hat descriptors.
    }
    found = RangeTarget{before.pool, object, keys, radius};
  }
  PoolHeader after{};
  return header(location, read, after) && before == after ? found : std::nullopt;
}
} // namespace

bool RangeTarget::same_object(const RangeTarget& other) const {
  return pool == other.pool && object == other.object && keys == other.keys;
}

std::optional<RangeTarget>
find_range_target(const RangeLocation& location, const MemoryReader& read) {
  if (!read) {
    return {};
  }
  const auto first = find_once(location, read);
  const auto second = first ? find_once(location, read) : std::nullopt;
  return first && second && first->same_object(*second) && first->radius == second->radius
    ? second
    : std::nullopt;
}

bool exchange_range_float(uintptr_t address, float expected, float desired) noexcept {
  if (!address || address % alignof(LONG) || sizeof(float) > UINTPTR_MAX - address) {
    return false;
  }
  MEMORY_BASIC_INFORMATION region{};
  if (
    !VirtualQuery(reinterpret_cast<const void*>(address), &region, sizeof(region)) ||
    region.State != MEM_COMMIT ||
    (region.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
    (region.Protect & 0xFF) != PAGE_READWRITE
  ) {
    return false;
  }
  const auto base = reinterpret_cast<uintptr_t>(region.BaseAddress);
  if (
    address < base ||
    address - base > region.RegionSize ||
    sizeof(float) > region.RegionSize - (address - base)
  ) {
    return false;
  }
  __try {
    return InterlockedCompareExchange(
             reinterpret_cast<volatile LONG*>(address),
             std::bit_cast<LONG>(desired),
             std::bit_cast<LONG>(expected)
           ) == std::bit_cast<LONG>(expected);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

RangeStatus RangeOverride::restore(
  const RangeLocation& location,
  const MemoryReader& read,
  const FloatExchange& exchange
) {
  if (!owned_) {
    return RangeStatus::disabled;
  }
  const auto current = find_range_target(location, read);
  if (!current) {
    return RangeStatus::waiting;
  }
  if (!owned_->same_object(*current) || current->radius != applied_) {
    owned_.reset();
    return RangeStatus::conflict;
  }
  if (!exchange || !exchange(current->object + kRadiusOffset, applied_, owned_->radius)) {
    return RangeStatus::write_failed;
  }
  owned_.reset();
  return RangeStatus::disabled;
}

RangeStatus RangeOverride::update(
  const RangeLocation& location,
  const MemoryReader& read,
  const FloatExchange& exchange,
  float requested
) {
  if (
    !std::isfinite(requested) ||
    requested < 0 ||
    requested > 1000 ||
    (requested > 0 && requested < 1)
  ) {
    return RangeStatus::conflict;
  }
  if (requested != requested_) {
    requested_ = requested;
    blocked_ = false;
  }
  if (requested == 0) {
    return restore(location, read, exchange);
  }
  if (blocked_) {
    return RangeStatus::conflict;
  }
  const auto current = find_range_target(location, read);
  if (!current) {
    return RangeStatus::waiting;
  }
  if (owned_ && !owned_->same_object(*current)) {
    owned_.reset(); // The pool was reconstructed; never write through an old pointer.
  }
  if (owned_ && current->radius != applied_) {
    owned_.reset();
    blocked_ = true;
    return RangeStatus::conflict;
  }
  if (owned_ && applied_ == requested) {
    return RangeStatus::applied;
  }
  if (!exchange || !exchange(current->object + kRadiusOffset, current->radius, requested)) {
    return RangeStatus::write_failed;
  }
  if (!owned_) {
    owned_ = current;
  }
  applied_ = requested;
  return RangeStatus::applied;
}

float RangeOverride::original() const {
  return owned_ ? owned_->radius : 0;
}
} // namespace phi::detail
