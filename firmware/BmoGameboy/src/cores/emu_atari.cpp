#pragma GCC optimize("O3,unroll-loops")
#include "emu_atari.h"
#include "../hal/config.h"

#if BMO_EXPERIMENTAL_CORES

#include <Arduino.h>
#include <string.h>
#include <limits.h>

#include "../hal/buttons.h"
#include "../hal/psram_alloc.h"
#include "../hal/panel_color.h"
#include "../storage/save_manager.h"
#include "../ui/display_emu.h"

#include "../third_party/stella2014/src/bspf.h"
#include "../third_party/stella2014/src/Cart.h"
#include "../third_party/stella2014/src/Console.h"
#include "../third_party/stella2014/src/Event.h"
#include "../third_party/stella2014/src/EventHandler.h"
#include "../third_party/stella2014/src/MD5.h"
#include "../third_party/stella2014/src/OSystem.h"
#include "../third_party/stella2014/src/Props.h"
#include "../third_party/stella2014/src/PropsSet.h"
#include "../third_party/stella2014/src/Serializer.h"
#include "../third_party/stella2014/src/Settings.h"
#include "../third_party/stella2014/src/StateManager.h"
#include "../third_party/stella2014/src/TIA.h"
#include "../third_party/stella2014/src/TIATables.h"

namespace {
constexpr int ATARI_W = 160;
constexpr int ATARI_H = 192;
constexpr size_t ATARI_STATE_BUF_MAX = 128U * 1024U;

OSystem* s_osystem = nullptr;
Settings* s_settings = nullptr;
Console* s_console = nullptr;
StateManager* s_stateMgr = nullptr;
uint16_t* s_panelFb = nullptr;
uint16_t s_palette565[256];
bool s_running = false;

void refreshPaletteLut() {
  if (!s_console) return;
  const uint32_t* pal = s_console->getPalette(0);
  if (!pal) return;
  for (int i = 0; i < 256; ++i) {
    // Stella Console stores palette words as 0x00RRGGBB when XBGR8888 is unset.
    const uint32_t c = pal[i];
    const uint8_t r = (uint8_t)((c >> 16) & 0xFFU);
    const uint8_t g = (uint8_t)((c >> 8) & 0xFFU);
    const uint8_t b = (uint8_t)(c & 0xFFU);
    s_palette565[i] = bmo_rgb888_to_panel565(r, g, b);
  }
}

void renderTiaToPanel() {
  if (!s_console || !s_panelFb) return;
  TIA& tia = s_console->tia();
  const uint8_t* src = tia.currentFrameBuffer();
  const uint32_t tiaW = tia.width();
  const uint32_t tiaH = tia.height();
  if (!src || tiaW == 0 || tiaH == 0) return;

  const uint32_t copyW = (tiaW < (uint32_t)ATARI_W) ? tiaW : (uint32_t)ATARI_W;
  memset(s_panelFb, 0, ATARI_W * ATARI_H * sizeof(uint16_t));

  if (tiaH <= (uint32_t)ATARI_H) {
    const int yPad = (ATARI_H - (int)tiaH) / 2;
    for (uint32_t sy = 0; sy < tiaH; ++sy) {
      const uint8_t* srow = src + sy * tiaW;
      uint16_t* drow = s_panelFb + (yPad + (int)sy) * ATARI_W;
      for (uint32_t x = 0; x < copyW; ++x) {
        drow[x] = s_palette565[srow[x]];
      }
    }
  } else if (tiaH <= 216U) {
    // Standard NTSC 210-line frame: center-crop overscan so pixels stay 1:1 sharp.
    const uint32_t yStart = (tiaH - (uint32_t)ATARI_H) / 2U;
    for (int dy = 0; dy < ATARI_H; ++dy) {
      const uint8_t* srow = src + (yStart + (uint32_t)dy) * tiaW;
      uint16_t* drow = s_panelFb + dy * ATARI_W;
      for (uint32_t x = 0; x < copyW; ++x) {
        drow[x] = s_palette565[srow[x]];
      }
    }
  } else {
    // PAL/SECAM tall frames (>216 lines): uniform vertical scale keeps scorebars visible.
    for (int dy = 0; dy < ATARI_H; ++dy) {
      const uint32_t sy = ((uint32_t)dy * tiaH) / (uint32_t)ATARI_H;
      const uint8_t* srow = src + sy * tiaW;
      uint16_t* drow = s_panelFb + dy * ATARI_W;
      for (uint32_t x = 0; x < copyW; ++x) {
        drow[x] = s_palette565[srow[x]];
      }
    }
  }

  DisplayEmu::streamAtariFrame(s_panelFb);
}
}  // namespace

bool AtariEmu::begin(const uint8_t* romData, size_t romSize) {
  if (!romData || romSize == 0 || romSize > 512UL * 1024UL) {
    return false;
  }
  destroy();

  if (!TIATables::allocateAllTables()) {
    destroy();
    return false;
  }

  s_panelFb = (uint16_t*)bmo_psram_calloc(ATARI_W * ATARI_H, sizeof(uint16_t));
  if (!s_panelFb) {
    destroy();
    return false;
  }

  s_osystem = new OSystem();
  if (!s_osystem) {
    destroy();
    return false;
  }

  s_settings = new Settings(s_osystem);
  if (!s_settings) {
    destroy();
    return false;
  }
  s_osystem->attach(s_settings);
  s_settings->setValue("palette", "standard");
  s_settings->setValue("fastscbios", true);
  s_settings->setValue("rominfo", false);
  s_settings->setValue("tiadriven", false);
  s_settings->setValue("ramrandom", true);
  s_settings->setValue("joyallow4", false);

  string cartMD5 = MD5(romData, (uint32_t)romSize);
  Properties props;
  s_osystem->propSet().getMD5(cartMD5, props);

  string cartType = props.get(Cartridge_Type);
  string cartId;
  Cartridge* cart = Cartridge::create(romData, (uint32_t)romSize, cartMD5,
                                      cartType, cartId, *s_osystem,
                                      *s_settings, "");
  if (!cart) {
    destroy();
    return false;
  }

  // Console::Console triggers system resets during setup; Stubs.cpp guards null myConsole.
  s_console = new Console(s_osystem, cart, props);
  if (!s_console) {
    delete cart;
    destroy();
    return false;
  }
  s_osystem->myConsole = s_console;
  s_console->initializeVideo();
  s_console->initializeAudio();

  s_stateMgr = new StateManager(s_osystem);
  if (!s_stateMgr) {
    destroy();
    return false;
  }

  refreshPaletteLut();
  s_running = true;
  return true;
}

void AtariEmu::updateJoypad() {
  if (!s_running || !s_osystem) return;
  Event& ev = s_osystem->eventHandler().event();

  const bool up = Buttons::get(Buttons::UP).pressed;
  const bool down = Buttons::get(Buttons::DOWN).pressed;
  const bool left = Buttons::get(Buttons::LEFT).pressed;
  const bool right = Buttons::get(Buttons::RIGHT).pressed;
  const bool fire = Buttons::get(Buttons::A).pressed || Buttons::get(Buttons::B).pressed;
  const bool selectBtn = Buttons::get(Buttons::SELECT).pressed;
  const bool resetBtn = Buttons::get(Buttons::START).pressed;

  ev.set(Event::JoystickZeroUp, up ? 1 : 0);
  ev.set(Event::JoystickZeroDown, down ? 1 : 0);
  ev.set(Event::JoystickZeroLeft, left ? 1 : 0);
  ev.set(Event::JoystickZeroRight, right ? 1 : 0);
  ev.set(Event::JoystickZeroFire, fire ? 1 : 0);
  ev.set(Event::ConsoleSelect, selectBtn ? 1 : 0);
  ev.set(Event::ConsoleReset, resetBtn ? 1 : 0);
}

void AtariEmu::runFrame() {
  if (!s_running || !s_console) return;
  s_console->tia().update();
  renderTiaToPanel();
}

void AtariEmu::destroy() {
  s_running = false;
  if (s_stateMgr) {
    delete s_stateMgr;
    s_stateMgr = nullptr;
  }
  if (s_osystem) {
    s_osystem->myConsole = nullptr;
  }
  if (s_console) {
    delete s_console;
    s_console = nullptr;
  }
  if (s_settings) {
    delete s_settings;
    s_settings = nullptr;
  }
  if (s_osystem) {
    delete s_osystem;
    s_osystem = nullptr;
  }
  if (s_panelFb) {
    bmo_psram_free(s_panelFb);
    s_panelFb = nullptr;
  }
  TIATables::freeAllTables();
}

bool AtariEmu::saveBatteryRam(const char* romFilename) {
  (void)romFilename;
  return false;
}

bool AtariEmu::loadBatteryRam(const char* romFilename) {
  (void)romFilename;
  return false;
}

bool AtariEmu::saveState(const char* romFilename, int slot) {
  if (!s_running || !s_stateMgr || !romFilename) return false;
  Serializer ser;
  if (!s_stateMgr->saveState(ser) || ser.size() == 0 || !ser.data()) {
    return false;
  }
  return SaveManager::saveState(romFilename, slot, SaveManager::CORE_ATARI,
                                ser.data(), ser.size(), nullptr, 0);
}

bool AtariEmu::loadState(const char* romFilename, int slot) {
  if (!s_running || !s_stateMgr || !romFilename) return false;
  uint8_t* stateBuf = (uint8_t*)bmo_psram_malloc(ATARI_STATE_BUF_MAX);
  if (!stateBuf) return false;

  size_t outSize = 0;
  bool ok = SaveManager::loadState(romFilename, slot, SaveManager::CORE_ATARI,
                                   stateBuf, ATARI_STATE_BUF_MAX, &outSize,
                                   nullptr, 0, nullptr);
  if (ok && outSize > 0) {
    Serializer ser;
    ok = ser.loadFromBuffer(stateBuf, outSize) && s_stateMgr->loadState(ser);
    if (ok) refreshPaletteLut();
  } else {
    ok = false;
  }

  bmo_psram_free(stateBuf);
  return ok;
}

#else  // BMO_EXPERIMENTAL_CORES == 0

bool AtariEmu::begin(const uint8_t* romData, size_t romSize) {
  (void)romData;
  (void)romSize;
  return false;
}

void AtariEmu::updateJoypad() {}
void AtariEmu::runFrame() {}
void AtariEmu::destroy() {}

bool AtariEmu::saveBatteryRam(const char* romFilename) {
  (void)romFilename;
  return false;
}

bool AtariEmu::loadBatteryRam(const char* romFilename) {
  (void)romFilename;
  return false;
}

bool AtariEmu::saveState(const char* romFilename, int slot) {
  (void)romFilename;
  (void)slot;
  return false;
}

bool AtariEmu::loadState(const char* romFilename, int slot) {
  (void)romFilename;
  (void)slot;
  return false;
}

#endif
