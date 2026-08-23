#include "unit_tests.h"
#include <Arduino.h>
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
  // Check bounds
  ASSERT_TRUE(tobu_tobu_girl_rom_len > 0x150, "ROM length must be greater than header size");
  
  // Check Nintendo Logo Checksum (0x0104 - 0x0133)
  // The Gameboy validates the boot logo by checking against a hardcoded array.
  // Instead of doing the full array match, we can check the header checksum at 0x014D
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
  
  // Create a dummy bad ROM array (smaller than header size)
  uint8_t bad_rom[100] = {0};
  
  // PeanutEmu and WalnutEmu should reject this gracefully without crashing
  bool peanut_result = PeanutEmu::begin(bad_rom, sizeof(bad_rom));
  ASSERT_EQ(peanut_result, false, "PeanutEmu should reject corrupt ROM");
  
  bool walnut_result = WalnutEmu::begin(bad_rom, sizeof(bad_rom));
  ASSERT_EQ(walnut_result, false, "WalnutEmu should reject corrupt ROM");
}

void testDisplayPalette() {
  TEST_CASE("Display Palette Structure");
  
  // Verify the classic palette has 4 elements and matches expectations
  ASSERT_EQ(DisplayEmu::CLASSIC_PALETTE[0], 0xE19D, "Color 0 mismatch");
  ASSERT_EQ(DisplayEmu::CLASSIC_PALETTE[3], 0xC109, "Color 3 mismatch");
}

bool runAllTests() {
  Serial.println("\n========== STARTING UNIT TEST SUITE ==========");
  testsPassed = 0;
  testsFailed = 0;
  
  testRomIntegrity();
  testButtonsCount();
  testEmulatorRejection();
  testDisplayPalette();
  
  Serial.println("========== TEST SUITE FINISHED ==========");
  Serial.printf("PASSED: %d\n", testsPassed);
  Serial.printf("FAILED: %d\n", testsFailed);
  
  return (testsFailed == 0);
}
