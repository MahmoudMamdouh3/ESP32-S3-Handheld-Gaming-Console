#include "save_manager.h"
#include "sd_card.h"
#include "config.h"
#include "spi_arbiter.h"
#include "display_emu.h"
#if FEATURE_SD_CARD
#include <SD.h>
#include <SPI.h>
#endif
#include <string.h>
#include <stdio.h>

namespace SaveManager {

  static int s_activeSlot = 1;

  // CRC32 IEEE 802.3 bitwise computation (lightweight, zero memory table)
  uint32_t computeCRC32(const uint8_t* data, size_t length, uint32_t crc) {
    if (!data || length == 0) return crc;
    while (length--) {
      crc ^= *data++;
      for (int i = 0; i < 8; ++i) {
        if (crc & 1) {
          crc = (crc >> 1) ^ 0xEDB88320;
        } else {
          crc >>= 1;
        }
      }
    }
    return crc;
  }

  // Extracts pure basename from filename, stripping directories and extensions
  static void extractBaseName(const char* fullPath, char* outBase, size_t maxLen) {
    if (!fullPath || !outBase || maxLen == 0) return;
    
    // Find last path separator
    const char* p = strrchr(fullPath, '/');
    if (!p) p = strrchr(fullPath, '\\');
    const char* start = p ? (p + 1) : fullPath;
    
    // Find last extension dot
    const char* dot = strrchr(start, '.');
    size_t copyLen = dot ? (size_t)(dot - start) : strlen(start);
    if (copyLen >= maxLen) copyLen = maxLen - 1;
    
    strncpy(outBase, start, copyLen);
    outBase[copyLen] = '\0';
  }

  void getBatterySavePath(const char* romFilename, char* outPath, size_t maxLen) {
    char base[48];
    extractBaseName(romFilename, base, sizeof(base));
    snprintf(outPath, maxLen, "/saves/%s.sav", base);
  }

  void getSaveStatePath(const char* romFilename, int slot, char* outPath, size_t maxLen) {
    if (slot < 1) slot = 1;
    if (slot > MAX_SAVE_SLOTS) slot = MAX_SAVE_SLOTS;
    char base[48];
    extractBaseName(romFilename, base, sizeof(base));
    snprintf(outPath, maxLen, "/saves/%s.s0%d", base, slot);
  }

  bool begin() {
#if FEATURE_SD_CARD
    if (!SDCard::isMounted()) return false;
    DisplayEmu::waitForDisplay();
    SpiArbiter::lock();
    if (!SD.exists("/saves")) {
      SD.mkdir("/saves");
    }
    SpiArbiter::unlock();
    return true;
#else
    return false;
#endif
  }

  int getActiveSlot() {
    return s_activeSlot;
  }

  void setActiveSlot(int slot) {
    if (slot >= 1 && slot <= MAX_SAVE_SLOTS) {
      s_activeSlot = slot;
    }
  }

  void cycleActiveSlot() {
    s_activeSlot = (s_activeSlot % MAX_SAVE_SLOTS) + 1;
  }

  bool saveBatteryRam(const char* romFilename, const uint8_t* ram, size_t ramSize) {
    if (!romFilename || !ram || ramSize == 0) return false;
#if FEATURE_SD_CARD
    if (!SDCard::isMounted()) return false;

    char path[80];
    getBatterySavePath(romFilename, path, sizeof(path));

    DisplayEmu::waitForDisplay();
    SpiArbiter::lock();

    if (!SD.exists("/saves")) {
      SD.mkdir("/saves");
    }

    File f = SD.open(path, FILE_WRITE);
    if (!f) {
      SpiArbiter::unlock();
      LOG_ERROR("SaveManager: Failed to open %s for write", path);
      return false;
    }

    size_t written = f.write(ram, ramSize);
    f.flush();
    f.close();
    SpiArbiter::unlock();

    if (written == ramSize) {
      LOG_INFO("SaveManager: Saved battery RAM to %s (%u bytes)", path, (unsigned int)ramSize);
      return true;
    } else {
      LOG_ERROR("SaveManager: Incomplete write to %s (%u/%u bytes)", path, (unsigned int)written, (unsigned int)ramSize);
      return false;
    }
#else
    return false;
#endif
  }

  bool loadBatteryRam(const char* romFilename, uint8_t* ram, size_t maxRamSize, size_t* outLoadedSize) {
    if (outLoadedSize) *outLoadedSize = 0;
    if (!romFilename || !ram || maxRamSize == 0) return false;
#if FEATURE_SD_CARD
    if (!SDCard::isMounted()) return false;

    char path[80];
    getBatterySavePath(romFilename, path, sizeof(path));

    DisplayEmu::waitForDisplay();
    SpiArbiter::lock();

    if (!SD.exists(path)) {
      SpiArbiter::unlock();
      return false; // No existing save file
    }

    File f = SD.open(path, FILE_READ);
    if (!f) {
      SpiArbiter::unlock();
      LOG_WARN("SaveManager: Failed to open %s for read", path);
      return false;
    }

    size_t fileSize = f.size();
    size_t toRead = (fileSize < maxRamSize) ? fileSize : maxRamSize;
    size_t bytesRead = f.read(ram, toRead);
    f.close();
    SpiArbiter::unlock();

    if (outLoadedSize) *outLoadedSize = bytesRead;
    LOG_INFO("SaveManager: Loaded battery RAM from %s (%u bytes)", path, (unsigned int)bytesRead);
    return (bytesRead > 0);
#else
    return false;
#endif
  }

  bool hasBatteryRam(const char* romFilename) {
#if FEATURE_SD_CARD
    if (!romFilename || !SDCard::isMounted()) return false;
    char path[80];
    getBatterySavePath(romFilename, path, sizeof(path));

    DisplayEmu::waitForDisplay();
    SpiArbiter::lock();
    bool exists = SD.exists(path);
    SpiArbiter::unlock();
    return exists;
#else
    return false;
#endif
  }

  bool saveState(const char* romFilename, int slot, uint32_t coreId,
                 const void* stateData, size_t stateSize,
                 const uint8_t* ramData, size_t ramSize) {
    if (!romFilename || !stateData || stateSize == 0) return false;
#if FEATURE_SD_CARD
    if (!SDCard::isMounted()) return false;

    char path[80];
    getSaveStatePath(romFilename, slot, path, sizeof(path));

    // Calculate CRC32 of payload
    uint32_t crc = computeCRC32((const uint8_t*)stateData, stateSize, 0xFFFFFFFF);
    if (ramData && ramSize > 0) {
      crc = computeCRC32(ramData, ramSize, crc);
    }
    crc ^= 0xFFFFFFFF; // Finalize CRC

    SaveStateHeader header;
    memset(&header, 0, sizeof(header));
    header.magic = SAVE_STATE_MAGIC;
    header.version = SAVE_STATE_VERSION;
    header.timestamp = millis();
    header.coreId = coreId;
    header.stateSize = (uint32_t)stateSize;
    header.ramSize = (uint32_t)ramSize;
    header.crc32 = crc;
    extractBaseName(romFilename, header.romTitle, sizeof(header.romTitle));

    DisplayEmu::waitForDisplay();
    SpiArbiter::lock();

    if (!SD.exists("/saves")) {
      SD.mkdir("/saves");
    }

    File f = SD.open(path, FILE_WRITE);
    if (!f) {
      SpiArbiter::unlock();
      LOG_ERROR("SaveManager: Failed to open %s for write", path);
      return false;
    }

    size_t headerWritten = f.write((const uint8_t*)&header, sizeof(header));
    size_t stateWritten = f.write((const uint8_t*)stateData, stateSize);
    size_t ramWritten = 0;
    if (ramData && ramSize > 0) {
      ramWritten = f.write(ramData, ramSize);
    }

    f.flush();
    f.close();
    SpiArbiter::unlock();

    bool ok = (headerWritten == sizeof(header)) &&
              (stateWritten == stateSize) &&
              (ramWritten == ramSize);

    if (ok) {
      LOG_INFO("SaveManager: Saved state to %s (Slot %d, core %u, state %u B, ram %u B)",
               path, slot, (unsigned int)coreId, (unsigned int)stateSize, (unsigned int)ramSize);
    } else {
      LOG_ERROR("SaveManager: Incomplete write to %s", path);
    }
    return ok;
#else
    return false;
#endif
  }

  bool loadState(const char* romFilename, int slot, uint32_t expectedCoreId,
                 void* stateData, size_t maxStateSize, size_t* outStateSize,
                 uint8_t* ramData, size_t maxRamSize, size_t* outRamSize) {
    if (outStateSize) *outStateSize = 0;
    if (outRamSize) *outRamSize = 0;
    if (!romFilename || !stateData || maxStateSize == 0) return false;
#if FEATURE_SD_CARD
    if (!SDCard::isMounted()) return false;

    char path[80];
    getSaveStatePath(romFilename, slot, path, sizeof(path));

    DisplayEmu::waitForDisplay();
    SpiArbiter::lock();

    if (!SD.exists(path)) {
      SpiArbiter::unlock();
      return false;
    }

    File f = SD.open(path, FILE_READ);
    if (!f) {
      SpiArbiter::unlock();
      LOG_WARN("SaveManager: Failed to open %s for read", path);
      return false;
    }

    SaveStateHeader header;
    if (f.read((uint8_t*)&header, sizeof(header)) != sizeof(header)) {
      f.close();
      SpiArbiter::unlock();
      LOG_ERROR("SaveManager: Truncated header in %s", path);
      return false;
    }

    if (header.magic != SAVE_STATE_MAGIC || header.version != SAVE_STATE_VERSION) {
      f.close();
      SpiArbiter::unlock();
      LOG_ERROR("SaveManager: Invalid header in %s (magic: 0x%08X)", path, (unsigned int)header.magic);
      return false;
    }

    if (header.coreId != expectedCoreId) {
      f.close();
      SpiArbiter::unlock();
      LOG_ERROR("SaveManager: Core mismatch in %s (expected %u, got %u)",
                path, (unsigned int)expectedCoreId, (unsigned int)header.coreId);
      return false;
    }

    if (header.stateSize > maxStateSize) {
      f.close();
      SpiArbiter::unlock();
      LOG_ERROR("SaveManager: State buffer overflow (stateSize %u > max %u)",
                (unsigned int)header.stateSize, (unsigned int)maxStateSize);
      return false;
    }

    size_t stateRead = f.read((uint8_t*)stateData, header.stateSize);
    size_t ramRead = 0;
    if (header.ramSize > 0 && ramData && maxRamSize >= header.ramSize) {
      ramRead = f.read(ramData, header.ramSize);
    }

    f.close();
    SpiArbiter::unlock();

    if (stateRead != header.stateSize || (header.ramSize > 0 && ramRead != header.ramSize)) {
      LOG_ERROR("SaveManager: Truncated state data in %s", path);
      return false;
    }

    // Verify CRC32
    uint32_t calcCrc = computeCRC32((const uint8_t*)stateData, header.stateSize, 0xFFFFFFFF);
    if (ramData && header.ramSize > 0) {
      calcCrc = computeCRC32(ramData, header.ramSize, calcCrc);
    }
    calcCrc ^= 0xFFFFFFFF;

    if (calcCrc != header.crc32) {
      LOG_ERROR("SaveManager: CRC32 mismatch in %s (calc 0x%08X != hdr 0x%08X)",
                path, (unsigned int)calcCrc, (unsigned int)header.crc32);
      return false;
    }

    if (outStateSize) *outStateSize = stateRead;
    if (outRamSize) *outRamSize = ramRead;

    LOG_INFO("SaveManager: Restored state from %s (Slot %d, core %u, state %u B, ram %u B)",
             path, slot, (unsigned int)expectedCoreId, (unsigned int)stateRead, (unsigned int)ramRead);
    return true;
#else
    return false;
#endif
  }

  bool hasSaveState(const char* romFilename, int slot) {
#if FEATURE_SD_CARD
    if (!romFilename || !SDCard::isMounted()) return false;
    char path[80];
    getSaveStatePath(romFilename, slot, path, sizeof(path));

    DisplayEmu::waitForDisplay();
    SpiArbiter::lock();
    bool exists = SD.exists(path);
    SpiArbiter::unlock();
    return exists;
#else
    return false;
#endif
  }

  bool getSaveStateHeader(const char* romFilename, int slot, SaveStateHeader* outHeader) {
    if (!outHeader || !romFilename) return false;
#if FEATURE_SD_CARD
    if (!SDCard::isMounted()) return false;
    char path[80];
    getSaveStatePath(romFilename, slot, path, sizeof(path));

    DisplayEmu::waitForDisplay();
    SpiArbiter::lock();

    if (!SD.exists(path)) {
      SpiArbiter::unlock();
      return false;
    }

    File f = SD.open(path, FILE_READ);
    if (!f) {
      SpiArbiter::unlock();
      return false;
    }

    size_t n = f.read((uint8_t*)outHeader, sizeof(SaveStateHeader));
    f.close();
    SpiArbiter::unlock();

    return (n == sizeof(SaveStateHeader) && outHeader->magic == SAVE_STATE_MAGIC);
#else
    return false;
#endif
  }

}
