#pragma once
namespace phi {
enum class LogLevel { trace, debug, info, warn, error, off };
using LogCallback = void (*)(LogLevel, const char*);
} // namespace phi
