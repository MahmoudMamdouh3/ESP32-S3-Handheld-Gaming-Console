#include "save_manager.h"
#include "sd_card.h"
#include "atomic_fs.h"
#include "sd_atomic_ops.h"
#include "../hal/config.h"
#include "../hal/spi_arbiter.h"
#include "../ui/display_emu.h"
#include "path_key.h"
#if FEATURE_SD_CARD
#include <SD.h>
#include <SPI.h>
#endif
#include <string.h>
#include <stdio.h>

namespace SaveManager {

static int s_activeSlot = 1;

uint32_t computeCRC32(const uint8_t* data, size_t length, uint32_t crc) {
  return AtomicFs::crc32Update(data, length, crc);
}

#if FEATURE_SD_CARD
static bool makeTemporaryPath(const char* finalPath, char* temporaryPath, size_t maxLen) {
  return AtomicFs::makeTempPath(finalPath, temporaryPath, maxLen);
}

static bool recoverAtomicWrite(const char* finalPath) {
  const AtomicFs::FsOps ops = SdAtomicOps::makeOps();
  const bool ok = AtomicFs::recoverTransaction(ops, finalPath);
  if (!ok) {
    LOG_ERROR("SaveManager: Could not recover atomic save transaction %s", finalPath);
  }
  return ok;
}

static bool finishAtomicWrite(const char* temporaryPath, const char* finalPath, size_t expectedFileSize,
                              size_t payloadOffset, size_t payloadSize, uint32_t expectedCrc) {
  const AtomicFs::FsOps ops = SdAtomicOps::makeOps();
  bool backupDeferred = false;
  const bool ok = AtomicFs::commitTransaction(ops, temporaryPath, finalPath, expectedFileSize, payloadOffset,
                                              payloadSize, expectedCrc, &backupDeferred);
  if (!ok) {
    LOG_ERROR("SaveManager: Atomic commit failed for %s", finalPath);
  } else if (backupDeferred) {
    LOG_WARN("SaveManager: Verified new file; backup cleanup deferred for %s", finalPath);
  }
  return ok;
}
#endif

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

static bool makeLegacyBatterySavePath(const char* romFilename, char* outPath, size_t maxLen) {
  if (!romFilename || !outPath || maxLen == 0) return false;
  char base[48];
  extractBaseName(romFilename, base, sizeof(base));
  const int written = snprintf(outPath, maxLen, "/saves/%s.sav", base);
  return written > 0 && static_cast<size_t>(written) < maxLen;
}

static bool makeLegacySaveStatePath(const char* romFilename, int slot, char* outPath, size_t maxLen) {
  if (!romFilename || !outPath || maxLen == 0) return false;
  if (slot < 1) slot = 1;
  if (slot > MAX_SAVE_SLOTS) slot = MAX_SAVE_SLOTS;
  char base[48];
  extractBaseName(romFilename, base, sizeof(base));
  const int written = snprintf(outPath, maxLen, "/saves/%s.s0%d", base, slot);
  return written > 0 && static_cast<size_t>(written) < maxLen;
}

void getBatterySavePath(const char* romFilename, char* outPath, size_t maxLen) {
  if (!outPath || maxLen == 0) return;
  char key[33];
  if (!RomKey::make(romFilename, key, sizeof(key))) {
    outPath[0] = '\0';
    return;
  }
  snprintf(outPath, maxLen, "/saves/%s.sav", key);
}

void getSaveStatePath(const char* romFilename, int slot, char* outPath, size_t maxLen) {
  if (!outPath || maxLen == 0) return;
  if (slot < 1) slot = 1;
  if (slot > MAX_SAVE_SLOTS) slot = MAX_SAVE_SLOTS;
  char key[33];
  if (!RomKey::make(romFilename, key, sizeof(key))) {
    outPath[0] = '\0';
    return;
  }
  snprintf(outPath, maxLen, "/saves/%s.s0%d", key, slot);
}

bool begin() {
#if FEATURE_SD_CARD
  if (!SDCard::isMounted()) return false;
  DisplayEmu::waitForDisplay();
  SpiArbiter::ScopedLock spiLock;
  if (!SD.exists("/saves")) {
    SD.mkdir("/saves");
  }
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
  if (path[0] == '\0') return false;
  char temporaryPath[96];
  if (!makeTemporaryPath(path, temporaryPath, sizeof(temporaryPath))) return false;

  DisplayEmu::waitForDisplay();
  size_t written = 0;
  bool committed = false;
  {
    SpiArbiter::ScopedLock spiLock;

    if (!recoverAtomicWrite(path)) {
      return false;
    }

    if (!SD.exists("/saves")) {
      SD.mkdir("/saves");
    }

    if (SD.exists(temporaryPath)) SD.remove(temporaryPath);
    File f = SD.open(temporaryPath, FILE_WRITE);
    if (!f) {
      LOG_ERROR("SaveManager: Failed to open %s for write", path);
      return false;
    }

    written = f.write(ram, ramSize);
    f.flush();
    f.close();
    const uint32_t crc = computeCRC32(ram, ramSize, 0xFFFFFFFFU) ^ 0xFFFFFFFFU;
    committed = written == ramSize && finishAtomicWrite(temporaryPath, path, ramSize, 0, ramSize, crc);
  }

  if (committed) {
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
  if (path[0] == '\0') return false;
  char legacyPath[80];
  const bool hasLegacyPath = makeLegacyBatterySavePath(romFilename, legacyPath, sizeof(legacyPath));
  bool usedLegacyPath = false;

  DisplayEmu::waitForDisplay();
  size_t fileSize = 0;
  size_t bytesRead = 0;
  {
    SpiArbiter::ScopedLock spiLock;

    if (!recoverAtomicWrite(path)) {
      return false;
    }
    if (hasLegacyPath && !recoverAtomicWrite(legacyPath)) {
      return false;
    }

    if (!SD.exists(path)) {
      if (!hasLegacyPath || !SD.exists(legacyPath)) {
        return false; // No existing save file
      }
      strncpy(path, legacyPath, sizeof(path) - 1);
      path[sizeof(path) - 1] = '\0';
      usedLegacyPath = true;
    }

    File f = SD.open(path, FILE_READ);
    if (!f) {
      LOG_WARN("SaveManager: Failed to open %s for read", path);
      return false;
    }

    fileSize = f.size();
    size_t toRead = (fileSize < maxRamSize) ? fileSize : maxRamSize;
    bytesRead = f.read(ram, toRead);
    f.close();
  }

  if (outLoadedSize) *outLoadedSize = bytesRead;
  LOG_INFO("SaveManager: Loaded battery RAM from %s (%u bytes)", path, (unsigned int)bytesRead);
  if (usedLegacyPath && bytesRead > 0 && fileSize <= maxRamSize) {
    // Preserve the legacy file; write the new keyed copy only after a complete read.
    saveBatteryRam(romFilename, ram, bytesRead);
  }
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
  if (path[0] == '\0') return false;
  char legacyPath[80];
  const bool hasLegacyPath = makeLegacyBatterySavePath(romFilename, legacyPath, sizeof(legacyPath));

  DisplayEmu::waitForDisplay();
  SpiArbiter::ScopedLock spiLock;
  const bool recovered = recoverAtomicWrite(path) && (!hasLegacyPath || recoverAtomicWrite(legacyPath));
  bool exists = recovered && (SD.exists(path) || (hasLegacyPath && SD.exists(legacyPath)));
  return exists;
#else
  return false;
#endif
}

bool saveState(const char* romFilename, int slot, uint32_t coreId, const void* stateData, size_t stateSize,
               const uint8_t* ramData, size_t ramSize) {
  if (!romFilename || !stateData || stateSize == 0) return false;
#if FEATURE_SD_CARD
  if (!SDCard::isMounted()) return false;

  char path[80];
  getSaveStatePath(romFilename, slot, path, sizeof(path));
  if (path[0] == '\0') return false;
  char temporaryPath[96];
  if (!makeTemporaryPath(path, temporaryPath, sizeof(temporaryPath))) return false;

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
  bool committed = false;
  {
    SpiArbiter::ScopedLock spiLock;

    if (!recoverAtomicWrite(path)) {
      return false;
    }

    if (!SD.exists("/saves")) {
      SD.mkdir("/saves");
    }

    if (SD.exists(temporaryPath)) SD.remove(temporaryPath);
    File f = SD.open(temporaryPath, FILE_WRITE);
    if (!f) {
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
    const size_t expectedFileSize = sizeof(header) + stateSize + ramSize;
    committed = (headerWritten == sizeof(header)) && (stateWritten == stateSize) && (ramWritten == ramSize) &&
                finishAtomicWrite(temporaryPath, path, expectedFileSize, sizeof(header), stateSize + ramSize, crc);
  }

  bool ok = committed;

  if (ok) {
    LOG_INFO("SaveManager: Saved state to %s (Slot %d, core %u, state %u B, ram %u B)", path, slot,
             (unsigned int)coreId, (unsigned int)stateSize, (unsigned int)ramSize);
  } else {
    LOG_ERROR("SaveManager: Incomplete write to %s", path);
  }
  return ok;
#else
  return false;
#endif
}

bool loadState(const char* romFilename, int slot, uint32_t expectedCoreId, void* stateData, size_t maxStateSize,
               size_t* outStateSize, uint8_t* ramData, size_t maxRamSize, size_t* outRamSize) {
  if (outStateSize) *outStateSize = 0;
  if (outRamSize) *outRamSize = 0;
  if (!romFilename || !stateData || maxStateSize == 0) return false;
#if FEATURE_SD_CARD
  if (!SDCard::isMounted()) return false;

  char path[80];
  getSaveStatePath(romFilename, slot, path, sizeof(path));
  if (path[0] == '\0') return false;
  char legacyPath[80];
  const bool hasLegacyPath = makeLegacySaveStatePath(romFilename, slot, legacyPath, sizeof(legacyPath));
  bool usedLegacyPath = false;

  DisplayEmu::waitForDisplay();
  SaveStateHeader header;
  uint8_t* tempState = nullptr;
  uint8_t* tempRam = nullptr;
  size_t stateRead = 0;
  size_t ramRead = 0;
  {
    SpiArbiter::ScopedLock spiLock;

    if (!recoverAtomicWrite(path)) {
      return false;
    }
    if (hasLegacyPath && !recoverAtomicWrite(legacyPath)) {
      return false;
    }

    if (!SD.exists(path)) {
      if (!hasLegacyPath || !SD.exists(legacyPath)) {
        return false;
      }
      strncpy(path, legacyPath, sizeof(path) - 1);
      path[sizeof(path) - 1] = '\0';
      usedLegacyPath = true;
    }

    File f = SD.open(path, FILE_READ);
    if (!f) {
      LOG_WARN("SaveManager: Failed to open %s for read", path);
      return false;
    }

    if (f.read((uint8_t*)&header, sizeof(header)) != sizeof(header)) {
      f.close();
      LOG_ERROR("SaveManager: Truncated header in %s", path);
      return false;
    }

    if (header.magic != SAVE_STATE_MAGIC || header.version != SAVE_STATE_VERSION) {
      f.close();
      LOG_ERROR("SaveManager: Invalid header in %s (magic: 0x%08X)", path, (unsigned int)header.magic);
      return false;
    }

    if (header.coreId != expectedCoreId) {
      f.close();
      LOG_ERROR("SaveManager: Core mismatch in %s (expected %u, got %u)", path, (unsigned int)expectedCoreId,
                (unsigned int)header.coreId);
      return false;
    }

    if (header.stateSize > maxStateSize) {
      f.close();
      LOG_ERROR("SaveManager: State buffer overflow (stateSize %u > max %u)", (unsigned int)header.stateSize,
                (unsigned int)maxStateSize);
      return false;
    }

    if (header.ramSize > 0 && (!ramData || header.ramSize > maxRamSize)) {
      f.close();
      LOG_ERROR("SaveManager: RAM buffer overflow (ramSize %u > max %u)", (unsigned int)header.ramSize,
                (unsigned int)maxRamSize);
      return false;
    }

    tempState = (uint8_t*)heap_caps_malloc(header.stateSize, MALLOC_CAP_SPIRAM);
    if (!tempState) {
      f.close();
      LOG_ERROR("SaveManager: Out of PSRAM staging state load (%u B)", (unsigned int)header.stateSize);
      return false;
    }

    if (header.ramSize > 0) {
      tempRam = (uint8_t*)heap_caps_malloc(header.ramSize, MALLOC_CAP_SPIRAM);
      if (!tempRam) {
        heap_caps_free(tempState);
        f.close();
        LOG_ERROR("SaveManager: Out of PSRAM staging RAM load (%u B)", (unsigned int)header.ramSize);
        return false;
      }
    }

    stateRead = f.read(tempState, header.stateSize);
    if (header.ramSize > 0) {
      ramRead = f.read(tempRam, header.ramSize);
    }

    f.close();
  }

  if (stateRead != header.stateSize || (header.ramSize > 0 && ramRead != header.ramSize)) {
    heap_caps_free(tempState);
    if (tempRam) heap_caps_free(tempRam);
    LOG_ERROR("SaveManager: Truncated state data in %s", path);
    return false;
  }

  // Verify CRC32 before modifying caller's live emulator state or RAM buffers
  uint32_t calcCrc = computeCRC32(tempState, header.stateSize, 0xFFFFFFFF);
  if (tempRam && header.ramSize > 0) {
    calcCrc = computeCRC32(tempRam, header.ramSize, calcCrc);
  }
  calcCrc ^= 0xFFFFFFFF;

  if (calcCrc != header.crc32) {
    heap_caps_free(tempState);
    if (tempRam) heap_caps_free(tempRam);
    LOG_ERROR("SaveManager: CRC32 mismatch in %s (calc 0x%08X != hdr 0x%08X)", path, (unsigned int)calcCrc,
              (unsigned int)header.crc32);
    return false;
  }

  memcpy(stateData, tempState, header.stateSize);
  heap_caps_free(tempState);
  if (tempRam && ramData && header.ramSize > 0) {
    memcpy(ramData, tempRam, header.ramSize);
    heap_caps_free(tempRam);
  }

  if (outStateSize) *outStateSize = stateRead;
  if (outRamSize) *outRamSize = ramRead;

  LOG_INFO("SaveManager: Restored state from %s (Slot %d, core %u, state %u B, ram %u B)", path, slot,
           (unsigned int)expectedCoreId, (unsigned int)stateRead, (unsigned int)ramRead);
  if (usedLegacyPath) {
    saveState(romFilename, slot, expectedCoreId, stateData, stateRead, ramData, ramRead);
  }
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
  if (path[0] == '\0') return false;
  char legacyPath[80];
  const bool hasLegacyPath = makeLegacySaveStatePath(romFilename, slot, legacyPath, sizeof(legacyPath));

  DisplayEmu::waitForDisplay();
  SpiArbiter::ScopedLock spiLock;
  const bool recovered = recoverAtomicWrite(path) && (!hasLegacyPath || recoverAtomicWrite(legacyPath));
  bool exists = recovered && (SD.exists(path) || (hasLegacyPath && SD.exists(legacyPath)));
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
  if (path[0] == '\0') return false;
  char legacyPath[80];
  const bool hasLegacyPath = makeLegacySaveStatePath(romFilename, slot, legacyPath, sizeof(legacyPath));

  DisplayEmu::waitForDisplay();
  SpiArbiter::ScopedLock spiLock;

  if (!recoverAtomicWrite(path)) {
    return false;
  }
  if (hasLegacyPath && !recoverAtomicWrite(legacyPath)) {
    return false;
  }

  if (!SD.exists(path)) {
    if (!hasLegacyPath || !SD.exists(legacyPath)) {
      return false;
    }
    strncpy(path, legacyPath, sizeof(path) - 1);
    path[sizeof(path) - 1] = '\0';
  }

  File f = SD.open(path, FILE_READ);
  if (!f) {
    return false;
  }

  size_t n = f.read((uint8_t*)outHeader, sizeof(SaveStateHeader));
  f.close();

  return (n == sizeof(SaveStateHeader) && outHeader->magic == SAVE_STATE_MAGIC);
#else
  return false;
#endif
}

bool saveScreenshotBmp(const char* romFilename, const uint16_t* bgr565Buffer, int width, int height) {
  if (!romFilename || !bgr565Buffer || width <= 0 || height <= 0 || width > 320 || height > 240) {
    return false;
  }
#if FEATURE_SD_CARD
  if (!SDCard::isMounted()) return false;

  char base[33];
  if (!RomKey::make(romFilename, base, sizeof(base))) return false;
  char path[96];
  snprintf(path, sizeof(path), "/screenshots/%s_%lu.bmp", base, (unsigned long)millis());
  char temporaryPath[112];
  if (!makeTemporaryPath(path, temporaryPath, sizeof(temporaryPath))) return false;

  DisplayEmu::waitForDisplay();
  SpiArbiter::ScopedLock spiLock;

  if (!recoverAtomicWrite(path)) {
    return false;
  }

  if (!SD.exists("/screenshots")) {
    SD.mkdir("/screenshots");
  }

  if (SD.exists(temporaryPath)) SD.remove(temporaryPath);
  File f = SD.open(temporaryPath, FILE_WRITE);
  if (!f) {
    return false;
  }

  const uint32_t rowStride = (uint32_t)((width * 3 + 3) & ~3);
  const uint32_t dataSize = rowStride * (uint32_t)height;
  const uint32_t fileSize = 54U + dataSize;

  uint8_t header[54];
  memset(header, 0, sizeof(header));
  header[0] = 'B';
  header[1] = 'M';
  header[2] = (uint8_t)(fileSize & 0xFF);
  header[3] = (uint8_t)((fileSize >> 8) & 0xFF);
  header[4] = (uint8_t)((fileSize >> 16) & 0xFF);
  header[5] = (uint8_t)((fileSize >> 24) & 0xFF);
  header[10] = 54;
  header[14] = 40;
  header[18] = (uint8_t)(width & 0xFF);
  header[19] = (uint8_t)((width >> 8) & 0xFF);
  header[22] = (uint8_t)(height & 0xFF);
  header[23] = (uint8_t)((height >> 8) & 0xFF);
  header[26] = 1;
  header[28] = 24;
  header[34] = (uint8_t)(dataSize & 0xFF);
  header[35] = (uint8_t)((dataSize >> 8) & 0xFF);
  header[36] = (uint8_t)((dataSize >> 16) & 0xFF);
  header[37] = (uint8_t)((dataSize >> 24) & 0xFF);

  uint32_t screenshotCrc = computeCRC32(header, sizeof(header), 0xFFFFFFFFU);
  if (f.write(header, sizeof(header)) != sizeof(header)) {
    f.close();
    return false;
  }

  // Stream 24-bpp BMP rows in 64-pixel chunks (<200 B stack vs 960 B row buffer).
  static constexpr int kBmpChunkPixels = 64;
  uint8_t chunkBuf[kBmpChunkPixels * 3];
  const size_t padBytes = rowStride - static_cast<size_t>(width) * 3U;
  static const uint8_t kZeroPad[3] = {0, 0, 0};

  for (int y = height - 1; y >= 0; --y) {
    const uint16_t* srcRow = &bgr565Buffer[y * width];
    for (int xStart = 0; xStart < width; xStart += kBmpChunkPixels) {
      const int count = (width - xStart > kBmpChunkPixels) ? kBmpChunkPixels : (width - xStart);
      for (int i = 0; i < count; ++i) {
        uint16_t raw = srcRow[xStart + i];
        uint16_t unswapped = (uint16_t)(((raw & 0xFF) << 8) | ((raw >> 8) & 0xFF));
        uint8_t b = (uint8_t)((unswapped >> 8) & 0xF8);
        uint8_t g = (uint8_t)((unswapped >> 3) & 0xFC);
        uint8_t r = (uint8_t)((unswapped << 3) & 0xF8);
        chunkBuf[i * 3 + 0] = b;
        chunkBuf[i * 3 + 1] = g;
        chunkBuf[i * 3 + 2] = r;
      }
      const size_t chunkBytes = static_cast<size_t>(count) * 3U;
      if (f.write(chunkBuf, chunkBytes) != chunkBytes) {
        f.close();
        return false;
      }
      screenshotCrc = computeCRC32(chunkBuf, chunkBytes, screenshotCrc);
    }
    if (padBytes > 0U) {
      if (f.write(kZeroPad, padBytes) != padBytes) {
        f.close();
        return false;
      }
      screenshotCrc = computeCRC32(kZeroPad, padBytes, screenshotCrc);
    }
  }

  f.flush();
  f.close();
  const bool committed = finishAtomicWrite(temporaryPath, path, fileSize, 0, fileSize, screenshotCrc ^ 0xFFFFFFFFU);
  return committed;
#else
  return false;
#endif
}

} // namespace SaveManager
