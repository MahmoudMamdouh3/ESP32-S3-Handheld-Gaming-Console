# Firmware symbols: core (1/2)

Generated from the declarations in the listed headers. Edit the header, then run `python -m tools.guardian symbols`.

## `firmware/BmoGameboy/src/core/audio_i2s.h`

- `namespace AudioI2S`
- `bool begin();`
- `void pushSample(int16_t left, int16_t right);`

## `firmware/BmoGameboy/src/core/battery.h`

- `namespace Battery`
- `void begin();`
- `float getVoltage();`
- `int getVoltageMv();`
- `int getPercentage();`
- `void update();`
- `void safeShutdown();`

## `firmware/BmoGameboy/src/core/battery_save.h`

- `namespace BatterySave`
- `return (flags6 & 0x02U) != 0;`
- `return static_cast<size_t>(1U) << (sramSizeCode + 10U);`

## `firmware/BmoGameboy/src/core/bmo_face.h`

- `namespace BmoFace`
- `enum BmoExpression`
- `void begin();`
- `void setExpression(BmoExpression expr);`
- `BmoExpression getExpression();`
- `void update();`
- `void draw(int x, int y, int size);`
- `void draw();`
- `bool isDirty();`
- `void setGaze(float gx, float gy);`
- `void tickle(float intensity = 1.0f);`
- `void triggerWink();`
- `void triggerBlush();`
- `void triggerJoy();`
- `void say(const char* quote, unsigned long durationMs = 3500);`
- `const char* getCurrentQuote();`
- `int getVisibleChars();`
- `bool hasSpeech();`
- `void clearSpeech();`
- `void triggerDance(float bpm = 125.0f);`
- `void stopDance();`
- `bool isDancing();`
- `int getBuddyLevel();`
- `int getHappiness();`
- `void petCompanion();`
- `void renderToBuffer(uint16_t* dst, int stride, int x, int y, int w, int h);`
- `void renderFullScreen(uint16_t* dst, int width = 320, int height = 240);`
- `uint16_t getScreenBgColor();`

## `firmware/BmoGameboy/src/core/box_art.h`

- `class BoxArt`
- `static bool init();`
- `static bool load(const char* romFilename);`
- `static void unload();`
- `static bool hasArt();`
- `static const uint16_t* getBuffer();`
- `static void draw(GFXcanvas16* canvas, int x, int y, bool drawBorder = true, uint16_t borderColor = Theme::BMO_WHITE);`
- `static bool existsForRom(const char* romFilename);`
- `static void getBaseName(const char* romFilename, char* outBaseName, size_t maxLen);`
- `static bool loadRaw(const char* path);`
- `static bool loadBmp(const char* path);`

## `firmware/BmoGameboy/src/core/buttons.h`

- `struct ButtonState`
- `namespace Buttons`
- `enum Index : int { UP = 0, DOWN, LEFT, RIGHT, A, B, START, SELECT, X, Y, L, R };`
- `void begin();`
- `void update();`
- `int count();`
- `const ButtonState &get(int index);`
- `void setGameplayMode(bool inGame);`
- `void setSwapAB(bool swap);`
- `bool isSwapAB();`
- `bool toggleSwapAB();`
- `void setTurboEnabled(bool enabled, uint8_t periodFrames = 4);`
- `bool isTurboEnabled();`
- `bool toggleTurbo();`

## `firmware/BmoGameboy/src/core/config.h`

- `#define FEATURE_SD_CARD 1`
- `#define FEATURE_AUDIO 0`
- `#define FEATURE_BATTERY_MONITOR 0`
- `#define TFT_SCK    12`
- `#define TFT_MOSI   11`
- `#define TFT_RST    9`
- `#define TFT_DC     8`
- `#define TFT_CS     10`
- `#define TFT_WIDTH  240`
- `#define TFT_HEIGHT 320`
- `#define TFT_SPI_HZ 80000000UL`
- `#define TFT_SPI_FALLBACK_HZ 40000000UL // Diagnostics comparison only; default remains 80 MHz.`
- `#define SD_MISO    15`
- `#define SD_CS      13`
- `#define SD_SPI_HZ  25000000UL`
- `#define BTN_UP     4`
- `#define BTN_DOWN   5`
- `#define BTN_LEFT   6`
- `#define BTN_RIGHT  7`
- `#define BTN_A      16`
- `#define BTN_B      17`
- `#define BTN_START  18`
- `#define BTN_SELECT 21`
- `#define BTN_X      14`
- `#define BTN_Y      42`
- `#define BTN_L      47`
- `#define BTN_R      48`

## `firmware/BmoGameboy/src/core/display_emu.h`

- `namespace DisplayEmu`
- `void begin();`
- `void clearScreen();`
- `void showSDCardWarning();`
- `void initMenuUI();`
- `void drawBootSplash(bool pressAnyButtonBlink);`
- `void drawConsoleSelectMenu(int selectedIndex, const int* gameCounts, int consoleCount, bool sdMounted);`
- `void drawConsoleMuseumModal(RomType console);`
- `void cleanupMenuUI();`
- `void showToast(const char* message, uint16_t borderColor = Theme::BMO_DPAD_YELLOW, unsigned long durationMs = 1800);`
- `bool hasActiveToast();`
- `void triggerCarouselGlide(bool directionRight);`
- `void showAlphaBadge(char letter, unsigned long durationMs = 800);`
- `void setTopPicksFilter(bool enabled);`
- `bool getTopPicksFilter();`
- `uint32_t getDisplayStackHighWaterMarkWords();`
- `void setDiagnosticSpiClock(uint32_t spiClockHz);`
- `void drawSpiStressPattern(uint32_t spiClockHz, uint32_t frameNumber);`
- `void drawConsoleIcon(RomType type, int x, int y, uint16_t primaryColor = 0);`
- `void drawIdleMascotScreen(unsigned long idleSeconds, const char* stateMessage = nullptr);`
- `void sanitizeRomTitle(const char* src, char* dst, size_t maxLen);`
- `void setDmgPalette(int paletteIndex);`
- `int getDmgPaletteIndex();`
- `void cycleDmgPalette();`
- `const uint16_t* getActiveDmgPalette();`
- `void setBezelEnabled(bool enabled);`
- `bool isBezelEnabled();`
- `bool toggleBezel();`
- `void drawConsoleBezel(int consoleIndex);`
- `const uint16_t* getLastFramebuffer(int* outWidth, int* outHeight);`
- `void streamFrame(const uint8_t* gb_framebuffer, uint16_t* palette);`
- `void streamNESFrame(const uint8_t* nes_framebuffer);`
- `void streamDoomFrame(const uint8_t* cmap);`
- `void streamSMSFrame(const uint16_t* sms_framebuffer, bool isGameGear = false);`
- `void streamPCEFrame(const uint16_t* pce_framebuffer);`
- `void streamAtariFrame(const uint16_t* atari_framebuffer);`
- `void streamPicoFrame(const uint16_t* pico_framebuffer);`
- `void streamGenesisFrame(const uint16_t* genesis_framebuffer, int width, int height);`
- `void streamSNESFrame(const uint16_t* snes_framebuffer, int width, int height);`
- `void streamWSwanFrame(const uint16_t* wswan_framebuffer, int width, int height);`
- `void streamNGPFrame(const uint16_t* ngp_framebuffer, int width, int height);`
- `void streamLynxFrame(const uint16_t* lynx_framebuffer, int width, int height);`
- `void streamColemFrame(const uint16_t* colem_framebuffer, int width, int height);`
- `void pushPixels(int yOffset, const uint16_t* rowBuffer, int rowsToDraw);`
- `void pushPixelsRaw(int yOffset, const uint16_t* rowBuffer, int rowsToDraw);`
- `void pushPixelsFullScreen(const uint16_t* buffer);`
- `void pushPixelsAt(int x, int y, int w, int h, const uint16_t* buf);`
- `void startDirectWindow(int x, int y, int w, int h);`
- `void writeWindowBytes(const uint8_t* data, size_t len);`
- `void endDirectWindow();`
- `void startFrame();`
- `void endFrame();`
- `void streamPixelRow(const uint16_t* buf, int pixelCount);`
- `void streamGBFrame(const uint16_t* gb_framebuffer);`
- `void streamRawFrameAsync(const uint16_t* fb, int x, int y, int w, int h);`
- `void waitForDisplay();`
- `bool isDisplayBusy();`

## `firmware/BmoGameboy/src/core/fram_save.h`

- `namespace FramSave`
- `bool begin();`
- `bool readSaveData(uint16_t offset, uint8_t* buffer, size_t length);`
- `bool writeSaveData(uint16_t offset, const uint8_t* buffer, size_t length);`

## `firmware/BmoGameboy/src/core/frame_pacing.h`

- `namespace FramePacing`

## `firmware/BmoGameboy/src/core/frame_percentiles.h`

- `namespace FramePercentiles`
- `struct Summary`
- `const unsigned long a = *(const unsigned long*)left;`
- `const unsigned long b = *(const unsigned long*)right;`
- `return (a > b) - (a < b);`
- `qsort(samples, count, sizeof(samples[0]), compareUnsignedLong);`

## `firmware/BmoGameboy/src/core/harmonic_spring.h`

- `namespace BmoFaceInternal`
- `struct HarmonicSpring`
- `const float inverseDeterminant = 1.0f / (f + dtSquaredOmegaSquared);`
- `const float nextValue = (f * val + dt * vel + dtSquaredOmegaSquared * target) * inverseDeterminant;`
- `const float nextVelocity = (vel + dtOmegaSquared * (target - val)) * inverseDeterminant;`

## `firmware/BmoGameboy/src/core/path_key.h`

- `namespace RomKey`
- `crc ^= static_cast<uint8_t>(normalizePathByte(*relativePath++));`
- `crc = (crc >> 1) ^ ((crc & 1U) ? 0xEDB88320U : 0U);`
- `const char* pathEnd = relativePath + strlen(relativePath);`
- `const size_t stemLength = dot ? static_cast<size_t>(dot - filename) : strlen(filename);`
- `const bool allowed = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';`
- `static_cast<unsigned long>(crc32RelativePath(relativePath)));`

## `firmware/BmoGameboy/src/core/pause_quit_policy.h`

- `namespace PauseQuit`
- `struct State`
- `enum class Input`
- `enum class Action`

## `firmware/BmoGameboy/src/core/profiler.h`

- `#define FEATURE_PROFILER 0`
- `enum ProfileZone`
- `class BmoProfiler`
- `static void begin();`
- `static void startZone(ProfileZone zone);`
- `static void endZone(ProfileZone zone);`
- `static void reportIfInterval(unsigned long intervalMs = 1000);`
- `static void reset();`
- `static uint32_t getZoneAvgUs(ProfileZone zone);`
- `static uint32_t getZoneMinUs(ProfileZone zone);`
- `static uint32_t getZoneMaxUs(ProfileZone zone);`

## `firmware/BmoGameboy/src/core/rom_file_filter.h`

- `namespace RomFileFilter`
- `const char* slash = strrchr(name, '/');`
- `return strrchr(base, '.');`
- `equalIgnoreCase(name, "SYSTEM VOLUME INFORMATION");`
- `const char* ext = extensionOf(name);`
- `equalIgnoreCase(ext, ".col");`
- `const char* ext = extensionOf(name);`
- `memcmp(genesisHeader, "SEGA", 4) == 0;`
- `(prefix[0] == 0x55 && prefix[1] == 0xAA))) return true;`
- `return looksLikeBinary(prefix, prefixSize);`

## `firmware/BmoGameboy/src/core/rom_index.h`

- `struct BmoIndexHeader`
- `struct BmoIndexEntry`
- `namespace BmoIndex`
- `crc ^= (uint8_t)(words[word] >> (byte * 8));`
- `crc = (crc >> 1) ^ ((crc & 1U) ? 0xEDB88320U : 0U);`
- `static_assert(sizeof(BmoIndexHeader) == 36, "BmoIndexHeader v2 disk size changed");`
- `static_assert(offsetof(BmoIndexHeader, timestamp) == 24, "BmoIndexHeader v2 timestamp offset changed");`
- `static_assert(sizeof(BmoIndexEntry) == 72, "BmoIndexEntry v2 disk size changed");`
- `static_assert(offsetof(BmoIndexEntry, type) == 64, "BmoIndexEntry v2 type offset changed");`
- `static_assert(offsetof(BmoIndexEntry, fileSize) == 68, "BmoIndexEntry v2 fileSize offset changed");`
