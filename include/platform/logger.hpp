#pragma once
#include "core/log.hpp"
#include <cstddef>
#include <string>
namespace phi {
struct LogConfig {
  LogLevel level = LogLevel::info;
  std::size_t max_file_size = 5 * 1024 * 1024;
  unsigned max_files = 3;
};
// Folder must exist. Failures disable file logging without interrupting the mod.
bool open_log(const std::wstring& folder, const LogConfig& config = {});
void log(LogLevel level, const char* message);
void close_log();
} // namespace phi
