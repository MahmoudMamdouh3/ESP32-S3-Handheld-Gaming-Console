#pragma GCC optimize ("O3,unroll-loops")
#include "emu_genesis.h"
#include "../hal/config.h"
#include "../ui/display_emu.h"
#include "../ui/theme.h"
#include <Arduino.h>

#if BMO_EXPERIMENTAL_CORES
#include "../engines/genesis/genesis.h"
#include "../hal/buttons.h"
#include "../storage/save_manager.h"
#include <esp_heap_caps.h>
#include <string.h>

namespace {
  genesis_t genesisState;
  uint16_t* genesisFb = nullptr; // 320 x 224 in PSRAM
  uint8_t* genesisRam = nullptr; // 64KB RAM in PSRAM
  bool running = false;
}

bool GenesisEmu::init(uint8_t* romData, size_t romSize) {
  destroy();
  if (!romData || romSize < 512) return false;

  genesisFb = (uint16_t*)heap_caps_malloc(320 * 224 * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
  genesisRam = (uint8_t*)heap_caps_malloc(64 * 1024, MALLOC_CAP_SPIRAM);

  if (!genesisFb || !genesisRam) {
    destroy();
    return false;
  }

  genesis_init(&genesisState, romData, romSize, genesisRam, genesisFb);
  running = true;
  return true;
}

void GenesisEmu::update() {
  if (!running || !genesisFb) return;
  uint8_t pad = 0;
  if (Buttons::get(Buttons::UP).pressed)     pad |= 0x01;
  if (Buttons::get(Buttons::DOWN).pressed)   pad |= 0x02;
  if (Buttons::get(Buttons::LEFT).pressed)   pad |= 0x04;
  if (Buttons::get(Buttons::RIGHT).pressed)  pad |= 0x08;
  if (Buttons::get(Buttons::B).pressed)      pad |= 0x10; // A
  if (Buttons::get(Buttons::A).pressed)      pad |= 0x20; // B
  if (Buttons::get(Buttons::X).pressed)      pad |= 0x40; // C (3-button pad; the core has no 6-button protocol)
  if (Buttons::get(Buttons::START).pressed)  pad |= 0x80; // Start
  genesis_set_pad(&genesisState, pad);

  genesis_step_frame(&genesisState);
  DisplayEmu::streamGenesisFrame(genesisFb, 320, 224);
}

void GenesisEmu::destroy() {
  running = false;
  genesis_destroy(&genesisState);
  if (genesisFb) {
    heap_caps_free(genesisFb);
    genesisFb = nullptr;
  }
  if (genesisRam) {
    heap_caps_free(genesisRam);
    genesisRam = nullptr;
  }
  memset(&genesisState, 0, sizeof(genesisState));
}

bool GenesisEmu::isRunning() {
  return running;
}

bool GenesisEmu::saveBatteryRam(const char* romFilename) {
  (void)romFilename;
  return false;
}

bool GenesisEmu::loadBatteryRam(const char* romFilename) {
  (void)romFilename;
  return false;
}

bool GenesisEmu::saveState(const char* romFilename, int slot) {
  if (!running || !genesisRam || !genesisState.vram || !genesisState.z80_ram ||
      !genesisState.cram || !genesisState.vsram || !romFilename) return false;
  const size_t totalRam = (64 + 64 + 8) * 1024 + 128 + 80;
  uint8_t* combined = (uint8_t*)heap_caps_malloc(totalRam, MALLOC_CAP_SPIRAM);
  if (!combined) return false;
  size_t off = 0;
  memcpy(combined + off, genesisRam, 64 * 1024); off += 64 * 1024;
  memcpy(combined + off, genesisState.vram, 64 * 1024); off += 64 * 1024;
  memcpy(combined + off, genesisState.z80_ram, 8 * 1024); off += 8 * 1024;
  memcpy(combined + off, genesisState.cram, 128); off += 128;
  memcpy(combined + off, genesisState.vsram, 80); off += 80;
  bool ok = SaveManager::saveState(romFilename, slot, SaveManager::CORE_GENESIS,
                                   &genesisState, sizeof(genesis_t), combined, totalRam);
  heap_caps_free(combined);
  return ok;
}

bool GenesisEmu::loadState(const char* romFilename, int slot) {
  if (!running || !genesisRam || !genesisState.vram || !genesisState.z80_ram ||
      !genesisState.cram || !genesisState.vsram || !romFilename) return false;
  const size_t totalRam = (64 + 64 + 8) * 1024 + 128 + 80;
  uint8_t* combined = (uint8_t*)heap_caps_malloc(totalRam, MALLOC_CAP_SPIRAM);
  if (!combined) return false;
  genesis_t tempState;
  size_t stateSize = 0, ramSize = 0;
  bool ok = SaveManager::loadState(romFilename, slot, SaveManager::CORE_GENESIS,
                                   &tempState, sizeof(tempState), &stateSize,
                                   combined, totalRam, &ramSize);
  if (ok && stateSize == sizeof(genesis_t) && ramSize == totalRam) {
    size_t off = 0;
    memcpy(genesisRam, combined + off, 64 * 1024); off += 64 * 1024;
    memcpy(genesisState.vram, combined + off, 64 * 1024); off += 64 * 1024;
    memcpy(genesisState.z80_ram, combined + off, 8 * 1024); off += 8 * 1024;
    memcpy(genesisState.cram, combined + off, 128); off += 128;
    memcpy(genesisState.vsram, combined + off, 80); off += 80;
    uint8_t* rom = genesisState.rom;
    uint32_t rom_size = genesisState.rom_size;
    uint8_t* ram = genesisState.ram;
    uint8_t* z80_ram = genesisState.z80_ram;
    uint8_t* vram = genesisState.vram;
    uint16_t* cram = genesisState.cram;
    uint16_t* vsram = genesisState.vsram;
    uint16_t* fb = genesisState.framebuffer;
    memcpy(&genesisState, &tempState, sizeof(genesis_t));
    genesisState.rom = rom;
    genesisState.rom_size = rom_size;
    genesisState.ram = ram;
    genesisState.z80_ram = z80_ram;
    genesisState.vram = vram;
    genesisState.cram = cram;
    genesisState.vsram = vsram;
    genesisState.framebuffer = fb;
    heap_caps_free(combined);
    return true;
  }
  heap_caps_free(combined);
  return false;
}

#else // BMO_EXPERIMENTAL_CORES == 0
bool GenesisEmu::init(uint8_t* romData, size_t romSize) {
  (void)romData;
  (void)romSize;
  LOG_WARN_STR("[genesis] Experimental core disabled (-DBMO_EXPERIMENTAL_CORES=1 to enable)");
  DisplayEmu::showToast("EXPERIMENTAL CORE OFF", Theme::ACCENT_RED, 2200);
  return false;
}

void GenesisEmu::update() {}
void GenesisEmu::destroy() {}
bool GenesisEmu::isRunning() { return false; }
bool GenesisEmu::saveBatteryRam(const char* romFilename) { (void)romFilename; return false; }
bool GenesisEmu::loadBatteryRam(const char* romFilename) { (void)romFilename; return false; }
bool GenesisEmu::saveState(const char* romFilename, int slot) { (void)romFilename; (void)slot; return false; }
bool GenesisEmu::loadState(const char* romFilename, int slot) { (void)romFilename; (void)slot; return false; }

#endif // BMO_EXPERIMENTAL_CORES
