#include "emu_atari.h"
#include "../vendor/stella/atari.h"
#include "../core/display_emu.h"
#include "../core/buttons.h"
#include <Arduino.h>

static atari_context_t* atari_ctx = nullptr;

bool AtariEmu::begin(const uint8_t* romData, size_t romSize) {
  if (atari_ctx) {
    atari_destroy(atari_ctx);
    atari_ctx = nullptr;
  }
  
  atari_ctx = atari_create();
  if (!atari_ctx) {
    return false;
  }
  
  if (!atari_load_rom(atari_ctx, romData, romSize)) {
    atari_destroy(atari_ctx);
    atari_ctx = nullptr;
    return false;
  }
  return true;
}

void AtariEmu::updateJoypad() {
  if (!atari_ctx) return;
  
  // Atari Joystick bits: 0: Up, 1: Down, 2: Left, 3: Right, 4: Fire (A)
  uint8_t pad = 0xFF;
  if (Buttons::get(Buttons::UP).pressed)    pad &= ~(1 << 0);
  if (Buttons::get(Buttons::DOWN).pressed)  pad &= ~(1 << 1);
  if (Buttons::get(Buttons::LEFT).pressed)  pad &= ~(1 << 2);
  if (Buttons::get(Buttons::RIGHT).pressed) pad &= ~(1 << 3);
  if (Buttons::get(Buttons::A).pressed)      pad &= ~(1 << 4);
  
  atari_set_input(atari_ctx, pad);
}

void AtariEmu::runFrame() {
  if (!atari_ctx) return;
  
  updateJoypad();
  atari_run_frame(atari_ctx);
  
  if (atari_ctx->framebuffer) {
    DisplayEmu::streamAtariFrame(atari_ctx->framebuffer);
  }
}

void AtariEmu::destroy() {
  if (atari_ctx) {
    atari_destroy(atari_ctx);
    atari_ctx = nullptr;
  }
}
