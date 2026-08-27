# Hardware Notes & Lessons Learned

Board-specific facts and hard-won debugging lessons discovered during
bring-up. Recorded here so future milestones don't rediscover them the
hard way.

---

## 1. ESP32-S3-N16R8 pin restrictions

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

## 2. Display: software vs hardware SPI

- `Adafruit_ST7789(cs, dc, mosi, sclk, rst)` (5-arg constructor) uses
  **software/bit-banged SPI** — fine when the display is the only SPI
  device.

- The moment a second SPI device (SD card) shares the same physical
  lines, software SPI on the display + the SD library's own default
  hardware `SPI.begin()` call fight over the same pins — specifically,
  the SD library's default pin mapping collides with GPIO10 (display CS)
  and GPIO13. **Symptom:** display goes fully black (but backlight stays
  lit, since BLK is powered independently of the controller chip) the
  moment `SD.begin()` runs.

- **Fix used:** explicitly claim the SPI bus
  with our real wired pins before initializing either device, and switch
  the display to the hardware-SPI constructor so both devices share one
  bus cleanly:

  ```cpp
  SPI.begin(TFT_SCK, SD_MISO, TFT_MOSI, TFT_CS);
  Adafruit_ST7789 tft(&SPI, TFT_CS, TFT_DC, TFT_RST);
  // ...
  SD.begin(SD_CS, SPI);
  ```

---

## 3. SD card module needs 5V, not 3.3V

The cheap SD breakout module (VCC/GND/MISO/MOSI/SCK/CS, 6-pin) has an
onboard regulator + resistor-based level shifter designed assuming **5V
input**, even though the card itself runs at 3.3V. Powering VCC from
3.3V leaves the regulator without enough headroom and the level shifters
undersupplied. **Wire SD module VCC to 5Vin, not 3V3.** Logic lines
(SCK/MOSI/MISO/CS) stay on the ESP32's 3.3V logic regardless — the
module handles the level shift.

---

## 4. Breadboard power rail splits

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

## 5. USB ports (boards with two USB-C connectors)

Boards with both a "UART/COM" port and a "USB/OTG" port: use the
**UART/COM** one for flashing — it goes through a dedicated USB-to-serial
chip and is more reliable. The native USB/OTG port works too but needs
"USB CDC On Boot" enabled in Arduino IDE for Serial Monitor output, and
won't show a COM port reliably otherwise.

---

## 6. Multi-Emulator Architecture & Optimizations

The system now hosts four engines:
- **Peanut-GB** (Game Boy)
- **Walnut-CGB** (Game Boy Color)
- **Agnes** (NES)
- **doomgeneric** (DOOM)

- **Memory Endianness & SPI Performance:** We rely on Adafruit_GFX's native SPI `writeBytes()` rather than `pushColors()` because it provides massive speed improvements, avoiding byte-by-byte iteration. We pre-swap our color palette array bytes instead.

- **Display Scaling:** GB native resolution is 160×144. We implemented a fast nearest-neighbor 1.5x scale to 240x216 in `display_emu.cpp`.

- **Frame Pacing & FreeRTOS Watchdog:** `delay()` rounds up to the next
  1ms FreeRTOS tick, which causes pacing bias. We use `delay()` for the bulk 
  sleep to yield to FreeRTOS, followed by `ets_delay_us()` for a hardware-timer 
  spin on the sub-ms remainder to hit 16742 µs per frame without burning CPU.

- **Performance & Caching:** Arduino's default `-Os` (size) optimization makes emulators 
  run very slowly. We force `-O3,unroll-loops` via `#pragma GCC optimize` at the top 
  of emulator files to achieve full speed. We use zero-wait state polling for buttons via a `gb_joypad_state` static bitmask to eliminate function call overhead in the emulator hot loop. 
  Additionally, emulator state structs are strictly aligned to the ESP32-S3's 32-byte D-cache line boundaries (`__attribute__((aligned(32)))`) to prevent cache thrashing during hot register reads. 
  Performance critical functions are assigned zero-wait-state IRAM placement (`IRAM_ATTR`).

---

## 7. Display Color Quirks & ST7789

- **RGB vs BGR swapped colors:** Some generic ST7789 TFT panels are natively configured for BGR color order, while libraries like `Adafruit_ST7789` assume RGB. This results in Red and Blue channels being swapped (e.g., Mario's red overalls become blue, sky blue becomes orange). 
- **The Hardware Fix:** Instead of wasting CPU cycles trying to byte-swap the colors in software for every emulator and UI element, this can be solved instantly at initialization by manually sending an SPI command to toggle the BGR bit (`0x08`) in the `MADCTL` register (`0x36`).

---

## 8. UI Rendering Performance

- **Avoid Heavy GFX Drawing Loops:** Drawing complex or large amounts of basic geometry (like an animated background grid of `drawFastVLine` and `drawFastHLine`) in a 60FPS loop using standard GFX functions introduces massive CPU and SPI overhead. 
- **Optimization:** Keep menus visually simple. Removing the animated background grid and replacing solid filled shapes (`fillRoundRect`) with simple outlines (`drawRoundRect`) for the selected menu item drastically improved menu responsiveness and framerate.

---

## 9. Firmware Compilation & ROM Legality

- **Static Constraints & Partitions:** The default `arduino-cli` ESP32-S3 compilation profile allocates a 1.2MB `app0` partition. Because our emulator cores along with bundled assets currently exceeds 2.5MB, compilation will fail at the linking stage with "text section exceeds available space". 
- **Fix:** You must compile the firmware using the `PartitionScheme=custom` (or `huge_app`), along with `PSRAM=opi` and `FlashSize=16M` configurations to correctly map the N16R8 hardware limits. This allows the compiler to successfully map the `.rodata` bounds to the 16MB flash.

- **Baked ROM Limits (3MB Default Partition):** Baking multiple ~1MB ROMs (like Mario Deluxe and Zelda) via C headers can quickly exceed limits, resulting in compilation errors. The solution is correctly configuring the 16MB partition scheme in Arduino IDE, or simply relying on the SD card to load ROMs dynamically.

- **ROM legality:** Never embed/commit a copyrighted commercial ROM. Using freely-licensed homebrew only for tests. `.gitignore` excludes `*.gb`/`*.gbc`/`*.nes`/`*.wad` so ROM files and their generated C-array headers never get committed.
