# 7. Repository & File Structure
The project uses a **single, clean repository structure**. No numbered milestone folders (`01_foo`, `02_bar`) are permitted. Past states are tracked via git history.

```
repo-root/
├── README.md
├── CHANGELOG.md                      ← human-readable version log
├── .gitignore
├── docs/                             ← architecture docs & hardware notes
├── .agents/                          ← ground-truth LLM agent rules
│   └── rules/                        ← topic-specific modular rule files
├── tools/                            ← host test harness & asset tooling
├── scripts/                          ← ROM validator and build automation scripts
└── firmware/
    └── BmoGameboy/                   ← Arduino sketch folder (MUST match .ino)
        ├── BmoGameboy.ino            ← ONLY setup(), loop(), and state dispatch
        ├── partitions.csv            ← 8MB app0 custom partition table
        └── src/
            ├── core/                 ← hardware drivers & storage (config.h, display, buttons, SDF face, spi_arbiter, save_manager, box_art, rom_index)
            ├── emulators/            ← glue code per console (emu_peanut, emu_walnut, emu_nes, emu_doom)
            ├── engine/               ← header-only engines (walnut_cgb)
            ├── vendor/               ← third-party libraries (peanut_gb, agnes, doom)
            ├── assets/               ← asset headers
            │   └── roms/             ← baked commercial ROM headers (mario, zelda)
            └── tests/                ← on-device unit tests
```
- **Rule:** Custom engines live in `src/engine/<name>/`, while third-party vendor libraries go under `src/vendor/<name>/`.
- **Rule:** Generated assets and baked ROM headers go under `src/assets/`.

---

# 8. Testing & Verification Workflow
- The file `src/tests/unit_tests.cpp` contains critical validation tests (e.g., PSRAM speed checks, module validation). 
- **Rule:** No task should be reported complete without at minimum a successful compile.
- **Rule:** If touching memory architecture or core drivers, the unit tests should be executed by calling `UnitTests::runAll()` in `setup()` before integration.

---

# 9. IP & Licensing Notes
This project currently bakes multiple commercial ROMs directly into the firmware flash as fallback games (`mario_deluxe.h`, `zelda_ages.h`, `aladdin.h`, `lego_racers.h` under `src/assets/roms/`). 
- This is a known, deliberate personal/non-commercial choice.
- **Test ROMs:** We use Blargg's `cpu_instrs.gb` (Public Domain) for CPU correctness validation in the host-side test harness.
- **FORWARD-LOOKING RULE:** No new copyrighted commercial ROM or third-party copyrighted asset may be baked into the firmware without it being explicitly logged in this file first.

---

# 10. Agent Behavior Rules
1. **Never Assume Hardware:** Agents MUST NOT guess at unverified hardware/toolchain facts. If you cannot definitively verify a hardware state, library version, or wiring layout from the code, DO NOT fabricate confident prose. Check using command line tools, or ask the user directly.
2. **Graceful Degradation:** Hardware is brittle. Use `FEATURE_*` flags meticulously.
3. **Isolate Peripherals:** When introducing a new hardware module (e.g., I2C FRAM), always write an isolated, standalone `.ino` sketch in the `tools/` or a scratch directory to verify the hardware works before integrating it into `BmoGameboy.ino`.
4. **Follow the Ground Truth:** You must trust these rules over your own inferences when resolving architectural intent.

---

# 11. Documentation Maintenance Protocol
The files under `.agents/rules/` MUST be updated immediately upon completing a task that involves:
- **Pin Mapping:** Adding, removing, or changing any GPIO pin assignment.
- **Feature Flags:** Adding a new `FEATURE_*` toggle.
- **Architecture:** Introducing a new core module or changing a fundamental loop/memory pattern.
- **Hardware Wiring:** When dormant hardware (like the battery circuit) is physically soldered.
- **Known Issues:** Resolving or discovering a bug that belongs in the Technical Debt log.

Routine bug fixes not touching hardware/architecture, or pure refactors that don't change the directory conventions, do NOT require updating these files.
