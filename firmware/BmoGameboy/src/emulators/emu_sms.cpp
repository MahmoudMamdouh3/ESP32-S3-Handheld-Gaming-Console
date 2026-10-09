#pragma GCC optimize ("O3,unroll-loops")
#include "emu_sms.h"
#include "../vendor/smsplus/sms.h"
#include "../core/display_emu.h"
#include "../core/buttons.h"
#include "../core/save_manager.h"
#include <Arduino.h>

static sms_context_t* sms_ctx = nullptr;
static bool s_is_gamegear = false;

bool SmsEmu::begin(const uint8_t* romData, size_t romSize, bool isGameGear) {
  if (sms_ctx) {
    sms_destroy(sms_ctx);
    sms_ctx = nullptr;
  }
  s_is_gamegear = isGameGear;
  sms_ctx = sms_create();
  if (!sms_ctx) {
    return false;
  }
  
  if (!sms_load_rom(sms_ctx, romData, romSize, isGameGear)) {
    sms_destroy(sms_ctx);
    sms_ctx = nullptr;
    return false;
  }
  return true;
}

void SmsEmu::updateJoypad() {
  if (!sms_ctx) return;
  
  // SMS Port $DC bits (active-low 0=pressed):
  // Bit 0: Up, Bit 1: Down, Bit 2: Left, Bit 3: Right, Bit 4: Button 1 (B), Bit 5: Button 2 (A)
  uint8_t pad = 0xFF;
  if (Buttons::get(Buttons::UP).pressed)    pad &= ~(1 << 0);
  if (Buttons::get(Buttons::DOWN).pressed)  pad &= ~(1 << 1);
  if (Buttons::get(Buttons::LEFT).pressed)  pad &= ~(1 << 2);
  if (Buttons::get(Buttons::RIGHT).pressed) pad &= ~(1 << 3);
  if (Buttons::get(Buttons::B).pressed)      pad &= ~(1 << 4); // Button 1
  if (Buttons::get(Buttons::A).pressed)      pad &= ~(1 << 5); // Button 2
  
  uint8_t sys = 0xFF;
  if (Buttons::get(Buttons::START).pressed) sys &= ~(1 << 0); // Pause / Start
  
  sms_set_input(sms_ctx, pad, sys);
}

void SmsEmu::runFrame() {
  if (!sms_ctx) return;
  
  // PERF-L3: updateJoypad() is called here internally (NOT from BmoGameboy.ino).
  // Unlike GB/NES which expose separate updateJoypad() calls in the main loop,
  // SMS polls buttons inside runFrame() — do NOT add a redundant external call.
  updateJoypad();
  sms_run_frame(sms_ctx);
  
  if (sms_ctx->framebuffer) {
    DisplayEmu::streamSMSFrame(sms_ctx->framebuffer, s_is_gamegear);
  }
}

void SmsEmu::destroy() {
  if (sms_ctx) {
    sms_destroy(sms_ctx);
    sms_ctx = nullptr;
  }
}

bool SmsEmu::saveBatteryRam(const char* romFilename) {
  if (!sms_ctx || !romFilename) return false;
  return SaveManager::saveBatteryRam(romFilename, sms_ctx->cart_ram, sizeof(sms_ctx->cart_ram));
}

bool SmsEmu::loadBatteryRam(const char* romFilename) {
  if (!sms_ctx || !romFilename) return false;
  return SaveManager::loadBatteryRam(romFilename, sms_ctx->cart_ram, sizeof(sms_ctx->cart_ram));
}

bool SmsEmu::saveState(const char* romFilename, int slot) {
  if (!sms_ctx || !romFilename) return false;
  return SaveManager::saveState(romFilename, slot, SaveManager::CORE_SMS,
                                sms_ctx, sizeof(sms_context_t));
}

bool SmsEmu::loadState(const char* romFilename, int slot) {
  if (!sms_ctx || !romFilename) return false;
  sms_context_t tempCtx;
  size_t ctxSize = 0;
  bool ok = SaveManager::loadState(romFilename, slot, SaveManager::CORE_SMS,
                                   &tempCtx, sizeof(tempCtx), &ctxSize);
  if (ok && ctxSize == sizeof(sms_context_t)) {
    const uint8_t* rom = sms_ctx->rom;
    size_t rom_size = sms_ctx->rom_size;
    uint16_t* fb = sms_ctx->framebuffer;
    memcpy(sms_ctx, &tempCtx, sizeof(sms_context_t));
    sms_ctx->rom = rom;
    sms_ctx->rom_size = rom_size;
    sms_ctx->framebuffer = fb;
    return true;
  }
  return false;
}
