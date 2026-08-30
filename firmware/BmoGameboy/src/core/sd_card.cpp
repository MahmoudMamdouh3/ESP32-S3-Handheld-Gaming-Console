#include "sd_card.h"
#include "config.h"
#if FEATURE_SD_CARD
#include <SD.h>
#include <SPI.h>
#endif
#include <esp_heap_caps.h>
#include <string.h>
#include "../assets/roms/mario_deluxe.h"
#include "../assets/roms/zelda_ages.h"
// Baked GBC ROMs (1MB each; tracked in repository under src/assets/roms/)
#include "../assets/roms/aladdin.h"
#include "../assets/roms/lego_racers.h"

namespace {
  bool mounted = false;
  static const int MAX_ROMS = 16384;
  static RomFile fallbackRomList[32];
  static RomFile* romList = fallbackRomList;
  static int maxCapacity = 32;
  int numRoms = 0;

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
}

bool SDCard::begin() {
  numRoms = 0;

  if (romList == fallbackRomList) {
    RomFile* psramList = (RomFile*)heap_caps_malloc(sizeof(RomFile) * MAX_ROMS, MALLOC_CAP_SPIRAM);
    if (psramList) {
      romList = psramList;
      maxCapacity = MAX_ROMS;
    }
  }

  // Always add baked ROMs first!
  strncpy(romList[numRoms].filename, "Super Mario Bros Deluxe (Baked).gbc", 63);
  romList[numRoms].type = ROM_GBC;
  numRoms++;

  strncpy(romList[numRoms].filename, "Legend of Zelda Ages (Baked).gbc", 63);
  romList[numRoms].type = ROM_GBC;
  numRoms++;

  strncpy(romList[numRoms].filename, "Aladdin (Baked).gbc", 63);
  romList[numRoms].type = ROM_GBC;
  numRoms++;

  strncpy(romList[numRoms].filename, "Lego Racers (Baked).gbc", 63);
  romList[numRoms].type = ROM_GBC;
  numRoms++;

  // Use explicit "/sd" mount point so standard C functions (fopen) can access it
#if FEATURE_SD_CARD
  if (!SD.begin(SD_CS, SPI, 4000000, "/sd")) {
    mounted = false;
    // Do not reset numRoms to 0, because we have baked ROMs!
    return false;
  }
  mounted = true;
  scanRoms();
#else
  mounted = false;
#endif
  return true;
}

bool SDCard::isMounted() {
  return mounted;
}

void SDCard::scanRoms() {
#if FEATURE_SD_CARD
  // Do not reset numRoms to 0, we already added baked ROMs!
  File root = SD.open("/");
  if (!root || !root.isDirectory()) return;

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
        numRoms++;
      }
    }
    entry.close();
  }
  root.close();
#endif
}

int SDCard::getRomCount() {
  return numRoms;
}

const RomFile* SDCard::getRomInfo(int index) {
  if (index < 0 || index >= numRoms) return nullptr;
  return &romList[index];
}

uint8_t* SDCard::loadRom(const char* filename, size_t* outSize) {
  if (!filename || !outSize) return nullptr;
  *outSize = 0;

  // Check for baked ROMs first
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
  File file = SD.open(String("/") + filename, FILE_READ);
  if (!file) return nullptr;

  size_t size = file.size();
  if (size == 0) {
    file.close();
    return nullptr;
  }
  *outSize = size;

  // Allocate strictly in PSRAM (external SPI RAM) since ROMs are up to 4MB
  uint8_t* buffer = (uint8_t*)heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
  if (!buffer) {
    file.close();
    return nullptr;
  }

  // Read entire file in chunks
  size_t bytesRead = 0;
  while (bytesRead < size) {
    int chunk = file.read(buffer + bytesRead, size - bytesRead);
    if (chunk <= 0) break; // EOF or error
    bytesRead += chunk;
  }
  
  file.close();
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
    if (buffer == mario_deluxe_rom || buffer == zelda_ages_rom ||
        buffer == aladdin_rom || buffer == lego_racers_rom) {
      return;
    }
    heap_caps_free(buffer);
  }
}




