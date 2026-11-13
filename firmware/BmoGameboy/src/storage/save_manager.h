#pragma once

#include <stddef.h>
#include <stdint.h>

#include "save_state_format.h"

namespace SaveManager {
constexpr uint32_t SAVE_STATE_MAGIC_V1 = SaveStateFormat::MAGIC_V1;
constexpr uint32_t SAVE_STATE_VERSION_V1 = SaveStateFormat::VERSION_V1;
constexpr uint32_t SAVE_STATE_MAGIC = SaveStateFormat::MAGIC_V2;
constexpr uint32_t SAVE_STATE_VERSION = SaveStateFormat::VERSION_V2;
constexpr int MAX_SAVE_SLOTS = 5;

enum CoreId : uint32_t {
  CORE_NONE = 0,
  CORE_GB_DMG = 1,
  CORE_GB_CGB = 2,
  CORE_NES = 3,
  CORE_SMS = 4,
  CORE_DOOM = 5,
  CORE_PCE = 6,
  CORE_ATARI = 7,
  CORE_PICO8 = 8,
  CORE_GENESIS = 9,
  CORE_SNES = 10,
  CORE_WSWAN = 11,
  CORE_NGP = 12,
  CORE_LYNX = 13,
  CORE_COLEM = 14
};

using SaveStateHeaderV1 = SaveStateFormat::HeaderV1;
using SaveStateHeader = SaveStateFormat::HeaderV2;

// Initialize directory structure on SD card (/saves)
bool begin();

// Active slot management (1 to MAX_SAVE_SLOTS)
int getActiveSlot();
void setActiveSlot(int slot);
void cycleActiveSlot();

// Battery RAM Persistence (.sav)
bool saveBatteryRam(const char* romFilename, const uint8_t* ram, size_t ramSize);
bool loadBatteryRam(const char* romFilename, uint8_t* ram, size_t maxRamSize, size_t* outLoadedSize = nullptr);
bool hasBatteryRam(const char* romFilename);

// Real-Time Save States (.s01 - .s05)
bool saveState(const char* romFilename, int slot, uint32_t coreId, const void* stateData, size_t stateSize,
               const uint8_t* ramData = nullptr, size_t ramSize = 0, uint32_t expectedRomSize = 0);

bool loadState(const char* romFilename, int slot, uint32_t expectedCoreId, void* stateData, size_t maxStateSize,
               size_t* outStateSize = nullptr, uint8_t* ramData = nullptr, size_t maxRamSize = 0,
               size_t* outRamSize = nullptr, uint32_t expectedRomSize = 0);

bool hasSaveState(const char* romFilename, int slot);
bool getSaveStateHeader(const char* romFilename, int slot, SaveStateHeader* outHeader);

// Helper CRC32 computation
uint32_t computeCRC32(const uint8_t* data, size_t length, uint32_t crc = 0xFFFFFFFF);

// Path resolution helpers
void getBatterySavePath(const char* romFilename, char* outPath, size_t maxLen);
void getSaveStatePath(const char* romFilename, int slot, char* outPath, size_t maxLen);

// In-Game Screenshot Dump (.bmp on MicroSD /screenshots/)
bool saveScreenshotBmp(const char* romFilename, const uint16_t* bgr565Buffer, int width, int height);
} // namespace SaveManager
