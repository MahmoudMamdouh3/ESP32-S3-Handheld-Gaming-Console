#include <stdint.h>

#include "src/hal/input_frame.h"
#include "test_check.h"

int main() {
  BmoInput::Debouncer debouncer(20U, 350U, 100U);

  // Initial raw press resets debounce timer; 10ms bounce must not trigger press.
  auto f0 = debouncer.update(0x0001U, 1000U, true);
  BMO_CHECK(f0.pressedMask == 0U);
  BMO_CHECK(f0.justPressedMask == 0U);

  // Release at 1010ms (bounce glitch) clears raw edge cleanly.
  auto f1 = debouncer.update(0x0000U, 1010U, true);
  BMO_CHECK(f1.pressedMask == 0U);
  BMO_CHECK(f1.justPressedMask == 0U);

  // Clean press held across 20ms stabilizes and produces one rising edge.
  debouncer.update(0x0001U, 1020U, true);
  auto f2 = debouncer.update(0x0001U, 1039U, true);
  BMO_CHECK(f2.pressedMask == 0U);
  auto f3 = debouncer.update(0x0001U, 1040U, true);
  BMO_CHECK(f3.isDown(0));
  BMO_CHECK(f3.edgePressed(0));
  BMO_CHECK(f3.triggeredOrRepeat(0));

  // Holding past initial 350ms wait fires key repeat every 100ms.
  auto f4 = debouncer.update(0x0001U, 1389U, true);
  BMO_CHECK(f4.repeatMask == 0U);
  auto f5 = debouncer.update(0x0001U, 1390U, true);
  BMO_CHECK(f5.repeatMask == 0x0001U);
  BMO_CHECK(f5.triggeredOrRepeat(0));
  BMO_CHECK(!f5.edgePressed(0));

  auto f6 = debouncer.update(0x0001U, 1480U, true);
  BMO_CHECK(f6.repeatMask == 0U);
  auto f7 = debouncer.update(0x0001U, 1490U, true);
  BMO_CHECK(f7.repeatMask == 0x0001U);

  // Cooldown suppression mutes edges/repeats without losing held button tracking.
  debouncer.suppressUntil(1700U);
  auto f8 = debouncer.update(0x0003U, 1600U, true);
  BMO_CHECK(f8.repeatMask == 0U);
  BMO_CHECK(f8.justPressedMask == 0U);

  return 0;
}
