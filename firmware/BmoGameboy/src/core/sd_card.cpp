#include "sd_card.h"
#include "config.h"
#include "spi_arbiter.h"
#include "rom_index.h"
#include "save_manager.h"
#include "box_art.h"
#if FEATURE_SD_CARD
#include <SD.h>
#include <SPI.h>
#endif
#include <esp_heap_caps.h>
#include <string.h>
#include <stdlib.h>
#include "../assets/roms/virtual_bmo.h"
#include "../assets/roms/mario_deluxe.h"
#include "../assets/roms/zelda_ages.h"
// Baked GBC ROMs (1MB each; tracked in repository under src/assets/roms/)
#include "../assets/roms/aladdin.h"
#include "../assets/roms/lego_racers.h"

namespace {
  bool mounted = false;
  static const int MAX_ROMS = 16384;
  static const int BAKED_ROM_COUNT = 5;
  static RomFile fallbackRomList[32];
  static RomFile* romList = fallbackRomList;
  static int maxCapacity = 32;
  int numRoms = 0;
  static int s_favoritesCount = 0;                  // PERF-C1: O(1) counter — maintained by begin/toggleFavorite/loadFavorites
  static int romCountsByType[ROM_COLEM + 1] = {0};

  RomType determineType(const char* filename) {
    const char* ext = strrchr(filename, '.');
    if (!ext) return ROM_UNKNOWN;
    if (strcasecmp(ext, ".gb") == 0) return ROM_GB;
    if (strcasecmp(ext, ".gbc") == 0) return ROM_GBC;
    if (strcasecmp(ext, ".nes") == 0) return ROM_NES;
    if (strcasecmp(ext, ".wad") == 0) return ROM_WAD;
    if (strcasecmp(ext, ".sms") == 0) return ROM_SMS;
    if (strcasecmp(ext, ".gg") == 0) return ROM_GG;
    if (strcasecmp(ext, ".pce") == 0) return ROM_PCE;
    if (strcasecmp(ext, ".a26") == 0 || strcasecmp(ext, ".a78") == 0) return ROM_ATARI;
    if (strcasecmp(ext, ".p8") == 0) return ROM_PICO8;
    if (strcasecmp(ext, ".gen") == 0 || strcasecmp(ext, ".md") == 0 || strcasecmp(ext, ".smd") == 0) return ROM_GENESIS;
    if (strcasecmp(ext, ".sfc") == 0 || strcasecmp(ext, ".smc") == 0) return ROM_SNES;
    if (strcasecmp(ext, ".ws") == 0 || strcasecmp(ext, ".wsc") == 0) return ROM_WSWAN;
    if (strcasecmp(ext, ".ngp") == 0 || strcasecmp(ext, ".ngc") == 0) return ROM_NGP;
    if (strcasecmp(ext, ".lnx") == 0) return ROM_LYNX;
    if (strcasecmp(ext, ".col") == 0 || strcasecmp(ext, ".sg") == 0) return ROM_COLEM;
    return ROM_UNKNOWN;
  }

  int compareRoms(const void* a, const void* b) {
    const RomFile* ra = (const RomFile*)a;
    const RomFile* rb = (const RomFile*)b;
    return strcasecmp(ra->filename, rb->filename);
  }
}

bool SDCard::begin() {
  numRoms = 0;
  memset(romCountsByType, 0, sizeof(romCountsByType));

  if (romList == fallbackRomList) {
    RomFile* psramList = (RomFile*)heap_caps_malloc(sizeof(RomFile) * MAX_ROMS, MALLOC_CAP_SPIRAM);
    if (psramList) {
      romList = psramList;
      maxCapacity = MAX_ROMS;
    }
  }

  // Always add premier Virtual BMO (Official Game) first - pre-favorited!
  strncpy(romList[numRoms].filename, "Virtual BMO (Official Game).gb", 63);
  romList[numRoms].filename[63] = '\0';
  romList[numRoms].type = ROM_GB;
  romList[numRoms].isFavorite = true;
  romList[numRoms].hasBoxArt = false;
  s_favoritesCount++;                  // PERF-C1
  romCountsByType[ROM_GB]++;
  numRoms++;

  // Baked classics
  strncpy(romList[numRoms].filename, "Super Mario Bros Deluxe (Baked).gbc", 63);
  romList[numRoms].filename[63] = '\0';
  romList[numRoms].type = ROM_GBC;
  romList[numRoms].isFavorite = true;
  romList[numRoms].hasBoxArt = false;
  s_favoritesCount++;                  // PERF-C1
  romCountsByType[ROM_GBC]++;
  numRoms++;

  strncpy(romList[numRoms].filename, "Legend of Zelda Ages (Baked).gbc", 63);
  romList[numRoms].filename[63] = '\0';
  romList[numRoms].type = ROM_GBC;
  romList[numRoms].isFavorite = true;
  romList[numRoms].hasBoxArt = false;
  s_favoritesCount++;                  // PERF-C1
  romCountsByType[ROM_GBC]++;
  numRoms++;

  strncpy(romList[numRoms].filename, "Aladdin (Baked).gbc", 63);
  romList[numRoms].filename[63] = '\0';
  romList[numRoms].type = ROM_GBC;
  romList[numRoms].isFavorite = false;
  romList[numRoms].hasBoxArt = false;
  romCountsByType[ROM_GBC]++;
  numRoms++;

  strncpy(romList[numRoms].filename, "Lego Racers (Baked).gbc", 63);
  romList[numRoms].filename[63] = '\0';
  romList[numRoms].type = ROM_GBC;
  romList[numRoms].isFavorite = false;
  romList[numRoms].hasBoxArt = false;
  romCountsByType[ROM_GBC]++;
  numRoms++;

  // PERF-01: Run SD card at 25 MHz standard high-speed clock (up from 4 MHz)
#if FEATURE_SD_CARD
  SpiArbiter::lock();
  if (!SD.begin(SD_CS, SPI, 25000000, "/sd")) {
    mounted = false;
    SpiArbiter::unlock();
    // Do not reset numRoms to 0, because we have baked ROMs!
    return false;
  }
  mounted = true;
  scanRoms();
  loadFavorites();
  SpiArbiter::unlock();
#else
  mounted = false;
#endif
  return true;
}

bool SDCard::isMounted() {
  return mounted;
}

void SDCard::sortRoms() {
  if (numRoms > BAKED_ROM_COUNT) {
    qsort(&romList[BAKED_ROM_COUNT], numRoms - BAKED_ROM_COUNT, sizeof(RomFile), compareRoms);
  }
}

bool SDCard::loadIndex() {
#if FEATURE_SD_CARD
  if (!SD.exists(BMO_INDEX_PATH)) return false;

  File f = SD.open(BMO_INDEX_PATH, FILE_READ);
  if (!f) return false;

  BmoIndexHeader header;
  if (f.read((uint8_t*)&header, sizeof(header)) != sizeof(header)) {
    f.close();
    return false;
  }

  if (strncmp(header.magic, BMO_INDEX_MAGIC, 8) != 0 ||
      header.version != BMO_INDEX_VERSION ||
      header.entrySize != sizeof(BmoIndexEntry) ||
      header.entryCount == 0 ||
      header.entryCount > (uint32_t)(maxCapacity - BAKED_ROM_COUNT)) {
    f.close();
    return false;
  }

  size_t expectedFileSize = sizeof(BmoIndexHeader) + header.entryCount * sizeof(BmoIndexEntry);
  if (f.size() != expectedFileSize) {
    f.close();
    return false;
  }

  size_t entriesBytes = header.entryCount * sizeof(BmoIndexEntry);
  BmoIndexEntry* entries = (BmoIndexEntry*)heap_caps_malloc(entriesBytes, MALLOC_CAP_SPIRAM);
  if (!entries) {
    f.close();
    return false;
  }

  if (f.read((uint8_t*)entries, entriesBytes) != entriesBytes) {
    heap_caps_free(entries);
    f.close();
    return false;
  }
  f.close();

  uint32_t computedCrc = SaveManager::computeCRC32((const uint8_t*)entries, entriesBytes);
  if (computedCrc != header.crc32) {
    heap_caps_free(entries);
    return false;
  }

  for (uint32_t i = 0; i < header.entryCount && numRoms < maxCapacity; ++i) {
    const BmoIndexEntry& e = entries[i];
    strncpy(romList[numRoms].filename, e.filename, 63);
    romList[numRoms].filename[63] = '\0';
    romList[numRoms].type = (RomType)e.type;
    romList[numRoms].isFavorite = (e.isFavorite != 0);
    romList[numRoms].hasBoxArt = (e.hasBoxArt != 0);
    if (romList[numRoms].type <= ROM_COLEM) {
      romCountsByType[romList[numRoms].type]++;
    }
    numRoms++;
  }

  heap_caps_free(entries);
  return true;
#else
  return false;
#endif
}

bool SDCard::saveIndex() {
#if FEATURE_SD_CARD
  if (numRoms <= BAKED_ROM_COUNT) return false;

  uint32_t entryCount = numRoms - BAKED_ROM_COUNT;
  size_t entriesBytes = entryCount * sizeof(BmoIndexEntry);
  BmoIndexEntry* entries = (BmoIndexEntry*)heap_caps_malloc(entriesBytes, MALLOC_CAP_SPIRAM);
  if (!entries) return false;

  for (uint32_t i = 0; i < entryCount; ++i) {
    const RomFile& rf = romList[BAKED_ROM_COUNT + i];
    strncpy(entries[i].filename, rf.filename, 63);
    entries[i].filename[63] = '\0';
    entries[i].type = (uint8_t)rf.type;
    entries[i].isFavorite = rf.isFavorite ? 1 : 0;
    entries[i].hasBoxArt = rf.hasBoxArt ? 1 : 0;
    entries[i].reserved = 0;
    entries[i].fileSize = 0;
  }

  BmoIndexHeader header;
  memset(&header, 0, sizeof(header));
  memcpy(header.magic, BMO_INDEX_MAGIC, 8);
  header.version = BMO_INDEX_VERSION;
  header.entryCount = entryCount;
  header.entrySize = sizeof(BmoIndexEntry);
  header.crc32 = SaveManager::computeCRC32((const uint8_t*)entries, entriesBytes);
  header.timestamp = 0;

  File f = SD.open(BMO_INDEX_PATH, FILE_WRITE);
  if (!f) {
    heap_caps_free(entries);
    return false;
  }

  bool ok = true;
  if (f.write((const uint8_t*)&header, sizeof(header)) != sizeof(header)) ok = false;
  if (ok && f.write((const uint8_t*)entries, entriesBytes) != entriesBytes) ok = false;
  f.close();

  heap_caps_free(entries);
  return ok;
#else
  return false;
#endif
}

void SDCard::rebuildIndex() {
#if FEATURE_SD_CARD
  SpiArbiter::lock();
  if (SD.exists(BMO_INDEX_PATH)) {
    SD.remove(BMO_INDEX_PATH);
  }

  numRoms = BAKED_ROM_COUNT;
  memset(romCountsByType, 0, sizeof(romCountsByType));
  for (int i = 0; i < BAKED_ROM_COUNT; ++i) {
    if (romList[i].type <= ROM_COLEM) {
      romCountsByType[romList[i].type]++;
    }
  }

  scanRoms();
  loadFavorites();
  SpiArbiter::unlock();
#endif
}

void SDCard::scanRoms() {
#if FEATURE_SD_CARD
  SpiArbiter::lock();

  // Try fast-loading cached index first (<15ms vs 3500ms directory crawl)
  if (loadIndex()) {
    SpiArbiter::unlock();
    return;
  }

  File root = SD.open("/");
  if (!root || !root.isDirectory()) {
    SpiArbiter::unlock();
    return;
  }

  while (numRoms < maxCapacity) {
    File entry = root.openNextFile();
    if (!entry) break;

    const char* name = entry.name();
    if (name[0] != '.' && !entry.isDirectory()) {
      RomType type = determineType(name);
      if (type != ROM_UNKNOWN) {
        strncpy(romList[numRoms].filename, name, 63);
        romList[numRoms].filename[63] = '\0';
        romList[numRoms].type = type;
        romList[numRoms].isFavorite = false;
        romList[numRoms].hasBoxArt = BoxArt::existsForRom(name);
        if (type <= ROM_COLEM) {
          romCountsByType[type]++;
        }
        numRoms++;
      }
    }
    entry.close();
  }
  root.close();

  // Sort newly crawled ROMs alphabetically
  sortRoms();

  // Save fresh binary index cache
  saveIndex();

  SpiArbiter::unlock();
#endif
}

int SDCard::getRomCount() {
  return numRoms;
}

int SDCard::getRomCountForType(RomType type) {
  if (type == ROM_FAVORITES) {
    return getFavoritesCount();
  }
  if (type <= ROM_COLEM) return romCountsByType[type];
  return 0;
}

const RomFile* SDCard::getRomInfo(int index) {
  if (index < 0 || index >= numRoms) return nullptr;
  return &romList[index];
}

bool SDCard::isFavorite(int index) {
  if (index < 0 || index >= numRoms) return false;
  return romList[index].isFavorite;
}

bool SDCard::isFavorite(const char* filename) {
  if (!filename) return false;
  for (int i = 0; i < numRoms; ++i) {
    if (strcmp(romList[i].filename, filename) == 0) {
      return romList[i].isFavorite;
    }
  }
  return false;
}

void SDCard::toggleFavorite(int index) {
  if (index < 0 || index >= numRoms) return;
  // PERF-C1: Maintain O(1) counter instead of letting getFavoritesCount() re-scan.
  if (romList[index].isFavorite) {
    romList[index].isFavorite = false;
    s_favoritesCount--;
  } else {
    romList[index].isFavorite = true;
    s_favoritesCount++;
  }
  saveFavorites();
}

int SDCard::getFavoritesCount() {
  // PERF-C1: O(1) — counter maintained by begin/toggleFavorite/loadFavorites.
  return s_favoritesCount;
}

void SDCard::saveFavorites() {
#if FEATURE_SD_CARD
  if (!mounted) return;
  SpiArbiter::lock();
  File f = SD.open("/favorites.txt", FILE_WRITE);
  if (!f) {
    SpiArbiter::unlock();
    return;
  }
  for (int i = 0; i < numRoms; ++i) {
    if (romList[i].isFavorite) {
      f.println(romList[i].filename);
    }
  }
  f.close();
  SpiArbiter::unlock();
#endif
}

void SDCard::loadFavorites() {
#if FEATURE_SD_CARD
  if (!mounted) return;
  SpiArbiter::lock();
  if (!SD.exists("/favorites.txt")) {
    SpiArbiter::unlock();
    return;
  }
  File f = SD.open("/favorites.txt", FILE_READ);
  if (!f) {
    SpiArbiter::unlock();
    return;
  }
  // H-3 / L-5: Stack char[] instead of Arduino String — no heap alloc per line.
  char line[64];
  while (f.available()) {
    int len = f.readBytesUntil('\n', line, (int)sizeof(line) - 1);
    line[len] = '\0';
    // Trim trailing \r and spaces to handle Windows \r\n line endings.
    while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == ' ')) {
      line[--len] = '\0';
    }
    if (len > 0) {
      for (int i = 0; i < numRoms; ++i) {
        if (strcmp(romList[i].filename, line) == 0) {
          if (!romList[i].isFavorite) {
            romList[i].isFavorite = true;
            s_favoritesCount++;  // PERF-C1: maintain O(1) counter
          }
          break;
        }
      }
    }
  }
  f.close();
  SpiArbiter::unlock();
#endif
}

uint8_t* SDCard::loadRom(const char* filename, size_t* outSize) {
  if (!filename || !outSize) return nullptr;
  *outSize = 0;

  // Check for baked ROMs first
  if (strcmp(filename, "Virtual BMO (Official Game).gb") == 0) {
    *outSize = virtual_bmo_rom_size;
    return (uint8_t*)virtual_bmo_rom;
  }
  if (strcmp(filename, "Super Mario Bros Deluxe (Baked).gbc") == 0) {
    *outSize = mario_deluxe_rom_size;
    return (uint8_t*)mario_deluxe_rom;
  }
  if (strcmp(filename, "Legend of Zelda Ages (Baked).gbc") == 0) {
    *outSize = zelda_ages_rom_size;
    return (uint8_t*)zelda_ages_rom;
  }
  if (strcmp(filename, "Aladdin (Baked).gbc") == 0) {
    *outSize = aladdin_rom_size;
    return (uint8_t*)aladdin_rom;
  }
  if (strcmp(filename, "Lego Racers (Baked).gbc") == 0) {
    *outSize = lego_racers_rom_size;
    return (uint8_t*)lego_racers_rom;
  }

#if FEATURE_SD_CARD
  // H-3: Stack buffer instead of Arduino String — avoids DRAM heap alloc on every ROM launch.
  char path[128];
  snprintf(path, sizeof(path), "/%s", filename);
  SpiArbiter::lock();
  File file = SD.open(path, FILE_READ);
  if (!file) {
    SpiArbiter::unlock();
    return nullptr;
  }

  size_t size = file.size();
  if (size == 0) {
    file.close();
    SpiArbiter::unlock();
    return nullptr;
  }
  *outSize = size;

  // Allocate strictly in PSRAM (external SPI RAM) since ROMs are up to 4MB
  uint8_t* buffer = (uint8_t*)heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
  if (!buffer) {
    file.close();
    SpiArbiter::unlock();
    return nullptr;
  }

  // Read entire file in multi-sector burst chunks (PERF-17: 64KB chunk bursts)
  size_t bytesRead = 0;
  while (bytesRead < size) {
    size_t toRead = (size - bytesRead > 65536) ? 65536 : (size - bytesRead);
    int chunk = file.read(buffer + bytesRead, toRead);
    if (chunk <= 0) break; // EOF or error
    bytesRead += chunk;
  }
  
  file.close();
  SpiArbiter::unlock();
  if (bytesRead != size) {
    // Never hand a truncated ROM to an emulator: it can fail much later with
    // a misleading crash or an out-of-bounds bank read.
    heap_caps_free(buffer);
    *outSize = 0;
    return nullptr;
  }
  return buffer;
#else
  return nullptr;
#endif
}

void SDCard::freeRom(uint8_t* buffer) {
  if (buffer) {
    // DO NOT free baked ROM pointers residing in Flash .rodata
    if (buffer == virtual_bmo_rom || buffer == mario_deluxe_rom ||
        buffer == zelda_ages_rom || buffer == aladdin_rom ||
        buffer == lego_racers_rom) {
      return;
    }
    heap_caps_free(buffer);
  }
}




