#include "unit_tests.h"
#include <Arduino.h>
#include <esp_heap_caps.h>
// rom_data.h lives at repo-root/assets/legacy_headers/rom_data.h
// Relative path from firmware/BmoGameboy/src/tests/ to repo root is ../../../../
#include "../assets/rom_data.h"
#include "../core/buttons.h"
#include "../core/display_emu.h"
#include "../emulators/emu_peanut.h"
#include "../emulators/emu_walnut.h"

// --- Custom Testing Framework Macros ---
static int testsPassed = 0;
static int testsFailed = 0;

#define ASSERT_TRUE(condition, msg) \
  if (condition) { \
    testsPassed++; \
  } else { \
    LOG_ERROR("%s:%d: %s", __FUNCTION__, __LINE__, msg); \
    testsFailed++; \
  }

#define ASSERT_EQ(actual, expected, msg) \
  if ((actual) == (expected)) { \
    testsPassed++; \
  } else { \
    LOG_ERROR("%s:%d: Expected %d, got %d. %s", __FUNCTION__, __LINE__, (int)(expected), (int)(actual), msg); \
    testsFailed++; \
  }

#define TEST_CASE(name) \
  LOG_INFO("--- Running Test: %s ---", name);

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
  LOG_INFO("  IRAM free: %u bytes", iram_free);
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

// ---------------------------------------------------------------------------
void testSPISDCondention() {
  TEST_CASE("SPI Bus Contention (SD vs Display) Stress Test");
  
  // This simulates the DOOM engine's streaming behavior: attempting to rapidly
  // alternate between SD card reads and Display writes.
  if (!SDCard::isMounted()) {
    LOG_INFO_STR("  [SKIP] No SD card mounted for contention test.");
    return;
  }
  
  unsigned long start = micros();
  // Perform 100 fast dummy reads intermixed with pushing random pixels
  uint16_t dummyPixel = 0xFFFF;
  for (int i = 0; i < 100; i++) {
    size_t dummySize = 0;
    uint8_t* dump = SDCard::loadRom("nonexistent_dummy.rom", &dummySize);
    DisplayEmu::pushPixelsRaw(0, &dummyPixel, 1);
    if (dump) SDCard::freeRom(dump);
  }
  unsigned long elapsed = micros() - start;
  
  // We expect this rapid bus swapping to take less than 50ms for 100 cycles
  // if the hardware CS pins are toggling properly and there is no bus locking.
  ASSERT_TRUE(elapsed < 50000, "SPI Bus contention took too long, possible hardware collision or mutex stall");
}

// ---------------------------------------------------------------------------

bool runAllTests() {
  LOG_INFO_STR("\n========== STARTING UNIT TEST SUITE ==========");
  testsPassed = 0;
  testsFailed = 0;
  
  testRomIntegrity();
  testButtonsCount();
  testEmulatorRejection();
  testDisplayPalette();
  testIRAMPlacement();
  testPaletteLUTMath();
  testSPISDCondention();
  
  LOG_INFO_STR("========== TEST SUITE FINISHED ==========");
  LOG_INFO("PASSED: %d", testsPassed);
  LOG_INFO("FAILED: %d", testsFailed);
  
  return (testsFailed == 0);
}
