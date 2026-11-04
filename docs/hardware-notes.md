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

The standard blue SD breakout module (VCC/GND/MISO/MOSI/SCK/CS, 6-pin) has an onboard regulator + logic level shifter designed assuming **5V input**. Powering VCC from 3.3V leaves the regulator without enough headroom, causing the SD card to fail to mount.

**Wire SD module VCC to 5V (VBUS).** 
However, note the **MISO 5V Back-Powering Risk**: By powering the module with 5V, its logic-level shifter outputs a 5V signal back to the ESP32's `MISO` (GPIO 15) pin. The ESP32's internal clamping diodes dump this excess voltage onto the 3.3V power rail, instantly spiking it to ~4.4V. This overvoltage causes the internal Flash/PSRAM memory chips to instantly crash, resulting in a fatal boot loop (`entry 0x403c88b8` panic).

**Hardware Fix:** Either use a proper 3.3V-native MicroSD breakout, wire a 10k current-limiting resistor on the MISO line, or solder header pins to the 3.3V SD slot on the back of the generic red ST7789 display PCB instead of using the blue module.

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

## 6. Power & Battery Management (Handheld Conversion)

> [!WARNING]
> **Optional Feature (Disabled by Default - Hard Stop)**  
> The physical hardware runs on USB-C power. The `FEATURE_BATTERY_MONITOR` flag in `config.h` defaults to `0` and MUST be kept at `0` unless a verified voltage divider is soldered, otherwise reading floating GPIO1 triggers an ADC boot-loop.

To move away from bench/USB power and make the device a true handheld:
- **Power Architecture (AA / NiMH vs Li-ion):**
  - **NiMH / Alkaline AA Cells:** Use 4×AA cells (or 2×AA cells with a ≥1A boost converter to 5V feeding ESP32 VIN/5V and the SD module). NiMH cells must be removed and charged in an external dedicated NiMH charger. **DO NOT connect a TP4056 module to AA NiMH batteries** (TP4056 is strictly 4.2V CC/CV for 3.7V single-cell Lithium-ion).
  - **LiPo Alternative:** A 3.7V LiPo with a TP4056 module can be used instead of AAs.
  - **Dual USB / Battery Power Protection:** When combining battery power with USB programming power, place Schottky diodes on both 5V sources so they cannot back-feed each other.
- **Physical Switch:** Wire a physical SPST slide switch on the battery positive output *before* it reaches the ESP32 `Vin` or 5V rail. This ensures the battery can be fully disconnected from the load when powered off.
- **Battery Sensing (GPIO1):** When a physical voltage divider (via a 100k/100k resistor divider) is soldered to a battery, GPIO 1 can sample battery voltage.
- **Power Budget:** The ST7789 backlight, Octal PSRAM, and ESP32-S3 CPU running at 240MHz draw ~250mA total during active emulation. Ensure your power supply/batteries can supply ≥1A peak.

---

## 7. Audio Subsystem (I2S)

> [!WARNING]
> **Optional Feature (Disabled by Default)**
> The MAX98357A I2S DAC is not part of the baseline hardware spec. The `FEATURE_AUDIO` flag in `config.h` defaults to `0` and should remain disabled unless you have wired this specific amplifier.

- **MAX98357A I2S DAC:** The system uses I2S to stream digital audio to a MAX98357A class-D amplifier.
- **Pin Map:** `BCLK=38`, `LRC(WS)=39`, `DOUT=40`.
- **DMA Ring Buffer:** Audio is buffered into 4x512-byte DMA buffers. This blocks the emulator loop minimally when writing samples, naturally syncing the execution speed to 44.1kHz audio without requiring busy-wait timers.

---

## 8. Multi-Emulator Architecture & Optimizations

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

## 9. Display Color Quirks & ST7789

- **RGB vs BGR swapped colors:** Some generic ST7789 TFT panels are natively configured for BGR color order, while libraries like `Adafruit_ST7789` assume RGB. This results in Red and Blue channels being swapped (e.g., Mario's red overalls become blue, sky blue becomes orange). 
- **The Hardware Fix:** Instead of wasting CPU cycles trying to byte-swap the colors in software for every emulator and UI element, this can be solved instantly at initialization by manually sending an SPI command to toggle the BGR bit (`0x08`) in the `MADCTL` register (`0x36`).

---

## 10. UI Rendering Performance

- **Avoid Heavy GFX Drawing Loops:** Drawing complex or large amounts of basic geometry (like an animated background grid of `drawFastVLine` and `drawFastHLine`) in a 60FPS loop using standard GFX functions introduces massive CPU and SPI overhead. 
- **Optimization:** Keep menus visually simple. Removing the animated background grid and replacing solid filled shapes (`fillRoundRect`) with simple outlines (`drawRoundRect`) for the selected menu item drastically improved menu responsiveness and framerate.

---

## 11. Firmware Compilation & ROM Legality

- **Flash & PSRAM Modes (CRITICAL):** The ESP32-S3-WROOM-1 N16R8 module uses Quad SPI Flash and Octal SPI PSRAM. The Arduino IDE and `arduino-cli` MUST be set to `Flash Mode: QIO 80MHz` and `PSRAM: OPI PSRAM`. Setting Flash Mode to OPI causes an immediate cache panic / boot loop because the onboard Flash chip is Quad SPI.

- **Static Constraints & Partitions:** The default `arduino-cli` ESP32-S3 profile allocates a 1.2MB `app0` partition. Because our emulator cores exceed 2.5MB, compilation will fail at the linking stage with "text section exceeds available space". 
- **Fix:** You must compile the firmware using `PartitionScheme=custom` (`partitions.csv`), along with `PSRAM=opi` and `FlashMode=qio` configurations to correctly map the 16MB Quad Flash and 8MB Octal PSRAM hardware limits.

- **Baked ROM Limits (3MB Default Partition):** Baking multiple ~1MB ROMs (like Mario Deluxe and Zelda) via C headers can quickly exceed limits, resulting in compilation errors. The solution is correctly configuring the 16MB partition scheme in Arduino IDE, or simply relying on the SD card to load ROMs dynamically.

- **ROM legality:** Never embed/commit a copyrighted commercial ROM. Using freely-licensed homebrew only for tests. `.gitignore` excludes `*.gb`/`*.gbc`/`*.nes`/`*.wad` so ROM files and their generated C-array headers never get committed.
