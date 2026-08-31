#pragma once
#include <stdint.h>

// ---------------------------------------------------------------------------
// theme.h — Central UI & Visual Design System (Rule 08)
// ---------------------------------------------------------------------------
// All UI colors, layout dimensions, fonts, and palettes for BMO Gameboy.
// Authentic BMO OnionUI Palette & Adventure Time Design Tokens.
// Note on Byte-Swapping: Display is in ST7789 BGR565 mode. UI pixels drawn
// to little-endian GFXcanvases need both RGB->BGR and byte swap for writeBytes().
// ---------------------------------------------------------------------------

namespace Theme {

  // Primary Spacing Unit (8px Base Grid per Rule 08)
  constexpr int SPACING_GRID     = 8;
  constexpr int PADDING_SMALL    = 4;
  constexpr int PADDING_NORMAL   = 8;
  constexpr int PADDING_LARGE    = 16;

  // Helper constexpr functions for compile-time color conversions
  constexpr uint16_t swapBytes(uint16_t value) {
    return (uint16_t)((value << 8) | (value >> 8));
  }

  constexpr uint16_t makeUiColor(uint8_t r, uint8_t g, uint8_t b) {
    const uint16_t bgr565 = ((uint16_t)(b & 0xF8) << 8)
                          | ((uint16_t)(g & 0xFC) << 3)
                          | ((uint16_t)r >> 3);
    return swapBytes(bgr565);
  }

  // -------------------------------------------------------------------------
  // Authentic OnionUI BMO Theme Palette
  // -------------------------------------------------------------------------
  constexpr uint16_t BMO_SCREEN_MINT   = makeUiColor(206, 245, 228); // #CEF5E4 - Light Mint Screen
  constexpr uint16_t BMO_BODY_TEAL     = makeUiColor(95, 180, 156);  // #5FB49C - Body Shell Teal
  constexpr uint16_t BMO_DEEP_TEAL     = makeUiColor(26, 75, 66);    // #1A4B42 - Deep Forest Teal
  constexpr uint16_t BMO_DARK_FOREST   = makeUiColor(15, 38, 32);    // #0F2620 - High-Contrast Dark / Eyes
  constexpr uint16_t BMO_DPAD_YELLOW   = makeUiColor(255, 224, 51);  // #FFE033 - D-Pad Yellow / Star Badge
  constexpr uint16_t BMO_CORAL_RED     = makeUiColor(232, 23, 93);   // #E8175D - BMO Red Button
  constexpr uint16_t BMO_BLUE_BUTTON   = makeUiColor(82, 166, 186);  // #52A6BA - BMO Blue Button
  constexpr uint16_t BMO_WHITE         = makeUiColor(248, 253, 250); // Crisp Pure White
  constexpr uint16_t BMO_MOUTH_GREEN   = makeUiColor(76, 175, 80);   // #4CAF50 - Open Mouth Green
  constexpr uint16_t BMO_TONGUE_PINK   = makeUiColor(244, 143, 177); // #F48FB1 - Cute Tongue Pink

  // Semantic UI Aliases
  constexpr uint16_t COLOR_BG_DARK       = BMO_DARK_FOREST;
  constexpr uint16_t COLOR_PANEL_BG      = BMO_DEEP_TEAL;
  constexpr uint16_t COLOR_ACCENT_TEAL   = BMO_BODY_TEAL;
  constexpr uint16_t COLOR_ACCENT_GOLD   = BMO_DPAD_YELLOW;
  constexpr uint16_t COLOR_ACCENT_CORAL  = BMO_CORAL_RED;
  constexpr uint16_t COLOR_ACCENT_BLUE   = BMO_BLUE_BUTTON;
  constexpr uint16_t COLOR_TEXT_BRIGHT   = BMO_WHITE;
  constexpr uint16_t COLOR_TEXT_MUTED    = makeUiColor(140, 195, 180);
  constexpr uint16_t COLOR_SUCCESS       = makeUiColor(76, 217, 100);
  constexpr uint16_t COLOR_ERROR         = BMO_CORAL_RED;

  // -------------------------------------------------------------------------
  // Game Boy DMG 4-Color Runtime Palettes (in BGR565 byte-swapped for SPI)
  // -------------------------------------------------------------------------
  enum DmgPaletteType {
    PALETTE_CLASSIC_GREEN = 0,  // Original 1989 Pea-Soup Green
    PALETTE_BMO_TEAL      = 1,  // Modern BMO Mint/Teal
    PALETTE_POCKET_GRAY   = 2,  // Game Boy Pocket High-Contrast Silver
    PALETTE_LIGHT_CYAN    = 3,  // Game Boy Light Backlit Indigo/Cyan
    PALETTE_AMBER_CRT     = 4,  // Warm Amber Phosphor
    PALETTE_COUNT         = 5
  };

  // Palette 0: Classic 1989 Pea-Soup Green
  constexpr uint16_t PALETTE_CLASSIC[4] = {
    0xD7EE, // Lightest Green  (RGB 155, 188, 15)
    0x2EAE, // Light Green     (RGB 139, 172, 15)
    0x444E, // Dark Green      (RGB  48,  98, 48)
    0x0806  // Darkest Green   (RGB  15,  56, 15)
  };

  // Palette 1: BMO Mint / Teal
  constexpr uint16_t PALETTE_BMO[4] = {
    makeUiColor(206, 245, 228), // BMO Mint Screen
    makeUiColor(95, 180, 156),  // BMO Body Teal
    makeUiColor(26, 75, 66),    // Deep Forest Teal
    makeUiColor(15, 38, 32)     // Obsidian Green
  };

  // Palette 2: Game Boy Pocket Grayscale
  constexpr uint16_t PALETTE_POCKET[4] = {
    makeUiColor(245, 245, 245), // White
    makeUiColor(170, 170, 170), // Light Gray
    makeUiColor(85, 85, 85),    // Dark Gray
    makeUiColor(10, 10, 10)     // Black
  };

  // Palette 3: Game Boy Light Cyan
  constexpr uint16_t PALETTE_LIGHT[4] = {
    makeUiColor(180, 255, 240), // Bright Aqua
    makeUiColor(80, 200, 210),  // Vivid Cyan
    makeUiColor(20, 100, 120),  // Deep Cyan
    makeUiColor(2, 30, 40)      // Abyss
  };

  // Palette 4: Amber CRT Phosphor
  constexpr uint16_t PALETTE_AMBER[4] = {
    makeUiColor(255, 210, 80),  // Bright Amber
    makeUiColor(220, 150, 20),  // Golden Amber
    makeUiColor(120, 70, 0),    // Dark Amber
    makeUiColor(20, 10, 0)      // Black
  };

} // namespace Theme
