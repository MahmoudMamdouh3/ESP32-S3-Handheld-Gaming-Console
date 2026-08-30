#include "emu_wswan.h"
#include "../vendor/wswan/wswan.h"
#include "../core/display_emu.h"
#include "../core/buttons.h"
#include "../core/config.h"
#include <esp_heap_caps.h>
#include <string.h>

namespace {
  wswan_t wswanState;
  uint16_t* wswanFb = nullptr;  // 224 x 144 in PSRAM
  uint8_t* wswanRam = nullptr;  // 64KB IRAM in PSRAM
  bool running = false;
}

bool WSwanEmu::init(uint8_t* romData, size_t romSize, bool isColor) {
  destroy();
  if (!romData || romSize < 512) return false;

  wswanFb = (uint16_t*)heap_caps_malloc(224 * 144 * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
  wswanRam = (uint8_t*)heap_caps_malloc(64 * 1024, MALLOC_CAP_SPIRAM);

  if (!wswanFb || !wswanRam) {
    destroy();
    return false;
  }

  wswan_init(&wswanState, romData, romSize, wswanRam, wswanFb, isColor);
  running = true;
  return true;
}

void WSwanEmu::update() {
  if (!running || !wswanFb) return;
  uint16_t keys = 0;
  if (Buttons::get(Buttons::UP).pressed)     keys |= 0x0001; // X1
  if (Buttons::get(Buttons::RIGHT).pressed)  keys |= 0x0002; // X2
  if (Buttons::get(Buttons::DOWN).pressed)   keys |= 0x0004; // X3
  if (Buttons::get(Buttons::LEFT).pressed)   keys |= 0x0008; // X4
  if (Buttons::get(Buttons::A).pressed)      keys |= 0x0010; // Button A
  if (Buttons::get(Buttons::B).pressed)      keys |= 0x0020; // Button B
  if (Buttons::get(Buttons::START).pressed)  keys |= 0x0040; // Start
  wswan_set_keys(&wswanState, keys);

  wswan_step_frame(&wswanState);
  DisplayEmu::streamWSwanFrame(wswanFb, 224, 144);
}

void WSwanEmu::destroy() {
  running = false;
  if (wswanFb) {
    free(wswanFb);
    wswanFb = nullptr;
  }
  if (wswanRam) {
    free(wswanRam);
    wswanRam = nullptr;
  }
  memset(&wswanState, 0, sizeof(wswanState));
}

bool WSwanEmu::isRunning() {
  return running;
}
