# BMO-Handheld-Gaming-Console

A multi-platform retro gaming handheld console powered by the ESP32-S3 microcontroller, featuring a custom animated 2D Signed Distance Field (SDF) mascot face ("BMO") and support for Game Boy, Game Boy Color, NES, and DOOM.

---

## Key Highlights

- **Hardware Platform:** ESP32-S3-N16R8 (Dual-Core LX7 @ 240MHz, 16MB OPI Flash, 8MB Octal PSRAM).
- **Display Pipeline:** ST7789 240×320 SPI TFT (Landscape 320×240) @ 80MHz SPI with atomic N3 streaming protocol (`startFrame` / `streamPixelRow` / `endFrame`).
- **Mascot Face Engine:** Procedural 2D Signed Distance Field (SDF) mathematical renderer with analytic anti-aliasing and dynamic emotional expressions (`IDLE`, `HAPPY`, `SURPRISED`, `SLEEPY`, `LOW_BATTERY`, `CHARGING`, `ERROR`, `SHUTDOWN`).
- **Multi-Console Emulation:**
  - **Peanut-GB:** Game Boy DMG (`.gb`)
  - **Walnut-CGB:** Game Boy Color (`.gbc`) with custom CGB palette engine
  - **Agnes:** Nintendo Entertainment System (`.nes`)
  - **doomgeneric:** Classic DOOM (`.wad`) with direct VFS streaming
- **Storage & Fallback:** Dual-ROM system supporting hot-swappable MicroSD cards and built-in flash-baked ROMs (Super Mario Bros. Deluxe, Zelda: Oracle of Ages) running seamlessly without an SD card.
- **AI-Compatible Agent Environment:** Strict governance rules, zero-context primers, symbol verification tables, and anti-pattern registries under [`.agents/rules/`](file:///e:/BMO%20Gameboy/.agents/rules/README.md).

---

## Documentation Quick Links

- [**Software Design Document (SDD v3.0)**](file:///e:/BMO%20Gameboy/docs/software-design-document.md) — Authoritative living architectural specification covering hardware ground truth, state machine, memory layout, rendering pipeline, emulator contracts, and AI governance.
- [**Agent Quick-Start Primer**](file:///e:/BMO%20Gameboy/.agents/rules/31_quick_start_primer.md) — 90-second on-ramp and decision matrix for autonomous coding agents and human contributors.
- [**Hardware Notes & Lessons Learned**](file:///e:/BMO%20Gameboy/docs/hardware-notes.md) — Board-level wiring, pin restrictions, and power notes.
- [**Changelog**](file:///e:/BMO%20Gameboy/CHANGELOG.md) — Repository version history and milestone tracking.
- [**Agent Manifest**](file:///e:/BMO%20Gameboy/AGENT_MANIFEST.json) — Machine-readable hardware and build metadata.

---

## Repository Structure

```text
repo-root/
├── README.md                          <- Project overview & quick start
├── CHANGELOG.md                       <- Human-readable version history
├── AGENTS.md                          <- Entry point for AI coding agents (Ruleset v5)
├── AGENT_MANIFEST.json                <- Machine-readable project metadata
├── docs/                              <- Specifications & hardware notes
│   ├── software-design-document.md    <- Living reviewer-grade SDD (v3.0)
│   └── hardware-notes.md              <- Pin restrictions & electrical notes
├── .agents/
│   └── rules/                         <- Topic-specific modular agent rules (37 rules)
│       ├── 00_hard_stops.md           <- Non-negotiable hardware safety rules
│       ├── 01_hardware.md             <- Verified pin map & physical state
│       ├── 10_symbol_reference.md     <- Verified public symbol lookup table
│       ├── 27_codebase_map.md         <- System architecture & memory map
│       ├── 30_common_agent_mistakes.md<- Institutional anti-pattern catalogue (M1-M20)
│       ├── 31_quick_start_primer.md   <- 90-second zero-context on-ramp
│       ├── 35_bmo_face_contract.md    <- Procedural mascot renderer contract
│       ├── 36_bug_intake_protocol.md  <- Structured hardware bug intake protocol
│       ├── 37_rom_governance_and_flash_budget.md <- ROM tracking & flash budget invariant
│       └── CONTEXT_INDEX.json         <- Machine-readable task-to-rule map
├── firmware/
│   └── BmoGameboy/                    <- Main Arduino sketch directory
│       ├── BmoGameboy.ino             <- State machine, loop(), frame timing
│       ├── partitions.csv             <- Custom 8MB app0 partition table
│       └── src/
│           ├── core/                  <- Hardware drivers (display, buttons, SD, face)
│           ├── emulators/             <- Thin glue wrappers (peanut, walnut, nes, doom)
│           ├── engine/                <- Header-only emulator engines (walnut_cgb)
│           ├── vendor/                <- Upstream libraries (peanut_gb, agnes, doom)
│           └── assets/                <- Assets & baked flash ROM headers
├── tools/                             <- Host desktop test harness (Zig)
└── scripts/                           <- AI Guardian validation & asset pipeline
```


---

## Firmware Build & Flash

### Prerequisites
1. **Arduino CLI** or **Arduino IDE** with ESP32 board support (Core v3.3.11).
2. Libraries: `Adafruit_GFX`, `Adafruit_ST7789`.

### Build Command (Arduino CLI)
```powershell
.\arduino-cli.exe compile --fqbn "esp32:esp32:esp32s3:FlashMode=opi,FlashSize=16M,PartitionScheme=custom,PSRAM=opi" firmware/BmoGameboy
```

> [!IMPORTANT]
> Always compile with `FlashMode=opi` and `PSRAM=opi`. Using QPI/QIO will cause flash cache faults and boot loops on the ESP32-S3-N16R8.

---

## Tooling & Verification Pipeline

1. **Repository Health Check:**
   ```bash
   python scripts/validate_repo.py
   ```
2. **Python Test Suite:**
   ```bash
   python -m unittest discover -s tests -v
   ```
3. **Validation Benchmark:**
   ```bash
   python scripts/benchmark_repo.py
   ```
4. **ROM Integrity Verification:**
   ```bash
   python scripts/test_runner.py
   ```

---

## License

MIT - see [LICENSE](LICENSE).
