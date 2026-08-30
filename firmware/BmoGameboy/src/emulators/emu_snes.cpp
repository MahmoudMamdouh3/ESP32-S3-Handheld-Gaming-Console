#include "emu_snes.h"
#include "../vendor/snes/snes.h"
#include "../core/display_emu.h"
#include "../core/buttons.h"
#include "../core/config.h"
#include <esp_heap_caps.h>
#include <string.h>

namespace {
  snes_t snesState;
  uint16_t* snesFb = nullptr;  // 256 x 224 in PSRAM
  uint8_t* snesWram = nullptr; // 128KB WRAM in PSRAM
  bool running = false;
}

bool SNESEmu::init(uint8_t* romData, size_t romSize) {
  destroy();
  if (!romData || romSize < 0x8000) return false;

  snesFb = (uint16_t*)heap_caps_malloc(256 * 224 * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
  snesWram = (uint8_t*)heap_caps_malloc(128 * 1024, MALLOC_CAP_SPIRAM);

  if (!snesFb || !snesWram) {
    destroy();
    return false;
  }

  snes_init(&snesState, romData, romSize, snesWram, snesFb);
  running = true;
  return true;
}

void SNESEmu::update() {
  if (!running || !snesFb) return;
  uint8_t pad = 0;
  if (Buttons::get(Buttons::UP).pressed)     pad |= 0x01;
  if (Buttons::get(Buttons::DOWN).pressed)   pad |= 0x02;
  if (Buttons::get(Buttons::LEFT).pressed)   pad |= 0x04;
  if (Buttons::get(Buttons::RIGHT).pressed)  pad |= 0x08;
  if (Buttons::get(Buttons::B).pressed)      pad |= 0x10; // B
  if (Buttons::get(Buttons::A).pressed)      pad |= 0x20; // A
  if (Buttons::get(Buttons::START).pressed)  pad |= 0x40; // Start
  if (Buttons::get(Buttons::SELECT).pressed) pad |= 0x80; // Select
  snes_set_pad(&snesState, pad);

  snes_step_frame(&snesState);
  DisplayEmu::streamSNESFrame(snesFb, 256, 224);
}

void SNESEmu::destroy() {
  running = false;
  if (snesFb) {
    free(snesFb);
    snesFb = nullptr;
  }
  if (snesWram) {
    free(snesWram);
    snesWram = nullptr;
  }
  memset(&snesState, 0, sizeof(snesState));
}

bool SNESEmu::isRunning() {
  return running;
}
