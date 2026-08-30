#include "emu_ngp.h"
#include "../vendor/ngp/ngp.h"
#include "../core/display_emu.h"
#include "../core/buttons.h"
#include "../core/config.h"
#include <esp_heap_caps.h>
#include <string.h>

namespace {
  ngp_t ngpState;
  uint16_t* ngpFb = nullptr;   // 160 x 152 in PSRAM
  uint8_t* ngpRam = nullptr;   // 32KB RAM in PSRAM
  bool running = false;
}

bool NGPEmu::init(uint8_t* romData, size_t romSize, bool isColor) {
  destroy();
  if (!romData || romSize < 512) return false;

  ngpFb = (uint16_t*)heap_caps_malloc(160 * 152 * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
  ngpRam = (uint8_t*)heap_caps_malloc(32 * 1024, MALLOC_CAP_SPIRAM);

  if (!ngpFb || !ngpRam) {
    destroy();
    return false;
  }

  ngp_init(&ngpState, romData, romSize, ngpRam, ngpFb, isColor);
  running = true;
  return true;
}

void NGPEmu::update() {
  if (!running || !ngpFb) return;
  uint8_t pad = 0;
  if (Buttons::get(Buttons::UP).pressed)     pad |= 0x01;
  if (Buttons::get(Buttons::DOWN).pressed)   pad |= 0x02;
  if (Buttons::get(Buttons::LEFT).pressed)   pad |= 0x04;
  if (Buttons::get(Buttons::RIGHT).pressed)  pad |= 0x08;
  if (Buttons::get(Buttons::A).pressed)      pad |= 0x10; // Button A
  if (Buttons::get(Buttons::B).pressed)      pad |= 0x20; // Button B
  if (Buttons::get(Buttons::START).pressed)  pad |= 0x40; // Option
  ngp_set_joypad(&ngpState, pad);

  ngp_step_frame(&ngpState);
  DisplayEmu::streamNGPFrame(ngpFb, 160, 152);
}

void NGPEmu::destroy() {
  running = false;
  if (ngpFb) {
    free(ngpFb);
    ngpFb = nullptr;
  }
  if (ngpRam) {
    free(ngpRam);
    ngpRam = nullptr;
  }
  memset(&ngpState, 0, sizeof(ngpState));
}

bool NGPEmu::isRunning() {
  return running;
}
