#include "platform/logger.hpp"
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <mutex>
namespace phi {
namespace {
std::ofstream g_log;
std::mutex g_log_mutex;
} // namespace
bool open_log(const std::wstring& path) {
  std::lock_guard lock(g_log_mutex);
  g_log.open(path, std::ios::app);
  return static_cast<bool>(g_log);
}
void log(const char* message) {
  std::lock_guard lock(g_log_mutex);
  if (g_log) {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
    localtime_s(&local, &time);
    const auto milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() %
        1000;
    g_log << '[' << std::put_time(&local, "%Y-%m-%d %H:%M:%S") << '.' << std::setfill('0')
          << std::setw(3) << milliseconds << "] " << message << '\n';
    g_log.flush();
  }
}
void close_log() {
  std::lock_guard lock(g_log_mutex);
  g_log.close();
}
} // namespace phi
