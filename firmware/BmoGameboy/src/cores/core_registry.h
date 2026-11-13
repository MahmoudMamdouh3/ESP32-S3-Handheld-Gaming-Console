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
  return WalnutEmu::begin(romData, romSize);
}
inline void stepWalnut() {
  WalnutEmu::updateJoypad();
  WalnutEmu::runFrame();
}
inline bool hasGbBattery(const uint8_t* romData, size_t romSize) {
  if (!romData || romSize <= 0x0149U) return false;
  return BatterySave::gameBoyBatteryBytes(romData[0x0147], romData[0x0149]) > 0;
}

inline bool startPeanut(const RomFile* rom, uint8_t* romData, size_t romSize) {
  (void)rom;
  return PeanutEmu::begin(romData, romSize);
}
inline void stepPeanut() {
  PeanutEmu::updateJoypad();
  PeanutEmu::runFrame();
}

inline bool startNes(const RomFile* rom, uint8_t* romData, size_t romSize) {
  (void)rom;
  return NesEmu::begin(romData, romSize);
}
inline void stepNes() {
  NesEmu::updateJoypad();
  NesEmu::runFrame();
}
inline bool hasNesBattery(const uint8_t* romData, size_t romSize) {
  return romData && romSize >= 16U && BatterySave::nesHeaderHasBattery(romData[6]);
}

inline bool startDoom(const RomFile* rom, uint8_t* romData, size_t romSize) {
  (void)romData;
  (void)romSize;
  if (!rom) return false;
  static char wadPath[72];
  const int wadLen = snprintf(wadPath, sizeof(wadPath), "/sd/%s", rom->filename);
  if (wadLen <= 4 || static_cast<size_t>(wadLen) >= sizeof(wadPath)) return false;
  return DoomEmu::begin(wadPath);
}

inline bool noBattery(const uint8_t*, size_t) { return false; }
inline bool alwaysBattery(const uint8_t*, size_t) { return true; }
inline bool noFileOp(const char*) { return false; }
inline bool noStateOp(const char*, int) { return false; }

inline bool startSms(const RomFile* rom, uint8_t* romData, size_t romSize) {
  (void)rom;
  return SmsEmu::begin(romData, romSize, false);
}
inline bool startGg(const RomFile* rom, uint8_t* romData, size_t romSize) {
  (void)rom;
  return SmsEmu::begin(romData, romSize, true);
}
inline bool startPce(const RomFile* rom, uint8_t* romData, size_t romSize) {
  (void)rom;
  return PceEmu::begin(romData, romSize);
}
inline bool startAtari(const RomFile* rom, uint8_t* romData, size_t romSize) {
  (void)rom;
  return AtariEmu::begin(romData, romSize);
}
inline bool startPico(const RomFile* rom, uint8_t* romData, size_t romSize) {
  (void)rom;
  return PicoEmu::begin(romData, romSize);
}
inline bool startGenesis(const RomFile* rom, uint8_t* romData, size_t romSize) {
  (void)rom;
  return GenesisEmu::init(romData, romSize);
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
  { 0, ROM_GBC,     true,  startWalnut,  stepWalnut,         WalnutEmu::destroy,  hasGbBattery,  WalnutEmu::loadBatteryRam,  WalnutEmu::saveBatteryRam,  WalnutEmu::saveState,  WalnutEmu::loadState },
  { 1, ROM_GB,      true,  startPeanut,  stepPeanut,         PeanutEmu::destroy,  hasGbBattery,  PeanutEmu::loadBatteryRam,  PeanutEmu::saveBatteryRam,  PeanutEmu::saveState,  PeanutEmu::loadState },
  { 2, ROM_NES,     false, startNes,     stepNes,            NesEmu::destroy,     hasNesBattery, NesEmu::loadBatteryRam,     NesEmu::saveBatteryRam,     NesEmu::saveState,     NesEmu::loadState },
  { 3, ROM_WAD,     false, startDoom,    DoomEmu::runFrame,  DoomEmu::destroy,    noBattery,     noFileOp,                  noFileOp,                  noStateOp,             noStateOp },
  { 4, ROM_SMS,     false, startSms,     SmsEmu::runFrame,   SmsEmu::destroy,     alwaysBattery, SmsEmu::loadBatteryRam,     SmsEmu::saveBatteryRam,     SmsEmu::saveState,     SmsEmu::loadState },
  { 5, ROM_GG,      false, startGg,      SmsEmu::runFrame,   SmsEmu::destroy,     alwaysBattery, SmsEmu::loadBatteryRam,     SmsEmu::saveBatteryRam,     SmsEmu::saveState,     SmsEmu::loadState },
  { 6, ROM_PCE,     false, startPce,     PceEmu::runFrame,   PceEmu::destroy,     alwaysBattery, PceEmu::loadBatteryRam,     PceEmu::saveBatteryRam,     PceEmu::saveState,     PceEmu::loadState },
  { 7, ROM_ATARI,   false, startAtari,   AtariEmu::runFrame, AtariEmu::destroy,   noBattery,     AtariEmu::loadBatteryRam,   AtariEmu::saveBatteryRam,   AtariEmu::saveState,   AtariEmu::loadState },
  { 8, ROM_PICO8,   false, startPico,    PicoEmu::runFrame,  PicoEmu::destroy,    noBattery,     PicoEmu::loadBatteryRam,    PicoEmu::saveBatteryRam,    PicoEmu::saveState,    PicoEmu::loadState },
  { 9, ROM_GENESIS, false, startGenesis, GenesisEmu::update, GenesisEmu::destroy, alwaysBattery, GenesisEmu::loadBatteryRam, GenesisEmu::saveBatteryRam, GenesisEmu::saveState, GenesisEmu::loadState },
  {10, ROM_SNES,    false, startSnes,    SNESEmu::update,    SNESEmu::destroy,    hasSnesBattery,SNESEmu::loadBatteryRam,    SNESEmu::saveBatteryRam,    SNESEmu::saveState,    SNESEmu::loadState },
  {11, ROM_WSWAN,   false, startWSwan,   WSwanEmu::update,   WSwanEmu::destroy,   alwaysBattery, WSwanEmu::loadBatteryRam,   WSwanEmu::saveBatteryRam,   WSwanEmu::saveState,   WSwanEmu::loadState },
  {12, ROM_NGP,     false, startNgp,     NGPEmu::update,     NGPEmu::destroy,     alwaysBattery, NGPEmu::loadBatteryRam,     NGPEmu::saveBatteryRam,     NGPEmu::saveState,     NGPEmu::loadState },
  {13, ROM_LYNX,    false, startLynx,    LynxEmu::update,    LynxEmu::destroy,    hasLynxBattery,LynxEmu::loadBatteryRam,    LynxEmu::saveBatteryRam,    LynxEmu::saveState,    LynxEmu::loadState },
  {14, ROM_COLEM,   false, startColem,   ColemEmu::update,   ColemEmu::destroy,   noBattery,     ColemEmu::loadBatteryRam,   ColemEmu::saveBatteryRam,   ColemEmu::saveState,   ColemEmu::loadState },
};

}  // namespace detail

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

}  // namespace CoreRegistry
