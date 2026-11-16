// SPDX-License-Identifier: MIT OR GPL-2.0-or-later
#include "gb_common.h"

#include <atomic>

#include "../hal/feature_flags.h"
#include "../storage/battery_save.h"
#include "../ui/display_emu.h"

#if defined(__GNUC__) || defined(__clang__)
#define BMO_WEAK_ATTR __attribute__((weak))
#else
#define BMO_WEAK_ATTR
#endif

extern "C" BMO_WEAK_ATTR uint8_t audio_read(const uint16_t addr) {
  (void)addr;
  return 0;
}
extern "C" BMO_WEAK_ATTR void audio_write(const uint16_t addr, const uint8_t val) {
  (void)addr;
  (void)val;
}

namespace GbCommon {

namespace {
std::atomic<bool> s_errorPending{false};
std::atomic<int> s_errorCode{0};
std::atomic<uint16_t> s_errorAddr{0};
} // namespace

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

void clearError() {
  s_errorPending.store(false, std::memory_order_release);
  s_errorCode.store(0, std::memory_order_relaxed);
  s_errorAddr.store(0, std::memory_order_relaxed);
}

void reportError(const char* coreName, int errCode, uint16_t addr) {
  LOG_ERROR("%s Error: %d at PC 0x%04X", coreName ? coreName : "GB", errCode, static_cast<unsigned int>(addr));
  s_errorCode.store(errCode, std::memory_order_relaxed);
  s_errorAddr.store(addr, std::memory_order_relaxed);
  s_errorPending.store(true, std::memory_order_release);
}

bool consumeError(int* outErrCode, uint16_t* outAddr) {
  if (!s_errorPending.exchange(false, std::memory_order_acq_rel)) {
    if (outErrCode) *outErrCode = 0;
    if (outAddr) *outAddr = 0U;
    return false;
  }
  if (outErrCode) *outErrCode = s_errorCode.load(std::memory_order_relaxed);
  if (outAddr) *outAddr = s_errorAddr.load(std::memory_order_relaxed);
  return true;
}

} // namespace GbCommon
