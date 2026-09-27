#pragma once

#include <cstdint>
#include <vector>

namespace phi::render {
struct Image {
  unsigned width = 0;
  unsigned height = 0;
  std::vector<std::uint8_t> pixels;
};

// A null path selects the embedded default PNG. Explicit file failures do not fall back.
bool decode_image(const wchar_t* path, Image& image);
} // namespace phi::render
