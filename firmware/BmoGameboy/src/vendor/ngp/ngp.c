#include "ngp.h"
#include <string.h>

#define SWAP_16(x) (((x) >> 8) | (((x) & 0xFF) << 8))

void ngp_init(ngp_t* emu, uint8_t* rom, uint32_t rom_size, uint8_t* ram, uint16_t* fb, bool is_color) {
  if (!emu) return;
  memset(emu, 0, sizeof(ngp_t));
  emu->rom = rom;
  emu->rom_size = rom_size;
  emu->ram = ram;
  emu->framebuffer = fb;
  emu->is_color = is_color;
  if (rom && rom_size >= 4) {
    emu->pc = ((uint32_t)rom[0]) | ((uint32_t)rom[1] << 8) | ((uint32_t)rom[2] << 16) | ((uint32_t)rom[3] << 24);
  }
}

void ngp_set_joypad(ngp_t* emu, uint8_t pad) {
  if (emu) emu->joypad = pad;
}

void ngp_step_frame(ngp_t* emu) {
  if (!emu || !emu->framebuffer) return;
  uint16_t* fb = emu->framebuffer;
  uint16_t bg = emu->is_color ? SWAP_16(0x0000) : SWAP_16(0xCE79);
  for (int y = 0; y < 152; y++) {
    for (int x = 0; x < 160; x++) {
      fb[y * 160 + x] = bg;
    }
  }
}
