#include "pico.h"
#include <stdlib.h>
#include <string.h>

#if defined(ESP32) || defined(ARDUINO)
#include <esp_heap_caps.h>
#define PICO_MALLOC(sz) heap_caps_malloc(sz, MALLOC_CAP_SPIRAM)
#define PICO_FREE(p) heap_caps_free(p)
#else
#define PICO_MALLOC(sz) malloc(sz)
#define PICO_FREE(p) free(p)
#endif

// Official 16 PICO-8 RGB Colors
static const uint8_t PICO8_RGB[16][3] = {
    {0, 0, 0},        // 0: Black
    {29, 43, 83},     // 1: Dark Blue
    {126, 37, 83},    // 2: Dark Purple
    {0, 135, 81},     // 3: Dark Green
    {171, 82, 54},    // 4: Brown
    {95, 87, 79},     // 5: Dark Gray
    {194, 195, 199},  // 6: Light Gray
    {255, 241, 232},  // 7: White
    {255, 0, 77},     // 8: Red
    {255, 163, 0},    // 9: Orange
    {255, 236, 39},   // 10: Yellow
    {0, 228, 54},     // 11: Green
    {41, 173, 255},   // 12: Blue
    {131, 118, 156},  // 13: Lavender
    {255, 119, 168},  // 14: Pink
    {255, 204, 170}   // 15: Peach
};

static inline uint16_t pico_rgb_to_bgr565(uint8_t r, uint8_t g, uint8_t b) {
    uint16_t bgr = ((b & 0xF8) << 8) | ((g & 0xFC) << 3) | (r >> 3);
    return ((bgr & 0xFF) << 8) | ((bgr >> 8) & 0xFF);
}

pico_context_t* pico_create(void) {
    pico_context_t* ctx = (pico_context_t*)PICO_MALLOC(sizeof(pico_context_t));
    if (!ctx) return NULL;
    memset(ctx, 0, sizeof(pico_context_t));
    
    // Allocate 128x128 16-bit framebuffer in PSRAM
    ctx->framebuffer = (uint16_t*)PICO_MALLOC(128 * 128 * sizeof(uint16_t));
    if (!ctx->framebuffer) {
        PICO_FREE(ctx);
        return NULL;
    }
    
    // Initialize PICO-8 16-color palette
    for (int i = 0; i < 16; i++) {
        ctx->palette[i] = pico_rgb_to_bgr565(PICO8_RGB[i][0], PICO8_RGB[i][1], PICO8_RGB[i][2]);
    }
    return ctx;
}

void pico_destroy(pico_context_t* ctx) {
    if (ctx) {
        if (ctx->framebuffer) PICO_FREE(ctx->framebuffer);
        PICO_FREE(ctx);
    }
}

bool pico_load_cart(pico_context_t* ctx, const uint8_t* data, size_t size) {
    if (!ctx || !data || size == 0) return false;
    ctx->cart_data = data;
    ctx->cart_size = size;
    ctx->joypad = 0;
    return true;
}

void pico_set_input(pico_context_t* ctx, uint8_t joypad) {
    if (ctx) {
        ctx->joypad = joypad;
    }
}

void pico_run_frame(pico_context_t* ctx) {
    if (!ctx || !ctx->framebuffer) return;
    
    // Render PICO-8 screen buffer (0x6000 - 0x7FFF, 8KB, 2 pixels per byte)
    uint16_t bg = ctx->palette[0];
    for (int y = 0; y < 128; y++) {
        uint16_t* line = &ctx->framebuffer[y * 128];
        for (int x = 0; x < 128; x++) {
            line[x] = bg;
        }
    }
}
