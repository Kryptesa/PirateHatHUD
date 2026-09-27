#include "platform/sound.hpp"
#include <Windows.h>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <future>
#include <mutex>
#include <limits>
#include <vector>
#include <thread>
#include <type_traits>

#define CHECK(c)                                                                                   \
  do {                                                                                             \
    if (!(c)) {                                                                                    \
      return __LINE__;                                                                             \
    }                                                                                              \
  } while (false)

namespace {
struct BlockingAudio {
  std::mutex mutex;
  std::condition_variable changed;
  bool release_play = false;
  bool release_stop = false;
  bool bytes_valid = true;
  unsigned plays = 0;
  unsigned stops = 0;
  const uint8_t* retained = nullptr;
  std::vector<int> samples;
  std::thread::id worker;

  bool wait_for(unsigned expected_plays, unsigned expected_stops) {
    std::unique_lock lock(mutex);

    return changed.wait_for(lock, std::chrono::seconds(2), [&] {
      return plays >= expected_plays && stops >= expected_stops;
    });
  }

  void release(bool play, bool stop) {
    {
      std::lock_guard lock(mutex);
      release_play |= play;
      release_stop |= stop;
    }

    changed.notify_all();
  }
} audio;

bool blocking_playback(const std::uint8_t* bytes) noexcept {
  std::unique_lock lock(audio.mutex);
  audio.worker = std::this_thread::get_id();

  if (bytes) {
    ++audio.plays;
    audio.changed.notify_all();
    audio.changed.wait(lock, [] { return audio.release_play; });
    audio.bytes_valid &= std::memcmp(bytes, "RIFF", 4) == 0;
    audio.samples.push_back(bytes[44] | (bytes[45] << 8));
    if (audio.plays == 3) {
      // A busy device refuses the new buffer; the prior buffer must survive.
      return false;
    }
    audio.retained = bytes;
  } else {
    audio.bytes_valid &= audio.retained && std::memcmp(audio.retained, "RIFF", 4) == 0;
    audio.bytes_valid &=
      (audio.retained[44] | (audio.retained[45] << 8)) == audio.samples[audio.stops];
    audio.retained = nullptr;
    ++audio.stops;
    audio.changed.notify_all();
    audio.changed.wait(lock, [] { return audio.release_stop; });
  }

  return true;
}

struct ReleaseAudio {
  ~ReleaseAudio() {
    audio.release(true, true);
  }
};

struct Fixture {
  std::filesystem::path path = std::filesystem::temp_directory_path() /
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
    'R',
    'I',
    'F',
    'F',
    38,
    0,
    0,
    0,
    'W',
    'A',
    'V',
    'E',
    'f',
    'm',
    't',
    ' ',
    16,
    0,
    0,
    0,
    1,
    0,
    1,
    0,
    0x44,
    0xac,
    0,
    0,
    0x88,
    0x58,
    1,
    0,
    2,
    0,
    16,
    0,
    'd',
    'a',
    't',
    'a',
    2,
    0,
    0,
    0,
    0,
    0
  };
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

  auto audible = valid;
  audible[44] = 0x10;
  audible[45] = 0x27; // 10000: verify each playback scales the original sample.
  CHECK(fixture.write(audible));
  {
    phi::SoundPlayer threaded(blocking_playback);
    ReleaseAudio release_on_failure;
    CHECK(threaded.prepare(fixture.path.wstring()));
    CHECK(!threaded.play(-1));
    CHECK(!threaded.play(1.1f));
    CHECK(!threaded.play(std::numeric_limits<float>::quiet_NaN()));
    CHECK(threaded.play(0.5f));
    CHECK(audio.wait_for(1, 0));

    // Slow audio startup must not make stop wait on the device or its mutex.
    auto stop = std::async(std::launch::async, [&] { threaded.stop(); });
    const bool stop_ready =
      stop.wait_for(std::chrono::milliseconds(500)) == std::future_status::ready;
    audio.release(true, false);
    stop.get();
    CHECK(stop_ready);
    CHECK(audio.wait_for(1, 1));

    // Slow audio shutdown must not prevent a new notification being queued.
    auto play = std::async(std::launch::async, [&] { return threaded.play(0.25f); });
    const bool play_ready =
      play.wait_for(std::chrono::milliseconds(500)) == std::future_status::ready;
    audio.release(false, true);
    CHECK(play.get());
    CHECK(play_ready);
    CHECK(audio.wait_for(2, 1));
    CHECK(threaded.play(0.125f));
    CHECK(audio.wait_for(3, 1));
  } // Destruction joins the worker and stops playback before freeing PCM bytes.
  CHECK(audio.plays == 3 && audio.stops == 2);
  CHECK(audio.bytes_valid);
  CHECK(audio.samples == std::vector<int>({5000, 2500, 1250}));
  CHECK(audio.worker != std::this_thread::get_id());

  return 0;
}
