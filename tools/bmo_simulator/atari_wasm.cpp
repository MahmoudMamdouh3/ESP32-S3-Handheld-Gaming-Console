#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../../firmware/BmoGameboy/src/vendor/stella/atari.h"

extern "C" uint8_t* bmo_emu_rom_buffer();
extern "C" uint32_t bmo_emu_rom_capacity();

namespace {

constexpr uint32_t SCREEN_WIDTH = 160;
constexpr uint32_t SCREEN_HEIGHT = 192;
constexpr size_t FRAMEBUFFER_SIZE = SCREEN_WIDTH * SCREEN_HEIGHT;

atari_context_t* s_atari = nullptr;
uint16_t s_framebuffer[FRAMEBUFFER_SIZE];
atari_context_t* s_snapshot = nullptr;
uint32_t s_romChecksum = 0;
uint32_t s_frameCount = 0;
uint32_t s_buttonMask = 0;
uint32_t s_snapshotFrameCount = 0;
uint32_t s_snapshotRomChecksum = 0;
uint32_t s_snapshotButtonMask = 0;
int32_t s_error = 0;
bool s_running = false;
bool s_snapshotValid = false;

uint16_t browserRgb565(uint16_t panelPixel) {
  const uint16_t bgr565 = static_cast<uint16_t>(
      (panelPixel << 8) | (panelPixel >> 8));
  return static_cast<uint16_t>(
      ((bgr565 & 0x001FU) << 11) |
      (bgr565 & 0x07E0U) |
      ((bgr565 & 0xF800U) >> 11));
}

void updateFramebuffer() {
  if (!s_atari || !s_atari->framebuffer) return;
  for (size_t i = 0; i < FRAMEBUFFER_SIZE; ++i) {
    s_framebuffer[i] = browserRgb565(s_atari->framebuffer[i]);
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
  if (!s_atari) return;
  uint8_t joypad = 0xFFU;
  for (uint32_t bit = 0; bit < 5; ++bit) {
    if (s_buttonMask & (1U << bit)) {
      joypad = static_cast<uint8_t>(joypad & ~(1U << bit));
    }
  }
  atari_set_input(s_atari, joypad);
}

void releaseCore() {
  s_running = false;
  if (s_atari) {
    atari_destroy(s_atari);
    s_atari = nullptr;
  }
  free(s_snapshot);
  s_snapshot = nullptr;
  s_romChecksum = 0;
  s_frameCount = 0;
  s_buttonMask = 0;
  s_snapshotFrameCount = 0;
  s_snapshotRomChecksum = 0;
  s_snapshotButtonMask = 0;
  s_snapshotValid = false;
  memset(s_framebuffer, 0, sizeof(s_framebuffer));
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

  s_atari = atari_create();
  if (!s_atari || !atari_load_rom(s_atari, rom, size)) {
    releaseCore();
    s_error = -2;
    return s_error;
  }
  s_snapshot = static_cast<atari_context_t*>(malloc(sizeof(atari_context_t)));
  if (!s_snapshot) {
    releaseCore();
    s_error = -2;
    return s_error;
  }

  s_romChecksum = hashRom(rom, size);
  s_running = true;
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
  if (!s_running || !s_atari) return s_frameCount;
  atari_run_frame(s_atari);
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
  return s_atari ? s_atari->joypad : 0xFFU;
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
  if (!s_running || !s_atari || !s_snapshot) return;
  memcpy(s_snapshot, s_atari, sizeof(atari_context_t));
  s_snapshot->rom = nullptr;
  s_snapshot->framebuffer = nullptr;
  s_snapshotFrameCount = s_frameCount;
  s_snapshotRomChecksum = s_romChecksum;
  s_snapshotButtonMask = s_buttonMask;
  s_snapshotValid = true;
}

__attribute__((visibility("default")))
int32_t bmo_atari_load_state() {
  if (!s_running || !s_atari || !s_snapshot || !s_snapshotValid ||
      s_snapshotRomChecksum != s_romChecksum) {
    return 0;
  }
  const uint8_t* rom = s_atari->rom;
  const size_t romSize = s_atari->rom_size;
  uint16_t* framebuffer = s_atari->framebuffer;
  memcpy(s_atari, s_snapshot, sizeof(atari_context_t));
  s_atari->rom = rom;
  s_atari->rom_size = romSize;
  s_atari->framebuffer = framebuffer;
  s_frameCount = s_snapshotFrameCount;
  s_buttonMask = s_snapshotButtonMask;
  updateFramebuffer();
  applyButtons();
  return 1;
}

__attribute__((visibility("default")))
void bmo_atari_destroy() {
  releaseCore();
  s_error = 0;
}

}
