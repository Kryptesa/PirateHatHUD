#pragma once

#include "core/log.hpp"
#include <windows.h>
#include <atomic>

namespace phi::render {
enum class CallbackStage { entered, original_begin, original_returned, completed };

class DxgiDiagnostics {
public:
  void set_logger(LogCallback logger) noexcept;
  void log(LogLevel level, const char* message) const;
  bool sample(unsigned index, bool eligible = true) noexcept;
  void progress(
    bool sampled,
    const char* operation,
    CallbackStage stage,
    HRESULT result = S_OK
  ) noexcept;
  bool result(HRESULT result, const char* operation) const;

private:
  std::atomic<LogCallback> logger_{};
  std::atomic<bool> first_callbacks_[9]{};
};
} // namespace phi::render
