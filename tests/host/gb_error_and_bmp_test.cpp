#include <limits.h>
#include <stdint.h>
#include <string.h>
#include <Arduino.h>

#include "cores/gb_common.h"
#include "hal/config.h"
#include "hal/psram_alloc.h"
#include "storage/bmp_header.h"
#include "test_check.h"

namespace DisplayEmu {
void waitForDisplay() {}
} // namespace DisplayEmu

static_assert(FEATURE_AUDIO == 0, "FEATURE_AUDIO must stay 0");
static_assert(FEATURE_BATTERY_MONITOR == 0, "FEATURE_BATTERY_MONITOR must stay 0");
static_assert(TFT_CS == 10 && TFT_DC == 8 && TFT_RST == 9 && TFT_SCK == 12 && TFT_MOSI == 11, "TFT FSPI pins fixed");
static_assert(SD_CS == 13 && SD_MISO == 15, "SD FSPI pins fixed");

int main() {
  // 1) GbCommon error propagation recovers cleanly without rebooting
  GbCommon::clearError();
  int errCode = -1;
  uint16_t errAddr = 0x1234U;
  BMO_CHECK(!GbCommon::consumeError(&errCode, &errAddr));
  BMO_CHECK(errCode == 0 && errAddr == 0U);

  GbCommon::reportError("WalnutTest", 3, 0xBEEFU);
  BMO_CHECK(GbCommon::consumeError(&errCode, &errAddr));
  BMO_CHECK(errCode == 3);
  BMO_CHECK(errAddr == 0xBEEFU);
  // Consumed once -> must reset cleanly so next frame does not re-trigger
  BMO_CHECK(!GbCommon::consumeError(&errCode, &errAddr));

  // 2) BmpHeader::write24BppHeader round-trips cleanly via BmpHeader::parse64x64
  uint8_t bmp64[54];
  BmpHeader::write24BppHeader(bmp64, 64, -64, 64U * 64U * 3U);
  BmpHeader::Info parsed{};
  BMO_CHECK(BmpHeader::parse64x64(bmp64, 54U + 64U * 64U * 3U, &parsed));
  BMO_CHECK(parsed.bpp == 24U);
  BMO_CHECK(!parsed.bottomUp);
  BMO_CHECK(parsed.dataOffset == 54U);

  uint8_t bmp320[54];
  BmpHeader::write24BppHeader(bmp320, 320, -240, 320U * 240U * 3U);
  BMO_CHECK(bmp320[0] == 'B' && bmp320[1] == 'M');
  BMO_CHECK(BmpHeader::readLe32(bmp320 + 2U) == 54U + 320U * 240U * 3U);
  BMO_CHECK(BmpHeader::readLe32(bmp320 + 10U) == 54U);
  BMO_CHECK(static_cast<int32_t>(BmpHeader::readLe32(bmp320 + 18U)) == 320);
  BMO_CHECK(static_cast<int32_t>(BmpHeader::readLe32(bmp320 + 22U)) == -240);
  BMO_CHECK(BmpHeader::readLe16(bmp320 + 28U) == 24U);

  // 3) PSRAM allocation helper rejects integer multiplication overflow cleanly
  void* overflowed = bmo_psram_calloc((SIZE_MAX / 2U) + 1U, 4U);
  BMO_CHECK(overflowed == nullptr);

  uint8_t* smallBuf = static_cast<uint8_t*>(bmo_psram_calloc(16U, 4U));
  BMO_CHECK(smallBuf != nullptr);
  for (size_t i = 0; i < 64U; ++i) {
    BMO_CHECK(smallBuf[i] == 0U);
  }
  bmo_psram_free(smallBuf);
  return 0;
}
