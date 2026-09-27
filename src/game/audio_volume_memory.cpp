#include "game/audio_volume_memory.hpp"
#include <array>
#include <cstring>
#include <limits>

namespace phi::detail {
namespace {
struct Snapshot {
  uintptr_t engine = 0;
  uintptr_t manager = 0;
  std::array<uintptr_t, 2> roots{};
  std::array<uintptr_t, 2> audio{};
  std::array<AudioVolumeState, 2> volumes{};
  bool operator==(const Snapshot&) const = default;
};

template <typename T>
bool field(const MemoryReader& read, uintptr_t object, size_t offset, T& value) {
  return object &&
    offset <= UINTPTR_MAX - object &&
    sizeof(T) <= UINTPTR_MAX - (object + offset) &&
    read(object + offset, &value, sizeof(value));
}

bool pointer(const MemoryReader& read, uintptr_t object, size_t offset, uintptr_t& value) {
  return field(read, object, offset, value) && value && value % alignof(uintptr_t) == 0;
}

bool module_vtable(
  const AudioVolumeLocation& location,
  const MemoryReader& read,
  uintptr_t object,
  size_t offset = 0
) {
  uintptr_t vtable = 0;
  return pointer(read, object, offset, vtable) &&
    vtable >= location.image_base &&
    location.image_size >= sizeof(uintptr_t) &&
    vtable - location.image_base <= location.image_size - sizeof(uintptr_t);
}

template <size_t N>
bool property_name(
  const MemoryReader& read,
  uintptr_t object,
  size_t offset,
  const char (&name)[N]
) {
  uintptr_t string = 0;
  uintptr_t chars = 0;
  std::array<char, N> bytes{};
  return pointer(read, object, offset, string) &&
    field(read, string, 0, chars) &&
    chars &&
    chars <= UINTPTR_MAX - N &&
    read(chars, bytes.data(), N) &&
    std::memcmp(bytes.data(), name, N) == 0;
}

bool snapshot(const AudioVolumeLocation& location, const MemoryReader& read, Snapshot& result) {
  if (
    !pointer(read, location.engine_slot, 0, result.engine) ||
    !module_vtable(location, read, result.engine) ||
    !pointer(read, result.engine, 0x1070, result.manager) ||
    !module_vtable(location, read, result.manager)
  ) {
    return false;
  }
  for (size_t i = 0; i < result.roots.size(); ++i) {
    auto& root = result.roots[i];
    auto& audio = result.audio[i];
    if (
      !pointer(read, result.manager, i == 0 ? 0x50 : 0x68, root) ||
      root < 0x28 ||
      !module_vtable(location, read, root - 0x28) ||
      !pointer(read, root, 0x70, audio) ||
      audio < 0x28 ||
      !module_vtable(location, read, audio - 0x28)
    ) {
      return false;
    }
    const auto object = audio - 0x28;
    uintptr_t owner = 0, master_owner = 0, effects_owner = 0;
    uintptr_t master_vtable = 0, effects_vtable = 0;
    if (
      !pointer(read, object, 0x10, owner) ||
      owner != root - 0x28 ||
      !pointer(read, object, 0x88, master_owner) ||
      master_owner != object ||
      !pointer(read, object, 0xE8, effects_owner) ||
      effects_owner != object ||
      !module_vtable(location, read, object, 0x78) ||
      !pointer(read, object, 0x78, master_vtable) ||
      !pointer(read, object, 0xD8, effects_vtable) ||
      master_vtable != effects_vtable ||
      !property_name(read, object, 0xA0, "UI_GameSetting_Sound_MasterVolume") ||
      !property_name(read, object, 0x100, "UI_GameSetting_Sound_SFXVolume")
    ) {
      return false;
    }
    int32_t master = 0, effects = 0;
    if (
      !field(read, object, 0xD0, master) ||
      !field(read, object, 0x130, effects) ||
      master < 0 ||
      master > 100 ||
      effects < 0 ||
      effects > 100
    ) {
      return false;
    }
    result.volumes[i] = {true, static_cast<unsigned>(master), static_cast<unsigned>(effects)};
  }
  return result.volumes[0] == result.volumes[1];
}
} // namespace

AudioVolumeState
sample_audio_volume(const AudioVolumeLocation& location, const MemoryReader& read) {
  if (
    !read ||
    !location.image_base ||
    location.image_size > UINTPTR_MAX - location.image_base ||
    location.image_size < sizeof(uintptr_t) ||
    location.engine_slot < location.image_base ||
    location.engine_slot - location.image_base > location.image_size - sizeof(uintptr_t)
  ) {
    return {};
  }
  Snapshot first, second;
  if (!snapshot(location, read, first) || !snapshot(location, read, second) || first != second) {
    return {};
  }
  return first.volumes[0];
}
} // namespace phi::detail
