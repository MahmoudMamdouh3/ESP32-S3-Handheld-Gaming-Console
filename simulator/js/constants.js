/**
 * simulator/js/constants.js
 * Shared simulator state machine enums, 16-console metadata list, and title helpers.
 */
(function (root) {
  "use strict";

  // State Machine Constants (Synchronized with firmware ConsoleCatalog / BmoApp)
  root.STATE = {
    BOOT_SPLASH: 0,
    CONSOLE_SELECT: 1,
    GAME_SELECT: 2,
    CONSOLE_MUSEUM: 3,
    EMULATION_VIEW: 4,
    IDLE_MASCOT: 5
  };

  root.MAX_BROWSER_ROM_SIZE = 8 * 1024 * 1024;

  // Console Metadata (synchronized with firmware/BmoGameboy/src/ui/console_catalog.h)
  root.CONSOLES_LIST = [
    { id: "ROM_FAVORITES", name: "★ Favorites", year: "Starred", ext: "★", badge: "FAV", color: "#FFE033", count: 0 },
    { id: "ROM_GB", name: "Game Boy", year: "1989", ext: ".gb", badge: "DMG", color: "#5FB49C", count: 0, browserSupported: true },
    { id: "ROM_GBC", name: "Game Boy Color", year: "1998", ext: ".gbc", badge: "GBC", color: "#2ED573", count: 0 },
    { id: "ROM_NES", name: "Nintendo NES", year: "1983", ext: ".nes", badge: "NES", color: "#FF5E57", count: 0, browserSupported: true },
    { id: "ROM_SNES", name: "Super Nintendo", year: "1990", ext: ".sfc", badge: "SNES", color: "#9C88FF", count: 0 },
    { id: "ROM_GENESIS", name: "Sega Genesis", year: "1988", ext: ".gen", badge: "MD", color: "#2E86DE", count: 0 },
    { id: "ROM_SMS", name: "Master System", year: "1985", ext: ".sms", badge: "SMS", color: "#3B82F6", count: 0 },
    { id: "ROM_GG", name: "Game Gear", year: "1990", ext: ".gg", badge: "GG", color: "#F368E0", count: 0 },
    { id: "ROM_PCE", name: "PC Engine / TG16", year: "1987", ext: ".pce", badge: "PCE", color: "#FF6B6B", count: 0 },
    { id: "ROM_ATARI", name: "Atari 2600", year: "1977", ext: ".a26", badge: "A26", color: "#E056FD", count: 0, browserSupported: true },
    { id: "ROM_COLEM", name: "ColecoVision", year: "1982", ext: ".col", badge: "COL", color: "#00D2D3", count: 0, browserSupported: true },
    { id: "ROM_NGP", name: "Neo Geo Pocket", year: "1998", ext: ".ngc", badge: "NGP", color: "#FFA502", count: 0 },
    { id: "ROM_LYNX", name: "Atari Lynx", year: "1989", ext: ".lnx", badge: "LNX", color: "#10AC84", count: 0 },
    { id: "ROM_WSWAN", name: "WonderSwan Color", year: "1999", ext: ".wsc", badge: "WS", color: "#ECCC68", count: 0 },
    { id: "ROM_PICO8", name: "PICO-8", year: "2015", ext: ".p8", badge: "P8", color: "#FF78C4", count: 0 },
    { id: "ROM_WAD", name: "DOOM", year: "1993", ext: ".wad", badge: "WAD", color: "#FF4757", count: 0 }
  ];

  root.BmoSimModules = root.BmoSimModules || {};

  // Same slug rule as scripts/fetch_covers.py slugify_title().
  root.BmoSimModules.slugifyTitle = function slugifyTitle(title) {
    let s = String(title || "").replace(/\.[^/.]+$/, "").toLowerCase();
    s = s.replace(/\([^)]*\)|\[[^\]]*\]/g, "");
    s = s.replace(/[^a-z0-9]+/g, "_").replace(/^_+|_+$/g, "");
    return s || "cover";
  };
}(typeof globalThis !== "undefined" ? globalThis : window));
