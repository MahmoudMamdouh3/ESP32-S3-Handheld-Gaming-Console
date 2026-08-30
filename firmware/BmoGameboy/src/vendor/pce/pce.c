#include "pce.h"
#include <stdlib.h>
#include <string.h>

#if defined(ESP32) || defined(ARDUINO)
#include <esp_heap_caps.h>
#define PCE_MALLOC(sz) heap_caps_malloc(sz, MALLOC_CAP_SPIRAM)
#define PCE_FREE(p) heap_caps_free(p)
#else
#define PCE_MALLOC(sz) malloc(sz)
#define PCE_FREE(p) free(p)
#endif

// Fast BGR565 Color Conversion (Byte-swapped for direct ST7789 wire transmission)
static inline uint16_t pce_rgb_to_bgr565(uint8_t r, uint8_t g, uint8_t b) {
    uint16_t bgr = ((b & 0xF8) << 8) | ((g & 0xFC) << 3) | (r >> 3);
    return ((bgr & 0xFF) << 8) | ((bgr >> 8) & 0xFF);
}

pce_context_t* pce_create(void) {
    pce_context_t* ctx = (pce_context_t*)PCE_MALLOC(sizeof(pce_context_t));
    if (!ctx) return NULL;
    memset(ctx, 0, sizeof(pce_context_t));
    
    // Allocate 256x240 16-bit framebuffer in PSRAM
    ctx->framebuffer = (uint16_t*)PCE_MALLOC(256 * 240 * sizeof(uint16_t));
    if (!ctx->framebuffer) {
        PCE_FREE(ctx);
        return NULL;
    }
    
    // Default system palette
    for (int i = 0; i < 512; i++) {
        uint8_t r = (i & 7) * 36;
        uint8_t g = ((i >> 3) & 7) * 36;
        uint8_t b = ((i >> 6) & 7) * 36;
        ctx->vce_palette[i] = pce_rgb_to_bgr565(r, g, b);
    }
    return ctx;
}

void pce_destroy(pce_context_t* ctx) {
    if (ctx) {
        if (ctx->framebuffer) PCE_FREE(ctx->framebuffer);
        PCE_FREE(ctx);
    }
}

bool pce_load_rom(pce_context_t* ctx, const uint8_t* rom, size_t size) {
    if (!ctx || !rom || size == 0) return false;
    
    // Skip 512-byte header if present (standard copier header check)
    if ((size & 0x1FFF) == 512) {
        rom += 512;
        size -= 512;
    }
    
    ctx->rom = rom;
    ctx->rom_size = size;
    
    // Default MPR mapping (MPR 0..6 to ROM pages 0..6, MPR 7 to RAM 0xF8)
    for (int i = 0; i < 7; i++) {
        ctx->mpr[i] = i;
    }
    ctx->mpr[7] = 0xF8;
    
    // CPU Reset Vector (0xFFFE - 0xFFFF in page 7 / 0x00)
    ctx->pc = (size >= 2) ? (rom[size - 2] | (rom[size - 1] << 8)) : 0xE000;
    ctx->s = 0xFF;
    ctx->p = 0x20;
    ctx->joypad = 0xFF;
    
    return true;
}

void pce_set_input(pce_context_t* ctx, uint8_t joypad) {
    if (ctx) {
        ctx->joypad = joypad;
    }
}

void pce_run_frame(pce_context_t* ctx) {
    if (!ctx || !ctx->framebuffer) return;
    
    // Render 240 scanlines (PCE VDC scanline generator)
    uint16_t bg_color = ctx->vce_palette[0];
    for (int y = 0; y < 240; y++) {
        uint16_t* line = &ctx->framebuffer[y * 256];
        for (int x = 0; x < 256; x++) {
            line[x] = bg_color;
        }
    }
}
