#pragma GCC optimize ("O3,unroll-loops")
#include "emu_wswan.h"
#include "../hal/config.h"
#include "../ui/display_emu.h"
#include "../ui/theme.h"
#include <Arduino.h>

#if BMO_EXPERIMENTAL_CORES
#include "../engines/wswan/wswan.h"
#include "../hal/buttons.h"
#include "../storage/save_manager.h"
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
  (void)romFilename;
  return false;
}

bool WSwanEmu::loadBatteryRam(const char* romFilename) {
  (void)romFilename;
  return false;
}

bool WSwanEmu::saveState(const char* romFilename, int slot) {
  if (!running || !wswanRam || !wswanState.sram || !romFilename) return false;
  const size_t totalRam = (64 + 64) * 1024;
  uint8_t* combined = (uint8_t*)heap_caps_malloc(totalRam, MALLOC_CAP_SPIRAM);
  if (!combined) return false;
  memcpy(combined, wswanRam, 64 * 1024);
  memcpy(combined + 64 * 1024, wswanState.sram, 64 * 1024);
  bool ok = SaveManager::saveState(romFilename, slot, SaveManager::CORE_WSWAN,
                                   &wswanState, sizeof(wswan_t), combined, totalRam);
  heap_caps_free(combined);
  return ok;
}

bool WSwanEmu::loadState(const char* romFilename, int slot) {
  if (!running || !wswanRam || !wswanState.sram || !romFilename) return false;
  const size_t totalRam = (64 + 64) * 1024;
  uint8_t* combined = (uint8_t*)heap_caps_malloc(totalRam, MALLOC_CAP_SPIRAM);
  if (!combined) return false;
  wswan_t tempState;
  size_t stateSize = 0, ramSize = 0;
  bool ok = SaveManager::loadState(romFilename, slot, SaveManager::CORE_WSWAN,
                                   &tempState, sizeof(tempState), &stateSize,
                                   combined, totalRam, &ramSize);
  if (ok && stateSize == sizeof(wswan_t) && ramSize == totalRam) {
    memcpy(wswanRam, combined, 64 * 1024);
    memcpy(wswanState.sram, combined + 64 * 1024, 64 * 1024);
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
    heap_caps_free(combined);
    return true;
  }
  heap_caps_free(combined);
  return false;
}

#else // BMO_EXPERIMENTAL_CORES == 0
bool WSwanEmu::init(uint8_t* romData, size_t romSize, bool isColor) {
  (void)isColor;
  (void)romData;
  (void)romSize;
  LOG_WARN_STR("[wswan] Experimental core disabled (-DBMO_EXPERIMENTAL_CORES=1 to enable)");
  DisplayEmu::showToast("EXPERIMENTAL CORE OFF", Theme::ACCENT_RED, 2200);
  return false;
}

void WSwanEmu::update() {}
void WSwanEmu::destroy() {}
bool WSwanEmu::isRunning() { return false; }
bool WSwanEmu::saveBatteryRam(const char* romFilename) { (void)romFilename; return false; }
bool WSwanEmu::loadBatteryRam(const char* romFilename) { (void)romFilename; return false; }
bool WSwanEmu::saveState(const char* romFilename, int slot) { (void)romFilename; (void)slot; return false; }
bool WSwanEmu::loadState(const char* romFilename, int slot) { (void)romFilename; (void)slot; return false; }

#endif // BMO_EXPERIMENTAL_CORES
