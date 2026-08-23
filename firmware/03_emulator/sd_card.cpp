#include "sd_card.h"
#include "config.h"
#include <SD.h>
#include <SPI.h>
#include <esp_heap_caps.h>
#include <string.h>

namespace {
  bool mounted = false;
  static const int MAX_ROMS = 100;
  RomFile romList[MAX_ROMS];
  int numRoms = 0;

  RomType determineType(const char* filename) {
    const char* ext = strrchr(filename, '.');
    if (!ext) return ROM_UNKNOWN;
    if (strcasecmp(ext, ".gb") == 0) return ROM_GB;
    if (strcasecmp(ext, ".gbc") == 0) return ROM_GBC;
    if (strcasecmp(ext, ".nes") == 0) return ROM_NES;
    if (strcasecmp(ext, ".wad") == 0) return ROM_WAD;
    return ROM_UNKNOWN;
  }
}

bool SDCard::begin() {
  // Use explicit "/sd" mount point so standard C functions (fopen) can access it
  if (!SD.begin(SD_CS, SPI, 4000000, "/sd")) {
    mounted = false;
    numRoms = 0;
    return false;
  }
  mounted = true;
  scanRoms();
  return true;
}

bool SDCard::isMounted() {
  return mounted;
}

void SDCard::scanRoms() {
  numRoms = 0;
  File root = SD.open("/");
  if (!root || !root.isDirectory()) return;

  while (numRoms < MAX_ROMS) {
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
}

int SDCard::getRomCount() {
  return numRoms;
}

const RomFile* SDCard::getRomInfo(int index) {
  if (index < 0 || index >= numRoms) return nullptr;
  return &romList[index];
}

uint8_t* SDCard::loadRom(const char* filename, size_t* outSize) {
  File file = SD.open(String("/") + filename, FILE_READ);
  if (!file) return nullptr;

  size_t size = file.size();
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
  return buffer;
}

void SDCard::freeRom(uint8_t* buffer) {
  if (buffer) {
    heap_caps_free(buffer);
  }
}
