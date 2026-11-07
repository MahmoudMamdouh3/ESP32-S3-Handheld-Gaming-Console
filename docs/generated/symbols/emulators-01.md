# Firmware symbols: emulators (1/1)

Generated from the declarations in the listed headers. Edit the header, then run `python -m tools.guardian symbols`.

## `firmware/BmoGameboy/src/emulators/emu_atari.h`

- `class AtariEmu`
- `static bool begin(const uint8_t* romData, size_t romSize);`
- `static void updateJoypad();`
- `static void runFrame();`
- `static void destroy();`
- `static bool saveBatteryRam(const char* romFilename);`
- `static bool loadBatteryRam(const char* romFilename);`
- `static bool saveState(const char* romFilename, int slot);`
- `static bool loadState(const char* romFilename, int slot);`

## `firmware/BmoGameboy/src/emulators/emu_colem.h`

- `class ColemEmu`
- `static bool init(uint8_t* romData, size_t romSize);`
- `static void update();`
- `static void destroy();`
- `static bool isRunning();`
- `static bool saveBatteryRam(const char* romFilename);`
- `static bool loadBatteryRam(const char* romFilename);`
- `static bool saveState(const char* romFilename, int slot);`
- `static bool loadState(const char* romFilename, int slot);`

## `firmware/BmoGameboy/src/emulators/emu_doom.h`

- `namespace DoomEmu`
- `const char* lastError();`
- `bool begin(const char* wadPath);`
- `void runFrame();`
- `void destroy();`

## `firmware/BmoGameboy/src/emulators/emu_genesis.h`

- `class GenesisEmu`
- `static bool init(uint8_t* romData, size_t romSize);`
- `static void update();`
- `static void destroy();`
- `static bool isRunning();`
- `static bool saveBatteryRam(const char* romFilename);`
- `static bool loadBatteryRam(const char* romFilename);`
- `static bool saveState(const char* romFilename, int slot);`
- `static bool loadState(const char* romFilename, int slot);`

## `firmware/BmoGameboy/src/emulators/emu_lynx.h`

- `class LynxEmu`
- `static bool init(uint8_t* romData, size_t romSize);`
- `static void update();`
- `static void destroy();`
- `static bool isRunning();`
- `static bool saveBatteryRam(const char* romFilename);`
- `static bool loadBatteryRam(const char* romFilename);`
- `static bool saveState(const char* romFilename, int slot);`
- `static bool loadState(const char* romFilename, int slot);`

## `firmware/BmoGameboy/src/emulators/emu_nes.h`

- `class NesEmu`
- `static bool begin(const uint8_t* romData, size_t romSize);`
- `static void updateJoypad();`
- `static void runFrame();`
- `static void destroy();`
- `static bool saveBatteryRam(const char* romFilename);`
- `static bool loadBatteryRam(const char* romFilename);`
- `static bool saveState(const char* romFilename, int slot);`
- `static bool loadState(const char* romFilename, int slot);`

## `firmware/BmoGameboy/src/emulators/emu_ngp.h`

- `class NGPEmu`
- `static bool init(uint8_t* romData, size_t romSize, bool isColor);`
- `static void update();`
- `static void destroy();`
- `static bool isRunning();`
- `static bool saveBatteryRam(const char* romFilename);`
- `static bool loadBatteryRam(const char* romFilename);`
- `static bool saveState(const char* romFilename, int slot);`
- `static bool loadState(const char* romFilename, int slot);`

## `firmware/BmoGameboy/src/emulators/emu_pce.h`

- `class PceEmu`
- `static bool begin(const uint8_t* romData, size_t romSize);`
- `static void updateJoypad();`
- `static void runFrame();`
- `static void destroy();`
- `static bool saveBatteryRam(const char* romFilename);`
- `static bool loadBatteryRam(const char* romFilename);`
- `static bool saveState(const char* romFilename, int slot);`
- `static bool loadState(const char* romFilename, int slot);`

## `firmware/BmoGameboy/src/emulators/emu_peanut.h`

- `namespace PeanutEmu`
- `bool begin(const uint8_t* rom_data, size_t rom_len);`
- `void updateJoypad();`
- `void runFrame();`
- `void destroy();`
- `bool saveBatteryRam(const char* romFilename);`
- `bool loadBatteryRam(const char* romFilename);`
- `bool saveState(const char* romFilename, int slot);`
- `bool loadState(const char* romFilename, int slot);`

## `firmware/BmoGameboy/src/emulators/emu_pico.h`

- `class PicoEmu`
- `static bool begin(const uint8_t* romData, size_t romSize);`
- `static void updateJoypad();`
- `static void runFrame();`
- `static void destroy();`
- `static bool saveBatteryRam(const char* romFilename);`
- `static bool loadBatteryRam(const char* romFilename);`
- `static bool saveState(const char* romFilename, int slot);`
- `static bool loadState(const char* romFilename, int slot);`

## `firmware/BmoGameboy/src/emulators/emu_sms.h`

- `class SmsEmu`
- `static bool begin(const uint8_t* romData, size_t romSize, bool isGameGear = false);`
- `static void updateJoypad();`
- `static void runFrame();`
- `static void destroy();`
- `static bool saveBatteryRam(const char* romFilename);`
- `static bool loadBatteryRam(const char* romFilename);`
- `static bool saveState(const char* romFilename, int slot);`
- `static bool loadState(const char* romFilename, int slot);`

## `firmware/BmoGameboy/src/emulators/emu_snes.h`

- `class SNESEmu`
- `static bool init(uint8_t* romData, size_t romSize);`
- `static void update();`
- `static void destroy();`
- `static bool isRunning();`
- `static bool saveBatteryRam(const char* romFilename);`
- `static bool loadBatteryRam(const char* romFilename);`
- `static bool saveState(const char* romFilename, int slot);`
- `static bool loadState(const char* romFilename, int slot);`

## `firmware/BmoGameboy/src/emulators/emu_walnut.h`

- `namespace WalnutEmu`
- `bool begin(const uint8_t* rom_data, size_t rom_len);`
- `void updateJoypad();`
- `void runFrame();`
- `void destroy();`
- `bool saveBatteryRam(const char* romFilename);`
- `bool loadBatteryRam(const char* romFilename);`
- `bool saveState(const char* romFilename, int slot);`
- `bool loadState(const char* romFilename, int slot);`

## `firmware/BmoGameboy/src/emulators/emu_wswan.h`

- `class WSwanEmu`
- `static bool init(uint8_t* romData, size_t romSize, bool isColor);`
- `static void update();`
- `static void destroy();`
- `static bool isRunning();`
- `static bool saveBatteryRam(const char* romFilename);`
- `static bool loadBatteryRam(const char* romFilename);`
- `static bool saveState(const char* romFilename, int slot);`
- `static bool loadState(const char* romFilename, int slot);`

## `firmware/BmoGameboy/src/emulators/psram_zone_budget.h`

- `namespace DoomEmu`
- `candidate &= ~static_cast<size_t>(65535);`
