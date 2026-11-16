#include <SD.h>
#include <stdint.h>
#include <string.h>
#include <vector>

#include "cores/gb_common.h"
#include "hal/spi_arbiter.h"
#include "storage/atomic_fs.h"
#include "storage/bmp_header.h"
#include "storage/path_key.h"
#include "storage/save_manager.h"
#include "storage/sd_card.h"
#include "ui/display_emu.h"
#include "test_check.h"

namespace DisplayEmu {
void waitForDisplay() {}
} // namespace DisplayEmu

namespace {
bool g_sdMounted = true;

int testStateRoundTripAndCrc() {
  uint8_t stateOut[64];
  uint8_t ramOut[32];
  for (size_t i = 0; i < sizeof(stateOut); ++i)
    stateOut[i] = static_cast<uint8_t>(i * 3U + 1U);
  for (size_t i = 0; i < sizeof(ramOut); ++i)
    ramOut[i] = static_cast<uint8_t>(0xA0U + i);

  BMO_CHECK(SaveManager::saveState("gb/tetris.gb", 1, SaveManager::CORE_GB_DMG, stateOut, sizeof(stateOut), ramOut,
                                   sizeof(ramOut), 32768U));
  BMO_CHECK(SaveManager::hasSaveState("gb/tetris.gb", 1));

  char savePath[80];
  SaveManager::getSaveStatePath("gb/tetris.gb", 1, savePath, sizeof(savePath));
  const std::vector<uint8_t>* savedBytes = SD.rawBytes(savePath);
  BMO_CHECK(savedBytes != nullptr);
  BMO_CHECK(savedBytes->size() == sizeof(SaveManager::SaveStateHeader) + sizeof(stateOut) + sizeof(ramOut));

  uint8_t stateIn[64] = {};
  uint8_t ramIn[32] = {};
  size_t loadedStateSz = 0;
  size_t loadedRamSz = 0;
  BMO_CHECK(SaveManager::loadState("gb/tetris.gb", 1, SaveManager::CORE_GB_DMG, stateIn, sizeof(stateIn),
                                   &loadedStateSz, ramIn, sizeof(ramIn), &loadedRamSz, 32768U));
  BMO_CHECK(loadedStateSz == sizeof(stateOut));
  BMO_CHECK(loadedRamSz == sizeof(ramOut));
  BMO_CHECK(memcmp(stateIn, stateOut, sizeof(stateOut)) == 0);
  BMO_CHECK(memcmp(ramIn, ramOut, sizeof(ramOut)) == 0);

  std::vector<uint8_t> tampered = *savedBytes;
  tampered.back() ^= 0x55U;
  File wf = SD.open(savePath, FILE_WRITE);
  BMO_CHECK(static_cast<bool>(wf));
  BMO_CHECK(wf.write(tampered.data(), tampered.size()) == tampered.size());
  wf.close();

  uint8_t untouchedState[64];
  uint8_t untouchedRam[32];
  memset(untouchedState, 0x5CU, sizeof(untouchedState));
  memset(untouchedRam, 0x7DU, sizeof(untouchedRam));
  BMO_CHECK(!SaveManager::loadState("gb/tetris.gb", 1, SaveManager::CORE_GB_DMG, untouchedState, sizeof(untouchedState),
                                    &loadedStateSz, untouchedRam, sizeof(untouchedRam), &loadedRamSz, 32768U));
  for (uint8_t b : untouchedState)
    BMO_CHECK(b == 0x5CU);
  for (uint8_t b : untouchedRam)
    BMO_CHECK(b == 0x7DU);
  return 0;
}

int testLegacyReadOnlyV1() {
  SD.reset();
  SaveStateFormat::HeaderV1 v1{};
  v1.magic = SaveStateFormat::MAGIC_V1;
  v1.version = SaveStateFormat::VERSION_V1;
  v1.coreId = SaveManager::CORE_GB_DMG;
  v1.stateSize = 16U;
  v1.ramSize = 0U;
  memcpy(v1.romTitle, "LEGACY", 7);
  const uint8_t legacyPayload[16] = {9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 11, 12, 13, 14, 15, 16};
  v1.crc32 = AtomicFs::crc32Update(legacyPayload, sizeof(legacyPayload), 0xFFFFFFFFU) ^ 0xFFFFFFFFU;

  const char* legacyPath = "/saves/legacy_game.s02";
  File lf = SD.open(legacyPath, FILE_WRITE);
  BMO_CHECK(static_cast<bool>(lf));
  lf.write(reinterpret_cast<const uint8_t*>(&v1), sizeof(v1));
  lf.write(legacyPayload, sizeof(legacyPayload));
  lf.close();

  uint8_t restored[16] = {};
  size_t loadedStateSz = 0;
  size_t loadedRamSz = 0;
  BMO_CHECK(SaveManager::loadState("gb/legacy_game.gb", 2, SaveManager::CORE_GB_DMG, restored, sizeof(restored),
                                   &loadedStateSz, nullptr, 0U, &loadedRamSz, 0U));
  BMO_CHECK(loadedStateSz == 16U);
  BMO_CHECK(memcmp(restored, legacyPayload, 16U) == 0);

  char keyedV2Path[80];
  SaveManager::getSaveStatePath("gb/legacy_game.gb", 2, keyedV2Path, sizeof(keyedV2Path));
  BMO_CHECK(!SD.exists(keyedV2Path));
  const std::vector<uint8_t>* afterLegacy = SD.rawBytes(legacyPath);
  BMO_CHECK(afterLegacy != nullptr && afterLegacy->size() == sizeof(v1) + 16U);
  BMO_CHECK(memcmp(afterLegacy->data(), "BSS1", 4) == 0);
  return 0;
}

int testScreenshotBmpExport() {
  SD.reset();
  uint16_t pixels[4] = {0xF800U, 0x07E0U, 0x001FU, 0xFFFFU};
  HostRuntimeFake::g_micros = 42000ULL;
  BMO_CHECK(SaveManager::saveScreenshotBmp("gb/shot.gb", pixels, 2, 2));

  char key[33];
  BMO_CHECK(RomKey::make("gb/shot.gb", key, sizeof(key)));
  char bmpPath[96];
  snprintf(bmpPath, sizeof(bmpPath), "/screenshots/%s_42.bmp", key);
  const std::vector<uint8_t>* shotBytes = SD.rawBytes(bmpPath);
  BMO_CHECK(shotBytes != nullptr);
  BMO_CHECK(shotBytes->size() == 54U + 8U * 2U);
  BMO_CHECK((*shotBytes)[0] == 'B' && (*shotBytes)[1] == 'M');
  BMO_CHECK(static_cast<int32_t>(BmpHeader::readLe32(shotBytes->data() + 18U)) == 2);
  BMO_CHECK(static_cast<int32_t>(BmpHeader::readLe32(shotBytes->data() + 22U)) == 2);
  BMO_CHECK(BmpHeader::readLe16(shotBytes->data() + 28U) == 24U);
  return 0;
}
} // namespace

bool SDCard::isMounted() {
  return g_sdMounted;
}
uint32_t SDCard::findRomFileSize(const char*) {
  return 32768U;
}

int main() {
  SpiArbiter::init();
  SD.reset();
  BMO_CHECK(SaveManager::begin());
  if (testStateRoundTripAndCrc() != 0) return 1;
  if (testLegacyReadOnlyV1() != 0) return 1;
  if (testScreenshotBmpExport() != 0) return 1;
  return 0;
}
