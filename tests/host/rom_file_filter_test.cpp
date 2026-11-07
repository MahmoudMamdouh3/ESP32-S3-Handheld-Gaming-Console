#include <stdint.h>

#include "rom_file_filter.h"

int main() {
  if (!RomFileFilter::isIgnoredName("._README.md")) return 1;
  if (!RomFileFilter::isIgnoredName("$RECYCLE.BIN")) return 2;
  if (!RomFileFilter::isIgnoredName("System Volume Information")) return 3;
  if (RomFileFilter::isIgnoredName("Mario.md")) return 4;

  const uint8_t text[] = "This is a README file, not a cartridge image.";
  if (RomFileFilter::isPlausibleFile("README.md", sizeof(text), text, sizeof(text), nullptr, 0)) return 5;
  uint8_t segaHeader[] = {'S', 'E', 'G', 'A'};
  if (!RomFileFilter::isPlausibleFile("SONIC.MD", 0x200, text, sizeof(text), segaHeader, sizeof(segaHeader))) return 6;
  if (RomFileFilter::isPlausibleFile("SONIC.md", 0x200, text, sizeof(text), nullptr, 0)) return 7;

  const uint8_t sgText[] = "notes about a game with readable text only";
  if (RomFileFilter::isPlausibleFile("notes.sg", 4096, sgText, sizeof(sgText), nullptr, 0)) return 8;
  uint8_t rawRom[32] = {0xC3, 0x00, 0x80};
  if (!RomFileFilter::isPlausibleFile("GAME.SG", 8192, rawRom, sizeof(rawRom), nullptr, 0)) return 9;
  uint8_t colecoHeader[32] = {0xAA, 0x55};
  if (!RomFileFilter::isPlausibleFile("GAME.COL", 8192, colecoHeader, sizeof(colecoHeader), nullptr, 0)) return 10;

  if (RomFileFilter::isPlausibleFile("empty.nes", 0, nullptr, 0, nullptr, 0)) return 11;
  if (RomFileFilter::isPlausibleFile("tiny.gb", 32, nullptr, 0, nullptr, 0)) return 12;
  return 0;
}
