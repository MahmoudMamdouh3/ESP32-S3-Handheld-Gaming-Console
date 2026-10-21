#pragma GCC optimize ("O3,unroll-loops")
#include "emu_atari.h"
#include "../vendor/stella/atari.h"
#include "../core/display_emu.h"
#include "../core/buttons.h"
#include "../core/save_manager.h"
#include <Arduino.h>
#include <string.h>

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

  // Atari Joystick bits (active-low): 0: Up, 1: Down, 2: Left, 3: Right, 4: Fire (A)
  uint8_t pad = 0xFF;
  if (Buttons::get(Buttons::UP).pressed)    pad &= ~(1 << 0);
  if (Buttons::get(Buttons::DOWN).pressed)  pad &= ~(1 << 1);
  if (Buttons::get(Buttons::LEFT).pressed)  pad &= ~(1 << 2);
  if (Buttons::get(Buttons::RIGHT).pressed) pad &= ~(1 << 3);
  if (Buttons::get(Buttons::A).pressed)     pad &= ~(1 << 4);

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

bool AtariEmu::saveBatteryRam(const char* romFilename) {
  if (!atari_ctx || !romFilename) return false;
  return SaveManager::saveBatteryRam(romFilename, atari_ctx->ram, sizeof(atari_ctx->ram));
}

bool AtariEmu::loadBatteryRam(const char* romFilename) {
  if (!atari_ctx || !romFilename) return false;
  return SaveManager::loadBatteryRam(romFilename, atari_ctx->ram, sizeof(atari_ctx->ram));
}

bool AtariEmu::saveState(const char* romFilename, int slot) {
  if (!atari_ctx || !romFilename) return false;
  return SaveManager::saveState(romFilename, slot, SaveManager::CORE_ATARI,
                                atari_ctx, sizeof(atari_context_t));
}

bool AtariEmu::loadState(const char* romFilename, int slot) {
  if (!atari_ctx || !romFilename) return false;
  atari_context_t tempCtx;
  size_t ctxSize = 0;
  bool ok = SaveManager::loadState(romFilename, slot, SaveManager::CORE_ATARI,
                                   &tempCtx, sizeof(tempCtx), &ctxSize);
  if (ok && ctxSize == sizeof(atari_context_t)) {
    const uint8_t* rom = atari_ctx->rom;
    size_t rom_size = atari_ctx->rom_size;
    uint16_t* fb = atari_ctx->framebuffer;
    memcpy(atari_ctx, &tempCtx, sizeof(atari_context_t));
    atari_ctx->rom = rom;
    atari_ctx->rom_size = rom_size;
    atari_ctx->framebuffer = fb;
    return true;
  }
  return false;
}
