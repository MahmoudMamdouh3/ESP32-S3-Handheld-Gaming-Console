# Repository Coding, Naming, and Documentation Conventions

This document defines the naming, formatting, unit, and evidence standards enforced across the repository (`HOST_TESTED` via `python tasks.py check` and `python tasks.py format --check`).

---

## 1. Identifier & File Naming Conventions

| Scope | Convention | Examples |
| :--- | :--- | :--- |
| **C/C++ Classes, Structs, Enums, Namespaces** | `PascalCase` | `SpiArbiter`, `SaveStateFormat`, `BootSpiDecision` |
| **C/C++ Functions & Methods** | `camelCase` | `loadSavedDisplaySpiClock()`, `validateEntryRecord()` |
| **C/C++ Constants & Preprocessor Macros** | `UPPER_SNAKE_CASE` | `TFT_SPI_HZ`, `FEATURE_BATTERY_MONITOR`, `MAGIC_V2` |
| **C/C++ File-Static & Member Variables** | `s_camelCase` (file-static), `m_camelCase` (class member), or `camelCase` (local/struct field) | `s_activeEmulator`, `s_bootSplashStartMs`, `pathOffset` |
| **C/C++ Source & Header Files** | `snake_case.cpp` / `snake_case.h` | `rom_index.h`, `boot_safe_mode.h`, `pins_check.h` |
| **Python Modules, Functions & Variables** | `snake_case` (`UPPER_SNAKE_CASE` for module constants, `PascalCase` for classes) | `generate_hardware_layout_artifacts.py`, `REPO_ROOT` |
| **Documentation & ADR Files** | `kebab-case.md` (`NNNN-kebab-case.md` for ADRs) | `0014-display-spi-clock.md`, `owner-guide.md`, `perfboard-layout.md` |

> [!NOTE]
> Legacy underscore-named files (`docs/hardware_measurements.md`, `firmware/BmoGameboy/BmoGameboy.ino`) are preserved where external links or Arduino CLI sketch-folder rules require exact filename match.

---

## 2. Explicit Physical & Timing Units in Identifiers

Every variable, constant, or JSON key that represents a physical or timing quantity MUST include its unit suffix:

| Unit | Required Suffix | Examples |
| :--- | :--- | :--- |
| Milliseconds / Microseconds | `_ms` / `_us` (or `Ms` / `Us` in `camelCase`) | `s_bootSplashStartMs`, `frameBudgetUs` |
| Frequency (Hertz) | `_hz` (or `Hz`) | `TFT_SPI_HZ`, `spiHz`, `kSdSpiHz` |
| Memory / Byte Count | `_bytes`, `_kb` | `testBytes`, `freeHeapBytes` |
| Physical Length (Docs & Layout) | `_cm` (docs/layout) or `_mm` (CAD internals only) | `width_cm`, `snapped_x_cm`, `outerWidth` |
| Electrical Voltage / Current | `_mv`, `_v`, `_ma` | `cutoff_mv`, `load_ma` |

In all user-facing documentation and reports, physical dimensions MUST be written in **centimetres (`cm`)** (only the `2.4"` display diagonal uses inches).

---

## 3. Disambiguating "Core"

Never use the bare noun **"core"** when its meaning could be ambiguous:
- Write **"emulator core"** (or "console wrapper") when referring to `Peanut-GB`, `Walnut-CGB`, `agnes`, `DOOM`, etc.
- Write **"CPU core 0"** (Protocol / background SD & DMA helper) or **"CPU core 1"** (Application / Arduino `setup()` and `loop()`) when referring to the dual-core Xtensa LX7 processor on the ESP32-S3.

---

## 4. Evidence Vocabulary (Four-Level Classification)

Every verification claim in commit messages, documentation, and audit tables MUST use one of these four exact labels:

1. `NOT_TESTED` — Analytical calculation, datasheet inspection, or unexecuted code path.
2. `HOST_TESTED` — Executed on the host CPU via Python (`unittest`), native C/C++ (`CTest`), or WebAssembly tests.
3. `TARGET_COMPILED` — Cross-compiled with `arduino-cli` (`esp32:esp32:esp32s3`) with zero errors.
4. `HARDWARE_TESTED` — Physically observed on the soldered ESP32-S3 perfboard and logged in [`docs/hardware_measurements.md`](hardware_measurements.md).

---

## 5. Formatting, Line Endings, and Commit Messages

- **Line Endings:** LF (`\n`) across all text files on Windows, macOS, and Linux, enforced by [`.gitattributes`](../.gitattributes) and [`.editorconfig`](../.editorconfig).
- **C/C++ Formatting:** Enforced on all first-party C/C++ files via [`.clang-format`](../.clang-format) (`python tasks.py format --check`). Vendored code under `firmware/BmoGameboy/src/third_party/` is never reformatted.
- **Conventional Commits:** Commit subjects must match `type(scope): subject` (enforced by `.githooks/commit-msg`), where `type` is one of `feat`, `fix`, `docs`, `refactor`, `test`, `ci`, `chore`, `perf`, `build`, or `revert`.
