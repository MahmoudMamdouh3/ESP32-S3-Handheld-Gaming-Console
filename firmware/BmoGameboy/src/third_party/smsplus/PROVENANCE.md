# SMS Plus GX (Charles MacDonald / Retro-Go / Cardputer-Emulators port)

- **Origin**: Adapted from `Cardputer-Game-Station-Emulators` (`src/sms/smsplus/`) based on `retro-go` / Charles MacDonald's SMS Plus.
- **Upstream URL**:
  - `https://github.com/geo-tp/Cardputer-Game-Station-Emulators.git` (commit `74b47763c3f2ccbaed6505b83ef0ef37c2deac82`)
  - `https://github.com/ducalex/retro-go.git` (commit `4ced120669750ca7228fd0414211430c1d923166`)
- **License**: GNU General Public License v2.0 or later (`COPYING`), with MAME Z80 emulator v2.7 (`z80.c`, Juergen Buchmueller — note original non-commercial software notice inside `z80.c` header).
- **Local Modifications**:
  1. Added `smsplus_prefix.h` (included via `shared.h`) to prefix all exported symbols with `smsp_` so they link alongside `gwenesis` (`Z80`) and `race` (`CZ80`) in a single Arduino binary.
  2. Routed heap buffers (`render.c`, `tms.c`, `vdp.c`, `sms.c`, `z80.c`, `sn76496.c`, `system.c`) to PSRAM (`../../hal/psram_alloc.h`) with clean host fallbacks.
  3. Removed stdio `FILE*` save-state coupling from `system.c` and added clean non-macro C bridge `smsplus_bmo.h` / `smsplus_bmo.c` supporting SMS, Game Gear, SG-1000, and ColecoVision with optional SD BIOS + legal reset-trampoline fallback.
