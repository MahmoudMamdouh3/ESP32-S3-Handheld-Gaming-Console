// STUB_ENGINE: Architectural scaffold only. Renders a blank framebuffer.
// Not a functional emulator. Do not mark VERIFIED_HOST without a real engine.
#include "lynx.h"
#include <string.h>

#define SWAP_16(x) (((x) >> 8) | (((x) & 0xFF) << 8))

void lynx_init(lynx_t* emu, uint8_t* rom, uint32_t rom_size, uint8_t* ram, uint16_t* fb) {
  if (!emu) return;
  memset(emu, 0, sizeof(lynx_t));
  emu->rom = rom;
  emu->rom_size = rom_size;
  emu->ram = ram;
  emu->framebuffer = fb;
  if (rom && rom_size >= 64) {
    // Lynx cartridge header offset
    uint32_t header_len = (rom[0] == 'L' && rom[1] == 'Y' && rom[2] == 'N' && rom[3] == 'X') ? 64 : 0;
    emu->pc = header_len;
  }
}

void lynx_set_buttons(lynx_t* emu, uint8_t buttons) {
  if (emu) emu->buttons = buttons;
}

void lynx_step_frame(lynx_t* emu) {
  if (!emu || !emu->framebuffer) return;
  uint16_t* fb = emu->framebuffer;
  uint16_t bg = SWAP_16(0x18C3); // Lynx charcoal backdrop
  for (int y = 0; y < 102; y++) {
    for (int x = 0; x < 160; x++) {
      fb[y * 160 + x] = bg;
    }
  }
}
