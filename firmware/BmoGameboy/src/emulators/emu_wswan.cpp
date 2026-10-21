#pragma GCC optimize ("O3,unroll-loops")
#include "emu_wswan.h"
#include "../vendor/wswan/wswan.h"
#include "../core/display_emu.h"
#include "../core/buttons.h"
#include "../core/save_manager.h"
#include "../core/config.h"
#include <esp_heap_caps.h>
#include <string.h>

namespace {
  wswan_t wswanState;
  uint16_t* wswanFb = nullptr;  // 224 x 144 in PSRAM
  uint8_t* wswanRam = nullptr;  // 64KB IRAM in PSRAM
  bool running = false;
}

bool WSwanEmu::init(uint8_t* romData, size_t romSize, bool isColor) {
  destroy();
  if (!romData || romSize < 512) return false;

  wswanFb = (uint16_t*)heap_caps_malloc(224 * 144 * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
  wswanRam = (uint8_t*)heap_caps_malloc(64 * 1024, MALLOC_CAP_SPIRAM);

  if (!wswanFb || !wswanRam) {
    destroy();
    return false;
  }

  wswan_init(&wswanState, romData, romSize, wswanRam, wswanFb, isColor);
  running = true;
  return true;
}

void WSwanEmu::update() {
  if (!running || !wswanFb) return;
  uint16_t keys = 0;
  if (Buttons::get(Buttons::UP).pressed)     keys |= 0x0001; // X1
  if (Buttons::get(Buttons::RIGHT).pressed)  keys |= 0x0002; // X2
  if (Buttons::get(Buttons::DOWN).pressed)   keys |= 0x0004; // X3
  if (Buttons::get(Buttons::LEFT).pressed)   keys |= 0x0008; // X4
  if (Buttons::get(Buttons::A).pressed)      keys |= 0x0010; // Button A
  if (Buttons::get(Buttons::B).pressed)      keys |= 0x0020; // Button B
  if (Buttons::get(Buttons::START).pressed)  keys |= 0x0080; // Start (bit 7 per wswan_in port 0xB5)
  wswan_set_keys(&wswanState, keys);

  wswan_step_frame(&wswanState);
  DisplayEmu::streamWSwanFrame(wswanFb, 224, 144);
}

void WSwanEmu::destroy() {
  running = false;
  wswan_destroy(&wswanState);
  if (wswanFb) {
    heap_caps_free(wswanFb);
    wswanFb = nullptr;
  }
  if (wswanRam) {
    heap_caps_free(wswanRam);
    wswanRam = nullptr;
  }
  memset(&wswanState, 0, sizeof(wswanState));
}

bool WSwanEmu::isRunning() {
  return running;
}

bool WSwanEmu::saveBatteryRam(const char* romFilename) {
  if (!running || !wswanState.sram || !romFilename) return false;
  return SaveManager::saveBatteryRam(romFilename, wswanState.sram, 64 * 1024);
}

bool WSwanEmu::loadBatteryRam(const char* romFilename) {
  if (!running || !wswanState.sram || !romFilename) return false;
  return SaveManager::loadBatteryRam(romFilename, wswanState.sram, 64 * 1024);
}

bool WSwanEmu::saveState(const char* romFilename, int slot) {
  if (!running || !wswanRam || !romFilename) return false;
  return SaveManager::saveState(romFilename, slot, SaveManager::CORE_WSWAN,
                                &wswanState, sizeof(wswan_t), wswanRam, 64 * 1024);
}

bool WSwanEmu::loadState(const char* romFilename, int slot) {
  if (!running || !wswanRam || !romFilename) return false;
  wswan_t tempState;
  size_t stateSize = 0, ramSize = 0;
  bool ok = SaveManager::loadState(romFilename, slot, SaveManager::CORE_WSWAN,
                                   &tempState, sizeof(tempState), &stateSize,
                                   wswanRam, 64 * 1024, &ramSize);
  if (ok && stateSize == sizeof(wswan_t)) {
    uint8_t* rom = wswanState.rom;
    uint32_t rom_size = wswanState.rom_size;
    uint8_t* iram = wswanState.internal_ram;
    uint8_t* sram = wswanState.sram;
    uint16_t* fb = wswanState.framebuffer;
    memcpy(&wswanState, &tempState, sizeof(wswan_t));
    wswanState.rom = rom;
    wswanState.rom_size = rom_size;
    wswanState.internal_ram = iram;
    wswanState.sram = sram;
    wswanState.framebuffer = fb;
    return true;
  }
  return false;
}
