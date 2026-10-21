#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t ram[0x8000];       // 32KB PICO-8 RAM ($0000-$7FFF: GFX, MAP, GFF, DrawState, Screen)
    uint16_t palette[16];      // 16 PICO-8 colors (pre-swapped BGR565)
    uint8_t joypad;            // Bit 0: Left, 1: Right, 2: Up, 3: Down, 4: O (A), 5: X (B), 6: Pause
    uint32_t frame_count;

    const uint8_t* cart_data;
    size_t cart_size;

    uint16_t* framebuffer;     // 128x128 in PSRAM
} pico_context_t;

pico_context_t* pico_create(void);
void pico_destroy(pico_context_t* ctx);
bool pico_load_cart(pico_context_t* ctx, const uint8_t* data, size_t size);
void pico_set_input(pico_context_t* ctx, uint8_t joypad);
void pico_run_frame(pico_context_t* ctx);

// PICO-8 Hardware Graphics API (Callable from C/Lua/Cartridge VM)
void pico_cls(pico_context_t* ctx, uint8_t col);
void pico_pset(pico_context_t* ctx, int x, int y, uint8_t col);
uint8_t pico_pget(const pico_context_t* ctx, int x, int y);
void pico_rectfill(pico_context_t* ctx, int x0, int y0, int x1, int y1, uint8_t col);
void pico_circfill(pico_context_t* ctx, int cx, int cy, int r, uint8_t col);
void pico_spr(pico_context_t* ctx, int n, int x, int y, bool flip_x, bool flip_y);
void pico_map(pico_context_t* ctx, int cel_x, int cel_y, int sx, int sy, int cel_w, int cel_h);

#ifdef __cplusplus
}
#endif
