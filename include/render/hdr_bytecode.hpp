#pragma once

#include "core/log.hpp"
#include <windows.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

namespace phi::render {
// Prepared on the application thread before graphics hooks are activated.
// Bytecode is independent of GPU resources and survives resize/device replacement.
struct HdrBytecode {
  Microsoft::WRL::ComPtr<ID3DBlob> vertex;
  Microsoft::WRL::ComPtr<ID3DBlob> pixel;

  bool prepare(LogCallback logger = nullptr);
};
} // namespace phi::render
