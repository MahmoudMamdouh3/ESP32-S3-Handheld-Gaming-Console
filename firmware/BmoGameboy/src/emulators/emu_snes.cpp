#pragma GCC optimize ("O3,unroll-loops")
#include "emu_snes.h"
#include "../vendor/snes/snes.h"
#include "../core/display_emu.h"
#include "../core/buttons.h"
#include "../core/save_manager.h"
#include "../core/config.h"
#include <esp_heap_caps.h>
#include <string.h>

namespace {
  snes_t snesState;
  uint16_t* snesFb = nullptr;  // 256 x 224 in PSRAM
  uint8_t* snesWram = nullptr; // 128KB WRAM in PSRAM
  bool running = false;
}

bool SNESEmu::init(uint8_t* romData, size_t romSize) {
  destroy();
  if (!romData || romSize < 0x8000) return false;

  snesFb = (uint16_t*)heap_caps_malloc(256 * 224 * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
  snesWram = (uint8_t*)heap_caps_malloc(128 * 1024, MALLOC_CAP_SPIRAM);

  if (!snesFb || !snesWram) {
    destroy();
    return false;
  }

  snes_init(&snesState, romData, romSize, snesWram, snesFb);
  running = true;
  return true;
}

void SNESEmu::update() {
  if (!running || !snesFb) return;
  uint8_t pad = 0;
  if (Buttons::get(Buttons::UP).pressed)     pad |= 0x01;
  if (Buttons::get(Buttons::DOWN).pressed)   pad |= 0x02;
  if (Buttons::get(Buttons::LEFT).pressed)   pad |= 0x04;
  if (Buttons::get(Buttons::RIGHT).pressed)  pad |= 0x08;
  if (Buttons::get(Buttons::B).pressed)      pad |= 0x10; // B
  if (Buttons::get(Buttons::A).pressed)      pad |= 0x20; // A
  if (Buttons::get(Buttons::START).pressed)  pad |= 0x40; // Start
  if (Buttons::get(Buttons::SELECT).pressed) pad |= 0x80; // Select
  snes_set_pad(&snesState, pad);

  snes_step_frame(&snesState);
  DisplayEmu::streamSNESFrame(snesFb, 256, 224);
}

void SNESEmu::destroy() {
  running = false;
  snes_destroy(&snesState);
  if (snesFb) {
    heap_caps_free(snesFb);
    snesFb = nullptr;
  }
  if (snesWram) {
    heap_caps_free(snesWram);
    snesWram = nullptr;
  }
  memset(&snesState, 0, sizeof(snesState));
}

bool SNESEmu::isRunning() {
  return running;
}

bool SNESEmu::saveBatteryRam(const char* romFilename) {
  if (!running || !snesState.sram || !romFilename) return false;
  return SaveManager::saveBatteryRam(romFilename, snesState.sram, 32 * 1024);
}

bool SNESEmu::loadBatteryRam(const char* romFilename) {
  if (!running || !snesState.sram || !romFilename) return false;
  return SaveManager::loadBatteryRam(romFilename, snesState.sram, 32 * 1024);
}

bool SNESEmu::saveState(const char* romFilename, int slot) {
  if (!running || !snesWram || !romFilename) return false;
  return SaveManager::saveState(romFilename, slot, SaveManager::CORE_SNES,
                                &snesState, sizeof(snes_t), snesWram, 128 * 1024);
}

bool SNESEmu::loadState(const char* romFilename, int slot) {
  if (!running || !snesWram || !romFilename) return false;
  snes_t tempState;
  size_t stateSize = 0, ramSize = 0;
  bool ok = SaveManager::loadState(romFilename, slot, SaveManager::CORE_SNES,
                                   &tempState, sizeof(tempState), &stateSize,
                                   snesWram, 128 * 1024, &ramSize);
  if (ok && stateSize == sizeof(snes_t)) {
    uint8_t* rom = snesState.rom;
    uint32_t rom_size = snesState.rom_size;
    uint8_t* wram = snesState.wram;
    uint8_t* vram = snesState.vram;
    uint8_t* sram = snesState.sram;
    uint16_t* fb = snesState.framebuffer;
    memcpy(&snesState, &tempState, sizeof(snes_t));
    snesState.rom = rom;
    snesState.rom_size = rom_size;
    snesState.wram = wram;
    snesState.vram = vram;
    snesState.sram = sram;
    snesState.framebuffer = fb;
    return true;
  }
  return false;
}
