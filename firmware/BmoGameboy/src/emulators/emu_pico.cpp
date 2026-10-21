#pragma GCC optimize ("O3,unroll-loops")
#include "emu_pico.h"
#include "../vendor/pico/pico.h"
#include "../core/display_emu.h"
#include "../core/buttons.h"
#include "../core/save_manager.h"
#include <Arduino.h>
#include <string.h>

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

bool PicoEmu::saveBatteryRam(const char* romFilename) {
  if (!pico_ctx || !romFilename) return false;
  // PICO-8 Cartridge Data RAM ($5E00-$5EFF, 256 bytes persistent cart data)
  return SaveManager::saveBatteryRam(romFilename, &pico_ctx->ram[0x5E00], 256);
}

bool PicoEmu::loadBatteryRam(const char* romFilename) {
  if (!pico_ctx || !romFilename) return false;
  return SaveManager::loadBatteryRam(romFilename, &pico_ctx->ram[0x5E00], 256);
}

bool PicoEmu::saveState(const char* romFilename, int slot) {
  if (!pico_ctx || !romFilename) return false;
  return SaveManager::saveState(romFilename, slot, SaveManager::CORE_PICO8,
                                pico_ctx, sizeof(pico_context_t));
}

bool PicoEmu::loadState(const char* romFilename, int slot) {
  if (!pico_ctx || !romFilename) return false;
  pico_context_t* tempCtx = (pico_context_t*)heap_caps_malloc(sizeof(pico_context_t), MALLOC_CAP_SPIRAM);
  if (!tempCtx) return false;
  size_t ctxSize = 0;
  bool ok = SaveManager::loadState(romFilename, slot, SaveManager::CORE_PICO8,
                                   tempCtx, sizeof(pico_context_t), &ctxSize);
  if (ok && ctxSize == sizeof(pico_context_t)) {
    const uint8_t* cart = pico_ctx->cart_data;
    size_t cart_size = pico_ctx->cart_size;
    uint16_t* fb = pico_ctx->framebuffer;
    memcpy(pico_ctx, tempCtx, sizeof(pico_context_t));
    pico_ctx->cart_data = cart;
    pico_ctx->cart_size = cart_size;
    pico_ctx->framebuffer = fb;
    heap_caps_free(tempCtx);
    return true;
  }
  heap_caps_free(tempCtx);
  return false;
}
