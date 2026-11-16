// SPDX-License-Identifier: MIT OR GPL-2.0-or-later
#pragma GCC optimize("O3,unroll-loops")
#include "emu_walnut.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../hal/buttons.h"
#include "../hal/config.h"
#include "../storage/save_manager.h"
#include "../ui/display_emu.h"
#include "gb_common.h"

#define ENABLE_LCD 1
#define ENABLE_SOUND 1
#define WALNUT_FULL_GBC_SUPPORT 1
#define WALNUT_GB_RGB565_BIGENDIAN 1

namespace WGB {
#include "../third_party/walnut_cgb/walnut_cgb.h"
}
using namespace WGB;

namespace {

struct gb_s* gb = nullptr;
const uint8_t* current_rom_data = nullptr;
size_t current_rom_len = 0;
uint8_t* cart_ram = nullptr;
uint16_t* s_asyncGbcFrame = nullptr;

// Indexed by (pal_type << 2) | color_idx: 0=OBJ0, 1=OBJ1, 2=BG (ST7789 wire BGR565).
const uint16_t DMG_ON_GBC_PAL[16] = {
    0xFFFF, 0x1F00, 0x1000, 0x0000, 0xFFFF, 0xE007, 0xE003, 0x0000,
    0xFFFF, 0x8CF5, 0x0080, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
};

uint16_t DMG_ON_GBC_PAL_256[256];

size_t activeBatteryRamSize() {
  if (!gb || !gb->gb_rom_read) return 0;
  return GbCommon::batteryRamSize(gb->gb_rom_read(gb, 0x0147), gb->gb_rom_read(gb, 0x0149));
}

IRAM_ATTR uint8_t gb_rom_read(struct gb_s* gbCtx, const uint_fast32_t addr) {
  (void)gbCtx;
  if (addr >= current_rom_len) return 0xFF;
  return current_rom_data[addr];
}

// BMO-PATCH: Byte-wise little-endian reconstruction prevents unaligned Xtensa access.
IRAM_ATTR uint16_t gb_rom_read16(struct gb_s* gbCtx, const uint_fast32_t addr) {
  (void)gbCtx;
  if (addr + 2 > current_rom_len) return 0xFFFF;
  return static_cast<uint16_t>(current_rom_data[addr]) | (static_cast<uint16_t>(current_rom_data[addr + 1]) << 8);
}

// BMO-PATCH: Byte-wise little-endian reconstruction prevents unaligned Xtensa access.
IRAM_ATTR uint32_t gb_rom_read32(struct gb_s* gbCtx, const uint_fast32_t addr) {
  (void)gbCtx;
  if (addr + 4 > current_rom_len) return 0xFFFFFFFF;
  return static_cast<uint32_t>(current_rom_data[addr]) | (static_cast<uint32_t>(current_rom_data[addr + 1]) << 8) |
         (static_cast<uint32_t>(current_rom_data[addr + 2]) << 16) |
         (static_cast<uint32_t>(current_rom_data[addr + 3]) << 24);
}

IRAM_ATTR uint8_t gb_cart_ram_read(struct gb_s* gbCtx, const uint_fast32_t addr) {
  (void)gbCtx;
  if (!cart_ram || addr >= GbCommon::kCartRamSize) return 0xFF;
  return cart_ram[addr];
}

IRAM_ATTR void gb_cart_ram_write(struct gb_s* gbCtx, const uint_fast32_t addr, const uint8_t val) {
  (void)gbCtx;
  if (!cart_ram || addr >= GbCommon::kCartRamSize) return;
  cart_ram[addr] = val;
}

void gb_error(struct gb_s* gbCtx, const enum gb_error_e gb_err, const uint16_t val) {
  (void)gbCtx;
  GbCommon::reportError("Walnut-CGB", static_cast<int>(gb_err), val);
}

IRAM_ATTR void lcd_draw_line(struct gb_s* gbCtx, const uint8_t* pixels, const uint_fast8_t line) {
  if (gbCtx->cgb.cgbMode) {
    GbCommon::scaleAndBlitGbLine<0x3FU>(pixels, gbCtx->cgb.fixPalette, s_asyncGbcFrame, line);
  } else {
    GbCommon::scaleAndBlitGbLine<0xFFU>(pixels, DMG_ON_GBC_PAL_256, s_asyncGbcFrame, line);
  }
}

} // namespace

bool WalnutEmu::init(const uint8_t* rom_data, size_t rom_len) {
  destroy();
  current_rom_data = rom_data;
  current_rom_len = rom_len;

  for (int i = 0; i < 256; ++i) {
    DMG_ON_GBC_PAL_256[i] = DMG_ON_GBC_PAL[(((i >> 4) & 0x03) << 2) | (i & 0x03)];
  }

  gb = static_cast<struct gb_s*>(GbCommon::allocGbStateAligned(sizeof(struct gb_s), "Walnut-CGB"));
  if (!gb) return false;

  cart_ram = GbCommon::allocCartRam("Walnut-CGB", MALLOC_CAP_SPIRAM);
  if (!cart_ram) {
    destroy();
    return false;
  }

  const enum gb_init_error_e ret =
      gb_init(gb, gb_rom_read, gb_rom_read16, gb_rom_read32, gb_cart_ram_read, gb_cart_ram_write, gb_error, nullptr);
  if (ret != GB_INIT_NO_ERROR) {
    LOG_ERROR("Walnut-CGB init error: %d", ret);
    destroy();
    return false;
  }

  gb_init_lcd(gb, lcd_draw_line);
  gb->direct.joypad = 0xFF;
  return true;
}

void WalnutEmu::update() {
  if (!gb) return;
  gb->direct.joypad = Buttons::gb_joypad_state;
  s_asyncGbcFrame = DisplayEmu::acquireAsyncFrameBuffer();
  if (!s_asyncGbcFrame) return;
  gb_run_frame_dualfetch(gb);
  DisplayEmu::submitGBFrameAsync(s_asyncGbcFrame);
  s_asyncGbcFrame = nullptr;
}

void WalnutEmu::destroy() {
  GbCommon::clearError();
  s_asyncGbcFrame = nullptr;
  void* gbPtr = gb;
  GbCommon::freeGbRuntime(&gbPtr, &cart_ram, &current_rom_data, &current_rom_len);
  gb = nullptr;
}

bool WalnutEmu::saveBatteryRam(const char* romFilename) {
  if (!gb || !cart_ram || !romFilename) return false;
  const size_t size = activeBatteryRamSize();
  return (size > 0) ? SaveManager::saveBatteryRam(romFilename, cart_ram, size) : false;
}

bool WalnutEmu::loadBatteryRam(const char* romFilename) {
  if (!gb || !cart_ram || !romFilename) return false;
  const size_t size = activeBatteryRamSize();
  return (size > 0) ? SaveManager::loadBatteryRam(romFilename, cart_ram, size) : false;
}

bool WalnutEmu::saveState(const char* romFilename, int slot) {
  if (!gb || !cart_ram || !romFilename) return false;
  return SaveManager::saveState(romFilename, slot, SaveManager::CORE_GB_CGB, gb, sizeof(struct gb_s), cart_ram,
                                GbCommon::kCartRamSize);
}

bool WalnutEmu::loadState(const char* romFilename, int slot) {
  if (!gb || !cart_ram || !romFilename) return false;
  struct gb_s* tempGb = static_cast<struct gb_s*>(heap_caps_malloc(sizeof(struct gb_s), MALLOC_CAP_SPIRAM));
  uint8_t* tempRam = static_cast<uint8_t*>(heap_caps_malloc(GbCommon::kCartRamSize, MALLOC_CAP_SPIRAM));
  if (!tempGb || !tempRam) {
    if (tempGb) heap_caps_free(tempGb);
    if (tempRam) heap_caps_free(tempRam);
    return false;
  }
  size_t gbSize = 0, ramSize = 0;
  const bool ok = SaveManager::loadState(romFilename, slot, SaveManager::CORE_GB_CGB, tempGb, sizeof(struct gb_s),
                                         &gbSize, tempRam, GbCommon::kCartRamSize, &ramSize);
  if (ok && gbSize == sizeof(struct gb_s) && ramSize == GbCommon::kCartRamSize) {
    memcpy(cart_ram, tempRam, GbCommon::kCartRamSize);
    tempGb->gb_rom_read = gb_rom_read;
    tempGb->gb_rom_read_16bit = gb_rom_read16;
    tempGb->gb_rom_read_32bit = gb_rom_read32;
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

static_assert(ENABLE_SOUND == 1, "Walnut-CGB expects sound registers active before audio stub mixing.");
static_assert(WALNUT_FULL_GBC_SUPPORT == 1, "Walnut-CGB must keep full GBC banking/palette support enabled.");
static_assert(WALNUT_GB_RGB565_BIGENDIAN == 1, "Walnut-CGB color LUT must emit byte-swapped ST7789 wire BGR565.");
static_assert(sizeof(struct gb_s) < 262144U,
              "struct gb_s size sanity bound failed; update Walnut state buffer limits.");
