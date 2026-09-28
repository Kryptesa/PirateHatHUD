#include "render/dxgi_diagnostics.hpp"
#include <cstdio>

namespace phi::render {
void DxgiDiagnostics::set_logger(LogCallback logger) noexcept {
  logger_.store(logger);
}

void DxgiDiagnostics::log(LogLevel level, const char* message) const {
  if (auto logger = logger_.load()) {
    logger(level, message);
  }
}

void DxgiDiagnostics::progress(
  bool sampled,
  const char* operation,
  CallbackStage stage,
  HRESULT result
) noexcept {
  if (!sampled) {
    return;
  }
  try {
    char message[192]{};
    const char* text = stage == CallbackStage::entered
      ? "entered"
      : stage == CallbackStage::original_begin
        ? "original call begin"
        : stage == CallbackStage::original_returned
          ? "original call returned"
          : "completed";
    if (stage == CallbackStage::original_returned) {
      std::snprintf(
        message,
        sizeof(message),
        "DX12 first %s callback: %s; HRESULT=0x%08lX",
        operation,
        text,
        static_cast<unsigned long>(result)
      );
    } else {
      std::snprintf(message, sizeof(message), "DX12 first %s callback: %s", operation, text);
    }
    log(LogLevel::debug, message);
  } catch (...) {
    // Diagnostic failures must not disable rendering or escape a graphics callback.
  }
}

bool DxgiDiagnostics::sample(unsigned index, bool eligible) noexcept {
  return eligible &&
    !first_callbacks_[index].load(std::memory_order_relaxed) &&
    !first_callbacks_[index].exchange(true, std::memory_order_relaxed);
}

bool DxgiDiagnostics::result(HRESULT result, const char* operation) const {
  if (SUCCEEDED(result)) {
    return true;
  }
  char message[160]{};
  std::snprintf(
    message,
    sizeof(message),
    "DX12 %s failed; HRESULT=0x%08lX",
    operation,
    static_cast<unsigned long>(result)
  );
  log(LogLevel::error, message);
  return false;
}

} // namespace phi::render
