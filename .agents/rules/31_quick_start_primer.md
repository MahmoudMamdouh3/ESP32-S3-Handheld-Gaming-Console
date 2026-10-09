# 31. Quick-Start Primer (Zero-Context Agent On-Ramp)
Purpose: Any AI agent or developer starting a session with zero prior memory can read this ONE page in 90 seconds and safely navigate the repository without breaking hardware or hallucinating APIs.

---

## 1. What Is This Repository? (3 Core Facts)
1. **Target Hardware:** ESP32-S3-N16R8 (16MB Flash, 8MB Octal PSRAM) running on a permanent perfboard.
2. **Current Physical State:** USB-C powered, ST7789 240x320 SPI display, 8 GPIO tactile buttons, MicroSD card. **No battery divider, no I2S audio DAC physically soldered.**
3. **Firmware Purpose:** Multi-console retro handheld (Game Boy, GBC, NES, DOOM) with a procedural 2D SDF animated mascot face ("BMO").

---

## 2. The 3 Absolute Hard Stops (Never Violate)
1. **NEVER read GPIO1 or enable `FEATURE_BATTERY_MONITOR` (must stay `0`).** Floating ADC on GPIO1 causes bootloops.
2. **NEVER enable `FEATURE_AUDIO` (must stay `0`).** No physical I2S DAC is wired.
3. **NEVER set Flash Mode to OPI in Arduino IDE (requires `Flash Mode: QIO 80MHz` + `PSRAM: OPI PSRAM`).** OPI Flash mode bricks boot on this Quad-Flash module.

---

## 3. Architecture in 30 Seconds
- **Main State Machine ([`BmoGameboy.ino`](file:///e:/BMO%20Gameboy/firmware/BmoGameboy/BmoGameboy.ino)):**
  - `STATE_CONSOLE_MENU` -> `STATE_GAME_MENU` -> `STATE_EMULATOR`.
  - In-game quick pause menu (`STATE_PAUSE_MENU`) on `SELECT + START` with 5-slot save states (`SaveManager`).
  - Exiting emulator (`SELECT + UP`) auto-saves to Slot 1, invokes `destroy()` on active core, and frees PSRAM.
- **Display Streaming & Thread Safety:**
  - Display is in Landscape (`320x240`). Game Boy is centered (`240x216`, `OFFSET_X=40, OFFSET_Y=12`).
  - Wire format: **BGR565 byte-swapped**.
  - Non-blocking Core 0 dedicated FreeRTOS worker (`vDisplayTask`) eliminates 15.36ms SPI blocking stalls.
  - `SpiArbiter` mutex protects shared FSPI bus across display streaming and MicroSD card operations.
- **Storage, Cache & Box Art:**
  - Fast binary index cache (`/.bmo_index`) reduces SD enumeration to < 15ms.
  - `BoxArt` engine streams 64x64 raw/BMP cover art into PSRAM with zero DRAM consumption.
- **Button Polling:**
  - `Buttons::update()` called once per frame. Maintains `Buttons::gb_joypad_state` (0=pressed). Emulators read bitmask branchlessly.
- **Flash & Partition Table:**
  - Custom `partitions.csv` allocates an **8MB `app0`** partition for firmware + baked ROMs (`mario_deluxe.h`, `zelda_ages.h`).

---

## 4. Where Is Code Located?
| Path | Contents |
| :--- | :--- |
| `firmware/BmoGameboy/BmoGameboy.ino` | `setup()`, `loop()`, state machine dispatch, frame pacing |
| `firmware/BmoGameboy/src/core/` | Hardware drivers & storage (`config.h`, `display_emu`, `buttons`, `sd_card`, `spi_arbiter`, `save_manager`, `box_art`, `rom_index`, `bmo_face`, dormant `battery`/`audio_i2s`) |
| `firmware/BmoGameboy/src/emulators/` | Thin C++ glue wrappers (`emu_peanut`, `emu_walnut`, `emu_nes`, `emu_doom`) |
| `firmware/BmoGameboy/src/engine/` | Walnut GBC emulator engine (`walnut_cgb.h`) |
| `firmware/BmoGameboy/src/vendor/` | Pristine vendor engines (`peanut_gb`, `agnes`, `doom`) |
| `firmware/BmoGameboy/src/assets/roms/` | Baked flash ROM C headers |
| `tools/host_test.cpp` | Host desktop CPU verification harness (Zig compiler) |
| `.agents/rules/` | Agent governance rules (symbol reference, hardware pin map, incident logs) |

---

## 5. Task Decision Table: "I Want To Do X → Read File Y First"
> [!TIP]
> **Instant Zero-Shot CLI Router:** Run `python -m tools.guardian route "<your task>"` or `python -m tools.guardian lookup "<symbol>"` for instantaneous directions without searching!

| If your task touches... | You MUST read this rule file first: |
| :--- | :--- |
| Any pin, GPIO, or hardware component | `01_hardware.md` & `src/core/config.h` |
| Display rendering, colors, scaling, or SPI | `28_display_and_spi_contract.md` & `15_performance_budgets.md` |
| Adding or modifying an emulator core | `12_extensibility_contract.md`, `26_emulator_exit_contract.md`, & `32_modular_core_template.md` |
| Adding a new baked ROM | `29_adding_a_baked_rom.md` & `37_rom_governance_and_flash_budget.md` |
| Citing any function or variable name | `10_symbol_reference.md` or `python -m tools.guardian lookup "<name>"` |
| Performance benchmarks or profiling | `39_performance_and_benchmark_framework.md` & `tools/guardian/` |
| Save states, SD storage, or cover art | `19_security_and_data_integrity.md`, `src/core/save_manager.h`, `src/core/box_art.h`, `src/core/rom_index.h` |
| AI Knowledge base & indexing | `40_ai_knowledge_graph_and_indexing_protocol.md` |
| Modifying vendor `#define` flags | `24_vendor_flag_safety.md` |
| AI guardrails, anti-hallucination, invariants | `34_ai_agent_sandbox_and_guardrails.md` |
| Ending session / logging handoffs | `33_agent_handoff_and_optimization_cycle.md` & `04_known_issues.md` |
| Writing commit messages or status reports | `05_git_workflow.md` & `06_verification_standards.md` |
| Investigating an unexpected bug | `04_known_issues.md` & `30_common_agent_mistakes.md` |

---

## 6. How To Build & Hand Off Cleanly
1. **Run AI Guardian CI Validation:**
   ```powershell
   python scripts/validate_repo.py
   python -m unittest discover tests
   ```
2. **Verified Firmware Build Command:**
   ```powershell
   .\arduino-cli.exe compile --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashMode=qio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi" firmware/BmoGameboy
   ```
3. Run compilation / tests before claiming completion.
4. Update `04_known_issues.md` and `CHANGELOG.md` with a one-line dated entry.
5. Use the mandatory two-header final report format:
   - `## Verified by me this session`
   - `## Waiting on you`
