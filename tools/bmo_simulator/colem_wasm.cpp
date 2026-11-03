#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../../firmware/BmoGameboy/src/vendor/colem/colem.h"

extern "C" uint8_t* bmo_emu_rom_buffer();
extern "C" uint32_t bmo_emu_rom_capacity();

namespace {

constexpr uint32_t SCREEN_WIDTH = 256;
constexpr uint32_t SCREEN_HEIGHT = 192;
constexpr size_t FRAMEBUFFER_SIZE = SCREEN_WIDTH * SCREEN_HEIGHT;
constexpr size_t RAM_SIZE = 8U * 1024U;
constexpr size_t VRAM_SIZE = 16U * 1024U;
constexpr size_t SNAPSHOT_MEMORY_SIZE =
    RAM_SIZE + VRAM_SIZE + FRAMEBUFFER_SIZE * sizeof(uint16_t);

colem_t s_colem;
uint8_t* s_ram = nullptr;
uint8_t* s_vram = nullptr;
uint16_t* s_framebufferSource = nullptr;
uint16_t s_framebuffer[FRAMEBUFFER_SIZE];
colem_t* s_snapshot = nullptr;
uint8_t* s_snapshotMemory = nullptr;
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
  if (!s_framebufferSource) return;
  for (size_t i = 0; i < FRAMEBUFFER_SIZE; ++i) {
    s_framebuffer[i] = browserRgb565(s_framebufferSource[i]);
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
  if (!s_running) return;
  uint16_t joystick = 0;
  if (s_buttonMask & 0x01U) joystick |= 0x0001U;
  if (s_buttonMask & 0x08U) joystick |= 0x0002U;
  if (s_buttonMask & 0x02U) joystick |= 0x0004U;
  if (s_buttonMask & 0x04U) joystick |= 0x0008U;
  if (s_buttonMask & 0x10U) joystick |= 0x0010U;
  if (s_buttonMask & 0x20U) joystick |= 0x0020U;
  if (s_buttonMask & 0x40U) joystick |= 0x0040U;
  colem_set_joystick(&s_colem, joystick);
}

void releaseCore() {
  s_running = false;
  free(s_snapshotMemory);
  s_snapshotMemory = nullptr;
  free(s_snapshot);
  s_snapshot = nullptr;
  free(s_framebufferSource);
  s_framebufferSource = nullptr;
  free(s_vram);
  s_vram = nullptr;
  free(s_ram);
  s_ram = nullptr;
  memset(&s_colem, 0, sizeof(s_colem));
  memset(s_framebuffer, 0, sizeof(s_framebuffer));
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
int32_t bmo_colem_load_rom(uint32_t size) {
  releaseCore();
  s_error = 0;
  uint8_t* rom = bmo_emu_rom_buffer();
  const uint32_t capacity = bmo_emu_rom_capacity();
  if (!rom || size < 512U || size > capacity) {
    s_error = -1;
    return s_error;
  }

  s_ram = static_cast<uint8_t*>(calloc(1, RAM_SIZE));
  s_vram = static_cast<uint8_t*>(calloc(1, VRAM_SIZE));
  s_framebufferSource = static_cast<uint16_t*>(calloc(FRAMEBUFFER_SIZE, sizeof(uint16_t)));
  s_snapshot = static_cast<colem_t*>(malloc(sizeof(colem_t)));
  s_snapshotMemory = static_cast<uint8_t*>(malloc(SNAPSHOT_MEMORY_SIZE));
  if (!s_ram || !s_vram || !s_framebufferSource || !s_snapshot || !s_snapshotMemory) {
    releaseCore();
    s_error = -2;
    return s_error;
  }

  colem_init(&s_colem, rom, size, s_ram, s_vram, s_framebufferSource);
  s_romChecksum = hashRom(rom, size);
  s_running = true;
  updateFramebuffer();
  return 0;
}

__attribute__((visibility("default")))
void bmo_colem_set_buttons(uint32_t buttons) {
  s_buttonMask = buttons & 0xFFU;
  applyButtons();
}

__attribute__((visibility("default")))
uint32_t bmo_colem_run_frame() {
  if (!s_running) return s_frameCount;
  applyButtons();
  colem_step_frame(&s_colem);
  ++s_frameCount;
  updateFramebuffer();
  return s_frameCount;
}

__attribute__((visibility("default")))
uint16_t* bmo_colem_framebuffer() {
  return s_framebuffer;
}

__attribute__((visibility("default")))
uint8_t* bmo_colem_ram() {
  return s_ram;
}

__attribute__((visibility("default")))
uint32_t bmo_colem_frame_count() {
  return s_frameCount;
}

__attribute__((visibility("default")))
uint32_t bmo_colem_rom_checksum() {
  return s_romChecksum;
}

__attribute__((visibility("default")))
uint32_t bmo_colem_button_state() {
  return s_buttonMask;
}

__attribute__((visibility("default")))
uint16_t bmo_colem_joystick_state() {
  return s_running ? s_colem.joystick : 0;
}

__attribute__((visibility("default")))
int32_t bmo_colem_last_error() {
  return s_error;
}

__attribute__((visibility("default")))
int32_t bmo_colem_is_running() {
  return s_running ? 1 : 0;
}

__attribute__((visibility("default")))
void bmo_colem_save_state() {
  if (!s_running || !s_snapshot || !s_snapshotMemory) return;
  memcpy(s_snapshot, &s_colem, sizeof(s_colem));
  s_snapshot->rom = nullptr;
  s_snapshot->ram = nullptr;
  s_snapshot->vram = nullptr;
  s_snapshot->framebuffer = nullptr;
  uint8_t* cursor = s_snapshotMemory;
  memcpy(cursor, s_ram, RAM_SIZE);
  cursor += RAM_SIZE;
  memcpy(cursor, s_vram, VRAM_SIZE);
  cursor += VRAM_SIZE;
  memcpy(cursor, s_framebufferSource, FRAMEBUFFER_SIZE * sizeof(uint16_t));
  s_snapshotFrameCount = s_frameCount;
  s_snapshotRomChecksum = s_romChecksum;
  s_snapshotButtonMask = s_buttonMask;
  s_snapshotValid = true;
}

__attribute__((visibility("default")))
int32_t bmo_colem_load_state() {
  if (!s_running || !s_snapshot || !s_snapshotMemory || !s_snapshotValid ||
      s_snapshotRomChecksum != s_romChecksum) {
    return 0;
  }
  const uint8_t* rom = s_colem.rom;
  const uint32_t romSize = s_colem.rom_size;
  memcpy(&s_colem, s_snapshot, sizeof(s_colem));
  s_colem.rom = const_cast<uint8_t*>(rom);
  s_colem.rom_size = romSize;
  s_colem.ram = s_ram;
  s_colem.vram = s_vram;
  s_colem.framebuffer = s_framebufferSource;
  const uint8_t* cursor = s_snapshotMemory;
  memcpy(s_ram, cursor, RAM_SIZE);
  cursor += RAM_SIZE;
  memcpy(s_vram, cursor, VRAM_SIZE);
  cursor += VRAM_SIZE;
  memcpy(s_framebufferSource, cursor, FRAMEBUFFER_SIZE * sizeof(uint16_t));
  s_frameCount = s_snapshotFrameCount;
  s_buttonMask = s_snapshotButtonMask;
  updateFramebuffer();
  return 1;
}

__attribute__((visibility("default")))
void bmo_colem_destroy() {
  releaseCore();
  s_error = 0;
}

}
