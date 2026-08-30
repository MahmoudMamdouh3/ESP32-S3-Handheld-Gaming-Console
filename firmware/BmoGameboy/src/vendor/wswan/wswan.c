#include "wswan.h"
#include <string.h>

#define SWAP_16(x) (((x) >> 8) | (((x) & 0xFF) << 8))

void wswan_init(wswan_t* emu, uint8_t* rom, uint32_t rom_size, uint8_t* ram, uint16_t* fb, bool is_color) {
  if (!emu) return;
  memset(emu, 0, sizeof(wswan_t));
  emu->rom = rom;
  emu->rom_size = rom_size;
  emu->internal_ram = ram;
  emu->framebuffer = fb;
  emu->is_color = is_color;
  emu->cs = 0xFFFF;
  emu->ip = 0x0000;
}

void wswan_set_keys(wswan_t* emu, uint16_t keys) {
  if (emu) emu->key_state = keys;
}

void wswan_step_frame(wswan_t* emu) {
  if (!emu || !emu->framebuffer) return;
  uint16_t* fb = emu->framebuffer;
  uint16_t bg = emu->is_color ? SWAP_16(0xCE79) : SWAP_16(0x9E71);
  for (int y = 0; y < 144; y++) {
    for (int x = 0; x < 224; x++) {
      fb[y * 224 + x] = bg;
    }
  }
}
