#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define ENABLE_LCD 1
#define ENABLE_SOUND 0
#define WALNUT_FULL_GBC_SUPPORT 1
#define WALNUT_GB_RGB565_BIGENDIAN 0

namespace WGB {
#include "../firmware/BmoGameboy/src/third_party/walnut_cgb/walnut_cgb.h"
}

extern "C" uint8_t* bmo_emu_rom_buffer();
extern "C" uint32_t bmo_emu_rom_capacity();

namespace {

constexpr uint32_t SCREEN_WIDTH = 240;
constexpr uint32_t SCREEN_HEIGHT = 216;
constexpr uint32_t SOURCE_WIDTH = 160;
constexpr uint32_t SOURCE_HEIGHT = 144;
constexpr size_t CART_RAM_SIZE = 128U * 1024U;

struct GbcSnapshot {
  WGB::gb_s gameboy;
  uint8_t cartRam[CART_RAM_SIZE];
  uint16_t framebuffer[SCREEN_WIDTH * SCREEN_HEIGHT];
  uint16_t palette[4];
  uint32_t frameCount;
  uint32_t romChecksum;
  uint32_t buttonMask;
  bool valid;
};

WGB::gb_s* s_gameboy = nullptr;
uint8_t* s_cartRam = nullptr;
GbcSnapshot* s_snapshot = nullptr;
uint16_t s_framebuffer[SCREEN_WIDTH * SCREEN_HEIGHT];
uint16_t s_palette[4] = {0xFFFF, 0xAD55, 0x52AA, 0x0000};
uint32_t s_romSize = 0;
uint32_t s_romChecksum = 0;
uint32_t s_frameCount = 0;
uint32_t s_buttonMask = 0;
int32_t s_error = 0;
bool s_running = false;

uint8_t romRead(WGB::gb_s*, const uint_fast32_t address) {
  const uint8_t* rom = bmo_emu_rom_buffer();
  return (rom && address < s_romSize) ? rom[address] : 0xFF;
}

uint16_t romRead16(WGB::gb_s*, const uint_fast32_t address) {
  const uint8_t* rom = bmo_emu_rom_buffer();
  if (!rom || address + 2U > s_romSize) return 0xFFFF;
  return static_cast<uint16_t>(rom[address]) | (static_cast<uint16_t>(rom[address + 1U]) << 8);
}

uint32_t romRead32(WGB::gb_s*, const uint_fast32_t address) {
  const uint8_t* rom = bmo_emu_rom_buffer();
  if (!rom || address + 4U > s_romSize) return 0xFFFFFFFF;
  return static_cast<uint32_t>(rom[address]) | (static_cast<uint32_t>(rom[address + 1U]) << 8) |
         (static_cast<uint32_t>(rom[address + 2U]) << 16) | (static_cast<uint32_t>(rom[address + 3U]) << 24);
}

uint8_t cartRamRead(WGB::gb_s*, const uint_fast32_t address) {
  return (s_cartRam && address < CART_RAM_SIZE) ? s_cartRam[address] : 0xFF;
}

void cartRamWrite(WGB::gb_s*, const uint_fast32_t address, const uint8_t value) {
  if (s_cartRam && address < CART_RAM_SIZE) s_cartRam[address] = value;
}

void gameboyError(WGB::gb_s*, const WGB::gb_error_e error, const uint16_t) {
  s_error = static_cast<int32_t>(error) + 1;
}

void drawLine(WGB::gb_s* gbCtx, const uint8_t* pixels, const uint_fast8_t line) {
  if (!gbCtx || !pixels || line >= SOURCE_HEIGHT) return;
  const uint32_t destY = line + (line + 1U) / 2U;
  uint16_t* row = &s_framebuffer[destY * SCREEN_WIDTH];
  const bool cgbMode = gbCtx->cgb.cgbMode != 0;
  for (uint32_t x = 0; x < SOURCE_WIDTH; ++x) {
    const uint32_t destX = x + (x + 1U) / 2U;
    const uint16_t pxVal = cgbMode ? gbCtx->cgb.fixPalette[pixels[x] & 0x3FU] : s_palette[pixels[x] & 0x03U];
    row[destX] = pxVal;
    if ((x & 1U) == 0) row[destX + 1U] = pxVal;
  }
  if ((line & 1U) == 0) {
    memcpy(row + SCREEN_WIDTH, row, SCREEN_WIDTH * sizeof(uint16_t));
  }
}

uint32_t hashRom(const uint8_t* rom, uint32_t size) {
  uint32_t hash = 2166136261U;
  for (uint32_t i = 0; i < size; ++i) {
    hash = (hash ^ rom[i]) * 16777619U;
  }
  return hash;
}

void releaseGbc() {
  s_running = false;
  free(s_snapshot);
  s_snapshot = nullptr;
  free(s_cartRam);
  s_cartRam = nullptr;
  free(s_gameboy);
  s_gameboy = nullptr;
  s_romSize = 0;
  s_romChecksum = 0;
  s_frameCount = 0;
  s_buttonMask = 0;
  memset(s_framebuffer, 0, sizeof(s_framebuffer));
}

}  // namespace

extern "C" {

__attribute__((visibility("default")))
int32_t bmo_gbc_load_rom(uint32_t size) {
  releaseGbc();
  s_error = 0;
  const uint8_t* rom = bmo_emu_rom_buffer();
  const uint32_t capacity = bmo_emu_rom_capacity();
  if (!rom || size < 0x150U || size > capacity) return -1;
  if (rom[0x148] > 8U || rom[0x149] > 5U) return -2;
  const uint32_t declaredSize = (2U << rom[0x148]) * 0x4000U;
  if (size < declaredSize) return -2;

  s_gameboy = static_cast<WGB::gb_s*>(calloc(1, sizeof(WGB::gb_s)));
  s_cartRam = static_cast<uint8_t*>(calloc(1, CART_RAM_SIZE));
  s_snapshot = static_cast<GbcSnapshot*>(calloc(1, sizeof(GbcSnapshot)));
  if (!s_gameboy || !s_cartRam || !s_snapshot) {
    releaseGbc();
    return -4;
  }

  s_romSize = size;
  const WGB::gb_init_error_e result = WGB::gb_init(
      s_gameboy, &romRead, &romRead16, &romRead32, &cartRamRead, &cartRamWrite, &gameboyError, nullptr);
  if (result != WGB::GB_INIT_NO_ERROR) {
    releaseGbc();
    return result == WGB::GB_INIT_INVALID_CHECKSUM ? -3 : -4;
  }

  WGB::gb_init_lcd(s_gameboy, &drawLine);
  s_gameboy->direct.joypad = 0xFF;
  s_romChecksum = hashRom(rom, size);
  s_running = true;
  return 0;
}

__attribute__((visibility("default")))
void bmo_gbc_set_buttons(uint32_t buttons) {
  s_buttonMask = buttons & 0xFFU;
  if (!s_running || !s_gameboy) return;

  uint8_t pressed = 0;
  if (buttons & 0x01U) pressed |= JOYPAD_UP;
  if (buttons & 0x02U) pressed |= JOYPAD_DOWN;
  if (buttons & 0x04U) pressed |= JOYPAD_LEFT;
  if (buttons & 0x08U) pressed |= JOYPAD_RIGHT;
  if (buttons & 0x10U) pressed |= JOYPAD_A;
  if (buttons & 0x20U) pressed |= JOYPAD_B;
  if (buttons & 0x40U) pressed |= JOYPAD_START;
  if (buttons & 0x80U) pressed |= JOYPAD_SELECT;
  s_gameboy->direct.joypad = static_cast<uint8_t>(~pressed);
}

__attribute__((visibility("default")))
uint32_t bmo_gbc_run_frame() {
  if (!s_running || !s_gameboy) return s_frameCount;
  WGB::gb_run_frame_dualfetch(s_gameboy);
  ++s_frameCount;
  return s_frameCount;
}

__attribute__((visibility("default")))
uint16_t* bmo_gbc_framebuffer() {
  return s_framebuffer;
}

__attribute__((visibility("default")))
uint32_t bmo_gbc_frame_count() {
  return s_frameCount;
}

__attribute__((visibility("default")))
uint32_t bmo_gbc_rom_checksum() {
  return s_romChecksum;
}

__attribute__((visibility("default")))
uint32_t bmo_gbc_button_state() {
  return s_buttonMask;
}

__attribute__((visibility("default")))
uint8_t bmo_gbc_joypad_state() {
  return s_gameboy ? s_gameboy->direct.joypad : 0xFF;
}

__attribute__((visibility("default")))
int32_t bmo_gbc_last_error() {
  return s_error;
}

__attribute__((visibility("default")))
int32_t bmo_gbc_is_running() {
  return s_running ? 1 : 0;
}

__attribute__((visibility("default")))
void bmo_gbc_set_palette(uint32_t color0, uint32_t color1, uint32_t color2, uint32_t color3) {
  const uint32_t colors[4] = {color0, color1, color2, color3};
  for (uint32_t i = 0; i < 4; ++i) {
    const uint32_t color = colors[i];
    s_palette[i] = static_cast<uint16_t>(
        (((color >> 19) & 0x1FU) << 11) |
        (((color >> 10) & 0x3FU) << 5) |
        ((color >> 3) & 0x1FU));
  }
}

__attribute__((visibility("default")))
void bmo_gbc_save_state() {
  if (!s_running || !s_gameboy || !s_cartRam || !s_snapshot) return;
  memcpy(&s_snapshot->gameboy, s_gameboy, sizeof(WGB::gb_s));
  memcpy(s_snapshot->cartRam, s_cartRam, CART_RAM_SIZE);
  memcpy(s_snapshot->framebuffer, s_framebuffer, sizeof(s_framebuffer));
  memcpy(s_snapshot->palette, s_palette, sizeof(s_palette));
  s_snapshot->frameCount = s_frameCount;
  s_snapshot->romChecksum = s_romChecksum;
  s_snapshot->buttonMask = s_buttonMask;
  s_snapshot->valid = true;
}

__attribute__((visibility("default")))
int32_t bmo_gbc_load_state() {
  if (!s_running || !s_gameboy || !s_cartRam || !s_snapshot || !s_snapshot->valid ||
      s_snapshot->romChecksum != s_romChecksum) {
    return 0;
  }
  memcpy(s_gameboy, &s_snapshot->gameboy, sizeof(WGB::gb_s));
  s_gameboy->gb_rom_read = &romRead;
  s_gameboy->gb_rom_read_16bit = &romRead16;
  s_gameboy->gb_rom_read_32bit = &romRead32;
  s_gameboy->gb_cart_ram_read = &cartRamRead;
  s_gameboy->gb_cart_ram_write = &cartRamWrite;
  s_gameboy->gb_error = &gameboyError;
  s_gameboy->display.lcd_draw_line = &drawLine;
  memcpy(s_cartRam, s_snapshot->cartRam, CART_RAM_SIZE);
  memcpy(s_framebuffer, s_snapshot->framebuffer, sizeof(s_framebuffer));
  memcpy(s_palette, s_snapshot->palette, sizeof(s_palette));
  s_frameCount = s_snapshot->frameCount;
  s_buttonMask = s_snapshot->buttonMask;
  return 1;
}

__attribute__((visibility("default")))
void bmo_gbc_destroy() {
  releaseGbc();
  s_error = 0;
}

}
