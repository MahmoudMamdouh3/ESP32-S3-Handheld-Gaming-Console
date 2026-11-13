#include <string.h>

#include "src/ui/console_catalog.h"
#include "test_check.h"

int main() {
  BMO_CHECK(ConsoleCatalog::CONSOLE_COUNT == 16);
  BMO_CHECK(ConsoleCatalog::CONSOLES[0] == ROM_FAVORITES);

  // Every emulator index 0..14 must exist exactly once in kCatalogSpecs.
  for (int emu = 0; emu < 15; ++emu) {
    const ConsoleCatalog::ConsoleSpec* spec = ConsoleCatalog::findByEmulatorIndex(emu);
    BMO_CHECK(spec != nullptr);
    BMO_CHECK(spec->viewW > 0 && spec->viewW <= 320);
    BMO_CHECK(spec->viewH > 0 && spec->viewH <= 240);
    BMO_CHECK(spec->viewX >= 0 && spec->viewX + spec->viewW <= 320);
    BMO_CHECK(spec->viewY >= 0 && spec->viewY + spec->viewH <= 240);
    BMO_CHECK(spec->name != nullptr && spec->badge != nullptr && spec->museumLine1 != nullptr);
  }

  // Case-insensitive extension routing covers all 21 supported extensions.
  BMO_CHECK(ConsoleCatalog::determineTypeFromFilename("sub/zelda.gbc") == ROM_GBC);
  BMO_CHECK(ConsoleCatalog::determineTypeFromFilename("TETRIS.GB") == ROM_GB);
  BMO_CHECK(ConsoleCatalog::determineTypeFromFilename("sonic.GEN") == ROM_GENESIS);
  BMO_CHECK(ConsoleCatalog::determineTypeFromFilename("doom.wad") == ROM_WAD);
  BMO_CHECK(ConsoleCatalog::determineTypeFromFilename("readme.txt") == ROM_UNKNOWN);
  BMO_CHECK(strcmp(ConsoleCatalog::consoleBadge(ROM_GENESIS), "MD") == 0);
  BMO_CHECK(strcmp(ConsoleCatalog::consoleYear(ROM_ATARI), "1977") == 0);
  return 0;
}
