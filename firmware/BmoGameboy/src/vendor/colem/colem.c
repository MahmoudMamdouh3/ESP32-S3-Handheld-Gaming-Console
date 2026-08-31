// STUB_ENGINE: Architectural scaffold only. Renders a blank framebuffer.
// Not a functional emulator. Do not mark VERIFIED_HOST without a real engine.
#include "colem.h"
#include <string.h>

#define SWAP_16(x) (((x) >> 8) | (((x) & 0xFF) << 8))

void colem_init(colem_t* emu, uint8_t* rom, uint32_t rom_size, uint8_t* ram, uint8_t* vram, uint16_t* fb) {
  if (!emu) return;
  memset(emu, 0, sizeof(colem_t));
  emu->rom = rom;
  emu->rom_size = rom_size;
  emu->ram = ram;
  emu->vram = vram;
  emu->framebuffer = fb;
  emu->pc = 0x8000;
}

void colem_set_joystick(colem_t* emu, uint16_t joy) {
  if (emu) emu->joystick = joy;
}

void colem_step_frame(colem_t* emu) {
  if (!emu || !emu->framebuffer) return;
  uint16_t* fb = emu->framebuffer;
  uint16_t bg = SWAP_16(0x0000); // Coleco black backdrop
  for (int y = 0; y < 192; y++) {
    for (int x = 0; x < 256; x++) {
      fb[y * 256 + x] = bg;
    }
  }
}
