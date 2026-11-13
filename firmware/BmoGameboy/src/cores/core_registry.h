#pragma once

#include "../hal/config.h"
#include "../storage/sd_card.h"
#include "../storage/battery_save.h"
#include "emu_walnut.h"
#include "emu_peanut.h"
#include "emu_nes.h"
#include "emu_doom.h"
#include "emu_sms.h"
#include "emu_pce.h"
#include "emu_atari.h"
#include "emu_pico.h"
#include "emu_genesis.h"
#include "emu_snes.h"
#include "emu_wswan.h"
#include "emu_ngp.h"
#include "emu_lynx.h"
#include "emu_colem.h"
#include "doom_wad_path.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>

namespace CoreRegistry {

struct CoreDescriptor {
  int emulatorIndex;
  RomType romType;
  bool supportsDmgPalette;
  bool (*start)(const RomFile* rom, uint8_t* romData, size_t romSize);
  void (*stepFrame)();
  void (*destroy)();
  bool (*hasBatterySave)(const uint8_t* romData, size_t romSize);
  bool (*hasActiveBatteryRam)();
  bool (*loadBatteryRam)(const char* filename);
  bool (*saveBatteryRam)(const char* filename);
  bool (*saveState)(const char* filename, int slot);
  bool (*loadState)(const char* filename, int slot);
};

namespace detail {

inline bool hasExtension(const char* filename, const char* ext) {
  if (!filename || !ext) return false;
  const size_t fnLen = strlen(filename);
  const size_t extLen = strlen(ext);
  return fnLen >= extLen && strcasecmp(filename + fnLen - extLen, ext) == 0;
}

inline bool startWalnut(const RomFile* rom, uint8_t* romData, size_t romSize) {
  (void)rom;
  return WalnutEmu::init(romData, romSize);
}
inline bool hasGbBattery(const uint8_t* romData, size_t romSize) {
  if (!romData || romSize <= 0x0149U) return false;
  return BatterySave::gameBoyBatteryBytes(romData[0x0147], romData[0x0149]) > 0;
}

inline bool startPeanut(const RomFile* rom, uint8_t* romData, size_t romSize) {
  (void)rom;
  return PeanutEmu::init(romData, romSize);
}

inline bool startNes(const RomFile* rom, uint8_t* romData, size_t romSize) {
  (void)rom;
  return NesEmu::init(romData, romSize);
}
inline bool hasNesBattery(const uint8_t* romData, size_t romSize) {
  return romData && romSize >= 16U && BatterySave::nesHeaderHasBattery(romData[6]);
}

inline bool startDoom(const RomFile* rom, uint8_t* romData, size_t romSize) {
  (void)romData;
  (void)romSize;
  if (!rom) return false;
  static char wadPath[DoomWadPath::MAX_WAD_PATH_BYTES];
  if (!DoomWadPath::build(rom->filename, wadPath, sizeof(wadPath))) return false;
  return DoomEmu::init(wadPath);
}

inline bool noBattery(const uint8_t*, size_t) {
  return false;
}
inline bool alwaysBattery(const uint8_t*, size_t) {
  return true;
}
inline bool noFileOp(const char*) {
  return false;
}
inline bool noStateOp(const char*, int) {
  return false;
}

inline bool startSms(const RomFile* rom, uint8_t* romData, size_t romSize) {
  (void)rom;
  return SmsEmu::init(romData, romSize, false);
}
inline bool startGg(const RomFile* rom, uint8_t* romData, size_t romSize) {
  (void)rom;
  return SmsEmu::init(romData, romSize, true);
}
inline bool startPce(const RomFile* rom, uint8_t* romData, size_t romSize) {
  (void)rom;
  return PceEmu::init(romData, romSize);
}
inline bool startAtari(const RomFile* rom, uint8_t* romData, size_t romSize) {
  (void)rom;
  return AtariEmu::init(romData, romSize);
}
inline bool startPico(const RomFile* rom, uint8_t* romData, size_t romSize) {
  (void)rom;
  return PicoEmu::init(romData, romSize);
}
inline bool startGenesis(const RomFile* rom, uint8_t* romData, size_t romSize) {
  (void)rom;
  return GenesisEmu::init(romData, romSize);
}
inline bool hasGenesisBattery(const uint8_t* romData, size_t romSize) {
  return BatterySave::genesisHeaderHasBattery(romData, romSize);
}
inline bool startSnes(const RomFile* rom, uint8_t* romData, size_t romSize) {
  (void)rom;
  return SNESEmu::init(romData, romSize);
}
inline bool hasSnesBattery(const uint8_t* romData, size_t romSize) {
  return BatterySave::snesBatteryBytes(romData, romSize) > 0;
}
inline bool startWSwan(const RomFile* rom, uint8_t* romData, size_t romSize) {
  const bool isColor = !rom || !hasExtension(rom->filename, ".ws");
  return WSwanEmu::init(romData, romSize, isColor);
}
inline bool hasWSwanBattery(const uint8_t* romData, size_t romSize) {
  return BatterySave::wswanHeaderHasBattery(romData, romSize);
}
inline bool startNgp(const RomFile* rom, uint8_t* romData, size_t romSize) {
  const bool isColor = !rom || !hasExtension(rom->filename, ".ngp");
  return NGPEmu::init(romData, romSize, isColor);
}
inline bool hasLynxBattery(const uint8_t* romData, size_t romSize) {
  return romData && romSize >= 64U && memcmp(romData, "LYNX", 4) == 0 && romData[0x3C] != 0;
}
inline bool startLynx(const RomFile* rom, uint8_t* romData, size_t romSize) {
  (void)rom;
  return LynxEmu::init(romData, romSize);
}
inline bool startColem(const RomFile* rom, uint8_t* romData, size_t romSize) {
  const bool isSg1000 = rom && hasExtension(rom->filename, ".sg");
  return ColemEmu::init(romData, romSize, isSg1000);
}

inline constexpr CoreDescriptor kCores[] = {
    {0, ROM_GBC, true, startWalnut, WalnutEmu::update, WalnutEmu::destroy, hasGbBattery, nullptr,
     WalnutEmu::loadBatteryRam, WalnutEmu::saveBatteryRam, WalnutEmu::saveState, WalnutEmu::loadState},
    {1, ROM_GB, true, startPeanut, PeanutEmu::update, PeanutEmu::destroy, hasGbBattery, nullptr,
     PeanutEmu::loadBatteryRam, PeanutEmu::saveBatteryRam, PeanutEmu::saveState, PeanutEmu::loadState},
    {2, ROM_NES, false, startNes, NesEmu::update, NesEmu::destroy, hasNesBattery, nullptr, NesEmu::loadBatteryRam,
     NesEmu::saveBatteryRam, NesEmu::saveState, NesEmu::loadState},
    {3, ROM_WAD, false, startDoom, DoomEmu::update, DoomEmu::destroy, noBattery, nullptr, noFileOp, noFileOp, noStateOp,
     noStateOp},
    {4, ROM_SMS, false, startSms, SmsEmu::update, SmsEmu::destroy, alwaysBattery, SmsEmu::hasActiveBattery,
     SmsEmu::loadBatteryRam, SmsEmu::saveBatteryRam, SmsEmu::saveState, SmsEmu::loadState},
    {5, ROM_GG, false, startGg, SmsEmu::update, SmsEmu::destroy, alwaysBattery, SmsEmu::hasActiveBattery,
     SmsEmu::loadBatteryRam, SmsEmu::saveBatteryRam, SmsEmu::saveState, SmsEmu::loadState},
    {6, ROM_PCE, false, startPce, PceEmu::update, PceEmu::destroy, alwaysBattery, PceEmu::hasActiveBattery,
     PceEmu::loadBatteryRam, PceEmu::saveBatteryRam, PceEmu::saveState, PceEmu::loadState},
    {7, ROM_ATARI, false, startAtari, AtariEmu::update, AtariEmu::destroy, noBattery, nullptr, AtariEmu::loadBatteryRam,
     AtariEmu::saveBatteryRam, AtariEmu::saveState, AtariEmu::loadState},
    {8, ROM_PICO8, false, startPico, PicoEmu::update, PicoEmu::destroy, noBattery, nullptr, PicoEmu::loadBatteryRam,
     PicoEmu::saveBatteryRam, PicoEmu::saveState, PicoEmu::loadState},
    {9, ROM_GENESIS, false, startGenesis, GenesisEmu::update, GenesisEmu::destroy, hasGenesisBattery,
     GenesisEmu::hasActiveBattery, GenesisEmu::loadBatteryRam, GenesisEmu::saveBatteryRam, GenesisEmu::saveState,
     GenesisEmu::loadState},
    {10, ROM_SNES, false, startSnes, SNESEmu::update, SNESEmu::destroy, hasSnesBattery, SNESEmu::hasActiveBattery,
     SNESEmu::loadBatteryRam, SNESEmu::saveBatteryRam, SNESEmu::saveState, SNESEmu::loadState},
    {11, ROM_WSWAN, false, startWSwan, WSwanEmu::update, WSwanEmu::destroy, hasWSwanBattery, WSwanEmu::hasActiveBattery,
     WSwanEmu::loadBatteryRam, WSwanEmu::saveBatteryRam, WSwanEmu::saveState, WSwanEmu::loadState},
    {12, ROM_NGP, false, startNgp, NGPEmu::update, NGPEmu::destroy, alwaysBattery, NGPEmu::hasActiveBattery,
     NGPEmu::loadBatteryRam, NGPEmu::saveBatteryRam, NGPEmu::saveState, NGPEmu::loadState},
    {13, ROM_LYNX, false, startLynx, LynxEmu::update, LynxEmu::destroy, hasLynxBattery, nullptr,
     LynxEmu::loadBatteryRam, LynxEmu::saveBatteryRam, LynxEmu::saveState, LynxEmu::loadState},
    {14, ROM_COLEM, false, startColem, ColemEmu::update, ColemEmu::destroy, noBattery, nullptr,
     ColemEmu::loadBatteryRam, ColemEmu::saveBatteryRam, ColemEmu::saveState, ColemEmu::loadState},
};

constexpr bool validateDescriptors() {
  if (sizeof(kCores) / sizeof(kCores[0]) != 15) return false;
  for (int i = 0; i < 15; ++i) {
    const auto& d = kCores[i];
    if (d.emulatorIndex != i) return false;
    if (!d.start || !d.stepFrame || !d.destroy || !d.hasBatterySave || !d.loadBatteryRam || !d.saveBatteryRam ||
        !d.saveState || !d.loadState) {
      return false;
    }
  }
  return true;
}
static_assert(validateDescriptors(), "kCores registry invariant broken");

} // namespace detail

inline const CoreDescriptor* findByRomType(RomType type) {
  for (const auto& desc : detail::kCores) {
    if (desc.romType == type) return &desc;
  }
  return nullptr;
}

inline const CoreDescriptor* findByIndex(int index) {
  for (const auto& desc : detail::kCores) {
    if (desc.emulatorIndex == index) return &desc;
  }
  return nullptr;
}

} // namespace CoreRegistry
