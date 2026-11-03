#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../../firmware/BmoGameboy/src/vendor/peanut_gb/peanut_gb_config.h"

extern "C" uint8_t audio_read(uint16_t) {
  return 0;
}

extern "C" void audio_write(uint16_t, uint8_t) {}

namespace PGB {
#include "../../firmware/BmoGameboy/src/vendor/peanut_gb/peanut_gb.h"
}

namespace {

constexpr uint32_t MAX_ROM_SIZE = 8U * 1024U * 1024U;
constexpr uint32_t SCREEN_WIDTH = 240;
constexpr uint32_t SCREEN_HEIGHT = 216;
constexpr uint32_t SOURCE_WIDTH = 160;
constexpr uint32_t SOURCE_HEIGHT = 144;
constexpr size_t CART_RAM_SIZE = 128U * 1024U;

struct Snapshot {
  PGB::gb_s gameboy;
  uint8_t cartRam[CART_RAM_SIZE];
  uint16_t framebuffer[SCREEN_WIDTH * SCREEN_HEIGHT];
  uint16_t palette[4];
  uint32_t frameCount;
  uint32_t romChecksum;
  bool valid;
};

PGB::gb_s s_gameboy;
uint8_t s_rom[MAX_ROM_SIZE];
uint8_t s_cartRam[CART_RAM_SIZE];
uint16_t s_framebuffer[SCREEN_WIDTH * SCREEN_HEIGHT];
uint16_t s_palette[4] = {0xFFFF, 0xAD55, 0x52AA, 0x0000};
Snapshot s_snapshot;
uint32_t s_romSize = 0;
uint32_t s_romChecksum = 0;
uint32_t s_frameCount = 0;
uint32_t s_buttonMask = 0;
int32_t s_error = 0;
bool s_running = false;

uint8_t romRead(PGB::gb_s*, const uint_fast32_t address) {
  return address < s_romSize ? s_rom[address] : 0xFF;
}

uint8_t cartRamRead(PGB::gb_s*, const uint_fast32_t address) {
  return address < CART_RAM_SIZE ? s_cartRam[address] : 0xFF;
}

void cartRamWrite(PGB::gb_s*, const uint_fast32_t address, const uint8_t value) {
  if (address < CART_RAM_SIZE) s_cartRam[address] = value;
}

void gameboyError(PGB::gb_s*, const PGB::gb_error_e error, const uint16_t) {
  s_error = static_cast<int32_t>(error) + 1;
}

void drawLine(PGB::gb_s*, const uint8_t pixels[SOURCE_WIDTH], const uint_fast8_t line) {
  if (line >= SOURCE_HEIGHT) return;

  const uint32_t destY = line + (line + 1U) / 2U;
  uint16_t* row = &s_framebuffer[destY * SCREEN_WIDTH];
  for (uint32_t x = 0; x < SOURCE_WIDTH; ++x) {
    const uint32_t destX = x + (x + 1U) / 2U;
    row[destX] = s_palette[pixels[x] & 0x03U];
    if ((x & 1U) == 0) row[destX + 1U] = row[destX];
  }
  if ((line & 1U) == 0) {
    memcpy(row + SCREEN_WIDTH, row, SCREEN_WIDTH * sizeof(uint16_t));
  }
}

uint32_t hashRom(uint32_t size) {
  uint32_t hash = 2166136261U;
  for (uint32_t i = 0; i < size; ++i) {
    hash = (hash ^ s_rom[i]) * 16777619U;
  }
  return hash;
}

}  // namespace

extern "C" {

__attribute__((visibility("default")))
uint8_t* bmo_emu_rom_buffer() {
  return s_rom;
}

__attribute__((visibility("default")))
uint32_t bmo_emu_rom_capacity() {
  return MAX_ROM_SIZE;
}

__attribute__((visibility("default")))
int32_t bmo_emu_load_rom(uint32_t size) {
  s_running = false;
  s_error = 0;
  s_frameCount = 0;
  s_romSize = 0;
  s_romChecksum = 0;
  s_snapshot.valid = false;
  memset(s_framebuffer, 0, sizeof(s_framebuffer));

  if (size < 0x150U || size > MAX_ROM_SIZE) return -1;
  if (s_rom[0x148] > 8U || s_rom[0x149] > 5U) return -2;
  const uint32_t declaredSize = (2U << s_rom[0x148]) * 0x4000U;
  if (size < declaredSize) return -2;

  s_romSize = size;
  const PGB::gb_init_error_e result = PGB::gb_init(
      &s_gameboy, &romRead, &cartRamRead, &cartRamWrite, &gameboyError, nullptr);
  if (result != PGB::GB_INIT_NO_ERROR) {
    s_romSize = 0;
    return result == PGB::GB_INIT_INVALID_CHECKSUM ? -3 : -4;
  }

  PGB::gb_init_lcd(&s_gameboy, &drawLine);
  memset(s_cartRam, 0, sizeof(s_cartRam));
  s_buttonMask = 0;
  s_gameboy.direct.joypad = 0xFF;
  s_romChecksum = hashRom(size);
  s_running = true;
  return 0;
}

__attribute__((visibility("default")))
void bmo_emu_set_buttons(uint32_t buttons) {
  s_buttonMask = buttons & 0xFFU;
  if (!s_running) return;

  uint8_t pressed = 0;
  if (buttons & 0x01U) pressed |= JOYPAD_UP;
  if (buttons & 0x02U) pressed |= JOYPAD_DOWN;
  if (buttons & 0x04U) pressed |= JOYPAD_LEFT;
  if (buttons & 0x08U) pressed |= JOYPAD_RIGHT;
  if (buttons & 0x10U) pressed |= JOYPAD_A;
  if (buttons & 0x20U) pressed |= JOYPAD_B;
  if (buttons & 0x40U) pressed |= JOYPAD_START;
  if (buttons & 0x80U) pressed |= JOYPAD_SELECT;
  s_gameboy.direct.joypad = static_cast<uint8_t>(~pressed);
}

__attribute__((visibility("default")))
uint32_t bmo_emu_run_frame() {
  if (!s_running) return s_frameCount;
  PGB::gb_run_frame(&s_gameboy);
  ++s_frameCount;
  return s_frameCount;
}

__attribute__((visibility("default")))
uint16_t* bmo_emu_framebuffer() {
  return s_framebuffer;
}

__attribute__((visibility("default")))
uint32_t bmo_emu_frame_count() {
  return s_frameCount;
}

__attribute__((visibility("default")))
uint32_t bmo_emu_rom_checksum() {
  return s_romChecksum;
}

__attribute__((visibility("default")))
uint32_t bmo_emu_button_state() {
  return s_buttonMask;
}

__attribute__((visibility("default")))
uint8_t bmo_emu_joypad_state() {
  return s_gameboy.direct.joypad;
}

__attribute__((visibility("default")))
int32_t bmo_emu_last_error() {
  return s_error;
}

__attribute__((visibility("default")))
int32_t bmo_emu_is_running() {
  return s_running ? 1 : 0;
}

__attribute__((visibility("default")))
void bmo_emu_set_palette(uint32_t color0, uint32_t color1, uint32_t color2, uint32_t color3) {
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
void bmo_emu_save_state() {
  if (!s_running) return;
  memcpy(&s_snapshot.gameboy, &s_gameboy, sizeof(s_gameboy));
  memcpy(s_snapshot.cartRam, s_cartRam, sizeof(s_cartRam));
  memcpy(s_snapshot.framebuffer, s_framebuffer, sizeof(s_framebuffer));
  memcpy(s_snapshot.palette, s_palette, sizeof(s_palette));
  s_snapshot.frameCount = s_frameCount;
  s_snapshot.romChecksum = s_romChecksum;
  s_snapshot.valid = true;
}

__attribute__((visibility("default")))
int32_t bmo_emu_load_state() {
  if (!s_running || !s_snapshot.valid || s_snapshot.romChecksum != s_romChecksum) return 0;
  memcpy(&s_gameboy, &s_snapshot.gameboy, sizeof(s_gameboy));
  memcpy(s_cartRam, s_snapshot.cartRam, sizeof(s_cartRam));
  memcpy(s_framebuffer, s_snapshot.framebuffer, sizeof(s_framebuffer));
  memcpy(s_palette, s_snapshot.palette, sizeof(s_palette));
  s_frameCount = s_snapshot.frameCount;
  return 1;
}

__attribute__((visibility("default")))
void bmo_emu_destroy() {
  s_running = false;
  s_romSize = 0;
  s_romChecksum = 0;
  s_frameCount = 0;
  s_buttonMask = 0;
  s_error = 0;
  s_snapshot.valid = false;
  memset(s_framebuffer, 0, sizeof(s_framebuffer));
}

}
