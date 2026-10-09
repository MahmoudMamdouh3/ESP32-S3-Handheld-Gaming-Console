# Codebase Map — Ground Truth for Architecture
Purpose: an agent starting a new session should be able to read this ONE
file and understand the whole system before touching anything. Updated
whenever structure changes.

Last updated: 2026-10-09 (Antigravity)

---

## Directory tree (verified, current)
```
repo-root/
├── AGENTS.md                      <- entry point, points here (Ruleset v4)
├── README.md                      <- user-facing project summary
├── AGENT_MANIFEST.json            <- machine-readable hardware & build manifest
├── IMPLEMENTATION_PLAN.md         <- historical plan, status=COMPLETE, safe to ignore
├── partitions.csv                 <- WARNING: see Flash Partitions below
├── .agents/rules/                 <- agent ruleset (34 rules + CONTEXT_INDEX.json)
├── docs/
│   ├── software-design-document.md  <- Living architectural specification (v3.0 Ground Truth)
│   └── hardware-notes.md
├── firmware/BmoGameboy/
│   ├── BmoGameboy.ino             <- ONLY setup(), loop(), state machine
│   ├── partitions.csv             <- custom partition table (8MB app0)
│   └── src/
│       ├── core/                  <- hardware drivers, always compiled
│       │   ├── config.h           <- single source of truth for all pins + FEATURE flags
│       │   ├── display_emu.cpp/h  <- ST7789 SPI driver + all render APIs
│       │   ├── buttons.cpp/h      <- GPIO polling, joypad bitmask
│       │   ├── sd_card.cpp/h      <- SD + baked ROM registration
│       │   ├── bmo_face.cpp/h     <- procedural SDF mascot renderer
│       │   ├── spi_arbiter.cpp/h  <- FreeRTOS FSPI bus mutex arbiter
│       │   ├── save_manager.cpp/h <- Multi-slot save states & battery RAM
│       │   ├── box_art.cpp/h      <- 64x64 raw/BMP cover art streaming engine
│       │   ├── rom_index.h        <- Binary ROM index cache (.bmo_index)
│       │   ├── battery.cpp/h      <- DORMANT (FEATURE_BATTERY_MONITOR=0)
│       │   ├── audio_i2s.cpp/h    <- DORMANT (FEATURE_AUDIO=0)
│       │   └── fram_save.cpp/h    <- DORMANT (not wired, no FEATURE flag yet)
│       ├── emulators/             <- thin glue wrappers, one per console
│       │   ├── emu_peanut.cpp/h   <- GB (.gb)   -> peanut_gb engine
│       │   ├── emu_walnut.cpp/h   <- GBC (.gbc) -> walnut_cgb engine
│       │   ├── emu_nes.cpp/h      <- NES (.nes)  -> agnes engine
│       │   └── emu_doom.cpp/h     <- DOOM (.wad) -> doomgeneric engine
│       ├── engine/
│       │   └── walnut_cgb/
│       │       └── walnut_cgb.h   <- GBC engine (9937 lines, header-only)
│       ├── vendor/
│       │   ├── peanut_gb/         <- peanut_gb.h + peanut_gb_config.h
│       │   ├── agnes/             <- agnes.h + agnes.c (NES)
│       │   └── doom/              <- doomgeneric (DOOM)
│       ├── assets/
│       │   ├── bmo_assets.h       <- extern declarations for face bitmaps
│       │   ├── rom_data.h         <- legacy single-ROM header (tobu_tobu_girl), UNUSED in live build
│       │   └── roms/
│       │       ├── mario_deluxe.h   <- 1MB GBC ROM baked as C array
│       │       ├── zelda_ages.h     <- 1MB GBC ROM baked as C array
│       │       ├── aladdin.h        <- 1MB GBC ROM baked as C array
│       │       └── lego_racers.h    <- 1MB GBC ROM baked as C array
│       └── tests/
│           └── unit_tests.cpp     <- on-device test suite (requires ENABLE_UNIT_TESTS)
├── tools/
│   ├── host_test.cpp              <- desktop CPU-correctness harness (Zig compiler)
│   └── convert.py                 <- ROM-to-C-header converter
└── scripts/
    ├── process_games.py           <- ROM validation + C header generation
    ├── validate_repo.py           <- Python syntax check + ROM integrity
    └── (others)                   <- benchmark, color calc, fetch covers
```

---

## The state machine (BmoGameboy.ino)
Three states. THIS is the entire runtime control flow:
```
STATE_CONSOLE_MENU  -> user picks console (GB/GBC/NES/DOOM)
      |
      | A button
      v
STATE_GAME_MENU     -> user picks ROM file
      |
      | A button
      v
STATE_EMULATOR      -> one emulator runs, SELECT+UP exits back to CONSOLE_MENU
```
**KEY FACT:** `BmoFace::update()` and `Battery::update()` run every loop() tick
regardless of state. Emulator cores do NOT call them — only menu states do.

---

## Emulator routing (sd_card.h RomType enum -> BmoGameboy.ino)
| Extension | RomType | selectedEmulatorIndex | Engine | destroy() called on exit? |
|---|---|---|---|---|
| `.gb`  | ROM_GB  | 1 | PeanutEmu (peanut_gb.h) | YES (wired in BmoGameboy.ino) |
| `.gbc` | ROM_GBC | 0 | WalnutEmu (walnut_cgb.h) | YES (wired in BmoGameboy.ino) |
| `.nes` | ROM_NES | 2 | NesEmu (agnes) | YES |
| `.wad` | ROM_WAD | 3 | DoomEmu (doomgeneric) | YES (no-op) |

DOOM does NOT load ROM into PSRAM — it reads WAD directly from SD via fopen().
All others load full ROM into PSRAM via `SDCard::loadRom()`.

---

## Baked ROMs (always available, no SD card needed)
Registered in `SDCard::begin()` and `SDCard::loadRom()` in sd_card.cpp.
| Name in romList | Header | Size | Registered? |
|---|---|---|---|
| Super Mario Bros Deluxe (Baked).gbc | mario_deluxe.h | 1MB | YES |
| Legend of Zelda Ages (Baked).gbc | zelda_ages.h | 1MB | YES |
| Aladdin (Baked).gbc | aladdin.h | 1MB | YES |
| Lego Racers (Baked).gbc | lego_racers.h | 1MB | YES |

Note: Header files are ~6MB of formatted C text on disk, but each compiles
to exactly 1,048,576 bytes (1MB) in flash .rodata. All 4 baked ROMs consume
~4.98MB total firmware binary space, well within the 8MB app0 partition limit.

---

## Flash partition layout (partitions.csv)
| Name | Type | Offset | Size | Purpose |
|---|---|---|---|---|
| nvs | data/nvs | 0x9000 | 20KB | NVS storage |
| otadata | data/ota | 0xE000 | 8KB | OTA metadata |
| app0 | app/ota_0 | 0x10000 | **8MB** | Firmware + baked ROMs |
| ffat | data/fat | 0x810000 | ~7.9MB | FatFS (unused currently) |

The 8MB app0 is NON-STANDARD. The standard Arduino ESP32 partition schemes
use 1.3MB or 1.9MB for app. This custom partition is required because the
baked ROM headers (mario, zelda) alone consume ~10MB of raw C source which
compiles down to ~2MB of .rodata in flash. If the custom partitions.csv is
lost or overwritten, the firmware will not fit and will fail to flash.

---

## SPI bus sharing (critical — do not add new SPI devices without reading this)
The TFT display and SD card SHARE the SPI bus:
- SCK: GPIO12, MOSI: GPIO11 (shared)
- TFT CS: GPIO10, SD CS: GPIO13, MISO: GPIO15 (SD only)
- Bus initialized once in setup() via `SPI.begin(TFT_SCK, SD_MISO, TFT_MOSI, -1)`
- SD card uses a slower 4MHz init transaction; display runs at 80MHz during frames
- startFrame()/endFrame() hold the SPI bus open for an entire 144-scanline frame
  -- NO SD card reads can happen during an active frame render
- DOOM is the only engine that calls fread() during gameplay; it holds the SPI bus
  between DG_DrawFrame() calls; this is why DOOM latency spikes appear in the log

---

## Display coordinate system
- Physical display: 320px wide × 240px tall (landscape, rotation=3)
- Game Boy viewport: 240×216, centered at OFFSET_X=40, OFFSET_Y=12
- NES viewport: 256×240, centered at x=32 (full height)
- DOOM viewport: 320×200, y-offset=20 (20px letterbox top/bottom)
- Full-screen blit (BMO face, menus): 0,0 to 320,240
- Pixel format ON THE WIRE: BGR565 byte-swapped (big-endian on wire)
  -- `SPI.writeBytes()` sends LSB first; ST7789 expects MSB first per byte
  -- All emulator palettes must be pre-swapped; use the swapBytes() helper

---

## Buttons joypad bitmask (gb_joypad_state)
Active-low: 0=pressed, 1=released (same as Game Boy hardware).
| Bit | Button | Config pin |
|---|---|---|
| 0 | A | GPIO16 |
| 1 | B | GPIO17 |
| 2 | SELECT | GPIO21 |
| 3 | START | GPIO18 |
| 4 | RIGHT | GPIO7 |
| 5 | LEFT | GPIO6 |
| 6 | UP | GPIO4 |
| 7 | DOWN | GPIO5 |

Buttons::update() reads ALL button GPIO with a single REG_READ(GPIO_IN_REG)
call (all button pins are < GPIO32). Do not add buttons on GPIO >= 32 without
changing the read logic.

---

## PSRAM allocation map (current, approximate worst case)
| Allocator | Size | Notes |
|---|---|---|
| WalnutEmu cart_ram | 128KB | lazy-allocated on first begin(), freed on destroy() |
| PeanutEmu cart_ram | 128KB | same pattern |
| DisplayEmu menuCanvas | 320×240×2 = 150KB | freed on cleanupMenuUI() |
| SDCard ROM buffer | up to 4MB | freed on SDCard::freeRom() |
| DOOM heap | large | internally managed by doomgeneric |
| Total worst case | ~4.5MB+ | 8MB ceiling; fragmentation possible |

---

## Known dormant/aspirational features (do NOT enable without physical hardware)
| Feature | Flag | Why dormant |
|---|---|---|
| Battery monitor | FEATURE_BATTERY_MONITOR=0 | No voltage divider soldered |
| I2S audio | FEATURE_AUDIO=0 | No MAX98357A DAC wired |
| FRAM save | (no flag yet) | No I2C FRAM wired |
