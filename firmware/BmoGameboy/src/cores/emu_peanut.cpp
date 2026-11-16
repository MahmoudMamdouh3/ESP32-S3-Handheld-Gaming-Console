// SPDX-License-Identifier: MIT OR GPL-2.0-or-later
#pragma GCC optimize("O3,unroll-loops")
#include "emu_peanut.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../hal/buttons.h"
#include "../hal/config.h"
#include "../storage/save_manager.h"
#include "../ui/bmo_face.h"
#include "../ui/display_emu.h"
#include "gb_common.h"

#include "../third_party/peanut_gb/peanut_gb_config.h"

namespace PGB {
#include "../third_party/peanut_gb/peanut_gb.h"
}
using namespace PGB;

namespace {

struct gb_s* gb = nullptr;
const uint8_t* current_rom_data = nullptr;
size_t current_rom_len = 0;
uint8_t* cart_ram = nullptr;
uint16_t* s_asyncGbFrame = nullptr;
uint16_t PAL_256[256];

size_t activeBatteryRamSize() {
  if (!gb || !gb->gb_rom_read) return 0;
  return GbCommon::batteryRamSize(gb->gb_rom_read(gb, 0x0147), gb->gb_rom_read(gb, 0x0149));
}

IRAM_ATTR uint8_t gb_rom_read(struct gb_s* gbCtx, const uint_fast32_t addr) {
  (void)gbCtx;
  if (addr >= current_rom_len) return 0xFF;
  return current_rom_data[addr];
}

IRAM_ATTR uint8_t gb_cart_ram_read(struct gb_s* gbCtx, const uint_fast32_t addr) {
  (void)gbCtx;
  if (!cart_ram || addr >= GbCommon::kCartRamSize) {
    static bool warned = false;
    if (!warned) {
      LOG_WARN_STR("WARNING: Cart RAM read overflow!");
      warned = true;
    }
    return 0xFF;
  }
  return cart_ram[addr];
}

IRAM_ATTR void gb_cart_ram_write(struct gb_s* gbCtx, const uint_fast32_t addr, const uint8_t val) {
  (void)gbCtx;
  if (!cart_ram || addr >= GbCommon::kCartRamSize) {
    static bool warned = false;
    if (!warned) {
      LOG_WARN_STR("WARNING: Cart RAM write overflow!");
      warned = true;
    }
    return;
  }
  cart_ram[addr] = val;
}

void gb_error(struct gb_s* gbCtx, const enum gb_error_e gb_err, const uint16_t val) {
  (void)gbCtx;
  LOG_ERROR("Peanut-GB Error: %d at PC 0x%04X", gb_err, val);
  Serial.flush();
  BmoFace::setExpression(BmoFace::ERROR);
  BmoFace::draw();
  esp_restart();
}

IRAM_ATTR void lcd_draw_line(struct gb_s* gbCtx, const uint8_t pixels[160], const uint_fast8_t line) {
  (void)gbCtx;
  GbCommon::scaleAndBlitGbLine<0xFFU>(pixels, PAL_256, s_asyncGbFrame, line);
}

} // namespace

bool PeanutEmu::init(const uint8_t* rom_data, size_t rom_len) {
  destroy();
  current_rom_data = rom_data;
  current_rom_len = rom_len;

  for (int i = 0; i < 256; ++i) {
    PAL_256[i] = DisplayEmu::CLASSIC_PALETTE[i & 0x03];
  }

  gb = static_cast<struct gb_s*>(GbCommon::allocGbStateAligned(sizeof(struct gb_s), "Peanut-GB"));
  if (!gb) return false;

  cart_ram = GbCommon::allocCartRam("Peanut-GB", MALLOC_CAP_SPIRAM);
  if (!cart_ram) {
    destroy();
    return false;
  }

  LOG_INFO_STR("Initializing Peanut-GB...");
  LOG_INFO("ROM loaded: %u bytes", rom_len);

  const enum gb_init_error_e ret = gb_init(gb, &gb_rom_read, &gb_cart_ram_read, &gb_cart_ram_write, &gb_error, nullptr);
  if (ret != GB_INIT_NO_ERROR) {
    LOG_ERROR("gb_init() failed: %d", ret);
    destroy();
    return false;
  }

  gb_init_lcd(gb, &lcd_draw_line);
  gb->direct.joypad = 0xFF;
  LOG_INFO_STR("Peanut-GB initialized.");
  return true;
}

void PeanutEmu::update() {
  if (!gb) return;
  gb->direct.joypad = Buttons::gb_joypad_state;
  static int lastPaletteIdx = -1;
  const int curPaletteIdx = DisplayEmu::getDmgPaletteIndex();
  if (curPaletteIdx != lastPaletteIdx) {
    const uint16_t* pal = DisplayEmu::getActiveDmgPalette();
    for (int i = 0; i < 256; ++i) {
      PAL_256[i] = pal[i & 0x03];
    }
    lastPaletteIdx = curPaletteIdx;
  }

  s_asyncGbFrame = DisplayEmu::acquireAsyncFrameBuffer();
  if (!s_asyncGbFrame) return;
  gb_run_frame(gb);
  DisplayEmu::submitGBFrameAsync(s_asyncGbFrame);
  s_asyncGbFrame = nullptr;
}

void PeanutEmu::destroy() {
  s_asyncGbFrame = nullptr;
  void* gbPtr = gb;
  GbCommon::freeGbRuntime(&gbPtr, &cart_ram, &current_rom_data, &current_rom_len);
  gb = nullptr;
}

bool PeanutEmu::saveBatteryRam(const char* romFilename) {
  if (!gb || !cart_ram || !romFilename) return false;
  const size_t size = activeBatteryRamSize();
  return (size > 0) ? SaveManager::saveBatteryRam(romFilename, cart_ram, size) : false;
}

bool PeanutEmu::loadBatteryRam(const char* romFilename) {
  if (!gb || !cart_ram || !romFilename) return false;
  const size_t size = activeBatteryRamSize();
  return (size > 0) ? SaveManager::loadBatteryRam(romFilename, cart_ram, size) : false;
}

bool PeanutEmu::saveState(const char* romFilename, int slot) {
  if (!gb || !cart_ram || !romFilename) return false;
  return SaveManager::saveState(romFilename, slot, SaveManager::CORE_GB_DMG, gb, sizeof(struct gb_s), cart_ram,
                                GbCommon::kCartRamSize);
}

bool PeanutEmu::loadState(const char* romFilename, int slot) {
  if (!gb || !cart_ram || !romFilename) return false;
  struct gb_s* tempGb = static_cast<struct gb_s*>(heap_caps_malloc(sizeof(struct gb_s), MALLOC_CAP_SPIRAM));
  uint8_t* tempRam = static_cast<uint8_t*>(heap_caps_malloc(GbCommon::kCartRamSize, MALLOC_CAP_SPIRAM));
  if (!tempGb || !tempRam) {
    if (tempGb) heap_caps_free(tempGb);
    if (tempRam) heap_caps_free(tempRam);
    return false;
  }
  size_t gbSize = 0, ramSize = 0;
  const bool ok = SaveManager::loadState(romFilename, slot, SaveManager::CORE_GB_DMG, tempGb, sizeof(struct gb_s),
                                         &gbSize, tempRam, GbCommon::kCartRamSize, &ramSize);
  if (ok && gbSize == sizeof(struct gb_s) && ramSize == GbCommon::kCartRamSize) {
    memcpy(cart_ram, tempRam, GbCommon::kCartRamSize);
    tempGb->gb_rom_read = gb_rom_read;
    tempGb->gb_cart_ram_read = gb_cart_ram_read;
    tempGb->gb_cart_ram_write = gb_cart_ram_write;
    tempGb->gb_error = gb_error;
    tempGb->display.lcd_draw_line = lcd_draw_line;
    memcpy(gb, tempGb, sizeof(struct gb_s));
    heap_caps_free(tempRam);
    heap_caps_free(tempGb);
    return true;
  }
  heap_caps_free(tempRam);
  heap_caps_free(tempGb);
  return false;
}
