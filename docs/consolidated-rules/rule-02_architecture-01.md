# Section 02 — 02. Architecture Overview

Source: [`02_architecture.md`](../../.agents/rules/02_architecture.md).

# 02. Architecture Overview

## Runtime tasks

- Arduino setup() and loop() run the launcher and active emulator on the application's core.
- DisplayEmu creates a Core 0 task named BMO_Display for queued display transfers. It is not named vDisplayTask.
- GB and GBC wrappers use DisplayEmu::startFrame() / endFrame() and synchronously stream scanline callbacks while holding the shared bus. Other wrappers generally submit complete frames to the display worker; some indexed paths can fall back to synchronous transfer when async resources are unavailable.
- Do not claim overlap or a particular FPS without a fresh target measurement.

## Subsystem boundaries

- config.h is the source for pins and hard-stop feature values.
- Buttons samples the two ESP32-S3 GPIO input registers sequentially and translates button state. The two bank reads are not a single atomic sample.
- DisplayEmu owns TFT initialization, the display task, queues, canvas/staging buffers, menu drawing, and frame streaming.
- SpiArbiter serializes access to the shared FSPI bus used by TFT and SD.
- SDCard owns ROM discovery, the in-memory catalog, index cache, favorites/history, and SD ROM allocation.
- SaveManager owns save/state/screenshot file formats, ROM path keys, verification, and staged replacement.
- Emulator wrappers translate shared input, call their engines, submit display frames, and own engine allocations.

See the Software Design Document for subsystem details and docs/generated/symbols.md for declarations. The current emulator registry is implemented as dispatch logic in BmoGameboy.ino; a table-driven registry is still a future refactor.