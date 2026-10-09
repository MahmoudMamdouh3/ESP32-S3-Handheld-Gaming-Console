#pragma GCC optimize ("O3,unroll-loops")
#include "emu_nes.h"
#include "../vendor/agnes/agnes.h"
#include "../core/display_emu.h"
#include "../core/buttons.h"
#include "../core/save_manager.h"
#include <Arduino.h>

#include <esp_heap_caps.h>

static agnes_t* agnes_ctx = nullptr;
static uint8_t* s_nesStateBuf = nullptr;
static size_t s_nesStateSize = 0;

static uint8_t* getNesStateBuf() {
  if (!s_nesStateBuf) {
    s_nesStateSize = agnes_state_size();
    if (s_nesStateSize > 0) {
      s_nesStateBuf = (uint8_t*)heap_caps_malloc(s_nesStateSize, MALLOC_CAP_SPIRAM);
    }
  }
  return s_nesStateBuf;
}

bool NesEmu::begin(const uint8_t* romData, size_t romSize) {
  if (agnes_ctx) {
    agnes_destroy(agnes_ctx);
  }
  agnes_ctx = agnes_make();
  if (!agnes_ctx) {
    return false;
  }
  
  // Cast away const since agnes loads data (it doesn't modify it, but API lacks const)
  if (!agnes_load_ines_data(agnes_ctx, (void*)romData, romSize)) {
    agnes_destroy(agnes_ctx);
    agnes_ctx = nullptr;
    return false;
  }
  return true;
}

void NesEmu::updateJoypad() {
  if (!agnes_ctx) return;
  
  agnes_input_t input = {0};
  
  // Map Gameboy buttons to NES buttons
  input.a = Buttons::get(Buttons::A).pressed;
  input.b = Buttons::get(Buttons::B).pressed;
  input.select = Buttons::get(Buttons::SELECT).pressed;
  input.start = Buttons::get(Buttons::START).pressed;
  input.up = Buttons::get(Buttons::UP).pressed;
  input.down = Buttons::get(Buttons::DOWN).pressed;
  input.left = Buttons::get(Buttons::LEFT).pressed;
  input.right = Buttons::get(Buttons::RIGHT).pressed;
  
  // Set controller 1, controller 2 is null
  agnes_set_input(agnes_ctx, &input, nullptr);
}

void NesEmu::runFrame() {
  if (!agnes_ctx) return;
  
  // Render one frame
  agnes_next_frame(agnes_ctx);
  
  // Stream to display
  const uint8_t* frame_buffer = agnes_get_screen_buffer(agnes_ctx);
  DisplayEmu::streamNESFrame(frame_buffer);
}

void NesEmu::destroy() {
  if (agnes_ctx) {
    agnes_destroy(agnes_ctx);
    agnes_ctx = nullptr;
  }
  if (s_nesStateBuf) {
    heap_caps_free(s_nesStateBuf);
    s_nesStateBuf = nullptr;
    s_nesStateSize = 0;
  }
}

bool NesEmu::saveBatteryRam(const char* romFilename) {
  if (!agnes_ctx || !romFilename) return false;
  uint8_t* buf = getNesStateBuf();
  if (!buf || s_nesStateSize == 0) return false;
  agnes_dump_state(agnes_ctx, (agnes_state_t*)buf);
  return SaveManager::saveBatteryRam(romFilename, buf, s_nesStateSize);
}

bool NesEmu::loadBatteryRam(const char* romFilename) {
  if (!agnes_ctx || !romFilename) return false;
  uint8_t* buf = getNesStateBuf();
  if (!buf || s_nesStateSize == 0) return false;
  size_t loadedSize = 0;
  if (SaveManager::loadBatteryRam(romFilename, buf, s_nesStateSize, &loadedSize)) {
    if (loadedSize == s_nesStateSize) {
      return agnes_restore_state(agnes_ctx, (const agnes_state_t*)buf);
    }
  }
  return false;
}

bool NesEmu::saveState(const char* romFilename, int slot) {
  if (!agnes_ctx || !romFilename) return false;
  uint8_t* buf = getNesStateBuf();
  if (!buf || s_nesStateSize == 0) return false;
  agnes_dump_state(agnes_ctx, (agnes_state_t*)buf);
  return SaveManager::saveState(romFilename, slot, SaveManager::CORE_NES,
                                buf, s_nesStateSize);
}

bool NesEmu::loadState(const char* romFilename, int slot) {
  if (!agnes_ctx || !romFilename) return false;
  uint8_t* buf = getNesStateBuf();
  if (!buf || s_nesStateSize == 0) return false;
  size_t stateSize = 0;
  bool ok = SaveManager::loadState(romFilename, slot, SaveManager::CORE_NES,
                                   buf, s_nesStateSize, &stateSize);
  if (ok && stateSize == s_nesStateSize) {
    return agnes_restore_state(agnes_ctx, (const agnes_state_t*)buf);
  }
  return false;
}
