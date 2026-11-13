#pragma once

#include <stddef.h>
#include <stdint.h>

enum RomType : uint8_t {
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
  const char* filename; // Pointer into PSRAM variable-length path arena (<= MAX_ROM_PATH_LEN bytes)
  uint32_t fileSize;    // File size in bytes on FAT32 card
  uint32_t crc32Prefix; // Standard CRC32 of first min(fileSize, 4096) bytes
  RomType type;
  uint8_t isFavorite;
  uint8_t hasBoxArt;
  uint8_t isPopular;
};

static_assert(sizeof(RomFile) == (sizeof(const char*) == 4U ? 16U : 24U), "RomFile compact layout changed");
static_assert(offsetof(RomFile, fileSize) == sizeof(const char*), "RomFile fileSize offset changed");
static_assert(offsetof(RomFile, crc32Prefix) == sizeof(const char*) + 4U, "RomFile crc32Prefix offset changed");

class SDCard {
public:
  static constexpr int MIN_ROM_POOL = 256;
  static constexpr int MAX_ROM_POOL = 36000;
  static constexpr size_t MAX_ROM_PATH_LEN = 255U;

  struct ScanStats {
    uint32_t ignoredNames;
    uint32_t hiddenOrSystem;
    uint32_t filteredFormat;
    uint32_t pathTooLong;
    uint32_t poolFull;
    uint32_t unreadable;
    bool cancelled;
  };

  using ScanCancelFn = bool (*)();
  using ScanProgressFn = void (*)(int scannedCount);

  static constexpr int computePoolCapacity(int catalogCount) {
    if (catalogCount < 0) catalogCount = 0;
    if (catalogCount >= MAX_ROM_POOL) return MAX_ROM_POOL;
    int capacity = catalogCount + (catalogCount / 4) + ((catalogCount % 4) ? 1 : 0);
    if (capacity < MIN_ROM_POOL) capacity = MIN_ROM_POOL;
    if (capacity > MAX_ROM_POOL) capacity = MAX_ROM_POOL;
    return capacity;
  }
  static int getPoolCapacity();
  static int getSkippedRomCount();
  static ScanStats getScanStats();

  static bool begin();
  static bool isMounted();
  // Nominal SPI clock selected for the successful mount attempt, derived from
  // the pinned Arduino-ESP32 clock divider. Zero when the card is not mounted.
  static uint32_t getClockHz();

  // Scans the SD directory hierarchy (up to depth 3) for ROMs and builds a catalog
  static void scanRoms(ScanCancelFn shouldCancel = nullptr, ScanProgressFn onProgress = nullptr);
  static void scanDirectory(const char* dirPath, int depth, bool hasArtDirs);
  static int getRomCount();
  static int getRomCountForType(RomType type);
  static int getPopularCountForType(RomType type);
  static bool isPopularGame(const char* filename, RomType type);
  static const RomFile* getRomInfo(int index);
  static const RomFile* findRomByPath(const char* relativePath);
  static uint32_t findRomFileSize(const char* relativePath);
  static bool verifyRomIdentity(const RomFile* rom, const uint8_t* loadedBuffer = nullptr, size_t loadedSize = 0);

  // Favorites Management API
  static bool isFavorite(int index);
  static bool isFavorite(const char* filename);
  static void toggleFavorite(int index);
  static int getFavoritesCount();
  static void saveFavorites();
  static void loadFavorites();

  // Binary ROM Index Cache (.bmo_index) API (BMOIDX03, auto-rebuilds BMOIDX02)
  static bool loadIndex();
  static bool saveIndex();
  static void rebuildIndex();
  static void sortRoms();

  // Recently Played History Playlist API (tracks last 10 launched games)
  static void recordRecentLaunch(const char* filename);
  static int getRecentCount();
  static const char* getRecentFilename(int index);
  static void loadRecentHistory();
  static void saveRecentHistory();

  // Dynamically load a ROM file entirely into PSRAM
  // Returns pointer to PSRAM buffer (must be free'd via freeRom) or nullptr on failure.
  static uint8_t* loadRom(const char* filename, size_t* outSize);
  static void freeRom(uint8_t* buffer);
};
