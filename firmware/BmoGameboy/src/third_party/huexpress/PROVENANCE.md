# huexpress / pce-go (Hu-Go! / Retro-Go / Cardputer-Emulators port)

- **Origin**: Adapted from `Cardputer-Game-Station-Emulators` (`src/pce/pce-go/`) based on `retro-go` `pce-go` (derived from Hu-Go!).
- **Upstream URL**:
  - `https://github.com/geo-tp/Cardputer-Game-Station-Emulators.git` (commit `74b47763c3f2ccbaed6505b83ef0ef37c2deac82`)
  - `https://github.com/ducalex/retro-go.git` (commit `4ced120669750ca7228fd0414211430c1d923166`)
- **License**: GNU General Public License v2.0 or later (`COPYING`).
- **Local Modifications**:
  1. Removed FreeRTOS `vTaskDelay`/`esp_timer_get_time`/`ets_delay_us` blocking loop (`RunPCE`) and `FILE*` save/load functions, adding single-frame `StepPCE()`, `LoadCardBorrowed()`, `PceSaveStateMem()`, and `PceLoadStateMem()`.
  2. Added standard IEEE `pce_crc32_le()` so ROM flags (`Populous` ExRAM, `Blazing Lazers` split ROM, `US_ENCODED`) work without Retro-Go.
  3. Routed all RAM/VRAM/SPRAM/ExRAM/BackupRAM/palette buffers through PSRAM (`../../hal/psram_alloc.h`) and freed `PCE.SPRAM`, `PCE.PSG.chan`, and `PCE.BackupRAM` cleanly on `pce_term()`.
  4. Added `$1807`/`$F7` Backup RAM mapping (`PCE.BackupRAM`, gated on `"HUBM"` header before persisting `.sav`).
