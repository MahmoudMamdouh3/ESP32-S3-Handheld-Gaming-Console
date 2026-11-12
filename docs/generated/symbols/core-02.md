# Firmware symbols: core (2/2)

Generated from the declarations in the listed headers. Edit the header, then run `python -m tools.guardian symbols`.

## `firmware/BmoGameboy/src/core/save_manager.h`

- `namespace SaveManager`
- `enum CoreId : uint32_t`
- `struct SaveStateHeader`
- `bool begin();`
- `int getActiveSlot();`
- `void setActiveSlot(int slot);`
- `void cycleActiveSlot();`
- `bool saveBatteryRam(const char* romFilename, const uint8_t* ram, size_t ramSize);`
- `bool loadBatteryRam(const char* romFilename, uint8_t* ram, size_t maxRamSize, size_t* outLoadedSize = nullptr);`
- `bool hasBatteryRam(const char* romFilename);`
- `bool hasSaveState(const char* romFilename, int slot);`
- `bool getSaveStateHeader(const char* romFilename, int slot, SaveStateHeader* outHeader);`
- `uint32_t computeCRC32(const uint8_t* data, size_t length, uint32_t crc = 0xFFFFFFFF);`
- `void getBatterySavePath(const char* romFilename, char* outPath, size_t maxLen);`
- `void getSaveStatePath(const char* romFilename, int slot, char* outPath, size_t maxLen);`
- `bool saveScreenshotBmp(const char* romFilename, const uint16_t* bgr565Buffer, int width, int height);`

## `firmware/BmoGameboy/src/core/sd_card.h`

- `enum RomType : uint8_t`
- `struct RomFile`
- `static_assert(sizeof(RomFile) == 72, "RomFile cached index layout changed");`
- `static_assert(offsetof(RomFile, fileSize) == 68, "RomFile fileSize offset changed");`
- `class SDCard`
- `int capacity = catalogCount + (catalogCount / 4) + ((catalogCount % 4) ? 1 : 0);`
- `static int getPoolCapacity();`
- `static int getSkippedRomCount();`
- `static bool begin();`
- `static bool isMounted();`
- `static uint32_t getClockHz();`
- `static void scanRoms();`
- `static void scanDirectory(const char* dirPath, int depth, bool hasArtDirs);`
- `static int getRomCount();`
- `static int getRomCountForType(RomType type);`
- `static int getPopularCountForType(RomType type);`
- `static bool isPopularGame(const char* filename, RomType type);`
- `static const RomFile* getRomInfo(int index);`
- `static bool isFavorite(int index);`
- `static bool isFavorite(const char* filename);`
- `static void toggleFavorite(int index);`
- `static int getFavoritesCount();`
- `static void saveFavorites();`
- `static void loadFavorites();`
- `static bool loadIndex();`
- `static bool saveIndex();`
- `static void rebuildIndex();`
- `static void sortRoms();`
- `static void recordRecentLaunch(const char* filename);`
- `static int getRecentCount();`
- `static const char* getRecentFilename(int index);`
- `static void loadRecentHistory();`
- `static void saveRecentHistory();`
- `static uint8_t* loadRom(const char* filename, size_t* outSize);`
- `static void freeRom(uint8_t* buffer);`

## `firmware/BmoGameboy/src/core/spi_arbiter.h`

- `namespace SpiArbiter`
- `void init();`
- `void lock();`
- `void unlock();`

## `firmware/BmoGameboy/src/core/theme.h`

- `namespace Theme`
- `return (uint16_t)((value << 8) | (value >> 8));`
- `| ((uint16_t)r >> 3);`
- `return swapBytes(bgr565);`
- `| ((uint16_t)r >> 3);`
- `constexpr uint16_t PANEL_DEEP_TEAL   = makePanelColor(26, 75, 66);`
- `constexpr uint16_t PANEL_DPAD_YELLOW = makePanelColor(255, 224, 51);`
- `constexpr uint16_t COLOR_TEXT_MUTED    = makeUiColor(140, 195, 180);`
- `constexpr uint16_t COLOR_SUCCESS       = makeUiColor(76, 217, 100);`
- `enum DmgPaletteType`
