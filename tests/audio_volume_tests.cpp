#include "game/audio/memory.hpp"
#include "game/audio_volume_observer.hpp"
#include "game/audio/scan.hpp"
#include <array>
#include <cstring>
#include <map>
#include <vector>
#define CHECK(c)                                                                                   \
  do {                                                                                             \
    if (!(c)) {                                                                                    \
      return __LINE__;                                                                             \
    }                                                                                              \
  } while (false)

namespace {
struct Fixture {
  phi::detail::AudioVolumeLocation location{0x100100, 0x100000, 0x10000};
  std::map<uintptr_t, std::vector<uint8_t>> memory;
  template <typename T> void put(uintptr_t address, const T& value) {
    auto& bytes = memory[address];
    bytes.resize(sizeof(value));
    std::memcpy(bytes.data(), &value, sizeof(value));
  }
  bool read(uintptr_t address, void* destination, size_t size) const {
    auto it = memory.find(address);
    if (it == memory.end() || it->second.size() < size) {
      return false;
    }
    std::memcpy(destination, it->second.data(), size);
    return true;
  }
  Fixture() {
    put(location.engine_slot, uintptr_t{0x200000});
    put(0x200000, uintptr_t{0x101000});
    put(0x201070, uintptr_t{0x300000});
    put(0x300000, uintptr_t{0x101000});
    for (size_t i = 0; i < 2; ++i) {
      const uintptr_t root = 0x400000 + i * 0x1000;
      const uintptr_t audio = 0x500000 + i * 0x1000;
      put(0x300000 + (i == 0 ? 0x50 : 0x68), root + 0x28);
      put(root, uintptr_t{0x101000});
      put(root + 0x98, audio + 0x28);
      put(audio, uintptr_t{0x101000});
      put(audio + 0x10, root);
      put(audio + 0x88, audio);
      put(audio + 0xE8, audio);
      put(audio + 0x78, uintptr_t{0x102000});
      put(audio + 0xD8, uintptr_t{0x102000});
      put(audio + 0xA0, uintptr_t{0x600000});
      put(audio + 0x100, uintptr_t{0x600100});
      put(audio + 0xD0, int32_t{50});
      put(audio + 0x130, int32_t{25});
    }
    put(0x600000, uintptr_t{0x700001}); // Character buffers need not be aligned.
    put(0x600100, uintptr_t{0x700100});
    put(0x700001, "UI_GameSetting_Sound_MasterVolume");
    put(0x700100, "UI_GameSetting_Sound_SFXVolume");
  }
  phi::AudioVolumeState sample() const {
    return phi::detail::sample_audio_volume(location, [this](auto a, auto d, auto n) {
      return read(a, d, n);
    });
  }
};
} // namespace
int main() {
  Fixture fixture;
  const phi::AudioVolumeState expected{true, 50, 25};
  CHECK(fixture.sample() == expected);
  for (const auto& [address, bytes] : fixture.memory) {
    auto broken = fixture;
    broken.memory.erase(address);
    CHECK(!broken.sample().known);
  }
  for (auto percent : {-1, 101}) {
    auto broken = fixture;
    broken.put(0x5000D0, int32_t{percent});
    CHECK(!broken.sample().known);
  }
  for (auto address :
    {uintptr_t{0x500010}, uintptr_t{0x500088}, uintptr_t{0x5000E8}, uintptr_t{0x5000D8}}) {
    auto broken = fixture;
    broken.put(address, uintptr_t{0xDEAD0000});
    CHECK(!broken.sample().known);
  }
  auto broken = fixture;
  broken.put(0x501130, int32_t{26});
  CHECK(!broken.sample().known);
  broken = fixture;
  broken.memory[0x700001].back() = 'x';
  CHECK(!broken.sample().known);
  broken = fixture;
  broken.location.engine_slot = UINTPTR_MAX;
  CHECK(!broken.sample().known);
  unsigned samples = 0;
  CHECK(!phi::detail::sample_audio_volume(fixture.location, [&](auto a, auto d, auto n) {
    if (a == fixture.location.engine_slot && ++samples == 2) {
      fixture.put(0x5000D0, int32_t{60});
      fixture.put(0x5010D0, int32_t{60});
    }
    return fixture.read(a, d, n);
  }).known);
  CHECK(fixture.sample().master_percent == 60);

  std::vector<uint8_t> code = {
    0x48,
    0x8B,
    0x05,
    0xF9,
    0,
    0,
    0,
    0xC5,
    0xFB,
    0x10,
    0xB0,
    0xC8,
    0,
    0,
    0,
    0x8B,
    0x98,
    0xD0,
    0,
    0,
    0,
    0x48,
    0x8B,
    0xCE
  };
  CHECK(phi::scan_audio_volume_code(code, 0x1000).status == phi::ScanStatus::found);
  CHECK(phi::scan_audio_volume_code(code, 0x1001).status == phi::ScanStatus::no_match);
  CHECK(phi::scan_audio_volume_code(code, UINTPTR_MAX).status == phi::ScanStatus::no_match);
  auto duplicate = code;
  duplicate.insert(duplicate.end(), code.begin(), code.end());
  CHECK(phi::scan_audio_volume_code(duplicate, 0x1000).status == phi::ScanStatus::ambiguous);
  code.pop_back();
  CHECK(phi::scan_audio_volume_code(code, 0x1000).status == phi::ScanStatus::no_match);

  phi::AudioVolumeObserver observer;
  CHECK(!observer.start()); // The test process has no game module.
  observer.poll();
  CHECK(!observer.state().known);
  observer.stop();
}
