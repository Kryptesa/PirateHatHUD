#include "platform/sound.hpp"
#include <Windows.h>
#include <array>
#include <filesystem>
#include <fstream>
#include <type_traits>

#define CHECK(c)                                                                                   \
  do {                                                                                             \
    if (!(c)) {                                                                                    \
      return __LINE__;                                                                             \
    }                                                                                              \
  } while (false)

namespace {
struct Fixture {
  std::filesystem::path path =
      std::filesystem::temp_directory_path() /
      (L"PirateHatHUD_sound_test_" + std::to_wstring(GetCurrentProcessId()) + L".wav");
  ~Fixture() {
    std::error_code error;
    std::filesystem::remove(path, error);
  }

  bool write(const std::array<unsigned char, 46>& bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    return static_cast<bool>(output);
  }
};
} // namespace

int main() {
  static_assert(!std::is_copy_constructible_v<phi::SoundPlayer>);
  phi::SoundPlayer player;
  CHECK(!player.play());
  player.stop();
  CHECK(!player.prepare(L""));
  CHECK(player.prepare_embedded()); // Actual bundled WAV is valid and needs no external file.
  player.stop();
  Fixture fixture;
  // Mono 16-bit PCM, one silent sample. Preparation never opens an audio device.
  const std::array<unsigned char, 46> valid = {
      'R', 'I', 'F', 'F', 38,  0,   0,   0,   'W',  'A',  'V', 'E', 'f',  'm',  't', ' ',
      16,  0,   0,   0,   1,   0,   1,   0,   0x44, 0xac, 0,   0,   0x88, 0x58, 1,   0,
      2,   0,   16,  0,   'd', 'a', 't', 'a', 2,    0,    0,   0,   0,    0};
  CHECK(fixture.write(valid));
  CHECK(player.prepare(fixture.path.wstring()));
  player.stop();
  CHECK(player.prepare(fixture.path.wstring()));

  auto invalid = valid;
  invalid[40] = 255; // Data chunk extends past the file.
  CHECK(fixture.write(invalid));
  CHECK(!player.prepare(fixture.path.wstring()));
  CHECK(!player.play()); // Failed replacement clears the prior prepared wave.

  invalid = valid;
  invalid[20] = 3; // Floating point format is unsupported.
  CHECK(fixture.write(invalid));
  CHECK(!player.prepare(fixture.path.wstring()));

  invalid = valid;
  invalid[32] = 4; // Inconsistent PCM block alignment.
  CHECK(fixture.write(invalid));
  CHECK(!player.prepare(fixture.path.wstring()));

  invalid = valid;
  invalid[4] = 37; // RIFF length does not match the file.
  CHECK(fixture.write(invalid));
  CHECK(!player.prepare(fixture.path.wstring()));
  CHECK(!player.prepare(fixture.path.wstring() + L".missing"));
  CHECK(fixture.write(valid));
  CHECK(player.prepare(fixture.path.wstring()));
  return 0;
}
