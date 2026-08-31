#pragma once
#include <stdint.h>
#include <stddef.h>

enum RomType {
  ROM_UNKNOWN,
  ROM_FAVORITES,
  ROM_GB,
  ROM_GBC,
  ROM_NES,
  ROM_WAD,
  ROM_SMS,
  ROM_GG,
  ROM_PCE,
  ROM_ATARI,
  ROM_PICO8,
  ROM_GENESIS,
  ROM_SNES,
  ROM_WSWAN,
  ROM_NGP,
  ROM_LYNX,
  ROM_COLEM
};

struct RomFile {
  char filename[64];
  RomType type;
  bool isFavorite;
};

class SDCard {
public:
  static bool begin();
  static bool isMounted();
  
  // Scans the root directory for ROMs and builds a list
  static void scanRoms();
  static int getRomCount();
  static int getRomCountForType(RomType type);
  static const RomFile* getRomInfo(int index);

  // Favorites Management API
  static bool isFavorite(int index);
  static bool isFavorite(const char* filename);
  static void toggleFavorite(int index);
  static int getFavoritesCount();
  static void saveFavorites();
  static void loadFavorites();
  
  // Dynamically load a ROM file entirely into PSRAM
  // Returns pointer to PSRAM buffer (must be free'd via freeRom) or nullptr on failure.
  static uint8_t* loadRom(const char* filename, size_t* outSize);
  static void freeRom(uint8_t* buffer);
  
};
