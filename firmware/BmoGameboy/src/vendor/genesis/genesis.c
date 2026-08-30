#include "genesis.h"
#include <string.h>

#define SWAP_16(x) (((x) >> 8) | (((x) & 0xFF) << 8))

void genesis_init(genesis_t* emu, uint8_t* rom, uint32_t rom_size, uint8_t* ram, uint16_t* fb) {
  if (!emu) return;
  memset(emu, 0, sizeof(genesis_t));
  emu->rom = rom;
  emu->rom_size = rom_size;
  emu->ram = ram;
  emu->framebuffer = fb;
  if (rom && rom_size >= 4) {
    emu->a[7] = ((uint32_t)rom[0] << 24) | ((uint32_t)rom[1] << 16) | ((uint32_t)rom[2] << 8) | rom[3];
    emu->pc = ((uint32_t)rom[4] << 24) | ((uint32_t)rom[5] << 16) | ((uint32_t)rom[6] << 8) | rom[7];
  }
}

void genesis_set_pad(genesis_t* emu, uint8_t pad) {
  if (emu) emu->pad_state = pad;
}

void genesis_step_frame(genesis_t* emu) {
  if (!emu || !emu->framebuffer) return;
  uint16_t* fb = emu->framebuffer;
  uint16_t bg_color = SWAP_16(0x0821); // Genesis dark slate backdrop
  for (int y = 0; y < 224; y++) {
    for (int x = 0; x < 320; x++) {
      fb[y * 320 + x] = bg_color;
    }
  }
  emu->scanline = 224;
}
