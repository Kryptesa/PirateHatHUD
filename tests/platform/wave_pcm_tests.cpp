#include "platform/wave_pcm.hpp"
#include <algorithm>
#include <array>
#include <limits>
#include <vector>
#define CHECK(c)                                                                                   \
  do {                                                                                             \
    if (!(c)) {                                                                                    \
      return __LINE__;                                                                             \
    }                                                                                              \
  } while (false)
int main() {
  const std::vector<uint8_t> original = {
    'R',
    'I',
    'F',
    'F',
    44,
    0,
    0,
    0,
    'W',
    'A',
    'V',
    'E',
    'f',
    'm',
    't',
    ' ',
    16,
    0,
    0,
    0,
    1,
    0,
    1,
    0,
    0x40,
    0x1F,
    0,
    0,
    0x80,
    0x3E,
    0,
    0,
    2,
    0,
    16,
    0,
    'd',
    'a',
    't',
    'a',
    8,
    0,
    0,
    0,
    0,
    0x80,
    0xFF,
    0x7F,
    0,
    0,
    0x10,
    0x27
  };
  const auto format = phi::detail::parse_pcm_wave(original);
  CHECK(format && format->data_offset == 44 && format->data_size == 8);
  auto bytes = original;
  CHECK(phi::detail::scale_pcm_wave(bytes, *format, 0.5f));
  const std::array<uint8_t, 8> half = {0, 0xC0, 0, 0x40, 0, 0, 0x88, 0x13};
  CHECK(std::equal(half.begin(), half.end(), bytes.begin() + 44));
  CHECK(std::equal(original.begin(), original.begin() + 44, bytes.begin()));
  CHECK(phi::detail::scale_pcm_wave(bytes, *format, 0));
  CHECK(std::all_of(bytes.begin() + 44, bytes.end(), [](auto b) { return b == 0; }));
  bytes = original;
  CHECK(phi::detail::scale_pcm_wave(bytes, *format, 1));
  CHECK(bytes == original);
  for (float gain :
    {-1.f, 1.1f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()}) {
    CHECK(!phi::detail::scale_pcm_wave(bytes, *format, gain));
    CHECK(bytes == original);
  }
  auto invalid = *format;
  invalid.data_size = SIZE_MAX;
  CHECK(!phi::detail::scale_pcm_wave(bytes, invalid, 0.5f));
  bytes.pop_back();
  CHECK(!phi::detail::parse_pcm_wave(bytes));

  const std::vector<uint8_t> unsigned_pcm{0, 128, 255, 192};
  auto pcm = unsigned_pcm;
  const phi::detail::PcmWave eight{0, 4, 8};
  CHECK(phi::detail::scale_pcm_wave(pcm, eight, 0.5f));
  CHECK(pcm == std::vector<uint8_t>({64, 128, 192, 160}));
  CHECK(phi::detail::scale_pcm_wave(pcm, eight, 0));
  CHECK(pcm == std::vector<uint8_t>({128, 128, 128, 128}));
}
