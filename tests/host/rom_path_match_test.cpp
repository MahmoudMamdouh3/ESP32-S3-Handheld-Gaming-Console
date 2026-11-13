#include "rom_path_match.h"
#include "test_check.h"

#include <string.h>

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

int main() {
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

  // Exact relative match works without migration flag
  bool migrated = true;
  BMO_CHECK(RomPathMatch::resolveCatalogIndex("gb/zelda.gb", 3, uniqueCatalogAt, &migrated) == 1);
  BMO_CHECK(!migrated);

  // Leading slash normalized exact match
  migrated = true;
  BMO_CHECK(RomPathMatch::resolveCatalogIndex("/nes/mario.nes", 3, uniqueCatalogAt, &migrated) == 0);
  BMO_CHECK(!migrated);

  // Unique legacy basename entry migrates cleanly
  migrated = false;
  BMO_CHECK(RomPathMatch::resolveCatalogIndex("zelda.gb", 3, uniqueCatalogAt, &migrated) == 1);
  BMO_CHECK(migrated);

  // Ambiguous legacy basename across multiple consoles/folders must NOT guess
  migrated = false;
  BMO_CHECK(RomPathMatch::resolveCatalogIndex("zelda.gb", 3, ambiguousCatalogAt, &migrated) == -1);
  BMO_CHECK(!migrated);

  // Explicit subfolder path missing from catalog must NOT migrate onto another folder's basename
  migrated = false;
  BMO_CHECK(RomPathMatch::resolveCatalogIndex("snes/zelda.gb", 3, uniqueCatalogAt, &migrated) == -1);
  BMO_CHECK(!migrated);

  return 0;
}
