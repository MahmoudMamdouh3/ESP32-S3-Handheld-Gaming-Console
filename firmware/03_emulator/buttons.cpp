#include "buttons.h"
#include "config.h"

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
}

void Buttons::begin() {
  static_assert(NUM_BUTTONS == 8, "Expected exactly 8 buttons for GB joypad mapping");
  for (int i = 0; i < NUM_BUTTONS; i++) {
    pinMode(states[i].pin, INPUT_PULLUP);
  }
}

void Buttons::update() {
  for (int i = 0; i < NUM_BUTTONS; i++) {
    bool nowPressed = (digitalRead(states[i].pin) == LOW);
    states[i].changed = (nowPressed != states[i].pressed);
    states[i].pressed = nowPressed;
  }
}

int Buttons::count() {
  return NUM_BUTTONS;
}

const ButtonState &Buttons::get(int index) {
  // Clamp to valid range — silent UB on embedded is worse than a wrong button
  if (index < 0 || index >= NUM_BUTTONS) index = 0;
  return states[index];
}
