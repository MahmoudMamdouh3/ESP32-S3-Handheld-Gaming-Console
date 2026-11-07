# Wiring - Display and Buttons

Board: **ESP32-S3-N16R8** (16MB Flash, 8MB Octal PSRAM)

## 1. Display (ST7789, 2.4", SPI, 240x320)

The console uses a 2.4" ST7789 SPI TFT display (240×320 physical, 320×240 landscape viewport) connected to the ESP32-S3 FSPI peripheral running at 80 MHz.

| Display Pin | ESP32-S3 Pin | Notes |
|---|---|---|
| VCC | 3V3 | 3.3V logic supply rail |
| GND | GND | Common ground rail |
| SCL / SCK | GPIO 12 | FSPI Clock (shared with MicroSD) |
| SDA / MOSI | GPIO 11 | FSPI Master Out Slave In (shared with MicroSD) |
| RES | GPIO 9 | Display hardware reset (`TFT_RST`) |
| DC | GPIO 8 | Data / Command control (`TFT_DC`) |
| CS | GPIO 10 | Display chip select (`TFT_CS`), active-low |
| BLK | 3V3 | Backlight LED anode (tied to 3.3V) |

---

## 2. Buttons (12x Active-Low Configuration)

The console features 12 tactile pushbuttons: 8 standard base controls (D-Pad, A, B, Start, Select — physically soldered on the current board) plus 4 extended controls (X, Y, L, R — supported in firmware, Wokwi, and the 3D simulator, pending physical soldering) for SNES, Genesis, and DOOM.

All buttons are wired active-low: **one terminal connects to the designated GPIO pin, the other terminal connects to common ground (GND)**. Internal pull-ups are configured in firmware (`INPUT_PULLUP`), so no external pull-up resistors are required. A pressed button reads as `LOW` (0), and a released (or unwired) button reads as `HIGH` (1).

| Button | Macro (`config.h`) | ESP32-S3 GPIO | Polling Register | Physical Function |
|---|---|---|---|---|
| Up | `BTN_UP` | GPIO 4 | `GPIO_IN_REG` | D-Pad Up |
| Down | `BTN_DOWN` | GPIO 5 | `GPIO_IN_REG` | D-Pad Down |
| Left | `BTN_LEFT` | GPIO 6 | `GPIO_IN_REG` | D-Pad Left |
| Right | `BTN_RIGHT` | GPIO 7 | `GPIO_IN_REG` | D-Pad Right |
| A | `BTN_A` | GPIO 16 | `GPIO_IN_REG` | Action A / Primary Confirm |
| B | `BTN_B` | GPIO 17 | `GPIO_IN_REG` | Action B / Cancel |
| Start | `BTN_START` | GPIO 18 | `GPIO_IN_REG` | Start / Pause |
| Select | `BTN_SELECT` | GPIO 21 | `GPIO_IN_REG` | Select / Menu Modifier |
| X | `BTN_X` | GPIO 14 | `GPIO_IN_REG` | SNES X / Genesis C / DOOM Next Weapon |
| Y | `BTN_Y` | GPIO 42 | `GPIO_IN1_REG` | SNES Y / DOOM Previous Weapon |
| L | `BTN_L` | GPIO 47 | `GPIO_IN1_REG` | Left Shoulder Trigger / DOOM Strafe Left |
| R | `BTN_R` | GPIO 48 | `GPIO_IN1_REG` | Right Shoulder Trigger / DOOM Strafe Right |

### Dual Register Polling Architecture
Because the ESP32-S3 has 49 physical GPIOs, its input register is split:
- **GPIO 0 through 31** are sampled via `GPIO_IN_REG`.
- **GPIO 32 through 48** (including Y on GPIO 42, L on GPIO 47, and R on GPIO 48) are sampled via `GPIO_IN1_REG`.

Firmware samples both registers atomically in `Buttons::update()` to eliminate multi-call overhead and latency in emulator loops.

---

## 3. Physical Layout & Perfboard Geometry (2.54 mm Grid)

The console is assembled on a single 2.54 mm (0.1") pitch prototype perfboard cut to 31 × 46 holes (7.9 × 11.7 cm). Elements align to the following reference coordinates measured from the top-left corner of the 8.8 × 12.4 cm front shell:

1. **Display Window:** 5.0 × 3.8 cm aperture centered at `(4.4, 3.3) cm`. Accommodates the 4.88 × 3.66 cm active area of the 2.4" ST7789 module.
2. **D-Pad Cluster:** 2.8 × 2.8 cm total span, 0.95 cm arm width, centered at `(2.2, 8.4) cm`. Uses 4 tactile switches located at ±1.0 cm (4 perfboard holes = 1.016 cm) from center.
3. **ABXY Diamond:** Aligned square to the 2.54 mm perfboard grid (not rotated). Cap diameter 0.9 cm. Centers:
   - **X (Top):** `(6.6, 7.4) cm`
   - **Y (Left):** `(5.6, 8.4) cm`
   - **A (Right):** `(7.6, 8.4) cm`
   - **B (Bottom):** `(6.6, 9.4) cm`
   - Spacing: 1.41 cm diagonal center-to-center distance, leaving a 0.51 cm clearance gap between neighboring caps.
4. **Select & Start:** Horizontal stadium pills (1.4 × 0.5 cm) placed side-by-side at `(3.4, 11.0) cm` and `(5.4, 11.0) cm` on the lower-left beneath the D-pad cluster.
5. **L / R Shoulder Buttons:** 2.2 × 0.6 cm side-actuated (right-angle) tactile switches mounted directly along the top rim at `(1.7, 0.3) cm` (L) and `(7.1, 0.3) cm` (R).

### Tactile Switch Wiring Notes
- **4-Leg Tactile Switches (OMRON 12×12 mm or 6×6 mm):** These switches have 4 physical pins but only 2 distinct electrical nodes. Opposite pairs on the same side of the switch body are internally connected. Wire one pin from one side to the GPIO, and one pin from the opposite side to GND. The other two pins provide mechanical anchorage on the perfboard.
- **2-Pin Right-Angle Switches (Shoulders):** One terminal solders to GPIO 47 / 48, the other solders to the top ground bus.

---

## 4. Reserved Pins & Board Restrictions

- **GPIO 33–37 (Octal PSRAM):** Dedicated internally to the ESP32-S3-WROOM-1 N16R8 Octal SPI PSRAM. Never assign, pull, or route these pins.
- **GPIO 0, 3, 45, 46 (Boot Strapping):** Strapping pins that dictate chip boot mode and voltage at reset. Kept unconnected to buttons to prevent unintended boot states when keys are held at startup.
- **GPIO 1 (Battery ADC):** Reserved for optional voltage divider. Gated by `FEATURE_BATTERY_MONITOR = 0` (must remain 0 when unwired to avoid floating ADC boot loops).
- **GPIO 38, 39, 40 (I2S Audio):** Reserved for optional MAX98357A I2S DAC (`BCLK=38, LRC=39, DIN=40`). Gated by `FEATURE_AUDIO = 0`.
- **GPIO 43, 44 (I2C Bus):** Reserved for optional FM24C FRAM save memory (`SDA=43, SCL=44`).

---

## 5. MicroSD Storage Bus Integration

The MicroSD card slot connects to the same hardware SPI bus as the display (FSPI):
- **Clock:** GPIO 12 (`TFT_SCK`)
- **Data Out:** GPIO 11 (`TFT_MOSI`)
- **Data In:** GPIO 15 (`SD_MISO`)
- **Chip Select:** GPIO 13 (`SD_CS`)
- **Power:** 5V VBUS (due to onboard regulator on standard modules)

Both peripherals share the bus under the arbitration of `SpiArbiter`. See [02-sd-card.md](file:///e:/BMO%20Gameboy/docs/wiring/02-sd-card.md) for complete details.
