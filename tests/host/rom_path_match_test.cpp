#include "rom_catalog_lookup.h"
#include "rom_path_match.h"
#include "../ui/text_fit.h"
#include "test_check.h"

#include <string.h>
#include <vector>

namespace {

const char* g_uniqueCatalog[] = {
    "nes/mario.nes",
    "gb/zelda.gb",
    "gba/metroid.gba",
};

const char* uniqueCatalogAt(int index) {
  if (index < 0 || index >= 3) return nullptr;
  return g_uniqueCatalog[index];
}

const char* g_ambiguousCatalog[] = {
    "gb/zelda.gb",
    "gbc/zelda.gb",
    "nes/mario.nes",
};

const char* ambiguousCatalogAt(int index) {
  if (index < 0 || index >= 3) return nullptr;
  return g_ambiguousCatalog[index];
}

} // namespace

int testPathNormalization() {
  BMO_CHECK(strcmp(RomPathMatch::normalizeRelative("/gb/zelda.gb"), "gb/zelda.gb") == 0);
  BMO_CHECK(strcmp(RomPathMatch::normalizeRelative("///gb/zelda.gb"), "gb/zelda.gb") == 0);
  BMO_CHECK(strcmp(RomPathMatch::normalizeRelative("zelda.gb"), "zelda.gb") == 0);
  BMO_CHECK(strcmp(RomPathMatch::normalizeRelative(nullptr), "") == 0);

  BMO_CHECK(RomPathMatch::hasPathSeparator("gb/zelda.gb"));
  BMO_CHECK(!RomPathMatch::hasPathSeparator("zelda.gb"));
  BMO_CHECK(strcmp(RomPathMatch::baseNameOf("gb/sub/zelda.gb"), "zelda.gb") == 0);
  BMO_CHECK(strcmp(RomPathMatch::baseNameOf("zelda.gb"), "zelda.gb") == 0);

  BMO_CHECK(RomPathMatch::equalsNormalized("/gb/zelda.gb", "gb/zelda.gb"));
  BMO_CHECK(!RomPathMatch::equalsNormalized("gb/a/zelda.gb", "gb/b/zelda.gb"));
  BMO_CHECK(RomPathMatch::equalsIgnoreCaseAscii("GB/Zelda.GB", "gb/zelda.gb"));
  BMO_CHECK(!RomPathMatch::equalsIgnoreCaseAscii("gb/zelda.gb", "gb/mario.gb"));

  bool migrated = true;
  BMO_CHECK(RomPathMatch::resolveCatalogIndex("gb/zelda.gb", 3, uniqueCatalogAt, &migrated) == 1);
  BMO_CHECK(!migrated);

  migrated = true;
  BMO_CHECK(RomPathMatch::resolveCatalogIndex("/nes/mario.nes", 3, uniqueCatalogAt, &migrated) == 0);
  BMO_CHECK(!migrated);

  migrated = false;
  BMO_CHECK(RomPathMatch::resolveCatalogIndex("zelda.gb", 3, uniqueCatalogAt, &migrated) == 1);
  BMO_CHECK(migrated);

  migrated = false;
  BMO_CHECK(RomPathMatch::resolveCatalogIndex("zelda.gb", 3, ambiguousCatalogAt, &migrated) == -1);
  BMO_CHECK(!migrated);

  migrated = false;
  BMO_CHECK(RomPathMatch::resolveCatalogIndex("snes/zelda.gb", 3, uniqueCatalogAt, &migrated) == -1);
  BMO_CHECK(!migrated);
  return 0;
}

int testIndexedLookupParity() {
  RomPathMatch::CatalogHashEntry uNorm[3] = {};
  RomPathMatch::CatalogHashEntry uBase[3] = {};
  RomPathMatch::CatalogPathLookup uLookup;
  BMO_CHECK(uLookup.init(3, uniqueCatalogAt, uNorm, uBase));
  BMO_CHECK(uLookup.findExact("/nes/mario.nes", uniqueCatalogAt) == 0);
  BMO_CHECK(uLookup.findExact("nes\\mario.nes", uniqueCatalogAt) == 0);
  BMO_CHECK(uLookup.findExact("/NES/MARIO.NES", uniqueCatalogAt) == -1);
  BMO_CHECK(uLookup.findExact("zelda.gb", uniqueCatalogAt) == -1);
  const char* queries[] = {"gb/zelda.gb", "/nes/mario.nes", "\\nes\\mario.nes", "/zelda.gb",
                           "zelda.gb",    "ZELDA.GB",       "snes/zelda.gb",    "missing.gb"};
  for (const char* q : queries) {
    bool expMigrated = false;
    bool gotMigrated = false;
    const int expIdx = RomPathMatch::resolveCatalogIndex(q, 3, uniqueCatalogAt, &expMigrated);
    const int gotIdx = uLookup.resolve(q, uniqueCatalogAt, &gotMigrated);
    BMO_CHECK(gotIdx == expIdx);
    BMO_CHECK(gotMigrated == expMigrated);
  }

  RomPathMatch::CatalogHashEntry aNorm[3] = {};
  RomPathMatch::CatalogHashEntry aBase[3] = {};
  RomPathMatch::CatalogPathLookup aLookup;
  BMO_CHECK(aLookup.init(3, ambiguousCatalogAt, aNorm, aBase));
  for (const char* q : queries) {
    bool expMigrated = false;
    bool gotMigrated = false;
    const int expIdx = RomPathMatch::resolveCatalogIndex(q, 3, ambiguousCatalogAt, &expMigrated);
    const int gotIdx = aLookup.resolve(q, ambiguousCatalogAt, &gotMigrated);
    BMO_CHECK(gotIdx == expIdx);
    BMO_CHECK(gotMigrated == expMigrated);
  }

  static const char* kManyRoms[12] = {
      "snes/Zelda.sfc", "gb/Alpha.gb",   "gbc/Beta.gbc",     "nes/Gamma.nes", "gb/Shared.gb", "nes/Shared.gb",
      "pce/Delta.pce",  "md/Epsilon.md", "lynx/Foxtrot.lnx", "ngp/Golf.ngp",  "ws/Hotel.ws",  "a26/India.a26",
  };
  auto manyAt = [](int idx) -> const char* { return (idx >= 0 && idx < 12) ? kManyRoms[idx] : nullptr; };
  std::vector<RomPathMatch::CatalogHashEntry> mNorm(12);
  std::vector<RomPathMatch::CatalogHashEntry> mBase(12);
  RomPathMatch::CatalogPathLookup mLookup;
  BMO_CHECK(mLookup.init(12, manyAt, mNorm.data(), mBase.data()));
  const char* manyQueries[] = {"shared.gb", "GB/SHARED.GB", "/nes/shared.gb",
                               "delta.pce", "hotel.ws",     "snes/zelda.sfc"};
  for (const char* q : manyQueries) {
    bool em = false;
    bool gm = false;
    BMO_CHECK(mLookup.resolve(q, manyAt, &gm) == RomPathMatch::resolveCatalogIndex(q, 12, manyAt, &em));
    BMO_CHECK(gm == em);
  }
  return 0;
}

int testTextFitEllipsis() {
  char fitOut[64] = "";
  int calls = 0;
  BmoTextFit::fitCenteredWithEllipsis("Short", 100, fitOut, sizeof(fitOut), [&](const char* s) -> int {
    calls++;
    return static_cast<int>(strlen(s)) * 6;
  });
  BMO_CHECK(strcmp(fitOut, "Short") == 0);
  BMO_CHECK(calls == 1);

  calls = 0;
  BmoTextFit::fitCenteredWithEllipsis("Super Mario World - Super Mario Advance 2 (USA)", 120, fitOut, sizeof(fitOut),
                                      [&](const char* s) -> int {
                                        calls++;
                                        return static_cast<int>(strlen(s)) * 6;
                                      });
  BMO_CHECK(strlen(fitOut) == 20U);
  BMO_CHECK(strcmp(fitOut, "Super Mario World...") == 0);
  BMO_CHECK(calls <= 7);
  return 0;
}

int main() {
  if (testPathNormalization() != 0) return 1;
  if (testIndexedLookupParity() != 0) return 1;
  if (testTextFitEllipsis() != 0) return 1;
  return 0;
}
