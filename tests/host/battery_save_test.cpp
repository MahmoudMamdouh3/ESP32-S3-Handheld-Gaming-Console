#include "battery_save.h"

static_assert(BatterySave::gameBoyBatteryBytes(0x03, 0x03) == 0x8000, "MBC1 battery RAM size");
static_assert(BatterySave::gameBoyBatteryBytes(0x01, 0x03) == 0, "MBC1 volatile RAM is not persisted");
static_assert(BatterySave::gameBoyBatteryBytes(0x06, 0x00) == 512, "MBC2 battery RAM size");
static_assert(BatterySave::gameBoyBatteryBytes(0x22, 0x00) == 256, "MBC7 EEPROM window size");
static_assert(BatterySave::nesHeaderHasBattery(0x02), "iNES battery flag");
static_assert(!BatterySave::nesHeaderHasBattery(0x00), "iNES volatile RAM flag");
static_assert(BatterySave::snesLoRomBatteryBytes(3) == 8192, "SNES SRAM header size");
static_assert(BatterySave::snesLoRomBatteryBytes(0) == 0, "SNES no-SRAM header");
