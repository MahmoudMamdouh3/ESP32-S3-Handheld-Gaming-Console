#include <stdint.h>
#include <string.h>

#include "src/app/boot_safe_mode.h"
#include "test_check.h"

int main() {
  constexpr uint32_t DEFAULT_HZ = 80000000UL;
  constexpr uint32_t FALLBACK_HZ = 40000000UL;

  // Normal boot honors valid saved NVS clock.
  {
    const auto d = BmoApp::resolveBootDisplaySpiClock(false, false, DEFAULT_HZ, DEFAULT_HZ, FALLBACK_HZ);
    BMO_CHECK(!d.safeModeForced);
    BMO_CHECK(d.spiHz == DEFAULT_HZ);
  }
  {
    const auto d = BmoApp::resolveBootDisplaySpiClock(false, false, FALLBACK_HZ, DEFAULT_HZ, FALLBACK_HZ);
    BMO_CHECK(!d.safeModeForced);
    BMO_CHECK(d.spiHz == FALLBACK_HZ);
  }
  // Invalid saved clock falls back safely to default 80 MHz unless safe mode requested.
  {
    const auto d = BmoApp::resolveBootDisplaySpiClock(false, false, 123456UL, DEFAULT_HZ, FALLBACK_HZ);
    BMO_CHECK(!d.safeModeForced);
    BMO_CHECK(d.spiHz == DEFAULT_HZ);
  }
  // Holding SELECT or B at boot forces 40 MHz safe mode regardless of saved NVS clock.
  {
    const auto d1 = BmoApp::resolveBootDisplaySpiClock(true, false, DEFAULT_HZ, DEFAULT_HZ, FALLBACK_HZ);
    BMO_CHECK(d1.safeModeForced);
    BMO_CHECK(d1.spiHz == FALLBACK_HZ);
    const auto d2 = BmoApp::resolveBootDisplaySpiClock(false, true, DEFAULT_HZ, DEFAULT_HZ, FALLBACK_HZ);
    BMO_CHECK(d2.safeModeForced);
    BMO_CHECK(d2.spiHz == FALLBACK_HZ);
  }
  // Reset reason classification and human-readable names.
  {
    BMO_CHECK(strcmp(BmoApp::resetReasonName(1), "ESP_RST_POWERON") == 0);
    BMO_CHECK(strcmp(BmoApp::resetReasonName(4), "ESP_RST_PANIC") == 0);
    BMO_CHECK(strcmp(BmoApp::resetReasonName(9), "ESP_RST_BROWNOUT") == 0);
    BMO_CHECK(strcmp(BmoApp::resetReasonName(999), "ESP_RST_UNKNOWN") == 0);
    BMO_CHECK(!BmoApp::isAbnormalResetReason(1));
    BMO_CHECK(!BmoApp::isAbnormalResetReason(3));
    BMO_CHECK(BmoApp::isAbnormalResetReason(4));
    BMO_CHECK(BmoApp::isAbnormalResetReason(6));
    BMO_CHECK(BmoApp::isAbnormalResetReason(9));
  }
  // Boot breadcrumb: valid magic + CRC passes; cold-boot garbage and bit flips fail.
  {
    const auto validMenu = BmoApp::makeBootBreadcrumb(static_cast<uint8_t>(BmoApp::STATE_CONSOLE_MENU), -1);
    BMO_CHECK(BmoApp::isValidBootBreadcrumb(validMenu));

    const auto validEmu = BmoApp::makeBootBreadcrumb(static_cast<uint8_t>(BmoApp::STATE_EMULATOR), 14);
    BMO_CHECK(BmoApp::isValidBootBreadcrumb(validEmu));

    // Uninitialized RTC_NOINIT patterns on cold power-on.
    BmoApp::BootBreadcrumb zeroed{};
    memset(&zeroed, 0x00, sizeof(zeroed));
    BMO_CHECK(!BmoApp::isValidBootBreadcrumb(zeroed));

    BmoApp::BootBreadcrumb ones{};
    memset(&ones, 0xFF, sizeof(ones));
    BMO_CHECK(!BmoApp::isValidBootBreadcrumb(ones));

    BmoApp::BootBreadcrumb pattern{};
    memset(&pattern, 0xA5, sizeof(pattern));
    BMO_CHECK(!BmoApp::isValidBootBreadcrumb(pattern));

    // Deterministic pseudo-random cold-boot SRAM garbage with valid magic but random tail.
    uint32_t lfsr = 0x13579BDFU;
    for (int trial = 0; trial < 256; ++trial) {
      BmoApp::BootBreadcrumb garbage{};
      uint8_t* raw = reinterpret_cast<uint8_t*>(&garbage);
      for (size_t i = 0; i < sizeof(garbage); ++i) {
        lfsr = lfsr * 1664525U + 1013904223U;
        raw[i] = static_cast<uint8_t>(lfsr >> 24);
      }
      if (trial % 4 == 0) {
        garbage.magic = BmoApp::kBreadcrumbMagic; // Even matching magic must fail CRC/bounds check.
      }
      BMO_CHECK(!BmoApp::isValidBootBreadcrumb(garbage));
    }

    // Single-bit flip in CRC or payload fails validation.
    BmoApp::BootBreadcrumb badCrc = validEmu;
    badCrc.crc32 ^= 0x00010000U;
    BMO_CHECK(!BmoApp::isValidBootBreadcrumb(badCrc));

    BmoApp::BootBreadcrumb badPayload = validEmu;
    badPayload.emulatorIndex = 13;
    BMO_CHECK(!BmoApp::isValidBootBreadcrumb(badPayload));

    // Out-of-bounds state or emulator index fails even with a matching CRC.
    const auto outOfRangeState = BmoApp::makeBootBreadcrumb(BmoApp::kBreadcrumbMaxStateCount, 0);
    BMO_CHECK(!BmoApp::isValidBootBreadcrumb(outOfRangeState));
    const auto outOfRangeEmu = BmoApp::makeBootBreadcrumb(static_cast<uint8_t>(BmoApp::STATE_EMULATOR), -2);
    BMO_CHECK(!BmoApp::isValidBootBreadcrumb(outOfRangeEmu));
  }
  return 0;
}
