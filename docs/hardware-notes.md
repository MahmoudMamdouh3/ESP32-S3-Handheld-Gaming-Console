# Hardware Notes & Lessons Learned

Board-specific facts and debugging lessons discovered during bring-up. Recorded here so hardware revisions and firmware extensions do not rediscover them through component failure or bootloops.

---

## 1. ESP32-S3-N16R8 Pin Restrictions & Input Architecture

- **GPIO 33–37** are reserved for octal PSRAM on this board (the "R8" in N16R8). Even though these are physically broken out on some development board headers, using them as regular GPIO will corrupt PSRAM. **Never use them.**

- **GPIO 0, 3, 45, 46** are strapping pins — they determine chip boot mode and supply voltages at power-on. Avoided for buttons, since holding one down at power-up could force the board into the wrong boot mode.

- **Hardware SPI (FSPI)**: Default shared bus pins are **SCK = GPIO 12, MOSI = GPIO 11, MISO = GPIO 15, Display CS = GPIO 10, SD CS = GPIO 13**.

- **Console Inputs (12 Buttons)**:
  - Base inputs (GPIO 0–31): `BTN_UP` = 4, `BTN_DOWN` = 5, `BTN_LEFT` = 6, `BTN_RIGHT` = 7, `BTN_A` = 16, `BTN_B` = 17, `BTN_START` = 18, `BTN_SELECT` = 21, `BTN_X` = 14. Polled atomically via `GPIO_IN_REG`.
  - Upper inputs (GPIO 32–48): `BTN_Y` = 42, `BTN_L` = 47, `BTN_R` = 48. Polled atomically via `GPIO_IN1_REG`.
  - Note: On many ESP32-S3-DevKitC-1 development boards, GPIO 48 also drives the onboard addressable RGB LED (WS2812). Verify signal integrity when wiring an external switch to GPIO 48.

---

## 2. Display: Software vs Hardware SPI

- `Adafruit_ST7789(cs, dc, mosi, sclk, rst)` (5-argument constructor) uses software/bit-banged SPI.
- The moment a second SPI device (MicroSD card) shares the same physical bus, software SPI on the display and the SD library's default hardware `SPI.begin()` call conflict over pins (GPIO 10 and GPIO 13). **Symptom:** The display goes fully black upon calling `SD.begin()`.
- **Fix:** Explicitly claim the SPI bus before initializing either peripheral, and use the hardware-SPI constructor:

  ```cpp
  SPI.begin(TFT_SCK, SD_MISO, TFT_MOSI, TFT_CS);
  Adafruit_ST7789 tft(&SPI, TFT_CS, TFT_DC, TFT_RST);
  // ...
  SD.begin(SD_CS, SPI);
  ```

---

## 3. SD Card Module Needs 5V, Not 3.3V

The standard blue SD breakout module (VCC/GND/MISO/MOSI/SCK/CS) has an onboard regulator and logic level shifter designed for **5V input**. Powering VCC from 3.3V leaves the regulator without sufficient dropout headroom, causing SD initialization failure.

**Wire SD module VCC to 5V (VBUS).**  
However, note the **MISO 5V Back-Powering Risk**: When powered from 5V, the breakout's level shifter outputs 5V logic on `MISO` (GPIO 15). The ESP32 internal clamping diodes shunt this excess voltage onto the internal 3.3V rail, spiking it to ~4.4V. This overvoltage causes Flash/PSRAM memory timing failure and an immediate boot loop (`entry 0x403c88b8` panic).

**Hardware Fix:** Either use a 3.3V-native MicroSD breakout, add a 10 kΩ current-limiting resistor on the MISO line, or solder header pins to the 3.3V SD slot on the back of the generic red ST7789 display PCB instead of using the blue module.

---

## 4. Breadboard Power Rail Splits

Many prototype breadboards have their positive and negative power rails **physically split in the middle**. Components powered on one half while the microcontroller supply is on the other half exhibit intermittent power and erratic signal levels. When assembling breadboard prototypes, always bridge both rail halves with jumpers.

---

## 5. Dual USB Ports (COM vs OTG)

On boards with dual USB-C ports ("COM/UART" and "USB/OTG"):
- Use the **COM/UART** port for flashing and serial monitoring. It connects through a dedicated USB-to-UART bridge IC.
- The native USB/OTG port requires `USB CDC On Boot: Enabled` in compile options for Serial Monitor output.

---

## 6. Power & Battery Management (Handheld Conversion)

> [!WARNING]
> **Hard Stop:** The physical console runs on bench/USB-C power. The `FEATURE_BATTERY_MONITOR` flag in `config.h` defaults to `0` and MUST remain `0` unless a physical voltage divider is verified soldered, otherwise reading floating GPIO 1 triggers an ADC boot loop.

When converting from USB bench supply to internal battery power:
- **AA NiMH Power Architecture:**
  - Configuration: 4× AA NiMH cells in series (~4.8V nominal), or 2× AA NiMH cells (~2.4V nominal) stepped up through a synchronous boost converter rated for ≥1A output at 5.0V feeding the devkit 5V pin and SD module.
  - Charging: NiMH cells must be removed from the chassis and recharged in a dedicated external NiMH charger.
  - **TP4056 Prohibition:** Never connect a TP4056 module to NiMH AA cells. The TP4056 charger IC implements a 4.2V CC/CV profile intended strictly for 3.7V single-cell Lithium-ion chemistry.
- **LiPo Alternative:** A 3.7V LiPo cell with a TP4056 charger and protection circuit can be used instead of AA cells.
- **Dual Supply Isolation:** When both battery power and USB-C programming power are connected simultaneously, Schottky diodes must be placed in series with each 5V source to prevent reverse current flow.
- **Physical Power Switch:** Wire an SPST slide switch on the battery positive lead before the regulator/devkit input.
- **Power Budget:** The ST7789 display backlight, Octal PSRAM, and ESP32-S3 CPU @ 240 MHz draw ~250 mA continuous during emulation, with transient current spikes reaching ~600 mA during SD reads. The battery supply must sustain ≥1A peak.

---

## 7. Audio Subsystem (I2S)

> [!WARNING]
> The MAX98357A I2S DAC is optional hardware. The `FEATURE_AUDIO` flag in `config.h` defaults to `0` and must remain disabled unless physical wiring is completed.

- **DAC:** MAX98357A mono Class-D I2S amplifier.
- **Pin Mapping:** `I2S_BCLK` = 38, `I2S_LRC` = 39, `I2S_DIN` = 40.
- **DMA Buffer:** Audio uses 4×512-byte DMA buffers. This stream naturally paces 60Hz emulation frames to 44.1 kHz audio output without busy-waiting.

---

## 8. Multi-Emulator Architecture & Supported Engines

The console hosts 14 production emulator engines:
1. **Peanut-GB**: Nintendo Game Boy (DMG)
2. **Walnut-CGB**: Nintendo Game Boy Color (CGB)
3. **Agnes**: Nintendo Entertainment System (NES)
4. **doomgeneric**: Classic DOOM (WAD)
5. **SMSPlus**: Sega Master System & Game Gear
6. **PCE-Core**: PC Engine / TurboGrafx-16
7. **Atari-Core**: Atari 2600
8. **PICO-8-Core**: PICO-8 runtime (draw-bytecode engine)
9. **Genesis-Core**: Sega Genesis / Mega Drive
10. **SNES-Core**: Super Nintendo Entertainment System
11. **WonderSwan-Core**: Bandai WonderSwan & WonderSwan Color
12. **NGP-Core**: SNK Neo Geo Pocket & Neo Geo Pocket Color
13. **Lynx-Core**: Atari Lynx
14. **ColecoVision-Core**: ColecoVision & Sega SG-1000

### Key Performance Patterns
- **Direct SPI writeBytes:** Uses `SPI.writeBytes()` rather than `pushColors()` to avoid per-pixel loop overhead. Palette tables pre-swap color bytes at compile/load time.
- **Frame Pacing:** `delay()` provides coarse FreeRTOS yielding, followed by `ets_delay_us()` hardware spin for sub-millisecond precision targeting exact video refresh rates (e.g. 16,742 µs for Game Boy).
- **Compiler Optimizations:** Emulator translation units apply `#pragma GCC optimize("O3,unroll-loops")`. Hot scanline loops use `IRAM_ATTR` and 32-byte cache-aligned data structures (`__attribute__((aligned(32)))`).

---

## 9. Display Color Order & Panel Initialization

- **RGB vs BGR Color Inversion:** ST7789 panels vary in factory color configuration. Panels configured for BGR display inverted red and blue channels (red appears blue, orange appears sky blue).
- **Correction:** The panel is configured at startup by setting the BGR bit (`0x08`) in the ST7789 `MADCTL` register (`0x36`) via an initialization command sequence, eliminating runtime software color conversion.

---

## 10. UI Rendering Performance

- **Eliminate Complex Geometry Loops:** Drawing animated background grids or dense vector geometry with standard GFX calls introduces bus and CPU bottlenecks.
- **Outline Focus Markers:** Using outline rounded rectangles (`drawRoundRect`) for menu selection rather than full redraws preserves frame rate and input responsiveness.

---

## 11. Firmware Compilation & Flash Settings

- **Flash & PSRAM Modes (CRITICAL):** The ESP32-S3-WROOM-1 N16R8 module contains Quad SPI Flash and Octal SPI PSRAM. The toolchain configuration MUST specify:
  - `Flash Mode: QIO 80MHz`
  - `PSRAM: OPI PSRAM`
  - Setting Flash Mode to OPI causes a flash cache fault and bootloop because the physical flash IC is Quad SPI.
- **Custom Partition Table:** The default 1.2MB `app0` partition is insufficient for 14 emulator cores and baked test ROMs. The project uses `PartitionScheme=custom` (`partitions.csv`), allocating an 8MB `app0` partition.
- **ROM Distribution:** Commercial copyrighted ROMs must never be committed to the repository. Free homebrew test ROMs and user-provided files loaded via MicroSD are used exclusively.

---

## 12. Enclosure & Perfboard Geometry (Chassis Reference)

The console chassis follows the canonical BMO form-factor:

### Physical Enclosure Parameters
- **Chassis Dimensions:** 8.8 cm width × 12.4 cm height × ~3.2 cm thickness.
- **Corner Radii:** 0.6 cm perimeter corner bevels.
- **Display Window:** 5.0 × 3.8 cm opening centered at `(4.4, 3.3) cm` from the front top-left edge, accommodating the 4.88 × 3.66 cm active area of the 2.4" ST7789 display.
- **Display PCB:** ~6.0 × 4.3 cm module body centered behind the front window.
- **D-Pad:** 2.8 × 2.8 cm (0.95 cm arm width) centered at `(2.2, 8.4) cm`, with tact switches spaced at ±1.0 cm from center.
- **ABXY Diamond:** Aligned square to the 2.54 mm perfboard grid (not tilted 15°). 0.9 cm diameter button caps with centers at:
  - **X (Top):** `(6.6, 7.4) cm`
  - **Y (Left):** `(5.6, 8.4) cm`
  - **A (Right):** `(7.6, 8.4) cm`
  - **B (Bottom):** `(6.6, 9.4) cm`
  - Spacing: 1.41 cm diagonal center-to-center distance; 0.51 cm clearance between adjacent caps.
- **Select & Start:** Horizontal stadium pills (1.4 × 0.5 cm) placed side-by-side at `(3.4, 11.0) cm` and `(5.4, 11.0) cm` on the lower-left beneath the D-pad cluster.
- **Shoulder Triggers (L / R):** 2.2 × 0.6 cm right-angle tact switches mounted directly along the top rim at `(1.7, 0.3) cm` (L) and `(7.1, 0.3) cm` (R).
- **Cutouts & Ports:**
  - Top Rim: USB-C opening (1.0 × 0.45 cm) centered at `(4.4, 0.0) cm`.
  - Left Flank: MicroSD card slot (1.3 × 0.25 cm) at `y ≈ 4.0 cm`.
  - Right Flank: Power SPST slide switch (1.0 × 0.4 cm) at `y ≈ 3.0 cm`.
- **Side Acoustic & Aesthetic Details:**
  - Left & Right Flanks: 7-hole acoustic speaker array patterns and vertical "B M O" lettering with arm socket ports.
- **Rear Chassis Features:**
  - Upper Back: 5 vertical cooling vent slits.
  - Middle Back: Raised access panel (3.4 × 6.2 cm door centered at `x = 4.4 cm, y = 9.0 cm`) secured by 4 corner screws.
  - Lower Back: Recessed horizontal finger latch pocket.

### Perfboard Sizing & Internal Stackup
- **Perfboard:** Standard 2.54 mm (0.1") pitch prototyping board trimmed to 31 × 46 holes (7.9 × 11.7 cm).
- **Component Placement:** Front face holds display panel, D-pad, ABXY switches, Select/Start switches, and top shoulder switches. Rear face holds ESP32-S3 module, boost converter, and battery holder.
- **Thickness Stackup (~3.2 cm total):**
  - Front shell: 0.20 cm
  - Switches & keycaps: 0.70 cm
  - Perfboard substrate: 0.16 cm
  - Solder clearance / standoffs: 0.20 cm
  - Battery compartment (AA holder): 1.70 cm
  - Rear shell: 0.20 cm
