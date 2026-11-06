# Wiring - MicroSD Storage (Shared FSPI Bus + 12 Buttons)

Board: **ESP32-S3-N16R8** (16MB Flash, 8MB Octal PSRAM)

## 1. Overview & Bus Topology

The console integrates a MicroSD card socket on the same hardware SPI peripheral (`FSPI`) as the ST7789 display. By sharing clock (`SCK`) and data output (`MOSI`) lines while maintaining independent chip selects (`TFT_CS` and `SD_CS`), pin utilization is minimized while preserving full 80 MHz display blits and 25 MHz SD transfers under `SpiArbiter` mutex lock.

---

## 2. MicroSD Card Breakout Module (6-Pin SPI)

| SD Module Pin | ESP32-S3 Pin | Line Role | Electrical Notes |
|---|---|---|---|
| VCC | **5V** (VBUS) | Power Supply | Standard blue breakouts integrate a 3.3V LDO regulator that drops out if supplied with 3.3V. Must receive 5V. |
| GND | GND | Ground | Common ground rail. |
| SCK / CLK | GPIO 12 | FSPI Clock | **Shared** with display `TFT_SCK`. Clocked at 25 MHz for SD transfers. |
| MOSI / DI | GPIO 11 | Master Out Slave In | **Shared** with display `TFT_MOSI`. Host data into card. |
| MISO / DO | GPIO 15 | Master In Slave Out | **Dedicated** SD data output line back to ESP32 (`SD_MISO`). Display is write-only. |
| CS / SS | GPIO 13 | Chip Select | **Dedicated** SD chip select line (`SD_CS`), active-low. |

### Shared SPI Bus Initialization Contract
Both the display and MicroSD card share physical lines `SCK=12` and `MOSI=11`. The firmware must explicitly configure pin routing prior to initializing either peripheral:

```cpp
SPI.begin(TFT_SCK, SD_MISO, TFT_MOSI, TFT_CS);
```

Failing to pass explicit pin parameters causes `SD.begin()` to claim default ESP32-S3 SPI pins, causing bus contention and blanking the ST7789 display.

### MISO 5V Back-Powering Risk & Protection
Many commercial 5V MicroSD breakouts feature onboard 5V-to-3.3V level-shifting buffers that output 5V CMOS logic on `MISO` (GPIO 15) when powered by 5V. 
The ESP32-S3 internal clamping diodes shunt this excess voltage into the 3.3V internal rail, elevating it to ~4.4V. This overvoltage causes internal Flash/PSRAM timing breakdown, manifesting as an immediate boot loop (`entry 0x403c88b8` cache panic).

**Recommended Hardware Protection:**
1. Use a native 3.3V MicroSD socket without an active level shifter; or
2. Solder directly to the 3.3V SD socket contacts on the back of the generic red ST7789 PCB; or
3. Place a 10 kΩ series resistor or Schottky diode clamp on the `MISO` line (GPIO 15) to limit back-feed current.

---

## 3. Display Pinout (Shared FSPI Bus)

The display operates at 80 MHz on the shared FSPI bus:

| Display Pin | ESP32-S3 Pin | Macro (`config.h`) | Electrical Notes |
|---|---|---|---|
| VCC | 3V3 | — | 3.3V logic supply rail |
| GND | GND | — | Common ground rail |
| SCL / SCK | GPIO 12 | `TFT_SCK` | Shared with SD Card clock |
| SDA / MOSI | GPIO 11 | `TFT_MOSI` | Shared with SD Card data in |
| RES | GPIO 9 | `TFT_RST` | Hardware reset, active-low |
| DC | GPIO 8 | `TFT_DC` | Command (LOW) / Data (HIGH) |
| CS | GPIO 10 | `TFT_CS` | Display chip select, active-low |
| BLK | 3V3 | — | Backlight LED anode (3.3V) |

---

## 4. Complete Console Button Map (12 Buttons)

All 12 inputs are configured with internal pull-ups (`INPUT_PULLUP`) and switch directly to GND on contact closure.

| Button | Macro (`config.h`) | ESP32-S3 GPIO | Polling Register | Hardware Assignment |
|---|---|---|---|---|
| Up | `BTN_UP` | GPIO 4 | `GPIO_IN_REG` | D-Pad Up |
| Down | `BTN_DOWN` | GPIO 5 | `GPIO_IN_REG` | D-Pad Down |
| Left | `BTN_LEFT` | GPIO 6 | `GPIO_IN_REG` | D-Pad Left |
| Right | `BTN_RIGHT` | GPIO 7 | `GPIO_IN_REG` | D-Pad Right |
| A | `BTN_A` | GPIO 16 | `GPIO_IN_REG` | Action A / Confirm |
| B | `BTN_B` | GPIO 17 | `GPIO_IN_REG` | Action B / Cancel |
| Start | `BTN_START` | GPIO 18 | `GPIO_IN_REG` | Start / Pause |
| Select | `BTN_SELECT` | GPIO 21 | `GPIO_IN_REG` | Select / Modifier |
| X | `BTN_X` | GPIO 14 | `GPIO_IN_REG` | Extended Face Button (SNES X, Genesis C, DOOM Next Weapon) |
| Y | `BTN_Y` | GPIO 42 | `GPIO_IN1_REG` | Extended Face Button (SNES Y, DOOM Prev Weapon) |
| L | `BTN_L` | GPIO 47 | `GPIO_IN1_REG` | Left Shoulder Trigger (SNES L, DOOM Strafe Left) |
| R | `BTN_R` | GPIO 48 | `GPIO_IN1_REG` | Right Shoulder Trigger (SNES R, DOOM Strafe Right) |

> [!NOTE]
> Inputs on GPIO 32 and above (`BTN_Y` = 42, `BTN_L` = 47, `BTN_R` = 48) reside in the upper register `GPIO_IN1_REG`. Firmware samples both `GPIO_IN_REG` and `GPIO_IN1_REG` atomically in `Buttons::update()`.
