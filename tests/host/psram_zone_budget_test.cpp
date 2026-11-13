#include "../../firmware/BmoGameboy/src/cores/psram_zone_budget.h"
#include "../../firmware/BmoGameboy/src/storage/sd_card.h"

static_assert(sizeof(int) == 4, "ESP32-S3 int ABI changed");
static_assert(sizeof(void*) == 4, "ESP32-S3 pointer ABI changed");
static_assert(DoomEmu::computeZoneBytes(DoomEmu::DOOM_ZONE_FLOOR_BYTES +
                                        DoomEmu::DOOM_ZONE_MARGIN_BYTES - 1) == 0,
              "Zone allocation must reject a block below the floor plus margin");
static_assert(DoomEmu::computeZoneBytes(DoomEmu::DOOM_ZONE_FLOOR_BYTES +
                                        DoomEmu::DOOM_ZONE_MARGIN_BYTES) ==
                  DoomEmu::DOOM_ZONE_FLOOR_BYTES,
              "Zone allocation must accept the exact floor boundary");
static_assert(DoomEmu::computeZoneBytes(DoomEmu::DOOM_ZONE_FLOOR_BYTES +
                                        DoomEmu::DOOM_ZONE_MARGIN_BYTES + 65535) ==
                  DoomEmu::DOOM_ZONE_FLOOR_BYTES,
              "Zone allocation must round down to 64 KiB");
static_assert(DoomEmu::computeZoneBytes(32 * 1024 * 1024) == DoomEmu::DOOM_ZONE_CAP_BYTES,
              "Zone allocation must respect its configured cap");
static_assert(SDCard::computePoolCapacity(2000) == 2500,
              "Catalog pool headroom must be 25 percent");
static_assert(SDCard::computePoolCapacity(2001) == 2502,
              "Catalog pool headroom must round up");
static_assert(SDCard::computePoolCapacity(40000) == SDCard::MAX_ROM_POOL,
              "Catalog pool size must respect the maximum");

int main() { return 0; }
