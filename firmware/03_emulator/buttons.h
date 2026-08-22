#pragma once
// -----------------------------------------------------------------------
// buttons.h - Debounce-free polled button reading module.
//
// Owns all button state. Nothing outside this module should call
// digitalRead() on a button pin directly - go through this API instead,
// so button handling logic lives in exactly one place.
// -----------------------------------------------------------------------

#include <Arduino.h>

struct ButtonState {
  const char *name;
  uint8_t pin;
  bool pressed;   // current debounced state (true = physically pressed)
  bool changed;   // true only on the loop() iteration the state flipped
};

namespace Buttons {
  // Index into the button table. MUST stay in this exact order -
  // it has to match the states[] initializer order in buttons.cpp.
  enum Index : int { UP = 0, DOWN, LEFT, RIGHT, A, B, START, SELECT };

  // Call once in setup(). Configures all button pins as INPUT_PULLUP.
  void begin();

  // Call once per loop(). Refreshes pressed/changed for every button.
  void update();

  // Number of buttons tracked.
  int count();

  // Read-only access to a button's current state by index.
  const ButtonState &get(int index);
}
