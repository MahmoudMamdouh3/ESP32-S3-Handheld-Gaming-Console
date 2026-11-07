# Section 34 — 34. AI Agent Guardrails & Execution Invariants

Source: [`34_ai_agent_sandbox_and_guardrails.md`](../../.agents/rules/34_ai_agent_sandbox_and_guardrails.md).

# 34. AI Agent Guardrails & Execution Invariants

**Purpose:** This document establishes strict guardrails and fail-safe constraints for Large Language Models (LLMs) and autonomous agents operating in this repository. It eliminates common LLM failure modes (hallucinations, destructive refactoring, silent regressions, context overflow).

---

## 1. Golden Invariants for AI Agents

1. **Physical Hardware Precedence:**
   - The physical wiring and soldered state on the perfboard is ground truth.
   - Code must never enable dormant features (`FEATURE_BATTERY_MONITOR`, `FEATURE_AUDIO`) without user confirmation of physical soldering.
   - Never write to or read from GPIO 33-37 (reserved for internal Octal PSRAM).

2. **Zero-Hallucination API Rule:**
   - LLMs often hallucinate convenient helper methods (e.g. `DisplayEmu::drawText`, `Buttons::isDown`, `gb_write16`).
   - If a symbol is not present in `10_symbol_reference.md` or found in a live header file in `src/`, it DOES NOT EXIST. Do not call it.

3. **Atomic File Modifications:**
   - Avoid massive, monolithic file replacements when making localized bug fixes.
   - Preserve existing header guards, include hierarchies, and performance macros (`IRAM_ATTR`, `__attribute__((aligned(32)))`).

4. **Continuous CI Validation:**
   - Before finishing any task, run `python scripts/validate_repo.py`. If this script reports any violations, resolve them immediately before reporting completion.

5. **Endianness & Memory Alignment Safety:**
   - ST7789 wire format is **BGR565 Byte-Swapped (Big-Endian)**.
   - Flash memory `.rodata` is 8-bit byte aligned; unaligned 16-bit or 32-bit direct pointer dereferences on Flash can cause Xtensa exception crashes. Always use explicit byte reconstruction (`(b0) | (b1 << 8)`).

6. **Teardown & PSRAM Lifecycle Guardrail:**
   - Every emulator core must implement `destroy()`.
   - Every `begin()` dynamic allocation in PSRAM must be matched by a corresponding deallocation in `destroy()`.
   - The main `STATE_EMULATOR` loop in `BmoGameboy.ino` must call `destroy()` upon `SELECT + UP` exit.

7. **Critical Pushback & Anti-Sycophancy Guardrail (See Rule 41):**
   - If user instructions propose impossible physics, hazardous hardware states (e.g. enabling floating ADC lines), unbudgeted DRAM allocations, or architectural regressions, DO NOT blindly execute.
   - Respectfully explain the technical constraints with physics/memory figures, describe the failure consequences, and present standard engineering alternatives before taking action.

8. **Toolchain Autonomy & Resource Utilization:**
   - If a tool, dependency, compiler, or script is missing or fails, autonomously install, fix, or build it using full host system resources (CPU, GPU, RAM, internet) rather than assuming a blocker.

---

## 2. Guardrail Failure Matrix & Instant Resolutions

| Symptom / Failure Mode | Root Cause | Instant Resolution |
| :--- | :--- | :--- |
| MCU reboots immediately on startup | Reading floating `GPIO1` | Ensure `FEATURE_BATTERY_MONITOR = 0` in `config.h` |
| Screen colors inverted (Red is Blue) | ST7789 MADCTL BGR bit missing | Check `DisplayEmu::begin()` writes `0xA0 \| 0x08` to MADCTL |
| Memory leak after playing several games | Core `destroy()` not called | Verify `BmoGameboy.ino` calls `Core::destroy()` and `SDCard::freeRom()` |
| Frame rate drops in game / audio stutter | Dynamic allocation in `runFrame()` | Move all `malloc`/`heap_caps_malloc` out of frame loop into `begin()` |
| Build fails with "undefined reference" | Hallucinated function name | Check `10_symbol_reference.md` for exact signature |