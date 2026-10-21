#include "buttons.h"
#include "config.h"
#include "soc/gpio_reg.h"

namespace {
  ButtonState states[] = {
    {"Up",     BTN_UP,     false, false},
    {"Down",   BTN_DOWN,   false, false},
    {"Left",   BTN_LEFT,   false, false},
    {"Right",  BTN_RIGHT,  false, false},
    {"A",      BTN_A,      false, false},
    {"B",      BTN_B,      false, false},
    {"Start",  BTN_START,  false, false},
    {"Select", BTN_SELECT, false, false},
  };
  const int NUM_BUTTONS = sizeof(states) / sizeof(states[0]);

  // FIX Q3: File-scope static_assert so the check is always evaluated at
  // compile time, not only when begin() happens to be called.
  static_assert(sizeof(states) / sizeof(states[0]) == 8,
                "Expected exactly 8 buttons for GB joypad mapping");

  bool s_inGameplayMode = false;
  bool s_swapAB = false;
  bool s_turboEnabled = false;
  uint8_t s_turboPeriod = 4;
  uint32_t s_turboTick = 0;
}

namespace Buttons {
  uint8_t gb_joypad_state = 0xFF;
}

void Buttons::begin() {
  for (int i = 0; i < NUM_BUTTONS; i++) {
    pinMode(states[i].pin, INPUT_PULLUP);
  }
}

void Buttons::setGameplayMode(bool inGame) {
  s_inGameplayMode = inGame;
}

void Buttons::setSwapAB(bool swap) {
  s_swapAB = swap;
}

bool Buttons::isSwapAB() {
  return s_swapAB;
}

bool Buttons::toggleSwapAB() {
  s_swapAB = !s_swapAB;
  return s_swapAB;
}

void Buttons::setTurboEnabled(bool enabled, uint8_t periodFrames) {
  s_turboEnabled = enabled;
  s_turboPeriod = (periodFrames < 2) ? 2 : periodFrames;
}

bool Buttons::isTurboEnabled() {
  return s_turboEnabled;
}

bool Buttons::toggleTurbo() {
  s_turboEnabled = !s_turboEnabled;
  return s_turboEnabled;
}

void Buttons::update() {
  // Read all GPIO pins 0-31 in a single CPU cycle.
  // All button pins are guaranteed to be < 32 in config.h.
  uint32_t gpio_in = REG_READ(GPIO_IN_REG);

  bool rawPressed[NUM_BUTTONS];
  for (int i = 0; i < NUM_BUTTONS; i++) {
    rawPressed[i] = ((gpio_in & (1UL << states[i].pin)) == 0);
  }

  // Apply A/B button swap and auto-fire turbo only during active gameplay
  // (never when SELECT is held for system hotkeys or in menus).
  if (s_inGameplayMode && !rawPressed[SELECT]) {
    if (s_swapAB) {
      bool tmp = rawPressed[A];
      rawPressed[A] = rawPressed[B];
      rawPressed[B] = tmp;
    }
    if (s_turboEnabled) {
      s_turboTick++;
      bool pulseOn = ((s_turboTick % s_turboPeriod) < (s_turboPeriod / 2));
      if (rawPressed[A]) rawPressed[A] = pulseOn;
      if (rawPressed[B]) rawPressed[B] = pulseOn;
    }
  }

  for (int i = 0; i < NUM_BUTTONS; i++) {
    states[i].changed = (rawPressed[i] != states[i].pressed);
    states[i].pressed = rawPressed[i];
  }

  uint8_t joypad = 0xFF;
  if (states[UP].pressed)     joypad &= ~0x40u;
  if (states[DOWN].pressed)   joypad &= ~0x80u;
  if (states[LEFT].pressed)   joypad &= ~0x20u;
  if (states[RIGHT].pressed)  joypad &= ~0x10u;
  if (states[A].pressed)      joypad &= ~0x01u;
  if (states[B].pressed)      joypad &= ~0x02u;
  if (states[START].pressed)  joypad &= ~0x08u;
  if (states[SELECT].pressed) joypad &= ~0x04u;

  Buttons::gb_joypad_state = joypad;
}

int Buttons::count() {
  return NUM_BUTTONS;
}

const ButtonState &Buttons::get(int index) {
  // Clamp to valid range — silent UB on embedded is worse than a wrong button
  if (index < 0 || index >= NUM_BUTTONS) index = 0;
  return states[index];
}
