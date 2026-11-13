#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../firmware/BmoGameboy/src/third_party/stella2014/src/bspf.h"
#include "../firmware/BmoGameboy/src/third_party/stella2014/src/Cart.h"
#include "../firmware/BmoGameboy/src/third_party/stella2014/src/Console.h"
#include "../firmware/BmoGameboy/src/third_party/stella2014/src/Event.h"
#include "../firmware/BmoGameboy/src/third_party/stella2014/src/EventHandler.h"
#include "../firmware/BmoGameboy/src/third_party/stella2014/src/MD5.h"
#include "../firmware/BmoGameboy/src/third_party/stella2014/src/OSystem.h"
#include "../firmware/BmoGameboy/src/third_party/stella2014/src/Props.h"
#include "../firmware/BmoGameboy/src/third_party/stella2014/src/PropsSet.h"
#include "../firmware/BmoGameboy/src/third_party/stella2014/src/Serializer.h"
#include "../firmware/BmoGameboy/src/third_party/stella2014/src/Settings.h"
#include "../firmware/BmoGameboy/src/third_party/stella2014/src/StateManager.h"
#include "../firmware/BmoGameboy/src/third_party/stella2014/src/TIA.h"
#include "../firmware/BmoGameboy/src/third_party/stella2014/src/TIATables.h"

extern "C" uint8_t* bmo_emu_rom_buffer();
extern "C" uint32_t bmo_emu_rom_capacity();

namespace {

constexpr uint32_t SCREEN_WIDTH = 160;
constexpr uint32_t SCREEN_HEIGHT = 192;
constexpr size_t FRAMEBUFFER_SIZE = SCREEN_WIDTH * SCREEN_HEIGHT;

OSystem* s_osystem = nullptr;
Settings* s_settings = nullptr;
Console* s_console = nullptr;
StateManager* s_stateMgr = nullptr;
Serializer* s_snapshot = nullptr;
uint16_t s_palette565[256];
uint16_t s_framebuffer[FRAMEBUFFER_SIZE];
uint16_t s_snapshotFb[FRAMEBUFFER_SIZE];
uint32_t s_romChecksum = 0;
uint32_t s_frameCount = 0;
uint32_t s_buttonMask = 0;
uint32_t s_snapshotFrameCount = 0;
uint32_t s_snapshotRomChecksum = 0;
uint32_t s_snapshotButtonMask = 0;
int32_t s_error = 0;
bool s_running = false;
bool s_snapshotValid = false;

uint16_t rgb888ToBrowser565(uint32_t rgb) {
  const uint16_t r = static_cast<uint16_t>((rgb >> 16) & 0xFFU);
  const uint16_t g = static_cast<uint16_t>((rgb >> 8) & 0xFFU);
  const uint16_t b = static_cast<uint16_t>(rgb & 0xFFU);
  return static_cast<uint16_t>(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

void refreshPaletteLut() {
  if (!s_console) return;
  const uint32_t* pal = s_console->getPalette(0);
  if (!pal) return;
  for (int i = 0; i < 256; ++i) {
    s_palette565[i] = rgb888ToBrowser565(pal[i]);
  }
}

void updateFramebuffer() {
  if (!s_console) return;
  TIA& tia = s_console->tia();
  const uint8_t* src = tia.currentFrameBuffer();
  const uint32_t tiaW = tia.width();
  const uint32_t tiaH = tia.height();
  if (!src || tiaW == 0 || tiaH == 0) return;

  const uint32_t copyW = (tiaW < SCREEN_WIDTH) ? tiaW : SCREEN_WIDTH;
  memset(s_framebuffer, 0, sizeof(s_framebuffer));

  if (tiaH <= SCREEN_HEIGHT) {
    const uint32_t yPad = (SCREEN_HEIGHT - tiaH) / 2U;
    for (uint32_t sy = 0; sy < tiaH; ++sy) {
      const uint8_t* srow = src + sy * tiaW;
      uint16_t* drow = s_framebuffer + (yPad + sy) * SCREEN_WIDTH;
      for (uint32_t x = 0; x < copyW; ++x) {
        drow[x] = s_palette565[srow[x]];
      }
    }
  } else if (tiaH <= 216U) {
    const uint32_t yStart = (tiaH - SCREEN_HEIGHT) / 2U;
    for (uint32_t dy = 0; dy < SCREEN_HEIGHT; ++dy) {
      const uint8_t* srow = src + (yStart + dy) * tiaW;
      uint16_t* drow = s_framebuffer + dy * SCREEN_WIDTH;
      for (uint32_t x = 0; x < copyW; ++x) {
        drow[x] = s_palette565[srow[x]];
      }
    }
  } else {
    for (uint32_t dy = 0; dy < SCREEN_HEIGHT; ++dy) {
      const uint32_t sy = (dy * tiaH) / SCREEN_HEIGHT;
      const uint8_t* srow = src + sy * tiaW;
      uint16_t* drow = s_framebuffer + dy * SCREEN_WIDTH;
      for (uint32_t x = 0; x < copyW; ++x) {
        drow[x] = s_palette565[srow[x]];
      }
    }
  }
}

uint32_t hashRom(const uint8_t* rom, uint32_t size) {
  uint32_t hash = 2166136261U;
  for (uint32_t i = 0; i < size; ++i) {
    hash = (hash ^ rom[i]) * 16777619U;
  }
  return hash;
}

void applyButtons() {
  if (!s_osystem) return;
  Event& ev = s_osystem->eventHandler().event();
  ev.set(Event::JoystickZeroUp,    (s_buttonMask & 0x01U) ? 1 : 0);
  ev.set(Event::JoystickZeroDown,  (s_buttonMask & 0x02U) ? 1 : 0);
  ev.set(Event::JoystickZeroLeft,  (s_buttonMask & 0x04U) ? 1 : 0);
  ev.set(Event::JoystickZeroRight, (s_buttonMask & 0x08U) ? 1 : 0);
  ev.set(Event::JoystickZeroFire,  ((s_buttonMask & 0x10U) || (s_buttonMask & 0x20U)) ? 1 : 0);
  ev.set(Event::ConsoleReset,      (s_buttonMask & 0x40U) ? 1 : 0);
  ev.set(Event::ConsoleSelect,    (s_buttonMask & 0x80U) ? 1 : 0);
}

void releaseCore() {
  s_running = false;
  delete s_snapshot;
  s_snapshot = nullptr;
  delete s_stateMgr;
  s_stateMgr = nullptr;
  if (s_osystem) {
    s_osystem->myConsole = nullptr;
  }
  delete s_console;
  s_console = nullptr;
  delete s_settings;
  s_settings = nullptr;
  delete s_osystem;
  s_osystem = nullptr;
  TIATables::freeAllTables();

  memset(s_palette565, 0, sizeof(s_palette565));
  memset(s_framebuffer, 0, sizeof(s_framebuffer));
  memset(s_snapshotFb, 0, sizeof(s_snapshotFb));
  s_romChecksum = 0;
  s_frameCount = 0;
  s_buttonMask = 0;
  s_snapshotFrameCount = 0;
  s_snapshotRomChecksum = 0;
  s_snapshotButtonMask = 0;
  s_snapshotValid = false;
}

}  // namespace

extern "C" {

__attribute__((visibility("default")))
int32_t bmo_atari_load_rom(uint32_t size) {
  releaseCore();
  s_error = 0;
  uint8_t* rom = bmo_emu_rom_buffer();
  const uint32_t capacity = bmo_emu_rom_capacity();
  if (!rom || size > capacity ||
      (size != 2048U && size != 4096U && size != 8192U &&
       size != 16384U && size != 32768U)) {
    s_error = -1;
    return s_error;
  }

  if (!TIATables::allocateAllTables()) {
    releaseCore();
    s_error = -2;
    return s_error;
  }

  s_osystem = new OSystem();
  if (!s_osystem) {
    releaseCore();
    s_error = -2;
    return s_error;
  }

  s_settings = new Settings(s_osystem);
  if (!s_settings) {
    releaseCore();
    s_error = -2;
    return s_error;
  }
  s_osystem->attach(s_settings);
  s_settings->setValue("palette", "standard");
  s_settings->setValue("fastscbios", true);
  s_settings->setValue("rominfo", false);
  s_settings->setValue("tiadriven", false);
  s_settings->setValue("ramrandom", false);
  s_settings->setValue("joyallow4", false);

  string cartMD5 = MD5(rom, size);
  Properties props;
  s_osystem->propSet().getMD5(cartMD5, props);

  string cartType = props.get(Cartridge_Type);
  string cartId;
  Cartridge* cart = Cartridge::create(rom, size, cartMD5, cartType, cartId,
                                      *s_osystem, *s_settings, "");
  if (!cart) {
    releaseCore();
    s_error = -2;
    return s_error;
  }

  s_console = new Console(s_osystem, cart, props);
  if (!s_console) {
    delete cart;
    releaseCore();
    s_error = -2;
    return s_error;
  }
  s_osystem->myConsole = s_console;
  s_console->initializeVideo();
  s_console->initializeAudio();

  s_stateMgr = new StateManager(s_osystem);
  if (!s_stateMgr) {
    releaseCore();
    s_error = -2;
    return s_error;
  }

  refreshPaletteLut();
  s_romChecksum = hashRom(rom, size);
  s_running = true;
  applyButtons();
  updateFramebuffer();
  return 0;
}

__attribute__((visibility("default")))
void bmo_atari_set_buttons(uint32_t buttons) {
  s_buttonMask = buttons & 0xFFU;
  applyButtons();
}

__attribute__((visibility("default")))
uint32_t bmo_atari_run_frame() {
  if (!s_running || !s_console) return s_frameCount;
  applyButtons();
  s_console->tia().update();
  ++s_frameCount;
  updateFramebuffer();
  return s_frameCount;
}

__attribute__((visibility("default")))
uint16_t* bmo_atari_framebuffer() {
  return s_framebuffer;
}

__attribute__((visibility("default")))
uint32_t bmo_atari_frame_count() {
  return s_frameCount;
}

__attribute__((visibility("default")))
uint32_t bmo_atari_rom_checksum() {
  return s_romChecksum;
}

__attribute__((visibility("default")))
uint32_t bmo_atari_button_state() {
  return s_buttonMask;
}

__attribute__((visibility("default")))
uint8_t bmo_atari_joypad_state() {
  if (!s_running || !s_osystem) return 0xFFU;
  const Event& ev = s_osystem->eventHandler().event();
  uint8_t joypad = 0xFFU;
  if (ev.get(Event::JoystickZeroUp))    joypad = static_cast<uint8_t>(joypad & ~0x01U);
  if (ev.get(Event::JoystickZeroDown))  joypad = static_cast<uint8_t>(joypad & ~0x02U);
  if (ev.get(Event::JoystickZeroLeft))  joypad = static_cast<uint8_t>(joypad & ~0x04U);
  if (ev.get(Event::JoystickZeroRight)) joypad = static_cast<uint8_t>(joypad & ~0x08U);
  if (ev.get(Event::JoystickZeroFire))  joypad = static_cast<uint8_t>(joypad & ~0x10U);
  return joypad;
}

__attribute__((visibility("default")))
int32_t bmo_atari_last_error() {
  return s_error;
}

__attribute__((visibility("default")))
int32_t bmo_atari_is_running() {
  return s_running ? 1 : 0;
}

__attribute__((visibility("default")))
void bmo_atari_save_state() {
  if (!s_running || !s_stateMgr) return;
  delete s_snapshot;
  s_snapshot = new Serializer();
  if (!s_snapshot || !s_stateMgr->saveState(*s_snapshot) ||
      s_snapshot->size() == 0 || !s_snapshot->data()) {
    delete s_snapshot;
    s_snapshot = nullptr;
    return;
  }
  memcpy(s_snapshotFb, s_framebuffer, sizeof(s_framebuffer));
  s_snapshotFrameCount = s_frameCount;
  s_snapshotRomChecksum = s_romChecksum;
  s_snapshotButtonMask = s_buttonMask;
  s_snapshotValid = true;
}

__attribute__((visibility("default")))
int32_t bmo_atari_load_state() {
  if (!s_running || !s_stateMgr || !s_snapshot || !s_snapshotValid ||
      s_snapshotRomChecksum != s_romChecksum) {
    return 0;
  }
  s_snapshot->reset();
  if (!s_stateMgr->loadState(*s_snapshot)) {
    return 0;
  }
  refreshPaletteLut();
  memcpy(s_framebuffer, s_snapshotFb, sizeof(s_framebuffer));
  s_frameCount = s_snapshotFrameCount;
  s_buttonMask = s_snapshotButtonMask;
  applyButtons();
  return 1;
}

__attribute__((visibility("default")))
void bmo_atari_destroy() {
  releaseCore();
  s_error = 0;
}

}
