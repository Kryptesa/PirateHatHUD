#include "platform/config.hpp"
#include "platform/logger.hpp"
#include <windows.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#define CHECK(c)                                                                                   \
  do {                                                                                             \
    if (!(c)) {                                                                                    \
      return __LINE__;                                                                             \
    }                                                                                              \
  } while (false)
namespace {

namespace fs = std::filesystem;

std::vector<fs::path> logs(const fs::path& folder) {
  std::vector<fs::path> result;

  for (const auto& entry : fs::directory_iterator(folder)) {
    if (
      entry.path().filename().wstring().starts_with(L"PirateHatHUD_20") &&
      entry.path().extension() == L".log"
    ) {
      result.push_back(entry.path());
    }
  }

  std::sort(result.begin(), result.end());

  return result;
}

std::string read(const fs::path& path) {
  std::ifstream file(path, std::ios::binary);

  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
} // namespace

int main() {
  using namespace phi;
  const auto folder =
    fs::current_path() / ("logger-fixture-" + std::to_string(GetCurrentProcessId()));
  CHECK(fs::create_directory(folder));

  struct Cleanup {
    fs::path folder;
    ~Cleanup() {
      close_log();
      std::error_code error;
      fs::remove_all(folder, error);
    }
  } cleanup{folder};
  std::ofstream(folder / "PirateHatHUD.log") << "legacy";
  std::ofstream(folder / "PirateHatHUD_notes.log") << "unrelated";
  LogConfig config;
  CHECK(open_log(folder.wstring(), config));

  log(LogLevel::debug, "hidden");
  log(LogLevel::info, "startup");
  log(LogLevel::error, "failure");
  DWORD worker_id = 0;
  std::thread worker([&] {
    worker_id = GetCurrentThreadId();
    log(LogLevel::info, "worker record");
  });
  worker.join();
  close_log();
  auto files = logs(folder);
  CHECK(files.size() == 1);

  auto text = read(files.back());
  CHECK(text.find("hidden") == std::string::npos);
  CHECK(text.find("[INFO] startup") != std::string::npos);
  CHECK(text.find("[ERROR] failure") != std::string::npos);
  CHECK(text.find(" UTC]") != std::string::npos);
  CHECK(text.find("[TID=" + std::to_string(GetCurrentThreadId()) + "]") != std::string::npos);
  CHECK(worker_id != GetCurrentThreadId());
  CHECK(
    text.find("[TID=" + std::to_string(worker_id) + "] [INFO] worker record") != std::string::npos
  );
  CHECK(
    files.back().filename().wstring().size() ==
    std::wstring_view(L"PirateHatHUD_2026-09-27_18-42-03-123.log").size()
  );
  const auto oldest = files.front();

  for (int i = 0; i < 4; ++i) {
    CHECK(open_log(folder.wstring(), config));

    log(LogLevel::info, "next session");
    close_log();
  }

  CHECK(logs(folder).size() == 3);
  CHECK(!fs::exists(oldest));
  CHECK(read(folder / "PirateHatHUD.log") == "legacy");
  CHECK(read(folder / "PirateHatHUD_notes.log") == "unrelated");

  config.max_file_size = 512;
  config.max_files = 2;
  config.level = LogLevel::debug;
  CHECK(open_log(folder.wstring(), config));

  std::vector<std::thread> writers;

  for (int i = 0; i < 4; ++i) {
    writers.emplace_back([] {
      for (int j = 0; j < 30; ++j) {
        log(LogLevel::debug, "concurrent record");
      }
    });
  }

  for (auto& writer : writers) {
    writer.join();
  }

  const std::string huge(10000, 'x');
  log(LogLevel::error, huge.c_str());
  close_log();
  files = logs(folder);
  CHECK(files.size() == 2);

  for (const auto& file : files) {
    CHECK(fs::file_size(file) <= config.max_file_size);
    CHECK(read(file).back() == '\n');
  }

  CHECK(read(files.back()).find("[truncated]") != std::string::npos);

  config.max_files = 1;
  config.level = LogLevel::error;
  CHECK(open_log(folder.wstring(), config));

  log(LogLevel::warn, "filtered warning");
  log(LogLevel::error, "error only");
  close_log();
  CHECK(logs(folder).size() == 1);
  CHECK(read(logs(folder).front()).find("filtered warning") == std::string::npos);
  CHECK(read(logs(folder).front()).find("error only") != std::string::npos);

  config.level = LogLevel::off;
  const auto before = logs(folder);
  CHECK(open_log(folder.wstring(), config));

  log(LogLevel::error, "disabled");
  close_log();
  CHECK(logs(folder) == before);

  config.level = LogLevel::info;
  CHECK(!open_log((folder / "missing").wstring(), config));

  log(LogLevel::error, "safe after failure");
  CHECK(open_log(folder.wstring(), config));

  close_log();
  const auto ini = folder / "settings.ini";
  std::ofstream(ini) << "[logging]\nlevel=DeBuG\nmax_file_size_mb=7\nmax_files=4\n";
  auto settings = read_config(ini.wstring()).logging;
  CHECK(settings.level == LogLevel::debug);
  CHECK(settings.max_file_size == 7 * 1024 * 1024);
  CHECK(settings.max_files == 4);

  std::ofstream(ini) << "[logging]\nlevel=nonsense\nmax_file_size_mb=-1\nmax_files=0\n";
  settings = read_config(ini.wstring()).logging;
  CHECK(settings.level == LogLevel::info);
  CHECK(settings.max_file_size == 5 * 1024 * 1024);
  CHECK(settings.max_files == 3);
  CHECK(read_config((folder / "missing.ini").wstring()).logging.level == LogLevel::info);

  const auto defaults = read_config((folder / "missing.ini").wstring());
  CHECK(defaults.sound_enabled);
  CHECK(defaults.sound_cooldown_ms == 1000);
  CHECK(defaults.sound_volume_percent == 100);
  for (int volume : {0, 25, 100, -1, 101}) {
    std::ofstream(ini) << "[sound]\nvolume_percent=" << volume << "\n";
    CHECK(
      read_config(ini.wstring()).sound_volume_percent ==
      (volume >= 0 && volume <= 100 ? volume : 100)
    );
  }

  std::ofstream(ini) << "[sound]\nenabled=0\ncooldown_ms=2500\n";
  auto sound_settings = read_config(ini.wstring());
  CHECK(!sound_settings.sound_enabled);
  CHECK(sound_settings.sound_cooldown_ms == 2500);

  std::ofstream(ini) << "[sound]\nenabled=1\ncooldown_ms=0\n";
  sound_settings = read_config(ini.wstring());
  CHECK(sound_settings.sound_enabled);
  CHECK(sound_settings.sound_cooldown_ms == 0);

  std::ofstream(ini) << "[sound]\ncooldown_ms=60000\n";
  CHECK(read_config(ini.wstring()).sound_cooldown_ms == 60000);

  std::ofstream(ini) << "[sound]\ncooldown_ms=-1\n";
  CHECK(read_config(ini.wstring()).sound_cooldown_ms == 1000);

  std::ofstream(ini) << "[sound]\ncooldown_ms=60001\n";
  CHECK(read_config(ini.wstring()).sound_cooldown_ms == 1000);
  using Clock = std::chrono::steady_clock;
  const auto now = Clock::now();
  SoundVolumeConfig live(ini.wstring(), 75);
  auto stamp = std::filesystem::last_write_time(ini);
  const auto save_volume = [&](const char* contents) {
    std::ofstream(ini) << contents;
    stamp += std::chrono::seconds(2);
    std::filesystem::last_write_time(ini, stamp);
  };
  save_volume("[sound]\nvolume_percent=25\n");
  CHECK(live.poll(now) == 25);
  save_volume("[sound]\nvolume_percent=50\n"); // Same-length edit must still be noticed.
  CHECK(live.poll(now + std::chrono::milliseconds(500)) == 25);
  CHECK(live.poll(now + std::chrono::seconds(1)) == 50);
  save_volume("[sound]\nvolume_percent=0\n");
  CHECK(live.poll(now + std::chrono::seconds(2)) == 0);
  unsigned second = 3;
  for (const auto* invalid :
    {"",
      "[sound]\n",
      "[sound]\nvolume_percent=nope\n",
      "[sound]\nvolume_percent=101\n",
      "[sound]\nvolume_percent=-1\n",
      "[sound]\nvolume_percent=25junk\n"}) {
    save_volume(invalid);
    CHECK(live.poll(now + std::chrono::seconds(second++)) == 0);
  }
  std::filesystem::remove(ini);
  CHECK(live.poll(now + std::chrono::seconds(second++)) == 0);
  save_volume("[sound]\nvolume_percent=100\n");
  CHECK(live.poll(now + std::chrono::seconds(second++)) == 100);
}
