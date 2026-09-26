#include "render/hdr_shader.hpp"
#include <windows.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <cstring>
#include <cstdio>
#include <initializer_list>

int main() {
  for (const auto* entry : {"vs_main", "ps_main"}) {
    Microsoft::WRL::ComPtr<ID3DBlob> shader, errors;
    const auto result = D3DCompile(phi::render::kShader, std::strlen(phi::render::kShader), nullptr,
                                   nullptr, nullptr, entry, entry[0] == 'v' ? "vs_5_0" : "ps_5_0",
                                   D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS, 0,
                                   &shader, &errors);
    if (FAILED(result)) {
      if (errors) {
        std::fprintf(stderr, "%s", static_cast<const char*>(errors->GetBufferPointer()));
      }
      return 1;
    }
  }
  return 0;
}
