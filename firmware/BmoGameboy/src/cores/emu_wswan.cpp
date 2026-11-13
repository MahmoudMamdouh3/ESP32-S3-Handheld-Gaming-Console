#pragma GCC optimize ("O3,unroll-loops")
#include "emu_wswan.h"
#include "../hal/config.h"
#include "../ui/display_emu.h"
#include "../ui/theme.h"
#include <Arduino.h>

#if BMO_EXPERIMENTAL_CORES
#include "../hal/buttons.h"
#include "../hal/panel_color.h"
#include "../hal/psram_alloc.h"
#include "../storage/save_manager.h"
#include <string.h>

extern "C" {
  #include "../third_party/oswan/WS.h"
  #include "../third_party/oswan/WSRender.h"
  #include "../third_party/oswan/WSFileio.h"
}

namespace {
  constexpr int kWsWidth = 224;
  constexpr int kWsHeight = 144;
  uint16_t* s_panelFb = nullptr; // 224 x 144 panel BGR565 in PSRAM
  bool s_running = false;

  // WonderSwan KEYCTL bit layout matching oswan IO[0xB5]
  enum : int {
    WS_Y1     = 1 << 0,
    WS_Y2     = 1 << 1,
    WS_Y3     = 1 << 2,
    WS_Y4     = 1 << 3,
    WS_X1     = 1 << 4,
    WS_X2     = 1 << 5,
    WS_X3     = 1 << 6,
    WS_X4     = 1 << 7,
    WS_OPTION = 1 << 8,
    WS_START  = 1 << 9,
    WS_A      = 1 << 10,
    WS_B      = 1 << 11,
  };

  size_t wsTotalRamSaveBytes() {
    if (RAMBanks <= 0 || RAMSize <= 0 || !RAMMap) return 0;
    return (RAMSize < 0x10000) ? static_cast<size_t>(RAMSize) : static_cast<size_t>(RAMBanks) * 0x10000U;
  }
} // namespace

extern "C" void ws_graphics_paint(void) {
  if (!s_panelFb || !FrameBuffer) return;
  for (int y = 0; y < kWsHeight; ++y) {
    const uint16_t* src = reinterpret_cast<const uint16_t*>(FrameBuffer) + (size_t)y * SCREEN_WIDTH;
    uint16_t* dst = s_panelFb + (size_t)y * kWsWidth;
    for (int x = 0; x < kWsWidth; ++x) {
      dst[x] = bmo_rgb565_to_panel565(src[x]);
    }
  }
}

extern "C" int ws_input_poll(int mode) {
  int state = 0;
  const bool u = Buttons::get(Buttons::UP).pressed;
  const bool d = Buttons::get(Buttons::DOWN).pressed;
  const bool l = Buttons::get(Buttons::LEFT).pressed;
  const bool r = Buttons::get(Buttons::RIGHT).pressed;

  if (mode == 0) {
    if (u) state |= WS_X1;
    if (r) state |= WS_X2;
    if (d) state |= WS_X3;
    if (l) state |= WS_X4;
  } else {
    if (u) state |= WS_Y1;
    if (r) state |= WS_Y2;
    if (d) state |= WS_Y3;
    if (l) state |= WS_Y4;
  }

  if (Buttons::get(Buttons::A).pressed)      state |= WS_A;
  if (Buttons::get(Buttons::B).pressed)      state |= WS_B;
  if (Buttons::get(Buttons::START).pressed)  state |= WS_START;
  if (Buttons::get(Buttons::SELECT).pressed) state |= WS_OPTION;
  return state;
}

extern "C" unsigned long SDL_UXTimerRead(void) {
  return static_cast<unsigned long>(millis());
}

bool WSwanEmu::init(uint8_t* romData, size_t romSize, bool isColor) {
  (void)isColor;
  destroy();
  if (!romData || romSize < 16U) return false;

  s_panelFb = static_cast<uint16_t*>(bmo_psram_calloc((size_t)kWsWidth * kWsHeight, sizeof(uint16_t)));
  if (!s_panelFb) {
    destroy();
    return false;
  }

  WsInit();
  if (WsCreateFromMemory(romData, romSize) != 0) {
    destroy();
    return false;
  }
  WsReset();

  s_running = true;
  return true;
}

void WSwanEmu::update() {
  if (!s_running || !s_panelFb) return;
  WsRun();
  DisplayEmu::streamWSwanFrame(s_panelFb, kWsWidth, kWsHeight);
}

void WSwanEmu::destroy() {
  s_running = false;
  WsDeInit();
  if (s_panelFb) {
    bmo_psram_free(s_panelFb);
    s_panelFb = nullptr;
  }
}

bool WSwanEmu::isRunning() {
  return s_running;
}

bool WSwanEmu::saveBatteryRam(const char* romFilename) {
  const size_t totalBytes = wsTotalRamSaveBytes();
  if (!s_running || !romFilename || totalBytes == 0) return false;
  if (RAMBanks == 1 || RAMSize < 0x10000) {
    return RAMMap[0] && SaveManager::saveBatteryRam(romFilename, RAMMap[0], totalBytes);
  }
  uint8_t* staging = static_cast<uint8_t*>(bmo_psram_malloc(totalBytes));
  if (!staging) return false;
  for (int i = 0; i < RAMBanks; ++i) {
    if (RAMMap[i]) memcpy(staging + (size_t)i * 0x10000U, RAMMap[i], 0x10000U);
    else           memset(staging + (size_t)i * 0x10000U, 0, 0x10000U);
  }
  bool ok = SaveManager::saveBatteryRam(romFilename, staging, totalBytes);
  bmo_psram_free(staging);
  return ok;
}

bool WSwanEmu::loadBatteryRam(const char* romFilename) {
  const size_t totalBytes = wsTotalRamSaveBytes();
  if (!s_running || !romFilename || totalBytes == 0) return false;
  if (RAMBanks == 1 || RAMSize < 0x10000) {
    size_t loaded = 0;
    return RAMMap[0] && SaveManager::loadBatteryRam(romFilename, RAMMap[0], totalBytes, &loaded);
  }
  uint8_t* staging = static_cast<uint8_t*>(bmo_psram_malloc(totalBytes));
  if (!staging) return false;
  size_t loaded = 0;
  bool ok = SaveManager::loadBatteryRam(romFilename, staging, totalBytes, &loaded);
  if (ok) {
    for (int i = 0; i < RAMBanks && (size_t)i * 0x10000U < loaded; ++i) {
      size_t remain = loaded - (size_t)i * 0x10000U;
      size_t chunk = remain < 0x10000U ? remain : 0x10000U;
      if (RAMMap[i]) memcpy(RAMMap[i], staging + (size_t)i * 0x10000U, chunk);
    }
  }
  bmo_psram_free(staging);
  return ok;
}

bool WSwanEmu::saveState(const char* romFilename, int slot) {
  if (!s_running || !romFilename) return false;
  const size_t cap = WsStateGetSize();
  uint8_t* buf = static_cast<uint8_t*>(bmo_psram_malloc(cap));
  if (!buf) return false;

  size_t outBytes = 0;
  bool ok = (WsSaveStateToMem(buf, cap, &outBytes) != 0);
  if (ok) {
    ok = SaveManager::saveState(romFilename, slot, SaveManager::CORE_WSWAN, buf, outBytes);
  }
  bmo_psram_free(buf);
  return ok;
}

bool WSwanEmu::loadState(const char* romFilename, int slot) {
  if (!s_running || !romFilename) return false;
  const size_t cap = WsStateGetSize();
  uint8_t* buf = static_cast<uint8_t*>(bmo_psram_malloc(cap));
  if (!buf) return false;

  size_t outState = 0;
  bool ok = SaveManager::loadState(romFilename, slot, SaveManager::CORE_WSWAN, buf, cap, &outState);
  if (ok) {
    ok = (WsLoadStateFromMem(buf, outState) != 0);
  }
  bmo_psram_free(buf);
  return ok;
}

#else // BMO_EXPERIMENTAL_CORES == 0
bool WSwanEmu::init(uint8_t* romData, size_t romSize, bool isColor) {
  (void)isColor;
  (void)romData;
  (void)romSize;
  LOG_WARN_STR("[wswan] Experimental core disabled (-DBMO_EXPERIMENTAL_CORES=1 to enable)");
  DisplayEmu::showToast("EXPERIMENTAL CORE OFF", Theme::ACCENT_RED, 2200);
  return false;
}

void WSwanEmu::update() {}
void WSwanEmu::destroy() {}
bool WSwanEmu::isRunning() { return false; }
bool WSwanEmu::saveBatteryRam(const char* romFilename) { (void)romFilename; return false; }
bool WSwanEmu::loadBatteryRam(const char* romFilename) { (void)romFilename; return false; }
bool WSwanEmu::saveState(const char* romFilename, int slot) { (void)romFilename; (void)slot; return false; }
bool WSwanEmu::loadState(const char* romFilename, int slot) { (void)romFilename; (void)slot; return false; }

#endif // BMO_EXPERIMENTAL_CORES
