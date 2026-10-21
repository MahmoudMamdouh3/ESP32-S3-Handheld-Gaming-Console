# 31. Quick-Start Primer (Zero-Context Agent On-Ramp)
Purpose: Any AI agent or developer starting a session with zero prior memory can read this single page in 90 seconds and safely navigate the repository without breaking hardware or hallucinating APIs.

---

## 1. What Is This Repository? (3 Core Facts)
1. **Target Hardware:** ESP32-S3-N16R8 (16MB Quad SPI Flash, 8MB Octal SPI PSRAM) soldered on a permanent perfboard.
2. **Current Physical State:** USB-C powered, ST7789 320×240 landscape SPI display, 8 GPIO tactile buttons, MicroSD card. **No battery divider, no I2S audio DAC physically soldered.**
3. **Firmware Purpose:** 14-core retro gaming handheld (GB, GBC, NES, SMS/GG, Genesis, SNES, PCE, Atari 2600, Lynx, WonderSwan, NGP, ColecoVision, PICO-8, DOOM) + 320×240 procedural SDF BMO mascot companion.

---

## 2. The 3 Absolute Hard Stops (Never Violate)
1. **NEVER read GPIO1 or enable `FEATURE_BATTERY_MONITOR` (must stay `0`).** Floating ADC on GPIO1 causes bootloops.
2. **NEVER enable `FEATURE_AUDIO` (must stay `0`).** No physical I2S DAC is wired.
3. **NEVER set Flash Mode to OPI (requires `FlashMode=qio,PSRAM=opi`).** OPI Flash mode bricks boot on this Quad-Flash module. Never assign peripherals to strapping pins (`0, 3, 45, 46`) or Octal PSRAM pins (`33–37`).

---

## 3. Instant AI Context Harness (Run Before Editing)
Instead of reading dozens of files manually, generate a self-contained context packet for your exact task:
```powershell
# 1. Generate a complete task context card (target files, live C++ signatures, pins, guardrails)
python -m tools.guardian prompt-pack "<describe your task>"

# 2. Look up any function, struct, or hardware pin
python -m tools.guardian lookup "<symbol_or_pin>"

# 3. Pre-flight check code changes for brace balance, AST rules, and buzzwords
python -m tools.guardian patch-check

# 4. Audit your uncommitted diff before committing
python -m tools.guardian diff-audit

# 5. Export the entire AI harness & rules to another repository
python -m tools.guardian export-harness <destination_dir>
```

---

## 4. Architecture in 30 Seconds
- **Main State Machine ([`BmoGameboy.ino`](file:///e:/BMO%20Gameboy/firmware/BmoGameboy/BmoGameboy.ino)):**
  - `STATE_CONSOLE_MENU` -> `STATE_GAME_MENU` -> `STATE_EMULATOR` (plus `STATE_CONSOLE_SPECS`, `STATE_PAUSE_MENU`, `STATE_IDLE_MASCOT`).
  - Holding `START + SELECT` in-game opens `STATE_PAUSE_MENU` (Resume, Save/Load State Slots 0–4, `LEFT` = A/B swap, `RIGHT` = 1:1 console bezel, `SELECT` = 24-bit `.bmp` screenshot, Exit = auto-save `.sav` + `teardownCurrentEmulator()` + `SDCard::freeRom()`).
- **Display Streaming & Thread Safety:**
  - Landscape `320×240`, wire format **BGR565 byte-swapped** (`swapBytes()`).
  - Core 0 runs dedicated FreeRTOS display worker (`vDisplayTask`); Core 1 runs emulation and UI.
  - `SpiArbiter::lock()` protects the shared FSPI bus (`TFT_SCK=12`, `TFT_MOSI=11`) across display and MicroSD I/O.
- **Storage, Cache & Box Art:**
  - Recursive SD folder scan (depth 3) + binary cache (`/.bmo_index`), favorites (`/.favorites.txt`), and history (`/history.txt`).
  - `BoxArt` streams 64×64 `.raw`/`.bmp` cover art into PSRAM (8,192 B, 0 internal DRAM).
- **Button Polling:**
  - `Buttons::update()` samples all 8 active-low buttons atomically via `REG_READ(GPIO_IN_REG)` (`BTN_UP=4, BTN_DOWN=5, BTN_LEFT=6, BTN_RIGHT=7, BTN_A=16, BTN_B=17, BTN_START=18, BTN_SELECT=21`).

---

## 5. Task Decision Table: "I Want To Do X -> Read File Y First"
| If your task touches... | Read this rule file first: |
| :--- | :--- |
| Any pin, GPIO, or hardware component | `01_hardware.md` & `src/core/config.h` |
| Display rendering, colors, scaling, or SPI | `28_display_and_spi_contract.md` & `15_performance_budgets.md` |
| Adding or modifying an emulator core | `12_extensibility_contract.md`, `26_emulator_exit_contract.md`, & `32_modular_core_template.md` |
| Adding a new baked ROM | `29_adding_a_baked_rom.md` & `37_rom_governance_and_flash_budget.md` |
| Citing any function or variable name | `10_symbol_reference.md` or `python -m tools.guardian lookup "<name>"` |
| Performance benchmarks or profiling | `39_performance_and_benchmark_framework.md` & `tools/guardian/` |
| Save states, SD storage, or cover art | `19_security_and_data_integrity.md`, `src/core/save_manager.h`, `src/core/box_art.h`, `src/core/rom_index.h` |
| AI Knowledge base, rules, or Guardian CLI | `40_ai_knowledge_graph_and_indexing_protocol.md` & `42_quality_of_life_and_roadmap_ledger.md` |
| Writing comments, docs, or commit messages | `43_no_ai_jargon_and_buzzwords.md` & `05_git_workflow.md` |
| Ending session / committing & pushing | `05_git_workflow.md`, `33_agent_handoff_and_optimization_cycle.md`, & `04_known_issues.md` |

---

## 6. How To Verify, Commit & Hand Off Cleanly
1. **Fast Verification Gate (< 15s):**
   ```powershell
   python -m tools.guardian verify
   python -m tools.guardian diff-audit
   ```
2. **Full 8-Phase CI + ESP32-S3 Binary Compile Gate:**
   ```powershell
   python scripts/validate_repo.py
   ```
3. **Update Logs:** Add a concise dated entry in `.agents/rules/04_known_issues.md` and `CHANGELOG.md`.
4. **Commit with Mandatory Two-Part Message (`-m "subject" -m "bullet body"`):**
   ```powershell
   git commit -m "type(scope): concise summary" -m "- Detailed bullet 1`n- Detailed bullet 2`n- Verified: validate_repo.py PASS"
   ```
