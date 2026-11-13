#include <stdint.h>

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
  return 0;
}
