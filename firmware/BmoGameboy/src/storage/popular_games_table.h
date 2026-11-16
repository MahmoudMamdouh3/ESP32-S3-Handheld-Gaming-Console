// SPDX-License-Identifier: MIT OR GPL-2.0-or-later
#pragma once

#include "sd_card.h"
#include <stddef.h>
#include <string.h>

namespace PopularGamesTable {

struct KeywordTableEntry {
  RomType type;
  const char* const* words;
  size_t count;
};

inline void toLowerBasename(const char* filename, char* outLower, size_t cap) {
  const char* slash = strrchr(filename, '/');
  const char* backslash = strrchr(filename, '\\');
  const char* base = (backslash && (!slash || backslash > slash)) ? (backslash + 1) : (slash ? (slash + 1) : filename);
  size_t i = 0;
  for (; base[i] != '\0' && i + 1 < cap; ++i) {
    char c = base[i];
    outLower[i] = (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
  }
  outLower[i] = '\0';
}

inline bool matchesPopularKeyword(const char* filename, RomType type) {
  if (!filename || !filename[0]) return false;
  char lower[64];
  toLowerBasename(filename, lower, sizeof(lower));

  static constexpr const char* kGbTop[] = {
      "virtual bmo", "tetris",   "super mario", "zelda",     "link's awakening", "pokemon",
      "kirby",       "metroid",  "donkey kong", "gargoyle",  "castlevania",      "final fantasy",
      "wario land",  "mega man", "contra",      "ducktales", "dr. mario",        "spider-man"};
  static constexpr const char* kGbcTop[] = {"mario",      "zelda",         "oracle",        "pokemon",
                                            "metal gear", "shantae",       "wario land",    "dragon warrior",
                                            "aladdin",    "lego racers",   "kirby",         "grand theft auto",
                                            "tetris",     "resident evil", "survival kids", "lufia"};
  static constexpr const char* kNesTop[] = {
      "super mario", "zelda",      "metroid",       "mega man",       "castlevania",   "contra",
      "kirby",       "punch-out",  "final fantasy", "dragon warrior", "tetris",        "ninja gaiden",
      "ducktales",   "river city", "double dragon", "excitebike",     "blaster master"};
  static constexpr const char* kSnesTop[] = {
      "chrono trigger", "super mario", "zelda",          "link to the past", "super metroid", "donkey kong",
      "final fantasy",  "earthbound",  "secret of mana", "mega man x",       "contra",        "castlevania",
      "f-zero",         "star fox",    "street fighter", "actraiser",        "terranigma"};
  static constexpr const char* kGenesisTop[] = {
      "sonic",         "streets of rage", "gunstar",       "shinobi",       "phantasy star", "shining force",
      "castlevania",   "contra",          "golden axe",    "altered beast", "comix zone",    "earthworm jim",
      "mortal kombat", "aladdin",         "thunder force", "vectorman",     "ristar"};
  static constexpr const char* kSmsGgTop[] = {
      "alex kidd",          "phantasy star", "wonder boy", "sonic",  "shinobi", "castle of illusion",
      "master of darkness", "power strike",  "golden axe", "r-type", "outrun",  "columns",
      "defender of oasis",  "tails",         "gunstar",    "ristar"};
  static constexpr const char* kPceTop[] = {"bonk",          "blazing lazers", "rondo",        "castlevania",
                                            "soldier blade", "star soldier",   "bomberman",    "splatterhouse",
                                            "ys ",           "air zonk",       "neutopia",     "military madness",
                                            "parasol stars", "r-type",         "devil's crush"};
  static constexpr const char* kAtariTop[] = {"pitfall",        "river raid",      "adventure", "yar",     "kaboom",
                                              "space invaders", "missile command", "frogger",   "pac-man", "pacman",
                                              "asteroids",      "centipede",       "berzerk",   "h.e.r.o"};
  static constexpr const char* kColemTop[] = {"donkey kong", "zaxxon",    "venture",      "lady bug", "mouse trap",
                                              "smurf",       "pepper",    "frogger",      "galaxian", "gorf",
                                              "antarctic",   "montezuma", "boulder dash", "q*bert"};
  static constexpr const char* kNgpTop[] = {"snk vs",     "card fighters",    "millennium",      "metal slug",
                                            "sonic",      "king of fighters", "samurai shodown", "fatal fury",
                                            "last blade", "dark arms",        "faselei"};
  static constexpr const char* kLynxTop[] = {
      "california games", "blue lightning", "chip's challenge", "chips challenge", "klax", "rygar",   "stun runner",
      "s.t.u.n",          "xenophobe",      "rampart",          "gauntlet",        "toki", "lemmings"};
  static constexpr const char* kWSwanTop[] = {"final fantasy", "klonoa",      "gunpey",    "silversword",
                                              "dicing knight", "guilty gear", "rockman",   "mega man",
                                              "digimon",       "chocobo",     "makaimura", "golden axe"};
  static constexpr const char* kPicoTop[] = {"celeste", "high stakes", "slipways",     "picohot",
                                             "jelpi",   "combo pool",  "poom",         "dusk child",
                                             "bmo",     "cab ride",    "just one boss"};
  static constexpr const char* kDoomTop[] = {"doom", "freedoom", "plutonia", "tnt", "heretic", "chex"};

  static constexpr KeywordTableEntry kTable[] = {
      {ROM_GB, kGbTop, sizeof(kGbTop) / sizeof(kGbTop[0])},
      {ROM_GBC, kGbcTop, sizeof(kGbcTop) / sizeof(kGbcTop[0])},
      {ROM_NES, kNesTop, sizeof(kNesTop) / sizeof(kNesTop[0])},
      {ROM_SNES, kSnesTop, sizeof(kSnesTop) / sizeof(kSnesTop[0])},
      {ROM_GENESIS, kGenesisTop, sizeof(kGenesisTop) / sizeof(kGenesisTop[0])},
      {ROM_SMS, kSmsGgTop, sizeof(kSmsGgTop) / sizeof(kSmsGgTop[0])},
      {ROM_GG, kSmsGgTop, sizeof(kSmsGgTop) / sizeof(kSmsGgTop[0])},
      {ROM_PCE, kPceTop, sizeof(kPceTop) / sizeof(kPceTop[0])},
      {ROM_ATARI, kAtariTop, sizeof(kAtariTop) / sizeof(kAtariTop[0])},
      {ROM_COLEM, kColemTop, sizeof(kColemTop) / sizeof(kColemTop[0])},
      {ROM_NGP, kNgpTop, sizeof(kNgpTop) / sizeof(kNgpTop[0])},
      {ROM_LYNX, kLynxTop, sizeof(kLynxTop) / sizeof(kLynxTop[0])},
      {ROM_WSWAN, kWSwanTop, sizeof(kWSwanTop) / sizeof(kWSwanTop[0])},
      {ROM_PICO8, kPicoTop, sizeof(kPicoTop) / sizeof(kPicoTop[0])},
      {ROM_WAD, kDoomTop, sizeof(kDoomTop) / sizeof(kDoomTop[0])},
  };

  for (const KeywordTableEntry& entry : kTable) {
    if (entry.type != type) continue;
    for (size_t k = 0; k < entry.count; ++k) {
      if (strstr(lower, entry.words[k]) != nullptr) return true;
    }
    return false;
  }
  return false;
}

} // namespace PopularGamesTable
