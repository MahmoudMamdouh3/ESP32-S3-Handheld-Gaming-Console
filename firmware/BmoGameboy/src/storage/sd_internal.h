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
extern bool poolGrowthFailed;
extern int favoritesCount;
extern int romCountsByType[ROM_COLEM + 1];

bool ensurePoolCapacity(int targetCapacity);
int bakedRomCount();
void seedBakedRoms();
const uint8_t* tryLoadBakedRom(const char* filename, size_t* outSize);
bool isBakedRomPointer(const uint8_t* buffer);

} // namespace BmoSdInternal
