#include <stdint.h>
#include <string.h>
#include <vector>

#include "cores/pico_blit.h"
#include "test_check.h"

namespace {

void referenceRenderPico(const uint8_t* picoFb, const uint8_t* screenPalMap, const uint16_t* pal, uint8_t drawMode,
                         uint16_t* dst) {
  const int scaleX = (drawMode == 1 || drawMode == 3) ? 2 : 1;
  const int scaleY = (drawMode == 2 || drawMode == 3) ? 2 : 1;
  for (int y = 0; y < 128; ++y) {
    for (int x = 0; x < 128; ++x) {
      int sx = x;
      int sy = y;
      switch (drawMode) {
      case 129:
        sx = 127 - x;
        break;
      case 130:
        sy = 127 - y;
        break;
      case 131:
      case 134:
        sx = 127 - x;
        sy = 127 - y;
        break;
      case 133:
        sx = y;
        sy = 127 - x;
        break;
      case 135:
        sx = 127 - y;
        sy = x;
        break;
      default:
        break;
      }
      const int picox = sx / scaleX;
      const int picoy = sy / scaleY;
      const uint8_t packed = picoFb[picoy * 64 + (picox >> 1)];
      const uint8_t rawIdx = (picox & 1) ? (packed >> 4) : (packed & 0x0FU);
      dst[y * 128 + x] = pal[screenPalMap[rawIdx] & 0x8FU];
    }
  }
}

} // namespace

int main() {
  std::vector<uint8_t> picoFb(128U * 64U);
  for (size_t i = 0; i < picoFb.size(); ++i) {
    picoFb[i] = static_cast<uint8_t>(((i * 13U + 5U) & 0xFFU));
  }
  uint8_t screenPalMap[16];
  for (int i = 0; i < 16; ++i) {
    screenPalMap[i] = (i % 3 == 0) ? static_cast<uint8_t>(128 + ((15 - i) & 0x0F)) : static_cast<uint8_t>(i);
  }
  uint16_t pal[144];
  for (int i = 0; i < 144; ++i) {
    pal[i] = static_cast<uint16_t>((i * 457U + 0x1021U) & 0xFFFFU);
  }

  std::vector<uint16_t> expected(128U * 128U, 0);
  std::vector<uint16_t> actual(128U * 128U, 0);
  const uint8_t modes[] = {0, 1, 2, 3, 129, 130, 131, 133, 134, 135};
  for (uint8_t mode : modes) {
    referenceRenderPico(picoFb.data(), screenPalMap, pal, mode, expected.data());
    PicoBlit::renderFrame(picoFb.data(), screenPalMap, pal, mode, actual.data());
    BMO_CHECK(memcmp(expected.data(), actual.data(), expected.size() * sizeof(uint16_t)) == 0);
  }
  return 0;
}
