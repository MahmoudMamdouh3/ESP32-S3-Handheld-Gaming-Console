#pragma once

#include "../storage/sd_card.h"
#include "../storage/rom_file_filter.h"
#include <stddef.h>
#include <stdint.h>

namespace ConsoleCatalog {

struct ConsoleSpec {
  RomType type;
  int emulatorIndex; // -1 for ROM_FAVORITES, 0..14 for hardware cores
  const char* name;
  const char* year;
  const char* badge;
  const char* defaultExt;
  const char* bezelLabel;
  int viewX;
  int viewY;
  int viewW;
  int viewH;
  const char* museumLine1;
  const char* museumLine2;
  const char* museumLine3;
  const char* museumLine4;
  const char* museumLine5;
};

inline constexpr RomType CONSOLES[] = {ROM_FAVORITES, ROM_GB,  ROM_GBC,   ROM_NES,   ROM_WAD,     ROM_SMS,
                                       ROM_GG,        ROM_PCE, ROM_ATARI, ROM_PICO8, ROM_GENESIS, ROM_SNES,
                                       ROM_WSWAN,     ROM_NGP, ROM_LYNX,  ROM_COLEM};

inline constexpr int CONSOLE_COUNT = static_cast<int>(sizeof(CONSOLES) / sizeof(CONSOLES[0]));
static_assert(CONSOLE_COUNT == 16, "Expected Favorites + 15 console entries");

inline constexpr ConsoleSpec kCatalogSpecs[] = {
    {ROM_FAVORITES, -1, "FAVORITES", "STARRED", "FAV", "*", "BMO HANDHELD", 40, 12, 240, 216,
     "Category: Universal Starred Games Collection", "Consoles: Multi-System Auto-Dispatch Launcher",
     "BMO Tip: Press SELECT on ANY game in ANY console", "library to star it and launch directly from here!",
     "Auto-saves favorites list to SD: /favorites.txt"},
    {ROM_GBC, 0, "GAME BOY COLOR", "1998", "GBC", ".gbc", "GAME BOY COLOR", 40, 12, 240, 216,
     "CPU: Sharp LR35902 Dual @ 8.39MHz | 32KB RAM", "Display: 160x144, 32,768 Color TFT LCD",
     "Legacy: Full color portable revolution; double", "CPU speed, backwards compatible palette engine.",
     "Landmarks: Zelda Oracle Ages, Pokemon Crystal"},
    {ROM_GB, 1, "GAME BOY", "1989", "GB", ".gb", "GAME BOY DMG", 40, 12, 240, 216,
     "CPU: Sharp LR35902 @ 4.19MHz | 8KB RAM", "Display: 160x144, 4-Shade Olive Green STN LCD",
     "Legacy: Gunpei Yokoi's lateral thinking;", "30h battery life, Pokemon & Tetris phenomenon.",
     "Landmarks: Tetris, Pokemon R/B, Zelda Link's Awk"},
    {ROM_NES, 2, "NES", "1983", "NES", ".nes", "NINTENDO NES", 32, 0, 256, 240,
     "CPU: Ricoh 2A03 @ 1.79MHz | 2KB RAM + Mappers", "Display: 256x240, 25 Simultaneous Colors",
     "Legacy: Revived gaming after 1983 crash;", "Miyamoto created modern game design grammars.",
     "Landmarks: Super Mario, Zelda, Metroid, Mega Man"},
    {ROM_WAD, 3, "DOOM", "1993", "DOOM", ".wad", "CLASSIC DOOM", 0, 20, 320, 200,
     "Engine: id Tech 1 / DOOM Engine (BSP Raycasting)", "Display: 320x200 @ 35 FPS, 256 VGA Indexed Colors",
     "Legacy: Carmack & Romero transformed gaming;", "pioneered 3D FPS, multiplayer deathmatch, mods.",
     "Landmarks: DOOM, DOOM II, Freedoom, FreeDM"},
    {ROM_SMS, 4, "SEGA MASTER SYSTEM", "1986", "SMS", ".sms", "MASTER SYSTEM", 32, 24, 256, 192,
     "CPU: Zilog Z80 @ 3.58MHz | 8KB RAM + 16KB VRAM", "Display: 256x192, 32 Simultaneous Colors",
     "Legacy: Superior 8-bit color depth and memory;", "pioneered 3D dungeon rendering in Phantasy Star.",
     "Landmarks: Phantasy Star, Alex Kidd, Wonder Boy"},
    {ROM_GG, 5, "GAME GEAR", "1990", "GG", ".gg", "SEGA GAME GEAR", 80, 48, 160, 144,
     "CPU: Zilog Z80 @ 3.58MHz | 8KB RAM + 16KB VRAM", "Display: 160x144, 4096-Color Master Palette",
     "Legacy: Full-color landscape handheld with", "backlit screen and arcade porting excellence.",
     "Landmarks: Sonic Triple Trouble, Shinobi II"},
    {ROM_PCE, 6, "PC ENGINE", "1987", "PCE", ".pce", "PC ENGINE", 32, 0, 256, 240,
     "CPU: HuC6280 @ 7.16MHz | 16-bit HuC6270 GPU", "Display: 256x240, 482 Simultaneous Colors",
     "Legacy: Tiny credit-card HuCards with giant", "color palettes; the ultimate home arcade shmup.",
     "Landmarks: Castlevania Rondo, Soldier Blade"},
    {ROM_ATARI, 7, "ATARI 2600", "1977", "A26", ".a26", "ATARI 2600", 80, 24, 160, 192,
     "CPU: MOS 6507 @ 1.19MHz | 128 BYTES RAM", "Display: 160x192, Racing the beam with 0 VRAM",
     "Legacy: Invented home cartridge gaming; programmers", "calculated scanlines cycle-by-cycle in real time.",
     "Landmarks: Pitfall!, River Raid, Space Invaders"},
    {ROM_PICO8, 8, "PICO-8", "2015", "P8", ".p8", "PICO-8 FANTASY", 96, 56, 128, 128,
     "Engine: Lua Virtual Computer @ 8MHz virtual clock", "Display: 128x128, 16 Curated Colors, 32KB Carts",
     "Legacy: Ignited the modern fantasy console movement;", "deliberate constraints spark unbounded creativity.",
     "Landmarks: Celeste Classic, Slipways, High Stakes"},
    {ROM_GENESIS, 9, "SEGA GENESIS", "1988", "MD", ".md", "SEGA GENESIS", 0, 8, 320, 224,
     "CPU: Motorola 68000 @ 7.67MHz + Z80 Sound Co-CPU", "Display: 320x224, 64 Colors, YM2612 6-Ch FM Audio",
     "Legacy: Blast processing speed & gritty FM synth;", "Sonic momentum physics, electronic club tracks.",
     "Landmarks: Sonic 2, Streets of Rage 2, Gunstar"},
    {ROM_SNES, 10, "SUPER NINTENDO", "1990", "SNES", ".sfc", "SUPER NINTENDO", 32, 8, 256, 224,
     "CPU: 65C816 @ 3.58MHz | 128KB RAM + SPC700 DSP", "Display: 256x224, Mode 7 Scaling, 32,768 Colors",
     "Legacy: Golden era of 2D pixel art & sampled", "audio; multi-jointed sprites, 16-bit mastery.",
     "Landmarks: Chrono Trigger, Super Metroid, Mario"},
    {ROM_WSWAN, 11, "WONDERSWAN", "1999", "WS", ".ws", "WONDERSWAN", 48, 48, 224, 144,
     "CPU: 16-bit NEC V30 MZ @ 3.07MHz | 64KB RAM", "Display: 224x144 Widescreen, Dual D-Pad TATE Mode",
     "Legacy: Gunpei Yokoi's final design; 30+ hours on", "ONE AA battery, playable vertical or horizontal.",
     "Landmarks: Klonoa, Final Fantasy IV, Silversword"},
    {ROM_NGP, 12, "NEO GEO POCKET", "1998", "NGP", ".ngc", "NEO GEO POCKET", 80, 44, 160, 152,
     "CPU: Toshiba TLCS-900H 16/32-bit RISC @ 6.14MHz", "Display: 160x152, 4096 Colors, Microswitch Stick",
     "Legacy: Legendary arcade fighting precision and", "ultra-fluid chibi animations on a clicky stick.",
     "Landmarks: Card Fighters Clash, Match Millennium"},
    {ROM_LYNX, 13, "ATARI LYNX", "1989", "LNX", ".lnx", "ATARI LYNX", 80, 69, 160, 102,
     "CPU: MOS 65SC02 @ 4MHz + Suzy 16MHz Co-Processor", "Display: 160x102, 4096 Colors, Hardware 3D Scaler",
     "Legacy: World's first color handheld with real-time", "sprite zooming, scaling, and 3D pseudo-space.",
     "Landmarks: Blue Lightning, California Games"},
    {ROM_COLEM, 14, "COLECOVISION", "1982", "COL", ".col", "COLECOVISION", 32, 24, 256, 192,
     "CPU: Z80A @ 3.58MHz | 1KB RAM + 16KB TMS9918A VDP", "Display: 256x192, 16 Colors, 32 Hardware Sprites",
     "Legacy: First home console with true arcade ports", "and dedicated hardware video memory.",
     "Landmarks: Donkey Kong, Zaxxon, Venture"}};

static_assert(sizeof(kCatalogSpecs) / sizeof(kCatalogSpecs[0]) == CONSOLE_COUNT,
              "kCatalogSpecs and CONSOLES must have matching counts");

inline const ConsoleSpec* findByType(RomType type) {
  for (const auto& spec : kCatalogSpecs) {
    if (spec.type == type) return &spec;
  }
  return nullptr;
}

inline const ConsoleSpec* findByEmulatorIndex(int emulatorIndex) {
  for (const auto& spec : kCatalogSpecs) {
    if (spec.emulatorIndex == emulatorIndex) return &spec;
  }
  return nullptr;
}

inline const char* consoleName(RomType type) {
  const ConsoleSpec* spec = findByType(type);
  return spec ? spec->name : "GAMES";
}

inline const char* consoleYear(RomType type) {
  const ConsoleSpec* spec = findByType(type);
  return spec ? spec->year : "";
}

inline const char* consoleBadge(RomType type) {
  const ConsoleSpec* spec = findByType(type);
  return spec ? spec->badge : "GAME";
}

inline const char* consoleExt(RomType type) {
  const ConsoleSpec* spec = findByType(type);
  return spec ? spec->defaultExt : ".rom";
}

struct ExtensionRoute {
  const char* ext;
  RomType type;
};

inline constexpr ExtensionRoute kExtensionRoutes[] = {
    {".gb", ROM_GB},      {".gbc", ROM_GBC},     {".nes", ROM_NES},   {".wad", ROM_WAD},  {".sms", ROM_SMS},
    {".gg", ROM_GG},      {".pce", ROM_PCE},     {".a26", ROM_ATARI}, {".p8", ROM_PICO8}, {".gen", ROM_GENESIS},
    {".md", ROM_GENESIS}, {".smd", ROM_GENESIS}, {".sfc", ROM_SNES},  {".smc", ROM_SNES}, {".ws", ROM_WSWAN},
    {".wsc", ROM_WSWAN},  {".ngp", ROM_NGP},     {".ngc", ROM_NGP},   {".lnx", ROM_LYNX}, {".col", ROM_COLEM},
    {".sg", ROM_COLEM},
};

static_assert(sizeof(kExtensionRoutes) / sizeof(kExtensionRoutes[0]) == 21,
              "Expected exactly 21 ROM file extension routes");

inline RomType determineTypeFromFilename(const char* filename) {
  const char* ext = RomFileFilter::extensionOf(filename);
  if (!ext) return ROM_UNKNOWN;
  for (const auto& route : kExtensionRoutes) {
    if (RomFileFilter::equalIgnoreCase(ext, route.ext)) {
      return route.type;
    }
  }
  return ROM_UNKNOWN;
}

} // namespace ConsoleCatalog
