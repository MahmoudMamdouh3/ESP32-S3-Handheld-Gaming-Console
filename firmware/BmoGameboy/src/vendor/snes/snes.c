#include "snes.h"
#include <string.h>

#define SWAP_16(x) (((x) >> 8) | (((x) & 0xFF) << 8))

void snes_init(snes_t* emu, uint8_t* rom, uint32_t rom_size, uint8_t* wram, uint16_t* fb) {
  if (!emu) return;
  memset(emu, 0, sizeof(snes_t));
  emu->rom = rom;
  emu->rom_size = rom_size;
  emu->wram = wram;
  emu->framebuffer = fb;
  if (rom && rom_size >= 0x8000) {
    // Read reset vector at $FFFC or $7FFC
    uint32_t header_offset = (rom_size & 0x7FFF) == 512 ? 512 : 0;
    uint32_t reset_vec = header_offset + (rom_size > 0x8000 ? 0xFFFC : 0x7FFC);
    if (reset_vec + 1 < rom_size) {
      emu->pc = (uint32_t)rom[reset_vec] | ((uint32_t)rom[reset_vec + 1] << 8);
    }
  }
}

void snes_set_pad(snes_t* emu, uint8_t pad) {
  if (emu) emu->pad_state = pad;
}

void snes_step_frame(snes_t* emu) {
  if (!emu || !emu->framebuffer) return;
  uint16_t* fb = emu->framebuffer;
  uint16_t bg_color = SWAP_16(0x0000); // SNES black backdrop
  for (int y = 0; y < 224; y++) {
    for (int x = 0; x < 256; x++) {
      fb[y * 256 + x] = bg_color;
    }
  }
}
