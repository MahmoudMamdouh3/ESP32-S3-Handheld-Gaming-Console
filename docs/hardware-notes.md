# Hardware Notes & Lessons Learned

Board-specific facts and hard-won debugging lessons discovered during
bring-up. Recorded here so future milestones don't rediscover them the
hard way.

---

## ESP32-S3-N16R8 pin restrictions

- **GPIO 33-37** are reserved for octal PSRAM on this board (the "R8"
  in N16R8). Even though these are physically broken out on the header,
  using them as regular GPIO will corrupt PSRAM. **Never use them.**

- **GPIO 0, 3, 45, 46** are strapping pins — they affect boot mode
  selection at power-on. Avoided for buttons, since holding one down at
  power-up could force the board into the wrong boot mode.

- Default hardware SPI pins on this board are **SCK=12, MOSI=11,
  MISO=13, CS=10** — this matters a lot once a second SPI device (SD
  card) is added; see below.

---

## Display: software vs hardware SPI

- `Adafruit_ST7789(cs, dc, mosi, sclk, rst)` (5-arg constructor) uses
  **software/bit-banged SPI** — fine when the display is the only SPI
  device (milestone 1).

- The moment a second SPI device (SD card) shares the same physical
  lines, software SPI on the display + the SD library's own default
  hardware `SPI.begin()` call fight over the same pins — specifically,
  the SD library's default pin mapping collides with GPIO10 (display CS)
  and GPIO13. **Symptom:** display goes fully black (but backlight stays
  lit, since BLK is powered independently of the controller chip) the
  moment `SD.begin()` runs.

- **Fix used for milestone 2 (SD card):** explicitly claim the SPI bus
  with our real wired pins before initializing either device, and switch
  the display to the hardware-SPI constructor so both devices share one
  bus cleanly:

  ```cpp
  SPI.begin(TFT_SCK, SD_MISO, TFT_MOSI, TFT_CS);
  Adafruit_ST7789 tft(&SPI, TFT_CS, TFT_DC, TFT_RST);
  // ...
  SD.begin(SD_CS, SPI);
  ```

> **Note:** Milestone 1's firmware was already updated to use hardware
> SPI (`SPIClass(FSPI)`) during code review, so this transition is
> already partially done.

---

### 4. ROM Legality

- `tobu_tobu_girl.gb` (Tobu Tobu Girl) is an open-source homebrew game licensed under MIT and CC-BY by Tangram Games. It's safe to redistribute for testing.
- `cpu_instrs.gb` is a widely used Game Boy CPU-correctness diagnostic ROM commonly attributed to Shay Green ("blargg"). No formal license file has been located for it; it is commonly redistributed by the GB homebrew/emulator-development community for non-commercial testing purposes. This project uses it only as a local development/test aid and does not redistribute it as part of any built firmware image (see .gitignore).

---

## SD card module needs 5V, not 3.3V

The cheap SD breakout module (VCC/GND/MISO/MOSI/SCK/CS, 6-pin) has an
onboard regulator + resistor-based level shifter designed assuming **5V
input**, even though the card itself runs at 3.3V. Powering VCC from
3.3V leaves the regulator without enough headroom and the level shifters
undersupplied. **Wire SD module VCC to 5Vin, not 3V3.** Logic lines
(SCK/MOSI/MISO/CS) stay on the ESP32's 3.3V logic regardless — the
module handles the level shift.

---

## Breadboard power rail splits

Many breadboards have their + and − power rails **physically broken in
the middle** — the two halves are not electrically connected to each
other despite looking continuous. A device powered from one half while
the microcontroller's supply is on the other half gives
flickering/intermittent power, not a clean fail — this cost significant
debugging time before being identified. If a component behaves
erratically despite solid-looking wiring, check this first: either
bridge both rail halves with a jumper, or keep all power connections on
the same half.

---

## USB ports (boards with two USB-C connectors)

Boards with both a "UART/COM" port and a "USB/OTG" port: use the
**UART/COM** one for flashing — it goes through a dedicated USB-to-serial
chip and is more reliable. The native USB/OTG port works too but needs
"USB CDC On Boot" enabled in Arduino IDE for Serial Monitor output, and
won't show a COM port reliably otherwise.

---

## Milestone 3 (Complete): Peanut-GB emulator core

- Using [Peanut-GB](https://github.com/deltabeard/Peanut-GB)
  (deltabeard) — real, MIT-licensed, single C99 header.

- `ENABLE_SOUND 0` is an explicitly supported configuration (no audio
  hardware on this build).

### 3. Display and UI Rendering

- **SPI Performance:** We rely on Adafruit_GFX's native SPI `writeBytes()` rather than `pushColors()` because it provides massive speed improvements, avoiding byte-by-byte iteration. We pre-swap our color palette array bytes instead.
- **Cover Art:** The Game Selection Menu uses `Adafruit_GFX::drawRGBBitmap()` to render 100x100 RGB565 cover images from flash memory. Images are generated via Python and the bytes are statically swapped ahead of time to avoid runtime endianness overhead.

### 4. Peanut-GB Library Customizations

- The `peanut_gb.h` library has been slightly modified for integration:
  - **Joypad Input:** The `gb.direct.joypad_bits` struct no longer exists in this library version. Input is now a flat `uint8_t` byte (`gb.direct.joypad`).
  - **Memory Endianness:** We rely on Adafruit_GFX's native SPI `writeBytes()` rather than `pushColors()` because it provides massive speed improvements, avoiding byte-by-byte iteration. We pre-swap our color palette array bytes instead.

- **Display Scaling & Endianness:** GB native resolution is 160×144. 
  We implemented a fast nearest-neighbor 1.5x scale to 240x216 in `display_emu.cpp`.
  For raw `SPI.writeBytes` performance, colors must be pre-swapped from 
  RGB565 big-endian to little-endian (e.g. `0x9DE1` becomes `0xE19D`).

- **Frame Pacing & FreeRTOS Watchdog:** `delay()` rounds up to the next
  1ms FreeRTOS tick, which causes pacing bias. We use `delay()` for the bulk 
  sleep to yield to FreeRTOS, followed by `ets_delay_us()` for a hardware-timer 
  spin on the sub-ms remainder to hit 16742 µs per frame without burning CPU.

- **Performance:** Arduino's default `-Os` (size) optimization makes emulators 
  run very slowly. We force `-O3,unroll-loops` via `#pragma GCC optimize` at the top 
  of `emulator.cpp` and `display_emu.cpp` to achieve full speed. We also use 
  zero-wait state polling for buttons via a `gb_joypad_state` static bitmask to 
  eliminate function call overhead in the emulator hot loop.
  
- **Cache Alignment:** We strictly align emulator state structs to the ESP32-S3's 
  32-byte D-cache line boundaries (`__attribute__((aligned(32)))`) to prevent 
  cache thrashing during hot register reads.

### 5. Firmware Compilation & Toolchain Constraints

- **Static Constraints & Partitions:** The default `arduino-cli` ESP32-S3 compilation profile allocates a 1.2MB `app0` partition. Because our emulator core along with bundled assets currently exceeds 2.5MB, compilation will fail at the linking stage with "text section exceeds available space". 
- **Fix:** You must compile the firmware using the `PartitionScheme=custom` (or `huge_app`), along with `PSRAM=opi` and `FlashSize=16M` configurations to correctly map the N16R8 hardware limits. This allows the compiler to successfully map the `.rodata` bounds to the 16MB flash.

- **ROM legality:** never embed/commit a copyrighted commercial ROM
  (Pokemon, Mario, etc). Using freely-licensed homebrew only (e.g.
  [Tobu Tobu Girl](https://tangramgames.itch.io/tobutobugirl), MIT/CC-BY).
  `.gitignore` excludes `*.gb`/`*.gbc`/`rom_data.h` so ROM files and
  their generated C-array headers never get committed.
