#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "box_art_lookup.h"
#include "test_check.h"

int main() {
  char baseBuf[32] = "";
  BoxArtLookup::getBaseName("snes/Chrono Trigger (USA).sfc", baseBuf, sizeof(baseBuf));
  BMO_CHECK(strcmp(baseBuf, "Chrono Trigger (USA)") == 0);

  RomFile roms[5] = {};
  roms[0].filename = "gb/Mario.gb";
  roms[1].filename = "nes/Mario.nes";
  roms[2].filename = "gbc/Zelda.gbc";
  roms[3].filename = "snes/Chrono Trigger (USA).sfc";
  roms[4].filename = "Virtual BMO (Official Game).gb";

  BoxArtLookup::KeyEntry keys[5] = {};
  int32_t baseOrder[5] = {};
  BoxArtLookup::CatalogMatcher matcher;
  assert(matcher.init(roms, 5, keys, baseOrder));

  // Collision case: legacy /boxart/mario.raw matches BOTH gb/Mario.gb and nes/Mario.nes,
  // so neither should get hasBoxArt=1 regardless of discovery order.
  matcher.markArtFile("mario.raw");
  assert(roms[0].hasBoxArt == 0);
  assert(roms[1].hasBoxArt == 0);

  // Keyed art for gb/Mario.gb only enables gb/Mario.gb.
  char gbMarioKey[33];
  assert(RomKey::make("gb/Mario.gb", gbMarioKey, sizeof(gbMarioKey)));
  char keyedFilename[64];
  snprintf(keyedFilename, sizeof(keyedFilename), "%s.RAW", gbMarioKey);
  matcher.markArtFile(keyedFilename);
  assert(roms[0].hasBoxArt == 1);
  assert(roms[1].hasBoxArt == 0);

  // Unique legacy art (case-insensitive BMP) resolves cleanly.
  matcher.markArtFile("/covers/zelda.BMP");
  assert(roms[2].hasBoxArt == 1);

  // Directory entries or non-art extensions must be ignored.
  matcher.markArtFile("Chrono Trigger (USA).png");
  assert(roms[3].hasBoxArt == 0);

  // Keyed baked ROM art resolves cleanly.
  char bakedKey[33];
  assert(RomKey::make("Virtual BMO (Official Game).gb", bakedKey, sizeof(bakedKey)));
  snprintf(keyedFilename, sizeof(keyedFilename), "/boxart/%s.bmp", bakedKey);
  matcher.markArtFile(keyedFilename);
  assert(roms[4].hasBoxArt == 1);

  printf("BOX_ART_LOOKUP_OK\n");
  return 0;
}
