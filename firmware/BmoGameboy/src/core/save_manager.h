#pragma once
#include <stdint.h>
#include <stddef.h>

namespace SaveManager {
  constexpr uint32_t SAVE_STATE_MAGIC = 0x31535342; // "BSS1" ('B', 'S', 'S', '1')
  constexpr uint32_t SAVE_STATE_VERSION = 1;
  constexpr int MAX_SAVE_SLOTS = 5;

  enum CoreId : uint32_t {
    CORE_NONE   = 0,
    CORE_GB_DMG = 1,
    CORE_GB_CGB = 2,
    CORE_NES    = 3,
    CORE_SMS    = 4,
    CORE_DOOM   = 5
  };

  struct SaveStateHeader {
    uint32_t magic;         // SAVE_STATE_MAGIC
    uint32_t version;       // SAVE_STATE_VERSION
    uint32_t timestamp;     // System millis() or timestamp
    uint32_t coreId;        // CoreId enum
    uint32_t stateSize;     // Size of core state struct in bytes
    uint32_t ramSize;       // Size of cartridge RAM in bytes
    uint32_t crc32;         // CRC32 checksum of (stateData + ramData)
    char romTitle[32];      // Sanitized ROM title
  };

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
  bool saveState(const char* romFilename, int slot, uint32_t coreId,
                 const void* stateData, size_t stateSize,
                 const uint8_t* ramData = nullptr, size_t ramSize = 0);

  bool loadState(const char* romFilename, int slot, uint32_t expectedCoreId,
                 void* stateData, size_t maxStateSize, size_t* outStateSize = nullptr,
                 uint8_t* ramData = nullptr, size_t maxRamSize = 0, size_t* outRamSize = nullptr);

  bool hasSaveState(const char* romFilename, int slot);
  bool getSaveStateHeader(const char* romFilename, int slot, SaveStateHeader* outHeader);

  // Helper CRC32 computation
  uint32_t computeCRC32(const uint8_t* data, size_t length, uint32_t crc = 0xFFFFFFFF);

  // Path resolution helpers
  void getBatterySavePath(const char* romFilename, char* outPath, size_t maxLen);
  void getSaveStatePath(const char* romFilename, int slot, char* outPath, size_t maxLen);
}
