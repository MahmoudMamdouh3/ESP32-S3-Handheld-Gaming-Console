#pragma GCC optimize ("O3,unroll-loops")
#include "emu_pce.h"
#include "../vendor/pce/pce.h"
#include "../core/display_emu.h"
#include "../core/buttons.h"
#include <Arduino.h>

static pce_context_t* pce_ctx = nullptr;

bool PceEmu::begin(const uint8_t* romData, size_t romSize) {
  if (pce_ctx) {
    pce_destroy(pce_ctx);
    pce_ctx = nullptr;
  }
  
  pce_ctx = pce_create();
  if (!pce_ctx) {
    return false;
  }
  
  if (!pce_load_rom(pce_ctx, romData, romSize)) {
    pce_destroy(pce_ctx);
    pce_ctx = nullptr;
    return false;
  }
  return true;
}

void PceEmu::updateJoypad() {
  if (!pce_ctx) return;
  
  // PCE Pad bits: 0: I (B), 1: II (A), 2: Select, 3: Run (Start), 4: Up, 5: Right, 6: Down, 7: Left
  uint8_t pad = 0xFF;
  if (Buttons::get(Buttons::B).pressed)      pad &= ~(1 << 0); // Button I
  if (Buttons::get(Buttons::A).pressed)      pad &= ~(1 << 1); // Button II
  if (Buttons::get(Buttons::SELECT).pressed) pad &= ~(1 << 2);
  if (Buttons::get(Buttons::START).pressed)  pad &= ~(1 << 3);
  if (Buttons::get(Buttons::UP).pressed)     pad &= ~(1 << 4);
  if (Buttons::get(Buttons::RIGHT).pressed)  pad &= ~(1 << 5);
  if (Buttons::get(Buttons::DOWN).pressed)   pad &= ~(1 << 6);
  if (Buttons::get(Buttons::LEFT).pressed)   pad &= ~(1 << 7);
  
  pce_set_input(pce_ctx, pad);
}

void PceEmu::runFrame() {
  if (!pce_ctx) return;
  
  updateJoypad();
  pce_run_frame(pce_ctx);
  
  if (pce_ctx->framebuffer) {
    DisplayEmu::streamPCEFrame(pce_ctx->framebuffer);
  }
}

void PceEmu::destroy() {
  if (pce_ctx) {
    pce_destroy(pce_ctx);
    pce_ctx = nullptr;
  }
}
