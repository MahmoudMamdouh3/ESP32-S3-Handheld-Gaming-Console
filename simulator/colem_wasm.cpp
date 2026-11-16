#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../firmware/BmoGameboy/src/third_party/smsplus/smsplus_bmo.h"

extern "C" uint8_t* bmo_emu_rom_buffer();
extern "C" uint32_t bmo_emu_rom_capacity();

namespace {

constexpr uint32_t SCREEN_WIDTH = 256;
constexpr uint32_t SCREEN_HEIGHT = 192;
constexpr size_t FRAMEBUFFER_SIZE = SCREEN_WIDTH * SCREEN_HEIGHT;

uint16_t s_panelSource[FRAMEBUFFER_SIZE];
uint16_t s_framebuffer[FRAMEBUFFER_SIZE];
uint16_t s_snapshotFb[FRAMEBUFFER_SIZE];
uint8_t* s_snapshotBlob = nullptr;
size_t s_snapshotBlobCap = 0;
size_t s_snapshotBlobLen = 0;
uint32_t s_romChecksum = 0;
uint32_t s_frameCount = 0;
uint32_t s_buttonMask = 0;
uint32_t s_snapshotFrameCount = 0;
uint32_t s_snapshotRomChecksum = 0;
uint32_t s_snapshotButtonMask = 0;
int32_t s_error = 0;
BmoSmsConsoleMode s_activeMode = BMO_SMS_CONSOLE_COLECO;
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
  if (!s_running || !bmo_sms_is_running()) return;
  bmo_sms_render_panel565(s_panelSource);
  if (s_activeMode == BMO_SMS_CONSOLE_GG) {
    constexpr uint32_t ggWidth = 160;
    constexpr uint32_t ggHeight = 144;
    for (uint32_t r = 0; r < ggHeight; ++r) {
      const uint16_t* srcRow = &s_panelSource[(24U + r) * SCREEN_WIDTH + 48U];
      uint16_t* dstRow = &s_framebuffer[r * ggWidth];
      for (uint32_t c = 0; c < ggWidth; ++c) {
        dstRow[c] = browserRgb565(srcRow[c]);
      }
    }
    return;
  }
  for (size_t i = 0; i < FRAMEBUFFER_SIZE; ++i) {
    s_framebuffer[i] = browserRgb565(s_panelSource[i]);
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
  int pad = 0;
  if (s_buttonMask & 0x01U) pad |= BMO_SMS_INPUT_UP;
  if (s_buttonMask & 0x02U) pad |= BMO_SMS_INPUT_DOWN;
  if (s_buttonMask & 0x04U) pad |= BMO_SMS_INPUT_LEFT;
  if (s_buttonMask & 0x08U) pad |= BMO_SMS_INPUT_RIGHT;
  if (s_buttonMask & 0x10U) pad |= BMO_SMS_INPUT_BUTTON1;
  if (s_buttonMask & 0x20U) pad |= BMO_SMS_INPUT_BUTTON2;

  int sys = 0;
  if (s_activeMode == BMO_SMS_CONSOLE_COLECO) {
    int keypad = 0x0F;
    if (s_buttonMask & 0x40U) keypad = 0x01;       // Start -> Keypad 1
    else if (s_buttonMask & 0x80U) keypad = 0x0A;  // Select -> Keypad *
    sys = (keypad << BMO_SMS_COLECO_KEYPAD_SHIFT);
  } else if (s_activeMode == BMO_SMS_CONSOLE_GG) {
    if (s_buttonMask & 0x40U) sys |= BMO_SMS_SYS_START;
  } else if (s_buttonMask & 0x40U) {
    sys |= BMO_SMS_SYS_PAUSE;
  }

  bmo_sms_set_input(pad, sys);
}

void releaseCore() {
  s_running = false;
  bmo_sms_destroy();
  free(s_snapshotBlob);
  s_snapshotBlob = nullptr;
  s_snapshotBlobCap = 0;
  s_snapshotBlobLen = 0;
  memset(s_panelSource, 0, sizeof(s_panelSource));
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

int32_t loadSmsMode(uint32_t size, BmoSmsConsoleMode mode, uint32_t minSize) {
  releaseCore();
  s_error = 0;
  s_activeMode = mode;
  uint8_t* rom = bmo_emu_rom_buffer();
  const uint32_t capacity = bmo_emu_rom_capacity();
  if (!rom || size < minSize || size > capacity) {
    s_error = -1;
    return s_error;
  }

  if (!bmo_sms_init(rom, size, mode, nullptr)) {
    releaseCore();
    s_error = -2;
    return s_error;
  }

  s_snapshotBlobCap = bmo_sms_state_size();
  s_snapshotBlob = static_cast<uint8_t*>(malloc(s_snapshotBlobCap ? s_snapshotBlobCap : 1U));
  if (!s_snapshotBlob) {
    releaseCore();
    s_error = -2;
    return s_error;
  }

  s_romChecksum = hashRom(rom, size);
  s_running = true;
  applyButtons();
  updateFramebuffer();
  return 0;
}

void saveSmsSnapshot() {
  if (!s_running || !s_snapshotBlob) return;
  size_t outLen = 0;
  if (!bmo_sms_save_state(s_snapshotBlob, s_snapshotBlobCap, &outLen) || outLen == 0) {
    return;
  }
  s_snapshotBlobLen = outLen;
  memcpy(s_snapshotFb, s_framebuffer, sizeof(s_framebuffer));
  s_snapshotFrameCount = s_frameCount;
  s_snapshotRomChecksum = s_romChecksum;
  s_snapshotButtonMask = s_buttonMask;
  s_snapshotValid = true;
}

int32_t loadSmsSnapshot() {
  if (!s_running || !s_snapshotBlob || !s_snapshotValid ||
      s_snapshotRomChecksum != s_romChecksum || s_snapshotBlobLen == 0) {
    return 0;
  }
  if (!bmo_sms_load_state(s_snapshotBlob, s_snapshotBlobLen)) {
    return 0;
  }
  memcpy(s_framebuffer, s_snapshotFb, sizeof(s_framebuffer));
  s_frameCount = s_snapshotFrameCount;
  s_buttonMask = s_snapshotButtonMask;
  applyButtons();
  return 1;
}

}  // namespace

extern "C" {

__attribute__((visibility("default")))
int32_t bmo_colem_load_rom(uint32_t size) {
  return loadSmsMode(size, BMO_SMS_CONSOLE_COLECO, 512U);
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
  bmo_sms_run_frame(0);
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
  size_t wramSize = 0;
  return bmo_sms_wram_data(&wramSize);
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
  if (!s_running) return 0;
  const int pad0 = bmo_sms_input_pad0();
  const int sys = bmo_sms_input_system();
  uint16_t joystick = 0;
  if (pad0 & BMO_SMS_INPUT_UP)      joystick |= 0x0001U;
  if (pad0 & BMO_SMS_INPUT_RIGHT)   joystick |= 0x0002U;
  if (pad0 & BMO_SMS_INPUT_DOWN)    joystick |= 0x0004U;
  if (pad0 & BMO_SMS_INPUT_LEFT)    joystick |= 0x0008U;
  if (pad0 & BMO_SMS_INPUT_BUTTON1) joystick |= 0x0010U;
  if (pad0 & BMO_SMS_INPUT_BUTTON2) joystick |= 0x0020U;
  if (((sys >> BMO_SMS_COLECO_KEYPAD_SHIFT) & 0x0F) == 0x01) joystick |= 0x0040U;
  return joystick;
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
  saveSmsSnapshot();
}

__attribute__((visibility("default")))
int32_t bmo_colem_load_state() {
  return loadSmsSnapshot();
}

__attribute__((visibility("default")))
void bmo_colem_destroy() {
  releaseCore();
  s_error = 0;
}

__attribute__((visibility("default")))
int32_t bmo_sms_wasm_load_rom(uint32_t size) {
  return loadSmsMode(size, BMO_SMS_CONSOLE_SMS, 1024U);
}

__attribute__((visibility("default")))
void bmo_sms_wasm_set_buttons(uint32_t buttons) {
  bmo_colem_set_buttons(buttons);
}

__attribute__((visibility("default")))
uint32_t bmo_sms_wasm_run_frame() {
  return bmo_colem_run_frame();
}

__attribute__((visibility("default")))
uint16_t* bmo_sms_wasm_framebuffer() {
  return s_framebuffer;
}

__attribute__((visibility("default")))
uint32_t bmo_sms_wasm_frame_count() {
  return s_frameCount;
}

__attribute__((visibility("default")))
uint32_t bmo_sms_wasm_rom_checksum() {
  return s_romChecksum;
}

__attribute__((visibility("default")))
int32_t bmo_sms_wasm_last_error() {
  return s_error;
}

__attribute__((visibility("default")))
int32_t bmo_sms_wasm_is_running() {
  return s_running && s_activeMode == BMO_SMS_CONSOLE_SMS ? 1 : 0;
}

__attribute__((visibility("default")))
void bmo_sms_wasm_save_state() {
  saveSmsSnapshot();
}

__attribute__((visibility("default")))
int32_t bmo_sms_wasm_load_state() {
  return loadSmsSnapshot();
}

__attribute__((visibility("default")))
void bmo_sms_wasm_destroy() {
  releaseCore();
  s_error = 0;
}

__attribute__((visibility("default")))
int32_t bmo_gg_wasm_load_rom(uint32_t size) {
  return loadSmsMode(size, BMO_SMS_CONSOLE_GG, 1024U);
}

__attribute__((visibility("default")))
void bmo_gg_wasm_set_buttons(uint32_t buttons) {
  bmo_colem_set_buttons(buttons);
}

__attribute__((visibility("default")))
uint32_t bmo_gg_wasm_run_frame() {
  return bmo_colem_run_frame();
}

__attribute__((visibility("default")))
uint16_t* bmo_gg_wasm_framebuffer() {
  return s_framebuffer;
}

__attribute__((visibility("default")))
uint32_t bmo_gg_wasm_frame_count() {
  return s_frameCount;
}

__attribute__((visibility("default")))
uint32_t bmo_gg_wasm_rom_checksum() {
  return s_romChecksum;
}

__attribute__((visibility("default")))
int32_t bmo_gg_wasm_last_error() {
  return s_error;
}

__attribute__((visibility("default")))
int32_t bmo_gg_wasm_is_running() {
  return s_running && s_activeMode == BMO_SMS_CONSOLE_GG ? 1 : 0;
}

__attribute__((visibility("default")))
void bmo_gg_wasm_save_state() {
  saveSmsSnapshot();
}

__attribute__((visibility("default")))
int32_t bmo_gg_wasm_load_state() {
  return loadSmsSnapshot();
}

__attribute__((visibility("default")))
void bmo_gg_wasm_destroy() {
  releaseCore();
  s_error = 0;
}

}
