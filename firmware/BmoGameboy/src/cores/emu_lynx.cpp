#pragma GCC optimize("O3,unroll-loops")
#include "emu_lynx.h"
#include "../hal/config.h"
#include "../ui/display_emu.h"
#include "../ui/theme.h"
#include <Arduino.h>

#if BMO_EXPERIMENTAL_CORES
#include "../hal/buttons.h"
#include "../hal/panel_color.h"
#include "../hal/psram_alloc.h"
#include "../storage/save_manager.h"
#include "core_helpers.h"
#include "../third_party/handy/handy.h"

namespace {
constexpr size_t kLynxStateCap = 192U * 1024U;

CSystem* s_lynx = nullptr;
uint16_t* s_rawFb = nullptr;   // 160 x 102 RGB565 host-endian in PSRAM
uint16_t* s_panelFb = nullptr; // 160 x 102 ST7789 panel BGR565 in PSRAM
bool s_running = false;
} // namespace

bool LynxEmu::init(uint8_t* romData, size_t romSize) {
  destroy();
  if (!romData || romSize < 64) return false;

  s_rawFb = static_cast<uint16_t*>(bmo_psram_calloc(HANDY_SCREEN_WIDTH * HANDY_SCREEN_HEIGHT, sizeof(uint16_t)));
  s_panelFb = static_cast<uint16_t*>(bmo_psram_calloc(HANDY_SCREEN_WIDTH * HANDY_SCREEN_HEIGHT, sizeof(uint16_t)));
  if (!s_rawFb || !s_panelFb) {
    destroy();
    return false;
  }

  gAudioEnabled = 0;
  gPrimaryFrameBuffer = reinterpret_cast<UBYTE*>(s_rawFb);

  s_lynx = new CSystem(romData, static_cast<ULONG>(romSize), MIKIE_PIXEL_FORMAT_16BPP_565, HANDY_AUDIO_SAMPLE_FREQ);
  if (!s_lynx || s_lynx->mFileType == HANDY_FILETYPE_ILLEGAL) {
    destroy();
    return false;
  }

  s_lynx->mMikie->SetRotation(MIKIE_NO_ROTATE);
  s_running = true;
  return true;
}

void LynxEmu::update() {
  if (!s_running || !s_lynx || !s_rawFb || !s_panelFb) return;

  static constexpr CoreHelpers::ButtonBit kPadMap[] = {
      {Buttons::UP, BUTTON_UP},       {Buttons::DOWN, BUTTON_DOWN},   {Buttons::LEFT, BUTTON_LEFT},
      {Buttons::RIGHT, BUTTON_RIGHT}, {Buttons::A, BUTTON_A},         {Buttons::B, BUTTON_B},
      {Buttons::START, BUTTON_PAUSE}, {Buttons::SELECT, BUTTON_OPT1}, {Buttons::X, BUTTON_OPT2},
  };
  const ULONG pad = static_cast<ULONG>(CoreHelpers::readButtonMask(kPadMap));

  s_lynx->SetButtonData(pad);
  s_lynx->UpdateFrame(true);

  const int count = HANDY_SCREEN_WIDTH * HANDY_SCREEN_HEIGHT;
  for (int i = 0; i < count; ++i) {
    s_panelFb[i] = bmo_rgb565_to_panel565(s_rawFb[i]);
  }
  DisplayEmu::streamLynxFrame(s_panelFb, HANDY_SCREEN_WIDTH, HANDY_SCREEN_HEIGHT);
}

void LynxEmu::destroy() {
  s_running = false;
  gPrimaryFrameBuffer = nullptr;
  if (s_lynx) {
    delete s_lynx;
    s_lynx = nullptr;
  }
  if (s_rawFb) {
    bmo_psram_free(s_rawFb);
    s_rawFb = nullptr;
  }
  if (s_panelFb) {
    bmo_psram_free(s_panelFb);
    s_panelFb = nullptr;
  }
}

bool LynxEmu::isRunning() {
  return s_running && s_lynx != nullptr;
}

bool LynxEmu::saveBatteryRam(const char* romFilename) {
  if (!s_running || !s_lynx || !s_lynx->mEEPROM || !romFilename) return false;
  if (!s_lynx->mEEPROM->Available() || s_lynx->mEEPROM->Size() <= 0) return false;
  return SaveManager::saveBatteryRam(romFilename, s_lynx->mEEPROM->Data(),
                                     static_cast<size_t>(s_lynx->mEEPROM->Size()));
}

bool LynxEmu::loadBatteryRam(const char* romFilename) {
  if (!s_running || !s_lynx || !s_lynx->mEEPROM || !romFilename) return false;
  if (!s_lynx->mEEPROM->Available() || s_lynx->mEEPROM->Size() <= 0) return false;
  const int sz = s_lynx->mEEPROM->Size();
  uint8_t* tmp = static_cast<uint8_t*>(bmo_psram_malloc(static_cast<size_t>(sz)));
  if (!tmp) return false;

  size_t loaded = 0;
  bool ok = SaveManager::loadBatteryRam(romFilename, tmp, static_cast<size_t>(sz), &loaded) && loaded > 0;
  if (ok) {
    s_lynx->mEEPROM->InitFrom(reinterpret_cast<char*>(tmp), static_cast<int>(loaded));
  }
  bmo_psram_free(tmp);
  return ok;
}

bool LynxEmu::saveState(const char* romFilename, int slot) {
  if (!s_running || !s_lynx || !romFilename) return false;
  return CoreHelpers::savePsramState(romFilename, slot, SaveManager::CORE_LYNX, kLynxStateCap,
                                     [](uint8_t* buf, size_t cap, size_t* outSize) {
                                       LSS_FILE lss = {buf, 0, static_cast<ULONG>(cap)};
                                       if (!s_lynx->ContextSave(&lss) || lss.index == 0) return false;
                                       *outSize = static_cast<size_t>(lss.index);
                                       return true;
                                     });
}

bool LynxEmu::loadState(const char* romFilename, int slot) {
  if (!s_running || !s_lynx || !romFilename) return false;
  return CoreHelpers::loadPsramState(romFilename, slot, SaveManager::CORE_LYNX, kLynxStateCap,
                                     [](uint8_t* buf, size_t loaded) {
                                       LSS_FILE lss = {buf, 0, static_cast<ULONG>(loaded)};
                                       return s_lynx->ContextLoad(&lss);
                                     });
}

#else // BMO_EXPERIMENTAL_CORES == 0
bool LynxEmu::init(uint8_t* romData, size_t romSize) {
  (void)romData;
  (void)romSize;
  LOG_WARN_STR("[lynx] Experimental core disabled (-DBMO_EXPERIMENTAL_CORES=1 to enable)");
  DisplayEmu::showToast("EXPERIMENTAL CORE OFF", Theme::ACCENT_RED, 2200);
  return false;
}

void LynxEmu::update() {}
void LynxEmu::destroy() {}
bool LynxEmu::isRunning() {
  return false;
}
bool LynxEmu::saveBatteryRam(const char* romFilename) {
  (void)romFilename;
  return false;
}
bool LynxEmu::loadBatteryRam(const char* romFilename) {
  (void)romFilename;
  return false;
}
bool LynxEmu::saveState(const char* romFilename, int slot) {
  (void)romFilename;
  (void)slot;
  return false;
}
bool LynxEmu::loadState(const char* romFilename, int slot) {
  (void)romFilename;
  (void)slot;
  return false;
}

#endif // BMO_EXPERIMENTAL_CORES
