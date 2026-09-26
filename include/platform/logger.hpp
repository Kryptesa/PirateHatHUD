#pragma once
#include <string>
namespace phi {
bool open_log(const std::wstring& path);
void log(const char* message);
void close_log();
} // namespace phi
