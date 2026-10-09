# Symbol Reference (Ground Truth for Names)
Purpose: the semicolon-hallucination incident happened because a
plausible-sounding function name (`__gb_write16`) and struct field
(`gb->cpu_reg.sp.reg`) were invented and never existed anywhere in the
codebase. This file is the single place to check before referencing a
symbol you're not 100% sure of, so "does this function exist" is a table
lookup, not a memory guess.

## Maintenance rule
This file must be regenerated (not hand-edited from memory) any time a
core emulator file, driver, or shared header changes its public
functions/macros/structs. Regenerate by grepping actual function
signatures, macro `#define`s, and top-level struct declarations from:
`src/core/`, `src/emulators/`, `src/engine/*/`, `src/vendor/*/`.

## Staleness detection
This file was last regenerated: **2026-10-09**.
If any of the following files have a newer git commit date than that,
this file is stale — grep live and update before making any symbol claims:
- `src/emulators/emu_walnut.cpp/h`
- `src/emulators/emu_peanut.cpp/h`
- `src/emulators/emu_nes.cpp/h`
- `src/emulators/emu_doom.cpp/h`
- `src/emulators/emu_sms.cpp/h`
- `src/emulators/emu_pce.cpp/h`
- `src/emulators/emu_atari.cpp/h`
- `src/emulators/emu_pico.cpp/h`
- `src/emulators/emu_genesis.cpp/h`
- `src/emulators/emu_snes.cpp/h`
- `src/emulators/emu_wswan.cpp/h`
- `src/emulators/emu_ngp.cpp/h`
- `src/emulators/emu_lynx.cpp/h`
- `src/emulators/emu_colem.cpp/h`
- `src/core/display_emu.h`
- `src/core/buttons.h`
- `src/core/save_manager.h`
- `src/core/spi_arbiter.h`
- `src/core/box_art.h`
- `src/core/rom_index.h`
- `src/core/sd_card.h`
- `src/core/bmo_face.h`
-`src/core/battery.h`
- `src/core/profiler.h`
- `src/core/config.h`
- `src/engine/walnut_cgb/walnut_cgb.h`

Run: `git log --oneline -1 -- <file>` for each to check. If any show a
commit newer than the date above, grep live instead of trusting this table.

## Format
One table per file, columns: `Symbol | Kind (fn/macro/struct) | Signature | Notes`.

---

## src/core/display_emu.h (DisplayEmu)
| Symbol | Kind | Signature | Notes |
|---|---|---|---|
| `DisplayEmu::begin` | fn | `void begin()` | Initializes ST7789 display over SPI |
| `DisplayEmu::clearScreen` | fn | `void clearScreen()` | Clears screen to black |
| `DisplayEmu::showSDCardWarning` | fn | `void showSDCardWarning()` | Shows SD card error message |
| `DisplayEmu::initMenuUI` | fn | `void initMenuUI()` | Allocates menu buffer in PSRAM |
| `DisplayEmu::cleanupMenuUI` | fn | `void cleanupMenuUI()` | Frees menu buffer in PSRAM |
| `DisplayEmu::drawBootSplash` | fn | `void drawBootSplash(bool pressAnyButtonBlink)` | Renders Power-On Boot Splash (1:1 with Simulator) |
| `DisplayEmu::drawConsoleSelectMenu` | fn | `void drawConsoleSelectMenu(int selectedIndex, const int* gameCounts, int consoleCount, bool sdMounted)` | Renders 1:1 Carousel Card console selector |
| `DisplayEmu::drawConsoleMuseumModal` | fn | `void drawConsoleMuseumModal(RomType console)` | Renders console history/specs modal (STATE_CONSOLE_MUSEUM) |
| `DisplayEmu::drawGameSelectMenu` | fn | `void drawGameSelectMenu(const RomFile* const* games, int count, int selectedIndex, RomType console, bool sdMounted)` | Renders game list menu |
| `DisplayEmu::drawConsoleIcon` | fn | `void drawConsoleIcon(RomType type, int x, int y, uint16_t primaryColor = 0)` | Renders pixel-art console hardware silhouette |
| `DisplayEmu::drawIdleMascotScreen` | fn | `void drawIdleMascotScreen(unsigned long idleSeconds, const char* stateMessage)` | Renders Living BMO Screensaver (STATE_IDLE_MASCOT) |
| `DisplayEmu::setDmgPalette` | fn | `void setDmgPalette(int paletteIndex)` | Sets active DMG runtime palette |
| `DisplayEmu::getDmgPaletteIndex` | fn | `int getDmgPaletteIndex()` | Gets current DMG palette index |
| `DisplayEmu::cycleDmgPalette` | fn | `void cycleDmgPalette()` | Cycles through available DMG palettes |
| `DisplayEmu::getActiveDmgPalette` | fn | `const uint16_t* getActiveDmgPalette()` | Returns active DMG palette 4-color array |
| `DisplayEmu::startFrame` | fn | `void startFrame()` | Asserts CS & sets 240x216 window once |
| `DisplayEmu::endFrame` | fn | `void endFrame()` | Deasserts CS |
| `DisplayEmu::streamPixelRow` | fn | `void streamPixelRow(const uint16_t* buf, int pixelCount)` | Streams scanline without CS toggle |
| `DisplayEmu::streamNESFrame` | fn | `void streamNESFrame(const uint8_t* fb)` | 256x240 NES frame, scaled/cropped to 240x216 |
| `DisplayEmu::streamDoomFrame` | fn | `void streamDoomFrame(const uint8_t* cmap)` | 320x200 DOOM indexed frame |
| `DisplayEmu::streamSMSFrame` | fn | `void streamSMSFrame(const uint16_t* fb, bool isGameGear)` | 256x192 SMS or 160x144 GG frame |
| `DisplayEmu::streamPCEFrame` | fn | `void streamPCEFrame(const uint16_t* fb)` | 256x240 PC Engine frame |
| `DisplayEmu::streamAtariFrame` | fn | `void streamAtariFrame(const uint16_t* fb)` | 160x192 Atari 2600 frame |
| `DisplayEmu::streamPicoFrame` | fn | `void streamPicoFrame(const uint16_t* fb)` | 128x128 PICO-8 frame |
| `DisplayEmu::streamGenesisFrame` | fn | `void streamGenesisFrame(const uint16_t* fb, int w, int h)` | 320x224 Genesis frame, yOffset=8 |
| `DisplayEmu::streamSNESFrame` | fn | `void streamSNESFrame(const uint16_t* fb, int w, int h)` | 256x224 SNES frame, xOffset=32 |
| `DisplayEmu::streamWSwanFrame` | fn | `void streamWSwanFrame(const uint16_t* fb, int w, int h)` | 224x144 WonderSwan frame, centered |
| `DisplayEmu::streamNGPFrame` | fn | `void streamNGPFrame(const uint16_t* fb, int w, int h)` | 160x152 Neo Geo Pocket frame, centered |
| `DisplayEmu::streamLynxFrame` | fn | `void streamLynxFrame(const uint16_t* fb, int w, int h)` | 160x102 Atari Lynx frame, centered |
| `DisplayEmu::streamColemFrame` | fn | `void streamColemFrame(const uint16_t* fb, int w, int h)` | 256x192 ColecoVision frame, centered |
| `DisplayEmu::pushPixelsFullScreen` | fn | `void pushPixelsFullScreen(const uint16_t* buffer)` | Full 320x240 frame blit |
| `DisplayEmu::pushPixelsAt` | fn | `void pushPixelsAt(int x, int y, int w, int h, const uint16_t* buf)` | Arbitrary sub-rect blit |
| `DisplayEmu::startDirectWindow` | fn | `void startDirectWindow(int x, int y, int w, int h)` | Opens window & asserts SPI CS |
| `DisplayEmu::writeWindowBytes` | fn | `void writeWindowBytes(const uint8_t* data, size_t len)` | Streams raw bytes to open window |
| `DisplayEmu::endDirectWindow` | fn | `void endDirectWindow()` | Deasserts SPI CS |
| `DisplayEmu::streamGBFrame` | fn | `void streamGBFrame(const uint16_t* gb_framebuffer)` | Streams 240x216 Game Boy frame asynchronously via Core 0 |
| `DisplayEmu::streamRawFrameAsync` | fn | `void streamRawFrameAsync(const uint16_t* fb, int x, int y, int w, int h)` | Streams arbitrary 16-bit framebuffer asynchronously via Core 0 |
| `DisplayEmu::drawPauseMenu` | fn | `void drawPauseMenu(const char* romTitle, int currentSlot, bool hasSaveState, bool hasBatterySave, int selectedOption, const char* statusToast = nullptr)` | Renders In-Game Quick Pause & Save State Overlay Menu |
| `DisplayEmu::waitForDisplay` | fn | `void waitForDisplay()` | Blocks until Core 0 asynchronous display transfer completes |
| `DisplayEmu::isDisplayBusy` | fn | `bool isDisplayBusy()` | Returns true if Core 0 display worker is currently streaming |
| `DisplayEmu::CLASSIC_PALETTE` | const | `const uint16_t CLASSIC_PALETTE[4]` | Pre-swapped BGR565 green palette |
| `DisplayEmu::NES_PALETTE` | const | `const uint16_t NES_PALETTE[64]` | Pre-swapped BGR565 NES palette |

---

## src/core/save_manager.h (SaveManager)
| Symbol | Kind | Signature | Notes |
|---|---|---|---|
| `SaveManager::begin` | fn | `bool begin()` | Initializes `/saves` directory on MicroSD card |
| `SaveManager::getActiveSlot` | fn | `int getActiveSlot()` | Returns currently selected save slot (1–5) |
| `SaveManager::setActiveSlot` | fn | `void setActiveSlot(int slot)` | Sets active save slot (1–5) |
| `SaveManager::cycleActiveSlot` | fn | `void cycleActiveSlot()` | Cycles active slot to next (1..5) |
| `SaveManager::saveBatteryRam` | fn | `bool saveBatteryRam(const char* romFilename, const uint8_t* ram, size_t ramSize)` | Flushes raw battery RAM to `/saves/<rom>.sav` |
| `SaveManager::loadBatteryRam` | fn | `bool loadBatteryRam(const char* romFilename, uint8_t* ram, size_t maxRamSize, size_t* outLoadedSize = nullptr)` | Loads battery RAM from `/saves/<rom>.sav` |
| `SaveManager::hasBatteryRam` | fn | `bool hasBatteryRam(const char* romFilename)` | Checks if `.sav` file exists on SD card |
| `SaveManager::saveState` | fn | `bool saveState(const char* romFilename, int slot, uint32_t coreId, const void* stateData, size_t stateSize, const uint8_t* ramData = nullptr, size_t ramSize = 0)` | Writes snapshot with CRC32 to `/saves/<rom>.s0<slot>` |
| `SaveManager::loadState` | fn | `bool loadState(const char* romFilename, int slot, uint32_t expectedCoreId, void* stateData, size_t maxStateSize, size_t* outStateSize = nullptr, uint8_t* ramData = nullptr, size_t maxRamSize = 0, size_t* outRamSize = nullptr)` | Validates and restores snapshot from `/saves/<rom>.s0<slot>` |
| `SaveManager::hasSaveState` | fn | `bool hasSaveState(const char* romFilename, int slot)` | Checks if save state exists for given slot |
| `SaveManager::computeCRC32` | fn | `uint32_t computeCRC32(const uint8_t* data, size_t length, uint32_t crc = 0xFFFFFFFF)` | Fast IEEE 802.3 bitwise CRC32 calculation |
| `SaveStateHeader` | struct | `struct SaveStateHeader { uint32_t magic, version, timestamp, coreId, stateSize, ramSize, crc32; char romTitle[32]; }` | Binary save state header descriptor |

---

## src/core/spi_arbiter.h (SpiArbiter)
| Symbol | Kind | Signature | Notes |
|---|---|---|---|
| `SpiArbiter::init` | fn | `void init()` | Initializes recursive FreeRTOS SPI bus mutex |
| `SpiArbiter::lock` | fn | `bool lock(TickType_t waitTicks = portMAX_DELAY)` | Acquires exclusive lock on shared FSPI bus |
| `SpiArbiter::unlock` | fn | `void unlock()` | Releases exclusive lock on shared FSPI bus |

---

## src/core/buttons.h (Buttons)
| Symbol | Kind | Signature | Notes |
|---|---|---|---|
| `Buttons::begin` | fn | `void begin()` | Configures button GPIOs as INPUT_PULLUP |
| `Buttons::update` | fn | `void update()` | Reads GPIO_IN_REG atomically, updates state |
| `Buttons::count` | fn | `int count()` | Returns number of buttons (8) |
| `Buttons::get` | fn | `const ButtonState& get(int index)` | Returns ButtonState struct |
| `Buttons::gb_joypad_state` | var | `uint8_t gb_joypad_state` | Active-low joypad bitmask (0=pressed) |
| `Buttons::Index` | enum | `enum Index : int { UP, DOWN, LEFT, RIGHT, A, B, START, SELECT }` | Button indices |
| `ButtonState` | struct | `struct ButtonState { const char* name; uint8_t pin; bool pressed; bool changed; }` | Button state info |

---

## src/core/sd_card.h (SDCard)
| Symbol | Kind | Signature | Notes |
|---|---|---|---|
| `SDCard::begin` | fn | `bool begin()` | Mounts SD & registers baked flash ROMs |
| `SDCard::isMounted` | fn | `bool isMounted()` | Returns true if SD card is mounted |
| `SDCard::scanRoms` | fn | `void scanRoms()` | Populates romList from SD card |
| `SDCard::getRomCount` | fn | `int getRomCount()` | Returns total ROM count (baked + SD) |
| `SDCard::getRomCountForType` | fn | `int getRomCountForType(RomType type)` | Returns cached ROM count for specific console (O(1)) |
| `SDCard::getRomInfo` | fn | `const RomFile* getRomInfo(int index)` | Returns RomFile pointer |
| `SDCard::isFavorite` | fn | `bool isFavorite(int index)` | Returns true if game is in Favorites |
| `SDCard::toggleFavorite` | fn | `void toggleFavorite(int index)` | Toggles favorite status & persists to SD |
| `SDCard::getFavoritesCount` | fn | `int getFavoritesCount()` | Returns total count of starred favorite games |
| `SDCard::saveFavorites` | fn | `void saveFavorites()` | Writes starred titles to /favorites.txt |
| `SDCard::loadFavorites` | fn | `void loadFavorites()` | Reads starred titles from /favorites.txt |
| `SDCard::loadIndex` | fn | `bool loadIndex()` | Fast-loads binary cached ROM list from /.bmo_index (<15ms) |
| `SDCard::saveIndex` | fn | `bool saveIndex()` | Writes binary cached ROM list to /.bmo_index |
| `SDCard::rebuildIndex` | fn | `void rebuildIndex()` | Deletes cached index and forces full directory rescanning |
| `SDCard::sortRoms` | fn | `void sortRoms()` | Sorts scanned ROM entries alphabetically case-insensitively |
| `SDCard::loadRom` | fn | `uint8_t* loadRom(const char* filename, size_t* outSize)` | Loads ROM to PSRAM (or returns .rodata) |
| `SDCard::freeRom` | fn | `void freeRom(uint8_t* buffer)` | Frees PSRAM ROM (safe for .rodata) |
| `RomType` | enum | `enum RomType { ROM_FAVORITES, ROM_GB, ROM_GBC, ROM_NES, ... }` | 16-platform console enum |
| `RomFile` | struct | `struct RomFile { char filename[64]; RomType type; bool isFavorite; bool hasBoxArt; }` | ROM entry info with favorite and box art status |

---

## src/core/box_art.h (BoxArt)
| Symbol | Kind | Signature | Notes |
|---|---|---|---|
| `BoxArt::init` | fn | `bool init()` | Allocates 8KB buffer in PSRAM for 64x64 pixel data |
| `BoxArt::load` | fn | `bool load(const char* romFilename)` | Loads 64x64 raw/BMP cover art from /boxart/ or /covers/ |
| `BoxArt::unload` | fn | `void unload()` | Clears cached box art buffer and marks inactive |
| `BoxArt::hasArt` | fn | `bool hasArt()` | Returns true if valid cover art is currently loaded in PSRAM |
| `BoxArt::getBuffer` | fn | `const uint16_t* getBuffer()` | Returns pointer to 64x64 pixel buffer |
| `BoxArt::draw` | fn | `void draw(GFXcanvas16* canvas, int x, int y, bool drawBorder = true, uint16_t borderColor = 0xFFFF)` | Blits 64x64 art into canvas with optional rounded border |
| `BoxArt::existsForRom` | fn | `bool existsForRom(const char* romFilename)` | Checks if raw or BMP box art exists on SD card |
| `BoxArt::getBaseName` | fn | `void getBaseName(const char* romFilename, char* outBaseName, size_t maxLen)` | Strips directory path and file extension |

---

## src/core/rom_index.h (RomIndex)
| Symbol | Kind | Signature | Notes |
|---|---|---|---|
| `BMO_INDEX_MAGIC` | macro | `"#define BMO_INDEX_MAGIC \"BMOIDX01\""` | 8-byte magic header signature |
| `BMO_INDEX_VERSION` | macro | `"#define BMO_INDEX_VERSION 1"` | Version number (1) |
| `BMO_INDEX_PATH` | macro | `"#define BMO_INDEX_PATH \"/.bmo_index\""` | MicroSD path for binary ROM index cache |
| `BmoIndexHeader` | struct | `struct BmoIndexHeader { char magic[8]; uint32_t version, entryCount, entrySize, crc32, timestamp, reserved[2]; }` | 36-byte packed index file header |
| `BmoIndexEntry` | struct | `struct BmoIndexEntry { char filename[64]; uint8_t type, isFavorite, hasBoxArt, reserved; uint32_t fileSize; }` | 72-byte packed binary ROM entry record |

---

## src/core/theme.h (Theme)
| Symbol | Kind | Signature | Notes |
|---|---|---|---|
| `Theme::BMO_SCREEN_MINT` | const | `constexpr uint16_t BMO_SCREEN_MINT` | Authentic OnionUI Screen Mint (#CEF5E4) |
| `Theme::BMO_BODY_TEAL` | const | `constexpr uint16_t BMO_BODY_TEAL` | Authentic OnionUI Body Teal (#5FB49C) |
| `Theme::BMO_DEEP_TEAL` | const | `constexpr uint16_t BMO_DEEP_TEAL` | Deep Pine Teal (#1A4B42) |
| `Theme::BMO_DPAD_YELLOW` | const | `constexpr uint16_t BMO_DPAD_YELLOW` | Authentic D-Pad Yellow (#FFE033) |
| `Theme::BMO_CORAL_RED` | const | `constexpr uint16_t BMO_CORAL_RED` | BMO Coral Red (#E8175D) |
| `Theme::PALETTE_BMO` | const | `constexpr uint16_t PALETTE_BMO[4]` | 4-color BMO Mint/Teal runtime DMG palette |
| `Theme::PALETTE_COUNT` | const | `constexpr int PALETTE_COUNT = 5` | Number of selectable DMG color palettes |

---

## src/core/bmo_face.h (BmoFace)
| Symbol | Kind | Signature | Notes |
|---|---|---|---|
| `BmoFace::begin` | fn | `void begin()` | Initializes RNG and clears dirty flag |
| `BmoFace::setExpression` | fn | `void setExpression(BmoExpression expr)` | Sets target facial expression |
| `BmoFace::update` | fn | `void update()` | Ticks procedural animation & blink |
| `BmoFace::draw` | fn | `void draw(int x, int y, int size)` | Renders & blits scaled face |
| `BmoFace::draw` | fn | `void draw()` | Blits large centered face |
| `BmoFace::isDirty` | fn | `bool isDirty()` | True if expression or blink changed |
| `BmoFace::BmoExpression`| enum | `enum BmoExpression { IDLE, SURPRISED, HAPPY, SLEEPY, LOW_BATTERY, CHARGING, ERROR, SHUTDOWN, HIDDEN }` | Face emotion states |

---

## src/core/battery.h (Battery)
| Symbol | Kind | Signature | Notes |
|---|---|---|---|
| `Battery::begin` | fn | `void begin()` | Initializes ADC pin (dormant if flag=0) |
| `Battery::getVoltage` | fn | `float getVoltage()` | Returns smoothed voltage (0.0 - 4.2V) |
| `Battery::getPercentage`| fn | `int getPercentage()` | Returns percentage (0 - 100) |
| `Battery::update` | fn | `void update()` | Updates moving average; checks low-cutoff |
| `Battery::safeShutdown` | fn | `void safeShutdown()` | Shows low battery face & triggers deep sleep |

---

## src/emulators/ Public APIs
| Symbol | Kind | Signature | Notes |
|---|---|---|---|
| `PeanutEmu::begin` | fn | `bool begin(const uint8_t* rom_data, size_t rom_len)` | Starts Game Boy DMG core |
| `PeanutEmu::updateJoypad` | fn | `void updateJoypad()` | Syncs direct.joypad with buttons |
| `PeanutEmu::runFrame` | fn | `void runFrame()` | Runs 1 frame inside startFrame/endFrame |
| `PeanutEmu::destroy` | fn | `void destroy()` | Frees cart_ram in PSRAM |
| `WalnutEmu::begin` | fn | `bool begin(const uint8_t* rom_data, size_t rom_len)` | Starts Game Boy Color core |
| `WalnutEmu::updateJoypad` | fn | `void updateJoypad()` | Syncs direct.joypad with buttons |
| `WalnutEmu::runFrame` | fn | `void runFrame()` | Runs 1 frame inside startFrame/endFrame |
| `WalnutEmu::destroy` | fn | `void destroy()` | Frees cart_ram in PSRAM |
| `NesEmu::begin` | fn | `static bool begin(const uint8_t* romData, size_t romSize)` | Starts Agnes NES core |
| `NesEmu::updateJoypad` | fn | `static void updateJoypad()` | Syncs Agnes controller mask |
| `NesEmu::runFrame` | fn | `static void runFrame()` | Runs 1 NES frame |
| `NesEmu::destroy` | fn | `static void destroy()` | Shuts down Agnes NES core |
| `DoomEmu::begin` | fn | `bool begin(const char* wadPath)` | Starts doomgeneric core |
| `DoomEmu::runFrame` | fn | `void runFrame()` | Runs 1 DOOM engine tick |
| `DoomEmu::destroy` | fn | `void destroy()` | Shuts down doomgeneric core |

---

## src/engine/walnut_cgb/walnut_cgb.h
| Symbol | Kind | Signature | Notes |
|---|---|---|---|
| `gb_init` | fn | `enum gb_init_error_e gb_init(struct gb_s* gb, uint8_t(*gb_rom_read)(struct gb_s*, const uint_fast32_t), uint16_t(*gb_rom_read16)(struct gb_s*, const uint_fast32_t), uint32_t(*gb_rom_read32)(struct gb_s*, const uint_fast32_t), uint8_t(*gb_cart_ram_read)(struct gb_s*, const uint_fast32_t), void (*gb_cart_ram_write)(struct gb_s*, const uint_fast32_t, const uint8_t), void (*gb_error)(struct gb_s*, const enum gb_error_e, const uint16_t), void* priv)` | 8 arguments including 16/32 read callbacks |
| `gb_run_frame` | fn | `void gb_run_frame(struct gb_s *gb)` | Executes one frame via `__gb_step_cpu_x` |
| `gb_run_frame_dualfetch` | fn | `void gb_run_frame_dualfetch(struct gb_s *gb)` | Executes one frame via `__gb_step_cpu` (dual-fetch path) |
| `gb_init_lcd` | fn | `void gb_init_lcd(struct gb_s *gb, void(*lcd_draw_line)(...))` | Registers scanline callback |
| `gb_s` | struct | `struct gb_s` | Core emulator context, align(32) |
| `gb_init_error_e` | enum | `enum gb_init_error_e` | Initialization error codes |

---

## src/core/config.h
| Symbol | Kind | Signature | Notes |
|---|---|---|---|
| `FEATURE_SD_CARD` | macro | `#define FEATURE_SD_CARD 1` | Gates SD logic |
| `FEATURE_BATTERY_MONITOR` | macro | `#define FEATURE_BATTERY_MONITOR 0` | Dormant battery logic |
| `FEATURE_AUDIO` | macro | `#define FEATURE_AUDIO 0` | Dormant I2S audio logic |
| `BTN_UP`... | macro | `#define BTN_UP 4` (etc) | GPIO pin assignments |
| `LOG_INFO`, `LOG_ERROR`... | macro | `#define LOG_INFO(fmt, ...)` | Gated logging macros |

---

## src/core/profiler.h (BmoProfiler)
| Symbol | Kind | Signature | Notes |
|---|---|---|---|
| `BMO_PROFILE_BEGIN` | macro | `BMO_PROFILE_BEGIN()` | Gated profiler initialization |
| `BMO_PROFILE_START` | macro | `BMO_PROFILE_START(zone)` | Records start cycle timestamp (RSR CCOUNT) |
| `BMO_PROFILE_END` | macro | `BMO_PROFILE_END(zone)` | Records end cycle timestamp and updates stats |
| `BMO_PROFILE_REPORT_INTERVAL` | macro | `BMO_PROFILE_REPORT_INTERVAL(ms)` | Prints zone averages over time interval |
| `BMO_PROFILE_RESET` | macro | `BMO_PROFILE_RESET()` | Clears profiler sample accumulator |
| `BmoProfiler::begin` | fn | `static void begin()` | Profiler initialization |
| `BmoProfiler::startZone` | fn | `static void startZone(ProfileZone zone)` | Starts zone timing |
| `BmoProfiler::endZone` | fn | `static void endZone(ProfileZone zone)` | Ends zone timing |

