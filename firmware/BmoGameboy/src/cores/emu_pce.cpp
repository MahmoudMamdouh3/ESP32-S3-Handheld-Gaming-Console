#pragma GCC optimize ("O3,unroll-loops")
#include "emu_pce.h"
#include "../hal/config.h"
#include "../ui/display_emu.h"
#include "../ui/theme.h"
#include <Arduino.h>

#if BMO_EXPERIMENTAL_CORES
#include "../hal/buttons.h"
#include "../hal/panel_color.h"
#include "../hal/psram_alloc.h"
#include "../storage/battery_save.h"
#include "../storage/save_manager.h"
#include <string.h>

extern "C" {
#include "../third_party/huexpress/pce-go.h"
#include "../third_party/huexpress/pce.h"
}

namespace {
  constexpr int kPcePanelW = 256;
  constexpr int kPcePanelH = 240;

  uint8_t*  s_rawFb = nullptr;      // 16-byte scratch prefix + XBUF_WIDTH * (XBUF_HEIGHT + 1)
  uint16_t* s_panelFb = nullptr;    // 256 x 240 ST7789 panel BGR565 in PSRAM
  uint16_t* s_palette565 = nullptr; // 256-entry pre-swapped BGR565 LUT
  bool      s_running = false;
  uint32_t  s_prevSrcW = 0;
  uint32_t  s_prevSrcH = 0;
}

extern "C" uint8_t* osd_gfx_framebuffer(int width, int height) {
  (void)width;
  (void)height;
  return s_rawFb ? (s_rawFb + 16) : nullptr;
}

extern "C" void osd_input_read(uint8_t joypads[8]) {
  uint8_t pad = 0;
  if (Buttons::get(Buttons::B).pressed)      pad |= JOY_A;       // Button I
  if (Buttons::get(Buttons::A).pressed)      pad |= JOY_B;       // Button II
  if (Buttons::get(Buttons::SELECT).pressed) pad |= JOY_SELECT;
  if (Buttons::get(Buttons::START).pressed)  pad |= JOY_RUN;
  if (Buttons::get(Buttons::UP).pressed)     pad |= JOY_UP;
  if (Buttons::get(Buttons::RIGHT).pressed)  pad |= JOY_RIGHT;
  if (Buttons::get(Buttons::DOWN).pressed)   pad |= JOY_DOWN;
  if (Buttons::get(Buttons::LEFT).pressed)   pad |= JOY_LEFT;
  joypads[0] = pad;
  memset(&joypads[1], 0, 7);
}

extern "C" void osd_vsync(void) {
  if (!s_running || !s_rawFb || !s_panelFb || !s_palette565) return;

  uint32_t srcW = PCE.VDC.screen_width;
  uint32_t srcH = PCE.VDC.screen_height;
  if (srcW == 0 || srcW > 352U) srcW = 256U;
  if (srcH == 0 || srcH > static_cast<uint32_t>(XBUF_HEIGHT)) srcH = 224U;

  if (srcW != s_prevSrcW || srcH != s_prevSrcH) {
    memset(s_panelFb, 0, static_cast<size_t>(kPcePanelW) * kPcePanelH * sizeof(uint16_t));
    s_prevSrcW = srcW;
    s_prevSrcH = srcH;
  }

  const uint32_t srcYStart = (srcH > static_cast<uint32_t>(kPcePanelH))
                               ? ((srcH - static_cast<uint32_t>(kPcePanelH)) / 2U)
                               : 0U;
  const uint32_t dstYStart = (srcH < static_cast<uint32_t>(kPcePanelH))
                               ? ((static_cast<uint32_t>(kPcePanelH) - srcH) / 2U)
                               : 0U;
  const uint32_t copyH = (srcH <= static_cast<uint32_t>(kPcePanelH))
                           ? srcH
                           : static_cast<uint32_t>(kPcePanelH);

  const uint8_t* srcBase = s_rawFb + 16;
  const uint16_t* pal = s_palette565;

  if (srcW == static_cast<uint32_t>(kPcePanelW)) {
    for (uint32_t y = 0; y < copyH; ++y) {
      const uint8_t* srcRow = srcBase + (srcYStart + y) * static_cast<uint32_t>(XBUF_WIDTH);
      uint16_t* dstRow = s_panelFb + (dstYStart + y) * static_cast<uint32_t>(kPcePanelW);
      for (int x = 0; x < kPcePanelW; ++x) {
        dstRow[x] = pal[srcRow[x]];
      }
    }
  } else {
    const uint32_t xStep = (srcW << 16) / static_cast<uint32_t>(kPcePanelW);
    for (uint32_t y = 0; y < copyH; ++y) {
      const uint8_t* srcRow = srcBase + (srcYStart + y) * static_cast<uint32_t>(XBUF_WIDTH);
      uint16_t* dstRow = s_panelFb + (dstYStart + y) * static_cast<uint32_t>(kPcePanelW);
      uint32_t acc = 0;
      for (int x = 0; x < kPcePanelW; ++x) {
        dstRow[x] = pal[srcRow[acc >> 16]];
        acc += xStep;
      }
    }
  }

  DisplayEmu::streamPCEFrame(s_panelFb);
}

bool PceEmu::init(const uint8_t* romData, size_t romSize) {
  destroy();
  if (!romData || romSize < 0x2000U) return false;

  s_rawFb = static_cast<uint8_t*>(
    bmo_psram_calloc(1, 16U + static_cast<size_t>(XBUF_WIDTH) * (XBUF_HEIGHT + 1U)));
  s_panelFb = static_cast<uint16_t*>(
    bmo_psram_calloc(static_cast<size_t>(kPcePanelW) * kPcePanelH, sizeof(uint16_t)));
  if (!s_rawFb || !s_panelFb) {
    destroy();
    return false;
  }

  s_palette565 = static_cast<uint16_t*>(PalettePCE(16));
  if (!s_palette565) {
    destroy();
    return false;
  }
  for (int i = 0; i < 256; ++i) {
    s_palette565[i] = bmo_rgb565_to_panel565(s_palette565[i]);
  }

  if (InitPCE(22050, false) != 0) {
    destroy();
    return false;
  }

  if (LoadCardBorrowed(romData, romSize) != 0) {
    destroy();
    return false;
  }

  ResetPCE(false);
  s_prevSrcW = 0;
  s_prevSrcH = 0;
  s_running = true;
  return true;
}

void PceEmu::update() {
  if (!s_running) return;
  osd_input_read(PCE.Joypad.regs);
  StepPCE();
}

void PceEmu::destroy() {
  s_running = false;
  s_prevSrcW = 0;
  s_prevSrcH = 0;
  ShutdownPCE();
  if (s_palette565) {
    bmo_psram_free(s_palette565);
    s_palette565 = nullptr;
  }
  if (s_panelFb) {
    bmo_psram_free(s_panelFb);
    s_panelFb = nullptr;
  }
  if (s_rawFb) {
    bmo_psram_free(s_rawFb);
    s_rawFb = nullptr;
  }
}

bool PceEmu::saveBatteryRam(const char* romFilename) {
  if (!s_running || !romFilename || !PCE.BackupRAM) return false;
  if (!BatterySave::pceBramHasHeader(PCE.BackupRAM, 0x2000U)) {
    return false;
  }
  return SaveManager::saveBatteryRam(romFilename, PCE.BackupRAM, 0x2000U);
}

bool PceEmu::loadBatteryRam(const char* romFilename) {
  if (!s_running || !romFilename || !PCE.BackupRAM) return false;
  return SaveManager::loadBatteryRam(romFilename, PCE.BackupRAM, 0x2000U);
}

bool PceEmu::saveState(const char* romFilename, int slot) {
  if (!s_running || !romFilename) return false;
  const size_t cap = PceGetSaveStateSize();
  if (cap == 0) return false;
  uint8_t* buf = static_cast<uint8_t*>(bmo_psram_malloc(cap));
  if (!buf) return false;

  size_t actualLen = 0;
  bool ok = (PceSaveStateMem(buf, cap, &actualLen) == 0) &&
            SaveManager::saveState(romFilename, slot, SaveManager::CORE_PCE,
                                   buf, actualLen);
  bmo_psram_free(buf);
  return ok;
}

bool PceEmu::loadState(const char* romFilename, int slot) {
  if (!s_running || !romFilename) return false;
  const size_t cap = PceGetSaveStateSize();
  if (cap == 0) return false;
  uint8_t* buf = static_cast<uint8_t*>(bmo_psram_malloc(cap));
  if (!buf) return false;

  size_t loadedLen = 0;
  bool ok = SaveManager::loadState(romFilename, slot, SaveManager::CORE_PCE,
                                   buf, cap, &loadedLen) &&
            (PceLoadStateMem(buf, loadedLen) == 0);
  bmo_psram_free(buf);
  return ok;
}

#else // BMO_EXPERIMENTAL_CORES == 0
bool PceEmu::init(const uint8_t* romData, size_t romSize) {
  (void)romData;
  (void)romSize;
  LOG_WARN_STR("[pce] Experimental core disabled (-DBMO_EXPERIMENTAL_CORES=1 to enable)");
  DisplayEmu::showToast("EXPERIMENTAL CORE OFF", Theme::ACCENT_RED, 2200);
  return false;
}

void PceEmu::update() {}
void PceEmu::destroy() {}
bool PceEmu::saveBatteryRam(const char* romFilename) { (void)romFilename; return false; }
bool PceEmu::loadBatteryRam(const char* romFilename) { (void)romFilename; return false; }
bool PceEmu::saveState(const char* romFilename, int slot) { (void)romFilename; (void)slot; return false; }
bool PceEmu::loadState(const char* romFilename, int slot) { (void)romFilename; (void)slot; return false; }

#endif // BMO_EXPERIMENTAL_CORES
