#pragma GCC optimize ("O3,unroll-loops")
#include "emu_atari.h"
#include "../hal/config.h"
#include "../ui/display_emu.h"
#include "../ui/theme.h"
#include <Arduino.h>

#if BMO_EXPERIMENTAL_CORES
#include "../engines/stella/atari.h"
#include "../hal/buttons.h"
#include "../storage/save_manager.h"
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
  (void)romFilename;
  return false;
}

bool AtariEmu::loadBatteryRam(const char* romFilename) {
  (void)romFilename;
  return false;
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

#else // BMO_EXPERIMENTAL_CORES == 0
bool AtariEmu::begin(const uint8_t* romData, size_t romSize) {
  (void)romData;
  (void)romSize;
  LOG_WARN_STR("[atari] Experimental core disabled (-DBMO_EXPERIMENTAL_CORES=1 to enable)");
  DisplayEmu::showToast("EXPERIMENTAL CORE OFF", Theme::ACCENT_RED, 2200);
  return false;
}

void AtariEmu::updateJoypad() {}
void AtariEmu::runFrame() {}
void AtariEmu::destroy() {}
bool AtariEmu::saveBatteryRam(const char* romFilename) { (void)romFilename; return false; }
bool AtariEmu::loadBatteryRam(const char* romFilename) { (void)romFilename; return false; }
bool AtariEmu::saveState(const char* romFilename, int slot) { (void)romFilename; (void)slot; return false; }
bool AtariEmu::loadState(const char* romFilename, int slot) { (void)romFilename; (void)slot; return false; }

#endif // BMO_EXPERIMENTAL_CORES
