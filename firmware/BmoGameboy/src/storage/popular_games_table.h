// SPDX-License-Identifier: MIT OR GPL-2.0-or-later
#pragma once

#include "sd_card.h"
#include <stddef.h>
#include <string.h>

namespace PopularGamesTable {

inline bool matchesPopularKeyword(const char* filename, SDCard::RomType type) {
  if (!filename || !filename[0]) return false;
  const char* base = strrchr(filename, '/');
  if (!base) base = strrchr(filename, '\\');
  base = base ? (base + 1) : filename;

  char lower[64];
  size_t i = 0;
  for (; base[i] != '\0' && i + 1 < sizeof(lower); ++i) {
    char c = base[i];
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
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
  case SDCard::ROM_GB:
    return matchAny(kGbTop, sizeof(kGbTop) / sizeof(kGbTop[0]));
  case SDCard::ROM_GBC:
    return matchAny(kGbcTop, sizeof(kGbcTop) / sizeof(kGbcTop[0]));
  case SDCard::ROM_NES:
    return matchAny(kNesTop, sizeof(kNesTop) / sizeof(kNesTop[0]));
  case SDCard::ROM_SNES:
    return matchAny(kSnesTop, sizeof(kSnesTop) / sizeof(kSnesTop[0]));
  case SDCard::ROM_GENESIS:
    return matchAny(kGenesisTop, sizeof(kGenesisTop) / sizeof(kGenesisTop[0]));
  case SDCard::ROM_SMS:
  case SDCard::ROM_GG:
    return matchAny(kSmsGgTop, sizeof(kSmsGgTop) / sizeof(kSmsGgTop[0]));
  case SDCard::ROM_PCE:
    return matchAny(kPceTop, sizeof(kPceTop) / sizeof(kPceTop[0]));
  case SDCard::ROM_ATARI:
    return matchAny(kAtariTop, sizeof(kAtariTop) / sizeof(kAtariTop[0]));
  case SDCard::ROM_COLEM:
    return matchAny(kColemTop, sizeof(kColemTop) / sizeof(kColemTop[0]));
  case SDCard::ROM_NGP:
    return matchAny(kNgpTop, sizeof(kNgpTop) / sizeof(kNgpTop[0]));
  case SDCard::ROM_LYNX:
    return matchAny(kLynxTop, sizeof(kLynxTop) / sizeof(kLynxTop[0]));
  case SDCard::ROM_WSWAN:
    return matchAny(kWSwanTop, sizeof(kWSwanTop) / sizeof(kWSwanTop[0]));
  case SDCard::ROM_PICO8:
    return matchAny(kPicoTop, sizeof(kPicoTop) / sizeof(kPicoTop[0]));
  case SDCard::ROM_WAD:
    return matchAny(kDoomTop, sizeof(kDoomTop) / sizeof(kDoomTop[0]));
  default:
    return false;
  }
}

} // namespace PopularGamesTable
