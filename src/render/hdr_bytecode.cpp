#include "render/hdr_bytecode.hpp"
#include "render/hdr_shader.hpp"
#include <cstring>
#include <cstdio>
#include <utility>

namespace phi::render {
bool HdrBytecode::prepare(LogCallback logger) {
  if (vertex && pixel) {
    return true;
  }

  Microsoft::WRL::ComPtr<ID3DBlob> vs, ps, errors;
  constexpr auto flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS;
  auto compile = [&](const char* entry, const char* profile, ID3DBlob** output) {
    errors.Reset();
    const auto result = D3DCompile(
      kShader,
      std::strlen(kShader),
      nullptr,
      nullptr,
      nullptr,
      entry,
      profile,
      flags,
      0,
      output,
      &errors
    );

    if (FAILED(result) && logger) {
      char message[160]{};
      std::snprintf(
        message,
        sizeof(message),
        "HDR icon shader %s compilation failed before hook activation; HRESULT=0x%08lX",
        entry,
        static_cast<unsigned long>(result)
      );
      logger(LogLevel::error, message);

      if (errors) {
        logger(LogLevel::error, static_cast<const char*>(errors->GetBufferPointer()));
      }
    }

    return SUCCEEDED(result);
  };

  if (!compile("vs_main", "vs_5_0", &vs) || !compile("ps_main", "ps_5_0", &ps)) {
    return false;
  }

  vertex = std::move(vs);
  pixel = std::move(ps);

  return true;
}
} // namespace phi::render
