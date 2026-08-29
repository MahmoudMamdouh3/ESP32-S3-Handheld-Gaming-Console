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
}

namespace Buttons {
  uint8_t gb_joypad_state = 0xFF;
}

void Buttons::begin() {
  for (int i = 0; i < NUM_BUTTONS; i++) {
    pinMode(states[i].pin, INPUT_PULLUP);
  }
}

void Buttons::update() {
  // Read all GPIO pins 0-31 in a single CPU cycle.
  // All button pins are guaranteed to be < 32 in config.h.
  uint32_t gpio_in = REG_READ(GPIO_IN_REG);

  uint8_t joypad = 0xFF;
  for (int i = 0; i < NUM_BUTTONS; i++) {
    bool nowPressed = ((gpio_in & (1UL << states[i].pin)) == 0);
    states[i].changed = (nowPressed != states[i].pressed);
    states[i].pressed = nowPressed;
  }
  
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
