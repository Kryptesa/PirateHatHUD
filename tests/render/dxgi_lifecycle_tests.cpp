#include "render/dxgi_hooks.hpp"
#include "render/dxgi_diagnostics.hpp"
#include <atomic>
#include <cstring>
#include <stdexcept>
#include <thread>
#include <vector>

#define CHECK(c)                                                                                   \
  do {                                                                                             \
    if (!(c)) {                                                                                    \
      return __LINE__;                                                                             \
    }                                                                                              \
  } while (false)

namespace {
std::atomic<bool> probe_retained{}, probe_released{}, probe_retained_at_stop{};

void capture_log(phi::LogLevel, const char* message) {
  if (std::strstr(message, "probe device retained until")) {
    probe_retained = true;
  }
  if (std::strstr(message, "probe device released after")) {
    probe_released = true;
  }
  if (std::strstr(message, "probe device retained through")) {
    probe_retained_at_stop = true;
  }
}

void throwing_log(phi::LogLevel, const char*) {
  throw std::runtime_error("diagnostic logger failure");
}

phi::HudState snapshot() {
  return {};
}
} // namespace

int main() {
  using namespace phi::render;
  DxgiDiagnostics diagnostics;
  CHECK(!diagnostics.sample(0, false));
  std::atomic<unsigned> sampled{};
  std::vector<std::thread> workers;
  for (unsigned i = 0; i < 16; ++i) {
    workers.emplace_back([&] {
      if (diagnostics.sample(0)) {
        ++sampled;
      }
    });
  }
  for (auto& worker : workers) {
    worker.join();
  }
  CHECK(sampled == 1);
  CHECK(diagnostics.sample(1));
  diagnostics.set_logger(throwing_log);
  diagnostics.progress(true, "Present", CallbackStage::entered);
  diagnostics.set_logger(nullptr);
  diagnostics.progress(true, "Present", CallbackStage::completed);

  // Exercise actual probe creation, hook activation and scoped probe cleanup.
  // No game Present follows: stop must keep the device alive, rather than trigger
  // premature ReShade add-on unloading. This process exits with retained hooks.
  Dx12Renderer renderer;
  CHECK(start_hooks(renderer, snapshot, capture_log));
  CHECK(probe_retained);
  CHECK(!probe_released);
  const auto result = stop_hooks();
  CHECK(result.hooks_disabled);
  CHECK(result.callbacks_drained);
  CHECK(!result.gpu_resources_released);
  CHECK(result.module_must_remain_loaded);
  CHECK(probe_retained_at_stop);
  CHECK(!probe_released);
  CHECK(!start_hooks(renderer, snapshot, capture_log));
  const auto repeated = stop_hooks();
  CHECK(repeated.module_must_remain_loaded);
  CHECK(!repeated.gpu_resources_released);
  return 0;
}
