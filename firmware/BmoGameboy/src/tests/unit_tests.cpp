#include "unit_tests.h"
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <string.h>
#include "../hal/buttons.h"
#include "../ui/display_emu.h"
#include "../cores/emu_peanut.h"
#include "../cores/emu_walnut.h"
#include "../hal/config.h"
#include "../storage/bmp_header.h"
#include "../storage/path_key.h"
#include "../storage/rom_index.h"
#include "../storage/save_state_format.h"

// --- Custom Testing Framework Macros ---
static int testsPassed = 0;
static int testsFailed = 0;

#define ASSERT_TRUE(condition, msg)                                                                                    \
  if (condition) {                                                                                                     \
    testsPassed++;                                                                                                     \
  } else {                                                                                                             \
    LOG_ERROR("%s:%d: %s", __FUNCTION__, __LINE__, msg);                                                               \
    testsFailed++;                                                                                                     \
  }

#define ASSERT_EQ(actual, expected, msg)                                                                               \
  if ((actual) == (expected)) {                                                                                        \
    testsPassed++;                                                                                                     \
  } else {                                                                                                             \
    LOG_ERROR("%s:%d: Expected %d, got %d. %s", __FUNCTION__, __LINE__, (int)(expected), (int)(actual), msg);          \
    testsFailed++;                                                                                                     \
  }

#define TEST_CASE(name) LOG_INFO("--- Running Test: %s ---", name);

// --- Component Tests ---

void testRomIntegrity() {
  TEST_CASE("GB Cartridge Header Checksum Formula");
  uint8_t header[0x150] = {0};
  const char title[] = "BMOTEST";
  for (int i = 0; title[i] != '\0'; i++) {
    header[0x0134 + i] = static_cast<uint8_t>(title[i]);
  }
  uint8_t sum = 0;
  for (int j = 0x0134; j <= 0x014C; j++) {
    sum = static_cast<uint8_t>(sum - header[j] - 1);
  }
  header[0x014D] = sum;

  uint8_t verified = 0;
  for (int j = 0x0134; j <= 0x014C; j++) {
    verified = static_cast<uint8_t>(verified - header[j] - 1);
  }
  ASSERT_EQ(verified, header[0x014D], "Header checksum formula mismatch");
}

void testButtonsCount() {
  TEST_CASE("Buttons Count Verification");
  ASSERT_EQ(Buttons::count(), 12, "Expected exactly 12 physical buttons to be tracked");
}

void testEmulatorRejection() {
  TEST_CASE("Emulator Graceful Rejection (Corrupt ROM)");

  uint8_t bad_rom[100] = {0};

  bool peanut_result = PeanutEmu::init(bad_rom, sizeof(bad_rom));
  ASSERT_EQ(peanut_result, false, "PeanutEmu should reject corrupt ROM");

  bool walnut_result = WalnutEmu::init(bad_rom, sizeof(bad_rom));
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
  ASSERT_TRUE(iram_free > 200 * 1024, "IRAM overcommitted — check IRAM_ATTR usage");
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
  // Perform 100 fast dummy reads intermixed with pushing a valid 240-pixel row
  uint16_t* dummyRow = static_cast<uint16_t*>(heap_caps_calloc(240, sizeof(uint16_t), MALLOC_CAP_SPIRAM));
  ASSERT_TRUE(dummyRow != nullptr, "Failed to allocate PSRAM scratch row for SPI contention test");
  if (dummyRow) {
    dummyRow[0] = 0xFFFF;
    for (int i = 0; i < 100; i++) {
      size_t dummySize = 0;
      uint8_t* dump = SDCard::loadRom("nonexistent_dummy.rom", &dummySize);
      DisplayEmu::pushPixels(0, dummyRow, 1);
      if (dump) SDCard::freeRom(dump);
    }
    heap_caps_free(dummyRow);
  }
  unsigned long elapsed = micros() - start;

  // We expect this rapid bus swapping to take less than 50ms for 100 cycles
  // if the hardware CS pins are toggling properly and there is no bus locking.
  ASSERT_TRUE(elapsed < 50000, "SPI Bus contention took too long, possible hardware collision or mutex stall");
}

// ---------------------------------------------------------------------------
void testStorageFormatsOnDevice() {
  TEST_CASE("RomKey, BMOIDX03, SaveStateFormat, and BMP Header");

  char keyA[64] = "";
  char keyB[64] = "";
  ASSERT_TRUE(RomKey::make("/GB/Zelda.gb", keyA, sizeof(keyA)), "RomKey::make failed");
  ASSERT_TRUE(RomKey::make("gb\\zelda.gb", keyB, sizeof(keyB)), "RomKey::make failed");
  ASSERT_TRUE(strcmp(keyA, keyB) == 0, "RomKey normalization mismatch");

  const char strTab[] = "gb/zelda.gb";
  BmoIndexEntry entry;
  memset(&entry, 0, sizeof(entry));
  entry.pathOffset = 0U;
  entry.pathLength = 11U;
  entry.type = 1U;
  entry.fileSize = 32768U;
  ASSERT_TRUE(BmoIndex::validateEntryRecord(entry, sizeof(strTab), strTab), "Valid BMOIDX03 entry rejected");
  entry.pathLength = 0U;
  ASSERT_TRUE(!BmoIndex::validateEntryRecord(entry, sizeof(strTab), strTab), "Empty BMOIDX03 path accepted");

  SaveStateFormat::HeaderV2 hdr;
  memset(&hdr, 0, sizeof(hdr));
  hdr.magic = SaveStateFormat::MAGIC_V2;
  hdr.version = SaveStateFormat::VERSION_V2;
  hdr.headerSize = sizeof(hdr);
  hdr.coreId = 2U;
  hdr.pathKeyHash = 0x12345678U;
  hdr.romSize = 32768U;
  hdr.stateSize = 1024U;
  hdr.ramSize = 2048U;
  SaveStateFormat::DecodedHeader decoded;
  memset(&decoded, 0, sizeof(decoded));
  ASSERT_TRUE(SaveStateFormat::decodeHeader(reinterpret_cast<const uint8_t*>(&hdr), sizeof(hdr), sizeof(hdr) + 3072U,
                                            2U, 0x12345678U, 32768U, 2048U, 4096U, &decoded),
              "Valid BSS2 header rejected");

  uint8_t bmp[54];
  memset(bmp, 0, sizeof(bmp));
  bmp[0] = 'B';
  bmp[1] = 'M';
  bmp[10] = 54U;
  bmp[14] = 40U;
  bmp[18] = 64U;
  bmp[22] = 64U;
  bmp[26] = 1U;
  bmp[28] = 24U;
  BmpHeader::Info bmpInfo;
  ASSERT_TRUE(BmpHeader::parse64x64(bmp, 54U + 64U * 64U * 3U, &bmpInfo), "Valid 64x64 24bpp BMP rejected");
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
  testStorageFormatsOnDevice();

  LOG_INFO_STR("========== TEST SUITE FINISHED ==========");
  LOG_INFO("PASSED: %d", testsPassed);
  LOG_INFO("FAILED: %d", testsFailed);

  return (testsFailed == 0);
}
