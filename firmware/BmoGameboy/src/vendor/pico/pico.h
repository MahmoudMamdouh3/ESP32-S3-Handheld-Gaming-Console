#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t ram[0x8000];       // 32KB PICO-8 RAM (Screen + Sprite + Map + Audio)
    uint16_t palette[16];      // 16 PICO-8 colors (pre-swapped BGR565)
    uint8_t joypad;            // Bit 0: Left, 1: Right, 2: Up, 3: Down, 4: O (A), 5: X (B), 6: Pause
    
    const uint8_t* cart_data;
    size_t cart_size;
    
    uint16_t* framebuffer;     // 128x128 in PSRAM
} pico_context_t;

pico_context_t* pico_create(void);
void pico_destroy(pico_context_t* ctx);
bool pico_load_cart(pico_context_t* ctx, const uint8_t* data, size_t size);
void pico_set_input(pico_context_t* ctx, uint8_t joypad);
void pico_run_frame(pico_context_t* ctx);

#ifdef __cplusplus
}
#endif
