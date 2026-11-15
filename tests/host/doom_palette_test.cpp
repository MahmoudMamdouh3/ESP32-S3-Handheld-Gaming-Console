#include <stdint.h>
#include <string.h>

#include "src/ui/doom_palette.h"
#include "test_check.h"

namespace {

struct TestDoomColor {
  uint32_t b : 8;
  uint32_t g : 8;
  uint32_t r : 8;
  uint32_t a : 8;
};

struct LegacyCachedPaletteState {
  TestDoomColor lastColors[256]{};
  uint16_t doomPalette[256]{};

  void streamStep(const TestDoomColor* currentColors) {
    if (memcmp(lastColors, currentColors, sizeof(lastColors)) != 0) {
      memcpy(lastColors, currentColors, sizeof(lastColors));
      for (int i = 0; i < 256; ++i) {
        const TestDoomColor c = currentColors[i];
        const uint16_t p =
            static_cast<uint16_t>(((static_cast<uint16_t>(c.b) & 0xF8U) << 8) |
                                  ((static_cast<uint16_t>(c.g) & 0xFCU) << 3) | (static_cast<uint16_t>(c.r) >> 3));
        doomPalette[i] = static_cast<uint16_t>((p >> 8) | (p << 8));
      }
    }
  }
};

void fillDeterministicPalette(TestDoomColor* dst, uint32_t seed, uint8_t alpha) {
  for (int i = 0; i < 256; ++i) {
    seed = seed * 1664525U + 1013904223U;
    dst[i].r = static_cast<uint8_t>((seed >> 24) & 0xFFU);
    dst[i].g = static_cast<uint8_t>((seed >> 16) & 0xFFU);
    dst[i].b = static_cast<uint8_t>((seed >> 8) & 0xFFU);
    dst[i].a = alpha;
  }
}

} // namespace

int main() {
  LegacyCachedPaletteState cachedRef{};
  uint16_t uncachedPalette[256]{};
  TestDoomColor activeColors[256]{};

  // Frame 0: zero-initialized colors must match cached {0} baseline identically.
  cachedRef.streamStep(activeColors);
  Ui::rebuildDoomPalette(activeColors, uncachedPalette, 256U);
  BMO_CHECK(memcmp(cachedRef.doomPalette, uncachedPalette, sizeof(uncachedPalette)) == 0);

  // Frame 1: first real DOOM palette load.
  fillDeterministicPalette(activeColors, 0x12345678U, 0x00U);
  cachedRef.streamStep(activeColors);
  Ui::rebuildDoomPalette(activeColors, uncachedPalette, 256U);
  BMO_CHECK(memcmp(cachedRef.doomPalette, uncachedPalette, sizeof(uncachedPalette)) == 0);

  // Frame 2: unchanged colors (cache hit in legacy code) must produce bit-identical palette.
  cachedRef.streamStep(activeColors);
  Ui::rebuildDoomPalette(activeColors, uncachedPalette, 256U);
  BMO_CHECK(memcmp(cachedRef.doomPalette, uncachedPalette, sizeof(uncachedPalette)) == 0);

  // Frame 3: alpha-only mutation (memcmp miss in legacy code, unchanged RGB) produces identical output.
  fillDeterministicPalette(activeColors, 0x12345678U, 0xFFU);
  cachedRef.streamStep(activeColors);
  Ui::rebuildDoomPalette(activeColors, uncachedPalette, 256U);
  BMO_CHECK(memcmp(cachedRef.doomPalette, uncachedPalette, sizeof(uncachedPalette)) == 0);

  // Frame 4: palette transition (e.g., damage/pickup flash in DOOM) + pixel-conversion parity.
  fillDeterministicPalette(activeColors, 0xDEADBEEFU, 0x80U);
  cachedRef.streamStep(activeColors);
  Ui::rebuildDoomPalette(activeColors, uncachedPalette, 256U);
  BMO_CHECK(memcmp(cachedRef.doomPalette, uncachedPalette, sizeof(uncachedPalette)) == 0);
  for (int idx = 0; idx < 256; ++idx) {
    BMO_CHECK(cachedRef.doomPalette[idx] == uncachedPalette[idx]);
  }
  return 0;
}
