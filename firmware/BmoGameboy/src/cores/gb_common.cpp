// SPDX-License-Identifier: MIT OR GPL-2.0-or-later
#include "gb_common.h"

#include "../hal/feature_flags.h"
#include "../storage/battery_save.h"
#include "../ui/display_emu.h"

#if FEATURE_AUDIO
extern "C" __attribute__((weak)) uint8_t audio_read(const uint16_t addr) {
  (void)addr;
  return 0; // Replace with actual APU read if minigb_apu is integrated
}
extern "C" __attribute__((weak)) void audio_write(const uint16_t addr, const uint8_t val) {
  (void)addr;
  (void)val;
  // Replace with actual APU write / I2S push
}
#else
extern "C" __attribute__((weak)) uint8_t audio_read(const uint16_t addr) {
  (void)addr;
  return 0;
}
extern "C" __attribute__((weak)) void audio_write(const uint16_t addr, const uint8_t val) {
  (void)addr;
  (void)val;
}
#endif

namespace GbCommon {

alignas(4) uint16_t g_gbRowBuffer[240] = {0};

size_t batteryRamSize(uint8_t romType, uint8_t romSizeCode) {
  const size_t size = BatterySave::gameBoyBatteryBytes(romType, romSizeCode);
  return (size <= kCartRamSize) ? size : 0U;
}

void* allocGbStateAligned(size_t stateSize, const char* tag) {
  void* ptr = heap_caps_aligned_alloc(32, stateSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!ptr) {
    ptr = heap_caps_aligned_alloc(32, stateSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  }
  if (!ptr) {
    LOG_ERROR("%s: unable to allocate emulator state.", tag);
    return nullptr;
  }
  memset(ptr, 0, stateSize);
  return ptr;
}

uint8_t* allocCartRam(const char* tag, uint32_t heapCaps) {
  uint8_t* ram = static_cast<uint8_t*>(heap_caps_malloc(kCartRamSize, heapCaps));
  if (!ram) {
    LOG_ERROR("%s: unable to allocate PSRAM cartridge RAM.", tag);
    return nullptr;
  }
  memset(ram, 0, kCartRamSize);
  return ram;
}

void freeGbRuntime(void** gbPtr, uint8_t** cartRamPtr, const uint8_t** romPtr, size_t* romLen) {
  DisplayEmu::waitForDisplay();
  if (cartRamPtr && *cartRamPtr) {
    heap_caps_free(*cartRamPtr);
    *cartRamPtr = nullptr;
  }
  if (gbPtr && *gbPtr) {
    heap_caps_free(*gbPtr);
    *gbPtr = nullptr;
  }
  if (romPtr) *romPtr = nullptr;
  if (romLen) *romLen = 0;
}

} // namespace GbCommon
