#include "sd_internal.h"
#include "../hal/config.h"
#include "../hal/spi_arbiter.h"
#if FEATURE_SD_CARD
#include <SD.h>
#include <SPI.h>
#include <esp32-hal-spi.h>
#endif
#include <esp_heap_caps.h>
#include <string.h>

// Baked ROMs are disabled by default so the public repository compiles with no
// embedded game payloads. To include a local user-supplied header at
// src/storage/local_baked_rom.h, build with -DBMO_ENABLE_BAKED_ROMS=1.
#if BMO_ENABLE_BAKED_ROMS && defined(__has_include)
#if __has_include("local_baked_rom.h")
#include "local_baked_rom.h"
#define BMO_HAS_LOCAL_BAKED_ROM 1
#endif
#endif
#ifndef BMO_HAS_LOCAL_BAKED_ROM
#define BMO_HAS_LOCAL_BAKED_ROM 0
#endif

namespace BmoSdInternal {

bool mounted = false;
uint32_t mountedClockHz = 0;

static RomFile fallbackRomList[32];
RomFile* romList = fallbackRomList;
int maxCapacity = 32;
int numRoms = 0;
int skippedRomCount = 0;
SDCard::ScanStats scanStats = {};
bool poolGrowthFailed = false;
int favoritesCount = 0;
int romCountsByType[ROM_COLEM + 1] = {0};

static char* s_pathArena = nullptr;
static size_t s_pathArenaUsed = 0;
static size_t s_pathArenaCap = 0;

static bool growPathArena(size_t requiredCap) {
  if (requiredCap <= s_pathArenaCap && s_pathArena != nullptr) {
    return true;
  }
  size_t nextCap = (s_pathArenaCap == 0) ? 16384U : s_pathArenaCap;
  while (nextCap < requiredCap) {
    if (nextCap > (4U * 1024U * 1024U)) return false;
    nextCap *= 2U;
  }

  const uintptr_t oldStart = reinterpret_cast<uintptr_t>(s_pathArena);
  const uintptr_t oldEnd = oldStart + s_pathArenaUsed;
  char* nextBuf = static_cast<char*>(s_pathArena ? heap_caps_realloc(s_pathArena, nextCap, MALLOC_CAP_SPIRAM)
                                                 : heap_caps_malloc(nextCap, MALLOC_CAP_SPIRAM));
  if (!nextBuf) {
    return false;
  }
  if (s_pathArena && oldStart != reinterpret_cast<uintptr_t>(nextBuf)) {
    const ptrdiff_t delta = nextBuf - reinterpret_cast<const char*>(oldStart);
    for (int i = 0; i < numRoms; ++i) {
      const uintptr_t addr = reinterpret_cast<uintptr_t>(romList[i].filename);
      if (addr >= oldStart && addr < oldEnd) {
        romList[i].filename = romList[i].filename + delta;
      }
    }
  }
  s_pathArena = nextBuf;
  s_pathArenaCap = nextCap;
  return true;
}

void resetPathArena() {
  if (s_pathArena) {
    heap_caps_free(s_pathArena);
    s_pathArena = nullptr;
  }
  s_pathArenaUsed = 0;
  s_pathArenaCap = 0;
}

bool reserveExactPathArena(size_t totalBytes) {
  resetPathArena();
  if (totalBytes == 0U) return true;
  s_pathArena = static_cast<char*>(heap_caps_malloc(totalBytes, MALLOC_CAP_SPIRAM));
  if (!s_pathArena) return false;
  s_pathArenaCap = totalBytes;
  s_pathArenaUsed = totalBytes;
  return true;
}

const char* internPath(const char* relPath, size_t length) {
  if (!relPath || length == 0 || length > SDCard::MAX_ROM_PATH_LEN) return nullptr;
  const size_t needed = s_pathArenaUsed + length + 1U;
  if (!growPathArena(needed)) {
    return nullptr;
  }
  char* dst = s_pathArena + s_pathArenaUsed;
  memcpy(dst, relPath, length);
  dst[length] = '\0';
  s_pathArenaUsed += length + 1U;
  return dst;
}

void appendArenaPathDirect(const char* src, size_t len, size_t offset) {
  if (!s_pathArena || !src || offset + len + 1U > s_pathArenaCap) return;
  memcpy(s_pathArena + offset, src, len + 1U);
}

const char* arenaBase() {
  return s_pathArena;
}

bool ensurePoolCapacity(int targetCapacity) {
  if (targetCapacity < SDCard::MIN_ROM_POOL) targetCapacity = SDCard::MIN_ROM_POOL;
  if (targetCapacity > SDCard::MAX_ROM_POOL) targetCapacity = SDCard::MAX_ROM_POOL;
  if (romList == fallbackRomList) {
    RomFile* psramList = static_cast<RomFile*>(
        heap_caps_malloc(sizeof(RomFile) * static_cast<size_t>(targetCapacity), MALLOC_CAP_SPIRAM));
    if (!psramList) return false;
    if (numRoms > 0) {
      const int copyCount = (numRoms < targetCapacity) ? numRoms : targetCapacity;
      memcpy(psramList, fallbackRomList, sizeof(RomFile) * static_cast<size_t>(copyCount));
    }
    romList = psramList;
    maxCapacity = targetCapacity;
    return true;
  }
  if (targetCapacity == maxCapacity) return true;
  RomFile* resized = static_cast<RomFile*>(
      heap_caps_realloc(romList, sizeof(RomFile) * static_cast<size_t>(targetCapacity), MALLOC_CAP_SPIRAM));
  if (!resized) return (maxCapacity >= targetCapacity);
  romList = resized;
  maxCapacity = targetCapacity;
  if (numRoms > maxCapacity) numRoms = maxCapacity;
  return true;
}

int bakedRomCount() {
  return BMO_HAS_LOCAL_BAKED_ROM;
}

void seedBakedRoms() {
#if BMO_HAS_LOCAL_BAKED_ROM
  if (numRoms >= maxCapacity) return;
  romList[numRoms].filename = local_baked_rom_name;
  romList[numRoms].type = local_baked_rom_type;
  romList[numRoms].isFavorite = 1;
  romList[numRoms].hasBoxArt = 0;
  romList[numRoms].isPopular = 1;
  romList[numRoms].fileSize = static_cast<uint32_t>(local_baked_rom_size);
  romList[numRoms].crc32Prefix = 0;
  favoritesCount++;
  if (local_baked_rom_type <= ROM_COLEM) {
    romCountsByType[local_baked_rom_type]++;
  }
  numRoms++;
#endif
}

const uint8_t* tryLoadBakedRom(const char* filename, size_t* outSize) {
#if BMO_HAS_LOCAL_BAKED_ROM
  if (filename && outSize && strcmp(filename, local_baked_rom_name) == 0) {
    *outSize = local_baked_rom_size;
    return static_cast<const uint8_t*>(local_baked_rom);
  }
#else
  (void)filename;
  (void)outSize;
#endif
  return nullptr;
}

bool isBakedRomPointer(const uint8_t* buffer) {
#if BMO_HAS_LOCAL_BAKED_ROM
  return buffer != nullptr && buffer == local_baked_rom;
#else
  (void)buffer;
  return false;
#endif
}

} // namespace BmoSdInternal

int SDCard::getPoolCapacity() {
  return BmoSdInternal::maxCapacity;
}

int SDCard::getSkippedRomCount() {
  return BmoSdInternal::skippedRomCount;
}

SDCard::ScanStats SDCard::getScanStats() {
  return BmoSdInternal::scanStats;
}

bool SDCard::begin() {
  using namespace BmoSdInternal;
  numRoms = 0;
  skippedRomCount = 0;
  scanStats = ScanStats{};
  poolGrowthFailed = false;
  favoritesCount = 0;
  memset(romCountsByType, 0, sizeof(romCountsByType));
  memset(fallbackRomList, 0, sizeof(fallbackRomList));
  resetPathArena();

  ensurePoolCapacity(MIN_ROM_POOL);
  seedBakedRoms();

#if FEATURE_SD_CARD
  {
    SpiArbiter::ScopedLock spiLock;
    const uint32_t candidatesHz[] = {SD_SPI_HZ, 20000000UL, 10000000UL, 4000000UL};
    uint32_t successfulRequestHz = 0;
    for (size_t candidateIndex = 0; candidateIndex < sizeof(candidatesHz) / sizeof(candidatesHz[0]); ++candidateIndex) {
      const uint32_t candidateHz = candidatesHz[candidateIndex];
      if (candidateHz == 0 || candidateHz > SD_SPI_HZ) continue;

      bool duplicate = false;
      for (size_t earlier = 0; earlier < candidateIndex; ++earlier) {
        if (candidatesHz[earlier] == candidateHz) duplicate = true;
      }
      if (duplicate) continue;

      if (SD.begin(SD_CS, SPI, candidateHz, "/sd")) {
        successfulRequestHz = candidateHz;
        break;
      }
      LOG_WARN("SD mount failed at %lu Hz; trying a lower SPI clock when available", (unsigned long)candidateHz);
      SD.end();
    }

    if (successfulRequestHz == 0) {
      mounted = false;
      mountedClockHz = 0;
      return false;
    }
    mounted = true;
    const uint32_t cappedRequestHz = successfulRequestHz > 25000000UL ? 25000000UL : successfulRequestHz;
    mountedClockHz = spiClockDivToFrequency(nullptr, spiFrequencyToClockDiv(nullptr, cappedRequestHz));
    LOG_INFO("SD mounted with nominal SPI clock %lu Hz (request %lu Hz)", (unsigned long)mountedClockHz,
             (unsigned long)successfulRequestHz);
  }
  scanRoms();
  loadFavorites();
  loadRecentHistory();
#else
  mounted = false;
#endif
  return true;
}

bool SDCard::isMounted() {
  return BmoSdInternal::mounted;
}

uint32_t SDCard::getClockHz() {
  return BmoSdInternal::mounted ? BmoSdInternal::mountedClockHz : 0;
}
