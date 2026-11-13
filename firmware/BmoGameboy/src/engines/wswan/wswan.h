#ifndef WSWAN_H
#define WSWAN_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  // V30MZ CPU State (x86/80186 compatible)
  uint16_t ip;
  uint16_t ax, bx, cx, dx;
  uint16_t sp, bp, si, di;
  uint16_t cs, ds, ss, es;
  uint16_t flags; // CF, PF, AF, ZF, SF, TF, IF, DF, OF
  bool halted;

  // ROM & RAM
  uint8_t* rom;
  uint32_t rom_size;
  uint8_t* internal_ram; // 64KB IRAM (holds tiles, maps, palettes)
  uint8_t* sram;         // 64KB Cartridge Save RAM
  bool owns_ram;

  // Hardware I/O Ports ($00–$FF)
  uint8_t io[256];
  uint8_t bank_rom[4];   // Bank mapping registers ($C0-$C3)
  uint8_t bank_sram;

  // Video Output
  uint16_t* framebuffer; // 224 x 144
  uint16_t scanline;
  uint16_t key_state;
  bool is_color;
  uint32_t cycles;

  // Palette Cache: 16 palettes of 16 colors in BGR565 for ST7789 push
  uint16_t palette_cache[256];
} wswan_t;

void wswan_init(wswan_t* emu, uint8_t* rom, uint32_t rom_size, uint8_t* ram, uint16_t* fb, bool is_color);
void wswan_destroy(wswan_t* emu);
void wswan_reset(wswan_t* emu);
void wswan_step_frame(wswan_t* emu);
void wswan_set_keys(wswan_t* emu, uint16_t keys);

#ifdef __cplusplus
}
#endif

#endif // WSWAN_H
