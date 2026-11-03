#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../../firmware/BmoGameboy/src/vendor/agnes/agnes.h"

extern "C" uint8_t* bmo_emu_rom_buffer();
extern "C" uint32_t bmo_emu_rom_capacity();

namespace {

constexpr uint32_t SCREEN_WIDTH = 256;
constexpr uint32_t SCREEN_HEIGHT = 240;
constexpr size_t FRAMEBUFFER_SIZE = SCREEN_WIDTH * SCREEN_HEIGHT;

const uint16_t NES_PALETTE[64] = {
  0x2C63, 0x5101, 0x9410, 0x1438, 0x0F58, 0x0868, 0x2068, 0xE050,
  0xA031, 0x400A, 0x8002, 0x6102, 0x0902, 0x0000, 0x0000, 0x0000,
  0x75AD, 0xFB12, 0x1F42, 0x3F71, 0xD9A0, 0xEFB0, 0x84B1, 0x609A,
  0x606B, 0x203C, 0x800C, 0x6604, 0xF103, 0x0000, 0x0000, 0x0000,
  0xFFFF, 0x9F65, 0x9F94, 0xBFC3, 0x5FF3, 0x79FB, 0x0EFC, 0xE4EC,
  0xE0BD, 0xC08E, 0x265F, 0x1047, 0x7B4E, 0x694A, 0x0000, 0x0000,
  0xFFFF, 0x3FA7, 0xDFBD, 0xDFDD, 0xDFFD, 0x38FD, 0x96F6, 0x15FF,
  0xCFFE, 0xCFDF, 0xD7BF, 0xDBBF, 0xFF07, 0xDFFE, 0x0000, 0x0000,
};

agnes_t* s_agnes = nullptr;
uint16_t s_framebuffer[FRAMEBUFFER_SIZE];
uint8_t* s_state = nullptr;
size_t s_stateSize = 0;
uint32_t s_romSize = 0;
uint32_t s_romChecksum = 0;
uint32_t s_frameCount = 0;
uint32_t s_buttonMask = 0;
uint32_t s_snapshotFrameCount = 0;
uint32_t s_snapshotRomChecksum = 0;
uint32_t s_snapshotButtonMask = 0;
int32_t s_error = 0;
bool s_running = false;
bool s_snapshotValid = false;

uint16_t swapBytes(uint16_t value) {
  return static_cast<uint16_t>((value << 8) | (value >> 8));
}

uint16_t browserRgb565(uint16_t panelPixel) {
  // The firmware palette is byte-swapped BGR565 for SPI; the browser needs RGB565.
  const uint16_t bgr565 = swapBytes(panelPixel);
  return static_cast<uint16_t>(
      ((bgr565 & 0x001FU) << 11) |
      (bgr565 & 0x07E0U) |
      ((bgr565 & 0xF800U) >> 11));
}

void updateFramebuffer() {
  if (!s_agnes) return;
  const uint8_t* source = agnes_get_screen_buffer(s_agnes);
  if (!source) return;
  for (size_t i = 0; i < FRAMEBUFFER_SIZE; ++i) {
    s_framebuffer[i] = browserRgb565(NES_PALETTE[source[i] & 0x3FU]);
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
  if (!s_agnes) return;
  const agnes_input_t input = {
    (s_buttonMask & 0x10U) != 0,
    (s_buttonMask & 0x20U) != 0,
    (s_buttonMask & 0x80U) != 0,
    (s_buttonMask & 0x40U) != 0,
    (s_buttonMask & 0x01U) != 0,
    (s_buttonMask & 0x02U) != 0,
    (s_buttonMask & 0x04U) != 0,
    (s_buttonMask & 0x08U) != 0,
  };
  agnes_set_input(s_agnes, &input, nullptr);
}

void releaseCore() {
  s_running = false;
  if (s_agnes) {
    agnes_destroy(s_agnes);
    s_agnes = nullptr;
  }
  free(s_state);
  s_state = nullptr;
  s_stateSize = 0;
  s_romSize = 0;
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
int32_t bmo_nes_load_rom(uint32_t size) {
  releaseCore();
  s_error = 0;
  uint8_t* rom = bmo_emu_rom_buffer();
  const uint32_t capacity = bmo_emu_rom_capacity();
  if (!rom || size < 16U || size > capacity) {
    s_error = -1;
    return s_error;
  }
  if (memcmp(rom, "NES\x1A", 4) != 0 || rom[4] == 0) {
    s_error = -2;
    return s_error;
  }
  if ((rom[7] & 0x0CU) == 0x08U) {
    s_error = -3;
    return s_error;
  }

  const uint32_t trainerSize = (rom[6] & 0x04U) != 0 ? 512U : 0U;
  const uint32_t declaredSize =
      16U + trainerSize + static_cast<uint32_t>(rom[4]) * 16384U +
      static_cast<uint32_t>(rom[5]) * 8192U;
  if (size < declaredSize) {
    s_error = -4;
    return s_error;
  }

  const uint32_t mapper = ((rom[6] & 0xF0U) >> 4) | (rom[7] & 0xF0U);
  if (mapper != 0U && mapper != 1U && mapper != 2U && mapper != 4U) {
    s_error = -5;
    return s_error;
  }

  s_agnes = agnes_make();
  if (!s_agnes || !agnes_load_ines_data(s_agnes, rom, size)) {
    releaseCore();
    s_error = -6;
    return s_error;
  }

  s_stateSize = agnes_state_size();
  if (s_stateSize == 0) {
    releaseCore();
    s_error = -6;
    return s_error;
  }
  s_state = static_cast<uint8_t*>(malloc(s_stateSize));
  if (!s_state) {
    releaseCore();
    s_error = -6;
    return s_error;
  }

  s_romSize = size;
  s_romChecksum = hashRom(rom, size);
  s_running = true;
  applyButtons();
  updateFramebuffer();
  return 0;
}

__attribute__((visibility("default")))
void bmo_nes_set_buttons(uint32_t buttons) {
  s_buttonMask = buttons & 0xFFU;
  applyButtons();
}

__attribute__((visibility("default")))
uint32_t bmo_nes_run_frame() {
  if (!s_running || !s_agnes) return s_frameCount;
  agnes_next_frame(s_agnes);
  ++s_frameCount;
  updateFramebuffer();
  return s_frameCount;
}

__attribute__((visibility("default")))
uint16_t* bmo_nes_framebuffer() {
  return s_framebuffer;
}

__attribute__((visibility("default")))
uint32_t bmo_nes_frame_count() {
  return s_frameCount;
}

__attribute__((visibility("default")))
uint32_t bmo_nes_rom_checksum() {
  return s_romChecksum;
}

__attribute__((visibility("default")))
uint32_t bmo_nes_button_state() {
  return s_buttonMask;
}

__attribute__((visibility("default")))
int32_t bmo_nes_last_error() {
  return s_error;
}

__attribute__((visibility("default")))
int32_t bmo_nes_is_running() {
  return s_running ? 1 : 0;
}

__attribute__((visibility("default")))
void bmo_nes_save_state() {
  if (!s_running || !s_agnes || !s_state) return;
  agnes_dump_state(s_agnes, reinterpret_cast<agnes_state_t*>(s_state));
  s_snapshotFrameCount = s_frameCount;
  s_snapshotRomChecksum = s_romChecksum;
  s_snapshotButtonMask = s_buttonMask;
  s_snapshotValid = true;
}

__attribute__((visibility("default")))
int32_t bmo_nes_load_state() {
  if (!s_running || !s_agnes || !s_state || !s_snapshotValid ||
      s_snapshotRomChecksum != s_romChecksum) {
    return 0;
  }
  if (!agnes_restore_state(
          s_agnes, reinterpret_cast<const agnes_state_t*>(s_state))) {
    return 0;
  }
  s_frameCount = s_snapshotFrameCount;
  s_buttonMask = s_snapshotButtonMask;
  updateFramebuffer();
  applyButtons();
  return 1;
}

__attribute__((visibility("default")))
void bmo_nes_destroy() {
  releaseCore();
  s_error = 0;
}

}
