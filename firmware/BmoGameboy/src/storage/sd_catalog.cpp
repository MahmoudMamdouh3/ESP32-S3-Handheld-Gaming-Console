#include "sd_internal.h"
#include "../hal/config.h"
#include "../hal/spi_arbiter.h"
#include "../ui/console_catalog.h"
#include "atomic_fs.h"
#include "box_art.h"
#include "rom_file_filter.h"
#include "rom_index.h"
#include "sd_atomic_ops.h"
#if FEATURE_SD_CARD
#include <SD.h>
#endif
#include <esp_heap_caps.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

namespace {

static_assert(sizeof(RomFile) == sizeof(BmoIndexEntry), "RomFile and BmoIndexEntry size parity");
static_assert(offsetof(RomFile, fileSize) == offsetof(BmoIndexEntry, fileSize), "ROM index file-size offset parity");

int compareRoms(const void* a, const void* b) {
  const RomFile* ra = (const RomFile*)a;
  const RomFile* rb = (const RomFile*)b;
  const char* sa = strrchr(ra->filename, '/');
  const char* sb = strrchr(rb->filename, '/');
  sa = sa ? (sa + 1) : ra->filename;
  sb = sb ? (sb + 1) : rb->filename;
  const int cmp = strcasecmp(sa, sb);
  return (cmp != 0) ? cmp : strcasecmp(ra->filename, rb->filename);
}

bool isIgnoredDirName(const char* baseName) {
  if (!baseName || baseName[0] == '\0' || baseName[0] == '.') return true;
  if (strcasecmp(baseName, "saves") == 0 || strcasecmp(baseName, "screenshots") == 0 ||
      strcasecmp(baseName, "boxart") == 0 || strcasecmp(baseName, "covers") == 0 ||
      strcasecmp(baseName, "System Volume Information") == 0 || strcasecmp(baseName, "LOST.DIR") == 0) {
    return true;
  }
  return false;
}

uint32_t rootDirectoryStamp() {
#if FEATURE_SD_CARD
  File root = SD.open("/");
  if (!root || !root.isDirectory()) {
    if (root) root.close();
    return 0;
  }
  const time_t stamp = root.getLastWrite();
  root.close();
  return stamp > 0 ? (uint32_t)stamp : 0;
#else
  return 0;
#endif
}

bool verifyIndexFile(const char* path, size_t expectedSize, uint32_t expectedCrc) {
#if FEATURE_SD_CARD
  File file = SD.open(path, FILE_READ);
  if (!file) return false;
  if (file.size() != expectedSize) {
    file.close();
    return false;
  }
  BmoIndexHeader header;
  if (file.read((uint8_t*)&header, sizeof(header)) != sizeof(header) ||
      strncmp(header.magic, BMO_INDEX_MAGIC, sizeof(header.magic)) != 0 || header.version != BMO_INDEX_VERSION ||
      header.entrySize != sizeof(BmoIndexEntry) || header.crc32 != expectedCrc) {
    file.close();
    return false;
  }
  uint8_t buffer[256];
  size_t remaining = (size_t)header.entryCount * sizeof(BmoIndexEntry);
  uint32_t crc = 0xFFFFFFFFU;
  while (remaining > 0) {
    const size_t request = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
    const int got = file.read(buffer, request);
    if (got <= 0 || (size_t)got > request) {
      file.close();
      return false;
    }
    crc = AtomicFs::crc32Update(buffer, (size_t)got, crc);
    remaining -= (size_t)got;
  }
  file.close();
  return crc == expectedCrc;
#else
  (void)path;
  (void)expectedSize;
  (void)expectedCrc;
  return false;
#endif
}

bool indexFileIsValid(const char* path) {
#if FEATURE_SD_CARD
  File file = SD.open(path, FILE_READ);
  if (!file) return false;
  const size_t fileSize = file.size();
  BmoIndexHeader header;
  const bool headerRead = file.read((uint8_t*)&header, sizeof(header)) == sizeof(header);
  file.close();
  if (!headerRead || strncmp(header.magic, BMO_INDEX_MAGIC, sizeof(header.magic)) != 0 ||
      header.version != BMO_INDEX_VERSION || header.entrySize != sizeof(BmoIndexEntry) ||
      header.entryCount > (SIZE_MAX - sizeof(BmoIndexHeader)) / sizeof(BmoIndexEntry)) {
    return false;
  }
  const size_t expectedSize = sizeof(BmoIndexHeader) + (size_t)header.entryCount * sizeof(BmoIndexEntry);
  return fileSize == expectedSize && verifyIndexFile(path, expectedSize, header.crc32);
#else
  (void)path;
  return false;
#endif
}

void recoverIndexTransaction() {
#if FEATURE_SD_CARD
  const bool hasIndex = SD.exists(BMO_INDEX_PATH);
  const bool hasBackup = SD.exists(BMO_INDEX_BACKUP_PATH);
  if (hasIndex && indexFileIsValid(BMO_INDEX_PATH)) {
    if (hasBackup) SD.remove(BMO_INDEX_BACKUP_PATH);
  } else if (hasBackup && indexFileIsValid(BMO_INDEX_BACKUP_PATH)) {
    if (hasIndex) SD.remove(BMO_INDEX_PATH);
    SD.rename(BMO_INDEX_BACKUP_PATH, BMO_INDEX_PATH);
  }
  if (SD.exists(BMO_INDEX_TEMP_PATH)) SD.remove(BMO_INDEX_TEMP_PATH);
#endif
}

} // namespace

void SDCard::sortRoms() {
  using namespace BmoSdInternal;
  const int bakedCount = bakedRomCount();
  if (numRoms > bakedCount) {
    qsort(&romList[bakedCount], numRoms - bakedCount, sizeof(RomFile), compareRoms);
  }
}

bool SDCard::loadIndex() {
#if FEATURE_SD_CARD
  using namespace BmoSdInternal;
  if (!SD.exists(BMO_INDEX_PATH)) return false;

  File f = SD.open(BMO_INDEX_PATH, FILE_READ);
  if (!f) return false;

  BmoIndexHeader header;
  if (f.read((uint8_t*)&header, sizeof(header)) != sizeof(header)) {
    f.close();
    return false;
  }

  const int bakedCount = bakedRomCount();
  if (strncmp(header.magic, BMO_INDEX_MAGIC, 8) != 0 || header.version != BMO_INDEX_VERSION ||
      header.entrySize != sizeof(BmoIndexEntry) || header.entryCount == 0 ||
      header.entryCount > (uint32_t)(MAX_ROM_POOL - bakedCount) || header.timestamp == 0 ||
      header.timestamp != rootDirectoryStamp()) {
    f.close();
    return false;
  }

  const int targetCap = computePoolCapacity((int)header.entryCount + bakedCount);
  if (!ensurePoolCapacity(targetCap)) {
    f.close();
    return false;
  }

  const size_t expectedFileSize = sizeof(BmoIndexHeader) + header.entryCount * sizeof(BmoIndexEntry);
  if (f.size() != expectedFileSize) {
    f.close();
    return false;
  }

  const size_t entriesBytes = header.entryCount * sizeof(BmoIndexEntry);
  BmoIndexEntry* entries = (BmoIndexEntry*)heap_caps_malloc(entriesBytes, MALLOC_CAP_SPIRAM);
  if (!entries) {
    f.close();
    return false;
  }

  if (f.read((uint8_t*)entries, entriesBytes) != entriesBytes) {
    heap_caps_free(entries);
    f.close();
    return false;
  }
  f.close();

  const uint32_t computedCrc = AtomicFs::crc32Update((const uint8_t*)entries, entriesBytes, 0xFFFFFFFFU);
  if (computedCrc != header.crc32) {
    heap_caps_free(entries);
    return false;
  }

  uint64_t totalFileBytes = 0;
  for (uint32_t i = 0; i < header.entryCount; ++i) {
    if (entries[i].type < ROM_GB || entries[i].type > ROM_COLEM || entries[i].isFavorite > 1 ||
        entries[i].hasBoxArt > 1 || entries[i].reserved != 0 || entries[i].filename[0] == '\0' ||
        memchr(entries[i].filename, '\0', sizeof(entries[i].filename)) == nullptr) {
      heap_caps_free(entries);
      return false;
    }
    totalFileBytes += entries[i].fileSize;
  }
  if ((uint32_t)totalFileBytes != header.reserved[0] ||
      BmoIndex::catalogFingerprint(header.entryCount, totalFileBytes, header.timestamp, header.crc32) !=
          header.reserved[1]) {
    heap_caps_free(entries);
    return false;
  }

  for (uint32_t i = 0; i < header.entryCount && numRoms < maxCapacity; ++i) {
    const BmoIndexEntry& e = entries[i];
    strncpy(romList[numRoms].filename, e.filename, 63);
    romList[numRoms].filename[63] = '\0';
    romList[numRoms].type = (RomType)e.type;
    romList[numRoms].isFavorite = (e.isFavorite != 0);
    if (romList[numRoms].isFavorite) {
      favoritesCount++;
    }
    romList[numRoms].hasBoxArt = (e.hasBoxArt != 0);
    romList[numRoms].isPopular = SDCard::isPopularGame(romList[numRoms].filename, romList[numRoms].type);
    romList[numRoms].fileSize = e.fileSize;
    if (romList[numRoms].type <= ROM_COLEM) {
      romCountsByType[romList[numRoms].type]++;
    }
    numRoms++;
  }

  heap_caps_free(entries);
  return true;
#else
  return false;
#endif
}

bool SDCard::saveIndex() {
#if FEATURE_SD_CARD
  using namespace BmoSdInternal;
  const int bakedCount = bakedRomCount();
  if (numRoms <= bakedCount) return false;

  const uint32_t entryCount = (uint32_t)(numRoms - bakedCount);
  const size_t entriesBytes = entryCount * sizeof(BmoIndexEntry);
  BmoIndexEntry* entries = (BmoIndexEntry*)heap_caps_malloc(entriesBytes, MALLOC_CAP_SPIRAM);
  if (!entries) return false;

  for (uint32_t i = 0; i < entryCount; ++i) {
    const RomFile& rf = romList[bakedCount + (int)i];
    strncpy(entries[i].filename, rf.filename, 63);
    entries[i].filename[63] = '\0';
    entries[i].type = (uint8_t)rf.type;
    entries[i].isFavorite = rf.isFavorite ? 1 : 0;
    entries[i].hasBoxArt = rf.hasBoxArt ? 1 : 0;
    entries[i].reserved = 0;
    entries[i].fileSize = rf.fileSize;
  }

  uint64_t totalFileBytes = 0;
  for (uint32_t i = 0; i < entryCount; ++i)
    totalFileBytes += entries[i].fileSize;
  const uint32_t rootStamp = rootDirectoryStamp();
  if (rootStamp == 0) {
    heap_caps_free(entries);
    LOG_WARN_STR("SDCard: root directory timestamp unavailable; ROM index cache not saved");
    return false;
  }

  BmoIndexHeader header;
  memset(&header, 0, sizeof(header));
  memcpy(header.magic, BMO_INDEX_MAGIC, 8);
  header.version = BMO_INDEX_VERSION;
  header.entryCount = entryCount;
  header.entrySize = sizeof(BmoIndexEntry);
  header.crc32 = AtomicFs::crc32Update((const uint8_t*)entries, entriesBytes, 0xFFFFFFFFU);
  header.timestamp = 0;
  header.reserved[0] = (uint32_t)totalFileBytes;
  header.reserved[1] = BmoIndex::catalogFingerprint(entryCount, totalFileBytes, rootStamp, header.crc32);

  if (SD.exists(BMO_INDEX_TEMP_PATH)) SD.remove(BMO_INDEX_TEMP_PATH);
  File f = SD.open(BMO_INDEX_TEMP_PATH, FILE_WRITE);
  if (!f) {
    heap_caps_free(entries);
    return false;
  }

  bool ok = true;
  if (f.write((const uint8_t*)&header, sizeof(header)) != sizeof(header)) ok = false;
  if (ok && f.write((const uint8_t*)entries, entriesBytes) != entriesBytes) ok = false;
  f.flush();
  f.close();

  const size_t expectedFileSize = sizeof(BmoIndexHeader) + entriesBytes;
  ok = ok && verifyIndexFile(BMO_INDEX_TEMP_PATH, expectedFileSize, header.crc32);

  bool hadBackup = false;
  if (ok) {
    if (SD.exists(BMO_INDEX_BACKUP_PATH)) SD.remove(BMO_INDEX_BACKUP_PATH);
    if (SD.exists(BMO_INDEX_PATH)) {
      hadBackup = SD.rename(BMO_INDEX_PATH, BMO_INDEX_BACKUP_PATH);
      ok = hadBackup;
    }
  }
  if (ok) {
    ok = SD.rename(BMO_INDEX_TEMP_PATH, BMO_INDEX_PATH);
    if (!ok && hadBackup) SD.rename(BMO_INDEX_BACKUP_PATH, BMO_INDEX_PATH);
  }

  auto syncFinalTimestamp = [&]() -> bool {
    header.timestamp = rootDirectoryStamp();
    header.reserved[1] = BmoIndex::catalogFingerprint(entryCount, totalFileBytes, header.timestamp, header.crc32);
    bool synced = false;
    File finalFile = SD.open(BMO_INDEX_PATH, "r+");
    if (finalFile && header.timestamp != 0 && finalFile.seek(0) &&
        finalFile.write((const uint8_t*)&header, sizeof(header)) == sizeof(header)) {
      synced = true;
    }
    if (finalFile) {
      finalFile.flush();
      finalFile.close();
    }
    return synced && verifyIndexFile(BMO_INDEX_PATH, expectedFileSize, header.crc32);
  };

  if (ok) {
    // Renaming the index entry and removing the backup both mutate the root
    // directory mtime; sync before backup removal and again after cleanup.
    ok = syncFinalTimestamp();
  }
  if (ok) {
    if (hadBackup) SD.remove(BMO_INDEX_BACKUP_PATH);
    ok = syncFinalTimestamp();
  } else {
    if (SD.exists(BMO_INDEX_PATH)) SD.remove(BMO_INDEX_PATH);
    if (hadBackup && SD.exists(BMO_INDEX_BACKUP_PATH)) {
      SD.rename(BMO_INDEX_BACKUP_PATH, BMO_INDEX_PATH);
    }
    if (SD.exists(BMO_INDEX_TEMP_PATH)) SD.remove(BMO_INDEX_TEMP_PATH);
  }

  heap_caps_free(entries);
  return ok;
#else
  return false;
#endif
}

void SDCard::rebuildIndex() {
#if FEATURE_SD_CARD
  using namespace BmoSdInternal;
  {
    SpiArbiter::ScopedLock spiLock;
    skippedRomCount = 0;
    poolGrowthFailed = false;
    if (SD.exists(BMO_INDEX_PATH)) SD.remove(BMO_INDEX_PATH);
    if (SD.exists(BMO_INDEX_TEMP_PATH)) SD.remove(BMO_INDEX_TEMP_PATH);
    if (SD.exists(BMO_INDEX_BACKUP_PATH)) SD.remove(BMO_INDEX_BACKUP_PATH);

    const int bakedCount = bakedRomCount();
    numRoms = bakedCount;
    favoritesCount = 0;
    memset(romCountsByType, 0, sizeof(romCountsByType));
    for (int i = 0; i < bakedCount; ++i) {
      if (romList[i].type <= ROM_COLEM) {
        romCountsByType[romList[i].type]++;
      }
      if (romList[i].isFavorite) {
        favoritesCount++;
      }
    }
  }
  scanRoms();
  loadFavorites();
#endif
}

void SDCard::scanDirectory(const char* dirPath, int depth, bool hasArtDirs) {
#if FEATURE_SD_CARD
  using namespace BmoSdInternal;
  if (!dirPath || depth > 3) return;

  File dir = SD.open(dirPath);
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    return;
  }

  const char* relPrefix = (dirPath[0] == '/') ? (dirPath + 1) : dirPath;

  while (true) {
    File entry = dir.openNextFile();
    if (!entry) break;

    const char* rawName = entry.name();
    const char* slash = strrchr(rawName, '/');
    const char* baseName = slash ? (slash + 1) : rawName;
    const bool isDir = entry.isDirectory();
    const uint32_t fileSize = isDir ? 0 : (uint32_t)entry.size();

    if (RomFileFilter::isIgnoredName(baseName)) {
      entry.close();
      continue;
    }

    if (isDir) {
      if (depth < 3 && !isIgnoredDirName(baseName)) {
        char childPath[128];
        if (relPrefix[0] == '\0') {
          snprintf(childPath, sizeof(childPath), "/%s", baseName);
        } else {
          snprintf(childPath, sizeof(childPath), "/%s/%s", relPrefix, baseName);
        }
        entry.close();
        scanDirectory(childPath, depth + 1, hasArtDirs);
      } else {
        entry.close();
      }
      continue;
    }

    const RomType type = ConsoleCatalog::determineTypeFromFilename(baseName);
    if (type != ROM_UNKNOWN) {
      uint8_t prefix[32];
      size_t prefixSize = 0;
      uint8_t genesisHeader[4];
      size_t genesisHeaderSize = 0;
      if (RomFileFilter::needsSample(baseName)) {
        if (entry.seek(0)) prefixSize = entry.read(prefix, sizeof(prefix));
        const char* dot = strrchr(baseName, '.');
        if (dot && strcasecmp(dot, ".md") == 0 && fileSize >= 0x104 && entry.seek(0x100)) {
          genesisHeaderSize = entry.read(genesisHeader, sizeof(genesisHeader));
        }
      }
      if (!RomFileFilter::isPlausibleFile(baseName, fileSize, prefix, prefixSize, genesisHeader, genesisHeaderSize)) {
        entry.close();
        skippedRomCount++;
        continue;
      }
      entry.close();

      if (numRoms >= MAX_ROM_POOL ||
          (numRoms >= maxCapacity && (poolGrowthFailed || !ensurePoolCapacity(computePoolCapacity(numRoms + 1))))) {
        poolGrowthFailed = true;
        skippedRomCount++;
        continue;
      }

      char relPath[64];
      int written = 0;
      if (relPrefix[0] == '\0') {
        written = snprintf(relPath, sizeof(relPath), "%s", baseName);
      } else {
        written = snprintf(relPath, sizeof(relPath), "%s/%s", relPrefix, baseName);
      }
      if (written > 0 && written < (int)sizeof(relPath)) {
        strncpy(romList[numRoms].filename, relPath, 63);
        romList[numRoms].filename[63] = '\0';
        romList[numRoms].type = type;
        romList[numRoms].isFavorite = false;
        romList[numRoms].hasBoxArt = false;
        romList[numRoms].isPopular = SDCard::isPopularGame(baseName, type);
        romList[numRoms].fileSize = fileSize;
        if (type <= ROM_COLEM) {
          romCountsByType[type]++;
        }
        numRoms++;
      } else {
        skippedRomCount++;
      }
    } else {
      entry.close();
    }
  }
  dir.close();
  (void)hasArtDirs;
#else
  (void)dirPath;
  (void)depth;
  (void)hasArtDirs;
#endif
}

void SDCard::scanRoms() {
#if FEATURE_SD_CARD
  using namespace BmoSdInternal;
  skippedRomCount = 0;
  poolGrowthFailed = false;
  bool loadedFromCache = false;
  bool hasArtDirs = false;
  {
    SpiArbiter::ScopedLock spiLock;

    recoverIndexTransaction();
    if (loadIndex()) {
      loadedFromCache = true;
      hasArtDirs = SD.exists("/boxart") || SD.exists("/covers");
    } else {
      scanDirectory("/", 0, false);
    }
  }
  if (loadedFromCache) {
    const int bakedCount = bakedRomCount();
    for (int i = 0; i < bakedCount && i < numRoms; ++i) {
      romList[i].hasBoxArt = hasArtDirs ? BoxArt::existsForRom(romList[i].filename) : false;
    }
    return;
  }

  sortRoms();
  BoxArt::populateCatalogAvailability(romList, numRoms);

  if (skippedRomCount == 0) {
    SpiArbiter::ScopedLock spiLock;
    saveIndex();
  }
  if (skippedRomCount != 0) {
    LOG_WARN("SDCard: skipped %d ROM paths; library index not saved", skippedRomCount);
  }

  ensurePoolCapacity(computePoolCapacity(numRoms));
#endif
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

bool SDCard::isPopularGame(const char* filename, RomType type) {
  if (!filename || !filename[0]) return false;
  const char* base = strrchr(filename, '/');
  if (!base) base = strrchr(filename, '\\');
  base = base ? (base + 1) : filename;

  char lower[64];
  size_t i = 0;
  for (; base[i] != '\0' && i + 1 < sizeof(lower); ++i) {
    char c = base[i];
    if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    lower[i] = c;
  }
  lower[i] = '\0';

  auto matchAny = [&](const char* const* keywords, int n) -> bool {
    for (int k = 0; k < n; ++k) {
      if (strstr(lower, keywords[k]) != nullptr) return true;
    }
    return false;
  };

  static const char* const kGbTop[] = {"virtual bmo", "tetris",        "super mario", "zelda",       "link's awakening",
                                       "pokemon",     "kirby",         "metroid",     "donkey kong", "gargoyle",
                                       "castlevania", "final fantasy", "wario land",  "mega man",    "contra",
                                       "ducktales",   "dr. mario",     "spider-man"};
  static const char* const kGbcTop[] = {"mario",      "zelda",         "oracle",        "pokemon",
                                        "metal gear", "shantae",       "wario land",    "dragon warrior",
                                        "aladdin",    "lego racers",   "kirby",         "grand theft auto",
                                        "tetris",     "resident evil", "survival kids", "lufia"};
  static const char* const kNesTop[] = {"super mario", "zelda",         "metroid",   "mega man",      "castlevania",
                                        "contra",      "kirby",         "punch-out", "final fantasy", "dragon warrior",
                                        "tetris",      "ninja gaiden",  "ducktales", "river city",    "double dragon",
                                        "excitebike",  "blaster master"};
  static const char* const kSnesTop[] = {
      "chrono trigger", "super mario", "zelda",          "link to the past", "super metroid", "donkey kong",
      "final fantasy",  "earthbound",  "secret of mana", "mega man x",       "contra",        "castlevania",
      "f-zero",         "star fox",    "street fighter", "actraiser",        "terranigma"};
  static const char* const kGenesisTop[] = {
      "sonic",         "streets of rage", "gunstar",       "shinobi",       "phantasy star", "shining force",
      "castlevania",   "contra",          "golden axe",    "altered beast", "comix zone",    "earthworm jim",
      "mortal kombat", "aladdin",         "thunder force", "vectorman",     "ristar"};
  static const char* const kSmsGgTop[] = {
      "alex kidd",          "phantasy star", "wonder boy", "sonic",  "shinobi", "castle of illusion",
      "master of darkness", "power strike",  "golden axe", "r-type", "outrun",  "columns",
      "defender of oasis",  "tails",         "gunstar",    "ristar"};
  static const char* const kPceTop[] = {"bonk",          "blazing lazers", "rondo",        "castlevania",
                                        "soldier blade", "star soldier",   "bomberman",    "splatterhouse",
                                        "ys ",           "air zonk",       "neutopia",     "military madness",
                                        "parasol stars", "r-type",         "devil's crush"};
  static const char* const kAtariTop[] = {"pitfall",        "river raid",      "adventure", "yar",     "kaboom",
                                          "space invaders", "missile command", "frogger",   "pac-man", "pacman",
                                          "asteroids",      "centipede",       "berzerk",   "h.e.r.o"};
  static const char* const kColemTop[] = {"donkey kong", "zaxxon",    "venture",      "lady bug", "mouse trap",
                                          "smurf",       "pepper",    "frogger",      "galaxian", "gorf",
                                          "antarctic",   "montezuma", "boulder dash", "q*bert"};
  static const char* const kNgpTop[] = {"snk vs",     "card fighters",    "millennium",      "metal slug",
                                        "sonic",      "king of fighters", "samurai shodown", "fatal fury",
                                        "last blade", "dark arms",        "faselei"};
  static const char* const kLynxTop[] = {
      "california games", "blue lightning", "chip's challenge", "chips challenge", "klax", "rygar",   "stun runner",
      "s.t.u.n",          "xenophobe",      "rampart",          "gauntlet",        "toki", "lemmings"};
  static const char* const kWSwanTop[] = {"final fantasy", "klonoa",      "gunpey",    "silversword",
                                          "dicing knight", "guilty gear", "rockman",   "mega man",
                                          "digimon",       "chocobo",     "makaimura", "golden axe"};
  static const char* const kPicoTop[] = {"celeste", "high stakes", "slipways", "picohot",  "jelpi",        "combo pool",
                                         "poom",    "dusk child",  "bmo",      "cab ride", "just one boss"};
  static const char* const kDoomTop[] = {"doom", "freedoom", "plutonia", "tnt", "heretic", "chex"};

  switch (type) {
  case ROM_GB:
    return matchAny(kGbTop, sizeof(kGbTop) / sizeof(kGbTop[0]));
  case ROM_GBC:
    return matchAny(kGbcTop, sizeof(kGbcTop) / sizeof(kGbcTop[0]));
  case ROM_NES:
    return matchAny(kNesTop, sizeof(kNesTop) / sizeof(kNesTop[0]));
  case ROM_SNES:
    return matchAny(kSnesTop, sizeof(kSnesTop) / sizeof(kSnesTop[0]));
  case ROM_GENESIS:
    return matchAny(kGenesisTop, sizeof(kGenesisTop) / sizeof(kGenesisTop[0]));
  case ROM_SMS:
  case ROM_GG:
    return matchAny(kSmsGgTop, sizeof(kSmsGgTop) / sizeof(kSmsGgTop[0]));
  case ROM_PCE:
    return matchAny(kPceTop, sizeof(kPceTop) / sizeof(kPceTop[0]));
  case ROM_ATARI:
    return matchAny(kAtariTop, sizeof(kAtariTop) / sizeof(kAtariTop[0]));
  case ROM_COLEM:
    return matchAny(kColemTop, sizeof(kColemTop) / sizeof(kColemTop[0]));
  case ROM_NGP:
    return matchAny(kNgpTop, sizeof(kNgpTop) / sizeof(kNgpTop[0]));
  case ROM_LYNX:
    return matchAny(kLynxTop, sizeof(kLynxTop) / sizeof(kLynxTop[0]));
  case ROM_WSWAN:
    return matchAny(kWSwanTop, sizeof(kWSwanTop) / sizeof(kWSwanTop[0]));
  case ROM_PICO8:
    return matchAny(kPicoTop, sizeof(kPicoTop) / sizeof(kPicoTop[0]));
  case ROM_WAD:
    return matchAny(kDoomTop, sizeof(kDoomTop) / sizeof(kDoomTop[0]));
  default:
    return false;
  }
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
