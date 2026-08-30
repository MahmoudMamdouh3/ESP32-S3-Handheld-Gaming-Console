#include "emu_pico.h"
#include "../vendor/pico/pico.h"
#include "../core/display_emu.h"
#include "../core/buttons.h"
#include <Arduino.h>

static pico_context_t* pico_ctx = nullptr;

bool PicoEmu::begin(const uint8_t* romData, size_t romSize) {
  if (pico_ctx) {
    pico_destroy(pico_ctx);
    pico_ctx = nullptr;
  }
  
  pico_ctx = pico_create();
  if (!pico_ctx) {
    return false;
  }
  
  if (!pico_load_cart(pico_ctx, romData, romSize)) {
    pico_destroy(pico_ctx);
    pico_ctx = nullptr;
    return false;
  }
  return true;
}

void PicoEmu::updateJoypad() {
  if (!pico_ctx) return;
  
  // PICO-8 Bitmask: 0: Left, 1: Right, 2: Up, 3: Down, 4: O (A), 5: X (B), 6: Pause (Start)
  uint8_t pad = 0;
  if (Buttons::get(Buttons::LEFT).pressed)   pad |= (1 << 0);
  if (Buttons::get(Buttons::RIGHT).pressed)  pad |= (1 << 1);
  if (Buttons::get(Buttons::UP).pressed)     pad |= (1 << 2);
  if (Buttons::get(Buttons::DOWN).pressed)   pad |= (1 << 3);
  if (Buttons::get(Buttons::A).pressed)      pad |= (1 << 4); // Button O
  if (Buttons::get(Buttons::B).pressed)      pad |= (1 << 5); // Button X
  if (Buttons::get(Buttons::START).pressed)  pad |= (1 << 6); // Pause
  
  pico_set_input(pico_ctx, pad);
}

void PicoEmu::runFrame() {
  if (!pico_ctx) return;
  
  updateJoypad();
  pico_run_frame(pico_ctx);
  
  if (pico_ctx->framebuffer) {
    DisplayEmu::streamPicoFrame(pico_ctx->framebuffer);
  }
}

void PicoEmu::destroy() {
  if (pico_ctx) {
    pico_destroy(pico_ctx);
    pico_ctx = nullptr;
  }
}
