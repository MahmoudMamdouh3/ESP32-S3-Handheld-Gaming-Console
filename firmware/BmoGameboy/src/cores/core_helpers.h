#pragma once

#include "../hal/buttons.h"
#include "../hal/psram_alloc.h"
#include "../storage/save_manager.h"

#include <stddef.h>
#include <stdint.h>

namespace CoreHelpers {

struct ButtonBit {
  Buttons::Index button;
  uint32_t mask;
};

template <size_t N> inline uint32_t readButtonMask(const ButtonBit (&mapping)[N]) {
  uint32_t mask = 0;
  for (size_t i = 0; i < N; ++i) {
    if (Buttons::get(mapping[i].button).pressed) {
      mask |= mapping[i].mask;
    }
  }
  return mask;
}

template <typename SerializeFn>
inline bool savePsramState(const char* romFilename, int slot, uint32_t coreId, size_t capacity, SerializeFn&& serialize,
                           const uint8_t* batteryRam = nullptr, size_t batteryBytes = 0) {
  if (!romFilename || capacity == 0) return false;
  uint8_t* buf = static_cast<uint8_t*>(bmo_psram_malloc(capacity));
  if (!buf) return false;

  size_t stateBytes = 0;
  bool ok = serialize(buf, capacity, &stateBytes) && stateBytes > 0 && stateBytes <= capacity &&
            SaveManager::saveState(romFilename, slot, coreId, buf, stateBytes, batteryRam, batteryBytes);
  bmo_psram_free(buf);
  return ok;
}

template <typename DeserializeFn>
inline bool loadPsramState(const char* romFilename, int slot, uint32_t coreId, size_t capacity,
                           DeserializeFn&& deserialize, uint8_t* batteryRam = nullptr, size_t batteryCap = 0,
                           size_t* outBatteryBytes = nullptr) {
  if (!romFilename || capacity == 0) return false;
  uint8_t* buf = static_cast<uint8_t*>(bmo_psram_malloc(capacity));
  if (!buf) return false;

  size_t loadedBytes = 0;
  size_t dummyRam = 0;
  size_t* ramLenOut = (outBatteryBytes != nullptr) ? outBatteryBytes : (batteryRam ? &dummyRam : nullptr);
  bool ok =
      SaveManager::loadState(romFilename, slot, coreId, buf, capacity, &loadedBytes, batteryRam, batteryCap, ramLenOut);
  if (ok && loadedBytes > 0) {
    ok = deserialize(buf, loadedBytes);
  } else {
    ok = false;
  }
  bmo_psram_free(buf);
  return ok;
}

} // namespace CoreHelpers
