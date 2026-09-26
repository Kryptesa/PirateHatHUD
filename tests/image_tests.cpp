#include "render/image.hpp"
#include <filesystem>

#define CHECK(c)                                                                                   \
  do {                                                                                             \
    if (!(c)) {                                                                                    \
      return __LINE__;                                                                             \
    }                                                                                              \
  } while (false)

int main(int argc, char** argv) {
  CHECK(argc == 2);
  phi::render::Image image{1, 1, {1, 2, 3, 4}};
  CHECK(!phi::render::decode_image(nullptr, image));
  CHECK(image.width == 0 && image.height == 0 && image.pixels.empty());
  CHECK(!phi::render::decode_image(L"", image));
  const auto icon = std::filesystem::path(argv[1]).wstring();
  CHECK(phi::render::decode_image(icon.c_str(), image));
  CHECK(image.width > 0 && image.height > 0);
  CHECK(image.pixels.size() == static_cast<std::size_t>(image.width) * image.height * 4);
  const auto missing = icon + L".missing";
  CHECK(!phi::render::decode_image(missing.c_str(), image));
  CHECK(image.width == 0 && image.height == 0 && image.pixels.empty());
  CHECK(phi::render::decode_image(icon.c_str(), image));
  return 0;
}
