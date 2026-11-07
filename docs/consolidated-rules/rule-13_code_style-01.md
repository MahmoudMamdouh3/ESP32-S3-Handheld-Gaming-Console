# Section 13 — Code Style & Formatting

Source: [`13_code_style.md`](../../.agents/rules/13_code_style.md).

# Code Style & Formatting
Purpose: stop "works but inconsistent" drift across four emulator cores plus custom drivers, written across many sessions with no shared style memory between them.

## Naming Conventions
- `snake_case` for functions inside emulator glue, matching existing vendor convention (e.g., `gb_rom_read16`).
- `PascalCase` for first-party classes/structs (e.g., `Buttons`, matching `Buttons::update()`).
- `SCREAMING_SNAKE_CASE` for macros/constants (e.g., `FEATURE_SD_CARD`, `BTN_UP`). Extend the existing convention, don't introduce a third one.

## Module Structure
- One module = one `.h`/`.cpp` pair under `src/core/`. Don't add unrelated free functions to an already-open file just because it's open.
- Header guards: use `#pragma once` (matching existing convention in `src/core/config.h` and others).

## Language & Libraries
- **No `String` class** (Arduino) in any function called more than once per frame — it heap-allocates. Use fixed-size `char[]` buffers or `snprintf`. Applies to menu label rendering, assembled debug strings, and file path construction.
- RTTI and C++ exceptions are disabled by default in the Arduino-ESP32 build environment. Do not write code that depends on `try/catch` or `dynamic_cast`.

## Syntax & Const-Correctness
- No raw magic numbers for pins, colors, sizes, or timing outside `config.h`/`theme.h` — generalizes `08_ui_style_guide.md`'s theme rule to all constants, not just UI color.
- Braces: same-line opening brace; always brace single-statement `if`/`for` bodies — no bare-statement shorthand, full stop.
- `const`-correctness: any pointer/reference parameter a function doesn't mutate is `const`; any local never reassigned is `const`.

## Control Flow
- **No recursion in any emulator hot path** (CPU step, per-scanline render) — stack depth isn't statically boundable the way a loop is on this MCU.

## Comments
- Comment *why*, not *what*. `// skip if SD absent` next to `if (!sdPresent) return;` is noise; `// SD absent: fall back to baked ROM per hardware-notes.md §7` earns its line.
- TODO format: `// TODO(scope): description`. A bare `// TODO` with no scope/description isn't acceptable — if you can't state the follow-up, log it in `04_known_issues.md` instead.