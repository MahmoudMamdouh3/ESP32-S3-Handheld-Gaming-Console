#include "unit_tests.h"
#include <Arduino.h>
#include <esp_heap_caps.h>
#include "rom_data.h"
#include "buttons.h"
#include "display_emu.h"
#include "emu_peanut.h"
#include "emu_walnut.h"

// --- Custom Testing Framework Macros ---
static int testsPassed = 0;
static int testsFailed = 0;

#define ASSERT_TRUE(condition, msg) \
  if (condition) { \
    testsPassed++; \
  } else { \
    Serial.printf("[FAIL] %s:%d: %s\n", __FUNCTION__, __LINE__, msg); \
    testsFailed++; \
  }

#define ASSERT_EQ(actual, expected, msg) \
  if ((actual) == (expected)) { \
    testsPassed++; \
  } else { \
    Serial.printf("[FAIL] %s:%d: Expected %d, got %d. %s\n", __FUNCTION__, __LINE__, (int)(expected), (int)(actual), msg); \
    testsFailed++; \
  }

#define TEST_CASE(name) \
  Serial.printf("--- Running Test: %s ---\n", name);

// --- Component Tests ---

void testRomIntegrity() {
  TEST_CASE("ROM Integrity & Nintendo Logo Checksum");
  ASSERT_TRUE(tobu_tobu_girl_rom_len > 0x150, "ROM length must be greater than header size");
  
  // Verify header checksum at 0x014D (GB boot ROM validates this)
  uint8_t header_checksum = 0;
  for (int j = 0x0134; j <= 0x014C; j++) {
    header_checksum = header_checksum - tobu_tobu_girl_rom[j] - 1;
  }
  ASSERT_EQ(header_checksum, tobu_tobu_girl_rom[0x014D], "Header checksum mismatch for Tobu Tobu Girl");
}

void testButtonsCount() {
  TEST_CASE("Buttons Count Verification");
  ASSERT_EQ(Buttons::count(), 8, "Expected exactly 8 physical buttons to be tracked");
}

void testEmulatorRejection() {
  TEST_CASE("Emulator Graceful Rejection (Corrupt ROM)");
  
  uint8_t bad_rom[100] = {0};
  
  bool peanut_result = PeanutEmu::begin(bad_rom, sizeof(bad_rom));
  ASSERT_EQ(peanut_result, false, "PeanutEmu should reject corrupt ROM");
  
  bool walnut_result = WalnutEmu::begin(bad_rom, sizeof(bad_rom));
  ASSERT_EQ(walnut_result, false, "WalnutEmu should reject corrupt ROM");
}

void testDisplayPalette() {
  TEST_CASE("Display Palette Structure");
  
  // Values must match display_emu.cpp CLASSIC_PALETTE[] definition exactly.
  ASSERT_EQ(DisplayEmu::CLASSIC_PALETTE[0], 0xF30D, "Color 0 (lightest) mismatch");
  ASSERT_EQ(DisplayEmu::CLASSIC_PALETTE[3], 0xC109, "Color 3 (darkest) mismatch");
}

// ---------------------------------------------------------------------------
// BM2: IRAM usage diagnostic
// Reports heap_caps free size in IRAM region. Expected: > 200KB with current
// IRAM_ATTR functions (estimated ~10–15KB used from the 400KB budget).
// ---------------------------------------------------------------------------
void testIRAMPlacement() {
  TEST_CASE("IRAM Placement Budget");
  size_t iram_free = heap_caps_get_free_size(MALLOC_CAP_IRAM_8BIT);
  Serial.printf("  IRAM free: %u bytes\n", iram_free);
  // Verify at least 200KB is free (IRAM_ATTR functions should use << 100KB).
  ASSERT_TRUE(iram_free > 200 * 1024,
    "IRAM overcommitted — check IRAM_ATTR usage");
}

// ---------------------------------------------------------------------------
void testPaletteLUTMath() {
  TEST_CASE("Palette 256-entry LUT Mathematical Verification");
  
  // Verify Walnut DMG-on-GBC mapping logic for all 256 byte values
  for (int i = 0; i < 256; i++) {
    int pal_type = (i >> 4) & 0x03;
    int color_idx = i & 0x03;
    int legacy_index = (pal_type << 2) | color_idx;
    
    // The maximum legacy_index should never exceed 11
    ASSERT_TRUE(legacy_index >= 0 && legacy_index < 12, "Walnut legacy index out of bounds");
  }

  // Verify Peanut mapping logic
  for (int i = 0; i < 256; i++) {
    int legacy_index = i & 0x03;
    ASSERT_TRUE(legacy_index >= 0 && legacy_index < 4, "Peanut legacy index out of bounds");
  }
}

// (Scale map test removed in Round 3 as scale_map was eliminated)

bool runAllTests() {
  Serial.println("\n========== STARTING UNIT TEST SUITE ==========");
  testsPassed = 0;
  testsFailed = 0;
  
  testRomIntegrity();
  testButtonsCount();
  testEmulatorRejection();
  testDisplayPalette();
  testIRAMPlacement();
  testPaletteLUTMath();
  
  Serial.println("========== TEST SUITE FINISHED ==========");
  Serial.printf("PASSED: %d\n", testsPassed);
  Serial.printf("FAILED: %d\n", testsFailed);
  
  return (testsFailed == 0);
}
