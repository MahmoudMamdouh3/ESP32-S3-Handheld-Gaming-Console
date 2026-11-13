#pragma once

#include "sd_card.h"
#include <stddef.h>
#include <stdint.h>

namespace BmoSdInternal {

extern bool mounted;
extern uint32_t mountedClockHz;
extern RomFile* romList;
extern int maxCapacity;
extern int numRoms;
extern int skippedRomCount;
extern SDCard::ScanStats scanStats;
extern bool poolGrowthFailed;
extern int favoritesCount;
extern int romCountsByType[ROM_COLEM + 1];

bool ensurePoolCapacity(int targetCapacity);
void resetPathArena();
bool reserveExactPathArena(size_t totalBytes);
const char* internPath(const char* relPath, size_t length);
void appendArenaPathDirect(const char* src, size_t len, size_t offset);
const char* arenaBase();
int bakedRomCount();
void seedBakedRoms();
const uint8_t* tryLoadBakedRom(const char* filename, size_t* outSize);
bool isBakedRomPointer(const uint8_t* buffer);

} // namespace BmoSdInternal
