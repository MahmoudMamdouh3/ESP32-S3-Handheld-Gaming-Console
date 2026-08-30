#pragma once
#include <stdint.h>
#include <stddef.h>

enum RomType {
  ROM_UNKNOWN,
  ROM_GB,
  ROM_GBC,
  ROM_NES,
  ROM_WAD,
  ROM_SMS,
  ROM_GG,
  ROM_PCE,
  ROM_ATARI,
  ROM_PICO8
};

struct RomFile {
  char filename[64];
  RomType type;
};

class SDCard {
public:
  static bool begin();
  static bool isMounted();
  
  // Scans the root directory for ROMs and builds a list
  static void scanRoms();
  static int getRomCount();
  static const RomFile* getRomInfo(int index);
  
  // Dynamically load a ROM file entirely into PSRAM
  // Returns pointer to PSRAM buffer (must be free'd via freeRom) or nullptr on failure.
  static uint8_t* loadRom(const char* filename, size_t* outSize);
  static void freeRom(uint8_t* buffer);
  
};
