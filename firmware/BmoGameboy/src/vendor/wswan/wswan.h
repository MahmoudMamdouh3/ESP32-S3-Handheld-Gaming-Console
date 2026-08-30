#ifndef WSWAN_H
#define WSWAN_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint32_t ip;
  uint16_t ax, bx, cx, dx, sp, bp, si, di;
  uint16_t cs, ds, ss, es;
  uint16_t flags;
  uint8_t* rom;
  uint32_t rom_size;
  uint8_t* internal_ram; // 64KB IRAM
  uint16_t* framebuffer; // 224 x 144
  uint16_t key_state;
  bool is_color;
} wswan_t;

void wswan_init(wswan_t* emu, uint8_t* rom, uint32_t rom_size, uint8_t* ram, uint16_t* fb, bool is_color);
void wswan_step_frame(wswan_t* emu);
void wswan_set_keys(wswan_t* emu, uint16_t keys);

#ifdef __cplusplus
}
#endif

#endif // WSWAN_H
