#include "platform/logger.hpp"
#include <windows.h>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string_view>
#include <vector>
namespace phi {
namespace {
namespace fs = std::filesystem;
HANDLE g_file = INVALID_HANDLE_VALUE;
std::mutex g_mutex;
fs::path g_folder;
fs::path g_current;
LogConfig g_config;
std::size_t g_size = 0;
std::chrono::system_clock::time_point g_last_stamp{};
constexpr std::string_view kLevels[] = {"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "OFF"};
void close_file() {
  if (g_file != INVALID_HANDLE_VALUE) {
    CloseHandle(g_file);
    g_file = INVALID_HANDLE_VALUE;
  }
}
bool owned_name(const std::wstring& name) {
  // PirateHatHUD_YYYY-MM-DD_HH-MM-SS-mmm.log (UTC).
  constexpr std::wstring_view pattern = L"PirateHatHUD_0000-00-00_00-00-00-000.log";
  if (name.size() != pattern.size()) {
    return false;
  }
  for (std::size_t i = 0; i < pattern.size(); ++i) {
    if (pattern[i] == L'0') {
      if (name[i] < L'0' || name[i] > L'9') {
        return false;
      }
    } else if (name[i] != pattern[i]) {
      return false;
    }
  }
  return true;
}
bool prune() {
  std::vector<fs::path> files;
  for (const auto& entry : fs::directory_iterator(g_folder)) {
    if (entry.is_regular_file() && !entry.is_symlink() &&
        owned_name(entry.path().filename().wstring())) {
      files.push_back(entry.path());
    }
  }
  std::sort(files.begin(), files.end());
  while (files.size() > g_config.max_files) {
    const auto oldest = std::find_if(files.begin(), files.end(),
                                     [](const auto& path) { return path != g_current; });
    if (oldest == files.end() || !fs::remove(*oldest)) {
      return false;
    }
    files.erase(oldest);
  }
  return true;
}
bool rotate() {
  close_file();
  auto now =
      (std::max)(std::chrono::system_clock::now(), g_last_stamp + std::chrono::milliseconds(1));
  for (unsigned attempt = 0; attempt < 10000; ++attempt, now += std::chrono::milliseconds(1)) {
    const auto seconds = std::chrono::floor<std::chrono::seconds>(now);
    const auto time = std::chrono::system_clock::to_time_t(seconds);
    std::tm utc{};
    gmtime_s(&utc, &time);
    std::wostringstream name;
    name << L"PirateHatHUD_" << std::put_time(&utc, L"%Y-%m-%d_%H-%M-%S-") << std::setfill(L'0')
         << std::setw(3)
         << std::chrono::duration_cast<std::chrono::milliseconds>(now - seconds).count() << L".log";
    g_current = g_folder / name.str();
    g_file = CreateFileW(g_current.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW,
                         FILE_ATTRIBUTE_NORMAL, nullptr);
    if (g_file != INVALID_HANDLE_VALUE) {
      g_last_stamp = now;
      g_size = 0;
      if (!prune()) {
        close_file();
        return false;
      }
      return true;
    }
    if (GetLastError() != ERROR_FILE_EXISTS && GetLastError() != ERROR_ALREADY_EXISTS) {
      return false;
    }
  }
  return false;
}
} // namespace
bool open_log(const std::wstring& folder, const LogConfig& config) {
  std::lock_guard lock(g_mutex);
  close_file();
  try {
    g_config = config;
    if (g_config.level == LogLevel::off) {
      return true;
    }
    if (!g_config.max_files || g_config.max_file_size < 128) {
      return false;
    }
    g_folder = folder;
    return rotate();
  } catch (...) {
    close_file();
    return false;
  }
}
void log(LogLevel level, const char* message) {
  std::lock_guard lock(g_mutex);
  if (g_file == INVALID_HANDLE_VALUE || level < g_config.level || level >= LogLevel::off ||
      !message) {
    return;
  }
  try {
    SYSTEMTIME utc{};
    GetSystemTime(&utc);
    std::ostringstream line;
    line << '[' << std::setfill('0') << std::setw(4) << utc.wYear << '-' << std::setw(2)
         << utc.wMonth << '-' << std::setw(2) << utc.wDay << ' ' << std::setw(2) << utc.wHour << ':'
         << std::setw(2) << utc.wMinute << ':' << std::setw(2) << utc.wSecond << '.' << std::setw(3)
         << utc.wMilliseconds << " UTC] [" << kLevels[static_cast<int>(level)] << "] ";
    // Bound individual records as well as files; never allocate from unbounded diagnostics.
    const auto limit = (std::min)(std::size_t{4096}, g_config.max_file_size - 80);
    std::size_t length = 0;
    while (length < limit && message[length]) {
      ++length;
    }
    line.write(message, static_cast<std::streamsize>(length));
    if (message[length]) {
      line << " [truncated]";
    }
    line << '\n';
    const auto text = line.str();
    if (g_size + text.size() > g_config.max_file_size && !rotate()) {
      return;
    }
    DWORD written = 0;
    if (!WriteFile(g_file, text.data(), static_cast<DWORD>(text.size()), &written, nullptr) ||
        written != text.size()) {
      close_file();
      return;
    }
    g_size += written;
  } catch (...) {
    close_file();
  }
}
void close_log() {
  std::lock_guard lock(g_mutex);
  close_file();
}
} // namespace phi
