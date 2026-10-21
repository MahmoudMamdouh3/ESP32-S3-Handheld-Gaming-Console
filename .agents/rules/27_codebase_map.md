# Codebase Map — Ground Truth for Architecture
Purpose: An agent starting a session can read this file to understand the directory layout, runtime state machine, 14-core emulator routing, flash partition table, and SPI/PSRAM architecture before touching code.

Last updated: 2026-10-21

---

## Directory tree (verified, current)
```text
repo-root/
├── AGENTS.md                      <- Entry point for AI coding agents (Ruleset v8)
├── README.md                      <- Project overview, pinout, and build instructions
├── CHANGELOG.md                   <- Chronological version history
├── AGENT_MANIFEST.json            <- Auto-generated hardware & 14-core build manifest
├── AGENT_DECISION_TREE.json       <- Auto-generated task intent router
├── AGENT_KNOWLEDGE_GRAPH.json     <- Auto-generated AST, symbol, and pin graph
├── .agents/rules/                 <- Modular agent ruleset + CONTEXT_INDEX.json
├── docs/
│   ├── software-design-document.md <- Architectural specification
│   ├── performance_report.md       <- Guardian performance & memory report
│   └── hardware-notes.md           <- Board wiring & electrical notes
├── firmware/BmoGameboy/
│   ├── BmoGameboy.ino             <- Main state machine, loop(), pause menu, frame pacing
│   ├── partitions.csv             <- Custom 16MB flash partition table (8MB app0)
│   └── src/
│       ├── core/                  <- Hardware drivers & system services
│       │   ├── config.h           <- Single source of truth for all GPIO pins + FEATURE flags
│       │   ├── display_emu.cpp/h  <- ST7789 80MHz SPI driver, Core 0 worker, bezel & UI renderers
│       │   ├── buttons.cpp/h      <- Atomic GPIO_IN_REG polling, A/B remap, auto-fire turbo
│       │   ├── sd_card.cpp/h      <- Recursive SD scanner, .bmo_index cache, favorites, history
│       │   ├── bmo_face.cpp/h     <- Procedural 2D SDF mascot renderer (320x240 PSRAM)
│       │   ├── spi_arbiter.cpp/h  <- FreeRTOS FSPI bus mutex arbiter
│       │   ├── save_manager.cpp/h <- 5-slot save states, .sav battery RAM, 24-bit .bmp screenshots
│       │   ├── box_art.cpp/h      <- 64x64 .raw/.bmp cover art loader in PSRAM
│       │   ├── rom_index.h        <- Binary ROM index cache (.bmo_index) structures
│       │   ├── battery.cpp/h      <- DORMANT (FEATURE_BATTERY_MONITOR=0)
│       │   ├── audio_i2s.cpp/h    <- DORMANT (FEATURE_AUDIO=0)
│       │   └── fram_save.cpp/h    <- DORMANT (optional I2C FRAM)
│       ├── emulators/             <- 14 thin C++ glue wrappers (one per console)
│       │   ├── emu_walnut.cpp/h   <- Game Boy Color (.gbc)
│       │   ├── emu_peanut.cpp/h   <- Game Boy DMG (.gb)
│       │   ├── emu_nes.cpp/h      <- NES (.nes)
│       │   ├── emu_doom.cpp/h     <- Classic DOOM (.wad)
│       │   ├── emu_sms.cpp/h      <- Sega Master System / Game Gear (.sms, .gg)
│       │   ├── emu_pce.cpp/h      <- PC Engine / TurboGrafx-16 (.pce)
│       │   ├── emu_atari.cpp/h    <- Atari 2600 (.a26)
│       │   ├── emu_pico.cpp/h     <- PICO-8 (.p8)
│       │   ├── emu_genesis.cpp/h  <- Sega Genesis / Mega Drive (.gen, .md, .smd)
│       │   ├── emu_snes.cpp/h     <- Super Nintendo (.sfc, .smc)
│       │   ├── emu_wswan.cpp/h    <- Bandai WonderSwan / Color (.ws, .wsc)
│       │   ├── emu_ngp.cpp/h      <- Neo Geo Pocket / Color (.ngp, .ngc)
│       │   ├── emu_lynx.cpp/h     <- Atari Lynx (.lnx)
│       │   └── emu_colem.cpp/h    <- ColecoVision (.col, .sg)
│       ├── engine/walnut_cgb/     <- Header-only GBC engine (walnut_cgb.h)
│       ├── vendor/                <- 13 C emulator engines (peanut_gb, agnes, doom, smsplus, pce, stella, pico, genesis, snes, wswan, ngp, lynx, colem)
│       ├── assets/                <- Baked flash ROM C headers (mario_deluxe.h, zelda_ages.h, aladdin.h, lego_racers.h, virtual_bmo.h)
│       └── tests/unit_tests.cpp   <- On-device test suite
├── tools/
│   ├── host_cores_runner.c        <- Native x64 10-core 50-frame execution & FNV-1a harness
│   ├── host_test.cpp              <- Native x64 Walnut-CGB Blargg CPU test harness
│   ├── telemetry_plotter.py       <- Serial frame-time & jitter analyzer
│   ├── bmo_simulator/             <- Browser 320x240 simulator + WebSerial SD manager
│   └── guardian/                  <- AI Guardian CLI (audit, verify, doctor, prompt-pack, diff-audit, route, lookup)
├── scripts/
│   ├── validate_repo.py           <- 8-phase CI build & doc-truth validator
│   ├── generate_ai_knowledge_base.py <- Knowledge graph & decision tree compiler
│   ├── fetch_covers.py            <- 64x64 BGR565 .raw/.bmp cover generator
│   ├── convert_roms.py            <- ROM binary to .rodata C header converter
│   └── test_runner.py             <- Baked ROM header checksum verifier
└── tests/                         <- Python & native x64 C unit test suite (120+ tests)
```

---

## Runtime State Machine (`BmoGameboy.ino`)
Six states govern runtime control flow:
- `STATE_CONSOLE_MENU` — Console carousel selector (14 systems + Konami Code easter egg). `A` enters `STATE_GAME_MENU`; `SELECT` enters `STATE_CONSOLE_SPECS`; `UP` enters `STATE_IDLE_MASCOT`.
- `STATE_CONSOLE_SPECS` — Displays hardware history & specs for the selected console.
- `STATE_GAME_MENU` — ROM browser with 64×64 box art, recursive folders, A–Z jump, and favorites (`SELECT`). `A` launches game into `STATE_EMULATOR`.
- `STATE_EMULATOR` — Active emulator frame loop (`runFrame()`). Holding `START + SELECT` opens `STATE_PAUSE_MENU`.
- `STATE_PAUSE_MENU` — In-game overlay: Resume, Save State (Slots 0–4), Load State (Slots 0–4), Exit to Menu (auto-saves `.sav`, calls `teardownCurrentEmulator()`, frees ROM), plus `LEFT` (A/B swap), `RIGHT` (1:1 console bezel toggle), and `SELECT` (`.bmp` screenshot capture).
- `STATE_IDLE_MASCOT` — Interactive 320×240 BMO companion mode (expressions, dialogue bubbles, dance mode).

---

## 14-Core Emulator Routing (`sd_card.h` -> `BmoGameboy.ino`)
| Extension(s) | `RomType` | `selectedEmulatorIndex` | Wrapper | Native Res |
|---|---|---|---|---|
| `.gbc` | `ROM_GBC` | `0` | `WalnutEmu` (`emu_walnut`) | 160×144 (scaled 240×216) |
| `.gb` | `ROM_GB` | `1` | `PeanutEmu` (`emu_peanut`) | 160×144 (scaled 240×216) |
| `.nes` | `ROM_NES` | `2` | `NesEmu` (`emu_nes`) | 256×240 |
| `.wad` | `ROM_WAD` | `3` | `DoomEmu` (`emu_doom`) | 320×200 |
| `.sms`, `.gg` | `ROM_SMS`, `ROM_GG` | `4`, `5` | `SmsEmu` (`emu_sms`) | 256×192 / 160×144 |
| `.pce` | `ROM_PCE` | `6` | `PceEmu` (`emu_pce`) | 256×240 |
| `.a26` | `ROM_ATARI` | `7` | `AtariEmu` (`emu_atari`) | 160×192 |
| `.p8` | `ROM_PICO8` | `8` | `PicoEmu` (`emu_pico`) | 128×128 |
| `.gen`, `.md`, `.smd` | `ROM_GENESIS` | `9` | `GenesisEmu` (`emu_genesis`) | 320×224 |
| `.sfc`, `.smc` | `ROM_SNES` | `10` | `SNESEmu` (`emu_snes`) | 256×224 |
| `.ws`, `.wsc` | `ROM_WSWAN` | `11` | `WSwanEmu` (`emu_wswan`) | 224×144 |
| `.ngp`, `.ngc` | `ROM_NGP` | `12` | `NGPEmu` (`emu_ngp`) | 160×152 |
| `.lnx` | `ROM_LYNX` | `13` | `LynxEmu` (`emu_lynx`) | 160×102 |
| `.col`, `.sg` | `ROM_COLEM` | `14` | `ColemEmu` (`emu_colem`) | 256×192 |

---

## Flash Partition Layout (`partitions.csv`)
| Name | Type | Offset | Size | Purpose |
|---|---|---|---|---|
| `nvs` | `data/nvs` | `0x9000` | 20KB | NVS storage |
| `otadata` | `data/ota` | `0xE000` | 8KB | OTA metadata |
| `app0` | `app/ota_0` | `0x10000` | **8MB** | Firmware + 5 baked ROMs (`5.69 MB` used, `33.9%` of 16MB flash) |
| `ffat` | `data/fat` | `0x810000` | ~7.9MB | FatFS partition |

---

## SPI Bus Sharing & Display Format
- Shared FSPI bus: `TFT_SCK` (`12`), `TFT_MOSI` (`11`), `TFT_CS` (`10`), `TFT_DC` (`8`), `TFT_RST` (`9`), `SD_CS` (`13`), `SD_MISO` (`15`).
- All SPI transactions on Core 0 (`vDisplayTask`) and Core 1 (`SDCard`, `SaveManager`, `BoxArt`) are serialized via `SpiArbiter::lock()`.
- Display wire pixel format is **BGR565 byte-swapped** (`swapBytes()`).

---

## Buttons Joypad Bitmask (`Buttons::gb_joypad_state`)
Active-low (`0` = pressed, `1` = released), sampled atomically once per frame via `REG_READ(GPIO_IN_REG)`:
- Bit 0: `BTN_A` (`16`), Bit 1: `BTN_B` (`17`), Bit 2: `BTN_SELECT` (`21`), Bit 3: `BTN_START` (`18`), Bit 4: `BTN_RIGHT` (`7`), Bit 5: `BTN_LEFT` (`6`), Bit 6: `BTN_UP` (`4`), Bit 7: `BTN_DOWN` (`5`).
