#include "platform/logger.hpp"
#include <fstream>
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
    g_log << message << '\n';
    g_log.flush();
  }
}
void close_log() {
  std::lock_guard lock(g_log_mutex);
  g_log.close();
}
} // namespace phi
