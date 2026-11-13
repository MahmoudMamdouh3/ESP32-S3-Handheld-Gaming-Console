#pragma GCC optimize ("O3,unroll-loops")
#include "emu_lynx.h"
#include "../vendor/lynx/lynx.h"
#include "../core/display_emu.h"
#include "../core/buttons.h"
#include "../core/save_manager.h"
#include "../core/config.h"
#include <esp_heap_caps.h>
#include <string.h>

namespace {
  lynx_t lynxState;
  uint16_t* lynxFb = nullptr;   // 160 x 102 in PSRAM
  uint8_t* lynxRam = nullptr;   // 64KB RAM in PSRAM
  bool running = false;
}

bool LynxEmu::init(uint8_t* romData, size_t romSize) {
  destroy();
  if (!romData || romSize < 64) return false;

  lynxFb = (uint16_t*)heap_caps_malloc(160 * 102 * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
  lynxRam = (uint8_t*)heap_caps_malloc(64 * 1024, MALLOC_CAP_SPIRAM);

  if (!lynxFb || !lynxRam) {
    destroy();
    return false;
  }

  lynx_init(&lynxState, romData, romSize, lynxRam, lynxFb);
  running = true;
  return true;
}

void LynxEmu::update() {
  if (!running || !lynxFb) return;
  uint8_t pad = 0;
  if (Buttons::get(Buttons::UP).pressed)     pad |= 0x01;
  if (Buttons::get(Buttons::DOWN).pressed)   pad |= 0x02;
  if (Buttons::get(Buttons::LEFT).pressed)   pad |= 0x04;
  if (Buttons::get(Buttons::RIGHT).pressed)  pad |= 0x08;
  if (Buttons::get(Buttons::A).pressed)      pad |= 0x10; // A
  if (Buttons::get(Buttons::B).pressed)      pad |= 0x20; // B
  if (Buttons::get(Buttons::START).pressed)  pad |= 0x40; // Option 1
  lynx_set_buttons(&lynxState, pad);

  lynx_step_frame(&lynxState);
  DisplayEmu::streamLynxFrame(lynxFb, 160, 102);
}

void LynxEmu::destroy() {
  running = false;
  if (lynxFb) {
    heap_caps_free(lynxFb);
    lynxFb = nullptr;
  }
  if (lynxRam) {
    heap_caps_free(lynxRam);
    lynxRam = nullptr;
  }
  memset(&lynxState, 0, sizeof(lynxState));
}

bool LynxEmu::isRunning() {
  return running;
}

bool LynxEmu::saveBatteryRam(const char* romFilename) {
  (void)romFilename;
  return false;
}

bool LynxEmu::loadBatteryRam(const char* romFilename) {
  (void)romFilename;
  return false;
}

bool LynxEmu::saveState(const char* romFilename, int slot) {
  if (!running || !lynxRam || !romFilename) return false;
  return SaveManager::saveState(romFilename, slot, SaveManager::CORE_LYNX,
                                &lynxState, sizeof(lynx_t), lynxRam, 64 * 1024);
}

bool LynxEmu::loadState(const char* romFilename, int slot) {
  if (!running || !lynxRam || !romFilename) return false;
  const size_t expectedRamSize = 64 * 1024;
  uint8_t* tempRam = (uint8_t*)heap_caps_malloc(expectedRamSize, MALLOC_CAP_SPIRAM);
  if (!tempRam) return false;
  lynx_t tempState;
  size_t stateSize = 0, ramSize = 0;
  bool ok = SaveManager::loadState(romFilename, slot, SaveManager::CORE_LYNX,
                                   &tempState, sizeof(tempState), &stateSize,
                                   tempRam, expectedRamSize, &ramSize);
  if (ok && stateSize == sizeof(lynx_t) && ramSize == expectedRamSize) {
    memcpy(lynxRam, tempRam, expectedRamSize);
    uint8_t* rom = lynxState.rom;
    uint32_t rom_size = lynxState.rom_size;
    uint8_t* ram = lynxState.ram;
    uint16_t* fb = lynxState.framebuffer;
    memcpy(&lynxState, &tempState, sizeof(lynx_t));
    lynxState.rom = rom;
    lynxState.rom_size = rom_size;
    lynxState.ram = ram;
    lynxState.framebuffer = fb;
    heap_caps_free(tempRam);
    return true;
  }
  heap_caps_free(tempRam);
  return false;
}
