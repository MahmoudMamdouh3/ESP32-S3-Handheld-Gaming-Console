#include "emu_nes.h"
#include "../vendor/agnes/agnes.h"
#include "../core/display_emu.h"
#include "../core/buttons.h"
#include <Arduino.h>

static agnes_t* agnes_ctx = nullptr;

bool NesEmu::begin(const uint8_t* romData, size_t romSize) {
  if (agnes_ctx) {
    agnes_destroy(agnes_ctx);
  }
  agnes_ctx = agnes_make();
  if (!agnes_ctx) {
    return false;
  }
  
  // Cast away const since agnes loads data (it doesn't modify it, but API lacks const)
  if (!agnes_load_ines_data(agnes_ctx, (void*)romData, romSize)) {
    agnes_destroy(agnes_ctx);
    agnes_ctx = nullptr;
    return false;
  }
  return true;
}

void NesEmu::updateJoypad() {
  if (!agnes_ctx) return;
  
  agnes_input_t input = {0};
  
  // Map Gameboy buttons to NES buttons
  input.a = Buttons::get(Buttons::A).pressed;
  input.b = Buttons::get(Buttons::B).pressed;
  input.select = Buttons::get(Buttons::SELECT).pressed;
  input.start = Buttons::get(Buttons::START).pressed;
  input.up = Buttons::get(Buttons::UP).pressed;
  input.down = Buttons::get(Buttons::DOWN).pressed;
  input.left = Buttons::get(Buttons::LEFT).pressed;
  input.right = Buttons::get(Buttons::RIGHT).pressed;
  
  // Set controller 1, controller 2 is null
  agnes_set_input(agnes_ctx, &input, nullptr);
}

void NesEmu::runFrame() {
  if (!agnes_ctx) return;
  
  // Render one frame
  agnes_next_frame(agnes_ctx);
  
  // Stream to display
  const uint8_t* frame_buffer = agnes_get_screen_buffer(agnes_ctx);
  DisplayEmu::streamNESFrame(frame_buffer);
}

void NesEmu::destroy() {
  if (agnes_ctx) {
    agnes_destroy(agnes_ctx);
    agnes_ctx = nullptr;
  }
}
