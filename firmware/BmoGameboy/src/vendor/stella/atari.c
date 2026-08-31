// STUB_ENGINE: Architectural scaffold only. Renders a blank framebuffer.
// Not a functional emulator. Do not mark VERIFIED_HOST without a real engine.
#include "atari.h"
#include <stdlib.h>
#include <string.h>

#if defined(ESP32) || defined(ARDUINO)
#include <esp_heap_caps.h>
#define ATARI_MALLOC(sz) heap_caps_malloc(sz, MALLOC_CAP_SPIRAM)
#define ATARI_FREE(p) heap_caps_free(p)
#else
#define ATARI_MALLOC(sz) malloc(sz)
#define ATARI_FREE(p) free(p)
#endif

// Fast BGR565 Color Conversion (Byte-swapped for direct ST7789 wire transmission)
static inline uint16_t atari_rgb_to_bgr565(uint8_t r, uint8_t g, uint8_t b) {
    uint16_t bgr = ((b & 0xF8) << 8) | ((g & 0xFC) << 3) | (r >> 3);
    return ((bgr & 0xFF) << 8) | ((bgr >> 8) & 0xFF);
}

atari_context_t* atari_create(void) {
    atari_context_t* ctx = (atari_context_t*)ATARI_MALLOC(sizeof(atari_context_t));
    if (!ctx) return NULL;
    memset(ctx, 0, sizeof(atari_context_t));
    
    // Allocate 160x192 16-bit framebuffer in PSRAM
    ctx->framebuffer = (uint16_t*)ATARI_MALLOC(160 * 192 * sizeof(uint16_t));
    if (!ctx->framebuffer) {
        ATARI_FREE(ctx);
        return NULL;
    }
    
    // Initialize standard NTSC TIA palette
    for (int i = 0; i < 128; i++) {
        uint8_t lum = (i & 0x0F) * 17;
        uint8_t hue = (i >> 4) & 0x07;
        uint8_t r = lum, g = lum, b = lum;
        if (hue == 1) { r = lum; g = lum / 2; b = 0; }
        else if (hue == 2) { r = lum; g = 0; b = 0; }
        else if (hue == 3) { r = lum; g = 0; b = lum; }
        else if (hue == 4) { r = 0; g = 0; b = lum; }
        else if (hue == 5) { r = 0; g = lum / 2; b = lum; }
        else if (hue == 6) { r = 0; g = lum; b = 0; }
        else if (hue == 7) { r = lum / 2; g = lum; b = 0; }
        ctx->tia_palette[i] = atari_rgb_to_bgr565(r, g, b);
    }
    return ctx;
}

void atari_destroy(atari_context_t* ctx) {
    if (ctx) {
        if (ctx->framebuffer) ATARI_FREE(ctx->framebuffer);
        ATARI_FREE(ctx);
    }
}

bool atari_load_rom(atari_context_t* ctx, const uint8_t* rom, size_t size) {
    if (!ctx || !rom || size == 0) return false;
    ctx->rom = rom;
    ctx->rom_size = size;
    
    // 6507 Reset vector at 0xFFFC - 0xFFFD (mirrored at top of ROM)
    ctx->pc = (size >= 4) ? (rom[size - 4] | (rom[size - 3] << 8)) : 0xF000;
    ctx->s = 0xFF;
    ctx->p = 0x20;
    ctx->joypad = 0xFF;
    
    return true;
}

void atari_set_input(atari_context_t* ctx, uint8_t joypad) {
    if (ctx) {
        ctx->joypad = joypad;
    }
}

void atari_run_frame(atari_context_t* ctx) {
    if (!ctx || !ctx->framebuffer) return;
    
    // Render 192 visible TIA scanlines
    uint16_t bg = ctx->tia_palette[ctx->tia_regs[0x09] & 0x7F];
    for (int y = 0; y < 192; y++) {
        uint16_t* line = &ctx->framebuffer[y * 160];
        for (int x = 0; x < 160; x++) {
            line[x] = bg;
        }
    }
}
