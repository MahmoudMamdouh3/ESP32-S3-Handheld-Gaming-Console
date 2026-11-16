// SPDX-License-Identifier: MIT OR GPL-2.0-or-later
#include "rom_catalog_index.h"

#include "../hal/config.h"
#include "../hal/spi_arbiter.h"
#include "atomic_fs.h"
#include "popular_games_table.h"
#include "rom_index.h"
#include "rom_path_match.h"
#include "sd_internal.h"
#include "sd_scanner.h"
#if FEATURE_SD_CARD
#include <SD.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {

int compareRoms(const void* a, const void* b) {
  const RomFile* ra = static_cast<const RomFile*>(a);
  const RomFile* rb = static_cast<const RomFile*>(b);
  const char* fa = ra->filename ? ra->filename : "";
  const char* fb = rb->filename ? rb->filename : "";
  const char* sa = strrchr(fa, '/');
  const char* sb = strrchr(fb, '/');
  sa = sa ? (sa + 1) : fa;
  sb = sb ? (sb + 1) : fb;
  const int cmp = strcasecmp(sa, sb);
  return (cmp != 0) ? cmp : strcasecmp(fa, fb);
}

} // namespace

void SDCard::sortRoms() {
  using namespace BmoSdInternal;
  const int bakedCount = bakedRomCount();
  if (numRoms > bakedCount) {
    qsort(&romList[bakedCount], static_cast<size_t>(numRoms - bakedCount), sizeof(RomFile), compareRoms);
  }
}

int SDCard::getRomCount() {
  return BmoSdInternal::numRoms;
}

int SDCard::getRomCountForType(RomType type) {
  if (type == ROM_FAVORITES) {
    return getFavoritesCount();
  }
  if (type <= ROM_COLEM) return BmoSdInternal::romCountsByType[type];
  return 0;
}

const RomFile* SDCard::findRomByPath(const char* relativePath) {
  using namespace BmoSdInternal;
  if (!relativePath || !relativePath[0]) return nullptr;
  for (int i = 0; i < numRoms; ++i) {
    if (romList[i].filename && RomPathMatch::equalsNormalized(romList[i].filename, relativePath)) {
      return &romList[i];
    }
  }
  return nullptr;
}

uint32_t SDCard::findRomFileSize(const char* relativePath) {
  const RomFile* rf = findRomByPath(relativePath);
  return rf ? rf->fileSize : 0U;
}

bool SDCard::verifyRomIdentity(const RomFile* rom, const uint8_t* loadedBuffer, size_t loadedSize) {
  if (!rom || !rom->filename || rom->filename[0] == '\0') return false;
  if (loadedBuffer && BmoSdInternal::isBakedRomPointer(loadedBuffer)) {
    return true;
  }
  if (loadedBuffer != nullptr) {
    if (rom->fileSize != 0U && loadedSize != rom->fileSize) return false;
    if (rom->crc32Prefix != 0U) {
      const uint32_t actualPrefix = BmoSdScanner::computePrefixCrc32FromBuffer(loadedBuffer, loadedSize);
      if (actualPrefix != rom->crc32Prefix) return false;
    }
    return true;
  }
#if FEATURE_SD_CARD
  if (!BmoSdInternal::mounted) return false;
  char path[SDCard::MAX_ROM_PATH_LEN + 2U];
  const char* clean = RomPathMatch::normalizeRelative(rom->filename);
  const int wrote = snprintf(path, sizeof(path), "/%s", clean);
  if (wrote <= 1 || static_cast<size_t>(wrote) >= sizeof(path)) return false;
  SpiArbiter::ScopedLock spiLock;
  File f = SD.open(path, FILE_READ);
  if (!f || f.isDirectory()) {
    if (f) f.close();
    return false;
  }
  const uint32_t actualSize = static_cast<uint32_t>(f.size());
  if (actualSize == 0U || (rom->fileSize != 0U && actualSize != rom->fileSize)) {
    f.close();
    return false;
  }
  bool prefixOk = true;
  if (rom->crc32Prefix != 0U) {
    const uint32_t actualPrefix = BmoSdScanner::computePrefixCrc32FromFile(f, BmoIndex::ROM_PREFIX_HASH_BYTES, nullptr,
                                                                           nullptr, false, nullptr, nullptr);
    prefixOk = (actualPrefix == rom->crc32Prefix);
  }
  f.close();
  return prefixOk;
#else
  return true;
#endif
}

bool SDCard::isPopularGame(const char* filename, RomType type) {
  return PopularGamesTable::matchesPopularKeyword(filename, type);
}

int SDCard::getPopularCountForType(RomType type) {
  using namespace BmoSdInternal;
  int count = 0;
  for (int i = 0; i < numRoms; ++i) {
    if (romList[i].type == type && romList[i].isPopular) {
      count++;
    }
  }
  return count;
}

const RomFile* SDCard::getRomInfo(int index) {
  using namespace BmoSdInternal;
  if (index < 0 || index >= numRoms) return nullptr;
  return &romList[index];
}
