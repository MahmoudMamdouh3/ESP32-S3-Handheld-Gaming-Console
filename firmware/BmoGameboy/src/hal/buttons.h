#pragma once
// -----------------------------------------------------------------------
// buttons.h - Debounce-free polled button reading module.
//
// Owns all button state. Nothing outside this module should call
// digitalRead() on a button pin directly - go through this API instead,
// so button handling logic lives in exactly one place.
// -----------------------------------------------------------------------

#include <Arduino.h>
#include "input_frame.h"

struct ButtonState {
  const char* name;
  uint8_t pin;
  bool pressed; // raw low-latency state for active emulators
  bool changed; // true only on the loop() iteration the state flipped
};

namespace Buttons {
// Index into the button table. MUST stay in this exact order -
// it has to match the states[] initializer order in buttons.cpp.
enum Index : int { UP = 0, DOWN, LEFT, RIGHT, A, B, START, SELECT, X, Y, L, R };

// Fast Game Boy-compatible bitmask for zero-wait state polling
extern uint8_t gb_joypad_state;

// Call once in setup(). Configures all button pins as INPUT_PULLUP.
void begin();

// Call once per loop(). Refreshes raw state for cores and debounced/repeating
// InputFrame for menus.
void update(bool enableMenuRepeat = true);

// Number of buttons tracked.
int count();

// Read-only access to a button's current raw state by index.
const ButtonState& get(int index);

// Debounced menu input snapshot (20ms debounce, 350ms/100ms D-pad repeat).
const BmoInput::InputFrame& inputFrame();

// Suppress menu edges/repeats until `untilMs` after a mode switch.
void suppressMenuInputUntil(uint32_t untilMs);

// Hardware bitmask (bits 0..11 matching Index enum) before gameplay remap/turbo.
uint16_t rawMask();

// Per-Core Button Remap (A/B Swap) & Auto-Fire Turbo Controls
void setGameplayMode(bool inGame);
void setSwapAB(bool swap);
bool isSwapAB();
bool toggleSwapAB();

void setTurboEnabled(bool enabled, uint8_t periodFrames = 4);
bool isTurboEnabled();
bool toggleTurbo();
} // namespace Buttons
