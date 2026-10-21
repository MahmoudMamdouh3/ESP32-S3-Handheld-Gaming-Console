#pragma GCC optimize ("O3,unroll-loops")
#include "emu_colem.h"
#include "../vendor/colem/colem.h"
#include "../core/display_emu.h"
#include "../core/buttons.h"
#include "../core/save_manager.h"
#include "../core/config.h"
#include <esp_heap_caps.h>
#include <string.h>

namespace {
  colem_t colemState;
  uint16_t* colemFb = nullptr;   // 256 x 192 in PSRAM
  uint8_t* colemRam = nullptr;   // 8KB RAM in PSRAM
  uint8_t* colemVram = nullptr;  // 16KB VRAM in PSRAM
  bool running = false;
}

bool ColemEmu::init(uint8_t* romData, size_t romSize) {
  destroy();
  if (!romData || romSize < 512) return false;

  colemFb = (uint16_t*)heap_caps_malloc(256 * 192 * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
  colemRam = (uint8_t*)heap_caps_malloc(8 * 1024, MALLOC_CAP_SPIRAM);
  colemVram = (uint8_t*)heap_caps_malloc(16 * 1024, MALLOC_CAP_SPIRAM);

  if (!colemFb || !colemRam || !colemVram) {
    destroy();
    return false;
  }

  colem_init(&colemState, romData, romSize, colemRam, colemVram, colemFb);
  running = true;
  return true;
}

void ColemEmu::update() {
  if (!running || !colemFb) return;
  uint16_t joy = 0;
  if (Buttons::get(Buttons::UP).pressed)     joy |= 0x0001;
  if (Buttons::get(Buttons::RIGHT).pressed)  joy |= 0x0002;
  if (Buttons::get(Buttons::DOWN).pressed)   joy |= 0x0004;
  if (Buttons::get(Buttons::LEFT).pressed)   joy |= 0x0008;
  if (Buttons::get(Buttons::A).pressed)      joy |= 0x0010; // Left Fire
  if (Buttons::get(Buttons::B).pressed)      joy |= 0x0020; // Right Fire
  if (Buttons::get(Buttons::START).pressed)  joy |= 0x0040; // Keypad 1
  colem_set_joystick(&colemState, joy);

  colem_step_frame(&colemState);
  DisplayEmu::streamColemFrame(colemFb, 256, 192);
}

void ColemEmu::destroy() {
  running = false;
  if (colemFb) {
    heap_caps_free(colemFb);
    colemFb = nullptr;
  }
  if (colemRam) {
    heap_caps_free(colemRam);
    colemRam = nullptr;
  }
  if (colemVram) {
    heap_caps_free(colemVram);
    colemVram = nullptr;
  }
  memset(&colemState, 0, sizeof(colemState));
}

bool ColemEmu::isRunning() {
  return running;
}

bool ColemEmu::saveBatteryRam(const char* romFilename) {
  if (!running || !colemRam || !romFilename) return false;
  return SaveManager::saveBatteryRam(romFilename, colemRam, 8 * 1024);
}

bool ColemEmu::loadBatteryRam(const char* romFilename) {
  if (!running || !colemRam || !romFilename) return false;
  return SaveManager::loadBatteryRam(romFilename, colemRam, 8 * 1024);
}

bool ColemEmu::saveState(const char* romFilename, int slot) {
  if (!running || !colemRam || !romFilename) return false;
  return SaveManager::saveState(romFilename, slot, SaveManager::CORE_COLEM,
                                &colemState, sizeof(colem_t), colemRam, 8 * 1024);
}

bool ColemEmu::loadState(const char* romFilename, int slot) {
  if (!running || !colemRam || !romFilename) return false;
  colem_t tempState;
  size_t stateSize = 0, ramSize = 0;
  bool ok = SaveManager::loadState(romFilename, slot, SaveManager::CORE_COLEM,
                                   &tempState, sizeof(tempState), &stateSize,
                                   colemRam, 8 * 1024, &ramSize);
  if (ok && stateSize == sizeof(colem_t)) {
    uint8_t* rom = colemState.rom;
    uint32_t rom_size = colemState.rom_size;
    uint8_t* ram = colemState.ram;
    uint8_t* vram = colemState.vram;
    uint16_t* fb = colemState.framebuffer;
    memcpy(&colemState, &tempState, sizeof(colem_t));
    colemState.rom = rom;
    colemState.rom_size = rom_size;
    colemState.ram = ram;
    colemState.vram = vram;
    colemState.framebuffer = fb;
    return true;
  }
  return false;
}
